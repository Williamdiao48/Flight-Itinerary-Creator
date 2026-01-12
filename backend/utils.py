import csv


def write_flights_to_csv(flights, filename):
    with open(filename, mode = 'w', newline = '') as file:
        writer = csv.writer(file)
        for f in flights:
            writer.writerow([f['airline'], f['flight_no'], f['source_airport'], f['destination_airport'], f['departure_time'], f['arrival_time'], f['duration_sec'], float(f['price'])])