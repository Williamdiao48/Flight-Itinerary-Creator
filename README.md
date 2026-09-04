# Flight Itinerary Creator

A flight search engine that plans multi-leg itineraries with A\* pathfinding.
Live offers come from the Duffel API; the routing itself runs in C++ compiled
into the Python process, and a browser front end renders the results on a map.

Rather than showing whatever an airline API returns, it treats the segments as a
graph and searches for genuinely good combinations — including connections the
provider never offers as a single bookable route.

---

## How it works

Three layers, each doing the part it is best at:

```
frontend/          vanilla JS, no framework
  index.html       search form, itinerary cards, Leaflet map
  app.js           calls the API, renders results
        |
        |  HTTP (JSON)
        v
backend/           FastAPI
  main.py          /plan and /airports/search endpoints
  services.py      Duffel fetch, caching, timezone conversion
  models.py        pydantic request/response schemas
        |
        |  pybind11, in-process (no subprocess, no serialisation)
        v
backend/cpp/       the search engine
  tp.cpp           A* over flight segments
  fm.cpp           flights indexed by airport in a BST keyed on departure time
  bstset.h         hand-rolled BST set
  provided.cpp     haversine distance between airports
```

### The search

Each flight segment is an edge; the planner runs A\* over them.

- **g(n)** — elapsed travel time so far, plus layover time, plus
  `price × price_weight`.
- **h(n)** — great-circle distance from the current airport to the destination
  divided by 500 mph, an admissible estimate of the remaining flight time.

`price_weight` is what the search mode actually changes. It sets the exchange
rate between a dollar and a second, which is the only real question in flight
search:

| Mode | `price_weight` | Behaviour |
|---|---|---|
| `frugal` | 1000 | A dollar saved is worth a long detour |
| `balanced` | 300 | Default |
| `fast` | 1 | Price is nearly irrelevant; minimise time |

Flights are stored per-airport in a BST ordered by departure time, so finding
the onward connections from an airport within a time window is a range scan
rather than a filter over every flight.

Two details worth knowing if you read the code:

- Nodes track real duration and price totals *separately* from the A\* cost. The
  blended cost mixes seconds with `price × price_weight` and is comparable to
  neither the duration cap nor the price cap, so the constraints prune on the
  real totals.
- The dominance check (`bestKnownCostToAirport`) that keeps the search tractable
  is deliberately **not** applied at the destination. Applying it there would
  discard every route after the first and make `max_results` meaningless.

---

## Requirements

- Python 3.11
- A C++14 compiler (`clang++` on macOS, `g++` on Linux)
- A Duffel API key — free test keys at <https://app.duffel.com/join>

## Setup

```bash
# 1. Virtual environment
python3.11 -m venv venv
source venv/bin/activate          # Windows: venv\Scripts\activate

# 2. Python dependencies
pip install -r requirements.txt

# 3. Build the C++ extension (pybind11 is fetched automatically)
cd backend
python setup.py build_ext --inplace
cd ..

# 4. Credentials
cp .env.example .env
# then edit .env and set DUFFEL_API_KEY
```

Step 3 produces `backend/flight_planner_cpp.*.so`. It is platform- and
Python-version-specific and is deliberately not committed, so **rebuild it after
changing anything under `backend/cpp/`.**

## Running

Two processes. The API:

```bash
cd backend
uvicorn main:app --reload          # http://127.0.0.1:8000
```

And the front end, from a second terminal:

```bash
cd frontend
python3 -m http.server 5500        # http://127.0.0.1:5500
```

Serve the front end rather than opening `index.html` from disk — a `file://`
page has a null origin and the API calls will be blocked.

The API base URL is hardcoded at the top of `frontend/app.js`; change it there
if you move the backend off port 8000.

---

## API

### `GET /airports/search?q=<query>`

Airport autocomplete over the `airportsdata` IATA table. Returns at most 10
results, ranked by match quality: exact IATA code first, then code prefix, then
city, then name, then substring — with "international" promoted and municipal,
regional, county and military fields demoted, since a query is usually a city
name and the plain match otherwise puts small airfields above the intended
airport.

```json
[{ "iata": "DEN", "name": "Denver International Airport", "city": "Denver", "country": "US" }]
```

### `POST /plan`

| Field | Type | Default | Notes |
|---|---|---|---|
| `source` | string | — | IATA code |
| `destination` | string | — | IATA code |
| `departure_date` | string | — | `YYYY-MM-DD` |
| `search_mode` | string | — | `frugal` \| `balanced` \| `fast` |
| `trip_type` | string | `one-way` | `one-way` \| `round-trip` |
| `return_date` | string | `null` | Required for `round-trip` |
| `departure_window_start` | string | `00:00:00` | Local time at the origin |
| `departure_window_end` | string | `23:59:59` | Local time at the origin |
| `origin_timezone` | string | `null` | Resolved from the airport if omitted |
| `max_layovers` | int | `null` | `0` = non-stop only |
| `preferred_airlines` | list | `null` | IATA carrier codes, e.g. `["DL","UA"]` |
| `max_results` | int | `3` | Itineraries per direction |
| `max_duration_hours` | int | `48` | |
| `max_price` | float | `100000.0` | Per direction |
| `adults` | int | `1` | Accepted but not yet honoured — see below |

Returns `outbound` and `return_flight` itinerary lists:

```json
{
  "outbound": [{
    "source": "LAX",
    "destination": "JFK",
    "total_duration": 21600,
    "total_cost": 500.0,
    "flights": [{
      "airline": "AA", "flight_no": 300,
      "from": "LAX", "to": "JFK",
      "from_lat": 33.9425, "from_lon": -118.40805,
      "to_lat": 40.63993, "to_lon": -73.77869,
      "departure": "2026-06-01 08:00:00",
      "arrival": "2026-06-01 17:00:00",
      "duration": 21600, "price": 500.0
    }]
  }],
  "return_flight": null
}
```

Durations are in seconds. **`departure` and `arrival` are local wall-clock times
at their own airport**, not a shared timezone. The example above shows why that
matters: 08:00 to 17:00 reads as nine hours, but `duration` is 21600 seconds --
six hours -- because the departure is Pacific and the arrival is Eastern. Do not
subtract the two strings unless they come from the same airport. Internally
everything is UTC epochs; the conversion happens on the way out.

Results are cached in memory for one hour per route and date, since Duffel bills
per request and adjusting filters re-searches the same route.

---

## Standalone C++ CLI

The engine also builds as a binary that reads flights from a CSV, useful for
testing the search without an API key.

You supply the CSV — no sample is committed. The columns are
`airline,flight_no,source,destination,departure,arrival,duration,price`, with
times as UTC epochs. Save this as `sample.csv` to try it:

```
F9,2858,SFO,LAX,1769824500,1769829960,5460,76.64
F9,4158,SFO,LAS,1769806260,1769812320,6060,43.06
F9,1183,LAS,LAX,1769816520,1769821500,4980,43.06
```

```bash
cd backend/cpp
clang++ -std=c++14 -o planner main.cpp tp.cpp fm.cpp provided.cpp
./planner sample.csv SFO LAX 1769800000 airports.txt frugal
```

Arguments are: CSV path, origin, destination, start time as a UTC epoch, the
airport coordinate table, and the search mode — all six are required. It prints
the best itinerary as JSON, here the direct SFO-LAX at $76.64 rather than the
two-leg via LAS at $86.12.

The `arrival` column is parsed and discarded; `FlightSegment` stores departure
plus duration and derives arrival. It is in the format because this CSV was
originally written by the Python layer, before the pybind11 bindings removed
that round-trip.

---

## Project layout

```
backend/
  main.py, services.py, models.py    API, provider, schemas
  setup.py                           builds the C++ extension
  utils.py                           unused; see notes below
  cpp/
    tp.*, fm.*, bstset.h             the search engine
    provided.*                       base classes and haversine distance
    pybind_wrapper.cpp               Python bindings
    main.cpp                         standalone CLI
    airports.txt                     IATA coordinates for the heuristic
frontend/
  index.html, app.js, styles.css
```

`backend/cpp/airports.txt` is regenerated from `airportsdata` on first run if
missing, so a fresh checkout needs no manual step.

## Notes and known gaps

- **`adults` is ignored.** The field is accepted, but the Duffel request
  hardcodes a single adult passenger. Group pricing is not implemented.
- **`backend/utils.py` is dead code.** `write_flights_to_csv` served the old
  design, where flights were written to disk and the planner ran as a
  subprocess. The pybind11 bindings removed that boundary. Its C++ counterpart,
  `FlightManager::load_flight_data`, is still live — the CLI above uses it.
- **Layovers across a DST change at the connecting airport** can be off by an
  hour in the UI, which computes them from the local wall-clock strings. Fixing
  it properly means returning the UTC epochs alongside them.
- `provided.cpp` prints a `DEBUG: AirportDB attempting to open:` line on every
  search.
- `backend/cpp/main.cpp` checks `argc < 5` but reads up to `argv[6]`, so passing
  exactly five arguments reads out of bounds. Pass all six.
