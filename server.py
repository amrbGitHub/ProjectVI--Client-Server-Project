import socket
import threading
from datetime import datetime

# Thread-safe storage for airplane data
lock = threading.Lock()
airplanes = {}

def handle_client(conn, addr):
    print(f"New connection from {addr}")
    client_id = None
    prev_time = None
    prev_fuel = None
    fuel_consumption_data = []
    try:
        while True:
            data = conn.recv(1024).decode()
            if not data:
                break
            # Process each line of data
            for line in data.strip().split('\n'):
                # Parse the line 
                parts = line.strip().split(',')
                if len(parts) != 3:
                    continue  
                client_id, time_str, fuel_str = parts
                try:
                    # Parse timestamp and fuel remaining
                    current_time = datetime.strptime(time_str, "%d_%m_%Y %H:%M:%S")
                    current_fuel = float(fuel_str)
                except ValueError:
                    continue  
                # Calculate fuel consumption rate
                if prev_time and prev_fuel:
                    time_diff = (current_time - prev_time).total_seconds() / 3600  # Hours
                    fuel_diff = prev_fuel - current_fuel  # Gallons
                    if time_diff > 0:
                        rate = fuel_diff / time_diff  # Gallons/hr
                        fuel_consumption_data.append(rate)
                        print(f"Client {client_id}: Time = {time_str}, Fuel = {current_fuel:.2f} gal, Rate = {rate:.2f} gal/hr")
                prev_time, prev_fuel = current_time, current_fuel
        # Store the final average fuel consumption
        if fuel_consumption_data:
            avg_fuel_consumption = sum(fuel_consumption_data) / len(fuel_consumption_data)
        else:
            avg_fuel_consumption = 0.0
        with lock:
            if client_id not in airplanes:
                airplanes[client_id] = {'flights': [], 'overall_avg': 0.0}
            airplanes[client_id]['flights'].append(avg_fuel_consumption)
            # Update average fuel comsumption
            total = sum(airplanes[client_id]['flights'])
            count = len(airplanes[client_id]['flights'])
            airplanes[client_id]['overall_avg'] = total / count
        print(f"Client {client_id} finished. Average fuel consumption: {avg_fuel_consumption:.2f} gal/hr")
    finally:
        conn.close()

def main():
    host = '0.0.0.0'  # Listen on all interfaces
    port = 12345
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind((host, port))
        s.listen()
        print(f"Server listening on {host}:{port}")
        while True:
            conn, addr = s.accept()
            thread = threading.Thread(target=handle_client, args=(conn, addr))
            thread.start()

if __name__ == '__main__':
    main()