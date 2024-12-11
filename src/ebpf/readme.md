# Run on Raspeberry PI
1. Compile XDP with  `clang -O2 -g -target bpf -c xdp_prog.c -o xdp_prog.o`
2. Complie AF_XDP with  `gcc af_xdp_bandwidth_limit.c -o af_xdp_bandwidth_limit -lbpf`
3. Mount bpf map `sudo mount -t bpf none /sys/fs/bpf` `sudo bpftool map show` `sudo bpftool map pin id 1 /sys/fs/bpf/xdp_stats_map` 
4. check the folder `/sys/fs/bpf/` to find the map `drop_control`
5. Append the XDP on ip command `ip link set dev phy1-ap0 xdp obj XDP_frame_drop.o`
6. Run `sudo ./af_xdp_bandwidth_limit`