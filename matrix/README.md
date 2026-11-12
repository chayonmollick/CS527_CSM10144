# Matrix problems: everything in one place

Every matrix-related problem in this repo, with a link and a one-line description.
They are grouped as **(A)** mini-computer programs in the course language (Labs 1–6),
**(B)** OpenMP programs in C (Labs 7–9), **(C)** the rules in
[`MATRIX_RULES.md`](../MATRIX_RULES.md), and **(D)** the new programs in this folder.
Entries marked ★ are new in this folder.

```bash
cd matrix/minicomputer && ./run.sh   # builds ../../lab6/sim if needed; runs the 14 programs: 81 checks
cd matrix/openmp && ./run.sh         # builds the 8 programs in a temp dir and checks them: 8 programs
cd matrix/openmp && make && ./m04_cholesky 1000 && make clean   # build here, run one, tidy up
```

## Find it fast

| Operation | Mini-computer (course language) | OpenMP (C) |
|---|---|---|
| A ± B, kA, A∘B | [matrix_add_vec](../minicomputer/programs/matrix_add_vec.txt), ★[mat_sub_vec](minicomputer/mat_sub_vec.txt), ★[scalar_mul_vec](minicomputer/scalar_mul_vec.txt), ★[hadamard_vec](minicomputer/hadamard_vec.txt) | [lab7 task4](../lab7/task4_matadd.c), [q11](../practice/q11_matrix_checks.c), [p055](../problems/p055_matrix_linear_combo.c) |
| A·B, A·A, Aᵖ | [matmul4](../minicomputer/programs/matmul4.txt), [matmul8_vec](../minicomputer/programs/matmul8_vec.txt), ★[mat_square](minicomputer/mat_square.txt) | [lab7 task5](../lab7/task5_matmul.c), [q13](../practice/q13_matmul_variants.c), [p065](../problems/p065_rect_matmul.c), [p067](../problems/p067_matrix_power.c), [p068](../problems/p068_block_matmul_tasks.c), [p069](../problems/p069_triangular_matmul.c) |
| A·x, Aᵀ·x | [matvec_vec](../minicomputer/programs/matvec_vec.txt) | [q05](../practice/q05_matvec.c), [p056](../problems/p056_matvec_transpose.c), [p066](../problems/p066_collapse_correctness.c), [p076](../problems/p076_sparse_csr_spmv.c) |
| Transpose | [transpose](../minicomputer/programs/transpose.txt), ★[transpose_inplace](minicomputer/transpose_inplace.txt), [q044](../final_practice/q044_strided_vector_load/README.md) | [lab8 task4](../lab8/task4_transpose.c), [q14](../practice/q14_transpose_inplace.c) |
| Rotate, flip | ★[rotate90](minicomputer/rotate90.txt) | [p063](../problems/p063_rotate_matrix.c), [p064](../problems/p064_flip_matrix.c) |
| Row / column / total sums | ★[row_sums](minicomputer/row_sums.txt), ★[col_sums_vec](minicomputer/col_sums_vec.txt), ★[sum_all_vec](minicomputer/sum_all_vec.txt) | [q10](../practice/q10_matrix_stats.c), [p061](../problems/p061_normalize_rows_cols.c), [p062](../problems/p062_row_col_extremes.c), ★[m07](openmp/m07_tile_sums.c) |
| Trace, diagonals | [q015](../final_practice/q015_diagonals/solution.txt) | [p058](../problems/p058_diagonal_sums.c), [q10](../practice/q10_matrix_stats.c) |
| Max / min with (i, j), search, count | ★[max_elem](minicomputer/max_elem.txt), ★[count_zeros](minicomputer/count_zeros.txt) | [q10](../practice/q10_matrix_stats.c), [p054](../problems/p054_matrix_search.c), [p062](../problems/p062_row_col_extremes.c), ★[m06](openmp/m06_saddle_points.c) |
| Identity, triangular, symmetric checks | ★[is_identity](minicomputer/is_identity.txt), ★[is_upper](minicomputer/is_upper.txt), [q018](../final_practice/q018_symmetric/solution.txt) | [q11](../practice/q11_matrix_checks.c), [p059](../problems/p059_matrix_property_checks.c), [p060](../problems/p060_triangle_sum.c) |
| Determinant, inverse, solving Ax = b | [det3x3](../minicomputer/examples/det3x3.txt), ★[det_adj2x2](minicomputer/det_adj2x2.txt) | [p070](../problems/p070_lu_decomposition.c)–[p074](../problems/p074_jacobi_linear_solver.c), ★[m04](openmp/m04_cholesky.c), ★[m05](openmp/m05_forward_substitution.c) |
| Norms, Gram, Kronecker, outer product | | [q10](../practice/q10_matrix_stats.c), ★[m02](openmp/m02_matrix_norms.c), ★[m03](openmp/m03_gram_matrix.c), ★[m01](openmp/m01_kronecker.c), [p057](../problems/p057_outer_product.c) |
| Sparse matrices | ★[count_zeros](minicomputer/count_zeros.txt) | [p076](../problems/p076_sparse_csr_spmv.c), ★[m08](openmp/m08_coo_to_csr.c) |
| Stencils, convolution, grids | | [lab8 task5](../lab8/task5_convolution.c), [q32](../practice/q32_convolution_kernels.c), [q33](../practice/q33_jacobi_heat.c), [q34](../practice/q34_game_of_life.c), [p077](../problems/p077_gauss_seidel_red_black.c)–[p086](../problems/p086_mandelbrot.c) |
| DP tables, graphs as matrices | | [p078](../problems/p078_wavefront_antidiagonal.c), [p088](../problems/p088_pascal_triangle.c), [p093](../problems/p093_floyd_warshall.c), [p103](../problems/p103_matrix_chain_dp.c) |

---

## A. Mini-computer programs (course language, Labs 1–6)

Run one with `../lab6/sim -b <program>.txt <data>.byte` and read the result with
`minicomputer/tools/words.sh <data>_1.out`. How to write these programs (language, loop
skeletons, vector idioms): [`minicomputer/programs/README.md`](../minicomputer/programs/README.md).

| File | Task |
|---|---|
| [`minicomputer/examples/det3x3.txt`](../minicomputer/examples/det3x3.txt) | Determinant of a 3×3 matrix by cofactor expansion (Lab 1 example, straight-line code), result at 36 |
| [`minicomputer/programs/matrix_add_vec.txt`](../minicomputer/programs/matrix_add_vec.txt) | C = A + B for 8×8 matrices with vectors: one row = one vector, 4 vector instructions per row |
| [`minicomputer/programs/matvec_vec.txt`](../minicomputer/programs/matvec_vec.txt) | y = M·x for an 8×8 matrix: one vector multiply per row, then a sum of the 8 lanes |
| [`minicomputer/programs/matmul4.txt`](../minicomputer/programs/matmul4.txt) | C = A·B for 4×4 matrices with a scalar triple loop (i, j, k) |
| [`minicomputer/programs/matmul8_vec.txt`](../minicomputer/programs/matmul8_vec.txt) | C = A·B for 8×8 with vectors: row i of C = Σ A[i][k] · (row k of B), vector × scalar register |
| [`minicomputer/programs/transpose.txt`](../minicomputer/programs/transpose.txt) | Transpose of an N×N matrix, out of place (row i of A becomes column i of T) |
| [`final_practice/q015_diagonals`](../final_practice/q015_diagonals/README.md) ([solution](../final_practice/q015_diagonals/solution.txt)) | Trace and anti-diagonal sum of an 8×8 matrix (pointers step 36 and 28 bytes) |
| [`final_practice/q018_symmetric`](../final_practice/q018_symmetric/README.md) ([solution](../final_practice/q018_symmetric/solution.txt)) | Is a 4×4 matrix symmetric? Compares the 6 pairs above the diagonal with their mirror images |
| [`final_practice/q044_strided_vector_load`](../final_practice/q044_strided_vector_load/README.md) | Simulator extension: a strided vector load `vD = [x1], x2` that loads a column, used to transpose an 8×3 matrix |

Related array programs (a row-major matrix is one array of R·C words):

| File | Task |
|---|---|
| [`final_practice/q019_vec_product`](../final_practice/q019_vec_product/README.md) ([solution](../final_practice/q019_vec_product/solution.txt)) | Element-wise product of two arrays with vectors, any N: the Hadamard product of two flattened matrices |
| [`minicomputer/programs/dot_product_vec.txt`](../minicomputer/programs/dot_product_vec.txt) | Dot product with vectors: the row · column step of matvec and matmul |
| [`minicomputer/programs/sum_squares_vec.txt`](../minicomputer/programs/sum_squares_vec.txt) | Sum of squares with vectors and a shift-and-add lane sum (the squared Frobenius norm of a flattened matrix) |
| [`minicomputer/programs/scale_offset_vec.txt`](../minicomputer/programs/scale_offset_vec.txt) | Y = a·X + b with vectors, any N (kA + b for a flattened matrix) |

## B. OpenMP programs (C, Labs 7–9)

Each program has a serial version, the parallel version(s), a correctness check (OK/WRONG)
and a timing table for 1, 2, 4 and 8 threads. Build one with
`gcc -O2 -Wall -Wextra -fopenmp file.c -o prog -lm`.

**Lab tasks**

| File | Task |
|---|---|
| [`lab7/task4_matadd.c`](../lab7/task4_matadd.c) | C = A + B: parallel outer loop vs `collapse(2)`, N = 8 vs 4096 (answers in [`lab7/REPORT.md`](../lab7/REPORT.md)) |
| [`lab7/task5_matmul.c`](../lab7/task5_matmul.c) | Naive matmul: parallel i, `collapse(2)` on (i, j), never the k sum; bonus i-k-j order |
| [`lab8/task4_transpose.c`](../lab8/task4_transpose.c) | Out-of-place transpose: outer loop vs `collapse(2)` (answers in [`lab8/REPORT.md`](../lab8/REPORT.md)) |
| [`lab8/task5_convolution.c`](../lab8/task5_convolution.c) | 3×3 averaging filter, valid positions only: N×N image to (N−2)×(N−2) |

**Practice bank** ([`practice/README.md`](../practice/README.md))

| File | Task |
|---|---|
| [`practice/q05_matvec.c`](../practice/q05_matvec.c) | y = A·x: row-parallel vs column-parallel with `reduction(+:y[:n])` |
| [`practice/q10_matrix_stats.c`](../practice/q10_matrix_stats.c) | Row and column sums, trace, Frobenius norm, maximum with its (i, j) |
| [`practice/q11_matrix_checks.c`](../practice/q11_matrix_checks.c) | A == B, symmetric, identity with `reduction(&&:)`; A − B and kA with `collapse(2)` |
| [`practice/q13_matmul_variants.c`](../practice/q13_matmul_variants.c) | Matmul ijk, ikj, with Bᵀ, and 64×64 blocked; GFLOP/s |
| [`practice/q14_transpose_inplace.c`](../practice/q14_transpose_inplace.c) | In-place transpose (triangular loop: static vs dynamic vs guided) and blocked out of place |
| [`practice/q32_convolution_kernels.c`](../practice/q32_convolution_kernels.c) | 3×3 blur, Gaussian, sharpen and Sobel kernels; valid and same padding; the handout's 5×5 example |
| [`practice/q33_jacobi_heat.c`](../practice/q33_jacobi_heat.c) | 2D heat diffusion (Jacobi) until convergence: `reduction(max:diff)`, one region vs one per sweep |
| [`practice/q34_game_of_life.c`](../practice/q34_game_of_life.c) | Game of Life: double buffering, `collapse(2) reduction(+:pop)` |

**Problem bank** (full statements in [`PROBLEMS.md`](../PROBLEMS.md)): matrices and linear algebra

| File | Task |
|---|---|
| [`problems/p053_sort_rows.c`](../problems/p053_sort_rows.c) | Sort every row (and every column) of a matrix: one row per iteration, no shared writes |
| [`problems/p054_matrix_search.c`](../problems/p054_matrix_search.c) | Search a value: count, first occurrence via `reduction(min:)` on the linear index, all positions |
| [`problems/p055_matrix_linear_combo.c`](../problems/p055_matrix_linear_combo.c) | αA + βB and the Hadamard product A∘B: outer loop vs `collapse(2)` |
| [`problems/p056_matvec_transpose.c`](../problems/p056_matvec_transpose.c) | y = Aᵀx: parallel over columns (stride N) vs rows with an array reduction |
| [`problems/p057_outer_product.c`](../problems/p057_outer_product.c) | Outer product x·yᵀ and the rank-1 update A += x·yᵀ with `collapse(2)` |
| [`problems/p058_diagonal_sums.c`](../problems/p058_diagonal_sums.c) | Trace, anti-diagonal, border, and all 2N − 1 diagonal sums |
| [`problems/p059_matrix_property_checks.c`](../problems/p059_matrix_property_checks.c) | Upper/lower triangular, diagonal, strictly diagonally dominant with `reduction(&&:)` |
| [`problems/p060_triangle_sum.c`](../problems/p060_triangle_sum.c) | Sum of the strict upper triangle: schedules for a triangular loop, row pairing |
| [`problems/p061_normalize_rows_cols.c`](../problems/p061_normalize_rows_cols.c) | Normalise every row; standardise every column (mean 0, sd 1) |
| [`problems/p062_row_col_extremes.c`](../problems/p062_row_col_extremes.c) | Row with the largest sum, column with the smallest sum, every row's maximum |
| [`problems/p063_rotate_matrix.c`](../problems/p063_rotate_matrix.c) | Rotate 90° out of place and in place (transpose, then reverse rows) |
| [`problems/p064_flip_matrix.c`](../problems/p064_flip_matrix.c) | Flip left-right and up-down, rotate 180° in place: disjoint swaps over half the matrix |
| [`problems/p065_rect_matmul.c`](../problems/p065_rect_matmul.c) | Rectangular product (m×k)·(k×n) with `collapse(2)` and dimension checks |
| [`problems/p066_collapse_correctness.c`](../problems/p066_collapse_correctness.c) | When `collapse(2)` is legal and when it is correct (matvec race, triple loop) |
| [`problems/p067_matrix_power.c`](../problems/p067_matrix_power.c) | Aᵖ by repeated squaring: serial exponent loop, parallel products |
| [`problems/p068_block_matmul_tasks.c`](../problems/p068_block_matmul_tasks.c) | Recursive (divide and conquer) block matmul with tasks |
| [`problems/p069_triangular_matmul.c`](../problems/p069_triangular_matmul.c) | Product of two upper-triangular matrices: load balance with dynamic and static,1 |
| [`problems/p070_lu_decomposition.c`](../problems/p070_lu_decomposition.c) | LU without pivoting (Doolittle, in place): serial k loop, parallel rows |
| [`problems/p071_gaussian_elimination.c`](../problems/p071_gaussian_elimination.c) | Ax = b by Gaussian elimination with partial pivoting and back substitution |
| [`problems/p072_determinant.c`](../problems/p072_determinant.c) | Determinant by elimination: product of pivots, sign flip per row swap |
| [`problems/p073_matrix_inverse.c`](../problems/p073_matrix_inverse.c) | Inverse by Gauss–Jordan elimination with partial pivoting |
| [`problems/p074_jacobi_linear_solver.c`](../problems/p074_jacobi_linear_solver.c) | Jacobi iteration for Ax = b: double buffering, convergence test |
| [`problems/p075_power_iteration.c`](../problems/p075_power_iteration.c) | Dominant eigenvalue by power iteration: matvec, dot products, normalise |
| [`problems/p076_sparse_csr_spmv.c`](../problems/p076_sparse_csr_spmv.c) | Sparse CSR matrix–vector product with very uneven rows: static vs dynamic vs guided |

**Problem bank**: grids, stencils and images (2D arrays stored as matrices)

| File | Task |
|---|---|
| [`problems/p077_gauss_seidel_red_black.c`](../problems/p077_gauss_seidel_red_black.c) | Red-black Gauss–Seidel vs Jacobi for a 2D equation solver |
| [`problems/p078_wavefront_antidiagonal.c`](../problems/p078_wavefront_antidiagonal.c) | Wavefront: x[i][j] needs x[i−1][j] and x[i][j−1], so the anti-diagonals run in order |
| [`problems/p079_heat_explicit.c`](../problems/p079_heat_explicit.c) | 2D heat equation, explicit time steps with double buffering |
| [`problems/p080_image_pointwise.c`](../problems/p080_image_pointwise.c) | Point-wise image operations (grayscale, negative, threshold, contrast) with `collapse(2)` |
| [`problems/p081_histogram_equalization.c`](../problems/p081_histogram_equalization.c) | Histogram equalisation of a W×H image: array reduction, then a per-pixel lookup |
| [`problems/p082_median_filter.c`](../problems/p082_median_filter.c) | 3×3 median filter: every output pixel from the input only, `collapse(2)` |
| [`problems/p083_pooling.c`](../problems/p083_pooling.c) | Max and average pooling (k×k window, stride s) |
| [`problems/p084_summed_area_table.c`](../problems/p084_summed_area_table.c) | Summed-area table (2D prefix sum): rows in parallel, then columns; rectangle queries |
| [`problems/p085_cellular_automaton.c`](../problems/p085_cellular_automaton.c) | 1D elementary cellular automata and a 2D majority vote on a grid (double buffering) |
| [`problems/p086_mandelbrot.c`](../problems/p086_mandelbrot.c) | Mandelbrot image row by row: uneven rows, static vs dynamic vs guided |

**Problem bank**: dynamic-programming tables and graphs as matrices

| File | Task |
|---|---|
| [`problems/p088_pascal_triangle.c`](../problems/p088_pascal_triangle.c) | Pascal's triangle in a packed triangular array: rows in order, entries of a row in parallel |
| [`problems/p093_floyd_warshall.c`](../problems/p093_floyd_warshall.c) | All-pairs shortest paths on an adjacency matrix: serial k loop, parallel (i, j) |
| [`problems/p103_matrix_chain_dp.c`](../problems/p103_matrix_chain_dp.c) | Matrix-chain order and LCS tables by wavefront (cells of one chain length / diagonal in parallel) |

## C. Rules: [`MATRIX_RULES.md`](../MATRIX_RULES.md)

| Section | What it gives |
|---|---|
| [1. Storage and indexing](../MATRIX_RULES.md#1-storage-and-indexing-c-is-row-major) | Row-major `A[(long)i*n + j]`, `double **` rows, column-major, packed triangle, linear index ↔ (i, j) |
| [2. Dimension rules](../MATRIX_RULES.md#2-dimension-rules) | Sizes and element formulas of A ± B, kA, Aᵀ, A·x, A·B, outer and dot products |
| [3. Algebra rules](../MATRIX_RULES.md#3-algebra-rules) | Transpose, trace, determinant, inverse, symmetry and norm identities (‖A‖₁, ‖A‖∞, Frobenius) |
| [4. Special matrices](../MATRIX_RULES.md#4-special-matrices-index-conditions-and-tests) | Index conditions and parallel tests: identity, diagonal, triangular, symmetric, dominant, sparse |
| [5. Index regions](../MATRIX_RULES.md#5-index-regions-nn) | Diagonals, anti-diagonals, triangles, border, interior, neighbours, red/black, tiles (with counts) |
| [6. Rearrangements](../MATRIX_RULES.md#6-rearrangements-index-maps-nn-b-from-a) | Index maps for transpose, rotate 90°/180°, flips; in-place versions |
| [7. Parallelisation rules](../MATRIX_RULES.md#7-parallelisation-rules-for-matrix-code) | Which loop to parallelise, `collapse(2)` legality, in place, serial outer steps, cache, flop counts |
| [8. Solved matrix problems](../MATRIX_RULES.md#8-solved-matrix-problems-in-this-repo) | A shorter index of the solved OpenMP matrix problems (this file lists more) |

Also: [`CHEATSHEET.md`](../CHEATSHEET.md) section 2 (allocating a contiguous `double **` matrix)
and section 4 (Lab 7 and Lab 8 report answers on matrix add, matmul and transpose).

## D. New in this folder

### D1. Mini-computer programs (`minicomputer/`)

Each program has a header (task, data layout with addresses, result location, method,
variants, registers), one to three data files (`<name>_data*.byte`) and
`<name>_ref.cpp`, the same task in plain C++17 that prints the expected words as
`<address>: <value>`. [`minicomputer/run.sh`](minicomputer/run.sh) runs every program on every
data file, compares each word with the reference, and checks values worked out by hand.

| File | Task | Method |
|---|---|---|
| ★[`scalar_mul_vec.txt`](minicomputer/scalar_mul_vec.txt) | B = kA for any R×C matrix (k from the data, may be negative) | Vector blocks of 8 with `v1 * x5`, scalar tail |
| ★[`mat_sub_vec.txt`](minicomputer/mat_sub_vec.txt) | C = A − B and D = 2A − 3B for 4×4 matrices | Vector, no loop: a 4×4 matrix is exactly 2 vectors |
| ★[`hadamard_vec.txt`](minicomputer/hadamard_vec.txt) | H = A∘B for 8×8, the column sums of H, and S = Σ aᵢⱼbᵢⱼ | Vector: one row = one vector; lane sum at the end |
| ★[`row_sums.txt`](minicomputer/row_sums.txt) | Row sums of an R×C matrix and the row with the largest sum | Scalar nested loops, one pointer through A |
| ★[`col_sums_vec.txt`](minicomputer/col_sums_vec.txt) | The 8 column sums of an R×8 matrix | Vector: one vector add per row |
| ★[`sum_all_vec.txt`](minicomputer/sum_all_vec.txt) | Sum and integer mean of all elements of an R×C matrix | Vector blocks, scalar tail, shift-and-add lane sum |
| ★[`max_elem.txt`](minicomputer/max_elem.txt) | Largest element of an R×C matrix and its (row, column) | Scalar; row = k / C, column = k − row·C |
| ★[`count_zeros.txt`](minicomputer/count_zeros.txt) | Zero and nonzero elements and all-zero rows of an R×C matrix | Scalar nested loops, `x9 = x6 - 0` sets the flags |
| ★[`is_identity.txt`](minicomputer/is_identity.txt) | 1 if an N×N matrix is the identity, else 0 | One loop; a pointer to the next diagonal element (step 4(N+1)); early exit |
| ★[`is_upper.txt`](minicomputer/is_upper.txt) | 1 if an N×N matrix is upper triangular, and the nonzeros below the diagonal | Loops over the strict lower triangle only |
| ★[`transpose_inplace.txt`](minicomputer/transpose_inplace.txt) | Transpose an N×N matrix in place (4×4 and 3×3 data) | Swap A[i][j] ↔ A[j][i] for j > i only |
| ★[`rotate90.txt`](minicomputer/rotate90.txt) | Rotate an N×N matrix 90° clockwise in place | Transpose in place, then reverse every row |
| ★[`mat_square.txt`](minicomputer/mat_square.txt) | C = A·A for an N×N matrix (4×4 and 2×2 data) | Scalar triple loop: row pointer +4, column pointer +4N |
| ★[`det_adj2x2.txt`](minicomputer/det_adj2x2.txt) | 2×2 determinant, adjugate (A⁻¹ = adj A / det), invertible flag, check A·adj A = det·I | Straight-line scalar code, constant addresses |

### D2. OpenMP programs (`openmp/`)

Same format as `problems/` (they include [`problems/common.h`](../problems/common.h)): problem and
solution in the header, serial and parallel versions, OK/WRONG checks including a small
example worked by hand, a timing table for 1/2/4/8 threads, and viva notes at the end.
C99 and OpenMP 4.5 only, so they build with gcc 9. [`openmp/Makefile`](openmp/Makefile)
builds them all; [`openmp/run.sh`](openmp/run.sh) builds them in a temp dir and runs each at a
small size.

| File | Problem | Key constructs |
|---|---|---|
| ★[`m01_kronecker.c`](openmp/m01_kronecker.c) | Kronecker product A⊗B (m×n ⊗ p×q); trace(A⊗B) = trace A · trace B | Parallel over output rows vs `collapse(2)` over blocks |
| ★[`m02_matrix_norms.c`](openmp/m02_matrix_norms.c) | ‖A‖₁ (max column sum), ‖A‖∞ (max row sum), ‖A‖max | `reduction(max:)`; 1-norm by columns vs `reduction(+:cs[:n])` |
| ★[`m03_gram_matrix.c`](openmp/m03_gram_matrix.c) | Gram matrix G = AᵀA (j ≥ i only, then mirrored) | `schedule(dynamic)`, transpose first, private G per thread + `critical` |
| ★[`m04_cholesky.c`](openmp/m04_cholesky.c) | Cholesky A = LLᵀ (right-looking, in place); rejects a matrix that is not SPD | Serial k loop in one region, `omp for` per step, dynamic trailing update |
| ★[`m05_forward_substitution.c`](openmp/m05_forward_substitution.c) | Lx = b: why the row loop is serial; row, column, blocked and many-right-hand-side versions | `reduction(+:s)` per row, `single` + `omp for`, parallel over right-hand sides |
| ★[`m06_saddle_points.c`](openmp/m06_saddle_points.c) | Saddle points: smallest in the row and largest in the column; count and first | `reduction(max:cmax[:n])`, `collapse(2) reduction(+:count) reduction(min:first)` |
| ★[`m07_tile_sums.c`](openmp/m07_tile_sums.c) | b×b tile sums, edge tiles smaller when b does not divide N | `collapse(2)` over tiles vs `reduction(+:S[:nt*nt])` vs private S + `critical` |
| ★[`m08_coo_to_csr.c`](openmp/m08_coo_to_csr.c) | Sparse COO to CSR: row counts, prefix sum, scatter (identical to serial) | Per-thread histograms, blocked two-pass scan; `atomic capture` variant |
