#include "../../Support/Preview.h"
#include "Conversation.h"
#include "Verify.h"
#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Browser.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Window.H>
#include <FL/fl_ask.H>
#include <functional>
#include <iostream>

namespace
{
using namespace dialogue;
Conversation conversation;
fs::path scripts;
Fl_Window *window = nullptr;
Fl_Input *message = nullptr;
Fl_Text_Buffer *transcript = nullptr;
Fl_Text_Display *display = nullptr;
Fl_Browser *inspector = nullptr;
Fl_Box *status = nullptr;
void refresh()
{
    std::string log;
    for (const auto &turn : conversation.session().turns)
    {
        if (!turn.input.empty())
            log += "You: " + turn.input + "\n";
        log += "Reply: " + turn.response + "\n\n";
    }
    transcript->text(log.c_str());
    display->insert_position(transcript->length());
    display->show_insert_position();
    inspector->clear();
    inspector->add(("Matched rule: " + conversation.session().turns.back().rule).c_str());
    inspector->add("Current conversation state:");
    for (const auto &[name, value] : conversation.session().state)
        inspector->add((name + " = " + std::to_string(value)).c_str());
    const auto title =
        "Dialogue | " +
        (conversation.file().empty() ? std::string("Untitled") : conversation.file().filename().string()) +
        (conversation.dirty() ? " *" : "");
    window->copy_label(title.c_str());
    const auto description = conversation.session().scenario + " | " + conversation.source().filename().string() +
                             " | " + std::to_string(conversation.session().turns.size() - 1) + " messages";
    status->copy_label(description.c_str());
}
void safely(const std::function<void()> &operation)
{
    try
    {
        operation();
    }
    catch (const std::exception &error)
    {
        fl_alert("%s", error.what());
    }
}
fs::path choose(bool save, const char *title, const char *filter, const char *preset = "")
{
    Fl_Native_File_Chooser chooser;
    chooser.title(title);
    chooser.filter(filter);
    chooser.type(save ? Fl_Native_File_Chooser::BROWSE_SAVE_FILE : Fl_Native_File_Chooser::BROWSE_FILE);
    if (save)
    {
        chooser.options(Fl_Native_File_Chooser::SAVEAS_CONFIRM);
        chooser.preset_file(preset);
    }
    const int result = chooser.show();
    if (result < 0)
        throw std::runtime_error(chooser.errmsg());
    return result == 0 && chooser.filename() ? fs::path(chooser.filename()) : fs::path{};
}
bool draft()
{
    if (std::string(message->value()).empty())
        return true;
    const int action = fl_choice("Send the draft message before continuing?", "Cancel", "Discard draft", "Send");
    if (action == 0)
        return false;
    if (action == 2)
        conversation.send(message->value());
    message->value("");
    refresh();
    return true;
}
bool save(bool saveAs = false)
{
    if (!draft())
        return false;
    auto path = conversation.file();
    if (saveAs || path.empty())
        path = choose(true, "Save conversation", "Dialogue conversations\t*.dialogue", "conversation.dialogue");
    if (path.empty())
        return false;
    conversation.save(path);
    refresh();
    return true;
}
bool leaving()
{
    if (!draft())
        return false;
    if (!conversation.dirty())
        return true;
    const int answer = fl_choice("Save this conversation before leaving it?", "Cancel", "Discard", "Save");
    return answer == 1 || (answer == 2 && save());
}
void onSend(Fl_Widget *, void *)
{
    safely(
        []
        {
            if (std::string(message->value()).empty())
                return;
            conversation.send(message->value());
            message->value("");
            refresh();
        });
}
void onNew(Fl_Widget *, void *)
{
    safely(
        []
        {
            if (!leaving())
                return;
            conversation.start(conversation.source());
            refresh();
        });
}
void onOpen(Fl_Widget *, void *)
{
    safely(
        []
        {
            const auto file = choose(false, "Open conversation", "Dialogue conversations\t*.dialogue");
            if (file.empty() || !leaving())
                return;
            conversation.open(file);
            refresh();
        });
}
void onSave(Fl_Widget *, void *)
{
    safely([] { (void)save(); });
}
void onSaveAs(Fl_Widget *, void *)
{
    safely([] { (void)save(true); });
}
void onExport(Fl_Widget *, void *)
{
    safely(
        []
        {
            if (!draft())
                return;
            const auto file = choose(true, "Export transcript", "Text files\t*.txt", "conversation.txt");
            if (!file.empty())
                conversation.exportTranscript(file);
        });
}
void onReload(Fl_Widget *, void *)
{
    safely(
        []
        {
            conversation.reload();
            refresh();
        });
}
void onUndo(Fl_Widget *, void *)
{
    safely(
        []
        {
            conversation.undo();
            refresh();
        });
}
void loadScenario(const fs::path &path)
{
    if (!leaving())
        return;
    conversation.start(path);
    refresh();
}
void onScenario(Fl_Widget *, void *)
{
    safely(
        []
        {
            const auto path = choose(false, "Load scenario script", "KernScript scenarios\t*.ks");
            if (!path.empty())
                loadScenario(path);
        });
}
void onSupport(Fl_Widget *, void *)
{
    safely([] { loadScenario(scripts / "support.ks"); });
}
void onGate(Fl_Widget *, void *)
{
    safely([] { loadScenario(scripts / "gatekeeper.ks"); });
}
void onClose(Fl_Widget *, void *)
{
    safely(
        []
        {
            if (leaving())
                window->hide();
        });
}
fs::path resources(const fs::path &exe)
{
    for (const auto &path : {exe.parent_path() / "scripts", fs::path("scripts"), fs::path("Samples/Dialogue/scripts")})
        if (fs::is_regular_file(path / "support.ks"))
            return fs::absolute(path);
    throw std::runtime_error("Dialogue scenario scripts were not found");
}
} // namespace
int main(int argc, char **argv)
{
    try
    {
        fs::path source, openFile, preview, testRoot = "build/test-tmp/Dialogue";
        bool verifyMode = false;
        for (int i = 1; i < argc; ++i)
        {
            const std::string arg = argv[i];
            if (arg == "--help")
            {
                std::cout << "Dialogue: author and test stateful conversation scenarios.\n"
                             "Dialogue [--scenario FILE.ks] [--open FILE.dialogue]\n"
                             "Dialogue --verify [--test-root DIR] runs without windows.\n";
                return 0;
            }
            if (arg == "--verify")
            {
                verifyMode = true;
                continue;
            }
            if ((arg == "--scenario" || arg == "--open" || arg == "--test-root" || arg == "--preview") && i + 1 < argc)
            {
                const fs::path value = argv[++i];
                if (arg == "--scenario")
                    source = fs::absolute(value);
                else if (arg == "--open")
                    openFile = value;
                else if (arg == "--preview")
                    preview = value;
                else
                    testRoot = value;
            }
            else
                throw std::invalid_argument("Unknown or incomplete argument: " + arg);
        }
        scripts = resources(argc > 0 ? fs::path(argv[0]) : fs::path{});
        if (verifyMode)
            return verify(scripts, testRoot);
        conversation.start(source.empty() ? scripts / "support.ks" : source);
        if (!openFile.empty())
            conversation.open(openFile);
        Fl_Window app(1120, 740, "Dialogue");
        window = &app;
        Fl_Button fresh(20, 20, 75, 30, "New");
        fresh.callback(onNew);
        Fl_Button open(105, 20, 75, 30, "Open");
        open.callback(onOpen);
        Fl_Button saveButton(190, 20, 75, 30, "Save");
        saveButton.callback(onSave);
        Fl_Button saveAs(275, 20, 85, 30, "Save As");
        saveAs.callback(onSaveAs);
        Fl_Button exportText(370, 20, 150, 30, "Export Transcript");
        exportText.callback(onExport);
        Fl_Button undo(530, 20, 110, 30, "Undo Message");
        undo.callback(onUndo);
        Fl_Button support(20, 65, 150, 30, "Shop Support");
        support.callback(onSupport);
        Fl_Button gate(180, 65, 150, 30, "Gatekeeper NPC");
        gate.callback(onGate);
        Fl_Button load(340, 65, 150, 30, "Load Scenario");
        load.callback(onScenario);
        Fl_Button reload(500, 65, 140, 30, "Reload Script");
        reload.callback(onReload);
        Fl_Text_Buffer buffer;
        transcript = &buffer;
        Fl_Text_Display log(20, 115, 770, 510);
        display = &log;
        log.buffer(buffer);
        log.textsize(16);
        log.wrap_mode(Fl_Text_Display::WRAP_AT_BOUNDS, 0);
        Fl_Browser state(810, 115, 290, 510, "Rule and state inspector");
        inspector = &state;
        state.format_char(0);
        Fl_Input input(20, 645, 960, 32);
        message = &input;
        input.maximum_size(4096);
        input.when(FL_WHEN_ENTER_KEY_ALWAYS);
        input.callback(onSend);
        Fl_Button send(990, 645, 110, 32, "Send");
        send.callback(onSend);
        Fl_Box footer(20, 690, 1080, 30);
        status = &footer;
        footer.align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        app.callback(onClose);
        app.end();
        refresh();
        if (!preview.empty())
        {
            samples::renderPreview(app, preview);
            return 0;
        }
        app.show();
        return Fl::run();
    }
    catch (const std::exception &error)
    {
        std::cerr << "Dialogue: " << error.what() << '\n';
        return 1;
    }
}
