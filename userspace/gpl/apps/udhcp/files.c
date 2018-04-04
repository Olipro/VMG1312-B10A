/* 
 * files.c -- DHCP server file manipulation *
 * Rewrite by Russ Dill <Russ.Dill@asu.edu> July 2001
 */
 
#include <stdio.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/ether.h>
#include <ctype.h>

#include "cms_msg.h"
#include "debug.h"
#include "dhcpd.h"
#include "files.h"
#include "options.h"
#include "leases.h"
#include "static_leases.h"

#if 1 //__MSTC__,Lynn,DHCP
#include "cms_util.h"
#endif

/* enlarge retry interval/count, chchien */
//#define BRCM_RETRY_INTERVAL 1
//#define BRCM_RETRY_COUNT    3
#define BRCM_RETRY_INTERVAL 2
#define BRCM_RETRY_COUNT    9

extern int isblank(int c);

#ifdef DHCP_RELAY
static void register_message(CmsMsgType msgType);
#endif

typedef struct netiface {
	char nif_name[32];
	unsigned char nif_mac[6];
	unsigned int nif_index;
	in_addr_t nif_ip;
}netiface;

netiface *get_netifaces(int *count)
{
	netiface * netifaces = NULL;
	struct ifconf ifc;
	char buf[1024];
	int skt;
	int i;

	/* Create socket for querying interfaces */
	if ((skt = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		return NULL;

	/* Query available interfaces. */
	ifc.ifc_len = sizeof(buf);
	ifc.ifc_buf = buf;
	if(ioctl(skt, SIOCGIFCONF, &ifc) < 0)
		goto err;

	/* Allocate memory for netiface array */
	if (ifc.ifc_len < 1)
		goto err;
	*count = ifc.ifc_len / sizeof(struct ifreq);
	netifaces = calloc(*count, sizeof(netiface));
	if (netifaces == NULL)
		goto err;

	/* Iterate through the list of interfaces to retrieve info */
	for (i = 0; i < *count; i++) {
		struct ifreq ifr;
		ifr.ifr_addr.sa_family = AF_INET;
		strcpy(ifr.ifr_name, ifc.ifc_req[i].ifr_name);

		/* Interface name */
		strcpy(netifaces[i].nif_name, ifc.ifc_req[i].ifr_name);

		/* Interface index */
		if (ioctl(skt, SIOCGIFINDEX, &ifr))
			goto err;
		netifaces[i].nif_index = ifr.ifr_ifindex;

		/* IPv4 address */
		if (ioctl(skt, SIOCGIFADDR, &ifr))
			goto err;
		netifaces[i].nif_ip = ((struct sockaddr_in*)&ifr.ifr_addr)->
				sin_addr.s_addr;

		/* MAC address */
		if (ioctl(skt, SIOCGIFHWADDR, &ifr))
			goto err;
		memcpy(netifaces[i].nif_mac, ifr.ifr_hwaddr.sa_data, 6);

	}
	close(skt);
	return netifaces;
err:
	close(skt);
	if (netifaces)
		free(netifaces);
	return NULL;
}

/* on these functions, make sure you datatype matches */
static int read_ip(char *line, void *arg)
{
	struct in_addr *addr = arg;
	inet_aton(line, addr);
	return 1;
}

//For static IP lease
static int read_mac(const char *line, void *arg)
{
	uint8_t *mac_bytes = arg;
	struct ether_addr *temp_ether_addr;
	int retval = 1;

	temp_ether_addr = ether_aton(line);

	if(temp_ether_addr == NULL)
		retval = 0;
	else
		memcpy(mac_bytes, temp_ether_addr, 6);

	return retval;
}

static int read_str(char *line, void *arg)
{
	char **dest = arg;
	int i;
	
	if (*dest) free(*dest);

	*dest = strdup(line);
	
	/* elimate trailing whitespace */
	for (i = strlen(*dest) - 1; i > 0 && isspace((*dest)[i]); i--);
	(*dest)[i > 0 ? i + 1 : 0] = '\0';
	return 1;
}

#if 1 //__MSTC__,Lynn,DHCP:sync from SinJia, TR-098 DHCP Conditional Serving Pool
static source_port_t * read_sourcePortRules(char *line)
{
	char *token, tmp[BUFLEN_256];
	source_port_t *new, *header = NULL, *now;

	strcpy(tmp, line);
	token = strtok(line, ",");
	while (token != NULL) {
		new = malloc(sizeof(source_port_t));
		new->next = NULL;
		new->len = strlen(token);
		new->source_port = malloc(sizeof(char) * (new->len + 1));
		strcpy(new->source_port, token);
		if (header == NULL)
			header = now = new;
		else {
			now->next = new;
			now = new;
		}
		token = strtok(NULL, ",");
	}
	return header;
}
#endif


static int read_qstr(char *line, char *arg, int max_len)
{
	char * p = line;
	int quoted = 0;
	int len;
	
	if (*p == '\"') {
		quoted = 1;
		line++;
		p++;
	}
	
	while (*p) {
		if (*p == '\"' && quoted)
			break;
		else if (isspace(*p)) {
			if (!isblank(*p) || !quoted)
				break;
		}
		p++;
	}

	len = p - line;
	if (len >= max_len)
		len = max_len - 1;
	memcpy(arg, line, len);
	arg[len] = 0;
	
	return len;
}

#if 1 //__MSTC__,Lynn,DHCP
static vendor_id_t * read_op60Rules(char *line)
{
	char *token, tmp[BUFLEN_256];
	vendor_id_t *new, *header = NULL , *now = NULL;

	strcpy(tmp, line);
	token = strtok(line, ",");
	while (token != NULL) {
		new = malloc(sizeof(vendor_id_t));
		new->next = NULL;
		new->len = strlen(token);
		new->id = malloc(sizeof(char) * (new->len + 1));
		strcpy(new->id, token);
		if (header == NULL)
			header = now = new;
		else {
			now->next = new;
			now = new;
		}
		token = strtok(NULL, ",");
	}

	return header;
}

static mac_t * read_macRules(char *line)
{
	char *token, tmp[BUFLEN_256];
	mac_t *new, *header = NULL, *now;

	strcpy(tmp, line);
	token = strtok(line, ",");
	while (token != NULL) {
		new = malloc(sizeof(mac_t));
		new->next = NULL;
		new->len = strlen(token);
		new->mac = malloc(sizeof(char) * (new->len + 1));
		strcpy(new->mac, token);
		if (header == NULL)
			header = now = new;
		else {
			now->next = new;
			now = new;
		}
		token = strtok(NULL, ",");
	}

	return header;
}

static clnt_id_t * read_op61Rules(char *line)
{
	char *token, tmp[BUFLEN_256];
	clnt_id_t *new, *header = NULL, *now;

	strcpy(tmp, line);
	token = strtok(line, ",");
	while (token != NULL) {
		new = malloc(sizeof(clnt_id_t));
		new->next = NULL;
		new->len = strlen(token);
		new->id = malloc(sizeof(char) * (new->len + 1));
		strcpy(new->id, token);
		if (header == NULL)
			header = now = new;
		else {
			now->next = new;
			now = new;
		}
		token = strtok(NULL, ",");
	}

	return header;
}

static vsi_t * read_op125Rules(char *line)
{
	char *token, tmp[BUFLEN_256];
	vsi_t *new, *header = NULL, *now;

	strcpy(tmp, line);
	token = strtok(line, ",");
	while (token != NULL) {
		new = malloc(sizeof(vsi_t));
		new->next = NULL;
		new->len = strlen(token);
		new->vsi = malloc(sizeof(char) * (new->len + 1));
		strcpy(new->vsi, token);
		if (header == NULL)
			header = now = new;
		else {
			now->next = new;
			now = new;
		}
		token = strtok(NULL, ",");
	}

	return header;
}
#endif

static int read_u32(char *line, void *arg)
{
	u_int32_t *dest = arg;
	*dest = strtoul(line, NULL, 0);
	return 1;
}


static int read_yn(char *line, void *arg)
{
	char *dest = arg;
	if (!strcasecmp("yes", line) || !strcmp("1", line) || !strcasecmp("true", line))
		*dest = 1;
	else if (!strcasecmp("no", line) || !strcmp("0", line) || !strcasecmp("false", line))
		*dest = 0;
	else return 0;
	
	return 1;
}


/* read a dhcp option and add it to opt_list */
static int read_opt(char *line, void *arg)
{
	struct option_set **opt_list = arg;
	char *opt, *val;
	char fail;
	struct dhcp_option *option = NULL;
	int length = 0;
	char buffer[255];
	u_int16_t result_u16;
	int16_t result_s16;
	u_int32_t result_u32;
	int32_t result_s32;
	
	int i;
	
	if (!(opt = strtok(line, " \t="))) return 0;
	
	for (i = 0; options[i].code; i++)
		if (!strcmp(options[i].name, opt)) {
			option = &(options[i]);
			break;
		}
		
	if (!option) return 0;
	
	do {
#if 1 //__MSTC__,Lynn,DHCP
 /* Fix option does not support "," issue, __TELEFONICA__, ZyXEL YungAn, 20100927. */
                val = strtok(NULL, " \t");		
#else
		val = strtok(NULL, ", \t");
#endif
		if (val) {
			fail = 0;
			length = 0;
			switch (option->flags & TYPE_MASK) {
			case OPTION_IP:
				read_ip(val, buffer);
				break;
			case OPTION_IP_PAIR:
				read_ip(val, buffer);
				if ((val = strtok(NULL, ", \t/-")))
					read_ip(val, buffer + 4);
				else fail = 1;
				break;
			case OPTION_STRING:
				length = strlen(val);
				if (length > 254) length = 254;
				memcpy(buffer, val, length);
				break;
			case OPTION_BOOLEAN:
				if (!read_yn(val, buffer)) fail = 1;
				break;
			case OPTION_U8:
				buffer[0] = strtoul(val, NULL, 0);
				break;
			case OPTION_U16:
				result_u16 = htons(strtoul(val, NULL, 0));
				memcpy(buffer, &result_u16, 2);
				break;
			case OPTION_S16:
				result_s16 = htons(strtol(val, NULL, 0));
				memcpy(buffer, &result_s16, 2);
				break;
			case OPTION_U32:
				result_u32 = htonl(strtoul(val, NULL, 0));
				memcpy(buffer, &result_u32, 4);
				break;
			case OPTION_S32:
				result_s32 = htonl(strtol(val, NULL, 0));	
				memcpy(buffer, &result_s32, 4);
				break;
			default:
				break;
			}
			length += option_lengths[option->flags & TYPE_MASK];
			if (!fail)
				attach_option(opt_list, option, buffer, length);
		} else fail = 1;
	} while (!fail && option->flags & OPTION_LIST);
	return 1;
}

//For static IP lease
static int read_staticlease(const char *const_line, void *arg)
{

	char *line;
	char *mac_string;
	char *ip_string;
	uint8_t *mac_bytes;
	uint32_t *ip;


	/* Allocate memory for addresses */
	mac_bytes = xmalloc(sizeof(unsigned char) * 8);
	ip = xmalloc(sizeof(uint32_t));

	/* Read mac */
	line = (char *) const_line;
	mac_string = strtok(line, " \t");
	read_mac(mac_string, mac_bytes);

	/* Read ip */
	ip_string = strtok(NULL, " \t");
	read_ip(ip_string, ip);

	addStaticLease(arg, mac_bytes, ip);

#ifdef UDHCP_DEBUG
	printStaticLeases(arg);
#endif

	return 1;

}

static void release_iface_config(struct iface_config_t * iface)
{
	struct option_set *cur, *next;
	struct static_lease *sl_cur, *sl_next;
	vendor_id_t *vid_cur, *vid_next;
#if 1 //__MSTC__,Lynn,DHCP
	mac_t *mac_cur, *mac_next;
	clnt_id_t *clnt_cur, *clnt_next;
	vsi_t *vsi_cur, *vsi_next;
#endif

	if (iface->skt >= 0) {
		close(iface->skt);
		iface->skt = -1;
	}
	cur = iface->options;
	while(cur) {
		next = cur->next;
		if(cur->data)
			free(cur->data);
		free(cur);
		cur = next;
	}
	if (iface->interface)
		free(iface->interface);
	if (iface->sname)
		free(iface->sname);
	if (iface->boot_file)
		free(iface->boot_file);
	if (iface->leases)
		free(iface->leases);
	sl_cur = iface->static_leases;
	while(sl_cur) {
		sl_next = sl_cur->next;
		if(sl_cur->mac)
			free(sl_cur->mac);
		if(sl_cur->ip)
			free(sl_cur->ip);
		free(sl_cur);
		sl_cur = sl_next;
	}
	vid_cur = iface->vendor_ids;
	while(vid_cur) {
		vid_next = vid_cur->next;
		free(vid_cur);
		vid_cur = vid_next;
	}
#if 1 //__MSTC__,Lynn,DHCP
        iface->vendor_ids = NULL;
	mac_cur = iface->macs;
	while(mac_cur) {
		mac_next = mac_cur->next;
		free(mac_cur);
		mac_cur = mac_next;
	}
        iface->macs = NULL;
	clnt_cur = iface->clnt_ids;
	while(clnt_cur) {
		clnt_next = clnt_cur->next;
		free(clnt_cur);
		clnt_cur = clnt_next;
	}
        iface->clnt_ids = NULL;
	vsi_cur = iface->vsi;
	while(vsi_cur) {
		vsi_next = vsi_cur->next;
		free(vsi_cur);
		vsi_cur = vsi_next;
	}
        iface->vsi = NULL;
#endif
	free(iface);
}

static void set_server_config_defaults(void)
{
	server_config.remaining = 1;
	server_config.auto_time = 7200;
	server_config.decline_time = 3600;
	server_config.conflict_time = 3600;
	server_config.offer_time = 3600;
	server_config.min_lease = 60;
	if (server_config.lease_file)
		free(server_config.lease_file);
	server_config.lease_file = strdup("/etc/udhcpd.leases");
	if (server_config.pidfile)
		free(server_config.pidfile);
	server_config.pidfile = strdup("/var/run/udhcpd.pid");
	if (server_config.notify_file)
		free(server_config.notify_file);
	server_config.notify_file = NULL;
	if (server_config.decline_file)
		free(server_config.decline_file);
	server_config.decline_file = strdup("");
}

static int set_iface_config_defaults(void)
{
	int fd;
	struct ifreq ifr;
	struct sockaddr_in *sin;
	struct option_set *option;
	struct iface_config_t *iface;
	int retry_count;
	int local_rc;
	int foundBr = 0;
#if 1 //__MSTC__,Lynn,DHCP
	char *token = NULL;
#endif

	/* Create fd to retrieve interface info */
	if((fd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0) {
		LOG(LOG_ERR, "socket failed!");
		return 0;
	}

	for(iface = iface_config; iface; iface = iface->next) {
#if 1 //__MSTC__,Lynn,DHCP:sync from SinJia, TR-098 DHCP Conditional Serving Pool
      if (iface->interface == NULL || cmsUtl_strstr(iface->interface, "br") == 0 || cmsUtl_strcmp(iface->enableDHCP, "No") == 0)
#else
      if (iface->interface == NULL || strstr(iface->interface, "br") == 0)
#endif
			continue;

		/* Initialize socket to invalid */
		iface->skt = -1;
		/* Retrieve IP of the interface */
		/*
		 * BRCM begin: mwang: during startup, dhcpd is started by
		 * rcl_lanHostCfgObject, but br0 has not been created yet
		 * because that is done by rcl_lanIpIntfObject, which is
		 * called after rcl_lanHostCfgObject. So retry a few times
		 * to get br0 info before giving up.
		 */
		local_rc = -1;
		for (retry_count = 0; retry_count < BRCM_RETRY_COUNT;
			retry_count++) {
			ifr.ifr_addr.sa_family = AF_INET;
#if 1 //__MSTC__,Lynn,DHCP:sync from SinJia, TR-098 DHCP Conditional Serving Pool
                  strcpy(ifr.ifr_name, iface->UsedIf);
#else
                  strcpy(ifr.ifr_name, iface->interface);
#endif
			if ((local_rc = ioctl(fd, SIOCGIFADDR, &ifr)) == 0) {
				sin = (struct sockaddr_in *) &ifr.ifr_addr;
				iface->server = sin->sin_addr.s_addr;
				DEBUG(LOG_INFO, "server_ip(%s) = %s",
				ifr.ifr_name, inet_ntoa(sin->sin_addr));
				break;
			}
			sleep(BRCM_RETRY_INTERVAL);
		}
		if (local_rc < 0) {
#if 1 /* try one more SIOCGIFADDR again, chchien */
                        if ((local_rc = ioctl(fd, SIOCGIFADDR, &ifr)) == 0) {
                                        sin = (struct sockaddr_in *) &ifr.ifr_addr;
                                        iface->server = sin->sin_addr.s_addr;
                        }
                        else if(local_rc < 0 ){
                                LOG(LOG_ERR, "SIOCGIFADDR retry fail again on %s!---retry time = 10",
                                ifr.ifr_name);
                                break;
                        }
#else
			LOG(LOG_ERR, "SIOCGIFADDR failed on %s!",
				ifr.ifr_name);
			break;
#endif
		}

		/* Set default start and end if missing */
		if (iface->start == 0) {
			iface->start = (iface->server & htonl(0xffffff00)) |
				htonl(20);
		}
		if (iface->end == 0) {
			iface->end = (iface->server & htonl(0xffffff00)) |
				htonl(254);
		}

		/* Retrieve ifindex of the interface */
		if (ioctl(fd, SIOCGIFINDEX, &ifr) == 0) {
			DEBUG(LOG_INFO, "ifindex(%s)  = %d", ifr.ifr_name,
				ifr.ifr_ifindex);
			iface->ifindex = ifr.ifr_ifindex;
		} else {
			LOG(LOG_ERR, "SIOCGIFINDEX failed on %s!",
				ifr.ifr_name);
			break;
		}
		/* Retrieve MAC of the interface */
		if (ioctl(fd, SIOCGIFHWADDR, &ifr) == 0) {
			memcpy(iface->arp, ifr.ifr_hwaddr.sa_data, 6);
			DEBUG(LOG_INFO, "mac(%s) = "
				"%02x:%02x:%02x:%02x:%02x:%02x", ifr.ifr_name,
				iface->arp[0], iface->arp[1], iface->arp[2], 
				iface->arp[3], iface->arp[4], iface->arp[5]);
		} else {
			LOG(LOG_ERR, "SIOCGIFHWADDR failed on %s!",
				ifr.ifr_name);
			break;
		}
		/* set lease time from option or default */
		if ((option = find_option(iface->options, DHCP_LEASE_TIME))) {
			memcpy(&iface->lease, option->data + 2, 4);
			iface->lease = ntohl(iface->lease);
		}
		else
			iface->lease = LEASE_TIME;
		/* Set default max_leases */
		if (iface->max_leases == 0)
			iface->max_leases = 254;
		/* Allocate for leases */
		iface->leases = calloc(1, sizeof(struct dhcpOfferedAddr) *
			iface->max_leases);

		foundBr = 1;

#if 1 //__MSTC__,Lynn,DHCP
		if ((token = strstr(iface->interface, ":")) != NULL) {
			(*token) = '\0';
		}
		if ((token = strstr(iface->UsedIf, ":")) != NULL) {
			(*token) = '\0';
		}
#endif

	}
	close(fd);

   return foundBr;
}

#ifdef DHCP_RELAY
#if 0 // For single interface
static u_int32_t relay_remote;
#endif
void set_relays(void)
{
	int skt;
#if 1 //__MSTC__, Richard Huang
	socklen_t socklen;
#else
	int socklen;
#endif
	struct sockaddr_in addr;
	struct iface_config_t *iface;
	struct relay_config_t *relay;
	struct relay_config_t *new_relay;

	netiface *nifs = NULL;
	int nif_count;
	int i;

	/* Release all relays */
	cur_relay = relay_config;
	while(cur_relay) {
		relay = cur_relay->next;
		if (cur_relay->skt >= 0)
			close(cur_relay->skt);
		free(cur_relay);
		cur_relay = relay;
	}
	relay_config = cur_relay = NULL;

	/* Reset all relay interface names */
	for (iface = iface_config; iface; iface = iface->next) {
#if 0 // For single interface
		iface->relay_remote = relay_remote;
#endif
		iface->relay_interface[0] = 0;
	}

	/* Get network interface array */
	for (i = 0; i < BRCM_RETRY_COUNT; i++) {
		if ((nifs = get_netifaces(&nif_count)))
			break;
		if (i < BRCM_RETRY_COUNT)
			sleep(BRCM_RETRY_INTERVAL);
	}
	if (nifs == NULL) {
		LOG(LOG_ERR, "failed querying interfaces\n");
		return;
	}

	/* Create UDP for looking up routes */
	if ((skt = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		free(nifs);
		return;
	}

	for (iface = iface_config; iface; iface = iface->next) {
		/* Is this a relay interface? */
		if (iface->decline || iface->relay_remote == 0)
			continue;

		/* Connect UDP socket to relay to find out local IP address */
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = SERVER_PORT;
		addr.sin_addr.s_addr = iface->relay_remote;
		if (connect(skt, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
			LOG(LOG_WARNING, "no route to relay %u.%u.%u.%u",
			    ((unsigned char *)&addr.sin_addr.s_addr)[0],
			    ((unsigned char *)&addr.sin_addr.s_addr)[1],
			    ((unsigned char *)&addr.sin_addr.s_addr)[2],
			    ((unsigned char *)&addr.sin_addr.s_addr)[3]);
			continue;
		}
		socklen = sizeof(addr);
		if (getsockname(skt, (struct sockaddr *)&addr, &socklen) < 0)
			continue;

		/* Iterate through the list of interfaces to find the one that
		 * has route to remote DHCP server */
		for (i = 0; i < nif_count; i++) {
			if (nifs[i].nif_ip == addr.sin_addr.s_addr) {
				strcpy(iface->relay_interface,
				       nifs[i].nif_name);
                                /*                                
                                Fix: LAN&WAN has same IP address and has been configured DHCP relay.
                                Reason: There are two same ip address used br0/eth4.1 interface in set_relays method. 
                                Relay interface first decision to br0 (LAN).
                                so pocket cannot discover to WAN side.
                                solution: Let relay interface decision to eth4.0 (WAN).
                                */
                                if(strncmp(iface->relay_interface, "br", 2) != 0) 
                                {
				      break;
			        }
			}
		}
		if (!iface->relay_interface[0])
			continue;

		/* If the same relay (same relay interface) has been created,
		 * don't do it again */
		for (relay = relay_config; relay; relay = relay->next) {
			if (!strcmp(relay->interface, iface->relay_interface))
				break;
		}
		if (relay)
			continue;

		/* Create new relay entry */
		new_relay = malloc(sizeof(*new_relay));
		new_relay->next = NULL;
		strcpy(new_relay->interface, iface->relay_interface);
		new_relay->skt = -1;

		/* Link new relay */
		if (relay_config) {
			for (relay = relay_config; relay->next;
			     relay = relay->next);
			relay->next = new_relay;
		} else
			relay_config = new_relay;
	}
	close(skt);
	free(nifs);
}
#endif

int read_config(char *file)
{
	FILE *in;
#if 1 //__MSTC__,Lynn,DHCP
	char buffer[512], *token, *line;
#else
	char buffer[80], *token, *line;
#endif
	struct iface_config_t * iface;
#ifdef DHCP_RELAY
	int relayEnabled = 0;
#endif

#if 1 //ZyXEL, ShuYing, enable or disable the DHCP server assign LAN IP by mac hash method.
        AssignIpByMacHash=0;
#endif      
	/* Release all interfaces */
	cur_iface = iface_config;
	while(cur_iface) {
		iface = cur_iface->next;
		release_iface_config(cur_iface);
		cur_iface = iface;
	}
	iface_config = cur_iface = NULL;

	/* Reset server config to defaults */
	set_server_config_defaults();

	/* Allocate the first interface config */
	iface_config = cur_iface = calloc(1, sizeof(struct iface_config_t));

	if (!(in = fopen(file, "r"))) {
		LOG(LOG_ERR, "unable to open config file: %s", file);
		return 0;
	}

#ifdef DHCP_RELAY
#if 0 // For single interface
	relay_remote = 0;
#endif
#endif
	/* Read lines */
#if 1 //__MSTC__,Lynn,DHCP
	while (fgets(buffer, 512, in)) {
#else
	while (fgets(buffer, 80, in)) {
#endif
		if (strchr(buffer, '\n')) *(strchr(buffer, '\n')) = '\0';
		if (strchr(buffer, '#')) *(strchr(buffer, '#')) = '\0';
		token = buffer + strspn(buffer, " \t");
		if (*token == '\0') continue;
		line = token + strcspn(token, " \t=");
		if (*line == '\0') continue;
		*line = '\0';
		line++;
		line = line + strspn(line, " \t=");
		if (*line == '\0') continue;
		
		if (strcasecmp(token, "interface") == 0) {
			/* Read interface name */
			char * iface_name = NULL;
			read_str(line, &iface_name);
			if (!iface_name)
				continue;
			/* Lookup read interfaces. If this interface already
			 * read, ignore it */
			for (iface = iface_config; iface; iface = iface->next) {
				if (iface->interface &&
				    strcmp(iface->interface, iface_name) == 0) {
				    free(iface_name);
				    iface_name = NULL;
				    break;
				}
			}
			if (iface_name == NULL)
				continue;
			/* Assign the interface name to the first iface */
			if (cur_iface->interface == NULL)
				cur_iface->interface = iface_name;
			/* Finish the current iface, start a new one */
			else {
				iface = calloc(1, sizeof(struct iface_config_t));
				iface->interface = iface_name;
				cur_iface->next = iface;
				cur_iface = iface;
			}
#if 1 //__MSTC__,Lynn,DHCP:sync from SinJia, TR-098 DHCP Conditional Serving Pool
			/* Let virtual condservpool use physical interface for creating socket
				pool name will be condservpool1_br0, condservpool2_br1 ... */
			char poolDelim[] = "_";
			char *poolInterface = cmsMem_strdup(cur_iface->interface);
			char *poolSocketIf;
			if( strstr(cur_iface->interface, "condservpool") ){
				/*It is virtual interface*/
				poolSocketIf = strtok(poolInterface, poolDelim);
				poolSocketIf = strtok(NULL, poolDelim);
				cur_iface->UsedIf = cmsMem_strdup(poolSocketIf);
			}else{
				/*It is physical interface*/
				cur_iface->UsedIf = cmsMem_strdup(cur_iface->interface);
			}
			cmsMem_free(poolInterface);
#endif
		} else if (strcasecmp(token, "start") == 0)
			read_ip(line, &cur_iface->start);
		else if (strcasecmp(token, "end") == 0)
			read_ip(line, &cur_iface->end);
#if 1 //ZyXEL, ShuYing, enable or disable the DHCP server assign LAN IP by mac hash method.
                else if (strcasecmp(token, "AssignIpByMacHash") == 0){
                        read_u32(line, &AssignIpByMacHash); 
                        //cmsLog_error("after AssignIpByMacHash==%d",AssignIpByMacHash);
                }
#endif
#if 1 //ZyXEL, ShuYing, Support option 60 criteria in Bridge WAN, we need dhcpServerEnable info to decide whether to send offer.
                else if (strcasecmp(token, "dhcpServerEnable") == 0){
                        read_u32(line, &cur_iface->dhcpServerEnable);
                }
#endif
		else if (strcasecmp(token, "option") == 0 ||
			strcasecmp(token, "opt") == 0)
			read_opt(line, &cur_iface->options);
		else if (strcasecmp(token, "max_leases") == 0)
			read_u32(line, &cur_iface->max_leases);
		else if (strcasecmp(token, "remaining") == 0)
			read_yn(line, &server_config.remaining);
		else if (strcasecmp(token, "auto_time") == 0)
			read_u32(line, &server_config.auto_time);
		else if (strcasecmp(token, "decline_time") == 0)
			read_u32(line, &server_config.decline_time);
		else if (strcasecmp(token, "conflict_time") == 0)
			read_u32(line, &server_config.conflict_time);
		else if (strcasecmp(token, "offer_time") == 0)
			read_u32(line, &server_config.offer_time);
		else if (strcasecmp(token, "min_lease") == 0)
			read_u32(line, &server_config.min_lease);
		else if (strcasecmp(token, "lease_file") == 0)
			read_str(line, &server_config.lease_file);
		else if (strcasecmp(token, "pidfile") == 0)
			read_str(line, &server_config.pidfile);
		else if (strcasecmp(token, "notify_file") == 0)
			read_str(line, &server_config.notify_file);
		else if (strcasecmp(token, "siaddr") == 0)
			read_ip(line, &cur_iface->siaddr);
		else if (strcasecmp(token, "sname") == 0)
			read_str(line, &cur_iface->sname);
		else if (strcasecmp(token, "boot_file") == 0)
			read_str(line, &cur_iface->boot_file);
		else if (strcasecmp(token, "static_lease") == 0)
			read_staticlease(line, &cur_iface->static_leases);
#if 1 //__MSTC__,Lynn,DHCP	
/* For DHCP Option 60 Vendor ID Mode, __TELEFONICA__, ZyXEL Eva, 20100211. */
		else if (strcasecmp(token, "vendor_id_mode") == 0) {
			read_str(line, &cur_iface->vendor_id_mode);
		}
#endif
		else if (strcasecmp(token, "vendor_id") == 0) {
#if 1 //__MSTC__,Lynn,DHCP
			cur_iface->vendor_ids = read_op60Rules(line);
#else
			vendor_id_t * new = malloc(sizeof(vendor_id_t));
			new->next = NULL;
			new->len = read_qstr(line, new->id, sizeof(new->id));
			if (new->len > 0) {
				if (cur_iface->vendor_ids == NULL) {
					cur_iface->vendor_ids = new;
				} else {
					vendor_id_t * vid;

					for (vid = cur_iface->vendor_ids;
					     vid->next; vid = vid->next);
					vid->next = new;
				}
			} else
				free(new);
#endif
		}
#if 1 //__MSTC__,Lynn,DHCP: sync from SinJia, TR-098 DHCP Conditional Serving Pool
/* Support LAN port match criteria, __TELEFONICA__, ZyXEL YungAn, 20090819. */
      else if (strcasecmp(token, "source_port") == 0) 
         cur_iface->source_port = read_sourcePortRules(line);
#endif
#if 1 //__MSTC__,Lynn,DHCP
		else if (strcasecmp(token, "mac_addr") == 0) 
			cur_iface->macs = read_macRules(line);
		else if (strcasecmp(token, "client_id") == 0) 
			cur_iface->clnt_ids = read_op61Rules(line);
		else if (strcasecmp(token, "vsi") == 0) 
			cur_iface->vsi = read_op125Rules(line);
		else if (strcasecmp(token, "filter") == 0)
			cur_iface->filter = 1;
#endif
		else if (strcasecmp(token, "decline_file") == 0)
			read_str(line, &server_config.decline_file);
		else if (strcasecmp(token, "decline") == 0)
			cur_iface->decline = 1;
#ifdef MSTC_STATIC_DHCP_AUTO //__MSTC__, Lynn, Static DHCP automatically for STB
		else if (strcasecmp(token, "opt60ForSTB") == 0) 
			cur_iface->opt60ForSTB = read_op60Rules(line);		
#if 1 /*__ZyXEL__, David, support STB info configuration */
		else if (strcasecmp(token, "opt43ForSTB") == 0) 
			cur_iface->opt43ForSTB = read_op60Rules(line);
#endif	
#endif
#ifdef DHCP_RELAY
		else if (strcasecmp(token, "relay") == 0) {
			relayEnabled = 1;
                        cur_iface->relayEnable=1;
#if 0 // For single interface
			read_ip(line, &relay_remote);
#else
			read_ip(line, &cur_iface->relay_remote);
#endif
		}
#endif
		else
			LOG(LOG_WARNING, "unknown keyword '%s'", token);
	}
	fclose(in);

	/* Set default interface name if it's missing */
	if (iface_config->interface == NULL)
		iface_config->interface = strdup("eth0");

	/* Finish interface config automatically */
	if (!set_iface_config_defaults())
		exit_server(1);

#ifdef DHCP_RELAY
	set_relays();
      	if (relayEnabled)
     	{
      		register_message(CMS_MSG_WAN_CONNECTION_UP);
      	}
#endif

	return 1;
}


/* the dummy var is here so this can be a signal handler */
void write_leases(int dummy __attribute__((unused)))
{
	FILE *fp;
	unsigned int i;
	char buf[255];
	time_t curr = time(0);
	unsigned long lease_time;
	struct iface_config_t * iface;
	
	dummy = 0;
	
	if (!(fp = fopen(server_config.lease_file, "w"))) {
		LOG(LOG_ERR, "Unable to open %s for writing", server_config.lease_file);
		return;
	}

	for (iface = iface_config; iface; iface = iface->next) {
		for (i = 0; i < iface->max_leases; i++) {
			if (iface->leases[i].yiaddr != 0) {
				if (server_config.remaining) {
					if (lease_expired(&(iface->leases[i])))
						lease_time = 0;
					else
						lease_time =
							iface->leases[i].expires
							- curr;
				} else
					lease_time = iface->leases[i].expires;
				lease_time = htonl(lease_time);
				fwrite(iface->leases[i].chaddr, 16, 1, fp);
				fwrite(&(iface->leases[i].yiaddr), 4, 1, fp);
				fwrite(&lease_time, 4, 1, fp);
				//BRCM: hostname field is used by dproxy
				fwrite(iface->leases[i].hostname,
				       sizeof(iface->leases[i].hostname),
				       1, fp);
			}
		}
	}
	fclose(fp);
	
	if (server_config.notify_file) {
		sprintf(buf, "%s %s", server_config.notify_file, server_config.lease_file);
		system(buf);
	}
}

struct saved_lease {
	u_int8_t chaddr[16];
	u_int32_t yiaddr;	/* network order */
	u_int32_t expires;	/* host order */
	char hostname[64];
};


void read_leases(char *file)
{
	FILE *fp;
	time_t curr = time(0);
	struct saved_lease lease;
	struct iface_config_t *iface;
	int count = 0;
	
	if (!(fp = fopen(file, "r"))) {
		LOG(LOG_ERR, "Unable to open %s for reading", file);
		return;
	}

	while ((fread(&lease, sizeof lease, 1, fp) == 1)) {
		for (iface = iface_config; iface; iface = iface->next) {
			if (lease.yiaddr >= iface->start &&
				lease.yiaddr <= iface->end &&
				iface->cnt_leases < iface->max_leases) {
				iface->leases[cur_iface->cnt_leases].yiaddr =
					lease.yiaddr;
				iface->leases[cur_iface->cnt_leases].expires =
					ntohl(lease.expires);	
				if (server_config.remaining)
					iface->leases[cur_iface->cnt_leases].
					expires += curr;
				memcpy(iface->leases[cur_iface->cnt_leases].
					chaddr, lease.chaddr,
					sizeof(lease.chaddr));
				memcpy(iface->leases[cur_iface->cnt_leases].
					hostname, lease.hostname,
					sizeof(lease.hostname));
				iface->cnt_leases++;
				count++;
				break;
			}
		}
	}
	
	DEBUG(LOG_INFO, "Read %d leases", count);
	
	fclose(fp);
}
		
// BRCM_begin

void send_lease_info(UBOOL8 isDelete, const struct dhcpOfferedAddr *lease)
{
   char buf[sizeof(CmsMsgHeader) + sizeof(DhcpdHostInfoMsgBody)] = {0};
   CmsMsgHeader *hdr = (CmsMsgHeader *) buf;
   DhcpdHostInfoMsgBody *body = (DhcpdHostInfoMsgBody *) (hdr+1);
   CmsRet ret;
   struct in_addr inaddr;
   UINT32 remaining, now;
#if 1 //__MSTC__,Lynn,DHCP
   char intfName[BUFLEN_32];
#endif
#ifdef MSTC_STATIC_DHCP_AUTO //__MSTC__, Lynn, Static DHCP automatically for STB
   vendor_id_t *vid_cur;
   UBOOL8 isSTB = FALSE;
#endif
   inaddr.s_addr = lease->yiaddr;
   if (lease->expires == 0xffffffff)  /* check if expires == -1 */
   {
      remaining = lease->expires;
   }
   else
   {
      now = time(0);
      if (lease->expires < now)
      {
         remaining = 0;
      }
      else
      {
         remaining = lease->expires - now;
         /*
          * dhcpd is reporting remaining time to ssk, which sticks it into
          * the data model.  The data model expects a SINT32, so make sure
          * our UINT32 remaining does not go above MAX_SINT32.
          */
         if (remaining > MAX_SINT32)
         {
            remaining = MAX_SINT32;
         }
      }
   }

   DEBUG(LOG_INFO, "sending lease info update msg, isDelete=%d, leaseTimeRemaining=%d", isDelete, remaining);
   DEBUG(LOG_INFO, "assigned addr = %s", inet_ntoa(inaddr));

	hdr->type = CMS_MSG_DHCPD_HOST_INFO;
	hdr->src = EID_DHCPD;
	hdr->dst = EID_SSK;
	hdr->flags_event = 1;
   hdr->dataLength = sizeof(DhcpdHostInfoMsgBody);

   body->deleteHost = isDelete;
#if 1 //__MSTC__,Lynn,DHCP 
/*ZyXEL Autumn static DHCP will course error*/
   body->leaseTimeRemaining = ((remaining == 0xffffffff) ? -1:remaining);
#else
   body->leaseTimeRemaining = remaining;
#endif

#if 1 //__MSTC__,Lynn,DHCP:sync from SinJia, TR-098 DHCP Conditional Serving Pool
/* TR-098 DHCP Conditional Serving Pool, __TELEFONICA__, ZyXEL YungAn, 20090817. */
      char poolDelim[] = "_";
      char *poolInterface = cmsMem_strdup(cur_iface->interface);
      char *poolSocketIf;
      if( strstr(cur_iface->interface, "condservpool") ){
         /*It is virtual interface*/
         poolSocketIf = strtok(poolInterface, poolDelim);
         poolSocketIf = strtok(NULL, poolDelim);
         snprintf(body->ifName, sizeof(body->ifName), poolSocketIf);
      }else{
         /*It is physical interface*/
         snprintf(body->ifName, sizeof(body->ifName), cur_iface->interface);
      }
      cmsMem_free(poolInterface);
   
      snprintf(body->poolName, sizeof(body->poolName), cur_iface->interface);
   
#else
   snprintf(body->ifName, sizeof(body->ifName), cur_iface->interface);
#endif

   snprintf(body->ipAddr, sizeof(body->ipAddr), inet_ntoa(inaddr));
   snprintf(body->hostName, sizeof(body->hostName), lease->hostname);
   cmsUtl_macNumToStr(lease->chaddr, body->macAddr);

   
   /* does DHCP include the statically assigned addresses?  Or should that be STATIC? */
#if 1 //__MSTC__,Lynn,DHCP
   if (lease->isStatic)
   {
      snprintf(body->addressSource, sizeof(body->addressSource), MDMVS_STATIC);
   }
   else
   {
      snprintf(body->addressSource, sizeof(body->addressSource), MDMVS_DHCP);
   }
#ifdef MSTC_STATIC_DHCP_AUTO //__MSTC__, Lynn, Static DHCP automatically for STB
	vid_cur = cur_iface->opt60ForSTB;
	while (vid_cur) {
		/*
			support wild card Vendor ID
		*/
#if 1 //ZyXEL, ShuYing, fully matched STB Vendor ID
		if (cmsUtl_strcmp(vid_cur->id, lease->vendorid) == 0) {
#else
		if (cmsUtl_strncmp(vid_cur->id, lease->vendorid, cmsUtl_strlen(vid_cur->id)) == 0) {
#endif
			snprintf(body->hostType, sizeof(body->hostType), MDMVS_SETTOP);
			isSTB = TRUE;
			break;
		}	
		vid_cur = vid_cur->next;
	}

	if (!isSTB)
	{
     	snprintf(body->hostType, sizeof(body->hostType), MDMVS_COMPUTER); // default value
	}
#else
   if (strcmp(lease->vendorid, "IP-STB") == 0)
      snprintf(body->hostType, sizeof(body->hostType), MDMVS_SETTOP);
   else
      snprintf(body->hostType, sizeof(body->hostType), MDMVS_COMPUTER); // default value
#endif
#else
   snprintf(body->addressSource, sizeof(body->addressSource), MDMVS_DHCP);
#endif

   /* is there a way we can tell if we assigned this address to a host on WLAN? */
#if 1 //__MSTC__,Lynn,DHCP
  //detect if it is condservpoolX_brX, then strip the brX to query
   if(strstr(cur_iface->interface,"condservpool")){
      char ifName[16];
      sscanf(cur_iface->interface,"%*[^'_']_%s",ifName);
      cmsNet_getPortNameFromMac(ifName, lease->chaddr, intfName);
   }else{
      cmsNet_getPortNameFromMac(cur_iface->interface, lease->chaddr, intfName);
   }
   //fprintf(stderr, "interface name: %s\n", intfName);
   if (strstr(intfName, "eth"))
      snprintf(body->interfaceType, sizeof(body->interfaceType), MDMVS_ETHERNET);
   else if (strstr(intfName, "wl"))
      snprintf(body->interfaceType, sizeof(body->interfaceType), MDMVS_802_11);
   else if (strstr(intfName, "usb"))
      snprintf(body->interfaceType, sizeof(body->interfaceType), MDMVS_USB);
   else if (strstr(intfName, "moca"))
      snprintf(body->interfaceType, sizeof(body->interfaceType), MDMVS_COAX);
   else
      snprintf(body->interfaceType, sizeof(body->interfaceType), MDMVS_OTHER);
#else
   snprintf(body->interfaceType, sizeof(body->interfaceType), MDMVS_ETHERNET);
#endif

   /* the vendor id is also contained in the lease struct,
    * we could also send that to ssk to put into the host entry. */

   snprintf(body->oui, sizeof(body->oui), lease->oui);
   snprintf(body->serialNum, sizeof(body->serialNum), lease->serialNumber);
   snprintf(body->productClass, sizeof(body->productClass), lease->productClass);

   if ((ret = cmsMsg_send(msgHandle, hdr)) != CMSRET_SUCCESS)
   {
      LOG(LOG_WARNING, "could not send lease info update");
   }
   else
   {
      DEBUG(LOG_INFO, "lease info update sent!");
   }
}


void write_decline(void)
{
	FILE *fp;
	char msg[sizeof(CmsMsgHeader) + sizeof(DHCPDenyVendorID)] = {0};
	CmsMsgHeader *hdr = (CmsMsgHeader*)&msg;
	DHCPDenyVendorID *vid = (DHCPDenyVendorID*)(&msg[sizeof(CmsMsgHeader)]);
#if 0 //__MSTC__,Lynn,DHCP 

#ifndef MSTC_DHCP //__MSTC__,Lynn,DHCP
	/* Write a log to console */
	printf("Denied vendor ID \"%s\", MAC=%02x:%02x:%02x:%02x:%02x:%02x Interface=%s\n",
	       declines->vendorid, declines->chaddr[0], declines->chaddr[1],
	       declines->chaddr[2], declines->chaddr[3], declines->chaddr[4],
	       declines->chaddr[5], cur_iface->interface);
	fflush(stdout);
#endif

	if (!(fp = fopen(server_config.decline_file, "w"))) {
		LOG(LOG_ERR, "Unable to open %s for writing", server_config.decline_file);
		return;
	}

	fwrite(declines->chaddr, 16, 1, fp);
	fwrite(declines->vendorid, 64, 1, fp);
	fclose(fp);
#endif	
	/*
	 * Send an event msg to ssk.
	 */
	hdr->type = CMS_MSG_DHCPD_DENY_VENDOR_ID;
	hdr->src = EID_DHCPD;
	hdr->dst = EID_SSK;
	hdr->flags_event = 1;
	hdr->dataLength = sizeof(DHCPDenyVendorID);
	vid->deny_time = time(NULL);
#if 1 //__MSTC__,Lynn,DHCP
	memcpy(vid->chaddr, declines->chaddr, 16);
	if (strlen(declines->vendorid) != 0)
		strcpy(vid->vendor_id, declines->vendorid);
	else
		vid->vendor_id[0] = 0;
	if (strlen(declines->mac_addr) != 0)
		strcpy(vid->mac_addr, declines->mac_addr);
	else
		vid->mac_addr[0] = 0;
	if (strlen(declines->clientid) != 0)
		strcpy(vid->client_id, declines->clientid);
	else
		vid->client_id[0] = 0;
	if (strlen(declines->vsi) != 0)
		strcpy(vid->vsi, declines->vsi);
	else
		vid->vsi[0] = 0;
	memcpy(vid->ifName, cur_iface->interface, sizeof(vid->ifName));
#else
	memcpy(vid->chaddr, declines->chaddr, sizeof(vid->chaddr));
	strncpy(vid->vendor_id, declines->vendorid, sizeof(vid->vendor_id)-1);
	strncpy(vid->ifName, cur_iface->interface, sizeof(vid->ifName)-1);
#endif
	cmsMsg_send(msgHandle, hdr);
}

static struct dhcpOfferedAddr *find_expired_lease_by_yiaddr(u_int32_t yiaddr)
{
	struct iface_config_t * iface;

	for (iface = iface_config; iface; iface = iface->next) {
		unsigned int i;
		for (i = 0; i < iface->max_leases; i++) {
			if (iface->leases[i].yiaddr == yiaddr) {
				if (iface->leases[i].expires >
					(unsigned long) time(0))
					return &(iface->leases[i]);
				else
					return NULL;
			}
		}
	}
	return NULL;
}

/* get signal to write viTable to file */
void write_viTable(int dummy __attribute__((unused)))
{
	FILE *fp;
	int count;
	pVI_OPTION_INFO pPtr=NULL;

	if (!(fp = fopen("/var/udhcpd/managable.device", "w+"))) {
		LOG(LOG_ERR, "Unable to open %s for writing", "/var/udhcpd/managable.device");
		return;
	}
	count = viList->count;
	fprintf(fp,"NumberOfDevices %d\n",count);
	if (count > 0) {
	  pPtr = viList->pHead;
	  while (pPtr) {
	    if (find_expired_lease_by_yiaddr(pPtr->ipAddr)) {
	      strcpy(pPtr->oui,"");
	      strcpy(pPtr->serialNumber,"");
	      strcpy(pPtr->productClass,"");
	    }
	    fprintf(fp,"IPaddr %x Enterprise %d OUI %s SerialNumber %s ProductClass %s\n",
		    pPtr->ipAddr,pPtr->enterprise,pPtr->oui,pPtr->serialNumber,
		    pPtr->productClass);
	    pPtr = pPtr->next;
	  }
	}
	fclose(fp);
}

#ifdef DHCP_RELAY
/* Register interested message to smd to receive it later */
static void register_message(CmsMsgType msgType)
{
	CmsMsgHeader msg;
	CmsRet ret;

	memset(&msg, 0, sizeof(msg));
	msg.type = CMS_MSG_REGISTER_EVENT_INTEREST;
	msg.src = EID_DHCPD;
	msg.dst = EID_SMD;
	msg.flags_request = 1;
	msg.wordData = msgType;

	ret = cmsMsg_sendAndGetReply(msgHandle, &msg);
	if (ret != CMSRET_SUCCESS) {
		cmsLog_error("register_message(%d) error (%d)", msgType, ret);
	} else {
		cmsLog_debug("register_message(%d) succeeded", msgType);
	} 

   	return;
}
#endif  /* DHCP_RELAY */

#if 1 //__MSTC__,Lynn,DHCP
void getArpInfo(void)
{
   FILE *fsArp = fopen("/proc/net/arp", "r");
   char col[6][32];
   char line[512];
   int count = 0;
   struct arpEntry *new_entry;
   struct in_addr inaddr;

   if (fsArp != NULL)
   {
      cur_arp = arp_head = NULL;
      while (fgets(line, sizeof(line), fsArp))
      {
         if ( count++ < 1 ) continue;

         sscanf(line, "%s %s %s %s %s %s", col[0], col[1], col[2], col[3], col[4], col[5]);
#if 1 //__COMMON__, Steven: Support interface group
         if (cmsUtl_strcmp(col[2], "0x0") == 0 || cmsUtl_strstr(col[5], "br") == NULL) continue;
#else
	  if (strcmp(col[2], "0x0") == 0 || strcmp(col[5], "br0") != 0) continue;
#endif

         new_entry = (struct arpEntry *) malloc (sizeof(struct arpEntry));
         memset(&inaddr, 0, sizeof(inaddr));
         inet_aton(col[0], &inaddr);
         new_entry->yiaddr = inaddr.s_addr;
         memset(new_entry->chaddr, 0, sizeof(new_entry->chaddr));
         cmsUtl_macStrToNum(col[3], new_entry->chaddr);
         new_entry->next = NULL;
         if (arp_head == NULL)
         {
            arp_head = cur_arp = new_entry;
         }
         else
         {
            cur_arp->next = new_entry;
            cur_arp = new_entry;
         }
      }

      fclose(fsArp);
   }
}

void freeArpInfo(void)
{
   struct arpEntry *next;

   cur_arp = arp_head;
   while (cur_arp != NULL)
   {
      next = cur_arp->next;
      free(cur_arp);
      cur_arp = next;
   }

   cur_arp = arp_head = NULL;
   return;
}

#endif
// BRCM_end
