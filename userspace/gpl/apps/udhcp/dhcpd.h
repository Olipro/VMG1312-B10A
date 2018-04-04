/* dhcpd.h */
#ifndef _DHCPD_H
#define _DHCPD_H

#include <netinet/ip.h>
#include <netinet/udp.h>

#include "libbb_udhcp.h"
#include "leases.h"

/************************************/
/* Defaults _you_ may want to tweak */
/************************************/

/* the period of time the client is allowed to use that address */
#define LEASE_TIME              (60*60*24*10) /* 10 days of seconds */

/* where to find the DHCP server configuration file */
#define DHCPD_CONF_FILE         "/etc/udhcpd.conf"

/*****************************************************************/
/* Do not modify below here unless you know what you are doing!! */
/*****************************************************************/

/* DHCP protocol -- see RFC 2131 */
#define SERVER_PORT		67
#define CLIENT_PORT		68

#define DHCP_MAGIC		0x63825363

/* DHCP option codes (partial list) */
#define DHCP_PADDING		0x00
#define DHCP_SUBNET		0x01
#define DHCP_TIME_OFFSET	0x02
#define DHCP_ROUTER		0x03
#define DHCP_TIME_SERVER	0x04
#define DHCP_NAME_SERVER	0x05
#define DHCP_DNS_SERVER		0x06
#define DHCP_LOG_SERVER		0x07
#define DHCP_COOKIE_SERVER	0x08
#define DHCP_LPR_SERVER		0x09
#define DHCP_HOST_NAME		0x0c
#define DHCP_BOOT_SIZE		0x0d
#define DHCP_DOMAIN_NAME	0x0f
#define DHCP_SWAP_SERVER	0x10
#define DHCP_ROOT_PATH		0x11
#define DHCP_IP_TTL		0x17
#define DHCP_MTU		0x1a
#define DHCP_BROADCAST		0x1c     //28
#define DHCP_STATIC_ROUTE_OP33		0x21    // __ZyXEL__, Albert, 20131216, for DHCP option 33
#define DHCP_NTP_SERVER		0x2a     //42
#if 1 /*__MSTC*/
#define DHCP_VSI  0x2b
#endif
#define DHCP_WINS_SERVER	0x2c    //44
#define DHCP_REQUESTED_IP	0x32    //50
#define DHCP_LEASE_TIME		0x33    //51
#define DHCP_OPTION_OVER	0x34    //52
#define DHCP_MESSAGE_TYPE	0x35    //53
#define DHCP_SERVER_ID		0x36    //54
#define DHCP_PARAM_REQ		0x37    //55  
#define DHCP_MESSAGE		0x38    //56
#define DHCP_MAX_SIZE		0x39    //57
#define DHCP_T1			0x3a        //58  
#define DHCP_T2			0x3b        //59  
#define DHCP_VENDOR		0x3c        //60  
#define DHCP_CLIENT_ID		0x3d    //61  
#define DHCP_SIP_DNAME_LIST_OP120	0x78    // __ZyXEL__, Albert, 20131216, for DHCP option 120
#if 1 // __ZyXEL__, Wood, for DHCP option 121
#define DHCP_STATIC_ROUTES	0x79
#endif
#define DHCP_VENDOR_IDENTIFYING	0x7d
#define DHCP_USER_CLASS_ID 0x4d
#define DHCP_6RD_OPT		0xd4
#if 1 //ZyXEL,Support DHCP Option 66, Albert
#define DHCP_OPTION_66          0x42
#endif
#if 1 //ZyXEL,Support DHCP Option 12, Ricky
#define DHCP_OPTION_12          0x0c
#endif
#if 1 //ZyXEL,Support DHCP Option 15, Ricky
#define DHCP_OPTION_15          0x0f
#endif


#define DHCP_END		0xFF


#define BOOTREQUEST		1
#define BOOTREPLY		2

#define ETH_10MB		1
#define ETH_10MB_LEN		6

#define DHCPDISCOVER		1
#define DHCPOFFER		2
#define DHCPREQUEST		3
#define DHCPDECLINE		4
#define DHCPACK			5
#define DHCPNAK			6
#define DHCPRELEASE		7
#define DHCPINFORM		8

#define BROADCAST_FLAG		0x8000

#define OPTION_FIELD		0
#define FILE_FIELD		1
#define SNAME_FIELD		2

/* miscellaneous defines */
#define TRUE			1
#define FALSE			0
#define MAC_BCAST_ADDR		"\xff\xff\xff\xff\xff\xff"
#define OPT_CODE 0
#define OPT_LEN 1

#if 1 //ZyXEL, ShuYing, enable or disable the DHCP server assign LAN IP by mac hash method.
int AssignIpByMacHash;
#endif

struct option_set {
	unsigned char *data;
	struct option_set *next;
};

//For static IP lease
struct static_lease {
	uint8_t *mac;
	uint32_t *ip;
	struct static_lease *next;
};

// BRCM
typedef struct vendor_id_struct{
	int len;
	char *id;
	struct vendor_id_struct * next;
}vendor_id_t;

#if 1 //__MSTC__, Lynn,DHCP
typedef struct mac_struct{
	int len;
	char *mac;
	struct mac_struct * next;
}mac_t;

typedef struct clnt_id_struct{
	int len;
	char *id;
	struct clnt_id_struct * next;
}clnt_id_t;

typedef struct vsi_struct{
	int len;
	char *vsi;
	struct vsi_struct * next;
}vsi_t;

typedef struct source_port_struct{
	int len;
	char *source_port;
	struct source_port_struct * next;
}source_port_t;
#endif


struct server_config_t {
	char remaining; 		/* should the lease file be interpreted as lease time remaining, or
			 		 * as the time the lease expires */
	unsigned long auto_time; 	/* how long should udhcpd wait before writing a config file.
					 * if this is zero, it will only write one on SIGUSR1 */
	unsigned long decline_time; 	/* how long an address is reserved if a client returns a
				    	 * decline message */
	unsigned long conflict_time; 	/* how long an arp conflict offender is leased for */
	unsigned long offer_time; 	/* how long an offered address is reserved */
	unsigned long min_lease; 	/* minimum lease a client can request*/
	char *lease_file;
	char *pidfile;
	char *notify_file;		/* What to run whenever leases are written */
	u_int32_t siaddr;		/* next server bootp option */
	char *sname;			/* bootp server name */
	char *boot_file;		/* bootp boot file option */

	// BRCM decline_file
	char *decline_file;
};	

struct iface_config_t {
	struct iface_config_t * next;	/* Next interface config in the list */
	int skt;			/* The socket on this interface */
	u_int32_t server;		/* Our IP, in network order */
	u_int32_t start;		/* Start address of leases, network order */
	u_int32_t end;			/* End of leases, network order */
	struct option_set *options;	/* List of DHCP options loaded from the config file */
	char *interface;		/* The name of the interface to use */
	int ifindex;			/* Index number of the interface to use */
	unsigned char arp[6];		/* Our arp address */
	unsigned long lease;		/* lease time in seconds (host order) */
	unsigned long max_leases; 	/* maximum number of leases (including reserved address) */
	unsigned long cnt_leases;	/* Only used when reading file */
	u_int32_t siaddr;		/* next server bootp option */
	char *sname;			/* bootp server name */
	char *boot_file;		/* bootp boot file option */
	struct dhcpOfferedAddr *leases;
	struct static_lease *static_leases; /*List of ip/mac pairs to assign static leases */
	vendor_id_t *vendor_ids;	/* vendor ID list */
#if 1//__MSTC__, Lynn,DHCP
	char *vendor_id_mode; 	/* vendor ID mode */
	mac_t *macs;
	clnt_id_t *clnt_ids;
	vsi_t *vsi;
	int filter;
#endif
	int decline;			/* Ignore DHCP requests if set */
#ifdef DHCP_RELAY
	u_int32_t relay_remote;		/* upper level dhcp server's IP address,
					   network order. */
	char relay_interface[32];	/* The name of the interface to use.
					   Empty means no relay on this 
					   interface group or the WAN interface
					   is not up yet. */
        int relayEnable; /* dhcp relay enabled or disabled*/
#endif
#if 1 //__MSTC__, Lynn,sync from SinJia, TR-098 DHCP Conditional Serving Pool
     char *enableDHCP;      /* Enable (Yes) /Disable (No) DHCP server .*/
     source_port_t *source_port;
     char *UsedIf;     /* The name of the interfac to use (for create sockets).*/
#endif
#ifdef MSTC_STATIC_DHCP_AUTO //__MSTC__, Lynn, Static DHCP automatically for STB
	vendor_id_t *opt60ForSTB;
#if 1 /*__ZyXEL__, David, support STB info configuration */
    vendor_id_t *opt43ForSTB;
#endif
#endif
#if 1 //ZyXEL, ShuYing, Support option 60 criteria in Bridge WAN, we need dhcpServerEnable info to decide whether to send offer.
    int dhcpServerEnable;
#endif

};

#ifdef DHCP_RELAY
/* Multiple interface groups may share the same relay or the same route to
 * their relays, so we use a separate relay list here */
struct relay_config_t {
	struct relay_config_t * next;	/* Next relay config in the list */
	char interface[32];		/* The name of the interface to use */
	int skt;			/* The socket with this relay */
};
#endif

// BRCM
/* vendor identifying option */
typedef struct vi_option_info {
  u_int32_t enterprise;
  char *oui;
  char *serialNumber;
  char *productClass;
  u_int32_t ipAddr;
  struct vi_option_info *next;
} VI_OPTION_INFO, *pVI_OPTION_INFO;

typedef struct viInfoList 
{
  int count;
  pVI_OPTION_INFO pHead;
  pVI_OPTION_INFO pTail;
} VI_INFO_LIST, *pVI_INFO_LIST;

#if 1 //__MSTC__, Lynn,DHCP
struct arpEntry
{
   u_int8_t chaddr[16];
   u_int32_t yiaddr;
   struct arpEntry *next;
};

extern struct arpEntry *cur_arp;
extern struct arpEntry *arp_head;
#endif

extern pVI_INFO_LIST viList;

extern struct server_config_t server_config;
// BRCM
extern struct dhcpOfferedAddr *declines;
extern struct iface_config_t *iface_config;
extern struct iface_config_t *cur_iface;
#ifdef DHCP_RELAY
extern struct relay_config_t *relay_config;
extern struct relay_config_t *cur_relay;
#endif
extern void *msgHandle;

void exit_server(int retval);
		
#endif
