/*
 * Copyright (c) 1991-1997 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident   "@(#)wdresolve.c 1.10     97/10/24 SMI"

/*LINTLIBRARY*/

#define	dlopen	_dlopen
#define	dlsym	_dlsym
#define	dlclose	_dlclose

#pragma weak wdinit = _wdinit
#pragma weak wdchkind = _wdchkind
#pragma weak wdbindf = _wdbindf
#pragma weak wddelim = _wddelim
#pragma weak mcfiller = _mcfiller
#pragma weak mcwrap = _mcwrap

#include "mtlib.h"
#include <sys/types.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <euc.h>
#include <widec.h>
#include <wctype.h>
#include <limits.h>
#include <synch.h>
#include <thread.h>
#include <libintl.h>
#include "libc.h"

#if defined(PIC)

#include <locale.h>
#include <dlfcn.h>
#include "_loc_path.h"

#define	LC_NAMELEN 255	/* From _locale.h of libc. */
#ifdef _REENTRANT
static mutex_t wd_lock = DEFAULTMUTEX;
#endif /* _REENTRANT */

static int	wdchkind_C(wchar_t);
static int	(*wdchknd)(wchar_t) = wdchkind_C;
static int	wdbindf_C(wchar_t, wchar_t, int);
static int	(*wdbdg)(wchar_t, wchar_t, int) = wdbindf_C;
static wchar_t	*wddelim_C(wchar_t, wchar_t, int);
static wchar_t	*(*wddlm)(wchar_t, wchar_t, int) = wddelim_C;
static wchar_t	(*mcfllr)(void) = NULL;
static int	(*mcwrp)(void) = NULL;
static void	*modhandle = NULL;
static int	initialized = 0;

#endif /* !PIC */

/*
 * _wdinit() initializes other word-analyzing routines according to the
 * current locale.  Programmers are supposed to call this routine
 * every time the locale for the LC_CTYPE category is changed.  It returns
 * 0 when every initialization completes successfully, or -1 otherwise.
 */
int
_wdinit(void)
{
#if defined(PIC)

	char wdmodpath[LC_NAMELEN + 39];

	if (modhandle)
		(void) dlclose(modhandle);
#ifdef I18NDEBUG
	(void) strcpy(wdmodpath, getenv("LC_ROOT") ?
	    getenv("LC_ROOT") : _DFLT_LOC_PATH);
	(void) strcat(wdmodpath, "/");
#else
	(void) strcpy(wdmodpath, _DFLT_LOC_PATH);
#endif /* I18NDEBUG */
	(void) strcat(wdmodpath, setlocale(LC_CTYPE, NULL));
	(void) strcat(wdmodpath, _WDMOD_PATH);
	if ((modhandle = dlopen(wdmodpath, RTLD_LAZY)) != NULL) {
		wdchknd = (int(*)(wchar_t))dlsym(modhandle, "_wdchkind_");
		if (wdchknd == NULL) wdchknd = wdchkind_C;
		wdbdg = (int(*)(wchar_t, wchar_t, int))dlsym(modhandle,
			"_wdbindf_");
		if (wdbdg == NULL) wdbdg = wdbindf_C;
		wddlm = (wchar_t *(*)(wchar_t, wchar_t, int))
			dlsym(modhandle, "_wddelim_");
		if (wddlm == NULL) wddlm = wddelim_C;
		mcfllr = (wchar_t(*)(void))dlsym(modhandle, "_mcfiller_");
		mcwrp = (int(*)(void))dlsym(modhandle, "_mcwrap_");
	} else {
		wdchknd = wdchkind_C;
		wdbdg = wdbindf_C;
		wddlm = wddelim_C;
		mcfllr = NULL;
		mcwrp = NULL;
	}
	initialized = 1;
	return ((modhandle && wdchknd && wdbdg && wddlm && mcfllr && mcwrp) ?
		0 : -1);
#else /* !PIC */
	return (0);	/* A fake success from static lib version. */
#endif /* !PIC */
}

/*
 * _wdchkind() returns a non-negative integral value unique to the kind
 * of the character represented by given argument.
 */
int
_wdchkind(wchar_t wc)
{
#if defined(PIC)
	int i;

	(void) mutex_lock(&wd_lock);
	if (!initialized)
		(void) _wdinit();
	i = (*wdchknd)(wc);
	(void) mutex_unlock(&wd_lock);
	return (i);
}
static int
wdchkind_C(wchar_t wc)
{
#endif /* !PIC */
	switch (wcsetno(wc)) {
	case 1:
		return (2);
	case 2:
		return (3);
	case 3:
		return (4);
	case 0:
		return (isalpha(wc) || isdigit(wc) || wc == ' ');
	}
	return (0);
}

/*
 * _wdbindf() returns an integral value (0 - 7) indicating binding
 *  strength of two characters represented by the first two arguments.
 * It returns -1 when either of the two character is not printable.
 */
/*ARGSUSED*/
int
_wdbindf(wchar_t wc1, wchar_t wc2, int type)
{
#if defined(PIC)
	int i;

	(void) mutex_lock(&wd_lock);
	if (!initialized)
		(void) _wdinit();
	if (!iswprint(wc1) || !iswprint(wc2))
		return (-1);
	i = (*wdbdg)(wc1, wc2, type);
	(void) mutex_unlock(&wd_lock);
	return (i);
}
/*ARGSUSED*/
static int
wdbindf_C(wchar_t wc1, wchar_t wc2, int type)
{
#else
	if (!iswprint(wc1) || !iswprint(wc2))
		return (-1);
#endif /* !PIC */
	if (csetlen(wc1) > 1 && csetlen(wc2) > 1)
		return (4);
	return (6);
}

/*
 * _wddelim() returns a pointer to a null-terminated word delimiter
 * string in wchar_t type that is thought most appropriate to join
 * a text line ending with the first argument and a line beginning
 * with the second argument, with.  When either of the two character
 * is not printable it returns a pointer to a null wide character.
 */
/*ARGSUSED*/
wchar_t *
_wddelim(wchar_t wc1, wchar_t wc2, int type)
{
#if defined(PIC)
	wchar_t *i;

	(void) mutex_lock(&wd_lock);
	if (!initialized)
		(void) _wdinit();
	if (!iswprint(wc1) || !iswprint(wc2)) {
		(void) mutex_unlock(&wd_lock);
		return ((wchar_t *)L"");
	}
	i = (*wddlm)(wc1, wc2, type);
	(void) mutex_unlock(&wd_lock);
	return (i);
}
/*ARGSUSED*/
static wchar_t *
wddelim_C(wchar_t wc1, wchar_t wc2, int type)
{
#else /* !PIC */
	if (!iswprint(wc1) || !iswprint(wc2))
		return ((wchar_t *)L"");
#endif /* !PIC */
	return ((wchar_t *)L" ");
}

/*
 * _mcfiller returns a printable ASCII character suggested for use in
 * filling space resulted by a multicolumn character at the right margin.
 */
wchar_t
_mcfiller(void)
{
#if defined(PIC)
	wchar_t fillerchar;

	(void) mutex_lock(&wd_lock);
	if (!initialized)
		(void) _wdinit();
	if (mcfllr) {
		fillerchar = (*mcfllr)();
		if (!fillerchar)
			fillerchar = (wchar_t)'~';
		if (iswprint(fillerchar)) {
			(void) mutex_unlock(&wd_lock);
			return (fillerchar);
		}
	}
	(void) mutex_unlock(&wd_lock);
	return ((wchar_t)'~');
#else /* !PIC */
	return ((wchar_t)'~');
#endif /* !PIC */
}

/*
 * mcwrap returns an integral value indicating if a multicolumn character
 * on the right margin should be wrapped around on a terminal screen.
 */
int
_mcwrap(void)
{
#if defined(PIC)

	(void) mutex_lock(&wd_lock);
	if (!initialized)
		(void) _wdinit();
	if (mcwrp)
		if ((*mcwrp)() == 0) {
			(void) mutex_unlock(&wd_lock);
			return (0);
		}
	(void) mutex_unlock(&wd_lock);
	return (1);
#else /* !PIC */
	return (1);
#endif /* !PIC */
}
