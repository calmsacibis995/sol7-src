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
 *
#if !defined(lint) && !defined(_NOIDENT)
static char rcsid[] = "@(#)$RCSfile: __fnmatch_sb.c,v $ $Revision: 1.3.4.3"
	" $ (OSF) $Date: 1992/11/05 21:58:45 $";
#endif
 */
/*
 * COMPONENT_NAME: (LIBCPAT) Standard C Library Pattern Functions
 *
 * FUNCTIONS: __fnmatch_sb
 *
 *
 *
 * (C) COPYRIGHT International Business Machines Corp. 1991
 * All Rights Reserved
 * Licensed Materials - Property of IBM
 *
 * US Government Users Restricted Rights - Use, duplication or
 * disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
 *
 * 1.6  com/lib/c/pat/__fnmatch_sb.c, , bos320, 9134320 8/13/91 14:47:43
 *
 */

/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)__fnmatch_sb.c 1.10	97/01/06  SMI"

/*LINTLIBRARY*/

#include <sys/localedef.h>
#include <string.h>
#include <fnmatch.h>
#include "patlocal.h"
#include "libc.h"

/*
 * forward declarations
 */
static int bracket(_LC_collate_t *, const char *, const char **, wchar_t,
			wchar_t, int);

int __fnmatch_sb(_LC_collate_t *phdl, const char *ppat, const char *string,
			const char *pstr, int flags)
{
	char	*pse;		/* ptr to next string character		*/
	int	stat;		/* recursive rfnmatch() return status	*/
	wchar_t	ucoll_s;	/* string unique coll value		*/

/*
 * Loop through pattern, matching string characters with pattern
 * Return success when end-of-pattern and end-of-string reached simultaneously
 * Return no match if pattern/string mismatch
 */
	while (*ppat != '\0') {
		switch (*ppat) {
/*
 * <backslash> quotes the next character unless FNM_NOESCAPE flag is set
 * Otherwise treat <backslash> as itself
 * Return no match if pattern ends with quoting <backslash>
 */
		case '\\':
			if ((flags & FNM_NOESCAPE) == 0)
				if (*++ppat == '\0')
					return (FNM_NOMATCH);
/*
 * Ordinary character in pattern matches itself in string
 * Continue if pattern character matches string character
 * Return no match if pattern character does not match string character
 */
		default:
ordinary:
			if (*ppat++ == *pstr++)
				break;
			else
				return (FNM_NOMATCH);
/*
 * <asterisk> matches zero or more string characters
 * Cannot match <slash> if FNM_PATHNAME is set
 * Cannot match leading <period> if FNM_PERIOD is set
 * Consecutive <asterisk> are redundant
 *
 * Return success if remaining pattern matches remaining string
 * Otherwise advance to the next string character and try again
 * Return no match if string exhausted and more pattern remains
 */
		case '*':
			while (*++ppat == '*')
				;
			if (*ppat == '\0') {
				if ((flags & FNM_PATHNAME) != 0 &&
				    strchr(pstr, '/') != NULL)
					return (FNM_NOMATCH);
				if (*pstr == '.' && (flags & FNM_PERIOD) != 0)
					if (pstr == string ||
					    (pstr[-1] == '/' &&
						(flags & FNM_PATHNAME) != 0))
						return (FNM_NOMATCH);
				return (0);
			}
			while (*pstr != '\0') {
				stat = __fnmatch_sb(phdl, ppat, string, pstr,
							flags);
				if (stat != FNM_NOMATCH)
					return (stat);
				if (*pstr == '/') {
					if ((flags & FNM_PATHNAME) != 0)
						return (FNM_NOMATCH);
				} else if (*pstr == '.' &&
				    (flags & FNM_PERIOD) != 0)
					if (pstr == string ||
					    ((pstr[-1] == '/') &&
						(flags & FNM_PATHNAME) != 0))
						return (FNM_NOMATCH);
				pstr++;
			}
			return (FNM_NOMATCH);
/*
 * <question-mark> matches any single character
 * Cannot match <slash> if FNM_PATHNAME is set
 * Cannot match leading <period> if FNM_PERIOD is set
 *
 * Return no match if string is exhausted
 * Otherwise continue with next pattern and string character
 */
		case '?':
			if (*pstr == '/') {
				if ((flags & FNM_PATHNAME) != 0)
					return (FNM_NOMATCH);
			} else if (*pstr == '.' && (flags & FNM_PERIOD) != 0)
				if (pstr == string ||
				    (pstr[-1] == '/' &&
					(flags & FNM_PATHNAME) != 0))
					return (FNM_NOMATCH);
			pstr++;
			ppat++;
			break;
/*
 * <left-bracket> begins a [bracket expression] which matches single collating
 * element
 * [bracket expression] cannot match <slash> if FNM_PATHNAME is set
 * [bracket expression] cannot match leading <period> if FNM_PERIOD is set
 */
		case '[':
			if (*pstr == '/') {
				if ((flags & FNM_PATHNAME) != 0)
					return (FNM_NOMATCH);
			} else if (*pstr == '.' && (flags & FNM_PERIOD) != 0)
				if (pstr == string ||
				    (pstr[-1] == '/' &&
					(flags & FNM_PATHNAME) != 0))
					return (FNM_NOMATCH);
/*
 * Determine unique collating value of next collating element
 */
			ucoll_s = _mbucoll(phdl, (char *)pstr, (char **)&pse);
			if ((ucoll_s < MIN_UCOLL) || (ucoll_s > MAX_UCOLL))
				return (FNM_NOMATCH);

/*
 * Compare unique collating value to [bracket expression]
 *   > 0  no match
 *   = 0  match found
 *   < 0  error, treat [] as individual characters
 */
			stat = bracket(phdl, ppat + 1, &ppat, (wchar_t)*pstr,
					ucoll_s, flags);
			if (stat == 0)
				pstr = pse;
			else if (stat > 0)
				return (FNM_NOMATCH);
			else
				goto ordinary;
			break;
		}
	}
/*
 * <NUL> following end-of-pattern
 * Return success if string is also at <NUL>
 * Return no match if string not at <NUL>
 */
	if (*pstr == '\0')
		return (0);
	else
		return (FNM_NOMATCH);
}


/*
 * bracket()    - Determine if [bracket] matches filename character
 *
 *	pdhl	 - ptr to __lc_collate structure
 *	ppat	 - ptr to position of '[' in pattern
 *	wc_s	 - process code of next filename character
 *	ucoll_s	 - unique collating weight of next filename character
 *	flags	 - fnmatch() flags, see <fnmatch.h>
 */

static int bracket(_LC_collate_t *phdl, const char *ppat, const char **pend,
			wchar_t wc_s, wchar_t ucoll_s, int flags)
{
	int	neg;		/* nonmatching [] expression	*/
	int	dash;		/* <hyphen> found for a-z range expr	*/
	int	prev_min_ucoll;	/* low end of range expr		*/
	wchar_t	min_ucoll;	/* minimum unique collating value	*/
	wchar_t	max_ucoll;	/* maximum unique collating value	*/
	const char *pb;		/* ptr to [bracket] pattern		*/
	const char *pi;		/* ptr to international [] expression	*/
	char	*piend;		/* ptr to character after intl [] expr	*/
	int	found;		/* match found flag			*/
	char	type;		/* international [] type =:.		*/
	wchar_t	pcoll;		/* primary collation weight		*/
	wint_t	i;		/* process code loop index		*/
	char	class[CLASS_SIZE]; /* character class with <NUL>	*/
	char	*pclass;	/* class[] ptr				*/
	wchar_t temp_coll;	/* temporary value for collation weight */
	wchar_t t_pcoll;	/* temporary value for collation weight */
/*
 * Leading <exclamation-mark> designates nonmatching [bracket expression]
 */
	pb = ppat;
	neg = 0;
	if (*pb == '!') {
		pb++;
		neg++;
	}
/*
 * Loop through each [] collating element comparing unique collating values
 */
	dash = 0;
	found = 0;
	prev_min_ucoll = 0;
	min_ucoll = 0;
	max_ucoll = 0;
	while (*pb != '\0') {
/*
 * Final <right-bracket> so return status based upon whether match was found
 * Return character after final ] if match is found
 * Ordinary character if first character of [barcket expression]
 */
		if (*pb == ']')
			if ((neg == 0 && pb > ppat) ||
			    (neg != 0 && pb > ppat + 1)) {
				if ((found ^ neg) == 0)
					return (FNM_NOMATCH);
				*pend = ++pb;
				return (0);
			}
/*
 * Return error if embedded <slash> found.  POSIX.2 3.13.3 says slashes
 * not allowed in brackets when FNM_PATHNAME set.
 */
		else if (*pb == '/' && (flags&FNM_PATHNAME))
			return (-1);
/*
 * Decode next [] element
 */
		if (dash == 0)
			prev_min_ucoll = min_ucoll;
		switch (*pb) {
		default:
ordinary:
			min_ucoll = _mbucoll(phdl, (char *)pb, (char **)&pb);
			if (min_ucoll == ucoll_s)
				found = 1;
			if ((min_ucoll < MIN_UCOLL) || (min_ucoll > MAX_UCOLL))
				return (-1);
			max_ucoll = min_ucoll;
			break;
/*
 * <hyphen> deliniates a range expression unless it is first character of []
 * or it immediately follows another <hyphen> and is therefore an end point
 */
		case '-':
			if (dash == 0) {
				if ((neg == 0 && pb == ppat) ||
				    (neg != 0 && pb == ppat + 1) ||
				    (pb[1] == ']'))
					goto ordinary;
				dash++;
				pb++;
				continue;
			} else
				goto ordinary;
/*
 * <left-bracket> initiates one of the following internationalization
 *   character expressions
 *   [: :] character class
 *   [= =] equivalence character class
 *   [. .] collation symbol
 *
 * it is treated as itself if not followed by appropriate special character
 * it is treated as itself if any error is encountered
 */
		case '[':
			pi = pb + 2;
			if ((type = pb[1]) == ':') {
				pclass = class;
				for (;;) {
					if (*pi == '\0')
						return (-1);
					if (*pi == ':' && pi[1] == ']')
						break;
					if (pclass >= &class[CLASS_SIZE])
						return (-1);
					*pclass++ = *pi++;
				}
				if (pclass == class)
					return (-1);
				*pclass = '\0';
				if (METHOD_NATIVE(__lc_ctype, iswctype)(
				    __lc_ctype, wc_s, wctype(class)) != 0)
					found = 1;
				min_ucoll = 0;
				pb = pi + 2;
				break;
			}
/*
 * equivalence character class
 *   treat as ordinary if character in NUL or invalid [= =]
 *   treat as collation symbol if not entire contents of [= =]
 *   locate address of collation weight table
 *   get unique collation weight, error if none
 *   set found flag if unique collation weight matches that of string collating
 *   element
 *   if no match, compare unique collation weight of all equivalent characters
 */
			else if (type == '=') {
				if (pi[1] == '\0')
					return (-1);
				if (pi[1] != type)
					goto coll_sym;
				if (pi[2] != ']')
					return (-1);
				(void) _getcolval(phdl, &min_ucoll, *pi, "",
						MAX_NORDS);
				if ((min_ucoll < MIN_UCOLL) ||
				    (min_ucoll > MAX_UCOLL))
					return (-1);
				max_ucoll = min_ucoll;
				(void) _getcolval(phdl, &pcoll, *pi, "", 0);
				for (i = MIN_PC; i <= MAX_PC; i++) {
					(void) _getcolval(phdl, &t_pcoll, i,
						"", 0);
					if (t_pcoll == pcoll) {
						(void) _getcolval(phdl,
							&temp_coll, i,
							"", MAX_NORDS);
						if (temp_coll == ucoll_s)
							found = 1;
						if (temp_coll < min_ucoll)
							min_ucoll = temp_coll;
						if (temp_coll > max_ucoll)
							max_ucoll = temp_coll;
					}
				}
				pb = pi + 3;
				break;
			}
/*
 * collation symbol
 *   locate address of collation weight table, error if none
 *   verify collation symbol is entire contents of [. .] expression, error if
 *   not get unique collation weight, error if none
 *   set found flag if collation weight matches that of string collating element
 */
			else if (type == '.') {
coll_sym:
				min_ucoll = _mbucoll(phdl, (char *)pi,
							(char **)&piend);
				if ((min_ucoll < MIN_UCOLL) ||
				    (min_ucoll > MAX_UCOLL) ||
				    (*piend != type) || (piend[1] != ']'))
					return (-1);
				if (min_ucoll == ucoll_s)
					found = 1;
				max_ucoll = min_ucoll;
				pb = piend + 2;
				break;
			} else
				goto ordinary;
		} /* end of switch */
/*
 * Check for the completion of a range expression and determine
 * whether string collating element falls between end points
 */
		if (dash != 0) {
			dash = 0;
			if (prev_min_ucoll == 0 || prev_min_ucoll > max_ucoll)
				return (-1);
			if (ucoll_s >= prev_min_ucoll && ucoll_s <= max_ucoll)
				found = 1;
			min_ucoll = 0;
		}
	} /* end of while */
/*
 * Return < 0 since <NUL> was found
 */
	return (-1);
}
