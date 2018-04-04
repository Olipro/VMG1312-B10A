/*
<:copyright-broadcom 
 
 Copyright (c) 2008 Broadcom Corporation 

 All Rights Reserved 
 No portions of this material may be reproduced in any form without the 
 written permission of: 
          Broadcom Corporation 
          5300 California Avenue, 
          Irvine, California 92618 
 All information contained in this document is Broadcom Corporation 
 company private, proprietary, and trade secret. 
 
:>
*/
/***********************************************************************/
/*                                                                     */
/*   MODULE   bcm3450.h                                                */
/*   DATE:    06/18/08                                                 */
/*   PURPOSE: BCM3450 reg access API                                   */
/*                                                                     */
/***********************************************************************/
#ifndef __BCM3450_H
#define __BCM3450_H

/****************************************************************************/
/* Write BCM3450: Writes count number of bytes from buf on to the I2C bus   */
/* Returns:                                                                 */
/*   number of bytes written on success, negative value on failure.         */
/* Notes: 1. The buf[0] should be a DWORD aligned offset where write starts */
/*        2. The count > 8 is not yet supported                             */
/****************************************************************************/
ssize_t bcm3450_write(char *buf, size_t count);

/****************************************************************************/
/* Read BCM3450: Reads count number of bytes from BCM3450                   */
/* Returns:                                                                 */
/*   number of bytes read on success, negative value on failure.            */
/* Notes: 1. The buf[0] should be a DWORD aligned offset where read starts  */
/*        2. The count > 8 is not yet supported                             */
/****************************************************************************/
ssize_t bcm3450_read(char *buf, size_t count);

/****************************************************************************/
/* Write Register: Writes the val into BCM3450 register                     */
/* Returns:                                                                 */
/*   0 on success, negative value on failure.                               */
/* Notes: 1. The offset should be DWORD aligned                             */
/****************************************************************************/
int bcm3450_write_reg(u8 offset, int val);

/****************************************************************************/
/* Read Register: Read the BCM3450 register at given offset                 */
/* Returns:                                                                 */
/*   value on success, negative value on failure.                           */
/* Notes: 1. The offset should be DWORD aligned                             */
/****************************************************************************/
int bcm3450_read_reg(u8 offset);

/****************************************************************************/
/* Write Word: Writes the val into given word offset                        */ 
/* Returns:                                                                 */
/*   0 on success, negative value on failure.                               */
/* Notes: 1. The offset should be WORD aligned                              */
/****************************************************************************/
int bcm3450_write_word(u8 offset, u16 val);

/****************************************************************************/
/* Read Word: Reads the value from given WORD offset                        */ 
/* Returns:                                                                 */
/*   value on success, negative value on failure.                           */
/* Notes: 1. The offset should be WORD aligned                              */
/****************************************************************************/
u16 bcm3450_read_word(u8 offset);

/****************************************************************************/
/* Write Byte: Writes the val into given Byte offset                        */ 
/* Returns:                                                                 */
/*   0 on success, negative value on failure.                               */
/****************************************************************************/
int bcm3450_write_byte(u8 offset, u8 val);

/****************************************************************************/
/* Read Byte: Reads the value from given Byte offset                        */ 
/* Returns:                                                                 */
/*   value on success, negative value on failure.                           */
/****************************************************************************/
u8 bcm3450_read_byte(u8 offset);

#endif
