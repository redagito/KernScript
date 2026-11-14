# SignalDesk

SignalDesk is a small FLTK greenhouse dashboard. It can generate a deterministic simulated feed or replay the checked-in `dash/recorded_greenhouse.csv` recording without changing its input file.

## Workflow

- Select a chart and edit its title, source, window, threshold, or hysteresis. Apply the edit, or resolve pending edits when saving or changing selection. Add and remove charts to keep a saved layout between one and six charts.
- Use **Play**, **Pause**, **Step**, and **Reset** to control simulation or replay. **New simulation** switches back to a fresh deterministic feed.
- **Open layout** and **Save layout** load and save `.sdl` chart configurations. Unsaved layout and simulation changes prompt before discard or window close.
- **Replay CSV** loads the greenhouse CSV format: time plus four named feeds. Replay computes each step from the recording prefix through the current row; later rows do not affect earlier chart statistics.
- **Export CSV** saves the current simulation or imported recording. **Export alerts** saves the alert history.

## Dashboard scripts

The host exposes `sample_count()`, `sample(index)`, `window_size()`, `threshold()`, `hysteresis()`, and `is_armed()` during an evaluation. A policy calls `emit(mean, minimum, maximum, alert, next_armed)` once. The statistics module calculates mean, minimum, and maximum, and the alert module implements a high threshold with a hysteresis re-arm band. The host bounds script execution to 2000000 VM steps per chart and accepts only finite statistics and binary alert/state outputs. This includes the full 10000-sample configurable window.

`dash/stats.ks` is the reusable calculation module. `dash/alerts.ks` is the dashboard entry policy. To customize behavior, keep those host function names and the single `emit` call protocol, then update the policy modules. The four input feeds are `temperature_C`, `humidity_pct`, `power_W`, and `fan_pct`.

Use **Reload policy** to compile and validate a candidate against every current
chart before replacing the working VM. A failed reload retains the current
program, data and alert state. A tick commits all chart statistics, histories,
alerts and the simulation/replay cursor together; a failed chart pauses playback
without a partial tick. Recorded values are retained as doubles, while VM
calculations and host bindings use float32 precision. Non-finite values and
values outside the finite float32 range are rejected on import. A recording
supports 100000 rows. At capacity, export it and start a new simulation.

Output replacement is confirmed. CSV/alert export cannot replace the imported
recording, saved layout or an existing KernScript source file. No files are
created during startup. Build the `SignalDesk` target through the project's
normal Conan/CMake setup with GUI samples enabled. `--help` prints options and
`--verify --test-root build/test-tmp/SignalDesk` runs without windows.

## Recorded experiment

The included CSV contains 2,000 deterministic observations. It covers nominal operation, a periodic warm greenhouse excursion, and the corresponding fan and humidity changes. Replay it and step through the data to inspect the excursion windows and threshold alerts. The recording is treated as read-only input.

Start with an air-temperature chart using a 12-sample window, a threshold of
28 and hysteresis of 2. Replay the recording, export the alerts, then Reset and
compare a 36-sample window. Save each layout so the comparison can be repeated.
A useful policy customization is to alert on the maximum rather than the mean
when short excursions matter. Change the comparison in `alerts.ks`, reload it,
Reset the recording and compare the exported alert histories.

CTest registers `SignalDesk.Verify`. It covers model file roundtrips, invalid
input and output preservation, numeric editor formatting, exact known statistics,
hysteresis and rearming, failed candidate reloads, late-chart tick rollback,
prefix-only replay, EOF and a repeated 2000-row replay on six charts.
