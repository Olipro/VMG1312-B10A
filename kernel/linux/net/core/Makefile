#
# Makefile for the Linux networking core.
#

# CONFIG_MIPS_BRCM Begin Broadcom changed code.
ifeq ($(strip $(BUILD_ZYXEL_WLAN_DRIVER)), "4.12L08")
EXTRA_CFLAGS += -DSUPPORT_WLAN_DRIVER_412L08
endif
EXTRA_CFLAGS	+= -I$(INC_BRCMDRIVER_PUB_PATH)/$(BRCM_BOARD)
# CONFIG_MIPS_BRCM End Broadcom changed code.

obj-y := sock.o request_sock.o skbuff.o iovec.o datagram.o stream.o scm.o \
	 gen_stats.o gen_estimator.o net_namespace.o

obj-$(CONFIG_SYSCTL) += sysctl_net_core.o
obj-$(CONFIG_HAS_DMA) += skb_dma_map.o

obj-y		     += dev.o ethtool.o dev_mcast.o dst.o netevent.o \
			neighbour.o rtnetlink.o utils.o link_watch.o filter.o

obj-$(CONFIG_XFRM) += flow.o
obj-y += net-sysfs.o
obj-$(CONFIG_NET_PKTGEN) += pktgen.o
obj-$(CONFIG_NETPOLL) += netpoll.o
obj-$(CONFIG_NET_DMA) += user_dma.o
obj-$(CONFIG_FIB_RULES) += fib_rules.o
obj-$(CONFIG_TRACEPOINTS) += net-traces.o
obj-$(CONFIG_NET_DROP_MONITOR) += drop_monitor.o

# link updown message in kernel
obj-$(CONFIG_KLINK_UPDOWN) += klink_updown.o
obj-y += klink_updown.o

# CONFIG_MIPS_BRCM Begin Broadcom changed code for CONFIG_BLOG
obj-y += blog.o blog_rule.o nbuff.o iqos.o gbpm.o
#obj-$(CONFIG_BLOG) += blog.o
#obj-y += nbuff.o
# CONFIG_MIPS_BRCM End Broadcom changed code for CONFIG_BLOG
