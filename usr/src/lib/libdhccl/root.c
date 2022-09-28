/*
 * root.c: "Get a Portion of a Pathname".
 *
 * SYNOPSIS
 *    char *body(p, q)     "Get the body of a pathname".
 *    char *p
 *    char *q
 *
 *    char *extn(p)       "Get the extension of a pathname".
 *    char *p
 *
 *    char *head(p, q)     "Get the head of a pathname".
 *    char *p
 *    char *q
 *
 *    char *root(p, q)     "Get the root of a pathname".
 *    char *p
 *    char *q
 *
 *    char *tail(p)       "Get the tail of a pathname".
 *    char *p
 *
 * DESCRIPTION
 *    These functions return a pointer to the root, head, tail or extension of
 *    a pathname. Taking the pathnames indicated the functions return:
 *
 *              "/a/b.c/d.e.f"      "/a.b.c"        "a.b.c"     "../a.b/c.d.e"
 *
 *        root: "/a/b.c/d.e"        "/a.b"          "a.b"       "../a.b/c.d"
 *        head: "/a/b.c/"           "/"             ""          "../a.b/"
 *        tail: "d.e.f"             "a.b.c"         "a.b.c"     "c.d.e"
 *        body: "d.e"               "a.b"           "a.b"       "c.d"
 *        extn: ".f"                ".c"            ".c"        ".e"
 *
 *    The following logical relation exist (|| meaning concatenate):
 *        p       = root(p) || extn(p)
 *        p       = head(p) || tail(p)
 *        tail(p) = body(p) || extn(p)
 *
 *    If the pointer q is non-null it is taken to be the address of
 *    a character buffer in which the result is to be stored, otherwise
 *    root(), head(), and head() make calls to malloc() to allocate
 *    space for the new string; tail() and extn() always return
 *    pointers offset into the original string. In no case is the
 *    original string modified.
 *
 * ARGUMENTS
 *    p: a pointer to the pathname
 *    q: a pointer to a buffer to hold the result, or (char *)0 if the
 *       functions are to allocate a string dynamically.
 *
 * RETURNS
 *    Each function returns a pointer to datum calculated.
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)root.c 1.2 96/11/21 SMI"

#include <string.h>
#include "utils.h"

char *
root(char *p, char *r)
{
	register int j;
	register char *q;

	for (j = -1, q = p; *q; q++) {
		if (*q == '.')
			j = q - p;
		else if (*q == '/')
			j = -1;
	}
	if (j < 0) {
		if (!r)
			return (p);
		else {
			strcpy(r, p);
			return (r);
		}
	}
	if (!r)
		r = (char *)xmalloc(j + 1);
	strncpy(r, p, j);
	r[j] = 0;
	return (r);
}

char *
body(const char *p, char *r)
{
	const char *b, *e;
	int n;

	for (b = p; *p != 0; p++) {
		if (*p == '/')
			b = p + 1;
	}
	for (e = p; *p != '.' && p > b; p--)
		/* Null statement */;
	if (*p == '.')
		e = p;
	n = e - b;
	if (!r)
		r = (char *)xmalloc(n + 1);
	strncpy(r, b, n);
	r[n] = 0;
	return (r);
}

char *
head(char *p, char *r)
{
	register int i;
	register char *q;

	for (i = -1, q = p; *q; q++) {
		if (*q == '/')
			i = q - p;
	}
	if (i < 0) {
		if (!r)
			return (q);
		else {
			strcpy(r, q);
			return (r);
		}
	}

	i++;
	if (r)
		q = r;
	else
		q = (char *)xmalloc(i + 1);
	strncpy(q, p, i);
	q[i] = 0;
	return (q);
}

char *
tail(char *p)
{
	register char *q;

	for (q = p; *p; p++) {
		if (*p == '/')
			q = p + 1;
	}
	return (q);
}

char *
extn(char *p)
{
	register int i;
	register char *q;

	for (i = -1, q = p; *q; q++) {
		if (*q == '.')
			i = q - p;
		else if (*q == '/')
			i = -1;
	}
	if (i < 0)
		return (q);
	else
		return (p + i);
}
