/************************************************************
 *
 * <:copyright-BRCM:2012:DUAL/GPL:standard
 * 
 *    Copyright (c) 2012 Broadcom Corporation
 *    All Rights Reserved
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2, as published by
 * the Free Software Foundation (the "GPL").
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * 
 * A copy of the GPL is available at http://www.broadcom.com/licenses/GPLv2.php, or by
 * writing to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 * 
 * :>
 ************************************************************/


#ifdef __mips
#if ((defined(__MIPSEB)+defined(__MIPSEL)) != 1)
#error "Either __MIPSEB or __MIPSEL must be defined!"
#endif
#endif


#ifndef _LIB_TYPES_H
#define _LIB_TYPES_H

#ifndef _LINUX_TYPES_H

/*  *********************************************************************
    *  Constants
    ********************************************************************* */

#ifndef NULL
#define NULL 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

/*  *********************************************************************
    *  Basic types
    ********************************************************************* */
typedef long unsigned int size_t;

typedef char int8_t;
typedef unsigned char uint8_t;

typedef short int16_t;
typedef unsigned short uint16_t;

#ifdef __long64
typedef int int32_t;
typedef unsigned int uint32_t;
#else
typedef long int32_t;
typedef unsigned long uint32_t;
#endif

typedef long long int64_t;
typedef unsigned long long uint64_t;

#define unsigned signed		/* Kludge to get unsigned size-shaped type. */
typedef __SIZE_TYPE__ intptr_t;
#undef unsigned
typedef __SIZE_TYPE__ uintptr_t;

/*  *********************************************************************
    *  Macros
    ********************************************************************* */

#ifndef offsetof
#define offsetof(type,memb) ((size_t)&((type *)0)->memb)
#endif

#endif

/*  *********************************************************************
    *  Structures
    ********************************************************************* */

typedef struct cons_s {
    char *str;
    int num;
} cons_t;

#endif
