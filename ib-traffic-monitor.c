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

#include <errno.h>
#include <getopt.h>
#include <ncurses.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include "infiniband.h"
#include "ncurses_utils.h"
#include "utils.h"

#define VERSION "1.4.1"

/* define usage function */
static void usage(void) {
    printf(
        "InfiniBand Traffic Monitor - Version %s\n"
        "usage: ib-traffic-monitor [-r|--refresh <second(s)>]\n"
        "                          [-e|--ethernet]\n"
        "                          [-m|--memory-lock]\n"
        "                          [-h|--help]\n", VERSION
    );
}

/* define SIGINT signal handler */
static volatile sig_atomic_t break_flag = 0;
static void sigint_handler(int signo) {
    (void)signo;

    break_flag = 1;
}

int main(int argc, char *argv[]) {
    /* define command-line options */
    char *short_opts = "r:emh";
    struct option long_opts[] = {
        {"refresh", required_argument, NULL, 'r'},
        {"ethernet", no_argument, NULL, 'e'},
        {"memory-lock", no_argument, NULL, 'm'},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0}
    };

    long int refresh_second = 5;
    int ethernet_flag = 0;
    int memory_lock_flag = 0;
    int error_flag = 0;
    char error_msg[BUFSIZ];
    int exit_code = EXIT_SUCCESS;

    /* suppress default getopt error messages */
    opterr = 0;

    int c;

    while (1) {
        c = getopt_long(argc, argv, short_opts, long_opts, NULL);

        if (c == -1) {
            break;
        }

        switch (c) {
            case 'r':
                errno = 0;
                refresh_second = strtol(optarg, NULL, 10);

                if (errno != 0) {
                    fprintf(stderr, "ERROR: failed to convert refresh second value\n\n");
                    exit(EXIT_FAILURE);
                }

                if (refresh_second <= 0) {
                    fprintf(stderr, "ERROR: refresh second must be an integer and greater than 0\n\n");
                    usage();
                    exit(EXIT_FAILURE);
                }

                break;
            case 'e':
                ethernet_flag = 1;
                break;
            case 'm':
                memory_lock_flag = 1;
                break;
            case 'h':
                usage();
                exit(EXIT_SUCCESS);
            case '?':
                fprintf(stderr, "ERROR: Unknown option\n\n");
                usage();
                exit(EXIT_FAILURE);
            default:
                fprintf(stderr, "ERROR: Unimplemented option\n\n");
                usage();
                exit(EXIT_FAILURE);
        }
    }

    /* check if host OS is Linux */
    if (is_linux() != 1) {
        fprintf(stderr, "ERROR: InfiniBand Traffic Monitor can be only running on Linux operating system\n");
        exit(EXIT_FAILURE);
    }

    /* lock memory pages if memory lock option is enabled */
    if (memory_lock_flag > 0) {
        if (mlockall(MCL_CURRENT | MCL_FUTURE) < 0) {
            fprintf(stderr, "ERROR: failed to lock memory\n");
            exit(EXIT_FAILURE);
        }
    }

    /* initialize metric structs */
    struct infiniband_metrics cur_infiniband_metrics;
    struct infiniband_metrics prev_infiniband_metrics;

    /* delimiter positions */
    int interface_status_positions[] = {17, 27, 44, 62, 81};
    int interface_io_positions[] = {17, 31, 43, 57, 69, 86, 103, 120};
    int interface_error_positions[] = {17, 26, 35, 51, 69, 81, 93, 110, 123};
    int interface_link_error_positions[] = {17, 39, 62};

    /* previous data copy state flag */
    int prev_data_flag = 0;

    /* initialize previous infiniband interface return value */
    int prev_ret_get_infiniband_metrics;

    /* initialize signal-related variables */
    struct sigaction sa;
    sigset_t signal_empty_set;
    sigset_t signal_block_set;

    if (sigemptyset(&signal_empty_set) < 0) {
        fprintf(stderr, "ERROR: failed to clear signal set signal_empty_set\n");
        exit(EXIT_FAILURE);
    }

    if (sigemptyset(&signal_block_set) < 0) {
        fprintf(stderr, "ERROR: failed to clear signal set signal_block_set\n");
        exit(EXIT_FAILURE);
    }

    /* add SIGINT signal in signal_block_set */
    if (sigaddset(&signal_block_set, SIGINT) < 0) {
        fprintf(stderr, "ERROR: failed to add SIGINT signal in signal_block_set\n");
        exit(EXIT_FAILURE);
    }

    /* block SIGINT signal */
    if (sigprocmask(SIG_BLOCK, &signal_block_set, NULL) < 0) {
        fprintf(stderr, "ERROR: failed to block SIGINT signal\n");
        exit(EXIT_FAILURE);
    }

    /* install signal handler */
    sa.sa_handler = sigint_handler;
    sa.sa_flags = 0;

    if (sigemptyset(&sa.sa_mask) < 0) {
        fprintf(stderr, "ERROR: failed to clear signal set sa.sa_mask\n");
        exit(EXIT_FAILURE);
    }

    if (sigaction(SIGINT, &sa, NULL) < 0) {
        fprintf(stderr, "ERROR: failed to install signal handler\n");
        exit(EXIT_FAILURE);
    }

    /* initialize ncurses window struct */
    WINDOW *main_window;
    main_window = NULL;

    /* initialize screen */
    initscr();

    /* disable line buffering */
    cbreak();

    /* set cursor state to invisiable */
    curs_set(0);

    /* create window */
    main_window = newwin(0, 0, 0, 0);

    if (main_window == NULL) {
        fprintf(stderr, "ERROR: failed to create ncurses window\n");
        endwin();
        exit(EXIT_FAILURE);
    }

    /* enable non-blocking input */
    nodelay(main_window, TRUE);

    /* data collection and refresh logic */
    while (1) {
        /* clear window */
        wclear(main_window);

        /* create window boarder */
        box(main_window, 0, 0);

        /* initialize variables */
        int ret_get_infiniband_metrics;

        int infiniband_name_found = 0;

        /* pselect() use */
        struct timespec ts;
        fd_set readfds;
        int ret_pselect;

        /* retrieve metrics for infiniband_metrics */
        ret_get_infiniband_metrics = get_infiniband_metrics(&cur_infiniband_metrics, ethernet_flag);
        if (ret_get_infiniband_metrics < 0) {
            strcpy(error_msg, "ERROR: unable to retrieve InfiniBand metrics");
            ++error_flag;
            break;
        }

        if (ret_get_infiniband_metrics == 0) {
            strcpy(error_msg, "ERROR: no InfiniBand device found");
            ++error_flag;
            break;
        }

        /* construct window layout */
        construct_window_layout(main_window, ret_get_infiniband_metrics);

        print_delimiter(main_window, 3 * ret_get_infiniband_metrics + 19, interface_link_error_positions, SIZEOF(interface_link_error_positions));

        /* print available metrics */
        for (int i = 0; i < ret_get_infiniband_metrics; ++i) {
            /* print interface status metrics */
            print_delimiter(main_window, 4 + i, interface_status_positions, SIZEOF(interface_status_positions));
            mvwprintw(main_window, 4 + i, 1, "%-16s", cur_infiniband_metrics.infiniband[i].interface_name);
            mvwprintw(main_window, 4 + i, 22, "%5ld", cur_infiniband_metrics.infiniband[i].lid);
            mvwprintw(main_window, 4 + i, 34, "%10s", cur_infiniband_metrics.infiniband[i].link_layer);
            mvwprintw(main_window, 4 + i, 47, "%15s", cur_infiniband_metrics.infiniband[i].state);
            mvwprintw(main_window, 4 + i, 69, "%12s", cur_infiniband_metrics.infiniband[i].phys_state);
            mvwprintw(main_window, 4 + i, 83, "%22s", cur_infiniband_metrics.infiniband[i].rate);

            /* print error metrics */
            print_delimiter(main_window, 2 * ret_get_infiniband_metrics + 14 + i, interface_error_positions, SIZEOF(interface_error_positions));
            mvwprintw(main_window, 2 * ret_get_infiniband_metrics + 14 + i, 1, "%-16s", cur_infiniband_metrics.infiniband[i].interface_name);
            mvwprintw(main_window, 2 * ret_get_infiniband_metrics + 14 + i, 19, "%7ld", cur_infiniband_metrics.infiniband[i].symbol_error);
            mvwprintw(main_window, 2 * ret_get_infiniband_metrics + 14 + i, 28, "%7ld", cur_infiniband_metrics.infiniband[i].port_rcv_errors);
            mvwprintw(main_window, 2 * ret_get_infiniband_metrics + 14 + i, 43, "%8ld", cur_infiniband_metrics.infiniband[i].port_rcv_remote_physical_errors);
            mvwprintw(main_window, 2 * ret_get_infiniband_metrics + 14 + i, 61, "%8ld", cur_infiniband_metrics.infiniband[i].port_rcv_switch_relay_errors);
            mvwprintw(main_window, 2 * ret_get_infiniband_metrics + 14 + i, 73, "%8ld", cur_infiniband_metrics.infiniband[i].port_rcv_constraint_errors);
            mvwprintw(main_window, 2 * ret_get_infiniband_metrics + 14 + i, 85, "%8ld", cur_infiniband_metrics.infiniband[i].port_xmit_constraint_errors);
            mvwprintw(main_window, 2 * ret_get_infiniband_metrics + 14 + i, 102, "%8ld", cur_infiniband_metrics.infiniband[i].excessive_buffer_overrun_errors);
            mvwprintw(main_window, 2 * ret_get_infiniband_metrics + 14 + i, 115, "%8ld", cur_infiniband_metrics.infiniband[i].port_xmit_discards);
            mvwprintw(main_window, 2 * ret_get_infiniband_metrics + 14 + i, 129, "%8ld", cur_infiniband_metrics.infiniband[i].VL15_dropped);

            /* print link error metrics */
            print_delimiter(main_window, 3 * ret_get_infiniband_metrics + 19 + i, interface_link_error_positions, SIZEOF(interface_link_error_positions));
            mvwprintw(main_window, 3 * ret_get_infiniband_metrics + 19 + i, 1, "%-16s", cur_infiniband_metrics.infiniband[i].interface_name);
            mvwprintw(main_window, 3 * ret_get_infiniband_metrics + 19 + i, 29, "%10ld", cur_infiniband_metrics.infiniband[i].link_error_recovery);
            mvwprintw(main_window, 3 * ret_get_infiniband_metrics + 19 + i, 52, "%10ld", cur_infiniband_metrics.infiniband[i].local_link_integrity_errors);
            mvwprintw(main_window, 3 * ret_get_infiniband_metrics + 19 + i, 67, "%8ld", cur_infiniband_metrics.infiniband[i].link_downed);
        }

        /* print interface IO metrics that need time difference calculation */
        if (prev_data_flag > 0) {
            for (int i = 0; i < ret_get_infiniband_metrics; ++i) {
                for (int j = 0; j < prev_ret_get_infiniband_metrics; ++j) {
                    /* only process if current interface name exists */
                    if (strcmp(cur_infiniband_metrics.infiniband[i].interface_name, prev_infiniband_metrics.infiniband[j].interface_name) == 0) {
                        ++infiniband_name_found;

                        /* print IO metrics */
                        print_delimiter(main_window, ret_get_infiniband_metrics + 8 + infiniband_name_found, interface_io_positions, SIZEOF(interface_io_positions));
                        mvwprintw(main_window, ret_get_infiniband_metrics + 8 + infiniband_name_found, 1, "%-16s", cur_infiniband_metrics.infiniband[i].interface_name);
                        mvwprintw(main_window, ret_get_infiniband_metrics + 8 + infiniband_name_found, 21, "%10ld", (cur_infiniband_metrics.infiniband[i].port_rcv_packets - prev_infiniband_metrics.infiniband[j].port_rcv_packets) / refresh_second);
                        mvwprintw(main_window, ret_get_infiniband_metrics + 8 + infiniband_name_found, 33, "%10ld", (cur_infiniband_metrics.infiniband[i].port_rcv_data - prev_infiniband_metrics.infiniband[j].port_rcv_data) * 4 / 1024 / 1024 / refresh_second);
                        mvwprintw(main_window, ret_get_infiniband_metrics + 8 + infiniband_name_found, 47, "%10ld", (cur_infiniband_metrics.infiniband[i].port_xmit_packets - prev_infiniband_metrics.infiniband[j].port_xmit_packets) / refresh_second);
                        mvwprintw(main_window, ret_get_infiniband_metrics + 8 + infiniband_name_found, 59, "%10ld", (cur_infiniband_metrics.infiniband[i].port_xmit_data - prev_infiniband_metrics.infiniband[j].port_xmit_data) * 4 / 1024 / 1024 / refresh_second);
                        mvwprintw(main_window, ret_get_infiniband_metrics + 8 + infiniband_name_found, 76, "%10ld", (cur_infiniband_metrics.infiniband[i].unicast_rcv_packets - prev_infiniband_metrics.infiniband[j].unicast_rcv_packets) / refresh_second);
                        mvwprintw(main_window, ret_get_infiniband_metrics + 8 + infiniband_name_found, 93, "%10ld", (cur_infiniband_metrics.infiniband[i].unicast_xmit_packets - prev_infiniband_metrics.infiniband[j].unicast_xmit_packets) / refresh_second);
                        mvwprintw(main_window, ret_get_infiniband_metrics + 8 + infiniband_name_found, 110, "%10ld", (cur_infiniband_metrics.infiniband[i].multicast_rcv_packets - prev_infiniband_metrics.infiniband[j].multicast_rcv_packets) / refresh_second);
                        mvwprintw(main_window, ret_get_infiniband_metrics + 8 + infiniband_name_found, 125, "%10ld", (cur_infiniband_metrics.infiniband[i].multicast_xmit_packets - prev_infiniband_metrics.infiniband[j].multicast_xmit_packets) / refresh_second);

                        break;
                    }
                }
            }
        }

        wrefresh(main_window);

        /* clear readfds and only add stdin */
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);

        /* define sleep time period */
        ts.tv_sec = refresh_second;
        ts.tv_nsec = 0;

        ret_pselect = pselect(STDIN_FILENO + 1, &readfds, NULL, NULL, &ts, &signal_empty_set);

        /* sleep and exit the loop if q / Q is pressed */
        if (ret_pselect > 0) {
            char input_c;
            if (read(STDIN_FILENO, &input_c, 1) != 1 || (input_c == 'Q' || input_c == 'q')) {
                break;
            }
        }

        /* sleep and exit the loop if SIGINT is caught */
        if (ret_pselect < 0 && errno == EINTR) {
            if (break_flag > 0) {
                break;
            }
        }

        /* copy the current metrics as previous ones for next calculation */
        prev_infiniband_metrics = cur_infiniband_metrics;
        prev_ret_get_infiniband_metrics = ret_get_infiniband_metrics;

        /* set flag once previous data is copied */
        prev_data_flag = 1;
    }

    /* terminate ncurses window */
    wstandend(main_window);
    delwin(main_window);
    endwin();

    /* print error message if error_flag is set */
    if (error_flag > 0) {
        fprintf(stderr, "%s\n", error_msg);
        exit_code = EXIT_FAILURE;
    }

    /* unlock memory */
    if (memory_lock_flag > 0) {
        munlockall();
    }

    exit(exit_code);
}
