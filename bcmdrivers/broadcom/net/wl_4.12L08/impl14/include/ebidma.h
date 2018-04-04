/*
 * Generic Broadcom Home Networking Division (HND) DMA engine SW interface
 * This supports the following chips: BCM42xx, 44xx, 47xx .
 *
 * Copyright 2005, Broadcom Corporation      
 * All Rights Reserved.      
 *       
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY      
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM      
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS      
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.      
 * $Id$
 */

#ifndef	_ebidma_h_
#define	_ebidma_h_

/* export structure */
typedef volatile struct {
	/* rx error counters */
	uint		rxgiants;	/* rx giant frames */
	uint		rxnobuf;	/* rx out of dma descriptors */
	/* tx error counters */
	uint		txnobuf;	/* tx out of dma descriptors */
} ebidma_t;

#ifndef di_t
#define	di_t	void
#endif

#ifndef osl_t 
#define osl_t void
#endif

#ifndef ebi_t
typedef void ebi_t;
#endif

/* externs */
extern void * ebidma_attach(osl_t *osh, char *name, ebi_t *ebih, void *dmaregstx, void *dmaregsrx, 
			 uint ntxd, uint nrxd, uint rxbufsize, uint nrxpost, uint rxoffset, uint *msg_level);
extern void ebidma_detach(di_t *di);
extern void ebidma_txreset(di_t *di);
extern void ebidma_rxreset(di_t *di);
extern void ebidma_txinit(di_t *di);
extern bool ebidma_txenabled(di_t *di);
extern void ebidma_rxinit(di_t *di);
extern void ebidma_rxenable(di_t *di);
extern bool ebidma_rxenabled(di_t *di);
extern void ebidma_txsuspend(di_t *di);
extern void ebidma_txresume(di_t *di);
extern bool ebidma_txsuspended(di_t *di);
extern bool ebidma_txsuspendedidle(di_t *di);
extern bool ebidma_txstopped(di_t *di);
extern bool ebidma_rxstopped(di_t *di);
extern int  ebidma_txfast(di_t *di, uint32 addr, void *p0, uint len, uint32 coreflags, void *cb, void *param);
extern void ebidma_fifoloopbackenable(di_t *di);
extern void *ebidma_rx(di_t *di);
extern void ebidma_rxfill(di_t *di);
extern void ebidma_txreclaim(di_t *di, bool forceall);
extern void ebidma_rxreclaim(di_t *di);
extern uintptr ebidma_getvar(di_t *di, char *name);
extern void *ebidma_getnexttxp(di_t *di, bool forceall);
extern void *ebidma_peeknexttxp(di_t *di);
extern void *ebidma_getnextrxp(di_t *di, bool forceall);
extern void ebidma_txblock(di_t *di);
extern void ebidma_txunblock(di_t *di);
extern uint ebidma_txactive(di_t *di);
extern void ebidma_txrotate(di_t *di);

extern void ebidma_rxpiomode(dma32regs_t *);
extern void ebidma_txpioloopback(dma32regs_t *);

extern void  ebidma_tx_control(di_t *di, uint start);
extern void  ebidma_rx_control(di_t *di, uint start);

#ifdef DSLCPE_SDIO_EBIDMA
extern void ebidma_rx_start(di_t *di, uint32 addr, void *p0, uint len, uint32 coreflags, void *cb, void *param);
extern void* ebidma_rx_end(di_t *di);
extern int ebidma_tx_end(di_t *di);
#endif

#ifdef BCMDBG
extern char *ebidma_dump(di_t *di, char *buf, bool dumpring);
extern char *ebidma_dumptx(di_t *di, char *buf, bool dumpring);
extern char *ebidma_dumprx(di_t *di, char *buf, bool dumpring);
#endif

#ifdef DSLCPE_SDIO_EBIDMA
#define NONCACHE_TO_PHYS(x)         ((unsigned)(x)&0x1FFFFFFF)
#define PHYS_TO_CACHE(x)            ((unsigned)(x)|0x80000000)
#endif

#ifdef DSLCPE_SDIO_ASYNC_EBIDMA

#define EBIDMA_UNALIGNED_BUFFER_HEAD	0x00000001
#define EBIDMA_UNALIGNED_BUFFER_TRAILER	0x00000002
#define EBIDMA_START_OF_FRAME		0x00000004
#define EBIDMA_END_OF_FRAME		0x00000008
#define EBIDMA_TX_PENDING		0x00000010
#define EBIDMA_RX_PENDING		0x00000020 		

/* export structure */
typedef struct {
	uint		tx_fifo;			/* tx_fifo */
	void 		*p0;				/* MPDU frame */
	uint32          *pdata;				/* MPDU current aligned buffer */
	uint		len;				/* MPDU current aligned buffer length */			
	uint32		*fifocontrol;   		/* Fifo control address*/
	uint32		*fifodata;   			/* Fifo data address*/	
	uint32		unaligned_start_data;		/* unaligned start data */
	uint		unaligned_start_bytevalid; 	/* unaligned start bytevalid */
	uint32		unaligned_trailer_data;         /* unaligned trailer data */
	uint		unaligned_trailer_bytevalid; 	/* unaligned trailer bytevalid */
	uint32		status;				/* status of this frame */
} ebidma_buffer_t;

extern void* ebidma_enq(di_t *di, void *p, uint direction);
extern void* ebidma_enq_head(di_t *di, void *p, uint direction);
extern void* ebidma_deq(di_t *di, uint direction);
extern bool  ebidma_q_empty(di_t *di, uint direction);
#endif /*DSLCPE_SDIO_ASYNC_EBIDMA*/

#endif	/* _hnddma_h_ */
