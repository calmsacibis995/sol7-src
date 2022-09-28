/*
 * Copyright (c) 1994-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)pldd.c	1.5	98/01/30 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>
#include <link.h>
#include <libproc.h>
#include <proc_service.h>

static	int	show_map(void *, const prmap_t *, const char *);
static	void	uncontrol(char *, size_t);

static	char	*command;

main(int argc, char **argv)
{
	int rc = 0;
	int opt;
	int errflg = 0;
	int Fflag = 0;

	if ((command = strrchr(argv[0], '/')) != NULL)
		command++;
	else
		command = argv[0];

	/* options */
	while ((opt = getopt(argc, argv, "F")) != EOF) {
		switch (opt) {
		case 'F':		/* force grabbing (no O_EXCL) */
			Fflag = PGRAB_FORCE;
			break;
		default:
			errflg = 1;
			break;
		}
	}

	argc -= optind;
	argv += optind;

	if (errflg || argc <= 0) {
		(void) fprintf(stderr,
			"usage:\t%s [-F] pid ...\n", command);
		(void) fprintf(stderr,
			"  (report process dynamic libraries)\n");
		(void) fprintf(stderr,
			"  -F: force grabbing of the target process\n");
		return (2);
	}

	while (argc-- > 0) {
		char *arg;
		pid_t pid;
		int gcode;
		psinfo_t psinfo;
		struct ps_prochandle *Pr;

		/* get the specified pid and its psinfo struct */
		if ((pid = proc_pidarg(arg = *argv++, &psinfo)) < 0) {
			(void) fprintf(stderr, "%s: no such process: %s\n",
				command, arg);
			rc++;
			continue;
		}
		if ((Pr = Pgrab(pid, PGRAB_RETAIN|Fflag, &gcode))
		    == NULL) {
			(void) fprintf(stderr, "%s: cannot grab %d: %s\n",
				command, (int)pid, Pgrab_error(gcode));
			rc++;
			continue;
		}
		uncontrol(psinfo.pr_psargs, PRARGSZ);
		(void) printf("%d:\t%.70s\n", (int)pid, psinfo.pr_psargs);

		rc += proc_object_iter(Pr, show_map, Pr);

		Prelease(Pr, 0);
	}

	return (rc);
}

static int
show_map(void *cd, const prmap_t *pmap, const char *object_name)
{
	char pathname[PATH_MAX];
	struct ps_prochandle *Pr = cd;
	const auxv_t *auxv;
	int len;

	/* omit the executable file */
	if (strcmp(pmap->pr_mapname, "a.out") == 0)
		return (0);

	/* also omit the dynamic linker */
	if (ps_pauxv(Pr, &auxv) == PS_OK) {
		while (auxv->a_type != AT_NULL) {
			if (auxv->a_type == AT_BASE) {
				if (pmap->pr_vaddr == auxv->a_un.a_val)
					return (0);
				break;
			}
			auxv++;
		}
	}

	/* freedom from symlinks; canonical form */
	if ((len = resolvepath(object_name, pathname, sizeof (pathname))) > 0)
		pathname[len] = '\0';
	else
		(void) strncpy(pathname, object_name, sizeof (pathname));

	(void) printf("%s\n", pathname);
	return (0);
}

/*
 * Convert string into itself, replacing unprintable
 * characters with space along the way.  Stop on a null character.
 */
static void
uncontrol(char *buf, size_t n)
{
	int c;

	while (n-- != 0 && (c = (*buf & 0xff)) != '\0') {
		if (!isprint(c))
			c = ' ';
		*buf++ = (char)c;
	}

	*buf = '\0';
}
