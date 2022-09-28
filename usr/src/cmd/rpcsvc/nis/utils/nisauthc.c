/*
 * nisauthconf.c
 *
 * Configure NIS+ to use RPCSEC_GSS
 *
 *	Copyright (c) 1997 Sun Microsystems, Inc.
 *	All Rights Reserved.
 *
 */
#pragma ident	"@(#)nisauthconf.c 1.1     97/11/19 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <libgen.h>
#include <rpcsvc/nis_dhext.h>

static struct mech_t {
	char	*mechname;
	char	*keylen;
	char	*algtype;
	char	*alias;
	char	*additional;
} mechs[] = {
	"diffie_hellman_1024_0", "1024", "0", "dh1024-0", "default integrity",
	"diffie_hellman_640_0", "640", "0", "dh640-0", "default integrity",
	"-", "-", "-", "des", "# AUTH_DES",
	NULL, NULL, NULL, NULL, NULL
};

static void
preamble(FILE *f)
{
	(void) fprintf(f, "# DO NOT EDIT FILE, AUTOMATICALLY GENERATED\n");
	(void) fprintf(f, "# \n");
	(void) fprintf(f,
		"# The format of this file may change or it may be removed\n");
	(void) fprintf(f, "# in future versions of Solaris.\n# \n");
}

static void
printconf()
{
	int		i = 0;
	mechanism_t	**mechlist;

	if (mechlist = __nis_get_mechanisms(FALSE)) {
		while (mechlist[i]) {
			(void) printf("%s", mechlist[i]->alias);

			if (mechlist[++i])
				(void) printf(", ");
			else
				(void) printf("\n");
		}
	} else
		(void) printf("des\n");
	exit(0);
}


static void
usage(char *cmd)
{
	(void) fprintf(stderr, "usage:\n\t%s [-v] [mechanism, ...]\n", cmd);
	exit(1);
}


void
main(int argc, char **argv)
{
	int c, i, doprintconf = 0;
	char *cmd = basename(argv[0]);
	FILE *f;

	while ((c = getopt(argc, argv, "v")) != -1) {
		switch (c) {
		case 'v':
			doprintconf++;
			break;
		default:
			usage(cmd);
		}
	}

	if (doprintconf)
		printconf();

	if (argc < 2)
		usage(cmd);

	if (argc == 2 && (strcmp(argv[1], NIS_SEC_CF_DES_ALIAS) == 0)) {
		(void) unlink(NIS_SEC_CF_PATHNAME);
		exit(0);
	}

	if (!(f = fopen(NIS_SEC_CF_PATHNAME, "w"))) {
		(void) fprintf(stderr,
				"Could not open %s for writing.\n",
				NIS_SEC_CF_PATHNAME);
		exit(1);
	}

	preamble(f);

	for (i = 1; i < argc; i++) {
		int j = 0;
		int gotit = 0;

		while (mechs[j].alias) {
			if (!(strcmp(argv[i], mechs[j].alias))) {
				gotit++;

				(void) fprintf(f, "mech\t%s\t%s\t%s\t%s\t%s\n",
							mechs[j].mechname,
							mechs[j].keylen,
							mechs[j].algtype,
							mechs[j].alias,
							mechs[j].additional);
				(void) fflush(f);
				break;
			}
			j++;
		}

		if (!gotit) {
			(void) fprintf(stderr,
					"%s: Mechanism, %s, not found!\n", cmd,
					argv[i]);
			(void) fflush(f);
			(void) fclose(f);
			exit(1);
		}
	}
	(void) fclose(f);
}
