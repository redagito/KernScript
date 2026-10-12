#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Table_Row.H>
#include <FL/Fl_Window.H>
#include <FL/fl_draw.H>

#include <kern/Kern.h>

namespace
{

constexpr const char *kWindowTitle = "KernScript Sheet";
constexpr int kCols = 5;
constexpr int kRows = 6;

// Invoice-style seed: quantities times prices, a total row, one plain
// string cell (E2 "fresh"), one runtime error (B5 divides by zero) and
// one syntax error (E4 ends with a dangling "+"). E5/E6 reference each
// other so the startup grid also demonstrates cycle detection.
const char *kSeed[kRows][kCols] = {
    {"Item", "Qty", "Price", "Total", "Note"}, {"Apples", "3", "2", "=B2*C2", "fresh"},
    {"Pears", "4", "5", "=B3*C3", ""},         {"Sum", "", "", "=D2+D3", "=B1+"},
    {"Bad", "=1/0", "", "", "=E6+1"},          {"Loop", "", "", "", "=E5+1"},
};

const char *kExpectedStart[kRows][kCols] = {
    {"Item", "Qty", "Price", "Total", "Note"}, {"Apples", "3", "2", "6", "fresh"},    {"Pears", "4", "5", "20", ""},
    {"Sum", "", "", "26", "!ERR 5:16"},        {"Bad", "!ERR run", "", "", "!CYCLE"}, {"Loop", "", "", "", "!CYCLE"},
};

constexpr int kExpectedLoads = 7;
constexpr int kExpectedCalls = 27;

struct Cell
{
    std::string text;
    std::string shown;
    double value = 0.0;
    bool isNumber = false;
};

std::vector<std::vector<Cell>> g_cells;
int g_selRow = 1;
int g_selCol = 3;
int g_loadCount = 0;
int g_callCount = 0;
double g_emitValue = 0.0;
bool g_emitFired = false;
kern::IVirtualMachine *g_vm = nullptr;

Fl_Window *g_window = nullptr;
Fl_Input *g_formulaBar = nullptr;
Fl_Box *g_status = nullptr;
Fl_Button *g_setButton = nullptr;
Fl_Button *g_recalcButton = nullptr;
Fl_Button *g_resetButton = nullptr;

class SheetTable : public Fl_Table_Row
{
public:
    SheetTable(int x, int y, int w, int h) : Fl_Table_Row(x, y, w, h)
    {
        rows(kRows);
        cols(kCols);
        row_header(1);
        col_header(1);
        row_height_all(28);
        col_width_all(96);
        row_header_width(36);
        col_header_height(26);
        color(FL_WHITE);
    }

protected:
    void draw_cell(TableContext context, int row, int col, int x, int y, int w, int h) override
    {
        switch (context)
        {
        case CONTEXT_STARTPAGE:
            fl_font(FL_HELVETICA, 14);
            return;
        case CONTEXT_COL_HEADER:
        {
            const char label[2] = {static_cast<char>('A' + col), '\0'};
            fl_rectf(x, y, w, h, FL_GRAY);
            fl_color(FL_BLACK);
            fl_draw(label, x, y, w, h, FL_ALIGN_CENTER);
            fl_rect(x, y, w, h, FL_GRAY);
            return;
        }
        case CONTEXT_ROW_HEADER:
        {
            const std::string label = std::to_string(row + 1);
            fl_rectf(x, y, w, h, FL_GRAY);
            fl_color(FL_BLACK);
            fl_draw(label.c_str(), x, y, w, h, FL_ALIGN_CENTER);
            fl_rect(x, y, w, h, FL_GRAY);
            return;
        }
        case CONTEXT_CELL:
        {
            if (row == g_selRow && col == g_selCol)
            {
                fl_rectf(x, y, w, h, 255, 255, 200);
            }
            else
            {
                fl_rectf(x, y, w, h, FL_WHITE);
            }
            fl_color(FL_BLACK);
            fl_draw(g_cells[row][col].shown.c_str(), x + 4, y, w - 8, h, FL_ALIGN_LEFT);
            fl_rect(x, y, w, h, FL_GRAY);
            return;
        }
        case CONTEXT_TABLE:
        case CONTEXT_ENDPAGE:
        case CONTEXT_RC_RESIZE:
        case CONTEXT_NONE:
            return;
        }
    }
};

SheetTable *g_table = nullptr;

// Captures compiler diagnostics so a failing formula can show the real
// file:line:col position instead of a swallowed generic error.
class DiagLogger : public kern::ILogger
{
public:
    const std::vector<std::string> &messages() const { return m_messages; }

protected:
    void onError(const std::string &message) override { m_messages.push_back(message); }

private:
    std::vector<std::string> m_messages;
};

std::string cellName(int row, int col)
{
    std::string name;
    name += static_cast<char>('A' + col);
    name += static_cast<char>('1' + row);
    return name;
}

bool inGrid(int row, int col) { return row >= 0 && row < kRows && col >= 0 && col < kCols; }

// Host rule for cell("A1"): a numeric cell yields its value, anything else
// (empty, plain string, !ERR, !CYCLE or not yet computed) reads as 0. The
// VM cannot decide this; the host owns the sheet, the script only computes.
double cell_value(std::string name)
{
    if (name.size() == 2 && name[0] >= 'A' && name[0] <= 'E' && name[1] >= '1' && name[1] <= '6')
    {
        const Cell &cell = g_cells[name[1] - '1'][name[0] - 'A'];
        if (cell.isNumber)
        {
            return cell.value;
        }
    }
    return 0.0;
}

void emit_value(double value)
{
    g_emitValue = value;
    g_emitFired = true;
}

// Scans a formula for A1-style references, skipping "..." string literals
// so ="A1" stays a string instead of becoming a cell lookup. A reference
// needs identifier boundaries on both sides, so XA11 or A12 never match.
std::vector<std::string> findRefs(const std::string &expr)
{
    std::vector<std::string> refs;
    bool inString = false;
    for (std::size_t i = 0; i < expr.size();)
    {
        const char c = expr[i];
        if (inString)
        {
            if (c == '"')
            {
                inString = false;
            }
            ++i;
            continue;
        }
        if (c == '"')
        {
            inString = true;
            ++i;
            continue;
        }
        if (c >= 'A' && c <= 'E' && i + 1 < expr.size() && expr[i + 1] >= '1' && expr[i + 1] <= '6')
        {
            const bool headOk =
                i == 0 || (!std::isalnum(static_cast<unsigned char>(expr[i - 1])) && expr[i - 1] != '_');
            const bool tailOk =
                i + 2 >= expr.size() || (!std::isalnum(static_cast<unsigned char>(expr[i + 2])) && expr[i + 2] != '_');
            if (headOk && tailOk)
            {
                refs.push_back(expr.substr(i, 2));
                i += 2;
                continue;
            }
        }
        ++i;
    }
    return refs;
}

// Rewrites in-grid references into temp vars with the same scan, so the
// dependency graph and the compiled source always agree. Temps are needed
// because of the VM extern-call discipline (doc/Scripting Language.md):
// a valued cell() call leaves its return on the stack, and that value
// counts toward the next call's arguments, so each lookup is consumed by
// a var before the next one runs. =B2*C2 would otherwise fail with
// "expects 1 arguments, but 2 were pushed".
std::string rewriteWithTemps(const std::string &expr, const std::vector<std::string> &lookups, const std::string &stem)
{
    std::string out;
    bool inString = false;
    for (std::size_t i = 0; i < expr.size();)
    {
        const char c = expr[i];
        if (inString)
        {
            out += c;
            if (c == '"')
            {
                inString = false;
            }
            ++i;
            continue;
        }
        if (c == '"')
        {
            inString = true;
            out += c;
            ++i;
            continue;
        }
        if (c >= 'A' && c <= 'E' && i + 1 < expr.size() && expr[i + 1] >= '1' && expr[i + 1] <= '6')
        {
            const bool headOk =
                i == 0 || (!std::isalnum(static_cast<unsigned char>(expr[i - 1])) && expr[i - 1] != '_');
            const bool tailOk =
                i + 2 >= expr.size() || (!std::isalnum(static_cast<unsigned char>(expr[i + 2])) && expr[i + 2] != '_');
            if (headOk && tailOk)
            {
                const std::string ref = expr.substr(i, 2);
                for (std::size_t k = 0; k < lookups.size(); ++k)
                {
                    if (lookups[k] == ref)
                    {
                        out += stem + std::to_string(k);
                        break;
                    }
                }
                i += 2;
                continue;
            }
        }
        out += c;
        ++i;
    }
    return out;
}

struct Job
{
    int row = 0;
    int col = 0;
    std::string name;
    std::string body;
    std::vector<std::string> lookups;
    std::string stem;
};

Job makeJob(int row, int col, const std::string &expr)
{
    Job job;
    job.row = row;
    job.col = col;
    job.name = cellName(row, col);
    for (const std::string &ref : findRefs(expr))
    {
        bool known = false;
        for (const std::string &seen : job.lookups)
        {
            if (seen == ref)
            {
                known = true;
                break;
            }
        }
        if (!known)
        {
            job.lookups.push_back(ref);
        }
    }
    job.stem = "__cell";
    while (expr.find(job.stem) != std::string::npos)
    {
        job.stem += "_";
    }
    job.body = rewriteWithTemps(expr, job.lookups, job.stem);
    return job;
}

// One script func per formula: the lookups run first (each consumed by
// its var), then emit() reports the value as a statement. cell() stays
// value-use and emit() stays statement-use in every func, per the extern
// consistency rule. The body holds no extern declarations so any number
// of funcs can share one module (repeating an extern is a duplicate).
std::string funcBody(const Job &job, const std::string &funcName)
{
    std::string source = "func " + funcName + "() {\n";
    for (std::size_t i = 0; i < job.lookups.size(); ++i)
    {
        source += "  var " + job.stem + std::to_string(i) + " = cell(\"" + job.lookups[i] + "\");\n";
    }
    source += "  emit(" + job.body + ");\n}\n";
    return source;
}

std::string funcSource(const Job &job, const std::string &funcName)
{
    return "extern cell(name);\nextern emit(value);\n" + funcBody(job, funcName);
}

// Turns the first real diagnostic ("<file>:<line>:<col>: error: ...")
// into the in-cell marker. Anything unparseable is reported on stderr
// and shown as !ERR 0:0, never silently dropped.
std::string diagToErr(const std::string &cell, const std::string &message)
{
    const std::string prefix = cell + ":";
    if (message.compare(0, prefix.size(), prefix) == 0)
    {
        const char *rest = message.c_str() + prefix.size();
        char *end = nullptr;
        const long line = std::strtol(rest, &end, 10);
        if (end != rest && *end == ':')
        {
            rest = end + 1;
            const long col = std::strtol(rest, &end, 10);
            if (end != rest && *end == ':')
            {
                return "!ERR " + std::to_string(line) + ":" + std::to_string(col);
            }
        }
    }
    std::cerr << "unparsed diagnostic: " << message << '\n';
    return "!ERR 0:0";
}

bool compileOne(const Job &job, std::string &errText)
{
    DiagLogger logger;
    std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler(&logger));
    std::istringstream input(funcSource(job, "calc"));
    std::stringstream asmCode;
    if (compiler->compile(input, asmCode, job.name))
    {
        return true;
    }
    errText = "!ERR 0:0";
    if (!logger.messages().empty())
    {
        errText = diagToErr(job.name, logger.messages()[0]);
        std::cout << "compile error in " << job.name << ": " << logger.messages()[0] << '\n';
    }
    return false;
}

bool parseInteger(const std::string &text, double &out)
{
    if (text.empty())
    {
        return false;
    }
    errno = 0;
    char *end = nullptr;
    const long long value = std::strtoll(text.c_str(), &end, 10);
    if (errno != 0 || end == nullptr || *end != '\0')
    {
        return false;
    }
    out = static_cast<double>(value);
    return true;
}

bool parseFloat(const std::string &text, double &out)
{
    if (text.empty())
    {
        return false;
    }
    errno = 0;
    char *end = nullptr;
    const double value = std::strtod(text.c_str(), &end);
    if (errno != 0 || end == nullptr || *end != '\0')
    {
        return false;
    }
    out = value;
    return true;
}

// Plain cells try int, then float, then stay a string. The display keeps
// the raw text; only the numeric flag and value feed cell().
void parseLiteral(Cell &cell)
{
    double number = 0.0;
    if (parseInteger(cell.text, number) || parseFloat(cell.text, number))
    {
        cell.value = number;
        cell.isNumber = true;
    }
    else
    {
        cell.value = 0.0;
        cell.isNumber = false;
    }
    cell.shown = cell.text;
}

std::string formatNumber(double value)
{
    if (std::isfinite(value))
    {
        double whole = 0.0;
        if (std::modf(value, &whole) == 0.0 && std::fabs(whole) < 9007199254740992.0)
        {
            return std::to_string(static_cast<long long>(whole));
        }
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "%.6g", value);
        return std::string(buffer);
    }
    return "!ERR run";
}

bool recomputeAll()
{
    for (int row = 0; row < kRows; ++row)
    {
        for (int col = 0; col < kCols; ++col)
        {
            Cell &cell = g_cells[row][col];
            if (cell.text.empty() || cell.text[0] != '=')
            {
                parseLiteral(cell);
            }
            else
            {
                cell.value = 0.0;
                cell.isNumber = false;
                cell.shown = "!ERR run";
            }
        }
    }

    std::vector<Job> jobs;
    int index[kRows][kCols];
    for (int row = 0; row < kRows; ++row)
    {
        for (int col = 0; col < kCols; ++col)
        {
            index[row][col] = -1;
        }
    }
    for (int row = 0; row < kRows; ++row)
    {
        for (int col = 0; col < kCols; ++col)
        {
            const Cell &cell = g_cells[row][col];
            if (!cell.text.empty() && cell.text[0] == '=')
            {
                index[row][col] = static_cast<int>(jobs.size());
                jobs.push_back(makeJob(row, col, cell.text.substr(1)));
            }
        }
    }

    // Host-side topological order over formula cells. Plain cells are
    // always available; only formula-to-formula edges constrain the order.
    // Whatever Kahn leaves behind is circular, so it shows !CYCLE. The VM
    // cannot detect this: each func call runs in isolation.
    std::vector<std::vector<int>> dependents(jobs.size());
    std::vector<int> pending(jobs.size(), 0);
    for (std::size_t i = 0; i < jobs.size(); ++i)
    {
        std::vector<std::string> deps = jobs[i].lookups;
        std::sort(deps.begin(), deps.end());
        deps.erase(std::unique(deps.begin(), deps.end()), deps.end());
        for (const std::string &dep : deps)
        {
            const int depRow = dep[1] - '1';
            const int depCol = dep[0] - 'A';
            if (inGrid(depRow, depCol) && index[depRow][depCol] >= 0)
            {
                dependents[index[depRow][depCol]].push_back(static_cast<int>(i));
                ++pending[i];
            }
        }
    }
    std::vector<int> queue;
    for (std::size_t i = 0; i < jobs.size(); ++i)
    {
        if (pending[i] == 0)
        {
            queue.push_back(static_cast<int>(i));
        }
    }
    std::vector<int> order;
    std::size_t head = 0;
    while (head < queue.size())
    {
        const int job = queue[head++];
        order.push_back(job);
        for (int next : dependents[job])
        {
            if (--pending[next] == 0)
            {
                queue.push_back(next);
            }
        }
    }
    std::vector<bool> inOrder(jobs.size(), false);
    for (int job : order)
    {
        inOrder[job] = true;
    }
    for (std::size_t i = 0; i < jobs.size(); ++i)
    {
        if (!inOrder[i])
        {
            Cell &cell = g_cells[jobs[i].row][jobs[i].col];
            cell.shown = "!CYCLE";
            cell.isNumber = false;
            cell.value = 0.0;
        }
    }

    // Per-cell compile first so failures map to the real file:line:col
    // diagnostic of that cell's own source (source name = cell name).
    std::vector<int> runnable;
    for (int job : order)
    {
        std::string errText;
        if (compileOne(jobs[job], errText))
        {
            runnable.push_back(job);
        }
        else
        {
            Cell &cell = g_cells[jobs[job].row][jobs[job].col];
            cell.shown = errText;
            cell.isNumber = false;
            cell.value = 0.0;
            std::cout << "compile error in " << jobs[job].name << ": " << errText << '\n';
        }
    }
    if (runnable.empty())
    {
        return true;
    }

    // Every formula is its own script func sharing one module, so one
    // load runs them all in dependency order. cell() stays value-use and
    // emit() stays statement-use in every func, per the extern rule.
    std::string combined = "extern cell(name);\nextern emit(value);\n";
    for (int job : runnable)
    {
        combined += funcBody(jobs[job], "calc_" + jobs[job].name);
    }
    std::stringstream asmCode;
    {
        DiagLogger logger;
        std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler(&logger));
        std::istringstream input(combined);
        if (!compiler->compile(input, asmCode, "sheet"))
        {
            std::cerr << "combined compile failed (per-cell compiles passed):\n";
            for (const std::string &message : logger.messages())
            {
                std::cerr << message << '\n';
            }
            for (int job : runnable)
            {
                Cell &cell = g_cells[jobs[job].row][jobs[job].col];
                cell.shown = "!ERR run";
                cell.isNumber = false;
            }
            return false;
        }
    }
    std::stringstream byteCode;
    {
        std::unique_ptr<kern::IAssembler> assembler(kern::createAssembler());
        std::istringstream input(asmCode.str());
        if (!assembler->assemble(input, byteCode))
        {
            std::cerr << "assemble failed for the sheet module\n";
            return false;
        }
    }
    try
    {
        if (!g_vm->load(byteCode))
        {
            std::cerr << "load failed for the sheet module\n";
            return false;
        }
    }
    catch (const kern::RuntimeError &error)
    {
        std::cerr << "load error: " << error.what() << '\n';
        return false;
    }
    ++g_loadCount;

    for (int job : runnable)
    {
        Cell &cell = g_cells[jobs[job].row][jobs[job].col];
        g_emitFired = false;
        g_emitValue = 0.0;
        try
        {
            if (!g_vm->callFunction("calc_" + jobs[job].name))
            {
                std::cerr << "function calc_" << jobs[job].name << " not found\n";
                cell.shown = "!ERR run";
                continue;
            }
        }
        catch (const kern::RuntimeError &error)
        {
            std::cout << "runtime error in " << jobs[job].name << ": " << error.what() << '\n';
            cell.shown = "!ERR run";
            cell.isNumber = false;
            cell.value = 0.0;
            continue;
        }
        ++g_callCount;
        if (!g_emitFired)
        {
            std::cout << "missing emit in " << jobs[job].name << '\n';
            cell.shown = "!ERR run";
            cell.isNumber = false;
            cell.value = 0.0;
            continue;
        }
        cell.value = g_emitValue;
        cell.isNumber = true;
        cell.shown = formatNumber(g_emitValue);
    }
    return true;
}

void refreshUi()
{
    g_formulaBar->value(g_cells[g_selRow][g_selCol].text.c_str());
    const std::string text = cellName(g_selRow, g_selCol) + std::string(" | loads ") + std::to_string(g_loadCount) +
                             " | calls " + std::to_string(g_callCount);
    g_status->copy_label(text.c_str());
    g_table->redraw();
}

// Shared by the click handler and the verify harness: a click and the
// harness both end up here, so the edit path below stays identical.
void selectCell(int row, int col)
{
    if (!inGrid(row, col))
    {
        return;
    }
    g_selRow = row;
    g_selCol = col;
    g_table->select_row(row, 1);
    refreshUi();
}

// Clicks land here through the table widget itself: Fl_Table_Row tracks
// the push, then runs this callback with the cell that was hit.
void onTableSelect(Fl_Widget *widget, void * /*data*/)
{
    auto *table = static_cast<SheetTable *>(widget);
    if (table->callback_context() == Fl_Table::CONTEXT_CELL)
    {
        selectCell(table->callback_row(), table->callback_col());
    }
}

void resetSheet()
{
    g_cells.assign(kRows, std::vector<Cell>(kCols));
    for (int row = 0; row < kRows; ++row)
    {
        for (int col = 0; col < kCols; ++col)
        {
            g_cells[row][col].text = kSeed[row][col];
        }
    }
    g_selRow = 1;
    g_selCol = 3;
}

void dumpGrid(std::ostream &out)
{
    out << "grid:\n";
    for (int row = 0; row < kRows; ++row)
    {
        out << "  row " << row + 1 << ": ";
        for (int col = 0; col < kCols; ++col)
        {
            if (col > 0)
            {
                out << " | ";
            }
            out << g_cells[row][col].shown;
        }
        out << '\n';
    }
}

bool checkGrid(const char *label, const char *const expected[kRows][kCols])
{
    bool ok = true;
    for (int row = 0; row < kRows; ++row)
    {
        for (int col = 0; col < kCols; ++col)
        {
            if (g_cells[row][col].shown != expected[row][col])
            {
                std::cout << "MISMATCH " << label << " " << cellName(row, col) << ": got '" << g_cells[row][col].shown
                          << "' want '" << expected[row][col] << "'\n";
                ok = false;
            }
        }
    }
    if (ok)
    {
        std::cout << label << ": grid match (" << kRows * kCols << " cells)\n";
    }
    return ok;
}

void onSet(Fl_Widget * /*widget*/, void * /*data*/)
{
    Cell &cell = g_cells[g_selRow][g_selCol];
    cell.text = g_formulaBar->value() != nullptr ? g_formulaBar->value() : "";
    std::cout << "set " << cellName(g_selRow, g_selCol) << " = '" << cell.text << "'\n";
    if (!recomputeAll())
    {
        return;
    }
    refreshUi();
}

void onRecalc(Fl_Widget * /*widget*/, void * /*data*/)
{
    if (!recomputeAll())
    {
        return;
    }
    refreshUi();
    std::cout << "recalculated live (load #" << g_loadCount << ")\n";
}

void onReset(Fl_Widget * /*widget*/, void * /*data*/)
{
    resetSheet();
    if (!recomputeAll())
    {
        return;
    }
    refreshUi();
    std::cout << "reset: seed restored\n";
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

    dumpGrid(std::cout);
    ok &= checkGrid("startup", kExpectedStart);

    // Real widget callbacks from here on: the formula bar and the Set
    // button run the same commit path as interactive editing.
    selectCell(1, 1);
    g_formulaBar->value("5");
    g_setButton->do_callback();
    dumpGrid(std::cout);
    if (g_cells[1][3].shown != "10" || g_cells[3][3].shown != "30")
    {
        std::cout << "MISMATCH edit B2: D2='" << g_cells[1][3].shown << "' D4='" << g_cells[3][3].shown << "'\n";
        ok = false;
    }
    else
    {
        std::cout << "edit B2: D2=10 D4=30\n";
    }

    // Fixing the syntax error must clear the !ERR cell live.
    selectCell(3, 4);
    g_formulaBar->value("=1+2");
    g_setButton->do_callback();
    if (g_cells[3][4].shown != "3")
    {
        std::cout << "MISMATCH fix E4: got '" << g_cells[3][4].shown << "' want '3'\n";
        ok = false;
    }
    else
    {
        std::cout << "fix E4: shows 3\n";
    }

    // Break the cycle, then restore it: both transitions recompute live.
    selectCell(4, 4);
    g_formulaBar->value("=1");
    g_setButton->do_callback();
    if (g_cells[4][4].shown != "1" || g_cells[5][4].shown != "2")
    {
        std::cout << "MISMATCH break cycle: E5='" << g_cells[4][4].shown << "' E6='" << g_cells[5][4].shown << "'\n";
        ok = false;
    }
    else
    {
        std::cout << "break cycle: E5=1 E6=2\n";
    }
    g_formulaBar->value("=E6+1");
    g_setButton->do_callback();
    if (g_cells[4][4].shown != "!CYCLE" || g_cells[5][4].shown != "!CYCLE")
    {
        std::cout << "MISMATCH restore cycle: E5='" << g_cells[4][4].shown << "' E6='" << g_cells[5][4].shown << "'\n";
        ok = false;
    }
    else
    {
        std::cout << "restore cycle: E5=!CYCLE E6=!CYCLE\n";
    }

    // Recalc recompiles the module without changing any value.
    std::vector<std::vector<std::string>> before(kRows, std::vector<std::string>(kCols));
    for (int row = 0; row < kRows; ++row)
    {
        for (int col = 0; col < kCols; ++col)
        {
            before[row][col] = g_cells[row][col].shown;
        }
    }
    const int loadsBefore = g_loadCount;
    g_recalcButton->do_callback();
    bool same = true;
    for (int row = 0; row < kRows; ++row)
    {
        for (int col = 0; col < kCols; ++col)
        {
            if (g_cells[row][col].shown != before[row][col])
            {
                same = false;
            }
        }
    }
    if (g_loadCount != loadsBefore + 1)
    {
        std::cout << "MISMATCH: recalc did not reload (loads " << g_loadCount << " want " << (loadsBefore + 1) << ")\n";
        ok = false;
    }
    else if (!same)
    {
        std::cout << "MISMATCH: recalc changed a value\n";
        ok = false;
    }
    else
    {
        std::cout << "recalc: reloaded, values unchanged (" << g_loadCount << " loads)\n";
    }

    // Reset restores the seed grid exactly.
    g_resetButton->do_callback();
    dumpGrid(std::cout);
    ok &= checkGrid("after reset", kExpectedStart);

    std::cout << "vm loads: " << g_loadCount << " formula calls: " << g_callCount << '\n';
    if (g_loadCount != kExpectedLoads)
    {
        std::cout << "MISMATCH: expected " << kExpectedLoads << " vm loads\n";
        ok = false;
    }
    if (g_callCount != kExpectedCalls)
    {
        std::cout << "MISMATCH: expected " << kExpectedCalls << " formula calls\n";
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
        // Null logger: every load/call failure below is reported by the host
        // with its cell attached, so the VM has nothing extra to log.
        std::unique_ptr<kern::IVirtualMachine> vm(kern::createVirtualMachine(nullptr));
        vm->addFunction("cell", kern::createExternFunction(&cell_value));
        vm->addFunction("emit", kern::createExternFunction(&emit_value));
        g_vm = vm.get();
        resetSheet();

        Fl_Window window(560, 380, kWindowTitle);
        g_window = &window;
        SheetTable table(16, 16, 528, 200);
        g_table = &table;
        table.callback(onTableSelect);
        Fl_Input formulaBar(16, 224, 412, 28, "");
        formulaBar.when(FL_WHEN_ENTER_KEY_ALWAYS);
        formulaBar.callback(onSet);
        g_formulaBar = &formulaBar;
        Fl_Button setButton(436, 224, 108, 28, "Set");
        setButton.callback(onSet);
        g_setButton = &setButton;
        Fl_Button recalcButton(16, 260, 120, 30, "Recalc");
        recalcButton.callback(onRecalc);
        g_recalcButton = &recalcButton;
        Fl_Button resetButton(144, 260, 120, 30, "Reset");
        resetButton.callback(onReset);
        g_resetButton = &resetButton;
        Fl_Box status(16, 300, 528, 24, "");
        g_status = &status;
        window.end();
        if (!recomputeAll())
        {
            return 1;
        }
        selectCell(1, 3);
        refreshUi();

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
