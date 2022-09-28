/* wscoll() and wsxfrm(). */
/* This is Sun's propriatry implementation of wsxfrm() and wscoll()	*/
/* using dynamic linking.  It is probably free from AT&T copyright.	*/
/* 	COPYRIGHT (C) 1991-1996 SUN MICROSYSTEMS, INC.			*/
/*	ALL RIGHT RESERVED.						*/

#pragma ident	"@(#)wsxfrm.c	1.16	96/12/20 SMI"

/*LINTLIBRARY*/

#pragma weak wscoll = _wscoll
#pragma weak wsxfrm = _wsxfrm

#include <wchar.h>
#include "libc.h"

size_t
_wsxfrm(wchar_t *s1, const wchar_t *s2, size_t n)
{
	return (_wcsxfrm(s1, s2, n));
}

int
_wscoll(const wchar_t *s1, const wchar_t *s2)
{
	return (_wcscoll(s1, s2));
}
