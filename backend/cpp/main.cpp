#include "tp.h"
#include "fm.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
#include <limits>
#include <ctime>
#include <cmath>
#include <iomanip>



  // Helper function to format a UNIX timestamp.
std::string format_time(int unix_time)
{
    std::time_t time = unix_time;
    std::tm* tm_ptr = std::gmtime(&time);
    std::ostringstream oss;
    oss << std::put_time(tm_ptr, "%Y-%m-%d %H:%M UTC (") << unix_time << ")";
    return oss.str();
}

  // Function that prints an itinerary.
  void print_itinerary_json(int start_time, const Itinerary& itinerary) {
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "{\n";
    std::cout << "  \"source\": \"" << itinerary.source_airport << "\",\n";
    std::cout << "  \"destination\": \"" << itinerary.destination_airport << "\",\n";
    std::cout << "  \"total_duration\": " << itinerary.total_duration << ",\n";
    std::cout << "  \"total_cost\": " << itinerary.total_cost << ",\n";
    std::cout << "  \"flights\": [\n";

    for (size_t i = 0; i < itinerary.flights.size(); ++i) {
        const auto& f = itinerary.flights[i];
        std::cout << "    {\n";
        std::cout << "      \"airline\": \"" << f.airline << "\",\n";
        std::cout << "      \"flight_no\": " << f.flight_no << ",\n";
        std::cout << "      \"from\": \"" << f.source_airport << "\",\n";
        std::cout << "      \"to\": \"" << f.destination_airport << "\",\n";
        std::cout << "      \"departure\": " << f.departure_time << ",\n";
        std::cout << "      \"arrival\": "
                  << (f.departure_time + f.duration_sec) << ",\n";
        std::cout << "      \"duration\": " << f.duration_sec << ",\n";
        std::cout << "      \"price\": " << f.price << "\n";
        std::cout << "    }";

        if (i + 1 < itinerary.flights.size())
            std::cout << ",";

        std::cout << "\n";
    }

    std::cout << "  ]\n";
    std::cout << "}\n";
}

  // Main entry point
int main(int argc, char* argv[]) {
      // We expect either one argument, the test parameters file, or no arguments,
      // meaning we'll prompt for the test parameters file
    

      double max_duration_hours = 48;
      double min_connection_hours = 0.5;
      double max_layover_hours = 360;
      std::vector<std::string> preferred_airlines;


    if (argc < 5) {
      std::cerr << "Usage: " << argv[0]<< " flights.csv SRC DST START_TIME\n";
      return 1;
}
    std::string flight_data_file = argv[1];
    std::string source_airport = argv[2];
    std::string destination_airport= argv[3];
    int start_time = std::stoi(argv[4]);
    std::string airports_file = argv[5];
    std::string search_mode = argv[6];

    searchMode selectedMode;
    if(search_mode=="frugal"){
      selectedMode = FRUGAL;}
    else if(search_mode=="fast"){
      selectedMode = FAST;}
    else{
      selectedMode = BALANCED;}


    // Initialize the flight manager and travel planner
    FlightManager flight_manager;
    AirportDB airport_db;
    if (!airport_db.load_airport_data(airports_file)) {
        std::cerr << "Failed to load airport data.\n";
        return 1;
    }
      // Attempt to load the flights data
    if (!flight_manager.load_flight_data(flight_data_file)) {
        std::cerr << "Failed to load flight data from " << flight_data_file << ".\n";
        return 1;
    }

    TravelPlanner travel_planner(flight_manager, airport_db, selectedMode);

      // Convert hours to seconds
    int max_duration_sec    = static_cast<int>(std::round(max_duration_hours * 3600.0));
    int min_connection_sec  = static_cast<int>(std::round(min_connection_hours * 3600.0));
    int max_layover_sec     = static_cast<int>(std::round(max_layover_hours * 3600.0));

      // Set the travel-planner constraints
    travel_planner.set_max_duration(max_duration_sec);
    travel_planner.set_min_connection_time(min_connection_sec);
    travel_planner.set_max_layover(max_layover_sec);

      // Add preferred airlines, if any
    for (const auto& airline : preferred_airlines) {
        travel_planner.add_preferred_airline(airline);
    }

      // Now plan the itinerary
    Itinerary itinerary;
    bool result = travel_planner.plan_travel(source_airport, destination_airport, start_time, itinerary);

      // Print results
    if (!result) {
        std::cout << "No itineraries found matching your criteria.\n";
    }
    else {
          // Validate the itinerary
        std::string error_message;
        if (!validate_itinerary(
                flight_manager,
                itinerary,
                travel_planner.get_min_connection_time(),
                travel_planner.get_max_layover(),
                error_message)) {
            std::cerr << "Invalid itinerary: " << error_message << std::endl;
            return 1;
        }
        print_itinerary_json(start_time, itinerary);
    }
}

