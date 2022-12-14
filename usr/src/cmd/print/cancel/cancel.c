/*
 * Copyright (c) 1994,1995,1996 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident	"@(#)cancel.c	1.9	97/03/28 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#ifndef SUNOS_4
#include <libintl.h>
#endif

#include <print/ns.h>
#include <print/network.h>
#include <print/misc.h>
#include <print/list.h>
#include <print/job.h>

#include <cancel_list.h>

extern char *optarg;
extern int optind, opterr, optopt, exit_code = 0;
extern char *getenv(const char *);

static int all = 0;	/* global for canceling everything */



/*
 *  vappend_string() appends the string contained in item to the string passed
 *	in via stdargs.  This is intended for use with list_iterate().
 */
void
vappend_string(char *item, va_list ap)
{
	char *string = va_arg(ap, char *);

	strcat(string, " ");
	strcat(string, item);
}


/*
 *  vcancel_local() attempts to cancel all locally spooled jobs that are
 *	are associated with a cancel_req_t structure.  This function is
 *	intended to be called by list_iterate().
 */
int
vcancel_local(cancel_req_t *entry, va_list ap)
{
	char	*user = va_arg(ap, char *);
	job_t	**list = NULL;

	list = job_list_append(list, entry->binding->printer, SPOOL_DIR);
	return (list_iterate((void **)list, (VFUNC_T)vjob_cancel, user,
			entry->binding->printer, entry->binding->server,
			entry->list));
}


/*
 *  vcancel_remote() attempts to send a cancel request to a print server
 *	for any jobs that might be associated with the cancel_req_t structure
 *	passed in.  This function is intended to be called by list_iterate().
 */
int
vcancel_remote(cancel_req_t *entry, va_list ap)
{
	char	buf[BUFSIZ],
		*user = va_arg(ap, char *),
		*printer = entry->binding->printer,
		*server = entry->binding->server;
	int	nd;

	if ((nd = net_open(server, 15)) < 0) {
		fprintf(stderr,
			gettext("could not talk to print service at %s\n"),
			server);
		return (-1);
	}

	memset(buf, NULL, sizeof (buf));
	if (strcmp(user, "-all") != 0)
		list_iterate((void *)entry->list, (VFUNC_T)vappend_string, buf);

	syslog(LOG_DEBUG, "vcancel_remote(): %s %s%s", printer, user, buf);
	net_printf(nd, "%c%s %s%s\n", REMOVE_REQUEST, printer, user, buf);

	while (memset(buf, NULL, sizeof (buf)) &&
		(net_read(nd, buf, sizeof (buf)) > 0))
		printf("%s", buf);

	net_close(nd);
	return (0);
}


/*
 *  vsysv_printer() adds an entry to the cancel list with the items supplied.
 */
void
vsysv_printer(char *printer, va_list ap)
{
	cancel_req_t ***list = va_arg(ap, cancel_req_t ***);
	char	**items = va_arg(ap, char **);

	*list = cancel_list_add_list(*list, printer, items);
}

/*
 *  vsysv_binding() adds an entry to the cancel list with the items supplied.
 */
void
vsysv_binding(ns_bsd_addr_t *binding, va_list ap)
{
	cancel_req_t ***list = va_arg(ap, cancel_req_t ***);
	char	**items = va_arg(ap, char **);

	*list = cancel_list_add_binding_list(*list, binding, items);
}

/*
 *  sysv_remove() parses the command line arguments as defined for cancel
 *	and builds a list of cancel_req_t structures to return
 */
cancel_req_t **
sysv_remove(int ac, char **av)
{
	char	*printer,
		**printers = NULL,
		**items = NULL,
		*tmp;
	int	c;
	int	current = 1,
		user = 0;
	ns_bsd_addr_t *binding;
	char *user_name = get_user_name();
	cancel_req_t **list = NULL;

	if (ac == 1) {
		fprintf(stderr,
			gettext("printer, request-id, and/or user required\n"));
		exit(-1);
	}

	if ((printer = getenv((const char *)"LPDEST")) == NULL)
		printer = getenv((const char *)"PRINTER");
	if (printer == NULL)
		printer = NS_NAME_DEFAULT;

	while ((c = getopt(ac, av, "u")) != EOF)
		switch (c) {
		case 'u':
			user++;
			break;
		default:
			fprintf(stderr,
			"Usage:\t%s [-u user-list] [printer-list]\n", av[0]);
			fprintf(stderr, "\t%s [request-list] [printer-list]\n",
				av[0]);
			exit(-1);
		}

	ac--;
	while (optind <= ac) {				/* pull printers off */
		char	*p,
			*q;

		if (((p = strrchr(av[ac], ':')) != NULL) &&
		    ((q = strrchr(p, '-')) != NULL)) {
			int req = 0;
			while (*++q != NULL)
				if (isdigit(*q) == 0)
					req++;
			if (req == 0)
				break;
		}
		if ((binding = ns_bsd_addr_get_name(av[ac])) != NULL) {
			printers = (char **)list_append((void **)printers,
							(void *)av[ac]);
		} else
			break;
		ac--;
	}

	while (optind <= ac) {				/* get reqs or users */
		if (user != 0) {	/* list o users */
			items = (char **)list_append((void **)items,
							(void *)av[ac]);
		} else {		/* list o jobs */
			char *p;

			if ((p = strrchr(av[ac], '-')) != NULL) { /* job-id */
				*(p++) = NULL;
				if (*p == NULL) {
					fprintf(stderr,
					gettext("invalid job id: %s-\n"),
						av[ac]);
					exit(-1);
				}
				list = cancel_list_add_item(list, av[ac], p);
			} else {			/* just a number */
				list = cancel_list_add_item(list, av[ac], NULL);
			}
		}
		ac--;
	}

	if ((printers == NULL) && (items != NULL)) { /* handle "all" printers */
		ns_bsd_addr_t **addrs = NULL;

		if ((addrs = ns_bsd_addr_get_all(UNIQUE)) != NULL)
			list_iterate((void **)addrs, (VFUNC_T)vsysv_binding,
					&list, items);
	}

	if ((list == NULL) && (items == NULL))
		items = (char **)list_append((void **)items, NULL);

	list_iterate((void **)printers, (VFUNC_T)vsysv_printer, &list, items);

	return (list);
}


/*
 *  bsd_remove() parses the command line arguments as defined for lprm
 *	and builds a list of cancel_req_t structures to return
 */
cancel_req_t **
bsd_remove(int ac, char **av)
{
	char	*printer;
	int	c;
	int	rmc = 0;
	cancel_req_t **list = NULL;

	if ((printer = getenv((const char *)"PRINTER")) == NULL)
		printer = getenv((const char *)"LPDEST");
	if (printer == NULL)
		printer = NS_NAME_DEFAULT;

	while ((c = getopt(ac, av, "P:-")) != EOF)
		switch (c) {
		case 'P':
			printer = optarg;
			break;
		default:
			fprintf(stderr,
		"Usage: %s [-P printer] [-] [job # ...] [username ...]\n",
				av[0]);
			exit(-1);
		}

	while (optind < ac)
		if (strcmp(av[optind++], "-") == 0) {
			if (getuid() == 0) {
				all = 1;
				list = cancel_list_add_item(list, printer,
							"-all");
			} else {
				list = cancel_list_add_item(list, printer,
							get_user_name());
			}
		} else {
			list = cancel_list_add_item(list, printer,
							av[optind-1]);
		}

	if (list == NULL)
		list = cancel_list_add_item(list, printer, NULL);

	return (list);
}


/*
 *  main() calls the appropriate routine to parse the command line arguments
 *	and then calls the local remove routine, followed by the remote remove
 *	routine to remove jobs.
 */
int
main(int ac, char *av[])
{
	int rc = 0;
	char *program;
	cancel_req_t **list = NULL;

	if ((program = strrchr(av[0], '/')) == NULL)
		program = av[0];
	else
		program++;

	openlog(program, LOG_PID, LOG_LPR);

	if (check_client_spool(NULL) < 0) {
		fprintf(stderr, "couldn't validate local spool area (%s)\n",
			SPOOL_DIR);
		exit(-1);
	}

	if (strcmp(program, "lprm") == 0)
		list = bsd_remove(ac, av);
	else
		list = sysv_remove(ac, av);

	chdir(SPOOL_DIR);
	if (list_iterate((void **)list, (VFUNC_T)vcancel_local,
			get_user_name()) != 0)
		start_daemon(1);

	rc = list_iterate((void **)list, (VFUNC_T)vcancel_remote,
				((all == 0) ? get_user_name() : "-all"));

	if (exit_code == 0)
		exit_code = rc;

	exit(exit_code);
}
