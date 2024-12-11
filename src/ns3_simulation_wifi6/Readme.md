# Simulation with NS3

simulator from [github](https://github.com/nsnam/ns-3-dev-git) main branch

```
mm@mm-VirtualBox:~/ns-3-dev-git$ ./ns3 run wifi6-test-real
[  0%] Building CXX object scratch/CMakeFiles/scratch_wifi6-test-real.dir/wifi6-test-real.cc.o
[  0%] Linking CXX executable /home/mm/ns-3-dev-git/build/scratch/ns3-dev-wifi6-test-real-default
====WIFI HE 6====================================================================
Frequency: 5 GHz
Distance: (future disable)1 meters
Simulation time: +1e+10ns
UDP: (disable)true
Downlink: (disable) true
Use RTS/CTS: true
Use Extended Block Ack: false
Number of non-AP HE stations: 4
DL Ack Sequence Type: NO-OFDMA
Enable UL OFDMA: false
Enable BSRP: false
Access Req Interval: +0ns
MCS: 4
PHY Model: Yans
Guard Interval: 1600 ns
====WIFI HE 6====================================================================

total Packets send video: 7142
total Packets received video: 7119
total Packets send haptic: 9555
total Packets received haptic: 9551
total Packets send control: 6999
total Packets received control: 5992
Pakcet loss video in %: 0.322039%
Pakcet loss haptic in %: 0.0418629%
Pakcet loss control in %: 14.3878%
Program Running in cpu time: 37754ms
Simulate Running in cpu time: 37708ms
```