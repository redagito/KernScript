extern cell_alive() -> value;
extern neighbors() -> value;
extern emit(next) -> void;

func decide() -> void {
  var alive = cell_alive();
  var count = neighbors();
  if (alive == 1) {
    if (count == 3 or count == 4 or count == 6 or count == 7 or count == 8) { emit(1); } else { emit(0); }
  } else {
    if (count == 3 or count == 6 or count == 7 or count == 8) { emit(1); } else { emit(0); }
  }
}
