/*
 * Copyright (c) 1989, 1997-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_CPUVAR_H
#define	_SYS_CPUVAR_H

#pragma ident	"@(#)cpuvar.h	1.57	98/01/08 SMI"

#include <sys/thread.h>
#include <sys/sysinfo.h>	/* has cpu_stat_t definition */
#include <sys/gprof.h>
#include <sys/disp.h>
#include <sys/processor.h>

#if (defined(_KERNEL) || defined(_KMEMUSER)) && defined(_MACHDEP)
#include <sys/machcpuvar.h>
#endif

#include <sys/types.h>
#include <sys/file.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * This struct is defined to implement tracing on MPs.
 */

typedef struct tracedata {
	char	*tbuf_start;		/* start of ring buffer */
	char	*tbuf_end;		/* end of ring buffer */
	char	*tbuf_wrap;		/* wrap-around threshold */
	char	*tbuf_head;		/* where data is written to */
	char	*tbuf_tail;		/* where data is flushed from */
	char	*tbuf_redzone;		/* red zone in the ring buffer */
	char	*tbuf_overflow;		/* set if red zone is entered */
	uchar_t	*real_event_map;	/* event enabled/used bitmap */
	uchar_t	*event_map;		/* either real or null event map */
	struct file *trace_file;	/* file to flush to */
	uint32_t last_hrtime_lo32;	/* low 32 bits of hrtime at last TP */
	kthread_id_t	last_thread;	/* TID of thread at last TP */
	ulong_t	scratch[4];		/* traps-off TP register save area */
} tracedata_t;

/*
 * Per-CPU data.
 */
typedef struct cpu {
	processorid_t	cpu_id;		/* CPU number */
	processorid_t	cpu_seqid;	/* sequential CPU id (0..ncpus-1) */
	volatile ushort_t cpu_flags;	/* flags indicating CPU state */
	kthread_id_t	cpu_thread;		/* current thread */
	kthread_id_t	cpu_idle_thread; 	/* idle thread for this CPU */
	kthread_id_t	cpu_pause_thread;	/* pause thread for this CPU */
	klwp_id_t	cpu_lwp;		/* current lwp (if any) */
	klwp_id_t	cpu_fpowner;		/* currently loaded fpu owner */
	struct cpupart	*cpu_part;		/* partition with this CPU */
	int		cpu_cache_offset;	/* see kmem.c for details */

	/*
	 * Links - protected by cpu_lock.
	 */
	struct cpu	*cpu_next;	/* next existing CPU */
	struct cpu	*cpu_prev;

	struct cpu	*cpu_next_onln;	/* next online (enabled) CPU */
	struct cpu	*cpu_prev_onln;

	struct cpu	*cpu_next_part;	/* next CPU in partition */
	struct cpu	*cpu_prev_part;

	/*
	 * Scheduling variables.
	 */
	disp_t		cpu_disp;	/* dispatch queue data */
	char		cpu_runrun;	/* scheduling flag - set to preempt */
	char		cpu_kprunrun;	/* force kernel preemption */
	pri_t		cpu_chosen_level; /* priority level at which cpu */
					/* was chosen for scheduling */
	kthread_id_t	cpu_dispthread;	/* thread selected for dispatch */
	disp_lock_t	cpu_thread_lock; /* dispatcher lock on current thread */
	clock_t		cpu_last_swtch;	/* last time switched to new thread */

	/*
	 * Interrupt data.
	 */
	caddr_t		cpu_intr_stack;	/* interrupt stack */
	int		cpu_on_intr;	/* on interrupt stack */
	kthread_id_t	cpu_intr_thread; /* interrupt thread list */
	uint_t		cpu_intr_actv;	/* interrupt levels active (bitmask) */
	int		cpu_base_spl;	/* priority for highest rupt active */

	/*
	 * Statistics.
	 */
	cpu_stat_t	cpu_stat;	/* per cpu statistics */
	struct kstat	*cpu_kstat;	/* kstat for this cpu's statistics */

	struct	kern_profiling	*cpu_profiling; /* per cpu basis */
	tracedata_t	cpu_trace;	/* per cpu trace data */

	/*
	 * Configuration information for the processor_info system call.
	 */
	processor_info_t cpu_type_info;	/* config info */
	time_t	cpu_state_begin;	/* when CPU entered current state */
	char	cpu_cpr_flags;	/* CPR related info */

#if (defined(_KERNEL) || defined(_KMEMUSER)) && defined(_MACHDEP)
	/*
	 * XXX - needs to be fixed. Structure size should not change.
	 *	 probably needs to be a pointer to an opaque structure.
	 * XXX - this is OK as long as cpu structs aren't in an array.
	 *	 A user program will either read the first part,
	 *	 which is machine-independent, or read the whole thing.
	 */
	struct machcpu 	cpu_m;		/* per architecture info */
#endif
} cpu_t;

/* MEMBERS PROTECTED BY "atomicity": cpu_flags */

/*
 * Flags in the CPU structure.
 *
 * These are protected by cpu_lock (except during creation).
 *
 * Offlined-CPUs have three stages of being offline:
 *
 * CPU_ENABLE indicates that the CPU is participating in I/O interrupts
 * that can be directed at a number of different CPUs.  If CPU_ENABLE
 * is off, the CPU will not be given interrupts that can be sent elsewhere,
 * but will still get interrupts from devices associated with that CPU only,
 * and from other CPUs.
 *
 * CPU_OFFLINE indicates that the dispatcher should not allow any threads
 * other than interrupt threads to run on that CPU.  A CPU will not have
 * CPU_OFFLINE set if there are any bound threads (besides interrupts).
 *
 * CPU_QUIESCED is set if p_offline was able to completely turn idle the
 * CPU and it will not have to run interrupt threads.  In this case it'll
 * stay in the idle loop until CPU_QUIESCED is turned off.
 *
 * On some platforms CPUs can be individually powered off.
 * The following flags are set for powered off CPUs: CPU_QUIESCED,
 * CPU_OFFLINE, and CPU_POWEROFF.  The following flags are cleared:
 * CPU_RUNNING, CPU_READY, CPU_EXISTS, and CPU_ENABLE.
 */
#define	CPU_RUNNING	0x01		/* CPU running */
#define	CPU_READY	0x02		/* CPU ready for cross-calls */
#define	CPU_QUIESCED	0x04		/* CPU will stay in idle */
#define	CPU_EXISTS	0x08		/* CPU is configured */
#define	CPU_ENABLE	0x10		/* CPU enabled for interrupts */
#define	CPU_OFFLINE	0x20		/* CPU offline via p_online */
#define	CPU_POWEROFF	0x40		/* CPU is powered off */

#define	CPU_ACTIVE(cpu)	(((cpu)->cpu_flags & CPU_OFFLINE) == 0)

/*
 * Macros for manipulating sets of CPUs.  Note that this bit field may
 * vary in size depending on how many cpu's a specific architecture
 * may support. For now we define two sets of macros; one for
 * architectures supporting more than 32 cpus and one for architectures
 * supporting upto 32 cpus.
 */

#if NCPU > 32

#ifndef NBBY
#define	NBBY    8
#endif

#define	CPUSHIFT	5
#define	CPUBPM	(sizeof (uint32_t) * NBBY)  /* Number of bits in a mask */
#define	CPUMASKS(x, y)	(((x)+((y)-1))/(y)) /* Number of masks in a set */
#define	CPUSET_SIZE	CPUMASKS(NCPU, CPUBPM)

typedef struct cpuset {
	uint32_t	cpub[CPUSET_SIZE];
} cpuset_t;

extern	uint_t	cpuset_isnull(cpuset_t *);
extern	uint_t	cpuset_cmp(cpuset_t *, cpuset_t *);

#define	CPUSET(cpu)	((uint32_t)1 << ((cpu) & 0x1f))

/*
 * Making CPUSET_SIZE dynamic is nice.
 * But we have to make the CPUSET_ALL initializer dynamic too!!
 */
#if NCPU <= 64
#define	CPUSET_ALL	{~0U, ~0U}
#elif NCPU <= 96
#define	CPUSET_ALL	{~0U, ~0U, ~0U}
#else
#define	CPUSET_ALL	{~0U, ~0U, ~0U, ~0U}
#endif

#define	CPUSET_ALL_BUT(cpu)
#define	CPU_IN_SET(set, cpu)	(((set).cpub[(cpu)>>CPUSHIFT]) & CPUSET(cpu))
#define	CPUSET_ADD(set, cpu)	(((set).cpub[(cpu)>>CPUSHIFT]) |= CPUSET(cpu))
#define	CPUSET_DEL(set, cpu)	(((set).cpub[(cpu)>>CPUSHIFT]) &= ~CPUSET(cpu))
#define	CPUSET_ZERO(set)	bzero(&(set), sizeof (set))
#define	CPUSET_ISEQUAL(set1, set2)	(cpuset_cmp(&(set1), &(set2)))
#define	CPUSET_ISNULL(set)	cpuset_isnull(&(set))
#define	CPUSET_OR(set1, set2)	{			\
		int _i;					\
		uint32_t *_s1 = (uint32_t *)&(set1);	\
		uint32_t *_s2 = (uint32_t *)&(set2);	\
		for (_i = 0; _i < CPUSET_SIZE; _i++)	\
			*_s1++ |= *_s2++;		\
	}

#define	CPUSET_AND(set1, set2)	{			\
		int _i;					\
		uint32_t *_s1 = (uint32_t *)&(set1);	\
		uint32_t *_s2 = (uint32_t *)&(set2);	\
		for (_i = 0; _i < CPUSET_SIZE; _i++)	\
			*_s1++ &= *_s2++;		\
	}

#define	CPUSET_CAS(setp, cpu)	{					\
		uint32_t	_wix, _cwrd, _nwrd;			\
		extern	uint32_t cas32(uint32_t *, uint32_t, uint32_t);	\
									\
		_wix = (cpu) >> CPUSHIFT;				\
		do {							\
			_cwrd = (setp).cpub[_wix];			\
			_nwrd = _cwrd;					\
			_nwrd |= CPUSET((cpu));				\
			_nwrd = cas32(&(setp).cpub[_wix], _cwrd, _nwrd);\
		} while (_nwrd != _cwrd);				\
	}

#else	/* NCPU */

typedef	uint32_t	cpuset_t;	/* a set of CPUs */

#define	CPUSET(cpu)		(1 << (cpu))
#define	CPUSET_ALL		(~0U)
#define	CPUSET_ALL_BUT(cpu)	(~CPUSET(cpu))
#define	CPU_IN_SET(set, cpu)	((set) & CPUSET(cpu))
#define	CPUSET_ADD(set, cpu)	((set) |= CPUSET(cpu))
#define	CPUSET_DEL(set, cpu)	((set) &= ~CPUSET(cpu))
#define	CPUSET_OR(set1, set2)   { ((set1) |= (set2)); }
#define	CPUSET_AND(set1, set2)  { ((set1) &= (set2)); }
#define	CPUSET_ZERO(set)	((set) = 0)
#define	CPUSET_ISNULL(set)	((set) == 0)
#define	CPUSET_ISEQUAL(set1, set2)	((set1) == (set2))
#define	CPUSET_CAS(set, cpu)   {			\
		uint32_t    _cs, _ns;			\
		extern	uint32_t cas32(uint32_t *, uint32_t, uint32_t);	\
							\
		do {					\
			_cs = (set);			\
			_ns = _cs;			\
			CPUSET_ADD(_ns, (cpu));		\
			_ns = cas32(&(set), _cs, _ns);	\
		} while (_ns != _cs);			\
	}

#endif	/* NCPU	*/

#define	CPU_CPR_ONLINE		0x1
#define	CPU_CPR_IS_OFFLINE(cpu)	(((cpu)->cpu_cpr_flags & CPU_CPR_ONLINE) == 0)
#define	CPU_SET_CPR_FLAGS(cpu, flag)	((cpu)->cpu_cpr_flags |= flag)

extern struct cpu	*cpu[];		/* indexed by CPU number */
extern cpu_t		*cpu_list;	/* list of CPUs */
extern int		ncpus;		/* number of CPUs present */
extern int		ncpus_online;	/* number of CPUs not quiesced */
extern int		max_ncpus;	/* max present before ncpus is known */

#if defined(i386) || defined(__i386)
extern struct cpu *curcpup(void);
#define	CPU		(curcpup())	/* Pointer to current CPU */
#else
#define	CPU		(curthread->t_cpu)	/* Pointer to current CPU */
#endif

/*
 * Macros to update CPU statistics.
 *
 * CPU_STAT_ADD_K can be used when we want accurate counts, and we know
 * that an interrupt thread that could interrupt us will not try to
 * increment the same cpu stat.  The benefit of using these routines is
 * that we only increment t_kpreempt instead of acquiring a mutex.
 */

#define	CPU_STAT_ENTER_K()	kpreempt_disable()
#define	CPU_STAT_EXIT_K()	kpreempt_enable()

#define	CPU_STAT_ADD_K(thing, amount) \
	{	kpreempt_disable(); /* keep from switching CPUs */\
		CPU_STAT_ADDQ(CPU, thing, amount); \
		kpreempt_enable(); \
	}

#define	CPU_STAT_ADDQ(cpuptr, thing, amount) \
	cpuptr->cpu_stat.thing += amount

/*
 * CPU support routines.
 */
#if	defined(_KERNEL) && defined(__STDC__)	/* not for genassym.c */

/*
 * call_cpu() and call_cpu_soft() each call a function on another CPU.
 * The function may take one argument and return an int-sized value.
 * call_cpu() calls the function at a high interrupt priority, so the
 * function may not use mutexes or block.  call_cpu_soft() posts a low-
 * priority interrupt to the designated CPU to call the function, so
 * the function is allowed to use certain mutexes and to block.
 */
int	call_cpu(int cpun, int (*func)(), int arg);	/* call func on cpun */
int	call_cpu_soft(int cpun, int (*func)(), int arg); /* softcall cpun */
void	call_cpus(int (*func)(), int arg);
void	stop_cpus(char *msg);	 /* stop all CPUs (for panic) */
void	call_mon_enter(void);
void	call_prom_exit(void);

void	cpu_list_init(cpu_t *);
void	cpu_add_unit(cpu_t *);
void	cpu_add_active(cpu_t *);
void	cpu_kstat_init(cpu_t *);

void	mbox_lock_init(void);	 /* initialize cross-call locks */
void	mbox_init(int cpun);	 /* initialize cross-calls */
void	poke_cpu(int cpun);	 /* interrupt another CPU (to preempt) */

void	pause_cpus(cpu_t *off_cp);
void	start_cpus(void);

void	cpu_pause_init(void);
cpu_t	*cpu_get(processorid_t cpun);	/* get the CPU struct associated */
int	cpu_status(cpu_t *cp);
int	cpu_online(cpu_t *cp);
int	cpu_offline(cpu_t *cp);
int	cpu_poweron(cpu_t *cp);		/* take powered-off cpu to off-line */
int	cpu_poweroff(cpu_t *cp);	/* take off-line cpu to powered-off */

struct bind_arg {			/* args passed through dotoprocs */
	processorid_t		bind;
	processorid_t		obind;
	int			err;	/* non-zero error number if any */
};


int	cpu_bind_process(proc_t *pp, struct bind_arg *arg);
int	cpu_bind_thread(kthread_id_t tp, struct bind_arg *arg);

extern void affinity_set(int cpu_id);
extern void affinity_clear(void);

/*
 * The following routines affect the CPUs participation in interrupt processing,
 * if that is applicable on the architecture.  This only affects interrupts
 * which aren't directed at the processor (not cross calls).
 *
 * cpu_disable_intr returns non-zero if interrupts were previously enabled.
 */
int	cpu_disable_intr(struct cpu *cp); /* stop issuing interrupts to cpu */
void	cpu_enable_intr(struct cpu *cp); /* start issuing interrupts to cpu */

/*
 * The mutex cpu_lock protects cpu_flags for all CPUs, as well as the ncpus
 * and ncpus_online counts.
 */
extern kmutex_t	cpu_lock;	/* lock protecting CPU data */


#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif /* _SYS_CPUVAR_H */
