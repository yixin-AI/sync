#include "VideoStreamer_sw.h"
#include <iostream>


std::queue<int64_t> encodeStartTimes;

VideoStreamer_SW::VideoStreamer_SW() : codecContext(nullptr), sws_ctx(nullptr), avFrame(nullptr), packet(nullptr) {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    setupNetwork();
    setupVideo();
}

VideoStreamer_SW::~VideoStreamer_SW() {
    cleanup();
    WSACleanup();
}

void VideoStreamer_SW::setupNetwork() {
    server.sin_family = AF_INET;
    server.sin_port = htons(8888);
    inet_pton(AF_INET, "192.168.1.131", &server.sin_addr);
    server_size = sizeof(server);
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == SOCKET_ERROR) {
        std::cerr << "Socket creation failed.\n";
        exit(1);
    }
}

void VideoStreamer_SW::setupVideo() {
    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    codecContext = avcodec_alloc_context3(codec);
    codecContext->bit_rate = 1000000;
    //codecContext->width = 1920;
    //codecContext->height = 1080;
    codecContext->width = 640;
    codecContext->height = 480;
    codecContext->time_base = { 1, 30 };
    codecContext->framerate = { 30, 1 };
    codecContext->gop_size = 15;
    codecContext->max_b_frames = 1;
    codecContext->pix_fmt = AV_PIX_FMT_YUV420P;

    if (avcodec_open2(codecContext, codec, NULL) < 0) {
        std::cerr << "Failed to open codec\n";
        exit(1);
    }

    sws_ctx = sws_getContext(codecContext->width, codecContext->height, AV_PIX_FMT_BGR24,
        codecContext->width, codecContext->height, AV_PIX_FMT_YUV420P,
        SWS_BILINEAR, NULL, NULL, NULL);

    avFrame = av_frame_alloc();
    avFrame->format = codecContext->pix_fmt;
    avFrame->width = codecContext->width;
    avFrame->height = codecContext->height;
    av_image_alloc(avFrame->data, avFrame->linesize, avFrame->width, avFrame->height, (AVPixelFormat)avFrame->format, 32);

    packet = av_packet_alloc();
}

void VideoStreamer_SW::cleanup() {
    av_freep(&avFrame->data[0]);
    av_frame_free(&avFrame);
    av_packet_free(&packet);
    avcodec_free_context(&codecContext);
    sws_freeContext(sws_ctx);
    closesocket(sock);
    cv::destroyAllWindows();
}

void VideoStreamer_SW::sendPacketWithTimestamp(SOCKET sock, AVPacket* packet, const sockaddr_in& server, int64_t millis) {
    size_t new_size = packet->size + sizeof(millis);
    std::vector<char> new_packet(packet->size + sizeof(millis));
    memcpy(new_packet.data(), packet->data, packet->size);
    memcpy(new_packet.data() + packet->size, &millis, sizeof(millis));
    sendto(sock, new_packet.data(), new_size, 0, (struct sockaddr*)&server, sizeof(server));
}




bool VideoStreamer_SW::encodeAndSendFrames(cv::VideoCapture& cap) {
    cv::Mat frame;
    
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    int64_t millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    //std::cout << "Timestamp_before_cap:" << millis << std::endl;

    cap >> frame;  // Capture a new frame

    auto now1 = std::chrono::system_clock::now();
    auto duration1 = now1.time_since_epoch();
    int64_t millis1 = std::chrono::duration_cast<std::chrono::milliseconds>(duration1).count();
    std::cout << "Time_cap_sw:" << millis1 - millis << std::endl;

    encodeStartTimes.push(millis1); // Push it to the queue

    if (frame.empty()) return false;
    

    uint8_t* inData[1] = { frame.data };
    int inLinesize[1] = { static_cast<int>(frame.step) };
    sws_scale(sws_ctx, inData, inLinesize, 0, frame.rows, avFrame->data, avFrame->linesize);
    avFrame->pts = frame_count++;

    if (avcodec_send_frame(codecContext, avFrame) < 0) {
        std::cerr << "Error sending frame for encoding\n";
        return true;
    }

    while (avcodec_receive_packet(codecContext, packet) == 0) {

        auto now2 = std::chrono::system_clock::now();
        auto duration2 = now2.time_since_epoch();
        int64_t millis2 = std::chrono::duration_cast<std::chrono::milliseconds>(duration2).count();

        auto encode_start = encodeStartTimes.front();
        encodeStartTimes.pop()
        std::cout << "Time_encode_sw:" << millis2 - encode_start << std::endl;

        sendPacketWithTimestamp(sock, packet, server, millis2);
        av_packet_unref(packet);  // Reset packet for the next usage
    }
    return true;
}

int VideoStreamer_SW::run() {
    cv::VideoCapture cap(0);
    //double width = 1920, height = 1080;
    double width = 640, height = 480;
    cap.set(cv::CAP_PROP_FRAME_WIDTH, width);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, height);
    if (!cap.isOpened()) {
        std::cerr << "Cannot open the video camera.\n";
        return 1;
    }

    //cv::namedWindow("Live Video", cv::WINDOW_AUTOSIZE);
    while (true) {
        if (!encodeAndSendFrames(cap)) break;
        if (cv::waitKey(5) >= 0) break;
    }
    return 0;
}
