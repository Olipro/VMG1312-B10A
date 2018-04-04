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

#include "cache.h"
#include "hash.h"
#include "log.h"
#include "conntrackd.h"
#include "netlink.h"
#include "event.h"

#include <libnetfilter_conntrack/libnetfilter_conntrack.h>
#include <sched.h>
#include <errno.h>
#include <string.h>
#include <time.h>

struct __dump_container {
	int fd;
	int type;
};

static int do_dump(void *data1, void *n)
{
	char buf[1024];
	int size;
	struct __dump_container *container = data1;
	struct cache_object *obj = n;
	char *data = obj->data;
	unsigned i;

	/*
	 * XXX: Do not dump the entries that are scheduled to expire.
	 * 	These entries talk about already destroyed connections
	 * 	that we keep for some time just in case that we have to
	 * 	resent some lost messages. We do not show them to the
	 * 	user as he may think that the firewall replicas are not
	 * 	in sync. The branch below is a hack as it is quite
	 * 	specific and it breaks conntrackd modularity. Probably
	 * 	there's a nicer way to do this but until I come up with it...
	 */
	if (CONFIG(flags) & CTD_SYNC_FTFW && obj->status == C_OBJ_DEAD)
		return 0;

	/* do not show cached timeout, this may confuse users */
	if (nfct_attr_is_set(obj->ct, ATTR_TIMEOUT))
		nfct_attr_unset(obj->ct, ATTR_TIMEOUT);

	memset(buf, 0, sizeof(buf));
	size = nfct_snprintf(buf, 
			     sizeof(buf), 
			     obj->ct, 
			     NFCT_T_UNKNOWN, 
			     container->type,
			     0);

	for (i = 0; i < obj->cache->num_features; i++) {
		if (obj->cache->features[i]->dump) {
			size += obj->cache->features[i]->dump(obj, 
							      data, 
							      buf+size,
							      container->type);
			data += obj->cache->features[i]->size;
		}
	}
	if (container->type != NFCT_O_XML) {
		long tm = time(NULL);
		size += sprintf(buf+size, " [active since %lds]",
				tm - obj->lifetime);
	}
	size += sprintf(buf+size, "\n");
	if (send(container->fd, buf, size, 0) == -1) {
		if (errno != EPIPE)
			return -1;
	}

	return 0;
}

void cache_dump(struct cache *c, int fd, int type)
{
	struct __dump_container tmp = {
		.fd	= fd,
		.type	= type
	};

	hashtable_iterate(c->h, (void *) &tmp, do_dump);
}

struct __commit_container {
	struct nfct_handle 	*h;
	struct cache 		*c;
};

static void
__do_commit_step(struct __commit_container *tmp, struct cache_object *obj)
{
	int ret, retry = 1, timeout;
	struct nf_conntrack *ct = obj->ct;

	if (CONFIG(commit_timeout)) {
		timeout = CONFIG(commit_timeout);
	} else {
		timeout = time(NULL) - obj->lastupdate;
		if (timeout < 0) {
			/* XXX: Arbitrarily set the timer to one minute, how
			 * can this happen? For example, an adjustment due to
			 * daylight-saving. Probably other situations can
			 * trigger this. */
			timeout = 60;
		}
		/* calculate an estimation of the current timeout */
		timeout = nfct_get_attr_u32(ct, ATTR_TIMEOUT) - timeout;
		if (timeout < 0) {
			timeout = 60;
		}
	}

retry:
	if (nl_create_conntrack(tmp->h, ct, timeout) == -1) {
		if (errno == EEXIST && retry == 1) {
			ret = nl_destroy_conntrack(tmp->h, ct);
			if (ret == 0 || (ret == -1 && errno == ENOENT)) {
				if (retry) {
					retry = 0;
					goto retry;
				}
			}
			dlog(LOG_ERR, "commit-destroy: %s", strerror(errno));
			dlog_ct(STATE(log), ct, NFCT_O_PLAIN);
			tmp->c->stats.commit_fail++;
		} else {
			dlog(LOG_ERR, "commit-create: %s", strerror(errno));
			dlog_ct(STATE(log), ct, NFCT_O_PLAIN);
			tmp->c->stats.commit_fail++;
		}
	} else {
		tmp->c->stats.commit_ok++;
	}
}

static int do_commit_related(void *data, void *n)
{
	struct cache_object *obj = n;

	if (ct_is_related(obj->ct))
		__do_commit_step(data, obj);

	/* keep iterating even if we have found errors */
	return 0;
}

static int do_commit_master(void *data, void *n)
{
	struct cache_object *obj = n;

	if (ct_is_related(obj->ct))
		return 0;

	__do_commit_step(data, obj);
	return 0;
}

void cache_commit(struct cache *c, struct nfct_handle *h, int clientfd)
{
	unsigned int commit_ok, commit_fail;
	struct __commit_container tmp = {
		.h = h,
		.c = c,
	};
	struct timeval commit_stop, res;

	switch(STATE_SYNC(commit).state) {
	case COMMIT_STATE_INACTIVE:
		gettimeofday(&STATE_SYNC(commit).stats.start, NULL);
		STATE_SYNC(commit).stats.ok = c->stats.commit_ok;
		STATE_SYNC(commit).stats.fail = c->stats.commit_fail;
		STATE_SYNC(commit).clientfd = clientfd;
	case COMMIT_STATE_MASTER:
		STATE_SYNC(commit).current =
			hashtable_iterate_limit(c->h, &tmp,
						STATE_SYNC(commit).current,
						CONFIG(general).commit_steps,
						do_commit_master);
		if (STATE_SYNC(commit).current < CONFIG(hashsize)) {
			STATE_SYNC(commit).state = COMMIT_STATE_MASTER;
			/* give it another step as soon as possible */
			write_evfd(STATE_SYNC(commit).evfd);
			return;
		}
		STATE_SYNC(commit).current = 0;
		STATE_SYNC(commit).state = COMMIT_STATE_RELATED;
	case COMMIT_STATE_RELATED:
		STATE_SYNC(commit).current =
			hashtable_iterate_limit(c->h, &tmp,
						STATE_SYNC(commit).current,
						CONFIG(general).commit_steps,
						do_commit_related);
		if (STATE_SYNC(commit).current < CONFIG(hashsize)) {
			STATE_SYNC(commit).state = COMMIT_STATE_RELATED;
			/* give it another step as soon as possible */
			write_evfd(STATE_SYNC(commit).evfd);
			return;
		}
		/* calculate the time that commit has taken */
		gettimeofday(&commit_stop, NULL);
		timersub(&commit_stop, &STATE_SYNC(commit).stats.start, &res);

		/* calculate new entries committed */
		commit_ok = c->stats.commit_ok - STATE_SYNC(commit).stats.ok;
		commit_fail = 
			c->stats.commit_fail - STATE_SYNC(commit).stats.fail;

		/* log results */
		dlog(LOG_NOTICE, "Committed %u new entries", commit_ok);

		if (commit_fail)
			dlog(LOG_NOTICE, "%u entries can't be "
					 "committed", commit_fail);

		dlog(LOG_NOTICE, "commit has taken %lu.%06lu seconds", 
				res.tv_sec, res.tv_usec);

		/* prepare the state machine for new commits */
		STATE_SYNC(commit).current = 0;
		STATE_SYNC(commit).state = COMMIT_STATE_INACTIVE;

		/* Close the client socket now that we're done. */
		close(STATE_SYNC(commit).clientfd);
	}
}

static int do_flush(void *data, void *n)
{
	struct cache *c = data;
	struct cache_object *obj = n;

	cache_del(c, obj);
	cache_object_free(obj);
	return 0;
}

void cache_flush(struct cache *c)
{
	hashtable_iterate(c->h, c, do_flush);
	c->stats.flush++;
}
