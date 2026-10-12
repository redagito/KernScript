#include <array>
#include <chrono>
#include <iostream>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <utility>

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Widget.H>
#include <FL/Fl_Window.H>
#include <FL/fl_draw.H>

#include <kern/Kern.h>

namespace
{

constexpr int kCells = 20;
constexpr int kCellPixels = 20;
constexpr int kGridPixels = kCells * kCellPixels;
constexpr int kGliderX = 5;
constexpr int kGliderY = 5;
constexpr double kTickSeconds = 0.2;
constexpr const char *kWindowTitle = "KernScript Life";

constexpr const char *kConwaySource = R"(extern cell_alive();
extern neighbors();
extern emit(next);
func decide() {
  var a = cell_alive();
  var n = neighbors();
  if (a == 1) {
    if (n == 2) {
      emit(1);
    } else {
      if (n == 3) {
        emit(1);
      } else {
        emit(0);
      }
    }
  } else {
    if (n == 3) {
      emit(1);
    } else {
      emit(0);
    }
  }
}
)";

constexpr const char *kHighLifeSource = R"(extern cell_alive();
extern neighbors();
extern emit(next);
func decide() {
  var a = cell_alive();
  var n = neighbors();
  if (a == 1) {
    if (n == 2) {
      emit(1);
    } else {
      if (n == 3) {
        emit(1);
      } else {
        emit(0);
      }
    }
  } else {
    if (n == 3) {
      emit(1);
    } else {
      if (n == 6) {
        emit(1);
      } else {
        emit(0);
      }
    }
  }
}
)";

constexpr const char *kDayNightSource = R"(extern cell_alive();
extern neighbors();
extern emit(next);
func decide() {
  var a = cell_alive();
  var n = neighbors();
  if (a == 1) {
    if (n == 3 or n == 4 or n == 6 or n == 7 or n == 8) {
      emit(1);
    } else {
      emit(0);
    }
  } else {
    if (n == 3 or n == 6 or n == 7 or n == 8) {
      emit(1);
    } else {
      emit(0);
    }
  }
}
)";

struct RulePreset
{
    const char *name;
    const char *source;
};

const RulePreset kConway = {"Conway", kConwaySource};
const RulePreset kHighLife = {"HighLife", kHighLifeSource};
const RulePreset kDayNight = {"DayNight", kDayNightSource};

using Grid = std::array<std::array<char, kCells>, kCells>;
using CellSet = std::set<std::pair<int, int>>;

Grid g_grids[2] = {};
int g_read = 0;
int g_cellX = 0;
int g_cellY = 0;
int g_emitted = 0;
int g_generation = 0;
int g_decideCalls = 0;
int g_loadCount = 0;
std::string g_preset = "Conway";
bool g_playing = false;
kern::IVirtualMachine *g_vm = nullptr;

Fl_Window *g_window = nullptr;
Fl_Box *g_status = nullptr;
Fl_Button *g_playButton = nullptr;
Fl_Button *g_pauseButton = nullptr;
Fl_Button *g_stepButton = nullptr;
Fl_Button *g_conwayButton = nullptr;
Fl_Button *g_highLifeButton = nullptr;
Fl_Button *g_dayNightButton = nullptr;

class LifeGrid : public Fl_Widget
{
public:
    LifeGrid(int x, int y, int w, int h) : Fl_Widget(x, y, w, h) {}

    void draw() override
    {
        fl_push_clip(x(), y(), w(), h());
        fl_color(FL_WHITE);
        fl_rectf(x(), y(), w(), h());
        for (int row = 0; row < kCells; ++row)
        {
            for (int col = 0; col < kCells; ++col)
            {
                if (g_grids[g_read][row][col] != 0)
                {
                    fl_color(FL_BLACK);
                }
                else
                {
                    fl_color(FL_WHITE);
                }
                fl_rectf(x() + col * kCellPixels + 1, y() + row * kCellPixels + 1, kCellPixels - 1, kCellPixels - 1);
            }
        }
        fl_color(FL_GRAY);
        for (int i = 0; i <= kCells; ++i)
        {
            fl_line(x() + i * kCellPixels, y(), x() + i * kCellPixels, y() + h());
            fl_line(x(), y() + i * kCellPixels, x() + w(), y() + i * kCellPixels);
        }
        fl_pop_clip();
    }
};

LifeGrid *g_gridWidget = nullptr;

int cell_alive() { return g_grids[g_read][g_cellY][g_cellX] != 0 ? 1 : 0; }

int neighbors()
{
    int count = 0;
    for (int dy = -1; dy <= 1; ++dy)
    {
        for (int dx = -1; dx <= 1; ++dx)
        {
            if (dx == 0 && dy == 0)
            {
                continue;
            }
            const int x = g_cellX + dx;
            const int y = g_cellY + dy;
            if (x < 0 || y < 0 || x >= kCells || y >= kCells)
            {
                continue;
            }
            if (g_grids[g_read][y][x] != 0)
            {
                ++count;
            }
        }
    }
    return count;
}

void emit_next(int next) { g_emitted = next != 0 ? 1 : 0; }

bool callDecide()
{
    try
    {
        if (!g_vm->callFunction("decide"))
        {
            std::cerr << "function decide not found\n";
            return false;
        }
    }
    catch (const kern::RuntimeError &error)
    {
        std::cerr << "runtime error: " << error.what() << '\n';
        return false;
    }
    ++g_decideCalls;
    return true;
}

bool loadPreset(const RulePreset &preset)
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

bool stepGeneration()
{
    const int write = 1 - g_read;
    for (int y = 0; y < kCells; ++y)
    {
        for (int x = 0; x < kCells; ++x)
        {
            g_cellX = x;
            g_cellY = y;
            g_emitted = 0;
            if (!callDecide())
            {
                return false;
            }
            g_grids[write][y][x] = static_cast<char>(g_emitted);
        }
    }
    g_read = write;
    ++g_generation;
    return true;
}

void resetGrid()
{
    for (auto &row : g_grids[g_read])
    {
        row.fill(0);
    }
    g_grids[g_read][kGliderY + 0][kGliderX + 1] = 1;
    g_grids[g_read][kGliderY + 1][kGliderX + 2] = 1;
    g_grids[g_read][kGliderY + 2][kGliderX + 0] = 1;
    g_grids[g_read][kGliderY + 2][kGliderX + 1] = 1;
    g_grids[g_read][kGliderY + 2][kGliderX + 2] = 1;
    g_generation = 0;
}

unsigned int gridHash()
{
    unsigned int hash = 2166136261u;
    for (const auto &row : g_grids[g_read])
    {
        for (char cell : row)
        {
            hash ^= static_cast<unsigned int>(cell);
            hash *= 16777619u;
        }
    }
    return hash;
}

void dumpGrid(std::ostream &out)
{
    for (const auto &row : g_grids[g_read])
    {
        for (char cell : row)
        {
            out << (cell != 0 ? 'O' : '.');
        }
        out << '\n';
    }
}

void logStep()
{
    std::cout << g_preset << " gen " << g_generation << " hash " << std::hex << gridHash() << std::dec << '\n';
}

void refreshUi()
{
    const std::string text = g_preset + std::string(" | gen ") + std::to_string(g_generation);
    g_status->copy_label(text.c_str());
    g_gridWidget->redraw();
}

void tick_cb(void *data);

void stopPlaying()
{
    g_playing = false;
    Fl::remove_timeout(tick_cb);
}

void onStep(Fl_Widget * /*widget*/, void * /*data*/)
{
    if (!stepGeneration())
    {
        stopPlaying();
        return;
    }
    logStep();
    refreshUi();
}

void tick_cb(void * /*data*/)
{
    if (!g_playing)
    {
        return;
    }
    onStep(nullptr, nullptr);
    if (g_playing)
    {
        Fl::repeat_timeout(kTickSeconds, tick_cb);
    }
}

void onPlay(Fl_Widget * /*widget*/, void * /*data*/)
{
    if (g_playing)
    {
        return;
    }
    g_playing = true;
    Fl::add_timeout(kTickSeconds, tick_cb);
    std::cout << "play\n";
}

void onPause(Fl_Widget * /*widget*/, void * /*data*/)
{
    stopPlaying();
    std::cout << "pause at gen " << g_generation << '\n';
}

void onReset(Fl_Widget * /*widget*/, void * /*data*/)
{
    stopPlaying();
    resetGrid();
    logStep();
    refreshUi();
    std::cout << "reset: glider restored\n";
}

void onPreset(Fl_Widget * /*widget*/, void *data)
{
    const RulePreset *preset = static_cast<const RulePreset *>(data);
    stopPlaying();
    if (!loadPreset(*preset))
    {
        return;
    }
    resetGrid();
    logStep();
    refreshUi();
}

CellSet liveCells()
{
    CellSet cells;
    for (int y = 0; y < kCells; ++y)
    {
        for (int x = 0; x < kCells; ++x)
        {
            if (g_grids[g_read][y][x] != 0)
            {
                cells.insert({x, y});
            }
        }
    }
    return cells;
}

CellSet shifted(std::initializer_list<std::pair<int, int>> relative)
{
    CellSet cells;
    for (const auto &cell : relative)
    {
        cells.insert({kGliderX + cell.first, kGliderY + cell.second});
    }
    return cells;
}

bool checkGeneration(int generation, const CellSet &expected)
{
    const CellSet actual = liveCells();
    if (actual == expected)
    {
        std::cout << "gen " << generation << ": match (" << actual.size() << " live)\n";
        return true;
    }
    std::cout << "gen " << generation << ": MISMATCH\nactual:";
    for (const auto &cell : actual)
    {
        std::cout << ' ' << cell.first << ',' << cell.second;
    }
    std::cout << "\nexpected:";
    for (const auto &cell : expected)
    {
        std::cout << ' ' << cell.first << ',' << cell.second;
    }
    std::cout << '\n';
    return false;
}

int runVerify()
{
    bool ok = true;
    std::cout << "window title: " << g_window->label() << '\n';
    std::cout << "window shown: " << (g_window->shown() != 0 ? 1 : 0) << '\n';

    const CellSet expected[] = {
        shifted({{1, 0}, {2, 1}, {0, 2}, {1, 2}, {2, 2}}), shifted({{0, 1}, {2, 1}, {1, 2}, {2, 2}, {1, 3}}),
        shifted({{2, 1}, {0, 2}, {2, 2}, {1, 3}, {2, 3}}), shifted({{1, 1}, {2, 2}, {3, 2}, {1, 3}, {2, 3}}),
        shifted({{2, 1}, {3, 2}, {1, 3}, {2, 3}, {3, 3}}), shifted({{1, 2}, {3, 2}, {2, 3}, {3, 3}, {2, 4}}),
    };
    logStep();
    dumpGrid(std::cout);
    ok &= checkGeneration(0, expected[0]);
    for (int generation = 1; generation <= 5; ++generation)
    {
        g_stepButton->do_callback();
        dumpGrid(std::cout);
        ok &= checkGeneration(generation, expected[generation]);
    }

    g_highLifeButton->do_callback();
    g_stepButton->do_callback();
    g_dayNightButton->do_callback();
    g_stepButton->do_callback();
    std::cout << "vm loads: " << g_loadCount << " decide calls: " << g_decideCalls << '\n';
    if (g_loadCount != 3)
    {
        std::cout << "MISMATCH: expected 3 vm loads\n";
        ok = false;
    }
    if (g_decideCalls != 2800)
    {
        std::cout << "MISMATCH: expected 2800 decide calls\n";
        ok = false;
    }
    g_playButton->do_callback();
    const auto pumpStart = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - pumpStart < std::chrono::milliseconds(2500))
    {
        Fl::wait(0.1);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    g_pauseButton->do_callback();
    const int played = g_generation;
    std::cout << "autoplay reached gen " << played << '\n';
    if (played < 6)
    {
        std::cout << "MISMATCH: timer did not advance at least 5 generations\n";
        ok = false;
    }
    for (int i = 0; i < 3; ++i)
    {
        Fl::wait(0.3);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (g_generation != played)
    {
        std::cout << "MISMATCH: timer kept ticking after pause\n";
        ok = false;
    }
    std::cout << "vm loads: " << g_loadCount << " decide calls: " << g_decideCalls << '\n';
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
        vm->addFunction("cell_alive", kern::createExternFunction(&cell_alive));
        vm->addFunction("neighbors", kern::createExternFunction(&neighbors));
        vm->addFunction("emit", kern::createExternFunction(&emit_next));
        g_vm = vm.get();
        if (!loadPreset(kConway))
        {
            return 1;
        }

        Fl_Window window(440, 544, kWindowTitle);
        g_window = &window;
        LifeGrid grid(20, 20, kGridPixels, kGridPixels);
        g_gridWidget = &grid;
        Fl_Button playButton(20, 430, 80, 30, "Play");
        playButton.callback(onPlay);
        g_playButton = &playButton;
        Fl_Button pauseButton(110, 430, 80, 30, "Pause");
        pauseButton.callback(onPause);
        g_pauseButton = &pauseButton;
        Fl_Button stepButton(200, 430, 80, 30, "Step");
        stepButton.callback(onStep);
        g_stepButton = &stepButton;
        Fl_Button resetButton(290, 430, 80, 30, "Reset");
        resetButton.callback(onReset);
        Fl_Button conwayButton(20, 465, 120, 30, "Conway");
        conwayButton.callback(onPreset, const_cast<RulePreset *>(&kConway));
        g_conwayButton = &conwayButton;
        Fl_Button highLifeButton(150, 465, 120, 30, "HighLife");
        highLifeButton.callback(onPreset, const_cast<RulePreset *>(&kHighLife));
        g_highLifeButton = &highLifeButton;
        Fl_Button dayNightButton(280, 465, 140, 30, "DayNight");
        dayNightButton.callback(onPreset, const_cast<RulePreset *>(&kDayNight));
        g_dayNightButton = &dayNightButton;
        Fl_Box status(20, 505, 400, 24, "Conway | gen 0");
        g_status = &status;
        window.end();
        resetGrid();
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
