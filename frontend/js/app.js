/**
 * OPTIMAL DELIVERY ROUTE PLANNING FOR E-COMMERCE APPLICATIONS
 * Frontend Application Controller (Vanilla JavaScript)
 * Communicates with C HTTP Backend, Leaflet, and OpenStreetMap
 */

document.addEventListener('DOMContentLoaded', () => {
  // Application State
  const state = {
    locations: [
      { name: 'SRM University AP', lat: null, lon: null, display_name: '' }
    ],
    destinations: [
      { name: 'PVP Mall', lat: null, lon: null, display_name: '' },
      { name: 'Trendset Mall', lat: null, lon: null, display_name: '' },
      { name: 'Vijayawada Airport', lat: null, lon: null, display_name: '' },
      { name: 'Mangalagiri', lat: null, lon: null, display_name: '' }
    ],
    matrix: null,          // 2D NxN array of road distances in km
    isManualMatrix: false,
    matrixDisplayMode: 'both', // 'both', 'dist', 'cost'
    activeMethodView: 'both',   // 'both', 'bb', 'nn'
    costModel: {
      ratePerKm: 12.0,         // ₹ 12.00 per km
      handlingPerStop: 30.0    // ₹ 30.00 handling per delivery stop
    },
    results: null,
    map: null,
    tileLayer: null,
    isDarkMapTheme: true,
    mapLayers: {
      bb: null,
      nn: null,
      markers: [],
      bikeMarker: null
    },
    activeLayerFilter: 'both',
    simulation: {
      isPlaying: false,
      speed: 1,
      animId: null,
      currentCoordIndex: 0,
      routeCoords: [],     // array of [lat, lng]
      legs: [],            // array of leg metadata
      currentLegIndex: 0
    }
  };

  const MAX_TOTAL_LOCATIONS = 20;

  // DOM Elements
  const startingInput = document.getElementById('startingWarehouse');
  const destinationsContainer = document.getElementById('destinationsContainer');
  const btnAddDestination = document.getElementById('btnAddDestination');
  const destCountBadge = document.getElementById('destCountBadge');
  const btnFindDistances = document.getElementById('btnFindDistances');
  const btnCalculateOptimal = document.getElementById('btnCalculateOptimal');
  const btnToggleManualMatrix = document.getElementById('btnToggleManualMatrix');
  const btnLoadAPSample = document.getElementById('btnLoadAPSample');
  const selectBenchmarkSample = document.getElementById('selectBenchmarkSample');
  const matrixTable = document.getElementById('matrixTable');
  const matrixStatusBadge = document.getElementById('matrixStatusBadge');
  const resultsSection = document.getElementById('results-section');
  const historyTable = document.getElementById('historyTable');
  const btnRefreshHistory = document.getElementById('btnRefreshHistory');
  const toastContainer = document.getElementById('toastContainer');
  const backendStatus = document.getElementById('backendStatus');
  const navToggle = document.getElementById('navToggle');
  const primaryNavigation = document.getElementById('primaryNavigation');

  // Cost Configuration DOM Elements
  const vehicleTypeSelect = document.getElementById('vehicleTypeSelect');
  const costPerKmInput = document.getElementById('costPerKmInput');
  const handlingPerStopInput = document.getElementById('handlingPerStopInput');

  // Simulation DOM Elements
  const btnSimulateRide = document.getElementById('btnSimulateRide');
  const btnResetRide = document.getElementById('btnResetRide');
  const simBtnLabel = document.getElementById('simBtnLabel');
  const simPlayIcon = document.getElementById('simPlayIcon');
  const simStatusText = document.getElementById('simStatusText');
  const itineraryGrid = document.getElementById('itineraryGrid');
  const itineraryTotalBadge = document.getElementById('itineraryTotalBadge');
  const btnToggleMapTheme = document.getElementById('btnToggleMapTheme');

  // Method Selection DOM Elements
  const pillBothMethods = document.getElementById('pillBothMethods');
  const pillBBMethod = document.getElementById('pillBBMethod');
  const pillNNMethod = document.getElementById('pillNNMethod');
  const activeMethodBadge = document.getElementById('activeMethodBadge');
  const methodExplainerBanner = document.getElementById('methodExplainerBanner');
  const explainerTitleText = document.getElementById('explainerTitleText');
  const explainerCreditBadge = document.getElementById('explainerCreditBadge');
  const explainerDescText = document.getElementById('explainerDescText');
  const cardBB = document.getElementById('cardBB');
  const cardNN = document.getElementById('cardNN');
  const comparisonSummaryCard = document.getElementById('comparisonSummaryCard');

  // Initialize
  initNetworkCanvas();
  renderDestinationInputs();
  initMap();
  checkBackendStatus();
  fetchHistory();

  // Event Listeners
  btnAddDestination.addEventListener('click', addDestinationField);
  btnLoadAPSample.addEventListener('click', loadAPExample);
  selectBenchmarkSample.addEventListener('change', handleBenchmarkSelection);
  btnFindDistances.addEventListener('click', handleFindDistances);
  btnCalculateOptimal.addEventListener('click', handleCalculateOptimal);
  btnToggleManualMatrix.addEventListener('click', handleToggleManualMatrix);
  btnRefreshHistory.addEventListener('click', fetchHistory);

  if (navToggle && primaryNavigation) {
    navToggle.addEventListener('click', () => {
      const isOpen = document.body.classList.toggle('nav-open');
      navToggle.setAttribute('aria-expanded', String(isOpen));
      navToggle.setAttribute('aria-label', isOpen ? 'Close navigation' : 'Open navigation');
    });

    primaryNavigation.querySelectorAll('a').forEach(link => {
      link.addEventListener('click', () => {
        document.body.classList.remove('nav-open');
        navToggle.setAttribute('aria-expanded', 'false');
        navToggle.setAttribute('aria-label', 'Open navigation');
      });
    });
  }

  document.getElementById('btnToggleBoth').addEventListener('click', () => setLayerFilter('both'));
  document.getElementById('btnToggleBB').addEventListener('click', () => setLayerFilter('bb'));
  document.getElementById('btnToggleNN').addEventListener('click', () => setLayerFilter('nn'));

  if (btnToggleMapTheme) {
    btnToggleMapTheme.addEventListener('click', toggleMapTheme);
  }

  if (btnSimulateRide) {
    btnSimulateRide.addEventListener('click', toggleSimulation);
  }

  if (btnResetRide) {
    btnResetRide.addEventListener('click', resetSimulation);
  }

  document.querySelectorAll('.speed-btn').forEach(btn => {
    btn.addEventListener('click', (e) => {
      document.querySelectorAll('.speed-btn').forEach(b => b.classList.remove('active'));
      e.target.classList.add('active');
      state.simulation.speed = parseInt(e.target.dataset.speed, 10) || 1;
    });
  });

  // Matrix Unit Toggles (Both, Distance, Cost)
  document.querySelectorAll('#matrixUnitToggle .speed-btn').forEach(btn => {
    btn.addEventListener('click', (e) => {
      document.querySelectorAll('#matrixUnitToggle .speed-btn').forEach(b => b.classList.remove('active'));
      e.target.classList.add('active');
      state.matrixDisplayMode = e.target.dataset.unit || 'both';
      if (state.matrix) {
        const startName = startingInput.value.trim() || 'Warehouse';
        const destNames = state.destinations.map(d => d.name.trim() || 'Stop');
        renderMatrixTable([startName, ...destNames], state.matrix);
      }
    });
  });

  // Cost Input Listeners
  if (vehicleTypeSelect) {
    vehicleTypeSelect.addEventListener('change', (e) => {
      if (e.target.value !== 'custom') {
        const rate = parseFloat(e.target.value);
        state.costModel.ratePerKm = rate;
        if (costPerKmInput) costPerKmInput.value = rate.toFixed(1);
        recalcAndRefreshCosts();
      }
    });
  }

  if (costPerKmInput) {
    costPerKmInput.addEventListener('input', (e) => {
      const val = parseFloat(e.target.value);
      if (!isNaN(val) && val > 0) {
        state.costModel.ratePerKm = val;
        if (vehicleTypeSelect) vehicleTypeSelect.value = 'custom';
        recalcAndRefreshCosts();
      }
    });
  }

  if (handlingPerStopInput) {
    handlingPerStopInput.addEventListener('input', (e) => {
      const val = parseFloat(e.target.value);
      if (!isNaN(val) && val >= 0) {
        state.costModel.handlingPerStop = val;
        recalcAndRefreshCosts();
      }
    });
  }

  // Method Selection Listeners
  if (pillBothMethods) {
    pillBothMethods.addEventListener('click', () => setMethodView('both'));
  }
  if (pillBBMethod) {
    pillBBMethod.addEventListener('click', () => setMethodView('bb'));
  }
  if (pillNNMethod) {
    pillNNMethod.addEventListener('click', () => setMethodView('nn'));
  }

  // Setup Quick Suggestion Chips
  document.querySelectorAll('#quickSuggestChips .chip-btn').forEach(chip => {
    chip.addEventListener('click', () => {
      const placeName = chip.dataset.place;
      let emptyDest = state.destinations.find(d => !d.name || d.name.trim() === '');
      if (emptyDest) {
        emptyDest.name = placeName;
      } else {
        if (state.destinations.length + 1 >= MAX_TOTAL_LOCATIONS) {
          showToast(`Maximum limit of ${MAX_TOTAL_LOCATIONS} total locations reached.`, 'error');
          return;
        }
        state.destinations.push({ name: placeName, lat: null, lon: null, display_name: placeName });
      }
      renderDestinationInputs();
      showToast(`Added '${placeName}' to delivery destinations.`, 'info');
    });
  });

  // Check Backend Server Status
  async function checkBackendStatus() {
    try {
      const res = await fetch('/api/samples');
      if (res.ok) {
        backendStatus.className = 'status-badge online';
        backendStatus.innerHTML = '<span class="status-dot"></span> C Backend Online';
      } else {
        throw new Error();
      }
    } catch {
      backendStatus.className = 'status-badge offline';
      backendStatus.innerHTML = '<span class="status-dot"></span> Backend Offline (Check port 8080)';
    }
  }

  // Destination Input Management
  function renderDestinationInputs() {
    destinationsContainer.innerHTML = '';
    state.destinations.forEach((dest, idx) => {
      const row = document.createElement('div');
      row.className = 'destination-row';
      row.innerHTML = `
        <div class="input-with-icon">
          <span class="input-icon">
            <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><path d="M21 10c0 7-9 13-9 13s-9-6-9-13a9 9 0 0 1 18 0z"></path><circle cx="12" cy="10" r="3"></circle></svg>
          </span>
          <input type="text" class="form-control dest-input" data-index="${idx}" 
                 value="${escapeHtml(dest.name)}" 
                 placeholder="Destination ${idx + 1} address or place name" required>
        </div>
        <button type="button" class="btn-remove-dest" data-index="${idx}" title="Remove Destination">
          <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polyline points="3 6 5 6 21 6"></polyline><path d="M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2"></path></svg>
        </button>
      `;
      destinationsContainer.appendChild(row);
    });

    destCountBadge.textContent = `${state.destinations.length} Destination${state.destinations.length === 1 ? '' : 's'}`;

    // Attach listeners to dynamic elements
    destinationsContainer.querySelectorAll('.dest-input').forEach(input => {
      input.addEventListener('input', (e) => {
        const idx = parseInt(e.target.dataset.index, 10);
        state.destinations[idx].name = e.target.value;
      });
    });

    destinationsContainer.querySelectorAll('.btn-remove-dest').forEach(btn => {
      btn.addEventListener('click', (e) => {
        const idx = parseInt(e.currentTarget.dataset.index, 10);
        if (state.destinations.length <= 1) {
          showToast('At least 1 delivery destination is required.', 'error');
          return;
        }
        state.destinations.splice(idx, 1);
        renderDestinationInputs();
      });
    });
  }

  function addDestinationField() {
    const total = 1 + state.destinations.length;
    if (total >= MAX_TOTAL_LOCATIONS) {
      showToast(`Maximum limit of ${MAX_TOTAL_LOCATIONS} total locations reached.`, 'error');
      return;
    }
    state.destinations.push({ name: '', lat: null, lon: null, display_name: '' });
    renderDestinationInputs();
    const inputs = destinationsContainer.querySelectorAll('.dest-input');
    if (inputs.length > 0) inputs[inputs.length - 1].focus();
  }

  // Load AP Example with 5 Diverse Locations (demonstrating true route divergence)
  function loadAPExample() {
    startingInput.value = 'SRM University AP';
    state.destinations = [
      { name: 'PVP Mall', lat: null, lon: null, display_name: '' },
      { name: 'Trendset Mall', lat: null, lon: null, display_name: '' },
      { name: 'Vijayawada Airport', lat: null, lon: null, display_name: '' },
      { name: 'Mangalagiri', lat: null, lon: null, display_name: '' }
    ];
    renderDestinationInputs();
    showToast('Loaded SRM AP regional delivery network (5 Diverse Hubs).', 'info');
  }

  async function handleBenchmarkSelection(e) {
    const sampleId = e.target.value;
    if (!sampleId) return;

    try {
      const res = await fetch('/api/samples');
      const samples = await res.json();
      const sample = samples.find(s => s.id === parseInt(sampleId, 10));

      if (!sample) return;

      startingInput.value = sample.names[0];
      state.destinations = [];
      for (let i = 1; i < sample.num_locations; i++) {
        state.destinations.push({ name: sample.names[i], lat: null, lon: null, display_name: '' });
      }
      renderDestinationInputs();

      state.matrix = sample.matrix;
      state.isManualMatrix = true;
      renderMatrixTable(sample.names, state.matrix);
      matrixStatusBadge.textContent = `Predefined Benchmark (${sample.name})`;
      matrixStatusBadge.style.color = 'var(--accent-orange)';
      showToast(`Loaded ${sample.name}. You can calculate optimal route now.`, 'success');
    } catch {
      showToast('Failed to load benchmark sample dataset.', 'error');
    }
  }

  // Geocoding and OSRM Distance Computation
  async function handleFindDistances() {
    const startName = startingInput.value.trim();
    if (!startName) {
      showToast('Please enter a starting warehouse location.', 'error');
      startingInput.focus();
      return;
    }

    const destNames = state.destinations.map(d => d.name.trim()).filter(n => n.length > 0);
    if (destNames.length < 1) {
      showToast('Please enter at least 1 delivery destination.', 'error');
      return;
    }

    const allNames = [startName, ...destNames];
    if (allNames.length > MAX_TOTAL_LOCATIONS) {
      showToast(`Total locations exceeds limit of ${MAX_TOTAL_LOCATIONS}.`, 'error');
      return;
    }

    btnFindDistances.disabled = true;
    btnFindDistances.innerHTML = `<span class="status-dot"></span> Resolving locations...`;

    try {
      const resolvedLocations = [];
      for (let i = 0; i < allNames.length; i++) {
        const name = allNames[i];
        btnFindDistances.innerHTML = `<span class="status-dot"></span> Locating (${i + 1}/${allNames.length}): ${escapeHtml(truncate(name, 15))}...`;
        
        const geoRes = await fetch('/api/geocode', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ address: name })
        });

        const geoData = await geoRes.json();
        if (!geoData.success) {
          throw new Error(`Address '${name}' could not be located. Try selecting from 'Popular Stops' or check spelling.`);
        }

        resolvedLocations.push({
          name: name,
          display_name: geoData.display_name,
          lat: geoData.lat,
          lon: geoData.lon
        });
      }

      state.locations = [resolvedLocations[0]];
      state.destinations = resolvedLocations.slice(1);
      document.getElementById('sourceCoordsBadge').textContent = `${resolvedLocations[0].lat.toFixed(4)}, ${resolvedLocations[0].lon.toFixed(4)}`;

      btnFindDistances.innerHTML = `<span class="status-dot"></span> Querying OSRM road distances...`;
      const matrixRes = await fetch('/api/matrix', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ locations: resolvedLocations })
      });

      const matrixData = await matrixRes.json();
      if (!matrixData.success) {
        throw new Error(matrixData.error || 'Failed to compute road distance matrix.');
      }

      state.matrix = matrixData.matrix;
      state.isManualMatrix = false;

      renderMatrixTable(allNames, state.matrix);
      matrixStatusBadge.textContent = 'OSRM True Road Distances & Costs - Active';
      matrixStatusBadge.style.color = 'var(--accent-blue)';
      showToast(`Resolved ${allNames.length} locations and computed asymmetric road matrix.`, 'success');

      document.getElementById('matrix-section').scrollIntoView({ behavior: 'smooth' });
    } catch (err) {
      showToast(err.message, 'error', 6000);
    } finally {
      btnFindDistances.disabled = false;
      btnFindDistances.innerHTML = `
        <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="11" cy="11" r="8"></circle><line x1="21" y1="21" x2="16.65" y2="16.65"></line></svg>
        Find Distances (OSRM)
      `;
    }
  }

  // Render Matrix Table with Dual Display (Distance km + Financial Cost ₹)
  function renderMatrixTable(names, matrix) {
    const n = names.length;
    const mode = state.matrixDisplayMode;
    const rate = state.costModel.ratePerKm;

    let html = '<thead><tr><th>From \\ To</th>';
    for (let j = 0; j < n; j++) {
      html += `<th>[${j}] ${escapeHtml(truncate(names[j], 14))}</th>`;
    }
    html += '</tr></thead><tbody>';

    for (let i = 0; i < n; i++) {
      html += `<tr><th>[${i}] ${escapeHtml(truncate(names[i], 14))}</th>`;
      for (let j = 0; j < n; j++) {
        if (i === j) {
          html += '<td class="diag">--</td>';
        } else {
          const val = matrix && matrix[i] && matrix[i][j] !== undefined ? matrix[i][j] : -1;
          if (val < 0) {
            html += '<td class="val" style="color:var(--text-muted);">INF</td>';
          } else {
            const costVal = val * rate;
            let cellContent = '';
            if (mode === 'dist') {
              cellContent = `${val.toFixed(2)} km`;
            } else if (mode === 'cost') {
              cellContent = `<span style="color:#10B981; font-weight:700;">₹ ${costVal.toFixed(2)}</span>`;
            } else {
              // 'both'
              cellContent = `<div>${val.toFixed(2)} km</div><span class="matrix-cell-cost">₹ ${costVal.toFixed(2)}</span>`;
            }
            html += `<td class="val">${cellContent}</td>`;
          }
        }
      }
      html += '</tr>';
    }
    html += '</tbody>';
    matrixTable.innerHTML = html;
  }

  function handleToggleManualMatrix() {
    const startName = startingInput.value.trim() || 'Warehouse';
    const destNames = state.destinations.map(d => d.name.trim() || 'Stop');
    const allNames = [startName, ...destNames];
    const n = allNames.length;

    let html = '<thead><tr><th>From \\ To</th>';
    for (let j = 0; j < n; j++) {
      html += `<th>[${j}] ${escapeHtml(truncate(allNames[j], 14))}</th>`;
    }
    html += '</tr></thead><tbody>';

    for (let i = 0; i < n; i++) {
      html += `<tr><th>[${i}] ${escapeHtml(truncate(allNames[i], 14))}</th>`;
      for (let j = 0; j < n; j++) {
        if (i === j) {
          html += '<td class="diag">--</td>';
        } else {
          const currentVal = (state.matrix && state.matrix[i] && state.matrix[i][j] >= 0) ? state.matrix[i][j] : '';
          html += `<td><input type="number" step="0.1" min="-1" class="matrix-cell-input" data-row="${i}" data-col="${j}" value="${currentVal}" placeholder="-1"></td>`;
        }
      }
      html += '</tr>';
    }
    html += '</tbody>';
    matrixTable.innerHTML = html;

    state.isManualMatrix = true;
    matrixStatusBadge.textContent = 'Custom Travel Cost Matrix (Editable)';
    matrixStatusBadge.style.color = 'var(--accent-orange)';
    showToast('Manual matrix edit mode enabled. Enter travel costs or -1 for disconnected edges.', 'info');
    document.getElementById('matrix-section').scrollIntoView({ behavior: 'smooth' });

    matrixTable.querySelectorAll('.matrix-cell-input').forEach(input => {
      input.addEventListener('change', readManualMatrixFromTable);
    });
  }

  function readManualMatrixFromTable() {
    const inputs = matrixTable.querySelectorAll('.matrix-cell-input');
    const destNames = state.destinations.map(d => d.name.trim() || 'Stop');
    const n = 1 + destNames.length;

    const newMatrix = Array.from({ length: n }, () => Array(n).fill(-1));
    inputs.forEach(input => {
      const r = parseInt(input.dataset.row, 10);
      const c = parseInt(input.dataset.col, 10);
      const val = parseFloat(input.value);
      newMatrix[r][c] = (isNaN(val) || val < 0) ? -1 : val;
    });

    state.matrix = newMatrix;
  }

  // Calculate Optimal Route Execution
  async function handleCalculateOptimal() {
    const startName = startingInput.value.trim();
    if (!startName) {
      showToast('Please enter a starting warehouse location.', 'error');
      return;
    }

    const destNames = state.destinations.map(d => d.name.trim()).filter(n => n.length > 0);
    if (destNames.length < 1) {
      showToast('Please enter at least 1 delivery destination.', 'error');
      return;
    }

    const allNames = [startName, ...destNames];
    const n = allNames.length;

    if (!state.matrix || state.matrix.length !== n) {
      showToast('Cost matrix not ready. Please click "Find Distances (OSRM)" first.', 'error');
      return;
    }

    if (state.isManualMatrix) {
      readManualMatrixFromTable();
    }

    btnCalculateOptimal.disabled = true;
    btnCalculateOptimal.innerHTML = `<span class="status-dot"></span> Solving TSP via DAA Algorithms...`;

    try {
      const payloadLocations = [];
      payloadLocations.push({
        name: startName,
        lat: (state.locations[0] && state.locations[0].lat) || 0.0,
        lon: (state.locations[0] && state.locations[0].lon) || 0.0
      });

      state.destinations.forEach((d, i) => {
        payloadLocations.push({
          name: d.name || `Destination ${i + 1}`,
          lat: d.lat || 0.0,
          lon: d.lon || 0.0
        });
      });

      const optRes = await fetch('/api/optimize', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          name: `${startName} Delivery Plan`,
          locations: payloadLocations,
          matrix: state.matrix,
          startIndex: 0
        })
      });

      const data = await optRes.json();
      if (!data.success) {
        throw new Error(data.error || 'Failed to solve Travelling Salesperson Problem.');
      }

      state.results = data;
      renderOptimizationResults(data, payloadLocations);
      resultsSection.style.display = 'block';
      resultsSection.scrollIntoView({ behavior: 'smooth' });

      // Refresh map size
      setTimeout(() => {
        if (state.map) state.map.invalidateSize();
      }, 200);

      fetchHistory();
      showToast('Optimal delivery routes & real financial costs computed!', 'success');
    } catch (err) {
      showToast(err.message, 'error', 6000);
    } finally {
      btnCalculateOptimal.disabled = false;
      btnCalculateOptimal.innerHTML = `
        <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><polyline points="22 12 18 12 15 21 9 3 6 12 2 12"></polyline></svg>
        Calculate Optimal Route
      `;
    }
  }

  // Financial Cost Model Calculation
  function calculateCostBreakdown(totalDistKm, numLocations) {
    const numStops = Math.max(1, numLocations - 1);
    const ratePerKm = state.costModel.ratePerKm;
    const handlingRate = state.costModel.handlingPerStop;

    const fuelTransitCost = totalDistKm * ratePerKm;
    const handlingCost = numStops * handlingRate;
    const totalCost = fuelTransitCost + handlingCost;
    const costPerStop = totalCost / numStops;

    // Estimate transit time at 35 km/h urban speed + 10 mins per stop handling
    const travelHours = totalDistKm / 35.0;
    const handlingHours = (numStops * 10) / 60.0;
    const totalHours = travelHours + handlingHours;
    const hrs = Math.floor(totalHours);
    const mins = Math.round((totalHours - hrs) * 60);

    return {
      ratePerKm,
      numStops,
      fuelTransitCost,
      handlingCost,
      totalCost,
      costPerStop,
      timeString: `~${hrs}h ${mins}m`
    };
  }

  function recalcAndRefreshCosts() {
    if (!state.results) return;
    const payloadLocations = [
      { name: startingInput.value.trim() },
      ...state.destinations
    ];
    renderOptimizationResults(state.results, payloadLocations);
    if (state.matrix) {
      renderMatrixTable(payloadLocations.map(l => l.name), state.matrix);
    }
  }

  // Render DAA Results Dashboard with Real-World Costs (₹)
  function renderOptimizationResults(data, locations) {
    const bb = data.branch_and_bound;
    const nn = data.nearest_neighbor;
    const n = locations.length;

    // 1. Branch & Bound Card (Exact LCBB)
    const bbTotalCost = document.getElementById('bbTotalCost');
    const bbRouteSequence = document.getElementById('bbRouteSequence');
    const bbTimeMs = document.getElementById('bbTimeMs');
    const bbNodesExplored = document.getElementById('bbNodesExplored');
    const bbFinancialCost = document.getElementById('bbFinancialCost');
    const bbFuelCost = document.getElementById('bbFuelCost');
    const bbHandlingCost = document.getElementById('bbHandlingCost');
    const bbCostPerStop = document.getElementById('bbCostPerStop');
    const bbEstTime = document.getElementById('bbEstTime');

    let bbCostObj = null;
    if (bb.is_valid) {
      bbCostObj = calculateCostBreakdown(bb.total_cost, n);
      bbTotalCost.textContent = bb.total_cost.toFixed(2);
      bbRouteSequence.innerHTML = escapeHtml(bb.route_string).replace(/->/g, '<span class="route-arrow">&rarr;</span>');
      bbTimeMs.textContent = `${bb.execution_time_ms.toFixed(4)} ms`;
      bbNodesExplored.textContent = `${bb.nodes_explored} nodes`;

      if (bbFinancialCost) bbFinancialCost.textContent = bbCostObj.totalCost.toFixed(2);
      if (bbFuelCost) bbFuelCost.textContent = `₹ ${bbCostObj.fuelTransitCost.toFixed(2)}`;
      if (bbHandlingCost) bbHandlingCost.textContent = `₹ ${bbCostObj.handlingCost.toFixed(2)}`;
      if (bbCostPerStop) bbCostPerStop.textContent = `₹ ${bbCostObj.costPerStop.toFixed(2)}/stop`;
      if (bbEstTime) bbEstTime.textContent = bbCostObj.timeString;
    } else {
      bbTotalCost.textContent = 'INF';
      bbRouteSequence.textContent = bb.message || 'No feasible Hamiltonian cycle found.';
      bbTimeMs.textContent = 'N/A';
      bbNodesExplored.textContent = `${bb.nodes_explored} nodes`;
      if (bbFinancialCost) bbFinancialCost.textContent = 'N/A';
    }

    // 2. Nearest Neighbor Card (Greedy Heuristic)
    const nnTotalCost = document.getElementById('nnTotalCost');
    const nnRouteSequence = document.getElementById('nnRouteSequence');
    const nnTimeMs = document.getElementById('nnTimeMs');
    const nnNodesExplored = document.getElementById('nnNodesExplored');
    const nnFinancialCost = document.getElementById('nnFinancialCost');
    const nnFuelCost = document.getElementById('nnFuelCost');
    const nnHandlingCost = document.getElementById('nnHandlingCost');
    const nnCostPerStop = document.getElementById('nnCostPerStop');
    const nnEstTime = document.getElementById('nnEstTime');

    let nnCostObj = null;
    if (nn.is_valid) {
      nnCostObj = calculateCostBreakdown(nn.total_cost, n);
      nnTotalCost.textContent = nn.total_cost.toFixed(2);
      nnRouteSequence.innerHTML = escapeHtml(nn.route_string).replace(/->/g, '<span class="route-arrow">&rarr;</span>');
      nnTimeMs.textContent = `${nn.execution_time_ms.toFixed(4)} ms`;
      nnNodesExplored.textContent = `${nn.nodes_explored} steps`;

      if (nnFinancialCost) nnFinancialCost.textContent = nnCostObj.totalCost.toFixed(2);
      if (nnFuelCost) nnFuelCost.textContent = `₹ ${nnCostObj.fuelTransitCost.toFixed(2)}`;
      if (nnHandlingCost) nnHandlingCost.textContent = `₹ ${nnCostObj.handlingCost.toFixed(2)}`;
      if (nnCostPerStop) nnCostPerStop.textContent = `₹ ${nnCostObj.costPerStop.toFixed(2)}/stop`;
      if (nnEstTime) nnEstTime.textContent = nnCostObj.timeString;
    } else {
      nnTotalCost.textContent = 'INF';
      nnRouteSequence.textContent = nn.message || 'Greedy heuristic reached dead end.';
      nnTimeMs.textContent = 'N/A';
      nnNodesExplored.textContent = `${nn.nodes_explored} steps`;
      if (nnFinancialCost) nnFinancialCost.textContent = 'N/A';
    }

    // 3. Comparison Summary
    const compBBCost = document.getElementById('compBBCost');
    const compNNCost = document.getElementById('compNNCost');
    const compBBDist = document.getElementById('compBBDist');
    const compNNDist = document.getElementById('compNNDist');
    const compMoneySaved = document.getElementById('compMoneySaved');
    const compDiffKm = document.getElementById('compDiffKm');
    const compPctDiff = document.getElementById('compPctDiff');
    const compVerdictText = document.getElementById('compVerdictText');

    const cmp = data.comparison;
    if (bb && bb.is_valid && nn && nn.is_valid && bbCostObj && nnCostObj) {
      const moneySaved = Math.max(0, nnCostObj.totalCost - bbCostObj.totalCost);
      const distSaved = Math.max(0, nn.total_cost - bb.total_cost);
      const pctDiff = (bb.total_cost > 0.0) ? ((distSaved / bb.total_cost) * 100.0) : 0.0;

      if (compBBCost) compBBCost.textContent = `₹ ${bbCostObj.totalCost.toFixed(2)}`;
      if (compNNCost) compNNCost.textContent = `₹ ${nnCostObj.totalCost.toFixed(2)}`;
      if (compBBDist) compBBDist.textContent = `${bb.total_cost.toFixed(2)} km`;
      if (compNNDist) compNNDist.textContent = `${nn.total_cost.toFixed(2)} km`;
      if (compMoneySaved) compMoneySaved.textContent = `₹ ${moneySaved.toFixed(2)} Saved`;
      if (compDiffKm) compDiffKm.textContent = `${distSaved.toFixed(2)} km saved`;
      if (compPctDiff) compPctDiff.textContent = `${pctDiff.toFixed(2)}%`;

      if (distSaved > 0.05) {
        compVerdictText.innerHTML = `<strong>Branch and Bound saved ₹ ${moneySaved.toFixed(2)} (+${pctDiff.toFixed(2)}% financial savings)</strong> by pruning suboptimal routes and avoiding Nearest Neighbor's greedy detour traps!`;
      } else {
        compVerdictText.innerHTML = `Both algorithms found the minimal tour of <strong>${bb.total_cost.toFixed(2)} km (₹ ${bbCostObj.totalCost.toFixed(2)})</strong> on this specific road network topology.`;
      }
    } else {
      if (compBBCost) compBBCost.textContent = bb.is_valid ? `₹ ${(bbCostObj ? bbCostObj.totalCost : 0).toFixed(2)}` : 'INF';
      if (compNNCost) compNNCost.textContent = nn.is_valid ? `₹ ${(nnCostObj ? nnCostObj.totalCost : 0).toFixed(2)}` : 'INF';
      if (compBBDist) compBBDist.textContent = bb.is_valid ? `${bb.total_cost.toFixed(2)} km` : 'INF';
      if (compNNDist) compNNDist.textContent = nn.is_valid ? `${nn.total_cost.toFixed(2)} km` : 'INF';
      if (compMoneySaved) compMoneySaved.textContent = 'N/A';
      if (compDiffKm) compDiffKm.textContent = 'N/A';
      if (compPctDiff) compPctDiff.textContent = 'N/A';
      if (compVerdictText) compVerdictText.textContent = cmp ? cmp.winner : 'Calculation complete.';
    }

    // 4. Update Leaflet Map and Delivery Simulation
    updateMapRoutes(locations, bb, nn, state.matrix);
  }

  // Method Selection Mode Switcher
  function setMethodView(mode) {
    state.activeMethodView = mode;

    [pillBothMethods, pillBBMethod, pillNNMethod].forEach(p => {
      if (p) p.classList.remove('active', 'nn-only');
    });

    if (mode === 'both') {
      if (pillBothMethods) pillBothMethods.classList.add('active');
      if (cardBB) cardBB.style.display = 'block';
      if (cardNN) cardNN.style.display = 'block';
      if (comparisonSummaryCard) comparisonSummaryCard.style.display = 'block';
      if (activeMethodBadge) {
        activeMethodBadge.textContent = 'DAA Comparative Evaluation Active';
        activeMethodBadge.style.color = 'var(--accent-blue)';
      }
      if (methodExplainerBanner) {
        methodExplainerBanner.className = 'method-explainer-banner';
      }
      if (explainerTitleText) {
        explainerTitleText.textContent = 'Method: Branch & Bound (Exact Optimum) vs Nearest Neighbor (Greedy)';
      }
      if (explainerCreditBadge) {
        explainerCreditBadge.textContent = 'Mathews (AP25110070852) & Shubham (AP25110070745)';
      }
      if (explainerDescText) {
        explainerDescText.textContent = 'Side-by-side comparison of exact combinatorial search versus greedy local search. Branch & Bound uses matrix reduction lower bounds to guarantee minimum fuel expenditure, while Nearest Neighbor provides fast real-time upper bounds.';
      }
      setLayerFilter('both');
    } else if (mode === 'bb') {
      if (pillBBMethod) pillBBMethod.classList.add('active');
      if (cardBB) cardBB.style.display = 'block';
      if (cardNN) cardNN.style.display = 'none';
      if (comparisonSummaryCard) comparisonSummaryCard.style.display = 'none';
      if (activeMethodBadge) {
        activeMethodBadge.textContent = 'Method: Branch & Bound (Exact LCBB)';
        activeMethodBadge.style.color = 'var(--accent-blue)';
      }
      if (methodExplainerBanner) {
        methodExplainerBanner.className = 'method-explainer-banner';
      }
      if (explainerTitleText) {
        explainerTitleText.textContent = 'Active Method: Branch and Bound (Least-Cost B&B with Cost Matrix Reduction)';
      }
      if (explainerCreditBadge) {
        explainerCreditBadge.textContent = 'Algorithm Lead: Mathews (AP25110070852)';
      }
      if (explainerDescText) {
        explainerDescText.textContent = 'Explores the state space tree using Least-Cost Branch and Bound (LCBB). At each candidate node, computes a rigorous lower bound by reducing the row and column costs. Any branch with Lower Bound ≥ current best tour is strictly pruned, mathematically guaranteeing the global minimum delivery cost.';
      }
      setLayerFilter('bb');
    } else if (mode === 'nn') {
      if (pillNNMethod) pillNNMethod.classList.add('active', 'nn-only');
      if (cardBB) cardBB.style.display = 'none';
      if (cardNN) cardNN.style.display = 'block';
      if (comparisonSummaryCard) comparisonSummaryCard.style.display = 'none';
      if (activeMethodBadge) {
        activeMethodBadge.textContent = 'Method: Nearest Neighbor (Greedy Approximation)';
        activeMethodBadge.style.color = 'var(--accent-orange)';
      }
      if (methodExplainerBanner) {
        methodExplainerBanner.className = 'method-explainer-banner nn-mode';
      }
      if (explainerTitleText) {
        explainerTitleText.textContent = 'Active Method: Nearest Neighbor Heuristic (Greedy Local Search)';
      }
      if (explainerCreditBadge) {
        explainerCreditBadge.textContent = 'Algorithm Lead: Shubham (AP25110070745)';
      }
      if (explainerDescText) {
        explainerDescText.textContent = 'A greedy local heuristic that starts at the fulfillment warehouse and iteratively visits the nearest unvisited delivery location in O(N²) time. Fast and deterministic, but susceptible to greedy traps where the courier is forced to take long return journeys.';
      }
      setLayerFilter('nn');
    }
  }

  // Leaflet Map Initialization and Rendering
  function initMap() {
    const mapElement = document.getElementById('routeMap');
    if (!mapElement) return;

    // Center on SRM AP / Amaravati / Vijayawada
    state.map = L.map('routeMap', {
      zoomControl: true,
      scrollWheelZoom: true
    }).setView([16.4624, 80.5064], 11);

    // Standard OpenStreetMap Tiles (100% Free, Zero API Key required)
    state.tileLayer = L.tileLayer('https://tile.openstreetmap.org/{z}/{x}/{y}.png', {
      attribution: '&copy; <a href="https://www.openstreetmap.org/copyright" target="_blank">OpenStreetMap</a> contributors',
      maxZoom: 19
    }).addTo(state.map);

    window.addEventListener('resize', () => {
      if (state.map) state.map.invalidateSize();
    });
  }

  function toggleMapTheme() {
    state.isDarkMapTheme = !state.isDarkMapTheme;
    const mapEl = document.getElementById('routeMap');
    if (state.isDarkMapTheme) {
      mapEl.classList.remove('map-normal-tiles');
      if (btnToggleMapTheme) btnToggleMapTheme.textContent = '🌙 Dark Map';
    } else {
      mapEl.classList.add('map-normal-tiles');
      if (btnToggleMapTheme) btnToggleMapTheme.textContent = '☀️ Normal Map';
    }
  }

  function updateMapRoutes(locations, bb, nn, matrix) {
    if (!state.map) return;

    // Reset any running simulation
    resetSimulation();

    // Clear existing layers and markers
    if (state.mapLayers.bb) state.map.removeLayer(state.mapLayers.bb);
    if (state.mapLayers.nn) state.map.removeLayer(state.mapLayers.nn);
    if (state.mapLayers.bikeMarker) state.map.removeLayer(state.mapLayers.bikeMarker);
    state.mapLayers.markers.forEach(m => state.map.removeLayer(m));
    state.mapLayers.markers = [];

    const latlngs = [];
    const chosenTour = (bb && bb.is_valid) ? bb : nn;
    const path = (chosenTour && chosenTour.path) ? chosenTour.path : [];

    // Map stop sequence order from optimal tour path
    const stopSequenceMap = {};
    const arrivalDistMap = {};
    if (path.length > 0) {
      for (let step = 0; step < path.length - 1; step++) {
        const from = path[step];
        const to = path[step + 1];
        stopSequenceMap[to] = step + 1;
        if (matrix && matrix[from] && matrix[from][to] >= 0) {
          arrivalDistMap[to] = matrix[from][to];
        }
      }
    }

    // Add Stop Markers
    locations.forEach((loc, idx) => {
      if (!loc.lat || !loc.lon) return;

      latlngs.push([loc.lat, loc.lon]);
      const isWarehouse = (idx === 0);
      const seqNum = stopSequenceMap[idx] || (isWarehouse ? 'W' : `${idx}`);
      const arrDist = arrivalDistMap[idx];
      const arrCost = (arrDist !== undefined) ? (arrDist * state.costModel.ratePerKm) : 0;
      const distBadge = (arrDist !== undefined && !isWarehouse) ? `+${arrDist.toFixed(2)} km (₹ ${arrCost.toFixed(1)})` : '';

      const iconHtml = `
        <div class="custom-stop-marker">
          <div class="stop-pin ${isWarehouse ? 'warehouse' : 'customer'}">
            <span class="stop-number">${isWarehouse ? '🏢' : seqNum}</span>
          </div>
          <div class="stop-label-bubble">
            ${isWarehouse ? 'Warehouse (Start & End)' : `Stop #${seqNum}: ${escapeHtml(truncate(loc.name, 12))}`}
            ${distBadge ? `<span style="color:#FBBF24; margin-left:4px;">(${distBadge})</span>` : ''}
          </div>
        </div>
      `;

      const customIcon = L.divIcon({
        className: 'custom-stop-divicon',
        html: iconHtml,
        iconSize: [130, 50],
        iconAnchor: [65, 32],
        popupAnchor: [0, -32]
      });

      const marker = L.marker([loc.lat, loc.lon], { icon: customIcon })
        .bindPopup(`
          <div style="color:#000; font-family:sans-serif; min-width:180px; padding:2px;">
            <strong style="color:${isWarehouse ? '#D97706' : '#0284C7'}; font-size:13px;">
              ${isWarehouse ? '🏢 Central Fulfillment Warehouse' : `📦 Delivery Stop #${seqNum}`}
            </strong>
            <p style="margin:6px 0 3px; font-weight:700; font-size:13px;">${escapeHtml(loc.name)}</p>
            ${distBadge ? `<div style="background:#FEF3C7; color:#92400E; padding:3px 6px; border-radius:4px; font-size:11px; font-weight:600; margin-bottom:4px;">Arrival Distance: ${arrDist.toFixed(2)} km (₹ ${arrCost.toFixed(2)})</div>` : ''}
            <span style="font-size:11px; color:#666;">Coordinates: ${loc.lat.toFixed(5)}, ${loc.lon.toFixed(5)}</span>
          </div>
        `);

      marker.addTo(state.map);
      state.mapLayers.markers.push(marker);
    });

    // 1. Draw Branch and Bound Real Road Route (Electric Cyan)
    if (bb && bb.is_valid && bb.geometry) {
      state.mapLayers.bb = L.geoJSON(bb.geometry, {
        style: {
          color: '#38BDF8',
          weight: 4.5,
          opacity: 0.95,
          lineCap: 'round',
          lineJoin: 'round'
        }
      });
      state.mapLayers.bb.bindPopup('<strong>Branch and Bound Optimal Road Route (Exact Minimum Cost)</strong>');
    }

    // 2. Draw Nearest Neighbor Real Road Route (Warm Amber)
    if (nn && nn.is_valid && nn.geometry) {
      state.mapLayers.nn = L.geoJSON(nn.geometry, {
        style: {
          color: '#F59E0B',
          weight: 4,
          opacity: 0.85,
          dashArray: '8, 8',
          lineCap: 'round',
          lineJoin: 'round'
        }
      });
      state.mapLayers.nn.bindPopup('<strong>Nearest Neighbor Heuristic Route (Greedy Approximation)</strong>');
    }

    applyLayerFilter();

    // 3. Build Turn-by-Turn Delivery Itinerary and Bike Route Points
    buildItineraryAndBikeRoute(locations, chosenTour, matrix);

    // Fit map bounds
    if (latlngs.length > 0) {
      state.map.fitBounds(L.latLngBounds(latlngs), { padding: [55, 55], maxZoom: 14 });
    }

    setTimeout(() => {
      if (state.map) state.map.invalidateSize();
    }, 250);
  }

  function setLayerFilter(filter) {
    state.activeLayerFilter = filter;
    document.querySelectorAll('.layer-btn').forEach(btn => btn.classList.remove('active'));

    if (filter === 'both') document.getElementById('btnToggleBoth').classList.add('active');
    if (filter === 'bb') document.getElementById('btnToggleBB').classList.add('active');
    if (filter === 'nn') document.getElementById('btnToggleNN').classList.add('active');

    applyLayerFilter();
  }

  function applyLayerFilter() {
    if (!state.map) return;

    if (state.mapLayers.bb) state.map.removeLayer(state.mapLayers.bb);
    if (state.mapLayers.nn) state.map.removeLayer(state.mapLayers.nn);

    if (state.activeLayerFilter === 'both' || state.activeLayerFilter === 'bb') {
      if (state.mapLayers.bb) state.mapLayers.bb.addTo(state.map);
    }
    if (state.activeLayerFilter === 'both' || state.activeLayerFilter === 'nn') {
      if (state.mapLayers.nn) state.mapLayers.nn.addTo(state.map);
    }
  }

  // Build Turn-by-Turn Delivery Itinerary & Setup Delivery Bike Simulation
  function buildItineraryAndBikeRoute(locations, tour, matrix) {
    if (!itineraryGrid || !tour || !tour.is_valid || !tour.path || tour.path.length < 2) {
      if (itineraryGrid) itineraryGrid.innerHTML = '<p style="color:var(--text-muted); font-size:0.85rem;">No valid route itinerary available.</p>';
      return;
    }

    const path = tour.path;
    const legs = [];
    let cumDist = 0.0;
    let cumCost = 0.0;
    const rate = state.costModel.ratePerKm;
    const handling = state.costModel.handlingPerStop;
    let itineraryHtml = '';

    for (let k = 0; k < path.length - 1; k++) {
      const fromIdx = path[k];
      const toIdx = path[k + 1];
      const fromName = locations[fromIdx] ? locations[fromIdx].name : `Loc ${fromIdx}`;
      const toName = locations[toIdx] ? locations[toIdx].name : `Loc ${toIdx}`;
      const legDist = (matrix && matrix[fromIdx] && matrix[fromIdx][toIdx] >= 0) ? matrix[fromIdx][toIdx] : 0.0;
      const legFuelCost = legDist * rate;
      const isReturn = (k === path.length - 2);
      const isStart = (k === 0);
      const legHandling = isReturn ? 0 : handling;
      const legTotalCost = legFuelCost + legHandling;

      cumDist += legDist;
      cumCost += legTotalCost;

      legs.push({
        legIndex: k,
        fromIdx,
        toIdx,
        fromName,
        toName,
        distance: legDist,
        legCost: legTotalCost,
        cumulativeDist: cumDist,
        cumulativeCost: cumCost,
        isReturn
      });

      itineraryHtml += `
        <div class="itinerary-leg-card" data-leg="${k}">
          <div class="leg-top-row">
            <span class="leg-number-badge">Leg ${k + 1} of ${path.length - 1}</span>
            <span class="leg-dist-badge">${legDist.toFixed(2)} km &bull; ₹ ${legTotalCost.toFixed(2)}</span>
          </div>
          <div class="leg-path-text">
            ${isStart ? '🏢' : '📦'} [${fromIdx}] ${escapeHtml(truncate(fromName, 18))}
            <span style="color:var(--accent-blue); margin: 0 4px;">&rarr;</span>
            ${isReturn ? '🏢' : '📦'} [${toIdx}] ${escapeHtml(truncate(toName, 18))}
          </div>
          <div class="leg-cum-text">
            Cumulative: <strong>${cumDist.toFixed(2)} km (₹ ${cumCost.toFixed(2)})</strong>
            ${isReturn ? ' &bull; <span style="color:#F59E0B; font-weight:700;">Return to Warehouse</span>' : ''}
          </div>
        </div>
      `;
    }

    itineraryGrid.innerHTML = itineraryHtml;
    if (itineraryTotalBadge) {
      itineraryTotalBadge.textContent = `${cumDist.toFixed(2)} km | ₹ ${cumCost.toFixed(2)} Total Cost`;
    }

    // Attach click listener on leg cards to highlight and jump
    itineraryGrid.querySelectorAll('.itinerary-leg-card').forEach(card => {
      card.addEventListener('click', () => {
        const legIdx = parseInt(card.dataset.leg, 10);
        highlightLeg(legIdx);
      });
    });

    // Extract Dense Coordinates for Delivery Bike Movement
    let routeCoords = [];
    if (tour.geometry && tour.geometry.coordinates && tour.geometry.coordinates.length > 0) {
      routeCoords = tour.geometry.coordinates.map(c => [c[1], c[0]]);
    } else {
      // Fallback: interpolate points between waypoints
      for (let k = 0; k < path.length - 1; k++) {
        const p1 = locations[path[k]];
        const p2 = locations[path[k + 1]];
        if (p1 && p2 && p1.lat && p2.lat) {
          const steps = 30;
          for (let s = 0; s <= steps; s++) {
            const frac = s / steps;
            routeCoords.push([
              p1.lat + (p2.lat - p1.lat) * frac,
              p1.lon + (p2.lon - p1.lon) * frac
            ]);
          }
        }
      }
    }

    state.simulation.legs = legs;
    state.simulation.routeCoords = routeCoords;
    state.simulation.currentCoordIndex = 0;

    // Create or position the Delivery Bike marker
    if (routeCoords.length > 0) {
      const bikeHtml = `
        <div class="delivery-bike-wrapper">
          <div class="bike-bubble">🛵</div>
          <div class="bike-pulse"></div>
        </div>
      `;

      const bikeIcon = L.divIcon({
        className: 'delivery-bike-marker',
        html: bikeHtml,
        iconSize: [44, 44],
        iconAnchor: [22, 22]
      });

      if (state.mapLayers.bikeMarker) {
        state.map.removeLayer(state.mapLayers.bikeMarker);
      }

      state.mapLayers.bikeMarker = L.marker(routeCoords[0], { icon: bikeIcon, zIndexOffset: 1000 }).addTo(state.map);
      state.mapLayers.bikeMarker.bindPopup(`<strong>🛵 Delivery Courier</strong><br>Departing from Warehouse. Ready for delivery simulation! Total: ₹ ${cumCost.toFixed(2)}`);

      if (simStatusText) {
        simStatusText.textContent = `🛵 Courier ready at Warehouse. Total delivery tour: ${cumDist.toFixed(2)} km (₹ ${cumCost.toFixed(2)}) across ${legs.length} legs. Click 'Play Delivery Run'.`;
      }
    }
  }

  function highlightLeg(legIdx) {
    if (!state.simulation.legs || !state.simulation.legs[legIdx]) return;
    const leg = state.simulation.legs[legIdx];

    document.querySelectorAll('.itinerary-leg-card').forEach((card, idx) => {
      if (idx === legIdx) card.classList.add('active-leg');
      else card.classList.remove('active-leg');
    });

    if (simStatusText) {
      simStatusText.innerHTML = `<strong>Leg ${legIdx + 1}:</strong> ${escapeHtml(leg.fromName)} &rarr; ${escapeHtml(leg.toName)} (${leg.distance.toFixed(2)} km &bull; ₹ ${leg.legCost.toFixed(2)})${leg.isReturn ? ' [Return to Depot]' : ''}`;
    }
  }

  // Delivery Bike Simulation Animation Loop
  function toggleSimulation() {
    if (!state.simulation.routeCoords || state.simulation.routeCoords.length === 0) {
      showToast('Please calculate an optimal route first to simulate the delivery courier run.', 'error');
      return;
    }

    if (state.simulation.isPlaying) {
      pauseSimulation();
    } else {
      startSimulation();
    }
  }

  function startSimulation() {
    state.simulation.isPlaying = true;
    if (simBtnLabel) simBtnLabel.textContent = 'Pause Delivery Run';
    if (simPlayIcon) {
      simPlayIcon.innerHTML = '<rect x="6" y="4" width="4" height="16"></rect><rect x="14" y="4" width="4" height="16"></rect>';
    }

    runAnimFrame();
  }

  function pauseSimulation() {
    state.simulation.isPlaying = false;
    if (state.simulation.animId) {
      cancelAnimationFrame(state.simulation.animId);
      state.simulation.animId = null;
    }
    if (simBtnLabel) simBtnLabel.textContent = 'Play Delivery Run';
    if (simPlayIcon) {
      simPlayIcon.innerHTML = '<polygon points="5 3 19 12 5 21 5 3"></polygon>';
    }
  }

  function resetSimulation() {
    pauseSimulation();
    state.simulation.currentCoordIndex = 0;
    if (state.simulation.routeCoords && state.simulation.routeCoords.length > 0 && state.mapLayers.bikeMarker) {
      state.mapLayers.bikeMarker.setLatLng(state.simulation.routeCoords[0]);
    }
    document.querySelectorAll('.itinerary-leg-card').forEach(c => c.classList.remove('active-leg'));
    if (simStatusText) {
      simStatusText.textContent = `Simulation reset to Warehouse origin. Click 'Play Delivery Run'.`;
    }
  }

  function runAnimFrame() {
    if (!state.simulation.isPlaying) return;

    const coords = state.simulation.routeCoords;
    const totalPoints = coords.length;

    state.simulation.currentCoordIndex += state.simulation.speed;

    if (state.simulation.currentCoordIndex >= totalPoints - 1) {
      state.simulation.currentCoordIndex = totalPoints - 1;
      state.mapLayers.bikeMarker.setLatLng(coords[state.simulation.currentCoordIndex]);
      pauseSimulation();

      const totalDist = state.simulation.legs.reduce((acc, l) => acc + l.distance, 0);
      const totalCost = state.simulation.legs.reduce((acc, l) => acc + l.legCost, 0);
      if (simStatusText) {
        simStatusText.innerHTML = `🏁 <strong>Delivery tour completed!</strong> Returned safely to Warehouse. Total: <strong>${totalDist.toFixed(2)} km</strong> | Cost: <strong>₹ ${totalCost.toFixed(2)}</strong>.`;
      }
      showToast('Delivery tour completed! Returned to Central Warehouse.', 'success');
      return;
    }

    const currentPos = coords[state.simulation.currentCoordIndex];
    state.mapLayers.bikeMarker.setLatLng(currentPos);

    const frac = state.simulation.currentCoordIndex / totalPoints;
    const totalLegs = state.simulation.legs.length;
    const currentLegIdx = Math.min(Math.floor(frac * totalLegs), totalLegs - 1);
    const leg = state.simulation.legs[currentLegIdx];

    if (leg) {
      highlightLeg(currentLegIdx);
      if (simStatusText) {
        simStatusText.innerHTML = `🛵 <strong>In Transit (Leg ${currentLegIdx + 1}/${totalLegs}):</strong> Heading to [${leg.toIdx}] <strong>${escapeHtml(truncate(leg.toName, 18))}</strong> (${leg.distance.toFixed(2)} km &bull; ₹ ${leg.legCost.toFixed(2)})...`;
      }
    }

    state.simulation.animId = requestAnimationFrame(runAnimFrame);
  }

  // SQLite History Management
  async function fetchHistory() {
    try {
      const res = await fetch('/api/history');
      const data = await res.json();
      renderHistoryTable(data);
    } catch {
      historyTable.querySelector('tbody').innerHTML = `
        <tr><td colspan="7" style="text-align:center; color:var(--text-muted);">Database records unavailable.</td></tr>
      `;
    }
  }

  function renderHistoryTable(records) {
    const tbody = historyTable.querySelector('tbody');
    if (!records || records.length === 0) {
      tbody.innerHTML = `<tr><td colspan="7" style="text-align:center; color:var(--text-muted); padding:1.5rem;">No previous delivery plans stored in SQLite database.</td></tr>`;
      return;
    }

    tbody.innerHTML = '';
    records.forEach(rec => {
      const row = document.createElement('tr');
      const rate = state.costModel.ratePerKm;
      const bbDist = rec.bb_distance !== null ? `${rec.bb_distance.toFixed(2)} km (₹ ${(rec.bb_distance * rate).toFixed(0)})` : 'N/A';
      const nnDist = rec.nn_distance !== null ? `${rec.nn_distance.toFixed(2)} km (₹ ${(rec.nn_distance * rate).toFixed(0)})` : 'N/A';

      row.innerHTML = `
        <td><strong style="color:var(--accent-blue); font-family:monospace;">#${rec.plan_id}</strong></td>
        <td><strong>${escapeHtml(rec.source_location)}</strong></td>
        <td>${rec.total_locations} Stops</td>
        <td><span style="color:#38BDF8; font-weight:600;">${bbDist}</span></td>
        <td><span style="color:#F59E0B; font-weight:600;">${nnDist}</span></td>
        <td style="color:var(--text-muted); font-size:0.8rem;">${escapeHtml(rec.created_at)}</td>
        <td>
          <button type="button" class="btn btn-secondary btn-sm btn-load-plan" data-id="${rec.plan_id}">
            View
          </button>
        </td>
      `;
      tbody.appendChild(row);
    });

    tbody.querySelectorAll('.btn-load-plan').forEach(btn => {
      btn.addEventListener('click', async (e) => {
        const id = e.currentTarget.dataset.id;
        await loadSavedPlan(id);
      });
    });
  }

  async function loadSavedPlan(planId) {
    try {
      const res = await fetch(`/api/history/${planId}`);
      if (!res.ok) throw new Error('Failed to load plan details.');
      const data = await res.json();

      if (!data.locations || data.locations.length < 2) {
        throw new Error('Corrupted plan location data.');
      }

      startingInput.value = data.locations[0].name;
      state.destinations = [];
      for (let i = 1; i < data.locations.length; i++) {
        state.destinations.push({
          name: data.locations[i].name,
          lat: data.locations[i].lat,
          lon: data.locations[i].lon,
          display_name: data.locations[i].name
        });
      }
      renderDestinationInputs();

      state.matrix = data.matrix;
      renderMatrixTable(data.locations.map(l => l.name), state.matrix);
      matrixStatusBadge.textContent = `Loaded Past Delivery Plan #${planId}`;
      matrixStatusBadge.style.color = 'var(--accent-blue)';

      handleCalculateOptimal();
      showToast(`Restored Delivery Plan #${planId} from SQLite database.`, 'success');
    } catch (err) {
      showToast(err.message, 'error');
    }
  }

  // Interactive Hero Network Canvas
  function initNetworkCanvas() {
    const canvas = document.getElementById('networkCanvas');
    if (!canvas) return;
    const ctx = canvas.getContext('2d');

    function resizeCanvas() {
      const rect = canvas.getBoundingClientRect();
      const pixelRatio = window.devicePixelRatio || 1;
      canvas.width = Math.max(1, Math.floor(rect.width * pixelRatio));
      canvas.height = Math.max(1, Math.floor(rect.height * pixelRatio));
      ctx.setTransform(pixelRatio, 0, 0, pixelRatio, 0, 0);
    }

    resizeCanvas();
    window.addEventListener('resize', resizeCanvas);

    const nodes = [
      { x: 100, y: 130, label: 'Warehouse [0]' },
      { x: 280, y: 60, label: 'North Depot [1]' },
      { x: 440, y: 140, label: 'East Terminal [2]' },
      { x: 340, y: 220, label: 'South Depot [3]' },
      { x: 180, y: 210, label: 'West Hub [4]' }
    ];

    let t = 0;
    function render() {
      const w = canvas.getBoundingClientRect().width;
      const h = canvas.getBoundingClientRect().height;
      ctx.clearRect(0, 0, w, h);

      // Keep the network centered and fully visible on narrow screens.
      const scale = Math.min(w / 520, h / 280, 1);
      ctx.save();
      ctx.translate((w - (520 * scale)) / 2, (h - (280 * scale)) / 2);
      ctx.scale(scale, scale);

      t += 0.015;

      // Draw Edges with Directed Arrows
      for (let i = 0; i < nodes.length; i++) {
        const u = nodes[i];
        const v = nodes[(i + 1) % nodes.length];

        ctx.beginPath();
        ctx.moveTo(u.x, u.y);
        ctx.lineTo(v.x, v.y);
        ctx.strokeStyle = 'rgba(56, 189, 248, 0.4)';
        ctx.lineWidth = 2;
        ctx.stroke();

        // Pulsing courier particle along cycle
        const pt = (t + i * 0.2) % 1.0;
        const px = u.x + (v.x - u.x) * pt;
        const py = u.y + (v.y - u.y) * pt;

        ctx.beginPath();
        ctx.arc(px, py, 4, 0, Math.PI * 2);
        ctx.fillStyle = '#F59E0B';
        ctx.shadowColor = '#F59E0B';
        ctx.shadowBlur = 8;
        ctx.fill();
        ctx.shadowBlur = 0;
      }

      // Draw Nodes
      nodes.forEach((node, idx) => {
        const isWarehouse = (idx === 0);
        ctx.beginPath();
        ctx.arc(node.x, node.y, isWarehouse ? 9 : 7, 0, Math.PI * 2);
        ctx.fillStyle = isWarehouse ? '#F59E0B' : '#38BDF8';
        ctx.shadowColor = isWarehouse ? 'rgba(245, 158, 11, 0.8)' : 'rgba(56, 189, 248, 0.8)';
        ctx.shadowBlur = 12;
        ctx.fill();
        ctx.shadowBlur = 0;

        ctx.strokeStyle = '#FFFFFF';
        ctx.lineWidth = 1.5;
        ctx.stroke();

        ctx.fillStyle = '#CBD5E1';
        ctx.font = '10px Inter, sans-serif';
        ctx.fillText(node.label, node.x - 25, node.y - 12);
      });

      ctx.restore();

      requestAnimationFrame(render);
    }

    render();
  }

  // Toast Notification System
  function showToast(message, type = 'info', duration = 3500) {
    if (!toastContainer) return;
    const toast = document.createElement('div');
    toast.className = `toast ${type}`;
    toast.innerHTML = `
      <div style="display:flex; align-items:center; gap:0.6rem;">
        <span>${escapeHtml(message)}</span>
      </div>
    `;
    toastContainer.appendChild(toast);

    setTimeout(() => {
      toast.style.opacity = '0';
      toast.style.transform = 'translateY(10px)';
      toast.style.transition = 'all 0.25s ease';
      setTimeout(() => toast.remove(), 250);
    }, duration);
  }

  function truncate(str, max) {
    if (!str) return '';
    return str.length > max ? str.substring(0, max - 1) + '…' : str;
  }

  function escapeHtml(str) {
    if (!str) return '';
    return str.replace(/&/g, '&amp;')
              .replace(/</g, '&lt;')
              .replace(/>/g, '&gt;')
              .replace(/"/g, '&quot;')
              .replace(/'/g, '&#039;');
  }
});
