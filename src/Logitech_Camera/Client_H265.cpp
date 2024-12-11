#include <iostream>
#include <winsock2.h>
#include <Ws2tcpip.h>
#include <opencv2/opencv.hpp>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

#pragma comment(lib, "ws2_32.lib")

void send_packet_with_timestamp(SOCKET sock, AVPacket* packet, const sockaddr_in& server, int64_t millis) {

    

    //std::cout << sizeof(millis) << std::endl;
    


    size_t new_size = packet->size + sizeof(millis);


    std::vector<char> new_packet(packet->size + sizeof(millis));
    memcpy(new_packet.data(), packet->data, packet->size);
    memcpy(new_packet.data() + packet->size, &millis, sizeof(millis));


    sendto(sock, new_packet.data(), new_size, 0, (struct sockaddr*)&server, sizeof(server));
}


int main() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(8888);
    inet_pton(AF_INET, "192.168.0.1", &server.sin_addr);

    int server_size = sizeof(server);
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == SOCKET_ERROR) {
        std::cerr << "Socket creation failed.\n";
        return 1;
    }

    cv::VideoCapture cap(0); // Open default camera
    if (!cap.isOpened()) {
        std::cerr << "Cannot open the video camera.\n";
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    double width = 1920, height = 1080;
    cap.set(cv::CAP_PROP_FRAME_WIDTH, width);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, height);

    cv::namedWindow("Live Video", cv::WINDOW_AUTOSIZE);

    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_HEVC);
    AVCodecContext* codecContext = avcodec_alloc_context3(codec);
    codecContext->bit_rate = 400000; // Set bitrate
    codecContext->width = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
    codecContext->height = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
    codecContext->time_base = { 1,30 };
    codecContext->framerate = { 30, 1 };
    codecContext->gop_size = 15;
    codecContext->max_b_frames = 1;
    codecContext->pix_fmt = AV_PIX_FMT_YUV420P;

    if (avcodec_open2(codecContext, codec, NULL) < 0) {
        std::cerr << "Failed to open codec\n";
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    SwsContext* sws_ctx = sws_getContext(codecContext->width, codecContext->height, AV_PIX_FMT_BGR24,
        codecContext->width, codecContext->height, AV_PIX_FMT_YUV420P,
        SWS_BILINEAR, NULL, NULL, NULL);

    AVFrame* avFrame = av_frame_alloc();
    if (!avFrame) {
        std::cerr << "Could not allocate video frame\n";
        exit(1);
    }
    avFrame->format = codecContext->pix_fmt; // 
    avFrame->width = codecContext->width;
    avFrame->height = codecContext->height;

    // Allocate the buffers for the frame data
    if (av_image_alloc(avFrame->data, avFrame->linesize, avFrame->width, avFrame->height, (AVPixelFormat)avFrame->format, 32) < 0) {
        std::cerr << "Could not allocate raw picture buffer\n";
        exit(1);
    }


    AVPacket* packet = av_packet_alloc();

    int frame_count = 0;
    cv::Mat frame;
    while (true) {
        cap >> frame; // Capture a new frame


        //get timestamp
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        int64_t millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

        if (frame.empty()) break;

        //cv::imshow("Live Video", frame);
        if (cv::waitKey(5) >= 0) break;

        // Convert the image from BGR to YUV
        uint8_t* inData[1] = { frame.data };
        int inLinesize[1] = { static_cast<int>(frame.step) };
        sws_scale(sws_ctx, inData, inLinesize, 0, frame.rows, avFrame->data, avFrame->linesize);

        avFrame->pts = frame_count++;

        // Encode the frame
        if (avcodec_send_frame(codecContext, avFrame) < 0) {
            std::cerr << "Error sending frame for encoding\n";
            continue;
        }

        while (avcodec_receive_packet(codecContext, packet) == 0) {
            
            send_packet_with_timestamp(sock, packet, server, millis);
            av_packet_unref(packet); // Reset packet for the next usage
        }
    }

    // Cleanup
    av_freep(&avFrame->data[0]);
    av_frame_free(&avFrame);
    av_packet_free(&packet);
    avcodec_free_context(&codecContext);
    sws_freeContext(sws_ctx);
    closesocket(sock);
    WSACleanup();
    cv::destroyWindow("Live Video");
    return 0;
}
