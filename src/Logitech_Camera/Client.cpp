#include <iostream>
#include <winsock2.h>
#include <Ws2tcpip.h> 
#include <opencv2/opencv.hpp>

#pragma comment(lib, "ws2_32.lib") // Link with Winsock library

int main() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData); // Initialize Winsock

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(8888);
    inet_pton(AF_INET, "192.168.0.2", &server.sin_addr); 

    int server_size = sizeof(server);

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == SOCKET_ERROR) {
        std::cerr << "Socket creation failed.\n";
        return 1;
    }

    cv::VideoCapture cap(0); // Open the default camera
    if (!cap.isOpened()) {
        std::cerr << "Cannot open the video camera.\n";
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    cv::namedWindow("Live Video", cv::WINDOW_AUTOSIZE); // Create a window for display.

    cv::Mat frame;

    double time_start = static_cast<double>(cv::getTickCount());
    int frame_count = 0;
    double fps = 0.0;

    while (true) {
        cap >> frame; // Capture a new frame
        if (frame.empty()) break;

        // Display the resulting frame
        cv::imshow("Live Video", frame);
        if (cv::waitKey(5) >= 0) break; // Wait for a keystroke in the window

        frame_count++;
        if (frame_count % 10 == 0) { // Calculate FPS every 10 frames
            double current_time = static_cast<double>(cv::getTickCount());
            double delta_time = (current_time - time_start) / cv::getTickFrequency();
            fps = 10.0 / delta_time;
            time_start = current_time;
            std::cout << "FPS: " << fps << std::endl;
        }
        std::vector<uchar> buffer;
        cv::imencode(".jpg", frame, buffer); // Encode frame to JPEG
        int buffer_size = static_cast<int>(buffer.size()); 
        sendto(sock, reinterpret_cast<const char*>(buffer.data()), buffer_size, 0,
            (struct sockaddr*)&server, server_size);
    }

    closesocket(sock);
    WSACleanup(); // Cleanup Winsock
    cv::destroyWindow("Live Video"); // Close the display window
    return 0;
}
