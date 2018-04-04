#***********************************************************************
#
#  Copyright (c) 2006  Broadcom Corporation
#  All Rights Reserved
#
# 
# 
# This program is the proprietary software of Broadcom Corporation and/or its 
# licensors, and may only be used, duplicated, modified or distributed pursuant 
# to the terms and conditions of a separate, written license agreement executed 
# between you and Broadcom (an "Authorized License").  Except as set forth in 
# an Authorized License, Broadcom grants no license (express or implied), right 
# to use, or waiver of any kind with respect to the Software, and Broadcom 
# expressly reserves all rights in and to the Software and all intellectual 
# property rights therein.  IF YOU HAVE NO AUTHORIZED LICENSE, THEN YOU HAVE 
# NO RIGHT TO USE THIS SOFTWARE IN ANY WAY, AND SHOULD IMMEDIATELY NOTIFY 
# BROADCOM AND DISCONTINUE ALL USE OF THE SOFTWARE. 
# 
# Except as expressly set forth in the Authorized License, 
# 
# 1. This program, including its structure, sequence and organization, 
#    constitutes the valuable trade secrets of Broadcom, and you shall use 
#    all reasonable efforts to protect the confidentiality thereof, and to 
#    use this information only in connection with your use of Broadcom 
#    integrated circuit products. 
# 
# 2. TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS" 
#    AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES, REPRESENTATIONS OR 
#    WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH 
#    RESPECT TO THE SOFTWARE.  BROADCOM SPECIFICALLY DISCLAIMS ANY AND 
#    ALL IMPLIED WARRANTIES OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, 
#    FITNESS FOR A PARTICULAR PURPOSE, LACK OF VIRUSES, ACCURACY OR 
#    COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR CORRESPONDENCE 
#    TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING OUT OF USE OR 
#    PERFORMANCE OF THE SOFTWARE. 
# 
# 3. TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM OR 
#    ITS LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL, 
#    INDIRECT, OR EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY 
#    WAY RELATING TO YOUR USE OF OR INABILITY TO USE THE SOFTWARE EVEN 
#    IF BROADCOM HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES; 
#    OR (ii) ANY AMOUNT IN EXCESS OF THE AMOUNT ACTUALLY PAID FOR THE 
#    SOFTWARE ITSELF OR U.S. $1, WHICHEVER IS GREATER. THESE LIMITATIONS 
#    SHALL APPLY NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF ANY 
#    LIMITED REMEDY. 
#
#
#***********************************************************************/


#
# In most cases, you only need to modify this first section.
# Just add libraries to LIB_DIRS
#
LIB_DIRS = mdm cms_core language-preconfig language cms_dal cms_cli atmctl xdslctl bcmssl omci mocactl \
           vlanctl gponif ethswctl cmfapi spuctl pwrctl omcipm_drv gponctl \
           nanoxml fcctl mdk212 eponsdk iqctl snoopctl gmacctl

ifneq ($(strip $(BUILD_11ac_throughput_patch_from_412L07)),)
LIB_DIRS += fapctl
endif
#
# We need to declare these directory names as phony targets,
# otherwise make will think the directories themselves are
# the targets and will never execute the cmds.
# See GNU Make, 4.6 
#
.PHONY : $(LIB_DIRS)
all    : $(LIB_DIRS)


#
# Cleaning rules
# See example in GNU Make, 4.6
#
clean: lib_clean

lib_clean:
	for dir in $(LIB_DIRS); do \
		$(MAKE) -C $$dir clean; \
	done


#
# Set our CommEngine directory (by splitting the pwd into two words
# at /userspace and taking the first word only).
# Then include the common defines under CommEngine.
#
CURR_DIR := $(shell pwd)
BUILD_DIR:=$(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR:=$(word 1, $(BUILD_DIR))

include $(BUILD_DIR)/make.common

ifneq ($(strip $(BUILD_VODSL)),)
include $(BUILD_DIR)/make.voice
endif



##############################################################
# 
# Rules for inidividual libs
# libs can be compiled or not depending on the profile (which
# is modified via make menuconfig)
#
##############################################################

# always build this lib
mdm:
	$(MAKE) -C $@


# always build this lib
cms_core:
	$(MAKE) -C $@

# always build this lib
cms_dal:
	$(MAKE) -C $@

# always build this lib
nanoxml:
	$(MAKE) -C $@


# Always build this lib, but if CLI_CMD and CLI_MENU is not defined
# it will be a very small lib (should be a zero length lib)
cms_cli: language
	$(MAKE) -C $@

# __MSTC__, __Telus__, Axel, support Multilingual
ifneq ($(strip $(MSTC_MULTI_LANGUAGES)),)
MULTI_LANGUAGES_PATH=$(BUILD_DIR)/userspace/private/apps/language
language-preconfig:
	$(MAKE) -C language preconfig
language: language-preconfig
	$(MAKE) -C $@ after-preconfig
else
language-preconfig:
	@echo "skipping multi-languages-preconfig (not configured)"
language: language-preconfig
	@echo "skipping multi-languages (not configured)"
endif
# __MSTC__, __Telus__, Axel, support Multilingual end

ifneq ($(strip $(BUILD_XTMCTL)),)
atmctl:
	$(MAKE) -C atmctl $(BUILD_XTMCTL)
else
atmctl:
	@echo "skipping atmctl (not configured)"
endif


ifneq ($(strip $(BUILD_XDSLCTL)),)
xdslctl:
	$(MAKE) -C xdslctl $(BUILD_XDSLCTL)
else
xdslctl:
	@echo "skipping xdslctl (not configured)"
endif


ifneq ($(strip $(BUILD_TR69C_BCM_SSL)),)
bcmssl: sanity_check
	(tar xkjf bcmssl.tar.bz2 2> /dev/null || true)
	$(MAKE) -C bcmssl
else
bcmssl: sanity_check
	@echo "skipping bcmssl (not configured)"
endif

ifneq ($(strip $(BUILD_OMCI)),)
omci: omcipm_drv
	$(MAKE) -C $@
omcipm_drv:
	$(MAKE) -C $@
else
omci:
	@echo "skipping omci (not configured)"
endif

ifneq ($(strip $(BUILD_GPONCTL)),)
gponctl:
	$(MAKE) -C $@
else
gponctl:
	@echo "skipping gponctl (not configured)"
endif

ifneq ($(strip $(BUILD_MoCACTL)),)
mocactl:
	$(MAKE) -C $@
else
mocactl:
	@echo "skipping mocactl (not configured)"
endif

ifneq ($(strip $(BUILD_VLANCTL)),)
vlanctl: 
	$(MAKE) -C $@
else
vlanctl:
	@echo "skipping vlanctl (not configured)"
endif

ifneq ($(strip $(BUILD_GPONIF)),)
gponif: 
	$(MAKE) -C $@
else
gponif:
	@echo "skipping gponif (not configured)"
endif

ifneq ($(strip $(BUILD_ETHSWCTL)),)
ethswctl: 
	$(MAKE) -C $@
else
ethswctl:
	@echo "skipping ethswctl (not configured)"
endif

ifneq ($(strip $(BUILD_CMFAPI)),)
cmfapi: 
	$(MAKE) -C $@
else
cmfapi:
	@echo "skipping cmfapi (not configured)"
endif

ifneq ($(strip $(BUILD_SPUCTL)),)
spuctl:
	$(MAKE) -C spuctl $(BUILD_SPUCTL)
else
spuctl:
	@echo "skipping spuctl (not configured)"
endif

ifneq ($(strip $(BUILD_PWRCTL)),)
pwrctl:
	$(MAKE) -C pwrctl $(BUILD_PWRCTL)
else
pwrctl:
	@echo "skipping pwrctl (not configured)"
endif

ifneq ($(strip $(BUILD_FCCTL)),)
fcctl: 
	$(MAKE) -C $@
else
fcctl:
	@echo "skipping fcctl (not configured)"
endif

ifneq ($(strip $(BUILD_EPON_SDK)),)
eponsdk: mdk212
	$(MAKE) -C $@
else
eponsdk:
	@echo "Skipping epon sdk "
endif

# Switch MDK
mdk212: sanity_check
ifneq ($(strip $(BUILD_SWMDK)),)
	if [ ! -e mdk212/RELEASE ]; then \
         echo untaring original mdk; \
	  (tar xkfj mdk212/$(ORIGINAL_MDK) 2> /dev/null || true); \
	fi
	$(MAKE) -j 1 -C $@
else
	@echo "Skipping switch mdk "
endif

# Ingress QoS
ifneq ($(strip $(BUILD_IQCTL)),)
iqctl: 
	$(MAKE) -C $@
else
iqctl:
	@echo "skipping iqctl (not configured)"
endif
# GMAC
ifneq ($(strip $(BUILD_GMACCTL)),)
gmacctl: 
	$(MAKE) -C $@
else
gmacctl:
	@echo "skipping gmacctl (not configured)"
endif


# snoopctl Library
ifneq ($(strip $(BUILD_MCAST_SNOOP)),)
snoopctl: 
	echo Doing Make
	$(MAKE) -C snoopctl
else
snoopctl:
	@echo "skipping snoopctl (not configured)"
endif
ifneq ($(strip $(BUILD_11ac_throughput_patch_from_412L07)),)
# GMAC
ifneq ($(strip $(BUILD_GMACCTL)),)
gmacctl: 
	$(MAKE) -C $@
else
gmacctl:
	@echo "skipping gmacctl (not configured)"
endif

# FAP
ifneq ($(strip $(BUILD_FAPCTL)),)
fapctl: 
	$(MAKE) -C $@
else
fapctl:
	@echo "skipping fapctl (not configured)"
endif
endif




