# Specification Document: Milestone 1
## Timing DAG Construction & Topological Levelization

* **Target Target:** `sta-engine` / `vlsi-sta-engine`
* **Status:** Ready for Implementation
* **Scope:** Netlist ingestion, Bidirectional CSR Timing DAG, Graph Query Interfaces, Kahn's Topological Sort, and Level Partitioning.
* **Excluded:** Timing calculations ($AT$, $RAT$, Slack), Concurrency, Incremental Updates, and Optimization.

---

## 1. Milestone Objectives

1. Parse ISCAS85 `.bench` netlists into an internal representation without data loss.
2. Construct a **Pin-Centric Directed Acyclic Graph (DAG)** using contiguous, non-allocating **Bidirectional Compressed Sparse Row (Bidir-CSR)** memory layouts.
3. Model **Cell Arcs** (gate inputs to gate outputs) and **Net Arcs** (driver pin to load pins) with discrete min/max physical delay properties.
4. Correctly resolve multi-fanout nets ($1 \to N$) without node duplication.
5. Compute the topological order and topological level partitions via **Kahn's Algorithm**.
6. Provide zero-allocation query interfaces to inspect neighbors and level slices.
7. Verify all structural properties and invariants against the golden reference netlist `c17.bench`.

---

## 2. Graph Data Model & Core Abstractions

### 2.1 Pin-Centric Representation
The graph does not treat gates as monolithic nodes. Instead, every vertex represents a physical **Pin** or **Primary Port**.

* **Vertices ($V$):**
  * `PrimaryInput`: Circuit inputs (sources, in-degree = 0).
  * `PrimaryOutput`: Circuit outputs (sinks, out-degree = 0).
  * `CellInput`: Gate input pins (e.g., pin $A$ or pin $B$ of a NAND gate).
  * `CellOutput`: Gate output pins (e.g., pin $Y$ of a NAND gate).

* **Edges ($E$ - Timing Arcs):**
  * `CellArc`: Internal to a gate; connects a `CellInput` pin to that gate's `CellOutput` pin.
  * `NetArc`: Interconnect wire; connects a driver pin (`PrimaryInput` or `CellOutput`) to a receiver pin (`CellInput` or `PrimaryOutput`).

---

### 2.2 Bidirectional Compressed Sparse Row (Bidir-CSR) Layout

To ensure cache locality and allow zero-allocation traversals in both downstream and upstream directions, all connectivity is stored using two paired CSR structures.

#### Layout Schemas
1. **Forward CSR (Successors / Downstream):**
   * `fwd_offsets`: Array of size $|V| + 1$. Slot $i$ holds the starting index in `fwd_arcs` for node $i$.
   * `fwd_arcs`: Contiguous array of directed arc IDs leaving each node.
2. **Backward CSR (Predecessors / Upstream):**
   * `bwd_offsets`: Array of size $|V| + 1$. Slot $i$ holds the starting index in `bwd_arcs` for node $i$.
   * `bwd_arcs`: Contiguous array of directed arc IDs entering each node.
3. **Arc Storage (Structure-of-Arrays):**
   * Parallel flat arrays storing: source pin ID, destination pin ID, arc type (Cell or Net), early delay ($d_{\min}$), and late delay ($d_{\max}$).
4. **Node Storage:**
   * Flat array storing pin types (`PrimaryInput`, `PrimaryOutput`, `CellInput`, `CellOutput`).
   * Flat array of pin name strings (for human-readable tracing and debugging).

---

## 3. Netlist Parsing & Ingestion Specification

### 3.1 Input Format: ISCAS85 `.bench`
The parser reads standard ASCII `.bench` format:
* `#`: Comment line (ignore until end of line).
* `INPUT(net_name)`: Declares a primary input.
* `OUTPUT(net_name)`: Declares a primary output.
* `out_net = GATE(in_net1, in_net2, ...)`: Declares a logic gate. Standard gates include `AND`, `NAND`, `OR`, `NOR`, `NOT`, `BUFF`, `XOR`, `XNOR`.

---

### 3.2 Symbol Table & Fanout Handling
Net names in `.bench` files are sparse strings/numbers and must not be used as array indices. 

The parser maintains a temporary mapping table during file ingest:
* **Key:** `net_name` (`std::string`)
* **Value:** 
  * `driver_pin`: A single node ID driving this net (initialized to unset).
  * `load_pins`: A dynamic list of node IDs that receive this net.

---

### 3.3 Two-Phase Graph Construction Algorithm

```
Step 1: Parse File Line-by-Line
  ├── Case: INPUT(name)
  │     1. Create PrimaryInput pin node.
  │     2. Set node as driver_pin in NetTable[name].
  │
  ├── Case: OUTPUT(name)
  │     1. Create PrimaryOutput pin node.
  │     2. Append node to load_pins in NetTable[name].
  │
  └── Case: out_net = GATE(in_net_1, ..., in_net_k)
        1. Create one CellInput pin node for each input argument.
        2. Create one CellOutput pin node for out_net.
        3. Add Cell Arcs immediately: each CellInput -> CellOutput.
        4. Set CellOutput as driver_pin in NetTable[out_net].
        5. For each input argument, append the corresponding CellInput 
           pin to NetTable[in_net_i].

Step 2: Net Interconnect Assembly (Stitching)
  └── For each net in NetTable:
        Validate driver_pin is set (error if dangling net).
        For each load_pin in net.load_pins:
          Add Net Arc: driver_pin -> load_pin (with net delay).

Step 3: Graph Finalization (finalize())
  1. Allocate CSR offset buffers of size |V| + 1.
  2. Compute forward out-degrees and backward in-degrees.
  3. Prefix-sum degrees into fwd_offsets and bwd_offsets.
  4. Populate fwd_arcs and bwd_arcs contiguously.
  5. Clear temporary builder arrays and drop NetTable.
  6. Execute Kahn's algorithm to compute topological levels.
```

---

### 3.4 Delay Configuration Model
Because `.bench` files contain only connectivity, delays are assigned via a static configuration structure passed to the parser:
* **Cell Delays:** Assigned per gate primitive (e.g., `NAND`: early = 1.6, late = 2.0; `INV`: early = 0.8, late = 1.0).
* **Net Delays:** Assigned a nominal default interconnect delay (e.g., early = 0.3, late = 0.5) or zero delay for unit tests.

---

## 4. Graph Traversal Interface Specification

All query methods must be **non-allocating**, returning lightweight views (`std::span`) into contiguous internal storage.

### Required Query Capabilities:
1. **Node Degree Queries:**
   * `fanout_degree(NodeId u)`: Number of outgoing arcs from node $u$.
   * `fanin_degree(NodeId u)`: Number of incoming arcs to node $u$.
2. **Arc Traversal Queries:**
   * `fanout_arcs(NodeId u)`: Non-allocating view of all directed arc IDs leaving node $u$.
   * `fanin_arcs(NodeId u)`: Non-allocating view of all directed arc IDs entering node $u$.
3. **Arc Accessors:**
   * Given an `ArcId`, inspect: `src_node`, `dst_node`, `arc_type`, `delay_early`, and `delay_late`.
   * Mutate delays: `set_arc_delay(ArcId, float early, float late)`.
4. **Node Attribute Accessors:**
   * Given a `NodeId`, inspect: `pin_type` and `pin_name`.
5. **Level Queries:**
   * `num_levels()`: Total count of topological levels.
   * `level_nodes(Level lvl)`: Non-allocating view of all node IDs belonging to level `lvl`.
   * `topo_order()`: Non-allocating view of all node IDs in complete topological sequence.

---

## 5. Topological Sort & Levelization Specification

### 5.1 Kahn's Algorithm with Level Assignment
Levelization must run during `finalize()` to partition nodes into independent horizontal slices.

#### Algorithmic Invariants:
1. **Acyclic Verification:** If the total count of processed nodes does not equal $|V|$, the graph contains a cycle. The engine must throw an exception or return an error state.
2. **Level Invariant:** 
   $$\text{level}(v) = \begin{cases} 
    0 & \text{if } \text{in-degree}(v) = 0 \\ 
    \max_{u \in \text{pred}(v)}(\text{level}(u)) + 1 & \text{otherwise} 
    \end{cases}$$
3. **Monotonic Order:** In the final topological array, every node in level $L_k$ must precede all nodes in level $L_{k+1}$.

#### Operational Steps:
1. Initialize an `in_degrees` array tracking incoming edges per node.
2. Initialize a `levels` array of size $|V|$ to 0.
3. Seed a processing queue with all nodes where `in_degree == 0` (Level 0 nodes: Primary Inputs).
4. While the queue is not empty:
   * Pop node $u$.
   * Append $u$ to the provisional topological list.
   * For each arc $(u \to v)$ in `fanout_arcs(u)`:
     * Update $\text{level}(v) = \max(\text{level}(v), \, \text{level}(u) + 1)$.
     * Decrement `in_degrees[v]`.
     * If `in_degrees[v] == 0`, push $v$ into the queue.
5. Verify `provisional_list.size() == |V|`.
6. Sort nodes into contiguous topological buckets ordered by level:
   * Construct `topo_order`: Array storing all nodes grouped by level.
   * Construct `level_offsets`: Array of size $\text{num\_levels} + 1$, where slot $k$ marks the starting offset of level $k$ in `topo_order`.

---

## 6. Verification & Acceptance Criteria

Milestone 1 is complete when all the following unit tests pass under ASan and UBSan:

### 6.1 Reference Netlist Verification: `c17.bench`
Using the standard 6-gate ISCAS85 benchmark `c17`:
```text
INPUT(1), INPUT(2), INPUT(3), INPUT(6), INPUT(7)
OUTPUT(22), OUTPUT(23)
10 = NAND(1, 3)
11 = NAND(3, 6)
16 = NAND(2, 11)
19 = NAND(11, 7)
22 = NAND(10, 16)
23 = NAND(16, 19)
```

#### Exact Metric Assertions:
* **Pin/Node Count ($|V|$):** Exactly 25 pins
  * 5 Primary Inputs
  * 2 Primary Outputs
  * 6 Gates $\times$ 2 Input Pins each = 12 Cell Inputs
  * 6 Gates $\times$ 1 Output Pin each = 6 Cell Outputs
* **Arc Count ($|E|$):** Exactly 20 timing arcs
  * 6 Gates $\times$ 2 Cell Arcs each = 12 Cell Arcs
  * Net connections = 8 Net Arcs (including fanouts on nets 3, 11, and 16)
* **Topological Level Count:** Exactly 7 levels (Level 0 through Level 6)
  * `Level 0`: 5 nodes (Primary Inputs 1, 2, 3, 6, 7)
  * `Level 6`: 2 nodes (Primary Outputs 22, 23)

### 6.2 Structural Invariant Tests
1. **Topological Monotonicity:** For every arc $e = (u \to v)$ in the graph:
   $$\text{position\_in\_topo\_order}(u) < \text{position\_in\_topo\_order}(v)$$
2. **CSR Symmetry:** For every node $u$ and arc $a \in \text{fanout\_arcs}(u)$ pointing to $v$, arc $a$ must appear in $\text{fanin\_arcs}(v)$.
3. **Cycle Rejection:** Constructing a synthetic graph containing a cycle ($A \to B \to C \to A$) must fail Kahn's algorithm and report an error.
4. **Memory Hygiene:** Zero memory leaks, zero uninitialized variables, and zero out-of-bounds accesses detected by AddressSanitizer and UBSan.
