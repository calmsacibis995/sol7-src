/*
 * Copyright (c) 1993-1997 by Sun Microsystems, Inc.
 * All rights reserved
 */

#pragma ident	"@(#)aclmode.c 1.5 97/08/04 SMI"
/*LINTLIBARY*/

/*
 * Convert ACL to/from permission bits
 */

#include <errno.h>
#include <sys/acl.h>

int
acltomode(aclent_t *aclbufp, int nentries, mode_t *modep)
{
	aclent_t		*tp;
	unsigned long		mode;
	unsigned long		grpmode;
	int			which;
	int			got_grpmode = 0;

	*modep = 0;
	if (aclcheck(aclbufp, nentries, &which) != 0) {
		errno = EINVAL;
		return (-1);	/* errno is set in aclcheck() */
	}
	for (tp = aclbufp; nentries--; tp++) {
		if (tp->a_type == USER_OBJ) {
			mode = tp->a_perm;
			if (mode > 07)
				return (-1);
			*modep |= (mode << 6);
			continue;
		}
		if (tp->a_type == GROUP_OBJ) {
			grpmode = tp->a_perm;
			if (grpmode > 07)
				return (-1);
			continue;
		}
		if (tp->a_type == CLASS_OBJ) {
			got_grpmode = 1;
			mode = tp->a_perm;
			if (mode > 07)
				return (-1);
			*modep |= (mode << 3);
			continue;
		}
		if (tp->a_type == OTHER_OBJ) {
			mode = tp->a_perm;
			if (mode > 07)
				return (-1);
			*modep |= mode;
			continue; /* we may break here if it is sorted */
		}
	}
	if (!got_grpmode)
		*modep |= (grpmode << 3);
	return (0);
}


int
aclfrommode(aclent_t *aclbufp, int nentries, mode_t *modep)
{
	aclent_t		*tp;
	aclent_t		*savp;
	mode_t 			mode;
	mode_t 			grpmode;
	int			which;
	int			got_grpmode = 0;

	if (aclcheck(aclbufp, nentries, &which) != 0) {
		errno = EINVAL;
		return (-1);	/* errno is set in aclcheck() */
	}
	for (tp = aclbufp; nentries--; tp++) {
		if (tp->a_type == USER_OBJ) {
			mode = (*modep & 0700);
			tp->a_perm = (mode >> 6);
			continue;
		}
		if (tp->a_type == GROUP_OBJ) {
			grpmode = (*modep & 070);
			savp = tp;
			continue;
		}
		if (tp->a_type == CLASS_OBJ) {
			got_grpmode = 1;
			mode = (*modep & 070);
			tp->a_perm = (mode >> 3);
			continue;
		}
		if (tp->a_type == OTHER_OBJ) {
			mode = (*modep & 07);
			tp->a_perm = (o_mode_t) mode;
			continue; /* we may break here if it is sorted */
		}
	}
	if (!got_grpmode)
		savp->a_perm = (grpmode >> 3);
	return (0);
}
