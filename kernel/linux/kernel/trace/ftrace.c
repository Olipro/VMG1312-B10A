/*
 * Infrastructure for profiling code inserted by 'gcc -pg'.
 *
 * Copyright (C) 2007-2008 Steven Rostedt <srostedt@redhat.com>
 * Copyright (C) 2004-2008 Ingo Molnar <mingo@redhat.com>
 *
 * Originally ported from the -rt patch by:
 *   Copyright (C) 2007 Arnaldo Carvalho de Melo <acme@redhat.com>
 *
 * Based on code in the latency_tracer, that is:
 *
 *  Copyright (C) 2004-2006 Ingo Molnar
 *  Copyright (C) 2004 William Lee Irwin III
 */

#include <linux/stop_machine.h>
#include <linux/clocksource.h>
#include <linux/kallsyms.h>
#include <linux/seq_file.h>
#include <linux/suspend.h>
#include <linux/debugfs.h>
#include <linux/hardirq.h>
#include <linux/kthread.h>
#include <linux/uaccess.h>
#include <linux/kprobes.h>
#include <linux/ftrace.h>
#include <linux/sysctl.h>
#include <linux/ctype.h>
#include <linux/list.h>
#include <linux/hash.h>

#include <trace/sched.h>

#include <asm/ftrace.h>

#include "trace.h"

#define FTRACE_WARN_ON(cond)			\
	do {					\
		if (WARN_ON(cond))		\
			ftrace_kill();		\
	} while (0)

#define FTRACE_WARN_ON_ONCE(cond)		\
	do {					\
		if (WARN_ON_ONCE(cond))		\
			ftrace_kill();		\
	} while (0)

/* hash bits for specific function selection */
#define FTRACE_HASH_BITS 7
#define FTRACE_FUNC_HASHSIZE (1 << FTRACE_HASH_BITS)

/* ftrace_enabled is a method to turn ftrace on or off */
int ftrace_enabled __read_mostly;
static int last_ftrace_enabled;

/* Quick disabling of function tracer. */
int function_trace_stop;

/*
 * ftrace_disabled is set when an anomaly is discovered.
 * ftrace_disabled is much stronger than ftrace_enabled.
 */
static int ftrace_disabled __read_mostly;

static DEFINE_MUTEX(ftrace_lock);

static struct ftrace_ops ftrace_list_end __read_mostly =
{
	.func = ftrace_stub,
};

static struct ftrace_ops *ftrace_list __read_mostly = &ftrace_list_end;
ftrace_func_t ftrace_trace_function __read_mostly = ftrace_stub;
ftrace_func_t __ftrace_trace_function __read_mostly = ftrace_stub;
ftrace_func_t ftrace_pid_function __read_mostly = ftrace_stub;

static void ftrace_list_func(unsigned long ip, unsigned long parent_ip)
{
	struct ftrace_ops *op = ftrace_list;

	/* in case someone actually ports this to alpha! */
	read_barrier_depends();

	while (op != &ftrace_list_end) {
		/* silly alpha */
		read_barrier_depends();
		op->func(ip, parent_ip);
		op = op->next;
	};
}

static void ftrace_pid_func(unsigned long ip, unsigned long parent_ip)
{
	if (!test_tsk_trace_trace(current))
		return;

	ftrace_pid_function(ip, parent_ip);
}

static void set_ftrace_pid_function(ftrace_func_t func)
{
	/* do not set ftrace_pid_function to itself! */
	if (func != ftrace_pid_func)
		ftrace_pid_function = func;
}

/**
 * clear_ftrace_function - reset the ftrace function
 *
 * This NULLs the ftrace function and in essence stops
 * tracing.  There may be lag
 */
void clear_ftrace_function(void)
{
	ftrace_trace_function = ftrace_stub;
	__ftrace_trace_function = ftrace_stub;
	ftrace_pid_function = ftrace_stub;
}

#ifndef CONFIG_HAVE_FUNCTION_TRACE_MCOUNT_TEST
/*
 * For those archs that do not test ftrace_trace_stop in their
 * mcount call site, we need to do it from C.
 */
static void ftrace_test_stop_func(unsigned long ip, unsigned long parent_ip)
{
	if (function_trace_stop)
		return;

	__ftrace_trace_function(ip, parent_ip);
}
#endif

static int __register_ftrace_function(struct ftrace_ops *ops)
{
	ops->next = ftrace_list;
	/*
	 * We are entering ops into the ftrace_list but another
	 * CPU might be walking that list. We need to make sure
	 * the ops->next pointer is valid before another CPU sees
	 * the ops pointer included into the ftrace_list.
	 */
	smp_wmb();
	ftrace_list = ops;

	if (ftrace_enabled) {
		ftrace_func_t func;

		if (ops->next == &ftrace_list_end)
			func = ops->func;
		else
			func = ftrace_list_func;

		if (ftrace_pid_trace) {
			set_ftrace_pid_function(func);
			func = ftrace_pid_func;
		}

		/*
		 * For one func, simply call it directly.
		 * For more than one func, call the chain.
		 */
#ifdef CONFIG_HAVE_FUNCTION_TRACE_MCOUNT_TEST
		ftrace_trace_function = func;
#else
		__ftrace_trace_function = func;
		ftrace_trace_function = ftrace_test_stop_func;
#endif
	}

	return 0;
}

static int __unregister_ftrace_function(struct ftrace_ops *ops)
{
	struct ftrace_ops **p;

	/*
	 * If we are removing the last function, then simply point
	 * to the ftrace_stub.
	 */
	if (ftrace_list == ops && ops->next == &ftrace_list_end) {
		ftrace_trace_function = ftrace_stub;
		ftrace_list = &ftrace_list_end;
		return 0;
	}

	for (p = &ftrace_list; *p != &ftrace_list_end; p = &(*p)->next)
		if (*p == ops)
			break;

	if (*p != ops)
		return -1;

	*p = (*p)->next;

	if (ftrace_enabled) {
		/* If we only have one func left, then call that directly */
		if (ftrace_list->next == &ftrace_list_end) {
			ftrace_func_t func = ftrace_list->func;

			if (ftrace_pid_trace) {
				set_ftrace_pid_function(func);
				func = ftrace_pid_func;
			}
#ifdef CONFIG_HAVE_FUNCTION_TRACE_MCOUNT_TEST
			ftrace_trace_function = func;
#else
			__ftrace_trace_function = func;
#endif
		}
	}

	return 0;
}

static void ftrace_update_pid_func(void)
{
	ftrace_func_t func;

	if (ftrace_trace_function == ftrace_stub)
		return;

	func = ftrace_trace_function;

	if (ftrace_pid_trace) {
		set_ftrace_pid_function(func);
		func = ftrace_pid_func;
	} else {
		if (func == ftrace_pid_func)
			func = ftrace_pid_function;
	}

#ifdef CONFIG_HAVE_FUNCTION_TRACE_MCOUNT_TEST
	ftrace_trace_function = func;
#else
	__ftrace_trace_function = func;
#endif
}

/* set when tracing only a pid */
struct pid *ftrace_pid_trace;
static struct pid * const ftrace_swapper_pid = &init_struct_pid;

#ifdef CONFIG_DYNAMIC_FTRACE

#ifndef CONFIG_FTRACE_MCOUNT_RECORD
# error Dynamic ftrace depends on MCOUNT_RECORD
#endif

static struct hlist_head ftrace_func_hash[FTRACE_FUNC_HASHSIZE] __read_mostly;

struct ftrace_func_probe {
	struct hlist_node	node;
	struct ftrace_probe_ops	*ops;
	unsigned long		flags;
	unsigned long		ip;
	void			*data;
	struct rcu_head		rcu;
};


enum {
	FTRACE_ENABLE_CALLS		= (1 << 0),
	FTRACE_DISABLE_CALLS		= (1 << 1),
	FTRACE_UPDATE_TRACE_FUNC	= (1 << 2),
	FTRACE_ENABLE_MCOUNT		= (1 << 3),
	FTRACE_DISABLE_MCOUNT		= (1 << 4),
	FTRACE_START_FUNC_RET		= (1 << 5),
	FTRACE_STOP_FUNC_RET		= (1 << 6),
};

static int ftrace_filtered;

static struct dyn_ftrace *ftrace_new_addrs;

static DEFINE_MUTEX(ftrace_regex_lock);

struct ftrace_page {
	struct ftrace_page	*next;
	int			index;
	struct dyn_ftrace	records[];
};

#define ENTRIES_PER_PAGE \
  ((PAGE_SIZE - sizeof(struct ftrace_page)) / sizeof(struct dyn_ftrace))

/* estimate from running different kernels */
#define NR_TO_INIT		10000

static struct ftrace_page	*ftrace_pages_start;
static struct ftrace_page	*ftrace_pages;

static struct dyn_ftrace *ftrace_free_records;

/*
 * This is a double for. Do not use 'break' to break out of the loop,
 * you must use a goto.
 */
#define do_for_each_ftrace_rec(pg, rec)					\
	for (pg = ftrace_pages_start; pg; pg = pg->next) {		\
		int _____i;						\
		for (_____i = 0; _____i < pg->index; _____i++) {	\
			rec = &pg->records[_____i];

#define while_for_each_ftrace_rec()		\
		}				\
	}

#ifdef CONFIG_KPROBES

static int frozen_record_count;

static inline void freeze_record(struct dyn_ftrace *rec)
{
	if (!(rec->flags & FTRACE_FL_FROZEN)) {
		rec->flags |= FTRACE_FL_FROZEN;
		frozen_record_count++;
	}
}

static inline void unfreeze_record(struct dyn_ftrace *rec)
{
	if (rec->flags & FTRACE_FL_FROZEN) {
		rec->flags &= ~FTRACE_FL_FROZEN;
		frozen_record_count--;
	}
}

static inline int record_frozen(struct dyn_ftrace *rec)
{
	return rec->flags & FTRACE_FL_FROZEN;
}
#else
# define freeze_record(rec)			({ 0; })
# define unfreeze_record(rec)			({ 0; })
# define record_frozen(rec)			({ 0; })
#endif /* CONFIG_KPROBES */

static void ftrace_free_rec(struct dyn_ftrace *rec)
{
	rec->freelist = ftrace_free_records;
	ftrace_free_records = rec;
	rec->flags |= FTRACE_FL_FREE;
}

void ftrace_release(void *start, unsigned long size)
{
	struct dyn_ftrace *rec;
	struct ftrace_page *pg;
	unsigned long s = (unsigned long)start;
	unsigned long e = s + size;

	if (ftrace_disabled || !start)
		return;

	mutex_lock(&ftrace_lock);
	do_for_each_ftrace_rec(pg, rec) {
		if ((rec->ip >= s) && (rec->ip < e)) {
			/*
			 * rec->ip is changed in ftrace_free_rec()
			 * It should not between s and e if record was freed.
			 */
			FTRACE_WARN_ON(rec->flags & FTRACE_FL_FREE);
			ftrace_free_rec(rec);
		}
	} while_for_each_ftrace_rec();
	mutex_unlock(&ftrace_lock);
}

static struct dyn_ftrace *ftrace_alloc_dyn_node(unsigned long ip)
{
	struct dyn_ftrace *rec;

	/* First check for freed records */
	if (ftrace_free_records) {
		rec = ftrace_free_records;

		if (unlikely(!(rec->flags & FTRACE_FL_FREE))) {
			FTRACE_WARN_ON_ONCE(1);
			ftrace_free_records = NULL;
			return NULL;
		}

		ftrace_free_records = rec->freelist;
		memset(rec, 0, sizeof(*rec));
		return rec;
	}

	if (ftrace_pages->index == ENTRIES_PER_PAGE) {
		if (!ftrace_pages->next) {
			/* allocate another page */
			ftrace_pages->next =
				(void *)get_zeroed_page(GFP_KERNEL);
			if (!ftrace_pages->next)
				return NULL;
		}
		ftrace_pages = ftrace_pages->next;
	}

	return &ftrace_pages->records[ftrace_pages->index++];
}

static struct dyn_ftrace *
ftrace_record_ip(unsigned long ip)
{
	struct dyn_ftrace *rec;

	if (ftrace_disabled)
		return NULL;

	rec = ftrace_alloc_dyn_node(ip);
	if (!rec)
		return NULL;

	rec->ip = ip;
	rec->newlist = ftrace_new_addrs;
	ftrace_new_addrs = rec;

	return rec;
}

static void print_ip_ins(const char *fmt, unsigned char *p)
{
	int i;

	printk(KERN_CONT "%s", fmt);

	for (i = 0; i < MCOUNT_INSN_SIZE; i++)
		printk(KERN_CONT "%s%02x", i ? ":" : "", p[i]);
}

static void ftrace_bug(int failed, unsigned long ip)
{
	switch (failed) {
	case -EFAULT:
		FTRACE_WARN_ON_ONCE(1);
		pr_info("ftrace faulted on modifying ");
		print_ip_sym(ip);
		break;
	case -EINVAL:
		FTRACE_WARN_ON_ONCE(1);
		pr_info("ftrace failed to modify ");
		print_ip_sym(ip);
		print_ip_ins(" actual: ", (unsigned char *)ip);
		printk(KERN_CONT "\n");
		break;
	case -EPERM:
		FTRACE_WARN_ON_ONCE(1);
		pr_info("ftrace faulted on writing ");
		print_ip_sym(ip);
		break;
	default:
		FTRACE_WARN_ON_ONCE(1);
		pr_info("ftrace faulted on unknown error ");
		print_ip_sym(ip);
	}
}


static int
__ftrace_replace_code(struct dyn_ftrace *rec, int enable)
{
	unsigned long ftrace_addr;
	unsigned long ip, fl;

	ftrace_addr = (unsigned long)FTRACE_ADDR;

	ip = rec->ip;

	/*
	 * If this record is not to be traced and
	 * it is not enabled then do nothing.
	 *
	 * If this record is not to be traced and
	 * it is enabled then disable it.
	 *
	 */
	if (rec->flags & FTRACE_FL_NOTRACE) {
		if (rec->flags & FTRACE_FL_ENABLED)
			rec->flags &= ~FTRACE_FL_ENABLED;
		else
			return 0;

	} else if (ftrace_filtered && enable) {
		/*
		 * Filtering is on:
		 */

		fl = rec->flags & (FTRACE_FL_FILTER | FTRACE_FL_ENABLED);

		/* Record is filtered and enabled, do nothing */
		if (fl == (FTRACE_FL_FILTER | FTRACE_FL_ENABLED))
			return 0;

		/* Record is not filtered or enabled, do nothing */
		if (!fl)
			return 0;

		/* Record is not filtered but enabled, disable it */
		if (fl == FTRACE_FL_ENABLED)
			rec->flags &= ~FTRACE_FL_ENABLED;
		else
		/* Otherwise record is filtered but not enabled, enable it */
			rec->flags |= FTRACE_FL_ENABLED;
	} else {
		/* Disable or not filtered */

		if (enable) {
			/* if record is enabled, do nothing */
			if (rec->flags & FTRACE_FL_ENABLED)
				return 0;

			rec->flags |= FTRACE_FL_ENABLED;

		} else {

			/* if record is not enabled, do nothing */
			if (!(rec->flags & FTRACE_FL_ENABLED))
				return 0;

			rec->flags &= ~FTRACE_FL_ENABLED;
		}
	}

	if (rec->flags & FTRACE_FL_ENABLED)
		return ftrace_make_call(rec, ftrace_addr);
	else
		return ftrace_make_nop(NULL, rec, ftrace_addr);
}

static void ftrace_replace_code(int enable)
{
	struct dyn_ftrace *rec;
	struct ftrace_page *pg;
	int failed;

	do_for_each_ftrace_rec(pg, rec) {
		/*
		 * Skip over free records, records that have
		 * failed and not converted.
		 */
		if (rec->flags & FTRACE_FL_FREE ||
		    rec->flags & FTRACE_FL_FAILED ||
		    !(rec->flags & FTRACE_FL_CONVERTED))
			continue;

		/* ignore updates to this record's mcount site */
		if (get_kprobe((void *)rec->ip)) {
			freeze_record(rec);
			continue;
		} else {
			unfreeze_record(rec);
		}

		failed = __ftrace_replace_code(rec, enable);
		if (failed) {
			rec->flags |= FTRACE_FL_FAILED;
			if ((system_state == SYSTEM_BOOTING) ||
			    !core_kernel_text(rec->ip)) {
				ftrace_free_rec(rec);
				} else {
				ftrace_bug(failed, rec->ip);
					/* Stop processing */
					return;
				}
		}
	} while_for_each_ftrace_rec();
}

static int
ftrace_code_disable(struct module *mod, struct dyn_ftrace *rec)
{
	unsigned long ip;
	int ret;

	ip = rec->ip;

	ret = ftrace_make_nop(mod, rec, MCOUNT_ADDR);
	if (ret) {
		ftrace_bug(ret, ip);
		rec->flags |= FTRACE_FL_FAILED;
		return 0;
	}
	return 1;
}

/*
 * archs can override this function if they must do something
 * before the modifying code is performed.
 */
int __weak ftrace_arch_code_modify_prepare(void)
{
	return 0;
}

/*
 * archs can override this function if they must do something
 * after the modifying code is performed.
 */
int __weak ftrace_arch_code_modify_post_process(void)
{
	return 0;
}

static int __ftrace_modify_code(void *data)
{
	int *command = data;

	if (*command & FTRACE_ENABLE_CALLS)
		ftrace_replace_code(1);
	else if (*command & FTRACE_DISABLE_CALLS)
		ftrace_replace_code(0);

	if (*command & FTRACE_UPDATE_TRACE_FUNC)
		ftrace_update_ftrace_func(ftrace_trace_function);

	if (*command & FTRACE_START_FUNC_RET)
		ftrace_enable_ftrace_graph_caller();
	else if (*command & FTRACE_STOP_FUNC_RET)
		ftrace_disable_ftrace_graph_caller();

	return 0;
}

static void ftrace_run_update_code(int command)
{
	int ret;

	ret = ftrace_arch_code_modify_prepare();
	FTRACE_WARN_ON(ret);
	if (ret)
		return;

	stop_machine(__ftrace_modify_code, &command, NULL);

	ret = ftrace_arch_code_modify_post_process();
	FTRACE_WARN_ON(ret);
}

static ftrace_func_t saved_ftrace_func;
static int ftrace_start_up;

static void ftrace_startup_enable(int command)
{
	if (saved_ftrace_func != ftrace_trace_function) {
		saved_ftrace_func = ftrace_trace_function;
		command |= FTRACE_UPDATE_TRACE_FUNC;
	}

	if (!command || !ftrace_enabled)
		return;

	ftrace_run_update_code(command);
}

static void ftrace_startup(int command)
{
	if (unlikely(ftrace_disabled))
		return;

	ftrace_start_up++;
	command |= FTRACE_ENABLE_CALLS;

	ftrace_startup_enable(command);
}

static void ftrace_shutdown(int command)
{
	if (unlikely(ftrace_disabled))
		return;

	ftrace_start_up--;
	if (!ftrace_start_up)
		command |= FTRACE_DISABLE_CALLS;

	if (saved_ftrace_func != ftrace_trace_function) {
		saved_ftrace_func = ftrace_trace_function;
		command |= FTRACE_UPDATE_TRACE_FUNC;
	}

	if (!command || !ftrace_enabled)
		return;

	ftrace_run_update_code(command);
}

static void ftrace_startup_sysctl(void)
{
	int command = FTRACE_ENABLE_MCOUNT;

	if (unlikely(ftrace_disabled))
		return;

	/* Force update next time */
	saved_ftrace_func = NULL;
	/* ftrace_start_up is true if we want ftrace running */
	if (ftrace_start_up)
		command |= FTRACE_ENABLE_CALLS;

	ftrace_run_update_code(command);
}

static void ftrace_shutdown_sysctl(void)
{
	int command = FTRACE_DISABLE_MCOUNT;

	if (unlikely(ftrace_disabled))
		return;

	/* ftrace_start_up is true if ftrace is running */
	if (ftrace_start_up)
		command |= FTRACE_DISABLE_CALLS;

	ftrace_run_update_code(command);
}

static cycle_t		ftrace_update_time;
static unsigned long	ftrace_update_cnt;
unsigned long		ftrace_update_tot_cnt;

static int ftrace_update_code(struct module *mod)
{
	struct dyn_ftrace *p;
	cycle_t start, stop;

	start = ftrace_now(raw_smp_processor_id());
	ftrace_update_cnt = 0;

	while (ftrace_new_addrs) {

		/* If something went wrong, bail without enabling anything */
		if (unlikely(ftrace_disabled))
			return -1;

		p = ftrace_new_addrs;
		ftrace_new_addrs = p->newlist;
		p->flags = 0L;

		/* convert record (i.e, patch mcount-call with NOP) */
		if (ftrace_code_disable(mod, p)) {
			p->flags |= FTRACE_FL_CONVERTED;
			ftrace_update_cnt++;
		} else
			ftrace_free_rec(p);
	}

	stop = ftrace_now(raw_smp_processor_id());
	ftrace_update_time = stop - start;
	ftrace_update_tot_cnt += ftrace_update_cnt;

	return 0;
}

static int __init ftrace_dyn_table_alloc(unsigned long num_to_init)
{
	struct ftrace_page *pg;
	int cnt;
	int i;

	/* allocate a few pages */
	ftrace_pages_start = (void *)get_zeroed_page(GFP_KERNEL);
	if (!ftrace_pages_start)
		return -1;

	/*
	 * Allocate a few more pages.
	 *
	 * TODO: have some parser search vmlinux before
	 *   final linking to find all calls to ftrace.
	 *   Then we can:
	 *    a) know how many pages to allocate.
	 *     and/or
	 *    b) set up the table then.
	 *
	 *  The dynamic code is still necessary for
	 *  modules.
	 */

	pg = ftrace_pages = ftrace_pages_start;

	cnt = num_to_init / ENTRIES_PER_PAGE;
	pr_info("ftrace: allocating %ld entries in %d pages\n",
		num_to_init, cnt + 1);

	for (i = 0; i < cnt; i++) {
		pg->next = (void *)get_zeroed_page(GFP_KERNEL);

		/* If we fail, we'll try later anyway */
		if (!pg->next)
			break;

		pg = pg->next;
	}

	return 0;
}

enum {
	FTRACE_ITER_FILTER	= (1 << 0),
	FTRACE_ITER_CONT	= (1 << 1),
	FTRACE_ITER_NOTRACE	= (1 << 2),
	FTRACE_ITER_FAILURES	= (1 << 3),
	FTRACE_ITER_PRINTALL	= (1 << 4),
	FTRACE_ITER_HASH	= (1 << 5),
};

#define FTRACE_BUFF_MAX (KSYM_SYMBOL_LEN+4) /* room for wildcards */

struct ftrace_iterator {
	struct ftrace_page	*pg;
	int			hidx;
	int			idx;
	unsigned		flags;
	unsigned char		buffer[FTRACE_BUFF_MAX+1];
	unsigned		buffer_idx;
	unsigned		filtered;
};

static void *
t_hash_next(struct seq_file *m, void *v, loff_t *pos)
{
	struct ftrace_iterator *iter = m->private;
	struct hlist_node *hnd = v;
	struct hlist_head *hhd;

	WARN_ON(!(iter->flags & FTRACE_ITER_HASH));

	(*pos)++;

 retry:
	if (iter->hidx >= FTRACE_FUNC_HASHSIZE)
		return NULL;

	hhd = &ftrace_func_hash[iter->hidx];

	if (hlist_empty(hhd)) {
		iter->hidx++;
		hnd = NULL;
		goto retry;
	}

	if (!hnd)
		hnd = hhd->first;
	else {
		hnd = hnd->next;
		if (!hnd) {
			iter->hidx++;
			goto retry;
		}
	}

	return hnd;
}

static void *t_hash_start(struct seq_file *m, loff_t *pos)
{
	struct ftrace_iterator *iter = m->private;
	void *p = NULL;

	iter->flags |= FTRACE_ITER_HASH;

	return t_hash_next(m, p, pos);
}

static int t_hash_show(struct seq_file *m, void *v)
{
	struct ftrace_func_probe *rec;
	struct hlist_node *hnd = v;
	char str[KSYM_SYMBOL_LEN];

	rec = hlist_entry(hnd, struct ftrace_func_probe, node);

	if (rec->ops->print)
		return rec->ops->print(m, rec->ip, rec->ops, rec->data);

	kallsyms_lookup(rec->ip, NULL, NULL, NULL, str);
	seq_printf(m, "%s:", str);

	kallsyms_lookup((unsigned long)rec->ops->func, NULL, NULL, NULL, str);
	seq_printf(m, "%s", str);

	if (rec->data)
		seq_printf(m, ":%p", rec->data);
	seq_putc(m, '\n');

	return 0;
}

static void *
t_next(struct seq_file *m, void *v, loff_t *pos)
{
	struct ftrace_iterator *iter = m->private;
	struct dyn_ftrace *rec = NULL;

	if (iter->flags & FTRACE_ITER_HASH)
		return t_hash_next(m, v, pos);

	(*pos)++;

	if (iter->flags & FTRACE_ITER_PRINTALL)
		return NULL;

 retry:
	if (iter->idx >= iter->pg->index) {
		if (iter->pg->next) {
			iter->pg = iter->pg->next;
			iter->idx = 0;
			goto retry;
		} else {
			iter->idx = -1;
		}
	} else {
		rec = &iter->pg->records[iter->idx++];
		if ((rec->flags & FTRACE_FL_FREE) ||

		    (!(iter->flags & FTRACE_ITER_FAILURES) &&
		     (rec->flags & FTRACE_FL_FAILED)) ||

		    ((iter->flags & FTRACE_ITER_FAILURES) &&
		     !(rec->flags & FTRACE_FL_FAILED)) ||

		    ((iter->flags & FTRACE_ITER_FILTER) &&
		     !(rec->flags & FTRACE_FL_FILTER)) ||

		    ((iter->flags & FTRACE_ITER_NOTRACE) &&
		     !(rec->flags & FTRACE_FL_NOTRACE))) {
			rec = NULL;
			goto retry;
		}
	}

	return rec;
}

static void *t_start(struct seq_file *m, loff_t *pos)
{
	struct ftrace_iterator *iter = m->private;
	void *p = NULL;

	mutex_lock(&ftrace_lock);
	/*
	 * For set_ftrace_filter reading, if we have the filter
	 * off, we can short cut and just print out that all
	 * functions are enabled.
	 */
	if (iter->flags & FTRACE_ITER_FILTER && !ftrace_filtered) {
		if (*pos > 0)
			return t_hash_start(m, pos);
		iter->flags |= FTRACE_ITER_PRINTALL;
		(*pos)++;
		return iter;
	}

	if (iter->flags & FTRACE_ITER_HASH)
		return t_hash_start(m, pos);

	if (*pos > 0) {
		if (iter->idx < 0)
			return p;
		(*pos)--;
		iter->idx--;
	}

	p = t_next(m, p, pos);

	if (!p)
		return t_hash_start(m, pos);

	return p;
}

static void t_stop(struct seq_file *m, void *p)
{
	mutex_unlock(&ftrace_lock);
}

static int t_show(struct seq_file *m, void *v)
{
	struct ftrace_iterator *iter = m->private;
	struct dyn_ftrace *rec = v;
	char str[KSYM_SYMBOL_LEN];

	if (iter->flags & FTRACE_ITER_HASH)
		return t_hash_show(m, v);

	if (iter->flags & FTRACE_ITER_PRINTALL) {
		seq_printf(m, "#### all functions enabled ####\n");
		return 0;
	}

	if (!rec)
		return 0;

	kallsyms_lookup(rec->ip, NULL, NULL, NULL, str);

	seq_printf(m, "%s\n", str);

	return 0;
}

static struct seq_operations show_ftrace_seq_ops = {
	.start = t_start,
	.next = t_next,
	.stop = t_stop,
	.show = t_show,
};

static int
ftrace_avail_open(struct inode *inode, struct file *file)
{
	struct ftrace_iterator *iter;
	int ret;

	if (unlikely(ftrace_disabled))
		return -ENODEV;

	iter = kzalloc(sizeof(*iter), GFP_KERNEL);
	if (!iter)
		return -ENOMEM;

	iter->pg = ftrace_pages_start;

	ret = seq_open(file, &show_ftrace_seq_ops);
	if (!ret) {
		struct seq_file *m = file->private_data;

		m->private = iter;
	} else {
		kfree(iter);
	}

	return ret;
}

int ftrace_avail_release(struct inode *inode, struct file *file)
{
	struct seq_file *m = (struct seq_file *)file->private_data;
	struct ftrace_iterator *iter = m->private;

	seq_release(inode, file);
	kfree(iter);

	return 0;
}

static int
ftrace_failures_open(struct inode *inode, struct file *file)
{
	int ret;
	struct seq_file *m;
	struct ftrace_iterator *iter;

	ret = ftrace_avail_open(inode, file);
	if (!ret) {
		m = (struct seq_file *)file->private_data;
		iter = (struct ftrace_iterator *)m->private;
		iter->flags = FTRACE_ITER_FAILURES;
	}

	return ret;
}


static void ftrace_filter_reset(int enable)
{
	struct ftrace_page *pg;
	struct dyn_ftrace *rec;
	unsigned long type = enable ? FTRACE_FL_FILTER : FTRACE_FL_NOTRACE;

	mutex_lock(&ftrace_lock);
	if (enable)
		ftrace_filtered = 0;
	do_for_each_ftrace_rec(pg, rec) {
		if (rec->flags & FTRACE_FL_FAILED)
			continue;
		rec->flags &= ~type;
	} while_for_each_ftrace_rec();
	mutex_unlock(&ftrace_lock);
}

static int
ftrace_regex_open(struct inode *inode, struct file *file, int enable)
{
	struct ftrace_iterator *iter;
	int ret = 0;

	if (unlikely(ftrace_disabled))
		return -ENODEV;

	iter = kzalloc(sizeof(*iter), GFP_KERNEL);
	if (!iter)
		return -ENOMEM;

	mutex_lock(&ftrace_regex_lock);
	if ((file->f_mode & FMODE_WRITE) &&
	    !(file->f_flags & O_APPEND))
		ftrace_filter_reset(enable);

	if (file->f_mode & FMODE_READ) {
		iter->pg = ftrace_pages_start;
		iter->flags = enable ? FTRACE_ITER_FILTER :
			FTRACE_ITER_NOTRACE;

		ret = seq_open(file, &show_ftrace_seq_ops);
		if (!ret) {
			struct seq_file *m = file->private_data;
			m->private = iter;
		} else
			kfree(iter);
	} else
		file->private_data = iter;
	mutex_unlock(&ftrace_regex_lock);

	return ret;
}

static int
ftrace_filter_open(struct inode *inode, struct file *file)
{
	return ftrace_regex_open(inode, file, 1);
}

static int
ftrace_notrace_open(struct inode *inode, struct file *file)
{
	return ftrace_regex_open(inode, file, 0);
}

static loff_t
ftrace_regex_lseek(struct file *file, loff_t offset, int origin)
{
	loff_t ret;

	if (file->f_mode & FMODE_READ)
		ret = seq_lseek(file, offset, origin);
	else
		file->f_pos = ret = 1;

	return ret;
}

enum {
	MATCH_FULL,
	MATCH_FRONT_ONLY,
	MATCH_MIDDLE_ONLY,
	MATCH_END_ONLY,
};

/*
 * (static function - no need for kernel doc)
 *
 * Pass in a buffer containing a glob and this function will
 * set search to point to the search part of the buffer and
 * return the type of search it is (see enum above).
 * This does modify buff.
 *
 * Returns enum type.
 *  search returns the pointer to use for comparison.
 *  not returns 1 if buff started with a '!'
 *     0 otherwise.
 */
static int
ftrace_setup_glob(char *buff, int len, char **search, int *not)
{
	int type = MATCH_FULL;
	int i;

	if (buff[0] == '!') {
		*not = 1;
		buff++;
		len--;
	} else
		*not = 0;

	*search = buff;

	for (i = 0; i < len; i++) {
		if (buff[i] == '*') {
			if (!i) {
				*search = buff + 1;
				type = MATCH_END_ONLY;
			} else {
				if (type == MATCH_END_ONLY)
					type = MATCH_MIDDLE_ONLY;
				else
					type = MATCH_FRONT_ONLY;
				buff[i] = 0;
				break;
			}
		}
	}

	return type;
}

static int ftrace_match(char *str, char *regex, int len, int type)
{
	int matched = 0;
	char *ptr;

	switch (type) {
	case MATCH_FULL:
		if (strcmp(str, regex) == 0)
			matched = 1;
		break;
	case MATCH_FRONT_ONLY:
		if (strncmp(str, regex, len) == 0)
			matched = 1;
		break;
	case MATCH_MIDDLE_ONLY:
		if (strstr(str, regex))
			matched = 1;
		break;
	case MATCH_END_ONLY:
		ptr = strstr(str, regex);
		if (ptr && (ptr[len] == 0))
			matched = 1;
		break;
	}

	return matched;
}

static int
ftrace_match_record(struct dyn_ftrace *rec, char *regex, int len, int type)
{
	char str[KSYM_SYMBOL_LEN];

	kallsyms_lookup(rec->ip, NULL, NULL, NULL, str);
	return ftrace_match(str, regex, len, type);
}

static void ftrace_match_records(char *buff, int len, int enable)
{
	unsigned int search_len;
	struct ftrace_page *pg;
	struct dyn_ftrace *rec;
	unsigned long flag;
	char *search;
	int type;
	int not;

	flag = enable ? FTRACE_FL_FILTER : FTRACE_FL_NOTRACE;
	type = ftrace_setup_glob(buff, len, &search, &not);

	search_len = strlen(search);

	mutex_lock(&ftrace_lock);
	do_for_each_ftrace_rec(pg, rec) {

		if (rec->flags & FTRACE_FL_FAILED)
			continue;

		if (ftrace_match_record(rec, search, search_len, type)) {
			if (not)
				rec->flags &= ~flag;
			else
				rec->flags |= flag;
		}
		/*
		 * Only enable filtering if we have a function that
		 * is filtered on.
		 */
		if (enable && (rec->flags & FTRACE_FL_FILTER))
			ftrace_filtered = 1;
	} while_for_each_ftrace_rec();
	mutex_unlock(&ftrace_lock);
}

static int
ftrace_match_module_record(struct dyn_ftrace *rec, char *mod,
			   char *regex, int len, int type)
{
	char str[KSYM_SYMBOL_LEN];
	char *modname;

	kallsyms_lookup(rec->ip, NULL, NULL, &modname, str);

	if (!modname || strcmp(modname, mod))
		return 0;

	/* blank search means to match all funcs in the mod */
	if (len)
		return ftrace_match(str, regex, len, type);
	else
		return 1;
}

static void ftrace_match_module_records(char *buff, char *mod, int enable)
{
	unsigned search_len = 0;
	struct ftrace_page *pg;
	struct dyn_ftrace *rec;
	int type = MATCH_FULL;
	char *search = buff;
	unsigned long flag;
	int not = 0;

	flag = enable ? FTRACE_FL_FILTER : FTRACE_FL_NOTRACE;

	/* blank or '*' mean the same */
	if (strcmp(buff, "*") == 0)
		buff[0] = 0;

	/* handle the case of 'dont filter this module' */
	if (strcmp(buff, "!") == 0 || strcmp(buff, "!*") == 0) {
		buff[0] = 0;
		not = 1;
	}

	if (strlen(buff)) {
		type = ftrace_setup_glob(buff, strlen(buff), &search, &not);
		search_len = strlen(search);
	}

	mutex_lock(&ftrace_lock);
	do_for_each_ftrace_rec(pg, rec) {

		if (rec->flags & FTRACE_FL_FAILED)
			continue;

		if (ftrace_match_module_record(rec, mod,
					       search, search_len, type)) {
			if (not)
				rec->flags &= ~flag;
			else
				rec->flags |= flag;
		}
		if (enable && (rec->flags & FTRACE_FL_FILTER))
			ftrace_filtered = 1;

	} while_for_each_ftrace_rec();
	mutex_unlock(&ftrace_lock);
}

/*
 * We register the module command as a template to show others how
 * to register the a command as well.
 */

static int
ftrace_mod_callback(char *func, char *cmd, char *param, int enable)
{
	char *mod;

	/*
	 * cmd == 'mod' because we only registered this func
	 * for the 'mod' ftrace_func_command.
	 * But if you register one func with multiple commands,
	 * you can tell which command was used by the cmd
	 * parameter.
	 */

	/* we must have a module name */
	if (!param)
		return -EINVAL;

	mod = strsep(&param, ":");
	if (!strlen(mod))
		return -EINVAL;

	ftrace_match_module_records(func, mod, enable);
	return 0;
}

static struct ftrace_func_command ftrace_mod_cmd = {
	.name			= "mod",
	.func			= ftrace_mod_callback,
};

static int __init ftrace_mod_cmd_init(void)
{
	return register_ftrace_command(&ftrace_mod_cmd);
}
device_initcall(ftrace_mod_cmd_init);

static void
function_trace_probe_call(unsigned long ip, unsigned long parent_ip)
{
	struct ftrace_func_probe *entry;
	struct hlist_head *hhd;
	struct hlist_node *n;
	unsigned long key;
	int resched;

	key = hash_long(ip, FTRACE_HASH_BITS);

	hhd = &ftrace_func_hash[key];

	if (hlist_empty(hhd))
		return;

	/*
	 * Disable preemption for these calls to prevent a RCU grace
	 * period. This syncs the hash iteration and freeing of items
	 * on the hash. rcu_read_lock is too dangerous here.
	 */
	resched = ftrace_preempt_disable();
	hlist_for_each_entry_rcu(entry, n, hhd, node) {
		if (entry->ip == ip)
			entry->ops->func(ip, parent_ip, &entry->data);
	}
	ftrace_preempt_enable(resched);
}

static struct ftrace_ops trace_probe_ops __read_mostly =
{
	.func = function_trace_probe_call,
};

static int ftrace_probe_registered;

static void __enable_ftrace_function_probe(void)
{
	int i;

	if (ftrace_probe_registered)
		return;

	for (i = 0; i < FTRACE_FUNC_HASHSIZE; i++) {
		struct hlist_head *hhd = &ftrace_func_hash[i];
		if (hhd->first)
			break;
	}
	/* Nothing registered? */
	if (i == FTRACE_FUNC_HASHSIZE)
		return;

	__register_ftrace_function(&trace_probe_ops);
	ftrace_startup(0);
	ftrace_probe_registered = 1;
}

static void __disable_ftrace_function_probe(void)
{
	int i;

	if (!ftrace_probe_registered)
		return;

	for (i = 0; i < FTRACE_FUNC_HASHSIZE; i++) {
		struct hlist_head *hhd = &ftrace_func_hash[i];
		if (hhd->first)
			return;
	}

	/* no more funcs left */
	__unregister_ftrace_function(&trace_probe_ops);
	ftrace_shutdown(0);
	ftrace_probe_registered = 0;
}


static void ftrace_free_entry_rcu(struct rcu_head *rhp)
{
	struct ftrace_func_probe *entry =
		container_of(rhp, struct ftrace_func_probe, rcu);

	if (entry->ops->free)
		entry->ops->free(&entry->data);
	kfree(entry);
}


int
register_ftrace_function_probe(char *glob, struct ftrace_probe_ops *ops,
			      void *data)
{
	struct ftrace_func_probe *entry;
	struct ftrace_page *pg;
	struct dyn_ftrace *rec;
	int type, len, not;
	unsigned long key;
	int count = 0;
	char *search;

	type = ftrace_setup_glob(glob, strlen(glob), &search, &not);
	len = strlen(search);

	/* we do not support '!' for function probes */
	if (WARN_ON(not))
		return -EINVAL;

	mutex_lock(&ftrace_lock);
	do_for_each_ftrace_rec(pg, rec) {

		if (rec->flags & FTRACE_FL_FAILED)
			continue;

		if (!ftrace_match_record(rec, search, len, type))
			continue;

		entry = kmalloc(sizeof(*entry), GFP_KERNEL);
		if (!entry) {
			/* If we did not process any, then return error */
			if (!count)
				count = -ENOMEM;
			goto out_unlock;
		}

		count++;

		entry->data = data;

		/*
		 * The caller might want to do something special
		 * for each function we find. We call the callback
		 * to give the caller an opportunity to do so.
		 */
		if (ops->callback) {
			if (ops->callback(rec->ip, &entry->data) < 0) {
				/* caller does not like this func */
				kfree(entry);
				continue;
			}
		}

		entry->ops = ops;
		entry->ip = rec->ip;

		key = hash_long(entry->ip, FTRACE_HASH_BITS);
		hlist_add_head_rcu(&entry->node, &ftrace_func_hash[key]);

	} while_for_each_ftrace_rec();
	__enable_ftrace_function_probe();

 out_unlock:
	mutex_unlock(&ftrace_lock);

	return count;
}

enum {
	PROBE_TEST_FUNC		= 1,
	PROBE_TEST_DATA		= 2
};

static void
__unregister_ftrace_function_probe(char *glob, struct ftrace_probe_ops *ops,
				  void *data, int flags)
{
	struct ftrace_func_probe *entry;
	struct hlist_node *n, *tmp;
	char str[KSYM_SYMBOL_LEN];
	int type = MATCH_FULL;
	int i, len = 0;
	char *search;

	if (glob && (strcmp(glob, "*") || !strlen(glob)))
		glob = NULL;
	else {
		int not;

		type = ftrace_setup_glob(glob, strlen(glob), &search, &not);
		len = strlen(search);

		/* we do not support '!' for function probes */
		if (WARN_ON(not))
			return;
	}

	mutex_lock(&ftrace_lock);
	for (i = 0; i < FTRACE_FUNC_HASHSIZE; i++) {
		struct hlist_head *hhd = &ftrace_func_hash[i];

		hlist_for_each_entry_safe(entry, n, tmp, hhd, node) {

			/* break up if statements for readability */
			if ((flags & PROBE_TEST_FUNC) && entry->ops != ops)
				continue;

			if ((flags & PROBE_TEST_DATA) && entry->data != data)
				continue;

			/* do this last, since it is the most expensive */
			if (glob) {
				kallsyms_lookup(entry->ip, NULL, NULL,
						NULL, str);
				if (!ftrace_match(str, glob, len, type))
					continue;
			}

			hlist_del(&entry->node);
			call_rcu(&entry->rcu, ftrace_free_entry_rcu);
		}
	}
	__disable_ftrace_function_probe();
	mutex_unlock(&ftrace_lock);
}

void
unregister_ftrace_function_probe(char *glob, struct ftrace_probe_ops *ops,
				void *data)
{
	__unregister_ftrace_function_probe(glob, ops, data,
					  PROBE_TEST_FUNC | PROBE_TEST_DATA);
}

void
unregister_ftrace_function_probe_func(char *glob, struct ftrace_probe_ops *ops)
{
	__unregister_ftrace_function_probe(glob, ops, NULL, PROBE_TEST_FUNC);
}

void unregister_ftrace_function_probe_all(char *glob)
{
	__unregister_ftrace_function_probe(glob, NULL, NULL, 0);
}

static LIST_HEAD(ftrace_commands);
static DEFINE_MUTEX(ftrace_cmd_mutex);

int register_ftrace_command(struct ftrace_func_command *cmd)
{
	struct ftrace_func_command *p;
	int ret = 0;

	mutex_lock(&ftrace_cmd_mutex);
	list_for_each_entry(p, &ftrace_commands, list) {
		if (strcmp(cmd->name, p->name) == 0) {
			ret = -EBUSY;
			goto out_unlock;
		}
	}
	list_add(&cmd->list, &ftrace_commands);
 out_unlock:
	mutex_unlock(&ftrace_cmd_mutex);

	return ret;
}

int unregister_ftrace_command(struct ftrace_func_command *cmd)
{
	struct ftrace_func_command *p, *n;
	int ret = -ENODEV;

	mutex_lock(&ftrace_cmd_mutex);
	list_for_each_entry_safe(p, n, &ftrace_commands, list) {
		if (strcmp(cmd->name, p->name) == 0) {
			ret = 0;
			list_del_init(&p->list);
			goto out_unlock;
		}
	}
 out_unlock:
	mutex_unlock(&ftrace_cmd_mutex);

	return ret;
}

static int ftrace_process_regex(char *buff, int len, int enable)
{
	char *func, *command, *next = buff;
	struct ftrace_func_command *p;
	int ret = -EINVAL;

	func = strsep(&next, ":");

	if (!next) {
		ftrace_match_records(func, len, enable);
		return 0;
	}

	/* command found */

	command = strsep(&next, ":");

	mutex_lock(&ftrace_cmd_mutex);
	list_for_each_entry(p, &ftrace_commands, list) {
		if (strcmp(p->name, command) == 0) {
			ret = p->func(func, command, next, enable);
			goto out_unlock;
		}
	}
 out_unlock:
	mutex_unlock(&ftrace_cmd_mutex);

	return ret;
}

static ssize_t
ftrace_regex_write(struct file *file, const char __user *ubuf,
		   size_t cnt, loff_t *ppos, int enable)
{
	struct ftrace_iterator *iter;
	char ch;
	size_t read = 0;
	ssize_t ret;

	if (!cnt || cnt < 0)
		return 0;

	mutex_lock(&ftrace_regex_lock);

	if (file->f_mode & FMODE_READ) {
		struct seq_file *m = file->private_data;
		iter = m->private;
	} else
		iter = file->private_data;

	if (!*ppos) {
		iter->flags &= ~FTRACE_ITER_CONT;
		iter->buffer_idx = 0;
	}

	ret = get_user(ch, ubuf++);
	if (ret)
		goto out;
	read++;
	cnt--;

	if (!(iter->flags & ~FTRACE_ITER_CONT)) {
		/* skip white space */
		while (cnt && isspace(ch)) {
			ret = get_user(ch, ubuf++);
			if (ret)
				goto out;
			read++;
			cnt--;
		}

		if (isspace(ch)) {
			file->f_pos += read;
			ret = read;
			goto out;
		}

		iter->buffer_idx = 0;
	}

	while (cnt && !isspace(ch)) {
		if (iter->buffer_idx < FTRACE_BUFF_MAX)
			iter->buffer[iter->buffer_idx++] = ch;
		else {
			ret = -EINVAL;
			goto out;
		}
		ret = get_user(ch, ubuf++);
		if (ret)
			goto out;
		read++;
		cnt--;
	}

	if (isspace(ch)) {
		iter->filtered++;
		iter->buffer[iter->buffer_idx] = 0;
		ret = ftrace_process_regex(iter->buffer,
					   iter->buffer_idx, enable);
		if (ret)
			goto out;
		iter->buffer_idx = 0;
	} else
		iter->flags |= FTRACE_ITER_CONT;


	file->f_pos += read;

	ret = read;
 out:
	mutex_unlock(&ftrace_regex_lock);

	return ret;
}

static ssize_t
ftrace_filter_write(struct file *file, const char __user *ubuf,
		    size_t cnt, loff_t *ppos)
{
	return ftrace_regex_write(file, ubuf, cnt, ppos, 1);
}

static ssize_t
ftrace_notrace_write(struct file *file, const char __user *ubuf,
		     size_t cnt, loff_t *ppos)
{
	return ftrace_regex_write(file, ubuf, cnt, ppos, 0);
}

static void
ftrace_set_regex(unsigned char *buf, int len, int reset, int enable)
{
	if (unlikely(ftrace_disabled))
		return;

	mutex_lock(&ftrace_regex_lock);
	if (reset)
		ftrace_filter_reset(enable);
	if (buf)
		ftrace_match_records(buf, len, enable);
	mutex_unlock(&ftrace_regex_lock);
}

/**
 * ftrace_set_filter - set a function to filter on in ftrace
 * @buf - the string that holds the function filter text.
 * @len - the length of the string.
 * @reset - non zero to reset all filters before applying this filter.
 *
 * Filters denote which functions should be enabled when tracing is enabled.
 * If @buf is NULL and reset is set, all functions will be enabled for tracing.
 */
void ftrace_set_filter(unsigned char *buf, int len, int reset)
{
	ftrace_set_regex(buf, len, reset, 1);
}

/**
 * ftrace_set_notrace - set a function to not trace in ftrace
 * @buf - the string that holds the function notrace text.
 * @len - the length of the string.
 * @reset - non zero to reset all filters before applying this filter.
 *
 * Notrace Filters denote which functions should not be enabled when tracing
 * is enabled. If @buf is NULL and reset is set, all functions will be enabled
 * for tracing.
 */
void ftrace_set_notrace(unsigned char *buf, int len, int reset)
{
	ftrace_set_regex(buf, len, reset, 0);
}

static int
ftrace_regex_release(struct inode *inode, struct file *file, int enable)
{
	struct seq_file *m = (struct seq_file *)file->private_data;
	struct ftrace_iterator *iter;

	mutex_lock(&ftrace_regex_lock);
	if (file->f_mode & FMODE_READ) {
		iter = m->private;

		seq_release(inode, file);
	} else
		iter = file->private_data;

	if (iter->buffer_idx) {
		iter->filtered++;
		iter->buffer[iter->buffer_idx] = 0;
		ftrace_match_records(iter->buffer, iter->buffer_idx, enable);
	}

	mutex_lock(&ftrace_lock);
	if (ftrace_start_up && ftrace_enabled)
		ftrace_run_update_code(FTRACE_ENABLE_CALLS);
	mutex_unlock(&ftrace_lock);

	kfree(iter);
	mutex_unlock(&ftrace_regex_lock);
	return 0;
}

static int
ftrace_filter_release(struct inode *inode, struct file *file)
{
	return ftrace_regex_release(inode, file, 1);
}

static int
ftrace_notrace_release(struct inode *inode, struct file *file)
{
	return ftrace_regex_release(inode, file, 0);
}

static const struct file_operations ftrace_avail_fops = {
	.open = ftrace_avail_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = ftrace_avail_release,
};

static const struct file_operations ftrace_failures_fops = {
	.open = ftrace_failures_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = ftrace_avail_release,
};

static const struct file_operations ftrace_filter_fops = {
	.open = ftrace_filter_open,
	.read = seq_read,
	.write = ftrace_filter_write,
	.llseek = ftrace_regex_lseek,
	.release = ftrace_filter_release,
};

static const struct file_operations ftrace_notrace_fops = {
	.open = ftrace_notrace_open,
	.read = seq_read,
	.write = ftrace_notrace_write,
	.llseek = ftrace_regex_lseek,
	.release = ftrace_notrace_release,
};

#ifdef CONFIG_FUNCTION_GRAPH_TRACER

static DEFINE_MUTEX(graph_lock);

int ftrace_graph_count;
unsigned long ftrace_graph_funcs[FTRACE_GRAPH_MAX_FUNCS] __read_mostly;

static void *
g_next(struct seq_file *m, void *v, loff_t *pos)
{
	unsigned long *array = m->private;
	int index = *pos;

	(*pos)++;

	if (index >= ftrace_graph_count)
		return NULL;

	return &array[index];
}

static void *g_start(struct seq_file *m, loff_t *pos)
{
	void *p = NULL;

	mutex_lock(&graph_lock);

	/* Nothing, tell g_show to print all functions are enabled */
	if (!ftrace_graph_count && !*pos)
		return (void *)1;

	p = g_next(m, p, pos);

	return p;
}

static void g_stop(struct seq_file *m, void *p)
{
	mutex_unlock(&graph_lock);
}

static int g_show(struct seq_file *m, void *v)
{
	unsigned long *ptr = v;
	char str[KSYM_SYMBOL_LEN];

	if (!ptr)
		return 0;

	if (ptr == (unsigned long *)1) {
		seq_printf(m, "#### all functions enabled ####\n");
		return 0;
	}

	kallsyms_lookup(*ptr, NULL, NULL, NULL, str);

	seq_printf(m, "%s\n", str);

	return 0;
}

static struct seq_operations ftrace_graph_seq_ops = {
	.start = g_start,
	.next = g_next,
	.stop = g_stop,
	.show = g_show,
};

static int
ftrace_graph_open(struct inode *inode, struct file *file)
{
	int ret = 0;

	if (unlikely(ftrace_disabled))
		return -ENODEV;

	mutex_lock(&graph_lock);
	if ((file->f_mode & FMODE_WRITE) &&
	    !(file->f_flags & O_APPEND)) {
		ftrace_graph_count = 0;
		memset(ftrace_graph_funcs, 0, sizeof(ftrace_graph_funcs));
	}

	if (file->f_mode & FMODE_READ) {
		ret = seq_open(file, &ftrace_graph_seq_ops);
		if (!ret) {
			struct seq_file *m = file->private_data;
			m->private = ftrace_graph_funcs;
		}
	} else
		file->private_data = ftrace_graph_funcs;
	mutex_unlock(&graph_lock);

	return ret;
}

static int
ftrace_set_func(unsigned long *array, int *idx, char *buffer)
{
	struct dyn_ftrace *rec;
	struct ftrace_page *pg;
	int search_len;
	int found = 0;
	int type, not;
	char *search;
	bool exists;
	int i;

	if (ftrace_disabled)
		return -ENODEV;

	/* decode regex */
	type = ftrace_setup_glob(buffer, strlen(buffer), &search, &not);
	if (not)
		return -EINVAL;

	search_len = strlen(search);

	mutex_lock(&ftrace_lock);
	do_for_each_ftrace_rec(pg, rec) {

		if (*idx >= FTRACE_GRAPH_MAX_FUNCS)
			break;

		if (rec->flags & (FTRACE_FL_FAILED | FTRACE_FL_FREE))
			continue;

		if (ftrace_match_record(rec, search, search_len, type)) {
			/* ensure it is not already in the array */
			exists = false;
			for (i = 0; i < *idx; i++)
				if (array[i] == rec->ip) {
					exists = true;
					break;
				}
			if (!exists) {
				array[(*idx)++] = rec->ip;
				found = 1;
			}
		}
	} while_for_each_ftrace_rec();

	mutex_unlock(&ftrace_lock);

	return found ? 0 : -EINVAL;
}

static ssize_t
ftrace_graph_write(struct file *file, const char __user *ubuf,
		   size_t cnt, loff_t *ppos)
{
	unsigned char buffer[FTRACE_BUFF_MAX+1];
	unsigned long *array;
	size_t read = 0;
	ssize_t ret;
	int index = 0;
	char ch;

	if (!cnt || cnt < 0)
		return 0;

	mutex_lock(&graph_lock);

	if (ftrace_graph_count >= FTRACE_GRAPH_MAX_FUNCS) {
		ret = -EBUSY;
		goto out;
	}

	if (file->f_mode & FMODE_READ) {
		struct seq_file *m = file->private_data;
		array = m->private;
	} else
		array = file->private_data;

	ret = get_user(ch, ubuf++);
	if (ret)
		goto out;
	read++;
	cnt--;

	/* skip white space */
	while (cnt && isspace(ch)) {
		ret = get_user(ch, ubuf++);
		if (ret)
			goto out;
		read++;
		cnt--;
	}

	if (isspace(ch)) {
		*ppos += read;
		ret = read;
		goto out;
	}

	while (cnt && !isspace(ch)) {
		if (index < FTRACE_BUFF_MAX)
			buffer[index++] = ch;
		else {
			ret = -EINVAL;
			goto out;
		}
		ret = get_user(ch, ubuf++);
		if (ret)
			goto out;
		read++;
		cnt--;
	}
	buffer[index] = 0;

	/* we allow only one expression at a time */
	ret = ftrace_set_func(array, &ftrace_graph_count, buffer);
	if (ret)
		goto out;

	file->f_pos += read;

	ret = read;
 out:
	mutex_unlock(&graph_lock);

	return ret;
}

static const struct file_operations ftrace_graph_fops = {
	.open = ftrace_graph_open,
	.read = seq_read,
	.write = ftrace_graph_write,
};
#endif /* CONFIG_FUNCTION_GRAPH_TRACER */

static __init int ftrace_init_dyn_debugfs(struct dentry *d_tracer)
{
	struct dentry *entry;

	entry = debugfs_create_file("available_filter_functions", 0444,
				    d_tracer, NULL, &ftrace_avail_fops);
	if (!entry)
		pr_warning("Could not create debugfs "
			   "'available_filter_functions' entry\n");

	entry = debugfs_create_file("failures", 0444,
				    d_tracer, NULL, &ftrace_failures_fops);
	if (!entry)
		pr_warning("Could not create debugfs 'failures' entry\n");

	entry = debugfs_create_file("set_ftrace_filter", 0644, d_tracer,
				    NULL, &ftrace_filter_fops);
	if (!entry)
		pr_warning("Could not create debugfs "
			   "'set_ftrace_filter' entry\n");

	entry = debugfs_create_file("set_ftrace_notrace", 0644, d_tracer,
				    NULL, &ftrace_notrace_fops);
	if (!entry)
		pr_warning("Could not create debugfs "
			   "'set_ftrace_notrace' entry\n");

#ifdef CONFIG_FUNCTION_GRAPH_TRACER
	entry = debugfs_create_file("set_graph_function", 0444, d_tracer,
				    NULL,
				    &ftrace_graph_fops);
	if (!entry)
		pr_warning("Could not create debugfs "
			   "'set_graph_function' entry\n");
#endif /* CONFIG_FUNCTION_GRAPH_TRACER */

	return 0;
}

static int ftrace_convert_nops(struct module *mod,
			       unsigned long *start,
			       unsigned long *end)
{
	unsigned long *p;
	unsigned long addr;
	unsigned long flags;

	mutex_lock(&ftrace_lock);
	p = start;
	while (p < end) {
		addr = ftrace_call_adjust(*p++);
		/*
		 * Some architecture linkers will pad between
		 * the different mcount_loc sections of different
		 * object files to satisfy alignments.
		 * Skip any NULL pointers.
		 */
		if (!addr)
			continue;
		ftrace_record_ip(addr);
	}

	/* disable interrupts to prevent kstop machine */
	local_irq_save(flags);
	ftrace_update_code(mod);
	local_irq_restore(flags);
	mutex_unlock(&ftrace_lock);

	return 0;
}

void ftrace_init_module(struct module *mod,
			unsigned long *start, unsigned long *end)
{
	if (ftrace_disabled || start == end)
		return;
	ftrace_convert_nops(mod, start, end);
}

extern unsigned long __start_mcount_loc[];
extern unsigned long __stop_mcount_loc[];

void __init ftrace_init(void)
{
	unsigned long count, addr, flags;
	int ret;

	/* Keep the ftrace pointer to the stub */
	addr = (unsigned long)ftrace_stub;

	local_irq_save(flags);
	ftrace_dyn_arch_init(&addr);
	local_irq_restore(flags);

	/* ftrace_dyn_arch_init places the return code in addr */
	if (addr)
		goto failed;

	count = __stop_mcount_loc - __start_mcount_loc;

	ret = ftrace_dyn_table_alloc(count);
	if (ret)
		goto failed;

	last_ftrace_enabled = ftrace_enabled = 1;

	ret = ftrace_convert_nops(NULL,
				  __start_mcount_loc,
				  __stop_mcount_loc);

	return;
 failed:
	ftrace_disabled = 1;
}

#else

static int __init ftrace_nodyn_init(void)
{
	ftrace_enabled = 1;
	return 0;
}
device_initcall(ftrace_nodyn_init);

static inline int ftrace_init_dyn_debugfs(struct dentry *d_tracer) { return 0; }
static inline void ftrace_startup_enable(int command) { }
/* Keep as macros so we do not need to define the commands */
# define ftrace_startup(command)	do { } while (0)
# define ftrace_shutdown(command)	do { } while (0)
# define ftrace_startup_sysctl()	do { } while (0)
# define ftrace_shutdown_sysctl()	do { } while (0)
#endif /* CONFIG_DYNAMIC_FTRACE */

static ssize_t
ftrace_pid_read(struct file *file, char __user *ubuf,
		       size_t cnt, loff_t *ppos)
{
	char buf[64];
	int r;

	if (ftrace_pid_trace == ftrace_swapper_pid)
		r = sprintf(buf, "swapper tasks\n");
	else if (ftrace_pid_trace)
		r = sprintf(buf, "%u\n", pid_vnr(ftrace_pid_trace));
	else
		r = sprintf(buf, "no pid\n");

	return simple_read_from_buffer(ubuf, cnt, ppos, buf, r);
}

static void clear_ftrace_swapper(void)
{
	struct task_struct *p;
	int cpu;

	get_online_cpus();
	for_each_online_cpu(cpu) {
		p = idle_task(cpu);
		clear_tsk_trace_trace(p);
	}
	put_online_cpus();
}

static void set_ftrace_swapper(void)
{
	struct task_struct *p;
	int cpu;

	get_online_cpus();
	for_each_online_cpu(cpu) {
		p = idle_task(cpu);
		set_tsk_trace_trace(p);
	}
	put_online_cpus();
}

static void clear_ftrace_pid(struct pid *pid)
{
	struct task_struct *p;

	rcu_read_lock();
	do_each_pid_task(pid, PIDTYPE_PID, p) {
		clear_tsk_trace_trace(p);
	} while_each_pid_task(pid, PIDTYPE_PID, p);
	rcu_read_unlock();

	put_pid(pid);
}

static void set_ftrace_pid(struct pid *pid)
{
	struct task_struct *p;

	rcu_read_lock();
	do_each_pid_task(pid, PIDTYPE_PID, p) {
		set_tsk_trace_trace(p);
	} while_each_pid_task(pid, PIDTYPE_PID, p);
	rcu_read_unlock();
}

static void clear_ftrace_pid_task(struct pid **pid)
{
	if (*pid == ftrace_swapper_pid)
		clear_ftrace_swapper();
	else
		clear_ftrace_pid(*pid);

	*pid = NULL;
}

static void set_ftrace_pid_task(struct pid *pid)
{
	if (pid == ftrace_swapper_pid)
		set_ftrace_swapper();
	else
		set_ftrace_pid(pid);
}

static ssize_t
ftrace_pid_write(struct file *filp, const char __user *ubuf,
		   size_t cnt, loff_t *ppos)
{
	struct pid *pid;
	char buf[64];
	long val;
	int ret;

	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(&buf, ubuf, cnt))
		return -EFAULT;

	buf[cnt] = 0;

	ret = strict_strtol(buf, 10, &val);
	if (ret < 0)
		return ret;

	mutex_lock(&ftrace_lock);
	if (val < 0) {
		/* disable pid tracing */
		if (!ftrace_pid_trace)
			goto out;

		clear_ftrace_pid_task(&ftrace_pid_trace);

	} else {
		/* swapper task is special */
		if (!val) {
			pid = ftrace_swapper_pid;
			if (pid == ftrace_pid_trace)
				goto out;
		} else {
			pid = find_get_pid(val);

			if (pid == ftrace_pid_trace) {
				put_pid(pid);
				goto out;
			}
		}

		if (ftrace_pid_trace)
			clear_ftrace_pid_task(&ftrace_pid_trace);

		if (!pid)
			goto out;

		ftrace_pid_trace = pid;

		set_ftrace_pid_task(ftrace_pid_trace);
	}

	/* update the function call */
	ftrace_update_pid_func();
	ftrace_startup_enable(0);

 out:
	mutex_unlock(&ftrace_lock);

	return cnt;
}

static const struct file_operations ftrace_pid_fops = {
	.read = ftrace_pid_read,
	.write = ftrace_pid_write,
};

static __init int ftrace_init_debugfs(void)
{
	struct dentry *d_tracer;
	struct dentry *entry;

	d_tracer = tracing_init_dentry();
	if (!d_tracer)
		return 0;

	ftrace_init_dyn_debugfs(d_tracer);

	entry = debugfs_create_file("set_ftrace_pid", 0644, d_tracer,
				    NULL, &ftrace_pid_fops);
	if (!entry)
		pr_warning("Could not create debugfs "
			   "'set_ftrace_pid' entry\n");
	return 0;
}
fs_initcall(ftrace_init_debugfs);

/**
 * ftrace_kill - kill ftrace
 *
 * This function should be used by panic code. It stops ftrace
 * but in a not so nice way. If you need to simply kill ftrace
 * from a non-atomic section, use ftrace_kill.
 */
void ftrace_kill(void)
{
	ftrace_disabled = 1;
	ftrace_enabled = 0;
	clear_ftrace_function();
}

/**
 * register_ftrace_function - register a function for profiling
 * @ops - ops structure that holds the function for profiling.
 *
 * Register a function to be called by all functions in the
 * kernel.
 *
 * Note: @ops->func and all the functions it calls must be labeled
 *       with "notrace", otherwise it will go into a
 *       recursive loop.
 */
int register_ftrace_function(struct ftrace_ops *ops)
{
	int ret;

	if (unlikely(ftrace_disabled))
		return -1;

	mutex_lock(&ftrace_lock);

	ret = __register_ftrace_function(ops);
	ftrace_startup(0);

	mutex_unlock(&ftrace_lock);
	return ret;
}

/**
 * unregister_ftrace_function - unregister a function for profiling.
 * @ops - ops structure that holds the function to unregister
 *
 * Unregister a function that was added to be called by ftrace profiling.
 */
int unregister_ftrace_function(struct ftrace_ops *ops)
{
	int ret;

	mutex_lock(&ftrace_lock);
	ret = __unregister_ftrace_function(ops);
	ftrace_shutdown(0);
	mutex_unlock(&ftrace_lock);

	return ret;
}

int
ftrace_enable_sysctl(struct ctl_table *table, int write,
		     struct file *file, void __user *buffer, size_t *lenp,
		     loff_t *ppos)
{
	int ret;

	if (unlikely(ftrace_disabled))
		return -ENODEV;

	mutex_lock(&ftrace_lock);

	ret  = proc_dointvec(table, write, file, buffer, lenp, ppos);

	if (ret || !write || (last_ftrace_enabled == ftrace_enabled))
		goto out;

	last_ftrace_enabled = ftrace_enabled;

	if (ftrace_enabled) {

		ftrace_startup_sysctl();

		/* we are starting ftrace again */
		if (ftrace_list != &ftrace_list_end) {
			if (ftrace_list->next == &ftrace_list_end)
				ftrace_trace_function = ftrace_list->func;
			else
				ftrace_trace_function = ftrace_list_func;
		}

	} else {
		/* stopping ftrace calls (just send to ftrace_stub) */
		ftrace_trace_function = ftrace_stub;

		ftrace_shutdown_sysctl();
	}

 out:
	mutex_unlock(&ftrace_lock);
	return ret;
}

#ifdef CONFIG_FUNCTION_GRAPH_TRACER

static atomic_t ftrace_graph_active;
static struct notifier_block ftrace_suspend_notifier;

int ftrace_graph_entry_stub(struct ftrace_graph_ent *trace)
{
	return 0;
}

/* The callbacks that hook a function */
trace_func_graph_ret_t ftrace_graph_return =
			(trace_func_graph_ret_t)ftrace_stub;
trace_func_graph_ent_t ftrace_graph_entry = ftrace_graph_entry_stub;

/* Try to assign a return stack array on FTRACE_RETSTACK_ALLOC_SIZE tasks. */
static int alloc_retstack_tasklist(struct ftrace_ret_stack **ret_stack_list)
{
	int i;
	int ret = 0;
	unsigned long flags;
	int start = 0, end = FTRACE_RETSTACK_ALLOC_SIZE;
	struct task_struct *g, *t;

	for (i = 0; i < FTRACE_RETSTACK_ALLOC_SIZE; i++) {
		ret_stack_list[i] = kmalloc(FTRACE_RETFUNC_DEPTH
					* sizeof(struct ftrace_ret_stack),
					GFP_KERNEL);
		if (!ret_stack_list[i]) {
			start = 0;
			end = i;
			ret = -ENOMEM;
			goto free;
		}
	}

	read_lock_irqsave(&tasklist_lock, flags);
	do_each_thread(g, t) {
		if (start == end) {
			ret = -EAGAIN;
			goto unlock;
		}

		if (t->ret_stack == NULL) {
			t->curr_ret_stack = -1;
			/* Make sure IRQs see the -1 first: */
			barrier();
			t->ret_stack = ret_stack_list[start++];
			atomic_set(&t->tracing_graph_pause, 0);
			atomic_set(&t->trace_overrun, 0);
		}
	} while_each_thread(g, t);

unlock:
	read_unlock_irqrestore(&tasklist_lock, flags);
free:
	for (i = start; i < end; i++)
		kfree(ret_stack_list[i]);
	return ret;
}

static void
ftrace_graph_probe_sched_switch(struct rq *__rq, struct task_struct *prev,
				struct task_struct *next)
{
	unsigned long long timestamp;
	int index;

	/*
	 * Does the user want to count the time a function was asleep.
	 * If so, do not update the time stamps.
	 */
	if (trace_flags & TRACE_ITER_SLEEP_TIME)
		return;

	timestamp = trace_clock_local();

	prev->ftrace_timestamp = timestamp;

	/* only process tasks that we timestamped */
	if (!next->ftrace_timestamp)
		return;

	/*
	 * Update all the counters in next to make up for the
	 * time next was sleeping.
	 */
	timestamp -= next->ftrace_timestamp;

	for (index = next->curr_ret_stack; index >= 0; index--)
		next->ret_stack[index].calltime += timestamp;
}

/* Allocate a return stack for each task */
static int start_graph_tracing(void)
{
	struct ftrace_ret_stack **ret_stack_list;
	int ret, cpu;

	ret_stack_list = kmalloc(FTRACE_RETSTACK_ALLOC_SIZE *
				sizeof(struct ftrace_ret_stack *),
				GFP_KERNEL);

	if (!ret_stack_list)
		return -ENOMEM;

	/* The cpu_boot init_task->ret_stack will never be freed */
	for_each_online_cpu(cpu)
		ftrace_graph_init_task(idle_task(cpu));

	do {
		ret = alloc_retstack_tasklist(ret_stack_list);
	} while (ret == -EAGAIN);

	if (!ret) {
		ret = register_trace_sched_switch(ftrace_graph_probe_sched_switch);
		if (ret)
			pr_info("ftrace_graph: Couldn't activate tracepoint"
				" probe to kernel_sched_switch\n");
	}

	kfree(ret_stack_list);
	return ret;
}

/*
 * Hibernation protection.
 * The state of the current task is too much unstable during
 * suspend/restore to disk. We want to protect against that.
 */
static int
ftrace_suspend_notifier_call(struct notifier_block *bl, unsigned long state,
							void *unused)
{
	switch (state) {
	case PM_HIBERNATION_PREPARE:
		pause_graph_tracing();
		break;

	case PM_POST_HIBERNATION:
		unpause_graph_tracing();
		break;
	}
	return NOTIFY_DONE;
}

int register_ftrace_graph(trace_func_graph_ret_t retfunc,
			trace_func_graph_ent_t entryfunc)
{
	int ret = 0;

	mutex_lock(&ftrace_lock);

	/* we currently allow only one tracer registered at a time */
	if (atomic_read(&ftrace_graph_active)) {
		ret = -EBUSY;
		goto out;
	}

	ftrace_suspend_notifier.notifier_call = ftrace_suspend_notifier_call;
	register_pm_notifier(&ftrace_suspend_notifier);

	atomic_inc(&ftrace_graph_active);
	ret = start_graph_tracing();
	if (ret) {
		atomic_dec(&ftrace_graph_active);
		goto out;
	}

	ftrace_graph_return = retfunc;
	ftrace_graph_entry = entryfunc;

	ftrace_startup(FTRACE_START_FUNC_RET);

out:
	mutex_unlock(&ftrace_lock);
	return ret;
}

void unregister_ftrace_graph(void)
{
	mutex_lock(&ftrace_lock);

	if (!unlikely(atomic_read(&ftrace_graph_active)))
		goto out;

	atomic_dec(&ftrace_graph_active);
	unregister_trace_sched_switch(ftrace_graph_probe_sched_switch);
	ftrace_graph_return = (trace_func_graph_ret_t)ftrace_stub;
	ftrace_graph_entry = ftrace_graph_entry_stub;
	ftrace_shutdown(FTRACE_STOP_FUNC_RET);
	unregister_pm_notifier(&ftrace_suspend_notifier);

 out:
	mutex_unlock(&ftrace_lock);
}

/* Allocate a return stack for newly created task */
void ftrace_graph_init_task(struct task_struct *t)
{
	if (atomic_read(&ftrace_graph_active)) {
		t->ret_stack = kmalloc(FTRACE_RETFUNC_DEPTH
				* sizeof(struct ftrace_ret_stack),
				GFP_KERNEL);
		if (!t->ret_stack)
			return;
		t->curr_ret_stack = -1;
		atomic_set(&t->tracing_graph_pause, 0);
		atomic_set(&t->trace_overrun, 0);
		t->ftrace_timestamp = 0;
	} else
		t->ret_stack = NULL;
}

void ftrace_graph_exit_task(struct task_struct *t)
{
	struct ftrace_ret_stack	*ret_stack = t->ret_stack;

	t->ret_stack = NULL;
	/* NULL must become visible to IRQs before we free it: */
	barrier();

	kfree(ret_stack);
}

void ftrace_graph_stop(void)
{
	ftrace_stop();
}
#endif

