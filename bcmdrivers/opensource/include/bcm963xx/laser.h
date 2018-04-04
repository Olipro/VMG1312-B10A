/*
<:label-BRCM:2018:DUAL/GPL:standard 

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

#ifndef _LASER_H_
#define _LASER_H_

#include <linux/if.h>
#include <linux/types.h>
#include <linux/ioctl.h>

#define LASER_TOTAL_OPTICAL_PARAMS_LEN  96

#define LASER_IOC_MAGIC		'O'

#define LASER_IOCTL_GET_DRV_INFO	    _IOR    (LASER_IOC_MAGIC, 0, long)
#define LASER_IOCTL_SET_OPTICAL_PARAMS  _IOWR   (LASER_IOC_MAGIC, 1, LASER_OPTICAL_PARAMS)
#define LASER_IOCTL_GET_OPTICAL_PARAMS  _IOWR   (LASER_IOC_MAGIC, 2, LASER_OPTICAL_PARAMS)
#define LASER_IOCTL_INIT_TX_PWR	        _IO     (LASER_IOC_MAGIC, 3)
#define LASER_IOCTL_INIT_RX_PWR	        _IOW    (LASER_IOC_MAGIC, 4, long)
#define LASER_IOCTL_GET_RX_PWR	        _IOR    (LASER_IOC_MAGIC, 5, short)
#define LASER_IOCTL_GET_TX_PWR	        _IOR    (LASER_IOC_MAGIC, 6, short)
#define LASER_IOCTL_GET_INIT_PARAMS     _IOWR   (LASER_IOC_MAGIC, 7, LASER_INIT_PARAMS)

typedef struct LaserOpticalParams
{
    short opLength;
    unsigned char *opRegisters;
} LASER_OPTICAL_PARAMS, *PLASER_OPTICAL_PARAMS;

typedef struct LaserInitParams
{
    unsigned short initRxReading;
    unsigned short initRxOffset;
    unsigned short initTxReading;
} LASER_INIT_PARAMS, *PLASER_INIT_PARAMS;

int laser_i2c_write_byte(unsigned char , unsigned char);
unsigned char laser_i2c_read_byte(unsigned char );
ssize_t laser_i2c_write(char *, size_t);
ssize_t laser_i2c_read(char *, size_t);

#endif /* ! _LASER_H_ */
