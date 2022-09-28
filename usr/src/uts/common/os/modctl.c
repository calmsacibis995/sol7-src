/*
 * Copyright (c) 1988-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)modctl.c	1.89	98/02/19 SMI"

/*
 * modctl system call for loadable module support.
 */

#include <sys/param.h>
#include <sys/user.h>
#include <sys/systm.h>
#include <sys/exec.h>
#include <sys/file.h>
#include <sys/conf.h>
#include <sys/time.h>
#include <sys/reboot.h>
#include <sys/fs/ufs_fsdir.h>
#include <sys/kmem.h>
#include <sys/sysconf.h>
#include <sys/cmn_err.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/bootconf.h>

#include <sys/modctl.h>
#include <sys/kobj.h>
#include <sys/devops.h>
#include <sys/autoconf.h>
#include <sys/hwconf.h>
#include <sys/callb.h>
#include <sys/debug.h>
#include <sys/cpuvar.h>
#include <sys/sysmacros.h>

static int mod_circdep(struct modctl *);
static int modinfo(modid_t, struct modinfo *);

static void mod_uninstall_all(void);
static int mod_getinfo(struct modctl *, struct modinfo *);
static struct modctl *allocate_modp(char *, char *);

static int mod_load(struct modctl *, int);
static int mod_unload(struct modctl *);
static int modinstall(struct modctl *);
static int moduninstall(struct modctl *);

static struct modctl *mod_hold_by_name(char *);
static struct modctl *mod_hold_by_id(modid_t);
static struct modctl *mod_hold_next_by_id(modid_t);
static struct modctl *mod_hold_loaded_mod(char *, int, int *);
static struct modctl *mod_hold_installed_mod(char *, int, int *);

static void mod_release(struct modctl *);
static void mod_make_dependent(struct modctl *, struct modctl *);
static int mod_install_requisites(struct modctl *);
static int mod_hold_dependents(struct modctl *, int);
static void mod_release_dependents(struct modctl *);
static void check_esc_sequences(char *, char *);

/*
 * module loading thread control structure
 */
struct loadmt {
	ksema_t		sema;
	char 		*subdir;	/* module subdir "e.g. fs, misc, drv" */
	char		*name;		/* name of module */
	int		rv;		/* return from modload_now */
};

static int modload_now(char *, char *);
static void modload_thread(struct loadmt *);
static struct loadmt *loadmt_alloc(char *, char *);
static void loadmt_free(struct loadmt *);

/*
 * The following modconf* variables are used in modctl_modconfig() to
 * ensure that only one thread at a time can do a MODCONFIG modctl()
 */
static char modconfig_busy = 0;
static kcondvar_t modconfig_cv;
static kmutex_t modconfig_lock;

kcondvar_t mod_cv;
kcondvar_t mod_uninstall_cv;	/* Communication between swapper and the */
				/* uninstall daemon. */
kmutex_t mod_lock;		/* protects mod structures */
kmutex_t mod_uninstall_lock;	/* protects mod_uninstall_cv */
kmutex_t instub_lock;

int mod_no_unload;		/* temp lock to prevent unloading */
int modrootloaded; 		/* set after root driver and fs are loaded */
int moddebug = 0x0;		/* debug flags for module writers */
int swaploaded;			/* set after swap driver and fs are loaded */
int last_module_id;
int configdebug;

#define	KOBJ_SPACE	(1024 * 1024)

size_t kobj_map_space_len = KOBJ_SPACE;

struct devnames *devnamesp;
struct devnames orphanlist, deletedlist;
krwlock_t	devinfo_tree_lock;

kcondvar_t hotplug_cv;
kmutex_t   hotplug_lk;

#define	MAJBINDFILE "/etc/name_to_major"
#define	SYSBINDFILE "/etc/name_to_sysnum"

static char majbind[] = MAJBINDFILE;
static char sysbind[] = SYSBINDFILE;

extern int obpdebug;
#define	DEBUGGER_PRESENT	((boothowto & RB_DEBUG) || (obpdebug != 0))

void
mod_setup(void)
{
	struct sysent *callp;
	int callnum, exectype, strmod;
	int	num_devs;
	int	i;

	/*
	 * Sync up with the work that
	 * the stand-alone linker has
	 * already done.
	 */
	(void) kobj_sync();

	/*
	 * Initialize the list of loaded driver dev_ops.
	 * XXX - This must be done before reading the system file so that
	 * forceloads of drivers will work.
	 */
	(void) mod_sysctl(SYS_SET_KVAR, NULL);

	num_devs = read_binding_file(majbind, mb_hashtab);
	devcnt = num_devs + 30;		/* Some space for expansion */
	devopsp = kmem_zalloc(devcnt * sizeof (struct dev_ops *), KM_NOSLEEP);

	for (i = 0; i < devcnt; i++)
		devopsp[i] = &mod_nodev_ops;

	init_devnamesp(devcnt);
	make_aliases(mb_hashtab);

	(void) read_binding_file(sysbind, sb_hashtab);
	init_syscallnames(NSYSCALL);

	/*
	 * Allocate loadable native system call locks.
	 */
	for (callnum = 0, callp = sysent; callnum < NSYSCALL;
	    callnum++, callp++) {
		if (LOADABLE_SYSCALL(callp)) {
			if (mod_getsysname(callnum) != NULL) {
				callp->sy_lock =
				    kobj_zalloc(sizeof (krwlock_t), KM_SLEEP);
				rw_init(callp->sy_lock, NULL, RW_DEFAULT, NULL);
			} else {
				callp->sy_flags &= ~SE_LOADABLE;
				callp->sy_callc = nosys;
			}
#ifdef DEBUG
		} else {
			/*
			 * Do some sanity checks on the sysent table
			 */
			switch (callp->sy_flags & SE_RVAL_MASK) {
			case SE_32RVAL1:
				/* only r_val1 returned */
			case SE_32RVAL1 | SE_32RVAL2:
				/* r_val1 and r_val2 returned */
			case SE_64RVAL:
				/* 64-bit rval returned */
				break;
			default:
				cmn_err(CE_WARN, "sysent[%d]: bad flags %x",
				    callnum, callp->sy_flags);
			}
#endif
		}
	}

#ifdef _SYSCALL32_IMPL
	/*
	 * Allocate loadable system call locks for 32-bit compat syscalls
	 *
	 * XX64	Should we just share the lock with the native syscall?
	 */
	for (callnum = 0, callp = sysent32; callnum < NSYSCALL;
	    callnum++, callp++) {
		if (LOADABLE_SYSCALL(callp)) {
			if (mod_getsysname(callnum) != NULL) {
				callp->sy_lock =
				    kobj_zalloc(sizeof (krwlock_t), KM_SLEEP);
				rw_init(callp->sy_lock, NULL, RW_DEFAULT, NULL);
			} else {
				callp->sy_flags &= ~SE_LOADABLE;
				callp->sy_callc = nosys;
			}
#ifdef DEBUG
		} else {
			/*
			 * Do some sanity checks on the sysent table
			 */
			switch (callp->sy_flags & SE_RVAL_MASK) {
			case SE_32RVAL1:
				/* only r_val1 returned */
			case SE_32RVAL1 | SE_32RVAL2:
				/* r_val1 and r_val2 returned */
			case SE_64RVAL:
				/* 64-bit rval returned */
				break;
			default:
				cmn_err(CE_WARN, "sysent32[%d]: bad flags %x",
				    callnum, callp->sy_flags);
				goto skip;
			}

			/*
			 * Cross-check the native and compatibility tables.
			 */
			if (callp->sy_callc == nosys ||
			    sysent[callnum].sy_callc == nosys)
				continue;
			/*
			 * If only one or the other slot is loadable, then
			 * there's an error -- they should match!
			 */
			if ((callp->sy_callc == loadable_syscall) ^
			    (sysent[callnum].sy_callc == loadable_syscall)) {
				cmn_err(CE_WARN, "sysent[%d] loadable?",
				    callnum);
			}
			/*
			 * This is more of a heuristic test -- if the
			 * system call returns two values in the 32-bit
			 * world, it should probably return two 32-bit
			 * values in the 64-bit world too.
			 */
			if (((callp->sy_flags & SE_32RVAL2) == 0) ^
			    ((sysent[callnum].sy_flags & SE_32RVAL2) == 0)) {
				cmn_err(CE_WARN, "sysent[%d] rval2 mismatch!",
				    callnum);
			}
skip:;
#endif	/* DEBUG */
		}
	}
#endif	/* _SYSCALL32_IMPL */

	/*
	 * Allocate loadable exec locks.  (Assumes all execs are loadable)
	 */
	for (exectype = 0; exectype < nexectype; exectype++) {
		execsw[exectype].exec_lock =
		    kobj_zalloc(sizeof (krwlock_t), KM_SLEEP);
		rw_init(execsw[exectype].exec_lock, NULL, RW_DEFAULT, NULL);
	}

	/*
	 * Initialize f_lock filed for staticly bound streams.
	 */
	for (strmod = 0; strmod < fmodcnt; strmod++) {
		if (fmodsw[strmod].f_name[0] != '\0')
			fmodsw[strmod].f_lock = STATIC_STREAM;
	}

	read_class_file();
}

static int modctl_modload(int use_path, char *filename, int *rvp);
static int modctl_modunload(modid_t id);
static int modctl_modinfo(modid_t, struct modinfo *modinfo);
static int modctl_modreserve(modid_t id, int *data);
static int modctl_modconfig(int *data);
static int modctl_add_major(int *data);
static int modctl_getmodpath(char *data);
static int modctl_read_sysbinding_file(void);
static int modctl_getmaj(char *name, u_int len, int *major);
static int modctl_getname(char *name, u_int len, int *major);
static int modctl_sizeof_devid(dev_t dev, u_int *len);
static int modctl_get_devid(dev_t dev, u_int len, ddi_devid_t udevid);
static int modctl_sizeof_minorname(dev_t dev, int spectype, u_int *len);
static int modctl_get_minorname(dev_t dev, int spectype, u_int len,
    char *uname);

static int
modctl_modload(int use_path, char *filename, int *rvp)
{
	struct modctl *modp;
	int retval = 0;
	char *filenamep;

	filenamep = kmem_zalloc(MOD_MAXPATH, KM_SLEEP);

	if (copyinstr(filename, filenamep, MOD_MAXPATH, 0)) {
		retval = EFAULT;
		goto out;
	}

	filenamep[MOD_MAXPATH - 1] = 0;
	modp = mod_hold_installed_mod(filenamep, use_path, &retval);

	if (modp == NULL)
		goto out;

	modp->mod_loadflags |= MOD_NOAUTOUNLOAD;
	*rvp = modp->mod_id;
	mod_release_mod(modp);
out:
	kmem_free(filenamep, MOD_MAXPATH);

	return (retval);
}

static int
modctl_modunload(modid_t id)
{
	if (id == 0) {
		mod_uninstall_all();
#ifdef CANRELOAD
		modreap();
#endif
		return (0);
	}

	return (modunload(id));
}

static int
modctl_modinfo(modid_t id, struct modinfo *umodi)
{
	int retval;
	struct modinfo modi;
#if defined(_SYSCALL32_IMPL)
	struct modinfo32 modi32;
#endif

	if (get_udatamodel() == DATAMODEL_NATIVE) {
		if (copyin(umodi, &modi, sizeof (struct modinfo)) != 0)
			return (EFAULT);
	}
#ifdef _SYSCALL32_IMPL
	else {
		bzero(&modi, sizeof (modi));
		if (copyin(umodi, &modi32, sizeof (struct modinfo32)) != 0)
			return (EFAULT);
		modi.mi_info = modi32.mi_info;
		modi.mi_id = modi32.mi_id;
		modi.mi_nextid = modi32.mi_nextid;
	}
#endif
	/*
	 * This flag is -only- for the kernels use.
	 */
	modi.mi_info &= ~MI_INFO_LINKAGE;

	retval = modinfo(id, &modi);
	if (retval)
		return (retval);

	if (get_udatamodel() == DATAMODEL_NATIVE) {
		if (copyout(&modi, umodi, sizeof (struct modinfo)) != 0)
			retval = EFAULT;
	}
#ifdef _SYSCALL32_IMPL
	else {
		int i;

		if ((uintptr_t)modi.mi_base > UINT32_MAX)
			return (EOVERFLOW);

		modi32.mi_info = modi.mi_info;
		modi32.mi_state = modi.mi_state;
		modi32.mi_id = modi.mi_id;
		modi32.mi_nextid = modi.mi_nextid;
		modi32.mi_base = (caddr32_t)modi.mi_base;
		modi32.mi_size = modi.mi_size;
		modi32.mi_rev = modi.mi_rev;
		modi32.mi_loadcnt = modi.mi_loadcnt;
		bcopy(modi.mi_name, modi32.mi_name, sizeof (modi32.mi_name));
		for (i = 0; i < MODMAXLINK32; i++) {
			modi32.mi_msinfo[i].msi_p0 = modi.mi_msinfo[i].msi_p0;
			bcopy(modi.mi_msinfo[i].msi_linkinfo,
			    modi32.mi_msinfo[i].msi_linkinfo,
			    sizeof (modi32.mi_msinfo[0].msi_linkinfo));
		}
		if (copyout(&modi32, umodi, sizeof (struct modinfo32)) != 0)
			retval = EFAULT;
	}
#endif

	return (retval);
}

/*
 * Return the last major number in the range of permissible major numbers.
 */
/*ARGSUSED*/
static int
modctl_modreserve(modid_t id, int *data)
{
	if (copyout(&devcnt, data, sizeof (devcnt)) != 0)
		return (EFAULT);
	return (0);
}

static int
modctl_modconfig(int *data)
{
	struct modconfig *mcp;
	major_t major;
	int error = 0;

	/*
	 * We only want to allow one thread at a time to do a MODCONFIG
	 * modctl() call.  All other threads will block but can return
	 * if a signal (such as ^C) is received.
	 */
	mutex_enter(&modconfig_lock);
	while (modconfig_busy) {
		if (cv_wait_sig(&modconfig_cv, &modconfig_lock) == 0) {
			mutex_exit(&modconfig_lock);
			return (EINTR);
		}
	}
	modconfig_busy = 1;
	mutex_exit(&modconfig_lock);

	mcp = kmem_zalloc(sizeof (struct modconfig), KM_SLEEP);

	if (get_udatamodel() == DATAMODEL_NATIVE) {
		if (copyin(data, mcp, sizeof (struct modconfig)) != 0)
			error = EFAULT;
	}
#ifdef _SYSCALL32_IMPL
	else {
		struct modconfig32 modc32;

		if (copyin(data, &modc32, sizeof (struct modconfig32)) != 0)
			error = EFAULT;
		else {
			bcopy(modc32.rootdir, &mcp->rootdir,
			    sizeof (modc32.rootdir));
			bcopy(modc32.drvname, &mcp->drvname,
			    sizeof (modc32.drvname));
			bcopy(modc32.drvclass, &mcp->drvclass,
			    sizeof (modc32.drvclass));
			mcp->major = modc32.major;
			mcp->num_aliases = modc32.num_aliases;
			mcp->ap = (struct aliases *)modc32.ap;
			mcp->debugflag = modc32.debugflag;
		}
	}
#endif

	if (error == 0) {
		if (mcp->drvname[0] != '\0') {
			if ((major = ddi_name_to_major(mcp->drvname)) != -1) {
				if (make_devname(mcp->drvname, major) == 0)
					error = make_one_node(major, mcp);
				else
					error = EINVAL;
			} else
				error = EINVAL;
		} else {
			make_all_nodes(mcp);
		}
	}

	kmem_free(mcp, sizeof (struct modconfig));

	mutex_enter(&modconfig_lock);
	modconfig_busy = 0;
	cv_signal(&modconfig_cv);
	mutex_exit(&modconfig_lock);

	return (error);
}

static int
modctl_add_major(int *data)
{
	struct modconfig mc;
	int i;
	struct aliases alias;
	struct aliases *ap;
	char name[256];
	char cname[256];
	char *drvname;

	bzero(&mc, sizeof (struct modconfig));
	if (get_udatamodel() == DATAMODEL_NATIVE) {
		if (copyin(data, &mc, sizeof (struct modconfig)) != 0)
			return (EFAULT);
	}
#ifdef _SYSCALL32_IMPL
	else {
		struct modconfig32 modc32;

		if (copyin(data, &modc32, sizeof (struct modconfig32)) != 0)
			return (EFAULT);
		else {
			bcopy(modc32.rootdir, mc.rootdir,
			    sizeof (modc32.rootdir));
			bcopy(modc32.drvname, mc.drvname,
			    sizeof (modc32.drvname));
			bcopy(modc32.drvclass, mc.drvclass,
			    sizeof (modc32.drvclass));
			mc.major = modc32.major;
			mc.num_aliases = modc32.num_aliases;
			mc.ap = (struct aliases *)modc32.ap;
			mc.debugflag = modc32.debugflag;
		}
	}
#endif

	if ((drvname = ddi_major_to_name(mc.major)) != NULL &&
	    strcmp(drvname, mc.drvname) != 0)
		return (EINVAL);
	ap = mc.ap;
	for (i = 0; i < mc.num_aliases; i++) {
		bzero(&alias, sizeof (struct aliases));

		if (get_udatamodel() == DATAMODEL_NATIVE) {
			if (copyin(ap, &alias, sizeof (struct aliases)) != 0)
				return (EFAULT);
			if (copyin(alias.a_name, name, alias.a_len) != 0)
				return (EFAULT);
		}
#ifdef _SYSCALL32_IMPL
		else {
			struct aliases32 al32;

			bzero(&al32, sizeof (struct aliases32));
			if (copyin(ap, &al32, sizeof (struct aliases32)) != 0)
				return (EFAULT);
			if (copyin((void *)al32.a_name, name, al32.a_len) != 0)
				return (EFAULT);
			alias.a_next = (struct aliases *)al32.a_next;
		}
#endif
		check_esc_sequences(name, cname);
		make_mbind(cname, mc.major, mb_hashtab, NULL);
		ap = alias.a_next;
	}
	if (mc.drvclass[0] != '\0')
		add_class(mc.drvname, mc.drvclass);
	make_mbind(mc.drvname, mc.major, mb_hashtab, NULL);
	return (make_devname(mc.drvname, mc.major));
}

static void
check_esc_sequences(char *str, char *cstr)
{
	int i;
	size_t len;
	char *p;

	len = strlen(str);
	for (i = 0; i < len; i++, str++, cstr++) {
		if (*str != '\\') {
			*cstr = *str;
		} else {
			p = str + 1;
			/*
			 * we only handle octal escape sequences for SPACE
			 */
			if (*p++ == '0' && *p++ == '4' && *p == '0') {
				*cstr = ' ';
				str += 3;
			} else {
				*cstr = *str;
			}
		}
	}
	*cstr = 0;
}

static int
modctl_getmodpath(char *data)
{
	if (copyout(default_path, data, strlen(default_path) + 1) != 0)
		return (EFAULT);
	return (0);
}

static int
modctl_read_sysbinding_file(void)
{
	(void) read_binding_file(sysbind, sb_hashtab);
	return (0);
}

static int
modctl_getmaj(char *uname, u_int ulen, int *umajorp)
{
	char name[256];
	int retval;
	major_t major;

	if ((retval = copyinstr(uname, name,
	    (ulen < 256) ? ulen : 256, 0)) != 0)
		return (retval);
	if ((major = ddi_name_to_major(name)) == -1)
		return (ENODEV);
	if (copyout(&major, umajorp, sizeof (major_t)) != 0)
		return (EFAULT);
	return (0);
}

static int
modctl_getname(char *uname, u_int ulen, int *umajorp)
{
	char *name;
	major_t major;

	if (copyin(umajorp, &major, sizeof (major)) != 0)
		return (EFAULT);
	if ((name = ddi_major_to_name(major)) == NULL)
		return (ENODEV);
	if ((strlen(name) + 1) > ulen)
		return (ENOSPC);
	return (copyoutstr(name, uname, ulen, NULL));
}

/*
 * Return the sizeof of the device id.
 * XX64 - int or size_t for len?
 */
static int
modctl_sizeof_devid(dev_t dev, u_int *len)
{
	u_int		sz;
	ddi_devid_t	devid;

	/* get device id */
	if (ddi_lyr_get_devid(dev, &devid) == DDI_FAILURE)
		return (EINVAL);

	sz = ddi_devid_sizeof(devid);
	ddi_devid_free(devid);

	/* copyout device id size */
	if (copyout(&sz, len, sizeof (sz)) != 0)
		return (EFAULT);

	return (0);
}

/*
 * Return a copy of the device id.
 * XX64: int or size_t for len?
 */
static int
modctl_get_devid(dev_t dev, u_int len, ddi_devid_t udevid)
{
	u_int		sz;
	ddi_devid_t	devid;
	int		err = 0;

	/* get device id */
	if (ddi_lyr_get_devid(dev, &devid) == DDI_FAILURE)
		return (EINVAL);

	sz = ddi_devid_sizeof(devid);

	/* Error if device id is larger than space allocated */
	if (sz > len) {
		ddi_devid_free(devid);
		return (ENOSPC);
	}

	/* copy out device id */
	if (copyout(devid, udevid, sz) != 0)
		err = EFAULT;
	ddi_devid_free(devid);
	return (err);
}

/*
 * Return the size of the minor name.
 * XX64: int or size_t for len?
 */
static int
modctl_sizeof_minorname(dev_t dev, int spectype, u_int *len)
{
	u_int	sz;
	char	*name;

	/* get the minor name */
	if (ddi_lyr_get_minor_name(dev, spectype, &name) == DDI_FAILURE)
		return (EINVAL);

	sz = strlen(name) + 1;
	kmem_free(name, sz);

	/* copy out the size of the minor name */
	if (copyout(&sz, len, sizeof (sz)) != 0)
		return (EFAULT);

	return (0);
}

/*
 * Return the minor name.
 * XX64: int or size_t for len?
 */
static int
modctl_get_minorname(dev_t dev, int spectype, u_int len, char *uname)
{
	u_int	sz;
	char	*name;
	int	err = 0;

	/* get the minor name */
	if (ddi_lyr_get_minor_name(dev, spectype, &name) == DDI_FAILURE)
		return (EINVAL);

	sz = strlen(name) + 1;

	/* Error if the minor name is larger than the space allocated */
	if (sz > len) {
		kmem_free(name, sz);
		return (ENOSPC);
	}

	/* copy out the minor name */
	if (copyout(name, uname, sz) != 0)
		err = EFAULT;
	kmem_free(name, sz);
	return (err);
}

/*ARGSUSED5*/
int
modctl(int cmd, uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4,
    uintptr_t a5)
{
	int error;
	int rv = 0;

	if (!suser(CRED()) && (cmd != MODINFO))
		return (set_errno(EPERM));

	switch (cmd) {
	case MODLOAD:		/* load a module */
		error = modctl_modload((int)a1, (char *)a2, &rv);
		break;

	case MODUNLOAD:		/* unload a module */
		error = modctl_modunload((modid_t)a1);
		break;

	case MODINFO:		/* get module status */
		error = modctl_modinfo((modid_t)a1, (struct modinfo *)a2);
		break;

	case MODRESERVED:	/* get last major number in range */
		error = modctl_modreserve((modid_t)a1, (int *)a2);
		break;

	case MODCONFIG:		/* build device tree */
		error = modctl_modconfig((int *)a2);
		break;

	case MODADDMAJBIND:	/* read major binding file */
		error = modctl_add_major((int *)a2);
		break;

	case MODGETPATH:	/* get modpath */
		error = modctl_getmodpath((char *)a2);
		break;

	case MODREADSYSBIND:	/* read system call binding file */
		error = modctl_read_sysbinding_file();
		break;

	case MODGETMAJBIND:	/* get major number for named device */
		error = modctl_getmaj((char *)a1, (u_int)a2, (int *)a3);
		break;

	case MODGETNAME:	/* get name of device given major number */
		error = modctl_getname((char *)a1, (u_int)a2, (int *)a3);
		break;

	case MODSIZEOF_DEVID:	/* sizeof device id of device given dev_t */
		error = modctl_sizeof_devid((dev_t)a1, (u_int *)a2);
		break;

	case MODGETDEVID:	/* get device id of device given dev_t */
		error = modctl_get_devid((dev_t)a1, (u_int)a2, (ddi_devid_t)a3);
		break;

	case MODSIZEOF_MINORNAME:	/* sizeof minor nm of dev_t/spectype */
		error = modctl_sizeof_minorname((dev_t)a1, (int)a2,
		    (u_int *)a3);
		break;

	case MODGETMINORNAME:	/* get minor name of dev_t and spec type */
		error = modctl_get_minorname((dev_t)a1, (int)a2, (u_int)a3,
		    (char *)a4);
		break;

	default:
		error = EINVAL;
		break;
	}
	return (error ? set_errno(error) : rv);
}

/*
 * This is the primary kernel interface to load a module.
 *
 * This version loads and installs the named module.
 * Handoff the task of module loading to a seperate thread with a
 * large stack if possible, since this code may recurse a few times.
 */
int
modload(char *subdir, char *filename)
{
	struct loadmt *ltp = loadmt_alloc(subdir, filename);
	int rv;

	if (curthread != &t0 && thread_create(NULL, DEFAULTSTKSZ * 2,
	    modload_thread, (caddr_t)ltp, 0, &p0, TS_RUN, MAXCLSYSPRI) != NULL)
		sema_p(&ltp->sema);
	else
		ltp->rv = modload_now(subdir, filename);
	rv = ltp->rv;
	loadmt_free(ltp);
	return (rv);
}

/*
 * Calls to modload() are handled off to this routine in a separate
 * thread.
 */
static void
modload_thread(struct loadmt *ltp)
{
	/*
	 * load the module
	 * save return code for the creator of this thread and signal
	 */
	kmutex_t	cpr_lk;
	callb_cpr_t	cpr_i;

	mutex_init(&cpr_lk, NULL, MUTEX_DEFAULT, NULL);
	CALLB_CPR_INIT(&cpr_i, &cpr_lk, callb_generic_cpr, "modload");
	ltp->rv = modload_now(ltp->subdir, ltp->name);
	sema_v(&ltp->sema);
	mutex_enter(&cpr_lk);
	CALLB_CPR_EXIT(&cpr_i);
	mutex_destroy(&cpr_lk);
	thread_exit();
}

/*
 * allocate and initialize a modload thread control structure
 */
static struct loadmt *
loadmt_alloc(char *subdir, char *name)
{
	struct loadmt *ltp = kmem_zalloc(sizeof (*ltp), KM_SLEEP);

	ASSERT(name != NULL);
	/*
	 * subdir may or may not be present
	 */
	if (subdir != NULL) {
		ltp->subdir = kmem_alloc(strlen(subdir) + 1, KM_SLEEP);
		bcopy(subdir, ltp->subdir, strlen(subdir) + 1);
	}

	ltp->name = kmem_alloc(strlen(name) + 1, KM_SLEEP);
	bcopy(name, ltp->name, strlen(name) + 1);

	sema_init(&ltp->sema, 0, NULL, SEMA_DEFAULT, NULL);
	return (ltp);
}

/*
 * free a modload thread control structure
 */
static void
loadmt_free(struct loadmt *ltp)
{
	sema_destroy(&ltp->sema);

	kmem_free(ltp->name, strlen(ltp->name) + 1);

	if (ltp->subdir != NULL)
		kmem_free(ltp->subdir, strlen(ltp->subdir) + 1);

	kmem_free(ltp, sizeof (*ltp));
}

/*
 * load and install the module now
 *
 * this used to be modload().
 */
static int
modload_now(char *subdir, char *filename)
{
	struct modctl *modp;
	size_t size;
	int id;
	char *fullname;
	int retval;

	if (subdir != NULL) {
		/*
		 * allocate enough space for <subdir>/<filename><NULL>
		 */
		size = strlen(subdir) + strlen(filename) + 2;
		fullname = kmem_zalloc(size, KM_SLEEP);
		(void) sprintf(fullname, "%s/%s", subdir, filename);
	} else {
		fullname = filename;
	}

	modp = mod_hold_installed_mod(fullname, 1, &retval);
	if (modp != NULL) {
		id = modp->mod_id;
		mod_release_mod(modp);
	}

	if (subdir != NULL)
		kmem_free(fullname, size);

	if (retval == 0) {
		CPU_STAT_ADDQ(CPU, cpu_sysinfo.modload, 1);
		return (id);
	}

	return (-1);
}

/*
 * Load a module.
 */
int
modloadonly(char *subdir, char *filename)
{
	struct modctl *modp;
	char *fullname;
	size_t size;
	int id, retval;

	if (subdir != NULL) {
		/*
		 * allocate enough space for <subdir>/<filename><NULL>
		 */
		size = strlen(subdir) + strlen(filename) + 2;
		fullname = kmem_zalloc(size, KM_SLEEP);
		(void) sprintf(fullname, "%s/%s", subdir, filename);
	} else {
		fullname = filename;
	}

	modp = mod_hold_loaded_mod(fullname, 1, &retval);
	if (modp) {
		id = modp->mod_id;
		mod_release_mod(modp);
	}

	if (subdir != NULL)
		kmem_free(fullname, size);

	if (retval == 0)
		return (id);
	return (-1);
}

/*
 * Uninstall and unload a module.
 */
int
modunload(modid_t id)
{
	struct modctl *modp;
	int retval;

	if ((modp = mod_hold_by_id((modid_t)id)) == NULL)
		return (EINVAL);

	if ((retval = moduninstall(modp)) == 0) {
		retval = mod_unload(modp);
		if (retval != 0) {
			cmn_err(CE_WARN, "%s uninstalled but not unloaded",
				modp->mod_filename);
		} else
			CPU_STAT_ADDQ(CPU, cpu_sysinfo.modunload, 1);
	}
out:
	mod_release_mod(modp);
	return (retval);
}

/*
 * Return status of a loaded module.
 */
static int
modinfo(modid_t id, struct modinfo *modinfop)
{
	struct modctl *modp;
	modid_t mid;

	mid = modinfop->mi_id;
	if (modinfop->mi_info & MI_INFO_ALL) {
		while ((modp = mod_hold_next_by_id(mid++)) != NULL) {
			if ((modinfop->mi_info & MI_INFO_CNT) ||
			    modp->mod_installed)
				break;
			mod_release_mod(modp);
		}
		if (modp == NULL)
			return (EINVAL);
	} else {
		modp = mod_hold_by_id(id);
		if (modp == NULL)
			return (EINVAL);
		if (!(modinfop->mi_info & MI_INFO_CNT) &&
		    !modp->mod_installed) {
			mod_release_mod(modp);
			return (EINVAL);
		}
	}

	modinfop->mi_state = 0;
	if (modp->mod_loaded) {
		modinfop->mi_state = MI_LOADED;
		kobj_getmodinfo(modp->mod_mp, modinfop);
	}
	if (modp->mod_installed) {
		modinfop->mi_state |= MI_INSTALLED;
		(void) mod_getinfo(modp, modinfop);
	}

	modinfop->mi_id = modp->mod_id;
	modinfop->mi_loadcnt = modp->mod_loadcnt;
	(void) strcpy(modinfop->mi_name, modp->mod_modname);

	mod_release_mod(modp);
	return (0);
}

static char mod_stub_err[] = "mod_hold_stub: Couldn't load stub module %s";
static char no_err[] = "No error function for weak stub %s";

/*
 * used by the stubs themselves to load and hold a module.
 * Returns 0 if the stub doesn't need to call mod_release_stub.
 *	   1 if the stub should call mod_release_stub.
 *	   -1 if the stub should just call the err_fcn.
 * Note that this code is stretched out so that we avoid subroutine calls
 * and optimize for the most likely case.  That is,the case where the
 * module is loaded and installed and not held.  In that case we just inc
 * the mod_stub flag and continue.
 */

int
mod_hold_stub(struct mod_stub_info *stub)
{
	struct modctl *mp;
	struct mod_modinfo *mip;

	mip = stub->mods_modinfo;

	mutex_enter(&mod_lock);
	/*
	 * This gross flag is to prevent stack overflow when using modprintf
	 * See comment in modprintf
	 */
	instubs++;
	/* we do mod_hold_by_modctl inline for speed */
mod_check_again:
	if ((mp = mip->mp) != NULL) {
		if (mp->mod_busy == 0) {
			if (mp->mod_installed) {
			/* no one home so grab the stub lock if installed */
				mp->mod_stub++;
				instubs--;
				mutex_exit(&mod_lock);
				return (0);
			} else {
				mp->mod_busy = 1;
				mp->mod_inprogress_thread =
				    (curthread == NULL ?
				    (kthread_id_t)-1 : curthread);
			}
		/* Busy, check to see if this thread holds the lock */
		} else if (mp->mod_inprogress_thread ==
		    (curthread == NULL ? (kthread_id_t)-1 : curthread)) {
			/* mod_circdep */
			if (!mp->mod_busy || !mp->mod_installed) {
				mutex_exit(&mod_lock);
				cmn_err(CE_PANIC,
				    "stub not busy or not installed");
			}
			/*
			 * At this point module is held by this thread
			 * so no need to call mod_release_stub.
			 */
			mp->mod_stub++;
			instubs--;
			mutex_exit(&mod_lock);
			return (0);
		} else {
			/* gotta wait */
			if (mod_hold_by_modctl(mp))
				goto mod_check_again;
			/*
			 * what we have now may have been unloaded!, in
			 * that case, mip->mp will be NULL, we'll hit this
			 * module and load again..
			 */
			cmn_err(CE_PANIC, "mod_hold_stub should have blocked");
		}
		mutex_exit(&mod_lock);
	} else {
		/* first time we've hit this module */
		mutex_exit(&mod_lock);
		mp = mod_hold_by_name(mip->modm_module_name);
		mip->mp = mp;
	}
	ASSERT(mp != NULL);
	/* In most cases, module will be installed */
	if (!mp->mod_installed) {
		/* Module not loaded, if weak stub don't load it */
		if (stub->mods_weak) {
			if (stub->mods_errfcn == NULL) {
				mod_release_mod(mp);
				cmn_err(CE_PANIC, no_err,
				    mip->modm_module_name);
			}
		} else {
			/* Not a weak stub so load the module */
			if (mod_load(mp, 1) != 0 || modinstall(mp) != 0) {
				mod_release_mod(mp);
				if (stub->mods_errfcn == NULL) {
					cmn_err(CE_PANIC, mod_stub_err,
					    mip->modm_module_name);
				} else {
					mutex_enter(&mod_lock);
					instubs--;
					mutex_exit(&mod_lock);
					return (-1);
				}
			}
		}
	}
	/* we're holding the busy bit so we can just inc the stub count */
	mp->mod_stub++;
	/* Module is held and loaded */
	mutex_enter(&mod_lock);
	instubs--;
	mutex_exit(&mod_lock);
	return (1);
}

void
mod_release_stub(struct mod_stub_info *stub, int hold_flag)
{
	struct modctl *mp;

	/* inline mod_release_mod */
	mp = stub->mods_modinfo->mp;
	mutex_enter(&mod_lock);
	if (!hold_flag) {
		mp->mod_stub--;
		if (mp->mod_want)
			cv_broadcast(&mod_cv);
		mutex_exit(&mod_lock);
		return;
	}
	ASSERT(mp->mod_busy);
	ASSERT(mp->mod_stub == 1);
	mp->mod_busy = 0;
	mp->mod_inprogress_thread = NULL;
	mp->mod_stub--;
	if (mp->mod_want)
		cv_broadcast(&mod_cv);
	mutex_exit(&mod_lock);
}

static struct modctl *
mod_hold_loaded_mod(char *filename, int usepath, int *status)
{
	struct modctl *modp;
	int retval;
	/*
	 * Hold the module.
	 */
	modp = mod_hold_by_name(filename);
	if (modp) {
		retval = mod_load(modp, usepath);
		if (retval != 0) {
			mod_release_mod(modp);
			modp = NULL;
		}
		*status = retval;
	} else {
		*status = ENOSPC;
	}
	return (modp);
}

static struct modctl *
mod_hold_installed_mod(char *name, int usepath, int *r)
{
	struct modctl *modp;
	int retval;

	/*
	 * Hold the module.
	 */
	modp = mod_hold_by_name(name);
	if (modp) {
		retval = mod_load(modp, usepath);
		if (retval != 0) {
			mod_release_mod(modp);
			modp = NULL;
			*r = retval;
		} else {
			if ((*r = modinstall(modp)) != 0) {
				/*
				 * We loaded it, but failed to _init() it.
				 * Be kind to developers -- force it
				 * out of memory now so that the next
				 * attempt to use the module will cause
				 * a reload.  See 1093793.
				 */
				(void) mod_unload(modp);
				mod_release_mod(modp);
				modp = NULL;
			}
		}
	} else {
		*r = ENOSPC;
	}
	return (modp);
}

static char mod_excl_err[] =
	"module %s(%s) is EXCLUDED and will not be loaded\n";
static char mod_init_err[] = "loadmodule:%s(%s): _init() error %d\n";

/*
 * This routine is needed for dependencies.  Users specify dependencies
 * by declaring a character array initialized to filenames of dependents.
 * So the code that handles dependents deals with filenames (and not
 * module names) because that's all it has.  We load by filename and once
 * we've loaded a file we can get the module name.
 * Unfortunately there isn't a single unified filename/modulename namespace.
 * C'est la vie.
 *
 * We allow the name being looked up to be prepended by an optional
 * subdirectory e.g. we can lookup (NULL, "fs/ufs") or ("fs", "ufs")
 */
struct modctl *
mod_find_by_filename(char *subdir, char *filename)
{
	struct modctl *modp;
	size_t sublen;

	mutex_enter(&mod_lock);
	/* ASSERT(MUTEX_HELD(&mod_lock));	XXX bug - not obeyed */
	if (subdir != NULL)
		sublen = strlen(subdir);
	else
		sublen = 0;

	for (modp = modules.mod_next; modp != &modules; modp = modp->mod_next)
		if (sublen) {
			char *mod_filename = modp->mod_filename;

			if (strncmp(subdir, mod_filename, sublen) == 0 &&
			    mod_filename[sublen] == '/' &&
			    strcmp(filename, &mod_filename[sublen + 1]) == 0)
				break;
		} else
			if (strcmp(filename, modp->mod_filename) == 0)
				break;

	mutex_exit(&mod_lock);
	if (modp == &modules)
		modp = NULL;
	return (modp);
}

/*
 * Check for circular dependencies.  This is called from do_dependents()
 * in kobj.c.  If we are the thread already loading this module, then
 * we're trying to load a dependent that we're already loading which
 * means the user specified circular dependencies.
 */
static int
mod_circdep(struct modctl *modp)
{
	kthread_id_t thread;

	thread = (curthread == NULL ? (kthread_id_t)-1 : curthread);
	return (modp->mod_inprogress_thread == thread);
}

static int
mod_getinfo(struct modctl *modp, struct modinfo *modinfop)
{
	int (*func)(struct modinfo *);
	int retval;

	ASSERT(modp->mod_busy);

	func = (int (*)(struct modinfo *))kobj_lookup(modp->mod_mp, "_info");

	if (kobj_addrcheck(modp->mod_mp, (caddr_t)func)) {
		modprintf("_info() not defined properly\n");
		/*
		 * The semantics of mod_info(9F) are that 0 is failure
		 * and non-zero is success.
		 */
		retval = 0;
	} else
		retval = (*func)(modinfop);  	/* call _info() function */

	if (moddebug & MODDEBUG_USERDEBUG)
		modprintf("Returned from _info, retval = %x\n", retval);

	return (retval);
}

static void
modadd(struct modctl *mp)
{
	ASSERT(MUTEX_HELD(&mod_lock));

	mp->mod_id = last_module_id++;
	mp->mod_next = &modules;
	mp->mod_prev = modules.mod_prev;
	modules.mod_prev->mod_next = mp;
	modules.mod_prev = mp;
}

/*ARGSUSED*/
static struct modctl *
allocate_modp(char *filename, char *modname)
{
	struct modctl *mp;

	mp = kobj_zalloc(sizeof (*mp), KM_SLEEP);
	mp->mod_modname = kobj_zalloc(strlen(modname) + 1, KM_SLEEP);
	(void) strcpy(mp->mod_modname, modname);
	return (mp);
}

#ifdef	notdef
static void
free_modp(struct modctl *modp)
{
	if (modp->mod_filename)
		kobj_free(modp->mod_filename, strlen(modp->mod_filename) + 1);
	kobj_free(modp, sizeof (*modp));
}
#endif	/* unused code */

/*
 * Get the value of a symbol.  This is a wrapper routine that
 * calls kobj_getsymvalue().  kobj_getsymvalue() may go away but this
 * wrapper will prevent callers from noticing.
 */
uintptr_t
modgetsymvalue(char *name, int kernelonly)
{
	return (kobj_getsymvalue(name, kernelonly));
}

/*
 * Get the symbol nearest an address.  This is a wrapper routine that
 * calls kobj_getsymname().  kobj_getsymname() may go away but this
 * wrapper will prevent callers from noticing.
 */
char *
modgetsymname(uintptr_t value, u_long *offset)
{
	return (kobj_getsymname(value, offset));
}

/*
 * Lookup a symbol in a specified module.  This is a wrapper routine that
 * calls kobj_lookup().  kobj_lookup() may go away but this
 * wrapper will prevent callers from noticing.
 */
uintptr_t
modlookup(char *modname, char *symname)
{
	struct modctl *modp;
	uintptr_t val;

	if ((modp = mod_hold_by_name(modname)) == NULL)
		return (0);
	val = kobj_lookup(modp->mod_mp, symname);
	mod_release_mod(modp);
	return (val);
}

/*
 * Ask the user for the name of the system file and the default path
 * for modules.
 */
#define	MAXINPUTLEN 64		/* this should go somewhere else */

void
mod_askparams()
{
	static char s0[MAXINPUTLEN];
	intptr_t fd;

	if ((fd = kobj_open(systemfile)) != -1L)
		kobj_close(fd);
	else
		systemfile = NULL;

	/*CONSTANTCONDITION*/
	while (1) {
		modprintf("Name of system file [%s]:  ",
			systemfile ? systemfile : "/dev/null");

		gets(s0);

		if (s0[0] == '\0')
			break;
		else if (strcmp(s0, "/dev/null") == 0) {
			systemfile = NULL;
			break;
		} else {
			if ((fd = kobj_open(s0)) != -1L) {
				kobj_close(fd);
				systemfile = s0;
				break;
			}
		}
		modprintf("can't find file %s\n", s0);
	}
}

static char loading_msg[] = "loading '%s' id %d\n";
static char load_msg[] = "load '%s' id %d loaded @ 0x%p/0x%p size %d/%d\n";

static int
mod_load(struct modctl *mp, int usepath)
{
	int status = 0;
	struct modinfo *modinfop = NULL;

	ASSERT(MUTEX_NOT_HELD(&mod_lock));
	ASSERT(mp->mod_busy);

	if (mp->mod_loaded)
		return (0);

	if (mod_sysctl(SYS_CHECK_EXCLUDE, mp->mod_modname) != 0 ||
	    mod_sysctl(SYS_CHECK_EXCLUDE, mp->mod_filename) != 0) {
		if (moddebug & MODDEBUG_LOADMSG) {
			modprintf(mod_excl_err, mp->mod_filename,
				mp->mod_modname);
		}
		return (ENXIO);
	}
	if (moddebug & MODDEBUG_LOADMSG2)
		modprintf(loading_msg, mp->mod_filename, mp->mod_id);

	kobj_load_module(mp, usepath);

	if (mp->mod_mp) {
		mp->mod_loaded = 1;
		mp->mod_loadcnt++;
		if (moddebug & MODDEBUG_LOADMSG) {
			modprintf(load_msg, mp->mod_filename, mp->mod_id,
				((struct module *)mp->mod_mp)->text,
				((struct module *)mp->mod_mp)->data,
				((struct module *)mp->mod_mp)->text_size,
				((struct module *)mp->mod_mp)->data_size);
		}
	} else {
		status = ENOENT;
		if (moddebug & MODDEBUG_ERRMSG) {
			modprintf("error loading '%s', program '%s'\n",
				mp->mod_filename,
				u.u_comm ? u.u_comm : "unix");
		}
	}
	if (status == 0) {
		/*
		 * XXX - There should be a better way to get this.
		 */
		modinfop = kmem_zalloc(sizeof (struct modinfo), KM_SLEEP);
		modinfop->mi_info = MI_INFO_LINKAGE;
		if (mod_getinfo(mp, modinfop) == 0)
			mp->mod_linkage = 0;
		else {
			mp->mod_linkage = (void *)modinfop->mi_base;
			ASSERT(mp->mod_linkage->ml_rev == MODREV_1);
		}
		kmem_free(modinfop, sizeof (struct modinfo));
		(void) mod_sysctl(SYS_SET_MVAR, (void *)mp);
		install_stubs_by_name(mp, mp->mod_modname);
	}
	return (status);
}

static char unload_msg[] = "unloading %s, module id %d, loadcnt %d.\n";

static int
mod_unload(struct modctl *mp)
{
	int status = EBUSY;

	ASSERT(MUTEX_NOT_HELD(&mod_lock));
	ASSERT(mp->mod_busy);

	if (mod_hold_dependents(mp, 0)) {
		if (!mp->mod_installed && mp->mod_loaded) {
			if (moddebug & MODDEBUG_LOADMSG)
				modprintf(unload_msg, mp->mod_modname,
					mp->mod_id, mp->mod_loadcnt);
			/* reset stub functions to call the binder again */
			reset_stubs(mp);
			kobj_unload_module(mp); /* free the memory */
			mp->mod_loaded = 0;
			mp->mod_linkage = NULL;
			status = 0;
		}
		if (!mp->mod_installed && !mp->mod_loaded)
			status = 0;
		mod_release_dependents(mp);
	}
	return (status);
}

static int
modinstall(struct modctl *mp)
{
	int val;
	int (*func)(void);

	ASSERT(MUTEX_NOT_HELD(&mod_lock));
	ASSERT(mp->mod_busy && mp->mod_loaded);

	if (mp->mod_installed)
		return (0);

	if (moddebug & MODDEBUG_LOADMSG)
		modprintf("installing %s, module id %d.\n",
			mp->mod_modname, mp->mod_id);

	ASSERT(mp->mod_mp != NULL);
	if (mod_install_requisites(mp) == -1) {
		(void) mod_unload(mp);
		return (ENXIO);
	}

	if (moddebug & MODDEBUG_ERRMSG) {
		modprintf("init '%s' id %d loaded @ 0x%p/0x%p size %d/%d\n",
			mp->mod_filename, mp->mod_id,
			((struct module *)mp->mod_mp)->text,
			((struct module *)mp->mod_mp)->data,
			((struct module *)mp->mod_mp)->text_size,
			((struct module *)mp->mod_mp)->data_size);
	}

	func = (int (*)())kobj_lookup(mp->mod_mp, "_init");

	if (kobj_addrcheck(mp->mod_mp, (caddr_t)func)) {
		modprintf("_init() not defined properly\n");
		return (EFAULT);
	}

	if (moddebug & MODDEBUG_USERDEBUG) {
		modprintf("breakpoint before calling _init()\n");
			if (DEBUGGER_PRESENT)
				debug_enter("_init");
	}

	val = (*func)();		/* call _init */

	if (moddebug & MODDEBUG_USERDEBUG)
		modprintf("Returned from _init, val = %x\n", val);

	if (val == 0)
		mp->mod_installed = 1;
	else if (moddebug & MODDEBUG_ERRMSG)
		modprintf(mod_init_err, mp->mod_filename, mp->mod_modname, val);

	return (val);
}

static char finidef[] = "_fini() not defined properly in %s\n";
static char finiret[] = "Returned from _fini for %s, status = %x\n";

static int
moduninstall(struct modctl *mp)
{
	int status = 0;
	int (*func)(void);

	ASSERT(MUTEX_NOT_HELD(&mod_lock));
	ASSERT(mp->mod_busy);

	if (!mp->mod_installed)
		return (0);

	ASSERT(mp->mod_loaded);

	if (!mod_hold_dependents(mp, 1)) {
		status = EBUSY;
	} else {
		/*
		 * mod_hold_dependents() may give up the busy flag
		 * so we need to check to see that the module is
		 * still installed.
		 */
		if (mp->mod_installed) {
			if (moddebug & MODDEBUG_LOADMSG2)
				modprintf("uninstalling %s\n", mp->mod_modname);

			func = (int (*)())kobj_lookup(mp->mod_mp, "_fini");
			if (func == NULL) {
				status = EBUSY;	/* can't be unloaded */
				goto out;
			}

			if (kobj_addrcheck(mp->mod_mp, (caddr_t)func)) {
				modprintf(finidef, mp->mod_filename);
				status = EFAULT;
				goto out;
			}

			status = (*func)();  	/* call _fini() */
			if (status == 0 && (moddebug & MODDEBUG_LOADMSG))
				modprintf("uninstalled %s\n", mp->mod_modname);

			if (moddebug & MODDEBUG_USERDEBUG)
				modprintf(finiret, mp->mod_filename, status);

			if (status == 0)
				mp->mod_installed = 0;
		}
out:
		mod_release_dependents(mp);
	}
	return (status);
}

/*
 * Uninstall all modules.
 */

static void
mod_uninstall_all(void)
{
	struct modctl *mp;
	modid_t modid = 0;
	int status;

	do {
		while ((mp = mod_hold_next_by_id(modid)) != NULL) {
			/*
			 * If we were called from the uninstall daemon
			 * and the MOD_NOAUTOUNLOAD flag
			 * is set, skip this module.
			 */
			if (mp->mod_loadflags & MOD_NOAUTOUNLOAD) {
				modid = mp->mod_id;
				mod_release_mod(mp);
			} else {
				break;
			}
		}

		/*
		 * mp points to a held module.
		 */
		if (mp) {
			status = moduninstall(mp);
#ifndef CANRELOAD
			if (status == 0)
				status = mod_unload(mp);
#endif
			mod_release_mod(mp);
			modid = mp->mod_id;
		} else {
			modid = 0;
		}
	} while (modid != 0);
}

static int modunload_disable_count;

void
modunload_disable(void)
{
	INCR_COUNT(&modunload_disable_count, &mod_uninstall_lock);
}

void
modunload_enable(void)
{
	DECR_COUNT(&modunload_disable_count, &mod_uninstall_lock);
}

kthread_id_t mod_aul_thread;

void
mod_uninstall_daemon(void)
{
	callb_cpr_t cprinfo;
	mod_aul_thread = curthread;

	CALLB_CPR_INIT(&cprinfo, &mod_uninstall_lock, callb_generic_cpr, "mud");
	for (;;) {
		mutex_enter(&mod_uninstall_lock);
		CALLB_CPR_SAFE_BEGIN(&cprinfo);
		cv_wait(&mod_uninstall_cv, &mod_uninstall_lock);
		/*
		 * The whole daemon is safe for CPR except we don't want
		 * the daemon to run if FREEZE is issued and this daemon
		 * wakes up from the cv_wait above. In this case, it'll be
		 * blocked in CALLB_CPR_SAFE_END until THAW is issued.
		 *
		 * The reason of calling CALLB_CPR_SAFE_BEGIN twice is that
		 * mod_uninstall_lock is used to protect cprinfo and
		 * CALLB_CPR_SAFE_BEGIN assumes that this lock is held when
		 * called.
		 */
		CALLB_CPR_SAFE_END(&cprinfo, &mod_uninstall_lock);
		CALLB_CPR_SAFE_BEGIN(&cprinfo);
		mutex_exit(&mod_uninstall_lock);
		if ((modunload_disable_count == 0) &&
		    ((moddebug & MODDEBUG_NOAUTOUNLOAD) == 0))
			mod_uninstall_all();
	}
}

/*
 * Unload all uninstalled modules.
 */

void
modreap(void)
{
#ifdef CANRELOAD
	struct modctl *mp;

	for (mp = modules.mod_next; mp != &modules; mp = mp->mod_next) {
		mutex_enter(&mod_lock);
		while (mod_hold_by_modctl(mp) == 1)
				continue;
		mutex_exit(&mod_lock);
		(void) mod_unload(mp);
		mod_release_mod(mp);
	}
#endif
	mutex_enter(&mod_uninstall_lock);
	cv_broadcast(&mod_uninstall_cv);
	mutex_exit(&mod_uninstall_lock);
}

/*
 * Hold the specified module.
 *
 * Return values:
 *	 0 ==> the module was held without "sleeping."
 *	 1 ==> the module was held and the current thread "slept."
 *
 * This is the module holding primitive.
 */
int
mod_hold_by_modctl(struct modctl *mp)
{
	ASSERT(MUTEX_HELD(&mod_lock));

	if (mp->mod_busy || mp->mod_stub) {
		mp->mod_want = 1;
		cv_wait(&mod_cv, &mod_lock);
		mp->mod_want = 0;
		/*
		 * Module may be unloaded by daemon.
		 * Nevertheless, modctl structure is still in linked list
		 * (i.e., off &modules), not freed!
		 * Caller is not supposed to assume "mp" is valid, but there
		 * is no reasonable way to detect this but using
		 * mp->mod_modinfo->mp == NULL check (follow the back pointer)
		 *   (or similar check depending on calling context)
		 * DON'T free modctl structure, it will be very problematic.
		 */
		return (1);	/* caller has to decide whether to re-try */
	}
	mp->mod_inprogress_thread =
		(curthread == NULL ? (kthread_id_t)-1 : curthread);
	mp->mod_busy = 1;
	return (0);
}

static struct modctl *
mod_hold_by_name(char *filename)
{
	char *modname;
	struct modctl *mp;
	char *curname, *newname;

	mutex_enter(&mod_lock);

	if ((modname = strrchr(filename, '/')) == NULL)
		modname = filename;
	else
		modname++;

	for (mp = modules.mod_next; mp != &modules; mp = mp->mod_next)
		if (strcmp(modname, mp->mod_modname) == 0)
			break;

	if (mp == &modules) { /* Not found */
		mp = allocate_modp(filename, modname);
		modadd(mp);
	}

	/*
	 * If the module was held, then it must be us who has it held.
	 */

	if (mod_circdep(mp))
		mp = NULL;
	else {
		while (mod_hold_by_modctl(mp) == 1)
			continue;

		/*
		 * If the name hadn't been set or has changed, allocate
		 * space and set it.  Free space used by previous name.
		 */
		curname = mp->mod_filename;
		if (curname == NULL || (curname != filename &&
		    modname != filename &&
		    strcmp(curname, filename) != 0)) {
			newname = kobj_zalloc(strlen(filename) + 1, KM_SLEEP);
			(void) strcpy(newname, filename);
			mp->mod_filename = newname;
			if (curname != NULL)
				kobj_free(curname, strlen(curname) + 1);
		}
	}

	mutex_exit(&mod_lock);
	if (mp && moddebug & MODDEBUG_LOADMSG2)
		modprintf("Holding %s\n", mp->mod_filename);
	if (mp == NULL && moddebug & MODDEBUG_LOADMSG2)
		modprintf("circular dependency loading %s\n", filename);
	return (mp);
}

static struct modctl *
mod_hold_by_id(modid_t modid)
{
	struct modctl *mp;

	mutex_enter(&mod_lock);

	for (mp = modules.mod_next; mp != &modules && mp->mod_id != modid;
	    mp = mp->mod_next)
		;

	if ((mp == &modules) || mod_circdep(mp))
		mp = NULL;
	else {
		while (mod_hold_by_modctl(mp) == 1)
			continue;
	}

	mutex_exit(&mod_lock);
	return (mp);
}

static struct modctl *
mod_hold_next_by_id(modid_t modid)
{
	struct modctl *mp;

	mutex_enter(&mod_lock);

	for (mp = modules.mod_next; mp != &modules && mp->mod_id <= modid;
	    mp = mp->mod_next)
		;

	if ((mp == &modules) || mod_circdep(mp))
		mp = NULL;
	else {
		while (mod_hold_by_modctl(mp) == 1)
			continue;
	}

	mutex_exit(&mod_lock);
	return (mp);
}

static void
mod_release(struct modctl *mp)
{
	ASSERT(MUTEX_HELD(&mod_lock) && mp->mod_busy);
	mp->mod_busy = 0;
	mp->mod_inprogress_thread = NULL;
	cv_broadcast(&mod_cv);
}

void
mod_release_mod(struct modctl *mp)
{
	if (moddebug & MODDEBUG_LOADMSG2)
		modprintf("Releasing %s\n", mp->mod_filename);
	mutex_enter(&mod_lock);
	mod_release(mp);
	mutex_exit(&mod_lock);
}

int
mod_remove_by_name(char *name)
{
	struct modctl *mp;
	int retval;

	mp = mod_hold_by_name(name);

	if (mp == NULL)
		return (EINVAL);

	if ((retval = moduninstall(mp)) == 0)
		retval = mod_unload(mp);

	mod_release_mod(mp);
	return (retval);
}

/*
 * Record that module "dep" is dependent on module "on_mod."
 */

static void
mod_make_dependent(struct modctl *dependent, struct modctl *on_mod)
{
	struct modctl_list *dep, *req;

	ASSERT(dependent->mod_busy && on_mod->mod_busy);

	mutex_enter(&mod_lock);
	for (dep = on_mod->mod_dependents; dep; dep = dep->modl_next)
		if (dep->modl_modp == dependent)
			break;

	if (dep == NULL) { /* Not recorded */
		dep = kobj_zalloc(sizeof (*dep), KM_SLEEP);
		dep->modl_modp = dependent;
		dep->modl_next = on_mod->mod_dependents;
		on_mod->mod_dependents = dep;
	}

	for (req = dependent->mod_requisites; req; req = req->modl_next)
		if (req->modl_modp == on_mod)
			break;

	if (req == NULL) { /* Not recorded */
		req = kobj_zalloc(sizeof (*req), KM_SLEEP);
		req->modl_modp = on_mod;
		req->modl_next = dependent->mod_requisites;
		dependent->mod_requisites = req;
	}
	mutex_exit(&mod_lock);
}

/*
 * Process dependency of the module represented by "dep" on the
 * module named by "on."
 *
 * Called from kobj_do_dependents() to load a module "on" on which
 * "dep" depends.
 */

struct modctl *
mod_load_requisite(struct modctl *dep, char *on)
{
	struct modctl *on_mod;
	int retval;

	if ((on_mod = mod_hold_loaded_mod(on, 1, &retval)) != NULL) {
		mod_make_dependent(dep, on_mod);
	} else if (moddebug & MODDEBUG_ERRMSG) {
		modprintf("error processing %s on which module %s depends\n",
			on, dep->mod_modname);
	}
	return (on_mod);
}

static int
mod_install_requisites(struct modctl *modp)
{
	struct modctl_list *modl;
	struct modctl *req;
	int status = 0;

	ASSERT(modp->mod_busy);
	for (modl = modp->mod_requisites; modl; modl = modl->modl_next) {
		req = modl->modl_modp;
		mutex_enter(&mod_lock);

		while (mod_hold_by_modctl(req) == 1)
			continue;

		mutex_exit(&mod_lock);
		status = modinstall(req);
		mod_release_mod(req);
		if (status != 0)
			break;
	}
	return (status);
}

static int
mod_hold_dependents(struct modctl *modp, int not_modstat)
{
	struct modctl_list *modl, *undo;
	struct modctl *dep;
	int stat = 1;
	int mod_stub;

	ASSERT(modp->mod_busy);

	mutex_enter(&mod_lock);
	/*
	 * We *must* give up our hold on "modp" here in order to
	 * avoid deadlock.  We avoid deadlock by always holding a
	 * series of modules in the same order.  We have arbitrarily
	 * chosen that order to be dependent module before requisite
	 * module.
	 */
	modp->mod_busy = 0;
	mod_stub = modp->mod_stub;
	modp->mod_stub = 0;
	modp->mod_inprogress_thread = NULL;
	modl = modp->mod_dependents;
	while (modl && stat) {
		dep = modl->modl_modp;
		if (mod_circdep(dep))
			cmn_err(CE_PANIC, "Deadlock can occur!");

		while (mod_hold_by_modctl(dep) == 1)
			continue;

		if (not_modstat == 0)
			stat = !dep->mod_loaded;
		else
			stat = !dep->mod_installed;
		modl = modl->modl_next;
	}

	if (!stat) {
		for (undo = modp->mod_dependents;
		    undo != modl;
		    undo = undo->modl_next) {
			mod_release(undo->modl_modp);
		}
	}

	while (mod_hold_by_modctl(modp) == 1)
		continue;

	modp->mod_stub = mod_stub;
	mutex_exit(&mod_lock);
	return (stat);
}

static void
mod_release_dependents(struct modctl *modp)
{
	struct modctl_list *modl;
	struct modctl *dep;

	ASSERT(modp->mod_busy);
	mutex_enter(&mod_lock);

	for (modl = modp->mod_dependents; modl; modl = modl->modl_next) {
		dep = modl->modl_modp;
		mod_release(dep);
	}
	mutex_exit(&mod_lock);
}
