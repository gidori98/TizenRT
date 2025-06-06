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
 * pm/pm_initialize.c
 *
 *   Copyright (C) 2011-2012 Gregory Nutt. All rights reserved.
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
#include <stdlib.h>
#include <semaphore.h>
#include <debug.h>
#include <tinyara/pm/pm.h>
#include <time.h>
#include "pm.h"
#ifdef CONFIG_PM

/****************************************************************************
 * Public Data
 ****************************************************************************/

/* All PM global data: */

/* Initialize the registry and the PM global data structures.  The PM
 * global data structure resides in .data which is zeroed at boot time.  So
 * it is only required to initialize non-zero elements of the PM global
 * data structure here.
 */

struct pm_global_s g_pmglobals;
const char *pm_state_name[PM_COUNT] = {"NORMAL", "IDLE", "STANDBY", "SLEEP"};
const char *wakeup_src_name[PM_WAKEUP_SRC_COUNT] = {"UNKNOWN", "BLE", "WIFI", "UART CONSOLE", "UART TTYS2", "GPIO", "HW TIMER"};

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: pm_start
 *
 * Description:
 *   This function is called by the application thread to start the Power
 *   Management system. This fucntion sets the is_running flag which
 *   enables pm to transition between low and high power states.
 *
 * Input parameters:
 *   None.
 *
 * Returned value:
 *    None.
 *
 ****************************************************************************/

void pm_start(void) {
	g_pmglobals.is_running = true;
}

/****************************************************************************
 * Name: pm_initialize
 *
 * Description:
 *   This function is called by MCU-specific one-time at power on reset in
 *   order to initialize the power management capabilities.  This function
 *   must be called *very* early in the initializeation sequence *before* any
 *   other device drivers are initialize (since they may attempt to register
 *   with the power management subsystem). It also fills the PM ops with the
 *   required BSP APIs.
 *
 * Input parameters:
 *   sleep_ops: pm power gating operations to use.
 *
 * Returned value:
 *    None.
 *
 ****************************************************************************/
void pm_initialize(struct pm_sleep_ops *sleep_ops)
{
	sem_init(&g_pmglobals.regsem, 0, 1);

	/* Register the PM ops structures */
	g_pmglobals.sleep_ops = sleep_ops;

	/* Register Special Domains, which are specific to Kernel*/
	DEBUGASSERT(pm_domain_register("IDLE") == PM_IDLE_DOMAIN);
	DEBUGASSERT(pm_domain_register("SCREEN") == PM_LCD_DOMAIN);
}

/****************************************************************************
 * Name: pm_clock_initialize
 *
 * Description:
 *   This function is called by MCU-specific one-time at power on reset in
 *   order to initialize the pm clock capabilityes.  This function
 *   must be called *very* early in the initializeation sequence *before* any
 *   other device drivers are initialize (since they may attempt to register
 *   with the power management subsystem). It also fills the PM ops with the
 *   required BSP APIs.
 *
 * Input parameters:
 *   dvfs_ops: pm power gating operations to use.
 *
 * Returned value:
 *    None.
 *
 ****************************************************************************/
#ifdef CONFIG_PM_DVFS
void pm_clock_initialize(struct pm_clock_ops *dvfs_ops)
{
	g_pmglobals.dvfs_ops = dvfs_ops;
}
#endif

#endif /* CONFIG_PM */
