#ifndef __PKTDMA_BDS_H_INCLUDED__
#define __PKTDMA_BDS_H_INCLUDED__

#if defined (CONFIG_BCM_ETH_JUMBO_FRAME) /* Not chip specific but feature specific */
#define ENET_MAX_MTU_PAYLOAD_SIZE  (2048)  /* Ethernet Max Payload Size - mini jumbo */
#else
#define ENET_MAX_MTU_PAYLOAD_SIZE  (1500)  /* Ethernet Max Payload Size */
#endif
#define ENET_MAX_MTU_EXTRA_SIZE  (32) /* EH_SIZE(14) + VLANTAG(4) + BRCMTAG(6) + FCS(4) + Extra(??) (4)*/
#define ENET_MAX_MTU_SIZE       (ENET_MAX_MTU_PAYLOAD_SIZE + ENET_MAX_MTU_EXTRA_SIZE)    

#define ENET_MIN_MTU_SIZE       60            /* Without FCS */
#define ENET_MIN_MTU_SIZE_EXT_SWITCH       64            /* Without FCS */

#define DMA_MAX_BURST_LENGTH    8       /* in 64 bit words */
#define RX_BONDING_EXTRA        0
#define RX_ENET_FKB_INPLACE     sizeof(FkBuff_t)
#define RX_ENET_SKB_HEADROOM    ((208 + 0x0f) & ~0x0f)
#define SKB_ALIGNED_SIZE        ((sizeof(struct sk_buff) + 0x0f) & ~0x0f)
#define RX_BUF_LEN              ((ENET_MAX_MTU_SIZE + 63) & ~63)
#define RX_BUF_SIZE             (SKB_DATA_ALIGN(RX_ENET_FKB_INPLACE  + \
                                                RX_ENET_SKB_HEADROOM + \
                                                RX_BONDING_EXTRA     + \
                                                RX_BUF_LEN           + \
                                                sizeof(struct skb_shared_info)))

#define NON_JUMBO_MAX_MTU_SIZE  (1500 + ENET_MAX_MTU_EXTRA_SIZE)
#define NON_JUMBO_RX_BUF_LEN    ((NON_JUMBO_MAX_MTU_SIZE + 63) & ~63)

#define NON_JUMBO_RX_BUF_SIZE    (SKB_DATA_ALIGN(RX_ENET_FKB_INPLACE  + \
                                                RX_ENET_SKB_HEADROOM + \
                                                RX_BONDING_EXTRA     + \
                                                NON_JUMBO_RX_BUF_LEN + \
                                                sizeof(struct skb_shared_info)))

#endif /* __PKTDMA_BDS_H_INCLUDED__ */
