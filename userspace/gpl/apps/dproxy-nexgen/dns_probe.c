#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cms_params.h>

#include "dproxy.h"
#include "conf.h"
#include "dns_list.h"
#include "dns_probe.h"

#ifdef DESKTOP_LINUX
#define REAL_DNS_FYI_FILENAME   "./dns.real"
#else
#if 1 // __MSTC__, Richard Huang
#define REAL_DNS_FYI_FILENAME   "/var/fyi/sys/dns_config"
#else
#define REAL_DNS_FYI_FILENAME   "/var/fyi/sys/dns.real"
#endif
#endif

extern int dns_wanup;
extern int dns_querysock;
static char primary_ns[CMS_IPADDR_LENGTH];
static char secondary_ns[CMS_IPADDR_LENGTH];
static struct sockaddr_in probe_addr;
#ifdef DNS_PROBE
static time_t probe_next_time;
static int probe_tried;
static time_t probe_timeout;
static uint16_t probe_id;
static char probe_pkt[36] =
	{0x0, 0x0, 0x1, 0x0, 0x0, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
	 0x1, 'a', 0xc, 'r', 'o', 'o', 't', '-', 's', 'e', 'r', 'v',
	 'e', 'r', 's', 0x3, 'n', 'e', 't', 0x0, 0x0, 0x1, 0x0, 0x1, };
#else
time_t dns_recover_time;
#if 1//__MSTC__,kenny,Merge from DSL-491HNU-B1B_STD
int request_timeout;
int number_repeat;
#endif
#endif

/* Load name servers' addresses from /etc/resolv.conf */
static int load_brcm_name_servers(void)
{
	FILE * fp;
	char line[256];
	char addr[CMS_IPADDR_LENGTH];
	char domain[256];
#if 1 // __MSTC__, Richard Huang
   char lan[CMS_IPADDR_LENGTH];
#endif
#if 1//__MSTC__,kenny,Merge from DSL-491HNU-B1B_STD

config.timeout_primary =2;
config.timeout_secondary=2;
config.num_repeat_primary=3;
config.num_repeat_secondary=3;
config.fail_back_interval=120;
#endif

	memset(primary_ns, 0, sizeof(primary_ns));
	memset(secondary_ns, 0, sizeof(secondary_ns));

    //name server of resolv.conf is 127.0.0.1, it redirects ont dns query
    //to dns proxy to record dns query failed of ipHost PM
	if((fp = fopen(REAL_DNS_FYI_FILENAME, "r")) == NULL) {
		debug("Opend %s failed", REAL_DNS_FYI_FILENAME);
        // if dns.real does not exist, open /etc/resolv.conf
    	if((fp = fopen("/etc/resolv.conf", "r")) == NULL) {
	    	debug("Opend /etc/resolv.conf failed");
		    return 0;
	    }
	}

	while(fgets(line, sizeof(line), fp)) {
		if (sscanf(line, "nameserver %15s", addr) == 1) {
			if ( strcmp( addr, "0.0.0.0") && strcmp( addr, "127.0.0.1")) {
				if (primary_ns[0] == 0) {
					strncpy(primary_ns, addr, sizeof(primary_ns)-1);
				} else if (secondary_ns[0] == 0) {
					strncpy(secondary_ns, addr, sizeof(secondary_ns)-1);
				} else {
					printf("dnsproxy supports a max of two name servers.  "
							"Additional name servers are ignored.\n");
				}
			}
		} else if (sscanf(line, "domain %s", domain) == 1) {
			strncpy(config.domain_name, domain, sizeof(config.domain_name)-1);
		}
#if 1 // __MSTC__, Richard Huang
      else if (sscanf(line, "LAN %s", lan) == 1) {
         strncpy(config.lan_ip, lan, sizeof(config.lan_ip) - 1);
      }
#endif
	}
	fclose(fp);

	debug("dproxy: load nameserver %s %s", primary_ns, secondary_ns);
	debug("dproxy: load domain %s", config.domain_name);

	if (primary_ns[0] == 0)  /* not even a single valid nameserver found */
		return 0;
	return 1;
}

/* Initialization before probing */
int dns_probe_init(void)
{
	int ret;
	
	debug("dns_probe_init");

	/* Try to read name servers from /var/fyi/sys/dns */
	if ((ret = load_brcm_name_servers())) {
		strcpy(config.name_server, primary_ns);
#if 1//__MSTC__,kenny,Merge from DSL-491HNU-B1B_STD
		request_timeout = config.timeout_primary;
		number_repeat = config.num_repeat_primary;
#endif
	} else {
      /*
       * ret == 0 means wan is not up. 
       * BRCM: Use the magic ppp on demand address.
       */
      debug("wan is not up, set name_server to 128.9.0.107");
		strcpy(config.name_server, "128.9.0.107");
   }

	/* Set primary server as the probing address */
	memset(&probe_addr, 0, sizeof(probe_addr));
	probe_addr.sin_family = AF_INET;
	inet_aton(primary_ns, &probe_addr.sin_addr);
	probe_addr.sin_port = ntohs(PORT);

#ifdef DNS_PROBE
	/* Initialize request id */
	srandom(time(NULL));
	probe_id = (uint16_t)random();
	probe_next_time = 0;
	probe_tried = 0;
	probe_timeout = 0;
#endif
	return ret;
}

/* Send, resend probing request, check timeout, and switch name servers */
int dns_probe(void)
{
	int ret = 0;
	time_t now = time(NULL);

	debug("dns_probe: entered, dns_wanup=%d now=%d dns_recover_time=%d\n",
	      dns_wanup, now, dns_recover_time);

	if (!dns_wanup) { /* No need to probe if no WAN interface */
		return ret;
	}

#ifdef DNS_PROBE
	if (probe_tried) { /* Probing */
		if (now >= probe_timeout) { /* Timed out */
			if (probe_tried >= DNS_PROBE_MAX_TRY) {
				/* Probe failed */
				debug("dproxy: probing failed\n");
				if (secondary_ns[0] &&
				    strcmp(config.name_server, secondary_ns)) {
					printf("Primary DNS server Is Down... "
					       "Switching To Secondary DNS "
					       "server \n");
					strcpy(config.name_server,
					       secondary_ns);
				}
				probe_tried = 0;
				probe_next_time = now + DNS_PROBE_INTERVAL;
				ret = DNS_PROBE_INTERVAL;
			} else { /* Retry */
				sendto(dns_querysock, probe_pkt, sizeof(probe_pkt),
				       0, (struct sockaddr*)&probe_addr,
				       sizeof(probe_addr));
				probe_timeout = now + DNS_PROBE_TIMEOUT;
				probe_tried++;
				ret = DNS_PROBE_TIMEOUT;
			}
		} else {
			ret = probe_timeout - now;
		}
	} else if (now >= probe_next_time) { /* Time to probe */
		*((uint16_t*)probe_pkt) = htons(++probe_id);
		sendto(dns_querysock, probe_pkt, sizeof(probe_pkt), 0,
		       (struct sockaddr*)&probe_addr, sizeof(probe_addr));
		probe_tried = 1;
		probe_timeout = now + DNS_PROBE_TIMEOUT;
		ret = DNS_PROBE_TIMEOUT;
	} else {
		ret = probe_next_time - now;
	}
#else
	if (dns_recover_time) { /* Recovering */
		if (now >= dns_recover_time)
			dns_probe_switchback();
		else
			ret = dns_recover_time - now;
	}

#endif

	debug("dns_probe: done, ret=%d\n", ret);

	return ret;
}

#ifndef DNS_PROBE
int dns_probe_is_primary_up(void)
{
	if (dns_recover_time == 0)
		return 1;
	else
		return 0;
}

void dns_probe_set_recover_time(void)
{

   if (dns_recover_time) {
#if 1//__MSTC__,kenny,Merge from DSL-491HNU-B1B_STD
	 printf("Secondary DNS server (%s) Is Down..."
         "Switching To Primary DNS server (%s) \n",
         secondary_ns, primary_ns);

      strncpy(config.name_server, primary_ns, sizeof(config.name_server)-1);

      request_timeout = config.timeout_primary;
      number_repeat = config.num_repeat_primary;
      dns_recover_time = 0;
   } else {
      printf("Primary DNS server (%s) Is Down..."
         "Switching To Secondary DNS server (%s) "
         "for %d seconds.  \n",
         primary_ns, secondary_ns, config.fail_back_interval);

      strncpy(config.name_server, secondary_ns, sizeof(config.name_server)-1);

      request_timeout = config.timeout_secondary;
      number_repeat = config.num_repeat_secondary;
      dns_recover_time = time(NULL) + config.fail_back_interval;
#else
      /* We are currently using the secondary nameserver.  Do nothing */
      debug("still using secondary_ns, dns_recover_time=%d\n",
            dns_recover_time);
      return;
   }

   /* Only switch to the 2nd dns server when it is configured */
   if (secondary_ns[0]) {
     char date_time_buf[DATE_TIME_BUFLEN];
     get_date_time_str(date_time_buf, sizeof(date_time_buf));

     printf("%s Primary DNS server (%s) Is Down..."
          "Switching To Secondary DNS server (%s) "
          "for %d seconds.  \n",
          date_time_buf, primary_ns, secondary_ns, DNS_RECOVER_TIMEOUT);

     strncpy(config.name_server, secondary_ns, sizeof(config.name_server)-1);
     dns_recover_time = time(NULL) + DNS_RECOVER_TIMEOUT;
     dns_list_unarm_all_requests();
   }
   else
   {
      debug("no valid secondary_ns to switch to.\n");
   }
#endif
   }
}
#endif

/* Switch back to primary server */
void dns_probe_switchback(void)
{
	char date_time_buf[DATE_TIME_BUFLEN];

#ifdef DNS_PROBE
	static int first_time = 1;
#else
	dns_recover_time = 0;
#endif

	debug("config.name_server=%s primary_ns=%s",
	      config.name_server, primary_ns);

	if (!strcmp(config.name_server, primary_ns))
	{
		debug("dns_probe_switchback called even though primary is already up!\n");
		return;
	}

#ifdef DNS_PROBE
	if (first_time)
		first_time = 0;
	else
#endif
#if 1//__MSTC__,kenny,Merge from DSL-491HNU-B1B_STD
	 printf("Switching back to primary dns server (%s).\n", primary_ns);
   	 strncpy(config.name_server, primary_ns, sizeof(config.name_server)-1);
     request_timeout = config.timeout_primary;
     number_repeat = config.num_repeat_primary;
#else
	get_date_time_str(date_time_buf, sizeof(date_time_buf));
	printf("%s Switching back to primary dns server (%s).\n",
	       date_time_buf, primary_ns);

	strncpy(config.name_server, primary_ns, sizeof(config.name_server)-1);
	dns_list_unarm_all_requests();
#endif
}

/* Activate primary server */
int dns_probe_activate(uint32_t name_server)
{
	debug("name_server=0x%x probe_addr=0x%x\n", name_server, probe_addr.sin_addr.s_addr);

	if (name_server != probe_addr.sin_addr.s_addr)
		return 0;

#ifdef DNS_PROBE
	probe_tried = 0;
	probe_next_time = time(NULL) + DNS_PROBE_INTERVAL;
#endif

	dns_probe_switchback();
	debug("probe_activate: done\n");
	return 1;
}

/* Activate name server if it's the response for probing request */
int dns_probe_response(dns_request_t *m)
{
#ifdef DNS_PROBE
	if (m->message.header.flags.flags & 0x8000 &&
	    m->message.header.id != probe_id)
		return 0;
	return dns_probe_activate(m->src_addr.s_addr);
#else
	return 0;
#endif
}

#ifdef SUPPORT_DEBUG_TOOLS
void dns_probe_print(void)
{
   printf("primary_ns=%s secondary_ns=%s\n", primary_ns, secondary_ns);
#ifndef DNS_PROBE
   printf("dns_recover_time=%d\n", (int) dns_recover_time);
#endif
}
#endif  /* SUPPORT_DEBUG_TOOLS */
