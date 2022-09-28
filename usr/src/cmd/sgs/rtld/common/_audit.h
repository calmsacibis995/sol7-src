/*
 *	Copyright (c) 1997 by Sun Microsystems, Inc.
 *	All rights reserved.
 */

#ifndef	__AUDIT_DOT_H
#define	__AUDIT_DOT_H

#pragma ident	"@(#)_audit.h	1.7	97/11/23 SMI"


#ifndef _ASM


#include <sys/types.h>
#include <rtld.h>

typedef struct {
	const char *	al_libname;
	uint_t		al_vernum;	/* audit library version */
	Dl_obj *	al_dlp;		/* dlp of loaded audit library */
	uint_t (*	al_version)(uint_t);
	void (*		al_preinit)(uintptr_t *);
	uint_t (*	al_objopen)(Link_map *, Lmid_t, uintptr_t *);
	uint_t (*	al_objclose)(uintptr_t *);
#ifdef _LP64
	uintptr_t (*	al_pltenter)(Sym *, uint_t, uintptr_t *, uintptr_t *,
				void *, uint_t *, const char *);
	uintptr_t (*	al_pltexit)(Sym *, uint_t, uintptr_t *, uintptr_t *,
				uintptr_t, const char *);
	uintptr_t (*	al_symbind)(Sym *, uint_t, uintptr_t *,
				uintptr_t *, uint_t *, const char *);
#else
	uintptr_t (*	al_pltenter)(Sym *, uint_t, uintptr_t *, uintptr_t *,
				void *, uint_t *);
	uintptr_t (*	al_pltexit)(Sym *, uint_t, uintptr_t *, uintptr_t *,
				uintptr_t);
	uintptr_t (*	al_symbind)(Sym *, uint_t, uintptr_t *,
				uintptr_t *, uint_t *);
#endif /* _LP64 */
} Audit_library;

#define	DYNPLT_CACHE_SIZE	32	/* number of dynplts allocated */
					/*	at a time */

#define	FLG_AI_BINDTO	0x00001
#define	FLG_AI_BINDFROM	0x00002

typedef struct {
	uintptr_t	ai_cookie;
	Word		ai_flags;
} Audit_libinfo;


struct audit_fields {
	Audit_libinfo *	af_libinfo;
	uint_t		af_flags;
	void *		af_dynplts;
};

extern const char *	audit_libraries;
extern uint_t		audit_arg_count;	/* audit arg dup count */

/*
 * Link-Edit audit functions
 */
extern void	audit_preinit(Rt_map *);
extern void	audit_objclose(Rt_map *);
extern int	audit_objopen(Rt_map *);
extern int	audit_setup(const char *);
extern Addr	audit_symbind(Sym *, uint_t, Rt_map *, Rt_map *, Addr value,
			uint_t *);
extern Addr	audit_pltenter(Rt_map *, Rt_map *, Sym *, uint_t, void *,
			uint_t *);
extern Addr	audit_pltexit(uintptr_t, Rt_map *, Rt_map *, Sym *, uint_t);

extern uint_t	audit_flags;

#endif /* _ASM */

/*
 * Valid flags for 'audit_flags'.  These are local to audit.c so
 * they are not defined in '_audit.h'
 */
#define	AF_PLTENTER	0x0001		/* a plt_enter entry point exists */
#define	AF_PLTEXIT	0x0002		/* a plt_exit entry point exists */
					/* NOTE: if the value of AF_PLTEXIT */
					/*	is updated then boot_elf.s */
					/*	must also be updated */
#define	AF_SYMBIND	0x0004		/* a symbind() entry point exists */

#endif /* _AUDIT_DOT_H */
