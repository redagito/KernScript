#include "../../Support/Preview.h"
#include "Evaluator.h"
#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_File_Chooser.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Table_Row.H>
#include <FL/Fl_Window.H>
#include <FL/fl_ask.H>
#include <FL/fl_draw.H>
#include <iostream>

namespace fs = std::filesystem;
namespace
{
using namespace gridbook;
Workbook g_book;
std::unique_ptr<Evaluator> g_evaluator;
std::string g_path;
Address g_selected{1, 3};
Fl_Window *g_window = nullptr;
Fl_Input *g_formula = nullptr, *g_rows = nullptr, *g_columns = nullptr;
Fl_Box *g_status = nullptr;
Fl_Choice *g_template = nullptr;

class SheetTable : public Fl_Table_Row
{
public:
    SheetTable(int x, int y, int w, int h) : Fl_Table_Row(x, y, w, h)
    {
        row_header(1);
        col_header(1);
        row_height_all(28);
        col_width_all(112);
        row_header_width(45);
        col_header_height(26);
        col_resize(1);
        row_resize(1);
        end(); // Later controls belong to the window, outside the table's clipped group.
    }

protected:
    void draw_cell(TableContext context, int row, int col, int x, int y, int w, int h) override
    {
        if (context == CONTEXT_STARTPAGE)
        {
            fl_font(FL_HELVETICA, 14);
            return;
        }
        if (context == CONTEXT_COL_HEADER || context == CONTEXT_ROW_HEADER)
        {
            const auto label = context == CONTEXT_COL_HEADER ? columnName(col) : std::to_string(row + 1);
            fl_rectf(x, y, w, h, FL_LIGHT2);
            fl_color(FL_BLACK);
            fl_draw(label.c_str(), x, y, w, h, FL_ALIGN_CENTER);
            fl_rect(x, y, w, h, FL_GRAY);
            return;
        }
        if (context != CONTEXT_CELL || !g_book.data().contains({row, col}))
            return;
        const auto index = g_book.data().index({row, col});
        const auto result = g_evaluator->current()
                                ? g_evaluator->results().at(index)
                                : Result{false, 0,
                                         !g_book.data().cells[index].empty() && g_book.data().cells[index][0] == '='
                                             ? "#RECALC!"
                                             : g_book.data().cells[index],
                                         "Recalculation did not finish"};
        fl_push_clip(x, y, w, h);
        if (row == g_selected.row && col == g_selected.column)
            fl_rectf(x, y, w, h, 255, 248, 205);
        else if (!result.error.empty())
            fl_rectf(x, y, w, h, 255, 230, 230);
        else
            fl_rectf(x, y, w, h, FL_WHITE);
        fl_color(result.error.empty() ? FL_BLACK : FL_DARK_RED);
        fl_draw(result.display.c_str(), x + 5, y, w - 10, h, result.number ? FL_ALIGN_RIGHT : FL_ALIGN_LEFT);
        fl_rect(x, y, w, h, FL_LIGHT2);
        fl_pop_clip();
    }
};
SheetTable *g_table = nullptr;

void refresh(bool calculate = true)
{
    if (calculate)
        g_evaluator->recalculate(g_book.data());
    const auto &data = g_book.data();
    g_selected.row = std::clamp(g_selected.row, 0, data.rows - 1);
    g_selected.column = std::clamp(g_selected.column, 0, data.columns - 1);
    const int index = data.index(g_selected);
    g_formula->value(data.cells[index].c_str());
    g_rows->value(std::to_string(data.rows).c_str());
    g_columns->value(std::to_string(data.columns).c_str());
    g_table->rows(data.rows);
    g_table->cols(data.columns);
    g_table->redraw();
    const auto result =
        g_evaluator->current()
            ? g_evaluator->results()[index]
            : Result{false, 0, "#RECALC!", "Recalculation did not finish; edit the workbook or press Recalculate"};
    std::string status = name(g_selected) + (g_book.dirty() ? " | Unsaved changes" : " | Saved");
    if (!result.error.empty())
        status += " | " + result.error.substr(0, result.error.find('\n'));
    else
    {
        const auto &deps = g_evaluator->dependencies(index);
        if (!deps.empty())
        {
            status += " | Reads ";
            for (std::size_t i = 0; i < std::min<std::size_t>(deps.size(), 6); ++i)
                status += (i ? ", " : "") + name({deps[i] / data.columns, deps[i] % data.columns});
            if (deps.size() > 6)
                status += " and " + std::to_string(deps.size() - 6) + " more";
        }
    }
    g_status->copy_label(status.c_str());
    g_status->copy_tooltip(result.error.c_str());
    const auto title = "Gridbook | KernScript | " +
                       (g_path.empty() ? std::string("Untitled") : fs::path(g_path).filename().string()) +
                       (g_book.dirty() ? " *" : "");
    g_window->copy_label(title.c_str());
}

template <typename Work> void safely(Work work)
{
    try
    {
        work();
    }
    catch (const std::exception &error)
    {
        fl_alert("%s", error.what());
        if (g_window && g_formula && g_status && g_table && !g_evaluator->current())
            refresh(false);
    }
}

void commitPending()
{
    if (g_formula && g_book.data().cells[g_book.data().index(g_selected)] != g_formula->value())
    {
        g_book.set(g_selected, g_formula->value());
        g_evaluator->recalculate(g_book.data());
    }
}

bool save(bool saveAs = false)
{
    try
    {
        commitPending();
        std::string path = g_path;
        if (saveAs || path.empty())
        {
            const char *selected =
                fl_file_chooser("Save workbook", "*.gridbook", path.empty() ? "workbook.gridbook" : path.c_str());
            if (!selected)
                return false;
            path = selected;
            if (fs::path(path).extension() != ".gridbook")
                path += ".gridbook";
            if (fs::exists(path) && fl_choice("Replace the existing workbook?", "Cancel", "Replace", nullptr) != 1)
                return false;
        }
        g_book.save(path);
        g_path = path;
        refresh(false);
        return true;
    }
    catch (const std::exception &error)
    {
        fl_alert("Could not save workbook.\n%s", error.what());
        return false;
    }
}

bool confirmChanges()
{
    try
    {
        commitPending();
    }
    catch (const std::exception &error)
    {
        fl_alert("%s", error.what());
        return false;
    }
    if (!g_book.dirty())
        return true;
    const int action = fl_choice("Save changes to this workbook?", "Cancel", "Save", "Discard");
    return action == 2 || (action == 1 && save());
}

void setCell(Fl_Widget *, void *)
{
    safely(
        []
        {
            g_book.set(g_selected, g_formula->value());
            refresh();
        });
}
void selectCell(Fl_Widget *, void *)
{
    if (g_table->callback_context() != Fl_Table::CONTEXT_CELL)
        return;
    safely(
        []
        {
            commitPending();
            g_selected = {g_table->callback_row(), g_table->callback_col()};
            refresh(false);
        });
}
void open(Fl_Widget *, void *)
{
    safely(
        []
        {
            const char *selected = fl_file_chooser("Open workbook", "*.gridbook", "");
            if (!selected)
                return;
            const std::string path = selected;
            auto candidate = loadData(path);
            if (!confirmChanges())
                return;
            g_book.replace(std::move(candidate), true);
            g_path = path;
            refresh();
        });
}
void newWorkbook(Fl_Widget *, void *)
{
    safely(
        []
        {
            if (!confirmChanges())
                return;
            g_book.replace(g_template->value() == 1   ? budget()
                           : g_template->value() == 2 ? Data::blank(30, 10)
                                                      : invoice(),
                           false);
            g_path.clear();
            g_selected = {0, 0};
            refresh();
        });
}
int integer(const char *value)
{
    const std::string text(value);
    int result = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size())
        throw std::invalid_argument("Enter a whole number for rows and columns");
    return result;
}
void resize(Fl_Widget *, void *)
{
    safely(
        []
        {
            commitPending();
            g_book.resize(integer(g_rows->value()), integer(g_columns->value()));
            refresh();
        });
}
void undo(Fl_Widget *, void *)
{
    safely(
        []
        {
            commitPending();
            if (g_book.undo())
                refresh();
        });
}
void redo(Fl_Widget *, void *)
{
    safely(
        []
        {
            commitPending();
            if (g_book.redo())
                refresh();
        });
}
void reload(Fl_Widget *, void *)
{
    safely(
        []
        {
            commitPending();
            g_evaluator->reloadHelpers(g_book.data());
            refresh(false);
        });
}
void close(Fl_Widget *, void *)
{
    if (confirmChanges())
        g_window->hide();
}

void exportCsv(const fs::path &path, const Data &data, const std::vector<Result> &results)
{
    samples::writeAtomically(path,
                             [&](std::ostream &output)
                             {
                                 for (int row = 0; row < data.rows; ++row)
                                 {
                                     for (int col = 0; col < data.columns; ++col)
                                     {
                                         if (col)
                                             output << ',';
                                         output << '"';
                                         for (char c : results[data.index({row, col})].display)
                                         {
                                             if (c == '"')
                                                 output << '"';
                                             output << c;
                                         }
                                         output << '"';
                                     }
                                     output << "\r\n";
                                 }
                             });
}
void exportFile(Fl_Widget *, void *)
{
    safely(
        []
        {
            commitPending();
            if (!g_evaluator->current())
                g_evaluator->recalculate(g_book.data());
            const char *chosen = fl_file_chooser("Export displayed values as CSV", "*.csv", "values.csv");
            if (!chosen)
                return;
            fs::path path(chosen);
            std::error_code error;
            if (!g_path.empty() &&
                (fs::weakly_canonical(path) == fs::weakly_canonical(g_path) || fs::equivalent(path, g_path, error)))
                throw std::runtime_error("Choose another file so the workbook keeps its formulas");
            if (fs::exists(path) && fl_choice("Replace the existing CSV file?", "Cancel", "Replace", nullptr) != 1)
                return;
            exportCsv(path, g_book.data(), g_evaluator->results());
        });
}

int verify(const std::vector<std::string> &paths, const fs::path &root)
{
    auto require = [](bool condition, const std::string &message)
    {
        if (!condition)
            throw std::runtime_error(message);
    };
    auto value = [](const Evaluator &eval, const Data &data, const std::string &cell) -> const Result &
    { return eval.results().at(data.index(*address(cell))); };
    const auto directory = samples::testDirectory(root);
    const auto atomic = directory / "atomic.txt";
    samples::writeAtomically(atomic, [](std::ostream &out) { out << "original"; });
    samples::writeAtomically(atomic, [](std::ostream &out) { out << "replacement"; });
    require(samples::readText(atomic, 64) == "replacement", "Atomic overwrite did not replace existing output");
    bool writeFailed = false;
    try
    {
        samples::writeAtomically(atomic,
                                 [](std::ostream &out)
                                 {
                                     out << "partial";
                                     throw std::runtime_error("test failure");
                                 });
    }
    catch (const std::exception &)
    {
        writeFailed = true;
    }
    require(writeFailed && samples::readText(atomic, 64) == "replacement", "Failed write damaged existing output");
    Workbook book;
    Evaluator eval(paths);
    eval.recalculate(book.data());
    require(value(eval, book.data(), "D4").value == 26, "Invoice total is incorrect");
    const auto compiled = eval.compilations;
    book.set(*address("B2"), "5");
    eval.recalculate(book.data());
    require(value(eval, book.data(), "D4").value == 30, "Edited invoice did not recalculate");
    require(eval.compilations == compiled && eval.recalculated == 3,
            "Unchanged formulas were recompiled or unrelated cells recalculated");
    book.set(*address("A10"), "=B10+1");
    book.set(*address("B10"), "=A10+1");
    book.set(*address("C10"), "=A10+1");
    eval.recalculate(book.data());
    require(value(eval, book.data(), "A10").display == "#CYCLE!" &&
                value(eval, book.data(), "B10").display == "#CYCLE!" &&
                value(eval, book.data(), "C10").display == "#DEPEND!",
            "Cycle membership or propagation is incorrect");
    book.set(*address("A10"), "1");
    eval.recalculate(book.data());
    require(value(eval, book.data(), "B10").value == 2 && value(eval, book.data(), "C10").value == 2,
            "Breaking a cycle did not restore dependent formulas");
    book.set(*address("A11"), "=1/0");
    book.set(*address("B11"), "=A11+1");
    eval.recalculate(book.data());
    require(!value(eval, book.data(), "A11").error.empty() && value(eval, book.data(), "B11").display == "#DEPEND!",
            "Runtime error was not propagated");
    book.set(*address("A11"), "=1+");
    eval.recalculate(book.data());
    require(value(eval, book.data(), "A11").display == "#FORMULA!", "Syntax error was not reported");
    book.resize(40, 30);
    book.set(*address("AA12"), "10");
    book.set(*address("AB12"), "=aa12/4");
    eval.recalculate(book.data());
    require(value(eval, book.data(), "AB12").value == 2.5, "Multi-letter column reference or floating division failed");
    book.set(*address("AC12"), "=MAX(AA12:AB12)+MIN(AA12:AB12)+AVG(AA12:AB12)+COUNT(AA12:AB12)");
    book.set(*address("AD12"), "=1e2+1/2");
    book.set(*address("A15"), "=1 // A15 is a comment, not a dependency");
    eval.recalculate(book.data());
    require(value(eval, book.data(), "AC12").value == 20.75 && value(eval, book.data(), "AD12").value == 100.5,
            "Aggregate or exponent formula failed");
    require(value(eval, book.data(), "A15").value == 1 && value(eval, book.data(), "A15").error.empty(),
            "Formula comment introduced a false dependency");
    book.set(*address("A16"), "-4");
    book.set(*address("B16"), "text");
    book.set(*address("D16"), "=MIN(A16:C16)+MAX(A16:C16)+COUNT(A16:C16)");
    book.set(*address("E16"), "=AVG(B16:C16)");
    book.set(*address("A17"), "=\"A17\"");
    eval.recalculate(book.data());
    require(value(eval, book.data(), "D16").value == -7 && value(eval, book.data(), "E16").value == 0,
            "Aggregate handling of negative, empty, or text cells failed");
    require(value(eval, book.data(), "A17").display == "#VALUE!", "String contents became a false cell dependency");
    book.set(*address("A14"), "Notes with \"quotes\", tabs\tand \\ paths");
    const auto saved = directory / "invoice.gridbook";
    book.save(saved);
    require(!book.dirty(), "Save did not clear modified state");
    Workbook restored;
    restored.open(saved);
    require(restored.data() == book.data(), "Workbook round trip changed formulas, dimensions, or text");
    book.set(*address("B2"), "6");
    require(book.dirty() && book.undo() && !book.dirty(), "Undo did not restore saved revision");
    require(book.redo() && book.dirty(), "Redo did not restore edit");
    const auto original = restored.data();
    const auto invalid = directory / "invalid.gridbook";
    for (const auto &contents : {"Gridbook 1\n-1 3\n", "Gridbook 1\n3 3\nA1 \"first\"\nA1 \"second\"\n",
                                 "Gridbook 1\n3 3\nZ9 \"outside\"\n", "Gridbook 1\n3 3\nA1 \"bad\" trailing\n"})
    {
        samples::writeAtomically(invalid, [&](std::ostream &out) { out << contents; });
        bool rejected = false;
        try
        {
            restored.open(invalid);
        }
        catch (const std::exception &)
        {
            rejected = true;
        }
        require(rejected && restored.data() == original, "Invalid workbook replaced current work");
    }
    bool rejected = false;
    try
    {
        book.resize(10, 10);
    }
    catch (const std::exception &)
    {
        rejected = true;
    }
    require(rejected, "Shrinking removed populated cells without rejection");

    const auto helperDirectory = directory / "scripts";
    fs::create_directory(helperDirectory);
    const auto helper = samples::readText(fs::path(paths.front()) / "sheet.ks", 64 * 1024);
    samples::writeAtomically(helperDirectory / "sheet.ks", [&](std::ostream &out) { out << helper; });
    Evaluator live({helperDirectory.string()});
    Data simple = Data::blank(4, 4);
    simple.cells[0] = "8";
    simple.cells[1] = "=sheet::percent(A1,50)";
    live.recalculate(simple);
    require(live.results()[1].value == 4, "Custom helper did not execute");
    auto replacement = helper;
    const auto percent = replacement.find("return amount * rate / 100.0;");
    require(percent != std::string::npos, "Bundled percent helper is missing");
    replacement.replace(percent, std::string("return amount * rate / 100.0;").size(), "return amount * rate / 50.0;");
    samples::writeAtomically(helperDirectory / "sheet.ks", [&](std::ostream &out) { out << replacement; });
    live.reloadHelpers(simple);
    require(live.results()[1].value == 8, "A valid helper reload did not update formulas");
    samples::writeAtomically(helperDirectory / "sheet.ks",
                             [](std::ostream &out) { out << "module sheet; export func bad( {"; });
    rejected = false;
    try
    {
        live.reloadHelpers(simple);
    }
    catch (const std::exception &)
    {
        rejected = true;
    }
    simple.cells[0] = "10";
    live.recalculate(simple);
    require(rejected && live.results()[1].value == 10, "Failed helper reload replaced working formulas");

    Data large = Data::blank(502, 6);
    for (int row = 1; row <= 500; ++row)
    {
        large.cells[large.index({row, 0})] = "2";
        large.cells[large.index({row, 1})] = "3";
        large.cells[large.index({row, 2})] = "=A" + std::to_string(row + 1) + "*B" + std::to_string(row + 1);
        large.cells[large.index({row, 3})] =
            row == 1 ? "=C2" : "=D" + std::to_string(row) + "+C" + std::to_string(row + 1);
    }
    large.cells[large.index({501, 4})] = "=SUM(C2:C501)";
    Evaluator workload(paths);
    workload.recalculate(large);
    require(value(workload, large, "D501").value == 3000 && value(workload, large, "E502").value == 3000,
            "Large invoice workload failed");
    const auto before = workload.compilations;
    large.cells[large.index({500, 0})] = "4";
    workload.recalculate(large);
    require(value(workload, large, "E502").value == 3006 && workload.compilations == before &&
                workload.recalculated == 3,
            "Incremental large workbook recalculation failed");
    exportCsv(directory / "invoice.csv", large, workload.results());
    require(samples::readText(directory / "invoice.csv", kMaxFile).find("3006") != std::string::npos,
            "CSV export omitted calculated values");
    const auto budgetDirectory = directory / "budget-scripts";
    fs::create_directory(budgetDirectory);
    samples::writeAtomically(budgetDirectory / "sheet.ks", [](std::ostream &out)
                             { out << "module sheet; export func sum(id) -> value { return 0.0; }\n"; });
    // All referenced cells are empty. A zero-valued aggregate keeps this
    // regression focused on graph recovery rather than repeating range math.
    Data graph = Data::blank(1000, 10);
    for (int row = 0; row < 111; ++row)
        graph.cells[graph.index({row, 9})] = "=SUM(A1:I1000)";
    graph.cells[graph.index({111, 9})] = "7";
    Evaluator bounded({budgetDirectory.string()});
    bounded.recalculate(graph);
    require(bounded.current() && bounded.results()[graph.index({111, 9})].value == 7, "Graph-limit fixture failed");
    graph.cells[graph.index({111, 9})] = "=SUM(A1:I1000)";
    rejected = false;
    try
    {
        bounded.recalculate(graph);
    }
    catch (const std::exception &)
    {
        rejected = true;
    }
    require(rejected && !bounded.current(), "Graph limit failed to invalidate displayed results");
    graph.cells[graph.index({0, 9})].clear();
    bounded.recalculate(graph);
    require(bounded.current() && bounded.results()[graph.index({111, 9})].value == 0,
            "Graph recovery reused the stale literal result of a new formula");
    std::cout
        << "Gridbook verification passed: persistence, formulas, ranges, graph recovery, reload, and 1001 formulas\n";
    return 0;
}
} // namespace

int main(int argc, char **argv)
{
    try
    {
        bool headless = false;
        fs::path preview, testRoot = "build/test-tmp/Gridbook";
        for (int i = 1; i < argc; ++i)
        {
            const std::string argument = argv[i];
            if (argument == "--verify" || argument == "--verify-headless")
                headless = true;
            else if (argument == "--test-root" && i + 1 < argc)
                testRoot = argv[++i];
            else if (argument == "--preview" && i + 1 < argc)
                preview = argv[++i];
            else if (argument == "--help")
            {
                std::cout << "Gridbook: editable formula workbook.\nGridbook --verify [--test-root DIR] runs without a "
                             "window.\n";
                return 0;
            }
            else
                throw std::invalid_argument("Unknown argument: " + argument);
        }
        const auto exe = fs::absolute(argv[0]).parent_path();
        std::vector<std::string> paths;
        for (const auto &candidate :
             {exe / "scripts", fs::current_path() / "Samples/Gridbook/scripts", fs::current_path() / "scripts"})
            if (fs::is_regular_file(candidate / "sheet.ks"))
                paths.push_back(candidate.string());
        if (paths.empty())
            throw std::runtime_error("Cannot find scripts/sheet.ks beside Gridbook or in the project");
        if (headless)
            return verify(paths, testRoot);
        g_evaluator = std::make_unique<Evaluator>(paths);
        Fl_Window window(1040, 650, "Gridbook | KernScript");
        g_window = &window;
        window.callback(close);
        Fl_Button openButton(12, 12, 70, 28, "Open");
        openButton.callback(open);
        Fl_Button saveButton(90, 12, 70, 28, "Save");
        saveButton.callback([](Fl_Widget *, void *) { save(); });
        Fl_Button saveAsButton(168, 12, 80, 28, "Save As");
        saveAsButton.callback([](Fl_Widget *, void *) { save(true); });
        Fl_Button csvButton(256, 12, 90, 28, "Export CSV");
        csvButton.callback(exportFile);
        Fl_Choice templateChoice(370, 12, 140, 28);
        g_template = &templateChoice;
        templateChoice.add("Invoice|Budget|Blank");
        templateChoice.value(0);
        Fl_Button newButton(518, 12, 65, 28, "New");
        newButton.callback(newWorkbook);
        Fl_Button undoButton(600, 12, 65, 28, "Undo");
        undoButton.callback(undo);
        Fl_Button redoButton(673, 12, 65, 28, "Redo");
        redoButton.callback(redo);
        Fl_Button reloadButton(750, 12, 130, 28, "Reload helpers");
        reloadButton.callback(reload);
        Fl_Button recalculateButton(890, 12, 138, 28, "Recalculate");
        recalculateButton.callback([](Fl_Widget *, void *) { safely([] { refresh(); }); });
        SheetTable table(12, 55, 1016, 455);
        g_table = &table;
        table.callback(selectCell);
        Fl_Input formula(70, 525, 855, 30, "Cell");
        g_formula = &formula;
        formula.when(FL_WHEN_ENTER_KEY_ALWAYS);
        formula.callback(setCell);
        Fl_Button setButton(935, 525, 90, 30, "Set");
        setButton.callback(setCell);
        Fl_Input rows(70, 570, 70, 28, "Rows");
        g_rows = &rows;
        Fl_Input columns(235, 570, 70, 28, "Columns");
        g_columns = &columns;
        Fl_Button resizeButton(320, 570, 110, 28, "Resize sheet");
        resizeButton.callback(resize);
        Fl_Box status(12, 610, 1016, 28);
        g_status = &status;
        status.align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
        window.end();
        window.resizable(table);
        window.size_range(800, 550);
        refresh();
        if (!preview.empty())
        {
            if (formula.parent() != &window || rows.parent() != &window || status.parent() != &window)
                throw std::runtime_error("Workbook controls must be outside the table group");
            samples::renderPreview(window, preview);
            return 0;
        }
        window.show();
        return Fl::run();
    }
    catch (const std::exception &error)
    {
        std::cerr << "Gridbook: " << error.what() << '\n';
        return 1;
    }
}
