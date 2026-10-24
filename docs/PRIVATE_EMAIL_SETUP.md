# Private local email setup

Customers can register Gmail, Outlook, Yahoo and business addresses. ChayRound
uses one Resend sender to deliver their verification codes. This does not require
access to customer inboxes or their email passwords.

1. In your own browser, create a Resend account, verify a domain you own and create
   a sending API key. Follow https://resend.com/docs/dashboard/domains/introduction
   and https://resend.com/docs/api-reference/emails/send-email.
2. Stop any existing ChayRound development supervisor with Ctrl+C.
   If startup says it is already running, stop the original launcher first.
   Do not delete `stack.lock`: the active process owns the lock, not the filename.
3. Open your own Windows Terminal, outside the chat and any AI-managed terminal.
   Run:

   ```powershell
   wsl --exec bash -lc 'cd /mnt/c/Users/Pankil/Documents/ChatGPT/ChayRound && python3 scripts/private_email_start.py'
   ```

4. Enter the API key and verified sender at the hidden prompts. Do not paste them
   into chat, screenshots or shared terminal recordings. No credential values are
   command-line arguments or saved by this launcher; re-enter them after stopping.
5. Open http://localhost:9180 and request a verification code from the owner portal.
   Confirm receipt in your own mailbox. Report only success or a redacted error.
   Ctrl+C stops the services through the existing supervisor.

The launcher refuses noninteractive input and getpass's echoed fallback. It clears
automated fixture overrides and uses the existing real-email/Test-payment startup.
It does not create a Resend account, configure DNS, or prove real email delivery.
It does not configure Razorpay; existing per-stall Test account configuration still
applies. Without a verified sender/key, email verification remains unavailable.

Credentials live in server process environments while running. Only Merchant
receives the email key/sender from the supervisor. The assistant must not inspect
those environments, process memory or your private terminal. Hidden prompts keep
values out of chat output; they are not an absolute access barrier against software
running as the same OS user or an administrator. For that requirement, run the
credential-bearing application under a separate restricted OS account or machine
outside the agent's access. This helper is local development convenience, not a
production secret vault.
