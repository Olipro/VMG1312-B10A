/* serverpacket.c
 *
 * Constuct and send DHCP server packets
 *
 * Russ Dill <Russ.Dill@asu.edu> July 2001
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <time.h>

#include "packet.h"
#include "debug.h"
#include "dhcpd.h"
#include "options.h"
#include "leases.h"
#include "static_leases.h"

/* send a packet to giaddr using the kernel ip stack */
static int send_packet_to_relay(struct dhcpMessage *payload)
{
        DEBUG(LOG_INFO, "Forwarding packet to relay");

        return kernel_packet(payload, cur_iface->server, SERVER_PORT,
                        payload->giaddr, SERVER_PORT);
}


/* send a packet to a specific arp address and ip address by creating our own ip packet */
static int send_packet_to_client(struct dhcpMessage *payload, int force_broadcast)
{
        u_int32_t ciaddr;
        char chaddr[6];
        
        if (force_broadcast) {
                DEBUG(LOG_INFO, "broadcasting packet to client (NAK)");
                ciaddr = INADDR_BROADCAST;
                memcpy(chaddr, MAC_BCAST_ADDR, 6);              
        } else if (payload->ciaddr) {
                DEBUG(LOG_INFO, "unicasting packet to client ciaddr");
                ciaddr = payload->ciaddr;
                memcpy(chaddr, payload->chaddr, 6);
        } else if (ntohs(payload->flags) & BROADCAST_FLAG) {
                DEBUG(LOG_INFO, "broadcasting packet to client (requested)");
                ciaddr = INADDR_BROADCAST;
                memcpy(chaddr, MAC_BCAST_ADDR, 6);              
        } else {
                DEBUG(LOG_INFO, "unicasting packet to client yiaddr");
                ciaddr = payload->yiaddr;
                memcpy(chaddr, payload->chaddr, 6);
        }
        return raw_packet(payload, cur_iface->server, SERVER_PORT, 
                        ciaddr, CLIENT_PORT, chaddr, cur_iface->ifindex);
}


/* send a dhcp packet, if force broadcast is set, the packet will be broadcast to the client */
static int send_packet(struct dhcpMessage *payload, int force_broadcast)
{
        int ret;

        if (payload->giaddr)
                ret = send_packet_to_relay(payload);
        else ret = send_packet_to_client(payload, force_broadcast);
        return ret;
}


static void init_packet(struct dhcpMessage *packet, struct dhcpMessage *oldpacket, char type)
{
        memset(packet, 0, sizeof(struct dhcpMessage));
        
        packet->op = BOOTREPLY;
        packet->htype = ETH_10MB;
        packet->hlen = ETH_10MB_LEN;
        packet->xid = oldpacket->xid;
        memcpy(packet->chaddr, oldpacket->chaddr, 16);
        packet->cookie = htonl(DHCP_MAGIC);
        packet->options[0] = DHCP_END;
        packet->flags = oldpacket->flags;
        packet->giaddr = oldpacket->giaddr;
        packet->ciaddr = oldpacket->ciaddr;
        add_simple_option(packet->options, DHCP_MESSAGE_TYPE, type);
        add_simple_option(packet->options, DHCP_SERVER_ID,
		ntohl(cur_iface->server)); /* expects host order */
}


/* add in the bootp options */
static void add_bootp_options(struct dhcpMessage *packet)
{
        packet->siaddr = cur_iface->siaddr;
        if (cur_iface->sname)
                strncpy((char *)(packet->sname), cur_iface->sname,
			sizeof(packet->sname) - 1);
        if (cur_iface->boot_file)
                strncpy((char *)(packet->file), cur_iface->boot_file,
			sizeof(packet->file) - 1);
}
        

/* send a DHCP OFFER to a DHCP DISCOVER */
int sendOffer(struct dhcpMessage *oldpacket)
{
        struct dhcpMessage packet;
        struct dhcpOfferedAddr *lease = NULL;
        u_int32_t req_align, lease_time_align = cur_iface->lease;
        char *req, *lease_time;
        struct option_set *curr;
        struct in_addr addr;
	//For static IP lease
	uint32_t static_lease_ip;

        //brcm begin
        char VIinfo[VENDOR_IDENTIFYING_INFO_LEN];
        //brcm end
#ifdef MSTC_LOG //__MSTC__,Lynn,LOG
        char client_mac[20];
#endif

        init_packet(&packet, oldpacket, DHCPOFFER);
        
	//For static IP lease
	static_lease_ip = getIpByMac(cur_iface->static_leases,
		oldpacket->chaddr);

	if(!static_lease_ip) {
        	/* the client is in our lease/offered table */
        	if ((lease = find_lease_by_chaddr(oldpacket->chaddr))) {
                	if (!lease_expired(lease)) 
                        	lease_time_align = lease->expires - time(0);
                	packet.yiaddr = lease->yiaddr;
        	/* Or the client has a requested ip */
        	} else if ((req = (char *)get_option(oldpacket, DHCP_REQUESTED_IP)) &&

			/* Don't look here (ugly hackish thing to do) */
			memcpy(&req_align, req, 4) && 

			/* and the ip is in the lease range */
			ntohl(req_align) >= ntohl(cur_iface->start) &&
			ntohl(req_align) <= ntohl(cur_iface->end) && 

			/* and its not already taken/offered */
			((!(lease = find_lease_by_yiaddr(req_align)) ||

			/* or its taken, but expired */
			lease_expired(lease)))) {
				packet.yiaddr = req_align; 

		/* otherwise, find a free IP */
        	} else {
#if 1
                  //cmsLog_error("AssignIpByMacHash ==%d",AssignIpByMacHash);
                  if(AssignIpByMacHash){ //ZyXEL, ShuYing, enable or disable the DHCP server assign LAN IP by mac hash method.
                        packet.yiaddr = find_address_hash(oldpacket->chaddr,0);
                        if (!packet.yiaddr) packet.yiaddr = find_address_hash(oldpacket->chaddr,1);
                  }else{
#if 1 //__CTLK__, Thief
		        packet.yiaddr = find_address(0, 0);

		        /* try for an expired lease */
                if (!packet.yiaddr) packet.yiaddr = find_address(1, 0);

		        /* try for a lease whose device doesn't active*/
                if (!packet.yiaddr) packet.yiaddr = find_address(0, 1);
#endif
                  }
#else
#ifdef MSTC_DHCP_IP_BY_MAC_HASH
                	packet.yiaddr = find_address_hash(oldpacket->chaddr,0);
#else
					packet.yiaddr = find_address(0);
#endif
                	/* try for an expired lease */
#ifdef MSTC_DHCP_IP_BY_MAC_HASH
			if (!packet.yiaddr) packet.yiaddr = find_address_hash(oldpacket->chaddr,1);
#else
			if (!packet.yiaddr) packet.yiaddr = find_address(1);
#endif
#endif
        	}
        
        	if(!packet.yiaddr) {
                	LOG(LOG_WARNING, "no IP addresses to give -- "
				"OFFER abandoned");
                	return -1;
        	}
        
#if 1 //__MSTC__,Lynn,DHCP
        	if (!add_lease_MSTC(packet.chaddr, packet.yiaddr,
			server_config.offer_time, FALSE)) {
#else
        	if (!add_lease(packet.chaddr, packet.yiaddr,
			server_config.offer_time)) {
#endif
                	LOG(LOG_WARNING, "lease pool is full -- "
				"OFFER abandoned");
                	return -1;
        	}               

        	if ((lease_time = (char *)get_option(oldpacket, DHCP_LEASE_TIME))) {
                	memcpy(&lease_time_align, lease_time, 4);
                	lease_time_align = ntohl(lease_time_align);
                	if (lease_time_align > cur_iface->lease) 
                        	lease_time_align = cur_iface->lease;
        	}

        	/* Make sure we aren't just using the lease time from the
		 * previous offer */
        	if (lease_time_align < server_config.min_lease) 
                	lease_time_align = cur_iface->lease;

	} else {
		/* It is a static lease... use it */
		packet.yiaddr = static_lease_ip;
	}
                
        add_simple_option(packet.options, DHCP_LEASE_TIME, lease_time_align);

        curr = cur_iface->options;
        while (curr) {
                if (curr->data[OPT_CODE] != DHCP_LEASE_TIME)
                        add_option_string(packet.options, curr->data);
                curr = curr->next;
        }

        add_bootp_options(&packet);

        //brcm begin
        /* if DHCPDISCOVER from client has device identity, send back gateway identity */
        if ((req = (char *)get_option(oldpacket, DHCP_VENDOR_IDENTIFYING))) {
          if (createVIoption(VENDOR_IDENTIFYING_FOR_GATEWAY, VIinfo) != -1)
            add_option_string(packet.options, (unsigned char *)VIinfo);
        }
        //brcm end

#if 1 //ZyXEL,Support DHCP Option 66, Albert
        char *TFTPServerName;
        GetTFTPServername TFTPName;
        int i;
	int found = 0;
        struct opt66    {
            char TFTP, length;
            char str[256];
        } option_66 = { DHCP_OPTION_66, 256, ""};

        // __ZyXEL__, Wood, check option66 from Parameter Request List
        char *ParamRqList = NULL;
        int PrListlength = 0;
        ParamRqList = get_option(oldpacket, DHCP_PARAM_REQ);

        if (ParamRqList != NULL)
           PrListlength = *(ParamRqList - 1);

        for(i=0;i<PrListlength;i++)
        {
           if (ParamRqList[i] == DHCP_OPTION_66)
           {
              found = 1;
              break;
           }
        }

        if (found == 1)
        {
		//__ZyXEL__, Wood, Get TFTP server name fail then skip fill option 66 in DHCP Offer 
		if (getTFTPServerName(&TFTPName) == CMSRET_SUCCESS)
     		{
			option_66.length = strlen(TFTPName.servername);
                        strncpy(option_66.str, TFTPName.servername, option_66.length);
                       	add_option_string(packet.options, (char *) &option_66);
		}
	}
#endif

#if 1 //ZyXEL,Support DHCP Option 15, Ricky
						char *NATINFODomainName;
						GetDomainname DomainName;
						int k;
					int found_opt15 = 0;
						struct opt15	{
							char NatInfo, length;
							char str[256];
						} option_15 = { DHCP_OPTION_15, 256, ""};
				
						// __ZyXEL__, Ricky, check option12 from Parameter Request List
						char *ParamRqList_opt15 = NULL;
						int PrListlength_opt15 = 0;
						ParamRqList_opt15 = get_option(oldpacket, DHCP_PARAM_REQ);
				
						if (ParamRqList_opt15 != NULL)
						   PrListlength_opt15 = *(ParamRqList_opt15 - 1);
				
						for(k=0;k<PrListlength_opt15;k++)
						{
						   if (ParamRqList_opt15[k] == DHCP_OPTION_15)
						   {
							  found_opt15 = 1;
							  break;
						   }
						}
				
						if (found_opt15 == 1)
						{
						//__ZyXEL__, Ricky, Get NAT_INFO Domain name fail then skip fill option 15 in DHCP Offer 
						if (getDomainName(&DomainName) == CMSRET_SUCCESS)
							{
							option_15.length = strlen(DomainName.domainnameval);
										strncpy(option_15.str, DomainName.domainnameval, option_15.length);
										add_option_string(packet.options, (char *) &option_15);
						}
					}
#endif

        addr.s_addr = packet.yiaddr;
        LOG(LOG_INFO, "sending OFFER of %s", inet_ntoa(addr));
#ifdef MSTC_LOG //__MSTC__,Lynn,Log
	sprintf(client_mac, "%02X:%02X:%02X:%02X:%02X:%02X", (unsigned char) packet.chaddr[0], 
		(unsigned char) packet.chaddr[1], (unsigned char) packet.chaddr[2], 
		(unsigned char) packet.chaddr[3], (unsigned char) packet.chaddr[4], 
		(unsigned char) packet.chaddr[5]);

	zyLog_app(LOG_LEVEL_NOTICE, LOG_FAC_DHCPSERVER , "Send DHCP OFFER to %s with IP %s", client_mac, inet_ntoa(addr));
#endif
        return send_packet(&packet, 0);
}


int sendNAK(struct dhcpMessage *oldpacket)
{
        struct dhcpMessage packet;
#ifdef MSTC_LOG //__MSTC__,Lynn,Log
        char client_mac[20];
#endif

        init_packet(&packet, oldpacket, DHCPNAK);
        
        DEBUG(LOG_INFO, "sending NAK");
#ifdef MSTC_LOG //__MSTC__,Lynn,Log
	sprintf(client_mac, "%02X:%02X:%02X:%02X:%02X:%02X", (unsigned char) packet.chaddr[0], 
		(unsigned char) packet.chaddr[1], (unsigned char) packet.chaddr[2], 
		(unsigned char) packet.chaddr[3], (unsigned char) packet.chaddr[4], 
		(unsigned char) packet.chaddr[5]);

	zyLog_app(LOG_LEVEL_NOTICE, LOG_FAC_DHCPSERVER , "Send DHCP NACK to %s", client_mac);
#endif
        return send_packet(&packet, 1);
}


#if 1 //__MSTC__,Lynn,DHCP
struct dhcpOfferedAddr *sendACK(struct dhcpMessage *oldpacket, u_int32_t yiaddr)
#else
int sendACK(struct dhcpMessage *oldpacket, u_int32_t yiaddr)
#endif
{
        struct dhcpMessage packet;
        struct option_set *curr;
        struct dhcpOfferedAddr *offerlist;
        char *lease_time, *vendorid, *userclsid;
#if 1 /* Jennifer, support QoS option 61 & option 125 */
        char *clientid, *vsi;
#endif
        char length = 0;
        u_int32_t lease_time_align = cur_iface->lease;
        struct in_addr addr;
        //brcm begin
        char VIinfo[VENDOR_IDENTIFYING_INFO_LEN];
        char *req;
        int saveVIoptionNeeded = 0;
        //brcm end
#ifdef MSTC_LOG //__MSTC__,Lynn,Log
        char client_mac[20];
#endif
#if 1 /* Jennifer, support QoS option 61 & option 125 */
        int k=0;
#endif

        init_packet(&packet, oldpacket, DHCPACK);
        packet.yiaddr = yiaddr;
        
        if ((lease_time = (char *)get_option(oldpacket, DHCP_LEASE_TIME))) {
                memcpy(&lease_time_align, lease_time, 4);
                lease_time_align = ntohl(lease_time_align);
                if (lease_time_align > cur_iface->lease) 
                        lease_time_align = cur_iface->lease;
                else if (lease_time_align < server_config.min_lease) 
                        lease_time_align = cur_iface->lease;
        }
        
        add_simple_option(packet.options, DHCP_LEASE_TIME, lease_time_align);
        
        curr = cur_iface->options;
        while (curr) {
                if (curr->data[OPT_CODE] != DHCP_LEASE_TIME)
                        add_option_string(packet.options, curr->data);
                curr = curr->next;
        }

        add_bootp_options(&packet);

        //brcm begin
        /* if DHCPRequest from client has device identity, send back gateway identity,
           and save the device identify */
        if ((req = (char *)get_option(oldpacket, DHCP_VENDOR_IDENTIFYING))) {
          if (createVIoption(VENDOR_IDENTIFYING_FOR_GATEWAY, VIinfo) != -1)
          {
            add_option_string(packet.options, (unsigned char *)VIinfo);
          }
          saveVIoptionNeeded = 1;
        }
        //brcm end
#if 1 /*__ZyXEL__, David, support STB info configuration */
        vendorid = get_option(oldpacket, DHCP_VENDOR);
        char op43Value[256] = {0};
        struct opt43 {
            char vendor, length;
            char str[256];
        } option_43 = { DHCP_VSI, 256, ""};

        getSTBOp43(vendorid, op43Value);
        if (op43Value[0] != '\0' && cmsUtl_strcmp(op43Value, "-") != 0)
        {
            int len = strlen(op43Value);
            char hex[4] = {0};
            UBOOL8 isHex = FALSE;

            strncpy(hex, &op43Value[0], 2);
            hex[2] = '\0';
            if (!cmsUtl_strcmp(hex, "0x"))
            {   // hex string
                memmove(&op43Value[0], &op43Value[2], len-1);;
                isHex = TRUE;
            }
            cmsLog_debug("op43Value=%s, len=%d, isHex=%d", op43Value, len, isHex);
            if (isHex)
            {
                // for hex string
                int count = strlen(op43Value);
                char list[256] = {0};
                char tmp[4] = {0};
                int i, j;
            
                list[OPT_CODE] = DHCP_VSI;
                for (i=0, j=0; i<count; i+=2, j++)
                {
                    /* get 2 characters */
                    strncpy(tmp, &op43Value[i], 2);
                    tmp[2] = '\0';
                    /* convert hex string to integer */
                    list[j+2] = (unsigned char *)strtoul(tmp, NULL, 16);
                }
                list[OPT_LEN] = j;
                add_option_string(packet.options, (unsigned char *)list);
            }
            else
            {
                // for printable string
                option_43.length = strlen(op43Value);
                sprintf(option_43.str, "%s", op43Value);
                add_option_string(packet.options, (char *) &option_43);
            }
        }
#endif

#if 1 //ZyXEL,Support DHCP Option 66, Albert
        char *TFTPServerName;
        GetTFTPServername TFTPName;
        int i;
        int found = 0;
        struct opt66    {
            char TFTP, length;
            char str[256];
        } option_66 = { DHCP_OPTION_66, 256, ""};

        // __ZyXEL__, Wood, check option66 from Parameter Request List
        char *ParamRqList = NULL;
        int PrListlength = 0;
        ParamRqList = get_option(oldpacket, DHCP_PARAM_REQ);

        if (ParamRqList != NULL)
           PrListlength = *(ParamRqList - 1);

        for(i=0;i<PrListlength;i++)
        {
           if (ParamRqList[i] == DHCP_OPTION_66)
           {
              found = 1;
              break;
           }
        }

        if (found == 1)
        {
                //__ZyXEL__, Wood, Get TFTP server name fail then skip fill option 66 in DHCP ACK
                if (getTFTPServerName(&TFTPName) == CMSRET_SUCCESS)
                {
                        option_66.length = strlen(TFTPName.servername);
                        strncpy(option_66.str, TFTPName.servername, option_66.length);
                        add_option_string(packet.options, (char *) &option_66);
                }
        }
#endif

#if 1 //ZyXEL,Support DHCP Option 15, Ricky
				char *NATINFODomainName;
				GetDomainname DomainName;
				int l;
				int found_opt15 = 0;
				struct opt15	{
					char NatInfo, length;
					char str[256];
				} option_15 = { DHCP_OPTION_15, 256, ""};
		
				// __ZyXEL__, Ricky, check option15 from Parameter Request List
				char *ParamRqList_opt15 = NULL;
				int PrListlength_opt15 = 0;
				ParamRqList_opt15 = get_option(oldpacket, DHCP_PARAM_REQ);
		
				if (ParamRqList_opt15 != NULL)
				   PrListlength_opt15 = *(ParamRqList_opt15 - 1);
		
				for(l=0;l<PrListlength_opt15;l++)
				{
				   if (ParamRqList_opt15[l] == DHCP_OPTION_15)
				   {
					  found_opt15 = 1;
					  break;
				   }
				}
		
				if (found_opt15 == 1)
				{
						//__ZyXEL__, Ricky, Get NATINFO domain name fail then skip fill option 15 in DHCP ACK
						if (getDomainName(&DomainName) == CMSRET_SUCCESS)
						{
								option_15.length = strlen(DomainName.domainnameval);
								strncpy(option_15.str, DomainName.domainnameval, option_15.length);
								add_option_string(packet.options, (char *) &option_15);
						}
				}
#endif

        addr.s_addr = packet.yiaddr;
        LOG(LOG_INFO, "sending ACK to %s", inet_ntoa(addr));

        if (send_packet(&packet, 0) < 0) 
#ifdef MSTC_LOG //__MSTC__,Lynn,Log
		return NULL;
	sprintf(client_mac, "%02X:%02X:%02X:%02X:%02X:%02X", (unsigned char) packet.chaddr[0], 
		(unsigned char) packet.chaddr[1], (unsigned char) packet.chaddr[2], 
		(unsigned char) packet.chaddr[3], (unsigned char) packet.chaddr[4], 
		(unsigned char) packet.chaddr[5]);

	zyLog_app(LOG_LEVEL_NOTICE, LOG_FAC_DHCPSERVER , "Send DHCP ACK to %s with IP %s", client_mac, inet_ntoa(addr));
#else
                return -1;
#endif

#if 1 //__MSTC__,Lynn,DHCP
        add_lease_MSTC(packet.chaddr, packet.yiaddr, lease_time_align, FALSE);
#else
        add_lease(packet.chaddr, packet.yiaddr, lease_time_align);
#endif
        offerlist = find_lease_by_chaddr(packet.chaddr);
        if (saveVIoptionNeeded)
        {
           saveVIoption(req,offerlist);
        }
        vendorid = (char *)get_option(oldpacket, DHCP_VENDOR);
#if 1 /* Jennifer, support QoS option 61 & option 125 */
        clientid = get_option(oldpacket, DHCP_CLIENT_ID);
        vsi = get_option(oldpacket, DHCP_VENDOR_IDENTIFYING);
#endif
        userclsid = (char *)get_option(oldpacket, DHCP_USER_CLASS_ID);
        memset(offerlist->classid, 0, sizeof(offerlist->classid));
        memset(offerlist->vendorid, 0, sizeof(offerlist->vendorid));
        if( vendorid != NULL){
 	     length = *(vendorid - 1);
	     memcpy(offerlist->vendorid, vendorid, (size_t)length);
	     offerlist->vendorid[(int)length] = '\0';
        }

#if 1 /* Jennifer, support QoS option 61 & option 125 */
        if( clientid != NULL){
 	     length = *(clientid - 1);
		sprintf(offerlist->clientid,"%02X",(unsigned char)clientid[k]);
		 for(k=1;k<length;k++)
		 {
			 sprintf(offerlist->clientid,"%s%02X",offerlist->clientid,(unsigned char)clientid[k]);
		 }
        }
#endif

        if( userclsid != NULL){
 	     length = *(userclsid - 1);
	     memcpy(offerlist->classid, userclsid, (size_t)length);
	     offerlist->classid[(int)length] = '\0';
        }
#if 1 /* Jennifer, support QoS option 61 & option 125 */
        if( vsi != NULL){
			length = *(vsi - 1);
			int k=0;
			sprintf(offerlist->vsi,"%02X",(unsigned char)vsi[0]);
			for(k=1;k<length;k++)
			{
				sprintf(offerlist->vsi,"%s%02X",offerlist->vsi,(unsigned char)vsi[k]);
			}
	}		
#endif

#if 1 //__MSTC__,Lynn,DHCP
        return offerlist;
#else
        return 0;
#endif
}


int send_inform(struct dhcpMessage *oldpacket)
{
        struct dhcpMessage packet;
        struct option_set *curr;
        //brcm begin
        char VIinfo[VENDOR_IDENTIFYING_INFO_LEN];
        char *req;
        //brcm end
#ifdef MSTC_LOG //__MSTC__,Lynn,Log
        char client_mac[20];
#endif


        init_packet(&packet, oldpacket, DHCPACK);
        
        curr = cur_iface->options;
        while (curr) {
                if (curr->data[OPT_CODE] != DHCP_LEASE_TIME)
                        add_option_string(packet.options, curr->data);
                curr = curr->next;
        }

        add_bootp_options(&packet);

        //brcm begin
        /* if DHCPRequest from client has device identity, send back gateway identity,
           and save the device identify */
        if ((req = (char *)get_option(oldpacket, DHCP_VENDOR_IDENTIFYING))) {
          if (createVIoption(VENDOR_IDENTIFYING_FOR_GATEWAY, VIinfo) != -1)
            add_option_string(packet.options, (unsigned char *)VIinfo);
        }
        //brcm end

#ifdef MSTC_LOG //__MSTC__,Lynn,Log
	sprintf(client_mac, "%02X:%02X:%02X:%02X:%02X:%02X", (unsigned char) packet.chaddr[0], 
		(unsigned char) packet.chaddr[1], (unsigned char) packet.chaddr[2], 
		(unsigned char) packet.chaddr[3], (unsigned char) packet.chaddr[4], 
		(unsigned char) packet.chaddr[5]);

	zyLog_app(LOG_LEVEL_NOTICE, LOG_FAC_DHCPSERVER , "Send DHCP INFORM to %s", client_mac);
#endif
        return send_packet(&packet, 0);
}

#if 1 /*__ZyXEL__, David, support STB info configuration */
void getSTBOp43(char *vendorid, char *op43Value)
{
  	vendor_id_t *vid_cur, *vsi_cur;
	vid_cur = cur_iface->opt60ForSTB;
	vsi_cur = cur_iface->opt43ForSTB;
	int vid_idx = 0;
	int vsi_idx = 0;
	char tmpStr[256] = {0};
	
	while (vid_cur) {
		/*
			support wild card Vendor ID
		*/
		if (vid_cur->id[cmsUtl_strlen(vid_cur->id) - 1] == '*')
		{
			fprintf(stderr, "wildcard DHCP option 60 %s\r\n", vid_cur->id);
			if (cmsUtl_strncmp(vid_cur->id, vendorid, cmsUtl_strlen(vid_cur->id) - 1) == 0) {
				break;
			}	
		}
		else
		{
			if (cmsUtl_strncmp(vid_cur->id, vendorid, cmsUtl_strlen(vid_cur->id)) == 0) {
				fprintf(stderr, "DHCP option 60 %s\r\n", vid_cur->id);
				break;
			}	
		}
		vid_cur = vid_cur->next;
		vid_idx++;
	}
	while (vsi_cur) {
	    if (vsi_idx == vid_idx) {
	        sprintf(tmpStr, "%s%s", tmpStr, vsi_cur->id);
	        strcpy(op43Value, tmpStr);
	        cmsLog_debug("op43Value=%s", op43Value);
            break;
	    }
	    vsi_cur = vsi_cur->next;
		vsi_idx++;
	}
	
	return;
}
#endif
