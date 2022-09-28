/*
 * Copyright (c) 1996-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)reloc.h	1.13	98/02/02 SMI"

#ifndef _RELOC_DOT_H
#define	_RELOC_DOT_H

#if defined(_KERNEL)
#include <sys/machelf.h>
#include <sys/bootconf.h>
#include <sys/kobj.h>
#include <sys/kobj_impl.h>
#else
#include "machdep.h"
#endif /* _KERNEL */

/*
 * Global include file for relocation common code.
 *
 * Flags for reloc_entry->re_flags
 */
#define	FLG_RE_NOTREL		0x0000
#define	FLG_RE_GOTREL		0x0001
#define	FLG_RE_PCREL		0x0002
#define	FLG_RE_RELPC		0x0004
#define	FLG_RE_PLTREL		0x0008
#define	FLG_RE_VERIFY		0x0010	/* verify value fits */
#define	FLG_RE_UNALIGN		0x0020	/* offset is not aligned */
#define	FLG_RE_WDISP16		0x0040	/* funky sparc DISP16 rel */
#define	FLG_RE_SIGN		0x0080	/* value is signed */
#define	FLG_RE_SECTREL		0x1000	/* SECTOFF relative */
#define	FLG_RE_ADDRELATIVE	0x2000	/* RELATIVE relocatin required for */
					/* non-fixed objects */
#define	FLG_RE_EXTOFFSET	0x4000	/* extra offset required by OLO10 */


/*
 * Macros for testing relocation table flags
 */
extern	const Rel_entry		reloc_table[];

#define	IS_PLT(X)		((reloc_table[(X)].re_flags & \
					FLG_RE_PLTREL) != 0)
#define	IS_GOT_RELATIVE(X)	((reloc_table[(X)].re_flags & \
					FLG_RE_GOTREL) != 0)
#define	IS_GOT_PC(X)		((reloc_table[(X)].re_flags & \
					FLG_RE_RELPC) != 0)
#define	IS_PC_RELATIVE(X)	((reloc_table[(X)].re_flags & \
					FLG_RE_PCREL) != 0)
#define	IS_SDA_RELATIVE(X)	((reloc_table[(X)].re_flags & \
					FLG_RE_SDAREL) != 0)
#define	IS_SECT_RELATIVE(X)	((reloc_table[(X)].re_flags & \
					FLG_RE_SECTREL) != 0)
#define	IS_ADD_RELATIVE(X)	((reloc_table[(X)].re_flags & \
					FLG_RE_ADDRELATIVE) != 0)

/*
 * Functions.
 */
#if defined(i386)
extern	int	do_reloc(unsigned char, unsigned char *, Xword *,
			const char *, const char *);
#else /* sparc */

/*
 * NOTE:
 *	the DORELOC() macro is for common sparc code which wants
 *	to call the do_reloc() routine (both 64 & 32 bits).  This way
 *	we do not need to ifdef the call.
 */
#if defined(_ELF64)

extern	int	do_reloc(unsigned char, unsigned char *, Xword *,
			Word, const char *, const char *);
#define	DORELOC(rtype, off, value, xoff, sym, file) \
	do_reloc(rtype, off, value, xoff, sym, file)

#else /* ELF32 */

extern	int	do_reloc(unsigned char, unsigned char *, Xword *,
			const char *, const char *);
#define	DORELOC(rtype, off, value, notused, sym, file) \
	do_reloc(rtype, off, value, sym, file)

#endif /* _ELF64 */

#endif /* i386 */

#if defined(_KERNEL)
/*
 * These are macro's that are only needed for krtld.  Many of these
 * are already defined in the sgs/include files referenced by
 * ld and rtld
 */

#define	S_MASK(n)	((1l << (n)) - 1l)
#define	S_INRANGE(v, n)	(((-(1l << (n)) - 1l) < (v)) && ((v) < (1l << (n))))

/*
 * This converts the sgs eprintf() routine into the _printf()
 * as used by krtld.
 */
#define	eprintf		_kobj_printf
#define	ERR_FATAL	ops

/*
 * Message strings used by doreloc()
 */
#define	MSG_INTL(x)		x
#define	MSG_ORIG(x)		x
#define	MSG_STR_UNKNOWN		"(unknown)"
#define	MSG_REL_UNSUPSZ		"relocation error: %s: file %s: symbol %s: " \
				"offset size (%d bytes) is not supported"
#define	MSG_REL_ERR_STR		"relocation error: %s:"
#define	MSG_REL_ERR_WITH_FILE	"relocation error: file %s: "
#define	MSG_REL_ERR_FILE	" file %s: "
#define	MSG_REL_ERR_SYM		" symbol %s: "
#define	MSG_REL_ERR_VALUE	" value 0x%llx"
#define	MSG_REL_ERR_OFF		" offset 0x%llx\n"
#define	MSG_REL_UNIMPL		" unimplemented relocation type: %d"
#define	MSG_REL_NONALIGN	" offset 0x%llx is non-aligned\n"
#define	MSG_REL_UNNOBITS	" unsupported number of bits: %d"
#define	MSG_REL_NOFIT		" value 0x%llx does not fit\n"
#define	MSG_REL_LOOSEBITS	" looses %d bits at"


extern const char *conv_reloc_SPARC_type_str(Word rtype);
extern const char *conv_reloc_386_type_str(Word rtype);
#endif /* _KERNEL */

#endif /* RELOC_DOT_H */
