/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All Rights reserved.
 */

#pragma ident	"@(#)cuserid.c	1.11	96/12/02 SMI"	/* SVr4.0 1.14	*/

/*LINTLIBRARY*/

#pragma weak cuserid = _cuserid

#include "synonyms.h"
#include <stdio.h>
#include <pwd.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

static char res[L_cuserid];

char *
cuserid(char *s)
{
	struct passwd *pw;
	char utname[L_cuserid];
	char *p;

	if (s == NULL)
		s = res;
	p = getlogin_r(utname, L_cuserid);
	s[L_cuserid - 1] = '\0';
	if (p != NULL)
		return (strncpy(s, p, L_cuserid - 1));
	pw = getpwuid(getuid());
	if (pw != NULL)
		return (strncpy(s, pw->pw_name, L_cuserid - 1));
	*s = '\0';
	return (NULL);
}
