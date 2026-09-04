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
          const label = document.createElement('h3');
          label.textContent = `Option ${index + 1}`;
          label.style.marginTop = '15px';
          label.style.marginBottom = '5px';
          label.style.fontSize = '1.1rem';
          label.style.color = 'var(--text-secondary, #6b7280)';
          if (index === 0) {
              label.innerHTML += ' <span style="font-size: 0.8rem; background: var(--primary-color, #4f46e5); color: white; padding: 2px 6px; border-radius: 4px; margin-left: 10px; vertical-align: middle;">Best Value</span>';
          }
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
            const label = document.createElement('h3');
            label.textContent = `Option ${index + 1}`;
            label.style.marginTop = '15px';
            label.style.marginBottom = '5px';
            label.style.fontSize = '1.1rem';
            label.style.color = 'var(--text-secondary, #6b7280)';
            if (index === 0) {
                label.innerHTML += ' <span style="font-size: 0.8rem; background: var(--primary-color, #4f46e5); color: white; padding: 2px 6px; border-radius: 4px; margin-left: 10px; vertical-align: middle;">Best Value</span>';
            }
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

// Parses a backend "YYYY-MM-DD HH:MM:SS" wall-clock string on a fixed UTC offset.
// The result is NOT the real instant -- it is only meaningful when differenced against
// another string from the same timezone, which is what layover math needs. Reading it
// as UTC keeps the browser's own timezone (and its DST jumps) out of the arithmetic.
function parseLocalString(s) {
  if (typeof s !== "string") return NaN;
  return Date.parse(s.trim().replace(" ", "T") + "Z");
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
      // A connection happens at a single airport (flight.to === nextFlight.from), so
      // flight.arrival and nextFlight.departure are both wall-clock strings in that one
      // airport's timezone. Parsing them on a common fixed offset makes the difference
      // the true elapsed layover, independent of whichever timezone the viewer is in.
      const layoverMs = parseLocalString(nextFlight.departure) - parseLocalString(flight.arrival);
      let layoverStr = "";
      if (!isNaN(layoverMs) && layoverMs > 0) {
        layoverStr = ` (${formatDuration(layoverMs / 1000)})`;
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
  const polyline = L.polyline(latlngs, {
    color: '#4f46e5',
    weight: 3,
    opacity: 0.7,
    dashArray: '8, 8'
  }).addTo(map);

  // Auto-zoom map to fit all points
  map.fitBounds(polyline.getBounds(), { padding: [50, 50] });
}
