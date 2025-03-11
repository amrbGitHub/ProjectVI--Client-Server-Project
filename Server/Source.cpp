#include <iostream>
#include <thread>
#include <map>
#include <string>
#include <winsock2.h>
#pragma comment(lib, "Ws2_32.lib")

#define PORT 54000
#define MAX_BUFFER 1024

// Function to receive data from the client
std::string readData(SOCKET clientSocket) {
    char buffer[MAX_BUFFER];
    int bytesReceived = recv(clientSocket, buffer, MAX_BUFFER, 0);
    if (bytesReceived > 0) {
        return std::string(buffer, bytesReceived);
    }
    else {
        return "";
    }
}

// Function to handle client communication
void handleClient(SOCKET clientSocket) {
    // Step 1: Read the client's ID (sent from the client at the start of communication)
    std::string idMessage = readData(clientSocket);
    int clientID = 0;
    if (idMessage.find("ClientID:") != std::string::npos) {
        clientID = std::stoi(idMessage.substr(9));  // Extract the client ID from the message
    }

    std::cout << "Connected to Client " << clientID << ":" << std::endl;

    // Step 2: Read and process the transmitted data from the client
    while (true) {
        std::string data = readData(clientSocket);
        if (data.empty()) {
            std::cout << "No data received from client, terminating connection..." << std::endl;
            break;  // Stop if no data is received (EOF or client disconnect)
        }

        // Step 3: Display the received data along with the client ID
        std::cout << "Received from Client " << clientID << ": " << data << std::endl;
    }

    // Close the client socket connection after processing
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
            continue;  // Try accepting new connections if this one failed
        }

        // Create a new thread to handle the client
        std::thread clientThread(handleClient, clientSocket);
        clientThread.detach();  // Detach the thread so it runs independently
    }

    // Clean up (this part will never be reached due to infinite loop)
    closesocket(serverSocket);
    WSACleanup();
    return 0;
}
