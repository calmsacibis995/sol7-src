/*
 *	Copyright (c) 1996 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)syms.c	1.26	98/01/08 SMI"

/* LINTLIBRARY */

#include	"msg.h"
#include	"_debug.h"
#include	"libld.h"
#include	<stdio.h>

/*
 * Print out a single `symbol table node' entry.
 */
#if	!defined(_ELF64)
void
Gelf_sym_table_title(const char * index, const char * name)
{
	dbg_print(MSG_ORIG(MSG_SYM_TITLE), index, name);
}
void
Elf_sym_table_entry(const char * prestr, Elf32_Sym * sym, Elf32_Word verndx,
	const char * sec, const char * poststr)
{
	dbg_print(MSG_ORIG(MSG_SYM_ENTRY), prestr, EC_XWORD(sym->st_value),
		EC_XWORD(sym->st_size),
		conv_info_type_str(ELF32_ST_TYPE(sym->st_info)),
		conv_info_bind_str(ELF32_ST_BIND(sym->st_info)),
		EC_WORD(verndx), sec ? sec : conv_shndx_str(sym->st_shndx),
		poststr);
}

void
Gelf_sym_table_entry(const char * prestr, GElf_Sym * sym, GElf_Word verndx,
	const char * sec, const char * poststr)
{
	dbg_print(MSG_ORIG(MSG_SYM_ENTRY), prestr, EC_XWORD(sym->st_value),
		EC_XWORD(sym->st_size),
		conv_info_type_str(GELF_ST_TYPE(sym->st_info)),
		conv_info_bind_str(GELF_ST_BIND(sym->st_info)),
		EC_WORD(verndx), sec ? sec : conv_shndx_str(sym->st_shndx),
		poststr);
}

void
Gelf_syminfo_entry_title(const char * name)
{
	dbg_print(MSG_INTL(MSG_SYMI_TITLE1), name);
	dbg_print(MSG_INTL(MSG_SYMI_TITLE2));
}

void
Gelf_syminfo_entry(int ndx, GElf_Syminfo * sip, const char * sname,
	const char * needed)
{
	const char *	bind_str;
	char		flags[5];
	char		index[32];
	char		bind_index[32];
	int		flgndx = 0;

	if (sip->si_flags & SYMINFO_FLG_DIRECT) {
		if (sip->si_boundto == SYMINFO_BT_SELF)
			bind_str = MSG_INTL(MSG_SYMI_SELF);
		else if (sip->si_boundto == SYMINFO_BT_PARENT)
			bind_str = MSG_INTL(MSG_SYMI_PARENT);
		else
			bind_str = needed;
		flags[flgndx++] = 'D';
	} else
		bind_str = MSG_ORIG(MSG_STR_EMPTY);

	if (sip->si_flags & SYMINFO_FLG_PASSTHRU)
		flags[flgndx++] = 'P';
	if (sip->si_flags & SYMINFO_FLG_COPY)
		flags[flgndx++] = 'C';
	if (sip->si_flags & SYMINFO_FLG_LAZYLOAD)
		flags[flgndx++] = 'L';
	flags[flgndx] = '\0';

	sprintf(index, MSG_ORIG(MSG_SYMI_FMT1), ndx);
	sprintf(bind_index, MSG_ORIG(MSG_SYMI_FMT2), sip->si_boundto);
	dbg_print(MSG_ORIG(MSG_SYMI_FMT3), index, flags,
		bind_index, bind_str, sname);
}
#endif /* !defined(_ELF64) */


void
Dbg_syms_ar_title(const char * file, int found)
{
	if (DBG_NOTCLASS(DBG_SYMBOLS))
		return;

	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_INTL(MSG_SYM_AR_FILE), file,
	    found ? MSG_INTL(MSG_STR_AGAIN) : MSG_ORIG(MSG_STR_EMPTY));
}

void
Dbg_syms_ar_entry(Xword ndx, Elf_Arsym * arsym)
{
	if (DBG_NOTCLASS(DBG_SYMBOLS))
		return;

	dbg_print(MSG_INTL(MSG_SYM_AR_ENTRY), EC_XWORD(ndx), arsym->as_name);
}

void
Dbg_syms_ar_checking(Xword ndx, Elf_Arsym * arsym, const char * name)
{
	if (DBG_NOTCLASS(DBG_SYMBOLS))
		return;

	dbg_print(MSG_INTL(MSG_SYM_AR_CHECK), EC_XWORD(ndx),
		arsym->as_name, name);
}

void
Dbg_syms_ar_resolve(Xword ndx, Elf_Arsym * arsym, const char * name, int flag)
{
	if (DBG_NOTCLASS(DBG_SYMBOLS))
		return;

	if (flag)
		dbg_print(MSG_INTL(MSG_SYM_AR_FORCEDEXRT),
			EC_XWORD(ndx), arsym->as_name, name);
	else
		dbg_print(MSG_INTL(MSG_SYM_AR_RESOLVE),
			EC_XWORD(ndx), arsym->as_name, name);
}

void
Dbg_syms_spec_title()
{
	if (DBG_NOTCLASS(DBG_SYMBOLS))
		return;

	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_INTL(MSG_SYM_SPECIAL));
}

void
Dbg_syms_discarded(Sym_desc * sdp, Is_desc * disp)
{
	if (DBG_NOTCLASS(DBG_SYMBOLS))
		return;
	if (DBG_NOTDETAIL())
		return;
	dbg_print(MSG_INTL(MSG_SYM_DISCARDED), sdp->sd_name,
		disp->is_basename, disp->is_file->ifl_name);
}


void
Dbg_syms_entered(Sym * sym, Sym_desc * sdp)
{
	if (DBG_NOTCLASS(DBG_SYMBOLS))
		return;
	if (DBG_NOTDETAIL())
		return;

	Elf_sym_table_entry(MSG_INTL(MSG_STR_ENTERED), sym,
	    sdp->sd_aux ? sdp->sd_aux->sa_verndx : 0, NULL,
	    conv_deftag_str(sdp->sd_ref));
}

void
Dbg_syms_process(Ifl_desc * ifl)
{
	if (DBG_NOTCLASS(DBG_SYMBOLS))
		return;

	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_INTL(MSG_SYM_PROCESS), ifl->ifl_name,
	    conv_etype_str(ifl->ifl_ehdr->e_type));
}

void
Dbg_syms_entry(Xword ndx, Sym_desc * sdp)
{
	if (DBG_NOTCLASS(DBG_SYMBOLS))
		return;

	dbg_print(MSG_INTL(MSG_SYM_BASIC), EC_XWORD(ndx), sdp->sd_name);
}

void
Dbg_syms_global(Xword ndx, const char * name)
{
	if (DBG_NOTCLASS(DBG_SYMBOLS))
		return;

	dbg_print(MSG_INTL(MSG_SYM_ADDING), EC_XWORD(ndx), name);
}

void
Dbg_syms_sec_title()
{
	if (DBG_NOTCLASS(DBG_SYMBOLS))
		return;
	if (DBG_NOTDETAIL())
		return;

	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_INTL(MSG_SYM_INDEX));
}

void
Dbg_syms_sec_entry(int ndx, Sg_desc * sgp, Os_desc * osp)
{
	if (DBG_NOTCLASS(DBG_SYMBOLS))
		return;
	if (DBG_NOTDETAIL())
		return;

	dbg_print(MSG_INTL(MSG_SYM_SECTION), ndx, osp->os_name,
		(*sgp->sg_name ? sgp->sg_name : MSG_INTL(MSG_STR_NULL)));
}

void
Dbg_syms_up_title()
{
	if (DBG_NOTCLASS(DBG_SYMBOLS))
		return;
	if (DBG_NOTDETAIL())
		return;

	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_INTL(MSG_SYM_FINAL));
	Gelf_sym_table_title(MSG_ORIG(MSG_STR_EMPTY), MSG_ORIG(MSG_STR_EMPTY));
}

void
Dbg_syms_old(Sym_desc *	sdp)
{
	if (DBG_NOTCLASS(DBG_SYMBOLS))
		return;
	if (DBG_NOTDETAIL())
		return;

	Elf_sym_table_entry(MSG_INTL(MSG_STR_OLD), sdp->sd_sym,
	    sdp->sd_aux ? sdp->sd_aux->sa_verndx : 0, NULL, sdp->sd_name);
}

void
Dbg_syms_new(Sym * sym, Sym_desc * sdp)
{
	if (DBG_NOTCLASS(DBG_SYMBOLS))
		return;
	if (DBG_NOTDETAIL())
		return;

	Elf_sym_table_entry(MSG_INTL(MSG_STR_NEW), sym,
	    sdp->sd_aux ? sdp->sd_aux->sa_verndx : 0, NULL,
	    conv_deftag_str(sdp->sd_ref));
}

void
Dbg_syms_updated(Sym_desc * sdp, const char * name)
{
	if (DBG_NOTCLASS(DBG_SYMBOLS))
		return;

	dbg_print(MSG_INTL(MSG_SYM_UPDATE), name);

	if (DBG_NOTDETAIL())
		return;

	Elf_sym_table_entry(MSG_ORIG(MSG_STR_EMPTY), sdp->sd_sym,
	    sdp->sd_aux ? sdp->sd_aux->sa_verndx : 0, NULL,
	    conv_deftag_str(sdp->sd_ref));
}

void
Dbg_syms_created(const char * name)
{
	if (DBG_NOTCLASS(DBG_SYMBOLS))
		return;

	dbg_print(MSG_INTL(MSG_SYM_CREATE), name);
}

void
Dbg_syms_resolving1(Xword ndx, const char * name, int row, int col)
{
	if (DBG_NOTCLASS(DBG_SYMBOLS))
		return;

	dbg_print(MSG_INTL(MSG_SYM_RESOLVING), EC_XWORD(ndx), name, row, col);
}

void
Dbg_syms_resolving2(Sym * osym, Sym * nsym, Sym_desc * sdp, Ifl_desc * ifl)
{
	if (DBG_NOTCLASS(DBG_SYMBOLS))
		return;
	if (DBG_NOTDETAIL())
		return;

	Elf_sym_table_entry(MSG_INTL(MSG_STR_OLD), osym,
	    sdp->sd_aux ? sdp->sd_aux->sa_verndx : 0, NULL,
	    sdp->sd_file->ifl_name);
	Elf_sym_table_entry(MSG_INTL(MSG_STR_NEW), nsym, 0, NULL,
	    ifl->ifl_name);
}

void
Dbg_syms_resolved(Sym_desc * sdp)
{
	if (DBG_NOTCLASS(DBG_SYMBOLS))
		return;
	if (DBG_NOTDETAIL())
		return;

	Elf_sym_table_entry(MSG_INTL(MSG_STR_RESOLVED), sdp->sd_sym,
	    sdp->sd_aux ? sdp->sd_aux->sa_verndx : 0, NULL,
	    conv_deftag_str(sdp->sd_ref));
}

void
Dbg_syms_nl()
{
	if (DBG_NOTCLASS(DBG_SYMBOLS))
		return;

	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
}

static Boolean	symbol_title = TRUE;

static void
_Dbg_syms_reloc_title()
{
	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_INTL(MSG_SYM_BSS));

	symbol_title = FALSE;
}
void
Dbg_syms_reloc(Sym_desc * sdp, Boolean copy)
{
	if (DBG_NOTCLASS(DBG_SYMBOLS))
		return;

	if (symbol_title)
		_Dbg_syms_reloc_title();
	dbg_print(MSG_INTL(MSG_SYM_UPDATE), sdp->sd_name);

	if (DBG_NOTDETAIL())
		return;

	Elf_sym_table_entry(MSG_ORIG(copy ? MSG_SYM_COPY : MSG_STR_EMPTY),
	    sdp->sd_sym, sdp->sd_aux ? sdp->sd_aux->sa_verndx : 0, NULL,
	    conv_deftag_str(sdp->sd_ref));
}

void
Dbg_syms_lookup_aout(const char * name)
{
	if (DBG_NOTCLASS(DBG_SYMBOLS))
		return;

	dbg_print(MSG_INTL(MSG_SYM_AOUT), name);
}

void
Dbg_syms_lookup(const char * name, const char * file, const char * type)
{
	if (DBG_NOTCLASS(DBG_SYMBOLS))
		return;

	dbg_print(MSG_INTL(MSG_SYM_LOOKUP), name, file, type);
}

void
Dbg_syms_dlsym(const char * file, const char * name, int next)
{
	if (DBG_NOTCLASS(DBG_SYMBOLS))
		return;

	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	dbg_print(MSG_INTL(MSG_SYM_DLSYM), name, file,
	    MSG_ORIG(next ? MSG_SYM_NEXT : MSG_STR_EMPTY));
}

void
Dbg_syms_reduce(Sym_desc * sdp)
{
	static Boolean	sym_reduce_title = TRUE;

	if (DBG_NOTCLASS(DBG_SYMBOLS | DBG_VERSIONS))
		return;

	if (sym_reduce_title) {
		sym_reduce_title = FALSE;
		dbg_print(MSG_ORIG(MSG_STR_EMPTY));
		dbg_print(MSG_INTL(MSG_SYM_REDUCED));
	}

	if (sdp->sd_flags & FLG_SY_ELIM)
		dbg_print(MSG_INTL(MSG_SYM_ELIMINATING), sdp->sd_name);
	else
		dbg_print(MSG_INTL(MSG_SYM_REDUCING), sdp->sd_name);

	if (DBG_NOTDETAIL())
		return;

	Elf_sym_table_entry(MSG_ORIG(MSG_SYM_LOCAL), sdp->sd_sym,
	    sdp->sd_aux ? sdp->sd_aux->sa_verndx : 0, NULL,
	    sdp->sd_file->ifl_name);
}



void
Dbg_syminfo_title()
{
	if (DBG_NOTCLASS(DBG_SYMBOLS))
		return;
	if (DBG_NOTDETAIL())
		return;
	dbg_print(MSG_INTL(MSG_SYMI_DTITLE1));
}

void
Dbg_syminfo_entry(int ndx, Syminfo * sip, Sym * sym, const char * strtab,
	Dyn * dyn)
{
	GElf_Syminfo	gsi;
	const char *	sname;
	const char *	needed;
	if (DBG_NOTCLASS(DBG_SYMBOLS))
		return;
	if (DBG_NOTDETAIL())
		return;
	sname = strtab + sym->st_name;
	if (sip->si_boundto < SYMINFO_BT_LOWRESERVE)
		needed = strtab + dyn[sip->si_boundto].d_un.d_val;
	else
		needed = 0;

	Gelf_syminfo_entry(ndx, &gsi, sname, needed);
}
