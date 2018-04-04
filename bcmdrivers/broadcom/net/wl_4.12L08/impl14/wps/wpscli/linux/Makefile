#
# Copyright (C) 2009 Broadcom Corporation
#
# Linux Makefile to build wpscli library
#
# $Id: Makefile 291775 2011-10-25 07:57:12Z $


SRCBASE    := ../../..
BLDTYPE   ?= debug
#BLDTYPE    ?= release
CC         ?= gcc
TARGETARCH ?= x86
OBJDIR     ?= obj/$(TARGETARCH)-$(BLDTYPE)

ifeq ($(CC), arm-linux-gcc)
CFLAGS += -mstructure-size-boundary=8
endif

ifeq ($(CC), mipsel-uclibc-gcc)
endif

ifeq ($(CC), gcc)
  CFLAGS += -m32
endif

#flag to use external openssl library.
EXTERNAL_OPENSSL ?= 0
#flag to create dynamic library
SHARED_LIB ?= 0

ifeq ($(BLDTYPE),debug)
  CFLAGS += -Wall -Wnested-externs -g -D_TUDEBUGTRACE -DDEBUG -D_REENTRANT
  # Mark mips compiler to produce debugging information that is understood
  # by gdb
  ifneq ($(findstring mips,$(CC)),)
    CFLAGS += -gstabs+
  endif
else
  CFLAGS += -Wall -Os -Wnested-externs -DWPS_WIRELESS_ENROLLEE
endif

ifeq ($(SHARED_LIB), 1)
  CFLAGS += -fPIC
endif

ifeq ($(NO_WPS_THREAD),1)
CFLAGS += -DWPSCLI_NO_WPS_LOOP
endif


WPSCLIBASEDIR:= $(SRCBASE)/wps/wpscli
WPSLIBDIR    := $(WPSCLIBASEDIR)/linux
WPSCOMDIR    := $(SRCBASE)/wps/common
CRYPTDIR     := $(SRCBASE)/bcmcrypto
WPSCLICOMMDIR:= ../common
WPSCOMMON    := wpscommon
WPSCLIOSL    := wpscliosl
WPSCLIAPI    := wpscliapi
BCMCRYPTO    := bcmcrypto
WPSTESTCMD   := wpstestcmd

INCLUDE :=  -I$(SRCBASE)/include \
		-I$(WPSCLIBASEDIR)/include \
		-I$(WPSCOMDIR)/include \
		-I../common/include \
		-I../include \
		-I$(SRCBASE)/include/proto


ifeq ($(EXTERNAL_OPENSSL), 1)
EXTERNAL_OPENSSL_BASE ?= /data/openssl/mipsel-uclibc_stage-install/usr/local/
export EXTERNAL_OPENSSL_INC = $(EXTERNAL_OPENSSL_BASE)/include
export EXTERNAL_OPENSSL_LIB = $(EXTERNAL_OPENSSL_BASE)/lib

INCLUDE += -DEXTERNAL_OPENSSL  \
	-I$(EXTERNAL_OPENSSL_INC) -I$(EXTERNAL_OPENSSL_INC)/openssl
else
INCLUDE += -I$(SRCBASE)/include/bcmcrypto
endif

ifneq ($(SHARED_LIB), 1)
ifeq ($(EXTERNAL_OPENSSL), 1)
SAMPLEAPPLIBS := libwpscli.a libwpscore.a libwpscliosl.a
SAMPLEAPPLIBS := $(SAMPLEAPPLIBS:%=$(OBJDIR)/%) $(EXTERNAL_OPENSSL_LIB)/libcrypto.a
else
SAMPLEAPPLIBS := libwpscli.a libbcmcrypto.a libwpscore.a libwpscliosl.a
#SAMPLEAPPLIBS = libwpscli.a
#SAMPLEAPPLIBS = libfoo.a
SAMPLEAPPLIBS := $(SAMPLEAPPLIBS:%=$(OBJDIR)/%)
endif
else #SHARED_LIB
SAMPLEAPPLIBS = -L$(OBJDIR) -lwpscli -lwpscliosl -lwpscore 
ifeq ($(EXTERNAL_OPENSSL),1)
SAMPLEAPPLIBS += -L$(EXTERNAL_OPENSSL_LIB) -lcrypto
else
SAMPLEAPPLIBS += -lbcmcrypto
endif
SAMPLEAPPLIBS += $(EXTERNAL_LIBS)
endif #SHARED_LIB

vpath %.c $(WPSCLICOMMDIR) \
	$(WPSLIBDIR) \
	$(CRYPTDIR) \
	$(WPSCOMDIR)/shared \
	$(WPSCOMDIR)/ap \
	$(WPSCOMDIR)/sta \
	$(WPSCOMDIR)/enrollee \
	$(WPSCOMDIR)/registrar \
	$(WPSTESTCMDSRCS)

# shared files
WPSCORESRCS  = buffobj.c reg_proto_msg.c reg_proto_utils.c wps_sslist.c
WPSCORESRCS += state_machine.c tlv.c tutrace.c ie_utils.c wps_utils.c
# ap files
WPSCORESRCS += ap_api.c ap_eap_sm.c ap_upnp_sm.c dev_config.c
# sta files
WPSCORESRCS += sta_eap_sm.c
# enrollee files
WPSCORESRCS +=enr_api.c enr_reg_sm.c
# registrar files
WPSCORESRCS += reg_sm.c

# openssl files
ifeq ($(EXTERNAL_OPENSSL),1)
WPSCORESRCS += wps_openssl.c
endif

CRYPTSRCS    = aes.c rijndael-alg-fst.c dh.c bn.c sha256.c hmac_sha256.c
CRYPTSRCS   += random.c

OSLSRCS      = wpscli_osl_helper.c wpscli_osl_timer.c wpscli_pktdisp.c 
OSLSRCS     += wpscli_wl_handler.c wpscli_wlan.c

OSISRCS      = wpscli_api.c wpscli_softap.c wpscli_sta.c wpscli_wl.c
OSISRCS     += wpscli_wpscore_hooks.c

WPSCOREOBJS  = $(WPSCORESRCS:%.c=$(OBJDIR)/%.o)
CRYPTOBJS    = $(CRYPTSRCS:%.c=$(OBJDIR)/%.o)
OSLOBJS      = $(OSLSRCS:%.c=$(OBJDIR)/%.o)
OSIOBJS      = $(OSISRCS:%.c=$(OBJDIR)/%.o)

WPSTESTCMDSRCS = wpscli_test_cmd.c
WPSTESTCMDOBJS = $(WPSTESTCMDSRCS:%.c=$(OBJDIR)/%.o)

all: showinfo objdir 
ifeq ($(SHARED_LIB), 1)
ifeq ($(EXTERNAL_OPENSSL),1)
all: $(OBJDIR)/libwpscore.so $(OBJDIR)/libwpscliosl.so $(OBJDIR)/libwpscli.so 
else
all: $(OBJDIR)/libbcmcrypto.so $(OBJDIR)/libwpscore.so $(OBJDIR)/libwpscliosl.so $(OBJDIR)/libwpscli.so
endif #EXTERNAL_OPENSSL

else #SHARED_LIB

ifeq ($(EXTERNAL_OPENSSL),1)
all: $(OBJDIR)/libwpscore.a 
else
all: $(OBJDIR)/libbcmcrypto.a $(OBJDIR)/libwpscore.a 
endif#EXTERNAL_OPENSSL
all: $(OBJDIR)/libwpscliosl.a $(OBJDIR)/libwpscli.a

endif #SHARED_LIB
all: $(OBJDIR)/wpscli_test_cmd

#all : objdir libfoo.a wpscli_test_cmd

showinfo:
	@echo "WPSCOREOBJS = $(WPSCOREOBJS)"
	@echo "CRYPTOBJS = $(CRYPTOBJS)"
	@echo "OSLOBJS = $(OSLOBJS)"
	@echo "OSIOBJS = $(OSIOBJS)"

# create folders to hold objects files of each lib
objdir :
	@[ -d "$(OBJDIR)" ] || mkdir -pv $(OBJDIR)

ifneq ($(SHARED_LIB), 1)
# WPS common files shared by both wpscli and AP
libwpscore.a $(OBJDIR)/libwpscore.a : $(WPSCOREOBJS)
	@[ -d "$(@D)" ] || mkdir -pv $(@D)
	$(AR) crs$(if $(V),v) $(OBJDIR)/$(@F) $^

# shared crypto library
libbcmcrypto.a $(OBJDIR)/libbcmcrypto.a : $(CRYPTOBJS)
	@[ -d "$(@D)" ] || mkdir -pv $(@D)
	$(AR) crs$(if $(V),v) $(OBJDIR)/$(@F) $^

# wpscli osl lib file
ifneq ($(EXTERNAL_OPENSSL),1)
libwpscliosl.a $(OBJDIR)/libwpscliosl.a : $(OSLOBJS) $(CRYPTOBJS)
	@[ -d "$(@D)" ] || mkdir -pv $(@D)
	$(AR) crs$(if $(V),v) $(OBJDIR)/$(@F) $^

else
libwpscliosl.a $(OBJDIR)/libwpscliosl.a : $(OSLOBJS) 
	@[ -d "$(@D)" ] || mkdir -pv $(@D)
	$(AR) crs$(if $(V),v) $(OBJDIR)/$(@F) $^

endif

# final wpscli api lib file to be deliverred
libwpscli.a $(OBJDIR)/libwpscli.a : $(OSIOBJS) $(WPSCOREOBJS) $(OSLOBJS)
	@[ -d "$(@D)" ] || mkdir -pv $(@D)
	$(AR) crs$(if $(V),v) $(OBJDIR)/$(@F) $^

else #SHARED_LIB

# WPS common files shared by both wpscli and AP
libwpscore.so $(OBJDIR)/libwpscore.so : $(WPSCOREOBJS)
	@[ -d "$(@D)" ] || mkdir -pv $(@D)
	$(LD) -shared$(if $(V),v) -o $(OBJDIR)/$(@F) $^

# shared crypto library
libbcmcrypto.so $(OBJDIR)/libbcmcrypto.so : $(CRYPTOBJS)
	@[ -d "$(@D)" ] || mkdir -pv $(@D)
	$(LD) -shared$(if $(V),v) -o $(OBJDIR)/$(@F) $^

# wpscli osl lib file
ifneq ($(EXTERNAL_OPENSSL),1)
libwpscliosl.so $(OBJDIR)/libwpscliosl.so : $(OSLOBJS) $(CRYPTOBJS)
	@[ -d "$(@D)" ] || mkdir -pv $(@D)
	$(LD) -shared$(if $(V),v) -o $(OBJDIR)/$(@F) $^
else
libwpscliosl.so $(OBJDIR)/libwpscliosl.so : $(OSLOBJS) 
	@[ -d "$(@D)" ] || mkdir -pv $(@D)
	$(LD) -shared$(if $(V),v) -o $(OBJDIR)/$(@F) $^
endif

# final wpscli api lib file to be deliverred
libwpscli.so $(OBJDIR)/libwpscli.so : $(OSIOBJS) $(WPSCOREOBJS) $(OSLOBJS)
	@[ -d "$(@D)" ] || mkdir -pv $(@D)
	$(LD) -shared$(if $(V),v) -o $(OBJDIR)/$(@F) $^

endif #SHARED_LIB

wpscli_test_cmd $(OBJDIR)/wpscli_test_cmd : $(WPSTESTCMDOBJS)
	@[ -d "$(@D)" ] || mkdir -pv $(@D)
	$(CC) $(if $(V),-H) $(INCLUDE) $(WPSTESTCMDOBJS) $(SAMPLEAPPLIBS) \
		-lpthread -o $(OBJDIR)/$(@F)


ifeq ($(EXTERNAL_OPENSSL),1)
$(WPSCOREOBJS) $(OSLOBJS) $(OSIOBJS) $(WPSTESTCMDOBJS): $(OBJDIR)/%.o: %.c
	@[ -d "$(@D)" ] || mkdir -pv $(@D)
	$(CC) -c $(CFLAGS) $(if $(V),-H) $(INCLUDE) $< -o $@
else
$(WPSCOREOBJS) $(CRYPTOBJS) $(OSLOBJS) $(OSIOBJS) $(WPSTESTCMDOBJS): $(OBJDIR)/%.o: %.c
	@[ -d "$(@D)" ] || mkdir -pv $(@D)
	$(CC) -c $(CFLAGS) $(if $(V),-H) $(INCLUDE) $< -o $@
endif
# cleanup
clean: cleanfiles cleanfolders

cleanfiles:
	find $(OBJDIR) -name "*.o" | xargs rm -f
	find $(OBJDIR) -name "*.a" | xargs rm -f
	find $(OBJDIR) -name "wpscli_test_cmd"  | xargs rm -f

cleanfolders:
	@rm -rfv $(OBJDIR)

libfoo.a : $(OBJDIR)/foo.o 
	$(AR) crs$(if $(V),v) $@ $^

