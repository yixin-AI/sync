#pragma once
#ifndef VIDEOSTREAMER_H
#define VIDEOSTREAMER_H

#include <winsock2.h>
#include <Ws2tcpip.h>
#include <opencv2/opencv.hpp>

// FFmpeg
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libavutil/dict.h>
#include <libavutil/imgutils.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_cuda.h>
#include <libavutil/buffer.h>
}

#pragma comment(lib, "ws2_32.lib")

#include <chrono>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>

int64_t getCurrentTimeMillis();

class VideoStreamer {
public:
    VideoStreamer();
    ~VideoStreamer();
    int run();

private:
    void setupNetwork();
    void setupVideo();
    int initCuda();

    void addTimestampToFrame(cv::Mat& frame, int64_t millis);
    void sendPacketWithTimestamp(SOCKET sock, AVPacket* packet, const sockaddr_in& server, int64_t millis);

    void captureFrames(); // thread 0
    bool encodeFrames(); // thread 1
    bool sendPackets(); // thread 2

    std::queue<std::pair<cv::Mat, int64_t>> frameQueue;
    std::queue<AVPacket*> packetQueue;
    std::mutex frameQueueMutex;
    std::mutex packetQueueMutex;
    std::condition_variable frameQueueCondVar;
    std::condition_variable packetQueueCondVar;
    std::atomic<bool> running{ true };

    std::thread captureThread;
    std::thread encodeThread;
    std::thread sendThread;

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