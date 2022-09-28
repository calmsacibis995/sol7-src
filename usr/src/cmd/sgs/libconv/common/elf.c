/*
 *	Copyright (c) 1996 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)elf.c	1.9	97/10/21 SMI"

/* LINTLIBRARY */

/*
 * String conversion routines for ELF header attributes.
 */
#include	<stdio.h>
#include	<string.h>
#include	"_conv.h"
#include	"elf_msg.h"
#include	<sys/elf_SPARC.h>

static const int classes[] = {
	MSG_ELFCLASSNONE,	MSG_ELFCLASS32,		MSG_ELFCLASS64
};

const char *
conv_eclass_str(Byte class)
{
	static char	string[STRSIZE] = { '\0' };

	if (class >= ELFCLASSNUM)
		return (conv_invalid_str(string, STRSIZE, (int)class, 0));
	else
		return (MSG_ORIG(classes[class]));

}

static const int datas[] = {
	MSG_ELFDATANONE,	MSG_ELFDATA2LSB, 	MSG_ELFDATA2MSB
};

const char *
conv_edata_str(Byte data)
{
	static char	string[STRSIZE] = { '\0' };

	if (data >= ELFDATANUM)
		return (conv_invalid_str(string, STRSIZE, (int)data, 0));
	else
		return (MSG_ORIG(datas[data]));

}

static const int machines[] = {
	MSG_EM_NONE,		MSG_EM_M32,		MSG_EM_SPARC,
	MSG_EM_386,		MSG_EM_68K,		MSG_EM_88K,
	MSG_EM_486,		MSG_EM_860,		MSG_EM_MIPS,
	MSG_EM_UNKNOWN9,	MSG_EM_MIPS_RS3_LE, 	MSG_EM_RS6000,
	MSG_EM_UNKNOWN12,	MSG_EM_UNKNOWN13,	MSG_EM_UNKNOWN14,
	MSG_EM_PA_RISC,		MSG_EM_nCUBE,		MSG_EM_VPP500,
	MSG_EM_SPARC32PLUS,	MSG_EM_UNKNOWN19,	MSG_EM_PPC
};

const char *
conv_emach_str(Half machine)
{
	static char	string[STRSIZE] = { '\0' };

	if (machine == EM_SPARCV9)
		/* special case, not contiguous with other EM_'s */
		return (MSG_ORIG(MSG_EM_SPARCV9));
	else if (machine >= (EM_NUM-1))
		return (conv_invalid_str(string, STRSIZE, (int)machine, 0));
	else
		return (MSG_ORIG(machines[machine]));

}

static const int etypes[] = {
	MSG_ET_NONE,		MSG_ET_REL,		MSG_ET_EXEC,
	MSG_ET_DYN,		MSG_ET_CORE
};

const char *
conv_etype_str(Half etype)
{
	static char	string[STRSIZE] = { '\0' };

	if (etype >= ET_NUM)
		return (conv_invalid_str(string, STRSIZE, (int)etype, 0));
	else
		return (MSG_ORIG(etypes[etype]));
}

static const int versions[] = {
	MSG_EV_NONE,		MSG_EV_CURRENT
};

const char *
conv_ever_str(Word version)
{
	static char	string[STRSIZE] = { '\0' };

	if (version >= EV_NUM)
		return (conv_invalid_str(string, STRSIZE, (int)version, 0));
	else
		return (MSG_ORIG(versions[version]));
}


static const int mm_flags[] = {
	MSG_EF_SPARCV9_TSO,	MSG_EF_SPARCV9_PSO,	MSG_EF_SPARCV9_RMO
};

const char *
conv_eflags_str(Half mach, Word flags)
{
	static char	string[64];

/*
 * Valid vendor extension bits for SPARCV9. This must be
 * updated along with elf_SPARC.h.
 */
#define	EXTBITS_V9		(EF_SPARC_SUN_US1 | EF_SPARC_HAL_R1)


	/*
	 * Make a string representation of the e_flags field.
	 * If any bogus bits are set, then just return a string
	 * containing the numeric value.
	 */
	if ((mach == EM_SPARC) && (flags == EF_SPARC_32PLUS)) {
		sprintf(string, MSG_ORIG(MSG_EF_SPARC_32PLUS));
	} else if ((mach == EM_SPARCV9) &&
	    ((flags & ~(EF_SPARCV9_MM | EXTBITS_V9)) == 0)) {
		sprintf(string, MSG_ORIG(MSG_GBL_OSQBRKT));
		strcat(string, MSG_ORIG(mm_flags[flags &
		    EF_SPARCV9_MM]));
		if (flags & EF_SPARC_SUN_US1)
			strcat(string, MSG_ORIG(MSG_EF_SPARC_SUN_US1));
		if (flags & EF_SPARC_HAL_R1)
			strcat(string, MSG_ORIG(MSG_EF_SPARC_HAL_R1));
		strcat(string, MSG_ORIG(MSG_GBL_CSQBRKT));
	} else
		sprintf(string, MSG_ORIG(MSG_ELF_GEN_FLAGS), flags);

	return (string);
}
