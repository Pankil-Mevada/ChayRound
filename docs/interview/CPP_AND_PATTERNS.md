# C++ and low-level design

## Resource lifetimes and exception safety

`Db` owns sqlite3*, `Stmt` owns sqlite3_stmt*, and `Transaction` owns an active
transaction boundary. Stack unwinding finalizes statements before closing their
database because objects are destroyed in reverse construction order. Declare
the DB before its statements. Copying owners is disabled to avoid double release.

`Transaction` rolls back unless `commit()` succeeds. The destructor uses a
non-throwing rollback call: throwing while another exception is unwinding can
terminate the process. SQL changes, including the event, either commit or roll
back together. The domain test exercises this rollback behaviour.

The HTTP helper wraps CURL and curl_slist in unique_ptr with custom deleters.
This adapts C APIs to scope ownership. It does not need shared_ptr because there
is one clear owner. A future connection pool would need a different ownership
contract, bounded capacity and shutdown behaviour.

## Validation and types

`integer` rejects fractional numbers and enforces small documented ranges.
`total` uses bounded integer paise, avoiding floating-point currency arithmetic
in the authoritative calculation. A stronger model could introduce MoneyPaise,
OfficeId and OrderId types to prevent unit/identity mixups. These are not present
yet; current aliases are strings and integers.

`transition` is a pure domain function. It is easy to test without HTTP or a DB.
A table-driven state machine may become clearer as refund and payment states
grow. Do not combine fulfilment and payment into one enormous status enum.

## Concurrency

Each request opens its own SQLite connection and statements; there is no shared
mutable connection between Crow worker threads. SQLite, not a C++ mutex, serializes
writers across requests/processes. The shared database path is assigned before
the server starts and read afterwards.

Using mutex only inside one process would not protect a second service instance.
Use the persistence authority for money/credit invariants. Four worker threads
can still block on synchronous network calls or password hashing. A timeout on
each dependency does not by itself ensure the total end-to-end budget is met.

## Layers and patterns: current versus next

Current code is a small vertical slice: HTTP handlers include validation,
application decisions and prepared SQL. This makes the initial flow traceable,
but handlers are already dense. It is not a clean/hexagonal architecture merely
because services have separate directories.

Next refactor: extract `PlaceOrder` and `ChangeOrderStatus` use cases, a typed
OrderRepository, and a MerchantAccessClient port. Inject a client for focused
failure tests, keeping integration tests as the external contract. Introduce
abstractions where they make dependencies or invariants clearer.

Possible future Strategy: settlement schedule (daily/weekly/monthly) behind a
common calculation contract. The current two policy strings are not a Strategy
class implementation. Avoid a factory/singleton hierarchy without a need.

## Complexity and improvement prompts

- Menu validation scans the returned menu: O(m). For a tiny menu this is reasonable;
  a dedicated server quote endpoint would avoid transferring all items per order.
- Commitment SUM is O(k) matching unpaid order rows with an office index. At larger
  history sizes, a transactionally maintained account row can bound lookup work,
  with reconciliation to detect drift.
- Order/ledger lists now use rowid keyset cursors; test equal-time ordering and
  inspect query plans before claiming a particular index provides the full plan.
- Password derivation dominates login CPU; rate limiting and calibrated work
  factors matter more than micro-optimising a string copy.
- Additive schema migration now uses table introspection and user_version. A
  one-time import preserves old fulfilled charges without replaying it on restart.
  Offline backup/restore exists; online/destructive migrations need further design.

## Exercises

1. Extract PlaceOrder without breaking tests; identify its inputs and authority.
2. Add a MoneyPaise type and demonstrate a prevented unit error.
3. Extend cursor-pagination tests to large multi-tenant histories.
4. Replace generic DB exceptions with typed busy/constraint/unavailable errors.
5. Make a failed dependency test deterministic via an injected client.
6. Inspect cancelled-credit, partial-payment and receipt-reversal tests before changing policy.
