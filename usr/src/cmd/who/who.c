/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#pragma	ident	"@(#)who.c	1.19	97/06/21 SMI"	/* SVr4.0 1.23	*/

/*
	This program analyzes information found in /var/adm/utmp
	and the /var/adm/utmpx file if the e"x"tended version
	exists.

	Additionally information is gathered from /etc/inittab
	if requested.


	Syntax:

		who am i	Displays info on yourself

		who -a		Displays information about All
				entries in /var/adm/utmp

		who -A		Displays ACCOUNTING info - non-functional

		who -b		Displays info on last boot

		who -d		Displays info on DEAD PROCESSES

		who -H		Displays HEADERS for output

		who -l 		Displays info on LOGIN entries

		who -m 		Same as who am i

		who -p 		Displays info on PROCESSES spawned by init

		who -q		Displays short information on
				current users who LOGGED ON

		who -r		Displays info of current run-level

		who -s		Displays requested info in SHORT form

		who -t		Displays info on TIME changes

		who -T		Displays writeability of each user
				(+ writeable, - non-writeable, ? hung)

		who -u		Displays LONG info on users
				who have LOGGED ON
*/

#define		DATE_FMT	"%b %e %H:%M"
/*
 *  %b	Abbreviated month name
 *  %e	Day of month
 *  %H	hour (24-hour clock)
 *  %M  minute
 */
#include	<errno.h>
#include	<fcntl.h>
#include	<stdio.h>
#include	<string.h>
#include	<sys/types.h>
#include	<unistd.h>
#include	<stdlib.h>
#include	<sys/stat.h>
#include	<time.h>
#include	<utmp.h>
#include	<utmpx.h>
#include	<locale.h>
#include	<pwd.h>

static void process();
static void ck_file(char *);
static void dump();

extern	char *optarg;	/* for getopt()			*/
extern	int optind;	/* for getopt()			*/
extern	char *sys_errlist[]; /* error msgs for errno    */

static char	comment[80];	/* holds inittab comment	*/
static char	errmsg[1024];	/* used in sprintf for errors	*/
static int	fildes;		/* file descriptor for inittab	*/
static int	Hopt = 0;	/* 1 = who -H			*/
static char	*inittab;	/* ptr to inittab contents	*/
static char	*iinit;		/* index into inittab		*/
static int	justme = 0;	/* 1 = who am i			*/
static time_t	*lptr;
static char	*myname;	/* pointer to invoker's name 	*/
static char	*mytty;		/* holds device user is on	*/
static char	nameval[8];	/* holds invoker's name		*/
static int	number = 8;	/* number of users per -q line	*/
static int	optcnt = 0;	/* keeps count of options	*/
static char	outbuf[BUFSIZ];	/* buffer for output		*/
static char	*program;	/* holds name of this program	*/
#ifdef	XPG4
static int	aopt = 0;	/* 1 = who -a			*/
static int	lopt = 0;	/* 1 = who -l			*/
static int	dopt = 0;	/* 1 = who -d			*/
#endif	/* XPG4 */
static int	qopt = 0;	/* 1 = who -q			*/
static int	sopt = 0;	/* 1 = who -s 				*/
static struct	stat stbuf;	/* area for stat buffer		*/
static struct	stat *stbufp;	/* ptr to structure		*/
static int	terse = 1;	/* 1 = print terse msgs		*/
static int	Topt = 0;	/* 1 = who -T			*/
static time_t	timnow;		/* holds current time		*/
static int	totlusrs = 0;	/* cntr for users on system	*/
static int	uopt = 0;	/* 1 = who -u			*/
static char	user[80];	/* holds user name		*/
static struct	utmp *utmpp;	/* pointer for getutent()	*/
static struct	utmpx *utmpxp;	/* pointer for getutxent()	*/
static char	*utxname;	/* new name of utmpx file	*/
static int	validtype[UTMAXTYPE+1];	/* holds valid types	*/
static int	wrap;		/* flag to indicate wrap	*/
static char	time_buf[40];	/* holds date and time string	*/

main(argc, argv)
int	argc;
char	**argv;
{
	int	goerr = 0;	/* non-zero indicates cmd error	*/
	int	i;
	int	optsw;		/* switch for while of getopt()	*/

	(void) setlocale(LC_ALL, "");

#if	!defined(TEXT_DOMAIN)	/* Should be defined by cc -D */
#define	TEXT_DOMAIN "SYS_TEST"	/* Use this only if it weren't */
#endif
	(void) textdomain(TEXT_DOMAIN);

	validtype[USER_PROCESS] = 1;
	validtype[EMPTY] = 0;
	stbufp = &stbuf;

	/*
		Strip off path name of this command
	*/
	for (i = strlen(argv[0]); i >= 0 && argv[0][i] != '/'; --i);
	if (i >= 0) argv[0] += i+1;
	program = argv[0];

	/*
		Buffer stdout for speed
	*/
	setbuf(stdout, outbuf);

	/*
		Retrieve options specified on command line
		XCU4 - add -m option
	*/
	while ((optsw = getopt(argc, argv, "abdHlmn:pqrstTu")) != EOF) {
		optcnt++;
		switch (optsw) {

			case 'a':
				optcnt += 7;
				validtype[ACCOUNTING] = 1;
				validtype[BOOT_TIME] = 1;
				validtype[DEAD_PROCESS] = 1;
				validtype[LOGIN_PROCESS] = 1;
				validtype[INIT_PROCESS] = 1;
				validtype[RUN_LVL] = 1;
				validtype[OLD_TIME] = 1;
				validtype[NEW_TIME] = 1;
				validtype[USER_PROCESS] = 1;
#ifdef	XPG4
				aopt = 1;
#endif	/* XPG4 */
				uopt = 1;
				Hopt = 1;
				Topt = 1;
				if (!sopt) terse = 0;
				break;

			case 'A':
				validtype[ACCOUNTING] = 1;
				terse = 0;
				if (!uopt) validtype[USER_PROCESS] = 0;
				break;

			case 'b':
				validtype[BOOT_TIME] = 1;
				if (!uopt) validtype[USER_PROCESS] = 0;
				break;

			case 'd':
				validtype[DEAD_PROCESS] = 1;
				if (!uopt) validtype[USER_PROCESS] = 0;
#ifdef	XPG4
				dopt = 1;
#endif	/* XPG4 */
				break;

			case 'H':
				optcnt--; /* Don't count Header */
				Hopt = 1;
				break;

			case 'l':
				validtype[LOGIN_PROCESS] = 1;
				if (!uopt) validtype[USER_PROCESS] = 0;
#ifdef	XPG4
				lopt = 1;
#endif	/* XPG4 */
				terse = 0;
				break;
			case 'm':		/* New XCU4 option */
				justme = 1;
				break;

			case 'n':
				number = atoi(optarg);
				if (number < 1) {
					(void) fprintf(stderr, gettext(
			"%s: Number of users per line must be at least 1\n"),
					    program);
					exit(1);
				}
				break;

			case 'p':
				validtype[INIT_PROCESS] = 1;
				if (!uopt) validtype[USER_PROCESS] = 0;
				break;

			case 'q':
				qopt = 1;
				break;

			case 'r':
				validtype[RUN_LVL] = 1;
				terse = 0;
				if (!uopt) validtype[USER_PROCESS] = 0;
				break;

			case 's':
				sopt = 1;
				terse = 1;
				break;

			case 't':
				validtype[OLD_TIME] = 1;
				validtype[NEW_TIME] = 1;
				if (!uopt) validtype[USER_PROCESS] = 0;
				break;

			case 'T':
				Topt = 1;
#ifdef	XPG4
				/*
				 * XCU4 requires terse mode with -T
				 */
				terse = 1;
#else	/* XPG4 */
				terse = 0;
#endif	/* XPG4 */
				break;

			case 'u':
				uopt = 1;
				validtype[USER_PROCESS] = 1;
				if (!sopt) terse = 0;
				break;

			case '?':
				goerr++;
				break;
		}
	}
#ifdef	XPG4
	/*
	 * XCU4 changes - check for illegal sopt, Topt & aopt combination
	 */
	if (sopt == 1) {
		terse = 1;
		if (Topt == 1 || aopt == 1)
		goerr++;
	}
#endif	/* XPG4 */

	if (goerr > 0) {
#ifdef	XPG4
		/*
		 * XCU4 - slightly different usage with -s -a & -T
		 */
		(void) fprintf(stderr, gettext("\nUsage:\t%s"), program);
		(void) fprintf(stderr,
		    gettext(" -s [-bdHlmpqrtu] [utmp_like_file]\n"));

		(void) fprintf(stderr, gettext(
		    "\t%s [-abdHlmpqrtTu] [utmp_like_file]\n"), program);
#else	/* XPG4 */
		(void) fprintf(stderr, gettext(
		    "\nUsage:\t%s [-abdHlmpqrstTu] [utmp_like_file]\n"),
		    program);
#endif	/* XPG4 */
		(void) fprintf(stderr,
		    gettext("\t%s -q [-n x] [utmp_like_file]\n"), program);
		(void) fprintf(stderr, gettext("\t%s [am i]\n"), program);
		/*
		 * XCU4 changes - be explicit with "am i" options
		 */
		(void) fprintf(stderr, gettext("\t%s [am I]\n"), program);
		(void) fprintf(stderr, gettext("a\tall (Abdlprtu options)\n"));
		(void) fprintf(stderr, gettext("b\tboot time\n"));
		(void) fprintf(stderr, gettext("d\tdead processes\n"));
		(void) fprintf(stderr, gettext("H\tprint header\n"));
		(void) fprintf(stderr, gettext("l\tlogin processes\n"));
		(void) fprintf(stderr, gettext(
		    "n #\tspecify number of users per line for -q\n"));
		(void) fprintf(stderr,
		    gettext("p\tprocesses other than getty or users\n"));
		(void) fprintf(stderr, gettext("q\tquick %s\n"), program);
		(void) fprintf(stderr, gettext("r\trun level\n"));
		(void) fprintf(stderr, gettext(
		"s\tshort form of %s (no time since last output or pid)\n"),
		    program);
		(void) fprintf(stderr, gettext("t\ttime changes\n"));
		(void) fprintf(stderr, gettext(
		    "T\tstatus of tty (+ writable, - not writable, ? hung)\n"));
		(void) fprintf(stderr, gettext("u\tuseful information\n"));
		(void) fprintf(stderr,
		    gettext("m\tinformation only about current terminal\n"));
		(void) fprintf(stderr, gettext(
		    "am i\tinformation about current terminal (same as -m)\n"));
		(void) fprintf(stderr, gettext(
		    "am I\tinformation about current terminal (same as -m)\n"));
		exit(1);
	}

	/*
	 * XCU4: If -q option ignore all other options
	 */
	if (qopt == 1) {
		Hopt = 0; sopt = 0;
		Topt = 0; uopt = 0;
		justme = 0;
		validtype[ACCOUNTING] = 0;
		validtype[BOOT_TIME] = 0;
		validtype[DEAD_PROCESS] = 0;
		validtype[LOGIN_PROCESS] = 0;
		validtype[INIT_PROCESS] = 0;
		validtype[RUN_LVL] = 0;
		validtype[OLD_TIME] = 0;
		validtype[NEW_TIME] = 0;
		validtype[USER_PROCESS] = 1;
	}

	if (argc == optind + 1) {
		optcnt++;
		ck_file(argv[optind]);
		(void) utmpname(argv[optind]);
		if ((utxname = (char *) malloc(strlen(argv[optind])+2))
		    == NULL) {
			(void) sprintf(errmsg,
			    gettext("%s: Cannot allocate %d bytes"),
			    program, stbufp->st_size);
			perror(errmsg);
			exit(errno);
		}
		(void) strcpy(utxname, argv[optind]);
		(void) strcat(utxname, "x");
		(void) utmpxname(utxname);
		free(utxname);
	}

	/*
		Test for 'who am i' or 'who am I'
		XCU4 - check if justme was already set by -m option
	*/
	if (justme == 1 || (argc == 3 && strcmp(argv[1], "am") == 0 &&
	    ((argv[2][0] == 'i' || argv[2][0] == 'I') && argv[2][1] == '\0'))) {
		justme = 1;
		myname = nameval;
		(void) cuserid(myname);
		if ((mytty = ttyname(fileno(stdin))) == NULL &&
		    (mytty = ttyname(fileno(stdout))) == NULL &&
		    (mytty = ttyname(fileno(stderr))) == NULL) {
			(void) fprintf(stderr, gettext(
			"Must be attached to terminal for 'am I' option\n"));
			(void) fflush(stderr);
			exit(1);
		} else mytty += 5; /* bump past "/dev/" */
	}

	if (!terse) {
		if (Hopt)
			(void) printf(gettext(
	"NAME       LINE         TIME          IDLE    PID  COMMENTS\n"));

		timnow = time(0);

		if (stat("/etc/inittab", stbufp) == -1) {
			(void) sprintf(errmsg, gettext(
			    "%s: Cannot stat /etc/inittab"), program);
			perror(errmsg);
			exit(errno);
		}

		if ((inittab = malloc(stbufp->st_size + 1)) == NULL) {
			(void) sprintf(errmsg, gettext(
		    "%s: Cannot allocate %d bytes"), program, stbufp->st_size);
			perror(errmsg);
			exit(errno);
		}

		if ((fildes = open("/etc/inittab", O_RDONLY)) == -1) {
			(void) sprintf(errmsg,
			    gettext("%s: Cannot open /etc/inittab"), program);

			perror(errmsg);
			exit(errno);
		}

		if (read(fildes, inittab, stbufp->st_size)
		    != stbufp->st_size) {
			(void) sprintf(errmsg,
			    gettext("%s: Error reading /etc/inittab"), program);
			perror(errmsg);
			exit(errno);
		}

		inittab[stbufp->st_size] = '\0';
		iinit = inittab;
	} else {
		if (Hopt) {
#ifdef	XPG4
			if (dopt) {
				(void) printf(gettext(
			"NAME       LINE         TIME		COMMENTS\n"));
			} else {
				(void) printf(
				    gettext("NAME       LINE         TIME\n"));
			}
#else	/* XPG4 */
			(void) printf(
			    gettext("NAME       LINE         TIME\n"));
#endif	/* XPG4 */
		}
	}
	process();

	/*
		'who -q' requires EOL upon exit,
		followed by total line
	*/
	if (qopt)
		(void) printf(gettext("\n# users=%d\n"), totlusrs);
	return (0);
}

static void
dump()
{
	char	device[14];
	time_t hr;
	time_t	idle;
	time_t min;
	char	path[20];
	char	pexit;
	char	pterm;
	int	rc;
	char	w;	/* writeability indicator */

	/*
	 * Get and check user name
	 */
	if (utmpp->ut_user[0] == '\0')
		(void) strcpy(user, "   .");
	else {
		(void) strncpy(user, utmpp->ut_user, sizeof (utmpp->ut_user));
		user[sizeof (utmpp->ut_user)] = '\0';
	}
	totlusrs++;

	/*
	 * Do print in 'who -q' format
	 */
	if (qopt) {
		/*
		 * XCU4 - Use non user macro for correct user count
		 */
		if (((totlusrs - 1) % number) == 0 && totlusrs > 1)
			(void) printf("\n");
		(void) printf("%-8s ", user);
		return;
	}


	pexit = ' ';
	pterm = ' ';

	/*
		Get exit info if applicable
	*/
	if (utmpp->ut_type == RUN_LVL || utmpp->ut_type == DEAD_PROCESS) {
		pterm = utmpp->ut_exit.e_termination;
		pexit = utmpp->ut_exit.e_exit;
	}

	/*
		Massage ut_time field
	*/
	lptr = &utmpp->ut_time;
	(void) cftime(time_buf, DATE_FMT, lptr);

	/*
		Get and massage device
	*/
	if (utmpp->ut_line[0] == '\0')
		(void) strcpy(device, "     .");
	else {
		(void) strncpy(device, utmpp->ut_line, sizeof (utmpp->ut_line));
		device[sizeof (utmpp->ut_line)] = '\0';
	}

	/*
		Get writeability if requested
		XCU4 - only print + or - for user processes
	*/
	if (Topt && (utmpp->ut_type == USER_PROCESS)) {
		w = '-';
		(void) strcpy(path, "/dev/");
		(void) strncpy(path + 5, utmpp->ut_line,
		    sizeof (utmpp->ut_line));
		path[5 + sizeof (utmpp->ut_line)] = '\0';

		if ((rc = stat(path, stbufp)) == -1) w = '?';
		else if ((stbufp->st_mode & S_IWOTH) ||
		    (stbufp->st_mode & S_IWGRP))  /* Check group & other */
			w = '+';

	} else
		w = ' ';

	/*
		Print the TERSE portion of the output
	*/
	(void) printf("%-8s %c %-12s %s", user, w, device, time_buf);

	if (!terse) {
		(void) strcpy(path, "/dev/");
		(void) strncpy(path + 5, utmpp->ut_line,
		    sizeof (utmpp->ut_line));
		path[5 + sizeof (utmpp->ut_line)] = '\0';

		/*
			Stat device for idle time
			(Don't complain if you can't)
		*/
		if ((rc = stat(path, stbufp)) != -1) {
			idle = timnow - stbufp->st_mtime;
			hr = idle/3600;
			min = (unsigned)(idle/60)%60;
			if (hr == 0 && min == 0)
				(void) printf(gettext("   .  "));
			else {
				if (hr < 24)
					(void) printf(" %2d:%2.2d", (int) hr,
					    (int) min);
				else
					(void) printf(gettext("  old "));
			}
		}

		/*
			Add PID for verbose output
		*/
		if (utmpp->ut_type != BOOT_TIME && utmpp->ut_type != RUN_LVL &&
		    utmpp->ut_type != ACCOUNTING)
			(void) printf("  %5d", utmpp->ut_pid);

		/*
			Handle /etc/inittab comment
		*/
		if (utmpp->ut_type == DEAD_PROCESS) {
			(void) printf(gettext("  id=%4.4s "),
			    utmpp->ut_id);
			(void) printf(gettext("term=%-3d "), pterm);
			(void) printf(gettext("exit=%d  "), pexit);
		} else if (utmpp->ut_type != INIT_PROCESS) {
			/*
				Search for each entry in inittab
				string. Keep our place from
				search to search to try and
				minimize the work. Wrap once if needed
				for each entry.
			*/
			wrap = 0;
			/*
				Look for a line beginning with
				utmpp->ut_id
			*/
			while ((rc = strncmp(utmpp->ut_id, iinit,
			    strcspn(iinit, ":"))) != 0) {
				for (; *iinit != '\n'; iinit++);
				iinit++;

				/*
					Wrap once if necessary to
					find entry in inittab
				*/
				if (*iinit == '\0') {
					if (!wrap) {
						iinit = inittab;
						wrap = 1;
					}
				}
			}

			if (*iinit != '\0') {
				/*
					We found our entry
				*/
				for (iinit++; *iinit != '#' &&
				*iinit != '\n'; iinit++);

				if (*iinit == '#') {
					for (iinit++; *iinit == ' ' ||
					*iinit == '\t'; iinit++);
					for (rc = 0; *iinit != '\n'; iinit++)
						comment[rc++] = *iinit;
					comment[rc] = '\0';
				} else
					(void) strcpy(comment, " ");

				(void) printf("  %s", comment);
			} else
				iinit = inittab;	/* Reset pointer */
		}
		if (utmpp->ut_type == INIT_PROCESS)
			(void) printf(gettext("  id=%4.4s"), utmpp->ut_id);
	}
#ifdef	XPG4
	else
		if (dopt && utmpp->ut_type == DEAD_PROCESS) {
			(void) printf(gettext("\tterm=%-3d "), pterm);
			(void) printf(gettext("exit=%d  "), pexit);
		}
#endif	/* XPG4 */


	/*
		Handle RUN_LVL process - If no alt. file - Only one!
	*/
	if (utmpp->ut_type == RUN_LVL) {
		(void) printf("     %c  %5d  %c", pterm, utmpp->ut_pid, pexit);
		if (optcnt == 1 && !validtype[USER_PROCESS]) {
			(void) printf("\n");
			exit(0);
		}
	}

	/*
		Handle BOOT_TIME process -  If no alt. file - Only one!
	*/
	if (utmpp->ut_type == BOOT_TIME) {
		if (optcnt == 1 && !validtype[USER_PROCESS]) {
			(void) printf("\n");
			exit(0);
		}
	}

	/*
		Get remote host from utmpx structure
	*/
	if (utmpxp && utmpxp->ut_host[0])
		(void) printf("\t(%.*s)", sizeof (utmpxp->ut_host),
		    utmpxp->ut_host);

	/*
		Now, put on the trailing EOL
	*/
	(void) printf("\n");
}

static void
process()
{
	struct passwd *pwp;
	int i = 0;
	char *ttname;

	/*
		Loop over each entry in /var/adm/utmp and utmpx
	*/

	while ((utmpp = getutent()) != NULL) {
		utmpxp = getutxent();
#ifdef DEBUG
	(void) printf(
	    "ut_user '%s'\nut_id '%s'\nut_line '%s'\nut_type '%d'\n\n",
	    utmpp->ut_user, utmpp->ut_id, utmpp->ut_line, utmpp->ut_type);
#endif
		if (utmpp->ut_type <= UTMAXTYPE) {
			/*
				Handle "am i"
			*/
			if (justme) {
				if (
		strncmp(myname, utmpp->ut_user, sizeof (utmpp->ut_user)) == 0 &&
		strncmp(mytty, utmpp->ut_line, sizeof (utmpp->ut_line)) == 0 &&
		utmpp->ut_type == USER_PROCESS) {
					/*
					 * we have have found ourselves
					 * in the utmp file and the entry
					 * is a user process, this is not
					 * meaningful otherwise
					 *
					 */

					dump();
					exit(0);
				}
				continue;
			}

			/*
				Print the line if we want it
			*/
			if (validtype[utmpp->ut_type]) {
#ifdef	XPG4
				if ((lopt) && (utmpp->ut_line[0] == '\0'))
					continue;
#endif	/* XPG4 */
				dump();
			}
		} else {
			(void) fprintf(stderr,
			    gettext("%s: Error --- entry has ut_type of %d\n"),
			    program, utmpp->ut_type);
			(void) fprintf(stderr,
			    gettext(" when maximum is %d\n"), UTMAXTYPE);
		}

	}

	/*
	 * If justme is set at this point than the utmp entry
	 * was not found.
	 */
	if (justme) {
		static struct utmp utmpt;

		pwp = getpwuid(geteuid());

		if (pwp != NULL)
			while (i < sizeof (utmpt.ut_user) && *pwp->pw_name != 0)
				utmpt.ut_user[i++] = *pwp->pw_name++;

		ttname = ttyname(1);

		i = 0;
		if (ttname != NULL)
			while (i < sizeof (utmpt.ut_line) && *ttname != 0)
				utmpt.ut_line[i++] = *ttname++;

		utmpt.ut_id[0] = 0;
		utmpt.ut_pid = getpid();
		utmpt.ut_type = USER_PROCESS;
		(void) time(&utmpt.ut_time);
		utmpp = &utmpt;
		utmpxp = 0;
		dump();
		exit(0);
	}
}

/*
	This routine checks the following:

	1.	File exists

	2.	We have read permissions

	3.	It is a multiple of utmp entries in size

	Failing any of these conditions causes who(1) to
	abort processing.

	4.	If file is empty we exit right away as there
		is no info to report on.

	This routine does not check utmpx files.
*/
static void
ck_file(name)
char	*name;
{
	FILE	*file;
	struct	stat sbuf;
	int	rc;

	/*
		Does file exist? Do stat to check, and save structure
		so that we can check on the file's size later on.
	*/
	if ((rc = stat(name, &sbuf)) == -1) {
		(void) sprintf(errmsg,
		    gettext("%s: Cannot stat file '%s'"), program, name);
		perror(errmsg);
		exit(1);
	}

	/*
		The only real way we can be sure we can access the
		file is to try. If we succeed then we close it.
	*/
	if ((file = fopen(name, "r")) == NULL) {
		(void) sprintf(errmsg, gettext("%s: Cannot open file '%s'"),
		    program, name);
		perror(errmsg);
		exit(1);
	}
	(void) fclose(file);

	/*
		If the file is empty, we are all done.
	*/
	if (!sbuf.st_size) exit(0);

	/*
		Make sure the file is a utmp file.
		We can only check for size being a multiple of
		utmp structures in length.
	*/
	rc = sbuf.st_size % sizeof (struct utmp);
	if (rc) {
		fprintf(stderr, gettext("%s: File '%s' is not a utmp file\n"),
		    program, name);
		exit(1);
	}
}
