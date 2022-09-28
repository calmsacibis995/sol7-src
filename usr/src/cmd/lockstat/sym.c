/*
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)sym.c	1.2	97/07/16 SMI"

#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <libelf.h>
#include <link.h>
#include <elf.h>
#include <sys/machelf.h>

#include <kstat.h>
#include <sys/cpuvar.h>

typedef struct syment {
	uintptr_t	addr;
	char		*name;
	size_t		size;
} syment_t;

syment_t *symbol_table;
int nsyms, nfake;

#define	NFAKE	1000	/* maximum number of faked-up symbols */

#ifdef _ELF64
#define	elf_getshdr elf64_getshdr
#else
#define	elf_getshdr elf32_getshdr
#endif

static void
fake_up_certain_popular_kernel_symbols(void)
{
	kstat_ctl_t *kc;
	kstat_t *ksp;
	syment_t *sep;

	if ((kc = kstat_open()) == NULL)
		return;

	for (ksp = kc->kc_chain; ksp; ksp = ksp->ks_next) {
		if (strcmp(ksp->ks_module, "cpu_info") == 0) {
			char *name;

			if (++nfake >= NFAKE)
				break;
			if ((name = malloc(20)) == NULL)
				break;
			sprintf(name, "cpu[%d]", ksp->ks_instance);
			sep = &symbol_table[nsyms++];
			sep->addr = (uintptr_t)ksp->ks_private;
			sep->size = sizeof (struct cpu);
			sep->name = name;
		}
	}
	kstat_close(kc);
}

static int
symcmp(const void *p1, const void *p2)
{
	uintptr_t a1 = ((syment_t *)p1)->addr;
	uintptr_t a2 = ((syment_t *)p2)->addr;

	if (a1 < a2)
		return (-1);
	if (a1 > a2)
		return (1);
	return (0);
}

int
symtab_init(void)
{
	Elf		*elf;
	Elf_Scn		*scn = NULL;
	Sym		*symtab, *symp, *lastsym;
	char		*strtab;
	syment_t	*sep;
	u_int		cnt;
	int		fd;
	int		i;
	int		strindex = -1;

	if ((fd = open("/dev/ksyms", O_RDONLY)) == -1)
		return (-1);

	(void) elf_version(EV_CURRENT);

	elf = elf_begin(fd, ELF_C_READ, NULL);

	for (cnt = 1; (scn = elf_nextscn(elf, scn)) != NULL; cnt++) {
		Shdr *shdr = elf_getshdr(scn);
		if (shdr->sh_type == SHT_SYMTAB) {
			symtab = (Sym *)elf_getdata(scn, NULL)->d_buf;
			nsyms = shdr->sh_size / shdr->sh_entsize;
			strindex = shdr->sh_link;
		}
	}

	for (cnt = 1; (scn = elf_nextscn(elf, scn)) != NULL; cnt++) {
		if (cnt == strindex)
			strtab = (char *)elf_getdata(scn, NULL)->d_buf;
	}

	if ((symbol_table = calloc(nsyms + NFAKE, sizeof (syment_t))) == NULL)
		return (-1);

	lastsym = symtab + nsyms;
	nsyms = 0;
	for (symp = symtab; symp < lastsym; symp++) {
		if ((u_int)ELF32_ST_TYPE(symp->st_info) <= STT_FUNC) {
			sep = &symbol_table[nsyms++];
			sep->addr = (uintptr_t)symp->st_value;
			sep->size = (size_t)symp->st_size;
			sep->name = symp->st_name + strtab;
		}
	}

	fake_up_certain_popular_kernel_symbols();

	qsort(symbol_table, nsyms, sizeof (syment_t), symcmp);

	/*
	 * Destroy all duplicate symbols, then sort it again.
	 */
	for (i = 0; i < nsyms - 1; i++)
		if (symbol_table[i].addr == symbol_table[i + 1].addr)
			symbol_table[i].addr = 0;

	qsort(symbol_table, nsyms, sizeof (syment_t), symcmp);

	while (symbol_table[1].addr == 0) {
		symbol_table++;
		nsyms--;
	}
	symbol_table[0].name = "(wherever)";

	return (0);
}

char *
addr_to_sym(uintptr_t addr, uintptr_t *offset, size_t *sizep)
{
	int lo = 0;
	int hi = nsyms - 1;
	int mid;
	syment_t *sep;

	while (hi - lo > 1) {
		mid = (lo + hi) / 2;
		if (addr >= symbol_table[mid].addr) {
			lo = mid;
		} else {
			hi = mid;
		}
	}
	sep = &symbol_table[lo];
	*offset = addr - sep->addr;
	*sizep = sep->size;
	return (sep->name);
}

uintptr_t
sym_to_addr(char *name)
{
	int i;
	syment_t *sep = symbol_table;

	for (i = 0; i < nsyms; i++) {
		if (strcmp(name, sep->name) == 0)
			return (sep->addr);
		sep++;
	}
	return (NULL);
}

size_t
sym_size(char *name)
{
	int i;
	syment_t *sep = symbol_table;

	for (i = 0; i < nsyms; i++) {
		if (strcmp(name, sep->name) == 0)
			return (sep->size);
		sep++;
	}
	return (0);
}
