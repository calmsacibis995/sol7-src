/*
 * Copyright (c) 1993-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)readelf.c	1.18	98/02/19 SMI"

#include	"gprof.h"
#include	<stdlib.h>
#include	<sys/file.h>
#include	<fcntl.h>
#include	<unistd.h>
#include	<string.h>
#include	<sysexits.h>

#include	<libelf.h>
#include 	"gelf.h"

#ifdef DEBUG
static void	debug_dup_del(nltype *, nltype *);

#define	DPRINTF(msg, file)	if (debug & ELFDEBUG) \
					printf(msg, file);

#define	PRINTF(msg)		if (debug & ELFDEBUG) \
					printf(msg);

#define	DEBUG_DUP_DEL(keeper, louser)	if (debug & ELFDEBUG) \
						debug_dup_del(keeper, louser);

#else
#define	DPRINTF(msg, file)
#define	PRINTF(msg)
#define	DEBUG_DUP_DEL(keeper, louser)
#endif



#define	VOID_P		void *

size_t	textbegin, textsize;

/* Prototype definitions first */

static void	process(char *filename, int fd);
static void	get_symtab(Elf *elf, char *filename);
static void	get_textseg(Elf *elf, int fd, char *filename);
static int	compare(nltype *a, nltype *b);

static void
fatal_error(char *error)
{
	fprintf(stderr, "Fatal ELF error: %s (%s)\n", error, elf_errmsg(-1));
	exit(EX_SOFTWARE);
}

void
getnfile(char * aoutname)
{
	int	fd;

	DPRINTF(" Attempting to open %s  \n", aoutname);
	if ((fd = open((aoutname), O_RDONLY)) == -1) {
		(void) fprintf(stderr, "%s: cannot read file \n", aoutname);
		fprintf(stderr, "Exiting due to error(s)...\n");
		exit(EX_NOINPUT);
	}
	process(aoutname, fd);
	(void) close(fd);
}

/*
 * Get the ELF header and,  if it exists, call get_symtab()
 * to begin processing of the file; otherwise, return from
 * processing the file with a warning.
 */
static void
process(char *filename, int fd)
{
	Elf *elf;
	extern bool cflag;
	extern bool Bflag;

	if (elf_version(EV_CURRENT) == EV_NONE)
		fatal_error("libelf is out of date");

	if ((elf = elf_begin(fd, ELF_C_READ, NULL)) == NULL)
		fatal_error("can't read as ELF file");

	if (gelf_getclass(elf) == ELFCLASS64)
		Bflag = TRUE;

	get_symtab(elf, filename);

	if (cflag)
		get_textseg(elf, fd, filename);
}

static void
get_textseg(Elf *elf, int fd, char *filename)
{
	GElf_Ehdr ehdr;
	GElf_Phdr phdr;
	GElf_Half i;

	if (gelf_getehdr(elf, &ehdr) == NULL)
		fatal_error("can't read ehdr");

	for (i = 0; i < ehdr.e_phnum; i++) {

		if (gelf_getphdr(elf, i, &phdr) == NULL)
			continue;

		if (!(phdr.p_flags & PF_W) && (phdr.p_filesz > textsize)) {
			size_t chk;

			/*
			 * We could have multiple loadable text segments;
			 * keep the largest we find.
			 */
			if (textspace)
				free(textspace);

			/*
			 * gprof is a 32-bit program;  if this text segment
			 * has a > 32-bit offset or length, it's too big.
			 */
			chk = (size_t)phdr.p_vaddr + (size_t)phdr.p_filesz;
			if (phdr.p_vaddr + phdr.p_filesz != (GElf_Xword)chk)
				fatal_error("text segment too large for -c");

			textbegin = (size_t)phdr.p_vaddr;
			textsize = (size_t)phdr.p_filesz;

			textspace = malloc(textsize);

			if (lseek(fd, (off_t)phdr.p_offset, SEEK_SET) !=
			    (off_t)phdr.p_offset)
				fatal_error("cannot seek to text section");

			if (read(fd, textspace, textsize) != textsize)
				fatal_error("cannot read text");
		}
	}

	if (textsize == 0)
		fatal_error("can't find text segment");
}

#ifdef	DEBUG
static void
debug_dup_del(nltype * keeper, nltype * louser)
{
	printf("remove_dup_syms: discarding sym %s over sym %s\n",
		louser->name, keeper->name);
}
#endif

static void
remove_dup_syms(nltype *nl, sztype *sym_count)
{
	int	i;
	int	index;
	int	nextsym;

	nltype *	orig_list;
	if ((orig_list = malloc(sizeof (nltype) * *sym_count)) == NULL) {
		fprintf(stderr, "gprof: remove_dup_syms: malloc failed\n");
		fprintf(stderr, "Exiting due to error(s)...\n");
		exit(EX_UNAVAILABLE);
	}
	memcpy(orig_list, nl, sizeof (nltype) * *sym_count);

	for (i = 0, index = 0, nextsym = 1; nextsym < *sym_count; nextsym++) {
		int	i_type;
		int	n_bind;
		int	n_type;

		/*
		 * If orig_list[nextsym] points to a new symvalue, then we
		 * will copy our keeper and move on to the next symbol.
		 */
		if ((orig_list + i)->value < (orig_list + nextsym)->value) {
			*(nl + index++) = *(orig_list +i);
			i = nextsym;
			continue;
		}

		/*
		 * If these two symbols have the same info, then we
		 * keep the first and keep checking for dups.
		 */
		if ((orig_list + i)->syminfo ==
		    (orig_list + nextsym)->syminfo) {
			DEBUG_DUP_DEL(orig_list + i, orig_list + nextsym);
			continue;
		}
		n_bind = ELF32_ST_BIND((orig_list + nextsym)->syminfo);
		i_type = ELF32_ST_TYPE((orig_list + i)->syminfo);
		n_type = ELF32_ST_TYPE((orig_list + nextsym)->syminfo);

		/*
		 * If they have the same type we take the stronger
		 * bound function.
		 */
		if (i_type == n_type) {
			if (n_bind == STB_WEAK) {
				DEBUG_DUP_DEL((orig_list + i),
				    (orig_list + nextsym));
				continue;
			}
			DEBUG_DUP_DEL((orig_list + nextsym),
			    (orig_list + i));
			i = nextsym;
			continue;
		}

		/*
		 * If the first symbol isn't of type NOTYPE then it must
		 * be the keeper.
		 */
		if (i_type != STT_NOTYPE) {
			DEBUG_DUP_DEL((orig_list + i),
			    (orig_list + nextsym));
			continue;
		}

		/*
		 * Throw away the first one and take the new
		 * symbol
		 */
		DEBUG_DUP_DEL((orig_list + nextsym), (orig_list + i));
		i = nextsym;
	}

	if ((orig_list + i)->value > (nl + index - 1)->value)
		*(nl + index++) = *(orig_list +i);

	*sym_count = index;
}

/*
 * compare either by name or by value for sorting.
 * This is the comparison function called by qsort to
 * sort the symbols either by name or value when requested.
 */
static int
compare(nltype *a, nltype *b)
{
	if (a->value > b->value)
		return (1);
	else
		return ((a->value == b->value) - 1);
}

static int
is_function(Elf *elf, GElf_Sym *sym)
{
	Elf_Scn *scn;
	GElf_Shdr shdr;

	if (GELF_ST_TYPE(sym->st_info) == STT_FUNC) {
		if (GELF_ST_BIND(sym->st_info) == STB_GLOBAL)
			return (1);

		if (GELF_ST_BIND(sym->st_info) == STB_WEAK)
			return (1);

		if (!aflag && GELF_ST_BIND(sym->st_info) == STB_LOCAL)
			return (1);
	}

	/*
	 * It's not a function; determine if it's in an executable section.
	 */
	if (GELF_ST_TYPE(sym->st_info) != STT_NOTYPE)
		return (0);

	/*
	 * If it isn't global, and it isn't weak, and it either isn't
	 * local or the "all flag" isn't set, then get out.
	 */
	if (GELF_ST_BIND(sym->st_info) != STB_GLOBAL &&
	    GELF_ST_BIND(sym->st_info) != STB_WEAK &&
	    (GELF_ST_BIND(sym->st_info) != STB_LOCAL || aflag))
		return (0);

	if (sym->st_shndx >= SHN_LORESERVE)
		return (0);

	scn = elf_getscn(elf, sym->st_shndx);
	gelf_getshdr(scn, &shdr);

	if (!(shdr.sh_flags & SHF_EXECINSTR))
		return (0);

	return (1);
}

static void
get_symtab(Elf *elf, char *filename)
{
	Elf_Scn *scn = NULL, *sym = NULL;
	GElf_Word strndx = 0;
	sztype nsyms, i;
	Elf_Data *symdata;
	nltype *etext = NULL;

	while ((scn = elf_nextscn(elf, scn)) != NULL) {
		GElf_Shdr shdr;

		if (gelf_getshdr(scn, &shdr) == NULL)
			continue;

		if (shdr.sh_type == SHT_SYMTAB || shdr.sh_type == SHT_DYNSYM) {
			GElf_Xword chk = shdr.sh_size / shdr.sh_entsize;

			nsyms = (sztype)(shdr.sh_size / shdr.sh_entsize);

			if (chk != (GElf_Xword)nsyms)
				fatal_error("32-bit gprof cannot handle"
				    "more than 2^32 symbols");

			strndx = shdr.sh_link;
			sym = scn;
		}

		/*
		 * If we've found a real symbol table, we're done.
		 */
		if (shdr.sh_type == SHT_SYMTAB)
			break;
	}

	if (sym == NULL || strndx == 0)
		fatal_error("can't find symbol table.\n");

	if ((symdata = elf_getdata(scn, NULL)) == NULL)
		fatal_error("can't read symbol data.\n");

	if ((nl = npe = (nltype *)calloc(nsyms + PRF_SYMCNT,
	    sizeof (nltype))) == NULL)
		fatal_error("cannot allocate symbol data.\n");

	/*
	 * Now we need to cruise through the symbol table eliminating
	 * all non-functions from consideration, and making strings
	 * real.
	 */
	nname = 0;

	for (i = 1; i < nsyms; i++) {
		GElf_Sym gsym;
		char *name;

		gelf_getsym(symdata, i, &gsym);

		name = elf_strptr(elf, strndx, gsym.st_name);

		/*
		 * We're interested in this symbol if it's a function or
		 * if it's the symbol "_etext"
		 */
		if (is_function(elf, &gsym) || strcmp(name, PRF_ETEXT) == 0) {

			npe->name = name;
			npe->value = gsym.st_value;
			npe->sz = gsym.st_size;
			npe->syminfo = gsym.st_info;

			if (strcmp(name, PRF_ETEXT) == 0)
				etext = npe;

			if (lflag == TRUE &&
			    GELF_ST_BIND(gsym.st_info) == STB_LOCAL) {
				/*
				 * If the "locals only" flag is on, then
				 * we add the local symbols to the
				 * exclusion lists.
				 */
				addlist(Elist, name);
				addlist(elist, name);
			}
			DPRINTF("Index %lld:", nname);
			DPRINTF("\tValue: 0x%llx\t", npe->value);
			DPRINTF("Name: %s \n", npe->name);
			npe++;
			nname++;
		}
	}

	if (npe == nl)
		fatal_error("no valid functions found");

	/*
	 * Finally, we need to construct some dummy entries.
	 */
	if (etext) {
		npe->name = PRF_EXTSYM;
		npe->value = etext->value + 1;
		npe->syminfo = GELF_ST_INFO(STB_GLOBAL, STT_FUNC);
		npe++;
		nname++;
	}

	npe->name = PRF_MEMTERM;
	npe->value = (pctype)-1;
	npe->syminfo = GELF_ST_INFO(STB_GLOBAL, STT_FUNC);
	npe++;
	nname++;

	/*
	 * We're almost done;  all we need to do is sort the symbols
	 * and then remove the duplicates.
	 */
	qsort(nl, (size_t)nname, sizeof (nltype),
	    (int(*)(const void *, const void *)) compare);
	remove_dup_syms(nl, &nname);
}
