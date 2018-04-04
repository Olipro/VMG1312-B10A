/*
 *	Ioctl handler
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

#include <linux/capability.h>
#include <linux/kernel.h>
#include <linux/if_bridge.h>
#include <linux/netdevice.h>
#include <linux/times.h>
#include <net/net_namespace.h>
#include <asm/uaccess.h>
#include "br_private.h"
#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BR_IGMP_SNOOP)
#include "br_igmp.h"
#endif // defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BR_IGMP_SNOOP)
#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BR_MLD_SNOOP)
#include "br_mld.h"
#endif //defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BR_MLD_SNOOP)
#if defined(CONFIG_MIPS_BRCM)
#include "br_fdb.h"
#if defined(CONFIG_11ac_throughput_patch_from_412L07)
#include "br_flows.h"
#endif
#endif

#if defined(CONFIG_11ac_throughput_patch_from_412L07)
#if defined(CONFIG_MIPS_BRCM)
#include <linux/bcm_log.h>
#if defined(CONFIG_BCM96828) && !defined(CONFIG_EPON_HGU)
extern int uni_uni_enabled;
#endif
#endif
#endif

/* called with RTNL */
static int get_bridge_ifindices(struct net *net, int *indices, int num)
{
	struct net_device *dev;
	int i = 0;

	for_each_netdev(net, dev) {
		if (i >= num)
			break;
		if (dev->priv_flags & IFF_EBRIDGE)
			indices[i++] = dev->ifindex;
	}

	return i;
}

/* called with RTNL */
static void get_port_ifindices(struct net_bridge *br, int *ifindices, int num)
{
	struct net_bridge_port *p;

	list_for_each_entry(p, &br->port_list, list) {
		if (p->port_no < num)
			ifindices[p->port_no] = p->dev->ifindex;
	}
}

/*
 * Format up to a page worth of forwarding table entries
 * userbuf -- where to copy result
 * maxnum  -- maximum number of entries desired
 *            (limited to a page for sanity)
 * offset  -- number of records to skip
 */
static int get_fdb_entries(struct net_bridge *br, void __user *userbuf,
			   unsigned long maxnum, unsigned long offset)
{
	int num;
	void *buf;
	size_t size;

	/* Clamp size to PAGE_SIZE, test maxnum to avoid overflow */
	if (maxnum > PAGE_SIZE/sizeof(struct __fdb_entry))
		maxnum = PAGE_SIZE/sizeof(struct __fdb_entry);

	size = maxnum * sizeof(struct __fdb_entry);

	buf = kmalloc(size, GFP_USER);
	if (!buf)
		return -ENOMEM;

	num = br_fdb_fillbuf(br, buf, maxnum, offset);
	if (num > 0) {
		if (copy_to_user(userbuf, buf, num*sizeof(struct __fdb_entry)))
			num = -EFAULT;
	}
	kfree(buf);

	return num;
}

#if defined(CONFIG_MIPS_BRCM)
static int add_fdb_entries(struct net_bridge *br, void __user *userbuf,
			   unsigned long maxnum, int ifindex)
{
	struct net_device *dev;
	unsigned char     *pMacAddr = NULL;
	unsigned char     *pMac = NULL;
	int                size;
	int                i;
	int                ret = 0;

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	dev = dev_get_by_index(dev_net(br->dev), ifindex);
	if (dev == NULL)
		return -EINVAL;

	size     = maxnum * ETH_ALEN;
	pMacAddr = kmalloc(size, GFP_KERNEL);
	if (!pMacAddr)
		return -ENOMEM;

	copy_from_user(pMacAddr, userbuf, size);

	pMac = pMacAddr;
	for ( i = 0; i < maxnum; i++ )
	{
		ret = br_fdb_adddel_static(br, dev->br_port, (const unsigned char *)pMac, 1);    
		pMac += ETH_ALEN;
	}

	kfree(pMacAddr);
   
	return ret;
}


static int delete_fdb_entries(struct net_bridge *br, void __user *userbuf,
			unsigned long maxnum, int ifindex)
{
	struct net_device *dev;
	unsigned char     *pMacAddr = NULL;
	unsigned char     *pMac = NULL;
	int                size;
	int                i;
	int                ret = 0;

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	dev = dev_get_by_index(dev_net(br->dev), ifindex);
	if (dev == NULL)
		return -EINVAL;

	size     = maxnum * ETH_ALEN;
	pMacAddr = kmalloc(size, GFP_KERNEL);
	if (!pMacAddr)
    {
        dev_put(dev);
		return -ENOMEM;
    }

	copy_from_user(pMacAddr, userbuf, size);

	pMac = pMacAddr;
	for ( i = 0; i < maxnum; i++ )
	{
		ret = br_fdb_adddel_static(br, dev->br_port, (const unsigned char *)pMac, 0);
		pMac += ETH_ALEN;
	}

	kfree(pMacAddr);

    dev_put(dev);

	return ret;
}


#if defined(CONFIG_11ac_throughput_patch_from_412L07)
static int set_flows(struct net_bridge *br, int rxifindex, int txifindex)
{
	struct net_device *rxdev, *txdev;
	int                ret = 0;

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	rxdev = dev_get_by_index(dev_net(br->dev), rxifindex);
	if (rxdev == NULL)
		return -EINVAL;

	txdev = dev_get_by_index(dev_net(br->dev), txifindex);
	if (txdev == NULL)
		return -EINVAL;

   br_flow_blog_rules(br, rxdev, txdev);

   dev_put(rxdev);
   dev_put(txdev);

	return ret;
}

struct net_device *bridge_get_next_port(char *brName, unsigned int *brPort)
{
    struct net_bridge_port *cp;
    struct net_bridge_port *np;
    struct net_bridge *br;
    struct net_device *dev;
    struct net_device *prtDev;

    dev = dev_get_by_name(&init_net, brName);
    if(!dev)
        return NULL;

    br = netdev_priv(dev);
    rcu_read_lock();
    if (list_empty(&br->port_list))
    {
        rcu_read_unlock();
        dev_put(dev);
        return NULL;
    }

    if (*brPort == 0xFFFFFFFF)
    {
        np = list_first_entry(&br->port_list, struct net_bridge_port, list);
        *brPort = np->port_no;
        prtDev = np->dev;
    }
    else
    {
        cp = br_get_port(br, *brPort);
        if ( cp )
        {
           if (list_is_last(&cp->list, &br->port_list))
           {
               prtDev = NULL;
           }
           else
           {
              np = list_first_entry(&cp->list, struct net_bridge_port, list);
              *brPort = np->port_no;
              prtDev = np->dev;
           }
        }
        else
        {
           prtDev = NULL;
        }
    }

    rcu_read_unlock();
    dev_put(dev);
    return prtDev;
}
EXPORT_SYMBOL(bridge_get_next_port);

static RAW_NOTIFIER_HEAD(bridge_event_chain);
int register_bridge_notifier(struct notifier_block *nb)
{
    return raw_notifier_chain_register(&bridge_event_chain, nb);
}
EXPORT_SYMBOL(register_bridge_notifier);

int unregister_bridge_notifier(struct notifier_block *nb)
{
    return raw_notifier_chain_unregister(&bridge_event_chain, nb);
}
EXPORT_SYMBOL(unregister_bridge_notifier);
#endif
#endif

static int add_del_if(struct net_bridge *br, int ifindex, int isadd)
{
	struct net_device *dev;
	int ret;

	if (!capable(CAP_NET_ADMIN))
		return -EPERM;

	dev = dev_get_by_index(dev_net(br->dev), ifindex);
	if (dev == NULL)
		return -EINVAL;

	if (isadd)
		ret = br_add_if(br, dev);
	else
		ret = br_del_if(br, dev);

#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_11ac_throughput_patch_from_412L07)
	raw_notifier_call_chain(&bridge_event_chain, BREVT_IF_CHANGED, &br->dev->name);
#endif

	dev_put(dev);
	return ret;
}

/*
 * Legacy ioctl's through SIOCDEVPRIVATE
 * This interface is deprecated because it was too difficult to
 * to do the translation for 32/64bit ioctl compatability.
 */
static int old_dev_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	struct net_bridge *br = netdev_priv(dev);
	unsigned long args[4];

	if (copy_from_user(args, rq->ifr_data, sizeof(args)))
		return -EFAULT;

	switch (args[0]) {
	case BRCTL_ADD_IF:
	case BRCTL_DEL_IF:
		return add_del_if(br, args[1], args[0] == BRCTL_ADD_IF);

	case BRCTL_GET_BRIDGE_INFO:
	{
		struct __bridge_info b;

		memset(&b, 0, sizeof(struct __bridge_info));
		rcu_read_lock();
		memcpy(&b.designated_root, &br->designated_root, 8);
		memcpy(&b.bridge_id, &br->bridge_id, 8);
		b.root_path_cost = br->root_path_cost;
		b.max_age = jiffies_to_clock_t(br->max_age);
		b.hello_time = jiffies_to_clock_t(br->hello_time);
		b.forward_delay = br->forward_delay;
		b.bridge_max_age = br->bridge_max_age;
		b.bridge_hello_time = br->bridge_hello_time;
		b.bridge_forward_delay = jiffies_to_clock_t(br->bridge_forward_delay);
		b.topology_change = br->topology_change;
		b.topology_change_detected = br->topology_change_detected;
		b.root_port = br->root_port;

		b.stp_enabled = (br->stp_enabled != BR_NO_STP);
		b.ageing_time = jiffies_to_clock_t(br->ageing_time);
		b.hello_timer_value = br_timer_value(&br->hello_timer);
		b.tcn_timer_value = br_timer_value(&br->tcn_timer);
		b.topology_change_timer_value = br_timer_value(&br->topology_change_timer);
		b.gc_timer_value = br_timer_value(&br->gc_timer);
		rcu_read_unlock();

		if (copy_to_user((void __user *)args[1], &b, sizeof(b)))
			return -EFAULT;

		return 0;
	}

	case BRCTL_GET_PORT_LIST:
	{
		int num, *indices;

		num = args[2];
		if (num < 0)
			return -EINVAL;
		if (num == 0)
			num = 256;
		if (num > BR_MAX_PORTS)
			num = BR_MAX_PORTS;

		indices = kcalloc(num, sizeof(int), GFP_KERNEL);
		if (indices == NULL)
			return -ENOMEM;

		get_port_ifindices(br, indices, num);
		if (copy_to_user((void __user *)args[1], indices, num*sizeof(int)))
			num =  -EFAULT;
		kfree(indices);
		return num;
	}

	case BRCTL_SET_BRIDGE_FORWARD_DELAY:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		spin_lock_bh(&br->lock);
		br->bridge_forward_delay = clock_t_to_jiffies(args[1]);
		if (br_is_root_bridge(br))
			br->forward_delay = br->bridge_forward_delay;
		spin_unlock_bh(&br->lock);
		return 0;

	case BRCTL_SET_BRIDGE_HELLO_TIME:
	{
		unsigned long t = clock_t_to_jiffies(args[1]);
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		if (t < HZ)
			return -EINVAL;

		spin_lock_bh(&br->lock);
		br->bridge_hello_time = t;
		if (br_is_root_bridge(br))
			br->hello_time = br->bridge_hello_time;
		spin_unlock_bh(&br->lock);
		return 0;
	}

	case BRCTL_SET_BRIDGE_MAX_AGE:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		spin_lock_bh(&br->lock);
		br->bridge_max_age = clock_t_to_jiffies(args[1]);
		if (br_is_root_bridge(br))
			br->max_age = br->bridge_max_age;
		spin_unlock_bh(&br->lock);
		return 0;

	case BRCTL_SET_AGEING_TIME:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		br->ageing_time = clock_t_to_jiffies(args[1]);
		return 0;

	case BRCTL_GET_PORT_INFO:
	{
		struct __port_info p;
		struct net_bridge_port *pt;

		rcu_read_lock();
		if ((pt = br_get_port(br, args[2])) == NULL) {
			rcu_read_unlock();
			return -EINVAL;
		}

		memset(&p, 0, sizeof(struct __port_info));
		memcpy(&p.designated_root, &pt->designated_root, 8);
		memcpy(&p.designated_bridge, &pt->designated_bridge, 8);
		p.port_id = pt->port_id;
		p.designated_port = pt->designated_port;
		p.path_cost = pt->path_cost;
		p.designated_cost = pt->designated_cost;
		p.state = pt->state;
		p.top_change_ack = pt->topology_change_ack;
		p.config_pending = pt->config_pending;
		p.message_age_timer_value = br_timer_value(&pt->message_age_timer);
		p.forward_delay_timer_value = br_timer_value(&pt->forward_delay_timer);
		p.hold_timer_value = br_timer_value(&pt->hold_timer);

		rcu_read_unlock();

		if (copy_to_user((void __user *)args[1], &p, sizeof(p)))
			return -EFAULT;

		return 0;
	}

	case BRCTL_SET_BRIDGE_STP_STATE:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		br_stp_set_enabled(br, args[1]);
		return 0;

	case BRCTL_SET_BRIDGE_PRIORITY:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		spin_lock_bh(&br->lock);
		br_stp_set_bridge_priority(br, args[1]);
		spin_unlock_bh(&br->lock);
		return 0;

	case BRCTL_SET_PORT_PRIORITY:
	{
		struct net_bridge_port *p;
		int ret = 0;

		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		if (args[2] >= (1<<(16-BR_PORT_BITS)))
			return -ERANGE;

		spin_lock_bh(&br->lock);
		if ((p = br_get_port(br, args[1])) == NULL)
			ret = -EINVAL;
		else
			br_stp_set_port_priority(p, args[2]);
		spin_unlock_bh(&br->lock);
		return ret;
	}

	case BRCTL_SET_PATH_COST:
	{
		struct net_bridge_port *p;
		int ret = 0;

		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		if ((p = br_get_port(br, args[1])) == NULL)
			ret = -EINVAL;
		else
			br_stp_set_path_cost(p, args[2]);

		return ret;
	}

	case BRCTL_GET_FDB_ENTRIES:
		return get_fdb_entries(br, (void __user *)args[1],
				       args[2], args[3]);

#if defined(CONFIG_MIPS_BRCM)
	case BRCTL_ADD_FDB_ENTRIES:
		return add_fdb_entries(br, (void __user *)args[1],
				       args[2], args[3]);

	case BRCTL_DEL_FDB_ENTRIES:
		return delete_fdb_entries(br, (void __user *)args[1],
				       args[2], args[3]);
#if defined(CONFIG_11ac_throughput_patch_from_412L07)                   
	case BRCTL_SET_FLOWS:
		return set_flows(br, args[1], args[2]);
#endif
#endif
	}

	return -EOPNOTSUPP;
}

static int old_deviceless(struct net *net, void __user *uarg)
{
	unsigned long args[3];

	if (copy_from_user(args, uarg, sizeof(args)))
		return -EFAULT;

	switch (args[0]) {
	case BRCTL_GET_VERSION:
		return BRCTL_VERSION;

	case BRCTL_GET_BRIDGES:
	{
		int *indices;
		int ret = 0;

		if (args[2] >= 2048)
			return -ENOMEM;
		indices = kcalloc(args[2], sizeof(int), GFP_KERNEL);
		if (indices == NULL)
			return -ENOMEM;

		args[2] = get_bridge_ifindices(net, indices, args[2]);

		ret = copy_to_user((void __user *)args[1], indices, args[2]*sizeof(int))
			? -EFAULT : args[2];

		kfree(indices);
		return ret;
	}

	case BRCTL_ADD_BRIDGE:
	case BRCTL_DEL_BRIDGE:
	{
		char buf[IFNAMSIZ];

		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		if (copy_from_user(buf, (void __user *)args[1], IFNAMSIZ))
			return -EFAULT;

		buf[IFNAMSIZ-1] = 0;

		if (args[0] == BRCTL_ADD_BRIDGE)
			return br_add_bridge(net, buf);

		return br_del_bridge(net, buf);
	}
#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BR_IGMP_SNOOP)
	case BRCTL_ENABLE_SNOOPING:
	{
		char buf[IFNAMSIZ];
		struct net_device *dev;
		struct net_bridge *br;

		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		if (copy_from_user(buf, (void __user *)args[1], IFNAMSIZ))
			return -EFAULT;

		buf[IFNAMSIZ-1] = 0;

		dev = dev_get_by_name(&init_net, buf);
		if (dev == NULL) 
		    return  -ENXIO; 	/* Could not find device */
		
		br = netdev_priv(dev);
		br->igmp_snooping = args[2];
        dev_put(dev);

		return 0;
	}

	case BRCTL_ENABLE_PROXY_MODE:
	{
		char buf[IFNAMSIZ];
		struct net_device *dev;
		struct net_bridge *br;

		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		if (copy_from_user(buf, (void __user *)args[1], IFNAMSIZ))
			return -EFAULT;

		buf[IFNAMSIZ-1] = 0;

		dev = dev_get_by_name(&init_net, buf);
		if (dev == NULL) 
		    return  -ENXIO; 	/* Could not find device */
		
		br = netdev_priv(dev);
		br->igmp_proxy = args[2];

        dev_put(dev);

		return 0;
	}
#endif
#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BR_MLD_SNOOP)
	case BRCTL_MLD_ENABLE_SNOOPING:
	{
		char buf[IFNAMSIZ];
		struct net_device *dev;
		struct net_bridge *br;

		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		if (copy_from_user(buf, (void __user *)args[1], IFNAMSIZ))
			return -EFAULT;

		buf[IFNAMSIZ-1] = 0;

		dev = dev_get_by_name(&init_net, buf);
		if (dev == NULL) 
		    return  -ENXIO; 	/* Could not find device */
		
		br = netdev_priv(dev);
		br->mld_snooping = args[2];
#if defined(CONFIG_11ac_throughput_patch_from_412L08)
		if(br->mld_snooping==SNOOPING_DISABLED_MODE) 
			br_mcast_wl_flush(br) ;
#endif
        
        dev_put(dev);

		return 0;
	}

	case BRCTL_MLD_ENABLE_PROXY_MODE:
	{
		char buf[IFNAMSIZ];
		struct net_device *dev;
		struct net_bridge *br;

		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		if (copy_from_user(buf, (void __user *)args[1], IFNAMSIZ))
			return -EFAULT;

		buf[IFNAMSIZ-1] = 0;

		dev = dev_get_by_name(&init_net, buf);
		if (dev == NULL) 
		    return  -ENXIO; 	/* Could not find device */
		
		br = netdev_priv(dev);
		br->mld_proxy = args[2];

        
        dev_put(dev);

		return 0;
	}
#endif

#if defined(CONFIG_MIPS_BRCM)
	case BRCTL_ENABLE_IGMP_RATE_LIMIT:
	{
		char buf[IFNAMSIZ];
		struct net_device *dev;
		struct net_bridge *br;

		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		if (copy_from_user(buf, (void __user *)args[1], IFNAMSIZ))
		{
			return -EFAULT;
		}

		buf[IFNAMSIZ-1] = 0;

		dev = dev_get_by_name(&init_net, buf);
		if (dev == NULL)
		{
			return  -ENXIO; 	/* Could not find device */
		}

		if (args[2] > 500)
		{
			dev_put(dev);
			return  -EINVAL; 	/* Could not find device */
		}

		br = netdev_priv(dev);
		br->igmp_rate_limit       = args[2];
		br->igmp_rate_last_packet = ktime_set(0,0);
		br->igmp_rate_bucket      = 0;
      br->igmp_rate_rem_time    = 0;

		dev_put(dev);

		return 0;
	}
#if 0
defined(CONFIG_BCM96828) && !defined(CONFIG_EPON_HGU) 
    case BRCTL_SET_UNI_UNI_CTRL:
    {
        bcmFun_t *regFunc;
        BCM_EnetHandle_t param;
        BCM_EponHandle_t epon_param;
        char rxif[16], txif[16];
        int i, j, uniport_cnt = 0;
        char buf[IFNAMSIZ];
        struct net_device *dev, *rxdev, *txdev;
        struct net_bridge *br;

        uni_uni_enabled       = args[2];
        if ((regFunc = bcmFun_get(BCM_FUN_ID_ENET_HANDLE))) {
            param.type = BCM_ENET_FUN_TYPE_UNI_UNI_CTRL;
            param.enable = uni_uni_enabled;
            regFunc((void *)&param);
        }
        if ((regFunc = bcmFun_get(BCM_FUN_ID_EPON_HANDLE))) {
            epon_param.type = BCM_EPON_FUN_TYPE_UNI_UNI_CTRL;
            epon_param.enable = uni_uni_enabled;
            regFunc((void *)&epon_param);
        }

        if ((regFunc = bcmFun_get(BCM_FUN_ID_ENET_HANDLE))) {
            param.type = BCM_ENET_FUN_TYPE_GET_VPORT_CNT;
            regFunc((void *)&param);
            uniport_cnt = param.uniport_cnt;

            if (!capable(CAP_NET_ADMIN))
                return -EPERM;
            if (copy_from_user(buf, (void __user *)args[1], IFNAMSIZ))
                return -EFAULT;
            buf[IFNAMSIZ-1] = 0;
            dev = dev_get_by_name(&init_net, buf);
            if (dev == NULL) 
                return  -ENXIO;
            br = netdev_priv(dev);

            param.type = BCM_ENET_FUN_TYPE_GET_IF_NAME_OF_VPORT;
            for(i = 1; i <= uniport_cnt; i++) {
                param.port = i;
                regFunc((void *)&param);
                strcpy(rxif, param.name);
                strcat(rxif,".v0");
                rxdev = dev_get_by_name(&init_net, rxif);
                if (rxdev == NULL) 
                    return  -ENXIO;
                for(j = 1; j <= uniport_cnt; j++) {
                    if (i != j) {
                        param.port = j;
                        regFunc((void *)&param);
                        strcpy(txif, param.name);
                        strcat(txif,".v0");
                        txdev = dev_get_by_name(&init_net, txif);
                        if (txdev == NULL) 
                            return  -ENXIO;
                        if (uni_uni_enabled) {
                            br_flow_blog_rules(br, rxdev, txdev);
                        } else {
                            br_flow_path_delete(br, rxdev, txdev);
                        }
                        dev_put(txdev);
                    }
                }
                dev_put(rxdev);
            }

            dev_put(dev);
        }

        return 0;
    }
#endif
#endif
	}

	return -EOPNOTSUPP;
}

int br_ioctl_deviceless_stub(struct net *net, unsigned int cmd, void __user *uarg)
{
	switch (cmd) {
	case SIOCGIFBR:
	case SIOCSIFBR:
		return old_deviceless(net, uarg);

	case SIOCBRADDBR:
	case SIOCBRDELBR:
	{
		char buf[IFNAMSIZ];

		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		if (copy_from_user(buf, uarg, IFNAMSIZ))
			return -EFAULT;

		buf[IFNAMSIZ-1] = 0;
		if (cmd == SIOCBRADDBR)
			return br_add_bridge(net, buf);

		return br_del_bridge(net, buf);
	}
	}
	return -EOPNOTSUPP;
}

int br_dev_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	struct net_bridge *br = netdev_priv(dev);

	switch(cmd) {
	case SIOCDEVPRIVATE:
		return old_dev_ioctl(dev, rq, cmd);

	case SIOCBRADDIF:
	case SIOCBRDELIF:
		return add_del_if(br, rq->ifr_ifindex, cmd == SIOCBRADDIF);

	}

	pr_debug("Bridge does not support ioctl 0x%x\n", cmd);
	return -EOPNOTSUPP;
}
