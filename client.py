import socket
import uuid
import sys
import time

def main():
    if len(sys.argv) != 4:
        print("Usage: client.py <server_ip> <server_port> <telemetry_file>")
        return
    server_ip, server_port, file = sys.argv[1], int(sys.argv[2]), sys.argv[3]
    client_id = str(uuid.uuid4())  # Unique ID
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.connect((server_ip, server_port))
            with open(file, 'r') as f:
                next(f)  # Skip header
                for line in f:
                    line = line.strip().rstrip(',')
                    if not line:
                        continue
                    parts = line.split(',')
                    if len(parts) < 2:
                        continue
                    timestamp, fuel = parts[0].strip(), parts[1].strip()
                    # Packetize the data (format: <client_id>,<timestamp>,<fuel_remaining>)
                    message = f"{client_id},{timestamp},{fuel}\n"
                    s.sendall(message.encode())
                    time.sleep(0.1)  
    except Exception as e:
        print(f"Client error: {e}")

if __name__ == '__main__':
    main()