#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Browser.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/Fl_Window.H>
#include <FL/fl_ask.H>
#include <FL/fl_draw.H>

#include "../../Support/Files.h"
#include "../../Support/Preview.h"
#include "SignalDeskModel.h"
#include <kern/Kern.h>

namespace fs = std::filesystem;
namespace
{
constexpr int kMaxCharts = 6;
constexpr int kFeeds = 4;
const char *feedNames[kFeeds] = {"temperature_C", "humidity_pct", "power_W", "fan_pct"};
using Row = signaldesk::Row;
struct Chart
{
    std::string title;
    int source{};
    int window{12};
    double threshold{28};
    double hysteresis{2};
    bool armed{true};
    double mean{}, minimum{}, maximum{};
    std::vector<double> history;
};
std::vector<double> windowFor(const std::vector<Row> &rows, const Chart &c);
std::vector<double> windowForPrefix(const std::vector<Row> &rows, std::size_t end, const Chart &c);
struct Alert
{
    unsigned long long time;
    std::string title;
    double mean, threshold;
};
struct App
{
    std::vector<Row> rows;
    std::vector<Chart> charts;
    std::vector<Alert> alerts;
    fs::path layout, sourcePath;
    bool layoutDirty{};
    bool recordingDirty{};
    bool paused{true};
    bool replay{};
    std::size_t replayAt{};
    unsigned tick{};
    unsigned rng{91231};
    int edit{};
} app;
Fl_Window *window{};
Fl_Browser *chartList{}, *alertList{};
Fl_Choice *sourceInput{};
Fl_Input *titleInput{}, *windowInput{}, *thresholdInput{}, *hystInput{};
Fl_Button *playButton{};
Fl_Box *modeStatus{};
bool timerActive{};
fs::path dashboardDirectory;
bool editorReady = false;
std::array<std::string, 5> editorBaseline;
bool resolveEditor();
void loadEditor();
void onNewSimulation(Fl_Widget *, void *);
struct EvalContext
{
    const std::vector<double> *values{};
    int configuredWindow{};
    int emitted{};
    double threshold{}, hysteresis{}, mean{}, minimum{}, maximum{};
    int armed{}, nextArmed{};
    int alert{};
} *ctx{};
class Logger final : public kern::ILogger
{
public:
    std::string last;

protected:
    void onError(const std::string &s) override { last = s; }
};

int sample_count() { return ctx ? static_cast<int>(ctx->values->size()) : 0; }
double sample(int i)
{
    if (!ctx || i < 0 || static_cast<std::size_t>(i) >= ctx->values->size())
        return std::numeric_limits<double>::quiet_NaN();
    return (*ctx->values)[static_cast<std::size_t>(i)];
}
double window_size() { return ctx ? static_cast<double>(ctx->configuredWindow) : 0; }
double threshold() { return ctx ? ctx->threshold : 0; }
double hysteresis() { return ctx ? ctx->hysteresis : 0; }
int is_armed() { return ctx ? ctx->armed : 0; }
void emit(double mean, double lo, double hi, int alert, int armed)
{
    if (!ctx)
        return;
    ++ctx->emitted;
    ctx->mean = mean;
    ctx->minimum = lo;
    ctx->maximum = hi;
    ctx->alert = alert;
    ctx->nextArmed = armed;
}

struct Engine
{
    Logger logger;
    std::unique_ptr<kern::IVirtualMachine> vm;
    void registerHost(kern::IVirtualMachine *target)
    {
        target->addFunction("sample_count", &sample_count);
        target->addFunction("sample", &sample);
        target->addFunction("window_size", &window_size);
        target->addFunction("threshold", &threshold);
        target->addFunction("hysteresis", &hysteresis);
        target->addFunction("is_armed", &is_armed);
        target->addFunction("emit", &emit);
    }
    bool runWith(kern::IVirtualMachine *target, EvalContext &c, std::string &error)
    {
        ctx = &c;
        struct Reset
        {
            ~Reset() { ctx = nullptr; }
        } reset;
        try
        {
            logger.last.clear();
            if (!target || !target->callFunction("evaluate"))
            {
                error = logger.last.empty() ? "Script entry evaluate() is missing" : logger.last;
                return false;
            }
        }
        catch (const std::exception &e)
        {
            error = e.what();
            return false;
        }
        if (c.emitted != 1 || c.alert < 0 || c.alert > 1 || c.nextArmed < 0 || c.nextArmed > 1 ||
            !std::isfinite(c.mean) || !std::isfinite(c.minimum) || !std::isfinite(c.maximum))
        {
            error = "Script output must emit once with finite statistics and alert/state values of 0 or 1";
            return false;
        }
        return true;
    }
    Engine()
        : vm(kern::createVirtualMachine(&logger,
                                        []
                                        {
                                            kern::VMConfig c;
                                            c.maxExecutionSteps = 2000000;
                                            return c;
                                        }()))
    {
        registerHost(vm.get());
    }
    bool load(const fs::path &dash, std::string &error)
    {
        logger.last.clear();
        std::istringstream src("import alerts; func evaluate() -> void { alerts::evaluate(); }");
        std::ostringstream bytes;
        std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler(&logger));
        if (!compiler->compileBytecode(src, bytes, "SignalDesk dashboard", {dash.string()}))
        {
            error = logger.last.empty() ? "Script compilation failed" : logger.last;
            return false;
        }
        std::istringstream in(bytes.str());
        auto candidate = std::unique_ptr<kern::IVirtualMachine>(kern::createVirtualMachine(&logger,
                                                                                           []
                                                                                           {
                                                                                               kern::VMConfig c;
                                                                                               c.maxExecutionSteps =
                                                                                                   2000000;
                                                                                               return c;
                                                                                           }()));
        registerHost(candidate.get());
        logger.last.clear();
        if (!candidate->load(in))
        {
            error = logger.last.empty() ? "Script bytecode could not be loaded" : logger.last;
            return false;
        }
        const std::size_t available = app.replay ? std::min(app.replayAt, app.rows.size()) : app.rows.size();
        for (const auto &chart : app.charts)
        {
            std::vector<double> vals;
            if (available)
                vals = windowForPrefix(app.rows, available, chart);
            else
                vals = {0.0};
            EvalContext test;
            test.values = &vals;
            test.configuredWindow = chart.window;
            test.threshold = chart.threshold;
            test.hysteresis = chart.hysteresis;
            test.armed = chart.armed ? 1 : 0;
            if (!runWith(candidate.get(), test, error))
            {
                error = "Policy rejected by chart '" + chart.title + "': " + error;
                return false;
            }
        }
        vm.swap(candidate);
        return true;
    }
    bool run(EvalContext &c, std::string &error) { return runWith(vm.get(), c, error); }
};
std::unique_ptr<Engine> engine;
bool parseNumber(const std::string &s, double &v) { return signaldesk::finiteNumber(s, v); }
bool parseInt(const std::string &s, int &v) { return signaldesk::integer(s, v); }
void timer(void *);
void scheduleTimer();
void pausePlayback()
{
    app.paused = true;
    if (timerActive)
    {
        Fl::remove_timeout(timer);
        timerActive = false;
    }
}
std::vector<double> valuesFor(const Row &r) { return {r.value[0], r.value[1], r.value[2], r.value[3]}; }
std::string sourceName(int i) { return i >= 0 && i < kFeeds ? feedNames[i] : "unknown"; }
std::vector<double> windowFor(const std::vector<Row> &rows, const Chart &c)
{
    std::vector<double> v;
    auto n = std::min<std::size_t>(rows.size(), static_cast<std::size_t>(c.window));
    for (std::size_t i = rows.size() - n; i < rows.size(); ++i)
        v.push_back(rows[i].value[c.source]);
    return v;
}
std::vector<double> windowForPrefix(const std::vector<Row> &rows, std::size_t end, const Chart &c)
{
    end = std::min(end, rows.size());
    std::vector<double> v;
    auto n = std::min<std::size_t>(end, static_cast<std::size_t>(c.window));
    v.reserve(n);
    for (std::size_t i = end - n; i < end; ++i)
        v.push_back(rows[i].value[c.source]);
    return v;
}
struct ChartPlot final : Fl_Widget
{
    ChartPlot(int x, int y, int w, int h, const char *label) : Fl_Widget(x, y, w, h, label) {}
    void draw() override
    {
        fl_push_clip(x(), y(), w(), h());
        fl_color(FL_WHITE);
        fl_rectf(x(), y(), w(), h());
        fl_color(FL_DARK3);
        fl_rect(x(), y(), w(), h());
        if (app.charts.empty())
        {
            fl_pop_clip();
            return;
        }
        int selected = std::clamp(app.edit, 0, static_cast<int>(app.charts.size()) - 1);
        const auto &c = app.charts[selected];
        if (c.history.size() > 1)
        {
            auto mm = std::minmax_element(c.history.begin(), c.history.end());
            double lo = *mm.first, hi = *mm.second;
            if (hi <= lo)
            {
                hi = lo + 1;
            }
            fl_color(FL_BLUE);
            for (std::size_t i = 1; i < c.history.size(); ++i)
            {
                int x1 =
                    x() + 4 + static_cast<int>((i - 1) * (w() - 8) / std::max<std::size_t>(1, c.history.size() - 1));
                int x2 = x() + 4 + static_cast<int>(i * (w() - 8) / std::max<std::size_t>(1, c.history.size() - 1));
                int y1 = y() + h() - 5 - static_cast<int>((c.history[i - 1] - lo) * (h() - 10) / (hi - lo));
                int y2 = y() + h() - 5 - static_cast<int>((c.history[i] - lo) * (h() - 10) / (hi - lo));
                fl_line(x1, y1, x2, y2);
            }
        }
        fl_pop_clip();
    }
};
ChartPlot *chartPlot{};
bool evaluateTick(const Row &row, std::string &error)
{
    if (app.rows.size() == signaldesk::MaxRows)
    {
        error = "Recording reached 100000 rows. Export it and start a new simulation";
        return false;
    }
    auto candidate = app.rows;
    candidate.push_back(row);
    auto charts = app.charts;
    auto alerts = app.alerts;
    for (auto &c : charts)
    {
        auto vals = windowFor(candidate, c);
        EvalContext x;
        x.values = &vals;
        x.configuredWindow = c.window;
        x.threshold = c.threshold;
        x.hysteresis = c.hysteresis;
        x.armed = c.armed ? 1 : 0;
        if (!engine->run(x, error))
            return false;
        c.mean = x.mean;
        c.minimum = x.minimum;
        c.maximum = x.maximum;
        c.armed = x.nextArmed != 0;
        c.history.push_back(row.value[c.source]);
        if (c.history.size() > 240)
            c.history.erase(c.history.begin());
        if (x.alert)
            alerts.push_back({row.time, c.title, c.mean, c.threshold});
    }
    app.rows.swap(candidate);
    app.charts.swap(charts);
    app.alerts.swap(alerts);
    app.recordingDirty = !app.replay;
    return true;
}
Row greenhouse(unsigned t)
{
    Row r;
    r.time = t;
    double temp = 22.0 + 2.0 * std::sin(r.time * 0.12);
    double h = 54.0 + 8.0 * std::sin(r.time * 0.07 + 1.1);
    double p = 82.0 + 8.0 * std::sin(r.time * 0.2);
    double fan = 35.0 + 12.0 * std::sin(r.time * 0.16);
    if (r.time % 73 > 58)
    {
        temp += 7.0;
        fan += 22.0;
        h -= 11.0;
    }
    r.value[0] = temp;
    r.value[1] = h;
    r.value[2] = p;
    r.value[3] = std::clamp(fan, 0.0, 100.0);
    return r;
}
bool tick(std::string &err)
{
    if (app.replay)
    {
        if (app.replayAt >= app.rows.size())
            return true;
        const std::size_t next = app.replayAt + 1;
        auto charts = app.charts;
        auto alerts = app.alerts;
        for (auto &c : charts)
        {
            auto vals = windowForPrefix(app.rows, next, c);
            EvalContext x;
            x.values = &vals;
            x.configuredWindow = c.window;
            x.threshold = c.threshold;
            x.hysteresis = c.hysteresis;
            x.armed = c.armed ? 1 : 0;
            if (!engine->run(x, err))
                return false;
            c.mean = x.mean;
            c.minimum = x.minimum;
            c.maximum = x.maximum;
            c.armed = x.nextArmed != 0;
            c.history.push_back(app.rows[app.replayAt].value[c.source]);
            if (c.history.size() > 240)
                c.history.erase(c.history.begin());
            if (x.alert)
                alerts.push_back({app.rows[app.replayAt].time, c.title, c.mean, c.threshold});
        }
        app.charts.swap(charts);
        app.alerts.swap(alerts);
        app.replayAt = next;
        return true;
    }
    const unsigned next = app.tick + 1;
    if (!evaluateTick(greenhouse(next), err))
        return false;
    app.tick = next;
    return true;
}
void redraw()
{
    if (!chartList)
        return;
    chartList->clear();
    for (std::size_t i = 0; i < app.charts.size(); ++i)
    {
        const auto &c = app.charts[i];
        char s[256];
        std::snprintf(s, sizeof(s), "%s | %s | window %d | avg %.1f min %.1f max %.1f | limit %.1f %s", c.title.c_str(),
                      sourceName(c.source).c_str(), c.window, c.mean, c.minimum, c.maximum, c.threshold,
                      c.armed ? "armed" : "quiet");
        chartList->add(s);
    }
    if (app.edit >= 0 && app.edit < static_cast<int>(app.charts.size()))
        chartList->select(app.edit + 1);
    alertList->clear();
    for (const auto &a : app.alerts)
    {
        char s[256];
        std::snprintf(s, sizeof(s), "t%llu %s mean %.2f over %.2f", a.time, a.title.c_str(), a.mean, a.threshold);
        alertList->add(s);
    }
    if (playButton)
        playButton->label(app.paused ? "Play" : "Pause");
    if (modeStatus)
    {
        char s[160];
        if (app.replay)
            std::snprintf(s, sizeof(s), "Replay %s | row %llu / %llu",
                          app.replayAt >= app.rows.size() ? "complete" : (app.paused ? "paused" : "playing"),
                          static_cast<unsigned long long>(app.replayAt),
                          static_cast<unsigned long long>(app.rows.size()));
        else
            std::snprintf(s, sizeof(s), "Simulation %s | tick %u", app.paused ? "paused" : "playing", app.tick);
        modeStatus->copy_label(s);
    }
    if (chartPlot)
        chartPlot->redraw();
    if (window)
        window->redraw();
}
void timer(void *)
{
    timerActive = false;
    if (!app.paused)
    {
        std::string e;
        if (!tick(e))
        {
            app.paused = true;
            fl_alert("Monitoring paused: %s", e.c_str());
        }
        if (app.replay && app.replayAt >= app.rows.size())
            app.paused = true;
        redraw();
        scheduleTimer();
    }
}
void scheduleTimer()
{
    if (!app.paused && !timerActive)
    {
        timerActive = true;
        Fl::add_timeout(.5, timer);
    }
}
void onPlay(Fl_Widget *, void *)
{
    if (!app.paused)
    {
        pausePlayback();
        redraw();
        return;
    }
    if (!resolveEditor())
        return;
    app.paused = false;
    if (app.replay && app.replayAt >= app.rows.size())
    {
        app.replayAt = 0;
        app.alerts.clear();
        for (auto &c : app.charts)
        {
            c.armed = true;
            c.history.clear();
            c.mean = c.minimum = c.maximum = 0;
        }
    }
    scheduleTimer();
    redraw();
}
bool confirmDiscard(bool dirty, const char *what)
{
    return !dirty || fl_choice("Discard unsaved %s?", "Cancel", "Discard", nullptr, what) == 1;
}
void onStep(Fl_Widget *, void *)
{
    pausePlayback();
    if (!resolveEditor())
        return;
    std::string e;
    if (!tick(e))
        fl_alert("Step failed: %s", e.c_str());
    redraw();
}
void onReset(Fl_Widget *, void *)
{
    if (!app.replay)
    {
        onNewSimulation(nullptr, nullptr);
        return;
    }
    pausePlayback();
    app.replayAt = 0;
    app.alerts.clear();
    for (auto &c : app.charts)
    {
        c.armed = true;
        c.history.clear();
        c.mean = c.minimum = c.maximum = 0;
    }
    redraw();
}
std::array<std::string, 5> editorValues()
{
    return {titleInput->value(), std::to_string(sourceInput->value()), windowInput->value(), thresholdInput->value(),
            hystInput->value()};
}
void loadEditor()
{
    const auto &c = app.charts.at(app.edit);
    titleInput->value(c.title.c_str());
    sourceInput->value(c.source);
    windowInput->value(std::to_string(c.window).c_str());
    thresholdInput->value(signaldesk::formatNumber(c.threshold).c_str());
    hystInput->value(signaldesk::formatNumber(c.hysteresis).c_str());
    editorBaseline = editorValues();
    editorReady = true;
}
bool applyEditor()
{
    pausePlayback();
    if (app.edit < 0 || app.edit >= static_cast<int>(app.charts.size()))
        return false;
    auto c = app.charts[app.edit];
    double t, h;
    int n;
    if (!parseInt(windowInput->value(), n) || n < 1 || n > 10000 || !parseNumber(thresholdInput->value(), t) ||
        !parseNumber(hystInput->value(), h) || h < 0 || sourceInput->value() < 0 || sourceInput->value() >= kFeeds ||
        std::string(titleInput->value()).empty() || std::string(titleInput->value()).size() > 256)
    {
        fl_alert("Enter a title, source, window from 1 to 10000, finite threshold, and nonnegative hysteresis.");
        return false;
    }
    c.title = titleInput->value();
    c.source = sourceInput->value();
    c.window = n;
    c.threshold = t;
    c.hysteresis = h;
    try
    {
        (void)samples::quote(c.title);
    }
    catch (const std::exception &error)
    {
        fl_alert("Invalid chart title: %s", error.what());
        return false;
    }
    c.armed = true;
    c.mean = c.minimum = c.maximum = 0;
    c.history.clear();
    app.charts[app.edit] = c;
    app.layoutDirty = true;
    editorBaseline = editorValues();
    return true;
}
bool resolveEditor()
{
    if (!editorReady || editorValues() == editorBaseline)
        return true;
    const int answer = fl_choice("Apply the pending chart edits?", "Cancel", "Discard edits", "Apply");
    if (answer == 0)
        return false;
    if (answer == 1)
    {
        loadEditor();
        return true;
    }
    return applyEditor();
}
void onSelect(Fl_Widget *, void *)
{
    const int selected = chartList->value() - 1;
    if (selected < 0 || selected >= static_cast<int>(app.charts.size()))
        return;
    pausePlayback();
    if (!resolveEditor())
    {
        chartList->select(app.edit + 1);
        return;
    }
    app.edit = selected;
    loadEditor();
    redraw();
}
void onApply(Fl_Widget *, void *)
{
    (void)applyEditor();
    redraw();
}
void onAdd(Fl_Widget *, void *)
{
    pausePlayback();
    if (!resolveEditor())
        return;
    if (app.charts.size() >= kMaxCharts)
    {
        fl_alert("A layout can contain at most six charts.");
        return;
    }
    app.charts.push_back({"New chart", 0, 12, 28, 2});
    app.edit = static_cast<int>(app.charts.size()) - 1;
    app.layoutDirty = true;
    loadEditor();
    redraw();
}
void onRemove(Fl_Widget *, void *)
{
    pausePlayback();
    if (!resolveEditor())
        return;
    if (app.charts.size() <= 1)
    {
        fl_alert("A layout must contain at least one chart.");
        return;
    }
    if (app.edit < 0 || app.edit >= static_cast<int>(app.charts.size()))
        return;
    app.charts.erase(app.charts.begin() + app.edit);
    app.edit = std::min(app.edit, static_cast<int>(app.charts.size()) - 1);
    app.layoutDirty = true;
    loadEditor();
    redraw();
}
void saveLayout(const fs::path &p)
{
    std::vector<signaldesk::ChartConfig> configs;
    for (const auto &c : app.charts)
        configs.push_back({c.title, c.source, c.window, c.threshold, c.hysteresis});
    signaldesk::writeLayout(p, configs);
    app.layout = p;
    app.layoutDirty = false;
}
fs::path normalized(const fs::path &p);
void protectOutput(const fs::path &p, bool layoutOutput = false);
void onSave(Fl_Widget *, void *)
{
    pausePlayback();
    if (!resolveEditor())
        return;
    Fl_Native_File_Chooser f;
    f.title("Save SignalDesk layout");
    f.type(Fl_Native_File_Chooser::BROWSE_SAVE_FILE);
    f.options(Fl_Native_File_Chooser::SAVEAS_CONFIRM);
    f.preset_file("greenhouse.sdl");
    f.filter("SignalDesk layout\t*.sdl");
    if (f.show() == 0 && f.filename())
        try
        {
            protectOutput(f.filename(), true);
            saveLayout(f.filename());
        }
        catch (const std::exception &e)
        {
            fl_alert("Save failed: %s", e.what());
        }
}
void onLoad(Fl_Widget *, void *)
{
    pausePlayback();
    if (!resolveEditor())
        return;
    if (!confirmDiscard(app.layoutDirty, "layout"))
        return;
    Fl_Native_File_Chooser f;
    f.title("Open SignalDesk layout");
    f.type(Fl_Native_File_Chooser::BROWSE_FILE);
    f.filter("SignalDesk layout\t*.sdl");
    if (f.show() != 0 || !f.filename())
        return;
    try
    {
        auto configs = signaldesk::readLayout(f.filename());
        std::vector<Chart> cs;
        for (const auto &v : configs)
        {
            Chart c;
            c.title = v.title;
            c.source = v.source;
            c.window = v.window;
            c.threshold = v.threshold;
            c.hysteresis = v.hysteresis;
            cs.push_back(c);
        }
        app.charts = std::move(cs);
        app.edit = 0;
        app.layout = f.filename();
        app.layoutDirty = false;
        loadEditor();
        redraw();
    }
    catch (const std::exception &e)
    {
        fl_alert("Layout not loaded: %s", e.what());
    }
}
fs::path normalized(const fs::path &p)
{
    std::error_code ec;
    auto a = fs::weakly_canonical(p, ec);
    auto result = ec ? fs::absolute(p).lexically_normal() : a;
#ifdef _WIN32
    auto value = result.string();
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    result = fs::path(value);
#endif
    return result;
}
void protectOutput(const fs::path &p, bool layoutOutput)
{
    samples::requireDataDestination(p);
    const auto same = [&](const fs::path &other)
    {
        return !other.empty() &&
               (normalized(p) == normalized(other) || (fs::exists(p) && fs::exists(other) && fs::equivalent(p, other)));
    };
    if (same(app.sourcePath))
        throw std::runtime_error("Output cannot replace the imported source recording");
    if (!layoutOutput && same(app.layout))
        throw std::runtime_error("Recording or alert output cannot replace the saved layout");
}
void exportCsv(const fs::path &p)
{
    protectOutput(p, false);
    signaldesk::writeRecording(p, app.rows);
    app.recordingDirty = false;
}
void onExport(Fl_Widget *, void *)
{
    pausePlayback();
    Fl_Native_File_Chooser f;
    f.title("Export recorded CSV");
    f.type(Fl_Native_File_Chooser::BROWSE_SAVE_FILE);
    f.options(Fl_Native_File_Chooser::SAVEAS_CONFIRM);
    f.preset_file("greenhouse.csv");
    f.filter("CSV\t*.csv");
    if (f.show() == 0 && f.filename())
        try
        {
            exportCsv(f.filename());
        }
        catch (const std::exception &e)
        {
            fl_alert("Export failed: %s", e.what());
        }
}
std::string csvQuote(const std::string &s)
{
    std::string o = "\"";
    for (char c : s)
    {
        if (c == '\"')
            o += '\"';
        o += c;
    }
    return o + '\"';
}
void onAlerts(Fl_Widget *, void *)
{
    pausePlayback();
    Fl_Native_File_Chooser f;
    f.title("Export alert history");
    f.type(Fl_Native_File_Chooser::BROWSE_SAVE_FILE);
    f.options(Fl_Native_File_Chooser::SAVEAS_CONFIRM);
    f.preset_file("alerts.csv");
    f.filter("CSV\t*.csv");
    if (f.show() == 0 && f.filename())
        try
        {
            protectOutput(f.filename(), false);
            samples::writeAtomically(f.filename(),
                                     [](std::ostream &o)
                                     {
                                         o << "time,title,mean,threshold\n" << std::setprecision(17);
                                         for (auto &a : app.alerts)
                                             o << a.time << ',' << csvQuote(a.title) << ',' << a.mean << ','
                                               << a.threshold << '\n';
                                     });
        }
        catch (const std::exception &e)
        {
            fl_alert("Export failed: %s", e.what());
        }
}
void onReloadPolicy(Fl_Widget *, void *)
{
    pausePlayback();
    std::string error;
    if (!engine->load(dashboardDirectory, error))
        fl_alert("Policy not reloaded: %s", error.c_str());
    else
        fl_message("Dashboard policy reloaded.");
    redraw();
}
void onReplay(Fl_Widget *, void *)
{
    pausePlayback();
    if (!confirmDiscard(app.recordingDirty, "recording"))
        return;
    Fl_Native_File_Chooser f;
    f.title("Import recorded greenhouse CSV");
    f.type(Fl_Native_File_Chooser::BROWSE_FILE);
    f.filter("CSV\t*.csv");
    if (f.show() != 0 || !f.filename())
        return;
    try
    {
        auto rows = signaldesk::readRecording(f.filename());
        app.rows = std::move(rows);
        app.sourcePath = normalized(f.filename());
        app.replay = true;
        app.replayAt = 0;
        for (auto &c : app.charts)
        {
            c.armed = true;
            c.history.clear();
            c.mean = c.minimum = c.maximum = 0;
        }
        app.alerts.clear();
        app.recordingDirty = false;
        redraw();
    }
    catch (const std::exception &e)
    {
        fl_alert("Recording not loaded: %s", e.what());
    }
}
void onNewSimulation(Fl_Widget *, void *)
{
    pausePlayback();
    if (!confirmDiscard(app.recordingDirty, "recording"))
        return;
    app.rows.clear();
    app.replay = false;
    app.replayAt = 0;
    app.tick = 0;
    app.alerts.clear();
    app.recordingDirty = false;
    for (auto &c : app.charts)
    {
        c.armed = true;
        c.history.clear();
        c.mean = c.minimum = c.maximum = 0;
    }
    redraw();
}
void onClose(Fl_Widget *, void *)
{
    pausePlayback();
    if (!resolveEditor())
        return;
    if (!confirmDiscard(app.layoutDirty, "layout") || !confirmDiscard(app.recordingDirty, "recording"))
        return;
    window->hide();
}
bool verify(const fs::path &dash, const fs::path &testRoot)
{
    try
    {
        auto root = samples::testDirectory(testRoot.empty() ? fs::path(".") / "test-tmp" / "SignalDesk" : testRoot);
        engine = std::make_unique<Engine>();
        std::string error;
        if (!engine->load(dash, error))
            throw std::runtime_error(error);
        std::vector<signaldesk::ChartConfig> cfg{{"A, \"quoted\"", 0, 12, 28, 2}, {"humidity", 1, 7, 65, 3}};
        for (double original : {0.0000001, 1.0 / 3.0, -123456.789})
        {
            double restored = 0;
            if (!signaldesk::finiteNumber(signaldesk::formatNumber(original), restored) || restored != original)
                throw std::runtime_error("Chart editor numeric formatting loses precision");
        }
        auto parsed = signaldesk::parseLayout(signaldesk::serializeLayout(cfg));
        if (parsed.size() != 2 || parsed[0].title != cfg[0].title || parsed[1].window != 7)
            throw std::runtime_error("layout production parser roundtrip failed");
        auto layoutFile = root / "roundtrip.sdl";
        signaldesk::writeLayout(layoutFile, cfg);
        auto loaded = signaldesk::readLayout(layoutFile);
        if (loaded.size() != cfg.size() || loaded[0].title != cfg[0].title)
            throw std::runtime_error("layout file roundtrip failed");
        cfg[0].threshold = 31;
        signaldesk::writeLayout(layoutFile, cfg);
        if (signaldesk::readLayout(layoutFile)[0].threshold != 31)
            throw std::runtime_error("layout overwrite failed");
        {
            std::ofstream keep(layoutFile);
            keep << "preserve-me";
        }
        bool rejected = false;
        try
        {
            auto bad = cfg;
            bad[0].window = 0;
            signaldesk::writeLayout(layoutFile, bad);
        }
        catch (...)
        {
            rejected = true;
        }
        if (!rejected || samples::readText(layoutFile, 100) != "preserve-me")
            throw std::runtime_error("invalid layout write did not preserve destination");
        Row r1{1, {1, 2, 3, 4}}, r2{2, {5, 6, 7, 8}};
        std::vector<Row> rec{r1, r2};
        auto rec2 = signaldesk::parseRecording(signaldesk::serializeRecording(rec));
        if (rec2.size() != 2 || rec2[1].value[2] != 7)
            throw std::runtime_error("CSV parser roundtrip failed");
        auto csvFile = root / "roundtrip.csv";
        signaldesk::writeRecording(csvFile, rec);
        auto rec3 = signaldesk::readRecording(csvFile);
        if (rec3.size() != 2 || rec3[0].value[3] != 4)
            throw std::runtime_error("CSV file roundtrip failed");
        rec[0].value[0] = 9;
        signaldesk::writeRecording(csvFile, rec);
        if (signaldesk::readRecording(csvFile)[0].value[0] != 9)
            throw std::runtime_error("CSV overwrite failed");
        {
            std::ofstream keep(csvFile);
            keep << "preserve-me";
        }
        rejected = false;
        try
        {
            auto bad = rec;
            bad[0].value[0] = std::numeric_limits<double>::infinity();
            signaldesk::writeRecording(csvFile, bad);
        }
        catch (...)
        {
            rejected = true;
        }
        if (!rejected || samples::readText(csvFile, 100) != "preserve-me")
            throw std::runtime_error("invalid CSV write did not preserve destination");
        rejected = false;
        try
        {
            signaldesk::parseRecording("time,temperature_C,humidity_pct,power_W,fan_pct\n1,2,nan,3,4\n");
        }
        catch (...)
        {
            rejected = true;
        }
        if (!rejected)
            throw std::runtime_error("non-finite CSV input accepted");
        Chart c{"test", 0, 4, 5, 1};
        std::vector<double> vals{1, 3, 5, 7};
        EvalContext x;
        x.values = &vals;
        x.configuredWindow = 4;
        x.threshold = 5;
        x.hysteresis = 1;
        x.armed = 1;
        if (!engine->run(x, error) || std::abs(x.mean - 4) > 1e-9 || x.minimum != 1 || x.maximum != 7 || x.alert != 0)
            throw std::runtime_error("known window statistics failed");
        std::vector<double> largestWindow(10000, 1.0);
        EvalContext large;
        large.values = &largestWindow;
        large.configuredWindow = 10000;
        large.threshold = 5;
        large.hysteresis = 1;
        large.armed = 1;
        if (!engine->run(large, error) || large.mean != 1 || large.minimum != 1 || large.maximum != 1)
            throw std::runtime_error("The largest supported window cannot run: " + error);
        vals = {5, 7, 9, 11};
        x = {};
        x.values = &vals;
        x.configuredWindow = 4;
        x.threshold = 5;
        x.hysteresis = 1;
        x.armed = 1;
        if (!engine->run(x, error) || x.alert != 1 || x.nextArmed != 0)
            throw std::runtime_error("threshold crossing failed");
        x = {};
        x.values = &vals;
        x.configuredWindow = 4;
        x.threshold = 5;
        x.hysteresis = 1;
        x.armed = 0;
        if (!engine->run(x, error) || x.alert != 0 || x.nextArmed != 0)
            throw std::runtime_error("alert repeated while disarmed");
        vals = {0, 1, 2, 3};
        x = {};
        x.values = &vals;
        x.configuredWindow = 4;
        x.threshold = 5;
        x.hysteresis = 1;
        x.armed = 0;
        if (!engine->run(x, error) || x.nextArmed != 1)
            throw std::runtime_error("hysteresis rearm failed");
        if (engine->load(dash / "missing", error))
            throw std::runtime_error("bad reload unexpectedly compiled");
        vals = {1, 3, 5, 7};
        x = {};
        x.values = &vals;
        x.configuredWindow = 4;
        x.threshold = 5;
        x.hysteresis = 1;
        x.armed = 1;
        if (!engine->run(x, error) || std::abs(x.mean - 4) > 1e-9)
            throw std::runtime_error("failed reload replaced active policy");
        app.charts = {{"reload probe", 0, 4, 5, 1}};
        auto makePolicy = [&](const std::string &name, const std::string &code)
        {
            auto dir = root / name;
            fs::create_directories(dir);
            std::ofstream out(dir / "alerts.ks");
            out << code;
            out.close();
            return dir;
        };
        std::vector<std::pair<std::string, std::string>> badPolicies{
            {"badcompile", "module alerts; this is not valid;"},
            {"badprotocol", "module alerts; extern emit(mean, minimum, maximum, alert, next_armed) -> void; export "
                            "func evaluate() -> void { emit(0,0,0,0,1); emit(0,0,0,0,1); }"},
            {"badruntime", "module alerts; extern emit(mean, minimum, maximum, alert, next_armed) -> void; export func "
                           "evaluate() -> void { while (1==1) {} emit(0,0,0,0,1); }"}};
        for (const auto &fixture : badPolicies)
        {
            auto before = app.charts[0].mean;
            auto dir = makePolicy(fixture.first, fixture.second);
            if (engine->load(dir, error))
                throw std::runtime_error(fixture.first + " policy unexpectedly loaded");
            if (app.charts.size() != 1 || app.charts[0].mean != before)
                throw std::runtime_error("failed reload changed chart state");
            x = {};
            x.values = &vals;
            x.configuredWindow = 4;
            x.threshold = 5;
            x.hysteresis = 1;
            x.armed = 1;
            if (!engine->run(x, error) || std::abs(x.mean - 4) > 1e-9)
                throw std::runtime_error("failed reload replaced active VM");
        }
        auto lateDir = makePolicy(
            "latefailure",
            "module alerts; extern threshold() -> value; extern emit(mean, minimum, maximum, alert, next_armed) -> "
            "void; export func evaluate() -> void { if (threshold() > 999) { emit(0,0,0,2,0); } else { "
            "emit(0,0,0,0,1); } }");
        app.charts.clear();
        for (int i = 0; i < 6; ++i)
        {
            Chart q;
            q.title = "late" + std::to_string(i);
            q.source = i % 4;
            q.window = 4;
            q.threshold = 5;
            q.hysteresis = 1;
            app.charts.push_back(q);
        }
        if (!engine->load(lateDir, error))
            throw std::runtime_error("valid late-failure fixture did not load: " + error);
        app.charts.back().threshold = 1000;
        app.rows.clear();
        app.alerts.clear();
        app.replay = false;
        app.tick = 0;
        auto stateBefore = app.charts;
        if (tick(error))
            throw std::runtime_error("late chart failure unexpectedly committed");
        if (!app.rows.empty() || !app.alerts.empty() || app.tick != 0 || app.charts.size() != stateBefore.size())
            throw std::runtime_error("late chart failure partially committed tick state");
        for (std::size_t i = 0; i < app.charts.size(); ++i)
            if (app.charts[i].history.size() != stateBefore[i].history.size() ||
                app.charts[i].armed != stateBefore[i].armed)
                throw std::runtime_error("late chart failure changed chart state");
        app.charts.clear();
        if (!engine->load(dash, error))
            throw std::runtime_error("cannot restore production policy after failure fixture: " + error);
        auto recorded = signaldesk::readRecording(dash / "recorded_greenhouse.csv");
        if (recorded.size() != 2000)
            throw std::runtime_error("experiment is not 2,000 rows");
        app.rows = recorded;
        app.charts.clear();
        for (int i = 0; i < 6; ++i)
        {
            Chart q;
            q.title = "prefix" + std::to_string(i);
            q.source = i % 4;
            q.window = 12 + i;
            q.threshold = 500 + i;
            q.hysteresis = 2;
            app.charts.push_back(q);
        }
        app.alerts.clear();
        app.replay = true;
        app.replayAt = 0;
        if (!tick(error) || app.replayAt != 1)
            throw std::runtime_error("first replay tick did not advance exactly once");
        for (std::size_t i = 0; i < app.charts.size(); ++i)
            if (app.charts[i].mean != static_cast<double>(static_cast<float>(recorded[0].value[app.charts[i].source])))
                throw std::runtime_error("replay used future rows in the first window");
        app.replayAt = 0;
        app.alerts.clear();
        for (auto &q : app.charts)
        {
            q.armed = true;
            q.history.clear();
            q.mean = q.minimum = q.maximum = 0;
        }
        auto savedRows = std::move(app.rows);
        auto savedCharts = std::move(app.charts);
        auto savedAlerts = std::move(app.alerts);
        auto savedReplay = app.replay;
        auto savedCursor = app.replayAt;
        app.rows = recorded;
        app.charts.clear();
        for (int i = 0; i < 6; ++i)
        {
            Chart q;
            q.title = "chart" + std::to_string(i);
            q.source = i % 4;
            q.window = 12 + i;
            q.threshold = 50 + i * 10;
            q.hysteresis = 2;
            app.charts.push_back(q);
        }
        app.alerts.clear();
        app.replay = true;
        app.replayAt = 0;
        if (!engine->load(dash, error))
            throw std::runtime_error("six chart policy validation failed: " + error);
        std::vector<double> deterministic;
        for (std::size_t i = 0; i < recorded.size(); ++i)
        {
            if (!tick(error))
                throw std::runtime_error("six chart replay failed: " + error);
            for (const auto &q : app.charts)
                deterministic.push_back(q.mean);
        }
        if (app.replayAt != 2000)
            throw std::runtime_error("replay cursor did not reach EOF");
        auto firstAlerts = app.alerts;
        if (!tick(error) || app.replayAt != 2000)
            throw std::runtime_error("EOF replay changed the cursor");
        app.charts.clear();
        for (int i = 0; i < 6; ++i)
        {
            Chart q;
            q.title = "chart" + std::to_string(i);
            q.source = i % 4;
            q.window = 12 + i;
            q.threshold = 50 + i * 10;
            q.hysteresis = 2;
            app.charts.push_back(q);
        }
        app.alerts.clear();
        app.replayAt = 0;
        std::vector<double> repeated;
        for (std::size_t i = 0; i < recorded.size(); ++i)
        {
            if (!tick(error))
                throw std::runtime_error("repeat replay failed: " + error);
            for (const auto &q : app.charts)
                repeated.push_back(q.mean);
        }
        bool alertsEqual = firstAlerts.size() == app.alerts.size();
        for (std::size_t i = 0; alertsEqual && i < firstAlerts.size(); ++i)
            alertsEqual = firstAlerts[i].time == app.alerts[i].time && firstAlerts[i].title == app.alerts[i].title &&
                          firstAlerts[i].mean == app.alerts[i].mean &&
                          firstAlerts[i].threshold == app.alerts[i].threshold;
        if (deterministic != repeated || !alertsEqual)
            throw std::runtime_error("six chart replay was not deterministic");
        app.rows = std::move(savedRows);
        app.charts = std::move(savedCharts);
        app.alerts = std::move(savedAlerts);
        app.replay = savedReplay;
        app.replayAt = savedCursor;
        std::cout << "SignalDesk verify passed: production model I/O, exact stats, hysteresis, failed reload "
                     "retention, deterministic 2,000-row six-chart replay\n";
        return true;
    }
    catch (const std::exception &e)
    {
        std::cerr << "SignalDesk verify failed: " << e.what() << '\n';
        return false;
    }
}
} // namespace
int main(int argc, char **argv)
{
    bool help = false, verifyMode = false;
    fs::path testRoot, preview;
    for (int i = 1; i < argc; ++i)
    {
        std::string a = argv[i];
        if (a == "--help")
            help = true;
        else if (a == "--verify")
            verifyMode = true;
        else if (a == "--test-root" && i + 1 < argc)
            testRoot = argv[++i];
        else if (a == "--preview" && i + 1 < argc)
            preview = argv[++i];
        else
        {
            std::cerr << "Unknown or incomplete option: " << a << '\n';
            return 2;
        }
    }
    if (help)
    {
        std::cout << "SignalDesk: greenhouse monitoring dashboard (simulation)\n--help --verify [--test-root DIR]\n";
        return 0;
    }
    if (verifyMode)
    {
        fs::path dash = fs::path(argv[0]).parent_path() / "dash";
        if (!fs::is_directory(dash))
            dash = fs::current_path() / "Samples" / "SignalDesk" / "dash";
        return verify(dash, testRoot) ? 0 : 1;
    }
    if (!testRoot.empty())
    {
        std::cerr << "--test-root requires --verify\n";
        return 2;
    }
    try
    {
        dashboardDirectory = fs::path(argv[0]).parent_path() / "dash";
        if (!fs::is_directory(dashboardDirectory))
            dashboardDirectory = fs::current_path() / "Samples" / "SignalDesk" / "dash";
        app.charts = {{"Air temperature", 0, 12, 28, 2},
                      {"Relative humidity", 1, 12, 72, 4},
                      {"Electrical load", 2, 12, 100, 8},
                      {"Fan speed", 3, 12, 70, 8}};
        engine = std::make_unique<Engine>();
        std::string error;
        if (!engine->load(dashboardDirectory, error))
            throw std::runtime_error(error);
        Fl_Window appWindow(960, 700, "SignalDesk | Greenhouse monitoring simulation");
        window = &appWindow;
        chartList = new Fl_Browser(20, 35, 920, 150, "Charts and current window statistics");
        chartList->callback(onSelect);
        chartList->type(FL_HOLD_BROWSER);
        chartList->format_char(0);
        chartList->align(FL_ALIGN_TOP_LEFT);
        chartPlot = new ChartPlot(20, 215, 920, 58, "Selected chart history");
        chartPlot->align(FL_ALIGN_TOP_LEFT);
        alertList = new Fl_Browser(20, 305, 920, 160, "Alert history");
        alertList->format_char(0);
        alertList->align(FL_ALIGN_TOP_LEFT);
        modeStatus = new Fl_Box(20, 465, 920, 20, "Simulation paused");
        modeStatus->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        playButton = new Fl_Button(20, 490, 75, 32, "Play");
        playButton->callback(onPlay);
        Fl_Button step(100, 490, 75, 32, "Step");
        step.callback(onStep);
        Fl_Button reset(180, 490, 75, 32, "Reset");
        reset.callback(onReset);
        Fl_Button load(260, 490, 90, 32, "Open layout");
        load.callback(onLoad);
        Fl_Button save(355, 490, 90, 32, "Save layout");
        save.callback(onSave);
        Fl_Button import(450, 490, 120, 32, "Replay CSV");
        import.callback(onReplay);
        Fl_Button exportButton(575, 490, 115, 32, "Export CSV");
        exportButton.callback(onExport);
        Fl_Button al(695, 490, 130, 32, "Export alerts");
        al.callback(onAlerts);
        Fl_Button fresh(830, 490, 110, 32, "New simulation");
        fresh.callback(onNewSimulation);
        titleInput = new Fl_Input(20, 555, 210, 28, "Title");
        sourceInput = new Fl_Choice(260, 555, 160, 28, "Source");
        for (int i = 0; i < 4; ++i)
            sourceInput->add(feedNames[i]);
        windowInput = new Fl_Input(500, 555, 80, 28, "Window");
        thresholdInput = new Fl_Input(650, 555, 90, 28, "Threshold");
        hystInput = new Fl_Input(810, 555, 80, 28, "Hysteresis");
        for (auto *input : {titleInput, windowInput, thresholdInput, hystInput})
            input->align(FL_ALIGN_TOP_LEFT);
        sourceInput->align(FL_ALIGN_TOP_LEFT);
        Fl_Button apply(20, 600, 90, 30, "Apply edit");
        apply.callback(onApply);
        Fl_Button add(120, 600, 70, 30, "Add chart");
        add.callback(onAdd);
        Fl_Button remove(195, 600, 85, 30, "Remove chart");
        remove.callback(onRemove);
        Fl_Button reload(290, 600, 110, 30, "Reload policy");
        reload.callback(onReloadPolicy);
        Fl_Box info(
            410, 600, 530, 45,
            "Four greenhouse feeds: temperature, humidity, power and fan. Imported recordings replay read only.");
        info.align(FL_ALIGN_LEFT | FL_ALIGN_WRAP | FL_ALIGN_INSIDE);
        window->end();
        window->callback(onClose);
        loadEditor();
        redraw();
        if (!preview.empty())
        {
            samples::renderPreview(*window, preview);
            return 0;
        }
        window->show(argc, argv);
        Fl::run();
    }
    catch (const std::exception &e)
    {
        std::cerr << "SignalDesk: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
