/*
 *	Copyright (c) 1996 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)phdr.c	1.7	97/09/22 SMI"

/* LINTLIBRARY */

#include	"msg.h"
#include	"_debug.h"


#if !(defined(_ELF64) && defined(lint))


/*
 * Print out a single `program header' entry.
 */
void
Elf_phdr_entry(Elf32_Phdr * phdr)
{
	dbg_print(MSG_ORIG(MSG_PHD_VADDR), EC_ADDR(phdr->p_vaddr),
	    conv_phdrflg_str(phdr->p_flags));
	dbg_print(MSG_ORIG(MSG_PHD_PADDR), EC_ADDR(phdr->p_paddr),
	    conv_phdrtyp_str(phdr->p_type));
	dbg_print(MSG_ORIG(MSG_PHD_FILESZ), EC_XWORD(phdr->p_filesz),
	    EC_XWORD(phdr->p_memsz));
	dbg_print(MSG_ORIG(MSG_PHD_OFFSET), EC_OFF(phdr->p_offset),
	    EC_XWORD(phdr->p_align));
}

void
Gelf_phdr_entry(GElf_Phdr * phdr)
{
	dbg_print(MSG_ORIG(MSG_PHD_VADDR), EC_ADDR(phdr->p_vaddr),
	    conv_phdrflg_str(phdr->p_flags));
	dbg_print(MSG_ORIG(MSG_PHD_PADDR), EC_ADDR(phdr->p_paddr),
	    conv_phdrtyp_str(phdr->p_type));
	dbg_print(MSG_ORIG(MSG_PHD_FILESZ), EC_XWORD(phdr->p_filesz),
	    EC_XWORD(phdr->p_memsz));
	dbg_print(MSG_ORIG(MSG_PHD_OFFSET), EC_OFF(phdr->p_offset),
	    EC_XWORD(phdr->p_align));
}

#endif /* !(defined(_ELF64) && defined(lint)) */
