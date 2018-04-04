/* Kernel module to control the rate in kbps. */
/* This program is free software; you can redistribute it and/or modify 
 * it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation. */
/*  ZyXEL Stan, 20100105*/

#include <linux/module.h>
#if 1 // __MSTC__, ZyXEL richard, QoS
#include <linux/netfilter/x_tables.h>
#endif // __MSTC__, ZyXEL richard, QoS
#include <linux/netfilter_bridge/ebtables.h>
#include <linux/netfilter_bridge/ebt_policer.h>

#include <linux/netdevice.h>
#include <linux/spinlock.h>
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
static DEFINE_SPINLOCK(policer_lock);
#if 1//__MSTC__, Jones For compilation
static bool ebt_policer_match(const struct sk_buff *skb, const struct xt_match_param *par)
                             
{
       struct ebt_policer_info *r = (struct ebt_policer_info *)par->matchinfo;
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

		//printk(KERN_EMERG "111__skb->mark=%x\n\r", skb->mark);
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
		//printk(KERN_EMERG "222__skb->mark=%x\n\r", skb->mark);
			return true;
			break;
		}
		/* We're limited. (Not Match) */
		spin_unlock_bh(&policer_lock);
		//printk(KERN_EMERG "333__skb->mark=%x\n\r", skb->mark);
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
}
/* end ipt_policer_match */
#else
#if 0 // __MSTC__, ZyXEL richard, QoS
static int ebt_policer_match(const struct sk_buff *skb,
                             const struct net_device *in,
                             const struct net_device *out,
                             const void *data,
                             unsigned int datalen)
{
	struct ebt_policer_info *info = (struct ebt_policer_info *)data;
	unsigned long now = jiffies;

	spin_lock_bh(&policer_lock);
	info->credit += (now - xchg(&info->prev, now)) * info->avg; /* Add credit. */
	if (info->credit > info->credit_cap) {
		info->credit = info->credit_cap;
	}
	u_int32_t temp_cost = 0;
	temp_cost = (skb->len + skb->mac_len) * info->cost;
	if (info->credit >= temp_cost) {
		/* We're not limited. */
		info->credit -= temp_cost; /* Take out credit */
		spin_unlock_bh(&policer_lock);
		return EBT_MATCH;
	}

	spin_unlock_bh(&policer_lock);
	return EBT_NOMATCH;
}
#else
static bool ebt_policer_match(const struct sk_buff *skb, const struct xt_match_param *par)
{
	struct ebt_policer_info *info = par->matchinfo;
	unsigned long now = jiffies;

	spin_lock_bh(&policer_lock);
	info->credit += (now - xchg(&info->prev, now)) * info->avg; /* Add credit. */
	if (info->credit > info->credit_cap) {
		info->credit = info->credit_cap;
	}
   
	u_int32_t temp_cost;
	temp_cost = (skb->len + skb->mac_len) * info->cost;
	if (info->credit >= temp_cost) {
		/* We're not limited. */
		info->credit -= temp_cost; /* Take out credit */
		spin_unlock_bh(&policer_lock);
		return true;
		////return false;
	}

	spin_unlock_bh(&policer_lock);
   
	return false;
	////return true;
}
#endif // __MSTC__, ZyXEL richard, QoS
#endif

#if 1//__MSTC__, Jones For compilation
/* Precision saver. */
/* As a policer rule added, this function will be executed */ 
static bool ebt_policer_check(const struct xt_mtchk_param *par)
{
	struct ebt_policer_info *r = par->matchinfo;
	
	/* pRate must be equal or greater than crate. */
	if (r->policerMode == 2) {
		if (r->rate > r->pRate) {
			return false;	
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
			return false;				
		}
	}
	return true;;
}
#else
/* Precision saver. */
/* As a policer rule added, this function will be executed */ 
#if 0 // __MSTC__, ZyXEL richard, QoS
static int ebt_policer_check(const char *tablename, 
                             unsigned int hookmask,
                             const struct ebt_entry *e,
                             void *data,
                             unsigned int datalen)
{
	struct ebt_policer_info *info = (struct ebt_policer_info *)data;

	if (datalen != EBT_ALIGN(sizeof(struct ebt_policer_info))) {
		return -EINVAL;
	}
	
	info->prev = jiffies;
	info->credit_cap = info->burst * BITS_PER_BYTE * KILO_SCALE; /*Credits full.*/
	info->credit = info->burst * BITS_PER_BYTE * KILO_SCALE; /*Credits full.*/
	info->cost = BITS_PER_BYTE;
	
	return 0;
}
#else
static bool ebt_policer_check(const struct xt_mtchk_param *par)
{
	struct ebt_policer_info *info = par->matchinfo;

	/***if (par->datalen != EBT_ALIGN(sizeof(struct ebt_policer_info))) {
		return -EINVAL;
	}***/
	
	info->prev = jiffies;
	info->credit_cap = info->burst * BITS_PER_BYTE * KILO_SCALE; /*Credits full.*/
	info->credit = info->burst * BITS_PER_BYTE * KILO_SCALE; /*Credits full.*/
	info->cost = BITS_PER_BYTE;
	
	return true;
}
#endif // __MSTC__, ZyXEL richard, QoS
#endif

#if 0 // __MSTC__, ZyXEL richard, QoS
static struct ebt_match ebt_policer_reg =
#else
static struct xt_match ebt_policer_reg __read_mostly =
#endif
{
    .name = EBT_POLICER_MATCH,
#if 1 // __MSTC__, ZyXEL richard, QoS
    .revision	= 0,
    .family		= NFPROTO_BRIDGE,
    .match  = ebt_policer_match,
    .checkentry  = ebt_policer_check,
    .matchsize	= XT_ALIGN(sizeof(struct ebt_policer_info)),
#else
    .check  = ebt_policer_check,
    .match  = ebt_policer_match,
#endif // __MSTC__, ZyXEL richard, QoS
    .me     = THIS_MODULE,
};

static int __init ebt_policer_init(void)
{
#if 0 // __MSTC__, ZyXEL richard, QoS
    return ebt_register_match(&ebt_policer_reg);
#else
   return xt_register_match(&ebt_policer_reg);
#endif // __MSTC__, ZyXEL richard, QoS
}

static void __exit ebt_policer_fini(void)
{
#if 0 // __MSTC__, ZyXEL richard, QoS
    ebt_unregister_match(&ebt_policer_reg);
#else
   xt_unregister_match(&ebt_policer_reg);
#endif
}

module_init(ebt_policer_init);
module_exit(ebt_policer_fini);
MODULE_LICENSE("GPL");

