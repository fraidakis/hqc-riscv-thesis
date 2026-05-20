# Πλήρης Διάρθρωση Διπλωματικής Εργασίας

## Κεφάλαιο 1: Εισαγωγή

* **1.1 Η Ανάγκη για Post-Quantum Κρυπτογραφία (PQC):** Η κβαντική απειλή, ο αλγόριθμος του Shor και η αναγκαιότητα άμεσης μετάβασης από τα κλασικά σχήματα (RSA, ECC) στα μετα-κβαντικά.
* **1.2 Ο Αλγόριθμος HQC:** Βασικές αρχές της code-based κρυπτογραφίας, δομή των Hamming-weight σφαλμάτων και η θέση του HQC στο NIST PQC standardization.
* **1.3 Το Οικοσύστημα του RISC-V:** Η σημασία των ανοιχτών αρχιτεκτονικών και η έννοια του cryptographic agility μέσω ratified επεκτάσεων. Εισαγωγή στα Zbc, Zbb, Zbkb, V, Zvbc και Zvbb.
* **1.4 Σκοπός και Συνεισφορά της Διπλωματικής:** Ανάπτυξη αμιγώς βελτιστοποιημένου λογισμικού (pure software acceleration) με χρήση standard επεκτάσεων, αποφεύγοντας την ανάγκη για custom/proprietary hardware coprocessors.
* **1.5 Δομή της Εργασίας:** Σύντομη περιγραφή των περιεχομένων των επόμενων κεφαλαίων.

## Κεφάλαιο 2: Θεωρητικό Υπόβαθρο & Σχετικές Εργασίες

* **2.1 Key Encapsulation Mechanism (KEM) του HQC:** Μαθηματική θεμελίωση και λειτουργική ανάλυση των αλγορίθμων παραγωγής κλειδιών (Keygen), εγκλεισμού (Encapsulation) και αποεγκλεισμού (Decapsulation).
* **2.2 Πολυωνυμικός Πολλαπλασιασμός:** Ανάλυση της υπολογιστικής πράξης στον δακτύλιο $GF(2)[x] / (x^N - 1)$ και ανάδειξή της ως το παραδοσιακό bottleneck του αλγορίθμου.
* **2.3 Κρυπτογραφικές Συναρτήσεις Hash SHA-3 / Keccak:** Αρχιτεκτονική σπόγγου (Sponge Construction), ανάλυση των φάσεων Absorbing και Squeezing, και η δομή της εσωτερικής μετάθεσης Keccak-f[1600].
* **2.4 Κώδικες Διόρθωσης Σφαλμάτων (Error-Correcting Codes):** Θεωρία συνδυασμένης αποκωδικοποίησης (Concatenated Decoding) με χρήση κωδίκων Reed-Muller (RM) και Reed-Solomon (RS) μέσω του Fast Hadamard Transform.
* **2.5 Αρχιτεκτονικές Επεκτάσεις Κρυπτογραφίας RISC-V:**
    * **2.5.1 Scalar Extensions:** Ανάλυση των Zbc (Carry-less multiplication) και Zbb/Zbkb (Bit manipulation instructions).
    * **2.5.2 Vector Extensions:** Η αρχιτεκτονική της διανυσματικής μονάδας (RVV 1.0), οι έννοιες VLEN/LMUL και οι διανυσματικές κρυπτογραφικές οδηγίες Zvbc και Zvbb.
* **2.6 Επιθέσεις Πλευρικού Καναλιού (Side-Channel Attacks) [Προαιρετικό / TODO]:** Θεωρία χρονικών διαρροών (Timing Attacks) και η σημασία της constant-time εκτέλεσης κώδικα.
* **2.7 Σχετικές Εργασίες (Related Work):** Ανασκόπηση της σύγχρονης βιβλιογραφίας. Ανάλυση του PQRV framework (Zhang et al., TCHES 2025) για την επιτάχυνση Keccak/Kyber/Dilithium και της προσέγγισης FAFFT (2025) για την HQC.

## Κεφάλαιο 3: Αρχιτεκτονική Σχεδίαση και Βελτιστοποίηση Υλοποίησης

* **3.1 Αρχικό Προφίλ Εκτέλεσης και Στρατηγική Bottleneck Shifting:**
    * **3.1.1 Reference Profiling:** Ανάλυση της βασικής C υλοποίησης και ποσοτική ανάδειξη του πολυωνυμικού πολλαπλασιασμού ως το απόλυτο hotspot (>90%).
    * **3.1.2 Επαναληπτικό Profiling (Νόμος του Amdahl):** Πώς η ραγδαία επιτάχυνση του πολλαπλασιασμού μετατόπισε τα bottlenecks και αποκάλυψε την Keccak και το Decode ως τις νέες κυρίαρχες πηγές καθυστέρησης.
* **3.2 Βελτιστοποίηση Πολυωνυμικού Πολλαπλασιασμού:**
    * **3.2.1 Scalar Βελτιστοποιήσεις (Zbc):** Σχεδίαση αλγορίθμου Karatsuba με βελτιστοποίηση των τελικών φύλλων (leaf optimization) μέσω της scalar οδηγίας carry-less πολλαπλασιασμού `vclmul`.
    * **3.2.2 Vectorized Karatsuba (RVV & Zvbc):** Σχεδίαση αναδρομικής διανυσματικής δομής Karatsuba, διαχείριση registers για μεγάλα πολυώνυμα και αξιοποίηση της vector carry-less οδηγίας `vvclmul.vv`.
* **3.3 Βελτιστοποίηση Keccak / SHA-3 (The Vector Crypto Path):**
    * **3.3.1 Hoisting Πινάκων Μετάθεσης (Track B1):** Εξάλειψη των 96 επαναλαμβανόμενων vector οδηγιών φόρτωσης μνήμης (`vle64.v`) ανά permutation μέσω hoisting των σταθερών πινάκων έξω από το round loop.
    * **3.3.2 Διανυσματική Υλοποίηση Theta (Track B2):** Αντικατάσταση του store-to-stack buffer (200 bytes) με in-register vector extraction (`vslidedown`, `vmv.x.s`) και άμεσο διανυσματικό diffusion.
    * **3.3.3 Δικανάλια Παράλληλη Σχεδίαση (Track C2 — 2-way SoA):** Αξιοποίηση του Call-Graph Audit (ανάδειξη του `max_active_global=2`). Υλοποίηση της παράλληλης `KeccakF1600_StatePermute_x2` με Structure-of-Arrays (SoA) σε σταθερό `vl=2` για την ταυτόχρονη επεξεργασία των contexts του Keygen.
* **3.4 Βελτιστοποίηση Σταδίου Αποκωδικοποίησης (Decode):**
    * **3.4.1 Vectorized Fast Hadamard Transform:** Διανυσματική επιτάχυνση του Reed-Muller list decoder μέσω RVV.
    * **3.4.2 Αναδιοργάνωση Αλγορίθμου Reed-Solomon:** Βελτιστοποιήσεις στη διαχείριση registers και ευθυγράμμιση μνήμης για την ελαχιστοποίηση των spills.

## Κεφάλαιο 4: Μεθοδολογία και Περιβάλλον Επαλήθευσης

* **4.1 Περιβάλλον Ανάπτυξης και Εξομοίωσης:** Αρχιτεκτονική του Spike ISS (Instruction Set Simulator) και του QEMU. Ρύθμιση και παραμετροποίηση των cross-compilation toolchains (Clang/LLVM vs GCC).
* **4.2 Μεθοδολογία Benchmarking:** Καταμέτρηση Δυναμικών Εντολών (Dynamic Instruction Counts - `instret`). Ανάλυση των custom tracing και profiling scripts για την εξαγωγή line-level hotspots μέσω DWARF debug symbols.
* **4.3 Επαλήθευση Ορθότητας (Correctness Verification):** Ενσωμάτωση και αυτοματοποίηση των Known Answer Tests (KAT) της επίσημης προδιαγραφής HQC για τα επίπεδα ασφαλείας HQC-128, 192 και 256.
* **4.4 Έλεγχος Constant-Time Συμπεριφοράς [Προαιρετικό / TODO]:** * **4.4.1 Στατιστική Ανάλυση με dudect:** Μεθοδολογία t-test για την ανίχνευση χρονικών διαρροών.
    * **4.4.2 Dynamic Binary Analysis:** Χρήση Valgrind / ctgrind για τον έλεγχο uninitialized memory branches.

## Κεφάλαιο 5: Αποτελέσματα και Αξιολόγηση Απόδοσης

* **5.1 Ανάλυση Απομονωμένων Μικρο-αξιολογήσεων (Isolated Micro-benchmarks):** Παρουσίαση και σύγκριση αποτελεσμάτων (Reference vs Optimized) σε απομονωμένο επίπεδο για τα υποσυστήματα `vect_mul`, `keccak` και `decode`.
* **5.2 Αξιολόγηση σε Πραγματικό Υλικό [Προαιρετικό / TODO]:** Μετρήσεις πραγματικών κύκλων ρολογιού (Cycle counts) σε hardware development boards που υποστηρίζουν RVV 1.0 (π.χ. Banana Pi BPI-F3 / K230).
* **5.3 Αξιολόγηση Πλήρους Κρυπτοσυστήματος (End-to-End KEM Suite):** Παρουσίαση των τελικών αποτελεσμάτων για τις διεργασίες Keypair, Encaps και Decaps. Ανάδειξη της συνολικής μείωσης εντολών (π.χ. η πτώση του HQC-128 από τα 1.54M στα 1.21M εντολές).
* **5.4 Αρχιτεκτονική Αξιολόγηση Επεκτάσεων RISC-V (Ablation Study):** Ποσοτικός προσδιορισμός της επιτάχυνσης που προσφέρει η κάθε επέκταση ξεχωριστά. Ανάλυση της αποδοτικότητας των `Zbc`, `Zvbc`, `Zbb` και `Zvbb` στα αντίστοιχα κρυπτογραφικά hotspots.
* **5.5 Σύγκριση Μεταγλωττιστών (Compiler Profiling):** Clang/LLVM vs GCC στην παραγωγή βελτιστοποιημένου διανυσματικού κώδικα (ανάλυση LICM και register allocation).

## Κεφάλαιο 6: Συμπεράσματα και Μελλοντικές Επεκτάσεις

* **6.1 Σύνοψη και Αποτίμηση:** Ανακεφαλαίωση των συνολικών speedups. Επιβεβαίωση της επιτυχίας της στρατηγικής "pure software acceleration" για την επίτευξη υψηλών επιδόσεων.
* **6.2 Περιορισμοί της Υλοποίησης:** Αναφορά στις τρέχουσες συνθήκες της αγοράς υλικού (διαθεσιμότητα native Zvbc) και στους εγγενείς περιορισμούς των instruction-accurate εξομοιωτών (Spike) έναντι cycle-accurate μοντέλων.
* **6.3 Προτάσεις για Μελλοντική Έρευνα:** Ενσωμάτωση του constant-time ελέγχου, μεταφορά του κώδικα σε RTL soft-cores (π.χ. CVA6) σε FPGA για cycle-accurate μετρήσεις και διερεύνηση της υβριδικής scalar/vector σχεδίασης (Track C3) σε dual-issue pipelines.