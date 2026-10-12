// ScriptShrink brain: Freud personality (classic reflective therapist).
//
// Host protocol (tiny, fixed):
//   has_word(word) value-use: 1 when the CURRENT user input contains word
//     as a whole word (host lowercases, ASCII alnum boundaries), else 0.
//   mood() value-use: current host-side mood score (-10..10).
//   emit(text) statement-use: the reply line for this turn.
//   emit_mood(n) statement-use: host adds n to mood, clamped to -10..10.
//
// Script state resets per call, so mood lives host-side. Flat early-return
// chain: crisis keywords first, then 42 topic rules, then mood-based
// deflection fallbacks. respond() is void: bare return only, no return expr.
// String law: literals only flow into emit(); ==/!= on strings never used
// here because matching is delegated to has_word().
extern has_word(word);
extern mood();
extern emit(text);
extern emit_mood(n);
func respond() {
  if (has_word("die") == 1) {
    emit("You sound like you may be in crisis. Please contact a crisis helpline or someone you trust right now.");
    emit_mood(-3);
    return;
  }
  if (has_word("kill") == 1) {
    emit("You sound like you may be in crisis. Please contact a crisis helpline or someone you trust right now.");
    emit_mood(-3);
    return;
  }
  if (has_word("suicide") == 1) {
    emit("You sound like you may be in crisis. Please contact a crisis helpline or someone you trust right now.");
    emit_mood(-3);
    return;
  }
  if (has_word("hurt") == 1) {
    emit("You sound like you may be in crisis. Please contact a crisis helpline or someone you trust right now.");
    emit_mood(-3);
    return;
  }
  if (has_word("mother") == 1) {
    emit("Tell me more about your mother. What comes to mind first?");
    emit_mood(0);
    return;
  }
  if (has_word("father") == 1) {
    emit("How would you describe your father?");
    emit_mood(0);
    return;
  }
  if (has_word("dream") == 1) {
    emit("Dreams carry meaning. What stood out most in that dream?");
    emit_mood(1);
    return;
  }
  if (has_word("work") == 1) {
    emit("Work fills so much of life. How does your work make you feel?");
    emit_mood(-1);
    return;
  }
  if (has_word("boss") == 1) {
    emit("Authority figures stir strong feelings. Tell me about your boss.");
    emit_mood(-1);
    return;
  }
  if (has_word("sad") == 1) {
    emit("Sadness is heavy. How long has this sadness been with you?");
    emit_mood(-2);
    return;
  }
  if (has_word("happy") == 1) {
    emit("Happiness matters. What do you believe brought this on?");
    emit_mood(2);
    return;
  }
  if (has_word("angry") == 1) {
    emit("Anger points somewhere. What feels unfair to you?");
    emit_mood(-1);
    return;
  }
  if (has_word("fear") == 1) {
    emit("Fear protects and paralyzes. What frightens you most?");
    emit_mood(-1);
    return;
  }
  if (has_word("afraid") == 1) {
    emit("Being afraid is human. What feels threatening right now?");
    emit_mood(-1);
    return;
  }
  if (has_word("lonely") == 1) {
    emit("Loneliness aches. Who feels distant from you lately?");
    emit_mood(-2);
    return;
  }
  if (has_word("love") == 1) {
    emit("Love shapes us. Who do you love, and how is that?");
    emit_mood(1);
    return;
  }
  if (has_word("hate") == 1) {
    emit("Hate is heavy to carry. What feeds that feeling?");
    emit_mood(-1);
    return;
  }
  if (has_word("wife") == 1) {
    emit("Marriage holds mirrors. How are things with your wife?");
    emit_mood(0);
    return;
  }
  if (has_word("husband") == 1) {
    emit("Marriage holds mirrors. How are things with your husband?");
    emit_mood(0);
    return;
  }
  if (has_word("child") == 1) {
    emit("Children touch deep places. Tell me about your child.");
    emit_mood(1);
    return;
  }
  if (has_word("children") == 1) {
    emit("Children touch deep places. Tell me about your children.");
    emit_mood(1);
    return;
  }
  if (has_word("money") == 1) {
    emit("Money carries anxiety. What does money mean to you?");
    emit_mood(-1);
    return;
  }
  if (has_word("school") == 1) {
    emit("School years leave marks. What about school is on your mind?");
    emit_mood(0);
    return;
  }
  if (has_word("friend") == 1) {
    emit("Friendship mirrors us. Tell me about this friend.");
    emit_mood(1);
    return;
  }
  if (has_word("sleep") == 1) {
    emit("Sleep and mood walk together. How have you been sleeping?");
    emit_mood(0);
    return;
  }
  if (has_word("tired") == 1) {
    emit("Exhaustion speaks. What tires you most these days?");
    emit_mood(-1);
    return;
  }
  if (has_word("stress") == 1) {
    emit("Stress narrows everything. What presses on you right now?");
    emit_mood(-1);
    return;
  }
  if (has_word("anxious") == 1) {
    emit("Anxiety anticipates. What future moment worries you?");
    emit_mood(-1);
    return;
  }
  if (has_word("guilty") == 1) {
    emit("Guilt asks for repair. What do you feel guilty about?");
    emit_mood(-1);
    return;
  }
  if (has_word("shame") == 1) {
    emit("Shame hides. What feels hard to say aloud?");
    emit_mood(-1);
    return;
  }
  if (has_word("hope") == 1) {
    emit("Hope is a thread. What keeps your hope alive?");
    emit_mood(1);
    return;
  }
  if (has_word("future") == 1) {
    emit("The future pulls us. What do you picture ahead?");
    emit_mood(1);
    return;
  }
  if (has_word("past") == 1) {
    emit("The past lingers. What memory keeps returning?");
    emit_mood(0);
    return;
  }
  if (has_word("death") == 1) {
    emit("Death ends and starts thoughts. What about death is with you?");
    emit_mood(-2);
    return;
  }
  if (has_word("help") == 1) {
    emit("Asking for help is a start. What help do you need most?");
    emit_mood(1);
    return;
  }
  if (has_word("sorry") == 1) {
    emit("Sorry opens doors. What are you sorry about?");
    emit_mood(-1);
    return;
  }
  if (has_word("thank") == 1) {
    emit("Gratitude steadies. What are you thankful for?");
    emit_mood(1);
    return;
  }
  if (has_word("morning") == 1) {
    emit("Mornings set tone. How do your mornings usually go?");
    emit_mood(1);
    return;
  }
  if (has_word("night") == 1) {
    emit("Nights invite reflection. What keeps you up at night?");
    emit_mood(0);
    return;
  }
  if (has_word("food") == 1) {
    emit("Appetite and feeling link. How is food for you lately?");
    emit_mood(0);
    return;
  }
  if (has_word("dog") == 1) {
    emit("Pets read our hearts. Tell me about your dog.");
    emit_mood(1);
    return;
  }
  if (has_word("cat") == 1) {
    emit("Pets read our hearts. Tell me about your cat.");
    emit_mood(1);
    return;
  }
  if (has_word("brother") == 1) {
    emit("Siblings know old versions of us. How is your brother?");
    emit_mood(0);
    return;
  }
  if (has_word("sister") == 1) {
    emit("Siblings know old versions of us. How is your sister?");
    emit_mood(0);
    return;
  }
  if (has_word("home") == 1) {
    emit("Home should hold us. What is home like for you now?");
    emit_mood(0);
    return;
  }
  if (has_word("job") == 1) {
    emit("Jobs carry identity. What about your job is on your mind?");
    emit_mood(-1);
    return;
  }
  var m = mood();
  if (m < -2) {
    emit("This is a heavy place. Stay with that feeling and tell me what weighs most.");
    emit_mood(0);
    return;
  }
  if (m < 0) {
    emit("I hear a shadow in that. Say more about what is on your mind.");
    emit_mood(0);
    return;
  }
  if (m > 2) {
    emit("There is energy in that. Where does that feeling come from?");
    emit_mood(0);
    return;
  }
  emit("I see. Please go on.");
  emit_mood(0);
}
