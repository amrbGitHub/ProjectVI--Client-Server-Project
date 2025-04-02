#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <ctime>
#include <iomanip>
#include <mutex>
#include <thread>
#include <chrono>
#include <atomic>
#include <memory>

#pragma comment(lib, "ws2_32.lib")

// Thread-safe storage
std::mutex data_mutex;
std::map<std::string, struct AirplaneData> airplanes;

struct AirplaneData {
    std::vector<double> flights;
    double overall_avg;
};

// parse datetime string in format "dd_mm_YYYY HH:MM:SS"
bool parseDateTime(const std::string& datetimeStr, std::tm& timeStruct) {
    std::istringstream ss(datetimeStr);
    char delimiter;

    ss >> timeStruct.tm_mday >> delimiter;
    if (delimiter != '_') return false;

    ss >> timeStruct.tm_mon >> delimiter;
    if (delimiter != '_') return false;
    timeStruct.tm_mon -= 1; // tm_mon is 0-based

    ss >> timeStruct.tm_year;
    timeStruct.tm_year -= 1900; // tm_year is years since 1900

    ss >> timeStruct.tm_hour >> delimiter;
    if (delimiter != ':') return false;

    ss >> timeStruct.tm_min >> delimiter;
    if (delimiter != ':') return false;

    ss >> timeStruct.tm_sec;

    return true;
}

// calculate time difference in hours
    double timeDiff(const std::tm& tm1, const std::tm& tm2) {
    auto time1 = std::mktime(const_cast<std::tm*>(&tm1));
    auto time2 = std::mktime(const_cast<std::tm*>(&tm2));
    return std::difftime(time2, time1) / 3600.0; // Convert seconds to hours
}

void handleClient(SOCKET clientSocket, sockaddr_in clientAddr) {
    char ipStr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(clientAddr.sin_addr), ipStr, INET_ADDRSTRLEN);
    std::cout << "New connection from " << ipStr << ":" << ntohs(clientAddr.sin_port) << std::endl;

    std::string clientId;
    std::tm prevTime = { 0 };
    double prevFuel = 0.0;
    bool firstData = true;
    std::vector<double> fuelConsumptionData;
    char buffer[1024];
    int bytesReceived;

    try {
        while ((bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0)) > 0) {
            buffer[bytesReceived] = '\0';
            std::string data(buffer);

            // Process each line
            std::istringstream dataStream(data);
            std::string line;
            while (std::getline(dataStream, line)) {
                // Parse line
                std::vector<std::string> parts;
                std::istringstream lineStream(line);
                std::string part;

                while (std::getline(lineStream, part, ',')) {
                    parts.push_back(part);
                }

                if (parts.size() != 3) continue;

                clientId = parts[0];
                std::string timeStr = parts[1];
                std::string fuelStr = parts[2];

                // Parse timestamp and fuel remaining
                std::tm currentTime = { 0 };
                if (!parseDateTime(timeStr, currentTime)) {
                    continue;
                }

                double currentFuel;
                try {
                    currentFuel = std::stod(fuelStr);
                }
                catch (...) {
                    continue;
                }

                // Calculate fuel consumption rate
                if (!firstData) {
                    double timeDiffHours = timeDiff(prevTime, currentTime);
                    double fuelDiff = prevFuel - currentFuel; // Gallons

                    if (timeDiffHours > 0) {
                        double rate = fuelDiff / timeDiffHours; // Gallons/hr
                        fuelConsumptionData.push_back(rate);

                        std::cout << "Client " << clientId << ": Time = " << timeStr
                            << ", Fuel = " << std::fixed << std::setprecision(2) << currentFuel << " gal"
                            << ", Rate = " << rate << " gal/hr" << std::endl;
                    }
                }
                else {
                    firstData = false;
                }

                prevTime = currentTime;
                prevFuel = currentFuel;
            }
        }

        // Store the final average fuel consumption
        double avgFuelConsumption = 0.0;
        if (!fuelConsumptionData.empty()) {
            double sum = 0.0;
            for (double rate : fuelConsumptionData) {
                sum += rate;
            }
            avgFuelConsumption = sum / fuelConsumptionData.size();
        }

        {
            std::lock_guard<std::mutex> lock(data_mutex);
            if (airplanes.find(clientId) == airplanes.end()) {
                airplanes[clientId] = { std::vector<double>(), 0.0 };
            }

            airplanes[clientId].flights.push_back(avgFuelConsumption);

            // Update average fuel consumption
            double total = 0.0;
            for (double rate : airplanes[clientId].flights) {
                total += rate;
            }
            airplanes[clientId].overall_avg = total / airplanes[clientId].flights.size();
        }

        std::cout << "Client " << clientId << " finished. Average fuel consumption: "
            << std::fixed << std::setprecision(2) << avgFuelConsumption << " gal/hr" << std::endl;
    }
    catch (...) {
        std::cerr << "Error handling client" << std::endl;
    }

    closesocket(clientSocket);
}

int main() {
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cerr << "WSAStartup failed: " << result << std::endl;
        return 1;
    }

    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
        std::cerr << "socket failed: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    // Allow address reuse
    int enable = 1;
    if (setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&enable, sizeof(enable)) < 0) {
        std::cerr << "setsockopt failed" << std::endl;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY; // Listen on any
    serverAddr.sin_port = htons(12345);

    if (bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "bind failed: " << WSAGetLastError() << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "listen failed: " << WSAGetLastError() << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Server listening on port 12345" << std::endl;

    std::vector<std::thread> clientThreads;

    while (true) {
        sockaddr_in clientAddr;
        int clientAddrSize = sizeof(clientAddr);
        SOCKET clientSocket = accept(listenSocket, (sockaddr*)&clientAddr, &clientAddrSize);
        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "accept failed: " << WSAGetLastError() << std::endl;
            continue;
        }

        // Create a new thread for each client and detach it
        clientThreads.emplace_back([clientSocket, clientAddr]() {
            handleClient(clientSocket, clientAddr);
            });
        clientThreads.back().detach();
    }

    // Cleanup 
    closesocket(listenSocket);
    WSACleanup();
    return 0;
}