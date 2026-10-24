# Owner-approved office browsers

Email challenges and private links made tea ordering hard to onboard. Publicly
listing offices for credit ordering would permit impersonation. The implemented
alternative is a browser capability approved by the owner once.

`services/merchant/portal.h` implements public stall discovery and pending requests.
Owners assign a request to an office belonging to their own stall in a transaction.
`merchant.h::officeId` resolves approved token hashes; Order derives office identity
through Merchant and keeps its integer-paise pricing/atomic credit checks intact.
Approval/revocation is audited. Office approval does not fabricate email verification.

State transitions: PENDING -> APPROVED -> REVOKED, or PENDING -> REVOKED.
A grant cannot be reassigned after approval or resurrected after revocation.
Another browser requires a separate grant; all share the office's existing credit.

Tradeoffs: localStorage simplifies return visits but exposes bearer credentials to
XSS and shared-device users. Secure HttpOnly cookies plus CSRF protection, expiry,
device naming and stronger owner identity would improve production deployment;
these are not implemented. The owner must verify who requested access outside the
app. This is shared office access, not proof of employee identity.

E2E cases cover pending denial, owner scoping, cross-stall office denial, no-email
approval/order, restart persistence, revocation and rejection of resurrection.
They use disposable databases/providers. See ../OFFICE_ACCESS.md for product limits.
