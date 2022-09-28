/*
 * Copyright (c) 1991-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_KOBJ_H
#define	_SYS_KOBJ_H

#pragma ident	"@(#)kobj.h	1.34	98/01/13 SMI"

#include <sys/modctl.h>
#include <sys/elf.h>
#include <sys/machelf.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * List of modules maintained by kobj.c
 */
struct module_list {
	struct module_list *next;
	struct module *mp;
};

typedef unsigned short symid_t;		/* symbol table index */
typedef unsigned char *reloc_dest_t;

struct module {
	int total_allocated;

	Ehdr hdr;
	char *shdrs;
	Shdr *symhdr, *strhdr;

	char *depends_on;

	size_t symsize;
	char *symspace;	/* symbols + strings + hashtbl, or NULL */
	int flags;

	size_t text_size;
	size_t data_size;
	char *text;
	char *data;

	unsigned int symtbl_section;
	/* pointers into symspace, or NULL */
	char *symtbl;
	char *strings;

	unsigned int hashsize;
	symid_t *buckets;
	symid_t *chains;

	unsigned int nsyms;

	unsigned int bss_align;
	size_t bss_size;
	uintptr_t bss;

	char *filename;

	struct module_list *head, *tail;
	reloc_dest_t destination;
};

struct kobj_mem {
	struct kobj_mem	*km_next;
	struct kobj_mem *km_prev;
	uintptr_t	km_addr;
	size_t		km_size;
	uintptr_t	km_alloc_addr;
	size_t		km_alloc_size;
};

struct _buf {
	intptr_t	 _fd;
	char		*_ptr;
	char		*_base;
	char 		*_name;
	int		 _size;
	int		_cnt;
	int		 _off;
	int		_ln;
};


/*
 * Statistical info.
 */
typedef struct {
	int nalloc;
	int nfree;
	int nalloc_calls;
	int nfree_calls;
} kobj_stat_t;

#define	kobj_filename(p) ((p)->_name)
#define	kobj_linenum(p)  ((p)->_ln)
#define	kobj_newline(p)	 ((p)->_ln++)
#define	kobj_getc(p)	(--(p)->_cnt >= 0 ? ((int)*(p)->_ptr++):kobj_filbuf(p))
#define	kobj_ungetc(p)	 (++(p)->_cnt > (p)->_size ? -1 : ((int)*(p)->_ptr--))

#define	B_OFFSET(f_offset) (f_offset & (MAXBSIZE-1))	/* Offset into buffer */
#define	F_PAGE(f_offset)   (f_offset & ~(MAXBSIZE-1))	/* Start of page */

#if defined(_KERNEL)

extern void kobj_load_module(struct modctl *, int);
extern void kobj_unload_module(struct modctl *);
extern uintptr_t kobj_lookup(void *, char *);
extern Sym *kobj_lookup_all(struct module *, char *, int);
extern int kobj_addrcheck(void *, caddr_t);
extern int kobj_module_to_id(void *);
extern void kobj_getmodinfo(void *, struct modinfo *);
extern int kobj_get_needed(void *, short *, int);
extern uintptr_t kobj_getsymvalue(char *, int);
extern char *kobj_getsymname(uintptr_t, ulong_t *);
extern char *kobj_searchsym(struct module *, uintptr_t, ulong_t *);
extern void kobj_get_packing_info(char *);

extern intptr_t kobj_open(char *);
extern struct _buf *kobj_open_path(char *, int);
extern int kobj_read(intptr_t, char *, unsigned int, unsigned int);
extern void kobj_close(intptr_t);
extern void *kobj_alloc(size_t, int);
extern void *kobj_zalloc(size_t, int);
extern void kobj_free(void *, size_t);
extern struct _buf *kobj_open_file(char *);
extern void kobj_close_file(struct _buf *);
extern int kobj_read_file(struct _buf *, char *, unsigned, unsigned);
extern uintptr_t kobj_getelfsym(char *, void *, int *);

extern int kobj_filbuf(struct _buf *);
extern void kobj_sync(void);

extern int kobj_lock_syms(void);
extern int kobj_unlock_syms(void);

extern void kobj_stat_get(kobj_stat_t *);

#endif	/* defined(_KERNEL) */

#ifdef	__cplusplus
}
#endif

#endif /* !_SYS_KOBJ_H */
