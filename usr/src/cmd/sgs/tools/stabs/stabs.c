/*
 * Copyright (c) 1996-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)stabs.c	1.19	98/01/29 SMI"

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>

#include "stabs.h"

static struct tdesc *hash_table[BUCKETS];
static struct tdesc *name_table[BUCKETS];

/* XXX - need to replace */
static int hugenumber(char **pp, int end, int *bits);

static void reset(void);
static jmp_buf	resetbuf;

static char *get_line(void);
static void parseline(char *cp);
static char *soudef(char *cp, enum type type, struct tdesc **rtdp);
static void enumdef(char *cp, struct tdesc **rtdp);
static int compute_sum(char *w);
static struct tdesc *lookup(int h);

static char *number(char *cp, int *n);
static char *name(char *cp, char **w);
static char *id(char *cp, int *h);
static char *offsize(char *cp, struct mlist *mlp);
static char *whitesp(char *cp);
static void addhash(struct tdesc *tdp, int num);
static void tagadd(char *w, int h, struct tdesc *tdp);
static void tagdecl(char *cp, struct tdesc **rtdp, int h, char *w);
static char *tdefdecl(char *cp, int h, struct tdesc **rtdp);
static char *intrinsic(char *cp, struct tdesc **rtdp);
static char *arraydef(char *cp, struct tdesc **rtdp);

static int line_number = 0;
static int debug_line  = 0;
static char linebuf[MAXLINE];

extern int debug_level;

static void
debug(int level, char *cp, char *fmt, ...)
{
	va_list ap;
	char buf[1024];
	char tmp[32];
	int i;

	if (level > debug_level)
		return;

	if (cp != NULL) {
		for (i = 0; i < 30; i++) {
			if (cp[i] == '\0')
				break;
			if (!iscntrl(cp[i]))
				tmp[i] = cp[i];
		}
		tmp[i] = '\0';
		(void) sprintf(buf, "%s [cp='%s']\n", fmt, tmp);
	} else {
		strcpy(buf, fmt);
		strcat(buf, "\n");
	}

	va_start(ap, fmt);
	(void) vfprintf(stderr, buf, ap);
	va_end(ap);
}


/* Report unexpected syntax in stabs. */
static void
expected(
	char *who,	/* what function, or part thereof, is reporting */
	char *what,	/* what was expected */
	char *where)	/* where we were in the line of input */
{
	fprintf(stderr, "%s, input line %d: expecting \"%s\" at \"%s\"\n",
		who, line_number, what, where);
	exit(1);
}

/* Read a line from stdin into linebuf and increment line_number. */
static char *
get_line(void)
{
	char *cp = fgets(linebuf, MAXLINE, stdin);
	line_number++;

	/* For debugging, you can set debug_line to a line to stop on. */
	if (line_number == debug_line) {
		fprintf(stderr, "Hit debug line number %d\n", line_number);
		for (;;)
			sleep(1);
	}
	return (cp);
}

/* Get the continuation of the current input line. */
static char *
get_continuation(void)
{
	char *cp = get_line();
	if (!cp) {
		fprintf(stderr, "expecting continuation line, "
		    "got end of input\n");
		exit(1);
	}

	/* Skip to the quoted stuff. */
	while (*cp++ != '"')
		;
	return (cp);
}

void
parse_input(void)
{
	char *cp;
	int i = 0;

	for (i = 0; i < BUCKETS; i++) {
		hash_table[i] = NULL;
		name_table[i] = NULL;
	}

	/*
	 * get a line at a time from the .s stabs file and parse.
	 */
	while ((cp = get_line()) != NULL)
		parseline(cp);
}

/*
 * Parse each line of the .s file (stabs entry) gather meaningful information
 * like name of type, size, offsets of fields etc.
 */
static void
parseline(char *cp)
{
	struct tdesc *tdp;
	char c, *w;
	int h, tagdef;

	/*
	 * setup for reset()
	 */
	if (setjmp(resetbuf))
		return;

	/*
	 * Look for lines of the form
	 *	.stabs	"str",n,n,n,n
	 * The part in '"' is then parsed.
	 */
	cp = whitesp(cp);
#define	STLEN	6
	debug(2, cp, "parseline");
	if (strncmp(cp, ".stabs", STLEN) != 0)
		reset();
	cp += STLEN;
#undef STLEN
	cp = whitesp(cp);
	if (*cp++ != '"')
		reset();

	/*
	 * name:type		variable (ignored)
	 * name:ttype		typedef
	 * name:Ttype		struct tag define
	 */
	cp = whitesp(cp);
	cp = name(cp, &w);

	tagdef = 0;
	switch (c = *cp++) {
	case 't': /* type */
		break;
	case 'T': /* struct, union, enum */
		tagdef = 1;
		break;
	default:
		reset();
	}

	/*
	 * The type id and definition follow.
	 */
	cp = id(cp, &h);
	if (*cp == '"') {
		struct tdesc *ntdp;

		cp++;
		ntdp = lookup(h);
		if (ntdp == NULL) {  /* if that type isn't defined yet */
			if (*cp++ != '=')  /* better be defining it now */
				expected("parseline/'0-9'", "=", cp - 1);
			cp = tdefdecl(cp, h, &tdp);
			addhash(tdp, h); /* for *(x,y) types */
		} else { /* that type is already defined */
			tdp = malloc(sizeof (*tdp));
			tdp->type = TYPEOF;
			tdp->name = (w != NULL) ? strdup(w) : NULL;
			tdp->data.tdesc = ntdp;
			addhash(tdp, h); /* for *(x,y) types */
			debug(3, NULL, "    %s defined as %s(%d)", w,
			    (ntdp->name != NULL) ? ntdp->name : "anon", h);
		}
		return;
	} else if (*cp++ != '=') {
		expected("parseline", "=", cp - 1);
	}
	if (tagdef) {
		tagdecl(cp, &tdp, h, w);
	} else {
		tdefdecl(cp, h, &tdp);
		tagadd(w, h, tdp);
	}
}

/*
 * Check if we have this node in the hash table already
 */
static struct tdesc *
lookup(int h)
{
	int hash = HASH(h);
	struct tdesc *tdp = hash_table[hash];

	while (tdp != NULL) {
		if (tdp->id == h)
			return (tdp);
		tdp = tdp->hash;
	}
	return (NULL);
}

static char *
whitesp(char *cp)
{
	char *orig, c;

	orig = cp;
	for (c = *cp++; isspace(c); c = *cp++)
		;
	--cp;
	return (cp);
}

static char *
name(char *cp, char **w)
{
	char *new, *orig, c;
	int len;

	orig = cp;
	c = *cp++;
	if (c == ':')
		*w = NULL;
	else if (isalpha(c) || c == '_') {
		for (c = *cp++; isalnum(c) || c == ' ' || c == '_'; c = *cp++)
			;
		if (c != ':')
			reset();
		len = cp - orig;
		new = malloc(len);
		while (orig < cp - 1)
			*new++ = *orig++;
		*new = '\0';
		*w = new - (len - 1);
	} else
		reset();

	return (cp);
}

static char *
number(char *cp, int *n)
{
	char *next;

	*n = (int)strtol(cp, &next, 10);
	if (next == cp)
		expected("number", "<number>", cp);
	return (next);
}

static char *
id(char *cp, int *h)
{
	int n1, n2;

	if (*cp == '(') {	/* SunPro style */
		cp++;
		cp = number(cp, &n1);
		if (*cp++ != ',')
			expected("id", ",", cp - 1);
		cp = number(cp, &n2);
		if (*cp++ != ')')
			expected("id", ")", cp - 1);
		*h = n1 * 1000 + n2;
	} else if (isdigit(*cp)) { /* gcc style */
		cp = number(cp, &n1);
		*h = n1;
	} else {
		expected("id", "(/0-9", cp);
	}
	return (cp);
}

static void
tagadd(char *w, int h, struct tdesc *tdp)
{
	struct tdesc *otdp;

	tdp->name = w;
	if (!(otdp = lookup(h)))
		addhash(tdp, h);
	else if (otdp != tdp) {
		fprintf(stderr, "duplicate entry\n");
		fprintf(stderr, "old: %s %d %d %d\n",
		    otdp->name ? otdp->name : "NULL",
		    otdp->type, otdp->id / 1000, otdp->id % 1000);
		fprintf(stderr, "new: %s %d %d %d\n",
		    tdp->name ? tdp->name : "NULL",
		    tdp->type, tdp->id / 1000, tdp->id % 1000);
	}
}

static void
tagdecl(char *cp, struct tdesc **rtdp, int h, char *w)
{
	debug(1, NULL, "tagdecl: declaring '%s'", w ? w : "(anon)");
	if ((*rtdp = lookup(h)) != NULL) {
		if (w != NULL && strcmp((*rtdp)->name, w) != 0) {
			struct tdesc *tdp;

			tdp = malloc(sizeof (*tdp));
			tdp->name = strdup(w);
			tdp->type = TYPEOF;
			tdp->data.tdesc = *rtdp;
			addhash(tdp, h); /* for *(x,y) types */
			debug(3, NULL, "    %s defined as %s(%d)", w,
			    ((*rtdp)->name != NULL) ?
			    (*rtdp)->name : "anon", h);
		}
	} else {
		*rtdp = malloc(sizeof (**rtdp));
		(*rtdp)->name = w;
		addhash(*rtdp, h);
	}

	switch (*cp++) {
	case 's':
		soudef(cp, STRUCT, rtdp);
		break;
	case 'u':
		soudef(cp, UNION, rtdp);
		break;
	case 'e':
		enumdef(cp, rtdp);
		break;
	default:
		expected("tagdecl", "<tag type s/u/e>", cp - 1);
		break;
	}
}

static char *
tdefdecl(char *cp, int h, struct tdesc **rtdp)
{
	struct tdesc *ntdp;
	char *w;
	int c, h2;
	char type;

	debug(3, cp, "tdefdecl h=%d", h);

	/* Type codes */
	switch (type = *cp) {
	case 'b': /* integer */
		c = *++cp;
		if (c != 's' && c != 'u')
			expected("tdefdecl/b", "[su]", cp - 1);
		c = *++cp;
		if (c == 'c')
			cp++;
		cp = intrinsic(cp, rtdp);
		break;
	case 'R': /* fp */
		cp += 3;
		cp = intrinsic(cp, rtdp);
		break;
	case '(': /* equiv to another type */
		cp = id(cp, &h2);
		ntdp = lookup(h2);
		if (ntdp == NULL) {  /* if that type isn't defined yet */
			if (*cp++ != '=')  /* better be defining it now */
				expected("tdefdecl/'('", "=", cp - 1);
			cp = tdefdecl(cp, h2, rtdp);
			addhash(*rtdp, h2); /* for *(x,y) types */
		} else { /* that type is already defined */
			*rtdp = malloc(sizeof (**rtdp));
			(*rtdp)->type = TYPEOF;
			(*rtdp)->data.tdesc = ntdp;
		}
		break;
	case '*':
		ntdp = NULL;
		cp = tdefdecl(cp + 1, h, &ntdp);
		if (ntdp == NULL)
			expected("tdefdecl/*", "id", cp);

		*rtdp = malloc(sizeof (**rtdp));
		(*rtdp)->type = POINTER;
		(*rtdp)->size = model->pointersize;
		(*rtdp)->name = "pointer";
		(*rtdp)->data.tdesc = ntdp;
		break;
	case 'f':
		cp = tdefdecl(cp + 1, h, &ntdp);
		*rtdp = malloc(sizeof (**rtdp));
		(*rtdp)->type = FUNCTION;
		(*rtdp)->size = model->pointersize;
		(*rtdp)->name = "function";
		(*rtdp)->data.tdesc = ntdp;
		break;
	case 'a':
		cp++;
		if (*cp++ != 'r')
			expected("tdefdecl/a", "r", cp - 1);
		*rtdp = malloc(sizeof (**rtdp));
		(*rtdp)->type = ARRAY;
		(*rtdp)->name = "array";
		cp = arraydef(cp, rtdp);
		break;
	case 'x':
		c = *++cp;
		if (c != 's' && c != 'u' && c != 'e')
			expected("tdefdecl/x", "[sue]", cp - 1);
		cp = name(cp + 1, &w);
		*rtdp = malloc(sizeof (**rtdp));
		(*rtdp)->type = FORWARD;
		(*rtdp)->name = w;
		break;
	case 'B': /* volatile */
		cp = tdefdecl(cp + 1, h, &ntdp);
		*rtdp = malloc(sizeof (**rtdp));
		(*rtdp)->type = VOLATILE;
		(*rtdp)->size = 0;
		(*rtdp)->name = "volatile";
		(*rtdp)->data.tdesc = ntdp;
		break;
	case 'k': /* const */
		cp = tdefdecl(cp + 1, h, &ntdp);
		*rtdp = malloc(sizeof (**rtdp));
		(*rtdp)->type = CONST;
		(*rtdp)->size = 0;
		(*rtdp)->name = "const";
		(*rtdp)->data.tdesc = ntdp;
		break;
	case 'r': { /* range spec (gcc stabs) */
		int self_subrange;
		int nbits = 0;
		int n2, n3, n2bits, n3bits;

		cp = id(++cp, &h2);	/* type we are subrange of */
		self_subrange = (h2 == h);
		if (*cp++ != ';')
			expected("range", ";", cp - 1);
		n2 = hugenumber(&cp, ';', &n2bits);
		n3 = hugenumber(&cp, ';', &n3bits);
		debug(3, NULL, "range: n2=%d, n3=%d", n2, n3);
		if (n2bits == -1 || n3bits == -1)
			expected("range", "ranges", cp);
		if (n2bits != 0)
			nbits = n2bits;
		else if (n3bits != 0)
			nbits = n3bits;
		else {
			if (n3 == INT8_MAX || n3 == UINT8_MAX)
				nbits = 8;
			else if (n3 == INT32_MAX || n3 == UINT32_MAX)
				nbits = 32;
			else if (n3 == INT16_MAX || n3 == UINT16_MAX)
				nbits = 16;
			else if (n2 == 0 && n3 == -1)
				nbits = 32;
		}

		if (nbits == 0) {
			debug(1, NULL, "range: unexpected size");
			/* XXX - quit? */
		}
		*rtdp = malloc(sizeof (**rtdp));
		(*rtdp)->type = INTRINSIC;
		(*rtdp)->size = nbits / 8;
		(*rtdp)->name = NULL;
		(*rtdp)->data.tdesc = ntdp;
		debug(3, NULL, "Range-declared type: size=%d", nbits / 8);
		}
		break;
	case '0': case '1': case '2': case '3':	case '4':
	case '5': case '6': case '7': case '8': case '9':
		/* gcc equiv to another type */
		cp = id(cp, &h2);
		ntdp = lookup(h2);
		if (ntdp == NULL) {  /* if that type isn't defined yet */
			/* better be defining it now */
			if (*cp++ != '=') {
				if (h == h2) {
					/* defined in terms of itself */
					*rtdp = malloc(sizeof (**rtdp));
					(*rtdp)->type = INTRINSIC;
					(*rtdp)->name = "void";
					(*rtdp)->size = 0;
				} else {
					expected("tdefdecl/'0-9'", "=", cp - 1);
				}
			} else {
				cp = tdefdecl(cp, h2, rtdp);
				addhash(*rtdp, h2); /* for *(x,y) types */
			}
		} else { /* that type is already defined */
			*rtdp = malloc(sizeof (**rtdp));
			(*rtdp)->type = TYPEOF;
			(*rtdp)->data.tdesc = ntdp;
		}
		break;
	case 'u':
	case 's':
		cp++;

		*rtdp = malloc(sizeof (**rtdp));
		(*rtdp)->name = NULL;
		cp = soudef(cp, (type == 'u') ? UNION : STRUCT, rtdp);
		break;
	default:
		expected("tdefdecl", "<type code>", cp);
	}
	return (cp);
}

static char *
intrinsic(char *cp, struct tdesc **rtdp)
{
	struct tdesc *tdp;
	int size;

	cp = number(cp, &size);
	tdp = malloc(sizeof (*tdp));
	tdp->type = INTRINSIC;
	tdp->size = size;
	tdp->name = NULL;
	debug(3, NULL, "intrinsic: size=%ld", size);
	*rtdp = tdp;
	return (cp);
}

static char *
soudef(char *cp, enum type type, struct tdesc **rtdp)
{
	struct mlist *mlp, **prev;
	char *w;
	int h;
	int size;
	struct tdesc *tdp;

	cp = number(cp, &size);
	(*rtdp)->size = size;
	(*rtdp)->type = type; /* s or u */

	/*
	 * An '@' here indicates a bitmask follows.   This is so the
	 * compiler can pass information to debuggers about how structures
	 * are passed in the v9 world.  We don't need this information
	 * so we skip over it.
	 */
	if (cp[0] == '@')
		cp += 3;

	debug(3, cp, "soudef: %s size=%d",
	    (*rtdp)->name ? (*rtdp)->name : "(anonsou)",
	    (*rtdp)->size);

	prev = &((*rtdp)->data.members);
	/* now fill up the fields */
	while ((*cp != '"') && (*cp != ';')) { /* signifies end of fields */
		mlp = malloc(sizeof (*mlp));
		*prev = mlp;
		cp = name(cp, &w);
		mlp->name = w;
		cp = id(cp, &h);
		/*
		 * find the tdesc struct in the hash table for this type
		 * and stick a ptr in here
		 */
		tdp = lookup(h);
		if (tdp == NULL) { /* not in hash list */
			debug(3, NULL, "      defines %s (%d)", w, h);
			if (*cp++ != '=')
				expected("soudef", "=", cp - 1);
			cp = tdefdecl(cp, h, &tdp);
			addhash(tdp, h);
			debug(4, cp, "     soudef now looking at    ");
			cp++;

		} else {
			debug(3, NULL, "      refers to %s (%d, %s)",
			    w ? w : "anon", h, tdp->name ? tdp->name : "anon");
		}

		mlp->fdesc = tdp;
		cp = offsize(cp, mlp);
		/* cp is now pointing to next field */
		prev = &mlp->next;
		/* could be a continuation */
		if (*cp == '\\')
			cp = get_continuation();
	}
	return (cp);
}

static char *
offsize(char *cp, struct mlist *mlp)
{
	int offset, size;

	if (*cp == ',')
		cp++;
	cp = number(cp, &offset);
	if (*cp++ != ',')
		expected("offsize/2", ",", cp - 1);
	cp = number(cp, &size);
	if (*cp++ != ';')
		expected("offsize/3", ";", cp - 1);
	mlp->offset = offset;
	mlp->size = size;
	return (cp);
}

static char *
arraydef(char *cp, struct tdesc **rtdp)
{
	int h;
	int start, end;

	cp = id(cp, &h);
	if (*cp++ != ';')
		expected("arraydef/1", ";", cp - 1);

	(*rtdp)->data.ardef = malloc(sizeof (struct ardef));
	(*rtdp)->data.ardef->indices = malloc(sizeof (struct element));
	(*rtdp)->data.ardef->indices->index_type = lookup(h);

	cp = number(cp, &start); /* lower */
	if (*cp++ != ';')
		expected("arraydef/2", ";", cp - 1);
	cp = number(cp, &end);	/* upper */
	if (*cp++ != ';')
		expected("arraydef/3", ";", cp - 1);
	(*rtdp)->data.ardef->indices->range_start = start;
	(*rtdp)->data.ardef->indices->range_end = end;
#if 0
	if (isdigit(*cp)) {
		cp = number(cp, &contents_type); /* lower */
		tdp = lookup(contents_type);
		if (tdp != NULL) {
			(*rtdp)->data.ardef->contents = tdp;
		} else {
			if (*cp != '=')
				expected("arraydef/4", "=", cp);
			cp = tdefdecl(cp + 1, h, &tdp);
			addhash(tdp, h); /* for *(x,y) types */
			(*rtdp)->data.ardef->contents = tdp;
		}
	} /* else  */
#endif
	cp = tdefdecl(cp, h, &((*rtdp)->data.ardef->contents));
	return (cp);
}

static void
enumdef(char *cp, struct tdesc **rtdp)
{
	struct elist *elp, **prev;
	char *w;

	(*rtdp)->type = ENUM;
	(*rtdp)->data.emem = NULL;

	prev = &((*rtdp)->data.emem);
	while (*cp != ';') {
		elp = malloc(sizeof (*elp));
		elp->next = NULL;
		*prev = elp;
		cp = name(cp, &w);
		elp->name = w;
		cp = number(cp, &elp->number);
		debug(3, NULL, "enum %s: %s=%ld",
		    (*rtdp)->name ? (*rtdp)->name : "(anon enum)",
		    elp->name, elp->number);
		prev = &elp->next;
		if (*cp++ != ',')
			expected("enumdef", ",", cp - 1);
		if (*cp == '\\')
			cp = get_continuation();
	}
}

/*
 * Add a node to the hash queues.
 */
static void
addhash(struct tdesc *tdp, int num)
{
	int hash = HASH(num);

	tdp->id = num;
	tdp->hash = hash_table[hash];
	hash_table[hash] = tdp;

	if (tdp->name) {
		hash = compute_sum(tdp->name);
		tdp->next = name_table[hash];
		name_table[hash] = tdp;
	}
}

struct tdesc *
lookupname(char *name)
{
	int hash = compute_sum(name);
	struct tdesc *tdp, *ttdp = NULL;

	for (tdp = name_table[hash]; tdp != NULL; tdp = tdp->next) {
		if (tdp->name != NULL && strcmp(tdp->name, name) == 0) {
			if (tdp->type == STRUCT || tdp->type == UNION ||
			    tdp->type == ENUM || tdp->type == INTRINSIC)
				return (tdp);
			if (tdp->type == TYPEOF)
				ttdp = tdp;
		}
	}
	return (ttdp);
}

static int
compute_sum(char *w)
{
	char c;
	int sum;

	for (sum = 0; (c = *w) != '\0'; sum += c, w++)
		;
	return (HASH(sum));
}

static void
reset(void)
{
	longjmp(resetbuf, 1);
	/* NOTREACHED */
}


/* XXX XXX XXX - need to replace, this is copylefted */

/*
 * Read a number from the string pointed to by *pp.  The value of *pp is
 * advanced over the number.  If END is nonzero, the character that ends
 * the number must match END, or an error happens; and that character is
 * skipped if it does match.  If END is zero, *PP is left pointing to
 * that character.
 *
 * If the number fits in an int, set *BITS to 0 and return the value.
 * If not, set *BITS to be the number of bits in the number and return 0.
 *
 * If encounter garbage, set *BITS to -1 and return 0.
 */
static int
hugenumber(char **pp, int end, int *bits)
{
	char *p = *pp;
	int sign = 1;
	int n = 0;
	int radix = 10;
	char overflow = 0;
	int nbits = 0;
	int c;
	int upper_limit;

	if (*p == '-') {
		sign = -1;
		p++;
	}

	/*
	 * Leading zero means octal.  GCC uses this to output values
	 * larger than an int (because that would be hard in decimal).
	 */
	if (*p == '0') {
		radix = 8;
		p++;
	}

	upper_limit = INT32_MAX / radix;

	while ((c = *p++) >= '0' && c < ('0' + radix)) {
		if (n <= upper_limit) {
			n *= radix;
			n += c - '0';	/* FIXME this overflows anyway */
		} else {
			overflow = 1;
		}

		/*
		 * This depends on large values being output in octal, which is
		 * what GCC does.
		 */
		if (radix == 8) {
			if (nbits == 0) {
				if (c == '0')
					/* Ignore leading zeroes.  */
					;
				else if (c == '1')
					nbits = 1;
				else if (c == '2' || c == '3')
					nbits = 2;
				else
					nbits = 3;
			} else {
				nbits += 3;
			}
		}
	}
	if (end) {
		if (c && c != end) {
			if (bits != NULL)
				*bits = -1;
			return (0);
		}
	} else {
		--p;
	}

	*pp = p;
	if (overflow) {
		if (nbits == 0) {
			/*
			 * Large decimal constants are an error
			 * (because it is hard to count how many
			 * bits are in them).
			 */
			if (bits != NULL)
				*bits = -1;
			return (0);
		}

		/*
		 * -0x7f is the same as 0x80.  So deal with it by
		 * adding one to the number of bits.
		 */
		if (sign == -1)
			++nbits;
		if (bits)
			*bits = nbits;
	} else {
		if (bits)
			*bits = 0;
		return (n * sign);
	}
	/* It's *BITS which has the interesting information.  */
	return (0);
}
