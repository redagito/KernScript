// ScriptNovel chapter 2: The Hollow Road (16 beats, 0-15).
//
// Same host protocol as chapter 1 (see chapter1.ks and source/main.cpp):
// beat()/get_flag()/last_choice() value-use; say()/offer()/set_scene()/
// set_flag() statement-use. Adds hero() value-use: the host-owned hero
// name string, passed straight into say() as a line. Choice beats here
// are 1, 4, 7, 9, 13 with the same offer-then-resolve shape. The map is
// the run-once inventory item: taking it at beat 1 changes beats 2 and 6
// and gates ending 2A. play_beat() is void: bare returns only.
module chapter2;
import cast;
import world;
extern beat();
extern get_flag(name);
extern last_choice();
extern hero();
extern say(actor, text);
extern offer(a, b, c);
extern set_scene(text);
extern set_flag(name, v);
export func play_beat() {
  var b = beat();
  if (b == 0) {
    set_scene(world::title2());
    say(cast::name(0), "The Ember Gate is behind you. The Hollow Road is ahead.");
    var h = hero();
    say(cast::name(0), h);
    say(cast::name(0), "That is what the road calls you now. Walk, and listen.");
    say(cast::name(0), world::describe(0));
    return;
  }
  if (b == 1) {
    set_scene(world::title2());
    var c = last_choice();
    if (c == 0) {
      say(cast::name(3), "Road traveler. My map case is full and my pockets fuller.");
      say(cast::name(0), world::describe(3));
      say(cast::name(3), "Take my spare chart. The hollow moves the stones around.");
      offer("Accept the map", "Refuse the map", "");
      return;
    }
    if (c == 1) {
      say(cast::name(3), "Roll it tight. Wet paper lies, and dry paper saves.");
      say(cast::name(0), "The chart shows three fords, two bridges, and one warning.");
      set_flag("has_map", 1);
      return;
    }
    say(cast::name(3), "Proud feet make their own maps. Usually wrong ones.");
    say(cast::name(0), "Wren keeps the chart. The road keeps its secrets.");
    set_flag("refused_map", 1);
    return;
  }
  if (b == 2) {
    set_scene(world::title2());
    if (get_flag("has_map") == 1) {
      say(cast::name(0), "First ford exactly where the chart promised. Wren's ink holds true.");
      say(cast::name(3), "Told you. I never lie about water. Only about dice.");
      say(cast::name(0), "You mark the crossing with a cairn for the next traveler.");
      say(cast::name(3), "Kindness. The road notices kindness. It remembers.");
    } else {
      say(cast::name(0), "No chart, no cairns. The first ford finds you instead.");
      say(cast::name(0), "Cold water to the knees, then to the thighs, then a scramble out.");
      say(cast::name(3), "That splash was educational. The next ford is deeper.");
    }
    return;
  }
  if (b == 3) {
    set_scene(world::title2());
    say(cast::name(2), "Iron wears out. Boots wear out. Only the road keeps going.");
    say(cast::name(0), world::describe(2));
    var n = cast::name(2);
    if (n == "Bram the Blacksmith") {
      say(cast::name(0), "Bram's forge mark is on the milestone: a hammer over a gate.");
    } else {
      say(cast::name(0), "The milestone is weathered past reading.");
    }
    say(cast::name(2), "I walked this road before the hollow learned my name.");
    return;
  }
  if (b == 4) {
    set_scene(world::title2());
    var d = last_choice();
    if (d == 0) {
      say(cast::name(2), "Cold iron keeps the hollow from noticing you. Mostly.");
      say(cast::name(2), "This charm is spare. Cold iron, warm hands, fair trade.");
      offer("Take the iron charm", "Leave the charm", "");
      return;
    }
    if (d == 1) {
      say(cast::name(2), "Wear it inside your shirt. It itches less than fear.");
      say(cast::name(0), "The charm is heavy for its size, and cold even in sunlight.");
      set_flag("has_charm", 1);
      return;
    }
    say(cast::name(2), "Bare neck, brave heart. The hollow prefers them that way.");
    say(cast::name(0), "Bram pockets the charm with a shrug that means good luck.");
    set_flag("left_charm", 1);
    return;
  }
  if (b == 5) {
    set_scene(world::title2());
    if (get_flag("has_charm") == 1) {
      say(cast::name(0), "The hollow wind tests you once, finds iron, and moves on.");
      say(cast::name(2), "See? It prefers its travelers unseasoned.");
      say(cast::name(0), "The charm warms one degree, as if satisfied.");
    } else {
      say(cast::name(0), "The hollow wind circles twice, curious, tasting your edges.");
      say(cast::name(0), "You keep walking and do not look at the treeline.");
      say(cast::name(2), "Steady. It only takes the ones who stop.");
    }
    return;
  }
  if (b == 6) {
    set_scene(world::title2());
    if (get_flag("has_map") == 1) {
      say(cast::name(0), "The chart marks an herb garden behind the second bridge.");
      say(cast::name(3), "Wren's own patch. Take what you need, leave the roots.");
      say(cast::name(0), "You gather feverfew and woundwort into your satchel.");
      set_flag("has_herbs", 1);
    } else {
      say(cast::name(0), "No chart, no garden. Your satchel stays empty and your ribs show.");
      say(cast::name(0), "The road provides little for those who walk it blind.");
      say(cast::name(3), "Hungry miles. I know them well. Keep walking.");
    }
    return;
  }
  if (b == 7) {
    set_scene(world::title2());
    if (get_flag("has_herbs") == 0) {
      say(cast::name(3), "You have nothing to share, and that is its own answer.");
      say(cast::name(0), "Wren walks slower now, favoring her left ankle.");
      say(cast::name(3), "Do not worry for me. Worry for the road ahead.");
      return;
    }
    var e = last_choice();
    if (e == 0) {
      say(cast::name(3), "That ankle I turned at the ford is turning colors.");
      say(cast::name(3), "Your satchel smells of feverfew. I am not proud. Share?");
      offer("Share the herbs", "Keep them", "");
      return;
    }
    if (e == 1) {
      say(cast::name(3), "Bound tight, bitter tea, and I will walk like new by dusk.");
      say(cast::name(0), "You split the bundle. Wren's grin returns like sunrise.");
      set_flag("shared_herbs", 1);
      return;
    }
    say(cast::name(3), "Kept. Fair. Every traveler counts their own coins.");
    say(cast::name(0), "Wren limps on without complaint, which is worse than complaint.");
    set_flag("kept_herbs", 1);
    return;
  }
  if (b == 8) {
    set_scene(world::title2());
    if (get_flag("shared_herbs") == 1) {
      say(cast::name(3), "Dusk, as promised: walking like new, ankle quiet.");
      say(cast::name(0), "Wren scouts ahead, light-footed, whistling through the gap.");
      say(cast::name(3), "Herbs for a guide, luck for a friend. Fair trade, friend.");
    } else {
      say(cast::name(3), "The ankle holds, barely, one careful mile at a time.");
      say(cast::name(0), "You shorten your stride to hers and say nothing about it.");
      say(cast::name(3), "Kind of you. Do not think I failed to notice.");
    }
    return;
  }
  if (b == 9) {
    set_scene(world::title2());
    var f = last_choice();
    if (f == 0) {
      say(cast::name(1), "The hollow road crosses my seal's shadow. Nothing passes free.");
      say(cast::name(1), "My price: one true answer. Why do you walk this road?");
      offer("Pay Mara's price", "Refuse the price", "");
      return;
    }
    if (f == 1) {
      say(cast::name(0), "You answer truly: to reach the far side with your name your own.");
      say(cast::name(1), "A good answer. The shadow thins. Walk through, and walk tall.");
      set_flag("paid_price", 1);
      return;
    }
    say(cast::name(1), "Then the shadow keeps you. It is patient. It can wait.");
    say(cast::name(0), "The road loops back on itself twice before letting you on.");
    set_flag("refused_price", 1);
    return;
  }
  if (b == 10) {
    set_scene(world::title2());
    if (get_flag("paid_price") == 1) {
      say(cast::name(0), "The shadow parts like a curtain. Beyond it the road runs straight.");
      say(cast::name(1), "True answers open every gate. Remember that at the Hollow.");
      say(cast::name(0), "Your steps feel lighter, as if the road approved.");
    } else {
      say(cast::name(0), "The road loops: same oak, same milestone, same cold spring.");
      say(cast::name(0), "Third time past the oak you memorize its branches in defeat.");
      say(cast::name(1), "The shadow will keep you until you pay. It told you so.");
    }
    return;
  }
  if (b == 11) {
    set_scene(world::title2());
    if (get_flag("has_charm") == 1) {
      say(cast::name(0), "Bram's bridge sways over white water, but the charm steadies you.");
      say(cast::name(2), "I built it to sway. Swaying is stronger. Trust the sway.");
      say(cast::name(0), "Mid-span the hollow howls below, and the bridge holds.");
    } else {
      say(cast::name(0), "Bram's bridge sways over white water, and your knees sway with it.");
      say(cast::name(0), "You cross on all fours for the middle third. Nobody sees. Probably.");
      say(cast::name(2), "Crossed is crossed. The bridge tells no tales.");
    }
    return;
  }
  if (b == 12) {
    set_scene(world::title2());
    say(cast::name(0), world::omen());
    say(cast::name(3), "The wind just stopped. Wren's face says what that means.");
    say(cast::name(3), "Softly now. The Hollow is listening, and it knows our names.");
    return;
  }
  if (b == 13) {
    set_scene(world::title2());
    var g = last_choice();
    if (g == 0) {
      say(cast::name(0), "The Hollow opens ahead: a still place where the road forgets itself.");
      say(cast::name(0), "Turn back and the road keeps you. Go on and it tests you.");
      offer("Face the Hollow", "Turn back", "");
      return;
    }
    if (g == 1) {
      say(cast::name(0), "You step into the stillness with your name held tight.");
      say(cast::name(3), "I am with you. Say it back to me. Your name. Say it.");
      set_flag("faced", 1);
      return;
    }
    say(cast::name(0), "You turn, and the road lets you, and that is the whole trouble.");
    say(cast::name(0), "Every step back is easy. Every easy step costs double.");
    set_flag("turned", 1);
    return;
  }
  if (b == 14) {
    set_scene(world::title2());
    if (get_flag("faced") == 1) {
      say(cast::name(0), "The stillness presses in, wearing familiar faces, offering old roads.");
      say(cast::name(0), "You say your name aloud, and the faces flinch.");
      say(cast::name(3), "Again. Louder. It hates a named traveler.");
    } else {
      say(cast::name(0), "The way back is smooth and sweet and wrong in every stone.");
      say(cast::name(0), "The milestones count down instead of up. You notice. You walk on.");
      say(cast::name(3), "It will spit you out somewhere. Somewhere is not nowhere.");
    }
    return;
  }
  set_scene(world::title2());
  if (get_flag("faced") == 1) {
    if (get_flag("has_map") == 1) {
      say(cast::name(0), world::ending2a());
      say(cast::name(0), "Chart in hand, name on lips, you walk the Hollow through.");
      say(cast::name(3), "The road straightens behind you like a held breath released.");
      say(cast::name(0), "Every cairn you raised still stands. The road remembers.");
      set_flag("ending2a", 1);
      return;
    }
  }
  say(cast::name(0), world::ending2b());
  say(cast::name(0), "The Hollow keeps what it catches, politely, forever.");
  say(cast::name(0), "Somewhere a chart with your route gathers dust in a map case.");
  say(cast::name(3), "Walk softly, traveler. It knows your name now.");
  set_flag("ending2b", 1);
}
