/* Used by ebt_AUTOMAP.c, MitraStar Jeff, 20110114*/
#ifndef __LINUX_BRIDGE_EBT_AUTOMAP_H
#define __LINUX_BRIDGE_EBT_AUTOMAP_H

#define EBT_AUTOMAP_TARGET "AUTOMAP"

#define AUTOMAP_TYPE_8021P  0x1
#define AUTOMAP_TYPE_DSCP   0x2
#define AUTOMAP_TYPE_PKTLEN 0x4

#define DSCP_MASK_SHIFT   5
#define ETHERPRI_MARK_SHIFT   12

/*
Auto Priority Mapping Table


    	DSCP	|   Packet Length	| 802.1P	| Queue	|
   ---------------------------------------------
     			|				|   001	|	0	|
  			|				|		|		|
     			|				|   010	|	1	|
			|				|		|		|
     0x00		| 	>1100		|   000	|	2	|
			|				|		|		|
     0x08		|	250-1100	|   011	|	3	|
			|				|		|		|	
     0x10		|				|   100	|	4	|
			|				|		|		|
     0x18		|  	<250		|   101	|	5	|
  			|				|		|		|
     0x20,0x28	|				|   110	|	6	|
			|				|		|		|
     0x30,0x38	|				|   111	|	7	|
*/

unsigned short MapTable[2][8] = {
	/*DSCP, 802.1p ---> priority queue*/
	{2,3,4,5,6,6,7,7},
	{2,0,1,3,4,5,6,7},
};

/* target info */
struct ebt_automap_t_info {
	int type;	
	int marktable[8];
};

#endif

