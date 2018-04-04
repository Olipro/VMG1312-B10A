/*
 * Copyright (c) 1990, 1991, 1992, 1993, 1994, 1995, 1996
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that: (1) source code distributions
 * retain the above copyright notice and this paragraph in its entirety, (2)
 * distributions including binary code include the above copyright notice and
 * this paragraph in its entirety in the documentation or other materials
 * provided with the distribution, and (3) all advertising materials mentioning
 * features or use of this software display the following acknowledgement:
 * ``This product includes software developed by the University of California,
 * Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
 * the University nor the names of its contributors may be used to endorse
 * or promote products derived from this software without specific prior
 * written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * packet filter subroutines for tcpdump
 *	Extraction/creation by Jeffrey Mogul, DECWRL
 */

#ifndef lint
static const char rcsid[] =
    "@(#) $Header: /tcpdump/master/libpcap/pcap-pf.c,v 1.72 2003/01/03 08:33:24 guy Exp $ (LBL)";
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <net/pfilt.h>

struct mbuf;
struct rtentry;
#include <net/if.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/tcp.h>
#include <netinet/tcpip.h>

#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pcap-int.h"

#ifdef HAVE_OS_PROTO_H
#include "os-proto.h"
#endif

/*
 * BUFSPACE is the size in bytes of the packet read buffer.  Most tcpdump
 * applications aren't going to need more than 200 bytes of packet header
 * and the read shouldn't return more packets than packetfilter's internal
 * queue limit (bounded at 256).
 */
#define BUFSPACE (200 * 256)

int
pcap_read(pcap_t *pc, int cnt, pcap_handler callback, u_char *user)
{
	register u_char *p, *bp;
	struct bpf_insn *fcode;
	register int cc, n, buflen, inc;
	register struct enstamp *sp;
#ifdef LBL_ALIGN
	struct enstamp stamp;
#endif
#ifdef PCAP_FDDIPAD
	register int pad;
#endif

	fcode = pc->md.use_bpf ? NULL : pc->fcode.bf_insns;
 again:
	cc = pc->cc;
	if (cc == 0) {
		cc = read(pc->fd, (char *)pc->buffer + pc->offset, pc->bufsize);
		if (cc < 0) {
			if (errno == EWOULDBLOCK)
				return (0);
			if (errno == EINVAL &&
			    lseek(pc->fd, 0L, SEEK_CUR) + pc->bufsize < 0) {
				/*
				 * Due to a kernel bug, after 2^31 bytes,
				 * the kernel file offset overflows and
				 * read fails with EINVAL. The lseek()
				 * to 0 will fix things.
				 */
				(void)lseek(pc->fd, 0L, SEEK_SET);
				goto again;
			}
			snprintf(pc->errbuf, sizeof(pc->errbuf), "pf read: %s",
				pcap_strerror(errno));
			return (-1);
		}
		bp = pc->buffer + pc->offset;
	} else
		bp = pc->bp;
	/*
	 * Loop through each packet.
	 */
	n = 0;
#ifdef PCAP_FDDIPAD
	if (pc->linktype == DLT_FDDI)
		pad = pcap_fddipad;
	else
		pad = 0;
#endif
	while (cc > 0) {
		if (cc < sizeof(*sp)) {
			snprintf(pc->errbuf, sizeof(pc->errbuf),
			    "pf short read (%d)", cc);
			return (-1);
		}
#ifdef LBL_ALIGN
		if ((long)bp & 3) {
			sp = &stamp;
			memcpy((char *)sp, (char *)bp, sizeof(*sp));
		} else
#endif
			sp = (struct enstamp *)bp;
		if (sp->ens_stamplen != sizeof(*sp)) {
			snprintf(pc->errbuf, sizeof(pc->errbuf),
			    "pf short stamplen (%d)",
			    sp->ens_stamplen);
			return (-1);
		}

		p = bp + sp->ens_stamplen;
		buflen = sp->ens_count;
		if (buflen > pc->snapshot)
			buflen = pc->snapshot;

		/* Calculate inc before possible pad update */
		inc = ENALIGN(buflen + sp->ens_stamplen);
		cc -= inc;
		bp += inc;
#ifdef PCAP_FDDIPAD
		p += pad;
		buflen -= pad;
#endif
		pc->md.TotPkts++;
		pc->md.TotDrops += sp->ens_dropped;
		pc->md.TotMissed = sp->ens_ifoverflows;
		if (pc->md.OrigMissed < 0)
			pc->md.OrigMissed = pc->md.TotMissed;

		/*
		 * Short-circuit evaluation: if using BPF filter
		 * in kernel, no need to do it now.
		 */
		if (fcode == NULL ||
		    bpf_filter(fcode, p, sp->ens_count, buflen)) {
			struct pcap_pkthdr h;
			pc->md.TotAccepted++;
			h.ts = sp->ens_tstamp;
#ifdef PCAP_FDDIPAD
			h.len = sp->ens_count - pad;
#else
			h.len = sp->ens_count;
#endif
			h.caplen = buflen;
			(*callback)(user, &h, p);
			if (++n >= cnt && cnt > 0) {
				pc->cc = cc;
				pc->bp = bp;
				return (n);
			}
		}
	}
	pc->cc = 0;
	return (n);
}

int
pcap_stats(pcap_t *p, struct pcap_stat *ps)
{

	/*
	 * If packet filtering is being done in the kernel:
	 *
	 *	"ps_recv" counts only packets that passed the filter.
	 *	This does not include packets dropped because we
	 *	ran out of buffer space.  (XXX - perhaps it should,
	 *	by adding "ps_drop" to "ps_recv", for compatibility
	 *	with some other platforms.  On the other hand, on
	 *	some platforms "ps_recv" counts only packets that
	 *	passed the filter, and on others it counts packets
	 *	that didn't pass the filter....)
	 *
	 *	"ps_drop" counts packets that passed the kernel filter
	 *	(if any) but were dropped because the input queue was
	 *	full.
	 *
	 *	"ps_ifdrop" counts packets dropped by the network
	 *	inteface (regardless of whether they would have passed
	 *	the input filter, of course).
	 *
	 * If packet filtering is not being done in the kernel:
	 *
	 *	"ps_recv" counts only packets that passed the filter.
	 *
	 *	"ps_drop" counts packets that were dropped because the
	 *	input queue was full, regardless of whether they passed
	 *	the userland filter.
	 *
	 *	"ps_ifdrop" counts packets dropped by the network
	 *	inteface (regardless of whether they would have passed
	 *	the input filter, of course).
	 *
	 * These statistics don't include packets not yet read from
	 * the kernel by libpcap, but they may include packets not
	 * yet read from libpcap by the application.
	 */
	ps->ps_recv = p->md.TotAccepted;
	ps->ps_drop = p->md.TotDrops;
	ps->ps_ifdrop = p->md.TotMissed - p->md.OrigMissed;
	return (0);
}

pcap_t *
pcap_open_live(const char *device, int snaplen, int promisc, int to_ms,
    char *ebuf)
{
	pcap_t *p;
	short enmode;
	int backlog = -1;	/* request the most */
	struct enfilter Filter;
	struct endevp devparams;

	p = (pcap_t *)malloc(sizeof(*p));
	if (p == NULL) {
		snprintf(ebuf, PCAP_ERRBUF_SIZE,
		    "pcap_open_live: %s", pcap_strerror(errno));
		return (0);
	}
	memset(p, 0, sizeof(*p));

	/*
	 * XXX - we assume here that "pfopen()" does not, in fact, modify
	 * its argument, even though it takes a "char *" rather than a
	 * "const char *" as its first argument.  That appears to be
	 * the case, at least on Digital UNIX 4.0.
	 */
	p->fd = pfopen(device, O_RDONLY);
	if (p->fd < 0) {
		snprintf(ebuf, PCAP_ERRBUF_SIZE, "pf open: %s: %s\n\
your system may not be properly configured; see the packetfilter(4) man page\n",
			device, pcap_strerror(errno));
		goto bad;
	}
	p->md.OrigMissed = -1;
	enmode = ENTSTAMP|ENBATCH|ENNONEXCL;
	if (promisc)
		enmode |= ENPROMISC;
	if (ioctl(p->fd, EIOCMBIS, (caddr_t)&enmode) < 0) {
		snprintf(ebuf, PCAP_ERRBUF_SIZE, "EIOCMBIS: %s",
		    pcap_strerror(errno));
		goto bad;
	}
#ifdef	ENCOPYALL
	/* Try to set COPYALL mode so that we see packets to ourself */
	enmode = ENCOPYALL;
	(void)ioctl(p->fd, EIOCMBIS, (caddr_t)&enmode);/* OK if this fails */
#endif
	/* set the backlog */
	if (ioctl(p->fd, EIOCSETW, (caddr_t)&backlog) < 0) {
		snprintf(ebuf, PCAP_ERRBUF_SIZE, "EIOCSETW: %s",
		    pcap_strerror(errno));
		goto bad;
	}
	/* discover interface type */
	if (ioctl(p->fd, EIOCDEVP, (caddr_t)&devparams) < 0) {
		snprintf(ebuf, PCAP_ERRBUF_SIZE, "EIOCDEVP: %s",
		    pcap_strerror(errno));
		goto bad;
	}
	/* HACK: to compile prior to Ultrix 4.2 */
#ifndef	ENDT_FDDI
#define	ENDT_FDDI	4
#endif
	switch (devparams.end_dev_type) {

	case ENDT_10MB:
		p->linktype = DLT_EN10MB;
		p->offset = 2;
		break;

	case ENDT_FDDI:
		p->linktype = DLT_FDDI;
		break;

#ifdef ENDT_SLIP
	case ENDT_SLIP:
		p->linktype = DLT_SLIP;
		break;
#endif

#ifdef ENDT_PPP
	case ENDT_PPP:
		p->linktype = DLT_PPP;
		break;
#endif

#ifdef ENDT_LOOPBACK
	case ENDT_LOOPBACK:
		/*
		 * It appears to use Ethernet framing, at least on
		 * Digital UNIX 4.0.
		 */
		p->linktype = DLT_EN10MB;
		p->offset = 2;
		break;
#endif

#ifdef ENDT_TRN
	case ENDT_TRN:
		p->linktype = DLT_IEEE802;
		break;
#endif

	default:
		/*
		 * XXX - what about ENDT_IEEE802?  The pfilt.h header
		 * file calls this "IEEE 802 networks (non-Ethernet)",
		 * but that doesn't specify a specific link layer type;
		 * it could be 802.4, or 802.5 (except that 802.5 is
		 * ENDT_TRN), or 802.6, or 802.11, or....  That's why
		 * DLT_IEEE802 was hijacked to mean Token Ring in various
		 * BSDs, and why we went along with that hijacking.
		 *
		 * XXX - what about ENDT_HDLC and ENDT_NULL?
		 * Presumably, as ENDT_OTHER is just "Miscellaneous
		 * framing", there's not much we can do, as that
		 * doesn't specify a particular type of header.
		 */
		snprintf(ebuf, PCAP_ERRBUF_SIZE, "unknown data-link type %u",
		    devparams.end_dev_type);
		goto bad;
	}
	/* set truncation */
#ifdef PCAP_FDDIPAD
	if (p->linktype == DLT_FDDI)
		/* packetfilter includes the padding in the snapshot */
		snaplen += pcap_fddipad;
#endif
	if (ioctl(p->fd, EIOCTRUNCATE, (caddr_t)&snaplen) < 0) {
		snprintf(ebuf, PCAP_ERRBUF_SIZE, "EIOCTRUNCATE: %s",
		    pcap_strerror(errno));
		goto bad;
	}
	p->snapshot = snaplen;
	/* accept all packets */
	memset(&Filter, 0, sizeof(Filter));
	Filter.enf_Priority = 37;	/* anything > 2 */
	Filter.enf_FilterLen = 0;	/* means "always true" */
	if (ioctl(p->fd, EIOCSETF, (caddr_t)&Filter) < 0) {
		snprintf(ebuf, PCAP_ERRBUF_SIZE, "EIOCSETF: %s",
		    pcap_strerror(errno));
		goto bad;
	}

	if (to_ms != 0) {
		struct timeval timeout;
		timeout.tv_sec = to_ms / 1000;
		timeout.tv_usec = (to_ms * 1000) % 1000000;
		if (ioctl(p->fd, EIOCSRTIMEOUT, (caddr_t)&timeout) < 0) {
			snprintf(ebuf, PCAP_ERRBUF_SIZE, "EIOCSRTIMEOUT: %s",
				pcap_strerror(errno));
			goto bad;
		}
	}
	p->bufsize = BUFSPACE;
	p->buffer = (u_char*)malloc(p->bufsize + p->offset);

	return (p);
 bad:
	if (p->fd >= 0)
		close(p->fd);
	free(p);
	return (NULL);
}

int
pcap_platform_finddevs(pcap_if_t **alldevsp, char *errbuf)
{
	return (0);
}

int
pcap_setfilter(pcap_t *p, struct bpf_program *fp)
{
	/*
	 * See if BIOCSETF works.  If it does, the kernel supports
	 * BPF-style filters, and we do not need to do post-filtering.
	 */
	p->md.use_bpf = (ioctl(p->fd, BIOCSETF, (caddr_t)fp) >= 0);
	if (p->md.use_bpf) {
		struct bpf_version bv;

		if (ioctl(p->fd, BIOCVERSION, (caddr_t)&bv) < 0) {
			snprintf(p->errbuf, sizeof(p->errbuf),
			    "BIOCVERSION: %s", pcap_strerror(errno));
			return (-1);
		}
		else if (bv.bv_major != BPF_MAJOR_VERSION ||
			 bv.bv_minor < BPF_MINOR_VERSION) {
			fprintf(stderr,
		"requires bpf language %d.%d or higher; kernel is %d.%d",
				BPF_MAJOR_VERSION, BPF_MINOR_VERSION,
			      bv.bv_major, bv.bv_minor);
			/* don't give up, just be inefficient */
			p->md.use_bpf = 0;
		}
	} else {
		if (install_bpf_program(p, fp) < 0)
			return (-1);
	}

	/*XXX this goes in tcpdump*/
	if (p->md.use_bpf)
		fprintf(stderr, "tcpdump: Using kernel BPF filter\n");
	else
		fprintf(stderr, "tcpdump: Filtering in user process\n");
	return (0);
}

int
pcap_set_datalink_platform(pcap_t *p, int dlt)
{
	return (0);
}
