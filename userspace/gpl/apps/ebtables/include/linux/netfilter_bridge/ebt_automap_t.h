/* Used by ebt_AUTOMAP.c, MitraStar Jeff, 20110117*/
#ifndef __LINUX_BRIDGE_EBT_AUTOMAP_H
#define __LINUX_BRIDGE_EBT_AUTOMAP_H

#define EBT_AUTOMAP_TARGET "AUTOMAP"

#define AUTOMAP_TYPE_8021P  0x1
#define AUTOMAP_TYPE_DSCP   0x2
#define AUTOMAP_TYPE_PKTLEN 0x4

struct ebt_automap_t_info {
	int type;
	int marktable[8];
};
#endif
