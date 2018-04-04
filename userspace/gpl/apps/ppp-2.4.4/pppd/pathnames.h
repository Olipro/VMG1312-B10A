/*
 * define path names
 *
 * $Id: pathnames.h,v 1.18 2005/08/25 23:59:34 paulus Exp $
 */

#ifdef HAVE_PATHS_H
#include <paths.h>

#else /* HAVE_PATHS_H */
#ifndef _PATH_VARRUN
#define _PATH_VARRUN 	"/etc/ppp/"
#endif
#define _PATH_DEVNULL	"/dev/null"
#endif /* HAVE_PATHS_H */

#ifndef _ROOT_PATH
#define _ROOT_PATH
#endif

#define _PATH_UPAPFILE 	 _ROOT_PATH "/var/etc/ppp/pap-secrets"
#define _PATH_CHAPFILE 	 _ROOT_PATH "/var/etc/ppp/chap-secrets"
#define _PATH_SRPFILE 	 _ROOT_PATH "/var/etc/ppp/srp-secrets"
#define _PATH_SYSOPTIONS _ROOT_PATH "/var/ppp/options"
#define _PATH_IPUP	 _ROOT_PATH "/var/etc/ppp/ip-up"
#define _PATH_IPDOWN	 _ROOT_PATH "/var/etc/ppp/ip-down"
#define _PATH_IPPREUP	 _ROOT_PATH "/var/etc/ppp/ip-pre-up"
#define _PATH_AUTHUP	 _ROOT_PATH "/var/etc/ppp/auth-up"
#define _PATH_AUTHDOWN	 _ROOT_PATH "/var/etc/ppp/auth-down"
#define _PATH_TTYOPT	 _ROOT_PATH "/var/etc/ppp/options."
#define _PATH_CONNERRS	 _ROOT_PATH "/var/etc/ppp/connect-errors"
#define _PATH_PEERFILES	 _ROOT_PATH "/var/etc/ppp/peers/"

#ifndef TCSUPPORT_DNSEACHPVC
#define _PATH_RESOLV     _ROOT_PATH "/var/etc/ppp/resolv.conf"
#else
#define _PATH_RESOLV     _ROOT_PATH "/var/etc/ppp/resolv_%s.conf"
#endif

#define _PATH_USEROPT	 ".ppprc"
#define	_PATH_PSEUDONYM	 ".ppp_pseudonym"

#ifdef INET6
#define _PATH_IPV6UP     _ROOT_PATH "/etc/ppp/ipv6-up"
#define _PATH_IPV6DOWN   _ROOT_PATH "/etc/ppp/ipv6-down"
#endif

#ifdef IPX_CHANGE
#define _PATH_IPXUP	 _ROOT_PATH "/etc/ppp/ipx-up"
#define _PATH_IPXDOWN	 _ROOT_PATH "/etc/ppp/ipx-down"
#endif /* IPX_CHANGE */

#ifdef __STDC__
#define _PATH_PPPDB	_ROOT_PATH _PATH_VARRUN "pppd2.tdb"
#else /* __STDC__ */
#ifdef HAVE_PATHS_H
#define _PATH_PPPDB	"/var/run/pppd2.tdb"
#else
#define _PATH_PPPDB	"/etc/ppp/pppd2.tdb"
#endif
#endif /* __STDC__ */

#ifdef PLUGIN
#ifdef __STDC__
#define _PATH_PLUGIN	DESTDIR "/lib/pppd/" VERSION
#else /* __STDC__ */
#define _PATH_PLUGIN	"/usr/lib/pppd"
#endif /* __STDC__ */

#endif /* PLUGIN */
