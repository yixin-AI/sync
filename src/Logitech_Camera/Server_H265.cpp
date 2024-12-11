#include <iostream>
#include <winsock2.h>
#include <Ws2tcpip.h>
#include <opencv2/opencv.hpp>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/opt.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

#pragma comment(lib, "ws2_32.lib")

int main() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    struct sockaddr_in si_me, si_other;

    int s, slen = sizeof(si_other);
    char buf[65536]; // buffer size to receive encoded data

    // Create a UDP socket
    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == SOCKET_ERROR) {
        std::cerr << "Socket could not be created. Code: " << WSAGetLastError() << "\n";
        return 1;
    }

    // Zero out the structure
    memset((char*)&si_me, 0, sizeof(si_me));

    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(8888);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    //inet_pton(AF_INET, "192.168.0.2", &si_me.sin_addr);

    

    // Bind socket to port
    if (bind(s, (struct sockaddr*)&si_me, sizeof(si_me)) == SOCKET_ERROR) {
        std::cerr << "Bind failed with error code : " << WSAGetLastError() << "\n";
        closesocket(s);
        WSACleanup();
        return 1;
    }

    // Initialize FFmpeg decoder
    const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_HEVC);
    AVCodecContext* codecContext = avcodec_alloc_context3(codec);
    if (!codecContext) {
        std::cerr << "Could not allocate video codec context\n";
        exit(1);
    }
    if (avcodec_open2(codecContext, codec, nullptr) < 0) {
        std::cerr << "Could not open codec\n";
        exit(1);
    }

    AVFrame* frame = av_frame_alloc();
    AVPacket* packet = av_packet_alloc();
    if (!packet) {
        std::cerr << "Failed to allocate packet.\n";
        exit(1);
    }
    cv::Mat image;
    image.create(1080, 1920, CV_8UC3);
    // prepare sws context
    SwsContext* image_convert_ctx = sws_getContext(
        1920, 1080, AV_PIX_FMT_YUV420P,
        1920, 1080, AV_PIX_FMT_BGR24,
        SWS_BILINEAR, NULL, NULL, NULL);


    while (true) {
        fflush(stdout);

        // Try to receive some data, this is a blocking call
        int len = recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr*)&si_other, &slen);
        if (len == SOCKET_ERROR) {
            std::cerr << "recvfrom() failed with error code : " << WSAGetLastError() << "\n";
            break;
        }

        //remove timestamp
        int64_t receivedTimestamp;
        memcpy(&receivedTimestamp, buf + len - sizeof(int64_t), sizeof(int64_t));

        
        packet->size = len - sizeof(int64_t); // set the actual size of received data
        packet->data = (uint8_t*)buf; // packet data will point to the buffer

        // Send packet to the decoder
        if (avcodec_send_packet(codecContext, packet) < 0) {
            std::cerr << "Error sending a packet for decoding\n";
            continue;
        }

        // Receive frame from decoder
        if (avcodec_receive_frame(codecContext, frame) == 0) {

            
            
            
            uint8_t* dest[4] = { image.data };
            int dest_linesize[4] = { static_cast<int>(image.step[0]) };
            sws_scale(image_convert_ctx, frame->data, frame->linesize, 0, codecContext->height, dest, dest_linesize);
            cv::imshow("Frame", image);
            if (cv::waitKey(5) >= 0) break;
        }
        
        av_packet_unref(packet); // Reset packet for the next usage
    }
    sws_freeContext(image_convert_ctx);
    av_packet_free(&packet);
    closesocket(s);
    WSACleanup();
    av_frame_free(&frame);
    avcodec_free_context(&codecContext);

    return 0;
}
