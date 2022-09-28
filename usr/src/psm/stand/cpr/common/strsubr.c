/*
 * Copyright (c) 1994-1997, by Sun Microsystems Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)strsubr.c	1.11	97/12/03 SMI"

#include <sys/types.h>
#include <sys/mkdev.h>

/*
 * Miscellaneous routines used by the standalones.
 */

size_t
strlen(const char *s)
{
	size_t n;

	n = 0;
	while (*s++)
		n++;
	return (n);
}


char *
strcat(char *s1, const char *s2)
{
	char *os1 = s1;

	while (*s1++)
		;
	--s1;
	while (*s1++ = *s2++)
		;
	return (os1);
}

char *
strcpy(char *s1, const char *s2)
{
	char *os1;

	os1 = s1;
	while (*s1++ = *s2++)
		;
	return (os1);
}

/*
 * Compare strings:  s1>s2: >0  s1==s2: 0  s1<s2: <0
 */
int
strcmp(const char *s1, const char *s2)
{
	while (*s1 == *s2++)
		if (*s1++ == '\0')
			return (0);
	return (*(unsigned char *)s1 - *(unsigned char *)--s2);
}

int
strncmp(const char *s1, const char *s2, size_t n)
{
	if (s1 == s2)
		return (0);
	n++;
	while (--n != 0 && *s1 == *s2++)
		if (*s1++ == '\0')
			return (0);
	return (n == 0 ? 0 : *(unsigned char *)s1 - *(unsigned char *)--s2);
}

void
bcopy(const void *s, void *d, size_t count)
{
	const char *src = s;
	char *dest = d;

	if (src < dest && (src + count) > dest) {
		/* overlap copy */
		while (--count != -1)
			*(dest + count) = *(src + count);
	} else {
		while (--count != -1)
			*dest++ = *src++;
	}
}
