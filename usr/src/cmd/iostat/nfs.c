/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)nfs.c	1.1	97/04/21 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <kstat.h>
#include <sys/mnttab.h>
#include <sys/mntent.h>
#include <sys/sysmacros.h>
#include <sys/mkdev.h>
#include <sys/vfs.h>
#include <nfs/nfs.h>
#include <nfs/nfs_clnt.h>


extern void init_nfs(void);
extern char *lookup_nfs_name(char *);
extern kstat_ctl_t *kc;

static char *get_nfs_by_minor(int);
static char *cur_hostname(int);
static char *cur_special(char *, char *);

char *
lookup_nfs_name(char *ks)
{
	int minor;
	char *host, *path;
	char *cp;

	if (sscanf(ks, "nfs%d", &minor) != 1)
		return (NULL);

	cp = get_nfs_by_minor(minor);
	if (cp == NULL)
		return (NULL);

	if (strchr(cp, ',') == NULL) {
		return (strdup(cp));
	}

	host = cur_hostname(minor);
	if (host == NULL) {
		return (strdup(cp));
	}

	path = cur_special(host, cp);
	if (path == NULL) {
		return (strdup(cp));
	}

	cp = malloc(strlen(host) + strlen(path) + 2);
	if (cp == NULL)
		return (NULL);

	(void) sprintf(cp, "%s:%s", host, path);

	return (cp);
}

static char *
get_nfs_by_minor(int minor)
{
	struct mnttab mnt;
	FILE *fp;
	struct mnttab mpref;
	char *cp, *str, *ret = NULL;
	int found;

	(void) memset(&mnt, '\0', sizeof (struct mnttab));
	(void) memset(&mpref, '\0', sizeof (struct mnttab));

	if ((fp = fopen(MNTTAB, "r")) == NULL) {
		(void) fprintf(stderr, "Unable to open %s: %s\n",
						MNTTAB, strerror(errno));
		return (NULL);
	}

	mpref.mnt_fstype = MNTTYPE_NFS;

	while ((found = getmntany(fp, &mnt, &mpref)) != -1) {
		/*
		 * Skip over malformed entries
		 */
		if (found != 0)
			continue;

		/*
		 * Check for dev=
		 */
		if ((str = hasmntopt(&mnt, MNTOPT_DEV)) == NULL)
			continue;
		if ((cp = strchr(str, '=')) == NULL)
			continue;

		if ((strtoul(cp + 1, (char **)NULL, 16) & MAXMIN) == minor) {
			ret = mnt.mnt_special;
			break;
		}
	}

	(void) fclose(fp);

	return (ret);
}

/*
 * Read the cur_hostname from the mntinfo kstat
 */
static char *
cur_hostname(int minor)
{
	kstat_t *ksp;
	static struct mntinfo_kstat mik;

	for (ksp = kc->kc_chain; ksp; ksp = ksp->ks_next) {
		if (ksp->ks_type != KSTAT_TYPE_RAW)
			continue;
		if (ksp->ks_instance != minor)
			continue;
		if (strcmp(ksp->ks_module, "nfs") != 0)
			continue;
		if (strcmp(ksp->ks_name, "mntinfo") != 0)
			continue;
		if (ksp->ks_flags & KSTAT_FLAG_INVALID)
			return (NULL);
		if (kstat_read(kc, ksp, &mik) == -1)
			return (NULL);
		return (mik.mik_curserver);
	}
	return (NULL);
}

/*
 * Given the hostname of the mounted server, extract the server
 * mount point from the mnttab string.
 *
 * Common forms:
 *	server1,server2,server3:/path
 *	server1:/path,server2:/path
 * or a hybrid of the two
 */
static char *
cur_special(char *hostname, char *special)
{
	char *cp;
	char *path;
	int hlen = strlen(hostname);

	/*
	 * find hostname in string
	 */
again:
	if ((cp = strstr(special, hostname)) == NULL)
		return (NULL);

	/*
	 * hostname must be followed by ',' or ':'
	 */
	if (cp[hlen] != ',' && cp[hlen] != ':') {
		special = &cp[hlen];
		goto again;
	}

	/*
	 * If hostname is followed by a ',' eat all characters until a ':'
	 */
	cp = &cp[hlen];
	if (*cp == ',') {
		cp++;
		while (*cp != ':') {
			if (*cp == '\0')
				return (NULL);
			cp++;
		}
	}
	path = ++cp;			/* skip ':' */

	/*
	 * path is terminated by either 0, or space or ','
	 */
	while (*cp != '\0') {
		if (isspace(*cp) || *cp == ',') {
			*cp = '\0';
			return (path);
		}
		cp++;
	}
	return (path);
}
