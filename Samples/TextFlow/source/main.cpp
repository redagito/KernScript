#include <cctype>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

#include <kern/Kern.h>

namespace
{

struct Options
{
    std::optional<std::filesystem::path> input;
    std::optional<std::filesystem::path> output;
    std::optional<std::filesystem::path> script;
    bool help = false;
    bool verify = false;
};

struct RecordContext
{
    std::string line;
    std::uint64_t lineNumber = 0;
    std::ostream *output = nullptr;
    bool outputFailed = false;
};

RecordContext *g_record = nullptr;

class RecordScope
{
public:
    explicit RecordScope(RecordContext &record) { g_record = &record; }
    ~RecordScope() { g_record = nullptr; }
    RecordScope(const RecordScope &) = delete;
    RecordScope &operator=(const RecordScope &) = delete;
};

class StderrLogger final : public kern::ILogger
{
protected:
    void onError(const std::string &message) override { std::cerr << "textflow: " << message << '\n'; }
};

int contains(const std::string &part) { return g_record->line.find(part) != std::string::npos ? 1 : 0; }

std::string line() { return g_record->line; }

std::string upper()
{
    std::string result = g_record->line;
    for (char &character : result)
    {
        character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
    }
    return result;
}

std::string replace(const std::string &from, const std::string &to)
{
    std::string result = g_record->line;
    if (from.empty())
    {
        return result;
    }
    std::size_t position = 0;
    while ((position = result.find(from, position)) != std::string::npos)
    {
        result.replace(position, from.size(), to);
        position += to.size();
    }
    return result;
}

std::uint32_t line_number()
{
    if (g_record->lineNumber > std::numeric_limits<std::uint32_t>::max())
    {
        throw std::overflow_error("input line number exceeds the 32-bit host API range");
    }
    return static_cast<std::uint32_t>(g_record->lineNumber);
}

void emit(const std::string &text)
{
    *g_record->output << text << '\n';
    if (!*g_record->output)
    {
        g_record->outputFailed = true;
    }
}

bool sameFile(const std::filesystem::path &first, const std::filesystem::path &second)
{
    std::error_code error;
    if (std::filesystem::exists(first, error) && !error && std::filesystem::exists(second, error) && !error &&
        std::filesystem::equivalent(first, second, error) && !error)
    {
        return true;
    }

    error.clear();
    const auto firstPath = std::filesystem::weakly_canonical(std::filesystem::absolute(first), error);
    if (error)
    {
        return std::filesystem::absolute(first).lexically_normal() ==
               std::filesystem::absolute(second).lexically_normal();
    }
    const auto secondPath = std::filesystem::weakly_canonical(std::filesystem::absolute(second), error);
    if (error)
    {
        return firstPath == std::filesystem::absolute(second).lexically_normal();
    }
    return firstPath == secondPath;
}

void registerFunctions(kern::IVirtualMachine &vm)
{
    vm.addFunction("contains", &contains);
    vm.addFunction("line", &line);
    vm.addFunction("upper", &upper);
    vm.addFunction("replace", &replace);
    vm.addFunction("line_number", &line_number);
    vm.addFunction("emit", &emit);
}

bool loadScript(const std::filesystem::path &scriptPath, std::unique_ptr<kern::IVirtualMachine> &vm)
{
    std::ifstream source(scriptPath);
    if (!source)
    {
        std::cerr << "textflow: cannot open script: " << scriptPath.string() << '\n';
        return false;
    }

    std::stringstream byteCode;
    StderrLogger logger;
    std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler(&logger));
    const std::filesystem::path scriptParent = scriptPath.has_parent_path() ? scriptPath.parent_path() : ".";
    if (!compiler->compileBytecode(source, byteCode, scriptPath.string(), {scriptParent.string()}))
    {
        std::cerr << "textflow: script compilation failed: " << scriptPath.string() << '\n';
        return false;
    }
    if (source.bad())
    {
        std::cerr << "textflow: read failed while loading script: " << scriptPath.string() << '\n';
        return false;
    }

    vm.reset(kern::createVirtualMachine(nullptr));
    registerFunctions(*vm);
    try
    {
        if (!vm->load(byteCode))
        {
            std::cerr << "textflow: could not load compiled script: " << scriptPath.string() << '\n';
            return false;
        }
    }
    catch (const kern::RuntimeError &error)
    {
        std::cerr << "textflow: script load error: " << error.what() << '\n';
        return false;
    }
    return true;
}

class StagedOutput
{
public:
    explicit StagedOutput(std::filesystem::path destination) : m_destination(std::move(destination))
    {
        std::random_device random;
        const auto seed = static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
        std::mt19937_64 generator(seed ^ (static_cast<std::uint64_t>(random()) << 32) ^ random());
        for (unsigned int attempt = 0; attempt < 64; ++attempt)
        {
            m_temporary = m_destination.parent_path() /
                          (m_destination.filename().string() + ".textflow-" + std::to_string(generator()) + ".tmp");
            std::error_code error;
            if (std::filesystem::exists(m_temporary, error) || error)
            {
                continue;
            }
            m_stream.open(m_temporary, std::ios::binary | std::ios::out | std::ios::trunc);
            if (m_stream)
            {
                return;
            }
        }
        throw std::runtime_error("cannot create a unique output staging file beside " + m_destination.string());
    }

    ~StagedOutput()
    {
        if (!m_committed)
        {
            std::error_code ignored;
            m_stream.close();
            std::filesystem::remove(m_temporary, ignored);
        }
    }

    std::ostream &stream() { return m_stream; }

    bool commit(std::string &diagnostic)
    {
        m_stream.flush();
        if (!m_stream)
        {
            diagnostic = "flush failed for staged output";
            return false;
        }
        m_stream.close();
        if (m_stream.fail())
        {
            diagnostic = "close failed for staged output";
            return false;
        }
#ifdef _WIN32
        if (!MoveFileExW(m_temporary.c_str(), m_destination.c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            diagnostic = "could not replace output file (Windows error " + std::to_string(GetLastError()) + ")";
            return false;
        }
#else
        std::error_code error;
        std::filesystem::rename(m_temporary, m_destination, error);
        if (error)
        {
            diagnostic = "could not replace output file: " + error.message();
            return false;
        }
#endif
        m_committed = true;
        return true;
    }

private:
    std::filesystem::path m_destination;
    std::filesystem::path m_temporary;
    std::ofstream m_stream;
    bool m_committed = false;
};

bool run(const Options &options, std::istream *injectedInput = nullptr, std::ostream *injectedOutput = nullptr)
{
    if (!options.script)
    {
        std::cerr << "textflow: a script path is required\n";
        return false;
    }
    if (options.input && options.output && sameFile(*options.input, *options.output))
    {
        std::cerr << "textflow: input and output refer to the same file\n";
        return false;
    }
    if (options.output && sameFile(*options.script, *options.output))
    {
        std::cerr << "textflow: output refers to the script file\n";
        return false;
    }

    std::unique_ptr<kern::IVirtualMachine> vm;
    if (!loadScript(*options.script, vm))
    {
        return false;
    }

    std::ifstream inputFile;
    std::istream *input = injectedInput ? injectedInput : &std::cin;
    if (options.input)
    {
        inputFile.open(*options.input, std::ios::binary);
        if (!inputFile)
        {
            std::cerr << "textflow: cannot open input: " << options.input->string() << '\n';
            return false;
        }
        input = &inputFile;
    }

    std::ostream *output = injectedOutput ? injectedOutput : &std::cout;
    std::unique_ptr<StagedOutput> staged;
    if (options.output)
    {
        try
        {
            staged = std::make_unique<StagedOutput>(*options.output);
        }
        catch (const std::exception &error)
        {
            std::cerr << "textflow: " << error.what() << '\n';
            return false;
        }
        output = &staged->stream();
    }

    RecordContext record;
    record.output = output;
    RecordScope recordScope(record);
    std::string line;
    while (std::getline(*input, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        record.line = line;
        ++record.lineNumber;
        try
        {
            if (!vm->callFunction("process"))
            {
                std::cerr << "textflow: script must define func process() -> void\n";
                return false;
            }
        }
        catch (const std::exception &error)
        {
            std::cerr << "textflow: runtime error on line " << record.lineNumber << ": " << error.what() << '\n';
            return false;
        }
        if (record.outputFailed)
        {
            std::cerr << "textflow: write failed while processing line " << record.lineNumber << '\n';
            return false;
        }
    }
    if (input->bad())
    {
        std::cerr << "textflow: read failed after line " << record.lineNumber << '\n';
        return false;
    }
    output->flush();
    if (output->bad() || output->fail())
    {
        std::cerr << "textflow: output stream failed\n";
        return false;
    }
    if (staged)
    {
        std::string diagnostic;
        if (!staged->commit(diagnostic))
        {
            std::cerr << "textflow: " << diagnostic << '\n';
            return false;
        }
    }
    return true;
}

void printHelp()
{
    std::cout << "TextFlow: stream lines through a KernScript process() rule.\n"
                 "Usage: TextFlow [--input PATH] [--output PATH] --script PATH\n"
                 "       TextFlow --help\n"
                 "       TextFlow --verify\n\n"
                 "Input defaults to stdin and output defaults to stdout. Each input line invokes\n"
                 "process() once with a fresh VM instruction budget. See README.md for the host API.\n";
}

bool parseOptions(int argc, char **argv, Options &options)
{
    for (int index = 1; index < argc; ++index)
    {
        const std::string_view argument(argv[index]);
        if (argument == "--help" || argument == "-h")
        {
            options.help = true;
        }
        else if (argument == "--verify")
        {
            options.verify = true;
        }
        else if (argument == "--input" || argument == "--output" || argument == "--script")
        {
            if (index + 1 >= argc)
            {
                std::cerr << "textflow: missing path after " << argument << '\n';
                return false;
            }
            const std::filesystem::path path(argv[++index]);
            if (argument == "--input")
            {
                if (options.input)
                {
                    std::cerr << "textflow: --input may be given only once\n";
                    return false;
                }
                options.input = path;
            }
            else if (argument == "--output")
            {
                if (options.output)
                {
                    std::cerr << "textflow: --output may be given only once\n";
                    return false;
                }
                options.output = path;
            }
            else
            {
                if (options.script)
                {
                    std::cerr << "textflow: --script may be given only once\n";
                    return false;
                }
                options.script = path;
            }
        }
        else
        {
            std::cerr << "textflow: unknown argument: " << argument << '\n';
            return false;
        }
    }
    if (options.help || options.verify)
    {
        if (options.help && options.verify)
        {
            std::cerr << "textflow: --help and --verify cannot be combined\n";
            return false;
        }
        return true;
    }
    return true;
}

bool verify(const std::filesystem::path &executable)
{
    std::filesystem::path projectRoot;
    std::error_code error;
    auto candidate = std::filesystem::absolute(executable, error).parent_path();
    while (!error && !candidate.empty())
    {
        if (std::filesystem::exists(candidate / "Samples" / "TextFlow" / "rules.ks"))
        {
            projectRoot = candidate;
            break;
        }
        const auto parent = candidate.parent_path();
        if (parent == candidate)
        {
            break;
        }
        candidate = parent;
    }
    if (projectRoot.empty())
    {
        std::cerr << "textflow: verifier could not locate the project directory from " << executable.string() << '\n';
        return false;
    }

    const std::filesystem::path temp = projectRoot / "build" / "test-tmp" / "TextFlow";
    std::filesystem::create_directories(temp, error);
    if (error)
    {
        std::cerr << "textflow: cannot create verifier directory " << temp.string() << ": " << error.message() << '\n';
        return false;
    }

    std::random_device random;
    const auto seed = static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    std::mt19937_64 generator(seed ^ (static_cast<std::uint64_t>(random()) << 32) ^ random());
    std::filesystem::path runDirectory;
    for (unsigned int attempt = 0; attempt < 64; ++attempt)
    {
        runDirectory = temp / ("run-" + std::to_string(generator()));
        if (std::filesystem::create_directory(runDirectory, error))
        {
            break;
        }
        if (error)
        {
            error.clear();
        }
        runDirectory.clear();
    }
    if (runDirectory.empty())
    {
        std::cerr << "textflow: cannot create a unique verifier directory under " << temp.string() << '\n';
        return false;
    }

    const auto rules = runDirectory / "verify-rules.ks";
    const auto input = runDirectory / "verify-input.log";
    const auto output = runDirectory / "verify-output.log";
    const auto bundledInput = projectRoot / "Samples" / "TextFlow" / "sample.log";
    const auto bundledRules = projectRoot / "Samples" / "TextFlow" / "rules.ks";
    auto writeFile = [](const std::filesystem::path &path, const std::string &contents)
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file.write(contents.data(), static_cast<std::streamsize>(contents.size()));
        return static_cast<bool>(file);
    };
    auto readFile = [](const std::filesystem::path &path)
    {
        std::ifstream file(path, std::ios::binary);
        return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    };
    auto expect = [](bool condition, const char *message)
    {
        if (!condition)
        {
            std::cerr << "textflow: verifier failed: " << message << '\n';
        }
        return condition;
    };
    auto expectRunFailure =
        [](const Options &testOptions, std::istream *testInput = nullptr, std::ostream *testOutput = nullptr)
    {
        std::ostringstream diagnostic;
        auto *previous = std::cerr.rdbuf(diagnostic.rdbuf());
        const bool result = run(testOptions, testInput, testOutput);
        std::cerr.rdbuf(previous);
        return !result;
    };

    if (!expect(
            std::filesystem::copy_file(bundledInput, input, std::filesystem::copy_options::overwrite_existing, error),
            "could not copy bundled log fixture"))
    {
        return false;
    }
    if (!run(Options{input, output, bundledRules, false, false}))
    {
        return false;
    }
    const std::string expectedBundled = "2026-11-14T08:15:02Z INFO: [gateway] listening on 0.0.0.0:8080\n"
                                        "2026-11-14T08:15:04Z INFO: [auth] provider ready\n"
                                        "2026-11-14T08:15:07Z warn: [auth] rejected token for user=guest\n"
                                        "2026-11-14T08:15:09Z ERROR: [STORAGE] CONNECTION TIMED OUT AFTER 5000MS\n"
                                        "2026-11-14T08:15:12Z INFO: [storage] retry connected\n"
                                        "2026-11-14T08:15:17Z ERROR: [WORKER] JOB=4821 FAILED: MISSING ACCOUNT\n"
                                        "2026-11-14T08:15:23Z INFO: [gateway] shutdown requested\n";
    if (!expect(readFile(output) == expectedBundled, "bundled rules did not filter and transform the sample log"))
    {
        return false;
    }

    const std::string inspectionRules =
        "extern line() -> value;\n"
        "extern replace(from, to) -> value;\n"
        "extern line_number() -> value;\n"
        "extern emit(text) -> void;\n"
        "func process() -> void {\n"
        "  if (line_number() == 1) { emit(line()); } else { emit(replace(\"x\", \"Y\")); }\n"
        "}\n";
    if (!writeFile(rules, inspectionRules))
    {
        std::cerr << "textflow: verifier could not write inspection rules\n";
        return false;
    }
    std::istringstream pipedInput("raw x\r\nsecond x\n");
    std::ostringstream pipedOutput;
    if (!expect(run(Options{std::nullopt, std::nullopt, rules, false, false}, &pipedInput, &pipedOutput),
                "stdin/stdout processing failed"))
    {
        return false;
    }
    if (!expect(pipedOutput.str() == "raw x\nsecond Y\n", "raw line, replace, line number, or CRLF handling failed"))
    {
        return false;
    }

    std::istringstream emptyInput;
    std::ostringstream emptyOutput;
    if (!expect(run(Options{std::nullopt, std::nullopt, rules, false, false}, &emptyInput, &emptyOutput) &&
                    emptyOutput.str().empty(),
                "empty input was not handled cleanly"))
    {
        return false;
    }

    const std::string missingEntry = "func other() -> void { var placeholder = 0; }\n";
    if (!writeFile(rules, missingEntry) || !writeFile(output, "preserve missing-entry\n"))
    {
        return false;
    }
    if (!expect(expectRunFailure(Options{input, output, rules, false, false}), "missing process entry was accepted") ||
        !expect(readFile(output) == "preserve missing-entry\n", "missing entry damaged existing output"))
    {
        return false;
    }

    if (!writeFile(rules, "func process( -> void {\n") || !writeFile(output, "preserve compile-failure\n"))
    {
        return false;
    }
    if (!expect(expectRunFailure(Options{input, output, rules, false, false}), "invalid script was accepted") ||
        !expect(readFile(output) == "preserve compile-failure\n", "compile failure damaged existing output"))
    {
        return false;
    }

    const std::string runtimeFailure = "func process() -> void { var broken = 1 / 0; }\n";
    if (!writeFile(rules, runtimeFailure) || !writeFile(output, "preserve runtime-failure\n"))
    {
        return false;
    }
    if (!expect(expectRunFailure(Options{input, output, rules, false, false}), "runtime failure was accepted") ||
        !expect(readFile(output) == "preserve runtime-failure\n", "runtime failure damaged existing output"))
    {
        return false;
    }

    if (!writeFile(rules, inspectionRules) || !writeFile(output, "preserve same-path\n"))
    {
        return false;
    }
    if (!expect(expectRunFailure(Options{output, output, rules, false, false}), "input/output alias was accepted") ||
        !expect(readFile(output) == "preserve same-path\n", "same-path rejection damaged input"))
    {
        return false;
    }
    if (!expect(expectRunFailure(Options{input, rules, rules, false, false}), "output/script alias was accepted"))
    {
        return false;
    }

    if (!writeFile(rules, inspectionRules))
    {
        return false;
    }
    std::istringstream failedOutputInput("line\n");
    std::ostringstream failedOutput;
    failedOutput.setstate(std::ios::badbit);
    if (!expect(expectRunFailure(Options{std::nullopt, std::nullopt, rules, false, false}, &failedOutputInput,
                                 &failedOutput),
                "write failure was not reported"))
    {
        return false;
    }

    const auto hardlink = runDirectory / "input-hardlink.log";
    error.clear();
    std::filesystem::create_hard_link(input, hardlink, error);
    if (!error)
    {
        if (!expect(expectRunFailure(Options{input, hardlink, rules, false, false}),
                    "hard-linked input/output alias was accepted"))
        {
            return false;
        }
    }

    std::cout << "TextFlow verifier passed (" << std::filesystem::absolute(executable).filename().string() << ")\n";
    return true;
}

} // namespace

int main(int argc, char **argv)
{
    try
    {
        Options options;
        if (!parseOptions(argc, argv, options))
        {
            return 2;
        }
        if (options.help)
        {
            printHelp();
            return 0;
        }
        if (options.verify)
        {
            return verify(std::filesystem::path(argv[0])) ? 0 : 1;
        }
        return run(options) ? 0 : 1;
    }
    catch (const std::exception &error)
    {
        std::cerr << "textflow: " << error.what() << '\n';
        return 1;
    }
}
