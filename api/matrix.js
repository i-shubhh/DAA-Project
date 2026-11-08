// Road distance matrix service using OSRM Table API with Haversine fallback

function haversineDistance(lat1, lon1, lat2, lon2) {
  const R = 6371.0; // Earth radius in km
  const dLat = (lat2 - lat1) * Math.PI / 180.0;
  const dLon = (lon2 - lon1) * Math.PI / 180.0;
  const a = Math.sin(dLat / 2) * Math.sin(dLat / 2) +
            Math.cos(lat1 * Math.PI / 180.0) * Math.cos(lat2 * Math.PI / 180.0) *
            Math.sin(dLon / 2) * Math.sin(dLon / 2);
  const c = 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a));
  const crow = R * c;
  // Detour factor ~1.32 for urban/suburban roads
  return parseFloat((crow * 1.32).toFixed(2));
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
  const n = locs.length;
  if (n < 2 || n > 20) {
    return res.status(400).json({ success: false, error: 'Location count must be between 2 and 20' });
  }

  const names = locs.map(l => l.name || 'Location');
  const coordinates = locs.map(l => ({ lat: l.lat, lon: l.lon }));

  let matrix = Array.from({ length: n }, () => new Array(n).fill(-1));
  let osrmSuccess = false;

  // Try querying Project OSRM table API
  try {
    const coordsStr = locs.map(l => `${l.lon.toFixed(6)},${l.lat.toFixed(6)}`).join(';');
    const osrmUrl = `https://router.project-osrm.org/table/v1/driving/${coordsStr}?annotations=distance`;
    const osrmRes = await fetch(osrmUrl, {
      headers: { 'User-Agent': 'OptimalDeliveryRoutePlanner/2.0' }
    });
    if (osrmRes.ok) {
      const data = await osrmRes.json();
      if (data && data.code === 'Ok' && Array.isArray(data.distances)) {
        for (let i = 0; i < n; i++) {
          for (let j = 0; j < n; j++) {
            if (i === j) {
              matrix[i][j] = -1;
            } else {
              const meters = data.distances[i][j];
              matrix[i][j] = (meters === null || meters === undefined)
                ? -1
                : parseFloat((meters / 1000.0).toFixed(2));
            }
          }
        }
        osrmSuccess = true;
      }
    }
  } catch (e) {
    osrmSuccess = false;
  }

  // Fallback to accurate Haversine road detour formula if OSRM is busy/throttled
  if (!osrmSuccess) {
    for (let i = 0; i < n; i++) {
      for (let j = 0; j < n; j++) {
        if (i === j) {
          matrix[i][j] = -1;
        } else {
          matrix[i][j] = haversineDistance(locs[i].lat, locs[i].lon, locs[j].lat, locs[j].lon);
        }
      }
    }
  }

  res.status(200).json({
    success: true,
    num_locations: n,
    names,
    coordinates,
    matrix
  });
};

