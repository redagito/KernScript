#pragma once

#include "song.hpp"

#include <kern/Kern.h>

#include <array>
#include <filesystem>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace cadence
{
struct GenerationContext
{
    Song *song = nullptr;
    int step = -1;
    int expectedTrack = -1;
    int emissionCount = 0;
    std::string error;
};

inline GenerationContext *&activeContext()
{
    static GenerationContext *context = nullptr;
    return context;
}

class ContextScope
{
public:
    explicit ContextScope(GenerationContext &context) : m_previous(activeContext()) { activeContext() = &context; }
    ~ContextScope() { activeContext() = m_previous; }
    ContextScope(const ContextScope &) = delete;
    ContextScope &operator=(const ContextScope &) = delete;

private:
    GenerationContext *m_previous;
};

inline int curStepValue()
{
    const auto *context = activeContext();
    return context ? context->step : -1;
}
inline int rootValue()
{
    const auto *context = activeContext();
    return context && context->song ? 60 + context->song->root : 60;
}
inline int scaleValue()
{
    const auto *context = activeContext();
    return context && context->song ? context->song->scale : 0;
}
inline void emitNote(int track, int note)
{
    auto *context = activeContext();
    if (!context || !context->song)
        return;
    ++context->emissionCount;
    if (context->emissionCount != 1)
    {
        context->error = "Music script emitted more than one note for a step";
        return;
    }
    if (track != context->expectedTrack)
    {
        context->error = "Music script emitted to the wrong track";
        return;
    }
    if (context->step < 0 || context->step >= context->song->steps())
    {
        context->error = "Music script emitted outside the active composition";
        return;
    }
    if (note != Rest && (note < NoteLow || note > NoteHigh))
    {
        context->error = "Music script emitted a note outside MIDI range";
        return;
    }
    context->song->tracks[track].notes[context->step] = note;
}

inline std::string rootSource()
{
    return "import bass;\nimport drums;\nimport lead;\n"
           "extern cur_step() -> value;\nextern emit(track, note) -> void;\n"
           "func bass_note() -> void { emit(0, bass::pattern(cur_step())); }\n"
           "func drums_note() -> void { emit(1, drums::pattern(cur_step())); }\n"
           "func lead_note() -> void { emit(2, lead::pattern(cur_step())); }\n";
}

inline bool compileCandidate(const std::filesystem::path &directory, kern::ILogger *logger,
                             std::unique_ptr<kern::IVirtualMachine> &candidate, std::string &error,
                             const std::string &sourceText = rootSource())
{
    try
    {
        std::stringstream assembly;
        std::istringstream source(sourceText);
        std::unique_ptr<kern::ICompiler> compiler(kern::createCompiler(logger));
        const std::vector<std::string> imports{directory.string()};
        if (!compiler->compile(source, assembly, "<cadence-root>", imports))
        {
            error = "Music script compile failed";
            return false;
        }
        std::stringstream bytecode;
        std::istringstream assembled(assembly.str());
        std::unique_ptr<kern::IAssembler> assembler(kern::createAssembler());
        if (!assembler->assemble(assembled, bytecode))
        {
            error = "Music script assembly failed";
            return false;
        }
        candidate.reset(kern::createVirtualMachine(logger));
        candidate->addFunction("cur_step", &curStepValue);
        candidate->addFunction("root", &rootValue);
        candidate->addFunction("scale", &scaleValue);
        candidate->addFunction("emit", &emitNote);
        if (!candidate->load(bytecode))
        {
            error = "Music script load failed";
            return false;
        }
        return true;
    }
    catch (const std::exception &exception)
    {
        error = exception.what();
        return false;
    }
}

inline bool runEntry(kern::IVirtualMachine &vm, GenerationContext &context, int track, int step, std::string &error)
{
    static const char *entries[Tracks] = {"bass_note", "drums_note", "lead_note"};
    context.step = step;
    context.expectedTrack = track;
    context.emissionCount = 0;
    context.error.clear();
    try
    {
        if (!vm.callFunction(entries[track]))
        {
            error = std::string("Missing music script entry: ") + entries[track];
            return false;
        }
    }
    catch (const std::exception &exception)
    {
        error = exception.what();
        return false;
    }
    if (!context.error.empty())
    {
        error = context.error;
        return false;
    }
    if (context.emissionCount != 1)
    {
        error = "Music script must emit exactly one note or rest per step";
        return false;
    }
    return true;
}

inline bool generateTransaction(Song &song, kern::IVirtualMachine &vm, int first, int last,
                                const std::array<bool, Tracks> &selectedTracks, std::string &error)
{
    try
    {
        validateSong(song);
        if (first < 0 || first >= last || last > song.steps())
            throw std::runtime_error("Generation range is outside the active composition");
        Song candidate = song;
        GenerationContext context{&candidate};
        ContextScope scope(context);
        for (int track = 0; track < Tracks; ++track)
        {
            if (!selectedTracks[track])
                continue;
            for (int step = first; step < last; ++step)
                if (!runEntry(vm, context, track, step, error))
                    return false;
        }
        validateSong(candidate);
        song = candidate;
        return true;
    }
    catch (const std::exception &exception)
    {
        error = exception.what();
        return false;
    }
}

inline bool validateCandidate(kern::IVirtualMachine &candidate, const Song &composition, std::string &error)
{
    try
    {
        validateSong(composition);
        Song scratch = composition;
        GenerationContext context{&scratch};
        ContextScope scope(context);
        for (int track = 0; track < Tracks; ++track)
            for (int step = 0; step < composition.steps(); ++step)
                if (!runEntry(candidate, context, track, step, error))
                    return false;
        validateSong(scratch);
        return true;
    }
    catch (const std::exception &exception)
    {
        error = exception.what();
        return false;
    }
}
} // namespace cadence
