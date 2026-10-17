
# Specification Document: Milestone 2
## Dual-Mode Batch Sweeps & Event-Driven Incremental STA

* **Target:** `sta-engine` / `vlsi-sta-engine`
* **Status:** Ready for Implementation
* **Scope:** Dual-Mode (Setup + Hold) Batch Sweeps, Critical Path Tracing, and Event-Driven Incremental Timing Updates with $\epsilon$-Pruning.
* **Excluded:** Taskflow Concurrency and Zero-Slack Optimization (ZSA).

---

## 1. Milestone Objectives

1. Implement the **`StaEngine`** state manager operating on a frozen, read-only `const TimingGraph&`.
2. Implement **Batch Dual-Mode Propagation**:
   * Forward Late/Early sweeps for Arrival Times ($AT_{\text{late}}, AT_{\text{early}}$).
   * Backward Late/Early sweeps for Required Arrival Times ($RAT_{\text{late}}, RAT_{\text{early}}$).
   * Setup Slack, Hold Slack, Worst Negative Slack (WNS), and Total Negative Slack (TNS).
3. Implement deterministic **Critical Path Tracing** from worst endpoints back to startpoints.
4. Implement **Event-Driven Incremental Timing Analysis**:
   * Directed propagation through Transitive Fanout (TFO) for $AT$ and Transitive Fanin (TFI) for $RAT$.
   * Monotonically ordered topological priority queues preventing out-of-order recomputations.
   * Floating-point early termination ($\epsilon$-pruning).
5. Verify numerical equivalence: Incremental STA must produce bit-exact (within float tolerance) results compared to full Batch STA on `c17.bench` and `c432.bench`.

---

## 2. Memory Architecture & Timing State Storage

The `StaEngine` owns contiguous numerical state buffers sized exactly to $|V|$ (number of pins). Traversals read and write directly to these pre-allocated buffers.

### 2.1 State Buffer Schema
All buffers are indexed directly by `NodeId`:
* `at_late`: Size $|V|$, Late Arrival Time.
* `at_early`: Size $|V|$, Early Arrival Time.
* `rat_late`: Size $|V|$, Late Required Arrival Time.
* `rat_early`: Size $|V|$, Early Required Arrival Time.
* `slack_setup`: Size $|V|$, Setup Slack margin.
* `slack_hold`: Size $|V|$, Hold Slack margin.

### 2.2 Numerical Sentinels & Tolerances
To ensure correct boundary behavior in $\min$ and $\max$ operations:
* Positive Infinity: $+\infty$ (`std::numeric_limits<float>::infinity()`)
* Negative Infinity: $-\infty$ (`-std::numeric_limits<float>::infinity()`)
* Convergence Tolerance: $\epsilon = 10^{-5}\text{f}$

---

## 3. Batch Timing Engine Specification

Batch sweeps evaluate all nodes level-by-level using the level partitions ($L_0, \dots, L_{K-1}$) generated in Milestone 1.

```
       [ Level 0 ] ===> [ Level 1 ] ===> ... ===> [ Level K-1 ]
Forward Sweep (Ascending Levels):
  AT_late  uses max over fanin
  AT_early uses min over fanin

       [ Level 0 ] <=== [ Level 1 ] <=== ... <=== [ Level K-1 ]
Backward Sweep (Descending Levels):
  RAT_late  uses min over fanout
  RAT_early uses max over fanout
```

---

### 3.1 Timing Constraints Specification
A configuration structure supplied to `StaEngine`:
* $T_{\text{period}}$: Clock period duration.
* $T_{\text{setup}}$: Capture setup time requirement.
* $T_{\text{hold}}$: Capture hold time requirement.
* $T_{\text{launch\_clk}}$: Launch clock latency (default = $0.0$).
* $T_{\text{capture\_clk}}$: Capture clock latency (default = $0.0$).

---

### 3.2 Forward Sweep (Arrival Time Propagation)

#### Initialization (Level $0$ / Sources):
For each node $s \in L_0$ (Primary Inputs and Register $Q$ pins):
$$AT_{\text{late}}(s) = T_{\text{launch\_clk}}$$
$$AT_{\text{early}}(s) = T_{\text{launch\_clk}}$$

#### Propagation Order:
Iterate sequentially through levels in **ascending order**: $\text{lvl} = 1 \to K - 1$.

#### Relaxation Operator (Pull Model):
For each node $v \in L_{\text{lvl}}$:
$$AT_{\text{late}}(v) = \max_{a \in \text{fanin\_arcs}(v)} \big( AT_{\text{late}}(\text{src}(a)) + d_{\max}(a) \big)$$
$$AT_{\text{early}}(v) = \min_{a \in \text{fanin\_arcs}(v)} \big( AT_{\text{early}}(\text{src}(a)) + d_{\min}(a) \big)$$

---

### 3.3 Backward Sweep (Required Arrival Time Propagation)

#### Endpoint Boundary Seeding (Sinks):
For each timing sink $t \in V$ (Primary Outputs and Register $D$ pins):
$$RAT_{\text{late}}(t) = T_{\text{period}} + T_{\text{capture\_clk}} - T_{\text{setup}}$$
$$RAT_{\text{early}}(t) = T_{\text{capture\_clk}} + T_{\text{hold}}$$

#### Propagation Order:
Iterate sequentially through levels in **descending order**: $\text{lvl} = K - 2 \text{ down to } 0$.

#### Relaxation Operator (Pull Model):
For each node $u \in L_{\text{lvl}}$ that is not a timing sink:
$$RAT_{\text{late}}(u) = \min_{a \in \text{fanout\_arcs}(u)} \big( RAT_{\text{late}}(\text{dst}(a)) - d_{\max}(a) \big)$$
$$RAT_{\text{early}}(u) = \max_{a \in \text{fanout\_arcs}(u)} \big( RAT_{\text{early}}(\text{dst}(a)) - d_{\min}(a) \big)$$

---

### 3.4 Slack & Summary Metrics

#### Per-Node Slack Evaluation:
$$\text{Slack}_{\text{setup}}(v) = RAT_{\text{late}}(v) - AT_{\text{late}}(v)$$
$$\text{Slack}_{\text{hold}}(v) = AT_{\text{early}}(v) - RAT_{\text{early}}(v)$$

#### Design Summary Metrics:
* **Worst Negative Slack (WNS):**
  $$\text{WNS}_{\text{setup}} = \min_{v \in \text{Sinks}} \big( \min(0.0, \, \text{Slack}_{\text{setup}}(v)) \big)$$
  $$\text{WNS}_{\text{hold}} = \min_{v \in \text{Sinks}} \big( \min(0.0, \, \text{Slack}_{\text{hold}}(v)) \big)$$
* **Total Negative Slack (TNS):**
  $$\text{TNS}_{\text{setup}} = \sum_{v \in \text{Sinks}, \, \text{Slack}_{\text{setup}}(v) < 0} \text{Slack}_{\text{setup}}(v)$$
  $$\text{TNS}_{\text{hold}} = \sum_{v \in \text{Sinks}, \, \text{Slack}_{\text{hold}}(v) < 0} \text{Slack}_{\text{hold}}(v)$$

---

### 3.5 Critical Path Tracing

To extract the most critical path for reporting:
1. Identify the sink $t^* = \arg\min_{t \in \text{Sinks}} \text{Slack}_{\text{setup}}(t)$.
2. Set $curr = t^*$. Append $curr$ to the path buffer.
3. While $curr$ is not a timing source (in-degree $> 0$):
   * Inspect all arcs $a \in \text{fanin\_arcs}(curr)$.
   * Select the predecessor arc $a^*$ that caused the critical arrival time:
     $$a^* = \arg\max_{a \in \text{fanin\_arcs}(curr)} \big( AT_{\text{late}}(\text{src}(a)) + d_{\max}(a) \big)$$
   * Set $curr = \text{src}(a^*)$. Append $curr$ to the path buffer.
4. Reverse the path buffer to output the ordered path from source to sink.

---

## 4. Event-Driven Incremental Engine Specification

When arc delays change (e.g., cell sizing or wire routing), only the transitive downstream fanout (TFO) and upstream fanin (TFI) cones are recomputed.

```
       [ Upstream TFI Cone ]
                 ^
                 | (Backward Priority Queue: Descending Topo Index)
         [ Modified Arc / Node ]
                 |
                 v (Forward Priority Queue: Ascending Topo Index)
       [ Downstream TFO Cone ]
```

---

### 4.1 Monotonic Priority Queue Invariant
To ensure each node is evaluated at most once during an incremental pass, queue ordering is tied strictly to the **topological sort index** (`topo_index(v)` $\in [0, |V|-1]$):

* **Forward Queue ($Q_{\text{fwd}}$):** Min-heap ordered by `topo_index` (lowest index first).
  * *Invariant:* When node $v$ is popped, all predecessors of $v$ have already settled their new arrival times.
* **Backward Queue ($Q_{\text{bwd}}$):** Max-heap ordered by `topo_index` (highest index first).
  * *Invariant:* When node $u$ is popped, all successors of $u$ have already settled their new required arrival times.

---

### 4.2 Duplicate Suppression & State Tracking
To prevent redundant queue allocations:
* Maintain a pre-allocated flat byte array: `in_queue` of size $|V|$.
* Set `in_queue[v] = 1` when pushing to a priority queue.
* Reset `in_queue[v] = 0` when popping from the priority queue.
* Check `if (!in_queue[w])` before pushing any neighbor $w$.

---

### 4.3 Incremental Propagation Algorithm

#### Step 1: Arc Delay Mutation
An external call updates an arc delay:
`set_arc_delay(arc_id, new_early, new_late)`

#### Step 2: Forward Sweep ($AT$ Update in TFO)
1. Initialize $Q_{\text{fwd}}$ and seed it with $\text{dst}(\text{arc\_id})$. Set `in_queue[dst] = 1`.
2. Initialize an empty tracking list: `affected_endpoints`.
3. While $Q_{\text{fwd}}$ is not empty:
   * Pop node $v$ with the minimum `topo_index`. Set `in_queue[v] = 0`.
   * Recompute $AT_{\text{new\_late}}(v)$ and $AT_{\text{new\_early}}(v)$ using the standard relaxation formulas over `fanin_arcs(v)`.
   * **$\epsilon$-Pruning Check:**
     $$\Delta_{\text{late}} = |AT_{\text{new\_late}}(v) - AT_{\text{late}}(v)|$$
     $$\Delta_{\text{early}} = |AT_{\text{new\_early}}(v) - AT_{\text{early}}(v)|$$
     If $\Delta_{\text{late}} < \epsilon$ and $\Delta_{\text{early}} < \epsilon$:
     * Do **not** propagate to successors. Continue to next queue item.
   * **Update & Propagate:**
     * Overwrite stored values: $AT_{\text{late}}(v) \leftarrow AT_{\text{new\_late}}(v)$, $AT_{\text{early}}(v) \leftarrow AT_{\text{new\_early}}(v)$.
     * If $v$ is a timing sink, append $v$ to `affected_endpoints`.
     * For each arc $a \in \text{fanout\_arcs}(v)$: let $w = \text{dst}(a)$. If `!in_queue[w]`, push $w$ to $Q_{\text{fwd}}$ and mark `in_queue[w] = 1`.

#### Step 3: Backward Sweep ($RAT$ Update in TFI)
1. Initialize $Q_{\text{bwd}}$ and seed it with:
   * $\text{src}(\text{arc\_id})$ (the driver of the modified delay).
   * All sinks in `affected_endpoints`.
2. While $Q_{\text{bwd}}$ is not empty:
   * Pop node $u$ with the maximum `topo_index`. Set `in_queue[u] = 0`.
   * If $u$ is not a timing sink:
     * Recompute $RAT_{\text{new\_late}}(u)$ and $RAT_{\text{new\_early}}(u)$ over `fanout_arcs(u)`.
     * **$\epsilon$-Pruning Check:**
       $$\Delta_{\text{late}} = |RAT_{\text{new\_late}}(u) - RAT_{\text{late}}(u)|$$
       $$\Delta_{\text{early}} = |RAT_{\text{new\_early}}(u) - RAT_{\text{early}}(u)|$$
       If $\Delta_{\text{late}} < \epsilon$ and $\Delta_{\text{early}} < \epsilon$:
       * Do **not** propagate to predecessors. Continue to next queue item.
     * **Update & Propagate:**
       * Overwrite stored values: $RAT_{\text{late}}(u) \leftarrow RAT_{\text{new\_late}}(u)$, $RAT_{\text{early}}(u) \leftarrow RAT_{\text{new\_early}}(u)$.
   * For each arc $a \in \text{fanin\_arcs}(u)$: let $p = \text{src}(a)$. If `!in_queue[p]`, push $p$ to $Q_{\text{bwd}}$ and mark `in_queue[p] = 1`.

#### Step 4: Localized Slack Update
Recompute $\text{Slack}_{\text{setup}}(x)$ and $\text{Slack}_{\text{hold}}(x)$ only for nodes touched during Steps 2 and 3.

---

## 5. Stateless Algorithmic Kernels (Functional Core)

To support Test-Driven Development (TDD), core mathematical operations must be implemented as stateless functions that accept explicit buffers:

### 5.1 Kernel Signatures Specification
1. **`relax_at(v, in_arcs, arc_srcs, arc_delays, at_in) -> std::pair<float, float>`**
   * Computes Late and Early arrival times for pin $v$ given input arrival times.
2. **`relax_rat(u, out_arcs, arc_dsts, arc_delays, rat_in) -> std::pair<float, float>`**
   * Computes Late and Early required arrival times for pin $u$ given downstream required times.
3. **`compute_slack(at, rat) -> float`**
   * Computes Setup slack ($RAT - AT$) or Hold slack ($AT - RAT$).
4. **`is_converged(val_old, val_new, epsilon) -> bool`**
   * Checks if $|val_{\text{new}} - val_{\text{old}}| < \epsilon$.

---

## 6. Verification & Acceptance Criteria

Milestone 2 is complete when all the following test suites pass under ASan and UBSan:

### 6.1 Golden Reference Batch Verification (`c17.bench`)
Setup: unit cell delays ($d_{\text{cell\_late}} = 1.0, d_{\text{cell\_early}} = 0.8$), zero net delays ($d_{\text{net}} = 0.0$), $T_{\text{period}} = 5.0$, $T_{\text{setup}} = 0.2$, $T_{\text{hold}} = 0.1$.

#### Required Numeric Assertions:
* **Arrival Times:**
  * Primary Inputs (1, 2, 3, 6, 7): $AT_{\text{late}} = 0.0$, $AT_{\text{early}} = 0.0$
  * Output Gate 22 output pin: $AT_{\text{late}} = 3.0$, $AT_{\text{early}} = 2.4$
  * Output Gate 23 output pin: $AT_{\text{late}} = 3.0$, $AT_{\text{early}} = 2.4$
* **Required Times at Sinks (Outputs 22 and 23):**
  * $RAT_{\text{late}} = 5.0 - 0.2 = 4.8$
  * $RAT_{\text{early}} = 0.0 + 0.1 = 0.1$
* **Slacks at Sinks (Outputs 22 and 23):**
  * $\text{Slack}_{\text{setup}} = 4.8 - 3.0 = +1.8$
  * $\text{Slack}_{\text{hold}} = 2.4 - 0.1 = +2.3$
* **Summary Metrics:**
  * $\text{WNS}_{\text{setup}} = 0.0$ (no violation)
  * Critical Path length: exactly 3 cell stages ($3 \to 11 \to 16 \to 22$ or $3 \to 11 \to 19 \to 23$).

### 6.2 Incremental Equivalence Tests
1. **Bit-Exact Equivalence:**
   * Run full batch STA on `c432.bench`. Record all slacks.
   * Modify the delay of a single arc in the middle of the graph: $d \leftarrow d + 2.5$.
   * Run `update_incremental()`.
   * Run a fresh `update_batch()` on a duplicate graph.
   * **Assert:** For all pins $v \in V$, $|\text{Slack}_{\text{incremental}}(v) - \text{Slack}_{\text{batch}}(v)| < 10^{-4}$.

2. **Early Pruning Test:**
   * Modify an arc delay by an amount smaller than epsilon: $d \leftarrow d + 10^{-6}$.
   * Run `update_incremental()`.
   * **Assert:** Zero successor nodes beyond the immediate arc sink are touched or evaluated.

3. **Performance Scaling Test:**
   * On `c7552.bench`, modify a leaf gate delay.
   * Measure execution time: incremental update must be at least $\mathbf{20\times}$ faster than full batch recomputation.