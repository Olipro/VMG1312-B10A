/*
    laser_i2c.c - Generic Laser I2C client driver
    Copyright (C) 2011 Broadcom Corp.

    04-18-2011  Tim Sharp <tsharp@broadcom.com>

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
#include <boardparms.h>

/* I2C client chip addresses */
#define LASER_I2C_ADDR1 0x4e    // address of mindspeed chip with BOSA optics

/* Addresses to scan */
static unsigned short normal_i2c[] = {LASER_I2C_ADDR1, LASER_I2C_ADDR1};



/* Size of client in bytes */
#define DATA_SIZE		      256
#define DWORD_ALIGN	          4
#define WORD_ALIGN            2
#define MAX_TRANSACTION_SIZE  32
#define MAX_REG_OFFSET        150

/* Insmod parameters */
I2C_CLIENT_INSMOD_1(laser_i2c);

/* Each client has this additional data */
struct laser_i2c_data {
    struct i2c_client client;
};

static struct laser_i2c_data *pclient_data; 

static int laser_i2c_attach_adapter(struct i2c_adapter *adapter);
static int laser_i2c_detect(struct i2c_adapter *adapter, int address, int kind);
static int laser_i2c_detach_client(struct i2c_client *client);

/* Check if given 3450 register offset is valid or not */
static inline int check_offset(u8 offset, u8 alignment)
{
    if (offset > MAX_REG_OFFSET) 
    {
        BCM_LOG_NOTICE(BCM_LOG_ID_I2C, "\nInvalid offset. It should be less than "
                      "%X \n", MAX_REG_OFFSET);
        return -EINVAL;
    }
    else if (offset % alignment) 
    {
        BCM_LOG_NOTICE(BCM_LOG_ID_I2C, "\nInvalid offset %d. The offset should be "
                      "%d byte alligned \n", offset, alignment);
        return -EINVAL;
    }
    return 0;
}

/****************************************************************************/
/* laser_i2c_write:                                                         */
/*      Writes count number of bytes from buf on to the I2C bus             */
/* Returns:                                                                 */
/*      number of bytes written on success, negative value on failure.      */
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
ssize_t laser_i2c_write( char *buf, size_t count)
{
    struct i2c_client *client = &pclient_data->client;

    BCM_LOG_DEBUG(BCM_LOG_ID_I2C, "Entering the function %s \n", __FUNCTION__);

    if(count > MAX_TRANSACTION_SIZE)
    {
        BCM_LOG_NOTICE(BCM_LOG_ID_I2C, "count > 32 is not yet supported \n");
        return -1;
    }
  
    return i2c_master_send(client, buf, count);
}
EXPORT_SYMBOL(laser_i2c_write);

/****************************************************************************/
/* laser_i2c_read: Reads count number of bytes from mindspeed 2098 BOSA optics     */
/* Returns:                                                                 */
/*   number of bytes read on success, negative value on failure.            */
/* Notes: 1. The offset should be provided in buf[0]                        */
/*        2. The count is limited to 32.                                    */
/*        3. The gponPhy with the serial EEPROM protocol requires the offset*/
/*        be written before reading the data on every I2C transaction       */
/****************************************************************************/
ssize_t laser_i2c_read( char *buf, size_t count)
{
    struct i2c_msg msg[2];
    struct i2c_client *client = &pclient_data->client;

    BCM_LOG_DEBUG(BCM_LOG_ID_I2C, "Entering the function %s \n", __FUNCTION__);

    if(count > MAX_TRANSACTION_SIZE)
    {
        BCM_LOG_NOTICE(BCM_LOG_ID_I2C, "count > 32 is not yet supported \n");
        return -1;
    }


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

EXPORT_SYMBOL(laser_i2c_read);

#if 0
/****************************************************************************/
/* Write laser optics: Writes count number of bytes from buf on to the I2C bus   */
/* Returns:                                                                 */
/*   number of bytes written on success, negative value on failure.         */
/* Notes: 1. The buf[0] should be a DWORD aligned offset where write starts */
/*        2. The LS byte should follow the offset                           */
/* Design Notes: The BCM3450 takes the first byte after the chip address    */
/*  as offset. The BCM6816 can only send/receive upto 8 or 32 bytes         */
/*  depending on I2C_CTLHI_REG.DATA_REG_SIZE configuration in one           */
/*  transaction without using the I2C_IIC_ENABLE NO_STOP functionality.     */
/*  The 6816 algorithm driver currently splits a given transaction larger   */
/*  than DATA_REG_SIZE into multiple transactions. This function is not     */   
/*  expected to be used very rarely and hence a simple approach is          */
/*  taken whereby this function limits the count to 8. This means, we can   */
/*  only write upto 7 bytes using this function.                            */
/****************************************************************************/
ssize_t laser_i2c_write(char *buf, size_t count)
{ 
    struct i2c_client *client = &pclient_data->client;
    BCM_LOG_DEBUG(BCM_LOG_ID_I2C, "Entering the function %s \n", __FUNCTION__);

    if(check_offset(buf[0], DWORD_ALIGN))
        return -1;

    if(count > MAX_TRANSACTION_SIZE)
    {
    	BCM_LOG_NOTICE(BCM_LOG_ID_I2C, "count > %d is not yet supported \n", 
                       MAX_TRANSACTION_SIZE);
        return -1;
    }
    
    return i2c_master_send(client, buf, count);
}
EXPORT_SYMBOL(laser_i2c_write);

/****************************************************************************/
/* Read laser optics: Reads count number of bytes from laser optics         */
/* Returns:                                                                 */
/*   number of bytes read on success, negative value on failure.            */
/* Notes: 1. The buf[0] should be a DWORD aligned offset where read starts  */
/*        2. Limits the count to 8. See the notes of laser_i2c_write function */
/****************************************************************************/
ssize_t laser_i2c_read(char *buf, size_t count)
{
    struct i2c_msg msg[2];
    struct i2c_client *client = &pclient_data->client;
    BCM_LOG_DEBUG(BCM_LOG_ID_I2C, "Entering the function %s \n", __FUNCTION__);

    if(check_offset(buf[0], DWORD_ALIGN))
        return -1;

    if(count > MAX_TRANSACTION_SIZE)
    {
    	BCM_LOG_NOTICE(BCM_LOG_ID_I2C, "count > %d is not yet supported \n", 
                       MAX_TRANSACTION_SIZE);
        return -1;
    }
 
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
EXPORT_SYMBOL(laser_i2c_read);
#endif

/****************************************************************************/
/* Write Register: Writes the val into laser optics register                */
/* Returns:                                                                 */
/*   0 on success, negative value on failure.                               */
/* Notes: 1. The offset should be DWORD aligned                             */
/****************************************************************************/
int laser_i2c_write_reg(u8 offset, int val)
{
    char buf[5];
    struct i2c_client *client = &pclient_data->client;

    BCM_LOG_DEBUG(BCM_LOG_ID_I2C, "Entering the function %s \n", __FUNCTION__);

    if(check_offset(offset, DWORD_ALIGN)) {
        return -EINVAL;
    }

    /* On the I2C bus, LS Byte should go first */
    val = swab32(val);

    memcpy(&buf[1], (char*)&val, 4);
    if (i2c_master_send(client, buf, 5) == 5)
	{
        return 0;
	}
    return -1;
}
EXPORT_SYMBOL(laser_i2c_write_reg);

/****************************************************************************/
/* Read Register: Read the laser optics register at given offset            */
/* Returns:                                                                 */
/*   value on success, negative value on failure.                           */
/* Notes: 1. The offset should be DWORD aligned                             */
/****************************************************************************/
int laser_i2c_read_reg(u8 offset)
{
    struct i2c_msg msg[2];
    int val;
    struct i2c_client *client = &pclient_data->client;

    BCM_LOG_DEBUG(BCM_LOG_ID_I2C, "Entering the function %s \n", __FUNCTION__);

    if(check_offset(offset, DWORD_ALIGN)) {
        return -EINVAL;
    }

    /* BCM3450 requires the offset to be the register number */
    offset = offset/4;

    msg[0].addr = msg[1].addr = client->addr;
    msg[0].flags = msg[1].flags = client->flags & I2C_M_TEN;

    msg[0].len = 1;
    msg[0].buf = (char *)&offset;

    msg[1].flags |= I2C_M_RD;
    msg[1].len = 4;
    msg[1].buf = (char *)&val;

    /* On I2C bus, we receive LS byte first. So swap bytes as necessary */
    if(i2c_transfer(client->adapter, msg, 2) == 2)
        return swab32(val);

    return -1;
}
EXPORT_SYMBOL(laser_i2c_read_reg);

/****************************************************************************/
/* Write Word: Writes the val into LSB 2 bytes of Register                  */ 
/* Returns:                                                                 */
/*   0 on success, negative value on failure.                               */
/* Notes: 1. The offset should be WORD aligned                              */
/*    2. ReadModifyWrite is required because the 3450 requires the register */ 
/* number and not byte offset.                                              */
/****************************************************************************/
int laser_i2c_write_word(u8 offset, u16 val)
{
    char buf[3];
    struct i2c_client *client = &pclient_data->client;
    
    BCM_LOG_DEBUG(BCM_LOG_ID_I2C, "Entering the function %s \n", __FUNCTION__);
    
    if(check_offset(offset, WORD_ALIGN))
    {
        return -1;
    }
    
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
EXPORT_SYMBOL(laser_i2c_write_word);

/****************************************************************************/
/* Read Word: Reads the LSB 2 bytes of Register                             */ 
/* Returns:                                                                 */
/*   value on success, negative value on failure.                           */
/* Notes: 1. The offset should be WORD aligned                              */
/****************************************************************************/
u16 laser_i2c_read_word(u8 offset)
{
    struct i2c_msg msg[2];
    u16 val;
    struct i2c_client *client = &pclient_data->client;

    BCM_LOG_DEBUG(BCM_LOG_ID_I2C, "Entering the function %s \n", __FUNCTION__);

    if(check_offset(offset, WORD_ALIGN))
    {
        return -1;
    }

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
EXPORT_SYMBOL(laser_i2c_read_word);


/****************************************************************************/
/* Write Byte: Writes the val into LS Byte of Register                      */ 
/* Returns:                                                                 */
/*   0 on success, negative value on failure.                               */
/* Note: ReadModifyWrite is required because the 3450 requires the register */ 
/* number and not byte offset.                                              */
/****************************************************************************/
int laser_i2c_write_byte(u8 offset, u8 val)
{

    char buf[2];    
    struct i2c_client *client = &pclient_data->client;


    BCM_LOG_DEBUG(BCM_LOG_ID_I2C, "\nEntering the function %s \n", __FUNCTION__);

    buf[0] = offset;
    buf[1] = val;
    
    if (i2c_master_send(client, buf, 2) == 2)     
    {        
        return 0;    
    }    
    
    return -1;

}
EXPORT_SYMBOL(laser_i2c_write_byte);

/****************************************************************************/
/* Read Byte: Reads the LS Byte of Register                                 */ 
/* Returns:                                                                 */
/*   value on success, negative value on failure.                           */
/****************************************************************************/
u8 laser_i2c_read_byte(u8 offset)
{
    struct i2c_msg msg[2];
    char val;
    struct i2c_client *client = &pclient_data->client;

    BCM_LOG_DEBUG(BCM_LOG_ID_I2C, "Entering the function %s \n", __FUNCTION__);

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
EXPORT_SYMBOL(laser_i2c_read_byte);


/* This is the driver that will be inserted */
static struct i2c_driver laser_i2c_driver = {
    .driver = {
        .name	= "laser i2c",
    },
    .attach_adapter	= laser_i2c_attach_adapter,
    .detach_client	= laser_i2c_detach_client,
};

static int laser_i2c_attach_adapter(struct i2c_adapter *adapter)
{

    int ret;

    BCM_LOG_DEBUG(BCM_LOG_ID_I2C, "\nEntering the function %s \n", __FUNCTION__);

    ret = i2c_probe(adapter, &addr_data, laser_i2c_detect);       

    return ret;
}

/* This function is called by i2c_probe */
static int laser_i2c_detect(struct i2c_adapter *adapter, int address, int kind)
{
    struct i2c_client *client;
    int err = 0;

    BCM_LOG_DEBUG(BCM_LOG_ID_I2C, "\nEntering the function %s \n", __FUNCTION__);

    if (!i2c_check_functionality(adapter, I2C_FUNC_I2C))
        goto exit;

    if (!(pclient_data = kzalloc(sizeof(struct laser_i2c_data), GFP_KERNEL))) 
    {
        err = -ENOMEM;
        goto exit;
    }

    /* Setup the i2c client data */
    client = &pclient_data->client;
    i2c_set_clientdata(client, pclient_data);
    client->addr = address;
    client->adapter = adapter;
    client->driver = &laser_i2c_driver;
    client->flags = 0;
    strlcpy(client->name, "laser i2c", I2C_NAME_SIZE);

    /* Tell the I2C layer a new client has arrived */
    if ((err = i2c_attach_client(client)))
        goto exit_kfree;

    return 0;

exit_kfree:
    kfree(pclient_data);
exit:
    return err;
}

static int laser_i2c_detach_client(struct i2c_client *client)
{
    int err;

    BCM_LOG_DEBUG(BCM_LOG_ID_I2C, "\nEntering the function %s \n", __FUNCTION__);

    err = i2c_detach_client(client);
    if (err)
        return err;

    kfree(i2c_get_clientdata(client));

    return 0;
}

static int __init laser_i2c_init(void)
{
    return i2c_add_driver(&laser_i2c_driver);
}

static void __exit laser_i2c_exit(void)
{
    i2c_del_driver(&laser_i2c_driver);
}

MODULE_AUTHOR("Tim Sharp <tsharp@broadcom.com>");
MODULE_DESCRIPTION("LASER I2C driver");
MODULE_LICENSE("GPL");

module_init(laser_i2c_init);
module_exit(laser_i2c_exit);
