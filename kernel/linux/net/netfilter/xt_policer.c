/* Kernel module to control the rate in kbps. */
/* This program is free software; you can redistribute it and/or modify
 *  * it under the terms of the GNU General Public License version 2 as
 *   * published by the Free Software Foundation. */
/* ZyXEL Birken, 20100107. */

#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter/xt_policer.h>
#if 1 //__MSTC__, Eric, Qos policer.
#include "skb_defines.h"
#define RED		1
#define YELLOW	2
#define GREEN 	3
#endif
#if 1//__MSTC__, Jones For compilation
#define MODE_TBF   0
#define MODE_SRTCM 1
#define MODE_TRTCM 2
#endif

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Herve Eychenne <rv@wallfire.org>");
MODULE_DESCRIPTION("iptables rate policer match");
MODULE_ALIAS("ipt_policer");
MODULE_ALIAS("ip6t_policer");

/* The algorithm used is the Simple Token Bucket Filter (TBF)
 *  * see net/sched/sch_tbf.c in the linux source tree. */

static DEFINE_SPINLOCK(policer_lock);

#if 0 //__MSTC__, richard, QoS
static int 
ipt_policer_match(const struct sk_buff *skb,
                  const struct net_device *in,
                  const struct net_device *out,
                  const struct xt_match *match,
                  const void *matchinfo,
                  int offset,
                  unsigned int protoff,
                  int *hotdrop)
#else
static bool ipt_policer_match(const struct sk_buff *skb, const struct xt_match_param *par)
#endif //__MSTC__, richard, QoS
{
#if 1//__MSTC__, Jones For compilation
	struct xt_policerinfo *r = (struct xt_policerinfo *)par->matchinfo;
	unsigned long now = jiffies;
	unsigned long timePassed = 0;
	struct sk_buff *tmp;
	u_int32_t cost = 0;
	u_int32_t extraCredit = 0; 
	spin_lock_bh(&policer_lock);
	
#if 1 //__OBM__, Jones
#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BLOG)
				blog_skip((struct sk_buff *)skb);
#endif
#endif
	switch(r->policerMode) {
	/* Token Bucket Filter (tbf) mode */
	/* The algorithm used is the Simple Token Bucket Filter (TBF)
	   see net/sched/sch_tbf.c in the linux source tree. */
	case MODE_TBF: 
		r->credit += (now - xchg(&r->prev, now)) * r->rate; /* Add TBF cerdit */ 
		if (r->credit > r->creditCap) {
			r->credit = r->creditCap;
		}
		cost = (skb->len + skb->mac_len) * BITS_PER_BYTE;
		if (r->credit >= cost) {
			/* We're not limited. (Match) */
			r->credit -= cost;						        /* Take out credit */ 	
			spin_unlock_bh(&policer_lock);
			return true;
			break;
		}
		/* We're limited. (Not Match) */
		spin_unlock_bh(&policer_lock);
		return false;
		break;

	/* Single Rate Three Color Marker (srTCM) Mode */
	case MODE_SRTCM:
		/* Add CBS first */
		r->credit += (now - xchg(&r->prev, now)) * r->rate; /* Add CBS cerdit */
		if (r->credit > r->creditCap) {
			extraCredit = r->credit - r->creditCap;
			r->credit = r->creditCap;
		}
		if (r->pbsCredit < r->pbsCreditCap && extraCredit > 0) {
			r->pbsCredit += extraCredit;                        /* Add EBS cerdit */
			if (r->pbsCredit > r->pbsCreditCap) {
				r->pbsCredit = r->pbsCreditCap;
			}
		}
		cost = (skb->len + skb->mac_len) * BITS_PER_BYTE;
		tmp = (struct sk_buff *)skb;
		if (r->credit >= cost) {
			tmp->mark &= ~(SKBMARK_POLICER_M); /* Reset 2 color bit */
			tmp->mark |= SKBMARK_SET_POLICER(0 , GREEN);    /* Green */
			r->credit -= cost;
		}
		else if (r->pbsCredit >= cost) {
			tmp->mark &= ~(SKBMARK_POLICER_M); /* Reset 2 color bit */
			tmp->mark |= SKBMARK_SET_POLICER(0 , YELLOW);    /* Yellow */
			r->pbsCredit -= cost;
		}
		else {
			tmp->mark &= ~(SKBMARK_POLICER_M); /* Reset 2 color bit */
			tmp->mark |= SKBMARK_SET_POLICER(0 , RED);    /* Red */
		}
		spin_unlock_bh(&policer_lock);
		return true;
		break;

	/* Two Rate Three Color Marker (srTCM) Mode */
	case MODE_TRTCM:
		timePassed = (now - xchg(&r->prev, now));
		r->credit += timePassed * r->rate;            /* Add CBS cerdit */
		r->pbsCredit += timePassed * r->pRate;        /* Add PBS cerdit */
		if (r->credit > r->creditCap) {
			r->credit = r->creditCap;
		}
		if (r->pbsCredit > r->pbsCreditCap) {
			r->pbsCredit = r->pbsCreditCap;
		}
		cost = (skb->len + skb->mac_len) * BITS_PER_BYTE;
		tmp = (struct sk_buff *)skb;
		if (r->pbsCredit < cost) {
			tmp->mark &= ~(SKBMARK_POLICER_M); /* Reset 2 color bit */
			tmp->mark |= SKBMARK_SET_POLICER(0 , RED);    /* Red */
		}
		else if (r->credit < cost) {
			tmp->mark &= ~(SKBMARK_POLICER_M); /* Reset 2 color bit */
			tmp->mark |= SKBMARK_SET_POLICER(0 , YELLOW);    /* Yellow */
			r->pbsCredit -= cost;
		}
		else {
			tmp->mark &= ~(SKBMARK_POLICER_M); /* Reset 2 color bit */
			tmp->mark |= SKBMARK_SET_POLICER(0 , GREEN);    /* Green */
			r->pbsCredit -= cost;
			r->credit -= cost;
		}
		spin_unlock_bh(&policer_lock);
		return true;
		break;

	default:
		return false;
	}
#else
#if 0 //__MSTC__, richard, QoS
	struct xt_policerinfo *r = ((struct xt_policerinfo *)matchinfo)->master;
#else
   struct xt_policerinfo *r = (struct xt_policerinfo *)par->matchinfo;
#endif //__MSTC__, richard, QoS

	unsigned long now = jiffies;
	spin_lock_bh(&policer_lock);
	r->credit += (now - xchg(&r->prev, now)) * r->avg; /* Add cerdit */ 
	if (r->credit > r->credit_cap) {
		r->credit = r->credit_cap;
	}	
	u_int32_t temp_cost = 0;
	temp_cost = (skb->len + skb->mac_len) * r->cost;
	if (r->credit >= temp_cost) {						
		/* We're not limited. */
		r->credit -= temp_cost;                        /* Take out credit */						
		spin_unlock_bh(&policer_lock);
#if 0 //__MSTC__, richard, QoS
      return 1;
#else
		return true;
		////return false;
#endif //__MSTC__, richard, QoS
	}
	spin_unlock_bh(&policer_lock);

#if 0 //__MSTC__, richard, QoS
   return 0;
#else
	return false;
	////return true;
#endif //__MSTC__, richard, QoS
#endif
}


#if 1//__MSTC__, Jones For compilation
/* Precision saver. */
/* As a policer rule added, this function will be executed */ 
static bool
ipt_policer_checkentry(const struct xt_mtchk_param *par)                       
{
       struct xt_policerinfo *r = (struct xt_policerinfo *)par->matchinfo;
	/* For SMP, we only want to use one set of counters. */
	r->master = r;

	/* pRate must be equal or greater than crate. */
	if (r->policerMode == 2) {
		if (r->rate > r->pRate) {
			return 0;
		}
	}
	
	if (r->creditCap == 0) { /* Check if policer initiate or not. */ 
		switch(r->policerMode) {
		case MODE_TBF:
			r->prev = jiffies;
			r->creditCap = r->burst * BITS_PER_BYTE * KILO_SCALE;       /* TBF Credits full */
			r->credit = r->creditCap;                                   /* TBF Credits full */		
			break;

		case MODE_SRTCM:
			r->prev = jiffies;
			r->creditCap = r->burst * BITS_PER_BYTE * KILO_SCALE;       /* CBS Credits full */
			r->credit = r->creditCap;                                   /* CBS Credits full */
			r->pbsCreditCap = r->pbsBurst * BITS_PER_BYTE * KILO_SCALE; /* EBS Credits full */
			r->pbsCredit = r->pbsCreditCap;                             /* EBS Credits full */
			break;
			
		case MODE_TRTCM:	
			r->prev = jiffies;
			r->creditCap = r->burst * BITS_PER_BYTE * KILO_SCALE;       /* CBS Credits full. */
			r->credit = r->creditCap;                                   /* CBS Credits full. */
			r->pbsCreditCap = r->pbsBurst * BITS_PER_BYTE * KILO_SCALE; /* PBS Credits full. */
			r->pbsCredit = r->pbsCreditCap;                             /* PBS Credits full. */ 
			break;

		default:
			return 0;				
		}
	}	
	return 1;
}
/* end ipt_policer_checkentry */
#else
/* Precision saver. */
/* As a policer rule added, this function will be executed */ 
#if 0 //__MSTC__, richard, QoS
static int
ipt_policer_checkentry(const char *tablename,
                       const void *inf,
                       const struct xt_match *match,
                       void *matchinfo,
                       unsigned int hook_mask)
#else
static bool
ipt_policer_checkentry(const struct xt_mtchk_param *par)
#endif //__MSTC__, richard, QoS
{
#if 0 //__MSTC__, richard, QoS
	struct xt_policerinfo *r = matchinfo;
#else
   struct xt_policerinfo *r = (struct xt_policerinfo *)par->matchinfo;
#endif //__MSTC__, richard, QoS

	/* For SMP, we only want to use one set of counters. */
	r->master = r;
	if (r->cost == 0) {
		r->prev = jiffies;
		r->credit_cap = r->burst * BITS_PER_BYTE * KILO_SCALE; /*Credits full.*/
		r->credit = r->credit_cap;                             /*Credits full.*/
		r->cost = BITS_PER_BYTE;			
	}
#if 0 //__MSTC__, richard, QoS
   return 1;
#else
	return true;
#endif //__MSTC__, richard, QoS
}
#endif

////#if 0 /* We do not know what this is for. Comment it temporarily. ZyXEL Birken, 20100107. */
#ifdef CONFIG_COMPAT
struct compat_xt_rateinfo {
	u_int32_t avg;
	u_int32_t burst;

	compat_ulong_t prev;
	u_int32_t credit;
	u_int32_t credit_cap, cost;

	u_int32_t master;
};

/* To keep the full "prev" timestamp, the upper 32 bits are stored in the
 *  * master pointer, which does not need to be preserved. */
static void compat_from_user(void *dst, void *src)
{
	struct compat_xt_rateinfo *cm = src;
	struct xt_policerinfo m = {
		.avg	             = cm->avg,
		.burst            = cm->burst,
		.prev             = cm->prev | (unsigned long)cm->master << 32,
		.credit           = cm->credit,
		.credit_cap     = cm->credit_cap,
		.cost             = cm->cost,
	};
	memcpy(dst, &m, sizeof(m));
}

static int compat_to_user(void __user *dst, void *src)
{
	struct xt_policerinfo *m = src;
	struct compat_xt_rateinfo cm = {
		.avg              = m->avg,
		.burst            = m->burst,
		.prev             = m->prev,
		.credit           = m->credit,
#if 1//__MSTC__, Jones For compilation
		.credit_cap     = m->creditCap,
#else
		.credit_cap     = m->credit_cap,
#endif		
		.cost             = m->cost,
		.master          = m->prev >> 32,
	};
	return copy_to_user(dst, &cm, sizeof(cm)) ? -EFAULT : 0;
}
#endif /* CONFIG_COMPAT */
////#endif

#if 0 //__MSTC__, richard, QoS
static struct xt_match xt_policer_match[] __read_mostly = {
    {
#else
static struct xt_match xt_policer_match __read_mostly = {
#endif //__MSTC__, richard, QoS
        .name              = "policer",
#if 0 //__MSTC__, richard, QoS
        .family            = AF_INET,
#else
        .family            = NFPROTO_UNSPEC,
#endif //__MSTC__, richard, QoS
        .checkentry        = ipt_policer_checkentry,
        .match             = ipt_policer_match,
        .matchsize         = sizeof(struct xt_policerinfo),
#ifdef CONFIG_COMPAT
        .compatsize        = sizeof(struct compat_xt_rateinfo),
        .compat_from_user  = compat_from_user,
        .compat_to_user    = compat_to_user,
#endif
        .me                = THIS_MODULE,
#if 0 //__MSTC__, richard, QoS
    },
    {
        .name              = "policer",
        .family            = AF_INET6,
        .checkentry        = ipt_policer_checkentry,
        .match             = ipt_policer_match,
        .matchsize         = sizeof(struct xt_policerinfo),
        .me                = THIS_MODULE,
    },
#endif //__MSTC__, richard, QoS
};

static int __init xt_policer_init(void) 
{
#if 0 //__MSTC__, richard, QoS
	return xt_register_matches(xt_policer_match, ARRAY_SIZE(xt_policer_match));
#else
   return xt_register_match(&xt_policer_match);
#endif //__MSTC__, richard, QoS
}

static void __exit xt_policer_fini(void)
{
#if 0 //__MSTC__, richard, QoS
	xt_unregister_matches(xt_policer_match, ARRAY_SIZE(xt_policer_match));
#else
   xt_unregister_match(&xt_policer_match);
#endif //__MSTC__, richard, QoS
}

module_init(xt_policer_init);
module_exit(xt_policer_fini);

