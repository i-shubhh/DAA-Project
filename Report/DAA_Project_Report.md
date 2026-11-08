# DAA PROJECT REPORT
# OPTIMAL DELIVERY ROUTE PLANNING FOR E-COMMERCE APPLICATIONS
### A Comparative Study of the Travelling Salesperson Problem Using Branch and Bound and Nearest Neighbor Algorithms

**Institution:** Department of Computer Science & Engineering, SRM University, Andhra Pradesh  
**Course:** Design and Analysis of Algorithms (DAA)  
**Implementation Language:** Pure C (C99 Standard) with Winsock2, SQLite3, cJSON, and Leaflet  
**Student Team:**
- Sugandh - Introduction & Problem Survey (AP25110070346)
- Shubham - Nearest Neighbor Algorithm (AP25110070745)
- Rohit - Execution, Build & System Verification (AP25110070453)
- Mathews - Branch and Bound Algorithm (AP25110070852)
- Bhuvan - Main Application, Routing & Database Management (AP25110070082)

---

## Executive Summary

Last-mile e-commerce package delivery is one of the most cost-intensive logistical operations globally. In urban delivery networks, a delivery fleet departs from a designated central fulfillment warehouse, visits a permutation of customer delivery destinations or regional hubs, and must return to the warehouse with minimum total cost (distance, fuel, and transit time). Because urban roads feature one-way corridors, expressway ramps, traffic congestion, and variable geography, pairwise travel distances are inherently **directed and asymmetric** ($C[i][j] \ne C[j][i]$).

This project presents a complete, production-grade software engineering system written **strictly in standard C (C99)**. It combines a high-performance multithreaded C HTTP backend server, an embedded SQLite database engine, OpenStreetMap Nominatim geocoding, OSRM road-distance calculation, an interactive Leaflet route visualization map, and a dual-interface delivery planner (Web Application and Console Application).

At its computational core, the application evaluates and contrasts two primary Design and Analysis of Algorithms (DAA) paradigms:
1. **Branch and Bound (LCBB with Matrix Reduction)**: An exact combinatorial optimization algorithm mathematically guaranteed to discover the global optimal delivery route.
2. **Nearest Neighbor Heuristic**: A fast greedy approximation algorithm designed for rapid, real-time route generation.

Both algorithms share a unified, validated algorithmic core (`Common/`) ensuring mathematical consistency across both the web dashboard and console application.

---

## 1. Problem Formulation: Travelling Salesperson Problem (TSP)

### 1.1 Mathematical Definition

Let the delivery network be modeled as a directed, weighted complete graph $G = (V, E)$:
- $V = \{v_0, v_1, v_2, \dots, v_{N-1}\}$ represents the set of $N$ locations, where $v_0$ is designated as the primary warehouse or fulfillment center.
- $E = \{(v_i, v_j) \mid v_i, v_j \in V, i \ne j\}$ represents directed road corridors connecting locations.
- $C: E \to \mathbb{R}^+$ represents the non-negative travel cost function, where $C[i][j]$ denotes the actual driving distance in kilometers from location $v_i$ to $v_j$.

### 1.2 Directed / Asymmetric Cost Matrix (ATSP)

In real-world street routing, travel distances are rarely symmetric:
$$C[i][j] \ne C[j][i]$$

This asymmetry arises due to:
- One-way traffic regulations and median barriers in municipal regions.
- Divided highways, cloverleaf interchanges, and U-turn requirements.
- Roadway construction and directional traffic routing.

**Network Constraints:**
1. Self-loops are prohibited: $C[i][i] = \infty$.
2. Disconnected, missing, or impassable road corridors are represented by $C[i][j] = \infty$.
3. All road costs are non-negative: $C[i][j] \ge 0$.

### 1.3 Objective Function

Find an optimal permutation $(p_0, p_1, p_2, \dots, p_{N-1})$ of the location set $V$, with $p_0 = v_{\text{warehouse}}$, such that every destination is visited exactly once, the vehicle returns to $p_0$ ($p_N = p_0$), and the total travel distance $Z$ is minimized:

$$\min Z = \sum_{k=0}^{N-1} C[p_k][p_{k+1}] \quad \text{where } p_N = p_0$$

The Travelling Salesperson Problem is NP-hard. For $N$ locations, the exhaustive brute-force search space comprises $(N-1)!$ possible directed tours, making unguided search impossible for practical fleets ($15! \approx 1.3 \times 10^{12}$ tours; $20! \approx 2.4 \times 10^{18}$ tours).

---

## 2. Algorithm 1: Branch and Bound (Exact Optimal)

### 2.1 State Space Tree Formulation

Branch and Bound explores the solution space by organizing partial tours into a **State Space Tree**:
- **Root Node (Level 0):** Represents the initial state where only the starting warehouse $v_0$ is chosen.
- **Internal Nodes (Level $k$):** Represents a partial delivery path $(p_0, p_1, \dots, p_k)$ consisting of $k$ directed edges.
- **Leaf Nodes (Level $N-1$):** Complete Hamiltonian tours with all $N$ delivery stops visited and the closing return edge $(p_{N-1}, p_0)$ appended.

```
                      [ Root: Start @ Warehouse 0 ] (LB = Initial Reduced Cost)
                             /         \              \
                            /           \              \
                    [0 -> 1]            [0 -> 2]       [0 -> 3]
                   /        \           /      \
               [0->1->2]  [0->1->3]   ...      ...
                  |          |
              [0->1->2->3]  ...
                  |
              [Return to 0]
```

### 2.2 Matrix Reduction Lower Bound Technique

To calculate a tight, admissible lower bound on the cost of any completion of a partial tour, the algorithm employs the **Reduced Cost Matrix** method (Horowitz & Sahni / Little's formulation):

1. **Row Reduction:**  
   For every row $i \in \{0, \dots, N-1\}$, identify the row minimum:
   $$r_i = \min_{j} \{ M[i][j] \mid M[i][j] < \infty \}$$
   If $0 < r_i < \infty$, subtract $r_i$ from every entry in row $i$, and accumulate:
   $$\text{Reduction}_{\text{row}} = \sum_{i=0}^{N-1} r_i$$

2. **Column Reduction:**  
   For every column $j \in \{0, \dots, N-1\}$, identify the column minimum:
   $$c_j = \min_{i} \{ M[i][j] \mid M[i][j] < \infty \}$$
   If $0 < c_j < \infty$, subtract $c_j$ from every entry in column $j$, and accumulate:
   $$\text{Reduction}_{\text{col}} = \sum_{j=0}^{N-1} c_j$$

3. **Total Matrix Reduction:**
   $$\Delta = \text{Reduction}_{\text{row}} + \text{Reduction}_{\text{col}}$$

The initial lower bound at the root node is $LB(\text{Root}) = \Delta(M_{\text{initial}})$.

### 2.3 Branching and Child Node Generation

When transitioning from parent vertex $u$ to an unvisited child vertex $v$:
1. A child state is allocated with partial path extended by $v$.
2. Row $u$ is invalidated ($M[u][j] = \infty$ for all $j$), as location $u$ can have only one departing road.
3. Column $v$ is invalidated ($M[i][v] = \infty$ for all $i$), as location $v$ can have only one arriving road.
4. Edge $(v, v_{\text{start}})$ is blocked ($M[v][v_{\text{start}}] = \infty$) unless the tour has reached level $N-1$, preventing premature subtours.
5. Immediate 2-cycle reverse transition is blocked: $M[v][u] = \infty$.
6. The child matrix is reduced via row and column reductions to yield child reduction cost $\Delta_{\text{child}}$.
7. The child's lower bound is computed as:
   $$LB(\text{Child}) = LB(\text{Parent}) + M_{\text{parent}}[u][v] + \Delta_{\text{child}}$$

### 2.4 Least-Cost Branch and Bound (LCBB) with Min-Heap

The exploration order is governed by a **Min-Heap Priority Queue** ordered by $LB$.
- At each step, the node with the lowest lower bound is extracted.
- **Pruning Criterion:** If $LB(\text{Node}) \ge \text{BestCost}$, the node is discarded immediately without generating its children.
- **Initial Upper Bound:** Prior to tree exploration, the algorithm executes Nearest Neighbor to establish a valid upper bound $\text{BestCost} = \text{Cost}(NN)$, enabling aggressive pruning right from the root.
- **Heap Compaction:** Whenever a superior complete tour is found, all nodes in the priority queue with $LB \ge \text{new\_best\_cost}$ are purged and freed, preventing memory exhaustion.

### 2.5 Complexity Analysis

- **Theoretical Worst-Case Time:** $O(N! \cdot N^2)$ when no pruning occurs (pathological adversarial networks).
- **Empirical Average Time:** $O(N^2 \cdot 2^N)$ with matrix reduction lower bounding.
- **Space Complexity:** $O(M \cdot N^2)$, where $M$ is the maximum number of live active nodes retained in the min-heap priority queue.

---

## 3. Algorithm 2: Nearest Neighbor Heuristic

### 3.1 Algorithmic Strategy

Nearest Neighbor is a greedy heuristic algorithm:
1. Initialize delivery trip at starting warehouse $v_{\text{start}}$.
2. Mark $v_{\text{start}}$ as visited ($V_{\text{visited}} = \{v_{\text{start}}\}$).
3. For step $k = 1$ to $N - 1$:
   - Identify unvisited vertex $v^* \notin V_{\text{visited}}$ minimizing $C[\text{current}][v^*]$.
   - Move to $v^*$, append $v^*$ to route, accumulate cost, and mark $v^*$ as visited.
4. Return from the final delivery stop to the starting warehouse $v_{\text{start}}$, adding edge $C[\text{last}][v_{\text{start}}]$.

### 3.2 Complexity Analysis

- **Time Complexity:** $O(N^2)$ — for each of the $N$ steps, scans up to $N$ candidate vertices.
- **Space Complexity:** $O(N)$ — requires only a single 1D boolean visited array and path array.
- **Optimality:** Suboptimal / Local Optima. Because greedy choices are irrevocable, the algorithm frequently leaves distant nodes for the end of the tour, incurring heavy penalty costs on the final segments or the closing return edge.

---

## 4. System Architecture & Engineering Upgrades

The overall application architecture integrates frontend, backend, network services, and persistent storage:

```
+-------------------------------------------------------------------+
|                        WEB FRONTEND                               |
|   HTML5  |  CSS3 (PPT Theme)  |  Vanilla JS  |  Leaflet Map      |
+-------------------------------------------------------------------+
                               | HTTP (Port 8080)
                               v
+-------------------------------------------------------------------+
|                    C HTTP BACKEND SERVER                          |
|   - Multi-threaded Winsock2 HTTP Server (backend/server.c)         |
|   - RESTful Dispatcher: /api/geocode, /api/matrix, /api/optimize  |
|   - cJSON Parser & Serializer (backend/cJSON.c)                   |
|   - SQLite3 Database Driver (backend/database.c)                  |
+-------------------------------------------------------------------+
        |                                       |
        | External API Requests                 | Database Persistence
        v                                       v
+-----------------------+              +----------------------------+
|   LOCATION & ROUTING  |              |      SQLITE DATABASE       |
| - OSM Nominatim       |              | (database/delivery.db)     |
|   (Geocoding)         |              | - Locations                |
| - OSRM Routing Engine |              | - Delivery_Plans           |
|   (Road Distances)    |              | - Route_Results            |
|   (Road Geometries)   |              | - Distance_Cache           |
+-----------------------+              +----------------------------+
                               |
                               | Directed Cost Matrix C[i][j]
                               v
+-------------------------------------------------------------------+
|                     COMMON C ALGORITHM ENGINE                     |
| - Exact Branch & Bound LCBB (Common/branch_bound.c)               |
| - Greedy Nearest Neighbor (Common/nearest_neighbor.c)             |
| - Topology Validation & Cycle Verification (Common/route.c)       |
+-------------------------------------------------------------------+
                               |
                               v
+-------------------------------------------------------------------+
|                 RESULTS & COMPARISON DASHBOARD                    |
| - Optimal Route Display (Branch and Bound)                        |
| - Approximate Route Display (Nearest Neighbor)                    |
| - Cost Difference, Gap Percentage, Real Road Map Polylines        |
+-------------------------------------------------------------------+
```

---

## 5. Database Architecture (SQLite)

The SQLite database (`database/delivery.db`) maintains four normalized tables:

1. **`Locations` Table:**
   - Caches geocoded latitude and longitude coordinates resolved via Nominatim to minimize external network requests.
   - Schema: `location_id` (PK), `location_name` (UNIQUE), `latitude`, `longitude`, `display_name`, `created_at`.

2. **`Delivery_Plans` Table:**
   - Records each execution session, including the warehouse origin, number of delivery stops, locations JSON payload, and full directed cost matrix JSON.
   - Schema: `plan_id` (PK AUTOINCREMENT), `source_location`, `total_locations`, `locations_json`, `matrix_json`, `created_at`.

3. **`Route_Results` Table:**
   - Stores the execution results of each algorithm run for a specific delivery plan.
   - Schema: `result_id` (PK), `plan_id` (FK), `algorithm_name`, `route_sequence`, `total_distance`, `execution_time`, `nodes_explored`.

4. **`Distance_Cache` Table:**
   - Caches pairwise driving distances in kilometers computed by OSRM.
   - Composite Primary Key: `(source_location, destination_location)`.

---

## 6. Experimental Benchmark & Performance Results

### 6.1 Benchmark Evaluation on Real-World and Standard ATSP Networks

| Benchmark Dataset | Locations ($N$) | Metric | Branch & Bound (Exact) | Nearest Neighbor (Heuristic) | Performance Gap ($\Delta$) | Optimality Verdict |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: |
| **SRM AP 5-Hub Network**<br>*(SRM, PVP, Trendset, Airport, Mangalagiri)* | 5 | Distance (km)<br>Operational Cost (₹)<br>Nodes Explored | **87.09 km**<br>**₹ 1,045.10**<br>5 nodes | **88.16 km**<br>**₹ 1,057.92**<br>21 steps | **+1.07 km (+1.22%)<br>B&B saves ₹ 12.82** | **B&B strictly superior (Prunes greedy trap)** |
| **Sample 1: Urban Circuit**<br>*(Asymmetric Urban Depots)* | 4 | Distance / Cost (₹)<br>Time (ms)<br>Nodes Explored | **72.00 km (₹ 864)**<br>0.01 ms<br>4 nodes | **76.00 km (₹ 912)**<br>0.00 ms<br>13 steps | **+4.00 units (+5.56%)<br>B&B saves ₹ 48.00** | **B&B strictly superior** |
| **Sample 2: Regional Hubs**<br>*(Classic ATSP Instance)* | 5 | Distance / Cost (₹)<br>Time (ms)<br>Nodes Explored | **28.00 km (₹ 336)**<br>0.04 ms<br>12 nodes | **36.00 km (₹ 432)**<br>0.00 ms<br>21 steps | **+8.00 units (+28.57%)<br>B&B saves ₹ 96.00** | **B&B strictly superior (28.6% Savings!)** |
| **Sample 3: Metro Express**<br>*(6 Metro Delivery Zones)* | 6 | Distance / Cost (₹)<br>Time (ms)<br>Nodes Explored | **83.00 km (₹ 996)**<br>0.12 ms<br>32 nodes | **83.00 km (₹ 996)**<br>0.01 ms<br>31 steps | 0.00 units (0.00%) | Both algorithms optimal |
| **Sample 4: Disconnected**<br>*(Isolated Node Edge Case)* | 4 | Topology Status | **No Route (INF)** | **No Route (INF)** | Validated Cleanly | Gracefully flagged |

### 6.2 E-Commerce Operational Financial Cost Formulation

In commercial package delivery logistics, routing solutions must be evaluated not only on pure mathematical graph distances but on actual monetary vehicle operational expenditures:

$$\text{Total Delivery Cost (₹)} = \left(\sum_{k=0}^{N-1} C[\pi(k)][\pi(k+1)] \times R_{\text{fuel}}\right) + (N_{\text{stops}} \times H_{\text{stop}})$$

Where:
- $C[\pi(k)][\pi(k+1)]$ is the road travel distance in kilometers along leg $k$.
- $R_{\text{fuel}}$ is the vehicle running cost (e.g. ₹ 8.50/km for motorcycle, ₹ 12.00/km for cargo van).
- $N_{\text{stops}} = N - 1$ is the total count of commercial parcel delivery destinations.
- $H_{\text{stop}}$ is the package handling, parking, and courier dispatch surcharge per stop (e.g. ₹ 30.00/stop).

**Financial Impact of Branch & Bound vs Nearest Neighbor:**
- On **Sample 2 (5 Regional Hubs)**, the greedy heuristic takes 36.00 km (₹ 432.00), while Branch & Bound identifies the exact global optimum of 28.00 km (₹ 336.00). Over a single 30-day operating month with 2 trips daily, Branch & Bound eliminates **₹ 5,760 in wasted fuel expense** per vehicle.
- On the **SRM AP 5-Hub network**, Nearest Neighbor greedily prioritizes PVP Mall before Trendset Mall, causing an unnecessary return detour, which Branch & Bound successfully prunes.

### 6.3 Key Algorithmic Insights

1. **Greedy Vulnerability to Suboptimal Traps:** In Sample 2, Nearest Neighbor selects local minimum edges early in the circuit, which forces an exceptionally expensive return edge, leading to a **28.57% excess cost penalty**. Branch and Bound systematically avoids this trap via lower bounding.
2. **Matrix Reduction Pruning Power:** For Sample 2 and Sample 3, matrix reduction pruned more than 90% of the possible permutation space, exploring only 12 and 32 nodes respectively out of hundreds of permutations.
3. **Execution Latency:** Nearest Neighbor executes in less than 0.01 ms across all tested instances, proving ideal for instant interactive responses, while Branch and Bound delivers guaranteed minimum-cost routing.

---

## 7. Verification and Automated Test Results

The automated verification suite (`tests/verify_all.c`) was compiled and executed directly against the core engine. All 24 test cases passed successfully (100% pass rate):

```
================================================================================
             DAA TEST SUITE: ALGORITHM & TOPOLOGY VERIFICATION                  
================================================================================

--- TEST SUITE 1: Predefined Benchmark Datasets ---
  [PASS] Sample 1: B&B Hamiltonian Cycle          | Valid cycle visiting all 4 nodes once, cost=72.00
  [PASS] Sample 1: NN Hamiltonian Cycle           | Valid cycle visiting all 4 nodes once, cost=76.00
  [PASS] Sample 1: B&B Optimality vs NN           | B&B (72.00) <= NN (76.00), Gap: +4.00 units
  [PASS] Sample 1: B&B matches Brute Force        | B&B (72.00) == Brute Force (72.00)
  [PASS] Sample 2: B&B Hamiltonian Cycle          | Valid cycle visiting all 5 nodes once, cost=28.00
  [PASS] Sample 2: NN Hamiltonian Cycle           | Valid cycle visiting all 5 nodes once, cost=36.00
  [PASS] Sample 2: B&B Optimality vs NN           | B&B (28.00) <= NN (36.00), Gap: +8.00 units
  [PASS] Sample 2: B&B matches Brute Force        | B&B (28.00) == Brute Force (28.00)
  [PASS] Sample 3: B&B Hamiltonian Cycle          | Valid cycle visiting all 6 nodes once, cost=83.00
  [PASS] Sample 3: NN Hamiltonian Cycle           | Valid cycle visiting all 6 nodes once, cost=83.00
  [PASS] Sample 3: B&B Optimality vs NN           | B&B (83.00) <= NN (83.00), Gap: +0.00 units
  [PASS] Sample 3: B&B matches Brute Force        | B&B (83.00) == Brute Force (83.00)
  [PASS] Sample 4 (Disconnected): B&B detects no route | Correctly identified no Hamiltonian cycle
  [PASS] Sample 4 (Disconnected): NN detects no route | Correctly reported unreachable / dead end

--- TEST SUITE 2: Custom Asymmetric Matrices ---
  [PASS] Asymmetric 4x4: B&B Hamiltonian Cycle    | Valid cycle visiting all 4 nodes
  [PASS] Asymmetric 4x4: B&B matches Brute Force  | B&B exact cost matches exhaustive permutation search
  [PASS] Asymmetric 4x4: B&B optimal vs NN        | B&B cost <= NN cost

--- TEST SUITE 3: Validation and Error Handling ---
  [PASS] Validation: Negative cost rejected       | Correctly rejected negative travel cost
  [PASS] Validation: Isolated node detected       | Correctly flagged node with no incoming/outgoing paths
  [PASS] Robustness: NULL network handled safely  | Cleanly reported error without segmentation fault
  [PASS] Robustness: Invalid start index handled  | Cleanly reported invalid starting location index

--- TEST SUITE 4: Starting Location Flexibility ---
  [PASS] Starting Location: Start from Node 0 valid | Valid cycle starting and ending at Node 0
  [PASS] Starting Location: Start from Node 2 valid | Valid cycle starting and ending at Node 2
  [PASS] Starting Location: Cycle cost equivalence | Optimal Hamiltonian cycle cost is invariant under starting node choice

================================================================================
TEST RESULTS: 24 / 24 TESTS PASSED
STATUS: ALL TESTS PASSED SUCCESSFULLY (100%)
================================================================================
```

---

## 8. Conclusion & Future Scope

The project successfully demonstrates how foundational algorithmic techniques from the Design and Analysis of Algorithms curriculum apply directly to solving complex e-commerce distribution challenges:
- **Branch and Bound** guarantees that the logistics vehicle takes the absolute minimum-distance tour, directly cutting transportation overhead and fuel emissions.
- **Nearest Neighbor** offers an instantaneous heuristic approximation suitable for real-time fleet dispatching.
- The web application architecture connects these C algorithms to real geospatial routing infrastructure (OSRM and Nominatim) and SQLite database persistence without introducing high-overhead external web runtimes.

**Future Scope:**
- Integration of live GPS telematics for dynamic in-transit rerouting.
- Capacity-constrained vehicle routing (CVRP) supporting multi-vehicle fleets.
- Customer delivery time-window constraints (TSPTW).
