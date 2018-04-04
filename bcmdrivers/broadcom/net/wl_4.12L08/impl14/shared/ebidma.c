/*
 * Generic Broadcom Home Networking Division (HND) DMA module.
 * This supports the following chips: BCM42xx, 44xx, 47xx .
 *
 * Copyright 2005, Broadcom Corporation
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 * $Id$
 */

#if !defined(DSLCPE_SDIO) && !defined(DSLCPE_SDIO_EBIDMA)
#error "DSLCPE_SDIO and DSLCPE_SDIO_EBIDMA must be defined" 
#endif

#include <typedefs.h>
#include <osl.h>
#include <bcmendian.h>
#include <sbconfig.h>
#include <bcmutils.h>
#include <bcmdevs.h>

#ifdef DSLCPE
#include <hndsoc.h>
#endif

#undef DMA_MAP
#undef DMA_UNMAP
#undef DMA_ALLOC_CONSISTENT
#undef DMA_FREE_CONSISTENT	

#define	DMA_MAP(osh, va, size, direction, p) \
	osl_dma_map((osh), (va), (size), (direction))
#define	DMA_UNMAP(osh, pa, size, direction, p) \
	osl_dma_unmap((osh), (pa), (size), (direction))
	
#define	DMA_CONSISTENT_ALIGN	PAGE_SIZE
#define	DMA_ALLOC_CONSISTENT(osh, size, pap) \
	osl_dma_alloc_consistent((osh), (size), (pap))
#define	DMA_FREE_CONSISTENT(osh, va, size, pa) \
	osl_dma_free_consistent((osh), (void*)(va), (size), (pa))
	
	
struct ebidma_info;	/* forward declaration */
#define di_t struct ebidma_info

/* debug/trace */
#ifdef BCMDBG
#define	DMA_ERROR(args) if (!(*di->msg_level & 1)) ; else printf args
#define	DMA_TRACE(args) if (!(*di->msg_level & 2)) ; else printf args
#else
#define	DMA_ERROR(args) printf args
#define	DMA_TRACE(args)
#endif

/* default dma message level (if input msg_level pointer is null in dma_attach()) */
static uint dma_msg_level =
#if defined(BCMDBG)
	1;
#else
	0;
#endif

#define	MAXNAMEL	8


#include <sbhnddma.h>
#include <ebidma.h>

/* tx callback*/
typedef void (*cb_ebidma_tx_t)(void *context, uint addr, void *pkt);

typedef struct _ebidma_data {
	//struct _ebidma_data *next;
	int inuse;
	uint fifo;
	void *pkt;
	cb_ebidma_tx_t cb;
	void *param;
} ebidma_data_t;


/* dma engine software state */
typedef struct ebidma_info {
	ebidma_t	ebidma;		/* exported structure */
	uint		*msg_level;	/* message level pointer */
	char		name[MAXNAMEL];	/* callers name for diag msgs */
	
	void		*osh;		/* os handle */
	void		*ebih;		/* ebi handle */
	    	
	bool		dma64;		/* dma64 enabled */
	bool		addrext;	/* this dma engine supports DmaExtendedAddrChanges */
	
	dma32regs_t	*d32txregs;	/* 32 bits dma tx engine registers */
	dma32regs_t	*d32rxregs;	/* 32 bits dma rx engine registers */
	dma64regs_t	*d64txregs;	/* 64 bits dma tx engine registers */
	dma64regs_t	*d64rxregs;	/* 64 bits dma rx engine registers */

	uint32		dma64align;	/* either 8k or 4k depends on number of dd */
	dma32dd_t	*txd32;		/* pointer to dma32 tx descriptor ring */
	dma64dd_t	*txd64;		/* pointer to dma64 tx descriptor ring */
	uint		ntxd;		/* # tx descriptors tunable */	
	uint		txin;		/* index of next descriptor to reclaim */
	uint		txout;		/* index of next descriptor to post */
	uint		txavail;	/* # free tx descriptors */
	void		**txp;		/* pointer to parallel array of pointers to packets */
	ulong		txdpa;		/* physical address of descriptor ring */
	uint		txdalign;	/* #bytes added to alloc'd mem to align txd */
	uint		txdalloc;	/* #bytes allocated for the ring */

	dma32dd_t	*rxd32;		/* pointer to dma32 rx descriptor ring */
	dma64dd_t	*rxd64;		/* pointer to dma64 rx descriptor ring */
	uint		nrxd;		/* # rx descriptors tunable */	
	uint		rxin;		/* index of next descriptor to reclaim */
	uint		rxout;		/* index of next descriptor to post */
	void		**rxp;		/* pointer to parallel array of pointers to packets */
	ulong		rxdpa;		/* physical address of descriptor ring */
	uint		rxdalign;	/* #bytes added to alloc'd mem to align rxd */
	uint		rxdalloc;	/* #bytes allocated for the ring */	
	/* tunables */
	uint		rxbufsize;	/* rx buffer size in bytes */
	uint		nrxpost;	/* # rx buffers to keep posted */
	uint		rxoffset;	/* rxcontrol offset */
	uint		ddoffsetlow;	/* add to get dma address of descriptor ring, low 32 bits */
	uint		ddoffsethigh;	/* add to get dma address of descriptor ring, high 32 bits */
	uint		dataoffsetlow;	/* add to get dma address of data buffer, low 32 bits */
	uint		dataoffsethigh;	/* add to get dma address of data buffer, high 32 bits */

	bool		txdma_pending;	/* flag wait for rx dma done and start tx dma */
	bool		rxdma_pending;	/* flag wait for tx dma done and start rx dma */
	bool		txdma_active;	/* tx dma is progress */
	bool		rxdma_active;	/* rx dma is progress */	
} dma_info_t;

#define	DMA64_ENAB(di)	(0)

/* descriptor bumping macros */
#define	XXD(x, n)	((x) & ((n) - 1))
#define	TXD(x)		XXD((x), di->ntxd)
#define	RXD(x)		XXD((x), di->nrxd)
#define	NEXTTXD(i)	TXD(i + 1)
#define	PREVTXD(i)	TXD(i - 1)
#define	NEXTRXD(i)	RXD(i + 1)
#define	NTXDACTIVE(h, t)	TXD(t - h)
#define	NRXDACTIVE(h, t)	RXD(t - h)

/* macros to convert between byte offsets and indexes */
#define	B2I(bytes, type)	((bytes) / sizeof(type))
#define	I2B(index, type)	((index) * sizeof(type))

#define	PCI32ADDR_HIGH		0xc0000000	/* address[31:30] */
#define	PCI32ADDR_HIGH_SHIFT	30


/* prototypes */
static bool ebidma_alloc(dma_info_t *di, uint direction);

static bool ebidma32_alloc(dma_info_t *di, uint direction);
static void ebidma32_txreset(dma_info_t *di);
static void ebidma32_rxreset(dma_info_t *di);
static bool ebidma32_txsuspendedidle(dma_info_t *di);
static int  ebidma32_txfast(dma_info_t *di, uint fifo, void *p0, uint len, uint32 coreflags, void *cb, void *param);
static int  ebidma32_tx_end(dma_info_t *di);
static void* ebidma32_getnexttxp(dma_info_t *di, bool forceall);
static void* ebidma32_getnextrxp(dma_info_t *di, bool forceall);
void ebidma_rx_control(dma_info_t *di, uint start);
#ifdef BCMDBG
static char *ebidma32_dumpring(dma_info_t *di, char *buf, dma32dd_t *ring, uint start, uint end, uint max);
static char *ebidma64_dumpring(dma_info_t *di, char *buf, dma64dd_t *ring, uint start, uint end, uint max);
#endif

void* 
ebidma_attach(osl_t *osh, char *name, ebi_t *ebih, void *dmaregstx, void *dmaregsrx,
	   uint ntxd, uint nrxd, uint rxbufsize, uint nrxpost, uint rxoffset, uint *msg_level)
{
	dma_info_t *di;
	uint size;

	/* allocate private info structure */
	if ((di = MALLOC(osh, sizeof (dma_info_t))) == NULL) {
#ifdef BCMDBG
		printf("dma_attach: out of memory, malloced %d bytes\n", MALLOCED(osh));
#endif
		return (NULL);
	}
	bzero((char*)di, sizeof (dma_info_t));

	di->msg_level = msg_level ? msg_level : &dma_msg_level;	
	
	/* check arguments */
	ASSERT(ISPOWEROF2(ntxd));
	ASSERT(ISPOWEROF2(nrxd));
	if (nrxd == 0)
		ASSERT(dmaregsrx == NULL);
	if (ntxd == 0)
		ASSERT(dmaregstx == NULL);


	/* init dma reg pointer */
	ASSERT(ntxd <= D32MAXDD);
	ASSERT(nrxd <= D32MAXDD);
	di->d32txregs = (dma32regs_t *)dmaregstx;
	di->d32rxregs = (dma32regs_t *)dmaregsrx;

	/* make a private copy of our callers name */
	strncpy(di->name, name, MAXNAMEL);
	di->name[MAXNAMEL-1] = '\0';

	di->osh = osh;
	di->ebih = ebih;

	/* save tunables */
	di->ntxd = ntxd;
	di->nrxd = nrxd;
	di->rxbufsize = rxbufsize;
	di->nrxpost = nrxpost;
	di->rxoffset = rxoffset;

	/* 
	 * figure out the DMA physical address offset for dd and data 
	 *   for old chips w/o sb, use zero
	 *   for new chips w sb, 
	 *     PCI/PCIE: they map silicon backplace address to zero based memory, need offset
	 *     Other bus: use zero
	 *     SB_BUS BIGENDIAN kludge: use sdram swapped region for data buffer, not descriptor
	 */
	di->ddoffsetlow = 0;
	di->dataoffsetlow = 0;

	DMA_TRACE(("%s: dma_attach: osh %p ntxd %d nrxd %d rxbufsize %d nrxpost %d rxoffset %d ddoffset 0x%x dataoffset 0x%x\n", 
		   name, osh, ntxd, nrxd, rxbufsize, nrxpost, rxoffset, di->ddoffsetlow, di->dataoffsetlow));

	/* allocate tx packet pointer vector */
	if (ntxd) {
		size = ntxd * sizeof (void*);
		if ((di->txp = MALLOC(osh, size)) == NULL) {
			DMA_ERROR(("%s: dma_attach: out of tx memory, malloced %d bytes\n", di->name, MALLOCED(osh)));
			goto fail;
		}
		bzero((char*)di->txp, size);
	}

	/* allocate rx packet pointer vector */
	if (nrxd) {
		size = nrxd * sizeof (void*);
		if ((di->rxp = MALLOC(osh, size)) == NULL) {
			DMA_ERROR(("%s: dma_attach: out of rx memory, malloced %d bytes\n", di->name, MALLOCED(osh)));
			goto fail;
		}
		bzero((char*)di->rxp, size);
	} 

	/* allocate transmit descriptor ring, only need ntxd descriptors but it must be aligned */
	if (ntxd) {
		if (!ebidma_alloc(di, DMA_TX))
			goto fail;
	}

	/* allocate receive descriptor ring, only need nrxd descriptors but it must be aligned */
	if (nrxd) {
		if (!ebidma_alloc(di, DMA_RX))
			goto fail;
	}

	
#ifndef DSLCPE_SDIO
	if ((di->ddoffsetlow == SI_PCI_DMA) && (di->txdpa > SI_PCI_DMA_SZ) && !di->addrext) {
		DMA_ERROR(("%s: dma_attach: txdpa 0x%lx: addrext not supported\n", di->name, di->txdpa));
		goto fail;
	}
	if ((di->ddoffsetlow == SI_PCI_DMA) && (di->rxdpa > SI_PCI_DMA_SZ) && !di->addrext) {
		DMA_ERROR(("%s: dma_attach: rxdpa 0x%lx: addrext not supported\n", di->name, di->rxdpa));
		goto fail;
	}
#endif
	return ((void*)di);
	
fail:
	ebidma_detach((void*)di);
	return (NULL);
}

static bool
ebidma_alloc(dma_info_t *di, uint direction)
{
	return ebidma32_alloc(di, direction);
}

/* may be called with core in reset */
void
ebidma_detach(dma_info_t *di)
{
	if (di == NULL)
		return;

	DMA_TRACE(("%s: dma_detach\n", di->name));

	/* shouldn't be here if descriptors are unreclaimed */
	ASSERT(di->txin == di->txout);
	ASSERT(di->rxin == di->rxout);

	/* free dma descriptor rings */
	if (di->txd32)
		DMA_FREE_CONSISTENT(di->osh, ((int8*)di->txd32 - di->txdalign), di->txdalloc, (di->txdpa - di->txdalign));
		
	if (di->rxd32)
		DMA_FREE_CONSISTENT(di->osh, ((int8*)di->rxd32 - di->rxdalign), di->rxdalloc, (di->rxdpa - di->rxdalign));

	/* free packet pointer vectors */
	if (di->txp)
		MFREE(di->osh, (void*)di->txp, (di->ntxd * sizeof (void*)));
	if (di->rxp)
		MFREE(di->osh, (void*)di->rxp, (di->nrxd * sizeof (void*)));

	/* free our private info structure */
	MFREE(di->osh, (void*)di, sizeof (dma_info_t));
}

void
ebidma_txreset(dma_info_t *di)
{
	DMA_TRACE(("%s: dma_txreset\n", di->name));
	ebidma32_txreset(di);
}

void
ebidma_rxreset(dma_info_t *di)
{
	DMA_TRACE(("%s: dma_rxreset\n", di->name));
	ebidma32_rxreset(di);
}

/* initialize descriptor table base address */
static void
ebidma_ddtable_init(dma_info_t *di, uint direction, ulong pa)
{	
	uint32 offset = di->ddoffsetlow;
	if ((offset != SI_PCI_DMA) || !(pa & PCI32ADDR_HIGH)) {
		if (direction == DMA_TX)				
			HOST_W_REG(&di->d32txregs->addr, (pa + offset));
		else			
			HOST_W_REG(&di->d32rxregs->addr, (pa + offset));
	} else {        
		/* dma32 address extension */
		uint32 ae;
		ASSERT(0);
		ASSERT(di->addrext);
		ae = (pa & PCI32ADDR_HIGH) >> PCI32ADDR_HIGH_SHIFT;
	
		if (direction == DMA_TX) {
			HOST_W_REG(&di->d32txregs->addr, ((pa & ~PCI32ADDR_HIGH) + offset));
			HOST_SET_REG(&di->d32txregs->control, XC_AE, (ae << XC_AE_SHIFT));
		} else {
			HOST_W_REG(&di->d32rxregs->addr, ((pa & ~PCI32ADDR_HIGH) + offset));
			HOST_SET_REG(&di->d32rxregs->control, RC_AE, (ae << RC_AE_SHIFT));
		}
	}
}

/* init the tx or rx descriptor */
static INLINE void
ebidma32_dd_upd(dma_info_t *di, dma32dd_t *ddring, ulong pa, uint outidx, uint32 *ctrl)
{
	uint offset = di->dataoffsetlow;

	if ((offset != SI_PCI_DMA) || !(pa & PCI32ADDR_HIGH)) {		
		W_SM(&ddring[outidx].addr, BUS_SWAP32(pa + offset));
		W_SM(&ddring[outidx].ctrl, BUS_SWAP32(*ctrl));
	} else {    		    
		/* address extension */
		uint32 ae;
#ifdef DSLCPE_SDIO
		printf("***ebidma32_dd_upd*** assert\n");
		ASSERT(0);		
#endif		
		ASSERT(di->addrext);
		ae = (pa & PCI32ADDR_HIGH) >> PCI32ADDR_HIGH_SHIFT;

		*ctrl |= (ae << CTRL_AE_SHIFT);
		W_SM(&ddring[outidx].addr, BUS_SWAP32((pa & ~PCI32ADDR_HIGH) + offset));
		W_SM(&ddring[outidx].ctrl, BUS_SWAP32(*ctrl));
	}
}

/* init the tx or rx descriptor */
void
ebidma_txinit(dma_info_t *di)
{
	DMA_TRACE(("%s: dma_txinit\n", di->name));

	di->txin = di->txout = 0;
	di->txavail = di->ntxd - 1;

	/* clear tx descriptor ring */
	BZERO_SM((void*)di->txd32, (di->ntxd * sizeof (dma32dd_t)));
	HOST_W_REG(&di->d32txregs->control, XC_XE);
	ebidma_ddtable_init(di, DMA_TX, di->txdpa);
	
}

void
ebidma_tx_control(dma_info_t *di, uint start)
{
	DMA_TRACE(("%s: ebidma_tx_control\n", di->name));

	if (start)
		HOST_W_REG(&di->d32txregs->control, XC_XE);		
	else
		HOST_W_REG(&di->d32txregs->control, 0);
}

void
ebidma_rx_control(dma_info_t *di, uint start)
{
	DMA_TRACE(("%s: ebidma_rx_control\n", di->name));

	if (start) {			
		HOST_W_REG(&di->d32rxregs->control, RC_RE);	
	} else
		HOST_W_REG(&di->d32rxregs->control, 0);		
}

bool
ebidma_txenabled(dma_info_t *di)
{
	uint32 xc;
	
	/* If the chip is dead, it is not enabled :-) */
	xc = HOST_R_REG(&di->d32txregs->control);
	return ((xc != 0xffffffff) && (xc & XC_XE));

}

void
ebidma_txsuspend(dma_info_t *di)
{
	DMA_TRACE(("%s: dma_txsuspend\n", di->name));
	HOST_OR_REG(&di->d32txregs->control, XC_SE);
}

void
ebidma_txresume(dma_info_t *di)
{
	DMA_TRACE(("%s: dma_txresume\n", di->name));
	HOST_AND_REG(&di->d32txregs->control, ~XC_SE);	
}

bool
ebidma_txsuspendedidle(dma_info_t *di)
{
	return ebidma32_txsuspendedidle(di);
}

bool
ebidma_txsuspended(dma_info_t *di)
{
	return ((HOST_R_REG(&di->d32txregs->control) & XC_SE) == XC_SE);	
}

bool
ebidma_txstopped(dma_info_t *di)
{
	return ((HOST_R_REG(&di->d32txregs->status) & XS_XS_MASK) == XS_XS_STOPPED);
}

bool
ebidma_rxstopped(dma_info_t *di)
{
	return ((HOST_R_REG(&di->d32rxregs->status) & RS_RS_MASK) == RS_RS_STOPPED);	
}

void
ebidma_fifoloopbackenable(dma_info_t *di)
{
	DMA_TRACE(("%s: dma_fifoloopbackenable\n", di->name));
	HOST_OR_REG(&di->d32txregs->control, XC_LE);
}

void
ebidma_rxinit(dma_info_t *di)
{
	DMA_TRACE(("%s: dma_rxinit\n", di->name));

	di->rxin = di->rxout = 0;

	/* clear rx descriptor ring */
	BZERO_SM((void*)di->rxd32, (di->nrxd * sizeof (dma32dd_t)));
	ebidma_ddtable_init(di, DMA_RX, di->rxdpa);
	
}

void
ebidma_rxenable(dma_info_t *di)
{
	DMA_TRACE(("%s: dma_rxenable\n", di->name));

	HOST_W_REG(&di->d32rxregs->control, ((di->rxoffset << RC_RO_SHIFT) | RC_RE));
}

bool
ebidma_rxenabled(dma_info_t *di)
{
	uint32 rc;

	rc = HOST_R_REG(&di->d32rxregs->control);
	return ((rc != 0xffffffff) && (rc & RC_RE));
}


/* !! tx entry routine */
int
ebidma_txfast(dma_info_t *di, uint32 addr, void *p0, uint len, uint32 coreflags, void *cb, void *param)
{
	
	ebidma_txinit(di);
	return ebidma32_txfast(di, addr, p0, len, coreflags, cb, param);
	
}

int
ebidma_tx_end(dma_info_t *di)
{
	return ebidma32_tx_end(di);
}

void*
ebidma_rx_end(dma_info_t *di)
{
	void *p; 
	  	
	p = ebidma_getnextrxp(di, FALSE);
	ebidma_rx_control(di, 0);
		
	return (p);
}

/* post receive buffers */
void
ebidma_rx_start(dma_info_t *di, uint32 addr, void *p0, uint len, uint32 coreflags, void *cb, void *param)
{
	void *p;
	uint rxin, rxout;
	uint32 ctrl;
	uint32 pa;
	uint rxbufsize;

	/*
	 * Determine how many receive buffers we're lacking
	 * from the full complement, allocate, initialize,
	 * and post them, then update the chip rx lastdscr.
	 */
	 
 	/* start rx dma */
	ebidma_rx_control(di, 1);
	
	/* initialize parameters */
	p = p0;
	di->rxbufsize = len;
	rxout = rxin = di->rxin= di->rxout = 0;
	rxbufsize = di->rxbufsize;

	DMA_TRACE(("%s: dma_rxfill: post %d\n", di->name, 1));

	/* Do a cached write instead of uncached write since DMA_MAP
	 * will flush the cache. */		
	*(uint32*)(p) = 0;
	pa = (uint32) DMA_MAP(di->osh, p , rxbufsize, DMA_RX, p);

	ASSERT(ISALIGNED(pa, 4));

	/* save the free packet pointer */
	ASSERT(di->rxp[rxout] == NULL);

	di->rxp[rxout] = p;

	/* prep the descriptor control value */
	ctrl = rxbufsize;
	if (rxout == (di->nrxd - 1)) { 
		ctrl |= CTRL_EOT;
	}
	ebidma32_dd_upd(di, di->rxd32, pa, rxout, &ctrl);

	rxout = NEXTRXD(rxout);	

	di->rxout = rxout;

	
	/* update the chip lastdscr pointer */
	HOST_W_REG(&di->d32rxregs->ptr, I2B(rxout, dma32dd_t));

}

/*
 * Reclaim next completed txd (txds if using chained buffers) and
 * return associated packet.
 * If 'force' is true, reclaim txd(s) and return associated packet
 * regardless of the value of the hardware "curr" pointer.
 */
void*
ebidma_getnexttxp(dma_info_t *di, bool forceall)
{
	return ebidma32_getnexttxp(di, forceall);

}

void *
ebidma_getnextrxp(dma_info_t *di, bool forceall)
{
	return ebidma32_getnextrxp(di, forceall);
}

uint
ebidma_txactive(dma_info_t *di)
{
	return (NTXDACTIVE(di->txin, di->txout));
}
	
void
ebidma_rxpiomode(dma32regs_t *regs)
{
	HOST_W_REG(&regs->control, RC_FM);
}

void
ebidma_txpioloopback(dma32regs_t *regs)
{
	HOST_OR_REG(&regs->control, XC_LE);
}


#ifdef BCMDBG
static char*
ebidma32_dumpring(dma_info_t *di, char *buf, dma32dd_t *ring, uint start, uint end, uint max)
{
	uint i;

	for (i = start; i != end; i = XXD((i + 1), max)) {
		buf += sprintf(buf, "%d: 0x%x/0x%x\n", i, ring[i].ctrl, ring[i].addr);
	}
	return (buf);
}

static char*
ebidma64_dumpring(dma_info_t *di, char *buf, dma64dd_t *ring, uint start, uint end, uint max)
{
	uint i;

	for (i = start; i != end; i = XXD((i + 1), max)) {
		buf += sprintf(buf, "%d: 0x%x/0x%x/0x%x/0x%x\n", i, ring[i].ctrl1, ring[i].ctrl2, 
			       ring[i].addrlow, ring[i].addrhigh);
	}
	return (buf);
}

char*
ebidma_dumptx(dma_info_t *di, char *buf, bool dumpring)
{
	if (DMA64_ENAB(di)) {
		buf += sprintf(buf, "DMA64: txd %p txdpa 0x%lx txp %p txin %d txout %d txavail %d\n",
			       di->txd64, di->txdpa, di->txp, di->txin, di->txout, di->txavail);

		buf += sprintf(buf, "xmtcontrol 0x%x xmtaddrlow 0x%x xmtaddrhigh 0x%x xmtptr 0x%x xmtstatus0 0x%x xmtstatus1 0x%x\n",
			HOST_R_REG(&di->d64txregs->control),
			HOST_R_REG(&di->d64txregs->addrlow),
			HOST_R_REG(&di->d64txregs->addrhigh),
			HOST_R_REG(&di->d64txregs->ptr),
			HOST_R_REG(&di->d64txregs->status0),
			HOST_R_REG(&di->d64txregs->status1));

		if (dumpring && di->txd64)
			buf = ebidma64_dumpring(di, buf, di->txd64, di->txin, di->txout, di->ntxd);
	} else {
		buf += sprintf(buf, "DMA32: txd %p txdpa 0x%lx txp %p txin %d txout %d txavail %d\n",
			       di->txd32, di->txdpa, di->txp, di->txin, di->txout, di->txavail);

		buf += sprintf(buf, "xmtcontrol 0x%x xmtaddr 0x%x xmtptr 0x%x xmtstatus 0x%x\n",
			HOST_R_REG(&di->d32txregs->control),
			HOST_R_REG(&di->d32txregs->addr),
			HOST_R_REG(&di->d32txregs->ptr),
			HOST_R_REG(&di->d32txregs->status));

		if (dumpring && di->txd32)
			//buf = ebidma32_dumpring(di, buf, di->txd32, di->txin, di->txout, di->ntxd);
			buf = ebidma32_dumpring(di, buf, di->txd32, di->txin,di->txavail, di->ntxd);
	}
	return (buf);
}

char*
ebidma_dumprx(dma_info_t *di, char *buf, bool dumpring)
{
	if (DMA64_ENAB(di)) {
		buf += sprintf(buf, "DMA64: rxd %p rxdpa 0x%lx rxp %p rxin %d rxout %d\n",
			di->rxd64, di->rxdpa, di->rxp, di->rxin, di->rxout);
	
		buf += sprintf(buf, "rcvcontrol 0x%x rcvaddrlow 0x%x rcvaddrhigh 0x%x rcvptr 0x%x rcvstatus0 0x%x rcvstatus1 0x%x\n",
			HOST_R_REG(&di->d64rxregs->control),
			HOST_R_REG(&di->d64rxregs->addrlow),
			HOST_R_REG(&di->d64rxregs->addrhigh),
			HOST_R_REG(&di->d64rxregs->ptr),
			HOST_R_REG(&di->d64rxregs->status0),
			HOST_R_REG(&di->d64rxregs->status1));
		if (di->rxd64 && dumpring)
			buf = ebidma64_dumpring(di, buf, di->rxd64, di->rxin, di->rxout, di->nrxd);
	} else {
		buf += sprintf(buf, "DMA32: rxd %p rxdpa 0x%lx rxp %p rxin %d rxout %d\n",
			di->rxd32, di->rxdpa, di->rxp, di->rxin, di->rxout);
	
		buf += sprintf(buf, "rcvcontrol 0x%x rcvaddr 0x%x rcvptr 0x%x rcvstatus 0x%x\n",
			HOST_R_REG(&di->d32rxregs->control),
			HOST_R_REG(&di->d32rxregs->addr),
			HOST_R_REG(&di->d32rxregs->ptr),
			HOST_R_REG(&di->d32rxregs->status));
		if (di->rxd32 && dumpring)
			buf = ebidma32_dumpring(di, buf, di->rxd32, di->rxin, di->rxout, di->nrxd);
	}
	return (buf);
}

char*
ebidma_dump(dma_info_t *di, char *buf, bool dumpring)
{
	buf = ebidma_dumptx(di, buf, dumpring);
	buf = ebidma_dumprx(di, buf, dumpring);
	return (buf);
}

#endif	/* BCMDBG */


/*** 32 bits DMA non-inline functions ***/
static bool
ebidma32_alloc(dma_info_t *di, uint direction)
{
	uint size;
	uint ddlen;
	void *va;

	ddlen = sizeof (dma32dd_t);

	size = (direction == DMA_TX) ? (di->ntxd * ddlen) : (di->nrxd * ddlen);

	if (!ISALIGNED(DMA_CONSISTENT_ALIGN, D32RINGALIGN))
		size += D32RINGALIGN;


	if (direction == DMA_TX) {
		if ((va = DMA_ALLOC_CONSISTENT(di->osh, size, &di->txdpa)) == NULL) {
			DMA_ERROR(("%s: dma_attach: DMA_ALLOC_CONSISTENT(ntxd) failed\n", di->name));
			return FALSE;
		}

		di->txd32 = (dma32dd_t*) ROUNDUP((uintptr)va, D32RINGALIGN);
		di->txdalign = (uint)((int8*)di->txd32 - (int8*)va);
		di->txdpa += di->txdalign;
		di->txdalloc = size;
		ASSERT(ISALIGNED((uintptr)di->txd32, D32RINGALIGN));
	} else {
		if ((va = DMA_ALLOC_CONSISTENT(di->osh, size, &di->rxdpa)) == NULL) {
			DMA_ERROR(("%s: dma_attach: DMA_ALLOC_CONSISTENT(nrxd) failed\n", di->name));
			return FALSE;
		}
		di->rxd32 = (dma32dd_t*) ROUNDUP((uintptr)va, D32RINGALIGN);
		di->rxdalign = (uint)((int8*)di->rxd32 - (int8*)va);
		di->rxdpa += di->rxdalign;
		di->rxdalloc = size;
		ASSERT(ISALIGNED((uintptr)di->rxd32, D32RINGALIGN));
	}

	return TRUE;
}

static void 
ebidma32_txreset(dma_info_t *di)
{
	uint32 status;

	/* suspend tx DMA first */
	HOST_W_REG(&di->d32txregs->control, XC_SE);
	SPINWAIT((status = (HOST_R_REG(&di->d32txregs->status) & XS_XS_MASK)) != XS_XS_DISABLED &&
		 status != XS_XS_IDLE &&
		 status != XS_XS_STOPPED,
		 10000);

	HOST_W_REG(&di->d32txregs->control, 0);
	SPINWAIT((status = (HOST_R_REG(&di->d32txregs->status) & XS_XS_MASK)) != XS_XS_DISABLED,
		 10000);

	if (status != XS_XS_DISABLED) {
		DMA_ERROR(("%s: dma_txreset: dma cannot be stopped\n", di->name));
	}

	/* wait for the last transaction to complete */
	OSL_DELAY(300);
}

static void 
ebidma32_rxreset(dma_info_t *di)
{
	uint32 status;

	HOST_W_REG(&di->d32rxregs->control, 0);
	SPINWAIT((status = (HOST_R_REG(&di->d32rxregs->status) & RS_RS_MASK)) != RS_RS_DISABLED,
		 10000);

	if (status != RS_RS_DISABLED) {
		DMA_ERROR(("%s: dma_rxreset: dma cannot be stopped\n", di->name));
	}
}

static bool
ebidma32_txsuspendedidle(dma_info_t *di)
{
	if (!(HOST_R_REG(&di->d32txregs->control) & XC_SE))
		return 0;
	
	if ((HOST_R_REG(&di->d32txregs->status) & XS_XS_MASK) != XS_XS_IDLE)
		return 0;

	OSL_DELAY(2);
	return ((HOST_R_REG(&di->d32txregs->status) & XS_XS_MASK) == XS_XS_IDLE);
}

/*
 * supports full 32bit dma engine buffer addressing so
 * dma buffers can cross 4 Kbyte page boundaries.
 */
static int
ebidma32_txfast(dma_info_t *di,  uint32 addr, void *p0, uint len, uint32 coreflags, void *cb, void *param)
{
	uchar *data;
	uint txout;
	uint32 ctrl;
	uint32 pa;

	DMA_TRACE(("%s: dma_txfast\n", di->name));

	ebidma_tx_control(di, 1);
	txout = di->txout;
	ctrl = 0;

	/*
	 * Walk the chain of packet buffers
	 * allocating and initializing transmit descriptor entries.
	 */
	/* 
         * buffer based
         */
	data = p0;

	/* return nonzero if out of tx descriptors */
	if (NEXTTXD(txout) == di->txin)
		goto outoftxd;
		
	if (len == 0)
		return(0);

	/* get physical address of buffer start */
	pa = (uint32) DMA_MAP(di->osh, data, len, DMA_TX, p);

	/* build the descriptor control value */
	ctrl = len & CTRL_BC_MASK;

	ctrl |= coreflags;
		
	ctrl |= (CTRL_SOF|CTRL_EOF); /* remove CTRL_IOC , let coreflag to deal */

	if (txout == (di->ntxd - 1))
		ctrl |= CTRL_EOT;

	ebidma32_dd_upd(di, di->txd32, pa, txout, &ctrl);


	ASSERT(di->txp[txout] == NULL);

	txout = NEXTTXD(txout);

	/* if last txd eof not set, fix it */
	if (!(ctrl & CTRL_EOF))
		W_SM(&di->txd32[PREVTXD(txout)].ctrl, BUS_SWAP32(ctrl | CTRL_EOF));
	
	/* save the packet */
	di->txp[PREVTXD(txout)] = p0;

	/* bump the tx descriptor index */
	di->txout = txout;
		
	/* kick the chip */
	HOST_W_REG(&di->d32txregs->ptr, I2B(txout, dma32dd_t));
	
	/* tx flow control */
	di->txavail = di->ntxd - NTXDACTIVE(di->txin, di->txout) - 1;

	return (0);

 outoftxd:
	DMA_ERROR(("%s: dma_txfast: out of txds\n", di->name));
	PKTFREE(di->osh, p0, TRUE);
	di->txavail = 0;
	di->ebidma.txnobuf++;
	return (-1);
}


static void*
ebidma32_getnexttxp(dma_info_t *di, bool forceall)
{
	uint start, end, i;
	void *txp;

	DMA_TRACE(("%s: dma_getnexttxp %s\n", di->name, forceall ? "all" : ""));
	
	txp = NULL;

	start = di->txin;
	if (forceall)
		end = di->txout;
	else
		end = B2I(HOST_R_REG(&di->d32txregs->status) & XS_CD_MASK, dma32dd_t);

	if ((start == 0) && (end > di->txout))
		goto bogus;

	for (i = start; i != end && !txp; i = NEXTTXD(i)) {
		DMA_UNMAP(di->osh, (BUS_SWAP32(R_SM(&di->txd32[i].addr)) - di->dataoffsetlow),
			  (BUS_SWAP32(R_SM(&di->txd32[i].ctrl)) & CTRL_BC_MASK), DMA_TX, di->txp[i]);
	
		W_SM(&di->txd32[i].addr, 0xdeadbeef);
		txp = di->txp[i];
		di->txp[i] = NULL;
	}

	di->txin = i;

	/* tx flow control */
	di->txavail = di->ntxd - NTXDACTIVE(di->txin, di->txout) - 1;

	return (txp);

bogus:
	return (NULL);
}

static int
ebidma32_tx_end(dma_info_t *di)
{
	void *p;
	
	p = ebidma32_getnexttxp(di, FALSE);
	ebidma_tx_control(di, 0);

}

static void *
ebidma32_getnextrxp(dma_info_t *di, bool forceall)
{
	uint i;
	void *rxp;

	/* if forcing, dma engine must be disabled */
	ASSERT(!forceall || !ebidma_rxenabled(di));

	i = di->rxin;

	if(i){
		printf("%s di->rxin = %d (!=0)\n", __FUNCTION__, i);			
		ASSERT(i==0);
	}
	        	
	/* return if no packets posted */
	if (i == di->rxout) {
		printf("%s %d returning NULL i = %d\n", __FUNCTION__, 1, i);		
		return (NULL);
	}	

	/* get the packet pointer that corresponds to the rx descriptor */
	rxp = di->rxp[i];
	ASSERT(rxp);
	di->rxp[i] = NULL;

	/* clear this packet from the descriptor ring */
	DMA_UNMAP(di->osh, (BUS_SWAP32(R_SM(&di->rxd32[i].addr)) - di->dataoffsetlow),
		  di->rxbufsize, DMA_RX, rxp);
	W_SM(&di->rxd32[i].addr, 0xdeadbeef);

	di->rxin = NEXTRXD(i);

	return (rxp);
}