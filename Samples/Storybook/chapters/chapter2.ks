module chapter2;
import cast;
import world;
extern hero() -> value;
extern get_flag(name) -> value;
extern set_flag(name, value) -> void;
extern title(text) -> void;
extern say(actor, text) -> void;
extern choice(id, label, target) -> void;
extern next(target) -> void;
extern finish(ending) -> void;

export func play(n) -> void {
  if (n == "road.arrival") {
    title(world::title2());
    if (get_flag("gate_saved") == 1) {
      say(cast::name(0), "Behind you, Ember Gate rings its dawn bell. Ahead lies the Hollow Road.");
    } else {
      say(cast::name(0), "Smoke follows you from Ember Gate. The Hollow Road offers no promises.");
    }
    say(cast::name(3), "The road rearranges itself. A map helps. So does a friend.");
    if (get_flag("has_map") == 1) {
      say(cast::name(0), "The map you found in the culvert marks a garden beyond the second ford.");
      next("road.garden");
    } else {
      choice("map", "Accept Wren's spare map", "road.map");
      choice("alone", "Find your own way", "road.lost");
    }
    return;
  }
  if (n == "road.map") {
    title("Ink that holds true");
    say(cast::name(3), "Roll it tight. Wet paper lies, and dry paper saves.");
    set_flag("has_map", 1);
    next("road.garden");
    return;
  }
  if (n == "road.lost") {
    title("The ford finds you");
    say(cast::name(0), "Cold water to your knees, then your thighs. You scramble out with an empty satchel.");
    say(cast::name(3), "That splash was educational. The next ford is deeper.");
    set_flag("lost_time", 1);
    next("road.shadow");
    return;
  }
  if (n == "road.garden") {
    title("Wren's garden");
    say(cast::name(0), "Beyond the second ford grows feverfew. Wren limps toward it, favoring her ankle.");
    choice("share", "Gather herbs and bind Wren's ankle", "road.friend");
    choice("keep", "Keep the herbs for yourself", "road.herbs");
    return;
  }
  if (n == "road.friend") {
    title("A fair trade");
    say(cast::name(3), "Herbs for a guide. Luck for a friend. I will walk with you to the Hollow.");
    set_flag("shared_herbs", 1);
    next("road.shadow");
    return;
  }
  if (n == "road.herbs") {
    title("Every traveler counts their coins");
    say(cast::name(3), "Kept. Fair. I will be a little slower. Do not wait on my account.");
    set_flag("kept_herbs", 1);
    next("road.shadow");
    return;
  }
  if (n == "road.shadow") {
    title("The shadow of the seal");
    say(cast::name(1), "One true answer opens this road. Why do you walk it?");
    if (get_flag("gate_saved") == 1) {
      say(cast::name(1), "I know what your hands can hold. Tell me what your heart holds.");
    }
    choice("truth", "To reach the far side with my name my own", "road.truth");
    choice("refuse", "Refuse to answer the shadow", "road.loop");
    return;
  }
  if (n == "road.truth") {
    title("The straight road");
    say(cast::name(0), "The shadow parts like a curtain. Your steps feel lighter.");
    set_flag("paid_price", 1);
    next("road.bridge");
    return;
  }
  if (n == "road.loop") {
    title("The same oak, again");
    say(cast::name(0), "The road loops twice. The third time past the oak you know every branch.");
    set_flag("lost_time", 1);
    next("road.bridge");
    return;
  }
  if (n == "road.bridge") {
    title("Bram's bridge");
    if (get_flag("has_charm") == 1) {
      say(cast::name(2), "Iron remembers the forge. The bridge remembers its smith. Trust the sway.");
    } else {
      say(cast::name(0), "You cross on all fours for the middle third. Nobody sees. Probably.");
    }
    say(cast::name(0), world::omen());
    next("hollow.threshold");
    return;
  }
  if (n == "hollow.threshold") {
    title("The Hollow listens");
    say(cast::name(0), "A still place opens ahead. Familiar faces offer you older, easier roads.");
    choice("face", "Step into the Hollow and speak your name", "hollow.face");
    choice("return", "Turn back toward Ember Gate", "hollow.return");
    return;
  }
  if (n == "hollow.face") {
    title("A named traveler");
    say("Traveler", hero());
    if (get_flag("has_map") == 1 and get_flag("paid_price") == 1) {
      say(cast::name(0), world::ending2a());
      if (get_flag("shared_herbs") == 1) {
        say(cast::name(3), "The road straightens behind us. Every cairn still stands. We made it, friend.");
        finish("The road remembers its friends");
      } else {
        say(cast::name(0), "Chart in hand, name on your lips, you walk the Hollow through. Alone, but yourself.");
        finish("The road remembers your name");
      }
      set_flag("escaped", 1);
    } else {
      say(cast::name(0), world::ending2b());
      if (get_flag("has_charm") == 1) {
        say(cast::name(0), "The charm turns warm. You hold it and find the bridge again. One more chance.");
        next("road.second-chance");
      } else {
        say(cast::name(0), "Somewhere a chart with your route gathers dust. Here, the road never ends.");
        finish("A traveler in the Hollow");
        set_flag("trapped", 1);
      }
    }
    return;
  }
  if (n == "road.second-chance") {
    title("Cold iron opens a way");
    say(cast::name(3), "Back again? Take the map this time. Then tell the shadow the truth.");
    set_flag("has_map", 1);
    next("road.shadow");
    return;
  }
  if (n == "hollow.return") {
    title("The way home");
    if (get_flag("gate_saved") == 1) {
      say(cast::name(0), "Ember Gate's bell carries across the hills. There is still a place for you beside the seal.");
      finish("Keeper of Ember Gate");
    } else {
      say(cast::name(2), "The gate needs rebuilding. A road can wait. Hand me that hammer.");
      finish("A promise to rebuild");
    }
    set_flag("returned", 1);
    return;
  }
}
