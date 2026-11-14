// Storybook world: shared titles, descriptions and omens.
//
// Imports cast, so with chapter > cast + chapter > world > cast every
// chapter compile is a genuine diamond: cast loads once and is shared by
// the chapter and by world. All funcs are valued (return expr only).
module world;
import cast;
export func title1() -> value {
  return "Chapter 1: The Ember Gate";
}
export func title2() -> value {
  return "Chapter 2: The Hollow Road";
}
export func describe(id) -> value {
  if (id == 1) {
    return "Mara studies the seal, chalk on her fingers, worry in her eyes.";
  }
  if (id == 2) {
    return "Bram leans on his hammer, arms crossed, lantern light on his beard.";
  }
  if (id == 3) {
    return "Wren walks light, map case over one shoulder, herbs in her pockets.";
  }
  return "The road waits, patient as stone, for someone to walk it.";
}
export func omen() -> value {
  return "When the hollow wind stops, the road is listening. Walk softly.";
}
export func ending1a() -> value {
  return "Ending 1A: Dawn of the Ember Gate";
}
export func ending1b() -> value {
  return "Ending 1B: Ashes on the Wind";
}
export func ending2a() -> value {
  return "Ending 2A: The Road Remembers";
}
export func ending2b() -> value {
  return "Ending 2B: The Hollow Keeps Its Own";
}
