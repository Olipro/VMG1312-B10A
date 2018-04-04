#ifndef __KLINK_UPDOWN_H__
#define __KLINK_UPDOWN_H__
#if 0
#include "zld-spec.h"
#endif
/****** SWITCH PORTS ******/
#ifndef MAX_PORTS
#define MAX_PORTS   8
#endif

#ifndef MIN_PORTS_IN_GROUP
#define MIN_PORTS_IN_GROUP  1
#endif

#if MAX_PORTS < MIN_PORTS_IN_GROUP
#error MAX_PORTS must >= MIN_PORTS_IN_GROUP
#endif

#if MAX_PORTS % MIN_PORTS_IN_GROUP != 0
#define MAX_GROUPS  ((MAX_PORTS / MIN_PORTS_IN_GROUP) + 1)
#else
#define MAX_GROUPS  (MAX_PORTS / MIN_PORTS_IN_GROUP)
#endif

#if MAX_PORTS % 8 != 0
#define BITMAP_COUNT    (MAX_PORTS / 8 + 1)
#else
#define BITMAP_COUNT    (MAX_PORTS / 8)
#endif

#define BITMAP_SET_PORT(bitmap, port)   (bitmap[port / 8] |= (0x80 >> (port % 8)))
#define BITMAP_ISSET_PORT(bitmap, port) ((bitmap[port / 8] & (0x80 >> (port % 8))) != 0)
#define BITMAP_UNSET_PORT(bitmap, port) (bitmap[port / 8] &= ~(0x80 >> (port % 8)))
#define FIBER_PRESENT   1
#define COPPER_PRESENT  0

/****** GENERAL******/
enum extension_card_type {

#if 1 /* For Telefonica 3G WAN backup, __TELEFONICA__, MitraStar Chehuai, 20110617 */
	EXT_CARD_PCMCIA = 0,
	EXT_CARD_USB = 1,
	EXT_CARD_MINPCI = 2,
	/*do not touch*/
	EXT_CARD_TYPE_MAX,
#else
	EXT_CARD_PCMCIA = -1,
	EXT_CARD_USB = 0,
	EXT_CARD_MINPCI
#endif
};
#if 1 /* For Telefonica 3G WAN backup, __TELEFONICA__, MitraStar Chehuai, 20110617 */
#define CELL_SLOT_PER_TYPE 	2

#define CELL_IF_INDEX_WEIGHT_PER_TYPE 	10
#define IF_INDEX(type, slot)	((type)*CELL_IF_INDEX_WEIGHT_PER_TYPE + (slot) + 1)
#else
#if 0
#define IF_INDEX(type, slot)	(type + slot + 1)
#endif
#define IF_INDEX(type, slot)	(type + 1)
#endif

/****** CELLULAR *****/
enum cell_enum_card_event {
	CELL_CARD_INSERTION = 0,
	CELL_CARD_REMOVAL
};

struct lkud_cell_event_t {
	unsigned int	event:2, /* Event Type */
					card_type:2, /* Extension Card Type */
					card_slot:2, /* PCMCIA Card or USB Slot */
					ndev:2; /* Number of Device */
	unsigned int	card_manfid:16, /* Card Manufacturer ID */
					card_prodid:16; /* Card Product ID */
	unsigned char	dev_name[4][32]; /* Device Name */
#if 1 /* For Telefonica 3G WAN backup, __TELEFONICA__, MitraStar Chehuai, 20110617 */
	unsigned char	devpath[16]; /* Indentify different devices */
#endif
};

/************ WIRELESS *******/

enum wireless_enum_card_event {
	WIRELESS_CARD_INSERTION = 0,
	WIRELESS_CARD_REMOVAL
};

struct wireless_event_t {
	unsigned int    event:2, /* Event Type */
			card_type:2, /* Extension Card Type */
			card_slot:2, /* PCMCIA Card or USB Slot */
			ndev:2; /* Number of Device */
	unsigned int    sub_vendor_id:16; /* Card Sub Vender ID */
	unsigned int    sub_device_id:16; /* Card Sub Device ID */
	unsigned char   dev_name[4][32]; /* Device Name */
};

/****** NETLINK ******/
#define LINK_UPDOWN_NETLINK 29
#define LINK_UPDOWN_MCAST_GROUP 0xFFFF

typedef enum {
	PORT_CONFIG = 0,
	LINK_CHANGE,
	CELL_CARD_CHANGE,
	WIRELESS_CARD_CHANGE,
	ETHER_BASE_LINKUP
} link_updown_cmd;

typedef struct {
	unsigned int port_no;
	unsigned int group_no;
} port_belong_t ;

typedef struct {
	unsigned char	link_status[BITMAP_COUNT];
	unsigned char	link_control_map[BITMAP_COUNT];
} link_cntl;

typedef struct {
	int command;
	union {

		link_cntl lc;
		port_belong_t	port_config;          /* for PORT_CONFIG */
		struct lkud_cell_event_t	cell_event; /* for Cellular Card Event */
		struct wireless_event_t 	wireless_event;/*for Wireless Card Event*/
	} payload;
} link_updown_msg;

int lkud_notify_userspace(link_updown_msg *msg);

#endif
