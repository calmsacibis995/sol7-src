/*
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)cpio.c	1.66	98/02/11	SMI"

/* ************************************************************* */

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
 *		Copyright Notice
 *
 * Notice of copyright on this source code product does not indicate
 * publication.
 *
 * Copyright (c) 1986,1987,1988,1989, 1996  Sun Microsystems, Inc
 * All rights reserved.
 *
 *	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *	          All rights reserved.
 */

/* ************************************************************* */

#include <stdio.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <memory.h>
#include <string.h>
#include <varargs.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/mkdev.h>
#include <sys/param.h>
#include <utime.h>
#include <pwd.h>
#include <grp.h>
#include <signal.h>
#include <ctype.h>
#include <archives.h>
#include <locale.h>
#include <sys/ioctl.h>
#include <sys/mtio.h>
#include <sys/fdio.h>
#include "cpio.h"
#include <sys/acl.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <fnmatch.h>
#include <libintl.h>
#include <dirent.h>
#include <limits.h>

#define	SECMODE	0xe080

#define	NAMELEN		32
#define	TYPELEN 	16
#define	PERMLEN		4

/*
 *	These limits reflect the maximum size regular file that
 *	can be archived, depending on the archive type. For archives
 *	with character-format headers (odc, tar, ustar) we use
 *	CHAR_OFFSET_MAX. Otherwise, we are limited to the size
 *	that will fit in a signed long value.
 */
#define	CHAR_OFFSET_MAX	077777777777	/* 11 octal digits */
#define	BIN_OFFSET_MAX	LONG_MAX	/* signed long max value */

static char	aclchar = ' ';

static struct Lnk *add_lnk(struct Lnk **);
static char *align(int size);
static int bfill(void);
static void bflush(void);
static int chgreel(int dir);
static int ckname(void);
static void ckopts(long mask);
static long cksum(char hdr, int byt_cnt, int *err);
static int creat_hdr(void);
static int creat_lnk(char *name1_p, char *name2_p);
static int creat_spec(void);
static int creat_tmp(char *nam_p);
static void data_in(int proc_mode);
static void data_out(void);
static void data_pass(void);
static void file_in(void);
static void file_out(void);
static void file_pass(void);
static void flush_lnks(void);
static int gethdr(void);
static int getname(void);
static void getpats(int largc, char **largv);
static void ioerror(int dir);
static int matched(void);
static int missdir(char *nam_p);
static long mklong(short v[]);
static void mkshort(short sval[], long v);
static void msg();
static int openout(void);
static int read_hdr(int hdr);
static void reclaim(struct Lnk *l_p);
static void rstbuf(void);
static void setpasswd(char *nam);
static void rstfiles(int over);
static void scan4trail(void);
static void setup(int largc, char **largv);
static void set_tym(char *nam_p, time_t atime, time_t mtime);
static void sigint(int sig);
static void swap(char *buf_p, int cnt);
static void usage(void);
static void verbose(char *nam_p);
static void write_hdr(int secflag, off_t len);
static void write_trail(void);
static int ustar_dir(void);
static int ustar_spec(void);
static int convert_to_old_stat(void);
static void read_bar_vol_hdr(void);
static void read_bar_file_hdr(void);
static void setup_uncompress(FILE **);
static void skip_bar_volhdr(void);
static void bar_file_in(void);
static int g_init(int *devtype, int *fdes);
static int g_read(int, int, char *, unsigned);
static int g_write(int, int, char *, unsigned);
static int is_floppy(int);
static int is_tape(int);
static int append_secattr(char **, int *, int, char *, char);
static void write_ancillary(char *secinfo, int len);
static int remove_dir(char *);
static int save_cwd(void);
static void rest_cwd(int cwd);

static
struct passwd	*Curpw_p,	/* Current password entry for -t option */
		*Rpw_p,		/* Password entry for -R option */
		*dpasswd;

static
struct group	*Curgr_p,	/* Current group entry for -t option */
		*dgroup;

/* Data structure for buffered I/O. */

static
struct buf_info {
	char	*b_base_p,	/* Pointer to base of buffer */
		*b_out_p,	/* Position to take bytes from buffer at */
		*b_in_p,	/* Position to put bytes into buffer at */
		*b_end_p;	/* Pointer to end of buffer */
	long	b_cnt,		/* Count of unprocessed bytes */
		b_size;		/* Size of buffer in bytes */
} Buffr;


static
struct cpioinfo {
	o_dev_t	st_dev;
	o_ino_t	st_ino;
	o_mode_t	st_mode;
	o_nlink_t	st_nlink;
	uid_t	st_uid;
	gid_t	st_gid;
	o_dev_t	st_rdev;
	off_t	st_size;
	time_t	st_modtime;
} *TmpSt;

/* Generic header format */

static
struct gen_hdr {
	ulong	g_magic,	/* Magic number field */
		g_ino,		/* Inode number of file */
		g_mode,		/* Mode of file */
		g_uid,		/* Uid of file */
		g_gid,		/* Gid of file */
		g_nlink,	/* Number of links */
		g_mtime;	/* Modification time */
	off_t	g_filesz;	/* Length of file */
	ulong	g_dev,		/* File system of file */
		g_rdev,		/* Major/minor numbers of special files */
		g_namesz,	/* Length of filename */
		g_cksum;	/* Checksum of file */
	char	g_gname[32],
		g_uname[32],
		g_version[2],
		g_tmagic[6],
		g_typeflag;
	char	*g_tname,
		*g_prefix,
		*g_nam_p;	/* Filename */
} Gen, *G_p;

/* Data structure for handling multiply-linked files */
static
char	prebuf[155],
	nambuf[100],
	fullnam[256];


static
struct Lnk {
	short	L_cnt,		/* Number of links encountered */
		L_data;		/* Data has been encountered if 1 */
	struct gen_hdr	L_gen;	/* gen_hdr information for this file */
	struct Lnk	*L_nxt_p,	/* Next file in list */
			*L_bck_p,	/* Previous file in list */
			*L_lnk_p;	/* Next link for this file */
} Lnk_hd;

static
struct hdr_cpio	Hdr;

static
struct stat	ArchSt,	/* stat(2) information of the archive */
		SrcSt,	/* stat(2) information of source file */
		DesSt;	/* stat(2) of destination file */

/*
 * bin_mag: Used to validate a binary magic number,
 * by combining to bytes into an unsigned short.
 */

static
union bin_mag {
	unsigned char b_byte[2];
	ushort b_half;
} Binmag;

static
union tblock *Thdr_p;	/* TAR header pointer */

static union b_block *bar_Vhdr;
static struct gen_hdr Gen_bar_vol;

/*
 * swpbuf: Used in swap() to swap bytes within a halfword,
 * halfwords within a word, or to reverse the order of the
 * bytes within a word.  Also used in mklong() and mkshort().
 */

static
union swpbuf {
	unsigned char	s_byte[4];
	ushort	s_half[2];
	ulong	s_word;
} *Swp_p;

static
char	Adir,			/* Flags object as a directory */
	Aspec,			/* Flags object as a special file */
	Do_rename,		/* Indicates rename() is to be used */
	Time[50],		/* Array to hold date and time */
	Ttyname[] = "/dev/tty",	/* Controlling console */
	T_lname[100],		/* Array to hold links name for tar */
	*Buf_p,			/* Buffer for file system I/O */
	*Empty,			/* Empty block for TARTYP headers */
	*Full_p,		/* Pointer to full pathname */
	*Efil_p,		/* -E pattern file string */
	*Eom_p = "Change to part %d and press RETURN key. [q] ",
	*Fullnam_p,		/* Full pathname */
	*Hdr_p,			/* -H header type string */
	*IOfil_p,		/* -I/-O input/output archive string */
	*Lnkend_p,		/* Pointer to end of Lnknam_p */
	*Lnknam_p,		/* Buffer for linking files with -p option */
	*Nam_p,			/* Array to hold filename */
	*Own_p,			/* New owner login id string */
	*Renam_p,		/* Buffer for renaming files */
	*Symlnk_p,		/* Buffer for holding symbolic link name */
	*Over_p,		/* Holds temporary filename when overwriting */
	**Pat_pp = 0,		/* Pattern strings */
	bar_linkflag,		/* flag to indicate if the file is a link */
	bar_linkname[MAXPATHLEN]; /* store the name of the link */

static
int	Append = 0,	/* Flag set while searching to end of archive */
	Archive,	/* File descriptor of the archive */
	Buf_error = 0,	/* I/O error occured during buffer fill */
	Def_mode = 0777,	/* Default file/directory protection modes */
	Device,		/* Device type being accessed (used with libgenIO) */
	Error_cnt = 0,	/* Cumulative count of I/O errors */
	Finished = 1,	/* Indicates that a file transfer has completed */
	Hdrsz = ASCSZ,	/* Fixed length portion of the header */
	Hdr_type,		/* Flag to indicate type of header selected */
	Ifile,		/* File des. of file being archived */
	Ofile,		/* File des. of file being extracted from archive */
	Oldstat = 0,	/* Create an old style -c hdr (small dev's)	*/
	Onecopy = 0,	/* Flags old vs. new link handling */
	Pad_val = 0,	/* Indicates the number of bytes to pad (if any) */
	Volcnt = 1,	/* Number of archive volumes processed */
	Verbcnt = 0,	/* Count of number of dots '.' output */
	Eomflag = 0,
	Dflag = 0,
	Compressed,	/* Flag to indicate if the bar archive is compressed */
	Bar_vol_num = 0; /* Volume number count for bar archive */

static
gid_t	Lastgid = -1;	/* Used with -t & -v to record current gid */

static
uid_t	Lastuid = -1;	/* Used with -t & -v to record current uid */

static
long	Args,		/* Mask of selected options */
	Max_namesz = CPATH;	/* Maximum size of pathnames/filenames */

static
int	Bufsize = BUFSZ;	/* Default block size */


static u_longlong_t    Blocks;	/* full blocks transferred */
static u_longlong_t    SBlocks;	/* cumulative char count from short reads */


static off_t	Max_offset = BIN_OFFSET_MAX;	/* largest file size */
static off_t	Max_filesz;			/* from getrlimit */

static
FILE	*Ef_p,			/* File pointer of pattern input file */
	*Err_p = stderr,	/* File pointer for error reporting */
	*Out_p = stdout,	/* File pointer for non-archive output */
	*Rtty_p,		/* Input file pointer for interactive rename */
	*Wtty_p;		/* Output file ptr for interactive rename */



static
ushort	Ftype = S_IFMT;	/* File type mask */
static
uid_t	Uid;		/* Uid of invoker */

/* ACL support */
static struct sec_attr {
	char	attr_type;
	char	attr_len[7];
	char	attr_info[1];
} *attr;

static int	Pflag = 0;	/* flag indicates that acl is preserved */
static int	aclcnt = 0;	/* acl entry count */
static aclent_t *aclp = NULL;	/* pointer to ACL */

static int	append_secattr(char **, int *, int, char *, char);
static void	write_ancillary(char *, int);

/*
 * main: Call setup() to process options and perform initializations,
 * and then select either copy in (-i), copy out (-o), or pass (-p) action.
 */

main(int argc, char **argv)
{
	int i;
	(void) setlocale(LC_ALL, "");
#if !defined(TEXT_DOMAIN)	/* Should be defined by cc -D */
#define	TEXT_DOMAIN "SYS_TEST"	/* Use this only if it weren't */
#endif
	(void) textdomain(TEXT_DOMAIN);

	setup(argc, argv);
	if (signal(SIGINT, sigint) == SIG_IGN)
		(void) signal(SIGINT, SIG_IGN);
	switch (Args & (OCi | OCo | OCp)) {
	case OCi: /* COPY IN */
		Hdr_type = NONE;
		while ((i = gethdr()) != 0) {
			if (i == 1)
				file_in();
		}
		/* Do not count "extra" "read-ahead" buffered data */
		if (Buffr.b_cnt > Bufsize)
			Blocks -=  (u_longlong_t)(Buffr.b_cnt / Bufsize);
		break;
	case OCo: /* COPY OUT */
		if (Args & OCA)
			scan4trail();
		while ((i = getname()) != 0) {
			if (i == 1)
				file_out();
			if (aclp != NULL) {
				free(aclp);
				aclcnt = 0;
				aclp = NULL;
			}
		}
		write_trail();
		break;
	case OCp: /* PASS */
		while (getname())
			file_pass();
		break;
	default:
		msg(EXT, "Impossible action.");
	}
	if (Ofile > 0) {
		if (close(Ofile) != 0)
			msg(EXTN, "close error");
	}
	if (Archive > 0) {
		if (close(Archive) != 0)
			msg(EXTN, "close error");
	}
	Blocks = (u_longlong_t)(Blocks * Bufsize + SBlocks + 0x1FF) >> 9;
	msg(EPOST, "%lld blocks", Blocks);
	if (Error_cnt)
		msg(EPOST, "%d error(s)", Error_cnt);
	return (Error_cnt);
}

/*
 * add_lnk: Add a linked file's header to the linked file data structure.
 * Either adding it to the end of an existing sub-list or starting
 * a new sub-list.  Each sub-list saves the links to a given file.
 */

static struct Lnk *
add_lnk(struct Lnk **l_p)
{
	register struct Lnk *t1l_p, *t2l_p, *t3l_p;

	t2l_p = Lnk_hd.L_nxt_p;
	while (t2l_p != &Lnk_hd) {
		if (t2l_p->L_gen.g_ino == G_p->g_ino && t2l_p->L_gen.g_dev ==
		    G_p->g_dev)
			break; /* found */
		t2l_p = t2l_p->L_nxt_p;
	}
	if (t2l_p == &Lnk_hd)
		t2l_p = (struct Lnk *) NULL;
	t1l_p = (struct Lnk *) malloc(sizeof (struct Lnk));
	if (t1l_p == (struct Lnk *)NULL)
		msg(EXT, "Out of memory.");
	t1l_p->L_lnk_p = (struct Lnk *)NULL;
	t1l_p->L_gen = *G_p; /* structure copy */
	t1l_p->L_gen.g_nam_p = (char *)malloc((unsigned int)G_p->g_namesz);
	if (t1l_p->L_gen.g_nam_p == (char *)NULL)
		msg(EXT, "Out of memory.");
	(void) strcpy(t1l_p->L_gen.g_nam_p, G_p->g_nam_p);
	if (t2l_p == (struct Lnk *)NULL) { /* start new sub-list */
		t1l_p->L_nxt_p = &Lnk_hd;
		t1l_p->L_bck_p = Lnk_hd.L_bck_p;
		Lnk_hd.L_bck_p = t1l_p->L_bck_p->L_nxt_p = t1l_p;
		t1l_p->L_lnk_p = (struct Lnk *)NULL;
		t1l_p->L_cnt = 1;
		t1l_p->L_data = Onecopy ? 0 : 1;
		t2l_p = t1l_p;
	} else { /* add to existing sub-list */
		t2l_p->L_cnt++;
		t3l_p = t2l_p;
		while (t3l_p->L_lnk_p != (struct Lnk *)NULL) {
			t3l_p->L_gen.g_filesz = G_p->g_filesz;
			t3l_p = t3l_p->L_lnk_p;
		}
		t3l_p->L_gen.g_filesz = G_p->g_filesz;
		t3l_p->L_lnk_p = t1l_p;
	}
	*l_p = t2l_p;
	return (t1l_p);
}

/*
 * align: Align a section of memory of size bytes on a page boundary and
 * return the location.  Used to increase I/O performance.
 */

static char *
align(int size)
{
	register int pad;

	int pagesize = sysconf(_SC_PAGESIZE);

	if (pagesize) {
		if ((pad = ((int)sbrk(0) % pagesize)) > 0) {
			pad = pagesize - pad;
			(void) sbrk(pad);
		}
	}
	return ((char *)sbrk(size));
}

/*
 * bfill: Read req_cnt bytes (out of filelen bytes) from the I/O buffer,
 * moving them to rd_buf_p.  When there are no bytes left in the I/O buffer,
 * Fillbuf is set and the I/O buffer is filled.  The variable dist is the
 * distance to lseek if an I/O error is encountered with the -k option set
 * (converted to a multiple of Bufsize).
 */

static int
bfill(void)
{
	register int i = 0, rv;
	static int eof = 0;

	if (!Dflag) {
	while ((Buffr.b_end_p - Buffr.b_in_p) >= Bufsize) {
		errno = 0;
		if ((rv = g_read(Device, Archive, Buffr.b_in_p, Bufsize)) < 0) {
			if (((Buffr.b_end_p - Buffr.b_in_p) >= Bufsize) &&
			    (Eomflag == 0)) {
				Eomflag = 1;
				return (1);
			}
			if (errno == ENOSPC) {
				(void) chgreel(INPUT);
				if (Hdr_type == BAR) {
					skip_bar_volhdr();
				}
				continue;
			} else if (Args & OCk) {
				if (i++ > MX_SEEKS)
					msg(EXT, "Cannot recover.");
				if (lseek(Archive, Bufsize, SEEK_REL) < 0)
					msg(EXTN, "Cannot lseek()");
				Error_cnt++;
				Buf_error++;
				rv = 0;
				continue;
			} else
				ioerror(INPUT);
		} /* (rv = g_read(Device, Archive ... */
		if (Hdr_type != BAR || rv == Bufsize) {
			Buffr.b_in_p += rv;
			Buffr.b_cnt += (long)rv;
		}
		if (rv == Bufsize) {
			eof = 0;
			Blocks++;
		} else if (rv == 0) {
			if (!eof) {
				eof = 1;
				break;
			}
			(void) chgreel(INPUT);
			eof = 0;	/* reset the eof after chgreel	*/

			/*
			 * if spans multiple volume, skip the volume header of
			 * the next volume so that the file currently being
			 * extracted can continue to be extracted.
			 */
			if (Hdr_type == BAR) {
				skip_bar_volhdr();
			}

			continue;
		} else {
			eof = 0;
			SBlocks += (u_longlong_t) rv;
		}
	} /* (Buffr.b_end_p - Buffr.b_in_p) <= Bufsize */

	} else {			/* Dflag */
		errno = 0;
		if ((rv = g_read(Device, Archive, Buffr.b_in_p, Bufsize)) < 0) {
			return (-1);
		} /* (rv = g_read(Device, Archive ... */
		Buffr.b_in_p += rv;
		Buffr.b_cnt += (long)rv;
		if (rv == Bufsize) {
			eof = 0;
			Blocks++;
		} else if (!rv) {
			if (!eof) {
				eof = 1;
				return (rv);
			}
			return (-1);
		} else {
			eof = 0;
			SBlocks += (u_longlong_t) rv;
		}
	}
	return (rv);
}

/*
 * bflush: Move wr_cnt bytes from data_p into the I/O buffer.  When the
 * I/O buffer is full, Flushbuf is set and the buffer is written out.
 */

static void
bflush(void)
{
	register int rv;

	while (Buffr.b_cnt >= Bufsize) {
		errno = 0;
		if ((rv = g_write(Device, Archive, Buffr.b_out_p,
		    Bufsize)) < 0) {
			if (errno == ENOSPC && !Dflag)
				rv = chgreel(OUTPUT);
			else
				ioerror(OUTPUT);
		}
		Buffr.b_out_p += rv;
		Buffr.b_cnt -= (long)rv;
		if (rv == Bufsize)
			Blocks++;
		else if (rv > 0)
			SBlocks += (u_longlong_t) rv;
	}
	rstbuf();
}

/*
 * chgreel: Determine if end-of-medium has been reached.  If it has,
 * close the current medium and prompt the user for the next medium.
 */

static int
chgreel(int dir)
{
	register int lastchar, tryagain, askagain, rv;
	int tmpdev;
	char str[APATH];
	struct stat statb;

	rv = 0;
	if (fstat(Archive, &statb) < 0)
		msg(EXTN, "Error during stat() of archive");
	if ((statb.st_mode & S_IFMT) != S_IFCHR) {
		if (dir == INPUT) {
			msg(EXT, "%s%s\n",
				"Can't read input:  end of file encountered ",
				"prior to expected end of archive.");
		}
	}
	msg(EPOST, "\007End of medium on \"%s\".", dir ? "output" : "input");
	if (is_floppy(Archive))
		(void) ioctl(Archive, FDEJECT, NULL);
	if ((close(Archive) != 0) && (dir == OUTPUT))
		msg(EXTN, "close error");
	Archive = 0;
	Volcnt++;
	for (;;) {
		if (Rtty_p == (FILE *)NULL)
			Rtty_p = fopen(Ttyname, "r");
		do { /* tryagain */
			if (IOfil_p) {
				do {
					msg(EPOST, gettext(Eom_p), Volcnt);
					if (!Rtty_p || fgets(str, sizeof (str),
					    Rtty_p) == (char *) NULL)
						msg(EXT, "Cannot read tty.");
					askagain = 0;
					switch (*str) {
					case '\n':
						(void) strcpy(str, IOfil_p);
						break;
					case 'q':
						exit(Error_cnt);
					default:
						askagain = 1;
					}
				} while (askagain);
			} else {

				if (Hdr_type == BAR)
					Bar_vol_num++;

				msg(EPOST,
				    "To continue, type device/file name when "
				    "ready.");
				if (!Rtty_p || fgets(str, sizeof (str),
				    Rtty_p) == (char *) NULL)
					msg(EXT, "Cannot read tty.");
				lastchar = strlen(str) - 1;
				if (*(str + lastchar) == '\n') /* remove '\n' */
					*(str + lastchar) = '\0';
				if (!*str)
					exit(Error_cnt);
			}
			tryagain = 0;
			if ((Archive = open(str, dir)) < 0) {
				msg(ERRN, "Cannot open \"%s\"", str);
				tryagain = 1;
			}
		} while (tryagain);
		(void) g_init(&tmpdev, &Archive);
		if (tmpdev != Device)
			msg(EXT, "Cannot change media types in mid-stream.");
		if (dir == INPUT)
			break;
		else { /* dir == OUTPUT */
			errno = 0;
			if ((rv = g_write(Device, Archive, Buffr.b_out_p,
			    Bufsize)) == Bufsize)
				break;
			else
				msg(ERR,
				    "Unable to write this medium, try "
				    "another.");
		}
	} /* ;; */
	Eomflag = 0;
	return (rv);
}

/*
 * ckname: Check filenames against user specified patterns,
 * and/or ask the user for new name when -r is used.
 */

static int
ckname(void)
{
	register int lastchar, len;

	if (G_p->g_namesz > Max_namesz) {
		msg(ERR, "Name exceeds maximum length - skipped.");
		return (F_SKIP);
	}
	if (Pat_pp && !matched())
		return (F_SKIP);
	if ((Args & OCr) && !Adir) { /* rename interactively */
		(void) fprintf(Wtty_p, gettext("Rename \"%s\"? "),
		    G_p->g_nam_p);
		(void) fflush(Wtty_p);
		if (fgets(Renam_p, Max_namesz, Rtty_p) == (char *)NULL)
			msg(EXT, "Cannot read tty.");
		if (feof(Rtty_p))
			exit(Error_cnt);
		lastchar = strlen(Renam_p) - 1;
		if (*(Renam_p + lastchar) == '\n') /* remove trailing '\n' */
			*(Renam_p + lastchar) = '\0';
		if (*Renam_p == '\0') {
			msg(POST, "%s Skipped.", G_p->g_nam_p);
			*G_p->g_nam_p = '\0';
			return (F_SKIP);
		} else if (strcmp(Renam_p, ".")) {
			len = strlen(Renam_p);
			if (len > (int)strlen(G_p->g_nam_p)) {
				if ((G_p->g_nam_p != &nambuf[0]) &&
				    (G_p->g_nam_p != &fullnam[0]))
					free(G_p->g_nam_p);
				G_p->g_nam_p = (char *)malloc((unsigned int)
				    (len + 1));
				if (G_p->g_nam_p == (char *) NULL)
					msg(EXT, "Out of memory.");
			}
			(void) strcpy(G_p->g_nam_p, Renam_p);
		}
	}
	VERBOSE((Args & OCt), G_p->g_nam_p);
	if (Args & OCt)
		return (F_SKIP);
	return (F_EXTR);
}

/*
 * ckopts: Check the validity of all command line options.
 */

static void
ckopts(long mask)
{
	register int oflag;
	register char *t_p;
	register long errmsk;

	if (mask & OCi)
		errmsk = mask & INV_MSK4i;
	else if (mask & OCo)
		errmsk = mask & INV_MSK4o;
	else if (mask & OCp)
		errmsk = mask & INV_MSK4p;
	else {
		msg(ERR, "One of -i, -o or -p must be specified.");
		errmsk = 0;
	}
	if (errmsk) /* if non-zero, invalid options were specified */
		Error_cnt++;
	if ((mask & OCa) && (mask & OCm))
		msg(ERR, "-a and -m are mutually exclusive.");
	if ((mask & OCc) && (mask & OCH) && (strcmp("odc", Hdr_p)))
		msg(ERR, "-c and -H are mutually exclusive.");
	if ((mask & OCv) && (mask & OCV))
		msg(ERR, "-v and -V are mutually exclusive.");
	if ((mask & OCB) && (mask & OCC))
		msg(ERR, "-B and -C are mutually exclusive.");
	if ((mask & OCH) && (mask & OC6))
		msg(ERR, "-H and -6 are mutually exclusive.");
	if ((mask & OCM) && !((mask & OCI) || (mask & OCO)))
		msg(ERR, "-M not meaningful without -O or -I.");
	if ((mask & OCA) && !(mask & OCO))
		msg(ERR, "-A requires the -O option.");
	if (Bufsize <= 0)
		msg(ERR, "Illegal size given for -C option.");
	if (mask & OCH) {
		t_p = Hdr_p;
		while (*t_p != NULL) {
			if (isupper(*t_p))
				*t_p = 'a' + (*t_p - 'A');
			t_p++;
		}
		if (!(strcmp("odc", Hdr_p))) {
			Hdr_type = CHR;
			Max_namesz = CPATH;
			Onecopy = 0;
			Oldstat = 1;
		} else if (!(strcmp("crc", Hdr_p))) {
			Hdr_type = CRC;
			Max_namesz = APATH;
			Onecopy = 1;
		} else if (!(strcmp("tar", Hdr_p))) {
			if (Args & OCo) {
				Hdr_type = USTAR;
				Max_namesz = HNAMLEN - 1;
			} else {
				Hdr_type = TAR;
				Max_namesz = TNAMLEN - 1;
			}
			Onecopy = 0;
		} else if (!(strcmp("ustar", Hdr_p))) {
			Hdr_type = USTAR;
			Max_namesz = HNAMLEN - 1;
			Onecopy = 0;
		} else if (!(strcmp("bar", Hdr_p))) {
			if ((Args & OCo) || (Args & OCp))
				msg(ERR,
				    "Header type bar can only be used with -i");
			if (Args & OCP)
				msg(ERR,
				    "Can't preserve using bar header");
			Hdr_type = BAR;
			Max_namesz = TNAMLEN - 1;
			Onecopy = 0;
		} else
			msg(ERR, "Invalid header \"%s\" specified", Hdr_p);
	}
	if (mask & OCr) {
		Rtty_p = fopen(Ttyname, "r");
		Wtty_p = fopen(Ttyname, "w");
		if (Rtty_p == (FILE *)NULL || Wtty_p == (FILE *)NULL)
			msg(ERR, "Cannot rename, \"%s\" missing", Ttyname);
	}
	if ((mask & OCE) && (Ef_p = fopen(Efil_p, "r")) == (FILE *)NULL)
		msg(ERR, "Cannot open \"%s\" to read patterns", Efil_p);
	if ((mask & OCI) && (Archive = open(IOfil_p, O_RDONLY)) < 0)
		msg(ERR, "Cannot open \"%s\" for input", IOfil_p);
	if (mask & OCO) {
		if (mask & OCA) {
			if ((Archive = open(IOfil_p, O_RDWR)) < 0)
				msg(ERR,
				    "Cannot open \"%s\" for append",
				    IOfil_p);
		} else {
			oflag = (O_WRONLY | O_CREAT | O_TRUNC);
			if ((Archive = open(IOfil_p, oflag, 0777)) < 0)
				msg(ERR,
				    "Cannot open \"%s\" for output",
				    IOfil_p);
		}
	}
	if (mask & OCR) {
		if (Uid != 0)
			msg(ERR, "R option only valid for super-user.");
		if ((Rpw_p = getpwnam(Own_p)) == (struct passwd *)NULL)
			msg(ERR, "\"%s\" is not a valid user id", Own_p);
	}
	if ((mask & OCo) && !(mask & OCO))
		Out_p = stderr;
}

/*
 * cksum: Calculate the simple checksum of a file (CRC) or header
 * (TARTYP (TAR and USTAR)).  For -o and the CRC header, the file is opened and
 * the checksum is calculated.  For -i and the CRC header, the checksum
 * is calculated as each block is transferred from the archive I/O buffer
 * to the file system I/O buffer.  The TARTYP (TAR and USTAR) headers calculate
 * the simple checksum of the header (with the checksum field of the
 * header initialized to all spaces (\040).
 */

static long
cksum(char hdr, int byt_cnt, int *err)
{
	register char *crc_p, *end_p;
	register int cnt;
	register long checksum = 0L, lcnt, have;

	if (err != NULL)
		*err = 0;
	switch (hdr) {
	case CRC:
		if (Args & OCi) { /* do running checksum */
			end_p = Buffr.b_out_p + byt_cnt;
			for (crc_p = Buffr.b_out_p; crc_p < end_p; crc_p++)
				checksum += (long)*crc_p;
			break;
		}
		/* OCo - do checksum of file */
		lcnt = G_p->g_filesz;
		while (lcnt > 0) {
			have = (lcnt < CPIOBSZ) ? lcnt : CPIOBSZ;
			errno = 0;
			if (read(Ifile, Buf_p, have) != have) {
				msg(ERR, "Error computing checksum.");
				if (err != NULL)
					*err = 1;
				break;
			}
			end_p = Buf_p + have;
			for (crc_p = Buf_p; crc_p < end_p; crc_p++)
				checksum += (long)*crc_p;
			lcnt -= have;
		}
		if (lseek(Ifile, (off_t)0, SEEK_ABS) < 0)
			msg(ERRN, "Cannot reset file after checksum");
		break;
	case TARTYP: /* TAR and USTAR */
		crc_p = Thdr_p->tbuf.t_cksum;
		for (cnt = 0; cnt < TCRCLEN; cnt++) {
			*crc_p = '\040';
			crc_p++;
		}
		crc_p = (char *)Thdr_p;
		for (cnt = 0; cnt < TARSZ; cnt++) {
			checksum += (long)*crc_p;
			crc_p++;
		}
		break;
	default:
		msg(EXT, "Impossible header type.");
	} /* hdr */
	return (checksum);
}

/*
 * creat_hdr: Fill in the generic header structure with the specific
 * header information based on the value of Hdr_type.
 */

static int
creat_hdr(void)
{
	register ushort ftype;
	char goodbuf[MAXNAM];
	char junkbuf[NAMSIZ];
	char abuf[PRESIZ];
	char *tmpbuf, *lastslash;
	int split, i;

	ftype = SrcSt.st_mode & Ftype;
	Adir = (ftype == S_IFDIR);
	Aspec = (ftype == S_IFBLK || ftype == S_IFCHR || ftype == S_IFIFO);
	switch (Hdr_type) {
	case BIN:
		Gen.g_magic = CMN_BIN;
		break;
	case CHR:
		Gen.g_magic = CMN_BIN;
		break;
	case ASC:
		Gen.g_magic = CMN_ASC;
		break;
	case CRC:
		Gen.g_magic = CMN_CRC;
		break;
	case USTAR:
		/*
		 * If the length of the fullname is greater than 256,
		 * print out a message and return.
		 */
		for (i = 0; i < MAXNAM; i++)
			goodbuf[i] = '\0';
		for (i = 0; i < NAMSIZ; i++)
			junkbuf[i] = '\0';
		for (i = 0; i < PRESIZ; i++)
			abuf[i] = '\0';
		i = 0;

		if ((split = strlen(Gen.g_nam_p)) > MAXNAM) {
			msg(EPOST,
			    "cpio: %s: file name too long", Gen.g_nam_p);
			return (0);
		} else if (split > NAMSIZ) {
			/*
			 * The length of the fullname is greater than 100, so
			 * we must split the filename from the path
			 */
			(void) strcpy(&goodbuf[0], Gen.g_nam_p);
			tmpbuf = goodbuf;
			lastslash = strrchr(tmpbuf, '/');

			if (lastslash != NULL)
				i = strlen(lastslash++);
			else {
				i = strlen(tmpbuf);
				lastslash = tmpbuf;
			}

			/*
			 * If the filename is greater than 100 we can't
			 * archive the file
			 */
			if (i > NAMSIZ) {
				msg(EPOST,
				    "cpio: %s: filename is greater than %d",
				lastslash, NAMSIZ);
				return (0);
			}
			(void) strncpy(&junkbuf[0], lastslash,
			    strlen(lastslash));
			/*
			 * If the prefix is greater than 155 we can't archive
			 * the file.
			 */
			if ((split - i) > PRESIZ) {
				msg(EPOST,
				    "cpio: %s: prefix is greater than %d",
				Gen.g_nam_p, PRESIZ);
				return (0);
			}
			(void) strncpy(&abuf[0], &goodbuf[0], split - i);
			Gen.g_tname = junkbuf;
			if ((Gen.g_prefix = (char *)malloc(strlen(abuf) + 1)) ==
			    NULL)
				msg(EXT, "Out of memory.");
			(void) strcpy(Gen.g_prefix, abuf);
		} else {
			Gen.g_tname = Gen.g_nam_p;
		}
		(void) strcpy(Gen.g_tmagic, "ustar");
		(void) strcpy(Gen.g_version, "00");

		dpasswd = getpwuid(SrcSt.st_uid);
		if (dpasswd == (struct passwd *) NULL) {
			msg(EPOST,
			    "cpio: could not get passwd information for %s",
			    Gen.g_nam_p);
			Gen.g_uname[0] = '\0';	/* make name null string */
		} else
			(void) strncpy(&Gen.g_uname[0], dpasswd->pw_name, 32);
		dgroup = getgrgid(SrcSt.st_gid);
		if (dgroup == (struct group *) NULL) {
			msg(EPOST,
			    "cpio: could not get group information for %s",
			    Gen.g_nam_p);
			Gen.g_gname[0] = '\0';	/* make name null string */
		} else
			(void) strncpy(&Gen.g_gname[0], dgroup->gr_name, 32);
		switch (ftype) {
			case S_IFDIR:
				Gen.g_typeflag = '5';
				break;
			case S_IFREG:
				Gen.g_typeflag = '0';
				break;
			case S_IFLNK:
				Gen.g_typeflag = '2';
				break;
			case S_IFBLK:
				Gen.g_typeflag = '4';
				break;
			case S_IFCHR:
				Gen.g_typeflag = '3';
				break;
			case S_IFIFO:
				Gen.g_typeflag = '6';
				break;
		}
	/* FALLTHROUGH */
	case TAR:
		(void) memset(T_lname, '\0', 100);
		break;
	default:
		msg(EXT, "Impossible header type.");
	}

	Gen.g_namesz = strlen(Gen.g_nam_p) + 1;
	Gen.g_uid = SrcSt.st_uid;
	Gen.g_gid = SrcSt.st_gid;
	Gen.g_dev = SrcSt.st_dev;
	Gen.g_ino = SrcSt.st_ino;
	Gen.g_mode = SrcSt.st_mode;
	Gen.g_mtime = SrcSt.st_mtime;
	Gen.g_nlink = SrcSt.st_nlink;
	if (ftype == S_IFREG || ftype == S_IFLNK)
		Gen.g_filesz = (off_t) SrcSt.st_size;
	else
		Gen.g_filesz = (off_t) 0;
	Gen.g_rdev = SrcSt.st_rdev;
	return (1);
}

/*
 * creat_lnk: Create a link from the existing name1_p to name2_p.
 */

static
int
creat_lnk(char *name1_p, char *name2_p)
{
	register int cnt = 0;

	do {
		errno = 0;
		if (!link(name1_p, name2_p)) {
			if (aclp != NULL) {
				free(aclp);
				aclp = NULL;
			}
			cnt = 0;
			break;
		} else if (errno == EEXIST) {
			if (!(Args & OCu) && G_p->g_mtime <= DesSt.st_mtime)
				msg(ERR,
				    "Existing \"%s\" same age or newer",
				    name2_p);
			else if (unlink(name2_p) < 0)
				msg(ERRN, "Error cannot unlink \"%s\"",
				    name2_p);
		}
		cnt++;
	} while ((cnt < 2) && !missdir(name2_p));
	if (!cnt) {
		if (Args & OCv)
			(void) fprintf(Err_p, gettext("%s linked to %s\n"),
				name2_p, name1_p);
		VERBOSE((Args & (OCv | OCV)), name2_p);
	} else if (cnt == 1)
		msg(ERRN,
		    "Unable to create directory for \"%s\"", name2_p);
	else if (cnt == 2)
		msg(ERRN,
		    "Cannot link \"%s\" and \"%s\"", name1_p, name2_p);
	return (cnt);
}

/*
 * creat_spec:
 */

static int
creat_spec(void)
{
	register char *nam_p;
	register int cnt, result, rv = 0;
	char *curdir;
	char *lastslash;

	Do_rename = 0;	/* creat_tmp() may reset this */
	if (Args & OCp)
		nam_p = Fullnam_p;
	else
		nam_p = G_p->g_nam_p;
	result = stat(nam_p, &DesSt);
	if (ustar_dir() || Adir) {
		curdir = strrchr(nam_p, '.');
		if (curdir != NULL && curdir[1] == NULL) {
			lastslash = strrchr(nam_p, '/');
			/* Return for "." & ".." only */
			if (lastslash != NULL)
				lastslash++;
			else
				lastslash = nam_p;
			if (!(strcmp(lastslash, ".")) ||
			    !(strcmp(lastslash, "..")))
					return (1);
		} else {
			if (!result && (Args & OCd)) {
				rstfiles(U_KEEP);
				return (1);
			}
			if (!result || !(Args & OCd))
				return (1);
		}
	} else if (!result && creat_tmp(nam_p) < 0)
		return (0);
	cnt = 0;
	do {
		if (ustar_dir() || Adir)
			result = mkdir(nam_p, G_p->g_mode);
		else if (ustar_spec() || Aspec)
			result = mknod(nam_p, (int)G_p->g_mode,
			    (int)G_p->g_rdev);
		if (result >= 0) {
			/* acl support */
			if (Pflag && aclp != NULL) {
				if (acl(nam_p, SETACL, aclcnt, aclp) < 0) {
					msg(ERRN,
					    "\"%s\": failed to set acl", nam_p);
				}
				free(aclp);
				aclp = NULL;
			}
			cnt = 0;
			break;
		}
		cnt++;
	} while (cnt < 2 && !missdir(nam_p));
	switch (cnt) {
	case 0:
		rv = 1;
		rstfiles(U_OVER);
		break;
	case 1:
		msg(ERRN,
		    "Cannot create directory for \"%s\"", nam_p);
		if (*Over_p == '\0')
			rstfiles(U_KEEP);
		break;
	case 2:
		if (ustar_dir() || Adir)
			msg(ERRN, "Cannot create directory \"%s\"", nam_p);
		else if (ustar_spec() || Aspec)
			msg(ERRN, "Cannot mknod() \"%s\"", nam_p);
		if (*Over_p == '\0')
			rstfiles(U_KEEP);
		break;
	default:
		msg(EXT, "Impossible case.");
	}
	return (rv);
}

/*
 * creat_tmp:
 */

static int
creat_tmp(char *nam_p)
{
	register char *t_p;
	int	cwd;

	if ((Args & OCp) && G_p->g_ino == DesSt.st_ino &&
	    G_p->g_dev == DesSt.st_dev) {
		msg(ERR, "Attempt to pass a file to itself.");
		return (-1);
	}
	if (G_p->g_mtime <= DesSt.st_mtime && !(Args & OCu)) {
		msg(ERR, "Existing \"%s\" same age or newer", nam_p);
		return (-1);
	}
	if (Uid && Aspec) {
		msg(ERR, "Cannot overwrite \"%s\"", nam_p);
		return (-1);
	}
	(void) strcpy(Over_p, nam_p);
	t_p = Over_p + strlen(Over_p);
	while (t_p != Over_p) {
		if (*(t_p - 1) == '/')
			break;
		t_p--;
	}
	(void) strcpy(t_p, "XXXXXX");
	(void) mktemp(Over_p);
	if (*Over_p == '\0') {
		msg(ERR, "Cannot get temporary file name.");
		return (-1);
	}
	/*
	 * If it's a regular file, write to the temporary file then
	 * rename in order to accomodate potential executables.
	 */
	if (G_p->g_typeflag == 0 &&
	    (DesSt.st_mode & (ulong)Ftype) == S_IFREG &&
	    (G_p->g_mode & (ulong)Ftype) == S_IFREG) {
		/* We write to the temporary file in this case */
		if (Args & OCp)
			Fullnam_p = Over_p;
		else
			G_p->g_nam_p = Over_p;

		Over_p = nam_p;

		Do_rename = 1;
	} else {
		Do_rename = 0;

		if (S_ISDIR(DesSt.st_mode)) {
			/*
			 * Save the current working directory because we
			 * will want to restore it back just in case
			 * remove_dir() fails or get confused about where
			 * we should be
			 */
			cwd = save_cwd();
			if (remove_dir(nam_p) < 0) {
				msg(ERRN,
				    "Cannot remove the directory \"%s\"",
				    nam_p);
				(void) unlink(Over_p);
				*Over_p = '\0';
				/*
				 * Resote working directory back to the one
				 * saved earlier
				 */
				rest_cwd(cwd);
				return (-1);
			}
			/*
			 * Resote working directory back to the one
			 * saved earlier
			 */
			rest_cwd(cwd);
		} else {
			/*
			 * Otherwise, use the original link/unlink construct.
			 */
			if (link(nam_p, Over_p) < 0) {
				msg(ERRN, "Cannot create temporary file");
				return (-1);
			}
			if (unlink(nam_p) < 0) {
				msg(ERRN, "Cannot unlink() current \"%s\"",
				    nam_p);
				(void) unlink(Over_p);
				*Over_p = '\0';
				return (-1);
			}
		}
	}
	return (1);
}

/*
 * data_in:  If proc_mode == P_PROC, bread() the file's data from the archive
 * and write(2) it to the open fdes gotten from openout().  If proc_mode ==
 * P_SKIP, or becomes P_SKIP (due to errors etc), bread(2) the file's data
 * and ignore it.  If the user specified any of the "swap" options (b, s or S),
 * and the length of the file is not appropriate for that action, do not
 * perform the "swap", otherwise perform the action on a buffer by buffer basis.
 * If the CRC header was selected, calculate a running checksum as each buffer
 * is processed.
 */

static void
data_in(int proc_mode)
{
	register char *nam_p;
	register int cnt, pad;
	register long cksumval = 0L;
	off_t filesz;
	register int rv, swapfile = 0;
	int compress_flag = 0;	/* if the file is compressed */
	int cstatus = 0;
	FILE *pipef;		/* pipe for bar to do de-compression */

	nam_p = G_p->g_nam_p;
	if (((G_p->g_mode & Ftype) == S_IFLNK && proc_mode != P_SKIP) ||
	    (Hdr_type == BAR && bar_linkflag == '2' && proc_mode != P_SKIP)) {
		proc_mode = P_SKIP;
		VERBOSE((Args & (OCv | OCV)), nam_p);
	}
	if (Args & (OCb | OCs | OCS)) { /* verfify that swapping is possible */
		swapfile = 1;
		if (Args & (OCs | OCb) && G_p->g_filesz % 2) {
			msg(ERR,
			    "Cannot swap bytes of \"%s\", odd number of bytes",
			    nam_p);
			swapfile = 0;
		}
		if (Args & (OCS | OCb) && G_p->g_filesz % 4) {
			msg(ERR,
			    "Cannot swap halfwords of \"%s\", odd number "
			    "of halfwords", nam_p);
			swapfile = 0;
		}
	}
	filesz = G_p->g_filesz;

	/* This writes out the file from the archive */
	while (filesz > 0) {
		cnt = (int)(filesz > CPIOBSZ) ? CPIOBSZ : filesz;
		FILL(cnt);
		if (proc_mode != P_SKIP) {
			if (Hdr_type == CRC)
				cksumval += cksum(CRC, cnt, NULL);
			if (swapfile)
				swap(Buffr.b_out_p, cnt);
			errno = 0;

			/*
			 * if the bar archive is compressed, set up a pipe and
			 * do the de-compression while reading in the file
			 */
			if (Hdr_type == BAR) {
				if (compress_flag == 0 && Compressed) {
					setup_uncompress(&pipef);
					compress_flag++;
				}

			}

			rv = write(Ofile, Buffr.b_out_p, cnt);
			if (rv < cnt) {
				if (rv < 0)
					msg(ERRN, "Cannot write \"%s\"", nam_p);
				else
					msg(EXTN, "Cannot write \"%s\"", nam_p);
				proc_mode = P_SKIP;
				cstatus = close(Ofile);
				Ofile = 0;
				rstfiles(U_KEEP);
				if (cstatus != 0)
					msg(EXTN, "close error");
			}
		}
		Buffr.b_out_p += cnt;
		Buffr.b_cnt -= (long)cnt;
		filesz -= (off_t)cnt;
	} /* filesz */

	pad = (Pad_val + 1 - (G_p->g_filesz & Pad_val)) & Pad_val;
	if (pad != 0) {
		FILL(pad);
		Buffr.b_out_p += pad;
		Buffr.b_cnt -= pad;
	}
	if (proc_mode != P_SKIP) {
		if (Hdr_type == BAR && compress_flag)
			(void) pclose(pipef);
		else
			cstatus = close(Ofile);
		Ofile = 0;
		if (Hdr_type == CRC && Gen.g_cksum != cksumval) {
			msg(ERR, "\"%s\" - checksum error", nam_p);
			rstfiles(U_KEEP);
		} else
			rstfiles(U_OVER);
		if (cstatus != 0)
			msg(EXTN, "close error");
	}
	VERBOSE((proc_mode != P_SKIP && (Args & (OCv | OCV))), G_p->g_nam_p);
	Finished = 1;
}

/*
 * data_out:  open(2) the file to be archived, compute the checksum
 * of it's data if the CRC header was specified and write the header.
 * read(2) each block of data and bwrite() it to the archive.  For TARTYP (TAR
 * and USTAR) archives, pad the data with NULLs to the next 512 byte boundary.
 */

static void
data_out(void)
{
	register char *nam_p;
	register int cnt, rv, pad;
	off_t filesz;
	long	csum;
	int	errret = 0;

	nam_p = G_p->g_nam_p;
	if (Aspec) {
		if (Pflag && aclp != NULL) {
			char    *secinfo = NULL;
			int	len = 0;

			/* append security attributes */
			if ((append_secattr(&secinfo, &len, aclcnt, (char *)
				aclp, UFSD_ACL)) == -1)
				msg(ERR,
				    "can create security information");

			/* call append_secattr() if more than one */

			if (len > 0) {
			/* write ancillary only if there is sec info */
				(void) write_hdr(1, (off_t)len);
				(void) write_ancillary(secinfo, len);
			}
		}
		write_hdr(0, (off_t)0);
		rstfiles(U_KEEP);
		VERBOSE((Args & (OCv | OCV)), nam_p);
		return;
	}
	if ((G_p->g_mode & Ftype) == S_IFLNK && (Hdr_type !=
	    USTAR && Hdr_type != TAR)) { /* symbolic link */
		write_hdr(0, (off_t)0);
		FLUSH(G_p->g_filesz);
		errno = 0;
		if (readlink(nam_p, Buffr.b_in_p, G_p->g_filesz) < 0) {
			msg(ERRN, "Cannot read symbolic link \"%s\"", nam_p);
			return;
		}
		Buffr.b_in_p += G_p->g_filesz;
		Buffr.b_cnt += G_p->g_filesz;
		pad = (Pad_val + 1 - (G_p->g_filesz & Pad_val)) & Pad_val;
		if (pad != 0) {
			FLUSH(pad);
			(void) memcpy(Buffr.b_in_p, Empty, pad);
			Buffr.b_in_p += pad;
			Buffr.b_cnt += pad;
		}
		VERBOSE((Args & (OCv | OCV)), nam_p);
		return;
	} else if ((G_p->g_mode & Ftype) == S_IFLNK && (Hdr_type ==
	    USTAR || Hdr_type == TAR)) {
		if (readlink(nam_p, T_lname, G_p->g_filesz) < 0) {
			msg(ERRN,
			    "Cannot read symbolic link \"%s\"", nam_p);
			return;
		}
		G_p->g_filesz = (off_t) 0;
		write_hdr(0, (off_t)0);
		VERBOSE((Args & (OCv | OCV)), nam_p);
		return;
	}
	if ((Ifile = open(nam_p, O_RDONLY)) < 0) {
		msg(ERR, "\"%s\" ?", nam_p);
		return;
	}
	if (Hdr_type == CRC) {
		csum = cksum(CRC, 0, &errret);
		if (errret != 0) {
			G_p->g_cksum = -1L;
			msg(POST, "\"%s\" skipped", nam_p);
			(void) close(Ifile);
			return;
		}
	}
	G_p->g_cksum = csum;
	/*
	 * ACL has been retrieved in getname().
	 */
	if (Pflag) {
		char    *secinfo = NULL;
		int	len = 0;

		/* append security attributes */
		if ((append_secattr(&secinfo, &len, aclcnt, (char *) aclp,
		    UFSD_ACL)) == -1)
			msg(ERR, "can create security information");

		/* call append_secattr() if more than one */

		if (len > 0) {
		/* write ancillary only if there is sec info */
			(void) write_hdr(1, (off_t)len);
			(void) write_ancillary(secinfo, len);
		}
	}

	write_hdr(0, (off_t)0);
	filesz = G_p->g_filesz;
	while (filesz > 0) {
		cnt = (unsigned)(filesz > CPIOBSZ) ? CPIOBSZ : filesz;
		FLUSH(cnt);
		errno = 0;
		if ((rv = read(Ifile, Buffr.b_in_p, (unsigned)cnt)) < 0) {
			msg(EXTN, "Cannot read \"%s\"", nam_p);
			break;
		}
		Buffr.b_in_p += rv;
		Buffr.b_cnt += (long)rv;
		filesz -= (off_t)rv;
	}
	pad = (Pad_val + 1 - (G_p->g_filesz & Pad_val)) & Pad_val;
	if (pad != 0) {
		FLUSH(pad);
		(void) memcpy(Buffr.b_in_p, Empty, pad);
		Buffr.b_in_p += pad;
		Buffr.b_cnt += pad;
	}
	(void) close(Ifile);
	rstfiles(U_KEEP);
	VERBOSE((Args & (OCv | OCV)), G_p->g_nam_p);
}

/*
 * data_pass:  If not a special file (Aspec), open(2) the file to be
 * transferred, read(2) each block of data and write(2) it to the output file
 * Ofile, which was opened in file_pass().
 */

static void
data_pass(void)
{
	register int cnt, done = 1;
	int cstatus;
	off_t filesz;

	if (Aspec) {
		cstatus = close(Ofile);
		Ofile = 0;
		rstfiles(U_KEEP);
		VERBOSE((Args & (OCv | OCV)), Nam_p);
		if (cstatus != 0)
			msg(EXTN, "close error");
		return;
	}
	if ((Ifile = open(Nam_p, 0)) < 0) {
		msg(ERRN, "Cannot open \"%s\", skipped", Nam_p);
		cstatus = close(Ofile);
		Ofile = 0;
		rstfiles(U_KEEP);
		if (cstatus != 0)
			msg(EXTN, "close error");
		return;
	}
	filesz = G_p->g_filesz;
	while (filesz > 0) {
		cnt = (unsigned)(filesz > CPIOBSZ) ? CPIOBSZ : filesz;
		errno = 0;
		if (read(Ifile, Buf_p, (unsigned)cnt) < 0) {
			msg(ERRN, "Cannot read \"%s\"", Nam_p);
			done = 0;
			break;
		}
		errno = 0;
		if (write(Ofile, Buf_p, (unsigned)cnt) < 0) {
			msg(ERRN, "Cannot write \"%s\"", Fullnam_p);
			done = 0;
			break;
		}
		Blocks += (u_longlong_t)((cnt + (BUFSZ - 1)) / BUFSZ);
		filesz -= (off_t)cnt;
	}
	(void) close(Ifile);
	cstatus = close(Ofile);
	Ofile = 0;
	if (done)
		rstfiles(U_OVER);
	else
		rstfiles(U_KEEP);
	if (cstatus != 0)
		msg(EXTN, "close error");
	VERBOSE((Args & (OCv | OCV)), Fullnam_p);
	Finished = 1;
}

/*
 * file_in:  Process an object from the archive.  If a TARTYP (TAR or USTAR)
 * archive and g_nlink == 1, link this file to the file name in t_linkname
 * and return.  Handle linked files in one of two ways.  If Onecopy == 0, this
 * is an old style (binary or -c) archive, create and extract the data for the
 * first link found, link all subsequent links to this file and skip their data.
 * If Oncecopy == 1, save links until all have been processed, and then
 * process the links first to last checking their names against the patterns
 * and/or asking the user to rename them.  The first link that is accepted
 * for xtraction is created and the data is read from the archive.
 * All subsequent links that are accepted are linked to this file.
 */

static void
file_in(void)
{
	register struct Lnk *l_p, *tl_p;
	int lnkem = 0, cleanup = 0;
	int proc_file;
	struct Lnk *ttl_p;
	int typeflag;
	char savacl;

	G_p = &Gen;

	if (Hdr_type == BAR) {
		bar_file_in();
		return;
	}

	/*
	 * For archives in USTAR format, the files are extracted according
	 * to the typeflag.
	 */
	if (Hdr_type == USTAR || Hdr_type == TAR) {
		typeflag = Thdr_p->tbuf.t_typeflag;
		if (G_p->g_nlink == 1) {		/* hard link */
			if (ckname() != F_SKIP) {
			    int i;
			    char lname[NAMSIZ+1];
			    (void) memset(lname, '\0', sizeof (lname));

			    (void) strncpy(lname, Thdr_p->tbuf.t_linkname,
				NAMSIZ);
			    for (i = 0; i <= NAMSIZ && lname[i] != 0; i++)
				;

			    lname[i] = 0;
			    (void) creat_lnk(&lname[0], G_p->g_nam_p);
			}
			return;
		}
		if (typeflag == '3' || typeflag == '4' || typeflag == '5' ||
		    typeflag == '6') {
			if (ckname() != F_SKIP && creat_spec() > 0)
				VERBOSE((Args & (OCv | OCV)), G_p->g_nam_p);
			return;
		} else if (Adir || Aspec) {
			if ((ckname() == F_SKIP) || (Ofile = openout()) < 0) {
				data_in(P_SKIP);
			} else {
				data_in(P_PROC);
			}
			return;
		}
	}

	if (Adir) {
		if (ckname() != F_SKIP && creat_spec() > 0)
			VERBOSE((Args & (OCv | OCV)), G_p->g_nam_p);
		return;
	}
	if (G_p->g_nlink == 1 || (Hdr_type == TAR ||
	    Hdr_type == USTAR)) {
		if (Aspec) {
			if (ckname() != F_SKIP && creat_spec() > 0)
				VERBOSE((Args & (OCv | OCV)), G_p->g_nam_p);
		} else {
			if ((ckname() == F_SKIP) || (Ofile = openout()) < 0) {
				data_in(P_SKIP);
			} else {
				data_in(P_PROC);
			}
		}
		return;
	}
	tl_p = add_lnk(&ttl_p);
	l_p = ttl_p;
	if (l_p->L_cnt == l_p->L_gen.g_nlink)
		cleanup = 1;
	if (!Onecopy) {
		lnkem = (tl_p != l_p) ? 1 : 0;
		G_p = &tl_p->L_gen;
		if (ckname() == P_SKIP) {
			data_in(P_SKIP);
		} else {
			if (!lnkem) {
				if (Aspec) {
					if (creat_spec() > 0)
						VERBOSE((Args & (OCv | OCV)),
						    G_p->g_nam_p);
				} else if ((Ofile = openout()) < 0) {
					data_in(P_SKIP);
					reclaim(l_p);
				} else {
					data_in(P_PROC);
				}
			} else {
				(void) strcpy(Lnkend_p, l_p->L_gen.g_nam_p);
				(void) strcpy(Full_p, tl_p->L_gen.g_nam_p);
				(void) creat_lnk(Lnkend_p, Full_p);
				data_in(P_SKIP);
				l_p->L_lnk_p = (struct Lnk *)NULL;
				free(tl_p->L_gen.g_nam_p);
				free(tl_p);
			}
		}
	} else { /* Onecopy */
		if (tl_p->L_gen.g_filesz)
			cleanup = 1;
		if (!cleanup)
			return; /* don't do anything yet */
		tl_p = l_p;
		/*
		 * ckname will clear aclchar. We need to keep aclchar for
		 * all links.
		 */
		savacl = aclchar;
		while (tl_p != (struct Lnk *)NULL) {
			G_p = &tl_p->L_gen;
			aclchar = savacl;
			if ((proc_file = ckname()) != F_SKIP) {
				if (l_p->L_data) {
					(void) creat_lnk(l_p->L_gen.g_nam_p,
					    G_p->g_nam_p);
				} else if (Aspec) {
					(void) creat_spec();
					l_p->L_data = 1;
					VERBOSE((Args & (OCv | OCV)),
					    G_p->g_nam_p);
				} else if ((Ofile = openout()) < 0) {
					proc_file = F_SKIP;
				} else {
					data_in(P_PROC);
					l_p->L_data = 1;
				}
			} /* (proc_file = ckname()) != F_SKIP */
			tl_p = tl_p->L_lnk_p;
			if (proc_file == F_SKIP && !cleanup) {
				tl_p->L_nxt_p = l_p->L_nxt_p;
				tl_p->L_bck_p = l_p->L_bck_p;
				l_p->L_bck_p->L_nxt_p = tl_p;
				l_p->L_nxt_p->L_bck_p = tl_p;
				free(l_p->L_gen.g_nam_p);
				free(l_p);
			}
		} /* tl_p->L_lnk_p != (struct Lnk *)NULL */
		if (l_p->L_data == 0) {
			data_in(P_SKIP);
		}
	}
	if (cleanup)
		reclaim(l_p);
}

/*
 * file_out:  If the current file is not a special file (!Aspec) and it
 * is identical to the archive, skip it (do not archive the archive if it
 * is a regular file).  If creating a TARTYP (TAR or USTAR) archive, the first
 * time a link to a file is encountered, write the header and file out normally.
 * Subsequent links to this file put this file name in their t_linkname field.
 * Otherwise, links are handled in one of two ways, for the old headers
 * (i.e. binary and -c), linked files are written out as they are encountered.
 * For the new headers (ASC and CRC), links are saved up until all the links
 * to each file are found.  For a file with n links, write n - 1 headers with
 * g_filesz set to 0, write the final (nth) header with the correct g_filesz
 * value and write the data for the file to the archive.
 */

static
void
file_out(void)
{
	register struct Lnk *l_p, *tl_p;
	register int cleanup = 0;
	struct Lnk *ttl_p;

	G_p = &Gen;
	if (!Aspec && IDENT(SrcSt, ArchSt))
		return; /* do not archive the archive if it's a regular file */
	if (G_p->g_filesz > Max_offset) {
		msg(ERR, "cpio: %s: too large to archive in current mode",
		    G_p->g_nam_p);
		return; /* do not archive if it's too big */
	}
	if (Hdr_type == TAR || Hdr_type == USTAR) { /* TAR and USTAR */
		if (G_p->g_nlink == 1) {
			data_out();
			return;
		}
		tl_p = add_lnk(&ttl_p);
		l_p = ttl_p;
		if (tl_p == l_p) { /* first link to this file encountered */
			data_out();
			return;
		}
		(void) strncpy(T_lname, l_p->L_gen.g_nam_p,
		    l_p->L_gen.g_namesz);

		/*
		 * check if linkname is greater than 100 characters
		 */
		if (strlen(T_lname) > NAMSIZ) {
			msg(EPOST, "cpio: %s: linkname %s is greater than %d",
				    G_p->g_nam_p, T_lname, NAMSIZ);
			return;
		}

		write_hdr(0, (off_t)0);
		VERBOSE((Args & (OCv | OCV)), tl_p->L_gen.g_nam_p);
		free(tl_p->L_gen.g_nam_p);
		free(tl_p);
		return;
	}
	if (Adir) {
		/*
		* ACL has been retrieved in getname().
		*/
		if (Pflag) {
			char    *secinfo = NULL;
			int	len = 0;

			/* append security attributes */
			if ((append_secattr(&secinfo, &len, aclcnt,
			    (char *) aclp, UFSD_ACL)) == -1)
				msg(ERR, "can create security information");

			/* call append_secattr() if more than one */

			if (len > 0) {
			/* write ancillary */
				(void) write_hdr(1, (off_t)len);
				(void) write_ancillary(secinfo, len);
			}
		}

		write_hdr(0, (off_t)0);
		VERBOSE((Args & (OCv | OCV)), G_p->g_nam_p);
		return;
	}
	if (G_p->g_nlink == 1) {
		data_out();
		return;
	} else {
		tl_p = add_lnk(&ttl_p);
		l_p = ttl_p;
		if (l_p->L_cnt == l_p->L_gen.g_nlink)
			cleanup = 1;
		else if (Onecopy)
			return; /* don't process data yet */
	}
	if (Onecopy) {
		tl_p = l_p;
		while (tl_p->L_lnk_p != (struct Lnk *)NULL) {
			G_p = &tl_p->L_gen;
			G_p->g_filesz = 0L;
			/* one link with the acl is sufficient */
			write_hdr(0, (off_t)0);
			VERBOSE((Args & (OCv | OCV)), G_p->g_nam_p);
			tl_p = tl_p->L_lnk_p;
		}
		G_p = &tl_p->L_gen;
	}
	/* old style: has acl and data for every link */
	data_out();
	if (cleanup)
		reclaim(l_p);
}

/*
 * file_pass:  If the -l option is set (link files when possible), and the
 * source and destination file systems are the same, link the source file
 * (G_p->g_nam_p) to the destination file (Fullnam) and return.  If not a
 * linked file, transfer the data.  Otherwise, the first link to a file
 * encountered is transferred normally and subsequent links are linked to it.
 */

static void
file_pass(void)
{
	register struct Lnk *l_p, *tl_p;
	struct Lnk *ttl_p;
	char *save_name;
	int size;

	G_p = &Gen;
	if (Adir && !(Args & OCd)) {
		msg(ERR, "Use -d option to copy \"%s\"", G_p->g_nam_p);
		return;
	}
	save_name = G_p->g_nam_p;
	while (*(G_p->g_nam_p) == '/')
		G_p->g_nam_p++;
	(void) strcpy(Full_p, G_p->g_nam_p);
	if ((Args & OCl) && !Adir && creat_lnk(G_p->g_nam_p, Fullnam_p) == 0)
		return;
	if ((G_p->g_mode & Ftype) == S_IFLNK && !(Args & OCL)) {
		errno = 0;
		if ((size = readlink(save_name, Symlnk_p, MAXPATHLEN)) < 0) {
			msg(ERRN,
			    "Cannot read symbolic link \"%s\"", save_name);
			return;
		}
		errno = 0;
		(void) missdir(Fullnam_p);
		*(Symlnk_p + size) = '\0';
		if (symlink(Symlnk_p, Fullnam_p) < 0) {
			if (errno == EEXIST) {
				if (openout() < 0) {
					return;
				}
			} else {
				msg(ERRN, "Cannot create \"%s\"", Fullnam_p);
				return;
			}
		}
		if (!Uid &&
		    lchown(Fullnam_p, (int)G_p->g_uid, (int)G_p->g_gid) < 0)
			msg(ERRN, "Error during chown() of \"%s\"", Fullnam_p);
		VERBOSE((Args & (OCv | OCV)), Fullnam_p);
		return;
	}
	if (!Adir && G_p->g_nlink > 1) {
		tl_p = add_lnk(&ttl_p);
		l_p = ttl_p;
		if (tl_p == l_p) /* was not found */
			G_p = &tl_p->L_gen;
		else { /* found */
			(void) strcpy(Lnkend_p, l_p->L_gen.g_nam_p);
			(void) strcpy(Full_p, tl_p->L_gen.g_nam_p);
			(void) creat_lnk(Lnknam_p, Fullnam_p);
			l_p->L_lnk_p = (struct Lnk *)NULL;
			free(tl_p->L_gen.g_nam_p);
			free(tl_p);
			if (l_p->L_cnt == G_p->g_nlink)
				reclaim(l_p);
			return;
		}
	}
	if (Adir || Aspec) {
		if (creat_spec() > 0)
			VERBOSE((Args & (OCv | OCV)), Fullnam_p);
	} else if ((Ofile = openout()) > 0)
		data_pass();
}

/*
 * flush_lnks: With new linked file handling, linked files are not archived
 * until all links have been collected.  When the end of the list of filenames
 * to archive has been reached, all files that did not encounter all their links
 * are written out with actual (encountered) link counts.  A file with n links
 * (that are archived) will be represented by n headers (one for each link (the
 * first n - 1 have g_filesz set to 0)) followed by the data for the file.
 */

static void
flush_lnks(void)
{
	register struct Lnk *l_p, *tl_p;
	off_t tfsize;

	l_p = Lnk_hd.L_nxt_p;
	while (l_p != &Lnk_hd) {
		(void) strcpy(Gen.g_nam_p, l_p->L_gen.g_nam_p);
		if (stat(Gen.g_nam_p, &SrcSt) == 0) { /* check if file exists */
			tl_p = l_p;
			(void) creat_hdr();
			Gen.g_nlink = l_p->L_cnt; /* "actual" link count */
			tfsize = Gen.g_filesz;
			Gen.g_filesz = 0L;
			G_p = &Gen;
			while (tl_p != (struct Lnk *)NULL) {
				Gen.g_nam_p = tl_p->L_gen.g_nam_p;
				Gen.g_namesz = tl_p->L_gen.g_namesz;
				if (tl_p->L_lnk_p == (struct Lnk *)NULL) {
					Gen.g_filesz = tfsize;
					data_out();
					break;
				}
				write_hdr(0, (off_t)0); /* header only */
				VERBOSE((Args & (OCv | OCV)), Gen.g_nam_p);
				tl_p = tl_p->L_lnk_p;
			}
			Gen.g_nam_p = Nam_p;
		} else /* stat(Gen.g_nam_p, &SrcSt) == 0 */
			msg(ERR, "\"%s\" has disappeared", Gen.g_nam_p);
		tl_p = l_p;
		l_p = l_p->L_nxt_p;
		reclaim(tl_p);
	} /* l_p != &Lnk_hd */
}

/*
 * gethdr: Get a header from the archive, validate it and check for the trailer.
 * Any user specified Hdr_type is ignored (set to NONE in main).  Hdr_type is
 * set appropriately after a valid header is found.  Unless the -k option is
 * set a corrupted header causes an exit with an error.  I/O errors during
 * examination of any part of the header cause gethdr to throw away any current
 * data and start over.  Other errors during examination of any part of the
 * header cause gethdr to advance one byte and continue the examination.
 */

static int
gethdr(void)
{
	register ushort ftype;
	register int hit = NONE, cnt = 0;
	int goodhdr, hsize, offset;
	int bswap = 0;
	char *preptr;
	int k = 0;
	int j;

	Gen.g_nam_p = Nam_p;
	do { /* hit == NONE && (Args & OCk) && Buffr.b_cnt > 0 */
		FILL(Hdrsz);
		switch (Hdr_type) {
		case NONE:
		case BIN:
			Binmag.b_byte[0] = Buffr.b_out_p[0];
			Binmag.b_byte[1] = Buffr.b_out_p[1];
			if ((Binmag.b_half == CMN_BIN) ||
			    (Binmag.b_half == CMN_BBS)) {
				hit = read_hdr(BIN);
				if (Hdr_type == NONE)
					bswap = 1;
				hsize = HDRSZ + Gen.g_namesz;
				break;
			}
			if (Hdr_type != NONE)
				break;
			/*FALLTHROUGH*/
		case CHR:
			if (!(strncmp(Buffr.b_out_p, CMS_CHR, CMS_LEN))) {
				hit = read_hdr(CHR);
				hsize = CHRSZ + Gen.g_namesz;
				break;
			}
			if (Hdr_type != NONE)
				break;
			/*FALLTHROUGH*/
		case ASC:
			if (!(strncmp(Buffr.b_out_p, CMS_ASC, CMS_LEN))) {
				hit = read_hdr(ASC);
				hsize = ASCSZ + Gen.g_namesz;
				break;
			}
			if (Hdr_type != NONE)
				break;
			/*FALLTHROUGH*/
		case CRC:
			if (!(strncmp(Buffr.b_out_p, CMS_CRC, CMS_LEN))) {
				hit = read_hdr(CRC);
				hsize = ASCSZ + Gen.g_namesz;
				break;
			}
			if (Hdr_type != NONE)
				break;
			/*FALLTHROUGH*/

		case BAR:
			if (Hdr_p != NULL && strcmp(Hdr_p, "bar") == 0) {
				Hdrsz = BARSZ;
				FILL(Hdrsz);
				if ((hit = read_hdr(BAR)) == NONE) {
					Hdrsz = ASCSZ;
					break;
				}
				hit = BAR;
				hsize = BARSZ;
				break;
			}
			/*FALLTHROUGH*/

		case USTAR:
			if (Hdr_p != NULL && strcmp(Hdr_p, "ustar") == 0) {
				Hdrsz = TARSZ;
				FILL(Hdrsz);
				if ((hit = read_hdr(USTAR)) == NONE) {
					Hdrsz = ASCSZ;
					break;
				}
				hit = USTAR;
				hsize = TARSZ;
				break;
			}
			/*FALLTHROUGH*/
		case TAR:
			if (Hdr_p != NULL && strcmp(Hdr_p, "tar") == 0) {
				Hdrsz = TARSZ;
				FILL(Hdrsz);
				if ((hit = read_hdr(TAR)) == NONE) {
					Hdrsz = ASCSZ;
					break;
				}
				hit = TAR;
				hsize = TARSZ;
				break;
			}
			/*FALLTHROUGH*/
		default:
			msg(EXT, "Impossible header type.");
		} /* Hdr_type */
		Gen.g_nam_p = &nambuf[0];
		if (hit != NONE) {
			FILL(hsize);
			goodhdr = 1;
			if (Gen.g_filesz < (off_t) 0 || Gen.g_namesz < 1)
				goodhdr = 0;
			if ((hit != USTAR) && (hit != TAR))
				if (Gen.g_namesz > Max_namesz)
					goodhdr = 0;
			/* TAR and USTAR */
			if ((hit == USTAR) || (hit == TAR)) {
				if (*Gen.g_nam_p == '\0') { /* tar trailer */
					goodhdr = 1;
				} else {

					G_p = &Gen;
					if (G_p->g_cksum !=
					    cksum(TARTYP, 0, NULL)) {
						goodhdr = 0;
						msg(ERR,
						    "Bad header - checksum "
						    "error.");
					}
				}
			} else if (hit != BAR) { /* binary, -c, ASC and CRC */
				if (Gen.g_nlink <= (ulong)0)
					goodhdr = 0;
				if (*(Buffr.b_out_p + hsize - 1) != '\0')
					goodhdr = 0;
			}
			if (!goodhdr) {
				hit = NONE;
				if (!(Args & OCk))
					break;
				msg(ERR,
				    "Corrupt header, file(s) may be lost.");
			} else {
				FILL(hsize);
			}
		} /* hit != NONE */
		if (hit == NONE) {
			Buffr.b_out_p++;
			Buffr.b_cnt--;
			if (!(Args & OCk))
				break;
			if (!cnt++)
				msg(ERR, "Searching for magic number/header.");
		}
	} while (hit == NONE);
	if (hit == NONE) {
		if (Hdr_type == NONE)
			msg(EXT, "Not a cpio file, bad header.");
		else
			msg(EXT, "Bad magic number/header.");
	} else if (cnt > 0) {
		msg(EPOST, "Re-synchronized on magic number/header.");
	}
	if (Hdr_type == NONE) {
		Hdr_type = hit;
		switch (Hdr_type) {
		case BIN:
			if (bswap)
				Args |= BSM;
			Hdrsz = HDRSZ;
			Max_namesz = CPATH;
			Pad_val = HALFWD;
			Onecopy = 0;
			break;
		case CHR:
			Hdrsz = CHRSZ;
			Max_namesz = CPATH;
			Pad_val = 0;
			Onecopy = 0;
			break;
		case ASC:
		case CRC:
			Hdrsz = ASCSZ;
			Max_namesz = APATH;
			Pad_val = FULLWD;
			Onecopy = 1;
			break;
		case USTAR:
			Hdrsz = TARSZ;
			Max_namesz = HNAMLEN - 1;
			Pad_val = FULLBK;
			Onecopy = 0;
			break;
		case BAR:
		case TAR:
			Hdrsz = TARSZ;
			Max_namesz = TNAMLEN - 1;
			Pad_val = FULLBK;
			Onecopy = 0;
			break;
		default:
			msg(EXT, "Impossible header type.");
		} /* Hdr_type */
	} /* Hdr_type == NONE */
	if ((Hdr_type == USTAR) || (Hdr_type == TAR) ||
	    (Hdr_type == BAR)) {			/* TAR, USTAR, BAR */
		Gen.g_namesz = 0;
		if (Gen.g_nam_p[0] == '\0')
			return (0);
		else {
			preptr = &prebuf[0];
			if (*preptr != (char) NULL) {
				k = strlen(&prebuf[0]);
				if (k < PRESIZ) {
					(void) strcpy(&fullnam[0], &prebuf[0]);
					j = 0;
					fullnam[k++] = '/';
					while ((j < NAMSIZ) && (&nambuf[j] !=
					    (char) NULL)) {
						fullnam[k] = nambuf[j];
						k++; j++;
					}
					fullnam[k] = '\0';
				} else if (k >= PRESIZ) {
					k = 0;
					while ((k < PRESIZ) && (prebuf[k] !=
					    (char) NULL)) {
						fullnam[k] = prebuf[k];
						k++;
					}
					fullnam[k++] = '/';
					j = 0;
					while ((j < NAMSIZ) && (nambuf[j] !=
					    (char) NULL)) {
						fullnam[k] = nambuf[j];
						k++; j++;
					}
					fullnam[k] = '\0';
				}
				Gen.g_nam_p = &fullnam[0];
			} else
				Gen.g_nam_p = &nambuf[0];

			/*
			 * initialize the buffer so that the prefix will not
			 * applied to the next entry in the archive
			 */
			(void) memset(prebuf, 0, 155);
		}
	} else if (Hdr_type != BAR) {
		(void) memcpy(Gen.g_nam_p, Buffr.b_out_p + Hdrsz, Gen.g_namesz);
		if (!(strcmp(Gen.g_nam_p, "TRAILER!!!")))
			return (0);
	}
	offset = ((hsize + Pad_val) & ~Pad_val);
	FILL(offset + Hdrsz);
	Thdr_p = (union tblock *) Buffr.b_out_p;
	Buffr.b_out_p += offset;
	Buffr.b_cnt -= (off_t)offset;
	ftype = Gen.g_mode & Ftype;

	/* acl support: grab acl info */
	if ((Gen.g_mode == SECMODE) || ((Hdr_type == USTAR ||
	    Hdr_type == TAR) && Thdr_p->tbuf.t_typeflag == 'A')) {
		/* this is an ancillary file */
		off_t	bytes;
		char	*secp;
		int	pad;
		int	cnt;
		char	*tp;
		int	attrsize;

		if (Pflag) {
			bytes = Gen.g_filesz;
			if ((secp = (char *)malloc((uint) bytes)) == NULL)
				(void) msg(EXT, "out of memory for acl");
			tp = secp;

			while (bytes > 0) {
				cnt = (int)(bytes > CPIOBSZ) ? CPIOBSZ : bytes;
				FILL(cnt);
				(void) memcpy(tp, Buffr.b_out_p, cnt);
				tp += cnt;
				Buffr.b_out_p += cnt;
				Buffr.b_cnt -= (off_t)cnt;
				bytes -= (off_t)cnt;
			}

			pad = (Pad_val + 1 - (Gen.g_filesz & Pad_val)) &
			    Pad_val;
			if (pad != 0) {
				FILL(pad);
				Buffr.b_out_p += pad;
				Buffr.b_cnt -= (off_t) pad;
			}

			/* got all attributes in secp */
			tp = secp;
			do {
				attr = (struct sec_attr *) tp;
				switch (attr->attr_type) {
				case UFSD_ACL:
					(void) sscanf(attr->attr_len, "%7lo",
					    (ulong *) &aclcnt);
				/* header is 8 */
					attrsize = 8 +
					    strlen(&attr->attr_info[0])
					    + 1;
					aclp = aclfromtext(&attr->attr_info[0],
					    &cnt);
					if (aclp == NULL) {
						msg(ERR, "aclfromtext failed");
						break;
					}
					if (aclcnt != cnt) {
						msg(ERR, "acl count error");
						break;
					}
					bytes -= attrsize;
					break;

				/* SunFed case goes here */

					default:
					msg(EXT, "unrecognized attr type");
					break;
			}
			/* next attributes */
			tp += attrsize;
			} while (bytes > 0);
			free(secp);
		} else {
			/* skip security info */
			G_p = &Gen;
			data_in(P_SKIP);
		}
		/*
		 * We already got the file content, dont call file_in()
		 * when return. The new return code(2) is used to
		 *  indicate that.
		 */
		VERBOSE((Args & OCt), Gen.g_nam_p);
		return (2);
	} /* acl */

	Adir = (ftype == S_IFDIR);
	Aspec = (ftype == S_IFBLK || ftype == S_IFCHR || ftype == S_IFIFO);
	return (1);
}

/*
 * getname: Get file names for inclusion in the archive.  When end of file
 * on the input stream of file names is reached, flush the link buffer out.
 * For each filename, remove leading "./"s and multiple "/"s, and remove
 * any trailing newline "\n".  Finally, verify the existance of the file,
 * and call creat_hdr() to fill in the gen_hdr structure.
 */

static int
getname(void)
{
	register int goodfile = 0, lastchar;

	Gen.g_nam_p = Nam_p;
	while (!goodfile) {
		if (fgets(Gen.g_nam_p, APATH, stdin) == (char *)NULL) {
			if (Onecopy && !(Args &OCp))
				flush_lnks();
			return (0);
		}
		while (*Gen.g_nam_p == '.' && Gen.g_nam_p[1] == '/') {
			Gen.g_nam_p += 2;
			while (*Gen.g_nam_p == '/')
				Gen.g_nam_p++;
		}
		lastchar = strlen(Gen.g_nam_p) - 1;
		if (*(Gen.g_nam_p + lastchar) == '\n')
			*(Gen.g_nam_p + lastchar) = '\0';
		if (Oldstat) {
			goodfile = convert_to_old_stat();
		} else {
			if (!lstat(Gen.g_nam_p, &SrcSt)) {
				goodfile = 1;
				if ((SrcSt.st_mode & Ftype) == S_IFLNK &&
				    (Args & OCL)) {
					errno = 0;
					if (stat(Gen.g_nam_p, &SrcSt) < 0) {
						msg(ERRN,
						    "Cannot follow \"%s\"",
						    Gen.g_nam_p);
						goodfile = 0;
					}
				}
			} else
				msg(ERRN,
				    "Error with lstat() of \"%s\"",
				    Gen.g_nam_p);
		}
		switch (Hdr_type) {
		case USTAR:
			break;	/* creat_hdr checks its filename length */
		default:
			if ((goodfile == 1) &&
			    (strlen(Gen.g_nam_p) >= Max_namesz)) {
				msg(ERR, "cpio:  %s name too long.", Nam_p);
				goodfile = 0;
			break;
		}
		}
	}

	/*
	* Get ACL info: dont bother allocating space if there are only
	*  standard permissions, i.e. ACL count < 4
	*/
	if (Pflag) {
		if ((aclcnt = acl(Gen.g_nam_p, GETACLCNT, 0, NULL)) < 0)
			msg(ERRN, "Error with acl() of \"%s\"", Gen.g_nam_p);
		if (aclcnt > MIN_ACL_ENTRIES) {
			if ((aclp = (aclent_t *)malloc(sizeof (aclent_t) *
			    aclcnt)) == NULL) {
				msg(ERRN,
				    "Error with malloc() of \"%d\"", aclcnt);
			}
			if (acl(Gen.g_nam_p, GETACL, aclcnt, aclp) < 0) {
				msg(ERRN,
				    "Error with getacl() of \"%s\"",
				    Gen.g_nam_p);
			}
		}
	/* else: only traditional permissions, so proceed as usual */
	}
	if (creat_hdr())
		return (1);
	else return (2);
}

/*
 * getpats: Save any filenames/patterns specified as arguments.
 * Read additional filenames/patterns from the file specified by the
 * user.  The filenames/patterns must occur one per line.
 */

static void
getpats(int largc, char **largv)
{
	register char **t_pp;
	register int len;
	register unsigned numpat = largc, maxpat = largc + 2;

	if ((Pat_pp = (char **)malloc(maxpat * sizeof (char *))) ==
	    (char **)NULL)
		msg(EXT, "Out of memory.");
	t_pp = Pat_pp;
	while (*largv) {
		if ((*t_pp = (char *)malloc((unsigned int) strlen(*largv) +
		    1)) == (char *)NULL)
			msg(EXT, "Out of memory.");
		(void) strcpy(*t_pp, *largv);
		t_pp++;
		largv++;
	}
	while (fgets(Nam_p, Max_namesz, Ef_p) != (char *)NULL) {
		if (numpat == maxpat - 1) {
			maxpat += 10;
			if ((Pat_pp = (char **)realloc((char *)Pat_pp,
			    maxpat * sizeof (char *))) == (char **)NULL)
				msg(EXT, "Out of memory.");
			t_pp = Pat_pp + numpat;
		}
		len = strlen(Nam_p); /* includes the \n */
		*(Nam_p + len - 1) = '\0'; /* remove the \n */
		*t_pp = (char *)malloc((unsigned int)len);
		if (*t_pp == (char *) NULL)
			msg(EXT, "Out of memory.");
		(void) strcpy(*t_pp, Nam_p);
		t_pp++;
		numpat++;
	}
	*t_pp = (char *)NULL;
}

static void
ioerror(int dir)
{
	register int t_errno;

	t_errno = errno;
	errno = 0;
	if (fstat(Archive, &ArchSt) < 0)
		msg(EXTN, "Error during stat() of archive");
	errno = t_errno;
	if ((ArchSt.st_mode & Ftype) != S_IFCHR) {
		if (dir) {
			if (errno == EFBIG)
				msg(EXT, "ulimit reached for output file.");
			else if (errno == ENOSPC)
				msg(EXT, "No space left for output file.");
			else
				msg(EXTN, "I/O error - cannot continue");
		} else
			msg(EXT, "Unexpected end-of-file encountered.");
	} else
		msg(EXTN, "\007I/O error on \"%s\"", dir ? "output" : "input");
}

/*
 * matched: Determine if a filename matches the specified pattern(s).  If the
 * pattern is matched (the second return), return 0 if -f was specified, else
 * return != 0.  If the pattern is not matched (the first and third
 * returns), return 0 if -f was not specified, else return != 0.
 */

static int
matched(void)
{
	register char *str_p = G_p->g_nam_p;
	register char **pat_pp = Pat_pp;
	register int negatep, result;

	for (pat_pp = Pat_pp; *pat_pp; pat_pp++) {
		negatep = (**pat_pp == '!');

		result = fnmatch(negatep ? (*pat_pp+1) : *pat_pp, str_p, 0);

		if (result != 0 && result != FNM_NOMATCH) {
			msg(POST, "error matching file %s with pattern"
			    " %s\n", str_p, *pat_pp);
			return (Args & OCf);
		}

		if ((result == 0 && ! negatep) ||
		    (result == FNM_NOMATCH && negatep)) {
			/* match occured */
			return (!(Args & OCf));
		}
	}
	return (Args & OCf); /* not matched */
}

/*
 * missdir: Create missing directories for files.
 * (Possible future performance enhancement, if missdir is called, we know
 * that at least the very last directory of the path does not exist, therefore,
 * scan the path from the end
 */

static int
missdir(char *nam_p)
{
	register char *c_p;
	register int cnt = 2;
	char *lastp;

	if (*(c_p = nam_p) == '/') /* skip over 'root slash' */
		c_p++;

	lastp = c_p + strlen(nam_p) - 1;
	if (*lastp == '/')
		*lastp = '\0';

	for (; *c_p; ++c_p) {
		if (*c_p == '/') {
			*c_p = '\0';
			if (stat(nam_p, &DesSt) < 0) {
				if (Args & OCd) {
					cnt = mkdir(nam_p, Def_mode);
					if (cnt != 0) {
						*c_p = '/';
						return (cnt);
					}
				} else {
					msg(ERR, "Missing -d option.");
					*c_p = '/';
					return (-1);
				}
			}
			*c_p = '/';
		}
	}
	if (cnt == 2) /* the file already exists */
		cnt = 0;
	return (cnt);
}

/*
 * mklong: Convert two shorts into one long.  For VAX, Interdata ...
 */

static long
mklong(short v[])
{

	register union swpbuf swp_b;

	swp_b.s_word = 1;
	if (swp_b.s_byte[0]) {
		swp_b.s_half[0] = v[1];
		swp_b.s_half[1] = v[0];
	} else {
		swp_b.s_half[0] = v[0];
		swp_b.s_half[1] = v[1];
	}
	return (swp_b.s_word);
}

/*
 * mkshort: Convert a long into 2 shorts, for VAX, Interdata ...
 */

static void
mkshort(short sval[], long v)
{
	register union swpbuf *swp_p, swp_b;

	swp_p = (union swpbuf *)sval;
	swp_b.s_word = 1;
	if (swp_b.s_byte[0]) {
		swp_b.s_word = v;
		swp_p->s_half[0] = swp_b.s_half[1];
		swp_p->s_half[1] = swp_b.s_half[0];
	} else {
		swp_b.s_word = v;
		swp_p->s_half[0] = swp_b.s_half[0];
		swp_p->s_half[1] = swp_b.s_half[1];
	}
}

/*
 * msg: Print either a message (no error) (POST), an error message with or
 * without the errno (ERRN or ERR), or print an error message with or without
 * the errno and exit (EXTN or EXT).
 */

static
void
/*VARARGS*/
msg(va_alist)
va_dcl
{
	register char *fmt_p;
	register int severity;
	register FILE *file_p;
	va_list v_Args;

	if ((Args & OCV) && Verbcnt) { /* clear current line of dots */
		(void) fputc('\n', Out_p);
		Verbcnt = 0;
	}
	va_start(v_Args);
	severity = va_arg(v_Args, int);
	if (severity == POST)
		file_p = Out_p;
	else
		if (severity == EPOST)
			file_p = Err_p;
		else {
			file_p = Err_p;
			Error_cnt++;
		}
	fmt_p = va_arg(v_Args, char *);
	(void) fflush(Out_p);
	(void) fflush(Err_p);
	if ((severity != POST) && (severity != EPOST))
		(void) fprintf(file_p, "cpio: ");

	/* gettext replaces version of string */

	(void) vfprintf(file_p, gettext(fmt_p), v_Args);
	if (severity == ERRN || severity == EXTN) {
		(void) fprintf(file_p, ", errno %d, ", errno);
		perror("");
	} else
		(void) fprintf(file_p, "\n");
	(void) fflush(file_p);
	va_end(v_Args);
	if (severity == EXT || severity == EXTN) {
		(void) fprintf(file_p, gettext("%d errors\n"), Error_cnt);
		exit(Error_cnt);
	}
}

/*
 * openout: Open files for output and set all necessary information.
 * If the u option is set (unconditionally overwrite existing files),
 * and the current file exists, get a temporary file name from mktemp(3C),
 * link the temporary file to the existing file, and remove the existing file.
 * Finally either creat(2), mkdir(2) or mknod(2) as appropriate.
 *
 */

static int
openout()
{
	register char *nam_p;
	register int cnt, result;

	Do_rename = 0;	/* creat_tmp() may reset this */
	if (Args & OCp)
		nam_p = Fullnam_p;
	else
		nam_p = G_p->g_nam_p;
	if ((Max_filesz != RLIM_INFINITY) &&
	    (Max_filesz < (G_p->g_filesz >> 9))) { /* / 512 */
		msg(ERR, "Skipping \"%s\": exceeds ulimit by %d bytes",
			nam_p, G_p->g_filesz - (Max_filesz << 9)); /* * 512 */
		return (-1);
	}

	if (!lstat(nam_p, &DesSt) && creat_tmp(nam_p) < 0)
		return (-1);

	if (Do_rename) {	/* nam_p was changed by creat_tmp() above. */
		if (Args & OCp)
			nam_p = Fullnam_p;
		else
			nam_p = G_p->g_nam_p;
	}

	cnt = 0;
	do {
		errno = 0;
		if (Hdr_type == TAR && Thdr_p->tbuf.t_typeflag == '2') {
			if ((result = symlink(Thdr_p->tbuf.t_linkname,
			    nam_p)) >= 0) {
				cnt = 0;
				if (Over_p != NULL) {
					(void) unlink(Over_p);
					*Over_p = '\0';
				}
				break;
			}
		} else if (Hdr_type == BAR && bar_linkflag == '2') {
			if ((result = symlink(bar_linkname, nam_p)) >= 0) {
				cnt = 0;
				if (Over_p != NULL) {
					(void) unlink(Over_p);
					*Over_p = '\0';
				}
				break;
			}
		} else if ((G_p->g_mode & Ftype) == S_IFLNK) {
			if ((!(Args & OCp)) && !(Hdr_type == USTAR)) {
			(void) strncpy(Symlnk_p, Buffr.b_out_p, G_p->g_filesz);
			*(Symlnk_p + G_p->g_filesz) = '\0';
			} else if ((!(Args & OCp)) && (Hdr_type == USTAR)) {
				(void) strcpy(Symlnk_p,
				    &Thdr_p->tbuf.t_linkname[0]);
			}
			if ((result = symlink(Symlnk_p, nam_p)) >= 0) {
				cnt = 0;
				if (Over_p != NULL) {
					(void) unlink(Over_p);
					*Over_p = '\0';
				}
				break;
			}
		} else if ((result = creat(nam_p, (int)G_p->g_mode)) >= 0) {
			/* acl support */
			if (Pflag && aclp != NULL) {
				if (acl(nam_p, SETACL, aclcnt, aclp) < 0) {
					msg(ERRN,
					    "\"%s\": failed to set acl", nam_p);
				}
			free(aclp);
			aclp = NULL;
			}
			cnt = 0;
			break;
		}
		cnt++;
	} while (cnt < 2 && !missdir(nam_p));
	switch (cnt) {
	case 0:
		if ((Args & OCi) && (Hdr_type == USTAR))
			setpasswd(nam_p);
		if ((G_p->g_mode & Ftype) == S_IFLNK ||
		    (Hdr_type == BAR && bar_linkflag == '2')) {
			if (!Uid && lchown(nam_p, (int)G_p->g_uid,
			    (int)G_p->g_gid) < 0)
				msg(ERRN,
				    "Error during chown() of \"%s\"",
				    nam_p);
			break;
		}
		if (!Uid && chown(nam_p, (int)G_p->g_uid, (int)G_p->g_gid) < 0)
			msg(ERRN, "Error during chown() of \"%s\"", nam_p);
		break;
	case 1:
		msg(ERRN, "Cannot create directory for \"%s\"", nam_p);
		break;
	case 2:
		msg(ERRN, "Cannot create \"%s\"", nam_p);
		break;
	default:
		msg(EXT, "Impossible case.");
	}
	Finished = 0;
	return (result);
}

/*
 * read_hdr: Transfer headers from the selected format
 * in the archive I/O buffer to the generic structure.
 */

static
int
read_hdr(int hdr)
{
	register int rv = NONE;
	major_t maj, rmaj;
	minor_t min, rmin;
	char tmpnull;
	static int bar_read_cnt = 0;

	if (hdr != BAR) {
		if (Buffr.b_end_p != (Buffr.b_out_p + Hdrsz)) {
			tmpnull = *(Buffr.b_out_p + Hdrsz);
			*(Buffr.b_out_p + Hdrsz) = '\0';
		}
	}

	switch (hdr) {
	case BIN:
		(void) memcpy(&Hdr, Buffr.b_out_p, HDRSZ);
		if (Hdr.h_magic == (short)CMN_BBS) {
			swap((char *)&Hdr, HDRSZ);
		}
		Gen.g_magic = Hdr.h_magic;
		Gen.g_mode = Hdr.h_mode;
		Gen.g_uid = Hdr.h_uid;
		Gen.g_gid = Hdr.h_gid;
		Gen.g_nlink = Hdr.h_nlink;
		Gen.g_mtime = mklong(Hdr.h_mtime);
		Gen.g_ino = Hdr.h_ino;
		Gen.g_dev = Hdr.h_dev;
		Gen.g_rdev = Hdr.h_rdev;
		Gen.g_cksum = 0L;
		Gen.g_filesz = (off_t) mklong(Hdr.h_filesize);
		Gen.g_namesz = Hdr.h_namesize;
		rv = BIN;
		break;
	case CHR:
		if (sscanf(Buffr.b_out_p,
		    "%6lo%6lo%6lo%6lo%6lo%6lo%6lo%6lo%11lo%6o%11llo",
		    &Gen.g_magic, &Gen.g_dev, &Gen.g_ino, &Gen.g_mode,
		    &Gen.g_uid, &Gen.g_gid, &Gen.g_nlink, &Gen.g_rdev,
		    (ulong *) &Gen.g_mtime, (uint *) &Gen.g_namesz,
		    (off_t *) &Gen.g_filesz) == CHR_CNT) {
			rv = CHR;
#define	cpioMAJOR(x)	(int)(((unsigned) x >> 8) & 0x7F)
#define	cpioMINOR(x)	(int)(x & 0xFF)
			maj = cpioMAJOR(Gen.g_dev);
			rmaj = cpioMAJOR(Gen.g_rdev);
			min = cpioMINOR(Gen.g_dev);
			rmin = cpioMINOR(Gen.g_rdev);
			if (Oldstat) {
				/* needs error checking */
				Gen.g_dev = (maj << 8) | min;
				Gen.g_rdev = (rmaj << 8) | rmin;
			} else {
				Gen.g_dev = makedev(maj, min);
				Gen.g_rdev = makedev(rmaj, rmin);
			}
		}
		break;
	case ASC:
	case CRC:
		if (sscanf(Buffr.b_out_p,
		    "%6lx%8lx%8lx%8lx%8lx%8lx%8lx%8llx%8x%8x%8x%8x%8x%8lx",
		    &Gen.g_magic, &Gen.g_ino, &Gen.g_mode, &Gen.g_uid,
		    &Gen.g_gid, &Gen.g_nlink, &Gen.g_mtime,
		    (off_t *) &Gen.g_filesz, (uint *) &maj, (uint *) &min,
		    (uint *) &rmaj, (uint *) &rmin, (uint *) &Gen.g_namesz,
		    &Gen.g_cksum) == ASC_CNT) {
			Gen.g_dev = makedev(maj, min);
			Gen.g_rdev = makedev(rmaj, rmin);
			rv = hdr;
		}
		break;
	case USTAR: /* TAR and USTAR */
		if (*Buffr.b_out_p == '\0') {
			*Gen.g_nam_p = '\0';
			nambuf[0] = '\0';
		} else {
			Thdr_p = (union tblock *)Buffr.b_out_p;
			Gen.g_nam_p[0] = '\0';
			(void) sscanf(Thdr_p->tbuf.t_name, "%100s",
			    (char *) &nambuf);
			(void) sscanf(Thdr_p->tbuf.t_mode, "%8lo",
			    &Gen.g_mode);
			(void) sscanf(Thdr_p->tbuf.t_uid, "%8lo", &Gen.g_uid);
			(void) sscanf(Thdr_p->tbuf.t_gid, "%8lo", &Gen.g_gid);
			(void) sscanf(Thdr_p->tbuf.t_size, "%11llo",
			    &Gen.g_filesz);
			(void) sscanf(Thdr_p->tbuf.t_mtime, "%12lo",
			    (ulong *) &Gen.g_mtime);
			(void) sscanf(Thdr_p->tbuf.t_cksum, "%8lo",
			    (ulong *) &Gen.g_cksum);
			if (Thdr_p->tbuf.t_linkname[0] != (char)NULL)
				Gen.g_nlink = 1;
			else
				Gen.g_nlink = 0;

			switch (Thdr_p->tbuf.t_typeflag) {
			case '2':
				/* Symbolic Link */
				Gen.g_nlink = 2;
				break;
			case '3':
				Gen.g_mode |= (S_IFMT & S_IFCHR);
				break;
			case '4':
				Gen.g_mode |= (S_IFMT & S_IFBLK);
				break;
			case '5':
				Gen.g_mode |= (S_IFMT & S_IFDIR);
				break;
			case '6':
				Gen.g_mode |= (S_IFMT & S_IFIFO);
				break;
			}

			(void) sscanf(Thdr_p->tbuf.t_magic, "%8lo",
			    (ulong *) &Gen.g_tmagic);
			(void) sscanf(Thdr_p->tbuf.t_version, "%8lo",
			    (ulong *) &Gen.g_version);
			(void) sscanf(Thdr_p->tbuf.t_uname, "%32s",
			    (char *) &Gen.g_uname);
			(void) sscanf(Thdr_p->tbuf.t_gname, "%32s",
			    (char *) &Gen.g_gname);
			(void) sscanf(Thdr_p->tbuf.t_devmajor, "%8lo",
			    &Gen.g_dev);
			(void) sscanf(Thdr_p->tbuf.t_devminor, "%8lo",
			    &Gen.g_rdev);
			(void) sscanf(Thdr_p->tbuf.t_prefix, "%155s",
			    (char *) &prebuf);
			Gen.g_namesz = strlen(Gen.g_nam_p) + 1;
			Gen.g_dev = makedev(maj, min);
		}
		rv = USTAR;
		break;
	case TAR:
		if (*Buffr.b_out_p == '\0') {
			*Gen.g_nam_p = '\0';
			nambuf[0] = '\0';
		} else {
			Thdr_p = (union tblock *)Buffr.b_out_p;
			Gen.g_nam_p[0] = '\0';
			(void) sscanf(Thdr_p->tbuf.t_mode, "%lo", &Gen.g_mode);
			(void) sscanf(Thdr_p->tbuf.t_uid, "%lo", &Gen.g_uid);
			(void) sscanf(Thdr_p->tbuf.t_gid, "%lo", &Gen.g_gid);
			(void) sscanf(Thdr_p->tbuf.t_size, "%llo",
			    &Gen.g_filesz);
			(void) sscanf(Thdr_p->tbuf.t_mtime, "%lo",
			    &Gen.g_mtime);
			(void) sscanf(Thdr_p->tbuf.t_cksum, "%lo",
			    &Gen.g_cksum);
			if (Thdr_p->tbuf.t_typeflag == '1')	/* hardlink */
				Gen.g_nlink = 1;
			else
				Gen.g_nlink = 0;
			(void) sscanf(Thdr_p->tbuf.t_name, "%s", Gen.g_nam_p);
			Gen.g_namesz = strlen(Gen.g_nam_p) + 1;
			(void) strcpy(nambuf, Gen.g_nam_p);
		}
		rv = TAR;
		break;
	case BAR:
		if (Bar_vol_num == 0 && bar_read_cnt == 0) {
			read_bar_vol_hdr();
			bar_read_cnt++;
		}
		else
			read_bar_file_hdr();
		rv = BAR;
		break;
	default:
		msg(EXT, "Impossible header type.");
	}

	if (hdr != BAR) {
		if (Buffr.b_end_p != (Buffr.b_out_p + Hdrsz))
			*(Buffr.b_out_p + Hdrsz) = tmpnull;
	}

	return (rv);
}

/*
 * reclaim: Reclaim linked file structure storage.
 */

static void
reclaim(struct Lnk *l_p)
{
	register struct Lnk *tl_p;

	l_p->L_bck_p->L_nxt_p = l_p->L_nxt_p;
	l_p->L_nxt_p->L_bck_p = l_p->L_bck_p;
	while (l_p != (struct Lnk *)NULL) {
		tl_p = l_p->L_lnk_p;
		free(l_p->L_gen.g_nam_p);
		free(l_p);
		l_p = tl_p;
	}
}

/*
 * rstbuf: Reset the I/O buffer, move incomplete potential headers to
 * the front of the buffer and force bread() to refill the buffer.  The
 * return value from bread() is returned (to identify I/O errors).  On the
 * 3B2, reads must begin on a word boundary, therefore, with the -i option,
 * any remaining bytes in the buffer must be moved to the base of the buffer
 * in such a way that the destination locations of subsequent reads are
 * word aligned.
 */

static void
rstbuf(void)
{
	register int pad;

	if ((Args & OCi) || Append) {
if (Buffr.b_out_p != Buffr.b_base_p) {
			pad = ((Buffr.b_cnt + FULLWD) & ~FULLWD);
			Buffr.b_in_p = Buffr.b_base_p + pad;
			pad -= Buffr.b_cnt;
			(void) memcpy(Buffr.b_base_p + pad, Buffr.b_out_p,
			    (int)Buffr.b_cnt);
			Buffr.b_out_p = Buffr.b_base_p + pad;
		}
		if (bfill() < 0)
			msg(EXT, "Unexpected end-of-archive encountered.");
	} else { /* OCo */
		(void) memcpy(Buffr.b_base_p, Buffr.b_out_p, (int)Buffr.b_cnt);
		Buffr.b_out_p = Buffr.b_base_p;
		Buffr.b_in_p = Buffr.b_base_p + Buffr.b_cnt;
	}
}

static void
setpasswd(char *nam)
{
	if ((dpasswd = getpwnam(&Gen.g_uname[0])) == (struct passwd *)NULL) {
		msg(EPOST, "cpio: problem reading passwd entry");
		msg(EPOST, "cpio: %s: owner not changed", nam);
		if (Gen.g_uid == UID_NOBODY && S_ISREG(Gen.g_mode))
			Gen.g_mode &= ~S_ISUID;
	} else
		Gen.g_uid = dpasswd->pw_uid;

	if ((dgroup = getgrnam(&Gen.g_gname[0])) == (struct group *)NULL) {
		msg(EPOST, "cpio: problem reading group entry");
		msg(EPOST, "cpio: %s: group not changed", nam);
		if (Gen.g_gid == GID_NOBODY && S_ISREG(Gen.g_mode))
			Gen.g_mode &= ~S_ISGID;
	} else
		Gen.g_gid = dgroup->gr_gid;
	G_p = &Gen;
}

/*
 * rstfiles:  Perform final changes to the file.  If the -u option is set,
 * and overwrite == U_OVER, remove the temporary file, else if overwrite
 * == U_KEEP, unlink the current file, and restore the existing version
 * of the file.  In addition, where appropriate, set the access or modification
 * times, change the owner and change the modes of the file.
 *
 * Note that if Do_rename is set, then the roles of original and temporary
 * file are reversed. If all went well, we will rename() the temporary file
 * over the original in order to accomodate potentially executing files.
 */
static void
rstfiles(int over)
{
	register char *inam_p, *onam_p, *nam_p;

	if (Args & OCp)
		nam_p = Fullnam_p;
	else
		if (Gen.g_nlink > (ulong)0)
			nam_p = G_p->g_nam_p;
		else
			nam_p = Gen.g_nam_p;

	if ((Args & OCi) && (Hdr_type == USTAR))
		setpasswd(nam_p);
	if (over == U_KEEP && *Over_p != '\0') {
		if (Do_rename) {
			msg(POST, "Restoring existing \"%s\"", Over_p);
		} else {
			msg(POST, "Restoring existing \"%s\"", nam_p);
		}
		(void) unlink(nam_p);	/* delete what we just built */

		/* If the old file needs restoring, do the necessary links */
		if (Do_rename) {
			char *tmp_ptr;

			if (Args & OCp) {
				tmp_ptr = Fullnam_p;
				Fullnam_p = Over_p;
			} else {
				tmp_ptr = G_p->g_nam_p;
				G_p->g_nam_p = Over_p;
			}
			Over_p = tmp_ptr;

			Do_rename = 0;	/* names now have original values */
		} else {
			if (link(Over_p, nam_p) < 0)
				msg(EXTN,
				    "Cannot recover original version of \"%s\"",
				    nam_p);
			if (unlink(Over_p))
				msg(ERRN,
				    "Cannot remove temp file \"%s\"", Over_p);
		}
		*Over_p = '\0';
		return;
	} else if (over == U_OVER && *Over_p != '\0') {
		if (Do_rename) {
			char *tmp_ptr;

			(void) rename(nam_p, Over_p);
			if (Args & OCp) {
				tmp_ptr = Fullnam_p;
				Fullnam_p = Over_p;
				Over_p = tmp_ptr;
			} else {
				tmp_ptr = G_p->g_nam_p;
				G_p->g_nam_p = Over_p;
				Over_p = tmp_ptr;
			}
			Do_rename = 0;	/* names now have original values */
		} else {
			if (unlink(Over_p) < 0)
				msg(ERRN,
				    "Cannot unlink() temp file \"%s\"", Over_p);
		}
		*Over_p = '\0';
	}
	if (Args & OCp) {
		inam_p = Nam_p;
		onam_p = Fullnam_p;
	} else /* OCi only uses onam_p, OCo only uses inam_p */
		inam_p = onam_p = G_p->g_nam_p;

	/*
	 * change the modes back to the ones originally created in the
	 * archive
	 */
	if (Args & OCi) {
		mode_t orig_mask, new_mask;
		struct stat sbuf;

		if (lstat(G_p->g_nam_p, &sbuf) == 0) {
			if ((sbuf.st_mode & Ftype) != S_IFLNK) {
				orig_mask = umask(0);
				new_mask = G_p->g_mode & ~orig_mask;
				if (chmod(G_p->g_nam_p, new_mask) < 0)
					msg(ERRN,
					    "Cannot chmod() \"%s\"",
					    G_p->g_nam_p);
				(void) umask(orig_mask);
			}
		}
	}

	if (Args & OCm)
		set_tym(onam_p, G_p->g_mtime, G_p->g_mtime);
	if (Uid == 0) {
		if (!(Args & OCo) && (chmod(onam_p, (int)G_p->g_mode) < 0))
			msg(ERRN, "Cannot chmod() \"%s\"", onam_p);
	}
	if (Args & OCa)
		set_tym(inam_p, (ulong) SrcSt.st_atime, (ulong) SrcSt.st_mtime);
	if ((Args & OCR) && (chown(onam_p, Rpw_p->pw_uid, Rpw_p->pw_gid) < 0))
		msg(ERRN, "Cannot chown() \"%s\"", onam_p);
	if ((Args & OCp) && !(Args & OCR)) {
		if (!Uid && (chown(onam_p, G_p->g_uid, G_p->g_gid) < 0))
			msg(ERRN, "Cannot chown() \"%s\"", onam_p);
	} else { /* OCi only uses onam_p, OCo only uses inam_p */
		if (!(Args & OCR)) {
			if ((Args & OCi) && !Uid && (chown(inam_p, G_p->g_uid,
			    G_p->g_gid) < 0))
				msg(ERRN, "Cannot chown() \"%s\"", onam_p);
		}
	}
}

/*
 * scan4trail: Scan the archive looking for the trailer.
 * When found, back the archive up over the trailer and overwrite
 * the trailer with the files to be added to the archive.
 */

static void
scan4trail(void)
{
	register int rv;
	register off_t off1, off2;

	Append = 1;
	Hdr_type = NONE;
	G_p = (struct gen_hdr *)NULL;
	while (gethdr()) {
		G_p = &Gen;
		data_in(P_SKIP);
	}
	off1 = Buffr.b_cnt;
	off2 = Bufsize - (Buffr.b_cnt % Bufsize);
	Buffr.b_out_p = Buffr.b_in_p = Buffr.b_base_p;
	Buffr.b_cnt = (off_t) 0;
	if (lseek(Archive, -(off1 + off2), SEEK_REL) < 0)
		msg(EXTN, "Unable to append to this archive");
	if ((rv = g_read(Device, Archive, Buffr.b_in_p, Bufsize)) < 0)
		msg(EXTN, "Cannot append to this archive");
	if (lseek(Archive, (off_t) -rv, SEEK_REL) < 0)
		msg(EXTN, "Unable to append to this archive");
	Buffr.b_cnt = off2;
	Buffr.b_in_p = Buffr.b_base_p + Buffr.b_cnt;
	Append = 0;
}

/*
 * setup:  Perform setup and initialization functions.  Parse the options
 * using getopt(3C), call ckopts to check the options and initialize various
 * structures and pointers.  Specifically, for the -i option, save any
 * patterns, for the -o option, check (via stat(2)) the archive, and for
 * the -p option, validate the destination directory.
 */

static void
setup(int largc, char **largv)
{
	extern int optind;
	extern char *optarg;
	register char	*opts_p = "abcdfiklmoprstuvABC:DE:H:I:LM:O:PR:SV6",
			*dupl_p = "Only one occurrence of -%c allowed";
	register int option;
	int blk_cnt;
	struct rlimit rlim;

	Uid = getuid();
	Hdr_type = BIN;
	Max_offset = (off_t)(BIN_OFFSET_MAX);
	Efil_p = Hdr_p = Own_p = IOfil_p = NULL;
	while ((option = getopt(largc, largv, opts_p)) != EOF) {
		switch (option) {
		case 'a':	/* reset access time */
			Args |= OCa;
			break;
		case 'b':	/* swap bytes and halfwords */
			Args |= OCb;
			break;
		case 'c':	/* select character header */
			Args |= OCc;
			Hdr_type = ASC;
			Onecopy = 1;
			break;
		case 'd':	/* create directories as needed */
			Args |= OCd;
			break;
		case 'f':	/* select files not in patterns */
			Args |= OCf;
			break;
		case 'i':	/* "copy in" */
			Args |= OCi;
			Archive = 0;
			break;
		case 'k':	/* retry after I/O errors */
			Args |= OCk;
			break;
		case 'l':	/* link files when possible */
			Args |= OCl;
			break;
		case 'm':	/* retain modification time */
			Args |= OCm;
			break;
		case 'o':	/* "copy out" */
			Args |= OCo;
			Archive = 1;
			break;
		case 'p':	/* "pass" */
			Args |= OCp;
			break;
		case 'r':	/* rename files interactively */
			Args |= OCr;
			break;
		case 's':	/* swap bytes */
			Args |= OCs;
			break;
		case 't':	/* table of contents */
			Args |= OCt;
			break;
		case 'u':	/* copy unconditionally */
			Args |= OCu;
			break;
		case 'v':	/* verbose - print file names */
			Args |= OCv;
			break;
		case 'A':	/* append to existing archive */
			Args |= OCA;
			break;
		case 'B':	/* set block size to 5120 bytes */
			Args |= OCB;
			Bufsize = 5120;
			break;
		case 'C':	/* set arbitrary block size */
			if (Args & OCC)
				msg(ERR, dupl_p, 'C');
			else {
				Args |= OCC;
				Bufsize = atoi(optarg);
			}
			break;
		case 'D':
			Dflag = 1;
			break;
		case 'E':	/* alternate file for pattern input */
			if (Args & OCE)
				msg(ERR, dupl_p, 'E');
			else {
				Args |= OCE;
				Efil_p = optarg;
			}
			break;
		case 'H':	/* select header type */
			if (Args & OCH)
				msg(ERR, dupl_p, 'H');
			else {
				Args |= OCH;
				Hdr_p = optarg;
			}
			break;
		case 'I':	/* alternate file for archive input */
			if (Args & OCI)
				msg(ERR, dupl_p, 'I');
			else {
				Args |= OCI;
				IOfil_p = optarg;
			}
			break;
		case 'L':	/* follow symbolic links */
			Args |= OCL;
			break;
		case 'M':	/* specify new end-of-media message */
			if (Args & OCM)
				msg(ERR, dupl_p, 'M');
			else {
				Args |= OCM;
				Eom_p = optarg;
			}
			break;
		case 'O':	/* alternate file for archive output */
			if (Args & OCO)
				msg(ERR, dupl_p, 'O');
			else {
				Args |= OCO;
				IOfil_p = optarg;
			}
			break;
		case 'P':	/* preserve acls */
			Args |= OCP;
			Pflag++;
			break;
		case 'R':	/* change owner/group of files */
			if (Args & OCR)
				msg(ERR, dupl_p, 'R');
			else {
				Args |= OCR;
				Own_p = optarg;
			}
			break;
		case 'S':	/* swap halfwords */
			Args |= OCS;
			break;
		case 'V':	/* print a dot '.' for each file */
			Args |= OCV;
			break;
		case '6':	/* for old, sixth-edition files */
			Args |= OC6;
			Ftype = SIXTH;
			break;
		default:
			Error_cnt++;
		} /* option */
	} /* (option = getopt(largc, largv, opts_p)) != EOF */
	largc -= optind;
	largv += optind;
	ckopts(Args);
	if (!Error_cnt) {
		if ((Buf_p = align(CPIOBSZ)) == (char *)-1)
			msg(EXTN, "Out of memory");
		if ((Empty = align(TARSZ)) == (char *)-1)
			msg(EXTN, "Out of memory");
		if ((Args & OCr) && (Renam_p = (char *)malloc(APATH)) ==
		    (char *)NULL)
			msg(EXTN, "Out of memory");
		if ((Symlnk_p = (char *)malloc(APATH)) == (char *)NULL)
			msg(EXTN, "Out of memory");
		if ((Over_p = (char *)malloc(APATH)) == (char *)NULL)
			msg(EXTN, "Out of memory");
		if ((Nam_p = (char *)malloc(APATH)) == (char *)NULL)
			msg(EXTN, "Out of memory");
		if ((Fullnam_p = (char *)malloc(APATH)) == (char *)NULL)
			msg(EXTN, "Out of memory");
		if ((Lnknam_p = (char *)malloc(APATH)) == (char *)NULL)
			msg(EXTN, "Out of memory");
		Gen.g_nam_p = Nam_p;
		if ((Fullnam_p = getcwd((char *)NULL, APATH)) == (char *)NULL)
			msg(EXT, "Unable to determine current directory.");
		if (Args & OCi) {
			if (largc > 0) /* save patterns for -i option, if any */
				Pat_pp = largv;
			if (Args & OCE)
				getpats(largc, largv);
		} else if (Args & OCo) {
			if (largc != 0) /* error if arguments left with -o */
				Error_cnt++;
			else if (fstat(Archive, &ArchSt) < 0)
				msg(ERRN, "Error during stat() of archive");
			switch (Hdr_type) {
			case BIN:
				Hdrsz = HDRSZ;
				Pad_val = HALFWD;
				break;
			case CHR:
				Hdrsz = CHRSZ;
				Pad_val = 0;
				Max_offset = (off_t)(CHAR_OFFSET_MAX);
				break;
			case ASC:
			case CRC:
				Hdrsz = ASCSZ;
				Pad_val = FULLWD;
				break;
			case TAR:
			/* FALLTHROUGH */
			case USTAR: /* TAR and USTAR */
				Hdrsz = TARSZ;
				Pad_val = FULLBK;
				Max_offset = (off_t)(CHAR_OFFSET_MAX);
				break;
			default:
				msg(EXT, "Impossible header type.");
			}
		} else { /* directory must be specified */
			if (largc != 1)
				Error_cnt++;
			else if (access(*largv, 2) < 0)
				msg(ERRN,
				    "Error during access() of \"%s\"", *largv);
		}
	}
	if (Error_cnt)
		usage(); /* exits! */
	if (Args & (OCi | OCo)) {
		if (!Dflag) {
			if (Args & (OCB | OCC)) {
				if (g_init(&Device, &Archive) < 0)
					msg(EXTN,
					    "Error during initialization");
			} else {
				if ((Bufsize = g_init(&Device, &Archive)) < 0)
					msg(EXTN,
					    "Error during initialization");
			}
		}
		blk_cnt = _20K / Bufsize;
		blk_cnt = (blk_cnt >= MX_BUFS) ? blk_cnt : MX_BUFS;
		while (blk_cnt > 1) {
			if ((Buffr.b_base_p = align(Bufsize * blk_cnt)) !=
			    (char *)-1) {
				Buffr.b_out_p = Buffr.b_in_p = Buffr.b_base_p;
				Buffr.b_cnt = 0L;
				Buffr.b_size = (long)(Bufsize * blk_cnt);
				Buffr.b_end_p = Buffr.b_base_p + Buffr.b_size;
				break;
			}
			blk_cnt--;
		}
		if (blk_cnt < 2 || Buffr.b_size < (2 * CPIOBSZ))
			msg(EXT, "Out of memory.");
	}
	if (Args & OCp) { /* get destination directory */
		(void) strcpy(Fullnam_p, *largv);
		if (stat(Fullnam_p, &DesSt) < 0)
			msg(EXTN, "Error during stat() of \"%s\"", Fullnam_p);
		if ((DesSt.st_mode & Ftype) != S_IFDIR)
			msg(EXT, "\"%s\" is not a directory", Fullnam_p);
	}
	Full_p = Fullnam_p + strlen(Fullnam_p) - 1;
	if (*Full_p != '/') {
		Full_p++;
		*Full_p = '/';
	}
	Full_p++;
	*Full_p = '\0';
	(void) strcpy(Lnknam_p, Fullnam_p);
	Lnkend_p = Lnknam_p + strlen(Lnknam_p);
	(void) getrlimit(RLIMIT_FSIZE, &rlim);
	Max_filesz = (off_t) rlim.rlim_cur;
	Lnk_hd.L_nxt_p = Lnk_hd.L_bck_p = &Lnk_hd;
	Lnk_hd.L_lnk_p = (struct Lnk *)NULL;
}

/*
 * set_tym: Set the access and/or modification times for a file.
 */

static void
set_tym(char *nam_p, time_t atime, time_t mtime)
{
	struct utimbuf timev;

	timev.actime = atime;
	timev.modtime = mtime;
	if (utime(nam_p, &timev) < 0) {
		if (Args & OCa)
			msg(ERRN,
			    "Unable to reset access time for \"%s\"",
			    nam_p);
		else
			msg(ERRN,
			    "Unable to reset modification time for \"%s\"",
			    nam_p);
	}
}

/*
 * sigint:  Catch interrupts.  If an interrupt occurs during the extraction
 * of a file from the archive with the -u option set, and the filename did
 * exist, remove the current file and restore the original file.  Then exit.
 */

static void
sigint(int sig)
{
	register char *nam_p;

	(void) signal(SIGINT, SIG_IGN); /* block further signals */
	if (!Finished) {
		if (Args & OCi)
			nam_p = G_p->g_nam_p;
		else /* OCp */
			nam_p = Fullnam_p;
		if (*Over_p != '\0') { /* There is a temp file */
			if (unlink(nam_p))
				msg(ERRN,
				    "Cannot remove incomplete \"%s\"", nam_p);
			if (link(Over_p, nam_p) < 0)
				msg(ERRN,
				    "Cannot recover original \"%s\"", nam_p);
			if (unlink(Over_p))
				msg(ERRN,
				    "Cannot remove temp file \"%s\"", Over_p);
		} else if (unlink(nam_p))
			msg(ERRN,
			    "Cannot remove incomplete \"%s\"", nam_p);
			*Over_p = '\0';
	}
	exit(Error_cnt);
}

/*
 * swap: Swap bytes (-s), halfwords (-S) or or both halfwords and bytes (-b).
 */

static void
swap(char *buf_p, int cnt)
{
	register unsigned char tbyte;
	register int tcnt;
	register int rcnt;
	register ushort thalf;

	rcnt = cnt % 4;
	cnt /= 4;
	if (Args & (OCb | OCs | BSM)) {
		tcnt = cnt;
		Swp_p = (union swpbuf *)buf_p;
		while (tcnt-- > 0) {
			tbyte = Swp_p->s_byte[0];
			Swp_p->s_byte[0] = Swp_p->s_byte[1];
			Swp_p->s_byte[1] = tbyte;
			tbyte = Swp_p->s_byte[2];
			Swp_p->s_byte[2] = Swp_p->s_byte[3];
			Swp_p->s_byte[3] = tbyte;
			Swp_p++;
		}
		if (rcnt >= 2) {
		tbyte = Swp_p->s_byte[0];
		Swp_p->s_byte[0] = Swp_p->s_byte[1];
		Swp_p->s_byte[1] = tbyte;
		tbyte = Swp_p->s_byte[2];
		}
	}
	if (Args & (OCb | OCS)) {
		tcnt = cnt;
		Swp_p = (union swpbuf *)buf_p;
		while (tcnt-- > 0) {
			thalf = Swp_p->s_half[0];
			Swp_p->s_half[0] = Swp_p->s_half[1];
			Swp_p->s_half[1] = thalf;
			Swp_p++;
		}
	}
}

/*
 * usage: Print the usage message on stderr and exit.
 */

static void
usage(void)
{

	(void) fflush(stdout);
	(void) fprintf(stderr, gettext("USAGE:\n"
				"\tcpio -i[bcdfkmrstuvBSV6] [-C size] "
				"[-E file] [-H hdr] [-I file [-M msg]] "
				"[-R id] [patterns]\n"
				"\tcpio -o[acvABLV] [-C size] "
				"[-H hdr] [-O file [-M msg]]\n"
				"\tcpio -p[adlmuvLV] [-R id] directory\n"));
	(void) fflush(stderr);
	exit(Error_cnt);
}

/*
 * verbose: For each file, print either the filename (-v) or a dot (-V).
 * If the -t option (table of contents) is set, print either the filename,
 * or if the -v option is also set, print an "ls -l"-like listing.
 */

static void
verbose(char *nam_p)
{
	register int i, j, temp;
	mode_t mode;
	char modestr[12];

	if ((Gen.g_mode == SECMODE) || ((Hdr_type == USTAR ||
	    Hdr_type == TAR) && Thdr_p->tbuf.t_typeflag == 'A')) {
		/* dont print ancillary file */
		aclchar = '+';
		return;
	}
	for (i = 0; i < 11; i++)
		modestr[i] = '-';
	modestr[i] = '\0';
	modestr[i-1] = aclchar;
	aclchar = ' ';

	if ((Args & OCt) && (Args & OCv)) {
		mode = Gen.g_mode;
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

		if (Hdr_type != BAR) {
			temp = Gen.g_mode & Ftype;
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
				msg(ERR, "Impossible file type");
			}
		} else {		/* bar */
			temp = Gen.g_mode & Ftype;
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
			}
			if (bar_linkflag == '2')
				modestr[0] = 'l';
		}
		if ((S_ISUID & Gen.g_mode) == S_ISUID)
			modestr[3] = 's';
		if ((S_ISVTX & Gen.g_mode) == S_ISVTX)
			modestr[9] = 't';
		if ((S_ISGID & G_p->g_mode) == S_ISGID && modestr[6] == 'x')
			modestr[6] = 's';
		else if ((S_ENFMT & Gen.g_mode) == S_ENFMT && modestr[6] != 'x')
			modestr[6] = 'l';
		if ((Hdr_type == TAR || Hdr_type == USTAR) && Gen.g_nlink == 0)
			(void) printf("%s%4d ", modestr, (int) Gen.g_nlink+1);
		else
			(void) printf("%s%4d ", modestr, (int) Gen.g_nlink);
		if (Lastuid == (int) Gen.g_uid)
			(void) printf("%-9s", Curpw_p->pw_name);
		else {
			if (Curpw_p = getpwuid((int)Gen.g_uid)) {
				(void) printf("%-9s", Curpw_p->pw_name);
				Lastuid = (int)Gen.g_uid;
			} else {
				(void) printf("%-9d", (int) Gen.g_uid);
				Lastuid = -1;
			}
		}
		if (Lastgid == (int)Gen.g_gid)
			(void) printf("%-9s", Curgr_p->gr_name);
		else {
			if (Curgr_p = getgrgid((int)Gen.g_gid)) {
				(void) printf("%-9s", Curgr_p->gr_name);
				Lastgid = (int)Gen.g_gid;
			} else {
				(void) printf("%-9d", (int) Gen.g_gid);
				Lastgid = -1;
			}
		}

		/* print file size */
		if (!Aspec || ((Gen.g_mode & Ftype) == S_IFIFO) ||
		    (Hdr_type == BAR && bar_linkflag == '2')) {
			if (Gen.g_filesz < (1LL << 31))
				(void) printf("%7lld ", Gen.g_filesz);
			else
				(void) printf("%11lld ", Gen.g_filesz);
		} else
			(void) printf("%3d,%3d ", (int) major(Gen.g_rdev),
			    (int) minor(Gen.g_rdev));
		(void) cftime(Time, dcgettext(NULL, FORMAT, LC_TIME),
		    (time_t *)&Gen.g_mtime);
		(void) printf("%s, %s", Time, nam_p);
		if ((Gen.g_mode & Ftype) == S_IFLNK) {
			if (Hdr_type == USTAR || Hdr_type == TAR)
				(void) strcpy(Symlnk_p,
				    Thdr_p->tbuf.t_linkname);
			else {
				(void) strncpy(Symlnk_p, Buffr.b_out_p,
				    Gen.g_filesz);
				*(Symlnk_p + Gen.g_filesz) = '\0';
			}
			(void) printf(" -> %s", Symlnk_p);
		}
		if (Hdr_type == BAR) {
			if (bar_linkflag == '2')
				(void) printf(gettext(" symbolic link to %s"),
					bar_linkname);
			else if (bar_linkflag == '1')
				(void) printf(gettext(" linked to %s"),
					bar_linkname);
		}
		if ((Hdr_type == USTAR || Hdr_type == TAR) &&
		    Thdr_p->tbuf.t_typeflag == '1') {
			(void) printf(gettext(" linked to %s"),
			    Thdr_p->tbuf.t_linkname);
		}
		(void) printf("\n");
	} else if ((Args & OCt) || (Args & OCv)) {
		(void) fputs(nam_p, Out_p);
		(void) fputc('\n', Out_p);
	} else { /* OCV */
		(void) fputc('.', Out_p);
		if (Verbcnt++ >= 49) { /* start a new line of dots */
			Verbcnt = 0;
			(void) fputc('\n', Out_p);
		}
	}
	(void) fflush(Out_p);
}

#define	MK_USHORT(a)	(a & 00000177777)

/*
 * write_hdr: Transfer header information for the generic structure
 * into the format for the selected header and bwrite() the header.
 * ACL support: add two new argumnets. secflag indicates that it's an
 *	ancillary file. len is the size of the file (incl. all security
 *	attributes). We only have acls now.
 */

static void
write_hdr(int secflag, off_t len)
{
	int cnt, pad;
	mode_t mode;
	uid_t uid;
	gid_t gid;
	const char warnfmt[] = "%s : %s";

	if (secflag)
		mode = SECMODE;
	else {
		len = G_p->g_filesz;
		mode = G_p->g_mode;
	}

	uid = G_p->g_uid;
	gid = G_p->g_gid;
	/*
	 * Handle EFT uids and gids.  If they get too big
	 * to be represented in a particular format, force 'em to 'nobody'.
	 */
	switch (Hdr_type) {
	case BIN:			/* 16-bits of u_short */
		if ((u_long)uid > (u_long)USHRT_MAX)
			uid = UID_NOBODY;
		if ((u_long)gid > (u_long)USHRT_MAX)
			gid = GID_NOBODY;
		break;
	case CHR:			/* %.6lo => 262143 base 10 */
		if ((u_long)uid > (u_long)0777777)
			uid = UID_NOBODY;
		if ((u_long)gid > (u_long)0777777)
			gid = GID_NOBODY;
		break;
	case ASC:			/* %.8lx => full 32 bits */
	case CRC:
		break;
	case USTAR:
	case TAR:			/* %.7lo => 2097151 base 10 */
		if ((u_long)uid > (u_long)07777777)
			uid = UID_NOBODY;
		if ((u_long)gid > (u_long)07777777)
			gid = GID_NOBODY;
		break;
	default:
		msg(EXT, "Impossible header type.");
	}

	/*
	 * Since cpio formats -don't- encode the symbolic names, print
	 * a warning message when we map the uid or gid this way.
	 * Also, if the ownership just changed, clear set[ug]id bits
	 *
	 * (Except for USTAR format of course, where we have a string
	 * representation of the username embedded in the header)
	 */
	if (uid != G_p->g_uid && Hdr_type != USTAR) {
		msg(ERR, warnfmt, G_p->g_nam_p,
		    gettext("uid too large for archive format"));
		if (S_ISREG(mode))
			mode &= ~S_ISUID;
	}
	if (gid != G_p->g_gid && Hdr_type != USTAR) {
		msg(ERR, warnfmt, G_p->g_nam_p,
		    gettext("gid too large for archive format"));
		if (S_ISREG(mode))
			mode &= ~S_ISGID;
	}

	switch (Hdr_type) {
	case BIN:
	case CHR:
	case ASC:
	case CRC:
		cnt = Hdrsz + G_p->g_namesz;
		break;
	case TAR:
		/*FALLTHROUGH*/
	case USTAR: /* TAR and USTAR */
		cnt = TARSZ;
		break;
	default:
		msg(EXT, "Impossible header type.");
	}
	FLUSH(cnt);

	switch (Hdr_type) {
	case BIN:
		Hdr.h_magic = (short)G_p->g_magic;
		Hdr.h_dev = G_p->g_dev;
		Hdr.h_ino = G_p->g_ino;
		Hdr.h_uid = uid;
		Hdr.h_gid = gid;
		Hdr.h_mode = mode;
		Hdr.h_nlink = G_p->g_nlink;
		Hdr.h_rdev = G_p->g_rdev;
		mkshort(Hdr.h_mtime, (long)G_p->g_mtime);
		Hdr.h_namesize = (short)G_p->g_namesz;
		mkshort(Hdr.h_filesize, (long)len);
		(void) strcpy(Hdr.h_name, G_p->g_nam_p);
		(void) memcpy(Buffr.b_in_p, &Hdr, cnt);
		break;
	case CHR:
		(void) sprintf(Buffr.b_in_p,
		    "%.6lo%.6lo%.6lo%.6lo%.6lo%.6lo%.6lo%.6lo%.11lo%.6lo%."
		    "11llo%s", G_p->g_magic, G_p->g_dev, G_p->g_ino, mode,
		    uid, gid, G_p->g_nlink, MK_USHORT(G_p->g_rdev),
		    G_p->g_mtime, (long) G_p->g_namesz, (off_t) len,
		    G_p->g_nam_p);
		break;
	case ASC:
	case CRC:
		(void) sprintf(Buffr.b_in_p,
		    "%.6lx%.8lx%.8lx%.8lx%.8lx%.8lx%.8lx%.8lx%.8lx%.8lx%."
		    "8lx%.8lx%.8lx%.8lx%s",
			G_p->g_magic, G_p->g_ino, mode, G_p->g_uid,
			G_p->g_gid, G_p->g_nlink, G_p->g_mtime, (long) len,
			major(G_p->g_dev), minor(G_p->g_dev),
			major(G_p->g_rdev), minor(G_p->g_rdev),
			G_p->g_namesz, G_p->g_cksum, G_p->g_nam_p);
		break;
	case USTAR:
		Thdr_p = (union tblock *)Buffr.b_in_p;
		(void) memcpy(Thdr_p, Empty, TARSZ);
		(void) strncpy(Thdr_p->tbuf.t_name, G_p->g_tname,
		    (int) strlen(G_p->g_tname));
		(void) sprintf(Thdr_p->tbuf.t_mode, "%07o", (int) mode);
		(void) sprintf(Thdr_p->tbuf.t_uid, "%07o", (int) uid);
		(void) sprintf(Thdr_p->tbuf.t_gid, "%07o", (int) gid);
		(void) sprintf(Thdr_p->tbuf.t_size, "%011llo", (off_t) len);
		(void) sprintf(Thdr_p->tbuf.t_mtime, "%011lo", G_p->g_mtime);
		if (secflag)
			Thdr_p->tbuf.t_typeflag = 'A';	/* ACL file type */
		else
			Thdr_p->tbuf.t_typeflag = G_p->g_typeflag;
		if (T_lname[0] != '\0') {
			/*
			 * if not a symbolic link
			 */
			if ((G_p->g_mode & Ftype) != S_IFLNK) {
				Thdr_p->tbuf.t_typeflag = '1';
				(void) sprintf(Thdr_p->tbuf.t_size,
				    "%011lo", 0L);
			}
			(void) strncpy(Thdr_p->tbuf.t_linkname, T_lname,
			    strlen(T_lname));
		}
		(void) sprintf(Thdr_p->tbuf.t_magic, "%s", TMAGIC);
		(void) sprintf(Thdr_p->tbuf.t_version, "%2s", TVERSION);
		(void) sprintf(Thdr_p->tbuf.t_uname, "%s",  G_p->g_uname);
		(void) sprintf(Thdr_p->tbuf.t_gname, "%s", G_p->g_gname);
		(void) sprintf(Thdr_p->tbuf.t_devmajor, "%07o",
		    (int) major(G_p->g_rdev));
		(void) sprintf(Thdr_p->tbuf.t_devminor, "%07o",
		    (int) minor(G_p->g_rdev));
		if (Gen.g_prefix) {
			(void) sprintf(Thdr_p->tbuf.t_prefix, "%s",
			    Gen.g_prefix);
			free(Gen.g_prefix);
			Gen.g_prefix = NULL;
		} else
			(void) sprintf(Thdr_p->tbuf.t_prefix, "%s", "");
		(void) sprintf(Thdr_p->tbuf.t_cksum, "%07o",
		    (int)cksum(TARTYP, 0, NULL));
		break;
	case TAR:
		Thdr_p = (union tblock *)Buffr.b_in_p;
		(void) memcpy(Thdr_p, Empty, TARSZ);
		(void) strncpy(Thdr_p->tbuf.t_name, G_p->g_nam_p,
		    G_p->g_namesz);
		(void) sprintf(Thdr_p->tbuf.t_mode, "%07o ", (int) mode);
		(void) sprintf(Thdr_p->tbuf.t_uid, "%07o ", (int) uid);
		(void) sprintf(Thdr_p->tbuf.t_gid, "%07o ", (int) gid);
		(void) sprintf(Thdr_p->tbuf.t_size, "%011o ", len);
		(void) sprintf(Thdr_p->tbuf.t_mtime, "%011o ",
		    (int) G_p->g_mtime);
		if (T_lname[0] != '\0')
			Thdr_p->tbuf.t_typeflag = '1';
		else
			Thdr_p->tbuf.t_typeflag = '\0';
		(void) strncpy(Thdr_p->tbuf.t_linkname, T_lname,
		    strlen(T_lname));
		break;
	default:
		msg(EXT, "Impossible header type.");
	} /* Hdr_type */

	Buffr.b_in_p += cnt;
	Buffr.b_cnt += cnt;
	pad = ((cnt + Pad_val) & ~Pad_val) - cnt;
	if (pad != 0) {
		FLUSH(pad);
		(void) memcpy(Buffr.b_in_p, Empty, pad);
		Buffr.b_in_p += pad;
		Buffr.b_cnt += pad;
	}
}

/*
 * write_trail: Create the appropriate trailer for the selected header type
 * and bwrite the trailer.  Pad the buffer with nulls out to the next Bufsize
 * boundary, and force a write.  If the write completes, or if the trailer is
 * completely written (but not all of the padding nulls (as can happen on end
 * of medium)) return.  Otherwise, the trailer was not completely written out,
 * so re-pad the buffer with nulls and try again.
 */

static void
write_trail(void)
{
	register int cnt, need;

	switch (Hdr_type) {
	case BIN:
		Gen.g_magic = CMN_BIN;
		break;
	case CHR:
		Gen.g_magic = CMN_BIN;
		break;
	case ASC:
		Gen.g_magic = CMN_ASC;
		break;
	case CRC:
		Gen.g_magic = CMN_CRC;
		break;
	}

	switch (Hdr_type) {
	case BIN:
	case CHR:
	case ASC:
	case CRC:
		Gen.g_mode = Gen.g_uid = Gen.g_gid = 0;
		Gen.g_nlink = 1;
		Gen.g_mtime = Gen.g_ino = Gen.g_dev = 0;
		Gen.g_rdev = Gen.g_cksum = 0;
		Gen.g_filesz = (off_t) 0;
		Gen.g_namesz = strlen("TRAILER!!!") + 1;
		(void) strcpy(Gen.g_nam_p, "TRAILER!!!");
		G_p = &Gen;
		write_hdr(0, (off_t)0);
		break;
	case TAR:
	/*FALLTHROUGH*/
	case USTAR: /* TAR and USTAR */
		for (cnt = 0; cnt < 3; cnt++) {
			FLUSH(TARSZ);
			(void) memcpy(Buffr.b_in_p, Empty, TARSZ);
			Buffr.b_in_p += TARSZ;
			Buffr.b_cnt += TARSZ;
		}
		break;
	default:
		msg(EXT, "Impossible header type.");
	}
	need = Bufsize - (Buffr.b_cnt % Bufsize);
	if (need == Bufsize)
		need = 0;

	while (Buffr.b_cnt > 0) {
		while (need > 0) {
			cnt = (need < TARSZ) ? need : TARSZ;
			need -= cnt;
			FLUSH(cnt);
			(void) memcpy(Buffr.b_in_p, Empty, cnt);
			Buffr.b_in_p += cnt;
			Buffr.b_cnt += cnt;
		}
		bflush();
	}
}

/*
 * if archives in USTAR format, check if typeflag == '5' for directories
 */
static int
ustar_dir(void)
{
	if (Hdr_type == USTAR || Hdr_type == TAR) {
		if (Thdr_p->tbuf.t_typeflag == '5')
			return (1);
	}
	return (0);
}

/*
 * if archives in USTAR format, check if typeflag == '3' || '4' || '6'
 * for character, block, fifo special files
 */
static int
ustar_spec(void)
{
	int typeflag;

	if (Hdr_type == USTAR || Hdr_type == TAR) {
		typeflag = Thdr_p->tbuf.t_typeflag;
		if (typeflag == '3' || typeflag == '4' || typeflag == '6')
			return (1);
	}
	return (0);
}

static int
convert_to_old_stat(void)
{
	extern struct cpioinfo *svr32lstat();
	extern struct cpioinfo *svr32stat();
	int goodfile = 0;

	if ((TmpSt = svr32lstat(Gen.g_nam_p))) {
		if ((TmpSt->st_rdev == (o_dev_t)NODEV) &&
		    (((TmpSt->st_mode & Ftype) == S_IFCHR) ||
			((TmpSt->st_mode & Ftype) == S_IFBLK))) {
			msg(ERR, "Error old format can't support expanded"
			    "types on %s", Gen.g_nam_p);
		} else {
			goodfile = 1;
			if ((TmpSt->st_ino == 0) ||
			    (TmpSt->st_dev == (o_dev_t)NODEV)) {
				/* set to transfer eft file */
				TmpSt->st_ino = 0;
				if (((TmpSt->st_mode & Ftype) != S_IFDIR) &&
				    TmpSt->st_nlink > 1)
					msg(POST,
					    "Warning: file %s has large "
					    "inode/device number - linked "
					    "files will be restored as "
					    "separate files", Gen.g_nam_p);
				/* ensure no links as no ino */
				TmpSt->st_nlink = 1;
			}
			if ((TmpSt->st_mode & Ftype) == S_IFLNK &&
			    (Args & OCL)) {
				errno = 0;
				TmpSt = svr32stat(Gen.g_nam_p);
				if (TmpSt->st_ino == 0) {
					TmpSt->st_nlink = 1;
					msg(POST,
					    "Warning: file %s is a link to a "
					    "file with a large inode - it will "
					    "be restored as a separate file",
					    Gen.g_nam_p);
				}
			}
		}
		if (TmpSt->st_dev < 0)
			SrcSt.st_dev = 0;
		else
			SrcSt.st_dev = (dev_t)TmpSt->st_dev;
		/* -actual- not truncated uid */
		SrcSt.st_uid = TmpSt->st_uid;
		/* -actual- not truncated gid */
		SrcSt.st_gid = TmpSt->st_gid;
		SrcSt.st_ino = (ino_t)TmpSt->st_ino;
		SrcSt.st_mode = (mode_t)TmpSt->st_mode;
		SrcSt.st_mtime = (ulong)TmpSt->st_modtime;
		SrcSt.st_nlink = (nlink_t)TmpSt->st_nlink;
		SrcSt.st_size = (off_t)TmpSt->st_size;
		SrcSt.st_rdev = (dev_t)TmpSt->st_rdev;
	} else
		msg(ERRN,
		    "Error with lstat() of \"%s\"", Gen.g_nam_p);

	return (goodfile);
}

/*
 * In the beginning of each bar archive, there is a header which describes the
 * current volume being created, followed by a header which describes the
 * current file being created, followed by the file itself.  If there is
 * more than one file to be created, a separate header will be created for
 * each additional file.  This structure may be repeated if the bar archive
 * contains multiple volumes.  If a file spans across volumes, its header
 * will not be repeated in the next volume.
 *               +------------------+
 *               |    vol header    |
 *               |------------------|
 *               |   file header i  |     i = 0
 *               |------------------|
 *               |     <file i>     |
 *               |------------------|
 *               |  file header i+1 |
 *               |------------------|
 *               |    <file i+1>    |
 *               |------------------|
 *               |        .         |
 *               |        .         |
 *               |        .         |
 *               +------------------+
 */

/*
 * read in the header that describes the current volume of the bar archive
 * to be extracted.
 */
static void
read_bar_vol_hdr(void)
{
	union b_block *tmp_hdr;

	tmp_hdr = (union b_block *)Buffr.b_out_p;
	if (tmp_hdr->dbuf.bar_magic[0] == BAR_VOLUME_MAGIC) {

		if (bar_Vhdr == NULL) {
			if ((bar_Vhdr = (union b_block *)malloc(TBLOCK))
			    == NULL) {
				(void) fprintf(stderr, gettext(
			"error: cannot malloc for bar volume header\n"));
				exit(1);
			}
		}
		(void) memcpy(&(bar_Vhdr->dbuf), &(tmp_hdr->dbuf), TBLOCK);
	} else {
		(void) fprintf(stderr, gettext(
			"bar error: cannot read volume header\n"));
		exit(1);
	}

	(void) sscanf(bar_Vhdr->dbuf.mode, "%8lo", &Gen_bar_vol.g_mode);
	(void) sscanf(bar_Vhdr->dbuf.uid, "%8d", (int *) &Gen_bar_vol.g_uid);
	(void) sscanf(bar_Vhdr->dbuf.gid, "%8d", (int *) &Gen_bar_vol.g_gid);
	(void) sscanf(bar_Vhdr->dbuf.size, "%12lo",
	    (ulong *) &Gen_bar_vol.g_filesz);
	(void) sscanf(bar_Vhdr->dbuf.mtime, "%12lo", &Gen_bar_vol.g_mtime);
	(void) sscanf(bar_Vhdr->dbuf.chksum, "%8lo", &Gen_bar_vol.g_cksum);

	/* set the compress flag */
	if (bar_Vhdr->dbuf.compressed == '1')
		Compressed = 1;
	else
		Compressed = 0;

	Buffr.b_out_p += 512;
	Buffr.b_cnt -= 512;

	/*
	 * not the first volume; exit
	 */
	if (strcmp(bar_Vhdr->dbuf.volume_num, "1") != 0) {
		(void) fprintf(stderr,
		    gettext("error: This is not volume 1.  "));
		(void) fprintf(stderr, gettext("This is volume %s.  "),
			bar_Vhdr->dbuf.volume_num);
		(void) fprintf(stderr, gettext("Please insert volume 1.\n"));
		exit(1);
	}

	read_bar_file_hdr();
}

/*
 * read in the header that describes the current file to be extracted
 */
static void
read_bar_file_hdr(void)
{
	union b_block *tmp_hdr;
	char *start_of_name, *name_p;
	char *tmp;

	if (*Buffr.b_out_p == '\0') {
		*Gen.g_nam_p = '\0';
		exit(0);
	}

	tmp_hdr = (union b_block *)Buffr.b_out_p;

	tmp = &tmp_hdr->dbuf.mode[1];
	(void) sscanf(tmp, "%8lo", &Gen.g_mode);
	(void) sscanf(tmp_hdr->dbuf.uid, "%8lo", &Gen.g_uid);
	(void) sscanf(tmp_hdr->dbuf.gid, "%8lo", &Gen.g_gid);
	(void) sscanf(tmp_hdr->dbuf.size, "%12lo",
	    (off_t *) &Gen.g_filesz);
	(void) sscanf(tmp_hdr->dbuf.mtime, "%12lo", &Gen.g_mtime);
	(void) sscanf(tmp_hdr->dbuf.chksum, "%8lo", &Gen.g_cksum);
	(void) sscanf(tmp_hdr->dbuf.rdev, "%8lo", &Gen.g_rdev);

#define	to_new_major(x)	(int)((unsigned)((x) & OMAXMAJ) << NBITSMINOR)
#define	to_new_minor(x)	(int)((x) & OMAXMIN)
	Gen.g_rdev = to_new_major(Gen.g_rdev) | to_new_minor(Gen.g_rdev);
	bar_linkflag = tmp_hdr->dbuf.linkflag;
	start_of_name = &tmp_hdr->dbuf.start_of_name;


	name_p = Gen.g_nam_p;
	while (*name_p++ = *start_of_name++)
		;
	*name_p = '\0';
	if (bar_linkflag == '1' || bar_linkflag == '2')
		(void) strcpy(bar_linkname, start_of_name);

	Gen.g_namesz = strlen(Gen.g_nam_p) + 1;
	(void) strcpy(nambuf, Gen.g_nam_p);
}

/*
 * if the bar archive is compressed, set up a pipe and do the de-compression
 * as the compressed file is read in.
 */
static void
setup_uncompress(FILE **pipef)
{
	char *cmd_buf;

	if ((cmd_buf = (char *)malloc(MAXPATHLEN*2)) == NULL) {
		(void) fprintf(stderr, gettext("can't malloc\n"));
		exit(1);
	}

	if (access(Gen.g_nam_p, W_OK) != 0) {
		(void) sprintf(cmd_buf, "chmod +w '%s'; uncompress -c > '%s'; "
		    "chmod 0%o '%s'", Gen.g_nam_p, Gen.g_nam_p,
		    (int) G_p->g_mode, Gen.g_nam_p);
	} else
		(void) sprintf(cmd_buf, "uncompress -c > '%s'", Gen.g_nam_p);

	if ((*pipef = popen(cmd_buf, "w")) == NULL) {
		(void) fprintf(stderr, gettext("error\n"));
		exit(1);
	}

	if (close(Ofile) != 0)
		msg(EXTN, "close error");
	Ofile = fileno(*pipef);
}

/*
 * if the bar archive spans multiple volumes, read in the header that
 * describes the next volume.
 */
static void
skip_bar_volhdr(void)
{
	char *buff;
	union b_block *tmp_hdr;

	if ((buff = (char *)malloc((uint) Bufsize)) == NULL) {
		(void) fprintf(stderr, gettext(
		    "Cannot malloc memory in skip_bar_volhdr\n"));
		exit(1);
	}
	if (g_read(Device, Archive, buff, Bufsize) < 0) {
		(void) fprintf(stderr, gettext(
		    "error in skip_bar_volhdr\n"));
	} else {

		tmp_hdr = (union b_block *)buff;
		if (tmp_hdr->dbuf.bar_magic[0] == BAR_VOLUME_MAGIC) {

			if (bar_Vhdr == NULL) {
				if ((bar_Vhdr = (union b_block *)
				    malloc(TBLOCK)) == NULL) {
					(void) fprintf(stderr, gettext(
					    "error: cannot malloc for bar "
					    "volume header\n"));
					exit(1);
				}
			}
			(void) memcpy(&(bar_Vhdr->dbuf),
			    &(tmp_hdr->dbuf), TBLOCK);
		} else {
			(void) fprintf(stderr, gettext(
			    "cpio error: cannot read bar volume "
			    "header\n"));
			exit(1);
		}

		(void) sscanf(bar_Vhdr->dbuf.mode, "%8lo",
		    &Gen_bar_vol.g_mode);
		(void) sscanf(bar_Vhdr->dbuf.uid, "%8lo",
		    &Gen_bar_vol.g_uid);
		(void) sscanf(bar_Vhdr->dbuf.gid, "%8lo",
		    &Gen_bar_vol.g_gid);
		(void) sscanf(bar_Vhdr->dbuf.size, "%12lo",
		    (off_t *) &Gen_bar_vol.g_filesz);
		(void) sscanf(bar_Vhdr->dbuf.mtime, "%12lo",
		    &Gen_bar_vol.g_mtime);
		(void) sscanf(bar_Vhdr->dbuf.chksum, "%8lo",
		    &Gen_bar_vol.g_cksum);
		if (bar_Vhdr->dbuf.compressed == '1')
			Compressed = 1;
		else
			Compressed = 0;
	}

	/*
	 * Now put the rest of the bytes read in into the data buffer.
	 */
	(void) memcpy(Buffr.b_in_p, &buff[512], (Bufsize - 512));
	Buffr.b_in_p += (Bufsize - 512);
	Buffr.b_cnt += (long)(Bufsize - 512);

	free(buff);
}

/*
 * check the linkflag which indicates the type of the file to be extracted,
 * invoke the corresponding routine to extract the file.
 */
static void
bar_file_in(void)
{
	/*
	 * the file is a directory
	 */
	if (Adir) {
		if (ckname() != F_SKIP && creat_spec() > 0)
			VERBOSE((Args & (OCv | OCV)), G_p->g_nam_p);
		return;
	}

	switch (bar_linkflag) {
	case '0':
		/* regular file */
		if ((ckname() == F_SKIP) || (Ofile = openout()) < 0) {
			data_in(P_SKIP);
		} else {
			data_in(P_PROC);
		}
		break;
	case '1':
		/* hard link */
		if (ckname() == F_SKIP)
			break;
		(void) creat_lnk(bar_linkname, G_p->g_nam_p);
		break;
	case '2':
		/* symbolic link */
		if ((ckname() == F_SKIP) || (Ofile = openout()) < 0) {
			data_in(P_SKIP);
		} else {
			data_in(P_PROC);
		}
		break;
	case '3':
		/* character device or FIFO */
		if (ckname() != F_SKIP && creat_spec() > 0)
			VERBOSE((Args & (OCv | OCV)), G_p->g_nam_p);
		break;
	default:
		(void) fprintf(stderr, gettext("error: unknown file type\n"));
		break;
	}
}


/*
 * This originally came from libgenIO/g_init.c
 * XXX	And it is very broken.
 */

/* #include <sys/statvfs.h> */
#include <ftw.h>
#include <libgenIO.h>

#if 0
/*
 * table of devices major numbers and their device type.
 */

int devices[] = {
	G_NO_DEV,		/* major 0			*/
	G_TM_TAPE,		/*  1: Tapemaster controller    */
	G_NO_DEV,		/* major 2			*/
	G_XY_DISK,		/* 3: xy 450/451 disk ctrls	*/
	G_NO_DEV,		/* major 4			*/
	G_NO_DEV,		/* major 5			*/
	G_NO_DEV,		/* major 6			*/
	G_SD_DISK,		/* 7: sd disks			*/
	G_XT_TAPE,		/* 8: xt tapes			*/
	G_SF_FLOPPY,		/* 9: sf floppy			*/
	G_XD_DISK,		/* 10: xd disk			*/
	G_ST_TAPE,		/* 11: st tape			*/
	G_NS,			/* 12: noswap pseudo-dev	*/
	G_RAM,			/* 13: ram pseudo-dev		*/
	G_FT,			/* 14: tftp			*/
	G_HD,			/* 15: 386 network disk		*/
	G_FD,			/* 16: 386 AT disk		*/
	G_NO_DEV,		/* major 17			*/
	G_NO_DEV,		/* major 18			*/
	G_NO_DEV		/* major 19			*/
	};
#endif


/*
 * g_init: Determine the device being accessed, set the buffer size,
 * and perform any device specific initialization. Since at this point
 * Sun has no system call to read the configuration, the major numbers
 * are assumed to be static and types are figured out as such. However,
 * as a rough estimate, the buffer size for all types is set to 512
 * as a default.
 */

static int
g_init(int *devtype, int *fdes)
{
	int bufsize;
	struct stat st_buf;
	struct statvfs stfs_buf;

	*devtype = G_NO_DEV;
	bufsize = -1;
	if (fstat(*fdes, &st_buf) == -1)
		return (-1);
	if (!(st_buf.st_mode & S_IFCHR) || !(st_buf.st_mode & S_IFBLK)) {
		if (st_buf.st_mode & S_IFIFO) {
			bufsize = 512;
		} else {
			/* find block size for this file system */
			*devtype = G_FILE;
			if (fstatvfs(*fdes, &stfs_buf) < 0) {
					bufsize = -1;
					errno = ENODEV;
			} else
				bufsize = stfs_buf.f_bsize;
		}

		return (bufsize);

	/*
	* We'll have to add a remote attribute to stat but this
	** should work for now.
	*/
	} else if (st_buf.st_dev & 0x8000)	/* if remote  rdev */
		return (512);

#if	0
{
	major_t maj;
	minor_t min;
	maj = major(st_buf.st_rdev);
	min = minor(st_buf.st_rdev);
	/*
	 * If the major device number associated with the device is less than
	 * G_DEV_MAX (30) then it is used to index into an array of device
	 * types. This array is static and compiled into the program. The
	 * problem is that this mapping can change between platforms and
	 * configured by root. So any such mapping can not be compiled into
	 * cpio(1). Since bufsize is always set to 512 by g_init() (provided
	 * that it is valid) devtype is left unchanged from its default value
	 * of G_NO_DEV, which does not matter because this value is only used
	 * inside dead code that has been commented out.
	 *
	 */

	/* prevention */
	if ((int)maj < G_DEV_MAX)
		*devtype = devices[(int)maj];
}
#endif

	switch (*devtype) {
		case G_NO_DEV:
		case G_TM_TAPE:
			bufsize = 512;
			break;
		case G_XY_DISK:
		case G_SD_DISK:
		case G_XT_TAPE:
		case G_SF_FLOPPY:
		case G_XD_DISK:
		case G_ST_TAPE:
			bufsize = 512;	/* for now */
			break;
		case G_NS:
		case G_RAM:
		case G_FT:
		case G_HD:
		case G_FD:
			bufsize = 512;	/* for now */
			break;
		default:
			bufsize = -1;
			errno = ENODEV;
	} /* *devtype */

	if (Hdr_type == BAR) {
		if (is_tape(*fdes)) {
			bufsize = BAR_TAPE_SIZE;
			(void) fprintf(stderr, "Archiving to tape");
			(void) fprintf(stderr, " blocking factor 126\n");
		} else if (is_floppy(*fdes)) {
			bufsize = BAR_FLOPPY_SIZE;
			(void) fprintf(stderr, "Archiving to floppy");
			(void) fprintf(stderr, " blocking factor 18\n");
		}
	}

	return (bufsize);
}

/*
 * This originally came from libgenIO/g_read.c
 */

/*
 * g_read: Read nbytes of data from fdes (of type devtype) and place
 * data in location pointed to by buf.  In case of end of medium,
 * translate (where necessary) device specific EOM indications into
 * the generic EOM indication of rv = -1, errno = ENOSPC.
 */

static int
g_read(int devtype, int fdes, char *buf, unsigned nbytes)
{
	int rv;

	if (devtype < 0 || devtype >= G_DEV_MAX) {
		errno = ENODEV;
		return (-1);
	}
#if 0
	if ((rv = read(fdes, buf, nbytes)) <= 0) {
		switch (devtype) {
			case G_FILE:	/* do not change returns for files */
				break;
			case G_NO_DEV:	/* returns -1 and ENOSPC */
			case G_TAPE:
				break;
			case G_3B2_HD:
			case G_3B2_FD:
			case G_3B2_CTC:
				if (rv == 0 && errno == 0) {
					errno = ENOSPC;
					rv = -1;
				}
				break;
			case G_386_HD:	/* not developed yet */
			case G_386_FD:
			case G_386_Q24:
			case G_SCSI_HD:
			case G_SCSI_FD:
			case G_SCSI_9T:
			case G_SCSI_Q24:
			case G_SCSI_Q120:
				break;
			default:
				rv = -1;
				errno = ENODEV;
		} /* devtype */
	} /* (rv = read(fdes, buf, nbytes)) <= 0 */
#endif
	rv = read(fdes, buf, nbytes);

	/* st devices return 0 when no space left */
	if ((rv == 0 && errno == 0 && Hdr_type != BAR) ||
	    (rv == -1 && errno == EIO)) {
		errno = 0;
		rv = 0;
	}

	return (rv);
}


/*
 * This originally came from libgenIO/g_write.c
 */

/*
 * g_write: Write nbytes of data to fdes (of type devtype) from
 * the location pointed to by buf.  In case of end of medium,
 * translate (where necessary) device specific EOM indications into
 * the generic EOM indication of rv = -1, errno = ENOSPC.
 */

static int
g_write(int devtype, int fdes, char *buf, unsigned nbytes)
{
	int rv;

	if (devtype < 0 || devtype >= G_DEV_MAX) {
		errno = ENODEV;
		return (-1);
	}
#if 0
	if ((rv = write(fdes, buf, nbytes)) <= 0) {
		switch (devtype) {
			case G_FILE: /* do not change returns for files */
				break;
			case G_NO_DEV: /* returns -1 ENOSPC */
			case G_TAPE:
				break;
			case G_3B2_HD:
			case G_3B2_FD:
			case G_3B2_CTC:
				if (rv == -1 && errno == ENXIO)
					errno = ENOSPC;
				break;
			case G_386_HD:	/* not developed yet */
			case G_386_FD:
			case G_386_Q24:
			case G_SCSI_HD:
			case G_SCSI_FD:
			case G_SCSI_9T:
			case G_SCSI_Q24:
			case G_SCSI_Q120:
				break;
			default:
				rv = -1;
				errno = ENODEV;
		} /* devtype */
	} /* (rv = write(fdes, buf, nbytes)) <= 0 */
#endif
	rv = write(fdes, buf, nbytes);

	/* st devices return 0 when no more space left */
	if ((rv == 0 && errno == 0) || (rv == -1 && errno == EIO)) {
		errno = ENOSPC;
		rv = -1;
	}

	return (rv);
}


/*
 * Test for tape
 */

static int
is_tape(int fd)
{
	struct mtget stuff;

	/*
	 * try to do a generic tape ioctl, just to see if
	 * the thing is in fact a tape drive(er).
	 */
	if (ioctl(fd, MTIOCGET, &stuff) != -1) {
		/* the ioctl succeeded, must have been a tape */
		return (1);
	}
	return (0);
}


/*
 * Test for floppy
 */

static int
is_floppy(int fd)
{
	struct fd_char stuff;

	/*
	 * try to get the floppy drive characteristics, just to see if
	 * the thing is in fact a floppy drive(er).
	 */
	if (ioctl(fd, FDIOGCHAR, &stuff) != -1) {
		/* the ioctl succeeded, must have been a floppy */
		return (1);
	}

	return (0);
}


/*
 * New functions for ACLs and other security attributes
 */

/*
 * The function appends the new security attribute info to the end of
 * existing secinfo.
 */
static int
append_secattr(
	char	**secinfo,	/* existing security info */
	int	*secinfo_len,	/* length of existing security info */
	int	size,		/* new attribute size: unit depends on type */
	char	*attrp,		/* new attribute data pointer */
	char	attr_type)	/* new attribute type */
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
			(void) fprintf(stderr, "acltotext failed\n");
			return (-1);
		}
		/* header: type + size = 8 */
	newattrsize = 8 + strlen(attrtext) + 1;
	attr = (struct sec_attr *) malloc((uint) newattrsize);
	if (attr == NULL) {
		(void) fprintf(stderr, "can't allocate memory\n");
		return (-1);
	}
	attr->attr_type = '1';		/* UFSD_ACL */
	(void) sprintf(attr->attr_len,  "%06o", size); /* acl entry count */
	(void) strcpy((char *) &attr->attr_info[0], attrtext);
	free(attrtext);
	break;

		/* SunFed's case goes here */

	default:
	(void) fprintf(stderr, "unrecognized attribute type\n");
	return (-1);
	}

	/* old security info + new attr header(8) + new attr */
	oldsize = *secinfo_len;
	*secinfo_len += newattrsize;
	new_secinfo = (char *) malloc((uint) *secinfo_len);
	if (new_secinfo == NULL) {
		(void) fprintf(stderr, "can't allocate memory\n");
		*secinfo_len -= newattrsize;
		return (-1);
	}

	(void) memcpy(new_secinfo, *secinfo, oldsize);
	(void) memcpy(new_secinfo + oldsize, attr, newattrsize);

	free(*secinfo);
	*secinfo = new_secinfo;
	return (0);
}

static void
write_ancillary(char *secinfo, int len)
{
	long    pad;
	long    cnt;

	/* Just tranditional permissions or no security attribute info */
	if (len == 0)
		return;

	/* write out security info */
	while (len > 0) {
		cnt = (unsigned)(len > CPIOBSZ) ? CPIOBSZ : len;
		FLUSH(cnt);
		errno = 0;
		(void) memcpy(Buffr.b_in_p, secinfo, (unsigned)cnt);
		Buffr.b_in_p += cnt;
		Buffr.b_cnt += (long)cnt;
		len -= (long)cnt;
	}
	pad = (Pad_val + 1 - (cnt & Pad_val)) & Pad_val;
	if (pad != 0) {
		FLUSH(pad);
	(void) memcpy(Buffr.b_in_p, Empty, pad);
	Buffr.b_in_p += pad;
	Buffr.b_cnt += pad;
	}
}

static int
remove_dir(char *path)
{
	DIR		*name;
	struct dirent	*direct;
	struct stat	sbuf;
#define	MSG1	"remove_dir() failed to stat(\"%s\") "
#define	MSG2	"remove_dir() failed to remove_dir(\"%s\") "
#define	MSG3	"remove_dir() failed to unlink(\"%s\") "

	/*
	 * Open the directory for reading.
	 */
	if ((name = opendir(path)) == NULL) {
		msg(ERRN, "remove_dir() failed to opendir(\"%s\") ", path);
		return (-1);
	}

	if (chdir(path) == -1) {
		msg(ERRN, "remove_dir() failed to chdir(\"%s\") ", path);
		return (-1);
	}

	/*
	 * Read every directory entry.
	 */
	while ((direct = readdir(name)) != NULL) {
		/*
		 * Ignore "." and ".." entries.
		 */
		if (strcmp(direct->d_name, ".") == 0 ||
			strcmp(direct->d_name, "..") == 0)
				continue;

			if (stat(direct->d_name, &sbuf) == -1) {
				msg(ERRN, MSG1, direct->d_name);
				(void) closedir(name);
			return (-1);
		}

		if (S_ISDIR(sbuf.st_mode)) {
			if (remove_dir(direct->d_name) == -1) {
				msg(ERRN, MSG2, direct->d_name);
				(void) closedir(name);
				return (-1);
			}
		} else {
			if (unlink(direct->d_name) == -1) {
				msg(ERRN, MSG3, direct->d_name);
				(void) closedir(name);
				return (-1);
			}
		}

	}

	/*
	 * Close the directory we just finished reading.
	 */
	(void) closedir(name);

	/*
	 * change directory to the parent directory
	 */
	if (chdir("..") == -1) {
		msg(ERRN, "remove_dir() failed to chdir(\"..\") ");
		return (-1);
	}

	/*
	 * change directory to the parent directory
	 */
	if (rmdir(path) == -1) {
		msg(ERRN, "remove_dir() failed to rmdir(\"%s\") ", path);
		return (-1);
	}

	return (0);

}

static int
save_cwd(void)
{
	return (open(".", O_RDONLY));
}

static void
rest_cwd(int cwd)
{
	(void) fchdir(cwd);
	(void) close(cwd);
}
