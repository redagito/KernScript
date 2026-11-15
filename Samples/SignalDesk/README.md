# SignalDesk

SignalDesk is a FLTK dashboard for simulated or recorded greenhouse data. It can replay `dash/recorded_greenhouse.csv` without modifying the input.

## Data and layouts

Build the `SignalDesk` target with GUI samples enabled. Configure one to six charts, then use Play, Pause, Step and Reset. Layouts save as `.sdl`; CSV replay uses a time column and four feeds: `temperature_C`, `humidity_pct`, `power_W` and `fan_pct`. Replay evaluates each step from the recording prefix through that row. Export CSV writes simulation or imported data; alert export writes the alert history. Output replacement is confirmed, and exports cannot overwrite the recording, layout or KernScript source files.

Recordings support 100,000 rows. Values are retained as doubles and imported values outside finite float32 range are rejected; VM calculations and host bindings use float32. Startup creates no files.

## Dashboard policy

Policies export `func evaluate() -> void` from the `alerts` module.
`dash/alerts.ks` supplies the policy; `dash/stats.ks` contains shared calculations.
The host provides value-returning `sample_count()`, `sample(index)`,
`window_size()`, `threshold()`, `hysteresis()` and `is_armed()`.

Call `emit(mean, minimum, maximum, alert, next_armed) -> void` exactly once.
Statistics must be finite and alert/state outputs binary. Each chart call has
a 2,000,000-instruction budget; windows contain up to 10,000 samples.

Use **Reload policy** to validate the candidate against every chart. Failed reloads retain the active policy and state. A tick commits chart statistics, history, alerts and replay/simulation cursor together; a chart failure pauses playback without partial updates. For short excursions, change the alert comparison to use the maximum rather than mean, reload and reset replay.

`SignalDesk --help` lists options. `SignalDesk --verify --test-root <directory>` runs headlessly; CTest registers `SignalDesk.Verify`.
