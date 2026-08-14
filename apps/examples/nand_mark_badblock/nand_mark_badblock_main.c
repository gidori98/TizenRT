/****************************************************************************
 *
 * Copyright 2026 Samsung Electronics All Rights Reserved.
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
 *
 ****************************************************************************/

#include <tinyara/config.h>
#include <tinyara/fs/mtd.h>

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef CONFIG_EXAMPLES_NAND_MARK_BADBLOCK
#include <string.h>
#endif

#ifndef CONFIG_FS_LITTLEFS
#error "CONFIG_FS_LITTLEFS is required"
#endif

#ifndef CONFIG_MTD_DHARA
#error "CONFIG_MTD_DHARA is required"
#endif

static int nand_mark_badblock_parse(FAR const char *arg, off_t neraseblocks,
			       FAR off_t *block)
{
	FAR char *endptr;
	unsigned long value;

	if (arg == NULL || *arg == '\0' || *arg == '-') {
		return -EINVAL;
	}

	errno = 0;
	value = strtoul(arg, &endptr, 0);
	if (errno != 0 || *endptr != '\0' ||
	    value >= (unsigned long)neraseblocks) {
		return -ERANGE;
	}

	*block = (off_t)value;
	return OK;
}

#ifdef CONFIG_EXAMPLES_NAND_MARK_BADBLOCK
#define NAND_MARK_STATUS_COLUMNS 5

static int nand_mark_badblock_is_active(
		off_t block, uint32_t pages_per_block,
		uint32_t erase_blocks,
		FAR const struct dhara_journal_state_s *state)
{
	uint32_t first_active;
	uint32_t last_active;

	if (state->tail_sync == state->head) {
		return 0;
	}

	first_active = state->tail_sync / pages_per_block;
	last_active = state->head == 0 ? erase_blocks - 1 :
		      (state->head - 1) / pages_per_block;

	if (state->tail_sync < state->head) {
		return (uint32_t)block >= first_active &&
		       (uint32_t)block <= last_active;
	}

	/* The protected interval wraps around the end of NAND. */

	return (uint32_t)block >= first_active ||
	       (uint32_t)block <= last_active;
}

static int nand_mark_badblock_status(FAR struct mtd_dev_s *mtd,
				     FAR const struct mtd_geometry_s *geo)
{
	struct dhara_journal_state_s state;
	off_t block;
	unsigned int bad = 0;
	unsigned int active = 0;
	unsigned int errors = 0;
	uint32_t pages_per_block;
	int ret;

	ret = dhara_get_journal_state(mtd, &state);
	if (ret < 0) {
		printf("ERROR: Failed to get Dhara journal state: %d\n", ret);
		return ret;
	}

	if (geo->blocksize == 0 || geo->neraseblocks == 0 ||
	    geo->erasesize % geo->blocksize != 0) {
		printf("ERROR: Invalid MTD geometry\n");
		return -EINVAL;
	}

	pages_per_block = geo->erasesize / geo->blocksize;
	printf("Dhara snapshot: head=%lu tail_sync=%lu\n",
	       (unsigned long)state.head, (unsigned long)state.tail_sync);
	printf("LittleFS MTD partition physical bad-block status:\n");
	for (block = 0; block < geo->neraseblocks; block++) {
		FAR const char *status;

		ret = MTD_ISBAD(mtd, block);
		if (ret < 0) {
			status = "ERROR";
			errors++;
		} else if (ret > 0) {
			status = "BAD";
			bad++;
		} else if (nand_mark_badblock_is_active(block, pages_per_block,
						     geo->neraseblocks,
						     &state)) {
			status = "ACTIVE";
			active++;
		} else {
			status = "GOOD";
		}

		printf("[%4lu] %-6s", (unsigned long)block, status);
		if (((block + 1) % NAND_MARK_STATUS_COLUMNS) == 0 ||
		    block + 1 == geo->neraseblocks) {
			printf("\n");
		} else {
			printf("  ");
		}
	}

	printf("Summary: total=%lu good=%lu active=%u bad=%u errors=%u\n",
	       (unsigned long)geo->neraseblocks,
	       (unsigned long)geo->neraseblocks - active - bad - errors,
	       active, bad, errors);

	return errors ? -EIO : OK;
}
#endif

#ifdef CONFIG_BUILD_KERNEL
int main(int argc, FAR char *argv[])
#else
int nand_mark_badblock_main(int argc, char *argv[])
#endif
{
	FAR struct mtd_dev_s *mtd;
	struct mtd_geometry_s geo;
	off_t block;
	int ret;

	if (argc != 2) {
		printf("Usage: nand_mark_badblock <partition-block>\n");
#ifdef CONFIG_EXAMPLES_NAND_MARK_BADBLOCK
		printf("       nand_mark_badblock status\n");
#endif
		return -EINVAL;
	}

	mtd = get_mtd_partition(MTD_FS);
	if (mtd == NULL) {
		printf("ERROR: LittleFS MTD partition was not found\n");
		return -ENODEV;
	}

	ret = MTD_IOCTL(mtd, MTDIOC_GEOMETRY,
			(unsigned long)((uintptr_t)&geo));
	if (ret < 0) {
		printf("ERROR: Failed to get LittleFS MTD geometry: %d\n", ret);
		return ret;
	}

	printf("LittleFS partition: %lu erase blocks, %lu bytes per block\n",
	       (unsigned long)geo.neraseblocks,
	       (unsigned long)geo.erasesize);

#ifdef CONFIG_EXAMPLES_NAND_MARK_BADBLOCK
	if (strcmp(argv[1], "status") == 0) {
		return nand_mark_badblock_status(mtd, &geo);
	}
#endif

	ret = nand_mark_badblock_parse(argv[1], geo.neraseblocks, &block);
	if (ret < 0) {
		printf("ERROR: Invalid partition block '%s' "
		       "(valid range: 0-%lu)\n", argv[1],
		       (unsigned long)geo.neraseblocks - 1);
		return ret;
	}

	ret = MTD_MARKBAD(mtd, block);
	if (ret < 0) {
		printf("ERROR: Failed to mark partition block %lu bad: %d\n",
		       (unsigned long)block, ret);
		return ret;
	}

	ret = MTD_ISBAD(mtd, block);
	if (ret <= 0) {
		printf("ERROR: Bad-block verification failed for partition "
		       "block %lu: %d\n", (unsigned long)block, ret);
		return ret < 0 ? ret : -EIO;
	}

	printf("Marked LittleFS partition block %lu as bad\n",
	       (unsigned long)block);

	return OK;
}
