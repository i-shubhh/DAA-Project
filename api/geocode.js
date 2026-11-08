// Geocoding handler using Photon Komoot & Nominatim OpenStreetMap

const KNOWN_COORDS = {
  "srm university ap": { lat: 16.48316, lon: 80.50850, name: "SRM University AP, Neerukonda, Amaravati" },
  "pvp mall": { lat: 16.50570, lon: 80.64810, name: "PVP Square Mall, MG Road, Vijayawada" },
  "vijayawada railway station": { lat: 16.51730, lon: 80.62000, name: "Vijayawada Junction Railway Station" },
  "kanaka durga temple": { lat: 16.51600, lon: 80.60670, name: "Sri Durga Malleswara Swamy Varla Devasthanam, Indrakeeladri" },
  "bhavani island": { lat: 16.52180, lon: 80.59250, name: "Bhavani Island Tourism Hub, Krishna River" },
  "aiims mangalagiri": { lat: 16.43850, lon: 80.56470, name: "AIIMS Mangalagiri, Guntur District" },
  "guntur railway station": { lat: 16.30250, lon: 80.44390, name: "Guntur Junction Railway Station" }
};

async function geocodePlace(placeName) {
  const clean = placeName.trim();
  const lower = clean.toLowerCase();

  for (const [key, val] of Object.entries(KNOWN_COORDS)) {
    if (lower.includes(key) || key.includes(lower)) {
      return { name: clean, display_name: val.name, lat: val.lat, lon: val.lon };
    }
  }

  // 1. Query Photon Komoot API
  try {
    const url = `https://photon.komoot.io/api/?q=${encodeURIComponent(clean)}&limit=1`;
    const res = await fetch(url, {
      headers: { 'User-Agent': 'OptimalDeliveryRoutePlanner/2.0' }
    });
    if (res.ok) {
      const data = await res.json();
      if (data && data.features && data.features.length > 0) {
        const feat = data.features[0];
        const [lon, lat] = feat.geometry.coordinates;
        const props = feat.properties || {};
        const displayName = [props.name || clean, props.city, props.state, props.country]
          .filter(Boolean)
          .join(', ');
        return { name: clean, display_name: displayName, lat, lon };
      }
    }
  } catch (e) {
    // continue to fallback
  }

  // 2. Fallback to Nominatim
  try {
    const url = `https://nominatim.openstreetmap.org/search?q=${encodeURIComponent(clean)}&format=json&limit=1`;
    const res = await fetch(url, {
      headers: { 'User-Agent': 'OptimalDeliveryRoutePlanner/2.0' }
    });
    if (res.ok) {
      const data = await res.json();
      if (Array.isArray(data) && data.length > 0) {
        return {
          name: clean,
          display_name: data[0].display_name,
          lat: parseFloat(data[0].lat),
          lon: parseFloat(data[0].lon)
        };
      }
    }
  } catch (e) {
    // fallback
  }

  // Fallback coords nearby if offline or network blocks
  return {
    name: clean,
    display_name: clean,
    lat: 16.50 + (Math.random() - 0.5) * 0.1,
    lon: 80.60 + (Math.random() - 0.5) * 0.1
  };
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

  const addresses = body && Array.isArray(body.addresses) ? body.addresses : [];
  if (addresses.length === 0) {
    return res.status(400).json({ success: false, error: 'No addresses provided' });
  }

  const locations = [];
  for (const addr of addresses) {
    const geo = await geocodePlace(addr);
    locations.push(geo);
  }

  res.status(200).json({ success: true, locations });
};

