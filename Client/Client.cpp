#include <iostream>
#include <fstream>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>  // For inet_pton
#include <cstdlib>      // For std::rand() and std::srand
#include <ctime>        // For std::time()
#pragma comment(lib, "Ws2_32.lib")

#define PORT 54000
#define MAX_BUFFER 1024
#define SERVERIP "127.0.0.1"    //Replace with IP of actual Server
#define DATAFILE "katl-kefd-B737-700.txt"      //This can be changed to any of the 4 Telem files provided 

// Function to generate a random client ID
int generateClientID() {
    std::srand(std::time(0));
    return std::rand();
}

// Function to read fuel data and send it quickly
void sendData(const std::string& filename, SOCKET clientSocket, int clientID) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open file!" << std::endl;
        return;
    }

    // Send Client ID
    std::string clientIDMessage = "ClientID:" + std::to_string(clientID);
    send(clientSocket, clientIDMessage.c_str(), clientIDMessage.length(), 0);
    std::cout << "Sent Client ID to server: " << clientIDMessage << std::endl;

    std::string line;
    while (std::getline(file, line)) {
        // Parse timestamp and fuel data
        size_t commaPos = line.find(",");
        if (commaPos != std::string::npos) {
            std::string timestamp = line.substr(0, commaPos);
            std::string fuelRemainingStr = line.substr(commaPos + 1);
            fuelRemainingStr.erase(fuelRemainingStr.find_last_not_of(" \n\r\t") + 1);

            double fuelRemaining = 0.0;
            try {
                fuelRemaining = std::stod(fuelRemainingStr);
            }
            catch (...) {
                std::cerr << "Invalid fuel data: " << fuelRemainingStr << " - Skipping" << std::endl;
                continue;
            }

            // Send formatted data
            std::string dataToSend = timestamp + " | fuel: " + std::to_string(fuelRemaining);
            send(clientSocket, dataToSend.c_str(), dataToSend.length(), 0);
            std::cout << "Client " << clientID << " sent: " << dataToSend << std::endl;
        }
    }

    file.close();
}

int main() {
    WSADATA wsaData;
    SOCKET clientSocket;
    sockaddr_in serverAddr;

    int clientID = generateClientID();
    std::cout << "Generated Client ID: " << clientID << std::endl;

    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed!" << std::endl;
        return 1;
    }

    // Create socket
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed!" << std::endl;
        WSACleanup();
        return 1;
    }

    // Disable Nagle's Algorithm for fast sending
    int flag = 1;
    setsockopt(clientSocket, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(int));

    // Set up server address
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, SERVERIP, &serverAddr.sin_addr) <= 0) {
        std::cerr << "Invalid address!" << std::endl;
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    // Connect to the server
    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Connection failed!" << std::endl;
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    // Send fuel data quickly
    sendData(DATAFILE, clientSocket, clientID);

    // Shut down the connection cleanly
    shutdown(clientSocket, SD_SEND);
    closesocket(clientSocket);
    WSACleanup();

    std::cout << "\n" << std::endl;
    std::cout << "All data sent, client disconnecting..." << std::endl;
    return 0;
}
