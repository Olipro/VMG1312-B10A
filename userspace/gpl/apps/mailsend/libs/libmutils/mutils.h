#ifndef MUTILS_H
#define MUTILS_H

#include <stdio.h>

#if STDC_HEADERS || HAVE_STRING_H
#include <string.h> /* ANSI string.h and pre-ANSI memory.h might conflict*/
#if !STDC_HEADERS && HAVE_MEMORY_H
#include <memory.h>
#endif
#else
#if  HAVE_STRINGS_H
#include <strings.h>
#endif
#endif


#if HAVE_CTYPE_H
#include <ctype.h>
#endif

#if HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#if SYS_UNIX
#include <sys/types.h>
#include <sys/stat.h>
#endif

#ifdef WINNT
#include <sys/types.h>
#include <sys/stat.h>
#include <io.h>
#include <share.h>
#define ftruncate chsize
#endif

#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#if HAVE_STDLIB_H 
#include <stdlib.h>
#endif

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#if HAVE_FCNTL_H
#ifndef O_RDONLY    /* prevent multiple inclusion on lame systems (from
vile)*/
#include <fcntl.h>
#endif
#endif

#if HAVE_MALLOC_H
#include <malloc.h>
#endif


#ifdef HAVE_SYS_FILE_H
#include <sys/file.h>
#endif

#if TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#else
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#else
#include <time.h>
#endif
#endif

#define MUTILS_CFL  __FILE__,__LINE__


#if __STDC__ || defined(sgi) || defined(_AIX)
#undef _Declare
#define _Declare(formal_parameters) formal_parameters
#else
#define _Declare(formal_parameters) ()
#define const
#endif

#define MUTILS_MAX_TOKEN_LEN     1024


#define MUTILS_CHECK_MALLOC(p) \
do \
{ \
    if (p == NULL) \
    {\
        (void) fprintf(stderr,"%s (%d) - memory allocation problem\n",__FILE__,__LINE__); \
        goto ExitProcessing; \
    }\
}while(0)



/* function prototypes */
void  mutilsBase64Encode    (FILE *ifp,FILE *ofp);
void  mutilsGenerateMIMEBoundary(char *boundary,int len);
int   mutilsParseURL        (char *url,char *hostname, 
                             int hostname_len, int *port,
                             char *page,int page_len);

void  mutilsStripLeadingSpace  (char *s);
void  mutilsStripTrailingSpace (char *s);
char  *mutilsReverseString  (char *str);
char  *mutilsStrncat        (char *dst,char *src,int n);
char  *mutilsStrncpy        (char *dsr,char *src,int n);
int   mutilsStrncasecmp     (char *s1,char *s2,int n);
char  *mutilsStrdup         (char *str);
int   mutilsStrcasecmp      (char *a,char *b);
void  mutilsSafeStrcpy      (char *dst,char *src,int n);
void  mutilsSafeStrcat      (char *dsr,char *src,int n,int ss,int sl);
char  *mutilsStrtok         (char *s,char *delim);
int   mutilsHowmanyCommas   (char *buf);
void  mutilsCommaize        (char *buf);
void  mutilsCleanBuf        (char *buf,int bufsize,int *length);
char  *mutilsRmallws        (char *str);
char  *mutilsStristr        (char *s,char *t);
int   mutilsIsinname        (char *string,char *mask);
char  *mutilsGetTime        (void);
char  mutilsChopNL          (char *str);
int  mutilsTmpFilename     (char *filename);
char  *mutilsBasename       (char *path);
int   mutilsWhich           (char *name);
void  mutilsSetLock         (int fd);
void  mutilsDotLock         (char *filepath,char *errbuf);
void  mutilsDotUnlock       (int delete);
char  *mutilsStrUpper       (char *str);
char  *mutilsStrLower       (char *str);
int   mutilsEatComment      (FILE *fp);
int   mutilsEatWhitespace   (FILE *fp);
char  *mutilsGetDirname     (char *file);
char  *mutilsSpacesToChar   (char *str,int c);
char  **mutilsTokenize(char *str,int delim,int *ntokens);
void  mutilsFreeTokens(char **tokens,int ntokens);
unsigned char *mutils_encode_base64(void *src,unsigned long srcl,unsigned long *len);
void *mutils_decode_base64(unsigned char *src,unsigned long srcl,unsigned long *len);


#endif /* MUTILS_H */
