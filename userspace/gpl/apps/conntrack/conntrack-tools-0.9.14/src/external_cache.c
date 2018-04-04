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
 */
#include "conntrackd.h"
#include "sync.h"
#include "log.h"
#include "cache.h"
#include "external.h"

#include <libnetfilter_conntrack/libnetfilter_conntrack.h>
#include <stdlib.h>

static struct cache *external;

static int external_cache_init(void)
{
	external = cache_create("external",
				STATE_SYNC(sync)->external_cache_flags,
				NULL);
	if (external == NULL) {
		dlog(LOG_ERR, "can't allocate memory for the external cache");
		return -1;
	}
	return 0;
}

static void external_cache_close(void)
{
	cache_destroy(external);
}

static void external_cache_new(struct nf_conntrack *ct)
{
	struct cache_object *obj;
	int id;

	obj = cache_find(external, ct, &id);
	if (obj == NULL) {
retry:
		obj = cache_object_new(external, ct);
		if (obj == NULL)
			return;

		if (cache_add(external, obj, id) == -1) {
			cache_object_free(obj);
			return;
		}
	} else {
		cache_del(external, obj);
		cache_object_free(obj);
		goto retry;
	}
}

static void external_cache_upd(struct nf_conntrack *ct)
{
	cache_update_force(external, ct);
}

static void external_cache_del(struct nf_conntrack *ct)
{
	struct cache_object *obj;
	int id;

	obj = cache_find(external, ct, &id);
	if (obj) {
		cache_del(external, obj);
		cache_object_free(obj);
	}
}

static void external_cache_dump(int fd, int type)
{
	cache_dump(external, fd, type);
}

static void external_cache_commit(struct nfct_handle *h, int fd)
{
	cache_commit(external, h, fd);
}

static void external_cache_flush(void)
{
	cache_flush(external);
}

static void external_cache_stats(int fd)
{
	cache_stats(external, fd);
}

static void external_cache_stats_ext(int fd)
{
	cache_stats_extended(external, fd);
}

struct external_handler external_cache = {
	.init		= external_cache_init,
	.close		= external_cache_close,
	.new		= external_cache_new,
	.update		= external_cache_upd,
	.destroy	= external_cache_del,
	.dump		= external_cache_dump,
	.commit		= external_cache_commit,
	.flush		= external_cache_flush,
	.stats		= external_cache_stats,
	.stats_ext	= external_cache_stats_ext,
};
