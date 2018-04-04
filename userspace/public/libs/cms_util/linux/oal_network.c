/***********************************************************************
 *
 *  Copyright (c) 2007  Broadcom Corporation
 *  All Rights Reserved
 *
<:label-BRCM:2012:DUAL/GPL:standard

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2, as published by
the Free Software Foundation (the "GPL").

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.


A copy of the GPL is available at http://www.broadcom.com/licenses/GPLv2.php, or by
writing to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.

:>
 * 
 ************************************************************************/

#include <errno.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>

#if 1 //__MSTC__,Lynn,DHCP
#include <sys/sysinfo.h>
#include <netinet/if_ether.h>
#include <linux/if_bridge.h>
#include <net/if_arp.h>
#include <netinet/in.h>
#include <linux/sockios.h>
#endif

#include "bcmnet.h"
#include "cms.h"
#include "cms_util.h"

/** Ported from getLanInfo
 *
 */
CmsRet oal_getLanInfo(const char *lan_ifname, struct in_addr *lan_ip, struct in_addr *lan_subnetmask)
{
#ifdef DESKTOP_LINUX

   cmsLog_debug("fake ip info for interface %s", lan_ifname);
   lan_ip->s_addr = 0xc0a80100; /* 192.168.1.0 */
   lan_subnetmask->s_addr = 0xffffff00; /* 255.255.255.0 */
   return CMSRET_SUCCESS;

#else

   int socketfd;
   struct ifreq lan;

   if ((socketfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
      cmsLog_error("failed to open socket, errno=%d", errno);
      return CMSRET_INTERNAL_ERROR;
   }

   strcpy(lan.ifr_name,lan_ifname);
   if (ioctl(socketfd,SIOCGIFADDR,&lan) < 0) {
      cmsLog_error("SIOCGIFADDR failed, errno=%d", errno);
      close(socketfd);
      return CMSRET_INTERNAL_ERROR;
   }
   *lan_ip = ((struct sockaddr_in *)&(lan.ifr_addr))->sin_addr;

   if (ioctl(socketfd,SIOCGIFNETMASK,&lan) < 0) {
      cmsLog_error("SIOCGIFNETMASK failed, errno=%d", errno);
      close(socketfd);
      return CMSRET_INTERNAL_ERROR;
   }

   *lan_subnetmask = ((struct sockaddr_in *)&(lan.ifr_netmask))->sin_addr;

   close(socketfd);
   return CMSRET_SUCCESS;
   
#endif
}


/** Return true if the specified interface is up.
 *
 * ported from bcmCheckInterfaceUp.
 */
UBOOL8 oal_isInterfaceUp(const char *ifname)
{
   int  skfd;
   struct ifreq intf;
   UBOOL8 isUp = FALSE;


   if ((skfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
      return FALSE;
   }

   strcpy(intf.ifr_name, ifname);

   // if interface is br0:0 and
   // there is no binding IP address then return down
   if ( strchr(ifname, ':') != NULL ) {
      if (ioctl(skfd, SIOCGIFADDR, &intf) < 0) {
         close(skfd);
         return FALSE;
      }
   }

   // if interface flag is down then return down
   if (ioctl(skfd, SIOCGIFFLAGS, &intf) == -1) {
      isUp = 0;
   } else {
      isUp = (intf.ifr_flags & IFF_UP) ? TRUE : FALSE;
   }

   close(skfd);

   return isUp;
}


/* Get the existing interface names in the kernel, regardless they're active
 * or not. If success, the ifNameList will be assigned a new allocated string
 * containing names separated by commas. It may look like
 * "lo,dsl0,eth0,eth1,usb0,wl0".
 *
 * Caller should free ifNameList by cmsMem_free() after use.
 *
 * Return CMSRET_SUCCESS if success, error code otherwise.
 */
CmsRet oalNet_getIfNameList(char **ifNameList)
{
#ifdef DESKTOP_LINUX

   *ifNameList = cmsMem_alloc(512, 0);
   sprintf(*ifNameList, "lo,dsl0,eth0,eth1,usb0,moca0,moca1");

#else
   struct if_nameindex *ni_list = if_nameindex();
   struct if_nameindex *ni_list2 = ni_list;
   char buf[1024];
   char *pbuf = buf;
   int len;

   if (ni_list == NULL)
      return CMSRET_INTERNAL_ERROR;

   /* Iterate through the array of interfaces to concatenate interface
    * names, separated by commas */
   while(ni_list->if_index) {
      len = strlen(ni_list->if_name);
      memcpy(pbuf, ni_list->if_name, len);
      pbuf += len;
      *pbuf++ = ',';
      ni_list++;
   }
   len = pbuf - buf;
   buf[len-1] = 0;

   if_freenameindex(ni_list2);

   /* Allocate dynamic memory for interface name list */
   if ((*ifNameList = cmsMem_alloc(len, 0)) == NULL)
      return CMSRET_RESOURCE_EXCEEDED;
   memcpy(*ifNameList, buf, len);
   
#endif /* DESKTOP_LINUX */

   return CMSRET_SUCCESS;
}


CmsRet oal_Net_getPersistentWanIfNameList(char **persistentWanIfNameList)
{

#ifdef DESKTOP_LINUX

   *persistentWanIfNameList = cmsMem_alloc(512, 0);
   sprintf(*persistentWanIfNameList, "eth1,moca0");

#else

   SINT32  skfd;
   struct ifreq ifr;

   if ((skfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) 
   {
      cmsLog_error("Error openning socket for getting the enet WAN list");
      return CMSRET_INTERNAL_ERROR;
   }
   
    /* Get the name -> if_index mapping for ethswctl */
    strcpy(ifr.ifr_name, "bcmsw");
    if (ioctl(skfd, SIOCGIFINDEX, &ifr) < 0) 
    {
        close(skfd);
        cmsLog_debug("bcmsw interface does not exist.  Error: %d", errno);
        return CMSRET_INTERNAL_ERROR;
    }

   /* Allocate dynamic memory to hold max interface names (eth0,eth1,..eth10<cr>)*/
   if ((*persistentWanIfNameList = cmsMem_alloc(((MAX_PERSISTENT_WAN_PORT * (IFNAMSIZ+1)) + 2), ALLOC_ZEROIZE)) == NULL)
   {
      cmsLog_error("Fail to alloc mem in getting the enet WAN list");
      close(skfd);      
      return CMSRET_RESOURCE_EXCEEDED;
   }

   memset((void *) &ifr, sizeof(ifr), 0);
   ifr.ifr_data = *persistentWanIfNameList;
   if (ioctl(skfd, SIOCGWANPORT, &ifr) < 0)
   {
      cmsLog_error("ioct error in getting the enet WAN list.  Error: %d", errno);
      close(skfd);
      CMSMEM_FREE_BUF_AND_NULL_PTR(*persistentWanIfNameList);
      return CMSRET_INTERNAL_ERROR;
   }

   close(skfd);

   cmsLog_debug("WannEnetPortList=%s, strlen=%d", *persistentWanIfNameList, strlen(*persistentWanIfNameList));

#endif /* DESKTOP_LINUX */

   return CMSRET_SUCCESS;
   
}

CmsRet oal_Net_getGMACPortIfNameList(char **GMACPortIfNameList)
{

#ifdef CMS_BRCM_GMAC

#ifdef DESKTOP_LINUX

   *GMACPortIfNameList = cmsMem_alloc(512, 0);
   strcpy(*GMACPortIfNameList, "eth1,eth3");

#else

   SINT32  skfd;
   struct ifreq ifr;

   if ((skfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) 
   {
      cmsLog_error("Error openning socket for getting the  GMAC enet port list");
      return CMSRET_INTERNAL_ERROR;
   }
   
    /* Get the name -> if_index mapping for ethswctl */
    strcpy(ifr.ifr_name, "bcmsw");
    if (ioctl(skfd, SIOCGIFINDEX, &ifr) < 0) 
    {
        close(skfd);
        cmsLog_debug("bcmsw interface does not exist.  Error: %d", errno);
        return CMSRET_INTERNAL_ERROR;
    }

   /* Allocate dynamic memory to hold max interface names (eth0,eth1,..eth10<cr>)*/
   if ((*GMACPortIfNameList = cmsMem_alloc(((MAX_GMAC_ETH_PORT * (IFNAMSIZ+1)) + 2), ALLOC_ZEROIZE)) == NULL)
   {
      cmsLog_error("Fail to alloc mem in getting the GMAC enet port list");
      close(skfd);      
      return CMSRET_RESOURCE_EXCEEDED;
   }

   memset((void *) &ifr, sizeof(ifr), 0);
   ifr.ifr_data = *GMACPortIfNameList;
   if (ioctl(skfd, SIOCGGMACPORT, &ifr) < 0)
   {
      cmsLog_error("ioct error in getting the GMAC enet port list.  Error: %d", errno);
      close(skfd);
      CMSMEM_FREE_BUF_AND_NULL_PTR(*GMACPortIfNameList);
      return CMSRET_INTERNAL_ERROR;
   }

   close(skfd);

   cmsLog_debug("GMACPortIfNameList=%s, strlen=%d", *GMACPortIfNameList, strlen(*GMACPortIfNameList));

#endif /* DESKTOP_LINUX */

#endif /* CMS_BRCM_GMAC */

   return CMSRET_SUCCESS;
   
}



#ifdef DMP_X_5067F0_IPV6_1 /* aka SUPPORT_IPV6 */

/** Get the global unicast ipv6 address of the interface.
 *
 */
CmsRet oal_getLanAddr6(const char *ifname, char *ipAddr)
{
   FILE *fp;
   char *space, *p1, *p2;
   char line[BUFLEN_64];
   SINT32 i;

   *ipAddr = '\0';

   if ((fp = fopen("/proc/net/if_inet6", "r")) == NULL)
   {
      /* error */
      cmsLog_error("failed to open /proc/net/if_inet6");
      return CMSRET_INTERNAL_ERROR;
   }

   while (fgets(line, sizeof(line), fp) != NULL)
   {
      if (strstr(line, ifname) != NULL && strncmp(line, "fe80", 4) != 0)
      {
         /* the first string in the line is the ip address */
         if ((space = strchr(line, ' ')) != NULL)
         {
            /* terminate the ip address string */
            *space = '\0';
         }

         /* insert a colon every 4 digits in the address string */
         p2 = ipAddr;
         for (i = 0, p1 = line; *p1 != '\0'; i++)
         {
            if (i == 4)
            {
               i = 0;
               *p2++ = ':';
            }
            *p2++ = *p1++;
         }

         /* append prefix length 64 */
         *p2++ = '/';
         *p2++ = '6';
         *p2++ = '4';
         *p2 = '\0';
         break;   /* done */
      }
   }

   fclose(fp);

   return CMSRET_SUCCESS;

}  /* End of oal_getLanAddr6() */

/** Get the ipv6 address of the interface.
 *
 */
CmsRet oal_getIfAddr6(const char *ifname, UINT32 addrIdx,
                      char *ipAddr, UINT32 *ifIndex, UINT32 *prefixLen, UINT32 *scope, UINT32 *ifaFlags)
{
   CmsRet   ret = CMSRET_NO_MORE_INSTANCES;
   FILE     *fp;
   SINT32   count = 0;
   char     line[BUFLEN_64];

   *ipAddr = '\0';

   if ((fp = fopen("/proc/net/if_inet6", "r")) == NULL)
   {
      cmsLog_error("failed to open /proc/net/if_inet6");
      return CMSRET_INTERNAL_ERROR;
   }

   while (fgets(line, sizeof(line), fp) != NULL)
   {
      /* remove the carriage return char */
      line[strlen(line)-1] = '\0';

      if (strstr(line, ifname) != NULL)
      {
         char *addr, *ifidx, *plen, *scp, *flags, *devname; 
         char *nextToken = NULL;

         /* the first token in the line is the ip address */
         addr = strtok_r(line, " ", &nextToken);

         /* the second token is the Netlink device number (interface index) in hexadecimal */
         ifidx = strtok_r(NULL, " ", &nextToken);
         if (ifidx == NULL)
         {
            cmsLog_error("Invalid /proc/net/if_inet6 line");
            ret = CMSRET_INTERNAL_ERROR;
            break;
         }
            
         /* the third token is the Prefix length in hexadecimal */
         plen = strtok_r(NULL, " ", &nextToken);
         if (plen == NULL)
         {
            cmsLog_error("Invalid /proc/net/if_inet6 line");
            ret = CMSRET_INTERNAL_ERROR;
            break;
         }
            
         /* the forth token is the Scope value */
         scp = strtok_r(NULL, " ", &nextToken);
         if (scp == NULL)
         {
            cmsLog_error("Invalid /proc/net/if_inet6 line");
            ret = CMSRET_INTERNAL_ERROR;
            break;
         }
            
         /* the fifth token is the ifa flags */
         flags = strtok_r(NULL, " ", &nextToken);
         if (flags == NULL)
         {
            cmsLog_error("Invalid /proc/net/if_inet6 line");
            ret = CMSRET_INTERNAL_ERROR;
            break;
         }
            
         /* the sixth token is the device name */
         devname = strtok_r(NULL, " ", &nextToken);
         if (devname == NULL)
         {
            cmsLog_error("Invalid /proc/net/if_inet6 line");
            ret = CMSRET_INTERNAL_ERROR;
            break;
         }
         else
         {
            if (strcmp(devname, ifname) != 0)
            {
               continue;
            }
            else if (count == addrIdx)
            {
               SINT32   i;
               char     *p1, *p2;

               *ifIndex   = strtoul(ifidx, NULL, 16);
               *prefixLen = strtoul(plen, NULL, 16);
               *scope     = strtoul(scp, NULL, 16);
               *ifaFlags  = strtoul(flags, NULL, 16);

               /* insert a colon every 4 digits in the address string */
               p2 = ipAddr;
               for (i = 0, p1 = addr; *p1 != '\0'; i++)
               {
                  if (i == 4)
                  {
                     i = 0;
                     *p2++ = ':';
                  }
                  *p2++ = *p1++;
               }
               *p2 = '\0';
			   /* The IPv6 address stored in data model are supposed to be well-formatted. Merge DSL-491GNU-B1D_STD for Telus, r5714*/
			   struct in6_addr s;
			   inet_pton(AF_INET6, ipAddr, (void *)&s);
			   inet_ntop(AF_INET6, (void *)&s, ipAddr, CMS_IPADDR_LENGTH);

               ret = CMSRET_SUCCESS;
               break;   /* done */
            }
            else
            {
               count++;
            }
         }
      }
   }  /* while */

   fclose(fp);

   return ret;

}  /* End of oal_getIfAddr6() */

#endif
#if 1 //__MSTC__,Lynn,DHCP
CmsRet oal_getPortNameFromMac(const char *ifName, const unsigned char *macAddr, char *portName)
{
   int sfd;
   int portid = -1;
   int i;
   int n=-1;
   int retries=0;
   CmsRet ret=CMSRET_SUCCESS;

   struct ifreq ifr;
   struct __fdb_entry fe[128];
   int ifindices[1024];
   unsigned long args0[4] = { BRCTL_GET_FDB_ENTRIES,
                              (unsigned long) fe,
                              sizeof(fe)/sizeof(struct __fdb_entry), 0 };
   unsigned long args1[4] = { BRCTL_GET_PORT_LIST,
                              (unsigned long) ifindices,
                              sizeof(ifindices)/sizeof(int), 0 };


   if ((sfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
   {
      return CMSRET_INTERNAL_ERROR;
   }

   memset(ifr.ifr_name, 0, IFNAMSIZ);
   strncpy(ifr.ifr_name, ifName, IFNAMSIZ);
   ifr.ifr_data = (char *) args0;

   /*
    * The orignal code from Keven would allow the kernel to return EAGAIN an
    * infinite number of times, trapping us in this while loop.  I think the
    * right thing to do is only allow EAGAIN to be returned up to 10 times.
    */
   while (n < 0 && retries < 10)
   {
      n = ioctl(sfd, SIOCDEVPRIVATE, &ifr);
      if (n < 0)
      {
         cmsLog_error("showmacs error %d n=%d EAGAIN=%d", retries, n, errno == EAGAIN ? 1:0);

         if (errno == EAGAIN)
         {
            sleep(0);
            retries++;
         }
         else
         {
            break;
         }
      }
   }

   if (n < 0)
   {
      cmsLog_error("showmacs failed, n=%d retries=%d", n, retries);
      close(sfd);
      return CMSRET_INTERNAL_ERROR;
   }
   else
   {
      cmsLog_debug("got %d mac addresses from kernel", n);
   }

   for (i = 0; i < n; i++) {
      if (memcmp(macAddr, fe[i].mac_addr, MAC_ADDR_LEN) == 0)
      {
         portid = fe[i].port_no;
         cmsLog_debug("found port id = %d", portid);
         break;
      }
   }

   if (portid == -1)
   {
      close(sfd);
      return CMSRET_INTERNAL_ERROR;
   }


   memset(ifindices, 0, sizeof(ifindices));
   strncpy(ifr.ifr_name, ifName, IFNAMSIZ);
   ifr.ifr_data = (char *) &args1;

   n = ioctl(sfd, SIOCDEVPRIVATE, &ifr);

   close(sfd);

   if (n < 0) {
      cmsLog_error("list ports for bridge: br0 failed: %s",
                   strerror(errno));
      return CMSRET_INTERNAL_ERROR;
   }

   if (ifindices[portid] == 0)
   {
      cmsLog_error("ifindices[portid] is zero!, portid=%d", portid);
      return CMSRET_INTERNAL_ERROR;
   }

   if (!if_indextoname(ifindices[portid], portName))
   {
      cmsLog_error("if_indextoname failed, ifindices[portid]=%d", ifindices[portid]);
      return CMSRET_INTERNAL_ERROR;
   }

   if (strlen(portName) == 0)
   {
      cmsLog_error("empty portName?");
      ret = CMSRET_INTERNAL_ERROR;
   }
   else
   {
      cmsLog_debug("PortName=%s", portName);
   }

   return ret;
}

CmsRet oal_getMacFromIpAddr(const struct in_addr ipaddr, const char *brName, unsigned char *macaddr)
{
   int sfd;
   struct arpreq areq;
   struct sockaddr_in *sin;

   if ((sfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
   {
      return CMSRET_INTERNAL_ERROR;
   }

   // Richard, Check if ipaddr is 127.0.0.1
   if(!cmsUtl_strcmp(inet_ntoa(ipaddr), "127.0.0.1")){
      return CMSRET_INTERNAL_ERROR;
   }

   memset(&areq, 0, sizeof(areq));
   sin = (struct sockaddr_in *) &areq.arp_pa;
   sin->sin_family = AF_INET;

   sin->sin_addr = ipaddr;
   sin = (struct sockaddr_in *) &areq.arp_ha;
   sin->sin_family = ARPHRD_ETHER;

   strncpy(areq.arp_dev, brName, IFNAMSIZ);
   if (ioctl(sfd, SIOCGARP, (caddr_t) &areq) == -1) {
      close(sfd);
      cmsLog_error("Unable to make ARP request for %s", inet_ntoa(((struct sockaddr_in *) &areq.arp_pa)->sin_addr));
      return CMSRET_INTERNAL_ERROR;
   }

   close(sfd);
   memcpy(macaddr, areq.arp_ha.sa_data, 6);
   return CMSRET_SUCCESS;
}
#endif

#ifdef DMP_X_5067F0_IPV6_1 // __MSTC__, Richard Huang
CmsRet oal_getIfLinkLocalAddr6(const char *ifname, char *ipAddr)
{
   FILE *fp;
   char *space, *p1, *p2;
   char line[BUFLEN_64];
   SINT32 i;

   *ipAddr = '\0';

   if ((fp = fopen("/proc/net/if_inet6", "r")) == NULL)
   {
      /* error */
      cmsLog_error("failed to open /proc/net/if_inet6");
      return CMSRET_INTERNAL_ERROR;
   }

   while (fgets(line, sizeof(line), fp) != NULL)
   {
      if (strstr(line, ifname) != NULL && strncmp(line, "fe80", 4) == 0)
      {
         /* the first string in the line is the ip address */
         if ((space = strchr(line, ' ')) != NULL)
         {
            /* terminate the ip address string */
            *space = '\0';
         }

         /* insert a colon every 4 digits in the address string */
         p2 = ipAddr;
         for (i = 0, p1 = line; *p1 != '\0'; i++)
         {
            if (i == 4)
            {
               i = 0;
               *p2++ = ':';
            }
            *p2++ = *p1++;
         }
         *p2 = '\0';

         break;   /* done */
      }
   }

   fclose(fp);

   return CMSRET_SUCCESS;
}  /* End of oal_getIfLinkLocalAddr6() */
#endif // DMP_X_5067F0_IPV6_1
