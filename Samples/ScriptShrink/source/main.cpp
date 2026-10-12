#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Browser.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Widget.H>
#include <FL/Fl_Window.H>
#include <FL/fl_draw.H>

#include <kern/Kern.h>

namespace
{

constexpr const char *kWindowTitle = "KernScript Shrink";
constexpr const char *kFreudName = "Freud";
constexpr const char *kCoachName = "Coach";
constexpr const char *kFreudFile = "freud.ks";
constexpr const char *kCoachFile = "coach.ks";
constexpr int kMoodMin = -10;
constexpr int kMoodMax = 10;

// Verify script: exact replies must match scripts/*.ks byte for byte.
constexpr const char *kFreudMother = "Tell me more about your mother. What comes to mind first?";
constexpr const char *kFreudNeutral = "I see. Please go on.";
constexpr const char *kFreudSad = "Sadness is heavy. How long has this sadness been with you?";
constexpr const char *kFreudDark = "This is a heavy place. Stay with that feeling and tell me what weighs most.";
constexpr const char *kCoachMother = "Noted: mother matters to you. What is one action that would help there?";
constexpr const char *kCoachCrisis =
    "I am worried about you. Please reach out to a crisis helpline or someone you trust right now.";
constexpr const char *kCoachDark = "Tough patch acknowledged. Name one tiny thing still in your control.";

struct ScaleStats
{
    int sourceLines = 0;
    int stringEntries = 0;
    long long compileMs = 0;
    int asmLines = 0;
    int respondInstructions = 0;
};

struct Personality
{
    const char *name;
    const char *file;
};

const Personality kFreud = {kFreudName, kFreudFile};
const Personality kCoach = {kCoachName, kCoachFile};

std::string g_argv0;
std::string g_currentInput;
std::string g_lowerInput;
std::string g_reply;
bool g_emitFired = false;
int g_mood = 0;
int g_loadCount = 0;
int g_callCount = 0;
int g_hasWordCalls = 0;
long long g_lastLatencyUs = 0;
std::string g_personality = kFreudName;
std::string g_resolvedPath;
ScaleStats g_stats;
kern::IVirtualMachine *g_vm = nullptr;

Fl_Window *g_window = nullptr;
Fl_Browser *g_log = nullptr;
Fl_Input *g_input = nullptr;
Fl_Button *g_sendButton = nullptr;
Fl_Button *g_freudButton = nullptr;
Fl_Button *g_coachButton = nullptr;
Fl_Button *g_reloadButton = nullptr;
Fl_Box *g_status = nullptr;

std::string toLower(const std::string &text)
{
    std::string out = text;
    for (char &c : out)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

// Host-side word test: the script never sees the raw input, it only asks
// about one lowercase word at a time. Match is case-insensitive on whole
// words; ASCII alnum plus underscore count as word characters.
int has_word_value(std::string word)
{
    ++g_hasWordCalls;
    const std::string want = toLower(word);
    if (want.empty())
    {
        return 0;
    }
    const std::string &text = g_lowerInput;
    std::size_t pos = 0;
    while ((pos = text.find(want, pos)) != std::string::npos)
    {
        const bool headOk =
            pos == 0 || (!std::isalnum(static_cast<unsigned char>(text[pos - 1])) && text[pos - 1] != '_');
        const std::size_t end = pos + want.size();
        const bool tailOk =
            end >= text.size() || (!std::isalnum(static_cast<unsigned char>(text[end])) && text[end] != '_');
        if (headOk && tailOk)
        {
            return 1;
        }
        ++pos;
    }
    return 0;
}

int mood_value() { return g_mood; }

void emit_text(std::string text)
{
    g_reply = text;
    g_emitFired = true;
}

void emit_mood_value(int delta) { g_mood = std::max(kMoodMin, std::min(kMoodMax, g_mood + delta)); }

class MoodFace : public Fl_Widget
{
public:
    MoodFace(int x, int y, int w, int h) : Fl_Widget(x, y, w, h) {}

    void draw() override
    {
        fl_push_clip(x(), y(), w(), h());
        fl_color(FL_WHITE);
        fl_rectf(x(), y(), w(), h());
        const int cx = x() + w() / 2;
        const int cy = y() + h() / 2;
        const int r = std::min(w(), h()) / 2 - 4;
        fl_color(FL_YELLOW);
        fl_pie(cx - r, cy - r, r * 2, r * 2, 0.0, 360.0);
        fl_color(FL_BLACK);
        fl_circle(cx, cy, r);
        fl_rectf(cx - r / 2 - 2, cy - r / 3 - 2, 4, 4);
        fl_rectf(cx + r / 2 - 2, cy - r / 3 - 2, 4, 4);
        if (g_mood > 2)
        {
            fl_arc(cx - r / 2, cy - r / 4, r, r / 2 + r / 4, 200.0, 340.0);
        }
        else if (g_mood < -2)
        {
            fl_arc(cx - r / 2, cy + r / 4 - r / 4, r, r / 2 + r / 4, 20.0, 160.0);
        }
        else
        {
            fl_line(cx - r / 2, cy + r / 2, cx + r / 2, cy + r / 2);
        }
        fl_pop_clip();
    }
};

MoodFace *g_face = nullptr;

void refreshUi();

std::string trimLeft(const std::string &line)
{
    std::size_t i = 0;
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t' || line[i] == '\r'))
    {
        ++i;
    }
    return line.substr(i);
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

// Rough instruction count for func respond, measured on emitted asm:
// lines inside the func block that are not directives, declarations or
// labels. Reported as measured, not as a VM step count.
int countRespondInstructions(const std::string &asmCode)
{
    std::istringstream lines(asmCode);
    std::string line;
    bool inRespond = false;
    int count = 0;
    while (std::getline(lines, line))
    {
        const std::string t = trimLeft(line);
        if (!inRespond)
        {
            if (t == "func respond" || t.rfind("func respond ", 0) == 0)
            {
                inRespond = true;
            }
            continue;
        }
        if (t.rfind("func ", 0) == 0 || t.rfind("extern ", 0) == 0)
        {
            break;
        }
        if (t.empty() || t == "{" || t == "}" || t.rfind("param ", 0) == 0 || t.rfind("var ", 0) == 0 ||
            t.rfind("label ", 0) == 0)
        {
            continue;
        }
        ++count;
    }
    return count;
}

// Scripts live as separate files under scripts/. Resolve robustly so
// --verify works from the build tree: exe-dir/scripts wins, then
// CWD/scripts, then CWD/Samples/ScriptShrink/scripts (repo-root runs).
// The winning candidate is reported; a miss is a loud error, never silent.
bool resolveScriptPath(const char *file, std::string &out, int &candidate)
{
    std::vector<std::string> tries;
    try
    {
        const std::filesystem::path exe(g_argv0);
        if (exe.has_parent_path())
        {
            tries.push_back((exe.parent_path() / "scripts" / file).string());
        }
        else
        {
            tries.push_back((std::filesystem::current_path() / "scripts" / file).string());
        }
    }
    catch (const std::filesystem::filesystem_error &)
    {
        tries.push_back((std::filesystem::current_path() / "scripts" / file).string());
    }
    tries.push_back((std::filesystem::current_path() / "scripts" / file).string());
    tries.push_back((std::filesystem::current_path() / "Samples" / "ScriptShrink" / "scripts" / file).string());
    for (std::size_t i = 0; i < tries.size(); ++i)
    {
        std::error_code code;
        if (std::filesystem::is_regular_file(tries[i], code))
        {
            out = tries[i];
            candidate = static_cast<int>(i);
            return true;
        }
    }
    std::cerr << "script file missing: " << file << " (tried " << tries.size() << " candidates:";
    for (const std::string &t : tries)
    {
        std::cerr << " [" << t << "]";
    }
    std::cerr << ")\n";
    return false;
}

bool loadPersonality(const Personality &preset)
{
    std::string path;
    int candidate = -1;
    if (!resolveScriptPath(preset.file, path, candidate))
    {
        return false;
    }
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        std::cerr << "cannot open script file: " << path << '\n';
        return false;
    }
    std::stringstream source;
    source << file.rdbuf();
    const std::string sourceText = source.str();

    ScaleStats stats;
    stats.sourceLines = countLines(sourceText);
    stats.stringEntries = countDistinctStrings(sourceText);

    std::stringstream asmCode;
    {
        const auto start = std::chrono::steady_clock::now();
        std::istringstream input(sourceText);
        std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler());
        if (!compiler->compile(input, asmCode, preset.file))
        {
            std::cerr << "compile failed for " << preset.name << " (" << path << ")\n";
            return false;
        }
        const auto end = std::chrono::steady_clock::now();
        stats.compileMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    }
    const std::string asmText = asmCode.str();
    stats.asmLines = countLines(asmText);
    stats.respondInstructions = countRespondInstructions(asmText);

    std::stringstream byteCode;
    {
        std::istringstream input(asmText);
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
    g_personality = preset.name;
    g_resolvedPath = path;
    g_stats = stats;
    std::cout << "personality " << preset.name << " loaded from " << path << " (candidate #" << candidate << ", load #"
              << g_loadCount << "): " << stats.sourceLines << " lines, " << stats.compileMs << " ms, " << stats.asmLines
              << " asm lines, " << stats.stringEntries << " strings, " << stats.respondInstructions
              << " respond instr\n";
    return true;
}

const Personality *findPersonality(const std::string &name)
{
    if (name == kCoachName)
    {
        return &kCoach;
    }
    return &kFreud;
}

bool callRespond()
{
    g_reply.clear();
    g_emitFired = false;
    g_hasWordCalls = 0;
    const auto start = std::chrono::steady_clock::now();
    try
    {
        if (!g_vm->callFunction("respond"))
        {
            std::cerr << "function respond not found\n";
            return false;
        }
    }
    catch (const kern::RuntimeError &error)
    {
        std::cerr << "runtime error: " << error.what() << '\n';
        return false;
    }
    const auto end = std::chrono::steady_clock::now();
    g_lastLatencyUs = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    ++g_callCount;
    if (!g_emitFired)
    {
        std::cerr << "script produced no reply (missing emit)\n";
        return false;
    }
    return true;
}

bool sendMessage(const std::string &text, std::string &replyOut)
{
    g_currentInput = text;
    g_lowerInput = toLower(text);
    if (!callRespond())
    {
        return false;
    }
    replyOut = g_reply;
    g_log->add(("You: " + text).c_str());
    g_log->add((g_personality + std::string(": ") + g_reply).c_str());
    refreshUi();
    std::cout << "msg -> reply " << g_reply.size() << " chars in " << g_lastLatencyUs << " us (" << g_hasWordCalls
              << " has_word calls, mood " << g_mood << ")\n";
    return true;
}

void refreshUi()
{
    const std::string text =
        g_personality + std::string(" | mood ") + std::to_string(g_mood) + " | load #" + std::to_string(g_loadCount);
    g_status->copy_label(text.c_str());
    g_face->redraw();
}

void onSend(Fl_Widget * /*widget*/, void * /*data*/)
{
    const char *value = g_input->value();
    const std::string text = value != nullptr ? value : "";
    if (text.empty())
    {
        return;
    }
    std::string reply;
    if (!sendMessage(text, reply))
    {
        return;
    }
    g_input->value("");
    std::cout << "sent '" << text << "' -> '" << reply << "'\n";
}

void onPersonality(Fl_Widget * /*widget*/, void *data)
{
    const Personality *preset = static_cast<const Personality *>(data);
    if (!loadPersonality(*preset))
    {
        return;
    }
    refreshUi();
    std::cout << "switched to " << preset->name << '\n';
}

void onReload(Fl_Widget * /*widget*/, void * /*data*/)
{
    const Personality *preset = findPersonality(g_personality);
    if (!loadPersonality(*preset))
    {
        return;
    }
    refreshUi();
    std::cout << "reloaded " << preset->name << " live\n";
}

bool expectReply(const char *label, const std::string &actual, const char *want)
{
    if (actual == want)
    {
        std::cout << label << ": reply match (" << actual.size() << " chars)\n";
        return true;
    }
    std::cout << "MISMATCH " << label << ":\n  got  '" << actual << "'\n  want '" << want << "'\n";
    return false;
}

bool expectMood(const char *label, int want)
{
    if (g_mood == want)
    {
        std::cout << label << ": mood " << g_mood << " as expected\n";
        return true;
    }
    std::cout << "MISMATCH " << label << ": mood " << g_mood << " want " << want << '\n';
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
    std::cout << "startup personality: " << g_personality << " from " << g_resolvedPath << '\n';
    if (g_personality != kFreudName)
    {
        std::cout << "MISMATCH: startup personality is not Freud\n";
        ok = false;
    }
    std::cout << "scale freud: " << g_stats.sourceLines << " lines, " << g_stats.compileMs << " ms, "
              << g_stats.asmLines << " asm lines, " << g_stats.stringEntries << " strings, "
              << g_stats.respondInstructions << " respond instr\n";

    // Real widget callbacks from here on: the input field plus the Send
    // button run the same path as interactive chatting.
    std::string reply;
    g_input->value("I dreamed about my mother");
    g_sendButton->do_callback();
    reply = g_reply;
    std::cout << "t1 transcript: You: I dreamed about my mother / " << g_personality << ": " << reply << '\n';
    ok &= expectReply("t1 keyword hit", reply, kFreudMother);
    ok &= expectMood("t1 mood", 0);

    g_input->value("I bought a zebra");
    g_sendButton->do_callback();
    reply = g_reply;
    std::cout << "t2 transcript: You: I bought a zebra / " << g_personality << ": " << reply << '\n';
    ok &= expectReply("t2 deflection", reply, kFreudNeutral);
    ok &= expectMood("t2 mood", 0);

    g_input->value("I feel so sad today");
    g_sendButton->do_callback();
    reply = g_reply;
    std::cout << "t3 transcript: You: I feel so sad today / " << g_personality << ": " << reply << '\n';
    ok &= expectReply("t3 mood shift", reply, kFreudSad);
    ok &= expectMood("t3 mood", -2);

    // Personality switch mid-chat: same input, Coach tone expected.
    g_coachButton->do_callback();
    std::cout << "scale coach: " << g_stats.sourceLines << " lines, " << g_stats.compileMs << " ms, "
              << g_stats.asmLines << " asm lines, " << g_stats.stringEntries << " strings, "
              << g_stats.respondInstructions << " respond instr\n";
    if (g_personality != kCoachName)
    {
        std::cout << "MISMATCH: switch did not select Coach\n";
        ok = false;
    }
    g_input->value("I dreamed about my mother");
    g_sendButton->do_callback();
    reply = g_reply;
    std::cout << "t4 transcript: You: I dreamed about my mother / " << g_personality << ": " << reply << '\n';
    ok &= expectReply("t4 switch reply", reply, kCoachMother);
    if (reply == kFreudMother)
    {
        std::cout << "MISMATCH t4: Coach reply identical to Freud reply\n";
        ok = false;
    }
    ok &= expectMood("t4 mood", -2);

    g_input->value("I want to die");
    g_sendButton->do_callback();
    reply = g_reply;
    std::cout << "t5 transcript: You: I want to die / " << g_personality << ": " << reply << '\n';
    ok &= expectReply("t5 crisis", reply, kCoachCrisis);
    ok &= expectMood("t5 mood", -5);

    // Mood now -5 (< -2): an unmatched line must take the dark fallback.
    g_input->value("I bought a zebra");
    g_sendButton->do_callback();
    reply = g_reply;
    std::cout << "t6 transcript: You: I bought a zebra / " << g_personality << ": " << reply << '\n';
    ok &= expectReply("t6 dark fallback", reply, kCoachDark);
    ok &= expectMood("t6 mood", -5);

    // Reload recompiles the current personality without changing behavior.
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
    g_input->value("I bought a zebra");
    g_sendButton->do_callback();
    ok &= expectReply("t7 after reload", g_reply, kCoachDark);

    // Back to Freud at low mood: the deflection tone follows the mood too.
    g_freudButton->do_callback();
    g_input->value("I bought a zebra");
    g_sendButton->do_callback();
    ok &= expectReply("t8 freud dark", g_reply, kFreudDark);

    std::cout << "vm loads: " << g_loadCount << " respond calls: " << g_callCount << '\n';
    if (g_loadCount != 4)
    {
        std::cout << "MISMATCH: expected 4 vm loads\n";
        ok = false;
    }
    if (g_callCount != 8)
    {
        std::cout << "MISMATCH: expected 8 respond calls\n";
        ok = false;
    }
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
        vm->addFunction("has_word", kern::createExternFunction(&has_word_value));
        vm->addFunction("mood", kern::createExternFunction(&mood_value));
        vm->addFunction("emit", kern::createExternFunction(&emit_text));
        vm->addFunction("emit_mood", kern::createExternFunction(&emit_mood_value));
        g_vm = vm.get();
        if (!loadPersonality(kFreud))
        {
            return 1;
        }

        Fl_Window window(560, 470, kWindowTitle);
        g_window = &window;
        Fl_Browser log(20, 20, 400, 300);
        g_log = &log;
        MoodFace face(430, 20, 110, 110);
        g_face = &face;
        Fl_Input input(20, 330, 400, 30, "");
        input.when(FL_WHEN_ENTER_KEY_ALWAYS);
        input.callback(onSend);
        g_input = &input;
        Fl_Button sendButton(430, 330, 110, 30, "Send");
        sendButton.callback(onSend);
        g_sendButton = &sendButton;
        Fl_Button freudButton(20, 370, 110, 30, "Freud");
        freudButton.callback(onPersonality, const_cast<Personality *>(&kFreud));
        g_freudButton = &freudButton;
        Fl_Button coachButton(140, 370, 110, 30, "Coach");
        coachButton.callback(onPersonality, const_cast<Personality *>(&kCoach));
        g_coachButton = &coachButton;
        Fl_Button reloadButton(260, 370, 160, 30, "Reload");
        reloadButton.callback(onReload);
        g_reloadButton = &reloadButton;
        Fl_Box status(20, 410, 520, 24, "Freud | mood 0 | load #1");
        g_status = &status;
        window.end();
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
