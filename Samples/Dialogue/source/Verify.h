#pragma once
#include "Conversation.h"
#include <iostream>

namespace dialogue
{
inline int verify(const fs::path &scripts, const fs::path &testRoot)
{
    const auto directory = samples::testDirectory(testRoot);
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
        require(failed, "An invalid conversation operation was accepted");
    };
    Conversation support;
    support.start(scripts / "support.ks");
    require(support.session().scenario == "shop-support-v1" && !support.dirty(), "Support did not initialize");
    support.send("DELIVERY?");
    support.send("It is late.");
    require(support.session().turns.back().rule == "delivery.late" && support.session().state.at("followup") == 1,
            "Support did not retain its selected topic");
    auto checkpoint = support.session();
    const auto file = directory / "support.dialogue";
    support.save(file);
    support.send("arrived");
    require(support.session().state.at("followup") == 0, "Support did not resolve delivery");
    support.undo();
    require(support.session() == checkpoint && !support.dirty(), "Undo did not restore the conversation state");
    support.send("returns");
    support.send("damaged");
    require(support.session().turns.back().rule == "returns.damaged" && support.session().state.at("followup") == 2,
            "The return flow did not match its stateful rule");
    support.open(file);
    require(support.session() == checkpoint && !support.dirty(), "Save replay changed the conversation");
    support.save(file);
    support.reload();
    require(support.session() == checkpoint, "Reload duplicated messages or state changes");
    Conversation npc;
    npc.start(scripts / "gatekeeper.ks");
    npc.send("enter");
    require(npc.session().turns.back().rule == "gate.closed", "The gate opened without a permit");
    npc.send("buy");
    npc.send("buy");
    require(npc.session().state.at("coins") == 1 && npc.session().turns.back().rule == "permit.owned",
            "Repeated buying spent coins twice");
    npc.send("enter");
    npc.send("bridge");
    require(npc.session().turns.back().rule == "bridge.dark", "The dark bridge was passable");
    npc.send("lantern");
    npc.send("bridge");
    require(npc.session().state.at("crossed") == 1 && npc.session().state.at("coins") == 0,
            "The NPC conversation did not complete its inventory flow");
    const auto npcSession = npc.session();
    const auto npcFile = directory / "gate.dialogue";
    npc.save(npcFile);
    npc.start(scripts / "gatekeeper.ks");
    npc.open(npcFile);
    require(npc.session() == npcSession && support.session() == checkpoint, "Conversations shared or lost their state");
    rejected([&] { support.open(npcFile); });
    require(support.session() == checkpoint, "A wrong-scenario save replaced the current conversation");
    support.send("undelivered");
    require(support.session().turns.back().rule == "unmatched", "A substring matched a whole word");
    support.undo();
    const auto transcript = directory / "conversation.txt";
    npc.exportTranscript(transcript);
    const auto text = samples::readText(transcript, 65536);
    require(text.contains("Reply [bridge.crossed]") && text.contains("You: lantern"),
            "The transcript lost rule inspection or input");
    rejected([&] { npc.exportTranscript(npcFile); });

    const auto bad = directory / "bad.dialogue";
    auto write = [](const fs::path &path, const std::string &content)
    { samples::writeAtomically(path, [&](std::ostream &out) { out << content; }); };
    for (const auto &content : {std::string("DIALOGUE 9\n"),
                                std::string("DIALOGUE 1\nscenario \"shop-support-v1\"\nmessage \"unterminated\n"),
                                std::string("DIALOGUE 1\nscenario \"shop-support-v1\"\nunknown \"ignored\"\n")})
    {
        write(bad, content);
        rejected([&] { support.open(bad); });
        require(support.session() == checkpoint && support.file() == file,
                "Invalid open changed the current conversation");
    }
    const auto helper = directory / "helper.ks";
    write(helper, "module helper;\n");
    rejected([&] { support.save(helper); });
    rejected([&] { support.exportTranscript(helper); });
    require(samples::readText(helper, 4096) == "module helper;\n", "Conversation output overwrote a script dependency");
    const auto fixture = directory / "scenario.ks";
    const std::string protocol = "extern input() -> value; extern scenario(id) -> void; extern reply(r,t) -> void; "
                                 "extern state(k) -> value; extern set_state(k,v) -> void; ";
    const std::string valid =
        protocol + "func respond() -> void { scenario(\"fixture\"); "
                   "set_state(\"visits\",state(\"visits\")+1); reply(\"echo\",\"A reply with quotes and a comma\"); }";
    write(fixture, valid);
    Conversation authored;
    authored.start(fixture);
    authored.send("quote \" and slash \\ and a tab\t");
    const auto authoredSession = authored.session();
    const auto authoredFile = directory / "authored.dialogue";
    authored.save(authoredFile);
    authored.open(authoredFile);
    require(authored.session() == authoredSession, "Escaped conversation text did not roundtrip");
    for (const auto &source :
         {std::string("func respond() -> void { var broken = ; }"), std::string("func missing() -> void {}"),
          protocol + "func respond() -> void { scenario(\"fixture\"); }",
          protocol + "func respond() -> void { scenario(\"other\"); reply(\"rule\",\"wrong scenario\"); }",
          protocol +
              "func respond() -> void { scenario(\"fixture\"); reply(\"one\",\"One\"); reply(\"two\",\"Two\"); }",
          protocol + "func respond() -> void { set_state(\"changed\",1); var zero=0; var x=1/zero; }"})
    {
        write(fixture, source);
        rejected([&] { authored.reload(); });
        require(authored.session() == authoredSession && active == nullptr,
                "Failed reload leaked state or replaced a working conversation");
    }
    authored.send("The previous VM still responds");
    require(authored.session().state.at("visits") == 3, "A rejected reload broke the previous VM");
    // A later message fails after changing scratch state. No partial turn is committed.
    write(fixture, protocol + "func respond() -> void { scenario(\"fixture\"); if (input()!=\"\") { "
                              "set_state(\"changed\",1); var zero=0; var x=1/zero; } reply(\"welcome\",\"Hello\"); }");
    authored.start(fixture);
    const auto before = authored.session();
    rejected([&] { authored.send("fail"); });
    require(authored.session() == before && active == nullptr, "A failed reply committed partial state");
    write(fixture, valid);
    authored.start(fixture);
    for (int i = 0; i < 600; ++i)
        authored.send("Message " + std::to_string(i));
    const auto larger = authored.session();
    authored.save(authoredFile);
    authored.start(fixture);
    authored.open(authoredFile);
    authored.reload();
    require(authored.session() == larger && authored.session().state.at("visits") == 601,
            "The 600-message authoring session lost state on replay");
    std::cout << "Dialogue verify: PASS (support and NPC flows, isolated state, saved transcripts, rollback, "
                 "600-message replay)\n";
    return 0;
}
} // namespace dialogue
