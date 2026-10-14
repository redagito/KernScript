// ScriptNovel cast: shared actor table for every chapter.
//
// Both chapters import cast (and world, which imports cast too), so each
// compile loads cast once and shares it: the diamond root > chapter >
// cast/world > cast. All funcs are valued (return expr, no bare return).
// String law: names flow out as values; chapters compare them with ==/!=.
module cast;
export func name(id) {
  if (id == 1) {
    return "Mara the Mage";
  }
  if (id == 2) {
    return "Bram the Blacksmith";
  }
  if (id == 3) {
    return "Wren the Wayfinder";
  }
  return "Narrator";
}
export func title(id) {
  if (id == 1) {
    return "Keeper of the Ember Seal";
  }
  if (id == 2) {
    return "Smith of the Gate Forge";
  }
  if (id == 3) {
    return "Guide of the Hollow Road";
  }
  return "Voice of the Story";
}
