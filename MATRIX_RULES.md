# Matrix rules for OpenMP problems

Indexing formulas, math rules and parallelisation rules for matrix questions.
Section 8 lists solved matrix problems in this repo.

---

## 1. Storage and indexing (C is row-major)

| Layout | Declare / allocate | Element (i, j) |
|---|---|---|
| Flattened, square n×n (used in this repo) | `double *A = malloc((size_t)n*n*sizeof *A);` | `A[(long)i*n + j]` |
| Flattened, rows×cols | `malloc((size_t)rows*cols*sizeof *A)` | `A[(long)i*cols + j]` |
| 2D pointer to rows (contiguous, C99) | `double (*A)[n] = malloc(sizeof(double[n][n]));` | `A[i][j]` |
| Array of row pointers (`double **`, as in the Lab 7 sample) | rows = `malloc(n*sizeof(double*))`, data = one `calloc(n*n)`, `rows[i] = data + i*n` | `A[i][j]` |
| Column-major (Fortran style) | same malloc | `A[(long)j*rows + i]` |

- Linear index ↔ (i, j): `k = i*cols + j`, `i = k / cols`, `j = k % cols`.
- Use `long` (or cast) for `i*cols`: 50 000 × 50 000 overflows `int`.
- Pass a contiguous 2D array to a function (C99): `void f(int n, double A[n][n])`.
- Big matrices go on the heap (`malloc`), not the stack (`double A[4096][4096]` overflows the stack).
- Packed upper triangle (j ≥ i), row by row: `idx(i, j) = i*n − i*(i−1)/2 + (j − i)`.

## 2. Dimension rules

| Operation | Sizes | Result | Element formula |
|---|---|---|---|
| A + B, A − B, A∘B (element-wise) | both m×n | m×n | `C[i][j] = A[i][j] ± B[i][j]` |
| kA | m×n | m×n | `k*A[i][j]` |
| Transpose Aᵀ | m×n | n×m | `T[j][i] = A[i][j]` |
| Matrix–vector A·x | m×n · n | m | `y[i] = Σ_j A[i][j]*x[j]` (row i · x) |
| Aᵀ·x | m×n, x of size m | n | `y[j] = Σ_i A[i][j]*x[i]` |
| Matrix product A·B | m×k · k×n | m×n | `C[i][j] = Σ_k A[i][k]*B[k][j]` (row i of A · column j of B) |
| Outer product x·yᵀ | m, n | m×n | `A[i][j] = x[i]*y[j]` |
| Dot product xᵀy | n, n | scalar | `Σ x[i]*y[i]` |

**Rule:** A·B exists only if (columns of A) = (rows of B). The result takes the rows of A and
the columns of B. Check the sizes in code before multiplying.

## 3. Algebra rules

- **Addition:** A + B = B + A and (A + B) + C = A + (B + C); k(A + B) = kA + kB.
- **Multiplication:** **AB ≠ BA** in general; (AB)C = A(BC); A(B + C) = AB + AC; IA = AI = A.
- **Transpose:** (Aᵀ)ᵀ = A; (A + B)ᵀ = Aᵀ + Bᵀ; (kA)ᵀ = kAᵀ; **(AB)ᵀ = BᵀAᵀ**.
- **Trace** (sum of the diagonal): tr(A + B) = tr A + tr B; tr(kA) = k·tr A; tr(Aᵀ) = tr A; **tr(AB) = tr(BA)**.
- **Determinant (n×n):**
  - det(AB) = det A · det B; det Aᵀ = det A; det(kA) = kⁿ det A; det A⁻¹ = 1/det A.
  - Triangular or diagonal matrix: det = product of the diagonal.
  - Swapping two rows flips the sign; adding a multiple of one row to another changes nothing.
  - A zero row or two equal rows means det = 0.
  - Gaussian elimination gives det = (product of pivots) × (−1)^(number of row swaps).
- **Inverse:** exists ⇔ det A ≠ 0; (AB)⁻¹ = B⁻¹A⁻¹; (Aᵀ)⁻¹ = (A⁻¹)ᵀ; check with A·A⁻¹ = I.
- **Symmetry:** A + Aᵀ is symmetric, A − Aᵀ is skew-symmetric, and AAᵀ and AᵀA are symmetric.
- **Products of special matrices:** upper × upper = upper triangular; diagonal × diagonal = diagonal (and they commute).
- **Orthogonal:** QᵀQ = I, so Q⁻¹ = Qᵀ.
- **Norms:**
  - Frobenius ‖A‖_F = sqrt(Σ a_ij²).
  - ‖A‖₁ = max over columns of Σ|a_ij| (max column sum).
  - ‖A‖∞ = max over rows of Σ|a_ij| (max row sum).
- **Strictly diagonally dominant** (|a_ii| > Σ_{j≠i} |a_ij| for every row): the matrix is invertible,
  Jacobi and Gauss–Seidel converge, and LU without pivoting is safe. Use this to generate test matrices.

## 4. Special matrices: index conditions and tests

| Property | Condition (for all i, j) | Parallel test |
|---|---|---|
| Square | rows == cols | — |
| Identity | a_ij = (i == j ? 1 : 0) | `collapse(2) reduction(&&:ok)` |
| Diagonal | a_ij = 0 for i ≠ j | same |
| Upper triangular | a_ij = 0 for i > j | loop over j < i only, `reduction(&&:ok)` |
| Lower triangular | a_ij = 0 for i < j | loop over j > i only |
| Symmetric | a_ij = a_ji | loop over j > i only (half the pairs) |
| Skew-symmetric | a_ij = −a_ji (so a_ii = 0) | same |
| Equal | a_ij = b_ij | `collapse(2) reduction(&&:ok)` |
| Strictly diagonally dominant | \|a_ii\| > Σ_{j≠i}\|a_ij\| | parallel over rows, `reduction(&&:ok)` |
| Sparse | most a_ij = 0 | store as CSR: `row_ptr`, `col_idx`, `val` |

## 5. Index regions (n×n)

| Region | Condition / loop | Count |
|---|---|---|
| Main diagonal | i == j | n |
| Anti-diagonal | i + j == n − 1 | n |
| k-th diagonal (k = j − i) | `for i: j = i + k` (0 ≤ j < n) | n − \|k\| (2n − 1 diagonals) |
| Anti-diagonal d (wavefront) | i + j == d, `for i = max(0, d−n+1) .. min(d, n−1)`, j = d − i | d = 0 .. 2n−2 |
| Strict upper triangle | j > i, `for i: for j = i+1 .. n−1` | n(n−1)/2 |
| Upper triangle incl. diagonal | j ≥ i | n(n+1)/2 |
| Border | i == 0 or i == n−1 or j == 0 or j == n−1 | 4n − 4 (n ≥ 2) |
| Interior | 1 ≤ i, j ≤ n−2 | (n−2)² |
| 4-neighbours of (i, j) | (i±1, j), (i, j±1) | 4 (fewer on the border) |
| 8-neighbours | all (i+di, j+dj), di, dj ∈ {−1, 0, 1}, not both 0 | 8 |
| Red / black (checkerboard) | (i + j) % 2 == 0 / == 1 | ≈ n²/2 each |
| Tile (block) (bi, bj) of size b | i ∈ [bi·b, min(bi·b+b, n)), same for j | ⌈n/b⌉² tiles |

## 6. Rearrangements: index maps (n×n, B from A)

| Operation | Out of place | In place |
|---|---|---|
| Transpose | `B[j][i] = A[i][j]` | swap A[i][j] ↔ A[j][i] for **j > i only** |
| Rotate 90° clockwise | `B[j][n−1−i] = A[i][j]` | transpose, then reverse each row |
| Rotate 90° anticlockwise | `B[n−1−j][i] = A[i][j]` | transpose, then reverse each column |
| Rotate 180° | `B[n−1−i][n−1−j] = A[i][j]` | swap (i, j) ↔ (n−1−i, n−1−j) over the first half of the elements |
| Flip left–right (mirror) | `B[i][n−1−j] = A[i][j]` | reverse each row: j < n/2 |
| Flip up–down | `B[n−1−i][j] = A[i][j]` | swap rows i ↔ n−1−i for i < n/2 |

Check on [[1,2],[3,4]]: 90° clockwise → [[3,1],[4,2]]; anticlockwise → [[2,4],[1,3]]; transpose → [[1,3],[2,4]].

## 7. Parallelisation rules for matrix code

1. **Parallelise the loop over output elements (usually rows `i`).** If each thread owns whole rows of
   the output, no two threads write the same element, so there is no race (matmul, matvec, add,
   convolution, transpose out of place).
2. **Never split a summation loop (`k` in `C[i][j] += A[i][k]*B[k][j]`) with a plain `parallel for`**:
   every iteration writes the same C[i][j]. Accumulate in a local `sum` per (i, j). If k really must be
   parallel, use `reduction(+:sum)`.
3. **`collapse(2)` is legal only for perfectly nested loops** (nothing between the two `for` headers)
   with rectangular bounds (in OpenMP 4.5 the inner bounds must not depend on i). It is *correct* only if
   every (i, j) iteration writes a different element. `for i for j y[i] += …` with collapse(2)
   compiles, but races on y[i]. It helps when the outer loop is short (N ≈ threads, e.g. N = 8).
4. **In-place operations:** an iteration must not read an element that another iteration writes.
   - In-place transpose, rotate and flip: loop over half the pairs and swap.
   - In-place stencil or Gauss–Seidel: use a second buffer (Jacobi), red-black ordering, or a wavefront.
   - In-place matmul (`A = A·B`) is wrong: write to a separate C.
5. **Sequential outer steps:** LU, Gaussian elimination, Gauss–Jordan inverse, Floyd–Warshall, time steps,
   power iteration and matrix powers have an outer loop where step k needs step k−1. Keep that loop serial
   and parallelise the work *inside* each step. The implicit barrier at the end of each `omp for`
   keeps the steps in order. Pivot search is a max-with-index reduction.
6. **Column-wise work in row-major storage** (column sums, Aᵀx): either parallelise over columns (each
   thread reads with stride n: slower, but no race) or over rows with an array reduction
   `reduction(+:cs[:n])`. Transposing first is another option.
7. **Uneven rows** (triangular loops, sparse rows, triangular matmul): `schedule(dynamic, c)`, `guided`,
   or `static,1`. A plain `static` gives the last thread (or the first) most of the work.
8. **Whole-matrix reductions:** `#pragma omp parallel for collapse(2) reduction(+:s)` (sum, Frobenius
   norm, count). For max with (i, j), keep a per-thread best and break ties on the linear index i·n + j.
9. **Wavefront** (anti-diagonals) for `x[i][j] = f(x[i−1][j], x[i][j−1])`, LCS, edit distance and
   in-place Gauss–Seidel: the loop over diagonals d is sequential, and the points on one diagonal run in parallel.
10. **Red-black** for 5-point stencils: update all red points (i + j even) in parallel, then all black.
    Each colour reads only the other colour.

**Cache and performance rules:**
- The inner loop should run over `j` (stride 1) in row-major storage.
- For matmul, the `i-k-j` order or a transposed B beats `i-j-k`. Blocking/tiling (32–64) helps large N.
- Power-of-two sizes (512, 1024, 4096) can cause cache-set conflicts. Compare with 1000 or 1500.
- Accumulate into a register (`double s = 0; … ; C[i][j] = s;`) instead of updating `C[i][j]` in the inner loop.
- Floating-point results computed in a different order differ in the last bits, so compare matrices with a
  relative tolerance (e.g. 1e-9). Compare integer matrices exactly.

**Operation counts (to explain timings):**

| Operation | Flops | Data | Bound by |
|---|---|---|---|
| Add / scale / transpose | n² | 2–3 n² values | memory bandwidth |
| Matrix–vector | 2n² | n² values | memory bandwidth |
| Matrix multiply (m×k·k×n) | 2mkn (2n³) | 3n² values | compute (if cache-friendly) |
| LU / Gaussian elimination | ≈ 2n³/3 | n² | compute, with a serial step loop |
| Inverse (Gauss–Jordan) | ≈ 2n³ | 2n² | compute |
| Floyd–Warshall | n³ comparisons | n² | compute/memory, with a serial k loop |
| 3×3 convolution | 18 n² | 2n² | memory, partly compute |

GFLOP/s = flops / time / 1e9. Matmul's n³ work over n² data is why it scales best.

## 8. Solved matrix problems in this repo

| Problem | File |
|---|---|
| Matrix add, outer vs collapse (N = 8 vs 4096) | `task4_matadd.c` |
| Matmul ijk / collapse / ikj | `task5_matmul.c`, `practice/q13_matmul_variants.c` |
| Transpose (out of place, in place, blocked) | `session2/task4_transpose.c`, `practice/q14_transpose_inplace.c` |
| 3×3 convolution and kernels | `session2/task5_convolution.c`, `practice/q32_convolution_kernels.c` |
| Matrix–vector, Aᵀx, sparse CSR | `practice/q05_matvec.c`, `problems/p056`, `problems/p076` |
| Row/column sums, trace, Frobenius, max with (i,j) | `practice/q10_matrix_stats.c`, `problems/p058`, `problems/p062` |
| Equal / symmetric / identity / triangular / diagonally dominant | `practice/q11_matrix_checks.c`, `problems/p059` |
| αA + βB, Hadamard, outer product | `problems/p055`, `problems/p057` |
| Triangular sums and products | `problems/p060`, `problems/p069` |
| Normalise rows and columns | `problems/p061` |
| Rotate and flip | `problems/p063`, `problems/p064` |
| Rectangular matmul, collapse legality | `problems/p065`, `problems/p066` |
| Matrix power, recursive block matmul with tasks | `problems/p067`, `problems/p068` |
| LU, Gaussian elimination, determinant, inverse | `problems/p070`–`p073` |
| Jacobi solver, power iteration | `problems/p074`, `problems/p075` |
| Gauss–Seidel red-black, wavefront | `problems/p077`, `problems/p078` |
| Heat equation, Jacobi heat, Game of Life | `problems/p079`, `practice/q33`, `practice/q34` |
| Sort rows, search in a matrix | `problems/p053`, `problems/p054` |
| Summed-area table, pooling, median filter | `problems/p084`, `problems/p083`, `problems/p082` |
| Floyd–Warshall, matrix-chain DP | `problems/p093`, `problems/p103` |
