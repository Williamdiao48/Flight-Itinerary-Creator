#ifndef PROVIDED_H_
#define PROVIDED_H_
#include <string>
#include <unordered_map>
#include <vector>
#include <limits>

struct FlightSegment
{
    FlightSegment(std::string airline, int flight_no, std::string source_airport,
                  std::string destination_airport, int departure_time, int duration_sec, double price)
     : airline(airline), flight_no(flight_no), source_airport(source_airport),
       destination_airport(destination_airport), departure_time(departure_time),
    duration_sec(duration_sec), price(price)
    {}

    std::string airline;
    int flight_no;
    std::string source_airport;
    std::string destination_airport;
    int departure_time;
    int duration_sec;
    double price;
};

struct Itinerary
{
    std::string source_airport;
    std::string destination_airport;
    std::vector<FlightSegment> flights;
    int total_duration;  // from beginning at the first airport until deplaning at the last airport
    double total_cost;
};

class AirportDB
{
  public:
    bool load_airport_data(std::string filename);
    bool get_distance(std::string source_airport, std::string destination_airport, double& distance) const;

  private:
    struct GeoLocation
    {
        double latitude;
        double longitude;
    };

    std::unordered_map<std::string, GeoLocation> m_airports;
   
    double haversineDistance(const GeoLocation& l1, const GeoLocation& l2) const;
};

class FlightManagerBase
{
  public:
    FlightManagerBase() {}
    virtual ~FlightManagerBase() {}

    virtual bool load_flight_data(std::string filename) = 0;
    virtual std::vector<FlightSegment> find_flights(std::string source_airport, int start_time, int end_time) const = 0;
};

class TravelPlannerBase
{
public:
    TravelPlannerBase(const FlightManagerBase& flight_manager, const AirportDB& airport_db)
     : m_flight_manager(flight_manager), m_airport_db(airport_db),
       m_max_connections(std::numeric_limits<int>::max()),
       m_max_duration(24 * 3600),   // 24 hrs until set otherwise
       m_max_layover(12 * 3600),    // 12 hrs until set otherwise
       m_min_connection_time(3600)  // 1 hr until set otherwise
    {}
    virtual ~TravelPlannerBase() {}

    virtual bool plan_travel(std::string source_airport, std::string destination_airport,
                             int start_time, Itinerary& itinerary) const = 0;
    virtual void add_preferred_airline(std::string airline) = 0;

    const FlightManagerBase& get_flight_manager() const
    {
        return m_flight_manager;
    }

    const AirportDB& get_airport_db() const
    {
        return m_airport_db;
    }

    void set_max_duration(int max_duration)
    {
        m_max_duration = max_duration;
    }

    int get_max_duration() const
    {
        return m_max_duration;
    }

    void set_max_layover(int max_layover)
    {
        m_max_layover = max_layover;
    }

    int get_max_layover() const
    {
        return m_max_layover;
    }

    void set_min_connection_time(int min_connection_time)
    {
        m_min_connection_time = min_connection_time;
    }

    int get_min_connection_time() const
    {
        return m_min_connection_time;
    }

private:
    const FlightManagerBase& m_flight_manager;
    const AirportDB& m_airport_db;
    int m_max_connections;
    int m_max_duration;
    int m_max_layover;
    int m_min_connection_time;
};

  // When testing, you may call this to check the validity of an Itinerary.
  // Returns true if valid; if invalid, puts an error message in errMsg and
  // returns false.
bool validate_itinerary(
    const FlightManagerBase& flightManager,
    const Itinerary& itinerary,
    int min_connection_time,
    int max_layover,
    std::string& errMsg);

#endif // PROVIDED_H_

