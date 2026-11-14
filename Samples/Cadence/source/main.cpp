#include "../../Support/Preview.h"
#include "PianoGrid.h"
#include "audio.hpp"
#include "generator.hpp"
#include "song.hpp"

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_File_Chooser.H>
#include <FL/Fl_Value_Input.H>
#include <FL/Fl_Widget.H>
#include <FL/Fl_Window.H>
#include <FL/fl_ask.H>
#include <FL/fl_draw.H>
#include <kern/Kern.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace
{
using namespace cadence;
constexpr int RollLow = 36;
constexpr int RollHigh = 107;
constexpr int RollRows = RollHigh - RollLow + 1;
constexpr const char *TrackNames[Tracks] = {"Bass", "Drums", "Lead"};
constexpr Fl_Color TrackColors[Tracks] = {FL_RED, FL_DARK_GREEN, FL_BLUE};

class AppLogger final : public kern::ILogger
{
public:
    void onError(const std::string &message) override
    {
        if (!diagnostics.empty())
            diagnostics += '\n';
        diagnostics += message;
        std::cerr << message << '\n';
    }

    std::string diagnostics;
};

Song g_song;
AppLogger g_logger;
std::unique_ptr<kern::IVirtualMachine> g_vm;
std::filesystem::path g_musicDir;
std::filesystem::path g_outputDir;
std::filesystem::path g_currentFile;
std::string g_argv0;
std::string g_statusText = "Ready";
int g_track = 0;
int g_rangeFirst = 1;
int g_rangeLast = 32;
bool g_dirty = true;
Fl_Window *g_window = nullptr;
Fl_Widget *g_roll = nullptr;
Fl_Box *g_status = nullptr;
Fl_Button *g_trackButtons[Tracks]{};
Fl_Button *g_lengthButtons[3]{};
Fl_Button *g_scaleButton = nullptr;
Fl_Button *g_rootButton = nullptr;
Fl_Value_Input *g_tempoInput = nullptr;
Fl_Value_Input *g_rangeFirstInput = nullptr;
Fl_Value_Input *g_rangeLastInput = nullptr;

void setStatus(const std::string &text)
{
    g_statusText = text;
    if (g_status)
    {
        g_status->copy_label(text.c_str());
        g_status->redraw();
    }
}

void markDirty()
{
    g_dirty = true;
    if (g_window)
    {
        const std::string label =
            g_currentFile.empty() ? "Cadence | Untitled *" : "Cadence | " + g_currentFile.filename().string() + " *";
        g_window->copy_label(label.c_str());
    }
}

void updateTitle()
{
    if (!g_window)
        return;
    const std::string base = g_currentFile.empty() ? "Untitled" : g_currentFile.filename().string();
    const std::string label = "Cadence | " + base + (g_dirty ? " *" : "");
    g_window->copy_label(label.c_str());
}

bool reloadScripts(const std::string &source = rootSource())
{
    g_logger.diagnostics.clear();
    std::unique_ptr<kern::IVirtualMachine> candidate;
    std::string error;
    if (!compileCandidate(g_musicDir, &g_logger, candidate, error, source))
    {
        setStatus(g_logger.diagnostics.empty() ? error : g_logger.diagnostics);
        return false;
    }
    if (!validateCandidate(*candidate, g_song, error))
    {
        setStatus(g_logger.diagnostics.empty() ? error : g_logger.diagnostics + "\n" + error);
        return false;
    }
    g_vm = std::move(candidate);
    setStatus("Loaded music scripts from " + g_musicDir.filename().string());
    return true;
}

bool generateSelectedRange(int firstStep, int lastStep, bool allTracks)
{
    if (!g_vm)
    {
        setStatus("Load music scripts before generating");
        return false;
    }
    std::array<bool, Tracks> selected{};
    if (allTracks)
    {
        for (int track = 0; track < Tracks; ++track)
            selected[track] = !g_song.tracks[track].muted;
    }
    else
    {
        selected[g_track] = !g_song.tracks[g_track].muted;
    }
    std::string error;
    g_logger.diagnostics.clear();
    if (!generateTransaction(g_song, *g_vm, firstStep, lastStep, selected, error))
    {
        setStatus(g_logger.diagnostics.empty() ? error : g_logger.diagnostics + "\n" + error);
        return false;
    }
    markDirty();
    return true;
}

class PianoRoll final : public Fl_Widget
{
public:
    PianoRoll(int x, int y, int w, int h) : Fl_Widget(x, y, w, h) {}

    void draw() override
    {
        fl_push_clip(x(), y(), w(), h());
        fl_color(FL_BLACK);
        fl_rectf(x(), y(), w(), h());
        const auto stepX = [this](int step) { return x() + gridBoundary(step, g_song.steps(), w()); };
        const auto rowY = [this](int row) { return y() + gridBoundary(row, RollRows, h()); };
        for (int track = 0; track < Tracks; ++track)
        {
            if (g_song.tracks[track].muted)
                continue;
            fl_color(TrackColors[track]);
            for (int step = 0; step < g_song.steps(); ++step)
            {
                const int note = g_song.tracks[track].notes[step];
                if (note < RollLow || note > RollHigh)
                    continue;
                const int row = RollHigh - note;
                const int left = stepX(step);
                const int width = stepX(step + 1) - left;
                const int height = rowY(row + 1) - rowY(row);
                fl_rectf(left + 1, rowY(row) + 1, std::max(1, width - 2), std::max(1, height - 2));
            }
        }
        for (int step = 0; step <= g_song.steps(); ++step)
        {
            fl_color(step % 16 == 0 ? FL_GRAY : FL_DARK3);
            fl_line(stepX(step), y(), stepX(step), y() + h());
        }
        for (int row = 0; row <= RollRows; ++row)
        {
            const int note = RollHigh - row;
            fl_color(note % 12 == 0 ? FL_GRAY : FL_DARK3);
            fl_line(x(), rowY(row), x() + w(), rowY(row));
        }
        fl_color(FL_YELLOW);
        const int first = std::clamp(g_rangeFirst - 1, 0, g_song.steps());
        const int last = std::clamp(g_rangeLast, first, g_song.steps());
        fl_rect(stepX(first), y(), std::max(1, stepX(last) - stepX(first)), h());
        fl_pop_clip();
    }

    int handle(int event) override
    {
        if (event != FL_PUSH || Fl::event_button() != FL_LEFT_MOUSE)
            return Fl_Widget::handle(event);
        const int step = gridCell(Fl::event_x() - x(), g_song.steps(), w());
        const int row = gridCell(Fl::event_y() - y(), RollRows, h());
        if (step < 0 || row < 0)
            return 1;
        const int note = RollHigh - row;
        int &cell = g_song.tracks[g_track].notes[step];
        cell = cell == note ? Rest : note;
        markDirty();
        redraw();
        setStatus("Edited " + std::string(TrackNames[g_track]) + " step " + std::to_string(step + 1));
        return 1;
    }
};

void refresh()
{
    for (int track = 0; track < Tracks; ++track)
    {
        const std::string label = (track == g_track ? "> " : "") + std::string(TrackNames[track]) +
                                  (g_song.tracks[track].muted ? " [muted]" : " [on]") + " | instrument " +
                                  std::to_string(g_song.tracks[track].instrument + 1);
        g_trackButtons[track]->copy_label(label.c_str());
    }
    const int lengths[3] = {2, 4, 8};
    for (int i = 0; i < 3; ++i)
    {
        const std::string label = std::to_string(lengths[i]) + " bars" + (g_song.bars == lengths[i] ? " *" : "");
        g_lengthButtons[i]->copy_label(label.c_str());
    }
    g_tempoInput->value(g_song.tempo);
    g_rangeFirstInput->maximum(g_song.steps());
    g_rangeLastInput->maximum(g_song.steps());
    g_rangeFirst = std::clamp(g_rangeFirst, 1, g_song.steps());
    g_rangeLast = std::clamp(g_rangeLast, g_rangeFirst, g_song.steps());
    g_rangeFirstInput->value(g_rangeFirst);
    g_rangeLastInput->value(g_rangeLast);
    g_scaleButton->copy_label((std::string("Scale: ") + (g_song.scale == 0   ? "Major"
                                                         : g_song.scale == 1 ? "Minor"
                                                                             : "Pentatonic"))
                                  .c_str());
    static const char *roots[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    g_rootButton->copy_label((std::string("Root: ") + roots[g_song.root]).c_str());
    if (g_roll)
        g_roll->redraw();
    updateTitle();
}

void trackCallback(Fl_Widget *, void *data)
{
    const int track = static_cast<int>(reinterpret_cast<std::intptr_t>(data));
    if (track == g_track)
    {
        g_song.tracks[track].muted = !g_song.tracks[track].muted;
        markDirty();
    }
    else
        g_track = track;
    refresh();
}
void instrumentCallback(Fl_Widget *, void *)
{
    g_song.tracks[g_track].instrument = (g_song.tracks[g_track].instrument + 1) % 3;
    markDirty();
    refresh();
}
void lengthCallback(Fl_Widget *, void *data)
{
    g_song.bars = static_cast<int>(reinterpret_cast<std::intptr_t>(data));
    g_rangeFirst = 1;
    g_rangeLast = g_song.steps();
    markDirty();
    refresh();
}
void tempoCallback(Fl_Widget *, void *)
{
    const double value = g_tempoInput->value();
    if (!std::isfinite(value))
    {
        setStatus("BPM must be a finite number from 40 to 240");
        refresh();
        return;
    }
    g_song.tempo = static_cast<int>(std::lround(std::clamp(value, 40.0, 240.0)));
    markDirty();
    refresh();
}
void rangeCallback(Fl_Widget *, void *)
{
    if (!std::isfinite(g_rangeFirstInput->value()) || !std::isfinite(g_rangeLastInput->value()))
    {
        setStatus("Step range must contain finite numbers");
        refresh();
        return;
    }
    g_rangeFirst =
        static_cast<int>(std::lround(std::clamp(g_rangeFirstInput->value(), 1.0, static_cast<double>(g_song.steps()))));
    g_rangeLast = static_cast<int>(std::lround(
        std::clamp(g_rangeLastInput->value(), static_cast<double>(g_rangeFirst), static_cast<double>(g_song.steps()))));
    refresh();
}
void scaleCallback(Fl_Widget *, void *)
{
    g_song.scale = (g_song.scale + 1) % 3;
    markDirty();
    refresh();
}
void rootCallback(Fl_Widget *, void *)
{
    g_song.root = (g_song.root + 1) % 12;
    markDirty();
    refresh();
}
void generateTrackCallback(Fl_Widget *, void *)
{
    if (generateSelectedRange(g_rangeFirst - 1, g_rangeLast, false))
        setStatus(std::string(TrackNames[g_track]) + " generated for steps " + std::to_string(g_rangeFirst) + "-" +
                  std::to_string(g_rangeLast));
    refresh();
}
void generateAllCallback(Fl_Widget *, void *)
{
    if (generateSelectedRange(g_rangeFirst - 1, g_rangeLast, true))
        setStatus("All unmuted tracks generated for selected steps");
    refresh();
}
void reloadCallback(Fl_Widget *, void *) { reloadScripts(); }

bool saveAs();
bool saveCurrent()
{
    if (g_currentFile.empty())
        return saveAs();
    try
    {
        saveSong(g_song, g_currentFile);
        g_dirty = false;
        updateTitle();
        setStatus("Saved " + g_currentFile.filename().string());
        return true;
    }
    catch (const std::exception &exception)
    {
        setStatus(exception.what());
        return false;
    }
}
std::filesystem::path ensureExtension(std::filesystem::path path, const std::string &extension)
{
    if (path.extension() != extension)
        path += extension;
    return path;
}
bool saveAs()
{
    const std::string initial =
        g_currentFile.empty() ? (g_outputDir / "Untitled.cadence").string() : g_currentFile.string();
    const char *chosen = fl_file_chooser("Save Cadence Song As", "*.cadence", initial.c_str());
    if (!chosen)
        return false;
    const auto destination = ensureExtension(std::filesystem::path(chosen), ".cadence");
    try
    {
        if (std::filesystem::exists(destination) &&
            fl_choice("Replace the existing composition?", "Cancel", "Replace", nullptr) != 1)
            return false;
        saveSong(g_song, destination);
        g_currentFile = destination;
        g_dirty = false;
        updateTitle();
        setStatus("Saved " + g_currentFile.filename().string());
        return true;
    }
    catch (const std::exception &exception)
    {
        setStatus(exception.what());
        return false;
    }
}
bool confirmDiscardChanges()
{
    if (!g_dirty)
        return true;
    const int choice = fl_choice("Save changes to the current composition?", "Cancel", "Discard", "Save");
    if (choice == 0)
        return false;
    if (choice == 2)
        return saveCurrent();
    return true;
}

bool isCurrentDocument(const std::filesystem::path &path)
{
    if (g_currentFile.empty())
        return false;
    std::error_code error;
    return std::filesystem::weakly_canonical(path) == std::filesystem::weakly_canonical(g_currentFile) ||
           std::filesystem::equivalent(path, g_currentFile, error);
}
bool openDocument(const std::filesystem::path &path, std::string &error)
{
    try
    {
        Song candidate = openSong(path);
        g_song = candidate;
        g_currentFile = path;
        g_dirty = false;
        g_rangeFirst = 1;
        g_rangeLast = g_song.steps();
        refresh();
        return true;
    }
    catch (const std::exception &exception)
    {
        error = exception.what();
        return false;
    }
}
void saveCallback(Fl_Widget *, void *) { saveCurrent(); }
void saveAsCallback(Fl_Widget *, void *) { saveAs(); }
void openCallback(Fl_Widget *, void *)
{
    if (!confirmDiscardChanges())
        return;
    const std::string initial = g_currentFile.empty() ? g_outputDir.string() : g_currentFile.string();
    const char *chosen = fl_file_chooser("Open Cadence Song", "*.cadence", initial.c_str());
    if (!chosen)
        return;
    std::string error;
    if (openDocument(chosen, error))
    {
        setStatus("Opened " + g_currentFile.filename().string());
    }
    else
    {
        setStatus(error);
    }
}
void newCallback(Fl_Widget *, void *)
{
    if (!confirmDiscardChanges())
        return;
    g_song = Song{};
    g_currentFile.clear();
    g_rangeFirst = 1;
    g_rangeLast = g_song.steps();
    markDirty();
    refresh();
    setStatus("Started a new composition");
}
void exportCallback(Fl_Widget *, void *)
{
    const std::string initial =
        (g_currentFile.empty() ? g_outputDir / "song.wav" : g_currentFile.parent_path() / g_currentFile.stem())
            .string();
    const char *chosen = fl_file_chooser("Export WAV", "*.wav", initial.c_str());
    if (!chosen)
        return;
    try
    {
        const auto destination = ensureExtension(std::filesystem::path(chosen), ".wav");
        if (isCurrentDocument(destination))
            throw std::runtime_error("Choose another file so the composition keeps its notes");
        if (std::filesystem::exists(destination) &&
            fl_choice("Replace the existing WAV file?", "Cancel", "Replace", nullptr) != 1)
            return;
        exportWav(g_song, destination);
        setStatus("Exported WAV file");
    }
    catch (const std::exception &exception)
    {
        setStatus(exception.what());
    }
}
void closeCallback(Fl_Widget *widget, void *)
{
    if (confirmDiscardChanges())
        widget->hide();
}

bool resolveMusicDirectory(const std::filesystem::path &executable)
{
    std::vector<std::filesystem::path> candidates;
    if (executable.has_parent_path())
        candidates.push_back(executable.parent_path() / "music");
    candidates.push_back(std::filesystem::current_path() / "music");
    candidates.push_back(std::filesystem::current_path() / "Samples" / "Cadence" / "music");
    for (const auto &candidate : candidates)
    {
        if (std::filesystem::is_directory(candidate))
        {
            g_musicDir = candidate;
            return true;
        }
    }
    return false;
}

std::string readFile(const std::filesystem::path &path) { return samples::readText(path, 65536); }
void writeFile(const std::filesystem::path &path, const std::string &text)
{
    samples::writeAtomically(path, [&](std::ostream &out) { out << text; });
}
std::string replaceOnce(std::string source, const std::string &from, const std::string &to)
{
    const auto at = source.find(from);
    if (at == std::string::npos)
        throw std::runtime_error("Verifier fixture source did not contain its expected pattern");
    source.replace(at, from.size(), to);
    return source;
}

bool verifyHeadless(const std::filesystem::path &testRoot)
{
    try
    {
        const auto runDir = samples::testDirectory(testRoot);
        g_outputDir = runDir;
        g_song = Song{};
        g_song.bars = 8;
        g_song.tempo = 123;
        if (!reloadScripts())
            throw std::runtime_error(g_statusText);

        std::array<bool, Tracks> allTracks{true, true, true};
        std::string error;
        if (!generateTransaction(g_song, *g_vm, 0, g_song.steps(), allTracks, error))
            throw std::runtime_error(error);
        Song original = g_song;
        int noteCount = 0;
        for (const auto &track : g_song.tracks)
            for (int step = 0; step < g_song.steps(); ++step)
                if (track.notes[step] != Rest)
                    ++noteCount;
        if (noteCount < 50)
            throw std::runtime_error("Script generated too few notes for the eight-bar arrangement");

        Song changedKey = g_song;
        changedKey.root = 7;
        if (!generateTransaction(changedKey, *g_vm, 0, changedKey.steps(), allTracks, error))
            throw std::runtime_error(error);
        if (changedKey.tracks[0].notes == g_song.tracks[0].notes)
            throw std::runtime_error("Changing the root did not change generated notes");
        changedKey.scale = 1;
        Song minor = changedKey;
        if (!generateTransaction(minor, *g_vm, 0, minor.steps(), allTracks, error))
            throw std::runtime_error(error);
        if (minor.tracks[2].notes == changedKey.tracks[2].notes)
            throw std::runtime_error("Changing the scale did not change generated notes");

        g_song.tracks[0].notes[120] = 94;
        g_song.bars = 2;
        const auto document = runDir / "roundtrip.cadence";
        saveSong(g_song, document);
        saveSong(g_song, document); // Exercise replacement of an existing destination.
        const Song roundtrip = openSong(document);
        if (!(roundtrip == g_song))
            throw std::runtime_error("Cadence document did not roundtrip all 128 stored steps");
        g_currentFile = document;
        if (!isCurrentDocument(document))
            throw std::runtime_error("Export did not identify the current document");
        g_currentFile.clear();
        Song stable = g_song;
        const auto stableFile = g_currentFile;
        const bool stableDirty = g_dirty;
        const auto invalidDoc = runDir / "invalid.cadence";
        writeFile(invalidDoc, "CADENCE 1\n2 100 0 0\n0 0\nnot-a-note\n");
        std::string openError;
        if (openDocument(invalidDoc, openError))
            throw std::runtime_error("Invalid document was accepted");
        if (!(g_song == stable) || g_currentFile != stableFile || g_dirty != stableDirty)
            throw std::runtime_error("Invalid open changed the active composition");

        Song edited = original;
        edited.tracks[0].notes[5] = 79;
        edited.tracks[2].notes[20] = 65;
        const Song outside = edited;
        std::array<bool, Tracks> bassOnly{true, false, false};
        if (!generateTransaction(edited, *g_vm, 32, 48, bassOnly, error))
            throw std::runtime_error(error);
        if (edited.tracks[0].notes[5] != 79 || edited.tracks[2].notes[20] != 65 ||
            edited.tracks[1] != outside.tracks[1])
            throw std::runtime_error("Regional generation changed notes outside its selected track and range");

        const auto wavBefore = renderWav(original);
        if (wavBefore != renderWav(original))
            throw std::runtime_error("WAV rendering was not deterministic");
        Song wavControls = original;
        wavControls.tempo = 90;
        if (renderWav(wavControls) == wavBefore)
            throw std::runtime_error("Tempo did not affect WAV output");
        wavControls = original;
        wavControls.tracks[0].instrument = 1;
        if (renderWav(wavControls) == wavBefore)
            throw std::runtime_error("Instrument did not affect WAV output");
        wavControls = original;
        wavControls.tracks[0].muted = true;
        if (renderWav(wavControls) == wavBefore)
            throw std::runtime_error("Mute did not affect WAV output");
        const auto wavPath = runDir / "verify.wav";
        exportWav(original, wavPath);
        std::ifstream wav(wavPath, std::ios::binary);
        char header[44]{};
        wav.read(header, sizeof(header));
        if (std::string(header, 4) != "RIFF" || std::string(header + 8, 4) != "WAVE" ||
            std::string(header + 36, 4) != "data" || header[20] != 1 || header[22] != 1 || header[34] != 16 ||
            std::filesystem::file_size(wavPath) <= 44)
            throw std::runtime_error("WAV header or content was invalid");
        wav.seekg(44);
        bool audioContent = false;
        char sample = 0;
        while (wav.get(sample))
            if (sample != 0)
            {
                audioContent = true;
                break;
            }
        if (!audioContent)
            throw std::runtime_error("WAV sample payload was silent");

        const auto fixtureDir = runDir / "music-fixture";
        std::filesystem::create_directories(fixtureDir);
        for (const char *name : {"bass.ks", "drums.ks", "lead.ks", "theory.ks"})
            std::filesystem::copy_file(g_musicDir / name, fixtureDir / name);
        const auto savedDirectory = g_musicDir;
        const auto savedVm = g_vm.get();
        const Song savedSong = g_song;
        g_musicDir = fixtureDir;
        const std::string bassStart = "theory::pitch(-7 + bar)";
        const std::string bassAltered =
            replaceOnce(readFile(fixtureDir / "bass.ks"), bassStart, "theory::pitch(-6 + bar)");
        writeFile(fixtureDir / "bass.ks", bassAltered);
        if (!reloadScripts() || g_vm.get() == savedVm || !(g_song == savedSong))
            throw std::runtime_error("Valid script reload did not commit a new VM");
        Song altered = savedSong;
        if (!generateTransaction(altered, *g_vm, 0, altered.steps(), allTracks, error) ||
            altered.tracks[0].notes == savedSong.tracks[0].notes)
            throw std::runtime_error("Valid reload did not change generated behavior");

        const auto committedVm = g_vm.get();
        const Song beforeCompileFailure = savedSong;
        writeFile(fixtureDir / "bass.ks", "this is invalid");
        if (reloadScripts() || g_vm.get() != committedVm || !(g_song == beforeCompileFailure))
            throw std::runtime_error("Failed reload changed the working VM or composition");

        std::string noEntry = rootSource();
        noEntry = replaceOnce(noEntry, "func bass_note() -> void { emit(0, bass::pattern(cur_step())); }\n", "");
        if (reloadScripts(noEntry) || g_vm.get() != committedVm || !(g_song == beforeCompileFailure))
            throw std::runtime_error("Missing entry did not preserve the working VM and composition");

        writeFile(fixtureDir / "bass.ks", "module bass; import theory; export func pattern(step) -> value { if (step "
                                          "== 7) { var bad = 1 / 0; return bad; } return theory::pitch(0); }\n");
        if (reloadScripts() || g_vm.get() != committedVm || !(g_song == beforeCompileFailure))
            throw std::runtime_error("Nonzero-step runtime failure did not preserve the working VM and composition");

        std::unique_ptr<kern::IVirtualMachine> malformed;
        if (!compileCandidate(fixtureDir, &g_logger, malformed, error))
            throw std::runtime_error("Could not compile verifier fixture for transactional generation");
        Song unchanged = savedSong;
        if (generateTransaction(unchanged, *malformed, 0, unchanged.steps(), allTracks, error) ||
            !(unchanged == savedSong))
            throw std::runtime_error("Failed all-track generation partially changed the composition");

        std::string wrongTrack = rootSource();
        wrongTrack =
            replaceOnce(wrongTrack, "emit(0, bass::pattern(cur_step()))", "emit(1, bass::pattern(cur_step()))");
        std::unique_ptr<kern::IVirtualMachine> wrongTrackVm;
        if (!compileCandidate(fixtureDir, &g_logger, wrongTrackVm, error, wrongTrack) ||
            generateTransaction(unchanged, *wrongTrackVm, 0, 16, allTracks, error) || !(unchanged == savedSong))
            throw std::runtime_error("Wrong-track emission was accepted");
        std::string duplicate = rootSource();
        duplicate = replaceOnce(duplicate, "{ emit(0, bass::pattern(cur_step())); }",
                                "{ emit(0, bass::pattern(cur_step())); emit(0, bass::pattern(cur_step())); }");
        std::unique_ptr<kern::IVirtualMachine> duplicateVm;
        if (!compileCandidate(fixtureDir, &g_logger, duplicateVm, error, duplicate) ||
            generateTransaction(unchanged, *duplicateVm, 0, 16, allTracks, error) || !(unchanged == savedSong))
            throw std::runtime_error("Duplicate emission was accepted");

        g_musicDir = savedDirectory;
        std::cout
            << "Cadence verify: PASS (" << noteCount
            << " notes, 128-step storage, script reload transactions, region isolation, WAV and document checks)\n";
        return true;
    }
    catch (const std::exception &exception)
    {
        std::cerr << "Cadence verify failed: " << exception.what() << '\n';
        return false;
    }
}

} // namespace

int main(int argc, char **argv)
{
    try
    {
        g_argv0 = argc > 0 ? argv[0] : "";
        std::filesystem::path testRoot, preview;
        bool verify = false;
        for (int i = 1; i < argc; ++i)
        {
            const std::string argument = argv[i];
            if (argument == "--verify")
                verify = true;
            else if (argument == "--test-root" && i + 1 < argc)
                testRoot = argv[++i];
            else if (argument == "--preview" && i + 1 < argc)
                preview = argv[++i];
            else if (argument == "--help")
            {
                std::cout << "Cadence: algorithmic composition and WAV export.\nCadence --verify [--test-root DIR] "
                             "runs without windows or playback.\n";
                return 0;
            }
            else
                throw std::invalid_argument("Unknown or incomplete argument: " + argument);
        }
        if (!resolveMusicDirectory(g_argv0))
        {
            std::cerr << "Cadence music scripts were not found\n";
            return 1;
        }
        g_outputDir = testRoot.empty() ? (std::filesystem::path(g_argv0).has_parent_path()
                                              ? std::filesystem::path(g_argv0).parent_path()
                                              : std::filesystem::current_path())
                                       : testRoot;
        if (verify)
            return verifyHeadless(g_outputDir) ? 0 : 1;
        if (!reloadScripts())
            return 1;
        std::string generationError;
        std::array<bool, Tracks> initialTracks{true, true, true};
        if (!generateTransaction(g_song, *g_vm, 0, g_song.steps(), initialTracks, generationError))
        {
            std::cerr << generationError << '\n';
            return 1;
        }

        Fl_Window window(1180, 790, "Cadence | Untitled *");
        g_window = &window;
        PianoRoll roll(20, 20, 1140, 500);
        g_roll = &roll;
        Fl_Button bass(20, 535, 360, 32, "Bass");
        Fl_Button drums(400, 535, 360, 32, "Drums");
        Fl_Button lead(780, 535, 360, 32, "Lead");
        g_trackButtons[0] = &bass;
        g_trackButtons[1] = &drums;
        g_trackButtons[2] = &lead;
        bass.callback(trackCallback, reinterpret_cast<void *>(static_cast<std::intptr_t>(0)));
        drums.callback(trackCallback, reinterpret_cast<void *>(static_cast<std::intptr_t>(1)));
        lead.callback(trackCallback, reinterpret_cast<void *>(static_cast<std::intptr_t>(2)));

        Fl_Button instrument(20, 575, 125, 30, "Instrument +");
        instrument.callback(instrumentCallback);
        Fl_Button generateTrack(155, 575, 135, 30, "Generate Track");
        generateTrack.callback(generateTrackCallback);
        Fl_Button generateAll(300, 575, 120, 30, "Generate All");
        generateAll.callback(generateAllCallback);
        Fl_Button reload(430, 575, 130, 30, "Reload Scripts");
        reload.callback(reloadCallback);
        Fl_Box rangeLabel(575, 575, 58, 30, "Steps:");
        Fl_Box firstLabel(635, 575, 43, 30, "From");
        Fl_Value_Input first(680, 575, 55, 30);
        g_rangeFirstInput = &first;
        first.minimum(1);
        first.step(1);
        first.when(FL_WHEN_RELEASE | FL_WHEN_ENTER_KEY);
        first.callback(rangeCallback);
        Fl_Box lastLabel(745, 575, 28, 30, "To");
        Fl_Value_Input last(775, 575, 55, 30);
        g_rangeLastInput = &last;
        last.minimum(1);
        last.step(1);
        last.when(FL_WHEN_RELEASE | FL_WHEN_ENTER_KEY);
        last.callback(rangeCallback);

        Fl_Button twoBars(20, 615, 100, 30, "2 bars");
        Fl_Button fourBars(130, 615, 100, 30, "4 bars");
        Fl_Button eightBars(240, 615, 100, 30, "8 bars");
        g_lengthButtons[0] = &twoBars;
        g_lengthButtons[1] = &fourBars;
        g_lengthButtons[2] = &eightBars;
        twoBars.callback(lengthCallback, reinterpret_cast<void *>(static_cast<std::intptr_t>(2)));
        fourBars.callback(lengthCallback, reinterpret_cast<void *>(static_cast<std::intptr_t>(4)));
        eightBars.callback(lengthCallback, reinterpret_cast<void *>(static_cast<std::intptr_t>(8)));
        Fl_Box tempoLabel(350, 615, 36, 30, "BPM");
        Fl_Value_Input tempo(388, 615, 52, 30);
        g_tempoInput = &tempo;
        tempo.minimum(40);
        tempo.maximum(240);
        tempo.step(1);
        tempo.when(FL_WHEN_RELEASE | FL_WHEN_ENTER_KEY);
        tempo.callback(tempoCallback);
        Fl_Button root(460, 615, 125, 30, "Root");
        g_rootButton = &root;
        root.callback(rootCallback);
        Fl_Button scale(595, 615, 165, 30, "Scale");
        g_scaleButton = &scale;
        scale.callback(scaleCallback);

        Fl_Button newSong(780, 615, 90, 30, "New");
        newSong.callback(newCallback);
        Fl_Button open(880, 615, 90, 30, "Open");
        open.callback(openCallback);
        Fl_Button save(980, 615, 90, 30, "Save");
        save.callback(saveCallback);
        Fl_Button saveAsButton(1080, 615, 80, 30, "Save As");
        saveAsButton.callback(saveAsCallback);
        Fl_Button exportWavButton(980, 655, 180, 30, "Export WAV");
        exportWavButton.callback(exportCallback);
        Fl_Box help(20, 655, 920, 55,
                    "Click a piano-roll cell to toggle a note. Select a track, set the step range, then regenerate. "
                    "The full 128-step document is kept when you shorten the visible song.");
        help.align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_WRAP);
        Fl_Box status(20, 730, 1140, 36, "Ready");
        status.align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE | FL_ALIGN_WRAP);
        g_status = &status;
        window.callback(closeCallback);
        window.end();
        g_rangeFirst = 1;
        g_rangeLast = g_song.steps();
        refresh();
        if (!preview.empty())
        {
            samples::renderPreview(window, preview);
            return 0;
        }
        window.show(argc, argv);
        return Fl::run();
    }
    catch (const std::exception &exception)
    {
        std::cerr << "Cadence: " << exception.what() << '\n';
        return 1;
    }
}
