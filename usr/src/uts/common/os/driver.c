/*
 * Copyright (c) 1992-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)driver.c	1.39	97/10/22 SMI"

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/conf.h>
#include <sys/systm.h>
#include <sys/sysmacros.h>
#include <sys/buf.h>
#include <sys/cred.h>
#include <sys/user.h>
#include <sys/uio.h>
#include <sys/map.h>
#include <sys/vnode.h>
#include <sys/fs/snode.h>
#include <sys/open.h>
#include <sys/debug.h>
#include <sys/tnf_probe.h>

/* Don't #include <sys/ddi.h> - it #undef's getmajor() */

#include <sys/sunddi.h>
#include <sys/esunddi.h>
#include <sys/autoconf.h>
#include <sys/epm.h>

/*
 * Configuration-related entry points for nexus and leaf drivers
 */
int
devi_identify(dev_info_t *devi)
{
	struct dev_ops *ops;
	int (*fn)(dev_info_t *);

	if ((ops = ddi_get_driver(devi)) == NULL ||
	    (fn = ops->devo_identify) == NULL)
		return (DDI_NOT_IDENTIFIED);

	return ((*fn)(devi));
}

int
devi_probe(dev_info_t *devi)
{
	struct dev_ops *ops;
	int (*fn)(dev_info_t *);

	ops = ddi_get_driver(devi);
	ASSERT(ops);

	/*
	 * probe(9E) in 2.0 implies that you can get
	 * away with not writing one of these .. so we
	 * pretend we're 'nulldev' if we don't find one (sigh).
	 */
	if ((fn = ops->devo_probe) == NULL)
		return (DDI_PROBE_DONTCARE);

	return ((*fn)(devi));
}

int
devi_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	struct dev_ops *ops;
	int error;
	int (*fn)(dev_info_t *, ddi_attach_cmd_t);

	if ((cmd == DDI_RESUME || cmd == DDI_PM_RESUME) &&
	    e_ddi_parental_suspend_resume(devi)) {
		return (e_ddi_resume(devi, cmd));
	}
	if ((ops = ddi_get_driver(devi)) == NULL ||
	    (fn = ops->devo_attach) == NULL)
		return (DDI_FAILURE);

	error = (*fn)(devi, cmd);

	if (cmd == DDI_ATTACH && error == DDI_SUCCESS) {
		error = e_pm_props(devi);
		if (error != DDI_SUCCESS)
			(void) devi_detach(devi, DDI_DETACH);
	}
	return (error);
}

int
devi_detach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	struct dev_ops *ops;
	int error;
	int (*fn)(dev_info_t *, ddi_detach_cmd_t);

	if ((cmd == DDI_SUSPEND || cmd == DDI_PM_SUSPEND) &&
	    e_ddi_parental_suspend_resume(devi)) {
		return (e_ddi_suspend(devi, cmd));
	}
	if ((ops = ddi_get_driver(devi)) == NULL ||
	    (fn = ops->devo_detach) == NULL)
		return (DDI_FAILURE);

	error = (*fn)(devi, cmd);

	if (cmd == DDI_DETACH && error == DDI_SUCCESS)
		pm_destroy_components(devi);
	return (error);
}

/*
 * This entry point not defined by Solaris 2.0 DDI/DKI, so
 * its inclusion here is somewhat moot.
 */
int
devi_reset(dev_info_t *devi, ddi_reset_cmd_t cmd)
{
	struct dev_ops *ops;
	int (*fn)(dev_info_t *, ddi_reset_cmd_t);

	if ((ops = ddi_get_driver(devi)) == NULL ||
	    (fn = ops->devo_reset) == NULL)
		return (DDI_FAILURE);

	return ((*fn)(devi, cmd));
}

/*
 * Leaf driver entry points
 */
int
dev_open(dev_t *devp, int flag, int type, struct cred *cred)
{
	struct cb_ops	*cb;

	cb = devopsp[getmajor(*devp)]->devo_cb_ops;
	return ((*cb->cb_open)(devp, flag, type, cred));
}

/*
 * The target driver is held (referenced) until we are
 * done. (See spec_close).
 */
int
dev_close(dev_t dev, int flag, int type, struct cred *cred)
{
	struct cb_ops	*cb;

	cb = (devopsp[getmajor(dev)])->devo_cb_ops;
	return ((*cb->cb_close)(dev, flag, type, cred));
}

/*
 * We only attempt to find the devinfo node if the driver is *already*
 * in memory and has attached.  If it isn't in memory, we don't load it,
 * - that's left to open(2).  If it is in memory, but not attached,
 * we don't attach it, we just fail.
 *
 * If it is attached, we return the dev_info node and increment the
 * devops so that it won't disappear while we're looking at it..
 */
dev_info_t *
dev_get_dev_info(dev_t dev, int otyp)
{
	struct dev_ops	*ops;
	dev_info_t	*dip;
	int error;
	major_t major = getmajor(dev);
	struct devnames *dnp;

#ifdef	lint
	otyp = otyp;
#endif	/* lint */

	if (major >= devcnt)
		return (NULL);

	dnp = &(devnamesp[major]);
	LOCK_DEV_OPS(&(dnp->dn_lock));
	ops = devopsp[major];
	if (ops == NULL || ops->devo_getinfo == NULL) {
		UNLOCK_DEV_OPS(&(dnp->dn_lock));
		return (NULL);
	}

	if ((!CB_DRV_INSTALLED(ops)) || (ops->devo_getinfo == NULL) ||
	    ((dnp->dn_flags & DN_DEVS_ATTACHED) == 0))  {
		UNLOCK_DEV_OPS(&(dnp->dn_lock));
		return (NULL);
	}

	INCR_DEV_OPS_REF(ops);
	UNLOCK_DEV_OPS(&(dnp->dn_lock));

	error = (*ops->devo_getinfo)(NULL, DDI_INFO_DEVT2DEVINFO,
	    (void *)dev, (void **)&dip);

	if (error != DDI_SUCCESS || dip == NULL) {
		LOCK_DEV_OPS(&(dnp->dn_lock));
		DECR_DEV_OPS_REF(ops);
		UNLOCK_DEV_OPS(&(dnp->dn_lock));
		return (NULL);
	}

	return (dip);	/* with one hold outstanding .. */
}

/*
 * The following function does not load the driver if it's not loaded.
 * Returns DDI_FAILURE or the instance number of the given dev_t as
 * interpreted by the device driver.
 *
 * instance is supposed to be a int but drivers have assumed that
 * the pointer was a pointer to "void *" instead of a pointer to
 * "int *" so we now explicitly pass a pointer to "void *" and then
 * cast the result to an int when returning the value.
 */
int
dev_to_instance(dev_t dev)
{
	struct dev_ops	*ops;
	void *instance;
	int error;
	major_t major = getmajor(dev);
	struct devnames *dnp = &(devnamesp[major]);

	if (major >= devcnt)
		return (DDI_FAILURE);

	LOCK_DEV_OPS(&(dnp->dn_lock));
	ops = devopsp[major];
	if (ops == NULL || ops->devo_getinfo == NULL) {
		UNLOCK_DEV_OPS(&(dnp->dn_lock));
		return (DDI_FAILURE);
	}

	if ((!CB_DRV_INSTALLED(ops)) || (ops->devo_getinfo == NULL) ||
	    ((dnp->dn_flags & DN_DEVS_ATTACHED) == 0))  {
		UNLOCK_DEV_OPS(&(dnp->dn_lock));
		return (DDI_FAILURE);
	}

	INCR_DEV_OPS_REF(ops);
	UNLOCK_DEV_OPS(&(dnp->dn_lock));

	error = (*ops->devo_getinfo)(NULL, DDI_INFO_DEVT2INSTANCE,
	    (void *)dev, &instance);

	LOCK_DEV_OPS(&(dnp->dn_lock));
	DECR_DEV_OPS_REF(ops);
	UNLOCK_DEV_OPS(&(dnp->dn_lock));

	if (error != DDI_SUCCESS)
		return (DDI_FAILURE);
	return ((int)instance);
}

/*
 * bdev_strategy should really be a 'void' function, since
 * the driver's strategy doesn't return anything meaningful
 * (it returns errors and such through the buf(9S) structure).
 * However, this breaks some unbundled products that look at
 * the return value (which they shouldn't).
 */
int
bdev_strategy(struct buf *bp)
{
	struct cb_ops	*cb;

	/* Kernel probe */
	TNF_PROBE_5(strategy, "io blockio", /* CSTYLED */,
		tnf_device,	device,		bp->b_edev,
		tnf_diskaddr,	block,		bp->b_lblkno,
		tnf_size,	size,		bp->b_bcount,
		tnf_opaque,	buf,		bp,
		tnf_bioflags,	flags,		bp->b_flags);

	cb = devopsp[getmajor(bp->b_edev)]->devo_cb_ops;
	(void) (*cb->cb_strategy)(bp);
	return (0);
}

int
bdev_print(dev_t dev, caddr_t str)
{
	struct cb_ops	*cb;

	cb = devopsp[getmajor(dev)]->devo_cb_ops;
	return ((*cb->cb_print)(dev, str));
}

int
bdev_size(dev_t dev)
{
	return (e_ddi_getprop(dev, VBLK, "nblocks",
	    DDI_PROP_NOTPROM | DDI_PROP_DONTPASS, -1));
}

/*
 * Same for 64-bit Nblocks property
 */
uint64_t
bdev_Size(dev_t dev)
{
	return (e_ddi_getprop_int64(dev, VBLK, "Nblocks",
	    DDI_PROP_NOTPROM | DDI_PROP_DONTPASS, -1));
}

int
bdev_dump(dev_t dev, caddr_t addr, daddr_t blkno, int blkcnt)
{
	struct cb_ops	*cb;

	cb = devopsp[getmajor(dev)]->devo_cb_ops;
	return ((*cb->cb_dump)(dev, addr, blkno, blkcnt));
}

int
cdev_read(dev_t dev, struct uio *uiop, struct cred *cred)
{
	struct cb_ops	*cb;

	cb = devopsp[getmajor(dev)]->devo_cb_ops;
	return ((*cb->cb_read)(dev, uiop, cred));
}

int
cdev_write(dev_t dev, struct uio *uiop, struct cred *cred)
{
	struct cb_ops	*cb;

	cb = devopsp[getmajor(dev)]->devo_cb_ops;
	return ((*cb->cb_write)(dev, uiop, cred));
}

int
cdev_ioctl(dev_t dev, int cmd, intptr_t arg, int mode, struct cred *cred,
    int *rvalp)
{
	struct cb_ops	*cb;

	cb = devopsp[getmajor(dev)]->devo_cb_ops;
	return ((*cb->cb_ioctl)(dev, cmd, arg, mode, cred, rvalp));
}

int
cdev_devmap(dev_t dev, devmap_cookie_t dhp, offset_t off, size_t len,
	size_t *maplen, uint_t mode)
{
	struct cb_ops	*cb;

	cb = devopsp[getmajor(dev)]->devo_cb_ops;
	return ((*cb->cb_devmap)(dev, dhp, off, len, maplen, mode));
}

int
cdev_mmap(int (*mapfunc)(dev_t, off_t, int), dev_t dev, off_t off, int prot)
{
	return ((*mapfunc)(dev, off, prot));
}

int
cdev_segmap(dev_t dev, off_t off, struct as *as, caddr_t *addrp, off_t len,
	    u_int prot, u_int maxprot, u_int flags, cred_t *credp)
{
	struct cb_ops	*cb;

	cb = devopsp[getmajor(dev)]->devo_cb_ops;
	return ((*cb->cb_segmap)(dev, off, as, addrp,
	    len, prot, maxprot, flags, credp));
}

int
cdev_poll(dev_t dev, short events, int anyyet, short *reventsp,
	struct pollhead **pollhdrp)
{
	struct cb_ops	*cb;

	cb = devopsp[getmajor(dev)]->devo_cb_ops;
	return ((*cb->cb_chpoll)(dev, events, anyyet, reventsp, pollhdrp));
}

/*
 * A 'size' property can be provided by a VCHR device.
 *
 * Since it's defined as zero for STREAMS devices, so we avoid the
 * overhead of looking it up.  Note also that we don't force an
 * unused driver into memory simply to ask about it's size.  We also
 * don't bother to ask it its size unless it's already been attached
 * (the attach routine is the earliest place the property'll be created)
 *
 * XXX	In an ideal world, we'd call this at VOP_GETATTR() time.
 */
int
cdev_size(dev_t dev)
{
	major_t maj;
	struct devnames *dnp;

	if ((maj = getmajor(dev)) >= devcnt)
		return (0);

	dnp = &(devnamesp[maj]);
	if ((dnp->dn_flags & DN_DEVS_ATTACHED) && devopsp[maj]) {
		LOCK_DEV_OPS(&dnp->dn_lock);
		if (devopsp[maj] && devopsp[maj]->devo_cb_ops &&
		    !STREAMSTAB(maj)) {
			UNLOCK_DEV_OPS(&dnp->dn_lock);
			return (e_ddi_getprop(dev, VCHR, "size",
			    DDI_PROP_NOTPROM | DDI_PROP_DONTPASS, 0));
		} else
			UNLOCK_DEV_OPS(&dnp->dn_lock);
	}
	return (0);
}

/*
 * same for 64-bit Size property
 */
uint64_t
cdev_Size(dev_t dev)
{
	major_t maj;
	struct devnames *dnp;

	if ((maj = getmajor(dev)) >= devcnt)
		return (0);

	dnp = &(devnamesp[maj]);
	if ((dnp->dn_flags & DN_DEVS_ATTACHED) && devopsp[maj]) {
		LOCK_DEV_OPS(&dnp->dn_lock);
		if (devopsp[maj] && devopsp[maj]->devo_cb_ops &&
		    !STREAMSTAB(maj)) {
			UNLOCK_DEV_OPS(&dnp->dn_lock);
			return (e_ddi_getprop_int64(dev, VCHR, "Size",
			    DDI_PROP_NOTPROM | DDI_PROP_DONTPASS, 0));
		} else
			UNLOCK_DEV_OPS(&dnp->dn_lock);
	}
	return (0);
}

/*
 * XXX	This routine is poorly named, because block devices can and do
 *	have properties (see bdev_size() above).
 *
 * XXX	fix the comment in devops.h that claims that cb_prop_op
 *	is character-only.
 */
int
cdev_prop_op(dev_t dev, dev_info_t *dip, ddi_prop_op_t prop_op, int mod_flags,
    char *name, caddr_t valuep, int *lengthp)
{
	struct cb_ops	*cb;

	if ((cb = devopsp[getmajor(dev)]->devo_cb_ops) == NULL)
		return (DDI_PROP_NOT_FOUND);

	return ((*cb->cb_prop_op)(dev, dip, prop_op, mod_flags,
	    name, valuep, lengthp));
}
