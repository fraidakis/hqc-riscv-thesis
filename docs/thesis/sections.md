# Πλήρης Διάρθρωση Διπλωματικής Εργασίας

## Προκαταρκτικές Σελίδες
* **Περίληψη (Ελληνικά)**
* **Abstract (Αγγλικά)**
* **Περιεχόμενα**

## Κεφάλαιο 1: Εισαγωγή
* **1.1 Η Κβαντική Απειλή και η Post-Quantum Κρυπτογραφία:** Ο αλγόριθμος του Shor και η αναγκαιότητα μετάβασης από τα κλασικά σχήματα (RSA, ECC) σε μετα-κβαντικά.
* **1.2 Το Πλαίσιο Standardization του NIST και ο Αλγόριθμος HQC:** Η θέση της code-based κρυπτογραφίας και η ανάδειξη του HQC ως επίσημου προτύπου.
* **1.3 Cryptographic Agility & Pure Software Acceleration — Γιατί RISC-V:**
    * **Μέρος Α — Cryptographic Agility ως Απαίτηση:** Θεμελιώδη απαίτηση ασφαλείας λόγω της μεταβαλλόμενης φύσης των PQC προτύπων
    * **Μέρος Β — Pure Software Acceleration ως Απάντηση:** Το πλεονέκτημα των ευέλικτων, ratified ISA extensions έναντι εξειδικευμένων κυκλωμάτων. *(Η αρχιτεκτονική τεκμηρίωση των επεκτάσεων παρουσιάζεται στο Κεφάλαιο 3.)*
* **1.4 Σκοπός και Συνεισφορά της Διατριβής:** Ανάπτυξη αμιγώς βελτιστοποιημένου λογισμικού για τον HQC στο οικοσύστημα του RISC-V.
* **1.5 Δομή της Εργασίας:** Σύντομη περιγραφή των επόμενων κεφαλαίων.

## Κεφάλαιο 2: Θεωρητικό Υπόβαθρο HQC & Κρυπτογραφίας
* **2.1 Key Encapsulation Mechanism (KEM) του HQC:** Μαθηματική θεμελίωση και λειτουργική ανάλυση των διεργασιών Keygen, Encapsulation και Decapsulation.
* **2.2 Πολυωνυμικός Πολλαπλασιασμός σε Δυαδικά Πεδία:** Ανάλυση της πράξης στον δακτύλιο $GF(2)[x] / (x^N - 1)$ και μαθηματική ανάλυση του αλγορίθμου Karatsuba ως το κύριο υπολογιστικό εμπόδιο.
* **2.3 Συναρτήσεις Hash SHA-3 / Keccak:** Αρχιτεκτονική σπόγγου (Sponge Construction) και η δομή της εσωτερικής μετάθεσης Keccak-f[1600].
* **2.4 Κώδικες Διόρθωσης Σφαλμάτων (Error-Correcting Codes):** Θεωρία συνδυασμένης αποκωδικοποίησης (Concatenated Decoding) κωδίκων Reed-Muller (RM) και Reed-Solomon (RS).

## Κεφάλαιο 3: Αρχιτεκτονική RISC-V & Επεκτάσεις Κρυπτογραφίας RISC-V
* **3.1 Αρχιτεκτονική Φιλοσοφία και Σχεδιαστικές Αρχές του RISC-V**
* **3.2 Scalar Cryptographic Extensions:** Ανάλυση των Zbc (Carry-less multiplication) και Zbb/Zbkb (Bit manipulation instructions).
* **3.3 Vector Unit Architecture (RVV 1.0):** Η αρχιτεκτονική της διανυσματικής μονάδας (VLEN, LMUL, Vector-Length Agnosticism).
* **3.4 Vector Cryptographic Extensions:** Αναλυτική παρουσίαση των διανυσματικών επεκτάσεων Zvbc και Zvbb.
* **3.5 Σχετικές Εργασίες (Related Work):** Ανασκόπηση της βιβλιογραφίας. Σύγκριση με τις x86_64 AVX2 υλοποιήσεις αναφοράς, το PQRV framework και την προσέγγιση Frobenius Additive FFT.

## Κεφάλαιο 4: Πειραματική Υποδομή & Μεθοδολογία Αξιολόγησης 
* **4.1 Αρχιτεκτονική του Εξομοιωτή Spike ISS και του QEMU**
* **4.2 Cross-Compilation Toolchains & Compiler Profiles:** Σύγκριση Clang/LLVM vs GCC, optimization flags (`-O3`, `-march`).
* **4.3 Μεθοδολογία Benchmarking:** Καταμέτρηση δυναμικών εντολών (`instret`) και profiling μέσω DWARF debug symbols.
* **4.4 Framework Επαλήθευσης Ορθότητας:** Αυτοματοποιημένος έλεγχος μέσω των Known Answer Tests (KAT) του NIST.

## Κεφάλαιο 5: Αρχιτεκτονική Σχεδίαση και Βελτιστοποίηση Υλοποίησης
* **5.1 Αρχικό Προφίλ Εκτέλεσης (Reference Profiling) & Στρατηγική Bottleneck Shifting:** Παρουσίαση του αρχικού πίνακα Share εκτέλεσης και ανάλυση της μετατόπισης των hotspots βάσει του Νόμου του Amdahl.
* **5.2 Διανυσματική Επιτάχυνση Πολυωνυμικού Πολλαπλασιασμού:**
    * 5.2.1 Διανυσματικά Δέντρα Karatsuba & Διαχείριση Καταχωρητών (RVV 1.0) για την εξάλειψη του Register Spilling.
    * 5.2.2 Βελτιστοποίηση Φύλλων μέσω `vclmul.vv` (Zvbc) και 4× Unrolled Combine Loops.
* **5.3 Βελτιστοποίηση του Πυρήνα Keccak / SHA-3:**
    * 5.3.1 Εξέλιξη της Διανυσματοποίησης: Από το Gather-Based Interleaving στο Δικαναλικό 2-way Parallel SoA (Paired-Stream Keygen).
    * 5.3.2 Constant-Time Unrolling: Εξάλειψη των Loop-Carried Memory Loads μέσω Vector Immediates (`vror.vi`).
    * 5.3.3 Ελαχιστοποίηση των I/O Overheads με Aligned 64-bit Block Transfers.
* **5.4 Διανυσματική Σχεδίαση Σταδίου Αποκωδικοποίησης (Decode):**
    * 5.4.1 Fast Hadamard Transform με Strided Loads (`vlse16`).
    * 5.4.2 One-Pass Εύρεση Μεγίστου (`vfirst_m`, `vcpop`).

## Κεφάλαιο 6: Αποτελέσματα και Αξιολόγηση Απόδοσης
* **6.1 Ανάλυση Απομονωμένων Μικρο-αξιολογήσεων (Isolated Micro-benchmarks):** Συστηματική σύγκριση Reference vs Optimized επιδόσεων (instret) για τα `vect_mul`, `keccak` και `decode`.
* **6.2 Αξιολόγηση Πλήρους Κρυπτοσυστήματος (End-to-End KEM Suite):** Παρουσίαση των τελικών KEM πινάκων (Keypair, Encaps, Decaps) για όλα τα NIST security levels (128, 192, 256).
* **6.3 Αρχιτεκτονική Αξιολόγηση Επεκτάσεων (Ablation Study):** Ποσοτικός προσδιορισμός της μεμονωμένης συνεισφοράς των επεκτάσεων Zbc, Zvbc, Zbb και Zvbb στα hotspots.
* **6.4 Σύγκριση Μεταγλωττιστών (Compiler Profiling):** Αξιολόγηση των Clang/LLVM και GCC στην αυτόματη διαχείριση registers και vectorization.

## Κεφάλαιο 7: Συμπεράσματα και Μελλοντικές Επεκτάσεις
* **7.1 Σύνοψη και Αποτίμηση:** Ανακεφαλαίωση των speedups.
* **7.2 Περιορισμοί της Υλοποίησης:** Αναφορά στις τρέχουσες συνθήκες της αγοράς υλικού (διαθεσιμότητα native Zvbc) και στους εγγενείς περιορισμούς των instruction-accurate εξομοιωτών (Spike) έναντι cycle-accurate μοντέλων.
* **7.3 Προτάσεις για Μελλοντική Έρευνα:** Constant-time έλεγχοι (dudect) και εκτέλεση σε RTL soft-cores (FPGA) 

## Παράρτημα Α: Οδηγός Εγκατάστασης και Παραμετροποίησης Περιβάλλοντος
* **Α.1 Μεταγλώττιση του RISC-V Toolchain**
* **Α.2 Εγκατάσταση του Spike ISS**
