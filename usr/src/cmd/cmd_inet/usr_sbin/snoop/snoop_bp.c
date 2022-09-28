/*
 * Copyright (c) 1991 by Sun Microsystems, Inc.
 */

#ident	"@(#)snoop_bparam.c	1.5	93/03/07 SMI"	/* SunOS	*/

#include <sys/types.h>
#include <sys/errno.h>
#include <setjmp.h>

#include <netinet/in.h>
#include <rpc/types.h>
#include <rpc/rpc.h>
#include <rpc/xdr.h>
#include <rpc/auth.h>
#include <rpc/clnt.h>
#include <rpc/rpc_msg.h>
#include <rpcsvc/bootparam_prot.h>
#include "snoop.h"

extern char *dlc_header;
extern jmp_buf xdr_err;

void show_address();
char *sum_address();

static char *procnames_short[] = {
	"Null",		/*  0 */
	"WHOAMI?",	/*  1 */
	"GETFILE",	/*  2 */
};

static char *procnames_long[] = {
	"Null procedure",	/*  0 */
	"Who am I?",		/*  1 */
	"Get file name",	/*  2 */
};

#define	MAXPROC	2

interpret_bparam(flags, type, xid, vers, proc, data, len)
	int flags, type, xid, vers, proc;
	char *data;
	int len;
{
	char *line;
	char buff[256];
	char buff2[256];

	if (proc < 0 || proc > MAXPROC)
		return;

	if (flags & F_SUM) {
		if (setjmp(xdr_err)) {
			return;
		}

		line = get_sum_line();

		if (type == CALL) {
			(void) sprintf(line,
				"BPARAM C %s",
				procnames_short[proc]);
			line += strlen(line);

			switch (proc) {
			case BOOTPARAMPROC_WHOAMI:
				(void) sprintf(line, " %s",
					sum_address());
				break;
			case BOOTPARAMPROC_GETFILE:
				(void) getxdr_string(buff,
					MAX_MACHINE_NAME);
				(void) sprintf(line, " %s",
					getxdr_string(buff,
					MAX_FILEID));
				break;
			}

			check_retransmit(line, xid);
		} else {
			(void) sprintf(line, "BPARAM R %s ",
				procnames_short[proc]);
			line += strlen(line);
			switch (proc) {
			case BOOTPARAMPROC_WHOAMI:
				(void) getxdr_string(buff,
					MAX_MACHINE_NAME);
				(void) getxdr_string(buff2,
					MAX_MACHINE_NAME);
				(void) sprintf(line, "%s in %s",
					buff, buff2);
				break;
			case BOOTPARAMPROC_GETFILE:
				(void) getxdr_string(buff,
					MAX_MACHINE_NAME);
				(void) sum_address();
				(void) sprintf(line, "File=%s",
					getxdr_string(buff,
						MAX_PATH_LEN));
				break;
			}
		}
	}

	if (flags & F_DTAIL) {
		show_header("BPARAM:  ", "Boot Parameters", len);
		show_space();
		if (setjmp(xdr_err)) {
			return;
		}
		(void) sprintf(get_line(0, 0),
			"Proc = %d (%s)",
			proc, procnames_long[proc]);

		if (type == CALL) {
			switch (proc) {
			case BOOTPARAMPROC_WHOAMI:
				show_address("Client address");
				break;

			case BOOTPARAMPROC_GETFILE:
				(void) showxdr_string(MAX_MACHINE_NAME,
					"Hostname = %s");
				(void) showxdr_string(MAX_FILEID,
					"File = %s");
				break;
			}
		} else {
			switch (proc) {
			case BOOTPARAMPROC_WHOAMI:
				(void) showxdr_string(MAX_MACHINE_NAME,
					"Client name = %s");
				(void) showxdr_string(MAX_MACHINE_NAME,
					"Domain name = %s");
				show_address("Router addr");
				break;

			case BOOTPARAMPROC_GETFILE:
				(void) showxdr_string(MAX_MACHINE_NAME,
					"Server name = %s");
				show_address("Server addr");
				(void) showxdr_string(MAX_PATH_LEN,
					"Server file = %s");
				break;
			}
		}

		show_trailer();
	}
}

char *
sum_address()
{
	struct in_addr host;
	extern char *inet_ntoa();
	int atype;

	atype = getxdr_u_long();
	if (atype != IP_ADDR_TYPE)
		return ("?");

	host.S_un.S_un_b.s_b1 = getxdr_char();
	host.S_un.S_un_b.s_b2 = getxdr_char();
	host.S_un.S_un_b.s_b3 = getxdr_char();
	host.S_un.S_un_b.s_b4 = getxdr_char();

	return (inet_ntoa(host));
}

void
show_address(label)
	char *label;
{
	struct in_addr host;
	extern char *inet_ntoa();
	int atype;

	atype = getxdr_u_long();
	if (atype == IP_ADDR_TYPE) {
		host.S_un.S_un_b.s_b1 = getxdr_char();
		host.S_un.S_un_b.s_b2 = getxdr_char();
		host.S_un.S_un_b.s_b3 = getxdr_char();
		host.S_un.S_un_b.s_b4 = getxdr_char();

		(void) sprintf(get_line(0, 0),
			"%s = %s (%s)",
			label,
			inet_ntoa(host),
			addrtoname(host));
	} else {
		(void) sprintf(get_line(0, 0),
		"Router addr = ? (type not known)");
	}
}