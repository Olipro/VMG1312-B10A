/************************************************************************
 *
 *      Copyright (C) 2012 ZyXEL Communications, Corp.
 *      All Rights Reserved.
 *
 * ZyXEL Confidential; Need to Know only.
 * Protected as an unpublished work.
 *
 * The computer program listings, specifications and documentation
 * herein are the property of ZyXEL Communications, Corp. and shall
 * not be reproduced, copied, disclosed, or used in whole or in part
 * for any reason without the prior express written permission of
 * ZyXEL Communications, Corp.
 *
 *************************************************************************/

#if 1 //__ZyXEL__, Chia Chao, support IPSec

// Maximum length of textual representation of IPv6 address, with null char
#define IP_BUFLEN 46

enum {
  EVENT_P1_UP,
  EVENT_P1_DOWN,
  EVENT_P2_UP,
  EVENT_P2_DOWN
};

typedef struct {
  int msg;
  unsigned long src;
  unsigned long dst;
  unsigned long sa_src;
  unsigned long sa_dst;
  unsigned int spi; //Yetties, fix spi
  int status;
  char name[64];
} tunnel_info;

int fifo_read_fd;
int fifo_write_fd;

#define IPSEC_WRAPPER_FIFO   "/tmp/ipsec_wrapper_fifo"
#define IPSEC_APP_FIFO   "/tmp/ipsec_app_fifo"

extern int init_ipsec_fifo(char* fifo_name, int blocking);
extern int open_ipsec_send_fifo(char* fifo_name, int blocking);
extern int open_ipsec_recv_fifo(char* fifo_name, int blocking);
extern ssize_t put_ipsec_fifo(int fifo_write_fd, void* message, ssize_t len);
extern ssize_t get_ipsec_fifo(int fifo_read_fd, void* buffer, ssize_t len);
extern void free_ipsec_fifo(int fifo_fd, char* fifo_name);

extern int send_ipsec_event(int fifo_write_fd, struct sockaddr *src, struct sockaddr *dst, unsigned int spi, int status, int msg, char* name); //Yetties, fix spi
extern int init_ipsec_event(int* fifo_read_fd, int* fifo_write_fd);
extern void ipsec_event_sig_handler(int sig);

#endif

