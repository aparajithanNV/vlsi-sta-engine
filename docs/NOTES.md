# Engineering Notes & Architectural Decision Record (ADR)
**Project:** `sta-engine` / `vlsi-sta-engine`  
**Author:** Aparajithan N. V.  
**Theoretical Foundation:** *VLSI Physical Design: From Graph Partitioning to Timing Closure* (Kahng et al., Ch. 8)  
**Standard:** Modern C++20 / Data-Oriented Design (DOD)

---

## 1. Project Status Overview
- [x] **Milestone 1:** ISCAS85 `.bench` parsing, Pin-Centric Timing DAG, Bidirectional CSR, Kahn's Topological Levelization.
- [x] **Milestone 2:** Dual-Mode Batch Timing Analysis (Setup/Hold), Differential Incremental STA Engine with Priority Queue & $\epsilon$-Pruning.
- [x] **Milestone 3:** Taskflow Concurrency, Multi-Threaded Levelized Sweeps, Google Benchmark Scaling Analysis.

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
* **Environment:** Intel Core i7-12650H (6 P-cores @ 4.7 GHz, 4 E-cores @ 3.5 GHz), Linux Mint x86_64, Clang 16 (`-O3 -DNDEBUG`)
* **Benchmark Harness:** Google Benchmark (`release` preset)

### Sequential vs. Multi-Threaded Results
```text
--------------------------------------------------------------------------
Benchmark                                Time             CPU   Iterations
--------------------------------------------------------------------------
BENCHMARK_STA_SEQUENTIAL/c17         0.256 us        0.256 us      2633665
BENCHMARK_STA_SEQUENTIAL/c432         5.56 us         5.56 us       125950
BENCHMARK_STA_SEQUENTIAL/c880         12.4 us         12.4 us        57230
BENCHMARK_STA_SEQUENTIAL/c3540        51.5 us         51.5 us        10000
BENCHMARK_STA_SEQUENTIAL/c6288        75.4 us         75.4 us         9230
BENCHMARK_STA_SEQUENTIAL/c7552         102 us          102 us         6659
BENCHMARK_PARALLEL_STA_C7552/1         131 us         16.9 us        43568
BENCHMARK_PARALLEL_STA_C7552/2         130 us         18.7 us        37955
BENCHMARK_PARALLEL_STA_C7552/4         228 us         20.0 us        36156
BENCHMARK_PARALLEL_STA_C7552/6         344 us         24.1 us        10000
BENCHMARK_PARALLEL_STA_C7552/8         465 us         23.5 us        10000
BENCHMARK_PARALLEL_STA_C7552/12        766 us         25.4 us        10000
BENCHMARK_PARALLEL_STA_C7552/16       1028 us         24.0 us        10000
```

### Analysis & Performance Remarks

#### 1. Empirical Proof of $O(|V| + |E|)$ Linear Complexity
The sequential scaling characteristics across the ISCAS85 benchmark suite demonstrate uniform performance across varied circuit topologies:

| Circuit | Gate Count | Pin Count ($|V|$) | Pin-Level Topo Levels | Sequential Time | Normalized Time / Pin |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **`c17`** | 6 | 25 | 8 | **0.256 µs** | **10.2 ns / pin** |
| **`c432`** | 160 | 539 | 36 | **5.56 µs** | **10.3 ns / pin** |
| **`c880`** | 383 | 1,198 | 50 | **12.4 µs** | **10.4 ns / pin** |
| **`c3540`** | 1,669 | 4,680 | 96 | **51.5 µs** | **11.0 ns / pin** |
| **`c6288`** | 2,406 | 7,280 | 250 | **75.4 µs** | **10.4 ns / pin** |
| **`c7552`** | 3,512 | 9,971 | 88 | **102.0 µs** | **10.2 ns / pin** |

* **Hardware Cache Efficiency:** As the network grows by $400\times$ (from 25 pins to nearly 10,000 pins), amortized execution time remains fixed at **$\approx 10.3$ ns per pin**.
* **Immunity to Deep Logic Depths:** `c6288` features an extreme topological depth of **250 pin-levels** (a 16-bit multiplier with over 124,000 paths). Despite this depth, the engine maintains 10.4 ns/pin, proving that the Bidirectional Compressed Sparse Row (Bidir-CSR) memory buffers prevent cache-line thrashing and eliminate pointer-chasing latency.

---

#### 2. Multi-Threaded Taskflow Analysis on `c7552` (Barrier Latency vs. Granularity)
The multi-core scaling curve on `c7552` illustrates the synchronization bounds defined by Amdahl's Law on fine-grained workloads:

* **Initial Parallel Speedup (1 → 2 Threads):**
  * Scaling from 1 worker thread to 2 worker threads reduces wall-clock time from **131 µs to 130 µs**, confirming that concurrent execution of wide levels and independent AAT/RAT sweeps functions correctly.
* **Synchronization Bottleneck Beyond 2 Threads:**
  * Scaling across 4 to 16 threads causes wall-clock time to increase monotonically (reaching **1,028 µs at 16 threads**).
  * **The Mathematical Root Cause:**
    * In `c7552`, the graph contains **88 topological levels**, with total sequential compute requiring only **102 µs**.
    * Average compute time per topological level:
      $$\text{Compute per Level} = \frac{102\,\mu\text{s}}{88} \approx \mathbf{1.16\,\mu\text{s}}$$
    * In multi-threaded task runtimes (Taskflow, OpenMP, TBB), synchronizing worker threads across a task barrier incurs a latency of **~1 to 2 µs per barrier**.
    * With 88 forward levels and 88 backward levels (176 total barrier synchronizations per full pass), the cumulative barrier overhead is:
      $$\text{Barrier Overhead} = 176 \times 1.5\,\mu\text{s} \approx \mathbf{264\,\mu\text{s}}$$
    * When each level contains only **1.16 µs of actual math**, thread dispatch, work-stealing contention, and barrier waits exceed the computational payload.
* **CPU Time vs. Wall-Clock Time:**
  * At 16 threads, CPU time for the main thread remains low (**24.0 µs**), while wall-clock time reaches **1,028 µs**.
  * This demonstrates that worker threads are not compute-bound; they spend the majority of their cycles spinning on condition variables and work-stealing queues (oversubscription).

---

#### 3. Industrial Implications: Amdahl's Law vs. Gustafson's Law
* **Amdahl's Law (Micro-Circuits):** For circuits under 10,000 pins where sequential runtime is under $150\,\mu\text{s}$, single-threaded execution on modern high-IPC CPUs (Intel Golden Cove P-cores @ 4.7 GHz) is optimal due to zero synchronization latency.
* **Gustafson's Law (Industrial Netlists):** In commercial ASIC designs (10M to 50M gates), individual topological levels contain **50,000 to 200,000 pins**, requiring tens of milliseconds per level. At that scale, the ~1–2 µs barrier represents less than **0.01% overhead**, enabling level-parallel sweeps to achieve near-linear multi-core speedup.

### Example Timing Report from Unit test

```text
....................................................................................................
CRITICAL PATH TIMING REPORT LATE MODE                                                               
....................................................................................................
Startpoint:                           15
Endpoint:                             14
Slack:                             -0.35
....................................................................................................
            Pin       Arc Type     Incr Delay   Arrival Time  Required Time          Slack
             15         SOURCE              0              0          -0.35          -0.35
              1           WIRE              0              0          -0.35          -0.35
              4           WIRE            0.1            0.1          -0.25          -0.35
              3           GATE              1            1.1           0.75          -0.35
              9           WIRE            0.3            1.4           1.05          -0.35
              8           GATE              2            3.4           3.05          -0.35
             13           WIRE           0.25           3.65            3.3          -0.35
             11           GATE              2           5.65            5.3          -0.35
             14           WIRE            0.2           5.85            5.5          -0.35
....................................................................................................
Data Arrival Time:                            5.85
Data Required Time:                            5.5
Worst Negative Slack:                          -0.35 (TIMING VIOLATION) 
```