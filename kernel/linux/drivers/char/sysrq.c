/* -*- linux-c -*-
 *
 *	$Id: sysrq.c,v 1.15 1998/08/23 14:56:41 mj Exp $
 *
 *	Linux Magic System Request Key Hacks
 *
 *	(c) 1997 Martin Mares <mj@atrey.karlin.mff.cuni.cz>
 *	based on ideas by Pavel Machek <pavel@atrey.karlin.mff.cuni.cz>
 *
 *	(c) 2000 Crutcher Dunnavant <crutcher+kernel@datastacks.com>
 *	overhauled to use key registration
 *	based upon discusions in irc://irc.openprojects.net/#kernelnewbies
 */

#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/tty.h>
#include <linux/mount.h>
#include <linux/kdev_t.h>
#include <linux/major.h>
#include <linux/reboot.h>
#include <linux/sysrq.h>
#include <linux/kbd_kern.h>
#include <linux/proc_fs.h>
#include <linux/quotaops.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/suspend.h>
#include <linux/writeback.h>
#include <linux/buffer_head.h>		/* for fsync_bdev() */
#include <linux/swap.h>
#include <linux/spinlock.h>
#include <linux/vt_kern.h>
#include <linux/workqueue.h>
#include <linux/kexec.h>
#include <linux/hrtimer.h>
#include <linux/oom.h>

#include <asm/ptrace.h>
#include <asm/irq_regs.h>

/* Whether we react on sysrq keys or just ignore them */
int __read_mostly __sysrq_enabled = 1;

static int __read_mostly sysrq_always_enabled;

int sysrq_on(void)
{
	return __sysrq_enabled || sysrq_always_enabled;
}

/*
 * A value of 1 means 'all', other nonzero values are an op mask:
 */
static inline int sysrq_on_mask(int mask)
{
	return sysrq_always_enabled || __sysrq_enabled == 1 ||
						(__sysrq_enabled & mask);
}

static int __init sysrq_always_enabled_setup(char *str)
{
	sysrq_always_enabled = 1;
	printk(KERN_INFO "debug: sysrq always enabled.\n");

	return 1;
}

__setup("sysrq_always_enabled", sysrq_always_enabled_setup);


static void sysrq_handle_loglevel(int key, struct tty_struct *tty)
{
	int i;
	i = key - '0';
	console_loglevel = 7;
	printk("Loglevel set to %d\n", i);
	console_loglevel = i;
}
static struct sysrq_key_op sysrq_loglevel_op = {
	.handler	= sysrq_handle_loglevel,
	.help_msg	= "loglevel(0-9)",
	.action_msg	= "Changing Loglevel",
	.enable_mask	= SYSRQ_ENABLE_LOG,
};

#ifdef CONFIG_VT
static void sysrq_handle_SAK(int key, struct tty_struct *tty)
{
	struct work_struct *SAK_work = &vc_cons[fg_console].SAK_work;
	schedule_work(SAK_work);
}
static struct sysrq_key_op sysrq_SAK_op = {
	.handler	= sysrq_handle_SAK,
	.help_msg	= "saK",
	.action_msg	= "SAK",
	.enable_mask	= SYSRQ_ENABLE_KEYBOARD,
};
#else
#define sysrq_SAK_op (*(struct sysrq_key_op *)0)
#endif

#ifdef CONFIG_VT
static void sysrq_handle_unraw(int key, struct tty_struct *tty)
{
	struct kbd_struct *kbd = &kbd_table[fg_console];

	if (kbd)
		kbd->kbdmode = default_utf8 ? VC_UNICODE : VC_XLATE;
}
static struct sysrq_key_op sysrq_unraw_op = {
	.handler	= sysrq_handle_unraw,
	.help_msg	= "unRaw",
	.action_msg	= "Keyboard mode set to system default",
	.enable_mask	= SYSRQ_ENABLE_KEYBOARD,
};
#else
#define sysrq_unraw_op (*(struct sysrq_key_op *)0)
#endif /* CONFIG_VT */

#ifdef CONFIG_KEXEC
static void sysrq_handle_crashdump(int key, struct tty_struct *tty)
{
	crash_kexec(get_irq_regs());
}
static struct sysrq_key_op sysrq_crashdump_op = {
	.handler	= sysrq_handle_crashdump,
	.help_msg	= "Crashdump",
	.action_msg	= "Trigger a crashdump",
	.enable_mask	= SYSRQ_ENABLE_DUMP,
};
#else
#define sysrq_crashdump_op (*(struct sysrq_key_op *)0)
#endif

static void sysrq_handle_reboot(int key, struct tty_struct *tty)
{
	lockdep_off();
	local_irq_enable();
	emergency_restart();
}
static struct sysrq_key_op sysrq_reboot_op = {
	.handler	= sysrq_handle_reboot,
	.help_msg	= "reBoot",
	.action_msg	= "Resetting",
	.enable_mask	= SYSRQ_ENABLE_BOOT,
};

static void sysrq_handle_sync(int key, struct tty_struct *tty)
{
	emergency_sync();
}
static struct sysrq_key_op sysrq_sync_op = {
	.handler	= sysrq_handle_sync,
	.help_msg	= "Sync",
	.action_msg	= "Emergency Sync",
	.enable_mask	= SYSRQ_ENABLE_SYNC,
};

static void sysrq_handle_show_timers(int key, struct tty_struct *tty)
{
	sysrq_timer_list_show();
}

static struct sysrq_key_op sysrq_show_timers_op = {
	.handler	= sysrq_handle_show_timers,
	.help_msg	= "show-all-timers(Q)",
	.action_msg	= "Show clockevent devices & pending hrtimers (no others)",
};

static void sysrq_handle_mountro(int key, struct tty_struct *tty)
{
	emergency_remount();
}
static struct sysrq_key_op sysrq_mountro_op = {
	.handler	= sysrq_handle_mountro,
	.help_msg	= "Unmount",
	.action_msg	= "Emergency Remount R/O",
	.enable_mask	= SYSRQ_ENABLE_REMOUNT,
};

#ifdef CONFIG_LOCKDEP
static void sysrq_handle_showlocks(int key, struct tty_struct *tty)
{
	debug_show_all_locks();
}

static struct sysrq_key_op sysrq_showlocks_op = {
	.handler	= sysrq_handle_showlocks,
	.help_msg	= "show-all-locks(D)",
	.action_msg	= "Show Locks Held",
};
#else
#define sysrq_showlocks_op (*(struct sysrq_key_op *)0)
#endif

#if defined(CONFIG_SMP) || defined(CONFIG_MIPS_BRCM)
static DEFINE_SPINLOCK(show_lock);

static void showacpu(void *dummy)
{
	unsigned long flags;
#ifdef CONFIG_MIPS_BRCM
	struct pt_regs *regs = get_irq_regs();

	printk("=>entered showacpu on CPU %d: (idle=%d)\n",
	       smp_processor_id(), idle_cpu(smp_processor_id()));

	if (regs) {
		printk(KERN_INFO "=>calling show_regs:\n");
		show_regs(regs);
	}
#endif

	/* Idle CPUs have no interesting backtrace. */
	if (idle_cpu(smp_processor_id()))
		return;

	spin_lock_irqsave(&show_lock, flags);

	printk(KERN_INFO "CPU%d:\n", smp_processor_id());
#ifdef CONFIG_MIPS_BRCM
	printk(KERN_INFO "=>calling show_stack, current=%p ", current);
	if (current)
		printk(" (comm=%s)\n", current->comm);
	else
		printk("\n");

	show_stack(current, NULL);
#else
	show_stack(NULL, NULL);
#endif

	spin_unlock_irqrestore(&show_lock, flags);
}

static void sysrq_showregs_othercpus(struct work_struct *dummy)
{
	smp_call_function(showacpu, NULL, 0);
}

static DECLARE_WORK(sysrq_showallcpus, sysrq_showregs_othercpus);

static void sysrq_handle_showallcpus(int key, struct tty_struct *tty)
{
#ifdef CONFIG_MIPS_BRCM

	showacpu(NULL);

#ifdef CONFIG_SMP
	{
		int othercpu;
		othercpu = (0 == smp_processor_id()) ? 1 : 0;
		printk("=== Now call other CPU (%d)\n", othercpu);
		smp_call_function_single(othercpu, showacpu, (void *) 0xeeee, 0);
	}
#endif /* CONFIG_SMP */

#else
	struct pt_regs *regs = get_irq_regs();
	if (regs) {
		printk(KERN_INFO "CPU%d:\n", smp_processor_id());
		show_regs(regs);
	}
	schedule_work(&sysrq_showallcpus);
#endif
}

static struct sysrq_key_op sysrq_showallcpus_op = {
	.handler	= sysrq_handle_showallcpus,
	.help_msg	= "show-backtrace-all-active-cpus(L)",
	.action_msg	= "Show backtrace of all active CPUs",
	.enable_mask	= SYSRQ_ENABLE_DUMP,
};
#endif  /* CONFIG_MIPS || CONFIG_MIPS_BRCM */

static void sysrq_handle_showregs(int key, struct tty_struct *tty)
{
	struct pt_regs *regs = get_irq_regs();
	if (regs)
		show_regs(regs);
}
static struct sysrq_key_op sysrq_showregs_op = {
	.handler	= sysrq_handle_showregs,
	.help_msg	= "show-registers(P)",
	.action_msg	= "Show Regs",
	.enable_mask	= SYSRQ_ENABLE_DUMP,
};

static void sysrq_handle_showstate(int key, struct tty_struct *tty)
{
	show_state();
}
static struct sysrq_key_op sysrq_showstate_op = {
	.handler	= sysrq_handle_showstate,
	.help_msg	= "show-task-states(T)",
	.action_msg	= "Show State",
	.enable_mask	= SYSRQ_ENABLE_DUMP,
};

static void sysrq_handle_showstate_blocked(int key, struct tty_struct *tty)
{
	show_state_filter(TASK_UNINTERRUPTIBLE);
}
static struct sysrq_key_op sysrq_showstate_blocked_op = {
	.handler	= sysrq_handle_showstate_blocked,
	.help_msg	= "show-blocked-tasks(W)",
	.action_msg	= "Show Blocked State",
	.enable_mask	= SYSRQ_ENABLE_DUMP,
};

#ifdef CONFIG_TRACING
#include <linux/ftrace.h>

static void sysrq_ftrace_dump(int key, struct tty_struct *tty)
{
	ftrace_dump();
}
static struct sysrq_key_op sysrq_ftrace_dump_op = {
	.handler	= sysrq_ftrace_dump,
	.help_msg	= "dump-ftrace-buffer(Z)",
	.action_msg	= "Dump ftrace buffer",
	.enable_mask	= SYSRQ_ENABLE_DUMP,
};
#else
#define sysrq_ftrace_dump_op (*(struct sysrq_key_op *)0)
#endif

static void sysrq_handle_showmem(int key, struct tty_struct *tty)
{
	show_mem();
}
static struct sysrq_key_op sysrq_showmem_op = {
	.handler	= sysrq_handle_showmem,
	.help_msg	= "show-memory-usage(M)",
	.action_msg	= "Show Memory",
	.enable_mask	= SYSRQ_ENABLE_DUMP,
};

/*
 * Signal sysrq helper function.  Sends a signal to all user processes.
 */
static void send_sig_all(int sig)
{
	struct task_struct *p;

	for_each_process(p) {
		if (p->mm && !is_global_init(p))
			/* Not swapper, init nor kernel thread */
			force_sig(sig, p);
	}
}

static void sysrq_handle_term(int key, struct tty_struct *tty)
{
	send_sig_all(SIGTERM);
	console_loglevel = 8;
}
static struct sysrq_key_op sysrq_term_op = {
	.handler	= sysrq_handle_term,
	.help_msg	= "terminate-all-tasks(E)",
	.action_msg	= "Terminate All Tasks",
	.enable_mask	= SYSRQ_ENABLE_SIGNAL,
};

static void moom_callback(struct work_struct *ignored)
{
	out_of_memory(node_zonelist(0, GFP_KERNEL), GFP_KERNEL, 0);
}

static DECLARE_WORK(moom_work, moom_callback);

static void sysrq_handle_moom(int key, struct tty_struct *tty)
{
	schedule_work(&moom_work);
}
static struct sysrq_key_op sysrq_moom_op = {
	.handler	= sysrq_handle_moom,
	.help_msg	= "memory-full-oom-kill(F)",
	.action_msg	= "Manual OOM execution",
	.enable_mask	= SYSRQ_ENABLE_SIGNAL,
};

#ifdef CONFIG_BLOCK
static void sysrq_handle_thaw(int key, struct tty_struct *tty)
{
	emergency_thaw_all();
}
static struct sysrq_key_op sysrq_thaw_op = {
	.handler	= sysrq_handle_thaw,
	.help_msg	= "thaw-filesystems(J)",
	.action_msg	= "Emergency Thaw of all frozen filesystems",
	.enable_mask	= SYSRQ_ENABLE_SIGNAL,
};
#endif

static void sysrq_handle_kill(int key, struct tty_struct *tty)
{
	send_sig_all(SIGKILL);
	console_loglevel = 8;
}
static struct sysrq_key_op sysrq_kill_op = {
	.handler	= sysrq_handle_kill,
	.help_msg	= "kill-all-tasks(I)",
	.action_msg	= "Kill All Tasks",
	.enable_mask	= SYSRQ_ENABLE_SIGNAL,
};

static void sysrq_handle_unrt(int key, struct tty_struct *tty)
{
	normalize_rt_tasks();
}
static struct sysrq_key_op sysrq_unrt_op = {
	.handler	= sysrq_handle_unrt,
	.help_msg	= "nice-all-RT-tasks(N)",
	.action_msg	= "Nice All RT Tasks",
	.enable_mask	= SYSRQ_ENABLE_RTNICE,
};

#ifdef CONFIG_MIPS_BRCM

/*
 * This function is a "safe" function to call at anytime.  It should only
 * read data that do not require a lock and will not cause exceptions,
 * i.e. reading statistics but not dereferencing pointers.  It should not
 * assume interrupts are working on any of the CPU's.
 * The 'L' option is also good.
 */
static void sysrq_handle_cpu_summary(int key, struct tty_struct *tty)
{
	int cpu;
	int max_cpu=1;
#ifdef CONFIG_SMP
	max_cpu=2;
#endif


	printk("CPU summary invoked on cpu %d\n", smp_processor_id());
	for (cpu=0; cpu < max_cpu; cpu++) {
		if (cpu_online(cpu)) {
			printk("  cpu %d is online.\n", cpu);
		}
		else {
			printk("  WARNING: cpu %d is offline!\n", cpu);
		}
	}
	printk("\n\n");
	// dump interrupt statistics
	// print other CPU info that does not require getting a lock or
	// interrupts to be enabled on that CPU
}

static struct sysrq_key_op sysrq_cpu_summary_op = {
	.handler	= sysrq_handle_cpu_summary,
	.help_msg	= "BRCM: show summary status on all CPUs(A)",
	.action_msg	= "CPU Summary Status",
	.enable_mask	= 0x4000,   /* typically all flags will be enabled */
};


#include <linux/seq_file.h>
#include <linux/interrupt.h>

static char intrs_output_buf[1024];

static void sysrq_handle_show_intrs(int key, struct tty_struct *tty)
{
	struct seq_file intrs_seq_file;
	loff_t pos=0;

	// Just initialize the parts of seq_file that seq_printf needs
	memset(&intrs_seq_file, 0, sizeof(intrs_seq_file));
	intrs_seq_file.buf = intrs_output_buf;
	intrs_seq_file.size = sizeof(intrs_output_buf);

	/*
	 * Leverage the show_interrupts() function in kernel/mips/irq.c to dump
	 * to our buffer.
	 */
	while (pos <= NR_IRQS)
	{
		memset(intrs_output_buf, 0, sizeof(intrs_output_buf));
		intrs_seq_file.count = 0;
		show_interrupts(&intrs_seq_file, &pos);
		printk("%s", intrs_output_buf);
		pos++;
	}
}

static struct sysrq_key_op sysrq_show_intrs_op = {
	.handler	= sysrq_handle_show_intrs,
	.help_msg	= "BRCM: show interrupt counts on all CPUs(Y)",
	.action_msg	= "Interrupt Counts",
	.enable_mask	= 0x4000,   /* typically all flags will be enabled */
};

#endif /* CONFIG_MIPS_BRCM */

/* Key Operations table and lock */
static DEFINE_SPINLOCK(sysrq_key_table_lock);

static struct sysrq_key_op *sysrq_key_table[36] = {
	&sysrq_loglevel_op,		/* 0 */
	&sysrq_loglevel_op,		/* 1 */
	&sysrq_loglevel_op,		/* 2 */
	&sysrq_loglevel_op,		/* 3 */
	&sysrq_loglevel_op,		/* 4 */
	&sysrq_loglevel_op,		/* 5 */
	&sysrq_loglevel_op,		/* 6 */
	&sysrq_loglevel_op,		/* 7 */
	&sysrq_loglevel_op,		/* 8 */
	&sysrq_loglevel_op,		/* 9 */

#ifdef CONFIG_MIPS_BRCM
	&sysrq_cpu_summary_op,	/* a*/
#else
	/*
	 * a: Don't use for system provided sysrqs, it is handled specially on
	 * sparc and will never arrive.
	 */
	NULL,				/* a */
#endif
	&sysrq_reboot_op,		/* b */
	&sysrq_crashdump_op,		/* c & ibm_emac driver debug */
	&sysrq_showlocks_op,		/* d */
	&sysrq_term_op,			/* e */
	&sysrq_moom_op,			/* f */
	/* g: May be registered for the kernel debugger */
	NULL,				/* g */
	NULL,				/* h - reserved for help */
	&sysrq_kill_op,			/* i */
#ifdef CONFIG_BLOCK
	&sysrq_thaw_op,			/* j */
#else
	NULL,				/* j */
#endif
	&sysrq_SAK_op,			/* k */
#if defined(CONFIG_SMP) || defined(CONFIG_MIPS_BRCM)
	&sysrq_showallcpus_op,		/* l */
#else
	NULL,				/* l */
#endif
	&sysrq_showmem_op,		/* m */
	&sysrq_unrt_op,			/* n */
	/* o: This will often be registered as 'Off' at init time */
	NULL,				/* o */
	&sysrq_showregs_op,		/* p */
	&sysrq_show_timers_op,		/* q */
	&sysrq_unraw_op,		/* r */
	&sysrq_sync_op,			/* s */
	&sysrq_showstate_op,		/* t */
	&sysrq_mountro_op,		/* u */
	/* v: May be registered for frame buffer console restore */
	NULL,				/* v */
	&sysrq_showstate_blocked_op,	/* w */
	/* x: May be registered on ppc/powerpc for xmon */
	NULL,				/* x */
#ifdef CONFIG_MIPS_BRCM
	&sysrq_show_intrs_op,	/* y */
#else
	/* y: May be registered on sparc64 for global register dump */
	NULL,				/* y */
#endif
	&sysrq_ftrace_dump_op,		/* z */
};

/* key2index calculation, -1 on invalid index */
static int sysrq_key_table_key2index(int key)
{
	int retval;

	if ((key >= '0') && (key <= '9'))
		retval = key - '0';
	else if ((key >= 'a') && (key <= 'z'))
		retval = key + 10 - 'a';
#if defined(CONFIG_MIPS_BRCM)
	else if ((key >= 'A') && (key <= 'Z'))
		retval = key + 10 - 'A';
#endif
	else
		retval = -1;
	return retval;
}

/*
 * get and put functions for the table, exposed to modules.
 */
struct sysrq_key_op *__sysrq_get_key_op(int key)
{
        struct sysrq_key_op *op_p = NULL;
        int i;

	i = sysrq_key_table_key2index(key);
	if (i != -1)
	        op_p = sysrq_key_table[i];
        return op_p;
}

static void __sysrq_put_key_op(int key, struct sysrq_key_op *op_p)
{
        int i = sysrq_key_table_key2index(key);

        if (i != -1)
                sysrq_key_table[i] = op_p;
}

/*
 * This is the non-locking version of handle_sysrq.  It must/can only be called
 * by sysrq key handlers, as they are inside of the lock
 */
void __handle_sysrq(int key, struct tty_struct *tty, int check_mask)
{
	struct sysrq_key_op *op_p;
	int orig_log_level;
	int i;
	unsigned long flags;

#ifdef CONFIG_MIPS_BRCM
	printk("SysRq: intr handled on CPU %d\n", smp_processor_id());
#endif

	spin_lock_irqsave(&sysrq_key_table_lock, flags);
	/*
	 * Raise the apparent loglevel to maximum so that the sysrq header
	 * is shown to provide the user with positive feedback.  We do not
	 * simply emit this at KERN_EMERG as that would change message
	 * routing in the consumers of /proc/kmsg.
	 */
	orig_log_level = console_loglevel;
	console_loglevel = 7;
	printk(KERN_INFO "SysRq : ");

        op_p = __sysrq_get_key_op(key);
        if (op_p) {
		/*
		 * Should we check for enabled operations (/proc/sysrq-trigger
		 * should not) and is the invoked operation enabled?
		 */
		if (!check_mask || sysrq_on_mask(op_p->enable_mask)) {
			printk("%s\n", op_p->action_msg);
			console_loglevel = orig_log_level;
			op_p->handler(key, tty);
		} else {
			printk("This sysrq operation is disabled.\n");
		}
	} else {
		printk("HELP : ");
		/* Only print the help msg once per handler */
		for (i = 0; i < ARRAY_SIZE(sysrq_key_table); i++) {
			if (sysrq_key_table[i]) {
				int j;

				for (j = 0; sysrq_key_table[i] !=
						sysrq_key_table[j]; j++)
					;
				if (j != i)
					continue;
				printk("%s ", sysrq_key_table[i]->help_msg);
			}
		}
		printk("\n");
#if defined(CONFIG_MIPS_BRCM)
		printk("All commands are case insensitive.\n");
#endif
		console_loglevel = orig_log_level;
	}
	spin_unlock_irqrestore(&sysrq_key_table_lock, flags);
}

/*
 * This function is called by the keyboard handler when SysRq is pressed
 * and any other keycode arrives.
 */
void handle_sysrq(int key, struct tty_struct *tty)
{
	if (sysrq_on())
		__handle_sysrq(key, tty, 1);
}
EXPORT_SYMBOL(handle_sysrq);

static int __sysrq_swap_key_ops(int key, struct sysrq_key_op *insert_op_p,
                                struct sysrq_key_op *remove_op_p)
{

	int retval;
	unsigned long flags;

	spin_lock_irqsave(&sysrq_key_table_lock, flags);
	if (__sysrq_get_key_op(key) == remove_op_p) {
		__sysrq_put_key_op(key, insert_op_p);
		retval = 0;
	} else {
		retval = -1;
	}
	spin_unlock_irqrestore(&sysrq_key_table_lock, flags);
	return retval;
}

int register_sysrq_key(int key, struct sysrq_key_op *op_p)
{
	return __sysrq_swap_key_ops(key, op_p, NULL);
}
EXPORT_SYMBOL(register_sysrq_key);

int unregister_sysrq_key(int key, struct sysrq_key_op *op_p)
{
	return __sysrq_swap_key_ops(key, NULL, op_p);
}
EXPORT_SYMBOL(unregister_sysrq_key);

#ifdef CONFIG_PROC_FS
/*
 * writing 'C' to /proc/sysrq-trigger is like sysrq-C
 */
static ssize_t write_sysrq_trigger(struct file *file, const char __user *buf,
				   size_t count, loff_t *ppos)
{
	if (count) {
		char c;

		if (get_user(c, buf))
			return -EFAULT;
		__handle_sysrq(c, NULL, 0);
	}
	return count;
}

static const struct file_operations proc_sysrq_trigger_operations = {
	.write		= write_sysrq_trigger,
};

static int __init sysrq_init(void)
{
	proc_create("sysrq-trigger", S_IWUSR, NULL, &proc_sysrq_trigger_operations);
	return 0;
}
module_init(sysrq_init);
#endif
