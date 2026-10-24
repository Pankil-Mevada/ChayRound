# Secret configuration and process boundaries

`scripts/private_email_start.py` uses `getpass` with fallback warnings promoted
to errors, rejects noninteractive input, and replaces itself with the existing
supervisor through `os.execve`. Credentials are not arguments or persisted files.
`scripts/dev.py` removes email configuration from Order/Gateway environments and
the payment account path from Merchant/Gateway. This reduces accidental exposure
across children while retaining the existing provider contracts.

Hidden input is a user-interface property, not an OS security boundary. The
supervisor still has the environment and same-user/admin processes may inspect
it. A deployment requiring isolation needs separately permissioned identities
and a managed secret service; those are not implemented here. Environment-based
configuration is simple for local testing but is not a production secret vault.

Tests: `tests/private_email_start_test.py` checks refusal of piped credentials,
invalid/control-character inputs and removal of test endpoints without mutating
the parent environment. These tests use synthetic values and do not prove real
email delivery or isolation. Existing CTest, 252 HTTP checks and nine migration
invariants also passed for this change.
