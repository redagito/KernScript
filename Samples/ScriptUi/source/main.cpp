#include <cctype>
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

// Handles are 1-based creation-order ids: 1 label, 2 field, 3 field,
// 4 Close, 5 Transform, 6 Clear. The script hardcodes 2/3 in handlers
// because the language has no globals to share build() locals.
constexpr int kFirstField = 2;
constexpr int kSecondField = 3;
constexpr int kCloseButton = 4;
constexpr int kTransformButton = 5;
constexpr int kClearButton = 6;

enum class Kind
{
    Label,
    Field,
    Button
};

struct WidgetEntry
{
    Kind kind;
    Fl_Widget *widget;
};

std::string g_argv0;
kern::IVirtualMachine *g_vm = nullptr;
Fl_Window *g_window = nullptr;
std::vector<WidgetEntry> g_widgets;
std::unordered_map<int, std::string> g_handlers;
int g_nextY = kMargin;
bool g_quitRequested = false;

bool validHandle(int id) { return id >= 1 && id <= static_cast<int>(g_widgets.size()); }

int nextRowY()
{
    const int y = g_nextY;
    g_nextY += kRowH + kGap;
    return y;
}

// Bare shell: every factory appends one full-width row in creation order.
// No pixel-pushing in script; this vertical stack is the minimal host side.
int create_label_value(std::string text)
{
    if (g_window == nullptr)
    {
        throw kern::RuntimeError("create_label before window exists");
    }
    const int w = g_window->w() - kMargin * 2;
    auto *box = new Fl_Box(kMargin, nextRowY(), w, kRowH);
    box->copy_label(text.c_str());
    box->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    g_window->add(box);
    g_window->redraw();
    g_widgets.push_back({Kind::Label, box});
    return static_cast<int>(g_widgets.size());
}

int create_field_value()
{
    if (g_window == nullptr)
    {
        throw kern::RuntimeError("create_field before window exists");
    }
    const int w = g_window->w() - kMargin * 2;
    auto *input = new Fl_Input(kMargin, nextRowY(), w, kRowH, "");
    input->value("");
    g_window->add(input);
    g_window->redraw();
    g_widgets.push_back({Kind::Field, input});
    return static_cast<int>(g_widgets.size());
}

void onButton(Fl_Widget * /*widget*/, void *data);

int create_button_value(std::string caption)
{
    if (g_window == nullptr)
    {
        throw kern::RuntimeError("create_button before window exists");
    }
    const int w = g_window->w() - kMargin * 2;
    const int id = static_cast<int>(g_widgets.size()) + 1;
    auto *button = new Fl_Button(kMargin, nextRowY(), w, kRowH);
    button->copy_label(caption.c_str());
    button->callback(onButton, reinterpret_cast<void *>(static_cast<intptr_t>(id)));
    g_window->add(button);
    g_window->redraw();
    g_widgets.push_back({Kind::Button, button});
    return id;
}

void on_click_attach(int button, std::string handler)
{
    if (!validHandle(button) || g_widgets[button - 1].kind != Kind::Button)
    {
        throw kern::RuntimeError("on_click needs a button handle");
    }
    if (handler.empty())
    {
        throw kern::RuntimeError("on_click needs a handler name");
    }
    g_handlers[button] = handler;
}

std::string get_text_value(int field)
{
    if (!validHandle(field) || g_widgets[field - 1].kind != Kind::Field)
    {
        throw kern::RuntimeError("get_text needs a field handle");
    }
    auto *input = static_cast<Fl_Input *>(g_widgets[field - 1].widget);
    const char *value = input->value();
    return value != nullptr ? value : "";
}

void set_text_value(int field, std::string text)
{
    if (!validHandle(field) || g_widgets[field - 1].kind != Kind::Field)
    {
        throw kern::RuntimeError("set_text needs a field handle");
    }
    auto *input = static_cast<Fl_Input *>(g_widgets[field - 1].widget);
    input->value(text.c_str());
}

std::string upper_value(std::string text)
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

void quit_request()
{
    g_quitRequested = true;
    if (g_window != nullptr)
    {
        g_window->hide();
    }
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
    const int id = static_cast<int>(reinterpret_cast<intptr_t>(data));
    auto found = g_handlers.find(id);
    if (found == g_handlers.end())
    {
        std::cerr << "button " << id << " has no handler\n";
        return;
    }
    std::cout << "button " << id << " -> " << found->second << '\n';
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

Fl_Input *fieldBox(int id)
{
    if (!validHandle(id) || g_widgets[id - 1].kind != Kind::Field)
    {
        return nullptr;
    }
    return static_cast<Fl_Input *>(g_widgets[id - 1].widget);
}

Fl_Button *pushButton(int id)
{
    if (!validHandle(id) || g_widgets[id - 1].kind != Kind::Button)
    {
        return nullptr;
    }
    return static_cast<Fl_Button *>(g_widgets[id - 1].widget);
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

    Fl_Input *first = fieldBox(kFirstField);
    Fl_Input *second = fieldBox(kSecondField);
    Fl_Button *closeBtn = pushButton(kCloseButton);
    Fl_Button *transBtn = pushButton(kTransformButton);
    Fl_Button *clearBtn = pushButton(kClearButton);
    if (first == nullptr || second == nullptr || closeBtn == nullptr || transBtn == nullptr || clearBtn == nullptr)
    {
        std::cout << "MISMATCH: creation-order handles do not map to widgets\n";
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
    std::cout << "preset f1 to 'hello World 123'\n";
    transBtn->do_callback();
    const std::string afterTransform = second->value() != nullptr ? second->value() : "";
    std::cout << "after Transform: f1='" << first->value() << "' f2='" << afterTransform << "'\n";
    if (afterTransform != "HELLO WORLD 123")
    {
        std::cout << "MISMATCH transform: got '" << afterTransform << "' want 'HELLO WORLD 123'\n";
        ok = false;
    }
    else
    {
        std::cout << "transform: f2 == upper(f1)\n";
    }

    clearBtn->do_callback();
    const std::string afterClear1 = first->value() != nullptr ? first->value() : "";
    const std::string afterClear2 = second->value() != nullptr ? second->value() : "";
    std::cout << "after Clear: f1='" << afterClear1 << "' f2='" << afterClear2 << "'\n";
    if (!afterClear1.empty() || !afterClear2.empty())
    {
        std::cout << "MISMATCH clear: fields are not both empty\n";
        ok = false;
    }
    else
    {
        std::cout << "clear: both fields empty\n";
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
        vm->addFunction("create_label", kern::createExternFunction(&create_label_value));
        vm->addFunction("create_field", kern::createExternFunction(&create_field_value));
        vm->addFunction("create_button", kern::createExternFunction(&create_button_value));
        vm->addFunction("on_click", kern::createExternFunction(&on_click_attach));
        vm->addFunction("get_text", kern::createExternFunction(&get_text_value));
        vm->addFunction("set_text", kern::createExternFunction(&set_text_value));
        vm->addFunction("upper", kern::createExternFunction(&upper_value));
        vm->addFunction("quit", kern::createExternFunction(&quit_request));
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
