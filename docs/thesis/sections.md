## Κεφάλαιο 1: Εισαγωγή
* **1.1 Η Ανάγκη για Post-Quantum Κρυπτογραφία (PQC):** Η κβαντική απειλή, ο αλγόριθμος του Shor και η αναγκαιότητα άμεσης μετάβασης από τα κλασικά σχήματα (RSA, ECC) στα μετα-κβαντικά.
* **1.2 Ο Αλγόριθμος HQC και το Πλαίσιο Standardization του NIST:** Code-based κρυπτογραφία, η εξέλιξη του διαγωνισμού PQC του NIST και η θεσμοθέτηση του HQC ως επίσημο standard.
* **1.3 Το Οικοσύστημα του RISC-V και το Cryptographic Agility:** Η σημασία των ανοιχτών αρχιτεκτονικών και η έννοια του cryptographic agility μέσω ανοιχτών, ratified επεκτάσεων.
* **1.4 Σκοπός και Συνεισφορά της Διατριβής:** Ανάπτυξη αμιγώς βελτιστοποιημένου λογισμικού (pure software acceleration) με χρήση standard επεκτάσεων, αποφεύγοντας την ανάγκη για custom/proprietary hardware coprocessors.
* **1.5 Δομή της Εργασίας:** Σύντομη περιγραφή των περιεχομένων των επόμενων κεφαλαίων.

## Κεφάλαιο 2: Θεωρητικό Υπόβαθρο HQC & Κρυπτογραφίας
* **2.1 Key Encapsulation Mechanism (KEM) του HQC:** Μαθηματική θεμελίωση και λειτουργική ανάλυση των διεργασιών Keygen, Encapsulation και Decapsulation.
* **2.2 Πολυωνυμικός Πολλαπλασιασμός σε Δυαδικά Πεδία:** Ανάλυση της υπολογιστικής πράξης στον δακτύλιο $GF(2)[x] / (x^N - 1)$, μαθηματική ανάλυση του αλγορίθμου Karatsuba και ανάδειξή του ως το αρχικό bottleneck του συστήματος.
* **2.3 Συναρτήσεις Hash SHA-3 / Keccak:** Αρχιτεκτονική σπόγγου (Sponge Construction), ανάλυση των φάσεων Absorbing και Squeezing, και η δομή της εσωτερικής μετάθεσης Keccak-f[1600].
* **2.4 Κώδικες Διόρθωσης Σφαλμάτων (Error-Correcting Codes):** Θεωρία συνδυασμένης αποκωδικοποίησης (Concatenated Decoding) με χρήση κωδίκων Reed-Muller (RM) και Reed-Solomon (RS) μέσω του Fast Hadamard Transform.

## Κεφάλαιο 3: Μικροαρχιτεκτονική & Επεκτάσεις Κρυπτογραφίας RISC-V
* **3.1 Αρχιτεκτονική Φιλοσοφία και Σχεδιαστικές Αρχές του RISC-V**
* **3.2 Scalar Cryptographic Extensions:** Ανάλυση των Zbc (Carry-less multiplication) και Zbb/Zbkb (Bit manipulation instructions).
* **3.3 Vector Unit Architecture (RVV 1.0):** Η αρχιτεκτονική της διανυσματικής μονάδας, διαχείριση καταχωρητών και οι έννοιες VLEN, LMUL και Vector Chaining.
* **3.4 Vector Cryptographic Extensions:** Αναλυτική παρουσίαση των διανυσματικών κρυπτογραφικών επεκτάσεων Zvbc και Zvbb.
* **3.5 Σχετικές Εργασίες (Related Work):** Ανασκόπηση της σύγχρονης βιβλιογραφίας. Ανάλυση των AVX2 x86_64 υλοποιήσεων αναφοράς, του PQRV framework (TCHES 2025) και της προσέγγισης Frobenius Additive FFT (2025).

## Κεφάλαιο 4: Περιβάλλον Εξομοίωσης & Μεθοδολογία Μετρήσεων
* **4.1 Αρχιτεκτονική του Εξομοιωτή Spike ISS και του QEMU:** Λειτουργία και παραμετροποίηση κατά την αρχιτεκτονική εξομοίωση διανυσματικών διεργασιών.
* **4.2 Cross-Compilation Toolchains & Compiler Profiles:** Σύγκριση Clang/LLVM vs GCC, optimization flags (`-O3`, `-march`) και επίδραση στην αυτόματη διανυσματοποίηση.
* **4.3 Μεθοδολογία Benchmarking & Καταμέτρηση Δυναμικών Εντολών:** Καταγραφή του instruction footprint μέσω dynamic instruction counts (`instret`). Ανάλυση line-level hotspots μέσω DWARF debug symbols.
* **4.4 Framework Επαλήθευσης Ορθότητας:** Αυτοματοποιημένος έλεγχος συμμόρφωσης με τα επίσημα Known Answer Tests (KAT) του NIST για τα επίπεδα ασφαλείας HQC-128, 192 και 256.

## Κεφάλαιο 5: Αρχιτεκτονική Σχεδίαση και Βελτιστοποίηση Υλοποίησης
* **5.1 Αρχικό Προφίλ Εκτέλεσης (Reference Profiling) & Στρατηγική Bottleneck Shifting:** Παρουσίαση του αρχικού πίνακα Share εκτέλεσης και ανάλυση της μετατόπισης των hotspots βάσει του Νόμου του Amdahl.
* **5.2 Διανυσματική Επιτάχυνση Πολυωνυμικού Πολλαπλασιασμού (Polymul):**
    * 5.2.1 Σχεδίαση Διανυσματικών Δέντρων Karatsuba και Διαχείριση Καταχωρητών.
    * 5.2.2 Βελτιστοποίηση Φύλλων μέσω `vclmul.vv` (Zvbc) και 4× Unrolled Register Combine Loops.
* **5.3 Βελτιστοποίηση του Πυρήνα Keccak / SHA-3:**
    * 5.3.1 Hoisting Πινάκων Μετάθεσης έξω από το Round Loop και εξάλειψη των Redundant Memory Loads (`vle64.v`).
    * 5.3.2 Διανυσματοποίηση του Theta Step μέσω In-Register Parity Diffusion (`vslidedown`, `vmv.x.s`).
    * 5.3.3 Σχεδίαση και Υλοποίηση του Δικαναλικού 2-way Parallel SoA (Paired-Stream Keygen) έναντι του Single-Stream Local Fast-Path.
    * 5.3.4 Ελαχιστοποίηση των I/O Overheads μέσω Aligned 64-bit Block Transfers.
* **5.4 Διανυσματική Σχεδίαση Σταδίου Αποκωδικοποίησης (Decode):**
    * 5.4.1 Επιτάχυνση του Reed-Muller List Decoder με χρήση Strided Loads (`vlse16`) για την κατάργηση των Register Permutations.
    * 5.4.2 One-Pass Διανυσματική Εύρεση Μεγίστου (`vfirst_m`, `vcpop`) έναντι του Σειριακού Binary Search.
    * 5.4.3 Πλήρες Loop Unrolling της `gf_reduce` και Inline Assembly `clmul` επιτάχυνση στη `gf_square`.

## Κεφάλαιο 6: Αποτελέσματα και Αξιολόγηση Απόδοσης
* **6.1 Ανάλυση Απομονωμένων Μικρο-αξιολογήσεων (Isolated Micro-benchmarks):** Σύγκριση Reference vs Optimized επιδόσεων για τα `vect_mul`, `keccak` και `decode`.
* **6.2 Αξιολόγηση Πλήρους Κρυπτοσυστήματος (End-to-End KEM Suite):** Παρουσίαση των τελικών αποτελεσμάτων (Keypair, Encaps, Decaps) και της μείωσης του footprint κάτω από το όριο του 1M εντολών (~926K).
* **6.3 Αρχιτεκτονική Αξιολόγηση Επεκτάσεων (Ablation Study):** Ποσοτικός προσδιορισμός της συνεισφοράς των επεκτάσεων Zbc, Zvbc, Zbb και Zvbb στα επιμέρους hotspots.
* **6.4 Σύγκριση Μεταγλωττιστών (Compiler Profiling):** Αξιολόγηση των Clang/LLVM και GCC ως προς το LICM και το Register Allocation.

## Κεφάλαιο 7: Συμπεράσματα και Μελλοντικές Επεκτάσεις
* **7.1 Σύνοψη και Αποτίμηση:** Ανακεφαλαίωση των speedups και αξιολόγηση της στρατηγικής "pure software acceleration".
* **7.2 Περιορισμοί της Υλοποίησης:** Αναφορά στις τρέχουσες συνθήκες της αγοράς υλικού (διαθεσιμότητα native Zvbc) και στους εγγενείς περιορισμούς των instruction-accurate εξομοιωτών (Spike) έναντι cycle-accurate μοντέλων.
* **7.3 Προτάσεις για Μελλοντική Έρευνα:** Ενσωμάτωση constant-time ελέγχων (dudect/ctgrind), μεταφορά του κώδικα σε RTL soft-cores (CVA6) σε FPGA για cycle-accurate μετρήσεις και διερεύνηση Toom-Cook 3-way σχεδιάσεων.

## Παράρτημα Α: Οδηγός Εγκατάστασης και Παραμετροποίησης Περιβάλλοντος
* **Α.1 Μεταγλώττιση του RISC-V GNU/LLVM Cross-Compilation Toolchain**
* **Α.2 Εγκατάσταση και Παραμετροποίηση του Spike ISS και του PK (Proxy Kernel)**
* **Α.3 Ρύθμιση του Αυτοματοποιημένου Framework Δοκιμών και Benchmarking**