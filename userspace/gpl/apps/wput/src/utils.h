#ifndef __UTILS_H
#define __UTILS_H

#include "wput.h"

#include <stdarg.h>

#ifndef WIN32
 //#include <netdb.h>
 #include <sys/time.h>
 #define TIMER_GETTIMEOFDAY defined (__stub_gettimeofday) || defined (__stub___gettimeofday)
 #ifndef isspace
 int isspace(int c);
 #endif
#else
 char * win32_replace_dirsep(char * p);
#endif


unsigned char get_filemode(char * filename);
char * get_port_fmt(int ip, unsigned int port);

char * home_dir (void);
int file_exists (const char * filename);
char * read_line (FILE *fp);
char * basename(char * p);
void clear_path(char * path);
char * get_relative_path(char * src, char * dst);

void Abort(char * msg);
void printout(unsigned char verbose, const char * fmt, ...);

/* it is not included in c-libraries (AFAIK)...
 * only windows knows about it, so we have to take a different name then */
char * int64toa(off_t, char *, int);

#ifndef MEMDBG
char * cpy(char * s);
#endif

char * unescape(char * str);
void parse_passive_string(char * msg, unsigned int * ip, unsigned short int * port);
char * legible (off_t l);
int    numdigit (long number);
char * printip(unsigned char * ip);

void   retry_wait(_fsession * fsession);

int    parse_url(_fsession * fsession, char *url);
void   parse_proxy(char * url);
char * snip_basename(char * file);
#endif
