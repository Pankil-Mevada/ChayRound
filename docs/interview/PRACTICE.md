# Interview practice and answer checkpoints

## A 45-minute design practice

- Minutes 0-5: clarify office versus individual payment, scale and delivery scope.
- Minutes 5-10: state functional/nonfunctional requirements and estimate peak load.
- Minutes 10-20: draw services, database ownership and API contracts.
- Minutes 20-30: deep dive on credit concurrency and ambiguous retries.
- Minutes 30-38: failures, authorization, billing evolution and scale bottlenecks.
- Minutes 38-45: alternatives, operational risks and the next measurement.

## Questions with answer checkpoints

1. **Why C++?** User requirement and systems-learning value; RAII and explicit
   resource control. Acknowledge build time, ecosystem and development effort;
   do not claim measured superiority without benchmarks.
2. **Why three services?** Explain Merchant/Order ownership and Gateway routing,
   then acknowledge that a modular monolith may be cheaper for this business.
3. **Two offices order simultaneously. What happens?** SQLite serializes writers
   broadly; per-office uniqueness scopes retries, not per-office locks.
4. **Same office overspends concurrently?** Sum/check/insert share BEGIN IMMEDIATE;
   tests launch four requests with only one remaining order's worth of credit.
5. **Server commits but browser times out?** Same persisted key returns same order
   ID; altered payload with same key gets 409. No global exactly-once claim.
6. **Seller presses Ready on stale data?** Version mismatch returns 409; refresh.
7. **Someone changes stall_id in JSON?** Authority comes from credential lookup;
   item must exist in that stall's menu. Show the cross-tenant E2E checks.
8. **Office link leaked?** Anyone holding it can order; rotate immediately. Explain
   PIN/contact verification/expiry as planned hardening, not existing features.
9. **Why hash tokens but derive passwords slowly?** High-entropy random tokens
   differ from guessable human passwords. Both raw values should stay out of logs.
10. **Merchant goes down?** Order authorization fails closed; bounded network calls
    return an error. A future cache needs explicit stale-access semantics.
11. **Where is the event bus?** None. Atomic local events are stored; publication,
    consumer deduplication and outbox leasing are future work.
12. **What happens to prepaid orders?** Rejected before insertion. A fake success
    or unverified screenshot would violate the product's financial invariant.
13. **How do invoices affect balance?** Future invoice groups charges, not a second
    debit. Explain partial allocations and reconciliation; not implemented yet.
14. **What fails at ten times traffic?** Measure SQLite write waits, HTTP worker
    saturation and password CPU first; discuss PostgreSQL/per-office locking.
15. **Is HTTP stateless enough for scaling?** Services persist local files; multi-node
    deployment needs shared durable DB infrastructure, not just another process.
16. **What evidence supports reliability?** Quote actual PROJECT_STATUS results and
    explain what they do not cover: real payments, browser/platform variation,
    sustained load, restore drills and external assessment.

## Practical drills

- Draw order creation as a sequence diagram without looking at the code.
- Reproduce a duplicate-key request and explain the SQL unique constraint.
- Stop Merchant and inspect the owner/order errors without bypassing access.
- Restart Order with its original DB and explain durable versus in-memory state.
- Explain one rejected design: browser-authoritative totals, global process mutex,
  fake payment success, or shared cross-service SQL access.
- Describe a future payment webhook delivered twice and delayed out of order.

## Assess your answer

Score each item 0 (missing), 1 (partial), 2 (clear with evidence): requirements,
estimates, ownership, consistency, retry semantics, security, failure behaviour,
operability, alternatives and implementation honesty. Use the score to choose
the next practice topic, not to predict hiring outcomes.

For behavioural preparation, keep your own notes on a real bug, a tradeoff you
changed after evidence, and how you verified the fix. Do not invent production
users, traffic, revenue or incidents for this project.
