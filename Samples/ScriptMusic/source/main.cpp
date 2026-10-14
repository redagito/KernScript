// ScriptMusic: an algorithmic music workstation stressing module graphs.
//
// Composition lives in music/*.ks on disk: theory.ks is the shared hub
// (scale degrees, chord tables as if/else chains, note-name helpers) and
// bass.ks + drums.ks + arp.ks each import theory and export pattern(step)
// returning MIDI-ish ints (A4 = 69, -1 = rest). The host compiles one
// anonymous root importing all three voices, so every load is a genuine
// diamond (theory loads once, shared), then drives composition as MANY
// sequential calls on a persistent VM: 16 steps x 3 voices = 48 calls,
// each pulling the step cursor through the valued extern cur_step() and
// pushing its note back through the statement extern emit(voice, note).
// Scripts never touch floats for pitch: freq = 440*2^((n-69)/12) is
// computed host-side in double precision.
//
// Host protocol (tiny, fixed; mirrored in music/*.ks):
//   cur_step() value-use: host step cursor 0-15 (script keeps no state).
//   preset() value-use: host style flag, 0 plain song, 1 bright song.
//   emit(voice, note) statement-use: stores note for (voice, cur_step).
// Song A / Song B set the preset flag and RECOMPILE the same root;
// voice buttons only gate which voices generate, draw and synthesize.
//
// WAV synthesis is by hand (~40 lines, no audio dependency): 16-bit PCM
// mono 22050 Hz, one sine with exponential decay per note event, mixed,
// clipped, 44-byte header. Playback launches the OS player and degrades
// gracefully: Windows runs `cmd /c start "" /min "song.wav"`, Linux tries
// xdg-open then aplay, macOS tries afplay then open. A missing player
// only prints a fallback note ("open song.wav by hand"); --verify asserts
// file bytes (size + FNV-1a checksum), never playback.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
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

constexpr const char *kWindowTitle = "KernScript Music";
constexpr int kSteps = 16;
constexpr int kVoices = 3;
constexpr int kRollLo = 36;
constexpr int kRollHi = 79;
constexpr int kRollRows = kRollHi - kRollLo + 1;
constexpr int kSampleRate = 22050;
constexpr int kRest = -1;
constexpr int kTempoA = 90;
constexpr int kTempoB = 140;
constexpr unsigned int kWav90Fnv = 2817628738u;
constexpr unsigned int kWav140Fnv = 611581108u;

const char *kVoiceNames[kVoices] = {"Bass", "Drums", "Arp"};
const char *kVoiceEntries[kVoices] = {"bass_note", "drums_note", "arp_note"};
const Fl_Color kVoiceColors[kVoices] = {FL_RED, FL_DARK_GREEN, FL_BLUE};

const int kBass0[kSteps] = {45, 45, 52, 47, 41, 41, 48, 43, 36, 36, 43, 38, 43, 43, 50, 45};
const int kBass1[kSteps] = {45, 48, 52, 57, 41, 44, 48, 53, 36, 39, 43, 48, 43, 46, 50, 55};
const int kDrums0[kSteps] = {36, 44, 44, -1, 40, 44, 44, -1, 36, 44, 44, -1, 40, 44, 44, -1};
const int kDrums1[kSteps] = {36, 44, 36, -1, 40, 44, 36, -1, 36, 44, 36, -1, 40, 44, 36, -1};
const int kArp0[kSteps] = {57, 60, 64, 69, 53, 57, 60, 65, 60, 64, 67, 72, 55, 59, 62, 67};
const int kArp1[kSteps] = {57, 60, 64, 76, 53, 57, 60, 72, 60, 64, 67, 79, 55, 59, 62, 74};

std::string g_argv0;
std::string g_musicDir;
std::string g_wavPath;
std::vector<std::string> g_searchPaths;
kern::IVirtualMachine *g_vm = nullptr;

int g_step = 0;
int g_preset = 0;
int g_notes[kVoices][kSteps];
bool g_voiceOn[kVoices] = {true, true, true};
int g_tempo = kTempoA;
int g_loadCount = 0;
int g_callCount = 0;
long long g_compileMs = 0;
long long g_genMs = 0;
long long g_synthMs = 0;
int g_playStep = -1;

Fl_Window *g_window = nullptr;
Fl_Widget *g_roll = nullptr;
Fl_Button *g_voiceButtons[kVoices] = {nullptr, nullptr, nullptr};
Fl_Button *g_generateButton = nullptr;
Fl_Button *g_playButton = nullptr;
Fl_Button *g_stopButton = nullptr;
Fl_Button *g_tempoAButton = nullptr;
Fl_Button *g_tempoBButton = nullptr;
Fl_Button *g_songAButton = nullptr;
Fl_Button *g_songBButton = nullptr;
Fl_Box *g_status = nullptr;

int cur_step_value() { return g_step; }

int preset_value() { return g_preset; }

void emit_note_value(int voice, int note)
{
    if (voice < 0 || voice >= kVoices || g_step < 0 || g_step >= kSteps)
    {
        return;
    }
    g_notes[voice][g_step] = note;
}

void clearVoice(int voice)
{
    for (int s = 0; s < kSteps; ++s)
    {
        g_notes[voice][s] = kRest;
    }
}

void clearAllNotes()
{
    for (int v = 0; v < kVoices; ++v)
    {
        clearVoice(v);
    }
}

// Roll cell query shared by the widget and --verify: true when the
// enabled voice holds a drawable note at (voice, step).
bool cellOn(int voice, int step)
{
    if (voice < 0 || voice >= kVoices || step < 0 || step >= kSteps)
    {
        return false;
    }
    if (!g_voiceOn[voice])
    {
        return false;
    }
    const int note = g_notes[voice][step];
    return note >= kRollLo && note <= kRollHi;
}

class PianoRoll : public Fl_Widget
{
public:
    PianoRoll(int x, int y, int w, int h) : Fl_Widget(x, y, w, h) {}

    void draw() override
    {
        fl_push_clip(x(), y(), w(), h());
        fl_color(FL_BLACK);
        fl_rectf(x(), y(), w(), h());
        const int cw = w() / kSteps;
        const int ch = h() / kRollRows;
        for (int v = 0; v < kVoices; ++v)
        {
            if (!g_voiceOn[v])
            {
                continue;
            }
            fl_color(kVoiceColors[v]);
            for (int s = 0; s < kSteps; ++s)
            {
                const int note = g_notes[v][s];
                if (note < kRollLo || note > kRollHi)
                {
                    continue;
                }
                const int row = kRollHi - note;
                fl_rectf(x() + s * cw + 1, y() + row * ch + 1, cw - 2, ch - 2);
            }
        }
        fl_color(FL_DARK3);
        for (int s = 0; s <= kSteps; ++s)
        {
            fl_line(x() + s * cw, y(), x() + s * cw, y() + h());
        }
        for (int r = 0; r <= kRollRows; ++r)
        {
            const int note = kRollHi - r;
            if (note >= kRollLo && note % 12 == 0)
            {
                fl_color(FL_GRAY);
            }
            else
            {
                fl_color(FL_DARK3);
            }
            fl_line(x(), y() + r * ch, x() + w(), y() + r * ch);
        }
        if (g_playStep >= 0 && g_playStep < kSteps)
        {
            fl_color(FL_YELLOW);
            fl_line(x() + g_playStep * cw, y(), x() + g_playStep * cw, y() + h());
            fl_line(x() + (g_playStep + 1) * cw - 1, y(), x() + (g_playStep + 1) * cw - 1, y() + h());
        }
        fl_pop_clip();
    }
};

// Music files live as separate files under music/. Resolve robustly so
// --verify works from the build tree: exe-dir/music wins, then
// CWD/music, then CWD/Samples/ScriptMusic/music (repo-root runs).
bool resolveMusicDir(std::string &out, int &candidate)
{
    std::vector<std::string> tries;
    try
    {
        const std::filesystem::path exe(g_argv0);
        if (exe.has_parent_path())
        {
            tries.push_back((exe.parent_path() / "music").string());
        }
        else
        {
            tries.push_back((std::filesystem::current_path() / "music").string());
        }
    }
    catch (const std::filesystem::filesystem_error &)
    {
        tries.push_back((std::filesystem::current_path() / "music").string());
    }
    tries.push_back((std::filesystem::current_path() / "music").string());
    tries.push_back((std::filesystem::current_path() / "Samples" / "ScriptMusic" / "music").string());
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
    std::cerr << "music dir missing (tried " << tries.size() << " candidates:";
    for (const std::string &t : tries)
    {
        std::cerr << " [" << t << "]";
    }
    std::cerr << ")\n";
    return false;
}

bool findMusic(const char *file, std::string &out)
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
    std::cerr << "music file missing: " << file << " (searched " << g_searchPaths.size() << " dirs)\n";
    return false;
}

// One anonymous root importing every voice; the voices each import
// theory, so theory loads once and is shared (diamond). The wrappers are
// flat-named (anonymous root keeps flat names) and sequence each step as
// extern -> script -> extern through temp vars: no nested calls, so every
// value an extern leaves behind is consumed before the next call.
std::string buildRootSource()
{
    return "import bass;\nimport drums;\nimport arp;\n"
           "extern cur_step();\n"
           "extern emit(voice, note);\n"
           "func bass_note() {\n"
           "  var s = cur_step();\n"
           "  var n = bass::pattern(s);\n"
           "  emit(0, n);\n"
           "}\n"
           "func drums_note() {\n"
           "  var s = cur_step();\n"
           "  var n = drums::pattern(s);\n"
           "  emit(1, n);\n"
           "}\n"
           "func arp_note() {\n"
           "  var s = cur_step();\n"
           "  var n = arp::pattern(s);\n"
           "  emit(2, n);\n"
           "}\n";
}

bool loadMusic()
{
    std::string probe;
    if (!findMusic("bass.ks", probe))
    {
        return false;
    }
    const std::string root = buildRootSource();
    std::stringstream asmCode;
    {
        const auto start = std::chrono::steady_clock::now();
        std::istringstream input(root);
        std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
        if (!compiler->compile(input, asmCode, "<music-root>", g_searchPaths))
        {
            std::cerr << "compile failed for music root\n";
            return false;
        }
        const auto end = std::chrono::steady_clock::now();
        g_compileMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    }
    std::stringstream byteCode;
    {
        std::istringstream input(asmCode.str());
        std::unique_ptr<kern::IAssembler> assembler(kern::createAssembler());
        if (!assembler->assemble(input, byteCode))
        {
            std::cerr << "assemble failed for music root\n";
            return false;
        }
    }
    try
    {
        if (!g_vm->load(byteCode))
        {
            std::cerr << "load failed for music root\n";
            return false;
        }
    }
    catch (const kern::RuntimeError &error)
    {
        std::cerr << "load error: " << error.what() << '\n';
        return false;
    }
    ++g_loadCount;
    clearAllNotes();
    std::cout << "music loaded from " << g_musicDir << " (load #" << g_loadCount << ", preset " << g_preset << ", "
              << g_compileMs << " ms)\n";
    return true;
}

bool generateVoice(int voice)
{
    for (int s = 0; s < kSteps; ++s)
    {
        g_step = s;
        try
        {
            if (!g_vm->callFunction(kVoiceEntries[voice]))
            {
                std::cerr << "function " << kVoiceEntries[voice] << " not found\n";
                return false;
            }
        }
        catch (const kern::RuntimeError &error)
        {
            std::cerr << "runtime error in " << kVoiceEntries[voice] << " step " << s << ": " << error.what() << '\n';
            return false;
        }
        ++g_callCount;
    }
    return true;
}

bool generateAll()
{
    const auto start = std::chrono::steady_clock::now();
    for (int v = 0; v < kVoices; ++v)
    {
        if (!g_voiceOn[v])
        {
            continue;
        }
        if (!generateVoice(v))
        {
            return false;
        }
    }
    const auto end = std::chrono::steady_clock::now();
    g_genMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    int total = 0;
    for (int v = 0; v < kVoices; ++v)
    {
        if (!g_voiceOn[v])
        {
            continue;
        }
        for (int s = 0; s < kSteps; ++s)
        {
            if (g_notes[v][s] != kRest)
            {
                ++total;
            }
        }
    }
    std::cout << "generated " << total << " notes (" << g_callCount << " vm calls, " << g_genMs << " ms)\n";
    return true;
}

unsigned int fnv1a(const std::vector<unsigned char> &bytes)
{
    unsigned int hash = 2166136261u;
    for (unsigned char b : bytes)
    {
        hash ^= b;
        hash *= 16777619u;
    }
    return hash;
}

void pushU32(std::vector<unsigned char> &out, unsigned int v)
{
    out.push_back(static_cast<unsigned char>(v & 255u));
    out.push_back(static_cast<unsigned char>((v >> 8) & 255u));
    out.push_back(static_cast<unsigned char>((v >> 16) & 255u));
    out.push_back(static_cast<unsigned char>((v >> 24) & 255u));
}

void pushU16(std::vector<unsigned char> &out, unsigned int v)
{
    out.push_back(static_cast<unsigned char>(v & 255u));
    out.push_back(static_cast<unsigned char>((v >> 8) & 255u));
}

// Hand-written 16-bit PCM mono WAV: per-note sine with exponential decay,
// mixed, clipped. Deterministic: pure host-side double math, no RNG.
bool writeWav(int &outSamples, unsigned int &outFnv)
{
    const auto start = std::chrono::steady_clock::now();
    const double kTwoPi = 8.0 * std::atan(1.0);
    const double stepDur = 60.0 / static_cast<double>(g_tempo);
    const int total = static_cast<int>(kSteps * stepDur * kSampleRate + 0.5);
    std::vector<double> mix(static_cast<std::size_t>(total), 0.0);
    for (int v = 0; v < kVoices; ++v)
    {
        if (!g_voiceOn[v])
        {
            continue;
        }
        for (int s = 0; s < kSteps; ++s)
        {
            const int n = g_notes[v][s];
            if (n < 0)
            {
                continue;
            }
            const double freq = 440.0 * std::pow(2.0, (static_cast<double>(n) - 69.0) / 12.0);
            const double dur = stepDur * 0.9;
            const int i0 = static_cast<int>(s * stepDur * kSampleRate + 0.5);
            const int i1 = std::min(total, i0 + static_cast<int>(dur * kSampleRate + 0.5));
            for (int i = i0; i < i1; ++i)
            {
                const double t = (static_cast<double>(i - i0)) / kSampleRate;
                mix[static_cast<std::size_t>(i)] += 0.30 * std::sin(kTwoPi * freq * t) * std::exp(-3.0 * t / dur);
            }
        }
    }
    std::vector<unsigned char> bytes;
    bytes.reserve(static_cast<std::size_t>(total) * 2 + 44);
    bytes.insert(bytes.end(), {'R', 'I', 'F', 'F'});
    pushU32(bytes, static_cast<unsigned int>(36 + total * 2));
    bytes.insert(bytes.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
    pushU32(bytes, 16);
    pushU16(bytes, 1);
    pushU16(bytes, 1);
    pushU32(bytes, kSampleRate);
    pushU32(bytes, kSampleRate * 2);
    pushU16(bytes, 2);
    pushU16(bytes, 16);
    bytes.insert(bytes.end(), {'d', 'a', 't', 'a'});
    pushU32(bytes, static_cast<unsigned int>(total * 2));
    for (int i = 0; i < total; ++i)
    {
        double s = mix[static_cast<std::size_t>(i)];
        s = std::max(-1.0, std::min(1.0, s));
        pushU16(bytes, static_cast<unsigned int>(static_cast<int>(std::lround(s * 32767.0)) & 65535));
    }
    std::ofstream file(g_wavPath, std::ios::binary);
    if (!file)
    {
        std::cerr << "wav write failed: " << g_wavPath << '\n';
        return false;
    }
    file.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    file.close();
    outSamples = total;
    outFnv = fnv1a(bytes);
    const auto end = std::chrono::steady_clock::now();
    g_synthMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "wav " << g_wavPath << ": " << bytes.size() << " bytes, fnv " << outFnv << " (" << g_synthMs
              << " ms)\n";
    return true;
}

// OS playback, fire-and-forget: a missing player only prints the fallback
// and never fails the run; --verify asserts file bytes, not playback.
void launchPlayback()
{
    std::cout << "playback: ";
#if defined(_WIN32)
    std::cout << "cmd /c start (Windows shell)\n";
    const std::string cmd = "cmd /c start \"\" /min \"" + g_wavPath + "\"";
    const int code = std::system(cmd.c_str());
    if (code != 0)
    {
        std::cout << "playback fallback: no player took song.wav (code " << code << "); open " << g_wavPath
                  << " by hand\n";
    }
#elif defined(__APPLE__)
    std::cout << "afplay else open (macOS)\n";
    const int code = std::system(("afplay \"" + g_wavPath + "\" 2>/dev/null || open \"" + g_wavPath + "\"").c_str());
    if (code != 0)
    {
        std::cout << "playback fallback: no player took song.wav; open " << g_wavPath << " by hand\n";
    }
#else
    std::cout << "xdg-open else aplay (Linux)\n";
    const int code = std::system(("xdg-open \"" + g_wavPath + "\" 2>/dev/null || aplay \"" + g_wavPath + "\"").c_str());
    if (code != 0)
    {
        std::cout << "playback fallback: no player took song.wav; open " << g_wavPath << " by hand\n";
    }
#endif
}

void refreshLabels()
{
    for (int v = 0; v < kVoices; ++v)
    {
        g_voiceButtons[v]->copy_label((std::string(kVoiceNames[v]) + (g_voiceOn[v] ? ": on" : ": off")).c_str());
    }
    g_tempoAButton->copy_label((std::string("Tempo 90") + (g_tempo == kTempoA ? " [*]" : "")).c_str());
    g_tempoBButton->copy_label((std::string("Tempo 140") + (g_tempo == kTempoB ? " [*]" : "")).c_str());
    g_songAButton->copy_label((std::string("Song A") + (g_preset == 0 ? " [*]" : "")).c_str());
    g_songBButton->copy_label((std::string("Song B") + (g_preset == 1 ? " [*]" : "")).c_str());
    int total = 0;
    for (int v = 0; v < kVoices; ++v)
    {
        for (int s = 0; s < kSteps; ++s)
        {
            if (g_voiceOn[v] && g_notes[v][s] != kRest)
            {
                ++total;
            }
        }
    }
    const std::string text = "preset " + std::to_string(g_preset) + " | tempo " + std::to_string(g_tempo) +
                             " | notes " + std::to_string(total) + " | loads " + std::to_string(g_loadCount) +
                             " | calls " + std::to_string(g_callCount);
    g_status->copy_label(text.c_str());
}

void play_tick(void * /*data*/)
{
    ++g_playStep;
    if (g_playStep >= kSteps)
    {
        g_playStep = -1;
        g_roll->redraw();
        return;
    }
    g_roll->redraw();
    Fl::repeat_timeout(60.0 / static_cast<double>(g_tempo), play_tick);
}

void onVoice(Fl_Widget * /*widget*/, void *data)
{
    const int voice = static_cast<int>(reinterpret_cast<std::intptr_t>(data));
    g_voiceOn[voice] = !g_voiceOn[voice];
    if (!g_voiceOn[voice])
    {
        clearVoice(voice);
    }
    else if (!generateVoice(voice))
    {
        return;
    }
    refreshLabels();
    g_roll->redraw();
    std::cout << kVoiceNames[voice] << (g_voiceOn[voice] ? " on" : " off") << '\n';
}

void onGenerate(Fl_Widget * /*widget*/, void * /*data*/)
{
    if (!generateAll())
    {
        return;
    }
    refreshLabels();
    g_roll->redraw();
}

void onPlay(Fl_Widget * /*widget*/, void * /*data*/)
{
    int samples = 0;
    unsigned int fnv = 0;
    if (!writeWav(samples, fnv))
    {
        return;
    }
    launchPlayback();
    Fl::remove_timeout(play_tick);
    g_playStep = 0;
    g_roll->redraw();
    Fl::add_timeout(60.0 / static_cast<double>(g_tempo), play_tick);
    refreshLabels();
}

void onStop(Fl_Widget * /*widget*/, void * /*data*/)
{
    Fl::remove_timeout(play_tick);
    g_playStep = -1;
    g_roll->redraw();
    std::cout << "stopped\n";
}

void onTempo(Fl_Widget * /*widget*/, void *data)
{
    g_tempo = static_cast<int>(reinterpret_cast<std::intptr_t>(data));
    refreshLabels();
    g_roll->redraw();
    std::cout << "tempo " << g_tempo << '\n';
}

void onPreset(Fl_Widget * /*widget*/, void *data)
{
    g_preset = static_cast<int>(reinterpret_cast<std::intptr_t>(data));
    if (!loadMusic())
    {
        return;
    }
    refreshLabels();
    g_roll->redraw();
    std::cout << "preset " << g_preset << " recompiled\n";
}

bool expectStream(const char *label, int voice, const int *want)
{
    bool ok = true;
    for (int s = 0; s < kSteps; ++s)
    {
        if (g_notes[voice][s] != want[s])
        {
            std::cout << "MISMATCH " << label << " step " << s << ": got " << g_notes[voice][s] << " want " << want[s]
                      << '\n';
            ok = false;
        }
    }
    if (ok)
    {
        std::cout << label << ": 16/16 notes exact\n";
    }
    return ok;
}

bool expectEmpty(const char *label, int voice)
{
    for (int s = 0; s < kSteps; ++s)
    {
        if (g_notes[voice][s] != kRest || cellOn(voice, s))
        {
            std::cout << "MISMATCH " << label << " step " << s << ": note " << g_notes[voice][s] << " cell "
                      << cellOn(voice, s) << ", want rest/off\n";
            return false;
        }
    }
    std::cout << label << ": empty as expected\n";
    return true;
}

bool expectCell(const char *label, int voice, int step, bool want)
{
    const bool got = cellOn(voice, step);
    if (got == want)
    {
        std::cout << label << ": cell (" << voice << "," << step << ") = " << got << " as expected\n";
        return true;
    }
    std::cout << "MISMATCH " << label << ": cell (" << voice << "," << step << ") = " << got << " want " << want
              << '\n';
    return false;
}

bool expectWav(const char *label, std::size_t wantSize, unsigned int wantFnv)
{
    int samples = 0;
    unsigned int fnv = 0;
    if (!writeWav(samples, fnv))
    {
        return false;
    }
    std::error_code code;
    const std::size_t size = std::filesystem::file_size(g_wavPath, code);
    if (code)
    {
        std::cout << "MISMATCH " << label << ": cannot stat wav\n";
        return false;
    }
    bool ok = true;
    if (size != wantSize)
    {
        std::cout << "MISMATCH " << label << ": size " << size << " want " << wantSize << '\n';
        ok = false;
    }
    if (fnv != wantFnv)
    {
        std::cout << "MISMATCH " << label << ": fnv " << fnv << " want " << wantFnv << '\n';
        ok = false;
    }
    // Byte-exactness: rewrite and require identical bytes.
    int samples2 = 0;
    unsigned int fnv2 = 0;
    if (!writeWav(samples2, fnv2) || fnv2 != fnv || samples2 != samples)
    {
        std::cout << "MISMATCH " << label << ": rewrite not byte-identical\n";
        ok = false;
    }
    if (ok)
    {
        std::cout << label << ": " << size << " bytes, fnv " << fnv << ", rewrite identical\n";
    }
    return ok;
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

    for (int v = 0; v < kVoices; ++v)
    {
        if (!g_voiceOn[v])
        {
            g_voiceButtons[v]->do_callback();
        }
    }
    if (g_tempo != kTempoA)
    {
        g_tempoAButton->do_callback();
    }

    // Song A, all voices: exact streams, cells, 48 persistent calls.
    std::cout << "--- song A full generate ---\n";
    const int callsBefore = g_callCount;
    g_generateButton->do_callback();
    if (g_callCount != callsBefore + kSteps * kVoices)
    {
        std::cout << "MISMATCH: generate used " << (g_callCount - callsBefore) << " calls, want 48\n";
        ok = false;
    }
    ok &= expectStream("songA bass", 0, kBass0);
    ok &= expectStream("songA drums", 1, kDrums0);
    ok &= expectStream("songA arp", 2, kArp0);
    ok &= expectCell("songA cell bass", 0, 0, true);
    ok &= expectCell("songA cell rest", 1, 3, false);
    ok &= expectCell("songA cell arp top", 2, 11, true);

    // Play file at tempo 90: byte-exact wav.
    std::cout << "--- play at tempo 90 ---\n";
    g_playButton->do_callback();
    ok &= expectWav("wav90", 470444, kWav90Fnv);
    g_stopButton->do_callback();

    // Tempo switch: same notes, new bytes.
    std::cout << "--- tempo 140 ---\n";
    g_tempoBButton->do_callback();
    g_playButton->do_callback();
    ok &= expectWav("wav140", 302444, kWav140Fnv);
    g_stopButton->do_callback();
    g_tempoAButton->do_callback();

    // Preset switch recompiles and changes every voice deterministically.
    std::cout << "--- song B (preset switch) ---\n";
    const int loadsBefore = g_loadCount;
    g_songBButton->do_callback();
    if (g_loadCount != loadsBefore + 1)
    {
        std::cout << "MISMATCH: preset switch did not recompile\n";
        ok = false;
    }
    g_generateButton->do_callback();
    ok &= expectStream("songB bass", 0, kBass1);
    ok &= expectStream("songB drums", 1, kDrums1);
    ok &= expectStream("songB arp", 2, kArp1);

    // Per-voice generate: drums+arp off leaves only bass behind.
    std::cout << "--- per-voice generate ---\n";
    g_voiceButtons[1]->do_callback();
    g_voiceButtons[2]->do_callback();
    g_generateButton->do_callback();
    ok &= expectStream("solo bass", 0, kBass1);
    ok &= expectEmpty("muted drums", 1);
    ok &= expectEmpty("muted arp", 2);
    g_voiceButtons[1]->do_callback();
    g_voiceButtons[2]->do_callback();
    g_generateButton->do_callback();
    ok &= expectStream("restored drums", 1, kDrums1);
    ok &= expectStream("restored arp", 2, kArp1);

    // Back to song A: preset0 streams return exactly.
    std::cout << "--- back to song A ---\n";
    g_songAButton->do_callback();
    g_generateButton->do_callback();
    ok &= expectStream("songA bass again", 0, kBass0);
    ok &= expectStream("songA drums again", 1, kDrums0);
    ok &= expectStream("songA arp again", 2, kArp0);

    std::cout
        << "scale: 4 music files + 1 root | 48 calls/preset (44 sounded, 4 rests) | wav90 470444 wav140 302444 | loads "
        << g_loadCount << " calls " << g_callCount << " | compile " << g_compileMs << " ms gen " << g_genMs
        << " ms synth " << g_synthMs << " ms\n";
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

    try
    {
        std::unique_ptr<kern::IVirtualMachine> vm(kern::createVirtualMachine());
        vm->addFunction("cur_step", kern::createExternFunction(&cur_step_value));
        vm->addFunction("preset", kern::createExternFunction(&preset_value));
        vm->addFunction("emit", kern::createExternFunction(&emit_note_value));
        g_vm = vm.get();
        int candidate = -1;
        if (!resolveMusicDir(g_musicDir, candidate))
        {
            return 1;
        }
        g_searchPaths.push_back(g_musicDir);
        std::cout << "music dir: " << g_musicDir << " (candidate #" << candidate << ")\n";
        try
        {
            const std::filesystem::path exe(g_argv0);
            g_wavPath =
                ((exe.has_parent_path() ? exe.parent_path() : std::filesystem::current_path()) / "song.wav").string();
        }
        catch (const std::filesystem::filesystem_error &)
        {
            g_wavPath = (std::filesystem::current_path() / "song.wav").string();
        }
        if (!loadMusic())
        {
            return 1;
        }

        Fl_Window window(460, 600, kWindowTitle);
        g_window = &window;
        PianoRoll roll(30, 10, 400, 352);
        g_roll = &roll;
        Fl_Button bassButton(30, 370, 128, 30, "Bass: on");
        Fl_Button drumsButton(166, 370, 128, 30, "Drums: on");
        Fl_Button arpButton(302, 370, 128, 30, "Arp: on");
        bassButton.callback(onVoice, reinterpret_cast<void *>(static_cast<std::intptr_t>(0)));
        drumsButton.callback(onVoice, reinterpret_cast<void *>(static_cast<std::intptr_t>(1)));
        arpButton.callback(onVoice, reinterpret_cast<void *>(static_cast<std::intptr_t>(2)));
        g_voiceButtons[0] = &bassButton;
        g_voiceButtons[1] = &drumsButton;
        g_voiceButtons[2] = &arpButton;
        Fl_Button tempoAButton(30, 406, 128, 30, "Tempo 90 [*]");
        Fl_Button tempoBButton(166, 406, 128, 30, "Tempo 140");
        tempoAButton.callback(onTempo, reinterpret_cast<void *>(static_cast<std::intptr_t>(kTempoA)));
        tempoBButton.callback(onTempo, reinterpret_cast<void *>(static_cast<std::intptr_t>(kTempoB)));
        g_tempoAButton = &tempoAButton;
        g_tempoBButton = &tempoBButton;
        Fl_Button songAButton(302, 406, 62, 30, "Song A [*]");
        Fl_Button songBButton(368, 406, 62, 30, "Song B");
        songAButton.callback(onPreset, reinterpret_cast<void *>(static_cast<std::intptr_t>(0)));
        songBButton.callback(onPreset, reinterpret_cast<void *>(static_cast<std::intptr_t>(1)));
        g_songAButton = &songAButton;
        g_songBButton = &songBButton;
        Fl_Button generateButton(30, 442, 128, 30, "Generate");
        generateButton.callback(onGenerate);
        g_generateButton = &generateButton;
        Fl_Button playButton(166, 442, 128, 30, "Play file");
        playButton.callback(onPlay);
        g_playButton = &playButton;
        Fl_Button stopButton(302, 442, 128, 30, "Stop");
        stopButton.callback(onStop);
        g_stopButton = &stopButton;
        Fl_Box status(30, 478, 400, 60, "");
        g_status = &status;
        window.end();

        clearAllNotes();
        if (!generateAll())
        {
            return 1;
        }
        refreshLabels();

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
