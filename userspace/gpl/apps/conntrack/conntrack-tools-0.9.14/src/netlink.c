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
 */

#include "netlink.h"
#include "conntrackd.h"
#include "filter.h"
#include "log.h"

#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <libnetfilter_conntrack/libnetfilter_conntrack_tcp.h>

struct nfct_handle *nl_init_event_handler(void)
{
	struct nfct_handle *h;

	h = nfct_open(CONNTRACK, NFCT_ALL_CT_GROUPS);
	if (h == NULL)
		return NULL;

	if (STATE(filter)) {
		if (CONFIG(filter_from_kernelspace)) {
			if (nfct_filter_attach(nfct_fd(h),
					       STATE(filter)) == -1) {
				dlog(LOG_ERR, "cannot set event filtering: %s",
				     strerror(errno));
			}
			dlog(LOG_NOTICE, "using kernel-space event filtering");
		} else
			dlog(LOG_NOTICE, "using user-space event filtering");

		nfct_filter_destroy(STATE(filter));
	}

	fcntl(nfct_fd(h), F_SETFL, O_NONBLOCK);

	/* set up socket buffer size */
	if (CONFIG(netlink_buffer_size)) {
		CONFIG(netlink_buffer_size) = 
		    nfnl_rcvbufsiz(nfct_nfnlh(h), CONFIG(netlink_buffer_size));
	} else {
		socklen_t socklen = sizeof(unsigned int);
		unsigned int read_size;

		/* get current buffer size */
		getsockopt(nfct_fd(h), SOL_SOCKET,
			   SO_RCVBUF, &read_size, &socklen);

		CONFIG(netlink_buffer_size) = read_size;
	}

	dlog(LOG_NOTICE, "netlink event socket buffer size has been set "
			 "to %u bytes", CONFIG(netlink_buffer_size));

	/* ensure that maximum grown size is >= than maximum size */
	if (CONFIG(netlink_buffer_size_max_grown) < CONFIG(netlink_buffer_size))
		CONFIG(netlink_buffer_size_max_grown) =
					CONFIG(netlink_buffer_size);

	if (CONFIG(netlink).events_reliable) {
		int on = 1;

		setsockopt(nfct_fd(h), SOL_NETLINK,
			   NETLINK_BROADCAST_SEND_ERROR, &on, sizeof(int));

		setsockopt(nfct_fd(h), SOL_NETLINK,
			   NETLINK_NO_ENOBUFS, &on, sizeof(int));

		dlog(LOG_NOTICE, "reliable ctnetlink event delivery "
				 "is ENABLED.");
	}
	return h;
}

struct nlif_handle *nl_init_interface_handler(void)
{
	struct nlif_handle *h;
	h = nlif_open();
	if (h == NULL)
		return NULL;

	if (nlif_query(h) == -1) {
		free(h);
		return NULL;
	}
	fcntl(nlif_fd(h), F_SETFL, O_NONBLOCK);

	return h;
}

static int warned = 0;

void nl_resize_socket_buffer(struct nfct_handle *h)
{
	/* sock_setsockopt in net/core/sock.c doubles the size of the buffer */
	unsigned int s = CONFIG(netlink_buffer_size);

	/* already warned that we have reached the maximum buffer size */
	if (warned)
		return;

	if (s > CONFIG(netlink_buffer_size_max_grown)) {
		dlog(LOG_WARNING,
		     "maximum netlink socket buffer "
		     "size has been reached. We are likely to "
		     "be losing events, this may lead to "
		     "unsynchronized replicas. Please, consider "
		     "increasing netlink socket buffer size via "
		     "SocketBufferSize and "
		     "SocketBufferSizeMaxGrowth clauses in "
		     "conntrackd.conf");
		s = CONFIG(netlink_buffer_size_max_grown);
		warned = 1;
	}

	CONFIG(netlink_buffer_size) = nfnl_rcvbufsiz(nfct_nfnlh(h), s);

	/* notify the sysadmin */
	dlog(LOG_NOTICE, "netlink socket buffer size has been increased "
			 "to %u bytes", CONFIG(netlink_buffer_size));
}

int nl_dump_conntrack_table(struct nfct_handle *h)
{
	return nfct_query(h, NFCT_Q_DUMP, &CONFIG(family));
}

int nl_flush_conntrack_table(struct nfct_handle *h)
{
	return nfct_query(h, NFCT_Q_FLUSH, &CONFIG(family));
}

int nl_send_resync(struct nfct_handle *h)
{
	int family = CONFIG(family);
	return nfct_send(h, NFCT_Q_DUMP, &family);
}

/* if the handle has no callback, check for existence, otherwise, update */
int nl_get_conntrack(struct nfct_handle *h, const struct nf_conntrack *ct)
{
	int ret;
	char __tmp[nfct_maxsize()];
	struct nf_conntrack *tmp = (struct nf_conntrack *) (void *)__tmp;

	memset(__tmp, 0, sizeof(__tmp));

	/* use the original tuple to check if it is there */
	nfct_copy(tmp, ct, NFCT_CP_ORIG);

	ret = nfct_query(h, NFCT_Q_GET, tmp);
	if (ret == -1)
		return errno == ENOENT ? 0 : -1;

	return 1;
}

int nl_create_conntrack(struct nfct_handle *h, 
			const struct nf_conntrack *orig,
			int timeout)
{
	int ret;
	struct nf_conntrack *ct;

	ct = nfct_clone(orig);
	if (ct == NULL)
		return -1;

	if (timeout > 0)
		nfct_set_attr_u32(ct, ATTR_TIMEOUT, timeout);

	/* we hit error if we try to change the expected bit */
	if (nfct_attr_is_set(ct, ATTR_STATUS)) {
		uint32_t status = nfct_get_attr_u32(ct, ATTR_STATUS);
		status &= ~IPS_EXPECTED;
		nfct_set_attr_u32(ct, ATTR_STATUS, status);
	}

	nfct_setobjopt(ct, NFCT_SOPT_SETUP_REPLY);

	/*
	 * TCP flags to overpass window tracking for recovered connections
	 */
	if (nfct_attr_is_set(ct, ATTR_TCP_STATE)) {
		uint8_t flags = IP_CT_TCP_FLAG_BE_LIBERAL |
				IP_CT_TCP_FLAG_SACK_PERM;

		/* FIXME: workaround, we should send TCP flags in updates */
		if (nfct_get_attr_u8(ct, ATTR_TCP_STATE) >=
						TCP_CONNTRACK_TIME_WAIT) {
			flags |= IP_CT_TCP_FLAG_CLOSE_INIT;
		}
		nfct_set_attr_u8(ct, ATTR_TCP_FLAGS_ORIG, flags);
		nfct_set_attr_u8(ct, ATTR_TCP_MASK_ORIG, flags);
		nfct_set_attr_u8(ct, ATTR_TCP_FLAGS_REPL, flags);
		nfct_set_attr_u8(ct, ATTR_TCP_MASK_REPL, flags);
	}

	ret = nfct_query(h, NFCT_Q_CREATE, ct);
	nfct_destroy(ct);

	return ret;
}

int nl_update_conntrack(struct nfct_handle *h,
			const struct nf_conntrack *orig,
			int timeout)
{
	int ret;
	struct nf_conntrack *ct;

	ct = nfct_clone(orig);
	if (ct == NULL)
		return -1;

	if (timeout > 0)
		nfct_set_attr_u32(ct, ATTR_TIMEOUT, timeout);

	/* unset NAT info, otherwise we hit error */
	nfct_attr_unset(ct, ATTR_SNAT_IPV4);
	nfct_attr_unset(ct, ATTR_DNAT_IPV4);
	nfct_attr_unset(ct, ATTR_SNAT_PORT);
	nfct_attr_unset(ct, ATTR_DNAT_PORT);

	if (nfct_attr_is_set(ct, ATTR_STATUS)) {
		uint32_t status = nfct_get_attr_u32(ct, ATTR_STATUS);
		status &= ~IPS_NAT_MASK;
		nfct_set_attr_u32(ct, ATTR_STATUS, status);
	}
	/* we have to unset the helper to avoid EBUSY in reset timers */
	if (nfct_attr_is_set(ct, ATTR_HELPER_NAME))
		nfct_attr_unset(ct, ATTR_HELPER_NAME);

	/* we hit error if we try to update the master conntrack */
	if (ct_is_related(ct)) {
		nfct_attr_unset(ct, ATTR_MASTER_L3PROTO);
		nfct_attr_unset(ct, ATTR_MASTER_L4PROTO);
		nfct_attr_unset(ct, ATTR_MASTER_IPV4_SRC);
		nfct_attr_unset(ct, ATTR_MASTER_IPV4_DST);
		nfct_attr_unset(ct, ATTR_MASTER_IPV6_SRC);
		nfct_attr_unset(ct, ATTR_MASTER_IPV6_DST);
		nfct_attr_unset(ct, ATTR_MASTER_PORT_SRC);
		nfct_attr_unset(ct, ATTR_MASTER_PORT_DST);
	}

	/*
	 * TCP flags to overpass window tracking for recovered connections
	 */
	if (nfct_attr_is_set(ct, ATTR_TCP_STATE)) {
		uint8_t flags = IP_CT_TCP_FLAG_BE_LIBERAL |
				IP_CT_TCP_FLAG_SACK_PERM;

		/* FIXME: workaround, we should send TCP flags in updates */
		if (nfct_get_attr_u8(ct, ATTR_TCP_STATE) >=
						TCP_CONNTRACK_TIME_WAIT) {
			flags |= IP_CT_TCP_FLAG_CLOSE_INIT;
		}
		nfct_set_attr_u8(ct, ATTR_TCP_FLAGS_ORIG, flags);
		nfct_set_attr_u8(ct, ATTR_TCP_MASK_ORIG, flags);
		nfct_set_attr_u8(ct, ATTR_TCP_FLAGS_REPL, flags);
		nfct_set_attr_u8(ct, ATTR_TCP_MASK_REPL, flags);
	}

	ret = nfct_query(h, NFCT_Q_UPDATE, ct);
	nfct_destroy(ct);

	return ret;
}

int nl_destroy_conntrack(struct nfct_handle *h, const struct nf_conntrack *ct)
{
	return nfct_query(h, NFCT_Q_DESTROY, ct);
}
