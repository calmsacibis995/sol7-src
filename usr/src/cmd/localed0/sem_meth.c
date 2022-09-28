/*
 * Copyright (c) 1996, 1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */
#pragma	ident	"@(#)sem_method.c 1.19	97/11/19  SMI"

/*
 * COPYRIGHT NOTICE
 *
 * This source code is designated as Restricted Confidential Information
 * and is subject to special restrictions in a confidential disclosure
 * agreement between HP, IBM, SUN, NOVELL and OSF.  Do not distribute
 * this source code outside your company without OSF's specific written
 * approval.  This source code, and all copies and derivative works
 * thereof, must be returned or destroyed at request. You must retain
 * this notice on any copies which you make.
 *
 * (c) Copyright 1990, 1991, 1992, 1993 OPEN SOFTWARE FOUNDATION, INC.
 * ALL RIGHTS RESERVED
 */
/*
 * OSF/1 1.2
 */
#if !defined(lint) && !defined(_NOIDENT)
static char rcsid[] = "@(#)$RCSfile: sem_method.c,v $ $Revision: 1.1.4.2 $"
	" (OSF) $Date: 1992/10/06 15:21:11 $";
#endif

/*
 * COMPONENT_NAME: (CMDLOC) Locale Database Commands
 *
 * FUNCTIONS:
 *
 *
 * (C) COPYRIGHT International Business Machines Corp. 1991
 * All Rights Reserved
 *
 * US Government Users Restricted Rights - Use, duplication or
 * disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
 */

#include "method.h"
#include "semstack.h"
#include "err.h"
#include <string.h>
#include <dlfcn.h>

/*
** GLOBAL variables
*/

/* array containing all the libraries */
/* specified in methods source file */
library_t	lib_array[LAST_METHOD + 1];

extern int Native;
int user_specified_libc = FALSE;


static char *
grab(int *n, int want, char **previous)
/*
 * ARGUMENTS:
 *	n		# of values still on the stack
 *	want		which value we want.  If want > n, use previous
 *	previous	Remembers old values
 */
{
	item_t	*it;
	char	*str;

	if (*n == want) {
		it = sem_pop();		/* Pop top and reduce count */
		*n -= 1;

		if (!it || it->type != SK_STR) {
			INTERNAL_ERROR;	/* Should _never_ happen */
		}

		str = strdup(it->value.str);
		destroy_item(it);

		if (previous)
			*previous = str;	/* This is NEW previous */
	} else {
		if (previous)
			str = *previous;
		else
			str = NULL;
	}

	return (str);
}

/*
*  FUNCTION: set_method
*
*  DESCRIPTION:
* Used to parse the information in the methods file. An index into
* the std_methods array is passed to this routine from the grammer.
* The number of items is also passed (1 means just the c_symbol is
* passed, the package and library name is inherited from previous and 2 means
* the c_symbol and package name is passed. 3 means everything present.
*/
void
set_method(int index, int number)
{
	char *sym, *lib_dir, *pkg, *lib_name;
	static char *lastlib = DEFAULT_METHOD;
	static char	*lastlib_name = DEFAULT_METHOD_NAM;
	static char	*lastlib_dir = DEFAULT_METHOD_DIR;
	static char *lastpkg = "libc";
	static int	oldfmt = 0;
	int i;
	char	*lib, *lib64;
	size_t	dirlen, namelen, mach64len, len, len64;


	/* get the strings for the new method name and the library off  */
	/* of the stack */

	if (number == 3 || oldfmt == 1) {
		lib_dir  = grab(&number, 3, &lastlib);
		pkg = grab(&number, 2, &lastpkg);
		sym = grab(&number, 1, NULL);
		oldfmt = 1;
	} else {
		lib_name = grab(&number, 4, &lastlib_name);
		lib_dir  = grab(&number, 3, &lastlib_dir);
		pkg = grab(&number, 2, &lastpkg);
		sym = grab(&number, 1, NULL);
	}

	if (oldfmt == 1) {
		/* old format */
		/* lib_dir contains whole library pathname */
		lib = MALLOC(char, strlen(lib_dir) + 1);
		(void) strcpy(lib, lib_dir);
		lib64 = lib;
	} else {
		dirlen = strlen(lib_dir);
		namelen = strlen(lib_name);
		mach64len = strlen(MACH64);
		len = dirlen + namelen + 2;
		len64 = dirlen + mach64len + namelen + 2;
		lib = MALLOC(char, len);
		lib64 = MALLOC(char, len64);
		(void) strcpy(lib, lib_dir);
		(void) strcpy(lib64, lib_dir);
		if (*(lib_dir + dirlen - 1) != '/') {
			(void) strcat(lib, "/");
			(void) strcat(lib64, "/");
		}
		(void) strcat(lib64, MACH64);
		(void) strcat(lib, lib_name);
		(void) strcat(lib64, lib_name);
	}

	for (i = 0; i <= LAST_METHOD; i++) {
	    if (lib_array[i].library == NULL) {
			/* Reached an empty slot, add lib */
			lib_array[i].library = lib;
			lib_array[i].library64 = lib64;
			break;
	    } else if (strcmp(lib_array[i].library, lib) == 0) {
			/* Found it already on our list */
			free(lib);
			free(lib64);
			lib = lib_array[i].library;
			lib64 = lib_array[i].library64;
			break;
	    }
	    /* Keep looking */
	}

	/* add the info to the std_methods table */

	std_methods[index].c_symbol[method_class] = sym;
	std_methods[index].lib_name[method_class] = lib;
	std_methods[index].lib64_name[method_class] = lib64;
	std_methods[index].package[method_class] = pkg;

	/*
	 * if the user's extension file uses "libc" as a package name
	 * then we'll assume the user has specified a libc so we
	 * don't have to specify one.
	 * if the user doesn't specify a libc then we must add a -lc
	 * to our c compile line.
	 */
	if (strcmp(pkg, "libc") == 0)
	    user_specified_libc = TRUE;

	/*
	 * invalidate preset values to allow the user specified method
	 * to be loaded
	 */
	if (index == CHARMAP_MBTOWC)
	    std_methods[index].instance[method_class] = (int(*)(void))NULL;
}




/*
*  FUNCTION: load_method
*
*  DESCRIPTION:
*	Load a method from a shared library.
*  INPUTS
*	idx	index into std_methods of entry to load.
*/

static void
load_method(int idx)
{
	char		*sym = std_methods[idx].c_symbol[method_class];
	char		*lib = std_methods[idx].lib_name[method_class];
	int		(*q)(void);			/* Function handle */
	void		*library_handle;

	/*
	 * Load the module to make certain that the referenced package is
	 * in our address space.
	 */

	library_handle = dlopen(lib, RTLD_LAZY);
	if (library_handle == (void *)NULL) {
		perror("localedef");
		error(ERR_LOAD_FAIL, sym, lib);  /* Never returns */
	}

	q = (int(*)(void))dlsym(library_handle, sym);

	if (q == (int(*)(void))NULL) {
		dlclose(library_handle);
		perror("localedef");
		error(ERR_LOAD_FAIL, sym, lib);
	}

	std_methods[idx].instance[method_class] = q;
}


/*
*  FUNCTION: check_methods
*
*  DESCRIPTION:
*  There are certain methods that do not have defaults because they are
*  dependent on the process code and file code relationship. These methods
*  must be specified by the user if they specify any new methods at all
*
*/

void
check_methods(void)
{
	int MustDefine[] = { CHARMAP_MBFTOWC, CHARMAP_FGETWC,
		CHARMAP_MBLEN,
		CHARMAP_MBSTOWCS, CHARMAP_MBTOWC, CHARMAP_WCSTOMBS,
		CHARMAP_WCSWIDTH, CHARMAP_WCTOMB, CHARMAP_WCWIDTH,
		CHARMAP_EUCPCTOWC, CHARMAP_WCTOEUCPC,
		CHARMAP_BTOWC, CHARMAP_WCTOB, CHARMAP_MBRLEN,
		CHARMAP_MBRTOWC, CHARMAP_WCRTOMB,
		CHARMAP_MBSRTOWCS, CHARMAP_WCSRTOMBS };
	int MustDefine_at_native[] = { CHARMAP_MBFTOWC_AT_NATIVE,
		CHARMAP_FGETWC_AT_NATIVE,
		CHARMAP_MBLEN,
		CHARMAP_MBSTOWCS_AT_NATIVE,
		CHARMAP_MBTOWC_AT_NATIVE,
		CHARMAP_WCSTOMBS_AT_NATIVE,
		CHARMAP_WCSWIDTH_AT_NATIVE,
		CHARMAP_WCTOMB_AT_NATIVE, CHARMAP_WCWIDTH_AT_NATIVE,
		CHARMAP_BTOWC_AT_NATIVE, CHARMAP_WCTOB_AT_NATIVE,
		CHARMAP_MBRTOWC_AT_NATIVE, CHARMAP_WCRTOMB_AT_NATIVE,
		CHARMAP_MBSRTOWCS_AT_NATIVE, CHARMAP_WCSRTOMBS_AT_NATIVE };


	int j;
	int current_codeset;

	for (j = 0; j < (sizeof (MustDefine_at_native) / sizeof (int)); j++) {
		int idx = MustDefine_at_native[j];

		if (std_methods[idx].instance[method_class] == NULL) {
		/*
		 * Need to load a method to handle this operation
		 */
			if (!std_methods[idx].c_symbol[method_class]) {
		/*
		 * Did not get a definition for this method, and we need
		 * it to be defined
		 */
				diag_error(ERR_METHOD_REQUIRED,
					std_methods[idx].method_name);
			} else {
		/*
		 * load the shared module and fill in the table slot
		 */
				load_method(idx);
			}
		}
	}
	if (Native == FALSE)	/* check EUC BC methods */
		for (j = 0; j < (sizeof (MustDefine) / sizeof (int)); j++) {
			int idx = MustDefine[j];

			if (std_methods[idx].instance[method_class] == NULL) {
		/*
		 * Need to load a method to handle this operation
		 */
				if (!std_methods[idx].c_symbol[method_class]) {
		/*
		 * Did not get a definition for this method, and we need
		 * it to be defined
		 */
					diag_error(ERR_METHOD_REQUIRED,
						std_methods[idx].method_name);
				} else {
		/*
		 * load the shared module and fill in the table slot
		 */
					load_method(idx);
				}
			}
		}

/*
 * Finally, run thru the remaining methods and copy from the *_CODESET
 * that makes sense based on what mb_cur_max is set to.
 * (which has all "standard" methods for the remaining non-null entries.
 * We only need the symbolic info for these methods, not the function pointers
 */

	if (mb_cur_max == 1)
		current_codeset = SB_CODESET;
	else
		current_codeset = MB_CODESET;

	for (j = 0; j <= LAST_METHOD; j++) {

		if (std_methods[j].c_symbol[method_class] == NULL) {
		/*
		 * Still not set up?
		 */
			std_methods[j].c_symbol[method_class] =
				std_methods[j].c_symbol[current_codeset];
			std_methods[j].package[method_class] =
				std_methods[j].package[current_codeset];
			std_methods[j].lib_name[method_class] =
				std_methods[j].lib_name[current_codeset];
			std_methods[j].lib64_name[method_class] =
				std_methods[j].lib64_name[current_codeset];

		}
	}
}
