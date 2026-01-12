//
//  fm.h
//  CS32 Project 4 Air Anarchy
//
//  Created by William Diao on 3/17/25.
//


#ifndef FM_H
#define FM_H

#include "provided.h"
#include "bstset.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <limits>
#include <iostream>
#include <fstream>
#include <sstream>

class FlightManager : public FlightManagerBase{
private:
    std::unordered_map<std::string, BSTSet<FlightSegment>> flights_by_airport;
public:
    FlightManager(){}
    ~FlightManager(){}
    virtual bool load_flight_data(std::string filename);
    virtual std::vector<FlightSegment> find_flights(std::string source_airport, int start_time, int end_time) const;
};

#endif
