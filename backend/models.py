from fastapi import FastAPI
from pydantic import BaseModel, Field
from typing import List
from typing import Optional

class Flight(BaseModel):
    airline: str
    flight_no: int
    from_airport: str = Field(alias="from") 
    to_airport: str = Field(alias="to")
    from_lat: Optional[float] = None
    from_lon: Optional[float] = None
    to_lat: Optional[float] = None
    to_lon: Optional[float] = None
    departure: str
    arrival: str
    duration: int
    price: float
    model_config = {"populate_by_name": True}

class Itinerary(BaseModel):
    source: str
    destination: str
    total_duration: int
    total_cost:float
    flights: List[Flight]

class PlanRequest(BaseModel):
    trip_type: str = "one-way" # "one-way" or "round-trip"
    source: str
    destination: str
    departure_date: str
    return_date: Optional[str] = None
    departure_window_start: str = "00:00:00"
    departure_window_end: str = "23:59:59"
    origin_timezone: Optional[str] = None
    search_mode: str
    adults: int = 1
    max_layovers: Optional[int] = None
    preferred_airlines: Optional[List[str]] = None
    max_results: int = 3
    max_duration_hours: Optional[int] = 48
    max_price: Optional[float] = 100000.0

class PlanResponse(BaseModel):
    outbound: List[Itinerary]
    return_flight: Optional[List[Itinerary]] = None
