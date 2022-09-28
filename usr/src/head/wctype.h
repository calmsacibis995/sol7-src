/*	wctype.h	1.13 89/11/02 SMI; JLE	*/
/*	from AT&T JAE 2.1			*/
/*	definitions for international functions	*/

/*
 * Copyright (c) 1991,1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_WCTYPE_H
#define	_WCTYPE_H

#pragma ident	"@(#)wctype.h	1.17	97/12/18 SMI"

#include <sys/feature_tests.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef	_WINT_T
#define	_WINT_T
#if defined(_LP64)
typedef	int	wint_t;
#else
typedef long	wint_t;
#endif
#endif  /* !_WINT_T */

#ifndef	_WCTYPE_T
#define	_WCTYPE_T
typedef	int	wctype_t;
#endif

typedef unsigned int	wctrans_t;

/* not XPG4 and not XPG4v2 */
#if (!(defined(_XOPEN_SOURCE) && (_XOPEN_VERSION - 0 == 4)))
#ifndef WEOF
#define	WEOF	((wint_t) (-1))
#endif
#endif /* not XPG4 and not XPG4v2 */

#ifdef __STDC__
extern	int iswalpha(wint_t);
extern	int iswupper(wint_t);
extern	int iswlower(wint_t);
extern	int iswdigit(wint_t);
extern	int iswxdigit(wint_t);
extern	int iswalnum(wint_t);
extern	int iswspace(wint_t);
extern	int iswpunct(wint_t);
extern	int iswprint(wint_t);
extern	int iswgraph(wint_t);
extern	int iswcntrl(wint_t);
/* tow* also become functions */
extern	wint_t towlower(wint_t);
extern	wint_t towupper(wint_t);
extern	wctrans_t wctrans(const char *);
extern	wint_t towctrans(wint_t, wctrans_t);
extern  int iswctype(wint_t, wctype_t);
extern  wctype_t wctype(const char *);
#else
extern  int iswalpha();
extern  int iswupper();
extern  int iswlower();
extern  int iswdigit();
extern  int iswxdigit();
extern  int iswalnum();
extern  int iswspace();
extern  int iswpunct();
extern  int iswprint();
extern  int iswgraph();
extern  int iswcntrl();
/* tow* also become functions */
extern  wint_t towlower();
extern  wint_t towupper();
extern	wctrans_t wctrans();
extern	wint_t towctrans();
extern  int iswctype();
extern  wctype_t wctype();
#endif

/* bit definition for character class */

#define	_E1	0x00000100	/* phonogram (international use) */
#define	_E2	0x00000200	/* ideogram (international use) */
#define	_E3	0x00000400	/* English (international use) */
#define	_E4	0x00000800	/* number (international use) */
#define	_E5	0x00001000	/* special (international use) */
#define	_E6	0x00002000	/* other characters (international use) */
#define	_E7	0x00004000	/* reserved (international use) */
#define	_E8	0x00008000	/* reserved (international use) */

#define	_E9	0x00010000
#define	_E10	0x00020000
#define	_E11	0x00040000
#define	_E12	0x00080000
#define	_E13	0x00100000
#define	_E14	0x00200000
#define	_E15	0x00400000
#define	_E16	0x00800000
#define	_E17	0x01000000
#define	_E18	0x02000000
#define	_E19	0x04000000
#define	_E20	0x08000000
#define	_E21	0x10000000
#define	_E22	0x20000000
#define	_E23	0x40000000
#define	_E24	0x80000000

/* do not allow any of the following in a strictly conforming application */
#if ((!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE) && \
	!defined(_POSIX_SOURCE) && (__STDC__ - 0 != 1)) || \
	defined(__EXTENSIONS__))

#include <ctype.h>
#include <wchar.h>

/*
 * data structure for supplementary code set
 * for character class and conversion
 */
struct	_wctype {
	wchar_t	tmin;		/* minimum code for wctype */
	wchar_t	tmax;		/* maximum code for wctype */
	unsigned char  *index;	/* class index */
	unsigned int   *type;	/* class type */
	wchar_t	cmin;		/* minimum code for conversion */
	wchar_t	cmax;		/* maximum code for conversion */
	wchar_t *code;		/* conversion code */
};

/* character classification functions */

/* iswascii is still a macro */
#define	iswascii(c)	isascii(c)

/* isw*, except iswascii(), are not macros any more.  They become functions */
#ifdef __STDC__

extern	unsigned _iswctype(wchar_t, int);
extern	wchar_t _trwctype(wchar_t, int);
/* is* also become functions */
extern	int isphonogram(wint_t);
extern	int isideogram(wint_t);
extern	int isenglish(wint_t);
extern	int isnumber(wint_t);
extern	int isspecial(wint_t);
#else

extern  unsigned _iswctype();
extern  wchar_t _trwctype();
/* is* also become functions */
extern  int isphonogram();
extern  int isideogram();
extern  int isenglish();
extern  int isnumber();
extern  int isspecial();
#endif

#define	iscodeset0(c)	isascii(c)
#define	iscodeset1(c)	(((c) & WCHAR_CSMASK) == WCHAR_CS1)
#define	iscodeset2(c)	(((c) & WCHAR_CSMASK) == WCHAR_CS2)
#define	iscodeset3(c)	(((c) & WCHAR_CSMASK) == WCHAR_CS3)

#endif /* ((!defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE) ... */

#ifdef	__cplusplus
}
#endif

#endif	/* _WCTYPE_H */
