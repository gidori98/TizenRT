###########################################################################
#
# Copyright 2016 Samsung Electronics All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
# either express or implied. See the License for the specific
# language governing permissions and limitations under the License.
#
###########################################################################
############################################################################
# apps/system/utils/Makefile
#
#   Copyright (C) 2011 Uros Platise. All rights reserved.
#   Copyright (C) 2012 Gregory Nutt. All rights reserved.
#   Author: Uros Platise <uros.platise@isotel.eu>
#           Gregory Nutt <gnutt@nuttx.org>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in
#    the documentation and/or other materials provided with the
#    distribution.
# 3. Neither the name NuttX nor the names of its contributors may be
#    used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
# FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
# COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
# OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
# AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
# ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#
############################################################################

# TODO, this makefile should run make under the app dirs, instead of
# sourcing the Make.defs!

-include $(TOPDIR)/.config
-include $(TOPDIR)/Make.defs
include $(APPDIR)/Make.defs

ifeq ($(WINTOOL),y)
INCDIROPT = -w
endif

ASRCS =
CSRCS =
MAINSRC =

ifneq ($(CONFIG_DISABLE_ENVIRON),y)
CSRCS += fs_getpath.c
endif

ifeq ($(CONFIG_SYSTEM_CMDS),y)
CSRCS += systemcmd.c
endif

ifeq ($(CONFIG_SECURITY_LEVEL_CMDS),y)
CSRCS += security_level_cmd.c
endif

ifeq ($(CONFIG_FS_CMDS),y)
CSRCS += fscmd.c
endif

ifeq ($(CONFIG_NET_CMDS),y)
CSRCS += netcmd.c
ifeq ($(CONFIG_NET_PING_CMD),y)
CSRCS += netcmd_ping.c
endif
ifeq ($(CONFIG_NET_NETMON),y)
CSRCS += netcmd_netmon.c
endif
endif

ifeq ($(CONFIG_NETUTILS_DHCPD),y)
CSRCS += netcmd_dhcpd.c
#CFLAGS += -I$(EXTDIR)/dhcpd/
CFLAGS += -I../../../external/dhcpd/
endif

ifeq ($(CONFIG_NETUTILS_TFTPC),y)
CSRCS += netcmd_tftpc.c
endif

ifeq ($(CONFIG_ENABLE_CPULOAD),y)
CSRCS += utils_cpuload.c
endif

ifeq ($(CONFIG_ENABLE_DATE),y)
CSRCS += utils_date.c
endif

ifeq ($(CONFIG_ENABLE_DMESG),y)
CSRCS += utils_dmesg.c
endif

ifeq ($(CONFIG_ENABLE_ENV_GET),y)
CSRCS += utils_env.c
else ifeq ($(CONFIG_ENABLE_ENV_SET),y)
CSRCS += utils_env.c
else ifeq ($(CONFIG_ENABLE_ENV_UNSET),y)
CSRCS += utils_env.c
endif

ifeq ($(CONFIG_ENABLE_FREE),y)
CSRCS += utils_free.c
endif

ifeq ($(CONFIG_ENABLE_HEAPINFO),y)
CSRCS += utils_heapinfo.c
endif

ifeq ($(CONFIG_ENABLE_PRODCONFIG),y)
CSRCS += utils_prodconfig.c
endif

ifeq ($(CONFIG_ENABLE_IRQINFO),y)
CSRCS += utils_irqinfo.c
endif

ifeq ($(CONFIG_ENABLE_KILL),y)
CSRCS += utils_kill.c
else ifeq ($(CONFIG_ENABLE_KILLALL),y)
CSRCS += utils_kill.c
endif

ifeq ($(CONFIG_ENABLE_PS),y)
CSRCS += utils_ps.c
endif

ifeq ($(CONFIG_ENABLE_STACKMONITOR),y)
CSRCS += utils_stackmonitor.c
endif

ifeq ($(CONFIG_ENABLE_UPTIME),y)
CSRCS += utils_uptime.c
endif

ifeq ($(CONFIG_ENABLE_TZSELECT),y)
CSRCS += utils_tzselect.c
endif

ifeq ($(CONFIG_ENABLE_CPULOAD),y)
CSRCS += utils_proc.c
else ifeq ($(CONFIG_ENABLE_PS),y)
CSRCS += utils_proc.c
else ifeq ($(CONFIG_ENABLE_STACKMONITOR),y)
CSRCS += utils_proc.c
else ifeq ($(CONFIG_ENABLE_HEAPINFO),y)
CSRCS += utils_proc.c
else ifeq ($(CONFIG_ENABLE_KILLALL),y)
CSRCS += utils_proc.c
endif

AOBJS = $(ASRCS:.S=$(OBJEXT))
COBJS = $(CSRCS:.c=$(OBJEXT))
MAINOBJ = $(MAINSRC:.c=$(OBJEXT))

SRCS = $(ASRCS) $(CSRCS) $(MAINSRC)
OBJS = $(AOBJS) $(COBJS)

ifneq ($(CONFIG_BUILD_KERNEL),y)
  OBJS += $(MAINOBJ)
endif

ifeq ($(CONFIG_WINDOWS_NATIVE),y)
  BIN = ..\..\libapps$(LIBEXT)
else
ifeq ($(WINTOOL),y)
  BIN = ..\\..\\libapps$(LIBEXT)
else
  BIN = ../../libapps$(LIBEXT)
endif
endif

ifeq ($(WINTOOL),y)
  INSTALL_DIR = "${shell cygpath -w $(BIN_DIR)}"
else
  INSTALL_DIR = $(BIN_DIR)
endif

CONFIG_XYZ_PROGNAME ?= init$(EXEEXT)
PROGNAME = $(CONFIG_XYZ_PROGNAME)

ROOTDEPPATH = --dep-path .

# Common build

VPATH =

all: .built
.PHONY: context depend clean distclean

$(AOBJS): %$(OBJEXT): %.S
	$(call ASSEMBLE, $<, $@)

$(COBJS) $(MAINOBJ): %$(OBJEXT): %.c
	$(call COMPILE, $<, $@)

.built: $(OBJS)
	$(call ARCHIVE, $(BIN), $(OBJS))
	$(Q) touch .built

ifeq ($(CONFIG_BUILD_KERNEL),y)
$(BIN_DIR)$(DELIM)$(PROGNAME): $(OBJS) $(MAINOBJ)
	@echo "LD: $(PROGNAME)"
	$(Q) $(LD) $(LDELFFLAGS) $(LDLIBPATH) -o $(INSTALL_DIR)$(DELIM)$(PROGNAME) $(ARCHCRT0OBJ) $(MAINOBJ) $(LDLIBS)
	$(Q) $(NM) -u  $(INSTALL_DIR)$(DELIM)$(PROGNAME)

install: $(BIN_DIR)$(DELIM)$(PROGNAME)

else
install:

endif

context:

# Create dependencies

.depend: Makefile $(SRCS)
	$(Q) $(MKDEP) $(ROOTDEPPATH) "$(CC)" -- $(CFLAGS) -- $(SRCS) >Make.dep
	$(Q) touch $@

depend: .depend

clean:
	$(call DELFILE, .built)
	$(call CLEAN)

distclean: clean
	$(call DELFILE, Make.dep)
	$(call DELFILE, .depend)

-include Make.dep
.PHONY: preconfig
preconfig:
