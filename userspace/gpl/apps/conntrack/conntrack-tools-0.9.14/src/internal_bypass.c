/*
 * (C) 2009 by Pablo Neira Ayuso <pablo@netfilter.org>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This feature has been sponsored by 6WIND <www.6wind.com>.
 */
#include "conntrackd.h"
#include "sync.h"
#include "log.h"
#include "cache.h"
#include "netlink.h"
#include "network.h"
#include "origin.h"

static int _init(void)
{
	return 0;
}

static void _close(void)
{
}

static int dump_cb(enum nf_conntrack_msg_type type,
		   struct nf_conntrack *ct, void *data)
{
	char buf[1024];
	int size, *fd = data;

	size = nfct_snprintf(buf, 1024, ct, NFCT_T_UNKNOWN, NFCT_O_DEFAULT, 0);
	if (size < 1024) {
		buf[size] = '\n';
		size++;
	}
	send(*fd, buf, size, 0);

	return NFCT_CB_CONTINUE;
}

static void dump(int fd, int type)
{
	struct nfct_handle *h;
	u_int32_t family = AF_UNSPEC;
	int ret;

	h = nfct_open(CONNTRACK, 0);
	if (h == NULL) {
		dlog(LOG_ERR, "can't allocate memory for the internal cache");
		return;
	}
	nfct_callback_register(h, NFCT_T_ALL, dump_cb, &fd);
	ret = nfct_query(h, NFCT_Q_DUMP, &family);
	if (ret == -1) {
		dlog(LOG_ERR, "can't dump kernel table");
	}
	nfct_close(h);
}

static void flush(void)
{
	nl_flush_conntrack_table(STATE(flush));
}

struct {
	uint32_t	new;
	uint32_t	upd;
	uint32_t	del;
} internal_bypass_stats;

static void stats(int fd)
{
	char buf[512];
	int size;

	size = sprintf(buf, "internal bypass:\n"
			    "connections new:\t\t%12u\n"
			    "connections updated:\t\t%12u\n"
			    "connections destroyed:\t\t%12u\n\n",
			    internal_bypass_stats.new,
			    internal_bypass_stats.upd,
			    internal_bypass_stats.del);

	send(fd, buf, size, 0);
}

/* unused, INTERNAL_F_POPULATE is unset. No cache, nothing to populate. */
static void populate(struct nf_conntrack *ct)
{
}

/* unused, INTERNAL_F_RESYNC is unset. */
static void purge(void)
{
}

/* unused, INTERNAL_F_RESYNC is unset. Nothing to resync, we have no cache. */
static int resync(enum nf_conntrack_msg_type type,
		  struct nf_conntrack *ct,
		  void *data)
{
	return NFCT_CB_CONTINUE;
}

static void
event_new_sync(struct nf_conntrack *ct, int origin)
{
	struct nethdr *net;

	/* this event has been triggered by me, skip */
	if (origin != CTD_ORIGIN_NOT_ME)
		return;

	net = BUILD_NETMSG(ct, NET_T_STATE_NEW);
	multichannel_send(STATE_SYNC(channel), net);
	internal_bypass_stats.new++;
}

static void
event_update_sync(struct nf_conntrack *ct, int origin)
{
	struct nethdr *net;

	/* this event has been triggered by me, skip */
	if (origin != CTD_ORIGIN_NOT_ME)
		return;

	net = BUILD_NETMSG(ct, NET_T_STATE_UPD);
	multichannel_send(STATE_SYNC(channel), net);
	internal_bypass_stats.upd++;
}

static int
event_destroy_sync(struct nf_conntrack *ct, int origin)
{
	struct nethdr *net;

	/* this event has been triggered by me, skip */
	if (origin != CTD_ORIGIN_NOT_ME)
		return 1;

	net = BUILD_NETMSG(ct, NET_T_STATE_DEL);
	multichannel_send(STATE_SYNC(channel), net);
	internal_bypass_stats.del++;

	return 1;
}

struct internal_handler internal_bypass = {
	.init			= _init,
	.close			= _close,
	.dump			= dump,
	.flush			= flush,
	.stats			= stats,
	.stats_ext		= stats,
	.populate		= populate,
	.purge			= purge,
	.resync			= resync,
	.new			= event_new_sync,
	.update			= event_update_sync,
	.destroy		= event_destroy_sync,
};
