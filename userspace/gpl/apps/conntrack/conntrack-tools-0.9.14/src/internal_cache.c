/*
 * (C) 2009 by Pablo Neira Ayuso <pablo@netfilter.org>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "conntrackd.h"
#include "sync.h"
#include "log.h"
#include "cache.h"
#include "netlink.h"
#include "network.h"
#include "origin.h"

static inline void sync_send(struct cache_object *obj, int query)
{
	STATE_SYNC(sync)->enqueue(obj, query);
}

static int _init(void)
{
	STATE(mode)->internal->data =
		cache_create("internal", 
			     STATE_SYNC(sync)->internal_cache_flags,
			     STATE_SYNC(sync)->internal_cache_extra);

	if (!STATE(mode)->internal->data) {
		dlog(LOG_ERR, "can't allocate memory for the internal cache");
		return -1;
	}
	return 0;
}

static void _close(void)
{
	cache_destroy(STATE(mode)->internal->data);
}

static void dump(int fd, int type)
{
	cache_dump(STATE(mode)->internal->data, fd, NFCT_O_PLAIN);
}

static void flush(void)
{
	cache_flush(STATE(mode)->internal->data);
}

static void stats(int fd)
{
	cache_stats(STATE(mode)->internal->data, fd);
}

static void stats_ext(int fd)
{
	cache_stats_extended(STATE(mode)->internal->data, fd);
}

static void populate(struct nf_conntrack *ct)
{
	/* This is required by kernels < 2.6.20 */
	nfct_attr_unset(ct, ATTR_ORIG_COUNTER_BYTES);
	nfct_attr_unset(ct, ATTR_ORIG_COUNTER_PACKETS);
	nfct_attr_unset(ct, ATTR_REPL_COUNTER_BYTES);
	nfct_attr_unset(ct, ATTR_REPL_COUNTER_PACKETS);
	nfct_attr_unset(ct, ATTR_USE);

	cache_update_force(STATE(mode)->internal->data, ct);
}

static int purge_step(void *data1, void *data2)
{
	struct cache_object *obj = data2;

	STATE(get_retval) = 0;
	nl_get_conntrack(STATE(get), obj->ct);	/* modifies STATE(get_reval) */
	if (!STATE(get_retval)) {
		if (obj->status != C_OBJ_DEAD) {
			cache_object_set_status(obj, C_OBJ_DEAD);
			sync_send(obj, NET_T_STATE_DEL);
			cache_object_put(obj);
		}
	}

	return 0;
}

static void purge(void)
{
	cache_iterate(STATE(mode)->internal->data, NULL, purge_step);
}

static int resync(enum nf_conntrack_msg_type type,
		  struct nf_conntrack *ct,
		  void *data)
{
	struct cache_object *obj;

	if (ct_filter_conntrack(ct, 1))
		return NFCT_CB_CONTINUE;

	/* This is required by kernels < 2.6.20 */
	nfct_attr_unset(ct, ATTR_ORIG_COUNTER_BYTES);
	nfct_attr_unset(ct, ATTR_ORIG_COUNTER_PACKETS);
	nfct_attr_unset(ct, ATTR_REPL_COUNTER_BYTES);
	nfct_attr_unset(ct, ATTR_REPL_COUNTER_PACKETS);
	nfct_attr_unset(ct, ATTR_USE);

	obj = cache_update_force(STATE(mode)->internal->data, ct);
	if (obj == NULL)
		return NFCT_CB_CONTINUE;

	switch (obj->status) {
	case C_OBJ_NEW:
		sync_send(obj, NET_T_STATE_NEW);
		break;
	case C_OBJ_ALIVE:
		sync_send(obj, NET_T_STATE_UPD);
		break;
	}
	return NFCT_CB_CONTINUE;
}

static void
event_new_sync(struct nf_conntrack *ct, int origin)
{
	struct cache_object *obj;
	int id;

	/* this event has been triggered by a direct inject, skip */
	if (origin == CTD_ORIGIN_INJECT)
		return;

	/* required by linux kernel <= 2.6.20 */
	nfct_attr_unset(ct, ATTR_ORIG_COUNTER_BYTES);
	nfct_attr_unset(ct, ATTR_ORIG_COUNTER_PACKETS);
	nfct_attr_unset(ct, ATTR_REPL_COUNTER_BYTES);
	nfct_attr_unset(ct, ATTR_REPL_COUNTER_PACKETS);

	obj = cache_find(STATE(mode)->internal->data, ct, &id);
	if (obj == NULL) {
retry:
		obj = cache_object_new(STATE(mode)->internal->data, ct);
		if (obj == NULL)
			return;
		if (cache_add(STATE(mode)->internal->data, obj, id) == -1) {
			cache_object_free(obj);
			return;
		}
		/* only synchronize events that have been triggered by other
		 * processes or the kernel, but don't propagate events that
		 * have been triggered by conntrackd itself, eg. commits. */
		if (origin == CTD_ORIGIN_NOT_ME)
			sync_send(obj, NET_T_STATE_NEW);
	} else {
		cache_del(STATE(mode)->internal->data, obj);
		cache_object_free(obj);
		goto retry;
	}
}

static void
event_update_sync(struct nf_conntrack *ct, int origin)
{
	struct cache_object *obj;

	/* this event has been triggered by a direct inject, skip */
	if (origin == CTD_ORIGIN_INJECT)
		return;

	obj = cache_update_force(STATE(mode)->internal->data, ct);
	if (obj == NULL)
		return;

	if (origin == CTD_ORIGIN_NOT_ME)
		sync_send(obj, NET_T_STATE_UPD);
}

static int
event_destroy_sync(struct nf_conntrack *ct, int origin)
{
	struct cache_object *obj;
	int id;

	/* this event has been triggered by a direct inject, skip */
	if (origin == CTD_ORIGIN_INJECT)
		return 0;

	/* we don't synchronize events for objects that are not in the cache */
	obj = cache_find(STATE(mode)->internal->data, ct, &id);
	if (obj == NULL)
		return 0;

	if (obj->status != C_OBJ_DEAD) {
		cache_object_set_status(obj, C_OBJ_DEAD);
		if (origin == CTD_ORIGIN_NOT_ME) {
			sync_send(obj, NET_T_STATE_DEL);
		}
		cache_object_put(obj);
	}
	return 1;
}

struct internal_handler internal_cache = {
	.flags			= INTERNAL_F_POPULATE | INTERNAL_F_RESYNC,
	.init			= _init,
	.close			= _close,
	.dump			= dump,
	.flush			= flush,
	.stats			= stats,
	.stats_ext		= stats_ext,
	.populate		= populate,
	.purge			= purge,
	.resync			= resync,
	.new			= event_new_sync,
	.update			= event_update_sync,
	.destroy		= event_destroy_sync,
};
