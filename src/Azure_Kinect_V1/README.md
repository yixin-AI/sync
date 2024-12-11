# Kinect for Xbox 360 (2010)

[Kinect for Xbox 360](https://en.wikipedia.org/wiki/Kinect) is a discontinued line of motion sensing input devices produced by Microsoft and first released in 2010. The devices generally contain RGB cameras, and infrared projectors and detectors that map depth through either structured light or time of flight calculations, which can in turn be used to perform real-time gesture recognition and body skeletal detection, among other capabilities. They also contain microphones that can be used for speech recognition and voice control.

Here, two project were introduced to capture video using the Kinect

## DepthWithColor-D3D

This project is offered by the Microsoft SDK Toolkit, depending on the DirectX SDK. However, the video of RGB and Depth data are mixed and hard to recognize.

## RGBDCapture


This project is based on [this link](https://github.com/chaowang15/RGBDCapture). The difference is that I did not use the Boost Library to manage the filesystem. Instead, I used std::filesystem (C++17). And this code is based on OpenCV and [Openni](https://structure.io/openni/).

## Todo

The RGB image is 8UC3 (8-bit, 3-channel) and the depth image is 16UC1. What can be done is to convert the 16UC1 depth data to 8UC1 (which may lose some accuracy), and combine the RGB and depth data into 8UC4, then use H.264 to encode the stream.