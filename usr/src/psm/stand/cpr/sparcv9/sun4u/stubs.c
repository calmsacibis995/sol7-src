/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)stubs.c	1.10	97/12/17 SMI"

#include <sys/promif.h>

#define	errp	prom_printf

size_t strlen(register const char *);
char *strcat(register char *, register const char *);
char *strcpy(register char *, register const char *);

/*
 * Ask prom to open a disk file given either the OBP device path, or the
 * device path representing the target drive/partition and the fs-relative
 * path of the file.  Handle file pathnames with or without leading '/'.
 * if fs points to a null char (sorry, but it always points to a static
 * buffer, and this is much less change to other code than passing in NULL)
 * it indicates that we are opening a character special device, not a UFS
 * file
 */
/* ARGSUSED */
int
cpr_statefile_open(char *path, char *fs)
{
	char full_path[OBP_MAXPATHLEN];
	char *fp = full_path;
	int handle;
	int c;

	/*
	 * Because we have two booters who don't share an address space,
	 * instead of using specialstate, which only cprbooter knows,
	 * we use fs as the flag
	 */
	if (*fs == '\0') {	/* char statefile open */
		handle = prom_open(path);
		/* prom_open for IEEE 1275 returns 0 on failure; we return -1 */
		return (handle ? handle : -1);
	}

	/*
	 * IEEE 1275 prom needs "device-path,|file-path" where
	 * file-path can have embedded |'s.
	 */
	(void) strcpy(fp, fs);
	fp += strlen(fp);
	*fp++ = ',';
	*fp++ = '|';

	/* Skip a leading slash in file path -- we provided for it above. */
	if (*path == '/')
		path++;

	/* Copy file path and convert separators. */
	while ((c = *path++) != '\0')
		if (c == '/')
			*fp++ = '|';
		else
			*fp++ = c;
	*fp = '\0';

	/* prom_open for IEEE 1275 returns 0 on failure; we return -1 */
	return ((handle = prom_open(full_path)) ? handle : -1);
}

/*
 * Ask prom to open a disk file given the device path representing
 * the target drive/partition and the fs-relative path of the
 * file.  Handle file pathnames with or without leading '/'.
 * if fs points to a null char (sorry, but it always points to a static
 * buffer, and this is much less change to other code than passing in NULL)
 * it indicates that we are opening a character special device, not a UFS
 * file
 */
/* ARGSUSED */
int
cpr_ufs_open(char *path, char *fs)
{
	/*
	 * screen invalid state, then just use the other code rather than
	 * duplicating it
	 */
	if (*fs == '\0') {	/* char statefile open */
		errp("cpr_ufs_open: NULL fs, path %s\n", path);
		return (-1);
	}
	return (cpr_statefile_open(path, fs));
}

/*
 * On sun4u there's no difference here, since prom groks ufs directly
 */
int
cpr_statefile_read(int fd, caddr_t buf, int len)
{
	return (prom_read(fd, buf, len, 0, 0));
}

int
cpr_ufs_read(int fd, caddr_t buf, int len)
{
	return (prom_read(fd, buf, len, 0, 0));
}

int
cpr_ufs_close(int fd)
{
	return (prom_close(fd));
}

int
cpr_statefile_close(int fd)
{
	return (prom_close(fd));
}

void
cprboot_spinning_bar()
{
	static int spinix;

	switch (spinix) {
	case 0:
		prom_printf("|\b");
		break;
	case 1:
		prom_printf("/\b");
		break;
	case 2:
		prom_printf("-\b");
		break;
	case 3:
		prom_printf("\\\b");
		break;
	}
	if ((++spinix) & 0x4) spinix = 0;
}
