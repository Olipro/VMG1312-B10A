/* md5.c - Functions to compute MD5 message digest of files or memory blocks
   according to the definition of MD5 in RFC 1321 from April 1992.
   Copyright (C) 1995, 1996 Free Software Foundation, Inc.
   This file is part of the GNU C library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the GNU C Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* Written by Ulrich Drepper <drepper@gnu.ai.mit.edu>, 1995.  */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef USE_MD5

#include <stdlib.h>
#ifdef HAVE_STRING_H
# include <string.h>
#else
# include <strings.h>
#endif

#include "md5.h"

#ifdef _LIBC
# include <endian.h>
# if __BYTE_ORDER == __BIG_ENDIAN
#  define WORDS_BIGENDIAN 1
# endif
#endif

#if defined(__BYTE_ORDER) && __BYTE_ORDER == __BIG_ENDIAN
# define EZ_BIG_ENDIAN 1
#elif (defined(__BYTE_ORDER) && __BYTE_ORDER == __LITTLE_ENDIAN) || defined(__386__)
# define EZ_BIG_ENDIAN 0
#endif

#if defined(WORDS_BIGENDIAN) || defined(EZ_BIG_ENDIAN)
# define SWAP(n)							\
    (((n) << 24) | (((n) & 0xff00) << 8) | (((n) >> 8) & 0xff00) | ((n) >> 24))
#else
# define SWAP(n) (n)
#endif

#if 1// __ZyXEL__, Albert, 20140115, add myzyxel.in.th for Thailand

#define S11 7
#define S12 12
#define S13 17
#define S14 22
#define S21 5
#define S22 9
#define S23 14
#define S24 20
#define S31 4
#define S32 11
#define S33 16
#define S34 23
#define S41 6
#define S42 10
#define S43 15
#define S44 21

/* F, G, H and I are basic MD5 functions.
 */
#define ZYXEL_F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define ZYXEL_G(x, y, z) (((x) & (z)) | ((y) & (~z)))
#define ZYXEL_H(x, y, z) ((x) ^ (y) ^ (z))
#define ZYXEL_I(x, y, z) ((y) ^ ((x) | (~z)))

/* ROTATE_LEFT rotates x left n bits.
 */
#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32-(n))))

/* FF, GG, HH, and II transformations for rounds 1, 2, 3, and 4.
Rotation is separate from addition to prevent recomputation.
 */
#define ZYXEL_FF(a, b, c, d, x, s, ac) { \
 (a) += ZYXEL_F ((b), (c), (d)) + (x) + (uint32)(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
  }
#define ZYXEL_GG(a, b, c, d, x, s, ac) { \
 (a) += ZYXEL_G ((b), (c), (d)) + (x) + (uint32)(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
  }
#define ZYXEL_HH(a, b, c, d, x, s, ac) { \
 (a) += ZYXEL_H ((b), (c), (d)) + (x) + (uint32)(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
  }
#define ZYXEL_II(a, b, c, d, x, s, ac) { \
 (a) += ZYXEL_I ((b), (c), (d)) + (x) + (uint32)(ac); \
 (a) = ROTATE_LEFT ((a), (s)); \
 (a) += (b); \
  }

static unsigned char PADDING[64] = {
  0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

#endif

/* This array contains the bytes used to pad the buffer to the next
   64-byte boundary.  (RFC 1321, 3.1: Step 1)  */
static const unsigned char fillbuf[64] = { 0x80, 0 /* , 0, 0, ...  */ };


/* Initialize structure containing state of computation.
   (RFC 1321, 3.3: Step 3)  */
void
md5_init_ctx (struct md5_ctx *ctx)
{
  ctx->A = 0x67452301;
  ctx->B = 0xefcdab89;
  ctx->C = 0x98badcfe;
  ctx->D = 0x10325476;

  ctx->total[0] = ctx->total[1] = 0;
  ctx->buflen = 0;
}

/* Put result from CTX in first 16 bytes following RESBUF.  The result
   must be in little endian byte order.

   IMPORTANT: On some systems it is required that RESBUF is correctly
   aligned for a 32 bits value.  */
void *
md5_read_ctx (const struct md5_ctx *ctx, void *resbuf)
{
  ((md5_uint32 *) resbuf)[0] = SWAP (ctx->A);
  ((md5_uint32 *) resbuf)[1] = SWAP (ctx->B);
  ((md5_uint32 *) resbuf)[2] = SWAP (ctx->C);
  ((md5_uint32 *) resbuf)[3] = SWAP (ctx->D);

  return resbuf;
}

/* Process the remaining bytes in the internal buffer and the usual
   prolog according to the standard and write the result to RESBUF.

   IMPORTANT: On some systems it is required that RESBUF is correctly
   aligned for a 32 bits value.  */
void *
md5_finish_ctx (struct md5_ctx *ctx, void *resbuf)
{
  /* Take yet unprocessed bytes into account.  */
  md5_uint32 bytes = ctx->buflen;
  size_t pad;

  /* Now count remaining bytes.  */
  ctx->total[0] += bytes;
  if (ctx->total[0] < bytes)
    ++ctx->total[1];

  pad = bytes >= 56 ? 64 + 56 - bytes : 56 - bytes;
  memcpy (&ctx->buffer[bytes], fillbuf, pad);

  /* Put the 64-bit file length in *bits* at the end of the buffer.  */
  *(md5_uint32 *) &ctx->buffer[bytes + pad] = SWAP (ctx->total[0] << 3);
  *(md5_uint32 *) &ctx->buffer[bytes + pad + 4] = SWAP ((ctx->total[1] << 3) |
							(ctx->total[0] >> 29));

  /* Process last bytes.  */
  md5_process_block (ctx->buffer, bytes + pad + 8, ctx);

  return md5_read_ctx (ctx, resbuf);
}

/* Compute MD5 message digest for bytes read from STREAM.  The
   resulting message digest number will be written into the 16 bytes
   beginning at RESBLOCK.  */
int
md5_stream (FILE *stream, void *resblock)
{
  /* Important: BLOCKSIZE must be a multiple of 64.  */
#define BLOCKSIZE 4096
  struct md5_ctx ctx;
  char buffer[BLOCKSIZE + 72];
  size_t sum;

  /* Initialize the computation context.  */
  md5_init_ctx (&ctx);

  /* Iterate over full file contents.  */
  while (1)
    {
      /* We read the file in blocks of BLOCKSIZE bytes.  One call of the
	 computation function processes the whole buffer so that with the
	 next round of the loop another block can be read.  */
      size_t n;
      sum = 0;

      /* Read block.  Take care for partial reads.  */
      do
	{
	  n = fread (buffer + sum, 1, BLOCKSIZE - sum, stream);

	  sum += n;
	}
      while (sum < BLOCKSIZE && n != 0);
      if (n == 0 && ferror (stream))
        return 1;

      /* If end of file is reached, end the loop.  */
      if (n == 0)
	break;

      /* Process buffer with BLOCKSIZE bytes.  Note that
			BLOCKSIZE % 64 == 0
       */
      md5_process_block (buffer, BLOCKSIZE, &ctx);
    }

  /* Add the last bytes if necessary.  */
  if (sum > 0)
    md5_process_bytes (buffer, sum, &ctx);

  /* Construct result in desired memory.  */
  md5_finish_ctx (&ctx, resblock);
  return 0;
}

/* Compute MD5 message digest for LEN bytes beginning at BUFFER.  The
   result is always in little endian byte order, so that a byte-wise
   output yields to the wanted ASCII representation of the message
   digest.  */
void *
md5_buffer (const char *buffer, size_t len, void *resblock)
{
  struct md5_ctx ctx;

  /* Initialize the computation context.  */
  md5_init_ctx (&ctx);

  /* Process whole buffer but last len % 64 bytes.  */
  md5_process_bytes (buffer, len, &ctx);

  /* Put result in desired memory area.  */
  return md5_finish_ctx (&ctx, resblock);
}


void
md5_process_bytes (const void *buffer, size_t len, struct md5_ctx *ctx)
{
  /* When we already have some bits in our internal buffer concatenate
     both inputs first.  */
  if (ctx->buflen != 0)
    {
      size_t left_over = ctx->buflen;
      size_t add = 128 - left_over > len ? len : 128 - left_over;

      memcpy (&ctx->buffer[left_over], buffer, add);
      ctx->buflen += add;

      if (left_over + add > 64)
	{
	  md5_process_block (ctx->buffer, (left_over + add) & ~63, ctx);
	  /* The regions in the following copy operation cannot overlap.  */
	  memcpy (ctx->buffer, &ctx->buffer[(left_over + add) & ~63],
		  (left_over + add) & 63);
	  ctx->buflen = (left_over + add) & 63;
	}

      buffer = (const char *) buffer + add;
      len -= add;
    }

  /* Process available complete blocks.  */
  if (len > 64)
    {
      md5_process_block (buffer, len & ~63, ctx);
      buffer = (const char *) buffer + (len & ~63);
      len &= 63;
    }

  /* Move remaining bytes in internal buffer.  */
  if (len > 0)
    {
      memcpy (ctx->buffer, buffer, len);
      ctx->buflen = len;
    }
}


/* These are the four functions used in the four steps of the MD5 algorithm
   and defined in the RFC 1321.  The first function is a little bit optimized
   (as found in Colin Plumbs public domain implementation).  */
/* #define FF(b, c, d) ((b & c) | (~b & d)) */
#define FF(b, c, d) (d ^ (b & (c ^ d)))
#define FG(b, c, d) FF (d, b, c)
#define FH(b, c, d) (b ^ c ^ d)
#define FI(b, c, d) (c ^ (b | ~d))

/* Process LEN bytes of BUFFER, accumulating context into CTX.
   It is assumed that LEN % 64 == 0.  */

void
md5_process_block (const void *buffer, size_t len, struct md5_ctx *ctx)
{
  md5_uint32 correct_words[16];
  const md5_uint32 *words = buffer;
  size_t nwords = len / sizeof (md5_uint32);
  const md5_uint32 *endp = words + nwords;
  md5_uint32 A = ctx->A;
  md5_uint32 B = ctx->B;
  md5_uint32 C = ctx->C;
  md5_uint32 D = ctx->D;

  /* First increment the byte count.  RFC 1321 specifies the possible
     length of the file up to 2^64 bits.  Here we only compute the
     number of bytes.  Do a double word increment.  */
  ctx->total[0] += len;
  if (ctx->total[0] < len)
    ++ctx->total[1];

  /* Process all bytes in the buffer with 64 bytes in each round of
     the loop.  */
  while (words < endp)
    {
      md5_uint32 *cwp = correct_words;
      md5_uint32 A_save = A;
      md5_uint32 B_save = B;
      md5_uint32 C_save = C;
      md5_uint32 D_save = D;

      /* First round: using the given function, the context and a constant
	 the next context is computed.  Because the algorithms processing
	 unit is a 32-bit word and it is determined to work on words in
	 little endian byte order we perhaps have to change the byte order
	 before the computation.  To reduce the work for the next steps
	 we store the swapped words in the array CORRECT_WORDS.  */

#define OP(a, b, c, d, s, T)						\
      do								\
        {								\
	  a += FF (b, c, d) + (*cwp++ = SWAP (*words)) + T;		\
	  ++words;							\
	  CYCLIC (a, s);						\
	  a += b;							\
        }								\
      while (0)

      /* It is unfortunate that C does not provide an operator for
	 cyclic rotation.  Hope the C compiler is smart enough.  */
#define CYCLIC(w, s) (w = (w << s) | (w >> (32 - s)))

      /* Before we start, one word to the strange constants.
	 They are defined in RFC 1321 as

	 T[i] = (int) (4294967296.0 * fabs (sin (i))), i=1..64
       */

      /* Round 1.  */
      OP (A, B, C, D,  7, 0xd76aa478);
      OP (D, A, B, C, 12, 0xe8c7b756);
      OP (C, D, A, B, 17, 0x242070db);
      OP (B, C, D, A, 22, 0xc1bdceee);
      OP (A, B, C, D,  7, 0xf57c0faf);
      OP (D, A, B, C, 12, 0x4787c62a);
      OP (C, D, A, B, 17, 0xa8304613);
      OP (B, C, D, A, 22, 0xfd469501);
      OP (A, B, C, D,  7, 0x698098d8);
      OP (D, A, B, C, 12, 0x8b44f7af);
      OP (C, D, A, B, 17, 0xffff5bb1);
      OP (B, C, D, A, 22, 0x895cd7be);
      OP (A, B, C, D,  7, 0x6b901122);
      OP (D, A, B, C, 12, 0xfd987193);
      OP (C, D, A, B, 17, 0xa679438e);
      OP (B, C, D, A, 22, 0x49b40821);

      /* For the second to fourth round we have the possibly swapped words
	 in CORRECT_WORDS.  Redefine the macro to take an additional first
	 argument specifying the function to use.  */
#undef OP
#define OP(f, a, b, c, d, k, s, T)					\
      do 								\
	{								\
	  a += f (b, c, d) + correct_words[k] + T;			\
	  CYCLIC (a, s);						\
	  a += b;							\
	}								\
      while (0)

      /* Round 2.  */
      OP (FG, A, B, C, D,  1,  5, 0xf61e2562);
      OP (FG, D, A, B, C,  6,  9, 0xc040b340);
      OP (FG, C, D, A, B, 11, 14, 0x265e5a51);
      OP (FG, B, C, D, A,  0, 20, 0xe9b6c7aa);
      OP (FG, A, B, C, D,  5,  5, 0xd62f105d);
      OP (FG, D, A, B, C, 10,  9, 0x02441453);
      OP (FG, C, D, A, B, 15, 14, 0xd8a1e681);
      OP (FG, B, C, D, A,  4, 20, 0xe7d3fbc8);
      OP (FG, A, B, C, D,  9,  5, 0x21e1cde6);
      OP (FG, D, A, B, C, 14,  9, 0xc33707d6);
      OP (FG, C, D, A, B,  3, 14, 0xf4d50d87);
      OP (FG, B, C, D, A,  8, 20, 0x455a14ed);
      OP (FG, A, B, C, D, 13,  5, 0xa9e3e905);
      OP (FG, D, A, B, C,  2,  9, 0xfcefa3f8);
      OP (FG, C, D, A, B,  7, 14, 0x676f02d9);
      OP (FG, B, C, D, A, 12, 20, 0x8d2a4c8a);

      /* Round 3.  */
      OP (FH, A, B, C, D,  5,  4, 0xfffa3942);
      OP (FH, D, A, B, C,  8, 11, 0x8771f681);
      OP (FH, C, D, A, B, 11, 16, 0x6d9d6122);
      OP (FH, B, C, D, A, 14, 23, 0xfde5380c);
      OP (FH, A, B, C, D,  1,  4, 0xa4beea44);
      OP (FH, D, A, B, C,  4, 11, 0x4bdecfa9);
      OP (FH, C, D, A, B,  7, 16, 0xf6bb4b60);
      OP (FH, B, C, D, A, 10, 23, 0xbebfbc70);
      OP (FH, A, B, C, D, 13,  4, 0x289b7ec6);
      OP (FH, D, A, B, C,  0, 11, 0xeaa127fa);
      OP (FH, C, D, A, B,  3, 16, 0xd4ef3085);
      OP (FH, B, C, D, A,  6, 23, 0x04881d05);
      OP (FH, A, B, C, D,  9,  4, 0xd9d4d039);
      OP (FH, D, A, B, C, 12, 11, 0xe6db99e5);
      OP (FH, C, D, A, B, 15, 16, 0x1fa27cf8);
      OP (FH, B, C, D, A,  2, 23, 0xc4ac5665);

      /* Round 4.  */
      OP (FI, A, B, C, D,  0,  6, 0xf4292244);
      OP (FI, D, A, B, C,  7, 10, 0x432aff97);
      OP (FI, C, D, A, B, 14, 15, 0xab9423a7);
      OP (FI, B, C, D, A,  5, 21, 0xfc93a039);
      OP (FI, A, B, C, D, 12,  6, 0x655b59c3);
      OP (FI, D, A, B, C,  3, 10, 0x8f0ccc92);
      OP (FI, C, D, A, B, 10, 15, 0xffeff47d);
      OP (FI, B, C, D, A,  1, 21, 0x85845dd1);
      OP (FI, A, B, C, D,  8,  6, 0x6fa87e4f);
      OP (FI, D, A, B, C, 15, 10, 0xfe2ce6e0);
      OP (FI, C, D, A, B,  6, 15, 0xa3014314);
      OP (FI, B, C, D, A, 13, 21, 0x4e0811a1);
      OP (FI, A, B, C, D,  4,  6, 0xf7537e82);
      OP (FI, D, A, B, C, 11, 10, 0xbd3af235);
      OP (FI, C, D, A, B,  2, 15, 0x2ad7d2bb);
      OP (FI, B, C, D, A,  9, 21, 0xeb86d391);

      /* Add the starting values of the context.  */
      A += A_save;
      B += B_save;
      C += C_save;
      D += D_save;
    }

  /* Put checksum in context given as argument.  */
  ctx->A = A;
  ctx->B = B;
  ctx->C = C;
  ctx->D = D;
}

#if 1 // __ZyXEL__, Albert, 20140115, add myzyxel.in.th for Thailand
void MD5Init_ezipupdate (
	MD5_CTX_ezipupdate *context                              /* context */
)
{
  context->count[0] = context->count[1] = 0;
  /* Load magic initialization constants. */
  context->state[0] = 0x67452301;
  context->state[1] = 0xefcdab89;
  context->state[2] = 0x98badcfe;
  context->state[3] = 0x10325476;
}

void MD5Update_ezipupdate(
	MD5_CTX_ezipupdate *context,                  /* context */
	unsigned char *input,              /* input block */
	unsigned int inputLen              /* length of input block */
)
{
  unsigned int i, index, partLen;

  /* Compute number of bytes mod 64 */
  index = (unsigned int)((context->count[0] >> 3) & 0x3F);

  /* Update number of bits */
  if ((context->count[0] += ((uint32)inputLen << 3))
   < ((uint32)inputLen << 3))
    context->count[1]++;
  context->count[1] += ((uint32)inputLen >> 29);

  partLen = 64 - index;

  /* Transform as many times as possible. 
*/
  if (inputLen >= partLen) {
    MD5_memcpy
     ((POINTER)&context->buffer[index], (POINTER)input, partLen);
    MD5Transform_ezipupdate(context->state, context->buffer);

    for (i = partLen; i + 63 < inputLen; i += 64)
      MD5Transform_ezipupdate (context->state, &input[i]);

    index = 0;
  }
  else
 i = 0;

  /* Buffer remaining input */
  MD5_memcpy
 ((POINTER)&context->buffer[index], (POINTER)&input[i],
  inputLen-i);
}

/* Decodes input (unsigned char) into output (uint32). Assumes len is
  a multiple of 4.
 */
 void Decode_ezipupdate(
	uint32 *output,
	unsigned char *input,
	unsigned int len
)
{
  unsigned int i, j;

  for (i = 0, j = 0; j < len; i++, j += 4)
 output[i] = ((uint32)input[j]) | (((uint32)input[j+1]) << 8) |
   (((uint32)input[j+2]) << 16) | (((uint32)input[j+3]) << 24);
}

/* Note: Replace "for loop" with standard memcpy if possible.
 */

void MD5_memcpy (
POINTER output,
POINTER input,
unsigned int len)
{
  unsigned int i;

  for (i = 0; i < len; i++)
 output[i] = input[i];
}

/* Note: Replace "for loop" with standard memset if possible.
 */
void MD5_memset (
POINTER output,
int value,
unsigned int len)
{
  unsigned int i;

  for (i = 0; i < len; i++)
 ((char *)output)[i] = (char)value;
}

/* MD5 basic transformation. Transforms state based on block.
 */
void MD5Transform_ezipupdate (
	uint32 state[4],
	unsigned char block[64]
)
{
  uint32 a = state[0], b = state[1], c = state[2], d = state[3], x[16];

  Decode_ezipupdate (x, block, 64);

   /* Round 1 */
  ZYXEL_FF (a, b, c, d, x[ 0], S11, 0xd76aa478); /* 1 */
  ZYXEL_FF (d, a, b, c, x[ 1], S12, 0xe8c7b756); /* 2 */
  ZYXEL_FF (c, d, a, b, x[ 2], S13, 0x242070db); /* 3 */
  ZYXEL_FF (b, c, d, a, x[ 3], S14, 0xc1bdceee); /* 4 */
  ZYXEL_FF (a, b, c, d, x[ 4], S11, 0xf57c0faf); /* 5 */
  ZYXEL_FF (d, a, b, c, x[ 5], S12, 0x4787c62a); /* 6 */
  ZYXEL_FF (c, d, a, b, x[ 6], S13, 0xa8304613); /* 7 */
  ZYXEL_FF (b, c, d, a, x[ 7], S14, 0xfd469501); /* 8 */
  ZYXEL_FF (a, b, c, d, x[ 8], S11, 0x698098d8); /* 9 */
  ZYXEL_FF (d, a, b, c, x[ 9], S12, 0x8b44f7af); /* 10 */
  ZYXEL_FF (c, d, a, b, x[10], S13, 0xffff5bb1); /* 11 */
  ZYXEL_FF (b, c, d, a, x[11], S14, 0x895cd7be); /* 12 */
  ZYXEL_FF (a, b, c, d, x[12], S11, 0x6b901122); /* 13 */
  ZYXEL_FF (d, a, b, c, x[13], S12, 0xfd987193); /* 14 */
  ZYXEL_FF (c, d, a, b, x[14], S13, 0xa679438e); /* 15 */
  ZYXEL_FF (b, c, d, a, x[15], S14, 0x49b40821); /* 16 */

 /* Round 2 */
  ZYXEL_GG (a, b, c, d, x[ 1], S21, 0xf61e2562); /* 17 */
  ZYXEL_GG (d, a, b, c, x[ 6], S22, 0xc040b340); /* 18 */
  ZYXEL_GG (c, d, a, b, x[11], S23, 0x265e5a51); /* 19 */
  ZYXEL_GG (b, c, d, a, x[ 0], S24, 0xe9b6c7aa); /* 20 */
  ZYXEL_GG (a, b, c, d, x[ 5], S21, 0xd62f105d); /* 21 */
  ZYXEL_GG (d, a, b, c, x[10], S22,  0x2441453); /* 22 */
  ZYXEL_GG (c, d, a, b, x[15], S23, 0xd8a1e681); /* 23 */
  ZYXEL_GG (b, c, d, a, x[ 4], S24, 0xe7d3fbc8); /* 24 */
  ZYXEL_GG (a, b, c, d, x[ 9], S21, 0x21e1cde6); /* 25 */
  ZYXEL_GG (d, a, b, c, x[14], S22, 0xc33707d6); /* 26 */
  ZYXEL_GG (c, d, a, b, x[ 3], S23, 0xf4d50d87); /* 27 */
  ZYXEL_GG (b, c, d, a, x[ 8], S24, 0x455a14ed); /* 28 */
  ZYXEL_GG (a, b, c, d, x[13], S21, 0xa9e3e905); /* 29 */
  ZYXEL_GG (d, a, b, c, x[ 2], S22, 0xfcefa3f8); /* 30 */
  ZYXEL_GG (c, d, a, b, x[ 7], S23, 0x676f02d9); /* 31 */
  ZYXEL_GG (b, c, d, a, x[12], S24, 0x8d2a4c8a); /* 32 */

  /* Round 3 */
  ZYXEL_HH (a, b, c, d, x[ 5], S31, 0xfffa3942); /* 33 */
  ZYXEL_HH (d, a, b, c, x[ 8], S32, 0x8771f681); /* 34 */
  ZYXEL_HH (c, d, a, b, x[11], S33, 0x6d9d6122); /* 35 */
  ZYXEL_HH (b, c, d, a, x[14], S34, 0xfde5380c); /* 36 */
  ZYXEL_HH (a, b, c, d, x[ 1], S31, 0xa4beea44); /* 37 */
  ZYXEL_HH (d, a, b, c, x[ 4], S32, 0x4bdecfa9); /* 38 */
  ZYXEL_HH (c, d, a, b, x[ 7], S33, 0xf6bb4b60); /* 39 */
  ZYXEL_HH (b, c, d, a, x[10], S34, 0xbebfbc70); /* 40 */
  ZYXEL_HH (a, b, c, d, x[13], S31, 0x289b7ec6); /* 41 */
  ZYXEL_HH (d, a, b, c, x[ 0], S32, 0xeaa127fa); /* 42 */
  ZYXEL_HH (c, d, a, b, x[ 3], S33, 0xd4ef3085); /* 43 */
  ZYXEL_HH (b, c, d, a, x[ 6], S34,  0x4881d05); /* 44 */
  ZYXEL_HH (a, b, c, d, x[ 9], S31, 0xd9d4d039); /* 45 */
  ZYXEL_HH (d, a, b, c, x[12], S32, 0xe6db99e5); /* 46 */
  ZYXEL_HH (c, d, a, b, x[15], S33, 0x1fa27cf8); /* 47 */
  ZYXEL_HH (b, c, d, a, x[ 2], S34, 0xc4ac5665); /* 48 */

  /* Round 4 */
  ZYXEL_II (a, b, c, d, x[ 0], S41, 0xf4292244); /* 49 */
  ZYXEL_II (d, a, b, c, x[ 7], S42, 0x432aff97); /* 50 */
  ZYXEL_II (c, d, a, b, x[14], S43, 0xab9423a7); /* 51 */
  ZYXEL_II (b, c, d, a, x[ 5], S44, 0xfc93a039); /* 52 */
  ZYXEL_II (a, b, c, d, x[12], S41, 0x655b59c3); /* 53 */
  ZYXEL_II (d, a, b, c, x[ 3], S42, 0x8f0ccc92); /* 54 */
  ZYXEL_II (c, d, a, b, x[10], S43, 0xffeff47d); /* 55 */
  ZYXEL_II (b, c, d, a, x[ 1], S44, 0x85845dd1); /* 56 */
  ZYXEL_II (a, b, c, d, x[ 8], S41, 0x6fa87e4f); /* 57 */
  ZYXEL_II (d, a, b, c, x[15], S42, 0xfe2ce6e0); /* 58 */
  ZYXEL_II (c, d, a, b, x[ 6], S43, 0xa3014314); /* 59 */
  ZYXEL_II (b, c, d, a, x[13], S44, 0x4e0811a1); /* 60 */
  ZYXEL_II (a, b, c, d, x[ 4], S41, 0xf7537e82); /* 61 */
  ZYXEL_II (d, a, b, c, x[11], S42, 0xbd3af235); /* 62 */
  ZYXEL_II (c, d, a, b, x[ 2], S43, 0x2ad7d2bb); /* 63 */
  ZYXEL_II (b, c, d, a, x[ 9], S44, 0xeb86d391); /* 64 */


  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;

  /* Zeroize sensitive information.
   */
  MD5_memset ((POINTER)x, 0, sizeof (x));
}

void MD5Final_ezipupdate (
	MD5_CTX_ezipupdate *context,                        /* context */
	unsigned char digest[16]                 /* message digest */
)
{
  unsigned char bits[8];
  unsigned int index, padLen;

  /* Save number of bits */
  Encode_ezipupdate (bits, context->count, 8);

  /* Pad out to 56 mod 64.*/
  index = (unsigned int)((context->count[0] >> 3) & 0x3f);
  padLen = (index < 56) ? (56 - index) : (120 - index);
  MD5Update_ezipupdate(context, PADDING, padLen);

  /* Append length (before padding) */
  MD5Update_ezipupdate(context, bits, 8);

  /* Store state in digest */
  Encode_ezipupdate (digest, context->state, 16);

  /* Zeroize sensitive information.   */
  MD5_memset ((POINTER)context, 0, sizeof (*context));
}

/* Encodes input (uint32) into output (unsigned char). Assumes len is
  a multiple of 4.  */
void Encode_ezipupdate(
	unsigned char *output,
	uint32 *input,
	unsigned int len
)
{
  unsigned int i, j;

  for (i = 0, j = 0; j < len; i++, j += 4) {
 output[j] = (unsigned char)(input[i] & 0xff);
 output[j+1] = (unsigned char)((input[i] >> 8) & 0xff);
 output[j+2] = (unsigned char)((input[i] >> 16) & 0xff);
 output[j+3] = (unsigned char)((input[i] >> 24) & 0xff);
  }
}

#endif

#endif
