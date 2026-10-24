# Latest local verification

Date: 24 October 2026. WSL, GCC 13.3, C++20, Debug CMake build.

| Check | Actual result |
| --- | --- |
| Gateway, Merchant, Order, domain_test build | Passed |
| CTest | 1/1 passed |
| Phase 1/2 HTTP/concurrency/provider-contract/recovery | 252 checks passed |
| Legacy migration and repeat startup | 9 invariants passed |
| JavaScript syntax / Python compilation | Passed |
| Browser verification, basket, fulfilment, partial receipt and account | Passed with synthetic data |
| Browser error-log inspection | No error entries returned |
| Responsive width check | 375 px content / 375 px viewport |

Reproduce inside WSL from the project root:

```bash
BUILD_JOBS=2 bash scripts/build.sh
# Individual checks after compilation:
ctest --test-dir build --output-on-failure
python3 tests/e2e.py
python3 tests/migration_test.py
node --check frontend/app.js
```

Integration tests start isolated processes/ports with disposable databases and a
separate email/Razorpay contract server under tests/. Worker recovery is tested
explicitly against that server. Network response loss, concurrent requests,
changed contacts, invalid capture/signature/amount, missing configuration, live
key rejection, equal-time pagination and non-destructive restore are covered.
No external provider, real email, live payment or user production data is used.

Browser QA uses tests/browser_fixture.py on separate 9280-9282 ports and temporary
data. It is never used by normal scripts/dev.py. The real Razorpay popup is not
tested with fixture keys; actual Test credentials and provider setup are required.

These results are engineering evidence, not real-provider certification, actual
phone testing, production capacity or a paid pilot. See the acceptance checklist
and operations runbook for unresolved external validation.
