#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <net/if.h>
#include <linux/if_xdp.h>
#include <bpf/libbpf.h>
#include <bpf/bpf.h>
#include <linux/if_link.h>
#include <sys/resource.h>

#define INTERFACE "eth0"
#define HIGH_PRIORITY_RATE 8
#define LOW_PRIORITY_RATE 2
#define BURST 10

typedef struct {
    int tokens;
    int rate;
    int burst;
    pthread_mutex_t lock;
} TokenBucket;

void* token_generator(void *arg) {
    TokenBucket *bucket = (TokenBucket *)arg;
    while (1) {
        pthread_mutex_lock(&bucket->lock);
        if (bucket->tokens < bucket->burst) {
            bucket->tokens += bucket->rate;
            if (bucket->tokens > bucket->burst) {
                bucket->tokens = bucket->burst;
            }
        }
        pthread_mutex_unlock(&bucket->lock);
        sleep(1);
    }
    return NULL;
}

int main() {
    TokenBucket high_priority_bucket = {0, HIGH_PRIORITY_RATE, BURST, PTHREAD_MUTEX_INITIALIZER};
    TokenBucket low_priority_bucket = {0, LOW_PRIORITY_RATE, BURST, PTHREAD_MUTEX_INITIALIZER};
    pthread_t high_priority_thread, low_priority_thread;

    pthread_create(&high_priority_thread, NULL, token_generator, &high_priority_bucket);
    pthread_create(&low_priority_thread, NULL, token_generator, &low_priority_bucket);

    // 提升资源限制
    struct rlimit rlim = {RLIM_INFINITY, RLIM_INFINITY};
    if (setrlimit(RLIMIT_MEMLOCK, &rlim)) {
        perror("setrlimit");
        return 1;
    }

    // 获取 map FD
    int map_fd = bpf_obj_get("/sys/fs/bpf/drop_control"); // 假设 map 被存储在此路径
    if (map_fd < 0) {
        perror("Failed to get map fd");
        return 1;
    }

    __u32 key = 0, drop_value = 0;

    while (1) {
        pthread_mutex_lock(&high_priority_bucket.lock);
        if (high_priority_bucket.tokens > 0) {
            high_priority_bucket.tokens--;
            drop_value = 0;
        } else {
            drop_value = 1;
        }
        pthread_mutex_unlock(&high_priority_bucket.lock);

        if (bpf_map_update_elem(map_fd, &key, &drop_value, BPF_ANY) < 0) {
            perror("bpf_map_update_elem");
            break;
        }

        printf("Packet drop value set to: %u\n", drop_value);
        usleep(100000);  // 模拟包处理
    }

    return 0;
}
