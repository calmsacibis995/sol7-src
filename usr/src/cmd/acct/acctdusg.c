/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)acctdusg.c	1.8	98/01/22 SMI"	/* SVr4.0 1.8	*/
/*
 *	acctdusg [-u file] [-p file] > dtmp-file
 *	-u	file for names of files not charged to anyone
 *	-p	get password info from file
 *	reads std input (normally from find / -print)
 *	and computes disk resource consumption by login
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include "acctdef.h"

struct	disk {
	char	dsk_name[MAXNAME+1];	/* login name */
	uid_t	dsk_uid;		/* user id of login name */
	long	dsk_du;			/* disk usage */
	struct disk *next;		/* next entry at same hash tbl index */
};

char	*pfile = NULL;

struct disk	usglist[MAXUSERS];  /* holds data on disk usg by uid */

FILE	*pwf, *nchrg = NULL;
FILE	*names = {stdin};
FILE	*acctf = {stdout};

char	*calloc(), *strncpy(), *strcpy();

main(argc, argv)
char	**argv;
int 	argc;
{
	char	fbuf[BUFSIZ], *fb, *strchr();
	struct passwd	*(*getpw)();
	void	(*endpw)();
	unsigned int idx;
	void	openerr(), output(), makdlst();
	void	charge(), end_pwent(), endpwent(), setpwent(), freelist();
	int	stpwent();
	struct passwd	*getpwent(), *fgetpwent(), *pw;
	struct disk	*init_entry();
	struct disk	*hash();
	struct disk	*hash_find();
#ifdef DEBUG
	void	pdisk();
#endif

	while (--argc > 0) {
		++argv;
		if (**argv == '-')
		switch ((*argv)[1]) {
		case 'u':
			if (--argc <= 0)
				break;
			if ((nchrg = fopen(*(++argv), "w")) == NULL)
				openerr(*argv);
			chmod(*argv, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
			continue;
		case 'p':
			if (--argc <= 0)
				break;
			pfile = *(++argv);
			continue;
		}
		fprintf(stderr, "Invalid argument: %s\n", *argv);
		exit(1);
	}

	if (pfile) {
		if (!stpwent(pfile)) {
			openerr(pfile);
		}
		getpw = fgetpwent;
		endpw = end_pwent;
	} else {
		setpwent();
		getpw = getpwent;
		endpw = endpwent;
	}

	if (pfile)
		rewind(pwf);
	else
		setpwent();

	/* initialize hash table entries */
	for (idx = 0; idx < MAXUSERS; idx++)
		if (init_entry(&usglist[idx]) == NULL) {
			fprintf(stderr, "acctdusg:  hash table at index %d "
			    "unexpectedly NULL", idx);
			exit(1);
		}

	while ((pw = getpw(pwf)) != NULL) {
			/* fill usglist with the user's in the passwd file */
		makdlst(pw);
	}
	endpw();

	/* charge the files listed in names to users listed in the usglist */

	while (fgets(fbuf, sizeof (fbuf), names) != NULL) {
		if (fb = strchr(fbuf, '\n')) {  /* replace the newline char */
			*fb = '\0';		/* at the end of the filename */
		}				/* with a null character */
		charge(fbuf);
	}

	output();

	if (nchrg)
		fclose(nchrg);
#ifdef DEBUG
		pdisk();
#endif

	freelist();
	exit(0);
}

struct disk *
init_entry(struct disk *entry)
{
	if (entry != NULL) {
		entry->dsk_name[0] = '\0';
		entry->dsk_uid = UNUSED;
		entry->dsk_du = 0;
		entry->next = NULL;
		return (entry);
	}
	return (NULL);
}

/*
 * hash -
 * given a uid, return either an existing entry for that uid
 * in the hash table or an empty entry to use for this uid,
 * possibly allocating space for a new entry.
 */
struct disk *
hash(j)
uid_t j;
{
	struct disk *curdisk, *lastdisk;

	curdisk = &usglist[(unsigned int)j % MAXUSERS];

	do {
		if (curdisk->dsk_uid == j || curdisk->dsk_uid == UNUSED)
			return (curdisk);
		lastdisk = curdisk;
		curdisk = curdisk->next;
	} while (curdisk != NULL);
	if ((lastdisk->next = (struct disk *)malloc(sizeof (struct disk)))
	    == NULL) {
		fprintf(stderr, "acctdusg:  cannot allocate memory for hash "
		    "table entry\n");
		exit(1);
	}
	(void) init_entry(lastdisk->next);
	return (lastdisk->next);
}

/*
 * hash_find -
 * Return hash entry matching the given uid.
 * Return NULL if hash entry not found.
 */
struct disk *
hash_find(uid_t j)
{
	struct disk *curdisk;

	curdisk = &usglist[(unsigned int)j % MAXUSERS];

	while (curdisk != NULL) {
		if (curdisk->dsk_uid == j)
			return (curdisk);
		if (curdisk->dsk_uid == UNUSED)
			return (NULL);
		curdisk = curdisk->next;
	}
	return (NULL);
}

void
openerr(file)
char	*file;
{
	fprintf(stderr, "Cannot open %s\n", file);
	exit(1);
}

void
output()
{
	int	index;
	struct disk *entry;

	for (index = 0; index < MAXUSERS; index++) {
		entry = &usglist[index];
		while (entry != NULL) {
			if (entry->dsk_uid != UNUSED && entry->dsk_du != 0)
				printf("%ld	%s	%ld\n",
				    entry->dsk_uid,
				    entry->dsk_name,
				    entry->dsk_du);
			entry = entry->next;
		}
	}
}


/*
 *	make a list of all the
 *	users in password file
 */

void
makdlst(p)
register struct passwd	*p;
{

	int    i, index;
	struct disk *hash_entry;

	hash_entry = hash(p->pw_uid);
	if (hash_entry->dsk_uid == UNUSED) {
		hash_entry->dsk_uid = p->pw_uid;
		strncpy(hash_entry->dsk_name, p->pw_name, MAXNAME);
	}

}


void
charge(n)
register char *n;
{
	struct stat	statb;
	struct disk    *entry;

	if (lstat(n, &statb) == -1)
		return;

	if (((entry = hash_find(statb.st_uid)) != NULL) &&
	    (entry->dsk_uid == statb.st_uid)) {
		entry->dsk_du += statb.st_blocks;
	} else if (nchrg) {
			fprintf(nchrg, "%9ld\t%7lu\t%s\n",
			statb.st_uid, statb.st_blocks, n);
	}
}

#ifdef DEBUG
void
pdisk()
{
	int	index;
	struct disk *entry;

	for (index = 0; index < MAXUSERS; index++) {
		entry = &usglist[index];
		while (entry != NULL) {
			fprintf(stderr,  "%.8s\t%9ld\t%7lu\n",
			    entry->dsk_name,
			    entry->dsk_uid,
			    entry->dsk_du);
			entry = entry->next;
		}
	}

}
#endif

stpwent(pfile)
register char *pfile;
{
	if (pwf == NULL)
		pwf = fopen(pfile, "r");
	else
		rewind(pwf);
	return (pwf != NULL);
}

void
end_pwent()
{
	if (pwf != NULL) {
		(void) fclose(pwf);
		pwf = NULL;
	}
}

/*
 * freelist -
 * free malloc'd disk structs from linked lists on hash
 * table usglist.
 */
void
freelist()
{
	int	index;
	struct disk *entry, *next;

	for (index = 0; index < MAXUSERS; index++) {
		entry = usglist[index].next;
		while (entry != NULL) {
			next = entry->next;
			free(entry);
			entry = next;
		}
	}

}
