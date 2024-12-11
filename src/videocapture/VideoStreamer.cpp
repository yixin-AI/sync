#include "VideoStreamer.h"
#include <iostream>
#include <cuda_runtime.h>

#define CAMERA 0

std::queue<int64_t> encodeStartTime;

int64_t getCurrentTimeMillis() {
    auto v_timestamp = std::chrono::system_clock::now();
    auto v_epoch = v_timestamp.time_since_epoch();
    int64_t v_millis = std::chrono::duration_cast<std::chrono::milliseconds>(v_epoch).count();
    return v_millis;
}

VideoStreamer::VideoStreamer() : codecContext(nullptr), sws_ctx(nullptr), avFrame(nullptr), packet(nullptr) {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    setupNetwork();
    setupVideo();
}

VideoStreamer::~VideoStreamer() {
    // stop the thread
    running = false;
    frameQueueCondVar.notify_all();
    packetQueueCondVar.notify_all();

    if (captureThread.joinable()) captureThread.join();
    if (encodeThread.joinable()) encodeThread.join();
    if (sendThread.joinable()) sendThread.join();

    if (avFrame) {
        av_freep(&avFrame->data[0]);
        av_frame_free(&avFrame);
    }
    if (packet) {
        av_packet_free(&packet);
    }
    if (codecContext) {
        avcodec_free_context(&codecContext);
    }
    if (sws_ctx) {
        sws_freeContext(sws_ctx);
    }
    if (sock != INVALID_SOCKET) {
        closesocket(sock);
    }
    cv::destroyAllWindows();
#ifdef _WIN32
    WSACleanup();
#endif
}

void VideoStreamer::setupNetwork() {
    server.sin_family = AF_INET;
    server.sin_port = htons(8888);
    inet_pton(AF_INET, "127.0.0.1", &server.sin_addr);
    server_size = sizeof(server);
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == SOCKET_ERROR) {
        std::cerr << "Socket creation failed.\n";
        exit(1);
    }
}

void VideoStreamer::setupVideo() {

    // Find the encoder
    const AVCodec* codec = avcodec_find_encoder_by_name("h264_nvenc");
    if (!codec) {
        std::cerr << "Codec not found\n";
        exit(1);
    }

    // Allocate the codec context
    codecContext = avcodec_alloc_context3(codec);
    if (!codecContext) {
        std::cerr << "Could not allocate video codec context\n";
        exit(1);
    }

    // Set codec parameters
    codecContext->bit_rate = 4000000;
    codecContext->width = 640;
    codecContext->height = 480;
    codecContext->time_base = { 1, 30 };
    codecContext->framerate = { 30, 1 };
    codecContext->gop_size = 10;
    codecContext->max_b_frames = 1;
    codecContext->pix_fmt = AV_PIX_FMT_CUDA;

    // Initialize CUDA
    if (initCuda() < 0) {
        std::cerr << "Could not initialize CUDA\n";
        exit(1);
    }

    // Allocate and initialize the hardware frames context
    AVBufferRef* hw_device_ctx = nullptr;
    if (av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_CUDA, nullptr, nullptr, 0) < 0) {
        std::cerr << "Failed to create a CUDA device context\n";
        exit(1);
    }
    
    codecContext->hw_device_ctx = av_buffer_ref(hw_device_ctx);

    AVBufferRef* hw_frames_ref = av_hwframe_ctx_alloc(codecContext->hw_device_ctx);
    if (!hw_frames_ref) {
        std::cerr << "Could not allocate hardware frame context\n";
        exit(1);
    }

    AVHWFramesContext* frames_ctx = (AVHWFramesContext*)(hw_frames_ref->data);
    frames_ctx->format = AV_PIX_FMT_CUDA;
    frames_ctx->sw_format = AV_PIX_FMT_YUV420P;
    frames_ctx->width = codecContext->width;
    frames_ctx->height = codecContext->height;
    frames_ctx->initial_pool_size = 20;

    int err = av_hwframe_ctx_init(hw_frames_ref);
    if (err < 0) {
        std::cerr << "Failed to initialize hardware frame context\n";
        av_buffer_unref(&hw_frames_ref);
        exit(1);
    }

    codecContext->hw_frames_ctx = av_buffer_ref(hw_frames_ref);
    if (!codecContext->hw_frames_ctx) {
        std::cerr << "Failed to set hardware frame context\n";
        av_buffer_unref(&hw_frames_ref);
        exit(1);
    }
    av_buffer_unref(&hw_frames_ref);

    // Set encoder options
    AVDictionary* opts = nullptr;
    av_dict_set(&opts, "fast", "fast", 0);

    // Open the codec
    if (avcodec_open2(codecContext, codec, &opts) < 0) {
        std::cerr << "Could not open codec\n";
        av_dict_free(&opts);
        exit(1);
    }

    // Release dictionary
    av_dict_free(&opts);

    avFrame = av_frame_alloc();
    if (!avFrame) {
        std::cerr << "Could not allocate video frame\n";
        exit(1);
    }

    avFrame->format = AV_PIX_FMT_YUV420P;
    avFrame->width = codecContext->width;
    avFrame->height = codecContext->height;

    if (av_image_alloc(avFrame->data, avFrame->linesize, avFrame->width, avFrame->height, AV_PIX_FMT_YUV420P, 32) < 0) {
        std::cerr << "Could not allocate raw picture buffer\n";
        exit(1);
    }

    packet = av_packet_alloc();
    if (!packet) {
        std::cerr << "Could not allocate packet\n";
        exit(1);
    }
}

void VideoStreamer::addTimestampToFrame(cv::Mat& frame, int64_t millis) {
    std::stringstream ss;
    ss << "Timestamp: " << millis << " ms";
    cv::Point textOrg(10, 30);
    double fontScale = 1;
    cv::Scalar textColor(0, 0, 255);
    int fontFace = cv::FONT_HERSHEY_SIMPLEX;
    int thickness = 1;
    cv::putText(frame, ss.str(), textOrg, fontFace, fontScale, textColor, thickness);
}


int VideoStreamer::initCuda() {
    int err = av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_CUDA, nullptr, nullptr, 0);
    if (err < 0) {
        std::cerr << "Error: Could not create CUDA device context\n";
        return err;
    }
    codecContext->hw_device_ctx = av_buffer_ref(hw_device_ctx);
    if (!codecContext->hw_device_ctx) {
        std::cerr << "Error: Could not reference CUDA device context\n";
        return AVERROR(ENOMEM);
    }
    return 0;
}


void VideoStreamer::sendPacketWithTimestamp(SOCKET sock, AVPacket* packet, const sockaddr_in& server, int64_t millis) {
    size_t new_size = packet->size + sizeof(millis);
    std::vector<char> new_packet(packet->size + sizeof(millis));
    memcpy(new_packet.data(), packet->data, packet->size);
    memcpy(new_packet.data() + packet->size, &millis, sizeof(millis));
    sendto(sock, new_packet.data(), new_size, 0, (struct sockaddr*)&server, sizeof(server));
}

bool VideoStreamer::encodeFrames() {
    // Pre-allocate CUDA frames and other necessary resources
    
    /* 这里决定不使用opencv而使用软件帧。所有均使用FFmpeg的CUDA帧和缓冲区，首先将BGR格式的OpenCV
    Mat转换为YUV420P格式的AVFrame，然后将YUV420P的软件帧转换为CUDA格式的硬件帧。将处理后的CUDA帧
    发送到编码器进行编码，并处理生成的AVPacket*/
    // cv::cuda::GpuMat gpuFrame;

    AVFrame* hw_frame = av_frame_alloc();
    if (!hw_frame) {
        std::cerr << "Could not allocate CUDA frame\n";
        return false;
    }
    hw_frame->format = AV_PIX_FMT_CUDA;
    hw_frame->width = codecContext->width;
    hw_frame->height = codecContext->height;

    if (av_hwframe_get_buffer(codecContext->hw_frames_ctx, hw_frame, 0) < 0) {
        std::cerr << "Could not allocate CUDA frame buffer\n";
        av_frame_free(&hw_frame);
        return false;
    }
    
    AVFrame* sw_frame = av_frame_alloc();
    if (!sw_frame) {
        std::cerr << "Could not allocate software frame\n";
        av_frame_free(&hw_frame);
        return false;
    }
    sw_frame->format = AV_PIX_FMT_YUV420P;
    sw_frame->width = codecContext->width;
    sw_frame->height = codecContext->height;

    if (av_image_alloc(sw_frame->data, sw_frame->linesize, sw_frame->width, sw_frame->height, AV_PIX_FMT_YUV420P, 32) < 0) {
        std::cerr << "Could not allocate raw picture buffer\n";
        av_frame_free(&sw_frame);
        av_frame_free(&hw_frame);
        return false;
    }

    while (running) {
        std::unique_lock<std::mutex> frame_lock(frameQueueMutex);
        frameQueueCondVar.wait(frame_lock, [this] { return !frameQueue.empty() || !running; });

        if (!running) break;

        std::pair<cv::Mat, int64_t> frameWithTimestamp = frameQueue.front();
        frameQueue.pop();
        frame_lock.unlock();

        cv::Mat frame = frameWithTimestamp.first;
        int64_t timestamp = frameWithTimestamp.second;
        addTimestampToFrame(frame, timestamp);

        cv::imshow("Live Stream", frame); // show the frame locally
        if (cv::waitKey(1) >= 0) {
            running = false;
            break;
        }

        // Convert cv::Mat to AVFrame (software frame)
        sw_frame->format = AV_PIX_FMT_BGR24;
        sw_frame->width = frame.cols;
        sw_frame->height = frame.rows;

        // Ensure the AVFrame buffer is allocated
        if (av_frame_get_buffer(sw_frame, 32) < 0) {
            std::cerr << "Could not allocate buffer for software frame\n";
            av_frame_free(&hw_frame);
            av_frame_free(&sw_frame);
            return false;
        }

        // Copy cv::Mat data to AVFrame
        av_image_fill_arrays(sw_frame->data, sw_frame->linesize, frame.data, AV_PIX_FMT_BGR24, frame.cols, frame.rows, 1);

        // Transfer the software frame to CUDA frame
        if (av_hwframe_transfer_data(hw_frame, sw_frame, 0) < 0) {
            std::cerr << "Error transferring data to CUDA frame\n";
            av_frame_free(&hw_frame);
            av_frame_free(&sw_frame);
            return false;
        }

        hw_frame->pts = frame_count++;

        if (avcodec_send_frame(codecContext, hw_frame) < 0) {
            std::cerr << "Error sending frame for encoding\n";
            av_frame_free(&hw_frame);
            return true;
        }

        while (avcodec_receive_packet(codecContext, packet) == 0) {
            AVPacket* pkt_ref = av_packet_alloc();
            if (!pkt_ref) {
                std::cerr << "Could not allocate AVPacket\n";
                av_packet_unref(packet);
                break;
            }
        
            // Copy cv::Mat data to AVFrame
            av_image_fill_arrays(sw_frame->data, sw_frame->linesize, frame.data, AV_PIX_FMT_BGR24, frame.cols, frame.rows, 1);

            // Transfer the software frame to CUDA frame
            if (av_hwframe_transfer_data(hw_frame, sw_frame, 0) < 0) {
                std::cerr << "Error transferring data to CUDA frame\n";
                av_frame_free(&hw_frame);
                av_frame_free(&sw_frame);
                return false;
            }
            
            if (av_packet_ref(pkt_ref, packet) < 0) {
                std::cerr << "Could not reference AVPacket\n";
                av_packet_free(&pkt_ref);
                av_packet_unref(packet);
                break;
            }

            {
                std::unique_lock<std::mutex> pkt_lock(packetQueueMutex);
                packetQueue.push(pkt_ref);
            }

            packetQueueCondVar.notify_one();
            av_packet_unref(packet);  // Reset packet for the next usage
        }
    }

    av_frame_free(&hw_frame);
    av_frame_free(&sw_frame);
    return true;
}

bool VideoStreamer::sendPackets() {
    while (running) {
        AVPacket* pkt = nullptr;

        {
            std::unique_lock<std::mutex> pkt_lock(packetQueueMutex);
            packetQueueCondVar.wait(pkt_lock, [this] { return !packetQueue.empty() || !running; });

            if (!running) break;

            pkt = packetQueue.front();
            packetQueue.pop();
        }

        packetQueueCondVar.notify_one();

        if (pkt) {
            int64_t timestamp = getCurrentTimeMillis();
            sendPacketWithTimestamp(sock, pkt, server, timestamp);
            av_packet_free(&pkt);
        }
    }
    return true;
}

/* Capture the frame from camera and send to the frame_queue */
void VideoStreamer::captureFrames() {
    cv::VideoCapture cap;
    cap.open(CAMERA);
    double width = 640, height = 480;
    cap.set(cv::CAP_PROP_FRAME_WIDTH, width);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, height);

    if (!cap.isOpened()) {
        std::cerr << "Cannot open the video camera.\n";
    }

    while (running) {
        cv::Mat frame;
        cap >> frame;  // Capture a new frame
        if (frame.empty()) continue;

        int64_t timestamp = getCurrentTimeMillis();

        std::unique_lock<std::mutex> frame_lock(frameQueueMutex);
        frameQueue.push(std::make_pair(frame, timestamp));
        frame_lock.unlock();
        frameQueueCondVar.notify_one();
    }
}

int VideoStreamer::run() {
    avformat_network_init();

    captureThread = std::thread(&VideoStreamer::captureFrames, this);
    encodeThread = std::thread(&VideoStreamer::encodeFrames, this);
    sendThread = std::thread(&VideoStreamer::sendPackets, this);

    captureThread.join();
    encodeThread.join();
    sendThread.join();

    // Cleanup network functionality
    avformat_network_deinit();
    return 0;
}