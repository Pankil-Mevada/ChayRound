# Concept-to-code map

Paths are relative to the project root; symbols/routes are stable search anchors.
Use `rg -n 'anchor' common services tests frontend` to find exact current lines.

| Concept | Actual code location / anchor | Why it exists and how to discuss it |
| --- | --- | --- |
| API gateway / reverse proxy | services/gateway/main.cpp, `/api/<path>` | Same-origin entry point; routes to owning service; no SQL joins |
| Bounded context | services/merchant/main.cpp and services/order/main.cpp, schema creation | Merchant owns identity/catalog; Order owns transactional commitments |
| RAII | common/core.h, `Db`, `Stmt`, `Transaction` | Destructors close/finalize/rollback on every exit path |
| Unique ownership | common/core.h, `unique_ptr<CURL` | Custom deleter releases C library handles |
| Prepared statements | common/core.h, `Stmt::bind` | Values bound separately from SQL, also explicit resource lifetime |
| Unit of Work-like boundary | common/core.h, `Transaction`; Order POST route | Multiple SQL writes commit together; no repository framework is claimed |
| Money value invariant | common/core.h, `total`, `integer` | Paise integers and bounded quantity; browser prices ignored |
| Idempotency | services/order/main.cpp, `UNIQUE(office_id,idem)`, `request_hash` | Semantic payload check and durable office-scoped key; retries reuse response ID |
| Race prevention | services/order/main.cpp, `Transaction tx`, `SUM(total)` | Credit check and insert under one write transaction, not check-then-write outside it |
| State machine | common/core.h, `transition` | Central legal transitions reject skips and terminal reopen |
| Optimistic version contract | services/order/main.cpp, `version=version+1` | Stale owner action gets 409; transaction and predicate protect mutation |
| Snapshot | services/order/main.cpp, `item_name`, `total`, `policy` | Historical order does not depend on later menu reads |
| Capability-based access | services/merchant/main.cpp, `token_hash`, rotate route | Possession of private office token grants office scope; sharing is a tradeoff |
| Server-side session | services/merchant/main.cpp, `sessions`, `owner` | Random bearer session, expiry and logout; not JWT |
| Password derivation | common/core.h, `password` | Slow salted PBKDF2; separate from fast token hashing |
| Tenant isolation | Merchant `stall`, Order `owner`, office resolve | Owner IDs/stall IDs resolved from credentials rather than trusted JSON |
| Service authentication | common/core.h, `internal`, `serviceKey` | Shared runtime secret for internal access; not mTLS |
| Timeout / fail closed | common/core.h, `http`, `remote` | A failed access lookup does not bypass authorization |
| Atomic audit events | Order `events` insertion | Mutation and event share commit; no worker/publication/immutable external audit yet |
| Frontend retry key | frontend/index.html, `chayPending`, `Idempotency-Key` | Reuse after unknown network result; changed payload generates new key |
| Integration isolation | tests/e2e.py, `TemporaryDirectory`, `free_port` | Real local HTTP processes, disposable DBs and no payment providers |

## Trace one order

1. `orderForm` builds item_id and quantity, persists a retry key in sessionStorage.
2. Gateway routes to Order and forwards only selected authority headers.
3. Order asks Merchant to resolve the office token and menu with an internal key.
4. Merchant hashes the token, resolves the office/stall and returns current data.
5. Order checks the office-scoped key inside a write transaction.
6. It resolves menu price, checks all unpaid commitments and creates a snapshot.
7. Order plus PLACED event commit atomically; Gateway returns the result.
8. The UI clears the retry key after a successful response and shows a request,
   not a falsely confirmed or paid order.

## Patterns intentionally not claimed

No full Repository layer, rich aggregate model, abstract Strategy hierarchy,
CQRS read model, saga orchestrator, message bus or distributed transaction exists.
Current HTTP handlers contain application logic and SQL. A next refactor should
extract use cases and typed repositories when billing raises complexity, without
changing the externally tested invariants. Avoid adding abstractions solely to
name design patterns in an interview.
