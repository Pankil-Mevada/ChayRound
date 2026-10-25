# Growth, communication and expansion: code and tradeoffs

- `services/merchant/communication.h`: server-scoped office conversations, read
  cursors, idempotent writes and public profiles. Text is rendered with DOM text
  nodes. A chat message never grants financial or ordering authority.
- `services/merchant/workspaces.h` and `merchant.h`: a session keeps both its real
  principal and selected workspace. A branch is a distinct stall identity;
  membership resolves allowed capability on every request. Order propagates its
  required capability over authenticated internal calls rather than trusting a
  browser role or branch ID. Revocation is checked even for existing sessions.
- `services/order/checkout.h`: shared manual/recurring order creation with the
  same price, capacity, idempotency and credit transaction. This avoids a weaker
  scheduler-only ordering path. Offer/cost/settlement terms are snapshots.
- `services/order/automation.h`: consent plus deterministic round/day key. A
  schedule stores the grant hash, not a raw browser credential. Merchant rechecks
  that grant before each run. A bounded window skips old work. A database read
  initially held across order creation caused a WAL snapshot upgrade failure;
  collecting candidates and closing the read before network/write work fixed it.
- `services/order/advance.h`: available advance equals immutable balance minus
  active order reservations. A write transaction serializes competing orders,
  refunds and deductions. Fulfilment records the wallet deduction and balanced
  accounting entries together. Cancellation has no earned charge to reverse.
- `services/order/rewards.h`: one reward per referred office and qualifying order,
  distinct identity checks and a transactionally enforced budget. Cash-out is
  constrained to cash-funded balance. Real fraud review remains an external duty.
- `services/merchant/campaigns.h`, `marketing_automation.h`, and Order workers:
  separate transactional billing from optional marketing consent. Recheck opt-out
  at delivery; use idempotent stored messages and shared frequency caps. In-app
  delivery avoids inventing an external provider or claiming real sends occurred.
- `services/order/growth.h` and `reporting.h`: service-owned reporting data; obtain
  acquisition facts through Merchant's internal API, never by joining its database.
  Unknown historical costs yield an unavailable estimate rather than fake profit.

SQLite remains a deliberate small-pilot choice. Larger scale needs measured load,
list pagination and worker fairness/operational review. No Redis, Kafka or
Kubernetes is needed to demonstrate these invariants. New immutable histories
are additive; backups must include both service-owned databases offline.

Evidence lives in `tests/e2e.py` (authorization, isolation, racing reservations,
reward/receipt deduplication, consent, schedule retries, offers and statements),
`tests/migration_test.py`, and `tests/browser_phase345.cjs`. These use synthetic
identities and disposable providers/databases, not commercial acceptance evidence.
