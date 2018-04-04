#include "dproxy.h"

void *get_in_addr(struct sockaddr_storage *sa);
int dns_read_packet(int sock, dns_request_t *m);
int dns_write_packet(int sock, struct sockaddr_storage *in, dns_request_t *m);
