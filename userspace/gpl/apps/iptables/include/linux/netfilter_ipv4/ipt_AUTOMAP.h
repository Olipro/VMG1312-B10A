/* iptables module for setting the IPv4 AUTOMAP field
 *
 * ipt_AUTOMAP.h,v 1.1 2010/01/19 
*/
#ifndef _IPT_AUTOMAP_TARGET_H
#define _IPT_AUTOMAP_TARGET_H
#include <linux/netfilter/xt_AUTOMAP.h>

#define IPT_AUTO_TYPE	XT_AUTO_TYPE
//#define IPT_AUTO_MARK	XT_AUTO_MARK
//#define IPT_AUTO_DSCP	XT_AUTO_DSCP
//#define IPT_AUTO_ETHPRI	XT_AUTO_ETHPRI

#define ipt_automap_target_info xt_automap_target_info

#endif /* _IPT_AUTOMAP_TARGET_H */
