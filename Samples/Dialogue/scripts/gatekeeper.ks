import common;
extern input() -> value;
extern has_word(word) -> value;
extern state(name) -> value;
extern set_state(name, value) -> void;
extern scenario(id) -> void;
extern reply(rule, text) -> void;

func respond() -> void {
  scenario("gatekeeper-v1");
  if (input() == "") {
    set_state("coins", 3);
    reply("welcome", "Ember Gate is closed. The keeper offers a permit for two coins. You have three. Say buy, enter, lantern, status or help.");
    return;
  }
  if (has_word("help") == 1) {
    reply("help", "Buy a permit, enter the gate, or buy a lantern for one coin. Beyond the gate, ask about the bridge. Status shows whether you can enter; the inspector shows your inventory.");
    return;
  }
  if (has_word("status") == 1) {
    if (state("permit") == 1) {
      reply("status.permit", "The keeper recognizes your permit. The gate is open to you.");
    } else {
      reply("status.no-permit", "You still need a permit. The keeper asks for two coins.");
    }
    return;
  }
  if (has_word("lantern") == 1) {
    if (state("lantern") == 1) {
      reply("lantern.owned", "Your lantern is already lit. No need to buy another.");
    } else {
      if (state("coins") >= 1) {
        set_state("coins", state("coins") - 1);
        set_state("lantern", 1);
        reply("lantern.bought", "One coin changes hands. Warm light fills the archway.");
      } else {
        reply("lantern.no-coins", "You do not have a coin for the lantern.");
      }
    }
    return;
  }
  if (has_word("buy") == 1) {
    if (state("permit") == 1) {
      reply("permit.owned", "The keeper taps your existing permit. One is quite enough.");
    } else {
      if (state("coins") >= 2) {
        set_state("coins", state("coins") - 2);
        set_state("permit", 1);
        reply("permit.bought", "Two coins for a stamped permit. The keeper steps aside.");
      } else {
        reply("permit.no-coins", "A permit costs two coins. Come back when you have them.");
      }
    }
    return;
  }
  if (has_word("enter") == 1) {
    if (state("permit") == 1) {
      set_state("inside", 1);
      reply("gate.entered", "You pass under Ember Gate. A bridge disappears into the mist ahead.");
    } else {
      reply("gate.closed", "The keeper lowers a spear. No permit, no passage.");
    }
    return;
  }
  if (has_word("bridge") == 1) {
    if (state("inside") == 0) {
      reply("bridge.outside", "You cannot reach the bridge until you enter the gate.");
    } else {
      if (state("lantern") == 1) {
        set_state("crossed", 1);
        reply("bridge.crossed", "Your light reveals the missing plank. You cross safely. The road is yours.");
      } else {
        reply("bridge.dark", "The bridge is too dark to trust. The keeper still sells lanterns.");
      }
    }
    return;
  }
  reply("unmatched", common::unknown());
}
