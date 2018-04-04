/*
 * (C) 2006-2007 by Pablo Neira Ayuso <pablo@netfilter.org>
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

#include "sync.h"
#include "netlink.h"
#include "traffic_stats.h"
#include "log.h"
#include "cache.h"
#include "conntrackd.h"
#include "network.h"
#include "fds.h"
#include "event.h"
#include "queue.h"
#include "process.h"
#include "origin.h"
#include "internal.h"
#include "external.h"

#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <net/if.h>
#include <fcntl.h>

static void
do_channel_handler_step(int i, struct nethdr *net, size_t remain)
{
	char __ct[nfct_maxsize()];
	struct nf_conntrack *ct = (struct nf_conntrack *)(void*) __ct;

	if (net->version != CONNTRACKD_PROTOCOL_VERSION) {
		STATE_SYNC(error).msg_rcv_malformed++;
		STATE_SYNC(error).msg_rcv_bad_version++;
		return;
	}

	switch (STATE_SYNC(sync)->recv(net)) {
	case MSG_DATA:
		multichannel_change_current_channel(STATE_SYNC(channel), i);
		break;
	case MSG_CTL:
		multichannel_change_current_channel(STATE_SYNC(channel), i);
		return;
	case MSG_BAD:
		STATE_SYNC(error).msg_rcv_malformed++;
		STATE_SYNC(error).msg_rcv_bad_header++;
		return;
	case MSG_DROP:
		return;
	default:
		break;
	}

	if (net->type > NET_T_STATE_MAX) {
		STATE_SYNC(error).msg_rcv_malformed++;
		STATE_SYNC(error).msg_rcv_bad_type++;
		return;
	}
	memset(ct, 0, sizeof(__ct));

	if (parse_payload(ct, net, remain) == -1) {
		STATE_SYNC(error).msg_rcv_malformed++;
		STATE_SYNC(error).msg_rcv_bad_payload++;
		return;
	}

	switch(net->type) {
	case NET_T_STATE_NEW:
		STATE_SYNC(external)->new(ct);
		break;
	case NET_T_STATE_UPD:
		STATE_SYNC(external)->update(ct);
		break;
	case NET_T_STATE_DEL:
		STATE_SYNC(external)->destroy(ct);
		break;
	default:
		STATE_SYNC(error).msg_rcv_malformed++;
		STATE_SYNC(error).msg_rcv_bad_type++;
		break;
	}
}

static char __net[65536];		/* XXX: maximum MTU for IPv4 */
static char *cur = __net;

static int channel_stream(struct channel *m, const char *ptr, ssize_t remain)
{
	if (m->channel_flags & CHANNEL_F_STREAM) {
		/* truncated data. */
		memcpy(__net, ptr, remain);
		cur = __net + remain;
		return 1;
	}
	return 0;
}

/* handler for messages received */
static int channel_handler_routine(struct channel *m, int i)
{
	ssize_t numbytes;
	ssize_t remain, pending = cur - __net;
	char *ptr = __net;

	numbytes = channel_recv(m, cur, sizeof(__net) - pending);
	if (numbytes <= 0)
		return -1;

	remain = numbytes;
	if (pending) {
		remain += pending;
		cur = __net;
	}

	while (remain > 0) {
		struct nethdr *net = (struct nethdr *) ptr;
		int len;

		if (remain < NETHDR_SIZ) {
			if (!channel_stream(m, ptr, remain)) {
				STATE_SYNC(error).msg_rcv_malformed++;
				STATE_SYNC(error).msg_rcv_truncated++;
			}
			break;
		}

		len = ntohs(net->len);
		if (len <= 0) {
			STATE_SYNC(error).msg_rcv_malformed++;
			STATE_SYNC(error).msg_rcv_bad_size++;
			break;
		}

		if (len > remain) {
			if (!channel_stream(m, ptr, remain)) {
				STATE_SYNC(error).msg_rcv_malformed++;
				STATE_SYNC(error).msg_rcv_bad_size++;
			}
			break;
		}

		if (IS_ACK(net) || IS_NACK(net) || IS_RESYNC(net)) {
			if (remain < NETHDR_ACK_SIZ) {
				if (!channel_stream(m, ptr, remain)) {
					STATE_SYNC(error).msg_rcv_malformed++;
					STATE_SYNC(error).msg_rcv_truncated++;
				}
				break;
			}

			if (len < NETHDR_ACK_SIZ) {
				STATE_SYNC(error).msg_rcv_malformed++;
				STATE_SYNC(error).msg_rcv_bad_size++;
				break;
			}
		} else {
			if (len < NETHDR_SIZ) {
				STATE_SYNC(error).msg_rcv_malformed++;
				STATE_SYNC(error).msg_rcv_bad_size++;
				break;
			}
		}

		HDR_NETWORK2HOST(net);

		do_channel_handler_step(i, net, remain);
		ptr += net->len;
		remain -= net->len;
	}
	return 0;
}

/* handler for messages received */
static void channel_handler(struct channel *m, int i)
{
	int k;

	for (k=0; k<CONFIG(event_iterations_limit); k++) {
		if (channel_handler_routine(m, i) == -1) {
			break;
		}
	}
}

/* select a new interface candidate in a round robin basis */
static void interface_candidate(void)
{
	int i, idx;
	unsigned int flags;
	char buf[IFNAMSIZ];

	for (i=0; i<STATE_SYNC(channel)->channel_num; i++) {
		idx = multichannel_get_ifindex(STATE_SYNC(channel), i);
		if (idx == multichannel_get_current_ifindex(STATE_SYNC(channel)))
			continue;
		nlif_get_ifflags(STATE_SYNC(interface), idx, &flags);
		if (flags & (IFF_RUNNING | IFF_UP)) {
			multichannel_set_current_channel(STATE_SYNC(channel), i);
			dlog(LOG_NOTICE, "device `%s' becomes "
					 "dedicated link", 
					 if_indextoname(idx, buf));
			return;
		}
	}
	dlog(LOG_ERR, "no dedicated links available!");
}

static void interface_handler(void)
{
	int idx = multichannel_get_current_ifindex(STATE_SYNC(channel));
	unsigned int flags;

	nlif_catch(STATE_SYNC(interface));
	nlif_get_ifflags(STATE_SYNC(interface), idx, &flags);
	if (!(flags & IFF_RUNNING) || !(flags & IFF_UP))
		interface_candidate();
}

static void do_reset_cache_alarm(struct alarm_block *a, void *data)
{
	STATE(stats).nl_kernel_table_flush++;
	dlog(LOG_NOTICE, "flushing kernel conntrack table (scheduled)");

	/* fork a child process that performs the flush operation,
	 * meanwhile the parent process handles events. */
	if (fork_process_new(CTD_PROC_FLUSH, CTD_PROC_F_EXCL,
			     NULL, NULL) == 0) {
		nl_flush_conntrack_table(STATE(flush));
		exit(EXIT_SUCCESS);
	}
	/* this is not required if events don't get lost */
	STATE(mode)->internal->flush();
}

static int init_sync(void)
{
	int i;

	state.sync = malloc(sizeof(struct ct_sync_state));
	if (!state.sync) {
		dlog(LOG_ERR, "can't allocate memory for sync");
		return -1;
	}
	memset(state.sync, 0, sizeof(struct ct_sync_state));

	if (CONFIG(flags) & CTD_SYNC_FTFW)
		STATE_SYNC(sync) = &sync_ftfw;
	else if (CONFIG(flags) & CTD_SYNC_ALARM)
		STATE_SYNC(sync) = &sync_alarm;
	else if (CONFIG(flags) & CTD_SYNC_NOTRACK)
		STATE_SYNC(sync) = &sync_notrack;
	else {
		fprintf(stderr, "WARNING: No synchronization mode specified. "
				"Defaulting to FT-FW mode.\n");
		CONFIG(flags) |= CTD_SYNC_FTFW;
		STATE_SYNC(sync) = &sync_ftfw;
	}

	if (STATE_SYNC(sync)->init)
		STATE_SYNC(sync)->init();

	if (CONFIG(sync).internal_cache_disable == 0) {
		STATE(mode)->internal = &internal_cache;
	} else {
		STATE(mode)->internal = &internal_bypass;
		dlog(LOG_NOTICE, "disabling internal cache");

	}
	if (STATE(mode)->internal->init() == -1)
		return -1;

	if (CONFIG(sync).external_cache_disable == 0) {
		STATE_SYNC(external) = &external_cache;
	} else {
		STATE_SYNC(external) = &external_inject;
		dlog(LOG_NOTICE, "disabling external cache");
	}
	if (STATE_SYNC(external)->init() == -1)
		return -1;

	if (channel_init() == -1)
		return -1;

	/* channel to send events on the wire */
	STATE_SYNC(channel) =
		multichannel_open(CONFIG(channel), CONFIG(channel_num));
	if (STATE_SYNC(channel) == NULL) {
		dlog(LOG_ERR, "can't open channel socket");
		return -1;
	}
	for (i=0; i<STATE_SYNC(channel)->channel_num; i++) {
		int fd = channel_get_fd(STATE_SYNC(channel)->channel[i]);
		fcntl(fd, F_SETFL, O_NONBLOCK);
		if (register_fd(fd, STATE(fds)) == -1)
			return -1;
	}

	STATE_SYNC(interface) = nl_init_interface_handler();
	if (!STATE_SYNC(interface)) {
		dlog(LOG_ERR, "can't open interface watcher");
		return -1;
	}
	if (register_fd(nlif_fd(STATE_SYNC(interface)), STATE(fds)) == -1)
		return -1;

	STATE_SYNC(tx_queue) = queue_create("txqueue", INT_MAX, QUEUE_F_EVFD);
	if (STATE_SYNC(tx_queue) == NULL) {
		dlog(LOG_ERR, "cannot create tx queue");
		return -1;
	}
	if (register_fd(queue_get_eventfd(STATE_SYNC(tx_queue)), 
							STATE(fds)) == -1)
		return -1;

	STATE_SYNC(commit).h = nfct_open(CONNTRACK, 0);
	if (STATE_SYNC(commit).h == NULL) {
		dlog(LOG_ERR, "can't create handler to commit");
		return -1;
	}
	origin_register(STATE_SYNC(commit).h, CTD_ORIGIN_COMMIT);

	STATE_SYNC(commit).evfd = create_evfd();
	if (STATE_SYNC(commit).evfd == NULL) {
		dlog(LOG_ERR, "can't create eventfd to commit");
		return -1;
	}
	if (register_fd(get_read_evfd(STATE_SYNC(commit).evfd),
							STATE(fds)) == -1) {
		return -1;
	}

	init_alarm(&STATE_SYNC(reset_cache_alarm), NULL, do_reset_cache_alarm);

	/* initialization of message sequence generation */
	STATE_SYNC(last_seq_sent) = time(NULL);

	return 0;
}

static void channel_check(struct channel *c, int i, fd_set *readfds)
{
	/* In case that this channel is connection-oriented. */
	if (channel_accept_isset(c, readfds))
		channel_accept(c);

	/* For data handling. */
	if (channel_isset(c, readfds))
		channel_handler(c, i);
}

static void run_sync(fd_set *readfds)
{
	int i;

	for (i=0; i<STATE_SYNC(channel)->channel_num; i++)
		channel_check(STATE_SYNC(channel)->channel[i], i, readfds);

	if (FD_ISSET(queue_get_eventfd(STATE_SYNC(tx_queue)), readfds))
		STATE_SYNC(sync)->xmit();

	if (FD_ISSET(nlif_fd(STATE_SYNC(interface)), readfds))
		interface_handler();

	if (FD_ISSET(get_read_evfd(STATE_SYNC(commit).evfd), readfds)) {
		read_evfd(STATE_SYNC(commit).evfd);
		STATE_SYNC(external)->commit(STATE_SYNC(commit).h, 0);
	}

	/* flush pending messages */
	multichannel_send_flush(STATE_SYNC(channel));
}

static void kill_sync(void)
{
	STATE(mode)->internal->close();
	STATE_SYNC(external)->close();

	multichannel_close(STATE_SYNC(channel));

	nlif_close(STATE_SYNC(interface));

	queue_destroy(STATE_SYNC(tx_queue));

	channel_end();

	origin_unregister(STATE_SYNC(commit).h);
	nfct_close(STATE_SYNC(commit).h);
	destroy_evfd(STATE_SYNC(commit).evfd);

	if (STATE_SYNC(sync)->kill)
		STATE_SYNC(sync)->kill();
}

static void dump_stats_sync(int fd)
{
	char buf[512];
	int size;

	size = sprintf(buf, "message sequence tracking:\n"
			    "%20llu Msgs mfrm "
			    "%20llu Msgs lost\n\n",
			(unsigned long long)STATE_SYNC(error).msg_rcv_malformed,
			(unsigned long long)STATE_SYNC(error).msg_rcv_lost);

	send(fd, buf, size, 0);
}

static void dump_stats_sync_extended(int fd)
{
	char buf[512];
	int size;

	size = snprintf(buf, sizeof(buf),
			"network statistics:\n"
			"\trecv:\n"
			"\t\tMalformed messages:\t%20llu\n"
			"\t\tWrong protocol version:\t%20u\n"
			"\t\tMalformed header:\t%20u\n"
			"\t\tMalformed payload:\t%20u\n"
			"\t\tBad message type:\t%20u\n"
			"\t\tTruncated message:\t%20u\n"
			"\t\tBad message size:\t%20u\n"
			"\tsend:\n"
			"\t\tMalformed messages:\t%20u\n\n"
			"sequence tracking statistics:\n"
			"\trecv:\n"
			"\t\tPackets lost:\t\t%20llu\n"
			"\t\tPackets before:\t\t%20llu\n\n",
			(unsigned long long)STATE_SYNC(error).msg_rcv_malformed,
			STATE_SYNC(error).msg_rcv_bad_version,
			STATE_SYNC(error).msg_rcv_bad_header,
			STATE_SYNC(error).msg_rcv_bad_payload,
			STATE_SYNC(error).msg_rcv_bad_type,
			STATE_SYNC(error).msg_rcv_truncated,
			STATE_SYNC(error).msg_rcv_bad_size,
			STATE_SYNC(error).msg_snd_malformed,
			(unsigned long long)STATE_SYNC(error).msg_rcv_lost,
			(unsigned long long)STATE_SYNC(error).msg_rcv_before);

	send(fd, buf, size, 0);
}

/* handler for requests coming via UNIX socket */
static int local_handler_sync(int fd, int type, void *data)
{
	int ret = LOCAL_RET_OK;

	switch(type) {
	case DUMP_INTERNAL:
		ret = fork_process_new(CTD_PROC_ANY, 0, NULL, NULL);
		if (ret == 0) {
			STATE(mode)->internal->dump(fd, NFCT_O_PLAIN);
			exit(EXIT_SUCCESS);
		}
		break;
	case DUMP_EXTERNAL:
		ret = fork_process_new(CTD_PROC_ANY, 0, NULL, NULL);
		if (ret == 0) {
			STATE_SYNC(external)->dump(fd, NFCT_O_PLAIN);
			exit(EXIT_SUCCESS);
		} 
		break;
	case DUMP_INT_XML:
		ret = fork_process_new(CTD_PROC_ANY, 0, NULL, NULL);
		if (ret == 0) {
			STATE(mode)->internal->dump(fd, NFCT_O_XML);
			exit(EXIT_SUCCESS);
		}
		break;
	case DUMP_EXT_XML:
		ret = fork_process_new(CTD_PROC_ANY, 0, NULL, NULL);
		if (ret == 0) {
			STATE_SYNC(external)->dump(fd, NFCT_O_XML);
			exit(EXIT_SUCCESS);
		}
		break;
	case COMMIT:
		/* delete the reset alarm if any before committing */
		del_alarm(&STATE_SYNC(reset_cache_alarm));

		dlog(LOG_NOTICE, "committing external cache");
		STATE_SYNC(external)->commit(STATE_SYNC(commit).h, fd);
		/* Keep the client socket open, we want synchronous commits. */
		ret = LOCAL_RET_STOLEN;
		break;
	case RESET_TIMERS:
		if (!alarm_pending(&STATE_SYNC(reset_cache_alarm))) {
			dlog(LOG_NOTICE, "flushing conntrack table in %d secs",
					 CONFIG(purge_timeout));
			add_alarm(&STATE_SYNC(reset_cache_alarm),
				  CONFIG(purge_timeout), 0);
		}
		break;
	case FLUSH_CACHE:
		/* inmediate flush, remove pending flush scheduled if any */
		del_alarm(&STATE_SYNC(reset_cache_alarm));
		dlog(LOG_NOTICE, "flushing caches");
		STATE(mode)->internal->flush();
		STATE_SYNC(external)->flush();
		break;
	case FLUSH_INT_CACHE:
		/* inmediate flush, remove pending flush scheduled if any */
		del_alarm(&STATE_SYNC(reset_cache_alarm));
		dlog(LOG_NOTICE, "flushing internal cache");
		STATE(mode)->internal->flush();
		break;
	case FLUSH_EXT_CACHE:
		dlog(LOG_NOTICE, "flushing external cache");
		STATE_SYNC(external)->flush();
		break;
	case KILL:
		killer(0);
		break;
	case STATS:
		STATE(mode)->internal->stats(fd);
		STATE_SYNC(external)->stats(fd);
		dump_traffic_stats(fd);
		multichannel_stats(STATE_SYNC(channel), fd);
		dump_stats_sync(fd);
		break;
	case STATS_NETWORK:
		dump_stats_sync_extended(fd);
		multichannel_stats(STATE_SYNC(channel), fd);
		break;
	case STATS_CACHE:
		STATE(mode)->internal->stats_ext(fd);
		STATE_SYNC(external)->stats_ext(fd);
		break;
	case STATS_LINK:
		multichannel_stats_extended(STATE_SYNC(channel),
					    STATE_SYNC(interface), fd);
		break;
	case STATS_QUEUE:
		queue_stats_show(fd);
		break;
	default:
		if (STATE_SYNC(sync)->local)
			ret = STATE_SYNC(sync)->local(fd, type, data);
		break;
	}

	return ret;
}

struct ct_mode sync_mode = {
	.init 			= init_sync,
	.run			= run_sync,
	.local			= local_handler_sync,
	.kill			= kill_sync,
	/* the internal handler is set in run-time. */
};
