// ScriptPaint: a scriptable paint studio stressing the module system.
//
// Host = dumb shell: an FLTK window with a canvas widget (host-side packed
// RGB buffer, 320x200 so script per-pixel loops stay tractable), tool and
// filter buttons, and fixed color swatches. All pixels, rules and pictures
// live in scripts under scripts/; the host only owns the buffer, the mouse
// and the button wiring.
//
// Hot-reload by design: EVERY button press re-reads from disk and recompiles
// a small synthetic root that imports the SELECTED module plus its imports
// (module search path = the scripts dir plus its lib/tools/filters/gen
// subdirs, because `import name` maps to a bare `name.ks` lookup). Filter
// and generator presses also import the active tool: vm->load replaces the
// whole program, so without the resident tool import the next paint tick
// would call into an evicted program. Editing any .ks file and pressing the
// button again picks the edit up; no host rebuild is involved.
//
// Packed RGB is 0xRRGGBB as a script int. 0xFFFFFF = 16777215 < 2^31, so
// every color stays a signed-safe positive int32 (see also color.ks).
//
// VM step budget: per-pixel script loops cost millions of steps (Mandel is
// ~64k pixels x up to 30 iterations), so the VM runs with an explicit
// raised maxExecutionSteps. The budget still backstops runaway scripts;
// it is a configured ceiling, not an absent one.
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Widget.H>
#include <FL/Fl_Window.H>
#include <FL/fl_draw.H>

#include <kern/Kern.h>

namespace
{

constexpr int kCanvasW = 320;
constexpr int kCanvasH = 200;
constexpr int kPixels = kCanvasW * kCanvasH;
constexpr double kTickSeconds = 0.05;
constexpr const char *kWindowTitle = "KernScript Paint";
constexpr int kPenDefault = 16777215;
constexpr int kSwatchWhite = 16777215;
constexpr int kSwatchRed = 16711680;
constexpr int kSwatchGreen = 65280;
constexpr int kSwatchBlue = 255;

// Diamond probe (verify only, never shipped as a file): one anonymous root
// importing two filters that both import color, plus the resident tool.
// Success proves color.ks loads once and is shared; loading it twice would
// fail the merge with a duplicate-definition error.
constexpr const char *kDiamondSource = R"(import gray;
import edge;
import pencil;
extern canvas_w();
extern canvas_h();
extern get_pixel(x, y);
extern set_pixel(x, y, rgb);
func main() {
  gray::run();
  edge::run();
}
)";

struct ModuleButton
{
    const char *label;
    const char *file;
    const char *mod;
    const char *entry;
    const char *call;
};

const ModuleButton kPencil = {"Pencil", "pencil.ks", "pencil", "pencil@paint", "pencil::paint"};
const ModuleButton kSpray = {"Spray", "spray.ks", "spray", "spray@paint", "spray::paint"};
const ModuleButton kLine = {"Line", "lines.ks", "lines", "lines@paint", "lines::paint"};
const ModuleButton kGray = {"Gray", "gray.ks", "gray", "gray@run", "gray::run"};
const ModuleButton kEdge = {"Edge", "edge.ks", "edge", "edge@run", "edge::run"};
const ModuleButton kThreshold = {"Threshold", "threshold.ks", "threshold", "threshold@run", "threshold::run"};
const ModuleButton kMandel = {"Mandel", "mandel.ks", "mandel", "mandel@run", "mandel::run"};

struct Swatch
{
    const char *label;
    int color;
};

const Swatch kWhite = {"White", kSwatchWhite};
const Swatch kRed = {"Red", kSwatchRed};
const Swatch kGreen = {"Green", kSwatchGreen};
const Swatch kBlue = {"Blue", kSwatchBlue};

std::string g_argv0;
std::string g_scriptsDir;
std::vector<std::string> g_searchPaths;
std::vector<int> g_pixels(kPixels, 0);
std::vector<unsigned char> g_rgb(kPixels * 3, 0);
int g_mouseX = 0;
int g_mouseY = 0;
bool g_mouseDown = false;
int g_penColor = kPenDefault;
std::string g_activeEntry;
std::string g_activeModule = "pencil";
std::string g_activeTool = "Pencil";
int g_loadCount = 0;
int g_paintCalls = 0;
kern::IVirtualMachine *g_vm = nullptr;

Fl_Window *g_window = nullptr;
Fl_Box *g_status = nullptr;
Fl_Button *g_pencilButton = nullptr;
Fl_Button *g_sprayButton = nullptr;
Fl_Button *g_lineButton = nullptr;
Fl_Button *g_grayButton = nullptr;
Fl_Button *g_edgeButton = nullptr;
Fl_Button *g_thresholdButton = nullptr;
Fl_Button *g_mandelButton = nullptr;
Fl_Button *g_clearButton = nullptr;
Fl_Button *g_redButton = nullptr;

class PaintCanvas : public Fl_Widget
{
public:
    PaintCanvas(int x, int y, int w, int h) : Fl_Widget(x, y, w, h) {}

    void draw() override
    {
        fl_push_clip(x(), y(), w(), h());
        fl_draw_image(g_rgb.data(), x(), y(), kCanvasW, kCanvasH, 3);
        fl_pop_clip();
    }

    int handle(int event) override
    {
        if (event == FL_PUSH || event == FL_DRAG)
        {
            g_mouseX = Fl::event_x() - x();
            g_mouseY = Fl::event_y() - y();
            if (g_mouseX < 0)
            {
                g_mouseX = 0;
            }
            if (g_mouseY < 0)
            {
                g_mouseY = 0;
            }
            if (g_mouseX > kCanvasW - 1)
            {
                g_mouseX = kCanvasW - 1;
            }
            if (g_mouseY > kCanvasH - 1)
            {
                g_mouseY = kCanvasH - 1;
            }
            if (event == FL_PUSH)
            {
                g_mouseDown = true;
                startPainting();
            }
            return 1;
        }
        if (event == FL_RELEASE)
        {
            g_mouseDown = false;
            stopPainting();
            return 1;
        }
        return Fl_Widget::handle(event);
    }

    static void startPainting();
    static void stopPainting();
};

PaintCanvas *g_canvasWidget = nullptr;

void tick_cb(void *data);

int canvas_w_value() { return kCanvasW; }
int canvas_h_value() { return kCanvasH; }

int get_pixel_value(int x, int y)
{
    if (x < 0 || y < 0 || x >= kCanvasW || y >= kCanvasH)
    {
        return 0;
    }
    return g_pixels[y * kCanvasW + x];
}

void set_pixel_value(int x, int y, int rgb)
{
    if (x < 0 || y < 0 || x >= kCanvasW || y >= kCanvasH)
    {
        return;
    }
    g_pixels[y * kCanvasW + x] = rgb;
}

int mouse_x_value() { return g_mouseX; }
int mouse_y_value() { return g_mouseY; }
int mouse_down_value() { return g_mouseDown ? 1 : 0; }
int pen_color_value() { return g_penColor; }

void present_value()
{
    for (int i = 0; i < kPixels; ++i)
    {
        const int packed = g_pixels[i];
        g_rgb[i * 3 + 0] = static_cast<unsigned char>((packed >> 16) & 255);
        g_rgb[i * 3 + 1] = static_cast<unsigned char>((packed >> 8) & 255);
        g_rgb[i * 3 + 2] = static_cast<unsigned char>(packed & 255);
    }
    if (g_canvasWidget != nullptr)
    {
        g_canvasWidget->redraw();
    }
}

unsigned int pixelChecksum()
{
    unsigned int hash = 2166136261u;
    for (int packed : g_pixels)
    {
        hash ^= static_cast<unsigned int>(packed & 255);
        hash *= 16777619u;
        hash ^= static_cast<unsigned int>((packed >> 8) & 255);
        hash *= 16777619u;
        hash ^= static_cast<unsigned int>((packed >> 16) & 255);
        hash *= 16777619u;
    }
    return hash;
}

int countNonzero()
{
    int count = 0;
    for (int packed : g_pixels)
    {
        if (packed != 0)
        {
            ++count;
        }
    }
    return count;
}

int countNonzeroIn(int x0, int y0, int x1, int y1)
{
    int count = 0;
    for (int y = y0; y <= y1; ++y)
    {
        for (int x = x0; x <= x1; ++x)
        {
            if (x < 0 || y < 0 || x >= kCanvasW || y >= kCanvasH)
            {
                continue;
            }
            if (g_pixels[y * kCanvasW + x] != 0)
            {
                ++count;
            }
        }
    }
    return count;
}

// Scripts live as separate files under scripts/. Resolve robustly so
// --verify works from the build tree: exe-dir/scripts wins, then
// CWD/scripts, then CWD/Samples/ScriptPaint/scripts (repo-root runs).
// The winning directory is reported; a miss is a loud error, never silent.
bool resolveScriptsDir(std::string &out, int &candidate)
{
    std::vector<std::string> tries;
    try
    {
        const std::filesystem::path exe(g_argv0);
        if (exe.has_parent_path())
        {
            tries.push_back((exe.parent_path() / "scripts").string());
        }
        else
        {
            tries.push_back((std::filesystem::current_path() / "scripts").string());
        }
    }
    catch (const std::filesystem::filesystem_error &)
    {
        tries.push_back((std::filesystem::current_path() / "scripts").string());
    }
    tries.push_back((std::filesystem::current_path() / "scripts").string());
    tries.push_back((std::filesystem::current_path() / "Samples" / "ScriptPaint" / "scripts").string());
    for (std::size_t i = 0; i < tries.size(); ++i)
    {
        std::error_code code;
        if (std::filesystem::is_directory(tries[i], code))
        {
            out = tries[i];
            candidate = static_cast<int>(i);
            return true;
        }
    }
    std::cerr << "scripts dir missing (tried " << tries.size() << " candidates:";
    for (const std::string &t : tries)
    {
        std::cerr << " [" << t << "]";
    }
    std::cerr << ")\n";
    return false;
}

bool findScript(const char *file, std::string &out)
{
    for (const std::string &dir : g_searchPaths)
    {
        const std::string full = (std::filesystem::path(dir) / file).string();
        std::error_code code;
        if (std::filesystem::is_regular_file(full, code))
        {
            out = full;
            return true;
        }
    }
    std::cerr << "script file missing: " << file << " (searched " << g_searchPaths.size() << " dirs)\n";
    return false;
}

std::string buildRootSource(const char *selected, const char *call, const char *resident)
{
    std::string src;
    src += "import ";
    src += selected;
    src += ";\n";
    if (resident != nullptr && std::string(resident) != selected)
    {
        src += "import ";
        src += resident;
        src += ";\n";
    }
    src += "func main() {\n  ";
    src += call;
    src += "();\n}\n";
    return src;
}

bool loadSelection(const ModuleButton &module, const char *resident)
{
    std::string path;
    if (!findScript(module.file, path))
    {
        return false;
    }
    const std::string root = buildRootSource(module.mod, module.call, resident);
    const std::string sourceName = std::string("<paint-root:") + module.label + ">";
    std::stringstream asmCode;
    {
        std::istringstream input(root);
        std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
        if (!compiler->compile(input, asmCode, sourceName, g_searchPaths))
        {
            std::cerr << "compile failed for " << module.label << '\n';
            return false;
        }
    }
    std::stringstream byteCode;
    {
        std::istringstream input(asmCode.str());
        std::unique_ptr<kern::IAssembler> assembler(kern::createAssembler());
        if (!assembler->assemble(input, byteCode))
        {
            std::cerr << "assemble failed for " << module.label << '\n';
            return false;
        }
    }
    try
    {
        if (!g_vm->load(byteCode))
        {
            std::cerr << "load failed for " << module.label << '\n';
            return false;
        }
    }
    catch (const kern::RuntimeError &error)
    {
        std::cerr << "load error: " << error.what() << '\n';
        return false;
    }
    ++g_loadCount;
    std::cout << "module " << module.label << " loaded from " << path;
    if (resident != nullptr && std::string(resident) != module.mod)
    {
        std::cout << " (+ resident " << resident << ")";
    }
    std::cout << " (load #" << g_loadCount << ")\n";
    return true;
}

bool callEntry(const char *entry)
{
    try
    {
        if (!g_vm->callFunction(entry))
        {
            std::cerr << "function " << entry << " not found\n";
            return false;
        }
    }
    catch (const kern::RuntimeError &error)
    {
        std::cerr << "runtime error in " << entry << ": " << error.what() << '\n';
        return false;
    }
    return true;
}

bool paintTick()
{
    if (g_activeEntry.empty())
    {
        std::cerr << "no active tool\n";
        return false;
    }
    if (!callEntry(g_activeEntry.c_str()))
    {
        return false;
    }
    ++g_paintCalls;
    present_value();
    return true;
}

void PaintCanvas::startPainting()
{
    Fl::remove_timeout(tick_cb);
    if (g_activeEntry.empty())
    {
        return;
    }
    paintTick();
    Fl::add_timeout(kTickSeconds, tick_cb);
}

void PaintCanvas::stopPainting() { Fl::remove_timeout(tick_cb); }

void tick_cb(void * /*data*/)
{
    if (!g_mouseDown || g_activeEntry.empty())
    {
        return;
    }
    paintTick();
    Fl::repeat_timeout(kTickSeconds, tick_cb);
}

void refreshUi()
{
    const std::string text = g_activeTool + std::string(" | loads ") + std::to_string(g_loadCount) + " | paints " +
                             std::to_string(g_paintCalls);
    g_status->copy_label(text.c_str());
    if (g_canvasWidget != nullptr)
    {
        g_canvasWidget->redraw();
    }
}

void onTool(Fl_Widget * /*widget*/, void *data)
{
    const ModuleButton *module = static_cast<const ModuleButton *>(data);
    if (!loadSelection(*module, nullptr))
    {
        return;
    }
    g_activeEntry = module->entry;
    g_activeModule = module->mod;
    g_activeTool = module->label;
    refreshUi();
    std::cout << "tool " << module->label << " active (" << module->entry << ")\n";
}

void onFilter(Fl_Widget * /*widget*/, void *data)
{
    const ModuleButton *module = static_cast<const ModuleButton *>(data);
    if (!loadSelection(*module, g_activeModule.c_str()))
    {
        return;
    }
    const auto start = std::chrono::steady_clock::now();
    if (!callEntry(module->entry))
    {
        return;
    }
    const auto end = std::chrono::steady_clock::now();
    const long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    present_value();
    refreshUi();
    std::cout << "filter " << module->label << " ran in " << ms << " ms, checksum " << std::hex << pixelChecksum()
              << std::dec << " nonzero " << countNonzero() << '\n';
}

void onClear(Fl_Widget * /*widget*/, void * /*data*/)
{
    // Host-side zeroing: clearing 64k ints needs no script.
    for (int &packed : g_pixels)
    {
        packed = 0;
    }
    present_value();
    refreshUi();
    std::cout << "clear: checksum " << std::hex << pixelChecksum() << std::dec << '\n';
}

void onSwatch(Fl_Widget * /*widget*/, void *data)
{
    const Swatch *swatch = static_cast<const Swatch *>(data);
    g_penColor = swatch->color;
    refreshUi();
    std::cout << "pen " << swatch->label << " (" << g_penColor << ")\n";
}

void seedPixel(int x, int y, int rgb) { set_pixel_value(x, y, rgb); }

int checkPixel(const char *label, int x, int y, int want)
{
    const int got = get_pixel_value(x, y);
    if (got == want)
    {
        std::cout << label << ": pixel (" << x << "," << y << ") = " << got << " as expected\n";
        return 1;
    }
    std::cout << "MISMATCH " << label << ": pixel (" << x << "," << y << ") = " << got << " want " << want << '\n';
    return 0;
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
    std::cout << "scripts dir: " << g_scriptsDir << '\n';
    std::cout << "canvas: " << canvas_w_value() << "x" << canvas_h_value() << " (" << kPixels << " pixels)\n";

    // Every module file must resolve through the search paths.
    const char *files[] = {"color.ks", "geom.ks", "pencil.ks",    "spray.ks", "lines.ks",
                           "gray.ks",  "edge.ks", "threshold.ks", "mandel.ks"};
    for (const char *file : files)
    {
        std::string path;
        if (!findScript(file, path))
        {
            std::cout << "MISMATCH: module file missing: " << file << '\n';
            ok = false;
        }
        else
        {
            std::cout << "module file: " << file << " -> " << path << '\n';
        }
    }
    const int loadsAtStart = g_loadCount;
    std::cout << "startup loads: " << loadsAtStart << '\n';

    // Clear zeroes the canvas through the REAL button callback.
    g_clearButton->do_callback();
    present_value();
    if (countNonzero() != 0)
    {
        std::cout << "MISMATCH: clear did not zero the canvas (checksum " << std::hex << pixelChecksum() << std::dec
                  << " nonzero " << countNonzero() << ")\n";
        ok = false;
    }
    else
    {
        std::cout << "clear: canvas zeroed (all " << kPixels << " pixels read back 0)\n";
    }

    // Pencil draws through the REAL button callback plus timer ticks.
    g_redButton->do_callback();
    g_mouseX = 10;
    g_mouseY = 10;
    g_mouseDown = true;
    int loads = g_loadCount;
    g_pencilButton->do_callback();
    if (g_loadCount != loads + 1)
    {
        std::cout << "MISMATCH: pencil press did not recompile (loads " << g_loadCount << ")\n";
        ok = false;
    }
    for (int i = 0; i < 3; ++i)
    {
        if (!paintTick())
        {
            ok = false;
        }
    }
    present_value();
    ok &= checkPixel("pencil center", 10, 10, kSwatchRed) != 0;
    const int pencilBlock = countNonzeroIn(9, 9, 11, 11);
    std::cout << "pencil: 3x3 block nonzero = " << pencilBlock << " (want 9)\n";
    if (pencilBlock != 9)
    {
        std::cout << "MISMATCH: pencil 3x3 stamp incomplete\n";
        ok = false;
    }
    g_mouseDown = false;

    // Gray converts with hand-checked math on 3 seeded pixels:
    // red (255,0,0): lum = 255/3 = 85, pack = 85*65536+85*256+85 = 5592405.
    // 0x123456: r = 18, g = 52, b = 86, lum = 156/3 = 52,
    // pack = 52*65536+52*256+52 = 3421236. black stays 0.
    g_clearButton->do_callback();
    seedPixel(5, 5, 16711680);
    seedPixel(6, 5, 1193046);
    seedPixel(7, 5, 0);
    present_value();
    loads = g_loadCount;
    g_grayButton->do_callback();
    if (g_loadCount != loads + 1)
    {
        std::cout << "MISMATCH: gray press did not recompile\n";
        ok = false;
    }
    std::cout << "gray math: lum(255,0,0) = 255/3 = 85 -> pack(85,85,85) = 5592405\n";
    std::cout << "gray math: lum(0x123456) = (18+52+86)/3 = 52 -> pack(52,52,52) = 3421236\n";
    ok &= checkPixel("gray red", 5, 5, 5592405) != 0;
    ok &= checkPixel("gray 0x123456", 6, 5, 3421236) != 0;
    ok &= checkPixel("gray black", 7, 5, 0) != 0;
    const unsigned int grayChecksum = pixelChecksum();
    std::cout << "gray: checksum " << std::hex << grayChecksum << std::dec << '\n';

    // Edge runs the full 5-reads-per-pixel pass to completion.
    loads = g_loadCount;
    g_edgeButton->do_callback();
    if (g_loadCount != loads + 1)
    {
        std::cout << "MISMATCH: edge press did not recompile\n";
        ok = false;
    }
    const unsigned int edgeChecksum = pixelChecksum();
    std::cout << "edge: checksum " << std::hex << edgeChecksum << std::dec << " nonzero " << countNonzero() << '\n';
    if (edgeChecksum == grayChecksum)
    {
        std::cout << "MISMATCH: edge left the canvas unchanged\n";
        ok = false;
    }

    // Threshold on fresh seeds: white (lum 255 > 128) stays 16777215,
    // red (lum 85) and black go 0.
    g_clearButton->do_callback();
    seedPixel(5, 5, 16777215);
    seedPixel(6, 5, 16711680);
    seedPixel(7, 5, 0);
    present_value();
    g_thresholdButton->do_callback();
    ok &= checkPixel("threshold white", 5, 5, 16777215) != 0;
    ok &= checkPixel("threshold red", 6, 5, 0) != 0;
    ok &= checkPixel("threshold black", 7, 5, 0) != 0;

    // Spray splatters near the cursor; line stamps a diagonal.
    g_mouseX = 100;
    g_mouseY = 100;
    g_mouseDown = true;
    g_sprayButton->do_callback();
    for (int i = 0; i < 2; ++i)
    {
        if (!paintTick())
        {
            ok = false;
        }
    }
    const int sprayCount = countNonzeroIn(97, 97, 103, 103);
    std::cout << "spray: nonzero in 7x7 = " << sprayCount << '\n';
    if (sprayCount <= 0)
    {
        std::cout << "MISMATCH: spray painted nothing\n";
        ok = false;
    }
    g_mouseDown = false;
    g_mouseX = 200;
    g_mouseY = 150;
    g_mouseDown = true;
    g_lineButton->do_callback();
    if (!paintTick())
    {
        ok = false;
    }
    g_mouseDown = false;
    ok &= checkPixel("line anchor", 200, 150, kSwatchRed) != 0;
    ok &= checkPixel("line +2", 202, 152, kSwatchRed) != 0;
    ok &= checkPixel("line -2", 198, 148, kSwatchRed) != 0;

    // Mandel must complete within budget and be deterministic: press twice
    // (reload + run each time) and require identical checksums. Shade runs
    // 8..248, so every pixel is nonzero on full coverage.
    g_mandelButton->do_callback();
    const unsigned int mandelFirst = pixelChecksum();
    const int mandelNonzero = countNonzero();
    std::cout << "mandel run 1: checksum " << std::hex << mandelFirst << std::dec << " nonzero " << mandelNonzero
              << '\n';
    g_mandelButton->do_callback();
    const unsigned int mandelSecond = pixelChecksum();
    std::cout << "mandel run 2: checksum " << std::hex << mandelSecond << std::dec << '\n';
    if (mandelFirst != mandelSecond)
    {
        std::cout << "MISMATCH: mandel is not deterministic\n";
        ok = false;
    }
    if (mandelNonzero != kPixels)
    {
        std::cout << "MISMATCH: mandel left " << (kPixels - mandelNonzero) << " pixels unwritten\n";
        ok = false;
    }
    std::cout << "MANDEL CHECKSUM " << std::hex << mandelFirst << std::dec << " RECORDED\n";

    // Hot-reload: pressing Pencil again recompiles from disk (load +1) and
    // the fresh program still paints.
    loads = g_loadCount;
    g_pencilButton->do_callback();
    if (g_loadCount != loads + 1)
    {
        std::cout << "MISMATCH: hot-reload press did not recompile\n";
        ok = false;
    }
    else
    {
        std::cout << "hot-reload: pencil recompiled from disk (load #" << g_loadCount << ")\n";
    }
    g_mouseX = 20;
    g_mouseY = 20;
    g_mouseDown = true;
    if (!paintTick())
    {
        ok = false;
    }
    g_mouseDown = false;
    ok &= checkPixel("reloaded pencil", 20, 20, kSwatchRed) != 0;

    // Diamond: one anonymous root importing gray and edge, which both
    // import color. Shared-once proof is success itself (a double load of
    // color.ks would fail the merge with duplicate definitions).
    {
        std::istringstream input(kDiamondSource);
        std::stringstream asmCode;
        std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
        loads = g_loadCount;
        if (!compiler->compile(input, asmCode, std::string("<verify-diamond>"), g_searchPaths))
        {
            std::cout << "MISMATCH: diamond root failed to compile\n";
            ok = false;
        }
        else
        {
            std::istringstream asmInput(asmCode.str());
            std::unique_ptr<kern::IAssembler> assembler(kern::createAssembler());
            std::stringstream byteCode;
            if (!assembler->assemble(asmInput, byteCode))
            {
                std::cout << "MISMATCH: diamond root failed to assemble\n";
                ok = false;
            }
            else
            {
                try
                {
                    if (!g_vm->load(byteCode))
                    {
                        std::cout << "MISMATCH: diamond root failed to load\n";
                        ok = false;
                    }
                    else
                    {
                        ++g_loadCount;
                        const auto start = std::chrono::steady_clock::now();
                        const bool ran = callEntry("main");
                        const auto end = std::chrono::steady_clock::now();
                        const long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
                        present_value();
                        std::cout << "diamond gray+edge via color: " << (ran ? "ran" : "FAILED") << " in " << ms
                                  << " ms, checksum " << std::hex << pixelChecksum() << std::dec << '\n';
                        if (!ran)
                        {
                            ok = false;
                        }
                    }
                }
                catch (const kern::RuntimeError &error)
                {
                    std::cout << "MISMATCH: diamond load error: " << error.what() << '\n';
                    ok = false;
                }
            }
        }
        if (g_loadCount != loads + 1)
        {
            std::cout << "MISMATCH: diamond load did not register\n";
            ok = false;
        }
    }

    // Timer path: with the button held, Fl::add_timeout ticks must fire
    // headless and drive paint calls, then stop when released.
    g_mouseX = 30;
    g_mouseY = 30;
    g_mouseDown = true;
    PaintCanvas::startPainting();
    const int paintsBefore = g_paintCalls;
    const auto pumpStart = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - pumpStart < std::chrono::milliseconds(1200))
    {
        Fl::wait(0.1);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    g_mouseDown = false;
    PaintCanvas::stopPainting();
    const int paintsAfter = g_paintCalls;
    std::cout << "timer: paints " << paintsBefore << " -> " << paintsAfter << " while held\n";
    if (paintsAfter <= paintsBefore)
    {
        std::cout << "MISMATCH: timer did not drive painting\n";
        ok = false;
    }
    for (int i = 0; i < 3; ++i)
    {
        Fl::wait(0.2);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (g_paintCalls != paintsAfter)
    {
        std::cout << "MISMATCH: timer kept painting after release\n";
        ok = false;
    }
    ok &= checkPixel("timer paint", 30, 30, kSwatchRed) != 0;

    std::cout << "vm loads: " << g_loadCount << " paint calls: " << g_paintCalls << '\n';
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

    // Per-pixel script loops cost millions of steps (Mandel: 64k pixels x
    // up to 30 iterations plus an extern call per pixel), far past the
    // default 10M budget. Raise the ceiling explicitly; runaway scripts
    // still die on it, just later.
    kern::VMConfig config;
    config.maxExecutionSteps = 200000000u;
    try
    {
        std::unique_ptr<kern::IVirtualMachine> vm(kern::createVirtualMachine(config));
        vm->addFunction("canvas_w", kern::createExternFunction(&canvas_w_value));
        vm->addFunction("canvas_h", kern::createExternFunction(&canvas_h_value));
        vm->addFunction("get_pixel", kern::createExternFunction(&get_pixel_value));
        vm->addFunction("set_pixel", kern::createExternFunction(&set_pixel_value));
        vm->addFunction("mouse_x", kern::createExternFunction(&mouse_x_value));
        vm->addFunction("mouse_y", kern::createExternFunction(&mouse_y_value));
        vm->addFunction("mouse_down", kern::createExternFunction(&mouse_down_value));
        vm->addFunction("pen_color", kern::createExternFunction(&pen_color_value));
        vm->addFunction("present", kern::createExternFunction(&present_value));
        g_vm = vm.get();

        int candidate = -1;
        if (!resolveScriptsDir(g_scriptsDir, candidate))
        {
            return 1;
        }
        g_searchPaths.push_back(g_scriptsDir);
        g_searchPaths.push_back((std::filesystem::path(g_scriptsDir) / "lib").string());
        g_searchPaths.push_back((std::filesystem::path(g_scriptsDir) / "tools").string());
        g_searchPaths.push_back((std::filesystem::path(g_scriptsDir) / "filters").string());
        g_searchPaths.push_back((std::filesystem::path(g_scriptsDir) / "gen").string());
        std::cout << "scripts dir: " << g_scriptsDir << " (candidate #" << candidate << ")\n";

        Fl_Window window(340, 382, kWindowTitle);
        g_window = &window;
        PaintCanvas canvas(10, 10, kCanvasW, kCanvasH);
        g_canvasWidget = &canvas;
        Fl_Button pencilButton(10, 218, 100, 28, "Pencil");
        pencilButton.callback(onTool, const_cast<ModuleButton *>(&kPencil));
        g_pencilButton = &pencilButton;
        Fl_Button sprayButton(115, 218, 100, 28, "Spray");
        sprayButton.callback(onTool, const_cast<ModuleButton *>(&kSpray));
        g_sprayButton = &sprayButton;
        Fl_Button lineButton(220, 218, 100, 28, "Line");
        lineButton.callback(onTool, const_cast<ModuleButton *>(&kLine));
        g_lineButton = &lineButton;
        Fl_Button grayButton(10, 250, 100, 28, "Gray");
        grayButton.callback(onFilter, const_cast<ModuleButton *>(&kGray));
        g_grayButton = &grayButton;
        Fl_Button edgeButton(115, 250, 100, 28, "Edge");
        edgeButton.callback(onFilter, const_cast<ModuleButton *>(&kEdge));
        g_edgeButton = &edgeButton;
        Fl_Button thresholdButton(220, 250, 100, 28, "Thresh");
        thresholdButton.callback(onFilter, const_cast<ModuleButton *>(&kThreshold));
        g_thresholdButton = &thresholdButton;
        Fl_Button mandelButton(10, 282, 100, 28, "Mandel");
        mandelButton.callback(onFilter, const_cast<ModuleButton *>(&kMandel));
        g_mandelButton = &mandelButton;
        Fl_Button clearButton(115, 282, 100, 28, "Clear");
        clearButton.callback(onClear);
        g_clearButton = &clearButton;
        Fl_Button whiteButton(10, 314, 76, 28, "White");
        whiteButton.callback(onSwatch, const_cast<Swatch *>(&kWhite));
        Fl_Button redButton(90, 314, 76, 28, "Red");
        redButton.callback(onSwatch, const_cast<Swatch *>(&kRed));
        g_redButton = &redButton;
        Fl_Button greenButton(170, 314, 76, 28, "Green");
        greenButton.callback(onSwatch, const_cast<Swatch *>(&kGreen));
        Fl_Button blueButton(250, 314, 70, 28, "Blue");
        blueButton.callback(onSwatch, const_cast<Swatch *>(&kBlue));
        Fl_Box status(10, 348, 320, 24, "Pencil | loads 0 | paints 0");
        g_status = &status;
        window.end();
        present_value();

        // Startup tool so painting works before any button press.
        if (!loadSelection(kPencil, nullptr))
        {
            return 1;
        }
        g_activeEntry = kPencil.entry;
        g_activeModule = kPencil.mod;
        g_activeTool = kPencil.label;
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
