# Latest local results

Date: 24 October 2026. Environment: WSL, GCC 13.3, C++20, Debug CMake build.

| Check | Result |
| --- | --- |
| merchant, order, gateway, domain_test build | Passed |
| CTest | 1/1 passed |
| Disposable-database HTTP integration | 32 checks passed |
| Inline frontend JavaScript syntax | Passed with Node |
| Browser owner -> office -> fulfilment flow | Passed with synthetic local data |
| Browser error-log inspection after flow | No error entries returned |

Reproduce from WSL at the project root:

```bash
bash scripts/build.sh
# Or, after building:
ctest --test-dir build --output-on-failure
python3 tests/e2e.py
```

The build script's dependency defaults use the pre-existing sibling FoodService
installation. See README for overrides. Integration tests start their own three
processes on ephemeral loopback ports, use TemporaryDirectory databases, stop all
children, and remove test data. They do not alter the running .run/ preview.

The suite verifies concurrency and recoverability for this slice, not commercial
billing, real payment handling, end-to-end delivery or production capacity. There
is no live provider integration to test yet. Mobile browser behaviour, accessibility
audit, load/soak tests and backup/restore drills remain pending.
