#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <bpf/bpf.h>
#include <fcntl.h>
#include <errno.h>

#define INTERFACE "eth0"
#define BANDWIDTH_LIMIT_MB 1000 // 10 KB/s

#define DROP_CONTROL_MAP_PATH "/sys/fs/bpf/drop_control"

// Function to get current bandwidth usage using ifstat
double get_bandwidth_usage(const char *iface) {
    FILE *fp;
    char cmd[256];
    char buffer[1024];
    double rx = 0.0, tx = 0.0;

    // Construct command to call ifstat
    snprintf(cmd, sizeof(cmd), "ifstat -i %s 1 1 | awk 'NR==3 {print $1, $2}'", iface);
    fp = popen(cmd, "r");
    if (!fp) {
        perror("popen");
        return -1;
    }

    // Read output
    if (fgets(buffer, sizeof(buffer), fp) != NULL) {
        sscanf(buffer, "%lf %lf", &rx, &tx);
    }

    pclose(fp);

    // Return total bandwidth usage
    return rx + tx;
}

// Function to update the drop control map
int update_drop_control(int map_fd, __u32 value) {
    __u32 key = 0;
    if (bpf_map_update_elem(map_fd, &key, &value, BPF_ANY) != 0) {
        perror("Failed to update map");
        return -1;
    }
    return 0;
}

int main() {
    int map_fd;
    double bandwidth_usage;

    // Open BPF map
    map_fd = bpf_obj_get(DROP_CONTROL_MAP_PATH);
    if (map_fd < 0) {
        perror("Failed to open BPF map");
        return EXIT_FAILURE;
    }

    printf("Monitoring bandwidth on interface %s...\n", INTERFACE);

    while (1) {
        // Get current bandwidth usage
        bandwidth_usage = get_bandwidth_usage(INTERFACE);

        printf("Current Bandwidth Usage: %.2f KB/s\n", bandwidth_usage);

        if (bandwidth_usage > BANDWIDTH_LIMIT_MB) {
            // Update map to drop packets
            printf("Bandwidth limit exceeded, dropping packets...\n");
            if (update_drop_control(map_fd, 1) != 0) {
                close(map_fd);
                return EXIT_FAILURE;
            }
        } else {
            // Update map to pass packets
            if (update_drop_control(map_fd, 0) != 0) {
                close(map_fd);
                return EXIT_FAILURE;
            }
        }

        sleep(1); // Check every second
    }

    close(map_fd);
    return EXIT_SUCCESS;
}
