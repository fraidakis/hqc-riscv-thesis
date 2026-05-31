# SSH Setup for BPI-F3

The benchmark runner (`scripts/bench/run_bench.sh --platform hw`) connects to the
Banana Pi BPI-F3 board over SSH. Two authentication methods are supported:

| Method | Security | Setup | Performance |
|--------|----------|-------|-------------|
| SSH keys (recommended) | Good | One-time `ssh-copy-id` | No overhead per call |
| sshpass fallback | Poor | Automatic if keys absent | ~50ms overhead + password in procfs |

## Recommended: Key-Based Authentication

Run once from your development machine:

```bash
# Copy your public key to the board (uses password from credentials file)
sshpass -p bianbu ssh-copy-id -o StrictHostKeyChecking=no root@192.168.1.50
```

Or interactively (will prompt for password):

```bash
ssh-copy-id root@192.168.1.50
```

After this, `run_bench.sh --platform hw` will use key auth directly — no `sshpass`
needed at runtime.

### Verify it works

```bash
ssh -o BatchMode=yes root@192.168.1.50 echo ok
```

If this prints `ok` without a password prompt, you're set.

## Fallback: sshpass (automatic)

If key auth is not set up, the script automatically falls back to `sshpass` using
the password from `docs/hardware/BPI-F3/bpi-f3-credentials.env`. It will print a
reminder:

```
[ssh] Key auth unavailable — using sshpass fallback.
      Run: ssh-copy-id root@192.168.1.50  (one-time setup)
```

Requirements for the fallback:
- `sshpass` must be installed (`sudo apt install sshpass`)
- The credentials file must exist with a valid `BPI_PASS` value

## How the script selects transport

```
1. Try: ssh -o BatchMode=yes <remote> true
   ├── Success → use plain SSH (key auth)
   └── Fail → check for sshpass + password
       ├── Both available → use sshpass (warn user)
       └── Missing → ERROR: exit with setup instructions
```

## Security notes

- **sshpass** exposes the password in `/proc/<pid>/cmdline` — any local user can
  see it. This is acceptable for a lab dev board with a default password, but
  should not be used for sensitive hosts.
- The credentials file (`bpi-f3-credentials.env`) is gitignored and never committed.
- SSH keys are the proper solution. The sshpass path exists only for
  out-of-the-box convenience before keys are configured.

## Default connection parameters

When `--remote` is not specified, the script reads from the credentials file:

| Variable | Source | Default |
|----------|--------|---------|
| Host | `BPI_HOST` in credentials file | 192.168.1.50 |
| User | `BPI_USER` in credentials file | root |
| Password | `BPI_PASS` in credentials file | (gitignored) |

The effective remote becomes `${BPI_USER}@${BPI_HOST}`, so you can change the
board's IP by editing the credentials file — no script changes needed.
