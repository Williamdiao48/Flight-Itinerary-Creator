from datetime import datetime, timezone
import zoneinfo
from models import PlanRequest # pyre-ignore
import airportsdata # pyre-ignore
import subprocess
import time
import os
from dotenv import load_dotenv # pyre-ignore
import requests

FLIGHT_CACHE = {}
CACHE_TTL = 3600
load_dotenv()
duffel_api_key = os.getenv("DUFFEL_API_KEY", "")

if not duffel_api_key:
    print("WARNING: DUFFEL_API_KEY is missing. Downstream calls to Duffel will fail.")

iata_db = airportsdata.load('IATA')  

BACKEND_DIR = os.path.dirname(os.path.abspath(__file__))
AIRPORTS_TXT_PATH = os.path.join(BACKEND_DIR, "cpp", "airports.txt")

if not os.path.exists(AIRPORTS_TXT_PATH):
    print("Generating airports.txt for C++ planner...")
    # Ensure the parent directory exists
    os.makedirs(os.path.dirname(AIRPORTS_TXT_PATH), exist_ok=True)
    with open(AIRPORTS_TXT_PATH, "w") as f:
        for iata_code, info in iata_db.items():
            lat = info.get('lat', 0.0)
            lon = info.get('lon', 0.0)
            f.write(f"{iata_code},{lat},{lon}\n")

def airport_timezone(iata_code: str) -> str:
    try:
        return iata_db[iata_code]["tz"]
    except KeyError:
        return "UTC"

#converts from user input time to a utc time in epoch
def time_to_utc_epoch(date_str: str, time_str: str = "00:00", timezone_name: str = 'UTC')->int:
    parts = time_str.split(':')
    clean_time = f"{parts[0].zfill(2)}:{parts[1].zfill(2)}:00"

    dt_str = f"{date_str} {clean_time}"
    print(f"Date string is:::{dt_str}")
    try:
        local_tz = zoneinfo.ZoneInfo(timezone_name)
    except zoneinfo.ZoneInfoNotFoundError:
        print(f"Warning: Unknown timezone '{timezone_name}'. Defaulting to UTC.")
        local_tz = timezone.utc
    
    naive_dt = datetime.strptime(dt_str, "%Y-%m-%d %H:%M:%S")
    local_dt = naive_dt.replace(tzinfo=local_tz)
    utc_dt = local_dt.astimezone(timezone.utc)
    return int(utc_dt.timestamp())

#converts from given time to utc epoch
def iso_to_utc_epoch(iso_time_str: str, default_tz_name: str = 'UTC') -> int: 
    dt = datetime.fromisoformat(iso_time_str)
    if dt.tzinfo is None:
        try:
            local_tz = zoneinfo.ZoneInfo(default_tz_name)
        except zoneinfo.ZoneInfoNotFoundError:
           local_tz = timezone.utc 
        dt = dt.replace(tzinfo=local_tz)
    utc_dt = dt.astimezone(timezone.utc)
    return int(utc_dt.timestamp())

def utc_epoch_to_local_string(utc_timestamp: int, target_tz_name: str) -> str:
    utc_dt = datetime.fromtimestamp(utc_timestamp, tz=timezone.utc)
    try:
        target_tz = zoneinfo.ZoneInfo(target_tz_name)
    except zoneinfo.ZoneInfoNotFoundError:
        target_tz = timezone.utc
    local_dt = utc_dt.astimezone(target_tz)
    return local_dt.strftime("%Y-%m-%d %H:%M:%S")

def flight_fetch_duffel(source: str, destination: str, departure_date: str):
    print(">>> ENTERED flight_fetch_duffel <<<")
    print("PARAMS:", source, destination, departure_date)
    if not duffel_api_key:
        print("DUFFEL ERROR: Client not initialized due to missing credentials.")
        return []

    url = "https://api.duffel.com/air/offer_requests"
    headers = {
        "Authorization": f"Bearer {duffel_api_key}",
        "Duffel-Version": "v2",
        "Content-Type": "application/json"
    }
    payload = {
        "data": {
            "slices": [
                {
                    "origin": source,
                    "destination": destination,
                    "departure_date": departure_date
                }
            ],
            "passengers": [
                {"type": "adult"}
            ],
            "cabin_class": "economy"
        }
    }
    
    params = {"return_offers": "true"}
    
    try:
        resp = requests.post(url, headers=headers, json=payload, params=params)
        resp.raise_for_status()
        
        data = resp.json().get("data", {})
        offers = data.get("offers", [])
        
        flight_segments = []
        unique_seen = set()
        
        for offer in offers:
            total_price = float(offer.get("total_amount", 0))
            slices = offer.get("slices", [])
            if not slices:
                continue
                
            for slice_obj in slices:
                segments = slice_obj.get("segments", [])
                if not segments:
                    continue
                
                price_per_segment = total_price / len(segments)
                
                for segment in segments:
                    operating_carrier = segment.get("operating_carrier") or segment.get("marketing_carrier") or {}
                    airline = operating_carrier.get("iata_code", "XX")
                    
                    flight_no_str = segment.get("operating_carrier_flight_number") or segment.get("marketing_carrier_flight_number", "0")
                    flight_no_int = int(flight_no_str) if str(flight_no_str).isdigit() else 0
                    
                    from_airport = segment.get("origin", {}).get("iata_code", source)
                    to_airport = segment.get("destination", {}).get("iata_code", destination)
                    
                    depart_iso = segment.get("departing_at")
                    arrive_iso = segment.get("arriving_at")
                    
                    departure_time = iso_to_utc_epoch(depart_iso, airport_timezone(from_airport))
                    arrival_time = iso_to_utc_epoch(arrive_iso, airport_timezone(to_airport))
                    duration_sec = arrival_time - departure_time
                    
                    seg_dict = {
                        "airline": airline,
                        "flight_no": flight_no_int,
                        "source_airport": from_airport,
                        "destination_airport": to_airport,
                        "departure_time": departure_time,
                        "arrival_time": arrival_time,
                        "duration_sec": duration_sec,
                        "price": price_per_segment
                    }
                    
                    key = f"{airline}-{flight_no_int}-{departure_time}"
                    if key not in unique_seen:
                        unique_seen.add(key)
                        flight_segments.append(seg_dict)

        print(f"Duffel returned {len(flight_segments)} unique raw segments")
        return flight_segments
        
    except Exception as e:
        print("DUFFEL ERROR:", e)
        return []

async def plan_trip(source: str, destination: str, departure_date: str):
    import asyncio
    cache_key = f"{source}-{destination}-{departure_date}"
    current_time = time.time()
    if cache_key in FLIGHT_CACHE:
        entry = FLIGHT_CACHE[cache_key]

        if current_time - entry["timestamp"] < CACHE_TTL:
            print("cache hit")
            return entry["data"]
        else:
            print("updating cache, too old")
            FLIGHT_CACHE.pop(cache_key, None)
    
    print("No cache data, retriving")
    flights = await asyncio.to_thread( # pyre-ignore
        flight_fetch_duffel, 
        source, 
        destination, 
        departure_date
    )
    FLIGHT_CACHE[cache_key] = {
        "data": flights,
        "timestamp": current_time
    }

    return flights


async def run_cpp_planner_native(flights_data, source, destination, adjusted_window_start, adjusted_window_end, airports_txt_path, search_mode, max_layovers=None, preferred_airlines=None, max_results=3, max_duration_hours=48, max_price=100000.0):
    import asyncio
    import flight_planner_cpp # pyre-ignore
    
    def _run():
        start = time.time()
        manager = flight_planner_cpp.FlightManager()
        for f in flights_data:
            fs = flight_planner_cpp.FlightSegment(
                str(f['airline']),
                int(f['flight_no']),
                str(f['source_airport']),
                str(f['destination_airport']),
                int(f['departure_time']),
                int(f['duration_sec']),
                float(f['price'])
            )
            manager.add_flight(fs)
        
        db = flight_planner_cpp.AirportDB()
        db.load_airport_data(airports_txt_path)
        
        mode_val = flight_planner_cpp.SearchMode.BALANCED
        if search_mode == "frugal":
            mode_val = flight_planner_cpp.SearchMode.FRUGAL
        elif search_mode == "fast":
            mode_val = flight_planner_cpp.SearchMode.FAST
            
        planner = flight_planner_cpp.TravelPlanner(manager, db, mode_val)
        
        planner.set_max_duration(int(max_duration_hours * 3600))
        planner.set_max_price(float(max_price))
        planner.set_min_connection_time(int(0.5 * 3600))
        planner.set_max_layover(int(360 * 3600))
        
        if max_layovers is not None:
            planner.set_max_connections(max_layovers)
            
        if preferred_airlines:
            for airline in preferred_airlines:
                planner.add_preferred_airline(airline)
        
        itineraries = planner.plan_travel(source, destination, adjusted_window_start, adjusted_window_end, max_results)
        
        results = []
        for itinerary in itineraries:
            res_flights = []
            for f in itinerary.flights:
                res_flights.append({
                    "airline": f.airline,
                    "flight_no": f.flight_no,
                    "from": f.source_airport,
                    "to": f.destination_airport,
                    "departure": f.departure_time,
                    "arrival": f.departure_time + f.duration_sec,
                    "duration": f.duration_sec,
                    "price": f.price
                })
                
            results.append({
                "source": itinerary.source_airport,
                "destination": itinerary.destination_airport,
                "total_duration": itinerary.total_duration,
                "total_cost": itinerary.total_cost,
                "flights": res_flights
            })
            
        end = time.time()
        print(f"Native PyBind11 execution time: {end - start:.4f}s")
        return results

    return await asyncio.to_thread(_run) # pyre-ignore