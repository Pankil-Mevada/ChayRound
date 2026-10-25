# Latest verification

Date: 25 October 2026, Asia/Calcutta. WSL, GCC 13.3, C++20, Debug CMake build.

| Check | Actual result |
| --- | --- |
| Clean build of Gateway, Merchant, Order and domain test | Passed |
| Final incremental build after review fixes | Passed |
| CTest | 1/1 passed |
| Expanded HTTP/money/permissions/automation/provider/recovery suite | 503 checks passed |
| Legacy migration and repeat startup | 9 invariants passed |
| Private launcher/lock tests | 4 passed |
| Existing office browser UI scenarios | 7 passed |
| New JavaScript syntax checks | Passed |
| Fresh headless Edge owner/office workflow | Passed |
| Browser page/console errors in final run | None |
| Mobile layout | Document width fits 375px viewport |

The new real-browser test covers synthetic registration/email verification,
owner setup, public profile and access approval, literal-text message safety,
private two-way conversation, monthly-credit order fulfilment, repeat basket
reset, business reports, a generated QR invitation, the disabled money gate and
mobile layout. Local screenshots are generated under ignored
`.run-qa/screenshots/`; they are not production/customer evidence.

The first browser run caught assigning the read-only textarea type property in
the dynamic form builder. The fix is included. A subsequent run identified the
missing favicon; the app now serves its own SVG icon. Final browser QA passed.
Final financial review added cross-ledger receipt-reference checks so the same
confirmed money reference cannot fund both credit settlement and advance funds.

Reproduce backend checks inside WSL, with configured dependency paths:

```bash
BUILD_JOBS=2 bash scripts/build.sh
node --check frontend/app.js
node --check frontend/communication.js
node --check frontend/business.js
node tests/portal_ui_test.cjs
```

For the new headless browser test, start `python3 tests/browser_fixture.py` in WSL.
Run `node tests/browser_phase345.cjs <printed-synthetic-mailbox-path>` on Windows
with Playwright resolvable through NODE_PATH and Edge available. Stop the fixture
with Ctrl+C. The script explicitly requires a disposable browser fixture path.
Never use real credentials or a production stack for this test.

All providers, emails, identities, review IDs, money records, data directories and
ports used by automated tests are synthetic/disposable. No real email, WhatsApp,
SMS, payments, ads, bank transfers or external provider calls were made. The real
Razorpay popup, phones, paid pilot and production capacity remain unvalidated.
See docs/PHASE_3_5_ACCEPTANCE.md and docs/PILOT_CHECKLIST.md.
