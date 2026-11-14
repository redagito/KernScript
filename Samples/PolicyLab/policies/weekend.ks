import money;
extern subtotal_cents() -> value;
extern shipping_cents() -> value;
extern tax_bps() -> value;
extern tier() -> value;
extern emit(discount, shipping, tax) -> void;
extern explain(text) -> void;

func price() -> void {
  var subtotal = subtotal_cents();
  var customer = tier();
  var rate = 1000;
  if (customer == "staff") {
    rate = 3000;
    explain("Staff retain the 30 percent discount");
  } else {
    if (customer == "vip") {
      rate = 2000;
      explain("Weekend VIP discount: 20 percent at any order size");
    } else {
      explain("Weekend standard discount: 10 percent");
    }
  }
  var discount = money::apply_rate(subtotal, rate);
  var shipping = shipping_cents();
  if (subtotal >= 15000) {
    shipping = 0;
    explain("Weekend free delivery for orders at least 150.00 before discount");
  }
  var tax = money::apply_rate(subtotal - discount + shipping, tax_bps());
  emit(discount, shipping, tax);
}
