/*
 *	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T
 *	All Rights Reserved
 *
 *	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T
 *	The copyright notice above does not evidence any
 *	actual or intended publication of such source code.
 *
 *	Portions Copyright (c) 1988, 1991 - 1994, Sun Microsystems, Inc
 *	All Rights Reserved.
 */

/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma	ident	"@(#)sort.c	1.28	97/10/01	 SMI"

#include "sort.h"

static FILE		*output_fp;
static char		*tmp_dir = NULL;
static char		*filep;		/* pointer to suffix of tmp file name */
static char		*file;		/* tmp file name */
static int		nfiles;		/* num input files & extra tmp files */
static int		*maxbrk;	/* max breakpt of data segment */
static int		*lspace;	/* initial breakpt of data segment */
static unsigned int	alloc;		/* used to extend the data segment */
static unsigned int	tryfor = DEFMEM; /* initial amount of core memory */

/*
 * Use setbuf's to avoid malloc calls. malloc seems to get heartburn
 * when brk returns storage.
 */
static char	bufin[BUFSIZ];
static char	bufout[BUFSIZ];

static int	nway;
static int	maxrec;
static int 	mflg = 0;		/* -m: merge sorted input files */
static int	cflg = 0;		/* -c: check order of input file */
static int	uflg = 0;		/* -u: suppress duplicate lines */
static char	*outfil = NULL;		/* output file for sorted data */
static int	unsafeout = 0;		/* kludge to assure -m -o works */
static wchar_t	tabchar = NULL;		/* field separator */
static int 	eargc = 0;		/* count of number of input files */
static char	**eargv;		/* array of input file names */
static int	wasfirst = 0;
static int	notfirst = 0;
static int	bonus;
static wchar_t	*save;
static wchar_t	*lines[2];
static int	save_alloc;
static char	*mem_error = "sort:  allocation error, out of memory\n";

static int (*compare)(wchar_t *, wchar_t *) = cmpa;

static struct field	*fields;
static struct btree	tree[TREEZ];
static struct btree	*treep[TREEZ];

static struct field proto = {
	nofold,		/* code() */
	zero,		/* ignore() */
	ASC,		/* fcmp */
	1,		/* rflg */
	0,		/* bflg[0] */
	0,		/* bflg[1] */
	0,		/* m[0] */
	-1,		/* m[1] */
	0,		/* n[0] */
	0		/* n[1] */
};

static int	cur_num_fields = 0;
static int 	error = 2;

static int	not_c_locale;		/* true if LC_COLLATE is not C locale */
static int	modflg = 0;		/* true if -d, -i or -f is set */
static int	collsize = 0;
static wchar_t	*collb1;
static wchar_t	*collb2;

static wchar_t	*months[12];
static wchar_t	decpnt;			/* decimal point */
static wchar_t	mon_decpnt;		/* decimal point for monetary */
static wchar_t	thousands_sep;		/* thousands separator */
static wchar_t	mon_thousands_sep;	/* thousands separator for monetary */

static struct tm ct = {
	0,			/* tm_sec */
	0,			/* tm_min */
	0,			/* tm_hour */
	1,			/* tm_mday */
	0,			/* tm_mon */
	0,			/* tm_year */
	0,			/* tm_wday */
	0,			/* tm_yday */
	0			/* tm_isdst */
};


int
main(int argc, char **argv)
{
	int		a;
	int		i;
	int		output_fileno;
	int		num_fields_alloc;
	int		oldmaxrec;
	char		*arg;		/* argv being processed */
	char		*tabarg;	/* argument for -t: field separator */
	char		*file1;		/* name of tmp file */
	struct field	*p;
	struct field	*q;
	unsigned int	maxalloc;
	unsigned int	newalloc;

	/* Take locale values from environment variables */
	(void) setlocale(LC_ALL, "");
	(void) textdomain(TEXT_DOMAIN);
	not_c_locale = strcmp("C", setlocale(LC_COLLATE, NULL));

	if ((fields = (struct field *)malloc(INIT_NUM_FIELDS *
	    sizeof (struct field))) == NULL) {
		(void) fprintf(stderr, gettext(mem_error));
		term();
	}
	num_fields_alloc = INIT_NUM_FIELDS;
	fields[cur_num_fields] = proto;

	initree();
	initdecpnt();

	/* Allow eargv to overwrite argv */
	eargv = argv;
	while (--argc > 0) {
		if (**++argv == '-') {
			arg = *argv;
			switch (*++arg) {
			case '\0':
				if (arg[-1] == '-') {
					eargv[eargc++] = "-";
				}
				break;

			case 'o':
				if (*(arg + 1) != '\0') {
					outfil = arg + 1;
				} else {
					outfil = get_subopt(argc, argv, 'o');
					argc--;
					argv++;
				}
				break;

			case 'k':
				if (++cur_num_fields >= num_fields_alloc) {
					if ((fields = (struct field *)
					    realloc(fields, (num_fields_alloc +
					    INIT_NUM_FIELDS) *
					    sizeof (struct field))) == NULL) {
						(void) fprintf(stderr,
						    gettext(mem_error));
						return (2);
					}
					num_fields_alloc += INIT_NUM_FIELDS;
				}
				fields[cur_num_fields] = proto;
				(void) field(get_subopt(argc, argv, 'k'),
				    0, 1, cur_num_fields);
				argc--;
				argv++;
				break;

			case 'T':
				if (--argc > 0) {
					tmp_dir = *++argv;
				}
				break;

			case 't':
				/* treat field separators as white space */
				fields[0].bflg[0]++;
				compare = cmp;
				if (*(arg + 1) == '\0') {
					tabarg = get_subopt(argc, argv, 't');
					if (tabarg[1] != '\0') {
						usage();
					}
					(void) mbtowc(&tabchar, tabarg,
					    MB_CUR_MAX);
					argc--;
					argv++;
				} else {
					(void) mbtowc(&tabchar, arg + 1,
					    MB_CUR_MAX);
				}
				break;

			default:
				++*argv;
				if (isdigit((int)**argv)) {
					/* process -pos option */
					if (field(*argv, 1, 0,
					    cur_num_fields) < 0) {
						fields[cur_num_fields--] =
						    proto;
					}
				} else {
					/*
					 * Process all options that overide
					 * default ordering of keys.
					 * Options may preceed or follow the
					 * key defintions.
					 */
					(void) field(*argv, 0, 0, 0);
				}
				break;
			}
		} else if (**argv == '+') {
			if (++cur_num_fields >= num_fields_alloc) {
				if ((fields = (struct field *)
				    realloc(fields, (num_fields_alloc +
				    INIT_NUM_FIELDS) *
				    sizeof (struct field))) == NULL) {
					(void) fprintf(stderr,
					    gettext(mem_error));
					return (2);
				}
				num_fields_alloc += INIT_NUM_FIELDS;
			}
			fields[cur_num_fields] = proto;
			(void) field(++*argv, 0, 0, cur_num_fields);
		} else {
			/* Save filename */
			eargv[eargc++] = *argv;
		}
	}	/* while (--argc > 0) */

	q = &fields[0];

	for (a = 1; a <= cur_num_fields; a++) {
		p = &fields[a];
		if (p->code != proto.code) {
			continue;
		}
		if (p->ignore != proto.ignore) {
			continue;
		}
		if (p->fcmp != proto.fcmp) {
			continue;
		}
		if (p->rflg != proto.rflg) {
			continue;
		}
		if (p->bflg[0] != proto.bflg[0]) {
			continue;
		}
		if (p->bflg[1] != proto.bflg[1]) {
			continue;
		}
		p->code = q->code;
		p->ignore = q->ignore;
		p->fcmp = q->fcmp;
		p->rflg = q->rflg;
		p->bflg[0] = p->bflg[1] = q->bflg[0];
	}

	if (eargc == 0) {
		eargv[eargc++] = "-";
	}
	if (cflg && eargc > 1) {
		(void) fprintf(stderr,
		    gettext("sort: can check only 1 file\n"));
		return (2);
	}

	safeoutfil();

	/*
	 * Get current break value and maximum break value (address of
	 * first location beyond end of data segment)
	 */
	lspace = (int *)sbrk(0);
	maxbrk = (int *)ulimit(UL_GMEMLIM, 0L);
	if (!mflg && !cflg) {
		if ((alloc = grow_core(tryfor, (unsigned int)0)) == 0) {
			(void) fprintf(stderr, gettext(
			    "sort: allocation error before sort\n"));
			return (2);
		}
	}

	if ((filep = tempnam(tmp_dir, "stm")) == NULL) {
		(void) fprintf(stderr, gettext(
		    "sort: allocation error on temp name\n"));
		return (2);
	}

	/* add the suffix ".xxxxxxxx", used to keep count of files */
	/* 10 = strlen(".xxxxxxxx") + 1 */
	if ((file1 = (char *)malloc(strlen(filep) + 10)) == NULL) {
		(void) fprintf(stderr, gettext(mem_error));
		return (2);
	}

	(void) strcpy(file1, filep);
	(void) strcat(file1, ".00000000");
	free(filep);

	/* set filep to point to beginning of suffix */
	filep = file1;
	while (*filep) {
		filep++;
	}
	filep -= 9;
	file = file1;
	output_fileno = creat(file, 0600);

	if (output_fileno < 0) {
		diag1(gettext("sort: can't locate temp: "), errno);
		return (2);
	}
	(void) close(output_fileno);
	(void) unlink(file);

	if (sigset(SIGHUP, SIG_IGN) != SIG_IGN) {
		(void) sigset(SIGHUP, (void (*)(int))term);
	}
	if (sigset(SIGINT, SIG_IGN) != SIG_IGN) {
		(void) sigset(SIGINT, (void (*)(int))term);
	}
	(void) sigset(SIGPIPE, (void (*)(int))term);
	if (sigset(SIGTERM, SIG_IGN) != SIG_IGN) {
		(void) sigset(SIGTERM, (void (*)(int))term);
	}
	nfiles = eargc;

	if (cflg) {
		checksort();
		return (0);
	}

	/* only executed when -c is not used */
	maxrec = 0;
	if (!mflg) {
		if (not_c_locale && modflg) {
			collsize = INIT_COLL_LEN;
			if ((collb1 = (wchar_t *)
			    malloc(collsize * sizeof (wchar_t) * 2)) == NULL) {
				(void) fprintf(stderr, gettext(mem_error));
				return (2);
			}
		}

		sort();
		if (ferror(stdin)) {
			rderror(NULL);
		}
		(void) fclose(stdin);
	}

	if (maxrec == 0) {
		/* sorting phase is skipped */
		maxrec = INIT_MAXREC;
		if (not_c_locale && modflg) {
			/*
			 * collbuf is not allocated because sorting phase is
			 * skipped otherwise collbuf is allocated
			 * If LC_COLLATE == C, no collation buffers are needed
			 */
			if ((collb1 = (wchar_t *)malloc(maxrec *
			    sizeof (wchar_t) * 2)) == NULL) {
				(void) fprintf(stderr, gettext(mem_error));
				return (2);
			}
			collb2 = collb1 + maxrec;
		}
	}
	alloc = (N + 1) * (maxrec * sizeof (wchar_t)) + N * BUFSIZ;
	maxalloc = (maxbrk - lspace) * sizeof (int *);

	for (nway = N; nway >= 2; --nway) {
		if (alloc < maxalloc) {
			break;
		}
		alloc -= maxrec * sizeof (wchar_t) + BUFSIZ;
	}

	if (nway < 2 || brk((char *)lspace + alloc) != 0) {
		(void) fprintf(stderr, gettext(
		    "sort: allocation error before merge\n"));
		term();
	}

	wasfirst = notfirst = 0;
	oldmaxrec = maxrec;
	a = ((mflg != 0) ? 0 : eargc);

	/* Do leftovers early */
	if ((i = nfiles - a) > nway) {
		if ((i %= (nway - 1)) == 0) {
			i = nway - 1;
		}
		if (i != 1)  {
			newfile();
			setbuf(output_fp, bufout);
			merge(a, a + i);
			a += i;
		}
	}

	for (; (a + nway) < nfiles || unsafeout && (a < eargc); a = i) {
		i = a + nway;
		if (i >= nfiles) {
			i = nfiles;
		}
		newfile();
		setbuf(output_fp, bufout);
		if (oldmaxrec < maxrec) {
			newalloc = (nway + 1) * maxrec * sizeof (wchar_t);
			if (newalloc <= maxalloc) {
				alloc = newalloc;
				(void) brk((char *)lspace + alloc);
			}
			oldmaxrec = maxrec;
		}
		merge(a, i);
	}
	if (a != nfiles) {
		oldfile();
		setbuf(output_fp, bufout);
		if (oldmaxrec < maxrec) {
			newalloc = (nway + 1) * maxrec * sizeof (wchar_t);
			if (newalloc <= maxalloc) {
				alloc = newalloc;
				(void) brk((char *)lspace + alloc);
			}
			/* oldmaxrec = maxrec; */
		}
		merge(a, nfiles);
	}
	error = 0;
	term();
	/*NOTREACHED*/
}

static
void
sort(void)
{
	wchar_t		*cp;
	wchar_t		**lp;
	FILE		*iop;
	wchar_t		*keep = 0;
	wchar_t		*ekeep = 0;
	wchar_t		**mp;
	wchar_t		**lmp;
	wchar_t		**ep;
	int		len;
	int		done = 0;
	int		i = 0;		/* input file number 0 < i < eargc */
	int		first = 1;
	char		*f;

	/*
	 * Records are read in from the front of the buffer area.
	 * Pointers to the records are allocated from the back of the buffer.
	 * If a partially read record exhausts the buffer, it is saved and
	 * then copied to the start of the buffer for processing with the
	 * next coreload.
	 */
	ep = (wchar_t **)(((char *)lspace) + alloc);

	/* open first file */
	if ((f = setfil(i++)) == NULL) {
		iop = stdin;
	} else if ((iop = fopen(f, "r")) == NULL) {
		cant(f);
	}

	setbuf(iop, bufin);

	do {
		lp = ep - 1;
		cp = (wchar_t *)lspace;
		*lp-- = cp;
		/* move record from previous coreload */
		if (keep != 0) {
			for (; keep < ekeep; *cp++ = *keep++)
				/*LINTED [E_LOOP_EMPTY]*/
				;
		}
		while ((wchar_t *)lp - cp > 1) {
			if (fgetws(cp, (wchar_t *)lp - cp, iop) == NULL) {
				len = 0;
			} else {
				len = wslen(cp);
			}
			if (len == 0) {
				if (ferror(iop)) {
					rderror(f);
				}

				if (keep == 0) {
					if (i < eargc) {
						(void) fclose(iop);
						if ((f = setfil(i++)) == NULL) {
							iop = stdin;
						} else if ((iop = fopen(f, "r"))
						    == NULL) {
							cant(f);
						}
						setbuf(iop, bufin);
						continue;
					} else {
						done++;
						break;
					}
				}
			}
			cp += len - 1;
			if (*cp == L'\n') {
				cp += 2;
				if (cp - *(lp+1) > maxrec) {
					maxrec = cp - *(lp + 1);
					if (collsize != 0 &&
					    collsize < maxrec) {
						free(collb1);
						collsize = maxrec +
						    INIT_COLL_LEN;
						if ((collb1 = (wchar_t *)
						    malloc(collsize *
						    sizeof (wchar_t) * 2)) ==
						    NULL) {
							(void) fprintf(stderr,
							    gettext(mem_error));
							term();
						}
					}
				}
				*lp-- = cp;
				keep = 0;
			} else if (cp + 2 < (wchar_t *)lp) {
				/* the last record of the input */
				/* file is missing a NEWLINE    */
				if (f == NULL) {
					newline_warning();
				} else {
					(void) fprintf(stderr, gettext(
					    "sort: warning: missing NEWLINE "
					    "added at end of input file %s\n"),
					    f);
				}
				*++cp = L'\n';
				*++cp = 0;
				*lp-- = ++cp;
				keep = 0;
			} else {  /* the buffer is full */
				keep = *(lp+1);
				ekeep = ++cp;
			}

			if ((wchar_t *)lp - cp <= 2 && first == 1) {
				/* full buffer */
				tryfor = alloc;
				tryfor = grow_core(tryfor, alloc);

				/* could not grow */
				if (tryfor == 0) {
					first = 0;
				} else {
					/* move pointers */
					lmp = ep +
					    (tryfor / sizeof (wchar_t **) - 1);
					mp = ep - 1;
					while (mp > lp) {
						*lmp-- = *mp--;
					}
					ep += tryfor/sizeof (wchar_t **);
					lp += tryfor/sizeof (wchar_t **);
					alloc += tryfor;
				}
			}
		} /* while ((wchar_t *)lp - cp > 1) */
		if (keep != 0 && *(lp + 1) == (wchar_t *)lspace) {
			(void) fprintf(stderr, gettext(
			    "sort: fatal: record too large\n"));
			term();
		}
		first = 0;
		lp += 2;
		if (done == 0 || nfiles != eargc) {
			newfile();
		} else {
			oldfile();
		}
		setbuf(output_fp, bufout);
		collb2 = collb1 + collsize;
		msort(lp, ep);
		if (ferror(output_fp)) {
			wterror(gettext("sort: write error while sorting: "));
		}
		(void) fclose(output_fp);
	} while (done == 0);
}


static
void
msort(wchar_t **a, wchar_t **b)
{
	struct btree	**tp;
	int		i;
	int		j;
	int		n;
	wchar_t		*save;
	int		blkcnt[TREEZ];
	wchar_t		**blkcur[TREEZ];

	i = (b - a);
	if (i < 1) {
		return;
	} else if (i == 1) {
		write_line(*a);
		return;
	} else if (i >= TREEZ) {
		n = TREEZ;		/* number of blocks of records */
	} else {
		n = i;
	}

	/* break into n sorted subgroups of approximately equal size */
	tp = &(treep[0]);
	j = 0;
	do {
		(*tp++)->rn = j;
		b = a + (blkcnt[j] = i / n);
		qksort(a, b);
		blkcur[j] = a = b;
		i -= blkcnt[j++];
	} while (--n > 0);
	n = j;

	/* make a sorted binary tree using the first record in each group */
	i = 0;
	while (i < n) {
		(*--tp)->rp = *(--blkcur[--j]);
		insert(tp, ++i);
	}
	wasfirst = notfirst = 0;
	bonus = cmpsave(n);

	j = uflg;
	tp = &(treep[0]);
	while (n > 0)  {
		write_line((*tp)->rp);
		if (j) {
			save = (*tp)->rp;
		}

		/* Get another record and insert.  Bypass repeats if uflg */
		do {
			i = (*tp)->rn;
			if (j) {
				while ((blkcnt[i] > 1) &&
				    (**(blkcur[i]-1) == '\0')) {
					--blkcnt[i];
					--blkcur[i];
				}
			}
			if (--blkcnt[i] > 0) {
				(*tp)->rp = *(--blkcur[i]);
				insert(tp, n);
			} else {
				if (--n <= 0) {
					break;
				}
				bonus = cmpsave(n);
				tp++;
			}
		} while (j && (*compare)((*tp)->rp, save) == 0);
	}
}


/*
 * Insert the element at tp[0] into its proper place in the array of size n
 * Pretty much Algorith B from 6.2.1 of Knuth, Sorting and Searching
 * Special case for data that appears to be in correct order
 */

static
void
insert(struct btree **tp, int n)
{
	struct btree	**lop;
	struct btree	**hip;
	struct btree	**midp;
	int		c;
	struct btree	*hold;

	midp = lop = tp;
	hip = lop++ + (n - 1);
	if ((wasfirst > notfirst) && (n > 2) &&
	    ((*compare)((*tp)->rp, (*lop)->rp) >= 0)) {
		wasfirst += bonus;
		return;
	}
	while ((c = hip - lop) >= 0) {
		/* leave midp at the one tp is in front of */
		midp = lop + c / 2;
		if ((c = (*compare)((*tp)->rp, (*midp)->rp)) == 0) {
			break;			/* match */
		}
		if (c < 0) {
			lop = ++midp;		/* c < 0 => tp > midp */
		} else {
			hip = midp - 1;		/* c > 0 => tp < midp */
		}
	}
	c = midp - tp;

	/* number of moves to get tp just before midp */
	if (--c > 0) {
		hip = tp;
		lop = hip++;
		hold = *lop;
		do {
			*lop++ = *hip++;
		} while (--c > 0);
		*lop = hold;
		notfirst++;
	} else {
		wasfirst += bonus;
	}
}



/*
 * Memory allocation policy:
 *
 *	lspace == buffer ---->  +START OF HEAP------------------+
 *	tfile[0] (FILE *)	| IO buffer for file 0		|
 *	tfile[1] (FILE *)	| IO buffer for file 1		|
 *	  ...			.				.
 *	tfile[i] (FILE *) uses->| IO buffer (BUFSIZE bytes)	|
 *	  ...			.				.
 *	tfile[N-1] (FILE *)	|				|
 *			save -->|-------------------------------|
 *		                |<maxrec> characters for temp?  |
 *				|-------------------------------|
 *				|<maxrec> characters for file 0	|
 *				|<maxrec> characters for file 1	|
 *	treep[0]		.                              	.
 *	treep[1]		.				.
 *	  ...	       /------->|<maxrec> characters for file i	|
 *	treep[i].rp --/		.				.
 *	        .rn == i (?)	.				.
 *	  ...			.				.
 *	treep[N-1]		|-------------------------------|
 *
 */
static
void
merge(int a, int b)
{
	FILE		*tfile[N];
	wchar_t		*buffer;
	int		nf;		/* number of merge files */
	struct btree	**tp;
	int		i;		/* input file number */
	int		j;
	char		*f;
	char		*iobuf;
	struct btree	*bptr;

	iobuf = (char *) lspace;
	save = (wchar_t *)((char *)lspace + nway * BUFSIZ);
	save_alloc = 0;
	buffer = save + maxrec;
	tp = &(treep[0]);

	for (nf = 0, i = a; i < b; i++)  {
		f = setfil(i);
		if (f == 0) {
			tfile[nf] = stdin;
		} else if ((tfile[nf] = fopen(f, "r")) == NULL) {
			cant(f);
		}
		bptr = *tp;
		bptr->rn = nf;
		bptr->recsz = maxrec;
		if ((char *)(buffer + maxrec) > ((char *)lspace + alloc)) {
				if (bptr->allocflag) {
					free(bptr->rp);
				}
				if ((bptr->rp = (wchar_t *)malloc
				    (maxrec * sizeof (wchar_t))) == NULL) {
					(void) fprintf(stderr,
					    gettext(mem_error));
					term();
				}
				bptr->allocflag = 1;
		} else {
			if (bptr->allocflag) {
				free(bptr->rp);
			}
			bptr->rp = buffer;
			bptr->allocflag = 0;
		}
		buffer += maxrec;
		setbuf(tfile[nf], iobuf);
		iobuf += BUFSIZ;
		if (xrline(tfile[nf], (*tp)) == 0) {
			nf++;
			tp++;
		} else {
			if (ferror(tfile[nf])) {
				rderror(f);
			}
			(void) fclose(tfile[nf]);
		}
	}


	/* make a sorted btree from the first record of each file */
	--tp;
	i = 1;
	while (i++ < nf) {
		insert(--tp, i);
	}

	bonus = cmpsave(nf);
	tp = &(treep[0]);
	j = uflg;
	while (nf > 0) {
		write_line((*tp)->rp);
		if (j) {
			copy_line(save, (*tp)->rp);
		}

		/* Get another record and insert.  Bypass repeats if uflg */
		do {
			i = (*tp)->rn;
			if (xrline(tfile[i], (*tp))) {
				if (ferror(tfile[i])) {
					rderror(setfil(i+a));
				}
				(void) fclose(tfile[i]);
				if (--nf <= 0) {
					break;
				}
				++tp;
				bonus = cmpsave(nf);
			} else {
				insert(tp, nf);
			}
		} while (j && (*compare)((*tp)->rp, save) == 0);
	}


	for (i = a; i < b; i++) {
		if (i >= eargc) {
			(void) unlink(setfil(i));
		}
	}
	if (ferror(output_fp)) {
		wterror(gettext("sort: write error while merging: "));
	}
	(void) fclose(output_fp);
}

/* Copy the string in fp to tp */
static
void
copy_line(wchar_t *tp, wchar_t *fp)
{
	while ((*tp++ = *fp++) != L'\0')
		/*LINTED [E_LOOP_EMPTY]*/
		;
}

static
int
xrline(FILE *iop, struct btree *btp)
{
	int	len;
	int 	sz = btp->recsz;
	wchar_t	*s = btp->rp;
	int	y;


	if (fgetws(s, sz, iop) == NULL) {
		len = 0;
	} else {
		len = wslen(s);
	}

	if (len == 0) {
		return (1);
	}

	if (*(s + len - 1) == L'\n') {
		return (0);
	} else if (len < sz - 1) {
		newline_warning();
		s += len - 1;
		*++s = L'\n';
		*++s = L'\0';
		return (0);
	} else {
		sz += INC_MAXREC;
		if (btp->allocflag) {
			if ((btp->rp = (wchar_t *)
			    realloc(btp->rp, sz * sizeof (wchar_t))) == NULL) {
				(void) fprintf(stderr, gettext(mem_error));
				term();
			}
		} else {
			if ((btp->rp = (wchar_t *)malloc(sz *
			    sizeof (wchar_t))) == NULL) {
				(void) fprintf(stderr, gettext(mem_error));
				term();
			}
			(void) wscpy(btp->rp, s);
			btp->allocflag = 1;
		}
		s = btp->rp + len;
		for (;;) {
			if (fgetws(s, INC_MAXREC + 1, iop) == NULL) {
				len = 0;
			} else {
				len = wslen(s);
			}
			if (len == 0) {
				y = 1;
				break;
			}
			if (*(s + len - 1) == L'\n') {
				y = 0;
				break;
			} else if (len < INC_MAXREC) {
				newline_warning();
				s += len - 1;
				*++s = L'\n';
				*++s = L'\0';
				y = 0;
				break;
			} else {
				sz += INC_MAXREC;
				if ((btp->rp = (wchar_t *) realloc(btp->rp,
				    sz * sizeof (wchar_t))) == NULL) {
					(void) fprintf(stderr, gettext(
					    "sort: out of memory\n"));
					term();
				}
				s = btp->rp + sz - INC_MAXREC - 1;
			}
		}
		if (maxrec < (sz - (INC_MAXREC - len))) {
			maxrec = sz - (INC_MAXREC - len);
			if (not_c_locale && modflg) {
				free(collb1);
				if ((collb1 = (wchar_t *)malloc
				    (maxrec * sizeof (wchar_t) * 2)) == NULL) {
					(void) fprintf(stderr,
					    gettext(mem_error));
					term();
				}
				collb2 = collb1 + maxrec;
			}
			if (uflg) { /* expand save */
				s = save;
				if ((save = (wchar_t *) malloc(maxrec *
				    sizeof (wchar_t))) == NULL) {
					(void) fprintf(stderr,
					    gettext(mem_error));
					term();
				}
				(void) wscpy(save, s);
				if (save_alloc != 0) {
					free(s);
				}
				save_alloc = 1;
			}
		}
		btp->recsz = sz;
		return (y);
	} /* else */
}



static
int
yrline(FILE *iop, int i)
{
	int	len;
	wchar_t	*s;
	wchar_t *t;
	int	sz;
	int	y;

	s = lines[i];
	if (fgetws(s, maxrec, iop) == NULL) {
		len = 0;
	} else {
		len = wslen(s);
	}
	if (len == 0) {
		return (1);
	}
	if (*(s + len - 1) == L'\n') {
		return (0);
	} else if (len < maxrec - 1) {
		newline_warning();
		s += len - 1;
		*++s = L'\n';
		*++s = L'\0';
		return (0);
	} else {
		if ((t = (wchar_t *)malloc(maxrec *
		    sizeof (wchar_t))) == NULL) {
			(void) fprintf(stderr, gettext(mem_error));
			term();
		}
		(void) wscpy(t, lines[1 - i]); /* save the other line */

		if (lines[i] != (wchar_t *)lspace) {
			/* move around lines[i] */
			(void) wscpy((wchar_t *)lspace, lines[i]);
			s = lines[i] = (wchar_t *)lspace;
		}
		sz = INC_MAXREC + 1;
		s += len;
		for (;;) {
			maxrec += INC_MAXREC;
			alloc += INC_MAXREC * sizeof (wchar_t) * 2;
			if (brk((char *)lspace + alloc) != 0) {
				(void) fprintf(stderr, gettext(
				    "sort: fatal: line too long\n"));
				term();
			}
			if (fgetws(s, sz, iop) == NULL) {
				len = 0;
			} else {
				len = wslen(s);
			}
			if (len == 0) {
				y = 1;
				break;
			}
			s += len - 1;
			if (*s == L'\n') {
				y = 0;
				break;
			} else if (len < sz - 1) {
				newline_warning();
				*++s = L'\n';
				*++s = '\0';
				y = 0;
				break;
			} else {
				s++;
			}
		}
		lines[1-i] = (wchar_t *)lspace + maxrec;
		(void) wscpy(lines[1-i], t);			/* restore */
		free(t);
		if (not_c_locale && modflg) {
			free(collb1);
			if ((collb1 = (wchar_t *)
			    malloc(maxrec * 2 * sizeof (wchar_t))) == NULL) {
				(void) fprintf(stderr, gettext(mem_error));
				term();
			}
			collb2 = collb1 + maxrec;
		}
		return (y);
	}
}

/* Write the string in s to the output file */
static
void
write_line(wchar_t *s)
{
	(void) fputws(s, output_fp);
	if (ferror(output_fp)) {
		wterror(gettext("sort: write error while sorting: "));
	}

}

static
void
checksort(void)
{
	char	*f;		/* Temp file name. */
	int	i;
	int	j;
	int	r;
	FILE	*iop;

	f = setfil(0);
	if (f == 0) {
		iop = stdin;
	} else if ((iop = fopen(f, "r")) == NULL) {
		cant(f);
	}

	setbuf(iop, bufin);

	maxrec = INIT_MAXREC;
	alloc = maxrec * 2 * sizeof (wchar_t);
	if (alloc > (maxbrk - lspace) * sizeof (int *)) {
		maxrec = (maxbrk - lspace) / 2;
		alloc = maxrec * 2 * sizeof (int *);
	}
	(void) brk((char *)lspace + alloc);

	lines[0] = (wchar_t *)lspace;
	lines[1] = (wchar_t *)lspace + maxrec;

	if (not_c_locale && modflg) {
		if ((collb1 = (wchar_t *)malloc(maxrec * 2 *
		    sizeof (wchar_t))) == NULL) {
			(void) fprintf(stderr, gettext(mem_error));
			term();
		}
		collb2 = collb1 + maxrec;
	}

	if (yrline(iop, 0)) {
		if (ferror(iop)) {
			rderror(f);
		}
		(void) fclose(iop);
		exit(0);
	}
	i = 0;
	j = 1;
	while (!yrline(iop, j))  {
		r = (*compare)(lines[i], lines[j]);
		if (r < 0) {
			disorder(gettext("sort: disorder: %ws\n"),
			    lines[j]);
		}

		if (r == 0 && uflg) {
			disorder(gettext("sort: non-unique: %ws\n"),
			    lines[j]);
		}
		r = i;
		i = j;
		j = r;
	}
	if (ferror(iop)) {
		rderror(f);
	}
	(void) fclose(iop);
}

static
void
disorder(char *format, wchar_t *t)
{
	wchar_t	*u;

	for (u = t; *u != L'\n'; u++)
		/*LINTED [E_LOOP_EMPTY]*/
		;
	*u = 0;
#ifndef XPG4
	(void) fprintf(stderr, format, t);
#endif
	error = 1;
	term();
}

/* Create a new temp file and open it */
static
void
newfile(void)
{
	char	*f;

	/* Get name of temp file */
	f = setfil(nfiles);
	if ((output_fp = fopen(f, "w")) == NULL) {
		diag2(gettext("sort: can't create %s: "), f, errno);
		term();
	}
	nfiles++;
}

/* Returns the name of either the next input file or next temp file */
static
char *
setfil(int i)
{
	if (i < eargc) {
		/* Get next input file name */
		if (eargv[i][0] == '-' && eargv[i][1] == '\0') {
			return (0);
		} else {
			return (eargv[i]);
		}
	}
	/* Get next temp file name */
	i -= eargc;
	(void) sprintf(filep, ".%08x", i);
	return (file);
}

/* Opens the original output file (if specified) */
static
void
oldfile(void)
{
	if (outfil) {
		if ((output_fp = fopen(outfil, "w")) == NULL) {
			diag2(gettext("sort: can't create %s: "),
			    outfil, errno);
			term();
		}
	} else {
		output_fp = stdout;
	}
}

/* Determines if the output file is the same as any of the input files */
static
void
safeoutfil(void)
{
	int		i;		/* input file number */
	struct stat	ostat;
	struct stat	istat;

	if (!mflg || outfil == 0) {
		return;
	}
	if (stat(outfil, &ostat) == -1) {
		return;
	}
	if ((i = eargc - N) < 0) {
		i = 0;			/* -N is suff., not nec. */
	}
	for (; i < eargc; i++) {
		if (stat(eargv[i], &istat) == -1) {
			continue;
		}
		if (ostat.st_dev == istat.st_dev &&
		    ostat.st_ino == istat.st_ino) {
			unsafeout++;
		}
	}
}

static
void
cant(char *f)
{
	diag2(gettext("sort: can't open %s: "), f, errno);
	term();
}


static
void
diag1(const char *format, int errcode)
{
	char	*s = strerror(errcode);

	(void) fprintf(stderr, format);

	if (s == NULL) {
		(void) fprintf(stderr, gettext("Error %d\n"), errcode);
	} else {
		(void) fprintf(stderr, "%s\n", s);
	}
}


static
void
diag2(const char *format, const char *file, int errcode)
{
	char	*s = strerror(errcode);

	(void) fprintf(stderr, format, file);
	if (s == NULL) {
		(void) fprintf(stderr, gettext("Error %d\n"), errcode);
	} else {
		(void) fprintf(stderr, "%s\n", s);
	}
}

static
void
term(void)
{
	int	i;				/* input file number */

	if (nfiles == eargc) {
		nfiles++;
	}
	for (i = eargc; i <= nfiles; i++) {	/* <= in case of interrupt */
		(void) unlink(setfil(i));	/* with nfiles not updated */
	}
	exit(error);
}

static
int
getsign(wchar_t *pa, wchar_t *la)
{
	int	sign = 1;

	if (pa == la) {
		return (0);
	}
	if (*pa == L'-') {
		sign = -1;
		pa++;
	}
	while (pa < la && iswdigit(*pa)) {
		if (*pa != L'0') {
			return (sign);
		}
		pa++;
	}
	if (*pa != decpnt) {	/* sign is 0 */
		return (0);
	}
	pa++;
	while (pa < la && iswdigit(*pa)) {
		if (*pa != L'0') {
			return (sign);
		}
		pa++;
	}
	return (0);
}


/* Comparison function used if we are not using the default values in fields */
static
int
cmp(wchar_t *i, wchar_t *j)
{
	wchar_t		*pa;
	wchar_t		*pb;
	int		(*ignore)(wchar_t);
	int		sa;
	int		sb;
	wchar_t		(*code)(wchar_t);
	int		a;
	int		b;
	int		k;
	wchar_t		*la;
	wchar_t		*lb;
	wchar_t 	*ipa;
	wchar_t		*ipb;
	wchar_t		*jpa;
	wchar_t		*jpb;
	struct field	*fp;
	wchar_t		*p1;
	wchar_t		wa;
	wchar_t		wb;
	int		ret;

	for (k = (cur_num_fields > 0); k <= cur_num_fields; k++) {
		fp = &fields[k];
		pa = i;
		pb = j;
		if (k) {
			la = skip(pa, fp, 1);
			pa = skip(pa, fp, 0);
			lb = skip(pb, fp, 1);
			pb = skip(pb, fp, 0);
		} else {
			la = skip_to_eol(pa);
			lb = skip_to_eol(pb);
		}
		if (fp->fcmp == NUM) {
			sa = sb = fp->rflg;
			while (iswspace(*pa) && pa < la) {
				pa++;
			}
			while (iswspace(*pb) && pb < lb) {
				pb++;
			}
			if (pa == la) {				/* i is 0 */
				if (b = getsign(pb, lb)) {
					return (sb * b);
				}
				continue;
			} else if (pb == lb) {			/* j is 0 */
				if (a = getsign(pa, la)) {
					return ((-sa) * a);
				}
				continue;
			}
			if (*pa == '-') {
				pa++;
				sa = -sa;
			}
			if (*pb == '-') {
				pb++;
				sb = -sb;
			}
			for (ipa = pa; ipa < la; ipa++) {
				if (!(iswdigit(*ipa) ||
				    (*ipa == thousands_sep) ||
				    (*ipa == mon_thousands_sep))) {
					break;
				}
			}

			for (ipb = pb; ipb < lb; ipb++) {
				if (!(iswdigit(*ipb) ||
				    (*ipb == thousands_sep) ||
				    (*ipb == mon_thousands_sep))) {
					break;
				}
			}

			jpa = ipa;
			jpb = ipb;
			a = 0;
			if (sa == sb) {
				while (ipa > pa && ipb > pb) {
					ipa--;
					ipb--;
					while ((ipa > pa) &&
					    ((*ipa == thousands_sep) ||
					    (*ipa == mon_thousands_sep))) {
						ipa--;
					}
					while ((ipb > pb) &&
					    ((*ipb == thousands_sep) ||
					    (*ipb == mon_thousands_sep))) {
						ipb--;
					}
					if ((b = *ipb - *ipa) != 0) {
						a = b;
					}
				}
			}

			while (ipa > pa) {
				if ((*--ipa != L'0') &&
				    (*ipa != thousands_sep) &&
				    (*ipa != mon_thousands_sep)) {
					return (-sa);
				}
			}
			while (ipb > pb) {
				if ((*--ipb != L'0') &&
				    (*ipb != thousands_sep) &&
				    (*ipb != mon_thousands_sep)) {
					return (sb);
				}
			}
			if (a) {
				return (a * sa);
			}
			if ((*(pa = jpa) == decpnt) ||
			    (*(pa = jpa) == mon_decpnt)) {
				pa++;
			}
			if ((*(pb = jpb) == decpnt) ||
			    (*(pb = jpb) == mon_decpnt)) {
				pb++;
			}
			if (sa == sb) {
				while (pa < la && iswdigit(*pa) &&
				    pb < lb && iswdigit(*pb)) {
					if ((a = *pb++ - *pa++) != 0) {
						return (a * sa);
					}
				}
			}
			while (pa < la && iswdigit(*pa)) {
				if (*pa++ != L'0') {
					return (-sa);
				}
			}
			while (pb < lb && iswdigit(*pb)) {
				if (*pb++ != L'0') {
					return (sb);
				}
			}
			continue;
		} else if (fp->fcmp == MON)  {
			sa = fp->rflg * (month(pb) - month(pa));
			if (sa) {
				return (sa);
			} else {
				continue;
			}
		}
		code = fp->code;
		ignore = fp->ignore;

		if (not_c_locale == 0) {
			goto loop;
		}

		if (modflg) {
			for (p1 = collb1; pa < la && *pa != L'\n'; pa++) {
				if ((*ignore)(*pa)) {
					continue;
				}
				*p1++ = (*code)(*pa);
			}
			*p1 = L'\0';
			for (p1 = collb2; pb < lb && *pb != L'\n'; pb++) {
				if ((*ignore)(*pb)) {
					continue;
				}
				*p1++ = (*code)(*pb);
			}
			*p1 = L'\0';
			ret = wscoll(collb1, collb2);
		} else {	/* no transformation is needed */
			if (pa >= la || *pa == L'\n') {
				if (pb < lb && *pb != L'\n') {
					return (fp->rflg);
				} else {
					continue;
				}
			}
			if (pb >= lb || *pb == L'\n') {
				return (-fp->rflg);
			}
			wa = *la;
			*la = L'\0';
			wb = *lb;
			*lb = L'\0';
			ret = wscoll(pa, pb);
			*la = wa;
			*lb = wb;
		}
		if (ret > 0) {
			return (- fp->rflg);
		} else if (ret < 0) {
			return (fp->rflg);
		} else {
			continue;
		}

loop:		/* executed only when LC_COLLATE == C */
		while ((*ignore)(*pa) && *pa) {
			pa++;
		}
		while ((*ignore)(*pb) && *pb) {
			pb++;
		}
		if (pa >= la || *pa == L'\n') {
			if (pb < lb && *pb != L'\n') {
				return (fp->rflg);
			} else {
				continue;
			}
		}
		if (pb >= lb || *pb == L'\n') {
			return (-fp->rflg);
		}
		if ((sa = (*code)(*pb++) - (*code)(*pa++)) == 0) {
			goto loop;
		}
		return (sa * fp->rflg);
	}
	if (uflg) {
		return (0);
	}
	return (cmpa(i, j));
}

/* Comparison function used if we are using the default values in fields */
static
int
cmpa(wchar_t *pa, wchar_t *pb)
{
	int	result = wscoll(pa, pb);

	if (result == 0) {
		return (0);
	} else if (result > 0) {
		return (-fields[0].rflg);
	} else {
		return (fields[0].rflg);
	}
}

static
wchar_t *
skip(wchar_t *p, struct field *fp, int j)
{
	int	start_char;
	int	end_char;
	wchar_t tbc;

	if ((start_char = fp->m[j]) < 0) {
		return (skip_to_eol(p));
	}
	if ((tbc = tabchar) != 0) {
		while (--start_char >= 0) {
			while (*p != tbc) {
				if (*p != L'\n') {
					p++;
				} else {
					return (p);
				}
			}
			if (start_char > 0 || j == 0) {
				p++;
			}
		}
	} else {
		while (--start_char >= 0) {
			while (blank(*p)) {
				p++;
			}
			while (!blank(*p)) {
				if (*p != L'\n') {
					p++;
				} else {
					return (p);
				}
			}
		}
	}
	if (fp->bflg[j]) {
		if (j == 1 && fp->m[j] > 0) {
			p++;
		}
		while (blank(*p)) {
			p++;
		}
	}
	end_char = fp->n[j];
	while ((end_char-- > 0) && (*p != L'\n')) {
		p++;
	}
	return (p);
}

static
wchar_t *
skip_to_eol(wchar_t *p)
{
	while (*p != L'\n') {
		p++;
	}
	return (p);
}


static
void
initree(void)
{
	struct btree	**tpp;
	struct btree	*tp;
	int		i;

	tp = &(tree[0]);
	tpp = &(treep[0]);
	i = TREEZ;
	while (--i >= 0) {
		*tpp++ = tp++;
	}
}

int
cmpsave(int n)
{
	int	award;

	if (n < 2) {
		return (0);
	}
	for (n++, award = 0; (n >>= 1) > 0; award++)
		/*LINTED [E_LOOP_EMPTY]*/
		;
	return (award);
}

/*
 * Fill field[nnfields] for the current command argument s.
 * k is non-zero if keys are being verified for correctness
 * kflag is 1 if command line is XCU4 sort key field style:
 *	-k field_start[type][,field_end[type]]
 * kflag is 0 if command line is obsolescent sort key style:
 *	[+pos1 [-pos2]]
 * where pos1 and pos2 are of the form:
 *	field0_number[.first0_character][type]
 *
 * NOTE: the fields and characters in pos1 and pos2 are
 * numbered from 0; XCU4 fields and characters (-k) are
 * numbered from 1. The relation as specified in XCU4.2 is:
 *
 *	The fully specified +pos1 -pos2 form with type
 *	modifiers T and U:
 *		+w.xT -y.zU
 *	is equivalent to:
 *		undefined		(z == 0 & U contains b & -t is present)
 *		-k w+1.x+1T,y.0U	(z == 0 otherwise)
 *		-k w+1.x+1T,y+1.zU	(z > 0)
 */
static
int
field(char *s, int k, int kflag, int nnfields)
{
	int		(*save_compare)(wchar_t *, wchar_t *) = compare;
	struct field	*p;
	int		d;
	int		num_bytes;

	p = &fields[nnfields];

	for (; *s != 0; s++) {
		d = 0;
		switch (*s) {
		case '\0':
			return (0);

		case ',':
			k = (cur_num_fields > 0);
			break;

		case 'b':
			p->bflg[k]++;
			break;

		case 'd':
			p->ignore = dict;
			modflg = 1;
			break;

		case 'f':
			p->code = fold;
			modflg = 1;
			break;

		case 'i':
			p->ignore = nonprint;
			modflg = 1;
			break;

		case 'c':
			cflg = 1;
			continue;

		case 'm':
			mflg = 1;
			continue;

		case 'M':
			month_init();
			p->fcmp = MON;
			p->bflg[0]++;
			break;

		case 'n':
			p->fcmp = NUM;
			p->bflg[0]++;
			break;

		case 't':
			num_bytes = mbtowc(&tabchar, s + 1, MB_CUR_MAX);
			if (num_bytes > 0) {
				s += num_bytes;
			}
			continue;

		case 'r':
			p->rflg = -1;
			continue;

		case 'u':
			uflg = 1;
			continue;

		case 'y':
			if (*++s) {
				if (isdigit(*s)) {
					tryfor = number(&s) * 1024;
				} else {
					usage();
				}
			} else {
				--s;
				tryfor = MAXMEM;
			}
			continue;

		case 'z':
			/* obsolete option, ignore */
			return (0);

		case '.':
			if (p->m[k] == -1) {	/* -m.n with m missing */
				p->m[k] = 0;
			}
			d = &fields[0].n[0] - &fields[0].m[0];
			if (*++s == 0) {
				--s;
				/*
				 * This is the last character of either the
				 * start sort key or the ending sort key.
				 * k + d will equal 2 or 3 and will change
				 * the value of n[0] or n[1]
				 */
				/*
				p->m[k + d] = 0;
				*/
				p->n[k] = 0;
				continue;
			}

		default:
			if (isdigit(*s)) {
				/*
				 * If this is the first character of either the
				 * start sort key or the ending sort key,
				 * k + d will be 0 or 1 (d = 0).  If this is
				 * the last character of either the start sort
				 * key or the ending sort key, k + d will equal
				 * 2 or 3 (d = 2) and will actually
				 * change the value of n[0] or n[1]
				 */
				p->m[k + d] = number(&s);
			} else {
				usage();
			}
		}
		compare = cmp;
	}

	/* Adjust sort keys for -k */
	if (kflag == 1) {
		/* Shouldn't we have an error if m[0] does = 0 ? */
		/* Actually, probably not - could have been no initial key */
		if (p->m[0] != 0) {
			p->m[0]--;
		}
		if (p->n[0] != 0) {
			p->n[0]--;
		}
		if (p->n[1] != 0) {	/* this is not a bug */
			p->m[1]--;	/* decrement m[1] if n[1] != 0 */
		}			/* see comments above */
	}

	/* Verify keys for correctness */
	if (k) {
		/* Shouldn't this be m[1] != -1 ??? */
		/* CHECK ME */
		if ((p->m[1] != 0) && (p->m[0] > p->m[1])) {
			compare = save_compare;
			return (-1);
		}
		if ((p->m[0] == p->m[1]) &&
		    (p->n[0] != 0) &&
		    (p->n[0] > p->n[1])) {
			compare = save_compare;
			return (-1);
		}
	}

	return (0);
}

/*
 * Parse an integer at *ppa of a command argument, advance ppa past
 * the number, return the integer value.
 */
static
int
number(char **ppa)
{
	int	n;
	char	*pa;

	pa = *ppa;
	n = 0;
	while (isdigit(*pa)) {
		n = n * 10 + *pa - '0';
		*ppa = pa++;
	}
	return (n);
}

static
void
qksort(wchar_t **a, wchar_t **l)
{
	wchar_t 	**i;
	wchar_t		**j;
	wchar_t 	**lp;
	wchar_t		**hp;
	wchar_t 	**k;
	int		c;
	int		delta;
	wchar_t 	*t;		/* used for result of qsexc() */
	unsigned int	n;


start:
	if ((n = l - a) <= 1) {
		return;
	}

	n /= 2;
	if (n >= MTHRESH) {
		lp = a + n;
		i = lp - 1;
		j = lp + 1;
		delta = 0;
		c = (*compare)(*lp, *i);
		if (c < 0) {
			--delta;
		} else if (c > 0) {
			++delta;
		}
		c = (*compare)(*lp, *j);
		if (c < 0) {
			--delta;
		} else if (c > 0) {
			++delta;
		}
		if ((delta /= 2) && (c = (*compare)(*i, *j))) {
		    if (c > 0) {
			n -= delta;
		    } else {
			n += delta;
		    }
		}
	}
	hp = lp = a + n;
	i = a;
	j = l - 1;


	for (;;) {
		if (i < lp) {
			if ((c = (*compare)(*i, *lp)) == 0) {
				--lp;
				qsexc(i, lp);
				continue;
			}
			if (c < 0) {
				++i;
				continue;
			}
		}

loop:
		if (j > hp) {
			if ((c = (*compare)(*hp, *j)) == 0) {
				++hp;
				qsexc(hp, j);
				goto loop;
			}
			if (c > 0) {
				if (i == lp) {
					++hp;
					qstexc(i, hp, j);
					i = ++lp;
					goto loop;
				}
				qsexc(i, j);
				--j;
				++i;
				continue;
			}
			--j;
			goto loop;
		}


		if (i == lp) {
			if (uflg) {
				k = lp;
				while (k < hp)
					**k++ = 0;
			}
			if (lp - a >= l - hp) {
				qksort(hp + 1, l);
				l = lp;
			} else {
				qksort(a, lp);
				a = hp+1;
			}
			goto start;
		}

		--lp;
		qstexc(j, lp, i);
		j = --hp;
	}
}


static
void
month_init(void)
{
	char	time_buf[MAX_MON_LEN * MB_LEN_MAX];
	wchar_t	time_wbuf[MAX_MON_LEN];
	int	i;

	for (i = 0; i < 12; i++) {
		ct.tm_mon = i;
		/* Get locale's abbreviated month name */
		(void) ascftime(time_buf, "%b", &ct);
		(void) mbstowcs(time_wbuf, time_buf, MAX_MON_LEN);
		months[i] = wsdup(time_wbuf);
	}
}


static
int
month(wchar_t *s)
{
	wchar_t *t;
	wchar_t *u;
	int	i;

	for (i = 0; i < 12; i++) {
		for (t = s, u = months[i]; fold(*t++) == fold(*u++); ) {
			if (*u == 0) {
				return (i);
			}
		}
	}
	return (-1);
}

static
void
rderror(char *s)
{
	if (s == 0) {
		diag1(gettext("sort: read error on stdin: "), errno);
	} else {
		diag2(gettext("sort: read error on %s: "), s, errno);
	}
	term();
}

static
void
wterror(char *format)
{
	/* gettext has already been applied to format when wterror is invoked */
	diag1(format, errno);
	term();
}

static
int
grow_core(unsigned int size, unsigned int cursize)
{
	unsigned int	newsize;

	newsize = size + cursize;
	if (newsize < MINMEM) {
		newsize = MINMEM;
	} else if (newsize > MAXMEM) {
		newsize = MAXMEM;
	}

	/* Check for overflow */
	for (; ((char *)lspace + newsize) <= (char *)lspace; newsize >>= 1)
		/*LINTED [E_LOOP_EMPTY]*/
		;

	if (newsize > ((maxbrk - lspace) * sizeof (int *))) {
		newsize = (maxbrk - lspace) * sizeof (int *);
	}
	if (newsize <= cursize) {
		return (0);
	}
	if (brk((char *)lspace + newsize) != 0) {
		return (0);
	}
	return (newsize - cursize);
}


/*
 * The three "ignore" functions:
 *	zero - the default ignore function, ignore nothing
 *	nonprint - ignore non-printable characters
 *	dict - used with -d option, ignore anything that is not
 *		a letter, digit, or tab
 */

static
int
/*LINTED [E_FUNC_ARG_UNUSED]*/
zero(wchar_t w)
{
	return (0);
}

/* Return 0 if w is a printing character */
static
int
nonprint(wchar_t w)
{
	return (!iswprint(w));
}

/*
 * Return 0 if w is a space, tab, carriage return, newline, vertical tab,
 * formfeed, letter, or digit
 */
static
int
dict(wchar_t w)
{
	return (!(iswalnum(w) || iswspace(w)));
}


/*
 * The next two functions are used as the "code" function that
 * determines whether or not lower-case letters should be
 * folded into upper-case letters.
 */

/* Return the uppercase version of w */
static
wchar_t
fold(wchar_t w)
{
	return ((iswlower(w) != 0) ? towupper(w) : w);
}

/* No-op version of fold */
static
wchar_t
nofold(wchar_t w)
{
	return (w);
}

/* Load the decimal points and thousands separators for this locale. */
static
void
initdecpnt(void)
{
	struct lconv *l = localeconv();

	(void) mbtowc(&decpnt, l->decimal_point, MB_CUR_MAX);
	(void) mbtowc(&mon_decpnt, l->mon_decimal_point, MB_CUR_MAX);
	(void) mbtowc(&thousands_sep, l->thousands_sep, MB_CUR_MAX);
	(void) mbtowc(&mon_thousands_sep, l->mon_thousands_sep, MB_CUR_MAX);
}

static
void
newline_warning(void)
{
	(void) fprintf(stderr, gettext(
	    "sort: warning: missing NEWLINE added at EOF\n"));
}

static
void
usage(void)
{
	(void) fprintf(stderr, gettext(
	    "sort [-bcdfiMmnru] [-o output] [-T directory] [-ykmem] [-t char]\n"
	    "     [+pos1 [-pos2]] [-k field_start[type][,field_end[type]] "
	    "[file...]\n"));
	exit(2);
}

static
char *
get_subopt(int argc, char **argv, char option)
{
	if ((--argc <= 0) || (**++argv == '-')) {
		(void) fprintf(stderr, gettext(
		    "sort: option requires an argument -- %c\n"), option);
		usage();
	}
	return (*argv);
}
