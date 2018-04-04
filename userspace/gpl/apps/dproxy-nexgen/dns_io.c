#include <sys/socket.h>
#include <sys/uio.h>
#include <linux/sockios.h>
#include <linux/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "dns_io.h"
#include "dns_decode.h"

/*****************************************************************************/
void *get_in_addr( struct sockaddr_storage *sa )
{
   if (sa->ss_family == AF_INET)
      return &(((struct sockaddr_in *)sa)->sin_addr);

   return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

int dns_read_packet(int sock, dns_request_t *m)
{
  struct sockaddr_storage sa;
  socklen_t salen;
  char remoteIP[INET6_ADDRSTRLEN];
  
  /* Read in the actual packet */
  salen = sizeof(sa);
  
  m->numread = recvfrom(sock, m->original_buf, sizeof(m->original_buf), 0,
		     (struct sockaddr *)&sa, &salen);
  
  if ( m->numread < 0) {
    debug_perror("dns_read_packet: recvfrom\n");
    return -1;
  }
  
  /* TODO: check source addr against list of allowed hosts */

  /* record where it came from */
  memcpy( (void *)&m->src_info, (void *)&sa, sizeof(sa));
  inet_ntop(sa.ss_family, get_in_addr(&sa), 
            remoteIP, INET6_ADDRSTRLEN);
  debug("received pkt from %s:%d len %d", 
        remoteIP, ((struct sockaddr_in *)&sa)->sin_port, m->numread);

  /* check that the message is long enough */
  if( m->numread < sizeof (m->message.header) )
  {
    debug("dns_read_packet: packet too short to be dns packet");
    return -1;
  }

  /* pass on for full decode */
  dns_decode_request( m );

  return 0;
}
/*****************************************************************************/
int dns_write_packet(int sock, struct sockaddr_storage *sa, dns_request_t *m)
{
  int retval;
  char dstIp[INET6_ADDRSTRLEN];

  inet_ntop(sa->ss_family, get_in_addr(sa), dstIp, INET6_ADDRSTRLEN);

  if (sa->ss_family == AF_INET)
  {
    retval = sendto(sock, m->original_buf, m->numread, 0, 
                    (struct sockaddr *)sa, sizeof(struct sockaddr_in));
  }
  else
  {
    retval = sendto(sock, m->original_buf, m->numread, 0, 
                    (struct sockaddr *)sa, sizeof(struct sockaddr_in6));
  }
  
  if( retval < 0 )
  {
    debug_perror("dns_write_packet: sendto");
  }

  return retval;
}
