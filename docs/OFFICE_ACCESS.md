# Simple office ordering

The default office flow no longer requires customer email, email verification,
PIN entry, or a private link. Owner registration/email verification is unchanged.

## Customer

1. Open ChayRound. Search by stall name or area and choose the stall.
2. Enter your office name and press **Ask owner to connect my office**.
3. Ask the owner to approve it. The visible page checks automatically; **Check
   approval** also works. A pending request cannot access an office or order.
4. After approval, choose tea quantities and send the order.
5. Returning on the same browser remembers your office. Use **Change office or
   stall** to switch. Another browser or cleared site storage needs new approval.

No install or daily sign-in is needed. This grants shared office access, not
verified employee identity. Anyone using that browser can see the office's orders
and account. Existing monthly terms and Razorpay Test checkout still apply.

## Owner

1. Use **Tea-stall owner? Open your tea desk** on the home page or `/#owner`.
2. Save the office name, address, matching service area and payment terms under
   **Add an office**. Email and legacy PIN fields are optional advanced settings.
3. Under **Office access**, confirm who is requesting access in person/by your
   usual contact method. Choose the right saved office and **Approve office**.
   Do not approve based on the typed office name alone.
4. Only the owner sets monthly credit/limits and accepts/fulfils orders.
5. **Remove browser access** stops future requests from that browser, without
   deleting history, cancelling past orders or removing other browsers.

## Implementation and limits

Merchant owns `office_devices` and additive `offices.owner_approved`. The public
directory returns only stall identity/area, not offices, addresses, contacts or
balances. The current directory returns at most 100 verified-owner stalls and
filters locally; larger-market server search/pagination remains future work.

The browser stores an opaque token in localStorage, not a URL; Merchant stores
its hash and resolves an approved grant to one office. Owner/office stall ownership
is checked transactionally during approval. Order still derives identity through
Merchant and retains server pricing, credit reservations and payment checks.
`verified` in legacy API responses now means email verified OR owner approved;
`email_verified` and `owner_approved` expose the difference explicitly.

Legacy links and PIN APIs remain available; the default UI does not share those
links. Rotating a legacy link does not revoke browser grants. Use browser removal.
Migration approves no old office automatically. A revoked grant cannot be approved
again or silently reassigned; create a fresh customer request.

Pending submissions have a per-gateway-visible-IP limit and the owner queue returns
100 recent requests. Queue pagination, expiry and push notifications are not yet
implemented. Browser grants have no automatic expiry. LocalStorage bearer tokens
are exposed to same-origin scripts and shared-device users. HTTPS, session/XSS
hardening, trusted client-IP rate limiting and production review remain necessary.

## Manual test

Use an owner window at `/#owner` and a separate customer browser profile at `/`.
Save a monthly office without email/PIN. Find the stall, request access, approve
the correct office, order and fulfil tea. Reopen the customer browser to check
remembered access. Remove its access and confirm office/account/order requests
are denied. Use test records and Test Mode; automated tests use disposable DBs.
