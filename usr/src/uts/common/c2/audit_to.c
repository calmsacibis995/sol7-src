/*
 * Copyright (c) 1991-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * @(#)audit_token.c 2.12 92/02/29 SMI; SunOS CMW
 * @(#)audit_token.c 4.2.1.2 91/05/08 SMI; BSM Module
 *
 * Support routines for building audit records.
 */

#pragma ident	"@(#)audit_token.c	1.40	97/11/13 SMI"

#include <sys/param.h>
#include <sys/systm.h>		/* for rval */
#include <sys/time.h>
#include <sys/types.h>
#include <sys/vnode.h>
#include <sys/mode.h>
#include <sys/user.h>
#include <sys/session.h>
#include <sys/acl.h>
#include <sys/ipc.h>
#include <sys/cmn_err.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <net/route.h>
#include <netinet/in_pcb.h>
#include <c2/audit.h>
#include <c2/audit_kernel.h>
#include <c2/audit_record.h>

extern kmutex_t  au_seq_lock;
/*
 * These are the control tokens
 */

/*
 * au_to_header
 * returns:
 *	pointer to au_membuf chain containing a header token.
 */
token_t *
au_to_header(int byte_count, short e_type, short e_mod)
{
	adr_t adr;			/* adr memory stream header */
	token_t *m;			/* au_membuf pointer */
#ifdef _LP64
	char data_header = AUT_HEADER64;	/* header for this token */
#else
	char data_header = AUT_HEADER32;
#endif
	char version = TOKEN_VERSION;	/* version of token family */

	m = au_getclr(au_wait);

	adr_start(&adr, memtod(m, char *));
	adr_char(&adr, &data_header, 1);	/* token ID */
	adr_int32(&adr, (int32_t *)&byte_count, 1);	/* length of */
							/* audit record */
	adr_char(&adr, &version, 1);		/* version of audit tokens */
	adr_short(&adr, &e_type, 1);		/* event ID */
	adr_short(&adr, &e_mod, 1);		/* event ID modifier */
#ifdef _LP64
	adr_int64(&adr, (int64_t *)&hrestime, 2);	/* time & date */
#else
	adr_int32(&adr, (int32_t *)&hrestime, 2);
#endif
	m->len = adr_count(&adr);

	return (m);
}

/*
 * au_to_trailer
 * returns:
 *	pointer to au_membuf chain containing a trailer token.
 */
token_t *
au_to_trailer(int byte_count)
{
	adr_t adr;				/* adr memory stream header */
	token_t *m;				/* au_membuf pointer */
	char data_header = AUT_TRAILER;		/* header for this token */
	short magic = (short)AUT_TRAILER_MAGIC; /* trailer magic number */

	m = au_getclr(au_wait);

	adr_start(&adr, memtod(m, char *));
	adr_char(&adr, &data_header, 1);	/* token ID */
	adr_short(&adr, &magic, 1);		/* magic number */
	adr_int32(&adr, (int32_t *)&byte_count, 1);	/* length of */
							/* audit record */

	m->len = adr_count(&adr);

	return (m);
}
/*
 * These are the data tokens
 */

/*
 * au_to_data
 * returns:
 *	pointer to au_membuf chain containing a data token.
 */
token_t *
au_to_data(char unit_print, char unit_type, char unit_count, char *p)
{
	adr_t adr;			/* adr memory stream header */
	token_t *m;			/* au_membuf pointer */
	char data_header = AUT_DATA;	/* header for this token */

	if (p == (char *)0)
		return (au_to_text("au_to_data: NULL pointer"));
	if (unit_count == 0)
		return (au_to_text("au_to_data: Zero unit count"));

	switch (unit_type) {
	case AUR_SHORT:
		if (sizeof (short) * unit_count >= AU_BUFSIZE)
			return (au_to_text("au_to_data: unit count too big"));
		break;
	case AUR_INT32:
		if (sizeof (int32_t) * unit_count >= AU_BUFSIZE)
			return (au_to_text("au_to_data: unit count too big"));
		break;
	case AUR_INT64:
		if (sizeof (int64_t) * unit_count >= AU_BUFSIZE)
			return (au_to_text("au_to_data: unit count too big"));
		break;
	case AUR_BYTE:
	default:
		if (sizeof (char) * unit_count >= AU_BUFSIZE)
			return (au_to_text("au_to_data: unit count too big"));
		break;
	}

	m = au_getclr(au_wait);

	adr_start(&adr, memtod(m, char *));
	adr_char(&adr, &data_header, 1);
	adr_char(&adr, &unit_print, 1);
	adr_char(&adr, &unit_type, 1);
	adr_char(&adr, &unit_count, 1);

	switch (unit_type) {
	case AUR_SHORT:
		adr_short(&adr, (short *)p, unit_count);
		break;
	case AUR_INT32:
		adr_int32(&adr, (int32_t *)p, unit_count);
		break;
	case AUR_INT64:
		adr_int64(&adr, (int64_t *)p, unit_count);
		break;
	case AUR_BYTE:
	default:
		adr_char(&adr, p, unit_count);
		break;
	}

	m->len = adr_count(&adr);

	return (m);
}

/*
 * au_to_process
 * au_to_subject
 * returns:
 *	pointer to au_membuf chain containing a process token.
 */
static token_t *au_to_any_process(char, struct proc *);

token_t *
au_to_process(struct proc *pp)
{
#ifdef _LP64
	return (au_to_any_process(AUT_PROCESS64, pp));
#else
	return (au_to_any_process(AUT_PROCESS32, pp));
#endif
}

token_t *
au_to_subject(struct proc *pp)
{
#ifdef _LP64
	return (au_to_any_process(AUT_SUBJECT64, pp));
#else
	return (au_to_any_process(AUT_SUBJECT32, pp));
#endif
}

#ifdef	AU_MAY_USE_SOMEDAY
token_t *
au_to_server(struct proc *pp)
{
#ifdef _LP64
	return (au_to_any_process(AUT_SERVER64, pp));
#else
	return (au_to_any_process(AUT_SERVER32, pp));
#endif
}

#endif	AU_MAY_USE_SOMEDAY

/* should use credential from thread!!! */
static token_t *
au_to_any_process(char data_header, struct proc *pp)
{
	token_t *m;	/* local au_membuf */
	adr_t adr;	/* adr memory stream header */
	struct p_audit_data *pad = (struct p_audit_data *)P2A(pp);
	int temp = pad->pad_auid;
	int32_t value;

	m = au_getclr(au_wait);

	adr_start(&adr, memtod(m, char *));
	adr_char(&adr, &data_header, 1);
	adr_int32(&adr, &(temp), 1);
	value = (int32_t)CRED()->cr_uid;
	adr_int32(&adr, &value, 1);
	value = (int32_t)CRED()->cr_gid;
	adr_int32(&adr, &value, 1);
	value = (int32_t)CRED()->cr_ruid;
	adr_int32(&adr, &value, 1);
	value = (int32_t)CRED()->cr_rgid;
	adr_int32(&adr, &value, 1);
	value = (int32_t)pp->p_pid;
	adr_int32(&adr, &value, 1);
	value = (int32_t)pad->pad_asid;
	adr_int32(&adr, &value, 1);
#ifdef _LP64
	adr_int64(&adr, (int64_t *)&(pad->pad_termid.port), 1);
#else
	adr_int32(&adr, (int32_t *)&(pad->pad_termid.port), 1);
#endif
	adr_char(&adr, (char *)&(pad->pad_termid.machine), 4);

	m->len = adr_count(&adr);

	return (m);
}

/*
 * au_to_text
 * returns:
 *	pointer to au_membuf chain containing a text token.
 */
token_t *
au_to_text(char *text)
{
	token_t *token;			/* local au_membuf */
	token_t *m;			/* local au_membuf */
	adr_t adr;			/* adr memory stream header */
	char data_header = AUT_TEXT;	/* header for this token */
	short bytes;			/* length of string */

	token = au_getclr(au_wait);

	bytes = (short)strlen(text) + 1;
	adr_start(&adr, memtod(token, char *));
	adr_char(&adr, &data_header, 1);
	adr_short(&adr, &bytes, 1);

	token->len = (char)adr_count(&adr);
	/*
	 * Now really get the path
	 */
	for (; bytes > 0; bytes -= AU_BUFSIZE, text += AU_BUFSIZE) {
		m = au_getclr(au_wait);
		(void) au_append_buf(text,
			bytes > AU_BUFSIZE ? AU_BUFSIZE : bytes, m);
		(void) au_append_rec((token_t *)token, (token_t *)m);
	}

	return (token);
}

/*
 * au_to_exec_strings
 * returns:
 *	pointer to au_membuf chain containing a argv/arge token.
 */
token_t *
au_to_exec_strings(char header, char **argvp, uintptr_t start, int count)
{
	token_t *token;			/* local au_membuf */
	token_t *m;			/* local au_membuf */
	adr_t adr;			/* adr memory stream header */

	token = au_getclr(au_wait);

	adr_start(&adr, memtod(token, char *));
	adr_char(&adr, &header, 1);
	adr_int32(&adr, (int32_t *)&count, 1);

	token->len = (char)adr_count(&adr);
	/*
	 * Now really get the path
	 */
	for (; count > 0; count--, argvp++) {
		m = au_getclr(au_wait);
		(void) au_append_buf(*argvp + start,
			(uint)strlen(*argvp + start) + 1, m);
		(void) au_append_rec((token_t *)token, (token_t *)m);
	}

	return (token);
}

/*
 * au_to_exec_args
 * returns:
 *	pointer to au_membuf chain containing a argv token.
 */
token_t *
au_to_exec_args(char **argvp, uintptr_t start, int count)
{
	char data_header = AUT_EXEC_ARGS;	/* header for this token */

	return (au_to_exec_strings(data_header, argvp, start, count));
}

/*
 * au_to_exec_env
 * returns:
 *	pointer to au_membuf chain containing a arge token.
 */
token_t *
au_to_exec_env(char **argvp, uintptr_t start, int count)
{
	char data_header = AUT_EXEC_ENV;	/* header for this token */

	return (au_to_exec_strings(data_header, argvp, start, count));
}

/*
 * au_to_arg32
 *	char   n;	argument # being used
 *	char  *text;	optional text describing argument
 *	uint32_t v;		argument value
 * returns:
 *	pointer to au_membuf chain containing an argument token.
 */
token_t *
au_to_arg32(char n, char *text, uint32_t v)
{
	token_t *token;			/* local au_membuf */
	token_t *m;			/* local au_membuf */
	adr_t adr;			/* adr memory stream header */
	char data_header = AUT_ARG32;	/* header for this token */
	short bytes;			/* length of string */

	token = au_getclr(au_wait);

	bytes = strlen(text) + 1;
	adr_start(&adr, memtod(token, char *));
	adr_char(&adr, &data_header, 1);	/* token type */
	adr_char(&adr, &n, 1);			/* argument id */
	adr_int32(&adr, (int32_t *)&v, 1);	/* argument value */
	adr_short(&adr, &bytes, 1);

	token->len = adr_count(&adr);
	/*
	 * Now really get the path
	 */
	for (; bytes > 0; bytes -= AU_BUFSIZE, text += AU_BUFSIZE) {
		m = au_getclr(au_wait);
		(void) au_append_buf(text,
			bytes > AU_BUFSIZE ? AU_BUFSIZE : bytes, m);
		(void) au_append_rec((token_t *)token, (token_t *)m);
	}

	return (token);
}


/*
 * au_to_arg64
 *	char		n;		argument # being used
 *	char		*text;		optional text describing argument
 *	uint64_t	v;		argument value
 * returns:
 *	pointer to au_membuf chain containing an argument token.
 */
token_t *
au_to_arg64(char n, char *text, uint64_t v)
{
	token_t *token;			/* local au_membuf */
	token_t *m;			/* local au_membuf */
	adr_t adr;			/* adr memory stream header */
	char data_header = AUT_ARG64;	/* header for this token */
	short bytes;			/* length of string */

	token = au_getclr(au_wait);

	bytes = strlen(text) + 1;
	adr_start(&adr, memtod(token, char *));
	adr_char(&adr, &data_header, 1);	/* token type */
	adr_char(&adr, &n, 1);			/* argument id */
	adr_int64(&adr, (int64_t *)&v, 1);	/* argument value */
	adr_short(&adr, &bytes, 1);

	token->len = adr_count(&adr);
	/*
	 * Now really get the path
	 */
	for (; bytes > 0; bytes -= AU_BUFSIZE, text += AU_BUFSIZE) {
		m = au_getclr(au_wait);
		(void) au_append_buf(text,
			bytes > AU_BUFSIZE ? AU_BUFSIZE : bytes, m);
		(void) au_append_rec((token_t *)token, (token_t *)m);
	}

	return (token);
}


/*
 * au_to_path
 * returns:
 *	pointer to au_membuf chain containing a path token.
 */
token_t *
au_to_path(char *path, u_int pathlen)
{
	token_t *token;			/* local au_membuf */
	token_t *m;			/* local au_membuf */
	adr_t adr;			/* adr memory stream header */
	char data_header = AUT_PATH;	/* header for this token */
	short bytes;			/* length of string */
	struct p_audit_data *pad;	/* per process audit data */

	dprintf(0x1000, ("au_to_path(%p,%x)\n", (void *)path, pathlen));
	call_debug(0x1000);
	/*
	 * We simulate behaviour of adr_string so that we don't need
	 * any form of intermediate buffers.
	 */

	pad = (struct p_audit_data *)P2A(curproc);
		/* DEBUG sanity checks */
	if (pad == (struct p_audit_data *)0)
		panic("au_to_path: no process audit structure");
	if (pad->pad_cwrd == (struct cwrd *)0)
		panic("au_to_path: no process cwrd structure");

	bytes = (short)pathlen;
	m = au_getclr(au_wait);

	adr_start(&adr, memtod(m, char *));
	adr_char(&adr, &data_header, 1);
	adr_short(&adr, &bytes, 1);

	m->len = adr_count(&adr);

		/* OPTIMIZATION - use clusters if path long enough */
	for (token = m; bytes > 0; bytes -= AU_BUFSIZE, path += AU_BUFSIZE) {
		m = au_getclr(au_wait);
		(void) au_append_buf(path,
			bytes > AU_BUFSIZE ? AU_BUFSIZE : bytes, m);
		token = au_append_token(token, m);
	}

	dprintf(0x1000, ("au_to_path: token: %p\n", (void *)token));
	call_debug(0x1000);

	return (token);
}

#ifdef REMOVE
/*
 * au_to_path_chain
 * returns:
 *	pointer to au_membuf chain containing a path token.
 */
token_t *
au_to_path_chain(token_t *path)
{
	token_t *token;			/* local au_membuf */
	token_t *m;			/* local au_membuf */
	adr_t adr;			/* adr memory stream header */
	char data_header = AUT_PATH;	/* header for this token */
	short bytes;			/* length of string */

	/*
	 * We simulate behaviour of adr_string so that we don't need
	 * any form of intermediate buffers.
	 */

	bytes = au_token_size(u.u_cwd->cw_root) + 1;
	m = au_getclr(au_wait);

	adr_start(&adr, memtod(m, char *));
	adr_char(&adr, &data_header, 1);
	adr_short(&adr, &bytes, 1);

	m->len = adr_count(&adr);

	token = au_append_token(m, au_gather(u.u_cwd->cw_root));

	bytes = au_token_size(u.u_cwd->cw_dir) + 1;
	m = au_getclr(au_wait);

	adr_start(&adr, memtod(m, char *));
	adr_short(&adr, &bytes, 1);

	m->len = adr_count(&adr);

	token = au_append_token(token, m);
	token = au_append_token(token, au_gather(u.u_cwd->cw_dir));

	bytes = au_token_size(path) + 1;
	m = au_getclr(au_wait);

	adr_start(&adr, memtod(m, char *));
	adr_short(&adr, &bytes, 1);

	m->len = adr_count(&adr);

	token = au_append_token(token, m);
	token = au_append_token(token, au_gather(path));

	return (token);
}
#endif

/*
 * au_to_ipc
 * returns:
 *	pointer to au_membuf chain containing a System V IPC token.
 */
token_t *
au_to_ipc(char type, int id)
{
	token_t *m;			/* local au_membuf */
	adr_t adr;			/* adr memory stream header */
	char data_header = AUT_IPC;	/* header for this token */

	m = au_getclr(au_wait);

	adr_start(&adr, memtod(m, char *));
	adr_char(&adr, &data_header, 1);
	adr_char(&adr, &type, 1);		/* type of IPC object */
	adr_int32(&adr, (int32_t *)&id, 1);

	m->len = adr_count(&adr);

	return (m);
}

/*
 * au_to_return32
 * returns:
 *	pointer to au_membuf chain containing a return value token.
 */
token_t *
au_to_return32(int error, int32_t rv)
{
	token_t *m;			/* local au_membuf */
	adr_t adr;			/* adr memory stream header */
	char data_header = AUT_RETURN32; /* header for this token */
	int32_t val;
	char ed = error;

	m = au_getclr(au_wait);

	adr_start(&adr, memtod(m, char *));
	adr_char(&adr, &data_header, 1);
	adr_char(&adr, &ed, 1);

	if (error) {
		val = -1;
		adr_int32(&adr, &val, 1);
	} else {
		adr_int32(&adr, &rv, 1);
	}
	m->len = adr_count(&adr);

	return (m);
}

/*
 * au_to_return64
 * returns:
 *	pointer to au_membuf chain containing a return value token.
 */
token_t *
au_to_return64(int error, int64_t rv)
{
	token_t *m;			/* local au_membuf */
	adr_t adr;			/* adr memory stream header */
	char data_header = AUT_RETURN64; /* header for this token */
	int64_t val;
	char ed = error;

	m = au_getclr(au_wait);

	adr_start(&adr, memtod(m, char *));
	adr_char(&adr, &data_header, 1);
	adr_char(&adr, &ed, 1);

	if (error) {
		val = -1;
		adr_int64(&adr, &val, 1);
	} else {
		adr_int64(&adr, &rv, 1);
	}
	m->len = adr_count(&adr);

	return (m);
}

#ifdef	AU_MAY_USE_SOMEDAY
/*
 * au_to_opaque
 * returns:
 *	pointer to au_membuf chain containing a opaque token.
 */
token_t *
au_to_opaque(bytes, opaque)

	short bytes;
	char *opaque;
{
	token_t *token;			/* local au_membuf */
	token_t *m;			/* local au_membuf */
	adr_t adr;			/* adr memory stream header */
	char data_header = AUT_OPAQUE;	/* header for this token */

	token = au_getclr(au_wait);

	adr_start(&adr, memtod(token, char *));
	adr_char(&adr, &data_header, 1);
	adr_short(&adr, &bytes, 1);

	token->len = adr_count(&adr);

	for (; bytes > 0; bytes -= AU_BUFSIZE, opaque += AU_BUFSIZE) {
		m = au_getclr(au_wait);
		(void) au_append_buf(
			opaque, bytes > AU_BUFSIZE ? AU_BUFSIZE : bytes, m);
		token = au_append_token(token, m);
	}

	return (token);
}
#endif	AU_MAY_USE_SOMEDAY

/*
 * au_to_ip
 * returns:
 *	pointer to au_membuf chain containing a ip header token
 */
token_t *
au_to_ip(struct ip *ipp)
{
	token_t *m;			/* local au_membuf */
	adr_t adr;			/* adr memory stream header */
	char data_header = AUT_IP;	/* header for this token */

	m = au_getclr(au_wait);

	adr_start(&adr, memtod(m, char *));
	adr_char(&adr, &data_header, 1);
	adr_char(&adr, (char *)ipp, 2);
	adr_short(&adr, &(ipp->ip_len), 3);
	adr_char(&adr, (char *)&(ipp->ip_ttl), 2);
	adr_short(&adr, (short *)&(ipp->ip_sum), 1);
	adr_int32(&adr, (int32_t *)&(ipp->ip_src), 2);

	m->len = adr_count(&adr);

	return (m);
}

/*
 * au_to_iport
 * returns:
 *	pointer to au_membuf chain containing a ip path token
 */
token_t *
au_to_iport(u_short iport)
{
	token_t *m;			/* local au_membuf */
	adr_t adr;			/* adr memory stream header */
	char data_header = AUT_IPORT;	/* header for this token */

	m = au_getclr(au_wait);

	adr_start(&adr, memtod(m, char *));
	adr_char(&adr, &data_header, 1);
	adr_short(&adr, (short *)&iport, 1);

	m->len = adr_count(&adr);

	return (m);
}

/*
 * au_to_in_addr
 * returns:
 *	pointer to au_membuf chain containing a ip path token
 */
token_t *
au_to_in_addr(struct in_addr *internet_addr)
{
	token_t *m;			/* local au_membuf */
	adr_t adr;			/* adr memory stream header */
	char data_header = AUT_IN_ADDR;	/* header for this token */

	m = au_getclr(au_wait);

	adr_start(&adr, memtod(m, char *));
	adr_char(&adr, &data_header, 1);
	adr_char(&adr, (char *)internet_addr, sizeof (struct in_addr));

	m->len = adr_count(&adr);

	return (m);
}
/*
 * The Modifier tokens
 */

/*
 * au_to_attr
 * returns:
 *	pointer to au_membuf chain containing an attribute token.
 */
token_t *
au_to_attr(struct vattr *attr)
{
	token_t *m;			/* local au_membuf */
	adr_t adr;			/* adr memory stream header */
#ifdef _LP64
	char data_header = AUT_ATTR64;	/* header for this token */
#else
	char data_header = AUT_ATTR32;
#endif
	int32_t value;

	m = au_getclr(au_wait);

	adr_start(&adr, memtod(m, char *));
	adr_char(&adr, &data_header, 1);
	value = (int32_t)attr->va_mode;
	ADDTRACE("[%x] au_to_attr: type %x, mode %x\n", attr->va_type, value,
		0, 0, 0, 0);
	value |= (int32_t)(VTTOIF(attr->va_type));
	adr_int32(&adr, &value, 1);
	value = (int32_t)attr->va_uid;
	adr_int32(&adr, &value, 1);
	value = (int32_t)attr->va_gid;
	adr_int32(&adr, &value, 1);
	adr_int32(&adr, (int32_t *)&(attr->va_fsid), 1);
	adr_int64(&adr, (int64_t *)&(attr->va_nodeid), 1);
#ifdef _LP64
	adr_int64(&adr, (int64_t *)&(attr->va_rdev), 1);
#else
	adr_int32(&adr, (int32_t *)&(attr->va_rdev), 1);
#endif

	m->len = adr_count(&adr);

	return (m);
}

token_t *
au_to_acl(aclp)
	struct acl *aclp;
{
	token_t *m;				/* local au_membuf */
	adr_t adr;				/* adr memory stream header */
	char data_header = AUT_ACL;		/* header for this token */
	int32_t value;

	m = au_getclr(au_wait);

	adr_start(&adr, memtod(m, char *));
	adr_char(&adr, &data_header, 1);
	ADDTRACE("[%x] au_to_acl: type %x, id %x perm %x\n",
			aclp->a_type, aclp->a_id, aclp->a_perm, 0, 0, 0);

	value = (int32_t)aclp->a_type;
	adr_int32(&adr, &value, 1);
	value = (int32_t)aclp->a_id;
	adr_int32(&adr, &value, 1);
	value = (int32_t)aclp->a_perm;
	adr_int32(&adr, &value, 1);

	m->len = adr_count(&adr);
	return (m);
}

/*
 * au_to_ipc_perm
 * returns:
 *	pointer to au_membuf chain containing a System V IPC attribute token.
 */
token_t *
au_to_ipc_perm(struct ipc_perm *perm)
{
	token_t *m;				/* local au_membuf */
	adr_t adr;				/* adr memory stream header */
	char data_header = AUT_IPC_PERM;	/* header for this token */
	int32_t value;

	m = au_getclr(au_wait);

	adr_start(&adr, memtod(m, char *));
	adr_char(&adr, &data_header, 1);
	value = (int32_t)perm->uid;
	adr_int32(&adr, &value, 1);
	value = (int32_t)perm->gid;
	adr_int32(&adr, &value, 1);
	value = (int32_t)perm->cuid;
	adr_int32(&adr, &value, 1);
	value = (int32_t)perm->cgid;
	adr_int32(&adr, &value, 1);
	value = (int32_t)perm->mode;
	adr_int32(&adr, &value, 1);
	value = (int32_t)perm->seq;
	adr_int32(&adr, &value, 1);
	value = (int32_t)perm->key;
	adr_int32(&adr, &value, 1);

	m->len = adr_count(&adr);

	return (m);
}

#ifdef NOTYET
/*
 * au_to_label
 * returns:
 *	pointer to au_membuf chain containing a label token.
 */
token_t *
au_to_label(bilabel_t *label)
{
	token_t *m;			/* local au_membuf */
	adr_t adr;			/* adr memory stream header */
	char data_header = AUT_LABEL;	/* header for this token */
	short bs = sizeof (bilabel_t);

	m = au_getclr(au_wait);

	adr_start(&adr, memtod(m, char *));
	adr_char(&adr, &data_header, 1);
	adr_short(&adr, &bs, 1);
	adr_char(&adr, (char *)label, bs);

	m->len = adr_count(&adr);

	return (m);
}
#endif	/* NOTYET */

#ifdef OLD_GROUP
token_t *
au_to_groups(struct proc *pp)
{
	token_t *m;			/* local au_membuf */
	adr_t adr;			/* adr memory stream header */
	char data_header = AUT_GROUPS;	/* header for this token */

	m = au_getclr(au_wait);

	adr_start(&adr, memtod(m, char *));
	adr_char(&adr, &data_header, 1);
	adr_int32(&adr, (int32_t *)pp->p_cred->cr_groups, ngroups_max);

	m->len = adr_count(&adr);

	return (m);
}
#endif /* OLD_GROUP */

token_t *
au_to_groups(pp)
	struct proc *pp;
{
	token_t *m;			/* local au_membuf */
	adr_t adr;			/* adr memory stream header */
	char data_header = AUT_NEWGROUPS;	/* header for this token */
	short n_groups;

	m = au_getclr(au_wait);

	adr_start(&adr, memtod(m, char *));
	adr_char(&adr, &data_header, 1);
	n_groups = (short)(pp->p_cred->cr_ngroups);
	adr_short(&adr, &n_groups, 1);
	adr_int32(&adr, (int32_t *)pp->p_cred->cr_groups,
		(int)pp->p_cred->cr_ngroups);

	m->len = adr_count(&adr);

	return (m);
}

#ifdef NFSSERVER
/*
 * au_to_socket
 * returns:
 *	pointer to au_membuf chain containing a socket token.
 */
token_t *
au_to_socket(struct socket *so)
{
	adr_t adr;
	token_t *m;
	char data_header = AUT_SOCKET;
	struct inpcb *inp = (struct inpcb *)so->so_pcb;

	m = au_getclr(au_wait);

	adr_start(&adr, memtod(m, char *));
	adr_char(&adr, &data_header, 1);
	adr_short(&adr, (short *)&so->so_type, 1);
	adr_short(&adr, (short *)&inp->inp_lport, 1);
	adr_char(&adr, (char *)&inp->inp_laddr,
			sizeof (inp->inp_laddr));
	adr_short(&adr, (short *)&inp->inp_fport, 1);
	adr_char(&adr, (char *)&inp->inp_faddr,
			sizeof (inp->inp_laddr));

	m->len = adr_count(&adr);

	return (m);
}
#endif	/* NFSSERVER */

/*
 * au_to_seq
 * returns:
 *	pointer to au_membuf chain containing a sequence token.
 */
token_t *
au_to_seq()
{
	adr_t adr;
	token_t *m;
	char data_header = AUT_SEQ;
	extern int audit_count;

	m = au_getclr(au_wait);

	adr_start(&adr, memtod(m, char *));
	adr_char(&adr, &data_header, 1);
	mutex_enter(&au_seq_lock);
	audit_count ++;
	mutex_exit(&au_seq_lock);
	adr_int32(&adr, (int32_t *)&audit_count, 1);

	m->len = adr_count(&adr);

	return (m);
}
