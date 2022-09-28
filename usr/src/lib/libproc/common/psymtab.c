/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)Psymtab.c	1.1	97/12/23 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <string.h>
#include <memory.h>
#include <errno.h>
#include <dirent.h>
#include <signal.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "Pcontrol.h"

/* XXX A bug in the <string.h> header file requires this */
extern char *strtok_r(char *s1, const char *s2, char **lasts);

static file_info_t *build_map_symtab(struct ps_prochandle *, map_info_t *);
static void build_file_symtab(struct ps_prochandle *, file_info_t *);
static map_info_t *addr_to_map(struct ps_prochandle *, uintptr_t);
static map_info_t *exec_map(struct ps_prochandle *);
static map_info_t *object_to_map(struct ps_prochandle *, const char *);
static map_info_t *object_name_to_map(struct ps_prochandle *, const char *);
static void readauxvec(struct ps_prochandle *);

#if 0		/* debugging */
static void
mdump(map_info_t *mptr, char *lab)
{
	file_info_t *fptr;

	(void) fprintf(stderr,
		"%s: map_file = 0x%p\n", lab, (void *)mptr->map_file);
	if ((fptr = mptr->map_file) == NULL) {
		(void) fprintf(stderr, "\n");
		return;
	}
	(void) fprintf(stderr,
		"file_lname = %s\n",
		fptr->file_lname? fptr->file_lname : "NULL");
	(void) fprintf(stderr,
		"file_ref = %d, file_fd = %d, file_init = %d file_elf = 0x%p\n",
		fptr->file_ref, fptr->file_fd, fptr->file_init,
		(void *)fptr->file_elf);
	(void) fprintf(stderr, "\n");
}
#endif

/*
 * Linked list manipulation.
 */
static void
Link(void *new, void *existing)
{
	list_t *p = (list_t *)new;
	list_t *q = (list_t *)existing;

	if (q) {
		p->list_forw = q;
		p->list_back = q->list_back;
		q->list_back->list_forw = p;
		q->list_back = p;
	} else {
		p->list_forw = p->list_back = p;
	}
}

static void
UnLink(void *old)
{
	list_t *p = (list_t *)old;

	if (p->list_forw != p) {
		p->list_back->list_forw = p->list_forw;
		p->list_forw->list_back = p->list_back;
	}
	p->list_forw = p->list_back = p;
}

#define	Next(elem)	(void *)(((list_t *)(elem))->list_forw)
#define	Prev(elem)	(void *)(((list_t *)(elem))->list_back)

/*
 * Allocation function for a new file_info_t
 */
static file_info_t *
file_info_new(struct ps_prochandle *P, map_info_t *mptr)
{
	file_info_t *fptr = malloc(sizeof (file_info_t));
	map_info_t *mp;
	int i;

	(void) memset(fptr, 0, sizeof (file_info_t));
	Link(fptr, &P->file_head);
	(void) strcpy(fptr->file_pname, mptr->map_pmap.pr_mapname);
	mptr->map_file = fptr;
	fptr->file_ref = 1;
	fptr->file_fd = -1;
	P->num_files++;

	/*
	 * Attach the new file info struct to all matching maps.
	 */
	for (i = 0, mp = Next(&P->map_head); i < P->num_mappings;
	    i++, mp = Next(mp)) {
		if (mp->map_pmap.pr_mapname[0] != '\0' &&
		    mp->map_file == NULL &&
		    strcmp(fptr->file_pname, mp->map_pmap.pr_mapname) == 0) {
			mp->map_file = fptr;
			fptr->file_ref++;
		}
	}

	return (fptr);
}

/*
 * Deallocation function for a file_info_t
 */
static void
file_info_free(struct ps_prochandle *P, file_info_t *fptr)
{
	if (--fptr->file_ref == 0) {
		UnLink(fptr);
		if (fptr->file_lo)
			free(fptr->file_lo);
		if (fptr->file_lname)
			free(fptr->file_lname);
		if (fptr->file_elf)
			(void) elf_end(fptr->file_elf);
		if (fptr->file_fd >= 0)
			(void) close(fptr->file_fd);
		free(fptr);
		P->num_files--;
	}
}

/*
 * Allocation function for a new map_info_t
 */
static map_info_t *
map_info_new(struct ps_prochandle *P, prmap_t *pmap)
{
	map_info_t *mptr = malloc(sizeof (map_info_t));

	(void) memset(mptr, 0, sizeof (map_info_t));
	mptr->map_pmap = *pmap;
	mptr->map_file = NULL;
	P->num_mappings++;
	return (mptr);
}

/*
 * Deallocation function for a map_info_t
 */
static void
map_info_free(struct ps_prochandle *P, map_info_t *mptr)
{
	file_info_t *fptr;

	UnLink(mptr);
	if ((fptr = mptr->map_file) != NULL) {
		if (fptr->file_map == mptr)
			fptr->file_map = NULL;
		file_info_free(P, fptr);
	}
	if (P->execname && mptr == P->map_exec) {
		free(P->execname);
		P->execname = NULL;
	}
	if (P->auxv && (mptr == P->map_exec || mptr == P->map_ldso)) {
		free(P->auxv);
		P->auxv = NULL;
	}
	if (mptr == P->map_exec)
		P->map_exec = NULL;
	if (mptr == P->map_ldso)
		P->map_ldso = NULL;
	free(mptr);
	P->num_mappings--;
}

/*
 * Call-back function for librtld_db to iterate through all of its shared
 * libraries.  We use this to get the load object names for the mappings.
 */
static int
map_iter(const rd_loadobj_t *lop, void *cd)
{
	char buf[PATH_MAX];
	struct ps_prochandle *P = cd;
	map_info_t *mptr;
	file_info_t *fptr;

	if ((mptr = addr_to_map(P, lop->rl_base)) == NULL)
		return (1);

	if ((fptr = mptr->map_file) == NULL)
		fptr = file_info_new(P, mptr);
	fptr->file_map = mptr;

	if (fptr->file_lo)
		free(fptr->file_lo);
	fptr->file_lo = malloc(sizeof (rd_loadobj_t));
	*fptr->file_lo = *lop;

	if (fptr->file_lname)
		free(fptr->file_lname);
	if (proc_read_string(P->asfd, buf, sizeof (buf),
	    lop->rl_nameaddr) > 0)
		fptr->file_lname = strdup(buf);
	else
		fptr->file_lname = NULL;

	return (1);
}

/*
 * Go through all the address space mappings, validating or updating
 * the information already gathered, or gathering new information.
 */
void
proc_update_maps(struct ps_prochandle *P)
{
	char mapfile[64];
	int mapfd;
	struct stat statb;
	prmap_t *Pmap = NULL;
	prmap_t *pmap;
	ssize_t nmap;
	int i;
	map_info_t *mptr;
	map_info_t *nextptr;
	u_int oldmapcnt;
	int anychange = FALSE;

	if (P->info_valid)
		return;

	readauxvec(P);

	(void) sprintf(mapfile, "/proc/%d/map", (int)P->pid);
	if ((mapfd = open(mapfile, O_RDONLY)) < 0 ||
	    fstat(mapfd, &statb) != 0 ||
	    statb.st_size < sizeof (prmap_t) ||
	    (Pmap = malloc(statb.st_size)) == NULL ||
	    (nmap = pread(mapfd, Pmap, statb.st_size, 0L)) <= 0 ||
	    (nmap /= sizeof (prmap_t)) == 0) {
		if (Pmap != NULL)
			free(Pmap);
		if (mapfd >= 0)
			(void) close(mapfd);
		proc_reset_maps(P);	/* utter failure; destroy tables */
		return;
	}
	(void) close(mapfd);

	/*
	 * Merge the new mappings with the old mapping list.
	 * The map elements are maintained in order, sorted by address.
	 * /proc/<pid>/map returns the maps sorted by address.
	 */
	mptr = Next(&P->map_head);
	oldmapcnt = P->num_mappings;
	for (i = 0, pmap = Pmap; i < nmap; i++, pmap++) {
		if (oldmapcnt == 0) {	/* we ran out of old mappings */
			if (pmap->pr_mapname[0] != '\0')
				anychange = TRUE;
			Link(map_info_new(P, pmap), &P->map_head);
			continue;
		}
		if (pmap->pr_vaddr == mptr->map_pmap.pr_vaddr &&
		    pmap->pr_size == mptr->map_pmap.pr_size &&
		    pmap->pr_offset == mptr->map_pmap.pr_offset &&
		    pmap->pr_mflags == mptr->map_pmap.pr_mflags &&
		    pmap->pr_pagesize == mptr->map_pmap.pr_pagesize &&
		    pmap->pr_shmid == mptr->map_pmap.pr_shmid &&
		    strcmp(pmap->pr_mapname, mptr->map_pmap.pr_mapname) == 0) {
			/*
			 * The map elements match, no change.
			 */
			mptr = Next(mptr);
			oldmapcnt--;
			continue;
		}
		if (oldmapcnt &&
		    pmap->pr_vaddr + pmap->pr_size > mptr->map_pmap.pr_vaddr) {
			if (mptr->map_pmap.pr_mapname[0] != '\0')
				anychange = TRUE;
			nextptr = Next(mptr);
			map_info_free(P, mptr);
			mptr = nextptr;
			oldmapcnt--;
			i--;
			pmap--;
			continue;
		}
		if (pmap->pr_mapname[0] != '\0')
			anychange = TRUE;
		Link(map_info_new(P, pmap), mptr);
	}
	while (oldmapcnt) {
		if (mptr->map_pmap.pr_mapname[0] != '\0')
			anychange = TRUE;
		nextptr = Next(mptr);
		map_info_free(P, mptr);
		mptr = nextptr;
		oldmapcnt--;
	}

	free(Pmap);
	P->info_valid = 1;

	/*
	 * Consult librtld_db to get the load object
	 * names for all of the shared libraries.
	 */
	if (anychange && P->rap != NULL)
		(void) rd_loadobj_iter(P->rap, map_iter, P);
}

/*
 * Return the librtld_db agent handle for the victim process.
 * The handle will become invalid at the next successful exec() and the
 * client (caller of proc_rd_agent()) must not use it beyond that point.
 */
rd_agent_t *
proc_rd_agent(struct ps_prochandle *P)
{
	if (P->rap == NULL) {
		proc_update_maps(P);
		if ((P->rap = rd_new(P)) != NULL)
			(void) rd_loadobj_iter(P->rap, map_iter, P);
	}
	return (P->rap);
}

/*
 * Temporary interface.
 * Return the prmap_t structure containing 'addr', but only if it
 * is in the dynamic linker's link map and is the text section.
 */
const prmap_t *
proc_addr_to_map(struct ps_prochandle *P, uintptr_t addr)
{
	map_info_t *mptr;
	file_info_t *fptr;
	const prmap_t *pmp;
	uintptr_t base;
	int i;

	if (P->num_mappings == 0)
		proc_update_maps(P);

	for (i = 0, mptr = Next(&P->map_head); i < P->num_mappings;
	    i++, mptr = Next(mptr)) {
		pmp = &mptr->map_pmap;
		if (addr >= pmp->pr_vaddr &&
		    addr < pmp->pr_vaddr + pmp->pr_size) {
			if ((fptr = build_map_symtab(P, mptr)) != NULL &&
			    fptr->file_lo != NULL &&
			    (base = fptr->file_lo->rl_base) >= pmp->pr_vaddr &&
			    base < pmp->pr_vaddr + pmp->pr_size)
				return (pmp);
			break;
		}
	}

	return (NULL);
}

/*
 * Support routine for getauxval().
 */
static void
readauxvec(struct ps_prochandle *P)
{
	char auxfile[64];
	struct stat statb;
	ssize_t naux;
	int fd;

	if (P->auxv)
		free(P->auxv);
	P->auxv = NULL;
	(void) sprintf(auxfile, "/proc/%d/auxv", (int)P->pid);
	if ((fd = open(auxfile, O_RDONLY)) < 0 ||
	    fstat(fd, &statb) != 0 ||
	    statb.st_size < sizeof (auxv_t) ||
	    ((P->auxv = malloc(statb.st_size + sizeof (auxv_t))) == NULL) ||
	    (naux = read(fd, P->auxv, statb.st_size)) < 0 ||
	    (naux /= sizeof (auxv_t)) < 1) {
		if (P->auxv)
			free(P->auxv);
		P->auxv = malloc(sizeof (auxv_t));
		naux = 0;
	}
	if (fd >= 0)
		(void) close(fd);
	(void) memset(&P->auxv[naux], 0, sizeof (auxv_t));
}

/*
 * Return a requested element from the process's aux vector.
 * Return -1 on failure (this is adequate for our purposes).
 */
static long
getauxval(struct ps_prochandle *P, int type)
{
	auxv_t *auxv;

	if (P->auxv == NULL)
		readauxvec(P);

	for (auxv = P->auxv; auxv->a_type != AT_NULL; auxv++)
		if (auxv->a_type == type)
			return (auxv->a_un.a_val);

	return (-1);
}

/*
 * Find or build the symbol table for the given mapping.
 */
static file_info_t *
build_map_symtab(struct ps_prochandle *P, map_info_t *mptr)
{
	file_info_t	*fptr;
	u_int		i;

	if (mptr->map_pmap.pr_mapname[0] == '\0')
		return (NULL);

	if ((fptr = mptr->map_file) == NULL) {
		/*
		 * Attempt to find a matching file.
		 * (A file can be mapped at several different addresses.)
		 */
		for (i = 0, fptr = Next(&P->file_head); i < P->num_files;
		    i++, fptr = Next(fptr)) {
			if (strcmp(fptr->file_pname, mptr->map_pmap.pr_mapname)
			    == 0) {
				mptr->map_file = fptr;
				fptr->file_ref++;
				return (fptr);
			}
		}
		fptr = NULL;
	}

	if (fptr == NULL)
		fptr = file_info_new(P, mptr);

	if (!fptr->file_init)
		build_file_symtab(P, fptr);

	return (fptr);
}

/*
 * Build the symbol table for the given mapped file.
 */
static void
build_file_symtab(struct ps_prochandle *P, file_info_t *fptr)
{
	char		objectfile[64];
	GElf_Ehdr	ehdr;
	GElf_Shdr	*shdr;
	Elf		*elf;
	Elf_Scn		*scn;
	Elf_Data	*data;
	sym_tbl_t	*which;
	char		*names;
	u_int		i;
	struct {
		GElf_Shdr	c_shdr;
		Elf_Data	*c_data;
		char		*c_name;
	} *cache, *_cache;

	if (fptr->file_init)	/* already initialized */
		return;

	/*
	 * Mark the file_info struct as having the symbol table initialized
	 * even if we fail below.  We tried once; we don't try again.
	 */
	fptr->file_init = 1;

	if (elf_version(EV_CURRENT) == EV_NONE)
		return;

	(void) sprintf(objectfile, "/proc/%d/object/%s", (int)P->pid,
		fptr->file_pname);

	if ((fptr->file_fd = open(objectfile, O_RDONLY)) < 0)
		return;

	if ((elf = elf_begin(fptr->file_fd, ELF_C_READ, NULL)) == NULL ||
	    elf_kind(elf) != ELF_K_ELF) {
		(void) close(fptr->file_fd);
		fptr->file_fd = -1;
		return;
	}
	fptr->file_elf = elf;

	/*
	 * Obtain the .shstrtab data buffer to provide the required section
	 * name strings.
	 */
	if ((gelf_getehdr(elf, &ehdr)) == NULL ||
	    (scn = elf_getscn(elf, ehdr.e_shstrndx)) == NULL ||
	    (data = elf_getdata(scn, NULL)) == NULL)
		goto bad;
	names = data->d_buf;
	fptr->file_etype = ehdr.e_type;

	/*
	 * Fill in the cache descriptor with information for each section.
	 */
	_cache = cache = malloc(ehdr.e_shnum * sizeof (*cache));
	_cache++;

	for (scn = NULL; scn = elf_nextscn(elf, scn); _cache++) {
		if ((gelf_getshdr(scn, &_cache->c_shdr)) == NULL ||
		    (_cache->c_data = elf_getdata(scn, NULL)) == NULL) {
			free(cache);
			goto bad;
		}
		_cache->c_name = names + _cache->c_shdr.sh_name;
	}

	for (i = 1, _cache = &cache[1]; i < ehdr.e_shnum; i++, _cache++) {
		shdr = &_cache->c_shdr;

		if (shdr->sh_type == SHT_SYMTAB)
			which = &fptr->file_symtab;
		else if (shdr->sh_type == SHT_DYNSYM)
			which = &fptr->file_dynsym;
		else
			continue;

		/*
		 * Determine the symbol data and number.
		 */
		which->sym_data = _cache->c_data;
		which->sym_symn = shdr->sh_size / shdr->sh_entsize;
		which->sym_strs = (char *)cache[shdr->sh_link].c_data->d_buf;
	}

	free(cache);
	return;

bad:
	(void) elf_end(elf);
	fptr->file_elf = NULL;
	(void) close(fptr->file_fd);
	fptr->file_fd = -1;
}

/*
 * Given a process virtual adress, return the map_info_t containing it.
 * If none found, return NULL.
 */
static map_info_t *
addr_to_map(struct ps_prochandle *P, uintptr_t addr)
{
	u_int i;
	map_info_t *mptr;

	for (i = 0, mptr = Next(&P->map_head); i < P->num_mappings;
	    i++, mptr = Next(mptr)) {
		if (addr >= mptr->map_pmap.pr_vaddr &&
		    addr < mptr->map_pmap.pr_vaddr + mptr->map_pmap.pr_size)
			return (mptr);
	}

	return (NULL);
}

/*
 * Return the map_info_t for the executable file.
 * If not found, return NULL.
 */
static map_info_t *
exec_map(struct ps_prochandle *P)
{
	u_int i;
	map_info_t *mptr;
	map_info_t *mold = NULL;
	file_info_t *fptr;
	uintptr_t base;

	for (i = 0, mptr = Next(&P->map_head); i < P->num_mappings;
	    i++, mptr = Next(mptr)) {
		if (mptr->map_pmap.pr_mapname[0] == '\0')
			continue;
		if (strcmp(mptr->map_pmap.pr_mapname, "a.out") == 0) {
			if ((fptr = mptr->map_file) != NULL &&
			    fptr->file_lo != NULL) {
				base = fptr->file_lo->rl_base;
				if (base >= mptr->map_pmap.pr_vaddr &&
				    base < mptr->map_pmap.pr_vaddr +
				    mptr->map_pmap.pr_size)	/* text space */
					return (mptr);
				mold = mptr;	/* must be the data */
				continue;
			}
			/* This is a poor way to test for text space */
			if (!(mptr->map_pmap.pr_mflags & MA_EXEC) ||
			    (mptr->map_pmap.pr_mflags & MA_WRITE)) {
				mold = mptr;
				continue;
			}
			return (mptr);
		}
	}

	return (mold);
}

/*
 * Given a shared object name, return the map_info_t for it.
 * If none found, return NULL.
 */
/*
 * Question here about the meaning of 'objname'.
 * This is a string to be matched against ld.so.1's link map names.
 * However, libthread_db calls in here with the name TD_LIBTHREAD_NAME,
 * which is the literal string "libthread.so".  Normally the link maps
 * contain the full path name, "/usr/lib/libthread.so.1", so a simple
 * compare would not find a match.  XXX What to do?
 * For now, we show a match if 'objname' is a substring of the map name.
 */
static map_info_t *
object_to_map(struct ps_prochandle *P, const char *objname)
{
	u_int i;
	map_info_t *mptr;
	map_info_t *mold = NULL;
	file_info_t *fptr;
	uintptr_t base;

	for (i = 0, mptr = Next(&P->map_head); i < P->num_mappings;
	    i++, mptr = Next(mptr)) {
		if (mptr->map_pmap.pr_mapname[0] == '\0' ||
		    (fptr = mptr->map_file) == NULL ||
		    fptr->file_lo == NULL ||
		    fptr->file_lname == NULL)
			continue;
		base = fptr->file_lo->rl_base;
		if (strstr(fptr->file_lname, objname) != NULL) { /* match */
			if (base >= mptr->map_pmap.pr_vaddr &&
			    base < mptr->map_pmap.pr_vaddr +
			    mptr->map_pmap.pr_size)	/* address match */
				return (mptr);
			mold = mptr;	/* this must be the data segment */
		}
	}

	return (mold);
}

static map_info_t *
object_name_to_map(struct ps_prochandle *P, const char *object_name)
{
	map_info_t *mptr;

	if (P->num_mappings == 0)
		proc_update_maps(P);

	if (object_name == PR_OBJ_EXEC) {
		if ((mptr = P->map_exec) != NULL ||
		    (mptr = addr_to_map(P, getauxval(P, AT_ENTRY))) != NULL ||
		    (mptr = exec_map(P)) != NULL)
			P->map_exec = mptr;	/* remember for next time */
	} else if (object_name == PR_OBJ_LDSO) {
		if ((mptr = P->map_ldso) != NULL ||
		    (mptr = addr_to_map(P, getauxval(P, AT_BASE))) != NULL)
			P->map_ldso = mptr;	/* remember for next time */
	} else {
		mptr = (proc_rd_agent(P) == NULL)? NULL :
			object_to_map(P, object_name);
	}

	return (mptr);
}

/*
 * When two symbols are found by address, decide which one is to be preferred.
 */
static GElf_Sym *
sym_prefer(GElf_Sym *sym1, char *name1, GElf_Sym *sym2, char *name2)
{
	/*
	 * Prefer the non-NULL symbol.
	 */
	if (sym1 == NULL)
		return (sym2);
	if (sym2 == NULL)
		return (sym1);

	/*
	 * Prefer a function to an object.
	 */
	if (GELF_ST_TYPE(sym1->st_info) != GELF_ST_TYPE(sym2->st_info)) {
		if (GELF_ST_TYPE(sym1->st_info) == STT_FUNC)
			return (sym1);
		if (GELF_ST_TYPE(sym2->st_info) == STT_FUNC)
			return (sym2);
	}

	/*
	 * Prefer the one that has a value closer to the requested addr.
	 */
	if (sym1->st_value > sym2->st_value)
		return (sym1);
	if (sym1->st_value < sym2->st_value)
		return (sym2);

	/*
	 * Prefer the weak or strong global symbol to the local symbol.
	 */
	if (GELF_ST_BIND(sym1->st_info) != GELF_ST_BIND(sym2->st_info)) {
		if (GELF_ST_BIND(sym1->st_info) == STB_LOCAL)
			return (sym2);
		if (GELF_ST_BIND(sym2->st_info) == STB_LOCAL)
			return (sym1);
	}

	/*
	 * Prefer the symbol with fewer leading underscores in the name.
	 */
	while (*name1 == '_' && *name2 == '_')
		name1++, name2++;
	if (*name1 == '_')
		return (sym2);
	if (*name2 == '_')
		return (sym1);

	/*
	 * Prefer the smaller sized symbol.
	 */
	if (sym1->st_size < sym2->st_size)
		return (sym1);
	if (sym1->st_size > sym2->st_size)
		return (sym2);

	/*
	 * There is no reason to prefer one to the other.
	 * Arbitrarily prefer the first one.
	 */
	return (sym1);
}

/*
 * Look up a symbol by address in the specified symbol table.
 * Adjustment to 'addr' must already have been made for the
 * offset of the symbol if this is a dynamic library symbol table.
 */
static GElf_Sym *
sym_by_addr(sym_tbl_t *symtab, GElf_Addr addr, GElf_Sym *symbolp)
{
	Elf_Data *data = symtab->sym_data;
	size_t symn = symtab->sym_symn;
	char *strs = symtab->sym_strs;
	GElf_Sym sym, *symp = NULL;
	GElf_Sym osym, *osymp = NULL;
	int type;
	int i;

	if (data == NULL || symn == 0 || strs == NULL)
		return (NULL);

	for (i = 0; i < symn; i++) {
		if ((symp = gelf_getsym(data, i, &sym)) != NULL) {
			type = GELF_ST_TYPE(sym.st_info);
			if ((type == STT_OBJECT || type == STT_FUNC) &&
			    addr >= sym.st_value &&
			    addr < sym.st_value + sym.st_size) {
				if (osymp)
					symp = sym_prefer(
					    symp, strs + symp->st_name,
					    osymp, strs + osymp->st_name);
				if (symp != osymp) {
					osym = sym;
					osymp = &osym;
				}
			}
		}
	}
	if (osymp) {
		*symbolp = osym;
		return (symbolp);
	}
	return (NULL);
}

/*
 * Look up a symbol by name in the specified symbol table.
 */
static GElf_Sym *
sym_by_name(sym_tbl_t *symtab, const char *name, GElf_Sym *sym)
{
	Elf_Data *data = symtab->sym_data;
	size_t symn = symtab->sym_symn;
	char *strs = symtab->sym_strs;
	int type;
	int i;

	if (data == NULL || symn == 0 || strs == NULL)
		return (NULL);

	for (i = 0; i < symn; i++) {
		if (gelf_getsym(data, i, sym) != NULL) {
			type = GELF_ST_TYPE(sym->st_info);
			if ((type == STT_OBJECT || type == STT_FUNC) &&
			    strcmp(name, strs + sym->st_name) == 0) {
				return (sym);
			}
		}
	}

	return (NULL);
}

/*
 * Look up a 32-bit symbol by name in the specified symbol table.
 */
#if defined(_ILP32)
static Elf32_Sym *
sym_by_name32(sym_tbl_t *symtab, const char *name, Elf32_Sym *sym)
{
	Elf_Data *data = symtab->sym_data;
	size_t symn = symtab->sym_symn;
	char *strs = symtab->sym_strs;
	Elf32_Sym *symp;
	int type;
	int i;

	if (data == NULL || symn == 0 || strs == NULL)
		return (NULL);

	for (i = 0; i < symn; i++) {
		symp = &(((Elf32_Sym *)data->d_buf)[i]);
		type = ELF32_ST_TYPE(symp->st_info);
		if ((type == STT_OBJECT || type == STT_FUNC) &&
		    strcmp(name, strs + symp->st_name) == 0) {
			*sym = *symp;
			return (sym);
		}
	}

	return (NULL);
}
#endif

/*
 * Search the process symbol tables looking for a symbol whose
 * value to value+size contain the address specified by addr.
 * Return values are:
 *	sym_name_buffer containing the symbol name
 *	GElf_Sym symbol table entry
 * Returns 0 on success, -1 on failure.
 */
int
proc_lookup_by_addr(
	struct ps_prochandle *P,
	uintptr_t addr,			/* process address being sought */
	char *sym_name_buffer,		/* buffer for the symbol name */
	size_t bufsize,			/* size of sym_name_buffer */
	GElf_Sym *symbolp)		/* returned symbol table entry */
{
	GElf_Sym	*symp;
	char		*name;
	GElf_Sym	sym1, *sym1p = NULL;
	GElf_Sym	sym2, *sym2p = NULL;
	char		*name1 = NULL;
	char		*name2 = NULL;
	offset_t	offset = 0;
	map_info_t	*mptr;
	file_info_t	*fptr;

	if (P->num_mappings == 0)
		proc_update_maps(P);

	if ((mptr = addr_to_map(P, addr)) == NULL ||	/* no such address */
	    (fptr = build_map_symtab(P, mptr)) == NULL || /* no mapped file */
	    fptr->file_elf == NULL)			/* not an ELF file */
		return (-1);

	if (fptr->file_etype == ET_DYN) {
		offset = mptr->map_pmap.pr_vaddr - mptr->map_pmap.pr_offset;
		addr -= offset;
	}

	/*
	 * Search both symbol tables, symtab first, then dynsym.
	 */
	if ((sym1p = sym_by_addr(&fptr->file_symtab, addr, &sym1)) != NULL)
		name1 = fptr->file_symtab.sym_strs + sym1.st_name;
	if ((sym2p = sym_by_addr(&fptr->file_dynsym, addr, &sym2)) != NULL)
		name2 = fptr->file_dynsym.sym_strs + sym2.st_name;

	if ((symp = sym_prefer(sym1p, name1, sym2p, name2)) == NULL)
		return (-1);
	symp->st_value += offset;
	name = (symp == sym1p)? name1 : name2;

	*symbolp = *symp;
	(void) strncpy(sym_name_buffer, name, bufsize);
	return (0);
}

/*
 * Search the process symbol tables looking for a symbol
 * whose name matches the specified name.
 * Return values are:
 *	GElf_Sym symbol table entry
 * Returns 0 on success, -1 on failure.
 */
int
proc_lookup_by_name(
	struct ps_prochandle *P,
	const char *object_name,	/* load object name */
	const char *symbol_name,	/* symbol name */
	GElf_Sym *sym)			/* returned symbol table entry */
{
	map_info_t *mptr;
	file_info_t *fptr;
	int cnt;

	if (object_name == PR_OBJ_EVERY) {
		/* create all the file_info_t's for all the mappings */
		(void) proc_rd_agent(P);
		cnt = P->num_files;
		fptr = Next(&P->file_head);
	} else {
		cnt = 1;
		if ((mptr = object_name_to_map(P, object_name)) == NULL ||
		    (fptr = build_map_symtab(P, mptr)) == NULL)
			return (-1);
	}

	for (; cnt; cnt--, fptr = Next(fptr)) {
		/*
		 * Search both symbol tables, symtab first, then dynsym.
		 */
		if (!fptr->file_init)
			build_file_symtab(P, fptr);
		if (fptr->file_elf == NULL ||
		    (!sym_by_name(&fptr->file_symtab, symbol_name, sym) &&
		    !sym_by_name(&fptr->file_dynsym, symbol_name, sym)))
			continue;

		if (fptr->file_etype == ET_DYN &&
		    fptr->file_lo != NULL)
			sym->st_value += fptr->file_lo->rl_base;

		return (0);
	}

	return (-1);
}

/*
 * Iterate over the process's address space mappings.
 */
int
proc_mapping_iter(struct ps_prochandle *P, proc_map_f *func, void *cd)
{
	map_info_t *mptr;
	file_info_t *fptr;
	u_int cnt;
	char *object_name;
	int rc = 0;

	/* create all the file_info_t's for all the mappings */
	(void) proc_rd_agent(P);

	for (cnt = P->num_mappings, mptr = Next(&P->map_head);
	    cnt; cnt--, mptr = Next(mptr)) {
		if ((fptr = mptr->map_file) == NULL)
			object_name = NULL;
		else
			object_name = fptr->file_lname;
		if ((rc = func(cd, &mptr->map_pmap, object_name)) != 0)
			return (rc);
	}
	return (0);
}

/*
 * Iterate over the process's mapped objects.
 */
int
proc_object_iter(struct ps_prochandle *P, proc_map_f *func, void *cd)
{
	map_info_t *mptr;
	file_info_t *fptr;
	u_int cnt;
	char *object_name;
	int rc = 0;

	/* create all the file_info_t's for all the mappings */
	(void) proc_rd_agent(P);

	for (cnt = P->num_files, fptr = Next(&P->file_head);
	    cnt; cnt--, fptr = Next(fptr)) {
		if ((mptr = fptr->file_map) == NULL)	/* can't happen? */
			continue;
		object_name = fptr->file_lname;
		if ((rc = func(cd, &mptr->map_pmap, object_name)) != 0)
			return (rc);
	}
	return (0);
}

/*
 * Given a virtual address, return the name of the underlying
 * mapped object (file), as provided by the dynamic linker.
 * Return NULL on failure (no underlying shared library).
 */
char *
proc_objname(struct ps_prochandle *P, uintptr_t addr,
	char *buffer, size_t bufsize)
{
	map_info_t *mptr;
	file_info_t *fptr;

	/* create all the file_info_t's for all the mappings */
	(void) proc_rd_agent(P);

	if ((mptr = addr_to_map(P, addr)) != NULL &&
	    (fptr = mptr->map_file) != NULL &&
	    fptr->file_lname != NULL) {
		(void) strncpy(buffer, fptr->file_lname, bufsize);
		if (strlen(fptr->file_lname) >= bufsize)
			buffer[bufsize-1] = '\0';
		return (buffer);
	}
	return (NULL);
}

/*
 * Given an object name, iterate over the objects's symbols.
 * If which == PR_SYMTAB, search the normal symbol table.
 * If which == PR_DYNSYM, search the dynamic symbol table.
 */
int
proc_symbol_iter(struct ps_prochandle *P, const char *object_name,
	int which, int type, proc_sym_f *func, void *cd)
{
	GElf_Sym sym;
	map_info_t *mptr;
	file_info_t *fptr;
	sym_tbl_t *symtab;
	Elf_Data *data;
	size_t symn;
	const char *strs;
	size_t offset;
	int i;
	int rv;

	if ((mptr = object_name_to_map(P, object_name)) == NULL)
		return (-1);

	if ((fptr = build_map_symtab(P, mptr)) == NULL || /* no mapped file */
	    fptr->file_elf == NULL)			/* not an ELF file */
		return (-1);

	/*
	 * Search the specified symbol table.
	 */
	switch (which) {
	case PR_SYMTAB:
		symtab = &fptr->file_symtab;
		break;
	case PR_DYNSYM:
		symtab = &fptr->file_dynsym;
		break;
	default:
		return (-1);
	}
	data = symtab->sym_data;
	symn = symtab->sym_symn;
	strs = symtab->sym_strs;
	if (fptr->file_etype == ET_DYN)
		offset = mptr->map_pmap.pr_vaddr - mptr->map_pmap.pr_offset;
	else
		offset = 0;
	if (data == NULL || strs == NULL)
		return (-1);

	rv = 0;
	for (i = 0; i < symn; i++) {
		if (gelf_getsym(data, i, &sym) != NULL) {
			if (sym.st_shndx == SHN_UNDEF)	/* symbol reference */
				continue;

			switch (GELF_ST_BIND(sym.st_info)) {
			case STB_LOCAL:
				if (!(type & BIND_LOCAL))
					continue;
				break;
			case STB_GLOBAL:
				if (!(type & BIND_GLOBAL))
					continue;
				break;
			case STB_WEAK:
				if (!(type & BIND_WEAK))
					continue;
				break;
			default:
				continue;
			}

			switch (GELF_ST_TYPE(sym.st_info)) {
			case STT_NOTYPE:
				if (!(type & TYPE_NOTYPE))
					continue;
				break;
			case STT_OBJECT:
				if (!(type & TYPE_OBJECT))
					continue;
				break;
			case STT_FUNC:
				if (!(type & TYPE_FUNC))
					continue;
				break;
			case STT_SECTION:
				if (!(type & TYPE_SECTION))
					continue;
				break;
			case STT_FILE:
				if (!(type & TYPE_FILE))
					continue;
				break;
			default:
				continue;
			}

			sym.st_value += offset;
			if ((rv = func(cd, &sym, strs + sym.st_name)) != 0)
				break;
		}
	}

	return (rv);
}

/* number of argument or environment pointers to read all at once */
#define	NARG	100

/*
 * Like getenv(char *name), but applied to the target proces.
 * The caller must provide a buffer for the resulting string.
 */
char *
proc_getenv(struct ps_prochandle *P, const char *name,
	char *buffer, size_t bufsize)
{
	char string[PATH_MAX];
	psinfo_t psinfo;
	GElf_Sym sym;
	long envpoff;
	int nenv = NARG;
	long envp[NARG];
	off_t envoff;
	char *s;

	/*
	 * Attempt to find the "_environ" variable in the process.
	 * Failing that, use the original value provided by psinfo.
	 */
	if (proc_get_psinfo(P->pid, &psinfo) != 0)
		return (NULL);

	envpoff = (long)psinfo.pr_envp;		/* default value */
	if (proc_lookup_by_name(P, PR_OBJ_EXEC, "_environ", &sym) == 0) {
		if (P->status.pr_dmodel == PR_MODEL_NATIVE) {
			if (pread(P->asfd, &envpoff, sizeof (envpoff),
			    (off_t)sym.st_value) != sizeof (envpoff))
				envpoff = (long)psinfo.pr_envp;
		} else if (P->status.pr_dmodel == PR_MODEL_ILP32) {
			uint32_t envpoff32;

			if (pread(P->asfd, &envpoff32, sizeof (envpoff32),
			    (off_t)sym.st_value) != sizeof (envpoff32))
				envpoff = (long)psinfo.pr_envp;
			else
				envpoff = envpoff32;
		}
	}

	for (;;) {
		if (nenv == NARG) {
			(void) memset(envp, 0, sizeof (envp));
			if (P->status.pr_dmodel == PR_MODEL_NATIVE) {
				if (pread(P->asfd, envp, sizeof (envp),
				    envpoff) <= 0)
					break;
			} else if (P->status.pr_dmodel == PR_MODEL_ILP32) {
				int i;
				uint32_t envp32[NARG];

				if (pread(P->asfd, envp32, sizeof (envp32),
				    envpoff) <= 0)
					break;
				for (i = 0; i < NARG; i++)
					envp[i] = envp32[i];
			} else {
				break;
			}
			nenv = 0;
		}
		if ((envoff = envp[nenv++]) == 0)
			break;
		if (proc_read_string(P->asfd, string, sizeof (string),
		    envoff) > 0 && (s = strchr(string, '=')) != NULL) {
			*s++ = '\0';
			if (strcmp(name, string) == 0) {
				(void) strncpy(buffer, s, bufsize);
				return (buffer);
			}
		}
		envpoff += (P->status.pr_dmodel == PR_MODEL_LP64)? 8 : 4;
	}

	return (NULL);
}

/*
 * Work really hard to get the full pathname for the executable file.
 */
char *
proc_execname(struct ps_prochandle *P, char *buffer, size_t bufsize)
{
	char cwd[2*PATH_MAX];
	char exec_name[PATH_MAX];
	char fname[PRARGSZ+2];
	psinfo_t psinfo;
	struct stat astatb;
	struct stat statb;
	intptr_t fnamep;
	char *s;
	char *p;
	int i;

	if (P->execname) {	/* already have it */
		(void) strncpy(buffer, P->execname, bufsize);
		return (buffer);
	}

	/*
	 * Fetch and remember the real executable's dev/ino.
	 * This is the most reliable part of the operation.
	 */
	(void) sprintf(exec_name, "/proc/%d/object/a.out", (int)P->pid);
	if (stat(exec_name, &astatb) != 0 || !S_ISREG(astatb.st_mode))
		return (NULL);

	/*
	 * This works only if the executable is dynamically linked
	 * and if the process has not trashed its stack.
	 */
	fnamep = getauxval(P, AT_SUN_EXECNAME);
	if (fnamep == -1 ||
	    proc_read_string(P->asfd, exec_name, sizeof (exec_name),
	    (off_t)fnamep) <= 0)
		exec_name[0] = '\0';

	/*
	 * If exec_name is not a full pathname, make an
	 * effort to resolve it by prepending the current working
	 * directory of the target process.  This only works if
	 * the target process has not changed its current directory
	 * since it was exec()d.  *Whew*
	 */
	cwd[0] = '\0';
	if (exec_name[0] != '/') {
		char proc_cwd[64];

		(void) sprintf(proc_cwd, "/proc/%d/cwd", (int)P->pid);
		if (proc_dirname(proc_cwd, cwd, PATH_MAX) == NULL)
			cwd[0] = '\0';
		else if (exec_name[0] != '\0') {
			s = cwd + strlen(cwd);
			*s = '/';
			(void) strcpy(s + 1, exec_name);
			(void) strncpy(exec_name, cwd, PATH_MAX);
			*s = '\0';
		}
	}

	if (exec_name[0] == '/' &&
	    (i = resolvepath(exec_name, exec_name, sizeof (exec_name))) > 0) {
		exec_name[i] = '\0';
		if (stat(exec_name, &statb) == 0 &&
		    S_ISREG(statb.st_mode) &&
		    statb.st_dev == astatb.st_dev &&
		    statb.st_ino == astatb.st_ino) {
			P->execname = strdup(exec_name);
			(void) strncpy(buffer, exec_name, bufsize);
			return (buffer);
		}
	}

	/*
	 * The attempt to get the execname from AT_SUN_EXECNAME failed.
	 * As the next try, attempt to get it from psinfo.
	 * We already have the process's working directory.
	 */
	if (proc_get_psinfo(P->pid, &psinfo) != 0)
		return (NULL);

	(void) strncpy(exec_name, psinfo.pr_psargs, PRARGSZ);
	exec_name[PRARGSZ] = '\0';
	if ((s = strchr(exec_name, ' ')) != NULL)
		*s = '\0';

	/*
	 * Prepend cwd if necessary and possible.
	 */
	if (exec_name[0] != '/' && cwd[0] == '/') {
		s = cwd + strlen(cwd);
		*s = '/';
		(void) strcpy(s + 1, exec_name);
		(void) strncpy(exec_name, cwd, PATH_MAX);
		*s = '\0';
	}

	if (exec_name[0] == '/' &&
	    (i = resolvepath(exec_name, exec_name, sizeof (exec_name))) > 0) {
		exec_name[i] = '\0';
		if (stat(exec_name, &statb) == 0 &&
		    S_ISREG(statb.st_mode) &&
		    statb.st_dev == astatb.st_dev &&
		    statb.st_ino == astatb.st_ino) {
			P->execname = strdup(exec_name);
			(void) strncpy(buffer, exec_name, bufsize);
			return (buffer);
		}
	}

	/*
	 * At this point we go after the process's PATH and search
	 * each directory for the name matching psinfo.pr_fname.
	 */
	fname[0] = '/';
	(void) strncpy(fname+1, psinfo.pr_psargs, PRARGSZ);
	fname[PRARGSZ+1] = '\0';
	if ((s = strchr(fname, ' ')) != NULL)
		*s = '\0';

	/*
	 * If the name from pr_psargs contains pr_fname as its leading string,
	 * then accept the name from pr_psargs, else replace it with pr_fname.
	 */
	if (strchr(fname+1, '/') != NULL ||
	    strncmp(fname+1, psinfo.pr_fname, strlen(psinfo.pr_fname)) != 0)
		(void) strcpy(fname+1, psinfo.pr_fname);

	/*
	 * Get the PATH environment variable from the target process.
	 */
	if (proc_getenv(P, "PATH", cwd, sizeof (cwd)) == NULL)
		return (NULL);

	/*
	 * Look for the file in each element of the PATH.
	 */
	for (p = strtok_r(cwd, ":", &s); p != NULL;
	    p = strtok_r(NULL, ":", &s)) {
		if (*p != '/')
			continue;
		(void) strcpy(exec_name, p);
		(void) strcat(exec_name, fname);
		if ((i = resolvepath(exec_name, exec_name,
		    sizeof (exec_name))) > 0) {
			exec_name[i] = '\0';
			if (stat(exec_name, &statb) == 0 &&
			    S_ISREG(statb.st_mode) &&
			    statb.st_dev == astatb.st_dev &&
			    statb.st_ino == astatb.st_ino) {
				P->execname = strdup(exec_name);
				(void) strncpy(buffer, exec_name, bufsize);
				return (buffer);
			}
		}
	}

	/* abject failure */
	return (NULL);
}

/*
 * Called from Pcreate() and Pgrab() to initialize
 * the symbol table heads in the new ps_prochandle.
 */
void
Pinitsym(struct ps_prochandle *P)
{
	P->num_mappings = 0;
	P->num_files = 0;
	Link(&P->map_head, NULL);
	Link(&P->file_head, NULL);
}

/*
 * Called from Prelease() to destroy the symbol tables.
 * Must be called by the client after an exec() in the victim process.
 */
void
proc_reset_maps(struct ps_prochandle *P)
{
	map_info_t *mptr;

	if (P->rap)
		rd_delete(P->rap);
	P->rap = NULL;

	if (P->execname)
		free(P->execname);
	P->execname = NULL;

	if (P->auxv)
		free(P->auxv);
	P->auxv = NULL;

	while (P->num_mappings) {
		mptr = Next(&P->map_head);
		map_info_free(P, mptr);
	}

	if (P->map_head.list_forw != P->map_head.list_back ||
	    P->map_head.list_forw != &P->map_head) {
		(void) fprintf(stderr,
			"proc_reset_maps(): map_head inconsistency: "
			" forw = %p back = %p self = %p\n",
			(void *)P->map_head.list_forw,
			(void *)P->map_head.list_back,
			(void *)&P->map_head);
	}

	/* There should be no file_info_t's left now */
	if (P->num_files) {
		(void) fprintf(stderr,
			"proc_reset_maps(): count inconsistency: "
			"num_files not zero: %d\n",
			P->num_files);
	}

	if (P->file_head.list_forw != P->file_head.list_back ||
	    P->file_head.list_forw != &P->file_head) {
		(void) fprintf(stderr,
			"proc_reset_maps(): file_head inconsistency: "
			" forw = %p back = %p self = %p\n",
			(void *)P->file_head.list_forw,
			(void *)P->file_head.list_back,
			(void *)&P->file_head);
	}

	P->info_valid = 0;
}

/*
 * Here follows the proc_service symbol table functions.
 */

ps_err_e
ps_pglobal_lookup(
	struct ps_prochandle *P,
	const char *object_name,
	const char *sym_name,
	psaddr_t *sym_addr)
{
	map_info_t *mptr;
	file_info_t *fptr;
	GElf_Sym sym;

	if ((mptr = object_name_to_map(P, object_name)) == NULL)
		return (PS_NOSYM);

	if ((fptr = build_map_symtab(P, mptr)) == NULL || /* no mapped file */
	    fptr->file_elf == NULL)			/* not an ELF file */
		return (PS_NOSYM);

	/*
	 * Search both symbol tables, symtab first, then dynsym.
	 */
	if (sym_by_name(&fptr->file_symtab, sym_name, &sym) ||
	    sym_by_name(&fptr->file_dynsym, sym_name, &sym)) {
		if (fptr->file_etype == ET_DYN)
			*sym_addr = sym.st_value +
			    mptr->map_pmap.pr_vaddr - mptr->map_pmap.pr_offset;
		else
			*sym_addr = sym.st_value;
		return (PS_OK);
	}

	return (PS_NOSYM);
}

#if defined(_ILP32)

ps_err_e
ps_pglobal_sym(
	struct ps_prochandle *P,
	const char *object_name,
	const char *sym_name,
	Elf32_Sym *symp)
{
	map_info_t *mptr;
	file_info_t *fptr;

	if ((mptr = object_name_to_map(P, object_name)) == NULL)
		return (PS_NOSYM);

	if ((fptr = build_map_symtab(P, mptr)) == NULL || /* no mapped file */
	    fptr->file_elf == NULL ||			/* not an ELF file */
	    gelf_getclass(fptr->file_elf) != ELFCLASS32) /* not 32-bit */
		return (PS_NOSYM);

	/*
	 * Search both symbol tables, symtab first, then dynsym.
	 */
	if (sym_by_name32(&fptr->file_symtab, sym_name, symp))
		symp->st_name += (uintptr_t)fptr->file_symtab.sym_strs;
	else if (sym_by_name32(&fptr->file_dynsym, sym_name, symp))
		symp->st_name += (uintptr_t)fptr->file_dynsym.sym_strs;
	else
		return (PS_NOSYM);

	if (fptr->file_etype == ET_DYN)
		symp->st_value +=
		    mptr->map_pmap.pr_vaddr - mptr->map_pmap.pr_offset;

	return (PS_OK);
}

#elif defined(_LP64)

ps_err_e
ps_pglobal_sym(
	struct ps_prochandle *P,
	const char *object_name,
	const char *sym_name,
	Elf64_Sym *symp)
{
	map_info_t *mptr;
	file_info_t *fptr;

	if ((mptr = object_name_to_map(P, object_name)) == NULL)
		return (PS_NOSYM);

	if ((fptr = build_map_symtab(P, mptr)) == NULL || /* no mapped file */
	    fptr->file_elf == NULL)			/* not an ELF file */
		return (PS_NOSYM);

	/*
	 * Search both symbol tables, symtab first, then dynsym.
	 */
	if (!sym_by_name(&fptr->file_symtab, sym_name, symp) &&
	    !sym_by_name(&fptr->file_dynsym, sym_name, symp))
		return (PS_NOSYM);

	if (fptr->file_etype == ET_DYN)
		symp->st_value +=
		    mptr->map_pmap.pr_vaddr - mptr->map_pmap.pr_offset;

	return (PS_OK);
}

#endif

/*
 * This looks like it requires the aux vector to be stored
 * in the ps_prochandle structure and a pointer to this
 * instance be returned to the caller.  Is this right?
 * Hopefully the 'const' declaration will prevent the caller
 * from modifying it or holding on to it too long.
 *
 * Neither libthread_db nor librtld_db call this function.
 * XXX: What to do?
 */
/* ARGSUSED */
ps_err_e
ps_pauxv(struct ps_prochandle *P, const auxv_t **aux)
{
	if (P->auxv == NULL)
		readauxvec(P);
	*aux = (const auxv_t *)P->auxv;
	return (PS_OK);
}
