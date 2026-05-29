# Banana Pi BPI-F3 — Installed Packages Log

All packages installed on the board (192.168.1.50) are tracked here.
This is required by the lab supervisor for audit and reproducibility.

## Packages

| Date | Package | Version | Installed By | Reason |
|------|---------|---------|--------------|--------|
| 2026-05-28 | `linux-tools-6.6.63` | 6.6.63-2.2.7.3 | fraidakisg | Provides `perf` for hardware performance counter profiling (`perf_event_open` userspace cycles) |
| 2026-05-28 | `linux-tools-common` | 6.6.63-2.2.7.3 | (auto-dep) | Pulled in automatically as dependency of `linux-tools-6.6.63` |

## Side-Effect Upgrades

These were upgraded as a side effect of the `linux-tools` install (same apt transaction):

| Date | Package | From Version | To Version |
|------|---------|--------------|------------|
| 2026-05-28 | `linux-image-6.6.63` | 6.6.63-2.2.6.3 | 6.6.63-2.2.7.3 |
| 2026-05-28 | `linux-headers-6.6.63` | 6.6.63-2.2.6.3 | 6.6.63-2.2.7.3 |
| 2026-05-28 | `linux-libc-dev` | 6.6.63-2.2.6.3 | 6.6.63-2.2.7.3 |
