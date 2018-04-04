/***********************************************************************
 *
 *  Copyright (c) 2006-2010  Broadcom Corporation
 *  All Rights Reserved
 *
# 
# 
# This program is free software; you can redistribute it and/or modify 
# it under the terms of the GNU General Public License, version 2, as published by  
# the Free Software Foundation (the "GPL"). 
# 
#
# 
# This program is distributed in the hope that it will be useful,  
# but WITHOUT ANY WARRANTY; without even the implied warranty of  
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the  
# GNU General Public License for more details. 
#  
# 
#  
#   
# 
# A copy of the GPL is available at http://www.broadcom.com/licenses/GPLv2.php, or by 
# writing to the Free Software Foundation, Inc., 59 Temple Place - Suite 330, 
# Boston, MA 02111-1307, USA. 
#
 *
************************************************************************/


#include <fcntl.h>      /* open */ 
#include "cms.h"
#include "cms_log.h"
#include "cms_eid.h"
#include "oal.h"
#include <stdio.h> 
#include <stdlib.h>


/** local definitions **/

/* default settings */

/** external functions **/

/** external data **/

/** internal functions **/

/** public data **/

/** private data **/
static CmsEntityId gEid;
static CmsLogLevel             logLevel; /**< Message logging level.
                                          * This is set to one of the message
                                          * severity levels: LOG_ERR, LOG_NOTICE
                                          * or LOG_DEBUG. Messages with severity
                                          * level lower than or equal to logLevel
                                          * will be logged. Otherwise, they will
                                          * be ignored. This variable is local
                                          * to each process and is changeable
                                          * from CLI.
                                          */ 
static CmsLogDestination logDestination; /**< Message logging destination.
                                          * This is set to one of the
                                          * message logging destinations:
                                          * STDERR or SYSLOGD. This
                                          * variable is local to each
                                          * process and is changeable from
                                          * CLI.
                                          */
static UINT32 logHeaderMask; /**< Bitmask of which pieces of info we want
                              *   in the log line header.
                              */ 
#ifdef MSTC_LOG //__MSTC__,Lynn,Log
static char curUserName[BUFLEN_16] = {0};

void zyLog_cfgChangeUser(const char *userName)
{
	cmsUtl_strncpy(curUserName, userName, BUFLEN_16);
}

void zyLog_app(CmsLogLevel level, CmsLogFacility category, const char *detail, ...)
{
	va_list ap;
	int len=0, maxLen;
	char *categoryStr=NULL;
	char buf[MAX_LOG_LINE_LENGTH] = {0};

	maxLen = sizeof(buf);
	va_start(ap, detail);

    //len = snprintf(buf, maxLen, "%s:", einfo->name);
/*	time(&now);
	ctime_r(&now, timestamp);
	timeStr = timestamp + 4;
	timeStr[15] = '\0';
	len = snprintf(buf, maxLen, "%s ", timeStr);
*/	
	switch(category)
	{
		case LOG_FAC_DHCPCLIENT:
			categoryStr = "DHCP_client";
			break;
		case LOG_FAC_PPPOE:
			categoryStr = "PPPoE";
			break;
		case LOG_FAC_MOCA:
			categoryStr = "MoCA";
			break;
		case LOG_FAC_WIFI:
			categoryStr = "Wireless";
			break;
		case LOG_FAC_DHCPSERVER:
			categoryStr = "DHCP_server";
			break;
		case LOG_FAC_UPNP:
			categoryStr = "UPnP";
			break;
		case LOG_FAC_DLNA:
			categoryStr = "DLNA";
			break;
		case LOG_FAC_NAT:
			categoryStr = "NAT";
			break;
		case LOG_FAC_FIREWALL:
			categoryStr = "Sec_Firewall";
			break;
		case LOG_FAC_MACFILTER:
			categoryStr = "Sec_MAC_filter";
			break;
		case LOG_FAC_ALLOWEDWEB:
			categoryStr = "Sec_Forwarded_web";
			break;
		case LOG_FAC_BLOCKEDWEB:
			categoryStr = "Sec_Blocked_web";
			break;
		case LOG_FAC_ATTACK:
			categoryStr = "Sec_Attack";
			break;
		case LOG_FAC_CERT:
			categoryStr = "Sec_Certificate";
			break;
		case LOG_FAC_IPSEC:
			categoryStr = "Sec_IPSec";
			break;
		case LOG_FAC_ROUTE:
			categoryStr = "Static_route";
			break;
		case LOG_FAC_DDNS:
			categoryStr = "DDNS";
			break;
		case LOG_FAC_IGMP:
			categoryStr = "IGMP";
			break;
		case LOG_FAC_QOS:
			categoryStr = "QoS";
			break;
		case LOG_FAC_TR069:
			categoryStr = "TR069";
			break;
		case LOG_FAC_SNTP:
			categoryStr = "NTP";
			break;
		case LOG_FAC_OSGI:
			categoryStr = "OSGi";
			break;
		case LOG_FAC_ACCOUNT:
			categoryStr = "Sec_Account";
			break;
		case LOG_FAC_SYSTEM:
			categoryStr = "System";
			break;
		case LOG_FAC_XDSL:
			categoryStr = "XDSL";
			break;
		case LOG_FAC_Internet:
			categoryStr = "Internet";
			break;
		case LOG_FAC_3GDONGEL:
			categoryStr = "3GDongel";
			break;			
		case LOG_FAC_VoIP:
			categoryStr = "VoIP";
			break;

	}

	len = snprintf(&(buf[len]), maxLen - len, "category:\"%s\" ", categoryStr);
	len += snprintf(&(buf[len]), maxLen - len, "detail:\"");

	if (len+1 < maxLen)
	{
		maxLen -= len;
		len += vsnprintf(&buf[len], maxLen, detail, ap);
	}
	snprintf(&(buf[len]), 2, "\"");

	oalLog_syslog(level, buf);
	va_end(ap);
}

void zyLog_cfg(CmsLogFacility category, const char *msg, ...)
{
	
	va_list ap;
	char buf[MAX_LOG_LINE_LENGTH] = {0};
	int len=0, maxLen;

	maxLen = sizeof(buf);
	if(cmsUtl_strncmp(curUserName,"",BUFLEN_16))
		len = snprintf(&(buf[len]), maxLen - len, "Configuration by user %s: ", curUserName);

	va_start(ap, msg);
	vsnprintf(&buf[len], maxLen - len, msg, ap);
	va_end(ap);

	zyLog_app(LOG_LEVEL_NOTICE, category, buf);
}
#endif
void log_log(CmsLogLevel level, const char *func, UINT32 lineNum, const char *pFmt, ... )
{
   va_list		ap;
   char buf[MAX_LOG_LINE_LENGTH] = {0};
   int len=0, maxLen;
   char *logLevelStr=NULL;
   const CmsEntityInfo *einfo=NULL;
   int logTelnetFd = -1;

   maxLen = sizeof(buf);
   char syslog_buf[MAX_LOG_LINE_LENGTH] = {0};
   int syslog_len = 0;
   char devnode[16] = {0};
   int num = 0;

   /*
    * Use different log buffer to save log message for
    * Standard Error, Telent and Syslog.
    * Besides, add more than one destination that log sent to 
    * at the same time.
    */
   if (level <= logLevel)
   {
      va_start(ap, pFmt);

      if (logHeaderMask & CMSLOG_HDRMASK_APPNAME)
      {
         if ((einfo = cmsEid_getEntityInfo(gEid)) != NULL)
         {
            len = snprintf(buf, maxLen, "%s:", einfo->name);
            syslog_len = snprintf(syslog_buf, maxLen, "%s:", einfo->name);
         }
         else
         {
            len = snprintf(buf, maxLen, "unknown:");
            syslog_len = snprintf(syslog_buf, maxLen, "unknown:");
         }
      }


      if ((logHeaderMask & CMSLOG_HDRMASK_LEVEL) && (len < maxLen))
      {
         /*
          * Log the severity level when going to stderr and telnet
          * because syslog already logs the severity level for us.
          */
            switch(level)
            {
            case LOG_LEVEL_ERR:
               logLevelStr = "error";
               break;
            case LOG_LEVEL_NOTICE:
               logLevelStr = "notice";
               break;
            case LOG_LEVEL_DEBUG:
               logLevelStr = "debug";
               break;
            default:
               logLevelStr = "invalid";
               break;
            }
            len += snprintf(&(buf[len]), maxLen - len, "%s:", logLevelStr);
         }


      /*
       * Log timestamp for both stderr and telnet because syslog's
       * timestamp is when the syslogd gets the log, not when it was
       * generated.
       */
      if ((logHeaderMask & CMSLOG_HDRMASK_TIMESTAMP) && (len < maxLen))
      {
         CmsTimestamp ts;

         cmsTms_get(&ts);
         len += snprintf(&(buf[len]), maxLen - len, "%u.%03u:",
                         ts.sec%1000, ts.nsec/NSECS_IN_MSEC);
      }


      if ((logHeaderMask & CMSLOG_HDRMASK_LOCATION) && (len < maxLen) && (syslog_len < maxLen))
      {
         len += snprintf(&(buf[len]), maxLen - len, "%s:%u:", func, lineNum);
         syslog_len += snprintf(&(syslog_buf[syslog_len]), maxLen - syslog_len, "%s:%u:", func, lineNum);
      }


      if (len < maxLen)
      {
         maxLen -= len;
         vsnprintf(&buf[len], maxLen, pFmt, ap);
      }

      if (syslog_len < maxLen)
      {
         maxLen -= syslog_len;
         vsnprintf(&syslog_buf[syslog_len], maxLen, pFmt, ap);	 
      }

      if (logDestination == LOG_DEST_STDERR)
      {
         fprintf(stderr, "%s\n", buf);
         fflush(stderr);
      }
      else if (logDestination == LOG_DEST_TELNET )
      {
   #ifdef DESKTOP_LINUX
         /* Fedora Desktop Linux */
         logTelnetFd = open("/dev/pts/1", O_RDWR);
   #else
         for(num = 0 ; num<16 ; num++)
         {
		sprintf(devnode,"/dev/ttyp%x",num);
         /* CPE use ptyp0 as the first pesudo terminal */
         	logTelnetFd = open(devnode, O_RDWR);
   #endif
         if(logTelnetFd != -1)
         {
            write(logTelnetFd, buf, strlen(buf));
            write(logTelnetFd, "\n", strlen("\n"));
            close(logTelnetFd);
         }
   #ifndef DESKTOP_LINUX		
         }
   #endif
      }
      else if(logDestination == LOG_DEST_SYSLOG)
      {
         oalLog_syslog(level, syslog_buf);
      }
#if 1/*Support multi dest of cms log, __FTTB8-CHT__, MitraStar Peien, 20121030*/
      else if(logDestination == LOG_DEST_STDERR_TELNET)
      {
         fprintf(stderr, "%s\n", buf);
         fflush(stderr);
		 
#ifdef DESKTOP_LINUX
         /* Fedora Desktop Linux */
         logTelnetFd = open("/dev/pts/1", O_RDWR);
#else
         for(num = 0 ; num<16 ; num++)
         {
	   	sprintf(devnode,"/dev/ttyp%x",num);

         /* CPE use ptyp0 as the first pesudo terminal */
         	logTelnetFd = open(devnode, O_RDWR);
#endif
         if(logTelnetFd != -1)
         {
            write(logTelnetFd, buf, strlen(buf));
            write(logTelnetFd, "\n", strlen("\n"));
            close(logTelnetFd);
         }
#ifndef DESKTOP_LINUX
         }
#endif
      }
      else if(logDestination == LOG_DEST_STDERR_SYSLOG)
      {
         fprintf(stderr, "%s\n", buf);
         fflush(stderr);
         oalLog_syslog(level, syslog_buf);
      }
      else if(logDestination == LOG_DEST_TELNET_SYSLOG)
      {
#ifdef DESKTOP_LINUX
         /* Fedora Desktop Linux */
         logTelnetFd = open("/dev/pts/1", O_RDWR);
#else
         for(num = 0 ; num<16 ; num++)
         {
	   	sprintf(devnode,"/dev/ttyp%x",num);
							 
         /* CPE use ptyp0 as the first pesudo terminal */
	   	logTelnetFd = open(devnode, O_RDWR);
#endif
         if(logTelnetFd != -1)
         {
            write(logTelnetFd, buf, strlen(buf));
            write(logTelnetFd, "\n", strlen("\n"));
            close(logTelnetFd);
      }
#ifndef DESKTOP_LINUX
         }
#endif

         oalLog_syslog(level, syslog_buf);
      }
      else if(logDestination == LOG_DEST_STDERR_TELNET_SYSLOG)
      {
         fprintf(stderr, "%s\n", buf);
         fflush(stderr);
				   
#ifdef DESKTOP_LINUX
         /* Fedora Desktop Linux */
         logTelnetFd = open("/dev/pts/1", O_RDWR);
#else
         for(num = 0 ; num<16 ; num++)
         {
	   	sprintf(devnode,"/dev/ttyp%x",num);
				   
         /* CPE use ptyp0 as the first pesudo terminal */
	   	logTelnetFd = open(devnode, O_RDWR);
#endif
         if(logTelnetFd != -1)
         {
            write(logTelnetFd, buf, strlen(buf));
            write(logTelnetFd, "\n", strlen("\n"));
            close(logTelnetFd);
         }
#ifndef DESKTOP_LINUX
         }
#endif

         oalLog_syslog(level, syslog_buf);
      }
#endif

      va_end(ap);
   }

}  /* End of log_log() */


void cmsLog_init(CmsEntityId eid)
{
   logLevel       = DEFAULT_LOG_LEVEL;
   logDestination = DEFAULT_LOG_DESTINATION;
   logHeaderMask  = DEFAULT_LOG_HEADER_MASK;

   gEid = eid;

   oalLog_init();

   return;

}  /* End of cmsLog_init() */

  
void cmsLog_cleanup(void)
{
   oalLog_cleanup();
   return;

}  /* End of cmsLog_cleanup() */
  

void cmsLog_setLevel(CmsLogLevel level)
{
   logLevel = level;
   return;
}


CmsLogLevel cmsLog_getLevel(void)
{
   return logLevel;
}


void cmsLog_setDestination(CmsLogDestination dest)
{
   logDestination = dest;
   return;
}


CmsLogDestination cmsLog_getDestination(void)
{
   return logDestination;
}


void cmsLog_setHeaderMask(UINT32 headerMask)
{
   logHeaderMask = headerMask;
   return;
}


UINT32 cmsLog_getHeaderMask(void)
{
   return logHeaderMask;
} 


int cmsLog_readPartial(int ptr, char* buffer)
{
   return (oal_readLogPartial(ptr, buffer));
}

#if 1 //__MSTC__, TengChang Chen, Log, migrate from Common_406
int cmsLog_readPartial_MSTC(int ptr, char* buffer, int logType)
{
   return (oal_readLogPartial_MSTC(ptr, buffer, logType));
}

int cmsLog_readFromFile(char* filename, int offset, char* buffer)
{
	return (oal_readLogFromFile(filename, offset, buffer));
}

int cmsLog_getLogData(char *line, char *data, int field)
{
   char date[4];
   char *dot = NULL, *cp = NULL;
#ifdef SUPPORT_SNTP                  
   char month[4];
   char year[5];
   static char months[12][4] =
      { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
#else
   char times[9];
   static int daysOfMonth[12] =
      { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
   int days = 0, i = 0, j = 0;
#endif

   if ( line == NULL ) return -1;
   data[0] = '\0';

   switch (field) {
      case LOG_DATE_TIME:
#ifdef SUPPORT_SNTP
		  //Output format: 2010 Jan  1 00:00:00
		  //parse year
		  strncpy(year, line, 4);
		  year[4] = '\0';
		  //parse month
		  strncpy(month, &line[5], 2);
		  month[2] = '\0';
		  strncpy(month, months[atoi(month)-1], 4);
		  //parse date
		  strncpy(date, &line[8], 2);
		  date[2] = '\0';
		  snprintf(data, 21, "%s %s %2d %s", year, month, atoi(date), &line[11]);
		  data[21] = '\0';
#else
            // format of date/time as follow: "Jan  1 00:00:00"
            // need to convert to "1st day 00:00:00"
            strncpy(date, line, 3);
            date[3] = '\0';
            for ( i = 0; i < 12; i++ ) {
                if ( cmsUtl_strcmp(months[i], date) == 0 )
                   break;
            }
            if ( i < 12 ) {
                for ( j = 0; j < i; j++ )
                   days += daysOfMonth[j];
            }
            strncpy(date, &line[4], 2);
            date[2] = '\0';
            days += atoi(date);
            strncpy(times, &line[7], 8);
            times[8] = '\0';
            switch (days) {
               case 1:
                  sprintf(data, "%dst day %s", days, times);
                  break;
               case 2:
                  sprintf(data, "%dnd day %s", days, times);
                  break;
               case 3:
                  sprintf(data, "%drd day %s", days, times);
                  break;
               default:
                  sprintf(data, "%dth day %s", days, times);
                  break;
            }
#endif
         break;
      case LOG_FACILITY:
         dot = strchr(&line[20], '.');
         if ( dot != NULL ) {
            for ( cp = (dot - 1); cp != NULL && *cp !=  ' ' ; cp-- )
               ;
            if ( ++cp != NULL ) {
               strncpy(data, cp, dot - cp);
               data[dot - cp] = '\0';
            }
         }
         break;
      case LOG_SEVERITY:
         dot = strchr(&line[20], '.');
         if ( dot != NULL ) {
            for ( cp = (dot + 1); cp != NULL && *cp !=  ' ' ; cp++ )
               ;
            if ( cp != NULL ) {
               dot++;
               strncpy(data, dot, cp - dot);
               data[cp - dot] = '\0';
            }
         }
         break;
      case LOG_MESSAGE:
		 dot = strchr(&line[20], ':');
		 if ( dot != NULL ) {
			 for ( cp = (dot + 1); cp != NULL && *cp !=  ' ' ; cp++ )
				 ;
			 if(++cp != NULL)
				 strcpy(data, cp);
		 }
		 break;
	  case LOG_SYSTEM:
		 dot = strchr(&line[20], ':');
		 if(dot == NULL)
		 {
			 strcpy(data, "System");	
		 }
		 else
		 {
			 cp = dot-1;
			 while(cp != NULL && *cp != ' ')
				 cp--;
			 strncpy(data, cp+1, dot-cp-1);
			 data[dot-cp-1] = '\0';

			 //Replace '_' to ' ' in facility string
			 while((cp = strchr(data,'_'))!=NULL)
				 *cp = ' ';
		 }
		 break; 
      default:
         data[0] = '\0';
         break;
   }
   return 0;
}

#ifndef MSTC_SAVE_LOG_TO_FLASH //__MSTC__, TengChang
void cmsLog_backupLog()
{
	FILE *fp;
	char line[2048];
	int readPtr = BCM_SYSLOG_FIRST_READ;

	if(cmsLog_readPartial_MSTC(BCM_SYSLOG_FIRST_READ, line, SYSTEM_TYPE) != BCM_SYSLOG_READ_BUFFER_ERROR)
	{
		fp = fopen(LOG_FILE,"w");
		while(1){
			readPtr = cmsLog_readPartial_MSTC(readPtr, line, SYSTEM_TYPE);
			if( readPtr == BCM_SYSLOG_READ_BUFFER_ERROR || readPtr == BCM_SYSLOG_READ_BUFFER_END)
				break;
			fprintf(fp,line);
		}
		fclose(fp);
	}

	readPtr = BCM_SYSLOG_FIRST_READ;
	if(cmsLog_readPartial_MSTC(BCM_SYSLOG_FIRST_READ, line, SECURITY_TYPE) != BCM_SYSLOG_READ_BUFFER_ERROR)
	{
		fp = fopen(SECURITY_LOG_FILE,"w");
		while(1){
			readPtr = cmsLog_readPartial_MSTC(readPtr, line, SECURITY_TYPE);
			if( readPtr == BCM_SYSLOG_READ_BUFFER_ERROR || readPtr == BCM_SYSLOG_READ_BUFFER_END)
				break;
			fprintf(fp,line);
		}
		fclose(fp);
	}
	return;
}
#endif

#endif //__MSTC__, TengChang Chen, Log, migrate from Common_406
