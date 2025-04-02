#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <chrono>
#include <thread>
#include <random>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib")

// Generate Unique Client ID 
std::string generateUUID() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    std::uniform_int_distribution<> dis2(8, 11);

    std::stringstream ss;
    ss << std::hex;

    for (int i = 0; i < 8; i++) {
        ss << dis(gen);
    }
    ss << "-";
    for (int i = 0; i < 4; i++) {
        ss << dis(gen);
    }
    ss << "-4"; // UUID version 4
    for (int i = 0; i < 3; i++) {
        ss << dis(gen);
    }
    ss << "-";
    ss << dis2(gen);
    for (int i = 0; i < 3; i++) {
        ss << dis(gen);
    }
    ss << "-";
    for (int i = 0; i < 12; i++) {
        ss << dis(gen);
    }

    return ss.str();
}

int main(int argc, char* argv[]) {
    // command line args
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <server_ip> <server_port> <telemetry_file>" << std::endl;
        return 1;
    }

    const std::string server_ip = argv[1];
    const int server_port = std::stoi(argv[2]);
    const std::string telemetry_file = argv[3];

    // Initialize Winsock
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cerr << "WSAStartup failed: " << result << std::endl;
        return 1;
    }

    std::string client_id = generateUUID();

    try {
        // Create socket
        SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "socket failed: " << WSAGetLastError() << std::endl;
            WSACleanup();
            return 1;
        }

        // prepare server address
        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(server_port);
        inet_pton(AF_INET, server_ip.c_str(), &serverAddr.sin_addr);

        // Connect
        if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
            std::cerr << "connect failed: " << WSAGetLastError() << std::endl;
            closesocket(clientSocket);
            WSACleanup();
            return 1;
        }

        // Open file
        std::ifstream file(telemetry_file);
        if (!file.is_open()) {
            std::cerr << "Failed to open file: " << telemetry_file << std::endl;
            closesocket(clientSocket);
            WSACleanup();
            return 1;
        }

        std::string line;
        
        std::getline(file, line);

        // Read and send data
        while (std::getline(file, line)) {
            // Remove trailing comma and whitespace
            if (!line.empty() && line.back() == ',') {
                line.pop_back();
            }

            // Seperate the line into different segments
            std::vector<std::string> parts;
            std::istringstream iss(line);
            std::string part;
            while (std::getline(iss, part, ',')) {
                parts.push_back(part);
            }

            if (parts.size() < 2) continue;

            // Get timestamp and fuel information
            std::string timestamp = parts[0];
            std::string fuel = parts[1];

            // Create message (format: <client_id>,<timestamp>,<fuel_remaining>\n)
            std::string message = client_id + "," + timestamp + "," + fuel + "\n";

            // Send message to server
            if (send(clientSocket, message.c_str(), message.length(), 0) == SOCKET_ERROR) {
                std::cerr << "send failed: " << WSAGetLastError() << std::endl;
                break;
            }

            // Sleep for 100ms 
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // Cleanup
        file.close();
        closesocket(clientSocket);
        WSACleanup();

    }
    catch (const std::exception& e) {
        std::cerr << "Client error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}