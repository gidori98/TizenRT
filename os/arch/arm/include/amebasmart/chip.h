/****************************************************************************
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

#ifndef __ARCH_ARM_INCLUDE_AMEBASMART_CHIP_H
#define __ARCH_ARM_INCLUDE_AMEBASMART_CHIP_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <tinyara/config.h>

/****************************************************************************
 * Pre-processor Prototypes
 ****************************************************************************/
 #ifdef CONFIG_RAM_DDR
#  define AMEBASMART_DRAM_SIZE   (0x3F00000)   /* Size of the DDR for BL33 (63MB, 63*1024*1024 = 66060288) */
#else
#  define AMEBASMART_DRAM_SIZE   (0x700000)   /* Size of the PSRAM for BL33 (7MB, 7*1024*1024 = 7340032) */
#endif
#  define AMEBASMART_NXCPUS       2            /* Two CPUs */
/* L2 page table yet to be verified in TizenRT */
// #  define AMEBASMART_L2CACHE_SIZE (256*1024)  /* 256Kb L2 Cache */
/****************************************************************************
 * Public Types
 ****************************************************************************/

/****************************************************************************
 * Public Data
 ****************************************************************************/

/****************************************************************************
 * Public Functions Prototypes
 ****************************************************************************/

#endif							/* __ARCH_ARM_INCLUDE_AMEBASMART_CHIP_H */
