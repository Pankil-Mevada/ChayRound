# HTTP API: Phases 1/2

Default onboarding now uses [owner-approved browsers](OFFICE_ACCESS.md).
GET /api/stalls lists public stall identity/area. POST /api/access/request accepts
stall_id and office_name, returning a pending device_token. GET /api/access uses
X-Office-Token to read its status. GET /api/access/requests is owner-only;
POST /api/access/requests/{id} accepts status APPROVED plus an owned office_id,
or REVOKED. Approved tokens authenticate existing office/order/account routes.
Office email is optional; owner_approved and email_verified are separate facts.
Legacy verified is their OR; no email verification is fabricated by approval.

Gateway: localhost:9180. JSON bodies; money in integer paise. Owner requests use
Authorization: Bearer <session>. Office requests use X-Office-Token and optional
X-Office-Pin. Private browser links use /#office=<token>, avoiding query-string
token logging. Do not send both owner and office authority.

| Method/path | Authority | Contract |
| --- | --- | --- |
| POST /api/register | Public | email, password 12+ chars, stall_name |
| POST /api/login | Public | email/password; returns token |
| POST /api/logout | Owner | Revoke current session |
| POST /api/verification/request | Owner or office | Empty object; challenge_id, expires_in; code goes to configured email only |
| POST /api/verification/confirm | Same subject | challenge_id, six-digit code |
| GET /api/owner | Owner, may be unverified | Stall settings, verified flag, menu and office records |
| POST /api/stall | Verified owner | name, area, paused, opens/closes minutes 0-1439, days bitmask Sunday=1 through Saturday=64, version |
| POST /api/menu | Verified owner | name, price_paise |
| POST /api/menu/{id} | Verified owner | name, price_paise, available, version |
| POST /api/offices | Verified owner | name,email,address,area; optional instructions,pin,policy PREPAID/MONTHLY,credit_limit_paise; returns private office_token once |
| POST /api/offices/{id} | Verified owner | Same contact/menu-independent office fields plus policy,limit,service_enabled,credit_enabled,version,reason; optional pin changes it, empty pin removes it |
| POST /api/offices/{id}/rotate | Verified owner | Empty object; replaces link and increments version |
| GET /api/office | Office | Office/menu, contact verification, service/hours, committed credit and overdue flag |
| POST /api/orders | Verified office and owner | items [{item_id,quantity}], optional note, optional payment_mode PREPAID; Idempotency-Key required |
| GET /api/orders?before=N | Owner or office | Scoped orders array, optional next_cursor; latest 50 per page |
| POST /api/orders/{id}/status | Verified owner | status, expected version, reason required for cancellation |
| POST /api/orders/{id}/checkout | Scoped office/owner | Empty object; public key, stored provider order, amount and currency |
| POST /api/orders/{id}/verify | Scoped office/owner | payment_id, signature; canonical capture checked server-side |
| POST /api/orders/{id}/reconcile | Verified owner | Recover setup/capture/refund from configured provider |
| POST /api/orders/{id}/refund | Verified owner | Retry full refund of cancelled captured prepaid order |
| POST /api/webhooks/{stall_id} | Provider HMAC | Raw JSON with X-Razorpay-Signature, optional X-Razorpay-Event-Id |
| GET /api/account?before=N | Office | Account summary, entries, statements and optional next_cursor for entries |
| GET /api/accounts/{office_id}?before=N | Verified owner | Same account view, scoped to owner's office |
| POST /api/accounts/{id}/payments | Verified owner | amount_paise, method CASH/BANK, unique reference; Idempotency-Key required |
| POST /api/receipts/{receipt_id}/reverse | Verified owner | reason; idempotent compensating reversal |
| POST /api/orders/{id}/adjustment | Verified owner | Signed nonzero amount_paise, reason; Idempotency-Key required; fulfilled monthly orders only |
| POST /api/accounts/{id}/invoices | Verified owner | Closed period YYYY-MM; unique office/month, due seven days after next-month start in IST |
| GET /api/invoices/{id} | Scoped owner/office | Stable charge lines and current allocated outstanding amount |
| GET /api/operations | Verified owner | Pending payment/refund and uncertain setup counts |
| GET /health | Public local | Gateway liveness; Merchant/Order query own schema |

Ordinary states: PLACED -> ACCEPTED -> READY -> FULFILLED. PREPAID starts
PAYMENT_PENDING. Only canonical capture enables PLACED/preparation. Eligible
pre-fulfilment states can become CANCELLED; a paid cancellation becomes
REFUND_PENDING, then REFUNDED after provider confirmation. Payment settlement
and fulfilment are separate fields. Fulfilment is an owner-confirmed handover,
not independent physical proof.

Requests have size/range bounds. Order creation returns 201; replay returns 200;
semantic key conflicts, stale versions, unavailable items, closed service and
credit problems return 409. Authentication failures return 401; verification
requirements 403; foreign resource lookups 404; rate limits 429; unavailable
configuration/providers 503. Unexpected storage errors remain generic 500.

Internal Merchant paths /internal/owner, /internal/office and /internal/office/{id}
require X-Internal-Key and are not gateway-exposed. Test fixture provider and clock
overrides require explicit CHAY_TEST_FIXTURES=1 and loopback provider URLs. They
are never part of the browser API or normal startup workflow.
# Phase 3–5 additions

These routes retain the same bearer owner session or approved office browser
headers as the existing API. Owner scope and capabilities are resolved on the
server; request bodies cannot choose an authoritative stall, office or role.

| Route | Purpose / caller |
| --- | --- |
| `GET /api/public/stalls/:id` | Public menu/profile without private office or cost data |
| `POST /api/public-profile` | Owner/manager publishes genuine public details |
| `GET /api/conversations` | Team office list with unread office messages |
| `GET/POST /api/conversations/:office` | Scoped messages; office uses `0`; writes require `body` and stable `request_id` |
| `POST /api/conversations/:office/read` | Acknowledge an actually displayed `last_id` |
| `POST /api/orders/:id/feedback` | Office's fulfilled order, rating 1–5, optional comment |
| `GET/POST /api/rounds` | Office-owned recurring schedule with explicit consent and budget |
| `POST /api/rounds/:id` | Pause/resume, skip an India day, or permanently cancel future runs |
| `POST /api/stall/capacity` | Owner daily intake unit cap |
| `GET/POST /api/stall/holidays` | Owner holiday India-day numbers |
| `POST /api/offices/:id/billing-cycle` | Owner/manager, versioned future settlement terms and reason |
| `POST /api/accounts/:id/settlement-statement` | Closed DAILY/WEEKLY/AFTER_DELIVERY period by India start date |
| `POST /api/accounts/:id/remind` | Actual outstanding balance, one in-app reminder per 24 hours |
| `GET/POST /api/marketing-consent` | Office controls optional marketing consent/unsubscribe |
| `GET/POST /api/marketing-controls` | Owner master pause and rolling weekly cap |
| `GET/POST /api/marketing-automation` | Owner-approved exact welcome/reactivation templates |
| `POST /api/campaigns` | Approved, capped, idempotent in-app campaign |
| `GET /api/advance` | Office's balance, reservations, available funds and loyalty points |
| `GET/POST /api/accounts/:id/advance` | Reviewed, owner-confirmed cash/bank top-up or refund; stable request key |
| `GET/POST /api/reward-rules` | Per-stall reward amount, budget and pause |
| `POST /api/accounts/:id/referral` | Confirm distinct referrer before first fulfilment |
| `POST /api/accounts/:id/referral-reward` | One reviewed, budgeted reward after externally funded paid fulfilment |
| `POST /api/orders/:id/loyalty` | Claim non-cash points once for a paid fulfilled order |
| `POST /api/menu/:id/promotion` | Time window, unit cap, supplied cost/margin, discount budget and notice |
| `POST /api/menu/:id/cost` | Private cost assumption for future order snapshots |
| `GET /api/growth-report` | Branch-scoped sales, source, retention, feedback and conditional contribution |
| `GET/POST /api/business-expenses` | Actual expense records; duplicate references return the existing record |
| `POST /api/business-expenses/:id/reverse` | Immutable correction with a required reason |
| `GET /api/workspaces` | Authenticated principal's permitted branches/workspaces |
| `POST /api/workspaces/switch` | Select only a current server-authorized membership |
| `POST /api/branches` | Business owner creates an isolated, initially paused stall |
| `GET/POST /api/workers` | Business owner grants, changes or revokes verified worker membership |

`POST /api/orders` additionally accepts explicit `payment_mode: ADVANCE` and an
optional requested `delivery_at` epoch. Advance orders remain subject to the
financial-review gate. Existing PREPAID and default credit semantics remain.
Amounts in the API remain integer paise; the browser presents rupees.

Internal scheduled-office, reminder, acquisition and marketing APIs require the
service secret and are never exposed through the gateway. Order derives the
required worker capability itself; the browser cannot forward one. Worker roles
grant READ/COMMUNICATE plus MANAGE, FULFIL or ACCOUNT capabilities as described in
the acceptance guide. ADMIN remains owner-only. A worker's revoked current
workspace requires signing in again to return to their own workspace.

Read [Phase 3–5 acceptance](PHASE_3_5_ACCEPTANCE.md) for bounded-list, review,
delivery, financial and commercial limitations.
