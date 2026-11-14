// Canvas: a scriptable PNG image editor. The host owns the document, bounded
// undo history, PNG storage, preview transactions, and FLTK presentation.
// Scripts implement brushes, filters, and procedural images over documents
// up to 2048 x 2048. Larger images are processed in scheduled row bands.
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
#include "../../Support/Preview.h"
#include <algorithm>
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
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Color_Chooser.H>
#include <FL/Fl_File_Chooser.H>
#include <FL/Fl_Value_Slider.H>
#include <FL/Fl_Widget.H>
#include <FL/Fl_Window.H>
#include <FL/fl_ask.H>
#include <FL/fl_draw.H>

#include "Document.h"
#include "ImageFiles.h"
#include <kern/Kern.h>

namespace
{

constexpr int kCanvasW = 320;
constexpr int kCanvasH = 200;
constexpr int kPixels = kCanvasW * kCanvasH;
constexpr double kTickSeconds = 0.05;
constexpr int kBandRows = 20;
constexpr const char *kWindowTitle = "Canvas | KernScript";
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
extern canvas_w() -> value;
extern canvas_h() -> value;
extern get_pixel(x, y) -> value;
extern set_pixel(x, y, rgb) -> void;
func main() -> void {
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
    const char *bandEntry;
    const char *bandCall;
};

const ModuleButton kPencil = {"Pencil", "pencil.ks", "pencil", "pencil@paint", "pencil::paint", nullptr, nullptr};
const ModuleButton kSpray = {"Spray", "spray.ks", "spray", "spray@paint", "spray::paint", nullptr, nullptr};
const ModuleButton kLine = {"Line", "lines.ks", "lines", "lines@paint", "lines::paint", nullptr, nullptr};
const ModuleButton kGray = {"Gray", "gray.ks", "gray", "gray@run", "gray::run", "gray@run_band", "gray::run_band"};
const ModuleButton kEdge = {"Edge", "edge.ks", "edge", "edge@run", "edge::run", "edge@run_band", "edge::run_band"};
const ModuleButton kThreshold = {"Threshold",      "threshold.ks",       "threshold",          "threshold@run",
                                 "threshold::run", "threshold@run_band", "threshold::run_band"};
const ModuleButton kMandel = {"Mandel",      "mandel.ks",       "mandel",          "mandel@run",
                              "mandel::run", "mandel@run_band", "mandel::run_band"};

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
canvas::Document g_document;
std::vector<int> &g_pixels = g_document.image().pixels;
std::vector<unsigned char> g_rgb(kPixels * 3, 0);
std::string g_imagePath;
std::string g_lastError;
int g_brushRadius = 1;
int g_opacity = 100;
int g_threshold = 128;
bool g_preview = true;
bool g_previewReady = false;
const ModuleButton *g_previewModule = nullptr;
int g_mouseX = 0;
int g_mouseY = 0;
int g_strokeX = 0;
int g_strokeY = 0;
bool g_mouseDown = false;
int g_penColor = kPenDefault;
std::string g_activeEntry;
std::string g_activeModule = "pencil";
std::string g_activeTool = "Pencil";
int g_loadCount = 0;
int g_paintCalls = 0;
kern::IVirtualMachine *g_vm = nullptr;

// Banded run state: filter/generator buttons compile+load as before, then
// the host drives run_band() band-by-band across Fl timer ticks, pumping
// between bands so the window stays responsive. Bands are row ranges
// [band_lo(), band_hi()); scripts pull the bounds (callFunction takes no
// args, same as canvas_w/h). Determinism holds because bands run in row
// order over the same in-place buffer as the whole-canvas run().
int g_bandLo = 0;
int g_bandHi = 0;
bool g_bandActive = false;
const ModuleButton *g_bandModule = nullptr;
int g_bandNextRow = 0;
int g_bandRowsPerTick = kBandRows;
int g_bandTicks = 0;
std::chrono::steady_clock::time_point g_bandStart;

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
Fl_Button *g_cancelButton = nullptr;
Fl_Button *g_redButton = nullptr;

class PaintCanvas : public Fl_Widget
{
public:
    PaintCanvas(int x, int y, int w, int h) : Fl_Widget(x, y, w, h) {}

    void draw() override
    {
        fl_push_clip(x(), y(), w(), h());
        fl_color(FL_DARK3);
        fl_rectf(x(), y(), w(), h());
        Fl_RGB_Image image(g_rgb.data(), g_document.image().width, g_document.image().height, 3);
        const double scale = std::min(static_cast<double>(w()) / image.w(), static_cast<double>(h()) / image.h());
        std::unique_ptr<Fl_Image> fitted(image.copy(std::max(1, static_cast<int>(image.w() * scale)),
                                                    std::max(1, static_cast<int>(image.h() * scale))));
        fitted->draw(x(), y());
        fl_pop_clip();
    }

    int handle(int event) override
    {
        if (event == FL_PUSH || event == FL_DRAG)
        {
            if (g_bandActive || (g_document.editing() && !g_mouseDown))
                return 1;
            const double scale = std::min(static_cast<double>(w()) / g_document.image().width,
                                          static_cast<double>(h()) / g_document.image().height);
            g_mouseX = static_cast<int>((Fl::event_x() - x()) / scale);
            g_mouseY = static_cast<int>((Fl::event_y() - y()) / scale);
            if (g_mouseX < 0)
            {
                g_mouseX = 0;
            }
            if (g_mouseY < 0)
            {
                g_mouseY = 0;
            }
            if (g_mouseX > g_document.image().width - 1)
            {
                g_mouseX = g_document.image().width - 1;
            }
            if (g_mouseY > g_document.image().height - 1)
            {
                g_mouseY = g_document.image().height - 1;
            }
            if (event == FL_PUSH)
            {
                g_strokeX = g_mouseX;
                g_strokeY = g_mouseY;
                g_mouseDown = true;
                startPainting();
            }
            return 1;
        }
        if (event == FL_RELEASE)
        {
            if (g_mouseDown && g_activeModule == "lines")
                paintTickFinal();
            g_mouseDown = false;
            stopPainting();
            return 1;
        }
        return Fl_Widget::handle(event);
    }

    static void startPainting();
    static void stopPainting();
    static void paintTickFinal();
};

PaintCanvas *g_canvasWidget = nullptr;

void tick_cb(void *data);
void refreshUi();

int canvas_w_value() { return g_document.image().width; }
int canvas_h_value() { return g_document.image().height; }
int brush_radius_value() { return g_brushRadius; }
int filter_threshold_value() { return g_threshold; }
int band_lo_value() { return g_bandLo; }
int band_hi_value() { return g_bandHi; }

int get_pixel_value(int x, int y)
{
    if (x < 0 || y < 0 || x >= canvas_w_value() || y >= canvas_h_value())
    {
        return 0;
    }
    return g_pixels[y * canvas_w_value() + x];
}

void set_pixel_value(int x, int y, int rgb)
{
    if (x < 0 || y < 0 || x >= canvas_w_value() || y >= canvas_h_value())
    {
        return;
    }
    g_pixels[y * canvas_w_value() + x] = rgb & 0xffffff;
}

void dab_pixel_value(int x, int y, int rgb)
{
    const int old = get_pixel_value(x, y);
    const int r = (((rgb >> 16) & 255) * g_opacity + ((old >> 16) & 255) * (100 - g_opacity)) / 100;
    const int g = (((rgb >> 8) & 255) * g_opacity + ((old >> 8) & 255) * (100 - g_opacity)) / 100;
    const int b = ((rgb & 255) * g_opacity + (old & 255) * (100 - g_opacity)) / 100;
    set_pixel_value(x, y, r * 65536 + g * 256 + b);
}

int mouse_x_value() { return g_mouseX; }
int mouse_y_value() { return g_mouseY; }
int stroke_x_value() { return g_strokeX; }
int stroke_y_value() { return g_strokeY; }
int mouse_down_value() { return g_mouseDown ? 1 : 0; }
int pen_color_value() { return g_penColor; }

void present_value()
{
    g_rgb.resize(g_pixels.size() * 3);
    for (std::size_t i = 0; i < g_pixels.size(); ++i)
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
// CWD/scripts, then CWD/Samples/Canvas/scripts (repo-root runs).
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
    tries.push_back((std::filesystem::current_path() / "Samples" / "Canvas" / "scripts").string());
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
    src += "func main() -> void {\n  ";
    src += call;
    src += "();\n}\n";
    return src;
}

class AppLogger : public kern::ILogger
{
    void onError(const std::string &message) override
    {
        g_lastError += message + "\n";
        std::cerr << message << '\n';
    }
};
AppLogger g_logger;
std::unique_ptr<kern::IVirtualMachine> g_vmOwner;

std::unique_ptr<kern::IVirtualMachine> createBoundVm()
{
    kern::VMConfig config;
    config.maxExecutionSteps = 200000000u;
    std::unique_ptr<kern::IVirtualMachine> vm(kern::createVirtualMachine(&g_logger, config));
    vm->addFunction("canvas_w", &canvas_w_value);
    vm->addFunction("canvas_h", &canvas_h_value);
    vm->addFunction("band_lo", &band_lo_value);
    vm->addFunction("band_hi", &band_hi_value);
    vm->addFunction("get_pixel", &get_pixel_value);
    vm->addFunction("set_pixel", &set_pixel_value);
    vm->addFunction("dab_pixel", &dab_pixel_value);
    vm->addFunction("brush_radius", &brush_radius_value);
    vm->addFunction("filter_threshold", &filter_threshold_value);
    vm->addFunction("mouse_x", &mouse_x_value);
    vm->addFunction("mouse_y", &mouse_y_value);
    vm->addFunction("stroke_x", &stroke_x_value);
    vm->addFunction("stroke_y", &stroke_y_value);
    vm->addFunction("mouse_down", &mouse_down_value);
    vm->addFunction("pen_color", &pen_color_value);
    vm->addFunction("present", &present_value);
    return vm;
}

bool loadSelection(const ModuleButton &module, const char *resident)
{
    g_lastError.clear();
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
        std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler(&g_logger));
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
        auto candidate = createBoundVm();
        if (!candidate->load(byteCode))
        {
            std::cerr << "load failed for " << module.label << '\n';
            return false;
        }
        g_vmOwner = std::move(candidate);
        g_vm = g_vmOwner.get();
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
    g_lastError.clear();
    try
    {
        if (!g_vm->callFunction(entry))
        {
            g_lastError = std::string("Function not found: ") + entry;
            std::cerr << "function " << entry << " not found\n";
            return false;
        }
    }
    catch (const kern::RuntimeError &error)
    {
        g_lastError = error.what();
        std::cerr << "runtime error in " << entry << ": " << error.what() << '\n';
        return false;
    }
    return true;
}

void showExecutionError()
{
    if (g_window && g_window->shown())
        fl_alert("The script could not finish. The edit was cancelled.\n%s", g_lastError.c_str());
}

bool paintTick()
{
    if (g_activeEntry.empty())
    {
        std::cerr << "no active tool\n";
        return false;
    }
    if (g_activeModule == "lines" && g_document.editing())
        g_document.restartPreview();
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
    if (!g_document.editing())
        g_document.beginEdit();
    if (!paintTick())
    {
        g_document.cancelEdit();
        g_mouseDown = false;
        present_value();
        refreshUi();
        showExecutionError();
        return;
    }
    Fl::add_timeout(kTickSeconds, tick_cb);
}

void PaintCanvas::paintTickFinal()
{
    if (!paintTick())
    {
        g_document.cancelEdit();
        present_value();
        showExecutionError();
    }
}

void PaintCanvas::stopPainting()
{
    Fl::remove_timeout(tick_cb);
    if (!g_bandActive && !g_previewReady)
        g_document.commitEdit();
    refreshUi();
}

void tick_cb(void * /*data*/)
{
    if (!g_mouseDown || g_activeEntry.empty())
    {
        return;
    }
    if (!paintTick())
    {
        g_mouseDown = false;
        g_document.cancelEdit();
        present_value();
        refreshUi();
        showExecutionError();
        return;
    }
    Fl::repeat_timeout(kTickSeconds, tick_cb);
}

void band_tick_cb(void *data);

void updateBandButtons()
{
    if (g_grayButton != nullptr)
    {
        if (g_bandActive)
        {
            g_grayButton->deactivate();
        }
        else
        {
            g_grayButton->activate();
        }
    }
    if (g_edgeButton != nullptr)
    {
        if (g_bandActive)
        {
            g_edgeButton->deactivate();
        }
        else
        {
            g_edgeButton->activate();
        }
    }
    if (g_thresholdButton != nullptr)
    {
        if (g_bandActive)
        {
            g_thresholdButton->deactivate();
        }
        else
        {
            g_thresholdButton->activate();
        }
    }
    if (g_mandelButton != nullptr)
    {
        if (g_bandActive)
        {
            g_mandelButton->deactivate();
        }
        else
        {
            g_mandelButton->activate();
        }
    }
    if (g_cancelButton != nullptr)
    {
        if (g_bandActive || g_previewReady)
        {
            g_cancelButton->activate();
        }
        else
        {
            g_cancelButton->deactivate();
        }
    }
}

void refreshUi()
{
    if (g_status == nullptr)
        return;
    if (g_bandActive && g_bandModule != nullptr)
    {
        const std::string text = std::string(g_bandModule->label) + " rows " + std::to_string(g_bandNextRow) + "/" +
                                 std::to_string(canvas_h_value());
        g_status->copy_label(text.c_str());
    }
    else
    {
        const std::string text = g_previewReady ? "Preview ready. Apply to keep it, or Cancel to restore."
                                                : g_activeTool + " | " + std::to_string(canvas_w_value()) + " x " +
                                                      std::to_string(canvas_h_value()) +
                                                      (g_document.dirty() ? " | Unsaved changes" : " | Saved");
        g_status->copy_label(text.c_str());
    }
    if (g_window)
    {
        const auto filename = g_imagePath.empty() ? "Untitled" : std::filesystem::path(g_imagePath).filename().string();
        const auto title = std::string(kWindowTitle) + " | " + filename + (g_document.dirty() ? " *" : "");
        g_window->copy_label(title.c_str());
    }
    if (g_canvasWidget != nullptr)
    {
        g_canvasWidget->redraw();
    }
}

void finishBandRun(bool completed)
{
    Fl::remove_timeout(band_tick_cb);
    const auto end = std::chrono::steady_clock::now();
    const long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - g_bandStart).count();
    const char *label = (g_bandModule != nullptr) ? g_bandModule->label : "?";
    if (completed)
    {
        if (g_preview)
            g_previewReady = true;
        else
            g_document.commitEdit();
        present_value();
        std::cout << "filter " << label << " banded ran in " << ms << " ms over " << g_bandTicks << " ticks, checksum "
                  << std::hex << pixelChecksum() << std::dec << " nonzero " << countNonzero() << '\n';
    }
    else
    {
        g_document.cancelEdit();
        g_previewReady = false;
        present_value();
        std::cout << "filter " << label << " cancelled at rows " << g_bandNextRow << "/" << canvas_h_value() << '\n';
    }
    g_bandActive = false;
    g_bandModule = nullptr;
    g_bandNextRow = 0;
    updateBandButtons();
    refreshUi();
}

void cancelBandRun()
{
    if (g_previewReady)
    {
        g_document.cancelEdit();
        g_previewReady = false;
        g_previewModule = nullptr;
        present_value();
        updateBandButtons();
        refreshUi();
    }
    if (!g_bandActive)
    {
        return;
    }
    finishBandRun(false);
}

bool runBandStep()
{
    if (!g_bandActive || g_bandModule == nullptr)
    {
        return false;
    }
    int lo = g_bandNextRow;
    int hi = lo + g_bandRowsPerTick;
    if (hi > canvas_h_value())
    {
        hi = canvas_h_value();
    }
    g_bandLo = lo;
    g_bandHi = hi;
    if (!callEntry(g_bandModule->bandEntry))
    {
        finishBandRun(false);
        showExecutionError();
        return false;
    }
    g_bandNextRow = hi;
    ++g_bandTicks;
    present_value();
    refreshUi();
    return g_bandNextRow < canvas_h_value();
}

void band_tick_cb(void * /*data*/)
{
    if (!g_bandActive)
    {
        return;
    }
    if (runBandStep())
    {
        Fl::repeat_timeout(kTickSeconds, band_tick_cb);
    }
    else if (g_bandActive)
    {
        finishBandRun(true);
    }
}

void startBandRun(const ModuleButton *module)
{
    g_document.beginEdit();
    g_previewReady = false;
    g_previewModule = module;
    g_bandModule = module;
    g_bandNextRow = 0;
    g_bandLo = 0;
    g_bandHi = 0;
    g_bandTicks = 0;
    g_bandStart = std::chrono::steady_clock::now();
    g_bandActive = true;
    updateBandButtons();
    refreshUi();
    std::cout << "filter " << module->label << " banded start (" << canvas_h_value() << " rows, " << g_bandRowsPerTick
              << "/tick)\n";
    Fl::remove_timeout(band_tick_cb);
    Fl::add_timeout(kTickSeconds, band_tick_cb);
}

int escHandler(int event)
{
    if (event == FL_SHORTCUT && Fl::event_key() == FL_Escape && (g_bandActive || g_previewReady))
    {
        cancelBandRun();
        return 1;
    }
    return 0;
}

void onTool(Fl_Widget * /*widget*/, void *data)
{
    if (g_bandActive || g_previewReady)
    {
        cancelBandRun();
    }
    const ModuleButton *module = static_cast<const ModuleButton *>(data);
    if (!loadSelection(*module, nullptr))
    {
        if (g_window && g_window->shown())
            fl_alert("Could not load tool. Your previous tool is still available.\n%s", g_lastError.c_str());
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
    if (g_bandActive)
    {
        std::cout << "filter busy, ignoring press\n";
        return;
    }
    cancelBandRun();
    const ModuleButton *module = static_cast<const ModuleButton *>(data);
    if (module->bandEntry == nullptr || module->bandCall == nullptr)
    {
        return;
    }
    if (!loadSelection(*module, g_activeModule.c_str()))
    {
        if (g_window && g_window->shown())
            fl_alert("Could not load filter.\n%s", g_lastError.c_str());
        return;
    }
    startBandRun(module);
}

void onCancel(Fl_Widget * /*widget*/, void * /*data*/) { cancelBandRun(); }

void onClear(Fl_Widget * /*widget*/, void * /*data*/)
{
    cancelBandRun();
    g_document.beginEdit();
    // Host-side zeroing: clearing 64k ints needs no script.
    for (int &packed : g_pixels)
    {
        packed = 0;
    }
    g_document.commitEdit();
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

void onColor(Fl_Widget *, void *)
{
    unsigned char red = static_cast<unsigned char>((g_penColor >> 16) & 255);
    unsigned char green = static_cast<unsigned char>((g_penColor >> 8) & 255);
    unsigned char blue = static_cast<unsigned char>(g_penColor & 255);
    if (fl_color_chooser("Brush color", red, green, blue))
    {
        g_penColor = red * 65536 + green * 256 + blue;
        refreshUi();
    }
}

void onApply(Fl_Widget *, void *)
{
    if (!g_previewReady)
        return;
    g_document.commitEdit();
    g_previewReady = false;
    g_previewModule = nullptr;
    updateBandButtons();
    refreshUi();
}

void onUndo(Fl_Widget *, void *)
{
    cancelBandRun();
    g_document.undo();
    present_value();
    refreshUi();
}

void onRedo(Fl_Widget *, void *)
{
    cancelBandRun();
    g_document.redo();
    present_value();
    refreshUi();
}

bool saveImage(bool saveAs)
{
    if (g_document.editing())
    {
        fl_alert("Apply or cancel the current preview before saving.");
        return false;
    }
    std::string path = g_imagePath;
    if (saveAs || path.empty())
    {
        const char *chosen = fl_file_chooser("Save image as PNG", "*.png", path.empty() ? "artwork.png" : path.c_str());
        if (!chosen)
            return false;
        path = chosen;
        if (std::filesystem::path(path).extension() != ".png")
            path += ".png";
        if (std::filesystem::exists(path) &&
            fl_choice("Replace the existing image?", "Cancel", "Replace", nullptr) != 1)
            return false;
    }
    try
    {
        canvas::writePng(path, g_document.image());
        g_imagePath = path;
        g_document.markSaved();
        refreshUi();
        return true;
    }
    catch (const std::exception &error)
    {
        fl_alert("Could not save image.\n%s", error.what());
        return false;
    }
}

bool confirmDiscard()
{
    cancelBandRun();
    if (!g_document.dirty())
        return true;
    const int choice = fl_choice("Save changes to the current image?", "Cancel", "Save", "Discard");
    return choice == 2 || (choice == 1 && saveImage(false));
}

void onSave(Fl_Widget *, void *) { saveImage(false); }
void onSaveAs(Fl_Widget *, void *) { saveImage(true); }

void onOpen(Fl_Widget *, void *)
{
    const char *chosen = fl_file_chooser("Open PNG image", "*.png", "");
    if (!chosen)
        return;
    const std::string path(chosen);
    try
    {
        auto image = canvas::readPng(path);
        if (!confirmDiscard())
            return;
        g_document.replace(std::move(image), true);
        g_imagePath = path;
        present_value();
        refreshUi();
    }
    catch (const std::exception &error)
    {
        fl_alert("Could not open image.\n%s", error.what());
    }
}

void onNew(Fl_Widget *, void *data)
{
    if (!confirmDiscard())
        return;
    const int preset = static_cast<Fl_Choice *>(data)->value();
    const int width = preset == 0 ? 320 : preset == 1 ? 640 : 960;
    g_document.replace(canvas::Image::blank(width, width * 5 / 8), false);
    g_imagePath.clear();
    present_value();
    refreshUi();
}

void onClose(Fl_Widget *, void *)
{
    if (confirmDiscard())
        g_window->hide();
}

void onBrushRadius(Fl_Widget *widget, void *)
{
    g_brushRadius = static_cast<int>(static_cast<Fl_Value_Slider *>(widget)->value());
}

void onOpacity(Fl_Widget *widget, void *)
{
    g_opacity = static_cast<int>(static_cast<Fl_Value_Slider *>(widget)->value());
}

void onThreshold(Fl_Widget *widget, void *)
{
    g_threshold = static_cast<int>(static_cast<Fl_Value_Slider *>(widget)->value());
    if (g_previewReady && g_previewModule == &kThreshold)
    {
        cancelBandRun();
        onFilter(nullptr, const_cast<ModuleButton *>(&kThreshold));
    }
}

void onPreview(Fl_Widget *widget, void *)
{
    g_preview = static_cast<Fl_Check_Button *>(widget)->value() != 0;
    if (!g_preview && g_previewReady)
        onApply(nullptr, nullptr);
}

int runHeadlessVerify(const std::filesystem::path &directory)
{
    auto require = [](bool condition, const char *message)
    {
        if (!condition)
            throw std::runtime_error(message);
    };
    std::filesystem::create_directories(directory);
    g_document.replace(canvas::Image::blank(640, 400), true);
    g_preview = false;
    require(loadSelection(kPencil, nullptr), "Pencil failed to load");
    g_activeEntry = kPencil.entry;
    g_mouseX = 500;
    g_mouseY = 300;
    g_mouseDown = true;
    g_penColor = 0xff0000;
    g_brushRadius = 3;
    g_opacity = 50;
    g_document.beginEdit();
    require(paintTick(), "Pencil failed to paint");
    g_mouseDown = false;
    require(g_document.commitEdit(), "Stroke made no change");
    require(get_pixel_value(500, 300) == 0x7f0000, "Brush opacity or large-image coordinates failed");
    require(get_pixel_value(503, 300) == 0x7f0000, "Brush radius failed");
    require(g_document.undo(), "Stroke undo failed");
    require(get_pixel_value(500, 300) == 0, "Undo did not restore pixels");
    require(g_document.redo(), "Stroke redo failed");

    require(loadSelection(kLine, nullptr), "Line tool failed to load");
    g_activeEntry = kLine.entry;
    g_activeModule = kLine.mod;
    g_strokeX = 480;
    g_strokeY = 280;
    g_mouseX = 520;
    g_mouseY = 320;
    g_document.beginEdit();
    require(paintTick(), "Line preview failed");
    require(get_pixel_value(520, 320) == 0x7f0000, "Line endpoint not painted");
    g_mouseX = 520;
    g_mouseY = 280;
    require(paintTick(), "Changed line preview failed");
    require(get_pixel_value(520, 320) == 0, "Old line preview left pixels behind");
    require(get_pixel_value(520, 280) == 0x7f0000, "Horizontal line failed");
    g_document.cancelEdit();
    require(loadSelection(kPencil, nullptr), "Pencil reload failed");
    g_activeEntry = kPencil.entry;
    g_activeModule = kPencil.mod;

    const auto original = g_document.image();
    require(loadSelection(kThreshold, "pencil"), "Threshold failed to load");
    g_document.beginEdit();
    g_bandLo = 0;
    g_bandHi = canvas_h_value();
    g_threshold = 30;
    require(callEntry(kThreshold.bandEntry), "Threshold preview failed");
    require(get_pixel_value(500, 300) == 0xffffff, "Threshold parameter ignored");
    g_document.cancelEdit();
    require(g_document.image() == original, "Cancelled preview changed pixels");

    const auto png = directory / "roundtrip.png";
    canvas::writePng(png, g_document.image());
    require(canvas::readPng(png) == g_document.image(), "PNG round trip changed pixels or dimensions");
    canvas::writePng(png, g_document.image());
    require(canvas::readPng(png) == original, "PNG overwrite failed");
    const auto directoryTarget = directory / "not-an-image.png";
    std::filesystem::create_directories(directoryTarget);
    bool saveFailed = false;
    try
    {
        canvas::writePng(directoryTarget, g_document.image());
    }
    catch (const std::exception &)
    {
        saveFailed = true;
    }
    require(saveFailed && std::filesystem::is_directory(directoryTarget), "Failed save replaced its destination");
    bool failed = false;
    try
    {
        auto invalid = canvas::readPng(directory / "missing.png");
    }
    catch (const std::exception &)
    {
        failed = true;
    }
    require(failed && g_document.image() == original, "Failed open damaged document");

    auto input = canvas::Image::blank(48, 32);
    for (int y = 0; y < input.height; ++y)
        for (int x = 0; x < input.width; ++x)
            input.pixels[y * input.width + x] = (x * 5) * 65536 + (y * 7) * 256 + (x + y);
    for (const auto *module : {&kGray, &kEdge, &kThreshold, &kMandel})
    {
        g_document.replace(input, true);
        require(loadSelection(*module, "pencil"), "Filter failed to load");
        require(callEntry(module->entry), "Whole-image filter failed");
        const auto expected = g_document.image();
        g_document.replace(input, true);
        g_bandRowsPerTick = 5;
        startBandRun(module);
        while (runBandStep())
        {
        }
        require(g_bandActive, "Band run ended unexpectedly");
        finishBandRun(true);
        require(g_document.image() == expected, "Banded filter differed from whole-image filter");
        require(g_document.undo(), "Filter did not create an undo entry");
        require(g_document.image() == input, "Undo failed to restore pre-filter image");
    }
    g_preview = true;
    require(loadSelection(kThreshold, "pencil"), "Preview filter failed to load");
    startBandRun(&kThreshold);
    require(runBandStep(), "Preview finished before cancellation");
    cancelBandRun();
    require(!g_document.editing() && g_document.image() == input, "Mid-filter cancellation did not restore original");

    const auto faultScript = directory / "fault.ks";
    {
        std::ofstream script(faultScript);
        script << "module fault; extern set_pixel(x, y, rgb) -> void;\n"
                  "export func run_band() -> void { set_pixel(0, 0, 123); var bad = 1 / 0; }\n";
    }
    g_searchPaths.insert(g_searchPaths.begin(), std::filesystem::absolute(directory).string());
    const ModuleButton fault = {"Fault",           "fault.ks",       "fault",          "fault@run_band",
                                "fault::run_band", "fault@run_band", "fault::run_band"};
    require(loadSelection(fault, "pencil"), "Runtime-error fixture failed to load");
    startBandRun(&fault);
    require(!runBandStep() && !g_bandActive && !g_document.editing(), "Runtime error did not stop the filter");
    require(g_document.image() == input, "Runtime error left partial edits in the document");
    std::filesystem::remove(faultScript);

    // A failed replacement compile must leave the previous program callable.
    auto *previous = g_vm;
    const auto badScript = directory / "broken.ks";
    {
        std::ofstream script(badScript);
        script << "module broken; export func paint() -> void {\n";
    }
    g_searchPaths.insert(g_searchPaths.begin(), std::filesystem::absolute(directory).string());
    const ModuleButton bad = {"Broken", "broken.ks", "broken", "broken@paint", "broken::paint", nullptr, nullptr};
    require(!loadSelection(bad, nullptr) && g_vm == previous, "Failed reload replaced the working VM");
    require(callEntry(kPencil.entry), "Failed reload evicted the resident tool");
    std::filesystem::remove(badScript);
    std::cout << "Canvas headless verification passed\n";
    return 0;
}

void seedPixel(int x, int y, int rgb) { set_pixel_value(x, y, rgb); }

std::string statusText()
{
    if (g_status == nullptr || g_status->label() == nullptr)
    {
        return "";
    }
    return std::string(g_status->label());
}

// Pumps the Fl event loop until the active band run finishes (or timeout).
// Band ticks fire through Fl::wait, so a return means the pump stayed alive
// long enough to drive the run to completion. Returns true when no run is
// active afterwards.
bool pumpBandRun(int timeoutMs)
{
    const auto start = std::chrono::steady_clock::now();
    while (g_bandActive)
    {
        Fl::wait(0.05);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        if (std::chrono::steady_clock::now() - start > std::chrono::milliseconds(timeoutMs))
        {
            return false;
        }
    }
    // Drain one extra pump so the completion status settles.
    Fl::wait(0.05);
    return true;
}

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
    // Banded: the REAL button press only starts the run; ticks finish it.
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
    if (!g_bandActive)
    {
        std::cout << "MISMATCH: gray press did not start a band run\n";
        ok = false;
    }
    if (g_grayButton->active() != 0 || g_edgeButton->active() != 0 || g_mandelButton->active() != 0)
    {
        std::cout << "MISMATCH: filter buttons stayed enabled during gray run\n";
        ok = false;
    }
    if (!pumpBandRun(60000))
    {
        std::cout << "MISMATCH: gray band run timed out\n";
        ok = false;
    }
    if (g_grayButton->active() == 0 || g_edgeButton->active() == 0 || g_mandelButton->active() == 0)
    {
        std::cout << "MISMATCH: filter buttons not re-enabled after gray run\n";
        ok = false;
    }
    std::cout << "gray math: lum(255,0,0) = 255/3 = 85 -> pack(85,85,85) = 5592405\n";
    std::cout << "gray math: lum(0x123456) = (18+52+86)/3 = 52 -> pack(52,52,52) = 3421236\n";
    ok &= checkPixel("gray red", 5, 5, 5592405) != 0;
    ok &= checkPixel("gray 0x123456", 6, 5, 3421236) != 0;
    ok &= checkPixel("gray black", 7, 5, 0) != 0;
    const unsigned int grayChecksum = pixelChecksum();
    std::cout << "gray: checksum " << std::hex << grayChecksum << std::dec << '\n';
    if (grayChecksum != 0xa87787d8u)
    {
        std::cout << "MISMATCH: gray banded checksum " << std::hex << grayChecksum << " != recorded a87787d8"
                  << std::dec << '\n';
        ok = false;
    }

    // Edge runs the full 5-reads-per-pixel pass band-by-band to completion.
    loads = g_loadCount;
    g_edgeButton->do_callback();
    if (g_loadCount != loads + 1)
    {
        std::cout << "MISMATCH: edge press did not recompile\n";
        ok = false;
    }
    if (!pumpBandRun(60000))
    {
        std::cout << "MISMATCH: edge band run timed out\n";
        ok = false;
    }
    const unsigned int edgeChecksum = pixelChecksum();
    std::cout << "edge: checksum " << std::hex << edgeChecksum << std::dec << " nonzero " << countNonzero() << '\n';
    if (edgeChecksum == grayChecksum)
    {
        std::cout << "MISMATCH: edge left the canvas unchanged\n";
        ok = false;
    }
    if (edgeChecksum != 0x6baddf7cu)
    {
        std::cout << "MISMATCH: edge banded checksum " << std::hex << edgeChecksum << " != recorded 6baddf7c"
                  << std::dec << '\n';
        ok = false;
    }

    // Threshold on fresh seeds: white (lum 255 > 128) stays 16777215,
    // red (lum 85) and black go 0. Banded like the other filters.
    g_clearButton->do_callback();
    seedPixel(5, 5, 16777215);
    seedPixel(6, 5, 16711680);
    seedPixel(7, 5, 0);
    present_value();
    g_thresholdButton->do_callback();
    if (!pumpBandRun(60000))
    {
        std::cout << "MISMATCH: threshold band run timed out\n";
        ok = false;
    }
    ok &= checkPixel("threshold white", 5, 5, 16777215) != 0;
    ok &= checkPixel("threshold red", 6, 5, 0) != 0;
    ok &= checkPixel("threshold black", 7, 5, 0) != 0;

    // Spray splatters near the cursor; Line connects a drag anchor to its endpoint.
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
    g_strokeX = 198;
    g_strokeY = 148;
    g_mouseDown = true;
    g_lineButton->do_callback();
    if (!paintTick())
    {
        ok = false;
    }
    g_mouseDown = false;
    ok &= checkPixel("line anchor", 200, 150, kSwatchRed) != 0;
    ok &= checkPixel("line midpoint", 199, 149, kSwatchRed) != 0;
    ok &= checkPixel("line -2", 198, 148, kSwatchRed) != 0;

    // Mandel must complete within budget and be deterministic: press twice
    // (reload + banded run each time) and require identical checksums.
    // Shade runs 8..248, so every pixel is nonzero on full coverage.
    // Responsiveness evidence: the REAL button press returns quickly
    // (schedules bands, does not block); the Fl pump then drives ticks,
    // the status line advances (rows N/200 changes between polls), and the
    // total wall time is spread across ticks. A single blocking call could
    // not advance ticks or change status while running.
    const auto pressStart = std::chrono::steady_clock::now();
    g_mandelButton->do_callback();
    const auto pressEnd = std::chrono::steady_clock::now();
    const long long pressMs = std::chrono::duration_cast<std::chrono::milliseconds>(pressEnd - pressStart).count();
    std::cout << "mandel: button press returned in " << pressMs << " ms (schedules bands, must not block)\n";
    if (pressMs > 2000)
    {
        std::cout << "MISMATCH: mandel press blocked the UI thread (" << pressMs << " ms)\n";
        ok = false;
    }
    if (!g_bandActive)
    {
        std::cout << "MISMATCH: mandel press did not start a band run\n";
        ok = false;
    }
    else
    {
        // Reentrancy probe: a second filter press mid-run must be ignored
        // (no recompile, same run continues).
        const int loadsMid = g_loadCount;
        g_grayButton->do_callback();
        if (g_loadCount != loadsMid)
        {
            std::cout << "MISMATCH: reentrant filter press recompiled mid-run\n";
            ok = false;
        }
        if (g_bandModule == nullptr || std::string(g_bandModule->label) != "Mandel")
        {
            std::cout << "MISMATCH: reentrant press hijacked the band run\n";
            ok = false;
        }
        else
        {
            std::cout << "reentrancy: second filter press ignored mid-run (loads " << g_loadCount << ")\n";
        }
    }
    std::vector<std::string> statusSeen;
    int ticksSeen = g_bandTicks;
    const auto runStart = std::chrono::steady_clock::now();
    {
        const auto deadline = runStart + std::chrono::milliseconds(120000);
        std::string last;
        while (g_bandActive)
        {
            Fl::wait(0.05);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            const std::string cur = statusText();
            if (cur != last)
            {
                statusSeen.push_back(cur);
                last = cur;
            }
            ticksSeen = g_bandTicks;
            if (std::chrono::steady_clock::now() > deadline)
            {
                break;
            }
        }
        Fl::wait(0.05);
    }
    const auto runEnd = std::chrono::steady_clock::now();
    const long long runMs = std::chrono::duration_cast<std::chrono::milliseconds>(runEnd - runStart).count();
    if (g_bandActive)
    {
        std::cout << "MISMATCH: mandel band run timed out\n";
        ok = false;
    }
    std::cout << "mandel: pump drove " << ticksSeen << " band ticks, status changed " << statusSeen.size()
              << " times, spread over " << runMs << " ms\n";
    for (const std::string &s : statusSeen)
    {
        std::cout << "mandel status: " << s << '\n';
    }
    if (ticksSeen < 5)
    {
        std::cout << "MISMATCH: too few band ticks (" << ticksSeen << "), pump did not drive the run\n";
        ok = false;
    }
    if (statusSeen.size() < 3)
    {
        std::cout << "MISMATCH: status never advanced during mandel run (pump starved?)\n";
        ok = false;
    }
    if (!(pressMs * 5 < runMs))
    {
        std::cout << "MISMATCH: run wall time not spread across ticks (press " << pressMs << " ms, run " << runMs
                  << " ms)\n";
        ok = false;
    }
    const unsigned int mandelFirst = pixelChecksum();
    const int mandelNonzero = countNonzero();
    std::cout << "mandel run 1: checksum " << std::hex << mandelFirst << std::dec << " nonzero " << mandelNonzero
              << '\n';
    if (mandelFirst != 0xbb6f3fd5u)
    {
        std::cout << "MISMATCH: mandel banded checksum " << std::hex << mandelFirst << " != recorded bb6f3fd5"
                  << std::dec << '\n';
        ok = false;
    }
    g_mandelButton->do_callback();
    if (!pumpBandRun(120000))
    {
        std::cout << "MISMATCH: mandel run 2 timed out\n";
        ok = false;
    }
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

    // Cancel: start a third Mandel, let a couple of bands complete, then
    // abort through the REAL Cancel button. The run must stop cleanly
    // (no hang, buttons re-enabled) and restore the original pixels; a fresh full
    // run afterwards must still hit the recorded checksum.
    g_mandelButton->do_callback();
    {
        const auto cancelDeadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(120000);
        while (g_bandActive && g_bandTicks < 2)
        {
            Fl::wait(0.05);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            if (std::chrono::steady_clock::now() > cancelDeadline)
            {
                break;
            }
        }
    }
    if (!g_bandActive)
    {
        std::cout << "MISMATCH: mandel run 3 finished before cancel could be tested\n";
        ok = false;
    }
    else
    {
        const int ticksAtCancel = g_bandTicks;
        std::cout << "cancel: aborting mandel at tick " << ticksAtCancel << " status '" << statusText() << "'\n";
        g_cancelButton->do_callback();
        if (g_bandActive)
        {
            std::cout << "MISMATCH: cancel did not stop the band run\n";
            ok = false;
        }
        if (g_mandelButton->active() == 0 || g_cancelButton->active() != 0)
        {
            std::cout << "MISMATCH: buttons not restored after cancel\n";
            ok = false;
        }
        if (ticksAtCancel <= 0 || ticksAtCancel >= kCanvasH / kBandRows)
        {
            std::cout << "MISMATCH: cancel happened outside mid-run (tick " << ticksAtCancel << ")\n";
            ok = false;
        }
        else
        {
            std::cout << "cancel: clean abort after " << ticksAtCancel << " ticks\n";
        }
    }
    g_mandelButton->do_callback();
    if (!pumpBandRun(120000))
    {
        std::cout << "MISMATCH: mandel restore run timed out\n";
        ok = false;
    }
    if (pixelChecksum() != mandelFirst)
    {
        std::cout << "MISMATCH: mandel restore run diverged after cancel\n";
        ok = false;
    }
    else
    {
        std::cout << "cancel: restore run matches recorded checksum\n";
    }

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
    bool headless = false;
    std::filesystem::path preview, verifyDirectory = "build/test-tmp/Canvas";
    for (int i = 1; i < argc; ++i)
    {
        if (std::string(argv[i]) == "--verify")
        {
            verify = true;
        }
        else if (std::string(argv[i]) == "--verify-headless")
        {
            headless = true;
            if (i + 1 < argc)
                verifyDirectory = argv[++i];
        }
        else if (std::string(argv[i]) == "--help")
        {
            std::cout << "Canvas: scriptable PNG image editor.\n"
                         "Canvas --verify-headless [directory] runs without opening a window.\n"
                         "Canvas --verify exercises the interactive interface.\n";
            return 0;
        }
        else if (std::string(argv[i]) == "--preview")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "Canvas --preview requires a PNG path\n";
                return 2;
            }
            preview = argv[++i];
        }
    }

    // Per-pixel script loops cost millions of steps, far past the default
    // 10M budget, so the VM runs with an explicit raised ceiling. Bands
    // are small (20 rows per tick) so each callFunction stays far under
    // it; the budget still backstops runaway scripts, just later.
    try
    {
        g_vmOwner = createBoundVm();
        g_vm = g_vmOwner.get();

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

        if (headless)
            return runHeadlessVerify(verifyDirectory);
        if (verify)
            g_preview = false;

        Fl_Window window(920, 570, kWindowTitle);
        g_window = &window;
        window.callback(onClose);
        PaintCanvas canvas(10, 50, 640, 400);
        g_canvasWidget = &canvas;
        Fl_Choice sizeChoice(10, 10, 130, 28);
        sizeChoice.add("320 x 200|640 x 400|960 x 600");
        sizeChoice.value(0);
        Fl_Button newButton(150, 10, 70, 28, "New");
        newButton.callback(onNew, &sizeChoice);
        Fl_Button openButton(230, 10, 70, 28, "Open");
        openButton.callback(onOpen);
        Fl_Button saveButton(310, 10, 70, 28, "Save");
        saveButton.callback(onSave);
        Fl_Button saveAsButton(390, 10, 80, 28, "Save As");
        saveAsButton.callback(onSaveAs);
        Fl_Button undoButton(480, 10, 75, 28, "Undo");
        undoButton.callback(onUndo);
        Fl_Button redoButton(565, 10, 75, 28, "Redo");
        redoButton.callback(onRedo);
        Fl_Button pencilButton(670, 50, 110, 28, "Pencil");
        pencilButton.callback(onTool, const_cast<ModuleButton *>(&kPencil));
        g_pencilButton = &pencilButton;
        Fl_Button sprayButton(790, 50, 110, 28, "Spray");
        sprayButton.callback(onTool, const_cast<ModuleButton *>(&kSpray));
        g_sprayButton = &sprayButton;
        Fl_Button lineButton(670, 84, 110, 28, "Line");
        lineButton.callback(onTool, const_cast<ModuleButton *>(&kLine));
        g_lineButton = &lineButton;
        Fl_Button grayButton(670, 130, 110, 28, "Grayscale");
        grayButton.callback(onFilter, const_cast<ModuleButton *>(&kGray));
        g_grayButton = &grayButton;
        Fl_Button edgeButton(790, 130, 110, 28, "Edges");
        edgeButton.callback(onFilter, const_cast<ModuleButton *>(&kEdge));
        g_edgeButton = &edgeButton;
        Fl_Button thresholdButton(670, 164, 110, 28, "Threshold");
        thresholdButton.callback(onFilter, const_cast<ModuleButton *>(&kThreshold));
        g_thresholdButton = &thresholdButton;
        Fl_Button mandelButton(790, 164, 110, 28, "Mandelbrot");
        mandelButton.callback(onFilter, const_cast<ModuleButton *>(&kMandel));
        g_mandelButton = &mandelButton;
        Fl_Button clearButton(790, 84, 110, 28, "Clear");
        clearButton.callback(onClear);
        g_clearButton = &clearButton;
        Fl_Check_Button previewButton(670, 203, 200, 25, "Preview filters");
        previewButton.value(g_preview ? 1 : 0);
        previewButton.callback(onPreview);
        Fl_Button applyButton(670, 236, 110, 28, "Apply");
        applyButton.callback(onApply);
        Fl_Button cancelButton(790, 236, 110, 28, "Cancel");
        cancelButton.callback(onCancel);
        cancelButton.deactivate();
        g_cancelButton = &cancelButton;
        Fl_Value_Slider radiusSlider(670, 306, 230, 24, "Brush radius");
        radiusSlider.type(FL_HOR_NICE_SLIDER);
        radiusSlider.bounds(1, 20);
        radiusSlider.step(1);
        radiusSlider.value(g_brushRadius);
        radiusSlider.callback(onBrushRadius);
        Fl_Value_Slider opacitySlider(670, 362, 230, 24, "Opacity (%)");
        opacitySlider.type(FL_HOR_NICE_SLIDER);
        opacitySlider.bounds(1, 100);
        opacitySlider.step(1);
        opacitySlider.value(g_opacity);
        opacitySlider.callback(onOpacity);
        Fl_Value_Slider thresholdSlider(670, 418, 230, 24, "Threshold");
        thresholdSlider.type(FL_HOR_NICE_SLIDER);
        thresholdSlider.bounds(0, 255);
        thresholdSlider.step(1);
        thresholdSlider.value(g_threshold);
        thresholdSlider.when(FL_WHEN_RELEASE);
        thresholdSlider.callback(onThreshold);
        Fl_Button whiteButton(10, 466, 100, 28, "White");
        whiteButton.callback(onSwatch, const_cast<Swatch *>(&kWhite));
        Fl_Button redButton(120, 466, 100, 28, "Red");
        redButton.callback(onSwatch, const_cast<Swatch *>(&kRed));
        g_redButton = &redButton;
        Fl_Button greenButton(230, 466, 100, 28, "Green");
        greenButton.callback(onSwatch, const_cast<Swatch *>(&kGreen));
        Fl_Button blueButton(340, 466, 100, 28, "Blue");
        blueButton.callback(onSwatch, const_cast<Swatch *>(&kBlue));
        Fl_Button colorButton(450, 466, 190, 28, "Choose color");
        colorButton.callback(onColor);
        Fl_Box status(10, 520, 890, 30, "Ready");
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
        updateBandButtons();
        refreshUi();
        Fl::add_handler(escHandler);

        if (!preview.empty())
        {
            samples::renderPreview(window, preview);
            return 0;
        }
        if (verify)
        {
            window.show();
            const int code = runVerify();
            return code;
        }
        window.show(argc, argv);
        Fl::run();
    }
    catch (const std::exception &error)
    {
        std::cerr << "runtime error: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
