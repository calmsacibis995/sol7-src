/*
 *	Copyright (c) 1996 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)audit.c	1.9	97/11/19 SMI"

/* LINTLIBRARY */

#include	<dlfcn.h>
#include	"_debug.h"
#include	"msg.h"
#include	"libld.h"


void
Dbg_audit_version(const char * lib, ulong_t version)
{
	if (DBG_NOTCLASS(DBG_AUDITING))
		return;
	dbg_print(MSG_INTL(MSG_AUD_VERSION), lib, version);
}

void
Dbg_audit_badvers(const char * lib, ulong_t version)
{
	if (DBG_NOTCLASS(DBG_AUDITING))
		return;
	dbg_print(MSG_INTL(MSG_AUD_BADVERS), lib, version);
}

void
Dbg_audit_init(const char * libname)
{
	if (DBG_NOTCLASS(DBG_AUDITING))
		return;
	dbg_print(MSG_INTL(MSG_AUD_INIT), libname);
}

void
Dbg_audit_bound(const char * auditlib, const char * symname)
{
	if (DBG_NOTCLASS(DBG_AUDITING))
		return;
	dbg_print(MSG_INTL(MSG_AUD_BOUND), auditlib, symname);
}

void
Dbg_audit_libaudit(const char * soname)
{
	if (DBG_NOTCLASS(DBG_AUDITING))
		return;
	dbg_print(MSG_INTL(MSG_AUD_LIBAUDIT), soname);
}

void
Dbg_audit_pltenter(Sym * symp, const char * sym_name, const char * aud_lib)
{
	if (DBG_NOTCLASS(DBG_AUDITING))
		return;
	if (DBG_NOTDETAIL())
		return;
	dbg_print(MSG_INTL(MSG_AUD_PLTENTER), aud_lib,
		sym_name, EC_ADDR(symp->st_value));
}

void
Dbg_audit_pltbind(Sym * symp, const char * sym_name, const char * aud_lib,
	Addr newval)
{
	if (DBG_NOTCLASS(DBG_AUDITING))
		return;
	if (DBG_NOTDETAIL())
		return;
	dbg_print(MSG_INTL(MSG_AUD_PLTBIND), aud_lib, sym_name,
		EC_ADDR(newval));
}

void
Dbg_audit_symbind(const char * audlib, const char * symname, uint_t symndx)
{
	if (DBG_NOTCLASS(DBG_AUDITING))
		return;
	if (DBG_NOTDETAIL())
		return;
	dbg_print(MSG_INTL(MSG_AUD_SYMBIND), audlib, symname, EC_WORD(symndx));
}

void
Dbg_audit_symnewval(const char * symname, const char * audlib, Addr oldvalue,
	Addr newvalue)
{
	if (DBG_NOTCLASS(DBG_AUDITING))
		return;
	if (DBG_NOTDETAIL())
		return;
	dbg_print(MSG_INTL(MSG_AUD_SYMNEW), audlib, symname,
		EC_XWORD(oldvalue), EC_XWORD(newvalue));
}
