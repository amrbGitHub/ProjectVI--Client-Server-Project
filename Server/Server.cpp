#include <iostream>
#include <thread>
#include <map>
#include <string>
#include <vector>
#include <sstream>
#include <chrono>
#include <winsock2.h>
#pragma comment(lib, "Ws2_32.lib")

#define PORT 54000
#define MAX_BUFFER 1024
#define CLIENT_TIMEOUT_SECONDS 60 // Timeout after 60 seconds of no data

// Structure to store client's fuel consumption data
struct FuelData {
    std::vector<std::pair<std::chrono::steady_clock::time_point, double>> fuelReadings; // Stores (timestamp, fuelRemaining)
    double totalFuelConsumed = 0.0;    // Total fuel consumed for the session
    double fuelConsumptionValue = 0.0;  // Fuel consumption value in liters (used for fuel consumption calculations)
};

// Function to receive data from the client
std::string readData(SOCKET clientSocket) {
    char buffer[MAX_BUFFER];
    int bytesReceived = recv(clientSocket, buffer, MAX_BUFFER, 0);
    if (bytesReceived > 0) {
        return std::string(buffer, bytesReceived);
    }
    return "";
}

// Function to parse the data received from the client
bool parseData(const std::string& data, double& fuelRemaining) {
    size_t fuelPos = data.find("fuel: ");
    if (fuelPos != std::string::npos) {
        try {
            fuelRemaining = std::stod(data.substr(fuelPos + 6));
            return true;
        }
        catch (const std::invalid_argument&) {
            std::cerr << "Error parsing fuel data: " << data << std::endl;
        }
    }
    return false;
}

// Function to calculate and store the final fuel consumption value
void calculateFuelConsumptionValue(int clientID, FuelData& fuelData) {
    if (fuelData.fuelReadings.size() < 2) {
        std::cout << "Not enough data to calculate fuel consumption for Client " << clientID << std::endl;
        return;
    }

    double totalFuel = 0.0;
    auto startTime = fuelData.fuelReadings.front().first;
    auto endTime = fuelData.fuelReadings.back().first;

    for (size_t i = 1; i < fuelData.fuelReadings.size(); ++i) {
        double prevFuel = fuelData.fuelReadings[i - 1].second;
        double currFuel = fuelData.fuelReadings[i].second;
        totalFuel += (prevFuel - currFuel);
    }

    // Calculate total time in seconds
    double totalTimeElapsed = std::chrono::duration<double>(endTime - startTime).count();

    // Calculate fuel consumption value (total fuel consumed)
    fuelData.totalFuelConsumed = totalFuel;
    fuelData.fuelConsumptionValue = totalFuel / totalTimeElapsed;  // Fuel consumption value per time unit

    std::cout << "Client " << clientID << " - Final Total Fuel Consumed: " << fuelData.totalFuelConsumed << " liters" << std::endl;
    std::cout << "Client " << clientID << " - Final Fuel Consumption Value: " << fuelData.fuelConsumptionValue << " L/s" << std::endl;
}

// Function to handle client communication
void handleClient(SOCKET clientSocket) {
    std::string idMessage = readData(clientSocket);
    int clientID = 0;
    if (idMessage.find("ClientID:") != std::string::npos) {
        clientID = std::stoi(idMessage.substr(9));  // Extract client ID
    }

    std::cout << "Connected to Client " << clientID << ":" << std::endl;

    // Define a map to store fuel data for each client
    std::map<int, FuelData> clientFuelData;

    while (true) {
        std::string data = readData(clientSocket);
        if (data.empty()) {
            std::cout << "\nNo more data received from Client " << clientID << ", terminating connection..." << std::endl;
            break;
        }

        std::cout << "Received from Client " << clientID << ": " << data << std::endl;

        // Parse the fuel data
        double fuelRemaining = 0.0;
        if (!parseData(data, fuelRemaining)) {
            continue; // Skip if parsing failed
        }

        // Store the fuel reading with a timestamp
        auto currentTime = std::chrono::steady_clock::now();
        clientFuelData[clientID].fuelReadings.emplace_back(currentTime, fuelRemaining);

        // Check if at least two readings exist to compute consumption
        if (clientFuelData[clientID].fuelReadings.size() > 1) {
            double previousFuel = clientFuelData[clientID].fuelReadings[clientFuelData[clientID].fuelReadings.size() - 2].second;
            double fuelConsumed = previousFuel - fuelRemaining;
            clientFuelData[clientID].totalFuelConsumed += fuelConsumed;
        }

        // Check for timeout
        auto elapsedTime = std::chrono::duration_cast<std::chrono::seconds>(currentTime - clientFuelData[clientID].fuelReadings.back().first).count();
        if (elapsedTime >= CLIENT_TIMEOUT_SECONDS) {
            std::cout << "Flight has ended for Client " << clientID << ", calculating fuel consumption value..." << std::endl;
            calculateFuelConsumptionValue(clientID, clientFuelData[clientID]);
            break;
        }
    }

    // Ensure final calculation if client disconnects or session ends
    calculateFuelConsumptionValue(clientID, clientFuelData[clientID]);

    // Close the client socket
    closesocket(clientSocket);
}

int main() {
    WSADATA wsaData;
    SOCKET serverSocket, clientSocket;
    sockaddr_in serverAddr, clientAddr;
    int clientSize = sizeof(clientAddr);

    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed!" << std::endl;
        return 1;
    }

    // Create the server socket
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed!" << std::endl;
        WSACleanup();
        return 1;
    }

    // Set up the server address
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    // Bind the socket
    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed!" << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    // Listen for incoming connections
    if (listen(serverSocket, 5) == SOCKET_ERROR) {
        std::cerr << "Listen failed!" << std::endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Server is listening on port " << PORT << std::endl;

    while (true) {
        clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientSize);
        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "Accept failed!" << std::endl;
            continue;
        }

        // Create a new thread to handle the client
        std::thread clientThread(handleClient, clientSocket);
        clientThread.detach();
    }

    // Clean up (this part will never be reached due to infinite loop)
    closesocket(serverSocket);
    WSACleanup();
    return 0;
}
