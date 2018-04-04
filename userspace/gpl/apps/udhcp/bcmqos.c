/***********************************************************************
 * <:copyright-BRCM:2010:DUAL/GPL:standard
 * 
 *    Copyright (c) 2010 Broadcom Corporation
 *    All Rights Reserved
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2, as published by
 * the Free Software Foundation (the "GPL").
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * 
 * A copy of the GPL is available at http://www.broadcom.com/licenses/GPLv2.php, or by
 * writing to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 * 
 * :>
************************************************************************/
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include "dhcpd.h"
#include "cms_log.h"

/* Externs */
extern struct iface_config_t *iface_config;

typedef struct ExecIP {
   u_int32_t ipaddr;
   unsigned char execflag;
   u_int32_t index;
}EXECIP, PEXECIP;

typedef struct optioncmd {
   char command[1024];
   char action;
   int optionnum;
   char optionval[256];
   /* __MSTC__, klose, QoS classifier rule DHCP option exclude support, 20121207 */
   int exclude;
   struct ExecIP execip[254];
   struct optioncmd *pnext;
}OPTIONCMD, POPTIONCMD;

struct optioncmd *optioncmdHead = NULL;

void bcmDelObsoleteRules(void);
void bcmExecOptCmd(void);
/* __MSTC__, klose, QoS classifier rule DHCP option exclude support, 20121207 */
void bcmQosDhcp(int optionnum, CmsMsgHeader *msg);

static char bcmParseCmdAction(char *cmd);
static void bcmSetQosRule(char action, char *command, u_int32_t leaseip);
static void bcmAddOptCmdIP(struct optioncmd * optcmd, u_int32_t leaseip, int index);
/* __MSTC__, klose, QoS classifier rule DHCP option exclude support, 20121207 */
static struct optioncmd * bcmAddOptCmd(int optionnum, char action, char *cmd, UINT32 exclude);
static void bcmDelOptCmd(char *cmd);


char bcmParseCmdAction(char *cmd)
{
   char *token;
   char action = '\0';

   if ((token = strstr(cmd, "-A ")) == NULL)
   {
      if ((token = strstr(cmd, "-I ")) == NULL)
      {
         token = strstr(cmd, "-D ");
      }
   }
   if (token != NULL)
   {
      action = token[1];

      /* replace the command token with %s */
      token[0] = '%';
      token[1] = 's';
   }

   return action;

}  /* End of bcmParseCmdAction() */

void bcmSetQosRule(char action, char *command, u_int32_t leaseip)
{
   char *ptokenstart;
   char cmdseg[1024];
   char actionStr[3];   /* -A or -I or -D */
   struct in_addr ip;

   strcpy(cmdseg, command);
   ptokenstart = strstr(cmdseg, "[");
   ip.s_addr   = leaseip;
   strcpy(ptokenstart, inet_ntoa(ip));
   strcat(cmdseg, strstr(command, "]") + 1);
   sprintf(actionStr, "-%c", action);
   sprintf(cmdseg, cmdseg, actionStr);
   system(cmdseg);
    
}  /* End of bcmSetQosRule() */

void bcmAddOptCmdIP(struct optioncmd * optcmd, u_int32_t leaseip, int index)
{
   /* if lease ip address is the same and the QoS rule has been executed, do nothing */  
   if (optcmd->execip[index].ipaddr != leaseip || !optcmd->execip[index].execflag)
   {
      if (optcmd->execip[index].execflag)
      {
         /* delete the QoS rule with the old lease ip */
         bcmSetQosRule('D', optcmd->command, optcmd->execip[index].ipaddr);
         optcmd->execip[index].execflag = 0;
      }
      optcmd->execip[index].ipaddr = leaseip;
      optcmd->execip[index].execflag = 1;

      /* add QoS rule with the new lease ip */
      bcmSetQosRule(optcmd->action, optcmd->command, leaseip);
   }
}  /* End of bcmAddOptCmdIP() */

void bcmExecOptCmd(void)
{
   struct optioncmd *pnode;
	struct iface_config_t *iface;
	uint32_t i;

   /* execute all the commands in the option command list */
   for (pnode = optioncmdHead; pnode != NULL; pnode = pnode->pnext)
   {
	   for (iface = iface_config; iface; iface = iface->next)
	   {
		   for (i = 0; i < iface->max_leases; i++)
		   {
            /* skip if lease expires */
            if (lease_expired(&(iface->leases[i])))
               continue;

			   switch (pnode->optionnum)
			   {
				   case DHCP_VENDOR:
                  /* __MSTC__, klose, QoS classifier rule DHCP option exclude support, 20121207 */
					   if (!strcmp(iface->leases[i].vendorid,	pnode->optionval) ^ pnode->exclude)
					   {
						   bcmAddOptCmdIP(pnode, iface->leases[i].yiaddr, i);
					   }
					   break;
				   case DHCP_CLIENT_ID:
					   //printf("op61 not implement, please use the MAC filter\r\n");
#if 1 /* Jennifer, support QoS option 61 & option 125 */
                  /* __MSTC__, klose, QoS classifier rule DHCP option exclude support, 20121207 */
					   if (!strcasecmp(iface->leases[i].clientid,	pnode->optionval) ^ pnode->exclude)
					   {
						   bcmAddOptCmdIP(pnode, iface->leases[i].yiaddr, i);
					   }
#endif
					   break;
				   case DHCP_USER_CLASS_ID:
                  /* __MSTC__, klose, QoS classifier rule DHCP option exclude support, 20121207 */
					   if (!strcmp(iface->leases[i].classid, pnode->optionval) ^ pnode->exclude)
					   {
						   bcmAddOptCmdIP(pnode, iface->leases[i].yiaddr, i);
					   }
					   break;
#if 1 /* Jennifer, support QoS option 61 & option 125 */
				   case DHCP_VENDOR_IDENTIFYING:
                  /* __MSTC__, klose, QoS classifier rule DHCP option exclude support, 20121207 */
					   if (!strcasecmp(iface->leases[i].vsi, pnode->optionval) ^ pnode->exclude)
					   {
						   bcmAddOptCmdIP(pnode, iface->leases[i].yiaddr, i);
					   }
					   break;
#endif
				   default:
					   break;
			   }	
		   }
	   }
   }
}  /* End of bcmExecOptCmd() */
/* __MSTC__, klose, QoS classifier rule DHCP option exclude support, 20121207 */
struct optioncmd * bcmAddOptCmd(int optionnum, char action, char *cmd, UINT32 exclude)
{
   struct optioncmd *p, *pnode;
   char *ptokenstart, *ptokenend;

   for (pnode = optioncmdHead; pnode != NULL; pnode = pnode->pnext)
   {
      if (!strcmp(pnode->command, cmd))
         return NULL;
   }	

   pnode = (struct optioncmd *)malloc(sizeof(struct optioncmd));
   if ( pnode == NULL )
   {
      cmsLog_error("malloc failed");
      return NULL;
   }

   memset(pnode, 0, sizeof(struct optioncmd));
	/* __MSTC__, klose, QoS classifier rule DHCP option exclude support, 20121207 */
   pnode->exclude = exclude;
   strcpy(pnode->command, cmd);
   pnode->action = action;
   pnode->optionnum = optionnum;
   ptokenstart = strstr(cmd, "[");
   ptokenend = strstr(cmd, "]");
   strncpy(pnode->optionval, ptokenstart + 1, (size_t)(ptokenend - ptokenstart - 1));
   pnode->optionval[ptokenend - ptokenstart - 1] = '\0';
   p = optioncmdHead;	
   optioncmdHead = pnode;
   optioncmdHead->pnext = p;
   return pnode;

}  /* End of bcmAddOptCmd() */

void bcmDelOptCmd(char *cmd)
{
   struct optioncmd *pnode, *pprevnode;
   int i;

   pnode = pprevnode = optioncmdHead;
   for ( ; pnode != NULL;)
   {
      if (!strcmp(pnode->command, cmd))
      {
         /* delete all the ebtables or iptables rules that had been executed */
         for (i = 0; i < 254; i++)
         {
            if (pnode->execip[i].execflag)
            {
               bcmSetQosRule('D', pnode->command, pnode->execip[i].ipaddr);
               pnode->execip[i].execflag = 0;
            }
         }

         /* delete the option command node from the list */	       
         if (optioncmdHead == pnode)
            optioncmdHead = pnode->pnext;
         else
            pprevnode->pnext = pnode->pnext;
         free(pnode);
         break;
      }
      else
      {
         pprevnode = pnode;
         pnode = pnode->pnext;
      }
   }
}  /* End of bcmDelOptCmd() */
/* __MSTC__, klose, QoS classifier rule DHCP option exclude support, 20121207 */
void bcmQosDhcp(int optionnum, CmsMsgHeader *msg)
{
   char action;
   /* __MSTC__, klose, QoS classifier rule DHCP option exclude support, 20121207 */
   char *cmd = (char *)(msg+1);

	action = bcmParseCmdAction(cmd);

	switch (action)
	{
		case 'A':
		case 'I':
         /* __MSTC__, klose, QoS classifier rule DHCP option exclude support, 20121207 */
			if (bcmAddOptCmd(optionnum, action, cmd, msg->wordData) != NULL)
            bcmExecOptCmd();
         else
            cmsLog_error("bcmAddOptCmd returns error");
			break;
		case 'D':
			bcmDelOptCmd(cmd);
			break;
		default:
			cmsLog_error("incorrect command action");
			break;
	}
}  /* End of bcmQosDhcp() */

void bcmDelObsoleteRules(void)
{
   struct optioncmd *pnode;
	struct iface_config_t *iface;
   uint32_t delete;
   uint32_t i;

   for (pnode = optioncmdHead; pnode != NULL; pnode = pnode->pnext)
   {
      delete = 1;      
	   for (iface = iface_config; iface && delete; iface = iface->next)
	   {
		   for (i = 0; (i < iface->max_leases) && delete; i++)
		   {
            if (lease_expired(&(iface->leases[i])))
               continue;

			   switch (pnode->optionnum)
			   {
				   case DHCP_VENDOR:
					   if (!strcmp(iface->leases[i].vendorid,	pnode->optionval))
                     delete = 0;
					   break;
				   case DHCP_CLIENT_ID:
					   //printf("op61 not implement, please use the MAC filter\r\n");
#if 1 /* Jennifer, support QoS option 61 & option 125 */
						 if (!strcasecmp(iface->leases[i].clientid, pnode->optionval))
					   delete = 0;
#endif
					   break;
				   case DHCP_USER_CLASS_ID:
					   if (!strcmp(iface->leases[i].classid, pnode->optionval))
                     delete = 0;
					   break;
#if 1 /* Jennifer, support QoS option 61 & option 125 */
				   case DHCP_VENDOR_IDENTIFYING:
						 if (!strcasecmp(iface->leases[i].vsi, pnode->optionval))
					   delete = 0;
						 break;
#endif
				   default:
					   break;
			   }	
		   }
	   }

      if (delete)
      {
         /* delete all the ebtables or iptables rules that had been executed */
         for (i = 0; i < 254; i++)
         {
            if (pnode->execip[i].execflag)
            {
               bcmSetQosRule('D', pnode->command, pnode->execip[i].ipaddr);
               pnode->execip[i].execflag = 0;
            }
         }
      }
   }
}  /* End of bcmDelObsoleteRules() */

