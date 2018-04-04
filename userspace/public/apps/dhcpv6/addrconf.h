/*	$KAME: addrconf.h,v 1.1 2005/03/02 07:20:13 suz Exp $	*/

/*
 * Copyright (C) 2002 WIDE Project.
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

typedef enum { ADDR6S_ACTIVE, ADDR6S_RENEW, ADDR6S_REBIND} addr6state_t;

#if defined(CLIENT_DECLINE_SUPPORT)
TAILQ_HEAD(statefuladdr_list, statefuladdr);
struct iactl_na {
        struct iactl common;
        struct statefuladdr_list statefuladdr_head;
};
#define iacna_ia common.iactl_ia
#define iacna_callback common.callback
#define iacna_isvalid common.isvalid
#define iacna_duration common.duration
#define iacna_renew_data common.renew_data
#define iacna_rebind_data common.rebind_data
#define iacna_reestablish_data common.reestablish_data
#define iacna_release_data common.release_data
#define iacna_cleanup common.cleanup

struct statefuladdr {
        TAILQ_ENTRY (statefuladdr) link;

        struct dhcp6_statefuladdr addr;
        time_t updatetime;
        struct dhcp6_timer *timer;
        struct iactl_na *ctl;
        struct dhcp6_if *dhcpif;
};
#endif

extern int update_address __P((struct ia *, struct dhcp6_statefuladdr *,
    struct dhcp6_if *, struct iactl **, void (*)__P((struct ia *))));
