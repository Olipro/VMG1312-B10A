#include <linux/init.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/proc_fs.h>
#include <linux/version.h>
#include <bcm_map_part.h>
#include <board.h>

static struct task_struct *watchdog_tsk = NULL;
static int watchdog_enable = 0;
/* Default watchdog counter: 300 seconds */
static ulong wdt_counter = 300000000;
/* Default update counter timer: 5 seconds */
static long wdt_sleep = (5*HZ);

DEFINE_MUTEX(wdt_lock);

int mstc_wdt_init(void);
void mstc_wdt_exit(void);
int mstc_wdt_add_proc(void);
void mstc_wdt_del_proc(void);

static void kerSysSetWdTimer(ulong timeUs);
static void kerSysStopWdTimer(void);
static int watchdog_thread(void *data);
static int proc_wdt_enable_set(struct file *f, const char *buf, unsigned long cnt, void *data);
static int proc_wdt_enable_get(char *page, char **start, off_t off, int cnt, int *eof, void *data);
static int proc_wdt_timer_set(struct file *f, const char *buf, unsigned long cnt, void *data);
static int proc_wdt_timer_get(char *page, char **start, off_t off, int cnt, int *eof, void *data);

static void kerSysSetWdTimer(ulong timeUs)
{
	TIMER->WatchDogDefCount = timeUs * (FPERIPH/1000000);
	TIMER->WatchDogCtl = 0xFF00;
	TIMER->WatchDogCtl = 0x00FF;
}

static void kerSysStopWdTimer(void)
{
	TIMER->WatchDogCtl = 0xEE00;
	TIMER->WatchDogCtl = 0x00EE;
}

static int watchdog_thread(void *data)
{
	int sleep;

	do {
		mutex_lock(&wdt_lock);
		if(watchdog_enable != 1) {
			mutex_unlock(&wdt_lock);
			break;
		}

		/* set reboot time 30 seconds */
		kerSysSetWdTimer(wdt_counter);
		sleep = wdt_sleep;
		mutex_unlock(&wdt_lock);

		/* sleep 5 seconds */
		set_current_state(TASK_INTERRUPTIBLE);
        schedule_timeout(sleep);
	} while(!kthread_should_stop());

	return 0;
}

int mstc_wdt_init(void)
{
	if(watchdog_tsk != NULL) {
		printk("Watchdog Timer thread is alredy running.\n");
		return 0;
	}
	
	printk(KERN_INFO "Watchdog Timer Init -- kthread\n");

	mutex_lock(&wdt_lock);
	watchdog_enable = 1;
	mutex_unlock(&wdt_lock);

	watchdog_tsk = kthread_run(watchdog_thread, NULL, "watchdog_thread");
	if(IS_ERR(watchdog_tsk)) {
		printk(KERN_INFO "create watchdog_thread failed!\n");

		mutex_lock(&wdt_lock);
		watchdog_enable = 0;
		mutex_unlock(&wdt_lock);

		return -1;
	}

	return 0;
}

void mstc_wdt_exit(void)
{
	if(!IS_ERR(watchdog_tsk)) {
		printk(KERN_INFO "Watchdog Timer exit!\n");

		mutex_lock(&wdt_lock);
		kerSysStopWdTimer();
		watchdog_enable = 0;
		mutex_unlock(&wdt_lock);

		kthread_stop(watchdog_tsk);
		watchdog_tsk = NULL;
	} else {
		printk("Watchdog Timer thread is not exist.\n");
	}

	return;
}

static int proc_wdt_enable_set(struct file *f, const char *buf, unsigned long cnt, void *data)
{
	char input[4];

	memset(input, 0, sizeof(input));

	if ((cnt > 2) || (copy_from_user(input, buf, cnt) != 0)) {
		printk("Invalid value! Please input 0/1.");
		return -EFAULT;
	}

	input[1] = '\0';

	if(input[0] == '1') {
		mstc_wdt_init();
	} else if(input[0] == '0') {
		mstc_wdt_exit();
	} else {
		printk("Invalid value! Please input 0/1.");
		return -EFAULT;
	}

	return cnt;
}

static int proc_wdt_enable_get(char *page, char **start, off_t off, int cnt, int *eof, void *data)
{
	int len;

	mutex_lock(&wdt_lock);
	len = sprintf(page, "%d\n", watchdog_enable);
	mutex_unlock(&wdt_lock);

	return len;
}

static int proc_wdt_timer_set(struct file *f, const char *buf, unsigned long cnt, void *data)
{
	char input[256];
	int ret = -1;
	int counter = -1;
	int sleep = -1;
	int enable;

	memset(input, 0, sizeof(input));

	if ((cnt > 256) || (copy_from_user(input, buf, cnt) != 0)) {
		printk("Invalid value input!\n");
		return -EFAULT;
	}

	ret = sscanf(input, "%d %d\n", &counter, &sleep);
	if(ret != 2) {
		printk("Invalid value input!\n");
		return -EFAULT;
	}

	if((counter <= 0) || (sleep <= 0)) {
		printk("Invalid value input!\n");
		return -EFAULT;
	}

	if(counter < (2*sleep)) {
		printk("counter must be greater than (2*sleep) at least\n");
		return -EFAULT;
	}

	mutex_lock(&wdt_lock);
	wdt_counter = (counter*1000000);
	wdt_sleep = (sleep*HZ);
	enable = watchdog_enable;
	mutex_unlock(&wdt_lock);

	if(enable) {
		mstc_wdt_exit();
		mstc_wdt_init();
	}

	mutex_lock(&wdt_lock);
	enable = watchdog_enable;
	counter = (int)(wdt_counter/1000000);
	sleep = (int)(wdt_sleep/HZ);
	mutex_unlock(&wdt_lock);

	printk("Enable: %d\n", enable);
	printk("wdt_counter: %d sec\n", counter);
	printk("wdt_sleep: %d sec\n", sleep);

	return cnt;
}
static int proc_wdt_timer_get(char *page, char **start, off_t off, int cnt, int *eof, void *data)
{
	int len;
	int counter, sleep, enable;
	
	mutex_lock(&wdt_lock);
	enable = watchdog_enable;
	counter = (int)(wdt_counter/1000000);
	sleep = (int)(wdt_sleep/HZ);
	mutex_unlock(&wdt_lock);

	len = sprintf(page, "Enable: %d\nwdt_counter: %d sec\nwdt_sleep: %d sec\n", enable, counter, sleep);

	return len;
}

int mstc_wdt_add_proc(void)
{
	struct proc_dir_entry *wdt_dir = NULL;
	struct proc_dir_entry *wdt_enable = NULL;
	struct proc_dir_entry *wdt_timer = NULL;

	wdt_dir = proc_mkdir("wd_timer", NULL);
	if (wdt_dir == NULL) {
		printk(KERN_ERR "add_proc_files: failed to create wd_timer proc files!\n");
		return -1;
	}

	wdt_enable = create_proc_entry("enable", 0644, wdt_dir);
	if (wdt_enable == NULL) {
		printk(KERN_ERR "add_proc_files: failed to create wdt_enable proc files!\n");
		return -1;
	}
	wdt_enable->read_proc = proc_wdt_enable_get;
	wdt_enable->write_proc = proc_wdt_enable_set;
	#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
	/* New linux no longer requires proc_dir_entry->owner field. */
	#else
	wdt_enable->owner = THIS_MODULE;
	#endif

	wdt_timer = create_proc_entry("timer", 0644, wdt_dir);
	if (wdt_timer == NULL) {
		printk(KERN_ERR "add_proc_files: failed to create wdt_timer proc files!\n");
		return -1;
	}
	wdt_timer->read_proc = proc_wdt_timer_get;
	wdt_timer->write_proc = proc_wdt_timer_set;
	#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
	/* New linux no longer requires proc_dir_entry->owner field. */
	#else
	wdt_timer->owner = THIS_MODULE;
	#endif

	return 0;
}

void mstc_wdt_del_proc(void)
{
	remove_proc_entry("wd_timer", NULL);
	return;
}

EXPORT_SYMBOL(mstc_wdt_add_proc);
EXPORT_SYMBOL(mstc_wdt_del_proc);
EXPORT_SYMBOL(mstc_wdt_init);
EXPORT_SYMBOL(mstc_wdt_exit);