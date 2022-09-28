/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident	"@(#)_loc_path.h	1.2	97/08/14 SMI"

#include <sys/isa_defs.h>

#define	_DFLT_LOC_PATH	"/usr/lib/locale/"

#define	_ICONV_PATH1	"/usr/lib/iconv/"
#define	_ICONV_PATH2	"%s%%%s.so"
#define	_WDMOD_PATH1	"/LC_CTYPE/"
#define	_WDMOD_PATH2	"wdresolve.so"

#ifdef _LP64

#if defined(__sparcv9)

#define	_MACH64_NAME		"sparcv9"

#else  /* !defined(__sparcv9) */

#error "Unknown architecture"

#endif /* defined(__sparcv9) */

#define	_MACH64_NAME_LEN	(sizeof (_MACH64_NAME) - 1)

#define	_ICONV_PATH	_ICONV_PATH1 _MACH64_NAME "/" _ICONV_PATH2
#define	_WDMOD_PATH	_WDMOD_PATH1 _MACH64_NAME "/" _WDMOD_PATH2

#else  /* !LP64 */

#define	_ICONV_PATH	_ICONV_PATH1 _ICONV_PATH2
#define	_WDMOD_PATH	_WDMOD_PATH1 _WDMOD_PATH2

#endif /* _LP64 */
