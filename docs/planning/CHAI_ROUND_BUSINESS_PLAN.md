# Chai Round: concept and pilot budget

Prepared: 24 October 2026. Currency: Indian rupees (INR).

This is a proposed product and planning estimate, not an implemented feature,
supplier quotation, or validated business model. It is separate from the current
FoodService implementation and its release status.

## Product idea

Help local tea stalls serve nearby offices with minimal phone use by the owner.
Customers open a mobile website, select tea, and pay. The software groups orders
by office and preparation time. The owner's phone displays large controls and
speaks the preparation list in a supported local language.

Example: Five employees order separately. The seller receives one instruction:
"ABC office: three regular teas and two without sugar. Payment received."
Payment is described as received only after server-side verification.

The owner prepares the tea and taps Ready. Delivery uses the stall's existing
arrangement; this budget does not include a delivery fleet.

## Target pilot

- Three to five tea stalls and a few nearby offices.
- A mobile website opened through a link or QR code, rather than separate native apps.
- Existing smartphones and internet connections; no additional hardware.
- Customer ordering, menu and prices, office grouping, seller order screen,
  spoken instructions, verified payments, order history, and basic refund handling.
- Browser audio and background notification behaviour must be tested on the
  actual seller phones; unattended announcements cannot be assumed to work.

## Who could pay?

Offices and customers should initially order free. The most plausible payer is
the stall owner, provided the product brings profitable new business or reduces
unpaid bills enough to justify its price.

One offer to test is an agreed acquisition fee after a newly introduced office
completes a defined number of paid orders. Existing customers would remain free.
This requires acquiring office customers; it is not passive software income.

Do not assume that owners will pay for convenience alone. Subscription pricing,
acquisition fees, and willingness to pay remain unvalidated.

## Initial budget estimate

These are rough planning allowances, not developer quotations. Actual cost
depends on scope, reuse of suitable existing code, quality, and developer rates.
Do not assume the existing FoodService project is ready for this pilot.

| Expense | Build yourself | Hire a developer |
| --- | ---: | ---: |
| First working product | Your time plus Rs 0-10,000 in tools | Rs 60,000-1,50,000 |
| Hosting, database, backups: first three months | Rs 3,000-9,000 | Rs 3,000-9,000 |
| Domain, QR materials, stall visits | Rs 2,000-6,000 | Rs 2,000-6,000 |
| Fixes and contingency | Rs 10,000-15,000 | Rs 15,000-35,000 |
| Approximate total, rounded | Rs 15,000-40,000 | Rs 80,000-2,00,000 |

The self-build estimate excludes the value of your development time. Hired
development quotes may add taxes. Estimates exclude new phones, delivery,
substantial marketing, ongoing staff, business registration, and specialist
legal/accounting work. Obtain quotes for the agreed scope before committing.

## Ongoing costs

Allow approximately Rs 1,000-3,000 per month for a small pilot's infrastructure,
excluding maintenance labour, paid messaging, and payment processing. This is
an allowance, not a guaranteed hosting bill. SMS or WhatsApp automation would
need a separate usage-based budget.

Hosting promotions can require upfront payment and have higher renewal rates.
See [Hostinger's published VPS terms and prices](https://www.hostinger.com/in/vps-hosting).

Razorpay's published standard gateway price is 2% plus 18% GST on the fee:
approximately Rs 2.36 per Rs 100 collected. Pricing checked on 24 October 2026;
confirm the applicable merchant terms before launch.
See [Razorpay pricing](https://razorpay.com/pricing/).

For illustration, processing a Rs 15 payment at that rate costs approximately
Rs 0.35. That is a payment-provider charge, not revenue for Chai Round. Decide
who bears it before pricing the service. Aggregating orders does not remove a
percentage-based fee.

Merchant onboarding, settlement destinations, refunds, and any platform fees
need an agreed provider-supported arrangement. Do not assume a standard gateway
account automatically supports collecting and distributing money for many stalls.

## Automation boundaries

- Initial stall setup, menu configuration, onboarding, and unusual support cases
  still need effort.
- Software cannot observe walk-in cash sales or physical fulfilment without input.
- A Ready action records the seller's confirmation; it does not independently
  prove delivery.
- Failed payments, duplicate requests, cancellations, refunds, and network loss
  need reliable handling before real transactions.
- Automated order announcements depend on device/browser restrictions.

## Competition and unresolved risks

Tea-stall accounting and office-order management already exist. For example,
[ChaiHisab](https://play.google.com/store/apps/details?id=com.innocreek.chaiconnectvendor)
advertises order records, office payment tracking, revenue reporting, menus,
walk-in entries, and notifications.

Its listing alone does not establish whether it offers combined employee orders,
individual payment collection, or spoken preparation instructions. Test competing
products before claiming a gap. Chai Round is not established as a new invention.

The main business risks are unclear willingness to pay, small tea-order margins,
customer reluctance to use another ordering flow, onboarding effort, and support
costs. No customer interviews, paid commitments, or live pilot results are recorded
in this plan.

## Recommended next action

Spend no more than Rs 3,000-5,000 initially on a demonstration and visits to five
stalls and three offices. This is a separate validation-stage allowance; avoid
double-counting expenses reused in the later pilot.

1. Observe office-order interruptions, billing effort, and unpaid balances.
2. Show a simple customer order flow and seller preparation screen.
3. Ask owners what they would pay for a new office buying regularly.
4. Seek at least two concrete paid-pilot agreements with the fee and trigger written down.
5. If interest is sufficient, get a fixed-scope development quote and confirm
   payment-provider arrangements before funding the build.

The immediate decision is whether anyone will pay enough to cover acquisition,
processing, infrastructure, and support costs. Development should follow that
evidence.
