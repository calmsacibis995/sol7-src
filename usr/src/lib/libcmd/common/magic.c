/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma ident	"@(#)magic.c	1.15	97/07/16 SMI"	/* SVr4.0 1.2	*/
/*LINTLIBRARY*/

/*	Copyright (c) 1987, 1988 Microsoft Corporation	*/
/*	  All Rights Reserved	*/

/*	This Module contains Proprietary Information of Microsoft  */
/*	Corporation and should be treated as Confidential.	   */

/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include "libcmd.h"
#include "mtlib.h"

/*
**	Types
*/

#define	BYTE	0
#define	SHORT	2
#define	LONG	4
#define	STR	8

/*
**	Opcodes
*/

#define	EQ	0
#define	GT	1
#define	LT	2
#define	STRC	3	/* string compare */
#define	ANY	4
#define	AND	5
#define	NSET	6	/* True if bit is not set */
#define	SUB	64	/* or'ed in, SUBstitution string, for example */
			/* %ld, %s, %lo mask: with bit 6 on, used to locate */
			/* print formats */
/*
**	Misc
*/

#define	BSZ	128
#define	NENT	200

/*
**	Structure of magic file entry
*/

#if !defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN	"SYS_TEST"
#endif

struct	entry	{
	char	e_level;	/* 0 or 1 */
	long	e_off;		/* in bytes */
	char	e_type;		/* BYTE, SHORT, LONG, STR */
	char	e_opcode;	/* EQ, GT, LT, ANY, AND, NSET */
	long	e_mask;		/* if non-zero, mask value with this */
	union	{
		long	num;
		char	*str;
	}	e_value;
	char	*e_str;
};

typedef	struct entry	Entry;
static Entry	*mtab;

static long	atolo(char *s);

#ifdef _REENTRANT
static mutex_t mtab_lock = DEFAULTMUTEX;
#endif _REENTRANT

#define	prf(x)	printf("%s:%s", x, strlen(x) > 6 ? "\t" : "\t\t");

static char *
getstr(char *p)
{
	char	*newstr;
	char	*s;
	long	val;
	int	base;

	newstr = (char *)malloc((strlen(p) + 1) * sizeof (char));
	if (newstr == NULL) {
		perror(_dgettext(TEXT_DOMAIN,
		    "magic table string allocation"));
		return (NULL);
	}

	s = newstr;
	while (*p != '\0') {
		if (*p != '\\') {
			*s++ = *p++;
			continue;
		}
		p++;
		if (*p == '\0')
			break;
		if (isdigit(*p)) {
			if (*p == '0' && (*(p+1) == 'x' || *(p+1) == 'X')) {
				/* hex */
				base = 16;
			} else {
				base = 8;
			}
			errno = 0;
			val = strtol(p, &p, base);
			if (val > UCHAR_MAX || val < 0 || errno != 0) {
				perror(_dgettext(TEXT_DOMAIN,
				    "magic table invalid string value"));
				return (NULL);
			}
			*s++ = (char)val;
		} else {
			/* escape the character */
			switch (*p) {
			case 'n':
				*s = '\n';
				break;
			case 'r':
				*s = '\r';
				break;
			/* the next case causes a compiler warning for */
			/* some reason */
			/*
			case 'a':
				*s = '\a';
				break;

			 */
			case 'b':
				*s = '\b';
				break;
			case 'f':
				*s = '\f';
				break;
			case 't':
				*s = '\t';
				break;
			case 'v':
				*s = '\v';
				break;
			default:
				*s = *p;
				break;
			}
			p++;
			s++;
		}
	}
	*s = '\0';
	return (newstr);
}

int
mkmtab(char *magfile, int cflg)
{
	Entry	*ep;
	FILE	*fp;
	int	lcnt = 0;
	char	buf[BSZ];
	Entry	*mend;

	ep = (Entry *) calloc(sizeof (Entry), NENT);
	if (ep == NULL) {
		(void) fprintf(stderr,
		_dgettext(TEXT_DOMAIN, "no memory for magic table\n"));
		return (-1);
	}

	(void) mutex_lock(&mtab_lock);
	mtab = ep;
	mend = &mtab[NENT];
	fp = fopen(magfile, "r");
	if (fp == NULL) {
		(void) fprintf(stderr,
		_dgettext(TEXT_DOMAIN, "cannot open magic file <%s>.\n"),
		magfile);
		(void) mutex_unlock(&mtab_lock);
		return (-1);
	}
	while (fgets(buf, BSZ, fp) != NULL) {
		char	*p = buf;
		char	*p2;
		char	*p3;
		char	opc;

		lcnt++;
		if (*p == '\n' || *p == '#')
			continue;


			/* LEVEL */
		if (*p == '>') {
			ep->e_level = 1;
			p++;
		}
			/* OFFSET */
		p2 = strchr(p, '\t');
		if (p2 == NULL) {
			if (cflg)
				(void) fprintf(stderr,
				_dgettext(TEXT_DOMAIN,
				    "fmt error, no tab after %s on line %d\n"),
				    p, lcnt);
			continue;
		}
		*p2++ = NULL;
		ep->e_off = atolo(p);
		while (*p2 == '\t')
			p2++;
			/* TYPE */
		p = p2;
		p2 = strchr(p, '\t');
		if (p2 == NULL) {
			if (cflg)
				(void) fprintf(stderr,
				_dgettext(TEXT_DOMAIN, "\
fmt error, no tab after %s on line %d\n"), p, lcnt);
			continue;
		}
		*p2++ = NULL;
		p3 = strchr(p, '&');
		if (p3 != NULL) {
			*p3++ = '\0';
			ep->e_mask = atolo(p3);
		} else
			ep->e_mask = 0L;
		if (*p == 's') {
			if (*(p+1) == 'h')
				ep->e_type = SHORT;
			else
				ep->e_type = STR;
		} else if (*p == 'l')
			ep->e_type = LONG;
		while (*p2 == '\t')
			p2++;
			/* OP-VALUE */
		p = p2;
		p2 = strchr(p, '\t');
		if (p2 == NULL) {
			if (cflg)
				(void) fprintf(stderr,
				    _dgettext(TEXT_DOMAIN,
					"fmt error, no tab after %s on "
					    "line %d\n"),
					p, lcnt);
			continue;
		}
		*p2++ = NULL;
		if (ep->e_type != STR) {
			opc = *p++;
			switch (opc) {
			case '=':
				ep->e_opcode = EQ;
				break;

			case '>':
				ep->e_opcode = GT;
				break;

			case '<':
				ep->e_opcode = LT;
				break;

			case 'x':
				ep->e_opcode = ANY;
				break;

			case '&':
				ep->e_opcode = AND;
				break;

			case '^':
				ep->e_opcode = NSET;
				break;
			default:	/* EQ (i.e. 0) is default	*/
				p--;	/* since global ep->e_opcode=0	*/
			}
		}
		if (ep->e_opcode != ANY) {
			if (ep->e_type != STR)
				ep->e_value.num = atolo(p);
			else if ((ep->e_value.str = getstr(p)) == NULL) {
				(void) mutex_unlock(&mtab_lock);
				return (-1);
			}
		}
		p2 += strspn(p2, "\t");
			/* STRING */
		if ((ep->e_str = strdup(p2)) == NULL) {
			perror(_dgettext(TEXT_DOMAIN,
			    "magic table message allocation"));
			(void) mutex_unlock(&mtab_lock);
			return (-1);
		} else {
			if ((p = strchr(ep->e_str, '\n')) != NULL)
				*p = '\0';
			if (strchr(ep->e_str, '%') != NULL)
				ep->e_opcode |= SUB;
		}
		ep++;
		if (ep >= mend) {
			size_t tbsize, oldsize;

			oldsize = mend - mtab;  /* off by one? */
			tbsize = (NENT + oldsize) * sizeof (Entry);
			if ((mtab = (Entry *) realloc((char *) mtab, tbsize))
				== NULL) {
				perror(_dgettext(TEXT_DOMAIN,
				    "magic table overflow"));
				(void) mutex_unlock(&mtab_lock);
				return (-1);
			} else {
				(void) memset(mtab + oldsize, 0,
				    sizeof (Entry) * NENT);
				mend = &mtab[tbsize / sizeof (Entry)];
				ep = &mtab[oldsize];
			}
		}
	}
	ep->e_off = -1L;
	if (fclose(fp) == EOF) {
		perror(magfile);
		(void) mutex_unlock(&mtab_lock);
		return (-1);
	}
	mtab = (Entry *)realloc((char *)mtab, (1 + ep - mtab) * sizeof (Entry));
	if (mtab == NULL) {
		perror(_dgettext(TEXT_DOMAIN, "magic table memory error"));
		(void) mutex_unlock(&mtab_lock);
		return (-1);
	}
	(void) mutex_unlock(&mtab_lock);
	return (0);
}

static long
atolo(char *s)		/* determine read format and get e_value.num */
{
	char	*fmt = "%ld";
	long	j = 0L;

	if (*s == '0') {
		s++;
		if (*s == 'x') {
			s++;
			fmt = "%lx";
		} else
			fmt = "%lo";
	}
	(void) sscanf(s, fmt, &j);
	return (j);
}

/* Check for Magic Table entries in the file */
int
ckmtab(char *buf, int bufsize, int silent)
{
	int 		result;
	unsigned short	svar;
	Entry		*ep;
	char		*p;
	int		lev1 = 0;
	union	{
		long	l;
		char	ch[4];
	}	val, revval;
#ifdef u3b
	static	char	tmpbyte;
#endif /* u3b */

	(void) mutex_lock(&mtab_lock);

	for (ep = mtab; ep->e_off != -1L; ep++) {  /* -1 offset marks end of */
		if (lev1) {			/* valid magic file entries */
			if (ep->e_level != 1)
				break;
		} else if (ep->e_level == 1)
			continue;
		if (ep->e_off > (long) bufsize)
			continue;
		p = &buf[ep->e_off];
		switch (ep->e_type) {
		case STR:
		{
			if (strncmp(p, ep->e_value.str,
			    strlen(ep->e_value.str)))
				continue;
			if (!silent) {
				if (ep->e_opcode & SUB)
					(void) printf(ep->e_str,
					    ep->e_value.str);
				else
					(void) printf(ep->e_str);
			}
			lev1 = 1;
		}

		case BYTE:
			val.l = (long)(*(unsigned char *) p);
			break;

		case SHORT:
			/*
			 * This code is modified to avoid word alignment
			 * problem which caused command "more" to core dump
			 * on a 3b2 machine when the  word pointed to by p
			 * is not aleast halfword aligned.
			 */
			(void) memcpy(&svar, p, sizeof (unsigned short));
			val.l = svar;
			break;

		case LONG:
			/* This code is modified to avoid word */
			/* alignment problem */
			(void) memcpy(&val.l, p, sizeof (long));
			break;
		}
		if (ep->e_mask)
			val.l &= ep->e_mask;
		switch (ep->e_opcode & ~SUB) {
		case EQ:
#ifdef u3b
			if (val.l != ep->e_value.num)
				if (ep->e_type == SHORT) {
					/* reverse bytes */
					revval.l = 0L;
					tmpbyte = val.ch[3];
					revval.ch[3] = val.ch[2];
					revval.ch[2] = tmpbyte;
					if (revval.l != ep->e_value.num)
						continue;
					else
						break;
				} else
					continue;
			else
				break;
#else
			if (val.l != ep->e_value.num)
				continue;
			break;
#endif
		case GT:
			if (val.l <= ep->e_value.num)
				continue;
			break;

		case LT:
			if (val.l >= ep->e_value.num)
				continue;
			break;

		case AND:
			if ((val.l & ep->e_value.num) == ep->e_value.num)
				break;
			continue;
		case NSET:
			if ((val.l & ep->e_value.num) != ep->e_value.num)
				break;
			continue;
		}
		if (lev1 && !silent)
			(void) putchar(' ');
		if (!silent) {
			if (ep->e_opcode & SUB)
				(void) printf(ep->e_str, val.l);
			else
				(void) printf(ep->e_str);
		}
		lev1 = 1;
	}
	result = lev1 ? (int)(1 + ep - mtab) : 0;

	(void) mutex_unlock(&mtab_lock);
	return (result);
}

static void
showstr(char *s, int width)
{
	char c;

	while ((c = *s++) != '\0')
		if (c >= 040 && c < 0176) {
			(void) putchar(c);
			width--;
		} else {
			(void) putchar('\\');
			switch (c) {

			case '\n':
				(void) putchar('n');
				width -= 2;
				break;

			case '\r':
				(void) putchar('r');
				width -= 2;
				break;

			case '\b':
				(void) putchar('b');
				width -= 2;
				break;

			case '\t':
				(void) putchar('t');
				width -= 2;
				break;

			case '\f':
				(void) putchar('f');
				width -= 2;
				break;

			case '\v':
				(void) putchar('v');
				width -= 2;
				break;

			default:
				(void) printf("%.3o", c & 0377);
				width -= 4;
				break;
			}
		}
	while (width >= 0) {
		(void) putchar(' ');
		width--;
	};
}

static char *
type_to_name(Entry *ep)
{
	static char buf[20];
	char	*s;

	switch (ep->e_type) {
	case BYTE:
		s = "byte";
		break;
	case SHORT:
		s = "short";
		break;
	case LONG:
		s = "long";
		break;
	case STR:
		return ("string");
	default:
		/* more of an emergency measure .. */
		(void) sprintf(buf, "%d", ep->e_type);
		return (buf);
	}
	if (ep->e_mask) {
		(void) sprintf(buf, "%s&0x%lx", s, ep->e_mask);
		return (buf);
	} else
		return (s);
}

static char
op_to_name(char op)
{
	char c;

	switch (op & ~SUB) {

	case EQ:
	case STRC:
		c = '=';
		break;

	case GT:
		c = '>';
		break;

	case LT:
		c = '<';
		break;

	case ANY:
		c = 'x';
		break;

	case AND:
		c = '&';
		break;

	case NSET:
		c = '^';
		break;

	default:
		c = '?';
		break;
	}

	return (c);
}

void
prtmtab(void)
{
	Entry	*ep;

	FLOCKFILE(stdout);
	(void) mutex_lock(&mtab_lock);

	(void) printf("%-7s %-7s %-10s %-7s %-11s %s\n",
		"level", "off", "type", "opcode", "value", "string");
	for (ep = mtab; ep->e_off != -1L; ep++) {
		(void) printf("%-7d %-7ld %-10s %-7c ", ep->e_level, ep->e_off,
			type_to_name(ep), op_to_name(ep->e_opcode));
		if (ep->e_type == STR)
			showstr(ep->e_value.str, 10);
		else
			(void) printf("%-11lo", ep->e_value.num);
		(void) printf(" %s", ep->e_str);
		if (ep->e_opcode & SUB)
			(void) printf("\tsubst");
		(void) printf("\n");
	}
	(void) mutex_unlock(&mtab_lock);
	FUNLOCKFILE(stdout);
}
