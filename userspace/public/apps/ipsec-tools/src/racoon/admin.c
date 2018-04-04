/* $Id: admin.c,v 1.17.2.4 2005/07/12 11:49:44 manubsd Exp $ */
 
/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <sys/un.h>

#include <net/pfkeyv2.h>

#include <netinet/in.h>
#ifndef HAVE_NETINET6_IPSEC
#include <netinet/ipsec.h>
#else 
#include <netinet6/ipsec.h>
#endif


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "var.h"
#include "misc.h"
#include "vmbuf.h"
#include "plog.h"
#include "sockmisc.h"
#include "debug.h"

#include "schedule.h"
#include "localconf.h"
#include "remoteconf.h"
#include "grabmyaddr.h"
#include "isakmp_var.h"
#include "isakmp.h"
#include "oakley.h"
#include "handler.h"
#include "evt.h"
#include "pfkey.h"
#include "ipsec_doi.h"
#include "admin.h"
#include "admin_var.h"
#include "isakmp_inf.h"
#include "session.h"
#include "gcmalloc.h"
#if 1//Verizon,johann
#include "policy.h"
#endif

#ifdef ENABLE_ADMINPORT
char *adminsock_path = ADMINSOCK_PATH;
uid_t adminsock_owner = 0;
gid_t adminsock_group = 0;
mode_t adminsock_mode = 0600;

static struct sockaddr_un sunaddr;
static int admin_process __P((int, char *));
static int admin_reply __P((int, struct admin_com *, vchar_t *));

int
admin_handler()
{
	int so2;
	struct sockaddr_storage from;
	socklen_t fromlen = sizeof(from);
	struct admin_com com;
	char *combuf = NULL;
	pid_t pid = -1;
	int len, error = -1;

	so2 = accept(lcconf->sock_admin, (struct sockaddr *)&from, &fromlen);
	if (so2 < 0) {
		plog(LLV_ERROR, LOCATION, NULL,
			"failed to accept admin command: %s\n",
			strerror(errno));
		return -1;
	}

	/* get buffer length */
	while ((len = recv(so2, (char *)&com, sizeof(com), MSG_PEEK)) < 0) {
		if (errno == EINTR)
			continue;
		plog(LLV_ERROR, LOCATION, NULL,
			"failed to recv admin command: %s\n",
			strerror(errno));
		goto end;
	}

	/* sanity check */
	if (len < sizeof(com)) {
		plog(LLV_ERROR, LOCATION, NULL,
			"invalid header length of admin command\n");
		goto end;
	}

	/* get buffer to receive */
	if ((combuf = racoon_malloc(com.ac_len)) == 0) {
		plog(LLV_ERROR, LOCATION, NULL,
			"failed to alloc buffer for admin command\n");
		goto end;
	}

	/* get real data */
	while ((len = recv(so2, combuf, com.ac_len, 0)) < 0) {
		if (errno == EINTR)
			continue;
		plog(LLV_ERROR, LOCATION, NULL,
			"failed to recv admin command: %s\n",
			strerror(errno));
		goto end;
	}

	if (com.ac_cmd == ADMIN_RELOAD_CONF) {
		/* reload does not work at all! */
		signal_handler(SIGHUP);
		goto end;
	}

	error = admin_process(so2, combuf);

    end:
	(void)close(so2);
	if (combuf)
		racoon_free(combuf);

	/* exit if child's process. */
	if (pid == 0 && !f_foreground)
		exit(error);

	return error;
}

/*
 * main child's process.
 */
static int
admin_process(so2, combuf)
	int so2;
	char *combuf;
{
	struct admin_com *com = (struct admin_com *)combuf;
	vchar_t *buf = NULL;
	vchar_t *id = NULL;
	vchar_t *key = NULL;
	int idtype = 0;
	int error = 0;

	com->ac_errno = 0;

	switch (com->ac_cmd) {
	case ADMIN_RELOAD_CONF:
		/* don't entered because of proccessing it in other place. */
		plog(LLV_ERROR, LOCATION, NULL, "should never reach here\n");
		goto bad;

	case ADMIN_SHOW_SCHED:
	{
		caddr_t p;
		int len;
		if (sched_dump(&p, &len) == -1)
			com->ac_errno = -1;
		buf = vmalloc(len);
		if (buf == NULL)
			com->ac_errno = -1;
		else
			memcpy(buf->v, p, len);
	}
		break;

	case ADMIN_SHOW_EVT:
		/* It's not really an error, don't force racoonctl to quit */
		if ((buf = evt_dump()) == NULL)
			com->ac_errno = 0; 
		break;

	case ADMIN_SHOW_SA:
	case ADMIN_FLUSH_SA:
	    {
		switch (com->ac_proto) {
		case ADMIN_PROTO_ISAKMP:
			switch (com->ac_cmd) {
			case ADMIN_SHOW_SA:
				buf = dumpph1();
				if (buf == NULL)
					com->ac_errno = -1;
				break;
			case ADMIN_FLUSH_SA:
				flushph1();
				break;
			}
			break;
		case ADMIN_PROTO_IPSEC:
		case ADMIN_PROTO_AH:
		case ADMIN_PROTO_ESP:
			switch (com->ac_cmd) {
			case ADMIN_SHOW_SA:
			    {
				u_int p;
				p = admin2pfkey_proto(com->ac_proto);
				if (p == -1)
					goto bad;
				buf = pfkey_dump_sadb(p);
				if (buf == NULL)
					com->ac_errno = -1;
			    }
				break;
			case ADMIN_FLUSH_SA:
				pfkey_flush_sadb(com->ac_proto);
				break;
			}
			break;

		case ADMIN_PROTO_INTERNAL:
			switch (com->ac_cmd) {
			case ADMIN_SHOW_SA:
				buf = NULL; /*XXX dumpph2(&error);*/
				if (buf == NULL)
					com->ac_errno = error;
				break;
			case ADMIN_FLUSH_SA:
				/*XXX flushph2();*/
				com->ac_errno = 0;
				break;
			}
			break;

		default:
			/* ignore */
			com->ac_errno = -1;
		}
	    }
		break;

	case ADMIN_DELETE_SA: {
		struct ph1handle *iph1;
		struct sockaddr *dst;
		struct sockaddr *src;
		char *loc, *rem;

		src = (struct sockaddr *)
			&((struct admin_com_indexes *)
			    ((caddr_t)com + sizeof(*com)))->src;
		dst = (struct sockaddr *)
			&((struct admin_com_indexes *)
			    ((caddr_t)com + sizeof(*com)))->dst;

		if ((loc = strdup(saddrwop2str(src))) == NULL) {
			plog(LLV_ERROR, LOCATION, NULL, 
			    "cannot allocate memory\n");
			break;
		}
		if ((rem = strdup(saddrwop2str(dst))) == NULL) {
			plog(LLV_ERROR, LOCATION, NULL, 
			    "cannot allocate memory\n");
			break;
		}

		if ((iph1 = getph1byaddrwop(src, dst)) == NULL) {
			plog(LLV_ERROR, LOCATION, NULL, 
			    "phase 1 for %s -> %s not found\n", loc, rem);
		} else {
			if (iph1->status == PHASE1ST_ESTABLISHED)
				isakmp_info_send_d1(iph1);
			purge_remote(iph1);
		}

		racoon_free(loc);
		racoon_free(rem);

		break;
	}

	case ADMIN_DELETE_ALL_SA_DST: {
		struct ph1handle *iph1;
		struct sockaddr *dst;
		#if 1//_Verizon,Johann
		struct ph2handle *iph2;
		struct sockaddr *src;
		struct sockaddr *remote;
		struct sockaddr *local;
		#endif

		char *loc, *rem;

		#if 1//_Verizon,Johann
		src = (struct sockaddr *)
			&((struct admin_com_indexes *)
			    ((caddr_t)com + sizeof(*com)))->src;
		#endif

		dst = (struct sockaddr *)
			&((struct admin_com_indexes *)
			    ((caddr_t)com + sizeof(*com)))->dst;
		#if 1//_Verizon,Johann
		remote = dupsaddr(dst);
		local = dupsaddr(src);
		iph2 = getph2bysaddrwop(local,remote);
		if (iph2->status == PHASE2ST_ESTABLISHED) 
		{
			isakmp_info_send_d2(iph2);
		}
		delete_spd(iph2);
		unbindph12(iph2);
		remph2(iph2);
		delph2(iph2);
		#endif


		if ((rem = strdup(saddrwop2str(dst))) == NULL) {
			plog(LLV_ERROR, LOCATION, NULL, 
			    "cannot allocate memory\n");
			break;
		}

		plog(LLV_INFO, LOCATION, NULL, 
		    "Flushing all SAs for peer %s\n", rem);

		while ((iph1 = getph1bydstaddrwop(dst)) != NULL) {
			if ((loc = strdup(saddrwop2str(iph1->local))) == NULL) {
				plog(LLV_ERROR, LOCATION, NULL, 
				    "cannot allocate memory\n");
				break;
			}

			if (iph1->status == PHASE1ST_ESTABLISHED)
				isakmp_info_send_d1(iph1);
			#if 1//_Verizon,Johann
			purge_remote(iph1);
			remph1(iph1);
			delph1(iph1);
			#endif


			racoon_free(loc);
		}
		
		racoon_free(rem);

		break;
	}

	case ADMIN_ESTABLISH_SA_PSK: {
		struct admin_com_psk *acp;
		char *data;

		com->ac_cmd = ADMIN_ESTABLISH_SA;

		acp = (struct admin_com_psk *)
		    ((char *)com + sizeof(*com) + 
		    sizeof(struct admin_com_indexes));

		idtype = acp->id_type;

		if ((id = vmalloc(acp->id_len)) == NULL) {
			plog(LLV_ERROR, LOCATION, NULL,
			    "cannot allocate memory: %s\n", 
			    strerror(errno));
			break;
		}
		data = (char *)(acp + 1);
		memcpy(id->v, data, id->l);

		if ((key = vmalloc(acp->key_len)) == NULL) {
			plog(LLV_ERROR, LOCATION, NULL,
			    "cannot allocate memory: %s\n", 
			    strerror(errno));
			vfree(id);
			break;
		}
		data = (char *)(data + acp->id_len);
		memcpy(key->v, data, key->l);
	}
	/* FALLTHROUGH */
	case ADMIN_ESTABLISH_SA:
	    {
		struct sockaddr *dst;
		struct sockaddr *src;
		#if 1//_Verizon,Johann
		struct sockaddr *dstLocal;
		struct sockaddr *srcLocal;
		unsigned int prefs_local;
		unsigned int prefd_local;
		unsigned int sa_type;
		#endif
		src = (struct sockaddr *)
			&((struct admin_com_indexes *)
			    ((caddr_t)com + sizeof(*com)))->src;
		dst = (struct sockaddr *)
			&((struct admin_com_indexes *)
			    ((caddr_t)com + sizeof(*com)))->dst;
		#if 1//_Verizon,Johann
		srcLocal= (struct sockaddr *)
			&((struct admin_com_indexes *)
			    ((caddr_t)com + sizeof(*com)))->srcLocal;
		dstLocal= (struct sockaddr *)
			&((struct admin_com_indexes *)
			    ((caddr_t)com + sizeof(*com)))->dstLocal;
		prefs_local = ((struct admin_com_indexes *)
			    ((caddr_t)com + sizeof(*com)))->prefs_local;
		prefd_local = ((struct admin_com_indexes *)
			    ((caddr_t)com + sizeof(*com)))->prefd_local;
		sa_type = ((struct admin_com_indexes *)
			    ((caddr_t)com + sizeof(*com)))->saType;
		#endif

		switch (com->ac_proto) {
		case ADMIN_PROTO_ISAKMP:
		    {
			struct remoteconf *rmconf;
			struct sockaddr *remote;
			struct sockaddr *local;

			/* search appropreate configuration */
			rmconf = getrmconf(dst);
			if (rmconf == NULL) {
				plog(LLV_ERROR, LOCATION, NULL,
					"no configuration found "
					"for %s\n", saddrwop2str(dst));
				com->ac_errno = -1;
				break;
			}

			/* get remote IP address and port number. */
			remote = dupsaddr(dst);
			if (remote == NULL) {
				com->ac_errno = -1;
				break;
			}
			switch (remote->sa_family) {
			case AF_INET:
				((struct sockaddr_in *)remote)->sin_port =
					((struct sockaddr_in *)rmconf->remote)->sin_port;
				break;
#ifdef INET6
			case AF_INET6:
				((struct sockaddr_in6 *)remote)->sin6_port =
					((struct sockaddr_in6 *)rmconf->remote)->sin6_port;
				break;
#endif
			default:
				plog(LLV_ERROR, LOCATION, NULL,
					"invalid family: %d\n",
					remote->sa_family);
				com->ac_errno = -1;
				break;
			}

			/* get local address */
			local = dupsaddr(src);
			if (local == NULL) {
				com->ac_errno = -1;
				break;
			}
			switch (local->sa_family) {
			case AF_INET:
				((struct sockaddr_in *)local)->sin_port =
					getmyaddrsport(local);
				break;
#ifdef INET6
			case AF_INET6:
				((struct sockaddr_in6 *)local)->sin6_port =
					getmyaddrsport(local);
				break;
#endif
			default:
				plog(LLV_ERROR, LOCATION, NULL,
					"invalid family: %d\n",
					local->sa_family);
				com->ac_errno = -1;
				break;
			}

			/* Set the id and key */
			if (id && key) {
				if (rmconf->idv != NULL) {
					vfree(rmconf->idv);
					rmconf->idv = NULL;
				}
				if (rmconf->key != NULL) {
					vfree(rmconf->key);
					rmconf->key = NULL;
				}

				rmconf->idvtype = idtype;
				rmconf->idv = id;
				rmconf->key = key;
			}
 
			plog(LLV_INFO, LOCATION, NULL,
				"accept a request to establish IKE-SA: "
				"%s\n", saddrwop2str(remote));
                        #if 0 //_Verizon,Johann
			/* begin ident mode */
			if (isakmp_ph1begin_i(rmconf, remote, local) < 0) {
				com->ac_errno = -1;
				break;
			}
                        #endif
                         #if 1//_Verizon,Johann
			struct ph1handle * iph1;
			struct ph2handle * iph2;

			if ((iph1 = getph1byaddrwop(local, remote)) == NULL) {
				plog(LLV_ERROR, LOCATION, NULL, 
			    "phase 1  not found\n");
			} else {
					if (iph1->status == PHASE1ST_ESTABLISHED)
					{
						plog(LLV_ERROR, LOCATION, NULL, "iph1 is PHASE1ST_ESTABLISHED\n");
						break;
					}
			}
			struct sockaddr * saddr_soc1;
			struct sockaddr * daddr_soc1;
			saddr_soc1 = dupsaddr(srcLocal);
			daddr_soc1 = dupsaddr(dstLocal);
			struct policyindex spidx1;
			spidx1.dir = IPSEC_DIR_OUTBOUND;
			memcpy(&spidx1.src,saddr_soc1,sizeof(spidx1.src));
			memcpy(&spidx1.dst,daddr_soc1,sizeof(spidx1.dst));
			spidx1.ul_proto = 255;
			spidx1.prefs = prefs_local;
			spidx1.prefd = prefd_local;
			struct secpolicy *sp_out = NULL;
			struct secpolicy *sp_in = NULL;
			sp_out = getsp(&spidx1);

			saddr_soc1 = dupsaddr(dstLocal);
			daddr_soc1 = dupsaddr(srcLocal);
			spidx1.dir = IPSEC_DIR_INBOUND;
			memcpy(&spidx1.src,saddr_soc1,sizeof(spidx1.src));
			memcpy(&spidx1.dst,daddr_soc1,sizeof(spidx1.dst));
			sp_in = getsp(&spidx1);
			
			iph2 = newph2();
			if (iph2 == NULL)
				goto bad;
			iph2->dst = dupsaddr(remote);
			iph2->src = dupsaddr(local);

			iph2->spid = sp_out->id;
			iph2->side = INITIATOR;
			iph2->seq = 1;
			iph2->status = PHASE2ST_STATUS2;

			iph2->retry_checkph1 = LC_DEFAULT_RETRY_CHECKPH1;
			iph2->satype = sa_type;

			vchar_t *idsrc, *iddst;

			idsrc = ipsecdoi_sockaddr2id((struct sockaddr *)&sp_out->spidx.src,
								sp_out->spidx.prefs, sp_out->spidx.ul_proto);

			iddst = ipsecdoi_sockaddr2id((struct sockaddr *)&sp_out->spidx.dst,
								sp_out->spidx.prefd, sp_out->spidx.ul_proto);
						
			iph2->sainfo = getsainfo(idsrc, iddst, NULL);

			vfree(idsrc);
			vfree(iddst);

			set_proposal_from_policy(iph2, sp_out, sp_in);
						
			insph2(iph2);
			if (isakmp_post_acquire(iph2) < 0) 
			{
				plog(LLV_ERROR, LOCATION, NULL,"isakmp_post_acquire fail\n");
				com->ac_errno = -1;
				break;
			}	
			else
			{
				plog(LLV_DEBUG, LOCATION, NULL,"isakmp_post_acquire success\n");
				break;
			}	
			#endif
		    }
			break;
		case ADMIN_PROTO_AH:
		case ADMIN_PROTO_ESP:
			break;
		default:
			/* ignore */
			com->ac_errno = -1;
		}
	    }
		break;

	default:
		plog(LLV_ERROR, LOCATION, NULL,
			"invalid command: %d\n", com->ac_cmd);
		com->ac_errno = -1;
	}

	if (admin_reply(so2, com, buf) < 0)
		goto bad;

	if (buf != NULL)
		vfree(buf);

	return 0;

    bad:
	if (buf != NULL)
		vfree(buf);
	return -1;
}

static int
admin_reply(so, combuf, buf)
	int so;
	struct admin_com *combuf;
	vchar_t *buf;
{
	int tlen;
	char *retbuf = NULL;

	if (buf != NULL)
		tlen = sizeof(*combuf) + buf->l;
	else
		tlen = sizeof(*combuf);

	retbuf = racoon_calloc(1, tlen);
	if (retbuf == NULL) {
		plog(LLV_ERROR, LOCATION, NULL,
			"failed to allocate admin buffer\n");
		return -1;
	}

	memcpy(retbuf, combuf, sizeof(*combuf));
	((struct admin_com *)retbuf)->ac_len = tlen;

	if (buf != NULL)
		memcpy(retbuf + sizeof(*combuf), buf->v, buf->l);

	tlen = send(so, retbuf, tlen, 0);
	racoon_free(retbuf);
	if (tlen < 0) {
		plog(LLV_ERROR, LOCATION, NULL,
			"failed to send admin command: %s\n",
			strerror(errno));
		return -1;
	}

	return 0;
}

/* ADMIN_PROTO -> SADB_SATYPE */
int
admin2pfkey_proto(proto)
	u_int proto;
{
	switch (proto) {
	case ADMIN_PROTO_IPSEC:
		return SADB_SATYPE_UNSPEC;
	case ADMIN_PROTO_AH:
		return SADB_SATYPE_AH;
	case ADMIN_PROTO_ESP:
		return SADB_SATYPE_ESP;
	default:
		plog(LLV_ERROR, LOCATION, NULL,
			"unsupported proto for admin: %d\n", proto);
		return -1;
	}
	/*NOTREACHED*/
}

int
admin_init()
{
	#if 1//_Verizon,Johann
	FILE *sockfp = NULL;
	system("mkdir /var/racoon");
	sockfp = fopen("/var/racoon/racoon.sock","w");
	if(sockfp != NULL)
		fclose(sockfp);
	#endif
	
	if (adminsock_path == NULL) {
		lcconf->sock_admin = -1;
		return 0;
	}

	memset(&sunaddr, 0, sizeof(sunaddr));
	sunaddr.sun_family = AF_UNIX;
	snprintf(sunaddr.sun_path, sizeof(sunaddr.sun_path),
		"%s", adminsock_path);

	lcconf->sock_admin = socket(AF_UNIX, SOCK_STREAM, 0);
	if (lcconf->sock_admin == -1) {
		plog(LLV_ERROR, LOCATION, NULL,
			"socket: %s\n", strerror(errno));
		return -1;
	}

	unlink(sunaddr.sun_path);
	if (bind(lcconf->sock_admin, (struct sockaddr *)&sunaddr,
			sizeof(sunaddr)) != 0) {
		plog(LLV_ERROR, LOCATION, NULL,
			"bind(sockname:%s): %s\n",
			sunaddr.sun_path, strerror(errno));
		(void)close(lcconf->sock_admin);
		return -1;
	}

	if (chown(sunaddr.sun_path, adminsock_owner, adminsock_group) != 0) {
		plog(LLV_ERROR, LOCATION, NULL, 
		    "chown(%s, %d, %d): %s\n", 
		    sunaddr.sun_path, adminsock_owner, 
		    adminsock_group, strerror(errno));
		(void)close(lcconf->sock_admin);
		return -1;
	}

	if (chmod(sunaddr.sun_path, adminsock_mode) != 0) {
		plog(LLV_ERROR, LOCATION, NULL, 
		    "chmod(%s, 0%03o): %s\n", 
		    sunaddr.sun_path, adminsock_mode, strerror(errno));
		(void)close(lcconf->sock_admin);
		return -1;
	}

	if (listen(lcconf->sock_admin, 5) != 0) {
		plog(LLV_ERROR, LOCATION, NULL,
			"listen(sockname:%s): %s\n",
			sunaddr.sun_path, strerror(errno));
		(void)close(lcconf->sock_admin);
		return -1;
	}
	plog(LLV_DEBUG, LOCATION, NULL,
		"open %s as racoon management.\n", sunaddr.sun_path);

	return 0;
}

int
admin_close()
{
	close(lcconf->sock_admin);
	return 0;
}
#endif
