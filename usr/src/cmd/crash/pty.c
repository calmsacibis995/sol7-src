/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 *		Copyright (C) 1991  Sun Microsystems, Inc
 *			All rights reserved.
 *		Notice of copyright on this source code
 *		product does not indicate publication.
 *
 *		RESTRICTED RIGHTS LEGEND:
 *   Use, duplication, or disclosure by the Government is subject
 *   to restrictions as set forth in subparagraph (c)(1)(ii) of
 *   the Rights in Technical Data and Computer Software clause at
 *   DFARS 52.227-7013 and in similar clauses in the FAR and NASA
 *   FAR Supplement.
 */

/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pty.c	1.8	97/09/07 SMI"

/*
 * This file contains code for the crash function pty.
 */

#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stream.h>
#include <sys/termios.h>
#include <sys/ptms.h>
#include <sys/ptem.h>
#include "crash.h"

static short print_header;

static void prspty();
static struct queue *prsptem(int, int, struct queue *);
static void prspckt(struct queue *);

/* get arguments for tty function */
int
getpty()
{
	long slot = -1;
	int full = 0;
	int all = 0;
	int phys = 0;
	int count;
	int line_disp = 0;
	int ptem_flag = 0;
	int pckt_flag = 0;
	int c;

	char *type = "";
	char *heading = "SLOT   MWQPTR   SWQPTR  PT_BUFP  TTYPID STATE\n";

	long addr;
	long arg2 = -1;
	long arg1 = -1;


	optind = 1;
	while ((c = getopt(argcnt, args, "efhplsw:t:")) != EOF) {
		switch (c) {
			case 'e':	all = 1;
					break;
			case 'f':	full = 1;
					break;
			case 'p':	phys = 1;
					break;
			case 't':	type = optarg;
					break;
			case 'w':	redirect();
					break;
			case 'l':	line_disp = 1;
					break;
			case 'h':	ptem_flag = 1;
					break;
			case 's':	pckt_flag = 1;
					break;
			default:	longjmp(syn, 0);
		}
	}
	if ((strcmp("", type) == 0) && !args[optind]) {
		addr = getaddr("ptms");
		count = getcount("pt");
		readmem((void *)addr, 1, -1, (char *)&addr, sizeof (long),
							"ptms_pty address");
		print_header = 0;
		fprintf(fp, "ptms_tty TABLE SIZE = %d\n", count);

		if (!full)
			fprintf(fp, "%s", heading);

		for (slot = 0; slot < count; slot++)
			prspty(all, full, slot, phys, addr, "", line_disp,
				ptem_flag, pckt_flag, heading);
	} else {
		if (strcmp(type, "")) {
			addr = getaddr(type);
			count = getcount(type);
			readmem((void *)addr, 1, -1, &addr, sizeof (long),
				"%s address");
			fprintf(fp, "%s TABLE SIZE = %d\n", type, count);
		} else {
			addr = getaddr("ptms");
			count = getcount("pt");
			readmem((void *)addr, 1, -1, &addr, sizeof (long),
				"ptms_pty address");
			fprintf(fp, "ptms_tty TABLE SIZE = %d\n", count);
		}
		if (!full)
			fprintf(fp, "%s", heading);

		if (args[optind]) {
			all = 1;
			do {
				getargs(count, &arg1, &arg2, phys);
				if (arg1 == -1)
					continue;

				if (arg2 != -1)
					for (slot = arg1; slot <= arg2; slot++)
						prspty(all, full, slot, phys,
							addr, type, line_disp,
							ptem_flag, pckt_flag,
							heading);
				else {
					if ((unsigned long) arg1 < count)
						slot = arg1;
					else
						addr = arg1;

					prspty(all, full, slot, phys, addr,
						type, line_disp, ptem_flag,
						pckt_flag, heading);
				}
				slot = arg1 = arg2 = -1;
				if (strcmp(type, ""))
					addr = getaddr(type);
			} while (args[++optind]);
		} else
			for (slot = 0; slot < count; slot++)
				prspty(all, full, slot, phys, addr, "",
					line_disp, ptem_flag, pckt_flag,
					heading);
	}
	return (0);
}

/*
 * print streams pseudo tty table
 */
static void
prspty(all, full, slot, phys, addr, type, line_disp,
ptem_flag, pckt_flag, heading)
int all, full;
long slot;
int phys;
long addr;
char *type;
char *heading;
{
	struct pt_ttys	pt_tty;
	struct queue	q;
	int count;
	long base;
	intptr_t offset;


	if ((phys || !Virtmode) && (slot == -1))
		readmem((void *)addr, 0, -1, &pt_tty, sizeof (pt_tty),
			"ptms_tty structure");
	else if (slot == -1)
		readmem((void *)addr, 1, -1, &pt_tty, sizeof (pt_tty),
			"ptms_tty structure");
	else
		readmem((void *)(addr + slot * sizeof (pt_tty)), 1, -1,
			&pt_tty, sizeof (pt_tty),
			"ptms_tty structure");

	/*
	 * A free entry is one that does not have PTLOCK, PTMOPEN and
	 * PTSOPEN flags all set
	 */
	if (!(pt_tty.pt_state & (PTMOPEN | PTSOPEN | PTLOCK)) && !all)
		return;

	if (full || print_header) {
		print_header = 0;
		fprintf(fp, "%s", heading);
	}

	if ((slot == -1) && (strcmp(type, ""))) {
		base = getaddr(type);
		count = getcount(type);
		slot = getslot(addr, base, sizeof (pt_tty), phys, count);
	}

	if (slot == -1)
		fprintf(fp, "  - ");
	else
		fprintf(fp, "%4ld", slot);

	fprintf(fp, "%9p%9p%9p%8hd",	pt_tty.ptm_rdq,
					pt_tty.pts_rdq,
					pt_tty.pt_bufp,
					pt_tty.tty);

	fprintf(fp, "%s%s%s\n",
		pt_tty.pt_state & PTMOPEN ? " mopen" : "",
		pt_tty.pt_state & PTSOPEN ? " sopen" : "",
		pt_tty.pt_state & PTLOCK ? " lock" : "");

	if (line_disp || ptem_flag) {
		offset = (long)(pt_tty.pts_rdq) - (long)(sizeof (struct queue));
		readmem((void *)offset, 1, -1, &q, sizeof (struct queue), "");
		offset = (intptr_t)prsptem(full, ptem_flag, q.q_next);
		if (line_disp && offset && !prsldterm(full, offset))
			print_header = 1;
	}
	if (pckt_flag) {
		offset = (long)(pt_tty.ptm_rdq) - (long)(sizeof (struct queue));
		readmem((void *)offset, 1, -1, &q, sizeof (struct queue), "");
		prspckt(q.q_next);
	}
}

struct queue *
prsptem(all, print, addr)
int all;
int print;
struct queue *addr;
{
	char *heading = "\tMODNAME   MODID DACK_PTR  RDQ_PTR  STATE\n";
	char mname[9];	/* Buffer for the module name */

	struct ptem ptem;
	struct queue q;
	struct qinit qinfo;
	struct module_info minfo;

	/*
	 * Wade through the link-lists to extract the line disicpline
	 * name and id
	 */
	readmem(addr, 1, -1, &q, sizeof (struct queue), "");
	/*
	 * q_next is zero at the stream head, i.e. no modules have
	 *  been pushed
	 */
	if (!q.q_next)
		return (0);
	readmem(q.q_qinfo, 1, -1, &qinfo, sizeof (struct qinit), "");
	readmem(qinfo.qi_minfo, 1, -1, &minfo, sizeof (struct module_info), "");
	readmem(minfo.mi_idname, 1, -1, mname, 8, "");
	mname[8] = '\0';

	readmem(q.q_ptr, 1, -1, &ptem, sizeof (struct ptem), "");

	if (print) {
		fprintf(fp, "%s", heading);
		fprintf(fp, "\t%-9s%6d%9p%9p", mname, minfo.mi_idnum,
			ptem.dack_ptr, ptem.q_ptr);
		fprintf(fp, "%s%s\n",
			(ptem.state & REMOTEMODE) ? " remote"    : "",
			(ptem.state & OFLOW_CTL) ? " outflow"    : "");
	}

	if (!all)
		return (q.q_next);

	if (print) {
		fprintf(fp, "\tcflag: %s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
			(ptem.cflags & CBAUD) == B0    ? " b0"    : "",
			(ptem.cflags & CBAUD) == B50   ? " b50"   : "",
			(ptem.cflags & CBAUD) == B75   ? " b75"   : "",
			(ptem.cflags & CBAUD) == B110  ? " b110"  : "",
			(ptem.cflags & CBAUD) == B134  ? " b134"  : "",
			(ptem.cflags & CBAUD) == B150  ? " b150"  : "",
			(ptem.cflags & CBAUD) == B200  ? " b200"  : "",
			(ptem.cflags & CBAUD) == B300  ? " b300"  : "",
			(ptem.cflags & CBAUD) == B600  ? " b600"  : "",
			(ptem.cflags & CBAUD) == B1200 ? " b1200" : "",
			(ptem.cflags & CBAUD) == B1800 ? " b1800" : "",
			(ptem.cflags & CBAUD) == B2400 ? " b2400" : "",
			(ptem.cflags & CBAUD) == B4800 ? " b4800" : "",
			(ptem.cflags & CBAUD) == B9600 ? " b9600" : "",
			(ptem.cflags & CBAUD) == B19200 ? " b19200" : "");
		fprintf(fp, "%s%s%s%s%s%s%s%s%s%s\n",
			(ptem.cflags & CSIZE) == CS5   ? " cs5"   : "",
			(ptem.cflags & CSIZE) == CS6   ? " cs6"   : "",
			(ptem.cflags & CSIZE) == CS7   ? " cs7"   : "",
			(ptem.cflags & CSIZE) == CS8   ? " cs8"   : "",
			(ptem.cflags & CSTOPB) ? " cstopb" : "",
			(ptem.cflags & CREAD)  ? " cread"  : "",
			(ptem.cflags & PARENB) ? " parenb" : "",
			(ptem.cflags & PARODD) ? " parodd" : "",
			(ptem.cflags & HUPCL)  ? " hupcl"  : "",
			(ptem.cflags & CLOCAL) ? " clocal" : "");

		fprintf(fp,
			"\tNumber of rows: %d\tNumber of columns: %d\n",
			ptem.wsz.ws_row, ptem.wsz.ws_col);
		fprintf(fp,
"\tNumber of horizontal pixels: %d\tNumber of vertical pixels: %d\n",
			ptem.wsz.ws_xpixel, ptem.wsz.ws_ypixel);
	}

	return (q.q_next);
}

void
prspckt(addr)
struct queue *addr;
{
	char *heading = "\tMODNAME   MODID\n";
	char mname[9];	/* Buffer for the module name */

	struct queue q;
	struct qinit qinfo;
	struct module_info minfo;

	/*
	 * ??Wade through the link-lists to extract the line disicpline
	 * name and id
	 */
	readmem(addr, 1, -1, &q, sizeof (struct queue), "");
	/*
	 * q_next is zero at the stream head, i.e. no modules have
	 *  been pushed
	 */
	if (!q.q_next)
		return;

	readmem(q.q_qinfo, 1, -1, &qinfo, sizeof (struct qinit), "");
	readmem(qinfo.qi_minfo, 1, -1, &minfo, sizeof (struct module_info), "");
	readmem(minfo.mi_idname, 1, -1, mname, 8, "");
	mname[8] = '\0';


	fprintf(fp, "%s", heading);
	fprintf(fp, "\t %-9s%6d\n", mname, minfo.mi_idnum);
}
