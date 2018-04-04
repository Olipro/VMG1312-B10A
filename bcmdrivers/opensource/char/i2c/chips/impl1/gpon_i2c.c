/*
    gpon_i2c.c - I2C client driver for GPON transceiver
    Copyright (C) 2008 Broadcom Corp.

    06-18-2008  Pratapa Reddy, Vaka <pvaka@broadcom.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/slab.h>  /* kzalloc() */
#include <linux/types.h>
#include <linux/bcm_log.h>
#include "gpon_i2c.h"

#ifdef PROCFS_HOOKS
#include <asm/uaccess.h> /*copy_from_user*/
#include <linux/proc_fs.h>
#define PROC_DIR_NAME "i2c_gpon"
#define PROC_ENTRY_NAME1 "gponPhy_eeprom0"
#define PROC_ENTRY_NAME2 "gponPhy_eeprom1"
#ifdef GPON_I2C_TEST
#define PROC_ENTRY_NAME3 "gponPhyTest"
#endif
#endif

/* I2C client chip addresses */
/* Note that these addresses are 7-bit addresses without the LSB bit
   which indicates R/W operation */
#define GPON_PHY_I2C_ADDR1 0x50
#define GPON_PHY_I2C_ADDR2 0x51

/* Addresses to scan */
static unsigned short normal_i2c[] = {GPON_PHY_I2C_ADDR1, 
                                      GPON_PHY_I2C_ADDR2, I2C_CLIENT_END};

/* file system */
enum fs_enum {PROC_FS, SYS_FS};

/* Size of client in bytes */
#define DATA_SIZE             256
#define DWORD_ALIGN           4
#define WORD_ALIGN            2
#define MAX_TRANSACTION_SIZE  32

/* Client0 for Address 0x50 and Client1 for Address 0x51 */
#define client0               0
#define client1               1

/* Insmod parameters */
I2C_CLIENT_INSMOD_1(gponPhy);

/* Each client has this additional data */
struct gponPhy_data {
    struct i2c_client client;
};

/* Assumption: The i2c modules will be built-in to the kernel and will not be 
   unloaded; otherwise, it is possible for caller modules to call the exported
   functions even when the i2c modules are not loaded unless some registration
   mechanism is built-in to this module.  */
static struct gponPhy_data *pclient1_data; 
static struct gponPhy_data *pclient2_data; 

static int gponPhy_attach_adapter(struct i2c_adapter *adapter);
static int gponPhy_detect(struct i2c_adapter *adapter, int address, int kind);
static int gponPhy_detach_client(struct i2c_client *client);

/* Check if given offset is valid or not */
static inline int check_offset(u8 offset, u8 alignment)
{
    if (offset % alignment) {
        BCM_LOG_ERROR(BCM_LOG_ID_I2C, "Invalid offset %d. The offset should be"
                      " %d byte alligned \n", offset, alignment);
        return -1;
    }
    return 0;
}

/****************************************************************************/
/* generic_i2c_access: Provides a way to use BCM6816 algorithm driver to    */
/*  access any I2C device on the bus                                        */
/* Inputs: i2c_addr = 7-bit I2C address; offset = 8-bit offset; length =    */
/*  length (limited to 4); val = value to be written; set = indicates write */
/* Returns: None                                                            */
/****************************************************************************/
static void generic_i2c_access(u8 i2c_addr, u8 offset, u8 length, 
                               int val, u8 set)
{
    struct i2c_msg msg[2];
    char buf[5];
    int i;

    /* First write the offset  */
    msg[0].addr = msg[1].addr = i2c_addr;
    msg[0].flags = msg[1].flags = 0;

    /* if set = 1, do i2c write; otheriwse do i2c read */
    if (set) {
        msg[0].len = length + 1;
        buf[0] = offset;
        /* On the I2C bus, LS Byte should go first */
        val = swab32(val);
        memcpy(&buf[1], (char*)&val, length);
        msg[0].buf = buf;
        if(i2c_transfer(pclient1_data->client.adapter, msg, 1) != 1) {
            BCM_LOG_NOTICE(BCM_LOG_ID_I2C, "I2C Write Failed \n");
        }
    } else {
        /* write message */
        msg[0].len = 1;
        buf[0] = offset;
        msg[0].buf = buf;
        /* read message */
        msg[1].flags |= I2C_M_RD;
        msg[1].len = length;
        msg[1].buf = buf;

        /* On I2C bus, we receive LS byte first. So swap bytes as necessary */
        if(i2c_transfer(pclient1_data->client.adapter, msg, 2) == 2)
        {
            for (i=0; i < length; i++) {
                printk("0x%2x = 0x%2x \n", offset + i, buf[i] & 0xFF);
            }
            printk("\n");
        } else {
            BCM_LOG_NOTICE(BCM_LOG_ID_I2C, "I2C Read Failed \n");
        }
    }
}

/****************************************************************************/
/* Write gponPhy: Writes count number of bytes from buf on to the I2C bus   */
/* Returns:                                                                 */
/*   number of bytes written on success, negative value on failure.         */
/* Notes: 1. The LS byte should follow the offset                           */
/* Design Notes: The gponPhy takes the first byte after the chip address    */
/*  as offset. The BCM6816 can only send/receive upto 8 or 32 bytes         */
/*  depending on I2C_CTLHI_REG.DATA_REG_SIZE configuration in one           */
/*  transaction without using the I2C_IIC_ENABLE NO_STOP functionality.     */
/*  The 6816 algorithm driver currently splits a given transaction larger   */
/*  than DATA_REG_SIZE into multiple transactions. This function is         */   
/*  expected to be used very rarely and hence a simple approach is          */
/*  taken whereby this function limits the count to 32 (Note that the 6816  */
/*  I2C_CTLHI_REG.DATA_REG_SIZE is hard coded in 6816 algorithm driver for  */
/*  32B. This means, we can only write upto 31 bytes using this function.   */
/****************************************************************************/
ssize_t gponPhy_write(u8 client_num, char *buf, size_t count)
{
    struct i2c_client *client;
    BCM_LOG_DEBUG(BCM_LOG_ID_I2C, "Entering the function %s \n", __FUNCTION__);

    if(count > MAX_TRANSACTION_SIZE)
    {
        BCM_LOG_NOTICE(BCM_LOG_ID_I2C, "count > 32 is not yet supported \n");
        return -1;
    }
  
    if (client_num)
        client = &pclient2_data->client;
    else
        client = &pclient1_data->client;

    return i2c_master_send(client, buf, count);
}
EXPORT_SYMBOL(gponPhy_write);

/****************************************************************************/
/* Read BCM3450: Reads count number of bytes from BCM3450                   */
/* Returns:                                                                 */
/*   number of bytes read on success, negative value on failure.            */
/* Notes: 1. The offset should be provided in buf[0]                        */
/*        2. The count is limited to 32.                                    */
/*        3. The gponPhy with the serial EEPROM protocol requires the offset*/
/*        be written before reading the data on every I2C transaction       */
/****************************************************************************/
ssize_t gponPhy_read(u8 client_num, char *buf, size_t count)
{
    struct i2c_msg msg[2];
    struct i2c_client *client;
    BCM_LOG_DEBUG(BCM_LOG_ID_I2C, "Entering the function %s \n", __FUNCTION__);

    if(count > MAX_TRANSACTION_SIZE)
    {
        BCM_LOG_NOTICE(BCM_LOG_ID_I2C, "count > 32 is not yet supported \n");
        return -1;
    }

    if (client_num)
        client = &pclient2_data->client;
    else
        client = &pclient1_data->client;

    /* First write the offset  */
    msg[0].addr = msg[1].addr = client->addr;
    msg[0].flags = msg[1].flags = client->flags & I2C_M_TEN;

    msg[0].len = 1;
    msg[0].buf = buf;

    /* Now read the data */
    msg[1].flags |= I2C_M_RD;
    msg[1].len = count;
    msg[1].buf = buf;

    /* On I2C bus, we receive LS byte first. So swap bytes as necessary */
    if(i2c_transfer(client->adapter, msg, 2) == 2)
    {
        return count;
    }

    return -1;
}
EXPORT_SYMBOL(gponPhy_read);

/****************************************************************************/
/* Write Register: Writes the val into gponPhy register                     */
/* Returns:                                                                 */
/*   0 on success, negative value on failure.                               */
/* Notes: 1. The offset should be DWORD aligned                             */
/****************************************************************************/
int gponPhy_write_reg(u8 client_num, u8 offset, int val)
{
    char buf[5];
    struct i2c_client *client;

    BCM_LOG_DEBUG(BCM_LOG_ID_I2C, "Entering the function %s \n", __FUNCTION__);

    if(check_offset(offset, DWORD_ALIGN))
    {
        return -1;
    }

    if (client_num)
        client = &pclient2_data->client;
    else
        client = &pclient1_data->client;

    /* Set the buf[0] to be the offset for write operation */
    buf[0] = offset;

    /* On the I2C bus, LS Byte should go first */
    val = swab32(val);

    memcpy(&buf[1], (char*)&val, 4);
    if (i2c_master_send(client, buf, 5) == 5)
    {
        return 0;
    }
    return -1;
}
EXPORT_SYMBOL(gponPhy_write_reg);

/****************************************************************************/
/* Read Register: Read the gponPhy register                                 */
/* Returns:                                                                 */
/*   value on success, negative value on failure.                           */
/* Notes: 1. The offset should be DWORD aligned                             */
/****************************************************************************/
int gponPhy_read_reg(u8 client_num, u8 offset)
{
    struct i2c_msg msg[2];
    int val;
    struct i2c_client *client;

    BCM_LOG_DEBUG(BCM_LOG_ID_I2C, "Entering the function %s \n", __FUNCTION__);

    if(check_offset(offset, DWORD_ALIGN))
    {
        return -1;
    }

    if (client_num)
        client = &pclient2_data->client;
    else
        client = &pclient1_data->client;

    msg[0].addr = msg[1].addr = client->addr;
    msg[0].flags = msg[1].flags = client->flags & I2C_M_TEN;

    msg[0].len = 1;
    msg[0].buf = (char *)&offset;

    msg[1].flags |= I2C_M_RD;
    msg[1].len = 4;
    msg[1].buf = (char *)&val;

    /* On I2C bus, we receive LS byte first. So swap bytes as necessary */
    if(i2c_transfer(client->adapter, msg, 2) == 2)
    {
        return swab32(val);
    }

    return -1;
}
EXPORT_SYMBOL(gponPhy_read_reg);

/****************************************************************************/
/* Write Word: Writes the val into the word offset                          */ 
/* Returns:                                                                 */
/*   0 on success, negative value on failure.                               */
/* Notes: 1. The offset should be WORD aligned                              */
/****************************************************************************/
int gponPhy_write_word(u8 client_num, u8 offset, u16 val)
{
    char buf[3];
    struct i2c_client *client;

    BCM_LOG_DEBUG(BCM_LOG_ID_I2C, "Entering the function %s \n", __FUNCTION__);

    if(check_offset(offset, WORD_ALIGN))
    {
        return -1;
    }

    if (client_num)
        client = &pclient2_data->client;
    else
        client = &pclient1_data->client;

    /* The offset to be written should be the first byte in the I2C write */
    buf[0] = offset;
    buf[1] = (char)(val&0xFF);
    buf[2] = (char)(val>>8);
    if (i2c_master_send(client, buf, 3) == 3)
    {
        return 0;
    }
    return -1;
}
EXPORT_SYMBOL(gponPhy_write_word);

/****************************************************************************/
/* Read Word: Reads the LSB 2 bytes of Register                             */ 
/* Returns:                                                                 */
/*   value on success, negative value on failure.                           */
/* Notes: 1. The offset should be WORD aligned                              */
/****************************************************************************/
u16 gponPhy_read_word(u8 client_num, u8 offset)
{
    struct i2c_msg msg[2];
    u16 val;
    struct i2c_client *client;

    BCM_LOG_DEBUG(BCM_LOG_ID_I2C, "Entering the function %s \n", __FUNCTION__);

    if(check_offset(offset, WORD_ALIGN))
    {
        return -1;
    }

    if (client_num)
        client = &pclient2_data->client;
    else
        client = &pclient1_data->client;

    msg[0].addr = msg[1].addr = client->addr;
    msg[0].flags = msg[1].flags = client->flags & I2C_M_TEN;

    msg[0].len = 1;
    msg[0].buf = (char *)&offset;

    msg[1].flags |= I2C_M_RD;
    msg[1].len = 2;
    msg[1].buf = (char *)&val;

    if(i2c_transfer(client->adapter, msg, 2) == 2)
    {
        return swab16(val);
    }

    return -1;
}
EXPORT_SYMBOL(gponPhy_read_word);

/****************************************************************************/
/* Write Byte: Writes the val into LS Byte of Register                      */ 
/* Returns:                                                                 */
/*   0 on success, negative value on failure.                               */
/****************************************************************************/
int gponPhy_write_byte(u8 client_num, u8 offset, u8 val)
{
    char buf[2];
    struct i2c_client *client;

    BCM_LOG_DEBUG(BCM_LOG_ID_I2C, "Entering the function %s \n", __FUNCTION__);

    if (client_num)
        client = &pclient2_data->client;
    else
        client = &pclient1_data->client;

    /* BCM3450 requires the offset to be the register number */
    buf[0] = offset;
    buf[1] = val;
    if (i2c_master_send(client, buf, 2) == 2)
    {
        return 0;
    }
    return -1;
}
EXPORT_SYMBOL(gponPhy_write_byte);

/****************************************************************************/
/* Read Byte: Reads the LS Byte of Register                                 */ 
/* Returns:                                                                 */
/*   value on success, negative value on failure.                           */
/****************************************************************************/
u8 gponPhy_read_byte(u8 client_num, u8 offset)
{
    struct i2c_msg msg[2];
    char val;
    struct i2c_client *client;

    BCM_LOG_DEBUG(BCM_LOG_ID_I2C, "Entering the function %s \n", __FUNCTION__);

    if (client_num)
        client = &pclient2_data->client;
    else
        client = &pclient1_data->client;

    msg[0].addr = msg[1].addr = client->addr;
    msg[0].flags = msg[1].flags = client->flags & I2C_M_TEN;

    msg[0].len = 1;
    msg[0].buf = (char *)&offset;

    msg[1].flags |= I2C_M_RD;
    msg[1].len = 1;
    msg[1].buf = (char *)&val;

    if(i2c_transfer(client->adapter, msg, 2) == 2)
    {
        return val;
    }

    return -1;
}
EXPORT_SYMBOL(gponPhy_read_byte);

#if defined(SYSFS_HOOKS) || defined(PROCFS_HOOKS)
#ifdef GPON_I2C_TEST
static int client_num = 0;
/* Calls the appropriate function based on user command */
static int exec_command(const char *buf, size_t count, int fs_type)
{
#define MAX_ARGS 4
#define MAX_ARG_SIZE 32
    int i, argc = 0, val = 0;
    char cmd;
    u8 offset, i2c_addr, length, set = 0;
    char arg[MAX_ARGS][MAX_ARG_SIZE];
#if 0
    char temp_buf[20] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
#endif
#ifdef PROCFS_HOOKS
#define LOG_WR_KBUF_SIZE 128
    char kbuf[LOG_WR_KBUF_SIZE];

    if(fs_type == PROC_FS)
    {
        if ((count > LOG_WR_KBUF_SIZE-1) || 
            (copy_from_user(kbuf, buf, count) != 0))
            return -EFAULT;
        kbuf[count]=0;
        argc = sscanf(kbuf, "%c %s %s %s %s", &cmd, arg[0], arg[1], 
                      arg[2], arg[3]);
    }
#endif

#ifdef SYSFS_HOOKS
    if(fs_type == SYS_FS)
        argc = sscanf(buf, "%c %s %s %s %s", &cmd, arg[0], arg[1], 
                      arg[2], arg[3]);
#endif

    if (argc <= 1) {
        BCM_LOG_NOTICE(BCM_LOG_ID_I2C, "Need at-least 2 arguments \n");
        return -EFAULT;
    }

    for (i=0; i<MAX_ARGS; ++i) {
        arg[i][MAX_ARG_SIZE-1] = '\0';
    }

    offset = (u8) simple_strtoul(arg[0], NULL, 0);
    if (argc == 3)
        val = (int) simple_strtoul(arg[1], NULL, 0);

    switch (cmd) {
 
       case 'a':
        if (argc >= 4) {
            i2c_addr = (u8) simple_strtoul(arg[0], NULL, 0);
            offset = (u8) simple_strtoul(arg[1], NULL, 0);
            length = (u8) simple_strtoul(arg[2], NULL, 0);
            BCM_LOG_DEBUG(BCM_LOG_ID_I2C, "I2C Access: i2c_addr = 0x%x, offset"
                          " = 0x%x, len = %d \n", i2c_addr, offset, length);
            if (i2c_addr > 127 || length > 4) {
                BCM_LOG_NOTICE(BCM_LOG_ID_I2C, "Invalid I2C addr or len \n");
                return -EFAULT;
            }
            val = 0;
            if (argc > 4) {
                val = (int) simple_strtoul(arg[3], NULL, 0);
                set = 1;
            }
            generic_i2c_access(i2c_addr, offset, length, val, set);
        } else {
            BCM_LOG_NOTICE(BCM_LOG_ID_I2C, "Need at-least 3 arguments \n");
            return -EFAULT;
        }
        break;
    
#if 0
    case 'y':
        if (argc == 3) {
            if (val > 16) {
                BCM_LOG_INFO(BCM_LOG_ID_I2C, "Limiting byte count to 16 \n");
                val = 16;
            }
            BCM_LOG_DEBUG(BCM_LOG_ID_I2C, "Write Byte Stream: offset = 0x%x, " 
                          "count = 0x%x \n", offset, val);
            for (i=0; i< val; i++) {
                BCM_LOG_DEBUG(BCM_LOG_ID_I2C, "0x%x, ",temp_buf[i]);
            }
            temp_buf[0] = offset;
            gponPhy_write(client_num, temp_buf, val+1);
        }
        break;

    case 'z':
        if (argc == 3) {
            if (val > 16) {
                BCM_LOG_INFO(BCM_LOG_ID_I2C, "This test limits the byte"
                             "stream count to 16 \n");
                val = 16;
            }
            temp_buf[0] = offset;
            gponPhy_read(client_num, temp_buf, val);
            BCM_LOG_DEBUG(BCM_LOG_ID_I2C, "Read Byte Stream: offset =0x%x, " 
                          "count = 0x%x \n", offset, val);
            for (i=0; i< val; i++) {
                BCM_LOG_DEBUG(BCM_LOG_ID_I2C, "0x%x (%c)",(u8)temp_buf[i], 
                              (u8) temp_buf[i]);
            }
            BCM_LOG_DEBUG(BCM_LOG_ID_I2C, "\n");
        }
        break;
#endif

    case 'b':
        if (argc == 3) {
            BCM_LOG_DEBUG(BCM_LOG_ID_I2C, "Write Byte: offset = 0x%x, " 
                          "val = 0x%x \n", offset, val);
            if (gponPhy_write_byte(client_num, offset, (u8)val) < 0) {
                BCM_LOG_NOTICE(BCM_LOG_ID_I2C, "Write Failed \n"); 
            } else {
                BCM_LOG_NOTICE(BCM_LOG_ID_I2C, "Write Successful \n"); 
            }
        }
        else {
            if((val = gponPhy_read_byte(client_num, offset)) < 0) {
                BCM_LOG_NOTICE(BCM_LOG_ID_I2C, "Read Failed \n"); 
            } else {
                 BCM_LOG_NOTICE(BCM_LOG_ID_I2C, "Read Byte: offset = 0x%x, " 
                                "val = 0x%x \n", offset, val);
            }
        }
        break;

    case 'w':
        if (argc == 3) {
            BCM_LOG_DEBUG(BCM_LOG_ID_I2C, "Write Word: offset = 0x%x, " 
                              "val = 0x%x \n", offset, val);
            if (gponPhy_write_word(client_num, offset, (u16)val) < 0) {
                BCM_LOG_NOTICE(BCM_LOG_ID_I2C, "Write Failed \n"); 
            } else {
                BCM_LOG_NOTICE(BCM_LOG_ID_I2C, "Write Successful \n"); 
            }
        }
        else {
            if((val = gponPhy_read_word(client_num, offset)) < 0) {
                BCM_LOG_NOTICE(BCM_LOG_ID_I2C, "Read Failed \n"); 
            } else {
                BCM_LOG_NOTICE(BCM_LOG_ID_I2C, "Read Word: offset = 0x%x, " 
                               "val = 0x%x \n", offset, val);
            }
        }
        break;

    case 'd':    
        if (argc == 3) {
            BCM_LOG_DEBUG(BCM_LOG_ID_I2C, "Write Register: offset = 0x%x, " 
                          "val = 0x%x \n", offset, val);
            if (gponPhy_write_reg(client_num, offset, val) < 0) {
                BCM_LOG_NOTICE(BCM_LOG_ID_I2C, "Write Failed \n"); 
            } else {
                BCM_LOG_NOTICE(BCM_LOG_ID_I2C, "Write Successful \n"); 
            }
        }
        else {
            if((val = gponPhy_read_reg(client_num, offset)) < 0) {
                BCM_LOG_NOTICE(BCM_LOG_ID_I2C, "Read Failed \n"); 
            } else {
                BCM_LOG_NOTICE(BCM_LOG_ID_I2C, "Read Register: offset = 0x%x,"
                               " val = 0x%x \n", offset, val);
            }
        }
        break;

    case 'c':    
        if (offset == GPON_PHY_I2C_ADDR1)
            client_num = 0;
        else
            client_num = 1;
        break;

    default:
        BCM_LOG_NOTICE(BCM_LOG_ID_I2C, "Invalid command. \n Valid commands: \n" 
                       "  Change I2C Addr b/w 0x50 and 0x51: c addr \n" 
                       "  Write Reg:       d offset val \n" 
                       "  Read Reg:        d offset \n" 
                       "  Write Word:      w offset val \n" 
                       "  Read Word:       w offset \n" 
                       "  Write Byte:      b offset val \n" 
                       "  Read Byte:       b offset \n" 
                       "  Generic I2C access: a <i2c_addr(7-bit)>" 
                       " <offset> <length(1-4)> [value] \n" 
#if 0
                       "  Write Bytes:     y offset count \n" 
                       "  Read Bytes:      z offset count \n"
#endif
                       );
        break;
    }
    return count;
}
#endif
#endif

#ifdef PROCFS_HOOKS
#ifdef GPON_I2C_TEST
/* Read Function of PROCFS attribute "gponPhyTest" */
static ssize_t gponPhy_proc_test_read(struct file *f, char *buf, size_t count, 
                               loff_t *pos) 
{
    BCM_LOG_NOTICE(BCM_LOG_ID_I2C, " Usage: echo command > "
                   " /proc/i2c-gpon/gponPhyTest \n");
    BCM_LOG_NOTICE(BCM_LOG_ID_I2C, " supported commands: \n" 
                   "  Change I2C Addr b/w 0x50 and 0x51: c addr \n" 
                   "  Write Reg:       d offset val \n" 
                   "  Read Reg:        d offset \n" 
                   "  Write Word:      w offset val \n" 
                   "  Read Word:       w offset \n" 
                   "  Write Byte:      b offset val \n" 
                   "  Read Byte:       b offset \n" 
                   "  Generic I2C access: a <i2c_addr(7-bit)>" 
                   " <offset> <length(1-4)> [value] \n" 
                   );
    return 0;
}

/* Write Function of PROCFS attribute "gponPhyTest" */
static ssize_t gponPhy_proc_test_write(struct file *f, const char *buf, 
                                       size_t count, loff_t *pos)
{
    return exec_command(buf, count, PROC_FS);
}
#endif

#define GPON_PHY_EEPROM_SIZE  256
/* Read Function of PROCFS attribute "gponPhy_eepromX" */
static ssize_t gponPhy_proc_read(char *page, char **start, off_t off, int count,
                               int *eof, void *data) 
{
    int client_num = 0, max_offset, ret_val;
    struct gponPhy_data *pclient_data; 

    BCM_LOG_DEBUG(BCM_LOG_ID_I2C, "The offset is %d; the count is %d \n", 
                  (int)off, count);

    /* Verify that max_offset is below the max_eeprom_size (256 Bytes)*/
    max_offset = (int) (off + count);
    if (max_offset > GPON_PHY_EEPROM_SIZE) {
        BCM_LOG_NOTICE(BCM_LOG_ID_I2C, "offset + count must be less than "
                       "Max EEPROM Size of 256\n");
        return -1;
    }
 
    /* Set the page[0] to eeprom offset */
    page[0] = (u8)off;

    /* Select the eeprom of the 2 eeproms inside gponPhy */
    pclient_data = (struct gponPhy_data *)data;
    if (pclient_data->client.addr == GPON_PHY_I2C_ADDR2) {
        client_num = 1;
    }

    /*   See comments in the proc_file_read for info on 3 different
    *    ways of returning data. We are following below method.
    *    Set *start = an address within the buffer.
    *    Put the data of the requested offset at *start.
    *    Return the number of bytes of data placed there.
    *    If this number is greater than zero and you
    *    didn't signal eof and the reader is prepared to
    *    take more data you will be called again with the
    *    requested offset advanced by the number of bytes
    *    absorbed. */
    ret_val = gponPhy_read(client_num, page, count);
    *start = page;
    *eof = 1;

    return ret_val;
}

/* Write Function of PROCFS attribute "gponPhy_eepromX" */
static ssize_t gponPhy_proc_write(struct file *file, const char __user *buffer, 
                                  unsigned long count, void *data) 
{
    int client_num = 0, max_offset, offset = (int)file->f_pos;
    struct gponPhy_data *pclient_data; 
    char kbuf[MAX_TRANSACTION_SIZE];

    BCM_LOG_DEBUG(BCM_LOG_ID_I2C, "The offset is %d; the count is %ld \n", 
                  offset, count);

    /* Verify that count is less than 31 bytes */
    if ((count+1) > MAX_TRANSACTION_SIZE)
    {
        BCM_LOG_NOTICE(BCM_LOG_ID_I2C, "Writing more than 31 Bytes is not"
                       "yet supported \n");
        return -1;
    }
 
    /* Verify that max_offset is below the max_eeprom_size (256 Bytes)*/
    max_offset = (int) (offset + count);
    if (max_offset > GPON_PHY_EEPROM_SIZE)
    {
        BCM_LOG_NOTICE(BCM_LOG_ID_I2C, "offset + count must be less than "
                       "Max EEPROM Size of 256\n");
        return -1;
    }  
   
    /* Select the eeprom of the 2 eeproms inside gponPhy */
    pclient_data = (struct gponPhy_data *)data;
    if (pclient_data->client.addr == GPON_PHY_I2C_ADDR2)
        client_num = 1;

    kbuf[0] = (u8)offset;
    copy_from_user(&kbuf[1], buffer, count);
    /* Return the number of bytes written (exclude the address byte added
       at kbuf[0] */
    return (gponPhy_write(client_num, kbuf, count+1) - 1);
}
#endif

#ifdef SYSFS_HOOKS
/* Read Function of SYSFS attribute */
static ssize_t gponPhy_sys_read(struct device *dev, struct device_attribute *attr, 
                          char *buf)
{
    return snprintf(buf, PAGE_SIZE, "The gponPhy access read attribute \n");
}

/* Write Function of SYSFS attribute */
static ssize_t gponPhy_sys_write(struct device *dev, struct device_attribute *attr, 
                           const char *buf, size_t count)
{
    return exec_command(buf, count, SYS_FS);
}

static DEVICE_ATTR(gponPhy_access, S_IRWXUGO, gponPhy_sys_read, gponPhy_sys_write);

static struct attribute *gponPhy_attributes[] = {
    &dev_attr_gponPhy_access.attr,
    NULL
};

static const struct attribute_group gponPhy_attr_group = {
    .attrs = gponPhy_attributes,
};
#endif

#ifdef PROCFS_HOOKS
#ifdef GPON_I2C_TEST
static struct file_operations gponPhyTest_fops = {
    read: gponPhy_proc_test_read,
    write: gponPhy_proc_test_write
};
#endif
#endif

/* This is the driver that will be inserted */
static struct i2c_driver gponPhy_driver = {
    .driver = {
        .name    = "gpon_i2c",
    },
    .attach_adapter    = gponPhy_attach_adapter,
    .detach_client    = gponPhy_detach_client,
};

static int gponPhy_attach_adapter(struct i2c_adapter *adapter)
{
    BCM_LOG_DEBUG(BCM_LOG_ID_I2C, "Entering the function %s \n", __FUNCTION__);
    return i2c_probe(adapter, &addr_data, gponPhy_detect);
}

#ifdef PROCFS_HOOKS
static struct proc_dir_entry *q=NULL;
#endif
/* This function is called by i2c_probe for each I2C address*/
static int gponPhy_detect(struct i2c_adapter *adapter, int address, int kind)
{
    struct i2c_client *client;
    struct gponPhy_data *pclient_data; 
    int err = 0;
#ifdef PROCFS_HOOKS
    struct proc_dir_entry *p;
#endif

    BCM_LOG_DEBUG(BCM_LOG_ID_I2C, "Entering the function %s \n", __FUNCTION__);

    if (!i2c_check_functionality(adapter, I2C_FUNC_I2C))
        goto exit;

    if (!(pclient_data = kzalloc(sizeof(struct gponPhy_data), GFP_KERNEL))) 
    {
        err = -ENOMEM;
        goto exit;
    }

    /* Setup the i2c client data */
    client = &pclient_data->client;
    i2c_set_clientdata(client, pclient_data);
    client->addr = address;
    client->adapter = adapter;
    client->driver = &gponPhy_driver;
    client->flags = 0;

    /* Tell the I2C layer a new client has arrived */
    if ((err = i2c_attach_client(client)))
        goto exit_kfree;

    if (address == GPON_PHY_I2C_ADDR1)
    {
        pclient1_data = pclient_data;
    }
    else
    {
        pclient2_data = pclient_data;
    }

#ifdef SYSFS_HOOKS
    /* Register sysfs hooks */
    err = sysfs_create_group(&client->dev.kobj, &gponPhy_attr_group);
    if (err)
        goto exit_detach;
#endif

#ifdef PROCFS_HOOKS
    if (address == GPON_PHY_I2C_ADDR1)
    {
        q = proc_mkdir(PROC_DIR_NAME, NULL);
        if (!q) {
            BCM_LOG_ERROR(BCM_LOG_ID_I2C, "bcmlog: unable to create proc entry\n");
            err = -ENOMEM;
#ifdef SYSFS_HOOKS
            sysfs_remove_group(&client->dev.kobj, &gponPhy_attr_group);
#endif
            goto exit_detach;
        }
    }

    if (address == GPON_PHY_I2C_ADDR1)
        p = create_proc_entry(PROC_ENTRY_NAME1, 0, q);
    else
        p = create_proc_entry(PROC_ENTRY_NAME2, 0, q);

    if (!p) {
        BCM_LOG_ERROR(BCM_LOG_ID_I2C, "bcmlog: unable to create proc entry\n");
        err = -EIO;
#ifdef SYSFS_HOOKS
        sysfs_remove_group(&client->dev.kobj, &gponPhy_attr_group);
#endif
        goto exit_detach;
    }
    p->read_proc = gponPhy_proc_read;
    p->write_proc = gponPhy_proc_write;
    p->data = (void *)pclient_data;

#ifdef GPON_I2C_TEST
    /* Create only once */
    if (address == GPON_PHY_I2C_ADDR1)
    {
        p = create_proc_entry(PROC_ENTRY_NAME3, 0, q);
        if (p) {
            p->proc_fops = &gponPhyTest_fops;
        }
    }
#endif
#endif

    return 0;

#if defined(SYSFS_HOOKS) || defined(PROCFS_HOOKS)
exit_detach:
    i2c_detach_client(client);
#endif
exit_kfree:
    kfree(pclient_data);
exit:
    return err;
}

static int gponPhy_detach_client(struct i2c_client *client)
{
    int err;

    BCM_LOG_DEBUG(BCM_LOG_ID_I2C, "Entering the function %s \n", __FUNCTION__);

#ifdef SYSFS_HOOKS
    sysfs_remove_group(&client->dev.kobj, &gponPhy_attr_group);
#endif

#ifdef PROCFS_HOOKS
        remove_proc_entry(PROC_ENTRY_NAME1, q);
        remove_proc_entry(PROC_ENTRY_NAME2, q);
#ifdef GPON_I2C_TEST
        remove_proc_entry(PROC_ENTRY_NAME3, q);
#endif
        remove_proc_entry(PROC_DIR_NAME, NULL);
#endif

    err = i2c_detach_client(client);
    if (err)
        return err;

    kfree(i2c_get_clientdata(client));

    return 0;
}

static int __init gponPhy_init(void)
{
    return i2c_add_driver(&gponPhy_driver);
}

static void __exit gponPhy_exit(void)
{
    i2c_del_driver(&gponPhy_driver);
}

MODULE_AUTHOR("Pratapa Reddy, Vaka <pvaka@broadcom.com>");
MODULE_DESCRIPTION("GPON OLT Transceiver I2C driver");
MODULE_LICENSE("GPL");

module_init(gponPhy_init);
module_exit(gponPhy_exit);
