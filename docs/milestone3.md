
# Specification Document: Milestone 3
## Level-Parallel Concurrency & Multi-Core Scalability (Taskflow)

* **Target:** `sta-engine` / `vlsi-sta-engine`
* **Status:** Ready for Implementation (Pre-requisites: Milestone 1 & Milestone 2)
* **Scope:** Multi-threaded forward/backward sweeps via Taskflow, level-based dependency graph construction, chunking strategies, ThreadSanitizer (TSan) audit, and Google Benchmark scaling analysis.
* **Excluded:** Zero-Slack Algorithm (ZSA) and Incremental Timing Engine modifications.

---

## 1. Milestone Objectives

1. Integrate the header-only/CMake **Taskflow** runtime into `sta-engine` without polluting core sequential algorithms.
2. Construct static Taskflow dependency pipelines enforcing inter-level barrier synchronization ($L_0 \to L_1 \to \dots \to L_{K-1}$ for forward sweeps; reversed for backward sweeps).
3. Implement intra-level work distribution using Taskflow's parallel range primitives over contiguous `topo_order_` slices.
4. Guarantee **Data-Race Freedom by Construction** and verify zero race conditions under ThreadSanitizer (TSan).
5. Establish a formal benchmarking harness using **Google Benchmark** to measure speedup and parallel efficiency across core counts ($1, 2, 4, 8, \dots$) on large ISCAS85 netlists (`c6288`, `c7552`).

---

## 2. Concurrency Model & Dependency Graph

### 2.1 The Inter-Level Execution Graph
Topological levelization guarantees that all pins in level $L_k$ depend strictly on settled values from levels $L_0, \dots, L_{k-1}$. Therefore:
* Levels must execute sequentially relative to one another.
* All nodes *within* a single level $L_k$ can execute concurrently with zero locks.

```
Forward Sweep Taskflow Graph:
  +---------------+       +---------------+               +-----------------+
  | Task(Level 0) | ====> | Task(Level 1) | ====> ... ==> | Task(Level K-1) |
  +---------------+       +---------------+               +-----------------+
    [Parallel For]          [Parallel For]                  [Parallel For]
    (Workers 1..N)          (Workers 1..N)                  (Workers 1..N)

Backward Sweep Taskflow Graph:
  +-----------------+       +-----------------+               +---------------+
  | Task(Level K-1) | ====> | Task(Level K-2) | ====> ... ==> | Task(Level 0) |
  +-----------------+       +-----------------+               +---------------+
    [Parallel For]            [Parallel For]                  [Parallel For]
    (Workers 1..N)            (Workers 1..N)                  (Workers 1..N)
```

---

### 2.2 Data-Race Freedom Invariants
Multi-threading introduces concurrency hazards if memory access patterns are unconstrained. The engine maintains thread safety by adhering to three invariants:

1. **Disjoint Writes (Pull Model):**
   * During the forward sweep, worker threads processing node $v$ read from predecessors in prior levels ($L_{<k}$) and write strictly to `at_late[v]` and `at_early[v]`.
   * No two worker threads ever write to the same `NodeId` memory index.
2. **Read-Only Predecessors:**
   * Predecessor arrival times belong to completed levels. They are strictly immutable throughout the execution of level $L_k$.
3. **Contiguous Index Slices:**
   * Nodes in level $L_k$ reside in a single contiguous subrange `[level_offsets[k], level_offsets[k+1])` inside `topo_order_`. There is zero indirect pointer dereferencing to locate node indices.

---

## 3. Parallel Sweeps Specification

### 3.1 Taskflow Runtime Lifecycle
* **`tf::Executor` Ownership:** The executor maintains the underlying worker thread pool. To eliminate thread spawn and teardown overhead, the executor must be instantiated once (owned by `StaEngine` or passed as a persistent execution context) and reused across multiple timing iterations.
* **Transient `tf::Taskflow` Graphs:** The task dependency graph can be built once during `finalize()` (since graph topology is static) or constructed transiently during the timing update call.

---

### 3.2 Parallel Forward Sweep Algorithm
1. Instantiate a task collection of size $K$ (number of levels).
2. For each level $k \in [0, K-1]$:
   * Query the level's index range: `[start_idx, end_idx)`.
   * Bind task $T_k$ to a Taskflow parallel-for over `[start_idx, end_idx)`.
   * Inside the kernel: Map flat index $i$ to node $v = \text{topo\_order}[i]$ and invoke the stateless relaxation kernel `relax_at(v, ...)`.
   * If $k > 0$, declare the inter-level dependency edge: $T_{k-1}.\text{precede}(T_k)$.
3. Submit the graph to `tf::Executor::run()` and block until completion (`.wait()`).

---

### 3.3 Parallel Backward Sweep Algorithm
1. Initialize endpoints in parallel: invoke a parallel-for over all timing sinks to seed $RAT_{\text{late}}$ and $RAT_{\text{early}}$.
2. Instantiate a task collection of size $K$.
3. For each level $k \in [K-1 \text{ down to } 0]$:
   * Query the level's index range: `[start_idx, end_idx)`.
   * Bind task $T'_k$ to a Taskflow parallel-for over `[start_idx, end_idx)`.
   * Inside the kernel: Map flat index $i$ to node $u = \text{topo\_order}[i]$. If $u$ is not a sink, invoke the stateless relaxation kernel `relax_rat(u, ...)`.
   * If $k < K - 1$, declare the inter-level dependency edge: $T'_{k+1}.\text{precede}(T'_k)$.
4. Submit the graph to `tf::Executor::run()` and wait for completion.

---

### 3.4 Parallel Slack Evaluation
Unlike the sweeps, slack evaluation has **zero inter-node dependencies**:
$$\text{Slack}_{\text{setup}}(v) = RAT_{\text{late}}(v) - AT_{\text{late}}(v)$$
$$\text{Slack}_{\text{hold}}(v) = AT_{\text{early}}(v) - RAT_{\text{early}}(v)$$
* Execute as a single unconstrained parallel-for across the contiguous index range $[0, |V|)$.

---

## 4. Chunking Strategies & Cache Line Hygiene

### 4.1 False Sharing Mitigation
Parallel threads writing to adjacent slots in floating-point arrays can invalidate CPU L1/L2 cache lines if indices share the same 64-byte cache boundary (False Sharing).

```
Cache Line (64 Bytes = 16 floats)
[ f0 | f1 | f2 | f3 | f4 | f5 | f6 | f7 | f8 | f9 | f10 | f11 | f12 | f13 | f14 | f15 ]
 <----------- Thread 0 writes -----------> <----------- Thread 1 writes ----------->
```

#### Mitigation Invariant:
* Taskflow's chunking partitioner must assign contiguous chunks with a minimum block size of 16 or 32 nodes whenever level width exceeds thread count. This ensures worker threads write to separate physical cache lines for the vast majority of operations.

### 4.2 Handling Non-Uniform Levels
In real netlists, level widths vary widely:
* **Narrow Levels (e.g., Level 1 with 4 nodes):** The parallelization overhead (scheduling tasks) exceeds compute time.
* **Wide Levels (e.g., Level 15 with 1,500 nodes):** Substantial work; ideal for dynamic work-stealing across all threads.

#### Adaptive Chunking Policy:
* If $(\text{end\_idx} - \text{start\_idx}) < \text{THRESHOLD}$ (e.g., 64 nodes), execute the level sequentially on the current thread without dispatching to the Taskflow worker queue.
* Otherwise, dispatch to Taskflow's work-stealing parallel loop.

---

## 5. Benchmarking & Scalability Methodology

### 5.1 Harness Integration
Create a dedicated benchmark target `benchmarks/bench_parallel_sta.cpp` linked against **Google Benchmark** and **Taskflow**.

### 5.2 Test Circuits & Workload Characterization
1. **`c17` (11 pins) & `c432` (160 gates):** Used to measure and document scheduling overhead (Amdahl's law baseline; parallel speedup will be $< 1.0\times$ due to thread dispatch cost).
2. **`c6288` (16-bit multiplier, 2,406 gates, deep logic, 124,000+ paths):** Used to evaluate deep level pipeline scaling.
3. **`c7552` (3,512 gates, wide parallel logic):** Used to evaluate wide level throughput scaling across high core counts.

### 5.3 Scaling Metrics
For thread counts $N \in \{1, 2, 4, 8, 16\}$:
* **Speedup ($S_N$):**
  $$S_N = \frac{T_1}{T_N}$$
  *(where $T_1$ is the single-threaded baseline runtime, and $T_N$ is the multi-threaded runtime on $N$ threads).*
* **Parallel Efficiency ($E_N$):**
  $$E_N = \frac{S_N}{N} \times 100\%$$

---

## 6. Verification & Acceptance Criteria

Milestone 3 is complete when all the following criteria are met:

### 6.1 Numerical Equivalence Assertion
* Execute parallel STA on `c6288.bench` across 1, 2, 4, and 8 threads.
* Compare resulting arrays (`at_late`, `at_early`, `rat_late`, `rat_early`, `slack_setup`, `slack_hold`) against the single-threaded Milestone 2 golden output.
* **Pass Criterion:** Exactly $0.0$ floating-point difference across all $|V|$ pins (bit-exact match).

### 6.2 Concurrency Audit (ThreadSanitizer)
* Compile the entire engine and benchmark harness with `-fsanitize=thread` (TSan).
* Run full forward and backward parallel sweeps on `c7552.bench`.
* **Pass Criterion:** Zero data races, zero deadlocks, and zero thread leaks reported by TSan.

### 6.3 Performance Scaling Target
* On a multi-core machine (at least 4 physical cores / 8 threads):
  * **`c6288` / `c7552`:**
    * $\ge \mathbf{2.5\times}$ speedup on 4 threads over single-threaded execution.
    * $\ge \mathbf{4.0\times}$ speedup on 8 threads over single-threaded execution.
* Document runtime plots ($T$ vs. $N$) and efficiency tables in the repository README.