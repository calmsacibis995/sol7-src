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
#pragma	ident	"@(#)util.c	1.84	98/01/30 SMI"

/*
 * Utility routines for run-time linker.  some are duplicated here from libc
 * (with different names) to avoid name space collisions.
 */
#include	"_synonyms.h"

#include	<sys/types.h>
#include	<sys/mman.h>
#include	<sys/stat.h>
#include	<stdarg.h>
#include	<fcntl.h>
#include	<string.h>
#include	<ctype.h>
#include	<dlfcn.h>
#include	<unistd.h>
#include	<signal.h>
#include	<locale.h>
#include	<libintl.h>
#include	"_rtld.h"
#include	"_audit.h"
#include	"msg.h"
#include	"debug.h"
#include	"profile.h"

/*
 * All error messages go through eprintf().  During process initialization these
 * messages should be directed to the standard error, however once control has
 * been passed to the applications code these messages should be stored in an
 * internal buffer for use with dlerror().  Note, fatal error conditions that
 * may occur while running the application will still cause a standard error
 * message, see exit() in this file for details.
 * The `application' flag serves to indicate the transition between process
 * initialization and when the applications code is running.
 */

/*
 * Null function used as place where a debugger can set a breakpoint.
 */
void
rtld_db_dlactivity(void)
{
}

/*
 * Null function used as place where debugger can set a pre .init
 * processing breakpoint.
 */
void
rtld_db_preinit(void)
{
}


/*
 * Null function used as place where debugger can set a post .init
 * processing breakpoint.
 */
void
rtld_db_postinit(void)
{
}

/*
 * Execute any .init sections.  These are passed to us in an lmp array which
 * (by default) will have been sorted.
 */
void
call_init(Rt_map ** lmp)
{
	void (*		iptr)();
	Rt_map **	_lmp;
	int 		bind;

	PRF_MCOUNT(60, call_init);

	if (lmp == NULL)
		return;

	for (_lmp = lmp; *_lmp != NULL; _lmp++) {
		if (*_lmp == (Rt_map *)RT_SORT_UNUSED)
			continue;

		/*
		 * Skip any .init defined in the main executable, as this will
		 * be called from crt0.  Skip any objects that are still being
		 * relocated (we can get in this state by a filter's relocation
		 * processing triggering the loading of a filtee - we might be
		 * here to .init the filtee, but as the filter's relocation
		 * isn't complete we can't call its .init). Also skip any .init
		 * that has already been called.
		 */
		if ((bind = bind_guard(THR_FLG_INIT)) == 1)
			(void) rw_wrlock(&initlock);
		if (!(FLAGS(*_lmp) &
		    (FLG_RT_ISMAIN | FLG_RT_RELOCING | FLG_RT_INITDONE))) {
			/*
			 * Establish an initfirst state if necessary - no other
			 * inits will be fired (because of addition relocation
			 * bindings) when in this state.
			 */
			if (FLAGS(*_lmp) & FLG_RT_INITFRST)
				rtld_flags |= RT_FL_INITFIRST;

			/*
			 * Set the initdone flag regardless of whether this
			 * object actually contains an .init section.  This flag
			 * prevents us from processing this section again for an
			 * .init and also signifies that a .fini must be called
			 * should it exist.
			 */
			FLAGS(*_lmp) |= FLG_RT_INITDONE;
			if (bind) {
				(void) rw_unlock(&initlock);
				(void) bind_clear(THR_FLG_INIT);
			}

			if ((iptr = INIT(*_lmp)) != 0) {
				DBG_CALL(Dbg_util_call_init(NAME(*_lmp)));
				(*iptr)();
			}

			rtld_flags &= ~RT_FL_INITFIRST;
		} else {
			if (bind) {
				(void) rw_unlock(&initlock);
				(void) bind_clear(THR_FLG_INIT);
			}
		}
	}
	free(lmp);
}

/*
 * Function called by atexit(3C).  Calls all .fini sections related with the
 * mains dependent shared libraries in the order in which the shared libraries
 * have been loaded.  Skip any .fini defined in the main executable, as this
 * will be called by crt0 (main was never marked as initdone).
 */
void
call_fini()
{
	Rt_map **	lmp, **_lmp;
	Lm_list *	lml;
	Listnode *	lnp;
	void (*		fptr)();

	PRF_MCOUNT(61, call_fini);

	for (LIST_TRAVERSE(&dynlm_list, lnp, lml)) {
		/*
		 * reverse topological sort the dependency for .fini execution.
		 */
		if ((lmp = tsort(lml->lm_head, RT_SORT_FWD)) ==
		    (Rt_map **)S_ERROR)
			continue;

		if (lmp == (Rt_map **)NULL)
			continue;

		for (_lmp = lmp;  *_lmp != NULL; _lmp++) {
			if (*_lmp == (Rt_map *)RT_SORT_UNUSED)
				continue;
			if (!(FLAGS(*_lmp) &
			    (FLG_RT_FINIDONE | FLG_RT_ISMAIN))) {
				FLAGS(*_lmp) |= FLG_RT_FINIDONE;

				if ((fptr = FINI(*_lmp)) != 0) {
					DBG_CALL(
					Dbg_util_call_fini(NAME(*_lmp)));
					(*fptr)();
					/*
					 * If the shared object had AUDITING
					 * turned on call the la_objclose()
					 * entry point in the
					 * traceing library.
					 */
					if (FLAGS(*_lmp) & FLG_RT_AUDIT)
						audit_objclose(*_lmp);
				}
			}
		}
	}
}


/*
 * Append an item to the specified list, and return a pointer to the list
 * node created.
 */
Listnode *
list_append(List * lst, const void * item)
{
	Listnode *	_lnp;

	PRF_MCOUNT(62, list_append);

	if ((_lnp = (Listnode *)malloc(sizeof (Listnode))) == 0)
		return (0);

	_lnp->data = (void *) item;
	_lnp->next = NULL;

	if (lst->head == NULL)
		lst->tail = lst->head = _lnp;
	else {
		lst->tail->next = _lnp;
		lst->tail = lst->tail->next;
	}
	return (_lnp);
}


/*
 * Add and item to 'lst' immediatly after node  identiefied
 * by 'void * d'
 */
Listnode *
list_appafter(List * lst, void * d, void * item)
{
	Listnode *	lnp;
	Listnode *	_lnp;

	for (lnp = lst->head; lnp; lnp = lnp->next)
		if (lnp->data == d)
			break;
	if (!lnp)
		return (0);

	if ((_lnp = (Listnode *) malloc(sizeof (Listnode))) == 0)
		return (0);
	_lnp->data = (void *)item;
	_lnp->next = lnp->next;
	lnp->next = _lnp;
	if (lst->tail == lnp)
		lst->tail = _lnp;
	return (_lnp);
}


/*
 * Delete a 'listnode' from a list.  If the list not does not
 * reside on this list return (-1) else return (0) on success.
 */
int
list_delete(List * lst, void * item)
{
	Listnode *	clnp;
	Listnode *	plnp;

	for (plnp = NULL, clnp = lst->head; clnp; clnp = clnp->next) {
		if (item == clnp->data)
			break;
		plnp = clnp;
	}

	if (clnp == 0)
		return (0);

	if (lst->head == clnp)
		lst->head = clnp->next;
	if (lst->tail == clnp)
		lst->tail = plnp;

	if (plnp)
		plnp->next = clnp->next;

	free(clnp);

	return (1);
}

/*
 * Append an item to the specified link map list.
 */
int
lm_append(Lm_list * lml, Rt_map * lmp)
{
	PRF_MCOUNT(63, lm_append);

	if (lml->lm_head == NULL) {
		lml->lm_head = lmp;
		FLAGS(lmp) |= FLG_RT_FINIFRST;
		PREV(lmp) = NULL;
	} else {
		NEXT(lml->lm_tail) = (Link_map *)lmp;
		PREV(lmp) = (Link_map *)lml->lm_tail;
	}
	(lml->lm_numobj)++;
	if (FLAGS(lmp) & FLG_RT_FINIFRST)
		(lml->lm_numfinifirst)++;
	if (FLAGS(lmp) & FLG_RT_INITFRST)
		(lml->lm_numinitfirst)++;
	lml->lm_tail = lmp;
	if (FLAGS(lmp) & FLG_RT_INTRPOSE)
		if (list_append(&(lml->lm_interpose), lmp) == 0)
			return (0);
	return (1);
}

static int
listenv(const char * str, size_t len, List * lst)
{
	char *	_str;

	if ((_str = malloc(++len)) == 0)
		return (0);

	(void) strcpy(_str, str);
	str = _str;
	while (*_str != '\0') {
		if (isspace(*_str)) {
			*_str++ = '\0';
			if (!(list_append(lst, str)))
				return (0);
			while (isspace(*_str))
				_str++;
			str = _str;
		} else
			_str++;
	}
	if (str != _str)
		if (!(list_append(lst, str)))
			return (0);
	return (1);
}

/*
 * Internal getenv routine.  Only strings starting with `LD_' are reserved for
 * our use.  By convention, all strings should be of the form `LD_XXXXX=', if
 * the string is followed by a non-null value the appropriate functionality is
 * enabled.
 */
#define	LOC_LANG	1
#define	LOC_MESG	2
#define	LOC_ALL		3

int
readenv(const char ** envp, int aout)
{
	const char *	s1, * s2;
	const char *	flags_str = 0;
	size_t		i, loc = 0;
	int		ldpth_override = FALSE;
	int		ldprof = FALSE;
	int		audit_64 = FALSE;

	if (envp == (const char **)0)
		return (1);
	while (*envp != (const char *)0) {
		s1 = *envp++;
		if (*s1++ != 'L')
			continue;

		/*
		 * See if we have any locale environment settings.  The environ
		 * variables have a precedence, LC_ALL is higher than
		 * LC_MESSAGES which is higher than LANG.
		 */
		s2 = s1;
		if ((*s2++ == 'C') && (*s2++ == '_') && (*s2 != '\0')) {
			if (strncmp(s2, MSG_ORIG(MSG_LC_ALL),
			    MSG_LC_ALL_SIZE) == 0) {
				s2 += MSG_LC_ALL_SIZE;
				if ((*s2 != '\0') && (loc < LOC_ALL)) {
					locale = s2;
					loc = LOC_ALL;
				}
			} else if (strncmp(s2, MSG_ORIG(MSG_LC_MESSAGES),
			    MSG_LC_MESSAGES_SIZE) == 0) {
				s2 += MSG_LC_MESSAGES_SIZE;
				if ((*s2 != '\0') && (loc < LOC_MESG)) {
					locale = s2;
					loc = LOC_MESG;
				}
			}
			continue;
		}
		s2 = s1;
		if ((*s2++ == 'A') && (*s2++ == 'N') && (*s2++ == 'G') &&
		    (*s2++ == '=') && (*s2 != '\0') && (loc < LOC_LANG)) {
			locale = s2;
			loc = LOC_LANG;
			continue;
		}

		/*
		 * Pick off any LD_ environment variables.
		 */
		if ((*s1++ != 'D') || (*s1++ != '_') || (*s1 == '\0'))
			continue;

#ifdef _ELF64
		if (strncmp(s1, MSG_ORIG(MSG_LD_LIBPATH_64),
		    MSG_LD_LIBPATH_64_SIZE) == 0) {
			s1 += MSG_LD_LIBPATH_64_SIZE;
			if (*s1 != '\0')
				envdirs = s1;
			ldpth_override = TRUE;
		} else
#endif
		if (strncmp(s1, MSG_ORIG(MSG_LD_LIBPATH),
		    MSG_LD_LIBPATH_SIZE) == 0) {
			/*
			 * V9 ABI:
			 * LD_LIBRARY_PATH_64 overrides LD_LIBRARY_PATH.
			 */
			if (ldpth_override == FALSE) {
				s1 += MSG_LD_LIBPATH_SIZE;
				if (*s1 != '\0')
					envdirs = s1;
			}
		} else if (strncmp(s1, MSG_ORIG(MSG_LD_PRELOAD),
		    MSG_LD_PRELOAD_SIZE) == 0) {
			s1 += MSG_LD_PRELOAD_SIZE;
			while (isspace(*s1))
				s1++;
			if ((i = strlen(s1)) == 0)
				continue;
			if (!(listenv(s1, i, &preload)))
				return (0);
		} else if (strncmp(s1, MSG_ORIG(MSG_LD_TRACEOBJS),
		    MSG_LD_TRACEOBJS_SIZE) == 0) {
			s1 += MSG_LD_TRACEOBJS_SIZE;
			if ((*s1 == '=') && (*++s1 != '\0')) {
				tracing = *s1 - '0';
			} else if (((strncmp(s1, MSG_ORIG(MSG_LD_TRACE_E),
			    MSG_LD_TRACE_E_SIZE) == 0) && !aout) ||
			    ((strncmp(s1, MSG_ORIG(MSG_LD_TRACE_A),
			    MSG_LD_TRACE_A_SIZE) == 0) && aout)) {
				s1 += MSG_LD_TRACE_E_SIZE;
				if (*s1 != '\0')
					tracing = *s1 - '0';
			}
		} else if (strncmp(s1, MSG_ORIG(MSG_LD_TRACEPTHS),
		    MSG_LD_TRACEPTHS_SIZE) == 0) {
			s1 += MSG_LD_TRACEPTHS_SIZE;
			if (*s1 != '\0')
				rtld_flags |= RT_FL_SEARCH;
		} else if (strncmp(s1, MSG_ORIG(MSG_LD_VERBOSE),
		    MSG_LD_VERBOSE_SIZE) == 0) {
			s1 += MSG_LD_VERBOSE_SIZE;
			if (*s1 != '\0')
				rtld_flags |= RT_FL_VERBOSE;
		} else if (strncmp(s1, MSG_ORIG(MSG_LD_BREADTH),
		    MSG_LD_BREADTH_SIZE) == 0) {
			s1 += MSG_LD_BREADTH_SIZE;
			if (*s1 != '\0')
				rtld_flags |= RT_FL_BREADTH;
		} else if (strncmp(s1, MSG_ORIG(MSG_LD_INIT),
		    MSG_LD_INIT_SIZE) == 0) {
			s1 += MSG_LD_INIT_SIZE;
			if (*s1 != '\0')
				rtld_flags |= RT_FL_INIT;
		} else if (strncmp(s1, MSG_ORIG(MSG_LD_NODIRECT),
		    MSG_LD_NODIRECT_SIZE) == 0) {
			s1 += MSG_LD_NODIRECT_SIZE;
			if (*s1 != '\0')
				rtld_flags |= RT_FL_NODIRECT;
		} else if (strncmp(s1, MSG_ORIG(MSG_LD_WARN),
		    MSG_LD_WARN_SIZE) == 0) {
			s1 += MSG_LD_WARN_SIZE;
			if (*s1 != '\0')
				rtld_flags |= RT_FL_WARN;
		} else if (strncmp(s1, MSG_ORIG(MSG_LD_BINDINGS),
		    MSG_LD_BINDINGS_SIZE) == 0) {
			s1 += MSG_LD_BINDINGS_SIZE;
			if (*s1 != '\0') {
				/*
				 * NOTE, this variable is simply for backward
				 * compatibility.  If this and LD_DEBUG are both
				 * specified, only one of the strings is going
				 * to get processed.
				 */
				dbg_str = MSG_ORIG(MSG_TKN_BINDINGS);
			}
		} else if (strncmp(s1, MSG_ORIG(MSG_LD_BIND_NOW),
		    MSG_LD_BIND_NOW_SIZE) == 0) {
			s1 += MSG_LD_BIND_NOW_SIZE;
			if (*s1 != '\0')
				bind_mode = RTLD_NOW;
		} else if (strncmp(s1, MSG_ORIG(MSG_LD_BIND_NOT),
		    MSG_LD_BIND_NOT_SIZE) == 0) {
			s1 += MSG_LD_BIND_NOT_SIZE;
			if (*s1 != '\0')
				rtld_flags |= RT_FL_NOBIND;
		} else if (strncmp(s1, MSG_ORIG(MSG_LD_NOAUXFLTR),
		    MSG_LD_NOAUXFLTR_SIZE) == 0) {
			s1 += MSG_LD_NOAUXFLTR_SIZE;
			if (*s1 != '\0')
				rtld_flags |= RT_FL_NOAUXFLTR;
		} else if (strncmp(s1, MSG_ORIG(MSG_LD_LOADFLTR),
		    MSG_LD_LOADFLTR_SIZE) == 0) {
			s1 += MSG_LD_LOADFLTR_SIZE;
			if (*s1 != '\0')
				rtld_flags |= RT_FL_LOADFLTR;
		} else if (strncmp(s1, MSG_ORIG(MSG_LD_NOVERSION),
		    MSG_LD_NOVERSION_SIZE) == 0) {
			s1 += MSG_LD_NOVERSION_SIZE;
			if (*s1 != '\0')
				rtld_flags |= RT_FL_NOVERSION;
		} else if (strncmp(s1, MSG_ORIG(MSG_LD_CACHEDIR),
		    MSG_LD_CACHEDIR_SIZE) == 0) {
			s1 += MSG_LD_CACHEDIR_SIZE;
			if (*s1 != '\0')
				cd_dir = s1;
		} else if (strncmp(s1, MSG_ORIG(MSG_LD_NOCACHE),
		    MSG_LD_NOCACHE_SIZE) == 0) {
			s1 += MSG_LD_NOCACHE_SIZE;
			if (*s1 != '\0')
				rtld_flags |= RT_FL_NOCACHE;
		} else if (strncmp(s1, MSG_ORIG(MSG_LD_ORIGIN),
		    MSG_LD_ORIGIN_SIZE) == 0) {
			s1 += MSG_LD_ORIGIN_SIZE;
			if (*s1 != '\0')
				rtld_flags |= RT_FL_ORIGIN;
		} else if (strncmp(s1, MSG_ORIG(MSG_LD_AUDIT),
		    MSG_LD_AUDIT_SIZE) == 0 && !audit_64) {
			s1 += MSG_LD_AUDIT_SIZE;
			if (ldprof) {
				eprintf(ERR_WARNING,
				    MSG_INTL(MSG_AUD_PROFAUDINC));
				continue;
			}
			if (*s1 != '\0')
				audit_libraries = s1;
#ifdef _LP64
		} else if (strncmp(s1, MSG_ORIG(MSG_LD_AUDIT_64),
		    MSG_LD_AUDIT_64_SIZE) == 0) {
			s1 += MSG_LD_AUDIT_64_SIZE;
			if (ldprof) {
				eprintf(ERR_WARNING,
				    MSG_INTL(MSG_AUD_PROFAUDINC));
				continue;
			}
			audit_64 = TRUE;
			if (*s1 != '\0')
				audit_libraries = s1;
#endif
		} else if (strncmp(s1, MSG_ORIG(MSG_LD_FLAGS),
		    MSG_LD_FLAGS_SIZE) == 0) {
			s1 += MSG_LD_FLAGS_SIZE;
			flags_str = s1;
			continue;
		} else if (strncmp(s1, MSG_ORIG(MSG_LD_AUDIT_ARGS),
		    MSG_LD_AUDIT_ARGS_SIZE) == 0) {
			s1 += MSG_LD_AUDIT_ARGS_SIZE;
			audit_arg_count = atoi(s1);
			/*
			 * On sparc our stack must be 8 byte aligned
			 * so we might as well copy two arguments at
			 * a time.  Enforce that here.
			 */
			audit_arg_count += audit_arg_count % 2;
		} else if (strncmp(s1, MSG_ORIG(MSG_LD_DEBUG),
		    MSG_LD_DEBUG_SIZE) == 0) {
			s1 += MSG_LD_DEBUG_SIZE;
			if ((*s1 == '=') && (*++s1 != '\0'))
				dbg_str = s1;
			else if (strncmp(s1, MSG_ORIG(MSG_LD_OUTPUT),
			    MSG_LD_OUTPUT_SIZE) == 0) {
				s1 += MSG_LD_OUTPUT_SIZE;
				if (*s1 != '\0')
					dbg_file = s1;
			}
		} else if (strncmp(s1, MSG_ORIG(MSG_LD_PROFILE),
		    MSG_LD_PROFILE_SIZE) == 0) {
			s1 += MSG_LD_PROFILE_SIZE;
			if ((*s1 == '=') && (*++s1 != '\0')) {
				if (audit_libraries) {
					eprintf(ERR_WARNING,
					    MSG_INTL(MSG_AUD_PROFAUDINC));
					continue;
				}
				if (strcmp(s1, MSG_ORIG(MSG_FIL_RTLD)) == 0) {
#ifdef	PRF_RTLD
					profile_name = MSG_ORIG(MSG_FIL_RTLD);
#endif
					continue;
				}
				audit_libraries = MSG_ORIG(MSG_FIL_LDPROF);
				ldprof = TRUE;
			}
#ifdef	TIMING
		} else if (strncmp(s1, MSG_ORIG(MSG_LD_TIMING),
		    MSG_LD_TIMING_SIZE) == 0) {
			s1 += MSG_LD_TIMING_SIZE;
			if (*s1 != '\0')
				rtld_flags |= RT_FL_TIMING;
#endif
		}
	}

	s1 = flags_str;
	/* LINTED */
	while (flags_str) {
		size_t len;

		if (s2 = strchr(s1, ','))
			len = s2 - s1;
		else
			len = strlen(s1);

		if ((len == MSG_LDFLG_NOLAZY_SIZE) && (strncmp(s1,
		    MSG_ORIG(MSG_LDFLG_NOLAZY),
		    MSG_LDFLG_NOLAZY_SIZE) == 0)) {
			rtld_flags |= RT_FL_NOLAZYLD;
		} else if ((len == MSG_LDFLG_NODIRECT_SIZE) && (strncmp(s1,
		    MSG_ORIG(MSG_LDFLG_NODIRECT),
		    MSG_LDFLG_NODIRECT_SIZE) == 0)) {
			rtld_flags |= RT_FL_NODIRECT;
		}
		s1 += len;
		if (*s1 == '\0')
			break;
		s1++;
	}

	/*
	 * LD_WARN, LD_TRACE_SEARCH_PATHS and LD_VERBOSE are meaningful only if
	 * tracing.  Don't allow auditing when tracing.
	 */
	if (tracing) {
		audit_libraries = 0;
#ifdef	PRF_RTLD
		profile_name = 0;
#endif
	} else
		rtld_flags &= ~(RT_FL_SEARCH | RT_FL_WARN | RT_FL_VERBOSE);


	/*
	 * If we have a locale setting make sure its worth processing further.
	 */
	if (locale) {
		if (((*locale == 'C') && (*(locale + 1) == '\0')) ||
		    (strcmp(locale, MSG_ORIG(MSG_TKN_POSIX)) == 0))
			locale = 0;
	}
	return (1);
}

int
dowrite(Prfbuf * prf)
{
	/*
	 * We do not have a valid file descriptor, so we are unable
	 * to flush the buffer.
	 */
	if (prf->pr_fd == -1)
		return (0);
	(void) write(prf->pr_fd, prf->pr_buf, prf->pr_cur - prf->pr_buf);
	prf->pr_cur = prf->pr_buf;
	return (1);
}

/*
 * Simplified printing.  The following conversion specifications are supported:
 *
 *	% [#] [-] [min field width] [. precision] s|d|x|c
 *
 *
 * dorprf takes the output buffer in the form of Prfbuf which permits
 * the verification of the output buffer size and the concatination
 * of data to an already existing output buffer.  The Prfbuf
 * structure contains the following:
 *
 *	p_buf	- pointer to the beginning of the output buffer.
 *	p_cur	- pointer to the next available byte in the output
 *		  buffer.  By setting p_cur ahead of p_buf you can
 *		  append to an already existing buffer.
 *	p_len	- the size of the output buffer.  By setting p_len
 *		  to '0' you disable protection from overflows in the
 *		  output buffer.
 *	p_fd	- a pointer to the file-descriptor the buffer will
 *		  eventually be output to.  If p_fd is set to '-1' then
 *		  it's assumed there is no output buffer and doprf() will
 *		  return with an error if the output buffer is overflowed.
 *		  If p_fd is > '-1' then when the output buffer is filled
 *		  it will be flushed to 'p_fd' and then the available
 *		  for additional data.
 */
#define	FLG_UT_MINUS	0x0001	/* - */
#define	FLG_UT_SHARP	0x0002	/* # */
#define	FLG_UT_DOTSEEN	0x0008	/* dot appeared in format spec */

/*
 * This macro is for use from within doprf only.  it's to be used
 * for checking the output buffer size and placing characters into
 * the buffer.
 */
#define	PUTC(c) \
	{ \
		register char tmpc; \
		\
		tmpc = (c); \
		if ((bufsiz) && ((bp + 1) >= bufend)) { \
			prf->pr_cur = bp; \
			if (dowrite(prf) == 0) \
				return (0); \
			bp = prf->pr_cur; \
		} \
		*bp++ = tmpc; \
	}

size_t
doprf(const char * format, va_list args, Prfbuf * prf)
{
	char	c;
	char *	bp = prf->pr_cur;
	char *	bufend = prf->pr_buf + prf->pr_len;
	size_t	bufsiz = prf->pr_len;

	PRF_MCOUNT(65, doprf);

	while ((c = *format++) != '\0') {
		if (c != '%') {
			PUTC(c);
		} else {
			int	base = 0, flag = 0, width = 0, prec = 0;
			size_t	_i;
			int	_c, _n;
			char *	_s;
			int	ls = 0;
again:
			c = *format++;
			switch (c) {
			case '-':
				flag |= FLG_UT_MINUS;
				goto again;
			case '#':
				flag |= FLG_UT_SHARP;
				goto again;
			case '.':
				flag |= FLG_UT_DOTSEEN;
				goto again;
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				if (flag & FLG_UT_DOTSEEN)
					prec = (prec * 10) + c - '0';
				else
					width = (width * 10) + c - '0';
				goto again;
			case 'x':
			case 'X':
				base = 16;
				break;
			case 'd':
			case 'D':
			case 'u':
				base = 10;
				flag &= ~FLG_UT_SHARP;
				break;
			case 'l':
				base = 10;
				ls++; /* number of l's (long or long long) */
				if ((*format == 'l') ||
				    (*format == 'x') || (*format == 'X') ||
				    (*format == 'o') || (*format == 'O'))
					goto again;
				break;
			case 'o':
			case 'O':
				base = 8;
				break;
			case 'c':
				_c = va_arg(args, int);

				/* LINTED */
				for (_i = 24; _i > 0; _i -= 8) {
					if ((c = ((_c >> _i) & 0x7f)) != 0) {
						PUTC(c);
					}
				}
				if ((c = ((_c >> _i) & 0x7f)) != 0) {
					PUTC(c);
				}
				break;
			case 's':
				_s = va_arg(args, char *);
				_i = strlen(_s);
				/* LINTED */
				_n = (int)(width - _i);
				if (!prec)
					/* LINTED */
					prec = (int)_i;

				if (width && !(flag & FLG_UT_MINUS)) {
					while (_n-- > 0)
						PUTC(' ');
				}
				while (((c = *_s++) != 0) && prec--) {
					PUTC(c);
				}
				if (width && (flag & FLG_UT_MINUS)) {
					while (_n-- > 0)
						PUTC(' ');
				}
				break;
			case '%':
				PUTC('%');
				break;
			default:
				break;
			}

			/*
			 * Numeric processing
			 */
			if (base) {
				char		local[20];
				const char *	string =
						    MSG_ORIG(MSG_STR_HEXNUM);
				size_t		ssize = 0, psize = 0;
				const char *	prefix =
						    MSG_ORIG(MSG_STR_EMPTY);
				unsigned long long	num;

				switch (ls) {
				case 0:	/* int */
					num = (unsigned long long)
					    va_arg(args, unsigned int);
					break;
				case 1:	/* long */
					num = (unsigned long long)
					    va_arg(args, unsigned long);
					break;
				case 2:	/* long long */
					num = va_arg(args, unsigned long long);
					break;
				}

				if (flag & FLG_UT_SHARP) {
					if (base == 16) {
						prefix = MSG_ORIG(MSG_STR_HEX);
						psize = 2;
					} else {
						prefix = MSG_ORIG(MSG_STR_ZERO);
						psize = 1;
					}
				}
				if ((base == 10) && (long)num < 0) {
					prefix = MSG_ORIG(MSG_STR_NEGATE);
					psize = MSG_STR_NEGATE_SIZE;
					num = (unsigned long long)
					    (-(long long)num);
				}

				/*
				 * Convert the numeric value into a local
				 * string (stored in reverse order).
				 */
				_s = local;
				do {
					*_s++ = string[num % base];
					num /= base;
					ssize++;
				} while (num);

				/*
				 * Provide any precision or width padding.
				 */
				if (prec) {
					/* LINTED */
					_n = (int)(prec - ssize);
					while (_n-- > 0) {
						*_s++ = '0';
						ssize++;
					}
				}
				if (width && !(flag & FLG_UT_MINUS)) {
					/* LINTED */
					_n = (int)(width - ssize - psize);
					while (_n-- > 0) {
						PUTC(' ');
					}
				}

				/*
				 * Print any prefix and the numeric string
				 */
				while (*prefix)
					PUTC(*prefix++);
				do {
					PUTC(*--_s);
				} while (_s > local);

				/*
				 * Provide any width padding.
				 */
				if (width && (flag & FLG_UT_MINUS)) {
					/* LINTED */
					_n = (int)(width - ssize - psize);
					while (_n-- > 0)
						PUTC(' ');
				}
			}
		}
	}
	PUTC('\0');
	prf->pr_cur = bp;
	return (1);
}

/* VARARGS2 */
int
sprintf(char * buf, const char * format, ...)
{
	va_list	args;
	int	len;
	Prfbuf	prf;

	PRF_MCOUNT(67, sprintf);

	va_start(args, format);
	prf.pr_buf = prf.pr_cur = buf;
	prf.pr_len = 0;
	prf.pr_fd = -1;
	/* LINTED */
	len = (int)doprf(format, args, &prf);
	va_end(args);

	/* LINTED */
	return (len);
}

/* VARARGS3 */
int
snprintf(char * buf, size_t n, const char * format, ...)
{
	va_list	args;
	size_t	len;
	Prfbuf	prf;

	va_start(args, format);
	prf.pr_buf = prf.pr_cur = buf;
	prf.pr_len = n;
	prf.pr_fd = -1;
	if (doprf(format, args, &prf) == 0)
		len = 0;
	else
		len = (uintptr_t)prf.pr_cur - (uintptr_t)prf.pr_len;
	va_end(args);

	/* LINTED */
	return ((int)len);
}

/* VARARGS2 */
int
bufprint(Prfbuf * prf, const char * format, ...)
{
	va_list	args;
	int	rc;

	va_start(args, format);
	/* LINTED */
	rc = (int)doprf(format, args, prf);
	va_end(args);
	return (rc);
}

/* VARARGS1 */
int
printf(const char * format, ...)
{
	va_list	args;
	char 	buffer[ERRSIZE];
	Prfbuf	prf;

	PRF_MCOUNT(68, printf);

	va_start(args, format);
	prf.pr_buf = prf.pr_cur = buffer;
	prf.pr_len = ERRSIZE;
	prf.pr_fd = 1;
	(void) doprf(format, args, &prf);
	va_end(args);
	/*
	 * Trim trailing '\0' form buffer
	 */
	prf.pr_cur--;
	return (dowrite(&prf));
}

/* VARARGS2 */
void
eprintf(Error error, const char * format, ...)
{
	va_list			args;
	static char *		buffer = 0;
	int			bind;
	static const char *	strings[ERR_NUM] = {MSG_ORIG(MSG_STR_EMPTY)};
	static int		lock = 0;
	Prfbuf			prf;

	PRF_MCOUNT(66, eprintf);

	/*
	 * Because eprintf() uses a global buffer to store it's work a write
	 * lock is needed around the whole routine.
	 *
	 * Note: no lock is placed around printf() because it uses a buffer off
	 * of the stack and does it's write in a single atomic write().
	 */
	if ((lc_version > 0) &&
	    ((bind = bind_guard(THR_FLG_PRINT) == 1)))
		(void) rw_wrlock(&printlock);

	if (lock)
		return;

	/*
	 * Note: this lock is here to prevent the same thread from
	 *	 recursivly entering itself durring a eprintf.
	 *	 ie: durring eprintf malloc() fails and we try and call
	 *	 eprintf...and then malloc() fails....
	 */
	lock = 1;

	/*
	 * Allocate the error string buffer, if one doesn't already exist.
	 * Reassign lasterr, incase it was `cleared' by a dlerror() call.
	 */
	if (!buffer) {
		if ((buffer = (char *)malloc(ERRSIZE + 1)) == 0) {
			lasterr = (char *)MSG_ORIG(MSG_EMG_NOSPACE);
			lock = 0;
			if ((lc_version > 0) && bind) {
				(void) rw_unlock(&printlock);
				(void) bind_clear(THR_FLG_PRINT);
			}
			if (!(rtld_flags & RT_FL_APPLIC))
				write(2, MSG_ORIG(MSG_EMG_NOSPACE),
					MSG_EMG_NOSPACE_SIZE);
			return;
		}
	}
	lasterr = buffer;

	/*
	 * If we have completed startup initialization all error messages
	 * must be saved.  These are reported through dlerror().  If we're
	 * still in the initialization stage output the error directly and
	 * add a newline.
	 */
	va_start(args, format);

	prf.pr_buf = prf.pr_cur = buffer;
	prf.pr_len = ERRSIZE;

	if (!(rtld_flags & RT_FL_APPLIC))
		prf.pr_fd = 2;
	else
		prf.pr_fd = -1;

	if (error > ERR_NONE) {
		if (error == ERR_WARNING) {
			if (strings[ERR_WARNING] == 0)
			    strings[ERR_WARNING] = MSG_INTL(MSG_ERR_WARNING);
		} else if (error == ERR_FATAL) {
			if (strings[ERR_FATAL] == 0)
			    strings[ERR_FATAL] = MSG_INTL(MSG_ERR_FATAL);
		} else if (error == ERR_ELF) {
			if (strings[ERR_ELF] == 0)
			    strings[ERR_ELF] = MSG_INTL(MSG_ERR_ELF);
		}
		if (bufprint(&prf, MSG_ORIG(MSG_STR_EMSGFOR1),
		    rt_name, pr_name, strings[error]) == 0) {
			va_end(args);
			lock = 0;
			if ((lc_version > 0) && bind) {
				(void) rw_unlock(&printlock);
				(void) bind_clear(THR_FLG_PRINT);
			}
			eprintf(ERR_NONE, MSG_INTL(MSG_FMT_LONG),
				MSG_ORIG(MSG_STR_EMSGFOR1));
			eprintf(ERR_NONE, MSG_INTL(MSG_FMT_LONG),
				format);
			return;
		}
		/*
		 * remove the terminating '\0'
		 */
		prf.pr_cur--;
	}

	if (doprf(format, args, &prf) == 0) {
		va_end(args);
		lock = 0;
		if ((lc_version > 0) && bind) {
			(void) rw_unlock(&printlock);
			(void) bind_clear(THR_FLG_PRINT);
		}
		eprintf(ERR_NONE, MSG_INTL(MSG_FMT_LONG),
			format);
		return;
	}

	/*
	 * If this is an ELF error it will have been generated by a support
	 * object that has a dependency on libelf.  ld.so.1 doesn't generate any
	 * ELF error messages as it doesn't interact with libelf.  Determine the
	 * ELF error string.
	 */
	if (error == ERR_ELF) {
		static int (*		elfeno)() = 0;
		static const char * (*	elfemg)();
		const char *		emsg;
		Rt_map *		lmp = lml_rtld.lm_head;

		if (NEXT(lmp) && (elfeno == 0)) {
			if (((elfemg = (const char * (*)())dlsym_core(RTLD_NEXT,
			    MSG_ORIG(MSG_SYM_ELFERRMSG), lmp)) == 0) ||
			    ((elfeno = (int (*)())dlsym_core(RTLD_NEXT,
			    MSG_ORIG(MSG_SYM_ELFERRNO), lmp)) == 0))
				elfeno = 0;
		}
		/*
		 * Lookup the message; equivalent to elf_errmsg(elf_errno()).
		 */
		if (elfeno && ((emsg = (* elfemg)((* elfeno)())) != 0)) {
			prf.pr_cur--;
			if (bufprint(&prf, MSG_ORIG(MSG_STR_EMSGFOR2),
			    emsg) == 0) {
				va_end(args);
				lock = 0;
				if ((lc_version > 0) && bind) {
					(void) rw_unlock(&printlock);
					(void) bind_clear(THR_FLG_PRINT);
				}
				eprintf(ERR_NONE, MSG_INTL(MSG_FMT_LONG),
					MSG_ORIG(MSG_STR_EMSGFOR2));
				eprintf(ERR_NONE, MSG_INTL(MSG_FMT_LONG),
					format);
				return;
			}
		}
	}

	if (!(rtld_flags & RT_FL_APPLIC)) {
		*(prf.pr_cur - 1) = '\n';
		(void) dowrite(&prf);
	} else {
		DBG_CALL(Dbg_util_str(buffer));
	}
	va_end(args);
	lock = 0;
	if ((lc_version > 0) && bind) {
		(void) rw_unlock(&printlock);
		(void) bind_clear(THR_FLG_PRINT);
	}
}

/*
 * Exit.  If we arrive here with a non zero status it's because of a fatal
 * error condition (most commonly a relocation error).  If the application has
 * already had control, then the actual fatal error message will have been
 * recorded in the dlerror() message buffer.  Print the message before really
 * exiting.
 */
void
exit(int status)
{
	if (status) {
		if (rtld_flags & RT_FL_APPLIC) {
			(void) write(2, lasterr, strlen(lasterr));
			(void) write(2, MSG_ORIG(MSG_STR_NL), MSG_STR_NL_SIZE);
		}
		(void) kill(getpid(), SIGKILL);
	}
	_exit(status);
}

/*
 * Routines to co-ordinate the opening of /dev/zero and /proc.
 */
static int	dz_fd = FD_UNAVAIL;

void
dz_init(int fd)
{
	PRF_MCOUNT(69, dz_init);

	dz_fd = fd;
	rtld_flags |= RT_FL_CLEANUP;
}

int
dz_open()
{
	PRF_MCOUNT(70, dz_open);

	if (dz_fd == FD_UNAVAIL) {
		if ((dz_fd = open(MSG_ORIG(MSG_PTH_DEVZERO),
		    O_RDONLY)) == FD_UNAVAIL) {
			int	err = errno;

			eprintf(ERR_FATAL, MSG_INTL(MSG_SYS_OPEN),
			    MSG_ORIG(MSG_PTH_DEVZERO), strerror(err));
		}
	}
	rtld_flags |= RT_FL_CLEANUP;
	return (dz_fd);
}

static int	pr_fd = FD_UNAVAIL;

int
pr_open()
{
	char	proc[16];

	PRF_MCOUNT(71, pr_open);

	if (pr_fd == FD_UNAVAIL) {
		(void) snprintf(proc, 16, MSG_ORIG(MSG_FMT_PROC),
			(int)getpid());
		if ((pr_fd = open(proc, O_RDONLY)) == FD_UNAVAIL) {
			int	err = errno;

			eprintf(ERR_FATAL, MSG_INTL(MSG_SYS_OPEN), proc,
			    strerror(err));
		}
	}
	rtld_flags |= RT_FL_CLEANUP;
	return (pr_fd);
}

static int	cd_fd = FD_UNAVAIL;

int
cd_open()
{
	PRF_MCOUNT(88, cd_open);

	if (cd_fd == FD_UNAVAIL) {
		mode_t	mask;

		mask = umask(0);
		cd_fd = open(cd_file, (O_WRONLY | O_CREAT | O_APPEND), 0666);
		(void) umask(mask);
	}
	rtld_flags |= RT_FL_CLEANUP;
	return (cd_fd);
}

static int	nu_fd = FD_UNAVAIL;

int
nu_open()
{
	PRF_MCOUNT(89, nu_open);

	if (nu_fd == FD_UNAVAIL) {
		if ((nu_fd = open(MSG_ORIG(MSG_PTH_DEVNULL),
		    O_RDONLY)) == FD_UNAVAIL) {
			int	err = errno;

			eprintf(ERR_FATAL, MSG_INTL(MSG_SYS_OPEN),
			    MSG_ORIG(MSG_PTH_DEVNULL), strerror(err));
		}
	}
	rtld_flags |= RT_FL_CLEANUP;
	return (nu_fd);
}

/*
 * Routines to manage init file map structure
 */
Fmap *
fm_init()
{
	PRF_MCOUNT(86, fm_init);

	if (fmap == 0) {
		if ((fmap = (Fmap *)malloc(sizeof (Fmap))) == NULL)
			return (0);

		/*
		 * Map as private in case of any outstanding
		 * relocations against this page.
		 */
		fmap->fm_mflags = MAP_PRIVATE;
		fmap->fm_maddr = 0;
		fmap->fm_msize = syspagsz;
	}
	return (fmap);
}

/*
 * Generic cleanup routine called prior to returning control to the user.
 * Insures that any ld.so.1 specific file descriptors or temport mapping are
 * released.
 */
void
cleanup()
{
	PRF_MCOUNT(87, cleanup);

	if (dz_fd != FD_UNAVAIL) {
		(void) close(dz_fd);
		dz_fd = FD_UNAVAIL;
	}

	if (pr_fd != FD_UNAVAIL) {
		(void) close(pr_fd);
		pr_fd = FD_UNAVAIL;
	}

	if (cd_fd != FD_UNAVAIL) {
		(void) close(cd_fd);
		cd_fd = FD_UNAVAIL;
	}

	if (nu_fd != FD_UNAVAIL) {
		(void) close(nu_fd);
		nu_fd = FD_UNAVAIL;
	}

	if (fmap) {
		fmap->fm_mflags = MAP_PRIVATE;
		if (fmap->fm_maddr) {
			(void) munmap((caddr_t)fmap->fm_maddr, fmap->fm_msize);
			fmap->fm_maddr = 0;
		}
		fmap->fm_msize = syspagsz;
	}
	rtld_flags &= ~RT_FL_CLEANUP;
}

/*
 * Routines for initializing, testing, and freeing link map permission values.
 */
static Permit	__Permit = { 1, ~0UL };
static Permit *	_Permit = &__Permit;

Permit *
perm_get()
{
	unsigned long	_cnt, cnt = _Permit->p_cnt;
	unsigned long * _value, * value = &_Permit->p_value[0];
	Permit *	permit;

	PRF_MCOUNT(72, perm_get);

	/*
	 * Allocate a new Permit structure for return to the user based on the
	 * static Permit structure presently in use.
	 */
	if ((permit = (Permit *)calloc(sizeof (unsigned long), cnt + 1)) == 0)
		return ((Permit *)0);
	permit->p_cnt = cnt;
	_value = &permit->p_value[0];

	/*
	 * Determine the next available permission bit and update the global
	 * permit value to indicate this value is now taken.
	 */
	for (_cnt = 0; _cnt < cnt; _cnt++, _value++, value++) {
		unsigned long	bit;
		for (bit = 0x1; bit; bit = bit << 1) {
			if (*value & bit) {
				*value &= ~bit;
				*_value = bit;
				return (permit);
			}
		}
	}

	/*
	 * If all the present permission values have been exhausted allocate
	 * a new reference Permit structure.
	 */
	cnt++;
	if ((_Permit = (Permit *)calloc(sizeof (unsigned long), cnt + 1)) == 0)
		return ((Permit *)0);
	_Permit->p_cnt = cnt;
	value = &_Permit->p_value[cnt - 1];
	*value = ~0UL;

	/*
	 * Free the original Permit structure obtained for the user, and try
	 * again.
	 */
	free(permit);
	return (perm_get());
}

void
perm_free(Permit * permit)
{
	unsigned long	_cnt, cnt;
	unsigned long *	_value, * value;

	PRF_MCOUNT(73, perm_free);

	if (!permit)
		return;

	cnt = permit->p_cnt;
	_value = &permit->p_value[0];
	value = &_Permit->p_value[0];

	/*
	 * Set the users permit bit in the global Permit structure thus
	 * indicating thats its free for future use.
	 */
	for (_cnt = 0; _cnt < cnt; _cnt++, _value++, value++)
		*value |= *_value;

	free(permit);
}

int
perm_test(Permit * permit1, Permit * permit2)
{
	unsigned long	_cnt, cnt;
	unsigned long * _value1, * _value2;

	PRF_MCOUNT(74, perm_test);

	if (!permit1 || !permit2)
		return (0);

	_value1 = &permit1->p_value[0];
	_value2 = &permit2->p_value[0];

	/*
	 * Determine which permit structure is the smaller.  Loop through the
	 * `p_value' elements looking for a match.
	 */
	if ((cnt = permit1->p_cnt) > permit2->p_cnt)
		cnt = permit2->p_cnt;

	for (_cnt = 0; _cnt < cnt; _cnt++, _value1++, _value2++)
		if (*_value1 & *_value2)
			return (1);
	return (0);
}

Permit *
perm_set(Permit * permit1, Permit * permit2)
{
	unsigned long	_cnt, cnt;
	unsigned long * _value1, * _value2;
	Permit *	_permit = permit1;

	PRF_MCOUNT(75, perm_set);

	if (!permit2)
		return ((Permit *)0);

	cnt = permit2->p_cnt;
	_value2 = &permit2->p_value[0];

	/*
	 * If the original permission structure has not yet been initialized
	 * allocate a new structure for return to the user and simply copy the
	 * new structure to it.
	 */
	if (_permit == 0) {
		if ((_permit = (Permit *)calloc(sizeof (unsigned long),
		    cnt + 1)) == 0)
			return ((Permit *)0);
		_permit->p_cnt = cnt;
		_value1 = &_permit->p_value[0];
		for (_cnt = 0; _cnt < cnt; _cnt++, _value1++, _value2++)
			*_value1 = *_value2;
		return (_permit);
	}

	/*
	 * If we don't presently have room in the destination permit structure
	 * to hold the new permission bit, reallocate a new structure.
	 */
	if (cnt > _permit->p_cnt) {
		if ((_permit = (Permit *)realloc((void *) _permit,
		    (size_t)((cnt + 1) * sizeof (unsigned long)))) == 0)
			return ((Permit *)0);

		/*
		 * Make sure the newly added entries are cleared, and update the
		 * new permission structures count.
		 */
		for (_cnt = _permit->p_cnt; _cnt < cnt; _cnt++)
			_permit->p_value[_cnt] = 0;
		_permit->p_cnt = cnt;
	}

	/*
	 * Set the appropriate permission bits.
	 */
	_value1 = &_permit->p_value[0];
	for (_cnt = 0; _cnt < cnt; _cnt++, _value1++, _value2++)
		*_value1 |= *_value2;

	return (_permit);
}

/*
 * Remove a permit.  It's possible that we could calculate if any permits still
 * exist in permit1, and if so free up the entire structure.  However, if this
 * object has been assigned a permit once there's a strong change it will get
 * them assigned again, so leave the `empty' permit structure alone.
 */
Permit *
perm_unset(Permit * permit1, Permit * permit2)
{
	unsigned long	_cnt, cnt;
	unsigned long * _value1, * _value2;

	PRF_MCOUNT(76, perm_unset);

	if (!permit1 || !permit2)
		return ((Permit *)0);

	cnt = permit2->p_cnt;
	_value1 = &permit1->p_value[0];
	_value2 = &permit2->p_value[0];

	/*
	 * Unset the appropriate permission bits.
	 */
	for (_cnt = 0; _cnt < cnt; _cnt++, _value1++, _value2++)
		*_value1 &= ~(*_value2);

	return (permit1);
}



/*
 * If an environ symbol exists on the current link_map initialize
 * it to point to the system environ.  This is permits the 'getenv()'
 * operations to work.
 */
void
set_environ(Rt_map * lmp)
{
	Rt_map	*	_lmp;
	Sym *		sym;
	Slookup		sl;

	sl.sl_name = MSG_ORIG(MSG_SYM_ENVIRON);
	sl.sl_permit = PERMIT(lmp);
	sl.sl_cmap = lmp;
	sl.sl_imap = lmp;
	sl.sl_rsymndx = 0;

	if (sym = LM_LOOKUP_SYM(lmp)(&sl, &_lmp, LKUP_DEFT)) {
		char **		addr;
		addr = (char **)sym->st_value;
		if (!(FLAGS(_lmp) & FLG_RT_FIXED))
			addr = (char **)((uintptr_t)addr +
				(uintptr_t)ADDR(_lmp));
		*addr = (char *)environ;
	}
}

/*
 * Determine whether we have a secure executable.  Uid and gid information
 * can be passed to us via the aux vector, however if these values are -1
 * then use the appropriate system call to obtain them.
 *
 *  o	If the user is the root they can do anything
 *
 *  o	If the real and effective uid's don't match, or the real and
 *	effective gid's don't match then this is determined to be a `secure'
 *	application.
 *
 * This function is called prior to any dependency processing (see _setup.c).
 * Any secure setting will remain in effect for the life of the process.
 */
void
security(uid_t uid, uid_t euid, gid_t gid, gid_t egid)
{
	if (uid == -1)
		uid = getuid();
	if (uid) {
		if (euid == -1)
			euid = geteuid();
		if (uid != euid)
			rtld_flags |= RT_FL_SECURE;
		else {
			if (gid == -1)
				gid = getgid();
			if (egid == -1)
				egid = getegid();
			if (gid != egid)
				rtld_flags |= RT_FL_SECURE;
		}
	}
}

#ifdef	TIMING

/*
 * Dump the timing information gathered during process initialization.  This is
 * very crude.  (If gethrvtime() is in use but the process hasn't been enabled
 * to support this (see ptime(1)) then no timing information is gathered - hence
 * the elapse time check for 0).
 */
void
r_times()
{

#ifdef _BIG_ENDIAN
#define	LOWTIME		1
#define	HIGHTIME	0
#else
#define	LOWTIME		0
#define	HIGHTIME	1
#endif

	long	orig, elap;
	_r_times--;
	if ((elap = (_r_times->tm_u.hr - __r_times->tm_u.hr) / 1000) == 0)
		return;

	for (_r_times = __r_times, orig = 0; _r_times->tm_d; _r_times++) {
		long	diff = (orig ? _r_times->tm_u.lg[LOWTIME] - orig : 0);
		long	base = diff / elap;
		long	quot = base / 10;
		long	rema = base - quot * 10;

		(void) printf("%x:%x  \t%10d  %2d.%d%%\t%s\n",
		    _r_times->tm_u.lg[HIGHTIME], _r_times->tm_u.lg[LOWTIME],
		    diff, quot, rema, _r_times->tm_d);

		orig = _r_times->tm_u.lg[LOWTIME];
	}
	(void) printf("\ntotal elapsed time\t%10d\n\n", elap * 1000);

#undef LOWTIME
#undef HIGHTIME
}
#endif

/*
 * _REENTRANT code gets errno redefined to a function so provide for return
 * of the thread errno if applicable.  This has no meaning in ld.so.1 which
 * is basically singled threaded.  Provide the interface for our dependencies.
 */
#undef errno
int *
___errno()
{
	extern	int	errno;

	return (&errno);
}
