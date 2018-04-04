/*
 *  ebt_ip
 *
 *	Authors:
 *	Bart De Schuymer <bart.de.schuymer@pandora.be>
 *
 *  April, 2002
 *
 *  Changes:
 *    added ip-sport and ip-dport
 *    Innominate Security Technologies AG <mhopf@innominate.com>
 *    September, 2002
 */

#ifndef __LINUX_BRIDGE_EBT_IP_H
#define __LINUX_BRIDGE_EBT_IP_H

#define EBT_IP_SOURCE 0x01
#define EBT_IP_DEST 0x02
#define EBT_IP_TOS 0x04
#define EBT_IP_PROTO 0x08
#define EBT_IP_SPORT 0x10
#define EBT_IP_DPORT 0x20
#define EBT_IP_DSCP  0x40  /* brcm */
#if 1 /* ZyXEL QoS, John (porting from MSTC) */
#define EBT_IP_LENGTH 0x80
#define EBT_IP_TCP_FLAGS 0x100
#define EBT_IP_DHCP_OPT60 0x200
#define EBT_IP_DHCP_OPT61 0x400
#define EBT_IP_DHCP_OPT77 0x800
#define EBT_IP_DHCP_OPT125 0x1000
#define EBT_IP_MASK (EBT_IP_SOURCE | EBT_IP_DEST | EBT_IP_TOS | EBT_IP_PROTO |\
 EBT_IP_SPORT | EBT_IP_DPORT | EBT_IP_DSCP | EBT_IP_LENGTH | EBT_IP_TCP_FLAGS |\
 EBT_IP_DHCP_OPT60 | EBT_IP_DHCP_OPT61 | EBT_IP_DHCP_OPT77 | EBT_IP_DHCP_OPT125)
#define DHCP_OPTION_MAX_LEN 556 /* IP header(20) + UDP header(8)+ DHCP header(528) */

#define DHCP_PADDING                            0x00
#define DHCP_VENDOR                             0x3c    /*option 60 */
#define DHCP_CLIENT_ID                          0x3d    /*option 61 */
#define DHCP_USER_CLASS_ID                      0x4d    /*option 77 */
#define DHCP_VENDOR_IDENTIFYING 				0x7d    /*option 125 */
#define DHCP_OPTION_OVER                        0x34
#define DHCP_END                                0xFF

#define OPTION_FIELD            0
#define FILE_FIELD              1
#define SNAME_FIELD             2



/* miscellaneous defines */
#define OPT_CODE 0
#define OPT_LEN 1
#define OPT_DATA 2

#define OPTION_MAC_ENTRY 32

/* each option data shift length */
#define DHCP_OPT_LEN_FIELD_LEN  1
#define DHCP_OPT125_ENTERPRISE_NUM_LEN 4
#define DHCP_OPT125_DATA_SHIFT DHCP_OPT125_ENTERPRISE_NUM_LEN + DHCP_OPT_LEN_FIELD_LEN

#else 
#define EBT_IP_MASK (EBT_IP_SOURCE | EBT_IP_DEST | EBT_IP_TOS | EBT_IP_PROTO |\
 EBT_IP_SPORT | EBT_IP_DPORT | EBT_IP_DSCP )
#endif
#define EBT_IP_MATCH "ip"

/* the same values are used for the invflags */
#if 1 /* ZyXEL QoS, John (porting from MSTC) */
struct cfgopt{
        uint8_t len;
        char cfgdata[254];
};

struct dhcpMessage {
        uint8_t op;
        uint8_t htype;
        uint8_t hlen;
        uint8_t hops;
        uint32_t xid;
        uint16_t secs;
        uint16_t flags;
        uint32_t ciaddr;
        uint32_t yiaddr;
        uint32_t siaddr;
        uint32_t giaddr;
        uint8_t chaddr[16];
        uint8_t sname[64];
        uint8_t file[128];
        uint32_t cookie;
        uint8_t options[308]; /* 312 - cookie */
};

struct ebt_ip_info
{
	__be32 saddr;
	__be32 daddr;
	__be32 smsk;
	__be32 dmsk;
	uint8_t  tos;
	uint8_t  dscp; /* brcm */
	uint8_t  protocol;
	uint16_t  bitmask;
	uint16_t  invflags;
	uint8_t  tcp_flg_mask;
	uint8_t  tcp_flg_cmp;
	uint16_t sport[2];
	uint16_t dport[2];
	uint16_t length[2];
	struct cfgopt cfg60; //option 60
    struct cfgopt cfg61; //option 61
    struct cfgopt cfg77; //option 77
    struct cfgopt cfg125; //option 125
    char SrcMacArray[OPTION_MAC_ENTRY][ETH_ALEN];
};
#else
struct ebt_ip_info
{
	__be32 saddr;
	__be32 daddr;
	__be32 smsk;
	__be32 dmsk;
	uint8_t  tos;
	uint8_t  dscp; /* brcm */
	uint8_t  protocol;
	uint8_t  bitmask;
	uint8_t  invflags;
	uint16_t sport[2];
	uint16_t dport[2];
};
#endif

#endif
