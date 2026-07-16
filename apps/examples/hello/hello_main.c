/****************************************************************************
 *
 * Copyright 2016 Samsung Electronics All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 *
 ****************************************************************************/
/****************************************************************************
 * examples/hello/hello_main.c
 *
 *   Copyright (C) 2008, 2011-2012 Gregory Nutt. All rights reserved.
 *   Author: Gregory Nutt <gnutt@nuttx.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <tinyara/config.h>

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define DEFAULT_WORKER_PATH "/mnt/smartfs_cancel_worker.bin"
#define DEFAULT_PROBE_PATH  "/mnt/smartfs_cancel_probe.bin"
#define WRITE_BUFFER_SIZE   (64 * 1024)
// #define WRITE_BUFFER_SIZE   (64 * 1024)
#define WRITE_HOLD_USEC     1000
#define WRITE_WAIT_USEC     1000
#define WRITE_WAIT_COUNT    5000
#define PROBE_WAIT_SECONDS  1

/****************************************************************************
 * Private Data
 ****************************************************************************/

static sem_t g_worker_ready;
static FAR const char *g_worker_path;
static FAR const char *g_probe_path;
static volatile bool g_write_active;
static volatile bool g_worker_failed;
static volatile bool g_probe_done;
static int g_writer_fd = -1;
static int g_probe_result;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static FAR void *smartfs_writer(FAR void *arg)
{
    FAR uint8_t *buffer;
    unsigned long iteration = 0;
    ssize_t written;
    int ret;
    int fd;

    (void)arg;

    buffer = (FAR uint8_t *)malloc(WRITE_BUFFER_SIZE);
    if (buffer == NULL) {
        printf("writer: malloc failed\n");
        g_worker_failed = true;
        sem_post(&g_worker_ready);
        return (FAR void *)ERROR;
    }

    memset(buffer, 0xa5, WRITE_BUFFER_SIZE);
    fd = open(g_worker_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        printf("writer: open(%s) failed: %d\n", g_worker_path, errno);
        free(buffer);
        g_worker_failed = true;
        sem_post(&g_worker_ready);
        return (FAR void *)ERROR;
    }
    g_writer_fd = fd;

    printf("writer: repeatedly writing %u bytes to %s\n",
           (unsigned int)WRITE_BUFFER_SIZE, g_worker_path);
    sem_post(&g_worker_ready);

    for (;;) {
        if (lseek(fd, 0, SEEK_SET) < 0) {
            printf("writer: lseek failed: %d\n", errno);
            break;
        }

        iteration++;
        memcpy(buffer, &iteration, sizeof(iteration));

        /* g_write_active deliberately covers only the SmartFS write call. */

        g_write_active = true;
        printf("[DEBUG] write itr(%d)\n", iteration);
        written = write(fd, buffer, WRITE_BUFFER_SIZE);
        g_write_active = false;

        if (written != WRITE_BUFFER_SIZE) {
            printf("writer: write failed: result=%ld errno=%d\n",
                   (long)written, errno);
            break;
        }
        printf("[DEBUG] write pass\n");
    }

    g_worker_failed = true;
    close(fd);
    g_writer_fd = -1;
    free(buffer);
    return (FAR void *)ERROR;
}

static FAR void *smartfs_probe(FAR void *arg)
{
    static const char probe_data[] = "smartfs probe\n";
    ssize_t written;
    int fd;

    (void)arg;

    printf("probe: opening %s\n", g_probe_path);
    fd = open(g_probe_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        printf("probe: open failed: %d\n", errno);
        g_probe_result = ERROR;
        g_probe_done = true;
        return (FAR void *)ERROR;
    }

    written = write(fd, probe_data, sizeof(probe_data));
    if (written != sizeof(probe_data)) {
        printf("probe: write failed: result=%ld errno=%d\n",
               (long)written, errno);
        g_probe_result = ERROR;
    } else {
        g_probe_result = OK;
    }

    close(fd);
    g_probe_done = true;
    return g_probe_result == OK ? NULL : (FAR void *)ERROR;
}

/****************************************************************************
 * hello_main
 ****************************************************************************/

#ifdef CONFIG_BUILD_KERNEL
int main(int argc, FAR char *argv[])
#else
int hello_main(int argc, char *argv[])
#endif
{
    FAR void *join_value = NULL;
    pthread_t probe;
    pthread_t writer;
    unsigned int wait_count;
    int ret;

#ifdef CONFIG_CANCELLATION_POINTS
    printf("ERROR: This reproducer requires CONFIG_CANCELLATION_POINTS=n\n");
    return EXIT_FAILURE;
#endif

    g_worker_path = argc > 1 ? argv[1] : DEFAULT_WORKER_PATH;
    g_probe_path = argc > 2 ? argv[2] : DEFAULT_PROBE_PATH;
    g_write_active = false;
    g_worker_failed = false;
    g_probe_done = false;
    g_writer_fd = -1;
    g_probe_result = ERROR;

    printf("WARNING: CONFIG_CANCELLATION_POINTS must remain disabled.\n");
    printf("WARNING: The worker requests DEFERRED cancellation, but this build\n");
    printf("WARNING: may terminate it immediately while SmartFS holds g_sem.\n");

    ret = sem_init(&g_worker_ready, 0, 0);
    if (ret != OK) {
        printf("ERROR: sem_init failed: %d\n", errno);
        return EXIT_FAILURE;
    }

    ret = pthread_create(&writer, NULL, smartfs_writer, NULL);
    if (ret != OK) {
        printf("ERROR: writer pthread_create failed: %d\n", ret);
        sem_destroy(&g_worker_ready);
        return EXIT_FAILURE;
    }

    do {
        ret = sem_wait(&g_worker_ready);
    } while (ret != OK && errno == EINTR);
    if (ret != OK) {
        printf("ERROR: sem_wait failed: %d\n", errno);
        pthread_cancel(writer);
        pthread_join(writer, &join_value);
        sem_destroy(&g_worker_ready);
        return EXIT_FAILURE;
    }

    /* Wait until one write call has remained active long enough that the
     * writer is very likely inside SmartFS with g_sem held.
     */

    for (wait_count = 0; wait_count < WRITE_WAIT_COUNT; wait_count++) {
        if (g_worker_failed) {
            printf("ERROR: writer stopped before cancellation\n");
            pthread_join(writer, &join_value);
            sem_destroy(&g_worker_ready);
            return EXIT_FAILURE;
        }

        if (g_write_active) {
            usleep(WRITE_HOLD_USEC);
            if (g_write_active) {
                break;
            }
        }

        usleep(WRITE_WAIT_USEC);
    }

    if (!g_write_active) {
        printf("ERROR: Could not observe a long-running SmartFS write\n");
        pthread_cancel(writer);
        pthread_join(writer, &join_value);
        sem_destroy(&g_worker_ready);
        return EXIT_FAILURE;
    }

    printf("main: canceling DEFERRED writer while write() is active\n");
    ret = pthread_cancel(writer);
    if (ret != OK) {
        printf("ERROR: pthread_cancel(writer) failed: %d\n", ret);
        sem_destroy(&g_worker_ready);
        return EXIT_FAILURE;
    }

    ret = pthread_join(writer, &join_value);
    printf("main: writer join result=%d value=%p%s\n", ret, join_value,
           join_value == PTHREAD_CANCELED ? " (PTHREAD_CANCELED)" : "");

    /* A second thread now performs a tiny SmartFS operation.  If the canceled
     * writer stranded g_sem, this thread will remain blocked in SmartFS.
     */

    ret = pthread_create(&probe, NULL, smartfs_probe, NULL);
    if (ret != OK) {
        printf("ERROR: probe pthread_create failed: %d\n", ret);
        sem_destroy(&g_worker_ready);
        return EXIT_FAILURE;
    }

    sleep(PROBE_WAIT_SECONDS);
    if (!g_probe_done) {
        printf("REPRODUCED: probe is still blocked after %d seconds; "
               "SmartFS g_sem is likely stranded\n", PROBE_WAIT_SECONDS);
        pthread_cancel(probe);
        pthread_join(probe, &join_value);
    } else {
        pthread_join(probe, &join_value);
        printf("NOT REPRODUCED: probe completed with result=%d\n",
               g_probe_result);
        if (g_writer_fd >= 0) {
            close(g_writer_fd);
            g_writer_fd = -1;
        }
    }

    sem_destroy(&g_worker_ready);
    return EXIT_SUCCESS;
}
