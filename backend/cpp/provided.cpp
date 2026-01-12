#include "provided.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cmath>

bool AirportDB::load_airport_data(std::string filename)
{
    std::cerr << "DEBUG: AirportDB attempting to open: " << filename << std::endl;
    std::ifstream file(filename);
    if (!file)
    {
        std::cerr << "Can't open airport database: " << filename << std::endl;
        return false;
    }

    std::string line;
    while (std::getline(file, line))
    {
        std::istringstream ss(line);
        std::string code;
        std::string latitude_str, longitude_str;

        if (!std::getline(ss, code, ',') ||
            !std::getline(ss, latitude_str, ',') ||
            !std::getline(ss, longitude_str, ','))
            continue;

        double latitude = std::stod(latitude_str);
        double longitude = std::stod(longitude_str);

        m_airports[code] = { latitude, longitude };
    }

    return true;
}

bool AirportDB::get_distance(std::string source_airport, std::string destination_airport,
                             double& distance) const
{
    const auto it1 = m_airports.find(source_airport);
    if (it1 == m_airports.end() )
    {
        std::cerr << "Can't find source airport: " << source_airport << std::endl;
        return false;
    }

    const auto it2 = m_airports.find(destination_airport);
    if (it2 == m_airports.end())
    {
        std::cerr << "Can't find dest airport: " << destination_airport << std::endl;
        return false;
    }

    distance = haversineDistance(it1->second, it2->second);
    return true;
}

double AirportDB::haversineDistance(const GeoLocation& l1, const GeoLocation& l2) const
{
    const double EARTH_RADIUS_MILES = 3958.8; // Earth's radius in miles

    // Convert degrees to radians
    const double PI = 4 * atan(1.0);
    double lat1 = l1.latitude * PI / 180.0;
    double lon1 = l1.longitude * PI / 180.0;
    double lat2 = l2.latitude * PI / 180.0;
    double lon2 = l2.longitude * PI / 180.0;

    // Haversine formula
    double dlat = lat2 - lat1;
    double dlon = lon2 - lon1;
    double a = sin(dlat / 2) * sin(dlat / 2) +
               cos(lat1) * cos(lat2) * sin(dlon / 2) * sin(dlon / 2);
    double c = 2 * atan2(sqrt(a), sqrt(1 - a));

    return c * EARTH_RADIUS_MILES; // Distance in miles
}


// Check if an itinerary is valid (all connections make sense)
// - flightManager: reference to a FlightManagerBase to look up flight info
// - itinerary: the itinerary to validate.
// - min_connection_time: required minimum connection time (in seconds)
// - max_layover: the maximum allowed layover time (in seconds)
// - errMsg: a string reference to store error messages, if any
// Returns true if valid, false otherwise

bool validate_itinerary(
    const FlightManagerBase& flightManager,
    const Itinerary& itinerary,
    int min_connection_time,
    int max_layover,
    std::string& errMsg)
{
      // Clear out any previous error message
    errMsg.clear();

      // Edge case: empty itinerary
    if (itinerary.flights.empty())
    {
        errMsg = "Itinerary has no flights.";
        return false;
    }

      // Check that itinerary's source/destination match the first/last flight
    if (itinerary.flights.front().source_airport != itinerary.source_airport)
    {
        errMsg = "Itinerary source airport mismatch: "
                 + itinerary.flights.front().source_airport
                 + " != " + itinerary.source_airport;
        return false;
    }
    if (itinerary.flights.back().destination_airport != itinerary.destination_airport)
    {
        errMsg = "Itinerary destination airport mismatch: "
                 + itinerary.flights.back().destination_airport
                 + " != " + itinerary.destination_airport;
        return false;
    }

      // Traverse flights in a single loop
    for (size_t i = 0; i < itinerary.flights.size(); ++i)
    {
        const FlightSegment& flight = itinerary.flights[i];

          // --- 1) Verify the current flight is in the flight manager database

          // Look up possible flights in the flight manager
        auto possible_flights = flightManager.find_flights(
            flight.source_airport,
            flight.departure_time,
            flight.departure_time + 1 // short window to match exact flight
        );

        bool found_match = false;
        for (const auto& f : possible_flights)
        {
            if (f.airline == flight.airline &&
                f.flight_no == flight.flight_no &&
                f.source_airport == flight.source_airport &&
                f.destination_airport == flight.destination_airport &&
                f.departure_time == flight.departure_time &&
                f.duration_sec == flight.duration_sec)
            {
                found_match = true;
                break;
            }
        }
        if (!found_match)
        {
            errMsg = "Flight " + flight.airline + " " + std::to_string(flight.flight_no)
                     + " from " + flight.source_airport + " to " + flight.destination_airport
                     + " not found in flightManager database.";
            return false;
        }

          // --- 2) If there's a next flight, validate the connection
        if (i + 1 < itinerary.flights.size())
        {
            const FlightSegment& next = itinerary.flights[i + 1];

              // a) Destination of current must match source of next
            if (flight.destination_airport != next.source_airport)
            {
                errMsg = "Connection mismatch between flights "
                         + std::to_string(flight.flight_no) + " and "
                         + std::to_string(next.flight_no) + ": "
                         + flight.destination_airport + " != "
                         + next.source_airport;
                return false;
            }

              // b) Next flight must depart after current flight arrives and
              //    connection times must meet constraints
            int arrival_time_current = flight.departure_time + flight.duration_sec;
            int layover_time = next.departure_time - arrival_time_current;

              //   i) Minimum connection time (curr arrives too late for next)
            if (layover_time < min_connection_time)
            {
                if (layover_time < 0)  // next departs before curr arrives
                    errMsg = "Flight " + std::to_string(next.flight_no)
                             + " departs before the previous flight arrives.";
                else
                    errMsg = "Not enough connection time between flights "
                             + std::to_string(flight.flight_no) + " and "
                             + std::to_string(next.flight_no)
                             + ". Required minimum: " + std::to_string(min_connection_time)
                             + ", actual: " + std::to_string(layover_time);
                return false;
            }

            //   ii) Maximum layover time
            if (layover_time > max_layover)
            {
                errMsg = "Layover is too long between flights "
                         + std::to_string(flight.flight_no) + " and "
                         + std::to_string(next.flight_no)
                         + ". Maximum allowed: " + std::to_string(max_layover)
                         + ", actual: " + std::to_string(layover_time);
                return false;
            }
        }
    }

      // If we reach this point, all flights + connections are valid.
    return true;
}

