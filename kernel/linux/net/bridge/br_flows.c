/*
 * Copyright 2011 Broadcom Corporation
 *
 * <:label-BRCM:2012:DUAL/GPL:standard
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2, as published by
 * the Free Software Foundation (the "GPL").
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * 
 * A copy of the GPL is available at http://www.broadcom.com/licenses/GPLv2.php, or by
 * writing to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 * 
 * :>
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/times.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/jhash.h>
#include <asm/atomic.h>
#include <linux/ip.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <linux/list.h>
#if defined(CONFIG_BLOG)
#include <linux/if_vlan.h>
#include <linux/blog.h>
#include <linux/blog_rule.h>
#endif
#include <linux/rtnetlink.h>
#include "br_private.h"


#if defined(CONFIG_BLOG)

int testid = 0;

static void free_ruleid_list(struct br_blog_rule_id *id_p);
static int init_blog_header(struct net_device *dev_p, BlogHeader_t *bh_p);
static struct br_blog_rule_id * activate_blog_rules(Blog_t *blog_p);
static struct br_flow_path * deactivate_blog_rules(struct br_flow_path *path_p,
                                                   struct net_device *rxVlanDev_p);


/*
 *------------------------------------------------------------------------------
 * Function:
 *   void free_ruleid_list(struct br_blog_rule_id *id_p)
 * Description:
 *   Free a blog rule id list.
 * Parameters:
 *   id_p (input): pointer to the blog rule id list.
 *------------------------------------------------------------------------------
 */
void free_ruleid_list(struct br_blog_rule_id *id_p)
{
   struct br_blog_rule_id *nextid_p;
   
   while (id_p != NULL)
   {
      nextid_p = id_p->next_p;
      
//      printk(KERN_NOTICE "%s free blog rule id 0x%x\n",__FUNCTION__, id_p->id);
      kfree(id_p);
      id_p = nextid_p;
   }
   
   return;
   
}  /* free_ruleid_list() */

/*
 *------------------------------------------------------------------------------
 * Function:
 *   int init_blog_header(struct net_device *dev_p, BlogHeader_t *bh_p)
 * Description:
 *   Initialize the blog header data structure of a blog for
 *   the given device (dev_p).
 * Parameters:
 *   dev_p  (input): pointer to net device.
 *   bh_p   (input): pointer to the blog header data structure.
 * Returns:
 *   0:  succeeded.
 *   -1: failed.
 *------------------------------------------------------------------------------
 */
int init_blog_header(struct net_device *dev_p, BlogHeader_t *bh_p)
{
   int ret = 0;
   
   /* find the root device */
   while (1)
   {
      if (netdev_path_is_root(dev_p))
         break;
      dev_p = netdev_path_next_dev(dev_p);
   }
      
	bh_p->dev_p = dev_p;
   
   bh_p->info.phyHdr =
      netdev_path_get_hw_port_type(dev_p) & BLOG_PHYHDR_MASK;
   
   switch (bh_p->info.phyHdrType)
   {
      case BLOG_ENETPHY:
         bh_p->info.channel = netdev_path_get_hw_port(dev_p);
         bh_p->info.bmap.BCM_SWC = 1;
      break;
      
      case BLOG_XTMPHY:
         bh_p->info.channel = netdev_path_get_hw_port(dev_p);
         bh_p->info.bmap.BCM_XPHY = 1;
      break;
      
      default:
		   printk(KERN_WARNING "%s phyHdrType %d is not supported\n",
                __FUNCTION__, bh_p->info.phyHdrType);
         ret = -1;
      break;
   }

   return ret;
   
}  /* init_blog_header() */

/*
 *------------------------------------------------------------------------------
 * Function:
 *   struct br_blog_rule_id * activate_blog_rules(Blog_t *blog_p)
 * Description:
 *   Activate blog rules of a layer2 flow blog.
 * Parameters:
 *   blog_p (input): pointer to the layer2 flow blog.
 * Returns:
 *   The list of activated blog rule ids.
 *------------------------------------------------------------------------------
 */
struct br_blog_rule_id * activate_blog_rules(Blog_t *blog_p)
{
   Blog_t                 *new_blog_p;
   blogRule_t             *rule_p      = NULL;
   blogRule_t             *n_rule_p    = NULL;
   blogRuleFilter_t       *rule_filter = NULL;
   struct br_blog_rule_id *ruleId_p    = NULL;
   struct br_blog_rule_id *id_p        = NULL;
   uint32_t               vid          = 0;
   uint32_t               key;

   if (!blog_p || !blog_p->blogRule_p)
      return NULL;

	new_blog_p = blog_get();
	if (new_blog_p == BLOG_NULL)
   {
		printk(KERN_WARNING "%s new_blog_p allocation failed\n",__FUNCTION__);
		return NULL;
	}

   /* get a copy of blog_p */
   blog_copy(new_blog_p, blog_p);

   /* activate blog rules one at a time */
   for (rule_p = blog_p->blogRule_p; rule_p; rule_p = rule_p->next_p)
   {
      /* allocate a rule id node */
	   id_p = kmalloc(sizeof(struct br_blog_rule_id), GFP_KERNEL);
      if (id_p == NULL)
      {
		   printk(KERN_WARNING "%s ruleid_p allocation failed\n",__FUNCTION__);
         break;
      }

      /* save pointer to the next blog rule */      
      n_rule_p = rule_p->next_p;
      
      /* terminate the current blog rule node */
      rule_p->next_p = NULL;

      /* assign the blog rule to the new blog */
      new_blog_p->blogRule_p = rule_p;

      /* update vlan tag info of the new blog based on the blog rule */
      rule_filter = &(((blogRule_t *)new_blog_p->blogRule_p)->filter);
      new_blog_p->vtag_num = rule_filter->nbrOfVlanTags;
      vid = ((rule_filter->vlan[0].value.h_vlan_TCI &
              rule_filter->vlan[0].mask.h_vlan_TCI) & 0xFFF);
      new_blog_p->vid  = vid ? vid : 0xFFFF; 
      vid = ((rule_filter->vlan[1].value.h_vlan_TCI &
              rule_filter->vlan[1].mask.h_vlan_TCI) & 0xFFF);
      new_blog_p->vid |= vid ? (vid << 16) : 0xFFFF0000;

      /* activate the new blog */
      key = blog_activate(new_blog_p, BlogTraffic_Layer2_Flow, BlogClient_fap);
      if (key == BLOG_KEY_INVALID)
      {
#if 0
         /* Some flows can be rejected. use these prints only for debugging! */
         printk(KERN_WARNING "%s blog_activate failed!\n",__FUNCTION__);
         blog_rule_dump(rule_p);
#endif
         kfree(id_p);
      }
      else
      {
         /* save the blog rule activation key */
         id_p->id     = key;  //++testid;
         id_p->next_p = ruleId_p;
         ruleId_p     = id_p;
         
//         printk(KERN_NOTICE "%s blog_activate succeeded. id=0x%x\n",__FUNCTION__, key);
      }

      /* restore pointer to the next blog rule */      
      rule_p->next_p = n_rule_p;
   }

   /* free the new blog */   
   blog_put(new_blog_p);
   
   return ruleId_p;
   
} /* activate_blog_rules() */

/*
 *------------------------------------------------------------------------------
 * Function:
 *   struct br_flow_path * deactivate_blog_rules(struct br_flow_path *path_p,
 *                                               struct net_device *rxVlanDev_p)
 * Description:
 *   Deactivate blog rules associated with a layer2 flow path.
 *   Note that activated blog rule ids were saved in the flow path list
 *   in the tx vlan device bridge port data structure.
 * Parameters:
 *   path_p (input): pointer to the flow path list of the tx bridge port.
 *   rxVlanDev_p (input): the rx vlan device.
 * Returns:
 *   pointer to the flow path if found.
 *   NULL if flow path not found.
 *------------------------------------------------------------------------------
 */
struct br_flow_path * deactivate_blog_rules(struct br_flow_path *path_p,
                                            struct net_device *rxVlanDev_p)
{
   struct br_blog_rule_id *id_p;
   
   while (path_p != NULL)
   {
      if (rxVlanDev_p == NULL || rxVlanDev_p == path_p->rxDev_p)
      {
         /* found the existing flow path. Deactivate all the old blog rules. */
         id_p = path_p->blogRuleId_p;
         
         while (id_p != NULL)
         {
            /* deactivate blog rule */
//            printk(KERN_NOTICE "%s deactivate blog rule id 0x%x\n",__FUNCTION__, id_p->id);
            blog_deactivate(id_p->id, BlogTraffic_Layer2_Flow, BlogClient_fap);
            id_p = id_p->next_p;
         }
         
         free_ruleid_list(path_p->blogRuleId_p);
         path_p->blogRuleId_p = NULL;
         if (rxVlanDev_p != NULL)
            break;
      }
      path_p = path_p->next_p;
   }
  
   return path_p;
   
}  /* deactivate_blog_rules() */

/*
 *------------------------------------------------------------------------------
 * Function:
 *   int br_flow_blog_rules(struct net_bridge *br,
 *                          struct net_device *rxVlanDev_p,
 *                          struct net_device *txVlanDev_p)
 * Description:
 *   Generate and activate blog rules for a layer2 flow path going
 *   from the rx vlan device to the tx vlan device of a bridge.
 * Parameters:
 *   br (input): the bridge that the rx and tx vlan devices are member of.
 *   rxVlanDev_p (input): rx vlan device 
 *   txVlanDev_p (input): tx vlan device 
 * Returns:
 *   0:  succeeded
 *   -1 or -EINVAL: failed
 *------------------------------------------------------------------------------
 */
int br_flow_blog_rules(struct net_bridge *br,
                       struct net_device *rxVlanDev_p,
                       struct net_device *txVlanDev_p)
{
   Blog_t                 *blog_p      = BLOG_NULL;
   struct br_blog_rule_id *newRuleId_p = NULL;
   struct br_flow_path    *path_p      = NULL;
   struct net_bridge_port *port_p      = NULL;
   int ret = 0;

   if (rxVlanDev_p == NULL || txVlanDev_p == NULL)
	{
   	printk(KERN_WARNING "%s rx or tx VLAN device not specified\n",__FUNCTION__);
      return -EINVAL;
   }
   
   port_p = rxVlanDev_p->br_port;
	if (port_p == NULL || port_p->br != br)
   {
      printk(KERN_WARNING "%s rx VLAN device is not a bridge member\n",__FUNCTION__);
		return -EINVAL;
   }
   
   port_p = txVlanDev_p->br_port;
	if (port_p == NULL || port_p->br != br)
   {
      printk(KERN_WARNING "%s tx VLAN device is not a bridge member\n",__FUNCTION__);
		return -EINVAL;
   }
   
   if (!(rxVlanDev_p->priv_flags & IFF_BCM_VLAN))
   {
      printk(KERN_WARNING "%s %s is NOT a VLAN device\n",__FUNCTION__, rxVlanDev_p->name);
      return -EINVAL;
   }

   if (!(txVlanDev_p->priv_flags & IFF_BCM_VLAN))
   {
      printk(KERN_WARNING "%s %s is NOT a VLAN device\n",__FUNCTION__, txVlanDev_p->name);
      return -EINVAL;
   }
   
   /* allocate blog */
   blog_p = blog_get();
   if (blog_p == BLOG_NULL) 
   {
		printk(KERN_WARNING "%s blog_p allocation failed\n",__FUNCTION__);
      return -1;
   }

   /* initialize the blog header for the rx vlan device */
   if (init_blog_header(rxVlanDev_p, &(blog_p->rx)) != 0)
   {
		printk(KERN_WARNING "%s init_blog_header for rxVlanDev_p failed\n",__FUNCTION__);
      blog_put(blog_p);
      return -1;
   }
   
   /* initialize the blog header for the tx vlan device */
   if (init_blog_header(txVlanDev_p, &(blog_p->tx)) != 0)
   {
		printk(KERN_WARNING "%s init_blog_header for txVlanDev_p failed\n",__FUNCTION__);
      blog_put(blog_p);
      return -1;
   }

   blog_p->mark = blog_p->priority = 0;

   //????   
//   blog_p->key.l1_tuple.phy     = blog_p->rx.info.phyHdr;
//   blog_p->key.l1_tuple.channel = blog_p->rx.info.channel;
//   blog_p->key.protocol         = BLOG_IPPROTO_UDP;

   blog_p->blogRule_p = NULL;

   /* add vlan blog rules, if any vlan interfaces were found */
   if (blogRuleVlanHook) 
   {
      if (blogRuleVlanHook(blog_p, rxVlanDev_p, txVlanDev_p) < 0)
      {
         printk(KERN_WARNING "%s Error while processing VLAN blog rules\n",__FUNCTION__);
         blog_rule_free_list(blog_p);
         blog_put(blog_p);
         return -1;
      }
   }

   /* activate new blog rules for flow path rxVlanDev -> txVlanDev */
   newRuleId_p = activate_blog_rules(blog_p);

   /* blog rule and blog are no longer needed. free them. */
   blog_rule_free_list(blog_p);
   blog_put(blog_p);

   /* deactivate the old blog rules of the same flow path.
    * old blog rule ids were saved in the flow path list
    * in the tx bridge port data structure.
    */
   port_p = txVlanDev_p->br_port;
   
   path_p = deactivate_blog_rules(port_p->flowPath_p, rxVlanDev_p);
   if (path_p == NULL)
   {
      /* did not find the old blog rule id list for flow path
       * rxVlanDev -> txVlanDev. Allocate a flow path for the
       * newly activated blog rule id list.
       */
      path_p = kmalloc(sizeof(struct br_flow_path), GFP_KERNEL);
      if (path_p == NULL)
      {
         printk(KERN_WARNING "%s kmalloc failed for new flow path\n",__FUNCTION__);
         free_ruleid_list(newRuleId_p);
         return -1;
      }
      
      path_p->rxDev_p    = rxVlanDev_p;
      path_p->next_p     = port_p->flowPath_p;
      port_p->flowPath_p = path_p;
   }
   
   /* save the newly activated blog rule id list */
   path_p->blogRuleId_p = newRuleId_p;

   return ret;
    
}  /* br_flow_blog_rules() */

/*
 *------------------------------------------------------------------------------
 * Function:
 *   int br_flow_path_delete(struct net_bridge *br,
 *                           struct net_device *rxVlanDev_p,
 *                           struct net_device *txVlanDev_p)
 * Description:
 *   Deactivate blog rules for a layer2 flow path going
 *   from the rx vlan device (rxVlanDev_p is not NULL) or
 *   from any rx vlan devices (rxVlanDev_p is NULL)
 *   to the tx vlan device of a bridge.
 * Parameters:
 *   br (input): the bridge that the rx and tx vlan devices are member of.
 *   rxVlanDev_p (input): rx vlan device 
 *   txVlanDev_p (input): tx vlan device 
 * Returns:
 *   0:  succeeded
 *   -EINVAL: failed
 *------------------------------------------------------------------------------
 */
int br_flow_path_delete(struct net_bridge *br,
                        struct net_device *rxVlanDev_p,
                        struct net_device *txVlanDev_p)
{
	struct net_bridge_port *port_p;
   struct br_flow_path    *prevPath_p = NULL;
   struct br_flow_path    *path_p     = NULL;
   
   if (rxVlanDev_p != NULL)
   {
      port_p = rxVlanDev_p->br_port;
	   if (port_p == NULL || port_p->br != br)
      {
         printk(KERN_WARNING "%s rx VLAN device is not a bridge member\n",__FUNCTION__);
		   return -EINVAL;
      }
   
      if (!(rxVlanDev_p->priv_flags & IFF_BCM_VLAN))
      {
         printk(KERN_WARNING "%s %s is NOT a VLAN device\n",__FUNCTION__, rxVlanDev_p->name);
         return -EINVAL;
      }
   }
   
   if (txVlanDev_p == NULL)
	{
   	printk(KERN_WARNING "%s tx VLAN device not specified\n",__FUNCTION__);
      return -EINVAL;
   }
   
   port_p = txVlanDev_p->br_port;
	if (port_p == NULL || port_p->br != br)
   {
      printk(KERN_WARNING "%s tx VLAN device is not a bridge member\n",__FUNCTION__);
		return -EINVAL;
   }
   
   if (!(txVlanDev_p->priv_flags & IFF_BCM_VLAN))
   {
      printk(KERN_WARNING "%s %s is NOT a VLAN device\n",__FUNCTION__, txVlanDev_p->name);
      return -EINVAL;
   }
   
   /* deactivate all the blog rules of the flow path.
    * old blog rule ids were saved in the flow path list
    * in the tx bridge port data structure.
    */
   port_p = txVlanDev_p->br_port;
   
   deactivate_blog_rules(port_p->flowPath_p, rxVlanDev_p);
   
   /* now, clean up flow paths that do not have any blog rule */
   path_p = port_p->flowPath_p;
   while (path_p != NULL)
   {
      if (path_p->blogRuleId_p == NULL)
      {
         if (path_p == port_p->flowPath_p)
         {
            port_p->flowPath_p = path_p->next_p;
            kfree(path_p);
            path_p = port_p->flowPath_p;
         }
         else
         {
            prevPath_p->next_p = path_p->next_p;
            kfree(path_p);
            path_p = prevPath_p->next_p;
         }
      }
      else
      {
         prevPath_p = path_p;
         path_p = path_p->next_p; 
      }
   }
   
   return 0;
      
}  /* br_flow_path_delete() */

#else

int br_flow_blog_rules(struct net_bridge *br,
                       struct net_device *rxVlanDev_p,
                       struct net_device *txVlanDev_p)
{
   return -1;
}  /* br_flow_blog_rules() */

int br_flow_path_delete(struct net_bridge *br,
                        struct net_device *rxVlanDev_p,
                        struct net_device *txVlanDev_p)
{
   return -1;
}  /* br_flow_path_delete() */

#endif
