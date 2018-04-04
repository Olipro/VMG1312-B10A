#ifndef MSOCK_H
#define MSOCK_H

#ifdef WINNT
#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <windows.h>

#else

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>  /* ULTRIX didn't like stat with types*/
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <ctype.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>  /* for inet_ntoa */
#include <time.h>  /* for ctime */
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include <pwd.h>
#include <grp.h>
#include <fcntl.h>
#include <limits.h>


#undef SOCKET
#define SOCKET int

#undef INVALID_SOCKET
#define INVALID_SOCKET -1
#define _fileno fileno
#define _isatty isatty

#endif /* ! WINNT */

#ifdef HAVE_OPENSSL

#include <openssl/ssl.h>
#include <openssl/rand.h>
#include <openssl/err.h>

#endif /* HAVE_OPENSSL */

SOCKET clientSocket(char *,int);
int    sockGets(SOCKET,char *,size_t);
int    sockPuts(SOCKET sock,char *str);

void   msock_set_socket(SOCKET sock);
SOCKET msock_get_socket(void);
void   msock_turn_ssl_on(void);
void   msock_turn_ssl_off(void);
int    msock_is_ssl_on(void);
int    msock_gets(char *buf,size_t bufsiz);
int    msock_puts(char *str);
void   msock_close_socket(SOCKET fd);
void   msock_close(void);

#ifdef HAVE_OPENSSL

int    sockGetsSSL(SSL *ssl,char *buf,size_t count);
int    sockPutsSSL(SSL *ssl,char *str);
SSL    *msock_get_ssl(void);
void   msock_set_ssl(SSL *ssl);

#endif /* HAVE_OPENSSL */

#endif /* ! MSOCK_H */
