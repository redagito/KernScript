[CmdletBinding()]
param(
    [string]$BuildDirectory = 'build/redesign-release',
    [string]$OutputDirectory = 'build/performance-results',
    [ValidateRange(1, 100)][int]$Rounds = 5,
    [string[]]$Targets = @('CallOverhead', 'StackOps', 'ArithLoop', 'ExternRoundtrip',
        'NestedCalls', 'Pipeline', 'RealProgram', 'PerformanceLab', 'PerformanceMemory')
)

# Run already-built Release binaries one at a time. The runner never changes
# machine power settings, priority, affinity, or starts a graphical application.
$ErrorActionPreference = 'Stop'
$projectDirectory = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
function Get-ProjectPath([string]$Path) {
    $absolute = [IO.Path]::GetFullPath([IO.Path]::Combine($projectDirectory, $Path))
    if (-not $absolute.StartsWith($projectDirectory + [IO.Path]::DirectorySeparatorChar,
            [StringComparison]::OrdinalIgnoreCase)) {
        throw "Path must stay inside the project: $Path"
    }
    # Do not follow directory junctions or symlinks outside the project.
    $current = $absolute
    while ($current.Length -gt $projectDirectory.Length) {
        if (Test-Path -LiteralPath $current) {
            if ((Get-Item -LiteralPath $current).Attributes -band [IO.FileAttributes]::ReparsePoint) {
                throw "Linked paths are not supported: $current"
            }
        }
        $current = [IO.Path]::GetDirectoryName($current)
    }
    return $absolute
}

$buildPath = Get-ProjectPath $BuildDirectory
$outputPath = Get-ProjectPath $OutputDirectory
function Get-BenchmarkRelativePath([string]$Target) {
    $folder = if ($Target -eq 'PerformanceMemory') { 'PerformanceLab' } else { $Target }
    return "Benchmarks/$folder/Release/$Target.exe"
}
foreach ($target in $Targets) {
    if ($target -notmatch '^[A-Za-z0-9]+$') { throw "Invalid benchmark target: $target" }
    $executable = Get-ProjectPath (Join-Path $BuildDirectory (Get-BenchmarkRelativePath $target))
    if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
        throw "Build the Release target first: $target"
    }
}
New-Item -ItemType Directory -Path $outputPath -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $outputPath 'temp') -Force | Out-Null
$cachePath = Join-Path $buildPath 'CMakeCache.txt'
$buildSettings = if (Test-Path -LiteralPath $cachePath) {
    @(Get-Content -LiteralPath $cachePath | Select-String -Pattern '^(CMAKE_CXX_FLAGS[^:]*|CMAKE_GENERATOR|CMAKE_BUILD_TYPE|BUILD_SHARED_LIBS):' | ForEach-Object { $_.Line })
} else { @() }
$binaryHashes = [ordered]@{}
foreach ($target in $Targets) {
    $binaryHashes[$target] = (Get-FileHash -LiteralPath (Join-Path $buildPath (Get-BenchmarkRelativePath $target)) -Algorithm SHA256).Hash
}
$metadata = [ordered]@{
    UtcDate = [DateTime]::UtcNow.ToString('o')
    Commit = (& git -C $projectDirectory rev-parse HEAD)
    WorkingTree = @(& git -C $projectDirectory status --short)
    Processor = $env:PROCESSOR_IDENTIFIER
    LogicalProcessors = [Environment]::ProcessorCount
    OS = [Environment]::OSVersion.VersionString
    BuildDirectory = $buildPath
    BuildSettings = $buildSettings
    BinarySha256 = $binaryHashes
    Rounds = $Rounds
    MemoryNotes = 'OS peak working set includes executable/runtime. Private bytes sampled every 10 ms; short peaks may be missed. PerformanceMemory reports C++ new payloads separately.'
}
$metadata | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $outputPath 'metadata.json') -Encoding utf8
$processRows = [Collections.Generic.List[object]]::new()
$timingRows = [Collections.Generic.List[object]]::new()
$stageRows = [Collections.Generic.List[object]]::new()
$allocationRows = [Collections.Generic.List[object]]::new()
for ($round = 1; $round -le $Rounds; ++$round) {
    # Rotate order to reduce consistent first/last workload bias.
    for ($index = 0; $index -lt $Targets.Count; ++$index) {
        $target = $Targets[($index + $round - 1) % $Targets.Count]
        $executable = Join-Path $buildPath (Get-BenchmarkRelativePath $target)
        $info = [Diagnostics.ProcessStartInfo]::new($executable)
        $info.WorkingDirectory = $projectDirectory
        $info.UseShellExecute = $false
        $info.CreateNoWindow = $true
        $info.RedirectStandardOutput = $true
        $info.RedirectStandardError = $true
        $info.EnvironmentVariables['TEMP'] = Join-Path $outputPath 'temp'
        $info.EnvironmentVariables['TMP'] = Join-Path $outputPath 'temp'
        $info.EnvironmentVariables['PATH'] = ($env:PATH -split ';' | Where-Object { $_ -notmatch '^\s*[dD]:' }) -join ';'
        $process = [Diagnostics.Process]::Start($info)
        try {
            $stdoutTask = $process.StandardOutput.ReadToEndAsync()
            $stderrTask = $process.StandardError.ReadToEndAsync()
            $peakPrivate = [long]0
            $peakWorkingSet = [long]0
            while (-not $process.WaitForExit(10)) {
                try {
                    $process.Refresh()
                    $peakPrivate = [Math]::Max($peakPrivate, $process.PrivateMemorySize64)
                    $peakWorkingSet = [Math]::Max($peakWorkingSet, $process.PeakWorkingSet64)
                } catch [InvalidOperationException] { }
            }
            try {
                $process.Refresh()
                $peakWorkingSet = [Math]::Max($peakWorkingSet, $process.PeakWorkingSet64)
            } catch [InvalidOperationException] { }
            $stdout = $stdoutTask.GetAwaiter().GetResult()
            $stderr = $stderrTask.GetAwaiter().GetResult()
            $stdout | Set-Content -LiteralPath (Join-Path $outputPath "$target-$round.log") -Encoding utf8
            $stderr | Set-Content -LiteralPath (Join-Path $outputPath "$target-$round.stderr.log") -Encoding utf8
            if ($process.ExitCode -ne 0) { throw "$target failed with exit code $($process.ExitCode): $stderr" }
            $processRows.Add([pscustomobject]@{
                Round = $round; Target = $target
                PeakWorkingSetBytes = $peakWorkingSet; SampledPeakPrivateBytes = $peakPrivate
            })
            foreach ($line in ($stdout -split "`n")) {
                if ($line -match '^([^:]+): ([0-9.eE+-]+) ns/op, .*checksum OK') {
                    $timingRows.Add([pscustomobject]@{
                        Round = $round; Benchmark = $Matches[1]
                        NanosecondsPerOp = ([double]::Parse($Matches[2], [Globalization.CultureInfo]::InvariantCulture)).ToString('R', [Globalization.CultureInfo]::InvariantCulture)
                    })
                }
            }
            if ($stdout.StartsWith('record_type,')) {
                foreach ($row in ($stdout | ConvertFrom-Csv)) {
                    if ($row.record_type -eq 'timing' -or $row.record_type -eq 'memory') {
                        $row | Add-Member -NotePropertyName Round -NotePropertyValue $round
                        if ($row.record_type -eq 'timing') { $stageRows.Add($row) }
                        else { $allocationRows.Add($row) }
                    }
                }
            }
        } finally { $process.Dispose() }
    }
    # Preserve completed rounds even if a later child fails.
    $processRows | Export-Csv -LiteralPath (Join-Path $outputPath 'process-memory.csv') -NoTypeInformation
    $timingRows | Export-Csv -LiteralPath (Join-Path $outputPath 'existing-timings.csv') -NoTypeInformation
    $stageRows | Export-Csv -LiteralPath (Join-Path $outputPath 'stage-timings.csv') -NoTypeInformation
    $allocationRows | Export-Csv -LiteralPath (Join-Path $outputPath 'allocation-memory.csv') -NoTypeInformation
    Write-Output "Completed benchmark round $round/$Rounds"
}
Write-Output "Results: $outputPath"
