/*This file includes functions which is defined in old GCC version 
  and not in high GCC Version
  To be backward compatible, a couple functions is defined here*/

#define GCC_VERSION (__GNUC__ * 10000 \
                     + __GNUC_MINOR__ * 100 \
                     + __GNUC_PATCHLEVEL__)


/*GCC version 4.2.3*/
#if GCC_VERSION >= 40203
#include <stdio.h>

void bcopy(const void *src, void *dst, int len)
{
	memcpy(dst, src, len);
}

int bcmp(const void *b1, const void *b2, int len)
{
	return (memcmp(b1, b2, len));
}

void bzero(void *b, int len)
{
	memset(b, '\0', len);
}
#endif
