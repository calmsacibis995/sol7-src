/*
 *	dh_common.c
 *
 *	Copyright (c) 1997, by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#pragma ident	"@(#)dh_common.c	1.1	97/11/19 SMI"

#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>

#include "dh_gssapi.h"
#include "dh_common.h"

#define	MECH_LIB_PREFIX1	"/usr/lib/"

/*
 * This #ifdef mess figures out if we are to be compiled into
 * a sparcv9/lp64 binary for the purposes of figuring the absolute location
 * of gss-api mechanism modules.
 */
#ifdef  _LP64

#ifdef __sparc

#define	MECH_LIB_PREFIX2	"sparcv9/"

#else   /* __sparc */

you need to define where under /usr the LP64 libraries live for this platform

#endif  /* __sparc */

#else   /* _LP64 */

#define	MECH_LIB_PREFIX2	""

#endif  /* _LP64 */

#define	MECH_LIB_DIR		"gss/"

#define	MECH_LIB_PREFIX MECH_LIB_PREFIX1 MECH_LIB_PREFIX2 MECH_LIB_DIR

#define	DH_MECH_BACKEND		"mech_dh.so.1"

#define	DH_MECH_BACKEND_PATH MECH_LIB_PREFIX DH_MECH_BACKEND

static char *DHLIB = DH_MECH_BACKEND_PATH;

#ifndef DH_MECH_SYM
#define	DH_MECH_SYM		"__dh_gss_initialize"
#endif

gss_mechanism
__dh_generic_initialize(gss_mechanism dhmech,
    gss_OID_desc mech_type, keyopts_t keyopts)
{
	gss_mechanism (*mech_init)(gss_mechanism mech);
	gss_mechanism mech;
	void *dlhandle;
	dh_context_t context;

	if ((dlhandle = dlopen(DHLIB, RTLD_NOW)) == NULL) {
		return (NULL);
	}

	mech_init = (gss_mechanism (*)(gss_mechanism))
		dlsym(dlhandle, DH_MECH_SYM);

	if (mech_init == NULL) {
		return (NULL);

	}

	if ((mech = mech_init(dhmech)) == NULL) {
		return (NULL);
	}

	mech->mech_type = mech_type;

	context = (dh_context_t)mech->context;
	context->keyopts = keyopts;
	context->mech = &mech->mech_type;

	return (mech);
}
