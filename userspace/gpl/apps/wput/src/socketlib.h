#ifndef __SOCKET_H
#define __SOCKET_H

#include "config.h"

#ifdef HAVE_SSL
# ifndef WIN32
#  include <gnutls/openssl.h>
# endif
#endif

#ifndef WIN32

#include <sys/socket.h>

#define closesocket(x) close(x)
#define WPUT_EINTR EINTR
#else
#include "windows.h"
#endif

#define PROXY_OFF   0
#define PROXY_SOCKS 1
#define PROXY_HTTP  2

typedef struct _proxy_settings {
	unsigned int   ip;
	unsigned short port;
	char *         user;
	char *         pass;
	unsigned int   bind:1;
	unsigned int   type:2;
} proxy_settings;

typedef struct _wput_socket {
	int fd;
#ifdef HAVE_SSL
	SSL     * ssl;
	SSL_CTX * ctx;
#endif
} wput_socket;

wput_socket * socket_new();
void          socket_set_default_timeout(int timeout);
wput_socket * socket_connect(const unsigned int ip, const unsigned short port);
wput_socket * socket_listen(unsigned bindaddr, unsigned short * s_port);
wput_socket * socket_accept(wput_socket * sock);
void          socket_close(wput_socket * sock);
#ifdef HAVE_SSL
int           socket_transform_to_ssl(wput_socket * sock);
#endif

char * socket_read_line(wput_socket * sock);
int    socket_read (wput_socket * sock, void *buf, size_t len);
int    socket_write(wput_socket * sock, void *buf, size_t len);

int get_ip_addr(char* hostname, unsigned int * ip);
int get_local_ip(int sockfd, char * local_ip);

int socket_set_blocking(int sock, unsigned char block);
int socket_is_data_writeable(int s, int timeout);
int socket_is_data_readable(int s, int timeout);
wput_socket * socket_timeout_connect(wput_socket * sock, struct sockaddr *remote_addr, size_t size, int timeout);
wput_socket * proxy_init(proxy_settings * ps);
wput_socket * proxy_listen(proxy_settings * ps, unsigned int * ip, unsigned short * port);
wput_socket * proxy_accept(wput_socket * server);
wput_socket * proxy_connect(proxy_settings * ps, unsigned int ip, unsigned short port, const char * hostname);
#endif
