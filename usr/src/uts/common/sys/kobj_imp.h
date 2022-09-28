/*
 * Copyright (c) 1993-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 * Kernel Run-Time Linker/Loader private interfaces.
 */

#ifndef	_SYS_KOBJ_IMPL_H
#define	_SYS_KOBJ_IMPL_H

#pragma ident	"@(#)kobj_impl.h	1.23	98/02/20 SMI"

/*
 * Boot/aux vector attributes.
 */

#ifdef	__cplusplus
extern "C" {
#endif

#define	BA_DYNAMIC	0
#define	BA_PHDR		1
#define	BA_PHNUM	2
#define	BA_PHENT	3
#define	BA_ENTRY	4
#define	BA_PAGESZ	5
#define	BA_LPAGESZ	6
#define	BA_LDELF	7
#define	BA_LDSHDR	8
#define	BA_LDNAME	9
#define	BA_BSS		10
#define	BA_IFLUSH	11
#define	BA_CPU		12
#define	BA_MMU		13
#define	BA_NUM		14

typedef union {
	unsigned long ba_val;
	void *ba_ptr;
} val_t;

/*
 * Segment info.
 */
struct proginfo {
	uint_t size;
	uint_t align;
};

/*
 * Implementation-specific flags.
 */
#define		KOBJ_SYMSWAP	0x01	/* symbol tables swappable	*/
#define		KOBJ_SYMKMEM	0x02	/* symbol tables kmem-alloced	*/
#define		KOBJ_EXEC	0x04	/* executable (unix module)	*/
#define		KOBJ_INTERP	0x08	/* the interpreter module	*/
#define		KOBJ_PRIM	0x10	/* a primary kernel module	*/
#define		KOBJ_RESOLVED	0x20	/* fully resolved		*/
#define		KOBJ_TEXT	0x40	/* text allocation		*/
#define		KOBJ_DATA	0x80	/* data allocation		*/
#define		KOBJ_BSS	0x100	/* BSS allocation		*/

/*
 * kobj_notify_add() data notification structure
 */
typedef struct kobj_notify_list {
	unsigned int		kn_version;	/* interface vers */
	void			(*kn_func)(unsigned int, struct modctl *);
					/* notification func */
	unsigned int		kn_type;	/* notification type */
	struct kobj_notify_list *kn_prev;
	struct kobj_notify_list *kn_next;
} kobj_notify_list;

#define		KOBJ_NVERSION_NONE	0	/* kn_version */
#define		KOBJ_NVERSION_CURRENT	1
#define		KOBJ_NVERSION_NUM	2

/*
 * TOKENS for kobj_notify_list->kn_type
 *
 * krtld can provide notification to external clients on the
 * following events.
 */
#define		KOBJ_NOTIFY_MODLOAD	1	/* loading of modules */
#define		KOBJ_NOTIFY_MODUNLOAD	2	/* unloading of modules */
#define		KOBJ_NOTIFY_MAX		2


#ifdef	DEBUG
#define	KOBJ_DEBUG
#endif

#ifdef KOBJ_DEBUG
/*
 * Debugging flags.
 */
#define	D_DEBUG			0x001	/* general debugging */
#define	D_SYMBOLS		0x002	/* debug symbols */
#define	D_RELOCATIONS		0x004	/* debug relocations */
#define	D_BINDPRI		0x008	/* primary binding */
#define	D_BOOTALLOC		0x010	/* mem allocated from boot */
#define	D_LOADING		0x020	/* section loading */
#define	D_PRIMARY		0x040	/* debug primary mods only */
#define	D_LTDEBUG		0x080	/* light debugging */

extern int kobj_debug;		/* different than moddebug */
#endif

struct bootops;

extern void kobj_init(void *romvec, void *dvec,
	struct bootops *bootvec, val_t *bootaux);
extern int kobj_notify_add(kobj_notify_list *);
extern int kobj_notify_remove(kobj_notify_list *);
extern int do_relocations(struct module *);
extern int do_relocate(struct module *, char *, int, int, Addr);
extern void (*_kobj_printf)(void *, const char *fmt, ...);
extern struct bootops *ops;
extern void exitto(caddr_t);
extern void kobj_sync_instruction_memory(caddr_t, size_t);

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_KOBJ_IMPL_H */
