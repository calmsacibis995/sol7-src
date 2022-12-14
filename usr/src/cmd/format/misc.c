
/*
 * Copyright (c) 1991-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef lint
#pragma ident	"@(#)misc.c	1.13	98/01/24 SMI"
#endif	lint

/*
 * This file contains miscellaneous routines.
 */
#include "global.h"

#include <stdlib.h>
#include <signal.h>
#include <malloc.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/time.h>
#include <ctype.h>
#include <termio.h>
#include "misc.h"
#include "analyze.h"
#include "label.h"
#include "startup.h"


struct	env *current_env;		/* ptr to current environment */
int	stop_pending = 0;		/* ctrl-Z is pending */
struct	ttystate ttystate;		/* tty info */
int	aborting = 0;			/* in process of aborting */

/*
 * For 4.x, limit the choices of valid disk names to this set.
 */
static char		*disk_4x_identifiers[] = { "sd", "id"};
#define	N_DISK_4X_IDS	(sizeof (disk_4x_identifiers)/sizeof (char *))


/*
 * This is the list of legal inputs for all yes/no questions.
 */
char	*confirm_list[] = {
	"yes",
	"no",
	NULL,
};

/*
 * This routine is a wrapper for malloc.  It allocates pre-zeroed space,
 * and checks the return value so the caller doesn't have to.
 */
void *
zalloc(count)
	int	count;
{
	void	*ptr;

	if ((ptr = (void *) calloc(1, (unsigned)count)) == NULL) {
		err_print("Error: unable to calloc more space.\n");
		fullabort();
	}
	return (ptr);
}

/*
 * This routine is a wrapper for realloc.  It reallocates the given
 * space, and checks the return value so the caller doesn't have to.
 * Note that the any space added by this call is NOT necessarily
 * zeroed.
 */
void *
rezalloc(ptr, count)
	void	*ptr;
	int	count;
{
	void	*new_ptr;


	if ((new_ptr = (void *) realloc((char *)ptr,
				(unsigned)count)) == NULL) {
		err_print("Error: unable to realloc more space.\n");
		fullabort();
	}
	return (new_ptr);
}

/*
 * This routine is a wrapper for free.
 */
void
destroy_data(data)
	char	*data;
{
	free((char *)data);
}

#ifdef	not
/*
 * This routine takes the space number returned by an ioctl call and
 * returns a mnemonic name for that space.
 */
char *
space2str(space)
	u_int	space;
{
	char	*name;

	switch (space&SP_BUSMASK) {
	    case SP_VIRTUAL:
		name = "virtual";
		break;
	    case SP_OBMEM:
		name = "obmem";
		break;
	    case SP_OBIO:
		name = "obio";
		break;
	    case SP_MBMEM:
		name = "mbmem";
		break;
	    case SP_MBIO:
		name = "mbio";
		break;
	    default:
		err_print("Error: unknown address space type encountered.\n");
		fullabort();
	}
	return (name);
}
#endif	not

/*
 * This routine asks the user the given yes/no question and returns
 * the response.
 */
int
check(question)
	char	*question;
{
	int		answer;
	u_ioparam_t	ioparam;

	/*
	 * If we are running out of a command file, assume a yes answer.
	 */
	if (option_f)
		return (0);
	/*
	 * Ask the user.
	 */
	ioparam.io_charlist = confirm_list;
	answer = input(FIO_MSTR, question, '?', &ioparam,
	    (int *)NULL, DATA_INPUT);
	return (answer);
}

/*
 * This routine aborts the current command.  It is called by a ctrl-C
 * interrupt and also under certain error conditions.
 */
/*ARGSUSED*/
void
cmdabort(sig)
	int	sig;
{

	/*
	 * If there is no usable saved environment and we are running
	 * from a command file, we gracefully exit.  This allows the
	 * user to interrupt the program even when input is from a file.
	 * If there is no usable environment and we are running with a user,
	 * just ignore the interruption.
	 */
	if (current_env == NULL || !(current_env->flags & ENV_USE)) {
		if (option_f)
			fullabort();
		else
			return;
	}
	/*
	 * If we are in a critical zone, note the attempt and return.
	 */
	if (current_env->flags & ENV_CRITICAL) {
		current_env->flags |= ENV_ABORT;
		return;
	}
	/*
	 * All interruptions when we are running out of a command file
	 * cause the program to gracefully exit.
	 */
	if (option_f)
		fullabort();
	fmt_print("\n");
	/*
	 * Clean up any state left by the interrupted command.
	 */
	cleanup();
	/*
	 * Jump to the saved environment.
	 */
	longjmp(current_env->env, 0);
}

/*
 * This routine implements the ctrl-Z suspend mechanism.  It is called
 * when a suspend signal is received.
 */
/*ARGSUSED*/
void
onsusp(sig)
	int	sig;
{
	int		fix_term;
#ifdef	NOT_DEF
	sigset_t	sigmask;
#endif	NOT_DEF

	/*
	 * If we are in a critical zone, note the attempt and return.
	 */
	if (current_env != NULL && current_env->flags & ENV_CRITICAL) {
		stop_pending = 1;
		return;
	}
	/*
	 * If the terminal is mucked up, note that we will need to
	 * re-muck it when we start up again.
	 */
	fix_term = ttystate.ttyflags;
	fmt_print("\n");
	/*
	 * Clean up any state left by the interrupted command.
	 */
	cleanup();
#ifdef	NOT_DEF
	/* Investigate whether all this is necessary */
	/*
	 * Stop intercepting the suspend signal, then send ourselves one
	 * to cause us to stop.
	 */
	sigmask.sigbits[0] = (u_long) 0xffffffff;
	if (sigprocmask(SIG_SETMASK, &sigmask, (sigset_t *)NULL) == -1)
		err_print("sigprocmask failed %d\n", errno);
#endif	NOT_DEF
	(void) signal(SIGTSTP, SIG_DFL);
	(void) kill(0, SIGTSTP);
	/*
	 * PC stops here
	 */
	/*
	 * We are started again.  Set us up to intercept the suspend
	 * signal once again.
	 */
	(void) signal(SIGTSTP, onsusp);
	/*
	 * Re-muck the terminal if necessary.
	 */
	if (fix_term & TTY_ECHO_OFF)
		echo_off();
	if (fix_term & TTY_CBREAK_ON)
		charmode_on();
}

/*
 * This routine implements the timing function used during long-term
 * disk operations (e.g. formatting).  It is called when an alarm signal
 * is received.
 */
/*ARGSUSED*/
void
onalarm(sig)
	int	sig;
{
}


/*
 * This routine gracefully exits the program.
 */
void
fullabort()
{

	fmt_print("\n");
	/*
	 * Clean up any state left by an interrupted command.
	 * Avoid infinite loops caused by a clean-up
	 * routine failing again...
	 */
	if (!aborting) {
		aborting = 1;
		cleanup();
	}
	exit(-1);
	/*NOTREACHED*/
}

/*
 * This routine cleans up the state of the world.  It is a hodge-podge
 * of kludges to allow us to interrupt commands whenever possible.
 */
void
cleanup()
{

	/*
	 * Lock out interrupts to avoid recursion.
	 */
	enter_critical();
	/*
	 * Fix up the tty if necessary.
	 */
	if (ttystate.ttyflags & TTY_CBREAK_ON) {
		charmode_off();
	}
	if (ttystate.ttyflags & TTY_ECHO_OFF) {
		echo_on();
	}

	/*
	 * If the defect list is dirty, write it out.
	 */
	if (cur_list.flags & LIST_DIRTY) {
		cur_list.flags = 0;
		if (!EMBEDDED_SCSI)
			write_deflist(&cur_list);
	}
	/*
	 * If the label is dirty, write it out.
	 */
	if (cur_flags & LABEL_DIRTY) {
		cur_flags &= ~LABEL_DIRTY;
		(void) write_label();
	}
	/*
	 * If we are logging and just interrupted a scan, print out
	 * some summary info to the log file.
	 */
	if (log_file && scan_cur_block >= 0) {
		pr_dblock(log_print, scan_cur_block);
		log_print("\n");
	}
	if (scan_blocks_fixed >= 0)
		fmt_print("Total of %d defective blocks repaired.\n",
		    scan_blocks_fixed);
	scan_cur_block = scan_blocks_fixed = -1;
	exit_critical();
}

/*
 * This routine causes the program to enter a critical zone.  Within the
 * critical zone, no interrupts are allowed.  Note that calls to this
 * routine for the same environment do NOT nest, so there is not
 * necessarily pairing between calls to enter_critical() and exit_critical().
 */
void
enter_critical()
{

	/*
	 * If there is no saved environment, interrupts will be ignored.
	 */
	if (current_env == NULL)
		return;
	/*
	 * Mark the environment to be in a critical zone.
	 */
	current_env->flags |= ENV_CRITICAL;
}

/*
 * This routine causes the program to exit a critical zone.  Note that
 * calls to enter_critical() for the same environment do NOT nest, so
 * one call to exit_critical() will erase any number of such calls.
 */
void
exit_critical()
{

	/*
	 * If there is a saved environment, mark it to be non-critical.
	 */
	if (current_env != NULL)
		current_env->flags &= ~ENV_CRITICAL;
	/*
	 * If there is a stop pending, execute the stop.
	 */
	if (stop_pending) {
		stop_pending = 0;
		onsusp(SIGSTOP);
	}
	/*
	 * If there is an abort pending, execute the abort.
	 */
	if (current_env == NULL)
		return;
	if (current_env->flags & ENV_ABORT) {
		current_env->flags &= ~ENV_ABORT;
		cmdabort(SIGINT);
	}
}

/*
 * This routine turns off echoing on the controlling tty for the program.
 */
void
echo_off()
{
	/*
	 * Open the tty and store the file pointer for later.
	 */
	if (ttystate.ttyflags == 0) {
		if ((ttystate.ttyfile = open("/dev/tty",
					O_RDWR | O_NDELAY)) < 0) {
			err_print("Unable to open /dev/tty.\n");
			fullabort();
		}
	}
	/*
	 * Get the parameters for the tty, turn off echoing and set them.
	 */
	if (tcgetattr(ttystate.ttyfile, &ttystate.ttystate) < 0) {
		err_print("Unable to get tty parameters.\n");
		fullabort();
	}
	ttystate.ttystate.c_lflag &= ~ECHO;
	if (tcsetattr(ttystate.ttyfile, TCSANOW, &ttystate.ttystate) < 0) {
		err_print("Unable to set tty to echo off state.\n");
		fullabort();
	}

	/*
	 * Remember that we've successfully turned
	 * ECHO mode off, so we know to fix it later.
	 */
	ttystate.ttyflags |= TTY_ECHO_OFF;
}

/*
 * This routine turns on echoing on the controlling tty for the program.
 */
void
echo_on()
{

	/*
	 * Using the saved parameters, turn echoing on and set them.
	 */
	ttystate.ttystate.c_lflag |= ECHO;
	if (tcsetattr(ttystate.ttyfile, TCSANOW, &ttystate.ttystate) < 0) {
		err_print("Unable to set tty to echo on state.\n");
		fullabort();
	}
	/*
	 * Close the tty and mark it ok again.
	 */
	ttystate.ttyflags &= ~TTY_ECHO_OFF;
	if (ttystate.ttyflags == 0) {
		(void) close(ttystate.ttyfile);
	}
}

/*
 * This routine turns off single character entry mode for tty.
 */
void
charmode_on()
{

	/*
	 * If tty unopened, open the tty and store the file pointer for later.
	 */
	if (ttystate.ttyflags == 0) {
		if ((ttystate.ttyfile = open("/dev/tty",
					O_RDWR | O_NDELAY)) < 0) {
			err_print("Unable to open /dev/tty.\n");
			fullabort();
		}
	}
	/*
	 * Get the parameters for the tty, turn on char mode.
	 */
	if (tcgetattr(ttystate.ttyfile, &ttystate.ttystate) < 0) {
		err_print("Unable to get tty parameters.\n");
		fullabort();
	}
	ttystate.vmin = ttystate.ttystate.c_cc[VMIN];
	ttystate.vtime = ttystate.ttystate.c_cc[VTIME];

	ttystate.ttystate.c_lflag &= ~ICANON;
	ttystate.ttystate.c_cc[VMIN] = 1;
	ttystate.ttystate.c_cc[VTIME] = 0;

	if (tcsetattr(ttystate.ttyfile, TCSANOW, &ttystate.ttystate) < 0) {
		err_print("Unable to set tty to cbreak on state.\n");
		fullabort();
	}

	/*
	 * Remember that we've successfully turned
	 * CBREAK mode on, so we know to fix it later.
	 */
	ttystate.ttyflags |= TTY_CBREAK_ON;
}

/*
 * This routine turns on single character entry mode for tty.
 * Note, this routine must be called before echo_on.
 */
void
charmode_off()
{

	/*
	 * Using the saved parameters, turn char mode on.
	 */
	ttystate.ttystate.c_lflag |= ICANON;
	ttystate.ttystate.c_cc[VMIN] = ttystate.vmin;
	ttystate.ttystate.c_cc[VTIME] = ttystate.vtime;
	if (tcsetattr(ttystate.ttyfile, TCSANOW, &ttystate.ttystate) < 0) {
		err_print("Unable to set tty to cbreak off state.\n");
		fullabort();
	}
	/*
	 * Close the tty and mark it ok again.
	 */
	ttystate.ttyflags &= ~TTY_CBREAK_ON;
	if (ttystate.ttyflags == 0) {
		(void) close(ttystate.ttyfile);
	}
}


/*
 * Allocate space for and return a pointer to a string
 * on the stack.  If the string is null, create
 * an empty string.
 * Use destroy_data() to free when no longer used.
 */
char *
alloc_string(s)
	char	*s;
{
	char	*ns;

	if (s == (char *)NULL) {
		ns = (char *)zalloc(1);
	} else {
		ns = (char *)zalloc(strlen(s) + 1);
		(void) strcpy(ns, s);
	}
	return (ns);
}



/*
 * This function can be used to build up an array of strings
 * dynamically, with a trailing NULL to terminate the list.
 *
 * Parameters:
 *	argvlist:  a pointer to the base of the current list.
 *		   does not have to be initialized.
 *	size:	   pointer to an integer, indicating the number
 *		   of string installed in the list.  Must be
 *		   initialized to zero.
 *	alloc:	   pointer to an integer, indicating the amount
 *		   of space allocated.  Must be initialized to
 *		   zero.  For efficiency, we allocate the list
 *		   in chunks and use it piece-by-piece.
 *	str:	   the string to be inserted in the list.
 *		   A copy of the string is malloc'ed, and
 *		   appended at the end of the list.
 * Returns:
 *	a pointer to the possibly-moved argvlist.
 *
 * No attempt to made to free unused memory when the list is
 * completed, although this would not be hard to do.  For
 * reasonably small lists, this should suffice.
 */
#define	INITIAL_LISTSIZE	32
#define	INCR_LISTSIZE		32

char **
build_argvlist(argvlist, size, alloc, str)
	char	**argvlist;
	int	*size;
	int	*alloc;
	char	*str;
{
	if (*size + 2 > *alloc) {
		if (*alloc == 0) {
			*alloc = INITIAL_LISTSIZE;
			argvlist = (char **)
				zalloc(sizeof (char *) * (*alloc));
		} else {
			*alloc += INCR_LISTSIZE;
			argvlist = (char **)
				rezalloc((void *) argvlist,
				sizeof (char *) * (*alloc));
		}
	}

	argvlist[*size] = alloc_string(str);
	*size += 1;
	argvlist[*size] = NULL;

	return (argvlist);
}


/*
 * Useful parsing macros
 */
#define	must_be(s, c)		if (*s++ != c) return (0)
#define	skip_digits(s)		while (isdigit(*s)) s++


/*
 * Return true if a device name matches the conventions
 * for the particular system.
 */
int
conventional_name(char *name)
{
	must_be(name, 'c');
	skip_digits(name);
	if (*name == 't') {
		name++;
		skip_digits(name);
	}
	must_be(name, 'd');
	skip_digits(name);
	must_be(name, 's');
	skip_digits(name);
	return (*name == 0);
}

/*
 * Return true if a device name matches the intel physical name conventions
 * for the particular system.
 */
int
fdisk_physical_name(char *name)
{
	must_be(name, 'c');
	skip_digits(name);
	if (*name == 't') {
		name++;
		skip_digits(name);
	}
	must_be(name, 'd');
	skip_digits(name);
	must_be(name, 'p');
	skip_digits(name);
	return (*name == 0);
}

/*
 * Return true if a device name matches the conventions
 * for a "whole disk" name for the particular system.
 * The name in this case must match exactly that which
 * would appear in the device directory itself.
 */
int
whole_disk_name(name)
	char	*name;
{
	must_be(name, 'c');
	skip_digits(name);
	if (*name == 't') {
		name++;
		skip_digits(name);
	}
	must_be(name, 'd');
	skip_digits(name);
	must_be(name, 's');
	must_be(name, '2');
	return (*name == 0);
}


/*
 * Return true if a name is in the internal canonical form
 */
int
canonical_name(name)
	char	*name;
{
	must_be(name, 'c');
	skip_digits(name);
	if (*name == 't') {
		name++;
		skip_digits(name);
	}
	must_be(name, 'd');
	skip_digits(name);
	return (*name == 0);
}


/*
 * Return true if a name is in the internal canonical form for 4.x
 * Used to support 4.x naming conventions under 5.0.
 */
int
canonical4x_name(name)
	char	*name;
{
	char    **p;
	int	i;

	p = disk_4x_identifiers;
	for (i = N_DISK_4X_IDS; i > 0; i--, p++) {
		if (match_substr(name, *p)) {
			name += strlen(*p);
			break;
		}
	}
	if (i == 0)
		return (0);
	skip_digits(name);
	return (*name == 0);
}


/*
 * Map a conventional name into the internal canonical form:
 *
 *	/dev/rdsk/c0t0d0s0 -> c0t0d0
 */
void
canonicalize_name(dst, src)
	char	*dst;
	char	*src;
{
	char	*s;

	/*
	 * Copy from the 'c' to the end to the destination string...
	 */
	s = strchr(src, 'c');
	if (s != NULL) {
		(void) strcpy(dst, s);
		/*
		 * Remove the trailing slice (partition) reference
		 */
		s = dst + strlen(dst) - 2;
		if (*s == 's') {
			*s = 0;
		}
	} else {
		*dst = 0;	/* be tolerant of garbage input */
	}
}


/*
 * Return true if we find an occurance of s2 at the
 * beginning of s1.  We don't have to match all of
 * s1, but we do have to match all of s2
 */
int
match_substr(s1, s2)
	char    *s1;
	char    *s2;
{
	while (*s2 != 0) {
		if (*s1++ != *s2++)
		return (0);
	}

	return (1);
}


/*
 * Dump a structure in hexadecimal, for diagnostic purposes
 */
#define	BYTES_PER_LINE		16

void
dump(hdr, src, nbytes, format)
	char	*hdr;
	caddr_t	src;
	int	nbytes;
	int	format;
{
	int	i;
	int	n;
	char	*p;
	char	s[256];

	assert(format == HEX_ONLY || format == HEX_ASCII);

	strcpy(s, hdr);
	for (p = s; *p; p++) {
		*p = ' ';
	}

	p = hdr;
	while (nbytes > 0) {
		err_print("%s", p);
		p = s;
		n = min(nbytes, BYTES_PER_LINE);
		for (i = 0; i < n; i++) {
			err_print("%02x ", src[i] & 0xff);
		}
		if (format == HEX_ASCII) {
			for (i = BYTES_PER_LINE-n; i > 0; i--) {
				err_print("   ");
			}
			err_print("    ");
			for (i = 0; i < n; i++) {
				err_print("%c",
					isprint(src[i]) ? src[i] : '.');
			}
		}
		err_print("\n");
		nbytes -= n;
		src += n;
	}
}


float
bn2mb(daddr_t nblks)
{
#if ! defined(_NO_LONGLONG)
	long long	n;
#else
	long		n;
#endif

	n = nblks;
	return ((float)((float)n/(float)(1024*1024)) * DEV_BSIZE);
}


daddr_t
mb2bn(float mb)
{
#if ! defined(_NO_LONGLONG)
	long long	n;
#else
	long		n;
#endif

#if ! defined(_NO_LONGLONG)
	n = (long long)mb;
#else
	n = (long)mb;
#endif
	n *= (1024 * 1024 / DEV_BSIZE);
	return ((daddr_t)n);
}

float
bn2gb(daddr_t nblks)
{
#if ! defined(_NO_LONGLONG)
	long long	n;
#else
	long		n;
#endif
	float		temp;

	n = nblks;
	temp = (float)n/(float)(1024*1024);
	return (((float)(temp)/(float)1024)*DEV_BSIZE);

}


daddr_t
gb2bn(float gb)
{
#if ! defined(_NO_LONGLONG)
	long long	n;
#else
	long		n;
#endif

	n = gb * 1024 * (1024 * 1024 / DEV_BSIZE);
	return ((daddr_t)n);
}

/*
 * This routine finds out the number of lines (rows) in a terminal
 * window. The default value of TTY_LINES is returned on error.
 */
int
get_tty_lines()
{
	int	tty_lines = TTY_LINES;
	struct	winsize	winsize;

	if ((option_f == (char *)NULL) && isatty(0) == 1 && isatty(1) == 1) {
		/*
		 * We have a real terminal for std input and output
		 */
		winsize.ws_row = 0;
		if (ioctl(1, TIOCGWINSZ, &winsize) == 0) {
			if (winsize.ws_row > 2) {
				/*
				 * Should be atleast 2 lines, for division
				 * by (tty_lines - 1, tty_lines - 2) to work.
				 */
				tty_lines = winsize.ws_row;
			}
		}
	}
	return (tty_lines);
}
