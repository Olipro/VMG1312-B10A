#include <linux/version.h>
#include <linux/kernel.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
#include <linux/autoconf.h>
#else
#include <linux/config.h>
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0) */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <net/sock.h>
#include <linux/skbuff.h>
#include <linux/netlink.h>
#include <linux/klink_updown.h>

MODULE_AUTHOR("Kevin Chiang <KL.Chiang@zyxel.com.tw>");
/*MODULE_LICENSE("Proprietary");*/
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Link Up-Down Kernel Event Module");

/*******************************************************************************
* Local Variable Declaration
*******************************************************************************/
static struct sock *netlink_sock;

/*******************************************************************************
* Local Function Declaration
*******************************************************************************/

/*******************************************************************************
* External Variable Declaration
*******************************************************************************/

/*******************************************************************************
* External Function Declaration
*******************************************************************************/

/* Functions */
int
lkud_notify_userspace(link_updown_msg *msg)
{
	//printk("Jennifer, lkud_notify_userspace\n");
	int retval = 0;
	int msg_len = sizeof(link_updown_msg);
	struct sk_buff *skb = NULL;
	/* Check valid  */
	if( unlikely(!netlink_sock) ||
	    unlikely(!msg) ) {
		printk(KERN_ERR "%s:%d >> NULL link_updown_msg or netlink_sock\n", __FILE__, __LINE__);
		retval = -1;
		goto end;
	}
	/* Allocate SKB */
	skb = alloc_skb(msg_len, GFP_ATOMIC);
	if( unlikely(!skb) ) {
		printk(KERN_ERR "%s:%d >> Unable to alloc_skb()\n", __FILE__, __LINE__);
		retval = -1;
		goto end;
	}
	/* Copy Message into SKB */
#ifdef NET_SKBUFF_DATA_USES_OFFSET
	memcpy(skb_tail_pointer(skb), msg, msg_len);
#else
	memcpy(skb->tail, msg, msg_len);
#endif
	skb_put(skb, msg_len);
	/* Broadcast */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
	NETLINK_CB(skb).dst_group = 16; /* since LINK_UPDOWN_MCAST_GROUP = 0xffff */
	netlink_broadcast(netlink_sock, skb, 0, 16, GFP_ATOMIC);
#else
	NETLINK_CB(skb).dst_groups = LINK_UPDOWN_MCAST_GROUP;
	netlink_broadcast(netlink_sock, skb, 0, LINK_UPDOWN_MCAST_GROUP, GFP_ATOMIC);
#endif
end:
	return retval;
}
EXPORT_SYMBOL(lkud_notify_userspace);

static int __init
lkud_init_module(void)
{
	int retval = 0;
	/* Install Netlink for Link Up/Down notification */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,25)
	netlink_sock = netlink_kernel_create(&init_net, LINK_UPDOWN_NETLINK, LINK_UPDOWN_MCAST_GROUP, NULL, NULL, THIS_MODULE);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
	netlink_sock = netlink_kernel_create(LINK_UPDOWN_NETLINK, LINK_UPDOWN_MCAST_GROUP, NULL, THIS_MODULE);
#else
	netlink_sock = netlink_kernel_create(LINK_UPDOWN_NETLINK, NULL);
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0) */

	if( !netlink_sock ) {
		printk(KERN_ERR "%s:%d >> Cannot create link_updown kernel netlink event!\n", __FILE__, __LINE__);
		retval = -1;
	}

	return retval;
}

static void __exit
lkud_exit_module(void)
{
	/* Uninstall Netlink */
	if( netlink_sock ) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
		sock_release(netlink_sock->sk_socket);
#else
		sock_release(netlink_sock->socket);
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0) */
	}

	return;
}

module_init(lkud_init_module);
module_exit(lkud_exit_module);
