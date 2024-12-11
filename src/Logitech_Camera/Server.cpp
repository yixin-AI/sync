#include <iostream>
#include <winsock2.h>
#include <Ws2tcpip.h>
#include <opencv2/opencv.hpp>

#pragma comment(lib, "ws2_32.lib") // Link with Winsock library

int main() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData); // Initialize Winsock

    struct sockaddr_in server, client;
    int client_size = sizeof(client);

    server.sin_family = AF_INET;
    server.sin_port = htons(8888);
    inet_pton(AF_INET, "192.168.0.2", &server.sin_addr); // 

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == SOCKET_ERROR) {
        std::cerr << "Socket creation failed.\n";
        return 1;
    }

    bind(sock, (struct sockaddr*)&server, sizeof(server));

    char buffer[65536];
    double last_time = static_cast<double>(cv::getTickCount());
    int frame_count = 0;
    double fps = 0.0;

    while (true) {
        int received_len = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr*)&client, &client_size);
        if (received_len == SOCKET_ERROR) {
            std::cerr << "Receive failed.\n";
            break;
        }

        std::vector<uchar> data(buffer, buffer + received_len);
        cv::Mat frame = cv::imdecode(data, cv::IMREAD_COLOR); // Decode image
        if (frame.empty()) continue;

        frame_count++;
        if (frame_count % 10 == 0) { // Calculate FPS every 10 frames
            double current_time = static_cast<double>(cv::getTickCount());
            double delta_time = (current_time - last_time) / cv::getTickFrequency();
            fps = 10.0 / delta_time;
            last_time = current_time;
            std::cout << "FPS: " << fps << std::endl;
        }

        cv::imshow("Received Frame", frame);
        if (cv::waitKey(5) >= 0) break;
    }

    closesocket(sock);
    WSACleanup(); // Cleanup Winsock
    return 0;
}
