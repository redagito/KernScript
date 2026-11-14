#pragma once

#include <functional>
#include <iostream>
#include <set>

#include "Story.h"

namespace storybook
{
inline void require(bool condition, const std::string &message)
{
    if (!condition)
        throw std::runtime_error(message);
}
template <class F> void rejected(F &&operation, const std::string &message)
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
    require(failed, message);
}

inline int verify(const fs::path &source, const fs::path &testRoot)
{
    const auto directory = samples::testDirectory(testRoot);
    const auto scripts = directory / "chapters";
    fs::create_directory(scripts);
    for (const auto *name : {"story.ks", "chapter1.ks", "chapter2.ks", "cast.ks", "world.ks"})
        fs::copy_file(source.parent_path() / name, scripts / name);
    const auto root = scripts / "story.ks";
    const auto original = samples::readText(root, 1024 * 1024);
    const auto writeRoot = [&](const std::string &text)
    { samples::writeAtomically(root, [&](std::ostream &out) { out << text; }); };
    Engine engine;
    const std::string hero = "Ash \"the traveler\" \\ home";
    engine.start(root, hero);
    require(engine.current().node == "start" && engine.current().choices.size() == 3 && engine.dirty(),
            "The opening scene is incomplete");
    const auto opening = engine.session();
    rejected([&] { engine.advance("@next"); }, "A required choice could be skipped");
    rejected([&] { engine.advance("missing"); }, "An unavailable choice was accepted");
    require(engine.session() == opening, "An invalid choice changed story state");
    engine.advance("mage");
    require(engine.current().flags.at("rune_known") == 1, "Entry effects were not applied");
    engine.advance("@next");
    engine.advance("lantern");
    engine.advance("@next");
    engine.advance("trust");
    require(engine.current().flags.at("has_map") == 1, "An earlier choice did not affect the shortcut");
    const auto checkpoint = engine.session();
    const auto save = directory / "journey.storybook";
    engine.save(save);
    require(!engine.dirty(), "Saving did not clear the dirty state");
    engine.advance("@next");
    engine.advance("stand");
    engine.advance("@next");
    require(engine.current().flags.at("gate_saved") == 1 && engine.current().next == "road.garden",
            "State was lost at the chapter boundary");
    engine.open(save);
    require(engine.session() == checkpoint && !engine.dirty(), "Save/open did not reconstruct the exact playthrough");
    engine.save(save);
    require(!engine.dirty(), "An existing save could not be replaced");
    std::string crlf;
    for (char c : Engine::serialize(checkpoint))
        crlf += c == '\n' ? std::string("\r\n") : std::string(1, c);
    samples::writeAtomically(save, [&](std::ostream &out) { out << crlf; });
    engine.open(save);
    require(engine.session() == checkpoint, "A Windows-edited save did not open");
    engine.save(save);
    engine.back();
    require(engine.current().node == "gate.scout" && !engine.current().flags.contains("has_map") && engine.dirty(),
            "Back did not restore the state before entry effects");
    engine.advance("trust");
    require(engine.session() == checkpoint && !engine.dirty(), "Replaying the saved choice changed the session");

    const auto bad = directory / "invalid.storybook";
    for (const auto &content :
         {std::string("STORYBOOK 9\n"), std::string("STORYBOOK 1\nhero \"Ash\"\nstory \"other-story\"\n"),
          std::string("STORYBOOK 1\nhero \"Ash\"\nstory \"ember-road-v1\"\naction start mage road.bridge\n"),
          std::string("STORYBOOK 1\nhero \"Ash\"\nstory \"ember-road-v1\"\naction start mage gate.runes extra\n"),
          std::string("STORYBOOK 1\nhero \"unterminated\nstory \"ember-road-v1\"\n")})
    {
        samples::writeAtomically(bad, [&](std::ostream &out) { out << content; });
        rejected([&] { engine.open(bad); }, "An invalid saved game was accepted");
        require(engine.session() == checkpoint && engine.file() == save && !engine.dirty(),
                "A failed open changed the live game");
    }
    const auto transcript = directory / "journey.txt";
    engine.exportTranscript(transcript);
    const auto exported = samples::readText(transcript, 1024 * 1024);
    require(exported.contains(hero) && exported.contains("> Help Mara") && exported.contains("Wren never lies"),
            "The transcript lost the player, selected choices or scene dialogue");
    rejected([&] { engine.exportTranscript(save); }, "Transcript export overwrote the saved game");
    rejected([&] { engine.save(root); }, "Saving overwrote the story script");
    rejected([&] { engine.exportTranscript(root); }, "Transcript export overwrote the story script");
    require(samples::readText(root, 1024 * 1024) == original, "The active story script was damaged");
    const auto castPath = scripts / "cast.ks";
    const auto castSource = samples::readText(castPath, 1024 * 1024);
    rejected([&] { engine.save(castPath); }, "Saving overwrote a shared story module");
    rejected([&] { engine.exportTranscript(castPath); }, "Export overwrote a shared story module");
    require(samples::readText(castPath, 1024 * 1024) == castSource, "The shared story module was damaged");
    require(samples::readText(save, 256 * 1024) == Engine::serialize(checkpoint), "The saved game was damaged");

    for (const auto &broken :
         {std::string("func render() -> void { var broken = ; }\n"), std::string("func another() -> void {}\n"),
          std::string("extern story_id(id) -> void; func render() -> void { story_id(\"ember-road-v1\"); }\n"),
          std::string("func render() -> void { var zero = 0; var x = 10 / zero; }\n")})
    {
        writeRoot(broken);
        rejected([&] { engine.reload(); }, "A broken story replaced the current program");
        require(engine.session() == checkpoint && active == nullptr, "A failed reload changed state or leaked context");
    }
    writeRoot(original);
    engine.reload();
    require(engine.session() == checkpoint, "Reload reapplied scene effects or duplicated the backlog");
    engine.advance("@next");
    require(engine.current().node == "gate.heart", "The old VM stopped working after rejected reloads");

    // Visit every reachable scene/state combination. Stop repeated loop states,
    // while still checking every available exit and all distinct endings.
    engine.start(root, "Ash");
    std::set<std::string> visited, endings, nodes;
    std::function<void()> walk = [&]
    {
        const auto frame = engine.current();
        std::ostringstream state;
        state << frame.node;
        for (const auto &[name, value] : frame.flags)
            state << ' ' << name << '=' << value;
        if (!visited.insert(state.str()).second)
            return;
        require(visited.size() < 10000, "The bundled story unexpectedly expands without a bound");
        nodes.insert(frame.node);
        if (!frame.ending.empty())
        {
            endings.insert(frame.ending);
            return;
        }
        std::vector<std::string> actions;
        if (!frame.next.empty())
            actions.push_back("@next");
        for (const auto &choice : frame.choices)
            actions.push_back(choice.id);
        const auto before = engine.session();
        for (const auto &action : actions)
        {
            engine.advance(action);
            walk();
            engine.back();
            require(engine.session() == before, "Exploring a branch did not restore its origin");
        }
    };
    walk();
    require(nodes.size() == 26 &&
                endings == std::set<std::string>{"The road remembers its friends", "The road remembers your name",
                                                 "A traveler in the Hollow", "Keeper of Ember Gate",
                                                 "A promise to rebuild"},
            "Not all bundled scenes or endings were exercised");

    // A multi-scene failure must leave both the previous output and its flags intact.
    const std::string protocol = "extern node() -> value; extern story_id(id) -> void; extern title(t) -> void; "
                                 "extern say(a,t) -> void; extern next(n) -> void; extern finish(e) -> void; "
                                 "extern set_flag(n,v) -> void; extern get_flag(n) -> value; ";
    const std::string intro = "story_id(\"fixture\"); title(\"Fixture\"); say(\"Narrator\",\"Before\"); ";
    writeRoot(protocol + "func render() -> void { " + intro +
              "if (node() == \"start\") { next(\"broken\"); return; } "
              "set_flag(\"changed\", 1); say(\"Narrator\",\"Partial\"); var zero = 0; var x = 1 / zero; }");
    engine.start(root, "Ash");
    const auto beforeFailure = engine.session();
    rejected([&] { engine.advance("@next"); }, "A failing scene transition was accepted");
    require(engine.session() == beforeFailure && active == nullptr, "A failing scene leaked partial changes");
    writeRoot(protocol + "func render() -> void { " + intro + "next(\"other\"); finish(\"Invalid mixed exits\"); }");
    rejected([&] { engine.reload(); }, "Mutually incompatible exits were accepted");
    require(engine.session() == beforeFailure, "A protocol error changed the game");

    // A realistic authoring workload: 120 transitions, 2420 dialogue lines,
    // repeated flag updates and replay of the full saved journal.
    writeRoot(protocol + "func render() -> void { " + intro +
              "var count = get_flag(\"count\"); set_flag(\"count\", count + 1); "
              "var i = 0; while (i < 19) { say(\"Archivist\",\"An entry in the travel journal.\"); i = i + 1; } "
              "if (count == 120) { finish(\"Archive complete\"); } else { next(\"archive\"); } }");
    engine.start(root, hero);
    for (int i = 0; i < 120; ++i)
        engine.advance("@next");
    const auto archive = engine.session();
    require(archive.frames.size() == 121 && archive.frames.back().flags.at("count") == 121 &&
                archive.frames.back().ending == "Archive complete",
            "The larger story failed to finish");
    const auto archiveFile = directory / "archive.storybook";
    engine.save(archiveFile);
    engine.restart("Another traveler");
    engine.open(archiveFile);
    require(engine.session() == archive && !engine.dirty(), "The larger playthrough lost state on restore");
    engine.reload();
    require(engine.session() == archive, "Reload changed non-idempotent entry effects");
    std::cout << "Storybook verify: PASS (" << nodes.size() << " scenes, " << endings.size() << " endings, "
              << visited.size()
              << " reachable scene states; saved journals, transactional reloads, 121-scene archive)\n";
    return 0;
}
} // namespace storybook
