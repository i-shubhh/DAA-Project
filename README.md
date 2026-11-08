# Optimal Delivery Route Planning for E-Commerce Applications
### Design and Analysis of Algorithms (DAA) Project in C Language

[![Language: C99](https://img.shields.io/badge/Language-C99-00599C.svg)](https://en.wikipedia.org/wiki/C99)
[![Platform: Windows / Linux / macOS](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux%20%7C%20macOS-blue.svg)]()
[![Database: SQLite3](https://img.shields.io/badge/Database-SQLite3-003B57.svg)](https://www.sqlite.org/)
[![UI: Leaflet.js](https://img.shields.io/badge/Mapping-Leaflet.js%20%2B%20OSM-brightgreen.svg)](https://leafletjs.com/)
[![Tests: 24/24 Passing](https://img.shields.io/badge/Tests-24%2F24%20Passing-success.svg)]()

A comprehensive, production-grade academic and practical software system for solving the **Asymmetric Travelling Salesperson Problem (ATSP)** in last-mile e-commerce logistics. Built with a dual-interface architecture featuring both a **Full-Stack Web Application powered by a pure C HTTP server and SQLite database** and an interactive **Console CLI Application**, without relying on external web runtimes (No Node.js, Python, Java, or PHP).

---

## 1. Team & Project Credits

**Institution:** Department of Computer Science and Engineering, SRM University - AP  
**Course:** Design and Analysis of Algorithms (DAA)  
**Project Title:** Optimal Delivery Route Planning for E-Commerce Applications

| Name | Registration Number | Project Contribution |
| :--- | :--- | :--- |
| **Sugandh** | `AP25110070346` | Introduction, Problem Formulation, & Literature Review |
| **Shubham** | `AP25110070745` | Nearest Neighbor Greedy Heuristic Algorithm Design & Implementation |
| **Rohit** | `AP25110070453` | Execution, Web/Console Architecture, Full-Stack Integration & Testing |
| **Mathews** | `AP25110070852` | Branch and Bound Algorithm (LCBB with Cost Matrix Reduction) |
| **Bhuvan** | `AP25110070082` | Main Application, Route Management & Verification Suite |

---

## 2. Executive Summary & Problem Formulation

In modern e-commerce delivery networks, courier vehicles depart from a central fulfillment warehouse, visit a collection of $N$ customer delivery addresses or transit hubs, and return to the starting depot. Due to urban traffic restrictions, divided highways, elevation changes, and one-way streets, travel distances and times are inherently **asymmetric** ($C[i][j] \ne C[j][i]$).

The goal of this project is to model and solve the **Asymmetric Travelling Salesperson Problem (ATSP)** using rigorous DAA techniques:
- **Given:** A set of vertices $V = \{0, 1, \dots, N-1\}$ where vertex $0$ represents the central fulfillment warehouse and vertices $1 \dots N-1$ represent delivery destinations, connected by a directed cost matrix $C$ where $C[i][j] \ge 0$ denotes road distance or time from location $i$ to $j$, with $C[i][i] = \infty$.
- **Objective:** Find a simple directed cycle (Hamiltonian cycle) covering every vertex in $V$ exactly once with minimum total path cost:
  $$\min_{\pi \in S_N} \sum_{k=0}^{N-1} C[\pi(k)][\pi((k+1) \bmod N)]$$

---

## 3. Core DAA Algorithms Implemented

### A. Branch and Bound (Exact Optimal Tour)
- **Algorithm Class:** State Space Tree Search using **Least-Cost Branch and Bound (LCBB)**.
- **Lower Bound Calculation:**
  - Evaluates partial tours using **Matrix Reduction (Row and Column Reduction)**.
  - For each unassigned row, subtracts the minimum row element $r_i = \min_j C[i][j]$.
  - For each unassigned column, subtracts the minimum column element $c_j = \min_i C[i][j]$.
  - Lower Bound: $LB(\text{child}) = LB(\text{parent}) + \hat{C}[u][v] + \sum r_i + \sum c_j$.
- **Search Strategy:**
  - Employs a **Min-Heap Priority Queue** ordered by lowest lower bound $LB$.
  - Dynamic heap expansion with automatic resizing (no arbitrary fixed capacity limits).
  - Branch pruning: any candidate node with $LB \ge \text{current best}$ is immediately discarded.
  - Bounding updates dynamically trigger heap compaction to free memory.
- **Complexity:**
  - **Worst-case Time:** $O(N! \cdot N^2)$ (unpruned exhaustive search).
  - **Average Time:** $O(N^2 \cdot 2^N)$ with matrix reduction bounding.
  - **Space Complexity:** $O(M \cdot N^2)$ where $M$ is the maximum number of live active nodes.

### B. Nearest Neighbor Heuristic (Greedy Approximation)
- **Algorithm Class:** Greedy Local Search.
- **Strategy:**
  - Begins at the designated departure depot.
  - At each step, evaluates all unvisited nodes and greedily transitions to the closest neighbor:
    $$v^* = \arg\min_{v \in \text{Unvisited}} C[u][v]$$
  - Commits permanently to decisions without backtracking.
  - Closes the cycle by traversing the directed edge back to the depot from the last customer.
- **Complexity:**
  - **Deterministic Time:** $O(N^2)$.
  - **Space Complexity:** $O(N)$ for the boolean visited state tracking array.
- **Role in System:** Serves as a fast, real-time baseline and provides an initial upper bound ($UB$) to accelerate Branch & Bound search pruning.

---

## 4. System Architecture & Features

The project is structured into three clean, modular subsystems:

```text
Optimal_Delivery_Route_Planning/
│
├── Common/                      # Core Pure C DAA Algorithms (Shared)
│   ├── route.h                  # Network definitions, RouteResult, prototypes
│   ├── route.c                  # Validation, cycle verification, benchmark datasets, brute-force solver
│   ├── nearest_neighbor.h       # Nearest Neighbor interface
│   ├── nearest_neighbor.c       # Greedy heuristic implementation
│   ├── branch_bound.h           # Branch & Bound interface
│   └── branch_bound.c           # Matrix reduction, state space tree, LCBB min-heap
│
├── backend/                     # High-Performance C Web Backend
│   ├── server.c                 # Multi-threaded Winsock2 HTTP/1.1 REST Server (port 8080)
│   ├── database.h / .c          # SQLite3 storage engine (Plans, Routes, Geocode Cache)
│   ├── geocoding.h / .c         # OpenStreetMap Nominatim geocoding client with DB cache
│   ├── routing.h / .c           # OSRM Table API distance matrix & Route API GeoJSON geometry
│   ├── http_client.h / .c       # Resilient HTTP client (supports curl.exe and libcurl)
│   ├── cJSON.h / .c             # Embedded ultra-fast JSON parser/serializer
│   ├── sqlite3.h / .c / .o      # Precompiled SQLite amalgamation engine
│
├── frontend/                    # Single-Page Web Application (Vanilla JS / CSS3 / HTML5)
│   ├── index.html               # PPT slide-aligned visual layout, hero canvas, controls
│   ├── css/style.css            # Dark theme, atmospheric glow, glassmorphism cards, gold accents
│   ├── js/app.js                 # Leaflet.js interactive maps, real-time matrix editor, REST client
│   └── assets/                   # Reserved for local frontend images and icons
│
├── Console/                     # Interactive CLI Application
│   └── main.c                   # Menu-driven CLI with step-by-step validated single-value input
│
├── tests/                       # Automated Test Suite
│   └── verify_all.c             # 24 unit & regression tests validating B&B, NN, brute-force, edge cases
│
├── database/                    # SQLite Storage
│   └── delivery.db              # Embedded relational database file
│
├── bin/                         # Compiled Executable Binaries
│   ├── delivery_server.exe      # C HTTP Web Backend
│   ├── delivery_planner_console.exe # C Console Application
│   └── verify_all.exe           # Test Suite Binary
│
├── Report/                      # Academic Project Report
│   └── DAA_Project_Report.md    # Comprehensive DAA documentation & complexity proofs
│
├── .vscode/                     # VS Code Configuration
│   └── tasks.json               # One-click build and execution tasks
│
├── Makefile                     # Cross-platform Makefile
├── build.bat                    # Windows 1-click build script
├── run_server.bat               # Windows 1-click launcher for Web App
└── build_console.bat            # Windows 1-click build script for Console App
```

### Source vs generated files

- **Edit these:** `Common/`, `backend/`, `Console/`, `tests/`, and `frontend/`.
- **Run locally:** `run_server.bat` / `make run` starts the web application; the browser client lives entirely in `frontend/`.
- **Generated or local state:** `bin/` contains compiled executables, `.cache/` contains editor indexes, and `database/delivery.db*` contains SQLite data and WAL files. These are not application source files.
- **Frontend details:** `frontend/index.html` owns structure, `frontend/css/style.css` owns presentation and responsive behavior, and `frontend/js/app.js` owns browser state and API integration.

### Key Capabilities:
1. **Real-World Geocoding & Road Distances:** Users can type actual city or landmark names (e.g., *"SRM University AP"*, *"PVP Mall"*, *"Trendset Mall"*, *"Vijayawada Airport"*). The system queries OpenStreetMap / Photon for coordinates and OSRM Table API for driving road distances.
2. **Turn-by-Turn Real Road Geometry & Courier Simulation:** Renders real highway and street polyline curves on Leaflet.js with an animated 🛵 **Delivery Courier Bike**, speed controls (1x, 2x, 4x), and leg-by-leg turn-by-turn itinerary breakdown.
3. **E-Commerce Real Cost Model (₹ - INR):** Calculates real vehicle fuel expenditure, maintenance, handling payouts per stop, cost per package delivered, and direct financial savings in rupees achieved by Branch and Bound.
4. **Interactive Algorithm Method Selector & Explainer:** Instant toggles between Comparative View, Branch & Bound (Exact LCBB), and Nearest Neighbor (Greedy), featuring live DAA mathematical decision traces and team attribution.
5. **Dual-Unit Matrix View (Distance & Financial Cost):** 3-way toggle showing road distances (km), delivery expenses (₹), or both simultaneously, with inline cell editing for road closures.
6. **Zero-API-Key OpenStreetMap Engine:** Standard OSM tile foundation with a custom midnight-dark filter and live Dark / Normal street map switcher.
7. **Persistent SQLite Plan History:** All calculated delivery plans, location sets, matrices, and comparative metrics are saved to `database/delivery.db` with one-click reload and deletion capabilities.
8. **Exact PPT Visual Replication:** Dark palette (`#050505` to `#08090d`), ambient cyan and amber radial glows, condensed uppercase typography, glassmorphism cards, and gold accent dividers mirroring `DAA project ppt .pdf`.

---

## 5. Quick Start & Execution

### Prerequisites
- Windows 7/10/11 with MinGW / GCC (`gcc.exe` in PATH).
- Native Windows `curl.exe` (included by default in Windows 10/11) or `libcurl`.

---

### Option 1: Full-Stack Web Application (Recommended)

1. **One-Click Build & Launch:**
   Double-click `run_server.bat` or run in terminal:
   ```cmd
   run_server.bat
   ```
   *This will compile all components (if not already compiled), start the C HTTP backend server on `http://localhost:8080`, and open your default browser automatically.*

2. **Manual Compilation with GCC:**
   ```cmd
   build.bat
   bin\delivery_server.exe
   ```
   Then open `http://localhost:8080` in your web browser.

---

### Option 2: Console CLI Application

1. **One-Click Build:**
   Double-click `build_console.bat` or run:
   ```cmd
   build_console.bat
   bin\delivery_planner_console.exe
   ```

2. **Direct GCC Compilation:**
   ```bash
   gcc -std=c99 -Wall -Wextra -O2 -ICommon Common/route.c Common/nearest_neighbor.c Common/branch_bound.c Console/main.c -o bin/delivery_planner_console.exe
   .\bin\delivery_planner_console.exe
   ```

---

### Option 3: Using Visual Studio Code
1. Open this folder in VS Code (`File` -> `Open Folder`).
2. Press `Ctrl + Shift + B` to trigger the default build task (`Build All Components`).
3. Press `Ctrl + Shift + P` -> `Tasks: Run Task` and select:
   - **`Run Web Server (port 8080)`** to start the web backend.
   - **`Run Console Application`** to launch the CLI interface.
   - **`Run Automated Verification Suite`** to execute the 24 tests.

---

### Option 4: Using `make` (Linux / macOS / Windows with MSYS2)
```bash
make all      # Builds both Web Server and Console Application
make run      # Compiles and runs the Web Server
make test     # Compiles and runs the 24 automated verification tests
make clean    # Cleans all binary outputs
```

---

## 6. REST API Reference

The C HTTP backend exposes clean REST endpoints:

| Endpoint | Method | Description | Request Payload / Params |
| :--- | :---: | :--- | :--- |
| `/` | `GET` | Serves the main Single-Page Web Application | None |
| `/css/*`, `/js/*` | `GET` | Serves static styles, scripts, and assets | None |
| `/api/samples` | `GET` | Returns 4 standardized benchmark ATSP delivery networks | None |
| `/api/geocode` | `POST` | Geocodes an address query using Nominatim + SQLite cache | `{"address": "Vijayawada"}` |
| `/api/matrix` | `POST` | Computes driving distance matrix between $N$ coordinates via OSRM | `{"locations": [{"lat":16.5, "lng":80.6}, ...]}` |
| `/api/optimize` | `POST` | Solves ATSP via B&B and NN, fetches road geometry, saves to SQLite | `{"name": "...", "locations": [...], "matrix": [...]}` |
| `/api/history` | `GET` | Lists all past saved delivery plans from SQLite | None |
| `/api/history/{id}` | `GET` | Retrieves full plan details, locations, matrix, and solutions | URL path ID |
| `/api/history/{id}` | `DELETE`| Deletes a plan and associated route results from SQLite | URL path ID |

---

## 7. SQLite Database Schema

The backend embedded database (`database/delivery.db`) employs four relational tables configured in Write-Ahead Logging (`WAL`) mode:

```sql
-- 1. Persistent Delivery Plans
CREATE TABLE IF NOT EXISTS Delivery_Plans (
    plan_id INTEGER PRIMARY KEY AUTOINCREMENT,
    plan_name TEXT NOT NULL,
    num_locations INTEGER NOT NULL,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- 2. Locations associated with plans
CREATE TABLE IF NOT EXISTS Locations (
    location_id INTEGER PRIMARY KEY AUTOINCREMENT,
    plan_id INTEGER NOT NULL,
    location_index INTEGER NOT NULL,
    name TEXT NOT NULL,
    latitude REAL,
    longitude REAL,
    is_warehouse INTEGER DEFAULT 0,
    FOREIGN KEY(plan_id) REFERENCES Delivery_Plans(plan_id) ON DELETE CASCADE
);

-- 3. Optimization results (B&B and NN solutions)
CREATE TABLE IF NOT EXISTS Route_Results (
    result_id INTEGER PRIMARY KEY AUTOINCREMENT,
    plan_id INTEGER NOT NULL,
    algorithm_name TEXT NOT NULL,
    total_cost REAL NOT NULL,
    execution_time_ms REAL NOT NULL,
    nodes_evaluated INTEGER NOT NULL,
    route_sequence TEXT NOT NULL,
    geometry_geojson TEXT,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(plan_id) REFERENCES Delivery_Plans(plan_id) ON DELETE CASCADE
);

-- 4. Geocoding and distance cache
CREATE TABLE IF NOT EXISTS Distance_Cache (
    cache_id INTEGER PRIMARY KEY AUTOINCREMENT,
    source_lat REAL NOT NULL,
    source_lng REAL NOT NULL,
    dest_lat REAL NOT NULL,
    dest_lng REAL NOT NULL,
    distance_meters REAL NOT NULL,
    duration_seconds REAL NOT NULL,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(source_lat, source_lng, dest_lat, dest_lng)
);
```

---

## 8. Benchmark Datasets & Algorithmic Comparison

All algorithms were verified on benchmark instances and checked against an exhaustive brute-force solver:

| Benchmark Dataset | $N$ | Graph Type | Branch & Bound Cost | Nearest Neighbor Cost | Optimality Gap | Verification Status |
| :--- | :---: | :--- | :---: | :---: | :---: | :---: |
| **Sample 1: Urban Circuit** | 4 | Asymmetric (Traffic) | **72.00** | 76.00 | +5.56% (NN Suboptimal) | **PASS** (Matches Brute Force) |
| **Sample 2: Regional Hubs** | 5 | Asymmetric (ATSP) | **28.00** | 36.00 | **+28.57%** (Greedy Trap) | **PASS** (Matches Brute Force) |
| **Sample 3: Express Distribution** | 6 | Asymmetric (Highway) | **83.00** | 83.00 | 0.00% (Both Optimal) | **PASS** (Matches Brute Force) |
| **Sample 4: Disconnected Graph** | 4 | Blocked Path ($\infty$) | *No Route* | *No Route* | N/A | **PASS** (Dead End Detected) |

### Algorithmic Comparison Matrix:

| Dimension | Branch and Bound (LCBB) | Nearest Neighbor Heuristic |
| :--- | :--- | :--- |
| **Paradigm** | State Space Search + Matrix Reduction | Greedy Local Search |
| **Optimality** | **Guaranteed Global Minimum ($Z^*$)** | Approximate / Suboptimal |
| **Average Time Complexity** | $O(N^2 \cdot 2^N)$ with pruning | $O(N^2)$ deterministic |
| **Worst-Case Time Complexity** | $O(N! \cdot N^2)$ | $O(N^2)$ deterministic |
| **Space Complexity** | $O(M \cdot N^2)$ (Priority Queue Min-Heap) | $O(N)$ (Visited Array) |
| **Pruning Mechanism** | Lower Bound Matrix Reductions ($LB \ge Z^*$) | None (Commits greedily) |
| **E-Commerce Role** | Optimal fleet route planning for $N \le 16$ | Real-time immediate dispatching |

---

## 9. Automated Verification Suite

The project includes an automated test harness (`tests/verify_all.c`) executing 24 comprehensive unit and regression tests:

```cmd
.\bin\verify_all.exe
```

### Test Suite Output:
```text
[TEST 01] Network creation and initialization ............................ PASS
[TEST 02] Self-travel costs are INF ...................................... PASS
[TEST 03] Valid cycle cost evaluation .................................... PASS
[TEST 04] Invalid cycle detection (missing vertex) ....................... PASS
[TEST 05] Invalid cycle detection (duplicate vertex) ..................... PASS
[TEST 06] Invalid cycle detection (broken cycle return) .................. PASS
[TEST 07] Disconnected edge cycle cost evaluation (INF) .................. PASS
[TEST 08] Matrix reduction lower bound non-negativity .................... PASS
[TEST 09] Brute force solver on Sample 1 (Urban Circuit) ................. PASS
[TEST 10] Brute force solver on Sample 2 (Regional Hubs) ................. PASS
[TEST 11] Brute force solver on Sample 3 (Express Distribution) .......... PASS
[TEST 12] Branch and Bound optimal tour on Sample 1 ...................... PASS
[TEST 13] Branch and Bound matches brute-force on Sample 1 ............... PASS
[TEST 14] Branch and Bound optimal tour on Sample 2 (Cost = 28) .......... PASS
[TEST 15] Branch and Bound matches brute-force on Sample 2 ............... PASS
[TEST 16] Branch and Bound optimal tour on Sample 3 (Cost = 83) .......... PASS
[TEST 17] Branch and Bound matches brute-force on Sample 3 ............... PASS
[TEST 18] Branch and Bound on Disconnected Graph (Graceful handling) ..... PASS
[TEST 19] Nearest Neighbor tour validity on Sample 1 ..................... PASS
[TEST 20] Nearest Neighbor demonstrates greedy suboptimality on Sample 2 . PASS
[TEST 21] Nearest Neighbor on Sample 3 ................................... PASS
[TEST 22] Nearest Neighbor handles disconnected graphs gracefully ........ PASS
[TEST 23] Custom starting location invariance on Sample 1 ................ PASS
[TEST 24] Boundary condition: 2-node minimal network ..................... PASS
================================================================================
ALL 24 TESTS PASSED SUCCESSFULLY!
```

---

## 10. Verification of Project Constraints

- [x] **Strictly C Language:** Core algorithms, data structures, HTTP server, and SQLite integration written entirely in standard C99.
- [x] **Dual Interface:** Both a feature-complete Leaflet.js Web Application and an interactive Console CLI application.
- [x] **Embedded Persistence:** SQLite amalgamation used with Write-Ahead Logging (`WAL`) mode.
- [x] **Zero External Runtime Dependencies:** No Node.js, Python, or external web frameworks required.
- [x] **PPT Visual Style Conformity:** Strictly adheres to the deep dark background, glassmorphism cards, and cyan/amber atmospheric glows of `DAA project ppt .pdf`.
- [x] **Academic Rigor:** Full theoretical analysis, matrix reduction proofs, and complexity benchmarks documented in `Report/DAA_Project_Report.md`.
