# Documentation for `vendor/hqc-ref/`

## 1. High-Level Analysis

### Architectural Overview of the `src` Directory
The `src` directory constitutes the core implementation of the official Hamming Quasi-Cyclic (HQC) algorithm. HQC is a code-based Key Encapsulation Mechanism (KEM) designed to resist quantum computer attacks. The `src` directory strictly separates platform-independent abstractions (`common/`), a portable reference implementation (`ref/`), and an optimized hardware-specific implementation (`x86_64/`).

The hierarchy is modularized by mathematical scope and functional boundaries: 
- **Configuration & Types**: Parameters for different security levels (HQC-128, 192, 256, mapped as HQC-1, 3, 5).
- **Core Arithmetic**: Galois Field ($GF(2^m)$) operations, bit-vector utilities, and polynomial multiplication ($GF(2)[X]$).
- **Error Correcting Codes**: The concatenated code approach utilizing a Reed-Solomon (RS) outer code and a Reed-Muller (RM) inner code.
- **Symmetric Primitives**: Cryptographically secure pseudo-random number generation (PRNG) and hashing leveraging SHAKE256 (from FIPS 202).
- **Public Key Encryption (PKE) and KEM**: The mathematical `HQC` implementation and the overarching KEM wrapper conforming to NIST interfaces.

### HQC Cryptographic Flow and Module Interaction
The cryptographic objective of HQC is to allow two parties to securely establish a shared symmetric key over an insecure channel. The process flows top-down as follows:

1. **Key Generation (`crypto_kem_keypair` -> `hqc_pke_keygen`)**:
   - The algorithm generates a random seed securely (`randombytes`).
   - Using the symmetric primitive (SHAKE256 acting as a PRNG), it expands the seed to generate vectors $h$, $x$, and $y$ belonging to the polynomial ring $\mathcal{R}$.
   - The public key is forming by a public seed and a syndrome polynomial $s = x + h \cdot y$. The secret key constitutes the vectors $(x, y)$.

2. **Key Encapsulation (`crypto_kem_enc` -> `hqc_pke_encrypt`)**:
   - The sender picks a random message $m$ of $K$ bits.
   - A symmetric hash of $m$ is mixed with the public key to deterministically derive an encryption seed.
   - Using this seed, error vectors $e, r_1, r_2$ are generated.
   - The message $m$ is mapped to a concatenated codeword $c$ using **Reed-Solomon** and **Reed-Muller** encoding (`code.c`).
   - The ciphertext $(u, v)$ is calculated as $u = r_1 + h \cdot r_2$ and $v = m \cdot G \oplus s \cdot r_2 \oplus e$.
   - A shared session key $K$ is hashed from $m$ and the ciphertext using SHAKE256.

3. **Key Decapsulation (`crypto_kem_dec` -> `hqc_pke_decrypt`)**:
   - The receiver computes $v - u \cdot y = m \cdot G \oplus e - r_1 \cdot y + e \cdot y$.
   - This isolated bit-stream is passed through the **Decoder** (`code.c`), which runs **Reed-Muller** decoding to correct heavy noise, followed by **Reed-Solomon** decoding to patch residual errors, recovering $m$.
   - A re-encryption check verifies the validity of the ciphertext (implicitly proving CCA security via the Fujisaki-Okamoto transform). 
   - A shared session key $K$ is re-hashed.

### Surrounding Directories within `vendor/hqc-ref/`
Outside of `src/`, the directory contains necessary scaffolding for tests, compliance, and builds:
- **`lib/`**: Contains required 3rd-party dependencies. Specifically, `fips202/` provides the SHAKE functions for symmetric hashing and PRNG.
- **`tests/`**: Includes C files combining `munit` specifically to test individual modules (like RS/RM codecs, polynomial multiplication, KAT generation) ensuring functional correctness when code changes are made.
- **`kats/`**: "Known-Answer Tests." Stores standard NIST-format request (`.req`) and response (`.rsp`) files, supplying fixed seed and expected PKE/KEM outputs validating that the implementation adheres directly to the theoretical spec natively and across refactoring.
- **`packaging/`**: Houses utility scripts and Doxygen configuration files for generating documentation and bundling releases.
- **`CMakeLists.txt` & Build Configuration**: Drives project compilation, supporting varying architectures (`ref` vs `x86_64`), enabling target sanitizers, and generating test binaries.

---

## 2. Low-Level Analysis: File-by-File Breakdown of `src`

This section traces exactly how the project is built starting from underlying constant values, climbing up primitive mathematics, and closing with heavy cryptography.

### Phase 1: Parameter Setup & Base Data Types
These files establish memory formats and boundaries of the scheme.
- **`src/common/hqc-[1,3,5]/api.h`**
  - Defines the public NIST KEM API constants: `CRYPTO_SECRETKEYBYTES`, `CRYPTO_PUBLICKEYBYTES`, `CRYPTO_BYTES` (shared key length), and `CRYPTO_CIPHERTEXTBYTES` for different PQC security levels (128, 192, 256 bits).
- **`src/ref/hqc-[1,3,5]/parameters.h` & `src/x86_64/common/hqc-[1,3,5]/parameters.h`**
  - Holds the hardcoded mathematical boundaries specific to the security parameter: vector length ($N$), error weights ($W_e, W_{r1}, W_{r2}$), and dimensions for the Reed-Solomon and Reed-Muller codes.
- **`src/ref/data_structures.h` & `src/x86_64/common/data_structures.h`**
  - Exposes C-struct definitions defining `public_key`, `secret_key`, and `ciphertext`. Often utilized for passing references rather than massive raw byte arrays across core algorithmic functions.

### Phase 2: Memory & Cryptographic Primitives
These abstract underlying operations securely.
- **`src/common/crypto_memset.c` & `.h`**
  - Implements a compiler-proof constant-time memory zeroing utility. Protects against optimizing compilers discarding memory wipes, ensuring that transient secret states are effectively destroyed post-computation.
- **`src/common/symmetric.c` & `.h`**
  - The deterministic randomness powerhouse. Wraps `fips202` (SHAKE256).
  - *Primary Functions:* `hqc_prng_init()`, `hqc_prng_randombytes()` for extracting sequences. Uses standard Keccak state structures. `hqc_hash` is abstracted out directly using SHAKE to collapse objects into uniform hashes.

### Phase 3: Bit Manipulation and Byte Parsing
Conversion of cryptographic polynomials into network-readable bytes.
- **`src/ref/vector.c` & `.h` (Mirror: `src/x86_64/avx256/vector.c`)**
  - Utility for packing arrays of elements into tightly bound bit-arrays (required for memory conservation during code computation) and providing logic for cyclic shifts or sparse vector instantiations.
  - *Key feature:* Provides sub-functions to generate error arrays ensuring the target sparse Hamming weight is consistently achieved.
- **`src/ref/parsing.c` & `.h` (Mirror: `src/x86_64/common/parsing.c`)**
  - Converts human/math-readable data structures (`public_key`, `secret_key`, `ciphertext`) into raw byte-streams array equivalents required for KEM API outputs.
  - `hqc_public_key_to_string` loads struct bounds iteratively into the byte-buffer, whilst `hqc_public_key_from_string` reverses it safely.

### Phase 4: Finite Field Mathematics
The operations processing the underlying binary rings over GF(2).
- **`src/ref/gf.c` & `.h` (Mirror: `src/x86_64/avx256/gf.c`)**
  - Implements Galois Field $\mathbb{F}_{2^m}$ primitives heavily used by the Reed-Solomon code.
  - Generates exp and log tables representing finite field constants (`gf_generate()`) and executes additions (`gf_add`) and multiplications (`gf_mul`).
  - Contains modular reductions matching the specific HQC irreducible polynomials.
- **`src/ref/gf2x.c` & `.h` (Mirror: `src/x86_64/common/hqc-[1,3,5]/gf2x.c`)**
  - Core Polynomial Math. Allows multiplication of two elements in the ring $GF(2)[X]/(X^N - 1)$.
  - *Primary Function:* `vect_mul()`. Evaluates polynomial accumulation. In reference terms, this typically utilizes bit-interleaved shift-and-add (Karatsuba or operand scanning). In x86, intrinsics explicitly parallelize it via VPCLMULQDQ instructions.
- **`src/common/fft.c` & `.h`**
  - Advanced Fast Fourier Transform logic mapping polynomials across domains to significantly compress polynomial multiplication asymptotic complexity. Used commonly by advanced optimized routines bypassing standard slow degree limits.

### Phase 5: The Concatenated Code (Error Correction)
Protects against the engineered decryption failure rate of HQC.
- **`src/ref/reed_muller.c` & `src/common/reed_muller.h`**
  - The *Inner Code* layer mapping 1 bit into a much larger redundant word for extreme high-weight channel flip resilience.
  - `encode()` executes a Fast Hadamard Transform representation. `decode()` leverages a fast matching algorithm measuring correlations recursively against all RM codewords.
- **`src/ref/hqc-[1,3,5]/reed_solomon.h` & `src/ref/reed_solomon.c`**
  - The *Outer Code* layer. Corrects burst and lingering errors leftover by RM using Galois field abstractions from `gf.c`.
  - `encode()` drives cyclic polynomial reduction by generating generator polynomials.
  - `decode()` drives the Berlekamp-Massey algorithm, discovering error locator polynomials and computing Chien search/Forney formulas to isolate the physical bit locations to flip.
- **`src/common/code.c` & `.h`**
  - The unifier bridging both algorithms.
  - `code_encode(m, em)` reads message $m$, calls `reed_solomon_encode`, injects into `reed_muller_encode`, generating $em$.
  - `code_decode(em, m)` performs the reverse pass, allowing the KEM payload to survive the cyclic polynomial disruption.

### Phase 6: The Core Scheme
The integration of mathematics into functional cryptography.
- **`src/ref/hqc.c` & `.h` (Mirror: `src/x86_64/common/hqc.c`)**
  - Performs the actual math underlying the PKE scheme using building blocks.
  - `hqc_pke_keygen()` builds random $x, y, h$, generates public key vectors, packages them via `parsing.c`.
  - `hqc_pke_encrypt()` runs SHAKE hash, pulls uniformly random bit streams via `vector.c`, multiplies $h \cdot r_2$ using `gf2x.c`, encodes $m$ with `code.c`, and outputs a parsed packed sequence.
  - `hqc_pke_decrypt()` parses input strings into local structures, completes ciphertext vector additions using GF mathematics, calls the monolithic `code_decode()`, checks the checksum, and passes back identical plaintext hash targets.
- **`src/common/kem.c`**
  - The facade interface. Direct compliance wrapping for `crypto_kem_keypair`, `crypto_kem_enc`, and `crypto_kem_dec`.
  - Serves strictly to invoke the PKE functions defined above securely mapping user pointers and executing final layer cryptographic hash routines deriving $K$ (the final negotiated symmetric key).

### Optimization Context: The `x86_64` folder
While the `ref` folder contains portable standard C, the `src/x86_64` hierarchy mirrors its abstraction boundary point for point (`vector.c`, `gf.c`, `reed_muller.c`, `gf2x.c`). However, it drastically abandons theoretical readability in favor of 256-bit SIMD intrinsics (AVX2/AVX-512 memory payloads), unrolling array shifts and substituting native GF arithmetic implementations for high-scale enterprise hardware throughput.
