# Study system design through ChayRound

Goal: explain a real system, its invariants, implementation and tradeoffs in
large-tech-company interviews. No document can guarantee interview success.
Practice algorithms, data structures and behavioural interviews separately.

## Suggested study sequence

| Session | Read and do | Explain without notes |
| --- | --- | --- |
| 1 | Run README flow; read PRODUCT_PHASES and API | Requirements, scope, actors and non-goals |
| 2 | Follow CODE_MAP and one HTTP request | What each service owns; how authority crosses boundaries |
| 3 | Read C++ resource wrappers and domain test | RAII, exception safety and rollback |
| 4 | Trace POST orders; run concurrency E2E | Credit races, idempotency and transaction boundaries |
| 5 | Trace status updates and version checks | Legal state transitions and stale writes |
| 6 | Read SYSTEM_DESIGN and ARCHITECTURE | Estimates, bottlenecks, failure and scale evolution |
| 7 | Answer PRACTICE prompts and draw the design | Alternatives, limitations and evidence |

For each future feature add: product reason, invariant, design alternatives,
chosen code locations, test evidence, operational risks and an interview prompt.
Do not describe a planned broker, cache or ledger as implemented.

Phase 1/2 now includes the ledger and payment adapter. Read
[Financial workflows and recovery](FINANCIAL_WORKFLOWS.md) for current code anchors,
partial-payment examples, compensation, provider idempotency and durable worker
design. Earlier study examples are conceptual; current implementation status is
in PROJECT_STATUS.md and docs/PHASE_1_2_ACCEPTANCE.md.

## Primary references

See [Owner-approved office browsers](DEVICE_APPROVAL.md) for the current simpler
onboarding, tenant scoping, capability grants and revocation tradeoffs.

- [Crow routing and HTTP framework](https://crowcpp.org/master/)
- [SQLite transactions and single-writer semantics](https://www.sqlite.org/lang_transaction.html)
- [OpenSSL PBKDF2 API](https://docs.openssl.org/3.6/man3/PKCS5_PBKDF2_HMAC/)
- [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines)
- [libcurl timeouts](https://curl.se/libcurl/c/CURLOPT_TIMEOUT_MS.html)

Read the repository's actual code first. Library documentation explains API
behaviour; it is not evidence of this application's throughput or security.
