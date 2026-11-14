#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_File_Chooser.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Multiline_Input.H>
#include <FL/Fl_Multiline_Output.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/Fl_Scroll.H>
#include <FL/Fl_Window.H>
#include <kern/Kern.h>

#include "../../Support/Files.h"
#include "../../Support/Preview.h"

namespace fs = std::filesystem;
namespace
{
constexpr std::size_t MaxDoc = 8 * 1024 * 1024;
struct Descriptor
{
    std::string name, label;
    bool multiline = false, output = false;
};
struct Button
{
    std::string name, label, handler;
};
struct Model
{
    std::string tool, script;
    std::vector<Descriptor> fields;
    std::vector<Button> buttons;
    std::map<std::string, std::string> values;
};
struct Logger final : kern::ILogger
{
    std::string messages;

protected:
    void onError(const std::string &s) override { messages += s + '\n'; }
};
struct Runtime
{
    Logger logger;
    std::unique_ptr<kern::IVirtualMachine> vm;
};
struct Binding
{
    Model *model;
    std::map<std::string, std::string> *staged;
    bool building;
};
Binding *active = nullptr;
std::string g_argv0;

struct Scope
{
    Binding *old;
    explicit Scope(Binding &b) : old(active) { active = &b; }
    ~Scope() { active = old; }
};
void validName(const std::string &n)
{
    if (n.empty() || n.size() > 80)
        throw std::runtime_error("Field and button names must contain 1 to 80 characters");
    for (unsigned char c : n)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '.' ||
              c == '-'))
            throw std::runtime_error("Field, button and script IDs use letters, digits, dots, underscores and hyphens");
}
void createField(std::string name, std::string label, std::string multiline)
{
    validName(name);
    auto &m = *active->model;
    if (!active->building || m.values.count(name))
        throw std::runtime_error("Duplicate or late field: " + name);
    for (auto &b : m.buttons)
        if (b.name == name)
            throw std::runtime_error("Duplicate field: " + name);
    if (multiline != "yes" && multiline != "no" && multiline != "output")
        throw std::runtime_error("Field kind must be yes, no or output");
    m.fields.push_back({name, label, multiline != "no", multiline == "output"});
    m.values.emplace(name, "");
}
void createButton(std::string name, std::string label, std::string handler)
{
    validName(name);
    auto &m = *active->model;
    if (!active->building || m.values.count(name))
        throw std::runtime_error("Duplicate or late button: " + name);
    if (m.values.count(name))
        throw std::runtime_error("Duplicate button: " + name);
    for (auto &b : m.buttons)
        if (b.name == name)
            throw std::runtime_error("Duplicate button: " + name);
    m.buttons.push_back({name, label, handler});
}
void setTool(std::string name, std::string script)
{
    validName(script);
    if (!active->building || !active->model->tool.empty() || name.empty() || name.size() > 128)
        throw std::runtime_error("Invalid tool identity");
    active->model->tool = std::move(name);
    active->model->script = std::move(script);
}
std::string getText(std::string name)
{
    auto i = active->staged->find(name);
    if (i == active->staged->end())
        throw std::runtime_error("Unknown field: " + name);
    return i->second;
}
void setText(std::string name, std::string value)
{
    auto i = active->staged->find(name);
    if (i == active->staged->end())
        throw std::runtime_error("Unknown field: " + name);
    i->second = std::move(value);
}
std::string upper(std::string s)
{
    for (char &c : s)
        if (c >= 'a' && c <= 'z')
            c = char(c - 'a' + 'A');
    return s;
}
std::string lower(std::string s)
{
    for (char &c : s)
        if (c >= 'A' && c <= 'Z')
            c = char(c - 'A' + 'a');
    return s;
}
std::string trim(std::string s)
{
    auto ws = [](unsigned char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
    auto a = std::find_if_not(s.begin(), s.end(), ws);
    auto b = std::find_if_not(s.rbegin(), s.rend(), ws).base();
    return a < b ? std::string(a, b) : std::string();
}
std::string concat(std::string a, std::string b) { return a + b; }
std::string normalize(std::string s)
{
    std::string out;
    bool pending = false;
    for (unsigned char c : s)
    {
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
        {
            pending = !out.empty();
            continue;
        }
        if (pending)
            out += ' ';
        pending = false;
        out += static_cast<char>(c);
    }
    return out;
}
std::string replaceText(std::string s, std::string from, std::string to)
{
    if (from.empty())
        throw std::runtime_error("Find text cannot be empty");
    std::size_t p = 0;
    while ((p = s.find(from, p)) != std::string::npos)
    {
        s.replace(p, from.size(), to);
        p += to.size();
    }
    return s;
}
void buildFail() { throw std::runtime_error("A tool document must be created through a supported tool script"); }
std::unique_ptr<Runtime> compile(const fs::path &path)
{
    auto rt = std::make_unique<Runtime>();
    std::istringstream src(samples::readText(path, 1024 * 1024));
    std::stringstream bytecode;
    std::unique_ptr<kern::ICompiler> c(kern::createCompiler(&rt->logger));
    if (!c->compileBytecode(src, bytecode, path.string(), {path.parent_path().string()}))
        throw std::runtime_error("Script compile failed:\n" + rt->logger.messages);
    kern::VMConfig config;
    config.maxExecutionSteps = 100000;
    rt->vm.reset(kern::createVirtualMachine(&rt->logger, config));
    rt->vm->addFunction("tool", &setTool);
    rt->vm->addFunction("field", &createField);
    rt->vm->addFunction("button", &createButton);
    rt->vm->addFunction("get_text", &getText);
    rt->vm->addFunction("set_text", &setText);
    rt->vm->addFunction("upper", &upper);
    rt->vm->addFunction("lower", &lower);
    rt->vm->addFunction("trim", &trim);
    rt->vm->addFunction("concat", &concat);
    rt->vm->addFunction("replace", &replaceText);
    rt->vm->addFunction("normalize", &normalize);
    std::string unused;
    if (!rt->vm->load(bytecode))
        throw std::runtime_error("Script bytecode could not be loaded");
    return rt;
}
Model buildModel(Runtime &rt, const fs::path &path, const std::map<std::string, std::string> &preserve = {})
{
    Model m;
    Binding b{&m, &m.values, true};
    {
        Scope scope(b);
        if (!rt.vm->callFunction("build"))
            throw std::runtime_error("Tool script needs func build() -> void");
    }
    if (m.tool.empty() || m.fields.empty() || m.buttons.empty())
        throw std::runtime_error("Tool script must define an identity, fields and buttons");
    for (auto &d : m.fields)
    {
        auto i = preserve.find(d.name);
        if (i != preserve.end())
            m.values[d.name] = i->second;
    }
    return m;
}
void dispatch(Runtime &rt, Model &m, const std::string &button)
{
    auto i = std::find_if(m.buttons.begin(), m.buttons.end(), [&](auto &b) { return b.name == button; });
    if (i == m.buttons.end())
        throw std::runtime_error("Unknown button: " + button);
    if (i->handler.empty())
        throw std::runtime_error("Button has no handler");
    auto staged = m.values;
    Binding b{&m, &staged, false};
    Scope scope(b);
    if (!rt.vm->callFunction(i->handler))
        throw std::runtime_error("Handler not found: " + i->handler);
    m.values = std::move(staged);
}
std::string encode(const Model &m)
{
    std::ostringstream o;
    o << "TOOLSMITH 1\n" << "tool " << samples::quote(m.tool) << "\nscript " << samples::quote(m.script) << '\n';
    for (auto &[n, v] : m.values)
        o << "field " << samples::quote(n) << ' ' << samples::quote(v) << '\n';
    return o.str();
}
std::map<std::string, std::string> decode(const std::string &data, const Model &m)
{
    auto normalized = data;
    for (std::size_t i = 0; (i = normalized.find("\r\n", i)) != std::string::npos;)
        normalized.erase(i, 1);
    std::istringstream in(normalized);
    std::string line;
    if (!std::getline(in, line) || line != "TOOLSMITH 1")
        throw std::runtime_error("Unsupported Toolsmith document version");
    if (!std::getline(in, line) || !line.starts_with("tool ") || samples::unquote(line.substr(5)) != m.tool)
        throw std::runtime_error("Document belongs to another tool");
    if (!std::getline(in, line) || !line.starts_with("script ") || samples::unquote(line.substr(7)) != m.script)
        throw std::runtime_error("Document belongs to a different script");
    std::map<std::string, std::string> result;
    std::map<std::string, bool> seen;
    for (const auto &f : m.fields)
        result[f.name] = "";
    while (std::getline(in, line))
    {
        if (line.empty())
            continue;
        if (!line.starts_with("field "))
            throw std::runtime_error("Invalid document field");
        auto p = line.find(' ', 6);
        if (p == std::string::npos)
            throw std::runtime_error("Invalid document field");
        auto n = samples::unquote(line.substr(6, p - 6));
        auto v = samples::unquote(line.substr(p + 1));
        auto it = result.find(n);
        if (it == result.end() || seen[n])
            throw std::runtime_error("Unknown or duplicate field: " + n);
        seen[n] = true;
        it->second = std::move(v);
    }
    for (auto &[name, value] : result)
        if (!seen[name])
            throw std::runtime_error("Missing field: " + name);
    return result;
}
struct App
{
    std::unique_ptr<Runtime> runtime;
    Model model;
    fs::path script, document;
    std::string saved;
    bool dirty() const { return encode(model) != saved; }
};
fs::path bundled(const std::string &name)
{
    std::vector<fs::path> tries;
    fs::path exe(g_argv0);
    if (exe.has_parent_path())
        tries.push_back(exe.parent_path() / "scripts" / name);
    tries.push_back(fs::current_path() / "scripts" / name);
    tries.push_back(fs::current_path() / "Samples" / "Toolsmith" / "scripts" / name);
    for (auto &p : tries)
        if (fs::is_regular_file(p))
            return fs::absolute(p);
    throw std::runtime_error("Bundled script not found: " + name);
}
void install(App &a, const fs::path &p)
{
    auto candidate = compile(p);
    auto model = buildModel(*candidate, p);
    a.runtime = std::move(candidate);
    a.model = std::move(model);
    a.script = p;
    a.document.clear();
    a.saved = encode(a.model);
}
void saveDoc(App &a, const fs::path &p)
{
    samples::requireDataDestination(p);
    const auto data = encode(a.model);
    if (data.size() > MaxDoc)
        throw std::runtime_error("Tool document exceeds 8 MiB");
    if (fs::weakly_canonical(p) == fs::weakly_canonical(a.script) || (fs::exists(p) && fs::equivalent(p, a.script)))
        throw std::runtime_error("Choose a document path different from the tool script");
    samples::writeAtomically(p, [&](std::ostream &out) { out << data; });
    a.document = p;
    a.saved = data;
}
void openDoc(App &a, const fs::path &p)
{
    auto data = samples::readText(p, MaxDoc);
    auto values = decode(data, a.model);
    a.model.values = std::move(values);
    a.document = p;
    a.saved = encode(a.model);
}
void reloadScript(App &a, const fs::path &p)
{
    auto candidate = compile(p);
    auto model = buildModel(*candidate, p, a.model.values);
    if (model.script != a.model.script || model.tool != a.model.tool)
        throw std::runtime_error("Tool identity changed. Use Load Script to start a new document");
    for (const auto &[name, value] : a.model.values)
        if (!value.empty() && !model.values.contains(name))
            throw std::runtime_error("Reload would remove a populated field: " + name + ". Save the document first");
    a.runtime = std::move(candidate);
    a.model = std::move(model);
    a.script = p;
}
void expect(const std::string &got, const std::string &want, const std::string &caseName)
{
    if (got != want)
        throw std::runtime_error("Verify failed: " + caseName);
}
void verify(App &a, const fs::path &root)
{
    fs::create_directories(root);
    auto fixture = samples::testDirectory(root);
    auto original = a.model;
    if (a.model.tool == "Text Formatter")
    {
        a.model.values["input"] = "  Hello  WORLD\nsecond\tLINE  ";
        dispatch(*a.runtime, a.model, "uppercase");
        expect(a.model.values["output"], "  HELLO  WORLD\nSECOND\tLINE  ", "uppercase multiline");
        dispatch(*a.runtime, a.model, "lowercase");
        expect(a.model.values["output"], "  hello  world\nsecond\tline  ", "lowercase multiline");
        dispatch(*a.runtime, a.model, "trim");
        expect(a.model.values["output"], "Hello  WORLD\nsecond\tLINE", "trim");
        dispatch(*a.runtime, a.model, "normalize");
        expect(a.model.values["output"], "Hello WORLD second LINE", "normalize");
        a.model.values["find"] = "LINE";
        a.model.values["replacement"] = "part";
        dispatch(*a.runtime, a.model, "replace");
        expect(a.model.values["output"], "  Hello  WORLD\nsecond\tpart  ", "replace");
        a.model.values["input"] = "one two\tthree";
        a.model.values["find"] = " ";
        a.model.values["replacement"] = "_";
        dispatch(*a.runtime, a.model, "replace");
        expect(a.model.values["output"], "one_two\tthree", "replace a space");
        a.model.values["find"] = "";
        dispatch(*a.runtime, a.model, "replace");
        expect(a.model.values["output"], "Enter text to find before replacing.", "replace validation");
        std::string large;
        for (int i = 0; i < 2000; ++i)
            large += "line " + std::to_string(i) + "\n";
        a.model.values["input"] = large;
        dispatch(*a.runtime, a.model, "uppercase");
        if (a.model.values["output"].size() != large.size() ||
            a.model.values["output"].find("LINE 1999") == std::string::npos)
            throw std::runtime_error("Verify failed: large formatter input");
    }
    else if (a.model.tool == "Notes")
    {
        a.model.values["title"] = "Research";
        a.model.values["body"] = "first line\nsecond line\n\nfinal line";
        dispatch(*a.runtime, a.model, "new_note");
        expect(a.model.values["title"], "", "clear title");
        expect(a.model.values["body"], "", "clear multiline note");
        a.model.values["title"] = "Research";
        a.model.values["body"] = "first line\nsecond line\n\nfinal line";
        std::string large;
        for (int i = 0; i < 2000; ++i)
            large += "note line " + std::to_string(i) + "\n";
        a.model.values["body"] = large;
    }
    const auto doc = fixture / "roundtrip.kstool";
    saveDoc(a, doc);
    a.model.values.begin()->second += "changed";
    saveDoc(a, doc);
    const auto expectedDocument = encode(a.model);
    for (auto &[name, value] : a.model.values)
        value.clear();
    openDoc(a, doc);
    expect(encode(a.model), expectedDocument, "document overwrite roundtrip");
    const auto before = a.model.values;
    try
    {
        samples::writeAtomically(fixture / "invalid.kstool", [](std::ostream &o) { o << "TOOLSMITH 99\n"; });
        openDoc(a, fixture / "invalid.kstool");
        throw std::runtime_error("Verify failed: invalid document accepted");
    }
    catch (const std::runtime_error &e)
    {
        if (std::string(e.what()).find("Verify failed") != std::string::npos)
            throw;
    }
    if (a.model.values != before)
        throw std::runtime_error("Verify failed: invalid open changed working fields");
    const auto savedHandlers = a.model.buttons;
    const auto state = a.model.values;
    a.model.buttons.push_back({"__late_failure", "Regression", "verify_late_failure"});
    try
    {
        dispatch(*a.runtime, a.model, "__late_failure");
        throw std::runtime_error("Verify failed: mutating handler did not fail");
    }
    catch (const std::exception &e)
    {
        if (std::string(e.what()).find("Verify failed") != std::string::npos)
            throw;
    }
    a.model.buttons = savedHandlers;
    if (a.model.values != state)
        throw std::runtime_error("Verify failed: late handler failure changed fields");
    const auto oldRuntime = a.runtime.get();
    const auto oldScript = a.script;
    const auto working = encode(a.model);
    const auto bad = fixture / "bad.ks";
    {
        std::ofstream out(bad);
        out << "not valid script";
    }
    try
    {
        reloadScript(a, bad);
        throw std::runtime_error("Verify failed: bad compile reload accepted");
    }
    catch (const std::exception &e)
    {
        if (std::string(e.what()).find("Verify failed") != std::string::npos)
            throw;
    }
    if (a.runtime.get() != oldRuntime || a.script != oldScript || encode(a.model) != working)
        throw std::runtime_error("Verify failed: bad compile reload changed working tool");
    const auto badBuild = fixture / "bad_build.ks";
    {
        std::ofstream out(badBuild);
        out << "extern tool(name, script) -> void;\nextern field(name, label, kind) -> void;\nextern button(name, "
               "label, handler) -> void;\nfunc build() -> void { tool(\"Broken\", \"bad_build.ks\"); field(\"x\", "
               "\"X\", \"no\"); field(\"x\", \"Duplicate\", \"no\"); button(\"b\", \"B\", \"handler\"); }\n";
    }
    try
    {
        reloadScript(a, badBuild);
        throw std::runtime_error("Verify failed: invalid build reload accepted");
    }
    catch (const std::exception &e)
    {
        if (std::string(e.what()).find("Verify failed") != std::string::npos)
            throw;
    }
    if (a.runtime.get() != oldRuntime || a.script != oldScript || encode(a.model) != working)
        throw std::runtime_error("Verify failed: bad build reload changed working tool");
    a.model = std::move(original);
    std::cout << "VERIFY PASS " << a.model.tool << " fields=" << a.model.fields.size()
              << " buttons=" << a.model.buttons.size() << "\n";
}

Fl_Window *window = nullptr;
Fl_Scroll *content = nullptr;
App *app = nullptr;
std::map<std::string, Fl_Widget *> widgets;
Fl_Box *status = nullptr;
bool updating = false;
void action(Fl_Widget *, void *);
void buildContent()
{
    if (content)
    {
        window->remove(content);
        delete content;
    }
    widgets.clear();
    window->begin();
    content = new Fl_Scroll(0, 44, 860, 642);
    window->end();
    content->begin();
    int y = content->y() + 12;
    const auto areas = std::count_if(app->model.fields.begin(), app->model.fields.end(),
                                     [](const Descriptor &field) { return field.multiline; });
    const int textHeight = areas == 1 ? 400 : 160;
    for (auto &f : app->model.fields)
    {
        auto *label = new Fl_Box(20, y, 800, 24);
        label->copy_label(f.label.c_str());
        label->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        y += 26;
        Fl_Widget *w;
        if (f.output)
            w = new Fl_Multiline_Output(20, y, 800, textHeight);
        else if (f.multiline)
            w = new Fl_Multiline_Input(20, y, 800, textHeight);
        else
            w = new Fl_Input(20, y, 800, 30);
        widgets[f.name] = w;
        y += f.multiline ? textHeight + 12 : 42;
    }
    y += 8;
    int x = 20;
    for (auto &b : app->model.buttons)
    {
        auto *w = new Fl_Button(x, y, 130, 32, b.label.c_str());
        w->callback(action, const_cast<char *>(b.name.c_str()));
        x += 138;
        if (x + 130 > 840)
        {
            x = 20;
            y += 40;
        }
    }
    content->end();
    window->redraw();
}
void sync()
{
    updating = true;
    for (auto &f : app->model.fields)
    {
        auto *w = widgets.at(f.name);
        if (f.output)
            static_cast<Fl_Multiline_Output *>(w)->value(app->model.values[f.name].c_str());
        else if (f.multiline)
            static_cast<Fl_Multiline_Input *>(w)->value(app->model.values[f.name].c_str());
        else
            static_cast<Fl_Input *>(w)->value(app->model.values[f.name].c_str());
    }
    updating = false;
}
void collect()
{
    for (auto &f : app->model.fields)
    {
        if (f.output)
            continue;
        auto *w = widgets.at(f.name);
        app->model.values[f.name] =
            f.multiline ? static_cast<Fl_Multiline_Input *>(w)->value() : static_cast<Fl_Input *>(w)->value();
    }
}
void report(const std::string &s)
{
    status->copy_label(s.c_str());
    status->redraw();
}
void action(Fl_Widget *, void *data)
{
    try
    {
        collect();
        dispatch(*app->runtime, app->model, static_cast<const char *>(data));
        sync();
        report(app->dirty() ? "Unsaved changes" : "Saved");
    }
    catch (const std::exception &e)
    {
        report(e.what());
    }
}
bool confirmDiscard()
{
    collect();
    if (!app->dirty())
        return true;
    return fl_choice("Discard unsaved changes?", "Cancel", "Discard", nullptr) == 1;
}
void saveAs(Fl_Widget *, void *)
{
    try
    {
        collect();
        Fl_Native_File_Chooser chooser;
        chooser.title("Save Toolsmith document");
        chooser.type(Fl_Native_File_Chooser::BROWSE_SAVE_FILE);
        chooser.filter("Toolsmith documents\t*.kstool");
        chooser.options(Fl_Native_File_Chooser::SAVEAS_CONFIRM);
        chooser.preset_file(app->document.empty() ? "untitled.kstool" : app->document.string().c_str());
        const int chosen = chooser.show();
        if (chosen < 0)
            throw std::runtime_error(chooser.errmsg());
        if (chosen != 0 || !chooser.filename())
            return;
        saveDoc(*app, chooser.filename());
        report("Saved");
    }
    catch (const std::exception &e)
    {
        report(e.what());
    }
}
void save(Fl_Widget *, void *)
{
    try
    {
        collect();
        if (app->document.empty())
            saveAs(nullptr, nullptr);
        else
        {
            saveDoc(*app, app->document);
            report("Saved");
        }
    }
    catch (const std::exception &e)
    {
        report(e.what());
    }
}
void open(Fl_Widget *, void *)
{
    if (!confirmDiscard())
        return;
    const char *p = fl_file_chooser("Open Toolsmith document", "*.kstool", nullptr);
    if (!p)
        return;
    try
    {
        openDoc(*app, p);
        sync();
        report("Opened");
    }
    catch (const std::exception &e)
    {
        report(e.what());
    }
}
void closeWindow(Fl_Widget *w, void *)
{
    if (confirmDiscard())
        w->hide();
}
void newTool(Fl_Widget *, void *)
{
    if (!confirmDiscard())
        return;
    try
    {
        auto model = buildModel(*app->runtime, app->script);
        app->model = std::move(model);
        app->document.clear();
        app->saved = encode(app->model);
        buildContent();
        sync();
        report("New document");
    }
    catch (const std::exception &error)
    {
        report(error.what());
    }
}
void selectTool(Fl_Widget *, void *)
{
    if (!confirmDiscard())
        return;
    int choice = fl_choice("Choose a bundled tool", "Cancel", "Text Formatter", "Notes");
    if (choice == 0)
        return;
    try
    {
        install(*app, bundled(choice == 1 ? "ui.ks" : "notes.ks"));
        window->copy_label(app->model.tool.c_str());
        buildContent();
        sync();
        report("Tool loaded");
    }
    catch (const std::exception &e)
    {
        report(e.what());
    }
}
void loadCustom(Fl_Widget *, void *)
{
    if (!confirmDiscard())
        return;
    const char *p = fl_file_chooser("Load a custom tool script", "*.ks", nullptr);
    if (!p)
        return;
    try
    {
        install(*app, fs::absolute(p));
        window->copy_label(app->model.tool.c_str());
        buildContent();
        sync();
        report("Tool loaded");
    }
    catch (const std::exception &e)
    {
        report(e.what());
    }
}
void reload(Fl_Widget *, void *)
{
    try
    {
        collect();
        reloadScript(*app, app->script);
        window->copy_label(app->model.tool.c_str());
        buildContent();
        sync();
        report("Script reloaded");
    }
    catch (const std::exception &e)
    {
        report(e.what());
    }
}
} // namespace
int main(int argc, char **argv)
{
    bool verifyMode = false, scriptSpecified = false;
    fs::path script, openFile, testRoot, preview;
    for (int i = 1; i < argc; ++i)
    {
        std::string a = argv[i];
        if (a == "--verify")
            verifyMode = true;
        else if (a == "--help")
        {
            std::cout << "Toolsmith [--script FILE] [--open FILE] [--verify --test-root DIR] [--preview PNG]\n";
            return 0;
        }
        else if ((a == "--script" || a == "--open" || a == "--test-root" || a == "--preview") && i + 1 < argc)
        {
            fs::path p = argv[++i];
            if (a == "--script")
            {
                script = p;
                scriptSpecified = true;
            }
            else if (a == "--open")
                openFile = p;
            else if (a == "--test-root")
                testRoot = p;
            else
                preview = p;
        }
        else
        {
            std::cerr << "Unknown option: " << a << '\n';
            return 2;
        }
    }
    if (argc > 0)
        g_argv0 = argv[0];
    try
    {
        if (script.empty())
            script = bundled("ui.ks");
        else
            script = fs::absolute(script);
        App a;
        app = &a;
        install(a, script);
        if (!openFile.empty())
            openDoc(a, openFile);
        if (verifyMode)
        {
            auto root = testRoot.empty() ? fs::current_path() / "Samples" / "Toolsmith" / "test-tmp" : testRoot;
            verify(a, root);
            if (!scriptSpecified)
            {
                install(a, bundled("notes.ks"));
                verify(a, root);
            }
            return 0;
        }
        Fl_Window appWindow(860, 720, a.model.tool.c_str());
        window = &appWindow;
        window->begin();
        status = new Fl_Box(12, 690, 836, 22, "Ready");
        Fl_Button *n = new Fl_Button(8, 8, 74, 30, "New");
        n->callback(newTool);
        Fl_Button *o = new Fl_Button(84, 8, 74, 30, "Open");
        o->callback(open);
        Fl_Button *s = new Fl_Button(160, 8, 74, 30, "Save");
        s->callback(save);
        Fl_Button *sa = new Fl_Button(236, 8, 82, 30, "Save As");
        sa->callback(saveAs);
        Fl_Button *r = new Fl_Button(320, 8, 78, 30, "Reload");
        r->callback(reload);
        Fl_Button *st = new Fl_Button(400, 8, 92, 30, "Tool");
        st->callback(selectTool);
        Fl_Button *lc = new Fl_Button(494, 8, 112, 30, "Load Script");
        lc->callback(loadCustom);
        buildContent();
        window->end();
        window->callback(closeWindow);
        sync();
        if (!preview.empty())
        {
            samples::renderPreview(*window, preview);
            return 0;
        }
        window->show(argc, argv);
        Fl::run();
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
