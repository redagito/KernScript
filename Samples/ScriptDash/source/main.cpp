// ScriptDash: a scripted instruments dashboard stressing window math.
//
// Data scripts live in dash/*.ks on disk: stats.ks is the shared hub
// (avg/min/max over host-held windows, clamp, window sizes) and alerts.ks
// + views.ks each import stats, so every compile is a genuine diamond
// (stats loads once, shared). The host compiles one anonymous root
// importing all three modules, then drives the dashboard as MANY small
// sequential calls on a persistent VM: 6 wrappers x 3 charts = 18 calls
// per 5 Hz tick, each pulling window samples through valued externs and
// pushing results back through statement externs. Scripts never hold
// state across calls: ring buffers, the tick cursor, view mode and the
// hysteresis armed flags all live host-side.
//
// Host protocol (tiny, fixed; mirrored in dash/*.ks):
//   src_count(src) value-use: samples buffered for source 0-3.
//   src_val(src, idx) value-use: sample idx (0 = oldest) of source src.
//   view_mode() value-use: host view preset 0-2 (View A/B/C buttons).
//   is_armed(chart) value-use / set_armed(chart, flag) statement-use:
//     hysteresis flag per chart, read and written by alerts::check.
//   push_stat(chart, kind, value) statement-use: kind 0 avg, 1 min,
//     2 max, 3 limit; stored for the strip charts and status line.
//   push_src(chart, src) statement-use: script-chosen source per chart.
//   push_alert(chart, code) statement-use: code 1 appends one timestamped
//     line "[tN] CHART c HIGH over L" to the alert browser.
//
// Data sources are portable only: wall-clock seconds, a sine+noise
// generator (deterministic LCG), a bounded counter/random-walk generator
// and a tailed text feed (dash_feed.txt, seeded deterministically at
// startup when missing). The Spike button forces the sine source to 100
// for 8 ticks so --verify can cross a threshold deterministically.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Browser.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Widget.H>
#include <FL/Fl_Window.H>
#include <FL/fl_draw.H>

#include <kern/Kern.h>

namespace
{

constexpr const char *kWindowTitle = "KernScript Dash";
constexpr int kCharts = 3;
constexpr int kSources = 4;
constexpr int kCapacity = 120;
constexpr int kDrawLast = 60;
constexpr int kSpikeTicks = 8;
constexpr double kTickSeconds = 0.2;
constexpr double kGaugeLo = -15.0;
constexpr double kGaugeHi = 115.0;

const char *kSourceNames[kSources] = {"clock", "sine", "walk", "file"};
constexpr int kCallsPerTick = 18;

std::string g_argv0;
std::string g_dashDir;
std::string g_feedPath;
std::vector<std::string> g_searchPaths;
kern::IVirtualMachine *g_vm = nullptr;
std::chrono::steady_clock::time_point g_startTime;

long long g_tick = 0;
bool g_paused = false;
int g_viewMode = 0;
int g_spikeLeft = 0;
unsigned int g_rng = 0x12345678u;
double g_walk = 0.0;
std::size_t g_filePos = 0;
std::vector<float> g_feed;
std::vector<float> g_samples[kSources];
int g_chartSrc[kCharts] = {0, 1, 2};
float g_chartAvg[kCharts] = {0.0f, 0.0f, 0.0f};
float g_chartMin[kCharts] = {0.0f, 0.0f, 0.0f};
float g_chartMax[kCharts] = {0.0f, 0.0f, 0.0f};
float g_chartLimit[kCharts] = {8.0f, 12.0f, 6.0f};
int g_armed[kCharts] = {1, 1, 1};
int g_loadCount = 0;
int g_callCount = 0;
long long g_compileMs = 0;

Fl_Window *g_window = nullptr;
Fl_Widget *g_strips[kCharts] = {nullptr, nullptr, nullptr};
Fl_Browser *g_alerts = nullptr;
Fl_Button *g_viewButtons[3] = {nullptr, nullptr, nullptr};
Fl_Button *g_pauseButton = nullptr;
Fl_Button *g_reloadButton = nullptr;
Fl_Button *g_spikeButton = nullptr;
Fl_Button *g_clearButton = nullptr;
Fl_Box *g_status = nullptr;

unsigned int nextRand()
{
    g_rng = g_rng * 1103515245u + 12345u;
    return (g_rng >> 16) & 32767u;
}

double secondsSinceStart()
{
    const auto now = std::chrono::steady_clock::now();
    return std::chrono::duration<double>(now - g_startTime).count();
}

int src_count_value(int src)
{
    if (src < 0 || src >= kSources)
    {
        return 0;
    }
    return static_cast<int>(g_samples[src].size());
}

float src_val_value(int src, int idx)
{
    if (src < 0 || src >= kSources || idx < 0)
    {
        return 0.0f;
    }
    const std::size_t i = static_cast<std::size_t>(idx);
    if (i >= g_samples[src].size())
    {
        return 0.0f;
    }
    return g_samples[src][i];
}

int view_mode_value() { return g_viewMode; }

int is_armed_value(int chart)
{
    if (chart < 0 || chart >= kCharts)
    {
        return 0;
    }
    return g_armed[chart];
}

void set_armed_value(int chart, int flag)
{
    if (chart < 0 || chart >= kCharts)
    {
        return;
    }
    g_armed[chart] = flag != 0 ? 1 : 0;
}

void push_stat_value(int chart, int kind, float value)
{
    if (chart < 0 || chart >= kCharts)
    {
        return;
    }
    if (kind == 0)
    {
        g_chartAvg[chart] = value;
    }
    else if (kind == 1)
    {
        g_chartMin[chart] = value;
    }
    else if (kind == 2)
    {
        g_chartMax[chart] = value;
    }
    else if (kind == 3)
    {
        g_chartLimit[chart] = value;
    }
}

void push_src_value(int chart, int src)
{
    if (chart < 0 || chart >= kCharts || src < 0 || src >= kSources)
    {
        return;
    }
    g_chartSrc[chart] = src;
}

void push_alert_value(int chart, int code)
{
    if (code == 0 || chart < 0 || chart >= kCharts || g_alerts == nullptr)
    {
        return;
    }
    char line[96];
    std::snprintf(line, sizeof(line), "[t%lld] CHART %d HIGH over %.2f", g_tick, chart,
                  static_cast<double>(g_chartLimit[chart]));
    g_alerts->add(line);
    std::cout << "alert: " << line << '\n';
}

void pushSample(int src, float value)
{
    g_samples[src].push_back(value);
    if (g_samples[src].size() > static_cast<std::size_t>(kCapacity))
    {
        g_samples[src].erase(g_samples[src].begin());
    }
}

// Portable sources only: wall clock, sine+noise (LCG, deterministic),
// bounded counter/walk, tailed feed file cycling. Spike forces the sine
// source to 100 so threshold crossings stay deterministic.
void sampleSources()
{
    ++g_tick;
    const double now = secondsSinceStart();
    pushSample(0, static_cast<float>(now));
    float sine = 5.0f * static_cast<float>(std::sin(static_cast<double>(g_tick) * 0.25)) +
                 (static_cast<float>(nextRand() % 2000) / 1000.0f - 1.0f);
    if (g_spikeLeft > 0)
    {
        sine = 100.0f;
        --g_spikeLeft;
    }
    pushSample(1, sine);
    g_walk += static_cast<double>(nextRand() % 2000) / 1000.0 - 1.0;
    g_walk = std::max(-2.0, std::min(2.0, g_walk));
    const float walk = static_cast<float>(g_tick % 20) * 0.15f + static_cast<float>(g_walk);
    pushSample(2, walk);
    float feed = 3.5f;
    if (!g_feed.empty())
    {
        feed = g_feed[g_filePos % g_feed.size()];
        ++g_filePos;
    }
    pushSample(3, feed);
}

class StripChart : public Fl_Widget
{
public:
    StripChart(int x, int y, int w, int h, int chart) : Fl_Widget(x, y, w, h), m_chart(chart) {}

    void draw() override
    {
        fl_push_clip(x(), y(), w(), h());
        fl_color(FL_BLACK);
        fl_rectf(x(), y(), w(), h());
        const int src = g_chartSrc[m_chart];
        const std::vector<float> &data = g_samples[src];
        const std::size_t n = std::min(data.size(), static_cast<std::size_t>(kDrawLast));
        const int plotH = h() - 18;
        auto toY = [&](double v)
        {
            const double frac = (v - kGaugeLo) / (kGaugeHi - kGaugeLo);
            const double clamped = std::max(0.0, std::min(1.0, frac));
            return y() + static_cast<int>((1.0 - clamped) * plotH);
        };
        auto toX = [&](std::size_t i) { return x() + static_cast<int>(i * static_cast<std::size_t>(w()) / kDrawLast); };
        // Threshold line from script policy.
        fl_color(FL_RED);
        fl_line(x(), toY(g_chartLimit[m_chart]), x() + w(), toY(g_chartLimit[m_chart]));
        // History polyline.
        fl_color(FL_GREEN);
        const std::size_t skip = data.size() > n ? data.size() - n : 0;
        for (std::size_t i = 1; i < n; ++i)
        {
            fl_line(toX(i - 1), toY(data[skip + i - 1]), toX(i), toY(data[skip + i]));
        }
        // Average marker from script math.
        fl_color(FL_YELLOW);
        fl_line(x(), toY(g_chartAvg[m_chart]), x() + w(), toY(g_chartAvg[m_chart]));
        // Gauge bar: latest value on the fixed scale.
        fl_color(FL_DARK3);
        fl_rectf(x(), y() + plotH + 4, w(), 10);
        if (!data.empty())
        {
            const double frac = (static_cast<double>(data.back()) - kGaugeLo) / (kGaugeHi - kGaugeLo);
            const double clamped = std::max(0.0, std::min(1.0, frac));
            fl_color(FL_BLUE);
            fl_rectf(x(), y() + plotH + 4, static_cast<int>(clamped * w()), 10);
        }
        fl_color(FL_WHITE);
        char label[128];
        std::snprintf(label, sizeof(label), "C%d %s avg %.2f min %.2f max %.2f lim %.2f", m_chart, kSourceNames[src],
                      static_cast<double>(g_chartAvg[m_chart]), static_cast<double>(g_chartMin[m_chart]),
                      static_cast<double>(g_chartMax[m_chart]), static_cast<double>(g_chartLimit[m_chart]));
        fl_draw(label, x() + 4, y() + 4, w() - 8, 16, FL_ALIGN_LEFT);
        fl_pop_clip();
    }

private:
    int m_chart;
};

// Dash scripts live as separate files under dash/. Resolve robustly so
// --verify works from the build tree: exe-dir/dash wins, then
// CWD/dash, then CWD/Samples/ScriptDash/dash (repo-root runs).
bool resolveDashDir(std::string &out, int &candidate)
{
    std::vector<std::string> tries;
    try
    {
        const std::filesystem::path exe(g_argv0);
        if (exe.has_parent_path())
        {
            tries.push_back((exe.parent_path() / "dash").string());
        }
        else
        {
            tries.push_back((std::filesystem::current_path() / "dash").string());
        }
    }
    catch (const std::filesystem::filesystem_error &)
    {
        tries.push_back((std::filesystem::current_path() / "dash").string());
    }
    tries.push_back((std::filesystem::current_path() / "dash").string());
    tries.push_back((std::filesystem::current_path() / "Samples" / "ScriptDash" / "dash").string());
    for (std::size_t i = 0; i < tries.size(); ++i)
    {
        std::error_code code;
        if (std::filesystem::is_directory(tries[i], code))
        {
            out = tries[i];
            candidate = static_cast<int>(i);
            return true;
        }
    }
    std::cerr << "dash dir missing (tried " << tries.size() << " candidates:";
    for (const std::string &t : tries)
    {
        std::cerr << " [" << t << "]";
    }
    std::cerr << ")\n";
    return false;
}

bool findDash(const char *file, std::string &out)
{
    for (const std::string &dir : g_searchPaths)
    {
        const std::string full = (std::filesystem::path(dir) / file).string();
        std::error_code code;
        if (std::filesystem::is_regular_file(full, code))
        {
            out = full;
            return true;
        }
    }
    std::cerr << "dash file missing: " << file << " (searched " << g_searchPaths.size() << " dirs)\n";
    return false;
}

// Deterministic seed feed: 120 lines around 2.0-5.5, average ~3.75,
// safely below every chart limit so only the Spike button trips alerts.
bool ensureFeed()
{
    std::error_code code;
    if (std::filesystem::is_regular_file(g_feedPath, code))
    {
        return true;
    }
    std::ofstream file(g_feedPath);
    if (!file)
    {
        std::cerr << "feed write failed: " << g_feedPath << '\n';
        return false;
    }
    for (int i = 0; i < 120; ++i)
    {
        const double v = 2.0 + static_cast<double>(i % 8) * 0.5;
        char line[32];
        std::snprintf(line, sizeof(line), "%.4f", v);
        file << line << '\n';
    }
    file.close();
    std::cout << "feed seeded: " << g_feedPath << " (120 lines)\n";
    return true;
}

bool loadFeed()
{
    g_feed.clear();
    std::ifstream file(g_feedPath);
    if (!file)
    {
        std::cerr << "feed read failed: " << g_feedPath << '\n';
        return false;
    }
    std::string line;
    while (std::getline(file, line))
    {
        try
        {
            g_feed.push_back(std::stof(line));
        }
        catch (const std::exception &)
        {
            // Skip blank/trailing lines; numeric lines only.
        }
    }
    if (g_feed.empty())
    {
        std::cerr << "feed empty: " << g_feedPath << '\n';
        return false;
    }
    std::cout << "feed tailed: " << g_feed.size() << " values from " << g_feedPath << '\n';
    return true;
}

// One anonymous root importing every module; stats is the shared diamond
// hub (alerts + views import it). Every wrapper sequences extern and
// script calls through temp vars: no nested calls, so every value an
// extern leaves behind is consumed before the next call.
std::string buildRootSource()
{
    return "import stats;\nimport alerts;\nimport views;\n"
           "extern poll_chart();\n"
           "extern push_stat(chart, kind, value);\n"
           "extern push_src(chart, src);\n"
           "extern push_alert(chart, code);\n"
           "func chart_source() {\n"
           "  var chart = poll_chart();\n"
           "  var s = views::source_for_chart(chart);\n"
           "  push_src(chart, s);\n"
           "}\n"
           "func chart_avg() {\n"
           "  var chart = poll_chart();\n"
           "  var s = views::source_for_chart(chart);\n"
           "  var w = stats::disp_window();\n"
           "  var a = stats::avg(s, w);\n"
           "  push_stat(chart, 0, a);\n"
           "}\n"
           "func chart_min() {\n"
           "  var chart = poll_chart();\n"
           "  var s = views::source_for_chart(chart);\n"
           "  var w = stats::disp_window();\n"
           "  var m = stats::min_of(s, w);\n"
           "  push_stat(chart, 1, m);\n"
           "}\n"
           "func chart_max() {\n"
           "  var chart = poll_chart();\n"
           "  var s = views::source_for_chart(chart);\n"
           "  var w = stats::disp_window();\n"
           "  var m = stats::max_of(s, w);\n"
           "  push_stat(chart, 2, m);\n"
           "}\n"
           "func chart_limit() {\n"
           "  var chart = poll_chart();\n"
           "  var l = alerts::limit_for(chart);\n"
           "  push_stat(chart, 3, l);\n"
           "}\n"
           "func chart_alert() {\n"
           "  var chart = poll_chart();\n"
           "  var s = views::source_for_chart(chart);\n"
           "  var c = alerts::check_src(s, chart);\n"
           "  push_alert(chart, c);\n"
           "}\n";
}

bool loadDash()
{
    std::string probe;
    if (!findDash("stats.ks", probe))
    {
        return false;
    }
    const std::string root = buildRootSource();
    std::stringstream asmCode;
    {
        const auto start = std::chrono::steady_clock::now();
        std::istringstream input(root);
        std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
        if (!compiler->compile(input, asmCode, "<dash-root>", g_searchPaths))
        {
            std::cerr << "compile failed for dash root\n";
            return false;
        }
        const auto end = std::chrono::steady_clock::now();
        g_compileMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    }
    std::stringstream byteCode;
    {
        std::istringstream input(asmCode.str());
        std::unique_ptr<kern::IAssembler> assembler(kern::createAssembler());
        if (!assembler->assemble(input, byteCode))
        {
            std::cerr << "assemble failed for dash root\n";
            return false;
        }
    }
    try
    {
        if (!g_vm->load(byteCode))
        {
            std::cerr << "load failed for dash root\n";
            return false;
        }
    }
    catch (const kern::RuntimeError &error)
    {
        std::cerr << "load error: " << error.what() << '\n';
        return false;
    }
    ++g_loadCount;
    std::cout << "dash loaded from " << g_dashDir << " (load #" << g_loadCount << ", view " << g_viewMode << ", "
              << g_compileMs << " ms)\n";
    return true;
}

// callFunction takes only the function name, so the chart index travels
// through a host cursor the wrappers read via the valued extern
// poll_chart(). The cursor is set before each wrapper call.
int g_pollChart = 0;

int poll_chart_value() { return g_pollChart; }

bool pollOne(int chart)
{
    g_pollChart = chart;
    static const char *kWrappers[6] = {
        "chart_source", "chart_avg", "chart_min", "chart_max", "chart_limit", "chart_alert",
    };
    for (int k = 0; k < 6; ++k)
    {
        try
        {
            if (!g_vm->callFunction(kWrappers[k]))
            {
                std::cerr << "function " << kWrappers[k] << " not found\n";
                return false;
            }
        }
        catch (const kern::RuntimeError &error)
        {
            std::cerr << "runtime error in " << kWrappers[k] << " chart " << chart << ": " << error.what() << '\n';
            return false;
        }
        ++g_callCount;
    }
    return true;
}

void refreshStatus();

bool doTick()
{
    sampleSources();
    for (int c = 0; c < kCharts; ++c)
    {
        if (!pollOne(c))
        {
            return false;
        }
    }
    for (int c = 0; c < kCharts; ++c)
    {
        g_strips[c]->redraw();
    }
    refreshStatus();
    return true;
}

void tick_cb(void * /*data*/)
{
    if (g_paused)
    {
        Fl::repeat_timeout(kTickSeconds, tick_cb);
        return;
    }
    if (!doTick())
    {
        Fl::remove_timeout(tick_cb);
        return;
    }
    Fl::repeat_timeout(kTickSeconds, tick_cb);
}

void refreshStatus()
{
    char text[256];
    std::snprintf(text, sizeof(text), "tick %lld | view %d%s | loads %d | calls %d | buf %d/%d/%d/%d", g_tick,
                  g_viewMode, g_paused ? " PAUSED" : "", g_loadCount, g_callCount,
                  static_cast<int>(g_samples[0].size()), static_cast<int>(g_samples[1].size()),
                  static_cast<int>(g_samples[2].size()), static_cast<int>(g_samples[3].size()));
    g_status->copy_label(text);
}

void refreshButtons()
{
    g_viewButtons[0]->copy_label(g_viewMode == 0 ? "View A [*]" : "View A");
    g_viewButtons[1]->copy_label(g_viewMode == 1 ? "View B [*]" : "View B");
    g_viewButtons[2]->copy_label(g_viewMode == 2 ? "View C [*]" : "View C");
    g_pauseButton->copy_label(g_paused ? "Resume" : "Pause");
}

void onView(Fl_Widget * /*widget*/, void *data)
{
    g_viewMode = static_cast<int>(reinterpret_cast<std::intptr_t>(data));
    refreshButtons();
    refreshStatus();
    std::cout << "view " << g_viewMode << '\n';
}

void onPause(Fl_Widget * /*widget*/, void * /*data*/)
{
    g_paused = !g_paused;
    refreshButtons();
    refreshStatus();
    std::cout << (g_paused ? "paused" : "resumed") << " at tick " << g_tick << '\n';
}

void onReload(Fl_Widget * /*widget*/, void * /*data*/)
{
    if (!loadDash())
    {
        return;
    }
    refreshButtons();
    refreshStatus();
    for (int c = 0; c < kCharts; ++c)
    {
        g_strips[c]->redraw();
    }
    std::cout << "reloaded live at tick " << g_tick << '\n';
}

void onSpike(Fl_Widget * /*widget*/, void * /*data*/)
{
    g_spikeLeft = kSpikeTicks;
    std::cout << "spike armed for " << kSpikeTicks << " ticks\n";
}

void onClear(Fl_Widget * /*widget*/, void * /*data*/)
{
    g_alerts->clear();
    std::cout << "alerts cleared\n";
}

bool expectCounts(const char *label, long long tick, int perSource)
{
    bool ok = true;
    if (g_tick != tick)
    {
        std::cout << "MISMATCH " << label << ": tick " << g_tick << " want " << tick << '\n';
        ok = false;
    }
    for (int s = 0; s < kSources; ++s)
    {
        if (static_cast<int>(g_samples[s].size()) != perSource)
        {
            std::cout << "MISMATCH " << label << ": source " << s << " has " << g_samples[s].size() << " samples, want "
                      << perSource << '\n';
            ok = false;
        }
    }
    if (ok)
    {
        std::cout << label << ": tick " << tick << ", 4 sources x " << perSource << " samples\n";
    }
    return ok;
}

bool expectMapping(const char *label, int s0, int s1, int s2)
{
    const bool ok = g_chartSrc[0] == s0 && g_chartSrc[1] == s1 && g_chartSrc[2] == s2;
    if (ok)
    {
        std::cout << label << ": mapping (" << s0 << "," << s1 << "," << s2 << ") as scripted\n";
        return true;
    }
    std::cout << "MISMATCH " << label << ": mapping (" << g_chartSrc[0] << "," << g_chartSrc[1] << "," << g_chartSrc[2]
              << ") want (" << s0 << "," << s1 << "," << s2 << ")\n";
    return false;
}

int spikeAlertLines()
{
    int found = 0;
    for (int i = 1; i <= g_alerts->size(); ++i)
    {
        const std::string line = g_alerts->text(i) != nullptr ? g_alerts->text(i) : "";
        if (line.find("HIGH over 12.00") != std::string::npos && line.find("CHART 1") != std::string::npos)
        {
            ++found;
        }
    }
    return found;
}

int runVerify()
{
    bool ok = true;
    std::cout << "window title: " << (g_window->label() != nullptr ? g_window->label() : "(null)") << '\n';
    std::cout << "window shown: " << (g_window->shown() != 0 ? 1 : 0) << '\n';
    if (g_window->label() == nullptr || std::string(g_window->label()).empty())
    {
        std::cout << "MISMATCH: window title is empty\n";
        ok = false;
    }

    g_viewButtons[0]->do_callback();
    g_clearButton->do_callback();

    // Charts advance: 10 ticks x 18 script calls, every source grows.
    std::cout << "--- advance 10 ticks ---\n";
    const long long tick0 = g_tick;
    const int calls0 = g_callCount;
    const int base = static_cast<int>(g_samples[0].size());
    for (int i = 0; i < 10; ++i)
    {
        if (!doTick())
        {
            ok = false;
            break;
        }
    }
    ok &= expectCounts("advance", tick0 + 10, base + 10);
    if (g_callCount != calls0 + 10 * kCallsPerTick)
    {
        std::cout << "MISMATCH: 10 ticks used " << (g_callCount - calls0) << " calls, want " << 10 * kCallsPerTick
                  << '\n';
        ok = false;
    }
    else
    {
        std::cout << "calls: 10 ticks x 18 wrappers x 3 charts = " << (g_callCount - calls0) << '\n';
    }
    ok &= expectMapping("view A", 0, 1, 2);

    // Source switches drive views.ks through the REAL buttons.
    std::cout << "--- source switches ---\n";
    g_viewButtons[1]->do_callback();
    if (!doTick())
    {
        ok = false;
    }
    ok &= expectMapping("view B", 2, 3, 0);
    g_viewButtons[2]->do_callback();
    if (!doTick())
    {
        ok = false;
    }
    ok &= expectMapping("view C", 3, 0, 1);
    g_viewButtons[0]->do_callback();
    if (!doTick())
    {
        ok = false;
    }
    ok &= expectMapping("view A again", 0, 1, 2);

    // Pause freezes ticks and leaves scripts untouched.
    std::cout << "--- pause/resume ---\n";
    const long long tickFrozen = g_tick;
    const int callsFrozen = g_callCount;
    g_pauseButton->do_callback();
    tick_cb(nullptr);
    tick_cb(nullptr);
    tick_cb(nullptr);
    if (g_tick != tickFrozen || g_callCount != callsFrozen)
    {
        std::cout << "MISMATCH: paused ticks moved (" << tickFrozen << "->" << g_tick << ", calls " << callsFrozen
                  << "->" << g_callCount << ")\n";
        ok = false;
    }
    else
    {
        std::cout << "paused: 3 timer ticks, tick " << g_tick << ", calls " << g_callCount << " frozen\n";
    }
    g_pauseButton->do_callback();
    tick_cb(nullptr);
    if (g_tick != tickFrozen + 1 || g_callCount != callsFrozen + kCallsPerTick)
    {
        std::cout << "MISMATCH: resume did not advance one tick\n";
        ok = false;
    }
    else
    {
        std::cout << "resumed: one timer tick, +" << kCallsPerTick << " calls\n";
    }

    // Alert triggering: spike chart 1 (limit 12.00) deterministically.
    // Hysteresis means the 8-tick 100.0 plateau fires exactly once.
    std::cout << "--- spike alert + hysteresis ---\n";
    g_clearButton->do_callback();
    g_spikeButton->do_callback();
    const long long fireTick = g_tick + 1;
    for (int i = 0; i < kSpikeTicks; ++i)
    {
        if (!doTick())
        {
            ok = false;
            break;
        }
    }
    char want[96];
    std::snprintf(want, sizeof(want), "[t%lld] CHART 1 HIGH over 12.00", fireTick);
    if (spikeAlertLines() != 1)
    {
        std::cout << "MISMATCH: spike raised " << spikeAlertLines() << " CHART 1 alerts, want 1\n";
        ok = false;
    }
    else if (g_alerts->size() != 1 || std::string(g_alerts->text(1)) != want)
    {
        std::cout << "MISMATCH: alert text [" << (g_alerts->size() >= 1 ? g_alerts->text(1) : "(none)") << "] want ["
                  << want << "]\n";
        ok = false;
    }
    else
    {
        std::cout << "spike: exact alert [" << want << "], plateau silent (hysteresis)\n";
    }
    // Cooldown re-arms silently; a second spike fires exactly once more.
    for (int i = 0; i < 10; ++i)
    {
        if (!doTick())
        {
            ok = false;
            break;
        }
    }
    if (spikeAlertLines() != 1 || g_alerts->size() != 1)
    {
        std::cout << "MISMATCH: cooldown added alerts (size " << g_alerts->size() << ")\n";
        ok = false;
    }
    else
    {
        std::cout << "cooldown: 10 ticks, no new alerts, re-armed\n";
    }
    g_spikeButton->do_callback();
    const long long fireTick2 = g_tick + 1;
    for (int i = 0; i < kSpikeTicks; ++i)
    {
        if (!doTick())
        {
            ok = false;
            break;
        }
    }
    char want2[96];
    std::snprintf(want2, sizeof(want2), "[t%lld] CHART 1 HIGH over 12.00", fireTick2);
    if (spikeAlertLines() != 2 || g_alerts->size() != 2 || std::string(g_alerts->text(2)) != want2)
    {
        std::cout << "MISMATCH: second spike gave " << spikeAlertLines() << "/2 alerts"
                  << (g_alerts->size() == 2 ? "" : " (browser size changed)") << '\n';
        ok = false;
    }
    else
    {
        std::cout << "second spike: exact alert [" << want2 << "]\n";
    }

    // Reload policy live: recompile keeps buffers, ticks and mapping.
    std::cout << "--- reload ---\n";
    const int loadsBefore = g_loadCount;
    const long long tickBefore = g_tick;
    const int bufBefore = static_cast<int>(g_samples[0].size());
    g_reloadButton->do_callback();
    if (g_loadCount != loadsBefore + 1)
    {
        std::cout << "MISMATCH: reload did not recompile\n";
        ok = false;
    }
    if (g_tick != tickBefore || static_cast<int>(g_samples[0].size()) != bufBefore)
    {
        std::cout << "MISMATCH: reload dropped host state\n";
        ok = false;
    }
    if (!doTick())
    {
        ok = false;
    }
    ok &= expectMapping("after reload", 0, 1, 2);
    std::cout << "reload: load #" << g_loadCount << ", state kept, policy live\n";

    std::cout << "scale: 3 dash files + 1 root | " << g_tick << " ticks x " << kCallsPerTick
              << " calls = " << g_callCount << " vm calls | 3 charts x 4 sources | loads " << g_loadCount
              << " | compile " << g_compileMs << " ms\n";
    std::cout << (ok ? "VERIFY PASS\n" : "VERIFY FAIL\n");
    g_window->hide();
    return ok ? 0 : 1;
}

} // namespace

int main(int argc, char **argv)
{
    if (argc > 0 && argv[0] != nullptr)
    {
        g_argv0 = argv[0];
    }
    bool verify = false;
    for (int i = 1; i < argc; ++i)
    {
        if (std::string(argv[i]) == "--verify")
        {
            verify = true;
        }
    }

    try
    {
        g_startTime = std::chrono::steady_clock::now();
        std::unique_ptr<kern::IVirtualMachine> vm(kern::createVirtualMachine());
        vm->addFunction("src_count", kern::createExternFunction(&src_count_value));
        vm->addFunction("src_val", kern::createExternFunction(&src_val_value));
        vm->addFunction("view_mode", kern::createExternFunction(&view_mode_value));
        vm->addFunction("is_armed", kern::createExternFunction(&is_armed_value));
        vm->addFunction("set_armed", kern::createExternFunction(&set_armed_value));
        vm->addFunction("push_stat", kern::createExternFunction(&push_stat_value));
        vm->addFunction("push_src", kern::createExternFunction(&push_src_value));
        vm->addFunction("push_alert", kern::createExternFunction(&push_alert_value));
        vm->addFunction("poll_chart", kern::createExternFunction(&poll_chart_value));
        g_vm = vm.get();
        int candidate = -1;
        if (!resolveDashDir(g_dashDir, candidate))
        {
            return 1;
        }
        g_searchPaths.push_back(g_dashDir);
        std::cout << "dash dir: " << g_dashDir << " (candidate #" << candidate << ")\n";
        g_feedPath = (std::filesystem::path(g_dashDir) / "dash_feed.txt").string();
        if (!ensureFeed() || !loadFeed())
        {
            return 1;
        }
        if (!loadDash())
        {
            return 1;
        }

        Fl_Window window(680, 660, kWindowTitle);
        g_window = &window;
        StripChart strip0(20, 10, 640, 100, 0);
        StripChart strip1(20, 118, 640, 100, 1);
        StripChart strip2(20, 226, 640, 100, 2);
        g_strips[0] = &strip0;
        g_strips[1] = &strip1;
        g_strips[2] = &strip2;
        Fl_Box status(20, 334, 640, 30, "");
        g_status = &status;
        Fl_Browser alerts(20, 370, 640, 130);
        g_alerts = &alerts;
        Fl_Button viewAButton(20, 508, 90, 30, "View A [*]");
        Fl_Button viewBButton(115, 508, 90, 30, "View B");
        Fl_Button viewCButton(210, 508, 90, 30, "View C");
        viewAButton.callback(onView, reinterpret_cast<void *>(static_cast<std::intptr_t>(0)));
        viewBButton.callback(onView, reinterpret_cast<void *>(static_cast<std::intptr_t>(1)));
        viewCButton.callback(onView, reinterpret_cast<void *>(static_cast<std::intptr_t>(2)));
        g_viewButtons[0] = &viewAButton;
        g_viewButtons[1] = &viewBButton;
        g_viewButtons[2] = &viewCButton;
        Fl_Button pauseButton(305, 508, 110, 30, "Pause");
        pauseButton.callback(onPause);
        g_pauseButton = &pauseButton;
        Fl_Button reloadButton(420, 508, 90, 30, "Reload");
        reloadButton.callback(onReload);
        g_reloadButton = &reloadButton;
        Fl_Button spikeButton(515, 508, 75, 30, "Spike");
        spikeButton.callback(onSpike);
        g_spikeButton = &spikeButton;
        Fl_Button clearButton(595, 508, 65, 30, "Clear");
        clearButton.callback(onClear);
        g_clearButton = &clearButton;
        Fl_Box hint(20, 544, 640, 100, "");
        hint.copy_label("Sources: clock wall seconds, sine 5*sin+noise, walk counter+drift, file tailed "
                        "feed. Scripts own the math: stats windows, alerts thresholds, views mapping. "
                        "Spike forces sine to 100 for 8 ticks. Timer 5 Hz; pause freezes ticks.");
        hint.align(FL_ALIGN_LEFT | FL_ALIGN_TOP | FL_ALIGN_WRAP);
        window.end();

        for (int i = 0; i < 5; ++i)
        {
            if (!doTick())
            {
                return 1;
            }
        }
        refreshButtons();
        refreshStatus();

        if (verify)
        {
            window.show();
            const int code = runVerify();
            return code;
        }
        window.show(argc, argv);
        Fl::add_timeout(kTickSeconds, tick_cb);
        Fl::run();
    }
    catch (const kern::RuntimeError &error)
    {
        std::cerr << "runtime error: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
