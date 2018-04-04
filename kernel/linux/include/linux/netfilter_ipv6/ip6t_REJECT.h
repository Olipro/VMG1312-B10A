#ifndef _IP6T_REJECT_H
#define _IP6T_REJECT_H

enum ip6t_reject_with {
	IP6T_ICMP6_NO_ROUTE,
	IP6T_ICMP6_ADM_PROHIBITED,
	IP6T_ICMP6_NOT_NEIGHBOUR,
	IP6T_ICMP6_ADDR_UNREACH,
	IP6T_ICMP6_PORT_UNREACH,
	IP6T_ICMP6_ECHOREPLY,
#if 1 //__MSTC__, HuanYao Kang, FOR RFC 6204 L14 compliance
				/*
				L-14:  The IPv6 CE router MUST send an ICMP Destination Unreachable
							 message, code 5 (Source address failed ingress/egress policy)
							 for packets forwarded to it that use an address from a prefix
							 that has been deprecated.
				*/
	IP6T_ICMP6_SRC_ADDR_FAILED,
#endif
	IP6T_TCP_RESET
};

struct ip6t_reject_info {
	u_int32_t	with;	/* reject type */
};

#endif /*_IP6T_REJECT_H*/
