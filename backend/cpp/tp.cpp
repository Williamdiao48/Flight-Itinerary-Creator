//
//  tp.cpp
//  CS32 Project 4 Air Anarchy
//
//  Created by William Diao on 3/19/25.
//
#include "tp.h"
#include "provided.h"

int TravelPlanner::estimateRemainingCost(
    const std::string &current_airport,
    const std::string &destination_airport) const {
  double distance;
  // Get the distance in miles from the AirportDB
  if (!get_airport_db().get_distance(current_airport, destination_airport,
                                     distance)) {
    return 0; // Return 0 if there is an error
  }
  double estimated_flight_time_in_hours =
      distance / 500.0; // assuming average flight speed of 500 miles/hour
  int estimatedFlightTimeSec = static_cast<int>(estimated_flight_time_in_hours *
                                                3600); // Convert to seconds
  return estimatedFlightTimeSec;
}

int TravelPlanner::computeNextCostSoFar(const FlightNode &currentNode,
                                        const FlightSegment &nextFlight) const {
  int arrival_time =
      currentNode.flight.departure_time + currentNode.flight.duration_sec;
  int layover_time = nextFlight.departure_time - arrival_time;
  int time_cost = nextFlight.duration_sec + layover_time;
  int price_cost = static_cast<int>(nextFlight.price * price_weight);

  return currentNode.costSoFar + time_cost + price_cost;
}

std::vector<Itinerary>
TravelPlanner::plan_travel(std::string source_airport,
                           std::string destination_airport, int start_window, int end_window,
                           int max_results) const {
  std::vector<Itinerary> results;

  // Priority queue: stores nodes ordered by lower f scores (more optimal paths)
  std::priority_queue<FlightNode, std::vector<FlightNode>> openSet;
  // Find intial flights
  std::vector<FlightSegment> nextFlights = get_flight_manager().find_flights(
      source_airport, start_window, end_window);
  // Add each flight to the priority queue
  for (const FlightSegment &flight : nextFlights) {
    if (isFlightPreferred(flight, preferred_airlines)) {
      // Create a flight node for the first flight and calculate time elapsed
      // and estimated time left
      int new_costSoFar = computeInitialCost(flight);
      int new_reminaingCost = estimateRemainingCost(flight.destination_airport,
                                                    destination_airport);
      FlightNode firstFlightNode(flight, new_costSoFar, new_reminaingCost, flight.duration_sec, flight.price);
      firstFlightNode.add_to_path(flight);
      openSet.push(firstFlightNode);
    }
  }

  while (!openSet.empty()) {
    // Get the node with the lowest f_score
    FlightNode currentNode = openSet.top();
    openSet.pop();

    if (currentNode.durationSoFar > get_max_duration() || currentNode.priceSoFar > get_max_price()) {
      continue;
    }
    // Reached final destination, construct itinerary
    if (currentNode.flight.destination_airport == destination_airport) {
      Itinerary itinerary;
      itinerary.flights = currentNode.path;
      itinerary.source_airport = source_airport;
      itinerary.destination_airport = destination_airport;
      itinerary.total_duration =
          computeTotalTravelTime(currentNode.path, currentNode.path.front().departure_time);
      itinerary.total_cost = computeTotalPrice(currentNode.path);

      results.push_back(itinerary);

      if (results.size() >= static_cast<size_t>(max_results)) {
        return results;
      }

      continue; // Keep searching for alternative routes
    }

    // Check if we have reached this airport in a better time before
    auto it =
        bestKnownCostToAirport.find(currentNode.flight.destination_airport);

    // For intermediate airports, we want strict pruning.
    // We only prune if the new cost is significantly worse or we've already
    // found a strictly better path. However, for the destination airport, we
    // don't want to enforce this or we will only ever find 1 path.
    if (it != bestKnownCostToAirport.end() &&
        it->second <= currentNode.costSoFar &&
        currentNode.flight.destination_airport != destination_airport) {
      continue;
    }

    // Set this as new best time for reaching this airport
    bestKnownCostToAirport[currentNode.flight.destination_airport] =
        currentNode.costSoFar;

    if (currentNode.path.size() >= static_cast<size_t>(get_max_connections() + 1)) {
      continue;
    }

    std::vector<FlightSegment> nextFlights = get_flight_manager().find_flights(
        currentNode.flight.destination_airport,
        (currentNode.flight.departure_time + currentNode.flight.duration_sec +
         get_min_connection_time()),
        (currentNode.flight.departure_time + currentNode.flight.duration_sec +
         get_max_layover()));

    for (const FlightSegment &flight : nextFlights) {
      if (isFlightPreferred(flight, preferred_airlines)) {
        int new_costSoFar = computeNextCostSoFar(currentNode, flight);
        int new_remainingCost = estimateRemainingCost(
            flight.destination_airport, destination_airport);
        int additional_duration = flight.duration_sec + (flight.departure_time - (currentNode.flight.departure_time + currentNode.flight.duration_sec));
        FlightNode nextNode(flight, new_costSoFar, new_remainingCost, currentNode.durationSoFar + additional_duration, currentNode.priceSoFar + flight.price);
        nextNode.path = currentNode.path;
        nextNode.add_to_path(flight);
        openSet.push(nextNode);
      }
    }
  }
  return results;
}
