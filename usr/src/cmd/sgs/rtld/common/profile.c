/*
 *	Copyright (c) 1997 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)profile.c	1.22	97/05/22 SMI"

/*
 * Routines to provide profiling of ld.so itself, or any shared libraries
 * required by the called executable.
 */

#ifdef	PRF_RTLD

#include	"_synonyms.h"
#include	"_rtld.h"
#include	"profile.h"
#include	"msg.h"

uintptr_t (*	p_cg_interp)(int, caddr_t, caddr_t);

static int (*	p_open)(const char *, Link_map *);

int
profile_setup(Link_map * lmp)
{
	Rt_map *	_lmp;
	Dl_obj *	_handle;

	if ((_handle = (Dl_obj *)dlmopen_core((Lm_list *)LM_ID_LDSO,
	    MSG_ORIG(MSG_FIL_LDPROF), RTLD_LAZY | RTLD_GROUP | RTLD_WORLD |
	    RTLD_NOW,
	    &_lmp, lml_rtld.lm_head, LML_FLG_NOAUDIT)) == (Dl_obj *)0)
		return (0);

	if ((p_open = (int (*)(const char *, Link_map *))
	    dlsym_core(_handle, MSG_ORIG(MSG_SYM_PROFOPEN),
	    lml_rtld.lm_head)) == 0)
		return (0);

	if ((p_cg_interp = (uintptr_t(*)(int, caddr_t, caddr_t))
	    dlsym_core(_handle, MSG_ORIG(MSG_SYM_PROFCGINTRP),
	    lml_rtld.lm_head)) == 0)
		return (0);

	return (p_open(MSG_ORIG(MSG_FIL_RTLD), lmp));
}

#endif
