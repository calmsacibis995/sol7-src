#include <stdio.h>
#pragma ident   "@(#)regerror.c 1.2     97/03/07 SMI"
void
tpt_regerror(s)
char *s;
{
#ifdef ERRAVAIL
	error("tpt_regexp: %s", s);
#else
	fprintf(stderr, "tpt_regexp(3): %s", s);
	exit(1);
#endif
	/* NOTREACHED */
}
