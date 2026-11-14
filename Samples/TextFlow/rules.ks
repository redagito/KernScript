extern contains(part) -> value;
extern upper() -> value;
extern replace(from, to) -> value;
extern line_number() -> value;
extern emit(text) -> void;

func process() -> void {
  if (line_number() > 0) {
    if (contains("debug") == 0) {
      if (contains("error") == 1) {
        emit(upper());
      } else {
        emit(replace("info:", "INFO:"));
      }
    }
  }
}
