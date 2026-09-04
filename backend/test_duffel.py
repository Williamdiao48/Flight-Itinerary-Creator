import os
import requests
from dotenv import load_dotenv

load_dotenv()
duffel_api_key = os.getenv("DUFFEL_API_KEY", "")

url = "https://api.duffel.com/air/offer_requests"
headers = {
    "Authorization": f"Bearer {duffel_api_key}",
    "Duffel-Version": "beta",
    "Content-Type": "application/json"
}
payload = {
    "data": {
        "slices": [
            {
                "origin": "LAX",
                "destination": "SFO",
                "departure_date": "2026-04-16"
            }
        ],
        "passengers": [
            {"type": "adult"}
        ],
        "cabin_class": "economy"
    }
}
params = {"return_offers": "true"}

resp = requests.post(url, headers=headers, json=payload, params=params)
print("Status:", resp.status_code)
print("Response:", resp.text)
