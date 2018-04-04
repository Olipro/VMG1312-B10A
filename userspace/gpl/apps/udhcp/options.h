/* options.h */
#ifndef _OPTIONS_H
#define _OPTIONS_H

#include "packet.h"
#include "leases.h"

#define TYPE_MASK	0x0F

enum {
	OPTION_IP=1,
	OPTION_IP_PAIR,
	OPTION_STRING,
	OPTION_BOOLEAN,
	OPTION_U8,
	OPTION_U16,
	OPTION_S16,
	OPTION_U32,
	OPTION_S32,
#if 1 // __ZYXEL__, Wood, for DHCP option 121
	OPTION_STATIC_ROUTES,
#endif
#if 1 // __ZyXEL__, Albert, 20131216, for DHCP option 33
	OPTION_STATIC_ROUTES_OP33
#endif
};

#define OPTION_LIST	0x80

// __ZyXEL__, Albert, 20131216, for DHCP option 120,33
struct dhcp_option {
	char name[16];
	char flags;
	unsigned char code;
};

extern struct dhcp_option options[];
extern int option_lengths[];

unsigned char *get_option(struct dhcpMessage *packet, int code);
int end_option(unsigned char *optionptr);
int add_option_string(unsigned char *optionptr, unsigned char *string);
int add_simple_option(unsigned char *optionptr, unsigned char code, u_int32_t data);
struct option_set *find_option(struct option_set *opt_list, char code);
void attach_option(struct option_set **opt_list, struct dhcp_option *option, char *buffer, int length);

// brcm
extern char session_path[];

// brcm
int createVIoption(int type, char *VIinfo);
int CreateOption125(int type, char *oui, char *sn, char *prod, char *VIinfo);
int CreateClntId(char *iaid, char *duid, char *out_op);
int saveVIoption(char *option, struct dhcpOfferedAddr *lease);

#define _PATH_RESOLV	 "/var/fyi/sys/dns"
#define _PATH_GW	 "/var/fyi/sys/gateway"
#define _PATH_SYS_DIR	 "/var/fyi/sys"

#define _PATH_WAN_DIR	"/proc/var/fyi/wan"
#define _PATH_MSG	"daemonstatus"
#define _PATH_IP	"ipaddress"
#define _PATH_MASK	"subnetmask"
#define _PATH_PID	"pid"
#if 1 // __ZyXEL__, Wood, for DHCP option 121
#define _PATH_STATIC_ROUTE "staticroute"
#endif

/* option header: 2 bytes + subcode headers (6) + option data (64*2+6);
   these are TR111 option data.    Option can be longer.  */
#define VENDOR_BRCM_ENTERPRISE_NUMBER     4413
#if 1  /* The enterprise number for ADSL Forum */ /* OPTION 125 */
#define VENDOR_ADSL_FORUM_ENTERPRISE_NUMBER    3561
#endif

#define VENDOR_ENTERPRISE_LEN             4    /* 4 bytes */
#define VENDOR_IDENTIFYING_INFO_LEN       142
#define VENDOR_IDENTIFYING_OPTION_CODE    125
#define VENDOR_OPTION_CODE_OFFSET         0
#define VENDOR_OPTION_LEN_OFFSET          1
#define VENDOR_OPTION_ENTERPRISE_OFFSET   2
#define VENDOR_OPTION_DATA_OFFSET         6
#define VENDOR_OPTION_DATA_LEN            1
#define VENDOR_OPTION_SUBCODE_LEN         1
#define VENDOR_SUBCODE_AND_LEN_BYTES      2

#if 1  /* The sub-option code (1~6) should comply with spec TR111 */ /* OPTION 125 */
#define VENDOR_DEVICE_OUI_SUBCODE            1
#define VENDOR_DEVICE_SERIAL_NUMBER_SUBCODE  2
#define VENDOR_DEVICE_PRODUCT_CLASS_SUBCODE  3
#define VENDOR_GATEWAY_OUI_SUBCODE           4
#define VENDOR_GATEWAY_SERIAL_NUMBER_SUBCODE 5
#define VENDOR_GATEWAY_PRODUCT_CLASS_SUBCODE 6

/* Additional customized sub-option code */
#define VENDOR_DEVICE_MODEL_NAME_SUBCODE     7
#define VENDOR_GATEWAY_MODEL_NAME_SUBCODE    8
#else
#define VENDOR_DEVICE_OUI_SUBCODE            1
#define VENDOR_DEVICE_PRODUCT_CLASS_SUBCODE  2
#define VENDOR_DEVICE_MODEL_NAME_SUBCODE     3
#define VENDOR_DEVICE_SERIAL_NUMBER_SUBCODE  4
#define VENDOR_GATEWAY_OUI_SUBCODE           5
#define VENDOR_GATEWAY_PRODUCT_CLASS_SUBCODE 6
#define VENDOR_GATEWAY_MODEL_NAME_SUBCODE    7
#define VENDOR_GATEWAY_SERIAL_NUMBER_SUBCODE 8
#endif
#define CLIENT_IDENTIFIER_IAID_INFO_LEN      4
#define CLIENT_IDENTIFIER_IAID_INFO_OFFSET   3
#define CLIENT_IDENTIFIER_TYPE		     255
#define CLIENT_IDENTIFIER_TYPE_LEN	     1
#define VENDOR_DUID_LLT			     1
#define VENDOR_DUID_EN			     2
#define VENDOR_DUID_LL			     3
#define VENDOR_HARDWARE_TYPE		     1

#define VENDOR_IDENTIFYING_FOR_GATEWAY       1
#define VENDOR_IDENTIFYING_FOR_DEVICE        2
#define VENDOR_GATEWAY_OUI_MAX_LEN           6
#define VENDOR_GATEWAY_SERIAL_NUMBER_MAX_LEN 64 
#define VENDOR_GATEWAY_PRODUCT_CLASS_MAX_LEN 64

#endif
