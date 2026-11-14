module money;
extern rate_cents(amount, basis_points) -> value;

// The host primitive multiplies with a wide integer and rounds half up.
// Business rules choose the rate and which cent amounts form the tax base.
export func apply_rate(amount, basis_points) -> value {
  return rate_cents(amount, basis_points);
}
