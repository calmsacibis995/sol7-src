/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)dmesg.c	1.7	98/02/20 SMI"

/*
 *	Suck up system messages
 *	dmesg
 *		print current buffer
 *	dmesg -
 *		print and update incremental history
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <nlist.h>
#include <kvm.h>
#include <sys/msgbuf.h>

static struct	msgbuf_hd msgbuf_hd;
static struct	msgbuf *msgbufp, *omsgbufp;
static int	msgbufsize;
static int	sflg;
static int	of	= -1;

static struct	nlist nl[2] = {
#define	NL_MSGBUF	0
	{ "msgbuf" },
	{ "" }
};

static void done(char *);
static void pdate(void);

int
main(int argc, char **argv)
{
	kvm_t *kd;
	char *namelist;
	char *mp, *mlast, *mend;
	char *mstart;
	int newmsg, sawnl, ignore;

	if (argc > 1 && argv[1][0] == '-') {
		sflg++;
		argc--;
		argv++;
	}

	namelist = (argc > 1 ? argv[1] : NULL);
	if (namelist != NULL)
		(void) setgid(getgid());
	
	kd = kvm_open(namelist, NULL, NULL, O_RDONLY, NULL);
	if (kd == NULL)
		done("Can't read kernel memory\n");

	if (kvm_nlist(kd, nl) < 0)
		done("Can't get kernel namelist\n");

	if (kvm_kread(kd, nl[NL_MSGBUF].n_value, &msgbuf_hd,
	    sizeof (msgbuf_hd)) != sizeof (msgbuf_hd))
		done("Can't read kernel memory\n");

	if (msgbuf_hd.msgh_magic != MSG_MAGIC)
		done("Magic number wrong (namelist mismatch?)\n");
	msgbufsize = sizeof (struct msgbuf_hd) + msgbuf_hd.msgh_size;
	msgbufp = malloc(msgbufsize);
	if (msgbufp == NULL)
		done("Can't allocate memory\n");

	if (kvm_kread(kd, nl[NL_MSGBUF].n_value, msgbufp,
	    msgbufsize) != msgbufsize)
		done("Can't read msgbuf\n");

	if (msgbufp->msg_bufx >= msgbuf_hd.msgh_size)
		msgbufp->msg_bufx = 0;
	newmsg = 1;
	mend = &msgbufp->msg_bufc[msgbuf_hd.msgh_size];
	if (sflg) {
		of = open("/var/adm/msgbuf", O_RDWR | O_CREAT, 0644);
		if (of < 0)
			done("Can't open /var/adm/msgbuf\n");
		omsgbufp = calloc(msgbufsize, 1);
		if (omsgbufp == NULL)
			done("Can't allocate memory\n");
		(void) read(of, omsgbufp, msgbufsize);
		(void) lseek(of, 0L, 0);
		if (omsgbufp->msg_magic == MSG_MAGIC &&
		    omsgbufp->msg_size == msgbuf_hd.msgh_size) {
			char *omp, *omend;

			if (omsgbufp->msg_bufx >= msgbuf_hd.msgh_size)
				omsgbufp->msg_bufx = 0;
			mp = &msgbufp->msg_bufc[msgbufp->msg_bufx];
			omp = &omsgbufp->msg_bufc[msgbufp->msg_bufx];
			omend = &omsgbufp->msg_bufc[msgbuf_hd.msgh_size];
			mstart = &msgbufp->msg_bufc[omsgbufp->msg_bufx];
			newmsg = 0;
			do {
				if (*mp++ != *omp++) {
					newmsg = 1;
					break;
				}
				if (mp >= mend)
					mp = &msgbufp->msg_bufc[0];
				if (omp >= omend)
					omp = &omsgbufp->msg_bufc[0];
			} while (mp != mstart);
			if (newmsg == 0 &&
			    omsgbufp->msg_bufx == msgbufp->msg_bufx)
				exit(0);
		}
		if (newmsg) {
			pdate();
			printf("...\n");
		}
	}
	pdate();
	if (newmsg)
		mstart = &msgbufp->msg_bufc[msgbufp->msg_bufx];
	sawnl = 1;
	ignore = 0;
	mp = mstart;
	mlast = &msgbufp->msg_bufc[msgbufp->msg_bufx];
	do {
		char c;

		c = *mp++;
		if (sawnl && c == '<')
			ignore = 1;
		if (c && (c & 0200) == 0 && !ignore)
			putchar(c);
		if (ignore && c == '>')
			ignore = 0;
		sawnl = (c == '\n');
		if (mp >= mend)
			mp = &msgbufp->msg_bufc[0];
	} while (mp != mlast);
	done(NULL);
	/* NOTREACHED */
	return (0);
}

static void
done(char *s)
{
	if (s) {
		pdate();
		(void) printf(s);
	} else if (of != -1) {
		(void) write(of, msgbufp, msgbufsize);
	}
	exit(s != NULL);
}

static void
pdate(void)
{
	static firstime;
	time_t tbuf;

	if (firstime == 0) {
		firstime++;
		time(&tbuf);
		printf("\n%.12s\n", ctime(&tbuf) + 4);
	}
}
