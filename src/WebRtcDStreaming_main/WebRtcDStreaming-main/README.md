# WebRTC Video Streaming with Latency Measurement

## Introduction

This document provides step-by-step instructions on how to set up a WebRTC video streaming system that measures video latency between two peers. The system uses a signaling server to exchange connection details and a data channel to measure the latency.

## Prerequisites

`Node.js` installed on your machine

Basic understanding of `JavaScript` and `WebRTC`

Two devices connected to the same network or accessible via the internet

## Components

Signaling Server: Facilitates the exchange of WebRTC connection details.

HTML and JavaScript Client: Captures and displays video streams, and measures latency.

## Install the required packages:

```
npm install express https socket.io
```


## Start the server:

```
node server.js
```

## start the client:

Using `Chrome` or `Edge` web browser, check the [browser_compatibility](https://developer.mozilla.org/en-US/docs/Web/API/RTCStatsReport#browser_compatibility)

```
open the web input https://[serverIP]:3000
```

# Debug

Round trip time, Packet loss and packet retransmission.

ref: [webpage](https://webrtcforthecurious.com/zh/docs/09-debugging/)

## Latency

1. Mobile phone photography `EndToEndLatency = T(observe) - T(happen)`.
2. End-to-end latency – measured automatically, with API `RTCStatsReport, roundTripTime in 'remote-inbound-rtp' stats`.

### Delay debugging tips

1. Camera delay: `focus_auto=0, focus_absolute=0`.
2. Encode delay: For x264, (reduce sub-frame delay) using `tune=zerolatency` and `profile=baseline` to get the lowest frame output latency.
3. Network delay: ? -> RTT, Drop rate, ReTrans.

### Debug with browser

`chrome://webrtc-internals/` or `edge://webrtc-internals/`
