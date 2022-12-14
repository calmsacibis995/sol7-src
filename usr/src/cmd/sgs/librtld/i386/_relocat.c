/*
 *	Copyright (c) 1996 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)_relocate.c	1.6	97/06/05 SMI"

#include	"machdep.h"
#include	"reloc.h"
#include	"_rtld.h"


/*
 * Undo relocations that have been applied to a memory image.  Basically this
 * involves copying the original files relocation offset into the new image
 * being created.  Note that .got enteries associated with .plt's must be
 * fixed to the new base address.
 */
void
undo_reloc(Rel * rel, unsigned char * oaddr, unsigned char * iaddr,
    Reloc * reloc)
{
	/* LINTED */
	unsigned long *	_oaddr = (unsigned long *)oaddr;
	/* LINTED */
	unsigned long *	_iaddr = (unsigned long *)iaddr;

	switch (ELF_R_TYPE(rel->r_info)) {
	case R_386_NONE:
	case R_386_COPY:
		break;

	case R_386_JMP_SLOT:
		if (_iaddr)
			*_oaddr = *_iaddr + reloc->r_value;
		else
			*_oaddr = reloc->r_value;
		break;

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
 * which this image is fixed.  Note that .got entries associated with .plt's
 * must be fixed to the new base address.
 */
void
inc_reloc(Rel * rel, Reloc * reloc, unsigned char * oaddr,
    unsigned char * iaddr)
{
	/* LINTED */
	unsigned long * _oaddr = (unsigned long *)oaddr;
	/* LINTED */
	unsigned long * _iaddr = (unsigned long *)iaddr;

	if (ELF_R_TYPE(rel->r_info) == R_386_JMP_SLOT) {
		if (_iaddr)
			*_oaddr = *_iaddr + reloc->r_value;
		else
			*_oaddr = reloc->r_value;
	}

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
	rel->r_info = ELF_R_INFO(0, R_386_NONE);
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
	unsigned char	type = ELF_R_TYPE(rel->r_info);
	Word		value = reloc->r_value;

	if (type == R_386_JMP_SLOT)
		/* LINTED */
		elf_plt_write((unsigned int *)oaddr, (unsigned long *)value);
	else
		(void) do_reloc(type, oaddr, &value, reloc->r_name, name);
}
