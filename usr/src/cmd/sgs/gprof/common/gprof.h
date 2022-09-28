/*
 * Copyright (c) 1993,1997 by Sun Microsystems, Inc.
 */

#ifndef	_SGS_GPROF_H
#define	_SGS_GPROF_H

#pragma ident	"@(#)gprof.h	1.15	97/07/28 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <elf.h>

#include "sparc.h"
#include "gelf.h"


/*
 * who am i, for error messages.
 */
extern char	*whoami;

/*
 * booleans
 */
typedef int	bool;
#define	FALSE	0
#define	TRUE	1

/*
 *	ticks per second
 */
long	hz;

typedef	short UNIT;		/* unit of profiling */
typedef unsigned short	unsigned_UNIT; /* to remove warnings from gprof.c */
char	*a_outname;
char	*prog_name;	/* keep the program name for error messages */
#define	A_OUTNAME		"a.out"

typedef unsigned long long pctype;
typedef unsigned long pctype32;
typedef size_t sztype;

/*
 * Type definition for the arc count.
 */
typedef long long actype;
typedef long actype32;

char	*gmonname;
#define	GMONNAME		"gmon.out"
#define	GMONSUM			"gmon.sum"

/*
 * Special symbols used for profiling of shared libraries through
 * the run-time linker.
 */
#define	PRF_ETEXT		"_etext"
#define	PRF_EXTSYM		"<external>"
#define	PRF_MEMTERM		"_END_OF_VIRTUAL_MEMORY"
#define	PRF_SYMCNT		3

/*
 *	blurbs on the flat and graph profiles.
 */
#define	FLAT_BLURB	"/gprof.flat.blurb"
#define	CALLG_BLURB	"/gprof.callg.blurb"

/*
 *	a raw arc,
 *	    with pointers to the calling site and the called site
 *          and a count.
 */
struct rawarc {
	pctype		raw_frompc;
	pctype		raw_selfpc;
	actype		raw_count;
};

struct rawarc32 {
	pctype32	raw_frompc;
	pctype32	raw_selfpc;
	actype32	raw_count;
};

/*
 *	a constructed arc,
 *	    with pointers to the namelist entry of the parent and the child,
 *	    a count of how many times this arc was traversed,
 *	    and pointers to the next parent of this child and
 *	    the next child of this parent.
 */
struct arcstruct {
    struct nl		*arc_parentp;	/* pointer to parent's nl entry */
    struct nl		*arc_childp;	/* pointer to child's nl entry */
    actype		arc_count;	/* how calls from parent to child */
    double		arc_time;	/* time inherited along arc */
    double		arc_childtime;	/* childtime inherited along arc */
    struct arcstruct	*arc_parentlist; /* parents-of-this-child list */
    struct arcstruct	*arc_childlist;	/* children-of-this-parent list */
};
typedef struct arcstruct	arctype;

/*
 * The symbol table;
 * for each external in the specified file we gather
 * its address, the number of calls and compute its share of cpu time.
 */
struct nl {
    char		*name;		/* the name */
    pctype		value;		/* the pc entry point */
    pctype		svalue;		/* entry point aligned to histograms */
    unsigned long	sz;		/* function size */
    unsigned char	syminfo;	/* sym info */
    double		time;		/* ticks in this routine */
    double		childtime;	/* cumulative ticks in children */
    actype		ncall;		/* how many times called */
    actype		selfcalls;	/* how many calls to self */
    double		propfraction;	/* what % of time propagates */
    double		propself;	/* how much self time propagates */
    double		propchild;	/* how much child time propagates */
    bool		printflag;	/* should this be printed? */
    int			index;		/* index in the graph list */
    int			toporder;	/* graph call chain top-sort order */
    int			cycleno;	/* internal number of cycle on */
    struct nl		*cyclehead;	/* pointer to head of cycle */
    struct nl		*cnext;		/* pointer to next member of cycle */
    arctype		*parents;	/* list of caller arcs */
    arctype		*children;	/* list of callee arcs */
};
typedef struct nl	nltype;

nltype	*nl;			/* the whole namelist */
nltype	*npe;			/* the virtual end of the namelist */
sztype	nname;			/* the number of function names */

/*
 *	flag which marks a nl entry as topologically ``busy''
 *	flag which marks a nl entry as topologically ``not_numbered''
 */
#define	DFN_BUSY	-1
#define	DFN_NAN		0

/*
 *	namelist entries for cycle headers.
 *	the number of discovered cycles.
 */
nltype	*cyclenl;		/* cycle header namelist */
int	ncycle;			/* number of cycles discovered */

/*
 * The header on the gmon.out file.
 * gmon.out consists of one of these headers,
 * and then an array of ncnt samples
 * representing the discretized program counter values.
 *	this should be a struct phdr, but since everything is done
 *	as UNITs, this is in UNITs too.
 */
struct hdr {
	pctype		lowpc;
	pctype		highpc;
	pctype		ncnt;
};

struct hdr32 {
	pctype32	lowpc;
	pctype32	highpc;
	pctype32	ncnt;
};

struct hdr	h;		/* header of profiled data */

int	debug;
int 	number_funcs_toprint;

/*
 * Each discretized pc sample has
 * a count of the number of samples in its range
 */
unsigned short	*samples;

pctype	s_lowpc;		/* lowpc from the profile file */
pctype	s_highpc;		/* highpc from the profile file */
sztype	sampbytes;		/* number of bytes of samples */
sztype	nsamples;		/* number of samples */
double	actime;			/* accumulated time thus far for putprofline */
double	totime;			/* total time for all routines */
double	printtime;		/* total of time being printed */
double	scale;			/* scale factor converting samples to pc */
				/* values: each sample covers scale bytes */
off_t	ssiz;			/* size of the string table */

unsigned char	*textspace;		/* text space of a.out in core */
bool	first_file;			/* for difference option */

/*
 *	option flags, from a to z.
 */
bool	aflag;				/* suppress static functions */
bool	bflag;				/* blurbs, too */
bool	Bflag;				/* big pc's (i.e. 64 bits) */
bool	cflag;				/* discovered call graph, too */
bool	Cflag;				/* gprofing c++ -- need demangling */
bool	dflag;				/* debugging options */
bool	Dflag;				/* difference option */
bool	eflag;				/* specific functions excluded */
bool	Eflag;				/* functions excluded with time */
bool	fflag;				/* specific functions requested */
bool	Fflag;				/* functions requested with time */
bool	lflag;				/* exclude LOCAL syms in output */
bool	sflag;				/* sum multiple gmon.out files */
bool	zflag;				/* zero time/called functions, too */
bool 	nflag;				/* print only n functions in report */
bool	rflag;				/* profiling input generated by */
					/* run-time linker */

/*
 *	structure for various string lists
 */
struct stringlist {
    struct stringlist	*next;
    char		*string;
};
extern struct stringlist	*elist;
extern struct stringlist	*Elist;
extern struct stringlist	*flist;
extern struct stringlist	*Flist;

/*
 *	function declarations
 */
void	addlist(struct stringlist *, char *);
void	addarc(nltype *, nltype *, actype);
int	arccmp(arctype *, arctype *);
arctype	*arclookup(nltype *, nltype *);
void	printblurb(char *);
void	dfn(nltype *);
bool	dfn_busy(nltype *);
void	dfn_findcycle(nltype *);
bool	dfn_numbered(nltype *);
void	dfn_post_visit(nltype *);
void	dfn_pre_visit(nltype *);
void	dfn_self_cycle(nltype *);
nltype	**doarcs(void);
void	done();
void	findcalls(nltype *, pctype, pctype);
void	flatprofheader(void);
void	flatprofline(nltype *);
void	getnfile(char *);
void	gprofheader(void);
void	gprofline(nltype *);
int	main();
int	membercmp(nltype *, nltype *);
nltype	*nllookup(pctype);
bool	onlist(struct stringlist *, char *);
void	printchildren(nltype *);
void	printcycle(nltype *);
void	printgprof(nltype **);
void	printindex(void);
void	printmembers(nltype *);
void	printname(nltype *);
void	printparents(nltype *);
void	printprof(void);
void	sortchildren(nltype *);
void	sortmembers(nltype *);
void	sortparents(nltype *);
int	timecmp(nltype **, nltype **);
int	totalcmp(nltype **, nltype **);

#define	LESSTHAN	-1
#define	EQUALTO		0
#define	GREATERTHAN	1

#define	DFNDEBUG	1
#define	CYCLEDEBUG	2
#define	ARCDEBUG	4
#define	TALLYDEBUG	8
#define	TIMEDEBUG	16
#define	SAMPLEDEBUG	32
#define	ELFDEBUG	64
#define	CALLSDEBUG	128
#define	LOOKUPDEBUG	256
#define	PROPDEBUG	512
#define	ANYDEBUG	1024

#ifdef	__cplusplus
}
#endif

#endif	/* _SGS_GPROF_H */
