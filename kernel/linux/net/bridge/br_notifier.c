#include <linux/capability.h>
#include <linux/kernel.h>
#include <linux/if_bridge.h>
#include <linux/netdevice.h>
#include <linux/slab.h>
#include <linux/times.h>
#include <net/net_namespace.h>
#include <asm/uaccess.h>
#include "br_private.h"

static RAW_NOTIFIER_HEAD(bridge_stp_event_chain);

void br_stp_notify_state_port(const struct net_bridge_port *p)
{
	struct stpPortInfo portInfo;
	if ( BR_NO_STP != p->br->stp_enabled )
	{
		memcpy(&portInfo.portName[0], p->dev->name, IFNAMSIZ);
		portInfo.stpState = p->state;
		raw_notifier_call_chain(&bridge_stp_event_chain, BREVT_STP_STATE_CHANGED, &portInfo);
	}
}

void br_stp_notify_state_bridge(const struct net_bridge *br)
{
	struct net_bridge_port *p;
	struct stpPortInfo portInfo;

	rcu_read_lock();
	list_for_each_entry_rcu(p, &br->port_list, list) {
		if ( BR_NO_STP == br->stp_enabled )
		{
			portInfo.stpState = 0xFF; /* disable */
		}
		else
		{
			portInfo.stpState = p->state;
		}
		memcpy(&portInfo.portName[0], p->dev->name, IFNAMSIZ);
		raw_notifier_call_chain(&bridge_stp_event_chain, BREVT_STP_STATE_CHANGED, &portInfo);
	}
	rcu_read_unlock();

}

int register_bridge_stp_notifier(struct notifier_block *nb)
{
    return raw_notifier_chain_register(&bridge_stp_event_chain, nb);
}
EXPORT_SYMBOL(register_bridge_stp_notifier);

int unregister_bridge_stp_notifier(struct notifier_block *nb)
{
    return raw_notifier_chain_unregister(&bridge_stp_event_chain, nb);
}
EXPORT_SYMBOL(unregister_bridge_stp_notifier);


