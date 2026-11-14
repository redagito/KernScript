#include <array>
#include <charconv>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "../../Support/Preview.h"
#include "model.hpp"
#include "ranker.hpp"

#include <FL/Fl.H>
#include <FL/Fl_Browser.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/Fl_Window.H>
#include <FL/fl_ask.H>

namespace fs = std::filesystem;

namespace
{
using daybook::Model;
using daybook::Ranked;
using daybook::Ranker;
using daybook::Task;

bool parseInt(const char *text, int &value)
{
    if (text == nullptr || *text == '\0')
        return false;
    const char *end = text;
    while (*end != '\0')
        ++end;
    const auto parsed = std::from_chars(text, end, value);
    return parsed.ec == std::errc{} && parsed.ptr == end;
}

Task exampleTask(std::string title, std::string kind, int priority, int minutes, std::string deadline, std::string tags,
                 bool completed)
{
    Task task;
    task.title = std::move(title);
    task.kind = std::move(kind);
    task.priority = priority;
    task.minutes = minutes;
    task.deadline = std::move(deadline);
    task.tags = std::move(tags);
    task.completed = completed;
    return task;
}

bool verify(const fs::path &testRoot, const fs::path &policyPath)
{
    try
    {
        const fs::path fixture = samples::testDirectory(testRoot);
        const auto fixedToday =
            std::chrono::year_month_day{std::chrono::year{2026}, std::chrono::month{11}, std::chrono::day{14}};
        Model model;
        const uint64_t writtenId = model.add(exampleTask("Write tests with spaces and \"quotes\"", "feature", 3, 40,
                                                         "2026-11-20", "home, writing", false));
        const uint64_t fixId = model.add(exampleTask("Fix bug", "bug", 5, 20, "2026-11-14", "release", false));
        if (!model.complete(writtenId, true))
            throw std::runtime_error("completion update failed");
        Task edited = exampleTask("Write more tests with spaces and \"quotes\"", "feature", 4, 45, "2026-11-18",
                                  "home, writing, review", true);
        if (!model.edit(writtenId, edited) || !model.findTask(fixId))
            throw std::runtime_error("task edit failed");
        const uint64_t temporaryId = model.add(exampleTask("Temporary task", "chore", 1, 5, "", "", false));
        if (!model.erase(temporaryId) || model.findTask(temporaryId) != nullptr)
            throw std::runtime_error("task deletion failed");

        const auto stableBeforeInvalid = model.tasks();
        const auto dirtyBeforeInvalid = model.dirty();
        auto rejects = [](auto &&operation)
        {
            try
            {
                operation();
            }
            catch (const std::exception &)
            {
                return true;
            }
            return false;
        };
        if (!rejects([&] { model.add(exampleTask("bad date", "task", 2, 10, "2026-02-30", "", false)); }))
            throw std::runtime_error("invalid add unexpectedly succeeded");
        if (!rejects(
                [&]
                {
                    Task invalid = edited;
                    invalid.priority = 8;
                    model.edit(writtenId, invalid);
                }))
            throw std::runtime_error("invalid edit unexpectedly succeeded");
        if (model.tasks() != stableBeforeInvalid || model.dirty() != dirtyBeforeInvalid)
            throw std::runtime_error("invalid mutation changed model");

        Ranker ranker(fixedToday);
        std::string error;
        Model empty;
        std::vector<Ranked> emptyRows;
        if (!ranker.reload(policyPath, empty, error) || !ranker.rank(empty, emptyRows, error) || !emptyRows.empty())
            throw std::runtime_error("Empty task list displayed the policy validation probe");
        if (!ranker.reload(policyPath, model, error))
            throw std::runtime_error("default policy failed: " + error);
        std::vector<Ranked> ordered;
        if (!ranker.rank(model, ordered, error) || ordered.size() != 2 || ordered.front().index != 1)
            throw std::runtime_error("fixed-date urgency ranking failed: " + error);

        const fs::path saved = fixture / "personal plans.daybook";
        model.save(saved);
        Model loaded;
        loaded.open(saved);
        if (loaded.tasks() != model.tasks() || loaded.nextId() != model.nextId() || loaded.dirty() ||
            loaded.file() != saved)
            throw std::runtime_error("full task roundtrip failed");

        model.complete(fixId, true);
        const fs::path protectedTarget = fixture / "protected-target";
        fs::create_directory(protectedTarget);
        const fs::path sentinel = protectedTarget / "keep.txt";
        {
            std::ofstream out(sentinel, std::ios::binary);
            out << "preserve this user file";
        }
        if (!rejects([&] { model.save(protectedTarget); }) || !model.dirty())
            throw std::runtime_error("failed save changed document state");
        if (samples::readText(sentinel, 1024) != "preserve this user file")
            throw std::runtime_error("failed save changed neighboring user data");
        model.save(saved); // Replace the existing file after a failed save.
        loaded.open(saved);
        if (loaded.tasks() != model.tasks())
            throw std::runtime_error("overwrite save roundtrip failed");

        const auto preservedTasks = loaded.tasks();
        const auto preservedNext = loaded.nextId();
        const fs::path malformed = fixture / "bad.daybook";
        {
            std::ofstream out(malformed, std::ios::binary);
            out << "broken\n";
        }
        if (!rejects([&] { loaded.open(malformed); }))
            throw std::runtime_error("malformed open unexpectedly succeeded");
        if (loaded.tasks() != preservedTasks || loaded.nextId() != preservedNext)
            throw std::runtime_error("failed open changed current model");

        const fs::path duplicate = fixture / "duplicate.daybook";
        {
            std::ofstream out(duplicate, std::ios::binary);
            out << "KernScript Daybook 1\n2 3\n"
                   "1 3 20 0 \"one\" \"task\" \"\" \"\"\n"
                   "1 2 25 0 \"two\" \"task\" \"\" \"\"\n";
        }
        if (!rejects([&] { loaded.open(duplicate); }))
            throw std::runtime_error("duplicate ID file unexpectedly opened");
        if (loaded.tasks() != preservedTasks)
            throw std::runtime_error("duplicate ID file changed model");

        const fs::path invalidDate = fixture / "bad-date.daybook";
        {
            std::ofstream out(invalidDate, std::ios::binary);
            out << "KernScript Daybook 1\n1 2\n1 3 20 0 \"one\" \"task\" \"\" \"2026-02-30\"\n";
        }
        if (!rejects([&] { loaded.open(invalidDate); }))
            throw std::runtime_error("invalid date file unexpectedly opened");
        if (loaded.tasks() != preservedTasks)
            throw std::runtime_error("invalid date file changed model");

        const fs::path customPolicy = fixture / "custom.ks";
        {
            std::ofstream out(customPolicy);
            out << R"(extern priority() -> value;
extern has_tag(tag) -> value;
extern emit(score) -> void;
extern explain(reason) -> void;
func rank() -> void {
  var score = priority() * 100;
  if (has_tag("review")) { score = score + 1; }
  emit(score);
  explain("Custom priority and tag policy");
}
)";
        }
        if (!ranker.reload(customPolicy, loaded, error) || !ranker.rank(loaded, ordered, error) ||
            ordered.front().score != 500 || ordered.back().score != 401)
            throw std::runtime_error("edited policy did not change ranking: " + error);

        const fs::path noEntry = fixture / "missing-rank.ks";
        {
            std::ofstream out(noEntry);
            out << "extern emit(score) -> void; func helper() -> void { emit(5); }\n";
        }
        if (ranker.reload(noEntry, loaded, error) || !ranker.rank(loaded, ordered, error) ||
            ordered.front().score != 500)
            throw std::runtime_error("missing entry policy replaced working policy");

        const fs::path runtimeFailure = fixture / "runtime-failure.ks";
        {
            std::ofstream out(runtimeFailure);
            out << "extern emit(score) -> void; extern explain(reason) -> void;\n"
                   "func rank() -> void { while (1 == 1) { emit(0); } explain(\"never reached\"); }\n";
        }
        if (ranker.reload(runtimeFailure, loaded, error) || !ranker.rank(loaded, ordered, error) ||
            ordered.front().score != 500)
            throw std::runtime_error("runtime-failing policy replaced working policy");

        Model large;
        for (int i = 0; i < 1'200; ++i)
            large.add(exampleTask("Work item " + std::to_string(i), (i % 2) ? "bug" : "chore", i % 5 + 1, i % 240, "",
                                  "load", false));
        if (!ranker.rank(large, ordered, error) || ordered.size() != 1'200)
            throw std::runtime_error("large workload ranking failed: " + error);
        std::vector<Ranked> repeated;
        if (!ranker.rank(large, repeated, error) || repeated.size() != ordered.size())
            throw std::runtime_error("repeat workload ranking failed: " + error);
        Model completionOrder;
        completionOrder.add(exampleTask("Long open task", "task", 1, 1000000, "", "", false));
        completionOrder.add(exampleTask("Completed urgent task", "bug", 5, 0, "2026-11-13", "", true));
        Ranker defaultRanker(fixedToday);
        std::vector<Ranked> completionRows;
        if (!defaultRanker.reload(policyPath, completionOrder, error) ||
            !defaultRanker.rank(completionOrder, completionRows, error) || completionRows.front().index != 0)
            throw std::runtime_error("Completed tasks outranked open work under the default policy");
        for (std::size_t i = 0; i < ordered.size(); ++i)
            if (ordered[i].score != repeated[i].score || ordered[i].index != repeated[i].index ||
                ordered[i].reason != repeated[i].reason)
                throw std::runtime_error("ranking is not deterministic");

        std::cout << "Daybook verification passed: CRUD, validation, policy reload, persistence, and 1200 tasks\n";
        return true;
    }
    catch (const std::exception &exception)
    {
        std::cerr << "Daybook verification failed: " << exception.what() << '\n';
        return false;
    }
}

struct UI
{
    Model model;
    Ranker ranker;
    std::vector<Ranked> rows;
    Fl_Window *window = nullptr;
    Fl_Browser *list = nullptr;
    Fl_Input *title = nullptr;
    Fl_Input *kind = nullptr;
    Fl_Input *priority = nullptr;
    Fl_Input *minutes = nullptr;
    Fl_Input *deadline = nullptr;
    Fl_Input *tags = nullptr;
    Fl_Button *complete = nullptr;
    uint64_t selectedId = 0;
    std::array<std::string, 6> editorBaseline;
};

UI *g_ui = nullptr;

std::array<std::string, 6> editorValues()
{
    return {g_ui->title->value(),   g_ui->kind->value(),     g_ui->priority->value(),
            g_ui->minutes->value(), g_ui->deadline->value(), g_ui->tags->value()};
}

template <typename Callback> void reportFailure(const char *action, Callback &&callback)
{
    try
    {
        callback();
    }
    catch (const std::exception &exception)
    {
        fl_alert("%s failed: %s", action, exception.what());
    }
}

void setDefaults()
{
    g_ui->selectedId = 0;
    g_ui->title->value("");
    g_ui->kind->value("task");
    g_ui->priority->value("3");
    g_ui->minutes->value("30");
    g_ui->deadline->value("");
    g_ui->tags->value("");
    g_ui->complete->deactivate();
    g_ui->complete->label("Mark complete");
    g_ui->editorBaseline = editorValues();
}

void showTask(const Task &task)
{
    g_ui->selectedId = task.id;
    g_ui->title->value(task.title.c_str());
    g_ui->kind->value(task.kind.c_str());
    g_ui->priority->value(std::to_string(task.priority).c_str());
    g_ui->minutes->value(std::to_string(task.minutes).c_str());
    g_ui->deadline->value(task.deadline.c_str());
    g_ui->tags->value(task.tags.c_str());
    g_ui->complete->activate();
    g_ui->complete->label(task.completed ? "Mark open" : "Mark complete");
    g_ui->editorBaseline = editorValues();
}

Task readEditor()
{
    Task task;
    task.title = g_ui->title->value();
    task.kind = g_ui->kind->value();
    task.tags = g_ui->tags->value();
    task.deadline = g_ui->deadline->value();
    if (!parseInt(g_ui->priority->value(), task.priority) || !parseInt(g_ui->minutes->value(), task.minutes))
        throw std::invalid_argument("Priority and minutes must be whole numbers with no extra characters");
    return task;
}

void refresh(uint64_t preserveId = 0)
{
    std::string error;
    std::vector<Ranked> newRows;
    g_ui->ranker.setToday(daybook::localToday());
    if (!g_ui->ranker.rank(g_ui->model, newRows, error))
    {
        g_ui->rows.clear();
        g_ui->list->clear();
        setDefaults();
        fl_alert("Ranking failed: %s", error.c_str());
        return;
    }
    g_ui->rows = std::move(newRows);
    g_ui->list->clear();
    int selectedRow = 0;
    for (std::size_t i = 0; i < g_ui->rows.size(); ++i)
    {
        const auto &ranked = g_ui->rows[i];
        const auto &task = g_ui->model.tasks()[ranked.index];
        const std::string line = std::string(task.completed ? "[x] " : "[ ] ") + std::to_string(ranked.score) + "  " +
                                 task.title + "  " + task.kind + " p" + std::to_string(task.priority) + "  " +
                                 std::to_string(task.minutes) + "m  " +
                                 (task.deadline.empty() ? "no deadline" : task.deadline) + "  | " + ranked.reason;
        g_ui->list->add(line.c_str());
        if (task.id == preserveId)
            selectedRow = static_cast<int>(i + 1);
    }
    if (selectedRow == 0 && !g_ui->rows.empty())
        selectedRow = 1;
    if (selectedRow != 0)
    {
        g_ui->list->value(selectedRow);
        const auto &task = g_ui->model.tasks()[g_ui->rows[static_cast<std::size_t>(selectedRow - 1)].index];
        showTask(task);
    }
    else
    {
        setDefaults();
    }
    g_ui->window->copy_label((std::string("Daybook | KernScript") + (g_ui->model.dirty() ? " *" : "")).c_str());
}

bool resolveEditor()
{
    if (editorValues() == g_ui->editorBaseline)
        return true;
    const int action = fl_choice("The task editor has unapplied changes.", "Cancel", "Discard",
                                 g_ui->selectedId ? "Update task" : "Add task");
    if (action == 0)
        return false;
    auto id = g_ui->selectedId;
    if (action == 2)
    {
        auto task = readEditor();
        if (id)
        {
            const auto *current = g_ui->model.findTask(id);
            if (!current)
                throw std::runtime_error("Selected task no longer exists");
            task.completed = current->completed;
            g_ui->model.edit(id, std::move(task));
        }
        else
            id = g_ui->model.add(std::move(task));
    }
    refresh(id);
    return true;
}

bool save(bool choosePath)
{
    if (!resolveEditor())
        return false;
    fs::path target = g_ui->model.file();
    if (choosePath || target.empty())
    {
        Fl_Native_File_Chooser chooser;
        chooser.title("Save Daybook");
        chooser.type(Fl_Native_File_Chooser::BROWSE_SAVE_FILE);
        chooser.options(Fl_Native_File_Chooser::SAVEAS_CONFIRM);
        chooser.filter("Daybook\t*.daybook");
        chooser.preset_file(target.empty() ? "tasks.daybook" : target.filename().string().c_str());
        if (chooser.show() != 0)
            return false;
        target = chooser.filename();
    }
    g_ui->model.save(target);
    refresh(g_ui->selectedId);
    return true;
}

bool resolveChanges()
{
    if (!resolveEditor())
        return false;
    if (!g_ui->model.dirty())
        return true;
    const int answer = fl_choice("Save your changes before continuing?", "Cancel", "Discard", "Save");
    if (answer == 0)
        return false;
    if (answer == 1)
        return true;
    return save(false);
}

void selected(Fl_Widget *, void *)
{
    const int row = g_ui->list->value() - 1;
    if (row < 0 || static_cast<std::size_t>(row) >= g_ui->rows.size())
    {
        return;
    }
    const auto wantedId = g_ui->model.tasks()[g_ui->rows[static_cast<std::size_t>(row)].index].id;
    bool proceed = false;
    reportFailure("Change task selection", [&] { proceed = resolveEditor(); });
    const auto id = proceed ? wantedId : g_ui->selectedId;
    for (std::size_t i = 0; i < g_ui->rows.size(); ++i)
        if (g_ui->model.tasks()[g_ui->rows[i].index].id == id)
        {
            g_ui->list->value(static_cast<int>(i + 1));
            if (proceed)
                showTask(g_ui->model.tasks()[g_ui->rows[i].index]);
            return;
        }
}

void addTask(Fl_Widget *, void *)
{
    reportFailure("Add task",
                  []
                  {
                      const uint64_t id = g_ui->model.add(readEditor());
                      refresh(id);
                  });
}

void updateTask(Fl_Widget *, void *)
{
    reportFailure("Update task",
                  []
                  {
                      if (g_ui->selectedId == 0)
                          return;
                      Task replacement = readEditor();
                      const Task *current = g_ui->model.findTask(g_ui->selectedId);
                      if (current == nullptr)
                          throw std::runtime_error("Selected task no longer exists");
                      replacement.completed = current->completed;
                      g_ui->model.edit(g_ui->selectedId, std::move(replacement));
                      refresh(g_ui->selectedId);
                  });
}

void deleteTask(Fl_Widget *, void *)
{
    reportFailure("Delete task",
                  []
                  {
                      const uint64_t id = g_ui->selectedId;
                      if (daybook::eraseAfterResolving(g_ui->model, id, resolveEditor))
                          refresh();
                  });
}

void toggleComplete(Fl_Widget *, void *)
{
    reportFailure("Change completion",
                  []
                  {
                      if (!resolveEditor())
                          return;
                      const Task *task = g_ui->model.findTask(g_ui->selectedId);
                      if (task != nullptr)
                          g_ui->model.complete(task->id, !task->completed);
                      refresh(g_ui->selectedId);
                  });
}

void saveTask(Fl_Widget *, void *)
{
    reportFailure("Save Daybook", [] { save(false); });
}

void saveAsTask(Fl_Widget *, void *)
{
    reportFailure("Save Daybook as", [] { save(true); });
}

void openTask(Fl_Widget *, void *)
{
    reportFailure("Open Daybook",
                  []
                  {
                      Fl_Native_File_Chooser chooser;
                      chooser.title("Open Daybook");
                      chooser.type(Fl_Native_File_Chooser::BROWSE_FILE);
                      chooser.filter("Daybook\t*.daybook");
                      if (chooser.show() != 0)
                          return;
                      Model candidate;
                      candidate.open(chooser.filename());
                      if (!resolveChanges())
                          return;
                      g_ui->model = std::move(candidate);
                      refresh();
                  });
}

void newTaskList(Fl_Widget *, void *)
{
    reportFailure("New Daybook",
                  []
                  {
                      if (!resolveChanges())
                          return;
                      g_ui->model = Model{};
                      refresh();
                  });
}

void reloadPolicy(Fl_Widget *, void *)
{
    reportFailure("Reload policy",
                  []
                  {
                      if (!resolveEditor())
                          return;
                      Fl_Native_File_Chooser chooser;
                      chooser.title("Reload ranking policy");
                      chooser.type(Fl_Native_File_Chooser::BROWSE_FILE);
                      chooser.filter("KernScript policy\t*.ks");
                      if (chooser.show() != 0)
                          return;
                      std::string error;
                      if (!g_ui->ranker.reload(chooser.filename(), g_ui->model, error))
                          throw std::runtime_error(error);
                      refresh(g_ui->selectedId);
                  });
}

void closeWindow(Fl_Widget *, void *)
{
    reportFailure("Close Daybook",
                  []
                  {
                      if (resolveChanges())
                          g_ui->window->hide();
                  });
}

int runUi(const fs::path &policyPath, const fs::path &preview)
{
    UI ui;
    g_ui = &ui;
    std::string error;
    if (!ui.ranker.reload(policyPath, ui.model, error))
    {
        if (!preview.empty())
            std::cerr << "Could not load ranking policy: " << error << '\n';
        else
            fl_alert("Could not load ranking policy: %s", error.c_str());
        return 1;
    }

    Fl_Window window(960, 610, "Daybook");
    ui.window = &window;
    window.callback(closeWindow);
    ui.list = new Fl_Browser(15, 25, 590, 515);
    ui.list->type(FL_HOLD_BROWSER);
    ui.list->callback(selected);
    constexpr int x = 625;
    ui.title = new Fl_Input(x, 50, 315, 28, "Title");
    ui.kind = new Fl_Input(x, 105, 150, 28, "Kind");
    ui.priority = new Fl_Input(x + 165, 105, 65, 28, "Priority");
    ui.minutes = new Fl_Input(x + 240, 105, 75, 28, "Minutes");
    ui.deadline = new Fl_Input(x, 160, 315, 28, "Deadline YYYY-MM-DD");
    ui.tags = new Fl_Input(x, 215, 315, 28, "Tags");
    for (auto *input : {ui.title, ui.kind, ui.priority, ui.minutes, ui.deadline, ui.tags})
        input->align(FL_ALIGN_TOP_LEFT);
    Fl_Button add(x, 270, 90, 30, "Add");
    add.callback(addTask);
    Fl_Button update(x + 100, 270, 90, 30, "Update");
    update.callback(updateTask);
    Fl_Button remove(x + 200, 270, 90, 30, "Delete");
    remove.callback(deleteTask);
    ui.complete = new Fl_Button(x, 315, 145, 30, "Mark complete");
    ui.complete->callback(toggleComplete);
    Fl_Button saveButton(x + 155, 315, 75, 30, "Save");
    saveButton.callback(saveTask);
    Fl_Button saveAs(x + 240, 315, 75, 30, "Save as");
    saveAs.callback(saveAsTask);
    Fl_Button open(x, 365, 75, 30, "Open");
    open.callback(openTask);
    Fl_Button fresh(x + 85, 365, 75, 30, "New list");
    fresh.callback(newTaskList);
    Fl_Button policy(x + 170, 365, 145, 30, "Choose policy");
    policy.callback(reloadPolicy);
    Fl_Button newTask(x, 415, 145, 30, "New task");
    newTask.callback(
        [](Fl_Widget *, void *)
        {
            reportFailure("New task",
                          []
                          {
                              if (resolveEditor())
                              {
                                  g_ui->list->value(0);
                                  setDefaults();
                              }
                          });
        });
    Fl_Button reloadActive(x + 155, 415, 160, 30, "Reload active policy");
    reloadActive.callback(
        [](Fl_Widget *, void *)
        {
            reportFailure("Reload policy",
                          []
                          {
                              if (!resolveEditor())
                                  return;
                              std::string error;
                              if (!g_ui->ranker.reload(g_ui->ranker.activePolicy(), g_ui->model, error))
                                  throw std::runtime_error(error);
                              refresh(g_ui->selectedId);
                          });
        });
    window.end();
    refresh();
    if (!preview.empty())
    {
        samples::renderPreview(window, preview);
        return 0;
    }
    window.show();
    return Fl::run();
}
} // namespace

int runCli(int argc, char **argv)
{
    fs::path policyPath = fs::absolute(argv[0]).parent_path() / "policies" / "urgent.ks";
    fs::path preview, testRoot = fs::current_path() / "build" / "test-tmp" / "Daybook";
    bool verifyMode = false;
    for (int i = 1; i < argc; ++i)
    {
        const std::string argument = argv[i];
        if (argument == "--verify")
            verifyMode = true;
        else if (argument == "--help")
        {
            std::cout << "Daybook [--verify] [--test-root DIR] [--policy FILE]\n";
            return 0;
        }
        else if (argument == "--test-root" && i + 1 < argc)
            testRoot = argv[++i];
        else if (argument == "--preview" && i + 1 < argc)
            preview = argv[++i];
        else if (argument == "--policy" && i + 1 < argc)
            policyPath = argv[++i];
        else
        {
            std::cerr << "Usage: Daybook [--verify] [--test-root DIR] [--policy FILE]\n";
            return 2;
        }
    }
    if (verifyMode)
        return verify(testRoot, policyPath) ? 0 : 1;
    return runUi(policyPath, preview);
}

int main(int argc, char **argv)
{
    try
    {
        return runCli(argc, argv);
    }
    catch (const std::exception &exception)
    {
        std::cerr << "Daybook: " << exception.what() << '\n';
        return 1;
    }
}
