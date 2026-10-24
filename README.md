# ChayRound

C++20 microservices and a dependency-free web portal for independent tea stalls
and their office clients. This is a local development prototype, not a commercial
release. Build the product and study its design together.

## Working slice

- Register/sign in as an owner and create menu items.
- Create an office with prepaid or monthly terms and a credit limit.
- Find the stall, request owner approval once per browser and place tea orders.
- Office access is remembered without email codes, daily login or private links.
- Seller accepts, prepares, fulfils, or cancels eligible orders.
- Verified email contacts, addresses, optional office PINs, hours and availability.
- Multi-item baskets, append-only ledger, partial receipts, reversals and statements.
- Per-stall Razorpay Test checkout, capture verification, refunds and durable recovery.
- Server pricing, tenant scoping, persistent orders, atomic credit checks,
  idempotency, version checks, and state-change events.

Configure email delivery and each stall's Razorpay Test account before real provider
testing. Missing configuration fails closed; live keys are blocked. Marketing and
recurring tea rounds remain future phases. Fixtures do not prove commercial readiness.

## Build on Windows through WSL

Prerequisites: Linux C++20 compiler, CMake 3.20+, Python 3, Crow headers,
standalone Asio headers, SQLite3, OpenSSL and libcurl development packages.
The current machine already has usable dependencies under the sibling
FoodService project. The build script reads them without modifying that project.
No dependencies are downloaded automatically.

From PowerShell:

```powershell
wsl --exec bash -lc 'cd /mnt/c/Users/Pankil/Documents/ChatGPT/ChayRound && bash scripts/build.sh'
wsl --exec bash -lc 'cd /mnt/c/Users/Pankil/Documents/ChatGPT/ChayRound && python3 scripts/dev.py'
```

Open **http://localhost:9180**. Keep the launcher terminal running; Ctrl+C stops
its three children. Merchant uses 9181, Order uses 9182, Gateway uses 9180. All
bind to loopback. The launcher refuses occupied ports. Data and logs are ignored
under `.run/`. Tests use unrelated temporary databases and ephemeral ports.

For an independent checkout, point `CHAY_DEPS_PREFIX` at your installed CMake
package prefix and `CROW_INCLUDE_DIR` at the directory containing `crow.h`, then
run `bash scripts/build.sh`. A system package installation may use `/usr` as the
prefix. The current dependency reuse is an explicit portability limitation.

## Try the product

The default customer flow is now **find stall -> enter office name -> owner
approves this browser -> order**. Offices need no email code, PIN or private link.
Return visits on that browser remember access. Owners use `/#owner`, save an
office's address/terms, then approve its browser request under **Office access**.
See [simple office ordering](docs/OFFICE_ACCESS.md) for current steps and limits.
The steps below describe this default workflow; legacy link APIs remain available.

1. Open `/#owner`, register/sign in and verify the owner email.
2. Set service area/hours, unpause ordering and add tea items/prices.
3. Save an office with its name/address, matching area and credit/prepaid terms.
   No office email or PIN is required.
4. In a separate customer browser/profile, open `/`, find the stall and enter the
   office name. Send an access request.
5. In the owner window, confirm the requester, select the saved office under
   **Office access** and approve the browser.
6. The customer page opens its menu. Return visits on the same browser remember
   access. Monthly orders use approved credit; prepaid orders need Razorpay Test
   account configuration and captured payment.
7. Accept -> Ready -> Fulfilled. Monthly fulfilment creates an account charge.
8. Open Account & statements to record confirmed CASH/BANK receipts and inspect
   balances. Remove a browser's access from the owner queue if needed.

Legacy links are bearer capabilities: anyone with a link can use that office's portal.
Rotating a link invalidates the old one. Links use a URL fragment and tokens
travel in a header, reducing URL-log exposure. Owner sessions are stored in
sessionStorage for this prototype; production needs stronger browser/session
hardening. The seller queue refreshes while open; sound is opt-in and foreground-
dependent. Server payment/refund recovery runs independently of the browser.

## Documentation

- [Project status and exact next action](PROJECT_STATUS.md)
- [Product phases](docs/PRODUCT_PHASES.md)
- [Architecture and tradeoffs](docs/ARCHITECTURE.md)
- [HTTP API](docs/API.md)
- [Provider setup and operations](docs/OPERATIONS.md)
- [Phase 1/2 acceptance and external work](docs/PHASE_1_2_ACCEPTANCE.md)
- [Interview study index](docs/interview/README.md)
- [Concept-to-code map](docs/interview/CODE_MAP.md)
- [System design walkthrough](docs/interview/SYSTEM_DESIGN.md)
- [C++ patterns and low-level design](docs/interview/CPP_AND_PATTERNS.md)
- [Interview practice](docs/interview/PRACTICE.md)
- [Financial workflows and recovery study](docs/interview/FINANCIAL_WORKFLOWS.md)

Source repository: [Pankil-Mevada/ChayRound](https://github.com/Pankil-Mevada/ChayRound).
No service is deployed, and no commercial readiness or interview outcome is guaranteed.
