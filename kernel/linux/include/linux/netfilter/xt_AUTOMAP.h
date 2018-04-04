/* x_tables module for setting the IPv4/IPv6 DSCP field
 *
 * (C) 2002 Harald Welte <laforge@gnumonks.org>
 * based on ipt_FTOS.c (C) 2000 by Matthew G. Marsh <mgm@paktronix.com>
 * This software is distributed under GNU GPL v2, 1991
 *
 * See RFC2474 for a description of the DSCP field within the IP Header.
 *
 * xt_DSCP.h,v 1.7 2002/03/14 12:03:13 laforge Exp
*/
#ifndef _XT_AUTOMAP_TARGET_H
#define _XT_AUTOMAP_TARGET_H

#define XT_AUTO_TYPE	0x1
//#define XT_AUTO_MARK	0x2
//#define XT_AUTO_DSCP	0x4
//#define XT_AUTO_ETHPRI	0x8

/*Automapping Type*/
#define AUTOMAP_TYPE_8021P	0x1
#define AUTOMAP_TYPE_DSCP		0x2
#define AUTOMAP_TYPE_PKTLEN	0x4

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
struct xt_automap_target_info {
	int type;	
	int marktable[8];
	int flags;
};

unsigned short  ipt_automap[2][8]={
		{0,0,0,1,2,3,4,7}, //802.1p mapping to DSCP
		{0,3,4,5,6,7,7,7},//DSCP mapping to 8021P
};

#endif /* _XT_AUTOMAP_TARGET_H */
