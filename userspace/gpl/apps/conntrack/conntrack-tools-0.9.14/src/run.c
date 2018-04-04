/*
 * (C) 2006-2009 by Pablo Neira Ayuso <pablo@netfilter.org>
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
 * Description: run and init functions
 */

#include "conntrackd.h"
#include "netlink.h"
#include "filter.h"
#include "log.h"
#include "alarm.h"
#include "fds.h"
#include "traffic_stats.h"
#include "process.h"
#include "origin.h"
#include "date.h"
#include "internal.h"

#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>

void killer(int foo)
{
	/* no signals while handling signals */
	sigprocmask(SIG_BLOCK, &STATE(block), NULL);

	if (!(CONFIG(flags) & CTD_POLL))
		nfct_close(STATE(event));

	nfct_close(STATE(resync));
	nfct_close(STATE(get));
	origin_unregister(STATE(flush));
	nfct_close(STATE(flush));

	if (STATE(us_filter))
		ct_filter_destroy(STATE(us_filter));
	local_server_destroy(&STATE(local));
	STATE(mode)->kill();

	if (STATE(mode)->internal->flags & INTERNAL_F_POPULATE) {
		nfct_close(STATE(dump));
	}
	destroy_fds(STATE(fds)); 

	unlink(CONFIG(lockfile));
	dlog(LOG_NOTICE, "---- shutdown received ----");
	close_log();

	sigprocmask(SIG_UNBLOCK, &STATE(block), NULL);

	exit(0);
}

static void child(int foo)
{
	int status, ret;

	while ((ret = waitpid(0, &status, WNOHANG)) != 0) {
		if (ret == -1) {
			if (errno == EINTR)
				continue;
			if (errno == ECHILD)
				break;
			STATE(stats).wait_failed++;
			break;
		}
		/* delete process from list and run the callback */
		fork_process_delete(ret);

		if (!WIFSIGNALED(status))
			continue;

		switch(WTERMSIG(status)) {
		case SIGSEGV:
			dlog(LOG_ERR, "child process (pid=%u) has aborted, "
				      "received signal SIGSEGV (crashed)", ret);
			STATE(stats).child_process_failed++;
			STATE(stats).child_process_error_segfault++;
			break;
		case SIGINT:
		case SIGTERM:
		case SIGKILL:
			dlog(LOG_ERR, "child process (pid=%u) has aborted, "
				      "received termination signal (%u)",
				      ret, WTERMSIG(status));
			STATE(stats).child_process_failed++;
			STATE(stats).child_process_error_term++;
			break;
		default:
			dlog(LOG_NOTICE, "child process (pid=%u) "
					 "received signal (%u)", 
					 ret, WTERMSIG(status));
			STATE(stats).child_process_failed++;
			break;
		}
	}
}

static void uptime(char *buf, size_t bufsiz)
{
	time_t tmp;
	int updays, upminutes, uphours;
	size_t size = 0;

	time(&tmp);
	tmp = tmp - STATE(stats).daemon_start_time;
	updays = (int) tmp / (60*60*24);
	if (updays) {
		size = snprintf(buf, bufsiz, "%d day%s ",
				updays, (updays != 1) ? "s" : "");
	}
	upminutes = (int) tmp / 60;
	uphours = (upminutes / 60) % 24;
	upminutes %= 60;
	if(uphours) {
		snprintf(buf + size, bufsiz, "%d h %d min", uphours, upminutes);
	} else {
		snprintf(buf + size, bufsiz, "%d min", upminutes);
	}
}

static void dump_stats_runtime(int fd)
{
	char buf[1024], uptime_string[512];
	int size;

	uptime(uptime_string, sizeof(uptime_string));
	size = snprintf(buf, sizeof(buf),
			"daemon uptime: %s\n\n"
			"netlink stats:\n"
			"\tevents received:\t%20llu\n"
			"\tevents filtered:\t%20llu\n"
			"\tevents unknown type:\t\t%12u\n"
			"\tcatch event failed:\t\t%12u\n"
			"\tdump unknown type:\t\t%12u\n"
			"\tnetlink overrun:\t\t%12u\n"
			"\tflush kernel table:\t\t%12u\n"
			"\tresync with kernel table:\t%12u\n"
			"\tcurrent buffer size (in bytes):\t%12u\n\n"
			"runtime stats:\n"
			"\tchild process failed:\t\t%12u\n"
			"\t\tchild process segfault:\t%12u\n"
			"\t\tchild process termsig:\t%12u\n"
			"\tselect failed:\t\t\t%12u\n"
			"\twait failed:\t\t\t%12u\n"
			"\tlocal read failed:\t\t%12u\n"
			"\tlocal unknown request:\t\t%12u\n\n",
			uptime_string,
			(unsigned long long)STATE(stats).nl_events_received,
			(unsigned long long)STATE(stats).nl_events_filtered,
			STATE(stats).nl_events_unknown_type,
			STATE(stats).nl_catch_event_failed,
			STATE(stats).nl_dump_unknown_type,
			STATE(stats).nl_overrun,
			STATE(stats).nl_kernel_table_flush,
			STATE(stats).nl_kernel_table_resync,
			CONFIG(netlink_buffer_size),
			STATE(stats).child_process_failed,
			STATE(stats).child_process_error_segfault,
			STATE(stats).child_process_error_term,
			STATE(stats).select_failed,
			STATE(stats).wait_failed,
			STATE(stats).local_read_failed,
			STATE(stats).local_unknown_request);

	send(fd, buf, size, 0);
}

static int local_handler(int fd, void *data)
{
	int ret = LOCAL_RET_OK;
	int type;

	ret = read(fd, &type, sizeof(type));
	if (ret == -1) {
		STATE(stats).local_read_failed++;
		return LOCAL_RET_OK;
	}
	if (ret == 0)
		return LOCAL_RET_OK;

	switch(type) {
	case FLUSH_MASTER:
		STATE(stats).nl_kernel_table_flush++;
		dlog(LOG_NOTICE, "flushing kernel conntrack table");

		/* fork a child process that performs the flush operation,
		 * meanwhile the parent process handles events. */
		if (fork_process_new(CTD_PROC_FLUSH, CTD_PROC_F_EXCL,
				     NULL, NULL) == 0) {
			nl_flush_conntrack_table(STATE(flush));
			exit(EXIT_SUCCESS);
		}
		break;
	case RESYNC_MASTER:
		if (STATE(mode)->internal->flags & INTERNAL_F_POPULATE) {
			STATE(stats).nl_kernel_table_resync++;
			dlog(LOG_NOTICE, "resync with master table");
			nl_dump_conntrack_table(STATE(dump));
		} else {
			dlog(LOG_NOTICE, "resync is unsupported in this mode");
		}
		break;
	case STATS_RUNTIME:
		dump_stats_runtime(fd);
		break;
	case STATS_PROCESS:
		fork_process_dump(fd);
		break;
	}

	ret = STATE(mode)->local(fd, type, data);
	if (ret == LOCAL_RET_ERROR) {
		STATE(stats).local_unknown_request++;
		return LOCAL_RET_ERROR;
	}
	return ret;
}

static void do_overrun_resync_alarm(struct alarm_block *a, void *data)
{
	nl_send_resync(STATE(resync));
	STATE(stats).nl_kernel_table_resync++;
}

static void do_polling_alarm(struct alarm_block *a, void *data)
{
	if (STATE(mode)->internal->purge)
		STATE(mode)->internal->purge();

	nl_send_resync(STATE(resync));
	add_alarm(&STATE(polling_alarm), CONFIG(poll_kernel_secs), 0);
}

static int event_handler(const struct nlmsghdr *nlh,
			 enum nf_conntrack_msg_type type,
			 struct nf_conntrack *ct,
			 void *data)
{
	int origin_type;

	STATE(stats).nl_events_received++;

	/* skip user-space filtering if already do it in the kernel */
	if (ct_filter_conntrack(ct, !CONFIG(filter_from_kernelspace))) {
		STATE(stats).nl_events_filtered++;
		goto out;
	}

	origin_type = origin_find(nlh);

	switch(type) {
	case NFCT_T_NEW:
		STATE(mode)->internal->new(ct, origin_type);
		break;
	case NFCT_T_UPDATE:
		STATE(mode)->internal->update(ct, origin_type);
		break;
	case NFCT_T_DESTROY:
		if (STATE(mode)->internal->destroy(ct, origin_type))
			update_traffic_stats(ct);
		break;
	default:
		STATE(stats).nl_events_unknown_type++;
		break;
	}

out:
	if (STATE(event_iterations_limit)-- <= 0) {
		STATE(event_iterations_limit) = CONFIG(event_iterations_limit);
		return NFCT_CB_STOP;
	} else
		return NFCT_CB_CONTINUE;
}

static int dump_handler(enum nf_conntrack_msg_type type,
			struct nf_conntrack *ct,
			void *data)
{
	if (ct_filter_conntrack(ct, 1))
		return NFCT_CB_CONTINUE;

	switch(type) {
	case NFCT_T_UPDATE:
		STATE(mode)->internal->populate(ct);
		break;
	default:
		STATE(stats).nl_dump_unknown_type++;
		break;
	}
	return NFCT_CB_CONTINUE;
}

static int get_handler(enum nf_conntrack_msg_type type,
		       struct nf_conntrack *ct,
		       void *data)
{
	if (ct_filter_conntrack(ct, 1))
		return NFCT_CB_CONTINUE;

	STATE(get_retval) = 1;
	return NFCT_CB_CONTINUE;
}

int
init(void)
{
	if (CONFIG(flags) & CTD_STATS_MODE)
		STATE(mode) = &stats_mode;
	else if (CONFIG(flags) & CTD_SYNC_MODE)
		STATE(mode) = &sync_mode;
	else {
		fprintf(stderr, "WARNING: No running mode specified. "
				"Defaulting to statistics mode.\n");
		CONFIG(flags) |= CTD_STATS_MODE;
		STATE(mode) = &stats_mode;
	}

	STATE(fds) = create_fds();
	if (STATE(fds) == NULL) {
		dlog(LOG_ERR, "can't create file descriptor pool");
		return -1;
	}

	/* Initialization */
	if (STATE(mode)->init() == -1) {
		dlog(LOG_ERR, "initialization failed");
		return -1;
	}

	/* local UNIX socket */
	if (local_server_create(&STATE(local), &CONFIG(local)) == -1) {
		dlog(LOG_ERR, "can't open unix socket!");
		return -1;
	}
	register_fd(STATE(local).fd, STATE(fds));

	if (!(CONFIG(flags) & CTD_POLL)) {
		STATE(event) = nl_init_event_handler();
		if (STATE(event) == NULL) {
			dlog(LOG_ERR, "can't open netlink handler: %s",
			     strerror(errno));
			dlog(LOG_ERR, "no ctnetlink kernel support?");
			return -1;
		}
		nfct_callback_register2(STATE(event), NFCT_T_ALL,
				        event_handler, NULL);
		register_fd(nfct_fd(STATE(event)), STATE(fds));
	}

	/* resynchronize (like 'dump' socket) but it also purges old entries */
	STATE(resync) = nfct_open(CONNTRACK, 0);
	if (STATE(resync)== NULL) {
		dlog(LOG_ERR, "can't open netlink handler: %s",
		     strerror(errno));
		dlog(LOG_ERR, "no ctnetlink kernel support?");
		return -1;
	}
	nfct_callback_register(STATE(resync),
			       NFCT_T_ALL,
			       STATE(mode)->internal->resync,
			       NULL);
	register_fd(nfct_fd(STATE(resync)), STATE(fds));
	fcntl(nfct_fd(STATE(resync)), F_SETFL, O_NONBLOCK);

	if (STATE(mode)->internal->flags & INTERNAL_F_POPULATE) {
		STATE(dump) = nfct_open(CONNTRACK, 0);
		if (STATE(dump) == NULL) {
			dlog(LOG_ERR, "can't open netlink handler: %s",
			     strerror(errno));
			dlog(LOG_ERR, "no ctnetlink kernel support?");
			return -1;
		}
		nfct_callback_register(STATE(dump), NFCT_T_ALL,
				       dump_handler, NULL);

		if (nl_dump_conntrack_table(STATE(dump)) == -1) {
			dlog(LOG_ERR, "can't get kernel conntrack table");
			return -1;
		}
	}

	STATE(get) = nfct_open(CONNTRACK, 0);
	if (STATE(get) == NULL) {
		dlog(LOG_ERR, "can't open netlink handler: %s",
		     strerror(errno));
		dlog(LOG_ERR, "no ctnetlink kernel support?");
		return -1;
	}
	nfct_callback_register(STATE(get), NFCT_T_ALL, get_handler, NULL);

	STATE(flush) = nfct_open(CONNTRACK, 0);
	if (STATE(flush) == NULL) {
		dlog(LOG_ERR, "cannot open flusher handler");
		return -1;
	}
	/* register this handler as the origin of a flush operation */
	origin_register(STATE(flush), CTD_ORIGIN_FLUSH);

	if (CONFIG(flags) & CTD_POLL) {
		init_alarm(&STATE(polling_alarm), NULL, do_polling_alarm);
		add_alarm(&STATE(polling_alarm), CONFIG(poll_kernel_secs), 0);
		dlog(LOG_NOTICE, "running in polling mode");
	} else {
		init_alarm(&STATE(resync_alarm), NULL, do_overrun_resync_alarm);
	}

	/* Signals handling */
	sigemptyset(&STATE(block));
	sigaddset(&STATE(block), SIGTERM);
	sigaddset(&STATE(block), SIGINT);
	sigaddset(&STATE(block), SIGCHLD);

	if (signal(SIGINT, killer) == SIG_ERR)
		return -1;

	if (signal(SIGTERM, killer) == SIG_ERR)
		return -1;

	/* ignore connection reset by peer */
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
		return -1;

	if (signal(SIGCHLD, child) == SIG_ERR)
		return -1;

	time(&STATE(stats).daemon_start_time);

	dlog(LOG_NOTICE, "initialization completed");

	return 0;
}

static void __run(struct timeval *next_alarm)
{
	int ret;
	fd_set readfds = STATE(fds)->readfds;

	ret = select(STATE(fds)->maxfd + 1, &readfds, NULL, NULL, next_alarm);
	if (ret == -1) {
		/* interrupted syscall, retry */
		if (errno == EINTR)
			return;

		STATE(stats).select_failed++;
		return;
	}

	/* signals are racy */
	sigprocmask(SIG_BLOCK, &STATE(block), NULL);

	/* order received via UNIX socket */
	if (FD_ISSET(STATE(local).fd, &readfds))
		do_local_server_step(&STATE(local), NULL, local_handler);

	if (!(CONFIG(flags) & CTD_POLL)) {
		/* conntrack event has happened */
		if (FD_ISSET(nfct_fd(STATE(event)), &readfds)) {
			ret = nfct_catch(STATE(event));
			/* reset event iteration limit counter */
			STATE(event_iterations_limit) =
					CONFIG(event_iterations_limit);
			if (ret == -1) {
			switch(errno) {
			case ENOBUFS:
				/* We have hit ENOBUFS, it's likely that we are
				 * losing events. Two possible situations may
				 * trigger this error:
				 *
				 * 1) The netlink receiver buffer is too small:
				 *    increasing the netlink buffer size should
				 *    be enough. However, some event messages
				 *    got lost. We have to resync ourselves
				 *    with the kernel table conntrack table to
				 *    resolve the inconsistency. 
				 *
				 * 2) The receiver is too slow to process the
				 *    netlink messages so that the queue gets
				 *    full quickly. This generally happens
				 *    if the system is under heavy workload
				 *    (busy CPU). In this case, increasing the
				 *    size of the netlink receiver buffer
				 *    would not help anymore since we would
				 *    be delaying the overrun. Moreover, we
				 *    should avoid resynchronizations. We 
				 *    should do our best here and keep
				 *    replicating as much states as possible.
				 *    If workload lowers at some point,
				 *    we resync ourselves.
				 */
				nl_resize_socket_buffer(STATE(event));
				if (CONFIG(nl_overrun_resync) > 0 &&
				    STATE(mode)->internal->flags &
				    			INTERNAL_F_RESYNC) {
					add_alarm(&STATE(resync_alarm),
						  CONFIG(nl_overrun_resync),0);
				}
				STATE(stats).nl_catch_event_failed++;
				STATE(stats).nl_overrun++;
				break;
			case ENOENT:
				/*
				 * We received a message from another
				 * netfilter subsystem that we are not
				 * interested in. Just ignore it.
				 */
				break;
			case EAGAIN:
				break;
			default:
				STATE(stats).nl_catch_event_failed++;
				break;
			}
			}
		}
		if (FD_ISSET(nfct_fd(STATE(resync)), &readfds)) {
			nfct_catch(STATE(resync));
			if (STATE(mode)->internal->purge)
				STATE(mode)->internal->purge();
		}
	} else {
		/* using polling mode */
		if (FD_ISSET(nfct_fd(STATE(resync)), &readfds)) {
			nfct_catch(STATE(resync));
		}
	}

	if (STATE(mode)->run)
		STATE(mode)->run(&readfds);

	sigprocmask(SIG_UNBLOCK, &STATE(block), NULL);
}

void __attribute__((noreturn))
run(void)
{
	struct timeval next_alarm; 
	struct timeval *next = NULL;

	while(1) {
		do_gettimeofday();

		sigprocmask(SIG_BLOCK, &STATE(block), NULL);
		if (next != NULL && !timerisset(next))
			next = do_alarm_run(&next_alarm);
		else
			next = get_next_alarm_run(&next_alarm);
		sigprocmask(SIG_UNBLOCK, &STATE(block), NULL);

		__run(next);
	}
}
