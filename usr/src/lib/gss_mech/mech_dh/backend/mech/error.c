/*
 *	error.c
 *
 *	Copyright (c) 1997, by Sun Microsystems, Inc.
 *	All rights reserved.
 *
 */

#pragma ident	"@(#)error.c	1.1	97/11/19 SMI"

#include "error.h"

#define	WHO "Diffie-Hellman mechanism: "

static char * error_tab[] = {
	"",
	WHO "No memory",
	WHO "Could not encode token",
	WHO "Could not decode token",
	WHO "Bad Argument",
	WHO "Cipher failure",
	WHO "Verifier failure",
	WHO "Session cipher failure",
	WHO "No secret key",
	WHO "No principal",
	WHO "Not local principal",
	WHO "Unkown QOP",
	WHO "Verifier mismatch",
	WHO "No such user",
	WHO "Could not generate netname"
};

#define	NO_ENTRIES (sizeof (error_tab) / sizeof (char *))

char *
__dh_error_msg(int error)
{
	if (ENUM(error) >= NO_ENTRIES)
		return (WHO "Unknown or Invalid error");

	return (error_tab[ENUM(error)]);
}
