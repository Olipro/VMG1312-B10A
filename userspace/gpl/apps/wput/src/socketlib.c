/* Declarations for wput.
   Copyright (C) 1989-1994, 1996-1999, 2001 
   This file is part of wput.

   The wput is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License 
   as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The wput is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

   You should have received a copy of the GNU General Public
   License along with the wput; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

/* This file contains several lower-level functions for dealing with sockets.
 * It is meant to provide some library functions. The only required external depency
 * the printip function that is provided in utils.c */

#include <malloc.h>
#include <string.h>
#include <fcntl.h>
#ifndef WIN32
#  include <unistd.h>
#  include <sys/select.h>
#endif


#define WPUT 1

#ifndef WPUT
/*#define printout(x, ...) do { printf(__VA_ARGS__); } while(x >= vNORMAL)*/
#  define printout(x, ...) 
#  define Abort(x) { puts(x); exit(1); }
#  define int64toa(num, buf, base) sprintf(buf, "%d", num)
#else
#  include "utils.h"
#endif

#include "constants.h"
#include "socketlib.h"
#include "windows.h"

#define ipaddr h_addr_list[0]

#ifndef WIN32
#include <netdb.h>
#include <sys/errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

extern int errno;
char * printip(unsigned char * ip);
char * base64(char * p, size_t len);

/* =================================== *
 * ====== main socket functions ====== *
 * =================================== */
int default_timeout = 300;

wput_socket * socket_new() {
	wput_socket * sock = malloc(sizeof(wput_socket));
	memset(sock, 0, sizeof(wput_socket));
	return sock;
}

void socket_set_default_timeout(int timeout) {
	default_timeout = timeout;
}

wput_socket *  socket_connect(const unsigned int ip, const unsigned short port){
	struct sockaddr_in remote_addr;
	wput_socket * sock = socket_new();
	
	memset((char *)&remote_addr,0,sizeof(remote_addr));
	remote_addr.sin_family      = AF_INET;
	remote_addr.sin_addr.s_addr = ip;
	remote_addr.sin_port        = htons((unsigned short) port);
	
	/*
	* Open a TCP socket(an internet stream socket).
	*/
	
	if( (sock->fd = socket(AF_INET,SOCK_STREAM,0)) < 0)
	#ifdef WIN32
	{ printf("%d", GetLastError()); exit(4); }
	#else
	perror(_("client: can't open stream socket"));
	#endif
	printout(vDEBUG, "c_sock: %x\n", sock->fd);
	
	/* do the actual connection */
	if(!socket_timeout_connect(sock,(struct sockaddr *)&remote_addr,sizeof(remote_addr), default_timeout)) {
		printout(vDEBUG, "socket_timeout_connect error \n");
		socket_close(sock);
		return NULL;
	}
	return sock;
}

wput_socket * socket_listen(unsigned bindaddr, unsigned short * s_port) {
	struct sockaddr_in serv_addr;
	wput_socket * sock = socket_new();
	
	/*
	* Open a TCP socket(an Internet STREAM socket)
	*/
	if ((sock->fd = socket(AF_INET, SOCK_STREAM, 0))<0)
		perror(_("server: can't open new socket"));
	/*
	* Bind out local address so that the client can send to us
	*/
	memset((void *)&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family            = AF_INET;
	serv_addr.sin_port              = htons(*s_port);
	serv_addr.sin_addr.s_addr       = htonl(bindaddr);
	
	if(bind(sock->fd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) <0){
		printout(vLESS, _("Error: "));
		printout(vLESS, _("server: can't bind local address\n"));
		exit(0);
	}
	if (!*s_port)
		{
		/* #### addrlen should be a 32-bit type, which int is not
			guaranteed to be.  Oh, and don't try to make it a size_t,
			because that can be 64-bit.  */
		socklen_t addrlen = sizeof (struct sockaddr_in);
		if (getsockname (sock->fd, (struct sockaddr *)&serv_addr, &addrlen) < 0)
			{
			if(sock->fd != -1) {
				close (sock->fd);
				sock->fd = -1;
			}
			printout(vDEBUG, "Failed to open server socket.\n");
#if 1 //__MSTC__, Dennis merge bugfix from TELEFONICA      
      //Memory leak problem, __TELEFONICA__, Mitrastar Smile, 20110824
         socket_close(sock);
#endif
			return NULL;
			}
		*s_port = ntohs (serv_addr.sin_port);
		}
	
	/* TODO USS install a signal handler to clean up if user interrupt the server */
	/* TODO USS sighandler(*s_sock); */
	listen(sock->fd, 1);
	printout(vDEBUG, "Server socket ready to accept client connection.\n");
	
	return sock;
}
/* s_sock is the listening server socket and we accept one incoming
 * connection */
wput_socket * socket_accept(wput_socket * sock){
  socklen_t clilen;
  struct sockaddr_in client_addr;
  wput_socket * child = socket_new();

  clilen = sizeof( client_addr);
  child->fd = accept(sock->fd, (struct sockaddr *)&client_addr, &clilen);
  if(child->fd == -1){
    perror(_("error accepting the incoming connection"));
    exit(4);
  }
  printout(vDEBUG, "Server socket accepted new connection from requesting client.\n");
  return child;
}
#ifdef HAVE_SSL
int socket_transform_to_ssl(wput_socket * sock) {
	sock->ctx = SSL_CTX_new(SSLv23_client_method());
	SSL_CTX_set_verify(sock->ctx, SSL_VERIFY_NONE, NULL);
	sock->ssl = SSL_new(sock->ctx);
	SSL_set_fd(sock->ssl, sock->fd);
	if(!SSL_connect(sock->ssl)) {
		printout(vNORMAL, _("TLS handshake failed.\n"));
		SSL_free(sock->ssl);
		SSL_CTX_free(sock->ctx);
		sock->ssl  = NULL;
		sock->ctx  = NULL;
		return ERR_FAILED;
	}
	return 0;
}
#endif
void socket_close(wput_socket * sock) {
	printout(vDEBUG, "Closing socket %x\n", sock);
	shutdown(sock->fd, 2);
#ifdef HAVE_SSL
	if(sock->ssl) {
		printout(vDEBUG, "Freeing SSL-Socket (%x)... ", sock->ssl);
		SSL_free(sock->ssl);
		SSL_CTX_free(sock->ctx);
		printout(vDEBUG, "done\n");
	}
#endif
	closesocket(sock->fd);
	free(sock);
}

/* =================================== *
 * ============ basic IO ============= *
 * =================================== */

/* reads a line from socket. terminating \r\n is replace by \0 */
/* error-levels: NULL (fatal read-error), ERR_TIMEOUT */
char * socket_read_line(wput_socket * sock) {
	int length  = 0;
	int bufsize = 82;
	char * buf  = malloc(bufsize+1);
	int res;
	
	while( (res = socket_read(sock, buf+length, 1)) > 0) {
		/* if multiline-messages can be like 220 Everything fine\nGreat\r\n
		 * reading the next answer will certainly fail and we'll get bug-reports */
		if(buf[length] == '\n')
			/* rfc states that lines have to end with CRLF. so remove the \r too */
			if(buf[length-1] == '\r') {
				buf[length-1] = 0;
				return buf;
			}

		if(length == bufsize) {
			bufsize <<= 1;
			buf = realloc(buf, bufsize+1);
		}
		length++;
	}
	/* this should never be reached. 
	 * however, make sure there is some debug-output if reading fails */
	buf[length] = 0;
	
	if(res == ERR_TIMEOUT) {
		printout(vNORMAL, _("Receive-Warning: read() timed out. Read '%s' so far.\n"), buf);
		free(buf);
		return (char *) ERR_TIMEOUT;
	}
	
	printout(vLESS, _("Receive-Error: read() failed. Read '%s' so far. (errno: %s (%d))\n"), buf, strerror(errno), errno);
	free(buf);
	return NULL;
}
/* recv, but take care of read-timeouts and read-interuptions */
/* error-levels: ERR_TIMEOUT, ERR_FAILED */
int socket_read(wput_socket * sock, void *buf, size_t len) {

  int res;
  /* TODO NRV looks like a possible bug to me. when there is no data pending,
   * TODO NRV but already data received (but not the complete ssl-block),
   * TODO NRV the next receive might fail, but we are in blocking read and
   * TODO NRV won't detect the connection-break-down. */
  if( 
#ifdef HAVE_SSL
	(sock->ssl && SSL_pending(sock->ssl) > 0) ||  
#endif
	socket_is_data_readable(sock->fd, default_timeout) ) ;
  else {
		printout(vNORMAL, _("Error: "));
		printout(vNORMAL, _("recv() timed out. No data received\n"));
		return ERR_TIMEOUT;
  }
  do {
#ifdef HAVE_SSL
    if(sock->ssl) res = SSL_read(sock->ssl, buf, len);
	else
#endif
	res = recv(sock->fd, buf, len, 0);
  } while( (res == -1 && errno == WPUT_EINTR) );
  //printout(vDEBUG, "fd: %d\tssl: %x\tbuf: %s\tlen:%d\n", fd, ssl, buf, len);
//  while ((res == -1 && errno == EINTR) || (res == -1 && errno == EINPROGRESS));
  if(res == 0) return ERR_FAILED;
  return res;
}
/* simple function to send through the socket
 * if ssl is available send through the ssl-module */
int socket_write(wput_socket * sock, void * buf, size_t len) {
  if(socket_is_data_writeable(sock->fd, default_timeout)) {
#ifdef HAVE_SSL
    if(sock->ssl) return SSL_write(sock->ssl, buf, len);
    else	
#endif
    return send(sock->fd, buf, len, 0);
  } else
    return ERR_FAILED;
}
/* =================================== *
 * ============= utils =============== *
 * =================================== */

int get_ip_addr(char* hostname, unsigned int * ip){ 
	struct hostent *ht;
#ifdef WIN32
	*ip = inet_addr(hostname);
	/* If we have an IP address we cannot resolve it as a hostname, 
	but this doesn't matter since we just need the ip. */
	if(*ip && *ip != 0xffffffff)
		return 0;
	else
#endif
	  ht = gethostbyname(hostname);
	
	if(ht != 0x0){
		printout(vDEBUG, "IP of `%s' is `%s'\n", hostname, printip((unsigned char *) ht->h_addr_list[0]));
		memcpy(ip, ht->h_addr_list[0], 4);
		return 0;
	}
	return ERR_FAILED;
}
#if 0
/* get interface-address of network interface ifname     *
 * return ip-address or 0 on error */
unsigned long get_ifaddr(char *ifname)
{
	int s;
	struct ifreq ifr;
	struct sockaddr_in sa;
	
	strcpy(ifr.ifr_name,ifname);
	s=socket(AF_INET,SOCK_DGRAM,0);
	if(s>=0) {
		if(ioctl(s,SIOCGIFADDR,&ifr)) {
			closesocket(s);
			return 0;
		}
		closesocket(s);
		memcpy(&sa,&ifr.ifr_addr,16);
		return ntohl(sa.sin_addr.s_addr);
	}
	return 0;
}
#endif
int get_local_ip(int sockfd, char * local_ip){
  struct sockaddr_in mysrv;
  struct sockaddr *myaddr;
  socklen_t addrlen = sizeof (mysrv); 
  myaddr = (struct sockaddr *) (&mysrv);
  if (getsockname (sockfd, myaddr, &addrlen) < 0)
    return ERR_FAILED;
  memcpy (local_ip, &mysrv.sin_addr, 4);
  return 0;
}

int socket_set_blocking(int sock, unsigned char block) {
#ifdef WIN32
  unsigned long flags = !block;
  return ioctlsocket(sock,FIONBIO,&flags)?-1:0;
#else
  int flags = fcntl(sock, F_GETFL, 0);
  if(block)
  	flags = flags & (O_NONBLOCK ^ 0xFFFFFFFF);
  else
  	flags |= O_NONBLOCK;
  if (fcntl(sock, F_SETFL, flags) < 0)
    return ERR_FAILED;
  return 0;
#endif
}

/* check whether a specific file-descriptor has data that can be read or written within timeout seconds */
int socket_is_data_writeable(int s, int timeout) {
	struct timeval t;
	int res = 0;
	fd_set inSet;
	FD_ZERO(&inSet);
	t.tv_sec = timeout / 10;
	t.tv_usec= (timeout % 10) * 100;
	FD_SET(s, &inSet);
	printout(vDEBUG, "Checking whether %d is writable... ", s);
	res = select(s+1, NULL, &inSet, NULL, &t);
	printout(vDEBUG, "%d (%d:%s)\n", res, errno, strerror(errno));
	if(errno > 0 && errno != EINPROGRESS)
		return 0;
	return res;
}
int socket_is_data_readable(int s, int timeout) {
      struct timeval t;
      fd_set inSet;
      FD_ZERO(&inSet);
      t.tv_sec = timeout / 10;
      t.tv_usec= (timeout % 10) * 100;
      FD_SET(s, &inSet);
      return select(s+1, &inSet, NULL, NULL, &t);
}

/* this is for not getting hanged up when a connection cannot be established in time */
wput_socket * socket_timeout_connect(wput_socket * sock, struct sockaddr *remote_addr, size_t size, int timeout) {
  int c = 0;
  printout(vDEBUG, "initiating timeout connect (%d)\n", timeout);
#ifdef WIN32
  /* reset errno before connecting. otherwise connection might "fail" for:
   * no such file or directory ;) */
  errno = 0;
#endif
  socket_set_blocking(sock->fd, 0);
  c = connect(sock->fd,remote_addr,size);
  if(errno > 0 && errno != EINPROGRESS) {
	printout(vMORE, "[%s]", strerror(errno));
	return NULL;
  }
  /* DEBUG if(c == -1) perror("connect"); */
  socket_set_blocking(sock->fd, 1);
  c = socket_is_data_writeable(sock->fd, timeout);
  return c < 1 ? NULL : sock;
}

/* =================================== *
 * ============= proxy =============== *
 * =================================== */

/* See the socks-proxy-rfc for reference to these codes */
/* as you can see, this code is quick and ugly. it shall simply work.
 * the intention was not to write a complete proxy-client but an piece
 * of ftp-software... */
/* error-levels: ERR_FAILED */
wput_socket * proxy_init(proxy_settings * ps) {
	/* TODO NRV add further authentication-methods support */
	char t[4] = {5, 1, 0};
	wput_socket * sock = socket_connect(ps->ip, ps->port);
	int res;
    printout(vDEBUG, "proxy-sock: %d\n", sock->fd);
	if(!sock) {
		printout(vNORMAL, _("failed.\n"));
		printout(vLESS, _("Error: "));
		printout(vLESS, _("Connection to proxy cannot be established.\n"));
		return NULL;
	}
    
    if(ps->user && ps->pass) {
        t[1] = 2;
        t[2] = 2;
        t[3] = 0;
    }
    
	send(sock->fd, t, strlen(t)+1, 0);
	res = socket_read(sock, t, 2);
	if(res < 0) {
		printout(vMORE, _("read() failed: %d (%d: %s)\n"), res, errno, strerror(errno));
#if 1 //__MSTC__, Dennis merge bugfix from TELEFONICA      
      // Memory leak problem, __TELEFONICA__, Mitrastar Smile, 20110824
      socket_close(sock);
#endif
		return NULL;
	}

	if(t[0] != 5) {
		printout(vNORMAL, _("failed.\n"));
		printout(vLESS, _("Error: "));
		printout(vLESS, _("Proxy version mismatch (%d)\n"), t[0]);
#if 1 //__MSTC__, Dennis merge bugfix from TELEFONICA      
      // Memory leak problem, __TELEFONICA__, Mitrastar Smile, 20110824
      socket_close(sock);
#endif
		return NULL;
	}
    if(t[1] == 2 && ps->user && ps->pass) {
        /* TODO USS it's a wonder that this code actually works. make it readable for heaven's sake. */
        char * p = malloc(strlen(ps->user) + strlen(ps->pass) + 3);
        p[0] = 5;
        p[1] = strlen(ps->user);
        strcpy(p+2, ps->user);
        p[2+p[1]] = strlen(ps->pass);
        strncpy(p+3+p[1], ps->pass, p[2+p[1]]);
        send(sock->fd, p, 3+p[1]+p[2+p[1]], 0);
        socket_read(sock, p, 2);
        if(p[1] != 0) {
            /* TODO NRV abort or return error-level on proxy-auth-failure? 
			 * TODO NRV abort is ok, since all other connections to this proxy
			 * TODO NRV will fail, too... but this would imply, that we'll need
			 * TODO NRV to change other proxy-errors to abort as well... */
            Abort(_("Proxy authentication failure\n"));
        }
        free(p);
    } else 
	if(t[1] != 0) {
		printout(vNORMAL, _("failed.\n"));
		printout(vLESS, _("Error: "));
		printout(vLESS, _("Proxy method mismatch (%d)\n"), t[1]);
#if 1 //__MSTC__, Dennis merge bugfix from TELEFONICA      
      // Memory leak problem, __TELEFONICA__, Mitrastar Smile, 20110824
      socket_close(sock);
#endif
		return NULL;
	}

	return sock;
}
/* SOCKS v5 Proxys can listen on a port for us. Use this great feature */
wput_socket * proxy_listen(proxy_settings * ps, unsigned int * ip, unsigned short * port) {
	/* v5, bind, rsv, ipv4, 0.0.0.0:0 */
	char t[10] = {5, 2, 0, 1, 0, 0, 0, 0, 0, 0};
	wput_socket * sock = proxy_init(ps);
	int res;
	if(!sock) return NULL;
	send(sock->fd, t, 10, 0);
	res = socket_read(sock, t, 10);
#if 1 //__MSTC__, Dennis merge bugfix from TELEFONICA      
      // Memory leak problem, __TELEFONICA__, Mitrastar Smile, 20110824
	if(res < 0){
       socket_close(sock);
	    return NULL;
	}
#else
	if(res < 0) return NULL;
#endif
	if(t[1] != 0) {
		printout(vLESS, _("Error: "));
		printout(vLESS, _("Proxy discarded listen-request. Error-Code: %d\n"), t[1]);
		printout(vMORE, _("Disabling listen-tries for proxy\n"));
		ps->bind = 0;
#if 1 //__MSTC__, Dennis merge bugfix from TELEFONICA      
      // Memory leak problem, __TELEFONICA__, Mitrastar Smile, 20110824
      socket_close(sock);
#endif
		return NULL;
	}
	*ip   = *(unsigned int *) (t+4);
	*port = *(unsigned short int *) (t+8);
	printout(vMORE, _("Proxy is listening on %s:%d for incoming connections\n"),  printip((unsigned char *) ip), *port);

	return sock;
}
/* accepts one incoming connection from a listening proxy_server.
 * the server-socket won't be valid anymore instead it is being transformed into
 * the client socket, since everything is on the same connection and there
 * is no way to accept more than one socket from a listening proxy */
wput_socket * proxy_accept(wput_socket * server) {
	char t[10];
	socket_read(server, t, 10);
	if(t[1] != 0) {
		printout(vLESS, _("Error: "));
		printout(vLESS, _("Proxy encountered an error while accepting. Error-Code: %d\n"), t[1]);
		return NULL;
	}
	/* this is all information we need. a \0 in t[1] is success,
	 * create a copy of the socket, so that the server-socket can be
	 * freed without trouble */
	printout(vDEBUG, "Proxy received an incoming connection on %s:%d.\n", printip((unsigned char *)t+4), *(unsigned short int *) (t+8));
	return server;	
}
/* quick and ugly implementation of v5/http proxy */
/* TODO IMP make proxy-implementation more relieable and maybe more read-/understandable */
wput_socket * proxy_connect(proxy_settings * ps, unsigned int ip, unsigned short port, const char * hostname) {
	int res;
    printout(vDEBUG, "Doing proxy connection\n");
	if(ps->type == PROXY_SOCKS) {
		wput_socket * sock = proxy_init(ps);
		char * t;
		if(!sock) return NULL;
		printout(vMORE, _("Using SOCKS5-Proxy %s:%d... "), printip((unsigned char *) &ps->ip), ps->port);
        
		/* if we could not resolve the hostname, let the proxy do it */
		if(ip == 0) {
			t = malloc(6 + strlen(hostname) + 1);
			/* v5, connect, rsv, hostname */
			printout(vDEBUG, "hostname: %s\n", hostname);
			memcpy(t, "\5\1\0\3", 4);
			t[4] = strlen(hostname);
			strcpy(t+5, hostname);
			res = htons(port);
			memcpy(t+strlen(hostname)+5, (char *) &res, 2);
			send(sock->fd, t, strlen(hostname)+7, 0);
			free(t);
			t = malloc(10);
		} else {
			t = malloc(10);
			/* v5, connect, rsv, ipv4, 0.0.0.0:0 */
			memcpy(t, "\5\1\0\1", 4);
            //res = htonl(ip);
			memcpy(t+4, &ip, 4);
            res = htons(port);
			memcpy(t+8, &res, 2);
			send(sock->fd, t, 10, 0);
		}
		res = socket_read(sock, t, 10);
		if(res < 0) {
			free(t);
#if 1 //__MSTC__, Dennis merge bugfix from TELEFONICA      
      // Memory leak problem, __TELEFONICA__, Mitrastar Smile, 20110824
         socket_close(sock);
#endif
			return NULL;
		}
		
		if(t[0] == 5 && t[1] == 0)
			printout(vMORE, _("Proxy connection established.\n"));
        else {
			printout(vLESS, _("Error: "));
			printout(vLESS, _("Connection through proxy failed. Error-code: %d\n"), t[1]);
			free(t);
#if 1 //__MSTC__, Dennis merge bugfix from TELEFONICA      
      // Memory leak problem, __TELEFONICA__, Mitrastar Smile, 20110824
         socket_close(sock);
#endif
			return NULL;
	    }
        free(t);
		return sock;
	} else {
		/* HTTP-proxy
		 * TODO USS proxy: error-handling
		 * TODO USS Proxy-Authentication has not yet been checked. Could someone report, if it works or not?
		 * TODO NRV SSL */
		wput_socket * sock = socket_connect(ps->ip, ps->port);
		char * userencoded = NULL;
		char * request;
        if(ps->user && ps->pass) {
            int len = strlen(ps->user) + strlen(ps->pass) + 3;
            char * userunencoded = (char *) malloc(len);
            strcpy(userunencoded, ps->user);
            strcpy(userunencoded+strlen(ps->user)+1, ps->pass);
            userunencoded[len-1] = 0; /* we need to have on char after our challenge to be 0 */
            userencoded = base64(userunencoded, len-1);
            free(userunencoded);
        }
        request = malloc(8 /* 'CONNECT ' */
            + (hostname ? strlen(hostname) : 15 /* '255.255.255.255' */)
            + (userencoded ? strlen(userencoded) + 29 : 0) /* '\r\nProxy-Authorization: Basic ' */
            + 10 + 5 + 5); /* ':' + (port (max '65535')) + ' HTTP/1.0' + '\r\n\r\n' + '\0' */
        strcpy(request, "CONNECT ");
		if(ip == 0)
            strcat(request, hostname);
		else
            strcat(request, printip((unsigned char *) &ip));
        strcat(request, ":");
        {
            char str_port[6];
			int64toa(port, str_port, 10);
            strcat(request, str_port);
        }
        strcat(request, " HTTP/1.0");
        if(userencoded) {
            strcat(request, "\r\nProxy-Authorization: Basic ");
            strcat(request, userencoded);
            free(userencoded);
        }
        strcat(request, "\r\n\r\n");
		printout(vDEBUG, "proxy-connect: '%s' (IP %x)\n", request, ip);
		send(sock->fd, request, strlen(request), 0);
		free(request);
        /* reading at most 512 bytes. i just hope that this tcp-paket does not include
		 * the first ftp-answer */
		request = malloc(512);
		request[0] = 0;
		res = socket_read(sock, request, 512);
		if(res < 0  || strncmp(request+9, "200", 3)) {
			printout(vLESS, _("Error: "));
			printout(vLESS, _("Connection could not be established.\nProxy states '%s'"), request);
			free(request);
#if 1 //__MSTC__, Dennis merge bugfix from TELEFONICA      
      // Memory leak problem, __TELEFONICA__, Mitrastar Smile, 20110824
         socket_close(sock);
#endif
			return NULL;
		}
        free(request);
		return sock;
	}
}
/* maybe not the most efficient method, but its enough
 * for our purposes (i spend about 20minutes figuring out the byte-shifting, where
 * i could have taken a sample source from the net, when my great provider
 * would not have forced to to stay offline for a while */
char * base64(char * p, size_t len) {
        int i = 0;
        int len2 = 3 * ((len+2) / 3);
        char * ret = malloc(len2 * 4 / 3);
        int bytepointer = 0;
        for(i = 0; i < len2 * 4 / 3; i++) {
                if((i+1) * 3 / 4 > len) {
                        ret[i] = '=';
                        continue;
                }
                ret[i] = (htons(*(int *) (p + bytepointer/8)) & (((1 << (10 - bytepointer%8))-1) ^ ((1 << (16 - bytepointer % 8))-1))) >> (10 - bytepointer%8);
                bytepointer+=6;
                if(ret[i] < 26)
                        ret[i] += 'A';
                else if(ret[i] < 52)
                        ret[i] += 'a' - 26;
                else if(ret[i] < 62)
                        ret[i] += '0' - 52;
                else if(ret[i] == 62)
                        ret[i] = '+';
                else
                        ret[i] = '/';
        }
	ret[i] = 0;
        return ret;
}
#ifdef WIN32
/* ssl for win32. load the functions from the dll */
void SSL_library_init(void)
{
	void * hLibSSL = LoadLibrary("SSLEAY32.DLL");

	if (!hLibSSL)
		hLibSSL = LoadLibrary("LIBSSL32.DLL");
	
	if (hLibSSL) {
		WSSL_library_init= (win_ssl_void_void) GetProcAddress(hLibSSL, "SSL_library_init");
		SSL_CTX_free = (win_ssl_pvoid_void) GetProcAddress(hLibSSL, "SSL_CTX_free");
		SSL_free = (win_ssl_pvoid_void) GetProcAddress(hLibSSL, "SSL_free");
		SSL_CTX_new = (win_ssl_pvoid_pvoid) GetProcAddress(hLibSSL, "SSL_CTX_new");
		WSSL_library_init = (win_ssl_void_void) GetProcAddress(hLibSSL, "SSL_library_init");
		SSLv23_client_method = (win_ssl_void_pvoid) GetProcAddress(hLibSSL, "SSLv23_method");
		SSL_new = (win_ssl_pvoid_pvoid) GetProcAddress(hLibSSL, "SSL_new");
		SSL_CTX_set_verify = (win_ssl_set_verify) GetProcAddress(hLibSSL, "SSL_CTX_set_verify");
		SSL_set_fd = (win_ssl_set_fd) GetProcAddress(hLibSSL, "SSL_set_fd");
		SSL_connect = (win_ssl_pvoid_int) GetProcAddress(hLibSSL, "SSL_connect");
		SSL_pending = (win_ssl_pvoid_int) GetProcAddress(hLibSSL, "SSL_pending");
		SSL_read = (win_ssl_read) GetProcAddress(hLibSSL, "SSL_read");
		SSL_write = (win_ssl_read) GetProcAddress(hLibSSL, "SSL_write");
	}
	/*
	printf("hLibSSL: %x\n", hLibSSL);
	printf("SSL_CTX_free: %x\n", SSL_CTX_free);
	printf("SSL_free: %x\n", SSL_free);
	printf("SSL_CTX_new: %x\n", SSL_CTX_new);
	printf("WSSL_library_init: %x\n", WSSL_library_init);
	printf("SSLv23_method: %x\n", SSLv23_method);
	printf("SSL_new: %x\n", SSL_new);
	printf("SSL_CTX_set_verify: %x\n", SSL_CTX_set_verify);
	printf("SSL_set_fd: %x\n", SSL_set_fd);
	printf("SSL_connect: %x\n", SSL_connect);
	printf("SSL_pending: %x\n", SSL_pending);
	printf("SSL_read: %x\n", SSL_read);
	printf("SSL_write: %x\n", SSL_write);
	*/
	if(hLibSSL && SSL_CTX_free && SSL_free && SSL_CTX_new && 
		WSSL_library_init && SSLv23_client_method && SSL_new && 
		SSL_CTX_set_verify && SSL_set_fd && SSL_connect &&
		SSL_pending && SSL_read && SSL_write) {
		WSSL_library_init();
		ssllib_in_use = 1;
	} else
		ssllib_in_use = 0;
	/*printf("ssllib_in_use: %x\n", ssllib_in_use);*/
}
#endif
