/****************************************************************************
 *
 * Copyright 2023 Samsung Electronics All Rights Reserved.
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
 * Included Files
 ****************************************************************************/

#include <tinyara/config.h>
#include <stdlib.h>
#include <debug.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <queue.h>
#include <sys/types.h>
#include <tinyara/mm/mm.h>
#include <tinyara/mm/heap_regioninfo.h>
#include <arch/chip/memory_region.h>
#include <tinyara/binfmt/elf.h>

#include "binary_manager/binary_manager_internal.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/
#define MEM_USED   0
#define MEM_LEAK   1
#define MEM_BROKEN 2
#define CMN_BIN_IDX 0

#define MEM_ACCESS_UNIT    0x04
#define MAX_ALLOC_COUNT    CONFIG_MEM_LEAK_CHECKER_MAX_ALLOC_COUNT
#define HASH_SIZE          CONFIG_MEM_LEAK_CHECKER_HASH_TABLE_SIZE

#define MM_PREV_NODE_SIZE(x)            ((x)->preceding & ~MM_ALLOC_BIT)

struct alloc_node_info_s {
	volatile struct mm_allocnode_s *node;
	struct alloc_node_info_s *next;
};

static struct alloc_node_info_s **g_hash_table;
static struct alloc_node_info_s *g_node_info;

static int hash_init(void)
{
	int index;

	g_hash_table = (struct alloc_node_info_s **)malloc(sizeof(struct alloc_node_info_s *) * HASH_SIZE);
	if (!g_hash_table) {
		return ERROR;
	}

	g_node_info = (struct alloc_node_info_s*)malloc(sizeof(struct alloc_node_info_s) * MAX_ALLOC_COUNT);
	if (!g_node_info) {
		free(g_hash_table);
		return ERROR;
	}

	for (index = 0; index < HASH_SIZE; ++index) {
		g_hash_table[index] = NULL;
	}

	return OK;
}

static void hash_deinit(void)
{
	free(g_hash_table);
	free(g_node_info);
	g_hash_table = NULL;
	g_node_info = NULL;
}

static void add_hash(int index)
{
	long key;
	struct alloc_node_info_s *cur;

	key = (long)g_node_info[index].node % HASH_SIZE;
	if (g_hash_table[key] == NULL) {
		g_hash_table[key] = &g_node_info[index];
		return;
	}

	cur = g_hash_table[key];
	while (cur->next) {
		cur = cur->next;
	}
	cur->next = &g_node_info[index];
}

static bool search_hash(unsigned long value)
{
	long key = value % HASH_SIZE;
	struct alloc_node_info_s *cur = g_hash_table[key];

	while (cur != NULL) {
		if ((unsigned long)cur->node == value) {
			if (cur->node->reserved == MEM_USED) {
				return false;
			}
			cur->node->reserved = MEM_USED;
			return true;
		}
		if (cur->next == NULL) {
			return false;
		}
		cur = cur->next;
	}
	return false;
}

static int get_node_cnt(struct mm_heap_s *heap)
{
	volatile struct mm_allocnode_s *node;
	mmsize_t node_size;
	node_size = SIZEOF_MM_ALLOCNODE;

	int ret = 0;
	
#if CONFIG_KMM_REGIONS > 1
	int region;
#else
#define region 0
#endif

	/* Visit each region */

#if CONFIG_KMM_REGIONS > 1
	for (region = 0; region < heap->mm_nregions; region++)
#endif
	{
		node_size = SIZEOF_MM_ALLOCNODE;
		for (node = heap->mm_heapstart[region]; node < heap->mm_heapend[region]; node = (struct mm_allocnode_s *)((char *)node + node->size)) {
			ASSERT(node->size);
			/* Ignore the heap start checking, because there is a guard node in heap start */
			if (node == heap->mm_heapstart[region]) {
				continue;
			}
			/* Check broken link */
			if (node_size != MM_PREV_NODE_SIZE(node)) {
				continue;
			}
			node_size = node->size;
			/* Check if the node corresponds to an allocated memory chunk */
			if ((node->preceding & MM_ALLOC_BIT) != 0) {
				ret++;
			}
		}
	}

	return ret;
}

static void fill_hash_table(struct mm_heap_s *heap, int *leak_cnt, int *broken_cnt)
{
	volatile struct mm_allocnode_s *node;
	mmsize_t node_size;
	node_size = SIZEOF_MM_ALLOCNODE;

	mm_takesemaphore(heap);

#if CONFIG_KMM_REGIONS > 1
	int region;
#else
#define region 0
#endif

	/* Visit each region */

#if CONFIG_KMM_REGIONS > 1
	for (region = 0; region < heap->mm_nregions; region++)
#endif
	{
		node_size = SIZEOF_MM_ALLOCNODE;
		for (node = heap->mm_heapstart[region]; node < heap->mm_heapend[region]; node = (struct mm_allocnode_s *)((char *)node + node->size)) {
			ASSERT(node->size);
			/* Ignore the heap start checking, because there is a guard node in heap start */
			if (node == heap->mm_heapstart[region]) {
				continue;
			}

			/* Check broken link */
			if (node_size != MM_PREV_NODE_SIZE(node)) {
				node->reserved = MEM_BROKEN;
				(*broken_cnt)++;
				continue;
			}
			node_size = node->size;
			if ((unsigned long)node + (unsigned long)SIZEOF_MM_ALLOCNODE == (unsigned long)g_node_info || 
					(unsigned long)node + (unsigned long)SIZEOF_MM_ALLOCNODE == (unsigned long)g_hash_table) {
				continue;
			}
			/* Check if the node corresponds to an allocated memory chunk */
			if ((node->preceding & MM_ALLOC_BIT) != 0) {
				g_node_info[*leak_cnt].node = node;
				g_node_info[*leak_cnt].next = NULL;
				node->reserved = MEM_LEAK;
				add_hash(*leak_cnt);
				(*leak_cnt)++;
			}
		}
	}
	mm_givesemaphore(heap);
}

static void search_addr(void *start_addr, void *end_addr, int *leak_cnt)
{
	/* This function traverse the memory from start_addr to end_addr for comparing the address based on hash table. */
	void *leak_chk;

	/* Not to access over its region, subtract 0x04 from the end of the address. */
	for (leak_chk = start_addr; leak_chk < end_addr - MEM_ACCESS_UNIT; leak_chk++) {
		if (search_hash(*(unsigned long volatile *)leak_chk - (unsigned long)SIZEOF_MM_ALLOCNODE)) {
			(*leak_cnt)--;
		}
	}
}

static void heap_check(struct mm_heap_s *heap, int checker_pid, int *leak_cnt)
{
	void *leak_chk;
	struct mm_allocnode_s *visit_node;
	void *exclude_top;
	void *exclude_bottom;

	struct tcb_s *ctcb = sched_gettcb(checker_pid);
	ASSERT(ctcb != NULL);
	exclude_top = ctcb->adj_stack_ptr;
	exclude_bottom = ctcb->adj_stack_ptr - ctcb->adj_stack_size;

#if CONFIG_KMM_REGIONS > 1
	int region;
#else
#define region 0
#endif

	/* Visit each region */

#if CONFIG_KMM_REGIONS > 1
	for (region = 0; region < heap->mm_nregions; region++)
#endif
	{
		for (visit_node = heap->mm_heapstart[region]; visit_node < heap->mm_heapend[region]; visit_node = (struct mm_allocnode_s *)((char *)visit_node + visit_node->size)) {
			if ((visit_node->preceding & MM_ALLOC_BIT) != 0) {
				if ((void *)((char *)visit_node + SIZEOF_MM_ALLOCNODE) == (void *)g_node_info) {
					continue;
				}
				for (leak_chk = (void *)visit_node; leak_chk < (void *)(((char *)visit_node) + visit_node->size); leak_chk++) {
					if ((leak_chk >= exclude_bottom && leak_chk <= exclude_top)) {
						continue;
					}
					if (search_hash(*(unsigned long volatile *)leak_chk - (unsigned long)SIZEOF_MM_ALLOCNODE)) {
						(*leak_cnt)--;
					}
				}
			}
		}
	}
}

static struct mm_heap_s * init_mem_leak_checker(int checker_pid, char *bin_name);

static void ram_check(struct mm_heap_s *heap, int checker_pid, char *bin_name, int *leak_cnt)
{
	
#ifdef CONFIG_APP_BINARY_SEPARATION
	bin_addr_info_t *info;
	struct mm_heap_s *kheap;
	int bin_idx;

	info = (bin_addr_info_t *)get_bin_addr_list();
	for (bin_idx = 0; bin_idx <= CONFIG_NUM_APPS; bin_idx++) {
		if (strncmp(BIN_NAME(bin_idx), bin_name, strlen(bin_name)) == 0) {
			break;
		}
	}
#endif
	/* Visit all the data regions
	 */
	int mem_region_idx;
	for (mem_region_idx = 0; mem_region_idx < MEM_VAR_REGION_COUNT; mem_region_idx++) {
		search_addr(variable_region_start_addr[mem_region_idx], variable_region_end_addr[mem_region_idx], leak_cnt);
	}

#ifdef CONFIG_APP_BINARY_SEPARATION
	if (strncmp(bin_name, "kernel", strlen("kernel") + 1) == 0) {
		/* do nothing */
	} else {
#ifdef CONFIG_SUPPORT_COMMON_BINARY
		search_addr((void *)info[CMN_BIN_IDX].data_addr, (void *)(info[CMN_BIN_IDX].data_addr + info[CMN_BIN_IDX].data_size), leak_cnt);
		search_addr((void *)info[CMN_BIN_IDX].bss_addr, (void *)(info[CMN_BIN_IDX].bss_addr + info[CMN_BIN_IDX].bss_size), leak_cnt);
#endif
		/* search the bss and data region of the loadable app */
		search_addr((void *)info[bin_idx].data_addr, (void *)(info[bin_idx].data_addr + info[bin_idx].data_size), leak_cnt);
		search_addr((void *)info[bin_idx].bss_addr, (void *)(info[bin_idx].bss_addr + info[bin_idx].bss_size), leak_cnt);
		/* search the kernel heap first */
		kheap = kmm_get_baseheap();
		heap_check(kheap, checker_pid, leak_cnt);
	}
#endif

	/* Visit heap region */
	heap_check(heap, checker_pid, leak_cnt);
}

static void print_info(struct mm_heap_s *heap, int leak_cnt, int broken_cnt)
{
	volatile struct mm_allocnode_s *node;
	uint32_t owner_addr;	

	if (leak_cnt > 0 || broken_cnt > 0) {
		printf("Type   |    Addr    | Size(byte) |    Owner   | PID \n");
		printf("---------------------------------------------------\n");

		mm_takesemaphore(heap);

#if CONFIG_KMM_REGIONS > 1
		int region;
#else
#define region 0
#endif

		/* Visit each region */

#if CONFIG_KMM_REGIONS > 1
		for (region = 0; region < heap->mm_nregions; region++)
#endif
		{
			for (node = heap->mm_heapstart[region]; node <  heap->mm_heapend[region]; node = (struct mm_allocnode_s *)((char *)node + node->size)) {
				ASSERT(node->size);
				if (node->reserved == MEM_LEAK) {
					/* alloc_call_addr can be from kernel, app or common binary.
					* based on the text addresses printed, user needs to check the
					* corresponding binaries accordingly
					*/
					owner_addr = (uint32_t)node->alloc_call_addr;
					pid_t pid = node->pid;
					if (pid < 0) {
						/* For stack allocated node, pid is negative value.
						* To use the pid, change it to original positive value.
						*/
						pid = (-1) * pid;
					}
					printf("LEAK   | %10p |  %8d  | %10p | %d\n", (void *)((char *)node + SIZEOF_MM_ALLOCNODE), node->size - SIZEOF_MM_ALLOCNODE, owner_addr, pid);
				} else if (node->reserved == MEM_BROKEN) {
					printf("BROKEN | %p\n", node);
				}
			}
		}

		mm_givesemaphore(heap);

		printf("*** %d LEAKS, %d BROKENS.\n", leak_cnt, broken_cnt);
	} else {
		printf("*** NO MEMORY LEAK.\n");
	}
}

int run_mem_leak_checker(int checker_pid, char *bin_name)
{
	int leak_cnt = 0;
	int node_cnt = 0;
	int broken_cnt = 0;
	struct mm_heap_s *heap = NULL;

	if (strncmp(bin_name, "kernel", strlen("kernel") + 1) == 0) {
		heap = kmm_get_baseheap();
	} 
#ifdef CONFIG_APP_BINARY_SEPARATION
	else {
		heap = mm_get_app_heap_with_name(bin_name);
	}
#endif

	if (!heap) {
		printf("Can't found heap, bin name: %s", bin_name);
		return ERROR;
	}

	node_cnt = get_node_cnt(heap);
	if (MAX_ALLOC_COUNT < node_cnt) {
		printf("Available buffer size (%d) is small.\nPlease increase CONFIG_MEM_LEAK_CHECKER_MAX_ALLOC_COUNT value more than %d.\n", MAX_ALLOC_COUNT, node_cnt);
		return ERROR;
	}

	if (g_hash_table || g_node_info) {
		printf("mem_leak_checker is already running.\n");
		return ERROR;
	}

	if (hash_init() != OK) {
		printf("hash table memory alloc is failed.\n");
		return ERROR;
	}

	fill_hash_table(heap, &leak_cnt, &broken_cnt);

	/* Visit RAM region */
	ram_check(heap, checker_pid, bin_name, &leak_cnt);

	print_info(heap, leak_cnt, broken_cnt);

	hash_deinit();
	return OK;
}

int run_all_mem_leak_checker(int checker_pid)
{
	int ret;
	printf("\nKernel :\n");
	ret = run_mem_leak_checker(checker_pid, "kernel");

	if (ret != OK) {
		return ERROR;
	}

#ifdef CONFIG_APP_BINARY_SEPARATION
	printf("\nBelow are text addresses of loadable apps (and common binary if enabled) :\n");
	printf("The pc value of the allocation can be obtained by subtracting the text start address of the appropriate binary\n\n");
	bin_addr_info_t *bin_addr_info = (bin_addr_info_t *)get_bin_addr_list();
	int bin_idx;
	for (bin_idx = 0; bin_idx <= CONFIG_NUM_APPS; bin_idx++) {
		if (bin_addr_info[bin_idx].text_addr != 0) {
			printf("[%s] Text Addr : %p, Text Size : %u\n", BIN_NAME(bin_idx), bin_addr_info[bin_idx].text_addr, bin_addr_info[bin_idx].text_size);
		}
	}
	printf("\n");
	/* bin_idx value zero is always reserved for common binary, so
	 * skip checking common binary and start checking from index one
	 */
	for (bin_idx = 1; bin_idx <= CONFIG_NUM_APPS; bin_idx++) {
		if (bin_addr_info[bin_idx].text_addr != 0) {
			printf("%s :\n", BIN_NAME(bin_idx));
			ret = run_mem_leak_checker(checker_pid, BIN_NAME(bin_idx));
			if (ret != OK) {
				return ERROR;
			}
		}
	}
#endif
	return OK;
}
