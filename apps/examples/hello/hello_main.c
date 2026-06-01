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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/mount.h>
#include <sys/ioctl.h>
#include <tinyara/fs/ioctl.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define NUM_FILES       10
#define FILE_SIZE       (32 * 1024)  /* 32KB */
#define NUM_ITERATIONS  10
#define SMART_DEV_PATH  "/dev/smart0p12"
#define MOUNT_POINT     "/mnt"

/****************************************************************************
 * hello_main
 ****************************************************************************/

#ifdef CONFIG_BUILD_KERNEL
int main(int argc, FAR char *argv[])
#else
int hello_main(int argc, char *argv[])
#endif
{
	int iter, i, fd;
	char filepath[32];
	char *write_buf;
	char *read_buf;
	ssize_t ret;
	struct timespec t_start, t_end;
	double write_time;
	double read_time;
	double total_write_time = 0.0;
	double total_read_time = 0.0;
	int total_write_ops = 0;
	int total_read_ops = 0;

	/* 힙에 버퍼 할당 (스택 오버플로우 방지) */
	write_buf = (char *)malloc(FILE_SIZE);
	read_buf = (char *)malloc(FILE_SIZE);
	if (!write_buf || !read_buf) {
		printf("FAIL: malloc failed\n");
		free(write_buf);
		free(read_buf);
		return -1;
	}

	memset(write_buf, 0xAA, FILE_SIZE);

	for (iter = 1; iter <= NUM_ITERATIONS; iter++) {
		printf("[Iteration %d/%d] Start\n", iter, NUM_ITERATIONS);

		/* 1. 32KB 파일 10개 생성 및 write */
		clock_gettime(CLOCK_MONOTONIC, &t_start);
		for (i = 1; i <= NUM_FILES; i++) {
			snprintf(filepath, sizeof(filepath), "/mnt/%d", i);
			fd = open(filepath, O_WRONLY | O_CREAT | O_TRUNC, 0666);
			if (fd < 0) {
				printf("FAIL: open for write %s (iter=%d)\n", filepath, iter);
				goto out_err;
			}
			ret = write(fd, write_buf, FILE_SIZE);
			if (ret != FILE_SIZE) {
				printf("FAIL: write %s (iter=%d, ret=%d)\n", filepath, iter, (int)ret);
				close(fd);
				goto out_err;
			}
			close(fd);
			total_write_ops++;
		}
		clock_gettime(CLOCK_MONOTONIC, &t_end);
		write_time = (t_end.tv_sec - t_start.tv_sec) + (t_end.tv_nsec - t_start.tv_nsec) / 1e9;
		total_write_time += write_time;
		printf("[Iteration %d/%d] Write done (%.6f sec)\n", iter, NUM_ITERATIONS, write_time);

		/* 2. 32KB 파일 10개 차례대로 read */
		clock_gettime(CLOCK_MONOTONIC, &t_start);
		for (i = 1; i <= NUM_FILES; i++) {
			snprintf(filepath, sizeof(filepath), "/mnt/%d", i);
			fd = open(filepath, O_RDONLY);
			if (fd < 0) {
				printf("FAIL: open for read %s (iter=%d)\n", filepath, iter);
				goto out_err;
			}
			ret = read(fd, read_buf, FILE_SIZE);
			if (ret != FILE_SIZE) {
				printf("FAIL: read %s (iter=%d, ret=%d)\n", filepath, iter, (int)ret);
				close(fd);
				goto out_err;
			}
			close(fd);
			total_read_ops++;
		}
		clock_gettime(CLOCK_MONOTONIC, &t_end);
		read_time = (t_end.tv_sec - t_start.tv_sec) + (t_end.tv_nsec - t_start.tv_nsec) / 1e9;
		total_read_time += read_time;
		printf("[Iteration %d/%d] Read done (%.6f sec)\n", iter, NUM_ITERATIONS, read_time);

		/* 3. 파일 모두 삭제 */
		for (i = 1; i <= NUM_FILES; i++) {
			snprintf(filepath, sizeof(filepath), "/mnt/%d", i);
			ret = unlink(filepath);
			if (ret < 0) {
				printf("FAIL: unlink %s (iter=%d)\n", filepath, iter);
				goto out_err;
			}
		}
		printf("[Iteration %d/%d] Delete done\n", iter, NUM_ITERATIONS);
	}

	/* Format the smartfs device after all tests */
	{
		int fmt_fd;
		int fmt_ret;

		printf("\nFormatting %s ...\n", SMART_DEV_PATH);

		/* Open the smart block device */
		fmt_fd = open(SMART_DEV_PATH, O_RDWR);
		if (fmt_fd < 0) {
			printf("FAIL: open %s for format\n", SMART_DEV_PATH);
			free(write_buf);
			free(read_buf);
			return -1;
		}

		/* Send BIOC_LLFORMAT ioctl to erase all blocks and reformat */
		clock_gettime(CLOCK_REALTIME, &t_start);
		fmt_ret = ioctl(fmt_fd, BIOC_LLFORMAT, 0);
		clock_gettime(CLOCK_REALTIME, &t_end);
		if (fmt_ret < 0) {
			printf("FAIL: BIOC_LLFORMAT failed (ret=%d)\n", fmt_ret);
			close(fmt_fd);
			free(write_buf);
			free(read_buf);
			return -1;
		}
		close(fmt_fd);

		double format_time = (t_end.tv_sec - t_start.tv_sec) + (t_end.tv_nsec - t_start.tv_nsec) / 1e9;
		printf("Format done (%.6f sec)\n", format_time);
	}

	/* 결과 출력 */
	printf("\n========== RESULT ==========\n");
	printf("Total iterations : %d\n", NUM_ITERATIONS);
	printf("Total write ops  : %d\n", total_write_ops);
	printf("Total read ops   : %d\n", total_read_ops);
	printf("Write total time : %.6f sec\n", total_write_time);
	printf("Write avg time   : %.6f sec (per 10 files)\n", total_write_time / NUM_ITERATIONS);
	printf("Read total time  : %.6f sec\n", total_read_time);
	printf("Read avg time    : %.6f sec (per 10 files)\n", total_read_time / NUM_ITERATIONS);
	printf("============================\n");

	free(write_buf);
	free(read_buf);
	return 0;

out_err:
	/* 에러 발생 시까지의 결과 출력 */
	printf("\n========== RESULT (ERROR) ==========\n");
	printf("Failed at iteration : %d\n", iter);
	printf("Write total time    : %.6f sec (%d ops)\n", total_write_time, total_write_ops);
	printf("Read total time     : %.6f sec (%d ops)\n", total_read_time, total_read_ops);
	printf("====================================\n");

	free(write_buf);
	free(read_buf);
	return -1;
}
