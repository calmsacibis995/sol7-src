/*
 *	Copyright (c) 1988 AT&T
 *	  All Rights Reserved
 *
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
 *	The copyright notice above does not evidence any
 *	actual or intended publication of such source code.
 *
 *	Copyright (c) 1997 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)sections.c	1.56	98/02/06 SMI"

/* LINTLIBRARY */

/*
 * Module sections. Initialize special sections
 */
#include	<string.h>
#include	<stdio.h>
#include	<unistd.h>
#include	<link.h>
#include	"debug.h"
#include	"msg.h"
#include	"_libld.h"

static const char
	* rel_prefix =	M_REL_PREFIX;

/*
 * Build a .bss section for allocation of tentative definitions.  Any `static'
 * .bss definitions would have been associated to their own .bss sections and
 * thus collected from the input files.  `global' .bss definitions are tagged
 * as COMMON and do not cause any associated .bss section elements to be
 * generated.  Here we add up all these COMMON symbols and generate the .bss
 * section required to represent them.
 */
Xword
make_bss(Ofl_desc * ofl, Xword size, Xword align)
{
	Shdr *		shdr;
	Elf_Data *	data;
	Is_desc *	isec;
	Xword		rsize = (Xword)ofl->ofl_relocbsssz;

	/*
	 * Allocate and initialize the Elf_Data structure.
	 */
	if ((data = (Elf_Data *)libld_calloc(sizeof (Elf_Data), 1)) == 0)
		return (S_ERROR);
	data->d_type = ELF_T_BYTE;
	data->d_size = (size_t)size;
	data->d_align = (size_t)align;
	data->d_version = ofl->ofl_libver;

	/*
	 * Allocate and initialize the Shdr structure.
	 */
	if ((shdr = (Shdr *)libld_calloc(sizeof (Shdr), 1)) == 0)
		return (S_ERROR);
	shdr->sh_type = SHT_NOBITS;
	shdr->sh_flags = SHF_ALLOC | SHF_WRITE;
	shdr->sh_size = size;
	shdr->sh_addralign = align;

	/*
	 * Allocate and initialize the Is_desc structure.
	 */
	if ((isec = (Is_desc *)libld_calloc(1, sizeof (Is_desc))) ==
	    (Is_desc *)0)
		return (S_ERROR);
	isec->is_name = MSG_ORIG(MSG_SCN_BSS);
	isec->is_shdr = shdr;
	isec->is_indata = data;

	/*
	 * Retain this .bss input section as this will be where global
	 * symbol references are added.
	 */
	ofl->ofl_isbss = isec;
	if (place_section(ofl, isec, M_ID_BSS, 0) == (Os_desc *)S_ERROR)
		return (S_ERROR);

	/*
	 * Make a relocation section if required.
	 */
	if (rsize) {
		Os_desc *	osp = ofl->ofl_isbss->is_osdesc;

		osp->os_szoutrels = rsize;
		if (!(ofl->ofl_flags1 & FLG_OF1_RELCNT))
			if (make_reloc(ofl, osp) == S_ERROR)
				return (S_ERROR);
	}
	return (1);
}

/*
 * Build a comment section (-Qy option).
 */
Xword
make_comment(Ofl_desc * ofl)
{
	Shdr *		shdr;
	Elf_Data *	data;
	Is_desc *	isec;

	/*
	 * Allocate and initialize the Elf_Data structure.
	 */
	if ((data = (Elf_Data *)libld_calloc(sizeof (Elf_Data), 1)) == 0)
		return (S_ERROR);
	data->d_type = ELF_T_BYTE;
	data->d_buf = (void *)ofl->ofl_sgsid;
	data->d_size = strlen(ofl->ofl_sgsid) + 1;
	data->d_align = 1;
	data->d_version = ofl->ofl_libver;

	/*
	 * Allocate and initialize the Shdr structure.
	 */
	if ((shdr = (Shdr *)libld_calloc(sizeof (Shdr), 1)) == 0)
		return (S_ERROR);
	shdr->sh_type = SHT_PROGBITS;
	shdr->sh_size = data->d_size;
	shdr->sh_addralign = 1;

	/*
	 * Allocate and initialize the Is_desc structure.
	 */
	if ((isec = (Is_desc *)libld_calloc(1, sizeof (Is_desc))) ==
	    (Is_desc *)0)
		return (S_ERROR);
	isec->is_name = MSG_ORIG(MSG_SCN_COMMENT);
	isec->is_shdr = shdr;
	isec->is_indata = data;

	return ((Xword)place_section(ofl, isec, M_ID_NOTE, 0));
}

/*
 * Make the dynamic section.  Calculate the size of any strings referenced
 * within this structure, they will be added to the global string table
 * (.dynstr).  This routine should be called before make_dynstr().
 */
Xword
make_dynamic(Ofl_desc * ofl)
{
	Shdr *		shdr;
	Elf_Data *	data;
	Is_desc *	isec;
	size_t		cnt = 0;
	Listnode *	lnp;
	Ifl_desc *	ifl;
	Sym_desc *	sdp;
	size_t		size;
	Word		flags = ofl->ofl_flags;

	/*
	 * Reserve entries for any needed dependencies.
	 */
	for (LIST_TRAVERSE(&ofl->ofl_sos, lnp, ifl)) {
		Sdf_desc *	sdf;

		if (!(ifl->ifl_flags & (FLG_IF_NEEDED | FLG_IF_NEEDSTR)))
			continue;

		/*
		 * If this dependency didn't satisfy any symbol references
		 * generate a debugging diagnostic (ld(1) -Dbasic should be
		 * enough to display these).  If this is a standard needed
		 * dependency and -z ignore is in effect, drop the dependency.
		 * Explicitly defined dependencies (i.e., -N dep) won't get
		 * dropped, and are flagged as being required to simplify
		 * update_odynamic() processing.
		 */
		if (!(ifl->ifl_flags & FLG_IF_DEPREQD)) {
			DBG_CALL(Dbg_file_unused(ifl->ifl_soname));

			if (ifl->ifl_flags & FLG_IF_NEEDSTR)
				ifl->ifl_flags |= FLG_IF_DEPREQD;
			else if (ifl->ifl_flags & FLG_IF_IGNORE)
				continue;
		}

		/*
		 * If this object has an accompanying shared object definition
		 * determine if a USED status is required, or whether an
		 * alternative shared object name has been specified.
		 */
		if ((sdf = ifl->ifl_sdfdesc) != 0) {
			if (sdf->sdf_flags & FLG_SDF_USED) {
				ifl->ifl_flags &= ~FLG_IF_NEEDED;
				ifl->ifl_flags |= FLG_IF_USED;
			}
			if (sdf->sdf_flags & FLG_SDF_SONAME)
				ifl->ifl_soname = sdf->sdf_soname;
		}
		/*
		 * If this object is to be lazyload save space
		 * for the DT_POSFLAG1 entry.
		 */
		if (ifl->ifl_flags & (FLG_IF_LAZYLD | FLG_IF_GRPPRM))
			cnt++;
		ofl->ofl_dynstrsz += strlen(ifl->ifl_soname) + 1;
		cnt++;

		/*
		 * If the needed entry contains the $ORIGIN token make sure
		 * the associated DT_1_FLAGS entry is created.
		 */
		if (strstr(ifl->ifl_soname, MSG_ORIG(MSG_STR_ORIGIN)))
			ofl->ofl_dtflags |= DF_1_ORIGIN;
	}

	/*
	 * Reserve entries for any _init() and _fini() section addresses.
	 */
	if (((sdp = sym_find(MSG_ORIG(MSG_SYM_INIT_U),
	    SYM_NOHASH, ofl)) != NULL) && sdp->sd_ref == REF_REL_NEED) {
		sdp->sd_flags |= FLG_SY_UPREQD;
		cnt++;
	}
	if (((sdp = sym_find(MSG_ORIG(MSG_SYM_FINI_U),
	    SYM_NOHASH, ofl)) != NULL) && sdp->sd_ref == REF_REL_NEED) {
		sdp->sd_flags |= FLG_SY_UPREQD;
		cnt++;
	}

	/*
	 * Reserve entries for any soname, filter name (shared libs only),
	 * run-path pointers and cache name.
	 */
	if (ofl->ofl_soname) {
		cnt++;
		ofl->ofl_dynstrsz += strlen(ofl->ofl_soname) + 1;
	}
	if (ofl->ofl_filtees) {
		cnt++;
		ofl->ofl_dynstrsz += strlen(ofl->ofl_filtees) + 1;

		/*
		 * If the filtees entry contains the $ORIGIN token make sure
		 * the associated DT_1_FLAGS entry is created.
		 */
		if (strstr(ofl->ofl_filtees, MSG_ORIG(MSG_STR_ORIGIN)))
			ofl->ofl_dtflags |= DF_1_ORIGIN;
	}
	if (ofl->ofl_rpath) {
		cnt++;
		ofl->ofl_dynstrsz += strlen(ofl->ofl_rpath) + 1;

		/*
		 * If the rpath entry contains the $ORIGIN token make sure
		 * the associated DT_1_FLAGS entry is created.
		 */
		if (strstr(ofl->ofl_rpath, MSG_ORIG(MSG_STR_ORIGIN)))
			ofl->ofl_dtflags |= DF_1_ORIGIN;
	}
	if (ofl->ofl_cache) {
		cnt++;
		ofl->ofl_dynstrsz += strlen(ofl->ofl_cache) + 1;

		/*
		 * If the cache entry contains the $ORIGIN token make sure
		 * the associated DT_1_FLAGS entry is created.
		 */
		if (strstr(ofl->ofl_cache, MSG_ORIG(MSG_STR_ORIGIN)))
			ofl->ofl_dtflags |= DF_1_ORIGIN;
	}

	/*
	 * Reserve entries for versioning.
	 */
	if ((flags & (FLG_OF_VERDEF | FLG_OF_NOVERSEC)) == FLG_OF_VERDEF)
		cnt += 2;			/* DT_VERDEF & DT_VERDEFNUM */

	if ((flags & (FLG_OF_VERNEED | FLG_OF_NOVERSEC)) == FLG_OF_VERNEED)
		cnt += 2;			/* DT_VERNEED & DT_VERNEEDNUM */

	/*
	 * The folowing DT_* entries do not apply to relocatable objects
	 */
	if (!(ofl->ofl_flags & FLG_OF_RELOBJ)) {
		/*
		 * Reserve entries for the HASH, STRTAB, SYMTAB, STRSZ,
		 * SYMENT, and for the text relocation flag if
		 * necessary.
		 */
		cnt += 5;

		if ((ofl->ofl_flags1 & FLG_OF1_RELCNT) &&
		    ofl->ofl_relocrelcnt)		/* RELACOUNT */
			cnt++;

		if (flags & FLG_OF_TEXTREL)	/* TEXTREL */
			cnt++;

		/*
		 * If we have plt's reserve a PLT, PLTSZ, PLTREL and
		 * JMPREL entry.
		 */
		if (ofl->ofl_pltcnt != M_PLT_XNumber)
			cnt += 4;

		/*
		 * If we have any relocations reserve a REL, RELSZ and
		 * RELENT entry.
		 */
		if (ofl->ofl_relocsz)
			cnt += 3;

		/*
		 * Are we building a .syminfo section.  If so create
		 * SYMINFO, SYMINSZ, & SYMINENT entries.
		 */
		if (ofl->ofl_ossyminfo)
			cnt += 3;


		if (ofl->ofl_osinterp)
			cnt++;			/* DEBUG */
	}


	if (flags & (FLG_OF_SYMBOLIC | FLG_OF_BINDSYMB))
		cnt++;				/* SYMBOLIC */

	if (ofl->ofl_dtflags)			/* DT_FLAGS_1 */
		cnt++;

	cnt++;					/* NULL */

	/*
	 * Allocate and initialize the Shdr structure.
	 */
	if ((shdr = (Shdr *)libld_calloc(sizeof (Shdr), 1)) == 0)
		return (S_ERROR);
	shdr->sh_type = SHT_DYNAMIC;
	shdr->sh_flags = SHF_WRITE;
	if (!(flags & FLG_OF_RELOBJ))
		shdr->sh_flags |= SHF_ALLOC;
	shdr->sh_addralign = M_WORD_ALIGN;
	shdr->sh_entsize = elf_fsize(ELF_T_DYN, 1, ofl->ofl_libver);
	if (shdr->sh_entsize == 0) {
		eprintf(ERR_ELF, MSG_ORIG(MSG_ELF_FSIZE), ofl->ofl_name);
		return (S_ERROR);
	}

	/*
	 * Determine the size of the section from the number of entries.
	 */
	size = cnt * (size_t)shdr->sh_entsize;

	/*
	 * Allocate and initialize the Elf_Data structure.
	 */
	if ((data = (Elf_Data *)libld_calloc(sizeof (Elf_Data), 1)) == 0)
		return (S_ERROR);
	data->d_type = ELF_T_DYN;
	data->d_size = size;
	data->d_align = M_WORD_ALIGN;
	data->d_version = ofl->ofl_libver;

	shdr->sh_size = size;

	/*
	 * Allocate and initialize the Is_desc structure.
	 */
	if ((isec = (Is_desc *)libld_calloc(1, sizeof (Is_desc))) ==
	    (Is_desc *)0)
		return (S_ERROR);
	isec->is_name = MSG_ORIG(MSG_SCN_DYNAMIC);
	isec->is_shdr = shdr;
	isec->is_indata = data;

	ofl->ofl_osdynamic = place_section(ofl, isec, M_ID_DYNAMIC, 0);
	return ((Xword)ofl->ofl_osdynamic);
}

/*
 * Build the GOT section and its associated relocation entries.
 */
Xword
make_got(Ofl_desc * ofl)
{
	Shdr *		shdr;
	Elf_Data *	data;
	Is_desc *	isec;
	size_t		size = (size_t)ofl->ofl_gotcnt * M_GOT_ENTSIZE;
	size_t		rsize = (size_t)ofl->ofl_relocgotsz;

	/*
	 * Allocate and initialize the Elf_Data structure.
	 */
	if ((data = (Elf_Data *)libld_calloc(sizeof (Elf_Data), 1)) == 0)
		return (S_ERROR);
	data->d_type = ELF_T_BYTE;
	data->d_size = size;
	data->d_align = M_WORD_ALIGN;
	data->d_version = ofl->ofl_libver;

	/*
	 * Allocate and initialize the Shdr structure.
	 */
	if ((shdr = (Shdr *)libld_calloc(sizeof (Shdr), 1)) == 0)
		return (S_ERROR);
	shdr->sh_type = SHT_PROGBITS;
	shdr->sh_flags = SHF_ALLOC | SHF_WRITE;
	shdr->sh_size = size;
	shdr->sh_addralign = M_WORD_ALIGN;
	shdr->sh_entsize = M_GOT_ENTSIZE;

	/*
	 * Allocate and initialize the Is_desc structure.
	 */
	if ((isec = (Is_desc *)libld_calloc(1, sizeof (Is_desc))) ==
	    (Is_desc *)0)
		return (S_ERROR);
	isec->is_name = MSG_ORIG(MSG_SCN_GOT);
	isec->is_shdr = shdr;
	isec->is_indata = data;

	if ((ofl->ofl_osgot = place_section(ofl, isec, M_ID_GOT, 0)) ==
	    (Os_desc *)S_ERROR)
		return (S_ERROR);

	/*
	 * Make a relocation section if required
	 */
	if (rsize) {
		ofl->ofl_osgot->os_szoutrels = rsize;
		if (!(ofl->ofl_flags1 & FLG_OF1_RELCNT))
			if (make_reloc(ofl, ofl->ofl_osgot) == S_ERROR)
				return (S_ERROR);
	}
	return (1);
}

/*
 * Build an interp section.
 */
Xword
make_interp(Ofl_desc * ofl)
{
	Shdr *		shdr;
	Elf_Data *	data;
	Is_desc *	isec;
	const char *	iname = ofl->ofl_interp;
	size_t		size;

	/*
	 * We always build an .interp section for dynamic executables.  However
	 * if the user has specifically specified an interpretor we'll build
	 * this section for any output (presumably the user knows what they are
	 * doing. refer ABI section 5-4, and ld.1 man page use of -I).
	 */
	if (((ofl->ofl_flags & (FLG_OF_DYNAMIC | FLG_OF_EXEC |
	    FLG_OF_RELOBJ)) != (FLG_OF_DYNAMIC | FLG_OF_EXEC)) && !iname)
		return (1);

	/*
	 * In the case of a dynamic executable supply a default interpretor
	 * if a specific interpreter has not been specified.
	 */
	if (!iname)
#ifdef _ELF64
		iname = ofl->ofl_interp = MSG_ORIG(MSG_PTH_RTLD_64);
#else
		iname = ofl->ofl_interp = MSG_ORIG(MSG_PTH_RTLD);
#endif

	size = strlen(iname) + 1;

	/*
	 * Allocate and initialize the Elf_Data structure.
	 */
	if ((data = (Elf_Data *)libld_calloc(sizeof (Elf_Data), 1)) == 0)
		return (S_ERROR);
	data->d_type = ELF_T_BYTE;
	data->d_size = size;
	data->d_version = ofl->ofl_libver;

	/*
	 * Allocate and initialize the Shdr structure.
	 */
	if ((shdr = (Shdr *)libld_calloc(sizeof (Shdr), 1)) == 0)
		return (S_ERROR);
	shdr->sh_type = SHT_PROGBITS;
	shdr->sh_flags = SHF_ALLOC;
	shdr->sh_size = size;

	/*
	 * Allocate and initialize the Is_desc structure.
	 */
	if ((isec = (Is_desc *)libld_calloc(1, sizeof (Is_desc))) ==
	    (Is_desc *)0)
		return (S_ERROR);
	isec->is_name = MSG_ORIG(MSG_SCN_INTERP);
	isec->is_shdr = shdr;
	isec->is_indata = data;

	ofl->ofl_osinterp = place_section(ofl, isec, M_ID_INTERP, 0);
	return ((Xword)ofl->ofl_osinterp);
}

/*
 * Build the PLT section and its associated relocation entries.
 */
Xword
make_plt(Ofl_desc * ofl)
{
	Shdr *		shdr;
	Elf_Data *	data;
	Is_desc *	isec;
	size_t		size = (size_t)ofl->ofl_pltcnt * M_PLT_ENTSIZE;
	size_t		rsize = (size_t)ofl->ofl_relocpltsz;

#if defined(sparc)
	/*
	 * Account for the NOP at the end of the plt.
	 */
	size += sizeof (Word);
#endif

	/*
	 * Allocate and initialize the Elf_Data structure.
	 */
	if ((data = (Elf_Data *)libld_calloc(sizeof (Elf_Data), 1)) == 0)
		return (S_ERROR);
	data->d_type = ELF_T_BYTE;
	data->d_size = size;
	data->d_align = M_PLT_ALIGN;
	data->d_version = ofl->ofl_libver;

	/*
	 * Allocate and initialize the Shdr structure.
	 */
	if ((shdr = (Shdr *)libld_calloc(sizeof (Shdr), 1)) == 0)
		return (S_ERROR);
	shdr->sh_type = SHT_PROGBITS;
	shdr->sh_flags = M_PLT_SHF_FLAGS;
	shdr->sh_size = size;
	shdr->sh_addralign = M_PLT_ALIGN;
	shdr->sh_entsize = M_PLT_ENTSIZE;

	/*
	 * Allocate and initialize the Is_desc structure.
	 */
	if ((isec = (Is_desc *)libld_calloc(1, sizeof (Is_desc))) ==
	    (Is_desc *)0)
		return (S_ERROR);
	isec->is_name = MSG_ORIG(MSG_SCN_PLT);
	isec->is_shdr = shdr;
	isec->is_indata = data;

	if ((ofl->ofl_osplt = place_section(ofl, isec, M_ID_PLT, 0)) ==
	    (Os_desc *)S_ERROR)
		return (S_ERROR);

	/*
	 * Make a relocation section if required.
	 */
	if (rsize) {
		ofl->ofl_osplt->os_szoutrels = rsize;
		if (make_reloc(ofl, ofl->ofl_osplt) == S_ERROR)
			return (S_ERROR);
	}
	return (1);
}

/*
 * Make the hash table.  Only built for dynamic executables and shared
 * libraries, and provides hashed lookup into the global symbol table
 * (.dynsym) for the run-time linker to resolve symbol lookups.
 *
 * The following bucket sizes are a sequence of prime numbers that seem
 * to provide the best concentration of single entry hash lists (within
 * the confines of the present elf_hash() functionality.
 */
static const hashsize[] = {
	3,	7,	13,	31,	53,	67,	83,	97,
	101,	151,	211,	251,	307,	353,	401,	457,	503,
	557,	601,	653,	701,	751,	809,	859,	907,	953,
	1009,	1103,	1201,	1301,	1409,	1511,	1601,	1709,	1801,
	1901,	2003,	2111,	2203,	2309,	2411,	2503,	2609,	2707,
	2801,	2903,	3001,	3109,	3203,	3301,	3407,	3511,	3607,
	3701,	3803,	3907,	4001,	5003,   6101,   7001,   8101,   9001,
	SYM_MAXNBKTS

};

Xword
make_hash(Ofl_desc * ofl)
{
	Shdr *		shdr;
	Elf_Data *	data;
	Is_desc *	isec;
	size_t		size;
	Xword		nsyms = ofl->ofl_globcnt;
	Xword		cnt;
	Word		ndx;

	/*
	 * Allocate and initialize the Elf_Data structure.
	 */
	if ((data = (Elf_Data *)libld_calloc(sizeof (Elf_Data), 1)) == 0)
		return (S_ERROR);
	data->d_type = ELF_T_WORD;
	data->d_align = M_WORD_ALIGN;
	data->d_version = ofl->ofl_libver;

	/*
	 * Allocate and initialize the Shdr structure.
	 */
	if ((shdr = (Shdr *)libld_calloc(sizeof (Shdr), 1)) == 0)
		return (S_ERROR);
	shdr->sh_type = SHT_HASH;
	shdr->sh_flags = SHF_ALLOC;
	shdr->sh_addralign = M_WORD_ALIGN;
	shdr->sh_entsize = elf_fsize(ELF_T_WORD, 1, ofl->ofl_libver);
	if (shdr->sh_entsize == 0) {
		eprintf(ERR_ELF, MSG_ORIG(MSG_ELF_FSIZE), ofl->ofl_name);
		return (S_ERROR);
	}

	/*
	 * Allocate and initialize the Is_desc structure.
	 */
	if ((isec = (Is_desc *)libld_calloc(1, sizeof (Is_desc))) ==
	    (Is_desc *)0)
		return (S_ERROR);
	isec->is_name = MSG_ORIG(MSG_SCN_HASH);
	isec->is_shdr = shdr;
	isec->is_indata = data;

	/*
	 * Place the section first since it will affect the local symbol
	 * count.
	 */
	if ((ofl->ofl_oshash = place_section(ofl, isec, M_ID_HASH, 0)) ==
	    (Os_desc *)S_ERROR)
		return (S_ERROR);

	/*
	 * Calculate the number of output hash buckets.
	 */
	for (ndx = 0; ndx < (sizeof (hashsize) / sizeof (int)); ndx++) {
		if (nsyms > hashsize[ndx])
			continue;
		ofl->ofl_hashbkts = hashsize[ndx];
		break;
	}
	if (!ofl->ofl_hashbkts)
		ofl->ofl_hashbkts = SYM_MAXNBKTS;

	/*
	 * The size of the hash table is determined by
	 *
	 *	i.	the initial nbucket and nchain entries
	 *	ii.	the number of buckets (calculated above)
	 *	iii.	the number of chains (this is based on the number of
	 *		symbols in the .dynsym array.  At this point we have
	 *		to account for sections .dynsym, .strtab, .symtab,
	 *		and .shstrtab that have not been processed yet.
	 *		Refer to processing order in main()).
	 *
	 * Below, 5 = i), .dynsym and .shstrtab, and initial null
	 * symbol entry.
	 */
	cnt = (size_t)(5 + ofl->ofl_hashbkts + ofl->ofl_shdrcnt +
		ofl->ofl_globcnt);
	if (!(ofl->ofl_flags & FLG_OF_STRIP))
		cnt += 2;
	size = (size_t)cnt * shdr->sh_entsize;

	/*
	 * Finalize the section header and data buffer initialization.
	 */
	if ((data->d_buf = (Elf_Void *)libld_calloc(size, 1)) == 0)
		return (S_ERROR);
	data->d_size = size;
	shdr->sh_size = size;

	return (1);
}

/*
 * Generate the standard symbol table.  Contains all locals and globals,
 * and resides in a non-allocatable section (ie. it can be stripped).
 */
Xword
make_symtab(Ofl_desc * ofl)
{
	Shdr *		shdr;
	Elf_Data *	data;
	Is_desc *	isec;
	size_t		size;

	/*
	 * Allocate and initialize the Elf_Data structure.
	 */
	if ((data = (Elf_Data *)libld_calloc(sizeof (Elf_Data), 1)) == 0)
		return (S_ERROR);
	data->d_type = ELF_T_SYM;
	data->d_align = M_WORD_ALIGN;
	data->d_version = ofl->ofl_libver;

	/*
	 * Allocate and initialize the Shdr structure.
	 */
	if ((shdr = (Shdr *)libld_calloc(sizeof (Shdr), 1)) == 0)
		return (S_ERROR);
	shdr->sh_type = SHT_SYMTAB;
	shdr->sh_addralign = M_WORD_ALIGN;
	shdr->sh_entsize = elf_fsize(ELF_T_SYM, 1, ofl->ofl_libver);
	if (shdr->sh_entsize == 0) {
		eprintf(ERR_ELF, MSG_ORIG(MSG_ELF_FSIZE), ofl->ofl_name);
		return (S_ERROR);
	}

	/*
	 * Allocate and initialize the Is_desc structure.
	 */
	if ((isec = (Is_desc *)libld_calloc(1, sizeof (Is_desc))) ==
	    (Is_desc *)0)
		return (S_ERROR);
	isec->is_name = MSG_ORIG(MSG_SCN_SYMTAB);
	isec->is_shdr = shdr;
	isec->is_indata = data;

	/*
	 * Place the section first since it will affect the local symbol
	 * count.
	 */
	if ((ofl->ofl_ossymtab = place_section(ofl, isec, M_ID_SYMTAB, 0)) ==
	    (Os_desc *)S_ERROR)
		return (S_ERROR);

	/*
	 * Calculated number of symbols, which need to be augmented by
	 * the null first entry, the FILE symbol, and the .shstrtab entry.
	 */
	size = (size_t)(3 + ofl->ofl_shdrcnt + ofl->ofl_scopecnt +
		ofl->ofl_locscnt + ofl->ofl_globcnt) * shdr->sh_entsize;

	/*
	 * Finalize the section header and data buffer initialization.
	 */
	data->d_size = size;
	shdr->sh_size = size;

	return (1);
}


/*
 * Build a dynamic symbol table.  Contains only globals symbols and resides
 * in the text segment of a dynamic executable or shared library.
 */
Xword
make_dynsym(Ofl_desc * ofl)
{
	Shdr *		shdr;
	Elf_Data *	data;
	Is_desc *	isec;
	size_t		size;
	Xword		cnt;
	Word		flags = ofl->ofl_flags;

	/*
	 * Allocate and initialize the Elf_Data structure.
	 */
	if ((data = (Elf_Data *)libld_calloc(sizeof (Elf_Data), 1)) == 0)
		return (S_ERROR);
	data->d_type = ELF_T_SYM;
	data->d_align = M_WORD_ALIGN;
	data->d_version = ofl->ofl_libver;

	/*
	 * Allocate and initialize the Shdr structure.
	 */
	if ((shdr = (Shdr *)libld_calloc(sizeof (Shdr), 1)) == 0)
		return (S_ERROR);
	shdr->sh_type = SHT_DYNSYM;
	shdr->sh_flags = SHF_ALLOC;
	shdr->sh_addralign = M_WORD_ALIGN;
	shdr->sh_entsize = elf_fsize(ELF_T_SYM, 1, ofl->ofl_libver);
	if (shdr->sh_entsize == 0) {
		eprintf(ERR_ELF, MSG_ORIG(MSG_ELF_FSIZE), ofl->ofl_name);
		return (S_ERROR);
	}

	/*
	 * Allocate and initialize the Is_desc structure.
	 */
	if ((isec = (Is_desc *)libld_calloc(1, sizeof (Is_desc))) ==
	    (Is_desc *)0)
		return (S_ERROR);
	isec->is_name = MSG_ORIG(MSG_SCN_DYNSYM);
	isec->is_shdr = shdr;
	isec->is_indata = data;

	/*
	 * Place the section first since it will affect the local symbol
	 * count.
	 */
	if ((ofl->ofl_osdynsym = place_section(ofl, isec, M_ID_DYNSYM, 0)) ==
	    (Os_desc *)S_ERROR)
		return (S_ERROR);

	/*
	 * We need to account for additional section entries that have not
	 * been processed (refer to calling order in main()):
	 *
	 *	i.	initial null entry and .shstrtab entry
	 *	ii.	.strtab and .symtab entries (if not stripped)
	 */
	cnt = 2 + ofl->ofl_shdrcnt + ofl->ofl_globcnt;
	if (!(flags & FLG_OF_STRIP) || (flags & FLG_OF_RELOBJ))
		cnt += 2;
	size = (size_t)cnt * shdr->sh_entsize;

	/*
	 * Finalize the section header and data buffer initialization.
	 */
	data->d_size = size;
	shdr->sh_size = size;

	return (1);
}


/*
 * Build a string table for the section headers.
 */
Xword
make_shstrtab(Ofl_desc * ofl)
{
	Shdr *		shdr;
	Elf_Data *	data;
	Is_desc *	isec;
	size_t		size;

	/*
	 * Allocate the Elf_Data structure.
	 */
	if ((data = (Elf_Data *)libld_calloc(sizeof (Elf_Data), 1)) == 0)
		return (S_ERROR);
	data->d_type = ELF_T_BYTE;
	data->d_align = 1;
	data->d_version = ofl->ofl_libver;

	/*
	 * Allocate the Shdr structure.
	 */
	if ((shdr = (Shdr *)libld_calloc(sizeof (Shdr), 1)) == 0)
		return (S_ERROR);
	shdr->sh_type = SHT_STRTAB;
	shdr->sh_addralign = 1;

	/*
	 * Allocate the Is_desc structure.
	 */
	if ((isec = (Is_desc *)libld_calloc(1, sizeof (Is_desc))) ==
	    (Is_desc *)0)
		return (S_ERROR);
	isec->is_name = MSG_ORIG(MSG_SCN_SHSTRTAB);
	isec->is_shdr = shdr;
	isec->is_indata = data;

	/*
	 * Place the section first, as it may effect the number of section
	 * headers to account for.
	 */
	if ((ofl->ofl_osshstrtab = place_section(ofl, isec, M_ID_NOTE, 0)) ==
	    (Os_desc *)S_ERROR)
		return (S_ERROR);

	/*
	 * Account for the null byte at the beginning of the section and
	 * finalize the section header and data buffer initialization.
	 */
	size = 1 + (size_t)ofl->ofl_shdrstrsz;

	data->d_size = size;
	shdr->sh_size = size;

	return (1);
}

/*
 * Build a string section for the standard symbol table.
 */
Xword
make_strtab(Ofl_desc * ofl)
{
	Shdr *		shdr;
	Elf_Data *	data;
	Is_desc *	isec;
	size_t		size;

	/*
	 * This string table consists of all the global and local symbols.
	 * Account for null bytes at end of the file name and the beginning
	 * of section.
	 */
	size = (size_t) 2 + ofl->ofl_globstrsz + ofl->ofl_locsstrsz +
		strlen(ofl->ofl_name);

	/*
	 * Allocate the Elf_Data structure.
	 */
	if ((data = (Elf_Data *)libld_calloc(sizeof (Elf_Data), 1)) == 0)
		return (S_ERROR);
	data->d_size = size;
	data->d_type = ELF_T_BYTE;
	data->d_align = 1;
	data->d_version = ofl->ofl_libver;

	/*
	 * Allocate the Shdr structure.
	 */
	if ((shdr = (Shdr *)libld_calloc(sizeof (Shdr), 1)) == 0)
		return (S_ERROR);
	shdr->sh_size = size;
	shdr->sh_addralign = 1;
	shdr->sh_type = SHT_STRTAB;

	/*
	 * Allocate and initialize the Is_desc structure.
	 */
	if ((isec = (Is_desc *)libld_calloc(1, sizeof (Is_desc))) ==
	    (Is_desc *)0)
		return (S_ERROR);
	isec->is_name = MSG_ORIG(MSG_SCN_STRTAB);
	isec->is_shdr = shdr;
	isec->is_indata = data;

	ofl->ofl_osstrtab = place_section(ofl, isec, M_ID_STRTAB, 0);
	return ((Xword)ofl->ofl_osstrtab);
}

/*
 * Build a string table for the dynamic symbol table.
 */
Xword
make_dynstr(Ofl_desc * ofl)
{
	Shdr *		shdr;
	Elf_Data *	data;
	Is_desc *	isec;
	size_t		size;

	/*
	 * Account for the null byte at beginning of the section and any
	 * strings referenced by the .dynamic entries.
	 */
	size = 1 + (size_t)ofl->ofl_dynstrsz;
	/*
	 * Account for the symbol names if we are building
	 * a .dynsym section as well.
	 */
	if (!(ofl->ofl_flags & FLG_OF_RELOBJ))
		size += ofl->ofl_globstrsz;

	/*
	 * Account for the symbol names if we are building a
	 * .dynsym section as well
	 */
	if (!(ofl->ofl_flags & FLG_OF_RELOBJ))
		size += ofl->ofl_globstrsz;

	/*
	 * Allocate the Elf_Data structure.
	 */
	if ((data = (Elf_Data *)libld_calloc(sizeof (Elf_Data), 1)) == 0)
		return (S_ERROR);
	data->d_type = ELF_T_BYTE;
	data->d_size = size;
	data->d_align = 1;
	data->d_version = ofl->ofl_libver;

	/*
	 * Allocate the Shdr structure.
	 */
	if ((shdr = (Shdr *)libld_calloc(sizeof (Shdr), 1)) == 0)
		return (S_ERROR);
	if (!(ofl->ofl_flags & FLG_OF_RELOBJ))
		shdr->sh_flags = SHF_ALLOC;

	shdr->sh_type = SHT_STRTAB;
	shdr->sh_size = size;
	shdr->sh_addralign = 1;

	/*
	 * Allocate and initialize the Is_desc structure.
	 */
	if ((isec = (Is_desc *)libld_calloc(1, sizeof (Is_desc))) ==
	    (Is_desc *)0)
		return (S_ERROR);
	isec->is_name = MSG_ORIG(MSG_SCN_DYNSTR);
	isec->is_shdr = shdr;
	isec->is_indata = data;

	ofl->ofl_osdynstr = place_section(ofl, isec, M_ID_DYNSTR, 0);
	return ((Xword)ofl->ofl_osdynstr);
}

/*
 * Generate an output relocation section which will contain the relocation
 * information to be applied to the `osp' section.
 *
 * If (osp == NULL) then we are creating the coalesced relocation section
 * for an executable and/or a shared object.
 */
Xword
make_reloc(Ofl_desc * ofl, Os_desc * osp)
{
	Shdr *		shdr;
	Elf_Data *	data;
	Is_desc *	isec;
	size_t		size;
	Xword		sh_flags;
	char *		sectname;
	Os_desc *	rosp;

	if (osp) {
		size = osp->os_szoutrels;
		sh_flags = osp->os_shdr->sh_flags;
		if ((sectname = (char *)libld_malloc(strlen(rel_prefix) +
		    strlen(osp->os_name) + 1)) == 0)
			return (S_ERROR);
		(void) strcpy(sectname, rel_prefix);
		(void) strcat(sectname, osp->os_name);
	} else {
		size = ofl->ofl_reloccnt * sizeof (Rel);
		sh_flags = SHF_ALLOC;
		sectname = (char *)MSG_ORIG(MSG_SCN_RELOC);
	}

	/*
	 * Allocate and initialize the Elf_Data structure.
	 */
	if ((data = (Elf_Data *)libld_calloc(sizeof (Elf_Data), 1)) == 0)
		return (S_ERROR);
	data->d_type = M_REL_ELF_TYPE;
	data->d_size = size;
	data->d_align = M_WORD_ALIGN;
	data->d_version = ofl->ofl_libver;

	/*
	 * Allocate and initialize the Shdr structure.
	 */
	if ((shdr = (Shdr *)libld_calloc(sizeof (Shdr), 1)) == 0)
		return (S_ERROR);
	shdr->sh_type = M_REL_SHT_TYPE;
	shdr->sh_size = size;
	shdr->sh_addralign = M_WORD_ALIGN;
	shdr->sh_entsize = elf_fsize(M_REL_ELF_TYPE, 1, ofl->ofl_libver);
	if (shdr->sh_entsize == 0) {
		eprintf(ERR_ELF, MSG_ORIG(MSG_ELF_FSIZE), ofl->ofl_name);
		return (S_ERROR);
	}
	if ((ofl->ofl_flags & FLG_OF_DYNAMIC) &&
	    !(ofl->ofl_flags & FLG_OF_RELOBJ) &&
	    (sh_flags & SHF_ALLOC))
		shdr->sh_flags = SHF_ALLOC;

	/*
	 * Allocate and initialize the Is_desc structure.
	 */
	if ((isec = (Is_desc *)libld_calloc(1, sizeof (Is_desc))) ==
	    (Is_desc *)0)
		return (S_ERROR);
	isec->is_shdr = shdr;
	isec->is_indata = data;
	isec->is_name = sectname;


	/*
	 * Associate this relocation section to the section its going to
	 * relocate.
	 */
	if ((rosp = place_section(ofl, isec, M_ID_REL, 0)) ==
	    (Os_desc *)S_ERROR)
		return (S_ERROR);

	if (osp)
		osp->os_relosdesc = rosp;

	/*
	 * If this is the first relocation section we've encountered save it
	 * so that the .dynamic entry can be initialized accordingly.
	 */
	if (ofl->ofl_osreloc == (Os_desc *)0)
		ofl->ofl_osreloc = rosp;

	return (1);
}

/*
 * Generate version needed section.
 */
Xword
make_verneed(Ofl_desc * ofl)
{
	Shdr *		shdr;
	Elf_Data *	data;
	Is_desc *	isec;
	size_t		size = (size_t)ofl->ofl_verneedsz;

	/*
	 * Allocate and initialize the Elf_Data structure.
	 */
	if ((data = (Elf_Data *)libld_calloc(sizeof (Elf_Data), 1)) == 0)
		return (S_ERROR);
	data->d_type = ELF_T_BYTE;
	data->d_size = size;
	data->d_align = M_WORD_ALIGN;
	data->d_version = ofl->ofl_libver;

	/*
	 * Allocate and initialize the Shdr structure.
	 */
	if ((shdr = (Shdr *)libld_calloc(sizeof (Shdr), 1)) == 0)
		return (S_ERROR);
	shdr->sh_type = (Word)SHT_SUNW_verneed;
	shdr->sh_flags = SHF_ALLOC;
	shdr->sh_size = size;
	shdr->sh_addralign = M_WORD_ALIGN;

	/*
	 * Allocate and initialize the Is_desc structure.
	 */
	if ((isec = (Is_desc *)libld_calloc(1, sizeof (Is_desc))) ==
	    (Is_desc *)0)
		return (S_ERROR);
	isec->is_name = MSG_ORIG(MSG_SCN_VERSION);
	isec->is_shdr = shdr;
	isec->is_indata = data;

	ofl->ofl_osverneed = place_section(ofl, isec, M_ID_VERSION, 0);
	return ((Xword)ofl->ofl_osverneed);
}

/*
 * Generate a version definition section.
 *
 *  o	the SHT_SUNW_verdef section defines the versions that exist within this
 *	image.
 */
Xword
make_verdef(Ofl_desc * ofl)
{
	Shdr *		shdr;
	Elf_Data *	data;
	Is_desc *	isec;
	Ver_desc *	vdp;
	size_t		size;

	/*
	 * Reserve a string table entry for the base version dependency (other
	 * dependencies have symbol representations, which will already be
	 * accounted for during symbol processing).
	 */
	vdp = (Ver_desc *)ofl->ofl_verdesc.head->data;
	size = strlen(vdp->vd_name) + 1;

	if (ofl->ofl_flags & FLG_OF_DYNAMIC)
		ofl->ofl_dynstrsz += size;
	else
		ofl->ofl_locsstrsz += size;

	/*
	 * During version processing we calculated the total number of entries.
	 * Allocate and initialize the Elf_Data structure.
	 */
	size = ofl->ofl_verdefsz;

	if ((data = (Elf_Data *)libld_calloc(sizeof (Elf_Data), 1)) == 0)
		return (S_ERROR);
	data->d_type = ELF_T_BYTE;
	data->d_size = size;
	data->d_align = M_WORD_ALIGN;
	data->d_version = ofl->ofl_libver;

	/*
	 * Allocate and initialize the Shdr structure.
	 */
	if ((shdr = (Shdr *)libld_calloc(sizeof (Shdr), 1)) == 0)
		return (S_ERROR);
	shdr->sh_type = (Word)SHT_SUNW_verdef;
	shdr->sh_flags = SHF_ALLOC;
	shdr->sh_size = size;
	shdr->sh_addralign = M_WORD_ALIGN;

	/*
	 * Allocate and initialize the Is_desc structure.
	 */
	if ((isec = (Is_desc *)libld_calloc(1, sizeof (Is_desc))) ==
	    (Is_desc *)0)
		return (S_ERROR);
	isec->is_name = MSG_ORIG(MSG_SCN_VERSION);
	isec->is_shdr = shdr;
	isec->is_indata = data;

	ofl->ofl_osverdef = place_section(ofl, isec, M_ID_VERSION, 0);
	return ((Xword)ofl->ofl_osverdef);
}

/*
 * Common function used to build both the SHT_SUNW_versym
 * section and the SHT_SUNW_syminfo section.  Each of these sections
 * provides additional symbol information.
 */
Os_desc *
make_sym_sec(Ofl_desc * ofl, char * sectname, Word entsize,
	Word stype, int ident)
{
	Shdr *		shdr;
	Elf_Data *	data;
	Is_desc *	isec;

	/*
	 * Allocate and initialize the Elf_Data structures for the symbol index
	 * array.
	 */
	if ((data = (Elf_Data *)libld_calloc(sizeof (Elf_Data), 1)) == 0)
		return ((Os_desc *)S_ERROR);
	data->d_type = ELF_T_BYTE;
	data->d_align = M_WORD_ALIGN;
	data->d_version = ofl->ofl_libver;

	/*
	 * Allocate and initialize the Shdr structure.
	 */
	if ((shdr = (Shdr *)libld_calloc(sizeof (Shdr), 1)) == 0)
		return ((Os_desc *)S_ERROR);
	shdr->sh_type = (Word)stype;
	shdr->sh_flags = SHF_ALLOC;
	shdr->sh_addralign = M_WORD_ALIGN;
	shdr->sh_entsize = entsize;

	/*
	 * Allocate and initialize the Is_desc structure.
	 */
	if ((isec = (Is_desc *)libld_calloc(1, sizeof (Is_desc))) ==
	    (Is_desc *)0)
		return ((Os_desc *)S_ERROR);
	isec->is_name = sectname;
	isec->is_shdr = shdr;
	isec->is_indata = data;

	return (place_section(ofl, isec, ident, 0));
}

/*
 * The following sections are built after all input file processing and symbol
 * validation has been carried out.  The order is important (because the
 * addition of a section adds a new symbol there is a chicken and egg problem
 * of maintaining the appropriate counts).  By maintaining a known order the
 * individual routines can compensate for later, known, additions.
 */
Xword
make_sections(Ofl_desc * ofl)
{
	Word		flags = ofl->ofl_flags;

	/*
	 * Generate any special sections.
	 */
	if (flags & FLG_OF_ADDVERS)
		if (make_comment(ofl) == S_ERROR)
			return (S_ERROR);
	if (make_interp(ofl) == S_ERROR)
		return (S_ERROR);

	/*
	 * Make the .plt section.  This occurs after any other relocation
	 * sections are generated (see reloc_init()) to ensure that the
	 * associated relocation section is after all the other relocation
	 * sections.
	 */
	if (ofl->ofl_pltcnt != M_PLT_XNumber)
		if (make_plt(ofl) == S_ERROR)
			return (S_ERROR);

	/*
	 * Add any necessary versioning information.
	 */
	if ((flags & (FLG_OF_VERNEED | FLG_OF_NOVERSEC)) == FLG_OF_VERNEED) {
		if (make_verneed(ofl) == S_ERROR)
			return (S_ERROR);
	}
	if ((flags & (FLG_OF_VERDEF | FLG_OF_NOVERSEC)) == FLG_OF_VERDEF) {
		if (make_verdef(ofl) == S_ERROR)
			return (S_ERROR);
		if ((ofl->ofl_osversym = make_sym_sec(ofl, ".SUNW_versym",
		    sizeof (Versym), SHT_SUNW_versym, M_ID_VERSION)) ==
		    (Os_desc*)S_ERROR)
			return (S_ERROR);
	}

	/*
	 * The setting of ofl_ossyminfo to a 'non-zero' value
	 * signals that a SUNW_syminfo section should be built
	 * and information recorded.
	 */
	if ((ofl->ofl_dtflags & (DF_1_DIRECT | DF_1_TRANS)) ||
	    ofl->ofl_ossyminfo) {
		if ((ofl->ofl_ossyminfo = make_sym_sec(ofl, ".SUNW_syminfo",
		    sizeof (Syminfo), SHT_SUNW_syminfo,
		    M_ID_SYMINFO)) == (Os_desc *)S_ERROR)
			return (S_ERROR);
	}

	/*
	 * Finally build the symbol and section header sections.
	 */
	if (flags & FLG_OF_DYNAMIC) {
		if (make_dynamic(ofl) == S_ERROR)
			return (S_ERROR);
		if (make_dynstr(ofl) == S_ERROR)
			return (S_ERROR);
		/*
		 * There is no use for the .hash and .dynsym
		 * sections in a relocatable object.
		 */
		if (!(flags & FLG_OF_RELOBJ)) {
			if (make_hash(ofl) == S_ERROR)
				return (S_ERROR);
			if (make_dynsym(ofl) == S_ERROR)
				return (S_ERROR);
		}
	}
	if (!(flags & FLG_OF_STRIP) || (flags & FLG_OF_RELOBJ)) {
		if (make_strtab(ofl) == S_ERROR)
			return (S_ERROR);
		if (make_symtab(ofl) == S_ERROR)
			return (S_ERROR);
	}

	if (make_shstrtab(ofl) == S_ERROR)
		return (S_ERROR);

	/*
	 * Now that we've created all of our sections adjust the size
	 * of SHT_SUNW_versym & SHT_SUNW_syminfo which are dependent on
	 * the symbol table sizes.
	 */
	if (ofl->ofl_osversym || ofl->ofl_ossyminfo) {
		Shdr *		shdr;
		Is_desc *	isec;
		Elf_Data *	data;
		size_t		size;
		ulong_t		cnt;

		if (flags & FLG_OF_RELOBJ)
			isec = (Is_desc *)ofl->ofl_ossymtab->
				os_isdescs.head->data;
		else
			isec = (Is_desc *)ofl->ofl_osdynsym->
				os_isdescs.head->data;
		cnt = isec->is_shdr->sh_size / isec->is_shdr->sh_entsize;

		if (ofl->ofl_osversym) {
			isec = (Is_desc *)ofl->ofl_osversym->os_isdescs.
				head->data;
			data = isec->is_indata;
			shdr = ofl->ofl_osversym->os_shdr;
			size = cnt * shdr->sh_entsize;
			shdr->sh_size = size;
			data->d_size = size;
		}
		if (ofl->ofl_ossyminfo) {
			isec = (Is_desc *)ofl->ofl_ossyminfo->os_isdescs.
				head->data;
			data = isec->is_indata;
			shdr = ofl->ofl_ossyminfo->os_shdr;
			size = cnt * shdr->sh_entsize;
			shdr->sh_size = size;
			data->d_size = size;
		}
	}

	return (1);
}
