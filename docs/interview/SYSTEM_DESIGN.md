# System design walkthrough

## 1. Clarify the problem before drawing boxes

Design a multi-tenant tea-ordering portal. Owners manage office credit; offices
order with private links. Needed: authoritative prices, no cross-stall access,
no duplicate request charges, no overspent credit, recoverable persisted state.
Ask about office-paid versus employee-paid orders, delivery responsibility,
refunds, daily peak windows, geographical scope, retention and availability goals.

Current scope is one item per request and unpaid monthly-credit ordering. A
commercial ledger, prepaid processing and verified office contacts remain future
work. Do not blur those boundaries in the interview.

## 2. Make explicit estimates

Example assumptions, not measured traffic: 1,000 stalls x 10 offices x 2 rounds
per working day = 20,000 office orders/day. If 50% arrive within a 10-minute
window, that is about 16.7 order writes/second in that window. Twenty-times burst
headroom would be about 334 writes/second. Employee-level orders change these
numbers substantially; ask whether orders are consolidated before sizing.

At an assumed 1 KB per order including a rough event allowance, 20,000/day is
about 20 MB/day or 7.3 GB/year before indexes, WAL, backups and replication. Actual
row sizes and retention must be measured. This is not a SQLite benchmark.

Four synchronous HTTP workers per service and PBKDF2 cost are current limits.
Benchmark peak latency, error rate, write contention and memory before selecting
deployment sizes. Do not infer 334 writes/second support from this example.

## 3. Identify the hardest invariant

For an office:

`unpaid non-cancelled commitments + proposed total <= approved credit limit`

Both reading commitments and inserting the order must share a serialization
boundary. A sequence of independent SELECT and INSERT statements is unsafe:
two requests can read the same remaining credit and both spend it.

The current `BEGIN IMMEDIATE` serializes SQLite writers. This protects the
invariant on a single database but serializes unrelated offices too. PostgreSQL
could lock one office's account row, update reserved credit, and insert an order
in a transaction, enabling unrelated offices to progress concurrently.

## 4. Discuss consistency across services honestly

The Merchant lookup precedes the Order transaction. Credit usage is locally
consistent, but catalog/policy is a lookup-time snapshot. Token rotation or future
policy edits can race an already-authorized request. A revoked token blocks new
lookups, not requests whose authorization already completed.

Options for stricter policy semantics: version policies and reject mismatched
versions at an authoritative reservation service; move credit authorization and
policy ownership together; or accept a documented short snapshot window. A second
remote lookup alone does not make the two services atomic. No distributed lock
has been implemented here.

## 5. Handle retries and ambiguous outcomes

An HTTP timeout does not prove an order failed. The DB may commit before the
response is lost. Persist a semantic request key on the client, scope uniqueness
by office, compare the payload fingerprint and return the original ID on replay.

This is idempotent order creation for one key, not universal exactly-once
processing. A client that invents a new key for the same intent can create another
order. A revoked link currently cannot replay its previous request.

## 6. Handle state and financial history

PLACED -> ACCEPTED -> READY -> FULFILLED, with cancellation before fulfilment.
Expected versions prevent stale seller actions. Fulfilment and payment are
separate dimensions. Delivered tea can remain unpaid; a payment may also require
a refund after cancellation. A future ledger should record immutable debits,
credits and allocations rather than overwrite an office balance without history.

Monthly invoice creation groups charges and must not create duplicate debt.
Use unique period/office invoice identity, explicit timezone/cutoff, partial
payment allocations, due dates and reconciliation. Define tax and rounding rules
with the business before coding them.

## 7. Add asynchronous work only where needed

Notifications should not decide whether a valid order exists. Future design:
commit order and outbox event together; a worker publishes with retry; consumers
deduplicate by event ID. At-least-once delivery can repeat messages; retry limits,
backoff, dead letters and operator recovery must be specified.

Avoid treating the existing local events table as a complete outbox. It has no
delivery state, publisher, leasing or consumer deduplication yet.

## 8. Availability and scaling choices

Reads could cache public menu data with short TTL/version invalidation. Do not
cache access or credit decisions indefinitely. Merchant failure currently blocks
new orders; this favours access correctness over order availability. Discuss
stale-read tolerance explicitly instead of saying CAP means merely choose two.

For multi-node deployment: managed PostgreSQL, load-balanced stateless HTTP
instances, private service identity, bounded DB/HTTP pools, timeouts aligned with
request budgets, retry budgets, rate limits and metrics. Shard by stall only after
measuring a need; offices and orders should stay colocated with credit accounts.
Hot tenants need capacity controls. Partitioning early increases operational cost.

## 9. Operate the system

Define proposed SLOs with stakeholders, then measure p95/p99 order latency, error
rate, credit conflicts, oldest unprocessed event and payment reconciliation lag.
Do not claim an SLO was met without a measurement window. Use trace IDs across
services, structured redacted logs and metrics without office tokens or PII.
Test DB restore, process restarts, dependency outages and deploy rollback.

## 10. Explain alternatives

- Modular monolith: simpler operations and transactions; less service isolation.
- Microservices: independent ownership/deployments, but latency and consistency cost.
- Polling: simple queue refresh; more requests and delayed updates.
- SSE: good fit for one-way order updates; reconnect/cursor handling still required.
- WebSockets: bidirectional needs only; not automatically better for a seller queue.
- SQLite: simple local persistence; single writer and local-file deployment limit.
- PostgreSQL: stronger multi-node operational path; additional setup and maintenance.

Finish the discussion with the highest-risk unresolved decision and the next
experiment, not a collection of technology names.
