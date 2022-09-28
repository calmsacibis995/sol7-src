/*
 *	Copyright (c) 1988 AT&T
 *	  All Rights Reserved
 *
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
 *	The copyright notice above does not evidence any
 *	actual or intended publication of such source code.
 *
 *	Copyright (c) 1996 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)machrel.c	1.4	97/09/22 SMI"

/* LINTLIBRARY */

#include	<string.h>
#include	<assert.h>
#include	<sys/elf_SPARC.h>
#include	"debug.h"
#include	"reloc.h"
#include	"msg.h"
#include	"_libld.h"

/*
 *	Local Variable Definitions
 */
static Xword negative_got_offset = 0;
				/* offset of GOT table from GOT symbol */
static Word countSmallGOT = M_GOT_XNumber;
				/* number of small GOT symbols */
Xword	orels;			/* counter for output relocations */


/*
 *	Build a single V9 P.L.T. entry - code is:
 *
 *	For Target Addresses +/- 4GB of the entry
 *	-----------------------------------------
 *	sethi	(. - .PLT0), %g1
 *	ba,a	%xcc, .PLT1
 *	nop
 *	nop
 *	nop
 *	nop
 *	nop
 *	nop
 *
 *	For Target Addresses +/- 2GB of the entry
 *	-----------------------------------------
 *
 *	.PLT0 is the address of the first entry in the P.L.T.
 *	This one is filled in by the run-time link editor. We just
 *	have to leave space for it.
 */
void
plt_entry(Ofl_desc * ofl, Xword pltndx)
{
	unsigned char *	pltent;	/* PLT entry being created. */
	Sxword		pltoff;	/* Offset of this entry from PLT top */


	/*
	 *  XX64:  The second part of the V9 ABI (sec. 5.2.4)
	 *  applies to plt entries greater than 0x8000 (32,768).
	 *  This isn't implemented yet.
	 */
	assert(pltndx < 0x8000);


	pltoff = pltndx * M_PLT_ENTSIZE;
	pltent = (unsigned char *)ofl->ofl_osplt->os_outdata->d_buf + pltoff;

	/*
	 * PLT[0]: sethi %hi(. - .L0), %g1
	 */
	/* LINTED */
	*(Word *)pltent = M_SETHIG1 | pltoff;

	/*
	 * PLT[1]: ba,a %xcc, .PLT1 (.PLT1 accessed as a PC-relative index
	 * of longwords).
	 */
	pltent += M_PLT_INSSIZE;
	pltoff += M_PLT_INSSIZE;
	pltoff = -pltoff;
	/* LINTED */
	*(Word *)pltent = M_BA_A_XCC | (((pltoff + M_PLT_ENTSIZE) >> 2) &
				S_MASK(19));

	/*
	 * PLT[2]: sethi 0, %g0 (NOP for delay slot of eventual CTI).
	 */
	pltent += M_PLT_INSSIZE;
	/* LINTED */
	*(Word *)pltent = M_NOP;

	/*
	 * PLT[3]: sethi 0, %g0 (NOP for PLT padding).
	 */
	pltent += M_PLT_INSSIZE;
	/* LINTED */
	*(Word *)pltent = M_NOP;

	/*
	 * PLT[4]: sethi 0, %g0 (NOP for PLT padding).
	 */
	pltent += M_PLT_INSSIZE;
	/* LINTED */
	*(Word *)pltent = M_NOP;

	/*
	 * PLT[5]: sethi 0, %g0 (NOP for PLT padding).
	 */
	pltent += M_PLT_INSSIZE;
	/* LINTED */
	*(Word *)pltent = M_NOP;

	/*
	 * PLT[6]: sethi 0, %g0 (NOP for PLT padding).
	 */
	pltent += M_PLT_INSSIZE;
	/* LINTED */
	*(Word *)pltent = M_NOP;

	/*
	 * PLT[7]: sethi 0, %g0 (NOP for PLT padding).
	 */
	pltent += M_PLT_INSSIZE;
	/* LINTED */
	*(Word *)pltent = M_NOP;
}


Xword
perform_outreloc(Rel_desc * orsp, Ofl_desc * ofl)
{
	Os_desc *		osp;		/* output section */
	Os_desc *		relosp;		/* reloc output section */
	Xword			ndx;		/* sym & scn index */
	Xword			roffset;	/* roffset for output rel */
	Xword			value;
	Sxword			raddend;	/* raddend for output rel */
	Rel			rea;		/* SHT_RELA entry. */
	char *			relbits;
	Sym_desc *		sdp;		/* current relocation sym */
	const Rel_entry *	rep;

	raddend = orsp->rel_raddend;
	sdp = orsp->rel_sym;
	/*
	 * If this is a relocation against a section then we
	 * need to adjust the raddend field to compensate
	 * for the new position of the input section within
	 * the new output section.
	 */
	if (ELF_ST_TYPE(sdp->sd_sym->st_info) == STT_SECTION) {
		raddend += sdp->sd_isc->is_indata->d_off;
		if (sdp->sd_isc->is_shdr->sh_flags & SHF_ALLOC)
			raddend += sdp->sd_isc->is_osdesc->os_shdr->sh_addr;
	}

	value = sdp->sd_sym->st_value;

	if (orsp->rel_flags & FLG_REL_GOT) {
		osp = ofl->ofl_osgot;
		roffset = (Xword) (osp->os_shdr->sh_addr) +
		    (-negative_got_offset * M_GOT_ENTSIZE) + (sdp->sd_GOTndx *
		    M_GOT_ENTSIZE);
	} else if (orsp->rel_flags & FLG_REL_PLT) {
		osp = ofl->ofl_osplt;
		roffset = (Xword) (osp->os_shdr->sh_addr) +
		    (sdp->sd_aux->sa_PLTndx * M_PLT_ENTSIZE);
		plt_entry(ofl, sdp->sd_aux->sa_PLTndx);
	} else if (orsp->rel_flags & FLG_REL_BSS) {
		/*
		 * this must be a R_SPARC_COPY - for those
		 * we also set the roffset to point to the
		 * new symbols location.
		 *
		 */
		osp = ofl->ofl_isbss->is_osdesc;
		roffset = (Xword)value;
		/*
		 * the raddend doesn't mean anything in an
		 * R_SPARC_COPY relocation.  We will null
		 * it out because it can be confusing to
		 * people.
		 */
		raddend = 0;
	} else {
		osp = orsp->rel_osdesc;
		/*
		 * Calculate virtual offset of reference point;
		 * equals offset into section + vaddr of section
		 * for loadable sections, or offset plus
		 * section displacement for nonloadable
		 * sections.
		 */
		roffset = orsp->rel_roffset +
		    orsp->rel_isdesc->is_indata->d_off;
		if (!(ofl->ofl_flags & FLG_OF_RELOBJ))
			roffset += orsp->rel_isdesc->is_osdesc->
			    os_shdr->sh_addr;
	}

	if ((relosp = osp->os_relosdesc) == 0)
		relosp = ofl->ofl_osreloc;


	/*
	 * Verify that the output relocations offset meets the
	 * alignment requirements of the relocation being processed.
	 */
	rep = &reloc_table[orsp->rel_rtype];
	if ((rep->re_flags & FLG_RE_UNALIGN) == 0) {
		if (((rep->re_fsize == 2) && (roffset & 0x1)) ||
		    ((rep->re_fsize == 4) && (roffset & 0x3)) ||
		    ((rep->re_fsize == 8) && (roffset & 0x7))) {
			eprintf(ERR_FATAL, MSG_INTL(MSG_REL_NONALIGN),
			    conv_reloc_SPARC_type_str(orsp->rel_rtype),
			    orsp->rel_fname, orsp->rel_sname,
			    EC_XWORD(roffset));
			return (S_ERROR);
		}
	}


	/*
	 * assign the symbols index for the output
	 * relocation.  If the relocation refers to a
	 * SECTION symbol then it's index is based upon
	 * the output sections symbols index.  Otherwise
	 * the index can be derived from the symbols index
	 * itself.
	 */
	if (orsp->rel_rtype == R_SPARC_RELATIVE)
		ndx = STN_UNDEF;
	else if ((orsp->rel_flags & FLG_REL_SCNNDX) ||
	    (ELF_ST_TYPE(sdp->sd_sym->st_info) == STT_SECTION))
		ndx = sdp->sd_isc->is_osdesc->os_scnsymndx;
	else
		ndx = sdp->sd_symndx;

	/*
	 * Add the symbols 'value' to the addend field.
	 */
	if (orsp->rel_flags & FLG_REL_ADVAL)
		raddend += value;

	relbits = (char *)relosp->os_outdata->d_buf;

	rea.r_info = ELF_R_INFO(ndx, ELF_R_TYPE_INFO(orsp->rel_rextoffset,
			orsp->rel_rtype));
	rea.r_offset = roffset;
	rea.r_addend = raddend;
	DBG_CALL(Dbg_reloc_out(M_MACH, M_REL_SHT_TYPE, &rea,
	    orsp->rel_sname, relosp->os_name));

	(void) memcpy((relbits + relosp->os_szoutrels),
		(char *)&rea, sizeof (Rel));
	relosp->os_szoutrels += sizeof (Rel);

	/*
	 * Determine whether this relocation is against a
	 * non-writeable, allocatable section.  If so we may
	 * need to provide a text relocation diagnostic.
	 */
	reloc_remain_entry(orsp, osp, ofl);

	return (1);
}


Xword
do_activerelocs(Ofl_desc *ofl)
{
	Rel_desc *	arsp;
	Rel_cache *	rcp;
	Listnode *	lnp;
	Xword		return_code = 1;


	DBG_CALL(Dbg_reloc_doactiverel());
	/*
	 * process active relocs
	 */
	for (LIST_TRAVERSE(&ofl->ofl_actrels, lnp, rcp)) {
		for (arsp = (Rel_desc *)(rcp + 1);
		    arsp < rcp->rc_free; arsp++) {
			unsigned char *	addr;
			Xword		value;
			Sym_desc *	sdp;
			Xword		refaddr = arsp->rel_roffset +
					    arsp->rel_isdesc->is_indata->d_off;
			sdp = arsp->rel_sym;

			if ((arsp->rel_flags & FLG_REL_CLVAL) ||
			    (arsp->rel_flags & FLG_REL_GOTCL))
				value = 0;
			else if (ELF_ST_TYPE(sdp->sd_sym->st_info) ==
			    STT_SECTION) {
				/*
				 * The value for a symbol pointing to a SECTION
				 * is based off of that sections position.
				 */
				value = sdp->sd_isc->is_indata->d_off;
				if (sdp->sd_isc->is_shdr->sh_flags & SHF_ALLOC)
					value += sdp->sd_isc->is_osdesc->
					    os_shdr->sh_addr;
			} else
				/*
				 * else the value is the symbols value
				 */
				value = sdp->sd_sym->st_value;

			/*
			 * relocation against the GLOBAL_OFFSET_TABLE
			 */
			if (arsp->rel_flags & FLG_REL_GOT)
				arsp->rel_osdesc = ofl->ofl_osgot;

			/*
			 * If loadable and not producing a relocatable object
			 * add the sections virtual address to the reference
			 * address.
			 */
			if ((arsp->rel_flags & FLG_REL_LOAD) &&
			    !(ofl->ofl_flags & FLG_OF_RELOBJ))
				refaddr += arsp->rel_isdesc->is_osdesc->
				    os_shdr->sh_addr;

			/*
			 * If this entry has a PLT assigned to it, it's
			 * value is actually the address of the PLT (and
			 * not the address of the function).
			 */
			if (IS_PLT(arsp->rel_rtype)) {
				if (sdp->sd_aux && sdp->sd_aux->sa_PLTndx)
					value = (Xword)(ofl->ofl_osplt->
					    os_shdr->sh_addr) +
					    (sdp->sd_aux->sa_PLTndx *
					    M_PLT_ENTSIZE);
			}

			if (arsp->rel_flags & FLG_REL_GOT) {
				Xword	R1addr;
				Xword	R2addr;
				/*
				 * Clear the GOT table entry, on SPARC
				 * we clear the entry and the 'value' if
				 * needed is stored in an output relocations
				 * addend.
				 */

				/*
				 * calculate offset into GOT at which to apply
				 * the relocation.
				 */
				R1addr = (Xword)((char *)(-negative_got_offset *
				    M_GOT_ENTSIZE) + (sdp->sd_GOTndx
				    * M_GOT_ENTSIZE));
				/*
				 * add the GOTs data's offset
				 */
				R2addr = R1addr + (Xword)
				    arsp->rel_osdesc->os_outdata->d_buf;

				DBG_CALL(Dbg_reloc_doact(M_MACH,
				    arsp->rel_rtype, R1addr, value,
				    arsp->rel_sname, arsp->rel_osdesc));

				/*
				 * and do it.
				 */
				*(Xword *)R2addr = value;
				continue;
			} else if (IS_PC_RELATIVE(arsp->rel_rtype)) {
				value -= refaddr;
			} else if (IS_GOT_RELATIVE(arsp->rel_rtype))
				value = sdp->sd_GOTndx * M_GOT_ENTSIZE;

			/*
			 * add relocations addend to value.
			 */
			value += arsp->rel_raddend;

			/*
			 * Make sure we have data to relocate.  Our compiler
			 * and assembler developers have been known
			 * to generate relocations against invalid sections
			 * (normally .bss), so for their benefit give
			 * them sufficient information to help analyze
			 * the problem.  End users should probably
			 * never see this.
			 */
			if (arsp->rel_isdesc->is_indata->d_buf == 0) {
				eprintf(ERR_FATAL, MSG_INTL(MSG_REL_EMPTYSEC),
				    conv_reloc_SPARC_type_str(arsp->rel_rtype),
				    arsp->rel_isdesc->is_file->ifl_name,
				    arsp->rel_sname, arsp->rel_isdesc->is_name);
				return (S_ERROR);
			}

			/*
			 * Get the address of the data item we need to modify.
			 */
			addr = (unsigned char *)arsp->rel_isdesc->
				is_indata->d_off + arsp->rel_roffset;

			DBG_CALL(Dbg_reloc_doact(M_MACH, arsp->rel_rtype,
			    (Xword)addr, value, arsp->rel_sname,
			    arsp->rel_osdesc));
			addr += (Xword)arsp->rel_osdesc->os_outdata->d_buf;

			if ((((Xword)addr -  (Xword)ofl->ofl_ehdr) >
			    ofl->ofl_size) || (arsp->rel_roffset >
			    arsp->rel_osdesc->os_shdr->sh_size)) {
				int	warn_level;
				if (((Xword)addr - (Xword)ofl->ofl_ehdr) >
				    ofl->ofl_size)
					warn_level = ERR_FATAL;
				else
					warn_level = ERR_WARNING;

				eprintf(warn_level,
					MSG_INTL(MSG_REL_INVALOFFSET),
					conv_reloc_SPARC_type_str(
					arsp->rel_rtype),
					arsp->rel_isdesc->is_file->ifl_name,
					arsp->rel_isdesc->is_name,
					arsp->rel_sname,
					(Xword)addr - (Xword)ofl->ofl_ehdr);

				if (warn_level == ERR_FATAL) {
					return_code = S_ERROR;
					continue;
				}
			}

			if (do_reloc(arsp->rel_rtype, addr, &value,
			    arsp->rel_rextoffset, arsp->rel_sname,
			    arsp->rel_isdesc->is_file->ifl_name) == 0)
				return_code = S_ERROR;
		}
	}
	return (return_code);
}


Xword
add_outrel(Half flags, Rel_desc * rsp, Rel * rloc, Ofl_desc * ofl)
{
	Rel_desc *	orsp;
	Rel_cache *	rcp;

	/*
	 * Static exectuables *do not* want any relocations against
	 * them.  Since our engine still creates relocations against
	 * a 'WEAK UNDEFINED' symbol in a static executable it's best
	 * to just disable them here instead of through out the relocation
	 * code.
	 */
	if ((ofl->ofl_flags & (FLG_OF_STATIC | FLG_OF_EXEC)) ==
	    (FLG_OF_STATIC | FLG_OF_EXEC))
		return (1);

	/*
	 * Because the R_SPARC_HIPLT22 & R_SPARC_LOPLT10 relocations
	 * are not relative they would not make any sense if they
	 * were created in a shared object - so emit the proper error
	 * message if that occurs.
	 */
	if ((ofl->ofl_flags & FLG_OF_SHAROBJ) && ((rsp->rel_rtype ==
	    R_SPARC_HIPLT22) || (rsp->rel_rtype == R_SPARC_LOPLT10))) {
		eprintf(ERR_FATAL, MSG_INTL(MSG_REL_UNRELREL),
			conv_reloc_SPARC_type_str(rsp->rel_rtype),
			rsp->rel_fname, rsp->rel_sname);
		return (S_ERROR);
	}

	/*
	 * If no relocation cache structures are available allocate
	 * a new one and link it into the cache list.
	 */
	if ((ofl->ofl_outrels.tail == 0) ||
	    ((rcp = (Rel_cache *)ofl->ofl_outrels.tail->data) == 0) ||
	    ((orsp = rcp->rc_free) == rcp->rc_end)) {
		if ((rcp = (Rel_cache *)libld_malloc(sizeof (Rel_cache) +
		    (sizeof (Rel_desc) * REL_OIDESCNO))) == 0)
			return (S_ERROR);
		rcp->rc_free = orsp = (Rel_desc *)(rcp + 1);
		rcp->rc_end = (Rel_desc *)((long)rcp->rc_free +
				(sizeof (Rel_desc) * REL_OIDESCNO));
		if (list_appendc(&ofl->ofl_outrels, rcp) ==
		    (Listnode *)S_ERROR)
			return (S_ERROR);
	}

	*orsp = *rsp;
	orsp->rel_flags |= flags;
	orsp->rel_raddend = rloc->r_addend;
	orsp->rel_roffset = rloc->r_offset;
	/* LINTED */
	orsp->rel_rextoffset = (Word)ELF_R_TYPE_DATA(rloc->r_info);
	rcp->rc_free++;

	if (flags & FLG_REL_GOT)
		ofl->ofl_relocgotsz += sizeof (Rel);
	else if (flags & FLG_REL_PLT)
		ofl->ofl_relocpltsz += sizeof (Rel);
	else if (flags & FLG_REL_BSS)
		ofl->ofl_relocbsssz += sizeof (Rel);
	else
		rsp->rel_osdesc->os_szoutrels += sizeof (Rel);

	if (orsp->rel_rtype == M_R_RELATIVE)
		ofl->ofl_relocrelcnt++;

	/*
	 * We don't perform sorting on PLT relocations because
	 * they have already been assigned a PLT index and if we
	 * were to sort them we would have to re-assign the plt indexes.
	 */
	if (!(flags & FLG_REL_PLT))
		ofl->ofl_reloccnt++;

	DBG_CALL(Dbg_reloc_ors_entry(M_MACH, orsp));
	return (1);
}


Xword
add_actrel(Half flags, Rel_desc * rsp, Rel * rloc, Ofl_desc * ofl)
{
	Rel_desc * 	arsp;
	Rel_cache *	rcp;

	/*
	 * If no relocation cache structures are available allocate a
	 * new one and link it into the bucket list.
	 */
	if ((ofl->ofl_actrels.tail == 0) ||
	    ((rcp = (Rel_cache *)ofl->ofl_actrels.tail->data) == 0) ||
	    ((arsp = rcp->rc_free) == rcp->rc_end)) {
		if ((rcp = (Rel_cache *)libld_malloc(sizeof (Rel_cache) +
			(sizeof (Rel_desc) * REL_AIDESCNO))) == 0)
				return (S_ERROR);
		rcp->rc_free = arsp = (Rel_desc *)(rcp + 1);
		rcp->rc_end = (Rel_desc *)((long)rcp->rc_free +
				(sizeof (Rel_desc) * REL_AIDESCNO));
		if (list_appendc(&ofl->ofl_actrels, rcp) ==
		    (Listnode *)S_ERROR)
			return (S_ERROR);
	}

	*arsp = *rsp;
	arsp->rel_flags |= flags;
	arsp->rel_raddend = rloc->r_addend;
	arsp->rel_roffset = rloc->r_offset;
	/* LINTED */
	arsp->rel_rextoffset = (Word)ELF_R_TYPE_DATA(rloc->r_info);
	rcp->rc_free++;

	DBG_CALL(Dbg_reloc_ars_entry(M_MACH, arsp));
	return (1);
}


/*
 * process relocation for a LOCAL symbol
 */
Xword
reloc_local(Rel_desc * rsp, Rel * reloc, Ofl_desc * ofl)
{
	Word		flags = ofl->ofl_flags;

	/*
	 * If ((shared object) and (not pc relative relocation))
	 * then
	 *	if (rtype != R_SPARC_32)
	 *	then
	 *		build relocation against section
	 *	else
	 *		build R_SPARC_RELATIVE
	 *	fi
	 * fi
	 */
	if ((flags & FLG_OF_SHAROBJ) && (rsp->rel_flags & FLG_REL_LOAD) &&
	    !(IS_PC_RELATIVE(rsp->rel_rtype))) {
		Word	ortype = rsp->rel_rtype;
		if ((rsp->rel_rtype != R_SPARC_32) &&
		    (rsp->rel_rtype != R_SPARC_PLT32) &&
		    (rsp->rel_rtype != R_SPARC_64))
			return (add_outrel(FLG_REL_SCNNDX |
			    FLG_REL_ADVAL, rsp, reloc, ofl));

		rsp->rel_rtype = R_SPARC_RELATIVE;
		if (add_outrel(FLG_REL_ADVAL, rsp, reloc, ofl) ==
		    S_ERROR)
			return (S_ERROR);
		rsp->rel_rtype = ortype;
		return (1);
	}

	if (!(rsp->rel_flags & FLG_REL_LOAD) &&
	    (rsp->rel_sym->sd_sym->st_shndx == SHN_UNDEF)) {
		(void) eprintf(ERR_WARNING, MSG_INTL(MSG_REL_EXTERNSYM),
		    conv_reloc_SPARC_type_str(rsp->rel_rtype), rsp->rel_fname,
		    rsp->rel_sname, rsp->rel_osdesc->os_name);
		return (1);
	}
	/*
	 * just do it.
	 */
	return (add_actrel(NULL, rsp, reloc, ofl));
}

Xword
reloc_relobj(Boolean local, Rel_desc * rsp, Rel * reloc, Ofl_desc * ofl)
{
	Word		rtype = rsp->rel_rtype;
	Sym_desc *	sdp = rsp->rel_sym;
	Is_desc *	isp = rsp->rel_isdesc;
	Word		flags = ofl->ofl_flags;

	/*
	 * Try to determine if we can do any relocations at
	 * this point.  We can if:
	 *
	 * (local_symbol) and (non_GOT_relocation) and
	 * (IS_PC_RELATIVE()) and
	 * (relocation to symbol in same section)
	 */
	if (local && !IS_GOT_RELATIVE(rtype) && IS_PC_RELATIVE(rtype) &&
	    ((sdp->sd_isc) && (sdp->sd_isc->is_osdesc == isp->is_osdesc)))
		return (add_actrel(NULL, rsp, reloc, ofl));

	/*
	 * If '-zredlocsym' is in effect make all local sym relocations
	 * against the 'section symbols', since they are the only symbols
	 * which will be added to the .symtab.
	 */
	if (local &&
	    (((ofl->ofl_flags1 & FLG_OF1_REDLSYM) &&
	    (ELF_ST_BIND(sdp->sd_sym->st_info) == STB_LOCAL)) ||
	    ((sdp->sd_flags & FLG_SY_ELIM) && (flags & FLG_OF_PROCRED)))) {
		return (add_outrel(FLG_REL_SCNNDX | FLG_REL_ADVAL,
			rsp, reloc, ofl));
	}

	return (add_outrel(NULL, rsp, reloc, ofl));
}


/*
 * allocate_got: if a GOT is to be made, after the section is built this
 * function is called to allocate all the GOT slots.  The allocation is
 * deferred until after all GOTs have been counted and sorted according
 * to their size, for only then will we know how to allocate them on
 * a processor like SPARC which has different models for addressing the
 * GOT.  SPARC has two: small and large, small uses a signed 13-bit offset
 * into the GOT, whereas large uses an unsigned 32-bit offset.
 */
static	Xword small_index;	/* starting index for small GOT entries */
static	Xword large_index;	/* starting index for large GOT entries */

Xword
assign_got(Sym_desc * sdp)
{
	switch (sdp->sd_GOTndx) {
	case M_GOT_SMALL:
		sdp->sd_GOTndx = small_index++;
		if (small_index == 0)
			small_index = M_GOT_XNumber;
		break;
	case M_GOT_LARGE:
		sdp->sd_GOTndx = large_index++;
		break;
	default:
		eprintf(ERR_FATAL, MSG_INTL(MSG_REL_ASSIGNGOT),
		    sdp->sd_GOTndx, sdp->sd_name);
		return (S_ERROR);
	}
	return (1);
}


Xword
assign_got_ndx(Word rtype, Xword prevndx, Sym_desc * sdp, Ofl_desc *ofl)
{
	/*
	 * If we'd previously assigned a M_GOT_LARGE index to a got index, it
	 * is possible that we will need to update it's index to an M_GOT_SMALL
	 * if we find a small GOT relocation against the same symbol.
	 *
	 * If we've already assigned an M_GOT_SMALL to the symbol no further
	 * action needs to be taken.
	 */
	if (prevndx == 0)
		ofl->ofl_gotcnt++;
	else if (prevndx == M_GOT_SMALL)
		return (M_GOT_SMALL);

	/*
	 * Because of the PIC vs. pic issue we can't assign the actual GOT
	 * index yet - instead we assign a token and track how many of each
	 * kind we have encountered.
	 *
	 * The actual index will be assigned during update_osym().
	 */
	if (rtype == R_SPARC_GOT13) {
		countSmallGOT++;
		sdp->sd_flags |= FLG_SY_SMGOT;
		return (M_GOT_SMALL);
	} else
		return (M_GOT_LARGE);
}


void
assign_plt_ndx(Sym_desc * sdp, Ofl_desc *ofl)
{
	sdp->sd_aux->sa_PLTndx = ofl->ofl_pltcnt++;
}


Xword
allocate_got(Ofl_desc * ofl)
{
	Sym_desc *	sdp;
	Addr		addr;

	/*
	 * Sanity check -- is this going to fit at all?
	 */
	if (countSmallGOT >= M_GOT_MAXSMALL) {
		eprintf(ERR_FATAL, MSG_INTL(MSG_REL_SMALLGOT), countSmallGOT,
		    M_GOT_MAXSMALL);
		return (S_ERROR);
	}

	/*
	 * Set starting offset to be either 0, or a negative index into
	 * the GOT based on the number of small symbols we've got.
	 */
	negative_got_offset = countSmallGOT > (M_GOT_MAXSMALL / 2) ?
	    -((Sxword)(countSmallGOT - (M_GOT_MAXSMALL / 2))) : 0;

	/*
	 * Initialize the large and small got offsets (used in assign_got()).
	 */
	small_index = negative_got_offset == 0 ?
	    M_GOT_XNumber : negative_got_offset;
	large_index = negative_got_offset + countSmallGOT;

	/*
	 * Assign bias to GOT symbols.
	 */
	addr = -negative_got_offset * M_GOT_ENTSIZE;
	if (sdp = sym_find(MSG_ORIG(MSG_SYM_GOFTBL), SYM_NOHASH, ofl))
		sdp->sd_sym->st_value = addr;
	if (sdp = sym_find(MSG_ORIG(MSG_SYM_GOFTBL_U), SYM_NOHASH, ofl))
		sdp->sd_sym->st_value = addr;
	return (1);
}


/*
 * Initializes .got[0] with the _DYNAMIC symbol value.
 */
void
fillin_gotplt1(Ofl_desc * ofl)
{
	Sym_desc *	sdp;

	if (ofl->ofl_osgot) {
		unsigned char *	genptr;

		if ((sdp = sym_find(MSG_ORIG(MSG_SYM_DYNAMIC_U),
		    SYM_NOHASH, ofl)) != NULL) {
			genptr = ((unsigned char *)
			    ofl->ofl_osgot->os_outdata->d_buf +
			    (-negative_got_offset * M_GOT_ENTSIZE) +
			    (M_GOT_XDYNAMIC * M_GOT_ENTSIZE));
			/* LINTED */
			*((Xword *)genptr) = sdp->sd_sym->st_value;
		}
	}
}


/*
 * Return plt[0].
 */
Addr
fillin_gotplt2(Ofl_desc * ofl)
{
	if (ofl->ofl_osplt)
		return (ofl->ofl_osplt->os_shdr->sh_addr);
	else
		return (0);
}
