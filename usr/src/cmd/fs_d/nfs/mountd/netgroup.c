/*
 *	netgroup.c
 *
 *	Copyright (c) 1988-1995 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#pragma ident	"@(#)netgroup.c 1.6     96/11/15 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <syslog.h>
#include <sys/param.h>
#include <rpc/rpc.h>
#include <sys/stat.h>
#include <netconfig.h>
#include <netdir.h>
#include <sys/file.h>
#include <sys/time.h>
#include <sys/errno.h>
#include <sys/resource.h>
#include <nfs/nfs.h>
#include <rpcsvc/mount.h>
#include <sys/pathconf.h>
#include <sys/systeminfo.h>
#include <sys/utsname.h>
#include <signal.h>
#include <locale.h>
#include <unistd.h>
#include <thread.h>
#include "../lib/sharetab.h"

extern char *exmalloc(int);

struct cache_entry {
	char	*cache_host;
	time_t	cache_time;
	int	cache_belong;
	char	**cache_grl;
	int	cache_grc;
	struct cache_entry *cache_next;
};

struct cache_entry *cache_head;

#define	VALID_TIME	60  /* seconds */

rwlock_t cache_lock;	/* protect the cache chain */

void netgroup_init(void);
int netgroup_check(struct nd_hostservlist *clnames, char *grl, int grc);
static void cache_free(struct cache_entry *entry);
static int cache_check(char *host, char **grl, int grc, int *belong);
static void cache_enter(char *host, char **grl, int grc, int belong);


void
netgroup_init()
{
	(void) rwlock_init(&cache_lock, USYNC_THREAD, NULL);
}

/*
 * Check whether any of the hostnames in clnames are
 * members (or non-members) of the netgroups in glist.
 * Since the innetgr lookup is rather expensive, the
 * result is cached. The cached entry is valid only
 * for VALID_TIME seconds.  This works well because
 * typically these lookups occur in clusters when
 * a client is mounting.
 *
 * Note that this routine establishes a host membership
 * in a list of netgroups - we've no idea just which
 * netgroup in the list it is a member of.
 */
int
netgroup_check(clnames, glist, grc)
	struct nd_hostservlist *clnames;
	char	*glist;	/* Contains (grc) strings separated by '\0' */
	int	grc;
{
	char **grl;
	char *gr;
	int nhosts = clnames->h_cnt;
	char *host0, *host;
	int i;
	int belong = 0;
	static char *domain;

	/*
	 * Private interface to innetgr().
	 * Accepts N strings rather than 1.
	 */
	extern	int __multi_innetgr();

	if (domain == NULL) {
		int	ssize;

		domain = exmalloc(SYS_NMLN);
		ssize = sysinfo(SI_SRPC_DOMAIN, domain, SYS_NMLN);
		if (ssize > SYS_NMLN) {
			free(domain);
			domain = exmalloc(ssize);
			ssize = sysinfo(SI_SRPC_DOMAIN, domain, ssize);
		}
		/* Check for error in syscall or NULL domain name */
		if (ssize <= 1) {
			syslog(LOG_ERR, "No default domain set");
			return (0);
		}
	}

	grl = (char **) calloc(grc, sizeof (char *));
	if (grl == NULL)
		return (0);

	for (i = 0, gr = glist; i < grc; i++, gr += strlen(gr) + 1)
		grl[i] = gr;

	host0 = clnames->h_hostservs[0].h_host;

	if (cache_check(host0, grl, grc, &belong))
		goto done;

	/*
	 * Check the netgroup for each of the
	 * hosts names (usually just one).
	 */
	for (i = 0; i < nhosts; i++) {
		host = clnames->h_hostservs[i].h_host;

		if (__multi_innetgr(grc,	grl,
				    1,		&host,
				    0,		NULL,
				    1,		&domain)) {
			belong = 1;
			break;
		}
	}

	cache_enter(host0, grl, grc, belong);
done:
	free(grl);
	return (belong);
}

/*
 * Free a cache entry and all entries
 * further down the chain since they
 * will also be expired.
 */
static void
cache_free(entry)
	struct cache_entry *entry;
{
	struct cache_entry *ce, *next;
	int i;

	for (ce = entry; ce; ce = next) {
		if (ce->cache_host)
			free(ce->cache_host);
		for (i = 0; i < ce->cache_grc; i++)
			if (ce->cache_grl[i])
				free(ce->cache_grl[i]);
		if (ce->cache_grl)
			free(ce->cache_grl);
		next = ce->cache_next;
		free(ce);
	}
}

/*
 * Search the entries in the cache chain looking
 * for an entry with a matching hostname and group
 * list.  If a match is found then return the "belong"
 * value which may be 1 or 0 depending on whether the
 * client is a member of the list or not.  This is
 * both a positive and negative cache.
 *
 * Cache entries have a validity of VALID_TIME seconds.
 * If we find an expired entry then blow away the entry
 * and the rest of the chain since entries further down
 * the chain will be expired too because we always add
 * new entries to the head of the chain.
 */
static int
cache_check(host, grl, grc, belong)
	char *host;
	char **grl;
	int grc;
	int *belong;
{
	struct cache_entry *ce, *prev;
	time_t timenow = time(NULL);
	int i;

	(void) rw_rdlock(&cache_lock);

	for (ce = cache_head; ce; ce = ce->cache_next) {

		/*
		 * If we find a stale entry, there can't
		 * be any valid entries from here on.
		 * Acquire a write lock, search the chain again
		 * and delete the stale entry and all following
		 * entries.
		 */
		if (timenow > ce->cache_time) {
		    (void) rw_unlock(&cache_lock);
		    (void) rw_wrlock(&cache_lock);

		    for (prev = NULL, ce = cache_head; ce;
			prev = ce, ce = ce->cache_next)
			if (timenow > ce->cache_time)
				break;

		    if (ce != NULL) {
			if (prev)
			    prev->cache_next = NULL;
			else
			    cache_head = NULL;

			cache_free(ce);
		    }

		    (void) rw_unlock(&cache_lock);

		    return (0);
		}

		if (ce->cache_grc != grc)
			continue;	/* no match */

		if (strcmp(host, ce->cache_host) != 0)
			continue;	/* no match */

		for (i = 0; i < grc; i++)
			if (strcmp(ce->cache_grl[i], grl[i]) != 0)
				break;	/* no match */
		if (i < grc)
			continue;

		*belong = ce->cache_belong;
		(void) rw_unlock(&cache_lock);

		return (1);
	}

	(void) rw_unlock(&cache_lock);

	return (0);
}

/*
 * Put a new entry in the cache chain by
 * prepending it to the front.
 * If there isn't enough memory then just give up.
 */
static void
cache_enter(host, grl, grc, belong)
	char *host;
	char **grl;
	int grc;
	int belong;
{
	struct cache_entry *entry;
	int i;

	entry = malloc(sizeof (*entry));
	if (entry == NULL)
		return;

	memset((caddr_t) entry, 0, sizeof (*entry));
	entry->cache_host = strdup(host);
	if (entry->cache_host == NULL) {
		cache_free(entry);
		return;
	}

	entry->cache_time = time(NULL) + VALID_TIME;
	entry->cache_belong = belong;
	entry->cache_grl = malloc(grc * sizeof (char *));
	if (entry->cache_grl == NULL) {
		cache_free(entry);
		return;
	}

	for (i = 0; i < grc; i++) {
		entry->cache_grl[i] = strdup(grl[i]);
		if (entry->cache_grl[i] == NULL) {
			entry->cache_grc = i;
			cache_free(entry);
			return;
		}
	}

	entry->cache_grc = grc;

	(void) rw_wrlock(&cache_lock);
	entry->cache_next = cache_head;
	cache_head = entry;
	(void) rw_unlock(&cache_lock);
}
