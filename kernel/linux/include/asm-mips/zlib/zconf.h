#ifndef ZCONF_H
#define ZCONF_H

/* Make aware of modifcation to codes */
#define CONFIG_MSTC_ZLIB_

/* Maximum value for memLevel in deflateInit2 */
#ifndef MAX_MEM_LEVEL
#  ifdef MAXSEG_64K
#    define MAX_MEM_LEVEL 8
#  else
#    define MAX_MEM_LEVEL 9
#  endif
#endif

/* Maximum value for windowBits in deflateInit2 and inflateInit2.
 * WARNING: reducing MAX_WBITS makes minigzip unable to extract .gz files
 * created by gzip. (Files created by minigzip can still be extracted by
 * gzip.)
 */
#ifndef MAX_WBITS
#  define MAX_WBITS   15 /* 32K LZ77 window */
#endif

#ifndef ZEXTERN
#  define ZEXTERN extern
#endif
#ifndef ZEXPORT
#  define ZEXPORT
#endif
#ifndef ZEXPORTVA
#  define ZEXPORTVA
#endif

#define OF(args)  args
#define FAR

typedef unsigned char       Byte;  /* 8 bits */
typedef unsigned int        uInt;  /* 16 bits or more */
typedef unsigned long       uLong; /* 32 bits or more */
typedef Byte  FAR           Bytef;
typedef char  FAR           charf;
typedef int   FAR           intf;
typedef uInt  FAR           uIntf;
typedef uLong FAR           uLongf;
typedef void const *voidpc;
typedef void FAR   *voidpf;
typedef void       *voidp;

#define z_off_t    long /* off_t <kernel.h> : <sys/types.h>*/

#endif /* ZCONF_H */
