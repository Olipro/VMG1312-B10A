/*
    Copyright 2000-2010 Broadcom Corporation

    <:label-BRCM:2012:DUAL/GPL:standard 
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License, version 2, as published by
    the Free Software Foundation (the "GPL").
    
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    
    
    A copy of the GPL is available at http://www.broadcom.com/licenses/GPLv2.php, or by
    writing to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
    
    :> 

*/                       

/***************************************************************************
 * File Name  : nandflash.c
 *
 * Description: This file implements the Broadcom DSL defined flash api for
 *              for NAND flash parts.
 ***************************************************************************/

/** Includes. **/
#if 1//__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11 
#ifdef _CFE_
#include "lib_types.h"
#include "lib_printf.h"
#include "lib_string.h"
#include "lib_malloc.h"
#include "bcm_map.h"  
#include "bcmtypes.h"
#include "bcm_hwdefs.h"
#include "flash_api.h"
#include "jffs2.h"
#else
// __MSTC__, zongyue
#include <linux/unistd.h>
#include <linux/kernel.h>
#include <linux/jffs2.h>
#include <linux/string.h>
#include <asm/addrspace.h>
#include "bcm_map.h"
#include "bcmtypes.h"
#include "bcm_hwdefs.h"
#include "flash_api.h"
#include "flash_common.h"
#endif
#else
#include "lib_types.h"
#include "lib_printf.h"
#include "lib_string.h"
#include "lib_malloc.h"
#include "bcm_map_part.h" 
#include "bcmtypes.h"
#include "bcm_hwdefs.h"
#include "flash_api.h"
#include "jffs2.h"
#if defined(CFG_RAMAPP) && (INC_SPI_FLASH_DRIVER==1)
#include "cfe_timer.h"
#endif
#endif

#if defined(_BCM96368_) || defined(_BCM96816_) || defined(_BCM96362_) || defined(_BCM96328_)
/* Not used on non-BCM963268 chips but prevents compile errors. */
#define NC_BLK_SIZE_2048K       0x60000000
#define NC_BLK_SIZE_1024K       0x50000000
#define NC_BLK_SIZE_256K        0x40000000
#define NC_PG_SIZE_8K           0x00300000
#define NC_PG_SIZE_4K           0x00200000
#define NAC_FAST_PGM_RDIN       0x10000000
#define NAC_PARTIAL_PAGE_EN     0x04000000
#define NAC_ECC_LVL_0_SHIFT     20     
#define NAC_ECC_LVL_0_MASK      0x00f00000
#define NAC_ECC_LVL_SHIFT       16     
#define NAC_ECC_LVL_MASK        0x000f0000
#define NAC_SPARE_SZ_SHIFT      0
#define NAC_SPARE_SZ_MASK       0x0000003f
#define NAC_ECC_LVL_DISABLE     0
#define NAC_ECC_LVL_BCH_1       1
#define NAC_ECC_LVL_BCH_2       2
#define NAC_ECC_LVL_BCH_3       3
#define NAC_ECC_LVL_BCH_4       4
#define NAC_ECC_LVL_BCH_5       5
#define NAC_ECC_LVL_BCH_6       6
#define NAC_ECC_LVL_BCH_7       7
#define NAC_ECC_LVL_BCH_8       8
#define NAC_ECC_LVL_BCH_9       9
#define NAC_ECC_LVL_BCH_10      10
#define NAC_ECC_LVL_BCH_11      11
#define NAC_ECC_LVL_BCH_12      12
#define NAC_ECC_LVL_RESVD_1     13
#define NAC_ECC_LVL_RESVD_2     14
#define NAC_ECC_LVL_HAMMING     15
#endif

/* for debugging in jtag */
#if !defined(CFG_RAMAPP)
#define static 
#endif


/** Defines. **/

#define SPARE_MAX_SIZE          (27 * 16)
#define CTRLR_CACHE_SIZE        512

#define NAND_CI_CELLTYPE_MSK    0x00000c00
#define NAND_IS_MLC(chip)               \
    (NAND->NandRevision > 0x00000202 && \
     (chip)->chip_page_size > 512 &&    \
     ((chip)->chip_device_id & NAND_CI_CELLTYPE_MSK))
#define NAND_CHIPID(chip)       ((chip)->chip_device_id >> 16)             

/* Flash manufacturers. */
#define FLASHTYPE_SAMSUNG       0xec
#define FLASHTYPE_ST            0x20
#define FLASHTYPE_MICRON        0x2c
#define FLASHTYPE_HYNIX         0xad
#define FLASHTYPE_TOSHIBA       0x98
#define FLASHTYPE_MXIC          0xc2
#define FLASHTYPE_SPANSION      0x01
#define FLASHTYPE_WINBOND       0xef

/* Samsung flash parts. */
#define SAMSUNG_K9F5608U0A      0x55
#define SAMSUNG_K9F1208U0       0x76
#if 1//__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11 
#define SAMSUNG_K9F1G08U0D      0xf1
#endif

/* ST flash parts. */
#define ST_NAND512W3A2CN6       0x76
#define ST_NAND01GW3B2CN6       0xf1

/* Micron flash parts. */
#define MICRON_MT29F1G08AAC     0xf1

/* Hynix flash parts. */
#define HYNIX_H27U1G8F2B        0xf1
#define HYNIX_H27U518S2C        0x76

/* MXIC flash parts */
#define MXIC_MX30LF1208AA       0xf0
#define MXIC_MX30LF1G08AA       0xf1

/* TOSHIBA flash parts. */
#define TOSHIBA_TC58NVG0S3E     0xd1
#define TOSHIBA_TC58NVM9S3E		0xf0

/* SPANSION flash parts */
#define SPANSION_S34ML01G1      0xf1
#define SPANSION_S34ML02G1      0xda
#define SPANSION_S34ML04G1      0xdc

/* WINBOND flash parts */
#define WINBOND_W29N01GV        0xf1

/* Flash id to name mapping. */
#define NAND_MAKE_ID(A,B)    \
    (((unsigned short) (A) << 8) | ((unsigned short) B & 0xff))

#define NAND_FLASH_DEVICES                                                    \
  {{NAND_MAKE_ID(FLASHTYPE_SAMSUNG,SAMSUNG_K9F5608U0A),"Samsung K9F5608U0"},  \
   {NAND_MAKE_ID(FLASHTYPE_SAMSUNG,SAMSUNG_K9F1208U0),"Samsung K9F1208U0"},   \
   {NAND_MAKE_ID(FLASHTYPE_SAMSUNG,SAMSUNG_K9F1G08U0D),"Samsung K9F1G08U0D"}, \
   {NAND_MAKE_ID(FLASHTYPE_ST,ST_NAND512W3A2CN6),"ST NAND512W3A2CN6"},        \
   {NAND_MAKE_ID(FLASHTYPE_ST,ST_NAND01GW3B2CN6),"ST NAND01GW3B2CN6"},        \
   {NAND_MAKE_ID(FLASHTYPE_MICRON,MICRON_MT29F1G08AAC),"Micron MT29F1G08AAC"},\
   {NAND_MAKE_ID(FLASHTYPE_HYNIX,HYNIX_H27U1G8F2B),"Hynix H27U1G8F2B"},       \
   {NAND_MAKE_ID(FLASHTYPE_HYNIX,HYNIX_H27U518S2C),"Hynix H27U518S2C"},       \
   {NAND_MAKE_ID(FLASHTYPE_MXIC,MXIC_MX30LF1208AA),"MXIC MX30LF1208AA"},      \
   {NAND_MAKE_ID(FLASHTYPE_MXIC,MXIC_MX30LF1G08AA),"MXIC MX30LF1G08AA"},      \
   {NAND_MAKE_ID(FLASHTYPE_TOSHIBA,TOSHIBA_TC58NVG0S3E),"TOSHIBA TC58NVG0S3E"},\
   {NAND_MAKE_ID(FLASHTYPE_TOSHIBA,TOSHIBA_TC58NVM9S3E),"TOSHIBA TC58NVM9S3E"},\
   {NAND_MAKE_ID(FLASHTYPE_SPANSION,SPANSION_S34ML01G1),"Spansion S34ML01G1"},\
   {NAND_MAKE_ID(FLASHTYPE_SPANSION,SPANSION_S34ML02G1),"Spansion S34ML02G1"},\
   {NAND_MAKE_ID(FLASHTYPE_SPANSION,SPANSION_S34ML04G1),"Spansion S34ML04G1"},\
   {NAND_MAKE_ID(FLASHTYPE_WINBOND,WINBOND_W29N01GV),"WINBOND W29N01GV"},\
   {0,""}                                                                     \
  }

/* Condition to determine the spare layout. */
#define LAYOUT_PARMS(L,S,P)     \
    (((unsigned long)(L)<<28) | ((unsigned long)(S)<<16) | (P))

/* Each bit in the ECCMSK array represents a spare area byte. Bits that are
 * set correspond to spare area bytes that are reserved for the ECC or bad
 * block indicator. Bits that are not set can be used for data such as the
 * JFFS2 clean marker. This macro returns 0 if the spare area byte at offset,
 * OFS, is available and non-0 if it is being used for the ECC or BI.
 */
#define ECC_MASK_BIT(ECCMSK, OFS)   (ECCMSK[OFS / 8] & (1 << (OFS % 8)))

#define SPARE_BI_MARKER         0
/* __MSTC__, zongyue: merge BRCM NAND bad block detect patch */
#define SPARE_GOOD_MARKER       0xFF

#define JFFS2_CLEANMARKER      {JFFS2_MAGIC_BITMASK, \
    JFFS2_NODETYPE_CLEANMARKER, 0x0000, 0x0008}

#undef DEBUG_NAND
#if defined(DEBUG_NAND) && defined(CFG_RAMAPP)
#if 1//__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11 
#define DBG_PRINTF printk
#else
#define DBG_PRINTF printf
#endif
#else
#define DBG_PRINTF(...)
#endif

/** Externs. **/

extern void board_setleds(unsigned long);


/** Structs. **/

typedef struct SpareLayout
{
    unsigned char sl_bi_ofs[2];
    unsigned char sl_spare_mask[];
} SPARE_LAYOUT, *PSPARE_LAYOUT;

typedef struct CfeNandChip
{
    char *chip_name;
    unsigned long chip_device_id;
    unsigned long chip_base;
    unsigned long chip_total_size;
    unsigned long chip_block_size;
    unsigned long chip_page_size;
    unsigned long chip_spare_size;
    unsigned long chip_spare_step_size;
    unsigned char *chip_spare_mask;
    unsigned long chip_bi_index_1;
    unsigned long chip_bi_index_2;
} CFE_NAND_CHIP, *PCFE_NAND_CHIP;

struct flash_name_from_id
{
    unsigned short fnfi_id;
    char fnfi_name[30];
};


#if defined(CFG_RAMAPP)
/** Prototypes for CFE RAM. **/
int nand_flash_init(flash_device_info_t **flash_info);
int mpinand_flash_init(flash_device_info_t **flash_info);
static void nand_init_cleanmarker(PCFE_NAND_CHIP pchip);
static void nand_read_cfg(PCFE_NAND_CHIP pchip);
static int nand_is_blk_cleanmarker(PCFE_NAND_CHIP pchip,
    unsigned long start_addr, int write_if_not);
#if 0 /* __MSTC__, zongyue: merge BRCM NAND bad block detect patch */
static int nand_initialize_spare_area(PCFE_NAND_CHIP pchip);
#else
static int nand_is_blk_bad(PCFE_NAND_CHIP pchip, unsigned long blockNum);
static int nand_initialize_spare_area(PCFE_NAND_CHIP pchip, int eraseBadBlocks);
#endif
#if 1//__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11 
#ifdef _CFE_  
static int nand_initialize_spare_area_for_SMT(PCFE_NAND_CHIP pchip);
#endif
#endif
static void nand_mark_bad_blk(PCFE_NAND_CHIP pchip, unsigned long page_addr);
static int nand_flash_sector_erase_int(unsigned short blk);
static int nand_flash_read_buf(unsigned short blk, int offset,
    unsigned char *buffer, int len);
static int nand_flash_write_buf(unsigned short blk, int offset,
    unsigned char *buffer, int numbytes);
static int nand_flash_get_numsectors(void);
static int nand_flash_get_sector_size(unsigned short sector);
static unsigned char *nand_flash_get_memptr(unsigned short sector);
static int nand_flash_get_blk(int addr);
static int nand_flash_get_total_size(void);
static int nandflash_wait_status(unsigned long status_mask);
static inline int nandflash_wait_device(void);
static int nandflash_read_spare_area(PCFE_NAND_CHIP pchip,
    unsigned long page_addr, unsigned char *buffer, int len);
static int nandflash_write_spare_area(PCFE_NAND_CHIP pchip,
    unsigned long page_addr, unsigned char *buffer, int len);
static int nandflash_read_page(PCFE_NAND_CHIP pchip,
    unsigned long start_addr, unsigned char *buffer, int len);
static int nandflash_write_page(PCFE_NAND_CHIP pchip, unsigned long start_addr,
    unsigned char *buffer, int len);
static int nandflash_block_erase(PCFE_NAND_CHIP pchip, unsigned long blk_addr);
#if 1//__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11 
static int check_ecc_for_ffs(PCFE_NAND_CHIP pchip, UINT32 step);
#endif
#else
/** Prototypes for CFE ROM. **/
void rom_nand_flash_init(void);
static int nand_is_blk_cleanmarker(PCFE_NAND_CHIP pchip,
    unsigned long start_addr, int write_if_not);
static void nand_read_cfg(PCFE_NAND_CHIP pchip);
int nand_flash_get_sector_size(unsigned short sector);
int nand_flash_get_numsectors(void);
static int nandflash_wait_status(unsigned long status_mask);
static int nandflash_read_page(PCFE_NAND_CHIP pchip, unsigned long start_addr,
    unsigned char *buffer, int len);
int nand_flash_read_buf(unsigned short blk, int offset,
    unsigned char *buffer, int len);
static inline void nandflash_copy_from_cache(unsigned char *buffer,
    int offset, int numbytes);
static inline void nandflash_copy_from_spare(unsigned char *buffer,
    int numbytes);
static int nandflash_wait_status(unsigned long status_mask);
static inline int nandflash_wait_cmd(void);
static inline int nandflash_wait_device(void);
static inline int nandflash_wait_cache(void);
static inline int nandflash_wait_spare(void);
static int nandflash_check_ecc(void);
static int check_ecc_for_ffs(PCFE_NAND_CHIP pchip, UINT32 step);
#endif


#if defined(CFG_RAMAPP)
/** Variables for CFE RAM. **/
CFE_NAND_CHIP g_chip = {NULL,0,0,0,0,0,0};
static unsigned char g_spare_cleanmarker[SPARE_MAX_SIZE];

static flash_device_info_t flash_nand_dev =
    {
        0xffff,
        FLASH_IFC_NAND,
        "",
        nand_flash_sector_erase_int,
        nand_flash_read_buf,
        nand_flash_write_buf,
        nand_flash_get_numsectors,
        nand_flash_get_sector_size,
        nand_flash_get_memptr,
        nand_flash_get_blk,
        nand_flash_get_total_size
    };

/* B,B,0,0,0,0,E,E-E,0,0,0,0,0,0,0-0,0,0,0,0,0,E,E-E,0,0,0,0,0,0,0
 * 0,0,0,0,0,0,E,E-E,0,0,0,0,0,0,0-0,0,0,0,0,0,E,E-E,0,0,0,0,0,0,0
 */
SPARE_LAYOUT brcmnand_oob_64 =
    {{0, 1}, {0xc3, 0x01, 0xc0, 0x01, 0xc0, 0x01, 0xc0, 0x01}};

/* 0,0,0,0,0,B,E,E-E,0,0,0,0,0,0,0 */
SPARE_LAYOUT brcmnand_oob_16 =
    {{5, 5}, {0xe0, 0x01}};

/* 0,0,0,0,0,B,0,0-0,E,E,E,E,E,E,E */
SPARE_LAYOUT brcmnand_oob_bch4_512 =
    {{5, 5}, {0x20, 0xfe}};

/* B,B,0,0,0,0,E,E-E,0,0,0,0,0,0,0-0,0,0,0,0,0,E,E-E,0,0,0,0,0,0,0
 * 0,0,0,0,0,0,E,E-E,0,0,0,0,0,0,0-0,0,0,0,0,0,E,E-E,0,0,0,0,0,0,0
 * 0,0,0,0,0,0,E,E-E,0,0,0,0,0,0,0-0,0,0,0,0,0,E,E-E,0,0,0,0,0,0,0
 * 0,0,0,0,0,0,E,E-E,0,0,0,0,0,0,0-0,0,0,0,0,0,E,E-E,0,0,0,0,0,0,0
 */
SPARE_LAYOUT brcmnand_oob_128 =
    {{0, 1}, {0xc3, 0x01, 0xc0, 0x01, 0xc0, 0x01, 0xc0, 0x01,
              0xc0, 0x01, 0xc0, 0x01, 0xc0, 0x01, 0xc0, 0x01}};

/* B,0,0,0,0,0,0,0-0,E,E,E,E,E,E,E-0,0,0,0,0,0,0,0-0,E,E,E,E,E,E,E
 * 0,0,0,0,0,0,0,0-0,E,E,E,E,E,E,E-0,0,0,0,0,0,0,0-0,E,E,E,E,E,E,E
 * 0,0,0,0,0,0,0,0-0,E,E,E,E,E,E,E-0,0,0,0,0,0,0,0-0,E,E,E,E,E,E,E
 * 0,0,0,0,0,0,0,0-0,E,E,E,E,E,E,E-0,0,0,0,0,0,0,0-0,E,E,E,E,E,E,E
 */
SPARE_LAYOUT brcmnand_oob_bch4_4k =
    {{0, 0}, {0x01, 0xfe, 0x00, 0xfe, 0x00, 0xfe, 0x00, 0xfe,
              0x00, 0xfe, 0x00, 0xfe, 0x00, 0xfe, 0x00, 0xfe}};

/* B,0,0,0,0,0,0,0-0,E,E,E,E,E,E,E-0,0,0,0,0,0,0,0-0,E,E,E,E,E,E,E
 * 0,0,0,0,0,0,0,0-0,E,E,E,E,E,E,E-0,0,0,0,0,0,0,0-0,E,E,E,E,E,E,E
 */
SPARE_LAYOUT brcmnand_oob_bch4_2k =
    {{0, 0}, {0x01, 0xfe, 0x00, 0xfe, 0x00, 0xfe, 0x00, 0xfe}};

/* B,0,0,E,E,E,E,E-E,E,E,E,E,E,E,E-0,0,0,E,E,E,E,E-E,E,E,E,E,E,E,E
 * 0,0,0,E,E,E,E,E-E,E,E,E,E,E,E,E-0,0,0,E,E,E,E,E-E,E,E,E,E,E,E,E
 */
SPARE_LAYOUT brcmnand_oob_bch8_16_2k =
    {{0, 0}, {0xf9, 0xff, 0xf8, 0xff, 0xf8, 0xff, 0xf8, 0xff}};

/* B,0,0,E,E,E,E,E-E,E,E,E,E,E,E,E-0,0,0,E,E,E,E,E,-E,E,E,E,E,E,E,E
 * 0,0,0,E,E,E,E,E-E,E,E,E,E,E,E,E-0,0,0,E,E,E,E,E,-E,E,E,E,E,E,E,E
 * 0,0,0,E,E,E,E,E-E,E,E,E,E,E,E,E-0,0,0,E,E,E,E,E,-E,E,E,E,E,E,E,E
 * 0,0,0,E,E,E,E,E-E,E,E,E,E,E,E,E-0,0,0,E,E,E,E,E,-E,E,E,E,E,E,E,E
 */
SPARE_LAYOUT brcmnand_oob_bch8_16_4k =
    {{0, 0}, {0xf9, 0xff, 0xf8, 0xff, 0xf8, 0xff, 0xf8, 0xff,
              0xf8, 0xff, 0xf8, 0xff, 0xf8, 0xff, 0xf8, 0xff}};

/* B,0,0,0,0,0,0,0-0,0,0,0,0,0,E,E-E,E,E,E,E,E,E,E-E,E,E
 * 0,0,0,0,0-0,0,0,0,0,0,0,0-0,E,E,E,E,E,E,E-E,E,E,E,E,E
 * 0,0-0,0,0,0,0,0,0,0-0,0,0,0,E,E,E,E-E,E,E,E,E,E,E,E-E
 * 0,0,0,0,0,0,0-0,0,0,0,0,0,0,E-E,E,E,E,E,E,E,E-E,E,E,E
 * 0,0,0,0-0,0,0,0,0,0,0,0-0,0,E,E,E,E,E,E-E,E,E,E,E,E,E
 * 0-0,0,0,0,0,0,0,0-0,0,0,0,0,E,E,E-E,E,E,E,E,E,E,E-E,E
 * 0,0,0,0,0,0-0,0,0,0,0,0,0,0-E,E,E,E,E,E,E,E-E,E,E,E,E
 * 0,0,0-0,0,0,0,0,0,0,0-0,0,0,E,E,E,E,E-E,E,E,E,E,E,E,E
 */
SPARE_LAYOUT brcmnand_oob_bch8_27_4k =
    {{0, 0}, {0x01, 0xc0, 0xff, 0x07, 0x00, 0xfe, 0x3f, 0x00, 0xf0, 0xff,
              0x01, 0x80, 0xff, 0x0f, 0x00, 0xfc, 0x7f, 0x00, 0xe0, 0xff,
              0x03, 0x00, 0xff, 0x1f, 0x00, 0xf8, 0xff}};

/* B,0,0,0,0,0,0,E-E,E,E,E,E,E,E,E-E,E,E,E,E,E,E,E-E,E,E
 * 0,0,0,0,0-0,0,E,E,E,E,E,E-E,E,E,E,E,E,E,E-E,E,E,E,E,E
 * 0,0-0,0,0,0,0,E,E,E-E,E,E,E,E,E,E,E-E,E,E,E,E,E,E,E-E
 * 0,0,0,0,0,0,0-E,E,E,E,E,E,E,E-E,E,E,E,E,E,E,E-E,E,E,E
 * 0,0,0,0-0,0,0,E,E,E,E,E-E,E,E,E,E,E,E,E-E,E,E,E,E,E,E
 * 0-0,0,0,0,0,0,E,E-E,E,E,E,E,E,E,E-E,E,E,E,E,E,E,E-E,E
 * 0,0,0,0,0,0-0,E,E,E,E,E,E,E-E,E,E,E,E,E,E,E-E,E,E,E,E
 * 0,0,0-0,0,0,0,E,E,E,E-E,E,E,E,E,E,E,E-E,E,E,E,E,E,E,E
 */
SPARE_LAYOUT brcmnand_oob_bch12_27_4k =
    {{0, 0}, {0x81, 0xff, 0xff, 0x07, 0xfc, 0xff, 0x3f, 0xe0, 0xff, 0xff,
              0x01, 0xff, 0xff, 0x0f, 0xf8, 0xff, 0x7f, 0xc0, 0xff, 0xff,
              0x03, 0xfe, 0xff, 0x1f, 0xf0, 0xff, 0xff}};

/* B,0,0,0,0,0,0,0-0,E,E,E,E,E,E,E-0,0,0,0,0,0,0,0-0,E,E,E,E,E,E,E
 * 0,0,0,0,0,0,0,0-0,E,E,E,E,E,E,E-0,0,0,0,0,0,0,0-0,E,E,E,E,E,E,E
 * 0,0,0,0,0,0,0,0-0,E,E,E,E,E,E,E-0,0,0,0,0,0,0,0-0,E,E,E,E,E,E,E
 * 0,0,0,0,0,0,0,0-0,E,E,E,E,E,E,E-0,0,0,0,0,0,0,0-0,E,E,E,E,E,E,E
 * 0,0,0,0,0,0,0,0-0,E,E,E,E,E,E,E-0,0,0,0,0,0,0,0-0,E,E,E,E,E,E,E
 * 0,0,0,0,0,0,0,0-0,E,E,E,E,E,E,E-0,0,0,0,0,0,0,0-0,E,E,E,E,E,E,E
 * 0,0,0,0,0,0,0,0-0,E,E,E,E,E,E,E-0,0,0,0,0,0,0,0-0,E,E,E,E,E,E,E
 * 0,0,0,0,0,0,0,0-0,E,E,E,E,E,E,E-0,0,0,0,0,0,0,0-0,E,E,E,E,E,E,E
 */
SPARE_LAYOUT brcmnand_oob_bch4_8k =
    {{0, 0}, {0x01, 0xfe, 0x00, 0xfe, 0x00, 0xfe, 0x00, 0xfe, 0x00, 0xfe,
              0x00, 0xfe, 0x00, 0xfe, 0x00, 0xfe, 0x00, 0xfe, 0x00, 0xfe,
              0x00, 0xfe, 0x00, 0xfe, 0x00, 0xfe, 0x00, 0xfe, 0x00, 0xfe,
              0x00, 0xfe}};

/* B,0,0,E,E,E,E,E-E,E,E,E,E,E,E,E-0,0,0,E,E,E,E,E-E,E,E,E,E,E,E,E
 * 0,0,0,E,E,E,E,E-E,E,E,E,E,E,E,E-0,0,0,E,E,E,E,E-E,E,E,E,E,E,E,E
 * 0,0,0,E,E,E,E,E-E,E,E,E,E,E,E,E-0,0,0,E,E,E,E,E-E,E,E,E,E,E,E,E
 * 0,0,0,E,E,E,E,E-E,E,E,E,E,E,E,E-0,0,0,E,E,E,E,E-E,E,E,E,E,E,E,E
 * 0,0,0,E,E,E,E,E-E,E,E,E,E,E,E,E-0,0,0,E,E,E,E,E-E,E,E,E,E,E,E,E
 * 0,0,0,E,E,E,E,E-E,E,E,E,E,E,E,E-0,0,0,E,E,E,E,E-E,E,E,E,E,E,E,E
 * 0,0,0,E,E,E,E,E-E,E,E,E,E,E,E,E-0,0,0,E,E,E,E,E-E,E,E,E,E,E,E,E
 * 0,0,0,E,E,E,E,E-E,E,E,E,E,E,E,E-0,0,0,E,E,E,E,E-E,E,E,E,E,E,E,E
 */
SPARE_LAYOUT brcmnand_oob_bch8_16_8k =
    {{0, 0}, {0xf9, 0xff, 0xf8, 0xff, 0xf8, 0xff, 0xf8, 0xff, 0xf8, 0xff,
              0xf8, 0xff, 0xf8, 0xff, 0xf8, 0xff, 0xf8, 0xff, 0xf8, 0xff,
              0xf8, 0xff, 0xf8, 0xff, 0xf8, 0xff, 0xf8, 0xff, 0xf8, 0xff,
              0xf8, 0xff}};

/* B,0,0,0,0,0,0,0-0,0,0,0,0,0,E,E-E,E,E,E,E,E,E,E-E,E,E
 * 0,0,0,0,0-0,0,0,0,0,0,0,0-0,E,E,E,E,E,E,E-E,E,E,E,E,E
 * 0,0-0,0,0,0,0,0,0,0-0,0,0,0,E,E,E,E-E,E,E,E,E,E,E,E-E
 * 0,0,0,0,0,0,0-0,0,0,0,0,0,0,E-E,E,E,E,E,E,E,E-E,E,E,E
 * 0,0,0,0-0,0,0,0,0,0,0,0-0,0,E,E,E,E,E,E-E,E,E,E,E,E,E
 * 0-0,0,0,0,0,0,0,0-0,0,0,0,0,E,E,E-E,E,E,E,E,E,E,E-E,E
 * 0,0,0,0,0,0-0,0,0,0,0,0,0,0-E,E,E,E,E,E,E,E-E,E,E,E,E
 * 0,0,0-0,0,0,0,0,0,0,0-0,0,0,E,E,E,E,E-E,E,E,E,E,E,E,E
 * 0,0,0,0,0,0,0,0-0,0,0,0,0,0,E,E-E,E,E,E,E,E,E,E-E,E,E
 * 0,0,0,0,0-0,0,0,0,0,0,0,0-0,E,E,E,E,E,E,E-E,E,E,E,E,E
 * 0,0-0,0,0,0,0,0,0,0-0,0,0,0,E,E,E,E-E,E,E,E,E,E,E,E-E
 * 0,0,0,0,0,0,0-0,0,0,0,0,0,0,E-E,E,E,E,E,E,E,E-E,E,E,E
 * 0,0,0,0-0,0,0,0,0,0,0,0-0,0,E,E,E,E,E,E-E,E,E,E,E,E,E
 * 0-0,0,0,0,0,0,0,0-0,0,0,0,0,E,E,E-E,E,E,E,E,E,E,E-E,E
 * 0,0,0,0,0,0-0,0,0,0,0,0,0,0-E,E,E,E,E,E,E,E-E,E,E,E,E
 * 0,0,0-0,0,0,0,0,0,0,0-0,0,0,E,E,E,E,E-E,E,E,E,E,E,E,E
 */
SPARE_LAYOUT brcmnand_oob_bch8_27_8k =
    {{0, 0}, {0x01, 0xc0, 0xff, 0x07, 0x00, 0xfe, 0x3f, 0x00, 0xf0, 0xff,
              0x01, 0x80, 0xff, 0x0f, 0x00, 0xfc, 0x7f, 0x00, 0xe0, 0xff,
              0x03, 0x00, 0xff, 0x1f, 0x00, 0xf8, 0xff, 0x00, 0xc0, 0xff,
              0x07, 0x00, 0xfe, 0x3f, 0x00, 0xf0, 0xff, 0x01, 0x80, 0xff,
              0x0f, 0x00, 0xfc, 0x7f, 0x00, 0xe0, 0xff, 0x03, 0x00, 0xff,
              0x1f, 0x00, 0xf8, 0xff}};

/* B,0,0,0,0,0,0,E-E,E,E,E,E,E,E,E-E,E,E,E,E,E,E,E-E,E,E
 * 0,0,0,0,0-0,0,E,E,E,E,E,E-E,E,E,E,E,E,E,E-E,E,E,E,E,E
 * 0,0-0,0,0,0,0,E,E,E-E,E,E,E,E,E,E,E-E,E,E,E,E,E,E,E-E
 * 0,0,0,0,0,0,0-E,E,E,E,E,E,E,E-E,E,E,E,E,E,E,E-E,E,E,E
 * 0,0,0,0-0,0,0,E,E,E,E,E-E,E,E,E,E,E,E,E-E,E,E,E,E,E,E
 * 0-0,0,0,0,0,0,E,E-E,E,E,E,E,E,E,E-E,E,E,E,E,E,E,E-E,E
 * 0,0,0,0,0,0-0,E,E,E,E,E,E,E-E,E,E,E,E,E,E,E-E,E,E,E,E
 * 0,0,0-0,0,0,0,E,E,E,E-E,E,E,E,E,E,E,E-E,E,E,E,E,E,E,E
 * 0,0,0,0,0,0,0,E-E,E,E,E,E,E,E,E-E,E,E,E,E,E,E,E-E,E,E
 * 0,0,0,0,0-0,0,E,E,E,E,E,E-E,E,E,E,E,E,E,E-E,E,E,E,E,E
 * 0,0-0,0,0,0,0,E,E,E-E,E,E,E,E,E,E,E-E,E,E,E,E,E,E,E-E
 * 0,0,0,0,0,0,0-E,E,E,E,E,E,E,E-E,E,E,E,E,E,E,E-E,E,E,E
 * 0,0,0,0-0,0,0,E,E,E,E,E-E,E,E,E,E,E,E,E-E,E,E,E,E,E,E
 * 0-0,0,0,0,0,0,E,E-E,E,E,E,E,E,E,E-E,E,E,E,E,E,E,E-E,E
 * 0,0,0,0,0,0-0,E,E,E,E,E,E,E-E,E,E,E,E,E,E,E-E,E,E,E,E
 * 0,0,0-0,0,0,0,E,E,E,E-E,E,E,E,E,E,E,E-E,E,E,E,E,E,E,E
 */
SPARE_LAYOUT brcmnand_oob_bch12_27_8k =
    {{0, 0}, {0x81, 0xff, 0xff, 0x07, 0xfc, 0xff, 0x3f, 0xe0, 0xff, 0xff,
              0x01, 0xff, 0xff, 0x0f, 0xf8, 0xff, 0x7f, 0xc0, 0xff, 0xff,
              0x03, 0xfe, 0xff, 0x1f, 0xf0, 0xff, 0xff, 0x81, 0xff, 0xff,
              0x07, 0xfc, 0xff, 0x3f, 0xe0, 0xff, 0xff, 0x01, 0xff, 0xff,
              0x0f, 0xf8, 0xff, 0x7f, 0xc0, 0xff, 0xff, 0x03, 0xfe, 0xff,
              0x1f, 0xf0, 0xff, 0xff}};

#else
/** Variables for CFE ROM. **/
CFE_NAND_CHIP g_chip;
#endif

#if defined(CONFIG_BRCMNAND_USE_HW_SEMAPHORE) && defined(CFG_RAMAPP)
#include <linux/hardirq.h>
#include <linux/sched.h>
#define BCHP_NAND_SEM_DRV_ID	0xaa
#define BCHP_NAND_SEM_DRV_MAX_SCHEDULE_CNT	3000		/* max schedule count for dead lock check */
#define BCHP_NAND_SEM_DRV_SCHEDULE_TIMEOUT	(HZ/100)	 /* sleep 10ms in every retry */

/* michael.zhang@mitrastar.cn:  Use Hardware Semaphore, prevent access conflict between mtd and nandflash driver*/
/* Set the semaphore register until succeed. 
     Hardware will refuse to change the register value if it is already contains a non-zero value
*/
static void nand_flash_set_semaphore(void)
{
	unsigned int cnt=0;
	NAND->NandSemaphore = BCHP_NAND_SEM_DRV_ID;
	while(NAND->NandSemaphore!=BCHP_NAND_SEM_DRV_ID)
	{
		if (!in_atomic() && !irqs_disabled() && !in_interrupt())
		{
			if(cnt ++ == BCHP_NAND_SEM_DRV_MAX_SCHEDULE_CNT)
			{
				printk("NAND Drv: #### Can't get NAND Flash hardware semaphore after %u times schedule. Must be in dead lock ####\n",BCHP_NAND_SEM_DRV_MAX_SCHEDULE_CNT);
				NAND->NandSemaphore = 0;
			}
			else
			{
				set_current_state(TASK_UNINTERRUPTIBLE);
				schedule_timeout(BCHP_NAND_SEM_DRV_SCHEDULE_TIMEOUT);
			}
		}
		else	/* oops! should not enter here */
		{
			printk("NAND Drv: %s: Bug!!!, Can't get NAND Flash hardware semaphore and can't yield CPU \n", __FUNCTION__);	
			NAND->NandSemaphore = 0;
		}
		NAND->NandSemaphore = BCHP_NAND_SEM_DRV_ID;
	}
}

/* Write zero to clear the semaphore, then other part can set their semaphore ID successfully.*/
static void nand_flash_clr_semaphore(void)
{
    NAND->NandSemaphore = 0;
}

#endif

#if defined(CFG_RAMAPP)
/***************************************************************************
 * Function Name: nand_flash_init
 * Description  : Initialize flash part.
 * Returns      : FLASH_API_OK or FLASH_API_ERROR
 ***************************************************************************/
int nand_flash_init(flash_device_info_t **flash_info)
{
    static int initialized = 0;
    int ret = FLASH_API_OK;

    if( initialized == 0 )
    {
        PCFE_NAND_CHIP pchip = &g_chip;
        static struct flash_name_from_id fnfi[] = NAND_FLASH_DEVICES;
        struct flash_name_from_id *fnfi_ptr;

        DBG_PRINTF(">> nand_flash_init - entry\n");

        /* Enable NAND data on MII ports. */
#if !defined(_BCM96328_)
        PERF->blkEnables |= NAND_CLK_EN;
#endif
#if (defined(_BCM96362_) || defined(_BCM963268_)) && (INC_SPI_FLASH_DRIVER==1)
        GPIO->GPIOBaseMode |= NAND_GPIO_OVERRIDE;
#endif

        NAND->NandNandBootConfig = NBC_AUTO_DEV_ID_CFG | 1;
#ifdef _CFE_    //__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11 
#if (INC_SPI_FLASH_DRIVER==1)
        cfe_usleep(1000);
#endif
#endif
        NAND->NandCmdAddr = 0;
        NAND->NandCmdExtAddr = 0;
        NAND->NandCmdStart = NCMD_DEV_ID_READ;
        nandflash_wait_device();

        /* Read the NAND flash chip id. */
        pchip->chip_device_id = NAND->NandFlashDeviceId;
        flash_nand_dev.flash_device_id = NAND_CHIPID(pchip);

        for( fnfi_ptr = fnfi; fnfi_ptr->fnfi_id != 0; fnfi_ptr++ )
        {
            if( fnfi_ptr->fnfi_id == flash_nand_dev.flash_device_id )
            {
                strcpy(flash_nand_dev.flash_device_name, fnfi_ptr->fnfi_name);
                break;
            }
        }

        /* If NAND chip is not in the list of NAND chips, the correct
         * configuration has still been set by the NAND controller.
         */
        if( flash_nand_dev.flash_device_name[0] == '\0' )
            strcpy(flash_nand_dev.flash_device_name, "<not identified>");

        *flash_info = &flash_nand_dev;

        NAND->NandCsNandXor = 0;
#if defined(CONFIG_BRCMNAND_USE_HW_SEMAPHORE)
        NAND->NandSemaphore = 0;	/* clear the semaphore */
#endif
        pchip->chip_base = 0;
        nand_read_cfg(pchip);
        nand_init_cleanmarker(pchip);

        /* If the first block's spare area is not a JFFS2 cleanmarker,
         * initialize all block's spare area to a cleanmarker.
         */
        if( !nand_is_blk_cleanmarker(pchip, 0, 0) )
#if 0 /* __MSTC__, zongyue: merge BRCM NAND bad block detect patch */
            ret = nand_initialize_spare_area(pchip);
#else
            ret = nand_initialize_spare_area(pchip, 0);
#endif
#if 1//__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11 
#ifdef _CFE_
        /* If the latest block's spare area is not a JFFS2 cleanmarker,
         * initialize all block's spare area to a cleanmarker.
         */
        ret = nand_initialize_spare_area_for_SMT(pchip);
#endif
#endif
        DBG_PRINTF(">> nand_flash_init - return %d\n", ret);

        initialized = 1;
    }
    else
        *flash_info = &flash_nand_dev;

    return( ret );
} /* nand_flash_init */

/***************************************************************************
 * Function Name: nand_init_cleanmarker
 * Description  : Initializes the JFFS2 clean marker buffer.
 * Returns      : None.
 ***************************************************************************/
static void nand_init_cleanmarker(PCFE_NAND_CHIP pchip)
{
    unsigned short cleanmarker[] = JFFS2_CLEANMARKER;
    unsigned char *pcm = (unsigned char *) cleanmarker;
    unsigned char *spare_mask = pchip->chip_spare_mask;
    int i, j;

    /* Skip spare area offsets reserved for ECC bytes. */
    for( i = 0, j = 0; i < pchip->chip_spare_size; i++ )
    {
        if( ECC_MASK_BIT(spare_mask, i) == 0 && j < sizeof(cleanmarker))
            g_spare_cleanmarker[i] = pcm[j++];
        else
            g_spare_cleanmarker[i] = 0xff;
    }
} /* nand_init_cleanmarker */

#else
/***************************************************************************
 * Function Name: rom_nand_flash_init
 * Description  : Initialize flash part just enough to read blocks.
 * Returns      : FLASH_API_OK or FLASH_API_ERROR
 ***************************************************************************/
void rom_nand_flash_init(void)
{
    PCFE_NAND_CHIP pchip = &g_chip;

    /* Enable NAND data on MII ports. */
#if !defined(_BCM96328_)
    PERF->blkEnables |= NAND_CLK_EN;
#endif
    NAND->NandNandBootConfig = NBC_AUTO_DEV_ID_CFG | 1;

    pchip->chip_base = 0;

    /* Read the chip id. Only use the most signficant 16 bits. */
    pchip->chip_device_id = NAND->NandFlashDeviceId;
    nand_read_cfg(pchip);

} /* nand_flash_init */
#endif

/***************************************************************************
 * Function Name: nand_read_cfg
 * Description  : Reads and stores the chip configuration.
 * Returns      : None.
 ***************************************************************************/
static void nand_read_cfg(PCFE_NAND_CHIP pchip)
{
    /* Read chip configuration. */
    unsigned long cfg = NAND->NandConfig;

    /* Special case changes from what the NAND controller configured. */
    switch(NAND_CHIPID(pchip))
    {
    case NAND_MAKE_ID(FLASHTYPE_HYNIX,HYNIX_H27U1G8F2B):
        cfg &= ~(NC_DEV_SIZE_MASK | NC_FUL_ADDR_MASK | (0xff << NC_BLK_ADDR_SHIFT));

        /* 128 MB device size, 4 full address bytes, 2 column address bytes, 2 block address bytes */
        cfg |= (5 << NC_DEV_SIZE_SHIFT) | (0x04 << NC_FUL_ADDR_SHIFT) | (0x22 << NC_BLK_ADDR_SHIFT);
        NAND->NandConfig = cfg;
        break;

    case NAND_MAKE_ID(FLASHTYPE_SAMSUNG,SAMSUNG_K9F5608U0A):
    case NAND_MAKE_ID(FLASHTYPE_SAMSUNG,SAMSUNG_K9F1208U0):
    case NAND_MAKE_ID(FLASHTYPE_HYNIX,HYNIX_H27U518S2C):
        /* Set device id "cell type" to 0 (SLC). */
        pchip->chip_device_id &= ~NAND_CI_CELLTYPE_MSK;
        NAND->NandFlashDeviceId = pchip->chip_device_id;
        break;

    case NAND_MAKE_ID(FLASHTYPE_MXIC,MXIC_MX30LF1208AA):
    	/* This 64MB device was detected as 256MB device on 63268. Manually update
    	 * device size in the cfg register.
    	 */
    	cfg &= ~NC_DEV_SIZE_MASK;
    	cfg |= (0x04<<NC_DEV_SIZE_SHIFT);
    	NAND->NandConfig = cfg;
    	break;
    case NAND_MAKE_ID(FLASHTYPE_TOSHIBA,TOSHIBA_TC58NVM9S3E):
        cfg &= ~(NC_DEV_SIZE_MASK);
	    cfg |= 4<<NC_DEV_SIZE_SHIFT;
	    NAND->NandConfig = cfg;
        break;
    case NAND_MAKE_ID(FLASHTYPE_SPANSION,SPANSION_S34ML01G1):
        /* Set device size to 128MB, it is misconfigured to 512MB. */
    	cfg &= ~NC_DEV_SIZE_MASK;
    	cfg |= (0x05<<NC_DEV_SIZE_SHIFT);
    	NAND->NandConfig = cfg;
        break;

    case NAND_MAKE_ID(FLASHTYPE_SPANSION,SPANSION_S34ML02G1):
        /* Set device size to 256MB, it is misconfigured to 512MB. */
    	cfg &= ~NC_DEV_SIZE_MASK;
    	cfg |= (0x06<<NC_DEV_SIZE_SHIFT);
    	NAND->NandConfig = cfg;
        break;

    case NAND_MAKE_ID(FLASHTYPE_SPANSION,SPANSION_S34ML04G1):
        /* Set block size to 128KB, it is misconfigured to 512MB. */
    	cfg &= ~NC_BLK_SIZE_MASK;
    	cfg |= NC_BLK_SIZE_128K;
    	NAND->NandConfig = cfg;
        break;
    }

    pchip->chip_total_size =
        (4 * (1 << ((cfg & NC_DEV_SIZE_MASK) >> NC_DEV_SIZE_SHIFT))) << 20;

    switch( (cfg & NC_BLK_SIZE_MASK) )
    {
    case NC_BLK_SIZE_2048K:
        pchip->chip_block_size = 2048 * 1024;
        break;

    case NC_BLK_SIZE_1024K:
        pchip->chip_block_size = 1024 * 1024;
        break;

    case NC_BLK_SIZE_512K:
        pchip->chip_block_size = 512 * 1024;
        break;

    case NC_BLK_SIZE_256K:
        pchip->chip_block_size = 256 * 1024;
        break;

    case NC_BLK_SIZE_128K:
        pchip->chip_block_size = 128 * 1024;
        break;

    case NC_BLK_SIZE_16K:
        pchip->chip_block_size = 16 * 1024;
        break;

    case NC_BLK_SIZE_8K:
        pchip->chip_block_size = 8 * 1024;
        break;
    }

    switch( (cfg & NC_PG_SIZE_MASK) )
    {
    case NC_PG_SIZE_8K:
        pchip->chip_page_size = 8 * 1024;
        break;

    case NC_PG_SIZE_4K:
        pchip->chip_page_size = 4 * 1024;
        break;

    case NC_PG_SIZE_2K:
        pchip->chip_page_size = 2 * 1024;
        break;

    case NC_PG_SIZE_512B:
        pchip->chip_page_size = 512;
        break;
    }

    if( NAND_IS_MLC(pchip) )
    {
        unsigned long acc = NAND->NandAccControl;
        unsigned long acc_save = acc;

        if( (acc & (NAC_FAST_PGM_RDIN | NAC_PARTIAL_PAGE_EN)) != 0 )
        {
#if 1//__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11 
            printk("Disable NAND fast program read and partial page enable"
                "for MLC part.\n");
#else
            printf("Disable NAND fast program read and partial page enable"
                "for MLC part.\n");
#endif
            acc &= ~(NAC_FAST_PGM_RDIN | NAC_PARTIAL_PAGE_EN);
        }

        if(((acc & NAC_ECC_LVL_MASK)>>NAC_ECC_LVL_SHIFT)==NAC_ECC_LVL_HAMMING)
        {
#if 1//__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11 
            printk("Changing ECC to BCH-4 from Hamming for MLC part\n");
#else
            printf("Changing ECC to BCH-4 from Hamming for MLC part\n");
#endif
            acc &= ~(NAC_ECC_LVL_0_MASK | NAC_ECC_LVL_MASK);
            acc |= (NAC_ECC_LVL_BCH_4 << NAC_ECC_LVL_0_SHIFT) |
                 (NAC_ECC_LVL_BCH_4 << NAC_ECC_LVL_SHIFT);
        }
        if( acc != acc_save )
        {
            NAND->NandAccControl = acc;
#if 1//__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11 
            printk("NAND Access Control register = 0x%8.8lx\n", acc);
#else
            printf("NAND Access Control register = 0x%8.8lx\n", acc);
#endif
        }
    }

#if defined(CFG_RAMAPP)
    if( NAND->NandRevision <= 0x00000202 )
    {
        PSPARE_LAYOUT spare_layout = NULL;

        pchip->chip_spare_step_size = 16;
        pchip->chip_spare_size = pchip->chip_page_size >> 5;
        switch(pchip->chip_page_size)
        {
        case 2048:
            spare_layout = &brcmnand_oob_64;
            break;

        case 4096:
            spare_layout = &brcmnand_oob_128;
            break;

        /* case 512: */
        default:
            spare_layout = &brcmnand_oob_16;
            break;
        }

        pchip->chip_spare_mask = spare_layout->sl_spare_mask;
        pchip->chip_bi_index_1 = spare_layout->sl_bi_ofs[0];
        pchip->chip_bi_index_2 = spare_layout->sl_bi_ofs[1];
    }
    else
    {
        PSPARE_LAYOUT spare_layout = NULL;
        unsigned long acc = NAND->NandAccControl;
        unsigned long ecc_lvl = (acc & NAC_ECC_LVL_MASK) >> NAC_ECC_LVL_SHIFT;
        unsigned long i, layout_parms;

        /* The access control register spare size is the number of spare area
         * bytes per 512 bytes of data.  The chip_spare_size is the number
         * of spare area bytes per page.
         */
        pchip->chip_spare_step_size =
            ((acc & NAC_SPARE_SZ_MASK) >> NAC_SPARE_SZ_SHIFT);
        pchip->chip_spare_size = pchip->chip_spare_step_size *
            (pchip->chip_page_size >> 9);

        layout_parms =
            LAYOUT_PARMS(ecc_lvl, pchip->chip_spare_size, pchip->chip_page_size);
        DBG_PRINTF(">> nand_read_cfg - layout_parms=0x%8.8lx\n", layout_parms);
        switch(layout_parms)
        {
        case (LAYOUT_PARMS(NAC_ECC_LVL_HAMMING,16,512)):
            spare_layout = &brcmnand_oob_16;
            break;

        case (LAYOUT_PARMS(NAC_ECC_LVL_BCH_4,16,512)):
            spare_layout = &brcmnand_oob_bch4_512;
            break;

        case (LAYOUT_PARMS(NAC_ECC_LVL_BCH_4,64,2048)):
            spare_layout = &brcmnand_oob_bch4_2k;
            break;

        case (LAYOUT_PARMS(NAC_ECC_LVL_BCH_8,64,2048)):
            spare_layout = &brcmnand_oob_bch8_16_2k;
            break;

        case (LAYOUT_PARMS(NAC_ECC_LVL_HAMMING,64,2048)):
            spare_layout = &brcmnand_oob_64;
            break;

        case (LAYOUT_PARMS(NAC_ECC_LVL_BCH_4,128,4096)):
            spare_layout = &brcmnand_oob_bch4_4k;
            break;

        case (LAYOUT_PARMS(NAC_ECC_LVL_BCH_8,128,4096)):
            spare_layout = &brcmnand_oob_bch8_16_4k;
            break;

        case (LAYOUT_PARMS(NAC_ECC_LVL_BCH_8,216,4096)):
            spare_layout = &brcmnand_oob_bch8_27_4k;
            break;

        case (LAYOUT_PARMS(NAC_ECC_LVL_BCH_12,216,4096)):
            spare_layout = &brcmnand_oob_bch12_27_4k;
            break;

        case (LAYOUT_PARMS(NAC_ECC_LVL_HAMMING,128,4096)):
            spare_layout = &brcmnand_oob_128;
            break;

        case (LAYOUT_PARMS(NAC_ECC_LVL_BCH_4,256,8192)):
            spare_layout = &brcmnand_oob_bch4_8k;
            break;

        case (LAYOUT_PARMS(NAC_ECC_LVL_BCH_8,256,8192)):
            spare_layout = &brcmnand_oob_bch8_16_8k;
            break;

        case (LAYOUT_PARMS(NAC_ECC_LVL_BCH_8,432,8192)):
            spare_layout = &brcmnand_oob_bch8_27_8k;
            break;

        case (LAYOUT_PARMS(NAC_ECC_LVL_BCH_12,432,8192)):
            spare_layout = &brcmnand_oob_bch12_27_8k;
            break;

        default:
            if( ecc_lvl != NAC_ECC_LVL_DISABLE )
            {
#if 1//__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11 
                printk("No valid spare layout for level=%lu, spare size=%lu,"
                    " page size=%lu\n", ecc_lvl, pchip->chip_spare_size,
                    pchip->chip_page_size);
#else
                printf("No valid spare layout for level=%lu, spare size=%lu,"
                    " page size=%lu\n", ecc_lvl, pchip->chip_spare_size,
                    pchip->chip_page_size);
#endif
            }

            /* Change to a disabled spare layout. */
            spare_layout = &brcmnand_oob_bch12_27_8k;
            for( i = 0; i < pchip->chip_spare_size / 8; i++ )
                spare_layout->sl_spare_mask[i] = 0;
            break;
        }

        pchip->chip_spare_mask = spare_layout->sl_spare_mask;
        pchip->chip_bi_index_1 = spare_layout->sl_bi_ofs[0];
        pchip->chip_bi_index_2 = spare_layout->sl_bi_ofs[1];
    }

    DBG_PRINTF(">> nand_read_cfg - size=%luMB, block=%luKB, page=%luB, "
        "spare=%lu\n", pchip->chip_total_size / (1024 * 1024),
        pchip->chip_block_size / 1024, pchip->chip_page_size,
        pchip->chip_spare_size);
#endif
} /* nand_read_cfg */

#if !defined(CFG_RAMAPP)
/* This function always returns that a NAND flash block spare area contains a
 * JFFS2 clean marker for the CFE ROM build.  It does not do the validation in
 * order to save code and data space.  However, the CFE ROM boot code verifies
 * that the first two bytes of each NAND flash data block contain the JFFS2
 * magic number so there is NAND flash block validation.
 */
static int nand_is_blk_cleanmarker(PCFE_NAND_CHIP pchip,
    unsigned long start_addr, int write_if_not)
{
    /* Always return TRUE in CFE ROM. */
    return(1);
}
#else
/***************************************************************************
 * Function Name: nand_is_blk_cleanmarker
 * Description  : Compares a buffer to see if it a JFFS2 cleanmarker.
 * Returns      : 1 - is cleanmarker, 0 - is not cleanmarker
 ***************************************************************************/
static int nand_is_blk_cleanmarker(PCFE_NAND_CHIP pchip,
    unsigned long start_addr, int write_if_not)
{
    unsigned short cleanmarker[] = JFFS2_CLEANMARKER;
    unsigned char *pcm = (unsigned char *) cleanmarker;
    unsigned char spare[SPARE_MAX_SIZE], comparebuf[SPARE_MAX_SIZE];
    unsigned char *spare_mask = pchip->chip_spare_mask;
    unsigned long i, j;
    int ret = 0;

    if( nandflash_read_spare_area( pchip, start_addr, spare,
        pchip->chip_spare_size) == FLASH_API_OK )
    {
        /* Skip spare offsets that are reserved for the ECC.  Make spare data
         * bytes contiguous in the spare buffer.
         */
        for( i = 0, j = 0; i < pchip->chip_spare_size; i++ )
            if( ECC_MASK_BIT(spare_mask, i) == 0 )
                comparebuf[j++] = spare[i];

        /* Compare spare area data to the JFFS2 cleanmarker. */
        for( i = 0, ret = 1; i < sizeof(cleanmarker) && ret == 1; i++ )
            if( comparebuf[i] != pcm[i])
                ret = 0;
    }

#if 0 /* __MSTC__, zongyue: merge BRCM NAND bad block detect patch */
    if( ret == 0 && spare[pchip->chip_bi_index_1] != SPARE_BI_MARKER &&
        spare[pchip->chip_bi_index_2] != SPARE_BI_MARKER && write_if_not )
    {
        /* The spare area is not a clean marker but the block is not bad.
         * Write a clean marker to this block. (Assumes the block is erased.)
         */
        if( nandflash_write_spare_area(pchip, start_addr, (unsigned char *)
            g_spare_cleanmarker, pchip->chip_spare_size) == FLASH_API_OK )
        {
            ret = nand_is_blk_cleanmarker(pchip, start_addr, 0);
        }
    }
#else
    if( (spare[pchip->chip_bi_index_1] == SPARE_GOOD_MARKER) &&
        (spare[pchip->chip_bi_index_2] == SPARE_GOOD_MARKER)  )
    {
        if ((ret == 0) && write_if_not)
        {
            /* The spare area is not a clean marker but the block is not bad.
             * Write a clean marker to this block. (Assumes the block is erased.)
             */
            if( nandflash_write_spare_area(pchip, start_addr, (unsigned char *)
                g_spare_cleanmarker, pchip->chip_spare_size) == FLASH_API_OK )
            {
                ret = nand_is_blk_cleanmarker(pchip, start_addr, 0);
        	}
        }
    }
    else 
    {
        /* This is a bad block */
        ret = 0;   
    }
#endif

    return( ret );
} /* nand_is_blk_cleanmarker */

#if 1 /* __MSTC__, zongyue: merge BRCM NAND bad block detect patch */

/* Check if the block is good or bad. If bad returns 1, if good returns 0 */
static int nand_is_blk_bad(PCFE_NAND_CHIP pchip, unsigned long blockNum)
{
    unsigned char spare[SPARE_MAX_SIZE];
    UINT32 page_addr = (blockNum * pchip->chip_block_size) &  ~(pchip->chip_page_size - 1);
    int i;

    /* Read the spare area of first page and check for bad block indicator */
    for (i=0; i<2; i+=1, page_addr += pchip->chip_page_size)
    {
        if (nandflash_read_spare_area(pchip,  page_addr, spare, pchip->chip_spare_size) == FLASH_API_OK)
        {
            if ( (spare[pchip->chip_bi_index_1] != SPARE_GOOD_MARKER) || (spare[pchip->chip_bi_index_2] != SPARE_GOOD_MARKER))
            {
                return 1; // bad block
            }
        }
        else
        {
            return 1;//bad block
        }    
    }

    return 0; // good block
}

#endif

/***************************************************************************
 * Function Name: nand_initialize_spare_area
 * Description  : Initializes the spare area of the first page of each block 
 *                to a cleanmarker.
 * Returns      : FLASH_API_OK or FLASH_API_ERROR
 ***************************************************************************/
#if 0 /* __MSTC__, zongyue: merge BRCM NAND bad block detect patch */
static int nand_initialize_spare_area(PCFE_NAND_CHIP pchip)
#else
static int nand_initialize_spare_area(PCFE_NAND_CHIP pchip, int eraseBadBlocks)
#endif
{
    unsigned char spare[SPARE_MAX_SIZE];
    unsigned long i;
    int ret;

    DBG_PRINTF(">> nand_initialize_spare_area - entry\n");

    for( i = 0; i < pchip->chip_total_size; i += pchip->chip_block_size )
    {
        /* Read the current spare area. */
#if 1//__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11 
        ret = nandflash_read_spare_area(pchip,i,spare,pchip->chip_spare_size);
#else
        ret = nandflash_read_spare_area(pchip,0,spare,pchip->chip_spare_size);
#endif
        if(ret == FLASH_API_OK
          /*&& spare[pchip->chip_bi_index_1] != SPARE_BI_MARKER*/
          /*&& spare[pchip->chip_bi_index_2] != SPARE_BI_MARKER*/)
        {
#if 0 /* __MSTC__, zongyue: merge BRCM NAND bad block detect patch */
            if( nandflash_block_erase(pchip, i) == FLASH_API_OK )
#else
            /* if eraseBadBlocks is set, erase bad blocks, else check spare area to see if 
             * the block is good and then erase.
             */
            int writeSpare = eraseBadBlocks || 
                            ( (spare[pchip->chip_bi_index_1] == SPARE_GOOD_MARKER)
                              && (spare[pchip->chip_bi_index_2] == SPARE_GOOD_MARKER) );          
            if( writeSpare && (nandflash_block_erase(pchip, i) == FLASH_API_OK) )
#endif
            {
                nandflash_write_spare_area(pchip, i, (unsigned char *)
                    g_spare_cleanmarker, pchip->chip_spare_size);
            }
            else
            {
#if 1//__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11 
                printk("\nInitialization error erasing flash block, "
                    "blk=%d\n", i);
#else
                printf("\nInitialization error erasing flash block, "
                    "blk=%d\n", i);
#endif
            }
        }
    }

    return( FLASH_API_OK );
} /* nand_initialize_spare_area */

#if 1//__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11 
#ifdef _CFE_  
static int nand_initialize_spare_area_for_SMT(PCFE_NAND_CHIP pchip)
{
    unsigned char spare[SPARE_MAX_SIZE];
    unsigned long i;
    int ret;
#if 1 /* __MSTC__, zongyue: want to skip BBT to prevent destory BBT signature */
    unsigned long ulBbtSizeKb = (((pchip->chip_total_size/1024) > NAND_BBT_THRESHOLD_KB)
        ? NAND_BBT_BIG_SIZE_KB : NAND_BBT_SMALL_SIZE_KB) * 1024;
#endif

	DBG_PRINTF(">> nand_initialize_spare_area - SMT\n");

#if 1 /* __MSTC__, zongyue: want to skip BBT to prevent destory BBT signature */
	for( i = 0; i < (pchip->chip_total_size - ulBbtSizeKb); i += pchip->chip_block_size )
#else
    for( i = 0; i < pchip->chip_total_size; i += pchip->chip_block_size )
#endif
    {
        /* Read the current spare area. */
        ret = nandflash_read_spare_area(pchip,i,spare,pchip->chip_spare_size);
        if(ret == FLASH_API_OK
          && spare[pchip->chip_bi_index_1] != SPARE_BI_MARKER
          && spare[pchip->chip_bi_index_2] != SPARE_BI_MARKER
          && spare[pchip->chip_bi_index_1+1] == 0xFF
          && spare[pchip->chip_bi_index_2+2] == 0xFF)
        {
            if( nandflash_block_erase(pchip, i) == FLASH_API_OK )
            {
                nandflash_write_spare_area(pchip, i, (unsigned char *)
                    g_spare_cleanmarker, pchip->chip_spare_size);
            }
        }
    }

    return( FLASH_API_OK );
} /* nand_initialize_spare_area */
#endif
#endif

/***************************************************************************
 * Function Name: nand_mark_bad_blk
 * Description  : Marks the specified block as bad by writing 0xFFs to the
 *                spare area and updating the in memory bad block table.
 * Returns      : None.
 ***************************************************************************/
static void nand_mark_bad_blk(PCFE_NAND_CHIP pchip, unsigned long page_addr)
{
#if 0
    static int set_nandcmd = 0;
#endif
    static int marking_bad_blk = 0;

    unsigned char spare[SPARE_MAX_SIZE];

#if 1 /* __MSTC__, zongyue: merge BRCM NAND bad block detect patch */
    int blk = page_addr/pchip->chip_block_size;
    
    if (nand_is_blk_bad(pchip, blk))
    {
        printk("nand_mark_bad_blk() : Attempting to mark a bad block %d that is already bad.\n", blk);
        return ;
    }
#endif

    if( marking_bad_blk == 0 )
    {
        marking_bad_blk = 1;
        DBG_PRINTF(">> nand_mark_bad_blk - addr=0x%8.8lx, block=0x%8.8lx\n",
            page_addr, page_addr / pchip->chip_block_size);

        nandflash_block_erase(pchip, page_addr);
        memset(spare, 0xff, pchip->chip_spare_size);
        spare[pchip->chip_bi_index_1] = SPARE_BI_MARKER;
        spare[pchip->chip_bi_index_2] = SPARE_BI_MARKER;
        nandflash_write_spare_area(pchip,page_addr,spare,pchip->chip_spare_size);
#if 1 /* __MSTC__, zongyue: merge BRCM NAND bad block detect patch */
#if (INC_BTRM_BOOT==1) && (INC_BTRM_SECURITY==1)
        if( page_addr / pchip->chip_block_size > 2 )
#endif
        {
            /* Also mark the first page in the block as bad so the Linux NAND
             * flash driver finds it when updating the BBT.
             */
            page_addr &= ~(pchip->chip_block_size - 1);
            nandflash_write_spare_area(pchip, page_addr, spare,
                pchip->chip_spare_size);

		    /* Mark second page of the block as well */
			page_addr += pchip->chip_page_size;
            nandflash_write_spare_area(pchip, page_addr, spare,
                pchip->chip_spare_size);
        }
#endif		
        marking_bad_blk = 0;
    }
} /* nand_mark_bad_blk */


/***************************************************************************
 * Function Name: nand_flash_sector_erase_int
 * Description  : Erase the specfied flash sector.
 * Returns      : FLASH_API_OK or FLASH_API_ERROR
 ***************************************************************************/
static int nand_flash_sector_erase_int(unsigned short blk)
{
    int ret = FLASH_API_OK;
    PCFE_NAND_CHIP pchip = &g_chip;
	
#if defined(CONFIG_BRCMNAND_USE_HW_SEMAPHORE)
    nand_flash_set_semaphore();
#endif
#if 0 /* __MSTC__, zongyue: merge BRCM NAND bad block detect patch */
    if( blk == NAND_REINIT_FLASH )
        nand_initialize_spare_area(pchip);
#else
    if( ( blk == NAND_REINIT_FLASH ) || (blk == NAND_REINIT_FLASH_BAD ))
        nand_initialize_spare_area(pchip, (blk == NAND_REINIT_FLASH_BAD));
#endif
    else
    {
        unsigned long page_addr = blk * pchip->chip_block_size;

        if( (ret = nandflash_block_erase(pchip, page_addr)) == FLASH_API_OK )
        {
            ret = nandflash_write_spare_area(pchip, page_addr,
                g_spare_cleanmarker, pchip->chip_spare_size);

            if( ret == FLASH_API_ERROR )
#if 1//__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11 
                printk("\nError writing clean marker, blk=%d\n", blk);
#else
                printf("\nError writing clean marker, blk=%d\n", blk);
#endif
        }
        else
#if 1//__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11 
            printk("\nError erasing flash block, blk=%d\n", blk);
#else
            printf("\nError erasing flash block, blk=%d\n", blk);
#endif
    }

#if defined(CONFIG_BRCMNAND_USE_HW_SEMAPHORE)
    nand_flash_clr_semaphore();
#endif
    return( ret );
} /* nand_flash_sector_erase_int */
#endif

/***************************************************************************
 * Function Name: nand_flash_read_buf
 * Description  : Reads from flash memory.
 * Returns      : number of bytes read or FLASH_API_ERROR
 ***************************************************************************/
#if defined(CFG_RAMAPP)
static
#endif
int nand_flash_read_buf(unsigned short blk, int offset, unsigned char *buffer,
    int len)
{
    int ret = len;
    PCFE_NAND_CHIP pchip = &g_chip;
    UINT32 start_addr;
    UINT32 blk_addr;
    UINT32 blk_offset;
    UINT32 size;
#if 1 /* __MSTC__, zongyue: merge BRCM NAND bad block detect patch */
    UINT32 numBlocksInChip = pchip->chip_total_size/pchip->chip_block_size;
#endif

    DBG_PRINTF(">> nand_flash_read_buf - 1 blk=0x%8.8lx, offset=%d, len=%lu\n",
        blk, offset, len);

#if defined(CONFIG_BRCMNAND_USE_HW_SEMAPHORE)
    nand_flash_set_semaphore();
#endif

    start_addr = (blk * pchip->chip_block_size) + offset;
    blk_addr = start_addr & ~(pchip->chip_block_size - 1);
    blk_offset = start_addr - blk_addr;
    size = pchip->chip_block_size - blk_offset;

    if(size > len)
        size = len;

    do
    {
#if 1 /* __MSTC__, zongyue: merge BRCM NAND bad block detect patch */
        if (blk >= numBlocksInChip)
        {
            printk("Attempt to read block number(%d) beyond the nand max blk(%d) \n", blk, numBlocksInChip-1);
            return FLASH_API_ERROR;
        }
        
#if defined(CFG_RAMAPP)        
        if (nand_is_blk_bad(pchip, blk))
        {
            //printk("nand_flash_read_buf(): Attempt to read bad nand block %d\n", blk);
            ret = FLASH_API_ERROR;
            break;
        }
#endif
#endif
        if(nandflash_read_page(pchip,start_addr,buffer,size) != FLASH_API_OK)
        {
            ret = FLASH_API_ERROR;
            break;
        }

        len -= size;
        if( len )
        {
            blk++;

            DBG_PRINTF(">> nand_flash_read_buf - 2 blk=0x%8.8lx, len=%lu\n",
                blk, len);

            start_addr = blk * pchip->chip_block_size;
            buffer += size;
            if(len > pchip->chip_block_size)
                size = pchip->chip_block_size;
            else
                size = len;
        }
    } while(len);

#if defined(CONFIG_BRCMNAND_USE_HW_SEMAPHORE)
    nand_flash_clr_semaphore();
#endif

    DBG_PRINTF(">> nand_flash_read_buf - ret=%d\n", ret);

    return( ret ) ;
} /* nand_flash_read_buf */

#if defined(CFG_RAMAPP)
/***************************************************************************
 * Function Name: nand_flash_write_buf
 * Description  : Writes to flash memory.
 * Returns      : number of bytes written or FLASH_API_ERROR
 ***************************************************************************/
static int nand_flash_write_buf(unsigned short blk, int offset,
    unsigned char *buffer, int len)
{
    int ret = len;
    PCFE_NAND_CHIP pchip = &g_chip;
    UINT32 start_addr;
    UINT32 blk_addr;
    UINT32 blk_offset;
    UINT32 size;
#if 1 /* __MSTC__, zongyue: merge BRCM NAND bad block detect patch */
	UINT32 numBlocksInChip = pchip->chip_total_size/pchip->chip_block_size;
#endif

    DBG_PRINTF(">> nand_flash_write_buf - 1 blk=0x%8.8lx, offset=%d, len=%d\n",
        blk, offset, len);

#if defined(CONFIG_BRCMNAND_USE_HW_SEMAPHORE)
    nand_flash_set_semaphore();
#endif

    start_addr = (blk * pchip->chip_block_size) + offset;
    blk_addr = start_addr & ~(pchip->chip_block_size - 1);
    blk_offset = start_addr - blk_addr;
    size = pchip->chip_block_size - blk_offset;

    if(size > len)
        size = len;

    do
    {
#if 1 /* __MSTC__, zongyue: merge BRCM NAND bad block detect patch */
        if (blk >= numBlocksInChip)
        {
            printk("Attempt to write block number(%d) beyond the nand max blk(%d) \n", blk, numBlocksInChip-1);
            return ret-len;
        }

        if (nand_is_blk_bad(pchip, blk))
        {
            printk("nand_flash_write_buf(): Attempt to write bad nand block %d\n", blk);
            ret = ret - len;
            break;
        }
#endif
        if(nandflash_write_page(pchip,start_addr,buffer,size) != FLASH_API_OK)
        {
            ret = ret - len;
            break;
        }
        else
        {
            len -= size;
            if( len )
            {
                blk++;

                DBG_PRINTF(">> nand_flash_write_buf- 2 blk=0x%8.8lx, len=%d\n",
                    blk, len);

                offset = 0;
                start_addr = blk * pchip->chip_block_size;
                buffer += size;
                if(len > pchip->chip_block_size)
                    size = pchip->chip_block_size;
                else
                    size = len;
            }
        }
    } while(len);

#if defined(CONFIG_BRCMNAND_USE_HW_SEMAPHORE)
    nand_flash_clr_semaphore();
#endif

    DBG_PRINTF(">> nand_flash_write_buf - ret=%d\n", ret);

    return( ret ) ;
} /* nand_flash_write_buf */

/***************************************************************************
 * Function Name: nand_flash_get_memptr
 * Description  : Returns the base MIPS memory address for the specfied flash
 *                sector.
 * Returns      : Base MIPS memory address for the specfied flash sector.
 ***************************************************************************/
static unsigned char *nand_flash_get_memptr(unsigned short sector)
{
    /* Bad things will happen if this pointer is referenced.  But it can
     * be used for pointer arithmetic to deterine sizes.
     */
    return((unsigned char *) (FLASH_BASE + (sector * g_chip.chip_block_size)));
} /* nand_flash_get_memptr */

/***************************************************************************
 * Function Name: nand_flash_get_blk
 * Description  : Returns the flash sector for the specfied MIPS address.
 * Returns      : Flash sector for the specfied MIPS address.
 ***************************************************************************/
static int nand_flash_get_blk(int addr)
{
    return( (addr - FLASH_BASE) / g_chip.chip_block_size );
} /* nand_flash_get_blk */

/***************************************************************************
 * Function Name: nand_flash_get_total_size
 * Description  : Returns the number of bytes in the "CFE Linux code"
 *                partition.
 * Returns      : Number of bytes
 ***************************************************************************/
static int nand_flash_get_total_size(void)
{
    return(g_chip.chip_total_size);
} /* nand_flash_get_total_size */
#endif

/***************************************************************************
 * Function Name: nand_flash_get_sector_size
 * Description  : Returns the number of bytes in the specfied flash sector.
 * Returns      : Number of bytes in the specfied flash sector.
 ***************************************************************************/
#if defined(CFG_RAMAPP)
static
#endif
int nand_flash_get_sector_size(unsigned short sector)
{
    return(g_chip.chip_block_size);
} /* nand_flash_get_sector_size */

/***************************************************************************
 * Function Name: nand_flash_get_numsectors
 * Description  : Returns the number of blocks in the "CFE Linux code"
 *                partition.
 * Returns      : Number of blocks
 ***************************************************************************/
#if defined(CFG_RAMAPP)
static
#endif
int nand_flash_get_numsectors(void)
{
    return(g_chip.chip_total_size / g_chip.chip_block_size);
} /* nand_flash_get_numsectors */


/***************************************************************************
 * NAND Flash Implementation Functions
 ***************************************************************************/

/***************************************************************************
 * Function Name: nandflash_copy_from_cache
 * Description  : Copies data from the chip NAND cache to a local memory
 *                buffer.
 * Returns      : None.
 ***************************************************************************/
static inline void nandflash_copy_from_cache(unsigned char *buffer,
    int offset, int numbytes)
{
    unsigned long *cache = (unsigned long *) NAND_CACHE;
    unsigned long *pbuff = (unsigned long *)buffer;
    unsigned long i;

    numbytes /= sizeof(long);
    for(i=0; i< numbytes; ++i)
         pbuff[i] = cache[i];
} /* nandflash_copy_from_cache */

/***************************************************************************
 * Function Name: nandflash_copy_from_spare
 * Description  : Copies data from the chip NAND spare registers to a local
 *                memory buffer.
 * Returns      : None.
 ***************************************************************************/
static inline void nandflash_copy_from_spare(unsigned char *buffer,
    int numbytes)
{
    unsigned long *spare_area = (unsigned long *) &NAND->NandSpareAreaReadOfs0;
    unsigned long *pbuff = (unsigned long *)buffer;
    unsigned long i;

    /* TBD. Support 27 byte spare area. */
    if( numbytes > 16 )
        numbytes = 16;

    numbytes /= sizeof(long);
    for(i=0; i< numbytes; ++i)
         pbuff[i] = spare_area[i];
} /* nandflash_copy_from_spare */

#if defined(CFG_RAMAPP)
/***************************************************************************
 * Function Name: nandflash_copy_to_cache
 * Description  : Copies data from a local memory buffer to the the chip NAND
 *                cache.
 * Returns      : None.
 ***************************************************************************/
static inline void nandflash_copy_to_cache(unsigned char *buffer, int offset,
    int numbytes)
{
    unsigned char *cache = (unsigned char *) NAND_CACHE;
    unsigned long i;

    for( i = 0; i < numbytes; i += sizeof(long) )
        *(unsigned long *) &cache[i] =
            ((unsigned long) buffer[i + 0] << 24) |
            ((unsigned long) buffer[i + 1] << 16) |
            ((unsigned long) buffer[i + 2] <<  8) |
            ((unsigned long) buffer[i + 3] <<  0);
} /* nandflash_copy_to_cache */

/***************************************************************************
 * Function Name: nandflash_copy_to_spare
 * Description  : Copies data from a local memory buffer to the the chip NAND
 *                spare registers.
 * Returns      : None.
 ***************************************************************************/
static inline void nandflash_copy_to_spare(unsigned char *buffer,int numbytes)
{
    unsigned long *spare_area = (unsigned long *) &NAND->NandSpareAreaWriteOfs0;
    unsigned long *pbuff = (unsigned long *)buffer;
    int i;

    /* TBD. Support 27 byte spare area. */
    if( numbytes > 16 )
        numbytes = 16;

    for(i=0; i< numbytes / sizeof(unsigned long); ++i)
        spare_area[i] = pbuff[i];
} /* nandflash_copy_to_spare */
#endif

/***************************************************************************
 * Function Name: nandflash_wait_status
 * Description  : Polls the NAND status register waiting for a condition.
 * Returns      : FLASH_API_OK or FLASH_API_ERROR
 ***************************************************************************/
static int nandflash_wait_status(unsigned long status_mask)
{

    const unsigned long nand_poll_max = 2000000;
    unsigned long data;
    unsigned long poll_count = 0;
    int ret = FLASH_API_OK;

    do
    {
        data = NAND->NandIntfcStatus;
    } while(!(status_mask & data) && (++poll_count < nand_poll_max));

    if(!(status_mask & NAND->NandIntfcStatus))
    {
#if 1//__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11 
        printk("Status wait timeout: nandsts=0x%8.8lx mask=0x%8.8lx, count="
            "%lu\n", NAND->NandIntfcStatus, status_mask, poll_count);
#else
        printf("Status wait timeout: nandsts=0x%8.8lx mask=0x%8.8lx, count="
            "%lu\n", NAND->NandIntfcStatus, status_mask, poll_count);
#endif
        ret = FLASH_API_ERROR;
    }

    return( ret );
} /* nandflash_wait_status */

static inline int nandflash_wait_cmd(void)
{
    return nandflash_wait_status(NIS_CTLR_READY);
} /* nandflash_wait_cmd */

static inline int nandflash_wait_device(void)
{
    return nandflash_wait_status(NIS_FLASH_READY);
} /* nandflash_wait_device */

static inline int nandflash_wait_cache(void)
{
    return nandflash_wait_status(NIS_CACHE_VALID);
} /* nandflash_wait_cache */

static inline int nandflash_wait_spare(void)
{
    return nandflash_wait_status(NIS_SPARE_VALID);
} /* nandflash_wait_spare */

#if !defined(CFG_RAMAPP)
static int nandflash_check_ecc(void)
{
    return( FLASH_API_OK );
}
static int check_ecc_for_ffs(PCFE_NAND_CHIP pchip, UINT32 step)
{
    return( 1 );
}
#else
/***************************************************************************
 * Function Name: nandflash_check_ecc
 * Description  : Reads ECC status.
 * Returns      : FLASH_API_OK or FLASH_API_ERROR
 ***************************************************************************/
static int nandflash_check_ecc(void)
{
    int ret = FLASH_API_OK;
    UINT32 intrCtrl;
    UINT32 accessCtrl;

    /* read interrupt status */
    intrCtrl = NAND_INTR->NandInterrupt;
    accessCtrl = NAND->NandAccControl;

#if 1//__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11 
    if( (intrCtrl & NINT_ECC_ERROR_UNC) != 0 ) 
    {
		if ( NAND->NandEccUncAddr ) {
			NAND->NandEccUncAddr = 0;
		}
		if ( NAND->NandEccUncExtAddr ) {
			NAND->NandEccUncExtAddr = 0;
		}
        ret = FLASH_API_ERROR;
    }
#else
    if( (intrCtrl & NINT_ECC_ERROR_UNC) != 0 )
        ret = FLASH_API_ERROR;
#endif

    if( (intrCtrl & NINT_ECC_ERROR_CORR) != 0 )
    {
#if 1//__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11 
        printk("Correctable ECC Error detected: addr=0x%8.8lx, intrCtrl=0x"
            "%08X, accessCtrl=0x%08X\n", NAND->NandEccCorrAddr, (UINT)intrCtrl,
            (UINT)accessCtrl);
#else
        printf("Correctable ECC Error detected: addr=0x%8.8lx, intrCtrl=0x"
            "%08X, accessCtrl=0x%08X\n", NAND->NandEccCorrAddr, (UINT)intrCtrl,
            (UINT)accessCtrl);
#endif
#if 1//__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11 
		if ( NAND->NandEccCorrAddr ) {
			NAND->NandEccCorrAddr = 0;
		}
		if ( NAND->NandEccCorrExtAddr ) {
			NAND->NandEccCorrExtAddr = 0;
		}
#endif
    }

    return( ret );
}

/***************************************************************************
 * Function Name: check_ecc_for_ffs
 * Description  : Checks if the ECC bytes are all 0xFFs
 * Returns      : 1 if all ECC bytes are 0xff, 0 if not
 ***************************************************************************/
static int check_ecc_for_ffs(PCFE_NAND_CHIP pchip, UINT32 step)
{
    int ret = 1;
    UINT8 spare[32];
    UINT8 *spare_mask = pchip->chip_spare_mask;
    UINT32 i, j = step * pchip->chip_spare_step_size;

    nandflash_copy_from_spare(spare, pchip->chip_spare_step_size);
    for( i = 0; i < pchip->chip_spare_step_size; i++, j++ )
    {
        if( ECC_MASK_BIT(spare_mask, j) != 0 && spare[i] != 0xff )
        {
            ret = 0;
            break;
        }
    }

    return( ret );
}

/***************************************************************************
 * Function Name: nandflash_read_spare_area
 * Description  : Reads the spare area for the specified page.
 * Returns      : FLASH_API_OK or FLASH_API_ERROR
 ***************************************************************************/
static int nandflash_read_spare_area(PCFE_NAND_CHIP pchip,
    unsigned long page_addr, unsigned char *buffer, int len)
{
    int ret = FLASH_API_ERROR;

    if( len >= pchip->chip_spare_size )
    {
        UINT32 steps = pchip->chip_spare_size / pchip->chip_spare_step_size;
        UINT32 i;

        for( i = 0; i < steps; i++ )
        {
            NAND->NandCmdAddr = pchip->chip_base + page_addr +
                (i * CTRLR_CACHE_SIZE);
            NAND->NandCmdExtAddr = 0;
            NAND->NandCmdStart = NCMD_SPARE_READ;

            if( (ret = nandflash_wait_cmd()) == FLASH_API_OK )
            {
                /* wait until data is available in the spare area registers */
                if( (ret = nandflash_wait_spare()) == FLASH_API_OK )
                    nandflash_copy_from_spare(buffer +
                        (i * pchip->chip_spare_step_size),
                        pchip->chip_spare_step_size);
                else
                    break;
            }
            else
                break;
        }
    }

    return ret;
} /* nandflash_read_spare_area */

/***************************************************************************
 * Function Name: nandflash_write_spare_area
 * Description  : Reads the spare area for the specified page.
 * Returns      : FLASH_API_OK or FLASH_API_ERROR
 ***************************************************************************/
static int nandflash_write_spare_area(PCFE_NAND_CHIP pchip,
    unsigned long page_addr, unsigned char *buffer, int len)
{
    int ret = FLASH_API_OK;
    unsigned char nand_cache[CTRLR_CACHE_SIZE];
    unsigned char spare[SPARE_MAX_SIZE];
    unsigned long is_mlc = NAND_IS_MLC(pchip);

    if( len <= pchip->chip_spare_size )
    {
        unsigned long acc = NAND->NandAccControl;
        unsigned long acc_save = acc;
                        
        UINT32 steps = pchip->chip_spare_size / pchip->chip_spare_step_size;
        UINT32 i;

        if( is_mlc )
            memset(nand_cache, 0xff, sizeof(nand_cache));

        memset(spare, 0xff, pchip->chip_spare_size);
        memcpy(spare, buffer, len);
        for( i = 0; i < steps; i++ )
        {
            NAND->NandCmdAddr = pchip->chip_base + page_addr +
                (i * CTRLR_CACHE_SIZE);
            NAND->NandCmdExtAddr = 0;

            if( is_mlc )
            {
                /* Disable ECC so 0xFFs are stored in the ECC offsets. Doing
                 * this allows the next page write to store the ECC correctly.
                 * If the ECC is not disabled here, then a ECC value will be
                 * stored at the ECC offsets.  This will cause the ECC value
                 * on the next write to be stored incorrectly.
                 */
                acc &= ~(NAC_ECC_LVL_0_MASK | NAC_ECC_LVL_MASK);
                acc |= (NAC_ECC_LVL_DISABLE << NAC_ECC_LVL_0_SHIFT) |
                       (NAC_ECC_LVL_DISABLE << NAC_ECC_LVL_SHIFT);

                NAND->NandAccControl = acc;

                /* MLC parts use PROGRAM_PAGE to program spare area. */
                nandflash_copy_to_cache(nand_cache, 0, sizeof(nand_cache));
                nandflash_copy_to_spare(spare+(i*pchip->chip_spare_step_size),
                    pchip->chip_spare_step_size);
                NAND->NandCmdStart = NCMD_PROGRAM_PAGE;
            }
            else
            {
                /* SLC parts use PROGRAM_SPARE to program spare area. */
                nandflash_copy_to_spare(spare+(i*pchip->chip_spare_step_size),
                    pchip->chip_spare_step_size);
                NAND->NandCmdStart = NCMD_PROGRAM_SPARE;
            }

            if( (ret = nandflash_wait_cmd()) == FLASH_API_OK )
            {
                unsigned long sts = NAND->NandIntfcStatus;

                if( (sts & NIS_PGM_ERASE_ERROR) != 0 )
                {
#if 1//__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11 
                    printk("Error writing to spare area, sts=0x%8.8lx\n", sts);
#else
                    printf("Error writing to spare area, sts=0x%8.8lx\n", sts);
#endif
                    nand_mark_bad_blk(pchip, page_addr);
                    ret = FLASH_API_ERROR;
                }
            }

            if( is_mlc )
                NAND->NandAccControl = acc_save;
        }
    }
    else
        ret = FLASH_API_ERROR;
 
    return( ret );
} /* nandflash_write_spare_area */
#endif

/***************************************************************************
 * Function Name: nandflash_read_page
 * Description  : Reads up to a NAND block of pages into the specified buffer.
 * Returns      : FLASH_API_OK or FLASH_API_ERROR
 ***************************************************************************/
static int nandflash_read_page(PCFE_NAND_CHIP pchip, unsigned long start_addr,
    unsigned char *buffer, int len)
{
    int ret = FLASH_API_ERROR;

    if( len <= pchip->chip_block_size )
    {
        UINT32 page_addr = start_addr & ~(pchip->chip_page_size - 1);
        UINT32 page_offset = start_addr - page_addr;
        UINT32 size = pchip->chip_page_size - page_offset;
        UINT32 index = 0;
        UINT32 i;

        if(size > len)
            size = len;

        do
        {
            for( i = 0, ret = FLASH_API_OK; i < pchip->chip_page_size &&
                 ret == FLASH_API_OK; i += CTRLR_CACHE_SIZE)
            {
                /* clear interrupts, so we can check later for ECC errors */
                NAND_INTR->NandInterrupt = NINT_STS_MASK;

                /* send command */
                NAND->NandCmdAddr = pchip->chip_base + page_addr + i;
                NAND->NandCmdExtAddr = 0;
                NAND->NandCmdStart = NCMD_PAGE_READ;

                if( (ret = nandflash_wait_cmd()) == FLASH_API_OK )
                {
                    /* wait until data is available in the cache */
                    if( (ret = nandflash_wait_cache()) == FLASH_API_OK )
                    {
                        if((ret = nandflash_check_ecc())==FLASH_API_ERROR)
                        {
                            if(check_ecc_for_ffs(pchip,i/CTRLR_CACHE_SIZE))
                                ret = FLASH_API_OK;
                            else
                            {
#if defined(CFG_RAMAPP)
#if 1//__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11 
                                printk("Uncorrectable ECC Error: addr="
                                    "0x%8.8lx, blk=%d,\n", NAND->NandEccUncAddr,
                                    NAND->NandEccUncAddr/pchip->chip_block_size);
#else
                                printf("Uncorrectable ECC Error: addr="
                                    "0x%8.8lx, blk=%d,\n", NAND->NandEccUncAddr,
                                    NAND->NandEccUncAddr/pchip->chip_block_size);
#endif
#if 1
                                ret = FLASH_API_OK;
#else
                                nand_mark_bad_blk(pchip,
                                    start_addr & ~(pchip->chip_page_size-1));
#endif
#endif /* CFG_RAMAPP */
                            }
                        }
                    }

                    if( ret == FLASH_API_OK )
                    {
                        if( i < size )
                        {
                            UINT32 copy_size =
                                (i + CTRLR_CACHE_SIZE <= size)
                                ? CTRLR_CACHE_SIZE : size - i;

                            nandflash_copy_from_cache(&buffer[index + i],
                                page_offset, copy_size);
                        }
                    }
                    else
                    {
                        break;
                    }
                }
            }

            if(ret != FLASH_API_OK)
                break;

            page_offset = 0;
            page_addr += pchip->chip_page_size;
            index += size;
            len -= size;
            if(len > pchip->chip_page_size)
                size = pchip->chip_page_size;
            else
                size = len;
        } while(len);

        /* Verify that the spare area contains a JFFS2 cleanmarker. */
        if( !nand_is_blk_cleanmarker(pchip,
            start_addr & ~(pchip->chip_page_size - 1), 0) )
        {
            ret = FLASH_API_ERROR;
            DBG_PRINTF("nandflash_read_page: cleanmarker not found at 0x%8.8lx\n",
                page_addr);
        }
    }

    return( ret ) ;
} /* nandflash_read_page */

#if defined(CFG_RAMAPP)
/***************************************************************************
 * Function Name: nandflash_write_page
 * Description  : Writes up to a NAND block of pages from the specified buffer.
 * Returns      : FLASH_API_OK or FLASH_API_ERROR
 ***************************************************************************/
static int nandflash_write_page(PCFE_NAND_CHIP pchip, unsigned long start_addr,
    unsigned char *buffer, int len)
{
    int ret = FLASH_API_ERROR;

    if( len <= pchip->chip_block_size )
    {
        unsigned char xfer_buf[CTRLR_CACHE_SIZE];
        UINT32 page_addr = start_addr & ~(pchip->chip_page_size - 1);
        UINT32 page_offset = start_addr - page_addr;
        UINT32 size = pchip->chip_page_size - page_offset;
        UINT32 index = 0;

        /* Verify that the spare area contains a JFFS2 cleanmarker. */
        if( nand_is_blk_cleanmarker(pchip, page_addr, 1) )
        {
            UINT32 steps = pchip->chip_page_size / CTRLR_CACHE_SIZE;
            UINT32 i, xfer_ofs, xfer_size;

            if(size > len)
                size = len;

            do
            {
                /* Don't write page if data is all 0xFFs.  This allows the
                 * Linux NAND flash driver to successfully write the page.
                 */
                UINT32 *pdw = (UINT32 *) (buffer + index);
                ret = FLASH_API_OK_BLANK;
                for( i = 0; i < pchip->chip_page_size / sizeof(long); i++ )
                {
                    if( *pdw++ != 0xffffffff )
                    {
                        ret = FLASH_API_OK;
                        break;
                    }
                }

                /* If the page is all 0xFFs, ret will equal FLASH_API_OK_BLANK,
                 * Skip writing the page but continue to process the buffer.
                 */
                for( i = 0, xfer_ofs = 0, xfer_size = 0;
                     i < steps && ret==FLASH_API_OK; i++)
                {
                    memset(xfer_buf, 0xff, sizeof(xfer_buf));

                    /* Copy JFFS2 clean marker to spare registers for the first
                     * page in the block and 0xFFs for other pages.
                     */
                    if( (page_addr % pchip->chip_block_size) == 0 )
                    {
                        nandflash_copy_to_spare(g_spare_cleanmarker +
                            (i * pchip->chip_spare_step_size),
                            pchip->chip_spare_step_size);
                    }
                    else
                    {
                        nandflash_copy_to_spare(xfer_buf,
                            pchip->chip_spare_step_size);
                    }

                    if(size - xfer_ofs > CTRLR_CACHE_SIZE)
                        xfer_size = CTRLR_CACHE_SIZE;
                    else
                        xfer_size = size - xfer_ofs;

                    if( xfer_size )
                    {
                        memcpy(xfer_buf + page_offset, buffer + index + xfer_ofs,
                            xfer_size);
                    }

                    xfer_ofs += xfer_size;

                    NAND->NandCmdAddr = pchip->chip_base + page_addr +
                        (i * CTRLR_CACHE_SIZE);
                    NAND->NandCmdExtAddr = 0;

                    nandflash_copy_to_cache(xfer_buf, 0, CTRLR_CACHE_SIZE);

                    NAND->NandCmdStart = NCMD_PROGRAM_PAGE;
                    if( (ret = nandflash_wait_cmd()) == FLASH_API_OK )
                    {
                        unsigned long sts = NAND->NandIntfcStatus;

                        if( (sts & NIS_PGM_ERASE_ERROR) != 0 )
                        {
#if 1//__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11 
                            printk("Error writing to block, sts=0x%8.8lx\n", sts);
#else
                            printf("Error writing to block, sts=0x%8.8lx\n", sts);
#endif
                            nand_mark_bad_blk(pchip,
                                start_addr & ~(pchip->chip_page_size - 1));
                            ret = FLASH_API_ERROR;
                        }
                    }
                }

                if(ret == FLASH_API_ERROR)
                    break;

                if(ret == FLASH_API_OK_BLANK)
                    ret = FLASH_API_OK;

                page_offset = 0;
                page_addr += pchip->chip_page_size;
                index += size;
                len -= size;
                if(len > pchip->chip_page_size)
                    size = pchip->chip_page_size;
                else
                    size = len;
            } while(len);
        }
        else
            DBG_PRINTF("nandflash_write_page: cleanmarker not found at 0x%8.8lx\n",
                page_addr);
    }

    return( ret );
} /* nandflash_write_page */

/***************************************************************************
 * Function Name: nandflash_block_erase
 * Description  : Erases a block.
 * Returns      : FLASH_API_OK or FLASH_API_ERROR
 ***************************************************************************/
static int nandflash_block_erase(PCFE_NAND_CHIP pchip, unsigned long blk_addr )
{

    int ret = FLASH_API_OK;
#if 1 /* __MSTC__, zongyue: merge BRCM NAND bad block detect patch */
    int blk = blk_addr/pchip->chip_block_size;
       
    if (nand_is_blk_bad(pchip, blk))
    {
        printk("nandflash_block_erase(): Attempt to erase bad nand block %d\n", blk);
        return  FLASH_API_ERROR;            
    }
#endif

    /* send command */
    NAND->NandCmdAddr = pchip->chip_base + blk_addr;
    NAND->NandCmdExtAddr = 0;
    NAND->NandCmdStart = NCMD_BLOCK_ERASE;
    if( (ret = nandflash_wait_cmd()) == FLASH_API_OK )
    {
        unsigned long sts = NAND->NandIntfcStatus;

        if( (sts & NIS_PGM_ERASE_ERROR) != 0 )
        {
            DBG_PRINTF("\nError erasing block 0x%8.8lx, sts=0x%8.8lx\n", \
                blk_addr, sts);
            nand_mark_bad_blk(pchip, blk_addr);
            ret = FLASH_API_ERROR;
        }
    }

    DBG_PRINTF(">> nandflash_block_erase - addr=0x%8.8lx, ret=%d\n", blk_addr,
        ret);

    return( ret );
} /* nandflash_block_erase */

void dump_spare(void);
void dump_spare(void)
{
    PCFE_NAND_CHIP pchip = &g_chip;
    unsigned char spare[SPARE_MAX_SIZE];
    unsigned long i;

    for( i = 0; i < pchip->chip_total_size; i += pchip->chip_block_size )
    {
        if( nandflash_read_spare_area(pchip, i, spare,
            pchip->chip_spare_size) == FLASH_API_OK )
        {
#if 1//__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11 
            printk("%8.8lx: %8.8lx %8.8lx %8.8lx %8.8lx\n", i,
                *(unsigned long *) &spare[0], *(unsigned long *) &spare[4],
                *(unsigned long *) &spare[8], *(unsigned long *) &spare[12]);
#else
            printf("%8.8lx: %8.8lx %8.8lx %8.8lx %8.8lx\n", i,
                *(unsigned long *) &spare[0], *(unsigned long *) &spare[4],
                *(unsigned long *) &spare[8], *(unsigned long *) &spare[12]);
#endif
            if( pchip->chip_spare_size > 16 )
            {
#if 1//__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11 
                printk("%8.8lx: %8.8lx %8.8lx %8.8lx %8.8lx\n", i,
                    *(unsigned long *)&spare[16],*(unsigned long *)&spare[20],
                    *(unsigned long *)&spare[24],*(unsigned long *)&spare[28]);
                printk("%8.8lx: %8.8lx %8.8lx %8.8lx %8.8lx\n", i,
                    *(unsigned long *)&spare[32],*(unsigned long *)&spare[36],
                    *(unsigned long *)&spare[40],*(unsigned long *)&spare[44]);
                printk("%8.8lx: %8.8lx %8.8lx %8.8lx %8.8lx\n", i,
                    *(unsigned long *)&spare[48],*(unsigned long *)&spare[52],
                    *(unsigned long *)&spare[56],*(unsigned long *)&spare[60]);
#else
                printf("%8.8lx: %8.8lx %8.8lx %8.8lx %8.8lx\n", i,
                    *(unsigned long *)&spare[16],*(unsigned long *)&spare[20],
                    *(unsigned long *)&spare[24],*(unsigned long *)&spare[28]);
                printf("%8.8lx: %8.8lx %8.8lx %8.8lx %8.8lx\n", i,
                    *(unsigned long *)&spare[32],*(unsigned long *)&spare[36],
                    *(unsigned long *)&spare[40],*(unsigned long *)&spare[44]);
                printf("%8.8lx: %8.8lx %8.8lx %8.8lx %8.8lx\n", i,
                    *(unsigned long *)&spare[48],*(unsigned long *)&spare[52],
                    *(unsigned long *)&spare[56],*(unsigned long *)&spare[60]);
#endif
            }
        }
        else
#if 1//__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11 
            printk("Error reading spare 0x%8.8lx\n", i);
#else
            printf("Error reading spare 0x%8.8lx\n", i);
#endif
    }
}

int read_spare_data(int blk, unsigned char *buf, int bufsize);
int read_spare_data(int blk, unsigned char *buf, int bufsize)
{
    PCFE_NAND_CHIP pchip = &g_chip;
    unsigned char spare[SPARE_MAX_SIZE];
    unsigned long page_addr = blk * pchip->chip_block_size;
    unsigned long i, j;
    int ret;

    if( (ret = nandflash_read_spare_area( pchip, page_addr, spare,
        pchip->chip_spare_size)) == FLASH_API_OK )
    {
        if( (bufsize & 0x80000000) != 0 )
        {
            bufsize &= ~0x80000000;
            memset(buf, 0x00, bufsize);
            memcpy(buf, spare, (pchip->chip_spare_size > bufsize)
                ? bufsize : pchip->chip_spare_size);
        }
        else
        {
            unsigned char *spare_mask = pchip->chip_spare_mask;

            if( spare[pchip->chip_bi_index_1] == SPARE_BI_MARKER ||
                spare[pchip->chip_bi_index_2] == SPARE_BI_MARKER )
            {
                ret = FLASH_API_ERROR;
            }
            else
            {
                /* Skip spare offsets that are reserved for the ECC.  Make
                 * spare data bytes contiguous in the spare buffer.
             */
            for( i = 0, j = 0; i < pchip->chip_spare_size; i++ )
                if( ECC_MASK_BIT(spare_mask, i) == 0 && j < bufsize )
                    buf[j++] = spare[i];
        }
    }
    }

    return(ret);
}

int dump_spare_pages(int blk);
int dump_spare_pages(int blk)
{
    PCFE_NAND_CHIP pchip = &g_chip;
    unsigned long spare[SPARE_MAX_SIZE / 4];
    unsigned long page_addr = blk * pchip->chip_block_size;
    unsigned long i;
    int ret = 0;

    for( i = 0; i < 6; i++ )
    {
        if( (ret = nandflash_read_spare_area( pchip, page_addr +
            (i * pchip->chip_page_size), (unsigned char *) spare,
            pchip->chip_spare_size)) == FLASH_API_OK )
        {
#if 1//__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11 
            printk("%8.8lx %8.8lx %8.8lx %8.8lx\n", spare[0], spare[1],
                spare[2], spare[3]);
#else
            printf("%8.8lx %8.8lx %8.8lx %8.8lx\n", spare[0], spare[1],
                spare[2], spare[3]);
#endif
            if( pchip->chip_spare_size > 16 )
            {
#if 1//__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11 
                printk("%8.8lx %8.8lx %8.8lx %8.8lx\n", spare[4], spare[5],
                    spare[6], spare[7]);
                printk("%8.8lx %8.8lx %8.8lx %8.8lx\n", spare[8], spare[9],
                    spare[10], spare[11]);
                printk("%8.8lx %8.8lx %8.8lx %8.8lx\n\n", spare[12], spare[13],
                    spare[14], spare[15]);
#else
                printf("%8.8lx %8.8lx %8.8lx %8.8lx\n", spare[4], spare[5],
                    spare[6], spare[7]);
                printf("%8.8lx %8.8lx %8.8lx %8.8lx\n", spare[8], spare[9],
                    spare[10], spare[11]);
                printf("%8.8lx %8.8lx %8.8lx %8.8lx\n\n", spare[12], spare[13],
                    spare[14], spare[15]);
#endif
            }
        }
    }

    return(ret);
}
#endif

