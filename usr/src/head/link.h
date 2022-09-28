/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 */

#ifndef _LINK_H
#define	_LINK_H

#pragma ident	"@(#)link.h	1.30	97/11/23 SMI"	/* SVr4.0 1.9	*/

#include <sys/link.h>

#ifndef _ASM
#include <libelf.h>
#include <sys/types.h>
#include <dlfcn.h>
#endif

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef _ASM
/*
 * ld support library calls
 */

#ifdef __STDC__
extern void	ld_start(const char *, const Elf32_Half, const char *);
extern void	ld_start64(const char *, const Elf64_Half, const char *);
extern void	ld_atexit(int);
extern void	ld_atexit64(int);
extern void	ld_file(const char *, const Elf_Kind, int, Elf *);
extern void	ld_file64(const char *, const Elf_Kind, int, Elf *);
extern void	ld_section(const char *, Elf32_Shdr *, Elf32_Word,
			Elf_Data *, Elf *);
extern void	ld_section64(const char *, Elf64_Shdr *, Elf64_Word,
			Elf_Data *, Elf *);


#else
extern void	ld_start();
extern void	ld_start64();
extern void	ld_atexit();
extern void	ld_atexit64();
extern void	ld_file();
extern void	ld_file64();
extern void	ld_section();
extern void	ld_section64();
#endif

/*
 * flags passed to ld support calls
 */
#define	LD_SUP_DERIVED		0x1	/* derived filename */
#define	LD_SUP_INHERITED	0x2	/* file inherited from .so DT_NEEDED */
#define	LD_SUP_EXTRACTED	0x4	/* file extracted from archive */
#endif

#define	LM_ID_BASE	0x00
#define	LM_ID_LDSO	0x01
#define	LM_ID_NUM	2

#define	LM_ID_NEWLM	0xff		/* create a new link-map */


/*
 * Run-Time Link-Edit Auditing
 */

#define	LAV_NONE	0
#define	LAV_VERSION1	1
#define	LAV_VERSION2	2
#define	LAV_CURRENT	LAV_VERSION2
#define	LAV_NUM		3

/*
 * Flags that can be or'd into the la_objopen() return code
 */
#define	LA_FLG_BINDTO	0x0001		/* audit symbinds TO this object */
#define	LA_FLG_BINDFROM	0x0002		/* audit symbinding FROM this object */

/*
 * Flags that can be or'd into the 'flags' argument of la_symbind32()
 */
#define	LA_SYMB_NOPLTENTER	0x0001	/* disable pltenter for this symbol */
#define	LA_SYMB_NOPLTEXIT	0x0002	/* disable pltexit for this symbol */
#define	LA_SYMB_STRUCTCALL	0x0004	/* this is functin call passes */
					/* a structure as it's return code */
#define	LA_SYMB_DLSYM		0x0008	/* this symbol bindings is due to */
					/* a call to dlsym() */
#define	LA_SYMB_ALTVALUE	0x0010	/* alternate symbol binding returned */
					/*	by la_symbind() */

#ifndef	_KERNEL
#ifndef	_ASM

#if defined(_LP64)
typedef long	lagreg_t;
#else
typedef int	lagreg_t;
#endif

struct _la_sparc_regs {
	lagreg_t	lr_rego0;
	lagreg_t	lr_rego1;
	lagreg_t	lr_rego2;
	lagreg_t	lr_rego3;
	lagreg_t	lr_rego4;
	lagreg_t	lr_rego5;
	lagreg_t	lr_rego6;
	lagreg_t	lr_rego7;
};

#if defined(_LP64)
typedef struct _la_sparc_regs	La_sparcv9_regs;
#else
typedef struct _la_sparc_regs	La_sparcv8_regs;
#endif


#if !defined(_LP64)
typedef struct {
	lagreg_t	lr_esp;
	lagreg_t	lr_ebp;
} La_i86_regs;
#endif


#if	!defined(_SYS_INT_TYPES_H)
#if	defined(_LP64) || defined(_I32LPx)
typedef unsigned long		uintptr_t;
#else
typedef	unsigned int		uintptr_t;
#endif
#endif


#ifdef	__STDC__
extern uint_t		la_version(uint_t);
extern void		la_preinit(uintptr_t *);
extern uint_t		la_objopen(Link_map *, Lmid_t, uintptr_t *);
extern uint_t		la_objclose(uintptr_t *);
#if	defined(_LP64)
extern uintptr_t	la_symbind64(Elf64_Sym *, uint_t, uintptr_t *,
				uintptr_t *, uint_t *, const char *);
extern uintptr_t	la_sparcv9_pltenter(Elf64_Sym *, uint_t, uintptr_t *,
				uintptr_t *, La_sparcv9_regs *,	uint_t *,
				const char *);
extern uintptr_t	la_pltexit64(Elf64_Sym *, uint_t, uintptr_t *,
				uintptr_t *, uintptr_t, const char *);
#else  /* !defined(_LP64) */
extern uintptr_t	la_symbind32(Elf32_Sym *, uint_t, uintptr_t *,
				uintptr_t *, uint_t *);
extern uintptr_t	la_sparcv8_pltenter(Elf32_Sym *, uint_t, uintptr_t *,
				uintptr_t *, La_sparcv8_regs *, uint_t *);
extern uintptr_t	la_i86_pltenter(Elf32_Sym *, uint_t, uintptr_t *,
				uintptr_t *, La_i86_regs *, uint_t *);
extern uintptr_t	la_pltexit(Elf32_Sym *, uint_t, uintptr_t *,
				uintptr_t *, uintptr_t);
#endif /* _LP64 */
#else  /* __STDC__ */
extern uint_t		la_version();
extern void		la_preinit();
extern uint_t		la_objopen();
extern uint_t		la_objclose();
#if	defined(_LP64)
extern uintptr_t	la_sparcv9_pltenter();
extern uintptr_t	la_pltexit64();
extern uintptr_t	la_symbind64();
#else  /* _ILP32 */
extern uintptr_t	la_sparcv8_pltenter();
extern uintptr_t	la_i86_pltenter();
extern uintptr_t	la_pltexit();
extern uintptr_t	la_symbind32();
#endif /* _LP64 */
#endif
#endif
#endif

#ifdef __cplusplus
}
#endif

#endif	/* _LINK_H */
