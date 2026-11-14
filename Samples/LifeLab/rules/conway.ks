extern cell_alive() -> value;
extern neighbors() -> value;
extern emit(next) -> void;

func decide() -> void {
  var alive = cell_alive();
  var count = neighbors();
  if (alive == 1) {
    if (count == 2 or count == 3) { emit(1); } else { emit(0); }
  } else {
    if (count == 3) { emit(1); } else { emit(0); }
  }
}
