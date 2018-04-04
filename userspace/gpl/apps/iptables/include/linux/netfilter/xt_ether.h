#ifndef _XT_ETHER_H
#define _XT_ETHER_H

struct xt_ether_info {
    uint16_t ethertype;
    uint16_t vlanpriority;	/* 802.1p */
    uint16_t vlanid;	/* 802.1q */
    int bitmask;
    int invert;
#ifdef KERNEL_64_USERSPACE_32
    u_int64_t prev;
    u_int64_t placeholder;
#else
	/* Used internally by the kernel */
    unsigned long prev;
    struct xt_ether_info *master;
#endif
};

#define IPT_ETHER_TYPE		0x1
#define IPT_ETHER_8021Q		0x2
#define IPT_ETHER_8021P		0x4

#define False 0
#define True 1

#endif /* _XT_ETHER_H */