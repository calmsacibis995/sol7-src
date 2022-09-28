/*
 *	Copyright (c) 1996, 1997 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)elfdump.c	1.6	97/11/17 SMI"

/*
 * Dump an elf file.
 */
#include	<sys/param.h>
#include	<fcntl.h>
#include	<stdio.h>
#include	<libelf.h>
#include	<gelf.h>
#include	<link.h>
#include	<stdlib.h>
#include	<stdarg.h>
#include	<string.h>
#include	<unistd.h>
#include	<libintl.h>
#include	<locale.h>
#include	<errno.h>
#include	"debug.h"
#include	"msg.h"

#define	FLG_DYNAMIC	0x0001
#define	FLG_EHDR	0x0002
#define	FLG_INTERP	0x0004
#define	FLG_SHDR	0x0008
#define	FLG_NOTE	0x0010
#define	FLG_PHDR	0x0020
#define	FLG_RELOC	0x0040
#define	FLG_SYMBOLS	0x0080
#define	FLG_VERSIONS	0x0100
#define	FLG_HASH	0x0200
#define	FLG_GOT		0x0400
#define	FLG_SYMINFO	0x0800

#define	FLG_EVERYTHING	0xffff

typedef struct cache {
	GElf_Shdr	c_shdr;
	Elf_Data *	c_data;
	char *		c_name;
} Cache;

typedef struct got_info {
	GElf_Word	g_rshtype;	/* it will never happen, but */
					/* support mixed relocations */
	GElf_Rela	g_rel;
	const char *	g_symname;
} Got_info;

static const Cache _cache_init = {{0}, NULL, NULL};


const char *
_elfdump_msg(int mid)
{
	return (gettext(MSG_ORIG(mid)));
}

/*
 * Define our own printing routine.  All Elf routines referenced call upon
 * this routine to carry out the actual printing.
 */
/* LINTED */
void
dbg_print(const char * format, ...)
{
	va_list		ap;

	va_start(ap, format);
	(void) vprintf(format, ap);
	(void) printf(MSG_ORIG(MSG_STR_NL));
}


/*
 * Define our own standard error routine.
 */
static void
failure(const char * file, const char * func)
{
	(void) fprintf(stderr, MSG_INTL(MSG_ERR_FAILURE),
	    file, func, elf_errmsg(elf_errno()));
}



/*
 * Lookup a symbol named 'symname' and set Sym accordingly.
 *
 * Returns:
 *	1 - symbol vound
 *	0 - symbol not found
 */
static int
symlookup(const char * symname, Cache * cache, GElf_Ehdr * ehdr,
	GElf_Sym * sym)
{
	static Cache *	csymtab = 0;
	GElf_Sym	tsym;
	const char *	strs;
	int		i = 0;

	if (csymtab == 0) {
		int cnt;
		Cache *	symtab = 0;
		for (cnt = 1; cnt < ehdr->e_shnum; cnt++) {
			if (cache[cnt].c_shdr.sh_type == SHT_DYNSYM) {
				csymtab = &cache[cnt];
				break;
			} else if (cache[cnt].c_shdr.sh_type == SHT_SYMTAB) {
				symtab = &cache[cnt];
			}
		}
		if (!csymtab) {
			if (!symtab)
				return (0);
			csymtab = symtab;
		}
	}

	strs = cache[csymtab->c_shdr.sh_link].c_data->d_buf;
	while (gelf_getsym(csymtab->c_data, i++, &tsym) != NULL) {
		const char * sname;

		sname = strs + tsym.st_name;
		if (strcmp(symname, sname) == 0) {
			*sym = tsym;
			sym->st_name = (Word)sname;
			return (1);
		}
	}
	return (0);
}

/*
 * The full usage message
 */
static void
detail_usage()
{
	(void) fprintf(stderr, MSG_INTL(MSG_USAGE_DETAIL1));
	(void) fprintf(stderr, MSG_INTL(MSG_USAGE_DETAIL2));
	(void) fprintf(stderr, MSG_INTL(MSG_USAGE_DETAIL3));
	(void) fprintf(stderr, MSG_INTL(MSG_USAGE_DETAIL4));
	(void) fprintf(stderr, MSG_INTL(MSG_USAGE_DETAIL13));
	(void) fprintf(stderr, MSG_INTL(MSG_USAGE_DETAIL5));
	(void) fprintf(stderr, MSG_INTL(MSG_USAGE_DETAIL6));
	(void) fprintf(stderr, MSG_INTL(MSG_USAGE_DETAIL7));
	(void) fprintf(stderr, MSG_INTL(MSG_USAGE_DETAIL8));
	(void) fprintf(stderr, MSG_INTL(MSG_USAGE_DETAIL9));
	(void) fprintf(stderr, MSG_INTL(MSG_USAGE_DETAIL10));
	(void) fprintf(stderr, MSG_INTL(MSG_USAGE_DETAIL11));
	(void) fprintf(stderr, MSG_INTL(MSG_USAGE_DETAIL12));
	(void) fprintf(stderr, MSG_INTL(MSG_USAGE_DETAIL13));
	(void) fprintf(stderr, MSG_INTL(MSG_USAGE_DETAIL14));
}



/*
 * Print section headers.
 */
static void
sections(Cache * cache, GElf_Ehdr * ehdr, const char * name)
{
	unsigned int	cnt;
	Cache *		_cache;

	for (cnt = 1; cnt < ehdr->e_shnum; cnt++) {
		_cache = &cache[cnt];

		if (name && strcmp(name, _cache->c_name))
			continue;

		dbg_print(MSG_ORIG(MSG_STR_EMPTY));
		dbg_print(MSG_INTL(MSG_ELF_SCN_HEADER), cnt,
			_cache->c_name);
		Gelf_shdr_entry(&_cache->c_shdr);
	}
}

/*
 * Print the interpretor section.
 */
static void
interp(Cache * cache, GElf_Ehdr * ehdr)
{
	unsigned int	cnt;
	Cache *		_cache;

	for (cnt = 1; cnt < ehdr->e_shnum; cnt++) {
		_cache = &cache[cnt];

		if (strcmp(MSG_ORIG(MSG_ELF_INTERP), _cache->c_name))
			continue;

		dbg_print(MSG_ORIG(MSG_STR_EMPTY));
		dbg_print(MSG_INTL(MSG_ELF_SCN_INTERP));
		dbg_print(MSG_ORIG(MSG_FMT_INDENT),
		    (char *)_cache->c_data->d_buf);
		break;
	}
}

/*
 * Print the syminfo section.
 */
static void
syminfo(Cache * cache, GElf_Ehdr * ehdr, const char * file)
{
	unsigned int	cnt;
	unsigned int	ndx, sindx;
	GElf_Shdr *	shdr;
	GElf_Syminfo 	gsip;
	Elf_Data *	dsyminfo = 0;
	Elf_Data *	dsyms;
	Elf_Data *	ddyn;
	char *		strtab;
	char *		sect_name;

	for (sindx = 1; sindx < ehdr->e_shnum; sindx++) {
		if (cache[sindx].c_shdr.sh_type == SHT_SUNW_syminfo) {
			dsyminfo = cache[sindx].c_data;
			shdr = &(cache[sindx].c_shdr);
			sect_name = cache[sindx].c_name;
			break;
		}
	}
	if (dsyminfo == NULL)
		return;

	Gelf_syminfo_entry_title(sect_name);
	dsyms = cache[shdr->sh_link].c_data;
	strtab = (char *)cache[cache[shdr->sh_link].c_shdr.sh_link].
		c_data->d_buf;
	ddyn = cache[shdr->sh_info].c_data;
	cnt = shdr->sh_size / shdr->sh_entsize;
	for (ndx = 1; ndx < cnt; ndx++) {
		if (gelf_getsyminfo(dsyminfo, ndx, &gsip) == 0) {
			(void) fprintf(stderr, MSG_INTL(MSG_ERR_SIBADCOUNT),
				file, cache[sindx].c_name, ndx);
			return;
		}
		if (gsip.si_flags || gsip.si_boundto) {
			GElf_Sym 	gsym;
			GElf_Dyn	gdyn;
			char *		needed;
			char *		sname;
			if (gelf_getsym(dsyms, ndx, &gsym) == 0) {
				(void) fprintf(stderr, MSG_INTL(MSG_ERR_BADSYM),
					file, cache[shdr->sh_link].c_name,
					ndx);
				return;
			}
			sname = strtab + gsym.st_name;
			needed = 0;
			if (gsip.si_boundto < SYMINFO_BT_LOWRESERVE) {
				if (gelf_getdyn(ddyn, gsip.si_boundto,
				    &gdyn) == 0) {
					(void) fprintf(stderr,
					    MSG_INTL(MSG_ERR_BADDYN),
					    file, cache[sindx].c_name,
					    gsip.si_boundto);
					return;
				}
				needed = strtab + gdyn.d_un.d_val;
			}
			Gelf_syminfo_entry(ndx, &gsip, sname, needed);
		}
	}
}


/*
 * Search for and process any version sections.
 */
static GElf_Versym *
versions(Cache * cache, GElf_Ehdr * ehdr, const char * file)
{
	unsigned int	cnt;
	Cache *		_cache;
	char *		strs;
	void *		ver;
	GElf_Versym *	versym = 0;

	for (cnt = 1; cnt < ehdr->e_shnum; cnt++) {
		unsigned int	num;

		_cache = &cache[cnt];

		if ((_cache->c_shdr.sh_type < SHT_LOSUNW) ||
		    (_cache->c_shdr.sh_type > SHT_HISUNW))
			continue;

		ver = (void *)_cache->c_data->d_buf;

		/*
		 * If this is the version symbol table simply record its
		 * data address for possible use in later symbol processing.
		 */
		if (_cache->c_shdr.sh_type == SHT_SUNW_versym) {
			versym = (GElf_Versym *)ver;
			continue;
		}

		if ((_cache->c_shdr.sh_link == 0) ||
			(_cache->c_shdr.sh_link > ehdr->e_shnum)) {
			(void) fprintf(stderr, MSG_INTL(MSG_ERR_ZEROLINK),
			    file, _cache->c_name);
			continue;
		}

		/*
		 * Get the data buffer for the associated string table.
		 */
		strs = (char *)cache[_cache->c_shdr.sh_link].c_data->d_buf;
		num = _cache->c_shdr.sh_info;

		dbg_print(MSG_ORIG(MSG_STR_EMPTY));
		if (_cache->c_shdr.sh_type == SHT_SUNW_verdef) {
			dbg_print(MSG_INTL(MSG_ELF_SCN_VERDEF),
			    _cache->c_name);
			Gelf_ver_def_print((GElf_Verdef *)ver, num, strs);
		} else if (_cache->c_shdr.sh_type == SHT_SUNW_verneed) {
			dbg_print(MSG_INTL(MSG_ELF_SCN_VERNEED),
			    _cache->c_name);
			Gelf_ver_need_print((GElf_Verneed *)ver, num, strs);
		}
	}
	return (versym);
}

/*
 * Search for and process any symbol tables.
 */
static void
symbols(Cache * cache, GElf_Ehdr * ehdr, const char * name,
    GElf_Versym * versym, const char * file)
{
	unsigned int	cnt;
	Cache *		_cache;
	char *		strs;
	GElf_Sym 	sym;
	int		symn, _cnt;

	for (cnt = 1; cnt < ehdr->e_shnum; cnt++) {
		_cache = &cache[cnt];

		if ((_cache->c_shdr.sh_type != SHT_SYMTAB) &&
		    (_cache->c_shdr.sh_type != SHT_DYNSYM))
			continue;
		if (name && strcmp(name, _cache->c_name))
			continue;

		if ((_cache->c_shdr.sh_link == 0) ||
			(_cache->c_shdr.sh_link > ehdr->e_shnum)) {
			(void) fprintf(stderr, MSG_INTL(MSG_ERR_ZEROLINK),
			    file, _cache->c_name);
			continue;
		}

		/*
		 * Determine the symbol data and number.
		 */
		symn = _cache->c_shdr.sh_size / _cache->c_shdr.sh_entsize;

		/*
		 * Get the data buffer for the associated string table.
		 */
		strs = (char *)cache[_cache->c_shdr.sh_link].c_data->d_buf;

		/*
		 * loop through the symbol tables entries.
		 */
		dbg_print(MSG_ORIG(MSG_STR_EMPTY));
		dbg_print(MSG_INTL(MSG_ELF_SCN_SYMTAB), _cache->c_name);
		Gelf_sym_table_title(MSG_INTL(MSG_STR_INDEX),
		    MSG_INTL(MSG_STR_NAME));

		for (_cnt = 0; _cnt < symn; _cnt++) {
			char	index[10];
			char *	sec;
			int	verndx;

			(void) gelf_getsym(_cache->c_data, _cnt, &sym);
			if (sym.st_shndx < SHN_LORESERVE)
				sec = cache[sym.st_shndx].c_name;
			else
				sec = NULL;

			/*
			 * If versioning is available display the version index
			 * for any dynsym entries.
			 */
			if ((_cache->c_shdr.sh_type == SHT_DYNSYM) && versym)
				verndx = versym[_cnt];
			else
				verndx = 0;

			(void) sprintf(index, MSG_ORIG(MSG_FMT_INDEX), _cnt);
			Gelf_sym_table_entry(index, &sym,
				verndx, sec,
				strs ? strs + sym.st_name :
				MSG_INTL(MSG_STR_UNKNOWN));
		}
	}
}

/*
 * Search for and process any relocation sections.
 */
static void
reloc(Elf * elf, Cache * cache, GElf_Ehdr * ehdr, const char * name,
	const char * file)
{
	unsigned int	cnt;
	GElf_Shdr *	shdr, *_shdr;
	Cache *		_cache;
	char *		strs;
	Word		type;
	int		i, numrels;
	GElf_Xword	offset;


	for (cnt = 1; cnt < ehdr->e_shnum; cnt++) {
		_cache = &cache[cnt];
		shdr = &_cache->c_shdr;

		if (((type = _cache->c_shdr.sh_type) != SHT_RELA) &&
		    (type != SHT_REL))
			continue;
		if (name && strcmp(name, _cache->c_name))
			continue;


		/*
		 * Determine the number of relocations available.
		 */
		numrels = _cache->c_shdr.sh_size / _cache->c_shdr.sh_entsize;


		/*
		 * Get the data buffer for the associated symbol table.  Note
		 * that we've been known to create static binaries containing
		 * relocations against weak symbols, if these get stripped the
		 * relocation records can't make symbolic references.
		 */
		if ((shdr->sh_link == 0) || (shdr->sh_link > ehdr->e_shnum)) {
			(void) fprintf(stderr, MSG_INTL(MSG_ERR_ZEROLINK),
			    file, _cache->c_name);
		} else {
			/*
			 * Get the data buffer for the associated string table.
			 */
			_shdr = &cache[shdr->sh_link].c_shdr;
			strs = (char *)cache[_shdr->sh_link].c_data->d_buf;
		}

		/*
		 * There have been cases where invalid relocation offsets have
		 * been found. If we can establish the section being relocated
		 * we can compare the relocation offset with the sections size.
		 * If this is a generic relocation section (i.e., it contains
		 * entries that span more than one section) then use the full
		 * size of the object to verify the relocation offset.
		 */
		if ((shdr->sh_info) && (ehdr->e_type == ET_REL)) {
			_shdr = &cache[shdr->sh_info].c_shdr;
			offset = (GElf_Xword)_shdr->sh_size;
			if (_shdr->sh_addr)
				offset += (GElf_Xword)_shdr->sh_addr;
		} else if (ehdr->e_phnum) {
			int		_cnt;
			GElf_Phdr	phdr;

			for (_cnt = 0; _cnt < ehdr->e_phnum; _cnt++) {
				(void) gelf_getphdr(elf, _cnt, &phdr);
				if (phdr.p_type != PT_LOAD)
					continue;
				offset = (GElf_Xword)phdr.p_vaddr +
				    (GElf_Xword)phdr.p_memsz;
			}
		} else
			offset = 0;


		/*
		 * loop through the relocation entries.
		 */
		dbg_print(MSG_ORIG(MSG_STR_EMPTY));
		dbg_print(MSG_INTL(MSG_ELF_SCN_RELOC), _cache->c_name);
		if (type == SHT_RELA)
			dbg_print(MSG_INTL(MSG_ELF_RELOC_RELA));
		else
			dbg_print(MSG_INTL(MSG_ELF_RELOC_REL));

		for (i = 0; i < numrels; ++i) {
			char	section[24];
			char *	_name;
			int	ndx;
			GElf_Sym _sym;
			GElf_Rela rela;

			/*
			 * Determine the symbol with which this relocation is
			 * associated.  If the symbol represents a section
			 * offset construct an appropriate string.
			 */
			if (type == SHT_RELA) {
				(void) gelf_getrela(_cache->c_data, i, &rela);
				ndx = GELF_R_SYM(rela.r_info);
			} else {
				(void) gelf_getrel(_cache->c_data, i,
					    (GElf_Rel*)&rela);
				ndx = GELF_R_SYM(rela.r_info);
			}

			(void) gelf_getsym(cache[_cache->c_shdr.sh_link].c_data,
				    ndx, &_sym);
			if ((GELF_ST_TYPE(_sym.st_info) == STT_SECTION) &&
			    (_sym.st_name == 0)) {
				(void) sprintf(section,
				    MSG_INTL(MSG_STR_SECTION),
				    cache[_sym.st_shndx].c_name);
				_name = section;
			} else
				_name = strs + _sym.st_name;

			Gelf_reloc_entry(MSG_ORIG(MSG_STR_EMPTY),
			    ehdr->e_machine, type, (void *)&rela,
			    _cache->c_name, _name);
		}
	}
}

/*
 * Search for and process a .dynamic section.
 */
static void
dynamic(Cache * cache, GElf_Ehdr * ehdr, const char * file)
{
	unsigned int	cnt;
	Cache *		_cache;
	GElf_Dyn	dyn;
	char *		strs;
	int		ndx;
	int		numdyn;

	for (cnt = 1; cnt < ehdr->e_shnum; cnt++) {
		_cache = &cache[cnt];

		if (_cache->c_shdr.sh_type != SHT_DYNAMIC)
			continue;

		if ((_cache->c_shdr.sh_link == 0) ||
			(_cache->c_shdr.sh_link > ehdr->e_shnum)) {
			(void) fprintf(stderr, MSG_INTL(MSG_ERR_ZEROLINK),
			    file, _cache->c_name);
			continue;
		}

		/*
		 * Get the data buffer for the associated string
		 * table.
		 */
		strs = (char *)cache[_cache->c_shdr.sh_link].c_data->d_buf;

		dbg_print(MSG_ORIG(MSG_STR_EMPTY));
		dbg_print(MSG_INTL(MSG_ELF_SCN_DYNAMIC), _cache->c_name);
		Gelf_dyn_title();
		numdyn = _cache->c_shdr.sh_size / _cache->c_shdr.sh_entsize;
		for (ndx = 0; ndx < numdyn; ++ndx) {
			(void) gelf_getdyn(_cache->c_data, ndx, &dyn);
			if (dyn.d_tag == DT_NULL)
				break;
			Gelf_dyn_print(&dyn, ndx+1, strs);
		}
	}
}

/*
 * Search for and process a .note section.
 */
static void
note(Cache * cache, GElf_Ehdr * ehdr, const char * name)
{
	unsigned int	cnt;
	Cache *		_cache;

	for (cnt = 1; cnt < ehdr->e_shnum; cnt++) {
		_cache = &cache[cnt];

		if (_cache->c_shdr.sh_type != SHT_NOTE)
			continue;
		if (name && strcmp(name, _cache->c_name))
			continue;

		dbg_print(MSG_ORIG(MSG_STR_EMPTY));
		dbg_print(MSG_INTL(MSG_ELF_SCN_NOTE), _cache->c_name);
		Gelf_note_entry((Word *)_cache->c_data->d_buf);
	}
}


#define	MAXCOUNT	50

static void
hash(Cache * cache, GElf_Ehdr * ehdr, const char * name, const char * file)
{
	static int	count[MAXCOUNT];
	unsigned long	cnt;
	unsigned int *	hash, * chain;
	GElf_Shdr *	shdr;
	char *		strs;
	unsigned int	ndx, bkts;
	char		number[10];
	Cache *		_cache;

	for (cnt = 1; cnt < ehdr->e_shnum; cnt++) {
		_cache = &cache[cnt];

		if (_cache->c_shdr.sh_type != SHT_HASH)
			continue;
		if (name && strcmp(name, _cache->c_name))
			continue;

		if ((_cache->c_shdr.sh_link == 0) ||
			(_cache->c_shdr.sh_link > ehdr->e_shnum)) {
			(void) fprintf(stderr, MSG_INTL(MSG_ERR_ZEROLINK),
			    file, _cache->c_name);
			continue;
		}


		hash = (unsigned int *)_cache->c_data->d_buf;

		/*
		 * Determine the symbol tables associated string table.
		 */
		shdr = &cache[_cache->c_shdr.sh_link].c_shdr;
		strs = (char *)cache[shdr->sh_link].c_data->d_buf;


		bkts = *hash;
		chain = hash + 2 + bkts;
		hash += 2;

		dbg_print(MSG_ORIG(MSG_STR_EMPTY));
		dbg_print(MSG_INTL(MSG_ELF_SCN_HASH), _cache->c_name);
		dbg_print(MSG_INTL(MSG_ELF_HASH_INFO));

		/*
		 * Loop through the hash buckets, printing the appropriate
		 * symbols.
		 */
		for (ndx = 0; ndx < bkts; ndx++, hash++) {
			GElf_Sym	_sym;
			char *		_strs;
			Word		_ndx, _cnt;
			char		_number[10];

			if (*hash == 0) {
				count[0]++;
				continue;
			}

			(void) gelf_getsym(cache[_cache->c_shdr.sh_link].c_data,
				    *hash, &_sym);

			_strs = strs + _sym.st_name;
			(void) sprintf(number, MSG_ORIG(MSG_FMT_INTEGER), ndx);
			(void) sprintf(_number, MSG_ORIG(MSG_FMT_INDEX2),
			    *hash);
			dbg_print(MSG_ORIG(MSG_FMT_HASH_INFO), number, _number,
			    _strs);

			/*
			 * Determine if any other symbols are chained to this
			 * bucket.
			 */
			_ndx = chain[*hash];
			_cnt = 1;
			while (_ndx) {
				Word link = _cache->c_shdr.sh_link;

				(void) gelf_getsym(cache[link].c_data,	_ndx,
					&_sym);

				_strs = strs + _sym.st_name;
				(void) sprintf(_number,
				    MSG_ORIG(MSG_FMT_INDEX2), _ndx);
				dbg_print(MSG_ORIG(MSG_FMT_HASH_INFO),
				    MSG_ORIG(MSG_STR_EMPTY), _number, _strs);
				_ndx = chain[_ndx];
				_cnt++;
			}
			if (_cnt >= MAXCOUNT)
				(void) fprintf(stderr,
				    MSG_INTL(MSG_HASH_OVERFLW),
				    ndx, _cnt);
			else
				count[_cnt]++;
		}
		break;
	}

	/*
	 * Print out the count information.
	 */
	bkts = cnt = 0;
	dbg_print(MSG_ORIG(MSG_STR_EMPTY));
	for (ndx = 0; ndx < MAXCOUNT; ndx++) {
		Word	_cnt;

		if ((_cnt = count[ndx]) == 0)
			continue;

		(void) sprintf(number, MSG_ORIG(MSG_FMT_INTEGER), _cnt);
		dbg_print(MSG_INTL(MSG_ELF_HASH_BKTS1), number, ndx);
		bkts += _cnt;
		cnt += (ndx * _cnt);
	}
	if (cnt) {
		(void) sprintf(number, MSG_ORIG(MSG_FMT_INTEGER), bkts);
		dbg_print(MSG_INTL(MSG_ELF_HASH_BKTS2),
		    number, cnt);
	}
}


static void
got(Cache * cache, GElf_Ehdr * ehdr, const char * file)
{
	unsigned int	cnt;
	Cache *		gotcache = 0;
	Cache *		_cache;
	GElf_Addr	gotbgn;
	GElf_Addr	gotend;
	GElf_Shdr *	gotshdr;
	int		gotents;
	int		gotndx;
	Got_info *	gottable;
	char *		gotdata;
	GElf_Sym	gsym;
	GElf_Xword	gsymaddr;
	int		gentsize;


	/*
	 * First we find the got
	 */
	for (cnt = 1; cnt < ehdr->e_shnum; cnt++) {
		_cache = &cache[cnt];
		if (strncmp(_cache->c_name, MSG_ORIG(MSG_ELF_GOT), 4) == 0) {
			gotcache = _cache;
			break;
		}
	}
	if (!gotcache)
		return;
	gotshdr = &gotcache->c_shdr;
	gotbgn = gotshdr->sh_addr;
	gotend = gotbgn + gotshdr->sh_size;
	gentsize = gotshdr->sh_entsize;
	gotents = gotshdr->sh_size / gentsize;
	gotdata = gotcache->c_data->d_buf;
	if ((gottable = (Got_info *)calloc(gotents,
	    sizeof (Got_info))) == 0) {
		(void) fprintf(stderr, MSG_INTL(MSG_ERR_GOT_CALLOC),
			file, strerror(errno));
		return;
	}

	/*
	 * Now we scan through all the sections looking for
	 * any relocations that may be against the GOT.  Since
	 * these may not be isolated to a .rel[a].got section
	 * we check them all.
	 */
	for (cnt = 1; cnt < ehdr->e_shnum; cnt++) {
		GElf_Shdr *	shdr, * _shdr;
		GElf_Word	rtype;
		Elf_Data *	symdata;
		Elf_Data *	reldata;
		const char *	strs;
		GElf_Rela	rel;
		size_t		rndx;
		size_t		rcount;

		_cache = &cache[cnt];
		shdr = &_cache->c_shdr;
		rtype = shdr->sh_type;
		if ((rtype != SHT_RELA) && (rtype != SHT_REL))
			continue;

		rcount = shdr->sh_size / shdr->sh_entsize;
		reldata = _cache->c_data;
		(void) gelf_getrela(reldata, 0, &rel);

		if ((shdr->sh_link == 0) || (shdr->sh_link > ehdr->e_shnum)) {
			(void) fprintf(stderr, MSG_INTL(MSG_ERR_ZEROLINK),
				file, _cache->c_name);
			return;
		}
		symdata = cache[shdr->sh_link].c_data;

		/*
		 * Get the data buffer for the associated string table
		 */
		_shdr = &cache[shdr->sh_link].c_shdr;
		strs = (const char *)cache[_shdr->sh_link].c_data->d_buf;
		for (rndx = 1; rndx < rcount; ++rndx) {
			GElf_Sym 	_sym;
			int		ndx;
			GElf_Addr	offset;
			Got_info *	gip;

			if (rtype == SHT_RELA) {
				offset = rel.r_offset;
				ndx = GELF_R_SYM(rel.r_info);
			} else {
				offset = rel.r_offset;
				ndx = GELF_R_SYM(rel.r_info);
			}
			/*
			 * Only pay attention to relocations against the
			 * GOT.
			 */
			if ((offset < gotbgn) || (offset > gotend))
				continue;

			gotndx = (offset - gotbgn) / gotshdr->sh_entsize;
			gip = &gottable[gotndx];
			if (gip->g_rshtype != 0) {
				(void) fprintf(stderr,
				    MSG_INTL(MSG_GOT_MULTIPLE_1),
				    MSG_INTL(MSG_GOT_MULTIPLE_2),
				    file, gotndx, (GElf_Xword)offset);
				continue;
			}
			(void) gelf_getsym(symdata, ndx, &_sym);
			gip->g_rshtype = rtype;
			gip->g_rel = rel;
			gip->g_symname = strs + _sym.st_name;

			if (rtype == SHT_RELA)
				(void) gelf_getrela(reldata, rndx, &rel);
			else
				(void) gelf_getrel(reldata, rndx,
					(GElf_Rel *)&rel);
		}
	}

	if (symlookup(MSG_ORIG(MSG_GOT_SYM_NAME), cache, ehdr, &gsym))
		gsymaddr = gsym.st_value;
	else
		gsymaddr = gotbgn;

	Gelf_got_title(gotents);
	for (gotndx = 0; gotndx < gotents; gotndx++) {
		Got_info *	gip;
		int		gindex;
		GElf_Addr	gaddr;
		GElf_Xword	gotentry;

		gip = &gottable[gotndx];

		gaddr = gotbgn + (gotndx * gotshdr->sh_entsize);
		gindex = (int)((gaddr - gsymaddr) /
			gotshdr->sh_entsize);

		if (gentsize == sizeof (GElf_Word))
			/* LINTED */
			gotentry = (GElf_Xword)(*((GElf_Word *)(gotdata) +
			    gotndx));
		else
			/* LINTED */
			gotentry = *((GElf_Xword *)(gotdata) + gotndx);

		Gelf_got_entry(gindex, gaddr,
			gotentry, ehdr->e_machine, gip->g_rshtype,
			&gip->g_rel, gip->g_symname);
	}

	free(gottable);
}


static void
regular(const char * file, Elf * elf, unsigned long flags,
	char * Nname, int wfd)
{
	Elf_Scn *	scn;
	GElf_Ehdr	ehdr;
	Elf_Data *	data;
	unsigned int	cnt;
	char *		names = 0;
	Cache *		cache, * _cache;
	Versym *	versym = 0;

	if (gelf_getehdr(elf, &ehdr) == NULL) {
		failure(file, MSG_ORIG(MSG_ELF_GETEHDR));
		return;
	}

	/*
	 * Print the elf header.
	 */
	if (flags & FLG_EHDR)
		Gelf_elf_header(&ehdr);

	/*
	 * Print the program headers.
	 */
	if ((flags & FLG_PHDR) && ehdr.e_phnum) {
		GElf_Phdr phdr;

		for (cnt = 0; cnt < ehdr.e_phnum; cnt++) {
			if (gelf_getphdr(elf, cnt, &phdr) == NULL) {
				failure(file, MSG_ORIG(MSG_ELF_GETPHDR));
				return;
			}

			dbg_print(MSG_ORIG(MSG_STR_EMPTY));
			dbg_print(MSG_INTL(MSG_ELF_SCN_PHDRS), cnt);
			Gelf_phdr_entry(&phdr);
		}
	}


	/*
	 * If there are no sections (core files), or if we don't want
	 * any section information we might as well return now.
	 */
	if ((ehdr.e_shnum == 0) || (flags & ~(FLG_EHDR | FLG_PHDR)) == 0)
		return;

	/*
	 * Obtain the .shstrtab data buffer to provide the required section
	 * name strings.
	 */
	if ((scn = elf_getscn(elf, ehdr.e_shstrndx)) == NULL) {
		failure(file, MSG_ORIG(MSG_ELF_GETSCN));
		(void) fprintf(stderr, MSG_INTL(MSG_ELF_ERR_SHDR),
		    ehdr.e_shstrndx);
	} else if ((data = elf_getdata(scn, NULL)) == NULL) {
		failure(file, MSG_ORIG(MSG_ELF_GETDATA));
		(void) fprintf(stderr, MSG_INTL(MSG_ELF_ERR_DATA),
		    ehdr.e_shstrndx);
	} else
		names = data->d_buf;

	/*
	 * Fill in the cache descriptor with information for each section.
	 */
	if ((cache = (Cache *)malloc(ehdr.e_shnum * sizeof (Cache))) == 0) {
		(void) fprintf(stderr, MSG_INTL(MSG_ERR_MALLOC),
		    file, strerror(errno));
		return;
	}

	*cache = _cache_init;
	_cache = cache;
	_cache++;
	for (scn = NULL; scn = elf_nextscn(elf, scn); _cache++) {
		if (gelf_getshdr(scn, &_cache->c_shdr) == NULL) {
			failure(file, MSG_ORIG(MSG_ELF_GETSHDR));
			(void) fprintf(stderr, MSG_INTL(MSG_ELF_ERR_SCN),
			    elf_ndxscn(scn));
		}

		if (names)
			_cache->c_name = names + _cache->c_shdr.sh_name;
		else {
			if ((_cache->c_name = malloc(4)) == 0) {
				(void) fprintf(stderr, MSG_INTL(MSG_ERR_MALLOC),
				    file, strerror(errno));
				return;
			}
			(void) sprintf(_cache->c_name,
			    MSG_ORIG(MSG_FMT_UNSIGNED),
			    _cache->c_shdr.sh_name);
		}

		if ((_cache->c_data = elf_getdata(scn, NULL)) == NULL) {
			failure(file, MSG_ORIG(MSG_ELF_GETDATA));
			(void) fprintf(stderr, MSG_INTL(MSG_ELF_ERR_SCNDATA),
			    elf_ndxscn(scn));
		}

		/*
		 * Do we wish to write the section out?
		 */
		if (wfd && Nname && (strcmp(Nname, _cache->c_name) == 0)) {
			(void) write(wfd, _cache->c_data->d_buf,
			    _cache->c_data->d_size);
		}
	}

	if (flags & FLG_SHDR)
		sections(cache, &ehdr, Nname);

	if (flags & FLG_INTERP)
		interp(cache, &ehdr);

	if (flags & FLG_VERSIONS)
		versym = versions(cache, &ehdr, file);

	if (flags & FLG_SYMBOLS)
		symbols(cache, &ehdr, Nname, versym, file);

	if (flags & FLG_HASH)
		hash(cache, &ehdr, Nname, file);

	if (flags & FLG_GOT)
		got(cache, &ehdr, file);

	if (flags & FLG_SYMINFO)
		syminfo(cache, &ehdr, file);

	if (flags & FLG_RELOC)
		reloc(elf, cache, &ehdr, Nname, file);

	if (flags & FLG_DYNAMIC)
		dynamic(cache, &ehdr, file);

	if (flags & FLG_NOTE)
		note(cache, &ehdr, Nname);

	free(cache);
}

static void
archive(const char * file, int fd, Elf * elf, unsigned long flags,
	char * Nname, int wfd)
{
	Elf_Cmd		cmd = ELF_C_READ;
	Elf_Arhdr *	arhdr;
	Elf *		_elf;

	/*
	 * Determine if the archive sysmbol table itself is required.
	 */
	if ((flags & FLG_SYMBOLS) &&
	    ((Nname == NULL) || (strcmp(Nname, MSG_ORIG(MSG_ELF_ARSYM))
	    == 0))) {
		Elf_Arsym *	arsym;
		size_t		cnt, ptr;
		char		index[10];
		size_t		offset = 0, _offset = 0;

		/*
		 * Get the archive symbol table.
		 */
		if ((arsym = elf_getarsym(elf, &ptr)) == 0) {
			failure(file, MSG_ORIG(MSG_ELF_GETARSYM));
			return;
		}

		/*
		 * Print out all the symbol entries.
		 */
		dbg_print(MSG_INTL(MSG_ARCHIVE_SYMTAB));
		dbg_print(MSG_INTL(MSG_ARCHIVE_FIELDS));
		for (cnt = 0; cnt < ptr; cnt++, arsym++) {
			/*
			 * For each object obtain an elf descriptor so that we
			 * can establish the members name.
			 */
			if ((offset == 0) || ((arsym->as_off != 0) &&
			    (arsym->as_off != _offset))) {
				if (elf_rand(elf, arsym->as_off) !=
				    arsym->as_off) {
					failure(file, MSG_ORIG(MSG_ELF_RAND));
					return;
				}
				if (!(_elf = elf_begin(fd, ELF_C_READ, elf))) {
					failure(file, MSG_ORIG(MSG_ELF_BEGIN));
					return;
				}
				if ((arhdr = elf_getarhdr(_elf)) == NULL) {
					failure(file,
					    MSG_ORIG(MSG_ELF_GETARHDR));
					return;
				}
				_offset = arsym->as_off;
				if (offset == 0)
					offset = _offset;
			}

			(void) sprintf(index, MSG_ORIG(MSG_FMT_INDEX), cnt);
			if (arsym->as_off)
				dbg_print(MSG_ORIG(MSG_FMT_ARSYM1), index,
				    arsym->as_off, arhdr->ar_name,
				    (arsym->as_name ? arsym->as_name :
					MSG_INTL(MSG_STR_NULL)));
			else
				dbg_print(MSG_ORIG(MSG_FMT_ARSYM2), index,
				    arsym->as_off);

			(void) elf_end(_elf);
		}

		/*
		 * If we only need the archive symbol table return.
		 */
		if ((flags == FLG_SYMBOLS) && Nname &&
		    (strcmp(Nname, MSG_ORIG(MSG_ELF_ARSYM)) == 0))
			return;

		/*
		 * Reset elf descriptor in preparation for processing each
		 * member.
		 */
		if (offset)
			(void) elf_rand(elf, offset);
	}

	/*
	 * Process each object within the archive.
	 */
	while ((_elf = elf_begin(fd, cmd, elf)) != NULL) {
		char	name[MAXPATHLEN];

		if ((arhdr = elf_getarhdr(_elf)) == NULL) {
			failure(file, MSG_ORIG(MSG_ELF_GETARHDR));
			return;
		}
		if (*arhdr->ar_name != '/') {
			(void) sprintf(name, MSG_ORIG(MSG_FMT_ARNAME),
			    file, arhdr->ar_name);
			dbg_print(MSG_ORIG(MSG_FMT_NLSTR), name);

			switch (elf_kind(_elf)) {
			case ELF_K_AR:
				archive(name, fd, _elf, flags, Nname, wfd);
				break;
			case ELF_K_ELF:
				regular(name, _elf, flags, Nname, wfd);
				break;
			default:
				(void) fprintf(stderr,
					MSG_INTL(MSG_ERR_BADFILE),
					name);
				break;
			}
		}

		cmd = elf_next(_elf);
		(void) elf_end(_elf);
	}
}


int
main(int argc, char ** argv)
{
	Elf *		elf;
	int		var, fd, wfd = 0;
	char *		Nname = NULL, * wname = NULL;
	unsigned long	flags = 0;

	/*
	 * Establish locale.
	 */
	(void) setlocale(LC_MESSAGES, MSG_ORIG(MSG_STR_EMPTY));
	(void) textdomain(MSG_ORIG(MSG_SUNW_OST_SGS));

	opterr = 0;
	while ((var = getopt(argc, argv, MSG_ORIG(MSG_STR_OPTIONS))) != EOF) {
		switch (var) {
		case 'c':
			flags |= FLG_SHDR;
			break;
		case 'd':
			flags |= FLG_DYNAMIC;
			break;
		case 'e':
			flags |= FLG_EHDR;
			break;
		case 'h':
			flags |= FLG_HASH;
			break;
		case 'i':
			flags |= FLG_INTERP;
			break;
		case 'n':
			flags |= FLG_NOTE;
			break;
		case 'p':
			flags |= FLG_PHDR;
			break;
		case 'r':
			flags |= FLG_RELOC;
			break;
		case 's':
			flags |= FLG_SYMBOLS;
			break;
		case 'w':
			wname = optarg;
			break;
		case 'v':
			flags |= FLG_VERSIONS;
			break;
		case 'G':
			flags |= FLG_GOT;
			break;
		case 'y':
			flags |= FLG_SYMINFO;
			break;
		case 'N':
			Nname = optarg;
			break;
		case '?':
			(void) fprintf(stderr, MSG_INTL(MSG_USAGE_BRIEF),
			    argv[0]);
			detail_usage();
			exit(1);
		default:
			break;
		}
	}

	/*
	 * Validate any arguments.
	 */
	if (flags == 0) {
		if (wname || Nname) {
			(void) fprintf(stderr, MSG_INTL(MSG_ERR_USAGE));
			exit(1);
		}
		flags = FLG_EVERYTHING;
	}
	if ((var = argc - optind) == 0) {
		(void) fprintf(stderr, MSG_INTL(MSG_USAGE_BRIEF), argv[0]);
		exit(1);
	}

	/*
	 * If the -w option has indicated an output file open it.  It's
	 * arguable whether this option has much use when multiple files are
	 * being processed.
	 */
	if (wname) {
		if ((wfd = open(wname, (O_RDWR | O_CREAT | O_TRUNC),
		    0666)) < 0) {
			(void) fprintf(stderr, MSG_INTL(MSG_ERR_OPEN),
			    wname, strerror(errno));
			wfd = 0;
		}
	}

	/*
	 * Open the input file and initialize the elf interface.
	 */
	for (; optind < argc; optind++) {
		const char *	file = argv[optind];

		if ((fd = open(argv[optind], O_RDONLY)) == -1) {
			(void) fprintf(stderr, MSG_INTL(MSG_ERR_OPEN),
			    file, strerror(errno));
			continue;
		}
		(void) elf_version(EV_CURRENT);
		if ((elf = elf_begin(fd, ELF_C_READ, NULL)) == NULL) {
			failure(file, MSG_ORIG(MSG_ELF_BEGIN));
			(void) close(fd);
			continue;
		}

		if (var > 1)
			dbg_print(MSG_ORIG(MSG_FMT_NLSTRNL), file);

		switch (elf_kind(elf)) {
		case ELF_K_AR:
			archive(file, fd, elf, flags, Nname, wfd);
			break;
		case ELF_K_ELF:
			regular(file, elf, flags, Nname, wfd);
			break;
		default:
			(void) fprintf(stderr, MSG_INTL(MSG_ERR_BADFILE), file);
			break;
		}

		(void) close(fd);
		(void) elf_end(elf);
	}

	if (wfd)
		(void) close(wfd);
	return (0);
}
