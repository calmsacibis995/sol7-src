/*
 *	Copyright (c) 1988 AT&T
 *	  All Rights Reserved
 *
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
 *	The copyright notice above does not evidence any
 *	actual or intended publication of such source code.
 *
 *	Copyright (c) 1996 by Sun Microsystems, Inc.
 *	All Rights Reserved
 */
#pragma ident	"@(#)_setup.c	1.6	98/01/06 SMI"

/*
 * SPARCV9-specific setup routine  -  relocate ld.so's symbols, setup its
 * environment, map in loadable sections of the executable.
 *
 * Takes base address ld.so was loaded at, address of ld.so's dynamic
 * structure, address of process environment pointers, address of auxiliary
 * vector and * argv[0] (process name).
 * If errors occur, send process signal - otherwise
 * return executable's entry point to the bootstrap routine.
 */
#include	"_synonyms.h"

#include	<signal.h>
#include	<stdlib.h>
#include	<sys/auxv.h>
#include	<sys/elf_SPARC.h>
#include	<sys/types.h>
#include	<sys/sysconfig.h>
#include	<sys/stat.h>
#include	<link.h>
#include	<unistd.h>
#include	<string.h>
#include	<dlfcn.h>
#include	"_rtld.h"
#include	"_elf.h"
#include	"msg.h"
#include	"_audit.h"
#include	"profile.h"
#include	"debug.h"

extern int	_end;
extern int	_etext;
extern void	_init(void);

extern void	elf_rtbndr(caddr_t pc, unsigned long reloc,
		    Rt_map * lmp, caddr_t from);
extern void	elf_rtbndr_err(unsigned long pltoff, unsigned long plt);



/*
 * Define for the executable's interpreter.
 * Usually it is ld.so.1, but for the first release of ICL binaries
 * it is libc.so.1.  We keep this information so that we don't end
 * up mapping libc twice if it is the interpreter.
 */
static Interp _interp;


/* VARARGS */
unsigned long
_setup(Boot * ebp, Dyn * ld_dyn)
{
	Addr		reladdr, etext, ld_base = 0;
	Addr		strtab, soname, entry, interp_base = 0;
	unsigned long	relacount;
	char *		c, * _rt_name, ** _envp, ** _argv, * _pr_name = 0;
	int		_syspagsz, phsize, phnum, i;
	int		_flags = 0;
	Dyn *		dyn_ptr;
	Phdr *		phdr;
	Ehdr *		ehdr;
	int		fd = -1;
	int		dz_fd = FD_UNAVAIL;
	Fct *		ftp;
	Rt_map *	lmp;
	auxv_t *	auxv, * _auxv;
	Addr		eaddr, esize;
	uid_t		uid = -1, euid = -1;
	gid_t		gid = -1, egid = -1;
	char *		_platform = 0;
	char *		_execname = 0;

	DEF_TIME(interval1);
	DEF_TIME(interval2);
	DEF_TIME(interval3);

	_GET_TIME(interval1);

	/*
	 * Scan the bootstrap structure to pick up the basics.
	 */
	for (; ebp->eb_tag != EB_NULL; ebp++)
		switch (ebp->eb_tag) {
		case EB_DYNAMIC: /* aoutflag in v8 */
			break;
		case EB_LDSO_BASE:
			ld_base = (Addr)ebp->eb_un.eb_val;
			break;
		case EB_ARGV:
			_argv = (char **)ebp->eb_un.eb_ptr;
			_pr_name = *_argv;
			break;
		case EB_ENVP:
			_envp = (char **)ebp->eb_un.eb_ptr;
			break;
		case EB_AUXV:
			_auxv = (auxv_t *)ebp->eb_un.eb_ptr;
			break;
		case EB_DEVZERO:
			/* LINTED */
			dz_fd = (int)ebp->eb_un.eb_val;
			break;
		case EB_PAGESIZE:
			/* LINTED */
			_syspagsz = (int)ebp->eb_un.eb_val;
			break;
		}

	/*
	 * Search the aux. vector for the information passed by exec.
	 */
	for (auxv = _auxv; auxv->a_type != AT_NULL; auxv++) {
		switch (auxv->a_type) {
		case AT_EXECFD:
			/* this is the old exec that passes a file descriptor */
			/* LINTED */
			fd = (int)auxv->a_un.a_val;
			break;
		case AT_FLAGS:
			/* processor flags (MAU available, etc) */
			/* LINTED */
			_flags = (int)auxv->a_un.a_val;
			break;
		case AT_PAGESZ:
			/* system page size */
			/* LINTED */
			_syspagsz = (int)auxv->a_un.a_val;
			break;
		case AT_PHDR:
			/* address of the segment table */
			phdr = (Phdr *) auxv->a_un.a_ptr;
			break;
		case AT_PHENT:
			/* size of each segment header */
			/* LINTED */
			phsize = (int)auxv->a_un.a_val;
			break;
		case AT_PHNUM:
			/* number of program headers */
			/* LINTED */
			phnum = (int)auxv->a_un.a_val;
			break;
		case AT_BASE:
			/* interpreter base address */
			if (ld_base == 0)
				ld_base = auxv->a_un.a_val;
			interp_base = auxv->a_un.a_val;
			break;
		case AT_ENTRY:
			/* entry point for the executable */
			entry = auxv->a_un.a_val;
			break;
		case AT_SUN_UID:
			/* effective user id for the executable */
			/* LINTED */
			euid = (uid_t)auxv->a_un.a_val;
			break;
		case AT_SUN_RUID:
			/* real user id for the executable */
			/* LINTED */
			uid = (uid_t)auxv->a_un.a_val;
			break;
		case AT_SUN_GID:
			/* effective group id for the executable */
			/* LINTED */
			egid = (gid_t)auxv->a_un.a_val;
			break;
		case AT_SUN_RGID:
			/* real group id for the executable */
			/* LINTED */
			gid = (gid_t)auxv->a_un.a_val;
			break;
#ifdef	AT_SUN_PLATFORM			/* Defined on SunOS 5.5 & greater. */
		case AT_SUN_PLATFORM:
			/* platform name */
			_platform = auxv->a_un.a_ptr;
			break;
#endif
#ifdef	AT_SUN_EXECNAME			/* Defined on SunOS 5.6 & greater. */
		case AT_SUN_EXECNAME:
			/* full pathname of execed object */
			_execname = auxv->a_un.a_ptr;
			break;
#endif
		}
	}

	/*
	 * Get needed info from ld.so's dynamic structure.
	 */
	/* LINTED */
	dyn_ptr = (Dyn *)((char *)ld_dyn + ld_base);
	for (ld_dyn = dyn_ptr; ld_dyn->d_tag != DT_NULL; ld_dyn++) {
		switch (ld_dyn->d_tag) {
		case DT_RELA:
			reladdr = ld_dyn->d_un.d_ptr + ld_base;
			break;
		case DT_RELACOUNT:
			relacount = ld_dyn->d_un.d_val;
			break;
		case DT_STRTAB:
			strtab = ld_dyn->d_un.d_ptr + ld_base;
			break;
		case DT_SONAME:
			soname = ld_dyn->d_un.d_val;
			break;
		}
	}
	_rt_name = (char *)strtab + soname;

	_GET_TIME(interval2);

	/*
	 * Because ld.so.1 is built with -Bsymbolic there
	 * should only be 'RELATIVE' relocations and
	 * JMPSLOT relocations.  We first process all of the
	 * relatives here.
	 */
	for (; relacount; relacount--) {
		Xword	roffset;

		roffset = ((Rel *)reladdr)->r_offset + ld_base;
		*((Xword *) roffset) = ld_base +
			((Rel *)reladdr)->r_addend;
		reladdr += sizeof (Rel);
	}

	_GET_TIME(interval3);
	_SAV_TIME(interval1, "begin");
	_SAV_TIME(interval2, "after boot block, aux and .dynamic");
	_SAV_TIME(interval3, "after ld.so.1 relocation");

	/*
	 * Now that ld.so has relocated itself, initialize any global variables.
	 * Initialize our own 'environ' so as to establish an address suitable
	 * for libc's hardware mul/div magic (libc/sparc/crt/hwmuldiv.o).
	 */
	_environ = (char **)((unsigned long)_auxv - sizeof (char *));
	_init();
	_environ = _envp;
	_GET_TIME(interval1);

	if (_pr_name)
		pr_name = (const char *)_pr_name;
	else
		pr_name = (const char *) MSG_INTL(MSG_STR_UNKNOWN);

	flags = _flags;
	if (dz_fd != FD_UNAVAIL)
		dz_init(dz_fd);
	platform = _platform;

	_GET_TIME(interval2);
	_SAV_TIME(interval1, ".init processed");
	_SAV_TIME(interval2, "dz initialized");

	/*
	 * If pagesize is unspecified find its value.
	 */
	if ((syspagsz = _syspagsz) == 0)
		syspagsz = _sysconfig(_CONFIG_PAGESIZE);

	/*
	 * Add the unused portion of the last data page to the free space list.
	 * The page size must be set before doing this.  Here, _end refers to
	 * the end of the runtime linkers bss.  Note that we do not use the
	 * unused data pages from any included .so's to supplement this free
	 * space as badly behaved .os's may corrupt this data space, and in so
	 * doing ruin our data.
	 */
	eaddr = S_DROUND((size_t)&_end);
	esize = eaddr % syspagsz;
	if (esize) {
		esize = syspagsz - esize;
		zero((caddr_t)eaddr, esize, 0);
		addfree((void *)eaddr, esize);
	}

	_GET_TIME(interval1);

	/*
	 * Initialize the fmap structure.
	 */
	if (fm_init() == NULL)
		exit(1);

	/*
	 * Determine whether we have a secure executable.
	 */
	security(uid, euid, gid, egid);

	_GET_TIME(interval2);

	/*
	 * We copy rtld's name here rather than just setting a pointer
	 * to it so that it will appear in the data segment and
	 * thus in any core file.
	 */
	if ((c = malloc(strlen(_rt_name) + 1)) == 0) {
		exit(1);
	}
	(void) strcpy(c, _rt_name);
	{
		rt_name = _rt_name;
		/*
		 * Get the filename of the rtld for use in any diagnostics (but
		 * save the full name in the link map for future comparisons)
		 */
		while (*c) {
			if (*c++ == '/')
				rt_name = c;
		}
	}


	/*
	 * Create a link map structure for ld.so.
	 */
	ftp = &elf_fct;
	if ((lmp = ftp->fct_new_lm(&lml_rtld, _rt_name, _rt_name,
	    dyn_ptr, ld_base, (unsigned long)&_etext,
	    (unsigned long)(eaddr - ld_base), 0, 0, 0, 0, ld_base,
	    (unsigned long)(eaddr - ld_base))) == 0) {
		exit(1);
	}
	COUNT(lmp) = 1;
	MODE(lmp) |= (RTLD_NODELETE | RTLD_GLOBAL | RTLD_WORLD);
	FLAGS(lmp) |= (FLG_RT_ANALYZED | FLG_RT_INITDONE | FLG_RT_FINIFRST);
	lml_rtld.lm_numfinifirst = 1;

	_GET_TIME(interval3);
	_SAV_TIME(interval1, "zero'ed bss");
	_SAV_TIME(interval2, "established security");
	_SAV_TIME(interval3, "generated ld.so.1 link-map");

	/*
	 * There is no need to analyze ld.so because we don't map in any of
	 * its dependencies.  However we may map these dependencies in later
	 * (as if ld.so had dlopened them), so initialize the plt and the
	 * permission information.
	 */
	if (PLTGOT(lmp)) {
		Xword pltoff;

		/*
		 * Install the lm pointer in .PLT2 as per the ABI.
		 */
		pltoff = (2 * M_PLT_ENTSIZE) / M_PLT_INSSIZE;
		elf_plt2_init(PLTGOT(lmp) + pltoff, lmp);

		/*
		 * The V9 ABI states that the first 32k PLT entries
		 * use .PLT1, with .PLT0 used by the "latter" entries.
		 * We don't currently implement the extendend format,
		 * so install an error handler in .PLT0 to catch anyone
		 * trying to use it.
		 */
		elf_plt_init(PLTGOT(lmp), (caddr_t)elf_rtbndr_err);


		/*
		 * Initialize .PLT1
		 */
		pltoff = M_PLT_ENTSIZE / M_PLT_INSSIZE;
		elf_plt_init(PLTGOT(lmp) + pltoff, (caddr_t)elf_rtbndr);
	}


	/*
	 * Look for environment strings (allows debugging to get switched on).
	 */
	if (!readenv((const char **)_envp, 0))
		exit(1);

	GET_TIME(interval1);
	SAV_TIME(interval1, "read environment");

#ifdef	PRF_RTLD
	/*
	 * Have we been requested to profile ourselves.
	 */
	if (profile_name)
		profile_rtld = profile_setup((Link_map *)lmp);
	PRF_MCOUNT(5, _setup);
#endif

	_flags = 0;
	/*
	 * Map in the file, if exec has not already done so.  If it has,
	 * simply create a new link map structure for the executable.
	 */
	if (fd != -1) {
		struct stat	status;

		/*
		 * Find out what type of object we have.
		 */
		(void) fstat(fd, &status);
		fmap->fm_fd = fd;
		fmap->fm_fsize = status.st_size;
		if ((ftp = are_u_this(pr_name)) == 0)
			exit(1);

		/*
		 * Map in object.
		 */
		if ((lmp = (ftp->fct_map_so)(&lml_main, 0, pr_name)) == 0)
			exit(1);

	} else {
		/*
		 * Set up function ptr and arguments according to the type
		 * of file class the executable is. (Currently only supported
		 * type is ELF)  Then create a link map for the executable.
		 */
		Phdr *		pptr;
		Phdr *		firstptr = 0;
		Phdr *		lastptr;
		Dyn *		dyn = 0;
		Off		i_offset;
		Addr		base = 0;
		char *		name = 0;
		unsigned long	memsize;

		/*
		 * Using the executables phdr address determine the base
		 * address of the input file.  Determine from the elf
		 * header if we're been called from a shared object or
		 * dynamic executable.  If the latter then any
		 * addresses within the object are to be used as is.
		 * The addresses within shared objects must be added to
		 * the process's base address.
		 */
		ehdr = (Ehdr *)((Addr)phdr - phdr->p_offset);
		if (ehdr->e_type == ET_DYN) {
			base = (Addr)ehdr;
			name = (char *)pr_name;
			_flags = 1;
		}

		/*
		 * Extract the needed information from the segment
		 * headers.
		 */
		for (i = 0, pptr = phdr; i < phnum; i++) {
			if (pptr->p_type == PT_INTERP) {
				interp = &_interp;
				i_offset = pptr->p_offset;
				interp->i_faddr =
				    (caddr_t)interp_base;
			}
			if ((pptr->p_type == PT_LOAD) &&
			    pptr->p_filesz) {
				if (!firstptr)
					firstptr = pptr;
				lastptr = pptr;
				if ((pptr->p_offset <= i_offset) &&
				    (i_offset <= (pptr->p_memsz +
				    pptr->p_offset))) {
					interp->i_name = (char *)
					    pptr->p_vaddr + i_offset -
					    pptr->p_offset + base;
				}
				if (!(pptr->p_flags & PF_W))
					etext = pptr->p_vaddr +
						pptr->p_memsz + base;
			} else if (pptr->p_type == PT_DYNAMIC)
				dyn = (Dyn *)(pptr->p_vaddr + base);
			pptr = (Phdr *)((unsigned long)pptr + phsize);
		}
		ftp = &elf_fct;
		memsize = (lastptr->p_vaddr + lastptr->p_memsz) -
			S_ALIGN(firstptr->p_vaddr, syspagsz);
		if (!(lmp = (ftp->fct_new_lm)(&lml_main, name, 0,
		    dyn, (Addr)ehdr, etext, memsize, entry,
		    phdr, phnum, phsize, (unsigned long)ehdr,
		    memsize))) {
			exit(1);
		}

		GET_TIME(interval1);
		SAV_TIME(interval1, "generated main's link-map\n");
	}

	/*
	 * Having mapped the executable in and created its link map, initialize
	 * the name and flags entries as necessary.  Note that any object that
	 * starts the process is identifed as `main', even shared objects.
	 * This assumes that the starting object will call .init and .fini from
	 * its own crt use (this is a pretty valid assumption as the crts also
	 * provide the necessary entry point).
	 */
	if (_flags == 0)
		NAME(lmp) = (char *)pr_name;
	FLAGS(lmp) |= FLG_RT_ISMAIN | FLG_RT_FINIFRST;
	lml_main.lm_numfinifirst = 1;
	PATHNAME(lmp) = _execname;

	/*
	 * Initialize debugger information structure.  Some parts of this
	 * structure were initialized statically.
	 */
	r_debug.r_map = (Link_map *)lmp;
	r_debug.r_ldsomap = (Link_map *)lml_rtld.lm_head;
	r_debug.r_ldbase = ld_base;

	/*
	 * Continue with generic startup processing.
	 */
	if (setup(lmp, (unsigned long)_envp, (unsigned long)_auxv) == 0) {
		exit(1);
	}

	DBG_CALL(Dbg_util_call_main(pr_name));
	return (LM_ENTRY_PT(lmp)());
}
