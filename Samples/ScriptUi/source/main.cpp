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

namespace
{

constexpr const char *kWindowTitle = "KernScript Ui";
constexpr const char *kScriptFile = "ui.ks";
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

const WidgetEntry *findEntry(const std::string &name)
{
    auto found = g_registry.find(name);
    if (found == g_registry.end() || found->second >= g_widgets.size())
    {
        return nullptr;
    }
    return &g_widgets[found->second];
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

// Statement-use externs (create_*, on_click, set_text, quit): a false host
// outcome becomes a false extern call, which the VM turns into RuntimeError.
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
        return;
    }
    const std::string &name = g_widgets[index].name;
    auto found = g_handlers.find(name);
    if (found == g_handlers.end())
    {
        std::cerr << "button " << name << " has no handler\n";
        return;
    }
    std::cout << "button " << name << " -> " << found->second << '\n';
    callByName(found->second);
}

// Scripts live as separate files under scripts/. Resolve robustly so
// --verify works from the build tree: exe-dir/scripts wins, then
// CWD/scripts, then CWD/Samples/ScriptUi/scripts (repo-root runs).
// The winning candidate is reported; a miss is a loud error, never silent.
bool resolveScriptPath(const char *file, std::string &out, int &candidate)
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

bool loadScript()
{
    std::string path;
    int candidate = -1;
    if (!resolveScriptPath(kScriptFile, path, candidate))
    {
        return false;
    }
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        std::cerr << "cannot open script file: " << path << '\n';
        return false;
    }
    std::stringstream source;
    source << file.rdbuf();

    std::stringstream asmCode;
    {
        std::istringstream input(source.str());
        std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
        if (!compiler->compile(input, asmCode, kScriptFile))
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

Fl_Input *fieldBox(const std::string &name)
{
    auto found = g_registry.find(name);
    if (found == g_registry.end() || found->second >= g_widgets.size())
    {
        return nullptr;
    }
    if (g_widgets[found->second].kind != Kind::Field)
    {
        return nullptr;
    }
    return static_cast<Fl_Input *>(g_widgets[found->second].widget);
}

Fl_Button *pushButton(const std::string &name)
{
    auto found = g_registry.find(name);
    if (found == g_registry.end() || found->second >= g_widgets.size())
    {
        return nullptr;
    }
    if (g_widgets[found->second].kind != Kind::Button)
    {
        return nullptr;
    }
    return static_cast<Fl_Button *>(g_widgets[found->second].widget);
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
    if (g_widgets.size() != 6 || labels != 1 || fields != 2 || buttons != 3)
    {
        std::cout << "MISMATCH: want 1 label, 2 fields, 3 buttons in creation order\n";
        ok = false;
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

    const WidgetEntry *title = findEntry("title");
    const WidgetEntry *input = findEntry("input");
    const WidgetEntry *output = findEntry("output");
    const WidgetEntry *close = findEntry("close");
    const WidgetEntry *transform = findEntry("transform");
    const WidgetEntry *clear = findEntry("clear");
    if (title == nullptr || title->kind != Kind::Label || input == nullptr || input->kind != Kind::Field ||
        output == nullptr || output->kind != Kind::Field || close == nullptr || close->kind != Kind::Button ||
        transform == nullptr || transform->kind != Kind::Button || clear == nullptr || clear->kind != Kind::Button)
    {
        std::cout << "MISMATCH: named registry misses title/input/output/close/transform/clear\n";
        ok = false;
    }
    else
    {
        std::cout << "registry: title/input/output/close/transform/clear all named\n";
    }

    Fl_Input *first = fieldBox("input");
    Fl_Input *second = fieldBox("output");
    Fl_Button *closeBtn = pushButton("close");
    Fl_Button *transBtn = pushButton("transform");
    Fl_Button *clearBtn = pushButton("clear");
    if (first == nullptr || second == nullptr || closeBtn == nullptr || transBtn == nullptr || clearBtn == nullptr)
    {
        std::cout << "MISMATCH: named lookup does not map to widgets\n";
        ok = false;
    }
    if (!ok)
    {
        std::cout << "VERIFY FAIL\n";
        g_window->hide();
        return 1;
    }

    // Real button callbacks from here on: preset, press, read back.
    first->value("hello World 123");
    std::cout << "preset input to 'hello World 123'\n";
    transBtn->do_callback();
    const std::string afterTransform = second->value() != nullptr ? second->value() : "";
    std::cout << "after Transform: input='" << first->value() << "' output='" << afterTransform << "'\n";
    if (afterTransform != "HELLO WORLD 123")
    {
        std::cout << "MISMATCH transform: got '" << afterTransform << "' want 'HELLO WORLD 123'\n";
        ok = false;
    }
    else
    {
        std::cout << "transform: output == upper(input)\n";
    }

    clearBtn->do_callback();
    const std::string afterClear1 = first->value() != nullptr ? first->value() : "";
    const std::string afterClear2 = second->value() != nullptr ? second->value() : "";
    std::cout << "after Clear: input='" << afterClear1 << "' output='" << afterClear2 << "'\n";
    if (!afterClear1.empty() || !afterClear2.empty())
    {
        std::cout << "MISMATCH clear: fields are not both empty\n";
        ok = false;
    }
    else
    {
        std::cout << "clear: both fields empty\n";
    }

    // Rejections: duplicate names and unknown names must surface as errors.
    // Each probe runs through a real extern call, so a missing rejection
    // (extern returning true) shows up as a clean run instead of a throw.
    const std::pair<const char *, const char *> rejections[] = {
        {"extern create_label(name, text);", "create_label(\"input\", \"dup\");"},
        {"extern create_field(name);", "create_field(\"input\");"},
        {"extern create_button(name, caption);", "create_button(\"transform\", \"Dup\");"},
        {"extern get_text(name);", "var x = get_text(\"nope\");"},
        {"extern set_text(name, text);", "set_text(\"nope\", \"x\");"},
        {"extern on_click(name, handler);", "on_click(\"nope\", \"on_transform\");"},
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

    // Close/quit path must end the app with code 0.
    closeBtn->do_callback();
    std::cout << "after Close: quit=" << (g_quitRequested ? 1 : 0) << " shown=" << (g_window->shown() != 0 ? 1 : 0)
              << '\n';
    if (!g_quitRequested)
    {
        std::cout << "MISMATCH: Close did not request quit\n";
        ok = false;
    }
    else
    {
        std::cout << "close: quit path exits 0\n";
    }

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
        std::unique_ptr<kern::IVirtualMachine> vm(kern::createVirtualMachine());
        registerExterns(*vm);
        g_vm = vm.get();
        if (!loadScript())
        {
            return 1;
        }

        Fl_Window window(kWinW, kWinH, kWindowTitle);
        g_window = &window;
        window.begin();
        if (!callByName("build"))
        {
            return 1;
        }
        window.end();
        std::cout << "built " << g_widgets.size() << " widgets in creation order\n";

        if (verify)
        {
            window.show();
            const int code = runVerify();
            return code;
        }
        window.show(argc, argv);
        Fl::run();
        return g_quitRequested ? 0 : 0;
    }
    catch (const kern::RuntimeError &error)
    {
        std::cerr << "runtime error: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
