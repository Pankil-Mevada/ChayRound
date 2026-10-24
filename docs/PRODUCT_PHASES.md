# Product roadmap

All statuses refer to actual code, not an approved commercial release.

| Phase | Scope | Current status |
| --- | --- | --- |
| 0 | Observe stalls/offices, test a specific paid offer | Business validation pending |
| 1 | Owner workspace, office access, menu and ordering prototype | Initial working slice; verification, hours, availability and full basket remain |
| 2 | Prepaid/monthly billing, ledger, refunds and controlled paid pilot | Credit reservation only; real payments and billing not implemented |
| 3 | Public stall pages, invitations/QR, office leads and repeat orders | Not implemented |
| 4 | Recurring rounds, reminders, additional settlement schedules, opt-in campaigns | Not implemented |
| 5 | Advance balances, referrals, loyalty, branches and advanced reporting | Not implemented |

The prototype accepts a single menu item with a quantity per order. Monthly
offices reserve credit and all orders remain unpaid. New office creation defaults
to PREPAID in the web form; owners explicitly select monthly credit for the local
workflow. Prepaid offices cannot order until payment handling is implemented.

Owner subscription is a hypothesis; office ordering is intended to be free.
Downloads do not create revenue. Software can supply marketing materials and
automate consented follow-ups later, but initial outreach still needs effort.
Ads, printing, discounts, messaging and referral rewards have separate costs.

The full planning documents are copied under `docs/planning/`. They are product
intent, not evidence that a feature is implemented. Current source and
PROJECT_STATUS.md take precedence for implementation status.
