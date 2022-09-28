/*
 * Copyright (c) 1991, 1993, 1996-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)profile.c	1.4	98/02/07 SMI"

/*
 * profile driver - initializes profile arrays for the kernel and
 * and loaded modules.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/file.h>
#include <sys/cmn_err.h>
#include <sys/t_lock.h>
#include <sys/proc.h>
#include <sys/fcntl.h>
#include <sys/uio.h>
#include <sys/kmem.h>
#include <sys/cred.h>
#include <sys/mman.h>
#include <sys/errno.h>
#include <sys/ioccom.h>
#include <sys/gprof.h>
#include <sys/cpuvar.h>
#include <sys/stat.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/modctl.h>
#include <sys/kobj.h>
#include <sys/systm.h>

extern int enable_profiling_interrupt(dev_info_t *, u_int (*)(void));
extern int disable_profiling_interrupt(dev_info_t *);

static int profile_identify(dev_info_t *);
static int profile_attach(dev_info_t *, ddi_attach_cmd_t);
static int profile_detach(dev_info_t *, ddi_detach_cmd_t);
static int profile_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg,
			void **result);
static int profile_open(dev_t *, int, int, struct cred *);
static int profile_close(dev_t, int, int, struct cred *);
static int profile_ioctl(dev_t, int, intptr_t, int, struct cred *, int *);
static int profile_init(void);
static void display_data(int);
static int profile_cleanup(void);
static int enable_mcount_tracing(void);
static int profile_ncpus(void);
static void unknown_routine(void);
static u_int profile_intr(void);

static dev_info_t *profile_devi;

static kmutex_t	profile_lock;

static struct cb_ops	profile_cb_ops = {
	profile_open,		/* open */
	profile_close,		/* close */
	nodev,			/* strategy */
	nodev,			/* print */
	nodev,			/* dump */
	nodev,			/* read */
	nodev,			/* write */
	profile_ioctl,		/* ioctl */
	nodev,			/* devmap */
	nodev,			/* mmap */
	nodev,			/* segmap */
	nochpoll,		/* poll */
	ddi_prop_op,		/* prop_op */
	0,			/* streamtab  */
	D_NEW | D_MP		/* Driver compatibility flag */
};

static struct dev_ops	profile_ops = {
	DEVO_REV,		/* devo_rev, */
	0,			/* refcnt  */
	profile_info,		/* get_dev_info */
	profile_identify,	/* identify */
	nulldev,		/* probe */
	profile_attach,		/* attach */
	profile_detach,		/* detach */
	nodev,			/* reset */
	&profile_cb_ops,	/* driver operations */
	(struct bus_ops *)0,	/* no bus operations */
	nulldev			/* power */
};

static struct modldrv modldrv = {
	&mod_driverops,
	"kernel profile driver",
	&profile_ops,
};

static struct modlinkage modlinkage = {
	MODREV_1,
	&modldrv,
	NULL
};

int
_init(void)
{
	int rc;

	mutex_init(&profile_lock, NULL, MUTEX_DEFAULT, NULL);
	if ((rc = mod_install(&modlinkage)) != 0)  {
		mutex_destroy(&profile_lock);
		return (rc);
	}
	return (0);
}

int
_fini(void)
{
	int rc;

	if ((rc = mod_remove(&modlinkage)) == 0) {
		(void) profile_cleanup();
		mutex_destroy(&profile_lock);
		return (0);
	}
	return (rc);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

static int
profile_identify(dev_info_t *devi)
{
	if (strcmp(ddi_get_name(devi), "profile") == 0) {
		return (DDI_IDENTIFIED);
	} else {
		return (DDI_NOT_IDENTIFIED);
	}
}

static int
profile_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	switch (cmd) {
	case DDI_ATTACH:
		if (enable_profiling_interrupt(devi, profile_intr) == -1)
			return (DDI_FAILURE);
		if (ddi_create_minor_node(devi, "profile", S_IFCHR, 0,
		    NULL, NULL) == DDI_FAILURE) {
			ddi_remove_minor_node(devi, NULL);
			(void) disable_profiling_interrupt(devi);
			return (DDI_FAILURE);
		}
		profile_devi = devi;
		return (DDI_SUCCESS);

	case DDI_RESUME:
		return (DDI_SUCCESS);
	}
	return (DDI_FAILURE);

}

static int
profile_detach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	int i;

	switch (cmd) {
	case DDI_DETACH:
		for (i = 0; i < NCPU; i++)
			if (VALID_CPU(i) && cpu[i]->cpu_profiling != NULL)
				return (DDI_FAILURE);
		ddi_remove_minor_node(devi, NULL);
		(void) disable_profiling_interrupt(devi);
		return (DDI_SUCCESS);

	case DDI_SUSPEND:
		return (DDI_SUCCESS);
	}
	return (DDI_FAILURE);

}

/* ARGSUSED */
static int
profile_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	int error;

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		*result = (void *) profile_devi;
		error = DDI_SUCCESS;
		break;
	case DDI_INFO_DEVT2INSTANCE:
		*result = (void *)0;
		error = DDI_SUCCESS;
		break;
	default:
		error = DDI_FAILURE;
	}
	return (error);
}

/* ARGSUSED */
static int
profile_open(dev_t *devp, int flag, int otyp, struct cred *cred)
{
#ifdef	PROFILE_DEBUG
	cmn_err(CE_NOTE, "profile_open: dev: %x\n", getminor(*devp));
#endif	/* PROFILE_DEBUG */

	return (0);
}

/* ARGSUSED */
static int
profile_close(dev_t dev, int flag, int otyp, struct cred *cred)
{

#ifdef	PROFILE_DEBUG
	cmn_err(CE_NOTE, "profile_close: dev: %x\n", getminor(dev));
#endif	/* PROFILE_DEBUG */

	return (0);
}

/* ARGSUSED */
static int
profile_ioctl(dev_t dev, int cmd, intptr_t data, int flag,
	cred_t *cred, int *rvalp)
{
	int rc = 0;

	switch (cmd) {

	case NCPUS_PROFILING:
		mutex_enter(&profile_lock);
		*rvalp = profile_ncpus();
		mutex_exit(&profile_lock);
		break;
	case INIT_PROFILING:
		mutex_enter(&profile_lock);
		rc = profile_init();
		mutex_exit(&profile_lock);
		break;
	case DEALLOC_PROFILING:
		mutex_enter(&profile_lock);
		rc = profile_cleanup();
		mutex_exit(&profile_lock);
		break;
	case MCOUNT_TRACING:
		mutex_enter(&profile_lock);
		rc = enable_mcount_tracing();
		mutex_exit(&profile_lock);
		break;
	default:
		/* invalid request */
		*rvalp = -1;
		return (EINVAL);
	}
	if (rc != 0)
		*rvalp = -1;
	return (rc);
}

static int
profile_init(void)
{
	int	i, ret;
	struct 	module 	*mp;
	struct 	modctl 	*modp;
	kernp_t	*p;
	size_t	s_hashsize, samplessize;
#ifdef GPROF
	size_t	fromssize, tossize;
#endif
	size_t	maskbit;
	uintptr_t kernel_lowpc, kernel_highpc;
	size_t	kernel_textsize;
	uintptr_t module_lowpc, module_highpc;
	size_t	module_real_textsize = 0;
	size_t	textsize;
	int 	debug = 0;

	for (i = 0; i < NCPU; i++) {
		if (! VALID_CPU(i))
			continue;
		p = cpu[i]->cpu_profiling;
		if (p != NULL) {
			return (EALREADY);
		}
	}

	kernel_highpc = (uintptr_t)e_text;
	kernel_lowpc = (uintptr_t)s_text;
	kernel_textsize = kernel_highpc - kernel_lowpc;
	cmn_err(CE_NOTE,
	    "Profiling kernel, textsize = %lu [%lx..%lx]",
	    kernel_textsize, kernel_lowpc, kernel_highpc);

	module_lowpc = (uintptr_t)-1;
	module_highpc = 0;
	modp = modules.mod_next;	/* skip kernel module */
	while (modp != &modules) {
		if ((mp = modp->mod_mp) != NULL) {
			if (debug) {
				cmn_err(CE_NOTE,
				    "module %s id %d",
				    modp->mod_modname, modp->mod_id);
				cmn_err(CE_NOTE,
				    "	base %p size %lu",
				    (void *)mp->text,
				    mp->text_size);
			}
			module_real_textsize += mp->text_size;
		}
		modp = modp->mod_next;
	}

	if (module_highpc != 0) {
		cmn_err(CE_NOTE,
		    "Profiling modules, size = %lu [%lx..%lx] (%lu used)",
		    module_real_textsize, module_lowpc, module_highpc,
		    module_real_textsize);
	}
	textsize = kernel_textsize + module_real_textsize;

	/* set s_hashsize to first power of two above desired size */
	s_hashsize = 2 * textsize / SAMPLE_HASH_RATIO;
	for (maskbit = 1; s_hashsize & ~maskbit; maskbit <<= 1) {
		s_hashsize &= ~maskbit;
	}
	s_hashsize = s_hashsize * sizeof (kp_sample_t *);

	samplessize = textsize / SAMPLE_RATIO;
	samplessize = samplessize * sizeof (kp_sample_t);

	cmn_err(CE_NOTE, "need %lu bytes per cpu for clock sampling",
	    (s_hashsize + samplessize));

#if defined(GPROF)
	/* set fromsize to first power of two above desired size */
	fromssize = 2 * textsize / CALL_HASH_RATIO;
	for (maskbit = 1; fromssize & ~maskbit; maskbit <<= 1) {
		fromssize &= ~maskbit;
	}
	fromssize = fromssize * sizeof (kp_call_t *);

	tossize = textsize / CALL_RATIO;
	tossize = tossize * sizeof (kp_call_t);

	cmn_err(CE_NOTE, "need %d bytes per cpu for call graph buffers",
		(fromssize + tossize));
#endif /* defined(GPROF) */

	for (i = 0; i < NCPU; i++) {
		if (! VALID_CPU(i))
			continue;
		p = cpu[i]->cpu_profiling;
		p = kmem_zalloc(sizeof (kernp_t), KM_NOSLEEP);
		if (p == NULL) {
			return (ENOMEM);
		}
		p->profiling = PROFILE_INIT;
		p->profiling_lock = 0;
		p->s_hash = (kp_sample_t **)
			kmem_zalloc((size_t)s_hashsize, KM_NOSLEEP);
		if (p->s_hash == NULL) {
			kmem_free(p, (sizeof (kernp_t)));
			return (ENOMEM);
		}
		p->s_hashsize = s_hashsize;
		p->samples = (kp_sample_t *)
			kmem_zalloc((size_t)samplessize, KM_NOSLEEP);
		if (p->samples == NULL) {
			kmem_free(p->s_hash, (size_t)s_hashsize);
			kmem_free(p, (sizeof (kernp_t)));
			return (ENOMEM);
		}
		p->samplessize = samplessize;
#if defined(GPROF)
		p->froms = (kp_call_t **)
			kmem_zalloc((size_t)fromssize, KM_NOSLEEP);
		if (p->froms == NULL) {
			kmem_free(p->samples, (size_t)samplessize);
			kmem_free(p->s_hash, (size_t)s_hashsize);
			kmem_free(p, (sizeof (kernp_t)));
			return (ENOMEM);
		}
		p->fromssize = fromssize;
		p->tos  = (kp_call_t *)
			kmem_zalloc((size_t)tossize, KM_NOSLEEP);
		if (p->tos == NULL) {
			kmem_free(p->froms, (size_t)fromssize);
			kmem_free(p->samples, (size_t)samplessize);
			kmem_free(p->s_hash, (size_t)s_hashsize);
			kmem_free(p, (sizeof (kernp_t)));
			return (ENOMEM);
		}
		p->tossize = tossize;
#else /* !defined(GPROF) */
		p->froms = NULL;
		p->fromssize = 0;
		p->tos = NULL;
		p->tossize = 0;
#endif
		p->samplesnext = p->samples;
		p->tosnext = p->tos;
		p->kernel_textsize = kernel_textsize;
		p->kernel_lowpc = kernel_lowpc;
		p->kernel_highpc = kernel_highpc;
		p->module_textsize = module_real_textsize;
		p->module_lowpc = module_lowpc;
		p->module_highpc = module_highpc;
		cpu[i]->cpu_profiling = p;
		if (debug) {
			display_data(i);
		}
		/*
		 * turn on the profiling timers for this
		 * cpu, enable_profiling is architecture
		 * dependent and lives in hardclk.c
		 */
		ret = enable_profiling(i);
		if (ret != 0) {
			return (ret);
		}
	}
	return (0);
}

static void
display_data(int cpu_index)
{
	kernp_t *p;

	p = cpu[cpu_index]->cpu_profiling;
	cmn_err(CE_NOTE, "kl = %lx\n", p->kernel_lowpc);
	cmn_err(CE_NOTE, "kh = %lx\n", p->kernel_highpc);
	cmn_err(CE_NOTE, "ksize = %lu\n", p->kernel_textsize);
	cmn_err(CE_NOTE, "ml = %lx\n", p->module_lowpc);
	cmn_err(CE_NOTE, "mh = %lx\n", p->module_highpc);
	cmn_err(CE_NOTE, "msize = %lu\n", p->module_textsize);
	cmn_err(CE_NOTE, "s_hash = %p, s_hashsize = %lu\n",
	    (void *)p->s_hash, p->s_hashsize);
	cmn_err(CE_NOTE, "samples = %p, samplessize = %lu, samplesnext = %p\n",
	    (void *)p->samples, p->samplessize, (void *)p->samplesnext);
	cmn_err(CE_NOTE, "froms = %p, fromssize = %lu\n",
	    (void *)p->froms, p->fromssize);
	cmn_err(CE_NOTE, "tos = %p, tossize = %lu, tosnext = %p\n",
	    (void *)p->tos, p->tossize, (void *)p->tosnext);
}

static int
profile_cleanup(void)
{
	int 	i;
	uintptr_t sysbase;
	kernp_t	*p;
	uintptr_t where;
	size_t	size;
	int 	debug = 0;

	for (i = 0; i < NCPU; i++) {
		if (! VALID_CPU(i))
			continue;
		p = cpu[i]->cpu_profiling;
		if (p == NULL) {
			return (ENOTINIT);
		}
		if (p->profiling == PROFILE_ON) {
			return (EPROFILING);
		}
	}

	sysbase = (uintptr_t)kernelheap;
	for (i = 0; i < NCPU; i++) {
		if (! VALID_CPU(i))
			continue;
		disable_profiling(i);
		p = cpu[i]->cpu_profiling;
		where = (uintptr_t)p->s_hash;
		size = p->s_hashsize;
		if (debug)
			cmn_err(CE_WARN,
			    "attempting to free s_hash at %p for %lu bytes",
			    (void *)where, size);
		if ((where > sysbase) && (size != 0)) {
			kmem_free((caddr_t)where, size);
		} else {
			cmn_err(CE_WARN, "didn't free s_hash addr %p "
			    "size %lu\n", (void *)where, size);
			if (debug) {
				display_data(i);
			}
		}
		where = (uintptr_t)p->samples;
		size = p->samplessize;
		if (debug)
			cmn_err(CE_WARN,
			    "attempting to free samples at %p for %lu bytes",
			    (void *)where, size);
		if ((where > sysbase) && (size != 0)) {
			kmem_free((caddr_t)where, (size_t)size);
		} else {
			cmn_err(CE_WARN,
			    "didn't free samples addr %p size %lu\n",
			    (void *)where, size);
			if (debug) {
				display_data(i);
			}
		}
#if defined(GPROF)
		where = (uintptr_t)p->froms;
		size = p->fromssize;
		if (debug)
			cmn_err(CE_WARN, "free froms at %x for %d bytes",
				where, size);
		if ((where > sysbase) && (size != 0)) {
			kmem_free((caddr_t)where, (size_t)size);
		} else {
			cmn_err(CE_WARN, "didn't free froms, addr %x size %d\n",
				where, size);
			if (debug) {
				display_data(i);
			}
		}
		where = (uintptr_t)p->tos;
		size = p->tossize;
		if (debug)
			cmn_err(CE_WARN,
				"attempting to free tos at %x for %d bytes",
				where, size);
		if ((where > sysbase) && (size != 0)) {
			kmem_free((caddr_t)where, (size_t)size);
		} else {
			cmn_err(CE_WARN, "didn't free tos, addr %x size %d\n",
				where, size);
			if (debug) {
				display_data(i);
			}
		}
#endif /* defined(GPROF) */
		kmem_free(p, (sizeof (kernp_t)));
		cpu[i]->cpu_profiling = NULL;
	}
	return (0);
}

static int
enable_mcount_tracing(void)
{
#ifdef TRACE
	int i;
	kernp_t	*p;

	for (i = 0; i < NCPU; i++) {
		if (! VALID_CPU(i))
			continue;
		p = cpu[i]->cpu_profiling;
		if (p == NULL) {
			p = kmem_zalloc(
			    sizeof (kernp_t), KM_NOSLEEP);
			if (p == NULL) {
				return (ENOMEM);
			}
			cmn_err(CE_NOTE,
			    "mcount tracing initialized for cpu %d", i);
		cpu[i]->cpu_profiling = p;
		} else {
			cmn_err(CE_NOTE, "mcount tracing re-initialized");
		}
	}
	return (0);
#else
	cmn_err(CE_NOTE, "This kernel was not compiled with tracing");
	return (ENOTTY);
#endif
}

static int
profile_ncpus(void)
{
	int i;
	int which;

	which = 0;
	for (i = 0; i < NCPU; i++) {
		if (VALID_CPU(i)) {
			which |= (1 << i);
		}
	}

	return (which);
}

static void
unknown_routine(void)
{
}

static u_int
profile_intr(void)
{
	struct cpu *cpup = CPU;
	kernp_t *p;
	void *rp;
	uintptr_t pc;
	size_t hashmask;
	kp_sample_t **link;
	kp_sample_t *node;

	if ((p = cpup->cpu_profiling) == NULL) {
		return (DDI_INTR_UNCLAIMED);	/* spurious? */
	}

	if ((rp = p->rp) == NULL) {
		return (DDI_INTR_UNCLAIMED);	/* not for us */
	}
	p->rp = NULL;

	clear_profiling_intr(cpup->cpu_id);

	if (p->profiling != PROFILE_ON) {
		return (DDI_INTR_CLAIMED);
	}

	/*
	 * We could collect user pc values but gprof wouldn't be able to do
	 * anything with them, so we simply toss them to save on buffer space.
	 */
	pc = get_profile_pc(rp);
	if (pc == 0) {
		return (DDI_INTR_CLAIMED);
	}

	if (!(p->kernel_lowpc <= pc && pc < p->kernel_highpc) &&
	    !(p->module_lowpc <= pc && pc < p->module_highpc)) {
		pc = (uintptr_t)unknown_routine;
	}

	/*
	 * Hash to some entry in s_hash.
	 *
	 * On many systems the low order bits of the pc are non-random, so we
	 * ignore them.
	 */
	hashmask = (p->s_hashsize / sizeof (kp_sample_t *)) - 1;
	link = &(p->s_hash[(pc >> 2) & hashmask]);

	while ((node = *link) != NULL) {
		if (pc == (uintptr_t)node->pc) {
			node->count++;
			return (DDI_INTR_CLAIMED);
		}
		link = &(node->link);
	}

	node = p->samplesnext++;
	if (node == (p->samples + (p->samplessize / sizeof (kp_sample_t)))) {
		printf("kprof: samples overflow\n");
		return (DDI_INTR_CLAIMED);
	}
	node->link = NULL;
	node->pc = pc;
	node->count = 1;

	*link = node;
	return (DDI_INTR_CLAIMED);
}
