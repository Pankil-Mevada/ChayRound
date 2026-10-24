# ChayRound

C++20 microservices and a dependency-free web portal for independent tea stalls
and their office clients. This is a local development prototype, not a commercial
release. Build the product and study its design together.

## Working slice

- Register/sign in as an owner and create menu items.
- Create an office with prepaid or monthly terms and a credit limit.
- Open a private office link without daily login; place monthly-credit orders.
- Seller accepts, prepares, fulfils, or cancels eligible orders.
- Server pricing, tenant scoping, persistent orders, atomic credit checks,
  idempotency, version checks, and state-change events.

Prepaid ordering is deliberately blocked. Real payments, contact verification,
monthly invoices, settlement recording, policy editing, marketing and recurring
orders are not implemented. Outstanding orders stay unpaid. Do not use real
customer data or collect real money in this prototype.

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

1. Create an owner account (password of at least 12 characters), then sign in.
2. Add Regular tea at Rs 15.
3. Add ABC Office, choose MONTHLY, and set a Rs 2,000 limit.
4. Copy the private link shown at creation; open it in another browser tab.
5. Request two teas. Refresh the owner's desk to see the request.
6. Accept, mark Ready, then Fulfilled. Refresh the office view for status.
7. Try a prepaid office: ordering is unavailable, rather than pretending to pay.

Links are bearer capabilities: anyone with a link can use that office's portal.
Rotating a link invalidates the old one. Links use a URL fragment and tokens
travel in a header, reducing URL-log exposure. Owner sessions are stored in
sessionStorage for this prototype; production needs stronger browser/session
hardening. There is no automatic background notification or live polling yet.

## Documentation

- [Project status and exact next action](PROJECT_STATUS.md)
- [Product phases](docs/PRODUCT_PHASES.md)
- [Architecture and tradeoffs](docs/ARCHITECTURE.md)
- [HTTP API](docs/API.md)
- [Interview study index](docs/interview/README.md)
- [Concept-to-code map](docs/interview/CODE_MAP.md)
- [System design walkthrough](docs/interview/SYSTEM_DESIGN.md)
- [C++ patterns and low-level design](docs/interview/CPP_AND_PATTERNS.md)
- [Interview practice](docs/interview/PRACTICE.md)

Source repository: [Pankil-Mevada/ChayRound](https://github.com/Pankil-Mevada/ChayRound).
No service is deployed, and no commercial readiness or interview outcome is guaranteed.
