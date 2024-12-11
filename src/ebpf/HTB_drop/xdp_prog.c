#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/types.h>

struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 2);  // 0 是高优先级，1 是低优先级
    __type(key, __u32);
    __type(value, int);  // 存储令牌数
} token_bucket SEC(".maps");

SEC("prog")
int xdp_control(struct xdp_md *ctx) {
    __u32 high_priority_key = 0, low_priority_key = 1;
    int *high_tokens = bpf_map_lookup_elem(&token_bucket, &high_priority_key);
    int *low_tokens = bpf_map_lookup_elem(&token_bucket, &low_priority_key);
    
    // 假设函数，根据数据包内容决定优先级
    //int priority = check_packet_priority(ctx);  
    int priority = 0;

    if (priority == 0 && high_tokens && *high_tokens > 0) {
        (*high_tokens)--;  // 减少令牌
        bpf_map_update_elem(&token_bucket, &high_priority_key, high_tokens, BPF_ANY);
        return XDP_PASS;
    } else if (priority == 1 && low_tokens && *low_tokens > 0) {
        (*low_tokens)--;  // 减少令牌
        bpf_map_update_elem(&token_bucket, &low_priority_key, low_tokens, BPF_ANY);
        return XDP_PASS;
    }
    
    return XDP_DROP;
}

char _license[] SEC("license") = "GPL";
