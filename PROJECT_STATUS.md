# ChayRound implementation status

Updated: 24 October 2026 (Asia/Calcutta).

## Current milestone

Phase 1 and Phase 2 engineering workflows implemented and locally validated.
Full phase acceptance is still pending actual-provider/device checks and the
controlled paid pilot. No production readiness, live money collection, merchant
settlement approval or commercial launch is claimed.

## Implemented

- Independent C++20/Crow Gateway, Merchant and Order services with service-owned
  SQLite databases; original FoodService code/startup/data is unchanged.
- Owner accounts/sessions; Resend email challenges with expiry, attempt/request
  limits, keyed hashes and subject/email binding; optional hashed office PINs.
- Office address, area and delivery instructions, link rotation, verified contact,
  service/credit enable controls and versioned policy/limit edits with audit.
- Menu price/name/availability edits, opening days/hours and ordering pause.
- Multi-item basket snapshots, integer-paise server pricing, durable semantic
  idempotency, tenant isolation and keyset order/ledger pagination.
- Monthly reservations plus unpaid fulfilled charges under atomic credit checks;
  overdue/disabled-credit blocking and explicit prepaid fallback.
- Append-only ledger and allocations; confirmed cash/bank receipts, partial FIFO
  allocation, unique references, idempotency, receipt reversal and debit/credit
  adjustments with reasons. Financial/audit update/delete triggers preserve history.
- Unique closed-month India-time account statements and due dates, referencing
  existing charges without generating duplicate debt. These are not configured
  GST tax invoices. Owner issues statements through the portal.
- Per-stall Razorpay Test account adapter; durable provider setup intent, receipt
  recovery after lost replies, stored-order HMAC plus canonical captured-payment
  checks, webhook deduplication, full idempotent refunds and late-capture protection.
- Durable C++ reconciliation worker with persistent schedule/attempt/lease,
  capped backoff and fenced schedule updates; provider I/O outside write locks.
- Owner and office UI exposing these workflows; foreground queue polling and
  opt-in sound. No customer installation or daily account password is required.
- Additive schema migration, one-time legacy charge import, offline backup,
  integrity/digest verification, non-destructive restore and operations counts/runbook.
- Updated API/architecture/product docs and interview study on accounting,
  compensation, provider adapters, resource lifetime and recovery patterns.

## Actual verification

- Final `BUILD_JOBS=2 bash scripts/build.sh` passed using WSL Debug/GCC 13.3.
- CTest: 1/1 passed, including money, transitions, token/hash, HMAC, calendar-month
  boundaries/leap year and rollback checks.
- 252 HTTP/provider-contract/concurrency/recovery checks passed with ephemeral
  ports, controlled isolated clock and disposable databases.
- 9 legacy schema/migration/restart invariants passed; old data preserved and
  fulfilled charges imported exactly once.
- Tests include credit races, concurrent replay, snapshots, tenant/PIN/contact
  access, changed-email code rejection, hours/availability, partial allocation,
  receipt reversal, adjustments, statement totals/overdue blocks, lost setup/refund
  replies, webhook replay, automatic capture/refund recovery, missing configuration,
  live-key rejection, pagination, restart and backup/restore.
- Node JavaScript syntax and Python compilation checks passed.
- Browser QA against a separate disposable fixture stack passed owner/office email
  verification, settings/menu, multi-item Rs 50 request, fulfilment, Rs 20 receipt,
  and matching Rs 30 balances/partial settlement. Inspected browser error log empty.
- Browser responsive check observed content width equal to viewport width at 375 px;
  this is not certification on an actual phone or background-notification testing.
- Automated tests made no external email/payment/provider calls. Real credentials
  were not supplied; no real provider checkout/refund/webhook execution occurred.

The first expanded suite exposed a borrowed Crow JSON subtree lifetime bug during
receipt recovery. Giving the recovered provider order its own parse root fixed it
before the passing runs. Financial/worker changes were subsequently rebuilt and
the full final pipeline passed.

## Local preview and data

Normal preview uses localhost:9180 with loopback services and original local data.
Before upgrade, the previous supervisor was stopped and its databases backed up
to ignored `.run/backups/pre-phase12-20261024`. Migration preserves legacy records,
requires contact verification and defaults legacy stalls to paused. It does not
fabricate verified contacts or payment success. Normal startup uses no fixtures.
Builds, DBs, backups, logs and local secrets remain ignored.

## Unresolved acceptance and risks

- Configure real transactional email and each stall's Razorpay Test account locally;
  perform actual email, checkout/capture, cancellation/refund and webhook recovery.
- Provider merchant onboarding/settlement ownership and an HTTPS webhook endpoint
  need operator configuration. Live keys are deliberately rejected.
- Actual stall-phone foreground/audio/background and real delivery trials remain.
- Controlled paid pilot, account reconciliation and collection of the app's fee
  have not occurred. Customer willingness to pay is unvalidated.
- No unit-level inventory, GPS geofence, individual employee accounts, recurring
  rounds, marketing, other settlement schedules, advance balances or payouts.
- No account recovery/MFA, production session/HTTPS hardening, external security
  review, sustained load test, encrypted scheduled backups or production deployment.
- Single-writer SQLite and synchronous Merchant lookup remain explicit tradeoffs;
  authorization/menu/policy edits have lookup-time snapshot semantics.
- Recovery uncertainty that provider receipt lookup cannot resolve needs operator
  investigation; the app never blindly creates a second provider order.
- Provider refunds created manually outside this app require an operator review;
  only the implemented full cancellation-refund workflow is locally validated.

## Precise next action

Follow docs/OPERATIONS.md to configure email and per-stall Test accounts, then
record real provider/device results against docs/PHASE_1_2_ACCEPTANCE.md. Run a
small approved paid pilot and reconcile its statements/payments before marking
the Phase 2 business exit complete. Do not replace these acceptance items with
fixture success or move to live money collection automatically.
