#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <bpf/libbpf.h>
#include <bpf/bpf.h>
#include <sys/resource.h>

#define HIGH_PRIORITY_RATE 8
#define LOW_PRIORITY_RATE 2
#define BURST 10

typedef struct {
    int tokens;
    int rate;
    int burst;
    pthread_mutex_t lock;
    int map_fd;  // Map file descriptor for BPF map updates
    __u32 key;   // Key in the BPF map
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
        // Update BPF map with new token count
        if (bpf_map_update_elem(bucket->map_fd, &(bucket->key), &(bucket->tokens), BPF_ANY) != 0) {
            fprintf(stderr, "Failed to update BPF map: %s\n", strerror(errno));
        }
        pthread_mutex_unlock(&bucket->lock);
        sleep(1);  // Adjust sleep to control rate of token refill
    }
    return NULL;
}

int main() {
    // 获取 map 文件描述符
    int map_fd = bpf_obj_get("/sys/fs/bpf/drop_control");
    if (map_fd < 0) {
        perror("Failed to get map fd");
        return 1;
    }

    TokenBucket high_priority_bucket = {0, HIGH_PRIORITY_RATE, BURST, PTHREAD_MUTEX_INITIALIZER, map_fd, 0};
    TokenBucket low_priority_bucket = {0, LOW_PRIORITY_RATE, BURST, PTHREAD_MUTEX_INITIALIZER, map_fd, 1};
    pthread_t high_priority_thread, low_priority_thread;

    pthread_create(&high_priority_thread, NULL, token_generator, &high_priority_bucket);
    pthread_create(&low_priority_thread, NULL, token_generator, &low_priority_bucket);

    pthread_join(high_priority_thread, NULL);
    pthread_join(low_priority_thread, NULL);

    return 0;
}
