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
* **1.4 Σκοπός και Συνεισφορά της Διατριβής:** Δεν υπάρχει μέχρι σήμερα δημοσιευμένη υλοποίηση του HQC που να αξιοποιεί τα Zbc/Zvbc και λοιπά ratified RISC-V extensions — ούτε σε hardware accelerator ούτε σε resource-constrained περιβάλλον. Οι μοναδικές σχετικές εργασίες (12/2025–03/2026) αφορούν αποκλειστικά πολλαπλασιασμό πολυωνύμων σε AVX2/NEON, χωρίς κάλυψη πλήρους KEM. Η παρούσα διατριβή καλύπτει αυτό το κενό με end-to-end βελτιστοποίηση στο RISC-V.
* **1.5 Δομή της Εργασίας:** Σύντομη περιγραφή των επόμενων κεφαλαίων.

## Κεφάλαιο 2: Θεωρητικό Υπόβαθρο HQC και Κρυπτογραφίας
* **2.1 Code-Based Κρυπτογραφία και ο Αλγόριθμος HQC:** Εισαγωγή στην code-based κρυπτογραφία (πρόβλημα αποκωδικοποίησης τυχαίου γραμμικού κώδικα), η θέση του HQC στο πεδίο, security levels (128/192/256) και παράμετροι. Υψηλού επιπέδου αρχιτεκτονική: ποιοι αλγοριθμικοί πυρήνες (πολυωνυμικός πολλαπλασιασμός, SHA-3, αποκωδικοποίηση) αποτελούν τα υπολογιστικά hotspots και γιατί.
* **2.2 Key Encapsulation Mechanism (KEM) του HQC:** Μαθηματική θεμελίωση και λειτουργική ανάλυση των διεργασιών Keygen, Encapsulation και Decapsulation.
* **2.3 Πολυωνυμικός Πολλαπλασιασμός σε Δυαδικά Πεδία:** Ανάλυση της πράξης στον δακτύλιο $GF(2)[x] / (x^N - 1)$ και μαθηματική μελέτη του Karatsuba ως main hotspot.
* **2.4 Συναρτήσεις Hash SHA-3 / Keccak:** Αρχιτεκτονική σπόγγου (Sponge Construction) και δομή της εσωτερικής μετάθεσης Keccak-f[1600].
* **2.5 Κώδικες Διόρθωσης Σφαλμάτων (Error-Correcting Codes):** Θεωρία συνδυασμένης αποκωδικοποίησης (Concatenated Decoding) κωδίκων Reed-Muller (RM) και Reed-Solomon (RS).

## Κεφάλαιο 3: Αρχιτεκτονική RISC-V και Κρυπτογραφικές Επεκτάσεις
* **3.1 Ιστορική Εξέλιξη RISC και Γένεση του RISC-V:** Από τα πρώτα RISC μοντέλα (Berkeley RISC, MIPS) στη σύγχρονη ανοιχτή αρχιτεκτονική RISC-V — σχεδιαστικές αρχές, modular ISA, και ο ρόλος του ανοιχτού οικοσυστήματος.
* **3.2 Scalar Cryptographic Extensions:** Αναλυτική παρουσίαση των Zbc (carry-less multiplication) και Zbb/Zbkb (bit-manipulation instructions).
* **3.3 Vector Unit Architecture (RVV 1.0):** Αρχιτεκτονική της διανυσματικής μονάδας (VLEN, LMUL, Vector-Length Agnosticism).
* **3.4 Vector Cryptographic Extensions:** Ανάλυση των διανυσματικών επεκτάσεων Zvbc και Zvbb. Σύγκριση αρχιτεκτονικής προσέγγισης με ARM SHA-3 dedicated instructions (EOR3, RAX1, XAR, BCAX): ARM = fused macro-ops για Keccak, RISC-V = general-purpose bit-manipulation (ευέλικτα αλλά χωρίς single-cycle Keccak steps).
* **3.5 Σχετικές Εργασίες (Related Work):** Σύντομη αναφορά στις ανταγωνιστικές ISA πλατφόρμες (x86 AVX2 `vpclmulqdq`, ARM NEON `pmull`) ως context, και ανασκόπηση της βιβλιογραφίας ανά θεματικό άξονα: υλοποιήσεις HQC σε άλλες αρχιτεκτονικές, επιτάχυνση πολυωνυμικού πολλαπλασιασμού, vectorized Keccak, και γενικότερες PQC εργασίες στο RISC-V. Σημείωση: προηγούμενες hardware υλοποιήσεις HQC αφορούσαν Round 3/4 dense-sparse πολλαπλασιασμό — η τελική spec χρησιμοποιεί dense-dense, καθιστώντας αυτές τις εργασίες μη άμεσα εφαρμόσιμες.

## Κεφάλαιο 4: Πειραματική Υποδομή και Μεθοδολογία Αξιολόγησης
* **4.1 Αρχιτεκτονική των Εξομοιωτών Spike ISS και QEMU**
* **4.2 Cross-Compilation Toolchains και Ρυθμίσεις Μεταγλώττισης:** Συγκριτική αξιολόγηση Clang/LLVM και GCC, CMake toolchain files, ISA flags (`-march`) και επίπεδα βελτιστοποίησης (`-O3`, LTO, PGO).
* **4.3 Μεθοδολογία Benchmarking:** Καταμέτρηση δυναμικών εντολών (`instret`) και profiling μέσω DWARF debug symbols.
* **4.4 Επαλήθευση Ορθότητας:** Αυτοματοποιημένος έλεγχος μέσω των Known Answer Tests (KAT) του NIST.
* **4.5 Bare-Metal Πλαίσιο Εκτέλεσης:**
    * **4.5.1 Αυτόνομη Μεταγλώττιση και Εκκίνηση:** Μεταγλώττιση με `-nostdlib`, ανάπτυξη εξειδικευμένου `startup.S` για άμεση αρχικοποίηση επεξεργαστή και έλεγχο της αρχικής κατάστασης εκτέλεσης.
    * **4.5.2 Χειροκίνητη Ενεργοποίηση Διανυσματικής Μονάδας:** Ρητή ενεργοποίηση της Vector Unit μέσω `mstatus.VS` κατά το στάδιο εκκίνησης, ώστε να εξασφαλίζεται ντετερμινιστική και αναπαραγώγιμη διαθεσιμότητα RVV.
    * **4.5.3 Προσαρμοσμένη Χαρτογράφηση Μνήμης:** Δέσμευση και οργάνωση των τμημάτων κώδικα/δεδομένων μέσω custom linker script `link.ld`, με πλήρη έλεγχο του memory layout.
    * **4.5.4 Εξειδικευμένες Ρουτίνες Μνήμης:** Υλοποίηση από μηδενική βάση των `memcpy`/`memset` στο `mem.c` με αποφυγή μη ελεγχόμενων βιβλιοθηκών runtime.
    * **4.5.5 Θεμελίωση Measurement Purity:** Απουσία OS jitter και syscall overhead στο bare-metal περιβάλλον, ώστε κάθε καταγεγραμμένη εντολή (`instret`) στον Spike να αποδίδεται αποκλειστικά στον κρυπτογραφικό πυρήνα.
    * **4.5.6 Επιχειρησιακή Σημασία για Embedded Deployment:** Τεκμηρίωση ότι η υλοποίηση είναι άμεσα αναπτύξιμη σε ενσωματωμένα συστήματα (ιδίως IoT) χωρίς λειτουργικό σύστημα.


## Κεφάλαιο 5: Αρχιτεκτονικός Σχεδιασμός και Μικροαρχιτεκτονική Βελτιστοποίηση

<!-- NOTE: Κάθε ενότητα 5.x πρέπει να ξεκινά με σύντομη εισαγωγή (2-3 γραμμές)
     που εξηγεί *τι αποφασίζεται* πριν αναλυθούν τα tracks.
     Επιπλέον, στην αρχή του κεφαλαίου, μετά την εισαγωγή και πριν το §5.1,
     να μπει ένα profiling overview figure (pie chart) που δείχνει τα τρία
     κύρια bottlenecks (vect_mul, Keccak, Decode) ως ποσοστά instret.
     Αυτό δημιουργεί motivation bridge από τη θεωρία στη βελτιστοποίηση.
     Ref: notes/notes_detailed.md — Discovery 6 (Amdahl's Law):
     "polynomial multiplication was originally about 96% of decaps.
     Once optimized, remaining ~4% dominates. Whole-system speedup
     asymptotically approaches 25x-28x." Αυτή η παρατήρηση αιτιολογεί
     γιατί μετά το polymul χρειάζεται Keccak + Decode βελτιστοποίηση. -->

* **5.1 Σχεδιαστικό Πλαίσιο: Hardware Tiers και Implementation Tracks:**
    <!-- Ref: notes/notes_detailed.md — Discovery 3 (3-Tier Hardware Profile):
         πίνακας Tier 1/2/3 με ISA, champion, speedups.
         "The tiered decomposition maps directly to RISC-V's modular ISA philosophy" -->
    * **5.1.1 Hardware Tier 1 (Scalar Deployment Class):** Στόχευση σε προφίλ `rv64gc` με scalar cryptographic extensions (`Zbc`/`Zbb`, και `Zbkb` όπου απαιτείται).
    * **5.1.2 Hardware Tier 2 (Base-RVV Deployment Class):** Στόχευση σε προφίλ `rv64gcv` χωρίς `Zvbc`/`Zvbb`, για πλατφόρμες με ευρύτερη διαθεσιμότητα διανυσματικής μονάδας.
    * **5.1.3 Hardware Tier 3 (Full Vector-Crypto Deployment Class):** Στόχευση σε `rv64gcv` με `Zvbc`/`Zvbb` και ελεγχόμενη εξάρτηση από `zvl256b`.
    * **5.1.4 Implementation Track A:** Scalar μονοπάτι με έμφαση σε `Zbc`/`Zbb` και strict non-vector execution discipline.
    * **5.1.5 Implementation Track B:** Base RVV μονοπάτι (χωρίς vector-crypto επεκτάσεις), με επιλεκτική διανυσματοποίηση αλγοριθμικών σταδίων.
    * **5.1.6 Implementation Track C:** Full vector-crypto μονοπάτι με αξιοποίηση `Zvbc`/`Zvbb` σε κρίσιμους πυρήνες.
    <!-- NOTE: Ο πίνακας Track-to-Tier mapping εντάσσεται ως Figure/Table
         εντός της ενότητας §5.1, χωρίς ξεχωριστή υπο-ενότητα. -->
* **5.2 Επιτάχυνση Πολυωνυμικού Πολλαπλασιασμού στο $GF(2)[x]$:**
    <!-- NOTE: Εισαγωγική παράγραφος πριν τα tracks: "Γιατί Karatsuba αντί FAFFT;"
         O(n log n) FAFFT υπερτερεί ασυμπτωτικά αλλά σε πρακτικά μεγέθη HQC (n≈17k bits)
         ο σταθερός παράγοντας (data shuffling, butterfly permutations χωρίς SIMD)
         καθιστά τον AP-2 Karatsuba ταχύτερο κατά 56× σε scalar RISC-V.
         Ref: notes/notes_detailed.md — "Discovery 5.3.2.Β — FAFFT Paradox" -->
    * **5.2.1 Track A — Scalar/Zbc Path:** Μετάβαση από bit-serial schoolbook σε word-level `clmul`/`clmulh` Karatsuba φύλλα.
    * **5.2.2 Track B — RVV-Assisted Path:** Διανυσματοποίηση combine και data-movement βρόχων με RVV 1.0, με στόχο μείωση register pressure.
    * **5.2.3 Track C — Zvbc Leaf Path:** Εξειδίκευση leaf kernels με `vclmul.vv`/`vclmulh.vv` και επιθετικό unrolling.
    * **5.2.4 Shared Mechanisms — Memory Discipline και AP-2 Generator:** Assign-on-first-write πειθαρχία μνήμης και compile-time παραγωγή recursion trees μέσω Python generator.
* **5.3 Επανασχεδιασμός Keccak / SHA-3:**
    * **5.3.1 Track A — Scalar/Zbb Path:** Scalar πυρήνας με αξιοποίηση `rori` και σταθερή πολιτική μετασχηματισμών.
    * **5.3.2 Track B — RVV-Only Path και "RVV Paradox":** Αξιολόγηση base-RVV διανυσματοποίησης και τεκμηριωμένη απόρριψη λόγω αυξημένου instruction footprint. Χωρίς `vror.vv` (απαιτεί Zvbb), κάθε rotation γίνεται 3 εντολές αντί 1.
    * **5.3.3 Track C — Zvbb SoA 2-way Path:** Δικαναλική αρχιτεκτονική SoA με `vror.vi`/`vror.vx`, elimination των gather-heavy bottlenecks και παραγωγική ενοποίηση. 2-way είναι το αρχιτεκτονικό sweet spot: 4-way προκαλεί register spilling (25 lanes × 4 > 32 vector registers).
    * **5.3.4 Cross-Track I/O Optimizations:** Βελτιστοποίηση absorb/squeeze μονοπατιών, byte-streaming loops και ευθυγραμμισμένες 64-bit μεταφορές.
* **5.4 Σχεδιασμός Decode (RM+RS) με Συμμετρική Track Πρόοδο:**
    * **5.4.1 Track A — Scalar Decode Path:** Baseline scalar υλοποίηση Reed-Muller και Reed-Solomon.
    * **5.4.2 Track B — RVV RM + RS Axis A:** Διανυσματοποίηση Reed-Muller πυρήνων και επιτάχυνση RS συνδρόμων (Axis A) με lane-packed πράξεις.
    * **5.4.3 Track C — Zvbc RS Axes B και D:** Διανυσματοποίηση Berlekamp-Massey και Forney inner loops μέσω `vclmul`-based GF(2^8) pipeline.
    * **5.4.4 RS Axis C (Scalar Status):** Τεκμηρίωση διατήρησης του root-finding άξονα σε scalar μορφή και ορίων τρέχουσας διανυσματοποίησης.
* **5.5 VLEN Scaling: Ανάλυση Κλιμάκωσης Διανυσματικού Εύρους:**
    * **5.5.1 Μοντέλο Κλιμάκωσης και Θεωρητικό Όριο:** Ανάλυση του ποσοστού instret που εξαρτάται από VLEN — μόνο ~27% (Karatsuba recomposition) ωφελείται, ενώ ~54% (vclmul_leaf + Keccak) παραμένει σταθερό.
    * **5.5.2 Proportional Leaf Threshold: Ανάλυση Αστοχίας:** Τεκμηρίωση της στρατηγικής threshold = VLMAX και εξήγηση αποτυχίας λόγω O(n²) κόστους schoolbook leaf.
    * **5.5.3 RVV Recomposition Loops: Τελική Αρχιτεκτονική:** Αντικατάσταση scalar 4×-unrolled loops στο generated header με VLA helpers (`rvv_copy_u64`, `rvv_xor3_u64`, `rvv_xor_u64`) που κλιμακώνουν με VLMAX.
    * **5.5.4 Πειραματικά Αποτελέσματα VLEN Sweep:** Παρουσίαση μετρήσεων instret για VLEN ∈ {256, 512, 1024, 2048} — sweet spot στο VLEN=512 με 5–6.5% μείωση.
    * **5.5.5 Αρχιτεκτονικοί Περιορισμοί Κλιμάκωσης:** Γιατί vclmul_leaf (O(n²), σταθερό n=16) και Keccak (σταθερή 25-lane κατάσταση) δεν ωφελούνται από ευρύτερο VLEN. Επίσης τεκμηρίωση ότι μεγαλύτερο VLEN μπορεί να *αυξήσει* instret (fixed-width coding patterns, VL-dependent control overhead).
* **5.6 Cross-Track Υποδομή και Runtime Μηχανισμοί:**
    * **5.6.1 Global Utility Επιταχύνσεις:** Κοινές βελτιστοποιήσεις βοηθητικών kernels και δομών δεδομένων για όλα τα tracks.
    * **5.6.2 `vect_add` και Συναφείς Πρωτόγονες Πράξεις:** Ομογενοποιημένη μεταχείριση bitwise vector πράξεων με φορητότητα μεταξύ tiers.
    * **5.6.3 Memory Alignment και Data-Movement Layouts:** Πολιτικές ευθυγράμμισης, packing και πρόσβασης μνήμης για ελαχιστοποίηση μεταφορικού κόστους.
    * **5.6.4 Runtime Βιβλιοθήκη (`memcpy`):** Βελτιστοποιήσεις επιπέδου runtime με στόχο διαλειτουργικότητα bare-metal και benchmark σταθερότητα.

## Κεφάλαιο 6: Αποτελέσματα και Αξιολόγηση Απόδοσης
* **6.1 Απομονωμένες Μικρο-αξιολογήσεις (Isolated Micro-benchmarks):** Συστηματική σύγκριση Reference και Optimized επιδόσεων (`instret`) για `vect_mul`, `keccak` και `decode`.
* **6.2 Αξιολόγηση Πλήρους Κρυπτοσυστήματος (End-to-End KEM):** Παρουσίαση τελικών πινάκων KEM (Keypair, Encaps, Decaps) για όλα τα NIST security levels (128, 192, 256).
* **6.3 Συγκριτική Αξιολόγηση με Ετερογενείς Αρχιτεκτονικές:** Πίνακας cross-architecture (Intel AVX-512, AMD Zen4, ARM Cortex-M4, FAFFT RISC-V, παρούσα εργασία) και μεθοδολογική σημείωση: instret ≠ cycles, αλλά instret = κάτω φράγμα κύκλων. Ανάλυση του χάσματος Cortex-M4 (απουσία clmul) και γιατί FAFFT O(n log n) χάνει πρακτικά από AP-2 Karatsuba σε scalar cores.
* **6.4 Κλιμάκωση ανά Security Level (Cross-Level Comparison):** Συγκριτικός πίνακας HQC-128 / HQC-192 / HQC-256 και ανάλυση scaling behavior μεταξύ επιπέδων ασφαλείας.
* **6.5 ISA Constraint Audits και Ablation Study:**
    * **6.5.1 Mandatory Extensions ανά Track/Tier:** Τυπικός ορισμός υποχρεωτικών ISA απαιτήσεων ανά deployment class.
    * **6.5.2 Ablation Testing Framework:** Ποσοτικός προσδιορισμός της μεμονωμένης συνεισφοράς των Zbc, Zvbc, Zbb και Zvbb μέσω συστηματικών ISA ablations.
    * **6.5.3 Compiler Auto-Lowering και Όρια Εμπιστοσύνης:** Τεκμηρίωση auto-lowering μοτίβων (συμπεριλαμβανομένων `Zba`, `Zicond`) και των ορίων τους σε κρίσιμα kernels. Απόδειξη ότι compilers δεν συνθέτουν αυτόματα Zvbc/Zvbb primitives από generic C — hand-written intrinsics είναι αναγκαία.
    * **6.5.4 PRNG Acceleration (Zvkned): Test-Harness Boundary:** Οριοθέτηση της χρήσης PRNG acceleration στο test-harness domain χωρίς μετατόπιση του κύριου security-critical execution path.
* **6.6 Συγκριτική Αξιολόγηση Μεταγλωττιστών (Compiler Comparison):** Ανάλυση Clang/LLVM και GCC ως προς register management, auto-vectorization και σταθερότητα παραγόμενου κώδικα.
* **6.7 Αναπαραγωγιμότητα και Περιγραφή Artifact:** Δομή του repository, αντιστοίχιση πινάκων αποτελεσμάτων σε συγκεκριμένα scripts και build configurations, και οδηγίες πλήρους αναπαραγωγής πειραμάτων.

## Κεφάλαιο 7: Συμπεράσματα και Μελλοντικές Επεκτάσεις
* **7.1 Σύνοψη και Αποτίμηση:** Ανακεφαλαίωση των speedups.
* **7.2 Περιορισμοί της Υλοποίησης:** Αναφορά στις τρέχουσες συνθήκες της αγοράς υλικού (διαθεσιμότητα native Zvbc) και στους εγγενείς περιορισμούς των instruction-accurate εξομοιωτών (Spike) έναντι cycle-accurate μοντέλων.
* **7.3 Προτάσεις για Μελλοντική Έρευνα:** Constant-time ανάλυση και side-channel resistance (dudect/ctgrind), εκτέλεση σε real hardware (native RISC-V boards), επέκταση σε RTL soft-cores (FPGA), και περαιτέρω vectorization των εκκρεμών σταδίων του decode.

## Παράρτημα Α: Οδηγός Εγκατάστασης και Παραμετροποίησης Περιβάλλοντος
* **Α.1 Μεταγλώττιση του RISC-V Toolchain**
* **Α.2 Εγκατάσταση του Spike ISS**
