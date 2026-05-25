# Πλήρης Διάρθρωση Διπλωματικής Εργασίας

## Προκαταρκτικές Σελίδες
* **Περίληψη (Ελληνικά)**
* **Abstract (Αγγλικά)**
* **Περιεχόμενα**

## Κεφάλαιο 1: Εισαγωγή
* **1.1 Η Κβαντική Απειλή και η Post-Quantum Κρυπτογραφία:** Ο αλγόριθμος του Shor και η αναγκαιότητα μετάβασης από τα κλασικά σχήματα (RSA, ECC) σε μετα-κβαντικά. Η στρατηγική "Harvest now, Decrypt later" και η εντολή NSA CNSA 2.0 (μετάβαση σε PQC έως 2035) ως πλαίσιο επιτακτικότητας.
    <!-- FIGURE: Timeline diagram — κλασική κρυπτογραφία → κβαντική απειλή → NIST competition → standardization. Δείχνει visually την επιτακτικότητα. -->
* **1.2 Το Πλαίσιο Τυποποίησης του NIST και ο Αλγόριθμος HQC:** Παρουσίαση της διαδικασίας τυποποίησης (γύροι αξιολόγησης, ολοκληρωμένα πρότυπα ML-KEM/ML-DSA/SLH-DSA) και η τρέχουσα φάση του HQC ως τελικού υποψηφίου προς δημοσίευση.
    <!-- FIGURE: Flowchart NIST rounds: Round 1 (69) → Round 2 (26) → Round 3 (7) → Round 4 (4) → Final (HQC). Με χρωματική επισήμανση του HQC path. -->
* **1.3 Cryptographic Agility και Καθαρά Λογισμικές Επιταχύνσεις:**
    * **1.3.1 Cryptographic Agility ως Απαίτηση:** Η ανωριμότητα του PQC τοπίου — αν αποδειχθεί ευπάθεια σε lattice-based σχήματα (π.χ. ML-KEM), tailor-made hardware καθίσταται άχρηστο. Concrete example: SIKE ήταν NIST Round 4 candidate, οργανισμοί ανέπτυσσαν FPGA accelerators, και σπάστηκε σε <1 ώρα σε laptop (2022) — κάθε hardware investment χάθηκε. Μέχρι να σταθεροποιηθεί η κατάσταση, απαιτούνται γενικές και ευέλικτες λύσεις.
    * **1.3.2 Καθαρά Λογισμική Επιτάχυνση σε Ανοιχτή Αρχιτεκτονική:** Το RISC-V σχεδιάστηκε με τη φιλοσοφία "ISA extensions replace fixed-function accelerators" — ratified cryptographic extensions χωρίς vendor lock-in. Το RVA23 profile (Zbb, V, Zvbb mandatory) επιβεβαιώνει ότι οι extensions που αξιοποιούμε βρίσκονται στο mainstream adoption path. Πλεονέκτημα ευέλικτων ISA extensions έναντι εξειδικευμένων κυκλωμάτων. *(Η αρχιτεκτονική τεκμηρίωση των επεκτάσεων παρουσιάζεται στο Κεφάλαιο 3.)*
        <!-- FIGURE: Diagram "spectrum of acceleration": Fixed ASIC ← FPGA ← ISA Extensions ← Pure Software. Με annotation: flexibility αυξάνεται →, peak perf μειώνεται →. Η παρούσα εργασία τοποθετείται στο "ISA Extensions". -->
* **1.4 Σκοπός και Συνεισφορά της Διατριβής:** Δεν υπάρχει μέχρι σήμερα δημοσιευμένη υλοποίηση του HQC που να αξιοποιεί τα Zbc/Zvbc και λοιπά ratified RISC-V extensions — ούτε σε hardware accelerator ούτε σε resource-constrained περιβάλλον. Οι μοναδικές σχετικές εργασίες (12/2025–03/2026) αφορούν αποκλειστικά πολλαπλασιασμό πολυωνύμων σε AVX2/NEON, χωρίς κάλυψη πλήρους KEM. Η παρούσα διατριβή καλύπτει αυτό το κενό με end-to-end βελτιστοποίηση στο RISC-V. Η εργασία εστιάζει σε performance optimization — timing side-channel resistance αφήνεται ρητά ως μελλοντική εργασία (βλ. §10.3).
* **1.5 Δομή της Εργασίας:** Σύντομη περιγραφή των επόμενων κεφαλαίων.

## Κεφάλαιο 2: Θεωρητικό Υπόβαθρο — HQC και Κρυπτογραφικοί Αλγόριθμοι
* **2.1 Code-Based Κρυπτογραφία και το KEM του HQC:** Εισαγωγή στην code-based κρυπτογραφία (πρόβλημα αποκωδικοποίησης τυχαίου γραμμικού κώδικα), η θέση του HQC στο πεδίο, security levels (128/192/256) και παράμετροι. Μαθηματική θεμελίωση και λειτουργική ανάλυση των διεργασιών Keygen, Encapsulation και Decapsulation. Pseudocode boxes (~10 γραμμές ανά algorithm) για τις 3 διεργασίες — υψηλού επιπέδου, χωρίς implementation details. Υψηλού επιπέδου αρχιτεκτονική: ποιοι αλγοριθμικοί πυρήνες (πολυωνυμικός πολλαπλασιασμός, SHA-3, αποκωδικοποίηση) συνθέτουν το κρυπτοσύστημα. Παρουσίαση pie chart κατανομής υπολογιστικού κόστους (instret) σε polymul (~96%), Keccak, decode — ως preview της δομής βελτιστοποίησης που ακολουθεί στα Κεφ. 6–7.
    <!-- PSEUDOCODE: KeyGen — generate random seeds, expand to polynomials h,x,y ∈ GF(2)[x]/(x^N-1), compute s = x + h·y, output (pk=seed||s, sk=seed||x||y). -->
    <!-- PSEUDOCODE: Encaps — parse pk, generate random m, expand to e,r₁,r₂, compute u = r₁ + h·r₂, v = m·G + s·r₂ + e, d = SHA3(m), ss = SHA3(m||u||v||d), output (ct=(u,v,d), ss). -->
    <!-- PSEUDOCODE: Decaps — compute m' = Decode(v - u·y), re-encapsulate, if ct'==ct: ss=SHA3(m'||...) else ss=SHA3(σ||...). Annotation: highlight ποιο βήμα καλεί polymul, ποιο Keccak, ποιο decode. -->
    <!-- NOTE (συγγραφή): Κράτα compact — μη γίνεις textbook. Το pie chart μπαίνει εδώ χωρίς να εξηγείται η μεθοδολογία μέτρησης (αυτή εξηγείται στο Κεφ. 5). Μία πρόταση caption αρκεί: "Figure 2.1: Κατανομή δυναμικών εντολών στο reference HQC-128 (βλ. §5.3 για μεθοδολογία)." -->
    <!-- FIGURE: Pie chart κατανομής instret (polymul ~96%, Keccak ~2-3%, decode ~1-2%, misc). -->
    <!-- FIGURE: Block diagram (flowchart) ροής δεδομένων Keygen/Encaps/Decaps — ποιος πυρήνας καλείται πού, με βέλη εισόδου/εξόδου. Χρωματική κωδικοποίηση: polymul=κόκκινο (hot), Keccak=πορτοκαλί, decode=κίτρινο. -->
    <!-- FIGURE: Πίνακας παραμέτρων HQC ανά security level: N, n₁, n₂, ω, δ — ο αναγνώστης χρειάζεται quick reference. -->
* **2.2 Πολυωνυμικός Πολλαπλασιασμός σε Δυαδικά Πεδία (GF(2)[x]):** Ανάλυση της πράξης στον δακτύλιο $GF(2)[x] / (x^N - 1)$ και μαθηματική μελέτη του αλγορίθμου Karatsuba. Αριθμητικό παράδειγμα (worked example) με 4-bit πολυώνυμα: βήμα-βήμα εφαρμογή Karatsuba decomposition σε μικρό μέγεθος ώστε να γίνει concrete η αναδρομική δομή. Δεύτερο παράδειγμα σε 8-bit ώστε να φαίνεται η αναδρομή σε 2 επίπεδα (3 sub-problems → 9 sub-sub-problems).
    <!-- NOTE (συγγραφή): Μόνο τα μαθηματικά που χρειάζονται για τον Karatsuba decomposition — μηχανική, όχι αξιώματα. -->
    <!-- FIGURE: Recursion tree diagram (μικρό, 2 levels) που δείχνει: a(x)·b(x) → {a_L·b_L, a_H·b_H, (a_L+a_H)·(b_L+b_H)} → 9 leaf products. Annotation: "3 multiplications of half-size" σε κάθε level. -->
    <!-- FIGURE: Σύγκριση complexity: schoolbook O(n²) vs Karatsuba O(n^1.585) — γράφημα με N στον x-axis και πράξεις στον y-axis. -->
* **2.3 Συναρτήσεις Κατακερματισμού SHA-3 / Keccak:** Αρχιτεκτονική σπόγγου (Sponge Construction) και δομή της εσωτερικής μετάθεσης Keccak-f[1600] (θ, ρ, π, χ, ι).
    <!-- NOTE (συγγραφή): Boundary — θεωρία αλγορίθμου εδώ, βελτιστοποιήσεις στο §7.1. -->
    <!-- FIGURE: Sponge construction diagram: absorb phase (XOR input blocks into state, apply f) → squeeze phase (extract output blocks). Με annotation rate r και capacity c. -->
    <!-- FIGURE: Πίνακας 5 Keccak steps: θ=column parity XOR, ρ=lane rotation (rotation offsets table), π=lane permutation (5×5 matrix), χ=nonlinear (row-wise AND-NOT-XOR), ι=round constant XOR. Μία γραμμή ανά step, με μαθηματικό τύπο και intuition. -->
    <!-- FIGURE: Visualization της 5×5×64 Keccak state ως 3D block — lanes, columns, planes. Βοηθάει τον αναγνώστη να καταλάβει γιατί ρ/π είναι permutations. -->
* **2.4 Κώδικες Διόρθωσης Σφαλμάτων (Reed-Muller, Reed-Solomon):** Θεωρία συνδυασμένης αποκωδικοποίησης (Concatenated Decoding) κωδίκων RM και RS. Berlekamp-Massey αλγόριθμος.
    <!-- NOTE (συγγραφή): Berlekamp-Massey theory μόνο εδώ· στο §7.2 μόνο vectorization. -->
    <!-- FIGURE: Concatenated decoding pipeline diagram: received_word → RM decode (inner code, bit-level) → RS decode (outer code, symbol-level) → corrected output. Annotation: "RM διορθώνει bit errors εντός symbols, RS διορθώνει symbol errors μεταξύ symbols." -->
    <!-- FIGURE: Flowchart RS decoding steps: Syndrome computation → Berlekamp-Massey (error locator polynomial) → Root-finding (Chien search) → Forney (error values) → Correction. Κάθε βήμα = box, αντιστοιχεί σε axis A/B/C/D στο §7.2. -->

## Κεφάλαιο 3: Αρχιτεκτονική RISC-V και Κρυπτογραφικές Επεκτάσεις
* **3.1 Σχεδιαστικές Αρχές και Βασική Αρχιτεκτονική RV64:**
    * **3.1.1 Ιστορικό Πλαίσιο και Φιλοσοφία:** Σύντομη αναφορά (Berkeley RISC → RISC-V), η αρχή "minimal base + optional extensions", ανοιχτό ISA χωρίς licensing, RISC-V International. Γιατί αυτό είναι σημαντικό για PQC (cryptographic agility μέσω ISA modularity).
    * **3.1.2 Βασικά Χαρακτηριστικά RV64I:** Load-store μοντέλο, 32 general-purpose registers × 64-bit (x0–x31), σταθερό μήκος εντολής 32-bit, τα 6 instruction formats (R/I/S/B/U/J) και η λογική τους, program counter. Σύντομη αναφορά σε privilege levels (Machine/Supervisor/User) — η παρούσα εργασία εκτελεί σε Machine mode (bare-metal).
        <!-- FIGURE: Diagram 6 instruction formats (R/I/S/B/U/J) side-by-side, δείχνοντας bit fields. Κλασικό RISC-V reference figure. -->
    * **3.1.3 Σύμβαση Ονοματοδοσίας Επεκτάσεων και Standard Profiles:** Σύμβαση ονοματοδοσίας: RV64GC = RV64IMAFD+C. Τι σημαίνει κάθε γράμμα (I=integer base, M=multiply/divide, A=atomics, F/D=floating-point single/double, C=compressed 16-bit). Αναφορά στο RVA23 profile ως mainstream adoption target.
        <!-- FIGURE: Πίνακας extensions: Extension | Περιγραφή | Σχετική με παρούσα εργασία; Highlight Zbc, Zbb, V, Zvbc, Zvbb. -->
    * **3.1.4 Σύγκριση Σχεδιαστικής Φιλοσοφίας με ARM και x86:** Fixed-width encoding (RISC-V) vs variable-length (x86 CISC) — implications σε decode simplicity και pipelining. Open ISA vs proprietary licensing (ARM royalties, x86 patent walls). ISA-level crypto υποστήριξη: RISC-V = general-purpose extensions (Zbc/Zvbc carry-less multiply) vs ARM = dedicated macro-ops (pmull, EOR3/RAX1 για SHA-3) vs x86 = wide SIMD (vpclmulqdq σε 512-bit ZMM). Φιλοσοφική επιλογή: ευελιξία vs peak single-op throughput.
        <!-- FIGURE: Comparative table (3 columns): Χαρακτηριστικό | RISC-V | ARM | x86. Rows: Encoding width, Licensing, Crypto support model, Vector model (VLA vs fixed-width SIMD), Register file size. -->
* **3.2 Scalar Κρυπτογραφικές Επεκτάσεις (Zbc, Zbb, Zbkb):** Αναλυτική παρουσίαση των Zbc (carry-less multiplication: `clmul`, `clmulh`, `clmulr`) και Zbb/Zbkb (bit-manipulation: `rori`, `andn`, `rev8`, `brev8`). Πώς αυτές οι εντολές αντιστοιχούν σε κρυπτογραφικά primitives (GF(2)[x] πολλαπλασιασμός, Keccak rotations). **Before/after πίνακας:** πολλαπλασιασμός 64-bit πολυωνύμων χωρίς clmul (64 shifts + XORs, ~128 εντολές) vs με clmul (2 εντολές: `clmul` + `clmulh`). **Assembly snippet:** 3–4 γραμμές shift-and-XOR loop vs 2 γραμμές clmul/clmulh.
    <!-- FIGURE: Before/after code listing (assembly): Left = naive shift-XOR loop (annotated), Right = clmul+clmulh (annotated). Caption: "64× speedup σε instruction count." -->
    <!-- FIGURE: Diagram "clmul semantics": δείχνει visually τι κάνει carry-less multiply σε bit level — XOR αντί ADD στα partial products. -->
* **3.3 Αρχιτεκτονική Διανυσματικής Μονάδας (RVV 1.0):**
    * **3.3.1 Γενική Αρχιτεκτονική και Vector-Length Agnosticism:** 32 vector registers (v0–v31), VLEN ως hardware parameter (VLEN = 2^k, τυπικά 128–2048 bits). Φιλοσοφία VLA: ο ίδιος κώδικας τρέχει σε οποιοδήποτε VLEN χωρίς recompilation.
        <!-- FIGURE: Diagram vector register file: v0–v31, κάθε ένα VLEN bits wide. Annotation: "VLEN is unknown at compile time — only at runtime via vlenb CSR." -->
    * **3.3.2 Control/Status Registers και Παραμετροποίηση:** `vtype` (SEW, LMUL, vta, vma, vill), `vl` (ενεργό μήκος διανύσματος), `vlenb` (VLEN/8 σε bytes). Τύπος VLMAX: `VLMAX = (VLEN / SEW) × LMUL`. Παράδειγμα: VLEN=256, SEW=64, LMUL=1 → VLMAX=4 elements.
        <!-- FIGURE: Πίνακας VLMAX για διάφορους συνδυασμούς VLEN/SEW/LMUL — quick reference. -->
    * **3.3.3 Strip-Mining και ο Μηχανισμός vsetvl:** Ο θεμελιώδης βρόχος RVV: `for (; n > 0; n -= vl) { vl = __riscv_vsetvl_e64m1(n); /* vector ops */ }`. Εξήγηση: ο compiler δεν γνωρίζει VLEN at compile-time — ο βρόχος αντλεί τόσα elements όσα χωράνε ανά iteration. Αυτό εγγυάται portability μεταξύ hardware.
        <!-- FIGURE: Strip-mining visualization: array 20 elements, VLMAX=4 → 5 iterations (4+4+4+4+4). Δίπλα: ίδιος κώδικας, VLMAX=8 → 3 iterations (8+8+4). Caption: "Same code, different hardware — VLA in action." -->
    * **3.3.4 Tail/Mask Policies και Κατηγορίες Εντολών:** Tail policy (vta), Mask policy (vma), ο v0 ως mask register. Memory access (unit-stride vle/vse, strided, indexed), arithmetic (vadd, vmul, vxor, vsll, vsrl), reductions (vredxor, vredsum), permutations (vrgather, vslidedown, vcompress). LMUL register grouping (1–8, fractional) και trade-offs.
        <!-- FIGURE: Diagram LMUL grouping: LMUL=1 → 32 register groups, LMUL=2 → 16 groups (v0-v1, v2-v3, ...), LMUL=4 → 8 groups. Visual: register file color-coded by group. -->
        <!-- FIGURE: Πίνακας κατηγοριών εντολών: Category | Examples | Χρήση στην παρούσα εργασία. -->
    * **3.3.5 Motivating Example — XOR Loop σε RVV:** Πλήρες worked example: ένας απλός `for` loop που κάνει XOR δύο arrays σε C (5 γραμμές), η αντίστοιχη RVV υλοποίηση (~8 γραμμές intrinsics), και annotation κάθε εντολής. Δείχνει strip-mining, vsetvl, vle/vse σε πραγματικό context.
        <!-- FIGURE: Side-by-side code: C scalar loop (left) vs RVV intrinsics (right). Annotated arrows δείχνουν ποια C line αντιστοιχεί σε ποια RVV intrinsic. -->
* **3.4 Διανυσματικές Κρυπτογραφικές Επεκτάσεις (Zvbc, Zvbb):** Ανάλυση των Zvbc (`vclmul.vv`, `vclmulh.vv` — carry-less multiply σε vector elements) και Zvbb (`vror.vv`/`vror.vi` — vector rotate, `vandn.vv`, `vbrev8.v`). Γιατί αυτές δεν εκπέμπονται αυτόματα από τον compiler (βλ. §9.2.3).
    <!-- FIGURE: Before/after table (vector context): Πράξη | Χωρίς Zvbc/Zvbb (εντολές) | Με Zvbc/Zvbb (εντολές). Rows: 64-bit carry-less mul, 64-bit rotation, byte-reverse. -->

## Κεφάλαιο 4: Σχετικές Εργασίες (Related Work)
* **4.1 Υλοποιήσεις HQC σε RISC-V:** Ανασκόπηση υπαρχουσών εργασιών. Σημείωση: προηγούμενες hardware υλοποιήσεις HQC αφορούσαν Round 3/4 dense-sparse πολλαπλασιασμό — η τελική spec χρησιμοποιεί dense-dense, καθιστώντας αυτές τις εργασίες μη άμεσα εφαρμόσιμες.
* **4.2 Υλοποιήσεις HQC σε x86 και ARM:** Εργασίες με AVX2 `vpclmulqdq`, ARM NEON `pmull`.
* **4.3 Γενικότερες PQC Εργασίες στο RISC-V:** ECO-CRYSTALS, TCHES 2025 Zhang et al., κ.ά.
* **4.4 Αποτίμηση Κενού και Τοποθέτηση Παρούσας Εργασίας:** Σύνοψη του ερευνητικού κενού που καλύπτει η παρούσα διατριβή σε σχέση με τη βιβλιογραφία.
    <!-- NOTE (συγγραφή): Μια παράγραφος ανά paper. -->
    <!-- FIGURE: Summary table related work: Paper | Platform | Algorithm | Metric | Result | Limitation vs παρούσα. -->

## Κεφάλαιο 5: Πειραματική Υποδομή και Μεθοδολογία Αξιολόγησης
* **5.1 Αρχιτεκτονική Εξομοιωτών (Spike ISS, QEMU)**
    <!-- FIGURE: Diagram Spike architecture: Host CPU → Spike ISS (functional model, instruction-by-instruction) → instret counter. Annotation: "No pipeline model, no cache model — pure instruction count." -->
* **5.2 Cross-Compilation Toolchains και Ρυθμίσεις Μεταγλώττισης:** Συγκριτική αξιολόγηση Clang/LLVM και GCC, CMake toolchain files, ISA flags (`-march`) και επίπεδα βελτιστοποίησης (`-O3`, LTO, PGO).
    <!-- FIGURE: Build pipeline diagram: Source (.c/.h) → CMake → Toolchain (Clang/GCC) → -march flags → ELF binary → Spike execution → instret output. -->
* **5.3 Μεθοδολογία Benchmarking και Περιορισμοί Μετρικής:** Καταμέτρηση δυναμικών εντολών (`instret`) και profiling μέσω DWARF debug symbols. **Σημαντική επισήμανση:** `instret ≠ cycles` — ο Spike ISS μετράει εντολές υπό ιδανικές συνθήκες pipeline (χωρίς stalls, memory latency, cache effects). Το instret αποτελεί κάτω φράγμα κύκλων σε πραγματικό hardware. Η ετερογενής σύγκριση με cycle counts τρίτων απαιτεί ρητές επιφυλάξεις (βλ. §9.1).
    <!-- FIGURE: Diagram "instret vs cycles": ideal single-issue pipeline (1 instruction/cycle = instret) vs real pipeline (stalls, cache misses → cycles > instret) vs dual-issue/superscalar pipeline (e.g., BananaPi BPI-F3 SpacemiT K1 — 2 instructions/cycle possible → cycles < instret). Caption: "instret ≠ cycles: σε single-issue hw, instret = lower bound. Σε dual-issue hw (π.χ. K1), cycles μπορεί να είναι < instret." -->
* **5.4 Επαλήθευση Ορθότητας (KAT Tests):** Αυτοματοποιημένος έλεγχος μέσω των Known Answer Tests του NIST. Αναπαραγωγιμότητα: όλα τα αποτελέσματα αναπαράγονται μέσω scripts (βλ. Παράρτημα Β).
* **5.5 Bare-Metal Πλαίσιο Εκτέλεσης:**
    * **5.5.1 Boot Sequence:** Μεταγλώττιση με `-nostdlib`, εξειδικευμένο `startup.S` για αρχικοποίηση επεξεργαστή, ρητή ενεργοποίηση Vector Unit μέσω `mstatus.VS`, και custom linker script `link.ld` για έλεγχο memory layout.
        <!-- FIGURE: Boot sequence flowchart: Reset → startup.S (set sp, enable VU, zero BSS) → main() → HQC execution → instret readout → ecall exit. -->
    * **5.5.2 Διαχείριση Μνήμης και Runtime Υποστήριξη:** Εξειδικευμένες ρουτίνες `memcpy`/`memset` χωρίς runtime βιβλιοθήκες. Πολιτικές ευθυγράμμισης (alignment) για ντετερμινιστική εκτέλεση. Βελτιστοποιημένη `memcpy` με RVV.
        <!-- FIGURE: Memory map diagram (linear, vertical): 0x80000000 .text (code) | .rodata (KAT data, constants) | .data (initialized globals) | .bss (zero-initialized, Keccak state, scratch buffers) | ↓ heap (polymul scratch) | ... | ↑ stack (top of RAM). Annotation: total RAM budget, alignment boundaries marked. -->
    * **5.5.3 Καθαρότητα Μέτρησης (Measurement Purity):** Απουσία OS jitter και syscall overhead — κάθε εντολή (`instret`) αποδίδεται αποκλειστικά στον κρυπτογραφικό πυρήνα.
* **5.6 Baseline Profiling:** Πρώτη εκτέλεση του reference HQC στο Spike environment. Αναφορά στο pie chart (§2.1) ως θεμελίωση στόχευσης βελτιστοποίησης.

## Κεφάλαιο 6: Βελτιστοποίηση Πολυωνυμικού Πολλαπλασιασμού (Polynomial Multiplication)

* **6.1 Σχεδιαστικό Πλαίσιο — Hardware Tiers και Implementation Paths:**
    * **6.1.1 Tier 1 — Scalar Deployment (rv64gc + Zbc/Zbb):** Στόχευση σε προφίλ χωρίς διανυσματική μονάδα.
    * **6.1.2 Tier 2 — Base-RVV Deployment (rv64gcv χωρίς Zvbc/Zvbb):** Πλατφόρμες με ευρύτερη διαθεσιμότητα διανυσματικής μονάδας.
    * **6.1.3 Tier 3 — Full Vector-Crypto Deployment (rv64gcv + Zvbc/Zvbb):** Πλήρης αξιοποίηση κρυπτογραφικών vector extensions.
    <!-- FIGURE: Venn diagram ή layered diagram: Tier 1 (base) ⊂ Tier 2 (+ RVV) ⊂ Tier 3 (+ Zvbc/Zvbb). Κάθε layer annotated με τις extensions που προσθέτει. -->
* **6.2 Scalar Path — Karatsuba με Zbc:**
    * **6.2.1 Μετάβαση από Bit-Serial σε Word-Level Πολλαπλασιασμό:** Από bit-serial schoolbook σε word-level `clmul`/`clmulh` Karatsuba φύλλα.
        <!-- FIGURE: Before/after: schoolbook inner loop (bit-serial, ~N² iterations) vs clmul leaf (word-level, 2 instructions per 64-bit product). -->
        <!-- PSEUDOCODE: Schoolbook reference (5 lines: for i, for j, if bit set, XOR shifted) vs Karatsuba leaf with clmul (5 lines: split, clmul low/high/mid, combine). Side-by-side algorithmic comparison. -->
    * **6.2.2 Δομή Αναδρομής και AP-2 Generator:** Compile-time παραγωγή recursion trees μέσω Python generator. Assign-on-first-write πειθαρχία μνήμης.
        <!-- FIGURE: Diagram AP-2 generator pipeline: Python script → recursion tree analysis → generated C header (.h) → compiled into final binary. Annotation: "Zero runtime overhead — all decisions at compile-time." -->
        <!-- PSEUDOCODE: Karatsuba recursion (high-level, ~8 lines): function polymul(a, b, n): if n ≤ LEAF_SIZE: return schoolbook(a,b); split a→(aL,aH), b→(bL,bH); P0=polymul(aL,bL); P1=polymul(aH,bH); P2=polymul(aL⊕aH, bL⊕bH); return combine(P0,P1,P2). -->
    * **6.2.3 Unrolling και Βάθος Αναδρομής:** Πίνακας: recursion depth vs instret vs .text size. Τεκμηρίωση diminishing returns και I-cache pressure implications.
        <!-- FIGURE: Dual-axis chart: x=recursion depth, left y-axis=instret (decreasing), right y-axis=.text size in KB (increasing). Annotation στο sweet spot. -->
    * **6.2.4 Worked Example — Πλήρης Αναδρομή HQC-128:** Concrete trace μίας πλήρους κλήσης polymul: πόσοι leaves, πόσα recursion levels, πώς μοιράζεται η μνήμη (scratch buffers), dataflow visualization.
        <!-- FIGURE: Full recursion tree diagram HQC-128: root (N words) → 3×(N/2) → ... → leaves (16 words each). Annotated: total leaves count, total scratch memory, μέγεθος ανά level. Color-coded: green=recomposition (RVV-scalable), red=leaf (fixed-cost). -->
* **6.3 Base-RVV Path — Διανυσματοποίηση Recomposition:**
    * **6.3.1 Διανυσματοποίηση Combine και Data-Movement Βρόχων:** Αντικατάσταση scalar 4×-unrolled loops με VLA helpers (`rvv_copy_u64`, `rvv_xor3_u64`, `rvv_xor_u64`).
        <!-- FIGURE: Before/after code listing: scalar 4×-unroll (left, 12 lines) vs VLA helper call (right, 3 lines). Caption: "Same semantics, VLEN-agnostic." -->
        <!-- PSEUDOCODE: rvv_xor3_u64 helper (~5 lines): function rvv_xor3(dst, a, b, c, n): while n > 0: vl = vsetvl(n); va = vle64(a); vb = vle64(b); vc = vle64(c); vr = vxor(vxor(va,vb),vc); vse64(dst, vr); advance pointers by vl. -->
    * **6.3.2 Μείωση Register Pressure μέσω RVV:** Στόχος μεταφοράς δεδομένων χωρίς scalar register exhaustion.
* **6.4 Vector-Crypto Path — Zvbc Leaf Kernels:**
    * **6.4.1 Εξειδίκευση Leaf Kernels με vclmul.vv/vclmulh.vv:** Επιθετικό unrolling σε vector carry-less multiply.
        <!-- FIGURE: Diagram vclmul leaf operation: 16 input elements loaded → vclmul.vv/vclmulh.vv pairs → accumulation → 31-element output. Dataflow arrows. -->
        <!-- PSEUDOCODE: vclmul_leaf kernel (~8 lines): load a[0..15] into va, for each b[j]: vb = splat(b[j]), vlo = vclmul(va, vb), vhi = vclmulh(va, vb), accumulate result[j..j+15] ⊕= vlo, result[j+1..j+16] ⊕= vhi. -->
    * **6.4.2 Schoolbook Leaf Trade-offs:** O(n²) κόστος, σταθερό n=16 — γιατί ο leaf δεν κλιμακώνει με VLEN.
* **6.5 Code Size vs Performance Trade-off:** Ανάλυση .text footprint σε σχέση με unroll depth. Diminishing returns σε βαθύτερα επίπεδα ξετυλίγματος.
    <!-- FIGURE: Bar chart: different unroll configurations → instret (bars) με .text size overlay (line). -->

## Κεφάλαιο 7: Βελτιστοποίηση Keccak, Decode και Βοηθητικών Πυρήνων

* **7.1 Επανασχεδιασμός Keccak / SHA-3:**
    * **7.1.1 Scalar Path (Zbb):** Scalar πυρήνας με αξιοποίηση `rori` και σταθερή πολιτική μετασχηματισμών.
        <!-- PSEUDOCODE: Keccak round (scalar, ~10 lines): θ: C[x] = A[x,0]⊕...⊕A[x,4]; D[x] = C[x-1]⊕ROT(C[x+1],1); A ⊕= D. ρ+π: B[y,2x+3y] = ROT(A[x,y], offsets[x,y]). χ: A[x,y] = B[x,y] ⊕ (NOT(B[x+1,y]) AND B[x+2,y]). ι: A[0,0] ⊕= RC[round]. Annotation: ROT = rori instruction on Zbb. -->
    * **7.1.2 Base-RVV Path και το "RVV Paradox":** Αξιολόγηση base-RVV διανυσματοποίησης και τεκμηριωμένη απόρριψη λόγω αυξημένου instruction footprint. Χωρίς `vror.vv` (απαιτεί Zvbb), κάθε rotation γίνεται 3 εντολές αντί 1. Σύγκριση με ARM SHA-3 dedicated instructions (EOR3, RAX1, XAR, BCAX).
        <!-- FIGURE: Comparison table: Keccak rotation on RISC-V without Zvbb (vsll+vsrl+vor = 3 insn) vs with Zvbb (vror = 1 insn) vs ARM (dedicated XAR = 1 insn). Caption: "The RVV Paradox — vectorization hurts without Zvbb." -->
    * **7.1.3 Vector-Crypto Path (Zvbb 2-way SoA):** Δικαναλική αρχιτεκτονική SoA με `vror.vi`/`vror.vx`, elimination gather-heavy bottlenecks. 2-way ως sweet spot: 4-way προκαλεί register spilling (25 lanes × 4 > 32 vector registers).
        <!-- FIGURE: Diagram SoA 2-way layout: Keccak state A (25 lanes) interleaved στα vector registers — lane[i] of state 0 and state 1 packed in same vector. Annotation: "Process 2 Keccak blocks simultaneously." -->
        <!-- FIGURE: Register pressure chart: 1-way (25 regs) vs 2-way (25 regs, doubled throughput) vs 4-way (50 regs needed > 32 available → spill). -->
        <!-- PSEUDOCODE: SoA 2-way ρ step (~5 lines): for each lane i: v_lane[i] = vle64(state0_lane[i], state1_lane[i]); // 2 elements per vector. v_lane[i] = vror(v_lane[i], rotation_offset[i]); // single instruction rotates both states. Annotation: "1 vror replaces 2× scalar rori — 2× throughput from SoA packing." -->
    * **7.1.4 Βελτιστοποιήσεις Absorb/Squeeze I/O:** Byte-streaming loops και ευθυγραμμισμένες 64-bit μεταφορές.
* **7.2 Βελτιστοποίηση Decode (Reed-Muller + Reed-Solomon):**
    * **7.2.1 Scalar Path:** Baseline scalar υλοποίηση RM και RS.
        <!-- PSEUDOCODE: Concatenated decode (high-level, ~8 lines): for each symbol s[i]: rm_decoded[i] = RM_decode(s[i]) // inner code, bit-level correction. syndromes = RS_syndromes(rm_decoded). Λ(x) = Berlekamp_Massey(syndromes). roots = Chien_search(Λ). errors = Forney(roots, syndromes). output = rm_decoded ⊕ errors. -->
    * **7.2.2 Base-RVV Path — Διανυσματοποίηση RM και RS Syndromes:** Lane-packed πράξεις για Reed-Muller πυρήνες και RS syndrome υπολογισμό.
        <!-- FIGURE: Diagram lane-packisent from mTHMMY  ng: πώς multiple RM codewords pack σε ένα vector register, κάθε lane = ένα codeword. Parallel decode. -->
    * **7.2.3 Vector-Crypto Path — Zvbc RS (Berlekamp-Massey, Forney):** Διανυσματοποίηση inner loops μέσω `vclmul`-based GF(2^8) pipeline.
        <!-- FIGURE: GF(2^8) multiplication pipeline diagram: element pairs → vclmul.vv → reduction mod irreducible polynomial → result. Dataflow. -->
        <!-- PSEUDOCODE: GF(2^8) vector multiply (~5 lines): function gf256_vmul(va, vb): vlo = vclmul(va, vb); // carry-less multiply, 16-bit intermediate. vhi = vclmulh(va, vb); // high half. reduced = vlo ⊕ reduce(vhi); // mod irreducible poly P(x). return reduced. Annotation: "N multiplications in parallel, N = VLMAX." -->
    * **7.2.4 Root-Finding (Scalar Status):** Τεκμηρίωση διατήρησης σε scalar μορφή και ορίων τρέχουσας διανυσματοποίησης.
* **7.3 Βοηθητικοί Πυρήνες και Πρωτόγονες Πράξεις:**
    * **7.3.1 `vect_add` και Bitwise Vector Πράξεις:** Ομογενοποιημένη μεταχείριση με φορητότητα μεταξύ tiers.
    * **7.3.2 Πολιτικές Ευθυγράμμισης Μνήμης (Memory Alignment):** Packing και πρόσβαση μνήμης για ελαχιστοποίηση μεταφορικού κόστους.
* **7.4 Ανάλυση Κλιμάκωσης VLEN (VLEN Scaling):**
    * **7.4.1 Μοντέλο Κλιμάκωσης και Θεωρητικό Όριο:** Ποσοστό instret που εξαρτάται από VLEN — ~27% (Karatsuba recomposition) ωφελείται, ~54% (vclmul_leaf + Keccak) παραμένει σταθερό, ~19% (decode + misc) δεν κλιμακώνει.
        <!-- FIGURE: Stacked bar chart: breakdown instret σε 3 κατηγορίες (VLEN-scalable, fixed-cost, non-vectorized). Caption: "Only ~27% benefits from wider VLEN." -->
    * **7.4.2 Πειραματικά Αποτελέσματα VLEN Sweep:** Μετρήσεις instret για VLEN ∈ {256, 512, 1024, 2048} — sweet spot στο VLEN=512 με 5–6.5% μείωση.
        <!-- FIGURE: Line chart: x=VLEN, y=instret. Multiple lines for HQC-128/192/256. Annotation sweet spot. -->
    * **7.4.3 Αρχιτεκτονικοί Περιορισμοί:** Γιατί vclmul_leaf (O(n²), σταθερό n=16), Keccak (σταθερή 25-lane κατάσταση) και decode δεν ωφελούνται από ευρύτερο VLEN. Τεκμηρίωση ότι μεγαλύτερο VLEN μπορεί να αυξήσει instret (fixed-width coding patterns, VL-dependent control overhead).
        <!-- FIGURE: Diagram "why vclmul_leaf doesn't scale": leaf operates on fixed 16 elements — even with VLMAX=32, only 16 are used (wasted lanes). -->

## Κεφάλαιο 8: Αποτελέσματα και Μετρήσεις Απόδοσης
* **8.1 Απομονωμένες Μικρο-αξιολογήσεις (Isolated Micro-benchmarks):** Συστηματική σύγκριση Reference και Optimized επιδόσεων (`instret`) για polymul, Keccak και decode.
    <!-- FIGURE: Grouped bar chart: κάθε kernel (polymul, keccak, decode) με bars Reference vs Optimized (Tier 1/2/3). Speedup annotation. -->
* **8.2 Αξιολόγηση Πλήρους Κρυπτοσυστήματος (End-to-End KEM):** Τελικοί πίνακες KEM (Keypair, Encaps, Decaps) για όλα τα NIST security levels (128, 192, 256).
    <!-- FIGURE: Table: Security Level | Operation | Reference instret | Optimized instret | Speedup. -->
    <!-- FIGURE: Stacked bar chart ανά operation: breakdown πού πάει ο χρόνος μετά τη βελτιστοποίηση (polymul πλέον ~X%, Keccak ~Y%, decode ~Z%). Σύγκριση με αρχικό pie chart §2.1 — "rebalancing μετά Amdahl." -->
* **8.3 Κλιμάκωση ανά Security Level:** Συγκριτικός πίνακας HQC-128 / HQC-192 / HQC-256 και ανάλυση scaling behavior.
    <!-- FIGURE: Line chart: security level (x) vs instret (y) ανά operation. Annotation: theoretical O(N²) vs observed scaling. -->
* **8.4 Συγκριτική Αξιολόγηση Μεταγλωττιστών (Clang vs GCC):** Register management, auto-vectorization, σταθερότητα παραγόμενου κώδικα.
    <!-- FIGURE: Side-by-side bar chart: Clang vs GCC instret ανά kernel και tier. Annotation: ποιος κερδίζει πού και γιατί. -->

## Κεφάλαιο 9: Συζήτηση και Ερμηνεία Αποτελεσμάτων
* **9.1 Σύγκριση με Ετερογενείς Αρχιτεκτονικές:** Πίνακας cross-architecture (Intel AVX-512, AMD Zen4, ARM Cortex-M4, FAFFT RISC-V, παρούσα εργασία). Μεθοδολογική σημείωση: instret = κάτω φράγμα κύκλων. Ανάλυση χάσματος Cortex-M4 (απουσία clmul) και γιατί FAFFT O(n log n) χάνει πρακτικά από AP-2 Karatsuba σε scalar cores.
    <!-- FIGURE: Cross-architecture comparison table: Platform | ISA | Metric | KeyGen | Encaps | Decaps | Notes. -->
* **9.2 ISA Ablation Study:**
    * **9.2.1 Υποχρεωτικές Επεκτάσεις ανά Tier:** Τυπικός ορισμός ISA απαιτήσεων ανά deployment class.
    * **9.2.2 Ποσοτική Συνεισφορά Κάθε Extension:** Μεμονωμένη συνεισφορά Zbc, Zvbc, Zbb, Zvbb μέσω συστηματικών ablations.
        <!-- FIGURE: Waterfall chart: baseline (no extensions) → +Zbc → +Zbb → +RVV → +Zvbc → +Zvbb = final. Κάθε step δείχνει incremental improvement. -->
    * **9.2.3 Compiler Auto-Lowering και Όριά του:** Τεκμηρίωση auto-lowering μοτίβων (`Zba`, `Zicond`) και απόδειξη ότι compilers δεν συνθέτουν αυτόματα Zvbc/Zvbb primitives από generic C.
        <!-- FIGURE: Code example: generic C loop → compiler output without Zvbc (multiple scalar ops) vs hand-written intrinsics with Zvbc (single vector op). Side-by-side disassembly. -->
* **9.3 Σχεδιαστικές Αρχές που Γενικεύονται:** Αρχές πέρα από τον HQC: αποδοτικότητα carry-less multiply για πολυωνυμική αριθμητική γενικά, η αξία VLA strip-mining σε cryptographic workloads, τα όρια auto-vectorization σε crypto kernels, η σχέση code-generation (metaprogramming) vs hand-tuning.

## Κεφάλαιο 10: Συμπεράσματα και Μελλοντικές Επεκτάσεις
* **10.1 Σύνοψη Συνεισφοράς:** Ανακεφαλαίωση speedups και κύριων τεχνικών αποτελεσμάτων.
* **10.2 Περιορισμοί:** Διαθεσιμότητα native Zvbc hardware, εγγενείς περιορισμοί instruction-accurate εξομοιωτών (Spike) έναντι cycle-accurate μοντέλων.
* **10.3 Μελλοντική Έρευνα:** Constant-time ανάλυση και side-channel resistance (dudect/ctgrind), εκτέλεση σε real hardware (native RISC-V boards), επέκταση σε RTL soft-cores (FPGA), περαιτέρω vectorization εκκρεμών decode σταδίων.

## Παράρτημα Α: Οδηγός Εγκατάστασης Περιβάλλοντος
* **Α.1 Μεταγλώττιση του RISC-V Toolchain**
* **Α.2 Εγκατάσταση του Spike ISS**

## Παράρτημα Β: Αναπαραγωγιμότητα και Περιγραφή Artifact
* **Β.1 Δομή του Repository:** Αντιστοίχιση πινάκων αποτελεσμάτων σε συγκεκριμένα scripts και build configurations.
* **Β.2 Οδηγίες Πλήρους Αναπαραγωγής Πειραμάτων:** Step-by-step instructions για reproduction ολόκληρου του benchmark suite.

## Παράρτημα Γ: Ακρωνύμια και Συντομογραφίες
