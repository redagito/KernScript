// A local customer-support flow for scenario authors to adapt.
import common;
extern input() -> value;
extern has_word(word) -> value;
extern state(name) -> value;
extern set_state(name, value) -> void;
extern scenario(id) -> void;
extern reply(rule, text) -> void;

func respond() -> void {
  scenario("shop-support-v1");
  if (input() == "") {
    reply("welcome", "Welcome to the shop support flow. Ask about delivery, returns or opening hours. Type help for topics.");
    return;
  }
  if (has_word("help") == 1) {
    reply("help", "Topics: delivery, returns, hours. After delivery, say late or arrived. After returns, say damaged or unwanted. Type reset to clear this flow.");
    return;
  }
  if (has_word("reset") == 1) {
    set_state("topic", 0);
    set_state("followup", 0);
    reply("reset", "The current support flow has been cleared. What would you like help with?");
    return;
  }
  if (has_word("hours") == 1) {
    reply("hours", "The sample shop is open Monday through Friday, 09:00 to 17:00. You can prepare a delivery or return question here at any time.");
    return;
  }
  if (has_word("delivery") == 1 or has_word("shipping") == 1) {
    set_state("topic", 1);
    reply("delivery.start", "Is your parcel late, or has it arrived? Reply late or arrived.");
    return;
  }
  if (has_word("returns") == 1 or has_word("return") == 1) {
    set_state("topic", 2);
    reply("returns.start", "Is the item damaged or unwanted? Reply damaged or unwanted to prepare the right question for the shop.");
    return;
  }
  if (state("topic") == 1) {
    if (has_word("late") == 1) {
      set_state("followup", 1);
      reply("delivery.late", "Prepare your order number, expected arrival date and the last tracking update. Send these to the shop so it can check the carrier. This flow has marked a delivery follow-up.");
      return;
    }
    if (has_word("arrived") == 1) {
      set_state("topic", 0);
      set_state("followup", 0);
      reply("delivery.arrived", "Good, the delivery question is resolved. Ask about returns if there is a problem with an item.");
      return;
    }
  }
  if (state("topic") == 2) {
    if (has_word("damaged") == 1) {
      set_state("followup", 2);
      reply("returns.damaged", "Prepare the order number, a description of the damage and photos of the item and packaging. Ask the shop which return method to use. This flow has marked a damage follow-up.");
      return;
    }
    if (has_word("unwanted") == 1) {
      set_state("followup", 3);
      reply("returns.unwanted", "Check the shop's return terms for this order, keep the item and accessories together, and ask for return instructions. This flow has marked an unwanted-item follow-up.");
      return;
    }
  }
  reply("unmatched", common::unknown());
}
