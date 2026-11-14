module math_helpers;

// Examples of small helpers available through :import math_helpers.
export func percent(amount, rate) -> value {
  return amount * rate / 100.0;
}

export func compound(principal, rate, periods) -> value {
  var balance = principal;
  var i = 0;
  while (i < periods) {
    balance = balance * (1.0 + rate / 100.0);
    i = i + 1;
  }
  return balance;
}
