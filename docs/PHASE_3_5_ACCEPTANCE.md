# Phases 3–5: implementation and acceptance

Updated 25 October 2026, Asia/Calcutta. Software implementation does not complete
the business exit gates in the planning document. No interviews, paid commitments,
production deployment, real provider payments, or successful commercial trial are
invented by this change.

## Phase 3: acquisition and repeat ordering

- Public stall pages show the actual available menu, current prices, opening
  hours, genuine owner-supplied photo URL, public business contact and service terms.
- Public invitations and a printable locally generated QR code carry a source
  tag. They never carry an office access token. Use a reachable HTTPS URL before
  printing; a localhost QR is only useful on the development computer.
- The owner confirms office identity/address and maps the browser request to the
  correct account. New office accounts still default to prepaid.
- First approved browser source is retained on the office, including after grant
  revocation. Reports distinguish browser requests from unique paying offices.
- Repeat ordering fills a fresh basket, clears unrelated quantities, and requires
  review and submission at current server prices and availability.
- Feedback is accepted once for the authenticated office's fulfilled order and
  appears in the owner's report. Feedback is not published as an invented rating.

External exit: measure an actual acquisition trial, costs, paying first orders,
repeat offices, service workload and contribution. Attribution alone does not
prove incremental sales.

## Phase 4: routine automation

- Offices explicitly consent to a basket, India-time schedule, days and maximum
  price. They can pause/resume and skip a date. Owners can close holidays and
  cap intake units requested per India day. Cancellation of an existing order is
  separate from pausing future rounds.
- A round is attempted within a fixed 15-minute window. Missed windows are
  skipped, not replayed as a backlog. Current access, office approval, service
  area, hours, menu availability, capacity, price ceiling, overdue state and atomic
  credit capacity are rechecked. A revoked originating browser grant stops future
  scheduled ordering. Prepaid automatic debits are not implemented: automatic
  rounds use expressly owner-approved credit only.
- A stable round/date request key prevents duplicate orders after worker crashes
  or competing ticks. Network calls run outside the selection database read.
  Business failures are recorded on the round; an uncertain failure can be retried
  with the same deterministic key, never a second new order intent.
- Credit accounting stays on the existing MONTHLY ledger policy internally;
  settlement cycle is a separate snapshotted term: AFTER_DELIVERY, DAILY, WEEKLY,
  or MONTHLY. Closed daily/weekly statements group existing unbilled charges.
  Weekly periods start on the explicitly selected date and are due seven days
  after period end. Daily statements are due one day after period end. Immediate
  after-delivery debt and overdue daily debt can block further credit before a
  statement is issued. These are account statements, not configured tax invoices.
- Billing reminders are stored in the office conversation at most once per rolling
  24 hours. Automatic overdue reminders persist retry status and use daily keys.
- Optional marketing has office-owned consent/unsubscribe, owner-approved exact
  welcome/reactivation text, a master pause, template pause, recipient cap and
  rolling seven-day cap (one to three). A welcome is sent once; reactivation uses
  actual fulfilled-order inactivity. All deliveries are in-app. No WhatsApp, SMS,
  email, advertising account, or external messaging spend is invoked.
- Workers are explicit operator switches. `scripts/dev.py` enables recurring
  rounds; reminder and marketing automation require their own opt-in settings.

External exit: verify actual phone/network behaviour, holiday/skip rules and
whether automation saves work without unwanted orders. The prototype has bounded
lists (100 discovery stalls, 100 recent access requests, 1,000 reporting/marketing
offices) and requires operational review before expanding beyond those limits.

## Phase 5: advance accounts, rewards, teams and reporting

- Advance entries are immutable. The owner records actual confirmed cash/bank
  top-ups or refunds with unique references. This action does not initiate a bank
  transfer or replace provider verification. Online top-up checkout and platform
  payouts are not provided.
- Order creation atomically reserves available advance funds. Fulfilment posts
  one deduction and balanced charge/payment entries; cancellation releases the
  reservation. Refunds cannot consume reserved funds or cash out referral rewards.
  The advance liability ledger is separate from outstanding credit charges.
- Advance money operations and monetary referral rewards require the operator to
  record `CHAY_FINANCIAL_REVIEW_ID` after actual provider/financial review. Merely
  setting a variable is not evidence that such review occurred. Automated tests
  use a synthetic review ID with disposable money records only.
- The owner confirms two distinct office identities and attributes a referral
  before first fulfilment. Self referrals, matching name/address/contact, duplicate
  attribution and repeated reward issuance are rejected. Rewards require paid
  fulfilment funded outside the advance/reward account, an enabled positive rule
  and remaining capped budget. Identity checks are basic safeguards, not a claim
  of comprehensive fraud prevention. Granted rewards are final promotional costs;
  later corrections to a qualifying receipt require operator reconciliation and
  are not automatically clawed back from funds already spent. Review that policy
  before enabling rewards.
- Loyalty points can be claimed once per paid fulfilled order, capped at 100 per
  order. They currently have zero cash value and no redemption promise.
- A named menu item can represent a snack combination or meeting pack. Offers
  enforce a configured time window, unit capacity, discount budget, supplied cost
  and margin floor, and advance notice. Editing the underlying menu invalidates
  the old offer until it is configured again. A new offer explicitly starts a new
  budget. This is not ingredient-level stock management or an automatic delivery
  guarantee; the stall still accepts each request.
- Separate branch workspaces own separate menus, office accounts and queues.
  Workers register/verify their own contact. The owner grants branch-specific
  MANAGER, FULFILMENT or ACCOUNTANT permissions. Only owners manage team/branch
  access. Membership is checked on every action, including Order-to-Merchant
  authorization, and revocation affects existing sessions.
- Reports show fulfilled sales, paid offices, repeat behaviour, refunds, feedback,
  source conversion, recorded acquisition spending, and a contribution estimate
  only when all fulfilled item costs were snapshotted. Historical unknown costs
  are not silently treated as zero. Actual expense references are deduplicated.
  Discounts already reduce sales and are not subtracted twice. Estimates exclude
  unrecorded expenses and taxes and are never labelled profit.

External exit: provider/financial review, reconciliation, release/security review,
real demand/retention, owner payments, support workload and a controlled pilot.
No live Razorpay keys, pooled collections, production readiness or real-user
commercial acceptance is claimed.

## Communication

See [COMMUNICATION.md](COMMUNICATION.md) for the office-to-stall workflow and why
structured orders and accounting remain separate from conversation messages.
