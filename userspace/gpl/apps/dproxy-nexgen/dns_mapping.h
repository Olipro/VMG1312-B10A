#ifndef __DNS_MAPPING_
#define __DNS_MAPPING_

/*
 * Read  "/var/dnsinfo.conf" and put the dns mapping info into a linked list.
 * Is called whenever the dnsinfo.conf changes (caused by  WAN connection status change)
 */
void dns_mapping_conifg_init(void);


/*
 * Find a WAN side dns ip from the lan Ip and source port.
 *  
 *
 * @param lanInfo     (IN) LAN ip of the dns query
 * @param queryType   (IN) Source port of the dns query 
 * @param dns1        (OUT) the primary dns ip
 * @param proto       (OUT) layer 3 protocol (IPv4 or IPv6)
 *
 */
UBOOL8 dns_mapping_find_dns_ip(struct sockaddr_storage *lanInfo, 
                               int queryType, char *dns1, int *proto);

#endif
