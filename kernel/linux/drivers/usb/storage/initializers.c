/* Special Initializers for certain USB Mass Storage devices
 *
 * Current development and maintenance by:
 *   (c) 1999, 2000 Matthew Dharm (mdharm-usb@one-eyed-alien.net)
 *
 * This driver is based on the 'USB Mass Storage Class' document. This
 * describes in detail the protocol used to communicate with such
 * devices.  Clearly, the designers had SCSI and ATAPI commands in
 * mind when they created this document.  The commands are all very
 * similar to commands in the SCSI-II and ATAPI specifications.
 *
 * It is important to note that in a number of cases this class
 * exhibits class-specific exemptions from the USB specification.
 * Notably the usage of NAK, STALL and ACK differs from the norm, in
 * that they are used to communicate wait, failed and OK on commands.
 *
 * Also, for certain devices, the interrupt endpoint is used to convey
 * status of a command.
 *
 * Please see http://www.one-eyed-alien.net/~mdharm/linux-usb for more
 * information about this driver.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/errno.h>

#include "usb.h"
#include "initializers.h"
#include "debug.h"
#include "transport.h"

/* This places the Shuttle/SCM USB<->SCSI bridge devices in multi-target
 * mode */
int usb_stor_euscsi_init(struct us_data *us)
{
	int result;

	US_DEBUGP("Attempting to init eUSCSI bridge...\n");
	us->iobuf[0] = 0x1;
	result = usb_stor_control_msg(us, us->send_ctrl_pipe,
			0x0C, USB_RECIP_INTERFACE | USB_TYPE_VENDOR,
			0x01, 0x0, us->iobuf, 0x1, 5*HZ);
	US_DEBUGP("-- result is %d\n", result);

	return 0;
}

/* This function is required to activate all four slots on the UCR-61S2B
 * flash reader */
int usb_stor_ucr61s2b_init(struct us_data *us)
{
	struct bulk_cb_wrap *bcb = (struct bulk_cb_wrap*) us->iobuf;
	struct bulk_cs_wrap *bcs = (struct bulk_cs_wrap*) us->iobuf;
	int res;
	unsigned int partial;
	static char init_string[] = "\xec\x0a\x06\x00$PCCHIPS";

	US_DEBUGP("Sending UCR-61S2B initialization packet...\n");

	bcb->Signature = cpu_to_le32(US_BULK_CB_SIGN);
	bcb->Tag = 0;
	bcb->DataTransferLength = cpu_to_le32(0);
	bcb->Flags = bcb->Lun = 0;
	bcb->Length = sizeof(init_string) - 1;
	memset(bcb->CDB, 0, sizeof(bcb->CDB));
	memcpy(bcb->CDB, init_string, sizeof(init_string) - 1);

	res = usb_stor_bulk_transfer_buf(us, us->send_bulk_pipe, bcb,
			US_BULK_CB_WRAP_LEN, &partial);
	if(res)
		return res;

	US_DEBUGP("Getting status packet...\n");
	res = usb_stor_bulk_transfer_buf(us, us->recv_bulk_pipe, bcs,
			US_BULK_CS_WRAP_LEN, &partial);

	return (res ? -1 : 0);
}

/* This places the HUAWEI E220 devices in multi-port mode */
int usb_stor_huawei_e220_init(struct us_data *us)
{
	int result;

	result = usb_stor_control_msg(us, us->send_ctrl_pipe,
				      USB_REQ_SET_FEATURE,
				      USB_TYPE_STANDARD | USB_RECIP_DEVICE,
				      0x01, 0x0, NULL, 0x0, 1000);
	US_DEBUGP("usb_control_msg performing result is %d\n", result);
	return (result ? 0 : -1);
}

/* This places the HUAWEI E1750/E1752/E169u/E161 devices in multi-port mode */
int usb_stor_huawei_e1750_init(struct us_data *us)
{
	const unsigned char rezero_msg[] = {
	  0x55, 0x53, 0x42, 0x43, 0x12, 0x34, 0x56, 0x78,
	  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11,
	  0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};

	char *buffer;
	int result;

	buffer = kzalloc(US_BULK_CB_WRAP_LEN, GFP_KERNEL);
	if (buffer == NULL)
		return USB_STOR_TRANSPORT_ERROR;

	memcpy(buffer, rezero_msg, sizeof (rezero_msg));
	result = usb_stor_bulk_transfer_buf(us,
			us->send_bulk_pipe,
			buffer, sizeof (rezero_msg), NULL);
	if (result != USB_STOR_XFER_GOOD) {
		result = USB_STOR_XFER_ERROR;
		goto out;
	}

	/* Some of the devices need to be asked for a response, but we don't
	 * care what that response is.
	 */
	result = usb_stor_bulk_transfer_buf(us,
			us->recv_bulk_pipe,
			buffer, US_BULK_CS_WRAP_LEN, NULL);
	result = USB_STOR_XFER_GOOD;

out:
	kfree(buffer);
	return result;
}


#if 1 // __MSTC__, Richard Huang, For Telefonica 3G WAN backup, __TELEFONICA__, MitraStar Chehuai, 20110622
/* This places the HUAWEI E169u devices in multi-port mode */
int usb_stor_huawei_e169u_init(struct us_data *us)
{
	int result = 0;
	static char init_string[] = 
		"\x55\x53\x42\x43\x00\x00\x00\x00"
		"\x00\x00\x00\x00\x00\x00\x00\x11"
		"\x06\x00\x00\x00\x00\x00\x00\x00"
		"\x00\x00\x00\x00\x00\x00\x00";

 	printk("%s\n", __FUNCTION__);
 	memcpy(us->iobuf, init_string, sizeof(init_string) - 1);
 	result = usb_stor_bulk_transfer_buf(us, us->send_bulk_pipe,
				us->iobuf,sizeof(init_string) - 1,NULL);
 	US_DEBUGP("usb_stor_bulk_transfer_buf result is %d\n", result); 
 	return (result ? -1 : 0); 
}
int usb_stor_qisda_h21_init(struct us_data *us)
{
	int result;
	struct
	{
		unsigned char header[4];
		unsigned char data[12];
	} h21_Switch_Mode_Req;

	h21_Switch_Mode_Req.data[5]=0x01;  
	result = usb_stor_control_msg(us, us->send_ctrl_pipe,
                                0x04,0 | (0x02 << 5) | 0x00,
                                0x00,0x00,(char*)&h21_Switch_Mode_Req, 
                                sizeof(h21_Switch_Mode_Req),1000);

	US_DEBUGP("usb_control_msg performing result is %d\n", result);
	return (result ? 0 : -1);
}

int usb_stor_huawei_e173_init(struct us_data *us){

	const unsigned char rezero_msg[] = {
 	 0x55, 0x53, 0x42, 0x43, 0x12, 0x34, 0x56, 0x78,
 	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11,
 	 0x06, 0x20, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
 	 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};

	char *buffer;
	int result;

	buffer = kzalloc(US_BULK_CB_WRAP_LEN, GFP_KERNEL);
	if (buffer == NULL)
		return USB_STOR_TRANSPORT_ERROR;

	memcpy(buffer, rezero_msg, sizeof (rezero_msg));
	result = usb_stor_bulk_transfer_buf(us,
			us->send_bulk_pipe,
			buffer, sizeof (rezero_msg), NULL);
	if (result != USB_STOR_XFER_GOOD) {
		result = USB_STOR_XFER_ERROR;
		goto out;
	}

	/* Some of the devices need to be asked for a response, but we don't
	 * care what that response is.
	 */
	result = usb_stor_bulk_transfer_buf(us,
			us->recv_bulk_pipe,
			buffer, US_BULK_CS_WRAP_LEN, NULL);
	result = USB_STOR_XFER_GOOD;

out:
	kfree(buffer);
	return result;
}

int usb_stor_zte_mf100_init(struct us_data *us)
{
	int result = 0;
	char *buffer;

	static char init_string1[] = 
		"\x55\x53\x42\x43\x12\x34\x56\x78"
		"\x00\x00\x00\x00\x00\x00\x06\x1e"
		"\x00\x00\x00\x00\x00\x00\x00\x00"
		"\x00\x00\x00\x00\x00\x00\x00";

	static char init_string2[] = 
		"\x55\x53\x42\x43\x12\x34\x56\x79"
		"\x00\x00\x00\x00\x00\x00\x06\x1b"
		"\x00\x00\x00\x02\x00\x00\x00\x00"
		"\x00\x00\x00\x00\x00\x00\x00";

	static char init_string3[] = 
		"\x55\x53\x42\x43\x12\x34\x56\x78"
		"\x20\x00\x00\x00\x80\x00\x0c\x85"
		"\x01\x01\x01\x18\x01\x01\x01\x01"
		"\x01\x00\x00\x00\x00\x00\x00";
#if 0 
	//ZTE K3565-Z
	static char init_string4[] = 
		"\x55\x53\x42\x43\x12\x34\x56\x79"
		"\x00\x00\x00\x00\x00\x00\x06\x1b"
		"\x00\x00\x00\x02\x00\x00\x00\x00"
		"\x00\x00\x00\x00\x00\x00\x00";
#endif
 	printk("%s\n", __FUNCTION__);

	buffer = kzalloc(US_BULK_CB_WRAP_LEN, GFP_KERNEL);

	if (buffer == NULL)
		return USB_STOR_TRANSPORT_ERROR;
	
 	memcpy(us->iobuf, init_string1, sizeof(init_string1) - 1);
 	result = usb_stor_bulk_transfer_buf(us, us->send_bulk_pipe,
				us->iobuf,sizeof(init_string1) - 1,NULL);

	result = usb_stor_bulk_transfer_buf(us, us->recv_bulk_pipe,
		buffer, US_BULK_CS_WRAP_LEN, NULL);

	memcpy(us->iobuf, init_string2, sizeof(init_string2) - 1);
 	result = usb_stor_bulk_transfer_buf(us, us->send_bulk_pipe,
				us->iobuf,sizeof(init_string2) - 1,NULL);

	result = usb_stor_bulk_transfer_buf(us, us->recv_bulk_pipe,
		buffer, US_BULK_CS_WRAP_LEN, NULL);

	memcpy(us->iobuf, init_string3, sizeof(init_string3) - 1);
 	result = usb_stor_bulk_transfer_buf(us, us->send_bulk_pipe,
				us->iobuf,sizeof(init_string3) - 1,NULL);

	result = usb_stor_bulk_transfer_buf(us, us->recv_bulk_pipe,
		buffer, US_BULK_CS_WRAP_LEN, NULL);
#if 0 
	memcpy(us->iobuf, init_string4, sizeof(init_string4) - 1);
 	result = usb_stor_bulk_transfer_buf(us, us->send_bulk_pipe,
				us->iobuf,sizeof(init_string4) - 1,NULL);

	result = usb_stor_bulk_transfer_buf(us, us->recv_bulk_pipe,
		buffer, US_BULK_CS_WRAP_LEN, NULL);	
#endif
	US_DEBUGP("usb_stor_bulk_transfer_buf result is %d\n", result); 
 	return (result ? -1 : 0); 
}

int usb_stor_vkom_301_init(struct us_data *us)
{
    const unsigned char rezero_msg[] = {
         0x55, 0x53, 0x42, 0x43, 0x12, 0x34, 0x56, 0x78,
         0x80, 0x00, 0x00, 0x00, 0x80, 0x00, 0x06, 0x06,
         0xf5, 0x04, 0x02, 0x52, 0x70, 0x00, 0x00, 0x00,
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        };

        char *buffer;
        int result;

        printk("usb_stor_vkom_301_init\n");
    
        buffer = kzalloc(US_BULK_CB_WRAP_LEN, GFP_KERNEL);
        if (buffer == NULL)
            return USB_STOR_TRANSPORT_ERROR;
    
        memcpy(buffer, rezero_msg, sizeof (rezero_msg));
        result = usb_stor_bulk_transfer_buf(us,
                us->send_bulk_pipe,
                buffer, sizeof (rezero_msg), NULL);
        
        if (result != USB_STOR_XFER_GOOD) {
            result = USB_STOR_XFER_ERROR;
            goto out;
        }

        /* Some of the devices need to be asked for a response, but we don't
         * care what that response is.
         */
        result = usb_stor_bulk_transfer_buf(us,
                us->recv_bulk_pipe,
                buffer, US_BULK_CS_WRAP_LEN, NULL);
        result = USB_STOR_XFER_GOOD;

    out:
        kfree(buffer);
        return result;
}

/* This places the K4505-z devices in multi-port mode */
int usb_stor_zte_k4505z_init(struct us_data *us)
{
	const unsigned char rezero_msg[] = {
         0x55, 0x53, 0x42, 0x43, 0x12, 0x34, 0x56, 0x78, 
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x1b, 
         0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};

	char *buffer;
	int result;

	buffer = kzalloc(US_BULK_CB_WRAP_LEN, GFP_KERNEL);
	if (buffer == NULL)
		return USB_STOR_TRANSPORT_ERROR;

	memcpy(buffer, rezero_msg, sizeof (rezero_msg));
	result = usb_stor_bulk_transfer_buf(us,
			us->send_bulk_pipe,
			buffer, sizeof (rezero_msg), NULL);
	if (result != USB_STOR_XFER_GOOD) {
		result = USB_STOR_XFER_ERROR;
		goto out;
	}

	/* Some of the devices need to be asked for a response, but we don't
	 * care what that response is.
	 */
	result = usb_stor_bulk_transfer_buf(us,
			us->recv_bulk_pipe,
			buffer, US_BULK_CS_WRAP_LEN, NULL);
	result = USB_STOR_XFER_GOOD;

out:
	kfree(buffer);
	return result;
}

#endif
