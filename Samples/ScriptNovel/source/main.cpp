// ScriptNovel: a visual-novel engine where chapters are script MODULES.
//
// Story logic lives in chapters/*.ks; the host owns the window, the beat
// cursor, flags/inventory and the hero name, and renders what the script
// emits. Content stress: ~150 dialogue string literals across a diamond
// import tree (chapter > cast + world, world > cast; cast loads once and
// is shared), string == dispatch, string-valued module funcs.
//
// Host protocol (tiny, fixed; mirrored in chapters/chapter*.ks):
//   beat() value-use: host beat cursor. Script keeps no state across
//     calls, so every play_beat() pulls beat() and runs one if-branch.
//   get_flag(name) value-use: host flag value, 0 when never set.
//   last_choice() value-use: 1-3 after a choice press, else 0.
//   hero() value-use: host-owned hero name string.
//   say(actor, text) statement-use: appends "actor: text" to this beat.
//   offer(a, b, c) statement-use: choice labels ("" hides that button).
//   set_scene(text) statement-use: title bar for this beat.
//   set_flag(name, v) statement-use: host stores v under name.
// Flow: Next sets last_choice 0, beat++, calls. A choice press sets
// last_choice 1-3 and re-calls the SAME beat, so choice beats first
// offer (last_choice 0) then resolve. Chapter buttons hot-swap: recompile
// the new chapter root, reset beat/flags. Reload recompiles in place and
// keeps beat/flags (hot-reload by design, like ScriptPaint).
//
// Chapter roots are built like ScriptPaint: anonymous root importing the
// chapter module and calling chapterN::play_beat() through main().

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Browser.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Widget.H>
#include <FL/Fl_Window.H>
#include <FL/fl_draw.H>

#include <kern/Kern.h>

namespace
{

constexpr const char *kWindowTitle = "KernScript Novel";
constexpr const char *kHeroName = "They call you Ash of the Ember Gate, and the hollow is listening.";
constexpr int kMaxBeat = 15;
constexpr int kBeatsPerChapter = 16;

struct Chapter
{
    const char *name;
    const char *file;
    const char *mod;
};

const Chapter kChapter1 = {"Ch1", "chapter1.ks", "chapter1"};
const Chapter kChapter2 = {"Ch2", "chapter2.ks", "chapter2"};

std::string g_argv0;
std::string g_chaptersDir;
std::vector<std::string> g_searchPaths;
kern::IVirtualMachine *g_vm = nullptr;

int g_beat = 0;
int g_lastChoice = 0;
std::map<std::string, int> g_flags;
std::string g_scene;
std::string g_actor;
std::string g_choices[3];
std::vector<std::string> g_beatLines;
const Chapter *g_chapter = &kChapter1;
int g_loadCount = 0;
int g_callCount = 0;
int g_chapterLines = 0;
int g_chapterLiterals = 0;
long long g_compileMs = 0;
int g_asmLines = 0;

Fl_Window *g_window = nullptr;
Fl_Browser *g_log = nullptr;
Fl_Box *g_sceneBox = nullptr;
Fl_Box *g_actorBox = nullptr;
Fl_Button *g_choiceButtons[3] = {nullptr, nullptr, nullptr};
Fl_Button *g_ch1Button = nullptr;
Fl_Button *g_ch2Button = nullptr;
Fl_Button *g_reloadButton = nullptr;
Fl_Button *g_nextButton = nullptr;
Fl_Box *g_status = nullptr;

int beat_value() { return g_beat; }

int get_flag_value(std::string name)
{
    const auto found = g_flags.find(name);
    if (found == g_flags.end())
    {
        return 0;
    }
    return found->second;
}

int last_choice_value() { return g_lastChoice; }

std::string hero_value() { return kHeroName; }

void say_text(std::string actor, std::string text)
{
    g_actor = actor;
    g_beatLines.push_back(actor + ": " + text);
    g_log->add((actor + ": " + text).c_str());
}

void offer_text(std::string a, std::string b, std::string c)
{
    g_choices[0] = a;
    g_choices[1] = b;
    g_choices[2] = c;
}

void set_scene_text(std::string text) { g_scene = text; }

void set_flag_value(std::string name, int value) { g_flags[name] = value; }

class ActorChip : public Fl_Widget
{
public:
    ActorChip(int x, int y, int w, int h) : Fl_Widget(x, y, w, h) {}

    void draw() override
    {
        fl_push_clip(x(), y(), w(), h());
        Fl_Color fill = FL_GRAY;
        if (g_actor == "Mara the Mage")
        {
            fill = FL_BLUE;
        }
        else if (g_actor == "Bram the Blacksmith")
        {
            fill = FL_RED;
        }
        else if (g_actor == "Wren the Wayfinder")
        {
            fill = FL_DARK_GREEN;
        }
        else if (g_actor == "Narrator")
        {
            fill = FL_DARK_YELLOW;
        }
        fl_color(fill);
        fl_rectf(x(), y(), w(), h());
        fl_color(FL_BLACK);
        fl_rect(x(), y(), w(), h());
        fl_pop_clip();
    }
};

ActorChip *g_chip = nullptr;

void refreshUi()
{
    g_sceneBox->copy_label(g_scene.c_str());
    g_actorBox->copy_label(g_actor.c_str());
    for (int i = 0; i < 3; ++i)
    {
        if (g_choices[i].empty())
        {
            g_choiceButtons[i]->label("");
            g_choiceButtons[i]->deactivate();
        }
        else
        {
            g_choiceButtons[i]->copy_label(g_choices[i].c_str());
            g_choiceButtons[i]->activate();
        }
    }
    const std::string text = std::string(g_chapter->name) + " | beat " + std::to_string(g_beat) + " | flags " +
                             std::to_string(g_flags.size()) + " | loads " + std::to_string(g_loadCount);
    g_status->copy_label(text.c_str());
    g_chip->redraw();
}

int countLines(const std::string &text)
{
    if (text.empty())
    {
        return 0;
    }
    int lines = 0;
    for (char c : text)
    {
        if (c == '\n')
        {
            ++lines;
        }
    }
    if (text.back() != '\n')
    {
        ++lines;
    }
    return lines;
}

// Distinct double-quoted literals on single lines: each becomes a string
// table entry when compiled, so this estimates string table pressure.
int countDistinctStrings(const std::string &text)
{
    std::set<std::string> seen;
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line))
    {
        std::size_t pos = 0;
        while ((pos = line.find('"', pos)) != std::string::npos)
        {
            const std::size_t end = line.find('"', pos + 1);
            if (end == std::string::npos)
            {
                break;
            }
            seen.insert(line.substr(pos + 1, end - pos - 1));
            pos = end + 1;
        }
    }
    return static_cast<int>(seen.size());
}

// Chapters live as separate files under chapters/. Resolve robustly so
// --verify works from the build tree: exe-dir/chapters wins, then
// CWD/chapters, then CWD/Samples/ScriptNovel/chapters (repo-root runs).
bool resolveChaptersDir(std::string &out, int &candidate)
{
    std::vector<std::string> tries;
    try
    {
        const std::filesystem::path exe(g_argv0);
        if (exe.has_parent_path())
        {
            tries.push_back((exe.parent_path() / "chapters").string());
        }
        else
        {
            tries.push_back((std::filesystem::current_path() / "chapters").string());
        }
    }
    catch (const std::filesystem::filesystem_error &)
    {
        tries.push_back((std::filesystem::current_path() / "chapters").string());
    }
    tries.push_back((std::filesystem::current_path() / "chapters").string());
    tries.push_back((std::filesystem::current_path() / "Samples" / "ScriptNovel" / "chapters").string());
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
    std::cerr << "chapters dir missing (tried " << tries.size() << " candidates:";
    for (const std::string &t : tries)
    {
        std::cerr << " [" << t << "]";
    }
    std::cerr << ")\n";
    return false;
}

bool findChapter(const char *file, std::string &out)
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
    std::cerr << "chapter file missing: " << file << " (searched " << g_searchPaths.size() << " dirs)\n";
    return false;
}

std::string chapterSourceSize(const std::string &path)
{
    std::ifstream file(path, std::ios::binary);
    std::stringstream source;
    source << file.rdbuf();
    return source.str();
}

bool loadChapter(const Chapter &chapter)
{
    std::string path;
    if (!findChapter(chapter.file, path))
    {
        return false;
    }
    // Anonymous root importing the chapter module (ScriptPaint pattern):
    // the chapter body (with its own cast+world diamond) compiles fresh on
    // every switch and reload, so edits apply without restarting the app.
    const std::string root =
        std::string("import ") + chapter.mod + ";\nfunc main() {\n  " + chapter.mod + "::play_beat();\n}\n";
    const std::string sourceName = std::string("<novel-root:") + chapter.name + ">";
    std::stringstream asmCode;
    {
        const auto start = std::chrono::steady_clock::now();
        std::istringstream input(root);
        std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
        if (!compiler->compile(input, asmCode, sourceName, g_searchPaths))
        {
            std::cerr << "compile failed for " << chapter.name << '\n';
            return false;
        }
        const auto end = std::chrono::steady_clock::now();
        g_compileMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    }
    const std::string asmText = asmCode.str();
    g_asmLines = countLines(asmText);
    const std::string chapterText = chapterSourceSize(path);
    g_chapterLines = countLines(chapterText);
    g_chapterLiterals = countDistinctStrings(chapterText);

    std::stringstream byteCode;
    {
        std::istringstream input(asmText);
        std::unique_ptr<kern::IAssembler> assembler(kern::createAssembler());
        if (!assembler->assemble(input, byteCode))
        {
            std::cerr << "assemble failed for " << chapter.name << '\n';
            return false;
        }
    }
    try
    {
        if (!g_vm->load(byteCode))
        {
            std::cerr << "load failed for " << chapter.name << '\n';
            return false;
        }
    }
    catch (const kern::RuntimeError &error)
    {
        std::cerr << "load error: " << error.what() << '\n';
        return false;
    }
    ++g_loadCount;
    g_chapter = &chapter;
    std::cout << "chapter " << chapter.name << " loaded from " << path << " (load #" << g_loadCount
              << "): " << g_chapterLines << " lines, " << g_compileMs << " ms, " << g_asmLines << " asm lines, "
              << g_chapterLiterals << " strings\n";
    return true;
}

bool callBeat()
{
    g_beatLines.clear();
    g_choices[0].clear();
    g_choices[1].clear();
    g_choices[2].clear();
    try
    {
        if (!g_vm->callFunction("main"))
        {
            std::cerr << "function main not found\n";
            return false;
        }
    }
    catch (const kern::RuntimeError &error)
    {
        std::cerr << "runtime error at beat " << g_beat << ": " << error.what() << '\n';
        return false;
    }
    ++g_callCount;
    if (g_beatLines.empty())
    {
        std::cerr << "script emitted no dialogue at beat " << g_beat << " (missing say)\n";
        return false;
    }
    refreshUi();
    std::cout << "beat " << g_beat << ": " << g_beatLines.size() << " lines, scene '" << g_scene << "'\n";
    return true;
}

void onNext(Fl_Widget * /*widget*/, void * /*data*/)
{
    g_lastChoice = 0;
    if (g_beat < kMaxBeat)
    {
        ++g_beat;
    }
    if (!callBeat())
    {
        return;
    }
    std::cout << "next -> beat " << g_beat << '\n';
}

void onChoice(Fl_Widget *widget, void *data)
{
    const int index = static_cast<int>(reinterpret_cast<std::intptr_t>(data));
    if (g_choices[index].empty())
    {
        return;
    }
    (void)widget;
    g_lastChoice = index + 1;
    if (!callBeat())
    {
        return;
    }
    std::cout << "choice " << (index + 1) << " at beat " << g_beat << '\n';
}

void onChapter(Fl_Widget * /*widget*/, void *data)
{
    const Chapter *chapter = static_cast<const Chapter *>(data);
    if (!loadChapter(*chapter))
    {
        return;
    }
    g_beat = 0;
    g_lastChoice = 0;
    g_flags.clear();
    if (!callBeat())
    {
        return;
    }
    refreshUi();
    std::cout << "switched to " << chapter->name << " (fresh flags)\n";
}

void onReload(Fl_Widget * /*widget*/, void * /*data*/)
{
    if (!loadChapter(*g_chapter))
    {
        return;
    }
    if (!callBeat())
    {
        return;
    }
    refreshUi();
    std::cout << "reloaded " << g_chapter->name << " live at beat " << g_beat << '\n';
}

bool expectLine(const char *label, std::size_t index, const char *want)
{
    if (index >= g_beatLines.size())
    {
        std::cout << "MISMATCH " << label << ": beat has " << g_beatLines.size() << " lines, want index " << index
                  << '\n';
        return false;
    }
    if (g_beatLines[index] == want)
    {
        std::cout << label << ": line match (" << g_beatLines[index].size() << " chars)\n";
        return true;
    }
    std::cout << "MISMATCH " << label << ":\n  got  '" << g_beatLines[index] << "'\n  want '" << want << "'\n";
    return false;
}

bool expectLines(const char *label, std::size_t want)
{
    if (g_beatLines.size() == want)
    {
        std::cout << label << ": " << want << " lines as expected\n";
        return true;
    }
    std::cout << "MISMATCH " << label << ": got " << g_beatLines.size() << " lines, want " << want << '\n';
    for (const std::string &line : g_beatLines)
    {
        std::cout << "  have '" << line << "'\n";
    }
    return false;
}

bool expectFlag(const char *label, const char *name, int want)
{
    const int got = get_flag_value(name);
    if (got == want)
    {
        std::cout << label << ": flag " << name << " = " << got << " as expected\n";
        return true;
    }
    std::cout << "MISMATCH " << label << ": flag " << name << " = " << got << " want " << want << '\n';
    return false;
}

bool expectOffer(const char *label, const char *a, const char *b, const char *c)
{
    bool ok = true;
    const char *wants[3] = {a, b, c};
    for (int i = 0; i < 3; ++i)
    {
        const char *shown = g_choiceButtons[i]->label() != nullptr ? g_choiceButtons[i]->label() : "";
        const bool active = g_choiceButtons[i]->active() != 0;
        const bool wantActive = wants[i][0] != '\0';
        if (std::string(shown) != wants[i] || active != wantActive)
        {
            std::cout << "MISMATCH " << label << " button " << (i + 1) << ": got '" << shown << "' active " << active
                      << ", want '" << wants[i] << "' active " << wantActive << '\n';
            ok = false;
        }
    }
    if (ok)
    {
        std::cout << label << ": offers match\n";
    }
    return ok;
}

bool expectNoOffers(const char *label) { return expectOffer(label, "", "", ""); }

bool expectScene(const char *label, const char *want)
{
    if (g_scene == want)
    {
        std::cout << label << ": scene match\n";
        return true;
    }
    std::cout << "MISMATCH " << label << ":\n  got  '" << g_scene << "'\n  want '" << want << "'\n";
    return false;
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

    // Run 1: chapter 1, kind path (help/take/trust/stand) -> ending 1A.
    std::cout << "--- run 1: ch1 kind path ---\n";
    ok &= expectScene("r1 scene", "Chapter 1: The Ember Gate");
    ok &= expectLines("r1 b0", 4);
    ok &= expectLine("r1 b0 first", 0, "Narrator: The Ember Gate has stood a hundred years. Tonight it trembles.");
    ok &= expectNoOffers("r1 b0");

    g_nextButton->do_callback(); // b1
    ok &= expectLines("r1 b1", 4);
    ok &=
        expectLine("r1 b1 first", 0, "Mara the Mage: You heard it too. Good. Most sleep through the end of the world.");
    ok &= expectFlag("r1 b1", "met_mage", 1);

    g_nextButton->do_callback(); // b2 offer
    ok &= expectLines("r1 b2 offer", 1);
    ok &= expectLine("r1 b2 prompt", 0, "Mara the Mage: Choose now. The chalk will not wait, and neither will I.");
    ok &= expectOffer("r1 b2", "Help Mara lift the seal", "Refuse and walk on", "");

    g_choiceButtons[0]->do_callback(); // choose help, same beat
    ok &= expectLines("r1 b2 resolve", 2);
    ok &= expectLine("r1 b2 help", 0, "Mara the Mage: Then take this chalk. Draw where I point, and do not smudge.");
    ok &= expectFlag("r1 b2", "helped_mage", 1);

    g_nextButton->do_callback(); // b3
    ok &= expectLines("r1 b3", 4);
    ok &= expectLine("r1 b3 first", 0, "Mara the Mage: First rune: ember. It means warmth, and also warning.");
    ok &= expectFlag("r1 b3", "rune_known", 1);

    g_nextButton->do_callback(); // b4 offer
    ok &= expectLines("r1 b4 offer", 3);
    ok &= expectLine("r1 b4 prompt", 0,
                     "Bram the Blacksmith: You there. Gate forge is dark, and my spare lantern hangs unused.");
    ok &= expectOffer("r1 b4", "Take the lantern", "Leave it hanging", "");

    g_choiceButtons[0]->do_callback(); // take lantern
    ok &= expectLines("r1 b4 resolve", 2);
    ok &= expectLine("r1 b4 take", 0, "Bram the Blacksmith: Trim the wick when it smokes. Oil is in the base.");
    ok &= expectFlag("r1 b4", "has_lantern", 1);

    g_nextButton->do_callback(); // b5
    ok &= expectLines("r1 b5", 5);
    ok &= expectLine("r1 b5 first", 0, "Narrator: The tunnel takes your light and gives back long shadows.");
    ok &= expectLine("r1 b5 confirm", 4, "Narrator: Mara's chalk marks glow on the walls, pointing the way.");

    g_nextButton->do_callback(); // b6
    ok &= expectLines("r1 b6", 3);
    ok &= expectLine("r1 b6 first", 0, "Wren the Wayfinder: Lost? Everyone is lost down here. I sell directions.");
    ok &= expectFlag("r1 b6", "met_wren", 1);

    g_nextButton->do_callback(); // b7 offer
    ok &= expectOffer("r1 b7", "Trust Wren's shortcut", "Take the long road", "");
    g_choiceButtons[0]->do_callback(); // trust
    ok &= expectLine("r1 b7 trust", 0, "Wren the Wayfinder: Wise. Hold my sleeve at the third turning.");
    ok &= expectFlag("r1 b7", "trusts_wren", 1);

    g_nextButton->do_callback(); // b8
    ok &= expectLine("r1 b8 first", 0, "Wren the Wayfinder: Third turning. Duck. The pipe drips, mind your eyes.");

    g_nextButton->do_callback(); // b9
    ok &= expectLine("r1 b9 first", 0, "Mara the Mage: The runes you drew hold the near arch. Feel it steady?");
    ok &= expectFlag("r1 b9", "seal_down", 1);

    g_nextButton->do_callback(); // b10
    ok &= expectLine("r1 b10 first", 0, "Narrator: Your lantern still burns, oil barely touched. Bram overfilled it.");

    g_nextButton->do_callback(); // b11
    ok &=
        expectLine("r1 b11 first", 0, "Wren the Wayfinder: You held my sleeve. I do not forget that. Take this token.");

    g_nextButton->do_callback(); // b12
    ok &= expectLine("r1 b12 first", 0,
                     "Mara the Mage: Whatever comes through the gate tonight, we face it chalked and ready.");

    g_nextButton->do_callback(); // b13 offer
    ok &= expectOffer("r1 b13", "Stand at the gate", "Flee into the night", "");
    g_choiceButtons[0]->do_callback(); // stand
    ok &= expectLine("r1 b13 stand", 0, "Narrator: You plant your feet on the threshold line and do not move.");
    ok &= expectFlag("r1 b13", "stood_fast", 1);

    g_nextButton->do_callback(); // b14
    ok &= expectLine("r1 b14 first", 0, "Narrator: The seal splits with a sound like winter ice on a lake.");

    g_nextButton->do_callback(); // b15 ending 1A
    ok &= expectLines("r1 ending", 4);
    ok &= expectLine("r1 ending first", 0, "Narrator: Ending 1A: Dawn of the Ember Gate");
    ok &= expectLine("r1 ending last", 3,
                     "Mara the Mage: Ember Gate stands because two pairs of hands held it. Remember this.");
    ok &= expectFlag("r1 ending", "ending1a", 1);

    g_nextButton->do_callback(); // clamp: still b15
    ok &= expectLine("r1 clamp", 0, "Narrator: Ending 1A: Dawn of the Ember Gate");

    // Run 2: chapter 1 reset, cold path (refuse/leave/alone/flee) -> ending 1B.
    std::cout << "--- run 2: ch1 cold path ---\n";
    const int loadsBeforeReset = g_loadCount;
    g_ch1Button->do_callback();
    if (g_loadCount != loadsBeforeReset + 1)
    {
        std::cout << "MISMATCH: ch1 reset did not recompile\n";
        ok = false;
    }
    ok &= expectFlag("r2 fresh", "helped_mage", 0);
    ok &= expectFlag("r2 fresh", "ending1a", 0);
    ok &= expectLine("r2 b0 first", 0, "Narrator: The Ember Gate has stood a hundred years. Tonight it trembles.");

    g_nextButton->do_callback();       // b1
    g_nextButton->do_callback();       // b2 offer
    g_choiceButtons[1]->do_callback(); // refuse
    ok &= expectLine("r2 b2 refuse", 0, "Mara the Mage: Walk on, then. The gate will remember that you walked.");
    ok &= expectFlag("r2 b2", "refused_mage", 1);

    g_nextButton->do_callback(); // b3
    ok &=
        expectLine("r2 b3 first", 0, "Mara the Mage: No chalk for the unwilling. Watch, if you must, from the steps.");

    g_nextButton->do_callback();       // b4 offer
    g_choiceButtons[1]->do_callback(); // leave lantern
    ok &= expectLine("r2 b4 leave", 0, "Bram the Blacksmith: Proud. The dark loves the proud.");
    ok &= expectFlag("r2 b4", "left_lantern", 1);

    g_nextButton->do_callback(); // b5 dark branch
    ok &= expectLine("r2 b5 first", 0, "Narrator: The tunnel is a throat, and you walk down it blind.");

    g_nextButton->do_callback();       // b6
    g_nextButton->do_callback();       // b7 offer
    g_choiceButtons[1]->do_callback(); // alone
    ok &= expectLine("r2 b7 alone", 0, "Wren the Wayfinder: Suit yourself. The guards bite, but only a little.");
    ok &= expectFlag("r2 b7", "went_alone", 1);

    g_nextButton->do_callback(); // b8
    ok &= expectLine("r2 b8 first", 0, "Narrator: The long road climbs past torch after torch, guard after guard.");

    g_nextButton->do_callback(); // b9
    ok &= expectLine("r2 b9 first", 0, "Mara the Mage: The near arch shudders. Nobody drew the holding runes.");

    g_nextButton->do_callback(); // b10
    ok &= expectLine("r2 b10 first", 0, "Narrator: The dark presses close now, curious, tasting your edges.");

    g_nextButton->do_callback(); // b11
    g_nextButton->do_callback(); // b12
    ok &= expectLine("r2 b12 first", 0,
                     "Bram the Blacksmith: The mage works alone up there. Alone is how mistakes happen.");

    g_nextButton->do_callback();       // b13 offer
    g_choiceButtons[1]->do_callback(); // flee
    ok &= expectLine("r2 b13 flee", 0, "Narrator: You run, and the stairs blur, and the night takes you.");
    ok &= expectFlag("r2 b13", "fled", 1);

    g_nextButton->do_callback(); // b14
    g_nextButton->do_callback(); // b15 ending 1B
    ok &= expectLines("r2 ending", 4);
    ok &= expectLine("r2 ending first", 0, "Narrator: Ending 1B: Ashes on the Wind");
    ok &= expectLine("r2 ending last", 3, "Narrator: Somewhere the wind carries chalk dust out to sea.");
    ok &= expectFlag("r2 ending", "ending1b", 1);

    // Run 3: chapter 2 hot-swap, charted path -> ending 2A.
    std::cout << "--- run 3: ch2 charted path ---\n";
    g_ch2Button->do_callback();
    ok &= expectScene("r3 scene", "Chapter 2: The Hollow Road");
    ok &= expectFlag("r3 fresh", "has_lantern", 0);
    ok &= expectLines("r3 b0", 4);
    ok &= expectLine("r3 b0 first", 0, "Narrator: The Ember Gate is behind you. The Hollow Road is ahead.");
    ok &= expectLine("r3 b0 hero", 1, std::string("Narrator: " + std::string(kHeroName)).c_str());

    g_nextButton->do_callback(); // b1 offer
    ok &= expectOffer("r3 b1", "Accept the map", "Refuse the map", "");
    g_choiceButtons[0]->do_callback(); // accept
    ok &= expectLine("r3 b1 accept", 0, "Wren the Wayfinder: Roll it tight. Wet paper lies, and dry paper saves.");
    ok &= expectFlag("r3 b1", "has_map", 1);

    g_nextButton->do_callback(); // b2
    ok &= expectLine("r3 b2 first", 0, "Narrator: First ford exactly where the chart promised. Wren's ink holds true.");

    g_nextButton->do_callback(); // b3
    ok &= expectLine("r3 b3 confirm", 2, "Narrator: Bram's forge mark is on the milestone: a hammer over a gate.");

    g_nextButton->do_callback(); // b4 offer
    ok &= expectOffer("r3 b4", "Take the iron charm", "Leave the charm", "");
    g_choiceButtons[0]->do_callback(); // take charm
    ok &= expectFlag("r3 b4", "has_charm", 1);

    g_nextButton->do_callback(); // b5
    ok &= expectLine("r3 b5 first", 0, "Narrator: The hollow wind tests you once, finds iron, and moves on.");

    g_nextButton->do_callback(); // b6
    ok &= expectLine("r3 b6 first", 0, "Narrator: The chart marks an herb garden behind the second bridge.");
    ok &= expectFlag("r3 b6", "has_herbs", 1);

    g_nextButton->do_callback(); // b7 offer (has herbs)
    ok &= expectOffer("r3 b7", "Share the herbs", "Keep them", "");
    g_choiceButtons[0]->do_callback(); // share
    ok &=
        expectLine("r3 b7 share", 0, "Wren the Wayfinder: Bound tight, bitter tea, and I will walk like new by dusk.");
    ok &= expectFlag("r3 b7", "shared_herbs", 1);

    g_nextButton->do_callback(); // b8
    ok &= expectLine("r3 b8 first", 0, "Wren the Wayfinder: Dusk, as promised: walking like new, ankle quiet.");

    g_nextButton->do_callback(); // b9 offer
    ok &= expectOffer("r3 b9", "Pay Mara's price", "Refuse the price", "");
    g_choiceButtons[0]->do_callback(); // pay
    ok &= expectFlag("r3 b9", "paid_price", 1);

    g_nextButton->do_callback(); // b10
    ok &= expectLine("r3 b10 first", 0, "Narrator: The shadow parts like a curtain. Beyond it the road runs straight.");

    g_nextButton->do_callback(); // b11
    ok &= expectLine("r3 b11 first", 0, "Narrator: Bram's bridge sways over white water, but the charm steadies you.");

    g_nextButton->do_callback(); // b12
    ok &= expectLine("r3 b12 omen", 0, "Narrator: When the hollow wind stops, the road is listening. Walk softly.");

    g_nextButton->do_callback(); // b13 offer
    ok &= expectOffer("r3 b13", "Face the Hollow", "Turn back", "");
    g_choiceButtons[0]->do_callback(); // face
    ok &= expectFlag("r3 b13", "faced", 1);

    g_nextButton->do_callback(); // b14
    ok &= expectLine("r3 b14 first", 0,
                     "Narrator: The stillness presses in, wearing familiar faces, offering old roads.");

    g_nextButton->do_callback(); // b15 ending 2A
    ok &= expectLines("r3 ending", 4);
    ok &= expectLine("r3 ending first", 0, "Narrator: Ending 2A: The Road Remembers");
    ok &= expectLine("r3 ending last", 3, "Narrator: Every cairn you raised still stands. The road remembers.");
    ok &= expectFlag("r3 ending", "ending2a", 1);

    // Run 4: chapter 2 reset, chartless path -> ending 2B, then reload.
    std::cout << "--- run 4: ch2 chartless path + reload ---\n";
    g_ch2Button->do_callback();
    ok &= expectFlag("r4 fresh", "has_map", 0);

    g_nextButton->do_callback();       // b1 offer
    g_choiceButtons[1]->do_callback(); // refuse map
    ok &= expectLine("r4 b1 refuse", 0, "Wren the Wayfinder: Proud feet make their own maps. Usually wrong ones.");

    g_nextButton->do_callback(); // b2
    ok &= expectLine("r4 b2 first", 0, "Narrator: No chart, no cairns. The first ford finds you instead.");

    g_nextButton->do_callback();       // b3
    g_nextButton->do_callback();       // b4 offer
    g_choiceButtons[1]->do_callback(); // leave charm
    ok &= expectFlag("r4 b4", "left_charm", 1);

    g_nextButton->do_callback(); // b5
    ok &= expectLine("r4 b5 first", 0, "Narrator: The hollow wind circles twice, curious, tasting your edges.");

    g_nextButton->do_callback(); // b6
    g_nextButton->do_callback(); // b7 herb-less branch
    ok &= expectLines("r4 b7", 3);
    ok &= expectLine("r4 b7 first", 0, "Wren the Wayfinder: You have nothing to share, and that is its own answer.");
    ok &= expectNoOffers("r4 b7");

    g_nextButton->do_callback();       // b8
    g_nextButton->do_callback();       // b9 offer
    g_choiceButtons[1]->do_callback(); // refuse price
    ok &= expectFlag("r4 b9", "refused_price", 1);

    g_nextButton->do_callback(); // b10
    ok &= expectLine("r4 b10 first", 0, "Narrator: The road loops: same oak, same milestone, same cold spring.");

    g_nextButton->do_callback();       // b11
    g_nextButton->do_callback();       // b12
    g_nextButton->do_callback();       // b13 offer
    g_choiceButtons[1]->do_callback(); // turn back
    ok &= expectFlag("r4 b13", "turned", 1);

    g_nextButton->do_callback(); // b14
    g_nextButton->do_callback(); // b15 ending 2B
    ok &= expectLines("r4 ending", 4);
    ok &= expectLine("r4 ending first", 0, "Narrator: Ending 2B: The Hollow Keeps Its Own");
    ok &= expectLine("r4 ending last", 3, "Wren the Wayfinder: Walk softly, traveler. It knows your name now.");
    ok &= expectFlag("r4 ending", "ending2b", 1);

    // Reload keeps beat and flags, recompiles from disk (hot-reload).
    const int loadsBefore = g_loadCount;
    g_reloadButton->do_callback();
    if (g_loadCount != loadsBefore + 1)
    {
        std::cout << "MISMATCH: reload did not recompile (loads " << g_loadCount << " want " << (loadsBefore + 1)
                  << ")\n";
        ok = false;
    }
    else
    {
        std::cout << "reload: recompiled, " << g_loadCount << " loads\n";
    }
    ok &= expectLine("r4 after reload", 0, "Narrator: Ending 2B: The Hollow Keeps Its Own");
    ok &= expectFlag("r4 flags kept", "ending2b", 1);

    std::cout << "vm loads: " << g_loadCount << " beat calls: " << g_callCount << '\n';
    if (g_loadCount != 5)
    {
        std::cout << "MISMATCH: expected 5 vm loads\n";
        ok = false;
    }
    std::cout << "content: 2 chapters x " << kBeatsPerChapter << " beats = " << (2 * kBeatsPerChapter) << " beats\n";
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
        vm->addFunction("beat", kern::createExternFunction(&beat_value));
        vm->addFunction("get_flag", kern::createExternFunction(&get_flag_value));
        vm->addFunction("last_choice", kern::createExternFunction(&last_choice_value));
        vm->addFunction("hero", kern::createExternFunction(&hero_value));
        vm->addFunction("say", kern::createExternFunction(&say_text));
        vm->addFunction("offer", kern::createExternFunction(&offer_text));
        vm->addFunction("set_scene", kern::createExternFunction(&set_scene_text));
        vm->addFunction("set_flag", kern::createExternFunction(&set_flag_value));
        g_vm = vm.get();
        int candidate = -1;
        if (!resolveChaptersDir(g_chaptersDir, candidate))
        {
            return 1;
        }
        g_searchPaths.push_back(g_chaptersDir);
        std::cout << "chapters dir: " << g_chaptersDir << " (candidate #" << candidate << ")\n";
        if (!loadChapter(kChapter1))
        {
            return 1;
        }

        Fl_Window window(640, 560, kWindowTitle);
        g_window = &window;
        Fl_Box scene(20, 10, 600, 28, "");
        g_sceneBox = &scene;
        ActorChip chip(20, 46, 36, 36);
        g_chip = &chip;
        Fl_Box actor(62, 46, 240, 36, "");
        g_actorBox = &actor;
        Fl_Browser log(20, 90, 600, 280);
        g_log = &log;
        Fl_Button choice0(20, 378, 600, 30, "");
        Fl_Button choice1(20, 412, 600, 30, "");
        Fl_Button choice2(20, 446, 600, 30, "");
        choice0.callback(onChoice, reinterpret_cast<void *>(static_cast<std::intptr_t>(0)));
        choice1.callback(onChoice, reinterpret_cast<void *>(static_cast<std::intptr_t>(1)));
        choice2.callback(onChoice, reinterpret_cast<void *>(static_cast<std::intptr_t>(2)));
        g_choiceButtons[0] = &choice0;
        g_choiceButtons[1] = &choice1;
        g_choiceButtons[2] = &choice2;
        Fl_Button ch1Button(20, 484, 100, 30, "Ch1");
        ch1Button.callback(onChapter, const_cast<Chapter *>(&kChapter1));
        g_ch1Button = &ch1Button;
        Fl_Button ch2Button(126, 484, 100, 30, "Ch2");
        ch2Button.callback(onChapter, const_cast<Chapter *>(&kChapter2));
        g_ch2Button = &ch2Button;
        Fl_Button reloadButton(232, 484, 120, 30, "Reload");
        reloadButton.callback(onReload);
        g_reloadButton = &reloadButton;
        Fl_Button nextButton(520, 484, 100, 30, "Next");
        nextButton.callback(onNext);
        g_nextButton = &nextButton;
        Fl_Box status(20, 520, 600, 24, "");
        g_status = &status;
        window.end();

        g_beat = 0;
        g_lastChoice = 0;
        if (!callBeat())
        {
            return 1;
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
    catch (const kern::RuntimeError &error)
    {
        std::cerr << "runtime error: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
