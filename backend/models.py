from fastapi import FastAPI
from pydantic import BaseModel, Field
from typing import List
from typing import Optional

class Flight(BaseModel):
    airline: str
    flight_no: int
    from_airport: str = Field(alias="from") 
    to_airport: str = Field(alias="to")
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
    source: str
    destination: str
    departure_date: str
    departure_time: str = "08:00:00"
    origin_timezone: Optional[str] = None
    search_mode: str
    adults: int = 1
