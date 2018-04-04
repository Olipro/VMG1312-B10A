#***********************************************************************
#
#  Copyright (c) 2007  Broadcom Corporation
#  All Rights Reserved
#
# 
# 
# This program is free software; you can redistribute it and/or modify 
# it under the terms of the GNU General Public License, version 2, as published by  
# the Free Software Foundation (the "GPL"). 
# 
#
# 
# This program is distributed in the hope that it will be useful,  
# but WITHOUT ANY WARRANTY; without even the implied warranty of  
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the  
# GNU General Public License for more details. 
#  
# 
#  
#   
# 
# A copy of the GPL is available at http://www.broadcom.com/licenses/GPLv2.php, or by 
# writing to the Free Software Foundation, Inc., 59 Temple Place - Suite 330, 
# Boston, MA 02111-1307, USA. 
#
#
#***********************************************************************/


#
# In most cases, you only need to modify this first section.
#
LIB  = libcms_util.so

OBJS := eid.o logging.o memory.o bget.o psp.o timestamp.o timer.o \
       strconv.o prctl.o \
       assert.o image.o network.o file.o base64decode.o base64encode.o \
       lzw_encode.o lzw_decode.o crc.o hexbinary.o xml.o led.o passwd.o \
       ftrace.o linklist.o random.o

all: sanity_check generic_public_lib_install

clean: generic_oalclean generic_modsw_contrib_libs_clean generic_clean 
	rm -f $(INSTALL_DIR)/lib/public/$(LIB)

modsw_clean: generic_modsw_base_clean


#
# Set our CommEngine directory (by splitting the pwd into two words
# at /userspace and taking the first word only).
# Then include the common defines under CommEngine.
# You do not need to modify this part.
#
CURR_DIR := $(shell pwd)
BUILD_DIR:=$(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR:=$(word 1, $(BUILD_DIR))

# automatically set BCM_BASE_BUILD_DIR so that the modsw dirs can compile
BCM_BASE_BUILD_DIR := $(BUILD_DIR)
export BCM_BASE_BUILD_DIR


include $(BUILD_DIR)/make.common

ifneq ($(strip $(BUILD_VODSL)),)
include $(BUILD_DIR)/make.voice
endif


#
# Public libs are only allowed to include header files from the
# public directory.
#
# WARNING: Do not modify this section unless you understand the
# license implications of what you are doing.
#
ALLOWED_INCLUDE_PATHS := -I.\
                         -I$(BUILD_DIR)/userspace/public/include  \
                         -I$(BUILD_DIR)/userspace/public/include/$(OALDIR)


# psp.c needs to do direct ioctl to the board to write to flash
ALLOWED_INCLUDE_PATHS += -I$(INC_BRCMSHARED_PUB_PATH)/$(BRCM_BOARD)  \
                         -I$(INC_BRCMDRIVER_PUB_PATH)/$(BRCM_BOARD)  \
                         -I$(INC_BRCMDRIVER_PRIV_PATH)/$(BRCM_BOARD) 


# treat all warnings as errors
# __MSTC__, Lynn
# CUSTOM_CFLAGS += -Werror -Wfatal-errors


#
# Implicit rule will make the .c into a .o
# Implicit rule is $(CC) -c $(CPPFLAGS) $(CFLAGS)
# See Section 10.2 of Gnu Make manual
# 
$(LIB): generic_oaldir generic_modsw_contrib_libs_make $(OBJS)
	$(CC) -shared -Wl,--whole-archive,-soname,$@ -o $@ \
	      $(OBJS) $(OALDIR)/oal.a $(MODSW_CONTRIB_LIBS) \
	      -Wl,-lcrypt -Wl,--no-whole-archive



#
# Include the rule for making dependency files.
# The '-' in front of the second include suppresses
# error messages when make cannot find the .d files.
# It will just regenerate them.
# See Section 4.14 of Gnu Make.
#

include $(BUILD_DIR)/make.deprules

-include $(OBJS:.o=.d)
