// ScriptShrink brain: Coach personality (upbeat, action-oriented).
//
// Same host protocol as freud.ks: has_word(word) and mood() are value-use,
// emit(text) and emit_mood(n) are statement-use. Same 46 keyword rules in
// the same order so a personality switch changes tone, never coverage.
// respond() is void: bare return only, no return expr.
extern has_word(word);
extern mood();
extern emit(text);
extern emit_mood(n);
func respond() {
  if (has_word("die") == 1) {
    emit("I am worried about you. Please reach out to a crisis helpline or someone you trust right now.");
    emit_mood(-3);
    return;
  }
  if (has_word("kill") == 1) {
    emit("I am worried about you. Please reach out to a crisis helpline or someone you trust right now.");
    emit_mood(-3);
    return;
  }
  if (has_word("suicide") == 1) {
    emit("I am worried about you. Please reach out to a crisis helpline or someone you trust right now.");
    emit_mood(-3);
    return;
  }
  if (has_word("hurt") == 1) {
    emit("I am worried about you. Please reach out to a crisis helpline or someone you trust right now.");
    emit_mood(-3);
    return;
  }
  if (has_word("mother") == 1) {
    emit("Noted: mother matters to you. What is one action that would help there?");
    emit_mood(0);
    return;
  }
  if (has_word("father") == 1) {
    emit("What is one thing about your father you want to handle better?");
    emit_mood(0);
    return;
  }
  if (has_word("dream") == 1) {
    emit("Dreams spark ideas. What is one insight you take from it?");
    emit_mood(1);
    return;
  }
  if (has_word("work") == 1) {
    emit("Work is a project. What is the smallest win you could score today?");
    emit_mood(-1);
    return;
  }
  if (has_word("boss") == 1) {
    emit("Boss trouble is solvable. What is one conversation that could help?");
    emit_mood(-1);
    return;
  }
  if (has_word("sad") == 1) {
    emit("Sad spells pass. What usually lifts you, even a little?");
    emit_mood(-2);
    return;
  }
  if (has_word("happy") == 1) {
    emit("Great. How can you get more of that this week?");
    emit_mood(2);
    return;
  }
  if (has_word("angry") == 1) {
    emit("Anger is fuel. What boundary will you set this week?");
    emit_mood(-1);
    return;
  }
  if (has_word("fear") == 1) {
    emit("Fear shrinks with action. What is one brave step you could take?");
    emit_mood(-1);
    return;
  }
  if (has_word("afraid") == 1) {
    emit("Courage beats comfort. What would you try if you were not afraid?");
    emit_mood(-1);
    return;
  }
  if (has_word("lonely") == 1) {
    emit("Loneliness is a signal. Who could you message today?");
    emit_mood(-2);
    return;
  }
  if (has_word("love") == 1) {
    emit("Love is a strength. How will you show it this week?");
    emit_mood(1);
    return;
  }
  if (has_word("hate") == 1) {
    emit("Hate drains you. What would letting go of it free up?");
    emit_mood(-1);
    return;
  }
  if (has_word("wife") == 1) {
    emit("Partnerships need upkeep. What is one kind act for your wife?");
    emit_mood(0);
    return;
  }
  if (has_word("husband") == 1) {
    emit("Partnerships need upkeep. What is one kind act for your husband?");
    emit_mood(0);
    return;
  }
  if (has_word("child") == 1) {
    emit("Kids need presence. What is one focused hour you can give?");
    emit_mood(1);
    return;
  }
  if (has_word("children") == 1) {
    emit("Kids need presence. What is one focused hour you can give them?");
    emit_mood(1);
    return;
  }
  if (has_word("money") == 1) {
    emit("Money is math plus habits. What is one expense you could cut?");
    emit_mood(-1);
    return;
  }
  if (has_word("school") == 1) {
    emit("School is a skill ladder. What is the next rung for you?");
    emit_mood(0);
    return;
  }
  if (has_word("friend") == 1) {
    emit("Friendships grow with effort. When will you reach out?");
    emit_mood(1);
    return;
  }
  if (has_word("sleep") == 1) {
    emit("Sleep is recovery. What is your bedtime plan tonight?");
    emit_mood(0);
    return;
  }
  if (has_word("tired") == 1) {
    emit("Tired means recharge. What rest will you schedule today?");
    emit_mood(-1);
    return;
  }
  if (has_word("stress") == 1) {
    emit("Stress needs an outlet. What is your reset ritual this week?");
    emit_mood(-1);
    return;
  }
  if (has_word("anxious") == 1) {
    emit("Anxiety fades with reps. What small exposure could you try?");
    emit_mood(-1);
    return;
  }
  if (has_word("guilty") == 1) {
    emit("Guilt needs amends. What apology or fix is overdue?");
    emit_mood(-1);
    return;
  }
  if (has_word("shame") == 1) {
    emit("Shame hates daylight. Who safe could you tell?");
    emit_mood(-1);
    return;
  }
  if (has_word("hope") == 1) {
    emit("Hope plus a plan wins. What step feeds your hope?");
    emit_mood(1);
    return;
  }
  if (has_word("future") == 1) {
    emit("The future is built. What is your 30-day target?");
    emit_mood(1);
    return;
  }
  if (has_word("past") == 1) {
    emit("The past is data. What lesson will you carry forward?");
    emit_mood(0);
    return;
  }
  if (has_word("death") == 1) {
    emit("Grief needs space and motion. What ritual would honor that?");
    emit_mood(-2);
    return;
  }
  if (has_word("help") == 1) {
    emit("Help is leverage. Who exactly will you ask, and when?");
    emit_mood(1);
    return;
  }
  if (has_word("sorry") == 1) {
    emit("Sorry works with change. What will you do differently?");
    emit_mood(-1);
    return;
  }
  if (has_word("thank") == 1) {
    emit("Gratitude compounds. What three good things happened today?");
    emit_mood(1);
    return;
  }
  if (has_word("morning") == 1) {
    emit("Mornings are launchpads. What is your first win tomorrow?");
    emit_mood(1);
    return;
  }
  if (has_word("night") == 1) {
    emit("Nights need wind-down. What is your screen-off time?");
    emit_mood(0);
    return;
  }
  if (has_word("food") == 1) {
    emit("Food is fuel. What is one healthier swap this week?");
    emit_mood(0);
    return;
  }
  if (has_word("dog") == 1) {
    emit("Dogs love routine. When is your next walk together?");
    emit_mood(1);
    return;
  }
  if (has_word("cat") == 1) {
    emit("Cats keep their own hours. What playtime can you share?");
    emit_mood(1);
    return;
  }
  if (has_word("brother") == 1) {
    emit("Sibling ties matter. What message could you send your brother?");
    emit_mood(0);
    return;
  }
  if (has_word("sister") == 1) {
    emit("Sibling ties matter. What message could you send your sister?");
    emit_mood(0);
    return;
  }
  if (has_word("home") == 1) {
    emit("Home base matters. What one corner will you improve?");
    emit_mood(0);
    return;
  }
  if (has_word("job") == 1) {
    emit("Jobs reward skills. What skill will you sharpen next?");
    emit_mood(-1);
    return;
  }
  var m = mood();
  if (m < -2) {
    emit("Tough patch acknowledged. Name one tiny thing still in your control.");
    emit_mood(0);
    return;
  }
  if (m < 0) {
    emit("Low energy noted. What is one small recharge you could do today?");
    emit_mood(0);
    return;
  }
  if (m > 2) {
    emit("Love that energy. How will you use it in the next hour?");
    emit_mood(0);
    return;
  }
  emit("Got it. What is the next thing on your mind?");
  emit_mood(0);
}
