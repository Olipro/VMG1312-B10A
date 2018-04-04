/*
 *	Linux ethernet bridge
 *
 *	Authors:
 *	Lennert Buytenhek		<buytenh@gnu.org>
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 */

#ifndef _LINUX_IF_BRIDGE_H
#define _LINUX_IF_BRIDGE_H

#include <linux/types.h>

#define SYSFS_BRIDGE_ATTR	"bridge"
#define SYSFS_BRIDGE_FDB	"brforward"
#define SYSFS_BRIDGE_PORT_SUBDIR "brif"
#define SYSFS_BRIDGE_PORT_ATTR	"brport"
#define SYSFS_BRIDGE_PORT_LINK	"bridge"

#define BRCTL_VERSION 1

#define BRCTL_GET_VERSION 0
#define BRCTL_GET_BRIDGES 1
#define BRCTL_ADD_BRIDGE 2
#define BRCTL_DEL_BRIDGE 3
#define BRCTL_ADD_IF 4
#define BRCTL_DEL_IF 5
#define BRCTL_GET_BRIDGE_INFO 6
#define BRCTL_GET_PORT_LIST 7
#define BRCTL_SET_BRIDGE_FORWARD_DELAY 8
#define BRCTL_SET_BRIDGE_HELLO_TIME 9
#define BRCTL_SET_BRIDGE_MAX_AGE 10
#define BRCTL_SET_AGEING_TIME 11
#define BRCTL_SET_GC_INTERVAL 12
#define BRCTL_GET_PORT_INFO 13
#define BRCTL_SET_BRIDGE_STP_STATE 14
#define BRCTL_SET_BRIDGE_PRIORITY 15
#define BRCTL_SET_PORT_PRIORITY 16
#define BRCTL_SET_PATH_COST 17
#define BRCTL_GET_FDB_ENTRIES 18

//#if defined(CONFIG_MIPS_BRCM)
//required by userspace - cannot use ifdef
#define BRCTL_ENABLE_SNOOPING        21
#define BRCTL_ENABLE_PROXY_MODE      22

#define BRCTL_ENABLE_IGMP_RATE_LIMIT 23

#define BRCTL_MLD_ENABLE_SNOOPING    24
#define BRCTL_MLD_ENABLE_PROXY_MODE  25

#define BRCTL_ADD_FDB_ENTRIES 26
#define BRCTL_DEL_FDB_ENTRIES 27
#if defined(CONFIG_11ac_throughput_patch_from_412L07)
#define BRCTL_SET_FLOWS              28
#define BRCTL_SET_UNI_UNI_CTRL       29
#endif
//#endif

#define BR_STATE_DISABLED 0
#define BR_STATE_LISTENING 1
#define BR_STATE_LEARNING 2
#define BR_STATE_FORWARDING 3
#define BR_STATE_BLOCKING 4

struct __bridge_info
{
	__u64 designated_root;
	__u64 bridge_id;
	__u32 root_path_cost;
	__u32 max_age;
	__u32 hello_time;
	__u32 forward_delay;
	__u32 bridge_max_age;
	__u32 bridge_hello_time;
	__u32 bridge_forward_delay;
	__u8 topology_change;
	__u8 topology_change_detected;
	__u8 root_port;
	__u8 stp_enabled;
	__u32 ageing_time;
	__u32 gc_interval;
	__u32 hello_timer_value;
	__u32 tcn_timer_value;
	__u32 topology_change_timer_value;
	__u32 gc_timer_value;
};

struct __port_info
{
	__u64 designated_root;
	__u64 designated_bridge;
	__u16 port_id;
	__u16 designated_port;
	__u32 path_cost;
	__u32 designated_cost;
	__u8 state;
	__u8 top_change_ack;
	__u8 config_pending;
	__u8 unused0;
	__u32 message_age_timer_value;
	__u32 forward_delay_timer_value;
	__u32 hold_timer_value;
};

struct __fdb_entry
{
	__u8 mac_addr[6];
	__u8 port_no;
	__u8 is_local;
	__u32 ageing_timer_value;
	__u8 port_hi;
	__u8 pad0;
	__u16 unused;
};

#ifdef __KERNEL__

#include <linux/netdevice.h>
#if defined(CONFIG_11ac_throughput_patch_from_412L07)
#if defined(CONFIG_MIPS_BRCM)
enum {
    BREVT_IF_CHANGED,
    BREVT_STP_STATE_CHANGED
};

struct stpPortInfo {
    char portName[IFNAMSIZ];
    unsigned char stpState;
};
extern struct net_device *bridge_get_next_port(char *brName, unsigned int *portNum);
extern int register_bridge_notifier(struct notifier_block *nb);
extern int unregister_bridge_notifier(struct notifier_block *nb);
extern int register_bridge_stp_notifier(struct notifier_block *nb);
extern int unregister_bridge_stp_notifier(struct notifier_block *nb);
#endif /* CONFIG_MIPS_BRCM */
#endif

extern void brioctl_set(int (*ioctl_hook)(struct net *, unsigned int, void __user *));
extern struct sk_buff *(*br_handle_frame_hook)(struct net_bridge_port *p,
					       struct sk_buff *skb);
extern int (*br_should_route_hook)(struct sk_buff *skb);

#endif

#endif
