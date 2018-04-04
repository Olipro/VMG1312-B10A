#ifndef _BR_MCAST_H
#define _BR_MCAST_H

#include <linux/netdevice.h>
#include <linux/if_bridge.h>
#include <linux/igmp.h>
#include <linux/in.h>
#include "br_private.h"
#include <linux/if_vlan.h>
#include <linux/blog.h>
#include <linux/blog_rule.h>

#define MCPD_IF_TYPE_UNKWN      0
#define MCPD_IF_TYPE_BRIDGED    1
#define MCPD_IF_TYPE_ROUTED     2

typedef enum br_mcast_proto_type {
    BR_MCAST_PROTO_NONE,
    BR_MCAST_PROTO_IGMP,
    BR_MCAST_PROTO_MLD
} t_BR_MCAST_PROTO_TYPE;

#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BLOG)
void br_mcast_blog_release(t_BR_MCAST_PROTO_TYPE proto, void *mc_fdb);
void br_mcast_vlan_notify_for_blog_update(struct net_device *ndev,
                                   blogRuleVlanNotifyDirection_t direction,
                                   uint32_t nbrOfTags);
void br_mcast_handle_netdevice_events(struct net_device *ndev, unsigned long event);
int br_mcast_blog_process(struct net_bridge *br,
                            void *mc_fdb,
                            t_BR_MCAST_PROTO_TYPE proto);
#endif


#if defined(CONFIG_MIPS_BRCM) && (defined(CONFIG_BR_IGMP_SNOOP) || defined(CONFIG_BR_MLD_SNOOP)) && defined(CONFIG_11ac_throughput_patch_from_412L07)
#define PROXY_DISABLED_MODE 0
#define PROXY_ENABLED_MODE 1
#define SNOOPING_DISABLED_MODE 0
#define SNOOPING_ENABLED_MODE 1
#define SNOOPING_BLOCKING_MODE 2

#define MCPD_MAX_IFS            10
typedef struct mcpd_wan_info
{
	char                      if_name[IFNAMSIZ];
	int                       if_ops;
} t_MCPD_WAN_INFO;

typedef struct mcpd_igmp_snoop_entry
{
	char                      br_name[IFNAMSIZ];
	int                       port_no;
	struct                    in_addr grp;
	struct                    in_addr src;
	struct                    in_addr rep;
	int                       mode;
	int                       code;
	__u16                     tci;/* vlan id */
	t_MCPD_WAN_INFO           wan_info[MCPD_MAX_IFS];
} t_MCPD_IGMP_SNOOP_ENTRY;

typedef struct mcastCfg {
	int          mcastPriQueue;
} t_MCAST_CFG;

void br_mcast_set_pri_queue(int val);
int  br_mcast_get_pri_queue(void);
void br_mcast_set_skb_mark_queue(struct sk_buff *skb);
#endif

#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BR_MLD_SNOOP) && defined(CONFIG_11ac_throughput_patch_from_412L07)
#define SNOOPING_ADD_ENTRY 0
#define SNOOPING_DEL_ENTRY 1
#define SNOOPING_FLUSH_ENTRY 2
#define SNOOPING_FLUSH_ENTRY_ALL 3
int mcast_snooping_call_chain(unsigned long val,void *v);
void br_mcast_wl_flush(struct net_bridge *br) ;

typedef struct mcpd_mld_snoop_entry
{
	char                      br_name[IFNAMSIZ];
	char                      port_no;
	struct                    in6_addr grp;
	struct                    in6_addr src;
	struct                    in6_addr rep;
	int                       mode;
	int                       code;
	unsigned char             srcMac[6];
	__u16                     tci;/* vlan id */
	t_MCPD_WAN_INFO           wan_info[MCPD_MAX_IFS];
} t_MCPD_MLD_SNOOP_ENTRY;
#endif

#endif
