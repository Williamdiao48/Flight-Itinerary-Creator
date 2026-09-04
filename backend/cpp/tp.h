//
//  tp.h
//  CS32 Project 4 Air Anarchy
//
//  Created by William Diao on 3/17/25.
//

#ifndef TP_H
#define TP_H
#include "bstset.h"
#include "fm.h"
#include "provided.h"
#include <queue>
#include <set>

enum searchMode { FRUGAL, BALANCED, FAST };

class TravelPlanner : public TravelPlannerBase {
private:
  double price_weight;
  static constexpr double TIME_WEIGHT = 1.0;
  std::set<std::string>
      preferred_airlines; // Set to store unique preferred airlines
  mutable std::unordered_map<std::string, int>
      bestKnownCostToAirport; // Map to store best times to airports to improve
                              // optimality
  int max_connections = 100;  // default large number

  int estimateRemainingCost(const std::string &current_airport,
                            const std::string &destination_airport) const;

  bool isFlightPreferred(const FlightSegment &flight,
                         const std::set<std::string> &preferredAirlines) const {
    return preferredAirlines.empty() ||
           preferredAirlines.find(flight.airline) != preferredAirlines.end();
  }

  int computeInitialCost(const FlightSegment &flight) const {
    int priceCost = static_cast<int>(flight.price * price_weight);
    return flight.duration_sec + priceCost;
  }

  class FlightNode {
  public:
    FlightSegment flight; // The flight information object
    int costSoFar; // g(n), The total(time+price) cost to reach this airport
                   // from the source
    int estimatedRemainingCost; // h(n) The heuristic cost (estimated remaining
                                // time to destination)
    int durationSoFar;
    double priceSoFar;
    std::vector<FlightSegment>
        path; // Path history holding past flight segments leading to this node

    void add_to_path(const FlightSegment &f) { path.push_back(f); }
    bool operator<(const FlightNode &other) const {
      return getTotalEstimatedCost() >
             other.getTotalEstimatedCost(); // Min-heap: lower f(n) has higher
                                            // priority
    }
    FlightNode(const FlightSegment &f, int costSoFar,
               int estimatedRemainingCost, int duration, double price) // Constructor
        : flight(f), costSoFar(costSoFar),
          estimatedRemainingCost(estimatedRemainingCost), durationSoFar(duration), priceSoFar(price) {}

    FlightNode(const FlightNode &other)
        : flight(other.flight), costSoFar(other.costSoFar),
          estimatedRemainingCost(other.estimatedRemainingCost),
          durationSoFar(other.durationSoFar), priceSoFar(other.priceSoFar),
          path(other.path) {} // Copy Constructor
    // Copy Assignment Operator
    FlightNode &operator=(const FlightNode &other) {
      if (this != &other) {
        flight = other.flight;
        costSoFar = other.costSoFar;
        estimatedRemainingCost = other.estimatedRemainingCost;
        durationSoFar = other.durationSoFar;
        priceSoFar = other.priceSoFar;
        path = other.path;
      }
      return *this;
    }
    // Estimate for how optimal the route is
    double getTotalEstimatedCost() const {
      return costSoFar + estimatedRemainingCost;
    }
  };

  int computeNextCostSoFar(const FlightNode &currentNode,
                           const FlightSegment &nextFlight) const;

  int computeTotalTravelTime(const std::vector<FlightSegment> &path,
                             int start_time) const {
    if (path.empty())
      return 0;
    int start_of_journey = path.front().departure_time;
    int end_of_journey = path.back().departure_time + path.back().duration_sec;
    return end_of_journey - start_of_journey;
  }

  double computeTotalPrice(const std::vector<FlightSegment> &path) const {
    double total = 0.0;
    for (const auto &f : path) {
      total += f.price;
    }
    return total;
  }

public:
  TravelPlanner(const FlightManagerBase &flight_manager,
                const AirportDB &airport_db, searchMode mode)
      : TravelPlannerBase(flight_manager, airport_db) {
    if (mode == FRUGAL) {
      price_weight = 1000;
    } else if (mode == FAST) {
      price_weight = 1;
    } else if (mode == BALANCED) {
      price_weight = 300;
    } else {
      price_weight = 300;
    }
  }

  ~TravelPlanner() {}
  void add_preferred_airline(std::string airline) {
    preferred_airlines.insert(
        airline); // Insert airline into the set of preferred airlines
  }
  void set_max_connections(int m) { max_connections = m; }
  int get_max_connections() const { return max_connections; }
  // The dollars-to-seconds exchange rate this mode searches with. Exposed so
  // callers can rank finished itineraries by the same objective g(n) used
  // during the search, instead of re-declaring the constants themselves.
  double get_price_weight() const { return price_weight; }
  std::vector<Itinerary> plan_travel(std::string source_airport,
                                     std::string destination_airport,
                                     int start_window, int end_window, int max_results = 3) const override;
};

#endif
