#include "cms.h"

#define NUM_RRS 5
/*****************************************************************************/
struct dns_rr{
  char name[NAME_SIZE];
  uint16 type;
  uint16 class;
  uint32 ttl;
  uint16 rdatalen;
  char data[NAME_SIZE];
};
/*****************************************************************************/
union header_flags {
  uint16 flags;
  
#if 1 // ZyXEL, Hong-Yu
  struct {
    unsigned short int question:1;
    unsigned short int opcode:4;
    unsigned short int authorative:1;
    unsigned short int truncated:1;
    unsigned short int want_recursion:1;
    unsigned short int recursion_avail:1;
    unsigned short int unused:3;
    unsigned short int rcode:4;
  } f;
#else
  struct {
    unsigned short int rcode:4;
    unsigned short int unused:3;
    unsigned short int recursion_avail:1;
    unsigned short int want_recursion:1;
    unsigned short int truncated:1;
    unsigned short int authorative:1;
    unsigned short int opcode:4;
    unsigned short int question:1;
  } f;
#endif
};
/*****************************************************************************/
struct dns_header_s{
  uint16 id;
  union header_flags flags;
  uint16 qdcount;
  uint16 ancount;
  uint16 nscount;
  uint16 arcount;
};
/*****************************************************************************/
struct dns_message{
  struct dns_header_s header;
  struct dns_rr question[NUM_RRS];
  struct dns_rr answer[NUM_RRS];
};
/*****************************************************************************/
typedef struct dns_request_s{
  char cname[NAME_SIZE];
  char ip[INET6_ADDRSTRLEN];
#ifdef DMP_X_5067F0_IPV6_1 // __MSTC__, Richard Huang
  char ip6[40];
#endif
  int ttl;
  //BRCM
  int switch_on_timeout;  /* if this request times out, switch to secondary server */
  int retx_count;   /* number of times this request has been retransmitted */
  time_t expiry; /* Time to expire */
#ifndef DNS_PROBE //__MSTC__,kenny,Merge from DSL-491HNU-B1B_STD
  int repeat_times;
#endif

  /* the actual dns request that was recieved */
  struct dns_message message;

  struct sockaddr_storage src_info;

  /* where the request came from */
#ifdef DMP_X_5067F0_IPV6_1
  struct in6_addr src_addr;
#else
  struct in_addr src_addr;
#endif
  int src_port;

  /* the orginal packet */
  char original_buf[MAX_PACKET_SIZE];
  int numread;
  char *here;

  /* previous node in list */
  struct dns_request_s *prev;
  /* next node in list */
  struct dns_request_s *next;
}dns_request_t;
/*****************************************************************************/
/* TYPE values */
enum{ A = 1,      /* a host address */
	NS,       /* an authoritative name server */
	MD,       /* a mail destination (Obsolete - use MX) */
	MF,       /* */
	CNAME,    /* the canonical name for an alias */
	SOA,      /* marks the start of a zone of authority  */
	MB,       /* a mailbox domain name (EXPERIMENTAL) */
	MG,       /* */
	MR,       /* */
	NUL,      /* */
	WKS,      /* a well known service description */
	PTR,      /* a domain name pointer */
	HINFO,    /* host information */
	MINFO,    /* mailbox or mail list information */
	MX,       /* mail exchange */
	TXT,      /* text strings */

	AAA = 0x1c, /* IPv6 A */
	SRVRR = 0x21  /* RFC 2782: location of services */
	};

/* CLASS values */
enum{
  IN = 1,         /* the Internet */
    CS,
    CH,
    HS
};

/* OPCODE values */
enum{
  QUERY,
    IQUERY,
    STATUS
};
