extern priority() -> value;
extern kind() -> value;
extern minutes() -> value;
extern completed() -> value;
extern due_days() -> value;
extern emit(score) -> void;
extern explain(reason) -> void;

func rank() -> void {
  var p = priority();
  var k = kind();
  var m = minutes();
  var done = completed();
  var due = due_days();
  var score = p * 20 - m / 5;
  explain("Priority affects ranking");
  if (k == "bug") {
    score = score + 80;
    explain("Bug urgency boost");
  } else {
    score = score + 0;
  }
  if (due < 0) {
    score = score + 200;
    explain("Overdue");
  } else {
    if (due == 0) {
      score = score + 150;
      explain("Due today");
    } else {
      if (due < 4) {
        score = score + 60;
        explain("Due soon");
      } else {
        score = score + 0;
      }
    }
  }
  if (done == 1) {
    score = -1000000;
    explain("Completed");
  } else {
    score = score + 0;
  }
  emit(score);
}
