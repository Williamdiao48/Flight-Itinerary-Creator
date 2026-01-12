from datetime import datetime, timezone
import pytz
from amadeus import Client, ResponseError
from models import PlanRequest
import airportsdata
import subprocess
import time
import os
from dotenv import load_dotenv

FLIGHT_CACHE = {}
CACHE_TTL = 3600
load_dotenv()
api_key = os.getenv("AMADEUS_API_KEY")
api_secret = os.getenv("AMADEUS_API_SECRET")
amadeus = Client(
    client_id = api_key,
    client_secret = api_secret
)

iata_db = airportsdata.load('IATA')  
with open("airports.txt", "w") as f:
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
        local_tz = pytz.timezone(timezone_name)
    except pytz.UnknownTimeZoneError:
        print(f"Warning: Unknown timezone '{timezone_name}'. Defaulting to UTC.")
        local_tz = timezone.utc
    
    naive_dt = datetime.strptime(dt_str, "%Y-%m-%d %H:%M:%S")
    local_dt = local_tz.localize(naive_dt, is_dst=None)
    utc_dt = local_dt.astimezone(pytz.utc)
    return int(utc_dt.timestamp())

#converts from given time to utc epoch
def iso_to_utc_epoch(iso_time_str: str, default_tz_name: str = 'UTC') -> int: 
    dt = datetime.fromisoformat(iso_time_str)
    if dt.tzinfo is None:
        try:
            local_tz = pytz.timezone(default_tz_name)
        except:
           local_tz = timezone.utc 
        dt = local_tz.localize(dt, is_dst=None)
    utc_dt = dt.astimezone(pytz.utc)
    return int(utc_dt.timestamp())

def utc_epoch_to_local_string(utc_timestamp: int, target_tz_name: str) -> str:
    utc_dt = datetime.utcfromtimestamp(utc_timestamp).replace(tzinfo=pytz.utc)
    try:
        target_tz = pytz.timezone(target_tz_name)
    except pytz.UnknownTimeZoneError:
        target_tz = pytz.utc
    local_dt = utc_dt.astimezone(target_tz)
    return local_dt.strftime("%Y-%m-%d %H:%M:%S")

def flight_fetch_offers_sandbox(source: str, destination: str, departure_date: str, adults: int = 1, max_results: int = 10):
    print(">>> ENTERED flight_fetch_offers_sandbox <<<")
    print("PARAMS:", source, destination, departure_date, adults)
    print(">>> CALLING AMADEUS API <<<")
    api_start = time.time()
    flight_segments = []
    try:
        response = amadeus.shopping.flight_offers_search.get(
            originLocationCode=source,
            destinationLocationCode=destination,
            departureDate=departure_date,
            adults=adults,
            max=max_results
        )
        
        for offer in response.data:
            total_price = float(offer['price']['total'])
            for itinerary in offer.get('itineraries', []):
                segments = itinerary.get('segments',[])
                if not segments:
                    continue
                price_per_segment = total_price / len(segments)
                for segment in segments:
                    airline = segment.get('carrierCode', 'XX')
                    flight_no = int(segment.get('number', 0))
                    from_airport = segment['departure'].get('iataCode', source)
                    to_airport = segment['arrival'].get('iataCode', destination)
                    departure_time = iso_to_utc_epoch(segment['departure'].get('at'), airport_timezone(from_airport))
                    arrival_time = iso_to_utc_epoch(segment['arrival'].get('at'), airport_timezone(to_airport))
                    duration_sec = arrival_time - departure_time

                    flight_segments.append({
                        "airline": airline,
                        "flight_no": flight_no,
                        "source_airport": from_airport,
                        "destination_airport": to_airport,
                        "departure_time": departure_time,
                        "arrival_time": arrival_time,
                        "duration_sec": duration_sec,
                        "price": price_per_segment})

    except ResponseError as e:
        print("AMADEUS ERROR:", e)

    # If sandbox returns nothing, fill with a mock flight
    if not flight_segments:
        print("No flights returned from sandbox")

    print(flight_segments)

    print(f"Returning {len(flight_segments)} flight segments")
    api_end = time.time()
    print(f"AMADEUS API RESPONDED IN: {api_end - api_start:.4f}s")
    return flight_segments

def plan_trip (request: PlanRequest):
    cache_key = f"{request.source}-{request.destination}-{request.departure_date}"
    current_time = time.time()
    if cache_key in FLIGHT_CACHE:
        entry = FLIGHT_CACHE[cache_key]

        if current_time - entry["timestamp"] < CACHE_TTL:
            print("cache hit")
            return entry["data"]
        else:
            print("updating cache, too old")
            del FLIGHT_CACHE[cache_key]
    
    print("No cache data, retriving")
    flights = flight_fetch_offers_sandbox(request.source, request.destination, request.departure_date)
    FLIGHT_CACHE[cache_key] = {
        "data": flights,
        "timestamp": current_time
    }

    return flights


def run_cpp_planner(planner_path, flight_csv_path, source, destination, adjusted_departure_time, airports_txt_path, search_mode):
    start = time.time()
    result = subprocess.run([
        planner_path,
        flight_csv_path,
        source,
        destination,
        str(adjusted_departure_time),
        airports_txt_path,
        search_mode
        ], capture_output=True, text=True, check = True)
    end = time.time()
    print(f"C++ Subprocess execution time: {end - start:.4f}s")
    return result.stdout  