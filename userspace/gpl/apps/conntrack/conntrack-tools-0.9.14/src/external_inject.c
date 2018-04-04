/*
 * (C) 2009 by Pablo Neira Ayuso <pablo@netfilter.org>
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
#include "origin.h"
#include "external.h"
#include "netlink.h"

#include <libnetfilter_conntrack/libnetfilter_conntrack.h>
#include <errno.h>
#include <stdlib.h>

static struct nfct_handle *inject;

struct {
	uint32_t	add_ok;
	uint32_t	add_fail;
	uint32_t	upd_ok;
	uint32_t	upd_fail;
	uint32_t	del_ok;
	uint32_t	del_fail;
} external_inject_stat;

static int external_inject_init(void)
{
	/* handler to directly inject conntracks into kernel-space */
	inject = nfct_open(CONNTRACK, 0);
	if (inject == NULL) {
		dlog(LOG_ERR, "can't open netlink handler: %s",
		     strerror(errno));
		dlog(LOG_ERR, "no ctnetlink kernel support?");
		return -1;
	}
	/* we are directly injecting the entries into the kernel */
	origin_register(inject, CTD_ORIGIN_INJECT);
	return 0;
}

static void external_inject_close(void)
{
	origin_unregister(inject);
	nfct_close(inject);
}

static void external_inject_new(struct nf_conntrack *ct)
{
	int ret, retry = 1;

retry:
	if (nl_create_conntrack(inject, ct, 0) == -1) {
		/* if the state entry exists, we delete and try again */
		if (errno == EEXIST && retry == 1) {
			ret = nl_destroy_conntrack(inject, ct);
			if (ret == 0 || (ret == -1 && errno == ENOENT)) {
				if (retry) {
					retry = 0;
					goto retry;
				}
			}
			external_inject_stat.add_fail++;
			dlog(LOG_ERR, "inject-add1: %s", strerror(errno));
			dlog_ct(STATE(log), ct, NFCT_O_PLAIN);
			return;
		}
		external_inject_stat.add_fail++;
		dlog(LOG_ERR, "inject-add2: %s", strerror(errno));
		dlog_ct(STATE(log), ct, NFCT_O_PLAIN);
	} else {
		external_inject_stat.add_ok++;
	}
}

static void external_inject_upd(struct nf_conntrack *ct)
{
	int ret;

	/* if we successfully update the entry, everything is OK */
	if (nl_update_conntrack(inject, ct, 0) != -1) {
		external_inject_stat.upd_ok++;
		return;
	}

	/* state entries does not exist, we have to create it */
	if (errno == ENOENT) {
		if (nl_create_conntrack(inject, ct, 0) == -1) {
			external_inject_stat.upd_fail++;
			dlog(LOG_ERR, "inject-upd1: %s", strerror(errno));
			dlog_ct(STATE(log), ct, NFCT_O_PLAIN);
		} else {
			external_inject_stat.upd_ok++;
		}
		return;
	}

	/* we failed to update the entry, there are some operations that
	 * may trigger this error, eg. unset some status bits. Try harder,
	 * delete the existing entry and create a new one. */
	ret = nl_destroy_conntrack(inject, ct);
	if (ret == 0 || (ret == -1 && errno == ENOENT)) {
		if (nl_create_conntrack(inject, ct, 0) == -1) {
			external_inject_stat.upd_fail++;
			dlog(LOG_ERR, "inject-upd2: %s", strerror(errno));
			dlog_ct(STATE(log), ct, NFCT_O_PLAIN);
		} else {
			external_inject_stat.upd_ok++;
		}
		return;
	}
	external_inject_stat.upd_fail++;
	dlog(LOG_ERR, "inject-upd3: %s", strerror(errno));
	dlog_ct(STATE(log), ct, NFCT_O_PLAIN);
}

static void external_inject_del(struct nf_conntrack *ct)
{
	if (nl_destroy_conntrack(inject, ct) == -1) {
		if (errno != ENOENT) {
			external_inject_stat.del_fail++;
			dlog(LOG_ERR, "inject-del: %s", strerror(errno));
			dlog_ct(STATE(log), ct, NFCT_O_PLAIN);
		}
	} else {
		external_inject_stat.del_ok++;
	}
}

static void external_inject_dump(int fd, int type)
{
}

static void external_inject_commit(struct nfct_handle *h, int fd)
{
}

static void external_inject_flush(void)
{
}

static void external_inject_stats(int fd)
{
	char buf[512];
	int size;

	size = sprintf(buf, "external inject:\n"
			    "connections created:\t\t%12u\tfailed:\t%12u\n"
			    "connections updated:\t\t%12u\tfailed:\t%12u\n"
			    "connections destroyed:\t\t%12u\tfailed:\t%12u\n\n",
			    external_inject_stat.add_ok,
			    external_inject_stat.add_fail,
			    external_inject_stat.upd_ok,
			    external_inject_stat.upd_fail,
			    external_inject_stat.del_ok,
			    external_inject_stat.del_fail);

	send(fd, buf, size, 0);
}

struct external_handler external_inject = {
	.init		= external_inject_init,
	.close		= external_inject_close,
	.new		= external_inject_new,
	.update		= external_inject_upd,
	.destroy	= external_inject_del,
	.dump		= external_inject_dump,
	.commit		= external_inject_commit,
	.flush		= external_inject_flush,
	.stats		= external_inject_stats,
	.stats_ext	= external_inject_stats,
};
