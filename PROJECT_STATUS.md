# ChayRound implementation status

Updated: 24 October 2026 (Asia/Calcutta).

## Current milestone

Phase 1 local prototype: build a working office-to-stall monthly-credit ordering
slice and explain its implementation for system-design interview practice.
No commercial launch, live payment collection, or deployment is approved by
these development results.

## Implemented in source

- Three C++20/Crow executables: Gateway, Merchant and Order; separate
  Merchant/Order SQLite databases. Gateway provides the same-origin web entry.
- Owner registration/sign-in/logout with salted PBKDF2 passwords, random sessions,
  hashed token storage and session expiry. Contact ownership is not verified.
- Owner-scoped menu and office creation; prepaid/monthly policy at creation;
  revocable private office links. The raw office token is shown once.
- Browser owner desk and office portal, accessible without customer installation
  or daily password entry. Manual refresh, accessible labels and status messages.
- Single-item monthly-credit requests using server-derived identities/prices,
  durable idempotency, bounded integer-paise arithmetic, and atomic credit checks.
- Versioned PLACED -> ACCEPTED -> READY -> FULFILLED transitions, eligible
  cancellation and transactionally recorded state-change events.
- Internal shared-key authentication, bounded dependency calls, own-DB health
  queries, loopback binding and a local supervisor with cleanup.
- CTest domain checks and disposable-database HTTP tests.
- Separate product, architecture, API and interview study documents. Prior
  planning documents are copied under docs/planning/ without implying completion.

## Verification

- WSL Debug CMake build passed for Merchant, Order, Gateway and domain_test.
- CTest: 1/1 passed (money bounds, state transitions, randomness/digest smoke
  checks and transaction rollback).
- HTTP integration: 32 checks passed, using ephemeral ports and disposable DBs.
  Coverage includes credentials, internal access, tenant isolation, forged totals,
  foreign menu items, replay conflicts, simultaneous credit spending, concurrent
  same-key retries, cancelled-credit release, versioned transitions, prepaid
  rejection, token rotation/hashed storage, restart persistence and dependency loss.
- Frontend JavaScript syntax check passed using Node.
- Browser smoke passed: synthetic owner registration/login, menu creation, office
  creation, private portal without customer login, Rs 45 request, seller acceptance
  through fulfilment, office refresh and reload. Inspected browser error log was empty.
- Local preview is running at http://localhost:9180 with three loopback services,
  launched by scripts/dev.py. Only synthetic browser-test data is in ignored .run/.
- No real payments, external provider calls, deployment, sustained load test or
  mobile-device/background-notification certification was performed.

The first HTTP integration run exposed an incorrect libcurl response-buffer
pointer. It was corrected before the successful suite and browser run. Final
source formatting is non-functional. Build output/logs and local DBs are ignored.
GitHub origin is https://github.com/Pankil-Mevada/ChayRound.git. The initial source,
tests and documentation snapshot is prepared for publication on main. Build output,
databases, logs and runtime secrets are excluded. Publication is separate from
deployment and does not change the prototype's release limitations.

## Known limitations

- Prepaid orders are blocked. No provider integration, settlement entry,
  invoices, reconciliation, refunds, wallet, reminders or recurring orders.
- Credit is reserved for PLACED requests; fulfilled orders remain unpaid.
- No office address/contact verification, optional PIN, policy editing, menu
  editing/availability/hours, multi-item basket, or service-area enforcement.
- No automatic background alerts, push/SSE updates or marketing features.
- Current lists cap at 200 without pagination; not account statements.
- No login throttling, production HTTPS/session hardening, recovery, versioned
  migrations, backup restore drill, sustained load or independent security review.
- Merchant lookup is synchronous; orders fail closed during Merchant outages.
  Authorization/policy is a lookup-time snapshot, not a cross-service transaction.
- SQLite is a single-writer local-file choice; production multi-node architecture
  requires further work. No broker, outbox publisher or circuit breaker exists.
- Local dependency configuration currently reuses installed libraries from the
  sibling FoodService tree; no code or runtime data is modified there.

## Precise next action

Implement verified office onboarding (contact and address), owner policy editing
with audit/version rules, and menu availability/hours enforcement. Then add a
multi-item basket and cursor pagination before starting Phase 2's ledger,
invoices and provider-backed payments. Keep prepaid ordering blocked until that
payment workflow is verified. Business willingness-to-pay remains unvalidated.
