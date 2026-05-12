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

#include <ncurses.h>
#include "ncurses_utils.h"

void construct_window_layout(WINDOW *input_window, int interface_count) {
    /* layour constants */
    char *interface_status_banner = "Interface Status";
    char *interface_status_layout = "Interface Name  |   LID   |   Link Layer   |      State      |  Physical State  |     Rate";

    char *interface_io_banner = "Interface I/O (per second)";
    char *interface_io_layout = "Interface Name  |  RX Packet  |   RX MB   |  TX Packet  |   TX MB   |  UC RX Packet  |  UC TX Packet  |  MC RX Packet  |  MC TX Packet";

    char *interface_error_banner = "Interface Error (cumulative)";
    char *interface_error_layout = "Interface Name  | Symbol |   RX   | RX Remote PHY | RX Switch Relay | RX Const. | TX Const. | Buffer Overrun | TX Discard | VL15 Dropped";

    char *interface_link_error_banner = "Interface Link Error (cumulative)";
    char *interface_link_error_layout = "Interface Name  | Link Error Recovery | Local Link Integrity | Link Downed";

    /* move curser and print layout 
     * banner should have A_STANDOUT attribute; metric names should have A_BOLD attribute
    */
    wattron(input_window, A_STANDOUT);
    mvwprintw(input_window, 1, 1, interface_status_banner);
    mvwprintw(input_window, interface_count + 6, 1, interface_io_banner);
    mvwprintw(input_window, 2 * interface_count + 11, 1, interface_error_banner);
    mvwprintw(input_window, 3 * interface_count + 16, 1, interface_link_error_banner);
    wattroff(input_window, A_STANDOUT);

    wattron(input_window, A_BOLD);
    mvwprintw(input_window, 3, 1, interface_status_layout);
    mvwprintw(input_window, interface_count + 8, 1, interface_io_layout);
    mvwprintw(input_window, 2 * interface_count + 13, 1, interface_error_layout);
    mvwprintw(input_window, 3 * interface_count + 18, 1, interface_link_error_layout);
    wattroff(input_window, A_BOLD);

    mvwhline(input_window, interface_count + 5, 1, ACS_HLINE, COLS - 2);
    mvwhline(input_window, 2 * interface_count + 10, 1, ACS_HLINE, COLS - 2);
    mvwhline(input_window, 3 * interface_count + 15, 1, ACS_HLINE, COLS - 2);

    /* print footer */
    mvwprintw(input_window, LINES - 1, 10, "press 'Q' to exit");

    /* refresh window */
    wrefresh(input_window);
}

void print_delimiter(WINDOW *input_window, int row_number, int *column_positions, size_t column_size) {
    char *delimiter = "|";

    wattron(input_window, A_BOLD);
    for (size_t i = 0; i < column_size; ++i) {
        mvwprintw(input_window, row_number, column_positions[i], delimiter);
    }
    wattroff(input_window, A_BOLD);
}
