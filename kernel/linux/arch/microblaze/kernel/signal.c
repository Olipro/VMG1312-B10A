/*
 * Signal handling
 *
 * Copyright (C) 2008-2009 Michal Simek <monstr@monstr.eu>
 * Copyright (C) 2008-2009 PetaLogix
 * Copyright (C) 2003,2004 John Williams <jwilliams@itee.uq.edu.au>
 * Copyright (C) 2001 NEC Corporation
 * Copyright (C) 2001 Miles Bader <miles@gnu.org>
 * Copyright (C) 1999,2000 Niibe Yutaka & Kaz Kojima
 * Copyright (C) 1991,1992 Linus Torvalds
 *
 * 1997-11-28 Modified for POSIX.1b signals by Richard Henderson
 *
 * This file was was derived from the sh version, arch/sh/kernel/signal.c
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License. See the file COPYING in the main directory of this
 * archive for more details.
 */

#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/kernel.h>
#include <linux/signal.h>
#include <linux/errno.h>
#include <linux/wait.h>
#include <linux/ptrace.h>
#include <linux/unistd.h>
#include <linux/stddef.h>
#include <linux/personality.h>
#include <linux/percpu.h>
#include <linux/linkage.h>
#include <asm/entry.h>
#include <asm/ucontext.h>
#include <linux/uaccess.h>
#include <asm/pgtable.h>
#include <asm/pgalloc.h>
#include <linux/syscalls.h>
#include <asm/cacheflush.h>
#include <asm/syscalls.h>

#define _BLOCKABLE (~(sigmask(SIGKILL) | sigmask(SIGSTOP)))

asmlinkage int do_signal(struct pt_regs *regs, sigset_t *oldset, int in_sycall);

/*
 * Atomically swap in the new signal mask, and wait for a signal.
 */
asmlinkage int
sys_sigsuspend(old_sigset_t mask, struct pt_regs *regs)
{
	sigset_t saveset;

	mask &= _BLOCKABLE;
	spin_lock_irq(&current->sighand->siglock);
	saveset = current->blocked;
	siginitset(&current->blocked, mask);
	recalc_sigpending();
	spin_unlock_irq(&current->sighand->siglock);

	regs->r3 = -EINTR;
	while (1) {
		current->state = TASK_INTERRUPTIBLE;
		schedule();
		if (do_signal(regs, &saveset, 1))
			return -EINTR;
	}
}

asmlinkage int
sys_rt_sigsuspend(sigset_t __user *unewset, size_t sigsetsize,
		struct pt_regs *regs)
{
	sigset_t saveset, newset;

	/* XXX: Don't preclude handling different sized sigset_t's. */
	if (sigsetsize != sizeof(sigset_t))
		return -EINVAL;

	if (copy_from_user(&newset, unewset, sizeof(newset)))
		return -EFAULT;
	sigdelsetmask(&newset, ~_BLOCKABLE);
	spin_lock_irq(&current->sighand->siglock);
	saveset = current->blocked;
	current->blocked = newset;
	recalc_sigpending();
	spin_unlock_irq(&current->sighand->siglock);

	regs->r3 = -EINTR;
	while (1) {
		current->state = TASK_INTERRUPTIBLE;
		schedule();
		if (do_signal(regs, &saveset, 1))
			return -EINTR;
	}
}

asmlinkage int
sys_sigaction(int sig, const struct old_sigaction *act,
		struct old_sigaction *oact)
{
	struct k_sigaction new_ka, old_ka;
	int ret;

	if (act) {
		old_sigset_t mask;
		if (!access_ok(VERIFY_READ, act, sizeof(*act)) ||
			__get_user(new_ka.sa.sa_handler, &act->sa_handler) ||
			__get_user(new_ka.sa.sa_restorer, &act->sa_restorer))
			return -EFAULT;
		__get_user(new_ka.sa.sa_flags, &act->sa_flags);
		__get_user(mask, &act->sa_mask);
		siginitset(&new_ka.sa.sa_mask, mask);
	}

	ret = do_sigaction(sig, act ? &new_ka : NULL, oact ? &old_ka : NULL);

	if (!ret && oact) {
		if (!access_ok(VERIFY_WRITE, oact, sizeof(*oact)) ||
			__put_user(old_ka.sa.sa_handler, &oact->sa_handler) ||
			__put_user(old_ka.sa.sa_restorer, &oact->sa_restorer))
			return -EFAULT;
		__put_user(old_ka.sa.sa_flags, &oact->sa_flags);
		__put_user(old_ka.sa.sa_mask.sig[0], &oact->sa_mask);
	}

	return ret;
}

asmlinkage int
sys_sigaltstack(const stack_t __user *uss, stack_t __user *uoss,
		struct pt_regs *regs)
{
	return do_sigaltstack(uss, uoss, regs->r1);
}

/*
 * Do a signal return; undo the signal stack.
 */

struct sigframe {
	struct sigcontext sc;
	unsigned long extramask[_NSIG_WORDS-1];
	unsigned long tramp[2];	/* signal trampoline */
};

struct rt_sigframe {
	struct siginfo info;
	struct ucontext uc;
	unsigned long tramp[2];	/* signal trampoline */
};

static int
restore_sigcontext(struct pt_regs *regs, struct sigcontext *sc, int *rval_p)
{
	unsigned int err = 0;

#define COPY(x)		{err |= __get_user(regs->x, &sc->regs.x); }
	COPY(r0);
	COPY(r1);
	COPY(r2);	COPY(r3);	COPY(r4);	COPY(r5);
	COPY(r6);	COPY(r7);	COPY(r8);	COPY(r9);
	COPY(r10);	COPY(r11);	COPY(r12);	COPY(r13);
	COPY(r14);	COPY(r15);	COPY(r16);	COPY(r17);
	COPY(r18);	COPY(r19);	COPY(r20);	COPY(r21);
	COPY(r22);	COPY(r23);	COPY(r24);	COPY(r25);
	COPY(r26);	COPY(r27);	COPY(r28);	COPY(r29);
	COPY(r30);	COPY(r31);
	COPY(pc);	COPY(ear);	COPY(esr);	COPY(fsr);
#undef COPY

	*rval_p = regs->r3;

	return err;
}

asmlinkage int sys_sigreturn(struct pt_regs *regs)
{
	struct sigframe *frame =
			(struct sigframe *)(regs->r1 + STATE_SAVE_ARG_SPACE);

	sigset_t set;
	int rval;

	if (!access_ok(VERIFY_READ, frame, sizeof(*frame)))
		goto badframe;

	if (__get_user(set.sig[0], &frame->sc.oldmask)
		|| (_NSIG_WORDS > 1
		&& __copy_from_user(&set.sig[1], &frame->extramask,
					sizeof(frame->extramask))))
		goto badframe;

	sigdelsetmask(&set, ~_BLOCKABLE);

	spin_lock_irq(&current->sighand->siglock);
	current->blocked = set;
	recalc_sigpending();
	spin_unlock_irq(&current->sighand->siglock);

	if (restore_sigcontext(regs, &frame->sc, &rval))
		goto badframe;
	return rval;

badframe:
	force_sig(SIGSEGV, current);
	return 0;
}

asmlinkage int sys_rt_sigreturn(struct pt_regs *regs)
{
	struct rt_sigframe *frame =
			(struct rt_sigframe *)(regs->r1 + STATE_SAVE_ARG_SPACE);

	sigset_t set;
	stack_t st;
	int rval;

	if (!access_ok(VERIFY_READ, frame, sizeof(*frame)))
		goto badframe;

	if (__copy_from_user(&set, &frame->uc.uc_sigmask, sizeof(set)))
		goto badframe;

	sigdelsetmask(&set, ~_BLOCKABLE);
	spin_lock_irq(&current->sighand->siglock);
	current->blocked = set;
	recalc_sigpending();
	spin_unlock_irq(&current->sighand->siglock);

	if (restore_sigcontext(regs, &frame->uc.uc_mcontext, &rval))
		goto badframe;

	if (__copy_from_user((void *)&st, &frame->uc.uc_stack, sizeof(st)))
		goto badframe;
	/* It is more difficult to avoid calling this function than to
	 call it and ignore errors. */
	do_sigaltstack(&st, NULL, regs->r1);

	return rval;

badframe:
	force_sig(SIGSEGV, current);
	return 0;
}

/*
 * Set up a signal frame.
 */

static int
setup_sigcontext(struct sigcontext *sc, struct pt_regs *regs,
		unsigned long mask)
{
	int err = 0;

#define COPY(x)		{err |= __put_user(regs->x, &sc->regs.x); }
	COPY(r0);
	COPY(r1);
	COPY(r2);	COPY(r3);	COPY(r4);	COPY(r5);
	COPY(r6);	COPY(r7);	COPY(r8);	COPY(r9);
	COPY(r10);	COPY(r11);	COPY(r12);	COPY(r13);
	COPY(r14);	COPY(r15);	COPY(r16);	COPY(r17);
	COPY(r18);	COPY(r19);	COPY(r20);	COPY(r21);
	COPY(r22);	COPY(r23);	COPY(r24);	COPY(r25);
	COPY(r26);	COPY(r27);	COPY(r28);	COPY(r29);
	COPY(r30);	COPY(r31);
	COPY(pc);	COPY(ear);	COPY(esr);	COPY(fsr);
#undef COPY

	err |= __put_user(mask, &sc->oldmask);

	return err;
}

/*
 * Determine which stack to use..
 */
static inline void *
get_sigframe(struct k_sigaction *ka, struct pt_regs *regs, size_t frame_size)
{
	/* Default to using normal stack */
	unsigned long sp = regs->r1;

	if ((ka->sa.sa_flags & SA_ONSTACK) != 0 && !on_sig_stack(sp))
		sp = current->sas_ss_sp + current->sas_ss_size;

	return (void *)((sp - frame_size) & -8UL);
}

static void setup_frame(int sig, struct k_sigaction *ka,
			sigset_t *set, struct pt_regs *regs)
{
	struct sigframe *frame;
	int err = 0;
	int signal;

	frame = get_sigframe(ka, regs, sizeof(*frame));

	if (!access_ok(VERIFY_WRITE, frame, sizeof(*frame)))
		goto give_sigsegv;

	signal = current_thread_info()->exec_domain
		&& current_thread_info()->exec_domain->signal_invmap
		&& sig < 32
		? current_thread_info()->exec_domain->signal_invmap[sig]
		: sig;

	err |= setup_sigcontext(&frame->sc, regs, set->sig[0]);

	if (_NSIG_WORDS > 1) {
		err |= __copy_to_user(frame->extramask, &set->sig[1],
					sizeof(frame->extramask));
	}

	/* Set up to return from userspace. If provided, use a stub
	 already in userspace. */
	/* minus 8 is offset to cater for "rtsd r15,8" offset */
	if (ka->sa.sa_flags & SA_RESTORER) {
		regs->r15 = ((unsigned long)ka->sa.sa_restorer)-8;
	} else {
		/* Note, these encodings are _big endian_! */

		/* addi r12, r0, __NR_sigreturn */
		err |= __put_user(0x31800000 | __NR_sigreturn ,
				frame->tramp + 0);
		/* brki r14, 0x8 */
		err |= __put_user(0xb9cc0008, frame->tramp + 1);

		/* Return from sighandler will jump to the tramp.
		 Negative 8 offset because return is rtsd r15, 8 */
		regs->r15 = ((unsigned long)frame->tramp)-8;

		__invalidate_cache_sigtramp((unsigned long)frame->tramp);
	}

	if (err)
		goto give_sigsegv;

	/* Set up registers for signal handler */
	regs->r1 = (unsigned long) frame - STATE_SAVE_ARG_SPACE;

	/* Signal handler args: */
	regs->r5 = signal; /* Arg 0: signum */
	regs->r6 = (unsigned long) &frame->sc; /* arg 1: sigcontext */

	/* Offset of 4 to handle microblaze rtid r14, 0 */
	regs->pc = (unsigned long)ka->sa.sa_handler;

	set_fs(USER_DS);

#ifdef DEBUG_SIG
	printk(KERN_INFO "SIG deliver (%s:%d): sp=%p pc=%08lx\n",
		current->comm, current->pid, frame, regs->pc);
#endif

	return;

give_sigsegv:
	if (sig == SIGSEGV)
		ka->sa.sa_handler = SIG_DFL;
	force_sig(SIGSEGV, current);
}

static void setup_rt_frame(int sig, struct k_sigaction *ka, siginfo_t *info,
			sigset_t *set, struct pt_regs *regs)
{
	struct rt_sigframe *frame;
	int err = 0;
	int signal;

	frame = get_sigframe(ka, regs, sizeof(*frame));

	if (!access_ok(VERIFY_WRITE, frame, sizeof(*frame)))
		goto give_sigsegv;

	signal = current_thread_info()->exec_domain
		&& current_thread_info()->exec_domain->signal_invmap
		&& sig < 32
		? current_thread_info()->exec_domain->signal_invmap[sig]
		: sig;

	err |= copy_siginfo_to_user(&frame->info, info);

	/* Create the ucontext. */
	err |= __put_user(0, &frame->uc.uc_flags);
	err |= __put_user(0, &frame->uc.uc_link);
	err |= __put_user((void *)current->sas_ss_sp,
			&frame->uc.uc_stack.ss_sp);
	err |= __put_user(sas_ss_flags(regs->r1),
			&frame->uc.uc_stack.ss_flags);
	err |= __put_user(current->sas_ss_size, &frame->uc.uc_stack.ss_size);
	err |= setup_sigcontext(&frame->uc.uc_mcontext,
			regs, set->sig[0]);
	err |= __copy_to_user(&frame->uc.uc_sigmask, set, sizeof(*set));

	/* Set up to return from userspace. If provided, use a stub
	 already in userspace. */
	/* minus 8 is offset to cater for "rtsd r15,8" */
	if (ka->sa.sa_flags & SA_RESTORER) {
		regs->r15 = ((unsigned long)ka->sa.sa_restorer)-8;
	} else {
		/* addi r12, r0, __NR_sigreturn */
		err |= __put_user(0x31800000 | __NR_rt_sigreturn ,
				frame->tramp + 0);
		/* brki r14, 0x8 */
		err |= __put_user(0xb9cc0008, frame->tramp + 1);

		/* Return from sighandler will jump to the tramp.
		 Negative 8 offset because return is rtsd r15, 8 */
		regs->r15 = ((unsigned long)frame->tramp)-8;

		__invalidate_cache_sigtramp((unsigned long)frame->tramp);
	}

	if (err)
		goto give_sigsegv;

	/* Set up registers for signal handler */
	regs->r1 = (unsigned long) frame - STATE_SAVE_ARG_SPACE;

	/* Signal handler args: */
	regs->r5 = signal; /* arg 0: signum */
	regs->r6 = (unsigned long) &frame->info; /* arg 1: siginfo */
	regs->r7 = (unsigned long) &frame->uc; /* arg2: ucontext */
	/* Offset to handle microblaze rtid r14, 0 */
	regs->pc = (unsigned long)ka->sa.sa_handler;

	set_fs(USER_DS);

#ifdef DEBUG_SIG
	printk(KERN_INFO "SIG deliver (%s:%d): sp=%p pc=%08lx\n",
		current->comm, current->pid, frame, regs->pc);
#endif

	return;

give_sigsegv:
	if (sig == SIGSEGV)
		ka->sa.sa_handler = SIG_DFL;
	force_sig(SIGSEGV, current);
}

/* Handle restarting system calls */
static inline void
handle_restart(struct pt_regs *regs, struct k_sigaction *ka, int has_handler)
{
	switch (regs->r3) {
	case -ERESTART_RESTARTBLOCK:
	case -ERESTARTNOHAND:
		if (!has_handler)
			goto do_restart;
		regs->r3 = -EINTR;
		break;
	case -ERESTARTSYS:
		if (has_handler && !(ka->sa.sa_flags & SA_RESTART)) {
			regs->r3 = -EINTR;
			break;
	}
	/* fallthrough */
	case -ERESTARTNOINTR:
do_restart:
		/* offset of 4 bytes to re-execute trap (brki) instruction */
		regs->pc -= 4;
		break;
	}
}

/*
 * OK, we're invoking a handler
 */

static void
handle_signal(unsigned long sig, struct k_sigaction *ka,
		siginfo_t *info, sigset_t *oldset, struct pt_regs *regs)
{
	/* Set up the stack frame */
	if (ka->sa.sa_flags & SA_SIGINFO)
		setup_rt_frame(sig, ka, info, oldset, regs);
	else
		setup_frame(sig, ka, oldset, regs);

	if (ka->sa.sa_flags & SA_ONESHOT)
		ka->sa.sa_handler = SIG_DFL;

	if (!(ka->sa.sa_flags & SA_NODEFER)) {
		spin_lock_irq(&current->sighand->siglock);
		sigorsets(&current->blocked,
				&current->blocked, &ka->sa.sa_mask);
		sigaddset(&current->blocked, sig);
		recalc_sigpending();
		spin_unlock_irq(&current->sighand->siglock);
	}
}

/*
 * Note that 'init' is a special process: it doesn't get signals it doesn't
 * want to handle. Thus you cannot kill init even with a SIGKILL even by
 * mistake.
 *
 * Note that we go through the signals twice: once to check the signals that
 * the kernel can handle, and then we build all the user-level signal handling
 * stack-frames in one go after that.
 */
int do_signal(struct pt_regs *regs, sigset_t *oldset, int in_syscall)
{
	siginfo_t info;
	int signr;
	struct k_sigaction ka;
#ifdef DEBUG_SIG
	printk(KERN_INFO "do signal: %p %p %d\n", regs, oldset, in_syscall);
	printk(KERN_INFO "do signal2: %lx %lx %ld [%lx]\n", regs->pc, regs->r1,
			regs->r12, current_thread_info()->flags);
#endif
	/*
	 * We want the common case to go fast, which
	 * is why we may in certain cases get here from
	 * kernel mode. Just return without doing anything
	 * if so.
	 */
	if (kernel_mode(regs))
		return 1;

	if (!oldset)
		oldset = &current->blocked;

	signr = get_signal_to_deliver(&info, &ka, regs, NULL);
	if (signr > 0) {
		/* Whee! Actually deliver the signal. */
		if (in_syscall)
			handle_restart(regs, &ka, 1);
		handle_signal(signr, &ka, &info, oldset, regs);
		return 1;
	}

	if (in_syscall)
		handle_restart(regs, NULL, 0);

	/* Did we come from a system call? */
	return 0;
}
