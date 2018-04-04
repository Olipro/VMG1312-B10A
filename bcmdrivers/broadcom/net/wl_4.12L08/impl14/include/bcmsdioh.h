/*
 * Broadcom SDIOH (host controller) driver APIs.
 *
 * SDIOH support 1/4 bit SDIO modes as well as SPI mode.
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

#ifndef	_sdioh_h_
#define	_sdioh_h_

/*
#ifndef DSLCPE
#ifndef osl_t 
#define osl_t void
#endif
#endif
*/

/* return code */
#define SDIOH_API_RC_SUCCESS                          (0x00)
#define SDIOH_API_RC_FAIL	                      (0x01)

/* test for API success */
#define SDIOH_API_SUCCESS(status) (status == 0)
/* test for success allowing timeout */
#define SDIOH_API_SUCCESS_RSPTIMEOUT(status) (((status) == 0) || (SDIOH_API_RC_RESPONSE_TIMEOUT == (status)))

#define SDIOH_READ		0
#define SDIOH_WRITE		1

#define SDIOH_DATA_FIX		0	/* incremental address */
#define SDIOH_DATA_INC		1	/* fix address  */

#define SDIOH_CMD_TYPE_NORMAL	0	/* normal command */
#define SDIOH_CMD_TYPE_APPEND	1	/* append command */
#define SDIOH_CMD_TYPE_CUTTHRU	2	/* cut through command */

#define SDIOH_DATA_PIO		0	/* pio mode */
#define SDIOH_DATA_DMA		1	/* dma mode */

typedef int SDIOH_API_RC;

/* SDio Host structure */
typedef struct sdioh_info sdioh_info_t;

/* callback function, taking one arg */
#ifdef DSLCPE
typedef int (*sdioh_cb_fn_t)(void *);
#else
typedef void (*sdioh_cb_fn_t)(void *);
#endif

/* attach, return handler on success, NULL if failed.
 *  The handler shall be provided by all subsequent calls. No local cache
 *  cfghdl points to the starting address of pci device mapped memory
 */
/* 
#ifdef DSLCPE
extern sdioh_info_t * sdioh_attach(osl_t *osh, void *cfghdl, void* sdh);
#else 
extern sdioh_info_t * sdioh_attach(osl_t *osh, void *cfghdl);
#endif

extern SDIOH_API_RC sdioh_detach(osl_t *osh, sdioh_info_t *si);
*/
extern SDIOH_API_RC sdioh_interrupt_register(sdioh_info_t *si, sdioh_cb_fn_t fn, void *argh);
extern SDIOH_API_RC sdioh_interrupt_deregister(sdioh_info_t *si);

/* query whether SD interrupt is enabled or not */
extern SDIOH_API_RC sdioh_interrupt_query(sdioh_info_t *si, bool *onoff);
/* enable or disable SD interrupt */
extern SDIOH_API_RC sdioh_interrupt_set(sdioh_info_t *si, bool enable_disable);


#ifdef DSLCPE_SDIO_EBIDMA
extern int  sdioh_ebidma_tx_start(sdioh_info_t *sdioh, uint32 addr, void *p0, uint len, uint32 coreflags, void *cb, void *param);
extern int  sdioh_ebidma_tx_end(sdioh_info_t *sdioh);
extern int  sdioh_ebidma_rx_start(sdioh_info_t *sdioh, uint32 addr, void *p0, uint len, uint32 coreflags, void *cb, void *param);
extern int  sdioh_ebidma_rx_end(sdioh_info_t *sdioh);
#endif

#ifdef DSLCPE
extern SDIOH_API_RC sdioh_interrupt_handler(sdioh_info_t *sdioh);
extern SDIOH_API_RC sdioh_interrupt_mask(sdioh_info_t *si);
extern SDIOH_API_RC sdioh_interrupt_unmask(sdioh_info_t *si);
#endif
/* read or write one byte using cmd52 */
#ifdef DSLCPE
extern SDIOH_API_RC sdioh_request_byte(sdioh_info_t *sdioh, uint rw, uint fnc_num, uint addr, uint8 *byte);
#else
extern SDIOH_API_RC sdioh_request_byte(sdioh_info_t *si, uint rw, uint fnc, uint addr, uint *byte);
#endif

/* read or write 2/4 bytes using cmd53 */
extern SDIOH_API_RC sdioh_request_word(sdioh_info_t *si, uint cmd_type, uint rw, uint fnc, uint addr, uint32 *word, uint nbyte);

/* read or write any buffer using cmd53 */
extern SDIOH_API_RC sdioh_request_buffer(sdioh_info_t *si, uint pio_dma, uint fix_inc, uint rw, uint fnc_num, uint32 addr, uint regwidth, uint32 buflen, uint8 *buffer);

/* get cis data */
extern SDIOH_API_RC sdioh_cis_read(sdioh_info_t *si, uint fuc, uint8 *cis, uint32 length);

extern SDIOH_API_RC sdioh_cfg_read(sdioh_info_t *si, uint fuc, uint32 addr, uint8 *data);
extern SDIOH_API_RC sdioh_cfg_write(sdioh_info_t *si, uint fuc, uint32 addr, uint8 *data);

/* query/config device capability(TBD) */
extern SDIOH_API_RC sdioh_query_device(sdioh_info_t *si);
extern SDIOH_API_RC sdioh_config_device(sdioh_info_t *si);

extern uint sdioh_query_iofnum(sdioh_info_t *sdioh);

#endif
