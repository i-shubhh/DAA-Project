// Route Optimization API (Branch and Bound vs Nearest Neighbor)

const { solveBranchAndBound, solveNearestNeighbor } = require('./lib/algorithms');

// In-memory persistent history store across serverless warm instances
const historyStore = global.__ROUTELAB_HISTORY__ || (global.__ROUTELAB_HISTORY__ = []);

async function fetchRouteGeometry(path, locs) {
  try {
    const coordsStr = path
      .map(idx => `${locs[idx].lon.toFixed(6)},${locs[idx].lat.toFixed(6)}`)
      .join(';');
    const url = `https://router.project-osrm.org/route/v1/driving/${coordsStr}?overview=full&geometries=geojson`;
    const res = await fetch(url, {
      headers: { 'User-Agent': 'OptimalDeliveryRoutePlanner/2.0' }
    });
    if (res.ok) {
      const data = await res.json();
      if (data && data.code === 'Ok' && data.routes && data.routes[0]) {
        return data.routes[0].geometry;
      }
    }
  } catch (e) {
    // fallback
  }
  return null;
}

module.exports = async (req, res) => {
  res.setHeader('Content-Type', 'application/json');
  res.setHeader('Access-Control-Allow-Origin', '*');
  res.setHeader('Access-Control-Allow-Methods', 'POST, OPTIONS');
  res.setHeader('Access-Control-Allow-Headers', 'Content-Type');

  if (req.method === 'OPTIONS') return res.status(200).end();

  let body = req.body;
  if (typeof body === 'string') {
    try { body = JSON.parse(body); } catch (e) {}
  }

  const locs = body && Array.isArray(body.locations) ? body.locations : [];
  const matrix = body && Array.isArray(body.matrix) ? body.matrix : [];
  const startIndex = (body && typeof body.startIndex === 'number') ? body.startIndex : 0;

  const n = locs.length;
  if (n < 2 || n > 20 || matrix.length !== n) {
    return res.status(400).json({
      success: false,
      error: "'locations' and 'matrix' arrays are required with matching dimensions [2..20]"
    });
  }

  const names = locs.map(l => (typeof l === 'string' ? l : l.name || 'Location'));

  // 1. Execute Algorithms
  const bbRes = solveBranchAndBound(names, matrix, startIndex);
  const nnRes = solveNearestNeighbor(names, matrix, startIndex);

  // 2. Fetch OSRM GeoJSON geometry for interactive Leaflet polyline if coordinates available
  const hasCoords = locs.every(l => typeof l.lat === 'number' && typeof l.lon === 'number' && l.lat !== 0);
  if (hasCoords) {
    if (bbRes.is_valid) {
      bbRes.geometry = await fetchRouteGeometry(bbRes.path, locs);
    }
    if (nnRes.is_valid) {
      nnRes.geometry = await fetchRouteGeometry(nnRes.path, locs);
    }
  }

  // 3. Comparison
  const cmp = {
    bb_valid: bbRes.is_valid,
    nn_valid: nnRes.is_valid
  };

  if (bbRes.is_valid && nnRes.is_valid) {
    const diffKm = parseFloat((nnRes.total_cost - bbRes.total_cost).toFixed(2));
    const pctDiff = bbRes.total_cost > 0
      ? parseFloat(((diffKm / bbRes.total_cost) * 100.0).toFixed(2))
      : 0.0;

    cmp.difference_km = diffKm;
    cmp.cost_difference = diffKm;
    cmp.percent_difference = pctDiff;
    cmp.optimality_gap_percent = pctDiff;

    if (diffKm > 0.001) {
      cmp.winner = "Branch and Bound produced a shorter route by optimizing the global permutation.";
    } else if (diffKm < -0.001) {
      cmp.winner = "Nearest Neighbor produced equal or shorter route (unusual for non-metric graphs).";
    } else {
      cmp.winner = "Both algorithms achieved identical travel distance on this network topology.";
    }
  } else {
    cmp.winner = "Direct comparison unavailable: one or both algorithms failed to form a complete Hamiltonian cycle.";
  }

  // 4. Save to History
  const planId = historyStore.length + 1;
  const historyItem = {
    plan_id: planId,
    start_location: names[startIndex],
    num_locations: n,
    created_at: new Date().toISOString().replace('T', ' ').substring(0, 19),
    locations_json: JSON.stringify(locs),
    cost_matrix_json: JSON.stringify(matrix),
    bb_cost: bbRes.is_valid ? bbRes.total_cost : -1,
    bb_route: bbRes.is_valid ? bbRes.route_string : 'N/A',
    bb_time_ms: bbRes.execution_time_ms,
    bb_nodes: bbRes.nodes_explored,
    nn_cost: nnRes.is_valid ? nnRes.total_cost : -1,
    nn_route: nnRes.is_valid ? nnRes.route_string : 'N/A',
    nn_time_ms: nnRes.execution_time_ms,
    nn_nodes: nnRes.nodes_explored
  };
  historyStore.unshift(historyItem);
  if (historyStore.length > 50) historyStore.pop();

  res.status(200).json({
    success: true,
    plan_id: planId,
    num_locations: n,
    start_index: startIndex,
    start_location: names[startIndex],
    branch_and_bound: bbRes,
    nearest_neighbor: nnRes,
    comparison: cmp
  });
};

