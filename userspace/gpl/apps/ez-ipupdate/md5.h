/* md5.h - Declaration of functions and data types used for MD5 sum
   computing library functions.
   Copyright (C) 1995, 1996 Free Software Foundation, Inc.
   NOTE: The canonical source of this file is maintained with the GNU C
   Library.  Bugs can be reported to bug-glibc@prep.ai.mit.edu.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2, or (at your option) any
   later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef _MD5_H
#define _MD5_H 1

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef USE_MD5

#define PARAMS(args) args

#include <stdio.h>

#if defined HAVE_LIMITS_H || _LIBC
# include <limits.h>
#endif

/* The following contortions are an attempt to use the C preprocessor
   to determine an unsigned integral type that is 32 bits wide.  An
   alternative approach is to use autoconf's AC_CHECK_SIZEOF macro, but
   doing that would require that the configure script compile and *run*
   the resulting executable.  Locally running cross-compiled executables
   is usually not possible.  */

#ifdef _LIBC
# include <sys/types.h>
typedef u_int32_t md5_uint32;
#else
# if defined __STDC__ && __STDC__
#  define UINT_MAX_32_BITS 4294967295U
# else
#  define UINT_MAX_32_BITS 0xFFFFFFFF
# endif

/* If UINT_MAX isn't defined, assume it's a 32-bit type.
   This should be valid for all systems GNU cares about because
   that doesn't include 16-bit systems, and only modern systems
   (that certainly have <limits.h>) have 64+-bit integral types.  */

# ifndef UINT_MAX
#  define UINT_MAX UINT_MAX_32_BITS
# endif

# if UINT_MAX == UINT_MAX_32_BITS
   typedef unsigned int md5_uint32;
# else
#  if USHRT_MAX == UINT_MAX_32_BITS
    typedef unsigned short md5_uint32;
#  else
#   if ULONG_MAX == UINT_MAX_32_BITS
     typedef unsigned long md5_uint32;
#   else
     /* The following line is intended to evoke an error.
        Using #error is not portable enough.  */
     "Cannot determine unsigned 32-bit data type."
#   endif
#  endif
# endif
#endif

/* Structure to save state of computation between the single steps.  */
struct md5_ctx
{
  md5_uint32 A;
  md5_uint32 B;
  md5_uint32 C;
  md5_uint32 D;

  md5_uint32 total[2];
  md5_uint32 buflen;
  char buffer[128];
};

/*
 * The following three functions are build up the low level used in
 * the functions `md5_stream' and `md5_buffer'.
 */

/* Initialize structure containing state of computation.
   (RFC 1321, 3.3: Step 3)  */
extern void md5_init_ctx PARAMS ((struct md5_ctx *ctx));

/* Starting with the result of former calls of this function (or the
   initialization function update the context for the next LEN bytes
   starting at BUFFER.
   It is necessary that LEN is a multiple of 64!!! */
extern void md5_process_block PARAMS ((const void *buffer, size_t len,
				       struct md5_ctx *ctx));

/* Starting with the result of former calls of this function (or the
   initialization function update the context for the next LEN bytes
   starting at BUFFER.
   It is NOT required that LEN is a multiple of 64.  */
extern void md5_process_bytes PARAMS ((const void *buffer, size_t len,
				       struct md5_ctx *ctx));

/* Process the remaining bytes in the buffer and put result from CTX
   in first 16 bytes following RESBUF.  The result is always in little
   endian byte order, so that a byte-wise output yields to the wanted
   ASCII representation of the message digest.

   IMPORTANT: On some systems it is required that RESBUF is correctly
   aligned for a 32 bits value.  */
extern void *md5_finish_ctx PARAMS ((struct md5_ctx *ctx, void *resbuf));


/* Put result from CTX in first 16 bytes following RESBUF.  The result is
   always in little endian byte order, so that a byte-wise output yields
   to the wanted ASCII representation of the message digest.

   IMPORTANT: On some systems it is required that RESBUF is correctly
   aligned for a 32 bits value.  */
extern void *md5_read_ctx PARAMS ((const struct md5_ctx *ctx, void *resbuf));


/* Compute MD5 message digest for bytes read from STREAM.  The
   resulting message digest number will be written into the 16 bytes
   beginning at RESBLOCK.  */
extern int md5_stream PARAMS ((FILE *stream, void *resblock));

/* Compute MD5 message digest for LEN bytes beginning at BUFFER.  The
   result is always in little endian byte order, so that a byte-wise
   output yields to the wanted ASCII representation of the message
   digest.  */
extern void *md5_buffer PARAMS ((const char *buffer, size_t len,
				 void *resblock));

#endif

#if 1 // __ZyXEL__, Albert, 20140115, add myzyxel.in.th for Thailand

#define LIST_CRITICAL_VALUE 265
#define LIST_ITEM_LEN 3
#define MD5_SIZE 16

/* POINTER defines a generic pointer type */
typedef unsigned char *POINTER;
typedef unsigned char uint8;		/* 8-bit unsigned integer  */
typedef unsigned short uint16;		/* 16-bit unsigned integer */
typedef unsigned int uint32;
typedef signed long sint31;

#ifdef PT_WLAN_AUTO_GENERATE
typedef signed char int8;
#endif

/* UINT2 defines a two byte word */
typedef unsigned short int UINT2;

/* UINT4 defines a four byte word */
typedef unsigned long int UINT4;
/* MD5 context. */

static unsigned char ThreeLetterList[][4]={
 "agn","aha","ake","aks","alm","alt","alv","and","ane","arm",
 "ask","asp","att","bag","bak","bie","bil","bit","bla","ble",
 "bli","bly","boa","bod","bok","bol","bom","bor","bra","bro",
 "bru","bud","bue","dal","dam","deg","der","det","din","dis",
 "dra","due","duk","dun","dyp","egg","eie","eik","elg","elv",
 "emu","ene","eng","enn","ert","ess","ete","ett","fei","fem",
 "fil","fin","flo","fly","for","fot","fra","fri","fus","fyr",
 "gen","gir","gla","gre","gro","gry","gul","hai","ham","han",
 "hav","hei","hel","her","hit","hiv","hos","hov","hue","huk",
 "hun","hus","hva","ide","ild","ile","inn","ion","ise","jag",
 "jeg","jet","jod","jus","juv","kai","kam","kan","kar","kle",
 "kli","klo","kna","kne","kok","kor","kro","kry","kul","kun",
 "kur","lad","lag","lam","lav","let","lim","lin","liv","lom",
 "los","lov","lue","lun","lur","lut","lyd","lyn","lyr","lys",
 "mai","mal","mat","med","meg","mel","men","mer","mil","min",
 "mot","mur","mye","myk","myr","nam","ned","nes","nok","nye",
 "nys","obo","obs","odd","ode","opp","ord","orm","ose","osp",
 "oss","ost","ovn","pai","par","pek","pen","pep","per","pip",
 "pop","rad","rak","ram","rar","ras","rem","ren","rev","rik",
 "rim","rir","ris","riv","rom","rop","ror","ros","rov","rur",
 "sag","sak","sal","sau","seg","sei","sel","sen","ses","sil",
 "sin","siv","sju","sjy","ski","sko","sky","smi","sne","snu",
 "sol","som","sot","spa","sti","sto","sum","sus","syd","syl",
 "syn","syv","tak","tal","tam","tau","tid","tie","til","tja",
 "tog","tom","tre","tue","tun","tur","uke","ull","ulv","ung",
 "uro","urt","ute","var","ved","veg","vei","vel","vev","vid",
 "vik","vis","vri","yre","yte"
};

typedef struct {
  UINT4 state[4];                                   /* state (ABCD) */
  UINT4 count[2];        /* number of bits, modulo 2^64 (lsb first) */
  unsigned char buffer[64];                         /* input buffer */
} MD5_CTX;

/* MD5 context for ez_ipupdate. */
typedef struct{
  uint32 state[4];				     /* state (ABCD) */
  uint32 count[2];	  /* number of bits, modulo 2^64 (lsb first) */
  unsigned char buffer[64];                         /* input buffer */
} MD5_CTX_ezipupdate;

void MD5Init_ezipupdate (MD5_CTX_ezipupdate *);
void MD5Update_ezipupdate (MD5_CTX_ezipupdate *, unsigned char *, unsigned int);
void MD5Final_ezipupdate (MD5_CTX_ezipupdate *, unsigned char [MD5_SIZE] );
void getMD5ExpectedContext_ezipupdate (char *input, char *expected );

int md5cInit(void);

void MD5Init  (MD5_CTX *);
void MD5Update (MD5_CTX *, unsigned char *, unsigned int);
void MD5Final  (unsigned char [16], MD5_CTX *);

void getMD5ExpectedContext (unsigned char  *input, unsigned char  *expected );
int dig2str( unsigned char 	*dig,unsigned char 	*str,uint16	dig_len);

#endif

#endif
