# Phase 1/2: financial correctness and recovery interview guide

## Code anchors

| Concern | Implementation anchor | What to explain |
| --- | --- | --- |
| Schema evolution | Merchant migrate; Order migrate; common column | Additive upgrades preserve old data; version controls one-time charge import |
| Email challenge | merchant/merchant.h verification | Expiry, attempt limits, HMAC hash, subject/email binding, atomic consumption |
| Access capability | merchant officeId | Revocable high-entropy link plus optional PIN; shared office identity tradeoff |
| Versioned policy | merchant main /api/offices/{id} | Current policy authorizes new requests, order records retain policy version/snapshot |
| Quote and basket | order main POST /api/orders | Normalize duplicate item IDs; fingerprint semantic intent; trust server catalog |
| Credit calculation | order/repository.h reserved, outstanding, overdue | Pending reservations plus unallocated fulfilled charges, atomically checked |
| Financial history | repository entry; billing payments/adjustment/reverse | Immutable signed entries and allocations, not destructive balance overwrites |
| Partial payment | allocate, updateSettlement | FIFO debit allocation; no overpayment; receipt replay cannot duplicate credit |
| Receipt reversal | billing /api/receipts/{id}/reverse | New debit and negative allocation records reopen debt while retaining original evidence |
| Invoicing | billing monthly issuance and invoice_lines | Stable closed-month snapshot, unique office/period, IST boundary, no second charge |
| Payment adapter | order/payments.h Account, provider | Per-stall credentials; test-only mode; provider I/O outside write transactions |
| Ambiguous setup | checkout, receipt | Persist intent before request; receipt lookup recovers lost response; never blind POST retry |
| Capture authenticity | /verify, applyCapture | Stored order HMAC plus canonical fetch, amount/currency/account/order checks |
| Refund idempotency | refund, finishRefund | Stable provider key; canonical processed status; immutable compensating entries |
| Webhook replay | Order /api/webhooks | Raw body HMAC, provider fetch and durable event IDs; no reliance on delivery order |
| Recovery worker | reconciliationWorker | Durable schedule, lease, capped backoff, idempotent application after crash |
| Backup/restore | scripts/backup.py | Stop writers, snapshot both DBs, verify integrity, restore into new directory |

## Work through an example

An office has a Rs 100 credit limit. Two pending Rs 50 orders commit serially.
A third is rejected. Fulfilling one converts its Rs 50 reservation into a ledger
charge in the same transaction; total committed credit is still Rs 100. Receiving
Rs 20 allocates part of that charge, reducing committed credit to Rs 80. Reversing
the mistaken receipt appends new records and reopens Rs 20 of debt. No history is
deleted. Cancelling the other pending order releases its Rs 50 reservation.

A monthly statement references the fulfilled charge. It does not add Rs 50 again.
Credit notes and receipts change outstanding allocations while the issued line
amount remains stable. Explain the difference between original charge, statement
total, current amount outstanding, and payment received.

## Distributed failure examples

1. **Payment provider creates an order, then connection closes.** Local setup is
   UNKNOWN. Receipt lookup adopts the same provider order. A missing lookup is not
   proof that creating another order is safe; unresolved work remains visible.
2. **Payment captured, customer closes the tab.** Webhook or worker fetches canonical
   payment state and applies charge/payment once. The browser is not the authority.
3. **Owner cancels, then capture arrives.** Fulfilment stays CANCELLED; refund
   liability is recorded and recovered. Do not reopen the order on payment success.
4. **Refund provider processes then reply is lost.** Same persisted refund key
   retrieves/retries the original result. Only processed canonical status completes
   local refund accounting.
5. **Worker crashes after claiming work.** Another attempt waits for lease expiry.
   A stale worker cannot overwrite a newer schedule's lease. Financial application
   remains idempotent even if network work overlaps after expiry.

## Patterns and boundaries

The system now has a provider adapter, repository functions, durable job scheduling,
an explicit order state machine, RAII transaction boundaries, immutable accounting
entries and compensating actions. It still does not use Kafka, Redis, a full saga
framework, distributed SQL transactions, or an event-sourced application.

The worker is not a complete transactional outbox publisher. Its durable task state
is colocated with orders; no broker/consumer acknowledgement protocol exists.
Append-only ledger and local audit are not externally tamper-proof: a database
administrator can change schema/triggers. Discuss external audit storage and roles
if the threat model requires stronger evidence.

## Remaining tradeoffs to discuss

- One SQLite writer serializes unrelated offices; PostgreSQL per-account row locks
  would improve write concurrency when measurements justify migration.
- Merchant policy/hours/availability are lookup-time snapshots. Rotation or policy
  edits cannot retroactively revoke a request already authorized before the change.
  Stronger semantics need colocated authorization/reservation or a version protocol.
- Availability is a menu toggle, not per-unit inventory reservation. No hidden claim
  that the app can observe stock, attendance, cash collection or physical delivery.
- Current background worker is deliberately small; scaling demands job indexes,
  bounded concurrency, provider rate budgets, jitter and per-merchant fairness.
- Real payment integration requires correct merchant settlement ownership, HTTPS
  webhooks, capture settings and reconciliation. Fixtures cannot validate provider
  account onboarding, actual settlement or customer willingness to pay.

## Practice prompts

Explain why retrying GET is different from retrying an ambiguous money-related
POST. Draw the order/charge/payment/refund dimensions. Show why a shared C++ mutex
does not protect multiple instances. Design an immutable correction when a receipt
was allocated across three invoices. Explain how you would reconcile a provider
refund created manually outside this application. State which of these scenarios
the current tests cover and which require additional integration work.
