# ChayRound engineering and study instructions

- Read PROJECT_STATUS.md, inspect Git status and recent commits, and verify claims
  against source before work. This is an independent project from FoodService.
- Backend is C++20/Crow under services/; shared C++ resource helpers under common/;
  dependency-free web UI under frontend/; tests under tests/.
- Use rg; exclude build*, .run and dependency installations for ordinary searches.
- Merchant owns merchant data; Order owns order/credit data. Never join service
  databases or trust browser-supplied owner/stall/office identities or prices.
- Preserve integer-paise totals, atomic credit reservations, office-scoped
  idempotency, legal versioned transitions and auditable events.
- Prepaid checkout must fail closed until real server-verified payments exist.
  Do not add fake payment success to the product. Tests use disposable DBs only.
- Run CTest and tests/e2e.py for backend changes. Test credentials are synthetic;
  do not use real customer data or provider credentials in automated fixtures.
- Update PROJECT_STATUS.md with implemented scope, actual results, limitations
  and the precise next action. Do not claim production readiness.
- Update docs/interview/ with concept, reason, code anchors, tradeoffs and test
  evidence for substantial features. Distinguish working code from planned design.
- Do not add Kafka/Redis/Kubernetes or design-pattern layers without a concrete
  need. Explain alternatives and costs as part of system-design learning.
- Never commit databases, logs, secrets, dependency binaries or build output.
