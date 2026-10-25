# Office and tea-stall communication

ChayRound is the order and account workspace. WhatsApp is not required.

1. An office finds the stall or scans its public QR, then requests browser access.
   The owner confirms the requester outside the app and chooses the saved office
   account. Another browser requires another approval.
2. The office submits quantities from the actual menu. The server determines
   identity, prices, payment requirements, capacity and credit. The request appears
   in the correct branch's queue. Sending a message does not place an order.
3. The owner accepts, marks ready and confirms actual fulfilment. The office sees
   those states while the portal is open. The queue can play an opt-in foreground
   sound. Ready does not claim that delivery has occurred.
4. Each office and stall have a private conversation shared by that office's
   approved browsers and permitted stall team. Use it for reception instructions,
   delays or service questions. The owner's office list shows unread office
   messages; explicit read acknowledgements show which messages the other side
   opened. Stored is not the same as read or acted upon.
5. Orders, payments, corrections and credit terms use their own controls. A message
   saying “paid” cannot mark a payment as verified; “send two more” cannot silently
   alter an existing priced order. Submit another reviewed basket or ask the owner
   to cancel/reject the old request under the recorded workflow.
6. Bills, advance balances and payment records are visible in the account tools.
   Billing reminders appear as system messages. Optional marketing is separately
   opt-in and can be disabled without disabling order/account access.

Conversations are office-level, not individual employee chats. Messages are
stored as text, scoped on the server, limited to 2,000 bytes and rate limited.
Request IDs deduplicate retries. Revoking a browser blocks further API access;
it cannot erase information somebody already viewed or copied.

The browser polls while visible (messages/order status roughly every ten seconds;
the seller queue also has its existing fifteen-second refresh). Neither push
notifications nor background browser sound are promised. Urgent exceptions still
need the stall's agreed real-world contact arrangement.

Unlike a free-text chat order, the product maintains current menu prices, explicit
acceptance, fulfilment state, duplicate protection, office credit limits, ledger
entries, statements and reconciliation. The conversation helps resolve exceptions;
those structured records are what make the product useful.
