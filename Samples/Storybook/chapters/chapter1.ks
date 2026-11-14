module chapter1;
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

export func play(n) -> value {
  if (n == "start") {
    title(world::title1());
    say(cast::name(0), "Rain follows you to Ember Gate. Beyond its cracked seal, something is knocking.");
    say("Traveler", hero());
    say(cast::name(1), "One night until the seal breaks. I need a second pair of hands.");
    choice("mage", "Help Mara draw the holding runes", "gate.runes");
    choice("forge", "Ask Bram for supplies first", "gate.forge");
    choice("scout", "Find Wren and scout another way", "gate.scout");
    return 1;
  }
  if (n == "gate.runes") {
    title("Chalk and promises");
    say(cast::name(1), "An ember rune is a promise: the dark has not won yet. Draw it with me.");
    say(cast::name(0), "Your chalk lines join Mara's. The near arch stops shaking.");
    set_flag("rune_known", 1);
    set_flag("helped_mage", 1);
    next("gate.forge");
    return 1;
  }
  if (n == "gate.forge") {
    title("The last light in the forge");
    say(cast::name(2), "Lantern or cold iron? Oil will light your way. Iron may keep it clear.");
    choice("lantern", "Take Bram's lantern", "gate.lantern");
    choice("iron", "Take the iron charm", "gate.iron");
    return 1;
  }
  if (n == "gate.lantern") {
    title("A generous measure of oil");
    say(cast::name(2), "Overfilled? That is called generosity. You are welcome.");
    set_flag("has_lantern", 1);
    next("gate.scout");
    return 1;
  }
  if (n == "gate.iron") {
    title("Cold iron, warm hands");
    say(cast::name(2), "Wear it inside your shirt. It itches less than fear.");
    set_flag("has_charm", 1);
    next("gate.scout");
    return 1;
  }
  if (n == "gate.scout") {
    title("Wren's shortcut");
    say(cast::name(3), "A dry culvert leads above the gate heart. Or there is the guarded stair.");
    choice("trust", "Trust Wren and take the culvert", "gate.culvert");
    choice("stairs", "Take the guarded stair", "gate.stairs");
    return 1;
  }
  if (n == "gate.culvert") {
    title("Under the rain");
    say(cast::name(3), "Duck. The pipe drips, mind your eyes. Wren never lies about water.");
    if (get_flag("has_lantern") == 1) {
      say(cast::name(0), "Lantern light catches a forgotten map case wedged in the stone.");
      set_flag("has_map", 1);
    } else {
      say(cast::name(0), "In the dark you hold Wren's sleeve. She guides you through.");
    }
    set_flag("trusts_wren", 1);
    next("gate.heart");
    return 1;
  }
  if (n == "gate.stairs") {
    title("The long stair");
    say(cast::name(0), "Your answers satisfy the guards, barely. At the top, Mara waits alone.");
    if (get_flag("rune_known") == 0) {
      say(cast::name(1), "There is still time. Help me draw the last rune, or stand clear.");
      choice("learn", "Learn the rune and help", "gate.last-rune");
      choice("wait", "Keep watch instead", "gate.heart");
    } else {
      next("gate.heart");
    }
    return 1;
  }
  if (n == "gate.last-rune") {
    title("A second pair of hands");
    say(cast::name(1), "Ember first. Then the holding line. Good. Now stand beside me.");
    set_flag("rune_known", 1);
    set_flag("helped_mage", 1);
    next("gate.heart");
    return 1;
  }
  if (n == "gate.heart") {
    title("The gate heart");
    say(cast::name(0), "The gate bulges inward. The hollow wind rises through the cracks.");
    if (get_flag("rune_known") == 1) {
      say(cast::name(1), "Your runes hold the near arch. Together we can hold the far one.");
    } else {
      say(cast::name(1), "No holding runes. I can close it, but I cannot promise the cost.");
    }
    choice("stand", "Stand with Mara at the gate", "gate.stand");
    choice("flee", "Flee into the night", "gate.flee");
    return 1;
  }
  if (n == "gate.stand") {
    title("Dawn at Ember Gate");
    set_flag("stood_fast", 1);
    if (get_flag("rune_known") == 1) {
      say(cast::name(0), world::ending1a());
      say(cast::name(1), "The gate holds because two pairs of hands held it. Remember this.");
      set_flag("gate_saved", 1);
    } else {
      say(cast::name(0), world::ending1b());
      say(cast::name(1), "It stands. Do not ask me what it cost. Take the road while it is open.");
      set_flag("gate_damaged", 1);
    }
    next("road.arrival");
    return 1;
  }
  if (n == "gate.flee") {
    title("Ashes on the wind");
    say(cast::name(0), "You run. From the hillside, light pours through the seal. Mara's voice is lost in it.");
    say(cast::name(2), "The road is open. We will hold what is left. Go.");
    set_flag("fled", 1);
    set_flag("gate_damaged", 1);
    next("road.arrival");
    return 1;
  }
  return 0;
}
