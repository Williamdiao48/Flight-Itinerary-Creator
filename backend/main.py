from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
from typing import List
from services import plan_trip, iso_to_utc_epoch, time_to_utc_epoch, run_cpp_planner_native, airport_timezone, utc_epoch_to_local_string, iata_db
from models import PlanRequest, Itinerary, Flight, PlanResponse
import os
import time
import json
app = FastAPI()

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],  # Allows all origins (good for local development)
    allow_credentials=True,
    allow_methods=["*"],  # Allows POST, GET, OPTIONS, etc.
    allow_headers=["*"],  # Allows all headers
)

@app.get("/")
def read_root():
    return {"message": "Flight Planner API"}

@app.get("/airports/search")
def search_airports(q: str = ""):
    if not q or len(q) < 2:
        return []
    
    q_lower = q.lower()
    results: List[dict] = []
    
    for code, info in iata_db.items():
        name = info.get("name", "").lower()
        city = info.get("city", "").lower()
        code_lower = code.lower()
        
        score = 99
        if q_lower == code_lower:
            score = 0
        elif code_lower.startswith(q_lower):
            score = 10
        elif city.startswith(q_lower):
            score = 20
        elif name.startswith(q_lower):
            score = 30
        elif q_lower in city:
            score = 40
        elif q_lower in name:
            score = 50
        elif q_lower in code_lower:
            score = 60
            
        if score < 99:
            if "international" in name:
                score -= 5
            
            demotions = ["municipal", "regional", "county", " af", "field", "air force base", "army", "aaf"]
            if any(d in name for d in demotions):
                score += 20
                
            results.append({
                "iata": code,
                "name": info.get("name", "Unknown Airport"),
                "city": info.get("city", "Unknown City"),
                "country": info.get("country", ""),
                "score": score
            })
                
    results.sort(key=lambda x: (x["score"], x["city"], x["name"]))
    
    # remove the score key before returning
    final_results = []
    count = 0
    for r in results:
        if count >= 10:
            break
        r.pop("score", None)
        final_results.append(r)
        count += 1
        
    return final_results

@app.post("/plan", response_model = PlanResponse)
async def plan_flight(request: PlanRequest):
    start = time.perf_counter()

    if request.origin_timezone is None:
        request.origin_timezone = airport_timezone(request.source)

    adjusted_window_start = time_to_utc_epoch(
        date_str=request.departure_date,
        time_str=request.departure_window_start,
        timezone_name=request.origin_timezone 
    )

    adjusted_window_end = time_to_utc_epoch(
        date_str=request.departure_date,
        time_str=request.departure_window_end,
        timezone_name=request.origin_timezone 
    )

    BACKEND_DIR = os.path.dirname(os.path.abspath(__file__))
    AIRPORTS_TXT_PATH = os.path.join(BACKEND_DIR, "cpp", "airports.txt")
    
    # 1. Outbound trip
    flight_data_outbound = await plan_trip(
        request.source, request.destination, request.departure_date
    )

    itinerary_outbound = await run_cpp_planner_native(
        flight_data_outbound, 
        request.source, 
        request.destination, 
        adjusted_window_start,
        adjusted_window_end,
        AIRPORTS_TXT_PATH, 
        request.search_mode,
        request.max_layovers,
        request.preferred_airlines,
        request.max_results,
        request.max_duration_hours,
        request.max_price
    )

    for itinerary in itinerary_outbound:
        for flight in itinerary["flights"]:
            from_tz = airport_timezone(flight["from"])
            to_tz = airport_timezone(flight["to"])

            from_info = iata_db.get(flight["from"], {})
            to_info = iata_db.get(flight["to"], {})
            flight["from_lat"] = from_info.get("lat", 0.0)
            flight["from_lon"] = from_info.get("lon", 0.0)
            flight["to_lat"] = to_info.get("lat", 0.0)
            flight["to_lon"] = to_info.get("lon", 0.0)

            flight["departure"] = utc_epoch_to_local_string(flight["departure"], from_tz)
            flight["arrival"] = utc_epoch_to_local_string(flight["arrival"], to_tz)

    itinerary_return = None
    if request.trip_type == "round-trip" and request.return_date:
        dest_timezone = airport_timezone(request.destination)
        adjusted_return_window_start = time_to_utc_epoch(
            date_str=request.return_date,
            time_str=request.departure_window_start,
            timezone_name=dest_timezone 
        )
        adjusted_return_window_end = time_to_utc_epoch(
            date_str=request.return_date,
            time_str=request.departure_window_end, # Assume same preferred time bounds
            timezone_name=dest_timezone 
        )

        flight_data_return = await plan_trip(
            request.destination, request.source, request.return_date
        )

        itinerary_return = await run_cpp_planner_native(
            flight_data_return, 
            request.destination, 
            request.source, 
            adjusted_return_window_start,
            adjusted_return_window_end,
            AIRPORTS_TXT_PATH, 
            request.search_mode,
            request.max_layovers,
            request.preferred_airlines,
            request.max_results,
            request.max_duration_hours,
            request.max_price
        )

        for itinerary in itinerary_return:
            for flight in itinerary["flights"]:
                from_tz = airport_timezone(flight["from"])
                to_tz = airport_timezone(flight["to"])

                from_info = iata_db.get(flight["from"], {})
                to_info = iata_db.get(flight["to"], {})
                flight["from_lat"] = from_info.get("lat", 0.0)
                flight["from_lon"] = from_info.get("lon", 0.0)
                flight["to_lat"] = to_info.get("lat", 0.0)
                flight["to_lon"] = to_info.get("lon", 0.0)

                flight["departure"] = utc_epoch_to_local_string(flight["departure"], from_tz)
                flight["arrival"] = utc_epoch_to_local_string(flight["arrival"], to_tz)


    end = time.perf_counter()
    print(f"Runtime of cpp+python: {end - start:.4f} seconds")
    return {
        "outbound": itinerary_outbound,
        "return_flight": itinerary_return
    }
    

    












