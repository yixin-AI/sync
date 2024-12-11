#ifndef VIDEOSTREAMER_H
#define VIDEOSTREAMER_H

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

#include <vector>
#include <chrono>

class VideoStreamer {
public:
    VideoStreamer();
    ~VideoStreamer();
    int run();

private:
    void setupNetwork();
    void setupVideo();
    void cleanup();
    void sendPacketWithTimestamp(SOCKET sock, AVPacket* packet, const sockaddr_in& server, int64_t millis);
    bool encodeAndSendFrames(cv::VideoCapture& cap);

    SOCKET sock;
    struct sockaddr_in server;
    AVCodecContext* codecContext;
    SwsContext* sws_ctx;
    AVFrame* avFrame;
    AVPacket* packet;
    int server_size;
    int frame_count = 0;
};

#endif // VIDEOSTREAMER_H
