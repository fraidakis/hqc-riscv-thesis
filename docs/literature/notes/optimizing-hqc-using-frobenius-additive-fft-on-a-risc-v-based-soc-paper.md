---
source_pdf: docs/literature/RISC-V/!Optimizing HQC using Frobenius Additive FFT on a RISC-V-based SoC - Paper.pdf
converted_at: 2026-05-27 21:32
conversion_time: 5.4s
tool: pymupdf4llm
---

Highlights the critical software scheduling gap caused by the lack of a native fused vclmuladd instruction in the RISC-V Zvbc extension. 

## Optimizing HQC using Frobenius Additive FFT on a RISC-V-based System-on-Chip 

Published 09/01/26 

Antonio Ras[1] , Antoine Loiseau[1] , Mikael Carmona[1] , Simon Ponti´e[1,5] , Gu´ena¨el Renault[3, 4] Benjamin Smith[3] , Emanuele Valea[2] 

> 1 _CEA-Leti_ , Grenoble, France 2 _CEA-List_ , Grenoble, France 3 _LIX, INRIA, CNRS, IP Paris_ , France 

> 4 _ANSSI_ , Paris, France 5 _Equipe Commune CEA Leti-Mines Saint-Etienne_ , Gardanne, name.surname@cea.fr, guenael.renault@ssi.gouv.fr, smith@lix.polytechnique.fr, 

_**Abstract**_ **—HQC is a quantum-resistant cryptographic key encapsulation mechanism, recently selected by NIST as a future standard. Polynomial multiplication is one of the most critical operations in HQC. Due to side-channel security concerns, the previously-used sparse-dense method was recently replaced by classical dense-dense multiplication implemented using Karatsuba’s algorithm. This change has made polynomial multiplication the primary performance bottleneck, accounting for approximately 95% of the total execution time. This paper** 

**presents an alternative polynomial multiplication technique for HQC: the Frobenius Additive Fast Fourier Transform (FAFFT), which provides significant algorithmic-level performance improvements. We also present ANDROMEDA, the first state-ofthe-art hardware implementation of FAFFT, and evaluate its performance impact by integrating our solution in a resourceconstrained RISC-V-based System-on-Chip scenario. Experimental results show that our solution improves HQC performance by approximately 9.64** _×_ **and 19.22** _×_ **across its security levels, making HQC more practical for real-world deployment.** _**Index Terms**_ **—Post-Quantum Cryptography, HQC, Polynomial Multiplication, FAFFT, RISC-V, FPGA, Hardware Acceleration** 

## I. INTRODUCTION 

Modern public-key cryptosystems like RSA and ECC depend on the difficulty of problems such as integer factorization and discrete logarithms, which are secure against classical computers. However, Shor’s quantum algorithm [1] can efficiently solve these problems, destroying their security. Post-Quantum Cryptography (PQC) [2] aims to develop new cryptosystems based on problems that remain hard even for quantum computers, including problems based on Euclidean lattices and error-correcting codes. In 2016, the National Institute of Standards and Technology (NIST) launched a process to standardize quantum-resistant algorithms for Key Encapsulation Mechanisms (KEM) and Digital Signatures (DS) [3]. Their KEM selection process has now concluded, with MLKEM (lattice-based) already standardized and HQC [4] (codebased) selected as a future standard, to enhance security and promote cryptographic diversity in the transition to quantumsafe cryptography [5]. 

HQC is a code-based IND-CCA2 secure KEM whose security relies on the syndrome decoding problem for QuasiCyclic codes. Binary polynomial multiplication is currently the major bottleneck for this scheme. Earlier drafts of HQC proposed _sparse-dense_ multiplication, exploiting the fixed low Hamming weight of one of the polynomials to skip inter- 

mediate operations and improve performance. However, this method was not constant-time, risking side-channel leaks. To mitigate this, the latest HQC specification switched to _densedense_ multiplication using the 2-way Karatsuba algorithm, which now accounts for about 95% of execution time. 

In this paper, we present an alternative method for performing binary polynomial multiplication in HQC. Our approach, called Frobenius Additive Fast Fourier Transform (FAFFT), is a state-of-the-art technique for long binary polynomial multiplication [6], [7]. The FAFFT has already been tested in BIKE [8], a former competitor in the NIST standardization process that was passed over in favour of HQC. We show that applying FAFFT to HQC significantly enhances polynomial multiplication performance on a resource-constrained RISCV-based platform, achieving speedups between 3.52 _×_ and 8.08 _×_ across the three HQC security levels. To further boost performance, we introduce ANDROMEDA, a hardware accelerator designed to mitigate the FAFFT’s main computational bottlenecks. It leverages a configurable hardware module, the Carry-Free Butterfly, which handles the execution of key processing steps within the multiplication workflow. We integrate our hardware solution into the RISC-V-based platform, using a loosely-coupled approach. ANDROMEDA achieves further speedups of between 2.74 _×_ and 2.38 _×_ over the FAFFTbased software implementation, resulting in total performance gains between 9.64 _×_ and 19.22 _×_ over the HQC reference implementation. 

To summarize our contributions: 

- We integrate the Frobenius Additive FFT (FAFFT) in HQC, showing a large improvement over Karatsubabased approaches in resource-constrained scenarios; 

- We propose ANDROMEDA, a hardware accelerator for the FAFFT; 

- We provide a system-level integration of ANDROMEDA in a resource-constrained RISC-V-based System-on-Chip, demonstrating its practical viability and significant performance gains in realistic post-quantum cryptography use cases. 

**Paper organization** : Section II provides background on the HQC cryptosystem and introduces the proposed alternative FAFFT-based multiplication method—with the existing FAFFT state-of-the-art solution. Sections III, IV, and V detail 

the three main contributions of this work, along with their corresponding results. Section VI presents our conclusions. 

## II. BACKGROUND 

We begin with an overview of HQC in Subsection II-A, then describe a generic FFT-based approach for binary multiplication in Subsection II-B. Subsection II-C presents he Frobenius Additive FFT, an optimized FFT-based multiplication method for binary polynomials. 

## _A. HQC_ 

Hamming Quasi-Cyclic (HQC) [4] is an IND-CCA2-secure KEM, recently selected by NIST as future standard. HQC uses two linear codes: a quasi-cyclic random code that underpins the security of the scheme, and a fixed public code with efficient decoding and strong error-correction properties. HQC uses the public code to encode a short random string, from which the KEM session key is derived. The secret key is then used to introduce errors into the encoded string—well beyond the correction threshold of the public code—ensuring that only the entity that generated the cryptographic key pair is able to remove the errors and recover the original message. 

TABLE I: HQC Parameter Sets 

|HQC|_n_1<br>_n_2<br>_n_<br>_w_<br>_w_**r** =_w_**e**<br>DFR|
|---|---|
|||
|HQC-128<br>HQC-192<br>HQC-256|46<br>384<br>17,669<br>66<br>75<br>2_−_128<br>56<br>640<br>35,851<br>100<br>114<br>2_−_192<br>90<br>640<br>57,637<br>131<br>149<br>2_−_256|



Table I shows the HQC parameter sets for the three main NIST security levels. The parameters _n_ and _w_ define a ring _R_ := F2[ _X_ ] _/_ ( _X[n]_ + 1) with a special subset _Rw_ = _{v ∈R | ω_ ( _v_ ) = _w}_ where _ω_ ( _·_ ) is the Hamming weight. HQC is constructed in two stages: the first defines HQC.PKE, an IND-CPA-secure public-key encryption scheme whose security relies on the hardness of the Quasi-Cyclic Syndrome Decoding problem (QCSD) [4]. The second applies the Hofheinz–H _o_ ¨velmanns–Kiltz (HHK) transform [9] to HQC.PKE to derive an IND-CCA2-secure KEM. HQC.PKE consists of three algorithms: 

- HQC.PKE.KeyGen: We uniformly sample a vector **h** from _R_ , and secret vectors **x** _,_ **y** from _Rw_ . The public key is ( **h** _, s_ := **x** + **h** _·_ **y** ), and the private key is ( **x** _,_ **y** ). 

- HQC.PKE.Encrypt: To encrypt a 32-byte message _m_ , we uniformly sample **r** 1 _,_ **r** 2 _∈Rwr_ , **e** _∈Rwe_ , then compute **u** = **r** 1+ **h** _·_ **r** 2 _∈R_ and **v** = _m_ **G** + **s** _·_ **r** 2+ **h** _∈R_ , where _m_ **G** = _C._ Encode( _m_ ) is the encoding of _m_ in the public linear code. The ciphertext is ( **u** _,_ **v** ). 

- HQC.PKE.Decrypt: To decrypt a ciphertext ( **u** _,_ **v** ) using the private key **y** , we compute _m_ := _C._ Decode( **v** _−_ **u** _·_ **y** ) _,_ where _C._ Decode( _·_ ) is the decoding process for the public linear code. 

Polynomial multiplication is a critical operation in HQC, appearing in each of the PKE functions. Earlier versions of HQC used special _sparse-dense_ multiplications, taking 

advantage of the fixed and low Hamming weight of one of the operands. This sparse binary polynomial can be represented by the positions of its non-zero coefficients. This representation allows skipping many intermediate operations during the multiplication, improving computational efficiency and overall performance [4], [10]. However, the efficient execution of the sparse-dense approach results in implementations that are not constant-time, which can leak sensitive information through side-channels [4, Section 3.2]. 

For this reason, sparse-dense multiplication was replaced with a dense-dense multiplication algorithm—2-way Karatsuba—in the latest update of the HQC. Thus, multiplication has become the main bottleneck of HQC, consuming approximately 95% of the total execution time. The Karatsuba approach recursively breaks down the multiplication of large polynomials into smaller and more manageable sub-problems. Once the sub-problems are solved, the intermediate results are combined to obtain the final product. However, for HQC the binary input polynomials range in size from 17,669 to 57,637 bits, so Karatsuba’s recursion is deep, with many intermediate polynomials. These require a significant amount of temporary memory and they contribute to increased computation time, further hindering performance. 

The need to improve polynomial multiplication in HQC motivates our use of an alternative algorithm: the FAFFT. 

## _B. General FFT-based Binary Multiplication_ 

We briefly recall the Fast Fourier Transform (FFT) based polynomial multiplication technique using _Kronecker segmentation_ (see [11] for further detail and background). Suppose we want to compute the product _C_ ( _x_ ) of two polynomials _A_ ( _x_ ) = _a_ 0+ _· · ·_ + _adx[d]_ and _B_ ( _x_ ) = _b_ 0+ _· · ·_ + _bdx[d] ∈_ F2[ _x_ ] _≤d_ , each represented as a bit sequence of length _d_ + 1. Filling high-degree coefficients with 0 as required, we can assume _n_ = 2( _d_ + 1) is a power of 2 (note that the coefficients of _C_ fit into a bit sequence of length _n_ ). Fix a parameter 0 _< w ≤ d_ and set _ℓ_ := _⌈d/w⌉_ . Choose an irreducible polynomial _g_ in F2[ _x_ ] of degree _m_ := 2 _w_ , and let F2 _m_ = F2[ _z_ ] _/g_ ( _z_ ). Now, to compute _C_ we proceed as follows: 

- 1) Partition the coefficients of _A_ and _B_ into _ℓ_ blocks of _w_ bits: 

**==> picture [234 x 44] intentionally omitted <==**

- 2) Now, applying the map _x �→ z ∈_ F2 _m_ to each coefficient block, we set 

**==> picture [219 x 52] intentionally omitted <==**

- 3) Using the FFT, evaluate _A[′]_ ( _y_ ) and _B[′]_ ( _y_ ) at 2 _ℓ_ points _α_ 0 _, . . . , α_ 2 _ℓ−_ 1 in F2 _[m]_ [ _y_ ] _≤_ 2 _ℓ_ to get 

**==> picture [183 x 28] intentionally omitted <==**

- 4) Perform point-wise multiplication (PWM) on the 2 _ℓ_ evaluations: 

**==> picture [191 x 13] intentionally omitted <==**

- 5) Use the IFFT to interpolate the _C[′]_ ( _y_ ) such that _C[′]_ ( _αi_ ) = _c_ ˆ _i_ for 0 _≤ i <_ 2 _ℓ_ . 

- 6) Map _C[′]_ ( _y_ ) to _C_ ( _x_ ) _∈_ F2[ _x_ ] _≤n_ via _z �→ x_ and _y �→ x[w]_ , collecting terms and coefficients using the _Interleave and Combine_ operation. 

## _C. Frobenius Additive FFT (FAFFT)_ 

We now present the necessary elements for binary polynomial multiplication using the Lin-Cheng-Han Additive FFT (AFFT) [6] and its optimized version based on the Frobenius map [12]. 

**Cantor Basis** : The representation of elements of F2 _m_ as vectors in F _[m]_ 2 always depends on a choice of F2-basis ( _β_ 0 _, . . . , βm−_ 1) of F2 _m_ . Cantor [13] defined useful bases when _m_ is a power of 2, i.e. _m_ = 2 _[ℓ][m]_ for some _ℓm >_ 0. Gao and Mateer [14] give an explicit recursive construction: 

**==> picture [224 x 12] intentionally omitted <==**

Given a Cantor basis ( _βi_ ) _[m] i_ =0 _[−]_[1] of F2 _[m]_ , we define a sequence of subspaces _Vk_ and a corresponding vanishing polynomial _sk_ ( _x_ ), for 0 _≤ k ≤ m_ , as follows: 

**==> picture [238 x 24] intentionally omitted <==**

Each _sk_ ( _x_ ) vanishing polynomial satisfies: 

- _Linearity_ : _sk_ ( _x_ + _y_ ) = _sk_ ( _x_ ) + _sk_ ( _y_ ); 

- _Two-term form_ : _sk_ ( _x_ ) = _x_[2] _[k]_ + _x_ when _Vk_ is a field; 

- _Recursivity_ : _sk_ + _i_ ( _x_ ) = _sk_ ( _si_ ( _x_ )) = _si_ ( _sk_ ( _x_ )). 

**Fast computation of si** ( _α_ ): We represent elements _α ∈_ F2 _m_ in the Cantor basis using the bijection defined by 

**==> picture [243 x 21] intentionally omitted <==**

This suggests a particularly convenient of _α ∈_ F2 _m_ as an unsigned _m_ -bit integer _k_ such that _ϕβ_ ( _k_ ) = _α_ . Using this encoding and the respective Cantor properties, we get 

**==> picture [195 x 11] intentionally omitted <==**

where _k ≫ i_ denotes the integer _k_ shifted right by _i_ bits. That is: evaluating _si_ at _α_ corresponds to computing the image under _ϕβ_ of the bit-shifted integer _k ≫ i_ . This makes evaluating _si_ extremely fast and convenient. 

**Basis conversion (BasisCvt)** : Evaluation and interpolation of a polynomial _f ∈_ F2[ _x_ ] _<n_ at _n_ = 2 _[l][n]_ points in F2 _m_ expressed in a Cantor basis is particularly efficient if _f_ is represented in a particular basis of F2[ _x_ ] called _novelpoly_ . Each of its basis 

elements _Xk_ ( _x_ ) is constructed as the product of the vanishing polynomials _si_ ( _x_ ) where the _i_ -th bit of _k_ is set: 

**==> picture [247 x 23] intentionally omitted <==**

We convert polynomials _f_ ( _x_ ) from the monomial basis to the novelpoly basis by splitting _f_ ( _x_ ) as _f_ 0( _x_ ) + _si_ ( _x_ ) _f_ 1( _x_ ), where _si_ ( _x_ ) is a vanishing polynomial with 2 _[i] <_ deg( _f_ ), and repeating recursively. Each division by _si_ ( _x_ ) is done with XOR operations, whose cost depends on the number of terms in _si_ . To reduce this cost, we select only the vanishing polynomials _s_ 2 _i_ ( _x_ ) = _x_[2][2] _[i]_ + _x_ , which have exactly two terms. This is achieved via variable substitution, expressing _f_ as a power series in _s_ 2 _i_ ( _x_ ). The result ensures both correctness and minimal XOR cost (see [6, §2.4] for more details). 

**FFT-based operations** : Given _f_ ( _x_ ) _∈_ F2 _m_ [ _x_ ] _<n_ converted to _g_ ( _X_ ) in the novelpoly basis, we can efficiently evaluate _f_ at the set Σ :=[ˆ] _α_ + _Vℓn_ = _{α_ + _u | u ∈ Vℓn}_ for _α ∈_ F2 _m_ (where _n_ = 2 _[ℓ][n]_ ) using the FFTLCH algorithm [6, §2.3.2, Alg. 1]. The general idea of evaluating at all points of _Vℓn_ is to divide the subspace in two sets, _Vℓn−_ 1 and _Vℓn−_ 1+ _βℓn−_ 1 := _{x_ + _βℓn−_ 1 : _x ∈ Vℓn−_ 1 _}_ . 

The algorithm follows a classic divide-and-conquer approach: we rewrite _f_ ( _x_ ) = _g_ ( _X_ ) = _p_ 0( _X_ ) + _si_ ( _x_ ) _· p_ 1( _X_ ), where _si_ ( _x_ ) = _X_ 2 _i_ ( _x_ ) with 2 _[i]_ = 2 _[ℓ][n][−]_[1] , and _p_ 0 _, p_ 1 are halfsized parts of _g_ . We then apply the butterfly operation, defined as follows: 

**==> picture [196 x 26] intentionally omitted <==**

The algorithm proceeds recursively on the two half-sized polynomials by calling the FFTLCH function again, passing _α_ as the input for _h_ 0( _X_ ) and _α_ + _βi_ for _h_ 1( _X_ ). The recursion continues until reaching the base case of size 1, where evaluation simply returns the constant coefficient of the polynomial. 

The inverse IFFTLCH algorithm recursively reconstructs the original polynomial from its evaluations using an inverse butterfly process: 

**==> picture [196 x 26] intentionally omitted <==**

The full procedure consists of _ℓn_ layers, each comprising _n/_ 2 butterfly operations, enabling fast evaluation and interpolation over structured subspaces in F2 _m_ . 

**New evaluation points** : We can reduce the number of evaluation points using the Frobenius map _ϕ_ 2 over F2, defined by _ϕ_ 2( _a_ ) = _a_[2] for _a ∈_ F2 _m_ . This map satisfies the key property 

**==> picture [218 x 11] intentionally omitted <==**

The starting _n_ evaluation points can be reduced to _np_ = _n/m_ using the Frobenius map and, as suggested in [12], by taking 

**==> picture [173 x 13] intentionally omitted <==**

where _ℓnp_ = log( _np_ ) _< m/_ 2 and _ℓnp_ + _m/_ 2 _< m_ . Note that the cardinality of the evaluation set is #Σ = _np_ = _n/m_ . 

Evaluating _f_ at Σ corresponds to performing a traditional size- _n_ evaluation of _f_ on the original point-set Σ =[ˆ] _α_ + _V_ ( _ℓnp_ + _ℓm_ ), avoiding all the redundant operations. To enable this, we define an invertible linear map _encode_ which maps F2[ _x_ ] _<n �→_ F _[n]_ 2 _[m][p]_[[] _[x]_[]][as][follows:] 

**==> picture [233 x 18] intentionally omitted <==**

where, representing indexes _j_ :=[�] _[ℓ] i_ =0 _[m][j][i]_[2] _[i]_[,][the][coefficients] are computed as 

**==> picture [242 x 40] intentionally omitted <==**

The overall process starts by converting the size- _n_ polynomial _f_ ( _x_ ) into the novelpoly basis. Then, we _encore_ the first _ℓm_ layers of the size- _n_ FFTLCH evaluation. After that, the remaining _ℓnp_ layers are computed using a standard size- _np_ FFTLCH evaluation over the reduced point set Σ. The last _ℓm_ layers of the IFFTLCH are handled by the _decode_ map corresponding to the inverse of the linear map. 

The overall Frobenius Additive FFT alternative method for binary polynomial multiplication is reported in Algorithm 1. 

## **Algorithm 1:** FAFFT-based Polynomial Multiplication 

**Input:** _n_ : Maximum length of the output polynomial. **1 Procedure** _FAFFT(f_ ( _x_ ) _∈_ F2[ _x_ ] _<d,_ Σ _)_ **2** ( _f_ 0 _, . . . , fn−_ 1) _∈_ F _[n]_ 2 _[←]_[BasisCvt][(] _[f]_[(] _[x]_[))][;] **3** ( _f_ 0 _[′][, . . . , f] n[′] p−_ 1[)] _[ ∈]_[F] _[n]_ 2 _[m][p][←]_[Encode][((] _[f]_[0] _[, f]_[1] _[, . . . , f][n][−]_[1][)] _[,]_[ Σ)][;] **4** ( _f_[ˆ] 0 _, . . . , f_[ˆ] _np−_ 1) _∈_ F _[n]_ 2 _[m][p][←]_[FFT][LCH][((] _[f]_ 0 _[′][, . . . , f] n[′] p−_ 1[)] _[,]_[ Σ)][;] **5 return** ( _f_[ˆ] 0 _, . . . , f_[ˆ] _np−_ 1); **6 Procedure** _FAFFT[−]_[1] _(f_[ˆ] ( _x_ ) _∈_ F _[n]_ 2 _[m][p][,]_[ Σ] _[)]_ **7** ( _f_ 0 _[′][, . . . , f] n[′] p−_ 1[)] _[ ∈]_[F] _[n]_ 2 _[m][p][←]_[IFFT][LCH][(( ˆ] _[f]_[0] _[, . . . ,][f]_[ˆ] _[n] p[−]_[1][)] _[,]_[ Σ)][;] **8** ( _f_ 0 _, . . . , fn−_ 1) _∈_ F _[n]_ 2 _[←]_[Decode][((] _[f]_ 0 _[′][, f]_ 1 _[′][, . . . , f] n[′] −_ 1[)] _[,]_[ Σ)][;] **9** _f_ ( _x_ ) _∈_ F2[ _x_ ] _<n ←_ iBasisCvt( _f_ 0 _, f_ 1 _, . . . , fn−_ 1); **10 return** _f_ ( _x_ ); **11 Procedure** _BitPolyMult(a_ ( _x_ ) _, b_ ( _x_ ) _∈_ F2[ _x_ ] _<d)_ // Input polynomials evaluation; **12** (ˆ _a_ 0 _, . . . ,_ ˆ _anp−_ 1) _∈_ F _[n]_ 2 _[m][p][←]_[FAFFT][(] _[a]_[(] _[x]_[)] _[,]_[ Σ)][;] **13** ([ˆ] _b_ 0 _, . . . ,_[ˆ] _bnp−_ 1) _∈_ F _[n]_ 2 _[m][p][←]_[FAFFT][(] _[b]_[(] _[x]_[)] _[,]_[ Σ)][;] // Point-Wise Multiplication (PWM) ; **14** (ˆ _c_ 0 _, . . . ,_ ˆ _cnp−_ 1) _∈_ F _[n]_ 2 _[m][p][←]_[(ˆ] _[a]_[0] _[ ·]_[ ˆ] _[b]_[0] _[, . . . ,]_[ ˆ] _[a][n] p[−]_[1] _[ ·]_[ ˆ] _[b][n] p[−]_[1][)][;] // Get multiplication result; **15** _c_ ( _x_ ) _∈_ F2[ _x_ ] _<n ←_ FAFFT _[−]_[1] ((ˆ _c_ 0 _, . . . ,_ ˆ _cnp−_ 1) _,_ Σ); **16 return** _c_ ( _x_ ); 

## _D. Existing FAFFT Software Implementations_ 

Software FAFFT implementations generally fall into two categories based on the technique used to multiply elements of F2 _m_ . For example, [7] proposes a complete software implementation of FAFFT for multiplying long binary polynomials, tailored for Intel Haswell, where field multiplication is performed using the Cantor representation. This implementation optimizes memory access, with SIMD instructions to enhance performance. However, this approach is not directly used for 

cryptographic applications, specifically for PQC schemes. On the other hand, [8] integrates an efficient FAFFT software implementation into BIKE (a post-quantum KEM passed over by NIST in favor of HQC). This solution, tailored for Intel Haswell and ARM Cortex-M4 processors, multiplies elements using the tower field construction, which gives better performance than the Cantor approach. The proposed solution also uses bit-sliced data representations, improves the encode/decode phases, and provides assembly-based optimizations for Cortex-M4. 

As far as we know, this paper is the first to propose implementing HQC based on the FAFFT technique. 

## III. HQC USING FROBENIUS ADDITIVE FFT 

This section presents the first software integration of the FAFFT-based multiplication method in HQC. In Subsection III-A, we introduce our FAFFT-based software implementation and further presenting the parameter settings used for integration across all HQC security levels. We finally demonstrate, in Subsection III-B, how the proposed alternative multiplication method shows performance improvements with respect to the reference HQC implementation. 

## _A. FAFFT in Software_ 

We implemented the FAFFT in C, starting from existing state-of-the-art solutions [7], [8]. Our software is intended to provide a clean and portable baseline, so we did not use any SIMD instructions or assembly-level optimizations. 

We choose the basic working field 

**==> picture [171 x 12] intentionally omitted <==**

A general scalar multiplication in this field involves a carryless multiplication between the two field elements, followed by a modular reduction using the specified irreducible polynomial, typically performed using the shift-and-add method. 

The tower field construction, implemented as a sequence of nested and smaller field extensions, offers advantages in software due to its recursive and modular structure. However, its portability to hardware is more challenging. The layered design requires additional control logic, intermediate storage, and sequential operations, which can increase complexity and limit parallelism. For this reason, in our FAFFT software, we opted for a Cantor representation-based multiplication, which relies solely on XOR and shift operations, enabling simpler, faster, and more parallel hardware solutions. 

TABLE II: FAFFT parameters for all HQC levels. _nHQC_ is the _n_ -parameter from the HQC specification, and _n_ denotes the FFT-based operation size in bits. 

|NIST Level|_nHQC_<br>_n_<br>_m_<br>_np_<br>_lnp_<br>Σ|
|---|---|
|||
|HQC-128|17,669<br>65536<br>32<br>2048<br>11<br>_β_27+_V_11|
|||
|HQC-192<br>HQC-256|35,851<br>131072<br>32<br>4096<br>12<br>_β_28+_V_12<br>57,637|



We integrate our FAFFT implementation across all NIST security levels of HQC, using Algorithm 1 with the parameter 

sets in Table II and the evaluation point sets of to Equation (9). We zero-padded polynomials to the next power-of-2 size (required by the FAFFT method). This allows us to use the same FAFFT internal parameters for HQC-192 and HQC-256. **Compute evaluation points** : We can significantly improve FAFFT performance by precomputing all _si_ ( _α_ ) constants, along with the evaluation points in Σ for the butterfly structures in Equations (6) and (7). However, for HQC-192 and HQC-256, the number of required constants grows to 4096 32-bit values, imposing considerable storage overhead for the hardware-based approach. To address this issue, we exploit the so-called _four Russians_ method [15]. The FAFFT uses fast evaluation of expressions _sk_ ( _α_ ) = _βm/_ 2+ _i_ + _sk_ ( _γ_ ) where 1 _≤ i ≤ q_ and _γ_ ranges over all the points in _Vq_ . Starting the original _V_ 12 = _⟨β_ 0 _, . . . , β_ 11 _⟩_ , where _q_ = 12, we prepare four constant tables: _T_ 0 contains the Cantor basis values _βm/_ 2+1 _, . . . , βm/_ 2+ _q_ , while _T_ 1, _T_ 2, and _T_ 3 contain all of the elements in 

**==> picture [209 x 11] intentionally omitted <==**

respectively. Splitting the binary vector _γ_ of _γ ∈ V_ 12 into 4-bit chunks, i.e. _γ_ = _γ_ 1 + _γ_ 2 + _γ_ 3 with each _γi ∈ ⟨β_ 4 _i−_ 4 _, . . . , β_ 4 _i−_ 1 _⟩_ , we have 

**==> picture [125 x 11] intentionally omitted <==**

using the _γi_ as table indexes. 

Overall, this method requires storing 60 32-bit constants and it is fully compatible for HQC-128. 

## _B. Performance results_ 

We integrated our FAFFT alternative in the official HQC software implementation (taken from the additional implementation folder of the Round-4 submission package[1] ). We benchmarked this across all HQC security levels on an embedded systems platform using the CV32E40P RISC-Vbased processor [16] with an Instruction RAM of 256KB and a Data RAM of the same size. The System-on-Chip (SoC) also includes a JTAG module for accessing the memory and the platform registers, and a UART module for serial communication. All components communicate via an OBI bus [17], an open-source interconnect standard for RISC-V systems. The SoC platform has been deployed on the Xilinx Kintex-7 FPGA from Diligent Genesys 2 board xc7k325t-2ffg900c [18]. Synthesis and Place&Route are performed using Xilinx Vivado 2019.1 [19]. The code was compiled using the riscv32-unknown-elf-gcc compiler with the -O3 optimization flag. 

Since this is the first integration of FAFFT in HQC, we compare our approach solely with the HQC software reference implementation, which relies on a conventional _dense-dense_ multiplication strategy. Table III reports cycle counts for key generation, encapsulation, and decapsulation. Our approach improves KEM performance over the baseline implementation by 3.52 _×_ , 4,98 _×_ , and 8,08 _×_ across the three HQC security 

TABLE III: Software execution time (in cycles) for the original HQC and our FAFFT-based variant. Speedups relative to our variant are shown in brackets. 

||Work|Level<br>HQC-128|KeyGen<br>43,592,464<br>[**3.57**_×_]|Encaps<br>87,574,395<br>[**3.52**_×_]|Decaps<br>132,101,254<br>[**3.47**_×_]|
|---|---|---|---|---|---|
||SW HQC [4]<br>**This work**<br>(SW)|HQC-192<br>HQC-256<br>HQC-128<br>HQC-192|132,528,662<br>[**5.03**_×_]<br>242,546,831<br>[**8.21**_×_]<br>12,208,482<br>26,347,517|265,882,733<br>[**4.97**_×_]<br>486,241,524<br>[**8.07**_×_]<br>24,882,981<br>53,528,998|399,562,345<br>[**4.93**_×_]<br>731,405,854<br>[**7.95**_×_]<br>38,099,269<br>81,043,401|
|||HQC-256|29,545,963|60,238,075|91,987,793|



levels, demonstrating the algorithmic efficiency gains achieved by the FAFFT. 

## IV. ACCELERATING FAFFT IN HARDWARE 

In this section, we present the first state-of-the-art hardware implementation of the FAFFT-based multiplication method. Subsection IV-A introduces the Carry-Free Butterfly unit, a configurable hardware block designed to accelerate the core FFT-based operations of the alternative FAFFT approach. In subsection IV-B, we propose ANDROMEDA —the first dedicated hardware design aimed at improving the primary FAFFT bottlenecks. In subsections IV-C, we demonstrate the performance advantages of this hardware solution by analyzing a single polynomial multiplication scenario. Finally, in Subsection IV-D we report the FPGA hardware resources of ANDROMEDA. 

## _A. Carry-Free Butterfly (CFB)_ 

We profiled Algorithm 1, including PWM step for consistency. We observed that FFT-based operations take up about 79% of the total polynomial multiplication time, and therefore chose to accelerate these steps using a hardware approach. 

The FFT-based operations rely on the butterfly structures outlined in Equations 6 and 7. To accelerate them, we introduce the Carry-Free Butterfly (CFB) unit, shown in Figure 1. This is a configurable, pipelined hardware architecture with a 2-clock cycles latency, designed to perform the required FAFFT arithmetic operations in F232. Figure 2 presents the three specific butterfly configurations that are employed in the computation of the target operations. 

The Carry-Free Butterfly unit takes as input two polynomial coefficients and one constant value, and produces two output values. It is composed of a modular carryless multiplier and three carryless adders, implemented using XOR operations. For the multiplication, the hardware implementation includes modular reduction of the multiplier through the _shift-andadd_ method, based on the chosen irreducible polynomial. To minimize hardware resources and improve performance, we employ the classical 2-way Karatsuba algorithm for scalar carryless multiplication. This allows us to multiply two 32bit field elements using three 16-bit multipliers, which we implemented using a Schoolbook-based architecture. 

1https://pqc-hqc.org/implementation.html 

**==> picture [207 x 119] intentionally omitted <==**

**----- Start of picture text -----**<br>
0<br>1<br>x<br>0 0<br>1 1<br>**----- End of picture text -----**<br>


Fig. 1: The configurable Carry-Free Butterfly hardware unit 

**==> picture [231 x 171] intentionally omitted <==**

**----- Start of picture text -----**<br>
x x<br>(a) FFT LCH butterfly (b) IFFT LCH butterfly<br>x<br>(c) PWM butterfly<br>**----- End of picture text -----**<br>


Fig. 2: Carry-Free Butterfly configuration features for FAFFT multiplication. Note that PWM requires only a carryless modular multiplication. 

## _B. ANDROMEDA Design Rationale_ 

To efficiently accelerate FAFFT-based polynomial multiplication in HQC, we leverage the Carry-Free Butterfly unit to perform the FFTLCH, PWM, and IFFTLCH operations within a dedicated hardware accelerator, ANDROMEDA, shown in Figure 3. Our proposal is currently the first proposal for hardware implementation of the FAFFT-based multiplication. 

ANDROMEDA performs operations _in-place_ , i.e., the memory addresses used to read coefficients are also used to write the corresponding results from the Carry-Free Butterfly units. **Components** : ANDROMEDA relies on four internal blocks: 

- Butterfly element contains two Carry-Free Butterfly units, CFB0 and CFB1, which work independently. They receive a unique control signal to properly configure them as in Figure 2. 

- Memories store both input polynomials and constant values. The polynomial memories are split into two sets, BRAM0 _−_ 3 and BRAM4 _−_ 7, which accomodate the two polynomials for the targeted operations. 

- Address generation calculates the memory indexes of the coefficients consumed by CFB0 and CFB1. Its Coeffs-indexes submodule stores index pairs. The bank-decoder then determines the corresponding 

**==> picture [247 x 172] intentionally omitted <==**

**----- Start of picture text -----**<br>
MEMORY<br>CONTROL UNIT<br>BRAM  BRAM  BUTTERFLY<br>0 1 ELEMENTS<br>ADRESSES<br>GENERATION BRAM  BRAM  CFB 0<br>COEFFS INDEXES 2 3<br>INDEX INDEX CFB 1<br>CFB 0 CFB 1 BRAM  BRAM<br>4 5<br>BRAM  BRAM<br>BANK DECODER 6 7<br>CONSTANTS<br>address<br>ROM  ROM  ROM  ROM<br>data 0 1 2 3<br>Andromeda<br>**----- End of picture text -----**<br>


Fig. 3: ANDROMEDA hardware and its internal components 

   - memory addresses and selects the appropriate coefficient instances for processing. 

- Control unit orchestrate everything, sending control signals to the previous blocks and managing the different polynomial sizes for different HQC security levels. 

**Memories and memory access** : The use of two Carry-Free Butterfly units requires reading four polynomial coefficients simultaneously for processing. Since these coefficients are distributed across multiple BRAMs, an inefficient selection of coefficient pairs may lead to performance overhead due to potential memory address conflicts. To prevent this, we adopt the conflict-free memory addressing strategy described in [20], organizing the input polynomials according to this method. While this approach necessitates a more complex bank decoder, it significantly enhances performance by ensuring conflict-free memory accesses and eliminating data dependencies. The two input polynomials are stored in 8 DualPort BRAMs, split into two groups of four, one for each polynomial. Each BRAM has 1024 32-bit data words to handle the maximum polynomial size. 

**Constants generation** : We employ the Four Russian method, as explained in Section II-C, by distributing the required constant values across four separate ROMs, each containing 16 entries of 32-bit values. This approach enables the constants to be computed _on-the-fly_ by reading the four necessary values in parallel and summing them. 

## _C. Performance results_ 

We implemented Carry-Free Butterfly and ANDROMEDA using SystemVerilog Hardware Description Language (HDL). We simulated, synthetized and implemented them using Vivado 2019.1 design suite on a Xilinx FPGA Artix-7, embedded on a Zybo Z7-20 Diligent board xc7z020clg400-1 [21]. The critical path delay of ANDROMEDA, imposed by the Carry-Free Butterfly unit, is 8ns; this corresponds to the maximum frequency of 125MHz. 

Table IV presents the execution time (in clock cycles) of a single FAFFT polynomial multiplication, showing the 

speedup factor between software-only and ANDROMEDA. The hardware-software co-design results exclude the data exchange overhead involved in offloading polynomials from the main memory to the hardware accelerator and vice versa. This overhead will be taken into account in Section V. The results across all HQC security levels, shown in Table IV, demonstrate that ANDROMEDA significantly accelerates the core FAFFT operations: FFTLCH, PWM, and IFFTLCH, achieving speedups of 442 _×_ , 385 _×_ , and 441 _×_ , respectively. Although the remaining FAFFT steps—grouped under the _Others_ category—remain unchanged and dominates the hardwareside total, the overall polynomial multiplication execution still benefits from a 4.30 _×_ improvement, confirming the impact of offloading the most expensive computations to hardware. 

TABLE IV: Cycle counts for FAFFT polynomial multiplication targeting the HQC parameter sets from Table II. 

|**FAFFT Size**<br>**[bits]**|**Function**|**Software**<br>**ANDROMEDA**<br>**Gain**|
|---|---|---|
||||
|65536|2_×_FFTLCH<br>1_×_PWM<br>1_×_IFFTLCH<br>_Others_<br>**Total**|5,122,527<br>11,520<br>_445×_<br>430,092<br>1,102<br>_390×_<br>2,559,224<br>5,760<br>_444×_<br>2,518,901<br>2,518,901<br>-<br>**10,630,744**<br>**2,537,283**<br>**_4.19_**_×_|
||||
|131072|2_×_ FFTLCH<br>1_×_PWM<br>1_×_IFFTLCH<br>_Others_<br>**Total**|11,133,449<br>25,316<br>_440×_<br>860,172<br>2,258<br>_381×_<br>5,562,637<br>12,658<br>_439×_<br>5,096,923<br>5,096,923<br>-<br>**22,653,181**<br>**5,137,155**<br>**_4.41_**_×_|



## _D. Area cost_ 

Table V present the SoC hardware-resources utilization on the Kintex-7 FPGA. As the first-ever hardware implementation of the FAFFT-based multiplication method, our results set a new benchmark in the field. The _Slice Equivalent Cost_ (SEC) metric [22] collects and weights various hardware resource costs in a single value. For our Kintex-7 FPGA platform, 

**==> picture [237 x 11] intentionally omitted <==**

Around 14% of ANDROMEDA’s slices are dedicated to its two Carry-Free Butterfly units, confirming their pivotal role in accelerating the main computational bottlenecks while maintaining a compact resource utilization. 

TABLE V: ANDROMEDA hardware resources utilizations 

||**LUT**<br>**FF**<br>**BRAM**<br>**DSP**<br>**SEC**|
|---|---|
|||
|**ANDROMEDA**<br>_⌊_1_×_ CFB butterfy|**2482**<br>**1158**<br>**8**<br>**0**<br>**2356**<br>600<br>127<br>0<br>0<br>166|



## _A. System-on-Chip performance of HQC_ 

We integrated ANDROMEDA as a loosely-coupled accelerator into the already-used RISC-V SoC platform illustrated in Figure 4. We repeated the Synthesis and Place&Route steps using Xilinx Vivado 2019.1, targeting the Kintex-7 FPGA. 

**==> picture [207 x 94] intentionally omitted <==**

**----- Start of picture text -----**<br>
CV32E40P ANDROMEDA<br>OBI System Bus<br>Instruction Data<br>UART JTAG<br>Memory Memory<br>**----- End of picture text -----**<br>


Fig. 4: System-on-Chip (SoC) RISC-V-based platform integrating loosely-coupled ANDROMEDA accelerator 

Table VI compares the performance (and relative speedup) of our ANDROMEDA integration against both the official HQC implementation and our software-only FAFFT-based alternative. For a fair comparison, the co-design was compiled using the riscv32-unknown-elf-gcc toolchain with - O3 optimization and executed on the FPGA platform. 

Table VI highlights the performance benefits of the proposed FAFFT-based approach across different HQC security levels. The integration of the ANDROMEDA accelerator significantly enhances performance, yielding KEM execution speedups of 9.64 _×_ , 13.87 _×_ and 19.22 _×_ over the original HQC implementation, and providing an additional improvement of 2.74 _×_ , 2.79 _×_ and 2.38 _×_ over our FAFFT-based software variant. 

TABLE VI: Cycle counts for official HQC, FAFFT-based software-only, and ANDROMEDA co-design. The relative speed-ups achieved by the co-design are given in brackets. 

||Work|Level|KeyGen|Encaps|Decaps|
|---|---|---|---|---|---|
|||HQC-128|43,592,464<br>[**10.12**_×_]|87,574,395<br>[**9.63**_×_]|132,101,254<br>[**9.18**_×_]|
||SW HQC [4]|HQC-192|132,528,662<br>[**14.35**_×_]|265,882,733<br>[**13.80**_×_]|399,562,345<br>[**13.47**_×_]|
||**This work**<br>(SW)|HQC-256<br>HQC-128<br>HQC-192|242,546,831<br>[**20.27**_×_]<br>12,208,482<br>[**2.83**_×_]<br>26,347,517<br>[**2.85**_×_]|486,241,524<br>[**19.39**_×_]<br>24,882,981<br>[**2.74**_×_]<br>53,528,998<br>[**2.78**_×_]|731,405,854<br>[**18.01**_×_]<br>38,099,269<br>[**2.65**_×_]<br>81,043,401<br>[**2.73**_×_]|
|||HQC-256|29,545,963<br>[**2.47**_×_]|60,238,075<br>[**2.40**_×_]|91,987,793<br>[**2.27**_×_]|
||**This work**|HQC-128|4,308,552|9,093,313|14,395,188|
||(HW/SW)|HQC-192<br>HQC-256|9,233,023<br>11,963,005|19,273,343<br>25,072,159|29,659,916<br>40,604,459|



## VI. CONCLUSION 

## V. INTEGRATION ON RISCV-BASED PLATFORM 

In this section, we evaluate the performance gains of HQC when using ANDROMEDA in a resource-constrained RISCV-based system-on-chip scenario. 

The future standardization of HQC and its recent multiplication strategy update, driven by security considerations, presents new challenges and opportunities for both software and hardware implementations. In this work, we introduce 

the first integration of the alternative Frobenius Additive FFT (FAFFT) multiplication method across all HQC security levels. Our comparison with the latest HQC release demonstrates significant algorithmic improvements with respect to the reference software implementation. Additionally, this technique enables an efficient hardware acceleration strategy, ANDROMEDA, which further boosts HQC performance when integrated into a system-on-chip RISC-V-based scenario, while maintaining minimal hardware resource overhead. 

- [22] M. Li, J. Tian, X. Hu, and Z. Wang, “Reconfigurable and highefficiency polynomial multiplication accelerator for Crystals-Kyber,” _IEEE Transactions on Computer-Aided Design of Integrated Circuits and Systems_ , vol. 42, no. 8, pp. 2540–2551, 2022. 

## REFERENCES 

- [1] P. W. Shor, “Algorithms for quantum computation: Discrete logarithms and factoring,” in _Proceedings of the 35th Annual Symposium on Foundations of Computer Science (FOCS)_ , pp. 124–134, IEEE, 1994. 

- [2] D. J. Bernstein, J. Buchmann, and E. Dahmen, “Introduction to postquantum cryptography,” _Post-Quantum Cryptography_ , pp. 1–14, 2009. 

- [3] L. Chen, S. Jordan, Y.-K. Liu, D. Moody, and R. Peralta, “Report on post-quantum cryptography,” Tech. Rep. NISTIR 8105, National Institute of Standards and Technology (NIST), 2016. Accessed: 202404-30. 

- [4] C. Aguilar Melchor, N. Aragon, S. Bettaieb, L. Bidoux, O. Blazy, J. Bos, J.-C. Deneuville, A. Dion, P. Gaborit, J. Lacan, E. Persichetti, J.-M. Robert, P. V´eron, and G. Z´emor, “HQC (Hamming Quasi-Cyclic),” 2024. 

- [5] G. Alagic _et al._ , “(2025) status report on the fourth round of the nist post-quantum cryptography standardization process,” 2025. https://doi. org/10.6028/NIST.IR.8545. 

- [6] M.-S. Chen, C.-M. Cheng, P.-C. Kuo, W.-D. Li, and B.-Y. Yang, “Faster multiplication for long binary polynomials,” _arXiv preprint arXiv:1708.09746_ , 2017. 

- [7] M.-S. Chen, C.-M. Cheng, P.-C. Kuo, W.-D. Li, and B.-Y. Yang, “Multiplying boolean polynomials with Frobenius partitions in additive Fast Fourier Transform,” _arXiv preprint arXiv:1803.11301_ , 2018. 

- [8] M.-S. Chen, T. Chou, and M. Krausz, “Optimizing BIKE for the Intel Haswell and ARM Cortex-M4,” _IACR Transactions on Cryptographic Hardware and Embedded Systems_ , pp. 97–124, 2021. 

- [9] D. Hofheinz, K. H¨ovelmanns, and E. Kiltz, “A modular analysis of the Fujisaki–Okamoto transformation,” in _Theory of Cryptography Conference_ , pp. 341–371, Springer, 2017. 

- [10] S. Deshpande, C. Xu, M. Nawan, K. Nawaz, and J. Szefer, “Fast and efficient hardware implementation of HQC,” in _International Conference on Selected Areas in Cryptography_ , pp. 297–321, Springer, 2023. 

- [11] T. H. Cormen, C. E. Leiserson, R. L. Rivest, and C. Stein, _Introduction to algorithms_ . MIT press, 2022. 

- [12] W.-D. Li, M.-S. Chen, P.-C. Kuo, C.-M. Cheng, and B.-Y. Yang, “Frobenius additive Fast Fourier Transform,” in _Proceedings of the 2018 ACM International Symposium on Symbolic and Algebraic Computation_ , pp. 263–270, 2018. 

- [13] D. G. Cantor, “On arithmetical algorithms over finite fields,” _Journal of Combinatorial Theory, Series A_ , vol. 50, no. 2, pp. 285–300, 1989. 

- [14] S. Gao and T. Mateer, “Additive Fast Fourier Transforms over finite fields,” _IEEE Transactions on Information Theory_ , vol. 56, no. 12, pp. 6265–6272, 2010. 

- [15] V. Arlazov, Y. Dinitz, M. Kronrod, and I. Faradzhev, “On economical construction of the transitive closure of an oriented graph,” _Dokl. Akad. Nauk SSR_ , vol. 194, pp. 487–488, 1970. 

- [16] OpenHW Group, _CV32E40 User Manual_ , 2024. Accessed: 2024-06-11. 

- [17] OpenHW Group, _Open Bus Interface Protocol_ , 2024. https://github.com/ openhwgroup/obi, Accessed: 2024-04-07. 

- [18] I. Digilent, _Genesys 2 FPGA Board Reference Manual_ . Digilent, 2017. https://digilent.com/reference/ media/reference/programmable-logic/ genesys-2/genesys2 rm.pdf. 

- [19] I. Xilinx, _Vivado Design Suite User Guide_ . Xilinx, 2019. https://www.xilinx.com/support/documentation-navigation/design-hubs/ dh0002-vivado-design-hub.html. 

- [20] J. Mu _et al._ , “Scalable and conflict-free NTT hardware accelerator design: Methodology, proof, and implementation,” _IEEE Transactions on Computer-Aided Design of Integrated Circuits and Systems_ , vol. 42, no. 5, pp. 1504–1517, 2022. 

- [21] I. Xilinx, _XC7Z020-1CLG400C: Zynq-7000 All Programmable SoC_ , 2023. https://docs.xilinx.com/v/u/en-US/ds190-Zynq-7000-Overview. 

