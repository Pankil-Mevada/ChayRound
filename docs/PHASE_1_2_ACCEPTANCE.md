# Phase 1/2 acceptance: engineering versus external validation

Product change: default office onboarding now uses stall discovery and owner-approved
remembered browser access instead of office email/PIN/private links. Owner email
verification is unchanged. See OFFICE_ACCESS.md; older office-verification/private-link
rows below describe the retained legacy flow. Real owner/device trials remain open.

Updated: 24 October 2026. No feature is marked commercially accepted from local
fixtures alone. The earlier roadmap includes actual-device testing and a real paid
pilot, which cannot be manufactured through code changes.

## Phase 1

| Requirement | Implemented and locally verified | External acceptance remaining |
| --- | --- | --- |
| Owner registration, verified contact and sessions | Email challenge/expiry/attempt limits, sign-in/logout, account scoping | Configure and test actual email delivery |
| Menu/prices, hours, availability and pause | Owner controls, version checks and enforced ordering rules | Owner trial on actual stall phone |
| Office contact/address/instructions | Email verification, owner-confirmed address and service-area label | Real office onboarding and delivery eligibility |
| Private links and access policy | Hashed/revocable links and optional PIN; no daily office password | Confirm shared-office access policy with pilot users |
| Seller acceptance/ready/fulfilment | Versioned transitions, visible queue, foreground refresh and opt-in sound | Actual-phone sound/background behaviour; no push guarantee |
| Server identity and money | Tenant-derived identity, integer paise, multi-item snapshots | Independent release/security review |
| Retry/isolation/connectivity | Durable idempotency, concurrent tests and fail-closed dependencies | Real network/device trial |
| Test payments and refunds | Razorpay adapter, signatures, canonical capture checks, idempotent refunds; isolated contract tests | Actual Razorpay Test checkout/refund/webhook cycle |

**Engineering result:** Phase 1 software scope implemented and local contract/browser
checks passed. Full phase acceptance remains open for real provider/device checks.

## Phase 2

| Requirement | Implemented and locally verified | External acceptance remaining |
| --- | --- | --- |
| Prepaid defaults; per-office monthly approval | Defaults, versioned policy/limits, snapshot preservation | Merchant-approved real terms |
| Credit safety and fallback | Atomic reservations, overdue/disabled-credit block, explicit prepay override | Real business collection policy |
| Ledger, bills and partial payments | Immutable entries/allocations, closed-month statements, due dates, FIFO receipts | Tax configuration if tax invoices are required |
| Traceable corrections/refunds | Credit/debit adjustments, receipt reversals, cancellation reasons, verified refund completion | Real provider refund reconciliation |
| Merchant account/settlement arrangement | Per-stall configured Test accounts; no platform pooling or payouts | Provider account onboarding/settlement agreement |
| Recovery/operations | Durable worker, bounded calls, retry leases, operations counts and runbook | Deployment monitoring/alerting and operator ownership |
| Backup/restore | Offline integrity/digest backup; restore to new directory; tested restart | Secure backup storage and scheduled restore drills |
| Controlled paid pilot | Software and runbook available | Enrol real stalls/offices, operate pilot, reconcile and collect app fee |

**Engineering result:** Phase 2 software workflows implemented and locally tested.
**The controlled paid pilot and commercial phase exit are not complete.**

## Evidence

- WSL Debug build for all services and test executable.
- CTest: 1/1 passed, including money/state/crypto/date/rollback checks.
- 252 HTTP/provider-contract/concurrency/recovery checks and 9 legacy migration/
  restart invariants passed with disposable DBs.
- Browser: synthetic owner/office verification, hours/menu setup, two-item Rs 50
  order, fulfilment, Rs 20 receipt and matching Rs 30 office balance; no console errors.
- Responsive viewport observation: content width matched viewport width at 375 px.
  This is browser breakpoint testing, not actual-device certification.
- Provider simulators are under tests/; normal startup does not use them. No real
  emails, payments or provider requests were made during automated verification.

## Next external step

Configure transactional email and per-stall Razorpay Test accounts as described
in OPERATIONS.md, then perform a real Test Mode checkout, capture, cancellation,
refund and webhook/recovery cycle. Record provider IDs, expected/actual amounts,
operator observations and outcome without committing secrets or customer data.
Then run a small user-approved pilot before declaring either phase fully accepted.
