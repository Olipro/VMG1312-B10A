#include "dproxy.h"

/*
 * Queries are encoded such that there is and integer specifying how long 
 * the next block of text will be before the actuall text. For eaxmple:
 *             www.linux.com => \03www\05linux\03com\0
 * This function assumes that buf points to an encoded name.
 * The name is decoded into name. Name should be at least 255 bytes long.
 * RETURNS: The length of the string including the '\0' terminator.
 */
void dns_decode_name(char *name, char **buf);
/*
 * Decodes the raw packet in buf to create a rr. Assumes buf points to the 
 * start of a rr. 
 * Note: Question rrs dont have rdatalen or rdata. Set is_question when
 * decoding question rrs, else clear is_question
 * RETURNS: the amount that buf should be incremented
 */
void dns_decode_rr(struct dns_rr *rr, char **buf,int is_question,char *header, char *buf_start, struct dns_message *m);

/*
 * A raw packet pointed to by buf is decoded in the assumed to be alloced 
 * dns_message structure.
 * RETURNS: 0
 */
int dns_decode_message(struct dns_message *m, char **buf);

/* 
 * 
 */
void dns_decode_request(dns_request_t *m);





