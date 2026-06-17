/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2019, Linaro Limited
 */
#ifndef __STRINGS_H
#define __STRINGS_H

#include <sys/types.h>
#include <sys/cdefs.h>

int bcmp(const void *s1, const void *s2, size_t n);
int strcasecmp(const char*, const char*);
int strncasecmp(const char*, const char*, size_t);

#endif /*__STRINGS_H*/
