/*
 * Copyright (c) 1996-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_MACHELF_H
#define	_SYS_MACHELF_H

#pragma ident	"@(#)machelf.h	1.8	97/10/31 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/elf_386.h>
#ifndef	_ASM
#include <sys/elf.h>
#include <sys/link.h>	/* for Elf*_Dyn */
#endif	/* _ASM */
/*
 * Make machine class dependent data types transparent to the common code
 */
#if defined(_ELF64)

#ifndef	_ASM
typedef	Elf64_Xword	Xword;
typedef	Elf64_Sxword	Sxword;
typedef	Elf64_Word	Word;
typedef	Elf64_Sword	Sword;
typedef	Elf64_Half	Half;
typedef	Elf64_Addr	Addr;
typedef	Elf64_Off	Off;
typedef	unsigned char	Byte;
#endif	/* _ASM */

#if defined(_KERNEL)
#define	ELF_R_TYPE	ELF64_R_TYPE
#define	ELF_R_SYM	ELF64_R_SYM
#define	ELF_R_INFO	ELF64_R_INFO
#define	ELF_ST_BIND	ELF64_ST_BIND
#define	ELF_ST_TYPE	ELF64_ST_TYPE
#endif

#ifndef	_ASM
typedef	Elf64_Ehdr	Ehdr;
typedef	Elf64_Shdr	Shdr;
typedef	Elf64_Sym	Sym;
typedef	Elf64_Syminfo	Syminfo;
typedef	Elf64_Rel	Rel;
typedef	Elf64_Rel	RelX;
typedef	Elf64_Nhdr	Nhdr;
typedef	Elf64_Phdr	Phdr;
typedef	Elf64_Dyn	Dyn;
typedef	Elf64_Boot	Boot;
typedef	Elf64_Verdef	Verdef;
typedef	Elf64_Verdaux	Verdaux;
typedef	Elf64_Verneed	Verneed;
typedef	Elf64_Vernaux	Vernaux;
typedef	Elf64_Versym	Versym;
#endif	/* _ASM */

#else  /* _ILP32 */

#ifndef	_ASM
typedef	Elf32_Word	Xword;	/* Xword/Sxword are 32-bits in Elf32 */
typedef	Elf32_Sword	Sxword;
typedef	Elf32_Word	Word;
typedef	Elf32_Sword	Sword;
typedef	Elf32_Half	Half;
typedef	Elf32_Addr	Addr;
typedef	Elf32_Off	Off;
typedef	unsigned char	Byte;
#endif	/* _ASM */

#if defined(_KERNEL)
#define	ELF_R_TYPE	ELF32_R_TYPE
#define	ELF_R_SYM	ELF32_R_SYM
#define	ELF_R_INFO	ELF32_R_INFO
#define	ELF_ST_BIND	ELF32_ST_BIND
#define	ELF_ST_TYPE	ELF32_ST_TYPE
#endif

#ifndef	_ASM
typedef	Elf32_Ehdr	Ehdr;
typedef	Elf32_Shdr	Shdr;
typedef	Elf32_Sym	Sym;
typedef Elf32_Syminfo	Syminfo;
typedef	Elf32_Rel	Rel;
typedef	Elf32_Rel	RelX;
typedef	Elf32_Nhdr	Nhdr;
typedef	Elf32_Phdr	Phdr;
typedef	Elf32_Dyn	Dyn;
typedef	Elf32_Boot	Boot;
typedef	Elf32_Verdef	Verdef;
typedef	Elf32_Verdaux	Verdaux;
typedef	Elf32_Verneed	Verneed;
typedef	Elf32_Vernaux	Vernaux;
typedef	Elf32_Versym	Versym;
#endif	/* _ASM */

#endif /* _ILP32 */


/*
 *  Elf `printf' type-cast macros.  These force arguements to
 *  be a fixed size so that Elf32 and Elf64 can share common
 *  format strings.
 */
#define	EC_ADDR(a)	((Elf64_Addr)(a))	/* "ull" */
#define	EC_OFF(a)	((Elf64_Off)(a))	/* "ll"  */
#define	EC_HALF(a)	((Elf64_Half)(a))	/* "d"   */
#define	EC_WORD(a)	((Elf64_Word)(a))	/* "u"   */
#define	EC_SWORD(a)	((Elf64_Sword)(a))	/* "d"   */
#define	EC_XWORD(a)	((Elf64_Xword)(a))	/* "ull" */
#define	EC_SXWORD(a)	((Elf64_Sxword)(a))	/* "ll"  */


#ifndef	_ASM
/*
 * Structure used to build the reloc_table[]
 */
typedef struct {
	unsigned short	re_flags;	/* relocation attributes */
	unsigned char	re_fsize;	/* field size (in #bytes) */
} Rel_entry;

#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_MACHELF_H */
