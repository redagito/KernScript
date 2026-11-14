# PolicyLab

PolicyLab prices an order batch using external business rules. It produces
explained quotes and can compare a proposed policy with the current one on the
same orders. Money stays in integer cents. The host owns input validation,
rounding and report files; scripts choose discounts, delivery and taxable amounts.

## Run a quote batch

Build the `PolicyLab` target with command-line samples enabled through the
project's normal Conan/CMake setup. Running `PolicyLab` with no arguments prices
the bundled `policies/orders.csv` using `policies/standard.ks` and prints CSV.
From the executable directory:

```text
PolicyLab --input policies/orders.csv --policy policies/standard.ks --output quotes.csv
PolicyLab --input policies/orders.csv --policy policies/standard.ks --compare policies/weekend.ks --output comparison.csv
```

The comparison adds the candidate total, its difference from the baseline and
the candidate explanation. A negative difference means the proposed quote is
lower. The first bundled order is 12000 cents of goods plus 500 cents delivery
with a 20 percent tax rate. Standard pricing gives 14280 cents; weekend pricing
gives 13560 cents, a difference of -720 cents.

Edit a copy of `orders.csv` to price your own orders. The five input columns are:

```csv
id,tier,subtotal_cents,shipping_cents,tax_bps
ORD-1001,std,12000,500,2000
```

Order IDs are unique. IDs and tiers use 1 through 64 letters, digits, underscores
or hyphens, so input fields need no CSV quoting. `std`, `vip` and `staff` have
rules in the bundled policies; other tier names use standard pricing. Amounts
are nonnegative integer cents, capped at 100000000 per input field. `tax_bps`
uses basis points: 2000 means 20 percent, 750 means 7.5 percent. Rates must be
between 0 and 10000. The app accepts up to 10000 orders in a 4 MiB input file.
It reports invalid input with the file and line number.

All orders and both policies must succeed before output is written. File output
uses atomic replacement, and the input orders and selected root policy files
cannot be overwritten by the report. Failed runs return a nonzero exit code.
Omit `--output` for stdout, which also makes the report usable in a pipeline.

## Change the business policy

Copy a policy and keep its `money.ks` helper beside it. Each policy exposes
`func price() -> void` and calls these host bindings:

| Function | Meaning |
| --- | --- |
| `subtotal_cents()` | Goods total before discount |
| `shipping_cents()` | Proposed delivery charge |
| `tax_bps()` | Input tax rate in basis points |
| `tier()` | Customer tier string |
| `rate_cents(amount, basis_points)` | Wide-integer multiplication, rounded half up to a cent |
| `emit(discount, shipping, tax)` | Emit exactly one quote, all values in cents |
| `explain(text)` | Append an explanation of the decision |

The host derives the final total as subtotal minus discount plus delivery plus
tax. Discounts cannot exceed the subtotal, delivery is capped at 100000000 cents,
and tax at 200000000 cents. These bounds keep the total within the VM's integer
range. `rate_cents` accepts a base of up to 200000000 cents and a rate from 0 to
10000. It uses a wider intermediate integer, so multiplication does not lose
cents or overflow at ordinary policy limits. Each order has a 100000-instruction
budget and needs at least one explanation. Up to 16 explanations of 1024 bytes
each are joined in the report. Quoted explanation fields use standard CSV
escaping, including doubled quote characters.

The bundled standard policy retains the original sample's tier discounts. The
weekend policy applies a broader discount and free delivery on orders of at
least 15000 cents before discount. A useful customization is to test a free
delivery threshold against customer tiers. Change the threshold in a copied
policy and use `--compare` to inspect which orders change and why.

## A larger project

Export a day's orders into this schema and compare the current and proposed
campaign before changing production rules. Retain the comparison CSV with the
policy files as a reproducible pricing experiment. The verifier exercises a
2000-order batch spanning tier and threshold combinations, compares both
policies, and checks each baseline result against independent integer arithmetic.

`PolicyLab --verify --test-root build/test-tmp/PolicyLab` checks rounding,
thresholds, explanations, malformed inputs, failed policies preserving output,
report replacement and the larger comparison. CTest registers `PolicyLab.Verify`.
`PolicyLab --help` lists the command-line options.
