// Replace this with your backend URL when deploying.
const API_BASE_URL = 'http://127.0.0.1:8000';
let map = null;

document.addEventListener("DOMContentLoaded", () => {
  setupAutocomplete("source", "source_iata", "source-dropdown");
  setupAutocomplete("destination", "destination_iata", "destination-dropdown");
});

async function planFlight() {
  const start = performance.now();
  const resultElement = document.getElementById('result');
  const loadingElement = document.getElementById('loading');

  const sourceIata = document.getElementById('source_iata').value || document.getElementById('source').value.toUpperCase().substring(0, 3);
  const destIata = document.getElementById('destination_iata').value || document.getElementById('destination').value.toUpperCase().substring(0, 3);

  const prefsInput = document.getElementById('preferred_airlines') ? document.getElementById('preferred_airlines').value.trim() : "";
  const prefAirlines = prefsInput ? prefsInput.split(',').map(s => s.trim().toUpperCase()) : null;
  const maxLayoversInput = document.getElementById('max_layovers') ? document.getElementById('max_layovers').value : "";
  const maxLayovers = maxLayoversInput !== "" ? parseInt(maxLayoversInput) : null;
  const maxResultsInput = document.getElementById('max_results') ? document.getElementById('max_results').value : "3";
  const maxResults = parseInt(maxResultsInput);
  const tripType = document.getElementById('trip_type') ? document.getElementById('trip_type').value : "one-way";
  const returnDate = tripType === "round-trip" ? document.getElementById('return_date').value : null;

  const windowType = document.getElementById('departure_window') ? document.getElementById('departure_window').value : "anytime";
  let winStart = "00:00:00";
  let winEnd = "23:59:59";
  
  if (windowType === "morning") {
      winStart = "04:00:00";
      winEnd = "12:00:00";
  } else if (windowType === "afternoon") {
      winStart = "12:00:00";
      winEnd = "18:00:00";
  } else if (windowType === "evening") {
      winStart = "18:00:00";
      winEnd = "23:59:59";
  } else if (windowType === "custom") {
      winStart = document.getElementById('custom_start_time').value + ":00";
      winEnd = document.getElementById('custom_end_time').value + ":00";
  }

  const maxDuration = parseInt(document.getElementById('max_duration_hours').value) || 48;
  const maxPrice = parseFloat(document.getElementById('max_price').value) || 10000.0;

  const input = {
    trip_type: tripType,
    source: sourceIata,
    destination: destIata,
    departure_date: document.getElementById('departure_date').value,
    return_date: returnDate,
    departure_window_start: winStart,
    departure_window_end: winEnd,
    search_mode: document.getElementById("mode").value,
    adults: 1,
    max_layovers: maxLayovers,
    preferred_airlines: prefAirlines,
    max_results: maxResults,
    max_duration_hours: maxDuration,
    max_price: maxPrice
  };

  if (!input.source || !input.destination || !input.departure_date || (tripType === "round-trip" && !input.return_date)) {
    showToast("Please fill in all required fields.", "error");
    return;
  }

  loadingElement.style.display = 'block';
  resultElement.innerHTML = '';
  document.getElementById('map').style.display = 'none';

  try {
    const response = await fetch(`${API_BASE_URL}/plan`, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json'
      },
      body: JSON.stringify(input)
    });
    if (!response.ok) {
      const errText = await response.text();
      throw new Error(`Server error: ${response.status} - ${errText}`);
    }

    const data = await response.json();

    if (data.outbound && data.outbound.length > 0) {
      const header = document.createElement('h2');
      header.textContent = `Outbound (${data.outbound[0].source} to ${data.outbound[0].destination})`;
      resultElement.appendChild(header);

      let allMapFlights = [];

      data.outbound.forEach((itinerary, index) => {
          const label = createOptionLabel(index, input.search_mode);
          resultElement.appendChild(label);
          resultElement.appendChild(createItineraryCard(itinerary));
          
          if (index === 0) {
              allMapFlights = [...itinerary.flights];
          }
      });

      if (data.return_flight && data.return_flight.length > 0) {
        const retHeader = document.createElement('h2');
        retHeader.textContent = `Return (${data.return_flight[0].source} to ${data.return_flight[0].destination})`;
        retHeader.style.marginTop = '20px';
        resultElement.appendChild(retHeader);

        data.return_flight.forEach((itinerary, index) => {
            const label = createOptionLabel(index, input.search_mode);
            resultElement.appendChild(label);
            resultElement.appendChild(createItineraryCard(itinerary));
            
            if (index === 0) {
                allMapFlights = allMapFlights.concat(itinerary.flights);
            }
        });
      }

      // Draw map overlay for the best (first) routes
      drawMap(allMapFlights);
    }
    else {
      showToast("No routes found matching your criteria. Try adjusting filters.", "warning");
    }
  } catch (error) {
    console.error(error);
    showToast("Failed to load flights: " + error.message, "error");
  } finally {
    loadingElement.style.display = 'none';
  }
  const end = performance.now();
  console.log(`Total Frontend-to-Backend-to-UI time: ${(end - start) / 1000}s`);
}

function formatDuration(seconds) {
  let minutes = seconds / 60;
  const hours = Math.floor(minutes / 60);
  const mins = Math.floor(minutes % 60);
  const formattedMins = String(mins).padStart(2, '0');
  return `${hours}h ${formattedMins}m`;
}

function toggleReturnDate() {
  const type = document.getElementById('trip_type').value;
  document.getElementById('return_date_group').style.display = type === 'round-trip' ? 'block' : 'none';
}

function toggleCustomWindow() {
  const windowType = document.getElementById('departure_window').value;
  document.getElementById('custom_window_group').style.display = windowType === 'custom' ? 'block' : 'none';
}

// The badge on the first result names what the ranking actually optimised for.
// Every mode minimises duration + price_weight * price, so no mode returns the
// strictly cheapest or strictly quickest itinerary -- frugal at 1000 s/$ will
// still prefer a direct flight over one $9 cheaper but three hours longer, since
// $9 only buys 2h30m. These say which term dominates, which is the true claim.
const MODE_BADGES = {
  frugal: 'Price First',
  fast: 'Time First',
  balanced: 'Best Value'
};

function createOptionLabel(index, searchMode) {
  const label = document.createElement('h3');
  label.textContent = `Option ${index + 1}`;
  label.style.marginTop = '15px';
  label.style.marginBottom = '5px';
  label.style.fontSize = '1.1rem';
  label.style.color = 'var(--text-secondary, #6b7280)';

  if (index === 0) {
    const badge = MODE_BADGES[searchMode] || MODE_BADGES.balanced;
    label.innerHTML += ` <span class="result-badge">${badge}</span>`;
  }
  return label;
}

function createItineraryCard(data) {
  const div = document.createElement('div');
  div.className = 'itinerary-card';

  const totalDurationStr = formatDuration(data.total_duration);
  const layovers = data.flights.length - 1;
  const stopsText = layovers === 0 ? "Non-stop" : `${layovers} Stop${layovers > 1 ? 's' : ''}`;

  let segmentsHtml = '';
  for (let i = 0; i < data.flights.length; i++) {
    const flight = data.flights[i];
    segmentsHtml += `
      <div class="segment-row">
        <div class="segment-airline">${flight.airline} ${flight.flight_no}</div>
        <div class="segment-path">
          <div class="timepoint">
            <span class="airport">${flight.from}</span>
            <span class="segment-time">${flight.departure}</span>
          </div>
          <div class="segment-duration-line">
            <span class="segment-duration-text">${formatDuration(flight.duration)}</span>
          </div>
          <div class="timepoint">
            <span class="airport">${flight.to}</span>
            <span class="segment-time">${flight.arrival}</span>
          </div>
        </div>
      </div>
    `;

    // Calculate layover duration between this flight and the next
    if (i < data.flights.length - 1) {
      const nextFlight = data.flights[i + 1];
      // Measure the layover on the UTC epochs, never on the displayed strings. Those
      // are local wall clock at the connecting airport, so if the clocks move while
      // the passenger waits -- a DST boundary during the connection -- their
      // difference is off by exactly the hour that was added or removed.
      const layoverSec = nextFlight.departure_utc - flight.arrival_utc;
      let layoverStr = "";
      if (Number.isFinite(layoverSec) && layoverSec > 0) {
        layoverStr = ` (${formatDuration(layoverSec)})`;
      }
      segmentsHtml += `<div class="layover-row">Connection at ${flight.to}${layoverStr}</div>`;
    }
  }

  div.innerHTML = `
    <div class="itinerary-header">
      <div class="itinerary-summary-info">
        <span>${stopsText}</span>
        <span>Total: ${totalDurationStr}</span>
      </div>
      <div class="itinerary-total-price">$${data.total_cost.toFixed(2)}</div>
    </div>
    <div class="segment-list">
      ${segmentsHtml}
    </div>
  `;

  return div;
}

function setupAutocomplete(inputId, hiddenId, dropdownId) {
  const input = document.getElementById(inputId);
  const hidden = document.getElementById(hiddenId);
  const dropdown = document.getElementById(dropdownId);
  let timeout = null;

  input.addEventListener("input", (e) => {
    clearTimeout(timeout);
    const query = e.target.value;

    if (query.length < 2) {
      dropdown.style.display = "none";
      return;
    }

    timeout = setTimeout(async () => {
      try {
        const response = await fetch(`${API_BASE_URL}/airports/search?q=${encodeURIComponent(query)}`);
        const results = await response.json();

        dropdown.innerHTML = "";
        if (results.length > 0) {
          results.forEach(airport => {
            const item = document.createElement("div");
            item.className = "dropdown-item";
            item.innerHTML = `
              <span class="dropdown-iata">${airport.iata}</span>
              <span class="dropdown-name">${airport.name}, ${airport.city} (${airport.country})</span>
            `;
            item.addEventListener("click", () => {
              input.value = `${airport.iata} - ${airport.name}`;
              hidden.value = airport.iata;
              dropdown.style.display = "none";
            });
            dropdown.appendChild(item);
          });
          dropdown.style.display = "block";
        } else {
          dropdown.style.display = "none";
        }
      } catch (err) {
        console.error("Autocomplete error:", err);
      }
    }, 300); // Debounce 300ms
  });

  // Hide dropdown when clicking outside
  document.addEventListener("click", (e) => {
    if (e.target !== input && e.target !== dropdown && !dropdown.contains(e.target)) {
      dropdown.style.display = "none";
    }
  });
}

function showToast(message, type = "info") {
  const toastContainer = document.getElementById("toast-container");
  if (!toastContainer) return;

  const toast = document.createElement("div");
  toast.className = `toast toast-${type}`;

  const icon = document.createElement("span");
  icon.className = "toast-icon";
  icon.innerHTML = type === "error" ? "!" : type === "warning" ? "!" : "i";

  const text = document.createElement("span");
  text.textContent = message;

  toast.appendChild(icon);
  toast.appendChild(text);
  toastContainer.appendChild(toast);

  // Trigger reflow for animation
  void toast.offsetWidth;
  toast.classList.add("show");

  setTimeout(() => {
    toast.classList.remove("show");
    setTimeout(() => {
      if (toastContainer.contains(toast)) {
        toastContainer.removeChild(toast);
      }
    }, 300); // Wait for fade out
  }, 4000);
}

// Leaflet draws a polyline as straight segments in the projected plane, which is
// not the path an aircraft flies and not the path the C++ engine's great-circle
// heuristic assumes when it estimates remaining cost. Interpolating along the
// great circle makes the drawing agree with the algorithm. The difference is
// invisible on a short domestic hop and pronounced on long or northerly routes.
const GEODESIC_STEPS = 48;

function toRadians(degrees) {
  return degrees * Math.PI / 180;
}

function toDegrees(radians) {
  return radians * 180 / Math.PI;
}

// Points along the great circle from start to end, endpoints included.
function greatCirclePoints(start, end, steps = GEODESIC_STEPS) {
  const lat1 = toRadians(start[0]);
  const lon1 = toRadians(start[1]);
  const lat2 = toRadians(end[0]);
  const lon2 = toRadians(end[1]);

  // Angular separation, by the same haversine the engine uses for distance.
  const h = Math.sin((lat2 - lat1) / 2) ** 2 +
    Math.cos(lat1) * Math.cos(lat2) * Math.sin((lon2 - lon1) / 2) ** 2;
  const d = 2 * Math.asin(Math.min(1, Math.sqrt(h)));

  // Coincident airports, or a missing coordinate, leave nothing to interpolate
  // and would divide by sin(0) below.
  if (!Number.isFinite(d) || d < 1e-9) {
    return [start, end];
  }

  const points = [];
  for (let i = 0; i <= steps; i++) {
    const f = i / steps;
    const a = Math.sin((1 - f) * d) / Math.sin(d);
    const b = Math.sin(f * d) / Math.sin(d);
    const x = a * Math.cos(lat1) * Math.cos(lon1) + b * Math.cos(lat2) * Math.cos(lon2);
    const y = a * Math.cos(lat1) * Math.sin(lon1) + b * Math.cos(lat2) * Math.sin(lon2);
    const z = a * Math.sin(lat1) + b * Math.sin(lat2);
    points.push([
      toDegrees(Math.atan2(z, Math.hypot(x, y))),
      toDegrees(Math.atan2(y, x))
    ]);
  }
  return points;
}

// One continuous great-circle path through every waypoint of the itinerary.
function geodesicPath(waypoints) {
  const path = [];
  for (let i = 0; i < waypoints.length - 1; i++) {
    const segment = greatCirclePoints(waypoints[i], waypoints[i + 1]);
    // Each segment repeats the previous segment's final point; drop the duplicate.
    path.push(...(i === 0 ? segment : segment.slice(1)));
  }

  // A route crossing the antimeridian produces longitudes that jump from +179 to
  // -179. Leaflet would draw that jump as a line racing back across the whole
  // map, so keep the sequence continuous by letting longitude run past +-180.
  // Adjustments accumulate because each step compares against the fixed previous.
  for (let i = 1; i < path.length; i++) {
    const delta = path[i][1] - path[i - 1][1];
    if (delta > 180) {
      path[i][1] -= 360;
    } else if (delta < -180) {
      path[i][1] += 360;
    }
  }
  return path;
}

function drawMap(flights) {
  const mapElement = document.getElementById('map');
  mapElement.style.display = 'block';

  // Destroy previous map instance if exists
  if (map !== null) {
    map.remove();
  }

  // Init map
  map = L.map('map');

  // Add OpenStreetMap tiles
  L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
    maxZoom: 19,
    attribution: '© OpenStreetMap contributors'
  }).addTo(map);

  const latlngs = [];

  // Custom icons (simple colored dots for airports)
  const waypointIcon = L.divIcon({
    className: 'custom-div-icon',
    html: "<div style='background-color:#4f46e5; width:12px; height:12px; border-radius:50%; border:2px solid white; box-shadow: 0 0 4px rgba(0,0,0,0.4);'></div>",
    iconSize: [12, 12],
    iconAnchor: [6, 6]
  });

  for (let i = 0; i < flights.length; i++) {
    const f = flights[i];

    // Add source pin
    const sourcePt = [f.from_lat, f.from_lon];
    latlngs.push(sourcePt);
    L.marker(sourcePt, { icon: waypointIcon }).addTo(map)
      .bindPopup(`<b>${f.from}</b>`);

    // Add destination pin
    const destPt = [f.to_lat, f.to_lon];
    if (i === flights.length - 1) { // Final destination
      latlngs.push(destPt);
      L.marker(destPt, { icon: waypointIcon }).addTo(map)
        .bindPopup(`<b>${f.to}</b>`);
    }
  }

  // Draw lines connecting them
  const polyline = L.polyline(geodesicPath(latlngs), {
    color: '#4f46e5',
    weight: 3,
    opacity: 0.7,
    dashArray: '8, 8'
  }).addTo(map);

  // Auto-zoom map to fit all points
  map.fitBounds(polyline.getBounds(), { padding: [50, 50] });
}
