# ChayRound implementation status

Updated: 24 October 2026 (Asia/Calcutta).

## Current milestone

Default office onboarding redesigned on 24 October 2026: customers find the stall,
request office access and receive one-time owner approval for that browser. No
office email verification, PIN or private link is required in this flow. Returning
on the same browser remembers access. Owner email verification remains required.

Phase 1 and Phase 2 engineering workflows implemented and locally validated.
Full phase acceptance is still pending actual-provider/device checks and the
controlled paid pilot. No production readiness, live money collection, merchant
settlement approval or commercial launch is claimed.

## Implemented

- Public stall-name/area discovery without public office/contact/balance exposure.
- Pending browser requests; owner maps each request to an office belonging to their
  stall, approves or revokes access with audit. Hash-only server token storage,
  localStorage return access, separate email_verified and owner_approved facts.
- Optional office email; simplified customer screens and owner access queue.
  Existing links/PIN APIs remain compatible, outside the default sharing flow.
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

Latest change verification: C++ build and CTest 1/1 passed; 290 disposable HTTP/
provider/concurrency/recovery checks, nine migration/restart checks and four
launcher tests passed. Node syntax checks and seven synthetic browser-flow UI
scenarios passed. Real browser on isolated fixture ports 9280-9282 verified stall
selection, no-email/no-PIN request, owner office selection/approval, a Rs 30 monthly
order and remembered access after reload. A browser-only revocation check stalled
at its confirmation dialog; backend revocation/account/order denial tests passed.
No real emails/payments or user credentials/data were used. An initial run caught
mixed Windows/UTF-8 page encoding (fixed); the expanded test also needed fresh
synthetic sessions after its intentional clock jump. Subsequent suites passed.

Next user action: restart the private launcher to load the new Merchant/Gateway,
then hard-refresh. Owner uses /#owner; a separate customer browser/profile opens /.
Follow docs/OFFICE_ACCESS.md. Existing offices can be selected when approving new
browser requests; no re-creation or automatic approval is necessary. Confirm
requesters outside the app before granting access to office orders/credit/history.
Prototype limits: first 100 stalls and recent requests, no grant expiry, shared
browser access, no employee identity, localStorage/XSS/HTTPS hardening outstanding.
Production/device/pilot acceptance remains open; owner verification still needs
configured email. Study: docs/interview/DEVICE_APPROVAL.md.

Earlier access-error diagnosis and launcher history (superseded default office flow):

Follow-up runtime diagnosis: public health checks passed on 9180-9182 and the
served HTML contained the new PIN explanation, but `/proc` executable metadata
showed Order still running the replaced `(deleted)` binary. This explains why
the new error diagnostics were not active; it does not establish the original
authorization failure. The outdated supervisor was stopped without reading its
environment, credentials, private links or DBs. User must privately restart with
scripts/private_email_start.py, then hard-refresh the office page and retry.
No code changed for this runtime restart; the passing suite below remains the
latest code validation.

Office-access diagnostics now preserve an allowlist of specific Merchant errors
through Order (wrong/missing PIN, revoked link, owner session/verification) without
forwarding arbitrary dependency bodies or relaxing authorization. The PIN screen
explains the optional owner-configured PIN. Fresh build passed on 24 October 2026:
CTest 1/1, 260 disposable HTTP/provider/concurrency checks, nine migration/restart
invariants and four launcher tests. Regressions verify no-PIN and correct-PIN
office onboarding before email verification, incorrect/missing PIN rejection and
invalid-link diagnostics. The user's historical denial is not diagnosed from its
generic screenshot. Next: user stops/restarts the private launcher and retries the
office link; use the specific error to correct the PIN or replace a revoked link.
The running private stack was not restarted or inspected, and no user credentials,
PINs, private links, logs or database contents were read during this fix.

Duplicate startup now exits with a clear Ctrl+C/retry instruction instead of a
lock traceback. Four synthetic launcher/lock tests passed on 24 October 2026;
the lock test uses a temporary directory and starts no services. The previous
local supervisor was stopped to allow private email startup; TCP checks confirmed
no listeners on 9180-9182. A plain immediate bind probe encountered address-in-use
after shutdown, so allow socket cleanup before retrying if needed. No credentials
or service logs were inspected. Backend code was unchanged; the earlier backend
suite results below were not rerun for this launcher error-message change.

Run scripts/private_email_start.py in your own interactive WSL terminal using
docs/PRIVATE_EMAIL_SETUP.md. The new launcher hides input, refuses piped input,
clears fixture overrides and saves no credentials. The supervisor passes email
configuration only to Merchant and the payment accounts path only to Order.
This is not isolation from the same OS user; no actual credentials were entered,
stored or inspected and real delivery remains untested. Validation for this
change: 3/3 synthetic launcher tests, CTest 1/1, 252 disposable HTTP/provider/
concurrency checks and 9 migration/restart invariants passed on 24 October 2026.

Follow docs/OPERATIONS.md to configure per-stall Test accounts, then
record real provider/device results against docs/PHASE_1_2_ACCEPTANCE.md. Run a
small approved paid pilot and reconcile its statements/payments before marking
the Phase 2 business exit complete. Do not replace these acceptance items with
fixture success or move to live money collection automatically.
