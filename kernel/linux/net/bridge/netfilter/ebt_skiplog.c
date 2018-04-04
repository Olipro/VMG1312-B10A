/*
 *  ebt_skiplog
 */
#include <linux/module.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter_bridge/ebtables.h>
#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BLOG)
#include <linux/blog.h>
#endif

static unsigned int
ebt_skiplog_tg(struct sk_buff *skb, const struct xt_target_param *par)
{
#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BLOG)
	blog_skip(skb);
#endif

	return EBT_CONTINUE;
}

static struct xt_target ebt_skiplog_tg_reg __read_mostly = {
	.name		= "SKIPLOG",
	.revision	= 0,
	.family		= NFPROTO_BRIDGE,
	.target		= ebt_skiplog_tg,
	.me		= THIS_MODULE,
};

static int __init ebt_skiplog_init(void)
{
	return xt_register_target(&ebt_skiplog_tg_reg);
}

static void __exit ebt_skiplog_fini(void)
{
	xt_unregister_target(&ebt_skiplog_tg_reg);
}

module_init(ebt_skiplog_init);
module_exit(ebt_skiplog_fini);
MODULE_DESCRIPTION("Ebtables: SKIPLOG target");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Broadcom");
