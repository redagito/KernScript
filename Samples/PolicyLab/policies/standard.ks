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
  var rate = 0;
  if (customer == "staff") {
    rate = 3000;
    explain("Staff discount: 30 percent");
  } else {
    if (customer == "vip") {
      if (subtotal >= 10000) {
        rate = 2000;
        explain("VIP order at least 100.00: 20 percent discount");
      } else {
        rate = 1000;
        explain("VIP order below 100.00: 10 percent discount");
      }
    } else {
      if (subtotal >= 10000) {
        rate = 500;
        explain("Standard order at least 100.00: 5 percent discount");
      } else {
        explain("Standard pricing, no discount");
      }
    }
  }
  var discount = money::apply_rate(subtotal, rate);
  var shipping = shipping_cents();
  var tax = money::apply_rate(subtotal - discount + shipping, tax_bps());
  explain("Tax applies to discounted goods plus delivery, rounded to cents");
  emit(discount, shipping, tax);
}
