/*
 *	Copyright (c) 1996 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)_relocate.c	1.3	98/01/06 SMI"

#include	"machdep.h"
#include	"reloc.h"
#include	"_rtld.h"


/*
 * Undo relocations that have been applied to a memory image.  Basically this
 * involves copying the original files relocation offset into the new image
 * being created.
 */
/* ARGSUSED3 */
void
undo_reloc(Rel * rel, unsigned char * oaddr, unsigned char * iaddr,
    Reloc * reloc)
{
	/* LINTED */
	unsigned long *	_oaddr = (unsigned long *)oaddr;
	/* LINTED */
	unsigned long *	_iaddr = (unsigned long *)iaddr;

	switch (ELF_R_TYPE(rel->r_info)) {
	case R_SPARC_NONE:
	case R_SPARC_COPY:
		break;

	case R_SPARC_JMP_SLOT:
		if (_iaddr) {
			*_oaddr++ = *_iaddr++;
			*_oaddr++ = *_iaddr++;
		} else {
			*_oaddr++ = 0;
			*_oaddr++ = 0;
		}
		/* FALLTHROUGH */

	default:
		if (_iaddr)
			*_oaddr = *_iaddr;
		else
			*_oaddr = 0;
		break;
	}
}


/*
 * Increment a relocation record.  The record must refect the new address to
 * which this image is fixed.
 */
/* ARGSUSED2 */
void
inc_reloc(Rel * rel, Reloc * reloc, unsigned char * oaddr,
    unsigned char * iaddr)
{
	rel->r_offset += reloc->r_value;
}


/*
 * Clear a relocation record.  The relocation has been applied to the image and
 * thus the relocation must not occur again.
 */
void
clear_reloc(Rel * rel)
{
	rel->r_offset = 0;
	rel->r_info = ELF_R_INFO(0, R_SPARC_NONE);
	rel->r_addend = 0;
}


/*
 * Apply a relocation to an image being built from an input file.  Use the
 * runtime linkers routines to do the necessary magic.
 */
/* ARGSUSED4 */
void
apply_reloc(Rel * rel, Reloc * reloc, const char * name, unsigned char * oaddr,
    Rt_map * lmp)
{
	/* LINTED */
	Byte	type = (Byte)ELF_R_TYPE(rel->r_info);
	Xword	value = reloc->r_value + rel->r_addend;

	if (type == R_SPARC_JMP_SLOT)
		/* LINTED */
		elf_plt_write((unsigned int *)oaddr, (unsigned long *)value);
	else {
		/* LINTED */
		Word extoff = (Word)ELF_R_TYPE_DATA(rel->r_info);
		(void) do_reloc(type, oaddr, &value, extoff, reloc->r_name,
		    name);
	}
}
