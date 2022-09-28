/*
 * Copyright (c) 1995-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ident	"@(#)rpcsec_gssmod.c	1.11	97/11/01 SMI"

#include <sys/types.h>
#include <sys/modctl.h>
#include <sys/errno.h>

char _depends_on[] = "strmod/rpcmod misc/kgssapi";

/*
 * Module linkage information for the kernel.
 */
extern struct mod_ops mod_miscops;

static struct modlmisc modlmisc = {
	&mod_miscops, "kernel RPCSEC_GSS security service."
};

static struct modlinkage modlinkage = {
	MODREV_1,
	(void *)&modlmisc,
	NULL
};

_init()
{
	int retval;
	extern void gssauth_init();
	extern void svc_gss_init();

	if ((retval = mod_install(&modlinkage)) != 0)
		return (retval);

	gssauth_init();
	svc_gss_init();

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
