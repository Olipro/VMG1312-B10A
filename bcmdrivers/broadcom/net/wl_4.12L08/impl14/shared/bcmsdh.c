/*
 *  BCMSDH interface glue
 *  implement bcmsdh API for SDIOH driver
 *
 * Copyright 2005, Broadcom Corporation
 * All Rights Reserved.                
 *                                     
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;   
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior      
 * written permission of Broadcom Corporation.                            
 *
 * $Id$
 */
/******************* BCMSDH Interface Functions ****************************/

#include <typedefs.h>
#include <bcmdevs.h>
#include <bcmendian.h>
#include <bcmutils.h>
#include <sbconfig.h>
#include <osl.h>

#ifdef DSLCPE
#include <hndsoc.h>
#endif

#include <bcmsdh.h>	/* BRCM wl driver SDIO APIs */
#include <bcmsdioh.h>	/* BRCM sdio Host APIs */

#include <sdio.h>	/* sdio spec */

uint bcmsdh_msglevel = 0x1;

#define BCMSDH_ERROR_VAL	0x0001
#define BCMSDH_INFO_VAL		0x0002

#define BCMSDH_ERROR(x)	do { if (bcmsdh_msglevel & BCMSDH_ERROR_VAL) printf x; } while (0)
#define BCMSDH_INFO(x)	do { if (bcmsdh_msglevel & BCMSDH_INFO_VAL) printf x; } while (0)

#define BCMSDH_DUMMY_REGSVA	0xA0000000

struct bcmsdh_info
{
	bool	init_success;	/* underline driver successfully attached */
	void	* sdioh;	/* handler for sdioh */
	uint32  sb_reg_base;	/* sb register base */
};

/* local copy of bcm sd handler */
bcmsdh_info_t * l_bcmsdh = NULL;

extern bcmsdh_info_t *
bcmsdh_attach(osl_t *osh, void *cfghdl, void **regsva)
{
	bcmsdh_info_t *bcmsdh;

	if ((bcmsdh = (bcmsdh_info_t *)MALLOC(osh, sizeof(bcmsdh_info_t))) == NULL) {
		BCMSDH_ERROR(("bcmsdh_attach: out of memory, malloced %d bytes\n", MALLOCED(osh)));
		return NULL;
	}
	bzero((char *)bcmsdh, sizeof(bcmsdh_info_t));

	/* save the handler locally */
	l_bcmsdh = bcmsdh;

#ifdef DSLCPE
	bcmsdh->sdioh = sdioh_attach(osh, cfghdl, (void*)bcmsdh);
#else 
	bcmsdh->sdioh = sdioh_attach(osh, cfghdl);
#endif	

	if (!bcmsdh->sdioh) {
		bcmsdh_detach(osh, bcmsdh);
		return NULL;
	}
	
	BCMSDH_ERROR(("bcmsdh_attach, sdioh_attach successful, bcmsdh->sdioh 0x%x\n", (uint32)bcmsdh->sdioh));

	bcmsdh->sb_reg_base = SI_ENUM_BASE;
	bcmsdh->init_success = TRUE;

	//*regsva = (uint32 *)BCMSDH_DUMMY_REGSVA;
	*regsva = (uint32 *)SI_ENUM_BASE;
	
	return bcmsdh;
}

extern int 
bcmsdh_detach(osl_t *osh, void *sdh)
{
	bcmsdh_info_t *bcmsdh = (bcmsdh_info_t *)sdh;

	if (bcmsdh->sdioh) {
	sdioh_detach(osh, bcmsdh->sdioh);
	bcmsdh->sdioh = NULL;
	}

	if (bcmsdh) {
		MFREE(osh, bcmsdh, sizeof(bcmsdh_info_t));
	}

	l_bcmsdh = NULL;
	return BCMSDH_SUCCESS;
}

extern bool 
bcmsdh_intr_query(void *sdh)
{
	bcmsdh_info_t *bcmsdh = (bcmsdh_info_t *)sdh;
	SDIOH_API_RC status;
	bool on;

	ASSERT(bcmsdh);
	status = sdioh_interrupt_query(bcmsdh->sdioh, &on);
	if (SDIOH_API_SUCCESS(status))
		return FALSE;
	else
		return on;
}

extern bool 
bcmsdh_intr_enable(void *sdh)
{
	bcmsdh_info_t *bcmsdh = (bcmsdh_info_t *)sdh;
	SDIOH_API_RC status;
	ASSERT(bcmsdh);

	status = sdioh_interrupt_set(bcmsdh->sdioh, TRUE);
	if (SDIOH_API_SUCCESS(status))
		return BCMSDH_SUCCESS;
	else
		return BCMSDH_FAIL;
}

extern bool 
bcmsdh_intr_disable(void *sdh)
{
	bcmsdh_info_t *bcmsdh = (bcmsdh_info_t *)sdh;
	SDIOH_API_RC status;
	ASSERT(bcmsdh);
	
	status = sdioh_interrupt_set(bcmsdh->sdioh, FALSE);
	return SDIOH_API_SUCCESS(status);
}

extern int 
bcmsdh_intr_reg(void *sdh, bcmsdh_cb_fn_t fn, void *argh)
{
	bcmsdh_info_t *bcmsdh = (bcmsdh_info_t *)sdh;
	SDIOH_API_RC status;
	ASSERT(bcmsdh);

	status = sdioh_interrupt_register(bcmsdh->sdioh, fn, argh);

	if (SDIOH_API_SUCCESS(status))
		return BCMSDH_SUCCESS;
	else
		return BCMSDH_FAIL;
}

#ifdef DSLCPE
extern int
bcmsdh_intr_handler(void *sdh)
{
	bcmsdh_info_t *bcmsdh = (bcmsdh_info_t *)sdh;
	SDIOH_API_RC status;
	ASSERT(bcmsdh);
	
	status = sdioh_interrupt_handler(bcmsdh->sdioh);
	
	return status;
}

#endif

extern int 
bcmsdh_intr_dereg(void *sdh)
{
	bcmsdh_info_t *bcmsdh = (bcmsdh_info_t *)sdh;
	SDIOH_API_RC status;
	ASSERT(bcmsdh);

	status = sdioh_interrupt_deregister(bcmsdh->sdioh);
	
	if (SDIOH_API_SUCCESS(status))
		return BCMSDH_SUCCESS;
	else
		return BCMSDH_FAIL;
}

extern int 
bcmsdh_devremove_reg(void *sdh, bcmsdh_cb_fn_t fn, void *argh)
{
	bcmsdh_info_t *bcmsdh = (bcmsdh_info_t *)sdh;
	ASSERT(bcmsdh);

	/* don't support yet */
	return BCMSDH_FAIL;	
}

extern void 
bcmsdh_regbase_set(void *sdh, uint32 addr)
{
	bcmsdh_info_t *bcmsdh = (bcmsdh_info_t *)sdh;
	ASSERT(addr >= SI_ENUM_BASE);

	bcmsdh->sb_reg_base = addr;
	return;
}

extern uint32 
bcmsdh_regbase_get(void *sdh)
{
	bcmsdh_info_t *bcmsdh = (bcmsdh_info_t *)sdh;
	return bcmsdh->sb_reg_base;
}


extern uint8 
bcmsdh_cfg_read(void *sdh, uint fnc_num, uint32 addr, int *err)
//bcmsdh_cfg_read(void *sdh, uint fnc_num, uint32 addr)
{
	bcmsdh_info_t *bcmsdh = (bcmsdh_info_t *)sdh;
	SDIOH_API_RC status;
	uint8 data = 0;

	if (!bcmsdh)
		bcmsdh = l_bcmsdh;

	ASSERT(bcmsdh->init_success);

	status = sdioh_cfg_read(bcmsdh->sdioh, fnc_num, addr, (uint8 *)&data);
	if (err)
		*err = (SDIOH_API_SUCCESS(status) ? 0 : BCME_SDIO_ERROR);

	return data;
}

extern void
bcmsdh_cfg_write(void *sdh, uint fnc_num, uint32 addr, uint8 data, int *err)
//extern uint 
//bcmsdh_cfg_write(void *sdh, uint fnc_num, uint32 addr, uint8 data)
{
	bcmsdh_info_t *bcmsdh = (bcmsdh_info_t *)sdh;
	SDIOH_API_RC status;

	if (!bcmsdh)
		bcmsdh = l_bcmsdh;
	
	ASSERT(bcmsdh->init_success);

	status = sdioh_cfg_write(bcmsdh->sdioh, fnc_num, addr, (uint8 *)&data);
	if (err)
		*err = SDIOH_API_SUCCESS(status) ? 0 : BCME_SDIO_ERROR;

	//return BCMSDH_SUCCESS;
}

extern uint8 
bcmsdh_cis_read(void *sdh, uint fnc_num, uint8 *cis, uint32 length)
{
	bcmsdh_info_t *bcmsdh = (bcmsdh_info_t *)sdh;
	SDIOH_API_RC status;

	if (!bcmsdh)
		bcmsdh = l_bcmsdh;

	ASSERT(bcmsdh->init_success);
	ASSERT(cis);

	status = sdioh_cis_read(bcmsdh->sdioh, fnc_num, cis, length);
	
	if (SDIOH_API_SUCCESS(status))
		return BCMSDH_SUCCESS;
	else
		return BCMSDH_FAIL;
}

#ifdef DSLCPE_SDIO_EBIDMA
void
#else
static void
#endif
address_covert(bcmsdh_info_t *bcmsdh, uint32 *addr)
{
#if 1	
        //printk("addr=0x%x\n", *addr);
	//ASSERT(((uint32)*(addr) >= SI_ENUM_BASE) && ((uint32)*(addr) < SI_ENUM_LIM)); 
	*addr -= SI_ENUM_BASE;	
#else	
	/* Check and substract the virtual offset first */
        //printk("addr=0x%x\n", *addr);	
	ASSERT(*addr >= BCMSDH_DUMMY_REGSVA);
	*addr = *addr - BCMSDH_DUMMY_REGSVA;
	/* Adjust address by adding regbase */
	// *addr = *addr + (bcmsdh->sb_reg_base - SI_ENUM_BASE);
#endif	
}

extern uint32 
bcmsdh_reg_read(void *sdh, uint32 addr, uint size)
{
	bcmsdh_info_t *bcmsdh = (bcmsdh_info_t *)sdh;
	SDIOH_API_RC status;
	uint32 word;
	
	if (!bcmsdh)
		bcmsdh = l_bcmsdh;
	
	ASSERT(bcmsdh->init_success);
	ASSERT((size == 2) || (size == 4));
	
	address_covert(bcmsdh, &addr);
	
	status = sdioh_request_word(bcmsdh->sdioh, SDIOH_CMD_TYPE_NORMAL, SDIOH_READ, SDIO_FUNC_1, addr, &word, size);

	if (SDIOH_API_SUCCESS(status))
#ifdef DSLCPE
		return (size == 4)?word:*((uint16 *)&word);
#else
		return word;
#endif
	return 0xFFFFFFFF;
}

extern uint32 
bcmsdh_reg_write(void *sdh, uint32 addr, uint size, uint32 data)
{
	bcmsdh_info_t *bcmsdh = (bcmsdh_info_t *)sdh;
	SDIOH_API_RC status;
#ifdef DSLCPE
	uint32 word;
#endif
	if (!bcmsdh)
		bcmsdh = l_bcmsdh;

	ASSERT(bcmsdh->init_success);
	ASSERT((size == 2) || (size == 4));

	address_covert(bcmsdh, &addr);
#ifdef DSLCPE
    if (size == 4)
        word = data;
    else
        *((uint16 *)&word) = data;

	status = sdioh_request_word(bcmsdh->sdioh, SDIOH_CMD_TYPE_NORMAL, SDIOH_WRITE, SDIO_FUNC_1,
								addr, &word, size);
#else

	status = sdioh_request_word(bcmsdh->sdioh, SDIOH_CMD_TYPE_NORMAL, SDIOH_WRITE, SDIO_FUNC_1, addr, &data, size);
#endif

	if (SDIOH_API_SUCCESS(status))
		return 0;

	return 0xFFFFFFFF;
}

extern bool 
bcmsdh_recv_buf(void *sdh, uint32 addr, uint size, bool isfifo, uint8 *buf, uint32 len)
{
	bcmsdh_info_t *bcmsdh = (bcmsdh_info_t *)sdh;
	SDIOH_API_RC status;
	uint incr_fix;

	ASSERT(bcmsdh);
	ASSERT(bcmsdh->init_success);

	address_covert(bcmsdh, &addr);

	/* ??? differentiate isfifo for 2/4 bytes fifo */
	incr_fix = isfifo ? SDIOH_DATA_FIX : SDIOH_DATA_INC;

	/* DMA, pio, blk, byte */
	status = sdioh_request_buffer(bcmsdh->sdioh, SDIOH_DATA_PIO, incr_fix, 
				      SDIOH_READ, SDIO_FUNC_1, addr, size, len, buf);
	
	if (SDIOH_API_SUCCESS(status))
		return TRUE;

	return FALSE;
}

extern bool 
bcmsdh_send_buf(void *sdh, uint32 addr, uint size, bool isfifo, uint8 *buf, uint32 len)
{
	bcmsdh_info_t *bcmsdh = (bcmsdh_info_t *)sdh;
	SDIOH_API_RC status;
	uint incr_fix;

	ASSERT(bcmsdh);
	ASSERT(bcmsdh->init_success);

	address_covert(bcmsdh, &addr);

	incr_fix = isfifo ? SDIOH_DATA_FIX : SDIOH_DATA_INC;

	/* DMA, pio, blk, byte */
	status = sdioh_request_buffer(bcmsdh->sdioh, SDIOH_DATA_PIO, incr_fix, 
				      SDIOH_WRITE, SDIO_FUNC_1, addr, size, len, buf);
	if (SDIOH_API_SUCCESS(status))
		return TRUE;

	return FALSE;
}

/* for testing */
extern bool
bcmsdh_test_diag(void *sdh)
{
	bcmsdh_info_t *bcmsdh = (bcmsdh_info_t *)sdh;
	sdioh_test_diag(bcmsdh->sdioh);
	return BCMSDH_SUCCESS;
}
#ifdef DSLCPE
extern void bcmsdh_intr_mask(void *sdh)
{
	bcmsdh_info_t *bcmsdh = (bcmsdh_info_t *)sdh;
	sdioh_interrupt_mask(bcmsdh->sdioh);
}

extern void bcmsdh_intr_unmask(void *sdh)
{
	bcmsdh_info_t *bcmsdh = (bcmsdh_info_t *)sdh;
	sdioh_interrupt_unmask(bcmsdh->sdioh);
}
void extern sdioh_host_mode(sdioh_info_t *sdioh, uint32 mask, uint32 val);
extern void bcmsdh_set_sdclk(void *sdh) 
{
	bcmsdh_info_t *bcmsdh = (bcmsdh_info_t *)sdh;	
	//switch to 50MHz 4312 test
	//sdioh_host_mode(bcmsdh->sdioh, 0x30, 0x30);	
}


#endif

#ifdef DSLCPE_SDIO_EBIDMA
extern int  bcmsdh_ebidma_tx_start(void *sdh, uint32 addr, void *p0, uint len, uint32 coreflags, void *cb, void *param)
{
	bcmsdh_info_t *bcmsdh = (bcmsdh_info_t *)sdh;
	sdioh_ebidma_tx_start(bcmsdh->sdioh, addr, p0, len, coreflags, cb, param);
	
}

extern int  bcmsdh_ebidma_rx_start(void *sdh, uint32 addr, void *p0, uint len, uint32 coreflags, void *cb, void *param)
{
	bcmsdh_info_t *bcmsdh = (bcmsdh_info_t *)sdh;
	sdioh_ebidma_rx_start(bcmsdh->sdioh, addr, p0, len, coreflags, cb, param);	
	
}

extern int  bcmsdh_ebidma_rx_end(void *sdh)
{
	bcmsdh_info_t *bcmsdh = (bcmsdh_info_t *)sdh;
	sdioh_ebidma_rx_end(bcmsdh->sdioh);	
	
}

extern bool
bcmsdh_send_dmabuf(void *sdh, uint32 addr, uint size, bool isfifo, uint8 *buf, uint32 len)
{
	bcmsdh_info_t *bcmsdh = (bcmsdh_info_t *)sdh;
	SDIOH_API_RC status;
	uint incr_fix;

	ASSERT(bcmsdh);
	ASSERT(bcmsdh->init_success);

	address_covert(bcmsdh, &addr);
	
	incr_fix = isfifo ? SDIOH_DATA_FIX : SDIOH_DATA_INC;

	/* DMA, pio, blk, byte */
	status = sdioh_request_buffer(bcmsdh->sdioh, SDIOH_DATA_DMA, incr_fix,
	                              SDIOH_WRITE, SDIO_FUNC_1, addr, size, len, buf);
	if (SDIOH_API_SUCCESS(status))
		return TRUE;

	return FALSE;
}

extern bool 
bcmsdh_ebidma_tx_end(void *sdh)
{
	bcmsdh_info_t *bcmsdh = (bcmsdh_info_t *)sdh;
	sdioh_ebidma_tx_end(bcmsdh->sdioh);
	
}

extern bool 
bcmsdh_recv_dmabuf(void *sdh, uint32 addr, uint size, bool isfifo, uint8 *buf, uint32 len)
{
	bcmsdh_info_t *bcmsdh = (bcmsdh_info_t *)sdh;
	SDIOH_API_RC status;
	uint incr_fix;

	ASSERT(bcmsdh);
	ASSERT(bcmsdh->init_success);

	address_covert(bcmsdh, &addr);

	/* ??? differentiate isfifo for 2/4 bytes fifo */
	incr_fix = isfifo ? SDIOH_DATA_FIX : SDIOH_DATA_INC;

	/* DMA, pio, blk, byte */
	status = sdioh_request_buffer(bcmsdh->sdioh, SDIOH_DATA_DMA, incr_fix, 
				      SDIOH_READ, SDIO_FUNC_1, addr, size, len, buf);
	
	if (SDIOH_API_SUCCESS(status))
		return TRUE;

	return FALSE;
}

#endif

uint
bcmsdh_query_iofnum(void *sdh)
{
	bcmsdh_info_t *bcmsdh = (bcmsdh_info_t *)sdh;

	if (bcmsdh == NULL)
		bcmsdh = l_bcmsdh;
		
	return (sdioh_query_iofnum(bcmsdh->sdioh));
}
