#ifndef _XT_MARK_H
#define _XT_MARK_H

struct xt_mark_info {
	unsigned long mark, mask;
#if 1 //__MSTC__, Jeff
        u_int8_t bitmask;
#endif
	u_int8_t invert;
};

#if 1 //__MSTC__, Jeff
#define IPT_MARK             0x1
#define IPT_MARK_CLASSID_CMP 0x2
#endif

#endif /*_XT_MARK_H*/
