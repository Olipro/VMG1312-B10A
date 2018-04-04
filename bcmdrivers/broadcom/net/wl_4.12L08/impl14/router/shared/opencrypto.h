/*
 * Broadcom Home Gateway Reference Design
 *
 * Copyright (C) 2012, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 * $Id: opencrypto.h 241182 2011-02-17 21:50:03Z $
 */

#ifndef _opencrypto_h
#define _opencrypto_h


#define AES_BLOCK_LEN   8	/* bytes for AES block */


/*  AES-based keywrap function defined in RFC3394 */
int aes_wrap(size_t kl, uint8 *key, size_t il, uint8 *input, uint8 *output);

/* AES-based key unwrap function defined in RFC3394 */
int aes_unwrap(size_t kl, uint8 *key, size_t il, uint8 *input, uint8 *output);

/* Pseudo random function */
int fPRF(unsigned char *key, int key_len, unsigned char *prefix,
        int prefix_len, unsigned char *data, int data_len,
        unsigned char *output, int len);

/* hmac-sha1 keyed secure hash algorithm */
void hmac_sha1(unsigned char *text, int text_len, unsigned char *key,
               int key_len, unsigned char *digest);


#endif /* _opencrypto_h */
