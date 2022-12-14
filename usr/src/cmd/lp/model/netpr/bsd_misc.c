
/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)bsd_misc.c 1.10     97/07/02 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <libintl.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <syslog.h>
#include "netpr.h"
#include "netdebug.h"

static int job_primative(np_bsdjob_t *, char, char *);
static int create_cfA_file(np_bsdjob_t *);
static char * create_cfname(np_bsdjob_t *);
static char * create_dfname(np_bsdjob_t *);

np_bsdjob_t *
create_bsd_job(np_job_t * injob, int pr_order, int filesize)
{

	np_bsdjob_t *job;
	char *id;
	char *p;
	int x;
	np_data_t * jobdata;

	if ((injob->request_id == NULL) || (injob->username == NULL) ||
	    (injob->dest == NULL) || (injob->printer ==  NULL)) {
		return (NULL);
	}

	job = (np_bsdjob_t *)malloc(sizeof (np_bsdjob_t));
	ASSERT(job, MALLOC_ERR);
	(void) memset(job, 0, sizeof (np_bsdjob_t));
	/*
	 * request-id comes in as printer-number
	 * pull apart to create number
	 */
	if ((id = strrchr(injob->request_id, (int)'-')) == NULL) {
		(void) fprintf(stderr,
		gettext("Netpr: request_id in unknown format:<%s>\n"),
			injob->request_id);
		syslog(LOG_DEBUG, "request id in unknown format: %s",
			injob->request_id);
		return (NULL);
	}

	id++;
	if ((strlen(id)) == MAX_REQ_ID) {
		job->np_request_id = malloc(strlen(id) + 1);
		ASSERT(job->np_request_id, MALLOC_ERR);
		(void) strcpy(job->np_request_id, id);
	} else {
		/* prepend job id with zeros, if needed */
		job->np_request_id =  calloc(1, MAX_REQ_ID + 1);
		ASSERT(job->np_request_id, MALLOC_ERR);
		p = job->np_request_id;
		for (x = MAX_REQ_ID - strlen(id); x > 0; x--)
			*p++ = '0';
		(void) strcat(job->np_request_id, id);

	}

	/*
	 * strip front off username : host!username
	 */

	if ((id = strrchr(injob->username, (int)'!')) == NULL) {
		(void) fprintf(stderr,
		gettext("Netpr: username in unknown format:<%s>\n"),
			injob->username);
		syslog(LOG_DEBUG, "username in unknown format: %s",
			injob->username);
		return (NULL);
	}
	/* get the host name and return string to original value */
	*id = '\0';
	job->np_host = strdup(injob->username);
	*id = '!';

	id++;
	job->np_username = id;

	job->np_printer = injob->printer;
	job->np_filename = injob->filename;

	job->np_df_letter = 'A';

	/* build cfAfilename: (cfA)(np_request_id)(np_host) */
	if ((job->np_cfAfilename = create_cfname(job)) == NULL) {
		(void) fprintf(stderr,
			gettext("Netpr: System error creating cfAfilename\n"));
			syslog(LOG_DEBUG, "System error creating cfAfilename");
		return (NULL);
	}

	job->np_timeout = injob->timeout;
	job->np_banner = injob->banner;
	job->np_print_order = pr_order;

	if (injob->title == NULL)
		job->np_title = injob->filename;
	else
		job->np_title = injob->title;

	if ((create_cfA_file(job)) == -1) {
		(void) fprintf(stderr,
		gettext("Netpr: Cannot create bsd control file\n"));
		syslog(LOG_DEBUG, "Cannot create bsd control file");
		return (NULL);
	}

	/* Now we have a title, add to the control file */
	if (injob->banner == BANNER) {
		(void) job_primative(job, 'C', job->np_host);
		(void) job_primative(job, 'J', job->np_title);
		(void) job_primative(job, 'L', job->np_username);
	}


	/* create dfname for this file */

	/* allocate the jobdata and initialize what we have so far */
	jobdata = malloc(sizeof (np_data_t));
	ASSERT(jobdata, MALLOC_ERR);
	(void) memset(jobdata, 0, sizeof (np_data_t));

	jobdata->np_path_file = malloc(strlen(job->np_filename) + 1);
	ASSERT(jobdata->np_path_file, MALLOC_ERR);
	(void) strcpy(jobdata->np_path_file, job->np_filename);

	jobdata->np_data_size = filesize;

	if ((jobdata->np_dfAfilename = create_dfname(job)) == NULL) {
		return (NULL);
	}
	/*
	 * attach np_data to bsdjob
	 */
	job->np_data = jobdata;

	return (job);
}


/*
 * Create df<x>name for this file
 * df<X><nnn><hostname>
 */
static char *
create_dfname(np_bsdjob_t *job)
{
	char * dfname;

	if (job == NULL)
		return (NULL);

	/* Trying to print too many files */
	if (job->np_df_letter > 'z') {
		errno = ENFILE;
		return (NULL);
	}

	dfname = (char *)malloc(strlen(job->np_host) + 3 + 3 + 1);
	ASSERT(dfname, MALLOC_ERR);
	(void) memset(dfname, 0, strlen(job->np_host) + 3 + 3 + 1);
	(void) sprintf(dfname, "%s%c%s%s", "df", job->np_df_letter,
	    job->np_request_id, job->np_host);

	/* udate np_df_letter for the next caller */
	job->np_df_letter += 1;
	if ((job->np_df_letter > 'Z') && (job->np_df_letter < 'a'))
		job->np_df_letter = 'a';

	return (dfname);
}

static char *
create_cfname(np_bsdjob_t * job)
{
	char * cfname;

	if (job == NULL)
		return (NULL);

	cfname = (char *)malloc(strlen(job->np_host) + 3 + 3 + 1);
	ASSERT(cfname, MALLOC_ERR);
	(void) memset(cfname, 0, strlen(job->np_host) + 3 + 3 + 1);
	(void) sprintf(cfname, "%s%s%s", "cfA",
	job->np_request_id, job->np_host);
	return (cfname);
}

static int
create_cfA_file(np_bsdjob_t *job)
{
	/*
	 * Read through job structure, creating entries
	 * in control file as appropriate
	 */
	if ((job->np_host == NULL) || (job->np_username == NULL)) {
		(void) fprintf(stderr, gettext(
		"Netpr: Missing required data, cannot build control file\n"));
		return (-1);
	}
	(void) job_primative(job, 'H', job->np_host);
	(void) job_primative(job, 'P', job->np_username);

	return (0);
}

static int
job_primative(np_bsdjob_t * job, char option, char *value)
{
	char buf[BUFSIZ];

	if ((job == NULL) || (value == NULL))
		return (-1);

	job->np_cfAfilesize += strlen(value) + 2; /* (opt)(value)\n */
	if (job->np_cfAfile == NULL) {
		/* Always allocate one greater than cfAfilesize for the \0 */
		job->np_cfAfile = calloc(1, job->np_cfAfilesize + 1);
		ASSERT(job->np_cfAfile, MALLOC_ERR);
	} else {
		job->np_cfAfile = realloc(job->np_cfAfile,
			job->np_cfAfilesize + 1);
		ASSERT(job->np_cfAfile, REALLOC_ERR);
	}
	(void) snprintf(buf, sizeof (buf),  "%c%s\n", option, value);
	(void) strcat(job->np_cfAfile, buf);


	return (0);
}
