/*
 * Copyright (c) 1998 by Sun Microsystems, Inc.
 * All Rights reserved.
 */

#pragma ident	"@(#)mbstatet.h	1.2	98/02/06 SMI"

#ifndef _MBSTATE_T
#define	_MBSTATE_T
typedef struct {
	void	*__lc_locale;	/* pointer to _LC_locale_t */
	void	*__state;		/* currently unused state flag */
	char	__consumed[8];	/* 8 bytes */
	char	__nconsumed;
	char	__fill[7];
} mbstate_t;
#endif /* _MBSTATE_T */
