// Dynamic history item endpoint (/api/history/:id)
const historyHandler = require('../history');

module.exports = (req, res) => {
  return historyHandler(req, res);
};

