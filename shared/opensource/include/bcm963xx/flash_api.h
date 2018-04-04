/*
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
 * File Name  : flash_api.h
 *
 * Description: This file contains definitions and prototypes for a public
 *              flash device interface and an internal flash device interface.
 ***************************************************************************/

#if !defined(_FLASH_API_H)
#define _FLASH_API_H

#ifdef __cplusplus
extern "C" {
#endif

/* Flash definitions. */
#define FLASH_API_OK                1
#define FLASH_API_OK_BLANK          2
#define FLASH_API_ERROR             -1

#define FLASH_IFC_UNKNOWN           0
#define FLASH_IFC_PARALLEL          1
#define FLASH_IFC_SPI               2
#define FLASH_IFC_HS_SPI            3
#define FLASH_IFC_NAND              4

#define NAND_REINIT_FLASH           0xffff
#if 1 /* __MSTC__, zongyue: merge BRCM NAND bad block detect patch */
#define NAND_REINIT_FLASH_BAD   0xfffe
#endif

/* Public Interface Prototypes. */
int flash_init(void);
int flash_sector_erase_int(unsigned short sector);
int flash_read_buf(unsigned short sector, int offset, unsigned char *buffer,
    int numbytes);
int flash_write_buf(unsigned short sector, int offset, unsigned char *buffer,
    int numbytes);
int flash_get_numsectors(void);
int flash_get_sector_size(unsigned short sector);
unsigned char *flash_get_memptr(unsigned short sector);
int flash_get_blk(int addr);
int flash_get_total_size(void);
int flash_get_flash_type(void);
void flash_change_flash_type(int type);

/* Internal Flash Device Driver Information. */
typedef struct flash_device_info_s
{
    unsigned short flash_device_id;
    unsigned short flash_type;
    char flash_device_name[30];

    int (*fn_flash_sector_erase_int) (unsigned short sector);
    int (*fn_flash_read_buf) (unsigned short sector, int offset,
        unsigned char *buffer, int numbytes);
    int (*fn_flash_write_buf) (unsigned short sector, int offset,
        unsigned char *buffer, int numbytes);
    int (*fn_flash_get_numsectors) (void);
    int (*fn_flash_get_sector_size) (unsigned short sector);
    unsigned char * (*fn_flash_get_memptr) (unsigned short sector);
    int (*fn_flash_get_blk) (int addr);
    int (*fn_flash_get_total_size) (void);
} flash_device_info_t;

#ifdef __cplusplus
}
#endif

#endif /* _FLASH_API_H */

