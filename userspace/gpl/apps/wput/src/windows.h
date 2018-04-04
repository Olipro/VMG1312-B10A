/* A few ideas for the windows-port of wput came from
   william <w99 _at_ terra.com _dot_ br>
   lots of thx for this contribution */
#ifndef __WIN_H
#define __WIN_H
#ifdef WIN32

/* undefine the default config.h to make compilation easier */
#undef HAVE_GETOPT_H
#undef HAVE_IOCTL
#undef HAVE_LOCALE_H
#undef ENABLE_NLS
#undef HAVE_STRINGS_H
#undef HAVE_STRING_H
#undef HAVE_UNISTD_H

#include <io.h>
#include <winsock2.h>
#include <winsock.h>

/* WINDOWS defines its UINT64 in basetsd.h */
#include <basetsd.h>
#include <sys/types.h>
/* WINDOWS does not know how to converte unsigned __int64 to double
	* therefore we have to cast it to signed first... */
#define WINCONV (signed __int64)
#define WPUTRC_FILENAME "wput.ini"

/* win-sleep uses milliseconds. on unix-platforms
   sleep meens seconds and usleep mikroseconds. so
   just do some conversion here */
#define usleep(X)     Sleep((X) / 1000)
#define sleep(X)      Sleep((X) * 1000)

//#define off_t __int64
#define size_t unsigned long
#define socklen_t unsigned long

/* we need to clean up the sockets and well just
   hope the compiler doesn't mind ;D */
#define exit(x) { WSACleanup(); exit(x); }

#define S_ISDIR(X)   ((X) & S_IFDIR)
#define S_ISCHR(X)   ((X) & S_IFCHR)
#define S_ISFIFO(X)  ((X) & S_IFIFO)
#define S_ISREG(X)   ((X) & S_IFREG)
#define S_ISREAD(X)  ((X) & S_IFREAD)
#define S_ISWRITE(X) ((X) & S_IFWRITE)
#define S_ISEXEC(X)  ((X) & S_IFEXEC)

#define strncasecmp strnicmp
#define strcasecmp  stricmp
#define popen       _popen
#define pclose      _pclose
#define isatty      _isatty
#define strtoll		strtol

void localtime_r(time_t * t, struct tm * res);

#define WPUT_EINTR WSAEINTR

/* SSL header fakes. the real functions are loaded from the 
 * dll in utils.c */
#define SSL_CTX void
#define SSL void
#define SSL_VERIFY_NONE	0

typedef void (*win_ssl_void_void) (void);
typedef void (*win_ssl_pvoid_void) (void * t);
typedef void * (*win_ssl_pvoid_pvoid) (void * t);
typedef void * (*win_ssl_void_pvoid) (void);
typedef int (*win_ssl_pvoid_int) (void * t);
typedef void (*win_ssl_set_verify) (void * t, int u, void * v);
typedef int (*win_ssl_read) (void * t, void * u, int v);
typedef int (*win_ssl_set_fd) (void * t, int u);
win_ssl_pvoid_void  SSL_CTX_free;
win_ssl_pvoid_void  SSL_free;
win_ssl_pvoid_pvoid SSL_CTX_new;
win_ssl_void_void   WSSL_library_init;
win_ssl_void_pvoid  SSLv23_client_method;
win_ssl_pvoid_pvoid SSL_new;
win_ssl_set_verify  SSL_CTX_set_verify;
win_ssl_pvoid_int   SSL_connect;
win_ssl_pvoid_int   SSL_pending;
win_ssl_read        SSL_read;
win_ssl_read        SSL_write;
win_ssl_set_fd      SSL_set_fd;

unsigned char ssllib_in_use;

#endif
#endif

