#include <cstdint>
#include <filesystem>
#include <functional>
#include <iostream>
#include <string>

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Hold_Browser.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Window.H>
#include <FL/fl_ask.H>

#include "../../Support/Preview.h"
#include "Story.h"
#include "Verify.h"

namespace
{
using namespace storybook;
Engine game;
Fl_Window *window = nullptr;
Fl_Input *heroInput = nullptr;
Fl_Hold_Browser *history = nullptr;
Fl_Text_Buffer *sceneText = nullptr;
Fl_Box *sceneTitle = nullptr, *status = nullptr;
Fl_Button *choices[3] = {}, *nextButton = nullptr, *backButton = nullptr;
std::size_t viewed = 0;

void showScene()
{
    const auto &session = game.session();
    const auto &frame = session.frames.at(viewed);
    sceneTitle->copy_label(frame.title.c_str());
    std::string text;
    for (const auto &line : frame.lines)
        text += line.actor + ":\n" + line.words + "\n\n";
    if (!frame.ending.empty())
        text += "Ending: " + frame.ending + "\n";
    if (viewed < session.actions.size())
        for (const auto &choice : frame.choices)
            if (choice.id == session.actions[viewed].id)
                text += "You chose: " + choice.label + "\n";
    sceneText->text(text.c_str());
    const bool current = viewed + 1 == session.frames.size();
    for (int i = 0; i < 3; ++i)
    {
        if (current && static_cast<std::size_t>(i) < frame.choices.size())
        {
            choices[i]->copy_label(frame.choices[i].label.c_str());
            choices[i]->activate();
        }
        else
        {
            choices[i]->label("");
            choices[i]->deactivate();
        }
    }
    if (current && !frame.next.empty())
        nextButton->activate();
    else
        nextButton->deactivate();
    if (session.actions.empty())
        backButton->deactivate();
    else
        backButton->activate();
    std::string state = current ? "Current scene" : "Reading history. Use Current to resume.";
    if (current && !frame.ending.empty())
        state = "Story complete. Use Back to explore another choice, or New to start again.";
    state += " | Scene " + std::to_string(viewed + 1) + " of " + std::to_string(session.frames.size());
    status->copy_label(state.c_str());
}

void refresh()
{
    history->clear();
    for (const auto &frame : game.session().frames)
        history->add(frame.title.c_str());
    viewed = game.session().frames.size() - 1;
    history->value(static_cast<int>(viewed + 1));
    history->bottomline(static_cast<int>(viewed + 1));
    const std::string title = "Storybook | " +
                              (game.file().empty() ? std::string("Untitled") : game.file().filename().string()) +
                              (game.dirty() ? " *" : "");
    window->copy_label(title.c_str());
    showScene();
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

fs::path chooseFile(bool save, const char *title, const char *filter, const char *preset)
{
    Fl_Native_File_Chooser chooser;
    chooser.title(title);
    chooser.type(save ? Fl_Native_File_Chooser::BROWSE_SAVE_FILE : Fl_Native_File_Chooser::BROWSE_FILE);
    chooser.filter(filter);
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

bool saveGame(bool saveAs = false)
{
    auto file = game.file();
    if (saveAs || file.empty())
        file = chooseFile(true, "Save playthrough", "Storybook saves\t*.storybook", "journey.storybook");
    if (file.empty())
        return false;
    game.save(file);
    refresh();
    return true;
}

bool resolveUnsaved()
{
    if (!game.dirty())
        return true;
    const int answer = fl_choice("Save this playthrough before leaving it?", "Cancel", "Discard", "Save");
    return answer == 1 || (answer == 2 && saveGame());
}

void onAction(Fl_Widget *, void *data)
{
    safely(
        [&]
        {
            const int index = static_cast<int>(reinterpret_cast<std::intptr_t>(data));
            const auto action = index < 0 ? std::string("@next") : game.current().choices.at(index).id;
            game.advance(action);
            refresh();
        });
}
void onHistory(Fl_Widget *, void *)
{
    safely(
        [&]
        {
            if (history->value() > 0)
            {
                viewed = static_cast<std::size_t>(history->value() - 1);
                showScene();
            }
        });
}
void onCurrent(Fl_Widget *, void *)
{
    viewed = game.session().frames.size() - 1;
    history->value(static_cast<int>(viewed + 1));
    showScene();
}
void onBack(Fl_Widget *, void *)
{
    safely(
        []
        {
            game.back();
            refresh();
        });
}
void onNew(Fl_Widget *, void *)
{
    safely(
        []
        {
            if (!resolveUnsaved())
                return;
            game.restart(heroInput->value());
            refresh();
        });
}
void onOpen(Fl_Widget *, void *)
{
    safely(
        []
        {
            const auto path = chooseFile(false, "Open playthrough", "Storybook saves\t*.storybook", "");
            if (path.empty() || !resolveUnsaved())
                return;
            game.open(path);
            heroInput->value(game.session().hero.c_str());
            refresh();
        });
}
void onSave(Fl_Widget *, void *)
{
    safely([] { (void)saveGame(); });
}
void onSaveAs(Fl_Widget *, void *)
{
    safely([] { (void)saveGame(true); });
}
void onReload(Fl_Widget *, void *)
{
    safely(
        []
        {
            game.reload();
            refresh();
        });
}
void onStory(Fl_Widget *, void *)
{
    safely(
        []
        {
            const auto path = chooseFile(false, "Load a story script", "KernScript stories\t*.ks", "");
            if (path.empty() || !resolveUnsaved())
                return;
            game.start(path, heroInput->value());
            refresh();
        });
}
void onExport(Fl_Widget *, void *)
{
    safely(
        []
        {
            const auto path = chooseFile(true, "Export story transcript", "Text files\t*.txt", "journey.txt");
            if (!path.empty())
                game.exportTranscript(path);
        });
}
void onClose(Fl_Widget *, void *)
{
    safely(
        []
        {
            if (resolveUnsaved())
                window->hide();
        });
}

fs::path resolveStory(const fs::path &executable)
{
    for (const auto &path : {executable.parent_path() / "chapters/story.ks", fs::path("chapters/story.ks"),
                             fs::path("Samples/Storybook/chapters/story.ks")})
        if (fs::is_regular_file(path))
            return fs::absolute(path);
    throw std::runtime_error("Story scripts were not found. Use --story PATH to select one");
}
} // namespace

int main(int argc, char **argv)
{
    try
    {
        fs::path source, openFile, preview, testRoot = "build/test-tmp/Storybook";
        bool runVerify = false;
        for (int i = 1; i < argc; ++i)
        {
            const std::string arg = argv[i];
            if (arg == "--verify")
                runVerify = true;
            else if ((arg == "--story" || arg == "--open" || arg == "--test-root" || arg == "--preview") &&
                     i + 1 < argc)
            {
                const fs::path value = argv[++i];
                if (arg == "--story")
                    source = fs::absolute(value);
                else if (arg == "--open")
                    openFile = value;
                else if (arg == "--preview")
                    preview = value;
                else
                    testRoot = value;
            }
            else if (arg == "--help")
            {
                std::cout << "Storybook: branching fiction with saved playthroughs and history.\n"
                             "Storybook [--story ROOT.ks] [--open SAVE.storybook]\n"
                             "Storybook --verify [--test-root DIR] runs without windows.\n";
                return 0;
            }
            else
                throw std::invalid_argument("Unknown or incomplete argument: " + arg);
        }
        if (source.empty())
            source = resolveStory(argc > 0 ? fs::path(argv[0]) : fs::path{});
        // The verifier never constructs or shows any FLTK widgets.
        if (runVerify)
            return verify(source, testRoot);
        game.start(source, "Ash");
        if (!openFile.empty())
            game.open(openFile);

        Fl_Window app(1080, 760, "Storybook");
        window = &app;
        Fl_Box nameLabel(20, 20, 150, 30, "Name for new story:");
        nameLabel.align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        Fl_Input name(175, 20, 210, 30);
        name.value(game.session().hero.c_str());
        name.maximum_size(80);
        heroInput = &name;
        Fl_Button fresh(400, 20, 70, 30, "New");
        fresh.callback(onNew);
        Fl_Button open(480, 20, 75, 30, "Open");
        open.callback(onOpen);
        Fl_Button save(565, 20, 75, 30, "Save");
        save.callback(onSave);
        Fl_Button saveAs(650, 20, 80, 30, "Save As");
        saveAs.callback(onSaveAs);
        Fl_Button exportText(740, 20, 150, 30, "Export Transcript");
        exportText.callback(onExport);
        Fl_Button load(900, 20, 160, 30, "Load Story Script");
        load.callback(onStory);
        Fl_Box backlogLabel(20, 65, 250, 28, "Scene history");
        backlogLabel.align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        Fl_Hold_Browser backlog(20, 95, 250, 500);
        history = &backlog;
        backlog.format_char(0);
        backlog.callback(onHistory);
        Fl_Box heading(290, 65, 770, 28);
        heading.align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        sceneTitle = &heading;
        Fl_Text_Buffer buffer;
        sceneText = &buffer;
        Fl_Text_Display reader(290, 95, 770, 500);
        reader.buffer(buffer);
        reader.textsize(17);
        reader.wrap_mode(Fl_Text_Display::WRAP_AT_BOUNDS, 0);
        Fl_Button option1(290, 610, 770, 30), option2(290, 648, 770, 30), option3(290, 686, 770, 30);
        choices[0] = &option1;
        choices[1] = &option2;
        choices[2] = &option3;
        for (int i = 0; i < 3; ++i)
            choices[i]->callback(onAction, reinterpret_cast<void *>(static_cast<std::intptr_t>(i)));
        Fl_Button current(20, 610, 120, 30, "Current");
        current.callback(onCurrent);
        Fl_Button back(150, 610, 120, 30, "Back / Undo");
        backButton = &back;
        back.callback(onBack);
        Fl_Button next(20, 650, 250, 30, "Continue");
        nextButton = &next;
        next.callback(onAction, reinterpret_cast<void *>(static_cast<std::intptr_t>(-1)));
        Fl_Button reload(20, 690, 250, 30, "Reload Story Scripts");
        reload.callback(onReload);
        Fl_Box footer(20, 725, 1040, 25);
        footer.align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        status = &footer;
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
        std::cerr << "Storybook: " << error.what() << '\n';
        return 1;
    }
}
