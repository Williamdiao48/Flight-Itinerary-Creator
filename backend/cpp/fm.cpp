//
//  fm.cpp
//  CS32 Project 4 Air Anarchy
//
//  Created by William Diao on 3/19/25.
//
#include "fm.h"
#include "bstset.h"

//Comparator functor for sorting FlightSegment by departure time
bool operator<(const FlightSegment& lhs, const FlightSegment& rhs) {
    if(lhs.departure_time==rhs.departure_time){
        return lhs.duration_sec < rhs.duration_sec;
    }
    return lhs.departure_time < rhs.departure_time;
}

bool FlightManager::load_flight_data(std::string filename){
    std::ifstream file(filename);
    if (!file.is_open()) {
        return false;
    }
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream ss(line);
        std::string airline, source_airport, destination_airport;
        int flight_no, departure_time, duration_sec;
        int tempArrival;
        double price;
        //Read the fields from the line ignoring  arrival time
        std::getline(ss, airline, ',');
        ss >> flight_no;
        ss.ignore(); //Ignore comma after flight number
        std::getline(ss, source_airport, ',');
        std::getline(ss, destination_airport, ',');
        ss >> departure_time;
        ss.ignore(); // Ignore the comma after departure time
        ss>>tempArrival; // To skip arrival time (not stored)
        ss.ignore(); //Ignore the comma
        ss >> duration_sec;
        ss.ignore();
        ss>>price;

        //Check if the data is valid and if so, create a FlightSegment and insert into the set
            if (!ss.fail()) {
                FlightSegment flight(airline, flight_no, source_airport, destination_airport, departure_time, duration_sec, price);
                flights_by_airport[source_airport].insert(flight);  // Insert into the set for the source airport
            }
    }
    file.close();
    return true;
}

std::vector<FlightSegment> FlightManager::find_flights(std::string source_airport, int start_time, int end_time) const{
    std::vector<FlightSegment> result;

    // Find the BSTSet for the airport
    auto it = flights_by_airport.find(source_airport);
    if (it != flights_by_airport.end()) {
        const BSTSet<FlightSegment>& flights = it->second;

        //Find the first flight that is not smaller than start_time
        auto iter = flights.find_first_not_smaller(FlightSegment("", 0, "", "", start_time, 0, 0));
        //Iterate through the set and collect all flights within the time range
        while (true) {
            const FlightSegment* flight = iter.get_and_advance();
            if(flight==nullptr){
                break;
            }
            
            if (flight->departure_time > end_time) {
                break;  //Stop checking because flights are sorted by time
            }
            result.push_back(*flight);
        }
    }
        return result;
}
