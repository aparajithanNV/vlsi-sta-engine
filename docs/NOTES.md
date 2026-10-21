# Engineering Notes & Architectural Decision Record (ADR)
**Project:** `sta-engine` / `vlsi-sta-engine`  
**Author:** Aparajithan N. V.  
**Theoretical Foundation:** *VLSI Physical Design: From Graph Partitioning to Timing Closure* (Kahng et al., Ch. 8)  
**Standard:** Modern C++20 / Data-Oriented Design (DOD)

---

## 1. Project Status Overview
- [x] **Milestone 1:** ISCAS85 `.bench` parsing, Pin-Centric Timing DAG, Bidirectional CSR, Kahn's Topological Levelization.
- [x] **Milestone 2:** Dual-Mode Batch Timing Analysis (Setup/Hold), Differential Incremental STA Engine with Priority Queue & $\epsilon$-Pruning.
- [ ] **Milestone 3:** Taskflow Concurrency, Multi-Threaded Levelized Sweeps, Google Benchmark Scaling Analysis.

---

## 2. Key Architectural Decisions (ADR)

### ADR-01: Pin-Centric over Gate-Centric Modeling
* **Decision:** Vertices in the graph represent physical pins (`PI`, `PO`, `INTERNAL`), not monolithic logic gates.
* **Rationale:** A gate-centric model cannot accurately model fanout wire delays ($1 \to N$) where different branching interconnects experience different physical RC delays. In this pin-centric model, a 2-input gate is modeled as 3 pin nodes connected by 2 internal cell arcs, while net fanouts are represented as independent directed wire arcs.
* **Impact:** Different net branches naturally carry independent delay numbers without requiring pin duplication or graph restructuring.

### ADR-02: Bidirectional Compressed Sparse Row (Bidir-CSR)
* **Decision:** Replace pointer-heavy graph representations (`std::list`, `std::vector<Node*>`) with two paired flat index arrays (`offsets` + `arcs`) for forward successors and backward predecessors.
* **Rationale:** Spatial and temporal cache locality. Traversing fanout and fanin arcs returns non-allocating `std::span<const ArcID>` views directly into contiguous vector memory.
* **Impact:** Eliminates dynamic heap allocations in hot traversal loops; minimizes L1/L2 data cache misses.

### ADR-03: Functional Core, Imperative Shell (Decoupled Timing State)
* **Decision:** Separate circuit topology (`TimingGraph`) from computational timing state (`STAState`).
* **Rationale:** Analysis functions (`calculate_slack_full_pass_late`, `calculate_incremental_AAT`, etc.) operate as stateless/semi-pure algorithms taking `const TimingGraph&` and `STAState&`.
* **Testing Impact:** Enables **Differential Testing** in GoogleTest. Incremental updates are verified by taking a baseline snapshot, running incremental updates, running a full batch pass from scratch into a separate golden state, and asserting bit-exact equivalence across all pins.

---

## 3. Algorithmic Discoveries & Invariants

### 1. Inherent Level Grouping in Kahn's FIFO Queue
* **Finding:** When running Kahn's algorithm using a standard FIFO queue (`std::queue<uint32_t>`), the popped output list is **naturally partitioned level-by-level**.
* **Proof:**
    1. All Level 0 nodes (in-degree = 0) are enqueued initially.
    2. A node $v$ enters the queue if and only if its in-degree reaches 0 (when its last predecessor is popped).
    3. The highest-level predecessor of node $v$ is at Level $\text{level}(v) - 1$.
    4. Therefore, node $v$ is pushed to the back of the queue strictly during the processing of Level $\text{level}(v) - 1$ and pops during the processing of Level $\text{level}(v)$.
* **Result:** No vector repacking or secondary sorting is needed. `find_pins_in_level(lvl)` directly slices contiguous blocks of `topological_levels`.

### 2. Priority Queue Ordering for Incremental STA
* **Forward Propagation (AAT):** Requires a **Min-Heap** (`CompareLevelsMin`, sorting with `>`). Nodes are popped in ascending topological level order, ensuring that all upstream inputs to a reconvergent gate settle before the gate evaluates.
* **Backward Propagation (RAT):** Requires a **Max-Heap** (`CompareLevelsMax`, sorting with `<`). Nodes must be popped in descending topological level order so downstream required arrival times pull back fully before upstream pins evaluate.
* **$\epsilon$-Pruning:** Changes smaller than $\Delta = 10^{-5}\text{f}$ are pruned immediately, preventing unnecessary propagation down non-critical branches.

---

## 4. Empirical Benchmark Performance
* **Environment:** Intel i712650H, Linux x86_64, Clang 16 (`-O2 -g -DNDEBUG`)
* **Benchmark Harness:** Google Benchmark (`profile` preset)

```text
--------------------------------------------------------------------------
Benchmark                                Time             CPU   Iterations
--------------------------------------------------------------------------
BENCHMARK_STA_SEQUENTIAL/c17         0.279 us        0.278 us      2515198
BENCHMARK_STA_SEQUENTIAL/c432         5.98 us         5.97 us       117243
BENCHMARK_STA_SEQUENTIAL/c880         12.0 us         12.0 us        54727
BENCHMARK_STA_SEQUENTIAL/c3540        46.1 us         46.0 us        15198
BENCHMARK_STA_SEQUENTIAL/c6288        71.2 us         71.0 us         9451
BENCHMARK_STA_SEQUENTIAL/c7552         101 us          101 us         6984
BENCHMARK_PARALLEL_STA_C7552/1         159 us         20.0 us        36701
BENCHMARK_PARALLEL_STA_C7552/2         138 us         19.2 us        35620
BENCHMARK_PARALLEL_STA_C7552/4         241 us         20.8 us        33940
BENCHMARK_PARALLEL_STA_C7552/6         352 us         23.9 us        10000
BENCHMARK_PARALLEL_STA_C7552/8         487 us         24.5 us        10000
BENCHMARK_PARALLEL_STA_C7552/12        772 us         25.7 us        10000
BENCHMARK_PARALLEL_STA_C7552/16       1051 us         24.4 us        10000
```

### Analysis & Performance Remarks

#### 1. Sequential Complexity & Cache Locality ($O(|V| + |E|)$ Linearity)
The sequential benchmark progression across the six ISCAS85 circuits demonstrates near-perfect empirical linearity:

| Circuit | Gate Count | Topo Levels | Sequential Time | Normalized Time / Gate |
| :--- | :--- | :--- | :--- | :--- |
| **`c17`** | 6 | 7 | **0.28 µs** | 46.5 ns / gate |
| **`c432`** | 160 | 17 | **5.98 µs** | 37.4 ns / gate |
| **`c880`** | 383 | 24 | **12.0 µs** | 31.3 ns / gate |
| **`c3540`** | 1,669 | 47 | **46.1 µs** | 27.6 ns / gate |
| **`c6288`** | 2,406 | 124 | **71.2 µs** | 29.5 ns / gate |
| **`c7552`** | 3,512 | 40 | **101.0 µs** | 28.7 ns / gate |

* **Constant Amortized Cost per Gate:** As the design scales from 160 gates to 3,512 gates ($22\times$ increase in logic), execution time remains remarkably bounded between **27 ns and 37 ns per gate**.
* **Cache Efficiency:** Even on `c6288` (which features an extreme topological depth of 124 levels and over 124,000 paths), per-gate compute time does not degrade. This confirms that the Bidirectional Compressed Sparse Row (Bidir-CSR) memory layout successfully avoids L1/L2 cache thrashing and preserves linear graph traversal complexity.

---

#### 2. Multi-Threaded Taskflow Analysis on `c7552` (The Granularity Threshold)
The multi-core benchmark reveals the classical systems tradeoff between **workload granularity** and **barrier synchronization latency**:

* **Initial Parallel Gain (1 → 2 Threads):**
  * Scaling from 1 worker thread to 2 worker threads reduces wall-clock execution time from **159 µs to 138 µs** (~1.15× speedup), confirming that concurrent execution of wide levels and independent AAT/RAT branches functions correctly.
* **Synchronization Bottleneck Beyond 2 Threads:**
  * Scaling from 4 to 16 threads causes wall-clock time to increase monotonically (reaching **1,051 µs at 16 threads**).
  * **The Mathematical Root Cause:**
    * In `c7552`, total sequential execution takes only **101 µs across 40 levels**.
    * Average compute duration per topological level:
      $$\text{Compute per Level} = \frac{101\,\mu\text{s}}{40} \approx \mathbf{2.5\,\mu\text{s}}$$
    * In modern multi-threading runtimes, coordinating worker threads across task barriers incurs a latency of **~1 to 2 µs per barrier**.
    * With 40 forward barriers and 40 backward barriers (80 total barriers per pass), the cumulative barrier latency is:
      $$\text{Barrier Overhead} = 80 \times 1.5\,\mu\text{s} \approx \mathbf{120\,\mu\text{s}}$$
    * When each level contains only **2.5 µs of actual math**, thread dispatch and barrier contention rapidly exceed the computational workload.
* **CPU Time vs. Wall-Clock Time Discrepancy:**
  * At 16 threads, CPU time for the main thread remains low (**24.4 µs**), while wall-clock time reaches **1,051 µs**.
  * This large delta indicates that worker threads are not compute-bound; they spend the vast majority of their CPU cycles spinning, work-stealing, and waiting on barrier condition variables (oversubscription).

---

#### 3. Industrial Implications: Amdahl's Law vs. Gustafson's Law
* **Amdahl's Law (Micro-Circuits):** For circuits under 10,000 gates, single-threaded execution on modern high-IPC CPUs (such as the Golden Cove P-cores @ 4.7 GHz) is optimal due to zero synchronization overhead.
* **Gustafson's Law (Industrial Production Netlists):** In commercial ASIC designs (10 million to 50 million gates), individual topological levels contain **50,000 to 200,000 gates**, requiring tens of milliseconds per level. At that scale, the ~1–2 µs Taskflow barrier represents less than **0.01% overhead**, allowing multi-threaded levelization to achieve near-linear multi-core speedup.