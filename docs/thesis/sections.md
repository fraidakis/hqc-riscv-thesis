# Πλήρης Διάρθρωση Διπλωματικής Εργασίας

## Προκαταρκτικές Σελίδες
* **Περίληψη (Ελληνικά)**
* **Abstract (Αγγλικά)**
* **Περιεχόμενα**

## Κεφάλαιο 1: Εισαγωγή
* **1.1 Η Κβαντική Απειλή και η Post-Quantum Κρυπτογραφία:** Ο αλγόριθμος του Shor και η αναγκαιότητα μετάβασης από τα κλασικά σχήματα (RSA, ECC) σε μετα-κβαντικά. Η στρατηγική "Harvest now, Decrypt later" και η εντολή NSA CNSA 2.0 (μετάβαση σε PQC έως 2035) ως πλαίσιο επιτακτικότητας.
* **1.2 Το Πλαίσιο Standardization του NIST και ο Αλγόριθμος HQC:** Παρουσίαση της διαδικασίας τυποποίησης (γύροι αξιολόγησης, ολοκληρωμένα πρότυπα ML-KEM/ML-DSA/SLH-DSA) και η τρέχουσα φάση του HQC ως τελικού υποψηφίου προς δημοσίευση.
* **1.3 Cryptographic Agility και Pure Software Acceleration:**
    * **1.3.1 Cryptographic Agility ως Απαίτηση:** Η ανωριμότητα του PQC τοπίου — αν αποδειχθεί ευπάθεια σε lattice-based σχήματα (π.χ. ML-KEM), tailor-made hardware καθίσταται άχρηστο. Concrete example: SIKE ήταν NIST Round 4 candidate, οργανισμοί ανέπτυσσαν FPGA accelerators, και σπάστηκε σε <1 ώρα σε laptop (2022) — κάθε hardware investment χάθηκε. Μέχρι να σταθεροποιηθεί η κατάσταση, απαιτούνται γενικές και ευέλικτες λύσεις.
    * **1.3.2 Pure Software Acceleration σε Ανοιχτή Αρχιτεκτονική:** Το RISC-V σχεδιάστηκε με τη φιλοσοφία "ISA extensions replace fixed-function accelerators" — ratified cryptographic extensions χωρίς vendor lock-in. Το RVA23 profile (Zbb, V, Zvbb mandatory) επιβεβαιώνει ότι οι extensions που αξιοποιούμε βρίσκονται στο mainstream adoption path. Πλεονέκτημα ευέλικτων ISA extensions έναντι εξειδικευμένων κυκλωμάτων. *(Η αρχιτεκτονική τεκμηρίωση των επεκτάσεων παρουσιάζεται στο Κεφάλαιο 3.)*
* **1.4 Σκοπός και Συνεισφορά της Διατριβής:** Δεν υπάρχει μέχρι σήμερα δημοσιευμένη υλοποίηση του HQC που να αξιοποιεί τα Zbc/Zvbc και λοιπά ratified RISC-V extensions — ούτε σε hardware accelerator ούτε σε resource-constrained περιβάλλον. Οι μοναδικές σχετικές εργασίες (12/2025–03/2026) αφορούν αποκλειστικά πολλαπλασιασμό πολυωνύμων σε AVX2/NEON, χωρίς κάλυψη πλήρους KEM. Η παρούσα διατριβή καλύπτει αυτό το κενό με end-to-end βελτιστοποίηση στο RISC-V. *Σημείωση: η εργασία εστιάζει σε performance optimization — constant-time ανάλυση αφήνεται ρητά ως μελλοντική εργασία (βλ. §8.3).*
* **1.5 Δομή της Εργασίας:** Σύντομη περιγραφή των επόμενων κεφαλαίων.

## Κεφάλαιο 2: Θεωρητικό Υπόβαθρο HQC και Κρυπτογραφίας
* **2.1 Code-Based Κρυπτογραφία και Key Encapsulation Mechanism (KEM) του HQC:** Εισαγωγή στην code-based κρυπτογραφία (πρόβλημα αποκωδικοποίησης τυχαίου γραμμικού κώδικα), η θέση του HQC στο πεδίο, security levels (128/192/256) και παράμετροι. Μαθηματική θεμελίωση και λειτουργική ανάλυση των διεργασιών Keygen, Encapsulation και Decapsulation. Υψηλού επιπέδου αρχιτεκτονική: ποιοι αλγοριθμικοί πυρήνες (πολυωνυμικός πολλαπλασιασμός, SHA-3, αποκωδικοποίηση) αποτελούν τα υπολογιστικά hotspots και γιατί.
    <!-- NOTE (συγγραφή): Κράτα compact — μη γίνεις textbook. -->
* **2.2 Πολυωνυμικός Πολλαπλασιασμός σε Δυαδικά Πεδία:** Ανάλυση της πράξης στον δακτύλιο $GF(2)[x] / (x^N - 1)$ και μαθηματική μελέτη του Karatsuba ως main hotspot.
    <!-- NOTE (συγγραφή): Μόνο τα μαθηματικά που χρειάζονται για τον Karatsuba decomposition — μηχανική, όχι αξιώματα. -->
* **2.3 Συναρτήσεις Hash SHA-3 / Keccak:** Αρχιτεκτονική σπόγγου (Sponge Construction) και δομή της εσωτερικής μετάθεσης Keccak-f[1600] (θ, ρ, π, χ, ι).
    <!-- NOTE (συγγραφή): Boundary — θεωρία αλγορίθμου εδώ, βελτιστοποιήσεις στο §6.1. -->
* **2.4 Κώδικες Διόρθωσης Σφαλμάτων (Error-Correcting Codes):** Θεωρία συνδυασμένης αποκωδικοποίησης (Concatenated Decoding) κωδίκων Reed-Muller (RM) και Reed-Solomon (RS).
    <!-- NOTE (συγγραφή): Berlekamp-Massey theory μόνο εδώ· στο §6.2.3 μόνο vectorization. -->

## Κεφάλαιο 3: Αρχιτεκτονική RISC-V και Κρυπτογραφικές Επεκτάσεις
* **3.1 Σχεδιαστικές Αρχές και Βασική Αρχιτεκτονική RV64:**
    * **3.1.1 Ιστορικό Context και Φιλοσοφία:** Σύντομη αναφορά (Berkeley RISC → RISC-V), η αρχή "minimal base + optional extensions", ανοιχτό ISA χωρίς licensing, RISC-V International. Γιατί αυτό είναι σημαντικό για PQC (cryptographic agility μέσω ISA modularity).
    * **3.1.2 Βασικά Χαρακτηριστικά RV64I:** Load-store μοντέλο, 32 general-purpose registers × 64-bit (x0–x31), σταθερό μήκος εντολής 32-bit, τα 6 instruction formats (R/I/S/B/U/J) και η λογική τους, program counter. Σύντομη αναφορά σε privilege levels (Machine/Supervisor/User) — η παρούσα εργασία εκτελεί σε Machine mode (bare-metal).
    * **3.1.3 Extension Naming Convention και Standard Profiles:** Σύμβαση ονοματοδοσίας: RV64GC = RV64IMAFD+C. Τι σημαίνει κάθε γράμμα (I=integer base, M=multiply/divide, A=atomics, F/D=floating-point single/double, C=compressed 16-bit). Αναφορά στο RVA23 profile ως mainstream adoption target.
    * **3.1.4 Σύγκριση Σχεδιαστικής Φιλοσοφίας με ARM και x86:** Fixed-width encoding (RISC-V) vs variable-length (x86 CISC) — implications σε decode simplicity και pipelining. Open ISA vs proprietary licensing (ARM royalties, x86 patent walls). ISA-level crypto υποστήριξη: RISC-V = general-purpose extensions (Zbc/Zvbc carry-less multiply) vs ARM = dedicated macro-ops (pmull, EOR3/RAX1 για SHA-3) vs x86 = wide SIMD (vpclmulqdq σε 512-bit ZMM). Φιλοσοφική επιλογή: ευελιξία vs peak single-op throughput.
* **3.2 Scalar Cryptographic Extensions:** Αναλυτική παρουσίαση των Zbc (carry-less multiplication: `clmul`, `clmulh`, `clmulr`) και Zbb/Zbkb (bit-manipulation: `rori`, `andn`, `rev8`, `brev8`). Πώς αυτές οι εντολές αντιστοιχούν σε κρυπτογραφικά primitives (GF(2)[x] πολλαπλασιασμός, Keccak rotations).
* **3.3 Vector Unit Architecture (RVV 1.0):**
    * **3.3.1 Γενική Αρχιτεκτονική:** 32 vector registers (v0–v31), VLEN ως hardware parameter (VLEN = 2^k, τυπικά 128–2048 bits). Φιλοσοφία Vector-Length Agnosticism (VLA): ο ίδιος κώδικας τρέχει σε οποιοδήποτε VLEN χωρίς recompilation.
    * **3.3.2 CSRs και Παραμετροποίηση:** Αναλυτική παρουσίαση των Control/Status Registers: `vtype` (SEW, LMUL, vta, vma, vill), `vl` (ενεργό μήκος διανύσματος), `vlenb` (VLEN/8 σε bytes). Τύπος VLMAX: `VLMAX = (VLEN / SEW) × LMUL`. Παράδειγμα: VLEN=256, SEW=64, LMUL=1 → VLMAX=4 elements.
    * **3.3.3 Strip-Mining Pattern και vsetvl Μηχανισμός:** Ο θεμελιώδης βρόχος RVV: `for (; n > 0; n -= vl) { vl = __riscv_vsetvl_e64m1(n); /* vector ops */ }`. Εξήγηση: ο compiler δεν γνωρίζει VLEN at compile-time — ο βρόχος αντλεί τόσα elements όσα χωράνε ανά iteration. Αυτό εγγυάται portability μεταξύ hardware.
    * **3.3.4 Tail Policy, Mask Policy και v0 Mask Register:** Tail policy (vta): τι γίνεται στα elements μετά το vl (undisturbed vs agnostic). Mask policy (vma): ο v0 ως mask register — inactive elements. Σημασία για correctness σε κρυπτογραφικό context (δεν πρέπει να leak-αρουν δεδομένα).
    * **3.3.5 Κατηγορίες Vector Εντολών:** Memory access (unit-stride vle/vse, strided, indexed gather/scatter), arithmetic (vadd, vmul, vxor, vsll, vsrl), reductions (vredxor, vredsum), permutations (vrgather, vslidedown, vcompress). Ποιες χρησιμοποιούνται στην παρούσα εργασία και γιατί.
    * **3.3.6 LMUL Register Grouping:** Δυνατότητα ομαδοποίησης (LMUL=2,4,8) για μεγαλύτερα logical vectors εις βάρος αριθμού διαθέσιμων register groups. Fractional LMUL (1/2, 1/4, 1/8) για narrowing operations. Trade-off: μεγαλύτερο LMUL = λιγότεροι register groups = μεγαλύτερο register pressure.
* **3.4 Vector Cryptographic Extensions:** Ανάλυση των διανυσματικών επεκτάσεων Zvbc (`vclmul.vv`, `vclmulh.vv` — carry-less multiply σε vector elements) και Zvbb (`vror.vv`/`vror.vi` — vector rotate, `vandn.vv`, `vbrev8.v`). Γιατί αυτές δεν εκπέμπονται αυτόματα από τον compiler (βλ. §7.6.3).
* **3.5 Σχετικές Εργασίες (Related Work):** Σύντομη ανασκόπηση της βιβλιογραφίας ανά θεματικό άξονα: υλοποιήσεις HQC σε RISC-V, HQC σε x86/ARM, και γενικότερες PQC εργασίες στο RISC-V. Αναφορά σε ανταγωνιστικές ISA πλατφόρμες (x86 AVX2 `vpclmulqdq`, ARM NEON `pmull`) ως context. Σημείωση: προηγούμενες hardware υλοποιήσεις HQC αφορούσαν Round 3/4 dense-sparse πολλαπλασιασμό — η τελική spec χρησιμοποιεί dense-dense, καθιστώντας αυτές τις εργασίες μη άμεσα εφαρμόσιμες.
    <!-- NOTE (συγγραφή): Μια παράγραφος ανά paper, οργανωμένα σε sub-bullets:
         - HQC on RISC-V (3 papers)
         - HQC on x86/ARM (5-6 papers)
         - General PQC on RISC-V (ECO-CRYSTALS, TCHES 2025 Zhang et al., κλπ.)
         Αν φουσκώσει >4 σελίδες, σπάσε σε ξεχωριστό κεφάλαιο. -->

## Κεφάλαιο 4: Πειραματική Υποδομή και Μεθοδολογία Αξιολόγησης
* **4.1 Αρχιτεκτονική των Εξομοιωτών Spike ISS και QEMU**
* **4.2 Cross-Compilation Toolchains και Ρυθμίσεις Μεταγλώττισης:** Συγκριτική αξιολόγηση Clang/LLVM και GCC, CMake toolchain files, ISA flags (`-march`) και επίπεδα βελτιστοποίησης (`-O3`, LTO, PGO).
* **4.3 Μεθοδολογία Benchmarking και Περιορισμοί Μετρικής:** Καταμέτρηση δυναμικών εντολών (`instret`) και profiling μέσω DWARF debug symbols. **Σημαντική επισήμανση:** `instret ≠ cycles` — ο Spike ISS μετράει εντολές υπό ιδανικές συνθήκες pipeline (χωρίς stalls, memory latency, cache effects). Το instret αποτελεί κάτω φράγμα κύκλων σε πραγματικό hardware. Η ετερογενής σύγκριση με cycle counts τρίτων απαιτεί ρητές επιφυλάξεις (βλ. §7.3).
* **4.4 Επαλήθευση Ορθότητας:** Αυτοματοποιημένος έλεγχος μέσω των Known Answer Tests (KAT) του NIST.
* **4.5 Bare-Metal Πλαίσιο Εκτέλεσης:**
    * **4.5.1 Boot Sequence:** Μεταγλώττιση με `-nostdlib`, εξειδικευμένο `startup.S` για αρχικοποίηση επεξεργαστή, ρητή ενεργοποίηση Vector Unit μέσω `mstatus.VS`, και custom linker script `link.ld` για έλεγχο memory layout.
    * **4.5.2 Memory Management:** Εξειδικευμένες ρουτίνες `memcpy`/`memset` (`mem.c`) χωρίς runtime βιβλιοθήκες. Πολιτικές ευθυγράμμισης για ντετερμινιστική εκτέλεση.
    * **4.5.3 Measurement Purity:** Απουσία OS jitter και syscall overhead — κάθε εντολή (`instret`) αποδίδεται αποκλειστικά στον κρυπτογραφικό πυρήνα.
* **4.6 Baseline Profiling και Ταυτοποίηση Bottlenecks:** Πρώτη εκτέλεση του reference HQC στο Spike environment. Pie chart: κατανομή instret σε polymul (~96%), Keccak, decode. Αυτό θεμελιώνει τη στόχευση βελτιστοποίησης στα επόμενα κεφάλαια.


## Κεφάλαιο 5: Βελτιστοποίηση Κύριου Υπολογιστικού Πυρήνα (Polynomial Multiplication)

* **5.1 Σχεδιαστικό Πλαίσιο: Hardware Tiers και Implementation Tracks:**
    * **5.1.1 Hardware Tier 1 (Scalar Deployment Class):** Στόχευση σε προφίλ `rv64gc` με scalar cryptographic extensions (`Zbc`/`Zbb`, και `Zbkb` όπου απαιτείται).
    * **5.1.2 Hardware Tier 2 (Base-RVV Deployment Class):** Στόχευση σε προφίλ `rv64gcv` χωρίς `Zvbc`/`Zvbb`, για πλατφόρμες με ευρύτερη διαθεσιμότητα διανυσματικής μονάδας.
    * **5.1.3 Hardware Tier 3 (Full Vector-Crypto Deployment Class):** Στόχευση σε `rv64gcv` με `Zvbc`/`Zvbb` και ελεγχόμενη εξάρτηση από `zvl256b`.
    * **5.1.4 Implementation Track A:** Scalar μονοπάτι με έμφαση σε `Zbc`/`Zbb` και strict non-vector execution discipline.
    * **5.1.5 Implementation Track B:** Base RVV μονοπάτι (χωρίς vector-crypto επεκτάσεις), με επιλεκτική διανυσματοποίηση αλγοριθμικών σταδίων.
    * **5.1.6 Implementation Track C:** Full vector-crypto μονοπάτι με αξιοποίηση `Zvbc`/`Zvbb` σε κρίσιμους πυρήνες.
* **5.2 Επιτάχυνση Πολυωνυμικού Πολλαπλασιασμού στο $GF(2)[x]$:**
    * **5.2.1 Track A — Scalar/Zbc Path:** Μετάβαση από bit-serial schoolbook σε word-level `clmul`/`clmulh` Karatsuba φύλλα.
    * **5.2.2 Track B — RVV-Assisted Path:** Διανυσματοποίηση combine και data-movement βρόχων με RVV 1.0, με στόχο μείωση register pressure.
    * **5.2.3 Track C — Zvbc Leaf Path:** Εξειδίκευση leaf kernels με `vclmul.vv`/`vclmulh.vv` και επιθετικό unrolling.
    * **5.2.4 Shared Mechanisms — Memory Discipline και AP-2 Generator:** Assign-on-first-write πειθαρχία μνήμης και compile-time παραγωγή recursion trees μέσω Python generator.
    * **5.2.5 Code Size vs Performance Trade-off:** Ανάλυση του .text footprint σε σχέση με unroll depth. Πίνακας: recursion depth vs instret vs code size. Τεκμηρίωση diminishing returns σε βαθύτερα επίπεδα ξετυλίγματος και I-cache pressure implications.


## Κεφάλαιο 6: Βελτιστοποίηση Δευτερευόντων Πυρήνων μετά Amdahl Rebalancing (Keccak, Decode, Infrastructure)

* **6.1 Επανασχεδιασμός Keccak / SHA-3:**
    * **6.1.1 Track A — Scalar/Zbb Path:** Scalar πυρήνας με αξιοποίηση `rori` και σταθερή πολιτική μετασχηματισμών.
    * **6.1.2 Track B — RVV-Only Path και "RVV Paradox":** Αξιολόγηση base-RVV διανυσματοποίησης και τεκμηριωμένη απόρριψη λόγω αυξημένου instruction footprint. Χωρίς `vror.vv` (απαιτεί Zvbb), κάθε rotation γίνεται 3 εντολές αντί 1. Σύγκριση με ARM SHA-3 dedicated instructions (EOR3, RAX1, XAR, BCAX) ως motivation: ARM = fused macro-ops, RISC-V χωρίς Zvbb = general-purpose bit-manipulation χωρίς single-cycle Keccak steps.
    * **6.1.3 Track C — Zvbb SoA 2-way Path:** Δικαναλική αρχιτεκτονική SoA με `vror.vi`/`vror.vx`, elimination των gather-heavy bottlenecks και παραγωγική ενοποίηση. 2-way είναι το αρχιτεκτονικό sweet spot: 4-way προκαλεί register spilling (25 lanes × 4 > 32 vector registers).
    * **6.1.4 Cross-Track I/O Optimizations:** Βελτιστοποίηση absorb/squeeze μονοπατιών, byte-streaming loops και ευθυγραμμισμένες 64-bit μεταφορές.
* **6.2 Σχεδιασμός Decode (RM+RS) με Συμμετρική Track Πρόοδο:**
    * **6.2.1 Track A — Scalar Decode Path:** Baseline scalar υλοποίηση Reed-Muller και Reed-Solomon.
    * **6.2.2 Track B — RVV RM + RS Axis A:** Διανυσματοποίηση Reed-Muller πυρήνων και επιτάχυνση RS συνδρόμων (Axis A) με lane-packed πράξεις.
    * **6.2.3 Track C — Zvbc RS Axes B και D:** Διανυσματοποίηση Berlekamp-Massey και Forney inner loops μέσω `vclmul`-based GF(2^8) pipeline.
    * **6.2.4 RS Axis C (Scalar Status):** Τεκμηρίωση διατήρησης του root-finding άξονα σε scalar μορφή και ορίων τρέχουσας διανυσματοποίησης.
* **6.3 VLEN Scaling: Ανάλυση Κλιμάκωσης Διανυσματικού Εύρους:**
    * **6.3.1 Μοντέλο Κλιμάκωσης και Θεωρητικό Όριο:** Ανάλυση του ποσοστού instret που εξαρτάται από VLEN — μόνο ~27% (Karatsuba recomposition) ωφελείται, ενώ ~54% (vclmul_leaf + Keccak) παραμένει σταθερό.
    * **6.3.2 Proportional Leaf Threshold: Ανάλυση Αστοχίας:** Τεκμηρίωση της στρατηγικής threshold = VLMAX και εξήγηση αποτυχίας λόγω O(n²) κόστους schoolbook leaf.
    * **6.3.3 RVV Recomposition Loops: Τελική Αρχιτεκτονική:** Αντικατάσταση scalar 4×-unrolled loops στο generated header με VLA helpers (`rvv_copy_u64`, `rvv_xor3_u64`, `rvv_xor_u64`) που κλιμακώνουν με VLMAX.
    * **6.3.4 Πειραματικά Αποτελέσματα VLEN Sweep:** Παρουσίαση μετρήσεων instret για VLEN ∈ {256, 512, 1024, 2048} — sweet spot στο VLEN=512 με 5–6.5% μείωση.
    * **6.3.5 Αρχιτεκτονικοί Περιορισμοί Κλιμάκωσης:** Γιατί vclmul_leaf (O(n²), σταθερό n=16) και Keccak (σταθερή 25-lane κατάσταση) δεν ωφελούνται από ευρύτερο VLEN. Επίσης τεκμηρίωση ότι μεγαλύτερο VLEN μπορεί να *αυξήσει* instret (fixed-width coding patterns, VL-dependent control overhead).
* **6.4 Cross-Track Υποδομή και Runtime Μηχανισμοί:**
    * **6.4.1 Global Utility Επιταχύνσεις:** Κοινές βελτιστοποιήσεις βοηθητικών kernels και δομών δεδομένων για όλα τα tracks.
    * **6.4.2 `vect_add` και Συναφείς Πρωτόγονες Πράξεις:** Ομογενοποιημένη μεταχείριση bitwise vector πράξεων με φορητότητα μεταξύ tiers.
    * **6.4.3 Memory Alignment και Data-Movement Layouts:** Πολιτικές ευθυγράμμισης, packing και πρόσβασης μνήμης για ελαχιστοποίηση μεταφορικού κόστους.
    * **6.4.4 Runtime Βιβλιοθήκη (`memcpy`):** Βελτιστοποιήσεις επιπέδου runtime με στόχο διαλειτουργικότητα bare-metal και benchmark σταθερότητα.

## Κεφάλαιο 7: Αποτελέσματα και Αξιολόγηση Απόδοσης
* **7.1 Απομονωμένες Μικρο-αξιολογήσεις (Isolated Micro-benchmarks):** Συστηματική σύγκριση Reference και Optimized επιδόσεων (`instret`) για `vect_mul`, `keccak` και `decode`.
* **7.2 Αξιολόγηση Πλήρους Κρυπτοσυστήματος (End-to-End KEM):** Παρουσίαση τελικών πινάκων KEM (Keypair, Encaps, Decaps) για όλα τα NIST security levels (128, 192, 256).
* **7.3 Συγκριτική Αξιολόγηση με Ετερογενείς Αρχιτεκτονικές:** Πίνακας cross-architecture (Intel AVX-512, AMD Zen4, ARM Cortex-M4, FAFFT RISC-V, παρούσα εργασία) και μεθοδολογική σημείωση: instret ≠ cycles, αλλά instret = κάτω φράγμα κύκλων. Ανάλυση του χάσματος Cortex-M4 (απουσία clmul) και γιατί FAFFT O(n log n) χάνει πρακτικά από AP-2 Karatsuba σε scalar cores.
* **7.4 Qualitative Σύγκριση HQC vs ML-KEM σε RISC-V:** Αντιπαράθεση με δημοσιευμένα αποτελέσματα ML-KEM (Kyber) σε RISC-V. Βάση: ECO-CRYSTALS (IACR 2024/1198) — Kyber-768 σε rv64im: KeyGen 399k, Encaps 448k, Decaps 479k cycles (real dual-issue hw). Παρούσα εργασία HQC-128: KeyGen 155k, Encaps 335k, Decaps 679k instret (Spike). Observation: HQC optimized είναι ~1.4× Decaps σε σχέση με Kyber optimized σε παρόμοια ISA class — αποδεικνύοντας ότι code-based PQC δεν είναι orders of magnitude πιο αργό αν βελτιστοποιηθεί κατάλληλα. Caveat: ετερογενής μετρική (instret vs cycles), qualitative comparison μόνο.
* **7.5 Κλιμάκωση ανά Security Level (Cross-Level Comparison):** Συγκριτικός πίνακας HQC-128 / HQC-192 / HQC-256 και ανάλυση scaling behavior μεταξύ επιπέδων ασφαλείας.
* **7.6 ISA Constraint Audits και Ablation Study:**
    * **7.6.1 Mandatory Extensions ανά Track/Tier:** Τυπικός ορισμός υποχρεωτικών ISA απαιτήσεων ανά deployment class.
    * **7.6.2 Ablation Testing Framework:** Ποσοτικός προσδιορισμός της μεμονωμένης συνεισφοράς των Zbc, Zvbc, Zbb και Zvbb μέσω συστηματικών ISA ablations.
    * **7.6.3 Compiler Auto-Lowering και Όρια Εμπιστοσύνης:** Τεκμηρίωση auto-lowering μοτίβων (συμπεριλαμβανομένων `Zba`, `Zicond`) και των ορίων τους σε κρίσιμα kernels. Απόδειξη ότι compilers δεν συνθέτουν αυτόματα Zvbc/Zvbb primitives από generic C — hand-written intrinsics είναι αναγκαία.
* **7.7 Συγκριτική Αξιολόγηση Μεταγλωττιστών (Compiler Comparison):** Ανάλυση Clang/LLVM και GCC ως προς register management, auto-vectorization και σταθερότητα παραγόμενου κώδικα.

## Κεφάλαιο 8: Συμπεράσματα και Μελλοντικές Επεκτάσεις
* **8.1 Σύνοψη και Αποτίμηση:** Ανακεφαλαίωση των speedups.
* **8.2 Περιορισμοί της Υλοποίησης:** Αναφορά στις τρέχουσες συνθήκες της αγοράς υλικού (διαθεσιμότητα native Zvbc) και στους εγγενείς περιορισμούς των instruction-accurate εξομοιωτών (Spike) έναντι cycle-accurate μοντέλων.
* **8.3 Προτάσεις για Μελλοντική Έρευνα:** Constant-time ανάλυση και side-channel resistance (dudect/ctgrind), εκτέλεση σε real hardware (native RISC-V boards), επέκταση σε RTL soft-cores (FPGA), και περαιτέρω vectorization των εκκρεμών σταδίων του decode.

## Παράρτημα Α: Οδηγός Εγκατάστασης και Παραμετροποίησης Περιβάλλοντος
* **Α.1 Μεταγλώττιση του RISC-V Toolchain**
* **Α.2 Εγκατάσταση του Spike ISS**

## Παράρτημα Β: Αναπαραγωγιμότητα και Περιγραφή Artifact
* **Β.1 Δομή του Repository:** Αντιστοίχιση πινάκων αποτελεσμάτων σε συγκεκριμένα scripts και build configurations.
* **Β.2 Οδηγίες Πλήρους Αναπαραγωγής Πειραμάτων:** Step-by-step instructions για reproduction ολόκληρου του benchmark suite.

## Παράρτημα Γ: Ακρωνύμια και Συντομογραφίες