#
# Broadcom Linux Router Makefile
# 
# Copyright (C) 2010, Broadcom Corporation. All Rights Reserved.
# 
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
# 
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
# OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
# CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#
# $Id: Makefile 260723 2011-05-20 02:06:51Z kenlo $
#

include .config




#
# Paths
#


CPU ?=
LINUX_VERSION ?= 2_4
MAKE_ARGS ?=

# Source bases
export PLATFORM LIBDIR USRLIBDIR LINUX_VERSION
export TOP := $(shell pwd)
export SRCBASE := $(shell (cd $(TOP)/.. && pwd -P))

ifeq ($(LINUX_VERSION),2_6)
export 	LINUXDIR := $(SRCBASE)/linux/linux-2.6
export 	KBUILD_VERBOSE := 1
export	BUILD_MFG := 0
else
export 	LINUXDIR := $(SRCBASE)/linux/linux
endif


#
# Cross-compile environment variables
#

# Build platform
export BUILD := i386-pc-linux-gnu
export HOSTCC := gcc

# uClibc wrapper
ifeq ($(CONFIG_UCLIBC),y)
export PLATFORM := $(PLATFORM)-uclibc
endif

ifeq ($(PLATFORM),mipsel)
export CROSS_COMPILE := mipsel-linux-
export CONFIGURE := ./configure mipsel-linux --build=$(BUILD)
export TOOLCHAIN := $(shell cd $(dir $(shell which $(CROSS_COMPILE)gcc))/../mipsel-linux && pwd -P)
endif

ifeq ($(PLATFORM),mipsel-uclibc)
ifeq ($(LINUX_VERSION),2_6)
export CROSS_COMPILE := mipsel-uclibc-linux26-
else
export CROSS_COMPILE := mipsel-uclibc-
endif
export CONFIGURE := ./configure mipsel-linux --build=$(BUILD)
export TOOLCHAIN := $(shell cd $(dir $(shell which $(CROSS_COMPILE)gcc))/.. && pwd -P)
endif


ifeq ($(CONFIG_RTR_OPTIMIZE_SIZE),y)
export CFLAGS += -Os
export OPTCFLAGS = -Os
else
export CFLAGS += -O2
export OPTCFLAGS = -O2
endif

#look at driver configuration
WLCFGDIR=$(SRCBASE)/wl/config

ifeq ($(CONFIG_BCMWPA2),y)
export CFLAGS += -DBCMWPA2
endif

ifeq ("$(CONFIG_USBAP)","y")
export CFLAGS += -D__CONFIG_USBAP__
endif

ifeq ($(CONFIG_BCMQOS),y)
export CFLAGS += -DBCMQOS
endif
ifeq ($(CONFIG_WSCCMD),y)
export CONFIG_WSCCMD
export CFLAGS += -DBCMWPS
# WFA WPS 2.0 Testbed extra caps
#export CFLAGS += -DWFA_WPS_20_TESTBED
endif

ifeq ($(CONFIG_EMF),y)
export CFLAGS += -D__CONFIG_EMF__
endif

ifeq ($(CONFIG_IGMP_PROXY),y)
export CFLAGS += -D__CONFIG_IGMP_PROXY__
endif

ifeq ($(CONFIG_WL_ACI),y)
export CFLAGS += -D__CONFIG_WL_ACI__
endif

ifeq ($(CONFIG_MEDIA_IPTV),y)
export CFLAGS += -D__CONFIG_MEDIA_IPTV__
endif

ifeq ($(CONFIG_SOUND),y)
export CFLAGS += -D__CONFIG_SOUND__
endif

ifeq ($(CONFIG_VOIP),y)
export CFLAGS += -DBCMVOIP
endif

ifeq ($(CONFIG_SQUASHFS), y)
ROOT_IMG :=target.squashfs
else
ROOT_IMG := target.cramfs
endif

ifeq ($(CONFIG_WAPI),y)
export CFLAGS += -DBCMWAPI_WAI -DBCMWAPI_WPI
endif

ifeq ($(CONFIG_PHYMON_UTILITY),y)
export CFLAGS += -DPHYMON
endif

ifeq ($(LINUXDIR), $(SRCBASE)/linux/linux-2.6)
CRAMFSDIR := cramfs
else
CRAMFSDIR := $(LINUXDIR)/scripts/cramfs
endif

ifeq ($(CONFIG_BCMDCS),y)
export CFLAGS += -DBCM_DCS
endif

ifeq ($(CONFIG_EXTACS),y)
export CFLAGS += -DEXT_ACS
endif

ifeq ($(CONFIG_MFP),y)
export CFLAGS += -DMFP
endif

ifeq ($(CONFIG_SIGMA),y)
export CFLAGS += -D__CONFIG_SIGMA__
endif


export CC := $(CROSS_COMPILE)gcc
export AR := $(CROSS_COMPILE)ar
export AS := $(CROSS_COMPILE)as
export LD := $(CROSS_COMPILE)ld
export NM := $(CROSS_COMPILE)nm
export RANLIB := $(CROSS_COMPILE)ranlib
export STRIP := $(CROSS_COMPILE)strip
export SIZE := $(CROSS_COMPILE)size

#
# Install and target directories
#

export PLATFORMDIR := $(TOP)/$(PLATFORM)
export INSTALLDIR := $(PLATFORMDIR)/install
export TARGETDIR := $(PLATFORMDIR)/target

define STRIP_DEBUG_SYMBOLS
	@dbgsymf=$(basename $(1))_dbgsym$(suffix $(1)); \
	if [ "$(1)" -nt "$${dbgsymf}" ]; then \
	   echo "#- $0"; \
	   ls -ln $1 | awk '{printf "Orig  size: %10d bytes, %s\n",$$5,$$NF}'; \
	   cp -p -v $1 $$dbgsymf; $(STRIP) -d $(1); touch $$dbgsymf; \
	   ls -ln $1 | awk '{printf "Strip size: %10d bytes, %s\n",$$5,$$NF}'; \
	fi
endef

# USB AP support
# note : the dongle target is only for after pre-build 
obj-$(CONFIG_USBAP) += bmac dongle

# always build libbcmcrypto
obj-y += libbcmcrypto

#
# Configuration
#

#ifdef BCMSOUND
obj-$(CONFIG_SIGMA) += sigma
obj-$(CONFIG_SALSA) += salsa
obj-$(CONFIG_LIBZ) += libz
obj-$(CONFIG_LIBID3TAG) += libid3tag
obj-$(CONFIG_LIBMAD) += libmad
obj-$(CONFIG_MADPLAY) += madplay
obj-$(CONFIG_APLAY) += alsa-utils/aplay
#endif
obj-$(CONFIG_NVRAM) += nvram
obj-$(CONFIG_SHARED) += shared
obj-$(CONFIG_LIBBCM) += libbcm
obj-$(CONFIG_HTTPD) += httpd
obj-$(CONFIG_RC) += rc
obj-$(CONFIG_WWW) += www
obj-$(CONFIG_GLIBC) += lib
obj-$(CONFIG_UCLIBC) += lib
obj-$(CONFIG_WLCONF) += wlconf
obj-$(CONFIG_BRIDGE) += bridge
obj-$(CONFIG_BUSYBOX) += busybox
obj-$(CONFIG_DNSMASQ) += dnsmasq
obj-$(CONFIG_IPTABLES) += iptables
obj-$(CONFIG_LIBIPT) += iptables
obj-$(CONFIG_NAS) += nas
obj-$(CONFIG_WAPI) += wapi/wapid
obj-$(CONFIG_WAPI_IAS) += wapi/as
obj-$(CONFIG_SES) += ses/ses
obj-$(CONFIG_SES_CL) += ses/ses_cl
obj-$(CONFIG_EZC) += ezc
obj-$(CONFIG_NETCONF) += netconf
obj-$(CONFIG_NTP) += ntpclient
obj-$(CONFIG_PPP) += ppp
obj-$(CONFIG_UDHCPD) += udhcpd
obj-$(CONFIG_UPNP) += bcmupnp
obj-$(CONFIG_IGD) += igd
obj-$(CONFIG_LIBUPNP) += libupnp
obj-$(CONFIG_FFMPEG) += ffmpeg
obj-$(CONFIG_DLNA_DMR) += dlna/dmr
obj-$(CONFIG_DLNA_DMS) += dlna/dms
obj-$(CONFIG_SAMBA) += samba
obj-$(CONFIG_UTILS) += utils
obj-$(CONFIG_ETC) += etc
obj-$(CONFIG_VLAN) += vlan
obj-$(CONFIG_WSCCMD) += wps
obj-$(CONFIG_EMF) += emf
obj-$(CONFIG_EMF) += igs
obj-$(CONFIG_IGMP_PROXY) += igmp
obj-$(CONFIG_WL_ACI) += aci
ifeq ($(LINUXDIR), $(SRCBASE)/linux/linux-2.6)
obj-y += udev
obj-y += hotplug2
endif
obj-$(CONFIG_LLD2D) += lltd/wrt54g-linux
#if defined(BCMDBG)
obj-$(CONFIG_UTELNETD) += utelnetd
#endif
#if defined(PHYMON)
obj-$(CONFIG_PHYMON_UTILITY) += phymon
#endif
#if defined(EXT_ACS)
obj-$(CONFIG_EXTACS) += acsd
#endif

obj-$(CONFIG_RADVD) += radvd
obj-$(CONFIG_IPROUTE2) += iproute2
obj-$(CONFIG_IPUTILS) += iputils
obj-$(CONFIG_DHCPV6S) += dhcp6s
obj-$(CONFIG_DHCPV6C) += dhcp6c

# Gigle apps
obj-$(CONFIG_PLC) += gigled libgigle gigle_util
ifeq ($(CONFIG_PLC),y)
CFLAGS	+= -DPLC
endif

# always build eap dispatcher
obj-y += eapd/linux

ifeq ($(CONFIG_VOIP),y)
obj-y += voipd
endif


obj-clean := $(foreach obj,$(obj-y) $(obj-n),$(obj)-clean)
obj-install := $(foreach obj,$(obj-y),$(obj)-install)

# separate the libraries which need to be built first
obj-prelibs =$(filter nvram libbcmcrypto shared netconf libupnp libz libid3tag ffmpeg, $(obj-y)) 
# remaining libraries that are built next
obj-postlibs := $(filter-out $(obj-prelibs), $(obj-y))

ifneq ($(WLTEST),1)
ifneq ($(shell grep "CONFIG_EMBEDDED_RAMDISK=y" $(LINUXDIR)/.config),)
export WLTEST := 1
endif
endif

ifeq ($(WLTEST),1)
export CFLAGS += -DWLTEST
endif

ifeq ($(LINUXDIR), $(SRCBASE)/linux/linux-2.6)
ifneq ($(shell grep "CONFIG_BLK_DEV_INITRD=y" $(LINUXDIR)/.config),)
ifeq ($(shell grep "CONFIG_BLK_DEV_RAM=y" $(LINUXDIR)/.config),)
export BUILD_MFG := 1
endif
endif
endif

#
# Basic rules
#

all: version check_kernel_config $(obj-prelibs)
	$(MAKE) $(MAKE_ARGS) post_preplibs

post_preplibs:  $(obj-postlibs)
        # flip MFP on all config including apdef
	-cp $(WLCFGDIR)/wlconfig_apdef $(WLCFGDIR)/wlconfig_apdef.tmp
ifeq ($(CONFIG_MFP),y)
	-sed -e "s/MFP=0/MFP=1/g" $(WLCFGDIR)/wlconfig_apdef.tmp > $(WLCFGDIR)/wlconfig_apdef;
else
	-sed -e "s/MFP=1/MFP=0/g" $(WLCFGDIR)/wlconfig_apdef.tmp > $(WLCFGDIR)/wlconfig_apdef;
endif
	-rm -f $(WLCFGDIR)/wlconfig_apdef.tmp
        # Also build kernel
ifeq ($(LINUXDIR), $(SRCBASE)/linux/linux-2.6)
ifneq ($(BUILD_MFG), 1)
	$(MAKE) -C $(LINUXDIR) zImage
	$(MAKE) CONFIG_SQUASHFS=$(CONFIG_SQUASHFS) -C $(SRCBASE)/router/compressed
endif
else
	if ! grep -q "CONFIG_EMBEDDED_RAMDISK=y" $(LINUXDIR)/.config ; then \
	    $(MAKE) -C $(LINUXDIR) zImage ; \
	fi
endif
	if grep -q "CONFIG_MODULES=y" $(LINUXDIR)/.config ; then \
	    $(MAKE) -C $(LINUXDIR) modules ; \
	fi
	# Preserve the debug versions of these and strip for release
	$(call STRIP_DEBUG_SYMBOLS,$(LINUXDIR)/vmlinux)
ifeq ($(LINUXDIR), $(SRCBASE)/linux/linux-2.6)
	$(call STRIP_DEBUG_SYMBOLS,$(LINUXDIR)/drivers/net/wl/wl.ko)
ifeq ("$(CONFIG_USBAP)","y")
	$(call STRIP_DEBUG_SYMBOLS,$(LINUXDIR)/drivers/net/wl/wl_high/wl_high.ko)
endif
	$(call STRIP_DEBUG_SYMBOLS,$(LINUXDIR)/drivers/net/et/et.ko)
	$(call STRIP_DEBUG_SYMBOLS,$(LINUXDIR)/drivers/net/ctf/ctf.ko)
	$(call STRIP_DEBUG_SYMBOLS,$(LINUXDIR)/drivers/net/bcm57xx/bcm57xx.ko)
	$(call STRIP_DEBUG_SYMBOLS,$(LINUXDIR)/drivers/net/emf/emf.ko)
	$(call STRIP_DEBUG_SYMBOLS,$(LINUXDIR)/drivers/net/igs/igs.ko)
else
	$(call STRIP_DEBUG_SYMBOLS,$(LINUXDIR)/drivers/net/wl/wl.o)
ifeq ("$(CONFIG_USBAP)","y")
	$(call STRIP_DEBUG_SYMBOLS,$(LINUXDIR)/drivers/net/wl/wl_high/wl_high.o)
endif
	$(call STRIP_DEBUG_SYMBOLS,$(LINUXDIR)/drivers/net/et/et.o)
	$(call STRIP_DEBUG_SYMBOLS,$(LINUXDIR)/drivers/net/ctf/ctf.o)
	$(call STRIP_DEBUG_SYMBOLS,$(LINUXDIR)/drivers/net/bcm57xx/bcm57xx.o)
	$(call STRIP_DEBUG_SYMBOLS,$(LINUXDIR)/drivers/net/emf/emf.o)
	$(call STRIP_DEBUG_SYMBOLS,$(LINUXDIR)/drivers/net/igs/igs.o)
endif


# well, we should always be able to use the BOM, but right now, the last build step on
# the build machine doesn't have it, so we don't rerun this is the file already 
# exists

version:  $(SRCBASE)/router/shared/router_version.h

# this could fail if the bom doesn't exist. We don't care as long as there is a valid router_version.h
# if not, the build will fail anyway.
$(SRCBASE)/router/shared/router_version.h: $(SRCBASE)/router/shared/version.h.in
#	# In SVN conversion, bom files are renamed, so following will be true 
#	# if *bom.mk doesn't exist or if .svn doesn't exist
	@if [ -s "$(SRCBASE)/tools/release/linux-router-bom.mk"  -a ! -d ".svn" ]; then \
		echo make SRCBASE=$(SRCBASE) -f \
			$(SRCBASE)/tools/release/linux-router-bom.mk version; \
		make SRCBASE=$(SRCBASE) -f \
			$(SRCBASE)/tools/release/linux-router-bom.mk version; \
	fi


router-clean: $(obj-clean) config-clean
	rm -rf $(TARGETDIR)
	rm -f $(PLATFORMDIR)/linux.trx $(PLATFORMDIR)/vmlinuz $(PLATFORMDIR)/target.cramfs
	rm -f $(PLATFORMDIR)/linux-lzma.trx $(PLATFORMDIR)/vmlinuz-lzma $(PLATFORMDIR)/target.squashfs
	rm -rf $(INSTALLDIR)/busybox

clean: router-clean
	@echo cleaning LINUXDIR = $(LINUXDIR)
ifeq ($(LINUXDIR), $(SRCBASE)/linux/linux-2.6)
	# we need to pass some conf file for cleaning 2.6. The kbuild clean doesn't seem to 
	# to load the .config the way our wl Makefile  is expecting.
	$(MAKE) CONFIG_WL_CONF=wlconfig_lx_router_ap -C $(LINUXDIR) clean
	$(MAKE) -C $(SRCBASE)/router/compressed clean
else
	$(MAKE) -C $(LINUXDIR) clean
endif
	$(MAKE) -C $(SRCBASE)/cfe/build/broadcom/bcm947xx clean
	[ ! -f $(SRCBASE)/tools/misc/Makefile ] || $(MAKE) -C $(SRCBASE)/tools/misc clean

distclean mrproper: clean
	rm -f .config $(LINUXDIR)/.config

install package: $(filter-out lib-install,$(obj-install)) check_kernel_config
        # Install binaries into target directory
	install -d $(TARGETDIR)
	for dir in $(wildcard $(patsubst %,$(INSTALLDIR)/%,$(obj-y))) ; do \
	    (cd $${dir} && tar cpf - .) | (cd $(TARGETDIR) && tar xpf -) \
	done
	# optimize the crypto library by removing unneeded symbols
	[ ! -d libbcmcrypto ] || $(MAKE) -C libbcmcrypto optimize
ifneq ("$(CONFIG_WAPI)$(CONFIG_WAPI_IAS)","")
	# optimize the OPENSSL library by removing unneeded symbols
#	[ ! -d wapi/wapid ] || $(MAKE) -C wapi/wapid optimize
endif
        # Install (and possibly optimize) C library
	$(MAKE) lib-install
        # Install modules into filesystem
	if grep -q "CONFIG_MODULES=y" $(LINUXDIR)/.config ; then \
	    $(MAKE) -C $(LINUXDIR) modules_install DEPMOD=/bin/true INSTALL_MOD_PATH=$(TARGETDIR) ; \
	fi
ifeq ($(LINUXDIR), $(SRCBASE)/linux/linux-2.6)
ifeq ("$(CONFIG_USBAP)","y")
	# Don't delete wl_high.ko for USBAP
	find $(TARGETDIR) -name "wl_*.ko" | sed s/\.\*wl_high\\\.ko//g | xargs rm -rf
else
	find $(TARGETDIR) -name "wl_*.ko" | xargs rm -rf
endif
else # Linux 2.4
ifeq ("$(CONFIG_USBAP)","y")
	# Don't delete wl_high.o for USBAP
	find $(TARGETDIR) -name "wl_*.o" | sed s/\.\*wl_high\\\.o//g | xargs rm -rf
else
	find $(TARGETDIR) -name "wl_*.o" | xargs rm -rf
endif
endif
        # Prepare filesystem
	cd $(TARGETDIR) && $(TOP)/misc/rootprep.sh

ifeq ($(CONFIG_SQUASHFS), y)
	# Make sure mksquashfs 3.0 is used
	$(MAKE) -C squashfs mksquashfs
	squashfs/mksquashfs $(TARGETDIR) $(PLATFORMDIR)/$(ROOT_IMG) -noappend -all-root
else
	# Make sure mkcramfs-2.0 is used
	$(MAKE) -C $(CRAMFSDIR) mkcramfs
	# '-u 0 -g 0' will set the uid and gid of all the files to 0 (root)
	# These options are currently only available on our version of mkcramfs
	$(CRAMFSDIR)/mkcramfs -u 0 -g 0 $(TARGETDIR) $(PLATFORMDIR)/$(ROOT_IMG)
endif

ifeq ($(LINUXDIR), $(SRCBASE)/linux/linux-2.6)
	# Package kernel and filesystem
ifeq ($(BUILD_MFG), 1)
	cd $(TARGETDIR) ; \
	find . | cpio -o -H newc | gzip > $(LINUXDIR)/usr/initramfs_data.cpio.gz
	ls -l $(LINUXDIR)/usr/initramfs_data.cpio.gz
	$(MAKE) -C $(LINUXDIR) zImage
	$(MAKE) -C $(SRCBASE)/router/compressed
else
	cp $(SRCBASE)/router/compressed/vmlinuz $(PLATFORMDIR)/
	trx -o $(PLATFORMDIR)/linux.trx $(PLATFORMDIR)/vmlinuz $(PLATFORMDIR)/$(ROOT_IMG)
	if grep -q "CONFIG_SQUASHFS=y" $(LINUXDIR)/.config ; then \
		cp $(SRCBASE)/router/compressed/vmlinuz-lzma $(PLATFORMDIR)/ ; \
		trx -o $(PLATFORMDIR)/linux-lzma.trx $(PLATFORMDIR)/vmlinuz-lzma $(PLATFORMDIR)/$(ROOT_IMG) ; \
	fi
endif
	# Pad self-booting Linux to a 64 KB boundary
	cp $(SRCBASE)/router/compressed/zImage $(PLATFORMDIR)/
else
	# Package kernel and filesystem
	if grep -q "CONFIG_EMBEDDED_RAMDISK=y" $(LINUXDIR)/.config ; then \
	    cp $(PLATFORMDIR)/$(ROOT_IMG) $(LINUXDIR)/arch/mips/ramdisk/$${CONFIG_EMBEDDED_RAMDISK_IMAGE} ; \
	    $(MAKE) -C $(LINUXDIR) zImage ; \
	else \
	    cp $(LINUXDIR)/arch/mips/brcm-boards/bcm947xx/compressed/vmlinuz $(PLATFORMDIR)/ ; \
	    trx -o $(PLATFORMDIR)/linux.trx $(PLATFORMDIR)/vmlinuz $(PLATFORMDIR)/$(ROOT_IMG) ; \
		if grep -q "CONFIG_SQUASHFS=y" $(LINUXDIR)/.config ; then \
			cp $(LINUXDIR)/arch/mips/brcm-boards/bcm947xx/compressed/vmlinuz-lzma $(PLATFORMDIR)/ ; \
			trx -o $(PLATFORMDIR)/linux-lzma.trx $(PLATFORMDIR)/vmlinuz-lzma $(PLATFORMDIR)/$(ROOT_IMG) ; \
		fi \
	fi
	# Pad self-booting Linux to a 64 KB boundary
	cp $(LINUXDIR)/arch/mips/brcm-boards/bcm947xx/compressed/zImage $(PLATFORMDIR)/
endif
	dd conv=sync bs=64k < $(PLATFORMDIR)/zImage > $(PLATFORMDIR)/linux.bin
	# Append filesystem to self-booting Linux
	cat $(PLATFORMDIR)/$(ROOT_IMG) >> $(PLATFORMDIR)/linux.bin
#
# Configuration rules
#

conf mconf:
	$(MAKE) -C config
	@./config/$@ ./config/Config
        # Also configure kernel
	$(MAKE) k$@

oldconf: .config
	$(MAKE) -C config
	@./config/conf -o ./config/Config
        # Also configure kernel
	$(MAKE) k$@

kconf:
	$(MAKE) -C $(LINUXDIR) config

kmconf: check_kernel_config 
	$(MAKE) -C $(LINUXDIR) menuconfig

koldconf: check_kernel_config
	$(MAKE) -C $(LINUXDIR) oldconfig

# Convenience
config: conf

menuconfig: mconf

oldconfig: oldconf

# Default configurations
ifneq ("$(LINUX_VERSION)","2_6")
.config:
	cp config/defconfig $@
	$(MAKE) oldconfig
else
.config:
	cp config/defconfig-2.6 $@
	$(MAKE) oldconfig
endif

ifneq ("$(LINUX_VERSION)","2_6")
$(LINUXDIR)/.config:
	cp $(LINUXDIR)/arch/mips/defconfig-bcm947xx $@
else
$(LINUXDIR)/.config:
	cp $(LINUXDIR)/arch/mips/defconfig-2.6-bcm947xx $@
endif

# Overwrite Kernel .config
check_kernel_config: $(LINUXDIR)/.config
	cp $(LINUXDIR)/.config $(LINUXDIR)/.config.tmp
ifeq ($(CONFIG_SQUASHFS), y)
	if ! grep -q "CONFIG_SQUASHFS=y" $(LINUXDIR)/.config ; then \
		cp $(LINUXDIR)/.config $(LINUXDIR)/.config.chk ; \
		sed -e "s/CONFIG_CRAMFS=y/# CONFIG_CRAMFS is not set/g" $(LINUXDIR)/.config.chk | \
		sed -e "s/# CONFIG_SQUASHFS is not set/CONFIG_SQUASHFS=y/g" > \
		$(LINUXDIR)/.config ; \
		rm -f $(LINUXDIR)/.config.chk ; \
	fi
endif
ifeq ($(CONFIG_CRAMFS), y)
	if ! grep -q "CONFIG_CRAMFS=y" $(LINUXDIR)/.config ; then \
		cp $(LINUXDIR)/.config $(LINUXDIR)/.config.chk ; \
		sed -e "s/CONFIG_SQUASHFS=y/# CONFIG_SQUASHFS is not set/g" $(LINUXDIR)/.config.chk | \
		sed -e "s/# CONFIG_CRAMFS is not set/CONFIG_CRAMFS=y/g" > \
		$(LINUXDIR)/.config ; \
		rm -f $(LINUXDIR)/.config.chk ; \
	fi
endif
ifeq ($(CONFIG_SHRINK_MEMORY), y)
	if ! grep -q "CONFIG_SHRINKMEM=y" $(LINUXDIR)/.config ; then \
		cp $(LINUXDIR)/.config $(LINUXDIR)/.config.chk ; \
		sed -e "s/# CONFIG_SHRINKMEM is not set/CONFIG_SHRINKMEM=y/g" $(LINUXDIR)/.config.chk > \
		$(LINUXDIR)/.config ; \
		rm -f $(LINUXDIR)/.config.chk ; \
	fi
else
	if grep -q "CONFIG_SHRINKMEM=y" $(LINUXDIR)/.config ; then \
		cp $(LINUXDIR)/.config $(LINUXDIR)/.config.chk ; \
		sed -e "s/CONFIG_SHRINKMEM=y/# CONFIG_SHRINKMEM is not set/g" $(LINUXDIR)/.config.chk > \
		$(LINUXDIR)/.config ; \
		rm -f $(LINUXDIR)/.config.chk ; \
	fi
endif

	# Make kernel config again if changed
	if ! cmp $(LINUXDIR)/.config $(LINUXDIR)/.config.tmp ; then \
		$(MAKE) -C $(LINUXDIR) oldconfig ; \
	fi

#
# Overrides
#

ifeq ($(LINUXDIR), $(SRCBASE)/linux/linux-2.6)
busybox-1.x/Config.h: dummy
	cd busybox-1.x && rm -f Config.h && ln -sf include/autoconf.h Config.h && touch Config.h
	cd busybox-1.x && cp configs/bbconfig-$(CONFIG_BUSYBOX_CONFIG) .config && make clean

busybox: busybox-1.x/Config.h
	$(MAKE) -C busybox-1.x ARCH=mips INSTALL

busybox-install:
	$(MAKE) -C busybox-1.x ARCH=mips CONFIG_PREFIX=$(INSTALLDIR)/busybox install


rc: netconf nvram shared
	make LINUXDIR=$(LINUXDIR)  -C rc
ifneq ($(CONFIG_BUSYBOX),)
rc: busybox-1.x/Config.h
endif
else #linux-2.6
CURBBCFG=$(CONFIG_BUSYBOX_CONFIG).h
OLDBBCFG=$(shell basename $$(readlink busybox/Config.h) 2> /dev/null)
busybox/Config.h: dummy
ifneq ($(OLDBBCFG),$(CURBBCFG))
	cd busybox && rm -f Config.h && ln -sf configs/$(CURBBCFG) Config.h && touch Config.h
endif

busybox: busybox/Config.h
	$(MAKE) -C busybox STRIPTOOL=$(STRIP)

busybox-install:
	$(MAKE) -C busybox STRIPTOOL=$(STRIP) PREFIX=$(INSTALLDIR)/busybox install

rc: netconf nvram shared
ifneq ($(CONFIG_BUSYBOX),)
rc: busybox/Config.h
endif
endif #linux-2.6

rc-install:
	make LINUXDIR=$(LINUXDIR) INSTALLDIR=$(INSTALLDIR)/rc -C rc install

lib-install:
	make LX_VERS=$(LINUX_VERSION) INSTALLDIR=$(INSTALLDIR)/lib -C lib install

www www-%:
	$(MAKE) -C www/$(CONFIG_VENDOR) $* INSTALLDIR=$(INSTALLDIR)/www





# To re-build bcmdl target, set BUILD_BCMDL to 1
BUILD_BCMDL := 0

ifeq ($(BUILD_BCMDL), 1)
libusb:
	[ ! -d $(SRCBASE)/usbdev/libusb ] || ( pushd $(SRCBASE)/usbdev/libusb; ./configure --host=mipsel-linux CC=$(CROSS_COMPILE)gcc; make; popd )

bcmdl :	libusb
ifeq ($(LINUX_VERSION), 2_6)
	make TARGETENV=linux26mips -C $(SRCBASE)/usbdev/usbdl
else
	make TARGETENV=linuxmips -C $(SRCBASE)/usbdev/usbdl
endif

bcmdl-install:
	install -d $(TARGETDIR)/sbin
	install -D $(SRCBASE)/usbdev/usbdl/bcmdl $(TARGETDIR)/sbin/bcmdl
else	# BUILD_BCMDL
libusb:
	# So that generic rule does not take precedence
	@true

bcmdl:
	# So that generic rule does not take precedence
	@true

bcmdl-install :	
	install -d $(INSTALLDIR)/dongle/sbin
	install -D $(SRCBASE)/usbdev/usbdl/mips_$(LINUX_VERSION)/bcmdl $(INSTALLDIR)/dongle/sbin/bcmdl
#	install -D $(SRCBASE)/dongle/rte/test/run-arm-thumb-cm3-poll-ai-uc/hello.bin.trx $(INSTALLDIR)/dongle/bin/hello.bin.trx
endif	# BUILD_BCMDL

bridge:
ifeq ($(LINUXDIR), $(SRCBASE)/linux/linux-2.6)
	$(MAKE) -C bridge-1.x
else
	$(MAKE) -C bridge brctl/brctl
endif

dongle :

bridge-install:
ifeq ($(LINUXDIR), $(SRCBASE)/linux/linux-2.6)
	install -D bridge-1.x/brctl/brctl $(INSTALLDIR)/bridge/usr/sbin/brctl
else
	install -D bridge/brctl/brctl $(INSTALLDIR)/bridge/usr/sbin/brctl
endif
	$(STRIP) $(INSTALLDIR)/bridge/usr/sbin/brctl

dnsmasq-install:
	install -D dnsmasq/dnsmasq $(INSTALLDIR)/dnsmasq/usr/sbin/dnsmasq
	$(STRIP) $(INSTALLDIR)/dnsmasq/usr/sbin/dnsmasq

ifeq ($(LINUXDIR), $(SRCBASE)/linux/linux-2.6)
iptables:
	$(MAKE) -C iptables-1.x BINDIR=/usr/sbin LIBDIR=/usr/lib KERNEL_DIR=$(LINUXDIR)

iptables-install:
ifeq ($(CONFIG_IPTABLES),y)
	install -d $(INSTALLDIR)/iptables/usr/lib/iptables
	install iptables-1.x/extensions/*.so $(INSTALLDIR)/iptables/usr/lib/iptables
	$(STRIP) $(INSTALLDIR)/iptables/usr/lib/iptables/*.so
	install -D iptables-1.x/iptables $(INSTALLDIR)/iptables/usr/sbin/iptables
	$(STRIP) $(INSTALLDIR)/iptables/usr/sbin/iptables
	install -D iptables-1.x/iptables-restore $(INSTALLDIR)/iptables/usr/sbin/iptables-restore # BCMQOS
	$(STRIP) $(INSTALLDIR)/iptables/usr/sbin/iptables-restore # BCMQOS
else
        # So that generic rule does not take precedence
	@true
endif
iptables-clean:
	-$(MAKE) -C iptables-1.x KERNEL_DIR=$(LINUXDIR) clean
else # linux-2.6
iptables:
	$(MAKE) -C iptables BINDIR=/usr/sbin LIBDIR=/usr/lib KERNEL_DIR=$(LINUXDIR)

iptables-install:
ifeq ($(CONFIG_IPTABLES),y)
	install -d $(INSTALLDIR)/iptables/usr/lib/iptables
	install iptables/extensions/*.so $(INSTALLDIR)/iptables/usr/lib/iptables
	$(STRIP) $(INSTALLDIR)/iptables/usr/lib/iptables/*.so
	install -D iptables/iptables $(INSTALLDIR)/iptables/usr/sbin/iptables
	$(STRIP) $(INSTALLDIR)/iptables/usr/sbin/iptables
else
        # So that generic rule does not take precedence
	@true
endif
iptables-clean:
	-$(MAKE) -C iptables KERNEL_DIR=$(LINUXDIR) clean
endif # linux-2.6

nas: nvram shared

netconf: iptables
ifeq ($(CONFIG_NETCONF),y)
	make LINUXDIR=$(LINUXDIR) -C netconf
else
	# In case of "Prerequisite 'iptables' is newer than target 'netconf'"
	@true
endif

ntpclient-install:
	install -D ntpclient/ntpclient $(INSTALLDIR)/ntpclient/usr/sbin/ntpclient
	$(STRIP) $(INSTALLDIR)/ntpclient/usr/sbin/ntpclient

ppp ppp-%:
	$(MAKE) -C ppp/pppoecd $* INSTALLDIR=$(INSTALLDIR)/ppp

udhcpd-install:
	install -D udhcpd/udhcpd $(INSTALLDIR)/udhcpd/usr/sbin/udhcpd
	$(STRIP) $(INSTALLDIR)/udhcpd/usr/sbin/udhcpd
	cd $(INSTALLDIR)/udhcpd/usr/sbin && ln -sf udhcpd udhcpc

upnp: netconf nvram shared

bcmupnp: netconf nvram shared
	[ ! -f bcmupnp/Makefile ] || $(MAKE) -C bcmupnp

bcmupnp-install:
	[ ! -f bcmupnp/Makefile ] || $(MAKE) -C bcmupnp install INSTALLDIR=$(INSTALLDIR)/bcmupnp

bcmupnp-clean:
	[ ! -f bcmupnp/Makefile ] || $(MAKE) -C bcmupnp clean

wlconf: nvram shared

vlan:
	$(MAKE) -C vlan CROSS=$(CROSS_COMPILE) STRIPTOOL=$(STRIP)

vlan-install:
	$(MAKE) -C vlan CROSS=$(CROSS_COMPILE) STRIPTOOL=$(STRIP) INSTALLDIR=$(INSTALLDIR) install

vlan-clean:
	$(MAKE) -C vlan clean


emf:
	$(MAKE) -C emf/emfconf CROSS=$(CROSS_COMPILE)

emf-install:
ifeq ($(CONFIG_EMF),y)
	install -d $(TARGETDIR)
	$(MAKE) -C emf/emfconf CROSS=$(CROSS_COMPILE) INSTALLDIR=$(INSTALLDIR) install
endif

emf-clean:
	$(MAKE) -C emf/emfconf clean

igs:
	$(MAKE) -C emf/igsconf CROSS=$(CROSS_COMPILE)

igs-install:
ifeq ($(CONFIG_EMF),y)
	install -d $(TARGETDIR)
	$(MAKE) -C emf/igsconf CROSS=$(CROSS_COMPILE) INSTALLDIR=$(INSTALLDIR) install
endif

igs-clean:
	$(MAKE) -C emf/igsconf clean
igmp:
	$(MAKE) -C igmp CROSS=$(CROSS_COMPILE)

igmp-install:
ifeq ($(CONFIG_IGMP_PROXY),y)
	install -d $(TARGETDIR)
	$(MAKE) -C igmp CROSS=$(CROSS_COMPILE) INSTALLDIR=$(INSTALLDIR) install
endif

igmp-clean:
	$(MAKE) -C igmp clean

wps: nvram shared
ifeq ($(CONFIG_WSCCMD),y)
	[ ! -f wps/Makefile ] || $(MAKE) -C wps
else
	# Prevent to use generic rules"
	@true
endif

wps-install:
ifeq ($(CONFIG_WSCCMD),y)
	[ ! -f wps/Makefile ] || $(MAKE) -C wps install INSTALLDIR=$(INSTALLDIR)/wps
else
	# Prevent to use generic rules"
	@true
endif

wps-clean:
ifeq ($(CONFIG_WSCCMD),y)
	[ ! -f wps/Makefile ] || $(MAKE) -C wps clean
else
	# Prevent to use generic rules"
	@true
endif

ifeq ($(LINUXDIR), $(SRCBASE)/linux/linux-2.6)
udev:
	$(MAKE) -C udev CROSS_COMPILE=$(CROSS_COMPILE)

udev-install:
	install -d $(TARGETDIR)
	$(MAKE) -C udev CROSS_COMPILE=$(CROSS_COMPILE) DESTDIR=$(INSTALLDIR) prefix=/udev install-udevtrigger

udev-clean:
	$(MAKE) -C udev clean

hotplug2:
	$(MAKE) -C hotplug2 CROSS_COMPILE=$(CROSS_COMPILE)

hotplug2-install:
	install -d $(TARGETDIR)
	install -d $(INSTALLDIR)/hotplug2
	$(MAKE) -C hotplug2 CROSS_COMPILE=$(CROSS_COMPILE) PREFIX=$(INSTALLDIR) install

hotplug2-clean:
	$(MAKE) -C hotplug2 clean
endif


IPROUTE2_SUBDIRS := lib 

ifeq ($(LINUX_VERSION),2_4)
UCLIBC_IPV6=$(TOP)/lib/mipsel-uclibc/libc.so.0
IPROUTE2_SUBDIRS += ip
endif

ifeq ($(CONFIG_BCMQOS),y)
IPROUTE2_SUBDIRS += tc
endif

radvd: flex dummy
	[ ! -d $@ ] || [ -f $@/Makefile ] || ( cd $@ && CC=$(CC) ./configure --host=mipsel-linux && cd .. )
	[ ! -d $@ ] || $(MAKE) -C radvd CC=$(CC) radvd_LDADD="-L../flex -lfl"

radvd-install:
	[ ! -d radvd ] || install -D -m 755 radvd/radvd $(INSTALLDIR)/radvd/usr/sbin/radvd
	[ ! -d radvd ] || $(STRIP) $(INSTALLDIR)/radvd/usr/sbin/radvd

radvd-clean:
	[ ! -f radvd/Makefile ] || $(MAKE) -C radvd distclean

flex: dummy
	[ ! -d $@ ] || [ -f $@/Makefile ] || ( cd $@ && CC=$(CC) ./configure --host=mipsel-linux && cd .. )
	[ ! -d $@ ] || $(MAKE) -C flex CC=$(CC) RANLIB=$(RANLIB)

iproute2:
	[ ! -d $@ ] || $(MAKE) -C iproute2 KERNEL_INCLUDE=$(LINUXDIR)/include CC=$(CC) AR=$(AR) SUBDIRS="$(IPROUTE2_SUBDIRS)" LDLIBS="$(UCLIBC_IPV6)" 

iproute2-install:
ifeq ($(CONFIG_BCMQOS),y)
	[ ! -d iproute2 ] || install -D -m 755 iproute2/tc/tc $(INSTALLDIR)/iproute2/usr/sbin/tc
	[ ! -d iproute2 ] || $(STRIP) $(INSTALLDIR)/iproute2/usr/sbin/tc
else
	[ ! -d iproute2 ] || install -D -m 755 iproute2/ip/ip $(INSTALLDIR)/iproute2/usr/sbin/ip
	[ ! -d iproute2 ] || $(STRIP) $(INSTALLDIR)/iproute2/usr/sbin/ip
endif

iputils:
	[ ! -d $@ ] || [ -f $@/include-glibc/bits/socket.h ] || ( cd $@/include-glibc/bits && ln -s ../socketbits.h socket.h && cd ../../.. )
	[ ! -d $@ ] || $(MAKE) -C iputils KERNEL_INCLUDE=$(LINUXDIR)/include CC=$(CC) LDLIBS=

iputils-install:
	[ ! -d iputils ] || install -D -m 755 iputils/ping6 $(INSTALLDIR)/iputils/usr/sbin/ping6
	[ ! -d iputils ] || $(STRIP) $(INSTALLDIR)/iputils/usr/sbin/ping6
	[ ! -d iputils ] || install -D -m 755 iputils/traceroute6 $(INSTALLDIR)/iputils/usr/sbin/traceroute6
	[ ! -d iputils ] || $(STRIP) $(INSTALLDIR)/iputils/usr/sbin/traceroute6
	[ ! -d iputils ] || install -D -m 755 iputils/tracepath6 $(INSTALLDIR)/iputils/usr/sbin/tracepath6
	[ ! -d iputils ] || $(STRIP) $(INSTALLDIR)/iputils/usr/sbin/tracepath6

dhcp6s dhcp6c: dummy
	[ ! -d dhcp6 ] || [ -f dhcp6/Makefile ] || ( cd dhcp6 && ./configure CC=gcc CFLAGS+="-I../shared" && cd .. )
ifeq ($(CONFIG_UCLIBC),y)
	[ ! -d dhcp6 ] || $(MAKE) -C dhcp6 CC=$(CC) LIBS="$(UCLIBC_IPV6)" $@
else
	[ ! -d dhcp6 ] || $(MAKE) -C dhcp6 CC=$(CC) LIBS="-lresolv -L../libbcmcrypto -lbcmcrypto -L$(INSTALLDIR)/libbcmcrypto/usr/lib" $@
endif

dhcp6s-install:
	[ ! -d dhcp6 ] || install -D -m 755 dhcp6/dhcp6s $(INSTALLDIR)/dhcp6s/usr/sbin/dhcp6s
	[ ! -d dhcp6 ] || $(STRIP) $(INSTALLDIR)/dhcp6s/usr/sbin/dhcp6s

dhcp6c-install:
	[ ! -d dhcp6 ] || install -D -m 755 dhcp6/dhcp6c $(INSTALLDIR)/dhcp6c/usr/sbin/dhcp6c
	[ ! -d dhcp6 ] || $(STRIP) $(INSTALLDIR)/dhcp6c/usr/sbin/dhcp6c

dhcp6s-clean dhcp6c-clean:
	[ ! -f dhcp6/Makefile ] || $(MAKE) -C dhcp6 distclean

#
# Generic rules
#

%:
	[ ! -d $* ] || $(MAKE) -C $*

%-clean:
	[ ! -d $* ] || $(MAKE) -C $* clean

%-install:
	[ ! -d $* ] || $(MAKE) -C $* install INSTALLDIR=$(INSTALLDIR)/$*

$(obj-y) $(obj-n) $(obj-clean) $(obj-install): dummy

.PHONY: all clean distclean mrproper install package check_kernel_config
.PHONY: conf mconf oldconf kconf kmconf config menuconfig oldconfig
.PHONY: dummy
