#ident  "@(#)gssdmod.c 1.4     97/09/26 SMI"

/*
 * Copyright (c) 1996-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#include <sys/types.h>
#include <sys/modctl.h>
#include <sys/errno.h>
#include <gssapi/kgssapi_defs.h>

char _depends_on[] = "strmod/rpcmod misc/rpcsec misc/tlimod";

/*
 * Module linkage information for the kernel.
 */
extern struct mod_ops mod_miscops;

static struct modlmisc modlmisc = {
	&mod_miscops, "in-kernel GSSAPI"
};

static struct modlinkage modlinkage = {
	MODREV_1,
	(void *)&modlmisc,
	NULL
};

_init()
{
	int retval;

	if ((retval = mod_install(&modlinkage)) != 0)
		return (retval);

	mutex_init(&gssrpcb_lock, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&__kgss_mech_lock, NULL, MUTEX_DEFAULT, NULL);

	return (0);
}

_fini()
{
	return (EBUSY);
}

_info(modinfop)
	struct modinfo *modinfop;
{
	return (mod_info(&modlinkage, modinfop));
}
