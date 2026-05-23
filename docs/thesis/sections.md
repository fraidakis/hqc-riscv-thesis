# Πλήρης Διάρθρωση Διπλωματικής Εργασίας

## Προκαταρκτικές Σελίδες
* **Περίληψη (Ελληνικά)**
* **Abstract (Αγγλικά)**
* **Περιεχόμενα**

## Κεφάλαιο 1: Εισαγωγή
* **1.1 Η Κβαντική Απειλή και η Post-Quantum Κρυπτογραφία:** Ο αλγόριθμος του Shor και η αναγκαιότητα μετάβασης από τα κλασικά σχήματα (RSA, ECC) σε μετα-κβαντικά.
* **1.2 Το Πλαίσιο Standardization του NIST και ο Αλγόριθμος HQC:** Η θέση της code-based κρυπτογραφίας και η ανάδειξη του HQC ως επίσημου προτύπου.
* **1.3 Cryptographic Agility και Pure Software Acceleration: Γιατί RISC-V:**
    * **Μέρος Α - Cryptographic Agility ως Απαίτηση:** Η μεταβαλλόμενη φύση των PQC προτύπων και η ανάγκη συνεχούς προσαρμογής.
    * **Μέρος Β - Pure Software Acceleration ως Απάντηση:** Το πλεονέκτημα των ευέλικτων ratified ISA extensions έναντι εξειδικευμένων κυκλωμάτων. *(Η αρχιτεκτονική τεκμηρίωση των επεκτάσεων παρουσιάζεται στο Κεφάλαιο 3.)*
* **1.4 Σκοπός και Συνεισφορά της Διατριβής:** Ανάπτυξη αμιγώς βελτιστοποιημένου λογισμικού για τον HQC στο οικοσύστημα του RISC-V.
* **1.5 Δομή της Εργασίας:** Σύντομη περιγραφή των επόμενων κεφαλαίων.

## Κεφάλαιο 2: Θεωρητικό Υπόβαθρο HQC και Κρυπτογραφίας
* **2.1 Key Encapsulation Mechanism (KEM) του HQC:** Μαθηματική θεμελίωση και λειτουργική ανάλυση των διεργασιών Keygen, Encapsulation και Decapsulation.
* **2.2 Πολυωνυμικός Πολλαπλασιασμός σε Δυαδικά Πεδία:** Ανάλυση της πράξης στον δακτύλιο $GF(2)[x] / (x^N - 1)$ και μαθηματική μελέτη του Karatsuba ως main hotspot.
* **2.3 Συναρτήσεις Hash SHA-3 / Keccak:** Αρχιτεκτονική σπόγγου (Sponge Construction) και δομή της εσωτερικής μετάθεσης Keccak-f[1600].
* **2.4 Κώδικες Διόρθωσης Σφαλμάτων (Error-Correcting Codes):** Θεωρία συνδυασμένης αποκωδικοποίησης (Concatenated Decoding) κωδίκων Reed-Muller (RM) και Reed-Solomon (RS).

## Κεφάλαιο 3: Αρχιτεκτονική RISC-V και Κρυπτογραφικές Επεκτάσεις
* **3.1 Αρχιτεκτονική Φιλοσοφία και Σχεδιαστικές Αρχές του RISC-V**
* **3.2 Scalar Cryptographic Extensions:** Αναλυτική παρουσίαση των Zbc (carry-less multiplication) και Zbb/Zbkb (bit-manipulation instructions).
* **3.3 Vector Unit Architecture (RVV 1.0):** Αρχιτεκτονική της διανυσματικής μονάδας (VLEN, LMUL, Vector-Length Agnosticism).
* **3.4 Vector Cryptographic Extensions:** Ανάλυση των διανυσματικών επεκτάσεων Zvbc και Zvbb.
* **3.5 Σχετικές Εργασίες (Related Work):** Ανασκόπηση της βιβλιογραφίας

## Κεφάλαιο 4: Πειραματική Υποδομή και Μεθοδολογία Αξιολόγησης
* **4.1 Αρχιτεκτονική των Εξομοιωτών Spike ISS και QEMU**
* **4.2 Cross-Compilation Toolchains και Compiler Profiles:** Συγκριτική αξιολόγηση Clang/LLVM και GCC, με έμφαση στα flags βελτιστοποίησης (`-O3`, `-march`).
* **4.3 Μεθοδολογία Benchmarking:** Καταμέτρηση δυναμικών εντολών (`instret`) και profiling μέσω DWARF debug symbols.
* **4.4 Επαλήθευσης Ορθότητας:** Αυτοματοποιημένος έλεγχος μέσω των Known Answer Tests (KAT) του NIST.

## Κεφάλαιο 5: Αρχιτεκτονικός Σχεδιασμός και Μικροαρχιτεκτονική Βελτιστοποίηση Υλοποίησης
* **5.1 Control Plane: Hardware Tiers, Tracks και Κανόνες Αντιστοίχισης:**
    * **5.1.1 Hardware Tier 1 (Scalar Deployment Class):** Στόχευση σε προφίλ `rv64gc` με scalar cryptographic extensions (`Zbc`/`Zbb`, και `Zbkb` όπου απαιτείται).
    * **5.1.2 Hardware Tier 2 (Base-RVV Deployment Class):** Στόχευση σε προφίλ `rv64gcv` χωρίς `Zvbc`/`Zvbb`, για πλατφόρμες με ευρύτερη διαθεσιμότητα διανυσματικής μονάδας.
    * **5.1.3 Hardware Tier 3 (Full Vector-Crypto Deployment Class):** Στόχευση σε `rv64gcv` με `Zvbc`/`Zvbb` και ελεγχόμενη εξάρτηση από `zvl256b`.
    * **5.1.4 Implementation Track A:** Scalar μονοπάτι με έμφαση σε `Zbc`/`Zbb` και strict non-vector execution discipline.
    * **5.1.5 Implementation Track B:** Base RVV μονοπάτι (χωρίς vector-crypto επεκτάσεις), με επιλεκτική διανυσματοποίηση αλγοριθμικών σταδίων.
    * **5.1.6 Implementation Track C:** Full vector-crypto μονοπάτι με αξιοποίηση `Zvbc`/`Zvbb` σε κρίσιμους πυρήνες.
    * **5.1.7 Πίνακας Track-to-Section Mapping:** Ρητή χαρτογράφηση Track-to-Tier και περιορισμός κάθε track στα επιτρεπτά ISA domains για αναπαραγώγιμη αξιολόγηση.
* **5.2 Επιτάχυνση Πολυωνυμικού Πολλαπλασιασμού στο $GF(2)[x]$:**
    * **5.2.1 Track A - Scalar/Zbc Path:** Μετάβαση από bit-serial schoolbook σε word-level `clmul`/`clmulh` Karatsuba φύλλα.
    * **5.2.2 Track B - RVV-Assisted Path:** Διανυσματοποίηση combine και data-movement βρόχων με RVV 1.0, με στόχο μείωση register pressure.
    * **5.2.3 Track C - Zvbc Leaf Path:** Εξειδίκευση leaf kernels με `vclmul.vv`/`vclmulh.vv` και επιθετικό unrolling.
    * **5.2.4 Shared Mechanisms - Memory Discipline και AP-2 Generator:** Assign-on-first-write πειθαρχία μνήμης και compile-time παραγωγή recursion trees μέσω Python generator.
* **5.3 Επανασχεδιασμός Keccak / SHA-3:**
    * **5.3.1 Track A - Scalar/Zbb Path:** Scalar πυρήνας με αξιοποίηση `rori` και σταθερή πολιτική constant-time μετασχηματισμών.
    * **5.3.2 Track B - RVV-Only Path και "RVV Paradox":** Αξιολόγηση base-RVV διανυσματοποίησης και τεκμηριωμένη απόρριψη λόγω αυξημένου instruction footprint.
    * **5.3.3 Track C - Zvbb SoA 2-way Path:** Δικαναλική αρχιτεκτονική SoA με `vror.vi`/`vror.vx`, elimination των gather-heavy bottlenecks και παραγωγική ενοποίηση.
    * **5.3.4 Cross-Track I/O Optimizations:** Βελτιστοποίηση absorb/squeeze μονοπατιών, byte-streaming loops και ευθυγραμμισμένες 64-bit μεταφορές.
* **5.4 Σχεδιασμός Decode (RM+RS) με Συμμετρική Track Πρόοδο:**
    * **5.4.1 Track A - Scalar Decode Path:** Baseline scalar υλοποίηση Reed-Muller και Reed-Solomon με constant-time masking discipline.
    * **5.4.2 Track B - RVV RM + RS Axis A:** Διανυσματοποίηση Reed-Muller πυρήνων και επιτάχυνση RS συνδρόμων (Axis A) με lane-packed πράξεις.
    * **5.4.3 Track C - Zvbc RS Axes B και D:** Διανυσματοποίηση Berlekamp-Massey και Forney inner loops μέσω `vclmul`-based GF(2^8) pipeline.
    * **5.4.4 RS Axis C (Scalar Status):** Τεκμηρίωση διατήρησης του root-finding άξονα σε scalar μορφή και ορίων τρέχουσας διανυσματοποίησης.
* **5.5 VLEN Scaling: Ανάλυση Κλιμάκωσης Διανυσματικού Εύρους:**
    * **5.5.1 Μοντέλο Κλιμάκωσης και Θεωρητικό Όριο:** Ανάλυση του ποσοστού instret που εξαρτάται από VLEN — μόνο ~27% (Karatsuba recomposition) ωφελείται, ενώ ~54% (vclmul_leaf + Keccak) παραμένει σταθερό.
    * **5.5.2 Αποτυχημένη Προσέγγιση — Proportional Leaf Threshold:** Τεκμηρίωση της στρατηγικής threshold = VLMAX και εξήγηση αποτυχίας λόγω O(n²) κόστους schoolbook leaf.
    * **5.5.3 Επιτυχημένη Προσέγγιση — RVV Recomposition:** Αντικατάσταση scalar 4×-unrolled loops στο generated header με VLA helpers (`rvv_copy_u64`, `rvv_xor3_u64`, `rvv_xor_u64`) που κλιμακώνουν με VLMAX.
    * **5.5.4 Πειραματικά Αποτελέσματα VLEN Sweep:** Παρουσίαση μετρήσεων instret για VLEN ∈ {256, 512, 1024, 2048} — sweet spot στο VLEN=512 με 5–6.5% μείωση.
    * **5.5.5 Αρχιτεκτονικοί Περιορισμοί Κλιμάκωσης:** Γιατί vclmul_leaf (O(n²), σταθερό n=16) και Keccak (σταθερή 25-lane κατάσταση) δεν ωφελούνται από ευρύτερο VLEN.
* **5.6 Cross-Track Υποδομή και Runtime Μηχανισμοί:**
    * **5.6.1 Global Utility Επιταχύνσεις:** Κοινές βελτιστοποιήσεις βοηθητικών kernels και δομών δεδομένων για όλα τα tracks.
    * **5.6.2 `vect_add` και Συναφείς Πρωτόγονες Πράξεις:** Ομογενοποιημένη μεταχείριση bitwise vector πράξεων με φορητότητα μεταξύ tiers.
    * **5.6.3 Memory Alignment και Data-Movement Layouts:** Πολιτικές ευθυγράμμισης, packing και πρόσβασης μνήμης για ελαχιστοποίηση μεταφορικού κόστους.
    * **5.6.4 Runtime Βιβλιοθήκη (`memcpy`):** Βελτιστοποιήσεις επιπέδου runtime με στόχο διαλειτουργικότητα bare-metal και benchmark σταθερότητα.
* **5.7 ISA Constraint Audits και Πολιτική Επικύρωσης:**
    * **5.7.1 Mandatory Extensions ανά Track/Tier:** Τυπικός ορισμός υποχρεωτικών ISA απαιτήσεων ανά deployment class.
    * **5.7.2 Ablation Testing Framework:** Ποσοτική αποτίμηση της συνεισφοράς κάθε επέκτασης μέσω συστηματικών ISA ablations.
    * **5.7.3 Compiler Auto-Lowering και Όρια Εμπιστοσύνης:** Τεκμηρίωση auto-lowering μοτίβων (συμπεριλαμβανομένων `Zba`, `Zicond`) και των ορίων τους σε κρίσιμα kernels.
    * **5.7.4 PRNG (`Zvkned`) Test-Harness Boundary Realization:** Οριοθέτηση της χρήσης PRNG acceleration στο test-harness domain χωρίς μετατόπιση του κύριου security-critical execution path.

## Κεφάλαιο 6: Αποτελέσματα και Αξιολόγηση Απόδοσης
* **6.1 Ανάλυση Απομονωμένων Μικρο-αξιολογήσεων (Isolated Micro-benchmarks):** Συστηματική σύγκριση Reference και Optimized επιδόσεων (`instret`) για `vect_mul`, `keccak` και `decode`.
* **6.2 Αξιολόγηση Πλήρους Κρυπτοσυστήματος (End-to-End KEM Suite):** Παρουσίαση τελικών πινάκων KEM (Keypair, Encaps, Decaps) για όλα τα NIST security levels (128, 192, 256).
* **6.3 Αρχιτεκτονική Αξιολόγηση Επεκτάσεων (Ablation Study):** Ποσοτικός προσδιορισμός της μεμονωμένης συνεισφοράς των Zbc, Zvbc, Zbb και Zvbb στα κρίσιμα hotspots.
* **6.4 Συγκριτική Αξιολόγηση Μεταγλωττιστών (Compiler Profiling):** Ανάλυση Clang/LLVM και GCC ως προς register management, auto-vectorization και σταθερότητα παραγόμενου κώδικα.

## Κεφάλαιο 7: Συμπεράσματα και Μελλοντικές Επεκτάσεις
* **7.1 Σύνοψη και Αποτίμηση:** Ανακεφαλαίωση των speedups.
* **7.2 Περιορισμοί της Υλοποίησης:** Αναφορά στις τρέχουσες συνθήκες της αγοράς υλικού (διαθεσιμότητα native Zvbc) και στους εγγενείς περιορισμούς των instruction-accurate εξομοιωτών (Spike) έναντι cycle-accurate μοντέλων.
* **7.3 Προτάσεις για Μελλοντική Έρευνα:** Εκτενέστεροι constant-time έλεγχοι (dudect/ctgrind), επέκταση σε RTL soft-cores (FPGA), και περαιτέρω vectorization των εκκρεμών σταδίων του decode.

## Παράρτημα Α: Οδηγός Εγκατάστασης και Παραμετροποίησης Περιβάλλοντος
* **Α.1 Μεταγλώττιση του RISC-V Toolchain**
* **Α.2 Εγκατάσταση του Spike ISS**
