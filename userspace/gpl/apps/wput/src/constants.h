/* this defines some contants to be used in the different modules */
#ifndef __CONSTANTS_H
#define __CONSTANTS_H

#define BUFLEN 4096
#define TBUFLEN 64
#define FNLEN 768

/* TODO definitions:
   TODO URG urgent		no execuses! this needs to be done NOW
   TODO IMP important		put effort on getting this done.
   TODO PRI primary objective	i want this to work!
   TODO USS usual suspects	do it if you can afford getting it done or st. like that
   TODO NRV nonrelevant		might be done some day, if you feel lucky
 */
 
/* verbosity options */
#define vLESS   1
#define vNORMAL 2
#define vMORE   3
#define vDEBUG	4

#define ERR_NOERROR 0
#define ERR_OK 0

#define SOCK_ERROR(x) (x == ERR_RECONNECT || x == ERR_TIMEOUT)
#define FTP_ERROR(x) (x == ERR_RETRY || x == ERR_PERMANENT)

#define ERR_FAILED -1
#define ERR_SKIP -2
#define ERR_POSITIVE_PRELIMARY -2
#define ERR_RETRY -3
#define ERR_PERMANENT -4
#define ERR_RECONNECT -8
#define ERR_TIMEOUT -9

#define TYPE_UNDEFINED -1
#define TYPE_A          0
#define TYPE_I          1

/* definitions to find memory leaks and causes for segfaults. linked with memdbg.c */
//#define MEMDBG 
#ifdef MEMDBG

void dbg_free(void * ptr, char * file, int line);
void * dbg_realloc(void * ptr, size_t size, char * file, int line);
void * dbg_malloc(size_t size, char * file, int line);
int dbg_socket(int domain, int type, int protocol, char * file, int line);
int dbg_shutdown(int s, int how, char * file, int line);
int dbg_open(const char *path, int flags, char * file, int line);
int dbg_close(int fd, char * file, int line);
char * dbg_strcat(char * s, const char * p, char * file, int line);
char * dbg_cpy(char * s, char * file, int line);

#define malloc(x)       dbg_malloc(x, __FILE__, __LINE__)
#define realloc(x,y)    dbg_realloc(x,y, __FILE__, __LINE__)
#define free(x)         dbg_free(x, __FILE__, __LINE__)
#define socket(x,y,z)   dbg_socket(x,y,z, __FILE__, __LINE__)
#define shutdown(x,y)   dbg_shutdown(x,y, __FILE__, __LINE__)
#define open(x,y)       dbg_open(x,y, __FILE__, __LINE__)
/*#define close(x)        dbg_close(x, __FILE__, __LINE__)*/
#undef closesocket
#define closesocket(x)  dbg_close(x, __FILE__, __LINE__)
#define strcat(x,y)     dbg_strcat(x,y, __FILE__, __LINE__)
#define cpy(x)          dbg_cpy(x, __FILE__, __LINE__)
void print_unfree(void);
#endif

/* i18n */
#ifdef ENABLE_NLS
# define _(string) gettext (string)
# ifdef HAVE_LIBINTL_H
#  include <libintl.h>
# endif
#else  /* not HAVE_NLS */
# define _(string) string
#endif

#endif
