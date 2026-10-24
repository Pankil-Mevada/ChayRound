# Phase 1/2 operations and provider configuration

## What is ready to test

The application implements verified-email onboarding, office access, ordering,
monthly credit, append-only account entries, statements, receipt allocation and
reversal, Razorpay Test Mode checkout/capture/refunds, and durable reconciliation.
Automated provider fixtures exercise contracts without sending mail or money.
They do not establish successful real-provider integration or a paid business pilot.

Live Razorpay key IDs are rejected. Do not enable real collections simply by
changing that check. Merchant onboarding, settlement ownership, security/release
review and a controlled pilot are prerequisites for a separate live release.

## Configure real email delivery

Create a transactional email account with Resend and verify a sender domain.
Keep its API key in a local ignored environment file, not in chat or Git. Configure:

```bash
export CHAY_EMAIL_API_KEY='<your-local-key>'
export CHAY_EMAIL_FROM='ChayRound <support@your-verified-domain>'
```

The app sends codes through Resend's HTTPS email API. Codes expire in ten minutes,
allow five confirmation attempts and have a per-subject request limit. A contact
email change requires new verification; old-email challenges cannot verify it.
The private office link and optional PIN are still needed to verify that office.
Owner email and office email are verified separately. Address/service eligibility
is confirmed by the owner; no physical-address or phone-number verification is claimed.

Missing configuration returns an explicit unavailable response and leaves the
contact unverified. There is no product endpoint to reveal or bypass codes.
See [Resend send-email API](https://resend.com/docs/api-reference/emails/send-email).

## Configure per-stall Razorpay Test Mode

1. Register the owner, verify email, and find stall_id in GET /api/owner.
2. Use that stall's own Razorpay Test Mode account. Do not collect unrelated
   merchants' money into one platform account as an implicit settlement scheme.
3. Create `.run/razorpay.local.json` using this structure, replacing placeholders
   locally. Protect it as sensitive configuration; it is ignored by Git.

```json
{
  "1": {
    "key_id": "rzp_test_REPLACE_LOCALLY",
    "key_secret": "REPLACE_LOCALLY",
    "webhook_secret": "REPLACE_LOCALLY"
  }
}
```

```bash
chmod 600 .run/razorpay.local.json
export CHAY_RAZORPAY_ACCOUNTS_FILE="$PWD/.run/razorpay.local.json"
python3 scripts/dev.py
```

Do not commit the configured file. Different stall IDs have independently
configured accounts. Checkout exposes only the public Test key and stored
provider order ID; secrets stay server-side. Configure provider capture settings
in the Test dashboard; an authorized payment alone cannot enable preparation.

The browser uses Razorpay Standard Checkout. Verification checks HMAC over the
server-stored order ID and payment ID, then fetches the payment using the stall's
credentials and checks captured status, order, INR currency and exact amount.
See [Orders API](https://razorpay.com/docs/api/orders/create/) and
[payment fetch API](https://razorpay.com/docs/api/payments/fetch-with-id/).

## Webhooks and reconciliation

Webhook path: `POST /api/webhooks/{stall_id}`. A real provider cannot reach a
loopback URL. For real Test webhook delivery, an operator must provision a suitable
HTTPS endpoint/reverse proxy and configure the stall's webhook secret. This work
does not expose the local application publicly or create a tunnel automatically.

Subscribe to payment.captured and refund.processed. The app verifies raw-body
HMAC, scopes provider fetches to the configured account, and deduplicates event IDs.
Webhook payload claims alone cannot settle money. Unknown setup receipts are
reconciled before applying events. Failed events return an error so the provider
can retry; manual reconciliation and the worker provide a second recovery path.

The local launcher enables a C++ reconciliation worker. Its durable next-attempt
timestamp, attempts and lease are stored with the order. It starts after a short
grace period, uses bounded provider calls, retries with backoff capped at five
minutes, and checks captured payments and pending refunds. `CHAY_RECONCILE_WORKER=0`
can disable the worker for controlled debugging. Tests enable it explicitly against
the isolated provider. A database lease expiry allows recovery after process death;
financial unique keys and provider refund idempotency protect overlapping attempts.

An ambiguous provider order-creation result is looked up by its stable receipt.
If it cannot be resolved, the app does not blindly create another provider order.
Investigate with provider support. An unpaid cancelled provider order can remain
under observation for late capture; a late capture creates refund liability and
does not reopen fulfilment. Provider order/key rotations affecting old payments
require operator planning; replacing account secrets with a different account can
make historical reconciliation unavailable.

Refunds use a persisted X-Refund-Idempotency key and exact original amount. A lost
reply can be retried with the same key. Only a canonical processed provider refund
marks REFUNDED. Pending/failed provider refunds stay unresolved in the account.
See [Razorpay idempotent refund contract](https://github.com/razorpay/markdown-docs/blob/master/api/refunds/normal-refunds-idempotent.md).

## Monthly accounting

- Monthly orders reserve credit while PLACED, ACCEPTED or READY.
- Fulfilment atomically creates a charge; pending reservations are not invoiced.
- Owner-confirmed CASH/BANK receipts allocate oldest unpaid fulfilled charges first.
- Partial payments, idempotent retries, unique references and receipt reversals
  preserve a traceable history. Confirmation is the owner's assertion, not bank API verification.
- Adjustments are separate debit or credit entries with required reasons. Credits
  cannot exceed the remaining unpaid charges on the selected order.
- Statements cover closed India calendar months only, uniquely per office/month.
  They reference existing charges and never create a second debt.
- Due date is the next month's start at India midnight plus seven days. Outstanding
  statement allocations determine overdue status. Overdue/disabled/insufficient
  credit blocks new credit orders; the office can explicitly choose prepayment.
- Statements are account statements, not configured GST tax invoices. Merchant tax
  fields and tax rules need business/accounting configuration before tax-invoice claims.

## Operator checks and recovery

1. Inspect /health on all three services. Merchant/Order health queries depend on
   their schema; Gateway health alone proves only Gateway liveness.
2. Sign in and inspect /api/operations: payment pending, refund pending and uncertain
   setup counts. Use the seller's reconcile/refund actions for specific orders.
3. Compare order/payment/refund IDs and amounts with the relevant Test dashboard.
   Never mark paid from a screenshot or edit financial rows directly.
4. Reconcile ledger balances, receipt allocations and statement outstanding amounts.
5. Preserve logs without credentials, office tokens, signatures, codes or full
   request bodies. Crow request logging is reduced to Warning.
6. For stuck jobs or provider faults, pause new orders, retain records and follow
   up with the configured provider. There is no delete-debt recovery procedure.

## Offline backup and restore drill

Stop the supervisor and ensure all manually launched processes using those DBs
have stopped. The new supervisor holds stack.lock; the backup command refuses
while that supervisor is running. The lock does not detect arbitrary external
processes that ignore it. Both service DBs must be quiescent for a coherent pair.

```bash
python3 scripts/backup.py backup --data-dir .run --output .run/backups/snapshot-001
python3 scripts/backup.py restore --source .run/backups/snapshot-001 --data-dir .run-restored
CHAY_DATA_DIR="$PWD/.run-restored" python3 scripts/dev.py
```

Restore writes a new directory only; it never replaces existing data. Backups
include integrity results and SHA-256 digests. They contain sensitive business
data and must be encrypted/access controlled for real use. Provider secrets are
not copied into backups. Restored pending verification challenges may require a
new code because the runtime secret is not restored. Reconcile provider activity
after recovery before reopening ordering. Disposable E2E tests exercise backup,
restore, refusal to overwrite, and service restart from restored DBs.

## Remaining commercial acceptance

Real email delivery, a real Razorpay Test checkout/refund/webhook cycle per merchant
arrangement, target-phone audio/background behaviour, production HTTPS/session
hardening, business onboarding, and a real paid pilot still require external
configuration or user/operator participation. Passing local fixtures does not
complete those acceptance items. See PHASE_1_2_ACCEPTANCE.md.
