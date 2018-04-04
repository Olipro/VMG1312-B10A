/*
 * (C) 2006 by Pablo Neira Ayuso <pablo@netfilter.org>
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
 *
 * Description: Logging support for the conntrack daemon
 */

#include "log.h"
#include "conntrackd.h"

#include <time.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

int init_log(void)
{
	if (CONFIG(logfile)[0]) {
		STATE(log) = fopen(CONFIG(logfile), "a+");
		if (STATE(log) == NULL) {
			fprintf(stderr, "ERROR: can't open logfile `%s'."
				"Reason: %s\n", CONFIG(logfile), 
						strerror(errno));
			return -1;
		}

		setlinebuf(STATE(log));
	}

	if (CONFIG(stats).logfile[0]) {
		STATE(stats_log) = fopen(CONFIG(stats).logfile, "a+");
		if (STATE(stats_log) == NULL) {
			fprintf(stderr, "ERROR: can't open logfile `%s'."
				"Reason: %s\n", CONFIG(stats).logfile, 
						strerror(errno));
			return -1;
		}

		setlinebuf(STATE(stats_log));
	}

	if (CONFIG(syslog_facility) != -1 || 
	    CONFIG(stats).syslog_facility != -1)
		openlog(PACKAGE, LOG_PID, CONFIG(syslog_facility));

	return 0;
}

void dlog(int priority, const char *format, ...)
 {
	FILE *fd = STATE(log);
	time_t t;
	char *buf;
	const char *prio;
 	va_list args;
 
	if (fd) {
		t = time(NULL);
		buf = ctime(&t);
		buf[strlen(buf)-1]='\0';
		switch (priority) {
		case LOG_INFO:
			prio = "info";
			break;
		case LOG_NOTICE:
			prio = "notice";
			break;
		case LOG_WARNING:
			prio = "warning";
			break;
		case LOG_ERR:
			prio = "ERROR";
			break;
		default:
			prio = "?";
			break;
		}
		va_start(args, format);
		fprintf(fd, "[%s] (pid=%d) [%s] ", buf, getpid(), prio);
		vfprintf(fd, format, args);
		va_end(args);
		fprintf(fd, "\n");
		fflush(fd);
	}

	if (CONFIG(syslog_facility) != -1) {
		va_start(args, format);
		vsyslog(priority, format, args);
		va_end(args);
	}
}

void dlog_ct(FILE *fd, struct nf_conntrack *ct, unsigned int type)
{
	time_t t;
	char buf[1024];
	char *tmp;
	unsigned int flags = 0;

	buf[0]='\0';

	switch(type) {
	case NFCT_O_PLAIN:
		t = time(NULL);
		ctime_r(&t, buf);
		tmp = buf + strlen(buf);
		buf[strlen(buf)-1]='\t';
		break;
	case NFCT_O_XML:
		tmp = buf;
		flags |= NFCT_OF_TIME;
		break;
	default:
		return;
	}
	nfct_snprintf(buf+strlen(buf), 1024-strlen(buf), ct, 0, type, flags);

	if (fd) {
		snprintf(buf+strlen(buf), 1024-strlen(buf), "\n");
		fputs(buf, fd);
	}

	if (fd == STATE(log)) {
		/* error reporting */
		if (CONFIG(syslog_facility) != -1)
			syslog(LOG_ERR, "%s", tmp);
	} else if (fd == STATE(stats_log)) {
		/* connection logging */
		if (CONFIG(stats).syslog_facility != -1)
			syslog(LOG_INFO, "%s", tmp);
	}
}

void close_log(void)
{
	if (STATE(log) != NULL)
		fclose(STATE(log));

	if (STATE(stats_log) != NULL)
		fclose(STATE(stats_log));

	if (CONFIG(syslog_facility) != -1 || 
	    CONFIG(stats).syslog_facility != -1)
		closelog();
}
