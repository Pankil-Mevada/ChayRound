# Product roadmap

All statuses refer to actual code, not an approved commercial release.

| Phase | Scope | Current status |
| --- | --- | --- |
| 0 | Observe stalls/offices, test a specific paid offer | Business validation pending |
| 1 | Owner workspace, office access, menu and ordering prototype | Engineering implemented and locally verified; actual provider/device acceptance pending |
| 2 | Prepaid/monthly billing, ledger, refunds and controlled paid pilot | Engineering implemented and locally verified; merchant/provider validation and paid pilot pending |
| 3 | Public stall pages, invitations/QR, office leads and repeat orders | Not implemented |
| 4 | Recurring rounds, reminders, additional settlement schedules, opt-in campaigns | Not implemented |
| 5 | Advance balances, referrals, loyalty, branches and advanced reporting | Not implemented |

Multi-item orders use server prices and snapshots. Monthly offices reserve credit;
fulfilled charges support partial receipts, allocations, statements and corrections.
New offices default to PREPAID. Prepaid requests require configured per-stall
Razorpay Test accounts and canonical capture verification before preparation.
Live payment keys are blocked. No provider credentials means no payment success.

See [the acceptance checklist](PHASE_1_2_ACCEPTANCE.md) for what is implemented,
what was tested, and what still needs real provider/device/pilot participation.

Owner subscription is a hypothesis; office ordering is intended to be free.
Downloads do not create revenue. Software can supply marketing materials and
automate consented follow-ups later, but initial outreach still needs effort.
Ads, printing, discounts, messaging and referral rewards have separate costs.

The full planning documents are copied under `docs/planning/`. They are product
intent, not evidence that a feature is implemented. Current source and
PROJECT_STATUS.md take precedence for implementation status.
