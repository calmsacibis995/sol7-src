/*
 * Copyright (c) 1991-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)kobj.c	1.108	98/02/20 SMI"

/*
 * Kernel's linker/loader
 */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/user.h>
#include <sys/kmem.h>
#include <sys/map.h>
#include <sys/reboot.h>
#include <sys/bootconf.h>
#include <sys/debug.h>
#include <sys/uio.h>
#include <sys/file.h>
#include <sys/vnode.h>
#include <sys/user.h>
#include <vm/as.h>
#include <vm/seg_kp.h>
#include <vm/seg_kmem.h>
#include <sys/elf.h>
#include <sys/elf_notes.h>
#include <sys/link.h>
#include <sys/kobj.h>
#include <sys/disp.h>
#include <sys/modctl.h>
#include <sys/varargs.h>
#include <sys/kstat.h>
#include <sys/kobj_impl.h>
#include <sys/machkobj.h>
#include <sys/callb.h>

/*
 * do_symbols() error codes
 */
#define	DOSYM_UNDEF		-1	/* undefined symbol */
#define	DOSYM_UNSAFE		-2	/* MT-unsafe driver symbol */

static struct module *load_exec(val_t *);
static void load_linker(val_t *);
static struct modctl *add_primary(char *filename);
static int bind_primary(val_t *);
static int load_primary(struct module *);
static int get_progbits_size(struct module *, struct proginfo *,
	struct proginfo *);
static int get_progbits(struct module *, struct _buf *);
static int get_syms(struct module *, struct _buf *);
static int do_common(struct module *);
static int do_dependents(struct modctl *);
static int do_symbols(struct module *, u_int);
static void module_assign(struct modctl *, struct module *);
static void free_module_data(struct modctl *);
static int map_setup(void);
static char *depends_on(struct module *);
static char *getmodpath(void);
static char *basename(char *);
static void attr_val(val_t *);
static void *kobj_mod_alloc(struct module *, size_t, int, reloc_dest_t *);
static void kobj_mod_free(void *, size_t, int);
static char *expand_libmacro(char *, char *, char *);

static Sym *lookup_one(struct module *, char *);
static Sym *lookup_kernel(char *);
static void sym_insert(struct module *, char *, symid_t);
static u_int hash_name(char *);
static u_int gethashsize(u_int);

/*PRINTFLIKE2*/
static void kprintf(void *, const char *, ...);
static caddr_t segbrk(caddr_t *, size_t, size_t, caddr_t);

static struct kobjopen_tctl *kobjopen_alloc(char *filename);
static void kobjopen_free(struct kobjopen_tctl *ltp);
static void kobjopen_thread(struct kobjopen_tctl *ltp);

static void *kobj_map_alloc(size_t);
static void kobj_map_free(void *, size_t);

#define	splimp()	splr(__ipltospl(6))	/* XXX -- see 4089398 */

extern int kcopy(const void *, void *, size_t);
extern int elf_mach_ok(Ehdr *);

extern int modrootloaded;
extern int swaploaded;
extern int moddebug;
extern struct bootops *bootops;
extern size_t kobj_map_space_len;
extern struct modctl modules;
extern int mod_mix_changed;
extern int last_module_id;

static struct kmem_backend *kmem_kobj_backend;

#ifdef KOBJ_DEBUG
/*
 * Values that can be or'd in to kobj_debug and their effects:
 *
 *	D_DEBUG		- misc. debugging information.
 *	D_SYMBOLS	- list symbols and their values as they are entered
 *			  into the hash table
 *	D_RELOCATIONS	- display relocation processing information
 *	D_LOADING	- display information about each module as it
 *			  is loaded.
 */
int kobj_debug;
#endif

#ifndef MOD_MAX_MODULES
#define	MOD_MAX_MODULES		100
#endif

#define	KOBJ_MAP_SEGMENTS	(MOD_MAX_MODULES)
#define	MODPATH_PROPNAME	"module-path"

#ifdef MODDIR_SUFFIX
static char slash_moddir_suffix_slash[] = "/" MODDIR_SUFFIX "/";
#endif

/*
 * Flags for memory allocation.
 */
#define	KM_WAIT			0x0		/* wait for it */
#define	KM_NOWAIT		0x1		/* return immediately */
#define	KM_TEMP			0x1000	/* use boot memory in standalone mode */

#define	ALIGN(x, a)	((a) == 0 ? (uintptr_t)(x) : \
	(((uintptr_t)(x) + (uintptr_t)(a) - 1l) & ~((uintptr_t)(a) - 1l)))

static int
get_weakish_int(int *ip)
{
	return (ip == NULL ? 0 : *ip);
}

static void *
get_weakish_pointer(void **ptrp)
{
	return (ptrp == NULL ? 0 : *ptrp);
}

#define	_moddebug	get_weakish_int(&moddebug)
#define	_modrootloaded	get_weakish_int(&modrootloaded)
#define	_swaploaded	get_weakish_int(&swaploaded)
#define	_bootops	get_weakish_pointer((void **)&bootops)

#define	mod(X)		(struct module *)((X)->modl_modp->mod_mp)

void	*romp;		/* rom vector (opaque to us) */
struct bootops *ops;	/* bootops vector */
void *dbvec;		/* debug vector */

#if defined(i386)
void *bopp;	/* XXX i386 kadb support */
#endif

/*
 * kobjopen thread control structure
 */
struct kobjopen_tctl {
	ksema_t		sema;
	char		*name;		/* name of file */
	struct vnode	*vp;		/* vnode return from vn_open() */
	int		errno;		/* error return from vnopen    */
};

/*
 * Structure for defining dynamically expandable library macros
 */

struct lib_macro_info {
	char	*lmi_list;		/* ptr to list of possible choices */
	char	*lmi_macroname;		/* pointer to macro name */
	ushort	lmi_ba_index;		/* index into bootaux vector */
	ushort	lmi_macrolen;		/* macro length */
} libmacros[] = {
	NULL, "CPU", BA_CPU, 0,
	NULL, "MMU", BA_MMU, 0,
};

#define	NLIBMACROS	sizeof (libmacros) / sizeof (struct lib_macro_info)

#ifdef	MPSAS
void	sas_prisyms(struct modctl_list *);
void	sas_syms(struct module *);
#endif

typedef struct kobj_map {
	char *base;
	size_t len;
	struct map *map;
	struct kobj_map *next;
} kobj_map_t;

static kobj_map_t *kobj_map_list;
static kmutex_t kobj_map_lock;

static struct modctl *kobj_modules = NULL;	/* modules loaded */
static struct modctl_list *primaries = NULL;	/* primary kernel module list */
static char *module_path;			/* module search path */
static int mmu_pagesize;			/* system pagesize */
static int lg_pagesize;				/* "large" pagesize */
static int kobj_last_module_id = 0;		/* id assignment */
static int kobj_locked = 0;			/* kobj symbol lock count */
static kobj_notify_list *notify_load = 0;	/* kobj module load */
						/*	notifcation list */
static kobj_notify_list *notify_unload = 0;	/* kobj module unload */
						/*	notifcation list */
static kmutex_t kobj_lock;			/* protects mach memory list */
/*
 * Beginning and end of the kernel's
 * dynamic text/data segments.
 */
static caddr_t _text;
static caddr_t _etext;
static caddr_t _data;
static caddr_t _edata;

static Addr dynseg = 0;	/* load address of "dynamic" segment */

int standalone = 1;			/* an unwholey kernel? */
int use_iflush;				/* iflush after relocations */

void (*_kobj_printf)(void *, const char *, ...);	/* printf routine */

static kobj_stat_t kobj_stat;

#define	MINALIGN	8	/* at least a double-word */

/*
 * XXX fix dependencies on "kernel"; this should work
 * for other standalone binaries as well.
 *
 * XXX Fix hashing code to use one pointer to
 * hash entries.
 *	|----------|
 *	| nbuckets |
 *	|----------|
 *	| nchains  |
 *	|----------|
 *	| bucket[] |
 *	|----------|
 *	| chain[]  |
 *	|----------|
 */

/*
 * Note: support has been added to kadb to help debug the linker
 * itself prior to the handoff to unix.  Just boot with kadb -k
 * and the linker's symbols will be available from the first kadb
 * prompt. Caution: do not set breakpoints on instructions that
 * haven't been relocated yet.  kadb will replace the instruction
 * with a software trap instruction and the linker will scrog this
 * when it tries to relocate the original instruction; you'll get
 * an 'unimplemented instruction' trap.
 */

/*
 * Load, bind and relocate all modules that
 * form the primary kernel. At this point, our
 * externals have not been relocated.
 */
void
kobj_init(
	void *romvec,
	void *dvec,
	struct bootops *bootvec,
	val_t *bootaux)
{
	struct module *mp;
	Addr entry;

	/*
	 * Save these to pass on to
	 * the booted standalone.
	 */
	romp = romvec;
	dbvec = dvec;

#if defined(i386)
	/*
	 * XXX This weirdness is needed because the 386 stuff passes
	 * (struct bootops **) rather than (struct bootops *) when
	 * the debugger is loaded.  Note: it also does the same
	 * with romp, but we simply pass that on without using it.
	 */
	bopp = (void *)bootvec;
	ops = (dvec) ? *(struct bootops **)bootvec : bootvec;
	_kobj_printf = (void (*)(void *, const char *, ...))ops->bsys_printf;
#else
	ops = bootvec;
	_kobj_printf = (void (*)(void *, const char *, ...))bop_putsarg;
#endif

	/*
	 * Save the interesting attribute-values
	 * (scanned by kobj_boot).
	 */
	attr_val(bootaux);

	/*
	 * Check bootops version.
	 */
	if (BOP_GETVERSION(ops) != BO_VERSION) {
		_kobj_printf(ops, "Warning: Using boot version %d, ",
		    BOP_GETVERSION(ops));
		_kobj_printf(ops, "expected %d\n", BO_VERSION);
	}

	/*
	 * Set the module search path.
	 */
	module_path = getmodpath();

	/*
	 * These two modules have actually been
	 * loaded by boot, but we finish the job
	 * by introducing them into the world of
	 * loadable modules.
	 */
	mp = load_exec(bootaux);
	load_linker(bootaux);

	/*
	 * Load all the primary dependent modules.
	 */
	if (load_primary(mp) == -1)
		goto fail;

	/*
	 * Glue it together.
	 */
	if (bind_primary(bootaux) == -1)
		goto fail;

	entry = bootaux[BA_ENTRY].ba_val;

#ifdef KOBJ_DEBUG
	/*
	 * Okay, we'll shut up now.
	 */
	if (kobj_debug & D_PRIMARY)
		kobj_debug &= ~D_PRIMARY;
#endif
	/*
	 * Post setup.
	 */
	standalone = 0;
#ifdef	MPSAS
	sas_prisyms(primaries);
#endif
	s_text = _text;
	e_text = _etext;

	/* sync_instruction_memory */
	kobj_sync_instruction_memory(s_text, e_text - s_text);

#ifdef	KOBJ_DEBUG
	if (kobj_debug & D_DEBUG)
		_kobj_printf(ops,
		    "krtld: transferring control to: 0x%p\n", entry);
#endif
	s_data = _data;
	e_data = _edata;

	/*
	 * Make sure the mod system (and kadb) knows about
	 * the modules already loaded.
	 */
	last_module_id = kobj_last_module_id;
	bcopy(kobj_modules, &modules, sizeof (modules));
	modules.mod_next->mod_prev = &modules;
	modules.mod_prev->mod_next = &modules;

	_kobj_printf = kprintf;
	exitto((caddr_t)entry);
fail:

	_kobj_printf(ops, "krtld: error during initial load/link phase\n");
}

/*
 * Set up any global information derived
 * from attribute/values in the boot or
 * aux vector.
 */
static void
attr_val(val_t *bootaux)
{
	Phdr *phdr;
	int phnum, phsize;
	int i;

	mmu_pagesize = bootaux[BA_PAGESZ].ba_val;
	lg_pagesize = bootaux[BA_LPAGESZ].ba_val;
	use_iflush = bootaux[BA_IFLUSH].ba_val;

	for (i = 0; i < NLIBMACROS; i++) {
		libmacros[i].lmi_list =
			bootaux[libmacros[i].lmi_ba_index].ba_ptr;
		libmacros[i].lmi_macrolen = strlen(libmacros[i].lmi_macroname);
	}

	phdr = (Phdr *)bootaux[BA_PHDR].ba_ptr;
	phnum = bootaux[BA_PHNUM].ba_val;
	phsize = bootaux[BA_PHENT].ba_val;
	for (i = 0; i < phnum; i++) {
		phdr = (Phdr *)(bootaux[BA_PHDR].ba_val + i * phsize);

		if (phdr->p_type != PT_LOAD)
			continue;
		/*
		 * Bounds of the various segments.
		 */
		if (!(phdr->p_flags & PF_X)) {
			dynseg = phdr->p_vaddr;
		} else {
			if (phdr->p_flags & PF_W) {
				_data = (caddr_t)phdr->p_vaddr;
				_edata = _data + phdr->p_memsz;
			} else {
				_text = (caddr_t)phdr->p_vaddr;
				_etext = _text + phdr->p_memsz;
			}
		}
	}
}

/*
 * Set up the booted executable.
 */
static struct module *
load_exec(val_t *bootaux)
{
	char filename[MAXPATHLEN];
	struct modctl *cp;
	struct module *mp;
	Dyn *dyn;
	Sym *sp;
	int i, lsize, osize, nsize, allocsize;
	char *libname, *tmp;

	(void) BOP_GETPROP(ops, "whoami", filename);

	cp = add_primary(filename);

	mp = kobj_zalloc(sizeof (struct module), KM_WAIT);
	cp->mod_mp = mp;

	/*
	 * We don't have the following information
	 * since this module is an executable and not
	 * a relocatable .o.
	 */
	mp->symtbl_section = NULL;
	mp->shdrs = NULL;
	mp->strhdr = NULL;

	/*
	 * Since this module is the only exception,
	 * we cons up some section headers.
	 */
	mp->symhdr = kobj_zalloc(sizeof (Shdr), KM_WAIT);
	mp->strhdr = kobj_zalloc(sizeof (Shdr), KM_WAIT);

	mp->symhdr->sh_type = SHT_SYMTAB;
	mp->strhdr->sh_type = SHT_STRTAB;
	/*
	 * Scan the dynamic structure.
	 */
	for (dyn = (Dyn *) bootaux[BA_DYNAMIC].ba_ptr;
	    dyn->d_tag != DT_NULL; dyn++) {
		switch (dyn->d_tag) {
		case DT_SYMTAB:
			dyn->d_un.d_ptr += dynseg;
			mp->symspace = mp->symtbl = (char *)dyn->d_un.d_ptr;
			mp->symhdr->sh_addr = dyn->d_un.d_ptr;
			break;
		case DT_HASH:
			dyn->d_un.d_ptr += dynseg;
			mp->nsyms = *((u_int *)dyn->d_un.d_ptr + 1);
			mp->hashsize = *(u_int *)dyn->d_un.d_ptr;
			break;
		case DT_STRTAB:
			dyn->d_un.d_ptr += dynseg;
			mp->strings = (char *)dyn->d_un.d_ptr;
			mp->strhdr->sh_addr = dyn->d_un.d_ptr;
			break;
		case DT_STRSZ:
			mp->strhdr->sh_size = dyn->d_un.d_val;
			break;
		case DT_SYMENT:
			mp->symhdr->sh_entsize = dyn->d_un.d_val;
			break;
		}
	}

	/*
	 * Collapse any DT_NEEDED entries into one string.
	 */
	nsize = osize = 0;
	allocsize = MAXPATHLEN;

	mp->depends_on = kobj_alloc(allocsize, KM_WAIT);

	for (dyn = (Dyn *) bootaux[BA_DYNAMIC].ba_ptr;
	    dyn->d_tag != DT_NULL; dyn++)
		if (dyn->d_tag == DT_NEEDED) {
			char *_lib;

			libname = mp->strings + dyn->d_un.d_val;
			if (strchr(libname, '$') != NULL) {
				if ((_lib = expand_libmacro(libname,
				    filename, filename)) != NULL)
					libname = _lib;
				else
					_kobj_printf(ops, "krtld: "
					    "load_exec: fail to "
					    "expand %s\n", libname);
			}
			lsize = strlen(libname);
			nsize += lsize;
			if (nsize + 1 > allocsize) {
				tmp = kobj_alloc(allocsize + MAXPATHLEN,
				    KM_WAIT);
				bcopy(mp->depends_on, tmp, osize);
				kobj_free(mp->depends_on, allocsize);
				mp->depends_on = tmp;
				allocsize += MAXPATHLEN;
			}
			bcopy(libname, mp->depends_on + osize, lsize);
			*(mp->depends_on + nsize) = ' '; /* seperate */
			nsize++;
			osize = nsize;
		}
	if (nsize) {
		mp->depends_on[nsize - 1] = '\0'; /* terminate the string */
		/*
		 * alloc with exact size and copy whatever it got over
		 */
		tmp = kobj_alloc(nsize, KM_WAIT);
		bcopy(mp->depends_on, tmp, nsize);
		kobj_free(mp->depends_on, allocsize);
		mp->depends_on = tmp;
	} else {
		kobj_free(mp->depends_on, allocsize);
		mp->depends_on = NULL;
	}

	mp->flags = KOBJ_EXEC|KOBJ_PRIM;	/* NOT a relocatable .o */
	mp->symhdr->sh_size = mp->nsyms * mp->symhdr->sh_entsize;
	/*
	 * We allocate our own table since we don't
	 * hash undefined references.
	 */
	mp->chains = kobj_zalloc(mp->nsyms * sizeof (symid_t), KM_WAIT);
	mp->buckets = kobj_zalloc(mp->hashsize * sizeof (symid_t), KM_WAIT);

	mp->text = _text;
	mp->data = _data;
	mp->filename = cp->mod_filename;

#ifdef	KOBJ_DEBUG
	if (kobj_debug & D_LOADING) {
		_kobj_printf(ops, "krtld: file=%s\n", mp->filename);
		_kobj_printf(ops, "\ttext:0x%8p", mp->text);
		_kobj_printf(ops, " size: 0x%8x", mp->text_size);
		_kobj_printf(ops, " data:0x%8p", mp->data);
		_kobj_printf(ops, " dsize: 0x%8x\n", mp->data_size);
	}
#endif
	/*
	 * Insert symbols into the hash table.
	 */
	for (i = 0; i < mp->nsyms; i++) {
		sp = (Sym *)(mp->symtbl + i * mp->symhdr->sh_entsize);

		if (sp->st_name == 0 || sp->st_shndx == SHN_UNDEF)
			continue;

		sym_insert(mp, mp->strings + sp->st_name, i);
	}

	return (mp);
}

/*
 * Set up the linker module.
 */
static void
load_linker(val_t *bootaux)
{
	struct module *kmp = (struct module *)kobj_modules->mod_mp;
	struct module *mp;
	struct modctl *cp;
	int i;
	Shdr *shp;
	Sym *sp;
	int shsize;
	char *dlname = (char *)bootaux[BA_LDNAME].ba_ptr;

	cp = add_primary(dlname);

	mp = kobj_zalloc(sizeof (struct module), KM_WAIT);

	cp->mod_mp = mp;
	mp->hdr = *(Ehdr *)bootaux[BA_LDELF].ba_ptr;
	shsize = mp->hdr.e_shentsize * mp->hdr.e_shnum;
	mp->shdrs = kobj_alloc(shsize, KM_WAIT);
	bcopy(bootaux[BA_LDSHDR].ba_ptr, mp->shdrs, shsize);

	for (i = 1; i < (int)mp->hdr.e_shnum; i++) {
		shp = (Shdr *)(mp->shdrs + (i * mp->hdr.e_shentsize));

		if (shp->sh_flags & SHF_ALLOC) {
			if (shp->sh_flags & SHF_WRITE) {
				if (mp->data == NULL)
					mp->data = (char *)shp->sh_addr;
			} else if (mp->text == NULL) {
				mp->text = (char *)shp->sh_addr;
			}
		}
		if (shp->sh_type == SHT_SYMTAB) {
			mp->symtbl_section = i;
			mp->symhdr = shp;
			mp->symspace = mp->symtbl = (char *)shp->sh_addr;
		}
	}
	mp->nsyms = mp->symhdr->sh_size / mp->symhdr->sh_entsize;
	mp->flags = KOBJ_INTERP|KOBJ_PRIM;
	mp->strhdr = (Shdr *)
		(mp->shdrs + mp->symhdr->sh_link * mp->hdr.e_shentsize);
	mp->strings = (char *)mp->strhdr->sh_addr;
	mp->hashsize = gethashsize(mp->nsyms);

	mp->chains = kobj_zalloc(mp->nsyms * sizeof (symid_t), KM_WAIT);
	mp->buckets = kobj_zalloc(mp->hashsize * sizeof (symid_t), KM_WAIT);

	mp->bss = bootaux[BA_BSS].ba_val;
	mp->bss_align = 0;	/* pre-aligned during allocation */
	mp->bss_size = (uintptr_t)_edata - mp->bss;
	mp->text_size = _etext - mp->text;
	mp->data_size = _edata - mp->data;
	mp->filename = cp->mod_filename;

	/*
	 * Now that we've figured out where the linker is,
	 * set the limits for the booted object.
	 */
	kmp->text_size = (size_t)(mp->text - kmp->text);
	kmp->data_size = (size_t)(mp->data - kmp->data);

#ifdef	KOBJ_DEBUG
	if (kobj_debug & D_LOADING) {
		_kobj_printf(ops, "krtld: file=%s\n", mp->filename);
		_kobj_printf(ops, "\ttext:0x%8p", mp->text);
		_kobj_printf(ops, " size: 0x%8x", mp->text_size);
		_kobj_printf(ops, " data:0x%8p", mp->data);
		_kobj_printf(ops, " dsize: 0x%8x\n", mp->data_size);
	}
#endif

	/*
	 * Insert the symbols into the hash table.
	 */
	for (i = 0; i < mp->nsyms; i++) {
		sp = (Sym *)(mp->symtbl + i * mp->symhdr->sh_entsize);

		if (sp->st_name == 0 || sp->st_shndx == SHN_UNDEF)
			continue;
		if (ELF_ST_BIND(sp->st_info) == STB_GLOBAL) {
			if (sp->st_shndx == SHN_COMMON)
				sp->st_shndx = SHN_ABS;
		}
		sym_insert(mp, mp->strings + sp->st_name, i);
	}
}

/*
 * kobj_notify_add()
 */
int
kobj_notify_add(kobj_notify_list * knp)
{
	kobj_notify_list **knl;

	if (knp->kn_version != KOBJ_NVERSION_CURRENT)
		return (-1);

	switch (knp->kn_type) {
	case KOBJ_NOTIFY_MODLOAD:
		knl = &notify_load;
		break;
	case KOBJ_NOTIFY_MODUNLOAD:
		knl = &notify_unload;
		break;
	default:
		return (-1);
	}
	knp->kn_next = 0;
	knp->kn_prev = 0;

	mutex_enter(&kobj_lock);

	if (*knl == 0) {
		/*
		 * first item on list
		 */
		(*knl) = knp;
	} else {
		/*
		 * Insert at head of list.
		 */
		(*knl)->kn_prev = knp;
		knp->kn_next = *knl;
		(*knl) = knp;
	}

	mutex_exit(&kobj_lock);
	return (0);
}


/*
 * kobj_notify_remove()
 */
int
kobj_notify_remove(kobj_notify_list * knp)
{
	kobj_notify_list **	knl;
	kobj_notify_list *	tknp;

	switch (knp->kn_type) {
	case KOBJ_NOTIFY_MODLOAD:
		knl = &notify_load;
		break;
	case KOBJ_NOTIFY_MODUNLOAD:
		knl = &notify_unload;
		break;
	default:
		return (-1);
	}

	mutex_enter(&kobj_lock);

	/* LINTED */
	if (tknp = knp->kn_next)
		tknp->kn_prev = knp->kn_prev;

	/* LINTED */
	if (tknp = knp->kn_prev)
		tknp->kn_next = knp->kn_next;
	else
		*knl = knp->kn_next;

	mutex_exit(&kobj_lock);

	return (0);
}

/*
 * kobj_notify_load()
 *
 * Common routine which can be used for both loading & unloading
 * notifcation.
 */
static void
kobj_notify_load(kobj_notify_list *knp, struct modctl *modp)
{
	while (knp) {
		void (*	fptr)(unsigned int, struct modctl *);
		fptr = (void(*)(unsigned int, struct modctl *))knp->kn_func;
		fptr(knp->kn_type, modp);
		knp = knp->kn_next;
	}
}


/*
 * Ask boot for the module path.
 */
static char *
getmodpath(void)
{
	char *path;
	int len;

	if ((len = BOP_GETPROPLEN(ops, MODPATH_PROPNAME)) == -1)
		return (MOD_DEFPATH);

	path = kobj_zalloc(len, KM_WAIT);

	(void) BOP_GETPROP(ops, MODPATH_PROPNAME, path);

	return (*path ? path : MOD_DEFPATH);
}

static struct modctl *
add_primary(char *filename)
{
	struct modctl *cp;
	struct modctl_list *lp;

	cp = kobj_zalloc(sizeof (struct modctl), KM_WAIT);

	cp->mod_filename = kobj_alloc(strlen(filename) + 1, KM_WAIT);

	/*
	 * For symbol lookup, we assemble our own
	 * modctl list of the primary modules.
	 */
	lp = kobj_zalloc(sizeof (struct modctl_list), KM_WAIT);

	(void) strcpy(cp->mod_filename, filename);
	cp->mod_modname = basename(cp->mod_filename);
	cp->mod_id = kobj_last_module_id++;
	/*
	 * Link the module in. We'll pass this info on
	 * to the mod squad later.
	 */
	if (kobj_modules == NULL) {
		kobj_modules = cp;
		cp->mod_prev = cp->mod_next = cp;
	} else {
		cp->mod_prev = kobj_modules->mod_prev;
		cp->mod_next = kobj_modules;
		kobj_modules->mod_prev->mod_next = cp;
		kobj_modules->mod_prev = cp;
	}

	lp->modl_modp = cp;
	if (primaries == NULL) {
		primaries = lp;
	} else {
		struct modctl_list *last;

		for (last = primaries; last->modl_next;
		    last = last->modl_next)
			;
		last->modl_next = lp;
	}
	return (cp);
}

static int
bind_primary(val_t *bootaux)
{
	struct modctl_list *lp;
	struct modctl *cp;
	struct module *mp;
	Dyn *dyn;
	Word relasz;
	Word relaent;
	char *rela;

	/*
	 * Do common symbols.
	 */
	for (lp = primaries; lp; lp = lp->modl_next) {
		mp = mod(lp);
		/*
		 * These modules should have their
		 * common already taken care of.
		 */
		if (mp->flags & (KOBJ_EXEC|KOBJ_INTERP))
			continue;

		if (do_common(mp) < 0)
			return (-1);
	}

	/*
	 * Resolve symbols.
	 */
	for (lp = primaries; lp; lp = lp->modl_next)
		if (do_symbols(mod(lp), 0) < 0)
			return (-1);

	/*
	 * Do relocations.
	 */
	for (lp = primaries; lp; lp = lp->modl_next) {
		mp = mod(lp);

		if (mp->flags & KOBJ_EXEC) {
			relasz = 0;
			relaent = 0;
			rela = NULL;

			for (dyn = (Dyn *)bootaux[BA_DYNAMIC].ba_ptr;
			    dyn->d_tag != DT_NULL; dyn++) {
				switch (dyn->d_tag) {
				case DT_RELASZ:
				case DT_RELSZ:
					relasz = dyn->d_un.d_val;
					break;
				case DT_RELAENT:
				case DT_RELENT:
					relaent = dyn->d_un.d_val;
					break;
				case DT_RELA:
				case DT_REL:
					rela = (char *)(dyn->d_un.d_ptr +
						dynseg);
					break;
				}
			}
			if (relasz == 0 ||
			    relaent == 0 || rela == NULL)
				return (-1);

#ifdef	KOBJ_DEBUG
			if (kobj_debug & D_RELOCATIONS)
				_kobj_printf(ops, "krtld: relocating: file=%s "
				    "KOBJ_EXEC\n", mp->filename);
#endif
			if (do_relocate(mp, rela, relasz/relaent,
			    relaent, (Addr)mp->text) < 0)
				return (-1);
		} else {
			if (do_relocations(mp) < 0)
				return (-1);
		}

		/* sync_instruction_memory */
		kobj_sync_instruction_memory(mp->text, mp->text_size);
	}

	for (lp = primaries; lp; lp = lp->modl_next) {
		cp = lp->modl_modp;
		mp = (struct module *)cp->mod_mp;

		/*
		 * XXX This is a crock.  Since we can't
		 * force ld to use the full symbol table,
		 * we reload the complete symbol/string
		 * tables (for debugging) once the relocations
		 * have been performed.
		 */
		if (mp->flags & KOBJ_EXEC) {
			struct _buf *file;
			int n;

			file = kobj_open_file(mp->filename);
			if (file == (struct _buf *)-1)
				return (-1);
			if (kobj_read_file(file, (char *)&mp->hdr,
			    sizeof (mp->hdr), 0) < 0)
				return (-1);
			n = mp->hdr.e_shentsize * mp->hdr.e_shnum;
			mp->shdrs = kobj_alloc(n, KM_WAIT);
			if (kobj_read_file(file, mp->shdrs, n,
			    mp->hdr.e_shoff) < 0)
				return (-1);
			if (get_syms(mp, file) < 0)
				return (-1);
			kobj_close_file(file);
		}
	}

	return (0);
}

/*
 * Load all the primary dependent modules.
 */
static int
load_primary(struct module *mp)
{
	struct modctl *cp;
	struct module *dmp;
	char *p, *q;
	char modname[MODMAXNAMELEN];

	/*
	 * If this is the booted executable and a
	 * dependency was specified by boot.
	 */
	if (!(mp->flags & KOBJ_EXEC))
		mp->depends_on = depends_on(mp);

	if ((p = mp->depends_on) == NULL)
		return (0);


	/* CONSTANTCONDITION */
	while (1) {
nextdep:
		/*
		 * Skip space.
		 */
		while (*p && (*p == ' ' || *p == '\t'))
			p++;
		/*
		 * Get module name.
		 */
		q = modname;
		while (*p && *p != ' ' && *p != '\t')
			*q++ = *p++;

		if (q == modname)
			break;

		*q = '\0';
		/*
		 * Check for dup dependencies.
		 */
		cp = kobj_modules;
		do {
			/*
			 * Already loaded.
			 */
			if (strcmp(modname, cp->mod_filename) == 0)
				goto nextdep;

			cp = cp->mod_next;
		} while (cp != kobj_modules);

		cp = add_primary(modname);
		cp->mod_busy = 1;
		/*
		 * Load it.
		 */
		kobj_load_module(cp, 1);
		cp->mod_busy = 0;

		if ((dmp = cp->mod_mp) == NULL)
			return (-1);

		dmp->flags |= KOBJ_PRIM;
		/*
		 * Recurse.
		 */
		if (load_primary(dmp) == -1)
			return (-1);
	}
	return (0);
}


/*
 * Return a string listing module dependencies.
 */
static char *
depends_on(struct module *mp)
{
	Dyn *dyn = NULL, *dynp;
	Shdr *dynshp;
	Shdr *strshp;
	Sym *sp;
	int shn, osize, nsize, allocsize, lsize;
	char *p = NULL;
	char *libname, *tmp, *path;
	char *dynstr = NULL;

	/*
	 * Find dynamic section.
	 */
	for (shn = 1; shn < (int)mp->hdr.e_shnum; shn++) {
		dynshp = (Shdr *)(mp->shdrs + shn * mp->hdr.e_shentsize);
		if (dynshp->sh_type == SHT_DYNAMIC) {
			if (dynshp->sh_addr)
				dyn = (Dyn *)dynshp->sh_addr;
			strshp = (Shdr *)(mp->shdrs +
						dynshp->sh_link *
						mp->hdr.e_shentsize);
			dynstr = (char *)strshp->sh_addr;
			break;
		}
	}

	nsize = osize = 0;
	allocsize = MAXPATHLEN;

	p = kobj_alloc(allocsize, KM_WAIT);

	path = NULL;
	for (dynp = dyn; dynp && dynp->d_tag != DT_NULL; dynp++)
		if (dynp->d_tag == DT_NEEDED) {
			char *_lib;

			libname = (ulong_t)dynp->d_un.d_ptr + dynstr;
			if (strchr(libname, '$') != NULL) {
				if (path == NULL)
					path = kobj_alloc(MAXPATHLEN, KM_WAIT);
				if ((_lib = expand_libmacro(libname,
				    path, path)) != NULL)
					libname = _lib;
				else
					_kobj_printf(ops, "krtld: "
					    "depends_on: failed to "
					    "expand %s\n", libname);
			}

			lsize = strlen(libname);
			nsize += lsize;
			if (nsize + 1 > allocsize) {
				tmp = kobj_alloc(allocsize + MAXPATHLEN,
				    KM_WAIT);
				bcopy(p, tmp, osize);
				kobj_free(p, allocsize);
				p = tmp;
				allocsize += MAXPATHLEN;
			}
			bcopy(libname, p + osize, lsize);
			*(p + nsize) = ' '; /* seperate */
			nsize++;
			osize = nsize;
		}
	if (nsize) {
		*(p + nsize - 1) = '\0'; /* terminate the string */
		if (path != NULL)
			kobj_free(path, MAXPATHLEN);
		/*
		 * alloc with exact size and copy whatever it got over
		 */
		tmp = kobj_alloc(nsize, KM_WAIT);
		bcopy(p, tmp, nsize);
		kobj_free(p, allocsize);
		p = tmp;

		/*
		 * Free up memory for SHT_DYNAMIC & SHT_DYNSTR
		 */
		kobj_free((void *)dynshp->sh_addr, dynshp->sh_size);
		dynshp->sh_addr = 0;
		kobj_free((void *)strshp->sh_addr, strshp->sh_size);
		strshp->sh_addr = 0;
	} else {
		kobj_free(p, allocsize);
		p = NULL;
	}

	/*
	 * Didn't find a DT_NEEDED entry,
	 * try the old "_depends_on" mechanism
	 */
	if (p == NULL && (sp = lookup_one(mp, "_depends_on"))) {
		char *q = (char *)sp->st_value;

		/*
		 * Idiot checks. Make sure it's
		 * in-bounds and NULL terminated.
		 */
		if (kobj_addrcheck(mp, q) || q[sp->st_size - 1] != '\0') {
			_kobj_printf(ops,
			    "Error processing dependency for %s\n",
			    mp->filename);
			return (NULL);
		}
		p = (char *)kobj_alloc(strlen(q) + 1, KM_WAIT);
		(void) strcpy(p, q);
	}
	return (p);
}

void
kobj_getmodinfo(void *xmp, struct modinfo *modinfo)
{
	struct module *mp;
	mp = (struct module *)xmp;

	modinfo->mi_base = mp->text;
	modinfo->mi_size = mp->text_size + mp->data_size;
}

/* return non-zero for a bad address */
int
kobj_addrcheck(void *xmp, caddr_t adr)
{
	struct module *mp;

	mp = (struct module *)xmp;

	if ((adr >= mp->text && adr < mp->text + mp->text_size) ||
	    (adr >= mp->data && adr < mp->data + mp->data_size))
		return (0); /* ok */
	if (mp->bss && adr >= (caddr_t)mp->bss &&
	    adr < (caddr_t)mp->bss + mp->bss_size)
		return (0);
	return (1);
}

void
kobj_load_module(struct modctl *modp, int use_path)
{
	char *filename = modp->mod_filename;
	char *modname = modp->mod_modname;
	int i;
	int n;
	struct _buf *file;
	struct module *mp = NULL;
	static int map_error;

	if (_swaploaded && kobj_map_list == NULL && map_error == 0) {
		if (map_setup() < 0)
			map_error = 1;
	}
	mp = kobj_zalloc(sizeof (*mp), KM_WAIT);

	if ((file = kobj_open_path(filename, use_path)) == (struct _buf *)-1)
		goto bad;

	mp->filename = kobj_alloc(strlen(file->_name) + 1, KM_WAIT);
	(void) strcpy(mp->filename, file->_name);

	if (kobj_read_file(file, (char *)&mp->hdr, sizeof (mp->hdr), 0) < 0) {
		_kobj_printf(ops, "kobj_load_module: %s read header failed\n",
		    modname);
		goto bad;
	}
	for (i = 0; i < SELFMAG; i++) {
		if (mp->hdr.e_ident[i] != ELFMAG[i]) {
			if (_moddebug & MODDEBUG_ERRMSG)
				_kobj_printf(ops, "%s not an elf module\n",
				    modname);
			goto bad;
		}
	}
	/*
	 * It's ELF, but is it our ISA?  Interpreting the header
	 * from a file for a byte-swapped ISA could cause a huge
	 * and unsatisfiable value to be passed to kobj_alloc below
	 * and therefore hang booting.
	 */
	if (!elf_mach_ok(&mp->hdr)) {
		if (_moddebug & MODDEBUG_ERRMSG)
			_kobj_printf(ops, "%s not an elf module for this ISA\n",
			    modname);
		goto bad;
	}

	n = mp->hdr.e_shentsize * mp->hdr.e_shnum;
	mp->shdrs = kobj_alloc(n, KM_WAIT);

	if (kobj_read_file(file, mp->shdrs, n, mp->hdr.e_shoff) < 0) {
		_kobj_printf(ops, "kobj_load_module: %s error reading "
		    "section headers\n", modname);
		goto bad;
	}

	module_assign(modp, mp);

	/* read in sections */
	if (get_progbits(mp, file) < 0) {
		_kobj_printf(ops, "%s error reading sections\n", modname);
		goto bad;
	}

#ifdef	KOBJ_DEBUG
	if (kobj_debug & D_LOADING) {
		_kobj_printf(ops, "krtld: file=%s\n", mp->filename);
		_kobj_printf(ops, "\ttext:0x%8x", mp->text);
		_kobj_printf(ops, " size: 0x%8x", mp->text_size);
		_kobj_printf(ops, " data:0x%8x", mp->data);
		_kobj_printf(ops, " dsize: 0x%8x\n", mp->data_size);
	}
#endif

	/* read in symbols; adjust values for each section's real address */
	if (get_syms(mp, file) < 0) {
		_kobj_printf(ops, "%s error reading symbols\n",
		    modname);
		goto bad;
	}

	/*
	 * For primary kernel modules, we defer
	 * symbol resolution and relocation until
	 * all primary objects have been loaded.
	 */
	if (!standalone) {
		int err;
		/* load all dependents */
		if (do_dependents(modp) < 0) {
			_kobj_printf(ops, "%s error doing dependents\n",
			    modname);
			goto bad;
		}
		/*
		 * resolve undefined and common symbols,
		 * also allocates common space
		 */
		if ((err = do_common(mp)) < 0) {
			_kobj_printf(ops, "WARNING: mod_load: ");
			_kobj_printf(ops, err == DOSYM_UNSAFE ?
			    "MT-unsafe module '%s' rejected\n" :
			    "cannot load module '%s'\n", modname);
			goto bad;
		}
		/* process relocation tables */
		if (do_relocations(mp) < 0) {
			_kobj_printf(ops, "%s error doing relocations\n",
			    modname);
			goto bad;
		}

		mach_mod_epilogue(mp->text_size, mp->destination);
		/* sync_instruction_memory */
		kobj_sync_instruction_memory(mp->text, mp->text_size);
#ifdef	MPSAS
		sas_syms(mp);
#endif
		kobj_notify_load(notify_load, modp);
	}
	kobj_close_file(file);
	return;
bad:
	if (file != (struct _buf *)-1)
		kobj_close_file(file);
	free_module_data(modp);

	module_assign(modp, NULL);
}

static void
module_assign(struct modctl *cp, struct module *mp)
{
	if (standalone) {
		cp->mod_mp = mp;
		return;
	}
	/*
	 * Get the mutex to make life easy in the ksyms driver.
	 */
	mutex_enter(&mod_lock);
	cp->mod_mp = mp;
	mod_mix_changed++;
	mutex_exit(&mod_lock);
}

void
kobj_unload_module(struct modctl *modp)
{
	mutex_enter(&mod_lock);
	kobj_notify_load(notify_unload, modp);
	free_module_data(modp);
	modp->mod_mp = NULL;
	mod_mix_changed++;
	mutex_exit(&mod_lock);
}

static void
free_module_data(struct modctl *modp)
{
	struct module_list *lp, *tmp;
	struct module *mp;

	ASSERT(modp != NULL);
	mp = (struct module *)modp->mod_mp;

	if (mp == NULL)
		return;

	lp = mp->head;
	while (lp) {
		tmp = lp;
		lp = lp->next;
		kobj_free((char *)tmp, sizeof (*tmp));
	}
	if (mp->symspace)
		if (mp->flags & KOBJ_SYMSWAP)
			kobj_map_free(mp->symspace, mp->symsize);
		else
			kobj_free(mp->symspace, mp->symsize);
	if (mp->bss)
		kobj_mod_free((void *)mp->bss, mp->bss_size,
		    mp->flags & KOBJ_BSS);
	if (mp->text)
		kobj_mod_free(mp->text, mp->text_size, mp->flags & KOBJ_TEXT);
	if (mp->data)
		kobj_mod_free(mp->data, mp->data_size, mp->flags & KOBJ_DATA);
	if (mp->symhdr)
		kobj_free(mp->symhdr, mp->hdr.e_shentsize);
	if (mp->shdrs)
		kobj_free(mp->shdrs,
		    mp->hdr.e_shentsize * mp->hdr.e_shnum);
	if (mp->depends_on)
		kobj_free(mp->depends_on, strlen(mp->depends_on)+1);
	if (mp->filename)
		kobj_free(mp->filename, strlen(mp->filename)+1);

	kobj_free((char *)mp, sizeof (*mp));
}

static int
get_progbits_size(struct module *mp, struct proginfo *tp, struct proginfo *dp)
{
	struct proginfo *pp;
	u_int shn;
	Shdr *shp;

	/*
	 * loop through sections to find out how much space we need
	 * for text, data, (also bss that is already assigned)
	 */
	for (shn = 1; shn < mp->hdr.e_shnum; shn++) {
		shp = (Shdr *)(mp->shdrs + shn * mp->hdr.e_shentsize);
		if (!(shp->sh_flags & SHF_ALLOC))
			continue;
		if (shp->sh_addr != 0) {
			_kobj_printf(ops,
			    "%s non-zero sect addr in input file\n",
			    mp->filename);
			return (-1);
		}
		pp = (shp->sh_flags & SHF_WRITE)? dp : tp;

		if (shp->sh_addralign > pp->align)
			pp->align = shp->sh_addralign;
		pp->size = ALIGN(pp->size, shp->sh_addralign);
		pp->size += shp->sh_size;
	}
	return (0);
}

static int
get_progbits(struct module *mp, struct _buf *file)
{
	struct proginfo *tp, *dp;
	Shdr *shp;
	reloc_dest_t dest = NULL;
	uintptr_t bits_ptr;
	uintptr_t text = 0, data, textptr;
	u_int shn;
	int err = -1;

	tp = kobj_zalloc(sizeof (struct proginfo), KM_WAIT);
	dp = kobj_zalloc(sizeof (struct proginfo), KM_WAIT);
	/*
	 * loop through sections to find out how much space we need
	 * for text, data, (also bss that is already assigned)
	 */
	if (get_progbits_size(mp, tp, dp) < 0)
		goto done;

	mp->text_size = tp->size;
	mp->data_size = dp->size;

	if (standalone) {
		mp->text = segbrk(&_etext, mp->text_size, tp->align, _data);
		/*
		 * If we can't grow the text segment, try the
		 * data segment before failing.
		 */
		if (mp->text == NULL)
			mp->text = segbrk(&_edata, mp->text_size,
					tp->align, 0);
		mp->data = segbrk(&_edata, mp->data_size, dp->align, 0);

		if (mp->text == NULL || mp->data == NULL)
			goto done;
	} else {
		static int uninitialized = 1;

		if (uninitialized) {
			uninitialized = 0;
			mach_init_kobj();
			kmem_kobj_backend = kmem_backend_create(
			    "kmem_kobj_backend",
#ifdef _LP64
			    kmem_getpages32, kmem_freepages32,
#else
			    kmem_getpages, kmem_freepages,
#endif
			    PAGESIZE, KMEM_CLASS_WIRED);
		}
		/*
		 * some architectures may want to load the module on a
		 * page that is currently read only. It may not be
		 * possible for those architectures to remap their page
		 * on the fly. So we provide a facility for them to hang
		 * a private hook where the memory they assign the module
		 * is not the actual place where the module loads.
		 *
		 * In this case there are two addresses that deal with the
		 * modload.
		 * 1) the final destination of the module
		 * 2) the address that is used to view the newly
		 * loaded module until all the relocations relative to 1
		 * above are completed.
		 *
		 * That is what dest is used for below.
		 */
		mp->text_size += tp->align;
		mp->data_size += dp->align;
		mp->text = kobj_mod_alloc(mp, mp->text_size, KOBJ_TEXT, &dest);
		/*
		 * a remap is taking place. Align the text ptr relative
		 * to the secondary mapping. That is where the bits will
		 * be read in.
		 */
		if (dest)
			text = ALIGN((uintptr_t)dest, tp->align);
		mp->data = kobj_mod_alloc(mp, mp->data_size, KOBJ_DATA, NULL);
	}
	textptr = (u_int)mp->text;
	textptr = ALIGN(textptr, tp->align);
	mp->destination = dest;

	/*
	 * This is the case where a remap is not being done.
	 */
	if (text == 0)
		text = ALIGN((uintptr_t)mp->text, tp->align);
	data = ALIGN((uintptr_t)mp->data, dp->align);

	/* now loop though sections assigning addresses and loading the data */
	for (shn = 1; shn < mp->hdr.e_shnum; shn++) {
		shp = (Shdr *)(mp->shdrs + shn * mp->hdr.e_shentsize);
		if (!(shp->sh_flags & SHF_ALLOC))
			continue;

		bits_ptr = (shp->sh_flags & SHF_WRITE) ? data:  text;
		bits_ptr = ALIGN(bits_ptr, shp->sh_addralign);
		if (shp->sh_type == SHT_NOBITS) {
			/*
			 * Zero bss.
			 */
			bzero((caddr_t)bits_ptr, shp->sh_size);
			shp->sh_type = SHT_PROGBITS;
		} else {
			if (kobj_read_file(file, (char *)bits_ptr,
			    shp->sh_size, shp->sh_offset) < 0)
				goto done;
		}

		if (shp->sh_flags & SHF_WRITE)
			shp->sh_addr = bits_ptr;
		else {
			textptr = ALIGN(textptr, shp->sh_addralign);
			shp->sh_addr = textptr;
			textptr += shp->sh_size;
		}

		bits_ptr += shp->sh_size;
		if (shp->sh_flags & SHF_WRITE)
			data = bits_ptr;
		else
			text = bits_ptr;
	}
	err = 0;
done:
	(void) kobj_free(tp, sizeof (struct proginfo));
	(void) kobj_free(dp, sizeof (struct proginfo));

	return (err);
}

static int
get_syms(struct module *mp, struct _buf *file)
{
	u_int		shn;
	Shdr	*shp;
	u_int		i;
	Sym	*sp, *ksp;
	char		*symname;
	int		dosymtab = 0;
	extern char 	stubs_base[], stubs_end[];

	/*
	 * Find the interesting sections.
	 */
	for (shn = 1; shn < mp->hdr.e_shnum; shn++) {
		Shdr *	_shp;
		shp = (Shdr *)(mp->shdrs + shn * mp->hdr.e_shentsize);
		switch (shp->sh_type) {
		case SHT_SYMTAB:
			mp->symtbl_section = shn;
			mp->symhdr = kobj_alloc(mp->hdr.e_shentsize, KM_WAIT);
			/*
			 * XXX We keep the section headers,
			 * so why do we need another copy?
			 */
			bcopy(shp, mp->symhdr, mp->hdr.e_shentsize);
			dosymtab++;
			break;

		case SHT_RELA:
		case SHT_REL:
			/*
			 * Already loaded.
			 */
			if (shp->sh_addr)
				continue;
			shp->sh_addr = (Addr)
			    kobj_alloc(shp->sh_size, KM_WAIT|KM_TEMP);

			if (kobj_read_file(file, (char *)shp->sh_addr,
			    shp->sh_size, shp->sh_offset) < 0) {
				_kobj_printf(ops, "krtld: get_syms: %s, ",
				    mp->filename);
				_kobj_printf(ops, "error reading section %d\n",
				    shn);
				return (-1);
			}
			break;
		case SHT_DYNAMIC:
			/*
			 * We allocate space for the DYNAMIC section
			 * here.  This will be examined in the
			 * do_depends section and then freed after
			 * use.
			 */
			if (shp->sh_link > mp->hdr.e_shnum) {
				_kobj_printf(ops, "krtld: get_syms: %s, ",
				    mp->filename);
				_kobj_printf(ops, "error reading section %d\n",
				    shp->sh_info);
				continue;
			}
			_shp = (Shdr *)(mp->shdrs + shp->sh_link *
				mp->hdr.e_shentsize);

			shp->sh_addr = (Addr)kobj_alloc(shp->sh_size,
				KM_WAIT|KM_TEMP);
			if (kobj_read_file(file, (char *)shp->sh_addr,
			    shp->sh_size, shp->sh_offset) < 0) {
				_kobj_printf(ops, "krtld: get_syms: %s, ",
				    mp->filename);
				_kobj_printf(ops, "error reading section %d\n",
				    shn);
				kobj_free((void *)shp->sh_addr, shp->sh_size);
				shp->sh_addr = 0;
				continue;
			}

			_shp->sh_addr = (Addr)kobj_alloc(_shp->sh_size,
				KM_WAIT|KM_TEMP);
			if (kobj_read_file(file, (char *)_shp->sh_addr,
			    _shp->sh_size, _shp->sh_offset) < 0) {
				_kobj_printf(ops, "krtld: get_syms: %s, ",
				    mp->filename);
				_kobj_printf(ops, "error reading section %d\n",
				    shp->sh_link);
				kobj_free((void *)shp->sh_addr, shp->sh_size);
				shp->sh_addr = 0;
				kobj_free((void *)_shp->sh_addr,
					_shp->sh_size);
				_shp->sh_addr = 0;
				continue;
			}
			break;
		}
	}

	/*
	 * This is true for a stripped executable.  In the case of
	 * 'unix' it can be stripped but it still contains the SHT_DYNSYM,
	 * and since that symbol information is still present everything
	 * is just fine.
	 */
	if (!dosymtab)
		return (0);

	/*
	 * get the associated string table header
	 */
	if ((mp->symhdr == 0) || (mp->symhdr->sh_link >= mp->hdr.e_shnum))
		return (-1);
	mp->strhdr = (Shdr *)
		(mp->shdrs + mp->symhdr->sh_link * mp->hdr.e_shentsize);

	mp->nsyms = mp->symhdr->sh_size / mp->symhdr->sh_entsize;
	mp->hashsize = gethashsize(mp->nsyms);
	/*
	 * allocate enough space in one chunk for the symbol table, string
	 * table, hash table buckets, and hash table chains
	 */
	mp->symsize = mp->symhdr->sh_size + mp->strhdr->sh_size + sizeof (int) +
		(mp->hashsize + mp->nsyms) * sizeof (symid_t);

	/* make it a multiple of sizeof (long), also ensure non-zero */
	mp->symsize = (mp->symsize + sizeof (long)) & ~(sizeof (long) - 1);

	/*
	 * Normally if KADB is present we lock down all symbol tables
	 * and if KADB is not present we unlock all symbol tables.
	 *
	 * There are reasons why users may want to override the default.
	 * One reason is that if the system is not boot with KADB and
	 * keeps crashing for some reason, symbols may not be in
	 * the crash dump unless the symbols are locked down.
	 * Another reason is that locking down symbols may make the
	 * system behave differently than if they are not locked down.
	 * So you may want to boot with KADB but not lock down symbols.
	 *
	 * There are two flags in moddebug to control the locking of symbols.
	 * MODDEBUG_LOCKSYMBOLS forces symbols to be locked down regardless
	 * of the presence of KADB.  If this flag is not set, then
	 * MODDEBUG_UNLOCKSYMBOLS can be set to unlock symbols even when
	 * KADB is present.
	 */
	if (kobj_map_list == NULL) {
		mp->symspace = kobj_zalloc(mp->symsize, KM_WAIT|KM_TEMP);
		if (!standalone)
			mp->flags |= KOBJ_SYMKMEM;
	} else {
		if ((mp->symspace = kobj_map_alloc(mp->symsize)) == NULL)
			return (-1);
		bzero(mp->symspace, mp->symsize);
		mp->flags |= KOBJ_SYMSWAP;
	}

	mp->symtbl = mp->symspace;
	mp->strings = mp->symspace + mp->symhdr->sh_size;
	mp->buckets = (symid_t *)((((uintptr_t)mp->strings +
	    mp->strhdr->sh_size) | (sizeof (symid_t) - 1)) + 1);
	mp->chains = mp->buckets + mp->hashsize;

	if (kobj_read_file(file, mp->symtbl,
	    mp->symhdr->sh_size, mp->symhdr->sh_offset) < 0 ||
	    kobj_read_file(file, mp->strings,
	    mp->strhdr->sh_size, mp->strhdr->sh_offset) < 0)
		return (-1);

	/*
	 * loop through the symbol table adjusting values to account
	 * for where each section got loaded into memory.  Also
	 * fill in the hash table.
	 */
	for (i = 1; i < mp->nsyms; i++) {
		sp = (Sym *)(mp->symtbl + i * mp->symhdr->sh_entsize);
		if (sp->st_shndx < SHN_LORESERVE) {
			if (sp->st_shndx >= mp->hdr.e_shnum) {
				_kobj_printf(ops, "%s bad shndx ",
				    file->_name);
				_kobj_printf(ops, "in symbol %d\n", i);
				return (-1);
			}
			shp = (Shdr *)
			    (mp->shdrs +
			    sp->st_shndx * mp->hdr.e_shentsize);
			if (!(mp->flags & KOBJ_EXEC))
				sp->st_value += shp->sh_addr;
		}

		if (sp->st_name == 0 || sp->st_shndx == SHN_UNDEF)
			continue;
		if (sp->st_name >= mp->strhdr->sh_size)
			return (-1);

		symname = mp->strings + sp->st_name;

		if (!(mp->flags & KOBJ_EXEC) &&
		    ELF_ST_BIND(sp->st_info) == STB_GLOBAL) {
			ksp = kobj_lookup_all(mp, symname, 0);

			if (ksp && ELF_ST_BIND(ksp->st_info) == STB_GLOBAL &&
			    sp->st_shndx != SHN_UNDEF &&
			    sp->st_shndx != SHN_COMMON &&
			    ksp->st_shndx != SHN_UNDEF &&
			    ksp->st_shndx != SHN_COMMON) {
				/*
				 * Unless this symbol is a stub,
				 * it's multiply defined.
				 */
				if (standalone ||
				    ksp->st_value < (uintptr_t)stubs_base ||
				    ksp->st_value >= (uintptr_t)stubs_end) {
					_kobj_printf(ops,
					    "%s symbol ", file->_name);
					_kobj_printf(ops,
					    "%s multiply defined\n", symname);
				}
			}
		}
		sym_insert(mp, symname, i);
	}
	return (0);
}

static int
do_dependents(struct modctl *modp)
{
	struct module *mp;
	struct modctl *req;
	struct module_list *lp;
	char modname[MODMAXNAMELEN];
	char *p, *q;
	int retval = 0;

	mp = modp->mod_mp;

	if ((mp->depends_on = depends_on(mp)) == NULL)
		return (0);

	p = mp->depends_on;
	for (;;) {
		retval = 0;
		/*
		 * Skip space.
		 */
		while (*p && (*p == ' ' || *p == '\t'))
			p++;
		/*
		 * Get module name.
		 */
		q = modname;
		while (*p && *p != ' ' && *p != '\t')
			*q++ = *p++;

		if (q == modname)
			break;

		*q = '\0';
		if ((req = mod_load_requisite(modp, modname)) == NULL)
			break;

		for (lp = mp->head; lp; lp = lp->next) {
			if (lp->mp == req->mod_mp)
				break;	/* already on the list */
		}

		if (lp == NULL) {
			lp = kobj_zalloc(sizeof (*lp), KM_WAIT);

			lp->mp = req->mod_mp;
			lp->next = NULL;
			if (mp->tail)
				mp->tail->next = lp;
			else
				mp->head = lp;
			mp->tail = lp;
		}
		mod_release_mod(req);

	}
	return (retval);
}

static int
do_common(struct module *mp)
{
	int err;

	/*
	 * first time through, assign all symbols defined in other
	 * modules, and count up how much common space will be needed
	 * (bss_size and bss_align)
	 */
	if ((err = do_symbols(mp, 0)) < 0)
		return (err);
	/*
	 * increase bss_size by the maximum delta that could be
	 * computed by the ALIGN below
	 */
	mp->bss_size += mp->bss_align;
	if (mp->bss_size) {
		if (standalone)
			mp->bss = (uintptr_t)segbrk(&_edata, mp->bss_size,
			    MINALIGN, 0);
		else
			mp->bss = (uintptr_t)kobj_mod_alloc(mp, mp->bss_size,
			    KOBJ_BSS, NULL);
		bzero((void *)mp->bss, mp->bss_size);
		/* now assign addresses to all common symbols */
		if ((err = do_symbols(mp, ALIGN(mp->bss, mp->bss_align))) < 0)
			return (err);
	}
	return (0);
}

static int
do_symbols(struct module *mp, u_int bss_base)
{
	int bss_align;
	uintptr_t bss_ptr;
	int err;
	int i;
	Sym *sp, *sp1;
	char *name;
	int assign;
	int resolved = 1;

	/*
	 * Nothing left to do (optimization).
	 */
	if (mp->flags & KOBJ_RESOLVED)
		return (0);

	assign = (bss_base) ? 1 : 0;
	bss_ptr = bss_base;
	bss_align = 0;
	err = 0;

	for (i = 1; i < mp->nsyms; i++) {
		sp = (Sym *)(mp->symtbl + mp->symhdr->sh_entsize * i);
		/*
		 * we know that st_name is in bounds, since get_sections
		 * has already checked all of the symbols
		 */
		name = mp->strings + sp->st_name;
		if (sp->st_shndx != SHN_UNDEF && sp->st_shndx != SHN_COMMON)
			continue;

		if (ELF_ST_BIND(sp->st_info) != STB_LOCAL) {
			if ((sp1 = kobj_lookup_all(mp, name, 0)) != NULL) {
				sp->st_shndx = SHN_ABS;
				sp->st_value = sp1->st_value;
				continue;
			}
		}
		if (sp->st_shndx == SHN_UNDEF) {
			resolved = 0;
			/*
			 * If it's not a weak reference and it's
			 * not a primary object, it's an error.
			 * (Primary objects may take more than
			 * one pass to resolve)
			 */
			if (!(mp->flags & KOBJ_PRIM) &&
			    ELF_ST_BIND(sp->st_info) != STB_WEAK) {
				_kobj_printf(ops, "%s: undefined symbol",
				    mp->filename);
				_kobj_printf(ops, " %s\n", name);
				/*
				 * Try to determine whether this symbol
				 * represents a dependency on obsolete
				 * unsafe driver support.  This is just
				 * to make the warning more informative.
				 */
				if (strcmp(name, "sleep") == 0 ||
				    strcmp(name, "unsleep") == 0 ||
				    strcmp(name, "wakeup") == 0 ||
				    strcmp(name, "bsd_compat_ioctl") == 0 ||
				    strcmp(name, "unsafe_driver") == 0 ||
				    strncmp(name, "spl", 3) == 0 ||
				    strncmp(name, "i_ddi_spl", 9) == 0)
					err = DOSYM_UNSAFE;
				if (err == 0)
					err = DOSYM_UNDEF;
			}
			continue;
		}
		/*
		 * It's a common symbol - st_value is the
		 * required alignment.
		 */
		if (sp->st_value > bss_align)
			bss_align = sp->st_value;
		bss_ptr = ALIGN(bss_ptr, sp->st_value);
		if (assign) {
			sp->st_shndx = SHN_ABS;
			sp->st_value = bss_ptr;
		}
		bss_ptr += sp->st_size;
	}
	if (err)
		return (err);
	if (assign == 0 && mp->bss == NULL) {
		mp->bss_align = bss_align;
		mp->bss_size = bss_ptr;
	} else if (resolved) {
		mp->flags |= KOBJ_RESOLVED;
	}

	return (0);
}

static u_int
hash_name(char *p)
{
	unsigned int g;
	u_int hval;

	hval = 0;
	while (*p) {
		hval = (hval << 4) + *p++;
		if ((g = (hval & 0xf0000000)) != 0)
			hval ^= g >> 24;
		hval &= ~g;
	}
	return (hval);
}

/* look for name in all modules */
uintptr_t
kobj_getsymvalue(char *name, int kernelonly)
{
	Sym *sp;
	struct modctl *modp;
	struct module *mp;

	if ((sp = lookup_kernel(name)) != NULL)
		return ((uintptr_t)sp->st_value);

	if (kernelonly)
		return (0);	/* didn't find it in the kernel so give up */

	for (modp = modules.mod_next; modp != &modules; modp = modp->mod_next) {
		mp = (struct module *)modp->mod_mp;

		if (mp == NULL || (mp->flags & KOBJ_PRIM))
			continue;

		if ((sp = lookup_one(mp, name)) != NULL)
			return ((uintptr_t)sp->st_value);
	}
	return (0);
}

/* look for a symbol near value. */
char *
kobj_getsymname(uintptr_t value, u_long *offset)
{
	char *name;
	struct modctl *modp;

	struct modctl_list *lp;
	struct module *mp;

	/*
	 * Loop through the primary kernel modules.
	 */
	for (lp = primaries; lp; lp = lp->modl_next) {
		mp = mod(lp);

		if ((name = kobj_searchsym(mp, value, offset)) != NULL)
			return (name);
	}
	for (modp = modules.mod_next; modp != &modules; modp = modp->mod_next) {
		mp = (struct module *)modp->mod_mp;

		if (mp == NULL || (mp->flags & KOBJ_PRIM))
			continue;

		if ((name = kobj_searchsym(modp->mod_mp, value,
		    offset)) != NULL)
			return (name);
	}
	return (NULL);
}

/* return address of symbol and size */

uintptr_t
kobj_getelfsym(char *name, void *mp, int *size)
{
	Sym *sp;

	if (mp == NULL)
		sp = lookup_kernel(name);
	else
		sp = lookup_one(mp, name);

	if (sp == NULL)
		return (0);

	*size = (int)sp->st_size;
	return ((uintptr_t)sp->st_value);
}

uintptr_t
kobj_lookup(void *mod, char *name)
{
	Sym *sp;

	sp = lookup_one(mod, name);

	if (sp == NULL)
		return (0);

	return ((uintptr_t)sp->st_value);
}

char *
kobj_searchsym(struct module *mp, uintptr_t value, u_long *offset)
{
	Sym *symtabptr;
	char *strtabptr;
	int symnum;
	Sym *sym;
	Sym *cursym;
	uintptr_t curval;

	*offset = (u_long)-1l;		/* assume not found */
	cursym  = NULL;

	if (kobj_addrcheck(mp, (void *)value) != 0)
		return (NULL);		/* not in this module */

	strtabptr  = mp->strings;
	symtabptr  = (Sym *)mp->symtbl;

	/*
	 * Scan the module's symbol table for a symbol <= value
	 */
	for (symnum = 0, sym = symtabptr;
		symnum < mp->nsyms;
		symnum++, sym = (Sym *)
		    ((uintptr_t)sym + mp->symhdr->sh_entsize)) {
		if (ELF_ST_BIND(sym->st_info) != STB_GLOBAL) {
			if (ELF_ST_BIND(sym->st_info) != STB_LOCAL)
				continue;
			if (ELF_ST_TYPE(sym->st_info) != STT_OBJECT &&
			    ELF_ST_TYPE(sym->st_info) != STT_FUNC)
				continue;
		}

		curval = (uintptr_t)sym->st_value;

		if (curval > value)
			continue;

		if (value - curval < *offset) {
			*offset = (u_long)(value - curval);
			cursym = sym;
		}
	}
	if (cursym == NULL)
		return (NULL);

	return (strtabptr + cursym->st_name);
}

Sym *
kobj_lookup_all(struct module *mp, char *name, int include_self)
{
	Sym *sp;
	struct module_list *mlp;
	struct modctl_list *clp;
	struct module *mmp;

	if (include_self && (sp = lookup_one(mp, name)) != NULL)
		return (sp);

	for (mlp = mp->head; mlp; mlp = mlp->next) {
		if ((sp = lookup_one(mlp->mp, name)) != NULL &&
		    ELF_ST_BIND(sp->st_info) != STB_LOCAL)
			return (sp);
	}
	/*
	 * Loop through the primary kernel modules.
	 */
	for (clp = primaries; clp; clp = clp->modl_next) {
		mmp = mod(clp);

		if (mmp == NULL || mp == mmp)
			continue;

		if ((sp = lookup_one(mmp, name)) != NULL &&
		    ELF_ST_BIND(sp->st_info) != STB_LOCAL)
			return (sp);
	}
	return (NULL);
}

static Sym *
lookup_kernel(char *name)
{
	struct modctl_list *lp;
	struct module *mp;
	Sym *sp;

	/*
	 * Loop through the primary kernel modules.
	 */
	for (lp = primaries; lp; lp = lp->modl_next) {
		mp = mod(lp);

		if (mp == NULL)
			continue;

		if ((sp = lookup_one(mp, name)) != NULL)
			return (sp);
	}
	return (NULL);
}

static Sym *
lookup_one(struct module *mp, char *name)
{
	symid_t *ip;
	char *name1;
	Sym *sp;

	for (ip = &mp->buckets[hash_name(name) % mp->hashsize]; *ip;
	    ip = &mp->chains[*ip]) {
		sp = (Sym *)(mp->symtbl +
		    mp->symhdr->sh_entsize * *ip);
		name1 = mp->strings + sp->st_name;
		if (strcmp(name, name1) == 0 &&
		    ELF_ST_TYPE(sp->st_info) != STT_FILE &&
		    sp->st_shndx != SHN_UNDEF &&
		    sp->st_shndx != SHN_COMMON)
			return (sp);
	}
	return (NULL);
}

static void
sym_insert(struct module *mp, char *name, symid_t index)
{
	symid_t *ip;

#ifdef KOBJ_DEBUG
		if (kobj_debug & D_SYMBOLS) {
			static struct module *lastmp = NULL;
			Sym *sp;
			if (lastmp != mp) {
				_kobj_printf(ops,
				    "krtld: symbol entry: file=%s\n",
				    mp->filename);
				_kobj_printf(ops,
				    "krtld:\tsymndx\tvalue\t\t"
				    "symbol name\n");
				lastmp = mp;
			}
			sp = (Sym *)(mp->symtbl +
				index * mp->symhdr->sh_entsize);
			_kobj_printf(ops, "krtld:\t[%3d]", index);
			_kobj_printf(ops, "\t0x%8x", sp->st_value);
			_kobj_printf(ops, "\t%s\n", name);
		}

#endif
	for (ip = &mp->buckets[hash_name(name) % mp->hashsize]; *ip;
	    ip = &mp->chains[*ip]) {
		;
	}
	*ip = index;
}

/*
 * fullname is dynamically allocated to be able to hold the
 * maximum size string that can be constructed from name.
 * path is exactly like the shell PATH variable.
 */
struct _buf *
kobj_open_path(char *name, int use_path)
{
	char *p, *q;
	char *pathp;
	char *pathpsave;
	char *fullname;
	int maxpathlen;
	struct _buf *file;

	if (!use_path)
		pathp = "";		/* use name as specified */
	else
		pathp = module_path;	/* use configured default path */

	pathpsave = pathp;		/* keep this for error reporting */

	/*
	 * Allocate enough space for the largest possible fullname.
	 * since path is of the form <directory> : <directory> : ...
	 * we're potentially allocating a little more than we need to
	 * but we'll allocate the exact amount when we find the right directory.
	 * (The + 3 below is one for NULL terminator and one for the '/'
	 * we might have to add at the beginning of path and one for
	 * the '/' between path and name.)
	 */
	maxpathlen = strlen(pathp) + strlen(name) + 3;
#ifdef MODDIR_SUFFIX
	/* sizeof includes null */
	maxpathlen += sizeof (slash_moddir_suffix_slash) - 1;
#endif
	fullname = kobj_zalloc(maxpathlen, KM_WAIT);

	for (;;) {
		p = fullname;
		if (*pathp != '\0' && *pathp != '/')
			*p++ = '/';	/* path must start with '/' */
		while (*pathp && *pathp != ':' && *pathp != ' ')
			*p++ = *pathp++;
		if (p != fullname && p[-1] != '/')
			*p++ = '/';
#ifdef MODDIR_SUFFIX
		{
		char *b = basename(name);
		char *s;

		/* copy everything up to the base name */
		q = name;
		while (q != b && *q)
			*p++ = *q++;
		s = slash_moddir_suffix_slash;
		while (*s)
			*p++ = *s++;
		/* copy the rest */
		while (*b)
			*p++ = *b++;
		}
#else
		q = name;
		while (*q)
			*p++ = *q++;
#endif
		*p = 0;
		if ((file = kobj_open_file(fullname)) != (struct _buf *)-1) {
			kobj_free(fullname, maxpathlen);
			return (file);
		}
		if (*pathp == 0)
			break;
		pathp++;
	}
	kobj_free(fullname, maxpathlen);
	if (_moddebug & MODDEBUG_ERRMSG) {
		_kobj_printf(ops, "can't open %s,", name);
		_kobj_printf(ops, " path is %s\n", pathpsave);
	}
	return ((struct _buf *)-1);
}

intptr_t
kobj_open(char *filename)
{
	struct vnode *vp;
	int fd, s;

	if (_modrootloaded) {
		struct kobjopen_tctl *ltp = kobjopen_alloc(filename);
		cred_t *saved_cred;
		int errno;

		/*
		 * Hand off the open to a thread who has a
		 * stack size capable handling the request.
		 */
		if (curthread != &t0 && thread_create(NULL, DEFAULTSTKSZ * 2,
		    kobjopen_thread, (caddr_t)ltp, 0, &p0, TS_RUN,
		    MAXCLSYSPRI) != NULL) {
			sema_p(&ltp->sema);
			errno = ltp->errno;
			vp = ltp->vp;
		} else {
			/*
			 * 1098067: module creds should not be those of the
			 * caller
			 */
			saved_cred = curthread->t_cred;
			curthread->t_cred = kcred;
			errno = vn_open(filename, UIO_SYSSPACE, FREAD, 0, &vp,
			    0, 0);
			curthread->t_cred = saved_cred;
		}
		kobjopen_free(ltp);

		if (errno) {
			if (_moddebug & MODDEBUG_ERRMSG) {
				_kobj_printf(ops,
				    "kobj_open: vn_open of %s fails, ",
				    filename);
				_kobj_printf(ops, "errno = %d\n", errno);
			}
			return (-1);
		} else {
			if (_moddebug & MODDEBUG_ERRMSG) {
				_kobj_printf(ops, "kobj_open: '%s'", filename);
				_kobj_printf(ops, " vp = %p\n", vp);
			}
			return ((intptr_t)vp);
		}
	} else {
		/*
		 * If the bootops are nil, it means boot is no longer
		 * available to us. So we make it look as if we can't
		 * open the named file - which is reasonably accurate.
		 */
		fd = -1;

		if (standalone) {
			fd = BOP_OPEN(ops, filename, 0);
		} else if (_bootops) {
			s = splimp();
			fd = BOP_OPEN(ops, filename, 0);
			splx(s);
		}
		if (_moddebug & MODDEBUG_ERRMSG) {
			if (fd < 0)
				_kobj_printf(ops,
				    "kobj_open: can't open %s\n",
				    filename);
			else {
				_kobj_printf(ops, "kobj_open: '%s'", filename);
				_kobj_printf(ops, " descr = 0x%x\n", fd);
			}
		}
		return ((intptr_t)fd);
	}
}

/*
 * Calls to kobj_open() are handled off to this routine as a separate thread.
 */
static void
kobjopen_thread(struct kobjopen_tctl *ltp)
{
	kmutex_t	cpr_lk;
	callb_cpr_t	cpr_i;

	mutex_init(&cpr_lk, NULL, MUTEX_DEFAULT, NULL);
	CALLB_CPR_INIT(&cpr_i, &cpr_lk, callb_generic_cpr, "kobjopen");
	ltp->errno = vn_open(ltp->name, UIO_SYSSPACE, FREAD, 0, &(ltp->vp),
									0, 0);
	sema_v(&ltp->sema);
	mutex_enter(&cpr_lk);
	CALLB_CPR_EXIT(&cpr_i);
	mutex_destroy(&cpr_lk);
	thread_exit();
}

/*
 * allocate and initialize a kobjopen thread structure
 */
static struct kobjopen_tctl *
kobjopen_alloc(char *filename)
{
	struct kobjopen_tctl *ltp = kmem_zalloc(sizeof (*ltp), KM_SLEEP);

	ASSERT(filename != NULL);

	ltp->name = kmem_alloc(strlen(filename) + 1, KM_SLEEP);
	bcopy(filename, ltp->name, strlen(filename) + 1);
	sema_init(&ltp->sema, 0, NULL, SEMA_DEFAULT, NULL);
	return (ltp);
}

/*
 * free a kobjopen thread control structure
 */
static void
kobjopen_free(struct kobjopen_tctl *ltp)
{
	sema_destroy(&ltp->sema);
	kmem_free(ltp->name, strlen(ltp->name) + 1);
	kmem_free(ltp, sizeof (*ltp));
}

int
kobj_read(intptr_t descr, char *buf, unsigned size, unsigned offset)
{
	int stat;
	ssize_t resid;

	if (_modrootloaded) {
		if ((stat = vn_rdwr(UIO_READ, (struct vnode *)descr, buf, size,
		    (offset_t)offset, UIO_SYSSPACE, 0, (rlim64_t)0, CRED(),
		    &resid)) != 0) {
			_kobj_printf(ops,
			    "vn_rdwr failed with error 0x%x\n", stat);
			return (-1);
		}
		return (size - resid);
	} else {
		int s;
		int count;

		if (!standalone)
			s = splimp();
		count = 0;

		if (BOP_SEEK(ops, (int)descr, (off_t)0, offset) != 0) {
			_kobj_printf(ops,
			    "kobj_read: seek 0x%x failed\n", offset);
			count = -1;
			goto out;
		}

		count = BOP_READ(ops, (int)descr, buf, size);
		if (count < size) {
			if (_moddebug & MODDEBUG_ERRMSG) {
				_kobj_printf(ops,
				    "kobj_read: req %d bytes, ", size);
				_kobj_printf(ops, "got %d\n", count);
			}
		}
out:
		if (!standalone)
			splx(s);
		return (count);
	}
}

void
kobj_close(intptr_t descr)
{
	if (_moddebug & MODDEBUG_ERRMSG)
		_kobj_printf(ops, "kobj_close: 0x%lx\n", descr);

	if (_modrootloaded) {
		struct vnode *vp = (struct vnode *)descr;
		(void) VOP_CLOSE(vp, FREAD, 1, (offset_t)0, CRED());
		VN_RELE(vp);
	} else if (standalone || _bootops)
		(void) BOP_CLOSE(ops, (int)descr);
}

struct _buf *
kobj_open_file(char *name)
{
	struct _buf *file;

	file = kobj_zalloc(sizeof (struct _buf), KM_WAIT);
	file->_name = kobj_alloc(strlen(name)+1, KM_WAIT);
	file->_base = kobj_zalloc(MAXBSIZE, KM_WAIT|KM_TEMP);

	if ((file->_fd = kobj_open(name)) == -1) {
		kobj_free(file->_base, MAXBSIZE);
		kobj_free(file->_name, strlen(name)+1);
		kobj_free(file, sizeof (struct _buf));
		return ((struct _buf *)-1);
	}
	file->_cnt = file->_size = file->_off = 0;
	file->_ln = 1;
	file->_ptr = file->_base;
	(void) strcpy(file->_name, name);
	return (file);
}

void
kobj_close_file(struct _buf *file)
{
	kobj_close(file->_fd);
	kobj_free(file->_base, MAXBSIZE);
	kobj_free(file->_name, strlen(file->_name)+1);
	kobj_free(file, sizeof (struct _buf));
}

int
kobj_read_file(struct _buf *file, char *buf, unsigned size, unsigned off)
{
	int b_size, c_size;
	int b_off;	/* Offset into buffer for start of bcopy */
	int count = 0;
	int page_addr;

	if (_moddebug & MODDEBUG_ERRMSG) {
		_kobj_printf(ops, "kobj_read_file: size=%x,", size);
		_kobj_printf(ops, " offset=%x\n", off);
	}

	while (size) {
		page_addr = F_PAGE(off);
		b_size = file->_size;
		/*
		 * If we have the filesystem page the caller's referring to
		 * and we have something in the buffer,
		 * satisfy as much of the request from the buffer as we can.
		 */
		if (page_addr == file->_off && b_size > 0) {
			b_off = B_OFFSET(off);
			c_size = b_size - b_off;
			/*
			 * If there's nothing to copy, we're at EOF.
			 */
			if (c_size <= 0)
				break;
			if (c_size > size)
				c_size = size;
			if (buf) {
				if (_moddebug & MODDEBUG_ERRMSG)
					_kobj_printf(ops, "copying %x bytes\n",
					    c_size);
				bcopy(file->_base+b_off, buf, c_size);
				size -= c_size;
				off += c_size;
				buf += c_size;
				count += c_size;
			} else {
				_kobj_printf(ops, "kobj_read: system error");
				count = -1;
				break;
			}
		} else {
			/*
			 * If the caller's offset is page aligned and
			 * the caller want's at least a filesystem page and
			 * the caller provided a buffer,
			 * read directly into the caller's buffer.
			 */
			if (page_addr == off &&
			    (c_size = F_PAGE(size)) && buf) {
				c_size = kobj_read(file->_fd, buf, c_size,
					page_addr);
				if (c_size < 0) {
					count = -1;
					break;
				}
				count += c_size;
				if (c_size != F_PAGE(size))
					break;
				size -= c_size;
				off += c_size;
				buf += c_size;
			/*
			 * Otherwise, read into our buffer and copy next time
			 * around the loop.
			 */
			} else {
				file->_off = page_addr;
				c_size = kobj_read(file->_fd, file->_base,
						MAXBSIZE, page_addr);
				file->_ptr = file->_base;
				file->_cnt = c_size;
				file->_size = c_size;
				/*
				 * If a _filbuf call or nothing read, break.
				 */
				if (buf == NULL || c_size <= 0) {
					count = c_size;
					break;
				}
			}
			if (_moddebug & MODDEBUG_ERRMSG)
				_kobj_printf(ops, "read %x bytes\n", c_size);
		}
	}
	if (_moddebug & MODDEBUG_ERRMSG)
		_kobj_printf(ops, "count = %x\n", count);

	return (count);
}

int
kobj_filbuf(struct _buf *f)
{
	if (kobj_read_file(f, NULL, MAXBSIZE, f->_off + f->_size) > 0)
		return (kobj_getc(f));
	return (-1);
}

int
map_setup(void)
{
	struct module *mp;
	char *old;
	struct modctl *modp;

	if (kobj_map_list)
		return (0);

	if ((_moddebug & MODDEBUG_LOCKSYMBOLS) ||
	    ((boothowto & RB_DEBUG) &&
	    (_moddebug & MODDEBUG_UNLOCKSYMBOLS) == 0)) {
		return (-1);
	}

	/*
	 * Normally if KADB is present we lock down all symbol tables
	 * and if KADB is not present we unlock all symbol tables.
	 *
	 * There are reasons why users may want to override the default.
	 * One reason is that if the system is not boot with KADB and
	 * keeps crashing for some reason, symbols may not be in
	 * the crash dump unless the symbols are locked down.
	 * Another reason is that locking down symbols may make the
	 * system behave differently than if they are not locked down.
	 * So you may want to boot with KADB but not lock down symbols.
	 *
	 * There are two flags in moddebug to control the locking of symbols.
	 * MODDEBUG_LOCKSYMBOLS forces symbols to be locked down regardless
	 * of the presence of KADB.  If this flag is not set, then
	 * MODDEBUG_UNLOCKSYMBOLS can be set to unlock symbols even when
	 * KADB is present.
	 */


	/*
	 * copy initial symbol tables from kmem_zalloc memory to
	 * kobj_map.
	 */
	modp = &modules;
	do {
		if ((mp = modp->mod_mp) != NULL && mp->symspace != 0 &&
		    (mp->flags & KOBJ_SYMKMEM)) {
			old = mp->symspace;
			mp->symspace = kobj_map_alloc(mp->symsize);
			(void) kcopy(old, mp->symspace, mp->symsize);
			mp->flags |= KOBJ_SYMSWAP;
			mp->flags &= ~KOBJ_SYMKMEM;

			mp->symtbl = mp->symspace;
			mp->strings = mp->symspace + mp->symhdr->sh_size;

			mp->buckets = (symid_t *)
				((((uintptr_t)mp->strings +
				mp->strhdr->sh_size) |
				(sizeof (symid_t) - 1)) + 1);
			mp->chains = mp->buckets + mp->hashsize;
			kobj_free(old, mp->symsize);
		}
		modp = modp->mod_next;
	} while (modp != &modules);
	return (0);
}

/*
 * Allocate memory from the kobj symbol map, adding a new map if
 * necessary.
 */
static void *
kobj_map_alloc(size_t size)
{
	kobj_map_t *kmp;
	char *base;
	void *addr;
	int flags = 0;

	mutex_enter(&kobj_map_lock);
	for (kmp = kobj_map_list; kmp != NULL; kmp = kmp->next) {
		if ((addr = (void *)rmalloc(kmp->map, size)) != NULL) {
			mutex_exit(&kobj_map_lock);
			return (addr);
		}
	}

	/*
	 * Arrange to lock down the symbol tables if requested.
	 */
	if (kobj_locked)
		flags = KPD_LOCKED;
	if ((base = segkp_get(segkp, kobj_map_space_len, flags)) == NULL) {
		_kobj_printf(ops,
		    "can't allocate address space for kobj symbols\n");
		mutex_exit(&kobj_map_lock);
		return (NULL);
	}

	kmp = kobj_alloc(sizeof (kobj_map_t), KM_WAIT);
	kmp->base = base;
	kmp->len = kobj_map_space_len;
	kmp->map = kobj_zalloc(sizeof (struct map) * KOBJ_MAP_SEGMENTS,
	    KM_WAIT);
	mapinit(kmp->map, kobj_map_space_len,
	    (u_long)kmp->base, "kobj_map", KOBJ_MAP_SEGMENTS);
	kmp->next = kobj_map_list;
	kobj_map_list = kmp;
	addr = (void *)rmalloc_wait(kmp->map, size);
	mutex_exit(&kobj_map_lock);
	return (addr);
}

/*
 * Free space in the kobj symbol maps.
 */
static void
kobj_map_free(void *addr, size_t size)
{
	kobj_map_t *kmp;

	mutex_enter(&kobj_map_lock);
	kmp = kobj_map_list;
	/* search until we find the right map */
	while ((caddr_t)addr < kmp->base ||
	    (caddr_t)addr >= kmp->base + kmp->len) {
		kmp = kmp->next;
		ASSERT(kmp != NULL);
	}
	rmfree(kmp->map, size, (u_long)addr);
	mutex_exit(&kobj_map_lock);
}

void
kobj_free(void *address, size_t size)
{
	if (standalone)
		return;

	kmem_free(address, size);
	kobj_stat.nfree_calls++;
	kobj_stat.nfree += size;
}

void *
kobj_zalloc(size_t size, int flag)
{
	void *v;

	if ((v = kobj_alloc(size, flag)) != 0)
		bzero(v, size);

	return (v);
}

void *
kobj_alloc(size_t size, int flag)
{
	/*
	 * If we are running standalone in the
	 * linker, we ask boot for memory.
	 * Either it's temporary memory that we lose
	 * once boot is mapped out or we allocate it
	 * permanently using the dynamic data segment.
	 */
	if (standalone) {
#ifdef BOOTSCRATCH
		if (flag & KM_TEMP)
			return (BOP_ALLOC(ops, (caddr_t)0,
			    roundup(size, mmu_pagesize), BO_NO_ALIGN));
		else
#endif
			return (segbrk(&_edata, size, MINALIGN, 0));
	}

	kobj_stat.nalloc_calls++;
	kobj_stat.nalloc += size;

	return (kmem_alloc(size, (flag & KM_NOWAIT) ? KM_NOSLEEP : KM_SLEEP));
}

/*
 * Allow the "mod" system to sync up with the work
 * already done by kobj during the initial loading
 * of the kernel.  This also gives us a chance
 * to reallocate memory that belongs to boot.
 */
void
kobj_sync(void)
{
	struct modctl_list *lp;
	struct module *mp;
	extern char *default_path;

	/*
	 * module_path can be set in /etc/system
	 */
	if (default_path != NULL)
		module_path = default_path;
	else
		default_path = module_path;

	/*
	 * Shuffle symbol tables from boot memory
	 * to kernel memory.
	 */
	for (lp = primaries; lp; lp = lp->modl_next) {
		caddr_t old;

		mp = mod(lp);
		if (mp->symspace < (char *)KERNELBASE) {
			/*
			 * Copy the pieces individually since
			 * they were set up by ld rather than us.
			 */
			if (mp->flags & KOBJ_EXEC) {
				caddr_t p;

				mp->symsize = mp->symhdr->sh_size +
					mp->strhdr->sh_size + sizeof (int) +
					mp->hashsize * sizeof (symid_t) +
					mp->nsyms * sizeof (symid_t);
				mp->symsize = (mp->symsize + sizeof (long)) &
				    ~(sizeof (long) - 1);
				p = kobj_zalloc(mp->symsize, KM_WAIT);
				mp->symspace = p;

				bcopy(mp->symtbl, p, mp->symhdr->sh_size);
				p += mp->symhdr->sh_size;

				bcopy(mp->strings, p, mp->strhdr->sh_size);
				p = (caddr_t)
					((((uintptr_t)p + mp->strhdr->sh_size) |
					(sizeof (symid_t) - 1)) + 1);

				bcopy(mp->buckets, p,
					mp->hashsize * sizeof (symid_t));
				p += mp->hashsize * sizeof (symid_t);

				bcopy(mp->chains, p,
					mp->nsyms * sizeof (symid_t));
			} else {
				old = mp->symspace;
				mp->symspace = kobj_alloc(mp->symsize,
				    KM_WAIT);
				bcopy(old, mp->symspace, mp->symsize);
			}
			mp->symtbl = mp->symspace;
			mp->strings = mp->symspace + mp->symhdr->sh_size;
			mp->buckets = (symid_t *)((((uintptr_t)mp->strings +
				mp->strhdr->sh_size) |
				(sizeof (symid_t) - 1)) + 1);
			mp->chains = mp->buckets + mp->hashsize;
			mp->flags |= KOBJ_SYMKMEM;
		}
	}
}

static caddr_t
segbrk(caddr_t *spp, size_t size, size_t align, caddr_t limit)
{
	uintptr_t va, pva;
	u_int alloc_pgsz = mmu_pagesize;
	u_int alloc_align = BO_NO_ALIGN;
	u_int alloc_size;

	/*
	 * If we are using "large" mappings for the kernel,
	 * request aligned memory from boot using the
	 * "large" pagesize.
	 */
	if (lg_pagesize) {
		alloc_align = lg_pagesize;
		alloc_pgsz = lg_pagesize;
	}
	va = ALIGN((uintptr_t)*spp, align);
	pva = roundup((uintptr_t)*spp, alloc_pgsz);
	/*
	 * Need more pages?
	 */
	if (va + size > pva) {
		alloc_size = roundup(size - (pva - va), alloc_pgsz);
		/*
		 * Check for overlapping segments.
		 */
		if (limit && limit <= *spp + alloc_size)
			return ((caddr_t)0);

		pva = (uintptr_t)BOP_ALLOC(ops, (caddr_t)pva,
					alloc_size, alloc_align);
		if (pva == NULL) {
			_kobj_printf(ops, "BOP_ALLOC refused, 0x%x bytes ",
			    alloc_size);
			_kobj_printf(ops, " at 0x%lx\n", pva);
		}
	}
	*spp = (caddr_t)(va + size);

	return ((caddr_t)va);
}

/*
 * Calculate the number of output hash buckets.
 * We use the next prime larger than n / 4,
 * so the average hash chain is about 4 entries.
 * More buckets would just be a waste of memory.
 */
static u_int
gethashsize(u_int n)
{
	int f;
	int hsize = MAX(n / 4, 2);

	for (f = 2; f * f <= hsize; f++)
		if (hsize % f == 0)
			hsize += f = 1;

	return (hsize);
}

static char *
basename(char *s)
{
	char *p, *q;

	q = NULL;
	p = s;
	do {
		if (*p == '/')
			q = p;
	} while (*p++);
	return (q ? q + 1 : s);
}

/*ARGSUSED*/
static void
kprintf(void *op, const char *fmt, ...)
{
	va_list adx;

	va_start(adx, fmt);
	modvprintf(fmt, adx);
	va_end(adx);
}

int
kobj_lock_syms(void)
{
	/*
	 * If kobj_map space in segkp is not yet allocated, just increment
	 * the counter, so it will be locked when allocated (by map_setup).
	 */
	mutex_enter(&kobj_map_lock);
	if (kobj_map_list == NULL) {
		++kobj_locked;
		mutex_exit(&kobj_map_lock);
		return (0);
	}

	if (kobj_locked++ == 0) {
		kobj_map_t *kmp;
		int rv;

		mutex_enter(&kobj_map_lock);
		for (kmp = kobj_map_list; kmp != NULL; kmp = kmp->next) {
			rv = (int)as_fault(kas.a_hat, &kas, kmp->base,
			    kmp->len, F_SOFTLOCK, S_OTHER);
			if (rv) {
				mutex_exit(&kobj_map_lock);
				return (rv);
			}
		}
	}
	mutex_exit(&kobj_map_lock);
	return (0);
}

int
kobj_unlock_syms(void)
{
	mutex_enter(&kobj_map_lock);
	if (kobj_map_list == NULL) {
		--kobj_locked;
		mutex_exit(&kobj_map_lock);
		return (0);
	}

	if (--kobj_locked == 0) {
		kobj_map_t *kmp;
		int rv;

		for (kmp = kobj_map_list; kmp != NULL; kmp = kmp->next) {
			rv = (int)as_fault(kas.a_hat, &kas, kmp->base,
			    kmp->len, F_SOFTUNLOCK, S_OTHER);
			if (rv) {
				mutex_exit(&kobj_map_lock);
				return (rv);
			}
		}
	}
	mutex_exit(&kobj_map_lock);
	return (0);
}

void
kobj_stat_get(kobj_stat_t *kp)
{
	*kp = kobj_stat;
}

kobj_free_t *
kobj_create_managed()
{
	kobj_free_t *ret;

	ret = kobj_alloc(sizeof (kobj_free_t), KM_WAIT);
	ret->next = ret->prev = ret;
	ret->mem = NULL;
	ret->size = 0;
	return (ret);
}


/*
 * kobj module memory is managed in one of two ways.
 * Either it is coalesced with adjacent wads into a bigger wad
 * or it just stuffed on the linked list if there are no adjacent
 * wads with which to mate.
 */
void
kobj_manage_mem(caddr_t addr, size_t size, kobj_free_t *kfree_headp)
{
	kobj_free_t *lp, *wp = kfree_headp->next;

	lp = kobj_alloc(sizeof (kobj_free_t), KM_WAIT);
	lp->mem = addr;
	lp->size = size;

	while (wp != kfree_headp) {
		if (lp->mem + size == wp->mem) {
			lp->size += (wp->mem - (lp->mem + lp->size)) +
				wp->size;
			ASSERT((int)lp->size > 0);
			wp->next->prev = wp->prev;
			wp->prev->next = wp->next;
			kobj_free(wp, sizeof (kobj_free_t));
			break;
		}
		if (wp->mem + wp->size == lp->mem) {
			wp->size += (lp->mem - (wp->mem + wp->size)) +
				lp->size;
			ASSERT((int)wp->size > 0);
			wp->next->prev = wp->prev;
			wp->prev->next = wp->next;
			lp->size = wp->size;
			lp->mem = wp->mem;
			kobj_free(wp, sizeof (kobj_free_t));
			break;
		}
		wp = wp->next;
	}

	wp = kfree_headp->next;
	while (wp != kfree_headp && ((lp->size > wp->size))) {
		ASSERT(addr != wp->mem);  /* freeing free mem */
		wp = wp->next;
	}

	lp->next = wp;
	lp->prev = wp->prev;
	lp->prev->next = lp;
	lp->next->prev = lp;
}

/*
 * find memory on our list that is at least size big.
 * If we find one that is big enough we break it up
 * and return that address back to the calling client.
 * The leftover piece of that memory is returned to the
 * freelist.
 */
caddr_t
kobj_find_managed_mem(size_t size, kobj_free_t *kfree_headp)
{
	kobj_free_t *wp = kfree_headp->next;
	caddr_t mem = NULL;

	while (wp != kfree_headp && size > wp->size)
		wp = wp->next;

	if (wp == kfree_headp) /* empty list or nothing big enough */
		return (NULL);

	if (size < wp->size) {  /* breaking up a chunk of memory */
		kobj_free_t np;

		wp->prev->next = wp->next;
		wp->next->prev = wp->prev;

		np.size = wp->size - size;
		np.mem = wp->mem + size;

		kobj_manage_mem(np.mem, np.size, kfree_headp);
	} else {
		wp->prev->next = wp->next;
		wp->next->prev = wp->prev;
	}

	mem = wp->mem;

	kobj_free(wp, sizeof (kobj_free_t));

	return (mem);
}

int
kobj_getpagesize()
{
	return (lg_pagesize);
}

/*
 * kobj_lock is used to protect the mach specific allocation
 * pool and the krtld managed internal lists that represent
 * that pool. Those functions are kobj_find_managed_mem and
 * kobj_manage_mem.
 */
static void
kobj_mod_free(void *address, size_t size, int flags)
{
	if (flags) {
		mutex_enter(&kobj_lock);
		mach_mod_free(address, size, flags);
		mutex_exit(&kobj_lock);
	} else {
		kmem_backend_free(kmem_kobj_backend, address, size);
	}
}

static void *
kobj_mod_alloc(struct module *mp, size_t size, int flags, reloc_dest_t *dest)
{
	void *modptr;

	mutex_enter(&kobj_lock);
	modptr = mach_mod_alloc(size, flags, dest);
	mutex_exit(&kobj_lock);

	if (modptr) {
		mp->flags |= flags;
	} else {
		modptr = kmem_backend_alloc(kmem_kobj_backend, size, KM_SLEEP);
		if (dest)
			*dest = NULL;
	}

	return (modptr);
}
/*
 * Check for $MACRO in tail (string to expand) and expand it in path at pathend
 * returns path if successful, else NULL
 * Support multiple $MACROs expansion and the first valid path will be returned
 * Caller's responsibility to provide enough space in path to expand
 */
char *
expand_libmacro(char *tail, char *path, char *pathend)
{
	char c, *p, *p1, *p2, *path2, *endp;
	int diff, lmi, macrolen, valid_macro, more_macro;
	struct _buf *file;

	/*
	 * check for $MACROS between nulls or slashes
	 */
	p = strchr(tail, '$');
	if (p == NULL)
		return (NULL);
	for (lmi = 0; lmi < NLIBMACROS; lmi++) {
		macrolen = libmacros[lmi].lmi_macrolen;
		if (strncmp(p + 1, libmacros[lmi].lmi_macroname, macrolen) == 0)
			break;
	}

	valid_macro = 0;
	if (lmi < NLIBMACROS) {
		/*
		 * The following checks are used to restrict expansion of
		 * macros to those that form a full directory/file name
		 * and to keep the behavior same as before.  If this
		 * restriction is removed or no longer valid in the future,
		 * the checks below can be deleted.
		 */
		if ((p == tail) || (*(p - 1) == '/')) {
			c = *(p + macrolen + 1);
			if (c == '/' || c == '\0')
				valid_macro = 1;
		}
	}

	if (! valid_macro) {
		p2 = strchr(p, '/');
		/*
		 * if no more macro to expand, then just copy whatever left
		 * and check whether it exists
		 */
		if (p2 == NULL || strchr(p2, '$') == NULL) {
			(void) strcpy(pathend, tail);
			if ((file = kobj_open_path(path, 1)) !=
			    (struct _buf *)-1) {
				kobj_close_file(file);
				return (path);
			} else
				return (NULL);
		} else {
			/*
			 * copy all chars before '/' and call expand_libmacro()
			 * again
			 */
			diff = p2 - tail;
			bcopy(tail, pathend, diff);
			pathend += diff;
			*(pathend) = '\0';
			return (expand_libmacro(p2, path, pathend));
		}
	}

	more_macro = 0;
	if (c != '\0') {
		endp = p + macrolen + 1;
		if (strchr(endp, '$') != NULL)
			more_macro = 1;
	} else
		endp = NULL;

	/*
	 * copy lmi_list and split it into components.
	 * then put the part of tail before $MACRO into path
	 * at pathend
	 */
	diff = p - tail;
	if (diff > 0)
		bcopy(tail, pathend, diff);
	path2 = pathend + diff;
	p1 = libmacros[lmi].lmi_list;
	while (*p1 != '\0') {
		p2 = strchr(p1, ':');
		if (p2) {
			diff = p2 - p1;
			bcopy(p1, path2, diff);
			*(path2 + diff) = '\0';
		} else {
			diff = strlen(p1);
			bcopy(p1, path2, diff + 1);
		}
		/* copy endp only if there isn't any more macro to expand */
		if (! more_macro && (endp != NULL))
			(void) strcat(path2, endp);
		if ((file = kobj_open_path(path, 1)) != (struct _buf *)-1) {
			kobj_close_file(file);
			/*
			 * if more macros to expand then call expand_libmacro(),
			 * else return path which has the whole path
			 */
			if (! more_macro || (expand_libmacro(endp, path,
				path2 + diff) != NULL)) {
				return (path);
			}
		}
		if (p2)
			p1 = ++p2;
		else
			return (NULL);
	}
	return (NULL);
}
