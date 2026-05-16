/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "infiniband.h"
#include "utils.h"

int get_infiniband_metrics(struct infiniband_metrics *input_infiniband_metrics, int show_ethernet_flag) {
    int count = 0;
    int ret_snprintf;
    size_t path_size;
    char char_value[BUFSIZ];
    long int long_int_value;
    int ret_read_file;

    DIR *sysfs_dir_handle;
    sysfs_dir_handle = NULL;
    struct dirent *sysfs_entry;

    DIR *device_dir_handle;
    device_dir_handle = NULL;
    struct dirent *device_entry;

    /* return error if /sys/class/infiniband does not exist or is failed to open */
    sysfs_dir_handle = opendir("/sys/class/infiniband");
    if (sysfs_dir_handle == NULL) {
        fprintf(stderr, "ERROR: unable to open /sys/class/infiniband: %s\n", strerror(errno));
        return -1;
    }

    /* sysfs_entry->d_name is interface name */
    while ((sysfs_entry = readdir(sysfs_dir_handle)) != NULL) {
        /* stop processing if number of interfaces is greater than INTERFACE_COUNT */
        if (count >= INTERFACE_COUNT) {
            break;
        }

        if (strcmp(sysfs_entry->d_name, ".") == 0 || strcmp(sysfs_entry->d_name, "..") == 0) {
            continue;
        }

        /* construct device path */
        path_size = strlen("/sys/class/infiniband/") + strlen(sysfs_entry->d_name) + strlen("/ports") + 1;
        char sysfs_device_path[path_size];

        ret_snprintf = snprintf(sysfs_device_path, path_size, "/sys/class/infiniband/%s/ports", sysfs_entry->d_name);
        if (ret_snprintf < 0) {
            continue;
        }

        /* open sysfs device path to read port number */
        device_dir_handle = opendir(sysfs_device_path);
        if (device_dir_handle == NULL) {
            continue;
        }

        /* device_entry->d_name is port number */
        while ((device_entry = readdir(device_dir_handle)) != NULL) {
            /* stop processing if number of interfaces is greater than INTERFACE_COUNT */
            if (count >= INTERFACE_COUNT) {
                break;
            }

            if (strcmp(device_entry->d_name, ".") == 0 || strcmp(device_entry->d_name, "..") == 0) {
                continue;
            }

            /* construct device + port path */
            path_size = strlen(sysfs_device_path) + strlen("/") + strlen(device_entry->d_name) + 1;
            char sysfs_device_port_path[path_size];

            ret_snprintf = snprintf(sysfs_device_port_path, path_size, "%s/%s", sysfs_device_path, device_entry->d_name);
            if (ret_snprintf < 0) {
                continue;
            }

            /* read interface link layer in each port */
            path_size = strlen(sysfs_device_port_path) + strlen("/link_layer") + 1;
            char link_layer_file_path[path_size];

            ret_snprintf = snprintf(link_layer_file_path, path_size, "%s/link_layer", sysfs_device_port_path);
            ret_read_file = read_file_char(link_layer_file_path, char_value);
            if (ret_snprintf < 0 || ret_read_file < 0) {
                continue;
            }

            if (show_ethernet_flag <= 0) {
                /* if link layer is not InfiniBand, quit loop */
                if (strcmp(char_value, "InfiniBand") != 0) {
                    continue;
                }
            }

            /* if "counters" directory does not exist(e.g.: soft RoCE device), quit loop */
            size_t sysfs_device_port_path_size = strlen(sysfs_device_port_path);
            path_size = sysfs_device_port_path_size + strlen("/counters") + 1;
            char sysfs_device_port_counters[path_size];

            ret_snprintf = snprintf(sysfs_device_port_counters, path_size, "%s/counters", sysfs_device_port_path);
            if (ret_snprintf < 0) {
                continue;
            }

            struct stat sb;
            if (stat(sysfs_device_port_counters, &sb) != 0 || !S_ISDIR(sb.st_mode)) {
                continue;
            }

            /* construct each interface information on each port */
            size_t interface_name_length = strlen(sysfs_entry->d_name) + strlen(device_entry->d_name) + strlen(":") + 1;
            char interface_name[interface_name_length];
            ret_snprintf = snprintf(interface_name, interface_name_length, "%s:%s", sysfs_entry->d_name, device_entry->d_name);
            if (ret_snprintf < 0) {
                continue;
            }

            strcpy(input_infiniband_metrics->infiniband[count].interface_name, interface_name); /* interface_name:port_name */
            strcpy(input_infiniband_metrics->infiniband[count].link_layer, char_value); /* link_layer */

            /* state */
            path_size = sysfs_device_port_path_size + strlen("/state") + 1;
            char state_file_path[path_size];

            ret_snprintf = snprintf(state_file_path, path_size, "%s/state", sysfs_device_port_path);
            ret_read_file = read_file_char(state_file_path, char_value);
            if (ret_snprintf < 0 || ret_read_file < 0) {
                continue;
            }

            strcpy(input_infiniband_metrics->infiniband[count].state, char_value);

            /* phys_state */
            path_size = sysfs_device_port_path_size + strlen("/phys_state") + 1;
            char phys_state_file_path[path_size];

            ret_snprintf = snprintf(phys_state_file_path, path_size, "%s/phys_state", sysfs_device_port_path);
            ret_read_file = read_file_char(phys_state_file_path, char_value);
            if (ret_snprintf < 0 || ret_read_file < 0) {
                continue;
            }

            strcpy(input_infiniband_metrics->infiniband[count].phys_state, char_value);

            /* rate */
            path_size = sysfs_device_port_path_size + strlen("/rate") + 1;
            char rate_file_path[path_size];

            ret_snprintf = snprintf(rate_file_path, path_size, "%s/rate", sysfs_device_port_path);
            ret_read_file = read_file_char(rate_file_path, char_value);
            if (ret_snprintf < 0 || ret_read_file < 0) {
                continue;
            }

            strcpy(input_infiniband_metrics->infiniband[count].rate, char_value);

            /* lid */
            path_size = sysfs_device_port_path_size + strlen("/lid") + 1;
            char lid_file_path[path_size];

            ret_snprintf = snprintf(lid_file_path, path_size, "%s/lid", sysfs_device_port_path);
            ret_read_file = read_file_long_int(lid_file_path, &long_int_value);
            if (ret_snprintf < 0 || ret_read_file < 0) {
                continue;
            }

            input_infiniband_metrics->infiniband[count].lid = long_int_value;

            /* construct counter metrics */
            char counter_metric_file_path[PATH_MAX];

            ret_snprintf = snprintf(counter_metric_file_path, PATH_MAX, "%s/symbol_error", sysfs_device_port_counters);
            ret_read_file = read_file_long_int(counter_metric_file_path, &long_int_value);
            if (ret_snprintf < 0 || ret_read_file < 0) {
                input_infiniband_metrics->infiniband[count].symbol_error = 0;
            } else {
                input_infiniband_metrics->infiniband[count].symbol_error = long_int_value;
            }

            ret_snprintf = snprintf(counter_metric_file_path, PATH_MAX, "%s/port_rcv_errors", sysfs_device_port_counters);
            ret_read_file = read_file_long_int(counter_metric_file_path, &long_int_value);
            if (ret_snprintf < 0 || ret_read_file < 0) {
                input_infiniband_metrics->infiniband[count].port_rcv_errors = 0;
            } else {
                input_infiniband_metrics->infiniband[count].port_rcv_errors = long_int_value;
            }

            ret_snprintf = snprintf(counter_metric_file_path, PATH_MAX, "%s/port_rcv_remote_physical_errors", sysfs_device_port_counters);
            ret_read_file = read_file_long_int(counter_metric_file_path, &long_int_value);
            if (ret_snprintf < 0 || ret_read_file < 0) {
                input_infiniband_metrics->infiniband[count].port_rcv_remote_physical_errors = 0;
            } else {
                input_infiniband_metrics->infiniband[count].port_rcv_remote_physical_errors = long_int_value;
            }

            ret_snprintf = snprintf(counter_metric_file_path, PATH_MAX, "%s/port_rcv_switch_relay_errors", sysfs_device_port_counters);
            ret_read_file = read_file_long_int(counter_metric_file_path, &long_int_value);
            if (ret_snprintf < 0 || ret_read_file < 0) {
                input_infiniband_metrics->infiniband[count].port_rcv_switch_relay_errors = 0;
            } else {
                input_infiniband_metrics->infiniband[count].port_rcv_switch_relay_errors = long_int_value;
            }

            ret_snprintf = snprintf(counter_metric_file_path, PATH_MAX, "%s/link_error_recovery", sysfs_device_port_counters);
            ret_read_file = read_file_long_int(counter_metric_file_path, &long_int_value);
            if (ret_snprintf < 0 || ret_read_file < 0) {
                input_infiniband_metrics->infiniband[count].link_error_recovery = 0;
            } else {
                input_infiniband_metrics->infiniband[count].link_error_recovery = long_int_value;
            }

            ret_snprintf = snprintf(counter_metric_file_path, PATH_MAX, "%s/port_xmit_constraint_errors", sysfs_device_port_counters);
            ret_read_file = read_file_long_int(counter_metric_file_path, &long_int_value);
            if (ret_snprintf < 0 || ret_read_file < 0) {
                input_infiniband_metrics->infiniband[count].port_xmit_constraint_errors = 0;
            } else {
                input_infiniband_metrics->infiniband[count].port_xmit_constraint_errors = long_int_value;
            }

            ret_snprintf = snprintf(counter_metric_file_path, PATH_MAX, "%s/port_rcv_constraint_errors", sysfs_device_port_counters);
            ret_read_file = read_file_long_int(counter_metric_file_path, &long_int_value);
            if (ret_snprintf < 0 || ret_read_file < 0) {
                input_infiniband_metrics->infiniband[count].port_rcv_constraint_errors = 0;
            } else {
                input_infiniband_metrics->infiniband[count].port_rcv_constraint_errors = long_int_value;
            }

            ret_snprintf = snprintf(counter_metric_file_path, PATH_MAX, "%s/local_link_integrity_errors", sysfs_device_port_counters);
            ret_read_file = read_file_long_int(counter_metric_file_path, &long_int_value);
            if (ret_snprintf < 0 || ret_read_file < 0) {
                input_infiniband_metrics->infiniband[count].local_link_integrity_errors = 0;
            } else {
                input_infiniband_metrics->infiniband[count].local_link_integrity_errors = long_int_value;
            }

            ret_snprintf = snprintf(counter_metric_file_path, PATH_MAX, "%s/excessive_buffer_overrun_errors", sysfs_device_port_counters);
            ret_read_file = read_file_long_int(counter_metric_file_path, &long_int_value);
            if (ret_snprintf < 0 || ret_read_file < 0) {
                input_infiniband_metrics->infiniband[count].excessive_buffer_overrun_errors = 0;
            } else {
                input_infiniband_metrics->infiniband[count].excessive_buffer_overrun_errors = long_int_value;
            }

            ret_snprintf = snprintf(counter_metric_file_path, PATH_MAX, "%s/port_xmit_data", sysfs_device_port_counters);
            ret_read_file = read_file_long_int(counter_metric_file_path, &long_int_value);
            if (ret_snprintf < 0 || ret_read_file < 0) {
                input_infiniband_metrics->infiniband[count].port_xmit_data = 0;
            } else {
                input_infiniband_metrics->infiniband[count].port_xmit_data = long_int_value;
            }

            ret_snprintf = snprintf(counter_metric_file_path, PATH_MAX, "%s/port_rcv_data", sysfs_device_port_counters);
            ret_read_file = read_file_long_int(counter_metric_file_path, &long_int_value);
            if (ret_snprintf < 0 || ret_read_file < 0) {
                input_infiniband_metrics->infiniband[count].port_rcv_data = 0;
            } else {
                input_infiniband_metrics->infiniband[count].port_rcv_data = long_int_value;
            }

            ret_snprintf = snprintf(counter_metric_file_path, PATH_MAX, "%s/port_xmit_packets", sysfs_device_port_counters);
            ret_read_file = read_file_long_int(counter_metric_file_path, &long_int_value);
            if (ret_snprintf < 0 || ret_read_file < 0) {
                input_infiniband_metrics->infiniband[count].port_xmit_packets = 0;
            } else {
                input_infiniband_metrics->infiniband[count].port_xmit_packets = long_int_value;
            }

            ret_snprintf = snprintf(counter_metric_file_path, PATH_MAX, "%s/port_rcv_packets", sysfs_device_port_counters);
            ret_read_file = read_file_long_int(counter_metric_file_path, &long_int_value);
            if (ret_snprintf < 0 || ret_read_file < 0) {
                input_infiniband_metrics->infiniband[count].port_rcv_packets = 0;
            } else {
                input_infiniband_metrics->infiniband[count].port_rcv_packets = long_int_value;
            }

            ret_snprintf = snprintf(counter_metric_file_path, PATH_MAX, "%s/unicast_rcv_packets", sysfs_device_port_counters);
            ret_read_file = read_file_long_int(counter_metric_file_path, &long_int_value);
            if (ret_snprintf < 0 || ret_read_file < 0) {
                input_infiniband_metrics->infiniband[count].unicast_rcv_packets = 0;
            } else {
                input_infiniband_metrics->infiniband[count].unicast_rcv_packets = long_int_value;
            }

            ret_snprintf = snprintf(counter_metric_file_path, PATH_MAX, "%s/unicast_xmit_packets", sysfs_device_port_counters);
            ret_read_file = read_file_long_int(counter_metric_file_path, &long_int_value);
            if (ret_snprintf < 0 || ret_read_file < 0) {
                input_infiniband_metrics->infiniband[count].unicast_xmit_packets = 0;
            } else {
                input_infiniband_metrics->infiniband[count].unicast_xmit_packets = long_int_value;
            }

            ret_snprintf = snprintf(counter_metric_file_path, PATH_MAX, "%s/multicast_rcv_packets", sysfs_device_port_counters);
            ret_read_file = read_file_long_int(counter_metric_file_path, &long_int_value);
            if (ret_snprintf < 0 || ret_read_file < 0) {
                input_infiniband_metrics->infiniband[count].multicast_rcv_packets = 0;
            } else {
                input_infiniband_metrics->infiniband[count].multicast_rcv_packets = long_int_value;
            }

            ret_snprintf = snprintf(counter_metric_file_path, PATH_MAX, "%s/multicast_xmit_packets", sysfs_device_port_counters);
            ret_read_file = read_file_long_int(counter_metric_file_path, &long_int_value);
            if (ret_snprintf < 0 || ret_read_file < 0) {
                input_infiniband_metrics->infiniband[count].multicast_xmit_packets = 0;
            } else {
                input_infiniband_metrics->infiniband[count].multicast_xmit_packets = long_int_value;
            }

            ret_snprintf = snprintf(counter_metric_file_path, PATH_MAX, "%s/link_downed", sysfs_device_port_counters);
            ret_read_file = read_file_long_int(counter_metric_file_path, &long_int_value);
            if (ret_snprintf < 0 || ret_read_file < 0) {
                input_infiniband_metrics->infiniband[count].link_downed = 0;
            } else {
                input_infiniband_metrics->infiniband[count].link_downed = long_int_value;
            }

            ret_snprintf = snprintf(counter_metric_file_path, PATH_MAX, "%s/port_xmit_discards", sysfs_device_port_counters);
            ret_read_file = read_file_long_int(counter_metric_file_path, &long_int_value);
            if (ret_snprintf < 0 || ret_read_file < 0) {
                input_infiniband_metrics->infiniband[count].port_xmit_discards = 0;
            } else {
                input_infiniband_metrics->infiniband[count].port_xmit_discards = long_int_value;
            }

            ret_snprintf = snprintf(counter_metric_file_path, PATH_MAX, "%s/VL15_dropped", sysfs_device_port_counters);
            ret_read_file = read_file_long_int(counter_metric_file_path, &long_int_value);
            if (ret_snprintf < 0 || ret_read_file < 0) {
                input_infiniband_metrics->infiniband[count].VL15_dropped = 0;
            } else {
                input_infiniband_metrics->infiniband[count].VL15_dropped = long_int_value;
            }

            ++count;
        }

        closedir(device_dir_handle);
    }

    closedir(sysfs_dir_handle);

    return count;
}
