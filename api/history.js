// History API endpoint (List & Query)

module.exports = (req, res) => {
  res.setHeader('Content-Type', 'application/json');
  res.setHeader('Access-Control-Allow-Origin', '*');
  res.setHeader('Access-Control-Allow-Methods', 'GET, DELETE, OPTIONS');
  res.setHeader('Access-Control-Allow-Headers', 'Content-Type');

  if (req.method === 'OPTIONS') return res.status(200).end();

  const historyStore = global.__ROUTELAB_HISTORY__ || (global.__ROUTELAB_HISTORY__ = []);

  const { id } = req.query || {};
  if (id) {
    const planId = parseInt(id, 10);
    const item = historyStore.find(h => h.plan_id === planId);
    if (!item) {
      return res.status(404).json({ success: false, error: 'Record not found' });
    }
    if (req.method === 'DELETE') {
      const idx = historyStore.indexOf(item);
      historyStore.splice(idx, 1);
      return res.status(200).json({ success: true, message: 'Deleted record' });
    }
    return res.status(200).json(item);
  }

  res.status(200).json(historyStore);
};

