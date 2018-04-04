/***********************************************************************
 *
 *  Copyright (c) 2007  Broadcom Corporation
 *  All Rights Reserved
 *
# 
# 
# This program is free software; you can redistribute it and/or modify 
# it under the terms of the GNU General Public License, version 2, as published by  
# the Free Software Foundation (the "GPL"). 
# 
#
# 
# This program is distributed in the hope that it will be useful,  
# but WITHOUT ANY WARRANTY; without even the implied warranty of  
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the  
# GNU General Public License for more details. 
#  
# 
#  
#   
# 
# A copy of the GPL is available at http://www.broadcom.com/licenses/GPLv2.php, or by 
# writing to the Free Software Foundation, Inc., 59 Temple Place - Suite 330, 
# Boston, MA 02111-1307, USA. 
#
 *
 ************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <net/if.h>

#include "cms.h"
#include "cms_util.h"
#include "cms_msg.h"
#include "cms_boardcmds.h"
#include "cms_boardioctl.h"

#include "bcmTag.h" /* in shared/opensource/include/bcm963xx, for FILE_TAG */
#include "board.h" /* in bcmdrivers/opensource/include/bcm963xx, for BCM_IMAGE_CFE */

static UBOOL8 matchChipId(const char *strTagChipId, const char *signature2);
#if 1 //__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11 
static CmsRet verifyBroadcomFileTag(PFILE_TAG pTag);
#else
CmsRet verifyBroadcomFileTag(FILE_TAG *pTag, int imageLen);
#endif
#ifdef MSTC_FWUP_FROM_FILE //__MSTC__, LingChun, merge MSTC_FWUP_FROM_FLASH from telefonica, http://svn.zyxel.com.tw/svn/CPE_TRUNK/BRCM_412/Telefonica_Common/
static CmsRet verifyBroadcomFileTagFlash(const FILE *imageFileFp);
static CmsRet flashImageFlash(const FILE *imageFileFp, UINT32 imageLen, CmsImageFormat format, char* filePath);
#endif
static CmsRet flashImage(char *imagePtr, UINT32 imageLen, CmsImageFormat format);
static CmsRet sendConfigMsg(const char *imagePtr, UINT32 imageLen, void *msgHandle, CmsMsgType msgType);
#ifdef SUPPORT_MOD_SW_UPDATE
static void sendStartModupdtdMsg(void *msgHandle);
#endif
#if 1 //__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11 
extern int sysGetNVRAMFromFlash(PNVRAM_DATA nvramData);
#endif

#ifdef MSTC_SAVE_LOG_TO_FLASH //__MSTC__, TengChang, for save log to flash
static void sendRequestRebootMsgNoSaveLog(void *msgHandle);
static void saveLog(void *msgHandle);
#endif //__MSTC__, TengChang, for save log to flash

#ifdef SUPPORT_ROMFILE_CONTROL
static void sendSaveCurFWVerMsg(void *msgHandle);
#endif

#ifdef BUILD_NORWAY_CUSTOMIZATION
static UBOOL8 needReboot = TRUE;
static char slotSelect = '0';
#endif

#ifdef TR69_AUTOREDOWNLOADFW //ZyXEL, Nick Lu, after plug out the power line when FW upgrade(TR069 download method), CPE will re-download FW auto
extern int sysGetTAGFromFlash( PFILE_TAG tagData );
#endif

/**
 * @return TRUE if the modem's chip id matches that of the image.
 */
UBOOL8 matchChipId(const char *strTagChipId, const char *signature2 __attribute__((unused)))
{
    UINT32 tagChipId = 0;
    UINT32 chipId; 
    UBOOL8 match;

    /* this is the image's chip id */
    tagChipId = (UINT32) strtol(strTagChipId, NULL, 16);
    
    /* get the system's chip id */
    devCtl_getChipId(&chipId);

    if (tagChipId == chipId)
    {
        match = TRUE;
    }
    else
    {
        cmsLog_error("Chip Id error.  Image Chip Id = %04x, Board Chip Id = %04x.", tagChipId, chipId);
        match = FALSE;
    }

    return match;
}

#if 1 //__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11 
// verify the tag of the image
CmsRet verifyModelId_MSTC(FILE_TAG *pTag)
{
#if 1//__COMMON__, jchang for model id check
    NVRAM_DATA nvramData;
    char ModelIdPrefix[3]={0};  /* Should be "ZY" Prefix */
    char ModelIdMRD[5]={0};  /* Should be 4 digits */
    char ModelIdTAGIdentity[6]={0};  /* Should be ZyXEL */	
    char ModelIdTAG[5]={0};  /* Should be 4 digits */
    char ModelIdCustomTAG[5]={0};  /* Should be 4 digits */

/* 
Here, we only check files with BCM TAG in the begining i.e. fs_kernel and cfe_fs_kernel.
For w image, we skip the model id check so user can still upgrade bootloader with new model id by w image.
*/
    if (!sysGetNVRAMFromFlash( &nvramData )){
        strncpy(ModelIdPrefix, nvramData.FeatureBits, 2);
        sprintf(ModelIdMRD, "%02x%02x",*((char*)(nvramData.FeatureBits+2)),*((char*)(nvramData.FeatureBits+3)));
        strncpy(ModelIdTAGIdentity, pTag->signiture_1, 4);
        strncpy(ModelIdTAG, pTag->signiture_1+5, 4);
        ModelIdPrefix[2] = '\0';
        ModelIdMRD[4] = '\0';	  
        ModelIdTAGIdentity[4] = '\0';	
        ModelIdTAG[4] = '\0';	   
        /* For Custom Tag, suppose everyone follow the naming rule ZyXEL_xxxx_yyyy */
        memcpy(ModelIdCustomTAG, pTag->signiture_1+10, 4);
        ModelIdCustomTAG[4] = '\0';

        cmsLog_debug(" ModelIdPrefix = %s", ModelIdPrefix);
        cmsLog_debug(" ModelIdMRD = %s",ModelIdMRD);   
        cmsLog_debug(" ModelIdTAGIdentity = %s", ModelIdTAGIdentity);
        cmsLog_debug(" ModelIdTAG = %s",ModelIdTAG);   
        cmsLog_debug(" ModelIdCustomTAG = %s",(0 < strlen(ModelIdCustomTAG))?ModelIdCustomTAG:"N/A");

        if(!strcmp(ModelIdPrefix,"MS") && !strcmp(ModelIdTAGIdentity,"MSTC")){
            cmsLog_debug("Finds ZyXEL identity, start checking model ID");
            if(strcmp(ModelIdMRD,ModelIdTAG) && strcmp(ModelIdMRD,ModelIdCustomTAG)){
                cmsLog_error("Check model from TAG is different with MRD");
                return CMSRET_INVALID_MODELID;	   	 
            }else{
                cmsLog_debug("Check modelID successfully");		
            }
        }else{
#if 1 /* If can't find the MSTC FW ID, should reject the FW */
           cmsLog_error("Not find MSTC identify");
           return CMSRET_INVALID_MODELID;		 
#else
           cmsLog_error("Not find ZyXEL identify, just fall through");
#endif
        }
    }else{
       cmsLog_error("Get MRD information fails, just fall through");
    }
#endif

    return CMSRET_SUCCESS;
}

// verify the tag of the image
CmsRet verifyBroadcomFileTag(FILE_TAG *pTag)
{
    UINT32 crc;
    int totalImageSize;
    int tagVer, curVer;
    UINT32 tokenCrc, imageCrc;

        
    cmsLog_debug("start of pTag=%p tagversion %02x %02x %02x %02x", pTag,
                  pTag->tagVersion[0], 
                  pTag->tagVersion[1], 
                  pTag->tagVersion[2], 
                  pTag->tagVersion[3]);

    tokenCrc = *((UINT32 *)pTag->tagValidationToken);
    imageCrc = *((UINT32 *)pTag->imageValidationToken);                  
#ifdef DESKTOP_LINUX
    /* assume desktop is running on little endien intel, but the CRC has been
     * written for big endien mips, so swap before compare.
     */
    tokenCrc = htonl(tokenCrc);
    imageCrc = htonl(imageCrc);
#endif
                  
                  
    // check tag validate token first
    crc = CRC_INITIAL_VALUE;
    crc = cmsCrc_getCrc32((UINT8 *) pTag, (UINT32)TAG_LEN-TOKEN_LEN, crc);
    if (crc != tokenCrc)
    {
        /* this function is called even when we are not sure the image is
         * a broadcom image.  So if crc fails, it is not a big deal.  It just
         * means this is not a broadcom image.
         */
        cmsLog_debug("token crc failed, this is not a valid broadcom image");
        cmsLog_debug("calculated crc=0x%x tokenCrc=0x%x", crc, tokenCrc);
        return CMSRET_INVALID_IMAGE;
    }
    cmsLog_debug("header CRC is OK.");

    
    // check imageCrc
    totalImageSize = atoi((char *) pTag->totalImageLen);
    cmsLog_debug("totalImageLen=%d", totalImageSize);

    crc = CRC_INITIAL_VALUE;
#if 1 // __MSTC__, zongyue
    crc = cmsCrc_getCrc32(((UINT8 *)pTag + TAG_BLOCK_LEN), (UINT32) totalImageSize, crc);
#else
    crc = cmsCrc_getCrc32(((UINT8 *)pTag + TAG_LEN), (UINT32) totalImageSize, crc);
#endif
    if (crc != imageCrc)
    {
        /*
         * This should not happen.  We already passed the crc check on the header,
         * so we should pass the crc check on the image.  If this fails, something
         * is wrong.
         */
        cmsLog_error("image crc failed after broadcom header crc succeeded");
        cmsLog_error("calculated crc=0x%x imageCrc=0x%x totalImageSize=%d", crc, imageCrc, totalImageSize); 
        return CMSRET_INVALID_IMAGE;
    }
    cmsLog_debug("image crc is OK");


    tagVer = atoi((char *) pTag->tagVersion);
    curVer = atoi(BCM_TAG_VER);

    if (tagVer != curVer)

    {
       cmsLog_error("Firmware tag version [%d] is not compatible with the current Tag version [%d]", tagVer, curVer);
       return CMSRET_INVALID_IMAGE;
    }
    cmsLog_debug("tarVer=%d, curVar=%d", tagVer, curVer);

    if (!matchChipId((char *) pTag->chipId, (char *) pTag->signiture_2))
    {
       cmsLog_error("chipid check failed");
       return CMSRET_INVALID_IMAGE;
    }

    cmsLog_debug("Good broadcom image");

    return CMSRET_SUCCESS;
}

#else
// verify the tag of the image
CmsRet verifyBroadcomFileTag(FILE_TAG *pTag, int imageLen)
{
    UINT32 crc;
    int totalImageSize;
    int tagVer, curVer;
    UINT32 tokenCrc, imageCrc;

        
    cmsLog_debug("start of pTag=%p tagversion %02x %02x %02x %02x", pTag,
                  pTag->tagVersion[0], 
                  pTag->tagVersion[1], 
                  pTag->tagVersion[2], 
                  pTag->tagVersion[3]);

    tokenCrc = *((UINT32 *)pTag->tagValidationToken);
    imageCrc = *((UINT32 *)pTag->imageValidationToken);                  
#ifdef DESKTOP_LINUX
    /* assume desktop is running on little endien intel, but the CRC has been
     * written for big endien mips, so swap before compare.
     */
    tokenCrc = htonl(tokenCrc);
    imageCrc = htonl(imageCrc);
#endif
                  
                  
    // check tag validate token first
    crc = CRC_INITIAL_VALUE;
    crc = cmsCrc_getCrc32((UINT8 *) pTag, (UINT32)TAG_LEN-TOKEN_LEN, crc);
    if (crc != tokenCrc)
    {
        /* this function is called even when we are not sure the image is
         * a broadcom image.  So if crc fails, it is not a big deal.  It just
         * means this is not a broadcom image.
         */
        cmsLog_debug("token crc failed, this is not a valid broadcom image");
        cmsLog_debug("calculated crc=0x%x tokenCrc=0x%x", crc, tokenCrc);
        return CMSRET_INVALID_IMAGE;
    }
    cmsLog_debug("header CRC is OK.");

    
    // check imageCrc
    totalImageSize = atoi((char *) pTag->totalImageLen);
    cmsLog_debug("totalImageLen=%d, imageLen=%d, TAG_LEN=%d\n", totalImageSize, imageLen, TAG_LEN);

    if (totalImageSize > (imageLen -TAG_LEN)) {
	 cmsLog_error("invalid size\n");
        return CMSRET_INVALID_IMAGE;
	}
    crc = CRC_INITIAL_VALUE;
    crc = cmsCrc_getCrc32(((UINT8 *)pTag + TAG_LEN), (UINT32) totalImageSize, crc);      
    if (crc != imageCrc)
    {
        /*
         * This should not happen.  We already passed the crc check on the header,
         * so we should pass the crc check on the image.  If this fails, something
         * is wrong.
         */
        cmsLog_error("image crc failed after broadcom header crc succeeded");
        cmsLog_error("calculated crc=0x%x imageCrc=0x%x totalImageSize", crc, imageCrc, totalImageSize); 
        return CMSRET_INVALID_IMAGE;
    }
    cmsLog_debug("image crc is OK");


    tagVer = atoi((char *) pTag->tagVersion);
    curVer = atoi(BCM_TAG_VER);

    if (tagVer != curVer)

    {
       cmsLog_error("Firmware tag version [%d] is not compatible with the current Tag version [%d]", tagVer, curVer);
       return CMSRET_INVALID_IMAGE;
    }
    cmsLog_debug("tarVer=%d, curVar=%d", tagVer, curVer);

    if (!matchChipId((char *) pTag->chipId, (char *) pTag->signiture_2))
    {
       cmsLog_error("chipid check failed");
       return CMSRET_INVALID_IMAGE;
    }

    cmsLog_debug("Good broadcom image");

    return CMSRET_SUCCESS;
}
#endif

#ifdef MSTC_FWUP_FROM_FILE //__MSTC__, LingChun, merge MSTC_FWUP_FROM_FLASH from telefonica, http://svn.zyxel.com.tw/svn/CPE_TRUNK/BRCM_412/Telefonica_Common/
static CmsRet verifyBroadcomFileTagFlash(const FILE *imageFileFp)
{
	UINT32 crc;
	int totalImageSize;
	int tagVer, curVer;
	UINT32 tokenCrc, imageCrc;
	UINT32 tagLen = 0;
	FILE_TAG *pTag = NULL;
	UINT8 c = 0;
	UINT32 i;

	if(imageFileFp == NULL) {
		cmsLog_error("File point is NULL.");
		return CMS_IMAGE_FORMAT_INVALID;
	}

	tagLen = sizeof(FILE_TAG);
	pTag = cmsMem_alloc(tagLen, 0);
	if(pTag == NULL) {
		cmsLog_error("Failed to allocate memory for the tag buffer. Size required %d", tagLen);
		return CMSRET_RESOURCE_EXCEEDED;
	}
	memset(pTag, 0, tagLen);
	fseek(imageFileFp, 0, SEEK_SET);
	fread(pTag, 1, tagLen, imageFileFp);

	cmsLog_debug("start of pTag=%p tagversion %02x %02x %02x %02x", pTag,
		pTag->tagVersion[0], 
		pTag->tagVersion[1], 
		pTag->tagVersion[2], 
		pTag->tagVersion[3]);

	tokenCrc = *((UINT32 *)pTag->tagValidationToken);
	imageCrc = *((UINT32 *)pTag->imageValidationToken);            
#ifdef DESKTOP_LINUX
	/* assume desktop is running on little endien intel, but the CRC has been
	* written for big endien mips, so swap before compare.
	*/
	tokenCrc = htonl(tokenCrc);
	imageCrc = htonl(imageCrc);
#endif

	// check tag validate token first
	crc = CRC_INITIAL_VALUE;
	crc = cmsCrc_getCrc32((UINT8 *)pTag, (UINT32)TAG_LEN-TOKEN_LEN, crc);
	if(crc != tokenCrc) {
		/* this function is called even when we are not sure the image is
		* a broadcom image.  So if crc fails, it is not a big deal.  It just
		* means this is not a broadcom image.
		*/
		cmsLog_debug("token crc failed, this is not a valid broadcom image");
		cmsLog_debug("calculated crc=0x%x tokenCrc=0x%x", crc, tokenCrc);
		return CMSRET_INVALID_IMAGE;
	}
	cmsLog_debug("header CRC is OK.");

	#if 1 /* Need to enhance CRC check time */
	// check imageCrc
	totalImageSize = atoi((char *) pTag->totalImageLen);
	cmsLog_debug("totalImageLen=%d", totalImageSize);

	fseek(imageFileFp, TAG_BLOCK_LEN, SEEK_SET);
	crc = CRC_INITIAL_VALUE;
	i = totalImageSize;
	while(i-- > 0) {
		c = (UINT8)fgetc(imageFileFp);
		crc = cmsCrc_getCrc32(&c, 1, crc);
	}

	if (crc != imageCrc) {
		/*
		* This should not happen.  We already passed the crc check on the header,
		* so we should pass the crc check on the image.  If this fails, something
		* is wrong.
		*/
		cmsLog_error("image crc failed after broadcom header crc succeeded");
		cmsLog_error("calculated crc=0x%x imageCrc=0x%x totalImageSize=%d", crc, imageCrc, totalImageSize); 
		return CMSRET_INVALID_IMAGE;
	}
	cmsLog_debug("image crc is OK");
	#endif

	tagVer = atoi((char *) pTag->tagVersion);
	curVer = atoi(BCM_TAG_VER);

	if (tagVer != curVer) {
		cmsLog_error("Firmware tag version [%d] is not compatible with the current Tag version [%d]", tagVer, curVer);
		return CMSRET_INVALID_IMAGE;
	}
	cmsLog_debug("tarVer=%d, curVar=%d", tagVer, curVer);

	if (!matchChipId((char *) pTag->chipId, (char *) pTag->signiture_2)) {
		cmsLog_error("chipid check failed");
		return CMSRET_INVALID_IMAGE;
	}

	fseek(imageFileFp, 0, SEEK_SET);
	cmsLog_debug("Good broadcom image");

    return CMSRET_SUCCESS;
}

static CmsRet flashImageFlash(const FILE *imageFileFp, UINT32 imageLen, CmsImageFormat format, char* filepath)
{
	FILE_TAG Tag, *pTag = NULL;
	int cfeSize, rootfsSize, kernelSize;
	unsigned long cfeAddr, rootfsAddr, kernelAddr;
	CmsRet ret=CMSRET_SUCCESS;
#if defined(MSTC_OBM_IMAGE_DEFAULT) || defined(MSTC_ROM_D)  /* Image Default, __CHT__, MitraStar Curtis, 20111124. */
	PIMAGE_TAG pTag_Img = NULL;
	unsigned long imageSize = 0;
	unsigned long search_tag_addr = 0;
#endif
	unsigned char *buf = NULL;

#if 1
	fwup_flash_parm_t fwup_flash_parm;
#endif

	if(imageFileFp == NULL || filepath==NULL) {
		cmsLog_error("File pointer is NULL.");
		return CMS_IMAGE_FORMAT_INVALID;
	}

	pTag = &Tag;
	memset(pTag, 0, sizeof(FILE_TAG));
	fseek(imageFileFp, 0, SEEK_SET);
	fread(pTag, 1, sizeof(FILE_TAG), imageFileFp);

#if 0//ZyXEL, ShuYing, not support CMS_IMAGE_FORMAT_FLASH (w file) via ZyXEL GUI
	cmsLog_notice("__(%s@%d)__ format=%d, filepath=%s, imageLen=%d ",__FUNCTION__,__LINE__,format,filepath,imageLen);
	if(format != CMS_IMAGE_FORMAT_FLASH && format != CMS_IMAGE_FORMAT_BROADCOM){
#else
	if (format != CMS_IMAGE_FORMAT_BROADCOM) {
#endif		
		cmsLog_error("invalid image format %d", format);
		return CMSRET_INVALID_IMAGE;
	}

#if 1//__MSTC__, LingChun
	if (format == CMS_IMAGE_FORMAT_FLASH)  
	{
		fprintf(stderr,"Flash whole image...\n");
		fflush(stderr);
		// Pass zero for the base address of whole image flash. It will be filled by kernel code
		// was  flashWholeImageFile / kerSysBcmImageFileSet
		memset(&fwup_flash_parm, 0, sizeof(fwup_flash_parm_t));
		sprintf(fwup_flash_parm.filename, "%s", filepath);
		fwup_flash_parm.size = imageLen-TOKEN_LEN;
		fwup_flash_parm.offset = 0; //file offset should be zero!!
 		//ioctl offset should be zero !! because this is .w
		ret = devCtl_boardIoctl(BOARD_IOCTL_FLASH_WRITE, BCM_IMAGE_WHOLE, NULL, 0, 0, &fwup_flash_parm);

		if (ret != CMSRET_SUCCESS)
		{
		   cmsLog_error("Failed to flash whole image");
		   return CMSRET_IMAGE_FLASH_FAILED;
		}
		else
		{
		   return CMSRET_SUCCESS;
		}
	}
#endif

	/* this must be a broadcom format image */
	// check imageCrc
	cfeSize = rootfsSize = kernelSize = 0;

	// check cfe's existence
	cfeAddr = (unsigned long) strtoul((char *) pTag->cfeAddress, NULL, 10);
	cfeSize = atoi((char *) pTag->cfeLen);
	// check kernel existence
	kernelAddr = (unsigned long) strtoul((char *) pTag->kernelAddress, NULL, 10);
	kernelSize = atoi((char *) pTag->kernelLen);
	// check root filesystem existence
	rootfsAddr = (unsigned long) strtoul((char *) pTag->rootfsAddress, NULL, 10);
	rootfsSize = atoi((char *) pTag->rootfsLen);
	cmsLog_notice("cfeSize=%d,cfeAddr=%x, kernelSize=%d,kernelAddr=%x, rootfsSize=%d,rootfsAddress=%x", cfeSize,cfeAddr, kernelSize,kernelAddr, rootfsSize,rootfsAddr);//lc

	if (cfeAddr) {
		fprintf(stderr, "Flashing CFE...\n");
		fflush(stderr);
		buf = cmsMem_alloc(cfeSize, 0);
		if(buf == NULL) {
			cmsLog_error("Failed to allocate memory for the tag buffer. Size required %d", cfeSize);
			return CMSRET_RESOURCE_EXCEEDED;
		}
		memset(buf, 0, cfeSize);				
		fseek(imageFileFp, TAG_BLOCK_LEN, SEEK_SET);
		fread(buf, 1, cfeSize, imageFileFp);	

		ret = devCtl_boardIoctl(BOARD_IOCTL_FLASH_WRITE,
				BCM_IMAGE_CFE,
				buf,
				cfeSize,
				(int) cfeAddr, 0);
		if (ret != CMSRET_SUCCESS) {
			if(buf) {
				cmsMem_free(buf);
				buf = NULL;
			}
			cmsLog_error("Failed to flash CFE");
			return CMSRET_IMAGE_FLASH_FAILED;
		}
		if(buf) {
			cmsMem_free(buf);
			buf = NULL;
		}
	}

#if (INC_NAND_FLASH_DRIVER == 1)    //__MTS__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11 
	if (rootfsAddr) 
#else
	if (rootfsAddr && kernelAddr)
#endif
	{
		char *tagFs = NULL;

		// tag is alway at the sector start of fs
		if (cfeAddr) {	
			tagFs = cmsMem_alloc(TAG_BLOCK_LEN, 0);
			if(tagFs == NULL) {
				cmsLog_error("Failed to allocate memory for the tag buffer. Size required %d", TAG_BLOCK_LEN);
				return CMSRET_RESOURCE_EXCEEDED;
			}
			memset(tagFs, 0, TAG_BLOCK_LEN);
			fseek(imageFileFp, 0, SEEK_SET);
			fread(tagFs, 1, TAG_BLOCK_LEN, imageFileFp);
			fseek(imageFileFp, cfeSize, SEEK_SET);
			fwrite(tagFs, 1, TAG_BLOCK_LEN, imageFileFp);
			if(tagFs) {
				cmsMem_free(tagFs);
				tagFs = NULL;
			}
		}	

//#ifdef ZYXEL_OBM_IMAGE_DEFAULT  /* Image Default, __CHT__, MitraStar Curtis, 20111124. */
#if defined(MSTC_OBM_IMAGE_DEFAULT) || defined(MSTC_ROM_D) //__MSTC__, DennisZyXEL ImageDefault/ROM-D feature, zongyue
		if (0x1 == pTag->imageNext[0]) {
			unsigned int TagCrc, crc;

			pTag_Img = cmsMem_alloc(sizeof(IMAGE_TAG), 0);
			if(pTag_Img == NULL) {
				cmsLog_error("Failed to allocate memory for the tag buffer. Size required %d", sizeof(IMAGE_TAG));
				return CMSRET_RESOURCE_EXCEEDED;
			}
			memset(pTag_Img, 0, sizeof(IMAGE_TAG));		

			search_tag_addr = cfeSize + TAG_BLOCK_LEN + rootfsSize + kernelSize;
			do {
				fseek(imageFileFp, search_tag_addr, SEEK_SET);
				fread(pTag_Img, 1, sizeof(IMAGE_TAG), imageFileFp);
				/* 4-byte boundary protection */
				memcpy(&TagCrc, pTag_Img->tagValidationToken, CRC_LEN);
				crc = CRC32_INIT_VALUE;
				crc = cmsCrc_getCrc32((unsigned char *)pTag_Img, (UINT32)(IMAGE_TAG_LEN-CRC_LEN), crc);
				if (crc != TagCrc) {
					fprintf(stderr,"IMAGE_TAG CRC error. Corrupted image?\n");
					break;
				}
				/* flashFsKernelImage function only process ImageDefault and ROM-D images */
				if (IMAGE_TYPE_IMGDEF == strtoul(pTag_Img->imageType, NULL, 10)) {	
					/* first extra image should be ImageDefault then ROM-D */
					imageSize += (strtoul(pTag_Img->imageLen, NULL, 10) + IMAGE_TAG_LEN);		cmsLog_notice("__(%s@%d) IMAGE_TYPE_IMGDEF imageSize=%d ",__FUNCTION__,__LINE__,strtoul(pTag_Img->imageLen, NULL, 10));
					search_tag_addr += (IMAGE_TAG_LEN + strtoul(pTag_Img->imageLen, NULL, 10));
				} else if (IMAGE_TYPE_ROMD == strtoul(pTag_Img->imageType, NULL, 10)) {
					/* ROM-D is last extra image we want to process at flashFsKernelImage */
					imageSize += (strtoul(pTag_Img->imageLen, NULL, 10) + IMAGE_TAG_LEN);		cmsLog_notice("__(%s@%d) IMAGE_TYPE_ROMD imageSize=%d ",__FUNCTION__,__LINE__,strtoul(pTag_Img->imageLen, NULL, 10));
					search_tag_addr += (IMAGE_TAG_LEN + strtoul(pTag_Img->imageLen, NULL, 10));
					break;
				} else {	
					search_tag_addr += (IMAGE_TAG_LEN + strtoul(pTag_Img->imageLen, NULL, 10));;
				}

				if (search_tag_addr > imageLen) {
					break;
				}
			} while (0x1 == pTag_Img->imageNext[0]);

			if(pTag_Img) {
				cmsMem_free(pTag_Img);
				pTag_Img = NULL;
			}
		}
#endif

        fprintf(stderr, "Flashing root file system and kernel...\n");
        fflush(stderr);

#if 1 
		/* only the buf pointer and length is needed, the offset parameter
		* was present in the legacy code, but is not used. */
		memset(&fwup_flash_parm, 0, sizeof(fwup_flash_parm_t));
		sprintf(fwup_flash_parm.filename, "%s", filepath);
		#if defined(MSTC_OBM_IMAGE_DEFAULT) || defined(MSTC_ROM_D) //__MSTC__, DennisZyXEL ImageDefault/ROM-D feature, zongyue
		fwup_flash_parm.size = (TAG_BLOCK_LEN + rootfsSize + kernelSize + imageSize);
		#else
		fwup_flash_parm.size = (TAG_BLOCK_LEN + rootfsSize+kernelSize);
		#endif
		if(cfeAddr) {
			fwup_flash_parm.offset = cfeSize;
		} else {
			fwup_flash_parm.offset = 0;
		}

		ret = devCtl_boardIoctl(BOARD_IOCTL_FLASH_WRITE, IMAGE_FS_FLASH, NULL, 0, 0, &fwup_flash_parm);
		if (ret != CMSRET_SUCCESS) {
			cmsLog_error("Failed to flash root file system and kernel");
			return CMSRET_IMAGE_FLASH_FAILED;
		}
#endif		
	}

	cmsLog_notice("Image flash done.");

	return CMSRET_SUCCESS;
}
#endif
// depending on the image type, do the brcm image or whole flash image
CmsRet flashImage(char *imagePtr, UINT32 imageLen, CmsImageFormat format)
{
    FILE_TAG *pTag = (FILE_TAG *) imagePtr;
#if 1 //__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11 

#ifdef BUILD_NORWAY_CUSTOMIZATION
    int cfeSize, rootfsSize, kernelSize, noReboot;
#else
    int cfeSize, rootfsSize, kernelSize;
#endif

#else
    int cfeSize, rootfsSize, kernelSize, noReboot;
#endif
    unsigned long cfeAddr, rootfsAddr, kernelAddr;
    CmsRet ret;
#if defined(MSTC_OBM_IMAGE_DEFAULT) || defined(MSTC_ROM_D) //__MSTC__, Dennis ZyXEL ImageDefault/ROM-D feature, zongyue
    PIMAGE_TAG pTag_Img = NULL;
    unsigned long imageSize = 0;
    unsigned long search_tag_addr = 0;
#endif

#ifdef BUILD_NORWAY_CUSTOMIZATION
	if( needReboot == FALSE )
	{
		if (slotSelect == '1')
		{
			noReboot = FLASH_PART2_NO_REBOOT;
		}else {
			noReboot = FLASH_PART1_NO_REBOOT;
		}
	}else{
		noReboot = FLASH_PARTDEFAULT_REBOOT;
	}
#endif

#if 0 //__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11 
    if( (format & CMS_IMAGE_FORMAT_PART1) == CMS_IMAGE_FORMAT_PART1 )
    {
        noReboot = ((format & CMS_IMAGE_FORMAT_NO_REBOOT) == 0)
            ? FLASH_PART1_REBOOT : FLASH_PART1_NO_REBOOT;
    }
    else if( (format & CMS_IMAGE_FORMAT_PART2) == CMS_IMAGE_FORMAT_PART2 )
    {
        noReboot = ((format & CMS_IMAGE_FORMAT_NO_REBOOT) == 0)
            ? FLASH_PART2_REBOOT : FLASH_PART2_NO_REBOOT;
    }
    else
        noReboot = ((format & CMS_IMAGE_FORMAT_NO_REBOOT) == 0)
            ? FLASH_PARTDEFAULT_REBOOT : FLASH_PARTDEFAULT_NO_REBOOT;
 
    format &= ~(CMS_IMAGE_FORMAT_NO_REBOOT | CMS_IMAGE_FORMAT_PART1 |
       CMS_IMAGE_FORMAT_PART2); 
#endif
#if 1 //ZyXEL, ShuYing, not support CMS_IMAGE_FORMAT_FLASH (w file) via ZyXEL GUI
    if (format != CMS_IMAGE_FORMAT_BROADCOM)
    {
       cmsLog_error("invalid image format %d", format);
       return CMSRET_INVALID_IMAGE;
    }
#else
    if (format != CMS_IMAGE_FORMAT_FLASH && format != CMS_IMAGE_FORMAT_BROADCOM)
    {
       cmsLog_error("invalid image format %d", format);
       return CMSRET_INVALID_IMAGE;
    }
#endif
    if (format == CMS_IMAGE_FORMAT_FLASH)  
    {
        cmsLog_notice("Flash whole image...");
        // Pass zero for the base address of whole image flash. It will be filled by kernel code
        // was sysFlashImageSet
        ret = devCtl_boardIoctl(BOARD_IOCTL_FLASH_WRITE,
                                BCM_IMAGE_WHOLE,
                                imagePtr,
                                imageLen-TOKEN_LEN,
                                0, 0);
        if (ret != CMSRET_SUCCESS)
        {
           cmsLog_error("Failed to flash whole image");
           return CMSRET_IMAGE_FLASH_FAILED;
        }
        else
        {
           return CMSRET_SUCCESS;
        }
    }

    /* this must be a broadcom format image */
    // check imageCrc
    cfeSize = rootfsSize = kernelSize = 0;

    // check cfe's existence
    cfeAddr = (unsigned long) strtoul((char *) pTag->cfeAddress, NULL, 10);
    cfeSize = atoi((char *) pTag->cfeLen);
    // check kernel existence
    kernelAddr = (unsigned long) strtoul((char *) pTag->kernelAddress, NULL, 10);
    kernelSize = atoi((char *) pTag->kernelLen);
    // check root filesystem existence
    rootfsAddr = (unsigned long) strtoul((char *) pTag->rootfsAddress, NULL, 10);
    rootfsSize = atoi((char *) pTag->rootfsLen);
    cmsLog_debug("cfeSize=%d kernelSize=%d rootfsSize=%d", cfeSize, kernelSize, rootfsSize);
    
    if (cfeAddr) 
    {
        printf("Flashing CFE...\n");
        ret = devCtl_boardIoctl(BOARD_IOCTL_FLASH_WRITE,
                                BCM_IMAGE_CFE,
#if 1 //__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11 
                                imagePtr+TAG_BLOCK_LEN,
#else
                                imagePtr+TAG_LEN,
#endif
                                cfeSize,
                                (int) cfeAddr, 0);
        if (ret != CMSRET_SUCCESS)
        {
            cmsLog_error("Failed to flash CFE");
            return CMSRET_IMAGE_FLASH_FAILED;
        }
    }
#if (INC_NAND_FLASH_DRIVER==1)    //__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11 
    if (rootfsAddr) 
#else
    if (rootfsAddr && kernelAddr) 
#endif
    {
        char *tagFs = imagePtr;

        // tag is alway at the sector start of fs
        if (cfeAddr)
        {
            tagFs = imagePtr + cfeSize;       // will trash cfe memory, but cfe is already flashed
#if 1 //__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11 
            memcpy(tagFs, imagePtr, TAG_BLOCK_LEN);
#else
            memcpy(tagFs, imagePtr, TAG_LEN);
#endif
        }

#if defined(MSTC_OBM_IMAGE_DEFAULT) || defined(MSTC_ROM_D) //__MSTC__, DennisZyXEL ImageDefault/ROM-D feature, zongyue
        if (0x1 == pTag->imageNext[0]) {
            unsigned int TagCrc, crc;
        
            search_tag_addr = cfeSize + TAG_BLOCK_LEN + rootfsSize + kernelSize;
            do {
                pTag_Img = (PIMAGE_TAG)(imagePtr + search_tag_addr);
                /* 4-byte boundary protection */
                memcpy(&TagCrc, pTag_Img->tagValidationToken, CRC_LEN);
                crc = CRC32_INIT_VALUE;
                crc = cmsCrc_getCrc32((unsigned char *)pTag_Img, (UINT32)(IMAGE_TAG_LEN-CRC_LEN), crc);
                if (crc != TagCrc) {
                    printf("IMAGE_TAG CRC error. Corrupted image?\n");
                    break;
                }
                /* flashFsKernelImage function only process ImageDefault and ROM-D images */
                if (IMAGE_TYPE_IMGDEF == strtoul(pTag_Img->imageType, NULL, 10)) {
                    /* first extra image should be ImageDefault then ROM-D */
                    imageSize += (strtoul(pTag_Img->imageLen, NULL, 10) + IMAGE_TAG_LEN);
                    search_tag_addr += (IMAGE_TAG_LEN + strtoul(pTag_Img->imageLen, NULL, 10));
                }
                else if (IMAGE_TYPE_ROMD == strtoul(pTag_Img->imageType, NULL, 10)) {
                    /* ROM-D is last extra image we want to process at flashFsKernelImage */
                    imageSize += (strtoul(pTag_Img->imageLen, NULL, 10) + IMAGE_TAG_LEN);
                    search_tag_addr += (IMAGE_TAG_LEN + strtoul(pTag_Img->imageLen, NULL, 10));
                    break;
                }
                else {
                    search_tag_addr += (IMAGE_TAG_LEN + strtoul(pTag_Img->imageLen, NULL, 10));;
                }
                
                if (search_tag_addr > imageLen)
                    break;
            } while (0x1 == pTag_Img->imageNext[0]);
        }
#endif
        fprintf(stderr, "Flashing root file system and kernel...\n");
        fflush(stderr);
        /* only the buf pointer and length is needed, the offset parameter
         * was present in the legacy code, but is not used. */
        ret = devCtl_boardIoctl(BOARD_IOCTL_FLASH_WRITE,
                                BCM_IMAGE_FS,
                                tagFs,
#if 1 //__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11 
#if defined(MSTC_OBM_IMAGE_DEFAULT) || defined(MSTC_ROM_D) //__MSTC__, Dennis ZyXEL ImageDefault/ROM-D feature, zongyue
                                TAG_BLOCK_LEN+rootfsSize+kernelSize+imageSize,
#else   
                                TAG_BLOCK_LEN+rootfsSize+kernelSize,
#endif
#ifdef BUILD_NORWAY_CUSTOMIZATION
								noReboot, 0);
#else
                                0, 0); 
#endif
#else
                                TAG_LEN+rootfsSize+kernelSize,
                                noReboot, 0);
#endif
        if (ret != CMSRET_SUCCESS)
        {
            cmsLog_error("Failed to flash root file system and kernel");
            return CMSRET_IMAGE_FLASH_FAILED;
        }
    }

    cmsLog_notice("Image flash done.");
    
    return CMSRET_SUCCESS;
}

#if 1 //__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11 
UINT32 cmsImg_getImageFlashSizeLimit(void)
{   
#ifdef ZYXEL_VMG1312
   return FLASH_IMAGE_DOWNLOAD_SIZE_2;
#else
   return FLASH_IMAGE_DOWNLOAD_SIZE;
#endif //ZYXEL_VMG1312
}
#endif

UINT32 cmsImg_getImageFlashSize(void)
{
   UINT32 flashSize=0;
   CmsRet ret;
   
   ret = devCtl_boardIoctl(BOARD_IOCTL_FLASH_READ,
                           FLASH_SIZE,
                           0, 0, 0, &flashSize);
   if (ret != CMSRET_SUCCESS)
   {
      cmsLog_error("Could not get flash size, return 0");
      flashSize = 0;
   }
   
   return flashSize;
}


UINT32 cmsImg_getBroadcomImageTagSize(void)
{
   return TOKEN_LEN;
}


UINT32 cmsImg_getConfigFlashSize(void)
{
   UINT32 realSize;

   realSize = cmsImg_getRealConfigFlashSize();

#ifdef COMPRESSED_CONFIG_FILE   
   /*
    * A multiplier of 2 is now too small for some of the big voice and WLAN configs,   
    * so allow for the possibility of 4x compression ratio.  In a recent test on the
    * 6368 with wireless enabled, I got a compression ratio of 3.5.
    * The real test comes in management.c: after we do the
    * compression, writeValidatedConfigBuf will verify that the compressed buffer can
    * fit into the flash.
    * A 4x multiplier should be OK for small memory systems such as the 6338.
    * The kernel does not allocate physical pages until they are touched.
    * However, allocating an overly large buffer could be rejected immediately by the
    * kernel because it does not know we don't actually plan to use the entire buffer.
    * So if this is a problem on the 6338, we could improve this algorithm to
    * use a smaller factor on low memory systems.
    */
   realSize = realSize * 4;
#endif

   return realSize;
}


UINT32 cmsImg_getRealConfigFlashSize(void)
{
   CmsRet ret;
   UINT32 size=0;

   ret = devCtl_boardIoctl(BOARD_IOCTL_GET_PSI_SIZE, 0, NULL, 0, 0, (void *)&size);
   if (ret != CMSRET_SUCCESS)
   {
      cmsLog_error("boardIoctl to get config flash size failed.");
      size = 0;
   }

   return size;
}


UBOOL8 cmsImg_willFitInFlash(UINT32 imageSize)
{
   UINT32 flashSize;
   
   flashSize = cmsImg_getImageFlashSize();

   cmsLog_debug("flash size is %u bytes, imageSize=%u bytes", flashSize, imageSize);
                     
   return (flashSize > (imageSize + CMS_IMAGE_OVERHEAD));
}


UBOOL8 cmsImg_isBackupConfigFlashAvailable(void)
{
   static UBOOL8 firstTime=TRUE;
   static UBOOL8 avail=FALSE;
   UINT32 size=0;
   CmsRet ret;

   if (firstTime)
   {
      ret = devCtl_boardIoctl(BOARD_IOCTL_GET_BACKUP_PSI_SIZE, 0, NULL, 0, 0, (void *)&size);
      if (ret == CMSRET_SUCCESS && size > 0)
      {
         avail = TRUE;
      }

      firstTime = FALSE;
   }

   return avail;
}


UBOOL8 cmsImg_isConfigFileLikely(const char *buf)
{
   const char *header = "<?xml version";
   const char *dslCpeConfig = "<DslCpeConfig";
   UINT32 len, i;
   UBOOL8 likely=FALSE;
   
   if (strncmp(buf, "<?xml version", strlen(header)) == 0)
   {
      len = strlen(dslCpeConfig);
      for (i=20; i<50 && !likely; i++)
      {
         if (strncmp(&(buf[i]), dslCpeConfig, len) == 0)
         {
            likely = TRUE;
         }
      }
   }

//__MSTC__, Paul Ho, 2011.06.24, to compress configuration file
#if defined(COMPRESSED_CONFIG_FILE) && defined(SUPPORT_MSTC_COMPRESS_CONF_FILE)
   else if ((!strncmp(buf, COMPRESSED_CONFIG_HEADER, strlen(COMPRESSED_CONFIG_HEADER))))
   {
      likely = TRUE;
   }
#endif
   
   cmsLog_debug("returning likely=%d", likely);
   
   return likely;
}

#ifdef BUILD_NORWAY_CUSTOMIZATION
CmsRet cmsImg_writeImageWithoutReboot(char *imagePtr, UINT32 imageLen, CmsImageFormat format, void *msgHandle, char imageSlot)
{
   CmsRet ret = CMSRET_SUCCESS;
   
   needReboot = FALSE;
   /* change partition we want to write image */
   slotSelect = imageSlot;
	
   /* start to write image */
   ret = cmsImg_writeValidatedImage(imagePtr, imageLen, format, msgHandle);
   if( ret != CMSRET_SUCCESS )
   {
		cmsLog_error("cmsImg_writeValidatedImage is failed");
		return ret;
   }
   
   needReboot = TRUE;
   
   return ret;
}
#endif

#ifdef MSTC_FWUP_FROM_FILE //__MSTC__, LingChun, merge MSTC_FWUP_FROM_FLASH from telefonica, http://svn.zyxel.com.tw/svn/CPE_TRUNK/BRCM_412/Telefonica_Common/
CmsRet cmsImg_writeImageFile(FILE *imageFileFp, UINT32 imageLen, void *msgHandle, char* filepath)
{
	CmsImageFormat format;
	CmsRet ret;

	if(imageFileFp == NULL) {
		cmsLog_error("File point is NULL.");
		return CMS_IMAGE_FORMAT_INVALID;
	}

	if ((format = cmsImg_validateImageFile(imageFileFp, imageLen, msgHandle)) == CMS_IMAGE_FORMAT_INVALID) {
		ret = CMSRET_INVALID_IMAGE;
	} else {
		ret = cmsImg_writeValidatedImageFile(imageFileFp, imageLen, format, msgHandle, filepath);
	}

	return ret;
}

CmsRet cmsImg_writeValidatedImageFile(FILE *imageFileFp, UINT32 imageLen, CmsImageFormat format, void *msgHandle, char* filepath)
{
	CmsRet ret = CMSRET_SUCCESS;	

	if(imageFileFp == NULL) {
		cmsLog_error("File point is NULL.");
		return CMS_IMAGE_FORMAT_INVALID;
	}			
	
	switch(format & ~(CMS_IMAGE_FORMAT_NO_REBOOT | CMS_IMAGE_FORMAT_PART1 | CMS_IMAGE_FORMAT_PART2)) {
#if 1//__MSTC__, LingChun
		case CMS_IMAGE_FORMAT_FLASH:
#endif
		case CMS_IMAGE_FORMAT_BROADCOM:
			// BcmNtwk_unInit(); mwang_todo: is it important to let Wireless do some
			// uninit before we write the flash image?
			ret = flashImageFlash(imageFileFp, imageLen, format, filepath);
			break;

		default:
			cmsLog_error("Unrecognized image format=%d", format);
			ret = CMSRET_INVALID_IMAGE;
			break;
	}

	return ret;
}

CmsImageFormat cmsImg_validateImageFile(const FILE *imageFileFp, UINT32 imageLen, void *msgHandle)
{
	CmsImageFormat result = CMS_IMAGE_FORMAT_INVALID;
	CmsRet ret;
	UINT32 tagLen = 0; 
	UINT8 *buf = NULL;
	UINT32 maxLen = 0;
	   
	if(imageFileFp == NULL) {
		return CMS_IMAGE_FORMAT_INVALID;
	}

	if ((imageLen > sizeof(FILE_TAG)) && (verifyBroadcomFileTagFlash(imageFileFp) == CMSRET_SUCCESS)) {
		/* Found a valid Broadcom defined TAG record at the beginning of the image */
		cmsLog_debug("Broadcom format verified.");
		maxLen = cmsImg_getImageFlashSize() + cmsImg_getBroadcomImageTagSize();
		if (imageLen > maxLen) {
			cmsLog_error("broadcom image is too large for flash, got %u, max %u", imageLen, maxLen);
		} else {
			result = CMS_IMAGE_FORMAT_BROADCOM;
		}
	}
	else {	
		/* if It is not a Broadcom flash format file.  Now check if it is a
		* flash image format file.  A flash image format file must have a
		* CRC at the end of the image.
		*/
		UINT32 crc = CRC_INITIAL_VALUE;
		UINT32 imageCrc;
		UINT8 *crcPtr;
		UINT8 c = 0;
		UINT32 i;					

		if (imageLen > TOKEN_LEN)
		{								
			buf = cmsMem_alloc(TOKEN_LEN, 0);
			if(buf == NULL) {
				cmsLog_error("Failed to allocate memory for the tag buffer. Size required %d", TOKEN_LEN);
				return CMSRET_RESOURCE_EXCEEDED;
			}
			memset(buf, 0, TOKEN_LEN);									
			fseek(imageFileFp, (imageLen - TOKEN_LEN), SEEK_SET);		
			fread(buf, 1, TOKEN_LEN, imageFileFp);				
			crcPtr = buf;											
			cmsLog_notice("__(%s@%d)__ imageLen=%d,crcPtr[0]=%d,crcPtr[1]=%d,crcPtr[2]=%d,crcPtr[3]=%d ",__FUNCTION__,__LINE__,imageLen,crcPtr[0],crcPtr[1],crcPtr[2],crcPtr[3]);
			/*
			* CRC may not be word aligned, so extract the bytes out one-by-one.
			* Whole image CRC is calculated, then htonl, then written out using
			* fwrite (see addvtoken.c in hostTools).	Because of the way we are
			* extracting the CRC here, we don't have to swap for endieness when
			* doing compares on desktop Linux and modem (?).
			*/
			imageCrc = (crcPtr[0] << 24) | (crcPtr[1] << 16) | (crcPtr[2] << 8) | crcPtr[3];	
			cmsMem_free(buf);
			buf = NULL;

			fseek(imageFileFp, 0, SEEK_SET);		
			i = (imageLen - TOKEN_LEN);			
			crc = CRC_INITIAL_VALUE;			
			while(i-- > 0) {
				c = (UINT8)fgetc(imageFileFp);		//cmsLog_error("__(%s@%d)__ c=%d ",__FUNCTION__,__LINE__,c);
				crc = cmsCrc_getCrc32(&c, 1, crc);	//cmsLog_error("__(%s@%d)__ i=%d end ",__FUNCTION__,__LINE__,i);
			}

			if (crc == imageCrc) {
				UINT32 maxLen;

				cmsLog_notice("Whole flash image format [%ld bytes] verified.", imageLen);
				cmsLog_debug("calculated crc=0x%x image crc=0x%x", crc, imageCrc);
				maxLen = cmsImg_getImageFlashSize();
				if (imageLen > maxLen) {
					cmsLog_error("whole image is too large for flash, got %u, max %u", imageLen, maxLen);
				} else {
					result = CMS_IMAGE_FORMAT_FLASH;
				}
			} else {
				cmsLog_error("Could not determine image format [%d bytes]", imageLen);
				cmsLog_notice("calculated crc=0x%x image crc=0x%x", crc, imageCrc);
			}
		}
	}

	if(result == CMS_IMAGE_FORMAT_BROADCOM) {
		tagLen = sizeof(FILE_TAG);
		buf = cmsMem_alloc(tagLen, 0);
		if(buf == NULL) {
			cmsLog_error("Failed to allocate memory for the tag buffer. Size required %d", tagLen);
			return CMSRET_RESOURCE_EXCEEDED;
		}
		fseek(imageFileFp, 0, SEEK_SET);
		fread(buf, 1, tagLen, imageFileFp);

#if 1//LingChun
		if(verifyModelId_MSTC((FILE_TAG *)buf) != CMSRET_SUCCESS) {
#else
		if(verifyZyXELModelId((FILE_TAG *)buf) != CMSRET_SUCCESS) {
#endif			
			result = CMS_IMAGE_MODELID_INVALID;
		}
		fseek(imageFileFp, 0, SEEK_SET);
		cmsMem_free(buf);
		buf = NULL;
	}

	cmsLog_debug("returning image format %d", result);

	return result;
}
#endif

/** General entry point for writing the image.
 *  The image can be a flash image or a config file.
 *  This function will first determine the image type, which has the side
 *  effect of validating it.
 */
CmsRet cmsImg_writeImage(char *imagePtr, UINT32 imageLen, void *msgHandle)
{
   CmsImageFormat format;
   CmsRet ret;

   if ((format = cmsImg_validateImage(imagePtr, imageLen, msgHandle)) == CMS_IMAGE_FORMAT_INVALID)
   {
      ret = CMSRET_INVALID_IMAGE;
   }
   else
   {
      ret = cmsImg_writeValidatedImage(imagePtr, imageLen, format, msgHandle);
   }

   return ret;
}


CmsRet cmsImg_writeValidatedImage(char *imagePtr, UINT32 imageLen, CmsImageFormat format, void *msgHandle)
{
   CmsRet ret=CMSRET_SUCCESS;
   
   switch(format & ~(CMS_IMAGE_FORMAT_NO_REBOOT | CMS_IMAGE_FORMAT_PART1 | CMS_IMAGE_FORMAT_PART2))
   {
   case CMS_IMAGE_FORMAT_BROADCOM:
   case CMS_IMAGE_FORMAT_FLASH:
      // BcmNtwk_unInit(); mwang_todo: is it important to let Wireless do some
      // uninit before we write the flash image?
#ifdef SUPPORT_ROMFILE_CONTROL
	  sendSaveCurFWVerMsg(msgHandle);
	  sleep(10);
#endif  	  
#ifdef TR69_AUTOREDOWNLOADFW //ZyXEL, Nick Lu, after plug out the power line when FW upgrade(TR069 download method), CPE will re-download FW auto
      {
	 CmsEntityId srcEid = cmsMsg_getHandleEid(msgHandle);

	 if(srcEid == EID_TR69C)
	 {
	    FILE_TAG tagData;

	    if ( !sysGetTAGFromFlash( &tagData ) )
	    {
	       char TempStr[32], cmd[128] = {0};

	       memset(TempStr, 0, sizeof(TempStr));
	       memcpy((char*)TempStr, (char *)tagData.internalversion, VERSION_LEN-1);
	       sprintf(cmd, "echo \"%s\" > /data/oldFwVersion", TempStr);
	       system(cmd);
	    }
	 }
      }
#endif
#ifdef MSTC_SAVE_LOG_TO_FLASH //__MSTC__, TengChang, write log before flashImage()
      saveLog(msgHandle);
#endif //__MSTC__, TengChang, for save log to flash

      ret = flashImage(imagePtr, imageLen, format);
#if 1 //__MSTC__, TengChang
      {
         CmsEntityId eid = cmsMsg_getHandleEid(msgHandle);
         if (eid != EID_HTTPD && eid != EID_HTTPD_SSL)
         {
#ifdef BUILD_NORWAY_CUSTOMIZATION		 
		if ( needReboot == TRUE )
		{
#endif
#ifdef MSTC_SAVE_LOG_TO_FLASH //__MSTC__, TengChang
		            sendRequestRebootMsgNoSaveLog(msgHandle);
#else
		            cmsUtil_sendRequestRebootMsg(msgHandle);
#endif //__MSTC__, TengChang
#ifdef BUILD_NORWAY_CUSTOMIZATION		 
		}
#endif	
         }
      }
#endif //__MSTC__, TengChang
      break;
 
#if 1 //__COMMON__, jchang, ROM-D      
   case CMS_IMAGE_FORMAT_XML_CFG_ROM_D:
      ret = sendConfigMsg(imagePtr, imageLen, msgHandle, CMS_MSG_WRITE_ROMD_FILE);
	  
      if (ret == CMSRET_SUCCESS)
      {
#ifdef TR69_SUPPORT_UPDATE_ROMD // __ZyXEL__, Wood, [TR69] when upgrade romd, also setup configuration.
			/*__ZyXEL__,Cj_Lai , To cancel this feature , when TR069 with FTP upgrade rom-d also upgrade runing config*/
			//ret = sendConfigMsg(imagePtr, imageLen, msgHandle, CMS_MSG_WRITE_CONFIG_FILE);
#endif
	 /*
          * Emulate the behavior of the driver when a flash image is written.
          * When we write a config image, also request immediate reboot
          * because we don't want to let any other app save the config file
          * to flash, thus wiping out what we just written.
          */
         cmsLog_debug("config file download written to userdef config, request reboot");
         cmsUtil_sendRequestRebootMsg(msgHandle);
      }
      break;
#endif   
     
   case CMS_IMAGE_FORMAT_XML_CFG:
      ret = sendConfigMsg(imagePtr, imageLen, msgHandle, CMS_MSG_WRITE_CONFIG_FILE);
      if (ret == CMSRET_SUCCESS)
      {
         /*
          * Emulate the behavior of the driver when a flash image is written.
          * When we write a config image, also request immediate reboot
          * because we don't want to let any other app save the config file
          * to flash, thus wiping out what we just written.
          */
         cmsLog_debug("config file download written, request reboot");
         cmsUtil_sendRequestRebootMsg(msgHandle);
      }
      break;
      
#ifdef SUPPORT_MOD_SW_UPDATE
   case CMS_IMAGE_FORMAT_MODSW_PKG:
   {
      char filename[BUFLEN_1024]={0};

      cmsFil_removeDir(SW_UPDATE_DATA_DIR);
      cmsFil_makeDir(SW_UPDATE_DATA_DIR);

      snprintf(filename, sizeof(filename)-1, "%s/%s",
                         SW_UPDATE_DATA_DIR, SW_UPDATE_PKG_FILE);
      cmsFil_writeBufferToFile(filename, (UINT8 *)imagePtr, imageLen);
      /*
       * modupdtd will unpack the sw package, and then send a graceful
       * shutdown msg to smd.
       */
      sendStartModupdtdMsg(msgHandle);
      break;
   }
#endif

   default:
       cmsLog_error("Unrecognized image format=%d", format);
       ret = CMSRET_INVALID_IMAGE;
       break;
    }
   
   return ret;
}

#ifdef SUPPORT_CONFIGURATION_FILTER /*CFG Filter*/
CmsRet cmsImg_procCFGFilter(char *imagePtr, UINT32 imageLen, void *msgHandle,void **responseMsg,char *curUserName)
{
	char *buf=NULL;
	char *body=NULL;
	CmsMsgHeader *msg;
	CmsRet ret;
    char curUserNameCmdBuf[BUFLEN_32]= {0};
	int ext_len = 0;
	FILE *fsin;
	UINT32 count=0;
	char inputpath[]="/var/cfginput";	
	
	cmsLog_debug ("imageLen = %d",imageLen);

	if (imagePtr == NULL)
	{
		cmsLog_error("imagePtr == NULL\n");	
	   return CMSRET_INVALID_ARGUMENTS;
	}
	if (!(imageLen > CMS_CONFIG_FILE_DETECTION_LENGTH &&
		cmsImg_isConfigFileLikely(imagePtr)))
	{
		cmsLog_error("Format Invalid \n");		
		return CMSRET_INVALID_ARGUMENTS;
	}
	if(curUserName==NULL){
		cmsLog_error("ERROR: NO curUserName setting \n");		
		return CMSRET_INVALID_ARGUMENTS;
	}

	cmsLog_debug ("open %s\n",inputpath);
	if(NULL == (fsin = fopen(inputpath ,"wb+"))){
		cmsLog_error("open file error %s\n",inputpath);
		return CMSRET_INTERNAL_ERROR;
	}
	else if(imageLen != (count = fwrite(imagePtr, 1, imageLen, fsin)))
	{
		cmsLog_error("write of input config file failed, count=%u len=%u \n",count,imageLen);
		fclose(fsin);
		return CMSRET_INTERNAL_ERROR;		
	}else{
		fclose(fsin);
		ext_len = strlen(curUserName);
		cmsLog_debug("ext_len=%d", ext_len);
		
		if ((buf = cmsMem_alloc(sizeof(CmsMsgHeader) + ext_len, ALLOC_ZEROIZE)) == NULL)
		{
		   cmsLog_error("failed to allocate %d bytes for msg 0x%x", 
						sizeof(CmsMsgHeader) + ext_len, CMS_MSG_PROC_CONFIG_FILTER);
		   return CMSRET_RESOURCE_EXCEEDED;
		}	
		cmsLog_debug ("sizeof(CmsMsgHeader) + ext_len = %d\n",sizeof(CmsMsgHeader) + ext_len);

		msg = (CmsMsgHeader *) buf;
		body = (char *) (msg + 1);
		 
		msg->type = CMS_MSG_PROC_CONFIG_FILTER;
		msg->src = cmsMsg_getHandleEid(msgHandle);
		msg->dst = EID_SMD;
		msg->flags_request = 1;
		msg->dataLength = ext_len;
		memcpy(body, curUserName, ext_len);

		cmsLog_debug("send mesg \n");
		if ((ret = cmsMsg_send(msgHandle, buf)) != CMSRET_SUCCESS)
		{
		   	cmsLog_error("could not send CMS_MSG_PROC_CONFIG_FILTER msg to smd.");
			CMSMEM_FREE_BUF_AND_NULL_PTR(buf);	   
		   return ret;
		}
		CMSMEM_FREE_BUF_AND_NULL_PTR(buf);
		
		cmsLog_debug("receive mesg \n");
	    if ((ret = cmsMsg_receive(msgHandle, (CmsMsgHeader **)responseMsg)) != CMSRET_SUCCESS)
	    {
	       cmsLog_error("could not receive CMS_MSG_PROC_CONFIG_FILTER msg from smd.");
	       CMSMEM_FREE_BUF_AND_NULL_PTR(responseMsg);
	       return ret;
	    }   
	}
	return ret;
}
#endif
 
CmsImageFormat cmsImg_validateImage(const char *imageBuf, UINT32 imageLen, void *msgHandle)
{
   CmsImageFormat result = CMS_IMAGE_FORMAT_INVALID;
   CmsRet ret;
   
   if (imageBuf == NULL)
   {
      return CMS_IMAGE_FORMAT_INVALID;
   }
   
   if (imageLen > CMS_CONFIG_FILE_DETECTION_LENGTH &&
       cmsImg_isConfigFileLikely(imageBuf))
   {
      cmsLog_debug("possible CMS XML config file format detected");
      ret = sendConfigMsg(imageBuf, imageLen, msgHandle, CMS_MSG_VALIDATE_CONFIG_FILE);
      if (ret == CMSRET_SUCCESS)
      { 
         cmsLog_debug("CMS XML config format verified.");
         return CMS_IMAGE_FORMAT_XML_CFG;
      }
   } 
   
   cmsLog_debug("not a config file");
   
#ifdef SUPPORT_MOD_SW_UPDATE
   if (cmsImg_isModSwPkg((unsigned char *) imageBuf, imageLen))
   {
      cmsLog_debug("detected modular sw pkg format!");
      return CMS_IMAGE_FORMAT_MODSW_PKG;
   }

   cmsLog_debug("not a modular sw pkg");
#endif

#if 1 //__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11 
   if ((imageLen > sizeof(FILE_TAG)) 
        && (verifyBroadcomFileTag((FILE_TAG *) imageBuf) == CMSRET_SUCCESS))
#else
   if ((imageLen > sizeof(FILE_TAG)) 
        && (verifyBroadcomFileTag((FILE_TAG *) imageBuf, imageLen) == CMSRET_SUCCESS))
#endif
   {
      UINT32 maxLen;
      
      /* Found a valid Broadcom defined TAG record at the beginning of the image */
      cmsLog_debug("Broadcom format verified.");
      maxLen = cmsImg_getImageFlashSize() + cmsImg_getBroadcomImageTagSize();
      if (imageLen > maxLen)
      {
         cmsLog_error("broadcom image is too large for flash, got %u, max %u", imageLen, maxLen);
      }
      else
      {
         result = CMS_IMAGE_FORMAT_BROADCOM;
      }
   }
   else
   {
      /* if It is not a Broadcom flash format file.  Now check if it is a
       * flash image format file.  A flash image format file must have a
       * CRC at the end of the image.
       */
      UINT32 crc = CRC_INITIAL_VALUE;
      UINT32 imageCrc;
      UINT8 *crcPtr;
      
      if (imageLen > TOKEN_LEN)
      {
         crcPtr = (UINT8 *) (imageBuf + (imageLen - TOKEN_LEN));
         /*
          * CRC may not be word aligned, so extract the bytes out one-by-one.
          * Whole image CRC is calculated, then htonl, then written out using
          * fwrite (see addvtoken.c in hostTools).  Because of the way we are
          * extracting the CRC here, we don't have to swap for endieness when
          * doing compares on desktop Linux and modem (?).
          */
         imageCrc = (crcPtr[0] << 24) | (crcPtr[1] << 16) | (crcPtr[2] << 8) | crcPtr[3];
      
         crc = cmsCrc_getCrc32((unsigned char *) imageBuf, imageLen - TOKEN_LEN, crc);      
         if (crc == imageCrc)
         {
            UINT32 maxLen;
          
            cmsLog_debug("Whole flash image format [%ld bytes] verified.", imageLen);
            maxLen = cmsImg_getImageFlashSize();
            if (imageLen > maxLen)
            {
               cmsLog_error("whole image is too large for flash, got %u, max %u", imageLen, maxLen);
            }
            else
            {
               result = CMS_IMAGE_FORMAT_FLASH;
            }
         }
         else
         {
#if defined(EPON_SDK_BUILD)
            cmsLog_debug("Could not determine image format [%d bytes]", imageLen);
#else
            cmsLog_error("Could not determine image format [%d bytes]", imageLen);
#endif
            cmsLog_debug("calculated crc=0x%x image crc=0x%x", crc, imageCrc);
         }
      }
   }

#if 1 //__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11 
   if ( result == CMS_IMAGE_FORMAT_BROADCOM ) {
      if ( verifyModelId_MSTC((FILE_TAG *) imageBuf) != CMSRET_SUCCESS ) {
	     result = CMS_IMAGE_MODELID_INVALID;
      }
   }
#endif
   cmsLog_debug("returning image format %d", result);

   return result;
}

CmsRet sendConfigMsg(const char *imagePtr, UINT32 imageLen, void *msgHandle, CmsMsgType msgType)
{
   char *buf=NULL;
   char *body=NULL;
   CmsMsgHeader *msg;
   CmsRet ret;

   if ((buf = cmsMem_alloc(sizeof(CmsMsgHeader) + imageLen, ALLOC_ZEROIZE)) == NULL)
   {
      cmsLog_error("failed to allocate %d bytes for msg 0x%x", 
                   sizeof(CmsMsgHeader) + imageLen, msgType);
      return CMSRET_RESOURCE_EXCEEDED;
   }
   
   msg = (CmsMsgHeader *) buf;
   body = (char *) (msg + 1);
    
   msg->type = msgType;
   msg->src = cmsMsg_getHandleEid(msgHandle);
   msg->dst = EID_SMD;
   msg->flags_request = 1;
   msg->dataLength = imageLen;
   
   memcpy(body, imagePtr, imageLen);

   ret = cmsMsg_sendAndGetReply(msgHandle, msg);
   
   cmsMem_free(buf);
   
   return ret;
}


void cmsImg_sendLoadStartingMsg(void *msgHandle, const char *connIfName)
{
   CmsMsgHeader *msg;
   char *data;
   void *msgBuf;
   UINT32 msgDataLen=0;

   #ifdef SIPLOAD
   CmsRet ret = CMSRET_SUCCESS;   
   CmsMsgHeader msg2 = EMPTY_MSG_HEADER;     
   CmsMsgHeader msgHdr = EMPTY_MSG_HEADER;
#ifdef SUPPORT_ZYIMSWATCHDOG	
	msgHdr.dst = EID_SSK;
	msgHdr.src = cmsMsg_getHandleEid(msgHandle);
	msgHdr.type = CMS_MSG_ZYIMSWATCHDOG_SET;
	msgHdr.flags_request = 1;
	msgHdr.wordData = 0; /*stop watchdog*/
	if ( (ret = cmsMsg_send(msgHandle, &msgHdr)) != CMSRET_SUCCESS )
	{
		  cmsLog_error( "Could not send CMS_MSG_ZYIMSWATCHDOG_SET msg to ssk, ret=%d", ret );
	}
#endif
   msg2.src = cmsMsg_getHandleEid(msgHandle);
   msg2.dst = EID_ZYIMS;
   msg2.type = CMS_MSG_VOICE_STOP_REQUEST;
   msg2.flags_request = 1;
   msg2.flags_response = 0;
   msg2.dataLength = 0;
   msg2.wordData = 2; /*1- REBOOT, 2- FIRMWARE LOAD STARTING*/
   
   cmsLog_debug("send CMS_MSG_VOICE_STOP_REQUEST \n");

   
   while(1){
	   	/*wait 5 seconds*/
	   if ((ret = cmsMsg_sendAndGetReplyWithTimeout(msgHandle, &msg2, 5000)) != CMSRET_SUCCESS)
	   {    
	   		if(ret == CMSRET_REQUEST_DENIED){
				cmsLog_error("CMS_MSG_VOICE_STOP_REQUEST request denied by voiceApp\n");
			}
	   		else{ /*timeout or other errors*/
				cmsLog_error("CMS_MSG_VOICE_STOP_REQUEST timeout ret = 0x%x, continue\n", ret);
				break;
	   		}
			
	   }   
	   else{
	   	  	cmsLog_debug("send CMS_MSG_VOICE_STOP_REQUEST success, continue\n");
			break;
	   }
	   sleep(1);
   }   


   #endif

   /* for the msg and the connIfName */
   if (connIfName)
   {
      msgDataLen = strlen(connIfName) + 1;
      msgBuf = cmsMem_alloc(sizeof(CmsMsgHeader) + msgDataLen, ALLOC_ZEROIZE);
   } 
   else
   {
      cmsLog_error("msg without connIfName");
      msgBuf = cmsMem_alloc(sizeof(CmsMsgHeader), ALLOC_ZEROIZE);
   }
   
   msg = (CmsMsgHeader *)msgBuf;
   msg->src = cmsMsg_getHandleEid(msgHandle);
   msg->dst = EID_SMD;
   msg->flags_request = 1;
   msg->type = CMS_MSG_LOAD_IMAGE_STARTING;

   if (connIfName)
   {
      data = (char *) (msg + 1);
      msg->dataLength = msgDataLen;
      memcpy(data, (char *)connIfName, msgDataLen);      
   }
   
   cmsLog_debug("connIfName=%s", connIfName);

   cmsMsg_sendAndGetReply(msgHandle, msg);

   CMSMEM_FREE_BUF_AND_NULL_PTR(msgBuf);

}


void cmsImg_sendLoadDoneMsg(void *msgHandle)
{
   CmsMsgHeader msg = EMPTY_MSG_HEADER;

   msg.type = CMS_MSG_LOAD_IMAGE_DONE;
   msg.src = cmsMsg_getHandleEid(msgHandle);
   msg.dst = EID_SMD;
   msg.flags_request = 1;
   
   cmsMsg_sendAndGetReply(msgHandle, &msg);
}


#ifdef SUPPORT_MOD_SW_UPDATE
void sendStartModupdtdMsg(void *msgHandle)
{
   CmsMsgHeader msg = EMPTY_MSG_HEADER;

   msg.type = CMS_MSG_START_APP;
   msg.src = cmsMsg_getHandleEid(msgHandle);
   msg.dst = EID_SMD;
   msg.wordData = EID_MODUPDTD;
   msg.flags_request = 1;

   cmsMsg_sendAndGetReply(msgHandle, &msg);
}
#endif

#ifdef SUPPORT_ROMFILE_CONTROL
void sendSaveCurFWVerMsg(void *msgHandle)
{
	CmsMsgHeader msg = EMPTY_MSG_HEADER;

	msg.type = CMS_MSG_SAVE_FW_VERSION;
	msg.src = cmsMsg_getHandleEid(msgHandle);
	msg.dst = EID_SMD;
	msg.flags_request = 1;

	cmsMsg_sendAndGetReply(msgHandle, &msg);
}
#endif

/* using a cmsUtil_ prefix because this can be used in non-upload scenarios */
void cmsUtil_sendRequestRebootMsg(void *msgHandle)
{
   CmsMsgHeader msg = EMPTY_MSG_HEADER;

   #ifdef SIPLOAD
   CmsRet ret;   
   CmsMsgHeader msg2;

#ifdef SUPPORT_ZYIMSWATCHDOG	
	msg.dst = EID_SSK;
	msg.src = cmsMsg_getHandleEid(msgHandle);
	msg.type = CMS_MSG_ZYIMSWATCHDOG_SET;
	msg.flags_request = 1;
	msg.wordData = 0; /*stop watchdog*/
	if ( (ret = cmsMsg_send(msgHandle, &msg)) != CMSRET_SUCCESS )
	{
		  cmsLog_error( "Could not send CMS_MSG_ZYIMSWATCHDOG_SET msg to ssk, ret=%d", ret );
	}
#endif

   msg2.src = cmsMsg_getHandleEid(msgHandle);
   msg2.dst = EID_ZYIMS;
   msg2.type = CMS_MSG_VOICE_STOP_REQUEST;
   msg2.flags_request = 1;
   msg2.flags_response = 0;
   msg2.dataLength = 0;
   msg2.wordData = 1; /*1- REBOOT, 2- FIRMWARE LOAD STARTING*/

   cmsLog_debug("send CMS_MSG_VOICE_STOP_REQUEST \n");
   
   
   
   if ((ret = cmsMsg_sendAndGetReplyWithTimeout(msgHandle, &msg2, 6000)) != CMSRET_SUCCESS)
   {    
   		if(ret == CMSRET_REQUEST_DENIED){
			cmsLog_error("CMS_MSG_VOICE_STOP_REQUEST request denied by voiceApp\n");
			return;
		}
   		else{ /*timeout or other errors*/
			cmsLog_error("CMS_MSG_VOICE_STOP_REQUEST timeout ret = 0x%x, continue\n", ret);			
   		}
   }   
   else{
   	  	cmsLog_debug("send CMS_MSG_VOICE_STOP_REQUEST success, continue\n");	   	
   } 
   
   #endif



#ifdef MSTC_SAVE_LOG_TO_FLASH //__MSTC__, TengChang
   saveLog(msgHandle);
#endif //__MSTC__, TengChang
   msg.type = CMS_MSG_REBOOT_SYSTEM;
   msg.src = cmsMsg_getHandleEid(msgHandle);
   msg.dst = EID_SMD;
   msg.flags_request = 1;

   cmsMsg_sendAndGetReply(msgHandle, &msg);
}

#ifdef MSTC_SAVE_LOG_TO_FLASH //__MSTC__, TengChang
static void saveLog(void *msgHandle)
{
	CmsMsgHeader msg = EMPTY_MSG_HEADER, msg2 = EMPTY_MSG_HEADER;

	msg.type = CMS_MSG_IS_APP_RUNNING;
	msg.src = cmsMsg_getHandleEid(msgHandle);
	msg.dst = EID_SMD;
	msg.flags_request = 1;
	msg.wordData = EID_SYSLOGD;
	if( cmsMsg_sendAndGetReply(msgHandle, &msg) == CMSRET_SUCCESS ){
		msg2.type = CMS_MSG_SAVE_LOG;
		msg2.src = cmsMsg_getHandleEid(msgHandle);
		msg2.dst = EID_SYSLOGD;
		msg2.flags_request = 1;
		cmsMsg_sendAndGetReply(msgHandle, &msg2);
	}
}

static void sendRequestRebootMsgNoSaveLog(void *msgHandle)
{
   CmsMsgHeader msg = EMPTY_MSG_HEADER;

   msg.type = CMS_MSG_REBOOT_SYSTEM;
   msg.src = cmsMsg_getHandleEid(msgHandle);
   msg.dst = EID_SMD;
   msg.flags_request = 1;

   cmsMsg_sendAndGetReply(msgHandle, &msg);
}
#endif //__MSTC__, TengChang

CmsRet cmsImg_saveIfNameFromSocket(SINT32 socketfd, char *connIfName)
{
  
   SINT32 i = 0;
   SINT32 fd = 0;
   SINT32 numifs = 0;
   UINT32 bufsize = 0;
   struct ifreq *all_ifr = NULL;
   struct ifconf ifc;
   struct sockaddr local_addr;
   socklen_t local_len = sizeof(struct sockaddr_in);

   if (socketfd < 0 || connIfName == NULL)
   {
      cmsLog_error("cmsImg_saveIfNameFromSocket: Invalid parameters: socket=%d, connIfName=%s", socketfd, connIfName);
      return CMSRET_INTERNAL_ERROR;
   }
   memset(&ifc, 0, sizeof(struct ifconf));
   memset(&local_addr, 0, sizeof(struct sockaddr));
   
   if (getsockname(socketfd, &local_addr,&local_len) < 0) 
   {
      cmsLog_error("cmsImg_saveIfNameFromSocket: Error in getsockname.");
      return CMSRET_INTERNAL_ERROR;
   }

   /* cmsLog_error("cmsImg_saveIfNameFromSocket: Session comes from: %s", inet_ntoa(((struct sockaddr_in *)&local_addr)->sin_addr)); */
   
   if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) 
   {
      cmsLog_error("cmsImg_saveIfNameFromSocket: Error openning socket when getting socket intface info");
      return CMSRET_INTERNAL_ERROR;
   }
   
   numifs = 16;

   bufsize = numifs*sizeof(struct ifreq);
   all_ifr = (struct ifreq *)cmsMem_alloc(bufsize, ALLOC_ZEROIZE);
   if (all_ifr == NULL) 
   {
      cmsLog_error("cmsImg_saveIfNameFromSocket: out of memory");
      close(fd);
      return CMSRET_INTERNAL_ERROR;
   }

   ifc.ifc_len = bufsize;
   ifc.ifc_buf = (char *)all_ifr;
   if (ioctl(fd, SIOCGIFCONF, &ifc) < 0) 
   {
      cmsLog_error("cmsImg_saveIfNameFromSocket: Error getting interfaces\n");
      close(fd);
      cmsMem_free(all_ifr);
      return CMSRET_INTERNAL_ERROR;
   }

   numifs = ifc.ifc_len/sizeof(struct ifreq);
   /* cmsLog_error("cmsImg_saveIfNameFromSocket: numifs=%d\n",numifs); */
   for (i = 0; i < numifs; i ++) 
   {
	  /* cmsLog_error("cmsImg_saveIfNameFromSocket: intface name=%s\n", all_ifr[i].ifr_name); */
	  struct in_addr addr1,addr2;
	  addr1 = ((struct sockaddr_in *)&(local_addr))->sin_addr;
	  addr2 = ((struct sockaddr_in *)&(all_ifr[i].ifr_addr))->sin_addr;
	  if (addr1.s_addr == addr2.s_addr) 
	  {
	      strcpy(connIfName, all_ifr[i].ifr_name);
	  	break;
	  }
   }

   close(fd);
   cmsMem_free(all_ifr);

   cmsLog_debug("connIfName=%s", connIfName);

   return CMSRET_SUCCESS;
   
}

