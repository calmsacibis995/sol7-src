/*
 * Copyright (c) 1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)cfgadm.c	1.4	98/01/23 SMI"

/*
 * This is the main program file for the configuration administration
 * command as set out in manual page cfgadm(1M).  It uses the configuration
 * administration library interface, libcfgadm, as set out in manual
 * page config_admin(3X).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <langinfo.h>
#include <time.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/sunddi.h>
#include <sys/openpromio.h>
#include <sys/ddi_impldefs.h>
#include <sys/systeminfo.h>

#include <config_admin.h>
#include "cfgadm.h"

/*
 * forward declarations
 */
static char *basename(char *);
static void cfgadm_error(int, char *);
static int confirm_interactive(void *, const char *);
static int confirm_no(void *, const char *);
static int confirm_yes(void *, const char *);
static void usage(void);
static void usage_field(void);
static int extract_list_suboptions(char *, char **, char **, char **,
    int *, char **);
static int message_output(void *appdata_ptr, const char *message);
static void *config_calloc_check(size_t, size_t, cfga_err_t *);
static int do_config_list(int, char *[], cfga_stat_data_t *, int, char *,
    char *, char *, int, char *);
static cfga_ap_types_t find_arg_type(char *);
static int yesno(char *, char *);


static int compare_ap_id(struct cfga_stat_data *, struct cfga_stat_data *);
static int compare_r_state(struct cfga_stat_data *, struct cfga_stat_data *);
static int compare_o_state(struct cfga_stat_data *, struct cfga_stat_data *);
static int compare_cond(struct cfga_stat_data *, struct cfga_stat_data *);
static int compare_time(struct cfga_stat_data *, struct cfga_stat_data *);
static int compare_info(struct cfga_stat_data *, struct cfga_stat_data *);
static int compare_type(struct cfga_stat_data *, struct cfga_stat_data *);
static int compare_busy(struct cfga_stat_data *, struct cfga_stat_data *);
static int compare_null(struct cfga_stat_data *, struct cfga_stat_data *);
static void print_log_id(struct cfga_stat_data *, int, char *);
static void print_r_state(struct cfga_stat_data *, int, char *);
static void print_o_state(struct cfga_stat_data *, int, char *);
static void print_cond(struct cfga_stat_data *, int, char *);
static void print_time(struct cfga_stat_data *, int, char *);
static void print_time_p(struct cfga_stat_data *, int, char *);
static void print_info(struct cfga_stat_data *, int, char *);
static void print_type(struct cfga_stat_data *, int, char *);
static void print_busy(struct cfga_stat_data *, int, char *);
static void print_phys_id(struct cfga_stat_data *, int, char *);
static void print_null(struct cfga_stat_data *, int, char *);
static int count_fields(char *, char);
static int process_sort_fields(int, struct sort_el *, char *);
static int process_fields(int, struct print_col *, int, char *);
static cfga_err_t print_fields(int, struct print_col *, int, int, char *,
    struct cfga_stat_data *, FILE *);
static int apid_in_list(cfga_stat_data_t *, char **, int);
static int aptype_in_list(const char *, char **, int);
static int apid_compare(const void *, const void *);

/*
 * global data
 */
/* command name for messages */
static char *cmdname;

/* control for comparing, printing cfga_stat_data */
static struct field_info all_fields[] = {
	{"ap_id", "Ap_Id", SZ_EL(ap_log_id), compare_ap_id, print_log_id},
	{"r_state", "Receptacle", STATE_WIDTH, compare_r_state, print_r_state},
	{"o_state", "Occupant", STATE_WIDTH, compare_o_state, print_o_state},
	{"condition", "Condition", COND_WIDTH, compare_cond, print_cond},
	{"status_time", "When", TIME_WIDTH, compare_time, print_time},
	{"status_time_p", "When", TIME_P_WIDTH, compare_time, print_time_p},
	{"info", "Information", 40, compare_info, print_info},
	{"type", "Type", SZ_EL(ap_type), compare_type, print_type},
	{"busy", "Busy", 8, compare_busy, print_busy},
	{"physid", "Phys_Id", SZ_EL(ap_phys_id), compare_ap_id, print_phys_id},
};

static struct field_info null_field =
	{"null", "", 0, compare_null, print_null};

static struct sort_el *sort_list;	/* Used in apid_compare() */
static int nsort_list;
static char unk_field[] = "%s: field \"%s\" unknown\n";

/* strings that make up the usage message */
static char *usage_tab[] = {
"\t%s [-f] [-y|-n] [-v] [-o hardware_opts ] -c function ap_id [ap_id...]\n",
"\t%s [-f] [-y|-n] [-v] [-o hardware_opts ] -x function ap_id [ap_id...]\n",
"\t%s [-v] [-s listing_options ] [-o hardware_opts ] [-l [ap_id|ap_type...]]\n",
"\t%s [-v] [-o hardware_opts ] -t ap_id [ap_id...]\n",
"\t%s [-v] [-o hardware_opts ] -h [ap_id|ap_type...]\n",
};

#define	APID_IS_PHYSICAL(a)	(*(a) == '/')

/*
 * main - the main routine of cfgadm, processes the command line
 * and dispatches functions off to libraries.
 */
void
main(
	int argc,
	char *argv[])
{
	extern char *optarg;
	extern int optind;
	int c;
	char targ_num;
	char *targ_nump;
	char *targ_arg;
	char *const *ap_args;
	cfga_cmd_t sc_opt;
	struct cfga_confirm confirm;
	struct cfga_msg message;
	int ret;
	int i;
	char *estrp;
	cfga_op_t action = CFGA_OP_NONE;
	char *act_arg = NULL;
	char *plat_opts = NULL;
	enum confirm confarg = CONFIRM_DEFAULT;
	char *list_opts = NULL;
	cfga_err_t flags = 0;
	int arg_error = 0;

	estrp = NULL;
	if (argc > 0)
		cmdname = basename(argv[0]);
	else
		cmdname = "cfgadm";
	(void) setlocale(LC_ALL, "");
#if !defined(TEXT_DOMAIN)
#define	TEXT_DOMAIN	"SYS_TEST"
#endif
	(void) textdomain(TEXT_DOMAIN);

	while ((c = getopt(argc, argv, OPTIONS)) != EOF) {
		static char dup_action[] =
"%s: more than one action specified (-c,-l,-t,-x)\n";
		static char dup_option[] =
"%s: more than one -%c option specified\n";
		switch (c) {
		case 'c':
			if (action != CFGA_OP_NONE) {
				arg_error = 1;
				(void) fprintf(stderr, gettext(dup_action),
				    cmdname);
			}
			action = CFGA_OP_CHANGE_STATE;
			targ_nump = &targ_num;
			targ_arg = optarg;
			if ((sc_opt = getsubopt(&targ_arg, state_opts,
						&targ_nump)) == -1) {
				arg_error = 1;
				break;
			}
			break;
		case 'f':
			if ((flags & CFGA_FLAG_FORCE) != 0) {
				arg_error = 1;
				(void) fprintf(stderr, gettext(dup_option),
				    cmdname, c);
			}
			flags |= CFGA_FLAG_FORCE;
			break;
		case 'h':
			if (action != CFGA_OP_NONE) {
				arg_error = 1;
				(void) fprintf(stderr, gettext(dup_action),
				    cmdname);
			}
			action = CFGA_OP_HELP;
			act_arg = optarg;
			break;
		case 'l':
			if (action != CFGA_OP_NONE) {
				arg_error = 1;
				(void) fprintf(stderr, gettext(dup_action),
				    cmdname);
			}
			action = CFGA_OP_LIST;
			act_arg = optarg;
			break;
		case 'n':
			if (confarg != CONFIRM_DEFAULT) {
				arg_error = 1;
				(void) fprintf(stderr, gettext(dup_option),
				    cmdname, c);
			}
			confarg = CONFIRM_NO;
			break;
		case 'o':
			if (plat_opts != NULL) {
				arg_error = 1;
				(void) fprintf(stderr, gettext(dup_option),
				    cmdname, c);
			}
			plat_opts = optarg;
			break;
		case 's':
			if (list_opts != NULL) {
				arg_error = 1;
				(void) fprintf(stderr, gettext(dup_option),
				    cmdname, c);
			}
			list_opts = optarg;
			break;
		case 't':
			if (action != CFGA_OP_NONE) {
				arg_error = 1;
				(void) fprintf(stderr, gettext(dup_action),
				    cmdname);
			}
			action = CFGA_OP_TEST;
			act_arg = optarg;
			break;
		case 'x':
			if (action != CFGA_OP_NONE) {
				arg_error = 1;
				(void) fprintf(stderr, gettext(dup_action),
				    cmdname);
			}
			action = CFGA_OP_PRIVATE;
			act_arg = optarg;
			break;
		case 'v':
			if ((flags & CFGA_FLAG_VERBOSE) != 0) {
				arg_error = 1;
				(void) fprintf(stderr, gettext(dup_option),
				    cmdname, c);
			}
			flags |= CFGA_FLAG_VERBOSE;
			break;
		case 'y':
			if (confarg != CONFIRM_DEFAULT) {
				arg_error = 1;
				(void) fprintf(stderr, gettext(dup_option),
				    cmdname, c);
			}
			confarg = CONFIRM_YES;
			break;
		case '?':	/* getopts issues message is this case */
		default:	/* catch programming errors */
			arg_error = 1;
			break;
		}
	}
	/* default action is list */
	if (action == CFGA_OP_NONE)
		action = CFGA_OP_LIST;

	if (arg_error) {
		usage();
		exit(EXIT_ARGERROR);
		/*NOTREACHED*/
	}
	ap_args = &argv[optind];

	/*
	 * If neither -n of -y was specified, interactive confirmation
	 * is used.  Check if the program has terminal I/O and
	 * enforce -n if not.
	 */
	(void) memset((void *)&confirm, 0, sizeof (confirm));
	if (action == CFGA_OP_CHANGE_STATE) {
		if (confarg == CONFIRM_DEFAULT &&
		    !(isatty(fileno(stdin)) && isatty(fileno(stderr))))
			confarg = CONFIRM_NO;
		switch (confarg) {
		case CONFIRM_DEFAULT:
			confirm.confirm = confirm_interactive;
			break;
		case CONFIRM_NO:
			confirm.confirm = confirm_no;
			break;
		case CONFIRM_YES:
			confirm.confirm = confirm_yes;
			break;
		default:	/* paranoia */
			abort();
			/*NOTREACHED*/
		}
	}

	/*
	 * set up message output routine
	 */
	message.message_routine = message_output;

	switch (action) {
	case CFGA_OP_CHANGE_STATE:
	    /* Sanity check - requires an argument */
	    if ((argc - optind) <= 0) {
		usage();
		break;
	    }
	    /* Sanity check - args cannot be ap_types */
	    for (i = 0; i < (argc - optind); i++) {
		    if (find_arg_type(ap_args[i]) == AP_TYPE) {
			    usage();
			    break;
		    }
	    }
	    ret = config_change_state(sc_opt, argc - optind, ap_args, plat_opts,
		&confirm, &message, &estrp, flags);
	    if (ret != CFGA_OK)
		    cfgadm_error(ret, estrp);
	    break;
	case CFGA_OP_PRIVATE:
	    /* Sanity check - requires an argument */
	    if ((argc - optind) <= 0) {
		usage();
		break;
	    }
	    /* Sanity check - args cannot be ap_types */
	    for (i = 0; i < (argc - optind); i++) {
		    if (find_arg_type(ap_args[i]) == AP_TYPE) {
			    usage();
			    break;
		    }
	    }
	    ret = config_private_func(act_arg, argc - optind, ap_args,
		plat_opts, &confirm, &message, &estrp, flags);
	    if (ret != CFGA_OK)
		    cfgadm_error(ret, estrp);
	    break;
	case CFGA_OP_TEST:
	    /* Sanity check - requires an argument */
	    if ((argc - optind) <= 0) {
		usage();
		break;
	    }
	    /* Sanity check - args cannot be ap_types */
	    for (i = 0; i < (argc - optind); i++) {
		    if (find_arg_type(ap_args[i]) == AP_TYPE) {
			    usage();
			    break;
		    }
	    }
	    ret = config_test(argc - optind, ap_args, plat_opts, &message,
		&estrp, flags);
	    if (ret != CFGA_OK)
		    cfgadm_error(ret, estrp);
	    break;
	case CFGA_OP_HELP:
	    /* always do usage? */
	    usage();
	    ret = config_help(argc - optind, ap_args, &message, plat_opts,
		flags);
	    if (ret != CFGA_OK)
		    cfgadm_error(ret, estrp);
	    break;
	case CFGA_OP_LIST: {
		struct cfga_stat_data *statlist;
		int nlist = 0;
		char *sort_fields = DEF_SORT_FIELDS;
		char *cols = DEF_COLS;
		char *cols2 = DEF_COLS2;
		int noheadings = 0;
		char *delim = DEF_DELIM;
		int exitcode;
		int i;
		int type = 0;

		if (flags & CFGA_FLAG_VERBOSE) {
			cols = DEF_COLS_VERBOSE;
			cols2 = DEF_COLS2_VERBOSE;
		}

		if (list_opts != NULL && !extract_list_suboptions(list_opts,
		    &sort_fields, &cols, &cols2, &noheadings, &delim)) {
			usage_field();
			exit(EXIT_ARGERROR);
			/*NOTREACHED*/
		}

		/*
		 * Scan any args and see if there are any ap_types.
		 * If there are we use config_list to get all
		 * attachment point stats and then filter what gets
		 * printed.
		 */
		if ((argc - optind) != 0) {
			for (i = 0; i < (argc - optind); i++) {
				if (find_arg_type(ap_args[i]) == AP_TYPE)
					type = 1;
			}
		}
		/*
		 * Check for args. No args means find all libs
		 * and call their cfga_list routines.
		 * With args, if the args are ap_types we
		 * call their lib's cfga_list routine.
		 * If the args are ap_id's we call their
		 * lib's cfga_stat routine.
		 */
		if (((argc - optind) == 0) || (type == 1)) {
			/*
			 * No args, or ap_type args
			 */
			exitcode = EXIT_OK;
			statlist = (cfga_stat_data_t *)NULL;
			if ((ret = config_list(&statlist, &nlist,
			    plat_opts, &estrp)) == CFGA_OK) {
				if (do_config_list(
				    (argc - optind), &argv[optind],
				    statlist, nlist, sort_fields, cols,
				    cols2, noheadings, delim) !=
				    CFGA_OK)
					exitcode = EXIT_ARGERROR;
			} else {
				cfgadm_error(ret, estrp);
				exitcode = EXIT_ARGERROR;
				break;
			}
			if (exitcode != EXIT_OK) {
				exit(exitcode);
				/*NOTREACHED*/
			}
			if (statlist != (cfga_stat_data_t *)NULL)
				free((void *)statlist);
		} else {
			cfga_err_t calloc_error = CFGA_OK;
			cfga_stat_data_t *buf;
			/*
			 * when presented with an ap_id we call the
			 * config_stat routine
			 */
			buf = (cfga_stat_data_t *)config_calloc_check(
			    (argc - optind), sizeof (cfga_stat_data_t),
			    &calloc_error);

			if (calloc_error != CFGA_OK)
				exit(EXIT_OPFAILED);

			if ((ret = config_stat(argc - optind, ap_args, buf,
			    plat_opts, &estrp)) == CFGA_OK) {
				ret = do_config_list((argc - optind),
				    &argv[optind], buf, argc - optind,
				    sort_fields, cols, cols2, noheadings,
				    delim);
			}
			free((void *)buf);
			if (ret != CFGA_OK) {
				cfgadm_error(ret, estrp);
				break;
			}
		}
		break;
	}
	default:	/* paranoia */
		abort();
		/*NOTREACHED*/
	}
	if (ret == CFGA_NOTSUPP)
		exit(EXIT_NOTSUPP);
	else if (ret != CFGA_OK)
		exit(EXIT_OPFAILED);
	else
		exit(EXIT_OK);
	/*NOTREACHED*/
}

/*
 * usage - outputs the usage help message.
 */
static void
usage(
	void)
{
	int i;

	(void) fprintf(stderr, "%s\n", gettext("Usage:"));
	for (i = 0; i < sizeof (usage_tab)/sizeof (usage_tab[0]); i++) {
		(void) fprintf(stderr, gettext(usage_tab[i]), cmdname);
	}
}

/*
 * Emit an error message.
 * As a side-effect the hardware specific error message is deallocated
 * as described in config_admin(3X).
 */
static void
cfgadm_error(int errnum, char *estrp)
{
	const char *ep;

	ep = config_strerror(errnum);
	if (ep == NULL)
		ep = gettext("configuration administration unknown error");
	if (estrp != NULL && *estrp != '\0') {
		(void) fprintf(stderr, "%s: %s: %s\n", cmdname, ep, estrp);
	} else {
		(void) fprintf(stderr, "%s: %s\n", cmdname, ep);
	}
	if (estrp != NULL)
		free((void *)estrp);
}

/*
 * confirm_interactive - prompt user for confirmation
 */
static int
confirm_interactive(
	void *appdata_ptr,
	const char *message)
{
	static char yeschr[YESNO_STR_MAX + 2];
	static char nochr[YESNO_STR_MAX + 2];
	static int inited = 0;
	int isyes;

#ifdef lint
	appdata_ptr = appdata_ptr;
#endif /* lint */
	/*
	 * First time through initialisation.  In the original
	 * version of this command this function is only called once,
	 * but this function is generalized for the future.
	 */
	if (!inited) {
		(void) strncpy(yeschr, nl_langinfo(YESSTR), YESNO_STR_MAX + 1);
		(void) strncpy(nochr, nl_langinfo(NOSTR), YESNO_STR_MAX + 1);
		inited = 1;
	}

	do {
		(void) fprintf(stderr, "%s (%s/%s)? ", message, yeschr, nochr);
		isyes = yesno(yeschr, nochr);
	} while (isyes == -1);
	return (isyes);
}

/*
 * If any text is input it must sub-string match either yes or no.
 * Failure of this match is indicated by return of -1.
 * If an empty line is input, this is taken as no.
 */
static int
yesno(
	char *yesp,
	char *nop)
{
	int	i, b;
	char	ans[YESNO_STR_MAX + 1];

	i = 0;
	while (1) {
		b = getc(stdin);	/* more explicit that rm.c version */
		if (b == '\n' || b == '\0' || b == EOF) {
			if (i < YESNO_STR_MAX)	/* bug fix to rm.c version */
				ans[i] = 0;
			break;
		}
		if (i < YESNO_STR_MAX)
			ans[i] = b;
		i++;
	}
	if (i >= YESNO_STR_MAX) {
		i = YESNO_STR_MAX;
		ans[YESNO_STR_MAX] = 0;
	}
	/* changes to rm.c version follow */
	if (i == 0)
		return (0);
	if (strncmp(nop, ans, i) == 0)
		return (0);
	if (strncmp(yesp, ans, i) == 0)
		return (1);
	return (-1);
}

/*ARGSUSED*/
static int
confirm_no(
	void *appdata_ptr,
	const char *message)
{
	return (0);
}

/*ARGSUSED*/
static int
confirm_yes(
	void *appdata_ptr,
	const char *message)
{
	return (1);
}

/*
 * Find base name of filename.
 */
static char *
basename(
	char *cp)
{
	char *sp;

	if ((sp = strrchr(cp, '/')) != NULL)
		return (sp + 1);
	return (cp);
}

/*ARGSUSED*/
static int
message_output(
	void *appdata_ptr,
	const char *message)
{
	(void) fprintf(stderr, "%s", message);
	return (CFGA_OK);

}

/*
 * extract_list_suboptions - process list option string
 */
static int
extract_list_suboptions(
	char *arg,
	char **sortpp,
	char **colspp,
	char **cols2pp,
	int *noheadingsp,
	char **delimpp)
{
	char *value;
	int subopt;
	int err;

	err = 0;

	while (*arg != '\0') {
		static char need_value[] =
"%s: sub-option \"%s\" requires a value\n";
		static char no_value[] =
"%s: sub-option \"%s\" does not take a value\n";
		static char unk_subopt[] =
"%s: sub-option \"%s\" unknown\n";
		char **pptr;

		subopt = getsubopt(&arg, list_options, &value);
		switch (subopt) {
		case LIST_SORT:
			pptr = sortpp;
			goto valcom;
		case LIST_COLS:
			pptr = colspp;
			goto valcom;
		case LIST_COLS2:
			pptr = cols2pp;
			goto valcom;
		case LIST_DELIM:
			pptr = delimpp;
		valcom:
			if (value == NULL) {
				(void) fprintf(stderr, gettext(need_value),
				    cmdname, list_options[subopt]);
				err = 1;
			} else
				*pptr = value;
			break;
		case LIST_NOHEADINGS:
			if (value != NULL) {
				(void) fprintf(stderr, gettext(no_value),
				    cmdname, list_options[subopt]);
				err = 1;
			} else
				*noheadingsp = 1;
			break;
		default:
			(void) fprintf(stderr, gettext(unk_subopt),
			    cmdname, value);
			err = 1;
			break;
		}
	}
	return (err == 0);
}

/*
 * compare_ap_id - compare two ap_id's
 */
static int
compare_ap_id(
	struct cfga_stat_data *p1,
	struct cfga_stat_data *p2)
{
	return (config_ap_id_cmp(p1->ap_log_id, p2->ap_log_id));
}

/*
 * print_log_id - print logical ap_id
 */
static void
print_log_id(
	struct cfga_stat_data *p,
	int width,
	char *lp)
{
	(void) sprintf(lp, "%-*.*s", width, sizeof (p->ap_log_id),
	    p->ap_log_id);
}

/*
 * compare_r_state - compare receptacle state of two ap_id's
 */
static int
compare_r_state(
	struct cfga_stat_data *p1,
	struct cfga_stat_data *p2)
{
	if (p1->ap_r_state > p2->ap_r_state)
		return (1);
	else
	if (p1->ap_r_state < p2->ap_r_state)
		return (-1);
	else
	return (0);
}

/*
 * compare_o_state - compare occupant state of two ap_id's
 */
static int
compare_o_state(
	struct cfga_stat_data *p1,
	struct cfga_stat_data *p2)
{
	if (p1->ap_o_state > p2->ap_o_state)
		return (1);
	else
	if (p1->ap_o_state < p2->ap_o_state)
		return (-1);
	else
	return (0);
}

/*
 * compare_busy - compare busy field of two ap_id's
 */
static int
compare_busy(
	struct cfga_stat_data *p1,
	struct cfga_stat_data *p2)
{
	if (p1->ap_busy > p2->ap_busy)
		return (1);
	else
	if (p1->ap_busy < p2->ap_busy)
		return (-1);
	else
	return (0);
}

/*
 * print_r_state - print receptacle state
 */
static void
print_r_state(
	struct cfga_stat_data *p,
	int width,
	char *lp)
{
	char *cp;

	switch (p->ap_r_state) {
	case CFGA_STAT_EMPTY:
		cp = "empty";
		break;
	case CFGA_STAT_CONNECTED:
		cp = "connected";
		break;
	case CFGA_STAT_DISCONNECTED:
		cp = "disconnected";
		break;
	default:
		cp = "???";
		break;
	}
	(void) sprintf(lp, "%-*s", width, cp);
}

/*
 * print_o_state - print occupant state
 */
static void
print_o_state(
	struct cfga_stat_data *p,
	int width,
	char *lp)
{
	char *cp;

	switch (p->ap_o_state) {
	case CFGA_STAT_UNCONFIGURED:
		cp = "unconfigured";
		break;
	case CFGA_STAT_CONFIGURED:
		cp = "configured";
		break;
	default:
		cp = "???";
		break;
	}
	(void) sprintf(lp, "%-*s", width, cp);
}

/*
 * compare_cond - compare condition field of two ap_id's
 */
static int
compare_cond(
	struct cfga_stat_data *p1,
	struct cfga_stat_data *p2)
{
	if (p1->ap_cond > p2->ap_cond)
		return (1);
	else
	if (p1->ap_cond < p2->ap_cond)
		return (-1);
	else
	return (0);
}

/*
 * print_cond - print attachment point condition
 */
static void
print_cond(
	struct cfga_stat_data *p,
	int width,
	char *lp)
{
	char *cp;

	switch (p->ap_cond) {
	case CFGA_COND_UNKNOWN:
		cp = "unknown";
		break;
	case CFGA_COND_UNUSABLE:
		cp = "unusable";
		break;
	case CFGA_COND_FAILING:
		cp = "failing";
		break;
	case CFGA_COND_FAILED:
		cp = "failed";
		break;
	case CFGA_COND_OK:
		cp = "ok";
		break;
	default:
		cp = "???";
		break;
	}
	(void) sprintf(lp, "%-*s", width, cp);
}

/*
 * compare_time - compare time field of two ap_id's
 */
static int
compare_time(
	struct cfga_stat_data *p1,
	struct cfga_stat_data *p2)
{
	if (p1->ap_status_time > p2->ap_status_time)
		return (1);
	else
	if (p1->ap_status_time < p2->ap_status_time)
		return (-1);
	else
	return (0);
}


/*
 * print_time - print time from cfga_stat_data.
 * Time print based on ls(1).
 */
static void
print_time(
	struct cfga_stat_data *p,
	int width,
	char *lp)
{
	static time_t   year, now;
	time_t stime;
	char	time_buf[50];	/* array to hold day and time */

	if (year == 0) {
		now = time((long *)NULL);
		year = now - 6L*30L*24L*60L*60L; /* 6 months ago */
		now = now + 60;
	}
	stime = p->ap_status_time;
	if ((stime < year) || (stime > now)) {
		(void) cftime(time_buf,
		    dcgettext(NULL, FORMAT1, LC_TIME), &stime);
	} else {
		(void) cftime(time_buf,
		    dcgettext(NULL, FORMAT2, LC_TIME), &stime);
	}
	(void) sprintf(lp, "%-*s", width, time_buf);
}

/*
 * print_time_p - print time from cfga_stat_data.
 */
static void
print_time_p(
	struct cfga_stat_data *p,
	int width,
	char *lp)
{
	struct tm *tp;
	char tstr[TIME_P_WIDTH+1];

	tp = localtime(&p->ap_status_time);
	(void) sprintf(tstr, "%04d%02d%02d%02d%02d%02d", tp->tm_year + 1900,
	    tp->tm_mon + 1, tp->tm_mday, tp->tm_hour, tp->tm_min, tp->tm_sec);
	(void) sprintf(lp, "%-*s", width, tstr);
}

/*
 * compare_info - compare info from two cfga_stat_data structs
 */
static int
compare_info(
	struct cfga_stat_data *p1,
	struct cfga_stat_data *p2)
{
	return (strncmp(p1->ap_info, p2->ap_info, sizeof (p2->ap_info)));
}

/*
 * print_info - print info from cfga_stat_data struct
 */
static void
print_info(
	struct cfga_stat_data *p,
	int width,
	char *lp)
{
	(void) sprintf(lp, "%-*.*s", width, sizeof (p->ap_info), p->ap_info);
}

/*
 * compare_type - compare type from two cfga_stat_data structs
 */
static int
compare_type(
	struct cfga_stat_data *p1,
	struct cfga_stat_data *p2)
{
	return (strncmp(p1->ap_type, p2->ap_type, sizeof (p2->ap_type)));
}

/*
 * print_type - print type from cfga_stat_data struct
 */
static void
print_type(
	struct cfga_stat_data *p,
	int width,
	char *lp)
{
	(void) sprintf(lp, "%-*.*s", width, sizeof (p->ap_type), p->ap_type);
}

/*
 * print_busy - print busy from cfga_stat_data struct
 */
/* ARGSUSED */
static void
print_busy(
	struct cfga_stat_data *p,
	int width,
	char *lp)
{
	if (p->ap_busy)
		(void) sprintf(lp, "%-*.*s", width, width, "y");
	else
		(void) sprintf(lp, "%-*.*s", width, width, "n");
}

/*
 * print_phys_id - print physical ap_id
 */
static void
print_phys_id(
	struct cfga_stat_data *p,
	int width,
	char *lp)
{
	(void) sprintf(lp, "%-*.*s", width, sizeof (p->ap_phys_id),
	    p->ap_phys_id);
}


/*
 * find_field - find the named field
 */
static struct field_info *
find_field(char *fname)
{
	struct field_info *fldp;

	for (fldp = all_fields; fldp < &all_fields[N_FIELDS]; fldp++)
		if (strcmp(fname, fldp->name) == 0)
			return (fldp);
	return (NULL);
}

/*
 * usage_field - print field usage
 */
static void
usage_field()
{
	struct field_info *fldp;
	const char *sep;
	static char field_list[] = "%s: print or sort fields must be one of:";

	(void) fprintf(stderr, gettext(field_list), cmdname);
	sep = "";

	for (fldp = all_fields; fldp < &all_fields[N_FIELDS]; fldp++) {
		(void) fprintf(stderr, "%s %s", sep, fldp->name);
		sep = ",";
	}
	(void) fprintf(stderr, "\n");
}

/*
 * compare_null - null comparison routine
 */
/*ARGSUSED*/
static int
compare_null(
	struct cfga_stat_data *p1,
	struct cfga_stat_data *p2)
{
	return (0);
}

/*
 * print_null - print out a field of spaces
 */
/*ARGSUSED*/
static void
print_null(
	struct cfga_stat_data *p,
	int width,
	char *lp)
{
	(void) sprintf(lp, "%-*s", width, "");
}

/*
 * do_config_list - directs the output of the listing functions
 */
static int
do_config_list(
	int l_argc,
	char *l_argv[],
	cfga_stat_data_t *statlist,
	int nlist,
	char *sortp,
	char *colsp,
	char *cols2p,
	int noheadings,
	char *delimp)
{
	int nprcols, ncols2;
	struct print_col *prnt_list;
	char **apids_to_list;
	int napids_to_list;
	FILE *fp;
	int f_err;
	struct cfga_stat_data **sel_boards;
	int nsel;
	int i, j;
	cfga_err_t calloc_error = CFGA_OK;

	f_err = 0;
	fp = stdout;
	nsort_list = count_fields(sortp, FDELIM);
	if (nsort_list != 0) {
		sort_list = (struct sort_el *)config_calloc_check(nsort_list,
		    sizeof (*sort_list), &calloc_error);
		if (calloc_error != CFGA_OK)
			return (calloc_error);
		f_err |= process_sort_fields(nsort_list, sort_list, sortp);
	} else
		sort_list = NULL;

	nprcols = count_fields(colsp, FDELIM);
	if ((ncols2 = count_fields(cols2p, FDELIM)) > nprcols)
		nprcols = ncols2;
	if (nprcols != 0) {
		prnt_list = (struct print_col *)config_calloc_check(nprcols,
		    sizeof (*prnt_list), &calloc_error);
		if (calloc_error != CFGA_OK)
			return (calloc_error);
		f_err |= process_fields(nprcols, prnt_list, 0, colsp);
		if (ncols2 != 0)
			f_err |= process_fields(nprcols, prnt_list, 1, cols2p);
	} else
		prnt_list = NULL;

	if (f_err) {
		usage_field();
		return (CFGA_ERROR);
	}

	if (l_argc != 0) {
		int i, j;

		napids_to_list = 0;

		for (i = 0; i < l_argc; i++)
			napids_to_list += count_fields(l_argv[i], ARG_DELIM);

		apids_to_list = (char **)config_calloc_check(napids_to_list
		    + 1, sizeof (*apids_to_list), &calloc_error);

		if (calloc_error != CFGA_OK)
			return (calloc_error);

		for (i = 0, j = 0; i < l_argc; i++) {
			int n;

			n = count_fields(l_argv[i], ARG_DELIM);
			if (n == 0) {
				continue;
			} else if (n == 1) {
				apids_to_list[j] = l_argv[i];
				j++;
			} else {
				char *cp, *ncp;

				cp = l_argv[i];
				for (;;) {
					apids_to_list[j] = cp;
					j++;
					ncp = strchr(cp, ARG_DELIM);
					if (ncp == NULL)
						break;
					*ncp = '\0';
					cp = ncp + 1;
				}
			}
		}
		assert(j == napids_to_list);
	} else {
		napids_to_list = 0;
		apids_to_list = NULL;
	}

	assert(nlist != 0);
	sel_boards = (struct cfga_stat_data **)config_calloc_check(nlist,
	    sizeof (*sel_boards), &calloc_error);
	if (calloc_error != CFGA_OK) {
		return (calloc_error);
	}
	for (i = 0, j = 0; i < nlist; i++) {
		if (napids_to_list == 0 ||
		    apid_in_list(&statlist[i], apids_to_list, napids_to_list)) {
			sel_boards[j] = &statlist[i];
			j++;
		} else {
			/* handle ap_types in list */
			if (aptype_in_list((const char *)&statlist[i].ap_log_id,
			    apids_to_list, napids_to_list)) {
				sel_boards[j] = &statlist[i];
				j++;
			}
		}
	}
	nsel = j;

	/* print headings */
	if (!noheadings && prnt_list != NULL) {
		if ((calloc_error = print_fields(nprcols, prnt_list, 1, 0,
		    delimp, NULL, fp)) != CFGA_OK)
			return (calloc_error);
		if (ncols2 != 0) {
			if ((calloc_error = print_fields(nprcols, prnt_list, 1,
			    1, delimp, NULL, fp)) != CFGA_OK)
				return (calloc_error);
		}
	}

	if (nsel != 0) {
		if (sort_list != NULL && nsel > 1) {
			qsort((void *)sel_boards, nsel, sizeof (sel_boards[0]),
			    apid_compare);
		}
		if (prnt_list != NULL) {
			for (i = 0; i < nsel; i++) {
				if ((calloc_error = print_fields(nprcols,
				    prnt_list, 0, 0, delimp, sel_boards[i], fp))
				    != CFGA_OK)
					return (calloc_error);
				if (ncols2 != 0) {
					if ((calloc_error = print_fields(
					    nprcols, prnt_list, 0, 1, delimp,
					    sel_boards[i], fp)) != CFGA_OK)
						return (calloc_error);
				}
			}
		}
	}
	free((void *)sel_boards);

	if (sort_list != NULL)
		free((void *)sort_list);
	if (prnt_list != NULL)
		free((void *)prnt_list);
	if (apids_to_list != NULL)
		free((void *)apids_to_list);
	return (CFGA_OK);
}

/*
 * apid_in_list - check for ap_id name in list.
 */
static int
apid_in_list(
	cfga_stat_data_t *sd,
	char **list,
	int nlist)
{
	int i;
	char *cp;

	for (i = 0; i < nlist; i++) {
		cp = list[i];
		if (APID_IS_PHYSICAL(cp)) {
			if (config_ap_id_cmp(sd->ap_phys_id, list[i]) == 0)
				return (1);
		} else {
			if (config_ap_id_cmp(sd->ap_log_id, list[i]) == 0)
				return (1);
		}
	}
	return (0);
}

/*
 * aptype_in_list - check for ap_id name in list.
 */
static int
aptype_in_list(
	const char *bk,
	char **list,
	int nlist)
{
	int i;

	for (i = 0; i < nlist; i++) {
		if (find_arg_type(list[i]) != AP_TYPE)
			continue;
		if (strncmp(bk, list[i], strlen(list[i])) == 0)
			return (1);
	}
	return (0);
}
/*
 * apid_compare - compare two attachment point stat data structures.
 */
static int
apid_compare(
	const void *vb1,
	const void *vb2)
{
	int i;
	int res;

#define	b1	(*(struct cfga_stat_data **)vb1)
#define	b2	(*(struct cfga_stat_data **)vb2)

	for (i = 0; i < nsort_list; i++) {
		res = (*(sort_list[i].fld->compare))(b1, b2);
		if (res != 0) {
			if (sort_list[i].reverse)
				res = -res;
			break;
		}
	}

#undef	b1
#undef	b2
	return (res);
}

/*
 * count_fields - Count the number of fields, using supplied delimiter.
 */
static int
count_fields(char *fspec, char delim)
{
	char *cp;
	int n;

	if (fspec == 0 || *fspec == '\0')
		return (0);
	n = 1;
	for (cp = fspec; *cp != '\0'; cp++)
		if (*cp == delim)
			n++;
	return (n);
}

/*
 * get_field
 * This function is not a re-implementation of strtok().
 * There can be null fields - strtok() eats spans of delimiters.
 */
static char *
get_field(char **fspp)
{
	char *cp, *fld;

	fld = *fspp;

	if (fld != NULL && *fld == '\0')
		fld = NULL;

	if (fld != NULL) {
		cp = strchr(*fspp, FDELIM);
		if (cp == NULL) {
			*fspp = NULL;
		} else {
			*cp = '\0';
			*fspp = cp + 1;
			if (*fld == '\0')
				fld = NULL;
		}
	}
	return (fld);
}

/*
 * process_fields -
 */
static int
process_fields(
	int ncol,
	struct print_col *list,
	int line2,
	char *fmt)
{
	struct print_col *pp;
	struct field_info *fldp;
	char *fmtx;
	char *fldn;
	int err;

	err = 0;
	fmtx = fmt;
	for (pp = list; pp < &list[ncol]; pp++) {
		fldn = get_field(&fmtx);
		fldp = &null_field;
		if (fldn != NULL) {
			struct field_info *tfldp;

			tfldp = find_field(fldn);
			if (tfldp != NULL) {
				fldp = tfldp;
			} else {
				(void) fprintf(stderr, gettext(unk_field),
				    cmdname, fldn);
				err = 1;
			}
		}
		if (line2) {
			pp->line2 = fldp;
			if (fldp->width > pp->width)
				pp->width = fldp->width;
		} else {
			pp->line1 = fldp;
			pp->width = fldp->width;
		}
	}
	return (err);
}

/*
 * process_sort_fields -
 */
static int
process_sort_fields(
	int nsort,
	struct sort_el *list,
	char *fmt)
{
	int i;
	int rev;
	struct field_info *fldp;
	char *fmtx;
	char *fldn;
	int err;

	err = 0;
	fmtx = fmt;
	for (i = 0; i < nsort; i++) {
		fldn = get_field(&fmtx);
		fldp = &null_field;
		rev = 0;
		if (fldn != NULL) {
			struct field_info *tfldp;

			if (*fldn == '-') {
				rev = 1;
				fldn++;
			}
			tfldp = find_field(fldn);
			if (tfldp != NULL) {
				fldp = tfldp;
			} else {
				(void) fprintf(stderr, gettext(unk_field),
				    cmdname, fldn);
				err = 1;
			}
		}
		list[i].reverse = rev;
		list[i].fld = fldp;
	}
	return (err);
}

/*
 * print_fields -
 */
static cfga_err_t
print_fields(
	int ncol,
	struct print_col *list,
	int heading,
	int line2,
	char *delim,
	struct cfga_stat_data *bdp,
	FILE *fp)
{
	char *del;
	struct print_col *pp;
	struct field_info *fldp;
	static char *outline;
	char *lp;
	cfga_err_t calloc_error = CFGA_OK;

	if (outline == NULL) {
		int out_len, delim_len;

		delim_len = strlen(delim);
		out_len = 0;
		for (pp = list; pp < &list[ncol]; pp++) {
			out_len += pp->width;
			out_len += delim_len;
		}
		out_len -= delim_len;
		outline = (char *)config_calloc_check(out_len + 1, 1,
		    &calloc_error);
		if (calloc_error != CFGA_OK)
			return (calloc_error);
	}

	lp = outline;
	del = "";
	for (pp = list; pp < &list[ncol]; pp++) {
		fldp = line2 ? pp->line2 : pp->line1;
		(void) sprintf(lp, "%s", del);
		lp += strlen(lp);
		if (heading) {
			(void) sprintf(lp, "%-*s", fldp->width, fldp->heading);
		} else {
			(*fldp->printfn)(bdp, fldp->width, lp);
		}
		lp += strlen(lp);
		del = delim;
	}
	/* Trim trailing spaces */
	while (--lp >= outline && *lp == ' ')
		*lp = '\0';
	(void) fprintf(fp, "%s\n", outline);
	return (CFGA_OK);
}

/*
 * config_calloc_check - perform allocation, check result and
 * set error indicator
 */
static void *
config_calloc_check(
	size_t nelem,
	size_t elsize,
	cfga_err_t *error)
{
	void *p;
	static char alloc_fail[] =
"%s: memory allocation failed (%d*%d bytes)\n";

	p = calloc(nelem, elsize);
	if (p == NULL) {
		(void) fprintf(stderr, gettext(alloc_fail), cmdname,
		    nelem, elsize);
		*error = CFGA_LIB_ERROR;
	} else {
		*error = CFGA_OK;
	}

	return (p);
}

/*
 * find_arg_type - determine if an argument is an ap_id or an ap_type.
 */
static cfga_ap_types_t
find_arg_type(
	char *ap_id)
{
	struct stat buf;
	cfga_ap_types_t retval = UNKNOWN_AP;

	/*
	 * sanity checks
	 */
	if (ap_id == (char *)NULL)
		return (retval);

	if (strcmp(ap_id, "") == 0)
		return (retval);

	/*
	 * start with physical
	 * If it starts with at slash and is stat-able
	 * its a physical.
	 */
	if (*ap_id == '/') {
		if (stat(ap_id, &buf) == 0)
			return (PHYSICAL_AP_ID);
	}

	/*
	 * Check for ":" which is always present in an ap_id
	 * but not in an ap_type.
	 */
	if (strchr(ap_id, ':') == (char *)NULL)
		retval = AP_TYPE;
	else
		retval = LOGICAL_AP_ID;
	return (retval);
}
