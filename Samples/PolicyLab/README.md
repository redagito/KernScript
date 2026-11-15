# PolicyLab

PolicyLab prices order batches with KernScript policies. The host validates input, calculates money and writes reports; policies select discounts, shipping and tax.

## Run

Build `PolicyLab` with command-line samples enabled. With no arguments it prices `policies/orders.csv` using `policies/standard.ks` and writes CSV to stdout. Use `--input`, `--policy`, `--compare` and `--output` to select files, for example:

```text
PolicyLab --input policies/orders.csv --policy policies/standard.ks --compare policies/weekend.ks --output comparison.csv
```

The comparison includes candidate total, difference from baseline and explanation. Input columns are `id,tier,subtotal_cents,shipping_cents,tax_bps`. Amounts are nonnegative integer cents, up to 100,000,000 each. Tax basis points range from 0 to 10,000 (2000 = 20%). IDs are unique; IDs and tiers use 1–64 letters, digits, underscores or hyphens. Input is limited to 10,000 orders and 4 MiB. All orders and policies must succeed before output is written; file output uses atomic replacement and cannot overwrite input or selected root policy files.

## Policy API

Copy a policy with its `money.ks` helper. A policy exports `func price() -> void` and can call:

| Binding | Meaning |
| --- | --- |
| `subtotal_cents()`, `shipping_cents()` | Goods and proposed delivery amounts in cents |
| `tax_bps()` | Tax rate in basis points |
| `tier()` | Customer tier string |
| `rate_cents(amount, basis_points)` | Wide-integer product rounded half-up to a cent; rate 0–10,000 |
| `emit(discount, shipping, tax)` | Emit one quote; amounts in cents |
| `explain(text)` | Add a reason to the report |

Declare readers and `rate_cents` as `-> value`; `emit` and `explain` return `void`.

Final total is subtotal minus discount plus delivery plus tax. Discount cannot exceed subtotal; delivery is capped at 100,000,000 cents and tax at 200,000,000 cents. Each order has a 100,000-instruction budget and requires an explanation; at most 16 explanations of 1024 bytes each are accepted. The bundled weekend policy changes the free-delivery threshold. Copy it and change that threshold to compare a pricing rule with `--compare`.

`PolicyLab --verify --test-root <directory>` runs headless checks; CTest registers `PolicyLab.Verify`.
