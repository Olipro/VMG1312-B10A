/* dhcpd.h */
#ifndef _DHCPC_H
#define _DHCPC_H


#define INIT_SELECTING	0
#define REQUESTING	1
#define BOUND		2
#define RENEWING	3
#define REBINDING	4
#define INIT_REBOOT	5
#define RENEW_REQUESTED 6
#define RELEASED	7

#define IS_EMPTY_STRING(s)    ((s == NULL) || (*s == '\0'))

// __ZyXEL__, Wood, for DHCP option 121
// __ZyXEL__, Albert, 20131216, for DHCP option 33
// __ZyXEL__, Albert, 20131216, for DHCP option 120   
/* Paramaters the client should request from the server */
#define PARM_REQUESTS \
	DHCP_SUBNET, \
	DHCP_ROUTER, \
	DHCP_DNS_SERVER, \
	DHCP_HOST_NAME, \
	DHCP_DOMAIN_NAME, \
	DHCP_BROADCAST, \
	DHCP_STATIC_ROUTES, \
    DHCP_STATIC_ROUTE_OP33, \   
    DHCP_SIP_DNAME_LIST_OP120     


struct client_config_t {
	char foreground;		/* Do not fork */
	char quit_after_lease;		/* Quit after obtaining lease */
	char abort_if_no_lease;		/* Abort if no lease */
	char *interface;		/* The name of the interface to use */
	char *pidfile;			/* Optionally store the process ID */
	char *script;			/* User script to run at dhcp events */
	char *clientid;			/* Optional client id to use */
	char *hostname;			/* Optional hostname to use */
	int ifindex;			/* Index number of the interface to use */
	unsigned char arp[6];		/* Our arp address */
};

extern struct client_config_t client_config;

// brcm
extern int ipv6rd_opt;
extern int en_vendor_specific_info;
extern char vendor_class_id[];
#if 1 //__MSTC__,Lynn
extern char oui[];
extern char product_class[];
extern char model_name[];
extern char serial_number[];
#endif
extern char en_vendor_class_id;
extern char en_client_id;
extern char duid[];
extern char iaid[];
extern char en_125;
extern char oui_125[];
extern char sn_125[];
extern char prod_125[];
#endif
