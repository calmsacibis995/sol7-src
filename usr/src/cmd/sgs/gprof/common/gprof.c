/*
 * Copyright (c) 1990-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)gprof.c	1.16	98/01/06 SMI"

#include	<sysexits.h>
#include	<stdlib.h>
#include	<unistd.h>
#include	"gprof.h"
#include	"profile.h"

char *		whoami = "gprof";
static pctype	lowpc, highpc;	/* range profiled, in UNIT's */

/*
 *	things which get -E excluded by default.
 */
static char *	defaultEs[] = {
		"mcount",
		"__mcleanup",
		0};



static pctype
min(pctype a, pctype b)
{
	if (a < b)
		return (a);
	return (b);
}

static pctype
max(pctype a, pctype b)
{
	if (a > b)
		return (a);
	return (b);
}


static void
tally(struct rawarc *rawp)
{
	nltype		*parentp;
	nltype		*childp;

	/*
	 * if count == 0 this is a null arc and
	 * we don't need to tally it.
	 */
	if (rawp->raw_count == 0)
		return;

	parentp = nllookup(rawp->raw_frompc);
	childp = nllookup(rawp->raw_selfpc);
	if (childp && parentp) {
		if (!Dflag)
			childp->ncall += rawp->raw_count;
		else {
			if (first_file)
				childp->ncall += rawp->raw_count;
			else {
				childp->ncall -= rawp->raw_count;
				if (childp->ncall < 0)
					childp->ncall = 0;
			}
		}
#ifdef DEBUG
		if (debug & TALLYDEBUG) {
			printf("[tally] arc from %s to %s traversed "
			    "%lld times\n", parentp->name,
			    childp->name, rawp->raw_count);
		}
#endif DEBUG
		addarc(parentp, childp, rawp->raw_count);
	}
}

static void
readsamples(FILE *pfile)
{
	sztype		i;
	unsigned_UNIT	sample;

	if (samples == 0) {
		samples = (unsigned_UNIT *) calloc(nsamples,
		    sizeof (unsigned_UNIT));
		if (samples == 0) {
			fprintf(stderr, "%s: No room for %ld sample pc's\n",
			    whoami, sampbytes / sizeof (unsigned_UNIT));
			exit(EX_OSERR);
		}
	}

	for (i = 0; i < nsamples; i++) {
		fread(&sample, sizeof (unsigned_UNIT), 1, pfile);
		if (feof(pfile))
			break;
		samples[i] += sample;
	}
	if (i != nsamples) {
		fprintf(stderr,
		    "%s: unexpected EOF after reading %ld/%ld samples\n",
		    whoami, --i, nsamples);
		exit(EX_IOERR);
	}
}

static FILE *
openpfile(char * filename)
{
	struct hdr	tmp;
	FILE *		pfile;
	unsigned long	magic_num;
	size_t		hdrsize = sizeof (struct hdr);

	if ((pfile = fopen(filename, "r")) == NULL) {
		perror(filename);
		exit(EX_IOERR);
	}

	/*
	 * First we need to determine if this is a run-time linker
	 * profiled file or if it is a standard gmon.out.
	 *
	 * We do this by reading in the first four bytes of the file
	 * and see if they match PRF_MAGIC.  If they do then this
	 * is a run-time linker profiled file, if they don't it
	 * must be a gmon.out file.
	 */
	if (fread(&magic_num, sizeof (unsigned long), 1, pfile) == 0) {
		perror("fread()");
		exit(EX_IOERR);
	}

	rewind(pfile);

	if (magic_num == (unsigned long)PRF_MAGIC)
		rflag = TRUE;
	else
		rflag = FALSE;

	if (rflag) {
		if (Bflag) {
			L_hdr64		l_hdr64;

			/*
			 * If the rflag is set then the input file is
			 * rtld profiled data, we'll read it in and convert
			 * it to the standard format (ie: make it look like
			 * a gmon.out file).
			 */
			if (fread(&l_hdr64, sizeof (L_hdr64), 1, pfile) == 0) {
				perror("fread()");
				exit(EX_IOERR);
			}
			if (l_hdr64.hd_version != PRF_VERSION_64) {
				fprintf(stderr, "gprof: expected version %d, got "
				    "version %d when processing 64-bit run-time linker "
				    "profiled file.\n", PRF_VERSION_64, l_hdr64.hd_version);
				exit(EX_SOFTWARE);
			}
			tmp.lowpc = 0;
			tmp.highpc = (pctype)l_hdr64.hd_hpc;
			tmp.ncnt = sizeof (M_hdr64) + l_hdr64.hd_psize;
		} else {
			L_hdr		l_hdr;

			/*
			 * If the rflag is set then the input file is
			 * rtld profiled data, we'll read it in and convert
			 * it to the standard format (ie: make it look like
			 * a gmon.out file).
			 */
			if (fread(&l_hdr, sizeof (L_hdr), 1, pfile) == 0) {
				perror("fread()");
				exit(EX_IOERR);
			}
			if (l_hdr.hd_version != PRF_VERSION) {
				fprintf(stderr, "gprof: expected version %d, got "
				    "version %d when processing run-time linker "
				    "profiled file.\n", PRF_VERSION, l_hdr.hd_version);
				exit(EX_SOFTWARE);
			}
			tmp.lowpc = 0;
			tmp.highpc = (pctype)l_hdr.hd_hpc;
			tmp.ncnt = sizeof (M_hdr) + l_hdr.hd_psize;
		}
	} else {
		if (Bflag) {
			if (fread(&tmp, sizeof (struct hdr), 1, pfile) == 0) {
				perror("fread()");
				exit(EX_IOERR);
			}
		} else {
			/*
			 * If we're not reading big %pc's, we need to read
			 * the 32-bit header, and assign the members to
			 * the actual header.
			 */
			struct hdr32 hdr32;
			if (fread(&hdr32, sizeof (hdr32), 1, pfile) == 0) {
				perror("fread()");
				exit(EX_IOERR);
			}
			tmp.lowpc = hdr32.lowpc;
			tmp.highpc = hdr32.highpc;
			tmp.ncnt = hdr32.ncnt;
			hdrsize = sizeof (struct hdr32);
		}
	}

	/*
	 * perform sanity check on profiled file we've opened.
	 */
	if (tmp.lowpc >= tmp.highpc) {
		if (rflag)
			fprintf(stderr, "%s: badly formed profiled data.\n",
			    filename);
		else
			fprintf(stderr, "%s: badly formed gmon.out file.\n",
			    filename);
		exit(EX_SOFTWARE);
	}

	if (s_highpc != 0 && (tmp.lowpc != h.lowpc ||
	    tmp.highpc != h.highpc || tmp.ncnt != h.ncnt)) {
		fprintf(stderr,
		    "%s: incompatible with first gmon file\n",
		    filename);
		exit(EX_IOERR);
	}
	h = tmp;
	s_lowpc = h.lowpc;
	s_highpc = h.highpc;
	lowpc = h.lowpc / sizeof (UNIT);
	highpc = h.highpc / sizeof (UNIT);
	sampbytes = h.ncnt - hdrsize;
	nsamples = sampbytes / sizeof (unsigned_UNIT);

#ifdef DEBUG
	if (debug & SAMPLEDEBUG) {
		printf("[openpfile] hdr.lowpc 0x%llx hdr.highpc "
		    "0x%llx hdr.ncnt %lld\n",
		    h.lowpc, h.highpc, h.ncnt);
		printf("[openpfile]   s_lowpc 0x%llx   s_highpc 0x%llx\n",
		    s_lowpc, s_highpc);
		printf("[openpfile]     lowpc 0x%llx     highpc 0x%llx\n",
		    lowpc, highpc);
		printf("[openpfile] sampbytes %d nsamples %d\n",
		    sampbytes, nsamples);
	}
#endif DEBUG
	return (pfile);
}



/*
 *	information from a gmon.out file is in two parts:
 *	an array of sampling hits within pc ranges,
 *	and the arcs.
 */
static void
getpfile(char *filename)
{
	FILE		*pfile;

	pfile = openpfile(filename);
	readsamples(pfile);
	/*
	 *	the rest of the file consists of
	 *	a bunch of <from,self,count> tuples.
	 */
	/* CONSTCOND */
	while (1) {
		struct rawarc	arc;

		if (rflag) {
			if (Bflag) {
				L_cgarc64		rtld_arc64;

				/*
				 * If rflag is set then this is an profiled
				 * image generated by rtld.  It needs to be
				 * 'converted' to the standard data format.
				 */
				if (fread(&rtld_arc64, sizeof (L_cgarc64), 1, pfile)
				    != 1)
					break;

				if (rtld_arc64.cg_from == PRF_OUTADDR64)
					arc.raw_frompc = s_highpc + 0x10;
				else
					arc.raw_frompc =
					    (pctype)rtld_arc64.cg_from;
				arc.raw_selfpc = (pctype)rtld_arc64.cg_to;
				arc.raw_count = (actype)rtld_arc64.cg_count;
			} else {
				L_cgarc		rtld_arc;

				/*
				 * If rflag is set then this is an profiled
				 * image generated by rtld.  It needs to be
				 * 'converted' to the standard data format.
				 */
				if (fread(&rtld_arc, sizeof (L_cgarc), 1, pfile) != 1)
					break;

				if (rtld_arc.cg_from == PRF_OUTADDR)
					arc.raw_frompc = s_highpc + 0x10;
				else
					arc.raw_frompc =
					    (pctype)rtld_arc.cg_from;
				arc.raw_selfpc = (pctype)rtld_arc.cg_to;
				arc.raw_count = (actype)rtld_arc.cg_count;
			}
		} else {
			if (Bflag) {
				if (fread(&arc, sizeof (struct rawarc), 1,
				    pfile) != 1) {
					break;
				}
			} else {
				/*
				 * If these aren't big %pc's, we need to read
				 * into the 32-bit raw arc structure, and
				 * assign the members into the actual arc.
				 */
				struct rawarc32 arc32;
				if (fread(&arc32, sizeof (struct rawarc32),
				    1, pfile) != 1)
					break;
				arc.raw_frompc = (pctype)arc32.raw_frompc;
				arc.raw_selfpc = (pctype)arc32.raw_selfpc;
				arc.raw_count  = (actype)arc32.raw_count;
			}
		}

#ifdef DEBUG
		if (debug & SAMPLEDEBUG) {
			printf("[getpfile] frompc 0x%llx selfpc "
			    "0x%llx count %lld\n", arc.raw_frompc,
			    arc.raw_selfpc, arc.raw_count);
		}
#endif DEBUG
		/*
		 *	add this arc
		 */
		tally(&arc);
	}
	if (first_file)
		first_file = FALSE;
	fclose(pfile);
}

/*
 * dump out the gmon.sum file
 */
static void
dumpsum(char *  sumfile)
{
	register nltype *nlp;
	register arctype *arcp;
	struct rawarc arc;
	FILE *sfile;

	if ((sfile = fopen(sumfile, "w")) == NULL) {
		perror(sumfile);
		exit(EX_IOERR);
	}
	/*
	 * dump the header; use the last header read in
	 */
	if (fwrite(&h, sizeof (h), 1, sfile) != 1) {
		perror(sumfile);
		exit(EX_IOERR);
	}
	/*
	 * dump the samples
	 */
	if (fwrite(samples, sizeof (unsigned_UNIT), nsamples, sfile) !=
	    nsamples) {
		perror(sumfile);
		exit(EX_IOERR);
	}
	/*
	 * dump the normalized raw arc information
	 */
	for (nlp = nl; nlp < npe; nlp++) {
		for (arcp = nlp->children; arcp;
		    arcp = arcp->arc_childlist) {
			arc.raw_frompc = arcp->arc_parentp->value;
			arc.raw_selfpc = arcp->arc_childp->value;
			arc.raw_count = arcp->arc_count;
			if (fwrite(&arc, sizeof (arc), 1, sfile) != 1) {
				perror(sumfile);
				exit(EX_IOERR);
			}
#ifdef DEBUG
			if (debug & SAMPLEDEBUG) {
				printf("[dumpsum] frompc 0x%llx selfpc "
				    "0x%llx count %lld\n", arc.raw_frompc,
				    arc.raw_selfpc, arc.raw_count);
			}
#endif DEBUG
		}
	}
	fclose(sfile);
}


/*
 *	calculate scaled entry point addresses (to save time in asgnsamples),
 *	and possibly push the scaled entry points over the entry mask,
 *	if it turns out that the entry point is in one bucket and the code
 *	for a routine is in the next bucket.
 */
static void
alignentries()
{
	register struct nl *	nlp;
#ifdef	DEBUG
	pctype			bucket_of_entry;
	pctype			bucket_of_code;
#endif

	for (nlp = nl; nlp < npe; nlp++) {
		nlp->svalue = nlp->value / sizeof (UNIT);
#ifdef DEBUG
		bucket_of_entry = (nlp->svalue - lowpc) / scale;
		bucket_of_code = (nlp->svalue + UNITS_TO_CODE - lowpc) / scale;
		if (bucket_of_entry < bucket_of_code) {
			if (debug & SAMPLEDEBUG) {
				printf("[alignentries] pushing svalue 0x%llx "
				"to 0x%llx\n", nlp->svalue,
				nlp->svalue + UNITS_TO_CODE);
			}
		}
#endif DEBUG
	}
}


/*
 *	Assign samples to the procedures to which they belong.
 *
 *	There are three cases as to where pcl and pch can be
 *	with respect to the routine entry addresses svalue0 and svalue1
 *	as shown in the following diagram.  overlap computes the
 *	distance between the arrows, the fraction of the sample
 *	that is to be credited to the routine which starts at svalue0.
 *
 *	    svalue0                                         svalue1
 *	       |                                               |
 *	       v                                               v
 *
 *	       +-----------------------------------------------+
 *	       |					       |
 *	  |  ->|    |<-		->|         |<-		->|    |<-  |
 *	  |         |		  |         |		  |         |
 *	  +---------+		  +---------+		  +---------+
 *
 *	  ^         ^		  ^         ^		  ^         ^
 *	  |         |		  |         |		  |         |
 *	 pcl       pch		 pcl       pch		 pcl       pch
 *
 *	For the vax we assert that samples will never fall in the first
 *	two bytes of any routine, since that is the entry mask,
 *	thus we give call alignentries() to adjust the entry points if
 *	the entry mask falls in one bucket but the code for the routine
 *	doesn't start until the next bucket.  In conjunction with the
 *	alignment of routine addresses, this should allow us to have
 *	only one sample for every four bytes of text space and never
 *	have any overlap (the two end cases, above).
 */
static void
asgnsamples()
{
	sztype		i, j;
	unsigned_UNIT	ccnt;
	double		time;
	pctype		pcl, pch;
	pctype		overlap;
	pctype		svalue0, svalue1;

	/* read samples and assign to namelist symbols */
	scale = highpc - lowpc;
	scale /= nsamples;
	alignentries();
	for (i = 0, j = 1; i < nsamples; i++) {
		ccnt = samples[i];
		if (ccnt == 0)
			continue;
		pcl = lowpc + scale * i;
		pch = lowpc + scale * (i + 1);
		time = ccnt;
#ifdef DEBUG
		if (debug & SAMPLEDEBUG) {
			printf("[asgnsamples] pcl 0x%llx pch 0x%llx ccnt %d\n",
			    pcl, pch, ccnt);
		}
#endif DEBUG
		totime += time;
		for (j = (j ? j - 1 : 0); j < nname; j++) {
			svalue0 = nl[j].svalue;
			svalue1 = nl[j+1].svalue;
			/*
			 *	if high end of tick is below entry address,
			 *	go for next tick.
			 */
			if (pch < svalue0)
				break;
			/*
			 *	if low end of tick into next routine,
			 *	go for next routine.
			 */
			if (pcl >= svalue1)
				continue;
			overlap = min(pch, svalue1) - max(pcl, svalue0);
			if (overlap != 0) {
#ifdef DEBUG
				if (debug & SAMPLEDEBUG) {
					printf("[asgnsamples] "
					    "(0x%llx->0x%llx-0x%llx) %s gets "
					    "%f ticks %lld overlap\n",
					    nl[j].value/sizeof (UNIT), svalue0,
					    svalue1, nl[j].name,
					    overlap * time / scale, overlap);
				}
#endif DEBUG
				nl[j].time += overlap * time / scale;
			}
		}
	}
#ifdef DEBUG
	if (debug & SAMPLEDEBUG) {
		printf("[asgnsamples] totime %f\n", totime);
	}
#endif DEBUG
}

void
done()
{

	exit(EX_OK);
}


main(int argc, char ** argv)
{
	char	**sp;
	nltype	**timesortnlp;
	int		c;
	int		errflg;
	extern char	*optarg;
	extern int	optind;

	prog_name = *argv;  /* preserve program name */
	debug = 0;
	nflag = FALSE;
	bflag = TRUE;
	lflag = FALSE;
	Cflag = FALSE;
	first_file = TRUE;
	rflag = FALSE;
	Bflag = FALSE;
	errflg = FALSE;

	while ((c = getopt(argc, argv, "abd:CcDE:e:F:f:ln:sz")) != EOF)
		switch (c) {
		case 'a':
			aflag = TRUE;
			break;
		case 'b':
			bflag = FALSE;
			break;
		case 'c':
			cflag = TRUE;
			break;
		case 'C':
			Cflag = TRUE;
			break;
		case 'd':
			dflag = TRUE;
			debug |= atoi(optarg);
			printf("[main] debug = 0x%x\n", debug);
			break;
		case 'D':
			Dflag = TRUE;
			break;
		case 'E':
			addlist(Elist, optarg);
			Eflag = TRUE;
			addlist(elist, optarg);
			eflag = TRUE;
			break;
		case 'e':
			addlist(elist, optarg);
			eflag = TRUE;
			break;
		case 'F':
			addlist(Flist, optarg);
			Fflag = TRUE;
			addlist(flist, optarg);
			fflag = TRUE;
			break;
		case 'f':
			addlist(flist, optarg);
			fflag = TRUE;
			break;
		case 'l':
			lflag = TRUE;
			break;
		case 'n':
			nflag = TRUE;
			number_funcs_toprint = atoi(optarg);
			break;
		case 's':
			sflag = TRUE;
			break;
		case 'z':
			zflag = TRUE;
			break;
		case '?':
			errflg++;

		}

	if (errflg) {
		(void) fprintf(stderr,
		    "usage: gprof [ -abcCDlsz ] [ -e function-name ] "
		    "[ -E function-name ]\n\t[ -f function-name ] "
		    "[ -F function-name  ]\n\t[  image-file  "
		    "[ profile-file ... ] ]\n");
		exit(EX_USAGE);
	}

	if (optind < argc) {
		a_outname  = argv[optind++];
	} else {
		a_outname  = A_OUTNAME;
	}
	if (optind < argc) {
		gmonname = argv[optind++];
	} else {
		gmonname = GMONNAME;
	}
	/*
	 *	turn off default functions
	 */
	for (sp = &defaultEs[0]; *sp; sp++) {
		Eflag = TRUE;
		addlist(Elist, *sp);
		eflag = TRUE;
		addlist(elist, *sp);
	}
	/*
	 *	how many ticks per second?
	 *	if we can't tell, report time in ticks.
	 */
	hz = sysconf(_SC_CLK_TCK);
	if (hz == -1) {
		hz = 1;
		fprintf(stderr, "time is in ticks, not seconds\n");
	}

	getnfile(a_outname);

	/*
	 *	get information about mon.out file(s).
	 */
	do {
		getpfile(gmonname);
		if (optind < argc)
			gmonname = argv[optind++];
		else
			optind++;
	} while (optind <= argc);
	/*
	 *	dump out a gmon.sum file if requested
	 */
	if (sflag || Dflag)
		dumpsum(GMONSUM);
	/*
	 *	assign samples to procedures
	 */
	asgnsamples();

	/*
	 *	assemble the dynamic profile
	 */
	timesortnlp = doarcs();

	/*
	 *	print the dynamic profile
	 */
#ifdef DEBUG
	if (debug & ANYDEBUG) {
		/* raw output of all symbols in all their glory */
		int i;
		printf(" Name, pc_entry_pt, svalue, tix_in_routine, "
		    "#calls, selfcalls, index \n");
		for (i = 0; i < nname; i++) { 	/* Print each symbol */
			if (timesortnlp[i]->name)
				printf(" %s ", timesortnlp[i]->name);
			else
				printf(" <cycle> ");
			printf(" %lld ", timesortnlp[i]->value);
			printf(" %lld ", timesortnlp[i]->svalue);
			printf(" %f ", timesortnlp[i]->time);
			printf(" %lld ", timesortnlp[i]->ncall);
			printf(" %lld ", timesortnlp[i]->selfcalls);
			printf(" %d ", timesortnlp[i]->index);
			printf(" \n");
		}
	}
#endif

	printgprof(timesortnlp);
	/*
	 *	print the flat profile
	 */
	printprof();
	/*
	 *	print the index
	 */
	printindex();
	done();
	/* NOTREACHED */
	return (0);
}
