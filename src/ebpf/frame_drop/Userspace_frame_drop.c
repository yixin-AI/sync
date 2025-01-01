#include <bpf/libbpf.h>
#include <bpf/bpf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <time.h>
#include <pthread.h>

#define INTERVAL_MS 1000


int get_sent_bytes(unsigned long *sent_bytes) {
    FILE *fp;
    char line[1035];


    fp = popen("tc -s qdisc show dev phy1-ap0 | grep 'Sent'", "r");
    if (fp == NULL) {
        fprintf(stderr, "Failed to run command\n");
        return -1;
    }

    int line_count = 0;
    while (fgets(line, sizeof(line), fp) != NULL) {
        line_count++;
        if (line_count == 2) {
            unsigned long sent;


            if (sscanf(line, " Sent %lu bytes", &sent) == 1) {
                if (sent_bytes != NULL) {
                    *sent_bytes = sent;
                }
            }
            break;
        }
    }

    pclose(fp);
    return 0;
}


double estimate_bandwidth(int interval_ms) {
    unsigned long prev_sent_bytes = 0, curr_sent_bytes = 0;
    struct timespec start_time, end_time;
    double elapsed_time, bandwidth;


    if (get_sent_bytes(&prev_sent_bytes) != 0) {
        fprintf(stderr, "Error retrieving initial sent bytes\n");
        return -1;
    }


    clock_gettime(CLOCK_MONOTONIC, &start_time);


    usleep(interval_ms * 1000);


    if (get_sent_bytes(&curr_sent_bytes) != 0) {
        fprintf(stderr, "Error retrieving current sent bytes\n");
        return -1;
    }


    clock_gettime(CLOCK_MONOTONIC, &end_time);


    elapsed_time = (end_time.tv_sec - start_time.tv_sec) +
                   (end_time.tv_nsec - start_time.tv_nsec) / 1000000000.0;


    unsigned long byte_delta = curr_sent_bytes - prev_sent_bytes;


    bandwidth = (byte_delta * 8) / (elapsed_time * 1000000);

    return bandwidth;
}



double estimate_max_bandwidth_over_15_seconds(int interval_ms) {
    double max_bandwidth = 0.0;
    double current_bandwidth;
    int total_duration = 15000;
    int elapsed_time = 0;


    while (elapsed_time < total_duration) {

        current_bandwidth = estimate_bandwidth(interval_ms);
        
        //printf("current_bandwidth: %.3f Mbps\n", current_bandwidth);


        if (current_bandwidth > max_bandwidth) {
            max_bandwidth = current_bandwidth;
        }


        usleep(interval_ms * 1000);


        elapsed_time += interval_ms;
    }

    return max_bandwidth;
}

int get_send_and_dropped(int *sent_pkt, int *dropped_pkt) {
    FILE *fp;
    char line[1035];


    fp = popen("tc -s qdisc show dev phy1-ap0 | grep 'Sent'", "r");
    if (fp == NULL) {
        fprintf(stderr, "Failed to run command\n");
        return -1;
    }

    int line_count = 0;
    while (fgets(line, sizeof(line), fp) != NULL) {
        line_count++;
        if (line_count == 2) {  
            int sent, dropped;

            if (sscanf(line, " Sent %*d bytes %d pkt (dropped %d,", &sent, &dropped) == 2) {
                if (sent_pkt != NULL) {
                    *sent_pkt = sent;
                }
                if (dropped_pkt != NULL) {
                    *dropped_pkt = dropped;
                }
            }
            break;
        }
    }

    pclose(fp);
    return 0;
}

int is_packet_loss_increasing(int *prev_dropped_pkt) {
    int curr_dropped_pkt = 0;
    int packet_dropped_increase = 0;


    if (get_send_and_dropped(NULL, &curr_dropped_pkt) != 0) {
        fprintf(stderr, "Error retrieving current drop count\n");
        return false;  
    }


    if (curr_dropped_pkt > (*prev_dropped_pkt + 4)) {
        packet_dropped_increase = curr_dropped_pkt - *prev_dropped_pkt;
        *prev_dropped_pkt = curr_dropped_pkt;  
        return packet_dropped_increase;  
    } else {
        *prev_dropped_pkt = curr_dropped_pkt;  
        return 0;  
    }
}

void* update_max_bandwidth(void* arg) {

    void **args = (void **)arg;  
    int map_fd = *(int *)args[0];  
    double decrease_factor = *(double *)args[1];  
    double increase_factor = *(double *)args[2];  
    double min_increase_step = 0.01;

    int prev_dropped_pkt = 0;
    double k = 1;
    double k_prev = 1;
    is_packet_loss_increasing(&prev_dropped_pkt);
    __u32 key = 2;
    __u32 value = 1250000;
    bpf_map_update_elem(map_fd, &key, &value, BPF_ANY);
    is_packet_loss_increasing(&prev_dropped_pkt);
    
    double max_bandwidth = estimate_max_bandwidth_over_15_seconds(INTERVAL_MS);
    int packet_dropped_increase = 0;

    if (is_packet_loss_increasing(&prev_dropped_pkt) != 0) {
    
        while (1) {
            packet_dropped_increase = is_packet_loss_increasing(&prev_dropped_pkt);
            if (packet_dropped_increase != 0) {
                //k_prev = k;
                //k = k - decrease_factor * packet_dropped_increase;
                k = k * decrease_factor;
            } else if (k < 1.05) {
                //double increase_step = (k_prev - k) / increase_factor;
                double increase_step = increase_factor;
                if (increase_step < min_increase_step) {
                    increase_step = min_increase_step;
                }
                k = k + increase_step;
            }
            
            if (k < 0.1) {
                k = 0.1;
            }
      
            printf("k: %.2f \n", k);
            __u32 rate_bytes_per_second = (unsigned int)(k * max_bandwidth * 25000);
            
            key = 2;
            value = rate_bytes_per_second;
            if (bpf_map_update_elem(map_fd, &key, &value, BPF_ANY) != 0) {
                perror("Error updating rate limit");
            }

            sleep(2);  
        }
    }
    
    return NULL;
}




int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <decrease_factor> <increase_step>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int map_fd;
    __u32 key, value;


    double decrease_factor = atof(argv[1]);
    double increase_step = atof(argv[2]);
    
    map_fd = bpf_obj_get("/sys/fs/bpf/xdp_stats_map");
    if (map_fd < 0) {
        fprintf(stderr, "Failed to open BPF map: %s\n", strerror(errno));
        return 1;
    }

    key = 1;  
    value = 0;  
    if (bpf_map_update_elem(map_fd, &key, &value, BPF_ANY) != 0) {
        perror("Error updating mark_count");
    }


    void *args[3];
    args[0] = &map_fd;
    args[1] = &decrease_factor;
    args[2] = &increase_step;

    pthread_t bandwidth_thread;
    if (pthread_create(&bandwidth_thread, NULL, update_max_bandwidth, args) != 0) {
        perror("Error creating thread");
        return 1;
    }
    
    key = 0;  
    while (1) {
        value = 0;
        if (bpf_map_update_elem(map_fd, &key, &value, BPF_ANY) != 0) {
            perror("Error updating mark_threshold");
        }

        usleep(200000);  
    }

    close(map_fd);
    return 0;
}
