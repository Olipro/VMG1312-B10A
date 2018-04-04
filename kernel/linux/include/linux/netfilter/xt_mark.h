#ifndef _XT_MARK_H
#define _XT_MARK_H

#include <linux/types.h>

struct xt_mark_info {
    unsigned long mark, mask;
#if 1 //__MSTC__, Jeff
	u_int8_t bitmask;
#endif
    __u8 invert;
};

struct xt_mark_mtinfo1 {
	__u32 mark, mask;
	__u8 invert;
};

#if 1 //__MSTC__, Jeff
#define IPT_MARK             0x1
#define IPT_MARK_CLASSID_CMP 0x2
#endif

#endif /*_XT_MARK_H*/
