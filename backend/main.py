from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
from typing import List
from services import plan_trip, iso_to_utc_epoch, time_to_utc_epoch, run_cpp_planner, airport_timezone, utc_epoch_to_local_string
from models import PlanRequest, Itinerary, Flight
from amadeus import Client, ResponseError
from utils import write_flights_to_csv
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

@app.post("/plan", response_model = Itinerary)
def plan_flight(request: PlanRequest):
    start = time.perf_counter()

    if request.origin_timezone is None:
        request.origin_timezone = airport_timezone(request.source)
    
    print(f"DEBUG: origin_timezone is '{request.origin_timezone}'")

    adjusted_departure_time = time_to_utc_epoch(
    date_str=request.departure_date,
    time_str=request.departure_time,
    timezone_name=request.origin_timezone 
    )
    #1768925060
    print(adjusted_departure_time)

    BACKEND_DIR = os.path.dirname(os.path.abspath(__file__))
    PROJECT_ROOT = os.path.dirname(BACKEND_DIR)
    FLIGHT_CSV_FILE_PATH = os.path.join(PROJECT_ROOT, "flights.csv")
    PLANNER_PATH = os.path.join(BACKEND_DIR, "cpp", "planner")
    AIRPORTS_TXT_PATH = os.path.join(BACKEND_DIR, "cpp", "airports.txt")

    flight_data = plan_trip(request)
    write_flights_to_csv(flight_data, filename=FLIGHT_CSV_FILE_PATH)

    print(f"DEBUG: C++ is being told start time is: {adjusted_departure_time}")
    cpp_output = run_cpp_planner(PLANNER_PATH, FLIGHT_CSV_FILE_PATH, request.source, request.destination, adjusted_departure_time, AIRPORTS_TXT_PATH, request.search_mode)
    print(f"DEBUG RAW CPP OUTPUT: {cpp_output}")

    itinerary_result = json.loads(cpp_output)

    for flight in itinerary_result["flights"]:
        from_tz = airport_timezone(flight["from"])
        to_tz = airport_timezone(flight["to"])

        flight["departure"] = utc_epoch_to_local_string(flight["departure"], from_tz)
        flight["arrival"] = utc_epoch_to_local_string(flight["arrival"], to_tz)
    end = time.perf_counter()
    print(f"Runtime of cpp+python: {end - start:.4f} seconds")
    return itinerary_result
    

    












