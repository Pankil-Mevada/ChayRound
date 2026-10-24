# Chai Round: tea-stall ordering and office accounts

Date: 24 October 2026
Status: Proposed requirements; not implemented or commercially validated.

## Product

A tea-stall owner registers and creates a dedicated ordering portal. The owner
adds offices as clients and decides each office's payment terms. Offices order
through a link without creating an account or entering a password each day.

This document refines the earlier concept in
[CHAI_ROUND_BUSINESS_PLAN.md](CHAI_ROUND_BUSINESS_PLAN.md). Its budgets are rough
earlier estimates, not quotations for the full scope described here. No changes
to the existing FoodService application or payment startup workflow are implied.

## Phase-wise delivery plan

All phases below are planned, not implemented. Complete the exit conditions of
each phase before funding the next. Phase 1 is a controlled prototype; Phase 2
completes the minimum commercial-pilot scope. Detailed requirements following
this plan remain applicable to their assigned phases.

| Phase | Outcome | Main scope |
| --- | --- | --- |
| 0 | Validate demand and payment willingness | Interviews, workflow demonstration, paid-pilot offer |
| 1 | Demonstrate core ordering safely | Owner setup, offices, private links, menu, seller queue, test payments |
| 2 | Run a controlled paid pilot | Prepaid/monthly credit, ledger, bills, refunds, operational readiness |
| 3 | Help stalls acquire and retain offices | Public pages, marketing materials, repeat orders, source tracking |
| 4 | Automate recurring work | Recurring rounds, billing reminders, additional settlement periods, opt-in campaigns |
| 5 | Expand after evidence of demand | Advance balances, referrals/loyalty, workers/branches, advanced reporting |

### Phase 0: validate before development

**Deliverables**

- Interview at least three stall owners and two office coordinators.
- Observe current orders, payment collection, delivery, and office credit.
- Demonstrate private links, prepaid orders, and monthly-account ordering.
- Agree who in an office may order, preferred terms, and acceptable link/PIN use.
- Test a specific subscription or paid-pilot offer with stall owners.
- Check competing products and obtain a fixed-scope development estimate.

**Exit condition:** Document the actual problems and seek at least two concrete
paid-pilot commitments. If owners will not pay, revisit the model before building.
Treat this as a proposed decision gate, not evidence already obtained.

**Spending:** Earlier validation allowance: Rs 3,000-5,000 for demonstrations,
visits, and basic materials. Owner/research time is additional.

### Phase 1: core ordering prototype

**Deliverables**

- Owner registration, verified contact, sign-in, and stall setup.
- Menu, prices, hours, stock/availability, and ordering pause controls.
- Office creation and verified contact, address, and delivery instructions.
- Private office links with revocation; choose PIN and access rules before use.
- Office ordering without daily password entry or customer app installation.
- Seller queue, acceptance, ready, and fulfilment controls.
- Server-derived office/stall identity and server-calculated integer-paise prices.
- Duplicate-request protection, stall isolation, and clear connection errors.
- Test-mode payment verification and cancellation/refund state demonstrations.
- Test the seller interface and optional audio on actual target phones.

**Exit condition:** Demonstrate the complete flow for two isolated stalls and
their offices. Check revoked links, forged prices/office IDs, duplicate requests,
failed payments, and interrupted connectivity. No real collections in this phase.

**Spending:** Obtain an estimate for this scope; no reliable phase-specific quote
exists. Existing FoodService test-payment startup steps must remain unchanged.

### Phase 2: credit, billing, and controlled paid pilot

**Deliverables**

- New offices default to prepaid; owner can approve monthly credit per office.
- Recorded payment terms, atomic credit limits, overdue-credit suspension, and
  prepaid fallback when credit is unavailable.
- Auditable ledger, monthly bills and due dates, partial-payment allocation,
  verified online payments, and owner-confirmed cash/bank payments.
- Traceable cancellations, refunds, and ledger corrections; no deletion of debt.
- Confirm merchant onboarding, settlement recipients, fees, and refund process
  with the payment provider before real transactions.
- Verify access control, backup/restore, order/payment recovery, monitoring, and
  a support process before the controlled pilot.
- Pilot with a small agreed set of stalls and offices using existing delivery.

**Exit condition:** Reconcile pilot orders, provider records, refunds, bills,
and balances. Resolve unexplained differences. Confirm owners can use the product
and obtain actual payment for the pilot service before broader expansion.

**Spending:** Earlier combined first-product planning ranges were Rs 15,000-40,000
for self-build cash costs or Rs 80,000-2,00,000 with hired development. They are not
validated quotations for Phases 1-2 or the full roadmap; re-estimate the agreed
scope. Infrastructure, processing, maintenance, and onboarding remain separate
cost categories as described in the business plan.

### Phase 3: basic business growth and marketing

**Deliverables**

- Public stall page with actual menu, photos, hours, area, and service terms.
- Office enquiry flow; owner approves service eligibility and any credit.
- Shareable public invitation, stall card, QR poster, and introduction message.
- Repeat-last-order with current-price and availability checks.
- Basic invitation/source tracking and a simple enquiry-to-paid-order report.
- Completed-order feedback flow and owner visibility into service issues.

**Exit condition:** Run a small acquisition trial and measure first paid orders,
repeat offices, acquisition spending, and service workload. Do not count page
views or downloads as proof of profitable growth.

**Spending:** Sharing links can be free; visits, printing, onboarding, discounts,
and optional ads require time or money. Marketing features also cost development
effort. Paid ads are optional and not included in infrastructure estimates.

### Phase 4: recurring orders and routine automation

**Deliverables**

- Explicitly enabled recurring office rounds with cutoff, holiday, skip, pause,
  cancellation, capacity, and payment rules; never infer employee attendance.
- Pay-after-delivery, daily, and weekly settlement presets using the same ledger.
- Billing reminders through supported providers and retry/status handling.
- Owner-approved marketing templates and configured welcome/reactivation offers.
- Marketing consent, unsubscribe, frequency caps, budgets, and a master pause.

**Exit condition:** Check duplicate scheduling, closed days, insufficient credit,
price changes, failed sends, opt-outs, and paused orders. Confirm automation saves
work without creating unwanted orders or spending beyond configured limits.

**Spending:** Messaging/provider charges and discounts are additional. Initial
outreach, fulfilment, and exceptions still need people; automation is not a
guarantee of new customers.

### Phase 5: advanced features and expansion

**Deliverables**

- Advance balances only after provider/financial review and a complete design
  for reservations, deductions, reversals, refunds, and reconciliation.
- Referral rewards after qualifying paid fulfilment, with abuse checks and caps.
- Loyalty, time-window offers, snack combinations, and meeting packs with
  availability, capacity, and margin controls.
- Multiple workers, role permissions, and multi-branch management.
- Advanced retention, campaign, contribution, and operations reporting.

**Exit condition:** Expand only when retention, owner payments, support workload,
service reliability, and contribution justify it. Review every new financial or
access-control feature before enabling it in the pilot.

**Spending:** Estimate separately based on measured demand. Reward budgets,
campaign spend, messaging, and additional operational support are not automatic
app income and are not included in the original build estimate.

## Detailed requirements by phase

The following sections specify the behaviour referenced above. The delivery plan
is authoritative for sequencing; these sections do not imply all features belong
in the first release.

## Users and access (Phases 1-2)

### Tea-stall owner

- Register, verify contact details, and sign into a private seller workspace.
- Configure stall name, menu, prices, hours, order cutoffs, and service area.
- Add office name, address, verified contact, and delivery instructions.
- Choose payment terms and credit limits separately for each office.
- Share, revoke, or regenerate an office's private ordering link.
- Accept orders, mark preparation/ready status, and record fulfilment.
- Review office balances, bills, payments, cancellations, and adjustments.
- Pause ordering when closed, overloaded, or out of stock.

### Office client

- Open the stall's office-specific link from a bookmark or WhatsApp.
- View the menu, prices, payment terms, and outstanding balance for that office.
- Select tea quantities and supported preferences, then place an order.
- Pay when required and view order status and office bills.
- No daily account creation or password entry is required.

## Office links without daily login (Phase 1)

Each office receives a private, hard-to-guess link scoped to one office and stall.
The server resolves the office identity and payment policy from that access;
browser-submitted office IDs or payment terms must not grant authority.

Verify the office contact during onboarding. Support revocation and regeneration
of links. A shared link grants access to anyone holding it, so the owner may
require a PIN for credit orders or sensitive billing views. Do not expose office
accounts on public searchable pages. Public menu access must not reveal private
contacts, addresses, balances, or order history.

Exact link lifetime, recovery, optional PIN rules, and permissions for individual
employees versus the office coordinator must be decided before implementation.

## Payment controls (Phases 2, 4, and 5)

Phase 2 delivers prepaid and monthly credit. Phase 4 adds pay-after-delivery,
daily, and weekly presets. Phase 5 considers advance balances.

| Owner-selected term | Proposed behaviour |
| --- | --- |
| Prepaid | Verified payment is required before preparation. |
| Pay after delivery | Fulfilled orders create an outstanding amount for later settlement. |
| Daily settlement | Unpaid fulfilled orders appear on a daily bill. |
| Weekly settlement | Unpaid fulfilled orders appear on a weekly bill. |
| Monthly account | Unpaid fulfilled orders appear on a monthly bill with an agreed due date. |
| Advance balance, later phase | Verified top-ups fund a balance used for fulfilled orders; reservation and reversal rules need separate design. |

New offices default to prepaid. The owner explicitly approves later payment or
monthly credit. For example, ABC Office may have monthly terms with a Rs 2,000
credit limit while XYZ Office remains prepaid.

Policy changes affect future orders. Preserve the terms and prices recorded on
existing orders. Term changes must not rewrite historical debts or payments.

### Credit limits

- Count outstanding debt and accepted unpaid commitments when checking credit.
- Enforce limits atomically so simultaneous orders cannot exceed available credit.
- When credit is insufficient, offer verified prepayment rather than silently
  extending credit.
- Allow owners to suspend further credit for overdue offices while retaining
  access to bills and payment.
- Record policy changes and any explicit overrides with an audit trail.

### Payment and ledger integrity

- Calculate totals on the server using current menu prices and integer paise.
- Verify gateway payments on the server; a screenshot or browser success message
  is not proof of payment.
- Distinguish unpaid, payment pending, paid, refund pending, and refunded states.
- Prevent duplicate orders, duplicate payment credits, and duplicate billing.
- A bill groups existing charges; generating it must not charge the office twice.
- Allocate partial payments to charges and show the remaining balance clearly.
- Record cash and bank-transfer payments with owner confirmation unless a
  connected provider verifies them. Preserve actor, timestamp, and reference.
- Correct ledger errors through traceable adjustments rather than deleting history.
- Define cancellation/refund rules, including paid orders rejected by the stall,
  before accepting real payments.
- Software can calculate debts and send reminders; it cannot guarantee collection.

Use provider-supported merchant onboarding and settlement arrangements. Do not
assume one standard payment account supports collecting and distributing funds
for unrelated stalls. Commercial payment integration remains a separate decision.

## Ordering workflow (Phases 1-2)

1. Owner creates ABC Office, verifies its contact, and selects monthly payment.
2. Owner shares ABC Office's private link.
3. Office selects 12 regular teas and three without sugar.
4. Server checks access, menu availability, hours, prices, and credit capacity.
5. Order appears in the seller workspace as awaiting acceptance.
6. Owner accepts; customer sees confirmation and the recorded payment terms.
7. Owner prepares the tea and marks it ready.
8. Fulfilment is recorded through an agreed handover/delivery confirmation.
9. Fulfilled unpaid items enter the office's billable ledger.
10. At the billing boundary, software generates the office bill and, if enabled,
    sends a reminder. Verified or confirmed payments reduce the balance.

For a prepaid office, payment must be verified before preparation. The UI must
distinguish payment success from seller acceptance. Rejection after payment
requires a traceable refund process, not silent order deletion.

The stall uses its existing delivery or pickup arrangement. This product does
not include a delivery fleet. A seller's Ready button alone is not proof of delivery.

## Seller interface (Phase 1, validated before Phase 2)

- Large buttons, minimal typing, and a simple order queue.
- Clear office name, quantities, preferences, and payment status.
- Optional local-language spoken instructions.
- Visible connection status and recovery after interrupted connectivity.
- Seller app or browser interface; choose after testing real stall phones.

Customers can use the website without installing an app. Seller background
notifications and audio require device testing; reliable unattended browser sound
must not be promised. Initial setup, fulfilment, cash confirmation, and exceptional
support still require human input.

## New office discovery (Phase 3)

A public stall page may allow a new office to request access. Verify its contact
and delivery eligibility. Until credit is explicitly approved, only prepaid terms
apply. Rate-limit requests and orders to reduce misuse.

## Minimum commercial-pilot scope (Phases 1-2)

- Owner registration and private seller workspace.
- Stall menu and availability controls.
- Office creation, contact verification, and revocable private links.
- Office ordering without daily password login.
- Prepaid and monthly-credit policies, with per-office credit limits.
- Order acceptance, ready and fulfilment states.
- Server-verified payments through an agreed provider arrangement.
- Office ledger, monthly bill, payment recording, and traceable adjustments.
- Basic cancellation/refund workflow and audit records.
- Strong separation of each stall's offices, orders, and finances.

## Later-phase scope summary (Phases 3-5)

- Pay-after-delivery, daily and weekly billing presets.
- Advance balances after a separate financial/provider review and ledger design.
- Optional recurring office orders, with explicit consent, skip/pause controls,
  and clear payment rules. Ordinary portal ordering still requires an office
  representative to open the link and submit an order.
- Configurable reminders through actual messaging providers.
- Advanced reports, multiple workers, and multi-branch management.

## How the app could bring more business (Phases 3-5)

These are proposed growth features, not promises of increased sales. The app
needs an initial way to reach offices; publishing a portal alone does not bring
customers. Start within the owner's actual delivery capacity and service area.

| Growth opportunity | Proposed app support | Expected benefit to test |
| --- | --- | --- |
| Reach nearby offices | Public stall page with menu, real photos, hours, service area, delivery minimum, and enquiry button | More qualified office enquiries |
| Convert walk-in relationships | Owner-generated QR poster and shareable invitation link for customers who work nearby | Turn existing awareness into office accounts |
| Encourage introductions | Office referral link with a reward after a referred office completes qualifying paid orders | Acquire offices through current clients |
| Make repeat buying easier | Repeat-last-order button and optional recurring tea rounds | More retained, regularly ordering offices |
| Increase order value | Relevant tea-and-snack combinations and meeting-size packs | More contribution per fulfilled order |
| Fill quieter periods | Owner-controlled offers for selected time windows | Additional demand when capacity is available |
| Recover inactive clients | Opt-in messages to offices that have stopped ordering | Reactivate customers without repeated manual follow-up |
| Build trust | Clear fulfilment expectations and a feedback link after completed orders | Find service problems and support repeat business |

### Owner's marketing workspace

Keep marketing simple: an owner chooses an action such as Invite an office,
Create an offer, or Share my menu. Generate local-language text and a preview
using actual menu prices, hours, and service terms. The owner can share it through
their usual channels. Do not invent ratings, customer counts, savings, or delivery
guarantees.

Useful materials include:

- A printable QR poster pointing to the public menu or office enquiry page.
- A shareable stall card with name, genuine photo, area, menu link, and contact.
- An office introduction message explaining order cutoffs and payment options.
- A time-limited trial offer with a minimum order and a capped discount budget.
- A meeting/bulk-order menu with capacity limits and advance-order requirements.

Public marketing links must be separate from private office ordering links.
Sharing a poster or referral link must never expose another office's credit
account, contact details, balance, or ordering authority.

### Example acquisition journey

1. An existing customer shares the stall's public invitation link with their
   office coordinator.
2. The coordinator sees the menu and submits an office-service request.
3. The owner confirms address, capacity, delivery terms, and contact identity.
4. The new office receives its own private link and starts prepaid.
5. After a successful trial, the owner may approve monthly billing and the
   office may opt into recurring rounds.
6. Any referral reward is issued only after the configured paid-fulfilment
   conditions are met, with duplicate/self-referral checks and spending caps.

The software can automate routing, eligible offers, and follow-ups. Initial
introductions, service approval, preparation, delivery, and complaints still need
people. It must not automatically enrol an office into credit or recurring orders.

### Optional automated marketing, later phase

With the owner's configured rules and recipients' marketing consent, software
could send welcome messages, eligible referral rewards, and limited reactivation
offers through supported messaging providers. Include an unsubscribe option,
frequency limits, a campaign budget, and a master pause control. Keep marketing
consent separate from necessary order/billing notifications. Do not scrape phone
numbers or send unsolicited bulk messages. Provider costs are additional.

Avoid unlimited discounts and automatic credit increases. Before enabling an
offer, show its cost and check the owner's configured margin assumptions. Without
ingredient, packaging, and delivery cost information, report sales and discounts;
do not label the result profit.

### Measure whether marketing works

Show the owner a small report:

- New office enquiries and first fulfilled paid orders by invitation/campaign.
- Acquisition cost: campaign spending and rewards divided by newly acquired
  paying offices; show no result when there are none.
- Repeat offices: acquired offices placing another fulfilled order within a
  stated period, such as 30 days.
- Fulfilled sales, discounts, refunds, payment fees, and overdue credit separately.
- Contribution estimate only when costs are available: sales less refunds,
  discounts, ingredients, packaging, delivery, payment fees, and attributable
  marketing costs, using clearly defined non-overlapping amounts.
- Late/rejected orders and complaints, so growth does not hide service problems.

Campaign-linked orders are not proof that all of those sales were additional.
Compare against the stall's baseline and, where feasible, a similar period or
group without the offer. The goal is profitable repeat business, not app downloads
or discount-driven order counts.

### Marketing scope and pilot

Phase 3: public stall page, shareable invitation/QR materials, office enquiry
capture, repeat ordering, and basic source tracking. These are separate from the
Phases 1-2 commercial-pilot scope and need their own development estimate.

Phases 4-5: referral rewards, scheduled campaigns, recurring rounds, loyalty
offers, and advanced campaign reporting, allocated as shown in the delivery plan.
Paid advertising spend is optional and is not included in the earlier
infrastructure estimate.

Pilot with one stall and a small set of nearby offices. Record the starting
sales and service workload, test one acquisition offer at a time, and review paid
repeat orders and contribution before expanding. Ask whether the owner will pay
for the observed outcome rather than assuming marketing features justify a fee.

## Business model hypothesis

Offices order free. Test a fixed monthly subscription paid by the tea-stall owner
for office ordering, credit control, and billing. Pricing and willingness to pay
are not validated. App downloads alone do not create revenue.

Differentiate on ease of use and useful business outcomes. Existing tea-ordering
and accounting products mean this is not established as a new invention.

## Validation and next action

No implementation, customer interviews, paid commitments, or pilot results are
claimed by this document.

Next: show the proposed workflow to three tea-stall owners and two office
coordinators. Confirm their current ordering and payment process, willingness to
use private links, preferred settlement terms, tolerance for a PIN, delivery
arrangement, and willingness to pay. Use that evidence to settle the first-version
scope and obtain an updated estimate before implementation.
