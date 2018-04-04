#include "wput.h"
#include "constants.h"
#define WPUT 1

/* Dummy function macros for ftp-ls.c. */

#define fgetc( x) nextchr()
#define fopen( x, y) stdin
#define fclose( x)
#define fflush( x)
#define xfree( x)

/* Real function macros for ftp-ls.c. */

#define xmalloc malloc
#define xstrdup strdup

#define FREE_MAYBE(foo) do { if (foo) xfree (foo); } while (0)

#ifndef WIN32
#  define WGET_PRINTOUT( ...) printout( vDEBUG, __VA_ARGS__)
#  define logprintf( x, ...) printout( vDEBUG, __VA_ARGS__)
#  define DEBUGP( x) WGET_PRINTOUT x;
#else
#  define DEBUGP( x)
#  define logprintf 0 && 
#endif

#define ISDIGIT(x) ((x) >= '0' && (x) <= '9')

#define TRUE 1
#define FALSE 0

enum log_options { LOG_VERBOSE, LOG_NOTQUIET, LOG_NONVERBOSE, LOG_ALWAYS };

char *read_whole_line( FILE *);

char nextchr();
