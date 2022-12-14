#include <locale.h>

#ifndef lint
static char	*sccsid = "@(#)token.c 1.3 92/11/02 SMI";
static char	*cmw_sccsid = "@(#)token.c 2.4 92/08/12 SMI; SunOS CMW";
static char	*bsm_sccsid = "@(#)token.c 4.8.1.1 91/11/12 SMI; BSM Module";
static char	*mls_sccsid = "@(#)token.c 3.2 90/11/13 SMI; SunOS MLS";
#endif

/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 */

/*
 * Token processing for auditreduce.
 */
#include "auditr.h"

/*
 * These tokens are the same in CMW 1.0, 4.1.X C2-BSM and 5.X C2-BSM
 */
extern int	arbitrary_data_token();
extern int	argument32_token();
extern int	argument64_token();
extern int	file_token();
extern int	group_token();
extern int	header_token();
extern int	ip_addr_token();
extern int	ip_token();
extern int	iport_token();
extern int	opaque_token();
extern int	return_value32_token();
extern int	return_value64_token();
extern int	s5_IPC_token();
extern int	sequence_token();
extern int	server_token();
extern int	text_token();
extern int	trailer_token();
extern int	attribute32_token();
extern int	attribute64_token();
extern int	acl_token();
extern int	cmd_token();
extern int	exit_token();
extern int	liaison_token();
extern int	path_token();
extern int	process32_token();
extern int	process64_token();
extern int	s5_IPC_perm_token();
extern int	socket_token();
extern int	subject32_token();
extern int	subject64_token();
extern int	xatom_token();
extern int	xobj_token();
extern int	xproto_token();
extern int	xselect_token();
extern int	old_header_token();

extern int	newgroup_token();
extern int	exec_args_token();
extern int	exec_env_token();


static void	anchor_path();
static char	*collapse_path();

struct token_desc {
	char	*t_name;	/* name of the token */
	int	tokenid;	/* token type */
	int	t_fieldcount;	/* number of fields in this token */
	int	(*func)();	/* token processing function */
	char	*t_fields[16];	/* The fields to watch */
};
typedef struct token_desc token_desc_t;

token_desc_t tokentable[] = {
	{ "argument32",	AUT_ARG32, 	3, 	argument32_token, 	},
	{ "argument64",	AUT_ARG64, 	3, 	argument64_token, 	},
	{ "acl",	AUT_ACL,	3,	acl_token,		},
	{ "attr32", 	AUT_ATTR32, 	6, 	attribute32_token, 	},
	{ "attr64", 	AUT_ATTR64, 	6, 	attribute64_token, 	},
	{ "cmd", 	AUT_CMD, 	2, 	cmd_token, 		},
	{ "data", 	AUT_DATA, 	4, 	arbitrary_data_token, 	},
	{ "exec_args", 	AUT_EXEC_ARGS, 	-1, 	exec_args_token, 	},
	{ "exec_env", 	AUT_EXEC_ENV, 	-1, 	exec_env_token, 	},
	{ "exit", 	AUT_EXIT, 	2, 	exit_token, 		},
	{ "groups", 	AUT_GROUPS, 	16, 	group_token, 		},
	{ "header32", 	AUT_HEADER32, 	4, 	header_token,		},
	{ "header64", 	AUT_HEADER64, 	4, 	header_token,		},
	{ "in_addr", 	AUT_IN_ADDR, 	1, 	ip_addr_token, 		},
	{ "ip", 	AUT_IP,		10, 	ip_token,		},
	{ "ipc", 	AUT_IPC, 	1, 	s5_IPC_token, 		},
	{ "ipc_perm", 	AUT_IPC_PERM, 	8, 	s5_IPC_perm_token, 	},
	{ "iport", 	AUT_IPORT, 	1, 	iport_token, 		},
	{ "liaison", 	AUT_LIAISON, 	1, 	liaison_token, 		},
	{ "newgroups", 	AUT_NEWGROUPS, 	-1, 	newgroup_token, 	},
	{ "old_header",	AUT_OHEADER, 	1, 	old_header_token, 	},
	{ "opaque", 	AUT_OPAQUE, 	2, 	opaque_token, 		},
	{ "other_file32", AUT_OTHER_FILE32,	3, 	file_token, 	},
	{ "other_file64", AUT_OTHER_FILE64,	3, 	file_token, 	},
	{ "path", 	AUT_PATH, 	3, 	path_token, 		},
	{ "process32", 	AUT_PROCESS32, 	5, 	process32_token,	},
	{ "process64", 	AUT_PROCESS64, 	5, 	process64_token,	},
	{ "return32", 	AUT_RETURN32, 	2, 	return_value32_token, 	},
	{ "return64", 	AUT_RETURN64, 	2, 	return_value64_token, 	},
	{ "sequence", 	AUT_SEQ, 	1, 	sequence_token, 	},
	{ "server32", 	AUT_SERVER32, 	5, 	server_token, 		},
	{ "server64", 	AUT_SERVER64, 	5, 	server_token, 		},
	{ "socket", 	AUT_SOCKET, 	5, 	socket_token, 		},
	{ "subject32", 	AUT_SUBJECT32, 	5, 	subject32_token,	},
	{ "subject64", 	AUT_SUBJECT64, 	5, 	subject64_token,	},
	{ "text", 	AUT_TEXT, 	1, 	text_token, 		},
	{ "trailer", 	AUT_TRAILER, 	2, 	trailer_token, 		},
	{ "xatom", 	AUT_XATOM, 	2, 	xatom_token, 		},
	{ "xobj", 	AUT_XOBJ, 	4, 	xobj_token, 		},
	{ "xproto", 	AUT_XPROTO, 	1, 	xproto_token, 		},
	{ "xselect", 	AUT_XSELECT, 	4, 	xselect_token, 		},
};

token_desc_t	*tokenp;
int	numtokenentries = sizeof (tokentable) / sizeof (token_desc_t);
/*
 * List of the interesting tokens
 */
static token_desc_t *interesting_tokens;


/*
 *  Process a token in a record to determine whether the record
 *  is interesting.
 */

int
token_processing(adr, tokenid)
adr_t	*adr;
int	tokenid;
{
	int	i;
	token_desc_t t;
	token_desc_t * k;
	int	rc;

	for (i = 0; i < numtokenentries; i++) {
		k = &(tokentable[i]);
		if ((k->tokenid) == tokenid) {
			rc = (*tokentable[i].func)(adr);
			return (rc);
		}
	}
	/* here if token id is not in table */
	return (-2);
}


/*
 * There should not be any file or header tokens in the middle of
 * a record
 */

file_token(adr)
adr_t	*adr;
{
	return (-2);
}


int
header_token(adr)
adr_t	*adr;
{
	return (-2);
}


int
old_header_token(adr)
adr_t	*adr;
{
	return (-2);
}


/*
 * ======================================================
 *  The following token processing routines return
 *  -1: if the record is not interesting
 *  -2: if an error is found
 * ======================================================
 */

int
trailer_token(adr)
adr_t	*adr;
{
	int	returnstat = 0;
	int	i;
	short	magic_number;
	uint32_t bytes;

	adrm_u_short(adr, (u_short *) & magic_number, 1);
	if (magic_number != AUT_TRAILER_MAGIC) {
		fprintf(stderr, "%s\n",
			gettext("auditreduce: Bad trailer token"));
		return (-2);
	}
	adrm_u_int32(adr, &bytes, 1);

	return (-1);
}


/*
 * Format of arbitrary data token:
 *	arbitrary data token id	adr char
 * 	how to print		adr_char
 *	basic unit		adr_char
 *	unit count		adr_char, specifying number of units of
 *	data items		depends on basic unit
 *
 */
int
arbitrary_data_token(adr)
adr_t	*adr;
{
	int	i;
	char	c1;
	short	c2;
	int32_t	c3;
	int64_t c4;
	char	how_to_print, basic_unit, unit_count;

	/* get how_to_print, basic_unit, and unit_count */
	adrm_char(adr, &how_to_print, 1);
	adrm_char(adr, &basic_unit, 1);
	adrm_char(adr, &unit_count, 1);
	for (i = 0; i < unit_count; i++) {
		switch (basic_unit) {
			/* case AUR_BYTE: has same value as AUR_CHAR */
		case AUR_CHAR:
			adrm_char(adr, &c1, 1);
			break;
		case AUR_SHORT:
			adrm_short(adr, &c2, 1);
			break;
		case AUR_INT32:
			adrm_int32(adr, (int32_t *) & c3, 1);
			break;
		case AUR_INT64:
			adrm_int64(adr, (int64_t *) & c4, 1);
			break;
		default:
			return (-2);
			break;
		}
	}
	return (-1);
}


/*
 * Format of opaque token:
 *	opaque token id		adr_char
 *	size			adr_short
 *	data			adr_char, size times
 *
 */
int
opaque_token(adr)
adr_t	*adr;
{
	int	i;
	short	size;
	char	*charp;

	adrm_short(adr, &size, 1);
	/* try to allocate memory for the character string */
	charp = a_calloc(1, (size_t) size);

	adrm_char(adr, charp, size);

	free(charp);
	return (-1);
}



/*
 * Format of return32 value token:
 * 	return value token id	adr_char
 *	error number		adr_char
 *	return value		adr_u_int32
 *
 */
int
return_value32_token(adr)
adr_t	*adr;
{
	char		errnum;
	uint32_t	value;

	adrm_char(adr, &errnum, 1);
	adrm_u_int32(adr, &value, 1);
	if ((flags & M_SORF) &&
		((global_class & mask.am_success) && (errnum == 0)) ||
		((global_class & mask.am_failure) && (errnum != 0))) {
			checkflags |= M_SORF;
	}
	return (-1);
}

/*
 * Format of return64 value token:
 * 	return value token id	adr_char
 *	error number		adr_char
 *	return value		adr_u_int64
 *
 */
int
return_value64_token(adr)
adr_t	*adr;
{
	char		errnum;
	uint64_t	value;

	adrm_char(adr, &errnum, 1);
	adrm_u_int64(adr, &value, 1);
	if ((flags & M_SORF) &&
		((global_class & mask.am_success) && (errnum == 0)) ||
		((global_class & mask.am_failure) && (errnum != 0))) {
			checkflags |= M_SORF;
	}
	return (-1);
}


/*
 * Format of sequence token:
 *	sequence token id	adr_char
 *	audit_count		int32_t
 *
 */
int
sequence_token(adr)
adr_t	*adr;
{
	int32_t	audit_count;

	adrm_int32(adr, &audit_count, 1);
	return (-1);
}


/*
 * Format of server token:
 *	server token id		adr_char
 *	auid			adr_u_short
 *	euid			adr_u_short
 *	ruid			adr_u_short
 *	egid			adr_u_short
 *	pid			adr_u_short
 *
 */
int
server_token(adr)
adr_t	*adr;
{
	unsigned short	auid, euid, ruid, egid, pid;

	adrm_u_short(adr, &auid, 1);
	adrm_u_short(adr, &euid, 1);
	adrm_u_short(adr, &ruid, 1);
	adrm_u_short(adr, &egid, 1);
	adrm_u_short(adr, &pid, 1);

	if (flags & M_USERA) {
		if (m_usera == auid)
			checkflags = checkflags | M_USERA;
	}
	if (flags & M_USERE) {
		if (m_usere == euid)
			checkflags = checkflags | M_USERE;
	}
	if (flags & M_USERR) {
		if (m_userr == ruid)
			checkflags = checkflags | M_USERR;
	}
	if (flags & M_GROUPE) {
		if (m_groupe == egid)
			checkflags = checkflags | M_GROUPE;
	}
	return (-1);
}


/*
 * Format of text token:
 *	text token id		adr_char
 * 	text			adr_string
 *
 */
int
text_token(adr)
adr_t	*adr;
{
	char	text[MAXFILELEN];

	text[0] = '\0';
	adrm_string(adr, text);

	return (-1);
}


/*
 * Format of ip_addr token:
 *	ip token id	adr_char
 *	address		adr_int32
 *
 */
int
ip_addr_token(adr)
adr_t	*adr;
{
	int32_t	address;

	adrm_char(adr, (char *) &address, 4);

	return (-1);
}


/*
 * Format of ip token:
 *	ip header token id	adr_char
 *	version			adr_char
 *	type of service		adr_char
 *	length			adr_short
 *	id			adr_u_short
 *	offset			adr_u_short
 *	ttl			adr_char
 *	protocol		adr_char
 *	checksum		adr_u_short
 *	source address		adr_int32
 *	destination address	adr_int32
 *
 */
int
ip_token(adr)
adr_t	*adr;
{
	char	version;
	char	type;
	short	len;
	unsigned short	id, offset, checksum;
	char	ttl, protocol;
	int32_t	src, dest;

	adrm_char(adr, &version, 1);
	adrm_char(adr, &type, 1);
	adrm_short(adr, &len, 1);
	adrm_u_short(adr, &id, 1);
	adrm_u_short(adr, &offset, 1);
	adrm_char(adr, &ttl, 1);
	adrm_char(adr, &protocol, 1);
	adrm_u_short(adr, &checksum, 1);
	adrm_char(adr, (char *) &src, 4);
	adrm_char(adr, (char *) &dest, 4);

	return (-1);
}


/*
 * Format of iport token:
 *	ip port address token id	adr_char
 *	port address			adr_short
 *
 */
int
iport_token(adr)
adr_t	*adr;
{
	short	address;

	adrm_short(adr, &address, 1);

	return (-1);
}


/*
 * Format of groups token:
 *	group token id		adr_char
 *	group list		adr_int32, 16 times
 *
 */
int
group_token(adr)
adr_t	*adr;
{
	int	gid[16];
	int	i;
	int	flag = 0;

	for (i = 0; i < 16; i++) {
		adrm_int32(adr, (int32_t *) & gid[i], 1);
		if (flags & M_GROUPR) {
			if ((unsigned short)m_groupr == gid[i])
				flag = 1;
		}
	}

	if (flags & M_GROUPR) {
		if (flag)
			checkflags = checkflags | M_GROUPR;
	}
	return (-1);
}

/*
 * Format of newgroups token:
 *	group token id		adr_char
 *	number of groups	adr_short
 *	group list		adr_int32, "number" times
 *
 */
int
newgroup_token(adr)
adr_t	*adr;
{
	int	gid[NGROUPS_MAX];
	int	i;
	short int   number;
	int	flag = 0;

	adrm_short(adr, &number, 1);

	for (i = 0; i < number; i++) {
		adrm_int32(adr, (int32_t *) & gid[i], 1);
		if (flags & M_GROUPR) {
			if ((unsigned short)m_groupr == gid[i])
				flag = 1;
		}
	}

	if (flags & M_GROUPR) {
		if (flag)
			checkflags = checkflags | M_GROUPR;
	}
	return (-1);
}

/*
 * Format of argument32 token:
 *	argument token id	adr_char
 *	argument number		adr_char
 *	argument value		adr_int32
 *	argument description	adr_string
 *
 */
int
argument32_token(adr)
adr_t	*adr;
{
	char	arg_num;
	int32_t	arg_val;
	char	text[MAXFILELEN];

	adrm_char(adr, &arg_num, 1);
	adrm_int32(adr, &arg_val, 1);
	text[0] = '\0';
	adrm_string(adr, text);

	return (-1);
}

/*
 * Format of argument64 token:
 *	argument token id	adr_char
 *	argument number		adr_char
 *	argument value		adr_int64
 *	argument description	adr_string
 *
 */
int
argument64_token(adr)
adr_t	*adr;
{
	char	arg_num;
	int64_t	arg_val;
	char	text[MAXFILELEN];

	adrm_char(adr, &arg_num, 1);
	adrm_int64(adr, &arg_val, 1);
	text[0] = '\0';
	adrm_string(adr, text);

	return (-1);
}

int
acl_token(adr)
adr_t	*adr;
{

	int32_t	id;
	int32_t	mode;
	int32_t	type;

	adrm_int32(adr, &type, 1);
	adrm_int32(adr, &id, 1);
	adrm_int32(adr, &mode, 1);

	return (-1);
}

/*
 * Format of attribute32 token:
 *	attribute token id	adr_char
 * 	mode			adr_int32 (printed in octal)
 *	uid			adr_int32
 *	gid			adr_int32
 *	file system id		adr_int32
 *	node id			adr_int64
 *	device			adr_int32
 *
 */
int
attribute32_token(adr)
adr_t	*adr;
{
	int32_t	dev;
	int32_t	file_sysid;
	int32_t	gid;
	int32_t	mode;
	int64_t	nodeid;
	int32_t	uid;

	adrm_int32(adr, &mode, 1);
	adrm_int32(adr, &uid, 1);
	adrm_int32(adr, &gid, 1);
	adrm_int32(adr, &file_sysid, 1);
	adrm_int64(adr, &nodeid, 1);
	adrm_int32(adr, &dev, 1);

	if (flags & M_USERE) {
		if (m_usere == uid)
			checkflags = checkflags | M_USERE;
	}
	if (flags & M_GROUPR) {
		if (m_groupr == gid)
			checkflags = checkflags | M_GROUPR;
	}
	return (-1);
}

/*
 * Format of attribute64 token:
 *	attribute token id	adr_char
 * 	mode			adr_int32 (printed in octal)
 *	uid			adr_int32
 *	gid			adr_int32
 *	file system id		adr_int32
 *	node id			adr_int64
 *	device			adr_int64
 *
 */
int
attribute64_token(adr)
adr_t	*adr;
{
	int64_t	dev;
	int32_t	file_sysid;
	int32_t	gid;
	int32_t	mode;
	int64_t	nodeid;
	int32_t	uid;

	adrm_int32(adr, &mode, 1);
	adrm_int32(adr, &uid, 1);
	adrm_int32(adr, &gid, 1);
	adrm_int32(adr, &file_sysid, 1);
	adrm_int64(adr, &nodeid, 1);
	adrm_int64(adr, &dev, 1);

	if (flags & M_USERE) {
		if (m_usere == uid)
			checkflags = checkflags | M_USERE;
	}
	if (flags & M_GROUPR) {
		if (m_groupr == gid)
			checkflags = checkflags | M_GROUPR;
	}
	return (-1);
}


/*
 * Format of command token:
 *	attribute token id	adr_char
 *	argc			adr_short
 *	argv len		adr_short	variable amount of argv len
 *	argv text		argv len	and text
 *	.
 *	.
 *	.
 *	envp count		adr_short	variable amount of envp len
 *	envp len		adr_short	and text
 *	envp text		envp		len
 *	.
 *	.
 *	.
 *
 */
int
cmd_token(adr)
adr_t	*adr;
{
	short	cnt;
	short	i;

	char	s[2048];

	adrm_short(adr, &cnt, 1);

	for (i = 0; i < cnt; i++)
		adrm_string(adr, s);

	adrm_short(adr, &cnt, 1);

	for (i = 0; i < cnt; i++)
		adrm_string(adr, s);

	return (-1);
}


/*
 * Format of exit token:
 *	attribute token id	adr_char
 *	return value		adr_int32
 *	errno			adr_int32
 *
 */
int
exit_token(adr)
adr_t	*adr;
{
	int32_t	retval;
	int32_t	errno;

	adrm_int32(adr, &retval, 1);
	adrm_int32(adr, &errno, 1);
	return (-1);
}

/*
 * Format of exec_args token:
 *	attribute token id	adr_char
 *	count value		adr_int32
 *	strings			null terminated strings
 *
 */
int
exec_args_token(adr)
adr_t *adr;
{
	int count, i;
	char c;

	adrm_int32(adr, (int32_t *)&count, 1);
	for (i = 1; i <= count; i++) {
		adrm_char(adr, &c, 1);
		while (c != (char) 0)
			adrm_char(adr, &c, 1);
	}
	/* no dump option here, since we will have variable length fields */
	return (-1);
}

/*
 * Format of exec_env token:
 *	attribute token id	adr_char
 *	count value		adr_int32
 *	strings			null terminated strings
 *
 */
int
exec_env_token(adr)
adr_t *adr;
{
	int count, i;
	char c;

	adrm_int32(adr, (int32_t *)&count, 1);
	for (i = 1; i <= count; i++) {
		adrm_char(adr, &c, 1);
		while (c != (char) 0)
			adrm_char(adr, &c, 1);
	}
	/* no dump option here, since we will have variable length fields */
	return (-1);
}

/*
 * Format of liaison token:
 */
int
liaison_token(adr)
adr_t	*adr;
{
	int32_t	li;

	adrm_int32(adr, &li, 1);
	return (-1);
}


/*
 * Format of path token:
 *	path				adr_string
 */
int
path_token(adr)
adr_t 	*adr;
{
	static char	path[MAXFILELEN+1];

	adrm_string(adr, path);
	if ((flags & M_OBJECT) && (obj_flag == OBJ_PATH)) {
		if (path[0] != '/')
			/*
			 * anchor the path. user apps may not do it.
			 */
			anchor_path(path);
		/*
		 * match against the collapsed path. that is what user sees.
		 */
		if (re_exec2(collapse_path(path)) == 1)
			checkflags = checkflags | M_OBJECT;
	}
	return (-1);
}


/*
 * Format of System V IPC permission token:
 *	System V IPC permission token id	adr_char
 * 	uid					adr_int32
 *	gid					adr_int32
 *	cuid					adr_int32
 *	cgid					adr_int32
 *	mode					adr_int32
 *	seq					adr_int32
 *	key					adr_int32
 *	label					adr_opaque, sizeof (bslabel_t)
 *							    bytes
 */
s5_IPC_perm_token(adr)
adr_t	*adr;
{
	int32_t	uid, gid, cuid, cgid, mode, seq;
	int32_t	key;

	adrm_int32(adr, &uid, 1);
	adrm_int32(adr, &gid, 1);
	adrm_int32(adr, &cuid, 1);
	adrm_int32(adr, &cgid, 1);
	adrm_int32(adr, &mode, 1);
	adrm_int32(adr, &seq, 1);
	adrm_int32(adr, &key, 1);

	if (flags & M_USERE) {
		if (m_usere == uid)
			checkflags = checkflags | M_USERE;
	}

	if (flags & M_USERE) {
		if (m_usere == cuid)
			checkflags = checkflags | M_USERE;
	}

	if (flags & M_GROUPR) {
		if (m_groupr == gid)
			checkflags = checkflags | M_GROUPR;
	}
	return (-1);
}


/*
 * Format of process32 token:
 *	process token id	adr_char
 *	auid			adr_int32
 *	euid			adr_int32
 *	egid 			adr_int32
 * 	ruid			adr_int32
 *	rgid			adr_int32
 * 	pid			adr_int32
 * 	sid			adr_int32
 * 	termid			adr_int32*2
 *
 */
int
process32_token(adr)
adr_t	*adr;
{
	int32_t	auid, euid, egid, ruid, rgid, pid;
	int32_t	sid;
	int32_t port, machine;

	adrm_int32(adr, &auid, 1);
	adrm_int32(adr, &euid, 1);
	adrm_int32(adr, &egid, 1);
	adrm_int32(adr, &ruid, 1);
	adrm_int32(adr, &rgid, 1);
	adrm_int32(adr, &pid, 1);
	adrm_int32(adr, &sid, 1);
	adrm_int32(adr, &port, 1);
	adrm_int32(adr, &machine, 1);

	if (flags & M_OBJECT && obj_flag == OBJ_PROC) {
		if (obj_id == pid)
			checkflags = checkflags | M_OBJECT;
	}
	if (flags & M_USERA) {
		if (m_usera == auid)
			checkflags = checkflags | M_USERA;
	}
	if (flags & M_USERE) {
		if (m_usere == euid)
			checkflags = checkflags | M_USERE;
	}
	if (flags & M_USERR) {
		if (m_userr == ruid)
			checkflags = checkflags | M_USERR;
	}
	if (flags & M_GROUPR) {
		if (m_groupr == egid)
			checkflags = checkflags | M_GROUPR;
	}
	if (flags & M_GROUPE) {
		if (m_groupe == egid)
			checkflags = checkflags | M_GROUPE;
	}
	return (-1);
}

/*
 * Format of process64 token:
 *	process token id	adr_char
 *	auid			adr_int32
 *	euid			adr_int32
 *	egid 			adr_int32
 * 	ruid			adr_int32
 *	rgid			adr_int32
 * 	pid			adr_int32
 * 	sid			adr_int32
 * 	termid			adr_int64+adr_int32
 *
 */
int
process64_token(adr)
adr_t	*adr;
{
	int32_t	auid, euid, egid, ruid, rgid, pid;
	int32_t	sid;
	int64_t port;
	int32_t machine;

	adrm_int32(adr, &auid, 1);
	adrm_int32(adr, &euid, 1);
	adrm_int32(adr, &egid, 1);
	adrm_int32(adr, &ruid, 1);
	adrm_int32(adr, &rgid, 1);
	adrm_int32(adr, &pid, 1);
	adrm_int32(adr, &sid, 1);
	adrm_int64(adr, &port, 1);
	adrm_int32(adr, &machine, 1);

	if (flags & M_OBJECT && obj_flag == OBJ_PROC) {
		if (obj_id == pid)
			checkflags = checkflags | M_OBJECT;
	}
	if (flags & M_USERA) {
		if (m_usera == auid)
			checkflags = checkflags | M_USERA;
	}
	if (flags & M_USERE) {
		if (m_usere == euid)
			checkflags = checkflags | M_USERE;
	}
	if (flags & M_USERR) {
		if (m_userr == ruid)
			checkflags = checkflags | M_USERR;
	}
	if (flags & M_GROUPR) {
		if (m_groupr == egid)
			checkflags = checkflags | M_GROUPR;
	}
	if (flags & M_GROUPE) {
		if (m_groupe == egid)
			checkflags = checkflags | M_GROUPE;
	}
	return (-1);
}


/*
 * Format of System V IPC token:
 *	System V IPC token id	adr_char
 *	object id		adr_int32
 *
 */
int
s5_IPC_token(adr)
adr_t	*adr;
{
	char	ipc_type;
	int32_t	ipc_id;

	adrm_char(adr, &ipc_type, 1);
	adrm_int32(adr, &ipc_id, 1);

	if ((flags & M_OBJECT) &&
	    ipc_type_match(obj_flag, ipc_type) &&
	    (obj_id == ipc_id))
		checkflags = checkflags | M_OBJECT;

	return (-1);
}


/*
 * Format of socket token:
 *	socket_type		adrm_short
 *	local_port		adrm_short
 *	local_inaddr		adrm_int32
 *	remote_port		adrm_short
 *	remote_inaddr		adrm_int32
 *
 */
int
socket_token(adr)
adr_t	*adr;
{
	short	socket_type;
	short	local_port;
	int32_t	local_inaddr;
	short	remote_port;
	int32_t	remote_inaddr;

	adrm_short(adr, &socket_type, 1);
	adrm_short(adr, &local_port, 1);
	adrm_char(adr, (char *) &local_inaddr, 4);
	adrm_short(adr, &remote_port, 1);
	adrm_char(adr, (char *) &remote_inaddr, 4);

	if ((flags & M_OBJECT) && (obj_flag == OBJ_SOCK)) {
		if (socket_flag == SOCKFLG_MACHINE) {
			if (local_inaddr  == obj_id ||
			    remote_inaddr == obj_id)
				checkflags = checkflags | M_OBJECT;
		} else if (socket_flag == SOCKFLG_PORT) {
			if (local_port  == obj_id ||
			    remote_port == obj_id)
				checkflags = checkflags | M_OBJECT;
		}
	}
	return (-1);
}


/*
 * Format of subject32 token:
 *	subject token id	adr_char
 *	auid			adr_int32
 *	euid			adr_int32
 *	egid 			adr_int32
 * 	ruid			adr_int32
 *	rgid			adr_int32
 * 	pid			adr_int32
 * 	sid			adr_int32
 * 	termid			adr_int32*2
 *
 */
int
subject32_token(adr)
adr_t	*adr;
{
	int32_t	auid, euid, egid, ruid, rgid, pid;
	int32_t	sid;
	int32_t port, machine;

	adrm_int32(adr, &auid, 1);
	adrm_int32(adr, &euid, 1);
	adrm_int32(adr, &egid, 1);
	adrm_int32(adr, &ruid, 1);
	adrm_int32(adr, &rgid, 1);
	adrm_int32(adr, &pid, 1);
	adrm_int32(adr, &sid, 1);
	adrm_int32(adr, &port, 1);
	adrm_int32(adr, &machine, 1);

	if (flags & M_SUBJECT) {
		if (subj_id == pid)
			checkflags = checkflags | M_SUBJECT;
	}
	if (flags & M_USERA) {
		if (m_usera == auid)
			checkflags = checkflags | M_USERA;
	}
	if (flags & M_USERE) {
		if (m_usere == euid)
			checkflags = checkflags | M_USERE;
	}
	if (flags & M_USERR) {
		if (m_userr == ruid)
			checkflags = checkflags | M_USERR;
	}
	if (flags & M_GROUPR) {
		if (m_groupr == egid)
			checkflags = checkflags | M_GROUPR;
	}
	if (flags & M_GROUPE) {
		if (m_groupe == egid)
			checkflags = checkflags | M_GROUPE;
	}
	return (-1);
}

/*
 * Format of subject64 token:
 *	subject token id	adr_char
 *	auid			adr_int32
 *	euid			adr_int32
 *	egid 			adr_int32
 * 	ruid			adr_int32
 *	rgid			adr_int32
 * 	pid			adr_int32
 * 	sid			adr_int32
 * 	termid			adr_int64+adr_int32
 *
 */
int
subject64_token(adr)
adr_t	*adr;
{
	int32_t	auid, euid, egid, ruid, rgid, pid;
	int32_t	sid;
	int64_t port;
	int32_t machine;

	adrm_int32(adr, &auid, 1);
	adrm_int32(adr, &euid, 1);
	adrm_int32(adr, &egid, 1);
	adrm_int32(adr, &ruid, 1);
	adrm_int32(adr, &rgid, 1);
	adrm_int32(adr, &pid, 1);
	adrm_int32(adr, &sid, 1);
	adrm_int64(adr, &port, 1);
	adrm_int32(adr, &machine, 1);

	if (flags & M_SUBJECT) {
		if (subj_id == pid)
			checkflags = checkflags | M_SUBJECT;
	}
	if (flags & M_USERA) {
		if (m_usera == auid)
			checkflags = checkflags | M_USERA;
	}
	if (flags & M_USERE) {
		if (m_usere == euid)
			checkflags = checkflags | M_USERE;
	}
	if (flags & M_USERR) {
		if (m_userr == ruid)
			checkflags = checkflags | M_USERR;
	}
	if (flags & M_GROUPR) {
		if (m_groupr == egid)
			checkflags = checkflags | M_GROUPR;
	}
	if (flags & M_GROUPE) {
		if (m_groupe == egid)
			checkflags = checkflags | M_GROUPE;
	}
	return (-1);
}


/*
 * Format of xatom token:
 */
int
xatom_token(adr)
adr_t	*adr;
{
	u_short alen;
	char	*atom;

	adrm_short(adr, (short *) & alen, 1);
	atom = a_calloc(1, (size_t)alen);
	adrm_char(adr, atom, alen);
	free(atom);

	return (-1);
}


/*
 * Format of xobj token:
 */
int
xobj_token(adr)
adr_t	*adr;
{
	int32_t	oid, xid, cuid;

	adrm_int32(adr, &oid, 1);
	adrm_int32(adr, &xid, 1);
	adrm_int32(adr, &cuid, 1);

	return (-1);
}


/*
 * Format of xproto token:
 */
int
xproto_token(adr)
adr_t	*adr;
{
	int32_t	pid;
	adrm_int32(adr, &pid, 1);
	return (-1);
}


/*
 * Format of xselect token:
 */
int
xselect_token(adr)
adr_t	*adr;
{
	short	len;
	char	*pstring;
	char	*type;
	char	*data;

	adrm_short(adr, &len, 1);
	pstring = a_calloc(1, (size_t)len);
	adrm_char(adr, pstring, len);
	adrm_short(adr, &len, 1);
	type = a_calloc(1, (size_t)len);
	adrm_char(adr, type, len);
	adrm_short(adr, &len, 1);
	data = a_calloc(1, (size_t)len);
	adrm_char(adr, data, len);
	free(pstring);
	free(type);
	free(data);

	return (-1);
}

/*
 * anchor a path name with a slash
 * assume we have enough space
 */
static void
anchor_path(path)
char	*path;
{
	(void) memmove((void *)(path + 1), (void *)path, strlen(path) + 1);
	*path = '/';
}


/*
 * copy path to collapsed path.
 * collapsed path does not contain:
 *	successive slashes
 *	instances of dot-slash
 *	instances of dot-dot-slash
 * passed path must be anchored with a '/'
 */
static char	*
collapse_path(s)
char	*s; /* source path */
{
	int	id;	/* index of where we are in destination string */
	int	is;	/* index of where we are in source string */
	int	slashseen;	/* have we seen a slash */
	int	ls;		/* length of source string */

	ls = strlen(s) + 1;

	slashseen = 0;
	for (is = 0, id = 0; is < ls; is++) {
		/* thats all folks, we've reached the end of input */
		if (s[is] == '\0') {
			if (id > 1 && s[id-1] == '/') {
				--id;
			}
			s[id++] = '\0';
			break;
		}
		/* previous character was a / */
		if (slashseen) {
			if (s[is] == '/')
				continue;	/* another slash, ignore it */
		} else if (s[is] == '/') {
			/* we see a /, just copy it and try again */
			slashseen = 1;
			s[id++] = '/';
			continue;
		}
		/* /./ seen */
		if (s[is] == '.' && s[is+1] == '/') {
			is += 1;
			continue;
		}
		/* XXX/. seen */
		if (s[is] == '.' && s[is+1] == '\0') {
			if (id > 1)
				id--;
			continue;
		}
		/* XXX/.. seen */
		if (s[is] == '.' && s[is+1] == '.' && s[is+2] == '\0') {
			is += 1;
			if (id > 0)
				id--;
			while (id > 0 && s[--id] != '/');
			id++;
			continue;
		}
		/* XXX/../ seen */
		if (s[is] == '.' && s[is+1] == '.' && s[is+2] == '/') {
			is += 2;
			if (id > 0)
				id--;
			while (id > 0 && s[--id] != '/');
			id++;
			continue;
		}
		while (is < ls && (s[id++] = s[is++]) != '/');
		is--;
	}
	return (s);
}


static int
ipc_type_match(flag, type)
int	flag;
char	type;
{
	if (flag == OBJ_SEM && type == AT_IPC_SEM)
		return (1);

	if (flag == OBJ_MSG && type == AT_IPC_MSG)
		return (1);

	if (flag == OBJ_SHM && type == AT_IPC_SHM)
		return (1);

	return (0);
}
