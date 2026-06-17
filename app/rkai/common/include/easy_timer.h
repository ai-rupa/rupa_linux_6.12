// Copyright (c) 2024 by Rockchip Electronics Co., Ltd. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef _RKAI_API_TIMER_H_
#define _RKAI_API_TIMER_H_

#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// #define TIMING_DISABLED // 如果不需要打印耗时，可以取消注释

typedef struct {
    struct timeval start_time;
    struct timeval stop_time;
    char indent[40];
} rkaiTimer;

static inline double __get_us(struct timeval t) {
    return (t.tv_sec * 1000000.0 + t.tv_usec);
}

static inline void timer_init(rkaiTimer* t) {
    if (t) {
        memset(t, 0, sizeof(rkaiTimer));
        strcpy(t->indent, "-- ");
    }
}

static inline void timer_indent_set(rkaiTimer* t, const char* s) {
    if (t && s) {
        strncpy(t->indent, s, sizeof(t->indent) - 1);
        t->indent[sizeof(t->indent) - 1] = '\0';
    }
}

static inline void tik(rkaiTimer* t) {
    if (t) {
        gettimeofday(&t->start_time, NULL);
    }
}

static inline void tok(rkaiTimer* t) {
    if (t) {
        gettimeofday(&t->stop_time, NULL);
    }
}

static inline float get_time(rkaiTimer* t) {
    if (!t) return 0.0f;
    return (float)((__get_us(t->stop_time) - __get_us(t->start_time)) / 1000.0);
}

#ifndef TIMING_DISABLED
static inline void print_time(rkaiTimer* t, const char* str) {
    if (t && str) {
        printf("%s%s use: %f ms\n", t->indent, str, get_time(t));
    }
}
#else
static inline void print_time(rkaiTimer* t, const char* str) {
    // Do nothing if timing is disabled
    (void)t;
    (void)str;
}
#endif
#endif 
