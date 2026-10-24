# Authorization errors across service boundaries

`services/merchant/merchant.h::officeId` authenticates office links and optional
PINs. `services/order/repository.h::merchant` previously replaced every client
error with a generic denial. This hid whether the user needed the office PIN,
a replacement link, an owner sign-in, or owner email verification.

Order now forwards an explicit allowlist of public error messages while keeping
the status code. Arbitrary dependency errors remain generic and server failures
remain unavailable responses. This preserves a useful API contract without
reflecting arbitrary provider bodies, secrets or database diagnostics. The PIN
form also explains that the PIN is owner-configured and separate from email codes.

No authentication rule is relaxed: a configured PIN is still mandatory, invalid
links remain denied, and unverified offices cannot place orders. HTTP regressions
exercise missing/wrong/correct PIN, no-PIN onboarding, and invalid-link messages
with disposable databases. Email verification is reachable after authenticating
the office link/PIN; it is not an additional prerequisite for viewing onboarding.

Tradeoff: an allowlist must track deliberate changes to Merchant's public messages.
Stable machine-readable error codes would be preferable as the API grows, but
are not implemented by this change. Specific errors do not prove the cause of a
historical screenshot: reproduce the request after restarting the updated service.
