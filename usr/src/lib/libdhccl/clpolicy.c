/*
 * clpolicy.c: "Read the agent configuration file and set parameters".
 *
 * SYNOPSIS
 *    void clpolicy(const char *)
 *
 * DESCRIPTION
 *    Read and parse the policy file (client.pcy). This file contains
 *    parameters affecting the operation of the client including:
 *
 *        backoff and retransmission times
 *        class id
 *        client id type specifier
 *        arp timeout
 *        parameters required from DHCP
 *        BOOTP compatibility switch
 *        lease time desired
 *        permit/deny using previously cached configuration
 *        # failed discover-offer-request-nak sequences permitted
 *        random wait before starting a new interface
 *
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)clpolicy.c 1.4 97/01/26 SMI"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "client.h"
#include "utils.h"
#include "hostdefs.h"
#include "hosttype.h"
#include "error.h"
#include "msgindex.h"
#include <alloca.h>
#include "ca_dict.h"
#include "camacros.h"
#include "ca_vbuf.h"
#include "unixgen.h"

/* These dictionaries must be lexically ordered by the desc member */

#define	PARM_REQUEST		0
#define	PARM_WAIT		1
#define	PARM_RETRIES		2
#define	PARM_TIMEOUTS		3
#define	PARM_JITTER		4
#define	PARM_ARP_TIMEOUT	5
#define	PARM_LEASE		6
#define	PARM_USESAVED		7
#define	PARM_CLASS		8
#define	PARM_CLIENT_ID		9
#define	PARM_BOOTP_OK		10
#define	PARM_DONTWANT		11
#define	PARM_DONTUSESAVED	12
#define	PARM_BOOTP_NOTOK	13

#define	MAXTOKENS	3
#ifndef MIN
#define	MIN(a, b) ((a) < (b) ? (a) : (b))
#endif
DHCPCLIENT client;

/* These must be in lexical order: */
static ca_dict config_options[] = {
	{ "!accept_bootp",	PARM_BOOTP_NOTOK},
	{ "!request",		PARM_DONTWANT},
	{ "!use_saved_config",	PARM_DONTUSESAVED},
	{ "accept_bootp",	PARM_BOOTP_OK},
	{ "arp_timeout",	PARM_ARP_TIMEOUT},
	{ "class_id",		PARM_CLASS},
	{ "client_id",		PARM_CLIENT_ID},
	{ "lease_desired",	PARM_LEASE},
	{ "max_bad_offers",	PARM_RETRIES},
	{ "request",		PARM_REQUEST},
	{ "start_delay",	PARM_WAIT},
	{ "timeouts",		PARM_TIMEOUTS},
	{ "use_saved_config",	PARM_USESAVED}
};

static void
pre_defaults(void)
{
	char *q;

	client.arp_timeout = 2;
	client.waitonboot = 10;
	client.retries = 2;
	client.timeouts = 0;
	client.lease_desired = 0; /* don't care */
	client.timewindow = 60;
#if EMPLOY_BOOTP
	client.acceptBootp = 1;
#endif
	q = xstrdup("SUNW.$CPU.$PLATFORM.$OSNAME");
	convertClass(&client.class_id, q);
	free(q);
	client.client_id = xstrdup("$IFHTYPE$IFHADDR.$IFNAME");

	/* These are the parameters request by default: */
	onhost(client.reqvec, TAG_SUBNET_MASK);
	onhost(client.reqvec, TAG_TIME_OFFSET);
	onhost(client.reqvec, TAG_GATEWAYS);
	onhost(client.reqvec, TAG_NAME_SERVERS);
	onhost(client.reqvec, TAG_DOMAIN_SERVERS);
	onhost(client.reqvec, TAG_HOSTNAME);
	onhost(client.reqvec, TAG_DNS_DOMAIN);
	onhost(client.reqvec, TAG_BROADCAST_FLAVOR);
	onhost(client.reqvec, TAG_STATIC_ROUTES);
	onhost(client.reqvec, TAG_NIS_DOMAIN);
	onhost(client.reqvec, TAG_NIS_SERVERS);
	onhost(client.reqvec, TAG_NISPLUS_DOMAIN);
	onhost(client.reqvec, TAG_NISPLUS_SERVERS);
	onhost(client.reqvec, TAG_BEROUTER);
	onhost(client.reqvec, TAG_STATIC_ROUTES);
}

static void
post_defaults(void)
{
	static short timeouts[] = { 4, 8, 16, 32, -120 };

	if (client.timeouts == 0)
		client.timeouts = timeouts;
	if (client.class_id != 0)
		setDefaultVendor(client.class_id);
}

static int
ReadFile(FILE *f)
{
	char *p[MAXTOKENS + 1], *q[3];
	int rc, nqtok, nptok;
	int match_len;
	int tag;
	struct Vbuf b;
	const HTSTRUCT *htp;

	b.len = 0;
	b.dbuf = 0;
	while (xgets(f, &b) != EOF) {
		nqtok = tokeniseString(b.dbuf, q, '#', 2);
		if (nqtok <= 0)
			continue;
		nptok = tokeniseString(q[0], p, 1, MAXTOKENS);
		for (; --nqtok >= 0; )
			free(q[nqtok]);
		if (nptok == 0)
			continue;
		rc = seekdict(p[0], config_options, ARY_SIZE(config_options),
		    &match_len);
		if (rc < 0 || match_len < strlen(p[0])) {
			logw(DHCPCMSG54, p[0]);
			for (; --nptok >= 0; )
				free(p[nptok]);
			continue;
		}
		if (match_len == strlen(p[0]))
			logw(DHCPCMSG55, p[0], config_options[rc].desc);

		switch (config_options[rc].tag) {
		case PARM_REQUEST:
		case PARM_DONTWANT:
			if (nptok < 2) {
				loge(DHCPCMSG56, config_options[rc].desc);
			} else if (sscanf(p[1], "%d", &tag) == 1) {
				onhost(client.reqvec, tag);
			} else {
				htp = find_bylongname(p[1]);
				if (htp == 0)
					loge(DHCPCMSG58, p[1]);
				else {
					if (config_options[rc].tag ==
					    PARM_REQUEST)
						onhost(client.reqvec, htp->tag);
					else
						ofhost(client.reqvec, htp->tag);
				}
			}
			break;
		case PARM_CLIENT_ID:
			if (client.client_id != 0) {
				free(client.client_id);
				client.client_id = 0;
			}
			if (nptok >= 2)
				client.client_id = xstrdup(p[1]);
			break;
		case PARM_CLASS:
			if (client.class_id != 0) {
				free(client.class_id);
				client.class_id = 0;
			}
			if (nptok >= 2)
				convertClass(&client.class_id, p[1]);
			break;
		case PARM_WAIT:
			if (nptok < 2)
				loge(DHCPCMSG56, config_options[rc].desc);
			else {
				if (sscanf(p[1], "%hu",
				    &client.waitonboot) != 1) {
					loge(DHCPCMSG57,
					    config_options[rc].desc, p[1]);
				}
			}
			break;
		case PARM_RETRIES:
			if (nptok < 2)
				loge(DHCPCMSG56, config_options[rc].desc);
			else {
				if (sscanf(p[1], "%hu", &client.retries) != 1) {
					loge(DHCPCMSG57,
					    config_options[rc].desc, p[1]);
				}
			}
			break;
		case PARM_TIMEOUTS:
			if (nptok < 2)
				loge(DHCPCMSG59, config_options[rc].desc);
			else {
				int i, nrtok;
				char **r;
				nrtok = countTokens(p[1], ',', 0);
				client.timeouts = (short *)xmalloc(
				    (2 + nrtok) * sizeof (u_short));
				r = (char **)xmalloc((1 + nrtok) *
				    sizeof (char *));
				tokeniseString(p[1], r, ',', 0);
				for (i = 0; i < nrtok; i++) {
					if (sscanf(r[i], "%hd",
					    client.timeouts + i) != 1)
						break;
					else {
						if (client.timeouts[i] <= 0)
							break;
					}
				}
				if (i < (nrtok - 1)) {
					logw(DHCPCMSG59,
					    config_options[rc].desc);
					free(client.timeouts);
					client.timeouts = 0;
				} else {
					if (client.timeouts[nrtok - 1] > 0)
						client.timeouts[nrtok] = 0;
				}
				for (i = 0; i < nrtok; i++)
					free(r[i]);
				free(r);
			}
			break;
		case PARM_ARP_TIMEOUT:
			if (nptok < 2)
				loge(DHCPCMSG56, config_options[rc].desc);
			else
				rc = sscanf(p[1], "%u", &client.arp_timeout);
			if (rc != 1)
				loge(DHCPCMSG57, config_options[rc].desc, p[1]);
			break;
		case PARM_LEASE:
			if (nptok < 2)
				loge(DHCPCMSG56, config_options[rc].desc);
			else {
				if (sscanf(p[1], "%d",
				    &client.lease_desired) != 1) {
					loge(DHCPCMSG57,
					    config_options[rc].desc, p[1]);
				}
			}
			break;
		case PARM_USESAVED:
			client.useNonVolatile = 1;
			break;
		case PARM_DONTUSESAVED:
			client.useNonVolatile = 0;
			break;
		case PARM_BOOTP_OK:
			client.acceptBootp = 1;
			break;
		case PARM_BOOTP_NOTOK:
			client.acceptBootp = 0;
			break;
		}
		for (; --nptok >= 0; )
			free(p[nptok]);
	}
	free(b.dbuf);
	return (0);
}

int
clpolicy(const char *configdir)
{
	char *configfile;
	FILE *f;
	int rc;

	pre_defaults();

	configfile = (char *)alloca(2 + strlen(configdir) +
	    strlen(POLICY_FILE));
	sprintf(configfile, "%s%s", configdir, POLICY_FILE);
	f = fopen(configfile, "r");
	if (f != 0) {
		rc = ReadFile(f);
		if (rc)
			return (rc);
		fclose(f);
	}
	post_defaults();
	return (0);
}
