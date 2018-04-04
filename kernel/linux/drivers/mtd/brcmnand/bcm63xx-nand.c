/*
 *
 *  drivers/mtd/brcmnand/bcm7xxx-nand.c
 *
<:copyright-BRCM:2002:GPL/GPL:standard

   Copyright (c) 2002 Broadcom Corporation
   All Rights Reserved

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

 * THIS DRIVER WAS PORTED FROM THE 2.6.18-7.2 KERNEL RELEASE
 */
 
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <asm/io.h>
#include <bcm_map_part.h>
#include <board.h>
#if 1//__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11
#include <flash_common.h>
#include <bcmTag.h>
#endif
#include "brcmnand_priv.h"

#define PRINTK(...)
//#define PRINTK printk

#define DRIVER_NAME     "brcmnand"
#define DRIVER_INFO     "Broadcom DSL NAND controller"

static int __devinit brcmnanddrv_probe(struct platform_device *pdev);
static int __devexit brcmnanddrv_remove(struct platform_device *pdev);

#if 1 /* Support the CLI command to test incorrect bad block tables(BBT), __CHT__, MitraStar SeanLu, 20140328 */
void dumpNandInformation (void);
void setNandBadBlockEntry (BRCMNAND_ACTION_t action, int Value);
extern int brcmnand_displayBBT(struct mtd_info* mtd);
extern void brcmnand_preprocessKernelArg(struct mtd_info *mtd);
#endif

static struct mtd_partition bcm63XX_nand_parts[] = 
{
#if 1 //__MSTC__, Paul Ho: Support 963268 nand flash, patch form SVN#3781 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11
    {name: "rootfs", offset: 0, size: 0},
    {name: "data",   offset: 0, size: 0},
    {name: "nvram",  offset: 0, size: 0},
    {name: NULL,     offset: 0, size: 0}
#else
    {name: "rootfs",        offset: 0, size: 0},
    {name: "rootfs_update", offset: 0, size: 0},
    {name: "data",          offset: 0, size: 0},
    {name: "nvram",         offset: 0, size: 0},
    {name: NULL,            offset: 0, size: 0}
#endif
};

#if 1//__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11
extern PFILE_TAG kerSysImageTagGet(void);
#endif

static struct platform_driver brcmnand_platform_driver =
{
    .probe      = brcmnanddrv_probe,
    .remove     = __devexit_p(brcmnanddrv_remove),
    .driver     =
     {
        .name   = DRIVER_NAME,
     },
};

static struct resource brcmnand_resources[] =
{
    [0] = {
            .name   = DRIVER_NAME,
            .start  = BPHYSADDR(BCHP_NAND_REG_START),
            .end    = BPHYSADDR(BCHP_NAND_REG_END) + 3,
            .flags  = IORESOURCE_MEM,
          },
};

struct brcmnand_info
{
    struct mtd_info mtd;
    struct brcmnand_chip brcmnand;
    int nr_parts;
    struct mtd_partition* parts;
} *gNandInfo[NUM_NAND_CS];

int gNandCS[NAND_MAX_CS];
/* Number of NAND chips, only applicable to v1.0+ NAND controller */
int gNumNand = 0;
int gClearBBT = 0;
char gClearCET = 0;
uint32_t gNandTiming1[NAND_MAX_CS], gNandTiming2[NAND_MAX_CS];
uint32_t gAccControl[NAND_MAX_CS], gNandConfig[NAND_MAX_CS];

#if 1 /* Support the CLI command to test incorrect bad block tables(BBT), __CHT__, MitraStar SeanLu, 20140328 */
int MinBadBlockSet = -1; /* max bad block index */
int MaxBadBlockSet = -1; /* min bad block index */
#endif

static unsigned long t1[NAND_MAX_CS] = {0};
static int nt1 = 0;
static unsigned long t2[NAND_MAX_CS] = {0};
static int nt2 = 0;
static unsigned long acc[NAND_MAX_CS] = {0};
static int nacc = 0;
static unsigned long nandcfg[NAND_MAX_CS] = {0};
static int ncfg = 0;
static void* gPageBuffer = NULL;

static void __devinit 
brcmnanddrv_setup_mtd_partitions(struct brcmnand_info* nandinfo)
{
#if 0//__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11
    int boot_from_nand = 1;

#if defined(CONFIG_BCM96368)
    if( ((GPIO->StrapBus & MISC_STRAP_BUS_BOOT_SEL_MASK) >>
        MISC_STRAP_BUS_BOOT_SEL_SHIFT) != MISC_STRAP_BUS_BOOT_NAND )
    {
        boot_from_nand = 0;
    }
#elif defined(CONFIG_BCM96362) || defined(CONFIG_BCM96328) || defined(CONFIG_BCM96816) || defined(CONFIG_BCM963268)
    if( ((MISC->miscStrapBus & MISC_STRAP_BUS_BOOT_SEL_MASK) >>
        MISC_STRAP_BUS_BOOT_SEL_SHIFT) != MISC_STRAP_BUS_BOOT_NAND )
    {
        boot_from_nand = 0;
    }
#endif

    if( boot_from_nand == 0 )
    {
        nandinfo->nr_parts = 1;
        nandinfo->parts = bcm63XX_nand_parts;

        bcm63XX_nand_parts[0].name = "data";
        bcm63XX_nand_parts[0].offset = 0;
        if( device_size(&(nandinfo->mtd)) < NAND_BBT_THRESHOLD_KB )
        {
            bcm63XX_nand_parts[0].size =
                device_size(&(nandinfo->mtd)) - (NAND_BBT_SMALL_SIZE_KB*1024);
        }
        else
        {
            bcm63XX_nand_parts[0].size =
                device_size(&(nandinfo->mtd)) - (NAND_BBT_BIG_SIZE_KB*1024);
        }
        bcm63XX_nand_parts[0].ecclayout = nandinfo->mtd.ecclayout;

        PRINTK("Part[0] name=%s, size=%llx, ofs=%llx\n", bcm63XX_nand_parts[0].name,
            bcm63XX_nand_parts[0].size, bcm63XX_nand_parts[0].offset);
    }
    else
    {
#endif
        static NVRAM_DATA nvram;
        struct mtd_info* mtd = &nandinfo->mtd;
#if 1 //__MSTC__, Paul Ho: Support 963268 nand flash, patch form SVN#3781 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11
        FLASH_ADDR_INFO finfo;
        unsigned long fsSize;
#endif
#if 1 //__MSTC__, Paul Ho: Support 963268 nand flash, patch form SVN#3781 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11
        PFILE_TAG pTag = (PFILE_TAG)NULL;
#endif
#if 1 //__MSTC__, Paul Ho: Support 963268 nand flash, patch form SVN#3781 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11
        kerSysFlashAddrInfoGet(&finfo);
        fsSize = finfo.flash_rootfs_max_size;
        printk("Root file system size %lx\n",fsSize);
#endif

#if 0//__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11
        unsigned long rootfs_ofs;
        int rootfs, rootfs_update;

        kerSysBlParmsGetInt(NAND_RFS_OFS_NAME, (int *) &rootfs_ofs);
#endif
        kerSysNvRamGet((char *)&nvram, sizeof(nvram), 0);

#if 1//__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11
        nandinfo->nr_parts = 3;
#else
        nandinfo->nr_parts = 4;
#endif
        nandinfo->parts = bcm63XX_nand_parts;

#if 1 //__MSTC__, Paul Ho: Support 963268 nand flash, patch form SVN#3781 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11
        /* Root FS. 
         * Support dual images and select the root fs that was was booted.
         * Data fill the runtime configuration of MTD RootFS Flash Device
         */
        if (! (pTag = kerSysImageTagGet()) ) {
            printk("Failed to read image tag from flash\n");
        }
        bcm63XX_nand_parts[0].offset = ((unsigned int)simple_strtoul(pTag->rootfsAddress, NULL, 10) + BOOT_OFFSET) - FLASH_BASE;
#if 1 //__MSTC__, Dennis
        bcm63XX_nand_parts[0].size = simple_strtoul(pTag->rootfsLen, NULL, 10) 
                                     + finfo.flash_rootfs_bad_block_number * TAG_BLOCK_LEN_NAND;
        printk(KERN_WARNING "bad block number = %d\n", finfo.flash_rootfs_bad_block_number);
#else
        if(bcm63XX_nand_parts[0].offset > fsSize)
#if 1 //__MSTC__, Dennis, merge from Elina
                bcm63XX_nand_parts[0].size = finfo.flash_persistent_blk_offset - bcm63XX_nand_parts[0].offset;
#else
                bcm63XX_nand_parts[0].size = nvram.ulNandPartOfsKb[NP_DATA] * 1024 - bcm63XX_nand_parts[0].offset;
#endif
        else
                bcm63XX_nand_parts[0].size = nvram.ulNandPartOfsKb[NP_ROOTFS_2] * 1024 - bcm63XX_nand_parts[0].offset;
#endif
        bcm63XX_nand_parts[0].ecclayout = mtd->ecclayout;

        /* Data (psi, scratch pad) */
        bcm63XX_nand_parts[1].offset = nvram.ulNandPartOfsKb[NP_DATA] * 1024;
        bcm63XX_nand_parts[1].size = nvram.ulNandPartSizeKb[NP_DATA] * 1024;
        bcm63XX_nand_parts[1].ecclayout = mtd->ecclayout;

        /* Boot and NVRAM data */
        bcm63XX_nand_parts[2].offset = nvram.ulNandPartOfsKb[NP_BOOT] * 1024;
        bcm63XX_nand_parts[2].size = nvram.ulNandPartSizeKb[NP_BOOT] * 1024;
        bcm63XX_nand_parts[2].ecclayout = mtd->ecclayout;

        PRINTK("Part[0] name=%s, size=%llx, ofs=%llx\n", bcm63XX_nand_parts[0].name,
                bcm63XX_nand_parts[0].size, bcm63XX_nand_parts[0].offset);
        PRINTK("Part[1] name=%s, size=%llx, ofs=%llx\n", bcm63XX_nand_parts[1].name,
                bcm63XX_nand_parts[1].size, bcm63XX_nand_parts[1].offset);
        PRINTK("Part[2] name=%s, size=%llx, ofs=%llx\n", bcm63XX_nand_parts[2].name,
                bcm63XX_nand_parts[2].size, bcm63XX_nand_parts[2].offset);
#else
        /* Root FS.  The CFE RAM boot loader saved the rootfs offset that the
         * Linux image was loaded from.
         */
        PRINTK("rootfs_ofs=0x%8.8lx, part1ofs=0x%8.8lx, part2ofs=0x%8.8lx\n",
            rootfs_ofs, nvram.ulNandPartOfsKb[NP_ROOTFS_1],
            nvram.ulNandPartOfsKb[NP_ROOTFS_2]);
        if( rootfs_ofs == nvram.ulNandPartOfsKb[NP_ROOTFS_1] )
        {
            rootfs = NP_ROOTFS_1;
            rootfs_update = NP_ROOTFS_2;
        }
        else
        {
            if( rootfs_ofs == nvram.ulNandPartOfsKb[NP_ROOTFS_2] )
            {
                rootfs = NP_ROOTFS_2;
                rootfs_update = NP_ROOTFS_1;
            }
            else
            {
                /* Backward compatibility with old cferam. */
                extern unsigned char _text;
                unsigned long rootfs_ofs = *(unsigned long *) (&_text - 4);

                if( rootfs_ofs == nvram.ulNandPartOfsKb[NP_ROOTFS_1] )
                {
                    rootfs = NP_ROOTFS_1;
                    rootfs_update = NP_ROOTFS_2;
                }
                else
                {
                    rootfs = NP_ROOTFS_2;
                    rootfs_update = NP_ROOTFS_1;
                }
            }
        }

        bcm63XX_nand_parts[0].offset = nvram.ulNandPartOfsKb[rootfs]*1024;
        bcm63XX_nand_parts[0].size = nvram.ulNandPartSizeKb[rootfs]*1024;
        bcm63XX_nand_parts[0].ecclayout = mtd->ecclayout;
        bcm63XX_nand_parts[1].offset = nvram.ulNandPartOfsKb[rootfs_update]*1024;
        bcm63XX_nand_parts[1].size = nvram.ulNandPartSizeKb[rootfs_update]*1024;
        bcm63XX_nand_parts[1].ecclayout = mtd->ecclayout;

        /* Data (psi, scratch pad) */
        bcm63XX_nand_parts[2].offset = nvram.ulNandPartOfsKb[NP_DATA] * 1024;
        bcm63XX_nand_parts[2].size = nvram.ulNandPartSizeKb[NP_DATA] * 1024;
        bcm63XX_nand_parts[2].ecclayout = mtd->ecclayout;

        /* Boot and NVRAM data */
        bcm63XX_nand_parts[3].offset = nvram.ulNandPartOfsKb[NP_BOOT] * 1024;
        bcm63XX_nand_parts[3].size = nvram.ulNandPartSizeKb[NP_BOOT] * 1024;
        bcm63XX_nand_parts[3].ecclayout = mtd->ecclayout;

        PRINTK("Part[0] name=%s, size=%llx, ofs=%llx\n", bcm63XX_nand_parts[0].name,
            bcm63XX_nand_parts[0].size, bcm63XX_nand_parts[0].offset);
        PRINTK("Part[1] name=%s, size=%llx, ofs=%llx\n", bcm63XX_nand_parts[1].name,
            bcm63XX_nand_parts[1].size, bcm63XX_nand_parts[1].offset);
        PRINTK("Part[2] name=%s, size=%llx, ofs=%llx\n", bcm63XX_nand_parts[2].name,
            bcm63XX_nand_parts[2].size, bcm63XX_nand_parts[2].offset);
        PRINTK("Part[3] name=%s, size=%llx, ofs=%llx\n", bcm63XX_nand_parts[3].name,
            bcm63XX_nand_parts[3].size, bcm63XX_nand_parts[3].offset);
    }
#endif
}


static int __devinit brcmnanddrv_probe(struct platform_device *pdev)
{
    static int csi = 0; // Index into dev/nandInfo array
    int cs = 0;  // Chip Select
    int err = 0;
    struct brcmnand_info* info = NULL;
    static struct brcmnand_ctrl* ctrl = (struct brcmnand_ctrl*) 0;

    if(!gPageBuffer &&
       (gPageBuffer = kmalloc(sizeof(struct nand_buffers),GFP_KERNEL)) == NULL)
    {
        err = -ENOMEM;
    }
    else
    {
        if( (ctrl = kmalloc(sizeof(struct brcmnand_ctrl), GFP_KERNEL)) != NULL)
        {
            memset(ctrl, 0, sizeof(struct brcmnand_ctrl));
            ctrl->state = FL_READY;
            init_waitqueue_head(&ctrl->wq);
            spin_lock_init(&ctrl->chip_lock);

            if((info=kmalloc(sizeof(struct brcmnand_info),GFP_KERNEL)) != NULL)
            {
                gNandInfo[csi] = info;
                memset(info, 0, sizeof(struct brcmnand_info));
                info->brcmnand.ctrl = ctrl;
                info->brcmnand.ctrl->numchips = gNumNand = 1;
                info->brcmnand.csi = csi;

                /* For now all devices share the same buffer */
                info->brcmnand.ctrl->buffers =
                    (struct nand_buffers*) gPageBuffer;

                info->brcmnand.ctrl->numchips = gNumNand; 
                info->brcmnand.chip_shift = 0; // Only 1 chip
                info->brcmnand.priv = &info->mtd;
                info->mtd.name = dev_name(&pdev->dev);
                info->mtd.priv = &info->brcmnand;
                info->mtd.owner = THIS_MODULE;

                /* Enable the following for a flash based bad block table */
                info->brcmnand.options |= NAND_USE_FLASH_BBT;

                /* Each chip now will have its own BBT (per mtd handle) */
                if (brcmnand_scan(&info->mtd, cs, gNumNand) == 0)
                {
                    PRINTK("Master size=%08llx\n", info->mtd.size); 
                    brcmnanddrv_setup_mtd_partitions(info);
                    add_mtd_partitions(&info->mtd, info->parts, info->nr_parts);
                    dev_set_drvdata(&pdev->dev, info);
                }
                else
                    err = -ENXIO;

            }
            else
                err = -ENOMEM;

        }
        else
            err = -ENOMEM;
    }

    if( err )
    {
        if( gPageBuffer )
        {
            kfree(gPageBuffer);
            gPageBuffer = NULL;
        }

        if( ctrl )
        {
            kfree(ctrl);
            ctrl = NULL;
        }

        if( info )
        {
            kfree(info);
            info = NULL;
        }
    }

    return( err );
}

static int __devexit brcmnanddrv_remove(struct platform_device *pdev)
{
    struct brcmnand_info *info = dev_get_drvdata(&pdev->dev);

    dev_set_drvdata(&pdev->dev, NULL);

    if (info)
    {
        del_mtd_partitions(&info->mtd);

        brcmnand_release(&info->mtd);
        kfree(gPageBuffer);
        kfree(info);
    }

    return 0;
}

static int __init brcmnanddrv_init(void)
{
    int ret = 0;
    int csi;
    int ncsi;
    char cmd[32] = "\0";
    struct platform_device *pdev;

    kerSysBlParmsGetStr("NANDCMD", cmd, sizeof(cmd));

    if (cmd[0])
    {
        if (strcmp(cmd, "rescan") == 0)
            gClearBBT = 1;
        else if (strcmp(cmd, "showbbt") == 0)
            gClearBBT = 2;
        else if (strcmp(cmd, "eraseall") == 0)
            gClearBBT = 8;
        else if (strcmp(cmd, "erase") == 0)
            gClearBBT = 7;
        else if (strcmp(cmd, "clearbbt") == 0)
            gClearBBT = 9;
        else if (strcmp(cmd, "showcet") == 0)
            gClearCET = 1;
        else if (strcmp(cmd, "resetcet") == 0)
            gClearCET = 2;
        else if (strcmp(cmd, "disablecet") == 0)
            gClearCET = 3;
        else
            printk(KERN_WARNING "%s: unknown command '%s'\n",
                __FUNCTION__, cmd);
    }
    
    for (csi=0; csi<NAND_MAX_CS; csi++)
    {
        gNandTiming1[csi] = 0;
        gNandTiming2[csi] = 0;
        gAccControl[csi] = 0;
        gNandConfig[csi] = 0;
    }

    if (nacc == 1)
        PRINTK("%s: nacc=%d, gAccControl[0]=%08lx, gNandConfig[0]=%08lx\n", \
            __FUNCTION__, nacc, acc[0], nandcfg[0]);

    if (nacc>1)
        PRINTK("%s: nacc=%d, gAccControl[1]=%08lx, gNandConfig[1]=%08lx\n", \
            __FUNCTION__, nacc, acc[1], nandcfg[1]);

    for (csi=0; csi<nacc; csi++)
        gAccControl[csi] = acc[csi];

    for (csi=0; csi<ncfg; csi++)
        gNandConfig[csi] = nandcfg[csi];

    ncsi = max(nt1, nt2);
    for (csi=0; csi<ncsi; csi++)
    {
        if (nt1 && csi < nt1)
            gNandTiming1[csi] = t1[csi];

        if (nt2 && csi < nt2)
            gNandTiming2[csi] = t2[csi];
        
    }

    printk (KERN_INFO DRIVER_INFO " (BrcmNand Controller)\n");
    if( (pdev = platform_device_alloc(DRIVER_NAME, 0)) != NULL )
    {
        platform_device_add(pdev);
        platform_device_put(pdev);
        ret = platform_driver_register(&brcmnand_platform_driver);
        if (ret >= 0)
            request_resource(&iomem_resource, &brcmnand_resources[0]);
        else
            printk("brcmnanddrv_init: driver_register failed, err=%d\n", ret);
    }
    else
        ret = -ENODEV;

    return ret;
}

static void __exit brcmnanddrv_exit(void)
{
    release_resource(&brcmnand_resources[0]);
    platform_driver_unregister(&brcmnand_platform_driver);
}

#if 1 /* Support the CLI command to test incorrect bad block tables(BBT), __CHT__, MitraStar SeanLu, 20140328 */

#define NANDCMD_RESCAN	1
#define NANDCMD_SHOWBBT	2

#define NANDCMD_ERASE		7
#define NANDCMD_ERASEALL	8
#define NANDCMD_CLEARBBT	9

void dumpNandInformation (void)
{
	struct brcmnand_info *info = gNandInfo[0];
	struct mtd_info *copy_mtd = &(info->mtd);
	FLASH_ADDR_INFO finfo;		
	kerSysFlashAddrInfoGet(&finfo);
	
	printk("Dump the brcmnand nand inforamtion\n\n");
	printk("----Image Information----\n");

	printk("0x%012llx-0x%012llx : \"%s\"\n", 
		(unsigned long long)bcm63XX_nand_parts[0].offset,
		(unsigned long long)(bcm63XX_nand_parts[0].offset + bcm63XX_nand_parts[0].size), 
		bcm63XX_nand_parts[0].name);

	printk("0x%012llx-0x%012llx : \"%s\"\n", 
		(unsigned long long)bcm63XX_nand_parts[1].offset,
		(unsigned long long)(bcm63XX_nand_parts[1].offset + bcm63XX_nand_parts[1].size), 
		bcm63XX_nand_parts[1].name);
	
	printk("0x%012llx-0x%012llx : \"%s\"\n", 
		(unsigned long long)bcm63XX_nand_parts[2].offset,
		(unsigned long long)(bcm63XX_nand_parts[2].offset + bcm63XX_nand_parts[2].size), 
		bcm63XX_nand_parts[2].name);
	
	printk("\n");
	
	printk("mount rootfs bad block number = %d\n\n", finfo.flash_rootfs_bad_block_number);
	brcmnand_displayBBT(copy_mtd);
	printk("\n");
	
	return;
}
void setNandBadBlockEntry (BRCMNAND_ACTION_t action, int Value)
{	
	struct brcmnand_info *info = gNandInfo[0];
	struct mtd_info *copy_mtd = &(info->mtd);
	struct brcmnand_chip *this = copy_mtd->priv;
	int res;

	switch(action){
		case BrcmBBTMinSave:
		{
			MinBadBlockSet = Value;
			break;
		}
		case BrcmBBTMaxSave:
		{
			MaxBadBlockSet = Value;
			break;
		}
		case BrcmBBTRun:
		{		
			/* Firstly clean BBT */
			gClearBBT = NANDCMD_CLEARBBT;
			if (gClearBBT) {
				(void) brcmnand_preprocessKernelArg(copy_mtd);
			}

			/* After we cleanup the BBT, we wust reset the value */
			kerSysFlashBadBlockNumSet(0);

			/* Before we re-scan bbt, we must to do clean the old bbt */
			if(this->bbt)
			{
				kfree(this->bbt);
				this->bbt = NULL;
			}
			res =  brcmnand_scan_bbt (copy_mtd, this->badblock_pattern);
			break;
		}
		default:
			break;
	}
	return;
}

#endif

module_init(brcmnanddrv_init);
module_exit(brcmnanddrv_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ton Truong <ttruong@broadcom.com>");
MODULE_DESCRIPTION("Broadcom NAND flash driver");

