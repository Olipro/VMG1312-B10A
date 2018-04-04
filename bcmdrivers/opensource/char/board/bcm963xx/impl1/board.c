/*
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
*/
/***************************************************************************
* File Name  : board.c
*
* Description: This file contains Linux character device driver entry
*              for the board related ioctl calls: flash, get free kernel
*              page and dump kernel memory, etc.
*
*
***************************************************************************/

/* Includes. */
#include <linux/version.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/capability.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/pagemap.h>
#include <asm/uaccess.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/if.h>
#include <linux/pci.h>
#include <linux/ctype.h>
#include <linux/proc_fs.h>
#include <linux/smp.h>
#include <linux/version.h>
#include <linux/reboot.h>
#include <linux/bcm_assert_locks.h>
#include <asm/delay.h>
#include <linux/fcntl.h>
#include <linux/syscalls.h>
#include <linux/fs.h>

#include <bcmnetlink.h>
#include <net/sock.h>
#include <bcm_map_part.h>
#include <board.h>
#include <spidevices.h>
#define  BCMTAG_EXE_USE
#include <bcmTag.h>
#include <boardparms.h>
#include <boardparms_voice.h>
#include <flash_api.h>
#include <bcm_intr.h>
#include <flash_common.h>
#if defined(CONFIG_11ac_throughput_patch_from_412L08)
#include <shared_utils.h>
#endif
#include <bcmpci.h>
#include <linux/bcm_log.h>
#include <bcmSpiRes.h>
#if 1// __MSTC__, Paul Ho, for WLAN/WPS Button
#include <linux/signal.h>
#endif
//extern unsigned int flash_get_reserved_bytes_at_end(const FLASH_ADDR_INFO *fInfo);

/* Typedefs. */

#if defined (WIRELESS)
#define SES_EVENT_BTN_PRESSED      0x00000001
#define SES_EVENTS                 SES_EVENT_BTN_PRESSED /*OR all values if any*/
#define SES_LED_OFF                0
#define SES_LED_ON                 1
#define SES_LED_BLINK              2

#if defined(CONFIG_BCM96362) || defined(CONFIG_BCM963268) || defined(CONFIG_BCM96318)
#define WLAN_ONBOARD_SLOT	WLAN_ONCHIP_DEV_SLOT
#else
#define WLAN_ONBOARD_SLOT       1 /* Corresponds to IDSEL -- EBI_A11/PCI_AD12 */
#endif

#define BRCM_VENDOR_ID       0x14e4
#define BRCM_WLAN_DEVICE_IDS 0x4300
#define BRCM_WLAN_DEVICE_IDS_DEC 43

#define WLAN_ON   1
#define WLAN_OFF  0
#endif

#if 1 /* Support the CLI command to test incorrect bad block tables(BBT), __CHT__, MitraStar SeanLu, 20140328 */
/*ChenHe@MSTC merge from http://svn.mitrastar.com.tw/svn/CPE_TRUNK/BRCM_412/Products/DSL-401GNU-B1B_V2_CTL/132VFX0. r16481*/
extern void dumpNandInformation(void);
extern void setNandBadBlockEntry (BRCMNAND_ACTION_t action, int Value);
extern int MinBadBlockSet;
extern int MaxBadBlockSet;
#endif
typedef struct
{
    unsigned long ulId;
    char chInUse;
    char chReserved[3];
} MAC_ADDR_INFO, *PMAC_ADDR_INFO;

typedef struct
{
    unsigned long ulNumMacAddrs;
    unsigned char ucaBaseMacAddr[NVRAM_MAC_ADDRESS_LEN];
    MAC_ADDR_INFO MacAddrs[1];
} MAC_INFO, *PMAC_INFO;

typedef struct
{
    unsigned char gponSerialNumber[NVRAM_GPON_SERIAL_NUMBER_LEN];
    unsigned char gponPassword[NVRAM_GPON_PASSWORD_LEN];
} GPON_INFO, *PGPON_INFO;

typedef struct
{
    unsigned long eventmask;
} BOARD_IOC, *PBOARD_IOC;


/*Dyinggasp callback*/
typedef void (*cb_dgasp_t)(void *arg);
typedef struct _CB_DGASP__LIST
{
    struct list_head list;
    char name[IFNAMSIZ];
    cb_dgasp_t cb_dgasp_fn;
    void *context;
}CB_DGASP_LIST , *PCB_DGASP_LIST;


/* Externs. */
extern struct file *fget_light(unsigned int fd, int *fput_needed);
extern unsigned long getMemorySize(void);
extern void __init boardLedInit(void);
extern void boardLedCtrl(BOARD_LED_NAME, BOARD_LED_STATE);

#ifdef CONFIG_MSTC_WDT
extern int mstc_wdt_init(void);
extern void mstc_wdt_exit(void);
extern int mstc_wdt_add_proc(void);
extern void mstc_wdt_del_proc(void);
#endif

/* Prototypes. */
static void set_mac_info( void );
static void set_gpon_info( void );
static int board_open( struct inode *inode, struct file *filp );
static int board_ioctl( struct inode *inode, struct file *flip, unsigned int command, unsigned long arg );
static ssize_t board_read(struct file *filp,  char __user *buffer, size_t count, loff_t *ppos);
static unsigned int board_poll(struct file *filp, struct poll_table_struct *wait);
static int board_release(struct inode *inode, struct file *filp);

static BOARD_IOC* borad_ioc_alloc(void);
static void borad_ioc_free(BOARD_IOC* board_ioc);

/*
 * flashImageMutex must be acquired for all write operations to
 * nvram, CFE, or fs+kernel image.  (cfe and nvram may share a sector).
 */
DEFINE_MUTEX(flashImageMutex);
static void writeNvramDataCrcLocked(PNVRAM_DATA pNvramData);
static PNVRAM_DATA readNvramData(void);
#if 1 //__MSTC__, Dennis
static void mustUpdateNvramfield(PNVRAM_DATA nData,PNVRAM_DATA oData);
#endif

/* DyingGasp function prototype */
static irqreturn_t kerSysDyingGaspIsr(int irq, void * dev_id);
static void __init kerSysInitDyingGaspHandler( void );
static void __exit kerSysDeinitDyingGaspHandler( void );
/* -DyingGasp function prototype - */
/* dgaspMutex protects list add and delete, but is ignored during isr. */
static DEFINE_MUTEX(dgaspMutex);

static int ConfigCs(BOARD_IOCTL_PARMS *parms);
#if defined (WIRELESS) && !defined(MSTC_WPS_BTN_SCE) // __MSTC__, Paul Ho, __MSTC__, Autumn
#if defined(CONFIG_BCM96368) || defined(CONFIG_BCM96328) || defined(CONFIG_BCM963268)
static GPIO_STATE_t GetGpio(int gpio);
#endif
#endif /* !defined(MSTC_WPS_BTN_SCE) */

#if defined (WIRELESS)
static irqreturn_t sesBtn_isr(int irq, void *dev_id);
static void __init sesBtn_mapIntr(int context);
static Bool sesBtn_pressed(void);
static unsigned int sesBtn_poll(struct file *file, struct poll_table_struct *wait);
static ssize_t sesBtn_read(struct file *file,  char __user *buffer, size_t count, loff_t *ppos);
static void __init sesLed_mapGpio(void);
static void sesLed_ctrl(int action, BOARD_LED_NAME ledName);
static void __init ses_board_init(void);
static void __exit ses_board_deinit(void);
static void __init kerSysScreenPciDevices(void);
static void kerSetWirelessPD(int state);
#endif

#if defined(CONFIG_BCM96816) || defined(CONFIG_BCM96362) || defined(CONFIG_BCM96328) || defined(CONFIG_BCM963268) || defined(CONFIG_BCM96318)
static void __init kerSysCheckPowerDownPcie(void);
#endif

static void str_to_num(char* in, char *out, int len);
static int add_proc_files(void);
static int del_proc_files(void);
static int proc_get_param(char *page, char **start, off_t off, int cnt, int *eof, void *data);
static int proc_set_param(struct file *f, const char *buf, unsigned long cnt, void *data);
static int proc_set_led(struct file *f, const char *buf, unsigned long cnt, void *data);

static irqreturn_t reset_isr(int irq, void *dev_id);

// macAddrMutex is used by kerSysGetMacAddress and kerSysReleaseMacAddress
// to protect access to g_pMacInfo
static DEFINE_MUTEX(macAddrMutex);
static PMAC_INFO g_pMacInfo = NULL;
static PGPON_INFO g_pGponInfo = NULL;
static unsigned long g_ulSdramSize;
#if defined(CONFIG_BCM96368)
static unsigned long g_ulSdramWidth;
#endif
static int g_ledInitialized = 0;
static wait_queue_head_t g_board_wait_queue;
static CB_DGASP_LIST *g_cb_dgasp_list_head = NULL;

#define MAX_PAYLOAD_LEN 64
static struct sock *g_monitor_nl_sk;
static int g_monitor_nl_pid = 0 ;
static void kerSysInitMonitorSocket( void );
static void kerSysCleanupMonitorSocket( void );

#if defined(CONFIG_BCM96368)
static void ChipSoftReset(void);
static void ResetPiRegisters( void );
static void PI_upper_set( volatile uint32 *PI_reg, int newPhaseInt );
static void PI_lower_set( volatile uint32 *PI_reg, int newPhaseInt );
static void TurnOffSyncMode( void );
#endif

#if defined(CONFIG_BCM96816)
void board_Init6829( void );
#endif
#if defined(BUILD_11ac_throughput_patch_from_412L07)
static kerSysMacAddressNotifyHook_t kerSysMacAddressNotifyHook = NULL;
#endif
#if !defined(CONFIG_AUXFS_JFFS2)
/* restore default work structure */
static struct work_struct restoreDefaultWork;
#endif

static struct file_operations board_fops =
{
    open:       board_open,
    ioctl:      board_ioctl,
    poll:       board_poll,
    read:       board_read,
    release:    board_release,
};

uint32 board_major = 0;

#if 1  //__MSTC__, Leo, support LED power management
static int pwrMngtOn_usb0 = 1;
static int pwrMngtOn_usb1 = 1;
#endif

#if 1 //__MSTC__, RaynorChung: Ssupport USB LED   
int chkUsbtimer=0;
static int isUSBDevice0Inserted=0;
static int isUSBDevice1Inserted=0;
struct timer_list gUsbTimer;
#endif
#if 1 /* PeterSu */
static int ledctl_usb_te_test = 0;
#endif


#if defined (WIRELESS)
static unsigned short sesBtn_irq = BP_NOT_DEFINED;
static unsigned short sesLed_gpio = BP_NOT_DEFINED;
#ifdef MSTC_WLAN_WPS_BTN_SEPARATE //__MSTC__, Eason, WLAN_WPS_BTN_SEPARATE
static unsigned short wlanBtn_irq = BP_NOT_DEFINED;
#endif
#if 1 // __MSTC__, Paul Ho, for WLAN/WPS Button
struct timer_list gResetTimer;
#if defined(MSTC_WPS_BTN_SCE)
static int chkWlanEnCounter = 0;
#else
int chkWlanBtnTimer;  /* add for WLAN Button */
struct timer_list gWlanTimer; /* add for WLAN Button */
#endif /* defined(MSTC_WPS_BTN_SCE) */
#ifdef MSTC_WLAN_WPS_BTN_SEPARATE //__MSTC__, Eason, WLAN_WPS_BTN_SEPARATE
struct timer_list gResetTimerWlanBtn;
static int chkWlanBtnCounter = 0;
#endif

pid_t  pid_no;

#endif // end, __MSTC__, Paul Ho
#endif /* defined (WIRELESS) */

#if 1 //__MSTC__, FuChia
uint8 stopblinkwl = 0;
uint8 stopblink = 0;
uint8 buttonTest = 0;
#define ktimems (HZ/10)
int ktimems_v = ktimems;
static unsigned short resetBtn_irq = BP_NOT_DEFINED;
int chkResetCounter = 0;
#endif //__MSTC__, FuChia

#if 1 /* chchien , disable wlan button in the front panel */
uint8 disWLANbutton = 0;
#endif

#ifdef CONFIG_MSTC_RECORD_CALL_TRACE
extern char oops_folder_path[64];
extern int oops_file_num;

static void mstc_koops_init(void)
{
	strcpy(oops_folder_path, CONFIG_MSTC_RECORD_CALL_TRACE_PATH);
	oops_file_num = CONFIG_MSTC_OOPS_FILE_NUM;
	return;
}

static int proc_koops_folder_get(char *page, char **start, off_t off, int cnt, int *eof, void *data)
{
	int len;

	len = sprintf(page, "Default folder: %s\nCurrent folder: %s\n", CONFIG_MSTC_RECORD_CALL_TRACE_PATH, oops_folder_path);

	return len;
}

static int proc_koops_folder_set(struct file *f, const char *buf, unsigned long cnt, void *data)
{
	char input[64];
	char tmp[64];
	int len = 0;
	int ret = -1;

	memset(input, 0, sizeof(input));
	memset(tmp, 0, sizeof(tmp));

	if ((cnt > 63) || (copy_from_user(input, buf, cnt) != 0)) {
		printk("Invalid value!");
		return -EFAULT;
	}

	ret = sscanf(input, "%s\n", tmp);
	len = strlen(tmp);
	if((ret != 1) || (len <= 0)) {
		printk("Invalid value!");
		return -EFAULT;
	}

	if(tmp[len - 1] != '/') {
		if(len < 63) {
			tmp[len] = '/';
		} else {
			printk("Invalid value!");
			return -EFAULT;
		}
	}

	strcpy(oops_folder_path, tmp);

	return cnt;
}

static int proc_koops_fn_get(char *page, char **start, off_t off, int cnt, int *eof, void *data)
{
	int len;

	len = sprintf(page, "Default file_num: %d\nCurrent file_num: %d\n", CONFIG_MSTC_OOPS_FILE_NUM, oops_file_num);

	return len;
}

static int proc_koops_fn_set(struct file *f, const char *buf, unsigned long cnt, void *data)
{
	char input[64];
	int fn;
	int ret = -1;

	memset(input, 0, sizeof(input));

	if ((cnt > 63) || (copy_from_user(input, buf, cnt) != 0)) {
		printk("Invalid value!");
		return -EFAULT;
	}

	ret = sscanf(input, "%d\n", &fn);
	if((ret != 1) || (fn <= 0)) {
		printk("Invalid value!");
		return -EFAULT;
	}

	oops_file_num = fn;

	return cnt;
}

static int mstc_koops_add_proc(void)
{
	struct proc_dir_entry *koops_dir = NULL;
	struct proc_dir_entry *koops_folder = NULL;
	struct proc_dir_entry *koops_file_num = NULL;

	koops_dir = proc_mkdir("mstc_koops", NULL);
	if (koops_dir == NULL) {
		printk(KERN_ERR "add_proc_files: failed to create mstc_kd proc files!\n");
		return -1;
	}

	koops_folder = create_proc_entry("oops_folder", 0644, koops_dir);
	if (koops_folder == NULL) {
		printk(KERN_ERR "add_proc_files: failed to create koops_folder proc files!\n");
		return -1;
	}
	koops_folder->read_proc = proc_koops_folder_get;
	koops_folder->write_proc = proc_koops_folder_set;
	#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
	/* New linux no longer requires proc_dir_entry->owner field. */
	#else
	koops_folder->owner = THIS_MODULE;
	#endif

	koops_file_num = create_proc_entry("file_num", 0644, koops_dir);
	if (koops_file_num == NULL) {
		printk(KERN_ERR "add_proc_files: failed to create coredump_preThreshold proc files!\n");
		return -1;
	}
	koops_file_num->read_proc = proc_koops_fn_get;
	koops_file_num->write_proc = proc_koops_fn_set;
	#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
	/* New linux no longer requires proc_dir_entry->owner field. */
	#else
	koops_file_num->owner = THIS_MODULE;
	#endif

	return 0;
}

static void mstc_koops_del_proc(void)
{
	remove_proc_entry("oops_folder", NULL);
	remove_proc_entry("file_num", NULL);

	return;
}
#endif

#ifdef CONFIG_MSTC_DEC_AND_GZIP_CORE
extern char core_folder_path[64];
extern int core_pre_threshold;
extern int core_post_threshold;

static void mstc_coredump_init(void)
{
	strcpy(core_folder_path, CONFIG_MSTC_COREDUMP_FOLDER_PATH);
	core_pre_threshold = CONFIG_MSTC_CORE_SIZE_PRE_THRESHOLD;
	core_post_threshold = CONFIG_MSTC_CORE_SIZE_POST_THRESHOLD;
	return;
}

static int proc_coredump_folder_get(char *page, char **start, off_t off, int cnt, int *eof, void *data)
{
	int len;

	len = sprintf(page, "Default folder: %s\nCurrent folder: %s\n", CONFIG_MSTC_COREDUMP_FOLDER_PATH, core_folder_path);

	return len;
}

static int proc_coredump_folder_set(struct file *f, const char *buf, unsigned long cnt, void *data)
{
	char input[64];
	char tmp[64];
	int len = 0;
	int ret = -1;

	memset(input, 0, sizeof(input));
	memset(tmp, 0, sizeof(tmp));

	if ((cnt > 63) || (copy_from_user(input, buf, cnt) != 0)) {
		printk("Invalid value!");
		return -EFAULT;
	}

	ret = sscanf(input, "%s\n", tmp);
	len = strlen(tmp);
	if((ret != 1) || (len <= 0)) {
		printk("Invalid value!");
		return -EFAULT;
	}

	if(tmp[len - 1] != '/') {
		if(len < 63) {
			tmp[len] = '/';
		} else {
			printk("Invalid value!");
			return -EFAULT;
		}
	}

	strcpy(core_folder_path, tmp);

	return cnt;
}

static int proc_coredump_preThreshold_get(char *page, char **start, off_t off, int cnt, int *eof, void *data)
{
	int len;

	len = sprintf(page, "Default preThreshold: %d\nCurrent preThreshold: %d\n", CONFIG_MSTC_CORE_SIZE_PRE_THRESHOLD, core_pre_threshold);

	return len;
}

static int proc_coredump_preThreshold_set(struct file *f, const char *buf, unsigned long cnt, void *data)
{
	char input[64];
	int threshold;
	int ret = -1;

	memset(input, 0, sizeof(input));

	if ((cnt > 63) || (copy_from_user(input, buf, cnt) != 0)) {
		printk("Invalid value!");
		return -EFAULT;
	}

	ret = sscanf(input, "%d\n", &threshold);
	if((ret != 1) || (threshold <= 0)) {
		printk("Invalid value!");
		return -EFAULT;
	}

	core_pre_threshold = threshold;

	return cnt;
}

static int proc_coredump_postThreshold_get(char *page, char **start, off_t off, int cnt, int *eof, void *data)
{
	int len;

	len = sprintf(page, "Default postThreshold: %d\nCurrent postThreshold: %d\n", CONFIG_MSTC_CORE_SIZE_POST_THRESHOLD, core_post_threshold);

	return len;
}

static int proc_coredump_postThreshold_set(struct file *f, const char *buf, unsigned long cnt, void *data)
{
	char input[64];
	int threshold;
	int ret = -1;

	memset(input, 0, sizeof(input));

	if ((cnt > 63) || (copy_from_user(input, buf, cnt) != 0)) {
		printk("Invalid value!");
		return -EFAULT;
	}

	ret = sscanf(input, "%d\n", &threshold);
	if((ret != 1) || (threshold <= 0)) {
		printk("Invalid value!");
		return -EFAULT;
	}

	core_post_threshold = threshold;

	return cnt;
}

static int mstc_coredump_add_proc(void)
{
	struct proc_dir_entry *coredump_dir = NULL;
	struct proc_dir_entry *coredump_folder = NULL;
	struct proc_dir_entry *coredump_preThreshold = NULL;
	struct proc_dir_entry *coredump_postThreshold = NULL;

	coredump_dir = proc_mkdir("mstc_coredump", NULL);
	if (coredump_dir == NULL) {
		printk(KERN_ERR "add_proc_files: failed to create mstc_kd proc files!\n");
		return -1;
	}

	coredump_folder = create_proc_entry("coredump_folder", 0644, coredump_dir);
	if (coredump_folder == NULL) {
		printk(KERN_ERR "add_proc_files: failed to create coredump_folder proc files!\n");
		return -1;
	}
	coredump_folder->read_proc = proc_coredump_folder_get;
	coredump_folder->write_proc = proc_coredump_folder_set;
	#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
	/* New linux no longer requires proc_dir_entry->owner field. */
	#else
	coredump_folder->owner = THIS_MODULE;
	#endif

	coredump_preThreshold = create_proc_entry("preThreshold", 0644, coredump_dir);
	if (coredump_preThreshold == NULL) {
		printk(KERN_ERR "add_proc_files: failed to create coredump_preThreshold proc files!\n");
		return -1;
	}
	coredump_preThreshold->read_proc = proc_coredump_preThreshold_get;
	coredump_preThreshold->write_proc = proc_coredump_preThreshold_set;
	#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
	/* New linux no longer requires proc_dir_entry->owner field. */
	#else
	coredump_preThreshold->owner = THIS_MODULE;
	#endif

	coredump_postThreshold = create_proc_entry("postThreshold", 0644, coredump_dir);
	if (coredump_postThreshold == NULL) {
		printk(KERN_ERR "add_proc_files: failed to create coredump_postThreshold proc files!\n");
		return -1;
	}
	coredump_postThreshold->read_proc = proc_coredump_postThreshold_get;
	coredump_postThreshold->write_proc = proc_coredump_postThreshold_set;
	#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
	/* New linux no longer requires proc_dir_entry->owner field. */
	#else
	coredump_postThreshold->owner = THIS_MODULE;
	#endif

	return 0;
}

static void mstc_coredump_del_proc(void)
{
	remove_proc_entry("coredump_folder", NULL);
	remove_proc_entry("preThreshold", NULL);
	remove_proc_entry("postThreshold", NULL);

	return;
}
#endif



#if 1 //__MSTC__, RaynorChung: Ssupport USB LED   
void setUsbDeviceInserted(BOARD_LED_NAME ledName, int status){
	if (status == 1)
	{
	  #if 1  //__MSTC__, Leo, support LED power management
	    if (ledName == kLedUSB0){
			isUSBDevice0Inserted = 1;
            pwrMngtOn_usb0 = 1;
	    }
		if (ledName == kLedUSB1){
			isUSBDevice1Inserted = 1;
			pwrMngtOn_usb1 = 1;
		}
	  #else
		if (ledName == kLedUSB0)
			isUSBDevice0Inserted = 1;
		else if (ledName == kLedUSB1)
			isUSBDevice1Inserted = 1;
	  #endif
	} else if (status == 0)
	{
		if (ledName == kLedUSB0)
			isUSBDevice0Inserted = 0;
		else if (ledName == kLedUSB1)
			isUSBDevice1Inserted = 0;
	}
	ledctl_usb_te_test = 0;
	return;
}


void UsbTimerExpire(void)
{
    chkUsbtimer++;
    if (chkUsbtimer > 5) /*check every 500 mini seconds*/
	{
#if 1 /* PeterSu */
        if (ledctl_usb_te_test == 0) 
#endif
        {
		  #if 1  //__MSTC__, Leo, support LED power management
			if (isUSBDevice0Inserted && pwrMngtOn_usb0)
		  #else
			if (isUSBDevice0Inserted)
		  #endif
				kerSysLedCtrl(kLedUSB0,kLedStateOn);
			else
				kerSysLedCtrl(kLedUSB0,kLedStateOff);

	      #if 1  //__MSTC__, Leo, support LED power management
			if (isUSBDevice1Inserted && pwrMngtOn_usb1)
		  #else
			if (isUSBDevice1Inserted)
		  #endif
				kerSysLedCtrl(kLedUSB1,kLedStateOn);
			else
				kerSysLedCtrl(kLedUSB1,kLedStateOff);
        }

		chkUsbtimer =0;
    }

	// start timer
	init_timer(&gUsbTimer);
	gUsbTimer.function = (void*)UsbTimerExpire;
	gUsbTimer.expires = jiffies+ktimems_v;        // timer expires in ~100ms
	add_timer (&gUsbTimer);

}
#endif


#if defined (WIRELESS) && !defined(MSTC_WPS_BTN_SCE) // __MSTC__, Paul Ho, for WLAN Button
void WlanTimerExpire(void)
{
   struct task_struct *p;
   const int CV_WLAN_BTN_PERIOD = 1; /* "Pressing the button for at least 1 second is enabling / disabling the WLAN." */
   unsigned short wlBtnGpio;

   if (BpGetWirelessBtnGpio(&wlBtnGpio) != BP_SUCCESS)
   {
      printk(KERN_ERR "Cannot get WLAN on/off button GPIO!!\n");   
      return;
   }
   
   if (GetGpio(wlBtnGpio) == 0)
   {
      chkWlanBtnTimer++;
   }
   else
   {
      if ( chkWlanBtnTimer >= CV_WLAN_BTN_PERIOD*10 ) {
         printk(KERN_WARNING "\r\n WLAN Behavior %d\n", chkWlanBtnTimer);
         /*if ( buttonTest == 1 ) {
            if ( stopblinkwl == 0 ) {
               kerSysLedCtrl(kLedPowerG, kLedStateFastBlinkContinues);
               stopblinkwl = 1;
            }
            else {
               kerSysLedCtrl(kLedPowerG, kLedStateOn);
               stopblinkwl = 0;
            }
         }
         else*/ {
            printk(KERN_WARNING "\r\n WLAN Button Pressed, send SIGUSR1 to wlmngr pid_no:%d\n", pid_no);
            p = find_task_by_vpid(pid_no);
            if(p!=NULL)
                send_sig(SIGUSR1,p,0);
            else
                printk(KERN_ERR "WLAN on/off can't work.\n");
         }
      }
      /*else if ( chkWlanBtnTimer >0 && buttonTest == 1 ) 
      {
         printk(KERN_ERR "\r\n WLAN Button Pressed\n");
         if ( stopblinkwl == 0 ) {
            kerSysLedCtrl(kLedPowerG, kLedStateFastBlinkContinues);
            stopblinkwl = 1;
         }
         else {
            kerSysLedCtrl(kLedPowerG, kLedStateOn);
            stopblinkwl = 0;
         }
      }*/
      chkWlanBtnTimer = 0;
   }

	// start timer
	init_timer(&gWlanTimer);
	gWlanTimer.function = (void*)WlanTimerExpire;
	gWlanTimer.expires = jiffies+ktimems_v;        // timer expires in ~100ms
	add_timer (&gWlanTimer);
}
#endif /* defined (WIRELESS) */

#if defined(MODULE)
int init_module(void)
{
    return( brcm_board_init() );
}

void cleanup_module(void)
{
    if (MOD_IN_USE)
        printk("brcm flash: cleanup_module failed because module is in use\n");
    else
        brcm_board_cleanup();
}
#endif //MODULE

static int map_external_irq (int irq)
{
    int map_irq;

    switch (irq) {
    case BP_EXT_INTR_0   :
        map_irq = INTERRUPT_ID_EXTERNAL_0;
        break ;
    case BP_EXT_INTR_1   :
        map_irq = INTERRUPT_ID_EXTERNAL_1;
        break ;
    case BP_EXT_INTR_2   :
        map_irq = INTERRUPT_ID_EXTERNAL_2;
        break ;
    case BP_EXT_INTR_3   :
        map_irq = INTERRUPT_ID_EXTERNAL_3;
        break ;
#if defined(CONFIG_BCM96368) || defined(CONFIG_BCM96816)
    case BP_EXT_INTR_4   :
        map_irq = INTERRUPT_ID_EXTERNAL_4;
        break ;
    case BP_EXT_INTR_5   :
        map_irq = INTERRUPT_ID_EXTERNAL_5;
        break ;
#endif
    default           :
        printk ("Invalid External Interrupt definition \n") ;
        map_irq = 0 ;
        break ;
    }
    return (map_irq) ;
}

/* A global variable used by Power Management and other features to determine if Voice is idle or not */
volatile int isVoiceIdle = 1;
EXPORT_SYMBOL(isVoiceIdle);

#if defined(CONFIG_BCM_AVS_PWRSAVE) && !defined(CONFIG_11ac_throughput_patch_from_412L08)
/* A generic kernel thread for board related maintenance activities */
/* Only used if AVS is configured, but feel free to change */
static DECLARE_COMPLETION(poll_done);
static atomic_t poll_lock = ATOMIC_INIT(1);
static int poll_pid = -1;

/* The AVS algorith lowers 1V2 until the slowest ring osc (the one with
   the largest count) reaches the RING_OSC_LOWER_BOUND. Then the algorithm
   will re-increase the voltage if the ring osc continues to slow down to a
   point where its count exceeds the RING_OSC_UPPER_BOUND. */

#if defined(CONFIG_BCM_PWRMNGT_MODULE)
int AvsEnabled = -1; // Wait for the module to control if it is enabled or not
#else
int AvsEnabled = 1; // There is no control so force it to be enabled
#endif
void kerSysBcmEnableAvs(int enable)
{
    unsigned short vregVsel1P2;

    /* Set target 1V2 level */
    if (BpGetVregSel1P2(&vregVsel1P2) == BP_SUCCESS ) {
        AvsEnabled = -1;
        printk("Adaptive Voltage Scaling is disabled because 1V2 is forced to a specific level by design\n");
    } else {
        AvsEnabled = enable;
        printk("Adaptive Voltage Scaling is now %s\n", (enable==1?"enabled":(enable==0?"disabled":"stopped")));
    }
}

int kerSysBcmAvsEnabled(void)
{
    return AvsEnabled;
}

#if defined(CONFIG_BCM96362)
/* 36483, Highest ring osc count read during successful PVT
   a small margin is included to avoid ever reaching this highest count */
//#define RING_OSC_UPPER_BOUND (0x8E83 - 100)
#define RING_OSC_UPPER_BOUND (0x6F1C - 100) // 3.6 MHz + tidbits
/* 1100 is about the max ring osc variation when doing a 1V2 step change */
#define RING_OSC_LOWER_BOUND (RING_OSC_UPPER_BOUND - 1100)
/* 1V2 is set by default to 1.225. HW team is asking that we don't allow the
   voltage to go below 1.19 V. */
#define VREG_VSEL1P2_LOWER_BOUND ((VREG_VSEL1P2_MIDDLE+1) - 3)
/* 6362 Slow parts need to have their voltage increased to avoid WLAN issues
   This is the threshold we use to identify what looks like a slow part.
   A large enough delta is needed between this threshold and the 
   RING_OSC_UPPER_BOUND (~2 MHz here) */
#define RING_OSC_SS_UPPER_BOUND (0x75A5) // 3.4 MHz
#define VREG_VSEL1P2_SS_TARGET   (VREG_VSEL1P2_MIDDLE+1)
#endif
#if defined(CONFIG_BCM96816)
/* PVT suggests we can go as high as A6AA but choosing to be conservative */
#define RING_OSC_UPPER_BOUND (0x9000 - 100)
#define RING_OSC_LOWER_BOUND (RING_OSC_UPPER_BOUND - 1100)
#define VREG_VSEL1P2_LOWER_BOUND ((VREG_VSEL1P2_MIDDLE+1) - 4)
#endif
#if defined(CONFIG_BCM96368)
#define RING_OSC_UPPER_BOUND (0x84FA - 100)
#define RING_OSC_LOWER_BOUND (RING_OSC_UPPER_BOUND - 1500)
#define VREG_VSEL1P2_LOWER_BOUND  ((VREG_VSEL1P2_MIDDLE+1) - 4)
#endif
#if defined(CONFIG_BCM96328)
#define RING_OSC_UPPER_BOUND
#define RING_OSC_LOWER_BOUND
#define VREG_VSEL1P2_LOWER_BOUND
#endif
#if defined(CONFIG_BCM963268) // untested
#define RING_OSC_UPPER_BOUND (0x84FA - 100)
#define RING_OSC_LOWER_BOUND (RING_OSC_UPPER_BOUND - 1500)
#define VREG_VSEL1P2_LOWER_BOUND  ((VREG_VSEL1P2_MIDDLE+1) - 4)
#endif

#define AVSDEBUG(f, ...)
//#define AVSDEBUG(f, ...) printk(f, __VA_ARGS__)

static void brcm_adaptive_voltage_scaling(void)
{
    static int max_count = 0;
    static int is_ss_part = 0;
    int ring_osc_select = 0;
    int current_1v2 = 0;
    int next_1v2 = 0;
    uint32 RingOscCtrl1 = GPIO->RingOscCtrl1;

    /* Verify is AVS is not forced off */
    if (AvsEnabled != -1) {
        /* Verify if the ring oscillator has completed a measurement */
        /* This will only fail on the very first call to this function */
        if (RingOscCtrl1 & RING_OSC_IRQ)
        {
            AVSDEBUG("Read ring osc %d: %lx\n",
                    (RingOscCtrl1 & RING_OSC_SELECT_MASK) >> RING_OSC_SELECT_SHIFT,
                     RingOscCtrl1 & RING_OSC_COUNT_MASK);
            if ((RingOscCtrl1 & RING_OSC_COUNT_MASK) > max_count)
            {
                max_count = RingOscCtrl1 & RING_OSC_COUNT_MASK;
                AVSDEBUG("max_count: %x\n", max_count);
            }

            /* Move to the next enabled ring osc */
            ring_osc_select = (RingOscCtrl1 & RING_OSC_SELECT_MASK) >> RING_OSC_SELECT_SHIFT;
            while (++ring_osc_select < RING_OSC_MAX)
            {
                if ((((1<<ring_osc_select)<<RING_OSC_ENABLE_SHIFT) & RING_OSC_ENABLE_MASK) != 0)
                {
                    break;
                }
            }

            /* If we have read all ring osc, determine if the voltage should be changed */
            if (ring_osc_select == RING_OSC_MAX)
            {
                /* All ring osc have been read, prepare for the next round */
                /* 0 is always a valid ring osc so no need to verify if it is enabled */
                ring_osc_select = 0;

            /* Check if the voltage should be adjusted */
            if ((max_count < RING_OSC_LOWER_BOUND) && AvsEnabled && !is_ss_part)
            {
                /* The ring osc is too fast, reduce the voltage if it is not too low */
#if defined(CONFIG_BCM96368) || defined(CONFIG_BCM96816)
                    current_1v2 = (GPIO->VregConfig & VREG_VSEL1P2_MASK) >> VREG_VSEL1P2_SHIFT;
#elif defined(CONFIG_BCM96362) 
                    current_1v2 = (MISC->miscVregCtrl1 & VREG_VSEL1P2_MASK) >> VREG_VSEL1P2_SHIFT;
#else
                    current_1v2 = (MISC->miscVregCtrl0 & VREG_VSEL1P2_MASK) >> VREG_VSEL1P2_SHIFT;
#endif
                    next_1v2 = current_1v2;
                    if (current_1v2 == 0)
                    {
                        next_1v2 = VREG_VSEL1P2_MIDDLE;
                    }
                    else if (current_1v2 > VREG_VSEL1P2_LOWER_BOUND)
                    {
                        next_1v2--;
                    }
                    AVSDEBUG("ring_osc is fast, can reduce voltage: %d to %d\n", current_1v2, next_1v2);
                }
            else if ((max_count > RING_OSC_UPPER_BOUND) || !AvsEnabled || is_ss_part)
            {
                /* The ring osc is too slow, increase the voltage up to the default of 0 */
                /* If AVS is disabled, we need to force the voltage to come back up to default */
#if defined(CONFIG_BCM96368) || defined(CONFIG_BCM96816)
                current_1v2 = (GPIO->VregConfig & VREG_VSEL1P2_MASK) >> VREG_VSEL1P2_SHIFT;
#elif defined(CONFIG_BCM96362) 
                current_1v2 = (MISC->miscVregCtrl1 & VREG_VSEL1P2_MASK) >> VREG_VSEL1P2_SHIFT;
#else
                current_1v2 = (MISC->miscVregCtrl0 & VREG_VSEL1P2_MASK) >> VREG_VSEL1P2_SHIFT;
#endif
                next_1v2 = current_1v2;

#if defined(CONFIG_BCM96362)
                /* On 6362, we try to identify SS parts to increase their voltage
                   to help WLAN performance */
                if (AvsEnabled && (max_count > RING_OSC_SS_UPPER_BOUND))
                {
                    is_ss_part = 1;
                }

                if (is_ss_part)
                {
                    if (current_1v2 == VREG_VSEL1P2_MIDDLE)
                    {
                        next_1v2 = 0;
                    }
                    else if (current_1v2 == 0)
                    {
                        next_1v2 = VREG_VSEL1P2_MIDDLE+1;
                    }
                    else if (current_1v2 < VREG_VSEL1P2_SS_TARGET)
                    {
                        next_1v2++;
                    }
                } else
#endif
                if (current_1v2 == VREG_VSEL1P2_MIDDLE)
                {
                    next_1v2 = 0;
                }
                else if (current_1v2 != 0)
                {
                    next_1v2++;
                }
                AVSDEBUG("ring_osc is slow, can increase voltage: %d to %d\n", current_1v2, next_1v2);
            }
            if (next_1v2 != current_1v2)
            {
#if defined(CONFIG_BCM96368) || defined(CONFIG_BCM96816)
                    GPIO->VregConfig = (GPIO->VregConfig & ~VREG_VSEL1P2_MASK) | (next_1v2 << VREG_VSEL1P2_SHIFT);
#elif defined(CONFIG_BCM96362) 
                    MISC->miscVregCtrl1 = (MISC->miscVregCtrl1 & ~VREG_VSEL1P2_MASK) | (next_1v2 << VREG_VSEL1P2_SHIFT);
#else
                    MISC->miscVregCtrl0 = (MISC->miscVregCtrl0 & ~VREG_VSEL1P2_MASK) | (next_1v2 << VREG_VSEL1P2_SHIFT);
#endif
                    AVSDEBUG("Adjusted voltage: %d to %d\n", current_1v2, next_1v2);
                }
                max_count = 0;
            }
        }         

        /* Start a new ring osc count cycle by resetting the counter */
        GPIO->RingOscCtrl1 = RING_OSC_ENABLE_MASK |
                             RING_OSC_COUNT_RESET;
        GPIO->RingOscCtrl1 = RING_OSC_ENABLE_MASK |
                             (ring_osc_select << RING_OSC_SELECT_SHIFT);
        /* Writing to this register starts the count */
        GPIO->RingOscCtrl0 = RING_OSC_512_CYCLES;
    }
}

static int brcm_board_timer( void *data )
{
    daemonize("board-timer");

    while (atomic_read(&poll_lock) > 0)
    {
        brcm_adaptive_voltage_scaling();

        /* Sleep for 1 second (HZ jiffies) */
        set_current_state(TASK_INTERRUPTIBLE);
        schedule_timeout(HZ);
    }

    complete_and_exit(&poll_done, 0);
    printk("brcm_board_timer: thread exits!\n");

}
#endif

#if defined(CONFIG_BCM96368)
static unsigned long getMemoryWidth(void)
{
    unsigned long memCfg;

    memCfg = MEMC->Config;
    memCfg &= MEMC_WIDTH_MASK;
    memCfg >>= MEMC_WIDTH_SHFT;

    return memCfg;
}
#endif

static int __init brcm_board_init( void )
{
#if 1 // __MSTC__, FuChia
#else
    unsigned short rstToDflt_irq;
#endif
    int ret;
    bcmLogSpiCallbacks_t loggingCallbacks;
	#ifdef CONFIG_MSTC_WDT
	/* Start hardware watchdog */
	mstc_wdt_init();
	#endif

	#ifdef CONFIG_MSTC_RECORD_CALL_TRACE
	mstc_koops_init();
	#endif

	#ifdef CONFIG_MSTC_DEC_AND_GZIP_CORE
	mstc_coredump_init();
	#endif

    ret = register_chrdev(BOARD_DRV_MAJOR, "brcmboard", &board_fops );
    if (ret < 0)
        printk( "brcm_board_init(major %d): fail to register device.\n",BOARD_DRV_MAJOR);
    else
    {
        printk("brcmboard: brcm_board_init entry\n");

#if 1 //__MSTC__, RaynorChung: Ssupport USB LED   
        init_timer(&gUsbTimer);
        gUsbTimer.function = (void*)UsbTimerExpire;
        gUsbTimer.expires = jiffies+ktimems_v;        // timer expires in ~100ms
        add_timer (&gUsbTimer);
#endif

#if defined (WIRELESS) && !defined(MSTC_WPS_BTN_SCE) // __MSTC__, Paul Ho, for WLAN Button
      // start WLAN button timer
      init_timer(&gWlanTimer);
      gWlanTimer.function = (void*)WlanTimerExpire;
      gWlanTimer.expires = jiffies+ktimems_v;        // timer expires in ~100ms
      add_timer (&gWlanTimer);
#endif
        
        board_major = BOARD_DRV_MAJOR;

        g_ulSdramSize = getMemorySize();
#if defined(CONFIG_BCM96368)
        g_ulSdramWidth = getMemoryWidth();
#endif
        set_mac_info();
        set_gpon_info();

        init_waitqueue_head(&g_board_wait_queue);
#if defined (WIRELESS)
        kerSysScreenPciDevices();
        ses_board_init();
        kerSetWirelessPD(WLAN_ON);
#endif
#if defined(CONFIG_BCM96816) || defined(CONFIG_BCM96362) || defined(CONFIG_BCM96328) || defined(CONFIG_BCM963268) || defined(CONFIG_BCM96318)
        kerSysCheckPowerDownPcie();
#endif
        kerSysInitMonitorSocket();
        kerSysInitDyingGaspHandler();

        boardLedInit();
        g_ledInitialized = 1;
#if 1 //__MSTC__, RaynorChung: To light up Power LED
		kerSysLedCtrl(kLedPowerG, kLedStateSlowBlinkContinues);
#endif
#if 1 // __MSTC__, FuChia
        kerSysLedCtrl(kLedPowerG, kLedStateSlowBlinkContinues);

        if( BpGetResetToDefaultExtIntr(&resetBtn_irq) == BP_SUCCESS )
        {
            resetBtn_irq = map_external_irq (resetBtn_irq) ;
            BcmHalMapInterrupt((FN_HANDLER)reset_isr, 0, resetBtn_irq);
            BcmHalInterruptEnable(resetBtn_irq);
        }
#else
        if( BpGetResetToDefaultExtIntr(&rstToDflt_irq) == BP_SUCCESS )
        {
            rstToDflt_irq = map_external_irq (rstToDflt_irq) ;
            BcmHalMapInterrupt((FN_HANDLER)reset_isr, 0, rstToDflt_irq);
            BcmHalInterruptEnable(rstToDflt_irq);
        }
#endif
#if defined(CONFIG_BCM_CPLD1)
        // Reserve SPI bus to control external CPLD for Standby Timer
        BcmCpld1Initialize();
#endif

#if defined(CONFIG_BCM_AVS_PWRSAVE) && !defined(CONFIG_11ac_throughput_patch_from_412L08)
        poll_pid = kernel_thread(brcm_board_timer, NULL, CLONE_KERNEL);
#if !defined(CONFIG_BCM_PWRMNGT_MODULE)
    // Show that AVS is enabled when PWRMGNT control is not compiled in
    printk("Adaptive Voltage Scaling is always enabled\n");
#endif
#endif

    }

    add_proc_files();

#if defined(CONFIG_BCM96816)
    board_Init6829();
    loggingCallbacks.kerSysSlaveRead   = kerSysBcmSpiSlaveRead;
    loggingCallbacks.kerSysSlaveWrite  = kerSysBcmSpiSlaveWrite;
    loggingCallbacks.bpGet6829PortInfo = BpGet6829PortInfo;
#else
    loggingCallbacks.kerSysSlaveRead   = NULL;
    loggingCallbacks.kerSysSlaveWrite  = NULL;
    loggingCallbacks.bpGet6829PortInfo = NULL;
#endif
    loggingCallbacks.reserveSlave      = BcmSpiReserveSlave;
    loggingCallbacks.syncTrans         = BcmSpiSyncTrans;
    bcmLog_registerSpiCallbacks(loggingCallbacks);

    return ret;
}

static void __init set_mac_info( void )
{
    NVRAM_DATA *pNvramData;
    unsigned long ulNumMacAddrs;

    if (NULL == (pNvramData = readNvramData()))
    {
        printk("set_mac_info: could not read nvram data\n");
        return;
    }

    ulNumMacAddrs = pNvramData->ulNumMacAddrs;

    if( ulNumMacAddrs > 0 && ulNumMacAddrs <= NVRAM_MAC_COUNT_MAX )
    {
        unsigned long ulMacInfoSize =
            //sizeof(MAC_INFO) + ((sizeof(MAC_ADDR_INFO) - 1) * ulNumMacAddrs);
            sizeof(MAC_INFO) + (sizeof(MAC_ADDR_INFO) * (ulNumMacAddrs - 1));

        g_pMacInfo = (PMAC_INFO) kmalloc( ulMacInfoSize, GFP_KERNEL );

        if( g_pMacInfo )
        {
            memset( g_pMacInfo, 0x00, ulMacInfoSize );
            g_pMacInfo->ulNumMacAddrs = pNvramData->ulNumMacAddrs;
            memcpy( g_pMacInfo->ucaBaseMacAddr, pNvramData->ucaBaseMacAddr,
                NVRAM_MAC_ADDRESS_LEN );
        }
        else
            printk("ERROR - Could not allocate memory for MAC data\n");
    }
    else
        printk("ERROR - Invalid number of MAC addresses (%ld) is configured.\n",
        ulNumMacAddrs);
    kfree(pNvramData);
}

static int gponParamsAreErased(NVRAM_DATA *pNvramData)
{
    int i;
    int erased = 1;

    for(i=0; i<NVRAM_GPON_SERIAL_NUMBER_LEN-1; ++i) {
        if((pNvramData->gponSerialNumber[i] != (char) 0xFF) &&
            (pNvramData->gponSerialNumber[i] != (char) 0x00)) {
                erased = 0;
                break;
        }
    }

    if(!erased) {
        for(i=0; i<NVRAM_GPON_PASSWORD_LEN-1; ++i) {
            if((pNvramData->gponPassword[i] != (char) 0xFF) &&
                (pNvramData->gponPassword[i] != (char) 0x00)) {
                    erased = 0;
                    break;
            }
        }
    }

    return erased;
}

static void __init set_gpon_info( void )
{
    NVRAM_DATA *pNvramData;

    if (NULL == (pNvramData = readNvramData()))
    {
        printk("set_gpon_info: could not read nvram data\n");
        return;
    }

    g_pGponInfo = (PGPON_INFO) kmalloc( sizeof(GPON_INFO), GFP_KERNEL );

    if( g_pGponInfo )
    {
        if ((pNvramData->ulVersion < NVRAM_FULL_LEN_VERSION_NUMBER) ||
            gponParamsAreErased(pNvramData))
        {
            strcpy( g_pGponInfo->gponSerialNumber, DEFAULT_GPON_SN );
            strcpy( g_pGponInfo->gponPassword, DEFAULT_GPON_PW );
        }
        else
        {
            strncpy( g_pGponInfo->gponSerialNumber, pNvramData->gponSerialNumber,
                NVRAM_GPON_SERIAL_NUMBER_LEN );
            g_pGponInfo->gponSerialNumber[NVRAM_GPON_SERIAL_NUMBER_LEN-1]='\0';
            strncpy( g_pGponInfo->gponPassword, pNvramData->gponPassword,
                NVRAM_GPON_PASSWORD_LEN );
            g_pGponInfo->gponPassword[NVRAM_GPON_PASSWORD_LEN-1]='\0';
        }
    }
    else
    {
        printk("ERROR - Could not allocate memory for GPON data\n");
    }
    kfree(pNvramData);
}

void __exit brcm_board_cleanup( void )
{
    printk("brcm_board_cleanup()\n");
    del_proc_files();

    if (board_major != -1)
    {
#if defined (WIRELESS)
        ses_board_deinit();
#endif
        kerSysDeinitDyingGaspHandler();
        kerSysCleanupMonitorSocket();
        unregister_chrdev(board_major, "board_ioctl");

#if defined(CONFIG_BCM_AVS_PWRSAVE) && !defined(CONFIG_11ac_throughput_patch_from_412L08)
        if (poll_pid >= 0)
        {
            atomic_dec(&poll_lock);
            wait_for_completion(&poll_done);
        }
#endif
    }
}

static BOARD_IOC* borad_ioc_alloc(void)
{
    BOARD_IOC *board_ioc =NULL;
    board_ioc = (BOARD_IOC*) kmalloc( sizeof(BOARD_IOC) , GFP_KERNEL );
    if(board_ioc)
    {
        memset(board_ioc, 0, sizeof(BOARD_IOC));
    }
    return board_ioc;
}

static void borad_ioc_free(BOARD_IOC* board_ioc)
{
    if(board_ioc)
    {
        kfree(board_ioc);
    }
}


static int board_open( struct inode *inode, struct file *filp )
{
    filp->private_data = borad_ioc_alloc();

    if (filp->private_data == NULL)
        return -ENOMEM;

    return( 0 );
}

static int board_release(struct inode *inode, struct file *filp)
{
    BOARD_IOC *board_ioc = filp->private_data;

    wait_event_interruptible(g_board_wait_queue, 1);
    borad_ioc_free(board_ioc);

    return( 0 );
}


static unsigned int board_poll(struct file *filp, struct poll_table_struct *wait)
{
    unsigned int mask = 0;
#if defined (WIRELESS)
    BOARD_IOC *board_ioc = filp->private_data;
#endif

    poll_wait(filp, &g_board_wait_queue, wait);
#if defined (WIRELESS)
    if(board_ioc->eventmask & SES_EVENTS){
        mask |= sesBtn_poll(filp, wait);
    }
#endif

    return mask;
}

static ssize_t board_read(struct file *filp,  char __user *buffer, size_t count, loff_t *ppos)
{
#if defined (WIRELESS)
    BOARD_IOC *board_ioc = filp->private_data;
    if(board_ioc->eventmask & SES_EVENTS){
        return sesBtn_read(filp, buffer, count, ppos);
    }
#endif
    return 0;
}

/***************************************************************************
// Function Name: getCrc32
// Description  : caculate the CRC 32 of the given data.
// Parameters   : pdata - array of data.
//                size - number of input data bytes.
//                crc - either CRC32_INIT_VALUE or previous return value.
// Returns      : crc.
****************************************************************************/
static UINT32 getCrc32(byte *pdata, UINT32 size, UINT32 crc)
{
    while (size-- > 0)
        crc = (crc >> 8) ^ Crc32_table[(crc ^ *pdata++) & 0xff];

    return crc;
}

/** calculate the CRC for the nvram data block and write it to flash.
 * Must be called with flashImageMutex held.
 */
static void writeNvramDataCrcLocked(PNVRAM_DATA pNvramData)
{
    UINT32 crc = CRC32_INIT_VALUE;

    BCM_ASSERT_HAS_MUTEX_C(&flashImageMutex);

    pNvramData->ulCheckSum = 0;
    crc = getCrc32((char *)pNvramData, sizeof(NVRAM_DATA), crc);
    pNvramData->ulCheckSum = crc;
    kerSysNvRamSet((char *)pNvramData, sizeof(NVRAM_DATA), 0);
}


/** read the nvramData struct from the in-memory copy of nvram.
 * The caller is not required to have flashImageMutex when calling this
 * function.  However, if the caller is doing a read-modify-write of
 * the nvram data, then the caller must hold flashImageMutex.  This function
 * does not know what the caller is going to do with this data, so it
 * cannot assert flashImageMutex held or not when this function is called.
 *
 * @return pointer to NVRAM_DATA buffer which the caller must free
 *         or NULL if there was an error
 */
static PNVRAM_DATA readNvramData(void)
{
    UINT32 crc = CRC32_INIT_VALUE, savedCrc;
    NVRAM_DATA *pNvramData;

    // use GFP_ATOMIC here because caller might have flashImageMutex held
    if (NULL == (pNvramData = kmalloc(sizeof(NVRAM_DATA), GFP_ATOMIC)))
    {
        printk("readNvramData: could not allocate memory\n");
        return NULL;
    }

    kerSysNvRamGet((char *)pNvramData, sizeof(NVRAM_DATA), 0);
    savedCrc = pNvramData->ulCheckSum;
    pNvramData->ulCheckSum = 0;
    crc = getCrc32((char *)pNvramData, sizeof(NVRAM_DATA), crc);
    if (savedCrc != crc)
    {
        // this can happen if we write a new cfe image into flash.
        // The new image will have an invalid nvram section which will
        // get updated to the inMemNvramData.  We detect it here and
        // commonImageWrite will restore previous copy of nvram data.
        kfree(pNvramData);
        pNvramData = NULL;
    }

    return pNvramData;
}



//**************************************************************************************
// Utitlities for dump memory, free kernel pages, mips soft reset, etc.
//**************************************************************************************

/***********************************************************************
* Function Name: dumpaddr
* Description  : Display a hex dump of the specified address.
***********************************************************************/
void dumpaddr( unsigned char *pAddr, int nLen )
{
    static char szHexChars[] = "0123456789abcdef";
    char szLine[80];
    char *p = szLine;
    unsigned char ch, *q;
    int i, j;
    unsigned long ul;

    while( nLen > 0 )
    {
        sprintf( szLine, "%8.8lx: ", (unsigned long) pAddr );
        p = szLine + strlen(szLine);

        for(i = 0; i < 16 && nLen > 0; i += sizeof(long), nLen -= sizeof(long))
        {
            ul = *(unsigned long *) &pAddr[i];
            q = (unsigned char *) &ul;
            for( j = 0; j < sizeof(long); j++ )
            {
                *p++ = szHexChars[q[j] >> 4];
                *p++ = szHexChars[q[j] & 0x0f];
                *p++ = ' ';
            }
        }

        for( j = 0; j < 16 - i; j++ )
            *p++ = ' ', *p++ = ' ', *p++ = ' ';

        *p++ = ' ', *p++ = ' ', *p++ = ' ';

        for( j = 0; j < i; j++ )
        {
            ch = pAddr[j];
            *p++ = (ch > ' ' && ch < '~') ? ch : '.';
        }

        *p++ = '\0';
        printk( "%s\r\n", szLine );

        pAddr += i;
    }
    printk( "\r\n" );
} /* dumpaddr */


/** this function actually does two things, stop other cpu and reset mips.
 * Kept the current name for compatibility reasons.  Image upgrade code
 * needs to call the two steps separately.
 */
void kerSysMipsSoftReset(void)
{
	unsigned long cpu;
	cpu = smp_processor_id();
	printk(KERN_INFO "kerSysMipsSoftReset: called on cpu %lu\n", cpu);

	stopOtherCpu();
	local_irq_disable();  // ignore interrupts, just execute reset code now
	resetPwrmgmtDdrMips();
}

extern void stop_other_cpu(void);  // in arch/mips/kernel/smp.c

void stopOtherCpu(void)
{
#if defined(CONFIG_SMP)
    stop_other_cpu();
#elif defined(CONFIG_BCM_ENDPOINT_MODULE) && defined(CONFIG_BCM_BCMDSP_MODULE)
    unsigned long cpu = (read_c0_diag3() >> 31) ? 0 : 1;

	// Disable interrupts on the other core and allow it to complete processing 
	// and execute the "wait" instruction
    printk(KERN_INFO "stopOtherCpu: stopping cpu %lu\n", cpu);	
    PERF->IrqControl[cpu].IrqMask = 0;
    mdelay(5);
#endif
}

void resetPwrmgmtDdrMips(void)
{
#if defined (CONFIG_BCM963268)
    MISC->miscVdslControl &= ~(MISC_VDSL_CONTROL_VDSL_MIPS_RESET | MISC_VDSL_CONTROL_VDSL_MIPS_POR_RESET );
#endif

#if !defined (CONFIG_BCM96816)
    // Power Management on Ethernet Ports may have brought down EPHY PLL
    // and soft reset below will lock-up 6362 if the PLL is not up
    // therefore bring it up here to give it time to stabilize
    GPIO->RoboswEphyCtrl &= ~EPHY_PWR_DOWN_DLL;
#endif

    // let UART finish printing
    udelay(100);


#if defined(CONFIG_BCM_CPLD1)
    // Determine if this was a request to enter Standby mode
    // If yes, this call won't return and a hard reset will occur later
    BcmCpld1CheckShutdownMode();
#endif

#if defined (CONFIG_BCM96368)
    {
        volatile int delay;
        volatile int i;
        local_irq_disable();
        // after we reset DRAM controller we can't access DRAM, so
        // the first iteration put things in i-cache and the scond interation do the actual reset
        for (i=0; i<2; i++) {
            DDR->DDR1_2PhaseCntl0 &= i - 1;
            DDR->DDR3_4PhaseCntl0 &= i - 1;

            if( i == 1 )
                ChipSoftReset();

            delay = 1000;
            while (delay--);
            PERF->pll_control |= SOFT_RESET*i;
            for(;i;) {} // spin mips and wait soft reset to take effect
        }
    }
#endif
#if !defined(CONFIG_BCM96328) && !defined(CONFIG_BCM96318)
#if defined (CONFIG_BCM96816)
    /* Work around reset issues */
    HVG_MISC_REG_CHANNEL_A->mask |= HVG_SOFT_INIT_0;
    HVG_MISC_REG_CHANNEL_B->mask |= HVG_SOFT_INIT_0;

    {
        unsigned char portInfo6829;
        /* for BHRGR board we need to toggle GPIO30 to
           reset - on early BHR baords this is the GPHY2
           link100 so setting it does not matter */
        if ( (BP_SUCCESS == BpGet6829PortInfo(&portInfo6829)) &&
             (0 != portInfo6829))
        {
            GPIO->GPIODir |= 1<<30;
            GPIO->GPIOio  &= ~(1<<30);
        }
    }
#endif
    PERF->pll_control |= SOFT_RESET;    // soft reset mips
#if defined(CONFIG_BCM96368) || defined(CONFIG_BCM96816)
    PERF->pll_control = 0;
#endif
#else
    TIMER->SoftRst = 1;
#endif
    for(;;) {} // spin mips and wait soft reset to take effect
}

unsigned long kerSysGetMacAddressType( unsigned char *ifName )
{
    unsigned long macAddressType = MAC_ADDRESS_ANY;

#if defined(MSTC_ALL_WAN_SAME_MAC) || defined(MSTC_DIF_L2_SAME_MAC) // __MSTC__, Richard Huang (ALL WAN Using the same MAC)
    if(strstr(ifName, IF_NAME_WETH))
    {
        macAddressType = MAC_ADDRESS_WAN;
#if defined(MSTC_DIF_L2_SAME_MAC)
        macAddressType |= 0x01;
#endif
    }
    else if(strstr(ifName, IF_NAME_WMOCA))
    {
        macAddressType = MAC_ADDRESS_WAN;
#if defined(MSTC_DIF_L2_SAME_MAC)
        macAddressType |= 0x02;
#endif
    }
    else if(strstr(ifName, IF_NAME_WPTM))
    {
        macAddressType = MAC_ADDRESS_WAN;
#if defined(MSTC_DIF_L2_SAME_MAC)
        macAddressType |= 0x04;
#endif
    }
    else if(strstr(ifName, IF_NAME_WATM))
    {
        macAddressType = MAC_ADDRESS_WAN;
#if defined(MSTC_DIF_L2_SAME_MAC)
        macAddressType |= 0x08;
#endif
    }
    else
#endif
    if(strstr(ifName, IF_NAME_ETH))
    {
        macAddressType = MAC_ADDRESS_ETH;
    }
    else if(strstr(ifName, IF_NAME_USB))
    {
        macAddressType = MAC_ADDRESS_USB;
    }
    else if(strstr(ifName, IF_NAME_WLAN))
    {
        macAddressType = MAC_ADDRESS_WLAN;
    }
    else if(strstr(ifName, IF_NAME_MOCA))
    {
        macAddressType = MAC_ADDRESS_MOCA;
    }
    else if(strstr(ifName, IF_NAME_ATM))
    {
        macAddressType = MAC_ADDRESS_ATM;
    }
    else if(strstr(ifName, IF_NAME_PTM))
    {
        macAddressType = MAC_ADDRESS_PTM;
    }
    else if(strstr(ifName, IF_NAME_GPON) || strstr(ifName, IF_NAME_VEIP))
    {
        macAddressType = MAC_ADDRESS_GPON;
    }
    else if(strstr(ifName, IF_NAME_EPON))
    {
        macAddressType = MAC_ADDRESS_EPON;
    }

    return macAddressType;
}
#ifdef BUILD_11ac_throughput_patch_from_412L07
static inline void kerSysMacAddressNotify(unsigned char *pucaMacAddr, MAC_ADDRESS_OPERATION op)
{
    if(kerSysMacAddressNotifyHook)
    {
        kerSysMacAddressNotifyHook(pucaMacAddr, op);
    }
}

int kerSysMacAddressNotifyBind(kerSysMacAddressNotifyHook_t hook)
{
    int nRet = 0;

    if(hook && kerSysMacAddressNotifyHook)
    {
        printk("ERROR: kerSysMacAddressNotifyHook already registered! <0x%08lX>\n",
               (unsigned long)kerSysMacAddressNotifyHook);
        nRet = -EINVAL;
    }
    else
    {
        kerSysMacAddressNotifyHook = hook;
    }

    return nRet;
}
#endif

#if 1 // __MSTC__, Richard Huang
int kerSysGetBaseMacAddress( unsigned char *pucaMacAddr, unsigned int index )
{
    int nRet = 0;

    memcpy((unsigned char *) pucaMacAddr+index,
            &g_pMacInfo->ucaBaseMacAddr[index],
            NVRAM_MAC_ADDRESS_LEN - index);

    return( nRet );
}
#endif

//__ZyXEL__, PingLin, 20171013, eITS #170300895 [BUGFIX][VMG8324-B10A]mac address changes [START]
#ifdef UPC_FIXMAC_CUSTOMIZATION
int kerSysGetMacAddressByIndex( unsigned char *pucaMacAddr, unsigned long ulId, unsigned int *index )
{
    int nRet = 0;
    unsigned long baseMacAddr = 0;

    const unsigned long constMacAddrIncIndex = 3;
    PMAC_ADDR_INFO pMai = NULL;
    PMAC_ADDR_INFO pMaiFreeNoId = NULL;
    PMAC_ADDR_INFO pMaiFreeId = NULL;
    unsigned long i = 0, ulIdxNoId = 0, ulIdxId = 0;

    printk(KERN_ERR "%s:%d ENTER, ulId = %u, macIndex = %u\n", __FUNCTION__, __LINE__, (unsigned int)ulId, (unsigned int)*index);

    if(*index > 0){
        memcpy((unsigned char *) &baseMacAddr,&g_pMacInfo->ucaBaseMacAddr[3],NVRAM_MAC_ADDRESS_LEN - 3);
        baseMacAddr >>= 8;
        baseMacAddr = (baseMacAddr + *index) << 8;
        memcpy( pucaMacAddr, g_pMacInfo->ucaBaseMacAddr, 3);
        memcpy( pucaMacAddr + 3, (unsigned char *)&baseMacAddr, NVRAM_MAC_ADDRESS_LEN - 3 );
        //printk(KERN_INFO "%s:%d ulId = %u, index = %u, pucaMacAddr = %02X:%02X:%02X:%02X:%02X:%02X\n", __FUNCTION__, __LINE__, (unsigned int)ulId, (unsigned int)*index, pucaMacAddr[0], pucaMacAddr[1], pucaMacAddr[2], pucaMacAddr[3], pucaMacAddr[4], pucaMacAddr[5]);
        
        for( i = 0, pMai = g_pMacInfo->MacAddrs; i < g_pMacInfo->ulNumMacAddrs; i++, pMai++ )
        {
            //printk(KERN_INFO "%s:%d i = %u ulId = %u, pMai->ulId = %u,  pMai->chInUse = %d\n", __FUNCTION__, __LINE__, (unsigned int)i, (unsigned int)ulId, (unsigned int)pMai->ulId, pMai->chInUse);
            if(i == *index){
                pMai->ulId = ulId;
#if defined(MSTC_ALL_WAN_SAME_MAC) || defined(MSTC_DIF_L2_SAME_MAC) // __MSTC__, Richard Huang
                pMai->chInUse = (pMai->chInUse < 0)? 1 : pMai->chInUse + 1;
#else
                pMai->chInUse = 1;
#endif
                break;
            }
        }
    }else{
        mutex_lock(&macAddrMutex);

        /* baseMacAddr = last 3 bytes of the base MAC address treated as a 24 bit integer */
        memcpy((unsigned char *) &baseMacAddr,
            &g_pMacInfo->ucaBaseMacAddr[constMacAddrIncIndex],
            NVRAM_MAC_ADDRESS_LEN - constMacAddrIncIndex);
        baseMacAddr >>= 8;

        for( i = 0, pMai = g_pMacInfo->MacAddrs; i < g_pMacInfo->ulNumMacAddrs;
            i++, pMai++ )
        {
            //printk(KERN_INFO "%s:%d i = %u ulId = %u, pMai->ulId = %u,  pMai->chInUse = %d\n", __FUNCTION__, __LINE__, (unsigned int)i, (unsigned int)ulId, (unsigned int)pMai->ulId, pMai->chInUse);
            if( ulId == pMai->ulId || ulId == MAC_ADDRESS_ANY 
#if defined(MSTC_ALL_WAN_SAME_MAC) || defined(MSTC_DIF_L2_SAME_MAC) // __MSTC__, Richard Huang
                || CHECK_WANID(pMai->ulId, ulId)
#endif
            )
            {
                //printk(KERN_INFO "%s:%d This MAC address has been used by the caller in the past.\n", __FUNCTION__, __LINE__);
                /* This MAC address has been used by the caller in the past. */
                baseMacAddr = (baseMacAddr + i) << 8;
                memcpy( pucaMacAddr, g_pMacInfo->ucaBaseMacAddr,
                    constMacAddrIncIndex);
                memcpy( pucaMacAddr + constMacAddrIncIndex, (unsigned char *)
                    &baseMacAddr, NVRAM_MAC_ADDRESS_LEN - constMacAddrIncIndex );

#if defined(MSTC_ALL_WAN_SAME_MAC) || defined(MSTC_DIF_L2_SAME_MAC) // __MSTC__, Richard Huang
                if ( CHECK_WANID(pMai->ulId, ulId) ) {
                pMai->ulId = ulId;
                }
                pMai->chInUse = (pMai->chInUse < 0)? 1 : pMai->chInUse + 1;
#else
                pMai->chInUse = 1;
#endif

                *index = i;
                //printk(KERN_INFO "%s:%d index = %u\n", __FUNCTION__, __LINE__, (unsigned int)*index);

                pMaiFreeNoId = pMaiFreeId = NULL;
                break;
            }
            else{
                if( pMai->chInUse == 0 )
                {
                    if( pMai->ulId == 0 && pMaiFreeNoId == NULL )
                    {
                        /* This is an available MAC address that has never been
                        * used.
                        */
                        pMaiFreeNoId = pMai;
                        ulIdxNoId = i;
                        //printk(KERN_INFO "%s:%d pMaiFreeNoId set to %u\n", __FUNCTION__, __LINE__, (unsigned int)ulIdxNoId);
                    }
                    else
                    {
                        if( pMai->ulId != 0 && pMaiFreeId == NULL )
                        {
                            /* This is an available MAC address that has been used
                            * before.  Use addresses that have never been used
                            * first, before using this one.
                            */
                            pMaiFreeId = pMai;
                            ulIdxId = i;
    
#if defined(MSTC_ALL_WAN_SAME_MAC) || defined(MSTC_DIF_L2_SAME_MAC) // __MSTC__, Richard Huang
                            if ( CHECK_WANID(pMai->ulId, ulId) ) {
                            pMaiFreeNoId = NULL;
                            break;
                            }
#endif
                            //printk(KERN_INFO "%s:%d pMaiFreeId set to %u\n", __FUNCTION__, __LINE__, (unsigned int)ulIdxId);
                        }
                    }
                }
            }
        }

        if( pMaiFreeNoId || pMaiFreeId )
        {
            /* An available MAC address was found. */
            memcpy(pucaMacAddr, g_pMacInfo->ucaBaseMacAddr,NVRAM_MAC_ADDRESS_LEN);
            if( pMaiFreeNoId )
            {
                //printk(KERN_INFO "%s:%d pMaiFreeNoId. ulIdxNoId = %u\n", __FUNCTION__, __LINE__, (unsigned int)ulIdxNoId);
                baseMacAddr = (baseMacAddr + ulIdxNoId) << 8;
                memcpy( pucaMacAddr, g_pMacInfo->ucaBaseMacAddr,
                    constMacAddrIncIndex);
                memcpy( pucaMacAddr + constMacAddrIncIndex, (unsigned char *)
                    &baseMacAddr, NVRAM_MAC_ADDRESS_LEN - constMacAddrIncIndex );
                pMaiFreeNoId->ulId = ulId;
#if defined(MSTC_ALL_WAN_SAME_MAC) || defined(MSTC_DIF_L2_SAME_MAC) // __MSTC__, Richard Huang
                pMaiFreeNoId->chInUse = (pMaiFreeNoId->chInUse < 0)? 1 : pMaiFreeNoId->chInUse + 1;
#else
                pMaiFreeNoId->chInUse = 1;
#endif

                *index = ulIdxNoId;
                //printk(KERN_INFO "%s:%d pMaiFreeNoId. index = %u\n", __FUNCTION__, __LINE__, (unsigned int)*index);

            }
            else
            {
                //printk(KERN_INFO "%s:%d pMaiFreeId. ulIdxId = %u\n", __FUNCTION__, __LINE__, (unsigned int)ulIdxId);
                baseMacAddr = (baseMacAddr + ulIdxId) << 8;
                memcpy( pucaMacAddr, g_pMacInfo->ucaBaseMacAddr,
                    constMacAddrIncIndex);
                memcpy( pucaMacAddr + constMacAddrIncIndex, (unsigned char *)
                    &baseMacAddr, NVRAM_MAC_ADDRESS_LEN - constMacAddrIncIndex );
                pMaiFreeId->ulId = ulId;
#if defined(MSTC_ALL_WAN_SAME_MAC) || defined(MSTC_DIF_L2_SAME_MAC) // __MSTC__, Richard Huang
                pMaiFreeId->chInUse = (pMaiFreeId->chInUse < 0)? 1 : pMaiFreeId->chInUse + 1;
#else
                pMaiFreeId->chInUse = 1;
#endif

                *index = ulIdxId;
                //printk(KERN_INFO "%s:%d pMaiFreeId. index = %u\n", __FUNCTION__, __LINE__,(unsigned int)*index);

            }
        }
        else
            if( i == g_pMacInfo->ulNumMacAddrs )
                nRet = -EADDRNOTAVAIL;

        mutex_unlock(&macAddrMutex);
    }

    printk(KERN_ERR "%s:%d ulId = %u, index = %u, pucaMacAddr = %02X:%02X:%02X:%02X:%02X:%02X\n", __FUNCTION__, __LINE__, 
        (unsigned int)ulId, (unsigned int)*index, pucaMacAddr[0], pucaMacAddr[1], pucaMacAddr[2], pucaMacAddr[3], pucaMacAddr[4], pucaMacAddr[5]);

    return( nRet );
}
#endif
//__ZyXEL__, PingLin, 20171013, eITS #170300895 [BUGFIX][VMG8324-B10A]mac address changes [END]

int kerSysGetMacAddress( unsigned char *pucaMacAddr, unsigned long ulId )
{
    const unsigned long constMacAddrIncIndex = 3;
    int nRet = 0;
    PMAC_ADDR_INFO pMai = NULL;
    PMAC_ADDR_INFO pMaiFreeNoId = NULL;
    PMAC_ADDR_INFO pMaiFreeId = NULL;
    unsigned long i = 0, ulIdxNoId = 0, ulIdxId = 0, baseMacAddr = 0;

    mutex_lock(&macAddrMutex);

    /* baseMacAddr = last 3 bytes of the base MAC address treated as a 24 bit integer */
    memcpy((unsigned char *) &baseMacAddr,
        &g_pMacInfo->ucaBaseMacAddr[constMacAddrIncIndex],
        NVRAM_MAC_ADDRESS_LEN - constMacAddrIncIndex);
    baseMacAddr >>= 8;

    for( i = 0, pMai = g_pMacInfo->MacAddrs; i < g_pMacInfo->ulNumMacAddrs;
        i++, pMai++ )
    {
        if( ulId == pMai->ulId || ulId == MAC_ADDRESS_ANY 
#if defined(MSTC_ALL_WAN_SAME_MAC) || defined(MSTC_DIF_L2_SAME_MAC) // __MSTC__, Richard Huang
            || CHECK_WANID(pMai->ulId, ulId)
#endif
        )
        {
            /* This MAC address has been used by the caller in the past. */
            baseMacAddr = (baseMacAddr + i) << 8;
            memcpy( pucaMacAddr, g_pMacInfo->ucaBaseMacAddr,
                constMacAddrIncIndex);
            memcpy( pucaMacAddr + constMacAddrIncIndex, (unsigned char *)
                &baseMacAddr, NVRAM_MAC_ADDRESS_LEN - constMacAddrIncIndex );

#if defined(MSTC_ALL_WAN_SAME_MAC) || defined(MSTC_DIF_L2_SAME_MAC) // __MSTC__, Richard Huang
            if ( CHECK_WANID(pMai->ulId, ulId) ) {
               pMai->ulId = ulId;
            }
            pMai->chInUse = (pMai->chInUse < 0)? 1 : pMai->chInUse + 1;
#else
            pMai->chInUse = 1;
#endif

            pMaiFreeNoId = pMaiFreeId = NULL;
            break;
        }
        else
            if( pMai->chInUse == 0 )
            {
                if( pMai->ulId == 0 && pMaiFreeNoId == NULL )
                {
                    /* This is an available MAC address that has never been
                    * used.
                    */
                    pMaiFreeNoId = pMai;
                    ulIdxNoId = i;
                }
                else
                    if( pMai->ulId != 0 && pMaiFreeId == NULL )
                    {
                        /* This is an available MAC address that has been used
                        * before.  Use addresses that have never been used
                        * first, before using this one.
                        */
                        pMaiFreeId = pMai;
                        ulIdxId = i;

#if defined(MSTC_ALL_WAN_SAME_MAC) || defined(MSTC_DIF_L2_SAME_MAC) // __MSTC__, Richard Huang
                        if ( CHECK_WANID(pMai->ulId, ulId) ) {
                           pMaiFreeNoId = NULL;
                           break;
                        }
#endif

                    }
            }
    }

    if( pMaiFreeNoId || pMaiFreeId )
    {
        /* An available MAC address was found. */
        memcpy(pucaMacAddr, g_pMacInfo->ucaBaseMacAddr,NVRAM_MAC_ADDRESS_LEN);
        if( pMaiFreeNoId )
        {
            baseMacAddr = (baseMacAddr + ulIdxNoId) << 8;
            memcpy( pucaMacAddr, g_pMacInfo->ucaBaseMacAddr,
                constMacAddrIncIndex);
            memcpy( pucaMacAddr + constMacAddrIncIndex, (unsigned char *)
                &baseMacAddr, NVRAM_MAC_ADDRESS_LEN - constMacAddrIncIndex );
            pMaiFreeNoId->ulId = ulId;
#if defined(MSTC_ALL_WAN_SAME_MAC) || defined(MSTC_DIF_L2_SAME_MAC) // __MSTC__, Richard Huang
            pMaiFreeNoId->chInUse = (pMaiFreeNoId->chInUse < 0)? 1 : pMaiFreeNoId->chInUse + 1;
#else
            pMaiFreeNoId->chInUse = 1;
#endif
        }
        else
        {
            baseMacAddr = (baseMacAddr + ulIdxId) << 8;
            memcpy( pucaMacAddr, g_pMacInfo->ucaBaseMacAddr,
                constMacAddrIncIndex);
            memcpy( pucaMacAddr + constMacAddrIncIndex, (unsigned char *)
                &baseMacAddr, NVRAM_MAC_ADDRESS_LEN - constMacAddrIncIndex );
            pMaiFreeId->ulId = ulId;
#if defined(MSTC_ALL_WAN_SAME_MAC) || defined(MSTC_DIF_L2_SAME_MAC) // __MSTC__, Richard Huang
            pMaiFreeId->chInUse = (pMaiFreeId->chInUse < 0)? 1 : pMaiFreeId->chInUse + 1;
#else
            pMaiFreeId->chInUse = 1;
#endif
        }
    }
    else
        if( i == g_pMacInfo->ulNumMacAddrs )
            nRet = -EADDRNOTAVAIL;

    mutex_unlock(&macAddrMutex);

    return( nRet );
} /* kerSysGetMacAddr */

int kerSysReleaseMacAddress( unsigned char *pucaMacAddr )
{
    const unsigned long constMacAddrIncIndex = 3;
    int nRet = -EINVAL;
    unsigned long ulIdx = 0;
    unsigned long baseMacAddr = 0;
    unsigned long relMacAddr = 0;

    mutex_lock(&macAddrMutex);

    /* baseMacAddr = last 3 bytes of the base MAC address treated as a 24 bit integer */
    memcpy((unsigned char *) &baseMacAddr,
        &g_pMacInfo->ucaBaseMacAddr[constMacAddrIncIndex],
        NVRAM_MAC_ADDRESS_LEN - constMacAddrIncIndex);
    baseMacAddr >>= 8;

    /* Get last 3 bytes of MAC address to release. */
    memcpy((unsigned char *) &relMacAddr, &pucaMacAddr[constMacAddrIncIndex],
        NVRAM_MAC_ADDRESS_LEN - constMacAddrIncIndex);
    relMacAddr >>= 8;

    ulIdx = relMacAddr - baseMacAddr;

    if( ulIdx < g_pMacInfo->ulNumMacAddrs )
    {
        PMAC_ADDR_INFO pMai = &g_pMacInfo->MacAddrs[ulIdx];
#if defined(MSTC_ALL_WAN_SAME_MAC) || defined(MSTC_DIF_L2_SAME_MAC) // __MSTC__, Richard Huang
        if( pMai->chInUse > 0 )
        {
            pMai->chInUse--;
            nRet = 0;
        }
        else {
            pMai->chInUse = 0;
            nRet = 0;
        }
#else
        if( pMai->chInUse == 1 )
        {
            pMai->chInUse = 0;
            nRet = 0;
        }
#endif
    }

    mutex_unlock(&macAddrMutex);

    return( nRet );
} /* kerSysReleaseMacAddr */


void kerSysGetGponSerialNumber( unsigned char *pGponSerialNumber )
{
    strcpy( pGponSerialNumber, g_pGponInfo->gponSerialNumber );
}


void kerSysGetGponPassword( unsigned char *pGponPassword )
{
    strcpy( pGponPassword, g_pGponInfo->gponPassword );
}

int kerSysGetSdramSize( void )
{
    return( (int) g_ulSdramSize );
} /* kerSysGetSdramSize */


#if defined(CONFIG_BCM96368)
/*
 * This function returns:
 * MEMC_32BIT_BUS for 32-bit SDRAM
 * MEMC_16BIT_BUS for 16-bit SDRAM
 */
unsigned int kerSysGetSdramWidth( void )
{
    return (unsigned int)(g_ulSdramWidth);
} /* kerSysGetSdramWidth */
#endif


/*Read Wlan Params data from CFE */
int kerSysGetWlanSromParams( unsigned char *wlanParams, unsigned short len)
{
    NVRAM_DATA *pNvramData;

    if (NULL == (pNvramData = readNvramData()))
    {
        printk("kerSysGetWlanSromParams: could not read nvram data\n");
        return -1;
    }

    memcpy( wlanParams,
           (char *)pNvramData + ((size_t) &((NVRAM_DATA *)0)->wlanParams),
            len );
    kfree(pNvramData);

    return 0;
}

/*Read Wlan Params data from CFE */
int kerSysGetAfeId( unsigned long *afeId )
{
    NVRAM_DATA *pNvramData;

    if (NULL == (pNvramData = readNvramData()))
    {
        printk("kerSysGetAfeId: could not read nvram data\n");
        return -1;
    }

    afeId [0] = pNvramData->afeId[0];
    afeId [1] = pNvramData->afeId[1];
    kfree(pNvramData);

    return 0;
}

void kerSysLedCtrl(BOARD_LED_NAME ledName, BOARD_LED_STATE ledState)
{
    #if 1  //__MSTC__, Leo, support LED power management
	 if (ledName == kLedUSB0 && ledState == kLedStateOff) 
		pwrMngtOn_usb0 = 0;
	 if (ledName == kLedUSB1 && ledState == kLedStateOff)
	 	pwrMngtOn_usb1 = 0;
	 if (ledName == kLedUSB0 && ledState == kLedStateOn)
		pwrMngtOn_usb0 = 1;
	 if (ledName == kLedUSB1 && ledState == kLedStateOn)
	 	pwrMngtOn_usb1 = 1;
    #endif
    if (g_ledInitialized)
        boardLedCtrl(ledName, ledState);
}

/*functionto receive message from usersapce
 * Currently we dont expect any messages fromm userspace
 */
void kerSysRecvFrmMonitorTask(struct sk_buff *skb)
{

   /*process the message here*/
   printk(KERN_WARNING "unexpected skb received at %s \n",__FUNCTION__);
   kfree_skb(skb);
   return;
}

void kerSysInitMonitorSocket( void )
{
   g_monitor_nl_sk = netlink_kernel_create(&init_net, NETLINK_BRCM_MONITOR, 0, kerSysRecvFrmMonitorTask, NULL, THIS_MODULE);

   if(!g_monitor_nl_sk)
   {
      printk(KERN_ERR "Failed to create a netlink socket for monitor\n");
      return;
   }

}


void kerSysSendtoMonitorTask(int msgType, char *msgData, int msgDataLen)
{

   struct sk_buff *skb =  NULL;
   struct nlmsghdr *nl_msgHdr = NULL;
   unsigned int payloadLen =sizeof(struct nlmsghdr);

   if(!g_monitor_nl_pid)
   {
      printk(KERN_INFO "message received before monitor task is initialized %s \n",__FUNCTION__);
      return;
   } 

   if(msgData && (msgDataLen > MAX_PAYLOAD_LEN))
   {
      printk(KERN_ERR "invalid message len in %s",__FUNCTION__);
      return;
   } 

   payloadLen += msgDataLen;
   payloadLen = NLMSG_SPACE(payloadLen);

   /*Alloc skb ,this check helps to call the fucntion from interrupt context */

   if(in_atomic())
   {
      skb = alloc_skb(payloadLen, GFP_ATOMIC);
   }
   else
   {
      skb = alloc_skb(payloadLen, GFP_KERNEL);
   }

   if(!skb)
   {
      printk(KERN_ERR "failed to alloc skb in %s",__FUNCTION__);
      return;
   }

   nl_msgHdr = (struct nlmsghdr *)skb->data;
   nl_msgHdr->nlmsg_type = msgType;
   nl_msgHdr->nlmsg_pid=0;/*from kernel */
   nl_msgHdr->nlmsg_len = payloadLen;
   nl_msgHdr->nlmsg_flags =0;

   if(msgData)
   {
      memcpy(NLMSG_DATA(nl_msgHdr),msgData,msgDataLen);
   }      

   NETLINK_CB(skb).pid = 0; /*from kernel */

   skb->len = payloadLen; 

   netlink_unicast(g_monitor_nl_sk, skb, g_monitor_nl_pid, MSG_DONTWAIT);
   return;
}

void kerSysCleanupMonitorSocket(void)
{
   g_monitor_nl_pid = 0 ;
   sock_release(g_monitor_nl_sk->sk_socket);
}

// Must be called with flashImageMutex held
static PFILE_TAG getTagFromPartition(int imageNumber)
{
    static unsigned char sectAddr1[sizeof(FILE_TAG) + sizeof(int)];
    static unsigned char sectAddr2[sizeof(FILE_TAG) + sizeof(int)];
    int blk = 0;
    UINT32 crc;
    PFILE_TAG pTag = NULL;
    unsigned char *pBase = flash_get_memptr(0);
    unsigned char *pSectAddr = NULL;
#if 1 /* fix bootup with wrong rootfs address,  Mitrastar Kid, 20120509. */
    int newRootfsAddr = 0;
    int tagRootfsAddr = 0;
    int tagKernelAddr = 0;
    int offset = 0;
#endif

    /* The image tag for the first image is always after the boot loader.
    * The image tag for the second image, if it exists, is at one half
    * of the flash size.
    */
#if defined(INC_NAND_FLASH_DRIVER) && (INC_NAND_FLASH_DRIVER==1) //__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11
	if( imageNumber == 1 )
	{
		FLASH_ADDR_INFO flash_info;

		kerSysFlashAddrInfoGet(&flash_info); 
		if( flash_get_flash_type() !=  FLASH_IFC_NAND ) {
			blk = flash_get_blk((int) (pBase+flash_info.flash_rootfs_start_offset));
		}
		else {
			blk = getNandBlock((int)(pBase+flash_info.flash_rootfs_start_offset), 0);
		}
		pSectAddr = sectAddr1;
	}
	else
		if( imageNumber == 2 )
		{
			if( flash_get_flash_type() !=  FLASH_IFC_NAND ) {
		   		blk = flash_get_blk((int) (pBase + (flash_get_total_size() / 2)));
			}
			else {
				blk = getNandBlock((int) (pBase + (flash_get_total_size() / 2)), 0);
			} 
			pSectAddr = sectAddr2;
		}
#else
    if( imageNumber == 1 )
    {
        FLASH_ADDR_INFO flash_info;

        kerSysFlashAddrInfoGet(&flash_info); 
        blk = flash_get_blk((int) (pBase+flash_info.flash_rootfs_start_offset));

        pSectAddr = sectAddr1;
    }
    else
        if( imageNumber == 2 )
        {
            blk = flash_get_blk((int) (pBase + (flash_get_total_size() / 2))); 
            pSectAddr = sectAddr2;
        }
#endif
#if 1 //__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11
        if( blk > 0 )
#else
        if( blk )
#endif
        {
            int *pn;

            memset(pSectAddr, 0x00, sizeof(FILE_TAG));
            flash_read_buf((unsigned short) blk, 0, pSectAddr, sizeof(FILE_TAG));
            crc = CRC32_INIT_VALUE;
            crc = getCrc32(pSectAddr, (UINT32)TAG_LEN-TOKEN_LEN, crc);
            pTag = (PFILE_TAG) pSectAddr;
            pn = (int *) (pTag + 1);
            *pn = blk;
            if (crc != (UINT32)(*(UINT32*)(pTag->tagValidationToken)))
                pTag = NULL;
#if 1 /* fix bootup with wrong rootfs address,  Mitrastar Kid, 20120509. */
            if (NULL != pTag) {
                newRootfsAddr = ((blk+1)*flash_get_sector_size(0)) + (unsigned long)IMAGE_BASE;
                tagRootfsAddr = simple_strtoul(pTag->rootfsAddress, NULL, 10);
                tagKernelAddr = simple_strtoul(pTag->kernelAddress, NULL, 10);
                offset = (newRootfsAddr - tagRootfsAddr);
                if ((0 != tagRootfsAddr) && (0 < offset)) {
                    tagKernelAddr += offset;

                    sprintf(pTag->rootfsAddress,"%lu", (unsigned long)newRootfsAddr);
                    sprintf(pTag->kernelAddress,"%lu", (unsigned long)tagKernelAddr);

				    // Update the tagValidationToken value because rootfsAddress and kernelAddress changed, Mitrastar, Delon, 20121115
				    crc = CRC32_INIT_VALUE;
				    crc = getCrc32((unsigned char *)pTag,(UINT32)TAG_LEN-TOKEN_LEN,crc);
				    *(unsigned long *) &pTag->tagValidationToken[0] = crc;
                }
            }
#endif
        }

        return( pTag );
}

// must be called with flashImageMutex held
static int getPartitionFromTag( PFILE_TAG pTag )
{
    int ret = 0;

    if( pTag )
    {
        PFILE_TAG pTag1 = getTagFromPartition(1);
        PFILE_TAG pTag2 = getTagFromPartition(2);
        int sequence = simple_strtoul(pTag->imageSequence,  NULL, 10);
        int sequence1 = (pTag1) ? simple_strtoul(pTag1->imageSequence, NULL, 10)
            : -1;
        int sequence2 = (pTag2) ? simple_strtoul(pTag2->imageSequence, NULL, 10)
            : -1;

        if( pTag1 && sequence == sequence1 )
            ret = 1;
        else
            if( pTag2 && sequence == sequence2 )
                ret = 2;
    }

    return( ret );
}


// must be called with flashImageMutex held
static PFILE_TAG getBootImageTag(void)
{
    static int displayFsAddr = 1;
    PFILE_TAG pTag = NULL;
    PFILE_TAG pTag1 = getTagFromPartition(1);
    PFILE_TAG pTag2 = getTagFromPartition(2);
#if 1 //__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11
    NVRAM_DATA *pNvramData;
#endif

    BCM_ASSERT_HAS_MUTEX_C(&flashImageMutex);

    if( pTag1 && pTag2 )
    {
#if 1 //__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11
        /* Two images are flashed. */
        int sequence1 = simple_strtoul(pTag1->imageSequence, NULL, 10);
        int sequence2 = simple_strtoul(pTag2->imageSequence, NULL, 10);
        char *p;
        char bootPartition = BOOT_LATEST_IMAGE;

        if (NULL == (pNvramData = readNvramData()))
        {
            return pTag;
        }

        for( p = pNvramData->szBootline; p[2] != '\0'; p++ )
        {
            if( p[0] == 'p' && p[1] == '=' )
            {
                bootPartition = p[2];
                break;
            }
        }

#ifdef MTSC_DUAL_IMAGE_SELECT
        if( pNvramData->partitionNum == '0' )
		{
			pTag = pTag1;
		}
		else if( pNvramData->partitionNum == '1' )
		{
			pTag = pTag2;
		}
		else {
#endif
        if( bootPartition == BOOT_LATEST_IMAGE )
            pTag = (sequence2 > sequence1) ? pTag2 : pTag1;
        else /* Boot from the image configured. */
            pTag = (sequence2 < sequence1) ? pTag2 : pTag1;
		
#ifdef MTSC_DUAL_IMAGE_SELECT			
		}
#endif

	kfree(pNvramData);
#else
        /* Two images are flashed. */
        int sequence1 = simple_strtoul(pTag1->imageSequence, NULL, 10);
        int sequence2 = simple_strtoul(pTag2->imageSequence, NULL, 10);
        int imgid = 0;

        kerSysBlParmsGetInt(BOOTED_IMAGE_ID_NAME, &imgid);
        if( imgid == BOOTED_OLD_IMAGE )
            pTag = (sequence2 < sequence1) ? pTag2 : pTag1;
        else
            pTag = (sequence2 > sequence1) ? pTag2 : pTag1;
#endif
    }
    else
        /* One image is flashed. */
        pTag = (pTag2) ? pTag2 : pTag1;

    if( pTag && displayFsAddr )
    {
        displayFsAddr = 0;
        printk("File system address: 0x%8.8lx\n",
            simple_strtoul(pTag->rootfsAddress, NULL, 10) + BOOT_OFFSET);
    }

    return( pTag );
}

#ifdef MSTC_OBM_IMAGE_DEFAULT //__MSTC__, Dennis ZyXEL OBM ImageDefault feature, zongyue
static PIMAGE_TAG getIMGDEFTagFromPartition(int imageNumber, unsigned int curImgSize)
{
    static unsigned char IMGDEFsectAddr1[sizeof(IMAGE_TAG)];
    static unsigned char IMGDEFsectAddr2[sizeof(IMAGE_TAG)];
    int blk = 0;
    UINT32 crc;
    PIMAGE_TAG pTag = NULL;
    unsigned char *pBase = flash_get_memptr(0);
    unsigned char *pSectAddr = NULL;

    /* The image tag for the first image is always after the boot loader.
    * The image tag for the second image, if it exists, is at one half
    * of the flash size.
    */
    if( imageNumber == 1 )
    {
        FLASH_ADDR_INFO flash_info;

        kerSysFlashAddrInfoGet(&flash_info);
        blk = flash_get_blk((int) (pBase+flash_info.flash_rootfs_start_offset));
        pSectAddr = IMGDEFsectAddr1;
    }
    else
        if( imageNumber == 2 )
        {
            blk = flash_get_blk((int) (pBase + (flash_get_total_size() / 2)));
            pSectAddr = IMGDEFsectAddr2;
        }

        if( blk )
        {
            memset(pSectAddr, 0x00, sizeof(FILE_TAG));
            flash_read_buf((unsigned short) blk, curImgSize, pSectAddr, sizeof(IMAGE_TAG));
            crc = CRC32_INIT_VALUE;
            crc = getCrc32(pSectAddr, (UINT32)TAG_LEN-TOKEN_LEN, crc);
            pTag = (PIMAGE_TAG) pSectAddr;
            if (crc != (UINT32)(*(UINT32*)(pTag->tagValidationToken)))
                pTag = NULL;
        }

        return( pTag );
}

static PIMAGE_TAG getBootImageDefTag(unsigned int curImgSize)
{
    PIMAGE_TAG pTag = NULL;
    PFILE_TAG pTag1 = getTagFromPartition(1);
    PFILE_TAG pTag2 = getTagFromPartition(2);
    NVRAM_DATA *pNvramData;

    if( pTag1 && pTag2 )
    {
        /* Two images are flashed. */
        int sequence1 = simple_strtoul(pTag1->imageSequence, NULL, 10);
        int sequence2 = simple_strtoul(pTag2->imageSequence, NULL, 10);
        char *p;
        char bootPartition = BOOT_LATEST_IMAGE;

        pNvramData = readNvramData();
        if (pNvramData == NULL)
        {
           return pTag;
        }

        for( p = pNvramData->szBootline; p[2] != '\0'; p++ )
        {
            if( p[0] == 'p' && p[1] == '=' )
            {
                bootPartition = p[2];
                break;
            }
        }

        kfree(pNvramData);

        if( bootPartition == BOOT_LATEST_IMAGE )
            pTag = (sequence2 > sequence1) ? getIMGDEFTagFromPartition(2,curImgSize) : getIMGDEFTagFromPartition(1,curImgSize);
        else /* Boot from the image configured. */
            pTag = (sequence2 < sequence1) ? getIMGDEFTagFromPartition(2,curImgSize) : getIMGDEFTagFromPartition(1,curImgSize);
    }
    else
        /* One image is flashed. */
        pTag = (pTag2) ? getIMGDEFTagFromPartition(2,curImgSize) : getIMGDEFTagFromPartition(1,curImgSize);

    return( pTag );
}
#endif
// Must be called with flashImageMutex held
static void UpdateImageSequenceNumber( unsigned char *imageSequence )
{
    int newImageSequence = 0;
    PFILE_TAG pTag = getTagFromPartition(1);

    if( pTag )
        newImageSequence = simple_strtoul(pTag->imageSequence, NULL, 10);

    pTag = getTagFromPartition(2);
    if(pTag && simple_strtoul(pTag->imageSequence, NULL, 10) > newImageSequence)
        newImageSequence = simple_strtoul(pTag->imageSequence, NULL, 10);

    newImageSequence++;
    sprintf(imageSequence, "%d", newImageSequence);
}

/* Must be called with flashImageMutex held */
static int flashFsKernelImage( unsigned char *imagePtr, int imageLen,
    int flashPartition, int *numPartitions )
{
    int status = 0;
    PFILE_TAG pTag = (PFILE_TAG) imagePtr;
    int rootfsAddr = simple_strtoul(pTag->rootfsAddress, NULL, 10);
    int kernelAddr = simple_strtoul(pTag->kernelAddress, NULL, 10);
#if 1 //__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11
    char *p;
#endif
    char *tagFs = imagePtr;
    unsigned int baseAddr = (unsigned int) flash_get_memptr(0);
    unsigned int totalSize = (unsigned int) flash_get_total_size();
    unsigned int reservedBytesAtEnd;
    unsigned int availableSizeOneImg;
    unsigned int reserveForTwoImages;
    unsigned int availableSizeTwoImgs;
    unsigned int newImgSize = simple_strtoul(pTag->rootfsLen, NULL, 10) +
        simple_strtoul(pTag->kernelLen, NULL, 10) + TAG_BLOCK_LEN;
    PFILE_TAG pCurTag = getBootImageTag();
#if 0 //__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11
    int nCurPartition = getPartitionFromTag( pCurTag );
    int should_yield =
        (flashPartition == 0 || flashPartition == nCurPartition) ? 0 : 1;
#endif
    UINT32 crc;
    unsigned int curImgSize = 0;
#if 1 //__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11
    unsigned int rootfsOffset = (unsigned int) rootfsAddr - IMAGE_BASE - TAG_BLOCK_LEN;
#else
    unsigned int rootfsOffset = (unsigned int) rootfsAddr - IMAGE_BASE - TAG_LEN;
#endif
    FLASH_ADDR_INFO flash_info;
    NVRAM_DATA *pNvramData;
#if defined(MSTC_OBM_IMAGE_DEFAULT) || defined(MSTC_ROM_D) //__MSTC__, Dennis merge from ZyXEL ROM-D feature, zongyue
    PIMAGE_TAG pTag_Img = NULL;
    UINT32 image_len = 0;
    UINT32 TagCrc;
#endif
#ifdef MSTC_OBM_IMAGE_DEFAULT //__MSTC__, Dennis merge from ZyXEL imagedefault feature, zongyue
    char *imgdefPtr = NULL;
#endif
#ifdef MSTC_ROM_D //__MSTC__, Dennis merge from ZyXEL ROM-D feature, zongyue
    UINT32 search_tag_addr = 0;
    char *romdPtr = NULL;
    unsigned char romd_sig = 0;
#endif

    BCM_ASSERT_HAS_MUTEX_C(&flashImageMutex);
#ifdef MSTC_ROM_D //__MSTC__, Dennis merge from ZyXEL ROM-D feature, zongyue
    if (0x1 == pTag->imageNext[0]) {
        search_tag_addr = newImgSize;
        do {
            pTag_Img = (PIMAGE_TAG)(tagFs + search_tag_addr);
            /* 4-byte boundary protection */
            memcpy(&TagCrc, pTag_Img->tagValidationToken, CRC_LEN);
            crc = CRC32_INIT_VALUE;
            crc = getCrc32((unsigned char *)pTag_Img, (UINT32)(IMAGE_TAG_LEN-CRC_LEN), crc);
            if (crc != TagCrc) {
                printk(KERN_ERR "IMAGE_TAG CRC error. Corrupted image? %x!=%x\n",(unsigned int)crc,(unsigned int)TagCrc);
                break;
            }
            if (IMAGE_TYPE_ROMD == simple_strtoul(pTag_Img->imageType, NULL, 10)) {
                romd_sig = simple_strtoul(pTag_Img->imageSignature, NULL, 10);
                image_len = simple_strtoul(pTag_Img->imageLen, NULL, 10);
                romdPtr = tagFs + search_tag_addr + IMAGE_TAG_LEN;
                break;
            }
            else {
                search_tag_addr += (IMAGE_TAG_LEN + simple_strtoul(pTag_Img->imageLen, NULL, 10));
            }
        } while (0x1 == pTag_Img->imageNext[0]);
    }

    if( 0 != romd_sig ) {
        memcpy(&TagCrc, pTag_Img->imageValidationToken, CRC_LEN);
        crc = CRC32_INIT_VALUE;
        crc = getCrc32(romdPtr, (UINT32)image_len, crc);
        if (crc != TagCrc) {
            printk(KERN_ERR "ROM-D CRC error. Corrupted image? %x!=%x\n",(unsigned int)crc,(unsigned int)TagCrc);
        }
        else {
            if (1 == romd_sig) {
                printk(KERN_INFO "attached ROM-D file will drop and erase device ROM-D!\n");
                kerSysRomdErase();
            }
            else if (2 == romd_sig) {
                printk(KERN_INFO "attached ROM-D file will replease device ROM-D!\n");
                kerSysRomdSet(romdPtr, image_len, 0);
            }
            else {
                printk(KERN_INFO "ROM-D signature = %d, donothing!\n", romd_sig);
            }
        }
    }
#endif
#ifdef MSTC_OBM_IMAGE_DEFAULT //__MSTC__, Dennis ZyXEL OBM ImageDefault feature, zongyue
    if (0x1 == pTag->imageNext[0]) {
        /* first attached image is ImageDeafult */
        pTag_Img = (PIMAGE_TAG)(tagFs + newImgSize);
        /* 4-byte boundary protection */
        memcpy(&TagCrc, pTag_Img->tagValidationToken, CRC_LEN);
        crc = CRC32_INIT_VALUE;
        crc = getCrc32((unsigned char *)pTag_Img, (UINT32)(IMAGE_TAG_LEN-CRC_LEN), crc);
        if (crc != TagCrc) {
            printk(KERN_ERR "IMAGE_TAG CRC error, ignore attached ImageDefault image.\n");
            ((PFILE_TAG) tagFs)->imageNext[0] = 0;
        }
        if (IMAGE_TYPE_IMGDEF == simple_strtoul(pTag_Img->imageType, NULL, 10)) {
            /* at now, only ImageDefault will attach behind fs_kernel image in flash */
            /* no other images behine ImageDefault */
            image_len = simple_strtoul(pTag_Img->imageLen, NULL, 10);
            imgdefPtr = tagFs + newImgSize + IMAGE_TAG_LEN;
            /* check ImageDefault image CRC, 4-byte boundary protection */
            memcpy(&TagCrc, pTag_Img->imageValidationToken, CRC_LEN);
            crc = CRC32_INIT_VALUE;
            crc = getCrc32((unsigned char *)(imgdefPtr), (UINT32)image_len, crc);
            if (crc != TagCrc) {
                printk(KERN_ERR "ImageDefault CRC error. Ignore\n");
                ((PFILE_TAG) tagFs)->imageNext[0] = 0;
            }
            else {
                newImgSize += (IMAGE_TAG_LEN + simple_strtoul(pTag_Img->imageLen, NULL, 10));
                pTag_Img->imageNext[0] = 0;
                /* update IMAGE_TAG CRC */
                crc = CRC32_INIT_VALUE;
                crc = getCrc32((unsigned char *)pTag_Img, (UINT32)(IMAGE_TAG_LEN-CRC_LEN), crc);
                /* 4-byte boundary protection */
                memcpy(pTag_Img->tagValidationToken, &crc, CRC_LEN);
            }
        }
    }
#endif //__MSTC__, Dennis ZyXEL OBM ImageDefault feature, zongyue
    if (NULL == (pNvramData = readNvramData()))
    {
        return -ENOMEM;
    }

    kerSysFlashAddrInfoGet(&flash_info);
    if( rootfsOffset < flash_info.flash_rootfs_start_offset )
    {
        // Increase rootfs and kernel addresses by the difference between
        // rootfs offset and what it needs to be.
        rootfsAddr += flash_info.flash_rootfs_start_offset - rootfsOffset;
        kernelAddr += flash_info.flash_rootfs_start_offset - rootfsOffset;
        sprintf(pTag->rootfsAddress,"%lu", (unsigned long) rootfsAddr);
        sprintf(pTag->kernelAddress,"%lu", (unsigned long) kernelAddr);
        crc = CRC32_INIT_VALUE;
        crc = getCrc32((unsigned char *)pTag, (UINT32)TAG_LEN-TOKEN_LEN, crc);
        *(unsigned long *) &pTag->tagValidationToken[0] = crc;
    }

    rootfsAddr += BOOT_OFFSET;
    kernelAddr += BOOT_OFFSET;

    reservedBytesAtEnd = flash_get_reserved_bytes_at_end(&flash_info);
    availableSizeOneImg = totalSize - ((unsigned int) rootfsAddr - baseAddr) -
        reservedBytesAtEnd;
    reserveForTwoImages =
        (flash_info.flash_rootfs_start_offset > reservedBytesAtEnd)
        ? flash_info.flash_rootfs_start_offset : reservedBytesAtEnd;
    availableSizeTwoImgs = (totalSize / 2) - reserveForTwoImages;

    //    printk("availableSizeOneImage=%dKB availableSizeTwoImgs=%dKB reserve=%dKB\n",
    //            availableSizeOneImg/1024, availableSizeTwoImgs/1024, reserveForTwoImages/1024);
    if( pCurTag )
    {
#if 1 //__MSTC__, Dennis merge from Autumn
        curImgSize = simple_strtoul(pCurTag->rootfsLen, NULL, 10) +
                   simple_strtoul(pCurTag->kernelLen, NULL, 10) + TAG_BLOCK_LEN;
#ifdef MSTC_OBM_IMAGE_DEFAULT //__MSTC__, Dennis ZyXEL OBM ImageDefault feature, zongyue
    if (0x1 == pCurTag->imageNext[0]) {
	    pTag_Img = getBootImageDefTag( curImgSize );
        if ( pTag_Img ) {
            /* 4-byte boundary protection */
            if (IMAGE_TYPE_IMGDEF == simple_strtoul(pTag_Img->imageType, NULL, 10)) {
                /* at now, only ImageDefault will attach behind fs_kernel image in flash */
                /* no other images behine ImageDefault */
                curImgSize += (IMAGE_TAG_LEN + simple_strtoul(pTag_Img->imageLen, NULL, 10));
            }
        }
    }
#endif //__MSTC__, Dennis ZyXEL OBM ImageDefault feature, zongyue
#else
        curImgSize = simple_strtoul(pCurTag->rootfsLen, NULL, 10) +
            simple_strtoul(pCurTag->kernelLen, NULL, 10);
#endif
    }

    if( newImgSize > availableSizeOneImg)
    {
        printk("Illegal image size %d.  Image size must not be greater "
            "than %d.\n", newImgSize, availableSizeOneImg);
        kfree(pNvramData);
        return -1;
    }

#if 0 //__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11
    *numPartitions = (curImgSize <= availableSizeTwoImgs &&
         newImgSize <= availableSizeTwoImgs &&
         flashPartition != nCurPartition) ? 2 : 1;
#endif

    // If the current image fits in half the flash space and the new
    // image to flash also fits in half the flash space, then flash it
    // in the partition that is not currently being used to boot from.
#if 1 //__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11

#ifdef MTSC_DUAL_IMAGE_SELECT
	//if first partition is choose, do not change to partition two
	if( pNvramData->partitionNum != '0' )
	{
		if( curImgSize <= availableSizeTwoImgs &&
			newImgSize <= availableSizeTwoImgs &&
			(getPartitionFromTag( pCurTag ) == 1 || pNvramData->partitionNum == '1') )
#else
    if( curImgSize <= availableSizeTwoImgs &&
        newImgSize <= availableSizeTwoImgs &&
        getPartitionFromTag( pCurTag ) == 1 )
#endif
		
#else
    if( curImgSize <= availableSizeTwoImgs &&
        newImgSize <= availableSizeTwoImgs &&
        ((nCurPartition == 1 && flashPartition != 1) || flashPartition == 2) )
#endif
    {
        // Update rootfsAddr to point to the second boot partition.
#if 1 //__MSTC__, Paul Ho: Support 963268 nand flash, patch form SVN#3781 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11
        int offset = 0;
        int sblk = 0; 
        int rblk = 0; //if rblk is not equal to sblk, it means block #512 is a bad block
                      //we need to shift our boot address to skip bad block
        offset = (totalSize / 2) + TAG_BLOCK_LEN;
        sblk = flash_get_blk((int) (IMAGE_BASE + (totalSize / 2) + BOOT_OFFSET));
        rblk = getNandBlock( (unsigned long) (IMAGE_BASE + (totalSize / 2) + BOOT_OFFSET), 0 );
        offset += (rblk - sblk)*128*1024;
#else
        int offset = (totalSize / 2) + TAG_LEN;
#endif
#if (INC_NAND_FLASH_DRIVER==1) //__MSTC__, Dennis
        if( flash_get_flash_type() !=  FLASH_IFC_NAND )
        {
#endif
        sprintf(((PFILE_TAG) tagFs)->kernelAddress, "%lu",
            (unsigned long) IMAGE_BASE + offset + (kernelAddr - rootfsAddr));
        kernelAddr = baseAddr + offset + (kernelAddr - rootfsAddr);
#if (INC_NAND_FLASH_DRIVER==1) //__MSTC__, Dennis
        }
#endif
        sprintf(((PFILE_TAG) tagFs)->rootfsAddress, "%lu",
            (unsigned long) IMAGE_BASE + offset);
        rootfsAddr = baseAddr + offset;
    }

#ifdef MTSC_DUAL_IMAGE_SELECT
	}
	if( pNvramData->partitionNum != '0' && pNvramData->partitionNum != '1' )
	{
    UpdateImageSequenceNumber( ((PFILE_TAG) tagFs)->imageSequence );
	}
#else
    UpdateImageSequenceNumber( ((PFILE_TAG) tagFs)->imageSequence );
#endif
    crc = CRC32_INIT_VALUE;
    crc = getCrc32((unsigned char *)tagFs, (UINT32)TAG_LEN-TOKEN_LEN, crc);
    *(unsigned long *) &((PFILE_TAG) tagFs)->tagValidationToken[0] = crc;
	#ifdef CONFIG_MSTC_WDT
	/* Stop hardware watchdog */
	mstc_wdt_exit();
	#endif

#if 1 //__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11

#ifdef MTSC_NORWAY_CUSTOMIZATION
	if( (status = kerSysBcmImageSet((rootfsAddr-TAG_BLOCK_LEN), tagFs,
        newImgSize, 0)) != 0 )
#else
    if( (status = kerSysBcmImageSet((rootfsAddr-TAG_BLOCK_LEN), tagFs,
        newImgSize)) != 0 )
#endif
		
#else
    if( (status = kerSysBcmImageSet((rootfsAddr-TAG_LEN), tagFs,
        TAG_LEN + newImgSize, should_yield)) != 0 )
#endif
    {
        printk("Failed to flash root file system. Error: %d\n", status);
        kfree(pNvramData);
        return status;
    }

#if 1 //__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11
    for( p = pNvramData->szBootline; p[2] != '\0'; p++ )
    {
        if( p[0] == 'p' && p[1] == '=' && p[2] != BOOT_LATEST_IMAGE )
        {
            // Change boot partition to boot from new image.
            p[2] = BOOT_LATEST_IMAGE;
            writeNvramDataCrcLocked(pNvramData);
            break;
        }
    }
#endif
    kfree(pNvramData);
    return(status);
}

#ifdef MTSC_NORWAY_CUSTOMIZATION
static int flashFsKernelImageForTr69( unsigned char *imagePtr, int imageLen,
    int flashPartition, int *numPartitions )
{
    int status = 0;
    PFILE_TAG pTag = (PFILE_TAG) imagePtr;
    int rootfsAddr = simple_strtoul(pTag->rootfsAddress, NULL, 10);
    int kernelAddr = simple_strtoul(pTag->kernelAddress, NULL, 10);

    char *tagFs = imagePtr;
    unsigned int baseAddr = (unsigned int) flash_get_memptr(0);
    unsigned int totalSize = (unsigned int) flash_get_total_size();
    unsigned int reservedBytesAtEnd;
    unsigned int availableSizeOneImg;
    unsigned int reserveForTwoImages;
    unsigned int availableSizeTwoImgs;
    unsigned int newImgSize = simple_strtoul(pTag->rootfsLen, NULL, 10) +
        simple_strtoul(pTag->kernelLen, NULL, 10) + TAG_BLOCK_LEN;
    PFILE_TAG pCurTag = getBootImageTag();

	int should_yield = ( flashPartition == 0 ) ? 0 : 1;
	
    UINT32 crc;
    unsigned int curImgSize = 0;
#if 1 //__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11
    unsigned int rootfsOffset = (unsigned int) rootfsAddr - IMAGE_BASE - TAG_BLOCK_LEN;
#else
    unsigned int rootfsOffset = (unsigned int) rootfsAddr - IMAGE_BASE - TAG_LEN;
#endif
    FLASH_ADDR_INFO flash_info;
    NVRAM_DATA *pNvramData;
#if defined(MSTC_OBM_IMAGE_DEFAULT) || defined(MSTC_ROM_D) //__MSTC__, Dennis merge from ZyXEL ROM-D feature, zongyue
    PIMAGE_TAG pTag_Img = NULL;
    UINT32 image_len = 0;
    UINT32 TagCrc;
#endif
#ifdef MSTC_OBM_IMAGE_DEFAULT //__MSTC__, Dennis merge from ZyXEL imagedefault feature, zongyue
    char *imgdefPtr = NULL;
#endif
#ifdef MSTC_ROM_D //__MSTC__, Dennis merge from ZyXEL ROM-D feature, zongyue
    UINT32 search_tag_addr = 0;
    char *romdPtr = NULL;
    unsigned char romd_sig = 0;
#endif

    BCM_ASSERT_HAS_MUTEX_C(&flashImageMutex);
#ifdef MSTC_ROM_D //__MSTC__, Dennis merge from ZyXEL ROM-D feature, zongyue
    if (0x1 == pTag->imageNext[0]) {
        search_tag_addr = newImgSize;
        do {
            pTag_Img = (PIMAGE_TAG)(tagFs + search_tag_addr);
            /* 4-byte boundary protection */
            memcpy(&TagCrc, pTag_Img->tagValidationToken, CRC_LEN);
            crc = CRC32_INIT_VALUE;
            crc = getCrc32((unsigned char *)pTag_Img, (UINT32)(IMAGE_TAG_LEN-CRC_LEN), crc);
            if (crc != TagCrc) {
                printk(KERN_ERR "IMAGE_TAG CRC error. Corrupted image? %x!=%x\n",(unsigned int)crc,(unsigned int)TagCrc);
                break;
            }
            if (IMAGE_TYPE_ROMD == simple_strtoul(pTag_Img->imageType, NULL, 10)) {
                romd_sig = simple_strtoul(pTag_Img->imageSignature, NULL, 10);
                image_len = simple_strtoul(pTag_Img->imageLen, NULL, 10);
                romdPtr = tagFs + search_tag_addr + IMAGE_TAG_LEN;
                break;
            }
            else {
                search_tag_addr += (IMAGE_TAG_LEN + simple_strtoul(pTag_Img->imageLen, NULL, 10));
            }
        } while (0x1 == pTag_Img->imageNext[0]);
    }
	
    if( 0 != romd_sig ) {
        memcpy(&TagCrc, pTag_Img->imageValidationToken, CRC_LEN);
        crc = CRC32_INIT_VALUE;
        crc = getCrc32(romdPtr, (UINT32)image_len, crc);
        if (crc != TagCrc) {
            printk(KERN_ERR "ROM-D CRC error. Corrupted image? %x!=%x\n",(unsigned int)crc,(unsigned int)TagCrc);
        }
        else {
            if (1 == romd_sig) {
                printk(KERN_INFO "attached ROM-D file will drop and erase device ROM-D!\n");
                kerSysRomdErase();
            }
            else if (2 == romd_sig) {
                printk(KERN_INFO "attached ROM-D file will replease device ROM-D!\n");
                kerSysRomdSet(romdPtr, image_len, 0);
            }
            else {
                printk(KERN_INFO "ROM-D signature = %d, donothing!\n", romd_sig);
            }
        }
    }
#endif
#ifdef MSTC_OBM_IMAGE_DEFAULT //__MSTC__, Dennis ZyXEL OBM ImageDefault feature, zongyue
    if (0x1 == pTag->imageNext[0]) {
        /* first attached image is ImageDeafult */
        pTag_Img = (PIMAGE_TAG)(tagFs + newImgSize);
        /* 4-byte boundary protection */
        memcpy(&TagCrc, pTag_Img->tagValidationToken, CRC_LEN);
        crc = CRC32_INIT_VALUE;
        crc = getCrc32((unsigned char *)pTag_Img, (UINT32)(IMAGE_TAG_LEN-CRC_LEN), crc);
        if (crc != TagCrc) {
            printk(KERN_ERR "IMAGE_TAG CRC error, ignore attached ImageDefault image.\n");
            ((PFILE_TAG) tagFs)->imageNext[0] = 0;
        }
        if (IMAGE_TYPE_IMGDEF == simple_strtoul(pTag_Img->imageType, NULL, 10)) {
            /* at now, only ImageDefault will attach behind fs_kernel image in flash */
            /* no other images behine ImageDefault */
            image_len = simple_strtoul(pTag_Img->imageLen, NULL, 10);
            imgdefPtr = tagFs + newImgSize + IMAGE_TAG_LEN;
            /* check ImageDefault image CRC, 4-byte boundary protection */
            memcpy(&TagCrc, pTag_Img->imageValidationToken, CRC_LEN);
            crc = CRC32_INIT_VALUE;
            crc = getCrc32((unsigned char *)(imgdefPtr), (UINT32)image_len, crc);
            if (crc != TagCrc) {
                printk(KERN_ERR "ImageDefault CRC error. Ignore\n");
                ((PFILE_TAG) tagFs)->imageNext[0] = 0;
            }
            else {
                newImgSize += (IMAGE_TAG_LEN + simple_strtoul(pTag_Img->imageLen, NULL, 10));
                pTag_Img->imageNext[0] = 0;
                /* update IMAGE_TAG CRC */
                crc = CRC32_INIT_VALUE;
                crc = getCrc32((unsigned char *)pTag_Img, (UINT32)(IMAGE_TAG_LEN-CRC_LEN), crc);
                /* 4-byte boundary protection */
                memcpy(pTag_Img->tagValidationToken, &crc, CRC_LEN);
            }
        }
    }
#endif //__MSTC__, Dennis ZyXEL OBM ImageDefault feature, zongyue
    if (NULL == (pNvramData = readNvramData()))
    {
        return -ENOMEM;
    }

    kerSysFlashAddrInfoGet(&flash_info);
    if( rootfsOffset < flash_info.flash_rootfs_start_offset )
    {
        // Increase rootfs and kernel addresses by the difference between
        // rootfs offset and what it needs to be.
        rootfsAddr += flash_info.flash_rootfs_start_offset - rootfsOffset;
        kernelAddr += flash_info.flash_rootfs_start_offset - rootfsOffset;
        sprintf(pTag->rootfsAddress,"%lu", (unsigned long) rootfsAddr);
        sprintf(pTag->kernelAddress,"%lu", (unsigned long) kernelAddr);
        crc = CRC32_INIT_VALUE;
        crc = getCrc32((unsigned char *)pTag, (UINT32)TAG_LEN-TOKEN_LEN, crc);
        *(unsigned long *) &pTag->tagValidationToken[0] = crc;
    }

    rootfsAddr += BOOT_OFFSET;
    kernelAddr += BOOT_OFFSET;

    reservedBytesAtEnd = flash_get_reserved_bytes_at_end(&flash_info);
    availableSizeOneImg = totalSize - ((unsigned int) rootfsAddr - baseAddr) -
        reservedBytesAtEnd;
    reserveForTwoImages =
        (flash_info.flash_rootfs_start_offset > reservedBytesAtEnd)
        ? flash_info.flash_rootfs_start_offset : reservedBytesAtEnd;
    availableSizeTwoImgs = (totalSize / 2) - reserveForTwoImages;

    //    printk("availableSizeOneImage=%dKB availableSizeTwoImgs=%dKB reserve=%dKB\n",
    //            availableSizeOneImg/1024, availableSizeTwoImgs/1024, reserveForTwoImages/1024);
    if( pCurTag )
    {
#if 1 //__MSTC__, Dennis merge from Autumn
        curImgSize = simple_strtoul(pCurTag->rootfsLen, NULL, 10) +
                   simple_strtoul(pCurTag->kernelLen, NULL, 10) + TAG_BLOCK_LEN;
#else
        curImgSize = simple_strtoul(pCurTag->rootfsLen, NULL, 10) +
            simple_strtoul(pCurTag->kernelLen, NULL, 10);
#endif
    }

    if( newImgSize > availableSizeOneImg)
    {
        printk("Illegal image size %d.  Image size must not be greater "
            "than %d.\n", newImgSize, availableSizeOneImg);
        kfree(pNvramData);
        return -1;
    }

    // If the current image fits in half the flash space and the new
    // image to flash also fits in half the flash space, then flash it
    // in the partition that is not currently being used to boot from.
#if 1 //__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11

#ifdef MTSC_DUAL_IMAGE_SELECT
	//if first partition is choose, do not change to partition two
	if( pNvramData->partitionNum != '0' || flashPartition == 2 )
		if( curImgSize <= availableSizeTwoImgs &&
			newImgSize <= availableSizeTwoImgs &&
			(getPartitionFromTag( pCurTag ) == 1 || pNvramData->partitionNum == '1' || flashPartition == 2) )
#else
    if( curImgSize <= availableSizeTwoImgs &&
        newImgSize <= availableSizeTwoImgs &&
        getPartitionFromTag( pCurTag ) == 1 )
#endif
		
#else
    if( curImgSize <= availableSizeTwoImgs &&
        newImgSize <= availableSizeTwoImgs &&
        ((nCurPartition == 1 && flashPartition != 1) || flashPartition == 2) )
#endif
    {
        // Update rootfsAddr to point to the second boot partition.
#if 1 //__MSTC__, Paul Ho: Support 963268 nand flash, patch form SVN#3781 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11
        int offset = 0;
        int sblk = 0; 
        int rblk = 0; //if rblk is not equal to sblk, it means block #512 is a bad block
                      //we need to shift our boot address to skip bad block
        offset = (totalSize / 2) + TAG_BLOCK_LEN;
        sblk = flash_get_blk((int) (IMAGE_BASE + (totalSize / 2) + BOOT_OFFSET));
        rblk = getNandBlock( (unsigned long) (IMAGE_BASE + (totalSize / 2) + BOOT_OFFSET), 0 );
        offset += (rblk - sblk)*128*1024;
#else
        int offset = (totalSize / 2) + TAG_LEN;
#endif
#if (INC_NAND_FLASH_DRIVER==1) //__MSTC__, Dennis
        if( flash_get_flash_type() !=  FLASH_IFC_NAND )
        {
#endif
        sprintf(((PFILE_TAG) tagFs)->kernelAddress, "%lu",
            (unsigned long) IMAGE_BASE + offset + (kernelAddr - rootfsAddr));
        kernelAddr = baseAddr + offset + (kernelAddr - rootfsAddr);
#if (INC_NAND_FLASH_DRIVER==1) //__MSTC__, Dennis
        }
#endif
        sprintf(((PFILE_TAG) tagFs)->rootfsAddress, "%lu",
            (unsigned long) IMAGE_BASE + offset);
        rootfsAddr = baseAddr + offset;
    }

    crc = CRC32_INIT_VALUE;
    crc = getCrc32((unsigned char *)tagFs, (UINT32)TAG_LEN-TOKEN_LEN, crc);
    *(unsigned long *) &((PFILE_TAG) tagFs)->tagValidationToken[0] = crc;
	#ifdef CONFIG_MSTC_WDT
	/* Stop hardware watchdog */
	//mstc_wdt_exit(); //marcus
	#endif

#if 1 //__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11

    if( (status = kerSysBcmImageSet((rootfsAddr-TAG_BLOCK_LEN), tagFs,
        newImgSize, should_yield)) != 0 )	
#else
    if( (status = kerSysBcmImageSet((rootfsAddr-TAG_LEN), tagFs,
        TAG_LEN + newImgSize, should_yield)) != 0 )
#endif
	
    {
        printk("Failed to flash root file system. Error: %d\n", status);
        kfree(pNvramData);
        return status;
    }

    kfree(pNvramData);
    return(status);
}
#endif

#ifdef MSTC_FWUP_FROM_FILE //__MSTC__, LingChun
static int flashWholeImageFile(char *filename, unsigned int file_offset, unsigned int size, int ctrl_offset)
{
	int status = 0;
	struct file *fp = NULL;
	mm_segment_t fs;
	int len;
	int blk_start;
	int sect_size;
	int start_addr;
	char *string = NULL;
	int badblknum; //CLK
	int ret;
	//int count = 0;
	int whole_image=0;

	if(filename == NULL) {
		return -1;
	}	

	printk("flashWholeImageFile__ filename=%s, ctrl_offset=0x%x, file_offset=0x%x, size=%d \n",filename,ctrl_offset,file_offset,size);


	BCM_ASSERT_HAS_MUTEX_C(&flashImageMutex);

	fp = filp_open(filename, O_RDWR, (S_IRUSR | S_IWUSR));
	if (!IS_ERR(fp) && fp->f_op && fp->f_op->read && fp->f_op->write && fp->f_op->llseek) {
		fs = get_fs();
		set_fs(get_ds());
	} else {
		return -1;
	}


	start_addr = ctrl_offset;
	blk_start = flash_get_blk(start_addr);
	sect_size = flash_get_sector_size(blk_start);

	string = kmalloc(sect_size, GFP_ATOMIC);
	if(string == NULL) {
		if(fp) {
			filp_close(fp, NULL);
		}
		return -1;
	}						

	len = size;
	
	ret = fp->f_op->llseek(fp, file_offset, 0);	

	while(len > 0) {
		ret = fp->f_op->read(fp, (void *)string, sect_size, &fp->f_pos);
		if((ret > 0) && (ret <= sect_size)) {
			badblknum = 0;
			status = kerSysBcmImageFileSet(start_addr, string, ret, &badblknum); //CLK
			if(status != 0) {
				break;
			}
			len -= ret;
			start_addr += ret + (sect_size*badblknum);
		} else {
			status = -1;
			break;
		}

	}
	
	printk("\n\n");

	if (ctrl_offset == FLASH_BASE){  // If flashing a whole image, erase to end of flash.
		whole_image = 1;			
		blk_start= flash_get_blk(start_addr);
		blk_start++;
		kerSysEraseFlashForFile(blk_start, size, whole_image);
	}

	if(string) {
		kfree(string);
		string = NULL;
	}

	if(fp) {
		filp_close(fp, NULL);
	}

	return(status);
}
#endif


#ifdef MSTC_FWUP_FROM_FILE //__MSTC__, LingChun, merge MSTC_FWUP_FROM_FLASH from telefonica, http://svn.zyxel.com.tw/svn/CPE_TRUNK/BRCM_412/Telefonica_Common/
static int flashFsKernelImageFile(char *filename, unsigned int offset, unsigned int size)
{
	int status = 0;
	FILE_TAG Tag;
	PFILE_TAG pTag = &Tag;
	int rootfsAddr = 0;
	int kernelAddr = 0;
	char *p;
	unsigned int baseAddr = (unsigned int) flash_get_memptr(0);
	unsigned int totalSize = (unsigned int) flash_get_total_size();
	//unsigned int reservedBytesAtEnd;
	unsigned int availableSizeOneImg;
	//unsigned int reserveForTwoImages;
	unsigned int availableSizeTwoImgs;
	unsigned int newImgSize = 0;
	PFILE_TAG pCurTag = getBootImageTag();
	UINT32 crc;
	unsigned int curImgSize = 0;
	unsigned int rootfsOffset = 0;
	FLASH_ADDR_INFO flash_info;
	NVRAM_DATA *pNvramData;
#if defined(MSTC_OBM_IMAGE_DEFAULT) || defined(MSTC_ROM_D)  /* Image Default  */
	IMAGE_TAG Tag_Img;
	PIMAGE_TAG pTag_Img = &Tag_Img;
	UINT32 image_len = 0;
	UINT32 TagCrc;
#endif
	UINT32 search_tag_addr = 0;

	struct file *fp = NULL;
	mm_segment_t fs;
	int i;
	char c;
	int len;
	int blk_start;
	int sect_size;
	int start_addr;
	char *string = NULL;
	int badblknum; //CLK
	int ret;
	int count = 0;

	if(filename == NULL) {
		return -1;
	}

	memset(pTag, 0, sizeof(FILE_TAG));
	memset(pTag_Img, 0, sizeof(IMAGE_TAG));

	BCM_ASSERT_HAS_MUTEX_C(&flashImageMutex);

	fp = filp_open(filename, O_RDWR, (S_IRUSR | S_IWUSR));
	if (!IS_ERR(fp) && fp->f_op && fp->f_op->read && fp->f_op->write && fp->f_op->llseek) {
		fs = get_fs();
		set_fs(get_ds());
		
		ret = fp->f_op->llseek(fp, offset, 0);
		ret = fp->f_op->read(fp, (void *)pTag, sizeof(FILE_TAG), &fp->f_pos);
	} else {
		return -1;
	}

	rootfsAddr = simple_strtoul(pTag->rootfsAddress, NULL, 10);
	kernelAddr = simple_strtoul(pTag->kernelAddress, NULL, 10);
	rootfsOffset = (unsigned int) rootfsAddr - IMAGE_BASE - TAG_BLOCK_LEN;
	newImgSize = simple_strtoul(pTag->rootfsLen, NULL, 10) +
#if defined(MSTC_OBM_IMAGE_DEFAULT) || defined(MSTC_ROM_D)  /* Image Default  */
		simple_strtoul(pTag->kernelLen, NULL, 10) + TAG_BLOCK_LEN;
#else
		simple_strtoul(pTag->kernelLen, NULL, 10);
#endif

#if defined(MSTC_OBM_IMAGE_DEFAULT)   /* Image Default  */
	if (0x1 == pTag->imageNext[0]) {				
		search_tag_addr = newImgSize + offset;

		ret = fp->f_op->llseek(fp, search_tag_addr, 0);
		ret = fp->f_op->read(fp, (void *)pTag_Img, sizeof(IMAGE_TAG), &fp->f_pos);

		/* 4-byte boundary protection */
		//printk(KERN_ERR "IMAGE_TAG %8.8lx\n", search_tag_addr);
		memcpy(&TagCrc, pTag_Img->tagValidationToken, CRC_LEN);
		crc = CRC32_INIT_VALUE;
		crc = getCrc32((unsigned char *)pTag_Img, (UINT32)(IMAGE_TAG_LEN-CRC_LEN), crc);

		if (crc != TagCrc) {
			printk(KERN_ERR "IMAGE_TAG CRC error, ignore attached ImageDefault image.\n");
  			pTag->imageNext[0] = 0;
		}
		if (IMAGE_TYPE_IMGDEF == simple_strtoul(pTag_Img->imageType, NULL, 10)) {		printk("__(%s@%d)__  IMAGE_TYPE_IMGDEF \n",__FUNCTION__,__LINE__);
			/* at now, only ImageDefault will attach behind fs_kernel image in flash */
 			/* no other images behine ImageDefault */
			image_len = simple_strtoul(pTag_Img->imageLen, NULL, 10);
			/* check ImageDefault image CRC, 4-byte boundary protection */
			memcpy(&TagCrc, pTag_Img->imageValidationToken, CRC_LEN);
			crc = CRC32_INIT_VALUE;
			ret = fp->f_op->llseek(fp, (newImgSize + offset + IMAGE_TAG_LEN), 0);
			i = image_len;
			#if 1 /* Need to enhance CRC check time */
			while(i-- > 0) {
				fp->f_op->read(fp, (void *)&c, 1, &fp->f_pos);
				crc = getCrc32(&c, 1, crc);
			}
			#else
			tmpBuf = kmalloc(TMP_BUF_SIZE, GFP_KERNEL);
			if(tmpBuf == NULL) {
				if(fp) {
					filp_close(fp, NULL);
					fp = NULL;
				}
				return -ENOMEM;
			}
			while(i > 0) {
				ret = fp->f_op->read(fp, (void *)tmpBuf, TMP_BUF_SIZE, &fp->f_pos);
				crc = getCrc32(tmpBuf, ret, crc);
				i -= ret;
			}
			if(tmpBuf) {
				kfree(tmpBuf);
			}
			#endif
			if (crc != TagCrc) {
				printk(KERN_ERR "ImageDefault CRC error. Ignore\n");
				pTag->imageNext[0] = 0;
			} else {						
				newImgSize += (IMAGE_TAG_LEN + simple_strtoul(pTag_Img->imageLen, NULL, 10));
				pTag_Img->imageNext[0] = 0;
				/* update IMAGE_TAG CRC */
				crc = CRC32_INIT_VALUE;
				crc = getCrc32((unsigned char *)pTag_Img, (UINT32)(IMAGE_TAG_LEN-CRC_LEN), crc);
				/* 4-byte boundary protection */
				memcpy(pTag_Img->tagValidationToken, &crc, CRC_LEN);
#if 1 
				ret = fp->f_op->llseek(fp, search_tag_addr, 0);
				ret = fp->f_op->write(fp, (void *)pTag_Img, sizeof(IMAGE_TAG), &fp->f_pos);
#endif				
			}
		}
	}
#endif

	if (NULL == (pNvramData = readNvramData())) {	
		if(fp) {
			filp_close(fp, NULL);
			fp = NULL;
		}
		return -ENOMEM;
	}	

	kerSysFlashAddrInfoGet(&flash_info);
	if( rootfsOffset < flash_info.flash_rootfs_start_offset ) {
		// Increase rootfs and kernel addresses by the difference between
		// rootfs offset and what it needs to be.
		rootfsAddr += flash_info.flash_rootfs_start_offset - rootfsOffset;
		kernelAddr += flash_info.flash_rootfs_start_offset - rootfsOffset;
		sprintf(pTag->rootfsAddress,"%lu", (unsigned long) rootfsAddr);
		sprintf(pTag->kernelAddress,"%lu", (unsigned long) kernelAddr);
		crc = CRC32_INIT_VALUE;
		crc = getCrc32((unsigned char *)pTag, (UINT32)TAG_LEN-TOKEN_LEN, crc);
		*(unsigned long *) &pTag->tagValidationToken[0] = crc;
	}

	rootfsAddr += BOOT_OFFSET;
	kernelAddr += BOOT_OFFSET;

	#if 1
	availableSizeOneImg = flash_info.flash_rootfs_max_size;
	availableSizeTwoImgs = flash_info.flash_rootfs_max_size;
	#else
	reservedBytesAtEnd = flash_get_reserved_bytes_at_end(&flash_info);
	availableSizeOneImg = totalSize - ((unsigned int) rootfsAddr - baseAddr) - reservedBytesAtEnd;
	reserveForTwoImages =
		(flash_info.flash_rootfs_start_offset > reservedBytesAtEnd)
		? flash_info.flash_rootfs_start_offset : reservedBytesAtEnd;
	availableSizeTwoImgs = (totalSize / 2) - reserveForTwoImages;
	#endif

//	    printk("availableSizeOneImage=%dKB availableSizeTwoImgs=%dKB reserve=%dKB\n",
//	            availableSizeOneImg/1024, availableSizeTwoImgs/1024, reserveForTwoImages/1024);
	if( pCurTag ) {
		curImgSize = simple_strtoul(pCurTag->rootfsLen, NULL, 10) +
			simple_strtoul(pCurTag->kernelLen, NULL, 10) + TAG_BLOCK_LEN;
#if defined(MSTC_OBM_IMAGE_DEFAULT)   /* Image Default  */
		if (0x1 == pCurTag->imageNext[0]) {
			pTag_Img = getBootImageDefTag( curImgSize );
			if ( pTag_Img ) {
				/* 4-byte boundary protection */
				if (IMAGE_TYPE_IMGDEF == simple_strtoul(pTag_Img->imageType, NULL, 10)) {
					/* at now, only ImageDefault will attach behind fs_kernel image in flash */
					/* no other images behine ImageDefault */
					curImgSize += (IMAGE_TAG_LEN + simple_strtoul(pTag_Img->imageLen, NULL, 10));
				}
			}
		}
#endif
	}					

	if( newImgSize > availableSizeOneImg) {
		printk("Illegal image size %d.  Image size must not be greater "
			"than %d.\n", newImgSize, availableSizeOneImg);
		kfree(pNvramData);
		if(fp) {
			filp_close(fp, NULL);
		}
		return -1;
	}			

    // If the current image fits in half the flash space and the new
    // image to flash also fits in half the flash space, then flash it
    // in the partition that is not currently being used to boot from.
	if( curImgSize <= availableSizeTwoImgs &&
		newImgSize <= availableSizeTwoImgs &&
		getPartitionFromTag( pCurTag ) == 1 )
	{
		// Update rootfsAddr to point to the second boot partition.
		int soffset = 0;
		int sblk = 0; 
		int rblk = 0;	//if rblk is not equal to sblk, it means block #512 is a bad block
						//we need to shift our boot address to skip bad block
		soffset = (totalSize / 2) + TAG_BLOCK_LEN;
		sblk = flash_get_blk((int) (IMAGE_BASE + (totalSize / 2) + BOOT_OFFSET));
		rblk = getNandBlock( (unsigned long) (IMAGE_BASE + (totalSize / 2) + BOOT_OFFSET), 0 );
		soffset += (rblk - sblk)*128*1024;
#if (INC_NAND_FLASH_DRIVER==1) //__MSTC__, Dennis
		if( flash_get_flash_type() !=  FLASH_IFC_NAND ) {	
#endif
			sprintf(pTag->kernelAddress, "%lu", (unsigned long) IMAGE_BASE + soffset + (kernelAddr - rootfsAddr));
			kernelAddr = baseAddr + soffset + (kernelAddr - rootfsAddr);			
#if (INC_NAND_FLASH_DRIVER==1) //__MSTC__, Dennis
		}
#endif
		sprintf(pTag->rootfsAddress, "%lu", (unsigned long) IMAGE_BASE + soffset);
		rootfsAddr = baseAddr + soffset;							printk("__(%s@%d)__  offset=%d,soffset=%d,baseAddr=0x%x,rootfsAddr=0x%x,kernelAddr=0x%x \n",__FUNCTION__,__LINE__,offset,soffset,baseAddr,rootfsAddr,kernelAddr);
	}									printk("__(%s@%d)__  offset=%d, baseAddr=0x%x,rootfsAddr=0x%x,kernelAddr=0x%x \n",__FUNCTION__,__LINE__,offset,baseAddr,rootfsAddr,kernelAddr);

	UpdateImageSequenceNumber( pTag->imageSequence );
	crc = CRC32_INIT_VALUE;
	crc = getCrc32((unsigned char *)pTag, (UINT32)TAG_LEN-TOKEN_LEN, crc);
	*(unsigned long *) &pTag->tagValidationToken[0] = crc;

	ret = fp->f_op->llseek(fp, offset, 0);							
	ret = fp->f_op->write(fp, (void *)pTag, sizeof(FILE_TAG), &fp->f_pos);

	start_addr = (rootfsAddr - TAG_BLOCK_LEN);
	blk_start = flash_get_blk(start_addr);
	sect_size = flash_get_sector_size(blk_start);					printk("__(%s@%d)__  kerSysBcmImageSet: flash_start_addr=0x%x, newImgSize=%d, offset=%d   \n",__FUNCTION__,__LINE__,start_addr,newImgSize,offset);

	string = kmalloc(sect_size, GFP_ATOMIC);
	if(string == NULL) {
		if(fp) {
			filp_close(fp, NULL);
		}
		return -1;
	}

	#if defined(MSTC_OBM_IMAGE_DEFAULT) || defined(MSTC_ROM_D)  /* Image Default  */	
	len = newImgSize;
	#else
	len = (TAG_BLOCK_LEN + newImgSize);
	#endif
	
	ret = fp->f_op->llseek(fp, offset, 0);			

	#ifdef CONFIG_MSTC_WDT
	/* Stop hardware watchdog */
	mstc_wdt_exit();							
	#endif

	while(len > 0) {
		ret = fp->f_op->read(fp, (void *)string, sect_size, &fp->f_pos);
		if((ret > 0) && (ret <= sect_size)) {
			badblknum = 0;
			status = kerSysBcmImageFileSet(start_addr, string, ret, &badblknum); //CLK
			if(status != 0) {
				break;
			}
			len -= ret;
			start_addr += ret + (sect_size*badblknum);
		} else {
			status = -1;
			break;
		}
		count++;
	}
	printk("\n\n");						

	if(string) {
		kfree(string);
		string = NULL;
	}

	for( p = pNvramData->szBootline; p[2] != '\0'; p++ ) {
		if( p[0] == 'p' && p[1] == '=' && p[2] != BOOT_LATEST_IMAGE ) {
			// Change boot partition to boot from new image.
			p[2] = BOOT_LATEST_IMAGE;
			writeNvramDataCrcLocked(pNvramData);
			break;
		}
	}

	kfree(pNvramData);
	if(fp) {
		filp_close(fp, NULL);
	}
	return(status);
}
#endif

#if 0 //__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11
static int getImageVersion( int imageNumber, char *verStr, int verStrSize)
{
    int ret = 0; /* zero bytes copied to verStr so far */

    PFILE_TAG pTag = NULL;

    if( imageNumber == 1 )
        pTag = getTagFromPartition(1);
    else
        if( imageNumber == 2 )
            pTag = getTagFromPartition(2);

    if( pTag )
    {
        if( verStrSize > sizeof(pTag->imageVersion) )
            ret = sizeof(pTag->imageVersion);
        else
            ret = verStrSize;

        memcpy(verStr, pTag->imageVersion, ret);
    }

    return( ret );
}
#endif

PFILE_TAG kerSysUpdateTagSequenceNumber(int imageNumber)
{
    PFILE_TAG pTag = NULL;
    UINT32 crc;

    switch( imageNumber )
    {
    case 0:
        pTag = getBootImageTag();
        break;

    case 1:
        pTag = getTagFromPartition(1);
        break;

    case 2:
        pTag = getTagFromPartition(2);
        break;

    default:
        break;
    }

    if( pTag )
    {
        UpdateImageSequenceNumber( pTag->imageSequence );
        crc = CRC32_INIT_VALUE;
        crc = getCrc32((unsigned char *)pTag, (UINT32)TAG_LEN-TOKEN_LEN, crc);
        *(unsigned long *) &pTag->tagValidationToken[0] = crc;
    }

    return(pTag);
}

int kerSysGetSequenceNumber(int imageNumber)
{
    int seqNumber = -1;
    unsigned long rootfs_ofs;
    if( kerSysBlParmsGetInt(NAND_RFS_OFS_NAME, (int *) &rootfs_ofs) == -1 )
    {
        /* NOR Flash */
        PFILE_TAG pTag = NULL;

    switch( imageNumber )
    {
    case 0:
        pTag = getBootImageTag();
        break;

    case 1:
        pTag = getTagFromPartition(1);
        break;

    case 2:
        pTag = getTagFromPartition(2);
        break;

    default:
        break;
    }

    if( pTag )
        seqNumber= simple_strtoul(pTag->imageSequence, NULL, 10);
    }
    else
    {
        /* NAND Flash */
        NVRAM_DATA *pNvramData;

        if( (pNvramData = readNvramData()) != NULL )
        {
            char fname[] = NAND_CFE_RAM_NAME;
            char cferam_buf[32], cferam_fmt[32]; 
            int i;

            mm_segment_t fs;
            struct file *fp;
            int updatePart, getFromCurPart;

            // updatePart is the partition number that is not booted
            // getFromCurPart is 1 to retrieive info from the booted partition
            updatePart = (rootfs_ofs==pNvramData->ulNandPartOfsKb[NP_ROOTFS_1])
                ? 2 : 1;
            getFromCurPart = (updatePart == imageNumber) ? 0 : 1;

            fs = get_fs();
            set_fs(get_ds());
            if( getFromCurPart == 0 )
            {
                strcpy(cferam_fmt, "/mnt/");
                sys_mount("mtd:rootfs_update", "/mnt","jffs2",MS_RDONLY,NULL);
            }
            else
                cferam_fmt[0] = '\0';

            /* Find the sequence number of the specified partition. */
            fname[strlen(fname) - 3] = '\0'; /* remove last three chars */
            strcat(cferam_fmt, fname);
            strcat(cferam_fmt, "%3.3d");

            for( i = 0; i < 999; i++ )
            {
                sprintf(cferam_buf, cferam_fmt, i);
                fp = filp_open(cferam_buf, O_RDONLY, 0);
                if (!IS_ERR(fp) )
                {
                    filp_close(fp, NULL);

                    /* Seqence number found. */
                    seqNumber = i;
                    break;
                }
            }

            if( getFromCurPart == 0 )
                sys_umount("/mnt", 0);

            set_fs(fs);
            kfree(pNvramData);
        }
    }

    return(seqNumber);
}

static int getBootedValue(int getBootedPartition)
{
    static int s_bootedPartition = -1;
    int ret = -1;
    int imgId = -1;

    kerSysBlParmsGetInt(BOOTED_IMAGE_ID_NAME, &imgId);

    /* The boot loader parameter will only be "new image", "old image" or "only
     * image" in order to be compatible with non-OMCI image update. If the
     * booted partition is requested, convert this boot type to partition type.
     */
    if( imgId != -1 )
    {
        if( getBootedPartition )
        {
            if( s_bootedPartition != -1 )
                ret = s_bootedPartition;
            else
            {
            /* Get booted partition. */
            int seq1 = kerSysGetSequenceNumber(1);
            int seq2 = kerSysGetSequenceNumber(2);

            switch( imgId )
            {
            case BOOTED_NEW_IMAGE:
                if( seq1 == -1 || seq2 > seq1 )
                    ret = BOOTED_PART2_IMAGE;
                else
                    if( seq2 == -1 || seq1 >= seq2 )
                        ret = BOOTED_PART1_IMAGE;
                break;

            case BOOTED_OLD_IMAGE:
                if( seq1 == -1 || seq2 < seq1 )
                    ret = BOOTED_PART2_IMAGE;
                else
                    if( seq2 == -1 || seq1 <= seq2 )
                        ret = BOOTED_PART1_IMAGE;
                break;

            case BOOTED_ONLY_IMAGE:
                ret = (seq1 == -1) ? BOOTED_PART2_IMAGE : BOOTED_PART1_IMAGE;
                break;

            default:
                break;
            }

                s_bootedPartition = ret;
            }
        }
        else
            ret = imgId;
    }

    return( ret );
}


#if !defined(CONFIG_BRCM_IKOS)
PFILE_TAG kerSysImageTagGet(void)
{
    PFILE_TAG tag;

    mutex_lock(&flashImageMutex);
    tag = getBootImageTag();
    mutex_unlock(&flashImageMutex);

    return tag;
}
#ifdef MSTC_OBM_IMAGE_DEFAULT //__MSTC__, Dennis ZyXEL OBM ImageDefault feature, zongyue
int kerSysImageTagPartitionGet(PFILE_TAG pTag)
{
    return getPartitionFromTag(pTag);
}
#endif
#if 1 //__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11
PFILE_TAG kerSysImageTagGetByPartition(int imageNumber)
{
    return( getTagFromPartition(imageNumber) );
}
#endif
#else
PFILE_TAG kerSysImageTagGet(void)
{
    return( (PFILE_TAG) (FLASH_BASE + FLASH_LENGTH_BOOT_ROM));
}
#endif

/*
 * Common function used by BCM_IMAGE_CFE and BCM_IMAGE_WHOLE ioctls.
 * This function will acquire the flashImageMutex
 *
 * @return 0 on success, -1 on failure.
 */
#if 0 //__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11
static int commonImageWrite(int flash_start_addr, char *string, int size)
{
    NVRAM_DATA * pNvramDataOrig;
    NVRAM_DATA * pNvramDataNew=NULL;
    int ret;

    mutex_lock(&flashImageMutex);

    // Get a copy of the nvram before we do the image write operation
    if (NULL != (pNvramDataOrig = readNvramData()))
    {

        ret = kerSysBcmImageSet(flash_start_addr, string, size, 0);

        /*
         * After the image is written, check the nvram.
         * If nvram is bad, write back the original nvram.
         */
        pNvramDataNew = readNvramData();
        if ((0 != ret) ||
            (NULL == pNvramDataNew) ||
            (BpSetBoardId(pNvramDataNew->szBoardId) != BP_SUCCESS)
#if defined (CONFIG_BCM_ENDPOINT_MODULE)
            || (BpSetVoiceBoardId(pNvramDataNew->szVoiceBoardId) != BP_SUCCESS)
#endif
            )
        {
            // we expect this path to be taken.  When a CFE or whole image
            // is written, it typically does not have a valid nvram block
            // in the image.  We detect that condition here and restore
            // the previous nvram settings.  Don't print out warning here.
            writeNvramDataCrcLocked(pNvramDataOrig);

            // don't modify ret, it is return value from kerSysBcmImageSet
        }
    }
    else
    {
        ret = -1;
    }

    mutex_unlock(&flashImageMutex);

    if (pNvramDataOrig)
        kfree(pNvramDataOrig);
    if (pNvramDataNew)
        kfree(pNvramDataNew);

    return ret;
}
#endif

struct file_operations monitor_fops;

//********************************************************************************************
// misc. ioctl calls come to here. (flash, led, reset, kernel memory access, etc.)
//********************************************************************************************
static int board_ioctl( struct inode *inode, struct file *flip,
                       unsigned int command, unsigned long arg )
{
    int ret = 0;
    BOARD_IOCTL_PARMS ctrlParms;
    unsigned char ucaMacAddr[NVRAM_MAC_ADDRESS_LEN];
#if 1 //__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11
    PFILE_TAG tag = NULL;
    int i=0;
    NVRAM_DATA * pNvramData;
    NVRAM_DATA * pTmpNvramData;
#endif

    switch (command) {
    case BOARD_IOCTL_FLASH_WRITE:
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0) {

            switch (ctrlParms.action) {
            case SCRATCH_PAD:
                if (ctrlParms.offset == -1)
                    ret =  kerSysScratchPadClearAll();
                else
                    ret = kerSysScratchPadSet(ctrlParms.string, ctrlParms.buf, ctrlParms.offset);
                break;

            case PERSISTENT:
#if 1 //__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11
                for ( i = 0 ; i < 10 ; i ++ ) {
                    if ( kerSysPersistentSet(ctrlParms.string, ctrlParms.strLen, ctrlParms.offset) != 0 ) {
                        udelay(100);
                    }
                    else {
                        break;
                    }
                }
#else
                ret = kerSysPersistentSet(ctrlParms.string, ctrlParms.strLen, ctrlParms.offset);
#endif			 
                break;

            case BACKUP_PSI:
#if 1 //__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11
                for ( i = 0 ; i < 10 ; i ++ ) {
                    if ( kerSysBackupPsiSet(ctrlParms.string, ctrlParms.strLen, ctrlParms.offset) != 0 ) {
                        udelay(100);
                    }
                    else {
                        break;
                    }
                }
#else
                ret = kerSysBackupPsiSet(ctrlParms.string, ctrlParms.strLen, ctrlParms.offset);
#endif
                break;

#ifdef MSTC_ROM_D //__MSTC__, Dennis merge from ZyXEL ROM-D feature, zongyue
            case ROMD_PSI:
                for ( i = 0 ; i < 10 ; i ++ ) {
                    if ( kerSysRomdSet(ctrlParms.string, ctrlParms.strLen, ctrlParms.offset) != 0 ) {
                        udelay(100);
                    }
                    else {
                        break;
                    }
                }
                break;
#endif

#if 1 /* __ZyXEL__, WeiZen, WWAN Package: Flash partition */ 
#ifdef CONFIG_ZyXEL_WWAN_PACKAGE
			case WWAN_PACKAGE:
				for ( i = 0 ; i < 10 ; i ++ ) {
					if ( kerSysWWANSet(ctrlParms.string, ctrlParms.strLen, ctrlParms.offset) != 0 ) {
						udelay(100);
					}
					else {
						break;
					}
				}
				break;
#endif
#endif

            case SYSLOG:
                ret = kerSysSyslogSet(ctrlParms.string, ctrlParms.strLen, ctrlParms.offset);
                break;

            case NVRAM:
            {
#if 1 //__MSTC__, Dennis
#else
                NVRAM_DATA * pNvramData;
#endif
                /*
                 * Note: even though NVRAM access is protected by
                 * flashImageMutex at the kernel level, this protection will
                 * not work if two userspaces processes use ioctls to get
                 * NVRAM data, modify it, and then use this ioctl to write
                 * NVRAM data.  This seems like an unlikely scenario.
                 */
                mutex_lock(&flashImageMutex);
                if (NULL == (pNvramData = readNvramData()))
                {
                    mutex_unlock(&flashImageMutex);
                    return -ENOMEM;
                }
                if ( !strncmp(ctrlParms.string, "WLANDATA", 8 ) ) { //Wlan Data data
                    memset((char *)pNvramData + ((size_t) &((NVRAM_DATA *)0)->wlanParams),
                        0, sizeof(pNvramData->wlanParams) );
                    memcpy( (char *)pNvramData + ((size_t) &((NVRAM_DATA *)0)->wlanParams),
                        ctrlParms.string+8,
                        ctrlParms.strLen-8);
                    writeNvramDataCrcLocked(pNvramData);
                }
                else {
                    // assumes the user has calculated the crc in the nvram struct
                    ret = kerSysNvRamSet(ctrlParms.string, ctrlParms.strLen, ctrlParms.offset);
                }
                mutex_unlock(&flashImageMutex);
                kfree(pNvramData);
                break;
            }

            case BCM_IMAGE_CFE:
                if( ctrlParms.strLen <= 0 || ctrlParms.strLen > FLASH_LENGTH_BOOT_ROM )
                {
                    printk("Illegal CFE size [%d]. Size allowed: [%d]\n",
                        ctrlParms.strLen, FLASH_LENGTH_BOOT_ROM);
                    ret = -1;
                    break;
                }
#if 1 //__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11
                mutex_lock(&flashImageMutex);
                if (NULL == (pNvramData = readNvramData()))
                {
                   mutex_unlock(&flashImageMutex);
                   return -ENOMEM;
                }

                #ifdef CONFIG_MSTC_WDT
                /* Stop hardware watchdog */
                mstc_wdt_exit();
                #endif
				
#ifdef MTSC_NORWAY_CUSTOMIZATION
                ret = kerSysBcmImageSet(ctrlParms.offset + BOOT_OFFSET, ctrlParms.string, ctrlParms.strLen, 0);
#else
                ret = kerSysBcmImageSet(ctrlParms.offset + BOOT_OFFSET, ctrlParms.string, ctrlParms.strLen);
#endif

                // Check if the new image has valid NVRAM
                pTmpNvramData = kmalloc(sizeof(NVRAM_DATA), GFP_KERNEL);
                if (pTmpNvramData == NULL)
                {
                   mutex_unlock(&flashImageMutex);
                   kfree(pNvramData);
                   return -ENOMEM;
                }
                memset((char *)pTmpNvramData, 0, sizeof(NVRAM_DATA));
                memcpy ( (char *)pTmpNvramData, ctrlParms.string + NVRAM_DATA_OFFSET, sizeof(NVRAM_DATA) );
                memcpy( pNvramData->BuildInfo, pTmpNvramData->BuildInfo, NVRAM_BUILDINFO_LEN );
#if 1 //__MSTC__, Dennis
                // We need restore nvram data.
                // 1. debug flag is zero
                // 2. nvram data is invalid.
				/* __MSTC__, zongyue: fix logic issue */
                if (0 == pNvramData->EngDebugFlag)
		{
					if (NULL != (pTmpNvramData = readNvramData()))
						mustUpdateNvramfield(pTmpNvramData, pNvramData);
					writeNvramDataCrcLocked(pNvramData); 
		}else{
#if defined (CONFIG_ZYXEL_VMG1312)
			if(
#if defined (CONFIG_BCM_ENDPOINT_MODULE)
/*__ZyXEL__, Cj_Lai, Support VMG1312-B10A and B10B difference GPIO in board -> BpSetBoardId_DiffGPIO()*/
				(NULL == (pTmpNvramData = readNvramData()) )|| 
				(BpSetBoardId_DiffGPIO(pTmpNvramData->szBoardId,pTmpNvramData) != BP_SUCCESS) || 
				(BpSetVoiceBoardId(pTmpNvramData->szVoiceBoardId) != BP_SUCCESS)
#else
				(NULL == (pTmpNvramData = readNvramData()) ) || 
				(BpSetBoardId_DiffGPIO(pTmpNvramData->szBoardId,pTmpNvramData) != BP_SUCCESS)
#endif
			)
#else //CONFIG_ZYXEL_VMG1312
			if(
#if defined (CONFIG_BCM_ENDPOINT_MODULE)
			   (NULL == (pTmpNvramData = readNvramData()) )|| 
			   (BpSetBoardId(pTmpNvramData->szBoardId) != BP_SUCCESS) || 
			   (BpSetVoiceBoardId(pTmpNvramData->szVoiceBoardId) != BP_SUCCESS)
#else
			   (NULL == (pTmpNvramData = readNvramData()) ) || 
			   (BpSetBoardId(pTmpNvramData->szBoardId) != BP_SUCCESS)
#endif
			)
#endif //CONFIG_ZYXEL_VMG1312
                   writeNvramDataCrcLocked(pNvramData); 
                }
#else
#if defined (CONFIG_ZYXEL_VMG1312)	
#if defined (CONFIG_BCM_ENDPOINT_MODULE)
                if ( (NULL != (pTmpNvramData = readNvramData()) )|| (BpSetBoardId_DiffGPIO(pTmpNvramData->szBoardId,pTmpNvramData) != BP_SUCCESS) || (BpSetVoiceBoardId(pTmpNvramData->szVoiceBoardId) != BP_SUCCESS))
#else
                if ((NULL != (pTmpNvramData = readNvramData()) ) || (BpSetBoardId_DiffGPIO(pTmpNvramData->szBoardId,pTmpNvramData) != BP_SUCCESS))
#endif
#else //CONFIG_ZYXEL_VMG1312
#if defined (CONFIG_BCM_ENDPOINT_MODULE)
                if ((NULL != (pTmpNvramData = readNvramData()) ) || (BpSetBoardId(pTmpNvramData->szBoardId) != BP_SUCCESS) || (BpSetVoiceBoardId(pTmpNvramData->szVoiceBoardId) != BP_SUCCESS))
#else
                if ((NULL != (pTmpNvramData = readNvramData()) ) || (BpSetBoardId(pTmpNvramData->szBoardId) != BP_SUCCESS))
#endif
#endif //CONFIG_ZYXEL_VMG1312
                    writeNvramDataCrcLocked(pNvramData);
#endif //__MSTC__, Dennis
                mutex_unlock(&flashImageMutex);
                kfree(pTmpNvramData);
                kfree(pNvramData);
#else
                ret = commonImageWrite(ctrlParms.offset + BOOT_OFFSET, ctrlParms.string, ctrlParms.strLen);
#endif
                break;

            case BCM_IMAGE_FS:
#if 1 //__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11

#ifdef MTSC_NORWAY_CUSTOMIZATION
				{
					int noReboot = FLASH_IS_NO_REBOOT(ctrlParms.offset);
					int partition = FLASH_GET_PARTITION(ctrlParms.offset);
					
					if( noReboot == 1 )
					{
						ret = flashFsKernelImageForTr69(ctrlParms.string, ctrlParms.strLen, partition, 0);
					}else{
						ret = flashFsKernelImage(ctrlParms.string, ctrlParms.strLen, 0, 0);
					}
					
					if (ret == 0 && noReboot == 0)
					{
						resetPwrmgmtDdrMips();
					}
				}
#else
                ret = flashFsKernelImage(ctrlParms.string, ctrlParms.strLen, 0, 0);
#if 1 //MitraStar, Elina
                if (ret == 0)
                {
                    resetPwrmgmtDdrMips();
                }
#endif
#endif

#else
                {
                int numPartitions = 1;
                int noReboot = FLASH_IS_NO_REBOOT(ctrlParms.offset);
                int partition = FLASH_GET_PARTITION(ctrlParms.offset);

                mutex_lock(&flashImageMutex);
                ret = flashFsKernelImage(ctrlParms.string, ctrlParms.strLen,
                    partition, &numPartitions);
                mutex_unlock(&flashImageMutex);

                if(ret == 0 && (numPartitions == 1 || noReboot == 0))
                    resetPwrmgmtDdrMips();
                }
#endif
                break;

#ifdef MSTC_FWUP_FROM_FILE //__MSTC__, LingChun, merge MSTC_FWUP_FROM_FLASH from telefonica, http://svn.zyxel.com.tw/svn/CPE_TRUNK/BRCM_412/Telefonica_Common/
			case IMAGE_FS_FLASH:
			{
				fwup_flash_parm_t *fwup_flash_parm_p = (fwup_flash_parm_t *)ctrlParms.buf;
				if(fwup_flash_parm_p == NULL) {
					printk("Error! fwup_flash_parm_p is NULL\n");
					ret = -1;
					break;
				}		
		
                ret = flashFsKernelImageFile(fwup_flash_parm_p->filename, fwup_flash_parm_p->offset, fwup_flash_parm_p->size);
				#if 1
                if (ret == 0) {
                    resetPwrmgmtDdrMips();
                }
				#endif 
			}
                break;
#endif

            case BCM_IMAGE_KERNEL:  // not used for now.
                break;

            case BCM_IMAGE_WHOLE:
			{
#ifdef MSTC_FWUP_FROM_FILE //__MSTC__, LingChun
				fwup_flash_parm_t *fwup_flash_parm_p = (fwup_flash_parm_t *)ctrlParms.buf;
				if( (fwup_flash_parm_p==NULL && ctrlParms.strLen <= 0) || 
					(fwup_flash_parm_p!=NULL && fwup_flash_parm_p->size <=0))
#else
                if(ctrlParms.strLen <= 0)
#endif					
                {
#ifdef MSTC_FWUP_FROM_FILE 
					printk("Illegal flash image size [%d].\n", !(fwup_flash_parm_p)? ctrlParms.strLen : fwup_flash_parm_p->size);
#else
                    printk("Illegal flash image size [%d].\n", ctrlParms.strLen);
#endif
                    ret = -1;
                    break;
                }

                if (ctrlParms.offset == 0)
                {
                    ctrlParms.offset = FLASH_BASE;
                }
#if 1 //__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11
                mutex_lock(&flashImageMutex);
                if (NULL == (pNvramData = readNvramData()))
                {
                    mutex_unlock(&flashImageMutex);
                    return -ENOMEM;
                }
		#ifdef CONFIG_MSTC_WDT
                /* Stop hardware watchdog */
                mstc_wdt_exit();
                #endif
#ifdef MTSC_NORWAY_CUSTOMIZATION
				ret = kerSysBcmImageSet(ctrlParms.offset, ctrlParms.string, ctrlParms.strLen, 0);
#else
               ret = kerSysBcmImageSet(ctrlParms.offset, ctrlParms.string, ctrlParms.strLen);
#endif
#ifdef MSTC_FWUP_FROM_FILE //__MSTC__, LingChun
				if(fwup_flash_parm_p!=NULL){
					ret = flashWholeImageFile(fwup_flash_parm_p->filename, fwup_flash_parm_p->offset, fwup_flash_parm_p->size,ctrlParms.offset);
				}else{
					ret = kerSysBcmImageSet(ctrlParms.offset, ctrlParms.string, ctrlParms.strLen);
				}
#else				
               ret = kerSysBcmImageSet(ctrlParms.offset, ctrlParms.string, ctrlParms.strLen);
#endif

               pTmpNvramData = kmalloc(sizeof(NVRAM_DATA), GFP_KERNEL);                
               if ( pTmpNvramData == NULL)
               {
                  mutex_unlock(&flashImageMutex);
                  kfree(pNvramData);
                  return -ENOMEM;
               }

                // Check if the new image has valid NVRAM
#if 1 //__MSTC__, Dennis
               // We need restore nvram data.
               // 1. debug flag is zero
               // 2. nvram data is invalid.
				/* __MSTC__, zongyue: fix logic issue */
                if (0 == pNvramData->EngDebugFlag){
				if (NULL != (pTmpNvramData = readNvramData()))
						mustUpdateNvramfield(pTmpNvramData, pNvramData);
					writeNvramDataCrcLocked(pNvramData); 
		}else{
#if defined (CONFIG_ZYXEL_VMG1312)
			if(
#if defined (CONFIG_BCM_ENDPOINT_MODULE)
/*__ZyXEL__, Cj_Lai, Support VMG1312-B10A and B10B difference GPIO in board -> BpSetBoardId_DiffGPIO()*/
					   (NULL == (pTmpNvramData = readNvramData()) )|| 
					   (BpSetBoardId_DiffGPIO(pTmpNvramData->szBoardId,pTmpNvramData) != BP_SUCCESS) || 
					   (BpSetVoiceBoardId(pTmpNvramData->szVoiceBoardId) != BP_SUCCESS)
#else
					   (NULL == (pTmpNvramData = readNvramData()) ) || 
					   (BpSetBoardId_DiffGPIO(pTmpNvramData->szBoardId,pTmpNvramData) != BP_SUCCESS)
#endif
			)
#else //	CONFIG_ZYXEL_VMG1312
			if(
#if defined (CONFIG_BCM_ENDPOINT_MODULE)
			  (NULL == (pTmpNvramData = readNvramData()) )|| 
			  (BpSetBoardId(pTmpNvramData->szBoardId) != BP_SUCCESS) || 
			  (BpSetVoiceBoardId(pTmpNvramData->szVoiceBoardId) != BP_SUCCESS)
#else
			  (NULL == (pTmpNvramData = readNvramData()) ) || 
			  (BpSetBoardId(pTmpNvramData->szBoardId) != BP_SUCCESS)
#endif
			)
#endif //CONFIG_ZYXEL_VMG1312
                  writeNvramDataCrcLocked(pNvramData); 
               }
#else
#if defined (CONFIG_ZYXEL_VMG1312)	       
#if defined (CONFIG_BCM_ENDPOINT_MODULE)
                if ( (NULL != (pTmpNvramData = readNvramData()) )|| (BpSetBoardId_DiffGPIO(pTmpNvramData->szBoardId,pTmpNvramData) != BP_SUCCESS) || (BpSetVoiceBoardId(pTmpNvramData->szVoiceBoardId) != BP_SUCCESS))
#else
                if ((NULL != (pTmpNvramData = readNvramData()) ) || (BpSetBoardId_DiffGPIO(pTmpNvramData->szBoardId,pTmpNvramData) != BP_SUCCESS))
#endif	       
#else //	CONFIG_ZYXEL_VMG1312
#if defined (CONFIG_BCM_ENDPOINT_MODULE)
                if ( (NULL != (pTmpNvramData = readNvramData()) )|| (BpSetBoardId(pTmpNvramData->szBoardId) != BP_SUCCESS) || (BpSetVoiceBoardId(pTmpNvramData->szVoiceBoardId) != BP_SUCCESS))
#else
                if ((NULL != (pTmpNvramData = readNvramData()) ) || (BpSetBoardId(pTmpNvramData->szBoardId) != BP_SUCCESS))
#endif
#endif //CONFIG_ZYXEL_VMG1312
                    writeNvramDataCrcLocked(pNvramData);
#endif //__MSTC__, Dennis
                mutex_unlock(&flashImageMutex);
                kfree(pTmpNvramData);
                kfree(pNvramData);
#else
                ret = commonImageWrite(ctrlParms.offset, ctrlParms.string, ctrlParms.strLen);
#endif
                if (ret == 0)
                {
                    resetPwrmgmtDdrMips();
                }
                else
                {
                    printk("flash of whole image failed, ret=%d\n", ret);
                }
            }
                break;

            default:
                ret = -EINVAL;
                printk("flash_ioctl_command: invalid command %d\n", ctrlParms.action);
                break;
            }
            ctrlParms.result = ret;
            __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
        }
        else
            ret = -EFAULT;
        break;

    case BOARD_IOCTL_FLASH_READ:
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0) {
            switch (ctrlParms.action) {
#if 1 //__MSTC__, Dennis merge from Elina
            case UPDATE_CONFIG:
                ret = kerSysUpdateConfigGet(ctrlParms.string, ctrlParms.strLen, ctrlParms.offset);
                break;
#endif
            case SCRATCH_PAD:
                ret = kerSysScratchPadGet(ctrlParms.string, ctrlParms.buf, ctrlParms.offset);
                break;

            case PERSISTENT:
                ret = kerSysPersistentGet(ctrlParms.string, ctrlParms.strLen, ctrlParms.offset);
                break;

            case BACKUP_PSI:
                ret = kerSysBackupPsiGet(ctrlParms.string, ctrlParms.strLen, ctrlParms.offset);
                break;

            case SYSLOG:
                ret = kerSysSyslogGet(ctrlParms.string, ctrlParms.strLen, ctrlParms.offset);
                break;

            case NVRAM:
                kerSysNvRamGet(ctrlParms.string, ctrlParms.strLen, ctrlParms.offset);
                ret = 0;
                break;
#if 1 //__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11
            case FILETAG:
               if ( *((int*)ctrlParms.buf) == 0 ) { //get boot
                  if ( (tag = kerSysImageTagGet()) != NULL ) {
                     memcpy(ctrlParms.string, tag, sizeof(FILE_TAG));
                     ret = 0;
                  }
                     else {
                     ret = -1;
                  }
               }
               else {
                  if ( (tag = kerSysImageTagGetByPartition(*((int*)ctrlParms.buf))) != NULL ) {
                     memcpy(ctrlParms.string, tag, sizeof(FILE_TAG));
                     ret = 0;
                  }
                  else {
                     ret = -1;
                  }
               }
               break;
#endif
            case FLASH_SIZE:
                ret = kerSysFlashSizeGet();
                break;
#ifdef MSTC_ROM_D //__MSTC__, Dennis merge from ZyXEL ROM-D feature, zongyue
		 case ROMD_PSI:
                ret = kerSysRomdGet(ctrlParms.string, ctrlParms.strLen, ctrlParms.offset); 
                break;
#endif
#ifdef MSTC_OBM_IMAGE_DEFAULT //__MSTC__, Dennis ZyXEL OBM ImageDefault feature, zongyue
		 case IMGDEF_PSI:
                ret = kerSysImgDefGet(ctrlParms.string, ctrlParms.strLen, ctrlParms.offset); 
                break;

		 case IMAGETAG:
                ret = kerSysImgDefTagGet(ctrlParms.string);
                break;
#endif
#if 1 /* __ZyXEL__, WeiZen, WWAN Package: Flash partition */ 
#ifdef CONFIG_ZyXEL_WWAN_PACKAGE 
		 case WWAN_PACKAGE:
				ret = kerSysWWANGet(ctrlParms.string, ctrlParms.strLen, ctrlParms.offset); 
				break;
#endif
#endif
            default:
                ret = -EINVAL;
                printk("Not supported.  invalid command %d\n", ctrlParms.action);
                break;
            }
            ctrlParms.result = ret;
            __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
        }
        else
            ret = -EFAULT;
        break;

    case BOARD_IOCTL_FLASH_LIST:
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0) {
            switch (ctrlParms.action) {
            case SCRATCH_PAD:
                ret = kerSysScratchPadList(ctrlParms.buf, ctrlParms.offset);
                break;

            default:
                ret = -EINVAL;
                printk("Not supported.  invalid command %d\n", ctrlParms.action);
                break;
            }
            ctrlParms.result = ret;
            __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
        }
        else
            ret = -EFAULT;
        break;

    case BOARD_IOCTL_DUMP_ADDR:
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0)
        {
            dumpaddr( (unsigned char *) ctrlParms.string, ctrlParms.strLen );
            ctrlParms.result = 0;
            __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
            ret = 0;
        }
        else
            ret = -EFAULT;
        break;

    case BOARD_IOCTL_SET_MEMORY:
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0) {
            unsigned long  *pul = (unsigned long *)  ctrlParms.string;
            unsigned short *pus = (unsigned short *) ctrlParms.string;
            unsigned char  *puc = (unsigned char *)  ctrlParms.string;
            switch( ctrlParms.strLen ) {
            case 4:
                *pul = (unsigned long) ctrlParms.offset;
                break;
            case 2:
                *pus = (unsigned short) ctrlParms.offset;
                break;
            case 1:
                *puc = (unsigned char) ctrlParms.offset;
                break;
            }
#if !defined(CONFIG_BCM96816)
            /* This is placed as MoCA blocks are 32-bit only
            * accessible and following call makes access in terms
            * of bytes. Probably MoCA address range can be checked
            * here.
            */
            dumpaddr( (unsigned char *) ctrlParms.string, sizeof(long) );
#endif
            ctrlParms.result = 0;
            __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
            ret = 0;
        }
        else
            ret = -EFAULT;
        break;

    case BOARD_IOCTL_MIPS_SOFT_RESET:
        kerSysMipsSoftReset();
        break;

    case BOARD_IOCTL_LED_CTRL:
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0)
        {
            if ( ((BOARD_LED_NAME)ctrlParms.strLen == kLedUSB0)
               || ((BOARD_LED_NAME)ctrlParms.strLen == kLedUSB1) ) 
            {
                /* PeterSu: to distinguish this CLI from GUI power management, 
                    CLI uses ALLLedStateOn/ALLLedStateOff instead of kLedStateON/kLedStateOff
                */
                if ((BOARD_LED_STATE)ctrlParms.offset == ALLLedStateOn) {
                    ledctl_usb_te_test = 1;
                    ctrlParms.offset = kLedStateOn;	
                }
                else if ((BOARD_LED_STATE)ctrlParms.offset == ALLLedStateOff) {
                    ledctl_usb_te_test = 1;
                    ctrlParms.offset = kLedStateOff;	
                }
                else {
                    ledctl_usb_te_test = 0;
                }
               	kerSysLedCtrl((kLedUSB0+kLedUSB1)-(BOARD_LED_NAME)ctrlParms.strLen, (BOARD_LED_STATE)ctrlParms.offset);
            }
            else {
                kerSysLedCtrl((BOARD_LED_NAME)ctrlParms.strLen, (BOARD_LED_STATE)ctrlParms.offset);
            }
            ret = 0;
        }
        break;

    case BOARD_IOCTL_GET_ID:
        if (copy_from_user((void*)&ctrlParms, (void*)arg,
            sizeof(ctrlParms)) == 0)
        {
            if( ctrlParms.string )
            {
                char p[NVRAM_BOARD_ID_STRING_LEN];
                kerSysNvRamGetBoardId(p);
                if( strlen(p) + 1 < ctrlParms.strLen )
                    ctrlParms.strLen = strlen(p) + 1;
                __copy_to_user(ctrlParms.string, p, ctrlParms.strLen);
            }

            ctrlParms.result = 0;
            __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms,
                sizeof(BOARD_IOCTL_PARMS));
        }
        break;

    case BOARD_IOCTL_GET_MAC_ADDRESS:
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0)
        {
            ctrlParms.result = kerSysGetMacAddress( ucaMacAddr,
                ctrlParms.offset );

            if( ctrlParms.result == 0 )
            {
                __copy_to_user(ctrlParms.string, ucaMacAddr,
                    sizeof(ucaMacAddr));
            }

            __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms,
                sizeof(BOARD_IOCTL_PARMS));
            ret = 0;
        }
        else
            ret = -EFAULT;
        break;

    case BOARD_IOCTL_RELEASE_MAC_ADDRESS:
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0)
        {
            if (copy_from_user((void*)ucaMacAddr, (void*)ctrlParms.string, \
                NVRAM_MAC_ADDRESS_LEN) == 0)
            {
                ctrlParms.result = kerSysReleaseMacAddress( ucaMacAddr );
            }
            else
            {
                ctrlParms.result = -EACCES;
            }

            __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms,
                sizeof(BOARD_IOCTL_PARMS));
            ret = 0;
        }
        else
            ret = -EFAULT;
        break;

    case BOARD_IOCTL_GET_PSI_SIZE:
        {
            FLASH_ADDR_INFO fInfo;
            kerSysFlashAddrInfoGet(&fInfo);
            ctrlParms.result = fInfo.flash_persistent_length;
            __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
            ret = 0;
        }
        break;

    case BOARD_IOCTL_GET_BACKUP_PSI_SIZE:
        {
            FLASH_ADDR_INFO fInfo;
            kerSysFlashAddrInfoGet(&fInfo);
            // if number_blks > 0, that means there is a backup psi, but length is the same
            // as the primary psi (persistent).

            ctrlParms.result = (fInfo.flash_backup_psi_number_blk > 0) ?
                fInfo.flash_persistent_length : 0;
            //printk("backup_psi_number_blk=%d result=%d\n", fInfo.flash_backup_psi_number_blk, fInfo.flash_persistent_length);
            __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
            ret = 0;
        }
        break;

    case BOARD_IOCTL_GET_SYSLOG_SIZE:
        {
            FLASH_ADDR_INFO fInfo;
            kerSysFlashAddrInfoGet(&fInfo);
            ctrlParms.result = fInfo.flash_syslog_length;
            __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
            ret = 0;
        }
        break;

    case BOARD_IOCTL_GET_SDRAM_SIZE:
        ctrlParms.result = (int) g_ulSdramSize;
        __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
        ret = 0;
        break;

    case BOARD_IOCTL_GET_BASE_MAC_ADDRESS:
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0)
        {
            __copy_to_user(ctrlParms.string, g_pMacInfo->ucaBaseMacAddr, NVRAM_MAC_ADDRESS_LEN);
            ctrlParms.result = 0;

            __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms,
                sizeof(BOARD_IOCTL_PARMS));
            ret = 0;
        }
        else
            ret = -EFAULT;
        break;

    case BOARD_IOCTL_GET_CHIP_ID:
        ctrlParms.result = kerSysGetChipId();


        __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
        ret = 0;
        break;

    case BOARD_IOCTL_GET_CHIP_REV:
        ctrlParms.result = (int) (PERF->RevID & REV_ID_MASK);
        __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
        ret = 0;
        break;

    case BOARD_IOCTL_GET_NUM_ENET_MACS:
    case BOARD_IOCTL_GET_NUM_ENET_PORTS:
        {
            ETHERNET_MAC_INFO EnetInfos[BP_MAX_ENET_MACS];
            int i, cnt, numEthPorts = 0;
            if (BpGetEthernetMacInfo(EnetInfos, BP_MAX_ENET_MACS) == BP_SUCCESS) {
                for( i = 0; i < BP_MAX_ENET_MACS; i++) {
                    if (EnetInfos[i].ucPhyType != BP_ENET_NO_PHY) {
                        bitcount(cnt, EnetInfos[i].sw.port_map);
                        numEthPorts += cnt;
                    }
                }
                ctrlParms.result = numEthPorts;
                __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms,  sizeof(BOARD_IOCTL_PARMS));
                ret = 0;
            }
            else {
                ret = -EFAULT;
            }
            break;
        }

    case BOARD_IOCTL_GET_CFE_VER:
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0) {
#if defined(CONFIG_11ac_throughput_patch_from_412L08)
            unsigned char vertag[CFE_VERSION_MARK_SIZE+CFE_VERSION_SIZE];
#else
            char vertag[CFE_VERSION_MARK_SIZE+CFE_VERSION_SIZE];
#endif
            kerSysCfeVersionGet(vertag, sizeof(vertag));
            if (ctrlParms.strLen < CFE_VERSION_SIZE) {
                ctrlParms.result = 0;
                __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
                ret = -EFAULT;
            }
            else if (strncmp(vertag, "cfe-v", 5)) { // no tag info in flash
                ctrlParms.result = 0;
                __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
                ret = 0;
            }
            else {
                ctrlParms.result = 1;
                __copy_to_user(ctrlParms.string, vertag+CFE_VERSION_MARK_SIZE, CFE_VERSION_SIZE);
                __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
                ret = 0;
            }
        }
        else {
            ret = -EFAULT;
        }
        break;

#if defined (WIRELESS)
    case BOARD_IOCTL_GET_WLAN_ANT_INUSE:
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0) {
            unsigned short antInUse = 0;
            if (BpGetWirelessAntInUse(&antInUse) == BP_SUCCESS) {
                if (ctrlParms.strLen == sizeof(antInUse)) {
                    __copy_to_user(ctrlParms.string, &antInUse, sizeof(antInUse));
                    ctrlParms.result = 0;
                    __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
                    ret = 0;
                } else
                    ret = -EFAULT;
            }
            else {
                ret = -EFAULT;
            }
            break;
        }
        else {
            ret = -EFAULT;
        }
        break;
#endif
    case BOARD_IOCTL_SET_TRIGGER_EVENT:
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0) {
            BOARD_IOC *board_ioc = (BOARD_IOC *)flip->private_data;
            ctrlParms.result = -EFAULT;
            ret = -EFAULT;
            if (ctrlParms.strLen == sizeof(unsigned long)) {
                board_ioc->eventmask |= *((int*)ctrlParms.string);
#if defined (WIRELESS)
                if((board_ioc->eventmask & SES_EVENTS)) {
                    if(sesBtn_irq != BP_NOT_DEFINED) {
                        BcmHalInterruptEnable(sesBtn_irq);
                        ctrlParms.result = 0;
                        ret = 0;
                    }
					#ifdef MSTC_WLAN_WPS_BTN_SEPARATE //__MSTC__, WLAN_WPS_BTN_SEPARATE
					if(wlanBtn_irq != BP_NOT_DEFINED) {
                        BcmHalInterruptEnable(wlanBtn_irq);
                        ctrlParms.result = 0;
                        ret = 0;
                    }
					#endif
                }
#endif
                __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
            }
            break;
        }
        else {
            ret = -EFAULT;
        }
        break;

    case BOARD_IOCTL_GET_TRIGGER_EVENT:
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0) {
            BOARD_IOC *board_ioc = (BOARD_IOC *)flip->private_data;
            if (ctrlParms.strLen == sizeof(unsigned long)) {
                __copy_to_user(ctrlParms.string, &board_ioc->eventmask, sizeof(unsigned long));
                ctrlParms.result = 0;
                __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
                ret = 0;
            } else
                ret = -EFAULT;

            break;
        }
        else {
            ret = -EFAULT;
        }
        break;

    case BOARD_IOCTL_UNSET_TRIGGER_EVENT:
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0) {
            if (ctrlParms.strLen == sizeof(unsigned long)) {
                BOARD_IOC *board_ioc = (BOARD_IOC *)flip->private_data;
                board_ioc->eventmask &= (~(*((int*)ctrlParms.string)));
                ctrlParms.result = 0;
                __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
                ret = 0;
            } else
                ret = -EFAULT;

            break;
        }
        else {
            ret = -EFAULT;
        }
        break;
#if defined (WIRELESS)
    case BOARD_IOCTL_SET_SES_LED:
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0) {
                sesLed_ctrl(*(int*)ctrlParms.string, (BOARD_LED_NAME)ctrlParms.strLen);
#ifdef CONFIG_WPS5GLED
				if(ctrlParms.offset){
					// wl0 wl1 wps work at the same time.
					sesLed_ctrl(*(int*)ctrlParms.string, kLed5GWps);
				}
#endif
                ctrlParms.result = 0;
                __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
                ret = 0;
            break;
        }
        else {
            ret = -EFAULT;
        }
        break;
#endif

    case BOARD_IOCTL_SET_MONITOR_FD:
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0) {

           g_monitor_nl_pid =  ctrlParms.offset;
           printk(KERN_INFO "monitor task is initialized pid= %d \n",g_monitor_nl_pid);
        }
        break;

    case BOARD_IOCTL_WAKEUP_MONITOR_TASK:
        kerSysSendtoMonitorTask(MSG_NETLINK_BRCM_WAKEUP_MONITOR_TASK, NULL, 0);
        break;

    case BOARD_IOCTL_SET_CS_PAR:
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0) {
            ret = ConfigCs(&ctrlParms);
            __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
        }
        else {
            ret = -EFAULT;
        }
        break;

    case BOARD_IOCTL_SET_GPIO:
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0) {
            kerSysSetGpioState(ctrlParms.strLen, ctrlParms.offset);
            __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
            ret = 0;
        }
        else {
            ret = -EFAULT;
        }
        break;
#if 1 //__MSTC__, Dennis
   case BOARD_IOCTL_SET_BUTTONTEST:
      if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0) {
         if ( ctrlParms.strLen == 1 ) {
            buttonTest = 1;
            stopblink = 0;
            stopblinkwl = 0;
#if defined (WIRELESS) || defined(CONFIG_WIRELESS) // __P870HN51D_STD__, Richard, add CONFIG_WIRELESS
            BcmHalInterruptEnable(sesBtn_irq);
#ifdef MSTC_WLAN_WPS_BTN_SEPARATE //__MSTC__, WLAN_WPS_BTN_SEPARATE
			BcmHalInterruptEnable(wlanBtn_irq);
#endif
#endif
            ctrlParms.result = buttonTest;
         }else if ( ctrlParms.strLen == 0 ) {
            buttonTest = 0;
            stopblink = 0;
            stopblinkwl = 0;
            ctrlParms.result = buttonTest;
         }else if ( ctrlParms.strLen == 2 ) {
            ctrlParms.result = buttonTest;
         }else if ( ctrlParms.strLen == 3 ) {
            ctrlParms.result = stopblink;
         }else if ( ctrlParms.strLen == 4 ) {
            ctrlParms.result = stopblinkwl;
         }
         __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
         ret = 0;
      } 
      else {
         ret = -EFAULT;  
      }
      break;
#endif //__MSTC__, Dennis
#if 1 /* chchien */
   case BOARD_IOCTL_WPS_CONTROL:        
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0) {
            if ( ctrlParms.strLen == 0 ) {
                disWLANbutton = 0;
                ctrlParms.result = 0;
                printk(KERN_WARNING "Enable WLAN button in front panel\n");
            }
            else if ( ctrlParms.strLen == 1 ) { /* disable WLAN button */
                disWLANbutton = 1;
                ctrlParms.result = 1;
                printk(KERN_WARNING "Disable WLAN button in front panel\n");
            }
            __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
            ret = 0;
        } 
        else {
            ret = -EFAULT;  
        }
        break;
#endif
#if 1//__MSTC__, RaynorChung: Support USB test CMD for 963268 platform
    case BOARD_IOCTL_SET_USBTEST:
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0) {
            if ( ctrlParms.offset ) {
            #if defined(CONFIG_BCM963268)
            	volatile int *USB20H_EHCI_PORTSC_0 = (int*)(USB_EHCI_BASE+0xa0000054);
				volatile int *USB20H_EHCI_PORTSC_1 = (int*)(USB_EHCI_BASE+0xa0000058);
            	*USB20H_EHCI_PORTSC_0 &= ~(0xf<<16);
            	*USB20H_EHCI_PORTSC_0 |= (0x4<<16);
            	printk("USB20H_EHCI_PORTSC_0(0x%x) = 0x%x\n", (int)USB20H_EHCI_PORTSC_0, *USB20H_EHCI_PORTSC_0);

				
            	*USB20H_EHCI_PORTSC_1 &= ~(0xf<<16);
            	*USB20H_EHCI_PORTSC_1 |= (0x4<<16);
				printk("USB20H_EHCI_PORTSC_0(0x%x) = 0x%x\n", (int)USB20H_EHCI_PORTSC_1, *USB20H_EHCI_PORTSC_1);
            #endif
            }
            else {	
            #if defined(CONFIG_BCM963268)
            	volatile int *USB20H_EHCI_PORTSC_0 = (int*)(USB_EHCI_BASE+0xa0000054);
			    volatile int *USB20H_EHCI_PORTSC_1 = (int*)(USB_EHCI_BASE+0xa0000058);
			
            	*USB20H_EHCI_PORTSC_0 &= ~(0xf<<16);
            	printk("USB20H_EHCI_PORTSC_0(0x%x) = 0x%x\n", (int)USB20H_EHCI_PORTSC_0, *USB20H_EHCI_PORTSC_0);

				
            	*USB20H_EHCI_PORTSC_1 &= ~(0xf<<16);
            	printk("USB20H_EHCI_PORTSC_0(0x%x) = 0x%x\n", (int)USB20H_EHCI_PORTSC_1, *USB20H_EHCI_PORTSC_1);
            #endif
            }
            ctrlParms.result = 1;
            __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
            ret = 0;
        } 
        else {
            ret = -EFAULT;  
        }
        break;
#endif
#if defined(CONFIG_BCM_CPLD1)
    case BOARD_IOCTL_SET_SHUTDOWN_MODE:
        BcmCpld1SetShutdownMode();
        ret = 0;
        break;

    case BOARD_IOCTL_SET_STANDBY_TIMER:
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0) {
            BcmCpld1SetStandbyTimer(ctrlParms.offset);
            ret = 0;
        }
        else {
            ret = -EFAULT;
        }
        break;
#endif

    case BOARD_IOCTL_BOOT_IMAGE_OPERATION:
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0) {
            switch(ctrlParms.offset)
            {
            case BOOT_SET_PART1_IMAGE:
            case BOOT_SET_PART2_IMAGE:
            case BOOT_SET_PART1_IMAGE_ONCE:
            case BOOT_SET_PART2_IMAGE_ONCE:
            case BOOT_SET_OLD_IMAGE:
            case BOOT_SET_NEW_IMAGE:
#if 0 //__MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11
            case BOOT_SET_NEW_IMAGE_ONCE:
                ctrlParms.result = kerSysSetBootImageState(ctrlParms.offset);
                break;

            case BOOT_GET_BOOT_IMAGE_STATE:
                ctrlParms.result = kerSysGetBootImageState();
                break;

            case BOOT_GET_IMAGE_VERSION:
                /* ctrlParms.action is parition number */
                ctrlParms.result = getImageVersion((int) ctrlParms.action,
                    ctrlParms.string, ctrlParms.strLen);
                break;
#endif
            case BOOT_GET_BOOTED_IMAGE_ID:
                /* ctrlParm.strLen == 1: partition or == 0: id (new or old) */
                ctrlParms.result = getBootedValue(ctrlParms.strLen);
                break;

            default:
                ctrlParms.result = -EFAULT;
                break;
            }
            __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
            ret = 0;
        }
        else {
            ret = -EFAULT;
        }
        break;

    case BOARD_IOCTL_GET_TIMEMS:
        ret = jiffies_to_msecs(jiffies - INITIAL_JIFFIES);
        break;

    case BOARD_IOCTL_GET_DEFAULT_OPTICAL_PARAMS:
    {
        unsigned char ucDefaultOpticalParams[BP_OPTICAL_PARAMS_LEN];
            
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0)
        {
            ret = 0;
            if (BP_SUCCESS == (ctrlParms.result = BpGetDefaultOpticalParams(ucDefaultOpticalParams)))
            {
                __copy_to_user(ctrlParms.string, ucDefaultOpticalParams, BP_OPTICAL_PARAMS_LEN);

                if (__copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS)) != 0)
                {
                    ret = -EFAULT;
                }
            }                        
        }
        else
        {
            ret = -EFAULT;
        }

        break;
    }
    
    break;
    case BOARD_IOCTL_GET_GPON_OPTICS_TYPE:
     
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0) 
        {
            unsigned short Temp=0;
            BpGetGponOpticsType(&Temp);
            *((UINT32*)ctrlParms.buf) = Temp;
            __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
        }        
        ret = 0;

        break;

#if defined(CONFIG_BCM96816) || defined(CONFIG_BCM963268)
    case BOARD_IOCTL_SPI_SLAVE_INIT:  
        ret = 0;
        if (kerSysBcmSpiSlaveInit() != SPI_STATUS_OK)  
        {
            ret = -EFAULT;
        }        
        break;   
        
    case BOARD_IOCTL_SPI_SLAVE_READ:  
        ret = 0;
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0)
        {
             if (kerSysBcmSpiSlaveRead(ctrlParms.offset, (unsigned long *)ctrlParms.buf, ctrlParms.strLen) != SPI_STATUS_OK)  
             {
                 ret = -EFAULT;
             } 
             else
             {
                   __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));    
             }
        }
        else
        {
            ret = -EFAULT;
        }                 
        break;    
        
    case BOARD_IOCTL_SPI_SLAVE_WRITE:  
        ret = 0;
        if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0)
        {
             if (kerSysBcmSpiSlaveWrite(ctrlParms.offset, ctrlParms.result, ctrlParms.strLen) != SPI_STATUS_OK)  
             {
                 ret = -EFAULT;
             } 
             else
             {
                   __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));    
             }
        }
        else
        {
            ret = -EFAULT;
        }                 
        break;    
#endif

#if 1 // __MSTC__, Paul Ho, __AT&T__, __P870HNP__ ,Autumn
#if defined (WIRELESS) && ( defined(CONFIG_BCM96368) || defined(CONFIG_BCM96328) || defined(CONFIG_BCM963268))
          case BOARD_IOCTL_GET_PROCCESS_PID:
             if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0) {
                if(ctrlParms.strLen>0){
                   int i=0;
                   pid_no = 0;
                   for(i=0;i<strlen(ctrlParms.string);i++){
                         pid_no=pid_no*10+(ctrlParms.string[i]-'0');
                   }
                      ctrlParms.result = 0;
                   ret = 0;
                }
                else{
                   ctrlParms.result = -1;
                   ret = -EFAULT;
                }
                __copy_to_user((BOARD_IOCTL_PARMS*)arg, &ctrlParms, sizeof(BOARD_IOCTL_PARMS));
    
             } 
             else
                ret = -EFAULT;
             break;   
#endif /* defined (WIRELESS) && ( defined(CONFIG_BCM96368) || defined(CONFIG_BCM96328)  || defined(CONFIG_BCM963268)) */
#endif
#if 1 /* Support the CLI command to test incorrect bad block tables(BBT), __CHT__, MitraStar SeanLu, 20140328 */
	case BOARD_IOCTL_BRCMNAND_BBT_READ:	
	{
		/* Dump some BBT inforamtion */ 
		dumpNandInformation();	
		break;
	}
	case BOARD_IOCTL_BRCMNAND_BBT_WRITE:	
	{
		if (copy_from_user((void*)&ctrlParms, (void*)arg, sizeof(ctrlParms)) == 0) 
		{            
			switch (ctrlParms.action) {
				case BrcmBBTMinSave:					
				case BrcmBBTMaxSave:
				{
					if(ctrlParms.buf != NULL)
					{
						setNandBadBlockEntry ( (ctrlParms.action),*((int*)ctrlParms.buf));
					}			
					break;
				}				
				case BrcmBBTRun:
				{				
					setNandBadBlockEntry (BrcmBBTRun, 0);
					break;
				}
				default:
					break;
			}
		}
		break;
	}
#endif

    default:
        ret = -EINVAL;
        ctrlParms.result = 0;
        printk("board_ioctl: invalid command %x, cmd %d .\n",command,_IOC_NR(command));
        break;

    } /* switch */

    return (ret);

} /* board_ioctl */
#if 1 //__MSTC__, Dennis
static void mustUpdateNvramfield(PNVRAM_DATA nData,PNVRAM_DATA oData)
{
   if(nData->ulVersion!=oData->ulVersion)
      oData->ulVersion = nData->ulVersion;
   if(strcmp(nData->BuildInfo, oData->BuildInfo)!=0)
      memcpy(oData->BuildInfo,nData->BuildInfo,NVRAM_BUILDINFO_LEN);
   if(strcmp(nData->VendorName, oData->VendorName)!=0)
      memcpy(oData->VendorName,nData->VendorName,NVRAM_VENDORNAME_LEN);
   if(strcmp(nData->ProductName, oData->ProductName)!=0)
      memcpy(oData->ProductName,nData->ProductName,NVRAM_PRODUCTNAME_LEN);
}
#endif
/***************************************************************************
* SES Button ISR/GPIO/LED functions.
***************************************************************************/
#if defined (WIRELESS)

static Bool sesBtn_pressed(void)
{
    if ( disWLANbutton == 1 ){ /* chchien, skip detection of button-pressed */
	return 0;
    }

    if ((sesBtn_irq >= INTERRUPT_ID_EXTERNAL_0) && (sesBtn_irq <= INTERRUPT_ID_EXTERNAL_3)) {
        if (!(PERF->ExtIrqCfg & (1 << (sesBtn_irq - INTERRUPT_ID_EXTERNAL_0 + EI_STATUS_SHFT)))) {
            return 1;
        }
    }
#if defined(CONFIG_BCM96368) || defined(CONFIG_BCM96816)
    else if ((sesBtn_irq >= INTERRUPT_ID_EXTERNAL_4) || (sesBtn_irq <= INTERRUPT_ID_EXTERNAL_5)) {
        if (!(PERF->ExtIrqCfg1 & (1 << (sesBtn_irq - INTERRUPT_ID_EXTERNAL_4 + EI_STATUS_SHFT)))) {
            return 1;
        }
    }
#endif
    return 0;
}
#ifdef MSTC_WLAN_WPS_BTN_SEPARATE //__MSTC__, Eason, WLAN_WPS_BTN_SEPARATE
static Bool wlanBtn_pressed(void)
{
    if ( disWLANbutton == 1 ){ /* chchien, skip detection of button-pressed */
        return 0;
    }

    if ((wlanBtn_irq >= INTERRUPT_ID_EXTERNAL_0) && (wlanBtn_irq <= INTERRUPT_ID_EXTERNAL_3)) {
        if (!(PERF->ExtIrqCfg & (1 << (wlanBtn_irq - INTERRUPT_ID_EXTERNAL_0 + EI_STATUS_SHFT)))) {
            return 1;
        }
    }
#if defined(CONFIG_BCM96368) || defined(CONFIG_BCM96816)
    else if ((wlanBtn_irq >= INTERRUPT_ID_EXTERNAL_4) || (wlanBtn_irq <= INTERRUPT_ID_EXTERNAL_5)) {
        if (!(PERF->ExtIrqCfg1 & (1 << (wlanBtn_irq - INTERRUPT_ID_EXTERNAL_4 + EI_STATUS_SHFT)))) {
            return 1;
        }
    }
#endif
    return 0;
}

#endif

#if 1 // __MSTC__, Paul Ho, for WPS Button (Merged from 406 common trunk)
void sesBtn_count ( void )
{
  if ( disWLANbutton == 1 ) { /* chchien */
	chkWlanEnCounter = 0;
  }else{

#if defined(MSTC_WPS_BTN_SCE)
   struct task_struct *p;
	if ( sesBtn_pressed() ) {
		chkWlanEnCounter++;
		if (chkWlanEnCounter%10 == 0 && chkWlanEnCounter < 200) // do not display "." continuously, just within 20 sec (avoid to affect some other boards)
			printk(".");
		// start timer
#ifdef MTS_WPS_BTN_PER
		if ( chkWlanEnCounter == MTS_WPS_BTN_PER*10 )
		{
			//if ( buttonTest == 0 ) {
				//struct task_struct *p;

				if(pid_no > 0){
					p = find_task_by_vpid(pid_no);
					if(p != NULL) {
						send_sig(SIGALRM,p,0);
					}
				}
			//}
		}
#endif /* MTS_WPS_BTN_PER */

		init_timer(&gResetTimer);
		gResetTimer.function = (void*)sesBtn_count;
		gResetTimer.expires = jiffies+ktimems_v;        // timer expires in ~100ms
		add_timer (&gResetTimer);
	}
	else {
#ifdef NORWAY_BTN_LED_OFF		
		if ( chkWlanEnCounter >= (30*60*10) )
		{
			kerSysSendtoMonitorTask(MSG_EXTSW_LED_OFF, NULL, 0);  //disable extSW LAN led
			LED->ledHWDis |= 0x1000;							 // disable wan act led
			kerSysLedCtrl(kLedAdsl, kLedStateOff);
			kerSysLedCtrl(kLedVdsl, kLedStateOff);
			kerSysLedCtrl(kLedWanData, kLedStateOff);
			kerSysLedCtrl(kLedInternetData, kLedStateOff);
			kerSysLedCtrl(kLedVoip1, kLedStateOff);
			kerSysLedCtrl(kLedVoip2, kLedStateOff);
		}else
#endif
#ifdef MTS_WPS_BTN_PER
		if ( chkWlanEnCounter >= MTS_WPS_BTN_PER*10 )
		{
			printk("\n");
			printk(KERN_WARNING "WPS toogle behavior\n");
			//if ( buttonTest == 0 ) {
				kerSysLedCtrl(kLedSes, kLedStateOff);
			//}
			//struct task_struct *p;

			if(pid_no > 0){
				p = find_task_by_vpid(pid_no);
				if(p != NULL) {
					send_sig(SIGUSR2,p,0);
				}
			}
		} 
		else
#endif /* MTS_WPS_BTN_PER */

#ifndef MSTC_WLAN_WPS_BTN_SEPARATE
#ifdef MTS_WLAN_BTN_PER
		if ( chkWlanEnCounter >= MTS_WLAN_BTN_PER*10 )
#endif /* MTS_WLAN_BTN_PER */
		{
			printk("\n");
			printk(KERN_WARNING "WLAN toogle behavior\n");
			//if ( buttonTest == 0 ) {
			//struct task_struct *p;

			printk(KERN_WARNING "\r\n WLAN Button Pressed, send SIGUSR1 to wlmngr pid_no:%d\n", pid_no);
			if(pid_no > 0) {            
				p = find_task_by_vpid(pid_no);
				if (p != NULL)
					send_sig(SIGUSR1,p,0);
				else
					printk(KERN_ERR "WLAN on/off can't work.\n");
			}

			//}
			/*else {
				if ( stopblinkwl == 0 ) {
					kerSysLedCtrl(kLedPowerG, kLedStateFastBlinkContinues);
					stopblinkwl = 1;
				}
				else {
					kerSysLedCtrl(kLedPowerG, kLedStateOn);
					stopblinkwl = 0;
				}
			}*/
		}
#ifdef MTS_WLAN_BTN_PER
		else 
#endif /* MTS_WLAN_BTN_PER */
#endif //MSTC_WLAN_WPS_BTN_SEPARATE
		{
			printk("\n");
		}
		chkWlanEnCounter = 0;
		BcmHalInterruptEnable(sesBtn_irq);
	}
#else /* defined(MSTC_WPS_BTN_SCE) */
	if (sesBtn_pressed()) {
		init_timer(&gResetTimer);
		gResetTimer.function = (void*)sesBtn_count;
		gResetTimer.expires = jiffies+ktimems_v;        // timer expires in ~100ms
		add_timer (&gResetTimer);
	}
	/*else {
		if ( stopblinkwl == 0 ) {
			kerSysLedCtrl(kLedPowerG, kLedStateFastBlinkContinues);
			stopblinkwl = 1;
		}
		else {
			kerSysLedCtrl(kLedPowerG, kLedStateOn);
			stopblinkwl = 0;
		}
		BcmHalInterruptEnable(sesBtn_irq);
	}*/
#endif /* defined(MSTC_WPS_BTN_SCE) */

  }
}

#ifdef MSTC_WLAN_WPS_BTN_SEPARATE //__MSTC__, Eason, WLAN_WPS_BTN_SEPARATE
void wlanBtn_count ( void )
{
  if ( disWLANbutton == 1 ) { /* chchien */
        chkWlanEnCounter = 0;
  }else{

   struct task_struct *p;
	if ( wlanBtn_pressed() ) {
		chkWlanBtnCounter++;
		if (chkWlanBtnCounter%10 == 0 && chkWlanBtnCounter < 200) // do not display "." continuously, just within 20 sec (avoid to affect some other boards)
			printk(".");
		// start timer
		init_timer(&gResetTimerWlanBtn);
		gResetTimerWlanBtn.function = (void*)wlanBtn_count;
		gResetTimerWlanBtn.expires = jiffies+ktimems_v;        // timer expires in ~100ms
		add_timer (&gResetTimerWlanBtn);
	}
	else {


#ifdef MTS_WLAN_BTN_PER
		if ( chkWlanBtnCounter >= MTS_WLAN_BTN_PER*10 )
#endif /* MTS_WLAN_BTN_PER */
		{
			printk("\n");
			printk(KERN_WARNING "WLAN toogle behavior\n");
			//if ( buttonTest == 0 ) {
			//struct task_struct *p;

			printk(KERN_WARNING "\r\n WLAN Button Pressed, send SIGUSR1 to wlmngr pid_no:%d\n", pid_no);
			if(pid_no > 0) {            
				p = find_task_by_vpid(pid_no);
				if (p != NULL)
					send_sig(SIGUSR1,p,0);
				else
					printk(KERN_ERR "WLAN on/off can't work.\n");
			}

			//}
			/*else {
				if ( stopblinkwl == 0 ) {
					kerSysLedCtrl(kLedPowerG, kLedStateFastBlinkContinues);
					stopblinkwl = 1;
				}
				else {
					kerSysLedCtrl(kLedPowerG, kLedStateOn);
					stopblinkwl = 0;
				}
			}*/
		}
#ifdef MTS_WLAN_BTN_PER
		else 
#endif /* MTS_WLAN_BTN_PER */
		{
			printk("\n");
		}
		chkWlanBtnCounter = 0;
		BcmHalInterruptEnable(wlanBtn_irq);
	}

  }
}

#endif

static irqreturn_t sesBtn_isr(int irq, void *dev_id)
{
    if (sesBtn_pressed()){
#if defined(MSTC_WPS_BTN_SCE)
		wake_up_interruptible(&g_board_wait_queue);

		if (0 == chkWlanEnCounter) {
			chkWlanEnCounter++;
			// start timer
			init_timer(&gResetTimer);
			gResetTimer.function = (void*)sesBtn_count;
			gResetTimer.expires = jiffies+ktimems_v;        // timer expires in ~100ms
			add_timer (&gResetTimer);
			#ifdef MSTC_WLAN_WPS_BTN_SEPARATE
			printk(KERN_WARNING "WPS btn pressed detected\n");
			#else
			printk(KERN_WARNING "WLAN btn pressed detected\n");
			#endif
		}
#else /* defined(MSTC_WPS_BTN_SCE) */
        wake_up_interruptible(&g_board_wait_queue);

/*        if ( buttonTest == 1 ) {
            // start timer
            init_timer(&gResetTimer);
            gResetTimer.function = (void*)sesBtn_count;
            gResetTimer.expires = jiffies+ktimems_v;        // timer expires in ~100ms
            add_timer (&gResetTimer);
            printk(KERN_WARNING "WLAN btn pressed detected (buttonTest=1)\n");
        }*/
#endif /* defined(MSTC_WPS_BTN_SCE) */
        return IRQ_RETVAL(1);
    } else {
        return IRQ_RETVAL(0);
    }
}
#else // original
static irqreturn_t sesBtn_isr(int irq, void *dev_id)
{
    if (sesBtn_pressed()){
        wake_up_interruptible(&g_board_wait_queue);
        return IRQ_RETVAL(1);
    } else {
        return IRQ_RETVAL(0);
    }
}
#endif // end, __MSTC__, Paul Ho
#ifdef MSTC_WLAN_WPS_BTN_SEPARATE //__MSTC__, Eason, WLAN_WPS_BTN_SEPARATE
static irqreturn_t wlanBtn_isr(int irq, void *dev_id)
{
    if (wlanBtn_pressed()){

		wake_up_interruptible(&g_board_wait_queue);

		if (0 == chkWlanBtnCounter) {
			chkWlanBtnCounter++;
			// start timer
			init_timer(&gResetTimerWlanBtn);
			gResetTimerWlanBtn.function = (void*)wlanBtn_count;
			gResetTimerWlanBtn.expires = jiffies+ktimems_v;        // timer expires in ~100ms
			add_timer (&gResetTimerWlanBtn);
			printk(KERN_WARNING "WLAN btn pressed detected\n");
		}

        return IRQ_RETVAL(1);
    } else {
        return IRQ_RETVAL(0);
    }
}

#endif

static void __init sesBtn_mapIntr(int context)
{
    if( BpGetWirelessSesExtIntr(&sesBtn_irq) == BP_SUCCESS )
    {
        printk("SES: Button Interrupt 0x%x is enabled\n", sesBtn_irq);
    }
    else
        return;

    sesBtn_irq = map_external_irq (sesBtn_irq) ;

    if (BcmHalMapInterrupt((FN_HANDLER)sesBtn_isr, context, sesBtn_irq)) {
        printk("SES: Interrupt mapping failed\n");
    }
    BcmHalInterruptEnable(sesBtn_irq);
}

#ifdef MSTC_WLAN_WPS_BTN_SEPARATE//__MSTC__, Eason, WLAN_WPS_BTN_SEPARATE
static void __init wlanBtn_mapIntr(int context)
{
    if( BpGetWirelessBtnExtIntr(&wlanBtn_irq) == BP_SUCCESS )
    {
        printk("WLAN BTN: Button Interrupt 0x%x is enabled\n", wlanBtn_irq);
    }
    else
        return;

    wlanBtn_irq = map_external_irq (wlanBtn_irq) ;

    if (BcmHalMapInterrupt((FN_HANDLER)wlanBtn_isr, context, wlanBtn_irq)) {
        printk(KERN_ERR "WLAN BTN: Interrupt mapping failed\n");
    }
    BcmHalInterruptEnable(wlanBtn_irq);
}
#endif

static unsigned int sesBtn_poll(struct file *file, struct poll_table_struct *wait)
{

    if (sesBtn_pressed()){
        return POLLIN;
    }
    return 0;
}

static ssize_t sesBtn_read(struct file *file,  char __user *buffer, size_t count, loff_t *ppos)
{
    volatile unsigned int event=0;
    ssize_t ret=0;

    if(!sesBtn_pressed()){
        BcmHalInterruptEnable(sesBtn_irq);
        return ret;
    }
    event = SES_EVENTS;
    __copy_to_user((char*)buffer, (char*)&event, sizeof(event));
    BcmHalInterruptEnable(sesBtn_irq);
    count -= sizeof(event);
    buffer += sizeof(event);
    ret += sizeof(event);
    return ret;
}

static void __init sesLed_mapGpio()
{
    if( BpGetWirelessSesLedGpio(&sesLed_gpio) == BP_SUCCESS )
    {
        printk("SES: LED GPIO 0x%x is enabled\n", sesLed_gpio);
    }
}

static void sesLed_ctrl(int action, BOARD_LED_NAME ledName)
{
    char blinktype = ((action >> 24) & 0xff); /* extract blink type for SES_LED_BLINK  */

    BOARD_LED_STATE led;

    if(sesLed_gpio == BP_NOT_DEFINED)
        return;

    action &= 0xff; /* extract led */

    switch (action) {
    case SES_LED_ON:
        led = kLedStateOn;
        break;
    case SES_LED_BLINK:
        if(blinktype)
            led = blinktype;
        else
            led = kLedStateSlowBlinkContinues;           		
        break;
    case SES_LED_OFF:
    default:
        led = kLedStateOff;
    }

    kerSysLedCtrl(ledName, led);
}

static void __init ses_board_init()
{
    sesBtn_mapIntr(0);
    sesLed_mapGpio();
	#ifdef MSTC_WLAN_WPS_BTN_SEPARATE	
    wlanBtn_mapIntr(0);
	#endif
}
static void __exit ses_board_deinit()
{
    if(sesBtn_irq)
        BcmHalInterruptDisable(sesBtn_irq);
	#ifdef MSTC_WLAN_WPS_BTN_SEPARATE		
    if(wlanBtn_irq)
        BcmHalInterruptDisable(wlanBtn_irq);
	#endif	
}
#endif

/***************************************************************************
* Dying gasp ISR and functions.
***************************************************************************/

static irqreturn_t kerSysDyingGaspIsr(int irq, void * dev_id)
{
    struct list_head *pos;
    CB_DGASP_LIST *tmp = NULL, *dslOrGpon = NULL;
	unsigned short usPassDyingGaspGpio;		// The GPIO pin to propogate a dying gasp signal

    UART->Data = 'D';
    UART->Data = '%';
    UART->Data = 'G';

#if defined (WIRELESS)
    kerSetWirelessPD(WLAN_OFF);
#endif
    /* first to turn off everything other than dsl or gpon */
    list_for_each(pos, &g_cb_dgasp_list_head->list) {
        tmp = list_entry(pos, CB_DGASP_LIST, list);
        if(strncmp(tmp->name, "dsl", 3) && strncmp(tmp->name, "gpon", 4)) {
            (tmp->cb_dgasp_fn)(tmp->context);
        }else {
            dslOrGpon = tmp;
        }
    }
	
    // Invoke dying gasp handlers
    if(dslOrGpon)
        (dslOrGpon->cb_dgasp_fn)(dslOrGpon->context);

    /* reset and shutdown system */

    /* Set WD to fire in 1 sec in case power is restored before reset occurs */
    TIMER->WatchDogDefCount = 1000000 * (FPERIPH/1000000);
    TIMER->WatchDogCtl = 0xFF00;
    TIMER->WatchDogCtl = 0x00FF;

	// If configured, propogate dying gasp to other processors on the board
	if(BpGetPassDyingGaspGpio(&usPassDyingGaspGpio) == BP_SUCCESS)
	    {
	    // Dying gasp configured - set GPIO
	    kerSysSetGpioState(usPassDyingGaspGpio, kGpioInactive);
	    }

    // If power is going down, nothing should continue!
    while (1);
    return( IRQ_HANDLED );
}

static void __init kerSysInitDyingGaspHandler( void )
{
    CB_DGASP_LIST *new_node;

    if( g_cb_dgasp_list_head != NULL) {
        printk("Error: kerSysInitDyingGaspHandler: list head is not null\n");
        return;
    }
    new_node= (CB_DGASP_LIST *)kmalloc(sizeof(CB_DGASP_LIST), GFP_KERNEL);
    memset(new_node, 0x00, sizeof(CB_DGASP_LIST));
    INIT_LIST_HEAD(&new_node->list);
    g_cb_dgasp_list_head = new_node;

    BcmHalMapInterrupt((FN_HANDLER)kerSysDyingGaspIsr, 0, INTERRUPT_ID_DG);
    BcmHalInterruptEnable( INTERRUPT_ID_DG );
} /* kerSysInitDyingGaspHandler */

static void __exit kerSysDeinitDyingGaspHandler( void )
{
    struct list_head *pos;
    CB_DGASP_LIST *tmp;

    if(g_cb_dgasp_list_head == NULL)
        return;

    list_for_each(pos, &g_cb_dgasp_list_head->list) {
        tmp = list_entry(pos, CB_DGASP_LIST, list);
        list_del(pos);
        kfree(tmp);
    }

    kfree(g_cb_dgasp_list_head);
    g_cb_dgasp_list_head = NULL;

} /* kerSysDeinitDyingGaspHandler */

void kerSysRegisterDyingGaspHandler(char *devname, void *cbfn, void *context)
{
    CB_DGASP_LIST *new_node;

    // do all the stuff that can be done without the lock first
    if( devname == NULL || cbfn == NULL ) {
        printk("Error: kerSysRegisterDyingGaspHandler: register info not enough (%s,%x,%x)\n", devname, (unsigned int)cbfn, (unsigned int)context);
        return;
    }

    if (strlen(devname) > (IFNAMSIZ - 1)) {
        printk("Warning: kerSysRegisterDyingGaspHandler: devname too long, will be truncated\n");
    }

    new_node= (CB_DGASP_LIST *)kmalloc(sizeof(CB_DGASP_LIST), GFP_KERNEL);
    memset(new_node, 0x00, sizeof(CB_DGASP_LIST));
    INIT_LIST_HEAD(&new_node->list);
    strncpy(new_node->name, devname, IFNAMSIZ-1);
    new_node->cb_dgasp_fn = (cb_dgasp_t)cbfn;
    new_node->context = context;

    // OK, now acquire the lock and insert into list
    mutex_lock(&dgaspMutex);
    if( g_cb_dgasp_list_head == NULL) {
        printk("Error: kerSysRegisterDyingGaspHandler: list head is null\n");
        kfree(new_node);
    } else {
        list_add(&new_node->list, &g_cb_dgasp_list_head->list);
        printk("dgasp: kerSysRegisterDyingGaspHandler: %s registered \n", devname);
    }
    mutex_unlock(&dgaspMutex);

    return;
} /* kerSysRegisterDyingGaspHandler */

void kerSysDeregisterDyingGaspHandler(char *devname)
{
    struct list_head *pos;
    CB_DGASP_LIST *tmp;
    int found=0;

    if(devname == NULL) {
        printk("Error: kerSysDeregisterDyingGaspHandler: devname is null\n");
        return;
    }

    printk("kerSysDeregisterDyingGaspHandler: %s is deregistering\n", devname);

    mutex_lock(&dgaspMutex);
    if(g_cb_dgasp_list_head == NULL) {
        printk("Error: kerSysDeregisterDyingGaspHandler: list head is null\n");
    } else {
        list_for_each(pos, &g_cb_dgasp_list_head->list) {
            tmp = list_entry(pos, CB_DGASP_LIST, list);
            if(!strcmp(tmp->name, devname)) {
                list_del(pos);
                kfree(tmp);
                found = 1;
                printk("kerSysDeregisterDyingGaspHandler: %s is deregistered\n", devname);
                break;
            }
        }
        if (!found)
            printk("kerSysDeregisterDyingGaspHandler: %s not (de)registered\n", devname);
    }
    mutex_unlock(&dgaspMutex);

    return;
} /* kerSysDeregisterDyingGaspHandler */


/***************************************************************************
 *
 *
 ***************************************************************************/
static int ConfigCs (BOARD_IOCTL_PARMS *parms)
{
    int                     retv = 0;
#if defined(CONFIG_BCM96368) || defined(CONFIG_BCM96816)
    int                     cs, flags;
    cs_config_pars_t        info;

    if (copy_from_user(&info, (void*)parms->buf, sizeof(cs_config_pars_t)) == 0)
    {
        cs = parms->offset;

        MPI->cs[cs].base = ((info.base & 0x1FFFE000) | (info.size >> 13));

        if ( info.mode == EBI_TS_TA_MODE )     // syncronious mode
            flags = (EBI_TS_TA_MODE | EBI_ENABLE);
        else
        {
            flags = ( EBI_ENABLE | \
                (EBI_WAIT_STATES  & (info.wait_state << EBI_WTST_SHIFT )) | \
                (EBI_SETUP_STATES & (info.setup_time << EBI_SETUP_SHIFT)) | \
                (EBI_HOLD_STATES  & (info.hold_time  << EBI_HOLD_SHIFT )) );
        }
        MPI->cs[cs].config = flags;
        parms->result = BP_SUCCESS;
        retv = 0;
    }
    else
    {
        retv -= EFAULT;
        parms->result = BP_NOT_DEFINED;
    }
#endif
    return( retv );
}

#if defined (WIRELESS) &&!defined(MSTC_WPS_BTN_SCE) // __MSTC__, Paul Ho, ZyXEL Autumn
#if defined(CONFIG_BCM96368) || defined(CONFIG_BCM96328) || defined(CONFIG_BCM963268)
static GPIO_STATE_t GetGpio(int gpio)
{
	GPIO->GPIODir &= ~GPIO_NUM_TO_MASK(gpio);
	if (GPIO->GPIOio & GPIO_NUM_TO_MASK(gpio))
		return kGpioActive;
	else
		return kGpioInactive;
}
#endif /* defined(CONFIG_BCM96368) || defined(CONFIG_BCM96328) || defined(CONFIG_BCM963268) */
#endif /* !defined(MSTC_WPS_BTN_SCE) */


/***************************************************************************
* Handle push of restore to default button
***************************************************************************/
#if 1 //__MSTC__ , __P870HNP__, __AT&T__ Autumn
#if defined(CONFIG_BCM96816) || defined(CONFIG_BCM96328)|| defined(CONFIG_BCM96368)|| defined(CONFIG_BCM963268)
#if !defined(CONFIG_AUXFS_JFFS2)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 20)
static void restore_to_default_thread(struct work_struct *work)
#else
static void restore_to_default_thread(void *arg)
#endif
{
	char buf[256]={0};
    // Do this in a kernel thread so we don't have any restriction
    printk ( KERN_WARNING "Clean Configuration" );
	while ( kerSysPersistentSet( buf, 256, 0 ) != 0 ) {
		printk ( "." );
		udelay(100);
	}
	printk ( "\n" );
#ifdef SUPPORT_BACKUP_PSI
    printk ( KERN_WARNING "Clean Backup Configuration" );
	while ( kerSysBackupPsiSet( buf, 256, 0 ) != 0 ) {
		printk ( "." );
		udelay(100);
	}
	printk ( "\n" );
#endif
	kernel_restart(NULL);
    return;
}
#endif

static Bool resetBtn_pressed(void)
{
    if ((resetBtn_irq >= INTERRUPT_ID_EXTERNAL_0) && (resetBtn_irq <= INTERRUPT_ID_EXTERNAL_3)) {
        if (!(PERF->ExtIrqCfg & (1 << (resetBtn_irq - INTERRUPT_ID_EXTERNAL_0 + EI_STATUS_SHFT)))) {
            return 1;
        }
    }
#if defined(CONFIG_BCM96368) || defined(CONFIG_BCM96816)
    else if ((resetBtn_irq >= INTERRUPT_ID_EXTERNAL_4) || (resetBtn_irq <= INTERRUPT_ID_EXTERNAL_5)) {
        if (!(PERF->ExtIrqCfg1 & (1 << (resetBtn_irq - INTERRUPT_ID_EXTERNAL_4 + EI_STATUS_SHFT)))) {
            return 1;
        }
    }
#endif
    return 0;
}

void ResetCounterAdd(void)
{
	if ( resetBtn_pressed() ) {
		chkResetCounter++;

#ifdef CV_RESTORE_BTN_PER
		if ( chkResetCounter >= CV_RESTORE_BTN_PER )
		{
			if ( buttonTest == 0 ) {
				printk("\n");
				printk(KERN_WARNING "Restore to default setting...\n");
				chkResetCounter = -1000;
#ifdef CONFIG_AUXFS_JFFS2 //__MSTC__, Jeff
				kerSysSendtoMonitorTask(MSG_NETLINK_MTS_RESET_DEFAULT, NULL, 0);
#else
				INIT_WORK(&restoreDefaultWork, restore_to_default_thread);
				schedule_work(&restoreDefaultWork);
#endif
				kerSysLedCtrl(kLedPowerG, kLedStateFail);
				return;
			}
		}
#endif

		if (chkResetCounter%10 == 0 )
			printk(".");
		// start timer
		init_timer(&gResetTimer);
		gResetTimer.function = (void*)ResetCounterAdd;
		gResetTimer.expires = jiffies+ktimems_v;        // timer expires in ~100ms
		add_timer (&gResetTimer);
	}
	else {
#ifdef CV_RESTORE_BTN_PER
		if ( chkResetCounter >= CV_RESTORE_BTN_PER )
		{
			printk("\n");
			printk(KERN_WARNING "Restore to default setting...\n");
			chkResetCounter = -1000;
			if ( buttonTest == 0 ) {
				kerSysLedCtrl(kLedPowerG, kLedStateOn);
#ifdef CONFIG_AUXFS_JFFS2 //__MSTC__, Jeff
				kerSysSendtoMonitorTask(MSG_NETLINK_MTS_RESET_DEFAULT, NULL, 0);
#else
				INIT_WORK(&restoreDefaultWork, restore_to_default_thread);
				schedule_work(&restoreDefaultWork);
#endif
				return;
			}
		} else
#endif
		if ( chkResetCounter >= 0 ) {
			printk("\n");
			printk(KERN_WARNING "System Reset button is press....\n");
			if ( buttonTest == 0 ) {
#ifdef CONFIG_AUXFS_JFFS2 //__MSTC__, Jeff
/*__ZyXEL__, Cj_Lai , Disable , When reset btn press (1-9 sec) then CPE will reboot. */
				//kerSysSendtoMonitorTask(MSG_NETLINK_MTS_REBOOT, NULL, 0);
#else

				kernel_restart(NULL);
#endif			
/*Disable the return value then always listen the reset btn Interrupt signals*/
				//return;
			}
			else {
				if ( stopblink == 0 ) {
					kerSysLedCtrl(kLedPowerG, kLedStateFastBlinkContinues);
					stopblink = 1;
				}
				else {
					kerSysLedCtrl(kLedPowerG, kLedStateOn);
					stopblink = 0;
				}
			}
		}
		chkResetCounter = 0;
		BcmHalInterruptEnable(resetBtn_irq);
	}
}

static irqreturn_t reset_isr(int irq, void *dev_id)
{
	if ( resetBtn_pressed() ) {
		chkResetCounter = 0;
		if ( buttonTest == 1 ) {
			wake_up_interruptible(&g_board_wait_queue);
		}

		printk(KERN_WARNING "reset btn pressed detected");

		// start timer
		init_timer(&gResetTimer);
		gResetTimer.function = (void*)ResetCounterAdd;
		gResetTimer.expires = jiffies+ktimems_v;        // timer expires in ~100ms
		add_timer (&gResetTimer);
		return IRQ_RETVAL(1);
	}
	else {
		return IRQ_RETVAL(0);
	}
}
#else
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 20)
static void restore_to_default_thread(struct work_struct *work)
#else
static void restore_to_default_thread(void *arg)
#endif
{
    char buf[256];

    memset(buf, 0, sizeof(buf));

    // Do this in a kernel thread so we don't have any restriction
    printk("Restore to Factory Default Setting ***\n\n");
    kerSysPersistentSet( buf, sizeof(buf), 0 );

    // kernel_restart is a high level, generic linux way of rebooting.
    // It calls a notifier list and lets sub-systems know that system is
    // rebooting, and then calls machine_restart, which eventually
    // calls kerSysMipsSoftReset.
    kernel_restart(NULL);
    return;
}

static irqreturn_t reset_isr(int irq, void *dev_id)
{
    printk("\n***reset button press detected***\n\n");

    INIT_WORK(&restoreDefaultWork, restore_to_default_thread);
    schedule_work(&restoreDefaultWork);
    return IRQ_HANDLED;
}
#endif
#endif

#if defined(WIRELESS)
/***********************************************************************
* Function Name: kerSysScreenPciDevices
* Description  : Screen Pci Devices before loading modules
***********************************************************************/
static void __init kerSysScreenPciDevices(void)
{
    unsigned short wlFlag;

    if((BpGetWirelessFlags(&wlFlag) == BP_SUCCESS) && (wlFlag & BP_WLAN_EXCLUDE_ONBOARD)) {
        /*
        * scan all available pci devices and delete on board BRCM wireless device
        * if external slot presents a BRCM wireless device
        */
        int foundPciAddOn = 0;
        struct pci_dev *pdevToExclude = NULL;
        struct pci_dev *dev = NULL;

        while((dev=pci_get_device(PCI_ANY_ID, PCI_ANY_ID, dev))!=NULL) {
            printk("kerSysScreenPciDevices: 0x%x:0x%x:(slot %d) detected\n", dev->vendor, dev->device, PCI_SLOT(dev->devfn));
            if((dev->vendor == BRCM_VENDOR_ID) &&
                (((dev->device & 0xff00) == BRCM_WLAN_DEVICE_IDS)|| 
                ((dev->device/1000) == BRCM_WLAN_DEVICE_IDS_DEC))) {
                    if(PCI_SLOT(dev->devfn) != WLAN_ONBOARD_SLOT) {
                        foundPciAddOn++;
                    } else {
                        pdevToExclude = dev;
                    }                
            }
        }

        if(((wlFlag & BP_WLAN_EXCLUDE_ONBOARD_FORCE) || foundPciAddOn) && pdevToExclude) {
            printk("kerSysScreenPciDevices: 0x%x:0x%x:(onboard) deleted\n", pdevToExclude->vendor, pdevToExclude->device);
            pci_remove_bus_device(pdevToExclude);
        }
    }
}

/***********************************************************************
* Function Name: kerSetWirelessPD
* Description  : Control Power Down by Hardware if the board supports
***********************************************************************/
static void kerSetWirelessPD(int state)
{
    unsigned short wlanPDGpio;
    if((BpGetWirelessPowerDownGpio(&wlanPDGpio)) == BP_SUCCESS) {
        if (wlanPDGpio != BP_NOT_DEFINED) {
            if(state == WLAN_OFF)
                kerSysSetGpioState(wlanPDGpio, kGpioActive);
            else
                kerSysSetGpioState(wlanPDGpio, kGpioInactive);
        }
    }
}

#endif


#if defined(CONFIG_BCM96816) || defined(CONFIG_BCM96362) || defined(CONFIG_BCM96328) || defined(CONFIG_BCM963268) || defined(CONFIG_BCM96318)
/***********************************************************************
* Function Name: kerSysCheckPowerDownPcie
* Description  : Power Down PCIe if no device enumerated
*                Otherwise enable Power Saving modes
***********************************************************************/
static void __init kerSysCheckPowerDownPcie(void)
{
    struct pci_dev *dev = NULL;
#if defined(CONFIG_BCM96328) || defined(CONFIG_BCM963268)
    unsigned long GPIOOverlays;
#endif

    while ((dev=pci_get_device(PCI_ANY_ID, PCI_ANY_ID, dev))!=NULL) {
        if(BCM_BUS_PCIE_DEVICE == dev->bus->number) {
            /* Enable PCIe L1 PLL power savings */
            PCIEH_BLK_1800_REGS->phyCtrl[1] |= REG_POWERDOWN_P1PLL_ENA;
#if defined(CONFIG_BCM96328) || defined(CONFIG_BCM963268)
            /* Enable PCIe CLKREQ# power savings */
            if( (BpGetGPIOverlays(&GPIOOverlays) == BP_SUCCESS) && (GPIOOverlays & BP_OVERLAY_PCIE_CLKREQ)) {
                PCIEH_BRIDGE_REGS->pcieControl |= PCIE_BRIDGE_CLKREQ_ENABLE;
            }
#endif
            return;
        }
    }
            
    printk("PCIe: No device found - Powering down\n");
    /* pcie clock disable*/
#if defined(PCIE_MISC_HARD_PCIE_HARD_DEBUG_SERDES_IDDQ)
    PCIEH_MISC_HARD_REGS->hard_eco_ctrl_hard |= PCIE_MISC_HARD_PCIE_HARD_DEBUG_SERDES_IDDQ;
#endif

    PERF->blkEnables &= ~PCIE_CLK_EN;
#if defined(CONFIG_BCM963268)
    MISC->miscLcpll_ctrl |= MISC_CLK100_DISABLE;
#endif

    /* pcie serdes disable */
#if defined(CONFIG_BCM96816)   
    GPIO->SerdesCtl &= ~(SERDES_PCIE_ENABLE|SERDES_PCIE_EXD_ENABLE);
#endif
#if defined(CONFIG_BCM96328) || defined(CONFIG_BCM96362) || defined(CONFIG_BCM963268)
    MISC->miscSerdesCtrl &= ~(SERDES_PCIE_ENABLE|SERDES_PCIE_EXD_ENABLE);
#endif

    /* pcie disable additional clocks */
#if defined(PCIE_UBUS_CLK_EN)
    PLL_PWR->PllPwrControlActiveUbusPorts &= ~PORT_ID_PCIE;
    PERF->blkEnablesUbus &= ~PCIE_UBUS_CLK_EN;
#endif

#if defined(PCIE_25_CLK_EN)
    PERF->blkEnables &= ~PCIE_25_CLK_EN;
#endif 

#if defined(PCIE_ASB_CLK_EN)
    PERF->blkEnables &= ~PCIE_ASB_CLK_EN;
#endif

#if defined(SOFT_RST_PCIE) && defined(SOFT_RST_PCIE_EXT) && defined(SOFT_RST_PCIE_CORE)
    /* pcie and ext device */
    PERF->softResetB &= ~(SOFT_RST_PCIE|SOFT_RST_PCIE_EXT|SOFT_RST_PCIE_CORE);
#endif    

#if defined(SOFT_RST_PCIE_HARD)
    PERF->softResetB &= ~SOFT_RST_PCIE_HARD;
#endif

#if defined(IDDQ_PCIE)
    PLL_PWR->PllPwrControlIddqCtrl |= IDDQ_PCIE;
#endif

}
#endif

extern unsigned char g_blparms_buf[];

/***********************************************************************
 * Function Name: kerSysBlParmsGetInt
 * Description  : Returns the integer value for the requested name from
 *                the boot loader parameter buffer.
 * Returns      : 0 - success, -1 - failure
 ***********************************************************************/
int kerSysBlParmsGetInt( char *name, int *pvalue )
{
    char *p2, *p1 = g_blparms_buf;
    int ret = -1;

    *pvalue = -1;

    /* The g_blparms_buf buffer contains one or more contiguous NULL termianted
     * strings that ends with an empty string.
     */
    while( *p1 )
    {
        p2 = p1;

        while( *p2 != '=' && *p2 != '\0' )
            p2++;

        if( *p2 == '=' )
        {
            *p2 = '\0';

            if( !strcmp(p1, name) )
            {
                *p2++ = '=';
                *pvalue = simple_strtol(p2, &p1, 0);
                if( *p1 == '\0' )
                    ret = 0;
                break;
            }

            *p2 = '=';
        }

        p1 += strlen(p1) + 1;
    }

    return( ret );
}

/***********************************************************************
 * Function Name: kerSysBlParmsGetStr
 * Description  : Returns the string value for the requested name from
 *                the boot loader parameter buffer.
 * Returns      : 0 - success, -1 - failure
 ***********************************************************************/
int kerSysBlParmsGetStr( char *name, char *pvalue, int size )
{
    char *p2, *p1 = g_blparms_buf;
    int ret = -1;

    /* The g_blparms_buf buffer contains one or more contiguous NULL termianted
     * strings that ends with an empty string.
     */
    while( *p1 )
    {
        p2 = p1;

        while( *p2 != '=' && *p2 != '\0' )
            p2++;

        if( *p2 == '=' )
        {
            *p2 = '\0';

            if( !strcmp(p1, name) )
            {
                *p2++ = '=';
                strncpy(pvalue, p2, size);
                ret = 0;
                break;
            }

            *p2 = '=';
        }

        p1 += strlen(p1) + 1;
    }

    return( ret );
}

static int add_proc_files(void)
{
#define offset(type, elem) ((int)&((type *)0)->elem)

    static int BaseMacAddr[2] = {offset(NVRAM_DATA, ucaBaseMacAddr), NVRAM_MAC_ADDRESS_LEN};
#if 1 // __MSTC__, zongyue: reduce manufacture bootup time for wireless calibration
#define offset_idx(type, elem, index) ((int)&((type *)0)->elem[index])
	static int ReduceBootTime[2] = {offset_idx(NVRAM_DATA, FeatureBits, ATMT_FEATUREBITS_IDX), 1};
#endif

    struct proc_dir_entry *p0;
    struct proc_dir_entry *p1;
#ifdef CONFIG_MSTC_WDT
	/* Add hardware watchdog proc entry */
	mstc_wdt_add_proc();
#endif

	#ifdef CONFIG_MSTC_RECORD_CALL_TRACE
	mstc_koops_add_proc();
	#endif

	#ifdef CONFIG_MSTC_DEC_AND_GZIP_CORE
	mstc_coredump_add_proc();
	#endif

    p0 = proc_mkdir("nvram", NULL);

    if (p0 == NULL)
    {
        printk("add_proc_files: failed to create proc files!\n");
        return -1;
    }

    p1 = create_proc_entry("BaseMacAddr", 0644, p0);

    if (p1 == NULL)
    {
        printk("add_proc_files: failed to create proc files!\n");
        return -1;
    }

    p1->data        = BaseMacAddr;
    p1->read_proc   = proc_get_param;
    p1->write_proc  = proc_set_param;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
	//New linux no longer requires proc_dir_entry->owner field.
#else
    p1->owner       = THIS_MODULE;
#endif

    p1 = create_proc_entry("led", 0644, NULL);
    if (p1 == NULL)
        return -1;

    p1->write_proc  = proc_set_led;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
	//New linux no longer requires proc_dir_entry->owner field.
#else
    p1->owner       = THIS_MODULE;
#endif

#if 1 // __MSTC__, zongyue: reduce manufacture bootup time for wireless calibration
    p1 = create_proc_entry("ReduceTimeFlag", 0644, p0);

    if (p1 == NULL)
    {
        printk("add_proc_files: failed to create proc files!\n");
        return -1;
    }

    p1->data        = ReduceBootTime;
    p1->read_proc   = proc_get_param;
	p1->write_proc  = proc_set_param;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
	//New linux no longer requires proc_dir_entry->owner field.
#else
    p1->owner       = THIS_MODULE;
#endif
#endif // __MSTC__, zongyue: reduce manufacture bootup time for wireless calibration

    return 0;
}

static int del_proc_files(void)
{
	#ifdef CONFIG_MSTC_WDT
	/* Remove hardware watchdog proc entry */
	mstc_wdt_del_proc();
	#endif

	#ifdef CONFIG_MSTC_RECORD_CALL_TRACE
	mstc_koops_del_proc();
	#endif

	#ifdef CONFIG_MSTC_DEC_AND_GZIP_CORE
	mstc_coredump_del_proc();
	#endif

    remove_proc_entry("nvram", NULL);
    remove_proc_entry("led", NULL);
    return 0;
}

static void str_to_num(char* in, char* out, int len)
{
    int i;
    memset(out, 0, len);

    for (i = 0; i < len * 2; i ++)
    {
        if ((*in >= '0') && (*in <= '9'))
            *out += (*in - '0');
        else if ((*in >= 'a') && (*in <= 'f'))
            *out += (*in - 'a') + 10;
        else if ((*in >= 'A') && (*in <= 'F'))
            *out += (*in - 'A') + 10;
        else
            *out += 0;

        if ((i % 2) == 0)
            *out *= 16;
        else
            out ++;

        in ++;
    }
    return;
}

static int proc_get_param(char *page, char **start, off_t off, int cnt, int *eof, void *data)
{
    int i = 0;
    int r = 0;
    int offset  = ((int *)data)[0];
    int length  = ((int *)data)[1];
    NVRAM_DATA *pNvramData;

    *eof = 1;

    if ((offset < 0) || (offset + length > sizeof(NVRAM_DATA)))
        return 0;

    if (NULL != (pNvramData = readNvramData()))
    {
        for (i = 0; i < length; i ++)
            r += sprintf(page + r, "%02x ", ((unsigned char *)pNvramData)[offset + i]);
    }

    r += sprintf(page + r, "\n");
    if (pNvramData)
        kfree(pNvramData);
    return (r < cnt)? r: 0;
}

static int proc_set_param(struct file *f, const char *buf, unsigned long cnt, void *data)
{
    NVRAM_DATA *pNvramData;
    char input[32];

    int i = 0;
    int r = cnt;
    int offset  = ((int *)data)[0];
    int length  = ((int *)data)[1];

    if ((offset < 0) || (offset + length > sizeof(NVRAM_DATA)))
        return 0;

    if ((cnt > 32) || (copy_from_user(input, buf, cnt) != 0))
        return -EFAULT;

    for (i = 0; i < r; i ++)
    {
        if (!isxdigit(input[i]))
        {
            memmove(&input[i], &input[i + 1], r - i - 1);
            r --;
            i --;
        }
    }

    mutex_lock(&flashImageMutex);

    if (NULL != (pNvramData = readNvramData()))
    {
#if 1 /* __MSTC__, zongyue: protect nvram data */
		if (1 == pNvramData->EngDebugFlag) {
			str_to_num(input, ((char *)pNvramData) + offset, length);
			writeNvramDataCrcLocked(pNvramData);
		}
#endif
    }
    else
    {
        cnt = 0;
    }

    mutex_unlock(&flashImageMutex);

    if (pNvramData)
        kfree(pNvramData);

    return cnt;
}

/*
 * This function expect input in the form of:
 * echo "xxyy" > /proc/led
 * where xx is hex for the led number
 * and   yy is hex for the led state.
 * For example,
 *     echo "0301" > led
 * will turn on led 3
 */
static int proc_set_led(struct file *f, const char *buf, unsigned long cnt, void *data)
{
    char leddata[16];
    char input[32];
    int i;
    int r;
    int num_of_octets;

    if (cnt > 32)
        cnt = 32;

    if (copy_from_user(input, buf, cnt) != 0)
        return -EFAULT;

    r = cnt;

    for (i = 0; i < r; i ++)
    {
        if (!isxdigit(input[i]))
        {
            memmove(&input[i], &input[i + 1], r - i - 1);
            r --;
            i --;
        }
    }

    num_of_octets = r / 2;

    if (num_of_octets != 2)
        return -EFAULT;

    str_to_num(input, leddata, num_of_octets);
    kerSysLedCtrl (leddata[0], leddata[1]);
    return cnt;
}


#if defined(CONFIG_BCM96368)

#define DSL_PHY_PHASE_CNTL      ((volatile uint32* const) 0xb00012a8)
#define DSL_CPU_PHASE_CNTL      ((volatile uint32* const) 0xb00012ac)
#define MIPS_PHASE_CNTL         ((volatile uint32* const) 0xb00012b0)
#define DDR1_2_PHASE_CNTL       ((volatile uint32* const) 0xb00012b4)
#define DDR3_4_PHASE_CNTL       ((volatile uint32* const) 0xb00012b8)

// The direction bit tells the automatic counters to count up or down to the
// desired value.
#define PI_VALUE_WIDTH 14
#define PI_COUNT_UP    ( 1 << PI_VALUE_WIDTH )
#define PI_MASK        ( PI_COUNT_UP - 1 )

// Turn off sync mode.  Set bit 28 of CP0 reg 22 sel 5.
static void TurnOffSyncMode( void )
{
    uint32 value;

    value = __read_32bit_c0_register( $22, 5 ) | (1<<28);
    __write_32bit_c0_register( $22, 5, value );
    //    Print( "Sync mode %x\n", value );
    value = DDR->MIPSPhaseCntl;

    // Reset the PH_CNTR_CYCLES to 7.
    // Set the phase counter cycles (bits 16-19) back to 7.
    value &= ~(0xf<<16);
    value |= (7<<16);

    // Set the LLMB counter cycles back to 7.
    value &= ~(0xf<<24);
    value |= (7<<24);
    // Set the UBUS counter cycles back to 7.
    value &= ~(0xf<<28);
    value |= (7<<28);

    // Turn off the LLMB counter, which is what maintains sync mode.
    // Clear bit 21, which is LLMB_CNTR_EN.
    value &= ~(1 << 21);
    // Turn off UBUS LLMB CNTR EN
    value &= ~(1 << 23);

    DDR->MIPSPhaseCntl = value;

    // Reset the MIPS phase to 0.
    PI_lower_set( MIPS_PHASE_CNTL, 0 );

    //Clear Count Bit
    value &= ~(1 << 14);
    DDR->MIPSPhaseCntl = value;

}

// Write the specified value in the lower half of a PI control register.  Each
// 32-bit register holds two values, but they can't be addressed separately.
static void
PI_lower_set( volatile uint32  *PI_reg,
             int               newPhaseInt )
{
    uint32  oldRegValue;
    uint32  saveVal;
    int32   oldPhaseInt;
    int32   newPhase;
    uint32  newVal;
    int     equalCount      = 0;

    oldRegValue = *PI_reg;
    // Save upper 16 bits, which is the other PI value.
    saveVal     = oldRegValue & 0xffff0000;

    // Sign extend the lower PI value, and shift it down into the lower 16 bits.
    // This gives us a 32-bit signed value which we can compare to the newPhaseInt
    // value passed in.

    // Shift the sign bit to bit 31
    oldPhaseInt = oldRegValue << ( 32 - PI_VALUE_WIDTH );
    // Sign extend and shift the lower value into the lower 16 bits.
    oldPhaseInt = oldPhaseInt >> ( 32 - PI_VALUE_WIDTH );

    // Take the low 10 bits as the new phase value.
    newPhase = newPhaseInt & PI_MASK;

    // If our new value is larger than the old value, tell the automatic counter
    // to count up.
    if ( newPhaseInt > oldPhaseInt )
    {
        newPhase = newPhase | PI_COUNT_UP;
    }

    // Or in the value originally in the upper 16 bits.
    newVal  = newPhase | saveVal;
    *PI_reg = newVal;

    // Wait until we match several times in a row.  Only the low 4 bits change
    // while the counter is working, so we can get a false "done" indication
    // when we read back our desired value.
    do
    {
        if ( *PI_reg == newVal )
        {
            equalCount++;
        }
        else
        {
            equalCount = 0;
        }

    } while ( equalCount < 3 );

}

// Write the specified value in the upper half of a PI control register.  Each
// 32-bit register holds two values, but they can't be addressed separately.
static void
PI_upper_set( volatile uint32  *PI_reg,
             int               newPhaseInt )
{
    uint32  oldRegValue;
    uint32  saveVal;
    int32   oldPhaseInt;
    int32   newPhase;
    uint32  newVal;
    int     equalCount      = 0;

    oldRegValue = *PI_reg;
    // Save lower 16 bits, which is the other PI value.
    saveVal     = oldRegValue & 0xffff;

    // Sign extend the upper PI value, and shift it down into the lower 16 bits.
    // This gives us a 32-bit signed value which we can compare to the newPhaseInt
    // value passed in.

    // Shift the sign bit to bit 31
    oldPhaseInt = oldRegValue << ( 16 - PI_VALUE_WIDTH );
    // Sign extend and shift the upper value into the lower 16 bits.
    oldPhaseInt = oldPhaseInt >> ( 32 - PI_VALUE_WIDTH );

    // Take the low 10 bits as the new phase value.
    newPhase = newPhaseInt & PI_MASK;

    // If our new value is larger than the old value, tell the automatic counter
    // to count up.
    if ( newPhaseInt > oldPhaseInt )
    {
        newPhase = newPhase | PI_COUNT_UP;
    }

    // Shift the new phase value into the upper 16 bits, and restore the value
    // originally in the lower 16 bits.
    newVal = (newPhase << 16) | saveVal;
    *PI_reg = newVal;

    // Wait until we match several times in a row.  Only the low 4 bits change
    // while the counter is working, so we can get a false "done" indication
    // when we read back our desired value.
    do
    {
        if ( *PI_reg == newVal )
        {
            equalCount++;
        }
        else
        {
            equalCount = 0;
        }

    } while ( equalCount < 3 );

}

// Reset the DDR PI registers to the default value of 0.
static void ResetPiRegisters( void )
{
    volatile int delay;
    uint32 value;

    //Skip This step for now load_ph should be set to 0 for this anyways.
    //Print( "Resetting DDR phases to 0\n" );
    //PI_lower_set( DDR1_2_PHASE_CNTL, 0 ); // DDR1 - Should be a NOP.
    //PI_upper_set( DDR1_2_PHASE_CNTL, 0 ); // DDR2
    //PI_lower_set( DDR3_4_PHASE_CNTL, 0 ); // DDR3 - Must remain at 90 degrees for normal operation.
    //PI_upper_set( DDR3_4_PHASE_CNTL, 0 ); // DDR4

    // Need to have VDSL back in reset before this is done.
    // Disable VDSL Mip's
    GPIO->VDSLControl = GPIO->VDSLControl & ~VDSL_MIPS_RESET;
    // Disable VDSL Core
    GPIO->VDSLControl = GPIO->VDSLControl & ~(VDSL_CORE_RESET | 0x8);


    value = DDR->DSLCpuPhaseCntr;

    // Reset the PH_CNTR_CYCLES to 7.
    // Set the VDSL Mip's phase counter cycles (bits 16-19) back to 7.
    value &= ~(0xf<<16);
    value |= (7<<16);

    // Set the VDSL PHY counter cycles back to 7.
    value &= ~(0xf<<24);
    value |= (7<<24);
    // Set the VDSL AFE counter cycles back to 7.
    value &= ~(0xf<<28);
    value |= (7<<28);

    // Turn off the VDSL MIP's PHY auto counter
    value &= ~(1 << 20);
    // Clear bit 21, which is VDSL PHY CNTR_EN.
    value &= ~(1 << 21);
    // Turn off the VDSL AFE auto counter
    value &= ~(1 << 22);

    DDR->DSLCpuPhaseCntr = value;

    // Reset the VDSL MIPS phase to 0.
    PI_lower_set( DSL_PHY_PHASE_CNTL, 0 ); // VDSL PHY - should be NOP
    PI_upper_set( DSL_PHY_PHASE_CNTL, 0 ); // VDSL AFE - should be NOP
    PI_lower_set( DSL_CPU_PHASE_CNTL, 0 ); // VDSL MIP's

    //Clear Count Bits for DSL CPU
    value &= ~(1 << 14);
    DDR->DSLCpuPhaseCntr = value;
    //Clear Count Bits for DSL Core
    DDR->DSLCorePhaseCntl &= ~(1<<30);
    DDR->DSLCorePhaseCntl &= ~(1<<14);
    // Allow some settle time.
    delay = 100;
    while (delay--);

    printk("\n****** DDR->DSLCorePhaseCntl=%lu ******\n\n", (unsigned long)
        DDR->DSLCorePhaseCntl);

    // Turn off the automatic counters.
    // Clear bit 20, which is PH_CNTR_EN.
    DDR->MIPSPhaseCntl &= ~(1<<20);
    // Turn Back UBUS Signals to reset state
    DDR->UBUSPhaseCntl = 0x00000000;
    DDR->UBUSPIDeskewLLMB0 = 0x00000000;
    DDR->UBUSPIDeskewLLMB1 = 0x00000000;

}

static void ChipSoftReset(void)
{
    TurnOffSyncMode();
    ResetPiRegisters();
}
#endif


/***************************************************************************
 * Function Name: kerSysGetUbusFreq
 * Description  : Chip specific computation.
 * Returns      : the UBUS frequency value in MHz.
 ***************************************************************************/
unsigned long kerSysGetUbusFreq(unsigned long miscStrapBus)
{
   unsigned long ubus = UBUS_BASE_FREQUENCY_IN_MHZ;

#if defined(CONFIG_BCM96362)
   /* Ref RDB - 6362 */
   switch (miscStrapBus) {

      case 0x4 :
      case 0xc :
      case 0x14:
      case 0x1c:
      case 0x15:
      case 0x1d:
         ubus = 100;
         break;
      case 0x2 :
      case 0xa :
      case 0x12:
      case 0x1a:
         ubus = 96;
         break;
      case 0x1 :
      case 0x9 :
      case 0x11:
      case 0xe :
      case 0x16:
      case 0x1e:
         ubus = 200;
         break;
      case 0x6:
         ubus = 183;
         break;
      case 0x1f:
         ubus = 167;
         break;
      default:
         ubus = 160;
         break;
   }
#endif

   return (ubus);

}  /* kerSysGetUbusFreq */


/***************************************************************************
 * Function Name: kerSysGetChipId
 * Description  : Map id read from device hardware to id of chip family
 *                consistent with  BRCM_CHIP
 * Returns      : chip id of chip family
 ***************************************************************************/
int kerSysGetChipId() { 
        int r;
        r = (int) ((PERF->RevID & CHIP_ID_MASK) >> CHIP_ID_SHIFT);
        /* Force BCM681x variants to be be BCM6816) */
        if( (r & 0xfff0) == 0x6810 )
            r = 0x6816;

        /* Force BCM6369 to be BCM6368) */
        if( (r & 0xfffe) == 0x6368 )
            r = 0x6368;

        /* Force BCM63168, BCM63169, and BCM63269 to be BCM63268) */
        if( ( (r & 0xffffe) == 0x63168 )
          || ( (r & 0xffffe) == 0x63268 ))
            r = 0x63268;

        return(r);
}

/***************************************************************************
 * Function Name: kerSysGetDslPhyEnable
 * Description  : returns true if device should permit Phy to load
 * Returns      : true/false
 ***************************************************************************/
int kerSysGetDslPhyEnable() {
        int id;
        int r = 1;
        id = (int) ((PERF->RevID & CHIP_ID_MASK) >> CHIP_ID_SHIFT);
        if ((id == 0x63169) || (id == 0x63269)) {
	    r = 0;
        }
        return(r);
}
#if defined(CONFIG_11ac_throughput_patch_from_412L08)
/***************************************************************************
 * Function Name: kerSysGetChipName
 * Description  : fills buf with the human-readable name of the device
 * Returns      : pointer to buf
 ***************************************************************************/
char *kerSysGetChipName(char *buf, int n) {
	return(UtilGetChipName(buf, n));
}
#endif
/***************************************************************************
* MACRO to call driver initialization and cleanup functions.
***************************************************************************/
module_init( brcm_board_init );
module_exit( brcm_board_cleanup );

EXPORT_SYMBOL(dumpaddr);
EXPORT_SYMBOL(kerSysGetChipId);
#ifdef BUILD_11ac_throughput_patch_from_412L07
#if defined(CONFIG_11ac_throughput_patch_from_412L08)
EXPORT_SYMBOL(kerSysGetChipName);
#endif
EXPORT_SYMBOL(kerSysMacAddressNotifyBind);
#endif
EXPORT_SYMBOL(kerSysGetMacAddressType);
#if 1 // __MSTC__, Richard Huang
EXPORT_SYMBOL(kerSysGetBaseMacAddress);
#endif
#ifdef UPC_FIXMAC_CUSTOMIZATION
EXPORT_SYMBOL(kerSysGetMacAddressByIndex);
#endif
EXPORT_SYMBOL(kerSysGetMacAddress);
EXPORT_SYMBOL(kerSysReleaseMacAddress);
EXPORT_SYMBOL(kerSysGetGponSerialNumber);
EXPORT_SYMBOL(kerSysGetGponPassword);
EXPORT_SYMBOL(kerSysGetSdramSize);
EXPORT_SYMBOL(kerSysGetDslPhyEnable);
EXPORT_SYMBOL(kerSysSetOpticalPowerValues);
EXPORT_SYMBOL(kerSysGetOpticalPowerValues);
#if defined(CONFIG_BCM96368)
EXPORT_SYMBOL(kerSysGetSdramWidth);
#endif
EXPORT_SYMBOL(kerSysLedCtrl);
EXPORT_SYMBOL(kerSysRegisterDyingGaspHandler);
EXPORT_SYMBOL(kerSysDeregisterDyingGaspHandler);
EXPORT_SYMBOL(kerSysSendtoMonitorTask);
EXPORT_SYMBOL(kerSysGetWlanSromParams);
EXPORT_SYMBOL(kerSysGetAfeId);
EXPORT_SYMBOL(kerSysGetUbusFreq);
#if defined(CONFIG_BCM96816)
EXPORT_SYMBOL(kerSysBcmSpiSlaveRead);
EXPORT_SYMBOL(kerSysBcmSpiSlaveReadReg32);
EXPORT_SYMBOL(kerSysBcmSpiSlaveWrite);
EXPORT_SYMBOL(kerSysBcmSpiSlaveWriteReg32);
EXPORT_SYMBOL(kerSysBcmSpiSlaveWriteBuf);
#endif
EXPORT_SYMBOL(BpGetBoardId);
EXPORT_SYMBOL(BpGetBoardIds);
EXPORT_SYMBOL(BpGetGPIOverlays);
EXPORT_SYMBOL(BpGetFpgaResetGpio);
EXPORT_SYMBOL(BpGetEthernetMacInfo);
EXPORT_SYMBOL(BpGetDeviceOptions);
#if (defined(CONFIG_BCM963268) || defined(CONFIG_BCM_6362_PORTS_INT_EXT_SW)) && defined(CONFIG_BCM_EXT_SWITCH) && defined(CONFIG_11ac_throughput_patch_from_412L08)
EXPORT_SYMBOL(BpGetPortConnectedToExtSwitch);
#endif
EXPORT_SYMBOL(BpGetRj11InnerOuterPairGpios);
EXPORT_SYMBOL(BpGetRtsCtsUartGpios);
EXPORT_SYMBOL(BpGetAdslLedGpio);
EXPORT_SYMBOL(BpGetAdslFailLedGpio);
EXPORT_SYMBOL(BpGetWanDataLedGpio);
#if 1 /* chchien */
EXPORT_SYMBOL(BpGetWanEth3LedGpio);
#endif
EXPORT_SYMBOL(BpGetWanErrorLedGpio);
EXPORT_SYMBOL(BpGetVoipLedGpio);
EXPORT_SYMBOL(BpGetPotsLedGpio);
EXPORT_SYMBOL(BpGetVoip2FailLedGpio);
EXPORT_SYMBOL(BpGetVoip2LedGpio);
EXPORT_SYMBOL(BpGetVoip1FailLedGpio);
EXPORT_SYMBOL(BpGetVoip1LedGpio);
EXPORT_SYMBOL(BpGetDectLedGpio);
EXPORT_SYMBOL(BpGetMoCALedGpio);
EXPORT_SYMBOL(BpGetMoCAFailLedGpio);
EXPORT_SYMBOL(BpGetWirelessSesExtIntr);
#if 1//__MSTC__, Eason, WLAN_WPS_BTN_SEPARATE
EXPORT_SYMBOL(BpGetWirelessBtnExtIntr);
#endif
EXPORT_SYMBOL(BpGetWirelessSesLedGpio);
EXPORT_SYMBOL(BpGetWirelessFlags);
EXPORT_SYMBOL(BpGetWirelessPowerDownGpio);
EXPORT_SYMBOL(BpUpdateWirelessSromMap);
EXPORT_SYMBOL(BpGetSecAdslLedGpio);
EXPORT_SYMBOL(BpGetSecAdslFailLedGpio);
EXPORT_SYMBOL(BpGetDslPhyAfeIds);
EXPORT_SYMBOL(BpGetExtAFEResetGpio);
EXPORT_SYMBOL(BpGetExtAFELDPwrGpio);
EXPORT_SYMBOL(BpGetExtAFELDModeGpio);
EXPORT_SYMBOL(BpGetIntAFELDPwrGpio);
EXPORT_SYMBOL(BpGetIntAFELDModeGpio);
EXPORT_SYMBOL(BpGetAFELDRelayGpio);
EXPORT_SYMBOL(BpGetExtAFELDDataGpio);
EXPORT_SYMBOL(BpGetExtAFELDClkGpio);
EXPORT_SYMBOL(BpGetUart2SdoutGpio);
EXPORT_SYMBOL(BpGetUart2SdinGpio);
EXPORT_SYMBOL(BpGet6829PortInfo);
EXPORT_SYMBOL(BpGetEthSpdLedGpio);
EXPORT_SYMBOL(BpGetLaserDisGpio);
EXPORT_SYMBOL(BpGetLaserTxPwrEnGpio);
EXPORT_SYMBOL(BpGetVregSel1P2);
EXPORT_SYMBOL(BpGetGponOpticsType);
EXPORT_SYMBOL(BpGetDefaultOpticalParams);
EXPORT_SYMBOL(BpGetMiiOverGpioFlag);
#if defined (CONFIG_BCM_ENDPOINT_MODULE)
EXPORT_SYMBOL(BpGetVoiceBoardId);
EXPORT_SYMBOL(BpGetVoiceBoardIds);
EXPORT_SYMBOL(BpGetVoiceParms);
#endif
#if defined (CONFIG_BCM_AVS_PWRSAVE) && !defined(CONFIG_11ac_throughput_patch_from_412L08)
EXPORT_SYMBOL(kerSysBcmEnableAvs);
EXPORT_SYMBOL(kerSysBcmAvsEnabled);
#endif
#if 1 //__MSTC__, RaynorChung: Ssupport USB LED   
EXPORT_SYMBOL(setUsbDeviceInserted);
#endif
