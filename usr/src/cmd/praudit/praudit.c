#ifndef lint
static char sccsid[] = "@(#)praudit.c 1.35 97/11/25 SMI";
#endif

/*
 * Copyright (c) 1993 by Sun Microsystems, Inc.
 */

#include <ctype.h>
#include <dirent.h>
#include <grp.h>
#include <libintl.h>
#include <limits.h>
#include <locale.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/inttypes.h>
#include <sys/file.h>
#include <sys/param.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/acl.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <netinet/in.h>
#include <sys/tiuser.h>
#include <rpc/types.h>
#include <rpc/auth.h>
#include <rpc/auth_unix.h>
#include <rpc/svc.h>
#include <rpc/xdr.h>
#include <nfs/nfs.h>
#include <sys/fs/ufs_quota.h>
#include <sys/time.h>
#include <sys/mkdev.h>
#include <unistd.h>

#include <bsm/audit.h>
#include <bsm/audit_record.h>
#include <bsm/libbsm.h>

#include "praudit.h"

#include <netdb.h>
#include <arpa/inet.h>

#if !defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN "SUNW_OST_OSCMD"
#endif

extern char	*sys_errlist[];
extern int	sys_nerr;

static char	*anchor_path();
static char	*bu2string();
static char	*collapse_path();
static int	convert_char_to_string();
static int	convert_int32_to_string();
static int	convert_int64_to_string();
static int	convert_short_to_string();
static void	convertascii();
static int	convertbinary();
static char	*eventmodifier2string();
static int	findfieldwidth();
static char	*get_Hname();
static char	*hexconvert();
static char	*htp2string();
static char	*pa_gettokenstring();
static int	pa_geti();
static int	pa_process_record();
static int	pa_tokenid();
static int	pa_print();
static int	pa_adr_int32();
static int	pa_adr_int64();
static int	pa_time32();
static int	pa_time64();
static int	pa_msec32();
static int	pa_msec64();
static int	pa_adr_string();
static int	pa_adr_u_int32();
static int	pa_adr_u_int64();
static int	pa_adr_byte();
static int	pa_event_type();
static int	pa_event_modifier();
static int	pa_adr_int32hex();
static int	pa_adr_int64hex();
static int	pa_pw_uid();
static int	pa_gr_uid();
static int	pa_pw_uid_gr_uid();
static int	pa_hostname();
static int	pa_adr_u_short();
static int	pa_tid32();
static int	pa_tid64();
static int	pa_adr_charhex();
static int	pa_adr_short();
static int	pa_adr_shorthex();
static int	pa_mode();
static int	pa_path();
static int	pa_cmd();

#ifdef SunOS_CMW
static char	*xobj2str();
static char	*xproto2str();
#endif
/*
 * ----------------------------------------------------------------------
 * praudit.c  -  display contents of audit trail file
 *
 * praudit() - main control
 * input:    - command line input:   praudit -r -s -l -ddelim. filename(s)
 * output:
 * ----------------------------------------------------------------------
 */

main(argc, argv)
int	argc;
char	**argv;
{
	int	i = 0, retstat;
	char	*names[MAXFILENAMES], fname[MAXFILELEN];

	/* Internationalization */
	(void) setlocale(LC_ALL, "");
	(void) textdomain(TEXT_DOMAIN);
	/*
	 * get audit file names
	 */
	if ((retstat = pa_geti(&argc, argv, names)) == 0) {
		do {
			retstat = 0;
			/*
			 * process each audit file
			 */
			if (file_mode) {
				(void) strcpy(fname, names[i]);
				if (freopen(fname, "r", stdin) == NULL) {
(void) fprintf(stderr, gettext("praudit: Can't assign %s to stdin.\n"), fname);
					retstat = -1;
				}
			} else
				(void) strcpy(fname, "stdin");
			/*
			 * process audit file header
			 */
			if (retstat == 0) {
				/*
				 * first get an adr pointer to the current
				 * audit file
				 */
				audit_adr = (adr_t *)malloc(sizeof (adr_t));
				(void) adrf_start(audit_adr, stdin);

				while (retstat == 0) {
					if (adrf_char(audit_adr,
					    (char *)&tokenid, 1) == 0)
						retstat = pa_process_record(
							tokenid);
					else
						retstat = -1;
				}
				(void) free(audit_adr);
			}
		} while ((++i < argc) && retstat >= 0);
	}
	if (retstat == -2) {
		(void) printf(gettext(
		"\nusage: praudit [-r/-s] [-l] [-ddel] [-c] filename...\n"));
		retstat = -1;
	}
	if (retstat == 1)
		retstat = 0;
	(void) exit(retstat);
}


/*
 * -------------------------------------------------------------------
 * pa_geti() - get command line flags and file names
 * input:    - praudit [-r]/[-s] [-l] [-ddel] [-c] {audit file names}
 * output:   - {audit file names}
 * globals set:	format:		RAWM or PRA_SHORT or DEFAULT
 *		ONELINE:    1 if output is unformatted
 *			SEPARATOR:  default, ",", set here if
 *				user specified
 * NOTE: no changes required here for new audit record format
 * -------------------------------------------------------------------
 */
int
pa_geti(argc, argv, names)
int	*argc;
char	**argv;
char	**names;
{
	int	i, count = 0, returnstat = 0, gotone = 0;

	/*
		 * check for flags
		 */
	for (i = 0, --(*argc); i < 3 && *argc > 0 && (*++argv)[0] == '-' &&
		returnstat == 0; i++, --(*argc)) {
		switch ((*argv)[1]) {
		case 'c':
			CACHE = 1; /* turn off cache */
			++gotone;
			break;
		case 'r':
			if (format == SHORTM)
				returnstat = -2;
			else {
				format = RAWM;
				++gotone;
			}
			break;
		case 's':
			if (format == RAWM)
				returnstat = -2;
			else {
				format = SHORTM;
				++gotone;
			}
			break;
		case 'l':
			ONELINE = 1;
			++gotone;
			break;
		case 'd':
			++gotone;
			if (strlen(*argv) != 2) {
				if (strlen(*argv + 2) < sizeof (SEPARATOR))
					(void) strncpy(SEPARATOR, *argv + 2,
						SEP_SIZE);
				else {
(void) fprintf(stderr,
	gettext("praudit: Delimiter too long.  Using default.\n"));
				}
			}
			break;
		default:
			if (gotone == 0)
				returnstat = -2;
			break;
		}
	}

	if (gotone != 3)
		--argv;

	if (returnstat == 0 && *argc > 0) {
		/*
		 * get file names from command line
		 */
		do {
			if (((int)access(*++argv, R_OK)) == -1) {
(void) fprintf(stderr, gettext("praudit: File, %s, not accessible\n"), *argv);
				returnstat = -1;
			} else if (*argc > MAXFILENAMES) {
(void) fprintf(stderr, gettext("praudit: Too many file names.\n"));
				returnstat = -1;
			}
			if (returnstat == 0) {
				count++;
				*names++ = *argv;
			}
		} while (--(*argc) > 0);

		if (returnstat == 0) {
			file_mode = FILEMODE;
			*argc = count;
		}
	} else if (returnstat == 0)
		file_mode = PIPEMODE;

	return (returnstat);
}

/*
 * -----------------------------------------------------------------------
 * pa_exit_token() 	: Process information label token and display contents
 * input		:
 * output		:
 * return codes		: -1 - error
 *			:  0 - successful
 * NOTE: At the time of call, the label token id has been retrieved
 *
 * Format of exit token:
 *	exit token id		adr_char
 * -----------------------------------------------------------------------
 */
int
pa_exit_token()
{
	int	returnstat;
	int	retval;

	returnstat = pa_tokenid();
	if (returnstat >= 0) {
		if ((returnstat = adrf_int32(audit_adr,
					(int32_t *)&retval, 1)) == 0) {
			if (format != RAWM) {
				uvaltype = PRA_STRING;
				if (retval > -1 && retval < sys_nerr)
					uval.string_val = strerror(retval);
				else
					uval.string_val =
						gettext("Unknown errno");
			} else {
				uvaltype = PRA_INT32;
				uval.int32_val = retval;
			}
			returnstat = pa_print(0, 0);
		}
	}
	returnstat =  pa_adr_int32(returnstat, 1, 0);
	return (returnstat);
}


/*
 * ------------------------------------------------------------------
 * pa_file_token: prints out seconds of time and other file name
 * input	:
 * output	:
 * return codes : -1 - error
 *		:  0 - successful, valid file token fields
 * At the time of entry, the file token ID has already been retrieved
 *
 * Format of file token:
 *	file token id		adr_char
 *	seconds of time		adr_u_int
 *	name of other file	adr_string
 * ------------------------------------------------------------------
 */
int
pa_file32_token()
{
	int	returnstat = 0;

	returnstat = pa_tokenid();
	returnstat = pa_time32(returnstat, 0, 0); /* seconds of time */
	returnstat = pa_msec32(returnstat, 0, 0); /* msec of time */
	returnstat = pa_adr_string(returnstat, 1, 1); /* other file name */

	return (returnstat);
}

int
pa_file64_token()
{
	int	returnstat = 0;

	returnstat = pa_tokenid();
	returnstat = pa_time64(returnstat, 0, 0); /* seconds of time */
	returnstat = pa_msec64(returnstat, 0, 0); /* msec of time */
	returnstat = pa_adr_string(returnstat, 1, 1); /* other file name */

	return (returnstat);
}


/*
 * -----------------------------------------------------------------------
 * pa_header_token()	: Process record header token and display contents
 * input    		:
 * output		:
 * return codes		: -1 - error
 *			:  0 - successful
 *			:  1 - warning, password entry not found
 *
 * NOTE: At the time of call, the header token id has been retrieved
 *
 * Format of header token:
 *	header token id 	adr_char
 * 	record byte count	adr_u_int
 *	event type		adr_u_short (printed either ASCII or raw)
 *	event class		adr_u_int   (printed either ASCII or raw)
 *	event action		adr_u_int
 *	seconds of time		adr_u_int   (printed either ASCII or raw)
 *	microseconds of time	adr_u_int
 * -----------------------------------------------------------------------
 */
int
pa_header32_token()
{
	int	returnstat = 0;

	if (ONELINE)
		putchar('\n');
	returnstat = pa_tokenid();
	returnstat = pa_adr_u_int32(returnstat, 0, 0);	/* record byte */
	returnstat = pa_adr_byte(returnstat, 0, 0);	/* version ID */
	returnstat = pa_event_type(returnstat, 0, 0);	/* event type */
	returnstat = pa_event_modifier(returnstat, 0, 0); /* event modifier */
	returnstat = pa_time32(returnstat, 0, 0);	/* seconds */
	returnstat = pa_msec32(returnstat, 1, 0);	/* msec */
	return (returnstat);
}

int
pa_header64_token()
{
	int	returnstat = 0;

	if (ONELINE)
		putchar('\n');
	returnstat = pa_tokenid();
	returnstat = pa_adr_u_int32(returnstat, 0, 0);	/* record byte */
	returnstat = pa_adr_byte(returnstat, 0, 0);	/* version ID */
	returnstat = pa_event_type(returnstat, 0, 0);	/* event type */
	returnstat = pa_event_modifier(returnstat, 0, 0); /* event modifier */
	returnstat = pa_time64(returnstat, 0, 0);	/* seconds */
	returnstat = pa_msec64(returnstat, 1, 0);	/* msec */
	return (returnstat);
}


/*
 * -----------------------------------------------------------------------
 * pa_trailer_token()	: Process record trailer token and display contents
 * input    		:
 * output		:
 * return codes		: -1 - error
 *			:  0 - successful
 * NOTE: At the time of call, the trailer token id has already been
 * retrieved
 *
 * Format of trailer token:
 * 	trailer token id	adr_char
 * 	record sequence no	adr_u_short (should be AUT_TRAILER_MAGIC)
 *	record byte count	adr_u_int
 * -----------------------------------------------------------------------
 */
int
pa_trailer_token()
{
	int	returnstat = 0;
	short	magic_number;

	returnstat = pa_tokenid();
	if (returnstat >= 0) {
		if (adrf_u_short(audit_adr, (u_short *)&magic_number, 1) < 0) {
(void) fprintf(stderr,
	gettext("praudit: Cannot retrieve trailer magic number\n"));
			return (-1);
		} else {
			if (magic_number != AUT_TRAILER_MAGIC) {
(void) fprintf(stderr, gettext("praudit: Invalid trailer magic number\n"));
				return (-1);
			} else
				return (pa_adr_u_int32(returnstat, 1, 1));
		}
	} else
		return (returnstat);
}


/*
 * -----------------------------------------------------------------------
 * pa_arbitrary_data_token():
 *			  Process arbitrary data token and display contents
 * input    		:
 * output		:
 * return codes		: -1 - error
 *			:  0 - successful
 * NOTE: At the time of call, the arbitrary data token id has already
 * been retrieved
 *
 * Format of arbitrary data token:
 *	arbitrary data token id	adr char
 * 	how to print		adr_char
 *				From audit_record.h, this may be either:
 *				AUP_BINARY	binary
 *				AUP_OCTAL	octal
 *				AUP_DECIMAL	decimal
 *				AUP_HEX		hexadecimal
 *	basic unit		adr_char
 *				From audit_record.h, this may be either:
 *				AUR_BYTE	byte
 *				AUR_CHAR	char
 *				AUR_SHORT	short
 *				AUR_INT32	int32_t
 *				AUR_INT64	int64_t
 *	unit count		adr_char, specifying number of units of
 *				data in the "data items" parameter below
 *	data items		depends on basic unit
 *
 * -----------------------------------------------------------------------
 */
int
pa_arbitrary_data_token()
{
	int	returnstat = 0;
	int	i;
	char	c1;
	short	c2;
	int32_t	c3;
	int64_t c4;
	char	how_to_print, basic_unit, unit_count, fwid;
	char	*p;
	int	index = 0;
	char	*pbuffer;
	char	buffer[100];
	int	loc = 2;
	char	*pformat = "%-     ";
	char	*fwd;

	pbuffer = &(buffer[0]);
	if ((returnstat = pa_tokenid()) < 0)
		return (returnstat);

	if ((returnstat = adrf_char(audit_adr, &how_to_print, 1)) != 0)
		return (returnstat);

	if ((returnstat = adrf_char(audit_adr, &basic_unit, 1)) != 0)
		return (returnstat);

	if ((returnstat = adrf_char(audit_adr, &unit_count, 1)) != 0)
		return (returnstat);

	if (format != RAWM) {
		uvaltype = PRA_STRING;
		uval.string_val = htp2string(how_to_print);
	} else {
		uvaltype = PRA_INT32;
		uval.int32_val = (int)how_to_print;
	}

	if ((returnstat = pa_print(0, 0)) < 0)
		return (returnstat);

	if (format != RAWM) {
		uvaltype = PRA_STRING;
		uval.string_val = bu2string(basic_unit);
	} else {
		uvaltype = PRA_INT32;
		uval.int32_val = (int32_t)basic_unit;
	}

	if ((returnstat = pa_print(0, 0)) < 0)
		return (returnstat);

	uvaltype = PRA_INT32;
	uval.int32_val = (int32_t)unit_count;

	if ((returnstat = pa_print(1, 0)) < 0)
		return (returnstat);

	/* get the field width in case we need to format output */
	fwid = findfieldwidth(basic_unit, how_to_print);
	p = (char *)malloc(80);

	/* now get the data items and print them */
	for (i = 0; i < unit_count; i++) {
		switch (basic_unit) {
			/* case AUR_BYTE: */
		case AUR_CHAR:
			if (adrf_char(audit_adr, &c1, 1) == 0)
				(void) convert_char_to_string(how_to_print,
					c1, p);
				else {
					free(p);
					return (-1);
				}
			break;
		case AUR_SHORT:
			if (adrf_short(audit_adr, &c2, 1) == 0)
				(void) convert_short_to_string(how_to_print,
					c2, p);
			else {
				free(p);
				return (-1);
			}
			break;
		case AUR_INT32:
			if (adrf_int32(audit_adr, &c3, 1) == 0)
				(void) convert_int32_to_string(how_to_print,
					c3, p);
			else {
				free(p);
				return (-1);
			}
			break;
		case AUR_INT64:
			if (adrf_int64(audit_adr, &c4, 1) == 0)
				(void) convert_int64_to_string(how_to_print,
					c4, p);
			else {
				free(p);
				return (-1);
			}
			break;
		default:
			free(p);
			return (-1);
			/* NOTREACHED */
			break;
		}

		/*
		 * At this point, we have successfully retrieved a data
		 * item and converted it into an ASCII string pointed to
		 * by p. If all output is to be printed on one line,
		 * simply separate the data items by a space (or by the
		 * delimiter if this is the last data item), otherwise, we
		 * need to format the output before display.
		 */
		if (ONELINE == 1) {
			(void) printf("%s", p);
			if (i == (unit_count - 1))
				(void) printf("%s", SEPARATOR);
				else
				(void) printf(" ");
		} else {	/* format output */
			loc = 2;
			pformat = "%-     ";
			fwd = (char *)malloc(5);
			(void) sprintf(fwd, "%d", fwid);
			(void) strncpy(pformat + loc, fwd, strlen(fwd));
			loc += strlen(fwd);
			(void) strcpy(pformat + loc, "s");
			(void) sprintf(pbuffer, pformat, p);
			(void) printf("%s", pbuffer);
			index += fwid;
			if (((index + fwid) > 75) || (i == (unit_count - 1))) {
				(void) printf("\n");
				index = 0;
			}
			free(fwd);
		} /* else if ONELINE */
	}
	free(p);
	return (returnstat);
}


/*
 * -----------------------------------------------------------------------
 * pa_opaque_token() 	: Process opaque token and display contents
 * input    		:
 * output		:
 * return codes		: -1 - error
 *			:  0 - successful
 * NOTE: At the time of call, the opaque token id has already been
 * retrieved
 *
 * Format of opaque token:
 *	opaque token id		adr_char
 *	size			adr_short
 *	data			adr_char, size times
 * -----------------------------------------------------------------------
 */
int
pa_opaque_token()
{
	int	returnstat = 0;
	short	size;
	char	*charp;

	returnstat = pa_tokenid();

	/* print the size of the token */
	if (returnstat >= 0) {
		if (adrf_short(audit_adr, &size, 1) == 0) {
			uvaltype = PRA_SHORT;
			uval.short_val = size;
			returnstat = pa_print(0, 0);
		} else
			returnstat = -1;
	}

	/* now print out the data field in hexadecimal */
	if (returnstat >= 0) {
		/* try to allocate memory for the character string */
		if ((charp = (char *)malloc(size * sizeof (char))) == NULL)
			returnstat = -1;
		else {
			if ((returnstat = adrf_char(audit_adr, charp, size))
					== 0) {
				/* print out in hexadecimal format */
				uvaltype = PRA_STRING;
				uval.string_val = hexconvert(charp, size, size);
				if (uval.string_val) {
					returnstat = pa_print(1, 0);
					free(uval.string_val);
				}
			}
			free(charp);
		}
	}

	return (returnstat);
}


/*
 * -----------------------------------------------------------------------
 * pa_path_token() 	: Process path token and display contents
 * input    		:
 * output		:
 * return codes		: -1 - error
 *			:  0 - successful
 * NOTE: At the time of call, the path token id has been retrieved
 *
 * Format of path token:
 *	token id	adr_char
 *	path		adr_string
 * -----------------------------------------------------------------------
 */
int
pa_path_token()
{
	int	returnstat = 0;

	returnstat = pa_tokenid();
	returnstat = pa_path(returnstat, 1, 0);
	return (returnstat);

}

/*
 * -----------------------------------------------------------------------
 * pa_cmd_token()	: Process cmd token and display contents
 * input		:
 * output		:
 * return codes		: -1 - error
 *			:  0 - successful
 * NOTE: At the time of call, the cmd token id has been retrieved
 *
 * Format of path token:
 *	token id	adr_char
 *	argc		adr_short
 *	N*argv[i]	adr_string (short, string)
 *	env cnt		adr_short
 *	N*arge[i]	adr_string (short, string)
 * -----------------------------------------------------------------------
 */
int
pa_cmd_token()
{
	int	returnstat = 0;
	int	flag;
	short num;

	returnstat = pa_tokenid();
	if (returnstat < 0)
		return (returnstat);

	returnstat = adrf_short(audit_adr, &num, 1);
	if (returnstat < 0)
		return (returnstat);

	printf("%s%s%d%s", (ONELINE)?"":"argcnt", (ONELINE)?"":SEPARATOR,
				num, SEPARATOR);

	for (; num > 0; num--) {
		if ((returnstat = pa_cmd(returnstat, 0, 0)) < 0)
			return (returnstat);
	}

	returnstat = adrf_short(audit_adr, &num, 1);
	if (returnstat < 0) {
		printf("%s\n", SEPARATOR);
		return (returnstat);
	}

	printf("%s%s%d%s", (ONELINE)?"":"envcnt", (ONELINE)?"":SEPARATOR,
				num, (num)?SEPARATOR:"");

	if (num == 0)
		putchar('\n');

	for (; num > 1; num--) {
		if ((returnstat = pa_cmd(returnstat, 0, 0)) < 0)
			return (returnstat);
	}
	if (num)
		returnstat = pa_cmd(returnstat, 1, 0);

	return (returnstat);

}


/*
 * -----------------------------------------------------------------------
 * pa_arg_token()	: Process argument token and display contents
 * input		:
 * output		:
 * return codes		: -1 - error
 *			:  0 - successful
 * NOTE: At the time of call, the arg token id has been retrieved
 *
 * Format of path token:
 *	current directory token id	adr_char
 *	argument number			adr_char
 *	argument value			adr_int32
 *	argument description		adr_string
 * -----------------------------------------------------------------------
 */
int
pa_arg32_token()
{
	int	returnstat = 0;

	returnstat = pa_tokenid();
	returnstat = pa_adr_byte(returnstat, 0, 0);
	returnstat = pa_adr_int32hex(returnstat, 0, 0);
	returnstat = pa_adr_string(returnstat, 1, 0);
	return (returnstat);

}

/*
 * -----------------------------------------------------------------------
 * pa_arg64_token()	: Process argument token and display contents
 * input		:
 * output		:
 * return codes		: -1 - error
 *			:  0 - successful
 * NOTE: At the time of call, the arg token id has been retrieved
 *
 * Format of path token:
 *	current directory token id	adr_char
 *	argument number			adr_char
 *	argument value			adr_int64
 *	argument description		adr_string
 * -----------------------------------------------------------------------
 */
int
pa_arg64_token()
{
	int	returnstat = 0;

	returnstat = pa_tokenid();
	returnstat = pa_adr_byte(returnstat, 0, 0);
	returnstat = pa_adr_int64hex(returnstat, 0, 0);
	returnstat = pa_adr_string(returnstat, 1, 0);
	return (returnstat);

}


/*
 * -----------------------------------------------------------------------
 * pa_process_token() 	: Process process token and display contents
 * input    		:
 * output		:
 * return codes		: -1 - error
 *			:  0 - successful
 * NOTE: At the time of call, the process token id has been retrieved
 *
 * Format of process token:
 *	process token id	adr_char
 *	auid			adr_u_int32
 *	euid			adr_u_int32
 *	egid			adr_u_int32
 *	ruid			adr_u_int32
 *	egid			adr_u_int32
 *	pid			adr_u_int32
 *	sid			adr_u_int32
 *	tid			adr_u_int32, adr_u_int32
 * -----------------------------------------------------------------------
 */
int
pa_process32_token()
{
	int	returnstat = 0;

	returnstat = pa_tokenid();
	returnstat = pa_pw_uid(returnstat, 0, 0);		/* auid */
	returnstat = pa_pw_uid(returnstat, 0, 0);		/* uid */
	returnstat = pa_gr_uid(returnstat, 0, 0);		/* gid */
	returnstat = pa_pw_uid(returnstat, 0, 0);		/* ruid */
	returnstat = pa_gr_uid(returnstat, 0, 0);		/* rgid */
	returnstat = pa_adr_u_int32(returnstat, 0, 0);		/* pid */
	returnstat = pa_adr_u_int32(returnstat, 0, 0);		/* sid */
	returnstat = pa_adr_u_int32(returnstat, 0, 0);		/* port ID */
	returnstat = pa_hostname(returnstat, 1, 0);		/* machine ID */
	return (returnstat);
}

int
pa_process64_token()
{
	int	returnstat = 0;

	returnstat = pa_tokenid();
	returnstat = pa_pw_uid(returnstat, 0, 0);		/* auid */
	returnstat = pa_pw_uid(returnstat, 0, 0);		/* uid */
	returnstat = pa_gr_uid(returnstat, 0, 0);		/* gid */
	returnstat = pa_pw_uid(returnstat, 0, 0);		/* ruid */
	returnstat = pa_gr_uid(returnstat, 0, 0);		/* rgid */
	returnstat = pa_adr_u_int32(returnstat, 0, 0);		/* pid */
	returnstat = pa_adr_u_int32(returnstat, 0, 0);		/* sid */
	returnstat = pa_adr_u_int64(returnstat, 0, 0);		/* port ID */
	returnstat = pa_hostname(returnstat, 1, 0);		/* machine ID */
	return (returnstat);
}


/*
 * -----------------------------------------------------------------------
 * pa_return32_token(): Process return value and display contents
 * input		:
 * output		:
 * return codes		: -1 - error
 *			:  0 - successful
 * NOTE: At the time of call, the return value token id has been retrieved
 *
 * Format of return value token:
 * 	return value token id	adr_char
 *	error number		adr_char
 *	return value		adr_int32
 * -----------------------------------------------------------------------
 */
int
pa_return32_token()
{
	int	returnstat = 0;
	signed char	number;
	char	pb[512];    /* print buffer */

	/*
	 * Every audit record generated contains a return token.
	 *
	 * The return token is a special token. It indicates the success
	 * or failure of the event that contains it.
	 * The return32 token contains two pieces of data:
	 *
	 * 	char number;
	 * 	int32_t  return_value;
	 *
	 * For audit records generated by the kernel:
	 * The kernel always puts a positive value in "number".
	 * Upon success "number" is 0.
	 * Upon failure "number" is a positive errno value that is less than
	 * sys_nerr.
	 *
	 * For audit records generated at the user level:
	 * Upon success "number" is 0.
	 * Upon failure "number" is -1.
	 *
	 * For both kernel and user land the value of "return_value" is
	 * arbitrary. For the kernel it contains the return value of
	 * the system call. For user land it contains an arbitrary return
	 * value. No interpretation is done on "return_value".
	 */

	returnstat = pa_tokenid();
	if (returnstat >= 0) {
		if ((returnstat = adrf_char(audit_adr, (char *)&number,
		    1)) == 0) {
			if (format != RAWM) {
				/*
				 * Success event
				 */
				if (number == 0)
					(void) strcpy(pb, gettext("success"));
				else if (number == -1)
					/*
					 * Failure event - in user land
					 */
					(void) strcpy(pb, gettext("failure"));
				else {
					/*
					 * Failure event - in kernel (an errno)
					 */
					(void) strcpy(pb, gettext("failure: "));
					if (number < sys_nerr)
						(void) strcat(pb,
							strerror(number));
						else
							(void) strcat(pb,
						gettext("Unknown errno"));
				}
				uvaltype = PRA_STRING;
				uval.string_val = pb;
			} else {
				uvaltype = PRA_INT32;
				uval.int32_val = (int32_t)number;
			}
			returnstat = pa_print(0, 0);
		}
	}
	returnstat =  pa_adr_int32(returnstat, 1, 0);
	return (returnstat);
}

/*
 * -----------------------------------------------------------------------
 * pa_return64_token(): Process return value and display contents
 * input		:
 * output		:
 * return codes		: -1 - error
 *			:  0 - successful
 * NOTE: At the time of call, the return value token id has been retrieved
 *
 * Format of return value token:
 * 	return value token id	adr_char
 *	error number		adr_char
 *	return value		adr_int64
 * -----------------------------------------------------------------------
 */
int
pa_return64_token()
{
	int	returnstat = 0;
	signed char	number;
	char	pb[512];    /* print buffer */

	/*
	 * Every audit record generated contains a return token.
	 *
	 * The return token is a special token. It indicates the success
	 * or failure of the event that contains it.
	 * The return64 token contains two pieces of data:
	 *
	 * 	char number;
	 * 	int64_t  return_value;
	 *
	 * For audit records generated by the kernel:
	 * The kernel always puts a positive value in "number".
	 * Upon success "number" is 0.
	 * Upon failure "number" is a positive errno value that is less than
	 * sys_nerr.
	 *
	 * For audit records generated at the user level:
	 * Upon success "number" is 0.
	 * Upon failure "number" is -1.
	 *
	 * For both kernel and user land the value of "return_value" is
	 * arbitrary. For the kernel it contains the return value of
	 * the system call. For user land it contains an arbitrary return
	 * value. No interpretation is done on "return_value".
	 */

	returnstat = pa_tokenid();
	if (returnstat >= 0) {
		if ((returnstat = adrf_char(audit_adr, (char *)&number,
		    1)) == 0) {
			if (format != RAWM) {
				/*
				 * Success event
				 */
				if (number == 0)
					(void) strcpy(pb, gettext("success"));
				else if (number == -1)
					/*
					 * Failure event - in user land
					 */
					(void) strcpy(pb, gettext("failure"));
				else {
					/*
					 * Failure event - in kernel (an errno)
					 */
					(void) strcpy(pb, gettext("failure: "));
					if (number < sys_nerr)
						(void) strcat(pb,
							strerror(number));
						else
							(void) strcat(pb,
						gettext("Unknown errno"));
				}
				uvaltype = PRA_STRING;
				uval.string_val = pb;
			} else {
				uvaltype = PRA_INT32;
				uval.int32_val = (int32_t)number;
			}
			returnstat = pa_print(0, 0);
		}
	}
	returnstat =  pa_adr_int64(returnstat, 1, 0);
	return (returnstat);
}


/*
 * -----------------------------------------------------------------------
 * pa_server_token()	: Process server token and display contents
 * input		:
 * output		:
 * return codes		: -1 - error
 *			:  0 - successful
 * NOTE: At the time of call, the server token id has been retrieved
 *
 * Format of server token:
 *	server token id		adr_char
 *	auid			adr_u_int32
 *	euid			adr_u_int32
 *	ruid			adr_u_int32
 *	egid			adr_u_int32
 *	pid			adr_u_int32
 * -----------------------------------------------------------------------
 */
int
pa_server_token()
{
	int	returnstat = 0;
	int	i;

	returnstat = pa_tokenid();
	for (i = 1; i < 4 && returnstat >= 0; i++)
		returnstat = pa_pw_uid(returnstat, 0, 0);
	returnstat = pa_gr_uid(returnstat, 0, 0);
	returnstat = pa_adr_u_short(returnstat, 1, 0);
	return (returnstat);
}

/*
 * -----------------------------------------------------------------------
 * pa_subject_token()	: Process subject token and display contents
 * input		:
 * output		:
 * return codes		: -1 - error
 *			:  0 - successful
 * NOTE: At the time of call, the subject token id has been retrieved
 *
 * Format of subject token:
 *	subject token id	adr_char
 *	auid			adr_u_int32
 *	euid			adr_u_int32
 *	egid			adr_u_int32
 *	ruid			adr_u_int32
 *	egid			adr_u_int32
 *	pid			adr_u_int32
 *	sid			adr_u_int32
 *	tid			adr_u_int32, adr_u_int32
 * -----------------------------------------------------------------------
 */
int
pa_subject32_token()
{
	int	returnstat = 0;

	returnstat = pa_tokenid();
	returnstat = pa_pw_uid(returnstat, 0, 0);		/* auid */
	returnstat = pa_pw_uid(returnstat, 0, 0);		/* uid */
	returnstat = pa_gr_uid(returnstat, 0, 0);		/* gid */
	returnstat = pa_pw_uid(returnstat, 0, 0);		/* ruid */
	returnstat = pa_gr_uid(returnstat, 0, 0);		/* rgid */
	returnstat = pa_adr_u_int32(returnstat, 0, 0);		/* pid */
	returnstat = pa_adr_u_int32(returnstat, 0, 0);		/* sid */
	returnstat = pa_tid32(returnstat, 1, 0);		/* tid */
	return (returnstat);
}

int
pa_subject64_token()
{
	int	returnstat = 0;

	returnstat = pa_tokenid();
	returnstat = pa_pw_uid(returnstat, 0, 0);		/* auid */
	returnstat = pa_pw_uid(returnstat, 0, 0);		/* uid */
	returnstat = pa_gr_uid(returnstat, 0, 0);		/* gid */
	returnstat = pa_pw_uid(returnstat, 0, 0);		/* ruid */
	returnstat = pa_gr_uid(returnstat, 0, 0);		/* rgid */
	returnstat = pa_adr_u_int32(returnstat, 0, 0);		/* pid */
	returnstat = pa_adr_u_int32(returnstat, 0, 0);		/* sid */
	returnstat = pa_tid64(returnstat, 1, 0);		/* tid */
	return (returnstat);
}


/*
 * -----------------------------------------------------------------------
 * pa_SystemV_IPC_token(): Process System V IPC token and display contents
 * input		 :
 * output		 :
 * return codes		: -1 - error
 *			:  0 - successful
 * NOTE: At the time of call, the System V IPC id has been retrieved
 *
 * Format of System V IPC token:
 *	System V IPC token id	adr_char
 *	object id		adr_int32
 * -----------------------------------------------------------------------
 */
int
pa_SystemV_IPC_token()
{
	int	returnstat = 0;

	returnstat = pa_tokenid();
	returnstat = pa_ipc_id(returnstat, 0, 0);
	returnstat = pa_adr_int32(returnstat, 1, 0);
	return (returnstat);
}


/*
 * -----------------------------------------------------------------------
 * pa_text_token(): Process text token and display contents
 * input	:
 * output	:
 * return codes	: -1 - error
 *		:  0 - successful
 * NOTE: At the time of call, the text token id has been retrieved
 *
 * Format of text token:
 *	text token id		adr_char
 * 	text			adr_string
 * -----------------------------------------------------------------------
 */
int
pa_text_token()
{
	int	returnstat = 0;

	returnstat = pa_tokenid();
	returnstat = pa_adr_string(returnstat, 1, 0);
	return (returnstat);
}


/*
 * -----------------------------------------------------------------------
 * pa_ip_addr() 	: Process ip token and display contents
 * input		:
 * output		:
 * return codes 	: -1 - error
 *			:  0 - successful
 * NOTE: At the time of call, the ip token id has been retrieved
 *
 * Format of attribute token:
 *	ip token id	adr_char
 *	address		adr_int32 (printed in hex)
 * -----------------------------------------------------------------------
 */

char	*
get_Hname(addr)
unsigned int	addr;
{
	extern char	*inet_ntoa(const struct in_addr);
	struct hostent *phe;
	static char	buf[256];
	struct in_addr ia;

	phe = gethostbyaddr((const char *) & addr, 4, AF_INET);
	if (phe == (struct hostent *)0) {
		ia.s_addr = addr;
		(void) sprintf(buf, "%s", inet_ntoa(ia));
		return (buf);
	}
	ia.s_addr = addr;
	(void) sprintf(buf, "%s", phe->h_name);
	return (buf);
}

static int
pa_hostname(status, flag, endrec)
int	status;
int	flag;
int	endrec;
{
	int	returnstat;
	int	ip_addr;
	struct in_addr ia;

	if (status <  0)
		return (status);

	if ((returnstat = adrf_char(audit_adr, (char *) &ip_addr, 4)) != 0)
		return (returnstat);

	uvaltype = PRA_STRING;

	if (format != RAWM) {
		uval.string_val = get_Hname(ip_addr);
		returnstat = pa_print(flag, endrec);
	} else {
		ia.s_addr = ip_addr;
		if ((uval.string_val = inet_ntoa(ia)) == NULL)
			return (-1);
		returnstat = pa_print(flag, endrec);
	}
	return (returnstat);
}


int
pa_ip_addr()
{
	int	returnstat = 0;

	returnstat = pa_tokenid();
	returnstat = pa_hostname(returnstat, 1, 0);
	return (returnstat);
}

/*
 * -----------------------------------------------------------------------
 * pa_ip()		: Process ip header token and display contents
 * input		:
 * output		:
 * return codes 	: -1 - error
 *			:  0 - successful
 * NOTE: At the time of call, the ip token id has been retrieved
 *
 * Format of attribute token:
 *	ip header token id	adr_char
 *	version			adr_char (printed in hex)
 *	type of service		adr_char (printed in hex)
 *	length			adr_short
 *	id			adr_u_short
 *	offset			adr_u_short
 *	ttl			adr_char (printed in hex)
 *	protocol		adr_char (printed in hex)
 *	checksum		adr_u_short
 *	source address		adr_int32 (printed in hex)
 *	destination address	adr_int32 (printed in hex)
 * -----------------------------------------------------------------------
 */
int
pa_ip()
{
	int	returnstat = 0;

	returnstat = pa_tokenid();
	returnstat = pa_adr_charhex(returnstat, 0, 0);
	returnstat = pa_adr_charhex(returnstat, 0, 0);
	returnstat = pa_adr_short(returnstat, 0, 0);
	returnstat = pa_adr_u_short(returnstat, 0, 0);
	returnstat = pa_adr_u_short(returnstat, 0, 0);
	returnstat = pa_adr_charhex(returnstat, 0, 0);
	returnstat = pa_adr_charhex(returnstat, 0, 0);
	returnstat = pa_adr_u_short(returnstat, 0, 0);
	returnstat = pa_adr_int32hex(returnstat, 0, 0);
	returnstat = pa_adr_int32hex(returnstat, 1, 0);
	return (returnstat);
}


/*
 * -----------------------------------------------------------------------
 * pa_iport() 	: Process ip port address token and display contents
 * input    	:
 * output	:
 * return codes	: -1 - error
 *		:  0 - successful
 * NOTE: At time of call, the ip port address token id has been retrieved
 *
 * Format of attribute token:
 *	ip port address token id	adr_char
 *	port address			adr_short (in hex)
 * -----------------------------------------------------------------------
 */
int
pa_iport()
{
	int	returnstat = 0;

	returnstat = pa_tokenid();
	returnstat = pa_adr_shorthex(returnstat, 1, 0);
	return (returnstat);
}


#define	NBITSMAJOR64	32	/* # of major device bits in 64-bit Solaris */
#define	NBITSMINOR64	32	/* # of minor device bits in 64-bit Solaris */
#define	MAXMAJ64	0xfffffffful	/* max major value */
#define	MAXMIN64	0xfffffffful	/* max minor value */

#define	NBITSMAJOR32	14	/* # of SVR4 major device bits */
#define	NBITSMINOR32	18	/* # of SVR4 minor device bits */
#define	NMAXMAJ32	0x3fff	/* SVR4 max major value */
#define	NMAXMIN32	0x3ffff	/* MAX minor for 3b2 software drivers. */


static int32_t
minor_64(uint64_t dev)
{
	if (dev == NODEV) {
		errno = EINVAL;
		return (NODEV);
	}
	return (int32_t)(dev & MAXMIN64);
}

static int32_t
major_64(uint64_t dev)
{
	uint32_t maj;

	maj = (uint32_t)(dev >> NBITSMINOR64);

	if (dev == NODEV || maj > MAXMAJ64) {
		errno = EINVAL;
		return (NODEV);
	}
	return (int32_t)(maj);
}

static int32_t
minor_32(uint32_t dev)
{
	if (dev == NODEV) {
		errno = EINVAL;
		return (NODEV);
	}
	return (int32_t)(dev & MAXMIN32);
}

static int32_t
major_32(uint32_t dev)
{
	uint32_t maj;

	maj = (uint32_t)(dev >> NBITSMINOR32);

	if (dev == NODEV || maj > MAXMAJ32) {
		errno = EINVAL;
		return (NODEV);
	}
	return (int32_t)(maj);
}


/*
 * -----------------------------------------------------------------------
 * pa_tid() 	: Process terminal id and display contents
 * return codes	: -1 - error
 *		:  0 - successful
 *
 * Format of attribute token:
 *	terminal id port		adr_int32
 *	terminal id machine		adr_int32
 * -----------------------------------------------------------------------
 */
static int
pa_tid32(status, flag, endrec)
int	status;
int	flag;
int	endrec;
{
	int	returnstat;
	int32_t dev_maj_min;
	int32_t	ip_addr;
	struct in_addr ia;
	char	*hostname;
	char	*ipstring;
	char	buf[256];

	if (status <  0)
		return (status);

	if ((returnstat = adrf_int32(audit_adr, &dev_maj_min, 1)) != 0)
		return (returnstat);

	if ((returnstat = adrf_char(audit_adr, (char *) &ip_addr, 4)) != 0)
		return (returnstat);

	uvaltype = PRA_STRING;
	uval.string_val = buf;

	if (format != RAWM) {
		hostname = get_Hname((int)ip_addr);
		(void) sprintf(buf, "%d %d %s",
			major_32(dev_maj_min),
			minor_32(dev_maj_min),
			hostname);
		return (pa_print(flag, endrec));
	}

	ia.s_addr = ip_addr;
	if ((ipstring = inet_ntoa(ia)) == NULL)
		return (-1);

	(void) sprintf(buf, "%d %d %s",
		major_32(dev_maj_min),
		minor_32(dev_maj_min),
		ipstring);

	return (pa_print(flag, endrec));
}



static int
pa_tid64(status, flag, endrec)
int	status;
int	flag;
int	endrec;
{
	int	returnstat;
	int64_t dev_maj_min;
	int32_t	ip_addr;
	struct in_addr ia;
	char	*hostname;
	char	*ipstring;
	char	buf[256];

	if (status <  0)
		return (status);

	if ((returnstat = adrf_int64(audit_adr, &dev_maj_min, 1)) != 0)
		return (returnstat);

	if ((returnstat = adrf_char(audit_adr, (char *) &ip_addr, 4)) != 0)
		return (returnstat);

	uvaltype = PRA_STRING;
	uval.string_val = buf;

	if (format != RAWM) {
		hostname = get_Hname((int)ip_addr);
		(void) sprintf(buf, "%d %d %s",
			major_64(dev_maj_min),
			minor_64(dev_maj_min),
			hostname);
		return (pa_print(flag, endrec));
	}

	ia.s_addr = ip_addr;
	if ((ipstring = inet_ntoa(ia)) == NULL)
		return (-1);

	(void) sprintf(buf, "%d %d %s",
		major_64(dev_maj_min),
		minor_64(dev_maj_min),
		ipstring);

	return (pa_print(flag, endrec));
}


/*
 * -----------------------------------------------------------------------
 * pa_socket() 	: Process socket token and display contents
 * input    	:
 * output	:
 * return codes	: -1 - error
 *		:  0 - successful
 * NOTE: At time of call, the socket token id has been retrieved
 *
 * Format of attribute token:
 *	ip socket token id		adr_char
 *	socket type			adr_short (in hex)
 *	local port			adr_short (in hex)
 *	local internet address		adr_hostname/adr_int32 (in ascii/hex)
 *	foreign port			adr_short (in hex)
 *	foreign internet address	adr_hostname/adr_int32 (in ascii/hex)
 * -----------------------------------------------------------------------
 *
 * Note: local port and local internet address have been removed for 5.x
 */
int
pa_socket_token()
{
	int	returnstat = 0;

	returnstat = pa_tokenid();
	returnstat = pa_adr_shorthex(returnstat, 0, 0);
	/* returnstat = pa_adr_shorthex(returnstat,0,0); */
	/* returnstat = pa_hostname(returnstat,0,0); */
	returnstat = pa_adr_shorthex(returnstat, 0, 0);
	returnstat = pa_hostname(returnstat, 1, 0);
	return (returnstat);
}


/*
 * -----------------------------------------------------------------------
 * pa_sequence_token() 	: Process socket token and display contents
 * input    	:
 * output	:
 * return codes	: -1 - error
 *		:  0 - successful
 * NOTE: At time of call, the socket token id has been retrieved
 *
 * Format of attribute token:
 *	ip sequence token id		adr_char
 *	sequence number 		adr_u_int32 (in hex)
 * -----------------------------------------------------------------------
 */
int
pa_sequence_token()
{
	int	returnstat = 0;

	returnstat = pa_tokenid();
	returnstat = pa_adr_u_int32(returnstat, 1, 0);	/* sequence number */
	return (returnstat);
}

int
pa_acl_token()
{
	int	returnstat = 0;

 	returnstat = pa_tokenid();
 	returnstat = pa_pw_uid_gr_gid(returnstat, 0, 0);
 	returnstat = pa_mode(returnstat, 1, 0);
 	return (returnstat);
}


/*
 * -----------------------------------------------------------------------
 * pa_attribute_token() : Process attribute token and display contents
 * input		:
 * output		:
 * return codes 	: -1 - error
 *			:  0 - successful
 * NOTE: At the time of call, the attribute token id has been retrieved
 *
 * Format of attribute token:
 *	attribute token id	adr_char
 * 	mode			adr_u_int (printed in octal)
 *	uid			adr_u_int
 *	gid			adr_u_int
 *	file system id		adr_int
 *	node id			adr_int64
 *	device			adr_u_int
 * -----------------------------------------------------------------------
 */
int
pa_attribute32_token()
{
	int	returnstat = 0;

	returnstat = pa_tokenid();
	returnstat = pa_mode(returnstat, 0, 0);
	returnstat = pa_pw_uid(returnstat, 0, 0);
	returnstat = pa_gr_uid(returnstat, 0, 0);
	returnstat = pa_adr_int32(returnstat, 0, 0);
	returnstat = pa_adr_int64(returnstat, 0, 0);
	returnstat = pa_adr_u_int32(returnstat, 1, 0);
	return (returnstat);
}

int
pa_attribute64_token()
{
	int	returnstat = 0;

	returnstat = pa_tokenid();
	returnstat = pa_mode(returnstat, 0, 0);
	returnstat = pa_pw_uid(returnstat, 0, 0);
	returnstat = pa_gr_uid(returnstat, 0, 0);
	returnstat = pa_adr_int32(returnstat, 0, 0);
	returnstat = pa_adr_int64(returnstat, 0, 0);
	returnstat = pa_adr_u_int64(returnstat, 1, 0);
	return (returnstat);
}


/*
 * -----------------------------------------------------------------------
 * pa_group_token() 	: Process group token and display contents
 * input			:
 * output		:
 * return codes 	: -1 - error
 *			:  0 - successful
 * NOTE: At the time of call, the group token id has been retrieved
 *
 * Format of group token:
 *	group token id		adr_char
 *	group list		adr_long, 16 times
 * -----------------------------------------------------------------------
 */
int
pa_group_token()
{
	int	returnstat = 0;
	int	i;

	returnstat = pa_tokenid();
	for (i = 0; i < NGROUPS_MAX - 1; i++) {
		if ((returnstat = pa_gr_uid(returnstat, 0, 0)) < 0)
			return (returnstat);
	}
	returnstat = pa_gr_uid(returnstat, 1, 0);
	return (returnstat);
}

/*
 * -----------------------------------------------------------------------
 * pa_newgroup_token() 	: Process group token and display contents
 * input			:
 * output		:
 * return codes 	: -1 - error
 *			:  0 - successful
 * NOTE: At the time of call, the group token id has been retrieved
 *
 * Format of group token:
 *	group token id		adr_char
 *	group number		adr_short
 *	group list		adr_int32, group number times
 * -----------------------------------------------------------------------
 */
int
pa_newgroup_token()
{
	int	returnstat = 0;
	int	i, num;
	short	n_groups;

	returnstat = pa_tokenid();
	returnstat = adrf_short(audit_adr, &n_groups, 1);
	num = (int) n_groups;
	for (i = 0; i < num - 1; i++) {
		if ((returnstat = pa_gr_uid(returnstat, 0, 0)) < 0)
			return (returnstat);
	}
	returnstat = pa_gr_uid(returnstat, 1, 0);
	return (returnstat);
}

int
pa_exec_token()
{
	int	returnstat = 0;
	int	num;

	returnstat = pa_tokenid();
	returnstat = adrf_int32(audit_adr, (int32_t *) &num, 1);
	if (returnstat == 0) {
		printf("%d%s", num, SEPARATOR);
		if (ONELINE != 1)
			putchar('\n');
	}
	for (; num > 1; num--) {
		if ((returnstat = pa_exec_string(returnstat, 0, 0)) < 0)
			return (returnstat);
	}
	returnstat = pa_exec_string(returnstat, 1, 0);
	return (returnstat);
}

/*
 * -----------------------------------------------------------------------
 * pa_SystemV_IPC_perm_token() :
 *			  Process System V IPC permission token and
 *			  display contents
 * input			:
 * output		:
 * return codes 	: -1 - error
 *			:  0 - successful
 * NOTE: At the time of call, the System V IPC permission token id
 * has been retrieved
 *
 * Format of System V IPC permission token:
 *	System V IPC permission token id	adr_char
 * 	uid					adr_u_int32
 *	gid					adr_u_int32
 *	cuid					adr_u_int32
 *	cgid					adr_u_int32
 *	mode					adr_u_int32
 *	seq					adr_u_int32
 *	key					adr_int32
 * -----------------------------------------------------------------------
 */
int
pa_SystemV_IPC_perm_token()
{
	int	returnstat = 0;

	returnstat = pa_tokenid();
	returnstat = pa_pw_uid(returnstat, 0, 0);	/* uid */
	returnstat = pa_gr_uid(returnstat, 0, 0);	/* gid */
	returnstat = pa_pw_uid(returnstat, 0, 0);	/* owner uid */
	returnstat = pa_gr_uid(returnstat, 0, 0);	/* owner gid */
	returnstat = pa_mode(returnstat, 0, 0);		/* mode */
	returnstat = pa_adr_u_int32(returnstat, 0, 0);	/* seq */
	returnstat = pa_adr_int32hex(returnstat, 1, 0);	/* key */
	return (returnstat);
}

/*
 * ----------------------------------------------------------------
 * findfieldwidth:
 * Returns the field width based on the basic unit and print mode.
 * This routine is called to determine the field width for the
 * data items in the arbitrary data token where the tokens are
 * to be printed in more than one line.  The field width can be
 * found in the fwidth structure.
 *
 * Input parameters:
 * basicunit	Can be one of AUR_CHAR, AUR_BYTE, AUR_SHORT,
 *		AUR_INT32, or AUR_INT64
 * howtoprint	Print mode. Can be one of AUP_BINARY, AUP_OCTAL,
 *		AUP_DECIMAL, or AUP_HEX.
 * ----------------------------------------------------------------
 */
int
findfieldwidth(basicunit, howtoprint)
char	basicunit;
char	howtoprint;
{
	int	i, j;

	for (i = 0; i < numwidthentries; i++) {
		if (fwidth[i].basic_unit == basicunit) {
			for (j = 0; j < 4; j++)
				if (fwidth[i].pwidth[j].print_base ==
					howtoprint)
				return (fwidth[i].pwidth[j].field_width);
			/*
			 * if we got here, then we didn't get what we were after
			 */
			return (0);
		}
	}
	/* if we got here, we didn't get what we wanted either */
	return (0);
}


/*
 * -----------------------------------------------------------------------
 * pa_gettokenstring	:
 *		  Obtains the token string corresponding to the token id
 *		  passed in the parameter from the token table, tokentab
 *		  Returns NULL if the tokenid is not found
 * return codes : -1 - error
 *		:  0 - successful
 * -----------------------------------------------------------------------
 */
char	*
pa_gettokenstring(tokenid)
int	tokenid;
{
	int	i;
	struct tokentable *k;

	for (i = 0; i < numtokenentries; i++) {
		k = &(tokentab[i]);
		if ((k->tokenid) == tokenid)
			return (gettext(k->tokentype));
	}
	/* here if token id is not in table */
	return (NULL);
}


/*
 * -----------------------------------------------------------------------
 * pa_path: Issues adrf_string to retrieve the path item from the
 *		  input stream. Collapses the path and prints it
 *		  if status >= 0
 * return codes : -1 - error
 *		:  0 - successful
 * -----------------------------------------------------------------------
 */
static int
pa_path(status, flag, endrec)
int	status;
int	flag;
int	endrec;
{
	char	*path;  /* path */
	char	*apath; /* anchored path */
	char	*cpath; /* collapsed path */
	short	length;
	int	returnstat = 0;

	/*
	 * note that adrf_string uses adrf_short to read a length and
	 * adrf_char to read that many bytes of data. We need to know
	 * how much space to allocate for our string so we have to read
	 * the length first
	 */
	if (status >= 0) {
		if (adrf_short(audit_adr, &length, 1) == 0) {
			if ((path = (char *)malloc(length + 1)) == NULL) {
				returnstat = -1;
			} else if (adrf_char(audit_adr, path, length) == 0) {
				uvaltype = PRA_STRING;
				if (*path != '/') {
					apath = anchor_path(path);
					free(path);
				} else
					apath = path;
				cpath = collapse_path(apath);
				uval.string_val = cpath;
				returnstat = pa_print(flag, endrec);
				free(cpath);
			} else {
				free(path);
				returnstat = -1;
			}
			return (returnstat);
		} else
			return (-1);
	} else
		return (status);
}

/*
 * -----------------------------------------------------------------------
 * pa_cmd: Issues adrf_string to retrieve the path item from the
 *		  input stream. Collapses the path and prints it
 *		  if status >= 0
 * return codes : -1 - error
 *		:  0 - successful
 * -----------------------------------------------------------------------
 */
static int
pa_cmd(status, flag, endrec)
	int status;
	int flag;
	int endrec;
{
	char	*cmd;  /* cmd */
	short	length;
	int	returnstat = 0;
	short	cnt;

	/*
	 * note that adrf_string uses adrf_short to read a length and
	 * adrf_char to read that many bytes of data. We need to know
	 * how much space to allocate for our string so we have to read
	 * the length first
	 */
	if (status >= 0) {
		if (adrf_short(audit_adr, &length, 1) == 0) {
			if ((cmd = (char *)malloc(length + 1)) == NULL)
				return (-1);
			if (adrf_char(audit_adr, cmd, length) == 0) {
				uvaltype = PRA_STRING;
				uval.string_val = cmd;
				returnstat = pa_print(flag, endrec);
				free(cmd);
			} else {
				free(cmd);
				returnstat = -1;
			}
			return (returnstat);
		} else
			return (-1);
	} else
		return (status);
}


/*
 * -----------------------------------------------------------------------
 * pa_process_record	:
 *		  Calls the routine corresponding to the token id
 *		  passed in the parameter from the token table, tokentab
 * return codes : -1 - error
 *		:  0 - successful
 * -----------------------------------------------------------------------
 */
static int
pa_process_record(tokenid)
int	tokenid;
{
	int	i;
	struct tokentable *k;

	for (i = 0; i < numtokenentries; i++) {
		k = &(tokentab[i]);
		if ((k->tokenid) == tokenid) {
			(*tokentab[i].func)();
			return (0);
		}
	}
	/* here if token id is not in table */
	(void) fprintf(stderr, gettext(
		"praudit: No code associated with token id %d\n"),
		tokenid);
	return (0);
}


/*
 * -----------------------------------------------------------------------
 * pa_tokenid	: Issues pa_print to print out the token id specified in
 *		  the global value tokenid either in decimal or ASCII.
 *		  If the token type is unknown, the decimal value is
 *		  printed even if ASCII representation is requested
 * return codes : -1 - error
 *		:  0 - successful
 * -----------------------------------------------------------------------
 */
static int
pa_tokenid()
{
	char	*pa_gettokenstring();
	char	*s;

	if (format != RAWM) {
		if ((s = pa_gettokenstring(tokenid)) != NULL) {
			uvaltype = PRA_STRING;
			uval.string_val = s;
		} else {
			uvaltype = PRA_BYTE;
			uval.char_val = tokenid;
		}
	} else {
		uvaltype = PRA_BYTE;
		uval.char_val = tokenid;
	}
	return (pa_print(0, 0));
}


/*
 * -----------------------------------------------------------------------
 * pa_adr_byte	: Issues adrf_char to retrieve the next ADR item from
 *		  the input stream pointed to by audit_adr, and prints it
 *		  as an integer if status >= 0
 * return codes : -1 - error
 *		:  0 - successful
 * -----------------------------------------------------------------------
 */
static int
pa_adr_byte(status, flag, endrec)
int	status;
int	flag;
int	endrec;
{
	char	c;

	if (status >= 0) {
		if (adrf_char(audit_adr, &c, 1) == 0) {
			uvaltype = PRA_BYTE;
			uval.char_val = c;
			return (pa_print(flag, endrec));
		} else
			return (-1);
	} else
		return (status);
}

/*
 * -----------------------------------------------------------------------
 * pa_adr_charhex: Issues adrf_char to retrieve the next ADR item from
 *			the input stream pointed to by audit_adr, and prints it
 *			in hexadecimal if status >= 0
 * return codes  : -1 - error
 *		 :  0 - successful
 * -----------------------------------------------------------------------
 */
static int
pa_adr_charhex(status, flag, endrec)
int	status;
int	flag;
int	endrec;
{
	char	p[2];
	int	returnstat = 0;

	if (status >= 0) {
		p[0] = p[1] = 0;

		if (returnstat == 0) {
			if ((returnstat = adrf_char(audit_adr, p, 1)) == 0) {
				uvaltype = PRA_STRING;
				uval.string_val = hexconvert(p,
					sizeof (char),
					sizeof (char));
				if (uval.string_val) {
					returnstat = pa_print(flag, endrec);
					free(uval.string_val);
				}
			}
		}
		return (returnstat);
	} else
		return (status);
}

/*
 * -----------------------------------------------------------------------
 * pa_adr_int32	: Issues adrf_int32 to retrieve the next ADR item from the
 *		  input stream pointed to by audit_adr, and prints it
 *		  if status >= 0
 * return codes : -1 - error
 *		:  0 - successful
 * -----------------------------------------------------------------------
 */



int
pa_adr_int32(status, flag, endrec)
int	status;
int	flag;
int	endrec;
{
	int32_t	c;

	if (status >= 0) {
		if (adrf_int32(audit_adr, &c, 1) == 0) {
			uvaltype = PRA_INT32;
			uval.int32_val = c;
			return (pa_print(flag, endrec));
		} else
			return (-1);
	} else
		return (status);
}




/*
 * -----------------------------------------------------------------------
 * pa_adr_int64	: Issues adrf_int64 to retrieve the next ADR item from the
 *		  input stream pointed to by audit_adr, and prints it
 *		  if status >= 0
 * return codes : -1 - error
 *		:  0 - successful
 * -----------------------------------------------------------------------
 */
static int
pa_adr_int64(status, flag, endrec)
int	status;
int	flag;
int	endrec;
{
	int64_t	c;

	if (status >= 0) {
		if (adrf_int64(audit_adr, &c, 1) == 0) {
			uvaltype = PRA_INT64;
			uval.int64_val = c;
			return (pa_print(flag, endrec));
		} else
			return (-1);
	} else
		return (status);
}

/*
 * -----------------------------------------------------------------------
 * pa_adr_int64hex: Issues adrf_int64 to retrieve the next ADR item from the
 *			input stream pointed to by audit_adr, and prints it
 *			in hexadecimal if status >= 0
 * return codes  : -1 - error
 *		:  0 - successful
 * -----------------------------------------------------------------------
 */
static int
pa_adr_int32hex(status, flag, endrec)
int	status;
int	flag;
int	endrec;
{
	int32_t	l;
	int	returnstat = 0;

	if (status >= 0) {
		if (returnstat == 0) {
			if ((returnstat = adrf_int32(audit_adr,
							&l, 1)) == 0) {
				uvaltype = PRA_HEX32;
				uval.int32_val = l;
				returnstat = pa_print(flag, endrec);
			}
		}
		return (returnstat);
	} else
		return (status);
}

/*
 * -----------------------------------------------------------------------
 * pa_adr_int64hex: Issues adrf_int64 to retrieve the next ADR item from the
 *			input stream pointed to by audit_adr, and prints it
 *			in hexadecimal if status >= 0
 * return codes  : -1 - error
 *		:  0 - successful
 * -----------------------------------------------------------------------
 */
static int
pa_adr_int64hex(status, flag, endrec)
int	status;
int	flag;
int	endrec;
{
	int64_t	l;
	int	returnstat = 0;

	if (status >= 0) {
		if (returnstat == 0) {
			if ((returnstat = adrf_int64(audit_adr,
							&l, 1)) == 0) {
				uvaltype = PRA_HEX64;
				uval.int64_val = l;
				returnstat = pa_print(flag, endrec);
			}
		}
		return (returnstat);
	} else
		return (status);
}


/*
 * -------------------------------------------------------------------
 * bu2string: Maps a print basic unit type to a string.
 * returns  : The string mapping or "unknown basic unit type".
 * -------------------------------------------------------------------
 */
char	*
bu2string(basic_unit)
char	basic_unit;
{
	register int	i;

	struct bu_map_ent {
		char	basic_unit;
		char	*string;
	};

	static struct bu_map_ent bu_map[] = {
				{ AUR_BYTE, "byte" },
				{ AUR_CHAR, "char" },
				{ AUR_SHORT, "short" },
				{ AUR_INT32, "int32" },
				{ AUR_INT64, "int64" } 	};

	for (i = 0; i < sizeof (bu_map) / sizeof (struct bu_map_ent); i++)
		if (basic_unit == bu_map[i].basic_unit)
			return (gettext(bu_map[i].string));

	return (gettext("unknown basic unit type"));
}


/*
 * -------------------------------------------------------------------
 * eventmodifier2string: Maps event modifier flags to a readable string.
 * returns: The string mapping or "none".
 * -------------------------------------------------------------------
 */
char	*
eventmodifier2string(emodifier)
ushort emodifier;
{
	register int	i, j;
	static char	retstring[64];

	struct em_map_ent {
		int	mask;
		char	*string;
	};

	static struct em_map_ent em_map[] = {
		{ (int)PAD_MACUSE,	"mu" },	/* mac used/not used */
		{ (int)PAD_MACREAD,	"mr" },	/* mac read   check (1) */
		{ (int)PAD_MACWRITE,	"mw" },	/* mac write  check (2) */
		{ (int)PAD_MACSEARCH,	"ms" },	/* mac search check (3) */
		{ (int)PAD_MACKILL,	"mk" },	/* mac kill   check (4) */
		{ (int)PAD_MACTRACE,	"mt" },	/* mac trace  check (5) */
		{ (int)PAD_MACIOCTL,	"mi" },	/* mac ioctl  check (6) */
		{ (int)PAD_SPRIVUSE,	"sp" },	/* successfully used priv */
		{ (int)PAD_FPRIVUSE,	"fp" },	/* failed use of priv */
		{ (int)PAD_NONATTR,	"na" }	/* non-attributable event */
 	};

	retstring[0] = '\0';

	for (i = 0, j = 0; i < sizeof (em_map) / sizeof (struct em_map_ent);
			i++) {
		if ((int)emodifier & em_map[i].mask) {
			if (j++)
				(void) strcat(retstring, ":");
			(void) strcat(retstring, em_map[i].string);
		}
	}

	return (retstring);
}


/*
 * ---------------------------------------------------------
 * convert_char_to_string:
 *   Converts a byte to string depending on the print mode
 * input	: printmode, which may be one of AUP_BINARY,
 *		  AUP_OCTAL, AUP_DECIMAL, and AUP_HEX
 *		  c, which is the byte to convert
 * output	: p, which is a pointer to the location where
 *		  the resulting string is to be stored
 *  ----------------------------------------------------------
 */

int
convert_char_to_string(printmode, c, p)
char	printmode;
char	c;
char	*p;
{
	union {
		char	c1[4];
		int	c2;
	} dat;

	dat.c2 = 0;
	dat.c1[3] = c;

	if (printmode == AUP_BINARY)
		(void) convertbinary(p, &c, sizeof (char));
	else if (printmode == AUP_OCTAL)
		(void) sprintf(p, "%o", (int) dat.c2);
	else if (printmode == AUP_DECIMAL)
		(void) sprintf(p, "%d", c);
	else if (printmode == AUP_HEX)
		(void) sprintf(p, "0x%x", (int) dat.c2);
	else if (printmode == AUP_STRING)
		convertascii(p, &c, sizeof (char));
	return (0);
}

/*
 * --------------------------------------------------------------
 * convert_short_to_string:
 * Converts a short integer to string depending on the print mode
 * input	: printmode, which may be one of AUP_BINARY,
 *		AUP_OCTAL, AUP_DECIMAL, and AUP_HEX
 *		c, which is the short integer to convert
 * output	: p, which is a pointer to the location where
 *		the resulting string is to be stored
 * ---------------------------------------------------------------
 */
int
convert_short_to_string(printmode, c, p)
char	printmode;
short	c;
char	*p;
{
	union {
		short	c1[2];
		int	c2;
	} dat;

	dat.c2 = 0;
	dat.c1[1] = c;

	if (printmode == AUP_BINARY)
		(void) convertbinary(p, (char *) & c, sizeof (short));
	else if (printmode == AUP_OCTAL)
		(void) sprintf(p, "%o", (int) dat.c2);
	else if (printmode == AUP_DECIMAL)
		(void) sprintf(p, "%hd", c);
	else if (printmode == AUP_HEX)
		(void) sprintf(p, "0x%x", (int) dat.c2);
	else if (printmode == AUP_STRING)
		convertascii(p, (char *) & c, sizeof (short));
	return (0);
}

/*
 * ---------------------------------------------------------
 * convert_int32_to_string:
 * Converts a integer to string depending on the print mode
 * input	: printmode, which may be one of AUP_BINARY,
 *		AUP_OCTAL, AUP_DECIMAL, and AUP_HEX
 *		c, which is the integer to convert
 * output	: p, which is a pointer to the location where
 *		the resulting string is to be stored
 * ----------------------------------------------------------
 */
int
convert_int32_to_string(printmode, c, p)
char	printmode;
int32_t	c;
char	*p;
{
	if (printmode == AUP_BINARY)
		(void) convertbinary(p, (char *) & c, sizeof (int32_t));
	else if (printmode == AUP_OCTAL)
		(void) sprintf(p, "%o", c);
	else if (printmode == AUP_DECIMAL)
		(void) sprintf(p, "%d", c);
	else if (printmode == AUP_HEX)
		(void) sprintf(p, "0x%x", c);
	else if (printmode == AUP_STRING)
		convertascii(p, (char *) & c, sizeof (int));
	return (0);
}

/*
 * ---------------------------------------------------------
 * convert_int64_to_string:
 * Converts a integer to string depending on the print mode
 * input	: printmode, which may be one of AUP_BINARY,
 *		AUP_OCTAL, AUP_DECIMAL, and AUP_HEX
 *		c, which is the integer to convert
 * output	: p, which is a pointer to the location where
 *		the resulting string is to be stored
 * ----------------------------------------------------------
 */
int
convert_int64_to_string(printmode, c, p)
char	printmode;
int64_t	c;
char	*p;
{
	if (printmode == AUP_BINARY)
		(void) convertbinary(p, (char *) & c, sizeof (int64_t));
	else if (printmode == AUP_OCTAL)
		(void) sprintf(p, "%"PRIo64, c);
	else if (printmode == AUP_DECIMAL)
		(void) sprintf(p, "%"PRId64, c);
	else if (printmode == AUP_HEX)
		(void) sprintf(p, "0x%"PRIx64, c);
	else if (printmode == AUP_STRING)
		convertascii(p, (char *) & c, sizeof (int64_t));
	return (0);
}


/*
 * -----------------------------------------------------------
 * convertbinary:
 * Converts a unit c of 'size' bytes long into a binary string
 * and returns it into the position pointed to by p
 * ------------------------------------------------------------
 */
int
convertbinary(p, c, size)
char	*p;
char	*c;
int	size;
{
	char	*s, *t, *ss;
	int	i, j;

	if ((s = (char *)malloc(8 * size + 1)) == NULL)
		return (0);

	ss = s;

	/* first convert to binary */
	t = s;
	for (i = 0; i < size; i++) {
		for (j = 0; j < 8; j++)
			(void) sprintf(t++, "%d", ((*c >> (7 - j)) & (0x01)));
		c++;
	}
	*t = '\0';

	/* now string leading zero's if any */
	j = strlen(s) - 1;
	for (i = 0; i < j; i++) {
		if (*s != '0')
			break;
			else
			s++;
	}

	/* now copy the contents of s to p */
	t = p;
	for (i = 0; i < (8 * size + 1); i++) {
		if (*s == '\0') {
			*t = '\0';
			break;
		}
		*t++ = *s++;
	}
	free(ss);

	return (1);
}


/*
 * -------------------------------------------------------------------
 * hexconvert	: Converts a string of (size) bytes to hexadecimal, and
 *		returns the hexadecimal string.
 * returns	: - NULL if memory cannot be allocated for the string, or
 *		- pointer to the hexadecimal string if successful
 * -------------------------------------------------------------------
 */
char	*
hexconvert(c, size, chunk)
unsigned char	*c;
int	size;
int	chunk;
{
	register char	*s, *t;
	register int	i, j, k;
	int	numchunks;
	int	leftovers;

	if (size <= 0)
		return (NULL);

	if ((s = (char *)malloc((size * 5) + 1)) == NULL)
		return (NULL);

	if (chunk > size || chunk <= 0)
		chunk = size;

	numchunks = size / chunk;
	leftovers = size % chunk;

	t = s;
	for (i = j = 0; i < numchunks; i++) {
		if (j++) {
			*t = ' ';
			t++;
		}
		(void) sprintf(t, "0x");
		t += 2;
		for (k = 0; k < chunk; k++) {
			(void) sprintf(t, "%02x", *c++);
			t += 2;
		}
	}

	if (leftovers) {
		*t++ = ' ';
		*t++ = '0';
		*t++ = 'x';
		for (i = 0; i < leftovers; i++) {
			(void) sprintf(t, "%02x", *c++);
			t += 2;
		}
	}

	*t = '\0';
	return (s);
}


/*
 * -------------------------------------------------------------------
 * htp2string: Maps a print suggestion to a string.
 * returns   : The string mapping or "unknown print suggestion".
 * -------------------------------------------------------------------
 */
char	*
htp2string(print_sugg)
char	print_sugg;
{
	register int	i;

	struct htp_map_ent {
		char	print_sugg;
		char	*print_string;
	};

	/*
	 * Note to C2/audit developers:
	 *	If the names of these fundamental datatype are changed,
	 *	you will also have to modify structures.po to keep the
	 *	internationalization text up to date.
	 */

	static struct htp_map_ent htp_map[] = {
				{ AUP_BINARY, "binary" },
				{ AUP_OCTAL, "octal" },
				{ AUP_DECIMAL, "decimal" },
				{ AUP_HEX, "hexadecimal" },
				{ AUP_STRING, "string" } 	};

	for (i = 0; i < sizeof (htp_map) / sizeof (struct htp_map_ent); i++)
		if (print_sugg == htp_map[i].print_sugg)
			return (gettext(htp_map[i].print_string));

	return (gettext("unknown print suggestion"));
}

/*
 * ----------------------------------------------------------------------
 * pa_adr_short: Issues adrf_short to retrieve the next ADR item from the
 *		input stream pointed to by audit_adr, and prints it
 *		if status >= 0
 * return codes: -1 - error
 *		:  0 - successful
 * ----------------------------------------------------------------------
 */
static int
pa_adr_short(status, flag, endrec)
int	status;
int	flag;
int	endrec;
{
	short	c;

	if (status >= 0) {
		if (adrf_short(audit_adr, &c, 1) == 0) {
			uvaltype = PRA_SHORT;
			uval.short_val = c;
			return (pa_print(flag, endrec));
		} else
			return (-1);
	} else
		return (status);
}


/*
 * -----------------------------------------------------------------------
 * pa_adr_shorthex: Issues adrf_short to retrieve the next ADR item from the
 *			input stream pointed to by audit_adr, and prints it
 *			in hexadecimal if status >= 0
 * return codes  : -1 - error
 *		:  0 - successful
 * -----------------------------------------------------------------------
 */
static int
pa_adr_shorthex(status, flag, endrec)
int	status;
int	flag;
int	endrec;
{
	short	s;
	int	returnstat = 0;

	if (status >= 0) {
		if (returnstat == 0) {
			if ((returnstat = adrf_short(audit_adr, &s, 1)) == 0) {
				uvaltype = PRA_STRING;
				uval.string_val = hexconvert((char *) & s,
						sizeof (s),
						sizeof (s));
				if (uval.string_val) {
					returnstat = pa_print(flag, endrec);
					free(uval.string_val);
				}
			}
		}
		return (returnstat);
	} else
		return (status);
}


/*
 * -----------------------------------------------------------------------
 * pa_adr_string: Issues adrf_string to retrieve the next ADR item from the
 *		  input stream pointed to by audit_adr, and prints it
 *		  if status >= 0
 * return codes : -1 - error
 *		:  0 - successful
 * -----------------------------------------------------------------------
 */
static int
pa_adr_string(status, flag, endrec)
int	status;
int	flag;
int	endrec;
{
	char	*c;
	char	*p;
	short	length;
	int	returnstat = 0;

	/*
	 * note that adrf_string uses adrf_short to read a length and
	 * adrf_char to read that many bytes of data. We need to know
	 * how much space to allocate for our string so we have to read
	 * the length first
	 */
	if (status < 0)
		return (status);

	if ((returnstat = adrf_short(audit_adr, &length, 1)) != 0)
		return (returnstat);
	if ((c = (char *)malloc(length + 1)) == NULL)
		return (-1);
	if ((p = (char *)malloc((length * 2) + 1)) == NULL) {
		free(c);
		return (-1);
	}
	if ((returnstat = adrf_char(audit_adr, c, length)) != 0) {
		free(c);
		free(p);
		return (returnstat);
	}
	convertascii(p, c, length - 1);
	uvaltype = PRA_STRING;
	uval.string_val = p;
	returnstat = pa_print(flag, endrec);
	free(c);
	free(p);
	return (returnstat);
}

static int
pa_exec_string(status, flag, endrec)
	int status;
	int flag;
	int endrec;
{
	int returnstat = 0;
	char c;

	if (status < 0)
		return (status);
	returnstat = adrf_char(audit_adr, &c, 1);
	while ((returnstat >= 0) && (c != (char) 0)) {
		c = (char)toascii(c);
		if ((int)iscntrl(c)) {
			putchar('^');
			putchar((int) (c + 0x40));
		} else
			putchar((int) c);
		returnstat = adrf_char(audit_adr, &c, 1);
	}
	if (returnstat < 0) {
		putchar('\n');
		return (returnstat);
	}
	if (ONELINE != 1) {
		if ((flag == 1) || (endrec == 1))
			(void) printf("\n");
		else
			(void) printf("%s", SEPARATOR);
	} else {
		if (endrec == 1)
			(void) printf("\n");
		else
			(void) printf("%s", SEPARATOR);
	}
	(void) fflush(stdout);
	return (returnstat);
}

/*
 * anchor a path name with a slash
 */
char	*
anchor_path(sp)
char	*sp;
{
	char	*dp; /* destination path */
	char	*tp; /* temporary path */

	if ((dp = tp = (char *)calloc(1, strlen(sp) + 2)) == (char *)0)
		return ((char *)0);

	*dp++ = '/';

	(void) strcpy(dp, sp);

	return (tp);
}


/*
 * copy path to collapsed path.
 * collapsed path does not contain:
 *	successive slashes
 *	instances of dot-slash
 *	instances of dot-dot-slash
 * passed path must be anchored with a '/'
 */
char	*
collapse_path(s)
char	*s; /* source path */
{
	int	id;	/* index of where we are in destination string */
	int	is;		/* index of where we are in source string */
	int	slashseen;	/* have we seen a slash */
	int	ls;		/* length of source string */

	ls = strlen(s) + 1;

	slashseen = 0;
	for (is = 0, id = 0; is < ls; is++) {
		/* thats all folks, we've reached the end of input */
		if (s[is] == '\0') {
			if (id > 1 && s[id-1] == '/') {
				--id;
			}
			s[id++] = '\0';
			break;
		}
		/* previous character was a / */
		if (slashseen) {
			if (s[is] == '/')
				continue;	/* another slash, ignore it */
		} else if (s[is] == '/') {
			/* we see a /, just copy it and try again */
			slashseen = 1;
			s[id++] = '/';
			continue;
		}
		/* /./ seen */
		if (s[is] == '.' && s[is+1] == '/') {
			is += 1;
			continue;
		}
		/* XXX/. seen */
		if (s[is] == '.' && s[is+1] == '\0') {
			if (id > 1)
				id--;
			continue;
		}
		/* XXX/.. seen */
		if (s[is] == '.' && s[is+1] == '.' && s[is+2] == '\0') {
			is += 1;
			if (id > 0)
				id--;
			while (id > 0 && s[--id] != '/');
			id++;
			continue;
		}
		/* XXX/../ seen */
		if (s[is] == '.' && s[is+1] == '.' && s[is+2] == '/') {
			is += 2;
			if (id > 0)
				id--;
			while (id > 0 && s[--id] != '/');
			id++;
			continue;
		}
		while (is < ls && (s[id++] = s[is++]) != '/');
		is--;
	}
	return (s);
}

/*
 * -----------------------------------------------------------------------
 * pa_adr_u_int32: Issues adrf_u_int32 to retrieve the next ADR item from
 *		  the input stream pointed to by audit_adr, and prints it
 *		  if status = 0
 * return codes : -1 - error
 *		:  0 - successful
 * -----------------------------------------------------------------------
 */


int
pa_adr_u_int32(status, flag, endrec)
int	status;
int	flag;
int	endrec;
{
	uint32_t c;

	if (status >= 0) {
		if (adrf_u_int32(audit_adr, &c, 1) == 0) {
			uvaltype = PRA_UINT32;
			uval.uint32_val = c;
			return (pa_print(flag, endrec));
		} else
			return (-1);
	} else
		return (status);
}



/*
 * -----------------------------------------------------------------------
 * pa_adr_u_int64: Issues adrf_u_int64 to retrieve the next ADR item from the
 *		  input stream pointed to by audit_adr, and prints it
 *		  if status = 0
 * return codes : -1 - error
 *		:  0 - successful
 * -----------------------------------------------------------------------
 */
static int
pa_adr_u_int64(status, flag, endrec)
int	status;
int	flag;
int	endrec;
{
	uint64_t c;

	if (status >= 0) {
		if (adrf_u_int64(audit_adr, &c, 1) == 0) {
			uvaltype = PRA_UINT64;
			uval.uint64_val = c;
			return (pa_print(flag, endrec));
		} else
			return (-1);
	} else
		return (status);
}


/*
 * -----------------------------------------------------------------------
 * pa_adr_u_short: Issues adrf_u_short to retrieve the next ADR item from
 *			the input stream pointed to by audit_adr, and prints it
 *			if status = 0
 * return codes : -1 - error
 *		:  0 - successful
 * -----------------------------------------------------------------------
 */
static int
pa_adr_u_short(status, flag, endrec)
int	status;
int	flag;
int	endrec;
{
	u_short c;

	if (status >= 0) {
		if (adrf_u_short(audit_adr, &c, 1) == 0) {
			uvaltype = PRA_USHORT;
			uval.ushort_val = c;
			return (pa_print(flag, endrec));
		} else
			return (-1);
	} else
		return (status);
}


/*
 * -----------------------------------------------------------------------
 * pa_mode	: Issues adrf_u_short to retrieve the next ADR item from
 *		the input stream pointed to by audit_adr, and prints it
 *		in octal if status = 0
 * return codes : -1 - error
 *		:  0 - successful
 * -----------------------------------------------------------------------
 */
static int
pa_mode(status, flag, endrec)
int	status;
int	flag;
int	endrec;
{
	uint32_t c;

	if (status >= 0) {
		if (adrf_u_int32(audit_adr, &c, 1) == 0) {
			uvaltype = PRA_LOCT;
			uval.uint32_val = c;
			return (pa_print(flag, endrec));
		} else
			return (-1);
	} else
		return (status);
}


/*
 * -----------------------------------------------------------------------
 * pa_pw_uid()	: Issues adrf_u_int32 to reads uid from input stream
 *		pointed to by audit_adr, and displays it in either
 *		raw form or its ASCII representation, if status >= 0.
 * input   	:
 * output	:
 * return codes : -1 - error
 * 		:  1 - warning, passwd entry not found
 *		:  0 - successful
 * -----------------------------------------------------------------------
 */
static int
pa_pw_uid(status, flag, endrec)
int	status;
int	flag;
int	endrec;
{
	int	returnstat = 0;
	struct passwd *pw;
	uint32_t uid;

	if (status < 0)
		return (status);

	if (adrf_u_int32(audit_adr, &uid, 1) != 0)
		/* cannot retrieve uid */
		return (-1);

	if (format != RAWM) {
		/* get password file entry */
		if ((pw = getpwuid(uid)) == NULL) {
			returnstat = 1;
		} else {
			/* print in ASCII form */
			uvaltype = PRA_STRING;
			uval.string_val = pw->pw_name;
			returnstat = pa_print(flag, endrec);
		}
	}
	/* print in integer form */
	if (format == RAWM || returnstat == 1) {
		uvaltype = PRA_INT32;
		uval.int32_val = uid;
		returnstat = pa_print(flag, endrec);
	}
	return (returnstat);
}


/*
 * -----------------------------------------------------------------------
 * pa_gr_uid()	: Issues adrf_u_int32 to reads group uid from input stream
 *			pointed to by audit_adr, and displays it in either
 *			raw form or its ASCII representation, if status >= 0.
 * input   	:
 * output	:
 * return codes : -1 - error
 * 		:  1 - warning, passwd entry not found
 *		:  0 - successful
 * -----------------------------------------------------------------------
 */
static int
pa_gr_uid(status, flag, endrec)
int	status;
int	flag;
int	endrec;
{
	int	returnstat = 0;
	struct group *gr;
	uint32_t gid;

	if (status < 0)
		return (status);

	if (adrf_u_int32(audit_adr, &gid, 1) != 0)
		/* cannot retrieve gid */
		return (-1);

	if (format != RAWM) {
		/* get group file entry */
		if ((gr = getgrgid(gid)) == NULL) {
			returnstat = 1;
		} else {
			/* print in ASCII form */
			uvaltype = PRA_STRING;
			uval.string_val = gr->gr_name;
			returnstat = pa_print(flag, endrec);
		}
	}
	/* print in integer form */
	if (format == RAWM || returnstat == 1) {
		uvaltype = PRA_INT32;
		uval.int32_val = gid;
		returnstat = pa_print(flag, endrec);
	}
	return (returnstat);
}


/*
 * -----------------------------------------------------------------------
 * pa_pw_uid_gr_uid()	: Issues adrf_u_int32 to reads uid or group uid
 *			from input stream 
 *			pointed to by audit_adr, and displays it in either
 *			raw form or its ASCII representation, if status >= 0.
 * input   	:
 * output	:
 * return codes : -1 - error
 * 		:  1 - warning, passwd entry not found
 *		:  0 - successful
 * -----------------------------------------------------------------------
 */
static int
pa_pw_uid_gr_gid(status, flag, endrec)
int	status;
int	flag;
int	endrec;
{
	int	returnstat = 0;
	uint32_t	value;

	if (status < 0)
		return (status);

	/* get value of a_type */
	if ((returnstat = adrf_u_int32(audit_adr, &value, 1)) != 0)
		return (returnstat);

	uvaltype = PRA_UINT32;
	uval.uint32_val = value;
	if ((returnstat = pa_print(flag, endrec)) != 0)
		return (returnstat);
	/*
	printf("value=%d ret=%d\n", value, returnstat);
	*/
	switch (value) {
		case USER_OBJ:
		case USER:
			returnstat = pa_pw_uid(returnstat, flag, endrec);
			break;
		case GROUP_OBJ:
		case GROUP:
			returnstat = pa_gr_uid(returnstat, flag, endrec);
			break;
		case CLASS_OBJ:
		    returnstat = adrf_u_int32(audit_adr, &value, 1);
		    if (returnstat != 0)
			return (returnstat);

		    if (format != RAWM) {
			uvaltype = PRA_STRING;
			uval.string_val = "mask";
			returnstat = pa_print(flag, endrec);
		    } else {
			uvaltype = PRA_UINT32;
			uval.uint32_val = value;
			if ((returnstat = pa_print(flag, endrec)) != 0)
				return (returnstat);
		    }
		    break;
		case OTHER_OBJ:
		    returnstat = adrf_u_int32(audit_adr, &value, 1);
		    if (returnstat != 0)
			return (returnstat);

		    if (format != RAWM) {
			uvaltype = PRA_STRING;
			uval.string_val = "other";
			returnstat = pa_print(flag, endrec);
		    } else {
			uvaltype = PRA_UINT32;
			uval.uint32_val = value;
			if ((returnstat = pa_print(flag, endrec)) != 0)
				return (returnstat);
		    }
		    break;
		default:
		    returnstat = adrf_u_int32(audit_adr, &value, 1);
		    if (returnstat != 0)
			return (returnstat);

		    if (format != RAWM) {
			uvaltype = PRA_STRING;
			uval.string_val = "unrecognized";
			returnstat = pa_print(flag, endrec);
		    } else {
			uvaltype = PRA_UINT32;
			uval.uint32_val = value;
			if ((returnstat = pa_print(flag, endrec)) != 0)
				return (returnstat);
		    }
	}


	return (returnstat);
}


/*
 * -----------------------------------------------------------------------
 * pa_event_modifier(): Issues adrf_u_short to retrieve the next ADR item from
 *		  the input stream pointed to by audit_adr.  This is the
 *		  event type, and is displayed in hex;
 * input   	:
 * output	:
 * return codes : -1 - error
 *		:  0 - successful
 * -----------------------------------------------------------------------
 */
static int
pa_event_modifier(status,  flag, endrec)
int	status;
int	flag;
int	endrec;
{
	int	returnstat = 0;
	ushort emodifier;

	if (status < 0)
		return (status);

	if ((returnstat = adrf_u_short(audit_adr, &emodifier, 1)) != 0)
		return (returnstat);

	uvaltype = PRA_STRING;

	if (format != RAWM)  {
		uval.string_val = eventmodifier2string(emodifier);
		returnstat = pa_print(flag, endrec);
	} else {
		uval.string_val = hexconvert((char *) & emodifier,
			sizeof (emodifier),
			sizeof (emodifier));
		if (uval.string_val) {
			returnstat = pa_print(flag, endrec);
			free(uval.string_val);
		}
	}

	return (returnstat);
}


/*
 * -----------------------------------------------------------------------
 * pa_event_type(): Issues adrf_u_short to retrieve the next ADR item from
 *		  the input stream pointed to by audit_adr.  This is the
 *		  event type, and is displayed in either raw or
 *		  ASCII form as appropriate
 * input   	:
 * output	:
 * return codes : -1 - error
 *		:  0 - successful
 * -----------------------------------------------------------------------
 */
static int
pa_event_type(status,  flag, endrec)
int	status;
int	flag;
int	endrec;
{
	u_short etype;
	int	returnstat = 0;
	au_event_ent_t * p_event;

	if (status >= 0) {
		if ((returnstat = adrf_u_short(audit_adr, &etype, 1)) == 0) {
			if (format != RAWM) {
				uvaltype = PRA_STRING;
				if (CACHE) {
					setauevent();
					p_event = getauevnum(etype);
				} else {
					(void) cacheauevent(&p_event, etype);
				}
				if (p_event != NULL) {
					if (format == SHORTM)
						uval.string_val =
							p_event->ae_name;
					else
						uval.string_val =
							p_event->ae_desc;
				} else {
					uval.string_val = gettext(
						"invalid event number");
				}
				returnstat = pa_print(flag, endrec);
			} else {
				uvaltype = PRA_SHORT;
				uval.short_val = etype;
				returnstat = pa_print(flag, endrec);
			}
		}
		return (returnstat);
	} else
		return (status);

}


/*
 * -----------------------------------------------------------------------
 * pa_time()	: Issues adrf_u_int to retrieve the next ADR item from
 *		the input stream pointed to by audit_adr.  This is the
 *		time in seconds, and is displayed in either raw or
 *		ASCII form as appropriate
 * input	:
 * output	:
 * return codes : -1 - error
 * 		:  1 - warning, passwd entry not found
 *		:  0 - successful
 * -----------------------------------------------------------------------
 */
static int
pa_time32(status, flag, endrec)
int	status;
int	flag;
int	endrec;
{
	int32_t t32;
	time_t c;
	struct tm *tm;
	char	time_created[80];
	int	returnstat = 0;

	if (status >= 0) {
	    if ((returnstat = adrf_u_int32(audit_adr, (uint32_t *) &t32, 1))
				== 0) {
			c = (time_t) t32;
			if (format != RAWM) {	/* try to convert to ASCII */
				tm = localtime(&c);
				strftime(time_created, 80, (char *)0, tm);
				uvaltype = PRA_STRING;
				uval.string_val = time_created;
				returnstat = pa_print(flag, endrec);
			} else {
				uvaltype = PRA_UINT32;
				uval.uint32_val = c;
				returnstat = pa_print(flag, endrec);
			}
		}
		return (returnstat);
	} else
		return (status);
}

static int
pa_time64(status, flag, endrec)
int	status;
int	flag;
int	endrec;
{
	int64_t t64;
	time_t c;
	struct tm *tm;
	char	time_created[80];
	int	returnstat = 0;

	if (status >= 0) {
	    if ((returnstat = adrf_u_int64(audit_adr, (uint64_t *) &t64, 1))
				== 0) {
#if (!defined(_LP64))
			if (t64 < (time_t)INT32_MIN || t64 > (time_t)INT32_MAX)
				c = 0;
			else
				c = (time_t) t64;
#else
			c = (time_t) t64;
#endif
			if (format != RAWM) {	/* try to convert to ASCII */
				tm = localtime(&c);
				strftime(time_created, 80, (char *)0, tm);
				uvaltype = PRA_STRING;
				uval.string_val = time_created;
				returnstat = pa_print(flag, endrec);
			} else {
				uvaltype = PRA_UINT64;
				uval.uint64_val = c;
				returnstat = pa_print(flag, endrec);
			}
		}
		return (returnstat);
	} else
		return (status);
}


/*
 * -----------------------------------------------------------------------
 * pa_msec()	: Issues adrf_u_int to retrieve the next ADR item from
 *		  the input stream pointed to by audit_adr.  This is the
 *		  time in microseconds, and is discarded
 * input   	:
 * output	:
 * return codes : -1 - error
 * 		:  1 - warning, passwd entry not found
 *		:  0 - successful
 * -----------------------------------------------------------------------
 */
static int
pa_msec32(status, flag, endrec)
int	status;
int	flag;
int	endrec;
{
	uint32_t c32;
	int	returnstat = 0;
	char	msec[64];

	if (status < 0)
		return (status);

	if ((returnstat = adrf_u_int32(audit_adr, &c32, 1)) != 0)
		return (returnstat);

	if (format != RAWM) {
		uvaltype = PRA_STRING;
		/*
		 * TRANSLATION_NOTE
		 *	The following string is used to indicate the time
		 *	in microseconds, added to the string used to
		 *	represent the date.  In English, the result would
		 *	would look like this:
		 *		Fri Feb 26 10:04:50 1993, + 860000000 msec
		 */
		(void) sprintf(msec, gettext(" + %d msec"), c32);
		uval.string_val = msec;
	} else {
		uvaltype = PRA_UINT32;
		uval.uint32_val = c32;
	}

	returnstat = pa_print(flag, endrec);
	return (returnstat);
}

static int
pa_msec64(status, flag, endrec)
int	status;
int	flag;
int	endrec;
{
	uint64_t c64;
	int	returnstat = 0;
	char	msec[64];

	if (status < 0)
		return (status);

	if ((returnstat = adrf_u_int64(audit_adr, &c64, 1)) != 0)
		return (returnstat);

	if (format != RAWM) {
		uvaltype = PRA_STRING;
		/*
		 * TRANSLATION_NOTE
		 *	The following string is used to indicate the time
		 *	in microseconds, added to the string used to
		 *	represent the date.  In English, the result would
		 *	would look like this:
		 *		Fri Feb 26 10:04:50 1993, + 860000000 msec
		 */
		(void) sprintf(msec, gettext(" + %"PRId64" msec"), c64);
		uval.string_val = msec;
	} else {
		uvaltype = PRA_UINT64;
		uval.uint64_val = c64;
	}

	returnstat = pa_print(flag, endrec);
	return (returnstat);
}

/*
 * -----------------------------------------------------------------------
 * pa_ipc_id()	: Issues adrf_u_char to reads IPC id from input stream
 *		pointed to by audit_adr, and displays it in either
 *		raw form or its ASCII representation, if status >= 0.
 * input   	:
 * output	:
 * return codes : -1 - error
 * 		:  1 - warning, passwd entry not found
 *		:  0 - successful
 * -----------------------------------------------------------------------
 */
int
pa_ipc_id(status, flag, endrec)
int	status;
int	flag;
int	endrec;
{
	int	returnstat = 0;
	u_char ipcid;

	if (status >= 0) {
		if (adrf_u_char(audit_adr, &ipcid, 1) == 0) {
			if (format != RAWM) {
				/* print in ASCII form */
				uvaltype = PRA_STRING;
				switch (ipcid) {
				case AT_IPC_MSG:
					uval.string_val = "msg";
					break;
				case AT_IPC_SEM:
					uval.string_val = "sem";
					break;
				case AT_IPC_SHM:
					uval.string_val = "shm";
					break;
				}
				returnstat = pa_print(0, 0);
			}
			/* print in integer form */
			if (format == RAWM || returnstat == 1) {
				uvaltype = PRA_BYTE;
				uval.char_val = ipcid;
				returnstat = pa_print(flag, endrec);
			}
			return (returnstat);
		} else
			/* cannot retrieve ipc ID */
			return (-1);
	} else
		return (status);
}

#ifdef SunOS_CMW

/*
 * -----------------------------------------------------------------------
 * pa_xatom_token(): Process Xatom token and display contents in hex.
 *
 * return codes		: -1 - error
 *			:  0 - successful
 *
 * Format of xatom token:
 *	token id		adr_char
 * 	length			adr_short
 * 	atom			adr_char length times
 * -----------------------------------------------------------------------
 */
int
pa_xatom_token()
{
	int	returnstat = 0;
	short	size;
	char	*charp;

	returnstat = pa_tokenid();

	/* print the size of the token */
	if (returnstat >= 0) {
		if (adrf_short(audit_adr, &size, 1) == 0) {
			uvaltype = PRA_SHORT;
			uval.short_val = size;
			returnstat = pa_print(0, 0);
		} else
			returnstat = -1;
	}

	/* now print out the data field in hexadecimal */
	if (returnstat >= 0) {
		if ((charp = (char *)malloc(size * sizeof (char))) == NULL)
			returnstat = -1;
		else {
			if ((returnstat = adrf_char(audit_adr, charp, size))
					== 0) {
				/* print out in hexadecimal format */
				uvaltype = PRA_STRING;
				uval.string_val = hexconvert(charp, size, size);
				if (uval.string_val) {
					returnstat = pa_print(1, 0);
					free(uval.string_val);
				}
			}
			free(charp);
		}
	}
	return (returnstat);
}


/*
 * -----------------------------------------------------------------------
 * pa_xobject_token(): Process Xobject token and display contents
 *
 * return codes		: -1 - error
 *			:  0 - successful
 * NOTE: At the time of call, the xatom token id has been retrieved
 *
 * Format of xobj
 *	text token id		adr_char
 * 	object type		adr_int32
 * 	XID 			adr_u_int32
 * 	creator uid		adr_pw_uid
 * -----------------------------------------------------------------------
 */
int
pa_xobject_token()
{
	int	returnstat = 0;
	int	xobj;
	int	xid;

	if ((returnstat = pa_tokenid()) < 0)
		return (returnstat);

	/*
	 * X object type
	 */
	if ((returnstat = adrf_int32(audit_adr, (int32_t *)&xobj, 1)) != 0)
		return (returnstat);

	if (format != RAWM) {
		uvaltype = PRA_STRING;
		uval.string_val = xobj2str(xobj);
	} else {
		uvaltype = PRA_INT32;
		uval.int32_val = xobj;
	}

	if ((returnstat = pa_print(0, 0)) < 0)
		return (returnstat);

	/*
	 * XID
	 */
	if ((returnstat = adrf_int32(audit_adr, (int32_t *)&xid, 1)) != 0)
		return (returnstat);

	uvaltype = PRA_STRING;
	uval.string_val =
		hexconvert((char *) & xid, sizeof (xid), sizeof (xid));
	if (uval.string_val) {
		returnstat = pa_print(0, 0);
		free(uval.string_val);
	}
	if (returnstat < 0)
		return (returnstat);

	/*
	 * creator UID
	 */
	returnstat = pa_pw_uid(returnstat, 1, 0);
	return (returnstat);
}


/*
 * -----------------------------------------------------------------------
 * pa_xprotocol_token(): Process Xprotocol token and display contents
 *
 * return codes		: -1 - error
 *			:  0 - successful
 * NOTE: At the time of call, the xatom token id has been retrieved
 *
 * Format of text token:
 *	token id		adr_char
 * 	protocol id		adr_int
 * -----------------------------------------------------------------------
 */
int
pa_xprotocol_token()
{
	int32_t	xproto;
	int	returnstat = 0;

	returnstat = pa_tokenid();

	if (returnstat < 0)
		return (returnstat);

	if (adrf_int32(audit_adr, &xproto, 1) == 0) {
		if (format != RAWM) {
			uvaltype = PRA_STRING;
			uval.string_val = xproto2str(xproto);
			returnstat = pa_print(1, 0);
		} else {
			uvaltype = PRA_INT32;
			uval.int32_val = xproto;
			returnstat = pa_print(1, 0);
		}
	} else
		return (-1);

	return (returnstat);

}


/*
 * -----------------------------------------------------------------------
 * pa_xselection_token(): Process Xselect token and display contents in hex
 *
 * return codes		: -1 - error
 *			:  0 - successful
 * NOTE: At the time of call, the xatom token id has been retrieved
 *
 * Format of xselect token
 *	text token id		adr_char
 * 	text			adr_string
 * 	text			adr_short
 * -----------------------------------------------------------------------
 */
int
pa_xselection_token()
{
	int	returnstat;
	short	length;
	char	*xselection_type;
	char	*windata;
	char	*windata_converted;

	if ((returnstat = pa_tokenid()) < 0)
		return (returnstat);
	if ((returnstat = pa_adr_string(returnstat, 0, 0)) < 0)
		return (returnstat);
	if ((returnstat = adrf_short(audit_adr, &length, 1)) != 0)
		return (returnstat);
	if ((xselection_type = (char *)malloc(length)) == NULL)
		return (-1);
	if ((returnstat = adrf_char(audit_adr, xselection_type, length)) != 0) {
		free(xselection_type);
		return (returnstat);
	}
	uvaltype = PRA_STRING;
	uval.string_val = xselection_type;
	(void) pa_print(0, 0);
	if ((returnstat = adrf_short(audit_adr, &length, 1)) != 0) {
		free(xselection_type);
		return (returnstat);
	}
	uvaltype = PRA_SHORT;
	uval.short_val = length;
	(void) pa_print(0, 0);
	if ((windata = (char *)malloc(length)) == NULL) {
		free(xselection_type);
		return (-1);
	}
	if ((returnstat = adrf_char(audit_adr, windata, length)) != 0) {
		free(xselection_type);
		free(windata);
		return (returnstat);
	}
	if (strcmp(xselection_type, "STRING") == 0 ||
		strcmp(xselection_type, "TEXT") == 0) {
		/*
		 * print the data field as an ASCII string
		 */
		if ((windata_converted = (char *)malloc((length * 2) + 1))
				== NULL) {
			free(xselection_type);
			free(windata);
			return (-1);
		}
		uvaltype = PRA_STRING;
		convertascii(windata_converted, windata, length);
		uval.string_val = windata_converted;
		(void) pa_print(1, 0);
		free(windata_converted);
	} else if (strcmp(xselection_type, "INTEGER") == 0) {
		/*
		 * print the data field as an integer
		 */
		uvaltype = PRA_INT32;
		(void) memcpy((char *) & uval.int32_val, windata,
			sizeof (uval.int32_val));
		(void) pa_print(1, 0);
	} else {
		/*
		 * print the data field in hex
		 */
		if ((windata_converted = hexconvert(windata, length, length))
				== NULL) {
			free(xselection_type);
			free(windata);
			return (-1);
		}
		uvaltype = PRA_STRING;
		uval.string_val = windata_converted;
		(void) pa_print(1, 0);
		free(windata_converted);
	}
	free(xselection_type);
	free(windata);
	return (0);
}


static char	*
xobj2str(xobj)
int	xobj;
{
#define	IsNULL		0
#define	IsWindow	1
#define	IsPixmap	2
#define	IsDrawable	3
#define	IsWindowPixmap	4
#define	IsCMWextension  9

#define	IsColormap	6
#define	IsGC		7

	register int	i;

	struct xobj_map_ent {
		int	xobj;
		char	*xobj_str;
	};

	static struct xobj_map_ent xobj_map[] = {
				{ IsNULL, "IsNULL" },
				{ IsWindow, "IsWindow" },
				{ IsPixmap, "IsPixmap" },
				{ IsDrawable, "IsDrawable" },
				{ IsWindowPixmap, "IsWindowPixmap" },
				{ IsCMWextension, "IsCMWextension" },
				{ IsColormap, "IsColormap" },
				{ IsGC, "IsGC" } 	};

	for (i = 0; i < sizeof (xobj_map) / sizeof (struct xobj_map_ent); i++) {
		if (xobj == xobj_map[i].xobj)
			return (xobj_map[i].xobj_str);
	}

	return ("unknown X object type");
}


static char	*
xproto2str(xproto)
int	xproto;
{
	register int	i;

	struct xproto_map {
		int	xproto;
		char	*xproto_str;
	};

	static struct xproto_map std_xproto_map[] = {
		{ X_CreateWindow, "X_CreateWindow" },
		{ X_ChangeWindowAttributes, "X_ChangeWindowAttributes" },
		{ X_GetWindowAttributes, "X_GetWindowAttributes" },
		{ X_DestroyWindow, "X_DestroyWindow" },
		{ X_DestroySubwindows, "X_DestroySubwindows" },
		{ X_ChangeSaveSet, "X_ChangeSaveSet" },
		{ X_ReparentWindow, "X_ReparentWindow" },
		{ X_MapWindow, "X_MapWindow" },
		{ X_MapSubwindows, "X_MapSubwindows" },
		{ X_UnmapWindow, "X_UnmapWindow" },
		{ X_UnmapSubwindows, "X_UnmapSubwindows" },
		{ X_ConfigureWindow, "X_ConfigureWindow" },
		{ X_CirculateWindow, "X_CirculateWindow" },
		{ X_GetGeometry, "X_GetGeometry" },
		{ X_QueryTree, "X_QueryTree" },
		{ X_InternAtom, "X_InternAtom" },
		{ X_GetAtomName, "X_GetAtomName" },
		{ X_ChangeProperty, "X_ChangeProperty" },
		{ X_DeleteProperty, "X_DeleteProperty" },
		{ X_GetProperty, "X_GetProperty" },
		{ X_ListProperties, "X_ListProperties" },
		{ X_SetSelectionOwner, "X_SetSelectionOwner" },
		{ X_GetSelectionOwner, "X_GetSelectionOwner" },
		{ X_ConvertSelection, "X_ConvertSelection" },
		{ X_SendEvent, "X_SendEvent" },
		{ X_GrabPointer, "X_GrabPointer" },
		{ X_UngrabPointer, "X_UngrabPointer" },
		{ X_GrabButton, "X_GrabButton" },
		{ X_UngrabButton, "X_UngrabButton" },
		{ X_ChangeActivePointerGrab, "X_ChangeActivePointerGrab" },
		{ X_GrabKeyboard, "X_GrabKeyboard" },
		{ X_UngrabKeyboard, "X_UngrabKeyboard" },
		{ X_GrabKey, "X_GrabKey" },
		{ X_UngrabKey, "X_UngrabKey" },
		{ X_AllowEvents, "X_AllowEvents" },
		{ X_GrabServer, "X_GrabServer" },
		{ X_UngrabServer, "X_UngrabServer" },
		{ X_QueryPointer, "X_QueryPointer" },
		{ X_GetMotionEvents, "X_GetMotionEvents" },
		{ X_TranslateCoords, "X_TranslateCoords" },
		{ X_WarpPointer, "X_WarpPointer" },
		{ X_SetInputFocus, "X_SetInputFocus" },
		{ X_GetInputFocus, "X_GetInputFocus" },
		{ X_QueryKeymap, "X_QueryKeymap" },
		{ X_OpenFont, "X_OpenFont" },
		{ X_CloseFont, "X_CloseFont" },
		{ X_QueryFont, "X_QueryFont" },
		{ X_QueryTextExtents, "X_QueryTextExtents" },
		{ X_ListFonts, "X_ListFonts" },
		{ X_ListFontsWithInfo, "X_ListFontsWithInfo" },
		{ X_SetFontPath, "X_SetFontPath" },
		{ X_GetFontPath, "X_GetFontPath" },
		{ X_CreatePixmap, "X_CreatePixmap" },
		{ X_FreePixmap, "X_FreePixmap" },
		{ X_CreateGC, "X_CreateGC" },
		{ X_ChangeGC, "X_ChangeGC" },
		{ X_CopyGC, "X_CopyGC" },
		{ X_SetDashes, "X_SetDashes" },
		{ X_SetClipRectangles, "X_SetClipRectangles" },
		{ X_FreeGC, "X_FreeGC" },
		{ X_ClearArea, "X_ClearArea" },
		{ X_CopyArea, "X_CopyArea" },
		{ X_CopyPlane, "X_CopyPlane" },
		{ X_PolyPoint, "X_PolyPoint" },
		{ X_PolyLine, "X_PolyLine" },
		{ X_PolySegment, "X_PolySegment" },
		{ X_PolyRectangle, "X_PolyRectangle" },
		{ X_PolyArc, "X_PolyArc" },
		{ X_FillPoly, "X_FillPoly" },
		{ X_PolyFillRectangle, "X_PolyFillRectangle" },
		{ X_PolyFillArc, "X_PolyFillArc" },
		{ X_PutImage, "X_PutImage" },
		{ X_GetImage, "X_GetImage" },
		{ X_PolyText8, "X_PolyText8" },
		{ X_PolyText16, "X_PolyText16" },
		{ X_ImageText8, "X_ImageText8" },
		{ X_ImageText16, "X_ImageText16" },
		{ X_CreateColormap, "X_CreateColormap" },
		{ X_FreeColormap, "X_FreeColormap" },
		{ X_CopyColormapAndFree, "X_CopyColormapAndFree" },
		{ X_InstallColormap, "X_InstallColormap" },
		{ X_UninstallColormap, "X_UninstallColormap" },
		{ X_ListInstalledColormaps, "X_ListInstalledColormaps" },
		{ X_AllocColor, "X_AllocColor" },
		{ X_AllocNamedColor, "X_AllocNamedColor" },
		{ X_AllocColorCells, "X_AllocColorCells" },
		{ X_AllocColorPlanes, "X_AllocColorPlanes" },
		{ X_FreeColors, "X_FreeColors" },
		{ X_StoreColors, "X_StoreColors" },
		{ X_StoreNamedColor, "X_StoreNamedColor" },
		{ X_QueryColors, "X_QueryColors" },
		{ X_LookupColor, "X_LookupColor" },
		{ X_CreateCursor, "X_CreateCursor" },
		{ X_CreateGlyphCursor, "X_CreateGlyphCursor" },
		{ X_FreeCursor, "X_FreeCursor" },
		{ X_RecolorCursor, "X_RecolorCursor" },
		{ X_QueryBestSize, "X_QueryBestSize" },
		{ X_QueryExtension, "X_QueryExtension" },
		{ X_ListExtensions, "X_ListExtensions" },
		{ X_ChangeKeyboardMapping, "X_ChangeKeyboardMapping" },
		{ X_GetKeyboardMapping, "X_GetKeyboardMapping" },
		{ X_ChangeKeyboardControl, "X_ChangeKeyboardControl" },
		{ X_GetKeyboardControl, "X_GetKeyboardControl" },
		{ X_Bell, "X_Bell" },
		{ X_ChangePointerControl, "X_ChangePointerControl" },
		{ X_GetPointerControl, "X_GetPointerControl" },
		{ X_SetScreenSaver, "X_SetScreenSaver" },
		{ X_GetScreenSaver, "X_GetScreenSaver" },
		{ X_ChangeHosts, "X_ChangeHosts" },
		{ X_ListHosts, "X_ListHosts" },
		{ X_SetAccessControl, "X_SetAccessControl" },
		{ X_SetCloseDownMode, "X_SetCloseDownMode" },
		{ X_KillClient, "X_KillClient" },
		{ X_RotateProperties, "X_RotateProperties" },
		{ X_ForceScreenSaver, "X_ForceScreenSaver" },
		{ X_SetPointerMapping, "X_SetPointerMapping" },
		{ X_GetPointerMapping, "X_GetPointerMapping" },
		{ X_SetModifierMapping, "X_SetModifierMapping" },
		{ X_GetModifierMapping, "X_GetModifierMapping" },
	};

	static struct xproto_map cmw_xprotoext_map[] = {
		{ X_SetTrusted, "XSetTrusted" },
		{ X_SetResLabel, "XResLabel" },
		{ X_SetResUID, "XSetResUID" },
		{ X_SetPropLabel, "XSetPropLabel" },
		{ X_SetPropUID, "XSetPropUID" },
		{ X_SetClearance, "XSetClearance" },
		{ X_GetResLabel, "XGetResLabel" },
		{ X_GetResUID, "XGetResUID" },
		{ X_GetPropLabel, "XGetPropLabel" },
		{ X_GetPropUID, "XGetPropUID" },
		{ X_CMWIntern, "XCMWIntern" } 	};

	static int cmw_xprotoext_map_size = sizeof (cmw_xprotoext_map) /
		sizeof (struct xproto_map);

	/*
	 * It's a standard X protocol request code
	 */
	if (xproto <= X_GetModifierMapping)
		return (std_xproto_map[xproto].xproto_str);

	if (xproto == X_NoOperation)
		return ("X_NoOperation");

	/*
	 * It a CMW X protocol request extension code
	 * Look it up in the CMW extension table.
	 */
	if (xproto > X_NoOperation) {
		xproto -= 128;
		for (i = 0; i < cmw_xprotoext_map_size; i++)
			if (xproto == cmw_xprotoext_map[i].xproto)
				return (cmw_xprotoext_map[xproto].xproto_str);
	}

	return ("unknown X protocol request code");
}

#endif /* SunOS_CMW */

/*
 * -----------------------------------------------------------------------
 * pa_print()	:  print as one str or formatted for easy reading.
 * 		: flag - indicates whether to output a new line for
 *		: multi-line output.
 * 		:		= 0; no new line
 *		:		= 1; new line if regular output
 *		: endrec - indicates whether to output
 *				a new line for one-line output.
 * 				= 0; no new line
 * 				= 1; new line
 * output	: The audit record information is displayed in the
 *		  type specified by uvaltype and value specified in
 *		  uval.  The printing of the delimiter or newline is
 *		  determined by the ONELINE, flag, and endrec values,
 *		  as follows:
 *			+--------+------+-------+----------------+
 *			|ONELINE | flag |endrec | Actioni	||
 *			+--------+------+-------+----------------+
 *			|    Y   |   Y  |   Y   | print new line |
 *			|    Y   |   Y  |   N   | print delimiter|
 *			|    Y   |   N  |   Y   | print new line |
 *			|    Y   |   N  |   N   | print delimiter|
 *			|    N   |   Y  |   Y   | print new line |
 *			|    N   |   Y  |   N   | print new line |
 *			|    N   |   N  |   Y   | print new line |
 *			|    N   |   N  |   N   | print delimiter|
 *			+--------+------+-------+----------------+
 *
 * return codes : -1 - error
 *		0 - successful
 * -----------------------------------------------------------------------
 */
static int
pa_print(flag, endrec)
int	flag;
int	endrec;
{
	int	returnstat = 0;

	switch (uvaltype) {
	case PRA_INT32:
		(void) printf("%d", uval.int32_val);
		break;
	case PRA_UINT32:
		(void) printf("%u", uval.uint32_val);
		break;
	case PRA_INT64:
		(void) printf("%"PRId64, uval.int64_val);
		break;
	case PRA_UINT64:
		(void) printf("%"PRIu64, uval.uint64_val);
		break;
	case PRA_SHORT:
		(void) printf("%hd", uval.short_val);
		break;
	case PRA_USHORT:
		(void) printf("%hd", uval.ushort_val);
		break;
	case PRA_CHAR:
		(void) printf("%c", uval.char_val);
		break;
	case PRA_BYTE:
		(void) printf("%d", uval.char_val);
		break;
	case PRA_STRING:
		(void) printf("%s", uval.string_val);
		break;
	case PRA_HEX32:
		(void) printf("0x%x", uval.int32_val);
		break;
	case PRA_HEX64:
		(void) printf("0x%"PRIx64, uval.int64_val);
		break;
	case PRA_SHEX:
		(void) printf("0x%hx", uval.short_val);
		break;
	case PRA_OCT:
		(void) printf("%ho", uval.ushort_val);
		break;
	case PRA_LOCT:
		(void) printf("%o", (int) uval.uint32_val);
		break;
	default:
		(void) fprintf(stderr, gettext("praudit: Unknown type.\n"));
		returnstat = -1;
		break;
	}

	if (ONELINE != 1) {
		if ((flag == 1) || (endrec == 1))
			(void) printf("\n");
		else
			(void) printf("%s", SEPARATOR);
	} else {
		if (endrec != 1) {
			char id = adrf_peek(audit_adr);
			if (id != AUT_HEADER32 && id != AUT_HEADER64) {
				(void) printf("%s", SEPARATOR);
			}
		}
	}
	(void) fflush(stdout);

	return (returnstat);
}

/*
 * Convert binary data to ASCII for printing.
 */
void
convertascii(p, c, size)
register char	*p;
register char	*c;
register int	size;
{
	register int	i;

	for (i = 0; i < size; i++) {
		*(c + i) = (char)toascii(*(c + i));
		if ((int)iscntrl(*(c + i))) {
			*p++ = '^';
			*p++ = (char)(*(c + i) + 0x40);
		} else
			*p++ = *(c + i);
	}

	*p = '\0';

}
