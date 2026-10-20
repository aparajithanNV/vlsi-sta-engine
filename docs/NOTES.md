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

## 4. Empirical Performance Baselines (Sequential Execution)

### Baseline: `c17.bench` (6 gates, 25 pins, 26 arcs)
* **Environment:** Intel i712650H, Linux x86_64, Clang 16 (`-O2 -g -DNDEBUG`)
* **Benchmark Harness:** Google Benchmark (`profile` preset)

```text
--------------------------------------------------------
Benchmark              Time             CPU   Iterations
--------------------------------------------------------
BM_FullPassLate      256 ns          256 ns      2640064
```
### `c432.bench` (40 inverters, 120 gates ( 4 ANDs + 119 NANDs + 19 NORs + 18 XORs ))
Full pass late calculation

### `c7552.bench` (876 inverters, 2636 gates ( 1310 ANDs + 1904 NANDs + 244 ORs + 54 NORs + 534 buffers ))