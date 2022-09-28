/*
 *	Copyright (c) 1988 AT&T
 *	  All Rights Reserved
 *
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
 *	The copyright notice above does not evidence any
 *	actual or intended publication of such source code.
 *
 *	Copyright (c) 1997 by Sun Microsystems, Inc.
 *	All rights reserved.
 */
#pragma ident	"@(#)analyze.c	1.62	97/03/04 SMI"

/*
 * If the environment flag LD_TRACE_LOADED_OBJECTS is set, we load
 * all objects, as above, print out the path name of each, and then exit.
 * If LD_WARN is also set, we also perform relocations, printing out a
 * diagnostic for any unresolved symbol.
 */
#include	"_synonyms.h"

#include	<string.h>
#include	<stdio.h>
#include	<unistd.h>
#include	<sys/stat.h>
#include	<sys/mman.h>
#include	<fcntl.h>
#include	<limits.h>
#include	<dlfcn.h>
#include	<errno.h>
#include	"_rtld.h"
#include	"_audit.h"
#include	"_elf.h"
#include	"msg.h"
#include	"profile.h"
#include	"debug.h"

static Fct *	vector[] = {
	&elf_fct,
#ifdef A_OUT
	&aout_fct,
#endif A_OUT
	0
};

static void
remove_so_core(Lm_list * lml)
{
	Rt_map *	lmp, *nlmp, *tlmp, *tlmp2;
	char *		oname;
	void (*		fptr)();

	PRF_MCOUNT(50, remove_so);

	/*
	 * Traverse the link map list looking for deletable candidates.
	 */
	for (lmp = lml->lm_head; lmp; lmp = nlmp) {
		Dl_obj *	dlp;
		Listnode *	lnp, *olnp, *lnp2;
		Pnode *		pnp, *opnp;

		nlmp = (Rt_map *)NEXT(lmp);

		/*
		 * If the objects reference count has dropped to zero it
		 * can be deleted as it is presently unreferenced.  Note
		 * that objects that aren't FLG_RT_ANALYZED are also
		 * applicable for deletion - these are objects that
		 * (via dlopen()) didn't get relocated correctly and
		 * hence we're here to clean them up.
		 */
		if (MODE(lmp) & RTLD_NODELETE)
			continue;

		if (COUNT(lmp) && (FLAGS(lmp) & FLG_RT_ANALYZED)) {
			Listnode *	flnp;

			/*
			 * While we're here determine if any dangling
			 * permits are around.  If an object has a
			 * reference to a permit structure whose count
			 * is 1, then this object is the only user of
			 * that permit so it can be freed (this is
			 * possible when the object dlopen'ed others
			 * using the RTLD_PARENT mode).
			 */
			flnp = olnp = 0;
			DBG_CALL(Dbg_file_bind_title(REF_UNPERMIT));
			for (LIST_TRAVERSE(&REFROM(lmp), lnp, dlp)) {
				if (flnp) {
					free(flnp);
					flnp = 0;
				}
				if (dlp->dl_permcnt != 1) {
					olnp = lnp;
					continue;
				}

				PERMIT(lmp) = perm_unset(PERMIT(lmp),
				    dlp->dl_permit);
				dlp_free(dlp);

				if (olnp)
					olnp->next = lnp->next;
				else
					REFROM(lmp).head = lnp->next;
				if (lnp->next == 0)
					REFROM(lmp).tail = olnp;

				flnp = lnp;

				DBG_CALL(Dbg_file_bind_entry(lmp, lmp));
			}
			if (flnp)
				free(flnp);
			continue;
		}

		/*
		 * Delete this object.
		 */
		DBG_CALL(Dbg_file_delete(NAME(lmp)));


		/*
		 * Set the deleting flag and call any .fini section.
		 * The deleting flag insures that no .fini code can
		 * result in another object binding back to this object.
		 *
		 * Here I also check if FLG_RT_INITDONE is set or not.
		 * It needs to be checked because this remove_so()
		 * could be called as a result of dlopen() failure.
		 * In that case, I don't want the .fini to be
		 * executed.
		 */
		FLAGS(lmp) |= FLG_RT_DELETING;
		if ((FLAGS(lmp) & FLG_RT_INITDONE) &&
		    !(FLAGS(lmp) & (FLG_RT_FINIDONE | FLG_RT_ISMAIN))) {
			FLAGS(lmp) |= FLG_RT_FINIDONE;
			if ((fptr = FINI(lmp)) != 0) {
				DBG_CALL(Dbg_util_call_fini(NAME(lmp)));
				(*fptr)();
			}
		}
		/*
		 * If the shared object had AUDITING turned on
		 * call the la_objclose() entry point in the
		 * tracing library.
		 */
		if (FLAGS(lmp) & FLG_RT_AUDIT)
			audit_objclose(lmp);

		/*
		 * Unlink the link map from chain and unmap the object.
		 */
		if (lml->lm_head == lmp)
			lml->lm_head = (Rt_map *)NEXT(lmp);
		else
			NEXT((Rt_map *)PREV(lmp)) = (void *)nlmp;

		if (lml->lm_tail == lmp)
			lml->lm_tail = (Rt_map *)PREV(lmp);
		else
			PREV(nlmp) = PREV(lmp);
		if (FLAGS(lmp) & FLG_RT_INTRPOSE)
			(void) list_delete(&(lml->lm_interpose), lmp);

		LM_UNMAP_SO(lmp)(lmp);

		/*
		 * Traverse the objects reference-from list and indicate
		 * that any permits are being given up.
		 */
		olnp = 0;
		for (LIST_TRAVERSE(&REFROM(lmp), lnp, dlp)) {
			if (--dlp->dl_permcnt == 0)
				dlp_free(dlp);
			if (olnp)
				free(olnp);
			olnp = lnp;
		}
		if (olnp)
			free(olnp);

		/*
		 * Traverse the objects dependency list decrementing the
		 * reference count of each object.
		 *
		 * And the object has to be in PARENTS list of its
		 * dependency. Remove the object from the dependency.
		 */
		DBG_CALL(Dbg_file_bind_title(REF_DELETE));
		olnp = 0;
		for (LIST_TRAVERSE(&DEPENDS(lmp), lnp, tlmp)) {
			Listnode *oolnp;
			if (!(MODE(tlmp) & RTLD_NODELETE)) {
				if (--COUNT(tlmp) == 0) {
					rtld_flags |= RT_FL_DELNEEDED;
					LIST(tlmp)->lm_flags |= LML_FLG_DELNEED;
				}
				DBG_CALL(Dbg_file_bind_entry(lmp,
				    tlmp));
			}
			/*
			 * Now, Remove me from its PARENTS list.
			 */
			oolnp = 0;
			for (LIST_TRAVERSE(&PARENTS(tlmp),
			    lnp2, tlmp2)) {
				if (tlmp2 == lmp) {
				    if (lnp2 == PARENTS(tlmp).head) {
					PARENTS(tlmp).head = lnp2->next;
				    }
				    if (lnp2 == PARENTS(tlmp).tail) {
					PARENTS(tlmp).tail = oolnp;
				    }
				    if (oolnp)
					oolnp->next = lnp2->next;
				    free(lnp2);
				    break;
				}
				oolnp = lnp2;
			}
			if (olnp)
				free(olnp);
			olnp = lnp;
		}
		if (olnp)
			free(olnp);

		/*
		 * Traverse the objects parent list and free them.
		 */
		olnp = 0;
		for (LIST_TRAVERSE(&PARENTS(lmp), lnp, tlmp)) {
			if (olnp)
				free(olnp);
			olnp = lnp;
		}
		if (olnp)
			free(olnp);

		/*
		 * Now we can free up the Rt_map itself and all of it's
		 * structures.
		 */
		for (LIST_TRAVERSE(&ALIAS(lmp), lnp, oname))
			free(oname);

		/*
		 * If this link-map was acting as a filter dlclose the
		 * filtees.  Note that this may reset the
		 * RT_FL_DELNEEDED and thus repeat the deletions loop.
		 */
		for (opnp = 0, pnp = FILTEES(lmp); pnp; opnp = pnp,
		    pnp = pnp->p_next) {
			if (pnp->p_len) {
				tlmp = (Rt_map *)pnp->p_info;
				(void) dlclose_core(tlmp, DLP(tlmp));
			}
			if (DYNINFO(lmp))
				free(DYNINFO(lmp));
			if (opnp)
				free((void *)opnp);
		}
		if (opnp)
			free((void *)opnp);

		/*
		 * Deallocate any remaining cruft and free the link-map.
		 */
		for (opnp = 0, pnp = RLIST(lmp); pnp; opnp = pnp,
		    pnp = pnp->p_next) {
			if (pnp->p_len)
				free((void *)pnp->p_name);
			if (opnp)
				free((void *)opnp);
		}
		if (opnp)
			free((void *)opnp);

		if (NAME(lmp) && (!(FLAGS(lmp) & FLG_RT_CACHED)))
			free(NAME(lmp));
		if (REFNAME(lmp))
			free(REFNAME(lmp));
		if (ELFPRV(lmp))
			free(ELFPRV(lmp));
		if (AUDIT(lmp)) {
			if (AUDIT(lmp)->af_dynplts)
				free(AUDIT(lmp)->af_dynplts);
			free(AUDIT(lmp));
		}
		(lmp->rt_list->lm_numobj)--;
		if (FLAGS(lmp) & FLG_RT_INITFRST)
			(lmp->rt_list->lm_numinitfirst)--;
		if (FLAGS(lmp) & FLG_RT_FINIFRST)
			(lmp->rt_list->lm_numfinifirst)--;
		free(lmp);
	}
}


/*
 * Search through the entire list of link map lists looking for objects
 * that can be freed from the process' address space.  Deletions of objects
 * can cause additional de-references, and thus we rescan the link-map when
 * RT_FL_DELNEEDED is in effect.  RT_FL_DELINPROG prevents recursion.
 */
void
remove_so(void)
{
	Listnode *	lnp;
	Lm_list *	lml;
	int		bindingstate = 0;

	/*
	 * Alert debuggers that link_map list is shrinking.
	 */
	if ((rtld_flags & RT_FL_DBNOTIF) == 0) {
		rtld_flags |= RT_FL_DBNOTIF;
		bindingstate++;
		rd_event(RD_DLACTIVITY, RT_DELETE, rtld_db_dlactivity());
	}

	lnp = dynlm_list.head;
	while (rtld_flags & RT_FL_DELNEEDED) {
		rtld_flags &= ~RT_FL_DELNEEDED;
		rtld_flags |= RT_FL_DELINPROG;
		while (lnp) {
			lml = lnp->data;
			if (lml->lm_flags & LML_FLG_DELNEED) {
				remove_so_core(lml);
				lml->lm_flags &= ~LML_FLG_DELNEED;
			}
			lnp = lnp->next;
			/*
			 * If there is nothing left on this link-map list
			 * delete it from the dynamic link-map list.
			 */
			if (lml->lm_head == NULL) {
				(void) list_delete(&dynlm_list, lml);
				free(lml);
			}
		}
	}

	rtld_flags &= ~RT_FL_DELINPROG;

	/*
	 * Alert debuggers that link_map is consistent again.
	 */
	if (bindingstate) {
		rtld_flags &= ~RT_FL_DBNOTIF;
		rd_event(RD_DLACTIVITY, RT_CONSISTENT, rtld_db_dlactivity());
	}
}

/*
 * Analyze a link map.  This routine is called at startup to continue the
 * processing of the main executable, or from a dlopen() to continue the
 * processing a newly opened shared object.
 *
 * In each instance we traverse the link-map list starting with the new objects
 * link-map, as dependencies are analyzed they are added to the link-map list.
 * Thus the list grows as we traverse it - this results in the breadth first
 * ordering of all needed objects.
 */
int
analyze_so(Lm_list * lml, Rt_map * clmp)
{
	Rt_map *	lmp;

	PRF_MCOUNT(51, analyze_so);

	for (lmp = clmp; lmp; lmp = (Rt_map *)NEXT(lmp)) {
		if ((FLAGS(lmp) & FLG_RT_ANALYZED))
			continue;

		/*
		 * The initial mode of a new object is established at setup() or
		 * as part of the dlopen(), and may be augmented by the objects
		 * DT entries.  This mode is propagated to each dependency via
		 * needed processing.
		 * If a DT entry indicated that a group was to be established
		 * for this object remove any RTLD_WORLD mode so that this mode
		 * will not be propagated to any of the groups dependencies.
		 */
		if (FLAGS(lmp) & FLG_RT_SETGROUP) {
			MODE(lmp) |= RTLD_GROUP;
			MODE(lmp) &= ~RTLD_WORLD;
		}

		DBG_CALL(Dbg_file_analyze(NAME(lmp), MODE(lmp)));
		DBG_CALL(Dbg_file_bind_entry(lmp, lmp));

		/*
		 * If this link map represents a relocatable object,
		 * then we need to finish up the link-editing of the
		 * object at this point.
		 */
		if (FLAGS(lmp) & FLG_RT_OBJECT) {
			if (!(elf_obj_fini(lml, lmp)))
				if (!tracing)
					return (0);
		}
		if (!(LM_LD_NEEDED(lmp)(lml, lmp)))
			if (!tracing)
				return (0);
	}

	/*
	 * If an analyzed object requires RTLD_GROUP symbol lookup symantics
	 * rescan the analyzed list now that all dependencies have been
	 * established and create a dlp for the group object.  The dlp causes
	 * the appropriate permit generation to identify the group.
	 */

	if (rtld_flags & RT_FL_SETGROUP) {
		rtld_flags &= ~RT_FL_SETGROUP;

		for (lmp = clmp; lmp; lmp = (Rt_map *)NEXT(lmp)) {
			if (!(FLAGS(lmp) & FLG_RT_SETGROUP))
				continue;
			FLAGS(lmp) &= ~FLG_RT_SETGROUP;

			if (dlp_create(lmp, lmp, MODE(lmp), 0) == 0)
				return (0);
		}
	}
	return (1);
}

/*
 * Relocate one or more objects that have just been mapped.
 */
int
relocate_so(Rt_map * clmp)
{
	Rt_map *	lmp;
	int		error;

	PRF_MCOUNT(52, relocate_so);

	for (lmp = clmp; lmp; lmp = (Rt_map *)NEXT(lmp)) {
		if (FLAGS(lmp) & FLG_RT_ANALYZED)
			continue;

		if (!tracing || (rtld_flags & RT_FL_WARN)) {
			FLAGS(lmp) |= FLG_RT_RELOCING;
			error = LM_RELOC(lmp)(lmp, 0);
			FLAGS(lmp) &= ~FLG_RT_RELOCING;

			if (!error)
				if (!tracing)
					return (0);
		}

		/*
		 * Indicate that the objects analysis is complete.
		 */
		FLAGS(lmp) |= FLG_RT_ANALYZED;

		/*
		 * If this object is a filter with the load filter flag in
		 * effect, or we're tracing, trigger the loading of all its
		 * filtees.
		 */
		if (REFNAME(lmp) && ((FLAGS(lmp) & FLG_RT_LOADFLTR) ||
		    (tracing && (rtld_flags & RT_FL_LOADFLTR))))
			(void) SYMINTP(lmp)(0, lmp, 0, 0, 0);

	}
	return (1);
}

/*
 * Determine the object type of a file.
 */
Fct *
are_u_this(const char * path)
{
	int	i;
	char *	maddr;

	PRF_MCOUNT(53, are_u_this);

	/*
	 * Map in the first page of the file.  Determine the memory size based
	 * on the larger of the filesize (obtained in load_so()) or the mapping
	 * size.  The mapping allows for execution as filter libraries may be
	 * able to use this initial mapping and require nothing else.
	 */
	if ((maddr = (char *)mmap(fmap->fm_maddr, fmap->fm_msize,
	    (PROT_READ | PROT_EXEC), fmap->fm_mflags, fmap->fm_fd, 0)) ==
	    (char *)-1) {
		int	err = errno;

		eprintf(ERR_FATAL, MSG_INTL(MSG_SYS_MMAP), path,
		    strerror(err));
		return (0);
	}

	/*
	 * From now on we will re-use fmap->fm_maddr as the mapping address
	 * so we augment the flags with MAP_FIXED.
	 */
	fmap->fm_maddr = maddr;
	fmap->fm_mflags |= MAP_FIXED;
	rtld_flags |= RT_FL_CLEANUP;

	/*
	 * Search through the object vectors to determine what kind of
	 * object we have.
	 */
	for (i = 0; vector[i]; i++) {
		if ((vector[i]->fct_are_u_this)())
			return (vector[i]);
	}

	/*
	 * Unknown file type - return error.
	 */
	eprintf(ERR_FATAL, MSG_INTL(MSG_GEN_UNKNFILE), path);
	return (0);

}


/*
 * Function that determines whether a file name has already been loaded; if so,
 * returns a pointer to its link map structure; else returns a NULL pointer.
 */
Rt_map *
is_so_loaded(Lm_list * lml, const char * name)
{
	Rt_map *	lmp;

	PRF_MCOUNT(54, is_so_loaded);

	for (lmp = lml->lm_head; lmp; lmp = (Rt_map *)NEXT(lmp)) {
		Listnode *	lnp;
		const char *	cp;

		if (FLAGS(lmp) & (FLG_RT_OBJECT | FLG_RT_DELETING))
			continue;

		for (LIST_TRAVERSE(&ALIAS(lmp), lnp, cp)) {
			if (strcmp(name, cp) == 0)
				return (lmp);
		}

		/*
		 * Finally compare full paths, this is sometimes useful for
		 * catching filter names.
		 */
		if (strcmp(name, NAME(lmp)) == 0)
			return (lmp);
	}
	return ((Rt_map *)0);
}

/*
 * Tracing is enabled by the LD_TRACE_LOADED_OPTIONS environment variable which
 * is normally set from ldd(1).  For each link map we load, print the load name
 * and the full pathname of the shared object.  Loaded objects are skipped until
 * tracing is 1 (ldd(1) uses higher values to skip preloaded shared libraries).
 */
static void
trace_so(int found, const char * name, const char * path, int cache)
{
	PRF_MCOUNT(85, trace_so);

	if (tracing != 1) {
		tracing--;
		return;
	}

	if (found == 0)
		(void) printf(MSG_INTL(MSG_LDD_FIL_NFOUND), name);
	else {
		const char *	str;

		if (cache)
			str = MSG_INTL(MSG_LDD_FIL_CACHE);
		else
			str = MSG_ORIG(MSG_STR_EMPTY);

		/*
		 * If the load name isn't a full pathname print its associated
		 * pathname.
		 */
		if (*name == '/')
			(void) printf(MSG_ORIG(MSG_LDD_FIL_PATH), name, str);
		else
			(void) printf(MSG_ORIG(MSG_LDD_FIL_EQUIV), name, path,
			    str);
	}
}


/*
 * Open a file.  If a cache is being used determine if a cached object is
 * available.  Note that we only check the cache for dependencies of the
 * application, not for ld.so.1's use (ld.so.1 can map shared objects for
 * its own use, for example libelf, libc, etc. - as the user will be using the
 * cached objects if they exist, ld.so.1 must get its own objects).
 */
static int
so_open(Lm_list * lml, const char * file, const char ** _file)
{
	int	fd;

	DEF_TIME(interval1);
	DEF_TIME(interval2);

	PRF_MCOUNT(25, so_open);

	GET_TIME(interval1);
	SAV_TIME(interval1, "      so_open");

	if ((rtld_flags & RT_FL_CACHEAVL) && (lml == &lml_main)) {
		Listnode *	lnp;
		Rtc_obj *	obj;

		for (LIST_TRAVERSE((List *)cachehead->rtc_objects, lnp, obj)) {
			if (strcmp(file, obj->rtc_name) != 0)
				continue;

			/*
			 * If the cached object can't be used for some reason
			 * simply continue to open the original referenced file.
			 */
			GET_TIME(interval1);
			if ((fd = open(obj->rtc_cache, O_RDONLY, 0)) != -1) {
				DBG_CALL(Dbg_file_cache_obj(obj->rtc_name,
				    obj->rtc_cache));
				*_file = (const char *)obj->rtc_cache;
				GET_TIME(interval2);
				SAV_TIME(interval1, "      open cache file");
				SAV_TIME(interval2, "      opened");
				return (fd);
			}
			break;
		}
	}

	*_file = file;
	GET_TIME(interval1);
	fd = open(file, O_RDONLY);
	GET_TIME(interval2);
	SAV_TIME(interval1, "      open regular file");
	SAV_TIME(interval2, "      opened");
	return (fd);
}

/*
 * This function loads the name files and returns a pointer to its link map.
 * It is assumed that the caller has already checked that the file is not
 * already loaded before calling this function (refer is_so_loaded()).
 * Find and open the file, map it into memory, add it to the end of the list
 * of link maps and return a pointer to the new link map.  Return 0 on error.
 */
Rt_map *
load_so(Lm_list * lml, const char * name, Rt_map * lmp)
{
	Fct *		ftp;
	struct stat	status;
	const char *	_str, * str = name;
	char *		__str;
	int		slash = 0, fd, cflag = 0;
	size_t		_len, len;
	Rt_map *	_lmp;
	int		why;
	Word		what;


	DEF_TIME(interval1);

	PRF_MCOUNT(55, load_so);

	if (name == 0) {
		eprintf(ERR_FATAL, MSG_INTL(MSG_GEN_NULLFILE));
		return (0);
	}

	GET_TIME(interval1);
	SAV_TIME(interval1, "    loadso - find file");

	/*
	 * If the file is the run time linker then it's already loaded.
	 */
	if (strcmp(name, NAME(lml_rtld.lm_head)) == 0)
		return (lml_rtld.lm_head);

	/*
	 * Determine the length of the input filename (for max path length
	 * verification) and whether the filename contains any '/'s.
	 */
	for (_str = str; *_str; _str++) {
		if (*_str == '/') {
			slash++;
		}
	}
	_len = len = (_str - str) + 1;

	/*
	 * If we are passed a 'null' link-map this means that this is the first
	 * object to be loaded on this link-map list. In that case we set the
	 * link-map to ld.so.1's link-map.
	 *
	 * This link-map is referenced to determine what lookup rules to use
	 * when searching for files.  By using ld.so.1's we are defaulting to
	 * ELF look-up rules.
	 *
	 * Note: This case happens when loading the first object onto
	 *	 the plt_tracing link-map.
	 */
	if (lmp == 0)
		lmp = lml_rtld.lm_head;

	if (slash) {
		/*
		 * Use the filename as is.  If a cache entry is found determine
		 * if it has already been loaded.
		 */
		fd = so_open(lml, str, &_str);
		if (str != _str) {
			if ((_lmp = is_so_loaded(lml, _str)) != 0)
				return (_lmp);
			cflag = 1;
		}

		if (fd > -1) {
			(void) fstat(fd, &status);
			fmap->fm_fd = fd;
			fmap->fm_fsize = status.st_size;

			if ((ftp = are_u_this(_str)) == 0) {
				(void) close(fd);
				fmap->fm_fd = 0;
			} else if ((why = ftp->fct_are_u_compat(&what)) > 0) {
				DBG_CALL(Dbg_file_rejected(_str, why, what));
				(void) close(fd);
				fmap->fm_fd = 0;
			}
		}
	} else {
		/*
		 * No '/' - for each directory on list, make a pathname using
		 * that directory and filename and try to open that file.
		 */
		Pnode *	dir, * dirlist = (Pnode *)0;

		DBG_CALL(Dbg_libs_find(str));

		for (fd = -1, dir = get_next_dir(&dirlist, lmp); dir;
		    dir = get_next_dir(&dirlist, lmp)) {
			if (dir->p_name == 0)
				continue;

			/*
			 * Protect ourselves from building an invalid pathname.
			 */
			len = _len + dir->p_len + 1;
			if (len >= PATH_MAX) {
				eprintf(ERR_FATAL, MSG_INTL(MSG_SYS_OPEN),
				    str, strerror(ENAMETOOLONG));
				continue;
			}
			if (!(__str = (LM_GET_SO(lmp)(dir->p_name, str))))
				continue;

			DBG_CALL(Dbg_libs_found(__str));
			if (rtld_flags & RT_FL_SEARCH)
				(void) printf(MSG_INTL(MSG_LDD_PTH_TRYING),
				    __str);

			if ((fd = so_open(lml, __str, &_str)) != -1) {
				if (__str != _str)
					cflag = 1;

				(void) fstat(fd, &status);
				fmap->fm_fd = fd;
				fmap->fm_fsize = status.st_size;

				if ((ftp = are_u_this(_str)) == 0) {
					(void) close(fd);
					fmap->fm_fd = 0;
					continue;
				}

				if ((why = ftp->fct_are_u_compat(&what)) > 0) {
					DBG_CALL(Dbg_file_rejected(__str, why,
					    what));
					(void) close(fd);
					fmap->fm_fd = 0;
					continue;
				}

				break;
			}
		}
	}

	GET_TIME(interval1);
	SAV_TIME(interval1, "    file lookup finished");

	/*
	 * If no file was found complete any tracing output and return.  Note
	 * that auxiliary filters do not constitute an error condition if they
	 * cannot be located.
	 */
	if (fd == -1) {
		if ((FLAGS(lmp) & (FLG_RT_AUX | FLG_RT_FLTRING)) !=
		    (FLG_RT_AUX | FLG_RT_FLTRING)) {
			if (tracing)
				trace_so(0, name, 0, cflag);
			else
				eprintf(ERR_FATAL, MSG_INTL(MSG_SYS_OPEN),
				    name, strerror(errno));
		}
		return (0);
	}

	/*
	 * If the file has been found determine from the new files status
	 * information if this file is actually linked to one we already have
	 * mapped.
	 */
	for (_lmp = lml->lm_head; _lmp; _lmp = (Rt_map *)NEXT(_lmp)) {
		if ((_lmp->rt_stdev != status.st_dev) ||
		    (_lmp->rt_stino != status.st_ino))
			continue;

		DBG_CALL(Dbg_file_skip(_str, NAME(_lmp)));
		(void) close(fd);
		if (list_append(&ALIAS(_lmp), strdup(name)) == 0)
			return (0);
		return (_lmp);
	}

	if (tracing)
		trace_so(1, name, _str, cflag);

	/*
	 * If cache diagnostics are being gathered write the name of each file
	 * we've opened, indicating whether its a cached version or not.
	 */
	if (cd_file) {
		int	cd_fd;

		if ((cd_fd = cd_open()) != FD_UNAVAIL) {
			char	diag[PATH_MAX + 6];

			(void) strcpy(diag, _str);
			if (cflag)
				(void) strcat(diag, MSG_ORIG(MSG_STR_CFLAG));
			(void) strcat(diag, MSG_ORIG(MSG_STR_NL));

			(void) write(cd_fd, diag, strlen(diag));
		}
	}

	/*
	 * Find out what type of object we have and map it in.  If the link-map
	 * is created save the dev/inode information for later comparisons.
	 */
	if (!cflag) {
		if ((__str = (char *)malloc(len)) == 0)
			return (0);
		(void) strcpy(__str, _str);
		_str = __str;
	}

	if (ftp && ((_lmp = (ftp->fct_map_so)(lml, _str, name)) != 0)) {
		_lmp->rt_stdev = status.st_dev;
		_lmp->rt_stino = status.st_ino;

		if (cflag)
			FLAGS(_lmp) |= FLG_RT_CACHED;

		/*
		 * Close the original file so as not to accumulate file
		 * descriptors.
		 */
		(void) close(fmap->fm_fd);
		fmap->fm_fd = 0;
	}

	GET_TIME(interval1);
	SAV_TIME(interval1, "    file mapped");

	/*
	 * Record the PARENT link-map at this stage - if it's important.
	 */
	if (_lmp && SYMINFO(_lmp))
		PARENT(_lmp) = lmp;

	return (_lmp);
}

/*
 * Symbol lookup routine.  Takes an ELF symbol name, and a list of link maps to
 * search (if the flag indicates LKUP_FIRST only the first link map of the list
 * is searched ie. we've been called from dlsym()).
 * If successful, return a pointer to the symbol table entry and a pointer to
 * the link map of the enclosing object.  Else return a null pointer.
 *
 * To improve elf performance, we first compute the elf hash value and pass
 * it to each find_sym() routine.  The elf function will use this value to
 * locate the symbol, the a.out function will simply ignore it.
 */
Sym *
lookup_sym(Slookup * slp, Rt_map ** dlmp, int flag)
{
	const char *	name = slp->sl_name;
	Permit *	permit = slp->sl_permit;
	Rt_map *	clmp = slp->sl_cmap;
	Rt_map *	ilmp = slp->sl_imap;
	unsigned long	rsymndx = slp->sl_rsymndx;
	unsigned long	hash;
	Sym *		sym = 0;
	Rt_map *	lmp;
	Syminfo *	sip;

	PRF_MCOUNT(56, lookup_sym);


	if (clmp && (sip = SYMINFO(clmp)) && rsymndx) {
		/*
		 * find coresponding Syminfo entry for the original
		 * referencing symbol.
		 */
		sip = (Syminfo *)((unsigned long)sip +
			rsymndx * SYMINENT(clmp));

		if (sip->si_flags & SYMINFO_FLG_DIRECT &&
		    elf_lazyload(sip->si_boundto, slp, dlmp,
		    &sym, flag) == 0) {
			/*
			 * Symbol was found durring elf_lazyload
			 * processing and can be returned now.
			 */
			if (sym)
				return (sym);

			if (!tracing)
				return (0);
		}
	}

	/*
	 * Search the initial link map for the required symbol (this category is
	 * selected by dlsym(), where individual link maps are searched for a
	 * required symbol.  Therefore, we know we have permission to look at
	 * the link map).
	 */
	if (flag & LKUP_FIRST)
		return (SYMINTP(ilmp)(name, ilmp, dlmp, flag, elf_hash(name)));

	/*
	 * Check if any interposing symbols satisfy this reference.
	 */
	hash = elf_hash(name);
	if ((flag & LKUP_NOINT) == 0) {
		Listnode *	lnp;
		for (LIST_TRAVERSE(&(LIST(clmp)->lm_interpose), lnp, lmp)) {
			if (sym = SYMINTP(lmp)(name, lmp, dlmp, flag, hash)) {
				return (sym);
			}
		}
	}

	/*
	 * COPY relocs should start there search just after the head
	 * of the list.  Since the list may have been null on entry
	 * but may now have additional objects lazily loaded we set
	 * the start point here.
	 */
	if (flag & LKUP_COPY)
		ilmp = (Rt_map *)NEXT(LIST(clmp)->lm_head);

	/*
	 * Examine the list of link maps, skipping any whose symbols are denied
	 * to this caller.
	 */
	for (lmp = ilmp; lmp; lmp = (Rt_map *)NEXT(lmp)) {
		if ((lmp == clmp) || (!(FLAGS(lmp) & FLG_RT_DELETING) &&
		    (((MODE(clmp) & RTLD_WORLD) &&
		    (MODE(lmp) & RTLD_GLOBAL)) ||
		    ((MODE(clmp) & RTLD_GROUP) &&
		    perm_test(PERMIT(lmp), permit))))) {
			if (sym = SYMINTP(lmp)(name, lmp, dlmp, flag, hash)) {
				Listnode *	lnp;
				Rel_copy *	rcp;

				if (!(MODE(clmp) & RTLD_GROUP) ||
				    !(FLAGS(lmp) & FLG_RT_COPYTOOK))
					return (sym);

				/*
				 * If this an RTLD_GROUP binding and we've bound
				 * to a copy relocation definition then we need
				 * to assign this binding to the original copy
				 * reference.
				 */
				for (LIST_TRAVERSE(&COPY(lmp), lnp, rcp)) {
					if (sym == rcp->r_dsym) {
						*dlmp = rcp->r_rlmp;
						return (rcp->r_rsym);
					}
				}
				return (sym);
			}
		}
	}
	return ((Sym *)0);
}

/*
 * Add a explicit (NEEDED) dependency to the DEPENDS list.  There is no need
 * to check whether the dependency already exists on the list, as dependencies
 * (DT_NEEDED entries) are unique.
 */
static int
add_neededs(Rt_map * clmp, Rt_map * nlmp)
{
	PRF_MCOUNT(97, add_neededs);

	/*
	 * Note: with the advent of lazyloading it could be that
	 * this NEEDED entry is comming in after other non-NEEDED
	 * entrys have been added to the depends list.  Because of
	 * this we must insert this into the list if required.
	 */
	if (LASTDEP(clmp)) {
		if (list_appafter(&DEPENDS(clmp), LASTDEP(clmp), nlmp) == 0)
			return (0);
	} else {
		/*
		 * Add the new link-map to the dependency list.
		 */
		if (list_append(&DEPENDS(clmp), nlmp) == 0)
			return (0);
	}

	/*
	 * Add clmp into the parents list of the new-link map.
	 */
	if (list_append(&PARENTS(nlmp), clmp) == 0)
		return (0);

	/*
	 * Keep track of this dependency as the last one added - the list of
	 * objects up to this dependency are acceptable for creating the
	 * Dl_obj handle for a dlopen().
	 */
	LASTDEP(clmp) = nlmp;

	/*
	 * If this is a deletable object bump its reference count.
	 */
	if (!(MODE(nlmp) & RTLD_NODELETE))
		COUNT(nlmp)++;

	return (1);
}

/*
 * During relocation processing, one or more symbol bindings may have been
 * established to non-deletable objects.  If these are not already explicit
 * dependencies, they are added to the DEPENDS list, and their reference count
 * incremented - these references insure that the bound object doesn't get
 * deleted while this object is using it.
 */
static int
add_many_bindings(Rt_map * clmp)
{
	Rt_map *	nlmp, * tlmp;

	PRF_MCOUNT(98, add_many_bindings);
	DBG_CALL(Dbg_file_bind_title(REF_SYMBOL));
	/*
	 * Traverse the entire link-map list to determine if the calling object
	 * has bound to any other objects.
	 */
	for (nlmp = LIST(clmp)->lm_head; nlmp; nlmp = (Rt_map *)NEXT(nlmp)) {
		Listnode *	lnp = 0;

		if (!(FLAGS(nlmp) & FLG_RT_BOUND))
			continue;

		FLAGS(nlmp) &= ~FLG_RT_BOUND;

		/*
		 * Determine if this object already exists on the dependency
		 * list - if so we're done.
		 */
		for (LIST_TRAVERSE(&DEPENDS(clmp), lnp, tlmp))
			if (tlmp == nlmp)
				break;
		if (lnp)
			continue;

		/*
		 * Add the new link-map to the dependency list, and as it is a
		 * deletable object bump its reference count.
		 */
		if (list_append(&DEPENDS(clmp), nlmp) == 0)
			return (0);

		/*
		 * Add clmp into the parents list of the new-link map.
		 */
		if (list_append(&PARENTS(nlmp), clmp) == 0)
			return (0);

		COUNT(nlmp)++;

		DBG_CALL(Dbg_file_bind_entry(clmp, nlmp));
	}
	return (1);
}

/*
 * During .plt relocation a binding may have been established to a non-deletable
 * object.  This is a subset of the add_many_bindings() logic.
 */
static int
add_one_binding(Rt_map * clmp, Rt_map * nlmp)
{
	Rt_map *	tlmp;
	Listnode *	lnp;

	PRF_MCOUNT(99, add_one_binding);

	/*
	 * Determine if this object already exists on the dependency list - if
	 * so we're done.
	 */
	for (LIST_TRAVERSE(&DEPENDS(clmp), lnp, tlmp)) {
		if (nlmp == tlmp)
			return (1);
	}

	/*
	 * Add the new link-map to the dependency list, and as it is a
	 * deletable object bump its reference count.
	 */
	if (list_append(&DEPENDS(clmp), nlmp) == 0)
		return (0);

	/*
	 * Add clmp into the parents list of the new-link map.
	 */
	if (list_append(&PARENTS(nlmp), clmp) == 0)
		return (0);

	COUNT(nlmp)++;

	DBG_CALL(Dbg_file_bind_title(REF_SYMBOL));
	DBG_CALL(Dbg_file_bind_entry(clmp, nlmp));

	return (1);
}

/*
 * Add a new dependency to the callers dependency list.  This dependency can
 * either be a standard (needed) dependency, or it may be an additional
 * dependency because of a symbol binding (these can only be to promiscuous
 * objects that are not defined as normal dependencies).
 *
 * Maintaining the DEPENDS list allows all referenced objects to have their
 * reference counts reduced should this object be deleted, and provides for
 * building the Dl_obj dependency list for dlopen'ed objects.
 */
int
bound_add(int mode, Rt_map * clmp, Rt_map * nlmp)
{
	int	ref, bind = 0;

	PRF_MCOUNT(57, bound_add);

	if (((Sxword)lc_version > 0) &&
	    (bind = bind_guard(THR_FLG_BOUND)))
		(void) rw_wrlock(&boundlock);

	if (mode == REF_SYMBOL) {
		if (nlmp == 0)
			ref = add_many_bindings(clmp);
		else
			ref = add_one_binding(clmp, nlmp);
	} else
		ref = add_neededs(clmp, nlmp);

	if ((mode == REF_DIRECT) && PERMIT(clmp)) {
		PERMIT(nlmp) = perm_set(PERMIT(nlmp), PERMIT(clmp));
		DBG_CALL(Dbg_file_bind_entry(clmp, nlmp));
	}

	if (bind) {
		(void) rw_unlock(&boundlock);
		(void) bind_clear(THR_FLG_BOUND);
	}
	return (ref);
}
