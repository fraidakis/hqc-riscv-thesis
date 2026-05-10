# Dropped Experiments and Methodology Decisions

## LLVM Polly (Dropped)
LLVM Polly experiments were shelved.

Reason:
- The custom RISC-V LLVM 21 toolchain required for the `zvbc` extension does not support Polly in a usable way for this project setup.
- We therefore rely on manual intrinsics and hand-tuned implementation paths.

## BINSEC Symbolic Execution (Dropped)
BINSEC integration was shelved.

Reason:
- HQC introduces large symbolic execution overhead, especially with deeply recursive and transform-heavy flows such as FAFFT.
- The resulting state-space explosion makes this path impractical for the thesis timeline.
- Constant-time confidence is tracked through dudect-based workflows.

## Timing Analysis and Emulation Policy
Spike/QEMU timing was deemed invalid for side-channel timing analysis.

Reason:
- Emulation introduces timing artifacts and microarchitectural mismatch relative to physical hardware.
- For timing-focused leakage checks, we require native execution on physical silicon.

Current policy:
- `zbc` scalar variants: cross-compile dudect binaries and run them natively on Banana Pi.
- `zvbc` vector variants: deferred to future work due to lack of compatible physical silicon.
- Host-native reference path remains useful for functional and workflow checks, not as final silicon-timing evidence.
