#ifndef VIDEOSTREAMER_SW_H
#define VIDEOSTREAMER_SW_H

#include <winsock2.h>
#include <Ws2tcpip.h>
#include <opencv2/opencv.hpp>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_cuda.h>
}

#pragma comment(lib, "ws2_32.lib")

#include <vector>
#include <chrono>

class VideoStreamer_SW {
public:
    VideoStreamer_SW();
    ~VideoStreamer_SW();
    int run();
    void streamFrame(const cv::Mat& frame, const cv::Mat& depth);


private:
    void setupNetwork();
    void setupVideo();
    void cleanup();
    int initCuda();
    void sendPacketWithTimestamp(SOCKET sock, AVPacket* packet, const sockaddr_in& server, int64_t millis);
    bool encodeAndSendFrames(cv::VideoCapture& cap);

    AVCodecContext* codecContext;
    SwsContext* sws_ctx;
    AVFrame* avFrame;
    AVPacket* packet;
    SOCKET sock;
    struct sockaddr_in server;
    int server_size;
    int frame_count = 0;
    AVBufferRef* hw_device_ctx;
};

#endif // VIDEOSTREAMER_H
