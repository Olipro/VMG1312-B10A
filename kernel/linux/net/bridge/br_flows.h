#ifndef _BR_FLOWS_H
#define _BR_FLOWS_H

#if defined(CONFIG_MIPS_BRCM)

extern int br_flow_blog_rules(struct net_bridge *br,
                              struct net_device *rxVlanDev_p,
                              struct net_device *txVlanDev_p);
                       
extern int br_flow_path_delete(struct net_bridge *br,
                               struct net_device *rxVlanDev_p,
                               struct net_device *txVlanDev_p);
                        
#endif /* CONFIG_MIPS_BRCM */

#endif /* _BR_FLOWS_H */

