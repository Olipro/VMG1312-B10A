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

#include "fap4keLib_types.h"

char *lib_strcpy(char *dest,const char *src);
char *lib_strncpy(char *dest,const char *src,size_t cnt);
size_t lib_xstrncpy(char *dest,const char *src,size_t cnt);
size_t lib_strlen(const char *str);

int lib_strcmp(const char *dest,const char *src);
int lib_strcmpi(const char *dest,const char *src);
char *lib_strchr(const char *dest,int c);
char *lib_strrchr(const char *dest,int c);
int lib_memcmp(const void *dest,const void *src,size_t cnt);
void *lib_memcpy(void *dest,const void *src,size_t cnt);
void *lib_memset(void *dest,int c,size_t cnt);
char *lib_strdup(char *str);
void lib_trimleading(char *str);
void lib_chop_filename(char *str,char **host,char **file);
void lib_strupr(char *s);
char lib_toupper(char c);
char *lib_strcat(char *dest,const char *src);
char *lib_gettoken(char **str);
char *lib_strnchr(const char *dest,int c,size_t cnt);
int lib_parseipaddr(const char *ipaddr,unsigned char *dest);
int lib_atoi(const char *dest);
int lib_lookup(const cons_t *list,char *str);
int lib_setoptions(const cons_t *list,char *str,unsigned int *flags);
int lib_xtoi(const char *dest);
unsigned long long lib_xtoq(const char *dest);



#if !(defined(_LIB_NO_MACROS_) || defined(_ASM_STRING_H) || defined(_LINUX_STRING_H_))
#define strcpy(d,s) lib_strcpy(d,s)
#define strncpy(d,s,c) lib_strncpy(d,s,c)
#define xstrncpy(d,s,c) lib_xstrncpy(d,s,c)
#define strlen(s) lib_strlen(s)
#define strchr(s,c) lib_strchr(s,c)
#define strrchr(s,c) lib_strrchr(s,c)
#define strdup(s) lib_strdup(s)
#define strcmp(d,s) lib_strcmp(d,s)
#define strcmpi(d,s) lib_strcmpi(d,s)
#define memcmp(d,s,c) lib_memcmp(d,s,c)
#define memset(d,s,c) lib_memset(d,s,c)
#define memcpy(d,s,c) lib_memcpy(d,s,c)
#define bcopy(s,d,c) lib_memcpy(d,s,c)
#define bzero(d,c) lib_memset(d,0,c)
#define strupr(s) lib_strupr(s)
#define toupper(c) lib_toupper(c)
#define strcat(d,s) lib_strcat(d,s)
#define gettoken(s) lib_gettoken(s)
#define strnchr(d,ch,cnt) lib_strnchr(d,ch,cnt)
#define atoi(d) lib_atoi(d)
#define xtoi(d) lib_xtoi(d)
#define xtoq(d) lib_xtoq(d)
#define parseipaddr(i,d) lib_parseipaddr(i,d)
#define lookup(x,y) lib_lookup(x,y)
#define setoptions(x,y,z) lib_setoptions(x,y,z)
#endif

void
qsort(void *bot, size_t nmemb, size_t size, int (*compar)(const void *,const void *));

