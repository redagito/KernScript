#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Window.H>

#include <kern/Kern.h>

// ScriptUi: script-built UI shell on FLTK. The host is deliberately dumb: it
// owns the window, the named widget registry and the extern protocol, while
// EVERYTHING visible (title, widgets, order, handlers) comes from the script
// it runs. There are no widget names, counts, orders or demo behaviors
// baked in below; the verify harness discovers all of them at runtime.
//
// Lifecycle contract (mirrored in the scripts/*.ks headers):
//   1. The host loads one script file (argv choice, else ui.ks) and calls its
//      nullary build() function FIRST. build() creates the widgets it needs,
//      wires buttons with on_click, and should call set_title (otherwise the
//      neutral default title stays).
//   2. Extern protocol, with arities and value/statement use:
//        create_label(name, text)      statement, arity 2: label under name.
//        create_field(name)            statement, arity 1: text field under name.
//        create_button(name, caption)  statement, arity 2: button under name.
//        on_click(name, handler)       statement, arity 2: wire button to a
//                                      script function BY NAME.
//        get_text(name)                value, arity 1: read a field.
//        set_text(name, text)          statement, arity 2: write a field.
//        upper(text)                   value, arity 1: ASCII uppercase.
//        set_title(text)               statement, arity 1: window title.
//        check_equal(actual, expected) statement, arity 2: mismatch fails.
//        quit()                        statement, arity 0: hide window, exit 0.
//   3. Registry rules: names are unique across ALL kinds; a duplicate create,
//      or get/set/on_click naming an unknown widget, returns false from the
//      extern, which the VM turns into a RuntimeError (loud, never silent).
//      A button whose handler names no script function fails loudly at click
//      time (stderr plus a failed-dispatch flag --verify asserts).
//   4. --verify hook convention (optional, nullary, script-side names only):
//      verify_setup() runs once before the harness presses anything; after
//      each REAL button press the harness calls check_<handler>, where
//      <handler> is the handler name the SCRIPT wired at runtime. A missing
//      hook only logs the field dump; a hook that throws (e.g. through
//      check_equal) fails the run. --verify drives every script in
//      kVerifyScripts through this loop, so a shell that only fits one demo
//      cannot pass.

namespace
{

constexpr const char *kDefaultTitle = "KernScript";
constexpr const char *kDefaultScript = "ui.ks";
constexpr const char *kVerifyScripts[] = {"ui.ks", "notes.ks"};
constexpr const char *kBogusHandler = "__verify_no_such_handler__";
constexpr const char *kAbsentStem = "__verify_missing__";
constexpr int kWinW = 400;
constexpr int kWinH = 340;
constexpr int kMargin = 20;
constexpr int kRowH = 30;
constexpr int kGap = 12;

// Named registry: every widget is known by name everywhere, so script
// functions share names instead of hardcoded integer handles (the language
// has no globals to share build() locals through).
enum class Kind
{
    Label,
    Field,
    Button
};

struct WidgetEntry
{
    Kind kind;
    std::string name;
    Fl_Widget *widget;
};

std::string g_argv0;
kern::IVirtualMachine *g_vm = nullptr;
Fl_Window *g_window = nullptr;
std::vector<WidgetEntry> g_widgets;                      // creation order: the vertical stack
std::unordered_map<std::string, std::size_t> g_registry; // name -> g_widgets index
std::unordered_map<std::string, std::string> g_handlers; // button name -> script func
int g_nextY = kMargin;
bool g_quitRequested = false;
bool g_titleWasSet = false;
bool g_lastDispatchOk = true;

const WidgetEntry *findEntry(const std::string &name)
{
    auto found = g_registry.find(name);
    if (found == g_registry.end() || found->second >= g_widgets.size())
    {
        return nullptr;
    }
    return &g_widgets[found->second];
}

void resetShell()
{
    g_vm = nullptr;
    g_window = nullptr;
    g_widgets.clear();
    g_registry.clear();
    g_handlers.clear();
    g_nextY = kMargin;
    g_quitRequested = false;
    g_titleWasSet = false;
    g_lastDispatchOk = true;
}

int nextRowY()
{
    const int y = g_nextY;
    g_nextY += kRowH + kGap;
    return y;
}

// Bare shell: every factory appends one full-width row in creation order.
// No pixel-pushing in script; this vertical stack is the minimal host side.
// Each returns false on a duplicate name; the extern wrapper turns that
// into a VM RuntimeError (loud failure, never silent).
bool doCreateLabel(const std::string &name, const std::string &text)
{
    if (g_window == nullptr)
    {
        throw kern::RuntimeError("create_label before window exists");
    }
    if (name.empty() || g_registry.count(name) != 0)
    {
        return false;
    }
    const int w = g_window->w() - kMargin * 2;
    auto *box = new Fl_Box(kMargin, nextRowY(), w, kRowH);
    box->copy_label(text.c_str());
    box->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    g_window->add(box);
    g_window->redraw();
    g_registry[name] = g_widgets.size();
    g_widgets.push_back({Kind::Label, name, box});
    return true;
}

bool doCreateField(const std::string &name)
{
    if (g_window == nullptr)
    {
        throw kern::RuntimeError("create_field before window exists");
    }
    if (name.empty() || g_registry.count(name) != 0)
    {
        return false;
    }
    const int w = g_window->w() - kMargin * 2;
    auto *input = new Fl_Input(kMargin, nextRowY(), w, kRowH, "");
    input->value("");
    g_window->add(input);
    g_window->redraw();
    g_registry[name] = g_widgets.size();
    g_widgets.push_back({Kind::Field, name, input});
    return true;
}

void onButton(Fl_Widget * /*widget*/, void *data);

bool doCreateButton(const std::string &name, const std::string &caption)
{
    if (g_window == nullptr)
    {
        throw kern::RuntimeError("create_button before window exists");
    }
    if (name.empty() || g_registry.count(name) != 0)
    {
        return false;
    }
    const int w = g_window->w() - kMargin * 2;
    const std::size_t index = g_widgets.size();
    auto *button = new Fl_Button(kMargin, nextRowY(), w, kRowH);
    button->copy_label(caption.c_str());
    button->callback(onButton, reinterpret_cast<void *>(static_cast<uintptr_t>(index)));
    g_window->add(button);
    g_window->redraw();
    g_registry[name] = index;
    g_widgets.push_back({Kind::Button, name, button});
    return true;
}

bool doOnClick(const std::string &name, const std::string &handler)
{
    const WidgetEntry *entry = findEntry(name);
    if (entry == nullptr || entry->kind != Kind::Button)
    {
        return false;
    }
    if (handler.empty())
    {
        return false;
    }
    g_handlers[name] = handler;
    return true;
}

bool doGetText(const std::string &name, std::string &out)
{
    const WidgetEntry *entry = findEntry(name);
    if (entry == nullptr || entry->kind != Kind::Field)
    {
        return false;
    }
    auto *input = static_cast<Fl_Input *>(entry->widget);
    const char *value = input->value();
    out = value != nullptr ? value : "";
    return true;
}

bool doSetText(const std::string &name, const std::string &text)
{
    const WidgetEntry *entry = findEntry(name);
    if (entry == nullptr || entry->kind != Kind::Field)
    {
        return false;
    }
    auto *input = static_cast<Fl_Input *>(entry->widget);
    input->value(text.c_str());
    return true;
}

// Script-owned window title. An empty title is rejected (loud, like every
// other statement-use extern validation); when the script never calls this,
// the neutral default title stays.
bool doSetTitle(const std::string &text)
{
    if (g_window == nullptr)
    {
        throw kern::RuntimeError("set_title before window exists");
    }
    if (text.empty())
    {
        return false;
    }
    g_window->copy_label(text.c_str());
    g_window->redraw();
    g_titleWasSet = true;
    return true;
}

bool doCheckEqual(const std::string &actual, const std::string &expected) { return actual == expected; }

std::string upperText(std::string text)
{
    for (char &c : text)
    {
        if (c >= 'a' && c <= 'z')
        {
            c = static_cast<char>(c - ('a' - 'A'));
        }
    }
    return text;
}

void quitRequest()
{
    g_quitRequested = true;
    if (g_window != nullptr)
    {
        g_window->hide();
    }
}

// Statement-use externs (create_*, on_click, set_text, set_title,
// check_equal, quit): a false host outcome becomes a false extern call,
// which the VM turns into RuntimeError.
class CreateLabelExtern : public kern::IExternFunction
{
public:
    unsigned int getArgumentCount() const override { return 2; }
    bool call(kern::IVirtualMachine &vm) override
    {
        std::string text;
        if (!vm.popParameter(text))
        {
            return false;
        }
        std::string name;
        if (!vm.popParameter(name))
        {
            return false;
        }
        if (!doCreateLabel(name, text))
        {
            return false;
        }
        vm.setReturnValue();
        return true;
    }
};

class CreateFieldExtern : public kern::IExternFunction
{
public:
    unsigned int getArgumentCount() const override { return 1; }
    bool call(kern::IVirtualMachine &vm) override
    {
        std::string name;
        if (!vm.popParameter(name))
        {
            return false;
        }
        if (!doCreateField(name))
        {
            return false;
        }
        vm.setReturnValue();
        return true;
    }
};

class CreateButtonExtern : public kern::IExternFunction
{
public:
    unsigned int getArgumentCount() const override { return 2; }
    bool call(kern::IVirtualMachine &vm) override
    {
        std::string caption;
        if (!vm.popParameter(caption))
        {
            return false;
        }
        std::string name;
        if (!vm.popParameter(name))
        {
            return false;
        }
        if (!doCreateButton(name, caption))
        {
            return false;
        }
        vm.setReturnValue();
        return true;
    }
};

class OnClickExtern : public kern::IExternFunction
{
public:
    unsigned int getArgumentCount() const override { return 2; }
    bool call(kern::IVirtualMachine &vm) override
    {
        std::string handler;
        if (!vm.popParameter(handler))
        {
            return false;
        }
        std::string name;
        if (!vm.popParameter(name))
        {
            return false;
        }
        if (!doOnClick(name, handler))
        {
            return false;
        }
        vm.setReturnValue();
        return true;
    }
};

class GetTextExtern : public kern::IExternFunction
{
public:
    unsigned int getArgumentCount() const override { return 1; }
    bool call(kern::IVirtualMachine &vm) override
    {
        std::string name;
        if (!vm.popParameter(name))
        {
            return false;
        }
        std::string out;
        if (!doGetText(name, out))
        {
            return false;
        }
        vm.setReturnValue(out);
        return true;
    }
};

class SetTextExtern : public kern::IExternFunction
{
public:
    unsigned int getArgumentCount() const override { return 2; }
    bool call(kern::IVirtualMachine &vm) override
    {
        std::string text;
        if (!vm.popParameter(text))
        {
            return false;
        }
        std::string name;
        if (!vm.popParameter(name))
        {
            return false;
        }
        if (!doSetText(name, text))
        {
            return false;
        }
        vm.setReturnValue();
        return true;
    }
};

class SetTitleExtern : public kern::IExternFunction
{
public:
    unsigned int getArgumentCount() const override { return 1; }
    bool call(kern::IVirtualMachine &vm) override
    {
        std::string text;
        if (!vm.popParameter(text))
        {
            return false;
        }
        if (!doSetTitle(text))
        {
            return false;
        }
        vm.setReturnValue();
        return true;
    }
};

class CheckEqualExtern : public kern::IExternFunction
{
public:
    unsigned int getArgumentCount() const override { return 2; }
    bool call(kern::IVirtualMachine &vm) override
    {
        std::string expected;
        if (!vm.popParameter(expected))
        {
            return false;
        }
        std::string actual;
        if (!vm.popParameter(actual))
        {
            return false;
        }
        if (!doCheckEqual(actual, expected))
        {
            std::cerr << "check_equal mismatch: got '" << actual << "' want '" << expected << "'\n";
            return false;
        }
        vm.setReturnValue();
        return true;
    }
};

class UpperExtern : public kern::IExternFunction
{
public:
    unsigned int getArgumentCount() const override { return 1; }
    bool call(kern::IVirtualMachine &vm) override
    {
        std::string text;
        if (!vm.popParameter(text))
        {
            return false;
        }
        vm.setReturnValue(upperText(text));
        return true;
    }
};

class QuitExtern : public kern::IExternFunction
{
public:
    unsigned int getArgumentCount() const override { return 0; }
    bool call(kern::IVirtualMachine &vm) override
    {
        quitRequest();
        vm.setReturnValue();
        return true;
    }
};

void registerExterns(kern::IVirtualMachine &vm)
{
    vm.addFunction("create_label", new CreateLabelExtern());
    vm.addFunction("create_field", new CreateFieldExtern());
    vm.addFunction("create_button", new CreateButtonExtern());
    vm.addFunction("on_click", new OnClickExtern());
    vm.addFunction("get_text", new GetTextExtern());
    vm.addFunction("set_text", new SetTextExtern());
    vm.addFunction("set_title", new SetTitleExtern());
    vm.addFunction("check_equal", new CheckEqualExtern());
    vm.addFunction("upper", new UpperExtern());
    vm.addFunction("quit", new QuitExtern());
}

bool callByName(const std::string &name)
{
    try
    {
        if (!g_vm->callFunction(name))
        {
            std::cerr << "function " << name << " not found\n";
            return false;
        }
    }
    catch (const kern::RuntimeError &error)
    {
        std::cerr << "runtime error in " << name << ": " << error.what() << '\n';
        return false;
    }
    return true;
}

void onButton(Fl_Widget * /*widget*/, void *data)
{
    const std::size_t index = static_cast<std::size_t>(reinterpret_cast<uintptr_t>(data));
    if (index >= g_widgets.size() || g_widgets[index].kind != Kind::Button)
    {
        std::cerr << "button callback without a known button\n";
        g_lastDispatchOk = false;
        return;
    }
    const std::string &name = g_widgets[index].name;
    auto found = g_handlers.find(name);
    if (found == g_handlers.end())
    {
        std::cerr << "button " << name << " has no handler\n";
        g_lastDispatchOk = false;
        return;
    }
    std::cout << "button " << name << " -> " << found->second << '\n';
    g_lastDispatchOk = callByName(found->second);
}

// Scripts live as separate files under scripts/. Resolve robustly so
// --verify works from the build tree: exe-dir/scripts wins, then
// CWD/scripts, then CWD/Samples/ScriptUi/scripts (repo-root runs).
// The winning candidate is reported; a miss is a loud error, never silent.
bool resolveScriptPath(const std::string &file, std::string &out, int &candidate)
{
    std::vector<std::string> tries;
    try
    {
        const std::filesystem::path exe(g_argv0);
        if (exe.has_parent_path())
        {
            tries.push_back((exe.parent_path() / "scripts" / file).string());
        }
        else
        {
            tries.push_back((std::filesystem::current_path() / "scripts" / file).string());
        }
    }
    catch (const std::filesystem::filesystem_error &)
    {
        tries.push_back((std::filesystem::current_path() / "scripts" / file).string());
    }
    tries.push_back((std::filesystem::current_path() / "scripts" / file).string());
    tries.push_back((std::filesystem::current_path() / "Samples" / "ScriptUi" / "scripts" / file).string());
    for (std::size_t i = 0; i < tries.size(); ++i)
    {
        std::error_code code;
        if (std::filesystem::is_regular_file(tries[i], code))
        {
            out = tries[i];
            candidate = static_cast<int>(i);
            return true;
        }
    }
    std::cerr << "script file missing: " << file << " (tried " << tries.size() << " candidates:";
    for (const std::string &t : tries)
    {
        std::cerr << " [" << t << "]";
    }
    std::cerr << ")\n";
    return false;
}

bool loadScript(const std::string &file)
{
    std::string path;
    int candidate = -1;
    if (!resolveScriptPath(file, path, candidate))
    {
        return false;
    }
    std::ifstream script(path, std::ios::binary);
    if (!script)
    {
        std::cerr << "cannot open script file: " << path << '\n';
        return false;
    }
    std::stringstream source;
    source << script.rdbuf();

    std::stringstream asmCode;
    {
        std::istringstream input(source.str());
        std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
        if (!compiler->compile(input, asmCode, file.c_str()))
        {
            std::cerr << "compile failed for " << path << '\n';
            return false;
        }
    }
    std::stringstream byteCode;
    {
        std::istringstream input(asmCode.str());
        std::unique_ptr<kern::IAssembler> assembler(kern::createAssembler());
        if (!assembler->assemble(input, byteCode))
        {
            std::cerr << "assemble failed for " << path << '\n';
            return false;
        }
    }
    try
    {
        if (!g_vm->load(byteCode))
        {
            std::cerr << "load failed for " << path << '\n';
            return false;
        }
    }
    catch (const kern::RuntimeError &error)
    {
        std::cerr << "load error: " << error.what() << '\n';
        return false;
    }
    std::cout << "script loaded from " << path << " (candidate #" << candidate << ")\n";
    return true;
}

// Grow the window to fit whatever the script built: the shell assumes no
// fixed widget count, so a tall stack gets a taller window instead of
// clipping. A small build keeps the default size.
void fitWindowHeight(Fl_Window &window)
{
    const int needed = g_nextY + kMargin;
    if (needed > window.h())
    {
        window.size(window.w(), needed);
    }
}

Fl_Button *buttonWidget(const std::string &name)
{
    const WidgetEntry *entry = findEntry(name);
    if (entry == nullptr || entry->kind != Kind::Button)
    {
        return nullptr;
    }
    return static_cast<Fl_Button *>(entry->widget);
}

// Button and field names in creation order: the whole verify harness drives
// these runtime-discovered lists, never hardcoded names.
std::vector<std::string> buttonNames()
{
    std::vector<std::string> names;
    for (const WidgetEntry &entry : g_widgets)
    {
        if (entry.kind == Kind::Button)
        {
            names.push_back(entry.name);
        }
    }
    return names;
}

std::vector<std::string> fieldNames()
{
    std::vector<std::string> names;
    for (const WidgetEntry &entry : g_widgets)
    {
        if (entry.kind == Kind::Field)
        {
            names.push_back(entry.name);
        }
    }
    return names;
}

void dumpFields()
{
    for (const std::string &name : fieldNames())
    {
        std::string value;
        doGetText(name, value);
        std::cout << "field " << name << "='" << value << "'\n";
    }
}

// A name guaranteed absent from the live registry, for unknown-name probes.
std::string absentName()
{
    std::string candidate = kAbsentStem;
    while (findEntry(candidate) != nullptr)
    {
        candidate += "_x";
    }
    return candidate;
}

// Quote a runtime-discovered name for embedding in a probe script.
std::string quoteForScript(const std::string &text)
{
    std::string quoted = "\"";
    for (char c : text)
    {
        if (c == '\\' || c == '"')
        {
            quoted += '\\';
        }
        quoted += c;
    }
    quoted += '"';
    return quoted;
}

// Runs a one-func probe script on a fresh VM sharing this registry and
// expects the VM to throw: the probe's extern must return false, which the
// VM turns into RuntimeError. Success (no throw) means the rejection is
// missing. The detail string carries the RuntimeError text or the failure.
bool probeExpectsFailure(const std::string &decls, const std::string &body, std::string &detail)
{
    const std::string source = decls + "func probe() { " + body + " }\n";
    std::stringstream asmCode;
    {
        std::istringstream input(source);
        std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
        if (!compiler->compile(input, asmCode, "<verify-probe>"))
        {
            detail = "compile failed";
            return false;
        }
    }
    std::stringstream byteCode;
    {
        std::istringstream input(asmCode.str());
        std::unique_ptr<kern::IAssembler> assembler(kern::createAssembler());
        if (!assembler->assemble(input, byteCode))
        {
            detail = "assemble failed";
            return false;
        }
    }
    std::unique_ptr<kern::IVirtualMachine> probe(kern::createVirtualMachine());
    registerExterns(*probe);
    try
    {
        if (!probe->load(byteCode))
        {
            detail = "load failed";
            return false;
        }
    }
    catch (const kern::RuntimeError &error)
    {
        detail = error.what();
        return false;
    }
    try
    {
        probe->callFunction("probe");
    }
    catch (const kern::RuntimeError &error)
    {
        detail = error.what();
        return true;
    }
    detail = "probe ran without a RuntimeError";
    return false;
}

enum class HookResult
{
    Missing,
    Ok,
    Failed
};

// Optional script hooks (verify_setup, check_<handler>): absent is fine and
// only logs; a throw fails the run. callFunction's false means "no such
// function", so missing and failing stay distinguishable.
HookResult callOptionalHook(const std::string &name)
{
    try
    {
        if (!g_vm->callFunction(name))
        {
            std::cout << "hook " << name << ": absent (generic drive only)\n";
            return HookResult::Missing;
        }
    }
    catch (const kern::RuntimeError &error)
    {
        std::cout << "hook " << name << " threw: " << error.what() << '\n';
        return HookResult::Failed;
    }
    std::cout << "hook " << name << ": ok\n";
    return HookResult::Ok;
}

// One script-agnostic verify pass: every name below is discovered at
// runtime from the registry the script just built. Per-button behavior
// assertions run through the script's own check_<handler> hooks (driven by
// REAL button presses), so distinct scripts prove distinct behaviors while
// the host stays name-free.
int verifyPass(const std::string &file)
{
    bool ok = true;
    std::cout << "--- verify " << file << " ---\n";
    const char *rawLabel = g_window->label();
    const std::string label = rawLabel != nullptr ? rawLabel : "";
    std::cout << "window title: '" << label << "'\n";
    std::cout << "window shown: " << (g_window->shown() != 0 ? 1 : 0) << '\n';
    if (g_window->shown() == 0)
    {
        std::cout << "MISMATCH: window is not visible\n";
        ok = false;
    }
    if (!g_titleWasSet || label.empty() || label == kDefaultTitle)
    {
        std::cout << "MISMATCH: window title was not set by the script\n";
        ok = false;
    }
    else
    {
        std::cout << "title: script-set\n";
    }

    int labels = 0;
    int fields = 0;
    int buttons = 0;
    for (const WidgetEntry &entry : g_widgets)
    {
        if (entry.kind == Kind::Label)
        {
            ++labels;
        }
        else if (entry.kind == Kind::Field)
        {
            ++fields;
        }
        else
        {
            ++buttons;
        }
    }
    std::cout << "widgets: " << g_widgets.size() << " (" << labels << " label, " << fields << " fields, " << buttons
              << " buttons)\n";
    for (const WidgetEntry &entry : g_widgets)
    {
        const char *kind = entry.kind == Kind::Label ? "label" : (entry.kind == Kind::Field ? "field" : "button");
        std::cout << "  [" << kind << "] " << entry.name << '\n';
    }
    if (g_widgets.empty())
    {
        std::cout << "MISMATCH: script built no widgets\n";
        ok = false;
    }
    if (g_registry.size() != g_widgets.size())
    {
        std::cout << "MISMATCH: registry holds " << g_registry.size() << " names for " << g_widgets.size()
                  << " widgets\n";
        ok = false;
    }
    for (const auto &slot : g_registry)
    {
        if (slot.second >= g_widgets.size() || g_widgets[slot.second].name != slot.first)
        {
            std::cout << "MISMATCH: registry entry '" << slot.first << "' does not map back\n";
            ok = false;
        }
    }
    bool stacked = true;
    for (std::size_t i = 1; i < g_widgets.size(); ++i)
    {
        if (g_widgets[i].widget->y() < g_widgets[i - 1].widget->y())
        {
            stacked = false;
        }
    }
    std::cout << "layout: creation-order vertical stack: " << (stacked ? "yes" : "no") << '\n';
    if (!stacked)
    {
        std::cout << "MISMATCH: widgets are not stacked in creation order\n";
        ok = false;
    }
    if (!ok)
    {
        std::cout << "VERIFY FAIL " << file << '\n';
        return 1;
    }

    // Behavior first, through REAL button callbacks in creation order. The
    // setup hook presets whatever fields THIS script owns; each check hook
    // asserts what THIS script's handler just did.
    if (callOptionalHook("verify_setup") == HookResult::Failed)
    {
        ok = false;
    }
    const std::vector<std::string> pressed = buttonNames();
    if (pressed.empty())
    {
        std::cout << "MISMATCH: script wired no buttons to drive\n";
        ok = false;
    }
    for (const std::string &name : pressed)
    {
        Fl_Button *button = buttonWidget(name);
        auto wired = g_handlers.find(name);
        if (button == nullptr || wired == g_handlers.end())
        {
            std::cout << "MISMATCH: button '" << name << "' has no widget or handler\n";
            ok = false;
            continue;
        }
        button->do_callback();
        if (!g_lastDispatchOk)
        {
            std::cout << "MISMATCH: press '" << name << "' did not dispatch to '" << wired->second << "'\n";
            ok = false;
            continue;
        }
        if (callOptionalHook("check_" + wired->second) == HookResult::Failed)
        {
            ok = false;
        }
        else
        {
            dumpFields();
        }
        if (g_quitRequested)
        {
            std::cout << "quit requested after '" << name << "'\n";
        }
    }

    // Rejections: duplicate names and unknown names must surface as errors.
    // The duplicate target is the first widget WHATEVER it is (the registry
    // is shared across kinds); the unknown target is verified absent first.
    // Each probe runs through a real extern call, so a missing rejection
    // (extern returning true) shows up as a clean run instead of a throw.
    const std::string dupe = quoteForScript(g_widgets.front().name);
    const std::string missing = quoteForScript(absentName());
    const std::pair<const char *, std::string> rejections[] = {
        {"extern create_label(name, text);", "create_label(" + dupe + ", \"dup\");"},
        {"extern create_field(name);", "create_field(" + dupe + ");"},
        {"extern create_button(name, caption);", "create_button(" + dupe + ", \"Dup\");"},
        {"extern get_text(name);", "var x = get_text(" + missing + ");"},
        {"extern set_text(name, text);", "set_text(" + missing + ", \"x\");"},
        {"extern on_click(name, handler);", "on_click(" + missing + ", \"on_press\");"},
    };
    for (const auto &rejection : rejections)
    {
        std::string detail;
        if (!probeExpectsFailure(rejection.first, rejection.second, detail))
        {
            std::cout << "MISMATCH rejection [" << rejection.second << "]: " << detail << '\n';
            ok = false;
        }
        else
        {
            std::cout << "reject ok [" << rejection.second << "]: " << detail << '\n';
        }
    }

    // Unknown handler names dispatched at click time must fail loudly: rewire
    // the first-created button (whatever it is) to a handler that does not
    // exist, press the REAL button, and require a failed dispatch. The saved
    // wiring is restored from the runtime copy afterwards.
    const std::string firstButton = pressed.front();
    const std::string savedHandler = g_handlers[firstButton];
    if (!doOnClick(firstButton, kBogusHandler))
    {
        std::cout << "MISMATCH: cannot rewire '" << firstButton << "' for the unknown-handler probe\n";
        ok = false;
    }
    else
    {
        buttonWidget(firstButton)->do_callback();
        if (g_lastDispatchOk)
        {
            std::cout << "MISMATCH: unknown handler '" << kBogusHandler << "' dispatched quietly\n";
            ok = false;
        }
        else
        {
            std::cout << "unknown handler failed loudly on '" << firstButton << "'\n";
        }
        if (!doOnClick(firstButton, savedHandler))
        {
            std::cout << "MISMATCH: cannot restore handler on '" << firstButton << "'\n";
            ok = false;
        }
    }

    // The sweep above must have ended the app through the script's own quit
    // path (whatever button owns it) with code 0.
    if (!g_quitRequested)
    {
        std::cout << "MISMATCH: button sweep never requested quit\n";
        ok = false;
    }
    else
    {
        std::cout << "close: quit path exits 0\n";
    }

    std::cout << (ok ? "VERIFY PASS " : "VERIFY FAIL ") << file << '\n';
    return ok ? 0 : 1;
}

// One isolated pass per script: fresh VM, fresh registry, fresh window, so
// scripts never see each other's widgets. The window is destroyed on return
// (freeing its widgets) before the next pass resets the shell.
int verifyScript(const std::string &file)
{
    resetShell();
    std::unique_ptr<kern::IVirtualMachine> vm(kern::createVirtualMachine());
    registerExterns(*vm);
    g_vm = vm.get();
    if (!loadScript(file))
    {
        return 1;
    }
    Fl_Window window(kWinW, kWinH, kDefaultTitle);
    g_window = &window;
    window.begin();
    if (!callByName("build"))
    {
        g_window = nullptr;
        return 1;
    }
    window.end();
    fitWindowHeight(window);
    std::cout << "built " << g_widgets.size() << " widgets in creation order\n";
    window.show();
    const int code = verifyPass(file);
    window.hide();
    g_window = nullptr;
    g_vm = nullptr;
    return code;
}

} // namespace

int main(int argc, char **argv)
{
    if (argc > 0 && argv[0] != nullptr)
    {
        g_argv0 = argv[0];
    }
    bool verify = false;
    std::string scriptArg;
    for (int i = 1; i < argc; ++i)
    {
        if (std::string(argv[i]) == "--verify")
        {
            verify = true;
        }
        else if (scriptArg.empty())
        {
            scriptArg = argv[i];
        }
    }

    try
    {
        if (verify)
        {
            // Bare --verify proves generality: it drives EVERY sample script,
            // each with a different shape and behavior, through real
            // callbacks. An explicit "ScriptUi --verify other.ks" drives just
            // that one script instead.
            if (!scriptArg.empty())
            {
                std::cout << "verify: explicit script " << scriptArg << '\n';
                return verifyScript(scriptArg);
            }
            int code = 0;
            for (const char *file : kVerifyScripts)
            {
                if (verifyScript(file) != 0)
                {
                    code = 1;
                }
            }
            std::cout << (code == 0 ? "ALL VERIFY PASS\n" : "VERIFY FAIL\n");
            return code;
        }

        // Interactive: run ANY script the shell can load, defaulting to the
        // button/fields demo. Usage: ScriptUi [script.ks]
        resetShell();
        std::unique_ptr<kern::IVirtualMachine> vm(kern::createVirtualMachine());
        registerExterns(*vm);
        g_vm = vm.get();
        const std::string file = scriptArg.empty() ? kDefaultScript : scriptArg;
        if (!loadScript(file))
        {
            return 1;
        }

        Fl_Window window(kWinW, kWinH, kDefaultTitle);
        g_window = &window;
        window.begin();
        if (!callByName("build"))
        {
            return 1;
        }
        window.end();
        fitWindowHeight(window);
        std::cout << "built " << g_widgets.size() << " widgets in creation order\n";
        window.show(argc, argv);
        Fl::run();
        return 0;
    }
    catch (const kern::RuntimeError &error)
    {
        std::cerr << "runtime error: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
