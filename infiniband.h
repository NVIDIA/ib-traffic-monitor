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

#ifndef INFINIBAND_H
#define INFINIBAND_H

#include <stdio.h>

#define INTERFACE_COUNT 32
#define IB_DEVICE_NAME_MAX 64

struct interface {
    char interface_name[IB_DEVICE_NAME_MAX];
    char link_layer[BUFSIZ];
    char state[BUFSIZ];
    char phys_state[BUFSIZ];
    char rate[BUFSIZ];
    long int lid;
    long int symbol_error;
    long int port_rcv_errors;
    long int port_rcv_remote_physical_errors;
    long int port_rcv_switch_relay_errors;
    long int link_error_recovery;
    long int port_xmit_constraint_errors;
    long int port_rcv_constraint_errors;
    long int local_link_integrity_errors;
    long int excessive_buffer_overrun_errors;
    long int port_xmit_data;
    long int port_rcv_data;
    long int port_xmit_packets;
    long int port_rcv_packets;
    long int unicast_rcv_packets;
    long int unicast_xmit_packets;
    long int multicast_rcv_packets;
    long int multicast_xmit_packets;
    long int link_downed;
    long int port_xmit_discards;
    long int VL15_dropped;
};

struct infiniband_metrics {
    /* interface metrics */
    struct interface infiniband[INTERFACE_COUNT];
};

extern int get_infiniband_metrics(struct infiniband_metrics *input_infiniband_metrics, int show_ethernet_flag);

#endif /* INFINIBAND_H */
