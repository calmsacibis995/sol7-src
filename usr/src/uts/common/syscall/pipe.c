/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma	ident	"@(#)pipe.c	1.24	97/04/08 SMI"	/* from SVr4.0 1.11 */

#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/cred.h>
#include <sys/user.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/stream.h>
#include <sys/strsubr.h>
#include <sys/errno.h>
#include <sys/debug.h>
#include <sys/fs/fifonode.h>

/*
 * This is the loadable module wrapper.
 */
#include <sys/modctl.h>
#include <sys/syscall.h>

char _depends_on[] = "fs/fifofs";

longlong_t pipe();

static struct sysent pipe_sysent = {
	0,
	SE_32RVAL1 | SE_32RVAL2 | SE_NOUNLOAD | SE_ARGC,
	(int (*)())pipe
};

/*
 * Module linkage information for the kernel.
 */
static struct modlsys modlsys = {
	&mod_syscallops, "pipe(2) syscall", &pipe_sysent
};

#ifdef _SYSCALL32_IMPL
static struct modlsys modlsys32 = {
	&mod_syscallops32, "32-bit pipe(2) syscall", &pipe_sysent
};
#endif

static struct modlinkage modlinkage = {
	MODREV_1,
	&modlsys,
#ifdef _SYSCALL32_IMPL
	&modlsys32,
#endif
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
	return (EBUSY);
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

/*
 * pipe(2) system call.
 * Create a pipe by connecting two streams together. Associate
 * each end of the pipe with a vnode, a file descriptor and
 * one of the streams.
 */
longlong_t
pipe()
{
	vnode_t *vp1, *vp2;
	struct file *fp1, *fp2;
	int error = 0;
	int fd1, fd2;
	rval_t	r;

	/*
	 * Allocate and initialize two vnodes.
	 */
	makepipe(&vp1, &vp2);

	/*
	 * Allocate and initialize two file table entries and two
	 * file pointers. Each file pointer is open for read and
	 * write.
	 */
	if (error = falloc(vp1, FWRITE|FREAD, &fp1, &fd1)) {
		VN_RELE(vp1);
		VN_RELE(vp2);
		return ((longlong_t)set_errno(error));
	}

	if (error = falloc(vp2, FWRITE|FREAD, &fp2, &fd2))
		goto out2;

	/*
	 * Create two stream heads and attach to each vnode.
	 */
	if (error = fifo_stropen(&vp1, FWRITE|FREAD, fp1->f_cred, 0, 0))
		goto out;

	if (error = fifo_stropen(&vp2, FWRITE|FREAD, fp2->f_cred, 0, 0)) {
		(void) VOP_CLOSE(vp1, FWRITE|FREAD, 0, (offset_t)0,
		    fp1->f_cred);
		goto out;
	}

	(void) str_mate(strvp2wq(vp1), strvp2wq(vp2));

	VTOF(vp1)->fn_ino = VTOF(vp2)->fn_ino = fifogetid();

	/*
	 * Now fill in the entries that falloc reserved
	 */
	mutex_exit(&fp1->f_tlock);
	mutex_exit(&fp2->f_tlock);
	setf(fd1, fp1);
	setf(fd2, fp2);

	/*
	 * Return the file descriptors to the user. They now
	 * point to two different vnodes which have different
	 * stream heads.
	 */
	r.r_val1 = fd1;
	r.r_val2 = fd2;
	return (r.r_vals);
out:
	unfalloc(fp2);
	setf(fd2, NULLFP);
out2:
	unfalloc(fp1);
	setf(fd1, NULLFP);
	VN_RELE(vp1);
	VN_RELE(vp2);
	return ((longlong_t)set_errno(error));
}
