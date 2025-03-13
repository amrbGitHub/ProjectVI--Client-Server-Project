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
    std::vector<double> fuelReadings;  // Store all fuel readings
    double totalFuelConsumed = 0.0;    // Total fuel consumed for the session
    double averageFuelConsumption = 0.0; // Final average fuel consumption when the flight ends
    std::chrono::steady_clock::time_point lastDataTime; // Track last received data timestamp
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
void parseData(const std::string& data, double& fuelRemaining) {
    size_t fuelPos = data.find("fuel: ");
    if (fuelPos != std::string::npos) {
        try {
            fuelRemaining = std::stod(data.substr(fuelPos + 6));
        }
        catch (const std::invalid_argument& e) {
            std::cerr << "Error parsing fuel data: " << data << std::endl;
        }
    }
}

// Function to calculate and store the final average fuel consumption for the client
void calculateAverageFuelConsumption(int clientID, FuelData& fuelData) {
    double totalFuel = 0.0;
    for (size_t i = 1; i < fuelData.fuelReadings.size(); ++i) {
        double fuelConsumed = fuelData.fuelReadings[i - 1] - fuelData.fuelReadings[i];
        totalFuel += fuelConsumed;
    }

    // Calculate average fuel consumption for the entire flight
    if (fuelData.fuelReadings.size() > 1) {
        fuelData.averageFuelConsumption = totalFuel / (fuelData.fuelReadings.size() - 1);
    }
    else {
        fuelData.averageFuelConsumption = 0.0; // No fuel data received
    }

    std::cout << "Final total fuel consumed for Client " << clientID << ": "
        << fuelData.totalFuelConsumed << " liters" << std::endl;
    std::cout << "Final average fuel consumption for Client " << clientID << ": "
        << fuelData.averageFuelConsumption << " liters" << std::endl;
}

// Function to handle client communication
void handleClient(SOCKET clientSocket) {
    // Read the client's ID (sent from the client at the start of communication)
    std::string idMessage = readData(clientSocket);
    int clientID = 0;
    if (idMessage.find("ClientID:") != std::string::npos) {
        clientID = std::stoi(idMessage.substr(9));  // Extract client ID
    }

    std::cout << "Connected to Client " << clientID << ":" << std::endl;

    // Define a map to store fuel data for each client
    std::map<int, FuelData> clientFuelData;

    // Continuously read and process transmitted data from the client
    while (true) {
        std::string data = readData(clientSocket);
        if (data.empty()) {
            std::cout << "\n" << std::endl;
            std::cout << "No more data received from client, terminating connection..." << std::endl;
            break;  // Stop if no data is received (client may have disconnected)
        }

        std::cout << "Received from Client " << clientID << ": " << data << std::endl;

        // Parse the fuel data
        double fuelRemaining = 0.0;
        parseData(data, fuelRemaining);

        // Store the fuel reading and update the last data timestamp
        clientFuelData[clientID].fuelReadings.push_back(fuelRemaining);
        clientFuelData[clientID].lastDataTime = std::chrono::steady_clock::now();

        // Calculate fuel consumption between the last two readings
        if (clientFuelData[clientID].fuelReadings.size() > 1) {
            double previousFuel = clientFuelData[clientID].fuelReadings[clientFuelData[clientID].fuelReadings.size() - 2];
            double fuelConsumed = previousFuel - fuelRemaining;
            clientFuelData[clientID].totalFuelConsumed += fuelConsumed;
        }

        // Check if the client has timed out based on no data for CLIENT_TIMEOUT_SECONDS
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsedTime = std::chrono::duration_cast<std::chrono::seconds>(currentTime - clientFuelData[clientID].lastDataTime).count();
        if (elapsedTime >= CLIENT_TIMEOUT_SECONDS) {
            std::cout << "Flight has ended for Client " << clientID << ", calculating final average fuel consumption..." << std::endl;
            calculateAverageFuelConsumption(clientID, clientFuelData[clientID]);
            break; // End the loop after calculating
        }
    }

    // Ensure final calculation if client disconnects or session ends
    calculateAverageFuelConsumption(clientID, clientFuelData[clientID]);

    // Close the client socket after processing
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

    // Main server loop to accept and spawn threads for each client
    while (true) {
        clientSocket = accept(serverSocket, (sockaddr*)&clientAddr, &clientSize);
        if (clientSocket == INVALID_SOCKET) {
            std::cerr << "Accept failed!" << std::endl;
            continue;
        }

        // Create a new thread to handle the client
        std::thread clientThread(handleClient, clientSocket);
        clientThread.detach(); // Detach the thread so it runs independently
    }

    // Clean up (this part will never be reached due to infinite loop)
    closesocket(serverSocket);
    WSACleanup();
    return 0;
}
