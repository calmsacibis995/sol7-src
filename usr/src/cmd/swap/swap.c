/*
 * Copyright (c) 1996-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

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
 *	(c) 1986, 1987, 1988, 1989  Sun Microsystems, Inc
 *	(c) 1983, 1984, 1985, 1986, 1987, 1988, 1989  AT&T.
 *	All rights reserved.
 */

/*
 * 	Swap administrative interface
 *	Used to add/delete/list swap devices.
 */

#pragma ident	"@(#)swap.c	%I	97/04/22 SMI"

#include	<sys/types.h>
#include	<stdio.h>
#include	<stdlib.h>
#include	<unistd.h>
#include	<errno.h>
#include	<sys/param.h>
#include	<dirent.h>
#include	<sys/swap.h>
#include	<sys/sysmacros.h>
#include	<sys/mkdev.h>
#include	<sys/stat.h>
#include	<sys/statvfs.h>
#include	<sys/uadmin.h>
#include	<vm/anon.h>
/* include	<kvm.h> */
#include	<fcntl.h>

#define	LFLAG 1
#define	DFLAG 2
#define	AFLAG 3
#define	SFLAG 4

static char *prognamep;

static int add(char *path, u_offset_t offset, u_offset_t cnt);
static int delete(char *path, u_offset_t offset);
static void usage(void);
static int doswap(void);
static int valid(char *pathname, u_offset_t offset, u_offset_t length);
static int list(void);

int
main(int argc, char **argv)
{
	int c, flag = 0;
	int ret;
	u_offset_t s_offset = 0;
	u_offset_t length = 0;
	char *pathname;

	prognamep = argv[0];
	if (argc < 2) {
		usage();
		exit(1);
	}

	while ((c = getopt(argc, argv, "lsd:a:")) != EOF) {
		char *char_p;
		switch (c) {
		case 'l': 	/* list all the swap devices */
			if (argc != 2 || flag) {
				usage();
				exit(1);
			}
			flag |= LFLAG;
			ret = list();
			break;
		case 's':
			if (argc != 2 || flag) {
				usage();
				exit(1);
			}
			flag |= SFLAG;
			ret = doswap();
			break;
		case 'd':
			/*
			 * The argument for starting offset is optional.
			 * If no argument is specified, the entire swap file
			 * is added although this will fail if a non-zero
			 * starting offset was specified when added.
			 */
			if ((argc != 3 && argc != 4) || flag) {
				usage();
				exit(1);
			}
			flag |= DFLAG;
			pathname = optarg;
			if (argc == 4) {
				errno = 0;
				s_offset = strtoull(argv[optind], &char_p, 10);
				if (errno != 0 || *char_p != '\0') {
					(void) fprintf(stderr,
					    "error in [low block]\n");
					exit(1);
				}
			}
			ret = delete(pathname, s_offset);
			break;

		case 'a':
			/*
			 * The arguments for starting offset and number of
			 * blocks are optional.  If only the starting offset
			 * is specified, all the blocks to the end of the swap
			 * file will be added.  If no starting offset is
			 * specified, the entire swap file is assumed.
			 */
			if (argc < 3 || argc > 5 || flag) {
				usage();
				exit(1);
			}
			if (*optarg != '/') {
				(void) fprintf(stderr,
				    "%s: path must be absolute\n", prognamep);
				exit(1);
			}
			flag |= AFLAG;
			pathname = optarg;
			if (argc >= 4) {
				errno = 0;
				s_offset = strtoull(argv[optind], &char_p, 10);
				if (errno != 0 || *char_p != '\0') {
					(void) fprintf(stderr,
					    "error in [low block]\n");
					exit(1);
				}
			}
			if (argc == 5) {
				errno = 0;
				length = strtoull(argv[++optind], &char_p, 10);
				if (errno != 0 || *char_p != '\0') {
					(void) fprintf(stderr,
					    "error in [nbr of blocks]\n");
					exit(1);
				}
			}
			if ((ret = valid(pathname, s_offset*512, length*512)) ==
			    0)
				ret = add(pathname, s_offset, length);
			break;
		case '?':
			usage();
			exit(1);
		}
	}
	if (!flag) {
		usage();
		exit(1);
	}
	return (ret);
}


static void
usage(void)
{
	(void) fprintf(stderr, "Usage:\t%s -l\n", prognamep);
	(void) fprintf(stderr, "\t%s -s\n", prognamep);
	(void) fprintf(stderr, "\t%s -d <file name> [low block]\n", prognamep);
	(void) fprintf(stderr, "\t%s -a <file name> [low block]"
	    " [nbr of blocks]\n", prognamep);
}

/*
 * Implement:
 *	#define ctok(x) ((ctob(x))>>10)
 * in a machine independent way. (Both assume a click > 1k)
 */
static u_int
ctok(u_int clicks)
{
	static int factor = -1;

	if (factor == -1)
		factor = (int) (sysconf(_SC_PAGESIZE) >> 10);
	return (clicks*factor);
}


static int
doswap(void)
{
	struct anoninfo ai;
	u_int allocated, reserved, available;

	/*
	 * max = total amount of swap space including physical memory
	 * ai.ani_max = MAX(anoninfo.ani_resv, anoninfo.ani_max) +
		availrmem - swapfs_minfree;
	 * ai.ani_free = amount of unallocated anonymous memory
		(ie. = resverved_unallocated + unreserved)
	 * ai.ani_free = anoninfo.ani_free + (availrmem - swapfs_minfree);
	 * ai.ani_resv = total amount of reserved anonymous memory
	 * ai.ani_resv = anoninfo.ani_resv;
	 *
	 * allocated = anon memory not free
	 * reserved = anon memory reserved but not allocated
	 * available = anon memory not reserved
	 */
	if (swapctl(SC_AINFO, &ai) == -1) {
		perror(prognamep);
		return (2);
	}

	allocated = ai.ani_max - ai.ani_free;
	reserved = ai.ani_resv - allocated;
	available = ai.ani_max - ai.ani_resv;

	(void) printf("total: %uk bytes allocated + %uk reserved = %uk used,"
	    " %uk available\n",
	    ctok(allocated), ctok(reserved), ctok(reserved) + ctok(allocated),
	    ctok(available));

	return (0);
}

static int
list(void)
{
	struct swaptable 	*st;
	struct swapent	*swapent;
	int	i;
	struct stat64 statbuf;
	char		*path;
	char		fullpath[MAXPATHLEN+1];
	int		num;

	if ((num = swapctl(SC_GETNSWP, NULL)) == -1) {
		perror(prognamep);
		return (2);
	}
	if (num == 0) {
		(void) fprintf(stderr, "No swap devices configured\n");
		return (1);
	}

	if ((st = malloc(num * sizeof (swapent_t) + sizeof (int)))
	    == NULL) {
		(void) fprintf(stderr, "Malloc failed. Please try later.\n");
		perror(prognamep);
		return (2);
	}
	if ((path = malloc(num * MAXPATHLEN)) == NULL) {
		(void) fprintf(stderr, "Malloc failed. Please try later.\n");
		perror(prognamep);
		return (2);
	}
	swapent = st->swt_ent;
	for (i = 0; i < num; i++, swapent++) {
		swapent->ste_path = path;
		path += MAXPATHLEN;
	}

	st->swt_n = num;
	if ((num = swapctl(SC_LIST, st)) == -1) {
		perror(prognamep);
		return (2);
	}

	(void) printf("swapfile             dev  swaplo blocks   free\n");

	swapent = st->swt_ent;
	for (i = 0; i < num; i++, swapent++) {
		if (*swapent->ste_path != '/')
			(void) sprintf(fullpath, "/dev/%s", swapent->ste_path);
		else
			(void) sprintf(fullpath, "%s", swapent->ste_path);

		if (stat64(fullpath, &statbuf) < 0)
			if (*swapent->ste_path != '/')
				(void) printf("%-20s  -  ", swapent->ste_path);
			else
				(void) printf("%-20s ?,? ", fullpath);
		else {
			if (statbuf.st_mode & (S_IFBLK | S_IFCHR))
				(void) printf("%-19s %2lu,%-2lu", fullpath,
				    major(statbuf.st_rdev),
				    minor(statbuf.st_rdev));
			else
				(void) printf("%-20s  -  ", fullpath);
		}
		{
		int diskblks_per_page =
			(int) (sysconf(_SC_PAGESIZE) >> DEV_BSHIFT);
		(void) printf(" %6lu %6lu %6lu", swapent->ste_start,
		    swapent->ste_pages * diskblks_per_page,
		    swapent->ste_free * diskblks_per_page);
		}
		if (swapent->ste_flags & ST_INDEL)
			(void) printf(" INDEL\n");
		else
			(void) printf("\n");
	}
	return (0);
}

static int
delete(char *path, u_offset_t offset)
{
	swapres_t	*si;
	swapres_t	swpi;

	si = &swpi;
	si->sr_name = path;
	/* before kernel fully support >2G swap devices, we type-cast */
	si->sr_start = (off_t) offset;

	if (swapctl(SC_REMOVE, si) < 0) {
		switch (errno) {
		case (ENOSYS):
			(void) fprintf(stderr,
			    "%s: Invalid operation for this filesystem type\n",
			    path);
			break;
		default:
			perror(path);
			break;
		}
		return (2);
	}
	return (0);
}

/*
 * swapres_t structure units are in 512-blocks
 */
static int
add(char *path, u_offset_t offset, u_offset_t cnt)
{
	swapres_t	*si;
	swapres_t	swpi;

	si = &swpi;
	si->sr_name = path;
	/* before kernel fully support >2G swap devices, we type-cast */
	si->sr_start = (off_t) offset;
	si->sr_length = (off_t) cnt;

	if (swapctl(SC_ADD, si) < 0) {
		switch (errno) {
		case (ENOSYS):
			(void) fprintf(stderr,
			    "%s: Invalid operation for this filesystem type\n",
			    path);
			break;
		case (EEXIST):
			(void) fprintf(stderr,
			    "%s: Overlapping swap files are not allowed\n",
			    path);
			break;
		default:
			perror(path);
			break;
		}
		return (2);
	}
	return (0);
}

static int
valid(char *pathname, u_offset_t offset, u_offset_t length)
{
	struct stat64		f;
	struct statvfs64	fs;
	u_offset_t		need;

	if (stat64(pathname, &f) < 0 || statvfs64(pathname,  &fs) < 0) {
		return (errno);
	}

	if (!((S_ISREG(f.st_mode) && (f.st_mode & S_ISVTX) == S_ISVTX) ||
		S_ISBLK(f.st_mode))) {
		(void) fprintf(stderr, "\"%s\" is not valid for swapping.\n"
		    "It must be a block device or a regular file with the\n"
		    "\"save user text on execution\" bit set.\n",
		    pathname);
		return (EINVAL);
	}

	if (S_ISREG(f.st_mode)) {
		if (length == 0)
			length = (u_offset_t) f.st_size;

		/*
		 * "f.st_blocks < 8" because the first eight
		 * 512-byte sectors are always skipped
		 */

		if (f.st_size < (length-offset) || f.st_size == 0 ||
		    f.st_size > MAXOFF_T || f.st_blocks < 8) {
			(void) fprintf(stderr, "%s: size is invalid\n",
			    pathname);
			return (EINVAL);
		}

		need = roundup(length, fs.f_bsize) / DEV_BSIZE;

		/*
		 * "need > f.st_blocks" to account for indirect blocks
		 * Note:
		 *  This can be fooled by a file large enough to
		 *  contain indirect blocks that also contains holes.
		 *  However, we don't know (and don't want to know)
		 *  about the underlying storage implementation.
		 *  But, if it doesn't have at least this many blocks,
		 *  there must be a hole.
		 */

		if (need > f.st_blocks) {
			(void) fprintf(stderr,
			"\"%s\" may contain holes - can't swap on it.\n",
			    pathname);
			return (EINVAL);
		}
	}
	/*
	 * else, we cannot get st_size for S_ISBLK device and
	 * no meaningful checking can be done.
	 */

	return (0);
}
