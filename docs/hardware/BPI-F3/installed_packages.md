# Banana Pi BPI-F3 — Installed Packages Log

All packages installed on the board (192.168.1.50) are tracked here.
This is required by the lab supervisor for audit and reproducibility.

## Packages

| Date | Package | Version | Installed By | Reason |
|------|---------|---------|--------------|--------|
| 2026-05-28 | `linux-tools-6.6.63` | 6.6.63-2.2.7.3 | fraidakisg | Provides `perf` for hardware performance counter profiling (`perf_event_open` userspace cycles) |
| 2026-05-28 | `linux-tools-common` | 6.6.63-2.2.7.3 | (auto-dep) | Pulled in automatically as dependency of `linux-tools-6.6.63` |
| 2026-06-22 | `clang-20` | 1:20.1.2-0ubuntu1 | fraidakisg | Native Clang compiler on BPI-F3 for hardware compiler comparison benchmarks (`run_compiler_compare.sh --variant <name>`) |
| 2026-06-22 | `clang-tools-20` | 1:20.1.2-0ubuntu1 | (auto-dep) | Pulled in automatically as dependency of `clang-20` |
| 2026-06-22 | `libclang1-20` | 1:20.1.2-0ubuntu1 | (auto-dep) | Pulled in automatically as dependency of `clang-20` |
| 2026-06-22 | `libclang-cpp20` | 1:20.1.2-0ubuntu1 | (auto-dep) | Pulled in automatically as dependency of `clang-20` |
| 2026-06-22 | `libclang-common-20-dev` | 1:20.1.2-0ubuntu1 | (auto-dep) | Pulled in automatically as dependency of `clang-20` |
| 2026-06-22 | `libclang-rt-20-dev` | 1:20.1.2-0ubuntu1 | (auto-dep) | Pulled in automatically as dependency of `clang-20` |
| 2026-06-22 | `libobjc4` | 15-20250404-0ubuntu1 | (auto-dep) | Pulled in automatically as dependency of `clang-20` |
| 2026-06-22 | `libobjc-14-dev` | 14.2.0-19ubuntu2bb3 | (auto-dep) | Pulled in automatically as dependency of `clang-20` |
| 2026-06-22 | `lld-20` | 1:20.1.2-0ubuntu1 | fraidakisg | LLVM linker for Clang LTO support (`-fuse-ld=lld`). GNU ld's `LLVMgold.so` is broken (dangling symlink in `llvm-20-linker-tools`), so `lld` is required for Clang `-flto` to work. |

## Side-Effect Upgrades

These were upgraded as a side effect of the `linux-tools` install (same apt transaction):

| Date | Package | From Version | To Version |
|------|---------|--------------|------------|
| 2026-05-28 | `linux-image-6.6.63` | 6.6.63-2.2.6.3 | 6.6.63-2.2.7.3 |
| 2026-05-28 | `linux-headers-6.6.63` | 6.6.63-2.2.6.3 | 6.6.63-2.2.7.3 |
| 2026-05-28 | `linux-libc-dev` | 6.6.63-2.2.6.3 | 6.6.63-2.2.7.3 |

## Manual Configuration

| Date | Action | Command | Reason |
|------|--------|---------|--------|
| 2026-06-22 | Register `clang` alternative | `update-alternatives --install /usr/bin/clang clang /usr/bin/clang-20 100` | `clang-20` installs only as `/usr/bin/clang-20`; `Makefile.hw` and bench scripts invoke it as `clang`, so a default alternative was registered. |
| 2026-06-22 | Register `ld.lld` alternative | `update-alternatives --install /usr/bin/ld.lld ld.lld /usr/bin/ld.lld-20 100` | `lld-20` installs only as `/usr/bin/ld.lld-20`; Clang's `-fuse-ld=lld` looks for `ld.lld` on PATH. |
