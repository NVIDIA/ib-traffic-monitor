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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>
#include "utils.h"

int is_linux(void) {
    struct utsname uname_data;
    int ret_uname;

    ret_uname = uname(&uname_data);
    if (ret_uname < 0) {
        return 0;
    }

    if (strcmp(uname_data.sysname, "Linux") == 0) {
        return 1;
    } else {
        return 0;
    }
}

int read_file_long_int(char *filename, long int *value) {
    FILE *file_handle;
    file_handle = NULL;

    char line[BUFSIZ];

    file_handle = fopen(filename, "r");
    if (file_handle == NULL) {
        goto handle_error;
    }

    int ret_fscanf = fscanf(file_handle, "%s", line);
    if (ret_fscanf < 1 || ret_fscanf == EOF) {
        goto handle_error;
    }

    *value = strtol(line, NULL, 0);

    fclose(file_handle);

    return 0;

handle_error:
    if (file_handle != NULL) {
        fclose(file_handle);
    }

    return -1;
}

int read_file_char(char *filename, char *value) {
    FILE *file_handle;
    file_handle = NULL;

    file_handle = fopen(filename, "r");
    if (file_handle == NULL) {
        goto handle_error;
    }

    if (fgets(value, BUFSIZ, file_handle) != NULL) {
        size_t value_length = strlen(value);
        if (value_length > 0 && value[value_length - 1] == '\n') {
            value[value_length - 1] = '\0';
        }
    } else {
        goto handle_error;
    }

    fclose(file_handle);

    return 0;

handle_error:
    if (file_handle != NULL) {
        fclose(file_handle);
    }

    return -1;
}
