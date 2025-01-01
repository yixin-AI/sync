#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/types.h>
#include <linux/in.h>  

#define TARGET_IP 0xC0A81568 
#define TARGET_PORT 54343     
#define RTP_HEADER_SIZE 12   

struct rtp_header {
    unsigned char cc:4;       // CSRC count
    unsigned char x:1;        // Header extension flag
    unsigned char p:1;        // Padding flag
    unsigned char version:2;  // Version
    unsigned char pt:7;       // Payload type
    unsigned char m:1;        // Marker bit
    unsigned short seq;       // Sequence number
    unsigned int ts;          // Timestamp
    unsigned int ssrc;        // Synchronization source
};

struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __type(key, __u32);
    __type(value, __u32);
    __uint(max_entries, 3);
} xdp_stats_map SEC(".maps");




SEC("prog")
int xdp_rtp_filter(struct xdp_md *ctx) {
    void *data_end = (void *)(long)ctx->data_end;
    void *data = (void *)(long)ctx->data;

    // Layer 2
    struct ethhdr *eth = data;
    if ((void *)(eth + 1) > data_end)
        return XDP_PASS;

    // Layer 3
    struct iphdr *ip = data + sizeof(*eth);
    if ((void *)(ip + 1) > data_end)
        return XDP_PASS;
    if (ip->protocol != IPPROTO_UDP)
        return XDP_PASS;
    if (ip->daddr != __builtin_bswap32(TARGET_IP))
        return XDP_PASS;

    // Layer 4
    struct udphdr *udp = (void *)ip + sizeof(*ip);
    if ((void *)(udp + 1) > data_end)
        return XDP_PASS;
    if (udp->dest != __builtin_bswap16(TARGET_PORT))
        return XDP_PASS;

    // RTP Header
    struct rtp_header *rtp = (void *)udp + sizeof(*udp);
    if ((void *)(rtp + 1) > data_end)
        return XDP_PASS;

 
    __u32 key0 = 0;
    __u32 key1 = 1;
    __u32 key2 = 2;
    
    
    __u32 *byte_count = bpf_map_lookup_elem(&xdp_stats_map, &key0);
    if (!byte_count)
        return XDP_PASS;  

    
    __u32 *drop_flag = bpf_map_lookup_elem(&xdp_stats_map, &key1);
    if (!drop_flag)
        return XDP_PASS;
        
        
    __u32 *rate_limit = bpf_map_lookup_elem(&xdp_stats_map, &key2);
    if (!rate_limit)
        return XDP_PASS;

    
    __u32 pkt_size = (__u32)(ctx->data_end - ctx->data);
    
    

    
    if (*drop_flag == 1) {
        if (*byte_count + pkt_size > *rate_limit || rtp->m == 0) {
            return XDP_DROP;  
        } else {
            *drop_flag = 0;
            bpf_map_update_elem(&xdp_stats_map, &key1, drop_flag, BPF_ANY);
            return XDP_DROP;            
        }
    }


    if (*byte_count + pkt_size > *rate_limit) {
       
        if (rtp->m == 1) {  
            *drop_flag = 1;  
            bpf_map_update_elem(&xdp_stats_map, &key1, drop_flag, BPF_ANY);
        }
    }


    __sync_fetch_and_add(byte_count, pkt_size);
    
    bpf_map_update_elem(&xdp_stats_map, &key0, byte_count, BPF_ANY);

    return XDP_PASS;  
    
    
    
}

char _license[] SEC("license") = "GPL";


