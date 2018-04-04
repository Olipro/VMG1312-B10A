/* vi: set sw=4 ts=4: */
/*
 * Mini ping implementation for busybox
 *
 * Copyright (C) 1999 by Randolph Chung <tausq@debian.org>
 *
 * Adapted from the ping in netkit-base 0.10:
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Mike Muuss.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */
/* from ping6.c:
 * Copyright (C) 1999 by Randolph Chung <tausq@debian.org>
 *
 * This version of ping is adapted from the ping in netkit-base 0.10,
 * which is:
 *
 * Original copyright notice is retained at the end of this file.
 *
 * This version is an adaptation of ping.c from busybox.
 * The code was modified by Bart Visscher <magick@linux-fan.com>
 */

#include <net/if.h>
#include <netinet/ip_icmp.h>
#include "libbb.h"

#if ENABLE_PING6
# include <netinet/icmp6.h>
/* I see RENUMBERED constants in bits/in.h - !!?
 * What a fuck is going on with libc? Is it a glibc joke? */
# ifdef IPV6_2292HOPLIMIT
#  undef IPV6_HOPLIMIT
#  define IPV6_HOPLIMIT IPV6_2292HOPLIMIT
# endif
#endif

#ifdef BRCM_CMS_BUILD
#include "cms_msg.h"
#include "cms_util.h"
#include "cms_log.h"
#endif

enum {
	DEFDATALEN = 56,
	MAXIPLEN = 60,
	MAXICMPLEN = 76,
	MAX_DUP_CHK = (8 * 128),
	MAXWAIT = 10,
	PINGINTERVAL = 1, /* 1 second */
};

/* Common routines */

// brcm begin
#ifdef BRCM_CMS_BUILD

#define PING_IN_PROGRESS  0
#define PING_FINISHED      1
#define PING_ERROR        2  /* ping process exits on error */
#define PING_UNKNOWN_HOST 3  /* ping unable to resolve host name */

#ifdef BRCM_OMCI
static UINT16 tcID = 0;
static OmciPingDataMsgBody omciMsg;
#endif
static void *msgHandle=NULL;
static CmsEntityId requesterId=0;
#endif  /* BRCM_CMS_BUILD */
// brcm end
#if 1 //__MSTC__, Ailsa, waittime
UBOOL8 isWaitInterval = FALSE;
static int waitingTime = 1000;
static int icount = 0;
#endif
#if 1 //__MSTC__, Ailsa, support dscp
static int dscp = 0;
#endif


static int in_cksum(unsigned short *buf, int sz)
{
	int nleft = sz;
	int sum = 0;
	unsigned short *w = buf;
	unsigned short ans = 0;

	while (nleft > 1) {
		sum += *w++;
		nleft -= 2;
	}

	if (nleft == 1) {
		*(unsigned char *) (&ans) = *(unsigned char *) w;
		sum += ans;
	}

	sum = (sum >> 16) + (sum & 0xFFFF);
	sum += (sum >> 16);
	ans = ~sum;
	return ans;
}

#if !ENABLE_FEATURE_FANCY_PING

/* Simple version */

struct globals {
	char *hostname;
	char packet[DEFDATALEN + MAXIPLEN + MAXICMPLEN];
} FIX_ALIASING;
#define G (*(struct globals*)&bb_common_bufsiz1)
#define INIT_G() do { } while (0)

static void noresp(int ign UNUSED_PARAM)
{
	printf("No response from %s\n", G.hostname);
	exit(EXIT_FAILURE);
}

static void ping4(len_and_sockaddr *lsa)
{
	struct icmp *pkt;
	int pingsock, c;

	pingsock = create_icmp_socket();

	pkt = (struct icmp *) G.packet;
	memset(pkt, 0, sizeof(G.packet));
	pkt->icmp_type = ICMP_ECHO;
	pkt->icmp_cksum = in_cksum((unsigned short *) pkt, sizeof(G.packet));

	xsendto(pingsock, G.packet, DEFDATALEN + ICMP_MINLEN, &lsa->u.sa, lsa->len);

	/* listen for replies */
	while (1) {
		struct sockaddr_in from;
		socklen_t fromlen = sizeof(from);

		c = recvfrom(pingsock, G.packet, sizeof(G.packet), 0,
				(struct sockaddr *) &from, &fromlen);
		if (c < 0) {
			if (errno != EINTR)
				bb_perror_msg("recvfrom");
			continue;
		}
		if (c >= 76) {			/* ip + icmp */
			struct iphdr *iphdr = (struct iphdr *) G.packet;

			pkt = (struct icmp *) (G.packet + (iphdr->ihl << 2));	/* skip ip hdr */
			if (pkt->icmp_type == ICMP_ECHOREPLY)
				break;
		}
	}
	if (ENABLE_FEATURE_CLEAN_UP)
		close(pingsock);
}

#if ENABLE_PING6
static void ping6(len_and_sockaddr *lsa)
{
	struct icmp6_hdr *pkt;
	int pingsock, c;
	int sockopt;

	pingsock = create_icmp6_socket();

	pkt = (struct icmp6_hdr *) G.packet;
	memset(pkt, 0, sizeof(G.packet));
	pkt->icmp6_type = ICMP6_ECHO_REQUEST;

	sockopt = offsetof(struct icmp6_hdr, icmp6_cksum);
	setsockopt(pingsock, SOL_RAW, IPV6_CHECKSUM, &sockopt, sizeof(sockopt));

	xsendto(pingsock, G.packet, DEFDATALEN + sizeof(struct icmp6_hdr), &lsa->u.sa, lsa->len);

	/* listen for replies */
	while (1) {
		struct sockaddr_in6 from;
		socklen_t fromlen = sizeof(from);

		c = recvfrom(pingsock, G.packet, sizeof(G.packet), 0,
				(struct sockaddr *) &from, &fromlen);
		if (c < 0) {
			if (errno != EINTR)
				bb_perror_msg("recvfrom");
			continue;
		}
		if (c >= ICMP_MINLEN) {			/* icmp6_hdr */
			pkt = (struct icmp6_hdr *) G.packet;
			if (pkt->icmp6_type == ICMP6_ECHO_REPLY)
				break;
		}
	}
	if (ENABLE_FEATURE_CLEAN_UP)
		close(pingsock);
}
#endif

#if !ENABLE_PING6
# define common_ping_main(af, argv) common_ping_main(argv)
#endif
static int common_ping_main(sa_family_t af, char **argv)
{
	len_and_sockaddr *lsa;

	INIT_G();

#if ENABLE_PING6
	while ((++argv)[0] && argv[0][0] == '-') {
		if (argv[0][1] == '4') {
			af = AF_INET;
			continue;
		}
		if (argv[0][1] == '6') {
			af = AF_INET6;
			continue;
		}
		bb_show_usage();
	}
#else
	argv++;
#endif

	G.hostname = *argv;
	if (!G.hostname)
		bb_show_usage();

#if ENABLE_PING6
	lsa = xhost_and_af2sockaddr(G.hostname, 0, af);
#else
	lsa = xhost_and_af2sockaddr(G.hostname, 0, AF_INET);
#endif
	/* Set timer _after_ DNS resolution */
	signal(SIGALRM, noresp);
	alarm(5); /* give the host 5000ms to respond */

#if ENABLE_PING6
	if (lsa->u.sa.sa_family == AF_INET6)
		ping6(lsa);
	else
#endif
		ping4(lsa);
	printf("%s is alive!\n", G.hostname);
	return EXIT_SUCCESS;
}


#else /* FEATURE_FANCY_PING */


/* Full(er) version */

#define OPT_STRING ("qvmc:s:w:W:I:t:i:4" IF_PING6("6")"b:d:")
enum {
	OPT_QUIET = 1 << 0,
	OPT_VERBOSE = 1 << 1,
	OPT_m = 1 << 2,
	OPT_c = 1 << 3,
	OPT_s = 1 << 4,
	OPT_w = 1 << 5,
	OPT_W = 1 << 6,
	OPT_I = 1 << 7,
	OPT_t = 1 << 8,
	OPT_i = 1 << 9,
	OPT_IPV4 = 1 << 10,
	OPT_IPV6 = (1 << 11) * ENABLE_PING6,
	OPT_b = 1 << 12,
	OPT_d = 1 << 13,
};


struct globals {
	int pingsock;
	int if_index;
	char *str_I;
	char *str_t;  // brcm
	char *str_i;  // brcm
	len_and_sockaddr *source_lsa;
	unsigned datalen;
	unsigned pingcount; /* must be int-sized */
	unsigned long ntransmitted, nreceived, nrepeats;
	uint16_t myid;
	unsigned tmin, tmax; /* in us */
	unsigned long long tsum; /* in us, sum of all times */
	unsigned deadline;
	unsigned timeout;
	unsigned total_secs;
	unsigned sizeof_rcv_packet;
	char *rcv_packet; /* [datalen + MAXIPLEN + MAXICMPLEN] */
	void *snd_packet; /* [datalen + ipv4/ipv6_const] */
	const char *hostname;
	const char *dotted;
	union {
		struct sockaddr sa;
		struct sockaddr_in sin;
#if ENABLE_PING6
		struct sockaddr_in6 sin6;
#endif
	} pingaddr;
	char rcvd_tbl[MAX_DUP_CHK / 8];
} FIX_ALIASING;
#define G (*(struct globals*)&bb_common_bufsiz1)
#define pingsock     (G.pingsock    )
#define if_index     (G.if_index    )
#define source_lsa   (G.source_lsa  )
#define str_I        (G.str_I       )
#define str_t        (G.str_t       )  // brcm
#define str_i        (G.str_i       )  // brcm
#define datalen      (G.datalen     )
#define ntransmitted (G.ntransmitted)
#define nreceived    (G.nreceived   )
#define nrepeats     (G.nrepeats    )
#define pingcount    (G.pingcount   )
#define myid         (G.myid        )
#define tmin         (G.tmin        )
#define tmax         (G.tmax        )
#define tsum         (G.tsum        )
#define deadline     (G.deadline    )
#define timeout      (G.timeout     )
#define total_secs   (G.total_secs  )
#define hostname     (G.hostname    )
#define dotted       (G.dotted      )
#define pingaddr     (G.pingaddr    )
#define rcvd_tbl     (G.rcvd_tbl    )
void BUG_ping_globals_too_big(void);
// brcm: changed default value of pingcount from 0 to 4.
#define INIT_G() do { \
	if (sizeof(G) > COMMON_BUFSIZE) \
		BUG_ping_globals_too_big(); \
	pingsock = -1; \
	datalen = DEFDATALEN; \
	timeout = MAXWAIT; \
	tmin = UINT_MAX; \
	pingcount = 4; \
} while (0)


#define	A(bit)		rcvd_tbl[(bit)>>3]	/* identify byte in array */
#define	B(bit)		(1 << ((bit) & 0x07))	/* identify bit in byte */
#define	SET(bit)	(A(bit) |= B(bit))
#define	CLR(bit)	(A(bit) &= (~B(bit)))
#define	TST(bit)	(A(bit) & B(bit))

/**************************************************************************/

// brcm begin
#ifdef BRCM_CMS_BUILD
/* this is call to send message back to SMD to relay to interested party about the
 * statistic of the most recent completed or stopped PING test */
static void sendEventMessage(int finish, const char *host)
{
   char buf[sizeof(CmsMsgHeader) + sizeof(PingDataMsgBody)]={0};
   CmsMsgHeader *msg=(CmsMsgHeader *) buf;
   PingDataMsgBody *pingBody = (PingDataMsgBody*) (msg+1);
   CmsRet ret;

   cmsLog_debug("finish %d, ip %s, nTransmited/receive/tmin/tmax/tsum %d/%d/%d/%d/%d",
                finish,host,ntransmitted,nreceived,tmin,tmax,tsum);

   msg->type = CMS_MSG_PING_STATE_CHANGED;
   msg->src = EID_PING;
   msg->dst = EID_SSK;
   msg->flags_event = 1;
   msg->dataLength = sizeof(PingDataMsgBody);

   if (finish == PING_FINISHED)
   {
      sprintf(pingBody->diagnosticsState,MDMVS_COMPLETE); 
   }
   else if (finish == PING_IN_PROGRESS)
   {
      sprintf(pingBody->diagnosticsState,MDMVS_PING_INPROGRESS); 
   }
   #if 1//__MSTC__, Ailsa
   else if (finish == PING_ERROR || finish == PING_UNKNOWN_HOST)
   #else
   else if (finish == PING_ERROR)
   #endif
   {
      /* none, no status to report because ping process has error and die */
      sprintf(pingBody->diagnosticsState,MDMVS_ERROR_CANNOTRESOLVEHOSTNAME); 
   }

   //   sprintf(pingBody->host,ip); 
   sprintf(pingBody->host,host); 
#if 1 //__MSTC__, Ailsa, waittime,dscp
   pingBody->numberOfRepetitions = pingcount;
   pingBody->pingTimeout = waitingTime;
   pingBody->dataBlockSize = datalen;
   pingBody->DSCP = dscp>>2;
#endif
   pingBody->requesterId = requesterId;
   pingBody->successCount = nreceived;
   pingBody->failureCount = (ntransmitted-nreceived);
   if (nreceived)
   {
      /*
       * tsum,tmin,tmax are in us, but data model expects value in ms, so
       * divide by 1000, with a round up to nearest ms.
       */
#ifndef CONFIG_FEATURE_FANCY_PING
      pingBody->averageResponseTime = tsum / (nreceived + nrepeats);
#else
      pingBody->averageResponseTime = tsum / nreceived;
#endif
      pingBody->averageResponseTime = (pingBody->averageResponseTime+500)/1000;
      pingBody->minimumResponseTime = (tmin+500) / 1000;
      pingBody->maximumResponseTime = (tmax+500) / 1000;
   }
   else 
   {
      pingBody->averageResponseTime = 0;
      pingBody->minimumResponseTime = 0;
      pingBody->maximumResponseTime = 0;
   }

   if ((ret = cmsMsg_send(msgHandle, msg)) != CMSRET_SUCCESS)
   {
      cmsLog_error("could not send out CMS_MSG_PING_STATE_CHANGED to SSK, ret=%d", ret);
   }
   else
   {
      cmsLog_notice("sent out CMS_MSG_PING_STATE_CHANGED (finish=%d) to SSK", finish);
   }
   if (requesterId != 0)
   {
      msg->dst = requesterId;
      if ((ret = cmsMsg_send(msgHandle, msg)) != CMSRET_SUCCESS)
      {
         cmsLog_error("could not send out CMS_MSG_PING_STATE_CHANGED to requestId %d, ret=%d", ret,(int)requesterId);
      }
      else
      {
         cmsLog_notice("sent out CMS_MSG_PING_STATE_CHANGED (finish=%d) to requesterId %d", finish,(int)requesterId);
      }   
   }
   return;
}

#ifdef BRCM_OMCI
static void sendOmciEventMessage(int finish)
{
   char buf[sizeof(CmsMsgHeader) + sizeof(OmciPingDataMsgBody)]={0};
   CmsMsgHeader *msg=(CmsMsgHeader *) buf;
   OmciPingDataMsgBody *pingBody = (OmciPingDataMsgBody*) (msg+1);
   CmsRet ret;

   if (finish == PING_IN_PROGRESS)
      return;

   cmsLog_debug("finish %d", finish);

   msg->type = CMS_MSG_PING_STATE_CHANGED;
   msg->src = EID_PING;
   msg->dst = EID_OMCIPMD;
   msg->flags_event = 1;
   msg->dataLength = sizeof(OmciPingDataMsgBody);

   omciMsg.tcid = tcID;
   if (finish == PING_FINISHED)
   {
      omciMsg.result = CMSRET_SUCCESS;
   }
   else 
   {
      /* none, no status to report because ping process has error and die */
      omciMsg.result = CMSRET_INTERNAL_ERROR;
   }
   memcpy(pingBody, &omciMsg, sizeof(omciMsg));

   if ((ret = cmsMsg_send(msgHandle, msg)) != CMSRET_SUCCESS)
   {
      cmsLog_error("could not send out CMS_MSG_PING_STATE_CHANGED to OMCIPMD, ret=%d", ret);
   }
   else
   {
      cmsLog_notice("sent out CMS_MSG_PING_STATE_CHANGED (finish=%d) to OMCIPMD", finish);
   }
}
#endif

static void cmsCleanup(void)
{
   if (option_mask32 & OPT_m)
   {
      cmsMsg_cleanup(&msgHandle);
   }
   cmsLog_cleanup();
}

static void logStat(int finish)
{
   /* 
    * Only call sendEventMessage if msgHandle to smd was successfully initialized.
    */
   if (msgHandle != NULL)
   {
#ifdef BRCM_OMCI
      if (option_mask32 & OPT_i)
         sendOmciEventMessage(finish);
      else
#endif
         sendEventMessage(finish,hostname);
   }
}
#endif  /* BRCM_CMS_BUILD */
// brcm end

static void print_stats_and_exit(int junk) NORETURN;
static void print_stats_and_exit(int junk UNUSED_PARAM)
{
	signal(SIGINT, SIG_IGN);

	printf("\n--- %s ping statistics ---\n", hostname);
	printf("%lu packets transmitted, ", ntransmitted);
	printf("%lu packets received, ", nreceived);
	if (nrepeats)
		printf("%lu duplicates, ", nrepeats);
	if (ntransmitted)
		//ntransmitted = (ntransmitted - nreceived) * 100 / ntransmitted;
	printf("%lu%% packet loss\n", (ntransmitted - nreceived) * 100 / ntransmitted);
	if (tmin != UINT_MAX) {
		unsigned tavg = tsum / (nreceived + nrepeats);
		printf("round-trip min/avg/max = %u.%03u/%u.%03u/%u.%03u ms\n",
			tmin / 1000, tmin % 1000,
			tavg / 1000, tavg % 1000,
			tmax / 1000, tmax % 1000);
	}


        logStat(PING_FINISHED);
#ifdef BRCM_CMS_BUILD
        cmsCleanup();
#endif

	/* if condition is true, exit with 1 -- 'failure' */
	exit(nreceived == 0 || (deadline && nreceived < pingcount));
}

static void sendping_tail(void (*sp)(int), const void *pkt, int size_pkt)
{
	int sz;

	CLR((uint16_t)ntransmitted % MAX_DUP_CHK);
	ntransmitted++;

	/* sizeof(pingaddr) can be larger than real sa size, but I think
	 * it doesn't matter */
//BRCM: fix network unreachable, ping exit in xsendto
#if 0
	sz = xsendto(pingsock, pkt, size_pkt, &pingaddr.sa, sizeof(pingaddr));
#else
    //copy from xsendto
	sz = sendto(pingsock, pkt, size_pkt, 0, &pingaddr.sa, sizeof(pingaddr));
	if (sz < 0) {
#if defined(BRCM_CMS_BUILD)
#ifdef BRCM_OMCI
        if (option_mask32 & OPT_i)
        {
            //clear icmpReply, omcipmd sends out test timeout
            memset(omciMsg.msg.icmpReply, 0, sizeof(omciMsg.msg.icmpReply));
        }
#endif
        logStat(PING_ERROR);
        cmsCleanup();
#endif
		if (ENABLE_FEATURE_CLEAN_UP)
			close(pingsock);
		bb_perror_msg_and_die("sendto");
	}

#endif
	if (sz != size_pkt) {
#if defined(BRCM_CMS_BUILD)
#ifdef BRCM_OMCI
        if (option_mask32 & OPT_i)
        {
            //clear icmpReply, omcipmd sends out test timeout
            memset(omciMsg.msg.icmpReply, 0, sizeof(omciMsg.msg.icmpReply));
        }
#endif
        logStat(PING_ERROR);
        cmsCleanup();
#endif
		bb_error_msg_and_die(bb_msg_write_error);
    }
#if 0
	if (pingcount == 0 || deadline || ntransmitted < pingcount) {
		/* Didn't send all pings yet - schedule next in 1s */
		signal(SIGALRM, sp);
		if (deadline) {
			total_secs += PINGINTERVAL;
			if (total_secs >= deadline)
				signal(SIGALRM, print_stats_and_exit);
		}
		alarm(PINGINTERVAL);
	} else { /* -c NN, and all NN are sent (and no deadline) */
		/* Wait for the last ping to come back.
		 * -W timeout: wait for a response in seconds.
		 * Affects only timeout in absense of any responses,
		 * otherwise ping waits for two RTTs. */
		unsigned expire = timeout;

		if (nreceived) {
			/* approx. 2*tmax, in seconds (2 RTT) */
			expire = tmax / (512*1024);
			if (expire == 0)
				expire = 1;
		}
		signal(SIGALRM, print_stats_and_exit);
		alarm(expire);
	}
	#endif
}

static void sendping4(int junk UNUSED_PARAM)
{
	struct icmp *pkt = G.snd_packet;

	//memset(pkt, 0, datalen + ICMP_MINLEN + 4); - G.snd_packet was xzalloced
	pkt->icmp_type = ICMP_ECHO;
	/*pkt->icmp_code = 0;*/
	pkt->icmp_cksum = 0; /* cksum is calculated with this field set to 0 */
	pkt->icmp_seq = htons(ntransmitted); /* don't ++ here, it can be a macro */
	pkt->icmp_id = myid;

	/* If datalen < 4, we store timestamp _past_ the packet,
	 * but it's ok - we allocated 4 extra bytes in xzalloc() just in case.
	 */
	/*if (datalen >= 4)*/
		/* No hton: we'll read it back on the same machine */
		*(uint32_t*)&pkt->icmp_dun = monotonic_us();

	pkt->icmp_cksum = in_cksum((unsigned short *) pkt, datalen + ICMP_MINLEN);

	sendping_tail(sendping4, pkt, datalen + ICMP_MINLEN);
}
#if ENABLE_PING6
static void sendping6(int junk UNUSED_PARAM)
{
	struct icmp6_hdr *pkt = G.snd_packet;

	//memset(pkt, 0, datalen + sizeof(struct icmp6_hdr) + 4);
	pkt->icmp6_type = ICMP6_ECHO_REQUEST;
	/*pkt->icmp6_code = 0;*/
	/*pkt->icmp6_cksum = 0;*/
	pkt->icmp6_seq = htons(ntransmitted); /* don't ++ here, it can be a macro */
	pkt->icmp6_id = myid;

	/*if (datalen >= 4)*/
		*(uint32_t*)(&pkt->icmp6_data8[4]) = monotonic_us();

	//TODO? pkt->icmp_cksum = in_cksum(...);

	sendping_tail(sendping6, pkt, datalen + sizeof(struct icmp6_hdr));
}
#endif

static const char *icmp_type_name(int id)
{
	switch (id) {
	case ICMP_ECHOREPLY:      return "Echo Reply";
	case ICMP_DEST_UNREACH:   return "Destination Unreachable";
	case ICMP_SOURCE_QUENCH:  return "Source Quench";
	case ICMP_REDIRECT:       return "Redirect (change route)";
	case ICMP_ECHO:           return "Echo Request";
	case ICMP_TIME_EXCEEDED:  return "Time Exceeded";
	case ICMP_PARAMETERPROB:  return "Parameter Problem";
	case ICMP_TIMESTAMP:      return "Timestamp Request";
	case ICMP_TIMESTAMPREPLY: return "Timestamp Reply";
	case ICMP_INFO_REQUEST:   return "Information Request";
	case ICMP_INFO_REPLY:     return "Information Reply";
	case ICMP_ADDRESS:        return "Address Mask Request";
	case ICMP_ADDRESSREPLY:   return "Address Mask Reply";
	default:                  return "unknown ICMP type";
	}
}
#if ENABLE_PING6
/* RFC3542 changed some definitions from RFC2292 for no good reason, whee!
 * the newer 3542 uses a MLD_ prefix where as 2292 uses ICMP6_ prefix */
#ifndef MLD_LISTENER_QUERY
# define MLD_LISTENER_QUERY ICMP6_MEMBERSHIP_QUERY
#endif
#ifndef MLD_LISTENER_REPORT
# define MLD_LISTENER_REPORT ICMP6_MEMBERSHIP_REPORT
#endif
#ifndef MLD_LISTENER_REDUCTION
# define MLD_LISTENER_REDUCTION ICMP6_MEMBERSHIP_REDUCTION
#endif
static const char *icmp6_type_name(int id)
{
	switch (id) {
	case ICMP6_DST_UNREACH:      return "Destination Unreachable";
	case ICMP6_PACKET_TOO_BIG:   return "Packet too big";
	case ICMP6_TIME_EXCEEDED:    return "Time Exceeded";
	case ICMP6_PARAM_PROB:       return "Parameter Problem";
	case ICMP6_ECHO_REPLY:       return "Echo Reply";
	case ICMP6_ECHO_REQUEST:     return "Echo Request";
	case MLD_LISTENER_QUERY:     return "Listener Query";
	case MLD_LISTENER_REPORT:    return "Listener Report";
	case MLD_LISTENER_REDUCTION: return "Listener Reduction";
	default:                     return "unknown ICMP type";
	}
}
#endif

static void unpack_tail(int sz, uint32_t *tp, uint32_t *seq UNUSED_PARAM,
		const char *from_str,
		uint16_t recv_seq, int ttl)
{
	const char *dupmsg = " (DUP!)";
	unsigned triptime = triptime; /* for gcc */

	++nreceived;

	if (tp) {
		/* (int32_t) cast is for hypothetical 64-bit unsigned */
		/* (doesn't hurt 32-bit real-world anyway) */
		triptime = (int32_t) ((uint32_t)monotonic_us() - *tp);
		tsum += triptime;
		if (triptime < tmin)
			tmin = triptime;
		if (triptime > tmax)
			tmax = triptime;
	}

	if (TST(recv_seq % MAX_DUP_CHK)) {
		++nrepeats;
		--nreceived;
	} else {
		SET(recv_seq % MAX_DUP_CHK);
		dupmsg += 7;
#if 1 //__MSTC__, Ailsa, waittime, 20110603
			icount++;
			// if the response time of less than one second, we have wait for the next send.
			if((triptime/1000) < (PINGINTERVAL*1000)) 
				isWaitInterval = TRUE;
#endif
	}

// brcm begin
#ifdef BRCM_CMS_BUILD
#ifdef BRCM_OMCI
   if (msgHandle != NULL)
   {
       if (recv_seq < pingcount) {
           omciMsg.msg.responseTime[recv_seq] = (triptime < 1000) ? 1 : (triptime/1000);
       }
   }
#endif
       logStat(PING_IN_PROGRESS);
#endif /* endif */
// brcm end

	if (option_mask32 & OPT_QUIET)
		return;

	printf("%d bytes from %s: seq=%u ttl=%d", sz,
		from_str, recv_seq, ttl);
	if (tp)
		printf(" time=%u.%03u ms", triptime / 1000, triptime % 1000);
	puts(dupmsg);
	fflush_all();
}
static void unpack4(char *buf, int sz, struct sockaddr_in *from)
{
	struct icmp *icmppkt;
	struct iphdr *iphdr;
	int hlen;

	/* discard if too short */
	if (sz < (datalen + ICMP_MINLEN))
		return;

	/* check IP header */
	iphdr = (struct iphdr *) buf;
	hlen = iphdr->ihl << 2;
	sz -= hlen;
	icmppkt = (struct icmp *) (buf + hlen);
	if (icmppkt->icmp_id != myid)
    {
#if defined(BRCM_CMS_BUILD) && defined(BRCM_OMCI)
	    if (icmppkt->icmp_type == ICMP_DEST_UNREACH || 
            icmppkt->icmp_type == ICMP_SOURCE_QUENCH ||
            icmppkt->icmp_type == ICMP_TIME_EXCEEDED)
        {
            struct iphdr *iphdr_in;
    	    struct icmp *icmppkt_in;
            int hlen_in;

            //icmp header len is 4, and 4 bytes unsed
            //then ori ip header and icmp header
            iphdr_in = (struct iphdr *) (buf + hlen + 8);
	        hlen_in = iphdr_in->ihl << 2;
            //now sz = sz - hlen, sz must >= 4(outter icmp header) + 4(unused of icmp) 
            // + hlen_in + 6(inner icmp header)
            if (sz < (4 + 4 + hlen_in + 6))
                return;

            icmppkt_in = (struct icmp *) (buf + hlen + 4 + 4 + hlen_in);
    	    if (icmppkt_in->icmp_id != myid)
                return;

            if (option_mask32 & OPT_i)
            {
                memcpy(omciMsg.msg.icmpReply, &buf[hlen], sizeof(omciMsg.msg.icmpReply));
                logStat(PING_ERROR);
                cmsCleanup();
                bb_error_msg_and_die("Warning: Got ICMP %d (%s)",
                icmppkt->icmp_type, icmp_type_name (icmppkt->icmp_type));
            }

        }
        else
#endif
		    return;				/* not our ping */
    }

#if 1 //__MSTC__, Ailsa,waittime, 20110603
		if(icmppkt->icmp_seq != icount)
			return;
#endif

	if (icmppkt->icmp_type == ICMP_ECHOREPLY) {
		uint16_t recv_seq = ntohs(icmppkt->icmp_seq);
		uint32_t *tp = NULL;

		if (sz >= ICMP_MINLEN + sizeof(uint32_t))
			tp = (uint32_t *) icmppkt->icmp_data;

		unpack_tail(sz, tp, (uint32_t *)&icmppkt->icmp_seq,
			inet_ntoa(*(struct in_addr *) &from->sin_addr.s_addr),
			recv_seq, iphdr->ttl);
	} else if (icmppkt->icmp_type != ICMP_ECHO) {
#if defined(BRCM_CMS_BUILD) && defined(BRCM_OMCI)
                if (option_mask32 & OPT_i)
                {
                   memcpy(omciMsg.msg.icmpReply, &buf[hlen], sizeof(omciMsg.msg.icmpReply));
                   logStat(PING_ERROR);
                   cmsCleanup();
                   bb_error_msg_and_die("Warning: Got ICMP %d (%s)",
                   icmppkt->icmp_type, icmp_type_name (icmppkt->icmp_type));
                }
                else
#endif
		   bb_error_msg("warning: got ICMP %d (%s)",
				icmppkt->icmp_type,
				icmp_type_name(icmppkt->icmp_type));
	}
}
#if ENABLE_PING6
static void unpack6(char *packet, int sz, /*struct sockaddr_in6 *from,*/ int hoplimit)
{
	struct icmp6_hdr *icmppkt;
	char buf[INET6_ADDRSTRLEN];

	/* discard if too short */
	if (sz < (datalen + sizeof(struct icmp6_hdr)))
		return;

	icmppkt = (struct icmp6_hdr *) packet;
	if (icmppkt->icmp6_id != myid)
		return;				/* not our ping */

#if 1 //__MSTC__, DingRuei, follow IPv4 waitting ping reply time
	if(icmppkt->icmp6_seq != icount)
		return;
#endif

	if (icmppkt->icmp6_type == ICMP6_ECHO_REPLY) {
		uint16_t recv_seq = ntohs(icmppkt->icmp6_seq);
		uint32_t *tp = NULL;

		if (sz >= sizeof(struct icmp6_hdr) + sizeof(uint32_t))
			tp = (uint32_t *) &icmppkt->icmp6_data8[4];
		unpack_tail(sz, tp, NULL, 
			inet_ntop(AF_INET6, &pingaddr.sin6.sin6_addr,
					buf, sizeof(buf)),
			recv_seq, hoplimit);
	} else if (icmppkt->icmp6_type != ICMP6_ECHO_REQUEST) {
		bb_error_msg("warning: got ICMP %d (%s)",
				icmppkt->icmp6_type,
				icmp6_type_name(icmppkt->icmp6_type));
	}
}
#endif

static void ping4(len_and_sockaddr *lsa)
{
	int sockopt;

	pingsock = create_icmp_socket();
	pingaddr.sin = lsa->u.sin;
	if (source_lsa) {
		if (setsockopt(pingsock, IPPROTO_IP, IP_MULTICAST_IF,
				&source_lsa->u.sa, source_lsa->len))
			bb_error_msg_and_die("can't set multicast source interface");
		xbind(pingsock, &source_lsa->u.sa, source_lsa->len);
	}
	if (str_I)
		setsockopt_bindtodevice(pingsock, str_I);

	/* enable broadcast pings */
	setsockopt_broadcast(pingsock);

	/* set recv buf (needed if we can get lots of responses: flood ping,
	 * broadcast ping etc) */
	sockopt = (datalen * 2) + 7 * 1024; /* giving it a bit of extra room */
	setsockopt(pingsock, SOL_SOCKET, SO_RCVBUF, &sockopt, sizeof(sockopt));
#if 1 //__MSTC__, Ailsa, waittime
	struct timeval tval;
	tval.tv_sec = waitingTime/1000;
	tval.tv_usec = (waitingTime%1000) * 1000;
	setsockopt(pingsock, SOL_SOCKET, SO_RCVTIMEO,(char *)&tval, sizeof(tval));
#endif
#if 1 //__MSTC__, Ailsa, support dscp
		sockopt = dscp;
		setsockopt(pingsock, IPPROTO_IP, IP_TOS, (char *) &sockopt,
				   sizeof(sockopt));
#endif

	signal(SIGINT, print_stats_and_exit);

	/* start the ping's going ... */
	sendping4(0);

	/* listen for replies */
	while (1) {
		struct sockaddr_in from;
		socklen_t fromlen = (socklen_t) sizeof(from);
		int c;
#if 1//__MSTC__, Ailsa
		if(ntransmitted == icount)
				{
					if(isWaitInterval){
						isWaitInterval = FALSE;
						sleep(PINGINTERVAL);
					}
					sendping4(0);
				}
#endif
		c = recvfrom(pingsock, G.rcv_packet, G.sizeof_rcv_packet, 0,
				(struct sockaddr *) &from, &fromlen);
		if (c < 0) {
			if (errno == EINTR)
				bb_perror_msg("recvfrom");
			//continue;
			if (errno == EAGAIN || errno == EWOULDBLOCK)
			{
				printf("Request timed out\n");
				icount++;
				// if the time out of less than one second, we have wait for the next send.
				if (waitingTime/1000==0)
					isWaitInterval = TRUE;
		}
		}else{
		unpack4(G.rcv_packet, c, &from);
			}
		#if 1 //__MSTC__, Ailsa
			if (pingcount > 0 && icount >= pingcount)
		#else
		if (pingcount && nreceived >= pingcount)
		#endif
			break;
	}
}
#if ENABLE_PING6
extern int BUG_bad_offsetof_icmp6_cksum(void);
static void ping6(len_and_sockaddr *lsa)
{
	int sockopt;
	struct msghdr msg;
	struct sockaddr_in6 from;
	struct iovec iov;
	char control_buf[CMSG_SPACE(36)];

	pingsock = create_icmp6_socket();
	pingaddr.sin6 = lsa->u.sin6;
	/* untested whether "-I addr" really works for IPv6: */
	if (source_lsa)
		xbind(pingsock, &source_lsa->u.sa, source_lsa->len);
	if (str_I)
		setsockopt_bindtodevice(pingsock, str_I);

#ifdef ICMP6_FILTER
	{
		struct icmp6_filter filt;
		if (!(option_mask32 & OPT_VERBOSE)) {
			ICMP6_FILTER_SETBLOCKALL(&filt);
			ICMP6_FILTER_SETPASS(ICMP6_ECHO_REPLY, &filt);
		} else {
			ICMP6_FILTER_SETPASSALL(&filt);
		}
		if (setsockopt(pingsock, IPPROTO_ICMPV6, ICMP6_FILTER, &filt,
					   sizeof(filt)) < 0)
			bb_error_msg_and_die("setsockopt(ICMP6_FILTER)");
	}
#endif /*ICMP6_FILTER*/

	/* enable broadcast pings */
	setsockopt_broadcast(pingsock);

	/* set recv buf (needed if we can get lots of responses: flood ping,
	 * broadcast ping etc) */
	sockopt = (datalen * 2) + 7 * 1024; /* giving it a bit of extra room */
	setsockopt(pingsock, SOL_SOCKET, SO_RCVBUF, &sockopt, sizeof(sockopt));

	sockopt = offsetof(struct icmp6_hdr, icmp6_cksum);
	if (offsetof(struct icmp6_hdr, icmp6_cksum) != 2)
		BUG_bad_offsetof_icmp6_cksum();
	setsockopt(pingsock, SOL_RAW, IPV6_CHECKSUM, &sockopt, sizeof(sockopt));

	/* request ttl info to be returned in ancillary data */
	setsockopt(pingsock, SOL_IPV6, IPV6_HOPLIMIT, &const_int_1, sizeof(const_int_1));

	if (if_index)
		pingaddr.sin6.sin6_scope_id = if_index;
#if 1 //__MSTC__, DingRuei, follow IPv4 waitting ping reply time
	struct timeval tval;
	tval.tv_sec = waitingTime/1000;
	tval.tv_usec = (waitingTime%1000) * 1000;
	setsockopt(pingsock, SOL_SOCKET, SO_RCVTIMEO, (char *)&tval, sizeof(tval));
#endif

	signal(SIGINT, print_stats_and_exit);

	/* start the ping's going ... */
	sendping6(0);

	/* listen for replies */
	msg.msg_name = &from;
	msg.msg_namelen = sizeof(from);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = control_buf;
	iov.iov_base = G.rcv_packet;
	iov.iov_len = G.sizeof_rcv_packet;
#if 1 //__MSTC__, DingRuei, follow IPv4 waitting ping reply time
	while (1) {
		int c;
		struct cmsghdr *mp;
		int hoplimit = -1;
		msg.msg_controllen = sizeof(control_buf);

		if(ntransmitted == icount)
		{
			if(isWaitInterval){
				isWaitInterval = FALSE;
				sleep(PINGINTERVAL);
			}
			sendping6(0);
		}

		c = recvmsg(pingsock, &msg, 0);
		if (c < 0) {
			if (errno == EINTR)
				bb_perror_msg("recvfrom");
			if (errno == EAGAIN || errno == EWOULDBLOCK)
			{
				printf("Request timed out\n");
				icount++;
				// if the time out of less than one second, we have wait for the next send.
				if (waitingTime/1000==0)
					isWaitInterval = TRUE;
			}
		}
		else
		{
			for (mp = CMSG_FIRSTHDR(&msg); mp; mp = CMSG_NXTHDR(&msg, mp)) {
				if (mp->cmsg_level == SOL_IPV6
				 && mp->cmsg_type == IPV6_HOPLIMIT
				 /* don't check len - we trust the kernel: */
				 /* && mp->cmsg_len >= CMSG_LEN(sizeof(int)) */
				) {
					/*hoplimit = *(int*)CMSG_DATA(mp); - unaligned access */
					move_from_unaligned_int(hoplimit, CMSG_DATA(mp));
				}
			}
			unpack6(G.rcv_packet, c, /*&from,*/ hoplimit);
		}
		if (pingcount && icount >= pingcount)
			break;
	}
#else
	while (1) {
		int c;
		struct cmsghdr *mp;
		int hoplimit = -1;
		msg.msg_controllen = sizeof(control_buf);

		c = recvmsg(pingsock, &msg, 0);
		if (c < 0) {
			     if (errno != EINTR)
				bb_perror_msg("recvfrom");
			continue;
		}
		for (mp = CMSG_FIRSTHDR(&msg); mp; mp = CMSG_NXTHDR(&msg, mp)) {
			if (mp->cmsg_level == SOL_IPV6
			 && mp->cmsg_type == IPV6_HOPLIMIT
			 /* don't check len - we trust the kernel: */
			 /* && mp->cmsg_len >= CMSG_LEN(sizeof(int)) */
			) {
				/*hoplimit = *(int*)CMSG_DATA(mp); - unaligned access */
				move_from_unaligned_int(hoplimit, CMSG_DATA(mp));
			}
		}
		unpack6(G.rcv_packet, c, /*&from,*/ hoplimit);
		if (pingcount && nreceived >= pingcount)
			break;
	}
#endif
}
#endif

static void ping(len_and_sockaddr *lsa)
{
	printf("PING %s (%s)", hostname, dotted);
	if (source_lsa) {
		printf(" from %s",
			xmalloc_sockaddr2dotted_noport(&source_lsa->u.sa));
	}
	printf(": %d data bytes\n", datalen);

	G.sizeof_rcv_packet = datalen + MAXIPLEN + MAXICMPLEN;
	G.rcv_packet = xzalloc(G.sizeof_rcv_packet);
#if ENABLE_PING6
	if (lsa->u.sa.sa_family == AF_INET6) {
		/* +4 reserves a place for timestamp, which may end up sitting
		 * _after_ packet. Saves one if() - see sendping4/6() */
		G.snd_packet = xzalloc(datalen + sizeof(struct icmp6_hdr) + 4);
		ping6(lsa);
	} else
#endif
	{
		G.snd_packet = xzalloc(datalen + ICMP_MINLEN + 4);
		ping4(lsa);
	}
}

static int common_ping_main(int opt, char **argv)
{
	len_and_sockaddr *lsa;
	char *str_s;
#if 1 //_MTS__,Ailsa, 20110603
	char *waittime_str;
	char *dscp_str;
#endif
	INIT_G();

	/* exactly one argument needed; -v and -q don't mix; -c NUM, -w NUM, -W NUM */
	opt_complementary = "=1:q--v:v--q:c+:w+:W+";
	opt |= getopt32(argv, OPT_STRING, &pingcount, &str_s, &deadline, &timeout, &str_I, &str_t, &str_i,&waittime_str,&dscp_str);
	if (opt & OPT_s)
		datalen = xatou16(str_s); // -s
	if (opt & OPT_I) { // -I
		if_index = if_nametoindex(str_I);
		if (!if_index) {
			/* TODO: I'm not sure it takes IPv6 unless in [XX:XX..] format */
			source_lsa = xdotted2sockaddr(str_I, 0);
			str_I = NULL; /* don't try to bind to device later */
		}
	}
#if 1 //__MSTC__, Ailsa, waittime, 20110603
	if (opt & OPT_b)
		waitingTime = xatou16(waittime_str);
#endif
#if 1 //__MSTC__, Ailsa , support dscp,20110603
	if (opt & OPT_d)
	dscp = xatou16(dscp_str) << 2;
#endif

#ifdef BRCM_CMS_BUILD
	if (option_mask32 & OPT_t)
		requesterId = xatou16(str_t); // -t

#ifdef BRCM_OMCI
	if (option_mask32 & OPT_i)
    {
		tcID = xatou16(str_i); // -i
        memset(&omciMsg, 0, sizeof(OmciPingDataMsgBody));
    }
#endif
    cmsLog_init(EID_PING);
    cmsLog_setLevel(DEFAULT_LOG_LEVEL);
    if (option_mask32 & OPT_m)
        cmsMsg_init(EID_PING, &msgHandle);

    cmsLog_debug("starting ping argv %s",*argv);
#endif

	myid = (uint16_t) getpid();
	hostname = argv[optind];
#if ENABLE_PING6
	{
		sa_family_t af = AF_UNSPEC;
		if (opt & OPT_IPV4)
			af = AF_INET;
		if (opt & OPT_IPV6)
			af = AF_INET6;
		lsa = xhost_and_af2sockaddr(hostname, 0, af);
	}
#else
	lsa = xhost_and_af2sockaddr(hostname, 0, AF_INET);
#endif

	if (source_lsa && source_lsa->u.sa.sa_family != lsa->u.sa.sa_family)
		/* leaking it here... */
		source_lsa = NULL;

	dotted = xmalloc_sockaddr2dotted_noport(&lsa->u.sa);
	ping(lsa);
	print_stats_and_exit(EXIT_SUCCESS);
	/*return EXIT_SUCCESS;*/
}
#endif /* FEATURE_FANCY_PING */


int ping_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int ping_main(int argc UNUSED_PARAM, char **argv)
{
#if !ENABLE_FEATURE_FANCY_PING
	return common_ping_main(AF_UNSPEC, argv);
#else
	return common_ping_main(0, argv);
#endif
}

#if ENABLE_PING6
int ping6_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int ping6_main(int argc UNUSED_PARAM, char **argv)
{
# if !ENABLE_FEATURE_FANCY_PING
	return common_ping_main(AF_INET6, argv);
# else
	return common_ping_main(OPT_IPV6, argv);
# endif
}
#endif

/* from ping6.c:
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Mike Muuss.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. <BSD Advertising Clause omitted per the July 22, 1999 licensing change
 *		ftp://ftp.cs.berkeley.edu/pub/4bsd/README.Impt.License.Change>
 *
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
