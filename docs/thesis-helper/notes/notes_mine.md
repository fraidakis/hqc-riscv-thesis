- Διαθεσιμοτητα του καθε extension? eg zvbc and Zvbb? 
- Zbb, V, Zvbb part of the RVA23 standard!

---

# Architectural Comparison: SHA-2 vs. SHA-3 Hardware Acceleration 
## ARMv8.2-A vs. RISC-V

### 1. The Mature State of SHA-2
* **RISC-V (`Zknh` Extension):** Fully mature. RISC-V features explicit, dedicated instructions (like `sha256sig0`) exclusively built to calculate SHA-256 and SHA-512 hashes. 

### 2. The Current State of SHA-3 (Keccak)
* **ARMv8.2-A (Dedicated Approach):** ARM introduced explicit instructions (`EOR3`, `RAX1`, `XAR`, `BCAX`) via its Cryptographic Extension. These are highly specialized macro-operations designed to fuse Keccak's heavy mathematical steps (Theta, Rho, Pi, Chi) into single clock cycles. 
* **RISC-V (General Purpose Approach):** Currently, RISC-V **lacks** dedicated SHA-3 instructions. Instead, it relies on general-purpose Bit Manipulation extensions:
    * **Scalar (`Zbkb`) & Vector (`Zvkb` / `Zvbb`):** These provide fast rotations (`vror`), AND-NOT operations (`vandn`), and rapid register-reordering (`vrgather`). 
    * **The Trade-off:** ARM's instructions act as "ASICs" (incredibly fast for hashing but useless elsewhere). RISC-V's bit manipulation instructions are highly flexible and can be reused for general computing tasks like data compression or networking.

### 3. RISC-V Vector Limits & Parallel Hashing
* To maximize SHA-3 throughput without dedicated instructions, RISC-V utilizes Vector Instruction sets (RVV) to process multiple hashes simultaneously (e.g., 2-way Structure of Arrays for Post-Quantum algorithms).
* **The 4-Way Bottleneck:** Scaling this to 4-way parallel hashing on standard hardware (VLEN=128) is unfeasible due to **Register Pressure**. The Keccak state requires 25 variables. Since RISC-V has exactly 32 vector registers, forcing 4 parallel states causes "Register Spilling" (writing/reading from slower RAM), which destroys performance. The 2-way execution is the architectural "sweet spot."

### 4. The Future of RISC-V & SHA-3
* Driven by the heavy SHA-3 demands of **Post-Quantum Cryptography (PQC)** algorithms (which require thousands of parallel hashes), the RISC-V PQC Task Group is actively developing dedicated Keccak/SHA-3 instructions. This upcoming extension will bridge the gap, bringing RISC-V to parity with ARM's explicit hardware acceleration.