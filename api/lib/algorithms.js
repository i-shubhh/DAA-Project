// ==============================================================================
// Optimal Delivery Route Planning Algorithms (Node.js / Vercel Serverless)
// Exact match with C implementations in Common/nearest_neighbor.c and Common/branch_bound.c
// ==============================================================================

const INF = 1e9;

// Sample benchmark networks
const SAMPLES = [
  {
    id: 1,
    name: "Sample 1: 4 Locations (Urban Delivery Circuit - Asymmetric)",
    num_locations: 4,
    names: [
      "Central Warehouse",
      "Downtown Hub",
      "North Suburb Depot",
      "South Suburb Depot"
    ],
    matrix: [
      [-1, 12.0, 25.0, 20.0],
      [10.0, -1, 15.0, 18.0],
      [22.0, 14.0, -1, 30.0],
      [19.0, 16.0, 28.0, -1]
    ]
  },
  {
    id: 2,
    name: "Sample 2: 5 Locations (Regional Fulfillment Centers - Classic ATSP)",
    num_locations: 5,
    names: [
      "Fulfillment Center",
      "Metro North Hub",
      "West Suburb Depot",
      "East Terminal",
      "South Depot"
    ],
    matrix: [
      [-1, 20.0, 30.0, 10.0, 11.0],
      [15.0, -1, 16.0, 4.0, 2.0],
      [3.0, 5.0, -1, 2.0, 4.0],
      [19.0, 6.0, 18.0, -1, 3.0],
      [16.0, 4.0, 7.0, 16.0, -1]
    ]
  },
  {
    id: 3,
    name: "Sample 3: 6 Locations (Metro Express Distribution Network)",
    num_locations: 6,
    names: [
      "Primary Logistics Hub",
      "Zone A (Commercial)",
      "Zone B (Residential)",
      "Zone C (Industrial)",
      "Zone D (Airport Cargo)",
      "Zone E (Harbor Depot)"
    ],
    matrix: [
      [-1, 15.0, 25.0, 35.0, 40.0, 20.0],
      [14.0, -1, 10.0, 28.0, 32.0, 18.0],
      [26.0, 12.0, -1, 14.0, 22.0, 30.0],
      [33.0, 27.0, 16.0, -1, 12.0, 25.0],
      [38.0, 30.0, 20.0, 15.0, -1, 10.0],
      [22.0, 17.0, 29.0, 24.0, 11.0, -1]
    ]
  },
  {
    id: 4,
    name: "Sample 4: 4 Locations (Disconnected Graph - Edge Case Test)",
    num_locations: 4,
    names: [
      "Central Terminal",
      "Isolated North Outpost",
      "East Depot",
      "South Hub"
    ],
    matrix: [
      [-1, 10.0, 15.0, 20.0],
      [-1, -1, -1, -1],
      [12.0, 8.0, -1, 14.0],
      [18.0, 9.0, 11.0, -1]
    ]
  }
];

function calculateRouteCost(matrix, path) {
  let cost = 0;
  for (let i = 0; i < path.length - 1; i++) {
    const u = path[i];
    const v = path[i + 1];
    const edge = matrix[u][v];
    if (edge < 0 || edge >= INF / 2) return INF;
    cost += edge;
  }
  return cost;
}

// -----------------------------------------------------------------------------
// Nearest Neighbor (Greedy Heuristic)
// -----------------------------------------------------------------------------
function solveNearestNeighbor(names, matrix, startIndex = 0) {
  const n = names.length;
  const start = performance.now();
  const visited = new Array(n).fill(false);
  const path = [startIndex];
  visited[startIndex] = true;
  let current = startIndex;
  let totalCost = 0;
  let nodesExplored = 0;

  for (let step = 1; step < n; step++) {
    let bestNext = -1;
    let minEdgeCost = INF;

    for (let candidate = 0; candidate < n; candidate++) {
      nodesExplored++;
      if (!visited[candidate] && candidate !== current) {
        const c = matrix[current][candidate];
        if (c >= 0 && c < minEdgeCost && c < INF / 2) {
          minEdgeCost = c;
          bestNext = candidate;
        }
      }
    }

    if (bestNext === -1) {
      return {
        is_valid: false,
        message: "Nearest Neighbor cannot proceed: Dead end reached without visiting all locations.",
        total_cost: INF,
        path: [],
        path_names: [],
        execution_time_ms: Math.max(0.01, performance.now() - start),
        nodes_explored: nodesExplored,
        route_string: "No valid route found"
      };
    }

    visited[bestNext] = true;
    path.push(bestNext);
    totalCost += minEdgeCost;
    current = bestNext;
  }

  // Return edge to start
  nodesExplored++;
  const returnCost = matrix[current][startIndex];
  if (returnCost < 0 || returnCost >= INF / 2) {
    return {
      is_valid: false,
      message: "Nearest Neighbor failed: No valid returning edge to warehouse to complete the cycle.",
      total_cost: INF,
      path: [],
      path_names: [],
      execution_time_ms: Math.max(0.01, performance.now() - start),
      nodes_explored: nodesExplored,
      route_string: "No valid route found"
    };
  }

  path.push(startIndex);
  totalCost += returnCost;

  const routeString = path.map(idx => `[${idx}] ${names[idx]}`).join(" -> ");

  return {
    is_valid: true,
    message: "Optimal nearest neighbor tour computed.",
    total_cost: parseFloat(totalCost.toFixed(2)),
    path,
    path_names: path.map(idx => names[idx]),
    execution_time_ms: Math.max(0.01, parseFloat((performance.now() - start).toFixed(3))),
    nodes_explored: nodesExplored,
    route_string: routeString
  };
}

// -----------------------------------------------------------------------------
// Matrix Reduction
// -----------------------------------------------------------------------------
function reduceMatrix(mat, n) {
  let reductionCost = 0;

  // 1. Row reduction
  for (let i = 0; i < n; i++) {
    let rowMin = INF;
    for (let j = 0; j < n; j++) {
      if (mat[i][j] < rowMin) rowMin = mat[i][j];
    }
    if (rowMin < INF / 2 && rowMin > 0) {
      reductionCost += rowMin;
      for (let j = 0; j < n; j++) {
        if (mat[i][j] < INF / 2) mat[i][j] -= rowMin;
      }
    }
  }

  // 2. Column reduction
  for (let j = 0; j < n; j++) {
    let colMin = INF;
    for (let i = 0; i < n; i++) {
      if (mat[i][j] < colMin) colMin = mat[i][j];
    }
    if (colMin < INF / 2 && colMin > 0) {
      reductionCost += colMin;
      for (let i = 0; i < n; i++) {
        if (mat[i][j] < INF / 2) mat[i][j] -= colMin;
      }
    }
  }

  return reductionCost;
}

// Clone 2D array
function cloneMatrix(mat, n) {
  const res = new Array(n);
  for (let i = 0; i < n; i++) {
    res[i] = mat[i].slice(0, n);
  }
  return res;
}

// -----------------------------------------------------------------------------
// Branch and Bound (Least-Cost Branch & Bound with Matrix Reduction)
// -----------------------------------------------------------------------------
function solveBranchAndBound(names, rawMatrix, startIndex = 0) {
  const n = names.length;
  const start = performance.now();

  // Prepare standard matrix with INF for self/disconnected
  const normMatrix = new Array(n);
  for (let i = 0; i < n; i++) {
    normMatrix[i] = new Array(n);
    for (let j = 0; j < n; j++) {
      if (i === j) normMatrix[i][j] = INF;
      else {
        const val = rawMatrix[i][j];
        normMatrix[i][j] = (val < 0 || val >= INF / 2) ? INF : val;
      }
    }
  }

  // Initial upper bound using Nearest Neighbor
  const nnInit = solveNearestNeighbor(names, rawMatrix, startIndex);
  let bestCost = nnInit.is_valid ? nnInit.total_cost : INF;
  let bestPath = nnInit.is_valid ? nnInit.path.slice() : null;

  // Root node
  const rootMat = cloneMatrix(normMatrix, n);
  const rootBound = reduceMatrix(rootMat, n);

  const rootNode = {
    level: 0,
    vertex: startIndex,
    bound: rootBound,
    path: [startIndex],
    visited: new Array(n).fill(false),
    matrix: rootMat
  };
  rootNode.visited[startIndex] = true;

  // Min-Heap priority queue
  const heap = [rootNode];
  let nodesExplored = 0;

  function pushHeap(node) {
    heap.push(node);
    let idx = heap.length - 1;
    while (idx > 0) {
      const parent = Math.floor((idx - 1) / 2);
      if (heap[parent].bound <= heap[idx].bound) break;
      const tmp = heap[parent];
      heap[parent] = heap[idx];
      heap[idx] = tmp;
      idx = parent;
    }
  }

  function popHeap() {
    if (heap.length === 0) return null;
    const top = heap[0];
    const last = heap.pop();
    if (heap.length > 0) {
      heap[0] = last;
      let idx = 0;
      while (idx * 2 + 1 < heap.length) {
        let left = idx * 2 + 1;
        let right = idx * 2 + 2;
        let smallest = left;
        if (right < heap.length && heap[right].bound < heap[left].bound) {
          smallest = right;
        }
        if (heap[idx].bound <= heap[smallest].bound) break;
        const tmp = heap[idx];
        heap[idx] = heap[smallest];
        heap[smallest] = tmp;
        idx = smallest;
      }
    }
    return top;
  }

  while (heap.length > 0) {
    const current = popHeap();
    if (!current) break;
    nodesExplored++;

    if (current.bound >= bestCost) {
      continue; // Pruned
    }

    if (current.level === n - 1) {
      const lastV = current.vertex;
      const returnEdge = normMatrix[lastV][startIndex];
      if (returnEdge < INF / 2) {
        const candidatePath = current.path.concat([startIndex]);
        const actualCost = calculateRouteCost(normMatrix, candidatePath);
        if (actualCost < bestCost) {
          bestCost = actualCost;
          bestPath = candidatePath;
        }
      }
      continue;
    }

    // Branch to unvisited children
    const u = current.vertex;
    for (let v = 0; v < n; v++) {
      if (!current.visited[v] && current.matrix[u][v] < INF / 2) {
        const edgeReducedCost = current.matrix[u][v];
        const childMat = cloneMatrix(current.matrix, n);

        for (let j = 0; j < n; j++) childMat[u][j] = INF;
        for (let i = 0; i < n; i++) childMat[i][v] = INF;
        if (current.level + 1 < n - 1) childMat[v][startIndex] = INF;
        childMat[v][u] = INF;

        const childReduction = reduceMatrix(childMat, n);
        const childBound = current.bound + edgeReducedCost + childReduction;

        if (childBound < bestCost) {
          const childVisited = current.visited.slice();
          childVisited[v] = true;
          const childNode = {
            level: current.level + 1,
            vertex: v,
            bound: childBound,
            path: current.path.concat([v]),
            visited: childVisited,
            matrix: childMat
          };
          pushHeap(childNode);
        }
      }
    }
  }

  if (!bestPath || bestCost >= INF / 2) {
    return {
      is_valid: false,
      message: "No complete Hamiltonian cycle found in the delivery network.",
      total_cost: INF,
      path: [],
      path_names: [],
      execution_time_ms: Math.max(0.01, performance.now() - start),
      nodes_explored: nodesExplored,
      route_string: "No valid route found"
    };
  }

  const routeString = bestPath.map(idx => `[${idx}] ${names[idx]}`).join(" -> ");

  return {
    is_valid: true,
    message: "Guaranteed global optimum computed by Branch and Bound.",
    total_cost: parseFloat(bestCost.toFixed(2)),
    path: bestPath,
    path_names: bestPath.map(idx => names[idx]),
    execution_time_ms: Math.max(0.01, parseFloat((performance.now() - start).toFixed(3))),
    nodes_explored: nodesExplored,
    route_string: routeString
  };
}

module.exports = {
  INF,
  SAMPLES,
  solveNearestNeighbor,
  solveBranchAndBound
};

