#include "fm.h"
#include "provided.h"
#include "tp.h"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(flight_planner_cpp, m) {
  py::class_<FlightSegment>(m, "FlightSegment")
      .def(py::init<std::string, int, std::string, std::string, int, int,
                    double>())
      .def_readwrite("airline", &FlightSegment::airline)
      .def_readwrite("flight_no", &FlightSegment::flight_no)
      .def_readwrite("source_airport", &FlightSegment::source_airport)
      .def_readwrite("destination_airport", &FlightSegment::destination_airport)
      .def_readwrite("departure_time", &FlightSegment::departure_time)
      .def_readwrite("duration_sec", &FlightSegment::duration_sec)
      .def_readwrite("price", &FlightSegment::price);

  py::class_<Itinerary>(m, "Itinerary")
      .def(py::init<>())
      .def_readwrite("source_airport", &Itinerary::source_airport)
      .def_readwrite("destination_airport", &Itinerary::destination_airport)
      .def_readwrite("flights", &Itinerary::flights)
      .def_readwrite("total_duration", &Itinerary::total_duration)
      .def_readwrite("total_cost", &Itinerary::total_cost);

  py::class_<AirportDB>(m, "AirportDB")
      .def(py::init<>())
      .def("load_airport_data", &AirportDB::load_airport_data)
      .def("get_distance", &AirportDB::get_distance);

  py::class_<FlightManagerBase>(m, "FlightManagerBase");

  py::class_<FlightManager, FlightManagerBase>(m, "FlightManager")
      .def(py::init<>())
      .def("load_flight_data", &FlightManager::load_flight_data)
      .def("add_flight", &FlightManager::add_flight)
      .def("find_flights", &FlightManager::find_flights);

  py::enum_<searchMode>(m, "SearchMode")
      .value("FRUGAL", FRUGAL)
      .value("BALANCED", BALANCED)
      .value("FAST", FAST)
      .export_values();

  py::class_<TravelPlannerBase>(m, "TravelPlannerBase");

  py::class_<TravelPlanner, TravelPlannerBase>(m, "TravelPlanner")
      .def(py::init<const FlightManagerBase &, const AirportDB &, searchMode>())
      .def("add_preferred_airline", &TravelPlanner::add_preferred_airline)
      .def("plan_travel", &TravelPlanner::plan_travel,
           py::arg("source_airport"), py::arg("destination_airport"),
           py::arg("start_window"), py::arg("end_window"), py::arg("max_results") = 3)
      .def("set_max_duration", &TravelPlanner::set_max_duration)
      .def("get_max_duration", &TravelPlanner::get_max_duration)
      .def("set_max_price", &TravelPlanner::set_max_price)
      .def("get_max_price", &TravelPlanner::get_max_price)
      .def("set_max_layover", &TravelPlanner::set_max_layover)
      .def("get_max_layover", &TravelPlanner::get_max_layover)
      .def("set_min_connection_time", &TravelPlanner::set_min_connection_time)
      .def("get_min_connection_time", &TravelPlanner::get_min_connection_time)
      .def("set_max_connections", &TravelPlanner::set_max_connections);

  m.def("validate_itinerary", &validate_itinerary);
}
