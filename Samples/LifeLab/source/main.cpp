#include "../../Support/Files.h"
#include "../../Support/Preview.h"
#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/Fl_Slider.H>
#include <FL/Fl_Window.H>
#include <FL/fl_ask.H>
#include <FL/fl_draw.H>
#include <algorithm>
#include <array>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <kern/Kern.h>
#include <limits>
#include <memory>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;
namespace
{
using Grid = std::vector<unsigned char>;
constexpr int MinSize = 8, MaxSize = 160;
int strictInt(const char *text)
{
    int value = 0;
    const char *end = text;
    while (*end)
        ++end;
    auto result = std::from_chars(text, end, value);
    if (result.ec != std::errc{} || result.ptr != end)
        throw std::runtime_error("Enter a whole number");
    return value;
}
class Logger final : public kern::ILogger
{
public:
    std::string messages;

protected:
    void onError(const std::string &message) override { messages += message + '\n'; }
};
struct Rule
{
    Logger logger;
    std::unique_ptr<kern::IVirtualMachine> vm;
    std::string name, source, bytecode;
    fs::path path;
    int alive = 0, neighbors = 0, emits = 0, nextValue = 0;
    void emit(int v)
    {
        if (++emits != 1 || (v != 0 && v != 1))
            throw kern::RuntimeError("emit must be called exactly once with 0 or 1");
        nextValue = v;
    }
    Rule()
    {
        kern::VMConfig config;
        config.maxExecutionSteps = 10000;
        vm.reset(kern::createVirtualMachine(&logger, config));
    }
};
// Kern's typed bindings accept plain function pointers, so rule context is routed through these active callbacks.
Rule *activeRule = nullptr;
struct RuleScope
{
    Rule *previous;
    explicit RuleScope(Rule &rule) : previous(activeRule) { activeRule = &rule; }
    ~RuleScope() { activeRule = previous; }
    RuleScope(const RuleScope &) = delete;
    RuleScope &operator=(const RuleScope &) = delete;
};
int hostAlive() { return activeRule->alive; }
int hostNeighbors() { return activeRule->neighbors; }
void hostEmit(int value) { activeRule->emit(value); }
void bindRule(Rule &rule)
{
    rule.vm->addFunction("cell_alive", &hostAlive);
    rule.vm->addFunction("neighbors", &hostNeighbors);
    rule.vm->addFunction("emit", &hostEmit);
}
// checkInputs=false is used only to inject an invalid VM in the rollback verifier.
std::unique_ptr<Rule> compileRule(const std::string &source, const std::string &name, std::string &error,
                                  bool checkInputs = true)
{
    try
    {
        auto r = std::make_unique<Rule>();
        std::stringstream byteCode;
        std::istringstream input(source);
        std::unique_ptr<kern::ICompiler> c(kern::createCompiler(&r->logger));
        const auto parent = fs::path(name).parent_path();
        const std::vector<std::string> paths =
            parent.empty() ? std::vector<std::string>{} : std::vector<std::string>{parent.string()};
        if (!c->compileBytecode(input, byteCode, name, paths))
            throw std::runtime_error("Rule compilation failed:\n" + r->logger.messages);
        bindRule(*r);
        std::istringstream bytein(byteCode.str());
        if (!r->vm->load(bytein))
            throw std::runtime_error("script load failed");
        r->name = name;
        r->source = source;
        r->bytecode = byteCode.str();
        RuleScope scope(*r);
        for (int alive = 0; checkInputs && alive <= 1; ++alive)
            for (int n = 0; n <= 8; ++n)
            {
                r->alive = alive;
                r->neighbors = n;
                r->emits = 0;
                if (!r->vm->callFunction("decide"))
                    throw std::runtime_error("missing decide() entry point");
                if (r->emits != 1)
                    throw std::runtime_error("decide() must emit exactly once for each input case");
            }
        error.clear();
        return r;
    }
    catch (const std::exception &e)
    {
        error = e.what();
        return {};
    }
}
struct Lab
{
    int w = 40, h = 40;
    Grid cells = Grid(1600), next = Grid(1600);
    bool wrap = false, dirty = false;
    unsigned long long generation = 0, births = 0, deaths = 0;
    fs::path pattern;
    std::unique_ptr<Rule> rule;
    std::size_t at(int x, int y) const { return static_cast<std::size_t>(y * w + x); }
    int population() const { return static_cast<int>(std::count(cells.begin(), cells.end(), 1)); }
    void paint(int x, int y, int value)
    {
        if (x >= 0 && y >= 0 && x < w && y < h && cells[at(x, y)] != (value ? 1 : 0))
        {
            cells[at(x, y)] = value ? 1 : 0;
            dirty = true;
        }
    }
    int count(int x, int y) const
    {
        int n = 0;
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx)
            {
                if (!dx && !dy)
                    continue;
                int xx = x + dx, yy = y + dy;
                if (wrap)
                {
                    xx = (xx + w) % w;
                    yy = (yy + h) % h;
                }
                else if (xx < 0 || yy < 0 || xx >= w || yy >= h)
                    continue;
                n += cells[at(xx, yy)] ? 1 : 0;
            }
        return n;
    }
    bool step(std::string &error)
    {
        if (generation == std::numeric_limits<unsigned long long>::max())
        {
            error = "Generation counter is full. Clear or start a new pattern";
            return false;
        }
        if (!rule)
        {
            error = "No rule is loaded";
            return false;
        }
        auto &r = *rule;
        RuleScope scope(r);
        int b = 0, d = 0;
        next.assign(cells.size(), 0);
        try
        {
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x)
                {
                    r.alive = cells[at(x, y)];
                    r.neighbors = count(x, y);
                    r.emits = 0;
                    if (!r.vm->callFunction("decide"))
                        throw std::runtime_error("missing decide() entry point");
                    if (r.emits != 1)
                        throw std::runtime_error("rule must emit exactly once per cell");
                    next[at(x, y)] = static_cast<unsigned char>(r.nextValue);
                    if (!cells[at(x, y)] && r.nextValue)
                        ++b;
                    if (cells[at(x, y)] && !r.nextValue)
                        ++d;
                }
        }
        catch (const std::exception &e)
        {
            error = e.what();
            return false;
        }
        cells.swap(next);
        ++generation;
        births = b;
        deaths = d;
        dirty = true;
        error.clear();
        return true;
    }
    bool reload(const fs::path &p, std::string &error)
    {
        std::string src;
        try
        {
            src = samples::readText(p, 256 * 1024);
        }
        catch (const std::exception &e)
        {
            error = e.what();
            return false;
        }
        auto candidate = compileRule(src, fs::absolute(p).string(), error);
        if (!candidate)
        {
            return false;
        }
        candidate->path = fs::absolute(p);
        candidate->name = p.filename().string();
        rule = std::move(candidate);
        return true;
    }
    bool resize(int nw, int nh, std::string &error)
    {
        if (nw < MinSize || nw > MaxSize || nh < MinSize || nh > MaxSize)
        {
            error = "Dimensions must be between 8 and 160";
            return false;
        }
        Grid replacement(static_cast<std::size_t>(nw * nh));
        for (int y = 0; y < std::min(h, nh); ++y)
            for (int x = 0; x < std::min(w, nw); ++x)
                replacement[static_cast<std::size_t>(y * nw + x)] = cells[at(x, y)];
        w = nw;
        h = nh;
        cells = std::move(replacement);
        next.assign(cells.size(), 0);
        dirty = true;
        return true;
    }
    void clear()
    {
        std::fill(cells.begin(), cells.end(), 0);
        generation = 0;
        births = 0;
        deaths = 0;
        dirty = true;
    }
    void randomize(unsigned seed)
    {
        std::mt19937 rng(seed);
        for (auto &c : cells)
            c = (rng() % 100 < 28) ? 1 : 0;
        generation = 0;
        births = 0;
        deaths = 0;
        dirty = true;
    }
    void preset(const std::string &name)
    {
        clear();
        int x = w / 2, y = h / 2;
        if (name == "Glider")
        {
            for (auto p : std::array<std::pair<int, int>, 5>{{{1, 0}, {2, 1}, {0, 2}, {1, 2}, {2, 2}}})
                cells[at(x - 1 + p.first, y - 1 + p.second)] = 1;
        }
        else if (name == "Blinker")
        {
            for (int dx = -1; dx <= 1; ++dx)
                cells[at(x + dx, y)] = 1;
        }
        else if (name == "Toad")
        {
            for (auto p : std::array<std::pair<int, int>, 6>{{{0, 0}, {1, 0}, {2, 0}, {-1, 1}, {0, 1}, {1, 1}}})
                cells[at(x + p.first, y + p.second)] = 1;
        }
        dirty = true;
    }
    void save(const fs::path &p)
    {
        if (p.empty())
            throw std::runtime_error("Choose a pattern file");
        samples::writeAtomically(p,
                                 [&](std::ostream &o)
                                 {
                                     o << "KernScript LifeLab 1\n"
                                       << w << ' ' << h << ' ' << generation << ' ' << (wrap ? 1 : 0) << '\n';
                                     for (int y = 0; y < h; ++y)
                                     {
                                         for (int x = 0; x < w; ++x)
                                             o << (cells[at(x, y)] ? '1' : '0');
                                         o << '\n';
                                     }
                                 });
        pattern = p;
        dirty = false;
    }
    void open(const fs::path &p)
    {
        auto encoded = samples::readText(p, 30000);
        for (std::size_t i = 0; (i = encoded.find("\r\n", i)) != std::string::npos;)
            encoded.erase(i, 1);
        std::istringstream in(encoded);
        std::string magic;
        int nw, nh, wr;
        unsigned long long gen;
        std::string generationText;
        if (!std::getline(in, magic) || magic != "KernScript LifeLab 1" || !(in >> nw >> nh >> generationText >> wr) ||
            nw < MinSize || nw > MaxSize || nh < MinSize || nh > MaxSize || (wr != 0 && wr != 1))
            throw std::runtime_error("Invalid LifeLab pattern header or dimensions");
        const auto parsed = std::from_chars(generationText.data(), generationText.data() + generationText.size(), gen);
        if (parsed.ec != std::errc{} || parsed.ptr != generationText.data() + generationText.size())
            throw std::runtime_error("Invalid pattern generation number");
        std::string line;
        std::getline(in, line);
        if (line.find_first_not_of(" \t\r") != std::string::npos)
            throw std::runtime_error("Unexpected data after pattern dimensions");
        Grid candidate(static_cast<std::size_t>(nw * nh));
        for (int y = 0; y < nh; ++y)
        {
            if (!std::getline(in, line) || static_cast<int>(line.size()) != nw)
                throw std::runtime_error("Invalid pattern row");
            for (int x = 0; x < nw; ++x)
            {
                if (line[x] != '0' && line[x] != '1')
                    throw std::runtime_error("Pattern cells must be 0 or 1");
                candidate[static_cast<std::size_t>(y * nw + x)] = line[x] == '1';
            }
        }
        if (std::getline(in, line))
            throw std::runtime_error("Unexpected data after pattern");
        w = nw;
        h = nh;
        cells = std::move(candidate);
        next.assign(cells.size(), 0);
        generation = gen;
        births = 0;
        deaths = 0;
        wrap = wr != 0;
        pattern = p;
        dirty = false;
    }
};
bool mapping(int px, int py, int gx, int gy, int gw, int gh, int w, int h, int &cx, int &cy)
{
    if (gw <= 0 || gh <= 0 || px < gx || py < gy || px >= gx + gw || py >= gy + gh)
        return false;
    // Invert the exact integer boundaries used by draw(), including uneven cell widths.
    cx = ((px - gx + 1) * w - 1) / gw;
    cy = ((py - gy + 1) * h - 1) / gh;
    return cx >= 0 && cy >= 0 && cx < w && cy < h;
}
std::string csv(const std::string &s)
{
    std::string q = "\"";
    for (char c : s)
    {
        if (c == '\"')
            q += "\"\"";
        else
            q += c;
    }
    return q + "\"";
}
void experiment(const Lab &lab, int generations, const fs::path &path)
{
    if (generations < 1 || generations > 10000)
        throw std::runtime_error("Experiment generations must be 1..10000");
    if (!lab.rule)
        throw std::runtime_error("Load a rule before running an experiment");
    if (!lab.pattern.empty() && (fs::weakly_canonical(path) == fs::weakly_canonical(lab.pattern) ||
                                 (fs::exists(path) && fs::equivalent(path, lab.pattern))))
        throw std::runtime_error("Experiment export cannot overwrite the open pattern");
    Lab scratch;
    scratch.w = lab.w;
    scratch.h = lab.h;
    scratch.cells = lab.cells;
    scratch.next.assign(lab.cells.size(), 0);
    scratch.wrap = lab.wrap;
    scratch.generation = lab.generation;
    // Use the exact loaded program even when its source was edited since loading.
    scratch.rule = std::make_unique<Rule>();
    bindRule(*scratch.rule);
    std::istringstream bytecode(lab.rule->bytecode);
    if (!scratch.rule->vm->load(bytecode))
        throw std::runtime_error("Could not prepare the experiment rule");
    scratch.rule->name = lab.rule->name;
    samples::writeAtomically(
        path,
        [&](std::ostream &o)
        {
            o << "generation,population,births,deaths,rule,boundary\n"
              << scratch.generation << ',' << scratch.population() << ",0,0," << csv(scratch.rule->name) << ','
              << (scratch.wrap ? "toroidal" : "finite") << '\n';
            std::string e;
            for (int i = 0; i < generations; ++i)
            {
                if (!scratch.step(e))
                    throw std::runtime_error("Experiment stopped at generation " + std::to_string(i + 1) + ": " + e);
                o << scratch.generation << ',' << scratch.population() << ',' << scratch.births << ',' << scratch.deaths
                  << ',' << csv(scratch.rule->name) << ',' << (scratch.wrap ? "toroidal" : "finite") << '\n';
            }
        });
}

Lab *ui = nullptr;
Fl_Window *win = nullptr;
Fl_Box *status = nullptr;
Fl_Choice *presetChoice = nullptr;
Fl_Input *widthInput = nullptr, *heightInput = nullptr, *seedInput = nullptr, *experimentInput = nullptr;
Fl_Slider *speed = nullptr;
bool playing = false;
int paintValue = 1;
void refresh();
void pause(Fl_Widget *, void *);
struct Board : Fl_Widget
{
    Board(int x, int y, int w, int h) : Fl_Widget(x, y, w, h) {}
    void draw() override
    {
        fl_push_clip(x(), y(), w(), h());
        for (int yy = 0; yy < ui->h; ++yy)
            for (int xx = 0; xx < ui->w; ++xx)
            {
                int left = x() + xx * w() / ui->w, top = y() + yy * h() / ui->h, right = x() + (xx + 1) * w() / ui->w,
                    bottom = y() + (yy + 1) * h() / ui->h;
                fl_color(ui->cells[ui->at(xx, yy)] ? fl_rgb_color(35, 70, 90) : FL_WHITE);
                fl_rectf(left, top, right - left, bottom - top);
            }
        fl_color(FL_GRAY);
        for (int xx = 0; xx <= ui->w; ++xx)
            fl_line(x() + xx * w() / ui->w, y(), x() + xx * w() / ui->w, y() + h());
        for (int yy = 0; yy <= ui->h; ++yy)
            fl_line(x(), y() + yy * h() / ui->h, x() + w(), y() + yy * h() / ui->h);
        fl_pop_clip();
    }
    int handle(int e) override
    {
        if (e == FL_PUSH || e == FL_DRAG)
        {
            int cx, cy;
            if (mapping(Fl::event_x(), Fl::event_y(), x(), y(), w(), h(), ui->w, ui->h, cx, cy))
            {
                pause(nullptr, nullptr);
                if (e == FL_PUSH)
                    paintValue = (Fl::event_button() == FL_RIGHT_MOUSE || ui->cells[ui->at(cx, cy)]) ? 0 : 1;
                ui->paint(cx, cy, paintValue);
                refresh();
                return 1;
            }
        }
        return Fl_Widget::handle(e);
    }
};
void refresh()
{
    if (!ui)
        return;
    const std::string label = ui->rule->name + (ui->wrap ? " | toroidal" : " | finite") + " | generation " +
                              std::to_string(ui->generation) + " | population " + std::to_string(ui->population()) +
                              " | +" + std::to_string(ui->births) + " / -" + std::to_string(ui->deaths) +
                              (ui->dirty ? " *" : "");
    status->copy_label(label.c_str());
    widthInput->value(std::to_string(ui->w).c_str());
    heightInput->value(std::to_string(ui->h).c_str());
    win->redraw();
}
void tick(void *)
{
    if (!playing)
        return;
    std::string e;
    if (!ui->step(e))
    {
        playing = false;
        fl_alert("Playback stopped: %s", e.c_str());
        return;
    }
    refresh();
    Fl::repeat_timeout(1.0 / std::max(0.2, speed->value()), tick);
}
bool savePattern(bool saveAs = false)
{
    fs::path p = ui->pattern;
    if (saveAs || p.empty())
    {
        Fl_Native_File_Chooser c;
        c.title("Save LifeLab pattern");
        c.type(Fl_Native_File_Chooser::BROWSE_SAVE_FILE);
        c.options(Fl_Native_File_Chooser::SAVEAS_CONFIRM);
        c.filter("LifeLab pattern\t*.life");
        c.preset_file("pattern.life");
        if (c.show() != 0)
            return false;
        p = c.filename();
    }
    ui->save(p);
    return true;
}
bool changes()
{
    if (!ui->dirty)
        return true;
    int v = fl_choice("Save changes to the current pattern?", "Cancel", "Discard", "Save");
    if (v == 0)
        return false;
    if (v == 1)
        return true;
    try
    {
        return savePattern();
    }
    catch (const std::exception &e)
    {
        fl_alert("Save failed: %s", e.what());
        return false;
    }
}
void report(const char *label, auto fn)
{
    pause(nullptr, nullptr);
    try
    {
        fn();
        refresh();
    }
    catch (const std::exception &e)
    {
        fl_alert("%s: %s", label, e.what());
    }
}
void doSave(Fl_Widget *, void *)
{
    report("Save",
           []
           {
               if (!savePattern())
                   return;
           });
}
void doOpen(Fl_Widget *, void *)
{
    pause(nullptr, nullptr);
    Fl_Native_File_Chooser c;
    c.type(Fl_Native_File_Chooser::BROWSE_FILE);
    c.filter("LifeLab pattern\t*.life");
    if (c.show())
        return;
    try
    {
        Lab candidate;
        if (!changes())
            return;
        candidate.open(c.filename());
        candidate.rule = std::move(ui->rule);
        *ui = std::move(candidate);
        refresh();
    }
    catch (const std::exception &e)
    {
        fl_alert("Open failed: %s", e.what());
    }
}
void doStep(Fl_Widget *, void *)
{
    report("Step",
           []
           {
               std::string e;
               if (!ui->step(e))
                   throw std::runtime_error(e);
           });
}
void doPreset(Fl_Widget *, void *)
{
    report("Preset",
           []
           {
               if (changes())
                   ui->preset(presetChoice->text());
           });
}
void doClear(Fl_Widget *, void *)
{
    report("Clear",
           []
           {
               if (changes())
                   ui->clear();
           });
}
void doRandom(Fl_Widget *, void *)
{
    report("Random",
           []
           {
               int s = strictInt(seedInput->value());
               if (s < 0)
                   throw std::runtime_error("Seed must be nonnegative");
               if (changes())
                   ui->randomize(static_cast<unsigned>(s));
           });
}
void doResize(Fl_Widget *, void *)
{
    report("Resize",
           []
           {
               int w = strictInt(widthInput->value()), h = strictInt(heightInput->value());
               if (w > 0 && h > 0 && (w < ui->w || h < ui->h) &&
                   fl_choice("Smaller dimensions discard cells outside the new grid.", "Cancel", "Resize", nullptr) ==
                       0)
                   return;
               std::string e;
               if (!ui->resize(w, h, e))
                   throw std::runtime_error(e);
           });
}
void doRule(Fl_Widget *, void *)
{
    pause(nullptr, nullptr);
    Fl_Native_File_Chooser c;
    c.type(Fl_Native_File_Chooser::BROWSE_FILE);
    c.filter("KernScript rule\t*.ks");
    if (c.show())
        return;
    report("Rule reload",
           [&]
           {
               std::string e;
               if (!ui->reload(c.filename(), e))
                   throw std::runtime_error(e);
           });
}
void doExperiment(Fl_Widget *, void *)
{
    pause(nullptr, nullptr);
    Fl_Native_File_Chooser c;
    c.type(Fl_Native_File_Chooser::BROWSE_SAVE_FILE);
    c.options(Fl_Native_File_Chooser::SAVEAS_CONFIRM);
    c.filter("CSV experiment\t*.csv");
    c.preset_file("experiment.csv");
    if (c.show())
        return;
    report("Experiment", [&] { experiment(*ui, strictInt(experimentInput->value()), c.filename()); });
}
void play(Fl_Widget *, void *)
{
    if (playing)
        return;
    playing = true;
    Fl::add_timeout(0.1, tick);
}
void pause(Fl_Widget *, void *)
{
    playing = false;
    Fl::remove_timeout(tick);
}

bool verify(const fs::path &testRoot, const fs::path &rules)
{
    try
    {
        const auto root = samples::testDirectory(testRoot);
        auto require = [](bool condition, const char *message)
        {
            if (!condition)
                throw std::runtime_error(message);
        };
        auto rejected = [&](auto operation)
        {
            bool failed = false;
            try
            {
                operation();
            }
            catch (const std::exception &)
            {
                failed = true;
            }
            require(failed, "An invalid operation was accepted");
        };
        auto write = [](const fs::path &path, const std::string &content)
        { samples::writeAtomically(path, [&](std::ostream &out) { out << content; }); };
        Lab l;
        std::string e;
        auto successful = [&](bool condition)
        {
            if (!condition)
                throw std::runtime_error(e);
        };
        successful(l.reload(rules / "conway.ks", e));
        require(l.resize(20, 20, e), "Resize failed");
        l.preset("Glider");
        for (int g = 0; g < 4; ++g)
            successful(l.step(e));
        Grid expected(400);
        for (auto p : std::array<std::pair<int, int>, 5>{{{1, 0}, {2, 1}, {0, 2}, {1, 2}, {2, 2}}})
            expected[l.at(10 + p.first, 10 + p.second)] = 1;
        require(l.cells == expected && l.population() == 5 && l.generation == 4, "Glider mismatch");
        l.preset("Blinker");
        successful(l.step(e));
        expected.assign(400, 0);
        for (int dy = -1; dy <= 1; ++dy)
            expected[l.at(10, 10 + dy)] = 1;
        require(l.cells == expected && l.births == 2 && l.deaths == 2, "Blinker mismatch");
        l.clear();
        l.wrap = false;
        l.paint(19, 19, 1);
        l.paint(0, 19, 1);
        l.paint(19, 0, 1);
        require(l.count(0, 0) == 0, "Finite edge mismatch");
        l.wrap = true;
        require(l.count(0, 0) == 3, "Toroidal edge mismatch");
        require(l.step(e) && l.cells[l.at(0, 0)] == 1, "Wrapped birth mismatch");
        // Every displayed pixel must map back to its drawn cell, including uneven divisions.
        for (int cell = 0; cell < 160; ++cell)
        {
            int first = cell * 650 / 160, last = (cell + 1) * 650 / 160;
            for (int pixel = first; pixel < last; ++pixel)
            {
                int cx = -1, cy = -1;
                require(mapping(15 + pixel, 15 + pixel, 15, 15, 650, 650, 160, 160, cx, cy) && cx == cell && cy == cell,
                        "Paint mapping disagrees with drawn boundaries");
            }
        }
        int cx, cy;
        require(!mapping(665, 20, 15, 15, 650, 650, 160, 160, cx, cy), "Outside click accepted");
        l.clear();
        l.paint(2, 3, 1);
        require(l.resize(12, 8, e) && l.population() == 1 && l.cells[l.at(2, 3)] == 1, "Resize lost overlapping cells");
        auto cells = l.cells;
        require(!l.resize(7, 100, e) && l.cells == cells && l.w == 12 && l.h == 8, "Invalid resize changed the grid");
        l.generation = 7;
        auto pattern = root / "sample.life";
        l.save(pattern);
        l.clear();
        l.open(pattern);
        require(l.cells == cells && l.generation == 7 && l.wrap && !l.dirty, "Pattern roundtrip failed");
        l.paint(3, 3, 1);
        l.save(pattern);
        l.open(pattern);
        require(l.population() == 2 && !l.dirty, "Pattern overwrite failed");
        cells = l.cells;
        auto bad = root / "invalid.life";
        for (const auto &content :
             {std::string("bad\n"), std::string("KernScript LifeLab 1\n12 8 1 0 extra\n"),
              std::string("KernScript LifeLab 1\n12 8 -1 0\n"), std::string("KernScript LifeLab 1\n161 8 0 0\n"),
              samples::readText(pattern, 30000) + "unexpected\n"})
        {
            write(bad, content);
            rejected([&] { l.open(bad); });
            require(l.cells == cells && l.pattern == pattern && l.generation == 7 && !l.dirty,
                    "Invalid open changed the current pattern");
        }
        // Verify the actual shipped external rules against their published B/S sets.
        for (const auto *name : {"conway.ks", "highlife.ks", "daynight.ks"})
        {
            successful(l.reload(rules / name, e));
            RuleScope scope(*l.rule);
            for (int alive = 0; alive <= 1; ++alive)
                for (int n = 0; n <= 8; ++n)
                {
                    auto &rule = *l.rule;
                    rule.alive = alive;
                    rule.neighbors = n;
                    rule.emits = 0;
                    require(rule.vm->callFunction("decide"), "Missing decide");
                    bool wanted = alive ? (n == 2 || n == 3) : n == 3;
                    if (std::string(name) == "highlife.ks" && !alive)
                        wanted = (n == 3 || n == 6);
                    if (std::string(name) == "daynight.ks")
                        wanted = alive ? (n == 3 || n == 4 || n == 6 || n == 7 || n == 8)
                                       : (n == 3 || n == 6 || n == 7 || n == 8);
                    require(rule.nextValue == static_cast<int>(wanted) && rule.emits == 1,
                            "Bundled rule truth table mismatch");
                }
        }
        auto oldRule = l.rule.get();
        auto candidate = root / "candidate.ks";
        for (const auto &src : {std::string("broken script"), std::string("func another() -> void {}"),
                                std::string("func decide() -> void {}"),
                                std::string("extern emit(x) -> void; func decide() -> void { emit(2); }"),
                                std::string("extern emit(x) -> void; func decide() -> void { emit(0); emit(1); }"),
                                std::string("func decide() -> void { var zero=0; var x=1/zero; }"),
                                std::string("func decide() -> void { while (1==1) {} }"),
                                std::string("extern emit(x) -> void; func decide() -> void { emit(\"wrong type\"); }")})
        {
            write(candidate, src);
            require(!l.reload(candidate, e) && l.rule.get() == oldRule && l.cells == cells && activeRule == nullptr,
                    "Rejected rule changed the loaded program or grid");
        }
        // Inject an unvalidated VM so the generation transaction is tested even
        // though normal rule admission checks every reachable input combination.
        l.clear();
        l.wrap = false;
        for (int y = 3; y <= 5; ++y)
            for (int x = 3; x <= 5; ++x)
                l.paint(x, y, 1);
        cells = l.cells;
        const std::string failing =
            "extern neighbors() -> value; extern emit(v) -> void; "
            "func decide() -> void { if (neighbors()==8) { var zero=0; var x=1/zero; } emit(0); }";
        l.rule = compileRule(failing, "late failure", e, false);
        require(l.rule && !l.step(e) && l.cells == cells && l.generation == 0 && activeRule == nullptr,
                "A late runtime failure committed a partial generation");
        successful(l.reload(rules / "conway.ks", e));
        l.preset("Blinker");
        l.generation = 4;
        cells = l.cells;
        const auto out = root / "experiment.csv";
        experiment(l, 2, out);
        const auto csvText = samples::readText(out, 4096);
        require(l.cells == cells && l.generation == 4 && activeRule == nullptr && csvText.contains("4,3,0,0,") &&
                    csvText.contains("5,3,2,2,") && csvText.contains("6,3,2,2,"),
                "Experiment results or scratch isolation failed");
        rejected([&] { experiment(l, 2, pattern); });
        require(samples::readText(pattern, 30000).starts_with("KernScript LifeLab 1\n"),
                "Pattern export protection failed");
        require(l.resize(80, 80, e), "Larger resize failed");
        l.randomize(1234);
        l.wrap = true;
        auto oracle = l.cells;
        for (int generation = 0; generation < 8; ++generation)
        {
            Grid following(oracle.size());
            for (int y = 0; y < 80; ++y)
                for (int x = 0; x < 80; ++x)
                {
                    int neighbors = 0;
                    for (int dy = -1; dy <= 1; ++dy)
                        for (int dx = -1; dx <= 1; ++dx)
                            if (dx || dy)
                                neighbors +=
                                    oracle[static_cast<std::size_t>(((y + dy + 80) % 80) * 80 + (x + dx + 80) % 80)];
                    const auto at = static_cast<std::size_t>(y * 80 + x);
                    following[at] = neighbors == 3 || (oracle[at] && neighbors == 2);
                }
            oracle = std::move(following);
            successful(l.step(e));
            require(l.cells == oracle, "Larger grid differs from reference");
        }
        std::cout << "LifeLab verify: PASS (patterns, 18-case external rules, editing, persistence, transactions, CSV, "
                     "80x80 reference)\n";
        return true;
    }
    catch (const std::exception &error)
    {
        std::cerr << "LifeLab verification failed: " << error.what() << '\n';
        return false;
    }
}
int runUi(const fs::path &rules, const fs::path &preview)
{
    Lab lab;
    ui = &lab;
    std::string e;
    if (!ui->reload(rules / "conway.ks", e))
        throw std::runtime_error(e);
    ui->preset("Glider");
    ui->dirty = false;
    auto owner = std::make_unique<Fl_Window>(920, 740, "LifeLab | KernScript");
    win = owner.get();
    auto *board = new Board(15, 15, 650, 650);
    (void)board;
    Fl_Button *b = new Fl_Button(690, 25, 95, 28, "Play");
    b->callback(play);
    b = new Fl_Button(790, 25, 95, 28, "Pause");
    b->callback(pause);
    b = new Fl_Button(690, 62, 95, 28, "Step");
    b->callback(doStep);
    b = new Fl_Button(790, 62, 95, 28, "Clear");
    b->callback(doClear);
    presetChoice = new Fl_Choice(690, 110, 190, 28, "Preset");
    presetChoice->add("Glider");
    presetChoice->add("Blinker");
    presetChoice->add("Toad");
    presetChoice->value(0);
    presetChoice->align(FL_ALIGN_TOP_LEFT);
    b = new Fl_Button(690, 143, 190, 28, "Apply preset");
    b->callback(doPreset);
    widthInput = new Fl_Input(690, 190, 85, 26, "Width");
    heightInput = new Fl_Input(790, 190, 85, 26, "Height");
    widthInput->align(FL_ALIGN_TOP_LEFT);
    heightInput->align(FL_ALIGN_TOP_LEFT);
    widthInput->value("40");
    heightInput->value("40");
    b = new Fl_Button(690, 225, 190, 28, "Resize grid");
    b->callback(doResize);
    seedInput = new Fl_Input(745, 264, 70, 26, "Seed");
    seedInput->value("1234");
    b = new Fl_Button(690, 295, 190, 28, "Randomize");
    b->callback(doRandom);
    b = new Fl_Button(690, 335, 190, 28, "Load / reload rule");
    b->callback(doRule);
    speed = new Fl_Slider(700, 390, 180, 25, "Generations / second");
    speed->type(FL_HORIZONTAL);
    speed->bounds(0.5, 20);
    speed->value(5);
    Fl_Button *wrapButton = new Fl_Button(690, 440, 190, 28, "Toggle finite / toroidal");
    wrapButton->callback(
        [](Fl_Widget *, void *)
        {
            report("Boundary",
                   []
                   {
                       ui->wrap = !ui->wrap;
                       ui->dirty = true;
                   });
        });
    b = new Fl_Button(690, 480, 90, 28, "Open");
    b->callback(doOpen);
    b = new Fl_Button(790, 480, 90, 28, "Save");
    b->callback(doSave);
    b = new Fl_Button(690, 515, 190, 28, "Save As");
    b->callback([](Fl_Widget *, void *) { report("Save As", [] { (void)savePattern(true); }); });
    experimentInput = new Fl_Input(795, 565, 80, 26, "Generations");
    experimentInput->value("100");
    b = new Fl_Button(690, 600, 190, 28, "Export experiment CSV");
    b->callback(doExperiment);
    status = new Fl_Box(680, 645, 220, 75, "");
    status->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_WRAP);
    auto *help = new Fl_Box(15, 680, 650, 40,
                            "Click and drag to paint. Start on a live cell or use the right button to erase.\n"
                            "Editing pauses playback. Save a pattern before comparing rule experiments.");
    help->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_WRAP);
    win->end();
    win->callback(
        [](Fl_Widget *, void *)
        {
            pause(nullptr, nullptr);
            if (changes())
                win->hide();
        });
    refresh();
    if (!preview.empty())
    {
        samples::renderPreview(*win, preview);
        return 0;
    }
    win->show();
    Fl::run();
    return 0;
}
} // namespace
int main(int argc, char **argv)
{
    bool verifyMode = false;
    fs::path preview, testRoot = fs::current_path() / "build" / "test-tmp" / "LifeLab";
    for (int i = 1; i < argc; ++i)
    {
        std::string a = argv[i];
        if (a == "--verify")
            verifyMode = true;
        else if (a == "--test-root" && i + 1 < argc)
            testRoot = argv[++i];
        else if (a == "--preview" && i + 1 < argc)
            preview = argv[++i];
        else if (a == "--help")
        {
            std::cout << "LifeLab [--help] [--verify --test-root DIR]\n";
            return 0;
        }
        else
        {
            std::cerr << "Usage: LifeLab [--help] [--verify --test-root DIR]\n";
            return 2;
        }
    }
    try
    {
        fs::path rules;
        for (const auto &directory : {fs::path(argc > 0 ? argv[0] : "").parent_path() / "rules", fs::path("rules"),
                                      fs::path("Samples/LifeLab/rules")})
            if (fs::is_regular_file(directory / "conway.ks"))
            {
                rules = fs::absolute(directory);
                break;
            }
        if (rules.empty())
            throw std::runtime_error("LifeLab's rules directory was not found");
        if (verifyMode)
            return verify(testRoot, rules) ? 0 : 1;
        return runUi(rules, preview);
    }
    catch (const std::exception &e)
    {
        std::cerr << "LifeLab: " << e.what() << '\n';
        return 1;
    }
}
