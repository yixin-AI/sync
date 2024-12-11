#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/types.h>

// 定义一个全局 map，用于存储控制信号
struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 1);
    __type(key, __u32);
    __type(value, __u32);
} drop_control SEC(".maps");

SEC("prog")  // 定义 XDP 程序部分
int xdp_drop_packet(struct xdp_md *ctx) {
    __u32 key = 0;
    __u32 *value;

    // 从 map 中读取控制信号
    value = bpf_map_lookup_elem(&drop_control, &key);
    if (value && *value == 1) {
        // 丢弃数据包
        return XDP_DROP;
    }

    // 允许数据包通过
    return XDP_PASS;
}

char _license[] SEC("license") = "GPL";  // 指定许可证部分
