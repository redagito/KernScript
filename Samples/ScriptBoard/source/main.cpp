#include <algorithm>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Browser.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Window.H>

#include <kern/Kern.h>

namespace
{

constexpr const char *kWindowTitle = "KernScript Board";

// Urgent-first policy: s = priority * 10 - minutes / 10, bugs get +100.
// Hand-computed scores (integer division on minutes / 10):
//   Null deref  bug p5 40min:    50 - 4 + 100 = 146
//   Crash on save bug p5 120min:  50 - 12 + 100 = 138
//   Memory leak bug p4 90min:     40 - 9 + 100 = 131
//   Flaky test bug p3 30min:      30 - 3 + 100 = 127
//   Slow query bug p2 20min:      20 - 2 + 100 = 118
//   Onboarding feature p5 25min:  50 - 2 = 48
//   Export CSV feature p4 50min:  40 - 5 = 35
//   Login button feature p3 60min: 30 - 6 = 24
//   Refactor net chore p2 45min:  20 - 4 = 16
//   Docs typo chore p1 15min:     10 - 1 = 9
//   Lint cleanup chore p1 10min:  10 - 1 = 9 (tie, stable order keeps Docs first)
//   Dark mode feature p2 180min:  20 - 18 = 2
constexpr const char *kUrgentSource = R"(extern priority();
extern kind();
extern minutes();
extern emit(score);
func score() {
  var p = priority();
  var k = kind();
  var m = minutes();
  var s = p * 10 - m / 10;
  if (k == "bug") {
    s = s + 100;
  } else {
    s = s + 0;
  }
  emit(s);
}
)";

// Bugs-only policy: bugs score 1000 + priority * 10 - minutes / 10, rest score 0.
// Hand-computed scores:
//   Null deref 1046, Crash on save 1038, Memory leak 1031, Flaky test 1027,
//   Slow query 1018, all others 0 (stable order keeps host order).
constexpr const char *kBugsSource = R"(extern priority();
extern kind();
extern minutes();
extern emit(score);
func score() {
  var p = priority();
  var k = kind();
  var m = minutes();
  var s = 0;
  if (k == "bug") {
    s = 1000 + p * 10 - m / 10;
  } else {
    s = 0;
  }
  emit(s);
}
)";

// Short-jobs policy: s = 1000 - minutes + priority * 2, chores +30, features +10.
// Hand-computed scores:
//   Lint cleanup 1022, Docs typo 1017, Onboarding 995, Refactor net 989,
//   Slow query 984, Flaky test 976, Null deref 970, Export CSV 968,
//   Login button 956, Memory leak 918, Crash on save 890, Dark mode 834.
constexpr const char *kShortSource = R"(extern priority();
extern kind();
extern minutes();
extern emit(score);
func score() {
  var p = priority();
  var k = kind();
  var m = minutes();
  var s = 1000 - m + p * 2;
  if (k == "chore") {
    s = s + 30;
  } else {
    if (k == "feature") {
      s = s + 10;
    } else {
      s = s + 0;
    }
  }
  emit(s);
}
)";

struct PolicyPreset
{
    const char *name;
    const char *source;
};

const PolicyPreset kUrgent = {"Urgent first", kUrgentSource};
const PolicyPreset kBugs = {"Bugs only", kBugsSource};
const PolicyPreset kShort = {"Short jobs", kShortSource};

struct Task
{
    const char *title;
    int priority;
    const char *kind;
    int minutes;
};

// Host-owned data: the script never sees this array, only one task per call.
const Task kTasks[] = {
    {"Crash on save", 5, "bug", 120}, {"Login button", 3, "feature", 60}, {"Docs typo", 1, "chore", 15},
    {"Memory leak", 4, "bug", 90},    {"Dark mode", 2, "feature", 180},   {"Flaky test", 3, "bug", 30},
    {"Refactor net", 2, "chore", 45}, {"Export CSV", 4, "feature", 50},   {"Slow query", 2, "bug", 20},
    {"Lint cleanup", 1, "chore", 10}, {"Onboarding", 5, "feature", 25},   {"Null deref", 5, "bug", 40},
};
constexpr int kTaskCount = 12;

const char *kExpectedUrgent[] = {
    "Null deref", "Crash on save", "Memory leak",  "Flaky test", "Slow query",   "Onboarding",
    "Export CSV", "Login button",  "Refactor net", "Docs typo",  "Lint cleanup", "Dark mode",
};
const char *kExpectedBugs[] = {
    "Null deref", "Crash on save", "Memory leak",  "Flaky test", "Slow query",   "Login button",
    "Docs typo",  "Dark mode",     "Refactor net", "Export CSV", "Lint cleanup", "Onboarding",
};
const char *kExpectedShort[] = {
    "Lint cleanup", "Docs typo",  "Onboarding",   "Refactor net", "Slow query",    "Flaky test",
    "Null deref",   "Export CSV", "Login button", "Memory leak",  "Crash on save", "Dark mode",
};

int g_taskIndex = 0;
int g_emitted = 0;
int g_loadCount = 0;
int g_scoreCalls = 0;
std::string g_preset = "Urgent first";
kern::IVirtualMachine *g_vm = nullptr;
std::vector<std::string> g_lines;
std::vector<std::string> g_order;

Fl_Window *g_window = nullptr;
Fl_Browser *g_browser = nullptr;
Fl_Box *g_status = nullptr;
Fl_Button *g_reloadButton = nullptr;
Fl_Button *g_urgentButton = nullptr;
Fl_Button *g_bugsButton = nullptr;
Fl_Button *g_shortButton = nullptr;

int priority_value() { return kTasks[g_taskIndex].priority; }

std::string kind_value() { return std::string(kTasks[g_taskIndex].kind); }

int minutes_value() { return kTasks[g_taskIndex].minutes; }

void emit_score(int score) { g_emitted = score; }

bool callScore()
{
    try
    {
        if (!g_vm->callFunction("score"))
        {
            std::cerr << "function score not found\n";
            return false;
        }
    }
    catch (const kern::RuntimeError &error)
    {
        std::cerr << "runtime error: " << error.what() << '\n';
        return false;
    }
    ++g_scoreCalls;
    return true;
}

bool loadPreset(const PolicyPreset &preset)
{
    std::stringstream asmCode;
    {
        std::istringstream input(preset.source);
        std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
        if (!compiler->compile(input, asmCode))
        {
            std::cerr << "compile failed for " << preset.name << '\n';
            return false;
        }
    }
    std::stringstream byteCode;
    {
        std::istringstream input(asmCode.str());
        std::unique_ptr<kern::IAssembler> assembler(kern::createAssembler());
        if (!assembler->assemble(input, byteCode))
        {
            std::cerr << "assemble failed for " << preset.name << '\n';
            return false;
        }
    }
    try
    {
        if (!g_vm->load(byteCode))
        {
            std::cerr << "load failed for " << preset.name << '\n';
            return false;
        }
    }
    catch (const kern::RuntimeError &error)
    {
        std::cerr << "load error: " << error.what() << '\n';
        return false;
    }
    ++g_loadCount;
    g_preset = preset.name;
    std::cout << "preset " << preset.name << " loaded (load #" << g_loadCount << ")\n";
    return true;
}

const PolicyPreset *findPreset(const std::string &name)
{
    if (name == kUrgent.name)
    {
        return &kUrgent;
    }
    if (name == kBugs.name)
    {
        return &kBugs;
    }
    return &kShort;
}

bool refreshBoard()
{
    std::vector<std::pair<int, int>> scored;
    for (int i = 0; i < kTaskCount; ++i)
    {
        g_taskIndex = i;
        g_emitted = 0;
        if (!callScore())
        {
            return false;
        }
        scored.push_back({g_emitted, i});
    }
    std::stable_sort(scored.begin(), scored.end(),
                     [](const auto &left, const auto &right) { return left.first > right.first; });
    g_lines.clear();
    g_order.clear();
    g_browser->clear();
    for (const auto &entry : scored)
    {
        const Task &task = kTasks[entry.second];
        std::string line = std::to_string(entry.first) + " " + task.title + " [" + task.kind + " p" +
                           std::to_string(task.priority) + " " + std::to_string(task.minutes) + "min]";
        g_lines.push_back(line);
        g_order.push_back(task.title);
        g_browser->add(line.c_str());
    }
    const std::string text = g_preset + std::string(" | load #") + std::to_string(g_loadCount);
    g_status->copy_label(text.c_str());
    return true;
}

void dumpBoard(std::ostream &out)
{
    out << "preset " << g_preset << ":\n";
    for (const std::string &line : g_lines)
    {
        out << "  " << line << '\n';
    }
}

bool checkOrder(const char *label, const char *const *expected)
{
    bool ok = true;
    for (int i = 0; i < kTaskCount; ++i)
    {
        if (g_order[i] != expected[i])
        {
            std::cout << "MISMATCH " << label << " row " << i << ": got '" << g_order[i] << "' want '" << expected[i]
                      << "'\n";
            ok = false;
        }
    }
    if (ok)
    {
        std::cout << label << ": order match (" << kTaskCount << " rows)\n";
    }
    return ok;
}

void onPreset(Fl_Widget * /*widget*/, void *data)
{
    const PolicyPreset *preset = static_cast<const PolicyPreset *>(data);
    if (!loadPreset(*preset))
    {
        return;
    }
    if (!refreshBoard())
    {
        return;
    }
    std::cout << "switched to " << preset->name << '\n';
}

void onReload(Fl_Widget * /*widget*/, void * /*data*/)
{
    const PolicyPreset *preset = findPreset(g_preset);
    if (!loadPreset(*preset))
    {
        return;
    }
    if (!refreshBoard())
    {
        return;
    }
    std::cout << "reloaded " << preset->name << " live\n";
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

    dumpBoard(std::cout);
    ok &= checkOrder("startup urgent", kExpectedUrgent);

    g_bugsButton->do_callback();
    dumpBoard(std::cout);
    ok &= checkOrder("bugs only", kExpectedBugs);

    g_shortButton->do_callback();
    dumpBoard(std::cout);
    ok &= checkOrder("short jobs", kExpectedShort);

    // Reload switch: going back to Urgent first must re-sort the list live.
    g_urgentButton->do_callback();
    dumpBoard(std::cout);
    ok &= checkOrder("reload switch urgent", kExpectedUrgent);

    // Reload button recompiles the current policy without restarting.
    const int loadsBefore = g_loadCount;
    const std::vector<std::string> orderBefore = g_order;
    g_reloadButton->do_callback();
    dumpBoard(std::cout);
    if (g_loadCount != loadsBefore + 1)
    {
        std::cout << "MISMATCH: reload did not recompile (loads " << g_loadCount << " want " << (loadsBefore + 1)
                  << ")\n";
        ok = false;
    }
    if (g_order != orderBefore)
    {
        std::cout << "MISMATCH: reload changed the order\n";
        ok = false;
    }
    else
    {
        std::cout << "reload: recompiled, order unchanged (" << g_loadCount << " loads)\n";
    }
    ok &= checkOrder("after reload urgent", kExpectedUrgent);

    std::cout << "vm loads: " << g_loadCount << " score calls: " << g_scoreCalls << '\n';
    if (g_loadCount != 5)
    {
        std::cout << "MISMATCH: expected 5 vm loads\n";
        ok = false;
    }
    if (g_scoreCalls != 60)
    {
        std::cout << "MISMATCH: expected 60 score calls\n";
        ok = false;
    }
    std::cout << (ok ? "VERIFY PASS\n" : "VERIFY FAIL\n");
    g_window->hide();
    return ok ? 0 : 1;
}

} // namespace

int main(int argc, char **argv)
{
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
        std::unique_ptr<kern::IVirtualMachine> vm(kern::createVirtualMachine());
        vm->addFunction("priority", kern::createExternFunction(&priority_value));
        vm->addFunction("kind", kern::createExternFunction(&kind_value));
        vm->addFunction("minutes", kern::createExternFunction(&minutes_value));
        vm->addFunction("emit", kern::createExternFunction(&emit_score));
        g_vm = vm.get();
        if (!loadPreset(kUrgent))
        {
            return 1;
        }

        Fl_Window window(520, 430, kWindowTitle);
        g_window = &window;
        Fl_Browser browser(20, 20, 480, 280);
        g_browser = &browser;
        Fl_Button urgentButton(20, 310, 110, 30, "Urgent first");
        urgentButton.callback(onPreset, const_cast<PolicyPreset *>(&kUrgent));
        g_urgentButton = &urgentButton;
        Fl_Button bugsButton(140, 310, 110, 30, "Bugs only");
        bugsButton.callback(onPreset, const_cast<PolicyPreset *>(&kBugs));
        g_bugsButton = &bugsButton;
        Fl_Button shortButton(260, 310, 110, 30, "Short jobs");
        shortButton.callback(onPreset, const_cast<PolicyPreset *>(&kShort));
        g_shortButton = &shortButton;
        Fl_Button reloadButton(380, 310, 120, 30, "Reload");
        reloadButton.callback(onReload);
        g_reloadButton = &reloadButton;
        Fl_Box status(20, 350, 480, 24, "Urgent first | load #1");
        g_status = &status;
        window.end();
        if (!refreshBoard())
        {
            return 1;
        }

        if (verify)
        {
            window.show();
            const int code = runVerify();
            return code;
        }
        window.show(argc, argv);
        Fl::run();
    }
    catch (const kern::RuntimeError &error)
    {
        std::cerr << "runtime error: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
