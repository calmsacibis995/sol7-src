/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)ruserpass.c	1.7	97/05/15 SMI"	/* SVr4.0 1.1	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
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
 *	(c) 1986,1987,1988.1989	 Sun Microsystems, Inc
 *	(c) 1983,1984,1985,1986,1987,1988,1989	AT&T.
 *		  All rights reserved.
 *
 */


#include "ftp_var.h"

static	FILE *cfile;

static int rnetrc(char *host, char **aname, char **apass, char **aacct);
static int token(void);

int
ruserpass(char *host, char **aname, char **apass, char **aacct)
{
#if 0
	renv(host, aname, apass, aacct);
	if (*aname == 0 || *apass == 0)
#endif
		return (rnetrc(host, aname, apass, aacct));
}

#define	DEFAULT	1
#define	LOGIN	2
#define	PASSWD	3
#define	ACCOUNT 4
#define	MACDEF	5
#define	ID	10
#define	MACHINE	11

static char tokval[100];

static struct toktab {
	char *tokstr;
	int tval;
} toktab[] = {
	"default",	DEFAULT,
	"login",	LOGIN,
	"password",	PASSWD,
	"account",	ACCOUNT,
	"machine",	MACHINE,
	"macdef",	MACDEF,
	0,		0
};

static int
rnetrc(char *host, char **aname, char **apass, char **aacct)
{
	char *hdir, buf[BUFSIZ], *tmp;
	int t, i, c;
	struct stat stb;
	extern int errno;

	hdir = getenv("HOME");
	if (hdir == NULL)
		hdir = ".";
	(void) sprintf(buf, "%s/.netrc", hdir);
	cfile = fopen(buf, "r");
	if (cfile == NULL) {
		if (errno != ENOENT)
			perror(buf);
		return (0);
	}
next:
	while ((t = token()))
		switch (t) {

	case DEFAULT:
		(void) token();
		continue;

	case MACHINE:
		if (token() != ID || strcmp(host, tokval))
			continue;
		while (((t = token()) != 0) && t != MACHINE)
			switch (t) {

		case LOGIN:
			if (token())
				if (*aname == 0) {
					*aname = malloc((unsigned)
					    strlen(tokval) + 1);
					if (*aname == NULL) {
						fprintf(stderr,
						    "Error - out of VM\n");
						exit(1);
					}
					(void) strcpy(*aname, tokval);
				} else {
					if (strcmp(*aname, tokval))
						goto next;
				}
			break;
		case PASSWD:
			if (fstat(fileno(cfile), &stb) >= 0 &&
			    (stb.st_mode & 077) != 0) {
				fprintf(stderr, "Error - .netrc file not "
				    "correct mode.\n");
				fprintf(stderr, "Remove password or correct "
				    "mode.\n");
				return (-1);
			}
			if (token() && *apass == 0) {
				*apass = malloc((unsigned)strlen(tokval) + 1);
				if (*apass == NULL) {
					fprintf(stderr, "Error - out of VM\n");
					exit(1);
				}
				(void) strcpy(*apass, tokval);
			}
			break;
		case ACCOUNT:
			if (fstat(fileno(cfile), &stb) >= 0 &&
			    (stb.st_mode & 077) != 0) {
				fprintf(stderr, "Error - .netrc file not "
				    "correct mode.\n");
				fprintf(stderr, "Remove account or correct "
				    "mode.\n");
				return (-1);
			}
			if (token() && *aacct == 0) {
				*aacct = malloc((unsigned)strlen(tokval) + 1);
				if (*aacct == NULL) {
					fprintf(stderr, "Error - out of VM\n");
					exit(1);
				}
				(void) strcpy(*aacct, tokval);
			}
			break;
		case MACDEF:
			if (proxy) {
				return (0);
			}
			while ((c = getc(cfile)) != EOF && c == ' ' ||
			    c == '\t');
			if (c == EOF || c == '\n') {
				printf("Missing macdef name argument.\n");
				return (-1);
			}
			if (macnum == 16) {
				printf("Limit of 16 macros have already "
				    "been defined\n");
				return (-1);
			}
			tmp = macros[macnum].mac_name;
			*tmp++ = c;
			for (i = 0; i < 8 && (c = getc(cfile)) != EOF &&
			    !isspace(c); ++i) {
				*tmp++ = c;
			}
			if (c == EOF) {
				printf("Macro definition missing null line "
				    "terminator.\n");
				return (-1);
			}
			*tmp = '\0';
			if (c != '\n') {
				while ((c = getc(cfile)) != EOF && c != '\n');
			}
			if (c == EOF) {
				printf("Macro definition missing null line "
				    "terminator.\n");
				return (-1);
			}
			if (macnum == 0) {
				macros[macnum].mac_start = macbuf;
			} else {
				macros[macnum].mac_start =
				    macros[macnum-1].mac_end + 1;
			}
			tmp = macros[macnum].mac_start;
			while (tmp != macbuf + 4096) {
				if ((c = getc(cfile)) == EOF) {
				printf("Macro definition missing null line "
				    "terminator.\n");
					return (-1);
				}
				*tmp = c;
				if (*tmp == '\n') {
					if (*(tmp-1) == '\0') {
						macros[macnum++].mac_end =
						    tmp - 1;
						break;
					}
					*tmp = '\0';
				}
				tmp++;
			}
			if (tmp == macbuf + 4096) {
				printf("4K macro buffer exceeded\n");
				return (-1);
			}
			break;
		default:
	fprintf(stderr, "Unknown .netrc keyword %s\n", tokval);
			break;
		}
		goto done;
	}
done:
	(void) fclose(cfile);
	return (0);
}

static int
token(void)
{
	char *cp;
	int c;
	struct toktab *t;
	int	len;

	if (feof(cfile))
		return (0);
	while ((c = fgetwc(cfile)) != EOF &&
	    (c == '\n' || c == '\t' || c == ' ' || c == ','))
		continue;
	if (c == EOF)
		return (0);
	cp = tokval;
	if (c == '"') {
		while ((c = fgetwc(cfile)) != EOF && c != '"') {
			if (c == '\\')
				c = fgetwc(cfile);
			if ((len = wctomb(cp, c)) <= 0) {
				len = 1;
				*cp = (unsigned char)c;
			}
			cp += len;
		}
	} else {
		if ((len = wctomb(cp, c)) <= 0) {
			*cp = (unsigned char)c;
			len = 1;
		}
		cp += len;
		while ((c = fgetwc(cfile)) != EOF && c != '\n' && c != '\t' &&
		    c != ' ' && c != ',') {
			if (c == '\\')
				c = fgetwc(cfile);
			if ((len = wctomb(cp, c)) <= 0) {
				len = 1;
				*cp = (unsigned char)c;
			}
			cp += len;
		}
	}
	*cp = 0;
	if (tokval[0] == 0)
		return (0);
	for (t = toktab; t->tokstr; t++)
		if (strcmp(t->tokstr, tokval) == 0)
			return (t->tval);
	return (ID);
}
