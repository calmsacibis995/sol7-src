/*
 * Copyright (c) 1987-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)wscons.c	1.5	97/10/22 SMI"

/*
 * "Workstation console" multiplexor driver for Sun.
 *
 * All the guts have been removed for the i86 version because the
 * console architecture is different.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/cred.h>
#include <sys/vnode.h>
#include <sys/stropts.h>
#include <sys/stream.h>
#include <sys/strredir.h>
#include <sys/stat.h>
#include <sys/errno.h>

#include <sys/conf.h>
#include <sys/modctl.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>

static dev_info_t *wc_dip;

extern vnode_t *rconsvp;
extern vnode_t *rwsconsvp;

/* ARGSUSED */
static int
wc_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	if (ddi_create_minor_node(devi, "wscons", S_IFCHR,
	    0, NULL, NULL) == DDI_FAILURE) {
		ddi_remove_minor_node(devi, NULL);
		return (-1);
	}
	wc_dip = devi;
	return (DDI_SUCCESS);
}

/* ARGSUSED */
static int
wc_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		*result = wc_dip;
		return (DDI_SUCCESS);
	case DDI_INFO_DEVT2INSTANCE:
		*result = 0;
		return (DDI_SUCCESS);
	}
	return (DDI_FAILURE);
}

/*
 * Auxiliary routines, for allowing the workstation console to be redirected.
 */

/*
 * Given a minor device number for a wscons instance, return a held vnode for
 * it.
 *
 * We currently support only one instance, for the "workstation console".
 */
int
wcvnget(int unit, vnode_t **vpp)
{
	if (unit != 0)
		return (ENXIO);

	/*
	 * rwsconsvp is already held, so we don't have to do it here.
	 */
	*vpp = rwsconsvp;
	return (0);
}

/*
 * Release the vnode that wcvnget returned.
 */
/*ARGSUSED*/
void
wcvnrele(int unit, vnode_t *vp)
{
	/*
	 * Nothing to do, since we only support the workstation console
	 * instance that's held throughout the system's lifetime.
	 */
}

/*
 * The declaration and initialization of the wscons_srvnops has been
 * moved to space.c to allow "wc" to become a loadable module.
 */

static struct cb_ops wc_cb_ops = {
	nodev,			/* open */
	nodev,			/* close */
	nodev,			/* strategy */
	nodev,			/* print */
	nodev,			/* dump */
	nodev,			/* read */
	nodev,			/* write */
	nodev,			/* ioctl */
	nodev,			/* devmap */
	nodev,			/* mmap */
	nodev, 			/* segmap */
	nochpoll,		/* poll */
	ddi_prop_op,		/* cb_prop_op */
	0,			/* streamtab  */
	D_NEW | D_MP,		/* Driver compatibility flag */

};

static struct dev_ops wc_ops = {
	DEVO_REV,		/* devo_rev, */
	0,			/* refcnt  */
	wc_info,		/* info */
	nulldev,		/* identify */
	nulldev,		/* probe */
	wc_attach,		/* attach */
	nodev,			/* detach */
	nodev,			/* reset */
	&wc_cb_ops,		/* driver operations */
	(struct bus_ops *)0	/* bus operations */
};

static struct modldrv modldrv = {
	&mod_driverops,
	"Workstation multiplexer Driver 'wc'",
	&wc_ops,
};

static struct modlinkage modlinkage = {
	MODREV_1,
	&modldrv,
	NULL
};

int
_init(void)
{
	return (mod_install(&modlinkage));
}

int
_fini(void)
{
	return (mod_remove(&modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}
