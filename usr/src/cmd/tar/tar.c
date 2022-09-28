/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* ************************************************************ */

/*
 *		PROPRIETARY NOTICE (Combined)
 *
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 *
 *
 *
 *		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * Copyright (c) 1986,1987,1988,1989,1996,1997,1998, by Sun Microsystems, Inc.
 * All rights reserved.
 *
 *	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *			All rights reserved.
 */
/*	Copyright (c) 1987, 1988 Microsoft Corporation	*/
/*	  All Rights Reserved	*/

/*	This Module contains Proprietary Information of Microsoft 	*/
/*	Corporation and should be treated as Confidential.	*/

#pragma ident   "@(#)tar.c 1.88     98/02/04 SMI"

#include <unistd.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/mkdev.h>
#include <sys/wait.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <ctype.h>
#include <locale.h>
#include <nl_types.h>
#include <langinfo.h>
#include <pwd.h>
#include <grp.h>
#include <fcntl.h>
#include <string.h>
#include <malloc.h>
#include <time.h>
#include <utime.h>
#include <stdlib.h>
#include <stdarg.h>
#include <widec.h>
#include <sys/mtio.h>
#include <libintl.h>
#include <sys/acl.h>
#include <strings.h>
#include <deflt.h>
#include <limits.h>
#include <iconv.h>
#include <assert.h>
#include "libcmd.h"		/* for defcntl */

/*
 * Compiling with -D_XPG4_2 gets this but produces other problems, so
 * instead of including sys/time.h and compiling with -D_XPG4_2, I'm
 * explicitly doing the declaration here.
 */
int utimes(const char *path, const struct timeval timeval_ptr[]);

#ifndef MINSIZE
#define	MINSIZE 250
#endif
#define	DEF_FILE "/etc/default/tar"

#define	writetape(b)	writetbuf(b, 1)
#define	min(a, b)  ((a) < (b) ? (a) : (b))
#define	max(a, b)  ((a) > (b) ? (a) : (b))

/* -DDEBUG	ONLY for debugging */
#ifdef	DEBUG
#undef	DEBUG
#define	DEBUG(a, b, c)\
	(void) fprintf(stderr, "DEBUG - "), (void) fprintf(stderr, a, b, c)
#endif

#define	TBLOCK	512	/* tape block size--should be universal */

#ifdef	BSIZE
#define	SYS_BLOCK BSIZE	/* from sys/param.h:  secondary block size */
#else	/* BSIZE */
#define	SYS_BLOCK 512	/* default if no BSIZE in param.h */
#endif	/* BSIZE */

#define	NBLOCK	20
#define	NAMSIZ	100
#define	PRESIZ	155
#define	MAXNAM	256
#define	MODEMASK 0777777	/* file creation mode mask */
#define	MAXEXT	9	/* reasonable max # extents for a file */
#define	EXTMIN	50	/* min blks left on floppy to split a file */

#define	TAR_OFFSET_MAX	077777777777	/* largest file we can archive */
					/* unless -E option is being used */
#define	OCTAL7CHAR	07777777	/* Limit for ustar gid, uid, dev */
					/* unless -E option is being used */

#define	TBLOCKS(bytes)	(((bytes) + TBLOCK - 1) / TBLOCK)
#define	K(tblocks)	((tblocks+1)/2)	/* tblocks to Kbytes for printing */
#define	max(a, b)	((a) > (b) ? (a) : (b))

#define	MAXLEV	18
#define	LEV0	1

#define	TRUE	1
#define	FALSE	0

#if _FILE_OFFSET_BITS == 64
#define	FMT_off_t "lld"
#define	FMT_off_t_o "llo"
#define	FMT_blkcnt_t "lld"
#else
#define	FMT_off_t "ld"
#define	FMT_off_t_o "lo"
#define	FMT_blkcnt_t "ld"
#endif

/* ACL support */

static
struct	sec_attr {
	char	attr_type;
	char	attr_len[7];
	char	attr_info[1];
} *attr;

/* Was statically allocated tbuf[NBLOCK] */
static
union hblock {
	char dummy[TBLOCK];
	struct header {
		char name[NAMSIZ];	/* If non-null prefix, path is	*/
					/* <prefix>/<name>;  otherwise	*/
					/* <name>			*/
		char mode[8];
		char uid[8];
		char gid[8];
		char size[12];		/* size of this extent if file split */
		char mtime[12];
		char chksum[8];
		char typeflag;
		char linkname[NAMSIZ];
		char magic[6];
		char version[2];
		char uname[32];
		char gname[32];
		char devmajor[8];
		char devminor[8];
		char prefix[PRESIZ];	/* Together with "name", the path of */
					/* the file:  <prefix>/<name>	*/
		char extno;		/* extent #, null if not split */
		char extotal;		/* total extents */
		char efsize[10];	/* size of entire file */
	} dbuf;
} dblock, *tbuf, xhdr_buf;

static
struct xtar_hdr {
	uid_t		x_uid,		/* Uid of file */
			x_gid;		/* Gid of file */
	major_t		x_devmajor;	/* Device major node */
	minor_t		x_devminor;	/* Device minor node */
	off_t		x_filesz;	/* Length of file */
	char		*x_uname,	/* Pointer to name of user */
			*x_gname,	/* Pointer to gid of user */
			*x_linkpath,	/* Path for a hard/symbolic link */
			*x_path;	/* Path of file */
	timestruc_t	x_mtime;	/* Seconds and nanoseconds */
} Xtarhdr;

static
struct gen_hdr {
	ulong		g_mode;		/* Mode of file */
	uid_t		g_uid,		/* Uid of file */
			g_gid;		/* Gid of file */
	off_t		g_filesz;	/* Length of file */
	time_t		g_mtime;	/* Modification time */
	uint_t		g_cksum;	/* Checksum of file */
	ulong		g_devmajor,	/* File system of file */
			g_devminor;	/* Major/minor of special files */
} Gen;

static
struct linkbuf {
	ino_t	inum;
	dev_t	devnum;
	int	count;
	char	pathname[MAXNAM+1];	/* added 1 for last NULL */
	struct	linkbuf *nextp;
} *ihead;

/* see comments before build_table() */
#define	TABLE_SIZE 512
struct	file_list	{
	char	*name;			/* Name of file to {in,ex}clude */
	struct	file_list	*next;	/* Linked list */
};
static	struct	file_list	*exclude_tbl[TABLE_SIZE],
				*include_tbl[TABLE_SIZE];

static int	append_secattr(char **, int *, int, aclent_t *, char);
static void	write_ancillary(union hblock *, char *, int);

static void add_file_to_table(struct file_list *table[], char *str);
static void assert_string(char *s, char *msg);
static int istape(int fd, int type);
static void backtape(void);
static void build_table(struct file_list *table[], char *file);
static void check_prefix(char **namep);
static void closevol(void);
static void copy(void *dst, void *src);
static void delete_target(char *namep);
static void doDirTimes(char *name, timestruc_t modTime);
static void done(int n);
static void dorep(char *argv[]);
#ifdef	_iBCS2
static void dotable(char *argv[], int cnt);
static void doxtract(char *argv[], int cnt);
#else
static void dotable(char *argv[]);
static void doxtract(char *argv[]);
#endif
static void fatal(char *format, ...);
static void vperror(int exit_status, char *fmt, ...);
static void flushtape(void);
static void getdir(void);
static void longt(struct stat *st, char aclchar);
static int makeDir(char *name);
static void mterr(char *operation, int i, int exitcode);
static void newvol(void);
static void passtape(void);
static void putempty(blkcnt_t n);
static void putfile(char *longname, char *shortname, char *parent, int lev);
static void readtape(char *buffer);
static void seekdisk(blkcnt_t blocks);
static void setPathTimes(char *path, timestruc_t modTime);
static void splitfile(char *longname, int ifd);
static void tomodes(struct stat *sp);
static void usage(void);
static void xblocks(off_t bytes, int ofile);
static void xsfile(int ofd);
static void resugname(char *name, int symflag);
static int bcheck(char *bstr);
static int checkdir(char *name);
static int checksum(union hblock *dblockp);
#ifdef	EUC
static int checksum_signed(union hblock *dblockp);
#endif	/* EUC */
static int checkupdate(char *arg);
static int checkw(char c, char *name);
static int cmp(char *b, char *s, int n);
static int defset(char *arch);
static int endtape(void);
static int is_in_table(struct file_list *table[], char *str);
static int notsame(void);
static int is_prefix(char *s1, char *s2);
static int response(void);
static int build_dblock(const char *, const char *, const char,
    const struct stat *, const dev_t, const char *);
static wchar_t yesnoresponse(void);
static unsigned int hash(char *str);

#ifdef	_iBCS2
static void initarg(char *argv[], char *file);
static char *nextarg();
#endif
static blkcnt_t kcheck(char *kstr);
static off_t bsrch(char *s, int n, off_t l, off_t h);
static void onintr(int sig);
static void onquit(int sig);
static void onhup(int sig);
static uid_t getuidbyname(char *);
static gid_t getgidbyname(char *);
static char *getname(gid_t);
static char *getgroup(gid_t);
static int checkf(char *name, int mode, int howmuch);
static int writetbuf(char *buffer, int n);
static int wantit(char *argv[], char **namep);

static int get_xdata(void);
static void gen_num(const char *keyword, const u_longlong_t number);
static void gen_date(const char *keyword, const timestruc_t time_value);
static void gen_string(const char *keyword, const char *value);
static void get_xtime(char *value, timestruc_t *xtime);
static int chk_path_build(char *name, char *longname, char *linkname,
    char *prefix, char type);
static int gen_utf8_names(const char *filename);
static int utf8_local(char *option, char **Xhdr_ptrptr, char *target,
    const char *src, int max_val);
static int local_utf8(char **Xhdr_ptrptr, char *target, const char *src,
    iconv_t iconv_cd, int xhdrflg, int max_val);
static int c_utf8(char *target, const char *source);

static	struct stat stbuf;

static	int	checkflag = 0;
#ifdef	_iBCS2
static	int	Fileflag;
char    *sysv3_env;
#endif
static	int	Xflag, Fflag, iflag, hflag, Bflag, Iflag;
static	int	rflag, xflag, vflag, tflag, mt, cflag, mflag, pflag;
static	int	uflag;
static	int	eflag, errflag, qflag;
static	int	oflag;
static	int	bflag, kflag, Aflag;
static 	int	Pflag;			/* POSIX conformant archive */
static	int	Eflag;			/* Allow files greater than 8GB */
static	int	term, chksum, wflag,
		first = TRUE, defaults_used = FALSE, linkerrok;
static	blkcnt_t	recno;
static	int	freemem = 1;
static	int	nblock = NBLOCK;
static	int	Errflg = 0;

static	dev_t	mt_dev;		/* device containing output file */
static	ino_t	mt_ino;		/* inode number of output file */
static	int	mt_devtype;	/* dev type of archive, from stat structure */

static	int update = 1;		/* for `open' call */

static	off_t	low;
static	off_t	high;

static	FILE	*tfile;
static	FILE	*vfile = stdout;
static	char	tname[] = "/tmp/tarXXXXXX";
static	char	archive[] = "archive0=";
static	char	*Xfile;
static	char	*usefile;
static	char	*Filefile;

static	int	mulvol;		/* multi-volume option selected */
static	blkcnt_t	blocklim; /* number of blocks to accept per volume */
static	blkcnt_t	tapepos; /* current block number to be written */
static	int	NotTape;	/* true if tape is a disk */
static	int	dumping;	/* true if writing a tape or other archive */
static	int	extno;		/* number of extent:  starts at 1 */
static	int	extotal;	/* total extents in this file */
static	off_t	efsize;		/* size of entire file */
static	ushort	Oumask = 0;	/* old umask value */
static 	int is_posix;	/* true if archive we're reading is POSIX-conformant */
static	const	char	*magic_type = "ustar";
static	size_t	xrec_size = 8 * PATH_MAX;	/* extended rec initial size */
static	char	*xrec_ptr;
static	off_t	xrec_offset = 0;
static	int	Xhdrflag;
static	int	charset_type = 0;

static	u_longlong_t	xhdr_flgs;	/* Bits set determine which items */
					/*   need to be in extended header. */
#define	_X_DEVMAJOR	0x1
#define	_X_DEVMINOR	0x2
#define	_X_GID		0x4
#define	_X_GNAME	0x8
#define	_X_LINKPATH	0x10
#define	_X_PATH		0x20
#define	_X_SIZE		0x40
#define	_X_UID		0x80
#define	_X_UNAME	0x100
#define	_X_ATIME	0x200
#define	_X_CTIME	0x400
#define	_X_MTIME	0x800
#define	_X_LAST		0x40000000

#define	PID_MAX_DIGITS		(10 * sizeof (pid_t) / 4)
#define	TIME_MAX_DIGITS		(10 * sizeof (time_t) / 4)
#define	LONG_MAX_DIGITS		(10 * sizeof (long) / 4)
#define	ULONGLONG_MAX_DIGITS	(10 * sizeof (u_longlong_t) / 4)
/*
 * UTF_8 encoding requires more space than the current codeset equivalent.
 * Currently a factor of 2-3 would suffice, but it is possible for a factor
 * of 6 to be needed in the future, so for saftey, we use that here.
 */
#define	UTF_8_FACTOR	6

static	u_longlong_t	xhdr_count = 0;
static char		xhdr_dirname[PRESIZ + 1];
static char		pidchars[PID_MAX_DIGITS + 1];
static char		*tchar = "";		/* null linkpath */
static aclent_t		*aclp;

static	char	local_path[UTF_8_FACTOR * PATH_MAX + 1];
static	char	local_linkpath[UTF_8_FACTOR * PATH_MAX + 1];
static	char	local_gname[UTF_8_FACTOR * _POSIX_NAME_MAX + 1];
static	char	local_uname[UTF_8_FACTOR * _POSIX_NAME_MAX + 1];

/*
 * For debugging of extended header, force extended header fields to be
 * produced with smaller values than normal.
 */
#ifdef XHDR_DEBUG
#define	TAR_OFFSET_MAX 9
#define	OCTAL7CHAR 2
#endif

int
main(int argc, char *argv[])
{
	char		*cp;
	char		*tmpdirp;
	pid_t		thispid;
#ifdef XENIX_ONLY
#if SYS_BLOCK > TBLOCK
	struct stat statinfo;
#endif
#endif

#ifdef	_iBCS2
	int	tbl_cnt = 0;
	sysv3_env = getenv("SYSV3");
#endif
	(void) setlocale(LC_ALL, "");
#if !defined(TEXT_DOMAIN)	/* Should be defined by cc -D */
#define	TEXT_DOMAIN "SYS_TEST"	/* Use this only if it weren't */
#endif
	(void) textdomain(TEXT_DOMAIN);
	if (argc < 2)
		usage();

	tfile = NULL;

	/*
	 *  For XPG4 compatibility, we must be able to accept the "--"
	 *  argument normally recognized by getopt; it is used to delimit
	 *  the end opt the options section, and so can only appear in
	 *  the position of the first argument.  We simply skip it.
	 */

	if (strcmp(argv[1], "--") == 0) {
		argv++;
		argc--;
	}

	argv[argc] = NULL;
	argv++;

	/*
	 * Set up default values.
	 * Search the option string looking for the first digit or an 'f'.
	 * If you find a digit, use the 'archive#' entry in DEF_FILE.
	 * If 'f' is given, bypass looking in DEF_FILE altogether.
	 * If no digit or 'f' is given, still look in DEF_FILE but use '0'.
	 */
	if ((usefile = getenv("TAPE")) == (char *)NULL) {
		for (cp = *argv; *cp; ++cp)
			if (isdigit(*cp) || *cp == 'f')
				break;
		if (*cp != 'f') {
			archive[7] = (*cp)? *cp: '0';
			if (!(defaults_used = defset(archive))) {
				usefile = NULL;
				nblock = 1;
				blocklim = 0;
				NotTape = 0;
			}
		}
	}

	for (cp = *argv++; *cp; cp++)
		switch (*cp) {
		case 'f':
			assert_string(*argv, gettext(
			"tar: tapefile must be specified with 'f' option\n"));
			usefile = *argv++;
			break;
		case 'F':
#ifdef	_iBCS2
			if (sysv3_env) {
				assert_string(*argv, gettext(
					"tar: 'F' requires a file name\n"));
				Filefile = *argv++;
				Fileflag++;
			} else
#endif	/*  _iBCS2 */
				Fflag++;
			break;
		case 'c':
			cflag++;
			rflag++;
			update = 1;
			break;
		case 'u':
			uflag++;	/* moved code after signals caught */
			rflag++;
			update = 2;
			break;
		case 'r':
			rflag++;
			update = 2;
			break;
		case 'v':
			vflag++;
			break;
		case 'w':
			wflag++;
			break;
		case 'x':
			xflag++;
			break;
		case 'X':
			assert_string(*argv, gettext(
			    "tar: exclude file must be specified with 'X' "
			    "option\n"));
			Xflag = 1;
			Xfile = *argv++;
			build_table(exclude_tbl, Xfile);
			break;
		case 't':
			tflag++;
			break;
		case 'm':
			mflag++;
			break;
		case 'p':
			pflag++;
			break;
		case '-':
			/* ignore this silently */
			break;
		case '0':	/* numeric entries used only for defaults */
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
			break;
		case 'b':
			assert_string(*argv, gettext(
			    "tar: blocking factor must be specified "
			    "with 'b' option\n"));
			bflag++;
			nblock = bcheck(*argv++);
			break;
		case 'q':
			qflag++;
			break;
		case 'k':
			assert_string(*argv, gettext(
			    "tar: size value must be specified with 'k' "
			    "option\n"));
			kflag++;
			blocklim = kcheck(*argv++);
			break;
		case 'n':		/* not a magtape (instead of 'k') */
			NotTape++;	/* assume non-magtape */
			break;
		case 'l':
			linkerrok++;
			break;
		case 'e':
#ifdef	_iBCS2
			/* If sysv3 IS set, don't be as verbose */
			if (!sysv3_env)
#endif	/* _iBCS2 */
				errflag++;
			eflag++;
			break;
		case 'o':
			oflag++;
			break;
		case 'h':
			hflag++;
			break;
		case 'i':
			iflag++;
			break;
		case 'B':
			Bflag++;
			break;
		case 'P':
			Pflag++;
			break;
		case 'E':
			Eflag++;
			Pflag++;	/* Only POSIX archive made */
			break;
		default:
			(void) fprintf(stderr, gettext(
			"tar: %c: unknown option\n"), *cp);
			usage();
		}

#ifdef	_iBCS2
	if (Xflag && Fileflag) {
		(void) fprintf(stderr, gettext(
		"tar: specify only one of X or F.\n"));
		usage();
	}
#endif	/*  _iBCS2 */

	if (!rflag && !xflag && !tflag)
		usage();
	if ((rflag && xflag) || (xflag && tflag) || (rflag && tflag)) {
		(void) fprintf(stderr, gettext(
		"tar: specify only one of [ctxru].\n"));
		usage();
	}
	if (cflag && *argv == NULL && Filefile == NULL)
		fatal(gettext("Missing filenames"));
	if (usefile == NULL)
		fatal(gettext("device argument required"));

	/* alloc a buffer of the right size */
	if ((tbuf = (union hblock *)
		    calloc(sizeof (union hblock) * nblock, sizeof (char))) ==
		(union hblock *) NULL) {
		(void) fprintf(stderr, gettext(
		"tar: cannot allocate physio buffer\n"));
		exit(1);
	}

	if ((xrec_ptr = malloc(xrec_size)) == NULL) {
		(void) fprintf(stderr, gettext(
		    "tar: cannot allocate extended header buffer\n"));
		exit(1);
	}


#ifdef XENIX_ONLY
#if SYS_BLOCK > TBLOCK
	/* if user gave blocksize for non-tape device check integrity */
	(void) fprintf(stderr,
	    "SYS_BLOCK == %d, TBLOCK == %d\n", SYS_BLOCK, TBLOCK);
	if (cflag &&			/* check only needed when writing */
	    NotTape &&
	    stat(usefile, &statinfo) >= 0 &&
	    ((statinfo.st_mode & S_IFMT) == S_IFCHR) &&
	    (nblock % (SYS_BLOCK / TBLOCK)) != 0)
		fatal(gettext(
		"blocksize must be multiple of %d."), SYS_BLOCK/TBLOCK);
#endif
#endif

	thispid = getpid();
	(void) sprintf(pidchars, "%ld", thispid);
	thispid = strlen(pidchars);

	if ((tmpdirp = getenv("TMPDIR")) == (char *)NULL)
		strcpy(xhdr_dirname, "/tmp");
	else {
		/*
		 * Make sure that dir is no longer than what can
		 * fit in the prefix part of the header.
		 */
		if (strlen(tmpdirp) > (size_t)(PRESIZ - thispid - 12)) {
			strcpy(xhdr_dirname, "/tmp");
			if ((vflag > 0) && (Eflag > 0))
				(void) fprintf(stderr, gettext(
				    "Ignoring TMPDIR\n"));
		} else
			strcpy(xhdr_dirname, tmpdirp);
	}
	(void) strcat(xhdr_dirname, "/PaxHeaders.");
	(void) strcat(xhdr_dirname, pidchars);

	if (rflag) {
		if (cflag && tfile != NULL)
			usage();
		if (signal(SIGINT, SIG_IGN) != SIG_IGN)
			(void) signal(SIGINT, onintr);
		if (signal(SIGHUP, SIG_IGN) != SIG_IGN)
			(void) signal(SIGHUP, onhup);
		if (signal(SIGQUIT, SIG_IGN) != SIG_IGN)
			(void) signal(SIGQUIT, onquit);
		if (uflag) {
			(void) mktemp(tname);
			if ((tfile = fopen(tname, "w")) == NULL)
				vperror(1, "%s", tname);
		}
		if (strcmp(usefile, "-") == 0) {
			if (cflag == 0)
				fatal(gettext(
				"can only create standard output archives."));
			vfile = stderr;
			mt = dup(1);
			++bflag;
		} else {
			if (cflag)
				mt = open(usefile,
				    O_RDWR|O_CREAT|O_TRUNC, 0666);
			else
				mt = open(usefile, O_RDWR);

			if (mt < 0) {
				if (cflag == 0 || (mt =  creat(usefile, 0666))
						< 0)
				vperror(1, "%s", usefile);
			}
		}
		/* Get inode and device number of output file */
		(void) fstat(mt, &stbuf);
		mt_ino = stbuf.st_ino;
		mt_dev = stbuf.st_dev;
		mt_devtype = stbuf.st_mode & S_IFMT;
		NotTape = !istape(mt, mt_devtype);

		if (rflag && !cflag && (mt_devtype == S_IFIFO))
			fatal(gettext("cannot append to pipe or FIFO."));

		if (Aflag && vflag)
			(void) printf(
			gettext("Suppressing absolute pathnames\n"));
		dorep(argv);
	} else if (xflag || tflag) {
		/*
		 * for each argument, check to see if there is a "-I file" pair.
		 * if so, move the 3rd argument into "-I"'s place, build_table()
		 * using "file"'s name and increment argc one (the second
		 * increment appears in the for loop) which removes the two
		 * args "-I" and "file" from the argument vector.
		 */
		for (argc = 0; argv[argc]; argc++) {
			if (strcmp(argv[argc], "-I") == 0) {
				if (!argv[argc+1]) {
					(void) fprintf(stderr, gettext(
					"tar: missing argument for -I flag\n"));
					done(2);
				} else {
					Iflag = 1;
					argv[argc] = argv[argc+2];
					build_table(include_tbl, argv[++argc]);
#ifdef	_iBCS2
					if (Fileflag) {
						(void) fprintf(stderr, gettext(
						"tar: only one of I or F.\n"));
						usage();
					}
#endif	/*  _iBCS2 */

				}
			}
		}
		if (strcmp(usefile, "-") == 0) {
			mt = dup(0);
			++bflag;
			/* try to recover from short reads when reading stdin */
			++Bflag;
		} else if ((mt = open(usefile, 0)) < 0)
			vperror(1, "%s", usefile);

		if (xflag) {
			if (Aflag && vflag)
				(void) printf(gettext
				("Suppressing absolute pathnames.\n"));

#ifdef	_iBCS2
			doxtract(argv, tbl_cnt);
#else
			doxtract(argv);
#endif
		} else if (tflag)

#ifdef	_iBCS2
			dotable(argv, tbl_cnt);
#else
			dotable(argv);
#endif
	}
	else
		usage();

	done(Errflg);

	/* Not reached:  keep compiler quiet */
	return (1);
}

static void
usage(void)
{

#ifdef	_iBCS2
	if (sysv3_env) {
		(void) fprintf(stderr, gettext(
		"Usage: tar {txruc}[vfbXhiBEelmopwnq[0-7]] [-k size] "
		"[-F filename] [tapefile] [blocksize] [exclude-file] "
		"[-I include-file] files ...\n"));
	} else
#endif	/* _iBCS2 */
	{
		(void) fprintf(stderr, gettext(
		"Usage: tar {txruc}[vfbFXhiBEelmopwnq[0-7]] [-k size] "
		"[tapefile] [blocksize] [exclude-file] "
		"[-I include-file] files ...\n"));
	}
	done(1);
}

/*
 * dorep - do "replacements"
 *
 *	Dorep is responsible for creating ('c'),  appending ('r')
 *	and updating ('u');
 */

static void
dorep(char *argv[])
{
	char *cp, *cp2, *p;
	char wdir[PATH_MAX+2], tempdir[PATH_MAX+2], *parent;
	char file[PATH_MAX], origdir[PATH_MAX+1];
	FILE *fp = (FILE *)NULL;
	FILE *ff = (FILE *)NULL;


	if (!cflag) {
		xhdr_flgs = 0;
		getdir();			/* read header for next file */
		if (Xhdrflag > 0) {
			if (!Eflag)
				fatal(gettext("Archive contains extended"
				    " header.  -E flag required.\n"));
			(void) get_xdata();	/* Get extended header items */
						/*   and regular header */
		} else {
			if (Eflag)
				fatal(gettext("Archive contains no extended"
				    " header.  -E flag not allowed.\n"));
		}
		while (!endtape()) {		/* changed from a do while */
			passtape();		/* skip the file data */
			if (term)
				done(Errflg);	/* received signal to stop */
			xhdr_flgs = 0;
			getdir();
			if (Xhdrflag > 0)
				(void) get_xdata();
		}
		backtape();			/* was called by endtape */
		if (tfile != NULL) {
			char buf[200];

			(void) sprintf(buf, "sort +0 -1 +1nr %s -o %s; awk '$1 "
			    "!= prev {print; prev=$1}' %s >%sX;mv %sX %s",
				tname, tname, tname, tname, tname, tname);
			(void) fflush(tfile);
			(void) system(buf);
			(void) freopen(tname, "r", tfile);
			(void) fstat(fileno(tfile), &stbuf);
			high = stbuf.st_size;
		}
	}

	dumping = 1;
	if (mulvol) {	/* SP-1 */
		if (nblock && (blocklim%nblock) != 0)
			fatal(gettext(
			"Volume size not a multiple of block size."));
		blocklim -= 2;			/* for trailer records */
		if (vflag)
			(void) fprintf(vfile, gettext("Volume ends at %"
			    FMT_blkcnt_t "K, blocking factor = %dK\n"),
			    K((blocklim - 1)), K(nblock));
	}

#ifdef	_iBCS2
	if (Fileflag) {
		if (Filefile != NULL) {
			if ((ff = fopen(Filefile, "r")) == NULL)
				vperror(0, "%s", Filefile);
		} else {
			(void) fprintf(stderr, gettext(
			    "tar: F requires a file name.\n"));
			usage();
		}
	}
#endif	/*  _iBCS2 */

	/*
	 * Save the original directory before it gets
	 * changed.
	 */
	if (getcwd(origdir, (PATH_MAX+1)) == NULL) {
		vperror(0, gettext("A parent directory cannot be read"));
		exit(1);
	}

	strcpy(wdir, origdir);

	while ((*argv || fp || ff) && !term) {
		if (fp || (strcmp(*argv, "-I") == 0)) {
#ifdef	_iBCS2
			if (Fileflag) {
				(void) fprintf(stderr, gettext(
				"tar: only one of I or F.\n"));
				usage();
			}
#endif	/*  _iBCS2 */
			if (fp == NULL) {
				if (*++argv == NULL)
					fatal(gettext(
					    "missing file name for -I flag."));
				else if ((fp = fopen(*argv++, "r")) == NULL)
					vperror(0, "%s", argv[-1]);
				continue;
			} else if ((fgets(file, PATH_MAX-1, fp)) == NULL) {
				(void) fclose(fp);
				fp = NULL;
				continue;
			} else {
				cp = cp2 = file;
				if ((p = strchr(cp2, '\n')))
					*p = 0;
			}
		} else if ((strcmp(*argv, "-C") == 0) && argv[1]) {
#ifdef	_iBCS2
			if (Fileflag) {
				(void) fprintf(stderr, gettext(
				"tar: only one of F or C\n"));
				usage();
			}
#endif	/*  _iBCS2 */

			if (chdir(*++argv) < 0)
				vperror(0, gettext(
				"can't change directories to %s"), *argv);
			else
				(void) getcwd(wdir, (sizeof (wdir)));
			argv++;
			continue;
#ifdef	_iBCS2
		} else if (Fileflag && (ff != NULL)) {
			if ((fgets(file, PATH_MAX-1, ff)) == NULL) {
				(void) fclose(ff);
				ff = NULL;
				continue;
			} else {
				cp = cp2 = file;
				if (p = strchr(cp2, '\n'))
					*p = 0;
			}
#endif	/*  _iBCS2 */
		} else
			cp = cp2 = strcpy(file, *argv++);

		parent = wdir;
		for (; *cp; cp++)
			if (*cp == '/')
				cp2 = cp;
		if (cp2 != file) {
			*cp2 = '\0';
			if (chdir(file) < 0) {
				vperror(0, gettext(
				"can't change directories to %s"), file);
				continue;
			}
			parent = getcwd(tempdir, (sizeof (tempdir)));
			*cp2 = '/';
			cp2++;
		}

		putfile(file, cp2, parent, LEV0);
		if (chdir(origdir) < 0)
			vperror(0, gettext("cannot change back?: %s"), origdir);
	}
	putempty((blkcnt_t)2);
	flushtape();
	closevol();	/* SP-1 */
	if (linkerrok == 1)
		for (; ihead != NULL; ihead = ihead->nextp) {
			if (ihead->count == 0)
				continue;
			(void) fprintf(stderr, gettext(
			"tar: missing links to %s\n"), ihead->pathname);
			if (errflag)
				done(1);
		}
}


/*
 * endtape - check for tape at end
 *
 *	endtape checks the entry in dblock.dbuf to see if its the
 *	special EOT entry.  Endtape is usually called after getdir().
 *
 *	endtape used to call backtape; it no longer does, he who
 *	wants it backed up must call backtape himself
 *	RETURNS:	0 if not EOT, tape position unaffected
 *			1 if	 EOT, tape position unaffected
 */

static int
endtape(void)
{
	if (dblock.dbuf.name[0] == '\0') {	/* null header = EOT */
		return (1);
	} else
		return (0);
}

/*
 *	getdir - get directory entry from tar tape
 *
 *	getdir reads the next tarblock off the tape and cracks
 *	it as a directory. The checksum must match properly.
 *
 *	If tfile is non-null getdir writes the file name and mod date
 *	to tfile.
 */

static void
getdir(void)
{
	struct stat *sp;
#ifdef EUC
	static int warn_chksum_sign = 0;
#endif /* EUC */

top:
	readtape((char *) &dblock);
	if (dblock.dbuf.name[0] == '\0')
		return;
	sp = &stbuf;
	(void) sscanf(dblock.dbuf.mode, "%8lo", &Gen.g_mode);
	(void) sscanf(dblock.dbuf.uid, "%8lo", (ulong *)&Gen.g_uid);
	(void) sscanf(dblock.dbuf.gid, "%8lo", (ulong *)&Gen.g_gid);
	(void) sscanf(dblock.dbuf.size, "%12" FMT_off_t_o, &Gen.g_filesz);
	(void) sscanf(dblock.dbuf.mtime, "%12lo", (ulong *)&Gen.g_mtime);
	(void) sscanf(dblock.dbuf.chksum, "%8o", &Gen.g_cksum);
	(void) sscanf(dblock.dbuf.devmajor, "%8lo", &Gen.g_devmajor);
	(void) sscanf(dblock.dbuf.devminor, "%8lo", &Gen.g_devminor);

	is_posix = (strcmp(dblock.dbuf.magic, magic_type) == 0);

	sp->st_mode = Gen.g_mode;
	if (is_posix && (sp->st_mode & S_IFMT) == 0)
		switch (dblock.dbuf.typeflag) {
		case '0': case 0: case '7':
			sp->st_mode |= S_IFREG;
			break;
		case '1':	/* hard link */
			break;
		case '2':
			sp->st_mode |= S_IFLNK;
			break;
		case '3':
			sp->st_mode |= S_IFCHR;
			break;
		case '4':
			sp->st_mode |= S_IFBLK;
			break;
		case '5':
			sp->st_mode |= S_IFDIR;
			break;
		case '6':
			sp->st_mode |= S_IFIFO;
			break;
		default:
			break;
		}

	if (dblock.dbuf.typeflag == 'X')
		Xhdrflag = 1;	/* Currently processing extended header */
	else
		Xhdrflag = 0;

	sp->st_uid = Gen.g_uid;
	sp->st_gid = Gen.g_gid;
	sp->st_size = Gen.g_filesz;
	sp->st_mtime = Gen.g_mtime;
	chksum = Gen.g_cksum;

	if (dblock.dbuf.extno != '\0') {	/* split file? */
		extno = dblock.dbuf.extno;
		extotal = dblock.dbuf.extotal;
		(void) sscanf(dblock.dbuf.efsize, "%10" FMT_off_t_o, &efsize);
	} else
		extno = 0;	/* tell others file not split */

#ifdef	EUC
	if (chksum != checksum(&dblock)) {
		if (chksum != checksum_signed(&dblock)) {
			(void) fprintf(stderr, gettext(
			    "tar: directory checksum error\n"));
			if (iflag)
				goto top;
			done(2);
		} else {
			if (! warn_chksum_sign) {
				warn_chksum_sign = 1;
				(void) fprintf(stderr, gettext(
			"tar: warning: tar file made with signed checksum\n"));
			}
		}
	}
#else
	if (chksum != checksum(&dblock)) {
		(void) fprintf(stderr, gettext(
		"tar: directory checksum error\n"));
		if (iflag)
			goto top;
		done(2);
	}
#endif	/* EUC */
	if (tfile != NULL && Xhdrflag == 0) {
		/*
		 * If an extended header is present, then time is available
		 * in nanoseconds in the extended header data, so set it.
		 * Otherwise, give an invalid value so that checkupdate will
		 * not test beyond seconds.
		 */
		if ((xhdr_flgs & _X_MTIME))
			sp->st_mtim.tv_nsec = Xtarhdr.x_mtime.tv_nsec;
		else
			sp->st_mtim.tv_nsec = -1;

		if (xhdr_flgs & _X_PATH)
			(void) fprintf(tfile, "%s %10ld.%9.9ld\n",
			    Xtarhdr.x_path, sp->st_mtim.tv_sec,
			    sp->st_mtim.tv_nsec);
		else
			(void) fprintf(tfile, "%.*s %10ld.%9.9ld\n",
			    NAMSIZ, dblock.dbuf.name, sp->st_mtim.tv_sec,
			    sp->st_mtim.tv_nsec);
	}
}


/*
 *	passtape - skip over a file on the tape
 *
 *	passtape skips over the next data file on the tape.
 *	The tape directory entry must be in dblock.dbuf. This
 *	routine just eats the number of blocks computed from the
 *	directory size entry; the tape must be (logically) positioned
 *	right after thee directory info.
 */

static void
passtape(void)
{
	blkcnt_t blocks;
	char buf[TBLOCK];

	/*
	 * Types link(1), sym-link(2), char special(3), blk special(4),
	 *  directory(5), and FIFO(6) do not have data blocks associated
	 *  with them so just skip reading the data block.
	 */
	if (dblock.dbuf.typeflag == '1' || dblock.dbuf.typeflag == '2' ||
		dblock.dbuf.typeflag == '3' || dblock.dbuf.typeflag == '4' ||
		dblock.dbuf.typeflag == '5' || dblock.dbuf.typeflag == '6')
		return;
	blocks = TBLOCKS(stbuf.st_size);

	/* if operating on disk, seek instead of reading */
	if (NotTape)
		seekdisk(blocks);
	else
		while (blocks-- > 0)
			readtape(buf);
}

static void
putfile(char *longname, char *shortname, char *parent, int lev)
{
	static void *getmem(size_t);
	int infile = -1;	/* deliberately invalid */
	blkcnt_t blocks;
	char buf[PATH_MAX + 2];	/* Add trailing slash and null */
	char *bigbuf;
	int	maxread;
	int	hint;		/* amount to write to get "in sync" */
	char filetmp[PATH_MAX + 1];
	char *cp;
	char *name;
	struct dirent *dp;
	DIR *dirp;
	int i;
	long l;
	int split;
	char newparent[PATH_MAX + MAXNAMLEN + 1];
	char *prefix = "";
	char *tmpbuf;
	char goodbuf[PRESIZ + 2];
	char junkbuf[MAXNAM+1];
	char *lastslash;
	int	j;
	int	printerr;
	int	slnkerr;
	struct stat symlnbuf;
	int		aclcnt;
	int		readlink_max;

	memset(goodbuf, '\0', sizeof (goodbuf));
	memset(junkbuf, '\0', sizeof (junkbuf));

	xhdr_flgs = 0;
	aclp = NULL;

	if (lev >= MAXLEV) {
		/*
		 * Notice that we have already recursed, so we have already
		 * allocated our frame, so things would in fact work for this
		 * level.  We put the check here rather than before each
		 * recursive call because it is cleaner and less error prone.
		 */
		(void) fprintf(stderr, gettext(
		"tar: directory nesting too deep, %s not dumped\n"), longname);
		return;
	}
	if (!hflag)
		i = lstat(shortname, &stbuf);
	else
		i = stat(shortname, &stbuf);

	if (i < 0) {
		/* Initialize flag to print error mesg. */
		printerr = 1;
		/*
		 * If stat is done, then need to do lstat
		 * to determine whether it's a sym link
		 */
		if (hflag) {
			/* Save returned error */
			slnkerr = errno;

			j = lstat(shortname, &symlnbuf);
			/*
			 * Suppress error message when file
			 * is a symbolic link and option -l
			 * is on.
			 */
			if ((j == 0) && (!linkerrok) &&
				(S_ISLNK(symlnbuf.st_mode)))
				printerr = 0;

			/*
			 * Restore errno in case the lstat
			 * on symbolic link change
			 */
			errno = slnkerr;
		}

		if (printerr) {
			(void) fprintf(stderr, gettext(
			"tar: %s: %s\n"), longname, strerror(errno));
			Errflg = 1;
		}
		return;
	}

	/*
	 * Check if the input file is the same as the tar file we
	 * are creating
	 */
	if ((mt_ino == stbuf.st_ino) && (mt_dev == stbuf.st_dev)) {
		(void) fprintf(stderr, gettext(
		"tar: %s same as archive file\n"), longname);
		Errflg = 1;
		return;
	}
	/*
	 * Check size limit - we can't archive files that
	 * exceed TAR_OFFSET_MAX bytes because of header
	 * limitations.
	 */
	if ((stbuf.st_size > (off_t)TAR_OFFSET_MAX) && (Eflag == 0)) {
		if (vflag) {
			(void) fprintf(vfile, gettext(
				"a %s too large to archive\n"),
			    longname);
		}
		Errflg = 1;
		return;
	}

	if (tfile != NULL && checkupdate(longname) == 0) {
		return;
	}
	if (checkw('r', longname) == 0) {
		return;
	}

	if (Fflag && checkf(shortname, stbuf.st_mode, Fflag) == 0)
		return;

	if (Xflag) {
		if (is_in_table(exclude_tbl, longname)) {
			if (vflag) {
				(void) fprintf(vfile, gettext(
				"a %s excluded\n"), longname);
			}
			return;
		}
	}

	/*
	 * If the length of the fullname is greater than MAXNAM,
	 * print out a message and return (unless extended headers are used,
	 * in which case fullname is limited to PATH_MAX).
	 */

	if ((((split = (int) strlen(longname)) > MAXNAM) && (Eflag == 0)) ||
	    (split > PATH_MAX)) {
		(void) fprintf(stderr, gettext(
		    "tar: %s: file name too long\n"), longname);
		if (errflag)
			done(1);
		return;
	} else if ((split > NAMSIZ) || (split == NAMSIZ && (stbuf.st_mode
	    & S_IFDIR) && !Pflag)) {
		/*
		 * The length of the fullname is greater than NAMSIZ, so
		 * we must split the filename from the path.
		 * Since path is limited to PRESIZ characters, look for the
		 * last slash within PRESIZ + 1 characters only.
		 */
		(void) strncpy(&goodbuf[0], longname, min(split, PRESIZ + 1));
		tmpbuf = goodbuf;
		lastslash = strrchr(tmpbuf, '/');
		if (lastslash == NULL) {
			i = split;		/* Length of name */
			j = 0;			/* Length of prefix */
			goodbuf[0] = '\0';
		} else {
			*lastslash = '\0';	/* Terminate the prefix */
			j = strlen(tmpbuf);
			i = split - j - 1;
		}
		/*
		 * If the filename is greater than NAMSIZ we can't
		 * archive the file unless we are using extended headers.
		 */
		if ((i > NAMSIZ) || (i == NAMSIZ && (stbuf.st_mode & S_IFDIR) &&
		    !Pflag)) {
			/* Determine which (filename or path) is too long. */
			lastslash = strrchr(longname, '/');
			if (lastslash != NULL)
				i = strlen(lastslash + 1);
			if (Eflag > 0) {
				xhdr_flgs |= _X_PATH;
				Xtarhdr.x_path = longname;
				if (i <= NAMSIZ)
					(void) strcpy(junkbuf, lastslash + 1);
				else
					(void) sprintf(junkbuf, "%llu",
					    xhdr_count + 1);
				if (split - i - 1 > PRESIZ)
					(void) strcpy(goodbuf, xhdr_dirname);
			} else {
				if ((i > NAMSIZ) || (i == NAMSIZ &&
				    (stbuf.st_mode & S_IFDIR) && !Pflag))
					(void) fprintf(stderr, gettext(
					    "tar: %s: filename is greater than "
					    "%d\n"), lastslash == NULL ?
					    longname : lastslash + 1, NAMSIZ);
				else
					(void) fprintf(stderr, gettext(
					    "tar: %s: prefix is greater than %d"
					    "\n"), longname, PRESIZ);
				if (errflag)
					done(1);
				return;
			}
		} else
			(void) strncpy(&junkbuf[0], longname + j + 1,
			    strlen(longname + j + 1));
		name = junkbuf;
		prefix = goodbuf;
	} else {
		name = longname;
	}
	if (Aflag)
		if ((prefix != NULL) && (*prefix != '\0'))
			while (*prefix == '/')
				++prefix;
		else
			while (*name == '/')
				++name;

	/* ACL support */
	if (pflag && ((stbuf.st_mode & S_IFMT) != S_IFLNK)) {
		/*
		 * Get ACL info: dont bother allocating space if there are only
		 * 	standard permissions, i.e. ACL count <= 4
		 */
		if ((aclcnt = acl(shortname, GETACLCNT, 0, NULL)) < 0) {
			(void) fprintf(stderr, gettext(
			    "%s: failed to get acl count\n"), longname);
			return;
		}
		if (aclcnt > MIN_ACL_ENTRIES) {
			if ((aclp = (aclent_t *)malloc(
			    sizeof (aclent_t) * aclcnt)) == NULL) {
				(void) fprintf(stderr, gettext(
				    "Insufficient memory\n"));
				return;
			}
			if (acl(shortname, GETACL, aclcnt, aclp) < 0) {
				(void) fprintf(stderr, gettext(
				    "%s: failed to get acl entries\n"),
				    longname);
				return;
			}
		}
		/* else: only traditional permissions, so proceed as usual */
	}

	switch (stbuf.st_mode & S_IFMT) {
	case S_IFDIR:
		stbuf.st_size = (off_t) 0;
		blocks = TBLOCKS(stbuf.st_size);
		i = 0;
		cp = buf;
		while ((*cp++ = longname[i++]))
			;
		*--cp = '/';
		*++cp = 0;
		if (!oflag) {
			tomodes(&stbuf);
			if (build_dblock(name, tchar, '5', &stbuf, stbuf.st_dev,
			    prefix) != 0)
				return;
			if (!Pflag) {
				/*
				 * Old archives require a slash at the end
				 * of a directory name.
				 *
				 * XXX
				 * If directory name is too long, will
				 * slash overfill field?
				 */
				if (strlen(name) > (unsigned)NAMSIZ-1) {
					(void) fprintf(stderr, gettext(
					    "tar: %s: filename is greater "
					    "than %d\n"), name, NAMSIZ);
					if (errflag)
						done(1);
					if (aclp != NULL)
						free(aclp);
					return;
				} else {
					if (strlen(name) == (NAMSIZ - 1)) {
						(void) memcpy(dblock.dbuf.name,
						    name, NAMSIZ);
						dblock.dbuf.name[NAMSIZ-1]
						    = '/';
					} else
						(void) sprintf(dblock.dbuf.name,
						    "%s/", name);

					/*
					 * need to recalculate checksum
					 * because the name changed.
					 */
					(void) sprintf(dblock.dbuf.chksum,
					    "%07o", checksum(&dblock));
				}
			}

			/* ACL support */
			if (pflag) {
				char	*secinfo = NULL;
				int	len = 0;

				/* append security attributes */
				(void) append_secattr(&secinfo, &len, aclcnt,
				    aclp, UFSD_ACL);

				/* call append_secattr() if more than one */

				/* write ancillary */
				(void) write_ancillary(&dblock, secinfo, len);
			}
			(void) sprintf(dblock.dbuf.chksum, "%07o",
			    checksum(&dblock));
			dblock.dbuf.typeflag = '5';
			(void) writetape((char *)&dblock);
		}
		if (vflag) {
#ifdef DEBUG
			if (NotTape)
				DEBUG("seek = %" FMT_blkcnt_t "K\t", K(tapepos),
				    0);
#endif
			(void) fprintf(vfile, "a %s/ ", longname);
			if (NotTape)
				(void) fprintf(vfile, "%" FMT_blkcnt_t "K\n",
				    K(blocks));
			else
				(void) fprintf(vfile, gettext("%" FMT_blkcnt_t
				    " tape blocks\n"), blocks);
		}
		if (*shortname != '/')
			(void) sprintf(newparent, "%s/%s", parent, shortname);
		else
			(void) sprintf(newparent, "%s", shortname);
		if (chdir(shortname) < 0) {
			vperror(0, "%s", newparent);
			if (aclp != NULL)
				free(aclp);
			return;
		}
		if ((dirp = opendir(".")) == NULL) {
			vperror(0, gettext(
			"can't open directory %s"), longname);
			if (chdir(parent) < 0)
				vperror(0, gettext("cannot change back?: %s"),
				parent);
			if (aclp != NULL)
				free(aclp);
			return;
		}
		while ((dp = readdir(dirp)) != NULL && !term) {
			if ((strcmp(".", dp->d_name) == 0) ||
			    (strcmp("..", dp->d_name) == 0))
				continue;
			(void) strcpy(cp, dp->d_name);
			l = telldir(dirp);
			(void) closedir(dirp);
			putfile(buf, cp, newparent, lev + 1);
			dirp = opendir(".");
			seekdir(dirp, l);
		}
		(void) closedir(dirp);
		if (chdir(parent) < 0)
			vperror(0, gettext("cannot change back?: %s"), parent);
		break;

	case S_IFLNK:
		readlink_max = NAMSIZ;
		if (stbuf.st_size > NAMSIZ) {
			if (Eflag > 0) {
				xhdr_flgs |= _X_LINKPATH;
				readlink_max = PATH_MAX;
			} else {
				(void) fprintf(stderr, gettext(
				"tar: %s: symbolic link too long\n"),
				    longname);
				if (errflag)
					done(1);
				if (aclp != NULL)
					free(aclp);
				return;
			}
		}
		/*
		 * Sym-links need header size of zero since you
		 * don't store any data for this type.
		 */
		stbuf.st_size = (off_t) 0;
		tomodes(&stbuf);
		i = readlink(shortname, filetmp, readlink_max);
		if (i < 0) {
			vperror(0, gettext(
			"can't read symbolic link %s"), longname);
			if (aclp != NULL)
				free(aclp);
			return;
		} else {
			filetmp[i] = 0;
		}
		if (vflag)
			(void) fprintf(vfile, gettext(
			    "a %s symbolic link to %s\n"),
			    longname, filetmp);
		if (xhdr_flgs & _X_LINKPATH) {
			Xtarhdr.x_linkpath = filetmp;
			if (build_dblock(name, tchar, '2', &stbuf,
			    stbuf.st_dev, prefix) != 0)
				return;
		} else
			if (build_dblock(name, filetmp, '2', &stbuf,
			    stbuf.st_dev, prefix) != 0)
				return;
		(void) writetape((char *)&dblock);
		/*
		 * No acls for symlinks: mode is always 777
		 * dont call write ancillary
		 */
		break;
	case S_IFREG:
		if ((infile = open(shortname, 0)) < 0) {
			vperror(0, "%s", longname);
			if (aclp != NULL)
				free(aclp);
			return;
		}

		blocks = TBLOCKS(stbuf.st_size);
		if (stbuf.st_nlink > 1) {
			struct linkbuf *lp;
			int found = 0;

			for (lp = ihead; lp != NULL; lp = lp->nextp)
				if (lp->inum == stbuf.st_ino &&
				    lp->devnum == stbuf.st_dev) {
					found++;
					break;
				}
			if (found) {
				stbuf.st_size = (off_t) 0;
				tomodes(&stbuf);
				if (chk_path_build(name, longname, lp->pathname,
				    prefix, '1') > 0) {
					(void) close(infile);
					return;
				}
				if (mulvol && tapepos + 1 >= blocklim)
					newvol();
				(void) writetape((char *)&dblock);
				/*
				 * write_ancillary() is not needed here.
				 * The first link is handled in the following
				 * else statement. No need to process ACLs
				 * for other hard links since they are the
				 * same file.
				 */

				if (vflag) {
#ifdef DEBUG
					if (NotTape)
						DEBUG("seek = %" FMT_blkcnt_t
						    "K\t", K(tapepos), 0);
#endif
					(void) fprintf(vfile, gettext(
					    "a %s link to %s\n"),
					    longname, lp->pathname);
				}
				lp->count--;
				(void) close(infile);
				if (aclp != NULL)
					free(aclp);
				return;
			} else {
				lp = (struct linkbuf *) getmem(sizeof (*lp));
				if (lp != NULL) {
					lp->nextp = ihead;
					ihead = lp;
					lp->inum = stbuf.st_ino;
					lp->devnum = stbuf.st_dev;
					lp->count = stbuf.st_nlink - 1;
					(void) strcpy(lp->pathname, longname);
				}
			}
		}
		tomodes(&stbuf);

		/* correctly handle end of volume */
		while (mulvol && tapepos + blocks + 1 > blocklim) {
			/* file won't fit */
			if (eflag) {
				if (blocks <= blocklim) {
					newvol();
					break;
				}
				(void) fprintf(stderr, gettext(
				"tar: Single file cannot fit on volume\n"));
				done(3);
			}
			/* split if floppy has some room and file is large */
			if (((blocklim - tapepos) >= EXTMIN) &&
			    ((blocks + 1) >= blocklim/10)) {
				splitfile(longname, infile);
				(void) close(infile);
				if (aclp != NULL)
					free(aclp);
				return;
			}
			newvol();	/* not worth it--just get new volume */
		}
#ifdef DEBUG
		DEBUG("putfile: %s wants %" FMT_blkcnt_t " blocks\n", longname,
		    blocks);
#endif
		if (build_dblock(name, tchar, '0', &stbuf, stbuf.st_dev,
		    prefix) != 0)
			return;
		if (vflag) {
#ifdef DEBUG
			if (NotTape)
				DEBUG("seek = %" FMT_blkcnt_t "K\t", K(tapepos),
				    0);
#endif
			(void) fprintf(vfile, "a %s ", longname);
			if (NotTape)
				(void) fprintf(vfile, "%" FMT_blkcnt_t "K\n",
				    K(blocks));
			else
				(void) fprintf(vfile,
				    gettext("%" FMT_blkcnt_t " tape blocks\n"),
				    blocks);
		}

		/* ACL support */
		if (pflag) {
			char	*secinfo = NULL;
			int	len = 0;

			/* append security attributes */
			(void) append_secattr(&secinfo, &len, aclcnt,
			    aclp, UFSD_ACL);

			/* call append_secattr() if more than one */

			/* write ancillary */
			(void) write_ancillary(&dblock, secinfo, len);
		}
		(void) sprintf(dblock.dbuf.chksum, "%07o", checksum(&dblock));
		dblock.dbuf.typeflag = '0';

		hint = writetape((char *)&dblock);
		maxread = max(stbuf.st_blksize, (nblock * TBLOCK));
		if ((bigbuf = calloc((unsigned)maxread, sizeof (char))) == 0) {
			maxread = TBLOCK;
			bigbuf = buf;
		}

		while (((i = (int)
		    read(infile, bigbuf, min((hint*TBLOCK), maxread))) > 0) &&
		    blocks) {
			blkcnt_t nblks;

			nblks = ((i-1)/TBLOCK)+1;
			if (nblks > blocks)
				nblks = blocks;
			hint = writetbuf(bigbuf, nblks);
			blocks -= nblks;
		}
		(void) close(infile);
		if (bigbuf != buf)
			free(bigbuf);
		if (i < 0)
			vperror(0, gettext("Read error on %s"), longname);
		else if (blocks != 0 || i != 0) {
			(void) fprintf(stderr, gettext(
			"tar: %s: file changed size\n"), longname);
			if (errflag)
				done(1);
		}
		putempty(blocks);
		break;
	case S_IFIFO:
		blocks = TBLOCKS(stbuf.st_size);
		stbuf.st_size = (off_t) 0;
		if (stbuf.st_nlink > 1) {
			struct linkbuf *lp;
			int found = 0;

			tomodes(&stbuf);
			for (lp = ihead; lp != NULL; lp = lp->nextp)
				if (lp->inum == stbuf.st_ino &&
				    lp->devnum == stbuf.st_dev) {
					found++;
					break;
				}
			if (found) {
				if (chk_path_build(name, longname, lp->pathname,
				    prefix, '6') > 0)
					return;
				if (mulvol && tapepos + 1 >= blocklim)
					newvol();
				(void) writetape((char *)&dblock);
				if (vflag) {
#ifdef DEBUG
					if (NotTape)
						DEBUG("seek = %" FMT_blkcnt_t
						    "K\t", K(tapepos), 0);
#endif
					(void) fprintf(vfile, gettext(
					    "a %s link to %s\n"),
					    longname, lp->pathname);
				}
				lp->count--;
				if (aclp != NULL)
					free(aclp);
				return;
			} else {
				lp = (struct linkbuf *) getmem(sizeof (*lp));
				if (lp != NULL) {
					lp->nextp = ihead;
					ihead = lp;
					lp->inum = stbuf.st_ino;
					lp->devnum = stbuf.st_dev;
					lp->count = stbuf.st_nlink - 1;
					(void) strcpy(lp->pathname, longname);
				}
			}
		}
		tomodes(&stbuf);

		while (mulvol && tapepos + blocks + 1 > blocklim) {
			if (eflag) {
				if (blocks <= blocklim) {
					newvol();
					break;
				}
				(void) fprintf(stderr, gettext(
				"tar: Single file cannot fit on volume\n"));
				done(3);
			}

			if (((blocklim - tapepos) >= EXTMIN) &&
			    ((blocks + 1) >= blocklim/10)) {
				splitfile(longname, infile);
				if (aclp != NULL)
					free(aclp);
				return;
			}
			newvol();
		}
#ifdef DEBUG
		DEBUG("putfile: %s wants %" FMT_blkcnt_t " blocks\n", longname,
		    blocks);
#endif
		if (vflag) {
#ifdef DEBUG
			if (NotTape)
				DEBUG("seek = %" FMT_blkcnt_t "K\t", K(tapepos),
				    0);
#endif
			if (NotTape)
				(void) fprintf(vfile, gettext("a %s %"
				    FMT_blkcnt_t "K\n "), longname, K(blocks));
			else
				(void) fprintf(vfile, gettext(
				    "a %s %" FMT_blkcnt_t " tape blocks\n"),
				    longname, blocks);
		}
		if (build_dblock(name, tchar, '6', &stbuf, stbuf.st_dev,
		    prefix) != 0)
			return;

		/* ACL support */
		if (pflag) {
			char	*secinfo = NULL;
			int	len = 0;

			/* append security attributes */
			(void) append_secattr(&secinfo, &len, aclcnt,
			    aclp, UFSD_ACL);

			/* call append_secattr() if more than one */

			/* write ancillary */
			(void) write_ancillary(&dblock, secinfo, len);
		}
		(void) sprintf(dblock.dbuf.chksum, "%07o", checksum(&dblock));
		dblock.dbuf.typeflag = '6';

		(void) writetape((char *)&dblock);
		break;
	case S_IFCHR:
		blocks = TBLOCKS(stbuf.st_size);
		stbuf.st_size = (off_t) 0;
		if (stbuf.st_nlink > 1) {
			struct linkbuf *lp;
			int found = 0;

			tomodes(&stbuf);
			for (lp = ihead; lp != NULL; lp = lp->nextp)
				if (lp->inum == stbuf.st_ino &&
				    lp->devnum == stbuf.st_dev) {
					found++;
					break;
				}
			if (found) {
				if (chk_path_build(name, longname, lp->pathname,
				    prefix, '3') > 0)
					return;
				if (mulvol && tapepos + 1 >= blocklim)
					newvol();
				(void) writetape((char *)&dblock);
				if (vflag) {
#ifdef DEBUG
					if (NotTape)
						DEBUG("seek = %" FMT_blkcnt_t
						    "K\t", K(tapepos), 0);
#endif
					(void) fprintf(vfile, gettext(
					    "a %s link to %s\n"), longname,
					    lp->pathname);
				}
				lp->count--;
				if (aclp != NULL)
					free(aclp);
				return;
			} else {
				lp = (struct linkbuf *) getmem(sizeof (*lp));
				if (lp != NULL) {
					lp->nextp = ihead;
					ihead = lp;
					lp->inum = stbuf.st_ino;
					lp->devnum = stbuf.st_dev;
					lp->count = stbuf.st_nlink - 1;
					(void) strcpy(lp->pathname, longname);
				}
			}
		}
		tomodes(&stbuf);

		while (mulvol && tapepos + blocks + 1 > blocklim) {
			if (eflag) {
				if (blocks <= blocklim) {
					newvol();
					break;
				}
				(void) fprintf(stderr, gettext(
				"tar: Single file cannot fit on volume\n"));
				done(3);
			}

			if (((blocklim - tapepos) >= EXTMIN) &&
			    ((blocks + 1) >= blocklim/10)) {
				splitfile(longname, infile);
				if (aclp != NULL)
					free(aclp);
				return;
			}
			newvol();
		}
#ifdef DEBUG
		DEBUG("putfile: %s wants %" FMT_blkcnt_t " blocks\n", longname,
		    blocks);
#endif
		if (vflag) {
#ifdef DEBUG
			if (NotTape)
				DEBUG("seek = %" FMT_blkcnt_t "K\t", K(tapepos),
				    0);
#endif
			if (NotTape)
				(void) fprintf(vfile, gettext("a %s %"
				    FMT_blkcnt_t "K\n"), longname, K(blocks));
			else
				(void) fprintf(vfile, gettext("a %s %"
				    FMT_blkcnt_t " tape blocks\n"), longname,
				    blocks);
		}
		if (build_dblock(name, tchar, '3', &stbuf, stbuf.st_rdev,
		    prefix) != 0)
			return;

		/* ACL support */
		if (pflag) {
			char	*secinfo = NULL;
			int	len = 0;

			/* append security attributes */
			(void) append_secattr(&secinfo, &len, aclcnt,
			    aclp, UFSD_ACL);

			/* call append_secattr() if more than one */

			/* write ancillary */
			(void) write_ancillary(&dblock, secinfo, len);
		}
		(void) sprintf(dblock.dbuf.chksum, "%07o", checksum(&dblock));
		dblock.dbuf.typeflag = '3';

		(void) writetape((char *)&dblock);
		break;
	case S_IFBLK:
		blocks = TBLOCKS(stbuf.st_size);
		stbuf.st_size = (off_t) 0;
		if (stbuf.st_nlink > 1) {
			struct linkbuf *lp;
			int found = 0;

			tomodes(&stbuf);
			for (lp = ihead; lp != NULL; lp = lp->nextp)
				if (lp->inum == stbuf.st_ino &&
				    lp->devnum == stbuf.st_dev) {
					found++;
					break;
				}
			if (found) {
				if (chk_path_build(name, longname, lp->pathname,
				    prefix, '4') > 0)
					return;
				if (mulvol && tapepos + 1 >= blocklim)
					newvol();
				(void) writetape((char *)&dblock);
				if (vflag) {
#ifdef DEBUG
					if (NotTape)
						DEBUG("seek = %" FMT_blkcnt_t
						    "K\t", K(tapepos), 0);
#endif
					(void) fprintf(vfile, gettext(
					    "a %s link to %s\n"),
					    longname, lp->pathname);
				}
				lp->count--;
				if (aclp != NULL)
					free(aclp);
				return;
			} else {
				lp = (struct linkbuf *) getmem(sizeof (*lp));
				if (lp != NULL) {
					lp->nextp = ihead;
					ihead = lp;
					lp->inum = stbuf.st_ino;
					lp->devnum = stbuf.st_dev;
					lp->count = stbuf.st_nlink - 1;
					(void) strcpy(lp->pathname, longname);
				}
			}
		}
		tomodes(&stbuf);

		while (mulvol && tapepos + blocks + 1 > blocklim) {
			if (eflag) {
				if (blocks <= blocklim) {
					newvol();
					break;
				}
				(void) fprintf(stderr, gettext(
				"tar: Single file cannot fit on volume\n"));
				done(3);
			}

			if (((blocklim - tapepos) >= EXTMIN) &&
			    ((blocks + 1) >= blocklim/10)) {
				splitfile(longname, infile);
				if (aclp != NULL)
					free(aclp);
				return;
			}
			newvol();
		}
#ifdef DEBUG
		DEBUG("putfile: %s wants %" FMT_blkcnt_t " blocks\n", longname,
		    blocks);
#endif
		if (vflag) {
#ifdef DEBUG
			if (NotTape)
				DEBUG("seek = %" FMT_blkcnt_t "K\t", K(tapepos),
				    0);
#endif
			(void) fprintf(vfile, "a %s ", longname);
			if (NotTape)
				(void) fprintf(vfile, "%" FMT_blkcnt_t "K\n",
				    K(blocks));
			else
				(void) fprintf(vfile, gettext("%"
				    FMT_blkcnt_t " tape blocks\n"), blocks);
		}
		if (build_dblock(name, tchar, '4', &stbuf, stbuf.st_rdev,
		    prefix) != 0)
			return;

		/* ACL support */
		if (pflag) {
			char	*secinfo = NULL;
			int	len = 0;

			/* append security attributes */
			(void) append_secattr(&secinfo, &len, aclcnt,
			    aclp, UFSD_ACL);

			/* call append_secattr() if more than one */

			/* write ancillary */
			(void) write_ancillary(&dblock, secinfo, len);
		}
		(void) sprintf(dblock.dbuf.chksum, "%07o", checksum(&dblock));
		dblock.dbuf.typeflag = '4';

		(void) writetape((char *)&dblock);
		break;
	default:
		(void) fprintf(stderr, gettext(
		"tar: %s is not a file. Not dumped\n"),
		    longname);
		if (errflag)
			done(1);
		break;
	}

	/* free up acl stuff */
	if (pflag && aclp != NULL) {
		free(aclp);
		aclp = NULL;
	}
}


/*
 *	splitfile	dump a large file across volumes
 *
 *	splitfile(longname, fd);
 *		char *longname;		full name of file
 *		int ifd;		input file descriptor
 *
 *	NOTE:  only called by putfile() to dump a large file.
 */

static void
splitfile(char *longname, int ifd)
{
	blkcnt_t blocks;
	off_t bytes, s;
	char buf[TBLOCK];
	int i, extents;

	blocks = TBLOCKS(stbuf.st_size);	/* blocks file needs */

	/*
	 * # extents =
	 *	size of file after using up rest of this floppy
	 *		blocks - (blocklim - tapepos) + 1	(for header)
	 *	plus roundup value before divide by blocklim-1
	 *		+ (blocklim - 1) - 1
	 *	all divided by blocklim-1 (one block for each header).
	 * this gives
	 *	(blocks - blocklim + tapepos + 1 + blocklim - 2)/(blocklim-1)
	 * which reduces to the expression used.
	 * one is added to account for this first extent.
	 *
	 * When one is dealing with extremely large archives, one may want
	 * to allow for a large number of extents.  This code should be
	 * revisited to determine if extents should be changed to something
	 * larger than an int.
	 */
	extents = (int)((blocks + tapepos - 1ULL)/(blocklim - 1ULL) + 1);

	if (extents < 2 || extents > MAXEXT) {	/* let's be reasonable */
		(void) fprintf(stderr, gettext(
		    "tar: %s needs unusual number of volumes to split\n"
		    "tar: %s not dumped\n"), longname, longname);
		return;
	}
	extents = dblock.dbuf.extotal;
	bytes = stbuf.st_size;
	(void) sprintf(dblock.dbuf.efsize, "%10" FMT_off_t_o, bytes);

	(void) fprintf(stderr, gettext(
	    "tar: large file %s needs %d extents.\n"
	    "tar: current device seek position = %" FMT_blkcnt_t "K\n"),
	    longname, extents, K(tapepos));

	s = (off_t)(blocklim - tapepos - 1) * TBLOCK;
	for (i = 1; i <= extents; i++) {
		if (i > 1) {
			newvol();
			if (i == extents)
				s = bytes;	/* last ext. gets true bytes */
			else
				s = (off_t)(blocklim - 1)*TBLOCK; /* all */
		}
		bytes -= s;
		blocks = TBLOCKS(s);

		(void) sprintf(dblock.dbuf.size, "%12" FMT_off_t_o, s);
		i = dblock.dbuf.extno;
		(void) sprintf(dblock.dbuf.chksum, "%07o", checksum(&dblock));
		(void) writetape((char *)&dblock);

		if (vflag)
			(void) fprintf(vfile,
			    "+++ a %s %" FMT_blkcnt_t "K [extent #%d of %d]\n",
			    longname, K(blocks), i, extents);
		while (blocks && read(ifd, buf, TBLOCK) > 0) {
			blocks--;
			(void) writetape(buf);
		}
		if (blocks != 0) {
			(void) fprintf(stderr, gettext(
			    "tar: %s: file changed size\n"), longname);
			(void) fprintf(stderr, gettext(
			    "tar: aborting split file %s\n"), longname);
			(void) close(ifd);
			return;
		}
	}
	(void) close(ifd);
	if (vflag)
		(void) fprintf(vfile, gettext("a %s %" FMT_off_t "K (in %d "
		    "extents)\n"), longname, K(TBLOCKS(stbuf.st_size)),
		    extents);
}

static void
#ifdef	_iBCS2
doxtract(char *argv[], int tbl_cnt)
#else
doxtract(char *argv[])
#endif

{
	struct	stat	xtractbuf;	/* stat on file after extracting */
	blkcnt_t blocks;
	off_t bytes;
	int ofile;
	int newfile;			/* Does the file already exist  */
	int xcnt = 0;			/* count # files extracted */
	int fcnt = 0;			/* count # files in argv list */
	int dir;
	uid_t Uid;
	char *namep, *linkp;		/* for removing absolute paths */
	char dirname[PATH_MAX+1];
	char templink[PATH_MAX+1];	/* temp link with terminating NULL */
	int once = 1;
	int symflag;
	int want;
	aclent_t	*aclp = NULL;	/* acl buffer pointer */
	int		aclcnt = 0;	/* acl entries count */
	timestruc_t	time_zero;	/* used for call to doDirTimes */

	time_zero.tv_sec = 0;
	time_zero.tv_nsec = 0;

	dumping = 0;	/* for newvol(), et al:  we are not writing */

	/*
	 * Count the number of files that are to be extracted
	 */
	Uid = getuid();

#ifdef	_iBCS2
	initarg(argv, Filefile);
	while (nextarg() != NULL)
		++fcnt;
	fcnt += tbl_cnt;
#endif	/*  _iBCS2 */

	for (;;) {
		symflag = 0;
		dir = 0;
		ofile = -1;

		/* namep is set by wantit to point to the full name */
		if ((want = wantit(argv, &namep)) == 0)
			continue;
		if (want == -1)
			break;

		if (xhdr_flgs & _X_LINKPATH)
			(void) strcpy(templink, Xtarhdr.x_linkpath);
		else
			(void) sprintf(templink, "%.*s", NAMSIZ,
			    dblock.dbuf.linkname);

		if (Fflag) {
			char *s;

			if ((s = strrchr(namep, '/')) == 0)
				s = namep;

			else
				s++;
			if (checkf(s, stbuf.st_mode, Fflag) == 0) {
				passtape();
				continue;
			}
		}

		if (checkw('x', namep) == 0) {
			passtape();
			continue;
		}
		if (once) {
			if (strcmp(dblock.dbuf.magic, magic_type) == 0) {
				if (geteuid() == (uid_t) 0) {
					checkflag = 1;
					pflag = 1;
				} else {
					/* get file creation mask */
					Oumask = umask(0);
					(void) umask(Oumask);
				}
				once = 0;
			} else {
				if (geteuid() == (uid_t) 0) {
					pflag = 1;
					checkflag = 2;
				}
				if (!pflag) {
					/* get file creation mask */
					Oumask = umask(0);
					(void) umask(Oumask);
				}
				once = 0;
			}
		}

		(void) strcpy(&dirname[0], namep);
		if (checkdir(&dirname[0]) &&
			(!is_posix || dblock.dbuf.typeflag == '5')) {
			dir = 1;
			if (vflag) {
				(void) fprintf(vfile, "x %s, 0 bytes, ",
				    &dirname[0]);
				if (NotTape)
					(void) fprintf(vfile, "0K\n");
				else
					(void) fprintf(vfile, gettext("%"
					    FMT_blkcnt_t " tape blocks\n"),
					    (blkcnt_t)0);
			}
			goto filedone;
		}
		if (dblock.dbuf.typeflag == '6') {	/* FIFO */
			if (rmdir(namep) < 0) {
				if (errno == ENOTDIR)
					(void) unlink(namep);
			}
			linkp = templink;
			if (*linkp !=  NULL) {
				if (Aflag && *linkp == '/')
					linkp++;
				if (link(linkp, namep) < 0) {
					(void) fprintf(stderr, gettext(
					    "tar: %s: cannot link\n"), namep);
					continue;
				}
				if (vflag)
					(void) fprintf(vfile, gettext(
					    "%s linked to %s\n"), namep, linkp);
				xcnt++;	 /* increment # files extracted */
				continue;
			}
			if (mknod(namep, (int)(Gen.g_mode|S_IFIFO),
			    (int)Gen.g_devmajor) < 0) {
				vperror(0, gettext("%s: mknod failed"), namep);
				continue;
			}
			bytes = stbuf.st_size;
			blocks = TBLOCKS(bytes);
			if (vflag) {
				(void) fprintf(vfile, "x %s, %" FMT_off_t
				    " bytes, ", namep, bytes);
				if (NotTape)
					(void) fprintf(vfile, "%" FMT_blkcnt_t
					    "K\n", K(blocks));
				else
					(void) fprintf(vfile, gettext("%"
					    FMT_blkcnt_t " tape blocks\n"),
					    blocks);
			}
			goto filedone;
		}
		if (dblock.dbuf.typeflag == '3' && !Uid) { /* CHAR SPECIAL */
			if (rmdir(namep) < 0) {
				if (errno == ENOTDIR)
					(void) unlink(namep);
			}
			linkp = templink;
			if (*linkp != NULL) {
				if (Aflag && *linkp == '/')
					linkp++;
				if (link(linkp, namep) < 0) {
					(void) fprintf(stderr, gettext(
					    "tar: %s: cannot link\n"), namep);
					continue;
				}
				if (vflag)
					(void) fprintf(vfile, gettext(
					    "%s linked to %s\n"), namep, linkp);
				xcnt++;	 /* increment # files extracted */
				continue;
			}
			if (mknod(namep, (int)(Gen.g_mode|S_IFCHR),
			    (int)makedev(Gen.g_devmajor, Gen.g_devminor)) < 0) {
				vperror(0, gettext(
				"%s: mknod failed"), namep);
				continue;
			}
			bytes = stbuf.st_size;
			blocks = TBLOCKS(bytes);
			if (vflag) {
				(void) fprintf(vfile, "x %s, %" FMT_off_t
				    " bytes, ", namep, bytes);
				if (NotTape)
					(void) fprintf(vfile, "%" FMT_blkcnt_t
					    "K\n", K(blocks));
				else
					(void) fprintf(vfile, gettext("%"
					    FMT_blkcnt_t " tape blocks\n"),
					    blocks);
			}
			goto filedone;
		} else if (dblock.dbuf.typeflag == '3' && Uid) {
			(void) fprintf(stderr, gettext(
			    "Can't create special %s\n"), namep);
			continue;
		}

		/* BLOCK SPECIAL */

		if (dblock.dbuf.typeflag == '4' && !Uid) {
			if (rmdir(namep) < 0) {
				if (errno == ENOTDIR)
					(void) unlink(namep);
			}
			linkp = templink;
			if (*linkp != NULL) {
				if (Aflag && *linkp == '/')
					linkp++;
				if (link(linkp, namep) < 0) {
					(void) fprintf(stderr, gettext(
					    "tar: %s: cannot link\n"), namep);
					continue;
				}
				if (vflag)
					(void) fprintf(vfile, gettext(
					    "%s linked to %s\n"), namep, linkp);
				xcnt++;	 /* increment # files extracted */
				continue;
			}
			if (mknod(namep, (int)(Gen.g_mode|S_IFBLK),
			    (int)makedev(Gen.g_devmajor, Gen.g_devminor)) < 0) {
				vperror(0, gettext("%s: mknod failed"), namep);
				continue;
			}
			bytes = stbuf.st_size;
			blocks = TBLOCKS(bytes);
			if (vflag) {
				(void) fprintf(vfile, gettext("x %s, %"
				    FMT_off_t " bytes, "), namep, bytes);
				if (NotTape)
					(void) fprintf(vfile, "%" FMT_blkcnt_t
					    "K\n", K(blocks));
				else
					(void) fprintf(vfile, gettext("%"
					    FMT_blkcnt_t " tape blocks\n"),
					    blocks);
			}
			goto filedone;
		} else if (dblock.dbuf.typeflag == '4' && Uid) {
			(void) fprintf(stderr,
			    gettext("Can't create special %s\n"), namep);
			continue;
		}
		if (dblock.dbuf.typeflag == '2') {	/* symlink */
			linkp = templink;
			if (Aflag && *linkp == '/')
				linkp++;
			if (rmdir(namep) < 0) {
				if (errno == ENOTDIR)
					(void) unlink(namep);
			}
			if (symlink(linkp, namep) < 0) {
				vperror(0, gettext("%s: symbolic link failed"),
				namep);
				continue;
			}
			if (vflag)
				(void) fprintf(vfile, gettext(
				"x %s symbolic link to %s\n"), namep, linkp);

			symflag = 1;
			goto filedone;
		}
		if (dblock.dbuf.typeflag == '1') {
			linkp = templink;
			if (Aflag && *linkp == '/')
				linkp++;
			if (rmdir(namep) < 0) {
				if (errno == ENOTDIR)
					(void) unlink(namep);
			}
			if (link(linkp, namep) < 0) {
				(void) fprintf(stderr, gettext(
				    "tar: %s: cannot link\n"), namep);
				continue;
			}
			if (vflag)
				(void) fprintf(vfile, gettext(
				"%s linked to %s\n"), namep, linkp);
			xcnt++;		/* increment # files extracted */
			continue;
		}

		/* REGULAR FILES */

		if ((dblock.dbuf.typeflag == '0') ||
		    (dblock.dbuf.typeflag == NULL)) {
			delete_target(namep);
			linkp = templink;
			if (*linkp != NULL) {
				if (Aflag && *linkp == '/')
					linkp++;
				if (link(linkp, namep) < 0) {
					(void) fprintf(stderr, gettext(
					    "tar: %s: cannot link\n"), namep);
					continue;
				}
				if (vflag)
					(void) fprintf(vfile, gettext(
					    "%s linked to %s\n"), namep, linkp);
				xcnt++;	 /* increment # files extracted */
				continue;
			}
		newfile = ((stat(namep, &xtractbuf) == -1) ? TRUE : FALSE);
		if ((ofile = creat(namep, stbuf.st_mode & MODEMASK)) < 0) {
			(void) fprintf(stderr, gettext(
			    "tar: %s - cannot create\n"), namep);
			passtape();
			continue;
		}

		if (extno != 0) {	/* file is in pieces */
			if (extotal < 1 || extotal > MAXEXT)
				(void) fprintf(stderr, gettext(
				    "tar: ignoring bad extent info for %s\n"),
				    namep);
			else {
				xsfile(ofile);	/* extract it */
				goto filedone;
			}
		}
		extno = 0;	/* let everyone know file is not split */
		bytes = stbuf.st_size;
		blocks = TBLOCKS(bytes);
		if (vflag) {
			(void) fprintf(vfile, "x %s, %" FMT_off_t " bytes, ",
			    namep, bytes);
			if (NotTape)
				(void) fprintf(vfile, "%" FMT_blkcnt_t "K\n",
				    K(blocks));
			else
				(void) fprintf(vfile, gettext("%"
				    FMT_blkcnt_t " tape blocks\n"), blocks);
		}

		xblocks(bytes, ofile);
filedone:
		if (mflag == 0 && !symflag) {
			if (dir)
				doDirTimes(namep, stbuf.st_mtim);
			else
				setPathTimes(namep, stbuf.st_mtim);
		}

		/* moved this code from above */
		if (pflag && !symflag)
			(void) chmod(namep, stbuf.st_mode & MODEMASK);
		/*
		 * Because ancillary file preceeds the normal file,
		 * acl info may have been retrieved (in aclp).
		 * All file types are directed here (go filedone).
		 * Always restore ACLs if there are ACLs.
		 */
		if (aclp != NULL) {
			if (acl(namep, SETACL, aclcnt, aclp) < 0) {
				if (pflag) {
					fprintf(stderr, gettext(
					    "%s: failed to set acl entries\n"),
					    namep);
				}
				/* else: silent and continue */
			}
			free(aclp);
			aclp = NULL;
		}

		if (!oflag)
		    resugname(namep, symflag); /* set file ownership */

		if (pflag && newfile == TRUE && !dir &&
				(dblock.dbuf.typeflag == '0' ||
				    dblock.dbuf.typeflag == NULL ||
				    dblock.dbuf.typeflag == '1')) {
			if (fstat(ofile, &xtractbuf) == -1)
				(void) fprintf(stderr, gettext(
				"tar: cannot stat extracted file %s\n"), namep);
			else if ((xtractbuf.st_mode & (MODEMASK & ~S_IFMT))
				!= (stbuf.st_mode & (MODEMASK & ~S_IFMT))) {
				(void) fprintf(stderr, gettext(
				    "tar: warning - file permissions have "
				    "changed for %s (are 0%o, should be "
				    "0%o)\n"),
				    namep, xtractbuf.st_mode, stbuf.st_mode);
			}
		}
		if (ofile != -1) {
			if (close(ofile) != 0)
				vperror(2, gettext("close error"));
		}
		xcnt++;			/* increment # files extracted */
		}
		if (dblock.dbuf.typeflag == 'A') { 	/* acl info */
			char	buf[TBLOCK];
			char	*secp;
			char	*tp;
			int	attrsize;
			int	cnt;


			if (pflag) {
				bytes = stbuf.st_size;
				if ((secp = malloc((int)bytes)) == NULL) {
					(void) fprintf(stderr, gettext(
					    "Insufficient memory for acl\n"));
					passtape();
					continue;
				}
				tp = secp;
				blocks = TBLOCKS(bytes);
				while (blocks-- > 0) {
					readtape(buf);
					if (bytes <= TBLOCK) {
						memcpy(tp, buf, (size_t)bytes);
						break;
					} else {
						memcpy(tp, buf, TBLOCK);
						tp += TBLOCK;
					}
					bytes -= TBLOCK;
				}
				/* got all attributes in secp */
				tp = secp;
				do {
					attr = (struct sec_attr *) tp;
					switch (attr->attr_type) {
					case UFSD_ACL:
						(void) sscanf(attr->attr_len,
						    "%7o", (uint *)&aclcnt);
						/* header is 8 */
						attrsize = 8 + (int) strlen(
						    &attr->attr_info[0]) + 1;
						aclp = aclfromtext(
						    &attr->attr_info[0], &cnt);
						if (aclp == NULL) {
							fprintf(stderr, gettext(
					"aclfromtext failed\n"));
							break;
						}
						if (aclcnt != cnt) {
							fprintf(stderr, gettext(
							    "aclcnt error\n"));
							break;
						}
						bytes -= attrsize;
						break;

					/* SunFed case goes here */

					default:
						fprintf(stderr, gettext(
				"unrecognized attr type\n"));
						bytes = (off_t)0;
						break;
					}

					/* next attributes */
					tp += attrsize;
				} while (bytes != 0);
				free(secp);
			} else
				passtape();
		} /* acl */

	} /* for */

	/*
	 *  Ensure that all the directories still on the directory stack
	 *  get their modification times set correctly by flushing the
	 *  stack.
	 */

	doDirTimes(NULL, time_zero);

	/*
	 * Check if the number of files extracted is different from the
	 * number of files listed on the command line
	 */
	if (fcnt > xcnt) {
		(void) fprintf(stderr,
		    gettext("tar: %d file(s) not extracted\n"),
		fcnt-xcnt);
		Errflg = 1;
	}
}

/*
 *	xblocks		extract file/extent from tape to output file
 *
 *	xblocks(bytes, ofile);
 *	unsigned long long bytes;	size of extent or file to be extracted
 *
 *	called by doxtract() and xsfile()
 */

static void
xblocks(off_t bytes, int ofile)
{
	blkcnt_t blocks;
	char buf[TBLOCK];
	char tempname[NAMSIZ+1];
	int write_count;

	blocks = TBLOCKS(bytes);
	while (blocks-- > 0) {
		readtape(buf);
		if (bytes > TBLOCK)
			write_count = TBLOCK;
		else
			write_count = bytes;
		if (write(ofile, buf, write_count) < 0) {
			if (xhdr_flgs & _X_PATH)
				(void) strcpy(tempname, Xtarhdr.x_path);
			else
				(void) sprintf(tempname, "%.*s", NAMSIZ,
				    dblock.dbuf.name);
			(void) fprintf(stderr, gettext(
			    "tar: %s: HELP - extract write error\n"), tempname);
			done(2);
		}
		bytes -= TBLOCK;
	}
}


/*
 * 	xsfile	extract split file
 *
 *	xsfile(ofd);	ofd = output file descriptor
 *
 *	file extracted and put in ofd via xblocks()
 *
 *	NOTE:  only called by doxtract() to extract one large file
 */

static	union	hblock	savedblock;	/* to ensure same file across volumes */

static void
xsfile(int ofd)
{
	int i, c;
	char name[PATH_MAX+1];	/* holds name for diagnostics */
	int extents, totalext;
	off_t bytes, totalbytes;

	if (xhdr_flgs & _X_PATH)
		(void) strcpy(name, Xtarhdr.x_path);
	else
		(void) sprintf(name, "%.*s", NAMSIZ, dblock.dbuf.name);

	totalbytes = (off_t)0;		/* in case we read in half the file */
	totalext = 0;		/* these keep count */

	(void) fprintf(stderr, gettext(
	    "tar: %s split across %d volumes\n"), name, extotal);

	/* make sure we do extractions in order */
	if (extno != 1) {	/* starting in middle of file? */
		wchar_t yeschar;
		wchar_t nochar;
		(void) mbtowc(&yeschar, nl_langinfo(YESSTR), MB_LEN_MAX);
		(void) mbtowc(&nochar, nl_langinfo(NOSTR), MB_LEN_MAX);
		(void) printf(gettext(
		"tar: first extent read is not #1\n"
		"OK to read file beginning with extent #%d (%wc/%wc) ? "),
		extno, yeschar, nochar);
		if (yesnoresponse() != yeschar) {
canit:
			passtape();
			if (close(ofd) != 0)
				vperror(2, gettext("close error"));
			return;
		}
	}
	extents = extotal;
	i = extno;
	/*CONSTCOND*/
	while (1) {
		bytes = stbuf.st_size;
		if (vflag)
			(void) fprintf(vfile, "+++ x %s [extent #%d], %"
			    FMT_off_t " bytes, %ldK\n", name, extno, bytes,
			    (long)K(TBLOCKS(bytes)));
		xblocks(bytes, ofd);

		totalbytes += bytes;
		totalext++;
		if (++i > extents)
			break;

		/* get next volume and verify it's the right one */
		copy(&savedblock, &dblock);
tryagain:
		newvol();
		xhdr_flgs = 0;
		getdir();
		if (Xhdrflag > 0)
			(void) get_xdata();	/* Get x-header & regular hdr */
		if (endtape()) {	/* seemingly empty volume */
			(void) fprintf(stderr, gettext(
			    "tar: first record is null\n"));
asknicely:
			(void) fprintf(stderr, gettext(
			    "tar: need volume with extent #%d of %s\n"),
			    i, name);
			goto tryagain;
		}
		if (notsame()) {
			(void) fprintf(stderr, gettext(
			    "tar: first file on that volume is not "
			    "the same file\n"));
			goto asknicely;
		}
		if (i != extno) {
			(void) fprintf(stderr, gettext(
		"tar: extent #%d received out of order\ntar: should be #%d\n"),
		extno, i);
			(void) fprintf(stderr, gettext(
			    "Ignore error, Abort this file, or "
			    "load New volume (i/a/n) ? "));
			c = response();
			if (c == 'a')
				goto canit;
			if (c != 'i')		/* default to new volume */
				goto asknicely;
			i = extno;		/* okay, start from there */
		}
	}
	if (vflag)
		(void) fprintf(vfile, gettext(
		    "x %s (in %d extents), %" FMT_off_t " bytes, %ldK\n"),
		    name, totalext, totalbytes, (long)K(TBLOCKS(totalbytes)));
}


/*
 *	notsame()	check if extract file extent is invalid
 *
 *	returns true if anything differs between savedblock and dblock
 *	except extno (extent number), checksum, or size (extent size).
 *	Determines if this header belongs to the same file as the one we're
 *	extracting.
 *
 *	NOTE:	though rather bulky, it is only called once per file
 *		extension, and it can withstand changes in the definition
 *		of the header structure.
 *
 *	WARNING:	this routine is local to xsfile() above
 */

static int
notsame(void)
{
	return (
	    (strncmp(savedblock.dbuf.name, dblock.dbuf.name, NAMSIZ)) ||
	    (strcmp(savedblock.dbuf.mode, dblock.dbuf.mode)) ||
	    (strcmp(savedblock.dbuf.uid, dblock.dbuf.uid)) ||
	    (strcmp(savedblock.dbuf.gid, dblock.dbuf.gid)) ||
	    (strcmp(savedblock.dbuf.mtime, dblock.dbuf.mtime)) ||
	    (savedblock.dbuf.typeflag != dblock.dbuf.typeflag) ||
	    (strncmp(savedblock.dbuf.linkname, dblock.dbuf.linkname, NAMSIZ)) ||
	    (savedblock.dbuf.extotal != dblock.dbuf.extotal) ||
	    (strcmp(savedblock.dbuf.efsize, dblock.dbuf.efsize)));
}


static void
#ifdef	_iBCS2
dotable(char *argv[], int tbl_cnt)
#else
dotable(char *argv[])
#endif

{
	int tcnt;			/* count # files tabled */
	int fcnt;			/* count # files in argv list */
	char *namep;
	int want;
	char aclchar = ' ';			/* either blank or '+' */
	char templink[PATH_MAX+1];

	dumping = 0;

	/* if not on magtape, maximize seek speed */
	if (NotTape && !bflag) {
#if SYS_BLOCK > TBLOCK
		nblock = SYS_BLOCK / TBLOCK;
#else
		nblock = 1;
#endif
	}
	/*
	 * Count the number of files that are to be tabled
	 */
	fcnt = tcnt = 0;

#ifdef	_iBCS2
	initarg(argv, Filefile);
	while (nextarg() != NULL)
		++fcnt;
	fcnt += tbl_cnt;
#endif	/*  _iBCS2 */

	for (;;) {

		/* namep is set by wantit to point to the full name */
		if ((want = wantit(argv, &namep)) == 0)
			continue;
		if (want == -1)
			break;
		if (dblock.dbuf.typeflag != 'A')
			++tcnt;
		/*
		 * ACL support:
		 * aclchar is introduced to indicate if there are
		 * acl entries. longt() now takes one extra argument.
		 */
		if (vflag) {
			if (dblock.dbuf.typeflag == 'A') {
				aclchar = '+';
				passtape();
				continue;
			}
			longt(&stbuf, aclchar);
			aclchar = ' ';
		}

		(void) printf("%s", namep);
		if (extno != 0) {
			if (vflag)
				(void) fprintf(vfile, gettext(
				    "\n [extent #%d of %d] %" FMT_off_t
				    " bytes total"), extno, extotal, efsize);
			else
				(void) fprintf(vfile, gettext(
				    " [extent #%d of %d]"), extno, extotal);
		}
		if (xhdr_flgs & _X_LINKPATH)
			(void) strcpy(templink, Xtarhdr.x_linkpath);
		else {
			(void) sprintf(templink, "%.*s", NAMSIZ,
			    dblock.dbuf.linkname);
			templink[NAMSIZ] = '\0';
		}
		if (dblock.dbuf.typeflag == '1')
			/*
			 * TRANSLATION_NOTE
			 *	Subject is omitted here.
			 *	Translate this as if
			 *		<subject> linked to %s
			 */
			(void) printf(gettext(" linked to %s"), templink);
		if (dblock.dbuf.typeflag == '2')
			(void) printf(gettext(
			/*
			 * TRANSLATION_NOTE
			 *	Subject is omitted here.
			 *	Translate this as if
			 *		<subject> symbolic link to %s
			 */
			" symbolic link to %s"), templink);
		(void) printf("\n");
		passtape();
	}
	/*
	 * Check if the number of files tabled is different from the
	 * number of files listed on the command line
	 */
	if (fcnt > tcnt) {
		(void) fprintf(stderr, gettext(
		    "tar: %d file(s) not found\n"), fcnt-tcnt);
		Errflg = 1;
	}
}

static void
putempty(blkcnt_t n)
{
	char buf[TBLOCK];
	char *cp;

	for (cp = buf; cp < &buf[TBLOCK]; )
		*cp++ = '\0';
	while (n--)
		(void) writetape(buf);
}

static	ushort	Ftype = S_IFMT;
static	void
verbose(struct stat *st, char aclchar)
{
	int i, j, temp;
	mode_t mode;
	char modestr[12];

	for (i = 0; i < 11; i++)
		modestr[i] = '-';
	modestr[i] = '\0';

	/* a '+' sign is printed if there is ACL */
	modestr[i-1] = aclchar;

	mode = st->st_mode;
	for (i = 0; i < 3; i++) {
		temp = (mode >> (6 - (i * 3)));
		j = (i * 3) + 1;
		if (S_IROTH & temp)
			modestr[j] = 'r';
		if (S_IWOTH & temp)
			modestr[j + 1] = 'w';
		if (S_IXOTH & temp)
			modestr[j + 2] = 'x';
	}
	temp = st->st_mode & Ftype;
	switch (temp) {
	case (S_IFIFO):
		modestr[0] = 'p';
		break;
	case (S_IFCHR):
		modestr[0] = 'c';
		break;
	case (S_IFDIR):
		modestr[0] = 'd';
		break;
	case (S_IFBLK):
		modestr[0] = 'b';
		break;
	case (S_IFREG): /* was initialized to '-' */
		break;
	case (S_IFLNK):
		modestr[0] = 'l';
		break;
	default:
		/* This field may be zero in old archives */
		if (is_posix)
			(void) fprintf(stderr, gettext(
			    "tar: impossible file type"));
	}

	if ((S_ISUID & Gen.g_mode) == S_ISUID)
		modestr[3] = 's';
	if ((S_ISVTX & Gen.g_mode) == S_ISVTX)
		modestr[9] = 't';
	if ((S_ISGID & Gen.g_mode) == S_ISGID && modestr[6] == 'x')
		modestr[6] = 's';
	else if ((S_ENFMT & Gen.g_mode) == S_ENFMT && modestr[6] != 'x')
		modestr[6] = 'l';
	(void) fprintf(vfile, "%s", modestr);
}

static void
longt(struct stat *st, char aclchar)
{
	char fileDate[30];
	struct tm *tm;

	verbose(st, aclchar);
	(void) fprintf(vfile, "%3ld/%-3ld", st->st_uid, st->st_gid);

	if (dblock.dbuf.typeflag == '2') {
		if (xhdr_flgs & _X_LINKPATH)
			st->st_size = (off_t) strlen(Xtarhdr.x_linkpath);
		else
			st->st_size = (off_t) (memchr(dblock.dbuf.linkname,
			    '\0', NAMSIZ) ?
			    (strlen(dblock.dbuf.linkname)) : (NAMSIZ));
	}
	(void) fprintf(vfile, " %6" FMT_off_t, st->st_size);

	tm = localtime(&(st->st_mtime));
	(void) strftime(fileDate, sizeof (fileDate),
	    dcgettext((const char *)0, "%b %e %R %Y", LC_TIME), tm);
	(void) fprintf(vfile, " %s ", fileDate);
}


/*
 *  checkdir - Attempt to ensure that the path represented in name
 *             exists, and return 1 if this is true and name itself is a
 *             directory.
 *             Return 0 if this path cannot be created or if name is not
 *             a directory.
 */

static int
checkdir(char *name)
{
	char lastChar;		   /* the last character in name */
	char *cp;		   /* scratch pointer into name */
	char *firstSlash = NULL;   /* first slash in name */
	char *lastSlash = NULL;	   /* last slash in name */
	int  nameLen;		   /* length of name */
	int  trailingSlash;	   /* true if name ends in slash */
	int  leadingSlash;	   /* true if name begins with slash */
	int  markedDir;		   /* true if name denotes a directory */
	int  success;		   /* status of makeDir call */


	/*
	 *  Scan through the name, and locate first and last slashes.
	 */

	for (cp = name; *cp; cp++) {
		if (*cp == '/') {
			if (! firstSlash) {
				firstSlash = cp;
			}
			lastSlash = cp;
		}
	}

	/*
	 *  Determine what you can from the proceeds of the scan.
	 */

	lastChar	= *(cp - 1);
	nameLen		= (int)(cp - name);
	trailingSlash	= (lastChar == '/');
	leadingSlash	= (*name == '/');
	markedDir	= (dblock.dbuf.typeflag == '5' || trailingSlash);

	if (! lastSlash && ! markedDir) {
		/*
		 *  The named file does not have any subdrectory
		 *  structure; just bail out.
		 */

		return (0);
	}

	/*
	 *  Make sure that name doesn`t end with slash for the loop.
	 *  This ensures that the makeDir attempt after the loop is
	 *  meaningful.
	 */

	if (trailingSlash) {
		name[nameLen-1] = '\0';
	}

	/*
	 *  Make the path one component at a time.
	 */

	for (cp = strchr(leadingSlash ? name+1 : name, '/');
	    cp;
	    cp = strchr(cp+1, '/')) {
		*cp = '\0';
		success = makeDir(name);
		*cp = '/';

		if (!success) {
			name[nameLen-1] = lastChar;
			return (0);
		}
	}

	/*
	 *  This makes the last component of the name, if it is a
	 *  directory.
	 */

	if (markedDir) {
		if (! makeDir(name)) {
			name[nameLen-1] = lastChar;
			return (0);
		}
	}

	name[nameLen-1] = (lastChar == '/') ? '\0' : lastChar;
	return (markedDir);
}

/*
 * resugname - Restore the user name and group name.  Search the NIS
 *             before using the uid and gid.
 *             (It is presumed that an archive entry cannot be
 *	       simultaneously a symlink and some other type.)
 */

static void
resugname(char *name,			/* name of the file to be modified */
	    int symflag)		/* true if file is a symbolic link */
{
	uid_t duid;
	gid_t dgid;
	struct stat *sp = &stbuf;
	char	*u_g_name;

	if (checkflag == 1) { /* Extended tar format and euid == 0 */

		/*
		 * Try and extract the intended uid and gid from the name
		 * service before believing the uid and gid in the header.
		 *
		 * In the case where we archived a setuid or setgid file
		 * owned by someone with a large uid, then it will
		 * have made it into the archive with a uid of nobody.  If
		 * the corresponding username doesn't appear to exist, then we
		 * want to make sure it *doesn't* end up as setuid nobody!
		 *
		 * Our caller will print an error message about the fact
		 * that the restore didn't work out quite right ..
		 */
		if (xhdr_flgs & _X_UNAME)
			u_g_name = Xtarhdr.x_uname;
		else
			u_g_name = dblock.dbuf.uname;
		if ((duid = getuidbyname(u_g_name)) == -1) {
			if (S_ISREG(sp->st_mode) && sp->st_uid == UID_NOBODY &&
			    (sp->st_mode & S_ISUID) == S_ISUID)
				(void) chmod(name,
					MODEMASK & sp->st_mode & ~S_ISUID);
			duid = sp->st_uid;
		}

		/* (Ditto for gids) */

		if (xhdr_flgs & _X_GNAME)
			u_g_name = Xtarhdr.x_gname;
		else
			u_g_name = dblock.dbuf.gname;
		if ((dgid = getgidbyname(u_g_name)) == -1) {
			if (S_ISREG(sp->st_mode) && sp->st_gid == GID_NOBODY &&
			    (sp->st_mode & S_ISGID) == S_ISGID)
				(void) chmod(name,
					MODEMASK & sp->st_mode & ~S_ISGID);
			dgid = sp->st_gid;
		}
	} else if (checkflag == 2) { /* tar format and euid == 0 */
		duid = sp->st_uid;
		dgid = sp->st_gid;
	}
	if ((checkflag == 1) || (checkflag == 2))
	    if (symflag)
		(void) lchown(name, duid, dgid);
	    else
		(void) chown(name, duid, dgid);
}

/*ARGSUSED*/
static void
onintr(int sig)
{
	(void) signal(SIGINT, SIG_IGN);
	term++;
}

/*ARGSUSED*/
static void
onquit(int sig)
{
	(void) signal(SIGQUIT, SIG_IGN);
	term++;
}

/*ARGSUSED*/
static void
onhup(int sig)
{
	(void) signal(SIGHUP, SIG_IGN);
	term++;
}

static void
tomodes(struct stat *sp)
{
	uid_t uid;
	gid_t gid;

	bzero(dblock.dummy, TBLOCK);

	/*
	 * If the uid or gid is too large, we can't put it into
	 * the archive.  We could fail to put anything in the
	 * archive at all .. but most of the time the name service
	 * will save the day when we do a lookup at restore time.
	 *
	 * Instead we choose a "safe" uid and gid, and fix up whether
	 * or not the setuid and setgid bits are left set to extraction
	 * time.
	 */
	if (Eflag) {
		if ((u_long)(uid = sp->st_uid) > (u_long)OCTAL7CHAR) {
			xhdr_flgs |= _X_UID;
			Xtarhdr.x_uid = uid;
		}
		if ((u_long)(gid = sp->st_gid) > (u_long)OCTAL7CHAR) {
			xhdr_flgs |= _X_GID;
			Xtarhdr.x_gid = gid;
		}
		if (sp->st_size > TAR_OFFSET_MAX) {
			xhdr_flgs |= _X_SIZE;
			Xtarhdr.x_filesz = sp->st_size;
			(void) sprintf(dblock.dbuf.size, "%011" FMT_off_t_o,
			    (off_t)0);
		} else
			(void) sprintf(dblock.dbuf.size, "%011" FMT_off_t_o,
			    sp->st_size);
	} else {
		(void) sprintf(dblock.dbuf.size, "%011" FMT_off_t_o,
		    sp->st_size);
	}
	if ((u_long)(uid = sp->st_uid) > (u_long)OCTAL7CHAR)
		uid = UID_NOBODY;
	if ((u_long)(gid = sp->st_gid) > (u_long)OCTAL7CHAR)
		gid = GID_NOBODY;
	(void) sprintf(dblock.dbuf.gid, "%07lo", gid);
	(void) sprintf(dblock.dbuf.uid, "%07lo", uid);
	(void) sprintf(dblock.dbuf.mode, "%07lo", sp->st_mode & MODEMASK);
	(void) sprintf(dblock.dbuf.mtime, "%011lo", sp->st_mtime);
}

static	int
#ifdef	EUC
/*
 * Warning:  the result of this function depends whether 'char' is a
 * signed or unsigned data type.  This a source of potential
 * non-portability among heterogeneous systems.  It is retained here
 * for backward compatibility.
 */
checksum_signed(union hblock *dblockp)
#else
checksum(union hblock *dblockp)
#endif	/* EUC */
{
	int i;
	char *cp;

	for (cp = dblockp->dbuf.chksum;
	    cp < &dblockp->dbuf.chksum[sizeof (dblockp->dbuf.chksum)]; cp++)
		*cp = ' ';
	i = 0;
	for (cp = dblockp->dummy; cp < &(dblockp->dummy[TBLOCK]); cp++)
		i += *cp;
	return (i);
}

#ifdef	EUC
/*
 * Generate unsigned checksum, regardless of what C compiler is
 * used.  Survives in the face of arbitrary 8-bit clean filenames,
 * e.g., internationalized filenames.
 */
static int
checksum(union hblock *dblockp)
{
	unsigned i;
	unsigned char *cp;

	for (cp = (unsigned char *) dblockp->dbuf.chksum;
	    cp < (unsigned char *)
	    &(dblockp->dbuf.chksum[sizeof (dblockp->dbuf.chksum)]); cp++)
		*cp = ' ';
	i = 0;
	for (cp = (unsigned char *) dblockp->dummy;
	    cp < (unsigned char *) &(dblockp->dummy[TBLOCK]); cp++)
		i += *cp;

	return (i);
}
#endif	/* EUC */

/*
 * If the w flag is set, output the action to be taken and the name of the
 * file.  Perform the action if the user response is affirmative.
 */

static int
checkw(char c, char *name)
{
	if (wflag) {
		(void) fprintf(vfile, "%c ", c);
		if (vflag)
			longt(&stbuf, ' ');	/* do we have acl info here */
		(void) fprintf(vfile, "%s: ", name);
		if (response() == 'y') {
			return (1);
		}
		return (0);
	}
	return (1);
}

/*
 * When the F flag is set, exclude RCS and SCCS directories.  If F is set
 * twice, also exclude .o files, and files names errs, core, and a.out.
 */

static int
checkf(char *name, int mode, int howmuch)
{
	int l;

	if ((mode & S_IFMT) == S_IFDIR) {
		if ((strcmp(name, "SCCS") == 0) || (strcmp(name, "RCS") == 0))
			return (0);
		return (1);
	}
	if ((l = (int) strlen(name)) < 3)
		return (1);
	if (howmuch > 1 && name[l-2] == '.' && name[l-1] == 'o')
		return (0);
	if (howmuch > 1) {
		if (strcmp(name, "core") == 0 || strcmp(name, "errs") == 0 ||
		    strcmp(name, "a.out") == 0)
			return (0);
	}

	/* SHOULD CHECK IF IT IS EXECUTABLE */
	return (1);
}

static int
response(void)
{
	int c;

	c = getchar();
	if (c != '\n')
		while (getchar() != '\n');
	else c = 'n';
	return ((c >= 'A' && c <= 'Z') ? c + ('a'-'A') : c);
}

/* Has file been modified since being put into archive? If so, return > 0. */

static int
checkupdate(char *arg)
{
	char name[PATH_MAX+1];
	time_t	mtime;
	long nsecs;
	off_t seekp;
	static off_t	lookup(char *);

	rewind(tfile);
	if ((seekp = lookup(arg)) < 0)
		return (1);
	(void) fseek(tfile, seekp, 0);
	(void) fscanf(tfile, "%s %ld.%ld", name, &mtime, &nsecs);

	/*
	 * Unless nanoseconds were stored in the file, only use seconds for
	 * comparison of time.  Nanoseconds are stored when -E is specified.
	 */
	if (Eflag == 0)
		return (stbuf.st_mtime > mtime);

	if ((stbuf.st_mtime < mtime) ||
	    ((stbuf.st_mtime == mtime) && (stbuf.st_mtim.tv_nsec <= nsecs)))
		return (0);
	return (1);
}


/*
 *	newvol	get new floppy (or tape) volume
 *
 *	newvol();		resets tapepos and first to TRUE, prompts for
 *				for new volume, and waits.
 *	if dumping, end-of-file is written onto the tape.
 */

static void
newvol(void)
{
	int c;
	extern char *sys_errlist[];

	if (dumping) {
#ifdef DEBUG
		DEBUG("newvol called with 'dumping' set\n", 0, 0);
#endif
		putempty((blkcnt_t)2);	/* 2 EOT marks */
		closevol();
		flushtape();
		sync();
		tapepos = 0;
	} else
		first = TRUE;
	if (close(mt) != 0)
		vperror(2, gettext("close error"));
	mt = 0;
	(void) fprintf(stderr, gettext(
	    "tar: \007please insert new volume, then press RETURN."));
	(void) fseek(stdin, (off_t)0, 2);	/* scan over read-ahead */
	while ((c = getchar()) != '\n' && ! term)
		if (c == EOF)
			done(0);
	if (term)
		done(0);

	errno = 0;

	mt = (strcmp(usefile, "-") == 0) ?
	    dup(1) : open(usefile, dumping ? update : 0);
	if (mt < 0) {
		(void) fprintf(stderr, gettext(
		    "tar: cannot reopen %s (%s)\n"),
		    dumping ? gettext("output") : gettext("input"), usefile);

		(void) fprintf(stderr, "update=%d, usefile=%s, mt=%d, [%s]\n",
		    update, usefile, mt, sys_errlist[errno]);

		done(2);
	}
}

/*
 * Write a trailer portion to close out the current output volume.
 */

static void
closevol(void)
{
	if (mulvol) {
		/*
		 * blocklim does not count the 2 EOT marks;
		 * tapepos  does count the 2 EOT marks;
		 * therefore we need the +2 below.
		 */
		putempty(blocklim + (blkcnt_t)2 - tapepos);
	}
}

static void
done(int n)
{
	(void) unlink(tname);
	if (mt > 0) {
		if ((close(mt) != 0) || (fclose(stdout) != 0)) {
			perror(gettext("tar: close error"));
			exit(2);
		}
	}
	exit(n);
}

/*
 * Determine if s1 is a prefix portion of s2 (or the same as s2).
 */

static	int
is_prefix(char *s1, char *s2)
{
	while (*s1)
		if (*s1++ != *s2++)
			return (0);
	if (*s2)
		return (*s2 == '/');
	return (1);
}

/*
 * lookup and bsrch look through tfile entries to find a match for a name.
 * The name can be up to PATH_MAX bytes.  bsrch compares what it sees between
 * a pair of newline chars, so the buffer it uses must be long enough for
 * two lines:  name and modification time as well as period, newline and space.
 *
 * A kludge was added to bsrch to take care of matching on the first entry
 * in the file--there is no leading newline.  So, if we are reading from the
 * start of the file, read into byte two and set the first byte to a newline.
 * Otherwise, the first entry cannot be matched.
 *
 */

#define	N	(2 * (PATH_MAX + TIME_MAX_DIGITS + LONG_MAX_DIGITS + 3))
static	off_t
lookup(char *s)
{
	int i;
	off_t a;

	for (i = 0; s[i]; i++)
		if (s[i] == ' ')
			break;
	a = bsrch(s, i, low, high);
	return (a);
}

static off_t
bsrch(char *s, int n, off_t l, off_t h)
{
	int i, j;
	char b[N];
	off_t m, m1;


loop:
	if (l >= h)
		return ((off_t)-1);
	m = l + (h-l)/2 - N/2;
	if (m < l)
		m = l;
	(void) fseek(tfile, m, 0);
	if (m == 0) {
		(void) fread(b+1, 1, N-1, tfile);
		b[0] = '\n';
		m--;
	} else
		(void) fread(b, 1, N, tfile);
	for (i = 0; i < N; i++) {
		if (b[i] == '\n')
			break;
		m++;
	}
	if (m >= h)
		return ((off_t)-1);
	m1 = m;
	j = i;
	for (i++; i < N; i++) {
		m1++;
		if (b[i] == '\n')
			break;
	}
	i = cmp(b+j, s, n);
	if (i < 0) {
		h = m;
		goto loop;
	}
	if (i > 0) {
		l = m1;
		goto loop;
	}
	if (m < 0)
		m = 0;
	return (m);
}

static int
cmp(char *b, char *s, int n)
{
	int i;

	assert(b[0] == '\n');

	for (i = 0; i < n; i++) {
		if (b[i+1] > s[i])
			return (-1);
		if (b[i+1] < s[i])
			return (1);
	}
	return (b[i+1] == ' '? 0 : -1);
}


/*
 *	seekdisk	seek to next file on archive
 *
 *	called by passtape() only
 *
 *	WARNING: expects "nblock" to be set, that is, readtape() to have
 *		already been called.  Since passtape() is only called
 *		after a file header block has been read (why else would
 *		we skip to next file?), this is currently safe.
 *
 *	changed to guarantee SYS_BLOCK boundary
 */

static void
seekdisk(blkcnt_t blocks)
{
	off_t seekval;
#if SYS_BLOCK > TBLOCK
	/* handle non-multiple of SYS_BLOCK */
	blkcnt_t nxb;	/* # extra blocks */
#endif

	tapepos += blocks;
#ifdef DEBUG
	DEBUG("seekdisk(%" FMT_blkcnt_t ") called\n", blocks, 0);
#endif
	if (recno + blocks <= nblock) {
		recno += blocks;
		return;
	}
	if (recno > nblock)
		recno = nblock;
	seekval = (off_t) blocks - (nblock - recno);
	recno = nblock;	/* so readtape() reads next time through */
#if SYS_BLOCK > TBLOCK
	nxb = (blkcnt_t) (seekval % (off_t)(SYS_BLOCK / TBLOCK));
#ifdef DEBUG
	DEBUG("xtrablks=%" FMT_blkcnt_t " seekval=%" FMT_blkcnt_t " blks\n",
	    nxb, seekval);
#endif
	if (nxb && nxb > seekval) /* don't seek--we'll read */
		goto noseek;
	seekval -=  nxb;	/* don't seek quite so far */
#endif
	if (lseek(mt, (off_t) (TBLOCK * seekval), 1) == (off_t) -1) {
		(void) fprintf(stderr, gettext(
		    "tar: device seek error\n"));
		done(3);
	}
#if SYS_BLOCK > TBLOCK
	/* read those extra blocks */
noseek:
	if (nxb) {
#ifdef DEBUG
		DEBUG("reading extra blocks\n", 0, 0);
#endif
		if (read(mt, tbuf, TBLOCK*nblock) < 0) {
			(void) fprintf(stderr, gettext(
			    "tar: read error while skipping file\n"));
			done(8);
		}
		recno = nxb;	/* so we don't read in next readtape() */
	}
#endif
}

static void
readtape(char *buffer)
{
	int i, j;

	++tapepos;
	if (recno >= nblock || first) {
		if (first) {
			/*
			 * set the number of blocks to
			 * read initially.
			 * very confusing!
			 */
			if (bflag)
				j = nblock;
			else if (NotTape)
				j = NBLOCK;
			else if (defaults_used)
				j = nblock;
			else
				j = NBLOCK;
		} else
			j = nblock;

		if ((i = read(mt, tbuf, TBLOCK*j)) < 0) {
			(void) fprintf(stderr, gettext(
			    "tar: tape read error\n"));
			done(3);
		} else if ((!first || Bflag) && i != TBLOCK*j) {
			/*
			 * Short read - try to get the remaining bytes.
			 */

			int remaining = (TBLOCK * j) - i;
			char *b = (char *)tbuf + i;
			int r;

			do {
				if ((r = read(mt, b, remaining)) < 0) {
					(void) fprintf(stderr,
					    gettext("tar: tape read error\n"));
					done(3);
				}
				b += r;
				remaining -= r;
				i += r;
			} while (remaining > 0 && r != 0);
		}
		if (first) {
			if ((i % TBLOCK) != 0) {
				(void) fprintf(stderr, gettext(
				    "tar: tape blocksize error\n"));
				done(3);
			}
			i /= TBLOCK;
			if (vflag && i != nblock && i != 1) {
				if (!NotTape)
					(void) fprintf(stderr, gettext(
					    "tar: blocksize = %d\n"), i);
			}

			/*
			 * If we are reading a tape, then a short read is
			 * understood to signify that the amount read is
			 * the tape's actual blocking factor.  We adapt
			 * nblock accordingly.  There is no reason to do
			 * this when the device is not blocked.
			 */

			if (!NotTape)
				nblock = i;
		}
		recno = 0;
	}

	first = FALSE;
	copy(buffer, &tbuf[recno++]);
}


/*
 * replacement for writetape.
 */

static int
writetbuf(char *buffer, int n)
{
	int i;

	tapepos += n;		/* output block count */

	if (recno >= nblock) {
		i = write(mt, (char *)tbuf, TBLOCK*nblock);
		if (i != TBLOCK*nblock)
			mterr("write", i, 2);
		recno = 0;
	}

	/*
	 *  Special case:  We have an empty tape buffer, and the
	 *  users data size is >= the tape block size:  Avoid
	 *  the bcopy and dma direct to tape.  BIG WIN.  Add the
	 *  residual to the tape buffer.
	 */
	while (recno == 0 && n >= nblock) {
		i = (int) write(mt, buffer, TBLOCK*nblock);
		if (i != TBLOCK*nblock)
			mterr("write", i, 2);
		n -= nblock;
		buffer += (nblock * TBLOCK);
	}

	while (n-- > 0) {
		(void) memcpy((char *)&tbuf[recno++], buffer, TBLOCK);
		buffer += TBLOCK;
		if (recno >= nblock) {
			i = (int) write(mt, (char *)tbuf, TBLOCK*nblock);
			if (i != TBLOCK*nblock)
				mterr("write", i, 2);
			recno = 0;
		}
	}

	/* Tell the user how much to write to get in sync */
	return (nblock - recno);
}

/*
 *	backtape - reposition tape after reading soft "EOF" record
 *
 *	Backtape tries to reposition the tape back over the EOF
 *	record.  This is for the -u and -r options so that the
 *	tape can be extended.  This code is not well designed, but
 *	I'm confident that the only callers who care about the
 *	backspace-over-EOF feature are those involved in -u and -r.
 *
 *	The proper way to backup the tape is through the use of mtio.
 *	Earlier spins used lseek combined with reads in a confusing
 *	maneuver that only worked on 4.x, but shouldn't have, even
 *	there.  Lseeks are explicitly not supported for tape devices.
 */

static void
backtape(void)
{
	struct mtop mtcmd;
#ifdef DEBUG
	DEBUG("backtape() called, recno=%" FMT_blkcnt_t " nblock=%d\n", recno,
	    nblock);
#endif
	/*
	 * Backup to the position in the archive where the record
	 * currently sitting in the tbuf buffer is situated.
	 */

	if (NotTape) {
		/*
		 * For non-tape devices, this means lseeking to the
		 * correct position.  The absolute location tapepos-recno
		 * should be the beginning of the current record.
		 */

		if (lseek(mt, (off_t) (TBLOCK*(tapepos-recno)), SEEK_SET) ==
		    (off_t) -1) {
			(void) fprintf(stderr,
			    gettext("tar: lseek to end of archive failed\n"));
			done(4);
		}
	} else {
		/*
		 * For tape devices, we backup over the most recently
		 * read record.
		 */

		mtcmd.mt_op = MTBSR;
		mtcmd.mt_count = 1;

		if (ioctl(mt, MTIOCTOP, &mtcmd) < 0) {
			(void) fprintf(stderr,
				gettext("tar: backspace over record failed\n"));
			done(4);
		}
	}

	/*
	 * Decrement the tape and tbuf buffer indices to prepare for the
	 * coming write to overwrite the soft EOF record.
	 */

	recno--;
	tapepos--;
}


/*
 *	flushtape  write buffered block(s) onto tape
 *
 *      recno points to next free block in tbuf.  If nonzero, a write is done.
 *	Care is taken to write in multiples of SYS_BLOCK when device is
 *	non-magtape in case raw i/o is used.
 *
 *	NOTE: this is called by writetape() to do the actual writing
 */

static void
flushtape(void)
{
#ifdef DEBUG
	DEBUG("flushtape() called, recno=%" FMT_blkcnt_t "\n", recno, 0);
#endif
	if (recno > 0) {	/* anything buffered? */
		if (NotTape) {
#if SYS_BLOCK > TBLOCK
			int i;

			/*
			 * an odd-block write can only happen when
			 * we are at the end of a volume that is not a tape.
			 * Here we round recno up to an even SYS_BLOCK
			 * boundary.
			 */
			if ((i = recno % (SYS_BLOCK / TBLOCK)) != 0) {
#ifdef DEBUG
				DEBUG("flushtape() %d rounding blocks\n", i, 0);
#endif
				recno += i;	/* round up to even SYS_BLOCK */
			}
#endif
			if (recno > nblock)
				recno = nblock;
		}
#ifdef DEBUG
		DEBUG("writing out %" FMT_blkcnt_t " blocks of %" FMT_blkcnt_t
		    " bytes\n", (blkcnt_t)(NotTape ? recno : nblock),
		    (blkcnt_t)(NotTape ? recno : nblock) * TBLOCK);
#endif
		if (write(mt, tbuf,
		    (size_t)(NotTape ? recno : nblock) * TBLOCK) < 0) {
			(void) fprintf(stderr, gettext(
			    "tar: tape write error\n"));
			done(2);
		}
		recno = 0;
	}
}

static void
copy(void *dst, void *src)
{
	char *to = (char *)dst;
	char *from = (char *)src;
	int i;

	i = TBLOCK;
	do {
		*to++ = *from++;
	} while (--i);
}

#ifdef	_iBCS2
/*
 *	initarg -- initialize things for nextarg.
 *
 *	argv		filename list, a la argv.
 *	filefile	name of file containing filenames.  Unless doing
 *		a create, seeks must be allowable (e.g. no named pipes).
 *
 *	- if filefile is non-NULL, it will be used first, and argv will
 *	be used when the data in filefile are exhausted.
 *	- otherwise argv will be used.
 */
static char **Cmdargv = NULL;
static FILE *FILEFile = NULL;
static long seekFile = -1;
static char *ptrtoFile, *begofFile, *endofFile;

static	void
initarg(char *argv[], char *filefile)
{
	struct stat statbuf;
	char *p;
	int nbytes;

	Cmdargv = argv;
	if (filefile == NULL)
		return;		/* no -F file */
	if (FILEFile != NULL) {
		/*
		 * need to REinitialize
		 */
		if (seekFile != -1)
			(void) fseek(FILEFile, seekFile, 0);
		ptrtoFile = begofFile;
		return;
	}
	/*
	 * first time initialization
	 */
	if ((FILEFile = fopen(filefile, "r")) == NULL)
		fatal(gettext("cannot open (%s)"), filefile);
	(void) fstat(fileno(FILEFile), &statbuf);
	if ((statbuf.st_mode & S_IFMT) != S_IFREG) {
		(void) fprintf(stderr, gettext(
			"tar: %s is not a regular file\n"), filefile);
		(void) fclose(FILEFile);
		done(1);
	}
	ptrtoFile = begofFile = endofFile;
	seekFile = 0;
	if (!xflag)
		return;		/* the file will be read only once anyway */
	nbytes = statbuf.st_size;
	while ((begofFile = calloc(nbytes, sizeof (char))) == NULL)
		nbytes -= 20;
	if (nbytes < 50) {
		free(begofFile);
		begofFile = endofFile;
		return;		/* no room so just do plain reads */
	}
	if (fread(begofFile, 1, nbytes, FILEFile) != nbytes)
		fatal(gettext("could not read %s"), filefile);
	ptrtoFile = begofFile;
	endofFile = begofFile + nbytes;
	for (p = begofFile; p < endofFile; ++p)
		if (*p == '\n')
			*p = '\0';
	if (nbytes != statbuf.st_size)
		seekFile = nbytes + 1;
	else
		(void) fclose(FILEFile);
}

/*
 *	nextarg -- get next argument of arglist.
 *
 *	The argument is taken from wherever is appropriate.
 *
 *	If the 'F file' option has been specified, the argument will be
 *	taken from the file, unless EOF has been reached.
 *	Otherwise the argument will be taken from argv.
 *
 *	WARNING:
 *	  Return value may point to static data, whose contents are over-
 *	  written on each call.
 */
static	char  *
nextarg(void)
{
	static char nameFile[PATH_MAX + 1];
	int n;
	char *p;

	if (FILEFile) {
		if (ptrtoFile < endofFile) {
			p = ptrtoFile;
			while (*ptrtoFile)
				++ptrtoFile;
			++ptrtoFile;
			return (p);
		}
		if (fgets(nameFile, PATH_MAX + 1, FILEFile) != NULL) {
			n = strlen(nameFile);
			if (n > 0 && nameFile[n-1] == '\n')
				nameFile[n-1] = '\0';
			return (nameFile);
		}
	}
	return (*Cmdargv++);
}
#endif	/*  _iBCS2 */

/*
 * kcheck()
 *	- checks the validity of size values for non-tape devices
 *	- if size is zero, mulvol tar is disabled and size is
 *	  assumed to be infinite.
 *	- returns volume size in TBLOCKS
 */

static blkcnt_t
kcheck(char *kstr)
{
	blkcnt_t kval;

	kval = strtoll(kstr, NULL, 0);
	if (kval == (blkcnt_t)0) {	/* no multi-volume; size is infinity. */
		mulvol = 0;	/* definitely not mulvol, but we must  */
		return (0);	/* took out setting of NotTape */
	}
	if (kval < (blkcnt_t) MINSIZE) {
		(void) fprintf(stderr, gettext(
		    "tar: sizes below %luK not supported (%" FMT_blkcnt_t
		    ").\n"), (ulong) MINSIZE, kval);
		if (!kflag)
			(void) fprintf(stderr, gettext(
			    "bad size entry for %s in %s.\n"),
			    archive, DEF_FILE);
		done(1);
	}
	mulvol++;
	NotTape++;			/* implies non-tape */
	return (kval * 1024 / TBLOCK);	/* convert to TBLOCKS */
}


/*
 * bcheck()
 *	- checks the validity of blocking factors
 *	- returns blocking factor
 */

static int
bcheck(char *bstr)
{
	blkcnt_t bval;

	bval = strtoll(bstr, NULL, 0);
	if ((bval <= 0) || (bval > INT_MAX / TBLOCK)) {
		(void) fprintf(stderr, gettext(
		    "tar: invalid blocksize \"%s\".\n"), bstr);
		if (!bflag)
			(void) fprintf(stderr, gettext(
			    "bad blocksize entry for '%s' in %s.\n"),
			    archive, DEF_FILE);
		done(1);
	}

	return ((int)bval);
}


/*
 * defset()
 *	- reads DEF_FILE for the set of default values specified.
 *	- initializes 'usefile', 'nblock', and 'blocklim', and 'NotTape'.
 *	- 'usefile' points to static data, so will be overwritten
 *	  if this routine is called a second time.
 *	- the pattern specified by 'arch' must be followed by four
 *	  blank-separated fields (1) device (2) blocking,
 *				 (3) size(K), and (4) tape
 *	  for example: archive0=/dev/fd 1 400 n
 */

static int
defset(char *arch)
{
	char *bp;

	if (defopen(DEF_FILE) != 0)
		return (FALSE);
	if (defcntl(DC_SETFLAGS, (DC_STD & ~(DC_CASE))) == -1) {
		(void) fprintf(stderr, gettext(
		    "tar: error setting parameters for %s.\n"), DEF_FILE);
		return (FALSE);			/* & following ones too */
	}
	if ((bp = defread(arch)) == NULL) {
		(void) fprintf(stderr, gettext(
		    "tar: missing or invalid '%s' entry in %s.\n"),
				arch, DEF_FILE);
		return (FALSE);
	}
	if ((usefile = strtok(bp, " \t")) == NULL) {
		(void) fprintf(stderr, gettext(
		    "tar: '%s' entry in %s is empty!\n"), arch, DEF_FILE);
		return (FALSE);
	}
	if ((bp = strtok(NULL, " \t")) == NULL) {
		(void) fprintf(stderr, gettext(
		    "tar: block component missing in '%s' entry in %s.\n"),
		    arch, DEF_FILE);
		return (FALSE);
	}
	nblock = bcheck(bp);
	if ((bp = strtok(NULL, " \t")) == NULL) {
		(void) fprintf(stderr, gettext(
		    "tar: size component missing in '%s' entry in %s.\n"),
		    arch, DEF_FILE);
		return (FALSE);
	}
	blocklim = kcheck(bp);
	if ((bp = strtok(NULL, " \t")) != NULL)
		NotTape = (*bp == 'n' || *bp == 'N');
	else
		NotTape = (blocklim != 0);
	(void) defopen(NULL);
#ifdef DEBUG
	DEBUG("defset: archive='%s'; usefile='%s'\n", arch, usefile);
	DEBUG("defset: nblock='%d'; blocklim='%" FMT_blkcnt_t "'\n",
	    nblock, blocklim);
	DEBUG("defset: not tape = %d\n", NotTape, 0);
#endif
	return (TRUE);
}


/*
 * Following code handles excluded and included files.
 * A hash table of file names to be {in,ex}cluded is built.
 * For excluded files, before writing or extracting a file
 * check to see if it is in the exclude_tbl.
 * For included files, the wantit() procedure will check to
 * see if the named file is in the include_tbl.
 */

static void
build_table(struct file_list *table[], char *file)
{
	FILE	*fp;
	char	buf[PATH_MAX + 1];

	if ((fp = fopen(file, "r")) == (FILE *)NULL)
		vperror(1, gettext("could not open %s"), file);
	while (fgets(buf, sizeof (buf), fp) != NULL) {
		buf[strlen(buf) - 1] = '\0';
		add_file_to_table(table, buf);
	}
	(void) fclose(fp);
}


/*
 * Add a file name to the the specified table, if the file name has any
 * trailing '/'s then delete them before inserting into the table
 */

static void
add_file_to_table(struct file_list *table[], char *str)
{
	char	name[PATH_MAX + 1];
	unsigned int h;
	struct	file_list	*exp;

	(void) strcpy(name, str);
	while (name[strlen(name) - 1] == '/') {
		name[strlen(name) - 1] = NULL;
	}

	h = hash(name);
	if ((exp = (struct file_list *) calloc(sizeof (struct file_list),
	    sizeof (char))) == NULL) {
		(void) fprintf(stderr, gettext(
		    "tar: out of memory, exclude/include table(entry)\n"));
		exit(1);
	}

	if ((exp->name = strdup(name)) == NULL) {
		(void) fprintf(stderr, gettext(
		    "tar: out of memory, exclude/include table(file name)\n"));
		exit(1);
	}

	exp->next = table[h];
	table[h] = exp;
}


/*
 * See if a file name or any of the file's parent directories is in the
 * specified table, if the file name has any trailing '/'s then delete
 * them before searching the table
 */

static int
is_in_table(struct file_list *table[], char *str)
{
	char	name[PATH_MAX + 1];
	unsigned int	h;
	struct	file_list	*exp;
	char	*ptr;

	(void) strcpy(name, str);
	while (name[strlen(name) - 1] == '/') {
		name[strlen(name) - 1] = NULL;
	}

	/*
	 * check for the file name in the passed list
	 */
	h = hash(name);
	exp = table[h];
	while (exp != NULL) {
		if (strcmp(name, exp->name) == 0) {
			return (1);
		}
		exp = exp->next;
	}

	/*
	 * check for any parent directories in the file list
	 */
	while ((ptr = strrchr(name, '/'))) {
		*ptr = NULL;
		h = hash(name);
		exp = table[h];
		while (exp != NULL) {
			if (strcmp(name, exp->name) == 0) {
				return (1);
			}
			exp = exp->next;
		}
	}

	return (0);
}


/*
 * Compute a hash from a string.
 */

static unsigned int
hash(char *str)
{
	char	*cp;
	unsigned int	h;

	h = 0;
	for (cp = str; *cp; cp++) {
		h += *cp;
	}
	return (h % TABLE_SIZE);
}


static	void *
getmem(size_t size)
{
	void *p = calloc((unsigned) size, sizeof (char));

	if (p == NULL && freemem) {
		(void) fprintf(stderr, gettext(
		    "tar: out of memory, link and directory modtime "
		    "info lost\n"));
		freemem = 0;
		if (errflag)
			done(1);
	}
	return (p);
}


/*
 * vperror() --variable argument perror.
 * Takes 3 args: exit_status, formats, args.  If exit status is 0, then
 * the eflag (exit on error) is checked -- if it is non-zero, tar exits
 * with the value of whatever "errno" is set to.  If exit_status is not
 * zero, then it exits with that error status. If eflag and exit_status
 * are both zero, the routine returns to where it was called.
 */

static void
vperror(int exit_status, char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	(void) fputs("tar: ", stderr);
	(void) vfprintf(stderr, fmt, ap);
	(void) fprintf(stderr, ": %s\n", strerror(errno));
	va_end(ap);
	if (exit_status)
		done(exit_status);
	else if (errflag)
		done(errno);
}


static void
fatal(char *format, ...)
{
	va_list	ap;

	va_start(ap, format);
	(void) fprintf(stderr, "tar: ");
	(void) vfprintf(stderr, format, ap);
	(void) fprintf(stderr, "\n");
	va_end(ap);
	done(1);
}


/*
 * Check to make sure that argument is a char * ptr.
 * Actually, we just check to see that it is non-null.
 * If it is null, print out the message and call usage(), bailing out.
 */

static void
assert_string(char *s, char *msg)
{
	if (s == NULL) {
		(void) fprintf(stderr, msg);
		usage();
	}
}


static void
mterr(char *operation, int i, int exitcode)
{
	fprintf(stderr, gettext(
	    "tar: %s error: "), operation);
	if (i < 0)
		perror("");
	else
		fprintf(stderr, gettext("unexpected EOF\n"));
	done(exitcode);
}

static int
wantit(char *argv[], char **namep)
{
	char **cp;
	int gotit;		/* true if we've found a match */

	xhdr_flgs = 0;
	getdir();
	if (Xhdrflag > 0) {
		if (get_xdata() != 0) {	/* Xhdr items and regular header */
			passtape();
			return (0);	/* Error--don't want to extract  */
		}
	}
	check_prefix(namep);	/* sets *namep to point at the proper */
				/* name  */
	if (endtape()) {
		if (Bflag) {
			/*
			 * Logically at EOT - consume any extra blocks
			 * so that write to our stdin won't fail and
			 * emit an error message; otherwise something
			 * like "dd if=foo.tar | (cd bar; tar xvf -)"
			 * will produce a bogus error message from "dd".
			 */

			while (read(mt, tbuf, TBLOCK*nblock) > 0) {
				/* empty body */
			}
		}
		return (-1);
	}

	gotit = 0;

	if ((Iflag && is_in_table(include_tbl, *namep)) ||
	    (! Iflag && *argv == NULL)) {
		gotit = 1;
	} else {
		for (cp = argv; *cp; cp++) {
			if (is_prefix(*cp, *namep)) {
				gotit = 1;
				break;
			}
		}
	}

	if (! gotit) {
		passtape();
		return (0);
	}

	if (Xflag && is_in_table(exclude_tbl, *namep)) {
		if (vflag) {
			fprintf(stderr, gettext("%s excluded\n"),
			    *namep);
		}
		passtape();
		return (0);
	}

	return (1);
}


/*
 *  Return through *namep a pointer to the proper fullname (i.e  "<name> |
 *  <prefix>/<name>"), as represented in the header entry dblock.dbuf.
 */

static void
check_prefix(char **namep)
{
	static char fullname[PATH_MAX + 1];

	if (xhdr_flgs & _X_PATH)
		(void) strcpy(fullname, Xtarhdr.x_path);
	else {
		if (dblock.dbuf.prefix[0] != '\0')
			(void) sprintf(fullname, "%.*s/%.*s", PRESIZ,
			    dblock.dbuf.prefix, NAMSIZ, dblock.dbuf.name);
		else
			(void) sprintf(fullname, "%.*s", NAMSIZ,
			    dblock.dbuf.name);
	}
	*namep = fullname;
}


static wchar_t
yesnoresponse(void)
{
	wchar_t c;

	c = getwchar();
	if (c != '\n')
		while (getwchar() != '\n');
	else c = 0;
	return (c);
}


/*
 * Return true if the object indicated by the file descriptor and type
 * is a tape device, false otherwise
 */

static int
istape(int fd, int type)
{
	int result = 0;

	if (type & S_IFCHR) {
		struct mtget mtg;

		if (ioctl(fd, MTIOCGET, &mtg) != -1) {
			result = 1;
		}
	}

	return (result);
}

#include <utmp.h>

struct	utmp utmp;

#define	NMAX	(sizeof (utmp.ut_name))

typedef struct cachenode {	/* this struct must be zeroed before using */
	struct cachenode *next;	/* next in hash chain */
	int val;		/* the uid or gid of this entry */
	int namehash;		/* name's hash signature */
	char name[NMAX+1];	/* the string that val maps to */
} cachenode_t;

#define	HASHSIZE	256

static cachenode_t *names[HASHSIZE];
static cachenode_t *groups[HASHSIZE];
static cachenode_t *uids[HASHSIZE];
static cachenode_t *gids[HASHSIZE];

static int
hash_byname(char *name)
{
	int i, c, h = 0;

	for (i = 0; i < NMAX; i++) {
		c = name[i];
		if (c == '\0')
			break;
		h = (h << 4) + h + c;
	}
	return (h);
}

static cachenode_t *
hash_lookup_byval(cachenode_t *table[], int val)
{
	int h = val;
	cachenode_t *c;

	for (c = table[h & (HASHSIZE - 1)]; c != NULL; c = c->next) {
		if (c->val == val)
			return (c);
	}
	return (NULL);
}

static cachenode_t *
hash_lookup_byname(cachenode_t *table[], char *name)
{
	int h = hash_byname(name);
	cachenode_t *c;

	for (c = table[h & (HASHSIZE - 1)]; c != NULL; c = c->next) {
		if (c->namehash == h && strcmp(c->name, name) == 0)
			return (c);
	}
	return (NULL);
}

static cachenode_t *
hash_insert(cachenode_t *table[], char *name, int value)
{
	cachenode_t *c;
	int signature;

	c = calloc(1, sizeof (cachenode_t));
	if (c == NULL) {
		perror("malloc");
		exit(1);
	}
	if (name != NULL) {
		strncpy(c->name, name, NMAX);
		c->namehash = hash_byname(name);
	}
	c->val = value;
	if (table == uids || table == gids)
		signature = c->val;
	else
		signature = c->namehash;
	c->next = table[signature & (HASHSIZE - 1)];
	table[signature & (HASHSIZE - 1)] = c;
	return (c);
}

static char *
getname(uid_t uid)
{
	cachenode_t *c;

	if ((c = hash_lookup_byval(uids, uid)) == NULL) {
		struct passwd *pwent = getpwuid(uid);
		c = hash_insert(uids, pwent ? pwent->pw_name : NULL, uid);
	}
	return (c->name);
}

static char *
getgroup(gid_t gid)
{
	cachenode_t *c;

	if ((c = hash_lookup_byval(gids, gid)) == NULL) {
		struct group *grent = getgrgid(gid);
		c = hash_insert(gids, grent ? grent->gr_name : NULL, gid);
	}
	return (c->name);
}

static uid_t
getuidbyname(char *name)
{
	cachenode_t *c;

	if ((c = hash_lookup_byname(names, name)) == NULL) {
		struct passwd *pwent = getpwnam(name);
		c = hash_insert(names, name, pwent ? (int)pwent->pw_uid : -1);
	}
	return ((uid_t)c->val);
}

static gid_t
getgidbyname(char *group)
{
	cachenode_t *c;

	if ((c = hash_lookup_byname(groups, group)) == NULL) {
		struct group *grent = getgrnam(group);
		c = hash_insert(groups, group, grent ? (int)grent->gr_gid : -1);
	}
	return ((gid_t)c->val);
}

/*
 * Build the header.
 * Determine whether or not an extended header is also needed.  If needed,
 * create and write the extended header and its data.
 * Writing of the extended header assumes that "tomodes" has been called and
 * the relevant information has been placed in the header block.
 */

static int
build_dblock(
	const char		*name,
	const char		*linkname,
	const char		typeflag,
	const struct stat	*sp,
	const dev_t		device,
	const char		*prefix)
{
	int nblks;
	major_t		dev;
	const char	*filename;
	const char	*lastslash;
	int		i;

	dblock.dbuf.typeflag = typeflag;
	memset(dblock.dbuf.name, '\0', NAMSIZ);
	memset(dblock.dbuf.linkname, '\0', NAMSIZ);
	memset(dblock.dbuf.prefix, '\0', PRESIZ);

	if (xhdr_flgs & _X_PATH)
		filename = Xtarhdr.x_path;
	else
		filename = name;

	if ((dev = major(device)) > OCTAL7CHAR) {
		if (Eflag) {
			xhdr_flgs |= _X_DEVMAJOR;
			Xtarhdr.x_devmajor = dev;
		} else {
			fprintf(stderr, gettext(
			    "Device major too large for %s.  Use -E flag."),
			    filename);
			if (errflag)
				done(1);
		}
		dev = 0;
	}
	(void) sprintf(dblock.dbuf.devmajor, "%07lo", dev);
	if ((dev = minor(device)) > OCTAL7CHAR) {
		if (Eflag) {
			xhdr_flgs |= _X_DEVMINOR;
			Xtarhdr.x_devminor = dev;
		} else {
			fprintf(stderr, gettext(
			    "Device minor too large for %s.  Use -E flag."),
			    filename);
			if (errflag)
				done(1);
		}
		dev = 0;
	}
	(void) sprintf(dblock.dbuf.devminor, "%07lo", dev);

	i = strlen(name);
	(void) memcpy(dblock.dbuf.name, name, min(i, NAMSIZ));
	i = strlen(linkname);
	(void) memcpy(dblock.dbuf.linkname, linkname,
		min(i, NAMSIZ));
	(void) sprintf(dblock.dbuf.magic, "%.5s", magic_type);
	(void) sprintf(dblock.dbuf.version, "00");
	(void) sprintf(dblock.dbuf.uname, "%.31s", getname(sp->st_uid));
	(void) sprintf(dblock.dbuf.gname, "%.31s", getgroup(sp->st_gid));
	i = strlen(prefix);
	(void) memcpy(dblock.dbuf.prefix, prefix, min(i, PRESIZ));
	(void) sprintf(dblock.dbuf.chksum, "%07o", checksum(&dblock));

	if (Eflag) {
		bcopy(dblock.dummy, xhdr_buf.dummy, TBLOCK);
		memset(xhdr_buf.dbuf.name, '\0', NAMSIZ);
		lastslash = strrchr(name, '/');
		if (lastslash == NULL)
			lastslash = name;
		else
			lastslash++;
		strcpy(xhdr_buf.dbuf.name, lastslash);
		memset(xhdr_buf.dbuf.linkname, '\0', NAMSIZ);
		memset(xhdr_buf.dbuf.prefix, '\0', PRESIZ);
		strcpy(xhdr_buf.dbuf.prefix, xhdr_dirname);
		xhdr_count++;
		xrec_offset = 0;
		gen_date("mtime", sp->st_mtim);
		xhdr_buf.dbuf.typeflag = 'X';
		if (gen_utf8_names(filename) != 0)
			return (1);

#ifdef XHDR_DEBUG
		Xtarhdr.x_uname = dblock.dbuf.uname;
		Xtarhdr.x_gname = dblock.dbuf.gname;
		xhdr_flgs |= (_X_UNAME | _X_GNAME);
#endif
		if (xhdr_flgs) {
			if (xhdr_flgs & _X_DEVMAJOR)
				gen_num("SUN.devmajor", Xtarhdr.x_devmajor);
			if (xhdr_flgs & _X_DEVMINOR)
				gen_num("SUN.devminor", Xtarhdr.x_devminor);
			if (xhdr_flgs & _X_GID)
				gen_num("gid", Xtarhdr.x_gid);
			if (xhdr_flgs & _X_UID)
				gen_num("uid", Xtarhdr.x_uid);
			if (xhdr_flgs & _X_SIZE)
				gen_num("size", Xtarhdr.x_filesz);
			if (xhdr_flgs & _X_PATH)
				gen_string("path", Xtarhdr.x_path);
			if (xhdr_flgs & _X_LINKPATH)
				gen_string("linkpath", Xtarhdr.x_linkpath);
			if (xhdr_flgs & _X_GNAME)
				gen_string("gname", Xtarhdr.x_gname);
			if (xhdr_flgs & _X_UNAME)
				gen_string("uname", Xtarhdr.x_uname);
		}
		sprintf(xhdr_buf.dbuf.size, "%011" FMT_off_t_o, xrec_offset);
		(void) sprintf(xhdr_buf.dbuf.chksum, "%07o",
		    checksum(&xhdr_buf));
		(void) writetape((char *)&xhdr_buf);
		nblks = TBLOCKS(xrec_offset);
		(void) writetbuf(xrec_ptr, nblks);
	}
	return (0);
}


/*
 *  makeDir - ensure that a directory with the pathname denoted by name
 *            exists, and return 1 on success, and 0 on failure (e.g.,
 *	      read-only file system, exists but not-a-directory).
 */

static int
makeDir(char *name)
{
	struct stat buf;

	if (access(name, 0) < 0) {  /* name doesn't exist */
		if (mkdir(name, 0777) < 0) {
			vperror(0, "%s", name);
			return (0);
		}

		if (!oflag)
		    resugname(name, 0);
	} else {		   /* name exists */
		if (stat(name, &buf) < 0) {
			vperror(0, "%s", name);
			return (0);
		}

		return ((buf.st_mode & S_IFMT) == S_IFDIR);
	}

	return (1);
}


/*
 * Save this directory and its mtime on the stack, popping and setting
 * the mtimes of any stacked dirs which aren't parents of this one.
 * A null name causes the entire stack to be unwound and set.
 *
 * Since all the elements of the directory "stack" share a common
 * prefix, we can make do with one string.  We keep only the current
 * directory path, with an associated array of mtime's, one for each
 * '/' in the path.  A negative mtime means no mtime.  The mtime's are
 * offset by one (first index 1, not 0) because calling this with a null
 * name causes mtime[0] to be set.
 *
 * This stack algorithm is not guaranteed to work for tapes created
 * with the 'r' option, but the vast majority of tapes with
 * directories are not.  This avoids saving every directory record on
 * the tape and setting all the times at the end.
 *
 * (This was borrowed from the 4.1.3 source, and adapted to the 5.x
 *  environment)
 */

#define	NTIM	(PATH_MAX/2+1)		/* a/b/c/d/... */

static void
doDirTimes(char *name, timestruc_t modTime)
{
	static char dirstack[PATH_MAX+1]; /* Add space for last NULL */
	static timestruc_t	modtimes[NTIM];

	char *p = dirstack;
	char *q = name;
	int ndir = 0;
	char *savp;
	int savndir;

	if (q) {
		/*
		 * Find common prefix
		 */

		while (*p == *q && *p) {
			if (*p++ == '/')
			    ++ndir;
			q++;
		}
	}

	savp = p;
	savndir = ndir;

	while (*p) {
		/*
		 * Not a child: unwind the stack, setting the times.
		 * The order we do this doesn't matter, so we go "forward."
		 */

		if (*p++ == '/')
			if (modtimes[++ndir].tv_sec >= 0) {
				*--p = '\0';	 /* zap the slash */
				setPathTimes(dirstack, modtimes[ndir]);
				*p++ = '/';
			}
	}

	p = savp;
	ndir = savndir;

	/*
	 *  Push this one on the "stack"
	 */

	if (q) {
		while ((*p = *q++)) {	/* append the rest of the new dir */
			if (*p++ == '/')
				modtimes[++ndir].tv_sec = -1;
		}
	}

	modtimes[ndir].tv_sec = modTime.tv_sec;	/* overwrite the last one */
	modtimes[ndir].tv_nsec = modTime.tv_nsec;
}


/*
 *  setPathTimes - set the modification time for given path.  Return 1 if
 *                 successful and 0 if not successful.
 */

static void
setPathTimes(char *path, timestruc_t modTime)
{
	struct utimbuf ubuf;
	struct timeval timebuf[2];

	if (xhdr_flgs & _X_MTIME) {	/* Extended header:  use microseconds */
		/*
		 * utimes takes an array of two timeval structs.
		 * The first entry contains access time.
		 * The second entry contains modification time.
		 * Unlike a timestruc_t, which uses nanoseconds, timeval uses
		 * microseconds.
		 */
		timebuf[0].tv_sec = time((time_t *) 0);
		timebuf[0].tv_usec = 0;
		timebuf[1].tv_sec = modTime.tv_sec;
		timebuf[1].tv_usec = modTime.tv_nsec/1000;
		if (utimes(path, timebuf) < 0)
			vperror(0, "can't set time on %s", path);
	} else {			/* File does not have microseconds */
		ubuf.actime   = time((time_t *) 0);
		ubuf.modtime  = modTime.tv_sec;
		if (utime(path, &ubuf) < 0)
			vperror(0, "can't set time on %s", path);
	}
}


/*
 * If hflag is set then delete the symbolic link's target.
 * If !hflag then delete the target.
 */

static void
delete_target(char *namep)
{
	struct	stat	xtractbuf;
	char buf[PATH_MAX + 1];
	int n;

	if (rmdir(namep) < 0) {
		if (errno == ENOTDIR && !hflag) {
			(void) unlink(namep);
		} else if (errno == ENOTDIR && hflag) {
			if (!lstat(namep, &xtractbuf)) {
				if ((xtractbuf.st_mode & S_IFMT) != S_IFLNK) {
					(void) unlink(namep);
				} else if ((n = readlink(namep, buf,
					    PATH_MAX)) != -1) {
					buf[n] = (char) NULL;
					(void) rmdir(buf);
					if (errno == ENOTDIR)
						(void) unlink(buf);
				} else {
					(void) unlink(namep);
				}
			} else {
				(void) unlink(namep);
			}
		}
	}
}


/*
 * ACL changes:
 *	putfile():
 *		Get acl info after stat. Write out ancillary file
 *		before the normal file, i.e. directory, regular, FIFO,
 *		link, special. If acl count is less than 4, no need to
 *		create ancillary file. (i.e. standard permission is in
 *		use.
 *	doxtract():
 *		Process ancillary file. Read it in and set acl info.
 *		watch out for -o option.
 *	-t option to display table
 */

/*
 * New functions for ACLs and other security attributes
 */

/*
 * The function appends the new security attribute info to the end of
 * existing secinfo.
 */
int
append_secattr(
	char	 **secinfo,	/* existing security info */
	int	 *secinfo_len,	/* length of existing security info */
	int	 size,		/* new attribute size: unit depends on type */
	aclent_t *attrp,	/* new attribute data pointer */
	char	 attr_type)	/* new attribute type */
{
	char	*new_secinfo;
	char	*attrtext;
	int	newattrsize;
	int	oldsize;

	/* no need to add */
	if (attrp == NULL)
		return (0);

	switch (attr_type) {
	case UFSD_ACL:
		attrtext = acltotext((aclent_t *)attrp, size);
		if (attrtext == NULL) {
			fprintf(stderr, "acltotext failed\n");
			return (-1);
		}
		/* header: type + size = 8 */
		newattrsize = 8 + (int) strlen(attrtext) + 1;
		attr = (struct sec_attr *) malloc(newattrsize);
		if (attr == NULL) {
			fprintf(stderr, "can't allocate memory\n");
			return (-1);
		}
		attr->attr_type = UFSD_ACL;
		sprintf(attr->attr_len,  "%06o", size); /* acl entry count */
		(void) strcpy((char *) &attr->attr_info[0], attrtext);
		free(attrtext);
		break;

	/* SunFed's case goes here */

	default:
		fprintf(stderr, "unrecognized attribute type\n");
		return (-1);
	}

	/* old security info + new attr header(8) + new attr */
	oldsize = *secinfo_len;
	*secinfo_len += newattrsize;
	new_secinfo = (char *) malloc(*secinfo_len);
	if (new_secinfo == NULL) {
		fprintf(stderr, "can't allocate memory\n");
		*secinfo_len -= newattrsize;
		return (-1);
	}

	memcpy(new_secinfo, *secinfo, oldsize);
	memcpy(new_secinfo + oldsize, attr, newattrsize);

	free(*secinfo);
	*secinfo = new_secinfo;
	return (0);
}

/*
 * write_ancillary(): write out an ancillary file.
 *      The file has the same header as normal file except the type and size
 *      fields. The type is 'A' and size is the sum of all attributes
 *	in bytes.
 *	The body contains a list of attribute type, size and info. Currently,
 *	there is only ACL info.  This file is put before the normal file.
 */
void
write_ancillary(union hblock *dblockp, char *secinfo, int len)
{
	long    blocks;
	int	savflag;
	int	savsize;

	/* Just tranditional permissions or no security attribute info */
	if (len == 0 || secinfo == NULL)
		return;

	/* save flag and size */
	savflag = (dblockp->dbuf).typeflag;
	(void) sscanf(dblockp->dbuf.size, "%12o", (uint *)&savsize);

	/* special flag for ancillary file */
	dblockp->dbuf.typeflag = 'A';

	/* for pre-2.5 versions of tar, need to make sure */
	/* the ACL file is readable			  */
	(void) sprintf(dblock.dbuf.mode, "%07lo",
		(stbuf.st_mode & MODEMASK) | 0000200);
	(void) sprintf(dblockp->dbuf.size, "%011o", len);
	(void) sprintf(dblockp->dbuf.chksum, "%07o", checksum(dblockp));

	/* write out the header */
	(void) writetape((char *)dblockp);

	/* write out security info */
	blocks = TBLOCKS(len);
	(void) writetbuf((char *)secinfo, (int)blocks);

	/* restore mode, flag and size */
	(void) sprintf(dblock.dbuf.mode, "%07lo", stbuf.st_mode & MODEMASK);
	dblockp->dbuf.typeflag = savflag;
	(void) sprintf(dblockp->dbuf.size, "%011o", savsize);
}

/*
 * Read the data record for extended headers and then the regular header.
 * The data are read into the buffer and then null-terminated.  Entries
 * are of the format:
 * 	"%d %s=%s\n"
 *
 * When an extended header record is found, the extended header must
 * be processed and its values used to override the values in the
 * normal header.  The way this is done is to process the extended
 * header data record and set the data values, then call getdir
 * to process the regular header, then then to reconcile the two
 * sets of data.
 */

static int
get_xdata(void)
{
	struct keylist_pair {
		int keynum;
		char *keylist;
	}	keylist_pair[] = {	_X_DEVMAJOR, "SUN.devmajor",
					_X_DEVMINOR, "SUN.devminor",
					_X_GID, "gid",
					_X_GNAME, "gname",
					_X_LINKPATH, "linkpath",
					_X_PATH, "path",
					_X_SIZE, "size",
					_X_UID, "uid",
					_X_UNAME, "uname",
					_X_MTIME, "mtime",
					_X_LAST, "NULL" };
	char		*lineloc;
	int		length, i;
	char		*keyword, *value;
	blkcnt_t	nblocks;
	int		bufneeded;
	struct stat	*sp = &stbuf;
	int		errors;

	Xtarhdr.x_uid = 0;
	Xtarhdr.x_gid = 0;
	Xtarhdr.x_devmajor = 0;
	Xtarhdr.x_devminor = 0;
	Xtarhdr.x_filesz = 0;
	Xtarhdr.x_uname = NULL;
	Xtarhdr.x_gname = NULL;
	Xtarhdr.x_linkpath = NULL;
	Xtarhdr.x_path = NULL;
	Xtarhdr.x_mtime.tv_sec = 0;
	Xtarhdr.x_mtime.tv_nsec = 0;
	xhdr_count++;
	errors = 0;

	nblocks = TBLOCKS(stbuf.st_size);
	bufneeded = nblocks * TBLOCK;
	if (bufneeded >= xrec_size) {
		free(xrec_ptr);
		xrec_size = bufneeded + 1;
		if ((xrec_ptr = malloc(xrec_size)) == NULL)
			fatal(gettext("cannot allocate buffer"));
	}

	lineloc = xrec_ptr;

	while (nblocks-- > 0) {
		readtape(lineloc);
		lineloc += TBLOCK;
	}
	lineloc = xrec_ptr;
	xrec_ptr[stbuf.st_size] = '\0';
	while (lineloc < xrec_ptr + stbuf.st_size) {
		length = atoi(lineloc);
		*(lineloc + length - 1) = '\0';
		keyword = strchr(lineloc, ' ') + 1;
		value = strchr(keyword, '=') + 1;
		*(value - 1) = '\0';
		i = 0;
		lineloc += length;
		while (keylist_pair[i].keynum != (int)_X_LAST) {
			if (strcmp(keyword, keylist_pair[i].keylist) == 0)
				break;
			i++;
		}
		errno = 0;
		switch (keylist_pair[i].keynum) {
		case _X_DEVMAJOR:
			Xtarhdr.x_devmajor = (major_t) strtoul(value, NULL, 0);
			if (errno) {
				(void) fprintf(stderr, gettext(
				    "tar: Extended header major value error "
				    "for file # %llu.\n"), xhdr_count);
				errors++;
			} else
				xhdr_flgs |= _X_DEVMAJOR;
			break;
		case _X_DEVMINOR:
			Xtarhdr.x_devminor = (minor_t) strtoul(value, NULL, 0);
			if (errno) {
				(void) fprintf(stderr, gettext(
				    "tar: Extended header minor value error "
				    "for file # %llu.\n"), xhdr_count);
				errors++;
			} else
				xhdr_flgs |= _X_DEVMINOR;
			break;
		case _X_GID:
			xhdr_flgs |= _X_GID;
			Xtarhdr.x_gid = strtol(value, NULL, 0);
			if ((errno) || (Xtarhdr.x_gid > UID_MAX)) {
				(void) fprintf(stderr, gettext(
				    "tar: Extended header gid value error "
				    "for file # %llu.\n"), xhdr_count);
				Xtarhdr.x_gid = GID_NOBODY;
			}
			break;
		case _X_GNAME:
			if (utf8_local("gname", &Xtarhdr.x_gname,
			    local_gname, value, _POSIX_NAME_MAX) == 0)
				xhdr_flgs |= _X_GNAME;
			break;
		case _X_LINKPATH:
			if (utf8_local("linkpath", &Xtarhdr.x_linkpath,
			    local_linkpath, value, PATH_MAX) == 0)
				xhdr_flgs |= _X_LINKPATH;
			else
				errors++;
			break;
		case _X_PATH:
			if (utf8_local("path", &Xtarhdr.x_path,
			    local_path, value, PATH_MAX) == 0)
				xhdr_flgs |= _X_PATH;
			else
				errors++;
			break;
		case _X_SIZE:
			Xtarhdr.x_filesz = strtoull(value, NULL, 0);
			if (errno) {
				(void) fprintf(stderr, gettext(
				    "tar: Extended header invalid filesize "
				    "for file # %llu.\n"), xhdr_count);
				errors++;
			} else
				xhdr_flgs |= _X_SIZE;
			break;
		case _X_UID:
			xhdr_flgs |= _X_UID;
			Xtarhdr.x_uid = strtol(value, NULL, 0);
			if ((errno) || (Xtarhdr.x_uid > UID_MAX)) {
				(void) fprintf(stderr, gettext(
				    "tar: Extended header uid value error "
				    "for file # %llu.\n"), xhdr_count);
				Xtarhdr.x_uid = UID_NOBODY;
			}
			break;
		case _X_UNAME:
			if (utf8_local("uname", &Xtarhdr.x_uname,
			    local_uname, value, _POSIX_NAME_MAX) == 0)
				xhdr_flgs |= _X_UNAME;
			break;
		case _X_MTIME:
			get_xtime(value, &(Xtarhdr.x_mtime));
			if (errno)
				(void) fprintf(stderr, gettext(
				    "tar: Extended header modification time "
				    "value error for file # %llu.\n"),
				    xhdr_count);
			else
				xhdr_flgs |= _X_MTIME;
			break;
		default:
			fprintf(stderr, gettext("tar:  unrecognized extended"
			    " header keyword '%s'.  Ignored.\n"), keyword);
			break;
		}
	}

	getdir();	/* get regular header */

	if (xhdr_flgs & _X_DEVMAJOR) {
		Gen.g_devmajor = Xtarhdr.x_devmajor;
	}
	if (xhdr_flgs & _X_DEVMINOR) {
		Gen.g_devminor = Xtarhdr.x_devminor;
	}
	if (xhdr_flgs & _X_GID) {
		Gen.g_gid = Xtarhdr.x_gid;
		sp->st_gid = Gen.g_gid;
	}
	if (xhdr_flgs & _X_UID) {
		Gen.g_uid = Xtarhdr.x_uid;
		sp->st_uid = Gen.g_uid;
	}
	if (xhdr_flgs & _X_SIZE) {
		Gen.g_filesz = Xtarhdr.x_filesz;
		sp->st_size = Gen.g_filesz;
	}
	if (xhdr_flgs & _X_MTIME) {
		Gen.g_mtime = Xtarhdr.x_mtime.tv_sec;
		sp->st_mtim.tv_sec = Gen.g_mtime;
		sp->st_mtim.tv_nsec = Xtarhdr.x_mtime.tv_nsec;
	}

	if (errors && errflag)
		done(1);
	return (errors);
}

/*
 * gen_num creates a string from a keyword and an usigned long long in the
 * format:  %d %s=%s\n
 * This is part of the extended header data record.
 */

void
gen_num(const char *keyword, const u_longlong_t number)
{
	char	save_val[ULONGLONG_MAX_DIGITS + 1];
	int	len;
	char	*curr_ptr;

	sprintf(save_val, "%llu", number);
	/*
	 * len = length of entire line, including itself.  len will be
	 * two digits.  So, add the string lengths plus the length of len,
	 * plus a blank, an equal sign, and a newline.
	 */
	len = strlen(save_val) + strlen(keyword) + 5;
	if (xrec_offset + len > xrec_size) {
		if (((curr_ptr = realloc(xrec_ptr, 2 * xrec_size)) == NULL))
			fatal(gettext(
			    "cannot allocate extended header buffer"));
		xrec_ptr = curr_ptr;
		xrec_size *= 2;
	}
	sprintf(&xrec_ptr[xrec_offset], "%d %s=%s\n", len, keyword, save_val);
	xrec_offset += len;
}

/*
 * gen_date creates a string from a keyword and a timestruc_t in the
 * format:  %d %s=%s\n
 * This is part of the extended header data record.
 * Currently, granularity is only microseconds, so the low-order three digits
 * will be truncated.
 */

void
gen_date(const char *keyword, const timestruc_t time_value)
{
	/* Allow for <seconds>.<nanoseconds>\n */
	char	save_val[TIME_MAX_DIGITS + LONG_MAX_DIGITS + 2];
	int	len;
	char	*curr_ptr;

	sprintf(save_val, "%ld", time_value.tv_sec);
	len = strlen(save_val);
	save_val[len] = '.';
	sprintf(&save_val[len + 1], "%9.9ld", time_value.tv_nsec);

	/*
	 * len = length of entire line, including itself.  len will be
	 * two digits.  So, add the string lengths plus the length of len,
	 * plus a blank, an equal sign, and a newline.
	 */
	len = strlen(save_val) + strlen(keyword) + 5;
	if (xrec_offset + len > xrec_size) {
		if (((curr_ptr = realloc(xrec_ptr, 2 * xrec_size)) == NULL))
			fatal(gettext(
			    "cannot allocate extended header buffer"));
		xrec_ptr = curr_ptr;
		xrec_size *= 2;
	}
	sprintf(&xrec_ptr[xrec_offset], "%d %s=%s\n", len, keyword, save_val);
	xrec_offset += len;
}

/*
 * gen_string creates a string from a keyword and a char * in the
 * format:  %d %s=%s\n
 * This is part of the extended header data record.
 */

void
gen_string(const char *keyword, const char *value)
{
	int	len;
	char	*curr_ptr;

	/*
	 * len = length of entire line, including itself.  The character length
	 * of len must be 1-4 characters, because the maximum size of the path
	 * or the name is PATH_MAX, which is 1024.  So, assume 1 character
	 * for len, one for the space, one for the "=", and one for the newline.
	 * Then adjust as needed.
	 */
	assert(PATH_MAX <= 9996);
	len = strlen(value) + strlen(keyword) + 4;
	if (len > 997)
		len += 3;
	else if (len > 98)
		len += 2;
	else if (len > 9)
		len += 1;
	if (xrec_offset + len > xrec_size) {
		if (((curr_ptr = realloc(xrec_ptr, 2 * xrec_size)) == NULL))
			fatal(gettext(
			    "cannot allocate extended header buffer"));
		xrec_ptr = curr_ptr;
		xrec_size *= 2;
	}
#ifdef XHDR_DEBUG
	if (strcmp(keyword+1, "name") != 0)
#endif
	sprintf(&xrec_ptr[xrec_offset], "%d %s=%s\n", len, keyword, value);
#ifdef XHDR_DEBUG
	else {
	len += 7;
	sprintf(&xrec_ptr[xrec_offset], "%d %s=%stoolong\n", len, keyword,
	    value);
	}
#endif
	xrec_offset += len;
}

/*
 * Convert time found in the extended header data to seconds and nanoseconds.
 */

void
get_xtime(char *value, timestruc_t *xtime)
{
	char nanosec[10];
	char *period;
	int i;

	memset(nanosec, '0', 9);
	nanosec[9] = '\0';

	period = strchr(value, '.');
	if (period != NULL)
		period[0] = '\0';
	xtime->tv_sec = strtol(value, NULL, 10);
	if (period == NULL)
		xtime->tv_nsec = 0;
	else {
		i = strlen(period +1);
		strncpy(nanosec, period + 1, min(i, 9));
		xtime->tv_nsec = strtol(nanosec, NULL, 10);
	}
}

/*
 *	Check linkpath for length.
 *	Emit an error message and return 1 if too long.
 */

int
chk_path_build(
	char	*name,
	char	*longname,
	char	*linkname,
	char	*prefix,
	char	type)
{

	if (strlen(linkname) > (size_t)NAMSIZ) {
		if (Eflag > 0) {
			xhdr_flgs |= _X_LINKPATH;
			Xtarhdr.x_linkpath = linkname;
		} else {
			(void) fprintf(stderr, gettext(
			    "tar: %s: linked to %s\n"), longname, linkname);
			(void) fprintf(stderr, gettext(
			    "tar: %s: linked name too long\n"), linkname);
			if (errflag)
				done(1);
			if (aclp != NULL)
				free(aclp);
			return (1);
		}
	}
	if (xhdr_flgs & _X_LINKPATH)
		return (build_dblock(name, tchar, type, &stbuf, stbuf.st_dev,
		    prefix));
	else
		return (build_dblock(name, linkname, type, &stbuf, stbuf.st_dev,
		    prefix));
}

/*
 * Convert from UTF8 to local character set.
 */

static int
utf8_local(
	char		*option,
	char		**Xhdr_ptrptr,
	char		*target,
	const char	*source,
	int		max_val)
{
	static	iconv_t	iconv_cd;
	char		*nl_target;
	const	char	*iconv_src;
	char		*iconv_trg;
	size_t		inlen,
			outlen;

	if (charset_type == -1) {	/* iconv_open failed in earlier try */
		fprintf(stderr, gettext(
		    "tar:  file # %llu: (%s) UTF-8 conversion failed.\n"),
		    xhdr_count, source);
		return (1);
	} else if (charset_type == 0) {	/* iconv_open has not yet been done */
		nl_target = nl_langinfo(CODESET);
		if (strlen(nl_target) == 0)	/* locale using 7-bit codeset */
			nl_target = "646";
		if (strcmp(nl_target, "646") == 0)
			charset_type = 1;
		else {
			if (strncmp(nl_target, "ISO", 3) == 0)
				nl_target += 3;
			charset_type = 2;
			errno = 0;
			if ((iconv_cd = iconv_open(nl_target, "UTF-8")) ==
			    (iconv_t) -1) {
				if (errno == EINVAL)
					fprintf(stderr, gettext(
					    "tar: conversion routines not "
					    "available for current locale.  "));
				fprintf(stderr, gettext(
				    "file # %llu: (%s) UTF-8 conversion"
				    " failed.\n"), xhdr_count, source);
				charset_type = -1;
				return (1);
			}
		}
	}

	if (charset_type == 1) {	/* locale using 7-bit codeset */
		if (strlen(source) > max_val) {
			(void) fprintf(stderr, gettext(
			    "tar: file # %llu: Extended header %s too long.\n"),
			    xhdr_count, option);
			return (1);
		}
		if (c_utf8(target, source) != 0) {
			fprintf(stderr, gettext(
			    "tar:  file # %llu: (%s) UTF-8 conversion"
			    " failed.\n"), xhdr_count, source);
			return (1);
		}
		*Xhdr_ptrptr = target;
		return (0);
	}

	iconv_src = source;
	iconv_trg = target;
	inlen = strlen(source);
	outlen = max_val * UTF_8_FACTOR;
	if (iconv(iconv_cd, &iconv_src, &inlen, &iconv_trg, &outlen) ==
	    (size_t) -1) {	/* Error occurred:  didn't convert */
		fprintf(stderr, gettext(
		    "tar:  file # %llu: (%s) UTF-8 conversion failed.\n"),
		    xhdr_count, source);
		/* Get remaining output; reinitialize conversion descriptor */
		iconv_src = (const char *)NULL;
		inlen = 0;
		(void) iconv(iconv_cd, &iconv_src, &inlen, &iconv_trg, &outlen);
		return (1);
	}
	/* Get remaining output; reinitialize conversion descriptor */
	iconv_src = (const char *)NULL;
	inlen = 0;
	if (iconv(iconv_cd, &iconv_src, &inlen, &iconv_trg, &outlen) ==
	    (size_t) -1) {	/* Error occurred:  didn't convert */
		fprintf(stderr, gettext(
		    "tar:  file # %llu: (%s) UTF-8 conversion failed.\n"),
		    xhdr_count, source);
		return (1);
	}

	*iconv_trg = '\0';	/* Null-terminate iconv output string */
	if (strlen(target) > max_val) {
		(void) fprintf(stderr, gettext(
		    "tar: file # %llu: Extended header %s too long.\n"),
		    xhdr_count, option);
		return (1);
	}
	*Xhdr_ptrptr = target;
	return (0);
}

/*
 * Check gname, uname, path, and linkpath to see if they need to go in an
 * extended header.  If they are already slated to be in an extended header,
 * or if they are not ascii, then they need to be in the extended header.
 * Then, convert all extended names to UTF8.
 */

int
gen_utf8_names(const char *filename)
{
	static	iconv_t	iconv_cd;
	char		*nl_target;
	char		tempbuf[MAXNAM + 1];
	int		nbytes,
			errors;

	if (charset_type == -1)	{	/* Previous failure to open. */
		fprintf(stderr, gettext(
		    "tar: file # %llu: UTF-8 conversion failed.\n"),
		    xhdr_count);
		return (1);
	}

	if (charset_type == 0) {	/* Need to get conversion descriptor */
		nl_target = nl_langinfo(CODESET);
		if (strlen(nl_target) == 0)	/* locale using 7-bit codeset */
			nl_target = "646";
		if (strcmp(nl_target, "646") == 0)
			charset_type = 1;
		else {
			if (strncmp(nl_target, "ISO", 3) == 0)
				nl_target += 3;
			charset_type = 2;
			errno = 0;
#ifdef ICONV_DEBUG
			fprintf(stderr, "Opening iconv_cd with target %s\n",
			    nl_target);
#endif
			if ((iconv_cd = iconv_open("UTF-8", nl_target)) ==
			    (iconv_t) -1) {
				if (errno == EINVAL)
					fprintf(stderr, gettext(
					    "tar: conversion routines not "
					    "available for current locale.  "));
				fprintf(stderr, gettext(
				    "file (%s): UTF-8 conversion failed.\n"),
				    filename);
				charset_type = -1;
				return (1);
			}
		}
	}

	errors = 0;

	errors += local_utf8(&Xtarhdr.x_gname, local_gname,
	    dblock.dbuf.gname, iconv_cd, _X_GNAME, _POSIX_NAME_MAX);
	errors += local_utf8(&Xtarhdr.x_uname, local_uname,
	    dblock.dbuf.uname, iconv_cd, _X_UNAME,  _POSIX_NAME_MAX);
	if ((xhdr_flgs & _X_LINKPATH) == 0) {	/* Need null-terminated str. */
		strncpy(tempbuf, dblock.dbuf.linkname, NAMSIZ);
		tempbuf[NAMSIZ] = '\0';
	}
	errors += local_utf8(&Xtarhdr.x_linkpath, local_linkpath,
	    tempbuf, iconv_cd, _X_LINKPATH, PATH_MAX);
	if ((xhdr_flgs & _X_PATH) == 0) {	/* Concatenate prefix & name */
		strncpy(tempbuf, dblock.dbuf.prefix, PRESIZ);
		tempbuf[NAMSIZ] = '\0';
		nbytes = strlen(tempbuf);
		if (nbytes > 0) {
			tempbuf[nbytes++] = '/';
			tempbuf[nbytes] = '\0';
		}
		strncat(tempbuf + nbytes, dblock.dbuf.name, NAMSIZ);
		tempbuf[nbytes + NAMSIZ] = '\0';
	}
	errors += local_utf8(&Xtarhdr.x_path, local_path,
	    tempbuf, iconv_cd, _X_PATH, PATH_MAX);

	if (errors > 0)
		fprintf(stderr, gettext(
		    "tar: file (%s): UTF-8 conversion failed.\n"), filename);

	if (errors && errflag)
		done(1);
	return (errors);
}

static int
local_utf8(
		char	**Xhdr_ptrptr,
		char	*target,
		const	char	*source,
		iconv_t	iconv_cd,
		int	xhdrflg,
		int	max_val)
{
	const	char	*iconv_src;
	const	char	*starting_src;
	char		*iconv_trg;
	size_t		inlen,
			outlen;
#ifdef ICONV_DEBUG
	unsigned char	c_to_hex;
#endif

	/*
	 * If the item is already slated for extended format, get the string
	 * to convert from the extended header record.  Otherwise, get it from
	 * the regular (dblock) area.
	 */
	if (xhdr_flgs & xhdrflg)
		iconv_src = (const char *) *Xhdr_ptrptr;
	else
		iconv_src = source;
	starting_src = iconv_src;
	iconv_trg = target;
	if ((inlen = strlen(iconv_src)) == 0)
		return (0);

	if (charset_type == 1) {	/* locale using 7-bit codeset */
		if (c_utf8(target, starting_src) != 0) {
			fprintf(stderr, gettext("tar: invalid character in"
			    " UTF-8 conversion of '%s'\n"), starting_src);
			return (1);
		}
		return (0);
	}

	outlen = max_val * UTF_8_FACTOR;
	errno = 0;
	if (iconv(iconv_cd, &iconv_src, &inlen, &iconv_trg, &outlen) ==
	    (size_t) -1) {
		/* An error occurred, or not all characters were converted */
		if (errno == EILSEQ)
			fprintf(stderr, gettext("tar: invalid character in"
			    " UTF-8 conversion of '%s'\n"), starting_src);
		else
			fprintf(stderr, gettext(
			    "tar: conversion to UTF-8 aborted for '%s'.\n"),
			    starting_src);
		/* Get remaining output; reinitialize conversion descriptor */
		iconv_src = (const char *)NULL;
		inlen = 0;
		(void) iconv(iconv_cd, &iconv_src, &inlen, &iconv_trg, &outlen);
		return (1);
	}
	/* Get remaining output; reinitialize conversion descriptor */
	iconv_src = (const char *)NULL;
	inlen = 0;
	if (iconv(iconv_cd, &iconv_src, &inlen, &iconv_trg, &outlen) ==
	    (size_t) -1) {	/* Error occurred:  didn't convert */
		if (errno == EILSEQ)
			fprintf(stderr, gettext("tar: invalid character in"
			    " UTF-8 conversion of '%s'\n"), starting_src);
		else
			fprintf(stderr, gettext(
			    "tar: conversion to UTF-8 aborted for '%s'.\n"),
			    starting_src);
		return (1);
	}

	*iconv_trg = '\0';	/* Null-terminate iconv output string */
	if (strcmp(starting_src, target) != 0) {
		*Xhdr_ptrptr = target;
		xhdr_flgs |= xhdrflg;
#ifdef ICONV_DEBUG
		fprintf(stderr, "***  inlen: %d %d; outlen: %d %d\n",
		    strlen(starting_src), inlen, max_val, outlen);
		fprintf(stderr, "Input string:\n  ");
		for (inlen = 0; inlen < strlen(starting_src); inlen++) {
			c_to_hex = (unsigned char)starting_src[inlen];
			fprintf(stderr, " %2.2x", c_to_hex);
			if (inlen % 20 == 19)
				fprintf(stderr, "\n  ");
		}
		fprintf(stderr, "\nOutput string:\n  ");
		for (inlen = 0; inlen < strlen(target); inlen++) {
			c_to_hex = (unsigned char)target[inlen];
			fprintf(stderr, " %2.2x", c_to_hex);
			if (inlen % 20 == 19)
				fprintf(stderr, "\n  ");
		}
		fprintf(stderr, "\n");
#endif
	}

	return (0);
}

/*
 *	Function to test each byte of the source string to make sure it is
 *	in within bounds (value between 0 and 127).
 *	If valid, copy source to target.
 */

int
c_utf8(char *target, const char *source)
{
	size_t		len;
	const char	*thischar;

	len = strlen(source);
	thischar = source;
	while (len-- > 0) {
		if (!isascii((int)(*thischar++)))
			return (1);
	}

	(void) strcpy(target, source);
	return (0);
}
