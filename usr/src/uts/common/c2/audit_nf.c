/*
 * Copyright (c) 1986-1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * @(#)audit_nfs.c 2.11 92/01/20 SMI; SunOS CMW
 * @(#)audit_nfs.c 4.2.1.2 91/05/08 SMI; BSM Module
 */

#pragma ident	"@(#)audit_nfs.c	1.17	97/10/29 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/uio.h>
#include <sys/user.h>
#include <sys/file.h>
#include <sys/pathname.h>
/* #include <sys/au_membuf.h> */		/* for so_to_bl() */
#include <netinet/in.h>
#include <net/route.h>
#include <netinet/in_pcb.h>
#include <rpc/types.h>
#include <rpc/auth.h>
#include <rpc/auth_unix.h>
#include <rpc/auth_des.h>
#include <sys/t_kuser.h>
#include <sys/tiuser.h>
#include <rpc/svc.h>
#include <nfs/nfs.h>
#include <nfs/export.h>
#include <c2/audit.h>
#include <c2/audit_kernel.h>
#include <c2/audit_record.h>
#include <c2/audit_kevents.h>

extern int audit_policy;

#ifdef NFSSERVER
/*ARGSUSED*/
/* exportfs start function */
aus_exportfs(struct p_audit_data *pad)
{
	/* XXX64 au_to_data64() */
	klwp_id_t clwp = ttolwp(curthread);

	struct a {
		long	dname;		/* char * */
		long	uex;	/* struct export * */
	} *uap = (struct a *)clwp->lwp_ap;

	int error;
	STRUCT_DECL(export, kex);
	caddr_t ptr;
	int	num;
	int	i;

	if (uap->uex == 0) {
			/* freeing export entry */
		au_uwrite(au_to_arg(1, "freeing export entry", (uint) 0));
		return;
	}

	/*
	 * Load in everything, and do sanity checking
	 */
	error = copyin((caddr_t) uap->uex,
			STRUCT_BUF(kex),
			STRUCT_SIZE(kex));
	if (error) {
		goto error_return;
	}
	if (STRUCT_FGET(kex, ex_flags) & ~(EX_RDONLY | EX_RDMOSTLY)) {
		goto error_return;
	}
	if (STRUCT_FGET(kex, ex_flags) & EX_RDMOSTLY) {
		error = loadaddrs(STRUCT_FADDR(kex, ex_writeaddrs));
		if (error) {
			goto error_return;
		}
	}
	switch (STRUCT_FGET(kex, ex_auth)) {
	case AUTH_UNIX:
		error = loadaddrs(STRUCT_FADDR(kex, ex_unix.rootaddrs));
		break;
	case AUTH_DES:
		error = loadrootnames(kex);
		break;
	default:
		error = EINVAL;
	}
	if (error) {
		goto errors;
	}

		/* audit system call here */
	au_uwrite(au_to_arg(2, "ex_flags", (uint)STRUCT_FGET(kex, ex_flags)));
	au_uwrite(au_to_arg(3, "ex_anon", (uint)STRUCT_FGET(kex, ex_anon)));
	au_uwrite(au_to_arg(4, "ex_auth", (uint)STRUCT_FGET(kex, ex_auth)));
	switch (STRUCT_FGET(kex, ex_auth)) {
	case AUTH_UNIX:
		num = STRUCT_FGET(kex, ex_unix.rootaddrs.naddrs);
		au_uwrite(au_to_arg(5, "unix rootaddrs", (uint)num));
		ptr = STRUCT_FGETP(kex, ex_unix.rootaddrs.addrvec);
		for (i = 0; i < num; i++) {
			/* WRONG!!! we have to copy in before */
			au_uwrite(au_to_data(AUP_HEX, AUR_SHORT, 1, ptr));
			ptr += 2;
			au_uwrite(au_to_data(AUP_HEX, AUR_CHAR, 14, ptr));
			ptr += 14;
		}
		break;
	case AUTH_DES:
		num = STRUCT_FGET(kex, ex_des.nnames);
		au_uwrite(au_to_arg(5, "des rootnames", (uint)num));
		for (i = 0; i < num; i++) {
			ptr = STRUCT_FGETP(kex, ex_des.rootnames);
			/* WRONG!!! we have to copy in before */
			au_uwrite(au_to_text((uint)name[i]));
		}
		break;
	}
	if (STRUCT_FGET(kex, ex_flags) & EX_RDMOSTLY) {
		num = STRUCT_FGET(kex, ex_writeaddrs.naddrs);
		au_uwrite(au_to_arg(6, "ex_writeaddrs", (uint)num));
		ptr = STRUCT_FGET(kex, ex_writeaddrs.addrvec);
		for (i = 0; i < num; i++) {
			/* WRONG!!! we have to copy in before */
			au_uwrite(au_to_data(AUP_HEX, AUR_SHORT, 1, ptr));
			ptr += 2;
			au_uwrite(au_to_data(AUP_HEX, AUR_CHAR, 14, ptr));
			ptr += 14;
		}
	}

		/* free up resources */
	switch (STRUCT_FGET(kex, ex_auth)) {
	case AUTH_UNIX:
		mem_free((char *)STRUCT_FGETP(kex, ex_unix.rootaddrs.addrvec),
			(STRUCT_FGET(kex, ex_unix.rootaddrs.naddrs) *
			    sizeof (struct sockaddr)));
		break;
	case AUTH_DES:
		freenames(kex);
		break;
	}

errors:
	if (STRUCT_FGET(kex, ex_flags) & EX_RDMOSTLY) {
		mem_free((char *)STRUCT_FGET(kex, ex_writeaddrs.addrvec),
			STRUCT_FGET(kex, ex_writeaddrs.naddrs) *
					sizeof (struct sockaddr));
	}

error_return:
/*
	mem_free((char *) kex, sizeof (struct export));
*/
}
#endif	/* NFSSERVER */

#ifdef notdef
#ifdef NFSSERVER
aus_nfssvc(struct p_audit_data *pad)
{
	/* XXX64 au_to_data64() */
	klwp_id_t clwp = ttolwp(curthread);

	struct a {
		long	sock;
	} *uap = (struct a *)clwp->lwp_ap;
	struct file   *fp;
	struct socket *so;
	struct file   *getsock();
	int flag;

	fp = getsock(uap->sock);

	if (fp == NULL)
		return;

	so = (struct socket *) fp->f_data;

	if (flag = audit_success(pad, 0)) {
		au_uwrite(au_to_socket(so));
#ifdef	SunOS_CMW
		au_uwrite(au_to_slabel(&so->so_sec->sc_slabel));
#endif	/* SunOS_CMW */
			/* Add a process token */
		au_uwrite(au_to_subject(u.u_procp));

			/* Add an optional group token */
		if (audit_policy&AUDIT_GROUP)
			au_write(&(u_ad), au_to_groups(u.u_procp));
#ifdef	SunOS_CMW
			/* Add a sensitivity label for the process */
		au_write(&(u_ad), au_to_slabel(&u.u_slabel));
#endif	/* SunOS_CMW */
			/* Add a return token */
		au_uwrite(au_to_return(0, 0));

		AS_INC(as_generated, 1);
		AS_INC(as_kernel, 1);

			/* do flow control on queue high water mark */
		if (audit_sync_block())
			flag = 0;

			/* Add an optional sequence token */
		if ((audit_policy&AUDIT_SEQ) && flag)
			au_write(&(u_ad), au_to_seq());
	}

	if (u.u_error)
		pad->pad_evmod |= PAD_FAILURE;

		/* Close up everything && preselect */
	au_close(&(u_ad), flag, pad->pad_event, pad->pad_evmod);
		/* free up any space remaining with the path's */
	if (pad->pad_pathlen) {
		dprintf(8, ("aus_nfssvc: pad %p %x %p\n", pad,
			pad->pad_pathlen, pad->pad_path));
		call_debug(8);

		AS_DEC(as_memused, pad->pad_pathlen);
		kmem_free(pad->pad_path, pad->pad_pathlen);
		pad->pad_pathlen = 0;
		pad->pad_path	= (caddr_t) 0;
		pad->pad_vn	= (struct vnode *) 0;
	}
}

auf_nfssvc(struct p_audit_data *pad)
{

		/* verify that there is no outstanding audit record */
	if (pad->pad_ad != -1) {
		panic("auf_nfssvc: audit record active\n");
	}

		/* are we auditing this event */
	if (!pad->pad_flag)
		return;

		/* reopen audit descripter for exit of daemon */
	au_uopen();
	pad->pad_event = AUE_NFSSVC_EXIT;
	pad->pad_evmod = 0;
	pad->pad_ctrl  = 0;
}
#endif	/* NFSSERVER */
#endif notdef
