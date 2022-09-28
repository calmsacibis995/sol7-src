/*
 *	Copyright (c) 1988 AT&T
 *	  All Rights Reserved
 *
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
 *	The copyright notice above does not evidence any
 *	actual or intended publication of such source code.
 *
 *	Copyright (c) 1996 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)setup.c	1.49	98/01/05 SMI"


/*
 * Run time linker common setup.
 *
 * Called from _setup to get the process going at startup.
 */
#include	"_synonyms.h"

#include	<string.h>
#include	<stdio.h>
#include	<limits.h>
#include	<unistd.h>
#include	<dlfcn.h>
#include	"_rtld.h"
#include	"_audit.h"
#include	"_elf.h"
#include	"profile.h"
#include	"debug.h"

extern char **	environ;

#if	defined(i386)
extern int	_elf_copy_gen(Rt_map *);
#endif


int
setup(Rt_map * lmp, unsigned long envp, unsigned long auxv)
{
	Listnode *	lnp1, * lnp2;
	List *		list;
	Rt_map *	nlmp;
	const char *	name;
	Rel_copy *	rcp;
	int		error = 0;
	Rt_map **	tobj;

	DEF_TIME(interval1);
	DEF_TIME(interval2);
	DEF_TIME(interval3);

	PRF_MCOUNT(49, setup);

	/*
	 * Alert the debuggers that we are about to mess with the link-map list
	 * (dbx doesn't care about this as it doesn't know we're around until
	 * the getpid() call later, prehaps adb needs this state information).
	 */
	rtld_flags |= RT_FL_DBNOTIF;
	rd_event(RD_DLACTIVITY, RT_ADD, rtld_db_dlactivity());

	/*
	 * Determine whether the kernel has supplied a AT_SUN_EXECNAME aux
	 * vector.  This vector points to the full pathname, on the stack, of
	 * the object that started the process.  If this is null then
	 * AT_SUN_EXECNAME isn't supported (if the pathname exceeded the system
	 * limit (PATH_MAX) the exec would have failed).
	 */
	if ((name = PATHNAME(lmp)) != 0)
		rtld_flags |= RT_FL_EXECNAME;
	else
		PATHNAME(lmp) = NAME(lmp);

	/*
	 * Far the most common application execution revolves around appending
	 * the application name to the users PATH definition, thus a full name
	 * is passed to exec() which will in turn be returned via
	 * AT_SUN_EXECNAME.  Applications may also be invoked from the current
	 * working directory, or via a relative name.
	 *
	 * By default, the expansion of a relative pathname is deferred until
	 * it is required.  However, should a process change directory before a
	 * full pathname is required, it may be necessary to determine the
	 * pathname in advance.
	 */
	if (FLAGS(lmp) & FLG_RT_ORIGIN)
		(void) fullpath(lmp);


	if (platform)
		platform_sz = strlen(platform);

	/*
	 * Initialize the cache.
	 */
	name = NAME(lmp);

	if (!(rtld_flags & RT_FL_NOCACHE))
		error = elf_cache(&name);

	/*
	 * If debugging was requested initialize things now that any cache has
	 * been established.  If we've been called via `ldd(1) -s' disable the
	 * tracing while processing the debug library.
	 */
	if (dbg_str) {
		int	_tracing = tracing;
		tracing = 0;

		dbg_mask |= dbg_setup(dbg_str);

		tracing = _tracing;
	}

	/*
	 * Establish the modes of the intial object.  These modes are
	 * propagated to any preloaded objects and explicit shared library
	 * dependencies.
	 */
	MODE(lmp) |= (bind_mode | RTLD_NODELETE | RTLD_GLOBAL | RTLD_WORLD);
	COUNT(lmp) = 1;

	/*
	 * Add our two main link-maps to the dynlm_list
	 */
	if (list_append(&dynlm_list, &lml_rtld) == 0)
		return (0);
	if (list_append(&dynlm_list, &lml_main) == 0)
		return (0);

	if (audit_libraries)
		(void) audit_setup(audit_libraries);

	/*
	 * Now that debugging is enabled generate any diagnostics from any
	 * previous events.
	 */
	if (error) {
		DBG_CALL(Dbg_file_cache_dis(name, error));
	}
	if (dbg_mask) {
		Rt_map *	lmp = lml_rtld.lm_head;

		DBG_CALL(Dbg_file_ldso(rt_name, (unsigned long)DYN(lmp),
		    ADDR(lmp), envp, auxv));
		lmp = lml_main.lm_head;
		DBG_CALL(Dbg_file_elf(PATHNAME(lmp), (unsigned long)DYN(lmp),
		    ADDR(lmp), MSIZE(lmp), ENTRY(lmp), (unsigned long)PHDR(lmp),
		    PHNUM(lmp), get_linkmap_id(LIST(lmp))));
	}

	/*
	 * Map in any preloadable shared objects.  Note, it is valid to preload
	 * a 4.x shared object with a 5.0 executable (or visa-versa), as this
	 * functionality is required by ldd(1).
	 */
	if (preload.head) {
		DBG_CALL(Dbg_util_nl());
		for (LIST_TRAVERSE(&preload, lnp1, name)) {
			if (is_so_loaded(&lml_main, name) == 0) {
				DBG_CALL(Dbg_file_preload(name));

				/*
				 * If this is a secure application only allow
				 * simple filenames to be preloaded. The lookup
				 * for these files will be restricted, but is
				 * allowed by placing preloaded objects in
				 * secure directories.
				 */
				if (rtld_flags & RT_FL_SECURE) {
					if (strchr(name, '/')) {
						DBG_CALL(Dbg_libs_ignore(name));
						continue;
					}
				}
				if ((nlmp = load_so(&lml_main, name, lmp)) == 0)
					if (tracing)
						continue;
					else
						return (0);

				FLAGS(nlmp) |= FLG_RT_PRELOAD;
				MODE(nlmp) |= (bind_mode | RTLD_NODELETE |
				    RTLD_GLOBAL | RTLD_WORLD);
				COUNT(nlmp) = 1;
			}
		}
	}

	/*
	 * Load all dependent (needed) objects.
	 */
	if (analyze_so(&lml_main, lmp) == 0)
		return (0);

	DBG_CALL(Dbg_file_nl());

	/*
	 * Relocate all the initial dependencies we've just added.
	 */
	if (relocate_so(lmp) == 0)
		return (0);

	/*
	 * reverse topological sort the dependency for .init execution.
	 */
	if ((tobj = tsort(lmp, RT_SORT_REV)) ==
	    (Rt_map **)S_ERROR)
		return (0);

	/*
	 * Perform special copy type relocations.
	 */
	if (!tracing || (rtld_flags & RT_FL_WARN)) {
#if	defined(i386)
		if (_elf_copy_gen(lmp) == 0)
			return (0);
#endif
		for (LIST_TRAVERSE(&COPY(lmp), lnp1, list)) {
			for (LIST_TRAVERSE(list, lnp2, rcp)) {
				(void) memcpy(rcp->r_radd, rcp->r_dadd,
				    rcp->r_size);
			}
		}
	}

	/*
	 * If we are tracing we're done.
	 */
	if (tracing)
		exit(0);

	/*
	 * Lookup up the _environ symbol for getenv() support in .init sections.
	 */
	set_environ(lmp);

	/*
	 * Inform the debuggers we're here and stable.  Newer debuggers can
	 * indicate their presence by setting the DT_DEBUG entry in the dynamic
	 * executable (see elf_new_lm()).  In this case call getpid() so that
	 * the debugger can catch the system call.  This allows the debugger to
	 * initialize at this point and consequently allows the user to set
	 * break points in .init code.
	 */
	rd_event(RD_DLACTIVITY, RT_CONSISTENT, rtld_db_dlactivity());
	rtld_flags &= ~ RT_FL_DBNOTIF;

	if (rtld_flags & RT_FL_DEBUGGER) {
		r_debug.r_flags |= RD_FL_ODBG;
		(void) getpid();
	}

	/*
	 * Call all dependencies .init sections and clean up any file
	 * descriptors we've opened for ourselves.
	 */
	rtld_flags |= RT_FL_APPLIC;

	rd_event(RD_PREINIT, 0, rtld_db_preinit());

	GET_TIME(interval1);


	if (rtld_flags & RT_FL_AUDITING)
		audit_preinit(lml_main.lm_head);

	if (rtld_flags & RT_FL_CLEANUP)
		cleanup();

	if (tobj != (Rt_map **) NULL)
		call_init(tobj);

	GET_TIME(interval2);

	rd_event(RD_POSTINIT, 0, rtld_db_postinit());

	GET_TIME(interval3);
	SAV_TIME(interval1, "initialization done");
	SAV_TIME(interval2, ".inits called");
	SAV_TIME(interval3, "cleanup done");

#ifdef	TIMING
	if (rtld_flags & RT_FL_TIMING)
		r_times();
#endif
	return (1);
}
