## Κεφάλαιο 1: Εισαγωγή

* **1.1 Η Ανάγκη για Post-Quantum Κρυπτογραφία (PQC):** Η κβαντική απειλή και ο αλγόριθμος του Shor.
* **1.2 Ο Αλγόριθμος HQC:** Βασικές αρχές της code-based κρυπτογραφίας και η θέση του HQC στο NIST PQC standardization.
* **1.3 Το Οικοσύστημα του RISC-V:** Η σημασία των ανοιχτών αρχιτεκτονικών και των ratififed επεκτάσεων (agility). Τα Zbc, Zbb, Zbkb, V, Zvbc στην κρυπτογραφία.
* **1.4 Σκοπός και Συνεισφορά της Διπλωματικής:** Τι ακριβώς επιτεύχθηκε (purely software acceleration αντί hardware coprocessor) και τα πλεονεκτήματα του.
* **1.5 Δομή της Εργασίας:** Σύντομη περιγραφή των επόμενων κεφαλαίων.

## Κεφάλαιο 2: Θεωρητικό Υπόβαθρο

* **2.1 Key Encapsulation Mechanism (KEM) του HQC:** Αλγόριθμοι Keygen, Encapsulation, Decapsulation.
* **2.2 Πολυωνυμικός Πολλαπλασιασμός (Polynomial Multiplication):** Το υπολογιστικό bottleneck του HQC και η πράξη στον δακτύλιο $\text{GF}(2)[x]$.
* **2.3 Κρυπτογραφικές Συναρτήσεις Hash:** Η οικογένεια SHA-3 / Keccak.
* **2.4 Κώδικες Διόρθωσης Σφαλμάτων:** Reed-Muller, Reed-Solomon και Hadamard.
* **2.5 RISC-V Επεκτάσεις Κρυπτογραφίας:** * Scalar Cryptography (Zbc, Zbkb) και η χρήση των bitmanip instructions.
    * Vector Extensions (RVV) και Vector Crypto (Zvbc).

---
* **2.6 Επιθέσεις Πλευρικού Καναλιού (Side-Channel Attacks):** Η σημασία του constant-time execution.
* Να βάλουμε και για σχετική δουλειά;
---


## Κεφάλαιο 3: Υλοποίηση και Βελτιστοποίηση Αλγορίθμων

3.1 Το Αρχικό Προφίλ Εκτέλεσης (Reference Profiling): Ανάλυση της βασικής υλοποίησης. Ανάδειξη του πολυωνυμικού πολλαπλασιασμού ως το κύριο hotspot (π.χ. >90% του χρόνου).

3.2 Βελτιστοποίηση Πολυωνυμικού Πολλαπλασιασμού: 
    Στάδιο 1: Scalar Βελτιστοποιήσεις μέσω Zbc (Carry-less multiplication) και αλγοριθμικά tricks (π.χ. Karatsuba leaf optimizations).
    Στάδιο 2: Vectorized Karatsuba (RVV) και χρήση του Zvbc.

3.3 Βελτιστοποίηση Keccak / SHA-3: Το νέο hotspot. Πώς αξιοποιήθηκε το Vector batching (και ενδεχομένως Zvbb αν χρησιμοποιήθηκε) για την επιτάχυνση της Keccak.

3.4 Βελτιστοποίηση Σταδίου Αποκωδικοποίησης (Decode): * Επιτάχυνση του Hadamard transform.
    Βελτιστοποιήσεις στους αλγορίθμους Reed-Muller και Reed-Solomon.

## Κεφάλαιο 4: Μεθοδολογία και Επαλήθευση (Methodology & Verification)

* **4.1 Περιβάλλον Ανάπτυξης και Εξομοίωσης:** QEMU, Spike simulator και τα σχετικά toolchains (GCC vs Clang).
* 4.2 Μεθοδολογία Benchmarking: Τι ακριβώς μετρήθηκε (Dynamic Instruction Count, χρήση custom tracing/profiling scripts).
* **4.2 Επαλήθευση Ορθότητας (Correctness Verification):** Χρήση των Known Answer Tests (KATs) για τα HQC-128, 192 και 256.

---
* **4.3 Επαλήθευση Constant-Time:** * Μεθοδολογία με το εργαλείο dudect.
    * Χρήση Valgrind (ctgrind) για έλεγχο μνήμης και διαρροών χρόνου.
---

## Κεφάλαιο 5: Αποτελέσματα και Αξιολόγηση Απόδοσης

5.1 Μείωση Εντολών (Instruction Counts) μέσω Εξομοίωσης (Spike): * Σύγκριση Reference vs Scalar Zbc vs Vector (RVV / Zvbc) για HQC-128, 192, 256.

5.2 Αξιολόγηση σε Πραγματικό Υλικό (εφόσον υπάρχει): * Μετρήσεις κύκλων (Cycle counts) σε hardware που υποστηρίζει RVV (π.χ. Banana Pi / K230).

5.3 Σύγκριση Μεταγλωττιστών (Compiler Comparison): Πώς επηρέασε την απόδοση των vector εντολών η χρήση GCC έναντι LLVM/Clang.

## Κεφάλαιο 6: Συμπεράσματα και Μελλοντικές Επεκτάσεις

6.1 Σύνοψη: Μια γενική εικόνα της βελτίωσης. Π.χ. "Μέσω της στοχευμένης βελτιστοποίησης πρώτα στον πολλαπλασιασμό, μετά στην Keccak και τέλος στο Decode, επιτύχαμε συνολική μείωση εντολών κατά X%".

6.2 Περιορισμοί της Υλοποίησης: Αναφορά στην έλλειψη πραγματικού hardware για το Zvbc extension και στο γεγονός ότι το strict constant-time verification εκκρεμεί.

6.3 Προτάσεις για Μελλοντική Έρευνα: * Πλήρης Side-Channel Analysis και dudect verification.
    Δοκιμή της πλήρους υλοποίησης (με Zvbc) σε μελλοντικά ASIC chips ή σε RTL soft-cores μέσω FPGA.
