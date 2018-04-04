#
# Makefile for the IEEE 802.1d ethernet bridging layer.
#

obj-$(CONFIG_BRIDGE) += bridge.o

bridge-y	:= br.o br_device.o br_fdb.o br_forward.o br_if.o br_input.o \
			br_ioctl.o br_notify.o br_stp.o br_stp_bpdu.o \
			br_stp_if.o br_stp_timer.o br_netlink.o

bridge-$(CONFIG_SYSFS) += br_sysfs_if.o br_sysfs_br.o

bridge-$(CONFIG_BR_IGMP_SNOOP) += br_igmp.o

bridge-$(CONFIG_BR_MLD_SNOOP) += br_mld.o

bridge-$(CONFIG_BRIDGE_NETFILTER) += br_netfilter.o

ifeq ($(strip $(CONFIG_BR_IGMP_SNOOP)),y)
bridge-$(CONFIG_BR_IGMP_SNOOP) += br_netlink_mcpd.o br_mcast.o
else
ifeq ($(strip $(CONFIG_BR_MLD_SNOOP)),y)
bridge-$(CONFIG_BR_MLD_SNOOP) += br_netlink_mcpd.o br_mcast.o
endif
endif

ifeq ($(strip $(CONFIG_11ac_throughput_patch_from_412L07)),y)
bridge-$(CONFIG_MIPS_BRCM) += br_notifier.o br_flows.o
endif

obj-$(CONFIG_BRIDGE_NF_EBTABLES) += netfilter/
