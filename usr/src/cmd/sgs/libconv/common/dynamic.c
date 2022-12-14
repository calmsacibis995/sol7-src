/*
 *	Copyright (c) 1996 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)dynamic.c	1.18	97/07/15 SMI"

/* LINTLIBRARY */

/*
 * String conversion routine for .dynamic tag entries.
 */
#include	<stdio.h>
#include	<string.h>
#include	"_conv.h"
#include	"dynamic_msg.h"


const char *
conv_dynposflag_1_str(Word flags)
{
	static char	string[STRSIZE] = { '\0' };
	if (flags == 0)
		return (MSG_ORIG(MSG_GBL_ZERO));
	(void) strcpy(string, MSG_ORIG(MSG_GBL_OSQBRKT));
	if (flags & DF_P1_LAZYLOAD)
		(void) strcat(string, MSG_ORIG(MSG_DFP_LAZYLOAD));
	if (flags & DF_P1_GROUPPERM)
		(void) strcat(string, MSG_ORIG(MSG_DFP_GROUPPERM));
	(void) strcat(string, MSG_ORIG(MSG_GBL_CSQBRKT));
	return ((const char *)string);
}

const char *
conv_dynflag_1_str(Word flags)
{
	static char	string[STRSIZE] = { '\0' };

	if (flags == 0)
		return (MSG_ORIG(MSG_GBL_ZERO));
	else {
		(void) strcpy(string, MSG_ORIG(MSG_GBL_OSQBRKT));
		if (flags & DF_1_NOW)
			(void) strcat(string, MSG_ORIG(MSG_DF_NOW));
		if (flags & DF_1_GROUP)
			(void) strcat(string, MSG_ORIG(MSG_DF_GROUP));
		if (flags & DF_1_NODELETE)
			(void) strcat(string, MSG_ORIG(MSG_DF_NODELETE));
		if (flags & DF_1_LOADFLTR)
			(void) strcat(string, MSG_ORIG(MSG_DF_LOADFLTR));
		if (flags & DF_1_INITFIRST)
			(void) strcat(string, MSG_ORIG(MSG_DF_INITFIRST));
		if (flags & DF_1_NOOPEN)
			(void) strcat(string, MSG_ORIG(MSG_DF_NOOPEN));
		if (flags & DF_1_ORIGIN)
			(void) strcat(string, MSG_ORIG(MSG_DF_ORIGIN));
		if (flags & DF_1_DIRECT)
			(void) strcat(string, MSG_ORIG(MSG_DF_DIRECT));
		if (flags & DF_1_TRANS)
			(void) strcat(string, MSG_ORIG(MSG_DF_TRANS));
		if (flags & DF_1_INTERPOSE)
			(void) strcat(string, MSG_ORIG(MSG_DF_INTERPOSE));

		(void) strcat(string, MSG_ORIG(MSG_GBL_CSQBRKT));

		return ((const char *)string);
	}
}

static const int tags[] = {
	MSG_DYN_NULL,		MSG_DYN_NEEDED,		MSG_DYN_PLTRELSZ,
	MSG_DYN_PLTGOT,		MSG_DYN_HASH,		MSG_DYN_STRTAB,
	MSG_DYN_SYMTAB,		MSG_DYN_RELA,		MSG_DYN_RELASZ,
	MSG_DYN_RELAENT,	MSG_DYN_STRSZ,		MSG_DYN_SYMENT,
	MSG_DYN_INIT,		MSG_DYN_FINI,		MSG_DYN_SONAME,
	MSG_DYN_RPATH,		MSG_DYN_SYMBOLIC,	MSG_DYN_REL,
	MSG_DYN_RELSZ,		MSG_DYN_RELENT,		MSG_DYN_PLTREL,
	MSG_DYN_DEBUG,		MSG_DYN_TEXTREL,	MSG_DYN_JMPREL
};

const char *
conv_dyntag_str(Sword tag)
{
	static char	string[STRSIZE] = { '\0' };

	if (tag < DT_MAXPOSTAGS)
		return (MSG_ORIG(tags[tag]));
	else {
		if (tag == DT_USED)
			return (MSG_ORIG(MSG_DYN_USED));
		else if (tag == DT_FILTER)
			return (MSG_ORIG(MSG_DYN_FILTER));
		else if (tag == DT_AUXILIARY)
			return (MSG_ORIG(MSG_DYN_AUXILIARY));
#ifdef	ENABLE_CACHE
		else if (tag == DT_CACHE)
			return (MSG_ORIG(MSG_DYN_CACHE));
#endif
		else if (tag == DT_VERDEF)
			return (MSG_ORIG(MSG_DYN_VERDEF));
		else if (tag == DT_VERDEFNUM)
			return (MSG_ORIG(MSG_DYN_VERDEFNUM));
		else if (tag == DT_VERNEED)
			return (MSG_ORIG(MSG_DYN_VERNEED));
		else if (tag == DT_VERNEEDNUM)
			return (MSG_ORIG(MSG_DYN_VERNEEDNUM));
		else if (tag == DT_FLAGS_1)
			return (MSG_ORIG(MSG_DYN_FLAGS_1));
		else if (tag == DT_RELACOUNT)
			return (MSG_ORIG(MSG_DYN_RELACOUNT));
		else if (tag == DT_RELCOUNT)
			return (MSG_ORIG(MSG_DYN_RELCOUNT));
		else if (tag == DT_SYMINFO)
			return (MSG_ORIG(MSG_DYN_SYMINFO));
		else if (tag == DT_SYMINSZ)
			return (MSG_ORIG(MSG_DYN_SYMINSZ));
		else if (tag == DT_SYMINENT)
			return (MSG_ORIG(MSG_DYN_SYMINENT));
		else if (tag == DT_POSFLAG_1)
			return (MSG_ORIG(MSG_DYN_POSFLAG_1));
		else
			return (conv_invalid_str(string, STRSIZE, (int)tag, 0));
	}
}
