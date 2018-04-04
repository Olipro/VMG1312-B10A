/* ebt_ip6
 * 
 * Authors:
 * Kuo-Lang Tseng <kuo-lang.tseng@intel.com>
 * Manohar Castelino <manohar.castelino@intel.com>
 *
 * Summary:
 * This is just a modification of the IPv4 code written by 
 * Bart De Schuymer <bdschuym@pandora.be>
 * with the changes required to support IPv6
 *
 */

/* CPE must not forward the RA to bridge WAN interface*/
#include <errno.h>
#include <inttypes.h>
#include <limits.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <netdb.h>
#include "../include/ebtables_u.h"
#include "../include/linux/netfilter_bridge/ebt_ip6.h"
#include <arpa/inet.h>


#define IP_SOURCE '1'
#define IP_DEST   '2'
#define IP_TCLASS '3'
#define IP_PROTO  '4'
#define IP_SPORT  '5'
#define IP_DPORT  '6'
#define IP_ICMP6  '7'
#if 1 //__MSTC__, Jeff
#define IP_LENGTH '8'
#endif

static const struct option opts[] =
{
	{ "ip6-source"           , required_argument, 0, IP_SOURCE },
	{ "ip6-src"              , required_argument, 0, IP_SOURCE },
	{ "ip6-destination"      , required_argument, 0, IP_DEST   },
	{ "ip6-dst"              , required_argument, 0, IP_DEST   },
	{ "ip6-traffic-class"    , required_argument, 0, IP_TCLASS },
	{ "ip6-tclass"           , required_argument, 0, IP_TCLASS },
	{ "ip6-protocol"         , required_argument, 0, IP_PROTO  },
	{ "ip6-proto"            , required_argument, 0, IP_PROTO  },
	{ "ip6-source-port"      , required_argument, 0, IP_SPORT  },
	{ "ip6-sport"            , required_argument, 0, IP_SPORT  },
	{ "ip6-destination-port" , required_argument, 0, IP_DPORT  },
	{ "ip6-dport"            , required_argument, 0, IP_DPORT  },
	{ "ip6-icmp-type"		 , required_argument, 0, IP_ICMP6  },
#if 1 //__MSTC__, Jeff
	{ "ip6-length"           , required_argument, 0, IP_LENGTH  },
	{ "ip6-len"            	, required_argument, 0, IP_LENGTH  },
#endif
	{ 0 }
};

struct icmpv6_names {
	const char *name;
	u_int8_t type;
	u_int8_t code_min, code_max;
};

static const struct icmpv6_names icmpv6_codes[] = {
	{ "destination-unreachable", 1, 0, 0xFF },
	{ "no-route", 1, 0, 0 },
	{ "communication-prohibited", 1, 1, 1 },
	{ "address-unreachable", 1, 3, 3 },
	{ "port-unreachable", 1, 4, 4 },

	{ "packet-too-big", 2, 0, 0xFF },

	{ "time-exceeded", 3, 0, 0xFF },
	/* Alias */ { "ttl-exceeded", 3, 0, 0xFF },
	{ "ttl-zero-during-transit", 3, 0, 0 },
	{ "ttl-zero-during-reassembly", 3, 1, 1 },

	{ "parameter-problem", 4, 0, 0xFF },
	{ "bad-header", 4, 0, 0 },
	{ "unknown-header-type", 4, 1, 1 },
	{ "unknown-option", 4, 2, 2 },

	{ "echo-request", 128, 0, 0xFF },
	/* Alias */ { "ping", 128, 0, 0xFF },

	{ "echo-reply", 129, 0, 0xFF },
	/* Alias */ { "pong", 129, 0, 0xFF },

	{ "router-solicitation", 133, 0, 0xFF },

	{ "router-advertisement", 134, 0, 0xFF },

	{ "neighbour-solicitation", 135, 0, 0xFF },
	/* Alias */ { "neighbor-solicitation", 135, 0, 0xFF },

	{ "neighbour-advertisement", 136, 0, 0xFF },
	/* Alias */ { "neighbor-advertisement", 136, 0, 0xFF },

	{ "redirect", 137, 0, 0xFF },
};


/* transform a protocol and service name into a port number */
static uint16_t parse_port(const char *protocol, const char *name)
{
	struct servent *service;
	char *end;
	int port;

	port = strtol(name, &end, 10);
	if (*end != '\0') {
		if (protocol && 
		    (service = getservbyname(name, protocol)) != NULL)
			return ntohs(service->s_port);
	}
	else if (port >= 0 || port <= 0xFFFF) {
		return port;
	}
	print_error("Problem with specified %s port '%s'", 
			protocol?protocol:"", name);
	return 0;
}

static void
parse_port_range(const char *protocol, const char *portstring, uint16_t *ports)
{
	char *buffer;
	char *cp;
	
	buffer = strdup(portstring);
	if ((cp = strchr(buffer, ':')) == NULL)
		ports[0] = ports[1] = parse_port(protocol, buffer);
	else {
		*cp = '\0';
		cp++;
		ports[0] = buffer[0] ? parse_port(protocol, buffer) : 0;
//		if (ebt_errormsg[0] != '\0')
//			return;
		ports[1] = cp[0] ? parse_port(protocol, cp) : 0xFFFF;
//		if (ebt_errormsg[0] != '\0')
//			return;
		
		if (ports[0] > ports[1])
			print_error("Invalid portrange (min > max)");
	}
	free(buffer);
}

static char*
parse_num(const char *str, long min, long max, long *num)
{
	char *end;

	errno = 0;
	*num = strtol(str,&end, 10);
	if (errno&&  (*num == LONG_MIN || *num == LONG_MAX)) {
		print_error("Invalid number %s: %s", str, strerror(errno));
		return NULL;
	}
	if (min<= max) {
		if (*num>  max || *num<  min) {
			print_error("Value %ld out of range (%ld, %ld)", *num, min, max);
			return NULL;
		}
	}
	if (*num == 0&&  str == end)
		return NULL;
	return end;
}

static char *
parse_range(const char *str, long min, long max, long num[])
{
	char *next;

	next = parse_num(str, min, max, num);
	if (next == NULL)
		return NULL;
	if (next&&  *next == ':')
		next = parse_num(next+1, min, max,&num[1]);
	else
		num[1] = num[0];
	return next;
}

static int
parse_icmpv6(const char *icmpv6type, uint8_t type[], uint8_t code[])
{
	static const unsigned int limit = ARRAY_SIZE(icmpv6_codes);
	unsigned int match = limit;
	unsigned int i;
	long number[2];

	for (i = 0; i<  limit; i++) {
		if (strncasecmp(icmpv6_codes[i].name, icmpv6type, strlen(icmpv6type)))
			continue;
		if (match != limit)
			print_error("Ambiguous ICMPv6 type `%s':"
					" `%s' or `%s'?",
					icmpv6type, icmpv6_codes[match].name,
					icmpv6_codes[i].name);
		match = i;
	}

	if (match<  limit) {
		type[0] = type[1] = icmpv6_codes[match].type;
		code[0] = icmpv6_codes[match].code_min;
		code[1] = icmpv6_codes[match].code_max;
	} else {
		char *next = parse_range(icmpv6type, 0, 255, number);
		if (!next) {
			print_error("Unknown ICMPv6 type `%s'",
							icmpv6type);
			return -1;
		}
		type[0] = (uint8_t) number[0];
		type[1] = (uint8_t) number[1];
		switch (*next) {
		case 0:
			code[0] = 0;
			code[1] = 255;
			return 0;
		case '/':
			next = parse_range(next+1, 0, 255, number);
			code[0] = (uint8_t) number[0];
			code[1] = (uint8_t) number[1];
			if (next == NULL)
				return -1;
			if (next&&  *next == 0)
				return 0;
		/* fallthrough */
		default:
			print_error("unknown character %c", *next);
			return -1;
		}
	}
	return 0;
}

static void print_port_range(uint16_t *ports)
{
	if (ports[0] == ports[1])
		printf("%d ", ports[0]);
	else
		printf("%d:%d ", ports[0], ports[1]);
}

static void print_icmp_code(uint8_t *code)
{
	if (code[0] == code[1])
		printf("/%"PRIu8 " ", code[0]);
	else
		printf("/%"PRIu8":%"PRIu8 " ", code[0], code[1]);
}

static void print_icmp_type(uint8_t *type, uint8_t *code)
{
	unsigned int i;

	if (type[0] != type[1]) {
		printf("%"PRIu8 ":%" PRIu8, type[0], type[1]);
		print_icmp_code(code);
		return;
	}

	for (i = 0; i<  ARRAY_SIZE(icmpv6_codes); i++) {
		if (icmpv6_codes[i].type != type[0])
			continue;

		if (icmpv6_codes[i].code_min == code[0]&&
		    icmpv6_codes[i].code_max == code[1]) {
			printf("%s ", icmpv6_codes[i].name);
			return;
		}
	}
	printf("%"PRIu8, type[0]);
	print_icmp_code(code);
}

static void print_icmpv6types(void)
{
	unsigned int i;
        printf("Valid ICMPv6 Types:");

	for (i=0; i<  ARRAY_SIZE(icmpv6_codes); i++) {
		if (i&&  icmpv6_codes[i].type == icmpv6_codes[i-1].type) {
			if (icmpv6_codes[i].code_min == icmpv6_codes[i-1].code_min
			&&  (icmpv6_codes[i].code_max
			        == icmpv6_codes[i-1].code_max))
				printf(" (%s)", icmpv6_codes[i].name);
			else
				printf("\n   %s", icmpv6_codes[i].name);
		}
		else
			printf("\n%s", icmpv6_codes[i].name);
	}
	printf("\n");
}

#if 1 //__MSTC__, Jeff
static uint16_t parse_length(const char *name)
{
	char *end;
	int length;

	length = strtol(name, &end, 10);

	if (*end != '\0'){
		print_error("Problem with specified length '%s'", name);
		return 0; /* never reached */
	}else if (length >= 0 || length <= 0xFFFF) {
		return length;
	}	
}

static void
parse_length_range( const char *lengthstring, uint16_t *length)
{
	char *buffer;
	char *cp;
	
	buffer = strdup(lengthstring);
	if ((cp = strchr(buffer, ':')) == NULL)
		length[0] = length[1] = parse_length(buffer);
	else {
		*cp = '\0';
		cp++;
		length[0] = buffer[0] ? parse_length( buffer) : 0;
		length[1] = cp[0] ? parse_length( cp) : 0xFFFF;
		
		if (length[0] > length[1])
			print_error("Invalid lengthrange (min > max)");
	}
	free(buffer);
}

static void print_length_range(uint16_t *length)
{
	if (length[0] == length[1])
		printf("%d ", length[0]);
	else
		printf("%d:%d ", length[0], length[1]);
}
#endif
static void print_help()
{
	printf(
"ip6 options:\n"
"--ip6-src    [!] address[/mask]: ipv6 source specification\n"
"--ip6-dst    [!] address[/mask]: ipv6 destination specification\n"
"--ip6-tclass [!] tclass        : ipv6 traffic class specification\n"
"--ip6-proto  [!] protocol      : ipv6 protocol specification\n"
"--ip6-sport  [!] port[:port]   : tcp/udp source port or port range\n"
"--ip6-dport  [!] port[:port]	: tcp/udp destination port or port range\n"
"--ip6-icmp-type [!] type[[:type]/code[:code]] : ipv6-icmp type/code or type/code range\n"
#if 1 //__MSTC__, Jeff
"--ip6-len    [!] length[:length] : ip length or legth range\n"
#endif
);
print_icmpv6types();
}

static void init(struct ebt_entry_match *match)
{
	struct ebt_ip6_info *ipinfo = (struct ebt_ip6_info *)match->data;

	ipinfo->invflags = 0;
	ipinfo->bitmask = 0;
}

#define OPT_SOURCE 0x01
#define OPT_DEST   0x02
#define OPT_TCLASS 0x04
#define OPT_PROTO  0x08
#define OPT_SPORT  0x10
#define OPT_DPORT  0x20
#if 1 //__MSTC__, Jeff
#define OPT_LENGTH 0X40
#endif

/* Most of the following code is derived from iptables */
static void
in6addrcpy(struct in6_addr *dst, struct in6_addr *src)
{
	memcpy(dst, src, sizeof(struct in6_addr));
}

int string_to_number_ll(const char *s, unsigned long long min,
            unsigned long long max, unsigned long long *ret)
{
	unsigned long long number;
	char *end;

	/* Handle hex, octal, etc. */
	number = strtoull(s, &end, 0);
	if (*end == '\0' && end != s) {
		/* we parsed a number, let's see if we want this */
		if (min <= number && (!max || number <= max)) {
			*ret = number;
			return 0;
		}
	}
	return -1;
}

int string_to_number_l(const char *s, unsigned long min, unsigned long max,
                       unsigned long *ret)
{
	int result;
	unsigned long long number;

	result = string_to_number_ll(s, min, max, &number);
	*ret = (unsigned long)number;

	return result;
}

int string_to_number(const char *s, unsigned int min, unsigned int max,
                     unsigned int *ret)
{
	int result;
	unsigned long number;

	result = string_to_number_l(s, min, max, &number);
	*ret = (unsigned int)number;

	return result;
}

static struct in6_addr *numeric_to_addr(const char *num)
{
	static struct in6_addr ap;
	int err;

	if ((err=inet_pton(AF_INET6, num, &ap)) == 1)
		return &ap;
	return (struct in6_addr *)NULL;
}

static struct in6_addr *parse_ip6_mask(char *mask)
{
	static struct in6_addr maskaddr;
	struct in6_addr *addrp;
	unsigned int bits;

	if (mask == NULL) {
		/* no mask at all defaults to 128 bits */
		memset(&maskaddr, 0xff, sizeof maskaddr);
		return &maskaddr;
	}
	if ((addrp = numeric_to_addr(mask)) != NULL)
		return addrp;
	if (string_to_number(mask, 0, 128, &bits) == -1)
		print_error("Invalid IPv6 Mask '%s' specified", mask);
	if (bits != 0) {
		char *p = (char *)&maskaddr;
		memset(p, 0xff, bits / 8);
		memset(p + (bits / 8) + 1, 0, (128 - bits) / 8);
		p[bits / 8] = 0xff << (8 - (bits & 7));
		return &maskaddr;
	}

	memset(&maskaddr, 0, sizeof maskaddr);
	return &maskaddr;
}

/* Set the ipv6 mask and address. Callers should check ebt_errormsg[0].
 *  * The string pointed to by address can be altered. */
void ebt_parse_ip6_address(char *address, struct in6_addr *addr,
                           struct in6_addr *msk)
{
   struct in6_addr *tmp_addr;
   char buf[256];
   char *p;
   int i;
   int err;

   strncpy(buf, address, sizeof(buf) - 1);
   /* first the mask */
   buf[sizeof(buf) - 1] = '\0';
   if ((p = strrchr(buf, '/')) != NULL) {
      *p = '\0';
      tmp_addr = parse_ip6_mask(p + 1);
   } else
      tmp_addr = parse_ip6_mask(NULL);
   in6addrcpy(msk, tmp_addr);

   /* if a null mask is given, the name is ignored, like in "any/0" */
   if (!memcmp(msk, &in6addr_any, sizeof(in6addr_any)))
      strcpy(buf, "::");

   if ((err=inet_pton(AF_INET6, buf, addr)) < 1) {
      print_error("Invalid IPv6 Address '%s' specified", buf);
      return;
   }

   for (i = 0; i < 4; i++)
      addr->s6_addr32[i] &= msk->s6_addr32[i];
}

/* Transform the ip6 addr into a string ready for output. */
char *ebt_ip6_to_numeric(const struct in6_addr *addrp)
{
   /* 0000:0000:0000:0000:0000:000.000.000.000
    *     * 0000:0000:0000:0000:0000:0000:0000:0000 */
   static char buf[50+1];
      return (char *)inet_ntop(AF_INET6, addrp, buf, sizeof(buf));
}

static int parse(int c, char **argv, int argc, const struct ebt_u_entry *entry,
   unsigned int *flags, struct ebt_entry_match **match)
{
	struct ebt_ip6_info *ipinfo = (struct ebt_ip6_info *)(*match)->data;
	char *end;
	long int i;

	switch (c) {
	case IP_SOURCE:
		check_option(flags, OPT_SOURCE);
		ipinfo->bitmask |= EBT_IP6_SOURCE;
		if (check_inverse(optarg)) {
		    ipinfo->invflags |= EBT_IP6_SOURCE;
		}
		ebt_parse_ip6_address(argv[optind - 1], &ipinfo->saddr, &ipinfo->smsk);
		break;

	case IP_DEST:
		check_option(flags, OPT_DEST);
		ipinfo->bitmask |= EBT_IP6_DEST;
		if (check_inverse(optarg)) {
			ipinfo->invflags |= EBT_IP6_DEST;
		}

      if (optind > argc)
         print_error("Missing IP address argument");

		ebt_parse_ip6_address(argv[optind - 1], &ipinfo->daddr, &ipinfo->dmsk);
		break;

	case IP_SPORT:
	case IP_DPORT:
		if (c == IP_SPORT) {
			check_option(flags, OPT_SPORT);
			ipinfo->bitmask |= EBT_IP6_SPORT;
			if (check_inverse(optarg))
				ipinfo->invflags |= EBT_IP6_SPORT;
		} else {
			check_option(flags, OPT_DPORT);
			ipinfo->bitmask |= EBT_IP6_DPORT;
			if (check_inverse(optarg))
				ipinfo->invflags |= EBT_IP6_DPORT;
		}

      if (optind > argc)
         print_error("Missing port argument");

		if (c == IP_SPORT)
			parse_port_range(NULL, argv[optind - 1], ipinfo->sport);
		else
			parse_port_range(NULL, argv[optind - 1], ipinfo->dport);
		break;

    case IP_ICMP6:
        check_option(flags, EBT_IP6_ICMP6);
        ipinfo->bitmask |= EBT_IP6_ICMP6;
        if (check_inverse(optarg))
           ipinfo->invflags |= EBT_IP6_ICMP6;
        if (parse_icmpv6(optarg, ipinfo->icmpv6_type, ipinfo->icmpv6_code))
           return 0;
        break;

	case IP_TCLASS:
		check_option(flags, OPT_TCLASS);
		if (check_inverse(optarg))
			ipinfo->invflags |= EBT_IP6_TCLASS;

      if (optind > argc)
         print_error("Missing TCLASS argument");

		i = strtol(argv[optind - 1], &end, 16);
		if (i < 0 || i > 255 || *end != '\0')
			print_error("Problem with specified IPv6 traffic class");
		ipinfo->tclass = i;
		ipinfo->bitmask |= EBT_IP6_TCLASS;
		break;

	case IP_PROTO:
		check_option(flags, OPT_PROTO);
		if (check_inverse(optarg))
			ipinfo->invflags |= EBT_IP6_PROTO;

      if (optind > argc)
         print_error("Missing IP protocol argument");

		i = strtoul(argv[optind - 1], &end, 10);
		if (*end != '\0') {
			struct protoent *pe;

			pe = getprotobyname(argv[optind - 1]);
			if (pe == NULL)
				print_error("Unknown specified IP protocol - %s", argv[optind - 1]);
			ipinfo->protocol = pe->p_proto;
		} else {
			ipinfo->protocol = (unsigned char) i;
		}
		ipinfo->bitmask |= EBT_IP6_PROTO;
		break;
#if 1 //__MSTC__, Jeff
	case IP_LENGTH:
		check_option(flags, OPT_LENGTH);
		if (check_inverse(optarg))
			ipinfo->invflags |= EBT_IP6_LENGTH;
		if (optind > argc)
			print_error("Missing IP length argument");
		parse_length_range(argv[optind - 1], ipinfo->length);
		ipinfo->bitmask |= EBT_IP6_LENGTH;
		break;
#endif
	default:
		return 0;
	}
	return 1;
}

static void final_check(const struct ebt_u_entry *entry,
   const struct ebt_entry_match *match, const char *name,
   unsigned int hookmask, unsigned int time)
{
	struct ebt_ip6_info *ipinfo = (struct ebt_ip6_info *)match->data;

	if (entry->ethproto != ETH_P_IPV6 || entry->invflags & EBT_IPROTO) {
		print_error("For IPv6 filtering the protocol must be "
		            "specified as IPv6");
	} else if (ipinfo->bitmask & (EBT_IP6_SPORT|EBT_IP6_DPORT) &&
		(!(ipinfo->bitmask & EBT_IP6_PROTO) ||
		ipinfo->invflags & EBT_IP6_PROTO ||
		(ipinfo->protocol!=IPPROTO_TCP &&
		 ipinfo->protocol!=IPPROTO_UDP &&
		 ipinfo->protocol!=IPPROTO_SCTP)))
//		 ipinfo->protocol!=IPPROTO_SCTP &&
//		 ipinfo->protocol!=IPPROTO_DCCP)))
		print_error("For port filtering the IP protocol must be "
				"either 6 (tcp), 17 (udp), 33 (dccp) or "
				"132 (sctp)");
		if ((ipinfo->bitmask&  EBT_IP6_ICMP6)&&
	    (!(ipinfo->bitmask&  EBT_IP6_PROTO) ||
	     ipinfo->invflags&  EBT_IP6_PROTO ||
	     ipinfo->protocol != IPPROTO_ICMPV6))
		print_error("For ipv6-icmp filtering the IP protocol must be "
				"58 (ipv6-icmp)");
}

static void print(const struct ebt_u_entry *entry,
   const struct ebt_entry_match *match)
{
	struct ebt_ip6_info *ipinfo = (struct ebt_ip6_info *)match->data;

	if (ipinfo->bitmask & EBT_IP6_SOURCE) {
		printf("--ip6-src ");
		if (ipinfo->invflags & EBT_IP6_SOURCE)
			printf("! ");
		printf("%s", ebt_ip6_to_numeric(&ipinfo->saddr));
		printf("/%s ", ebt_ip6_to_numeric(&ipinfo->smsk));
	}
	if (ipinfo->bitmask & EBT_IP6_DEST) {
		printf("--ip6-dst ");
		if (ipinfo->invflags & EBT_IP6_DEST)
			printf("! ");
		printf("%s", ebt_ip6_to_numeric(&ipinfo->daddr));
		printf("/%s ", ebt_ip6_to_numeric(&ipinfo->dmsk));
	}
	if (ipinfo->bitmask & EBT_IP6_TCLASS) {
		printf("--ip6-tclass ");
		if (ipinfo->invflags & EBT_IP6_TCLASS)
			printf("! ");
		printf("0x%02X ", ipinfo->tclass);
	}
	if (ipinfo->bitmask & EBT_IP6_PROTO) {
		struct protoent *pe;

		printf("--ip6-proto ");
		if (ipinfo->invflags & EBT_IP6_PROTO)
			printf("! ");
		pe = getprotobynumber(ipinfo->protocol);
		if (pe == NULL) {
			printf("%d ", ipinfo->protocol);
		} else {
			printf("%s ", pe->p_name);
		}
	}
	if (ipinfo->bitmask & EBT_IP6_SPORT) {
		printf("--ip6-sport ");
		if (ipinfo->invflags & EBT_IP6_SPORT)
			printf("! ");
		print_port_range(ipinfo->sport);
	}
	if (ipinfo->bitmask & EBT_IP6_DPORT) {
		printf("--ip6-dport ");
		if (ipinfo->invflags & EBT_IP6_DPORT)
			printf("! ");
		print_port_range(ipinfo->dport);
	}
	if (ipinfo->bitmask&  EBT_IP6_ICMP6) {
		printf("--ip6-icmp-type ");
		if (ipinfo->invflags&  EBT_IP6_ICMP6)
			printf("! ");
		print_icmp_type(ipinfo->icmpv6_type, ipinfo->icmpv6_code);
	}
#if 1 //__MSTC__, Jeff
	if (ipinfo->bitmask & EBT_IP6_LENGTH) {
		printf("--ip6-len ");
		if (ipinfo->invflags & EBT_IP6_LENGTH) {
			printf("! ");
		}
		print_length_range(ipinfo->length);
	}
#endif
}

static int compare(const struct ebt_entry_match *m1,
   const struct ebt_entry_match *m2)
{
	struct ebt_ip6_info *ipinfo1 = (struct ebt_ip6_info *)m1->data;
	struct ebt_ip6_info *ipinfo2 = (struct ebt_ip6_info *)m2->data;

	if (ipinfo1->bitmask != ipinfo2->bitmask)
		return 0;
	if (ipinfo1->invflags != ipinfo2->invflags)
		return 0;
	if (ipinfo1->bitmask & EBT_IP6_SOURCE) {
		if (!IN6_ARE_ADDR_EQUAL(&ipinfo1->saddr, &ipinfo2->saddr))
			return 0;
		if (!IN6_ARE_ADDR_EQUAL(&ipinfo1->smsk, &ipinfo2->smsk))
			return 0;
	}
	if (ipinfo1->bitmask & EBT_IP6_DEST) {
		if (!IN6_ARE_ADDR_EQUAL(&ipinfo1->daddr, &ipinfo2->daddr))
			return 0;
		if (!IN6_ARE_ADDR_EQUAL(&ipinfo1->dmsk, &ipinfo2->dmsk))
			return 0;
	}
	if (ipinfo1->bitmask & EBT_IP6_TCLASS) {
		if (ipinfo1->tclass != ipinfo2->tclass)
			return 0;
	}
	if (ipinfo1->bitmask & EBT_IP6_PROTO) {
		if (ipinfo1->protocol != ipinfo2->protocol)
			return 0;
	}
	if (ipinfo1->bitmask & EBT_IP6_SPORT) {
		if (ipinfo1->sport[0] != ipinfo2->sport[0] ||
		   ipinfo1->sport[1] != ipinfo2->sport[1])
			return 0;
	}
	if (ipinfo1->bitmask & EBT_IP6_DPORT) {
		if (ipinfo1->dport[0] != ipinfo2->dport[0] ||
		   ipinfo1->dport[1] != ipinfo2->dport[1])
			return 0;
	}
	if (ipinfo1->bitmask&  EBT_IP6_ICMP6) {
		if (ipinfo1->icmpv6_type[0] != ipinfo2->icmpv6_type[0] ||
		    ipinfo1->icmpv6_type[1] != ipinfo2->icmpv6_type[1] ||
		    ipinfo1->icmpv6_code[0] != ipinfo2->icmpv6_code[0] ||
		    ipinfo1->icmpv6_code[1] != ipinfo2->icmpv6_code[1])
			return 0;
	}
#if 1 //__MSTC__, Jeff
	if (ipinfo1->bitmask & EBT_IP6_LENGTH) {
		if (ipinfo1->length[0] != ipinfo2->length[0] ||
				ipinfo1->length[1] != ipinfo2->length[1])
			return 0;
	}
#endif
	return 1;
}

static struct ebt_u_match ip6_match =
{
	.name		= "ip6",
	.size		= sizeof(struct ebt_ip6_info),
	.help		= print_help,
	.init		= init,
	.parse		= parse,
	.final_check	= final_check,
	.print		= print,
	.compare	= compare,
	.extra_ops	= opts,
};

static void _init(void) __attribute((constructor));
static void _init(void)
{
   register_match(&ip6_match);
}
