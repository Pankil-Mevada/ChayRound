# HTTP contract: initial slice

All browser requests use Gateway at localhost:9180. JSON bodies; errors have an
`error` field. Use `Content-Type: application/json`. Money is integer paise.
API lists currently cap orders at 200 without pagination; do not use this as a
complete account statement.

| Method/path | Authority | Request / result |
| --- | --- | --- |
| POST /api/register | None | email, password (12+ chars), stall_name; 201 |
| POST /api/login | None | email, password; returns token |
| POST /api/logout | Owner | Revokes current session |
| GET /api/owner | Owner | Stall, menu and office summaries |
| POST /api/menu | Owner | name, price_paise; returns item id |
| POST /api/offices | Owner | name, policy PREPAID/MONTHLY, credit_limit_paise; id and office_token |
| POST /api/offices/{id}/rotate | Owner | Empty object; new office_token, invalidates old token |
| GET /api/office | Office | Menu, policy, limit and committed_paise |
| POST /api/orders | Office | item_id, quantity 1-500; Idempotency-Key header required |
| GET /api/orders | Owner or office | Scoped latest 200 orders |
| POST /api/orders/{id}/status | Owner | status, version |
| GET /health | None | Gateway liveness; service endpoints check own DB |

Owner authority: `Authorization: Bearer <session>`. Office authority:
`X-Office-Token: <private token>`. Do not send both. A browser office link uses
`/#office=<token>`; fragments are not sent in HTTP URLs. Share privately.

Allowed transitions: PLACED -> ACCEPTED -> READY -> FULFILLED. PLACED, ACCEPTED
and READY can become CANCELLED. Status responses require refresh to obtain the
new version. No customer cancellation API or owner settlement API exists yet.

Order creation returns 201; valid replay returns 200 with the original ID;
payload/key mismatch, credit excess, and prepaid ordering return 409. Unknown
credentials return 401; foreign-owner resource updates return 404. Downstream
connection failures return 503. Unexpected storage errors return generic 500.

Internal Merchant routes `/internal/owner` and `/internal/office` require
X-Internal-Key. The gateway does not expose these paths. Internal office lookup
uses POST to keep raw tokens out of request URLs. These are service contracts,
not browser endpoints.
