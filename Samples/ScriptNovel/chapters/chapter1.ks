// ScriptNovel chapter 1: The Ember Gate (16 beats, 0-15).
//
// Host protocol (tiny, fixed; also in source/main.cpp):
//   beat() value-use: host-owned beat cursor (script keeps no state).
//   get_flag(name) value-use: host flag, 0 when never set.
//   last_choice() value-use: 1-3 after a choice press, else 0.
//   say(actor, text) statement-use: one dialogue line (actor is a name).
//   offer(a, b, c) statement-use: choice labels ("" hides that button).
//   set_scene(text) statement-use: title bar for this beat.
//   set_flag(name, v) statement-use: host stores v under name.
// Choice beats (2, 4, 7, 13): first call has last_choice 0 and only
// offers; the choice press re-calls the SAME beat with 1-3 and the script
// resolves it (sets flags, says the outcome). Next advances the cursor.
// Endings: beat 15 picks 1A when the mage was helped and the hero stood,
// else 1B. play_beat() is void: bare returns only, no return expr.
module chapter1;
import cast;
import world;
extern beat();
extern get_flag(name);
extern last_choice();
extern say(actor, text);
extern offer(a, b, c);
extern set_scene(text);
extern set_flag(name, v);
export func play_beat() {
  var b = beat();
  if (b == 0) {
    set_scene(world::title1());
    say(cast::name(0), "The Ember Gate has stood a hundred years. Tonight it trembles.");
    say(cast::name(0), "You are Ash, lanternless, road-worn, and exactly on time.");
    say(cast::name(0), world::describe(0));
    say(cast::name(0), "Somewhere beyond the arch, a seal is cracking. Listen.");
    return;
  }
  if (b == 1) {
    set_scene(world::title1());
    say(cast::name(1), "You heard it too. Good. Most sleep through the end of the world.");
    say(cast::name(0), world::describe(1));
    say(cast::name(1), "I am Mara. The seal is mine to keep, and I am losing it.");
    say(cast::name(1), "Will you help me lift it before the hollow wind rises?");
    set_flag("met_mage", 1);
    return;
  }
  if (b == 2) {
    set_scene(world::title1());
    var c = last_choice();
    if (c == 0) {
      say(cast::name(1), "Choose now. The chalk will not wait, and neither will I.");
      offer("Help Mara lift the seal", "Refuse and walk on", "");
      return;
    }
    if (c == 1) {
      say(cast::name(1), "Then take this chalk. Draw where I point, and do not smudge.");
      say(cast::name(0), "Her hand is steady. Yours will learn to be.");
      set_flag("helped_mage", 1);
      return;
    }
    say(cast::name(1), "Walk on, then. The gate will remember that you walked.");
    say(cast::name(0), "Her voice follows you a long way down the steps.");
    set_flag("refused_mage", 1);
    return;
  }
  if (b == 3) {
    set_scene(world::title1());
    if (get_flag("helped_mage") == 1) {
      say(cast::name(1), "First rune: ember. It means warmth, and also warning.");
      say(cast::name(0), "The chalk line glows faintly where your hand passes.");
      say(cast::name(1), "Second rune: threshold. It means the door holds.");
      say(cast::name(0), "You repeat the shapes until your fingers ache.");
      set_flag("rune_known", 1);
      return;
    }
    say(cast::name(1), "No chalk for the unwilling. Watch, if you must, from the steps.");
    say(cast::name(0), "Mara works alone, quick and angry, and the seal keeps cracking.");
    say(cast::name(1), "Do not touch anything. Especially not that.");
    return;
  }
  if (b == 4) {
    set_scene(world::title1());
    var d = last_choice();
    if (d == 0) {
      say(cast::name(2), "You there. Gate forge is dark, and my spare lantern hangs unused.");
      say(cast::name(0), world::describe(2));
      say(cast::name(2), "Take it or leave it. Iron does not beg.");
      offer("Take the lantern", "Leave it hanging", "");
      return;
    }
    if (d == 1) {
      say(cast::name(2), "Trim the wick when it smokes. Oil is in the base.");
      say(cast::name(0), "The lantern is warm already, as if it expected you.");
      set_flag("has_lantern", 1);
      return;
    }
    say(cast::name(2), "Proud. The dark loves the proud.");
    say(cast::name(0), "The lantern sways on its hook as you pass.");
    set_flag("left_lantern", 1);
    return;
  }
  if (b == 5) {
    set_scene(world::title1());
    if (get_flag("has_lantern") == 1) {
      say(cast::name(0), "The tunnel takes your light and gives back long shadows.");
      say(cast::name(2), "Stay left where the stones sweat. That water is deep.");
      say(cast::name(0), "Your lantern finds every step before your boot does.");
      say(cast::name(2), "Good. A lit guest is a living guest.");
    } else {
      say(cast::name(0), "The tunnel is a throat, and you walk down it blind.");
      say(cast::name(0), "Your shin finds a stone. The stone wins the argument.");
      say(cast::name(2), "That crash was you, I hope, and not the gate waking.");
    }
    var n = cast::name(1);
    if (n == "Mara the Mage") {
      say(cast::name(0), "Mara's chalk marks glow on the walls, pointing the way.");
    } else {
      say(cast::name(0), "The walls are bare of marks, and the dark feels endless.");
    }
    return;
  }
  if (b == 6) {
    set_scene(world::title1());
    say(cast::name(3), "Lost? Everyone is lost down here. I sell directions.");
    say(cast::name(0), world::describe(3));
    say(cast::name(3), "I am Wren. I know the shortcut to the gate heart.");
    set_flag("met_wren", 1);
    return;
  }
  if (b == 7) {
    set_scene(world::title1());
    var e = last_choice();
    if (e == 0) {
      say(cast::name(3), "Shortcut runs past the cistern. Long road runs past the guards.");
      say(cast::name(3), "Both reach the heart. Only one reaches it dry.");
      offer("Trust Wren's shortcut", "Take the long road", "");
      return;
    }
    if (e == 1) {
      say(cast::name(3), "Wise. Hold my sleeve at the third turning.");
      say(cast::name(0), "Her sleeve smells of rain and crushed herbs.");
      set_flag("trusts_wren", 1);
      return;
    }
    say(cast::name(3), "Suit yourself. The guards bite, but only a little.");
    say(cast::name(0), "Wren whistles as you part ways at the crossing.");
    set_flag("went_alone", 1);
    return;
  }
  if (b == 8) {
    set_scene(world::title1());
    if (get_flag("trusts_wren") == 1) {
      say(cast::name(3), "Third turning. Duck. The pipe drips, mind your eyes.");
      say(cast::name(0), "The shortcut spills you out above the gate heart, dry and grinning.");
      say(cast::name(3), "Told you. Wren never lies about water.");
    } else {
      say(cast::name(0), "The long road climbs past torch after torch, guard after guard.");
      say(cast::name(0), "Your answers satisfy them, barely, and your boots complain.");
      say(cast::name(0), "The gate heart waits below, humming like a struck bell.");
      say(cast::name(3), "There you are. Wet, but walking. The road provides.");
    }
    return;
  }
  if (b == 9) {
    set_scene(world::title1());
    if (get_flag("rune_known") == 1) {
      say(cast::name(1), "The runes you drew hold the near arch. Feel it steady?");
      say(cast::name(0), "The humming drops half a tone. Mara exhales for the first time.");
      say(cast::name(1), "The far seal still strains, but we have cut its work in half.");
      set_flag("seal_down", 1);
    } else {
      say(cast::name(1), "The near arch shudders. Nobody drew the holding runes.");
      say(cast::name(0), "Dust falls like snow. Somewhere, stone groans.");
      say(cast::name(1), "Stand clear of the arch. When it goes, it goes outward.");
    }
    return;
  }
  if (b == 10) {
    set_scene(world::title1());
    if (get_flag("has_lantern") == 1) {
      say(cast::name(0), "Your lantern still burns, oil barely touched. Bram overfilled it.");
      say(cast::name(2), "Overfilled? That is called generosity. You are welcome.");
      say(cast::name(0), "Light like this is a promise: the dark has not won yet.");
    } else {
      say(cast::name(0), "The dark presses close now, curious, tasting your edges.");
      say(cast::name(0), "You walk by trailing one hand along the sweating wall.");
      say(cast::name(2), "Still no lantern, still walking. Stubborn. I respect it.");
    }
    return;
  }
  if (b == 11) {
    set_scene(world::title1());
    if (get_flag("trusts_wren") == 1) {
      say(cast::name(3), "You held my sleeve. I do not forget that. Take this token.");
      say(cast::name(0), "The token is a river stone, smooth, with a hole worn through.");
      say(cast::name(3), "When the hollow wind rises, hold it tight and think of rain.");
    } else {
      say(cast::name(3), "You walked alone and lived. The road respects that too.");
      say(cast::name(0), "Wren salutes you with two fingers, outrider style.");
      say(cast::name(3), "Next time, though, take the shortcut. My feet insist.");
    }
    return;
  }
  if (b == 12) {
    set_scene(world::title1());
    if (get_flag("helped_mage") == 1) {
      say(cast::name(1), "Whatever comes through the gate tonight, we face it chalked and ready.");
      say(cast::name(0), "Mara ties back her hair. Her chalk hand does not shake.");
      say(cast::name(1), "Stay behind me until I call. Then stand beside me.");
    } else {
      say(cast::name(2), "The mage works alone up there. Alone is how mistakes happen.");
      say(cast::name(0), "Bram hefts his hammer and takes the stair two steps at a time.");
      say(cast::name(2), "Whatever she did or did not teach you, keep your head down.");
    }
    return;
  }
  if (b == 13) {
    set_scene(world::title1());
    var f = last_choice();
    if (f == 0) {
      say(cast::name(0), "The gate bulges inward. The hollow wind rises through the cracks.");
      say(cast::name(0), "Every instinct says run. Every story says stand.");
      offer("Stand at the gate", "Flee into the night", "");
      return;
    }
    if (f == 1) {
      say(cast::name(0), "You plant your feet on the threshold line and do not move.");
      say(cast::name(1), "Beside me. Now. Whatever comes, it meets us both.");
      set_flag("stood_fast", 1);
      return;
    }
    say(cast::name(0), "You run, and the stairs blur, and the night takes you.");
    say(cast::name(0), "Behind you the gate roars, or perhaps that is your heart.");
    set_flag("fled", 1);
    return;
  }
  if (b == 14) {
    set_scene(world::title1());
    if (get_flag("stood_fast") == 1) {
      say(cast::name(0), "The seal splits with a sound like winter ice on a lake.");
      say(cast::name(1), "Chalk up. Ember rune first. On my mark.");
      say(cast::name(0), "Light pours through the cracks, gold and furious and warm.");
    } else {
      say(cast::name(0), "From the hillside you watch light pour through the broken seal.");
      say(cast::name(0), "Gold and furious and warm, and very far away.");
      say(cast::name(2), "Bram's voice carries up the hill: somebody stand with her.");
    }
    return;
  }
  set_scene(world::title1());
  if (get_flag("stood_fast") == 1) {
    if (get_flag("helped_mage") == 1) {
      say(cast::name(0), world::ending1a());
      say(cast::name(1), "The gate holds. The dawn comes up like a held note released.");
      say(cast::name(0), "Your chalk lines glow in the sunrise, every rune in place.");
      say(cast::name(1), "Ember Gate stands because two pairs of hands held it. Remember this.");
      set_flag("ending1a", 1);
      return;
    }
  }
  say(cast::name(0), world::ending1b());
  say(cast::name(0), "The gate holds, but barely, and the dawn tastes of smoke.");
  say(cast::name(1), "It stands. Do not ask me what it cost.");
  say(cast::name(0), "Somewhere the wind carries chalk dust out to sea.");
  set_flag("ending1b", 1);
}
