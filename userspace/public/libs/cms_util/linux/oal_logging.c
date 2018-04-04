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


#include "../oal.h"
#ifdef MSTC_LOG //__MSTC__, TengChang Chen
#include <errno.h>
#endif //__MSTC__, TengChang Chen

/** OS dependent logging functions go in this file.
 */
void oalLog_init(void)
{
   openlog(NULL, 0, LOG_DAEMON);
   return;
}

void oalLog_syslog(CmsLogLevel level, const char *buf)
{
#ifdef MSTC_LOG //__MSTC__, TengChang Chen
   errno = 0;
#endif //__MSTC__, TengChang Chen
   syslog(level, buf);
#ifdef MSTC_LOG //__MSTC__, TengChang Chen
   /*If syslogd restart, the log socket need to be re-connected*/
   if(errno == ECONNREFUSED){
      closelog();
      openlog(NULL, LOG_CONS, LOG_DAEMON);
      syslog(level, buf);
   }
#endif //__MSTC__, TengChang Chen
   return;
}

void oalLog_cleanup(void)
{
   closelog();
   return;
}
