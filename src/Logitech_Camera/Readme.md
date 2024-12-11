# Camera

## Hardware infomation


[Logitech C930E Business Webcam](https://www.logitech.com/de-at/products/webcams/c930e-business-webcam.960-000972.html) supports 1080p (Full HD) @ 30fps that enables expressions, non-verbal cues and movements to be seen clearly. 

And it is avaliable for H.264 UVC 1.5 with Scalable Video Coding for a smoother video stream in applications.

## 目前状态
`camera_detect.py` -- checking camera status

`Client_H265.cpp` -- As a video capture, encoding, and sending application, the capture is done at a resolution of 1920x1080, using H265 encoding, and sent via UDP. Additionally, a timestamp of the video capture time is appended to the end of the data.

`Server_H265.cpp` -- As an application for video reception, decoding, and playback, it also operates at a resolution of 1920x1080 and uses H265 decoding. Additionally, it processes the timestamp appended at the end of the UDP data to prevent any impact on the decoding.


# TO-Do

> H.265 编码 (可以参考GPT以及网页资料)
常见的编码有: ....(做一下背景调查,查看当前可能的一些编码),目前发送使用JPEG作为发送,其效率会差,以及稳定性(比如FPS)可能没有压缩后发送好

## Measurement

通过`tcpdump`或者`wireshark`捕获数据包得到`.pcap`文件,对该文件进行分析:

![Picture](/assets/img/philly-magic-garden.jpg "Magic Gardens")

1. `Packets per second(pps)` (每秒钟包数量).
2. 数据包大小长度,比如平均`256Bytes`大小.
3. 数据包数量和时间曲线.
4. 发现视频中画面静止 和 视频中画面运动(我们主要研究的) 的不同.
