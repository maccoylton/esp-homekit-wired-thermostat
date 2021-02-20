/*-
 * Copyright (c) 2018, Jeff Kletsky
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 *     * Neither the name of the copyright holder nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef EXTRAS_TIMEKEEPING_SNTP_IMPL_H
#define EXTRAS_TIMEKEEPING_SNTP_IMPL_H

#define SNTP_DEBUG      LWIP_DBG_ON

#define SNTP_SERVER_DNS 1

/*
 * 0 -- off
 * 1 -- enables address/port check
 * 2 -- adds timestamp check against that sent
 * Rest not yet implemented as of 2018-02-08; upstream commit 010b0210
 * 3 -- will add field-level checks
 * 4 -- will add root delay/dispersion tests
 */
#define SNTP_CHECK_RESPONSE 2

#define SNTP_COMP_ROUNDTRIP 1

#define SNTP_STARTUP_DELAY 0

#define SNTP_MAX_SERVERS 3

/* 60 seconds is lower threshold by NTPv4 spec */
#define SNTP_UPDATE_DELAY (64 * 1000)

#define SNTP_SERVER_ADDRESS         "213.161.194.93" /* pool.ntp.org */

#include <lwip/apps/sntp.h>

/*
 * Getter has to be a inline #define
 * as it modifies arguments in place
 * u32_t in lwip/apps/sntp.c
 * lwip/include/arch/cc.h:typedef uint32_t    u32_t;
 */
#include <stdint.h>
#define SNTP_GET_SYSTEM_TIME(S, F) do { \
    struct timeval _tv;                 \
    gettimeofday(&_tv, NULL);           \
    S = (uint32_t)_tv.tv_sec;           \
    F = (uint32_t)_tv.tv_usec;          \
} while (0)

#define SNTP_SET_SYSTEM_TIME_US(S, F) sntp_impl_set_system_time_us(S, F)


#include <sys/time.h>

void
sntp_impl_set_system_time_us(uint32_t secs, uint32_t frac);

/* See lwipopts.h for #define statements to couple with LWIP SNTP */

#endif /* EXTRAS_TIMEKEEPING_SNTP_IMPL_H */
