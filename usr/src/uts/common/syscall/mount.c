/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * 		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 * 		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * 	Copyright (c) 1986-1989,1993,1997,1998 by Sun Microsystems, Inc.
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *		All rights reserved.
 *
 */

#pragma ident	"@(#)mount.c	1.7	98/01/23 SMI"	/* SVr4 1.42	*/

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/user.h>
#include <sys/fstyp.h>
#include <sys/kmem.h>
#include <sys/systm.h>
#include <sys/mount.h>
#include <sys/vfs.h>
#include <sys/cred.h>
#include <sys/vnode.h>
#include <sys/dnlc.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/cmn_err.h>
#include <sys/swap.h>
#include <sys/debug.h>
#include <sys/pathname.h>

/*
 * System calls.
 */

/*
 * "struct mounta" defined in sys/vfs.h.
 */

/* ARGSUSED */
int
mount(char *spec, char *dir, int flags,
	char *fstype, char *dataptr, int datalen)
{
	vnode_t *vp = NULL;
	struct vfs *vfsp;	/* dummy argument */
	int error;
	struct mounta ua;

	ua.spec = spec;
	ua.dir = dir;
	ua.flags = flags;
	ua.fstype = fstype;
	ua.dataptr = dataptr;
	ua.datalen = datalen;

	/*
	 * Resolve second path name (mount point).
	 */
	if (error = lookupname(ua.dir, UIO_USERSPACE, FOLLOW, NULLVPP, &vp))
		return (set_errno(error));

	/*
	 * Some mount flags are disallowed through the system call interface.
	 */
	ua.flags &= MS_MASK;

	error = domount(NULL, &ua, vp, CRED(), &vfsp);

	VN_RELE(vp);
	return (error ? set_errno(error) : 0);
}
