#include <iostream>
#include <winsock2.h>
#include <Ws2tcpip.h>
#include <opencv2/opencv.hpp>

#pragma comment(lib, "ws2_32.lib") // Link with Winsock library

void initializeWinsock() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "Failed to initialize Winsock." << std::endl;
        exit(EXIT_FAILURE);
    }
}

SOCKET createSocket() {
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET) {
        std::cerr << "Socket creation failed with error: " << WSAGetLastError() << std::endl;
        WSACleanup();
        exit(EXIT_FAILURE);
    }
    return sock;
}

void bindSocket(SOCKET sock, struct sockaddr_in& server) {
    if (bind(sock, (struct sockaddr*)&server, sizeof(server)) == SOCKET_ERROR) {
        std::cerr << "Bind failed with error: " << WSAGetLastError() << std::endl;
        closesocket(sock);
        WSACleanup();
        exit(EXIT_FAILURE);
    }
}

void displayFrameRate(int& frame_count, double& last_time, double& fps) {
    frame_count++;
    if (frame_count % 10 == 0) { // Calculate FPS every 10 frames
        double current_time = static_cast<double>(cv::getTickCount());
        double delta_time = (current_time - last_time) / cv::getTickFrequency();
        fps = 10.0 / delta_time;
        last_time = current_time;
        std::cout << "FPS: " << fps << std::endl;
    }
}

void receiveAndDisplayFrames(SOCKET sock) {
    struct sockaddr_in client;
    int client_size = sizeof(client);
    char buffer[65536];
    double last_time = static_cast<double>(cv::getTickCount());
    int frame_count = 0;
    double fps = 0.0;

    while (true) {
        int received_len = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr*)&client, &client_size);
        if (received_len == SOCKET_ERROR) {
            std::cerr << "Receive failed with error: " << WSAGetLastError() << std::endl;
            break;
        }

        if (received_len < sizeof(int64_t)) {
            std::cerr << "Received packet is too small." << std::endl;
            continue;
        }

        // Extract the timestamp
        int64_t timestamp;
        memcpy(&timestamp, buffer + received_len - sizeof(int64_t), sizeof(int64_t));

        // Extract the image data
        std::vector<uchar> data(buffer, buffer + received_len - sizeof(int64_t));
        cv::Mat frame = cv::imdecode(data, cv::IMREAD_COLOR); // Decode image
        if (frame.empty()) {
            std::cerr << "Failed to decode frame." << std::endl;
            continue;
        }

        displayFrameRate(frame_count, last_time, fps);

        cv::imshow("Received Frame", frame);
        if (cv::waitKey(5) >= 0) break;
    }
}
int main() {
    initializeWinsock();

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(8888);
    inet_pton(AF_INET, "127.0.0.1", &server.sin_addr);

    SOCKET sock = createSocket();
    bindSocket(sock, server);

    receiveAndDisplayFrames(sock);

    closesocket(sock);
    WSACleanup(); // Cleanup Winsock

    return 0;
}
