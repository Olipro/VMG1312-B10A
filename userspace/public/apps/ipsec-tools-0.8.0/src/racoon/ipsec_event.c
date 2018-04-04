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

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <netdb.h>
#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifdef ENABLE_NATT
# ifdef __linux__
#  include <linux/udp.h>
# endif
# if defined(__NetBSD__) || defined(__FreeBSD__) ||	\
  (defined(__APPLE__) && defined(__MACH__))
#  include <netinet/udp.h>
# endif
#endif

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <sys/sysctl.h>

#include <net/route.h>
#include <net/pfkeyv2.h>

#include <netinet/in.h>
#include PATH_IPSEC_H
#include <fcntl.h>

#include "libpfkey.h"

#include "var.h"
#include "misc.h"
#include "vmbuf.h"
#include "plog.h"
#include "sockmisc.h"
#include "session.h"
#include "debug.h"

#include "schedule.h"
#include "localconf.h"
#include "remoteconf.h"
#include "handler.h"
#include "policy.h"
#include "proposal.h"
#include "isakmp_var.h"
#include "isakmp.h"
#include "isakmp_inf.h"
#include "ipsec_doi.h"
#include "oakley.h"
#include "pfkey.h"
#include "algorithm.h"
#include "sainfo.h"
#include "admin.h"
#include "evt.h"
#include "privsep.h"
#include "strnames.h"
#include "backupsa.h"
#include "gcmalloc.h"
#include "nattraversal.h"
#include "crypto_openssl.h"
#include "grabmyaddr.h"

#include <errno.h>
#include <signal.h>

#include "ipsec_event.h"

#if 0 // Chia Chao
int init_ipsec_fifo(char* fifo_name, int blocking) {
  int ret;

  plog(LLV_DEBUG, LOCATION, NULL, "---------- init_ipsec_fifo: 1\n");

  /* if my fifo exists, unlink it */
  if (access(fifo_name, F_OK) != -1) {
    
    plog(LLV_DEBUG, LOCATION, NULL, "---------- init_ipsec_fifo: unlink old fifo\n");

    unlink(fifo_name);
  }

  plog(LLV_DEBUG, LOCATION, NULL, "---------- init_ipsec_fifo: mkfifo\n");
    
  /* create my fifo */
  ret = mkfifo(fifo_name, 0777);
  if (ret != 0) {
    plog(LLV_DEBUG, LOCATION, NULL, "---------- init_ipsec_fifo: error no = %d\n", errno);
    perror("init_ipsec_fifo: mkfifo ");
    return -1;
  }

  plog(LLV_DEBUG, LOCATION, NULL, "---------- init_fifo: exit\n");
  return 0;
}
#endif

int open_ipsec_send_fifo(char* fifo_name, int blocking) {
  int fifo_fd;
  
  plog(LLV_DEBUG, LOCATION, NULL, "---------- open_ipsec_send_fifo: 1\n");
  
  /* check if app fifo exists */
  if (access(fifo_name, F_OK) == -1) {
    plog(LLV_DEBUG, LOCATION, NULL, "---------- open_ipsec_send_fifo: error no = %d\n", errno);
    perror("open_ipsec_send_fifo: client access ");
    return -1;
  }
  
  plog(LLV_DEBUG, LOCATION, NULL, "---------- open_ipsec_send_fifo: 2\n");
  
  // open app fifo for blocking write
  if (blocking == 1)
    fifo_fd = open(fifo_name, O_WRONLY);
  else if (blocking == 0)
    fifo_fd = open(fifo_name, O_WRONLY | O_NONBLOCK);
  else {
    // bad value for blocking
    return -2;
  }
  if (fifo_fd == -1) {
    plog(LLV_DEBUG, LOCATION, NULL, "---------- open_ipsec_send_fifo: error no = %d\n", errno);
    perror("open_ipsec_send_fifo: open write ");
    return -3;
  }

  plog(LLV_DEBUG, LOCATION, NULL, "---------- open_ipsec_send_fifo: exit fifo_fd = %d\n", fifo_fd);
  return fifo_fd;
}

int open_ipsec_recv_fifo(char* fifo_name, int blocking) {
  int fifo_fd;

  /* open fifo for reading */
  plog(LLV_DEBUG, LOCATION, NULL, "---------- open_ipsec_recv_fifo: open fifo\n");
  
  /* check if app fifo exists */
  if (access(fifo_name, F_OK) == -1) {
    plog(LLV_DEBUG, LOCATION, NULL, "---------- open_ipsec_recv_fifo: error no = %d\n", errno);
    perror("open_ipsec_recv_fifo: client access ");
    return -1;
  }
  
  plog(LLV_DEBUG, LOCATION, NULL, "---------- open_ipsec_recv_fifo: 2\n");
  
  if (blocking == 0) 
    fifo_fd = open(fifo_name, O_RDONLY | O_NONBLOCK);
  else if (blocking == 1)
    fifo_fd = open(fifo_name, O_RDONLY);
  else {
    // bad value for blocking
    return -2;
  }
  if (fifo_fd == -1) {
    plog(LLV_DEBUG, LOCATION, NULL, "---------- open_ipsec_recv_fifo: error no = %d\n", errno);
    perror("open_ipsec_recv_fifo: server open read ");
    return -3;
  }
  
  plog(LLV_DEBUG, LOCATION, NULL, "---------- open_ipsec_recv_fifo: fifo_fd = %d\n", fifo_fd);
  return fifo_fd;
}

#if 0 // Chia Chao
ssize_t get_ipsec_fifo(int fifo_read_fd, void* buffer, ssize_t len) {
  int ret = 0;

  plog(LLV_DEBUG, LOCATION, NULL, "---------- get_ipsec_fifo: 1 fifo_read_fd = %d, len = %d\n", fifo_read_fd, len);

  if (buffer == NULL) {
    plog(LLV_DEBUG, LOCATION, NULL, "---------- get_ipsec_fifo: buffer is NULL\n");
    return -1;
  }

  ret = read(fifo_read_fd, buffer, len);
  if (ret == -1) {
    plog(LLV_DEBUG, LOCATION, NULL, "---------- get_ipsec_fifo: error no = %d\n", errno);
    perror("get_ipsec_fifo: read ");
    return -1;
  }
  
  plog(LLV_DEBUG, LOCATION, NULL, "---------- get_ipsec_fifo: 2 ret = %d\n", ret);

  return ret;
}
#endif

ssize_t put_ipsec_fifo(int fifo_write_fd, void* message, ssize_t len) {
  ssize_t ret = 0;

  plog(LLV_DEBUG, LOCATION, NULL, "---------- put_ipsec_fifo: 1 fifo_write_fd = %d, len = %d\n", fifo_write_fd, len);

  if (message == NULL) {
    plog(LLV_DEBUG, LOCATION, NULL, "---------- put_ipsec_fifo: message is NULL\n");
    return -1;
  }

  plog(LLV_DEBUG, LOCATION, NULL, "---------- put_ipsec_fifo: 2\n");
  
  /* write message to fifo */
  ret = write(fifo_write_fd, message, len);    
  if (ret == -1) {
    plog(LLV_DEBUG, LOCATION, NULL, "---------- put_ipsec_fifo: error no = %d\n", errno);
    perror("put_ipsec_fifo: server write ");
    return -1;
  }
  
  plog(LLV_DEBUG, LOCATION, NULL, "---------- put_ipsec_fifo: 3\n");

  return ret;
}

#if 0 // Chia Chao
void free_ipsec_fifo(int fifo_fd, char* fifo_name) {

  plog(LLV_DEBUG, LOCATION, NULL, "---------- free_ipsec_fifo: freeing fd = %d, name = %s\n", fifo_fd, fifo_name);
  close(fifo_fd);
  unlink(fifo_name);
}
#endif

void ipsec_event_sig_handler(int sig) {
  plog(LLV_DEBUG, LOCATION, NULL, "---------- ipsec_event_sig_handler: enter\n");
  close(fifo_read_fd);
  close(fifo_write_fd);
  exit(1);
}

int init_ipsec_event(int* fifo_read_fd, int* fifo_write_fd ) {
  int i;
  int ret;

  plog(LLV_DEBUG, LOCATION, NULL, "---------- init_ipsec_event: enter\n");

  signal(SIGKILL, ipsec_event_sig_handler);
  signal(SIGINT, ipsec_event_sig_handler);
  signal(SIGTERM, ipsec_event_sig_handler);

  /* open fifo for read (fake) */
  plog(LLV_DEBUG, LOCATION, NULL, "---------- init_ipsec_event: open recv fifo\n");
  *fifo_read_fd = open_ipsec_recv_fifo(IPSEC_WRAPPER_FIFO, 0);
  if (*fifo_read_fd == -1) {
    plog(LLV_DEBUG, LOCATION, NULL, "---------- init_ipsec_event: open_recv_fifo failed\n");
    return -1;
  }

  plog(LLV_DEBUG, LOCATION, NULL, "---------- init_ipsec_event: fifo_read_fd = %d\n", *fifo_read_fd);
  /* open fifo for write */
  plog(LLV_DEBUG, LOCATION, NULL, "---------- init_ipsec_event: open send fifo\n");
  *fifo_write_fd = open_ipsec_send_fifo(IPSEC_WRAPPER_FIFO, 0);
  if (*fifo_write_fd == -1) {
    plog(LLV_DEBUG, LOCATION, NULL, "---------- init_ipsec_event: open_send_fifo failed, abort\n");
    return -1;
  } 

  plog(LLV_DEBUG, LOCATION, NULL, "---------- init_ipsec_event: fifo_write_fd = %d\n", *fifo_write_fd);

#if 0 // Chia Chao
  /* open fifo for read (fake) */
  plog(LLV_DEBUG, LOCATION, NULL, "---------- init_ipsec_event: open recv fifo\n");
  *fifo_read_fd = open_ipsec_recv_fifo(IPSEC_APP_FIFO, 1);
  if (*fifo_read_fd == -1) {
    plog(LLV_DEBUG, LOCATION, NULL, "---------- init_ipsec_event: open_recv_fifo failed\n");
    return -1;
  }

  plog(LLV_DEBUG, LOCATION, NULL, "---------- init_ipsec_event: fifo_read_fd = %d\n", *fifo_read_fd);
#endif

  return 0;
}

int send_ipsec_event(int fifo_write_fd, 
		     struct sockaddr *src, struct sockaddr *dst, 
		     unsigned int spi, int status, int msg, char* name) {
  int retcode = 0;
  tunnel_info ti;

  plog(LLV_DEBUG, LOCATION, NULL, "---------- send_ipsec_event: enter\n");

  /*
   * Now put some things into the memory for the 
   * other process to read
   */
  ti.msg = msg;
  ti.src = ((struct sockaddr_in *)src)->sin_addr.s_addr;
  ti.dst = ((struct sockaddr_in *)dst)->sin_addr.s_addr;
  ti.spi = ntohl(spi);
  ti.status = status;
  strcpy(ti.name, name);

  retcode = put_ipsec_fifo(fifo_write_fd, &ti, sizeof(tunnel_info));
  if (retcode == -1) {
    plog(LLV_DEBUG, LOCATION, NULL, "---------- ipsec_event: send_event() failed, abort\n");
    return -1;
  }
  
  plog(LLV_DEBUG, LOCATION, NULL, "---------- send_ipsec_event: exit\n");

  return 0;
}

#endif
