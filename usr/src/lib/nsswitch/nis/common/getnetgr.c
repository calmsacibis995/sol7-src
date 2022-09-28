/*
 *	Copyright (c) 1988-1992 Sun Microsystems Inc
 *	All Rights Reserved.
 *
 *	nis/getnetgrent.c -- "nis" backend for nsswitch "netgroup" database
 *
 *	The API for netgroups differs sufficiently from that for the average
 *	getXXXbyYYY function that we use very few of the support routines in
 *	nis_common.h.
 *
 *	The implementation of setnetgrent()/getnetgrent() here follows the
 *	the 4.x code, inasmuch as the setnetgrent() routine does all the work
 *	of traversing the netgroup graph and building a (potentially large)
 *	list in memory, and getnetgrent() just steps down the list.
 *
 *	An alternative, and probably better, implementation would lazy-eval
 *	the netgroup graph in response to getnetgrent() calls (though
 *	setnetgrent() should still check for the top-level netgroup name
 *	and return NSS_SUCCESS / NSS_NOTFOUND).
 */

#pragma ident "@(#)getnetgrent.c	1.13	97/08/27 SMI"

#include "nis_common.h"
#include <ctype.h>
#include <rpcsvc/ypclnt.h>
#include <malloc.h>
#include <string.h>

/*
 * The nss_backend_t for a getnetgrent() sequence;  we actually give the
 *   netgroup frontend a pointer to one of these structures in response to
 *   a (successful) setnetgrent() call on the nis_netgr_be backend
 *   described further down in this file.
 */

struct nis_getnetgr_be;
typedef nss_status_t	(*nis_getnetgr_op_t)(struct nis_getnetgr_be *, void *);

struct nis_getnetgr_be {
	nis_getnetgr_op_t	*ops;
	nss_dbop_t		n_ops;
	/*
	 * State for set/get/endnetgrent()
	 */
	char			*netgroup;
	struct grouplist	*all_members;
	struct grouplist	*next_member;
};

struct grouplist {  /* One element of the list generated by a setnetgrent() */
	char			*triple[NSS_NETGR_N];
	struct	grouplist	*gl_nxt;
};

static nss_status_t
getnetgr_set(be, a)
	struct nis_getnetgr_be	*be;
	void			*a;
{
	const char		*netgroup = (const char *) a;

	if (be->netgroup != 0 &&
	    strcmp(be->netgroup, netgroup) == 0) {
		/* We already have the member-list;  regurgitate it */
		be->next_member = be->all_members;
		return (NSS_SUCCESS);
	}
	return (NSS_NOTFOUND);
}

static nss_status_t
getnetgr_get(be, a)
	struct nis_getnetgr_be	*be;
	void			*a;
{
	struct nss_getnetgrent_args *args = (struct nss_getnetgrent_args *) a;
	struct grouplist	*mem;

	if ((mem = be->next_member) == 0) {
		args->status = NSS_NETGR_NO;
	} else {
		char			*buffer	= args->buffer;
		int			buflen	= args->buflen;
		enum nss_netgr_argn	i;

		args->status = NSS_NETGR_FOUND;

		for (i = 0;  i < NSS_NETGR_N;  i++) {
			const char	*str;
			ssize_t	len;

			if ((str = mem->triple[i]) == 0) {
				args->retp[i] = 0;
			} else if ((len = strlen(str) + 1) <= buflen) {
				args->retp[i] = buffer;
				memcpy(buffer, str, len);
				buffer += len;
				buflen -= len;
			} else {
				args->status = NSS_NETGR_NOMEM;
				break;
			}
		}
		be->next_member	= mem->gl_nxt;
	}
	return (NSS_SUCCESS);	/* Yup, even for end-of-list, i.e. */
				/* do NOT advance to next backend. */
}

/*ARGSUSED*/
static nss_status_t
getnetgr_end(be, dummy)
	struct nis_getnetgr_be	*be;
	void			*dummy;
{
	struct grouplist	*gl;

	for (gl = be->all_members; gl != NULL; gl = gl->gl_nxt) {
		enum nss_netgr_argn	i;

		for (i = NSS_NETGR_MACHINE;  i < NSS_NETGR_N;  i++) {
			if (gl->triple[i] != 0) {
				free(gl->triple[i]);
			}
		}
		free(gl);
	}
	be->all_members = 0;
	be->next_member = 0;
	if (be->netgroup != 0) {
		free(be->netgroup);
		be->netgroup = 0;
	}
	return (NSS_SUCCESS);
}

/*ARGSUSED*/
static nss_status_t
getnetgr_destr(be, dummy)
	struct nis_getnetgr_be	*be;
	void			*dummy;
{
	if (be != 0) {
		getnetgr_end(be, (void *)0);
		free (be);
	}
	return (NSS_SUCCESS);
}

static nis_getnetgr_op_t getnetgr_ops[] = {
	getnetgr_destr,
	getnetgr_end,
	getnetgr_set,
	getnetgr_get,	/* getnetgrent_r() */
};


/*
 * The nss_backend_t for innetgr() and setnetgrent().
 */

struct nis_netgr_be;
typedef nss_status_t	(*nis_netgr_op_t)(struct nis_netgr_be *, void *);

struct nis_netgr_be {
	nis_netgr_op_t		*ops;
	nss_dbop_t		n_ops;
	const char		*domain;	/* (default) YP domain */
};


/*
 * Code to do top-down search in the graph defined by the 'netgroup' YP map
 */

/*
 * ===> This code is now used for setnetgrent(), not just innetgr().
 *
 * If the easy way doesn't pan out, recursively search the 'netgroup' map.
 * In order to do this, we:
 *
 *    -	remember all the netgroup names we've seen during this search,
 *	whether or not we've expanded them yet (we want fast insertion
 *	with duplicate-detection, so use yet another chained hash table),
 *
 *    -	keep a list of all the netgroups we haven't expanded yet (we just
 *	want fast insertion and pop-first, so a linked list will do fine).
 *	If we insert at the head, we get a depth-first search;  insertion
 *	at the tail gives breadth-first (?), which seems preferable (?).
 *
 * A netgrnam struct contains pointers for both the hash-table and the list.
 * It also contains the netgroup name;  note that we embed the name at the
 * end of the structure rather than holding a pointer to yet another
 * malloc()ed region.
 *
 * A netgrtab structure contains the hash-chain heads and the head/tail
 * pointers for the expansion list.
 *
 * Most of this code is common to at least the NIS and NIS+ backends;  it
 * should be generalized and, presumably, moved into the frontend.
 * ==> Not any longer...
 */

struct netgrnam {
	struct netgrnam	*hash_chain;
	struct netgrnam	*expand_next;
	char		name[1];	/* Really [strlen(name) + 1] */
};

#define	HASHMOD	113

struct netgrtab {
	struct netgrnam	*expand_first;
	struct netgrnam	**expand_lastp;
	struct netgrnam	*hash_heads[HASHMOD];
};

static void
ngt_init(ngt)
	struct netgrtab	*ngt;
{
	memset((void *)ngt, 0, sizeof (*ngt));
	ngt->expand_lastp = &ngt->expand_first;
}

/* === ? Change ngt_init() and ngt_destroy() to malloc/free struct netgrtab */

static void
/* ==> ? Should return 'failed' (out-of-memory) status ? */
ngt_insert(ngt, name, namelen)
	struct netgrtab	*ngt;
	const char	*name;
	size_t		namelen;
{
	unsigned	hashval;
	size_t		i;
	struct netgrnam	*cur;
	struct netgrnam	**head;

#define	dummy		((struct netgrnam *)0)

	for (hashval = 0, i = 0;  i < namelen;  i++) {
		hashval = (hashval << 2) + hashval +
			((const unsigned char *)name)[i];
	}
	head = &ngt->hash_heads[hashval % HASHMOD];
	for (cur = *head;  cur != 0;  cur = cur->hash_chain) {
		if (strncmp(cur->name, name, namelen) == 0 &&
		    cur->name[namelen] == 0) {
			return;		/* Already in table, do nothing */
		}
	}
	/* Create new netgrnam struct */
	cur = (struct netgrnam *)
		malloc(namelen + 1 + (char *)&dummy->name[0] - (char *)dummy);
	if (cur == 0) {
		return;			/* Out of memory, too bad */
	}
	memcpy(cur->name, name, namelen);
	cur->name[namelen] = 0;

	/* Insert in hash table */
	cur->hash_chain = *head;
	*head = cur;

	/* Insert in expansion list (insert at end for breadth-first search */
	cur->expand_next = 0;
	*ngt->expand_lastp = cur;
	ngt->expand_lastp = &cur->expand_next;

#undef	dummy
}

static const char *
ngt_next(ngt)
	struct netgrtab	*ngt;
{
	struct netgrnam	*first;

	if ((first = ngt->expand_first) == 0) {
		return (0);
	}
	if ((ngt->expand_first = first->expand_next) == 0) {
		ngt->expand_lastp = &ngt->expand_first;
	}
	return (first->name);
}

static void
ngt_destroy(ngt)
	struct netgrtab	*ngt;
{
	struct netgrnam	*cur;
	struct netgrnam *next;
	int		i;

	for (i = 0;  i < HASHMOD;  i++) {
		for (cur = ngt->hash_heads[i];  cur != 0; /* cstyle */) {
			next = cur->hash_chain;
			free(cur);
			cur = next;
		}
	}
	/* Don't bother zeroing pointers;  must do init if we want to reuse */
}

typedef const char *ccp;

static nss_status_t
top_down(struct nis_netgr_be *be, const char **groups, int ngroups,
    int (*func)(ccp triple[3], void *iter_args, nss_status_t *return_val),
    void *iter_args)
{
	struct netgrtab		*ngt;
	/* netgrtab goes on the heap, not the stack, because it's large and */
	/* stacks may not be all that big in multi-threaded programs. */

	const char		*group;
	int			nfound;
	int			done;
	nss_status_t		result;

	if ((ngt = (struct netgrtab *) malloc(sizeof (*ngt))) == 0) {
		return (NSS_UNAVAIL);
	}
	ngt_init(ngt);

	while (ngroups > 0) {
		ngt_insert(ngt, *groups, strlen(*groups));
		groups++;
		ngroups--;
	}

	done	= 0;	/* Set to 1 to indicate that we cut the iteration  */
			/*   short (and 'result' holds the return value)   */
	nfound	= 0;	/* Number of successful netgroup yp_match calls	   */

	while (!done && (group = ngt_next(ngt)) != 0) {
		char		*val;
		int		vallen;
		char		*p;
		int		yperr;

		result = _nss_nis_ypmatch(be->domain, "netgroup", group,
					&val, &vallen, &yperr);
		if (result != NSS_SUCCESS) {
			if (result == NSS_NOTFOUND) {
#ifdef	DEBUG
				syslog(LOG_WARNING,
				    "NIS netgroup lookup: %s doesn't exist",
				    group);
#endif	DEBUG
			} else {
#ifdef	DEBUG
				syslog(LOG_WARNING,
			"NIS netgroup lookup: yp_match returned [%s]",
				    yperr_string(yperr));
#endif	DEBUG
				done = 1;	/* Give up, return result */
			}
			/* Don't need to clean up anything */
			continue;
		}

		nfound++;

		if ((p = strpbrk(val, "#\n")) != 0) {
			*p = '\0';
		}
		p = val;

		/* Parse val into triples and recursive netgroup references */
		/*CONSTCOND*/
		while (1) {
			ccp			triple[NSS_NETGR_N];
			int			syntax_err;
			enum nss_netgr_argn	i;

			while (isspace(*p)) {
				p++;
			}
			if (*p == '\0') {
				/* Finished processing this particular val */
				break;
			}
			if (*p != '(') {
				/* Doesn't look like the start of a triple, */
				/*   so assume it's a recursive netgroup.   */
				char *start = p;
				p = strpbrk(start, " \t");
				if (p == 0) {
					/* Point p at the final '\0' */
					p = start + strlen(start);
				}
				ngt_insert(ngt, start, (size_t)(p - start));
				continue;
			}

			/* Main case:  a (machine, user, domain) triple */
			p++;
			syntax_err = 0;
			for (i = NSS_NETGR_MACHINE; i < NSS_NETGR_N; i++) {
				char		*start;
				char		*limit;
				const char	*terminators = ",) \t";

				if (i == NSS_NETGR_DOMAIN) {
					/* Don't allow comma */
					terminators++;
				}
				while (isspace(*p)) {
					p++;
				}
				start = p;
				limit = strpbrk(start, terminators);
				if (limit == 0) {
					syntax_err++;
					break;
				}
				p = limit;
				while (isspace(*p)) {
					p++;
				}
				if (*p == terminators[0]) {
					/*
					 * Successfully parsed this name and
					 *   the separator after it (comma or
					 *   right paren); leave p ready for
					 *   next parse.
					 */
					p++;
					if (start == limit) {
						/* Wildcard */
						triple[i] = 0;
					} else {
						*limit = '\0';
						triple[i] = start;
					}
				} else {
					syntax_err++;
					break;
				}
			}

			if (syntax_err) {
/*
 * ===> log it;
 * ===> try skipping past next ')';  failing that, abandon the line;
 */
				break;	/* Abandon this line */
			} else if (!(*func)(triple, iter_args, &result)) {
				/* Return result, good or bad */
				done = 1;
				break;
			}
		}
		/* End of inner loop over val[] */
		free(val);
	}
	/* End of outer loop (!done && ngt_next(ngt) != 0) */

	ngt_destroy(ngt);
	free(ngt);

	if (done) {
		return (result);
	} else if (nfound > 0) {
		/* ==== ? Should only do this if all the top-level groups */
		/*	  exist in YP?					  */
		return (NSS_SUCCESS);
	} else {
		return (NSS_NOTFOUND);
	}
}


/*
 * Code for setnetgrent()
 */

/*
 * Iterator function for setnetgrent():  copy triple, add to be->all_members
 */
static int
save_triple(ccp trippp[NSS_NETGR_N], void *headp_arg,
    nss_status_t *return_val)
{
	struct grouplist	**headp = headp_arg;
	struct grouplist	*gl;
	enum nss_netgr_argn	i;

	if ((gl = (struct grouplist *)malloc(sizeof (*gl))) == 0) {
		/* Out of memory */
		*return_val = NSS_UNAVAIL;
		return (0);
	}
	for (i = NSS_NETGR_MACHINE;  i < NSS_NETGR_N;  i++) {
		if (trippp[i] == 0) {
			/* Wildcard */
			gl->triple[i] = 0;
		} else if ((gl->triple[i] = strdup(trippp[i])) == 0) {
			/* Out of memory.  Free any we've allocated */
			enum nss_netgr_argn	j;

			for (j = NSS_NETGR_MACHINE;  j < i;  j++) {
				if (gl->triple[j] != 0) {
					free(gl->triple[j]);
				}
			}
			*return_val = NSS_UNAVAIL;
			return (0);
		}
	}
	gl->gl_nxt = *headp;
	*headp = gl;
	return (1);	/* Tell top_down() to keep iterating */
}

static nss_status_t
netgr_set(be, a)
	struct nis_netgr_be	*be;
	void			*a;
{
	struct nss_setnetgrent_args *args = (struct nss_setnetgrent_args *) a;
	struct nis_getnetgr_be	*get_be;
	nss_status_t		res;

	get_be = (struct nis_getnetgr_be *) malloc(sizeof (*get_be));
	if (get_be == 0) {
		return (NSS_UNAVAIL);
	}

	get_be->all_members = 0;
	res = top_down(be, &args->netgroup, 1, save_triple,
		&get_be->all_members);

	if (res == NSS_SUCCESS) {
		get_be->ops		= getnetgr_ops;
		get_be->n_ops		= sizeof (getnetgr_ops) /
						sizeof (getnetgr_ops[0]);
		get_be->netgroup	= strdup(args->netgroup);
		get_be->next_member	= get_be->all_members;

		args->iterator		= (nss_backend_t *) get_be;
	} else {
		args->iterator		= 0;
		free(get_be);
	}
	return (res);
}


/*
 * Code for innetgr()
 */

/*
 * Iterator function for innetgr():  Check whether triple matches args
 */
static int
match_triple(ccp triple[NSS_NETGR_N], void *ia_arg, nss_status_t *return_val)
{
	struct nss_innetgr_args	*ia = ia_arg;
	enum nss_netgr_argn	i;

	for (i = NSS_NETGR_MACHINE;  i < NSS_NETGR_N;  i++) {
		int		(*cmpf)(const char *, const char *);
		char		**argv;
		int		n;
		const char	*name = triple[i];
		int		argc = ia->arg[i].argc;

		if (argc == 0 || name == 0) {
			/* Wildcarded on one side or t'other */
			continue;
		}
		argv = ia->arg[i].argv;
		cmpf = (i == NSS_NETGR_MACHINE) ? strcasecmp : strcmp;
		for (n = 0;  n < argc;  n++) {
			if ((*cmpf)(argv[n], name) == 0) {
				break;
			}
		}
		if (n >= argc) {
			/* Match failed, tell top_down() to keep looking */
			return (1);
		}
	}
	/* Matched on all three, so quit looking and declare victory */

	ia->status = NSS_NETGR_FOUND;
	*return_val = NSS_SUCCESS;
	return (0);
}

/*
 * inlist() -- return 1 if at least one item from the "what" list
 *   is in the comma-separated, newline-terminated "list"
 */
static const char comma = ',';	/* Don't let 'cfix' near this */

static int
inlist(nwhat, pwhat, list)
	nss_innetgr_argc	nwhat;
	nss_innetgr_argv	pwhat;
	char			*list;
{
	char			*p;
	nss_innetgr_argc	nw;
	nss_innetgr_argv	pw;

	while (*list != 0) {
		while (*list == comma || isspace(*list))
			list++;
		for (p = list;  *p != 0 && *p != comma &&
		    !isspace(*p); /* nothing */)
			p++;
		if (p != list) {
			if (*p != 0)
				*p++ = 0;
			for (pw = pwhat, nw = nwhat;  nw != 0;  pw++, nw--) {
				if (strcmp(list, *pw) == 0)
					return (1);
			}
			list = p;
		}
	}
	return (0);
}

/*
 * Generate a key for a netgroup.byXXXX NIS map
 */
static void
makekey(key, name, domain)
	char		*key;
	const char	*name;
	const char	*domain;
{
	while (*key++ = *name++)
		;
	*(key-1) = '.';
	while (*key++ = *domain++)
		;
}

static int
makekey_lc(key, name, domain)
	char		*key;
	const char	*name;		/* Convert this to lowercase */
	const char	*domain;	/* But not this */
{
	int		found_uc = 0;
	char		c;

	while (c = *name++) {
		if (isupper(c)) {
			++found_uc;
			c = tolower(c);
		}
		*key++ = c;
	}
	*key++ = '.';
	while (*key++ = *domain++)
		;
	return (found_uc);
}

/*
 * easy_way() --  try to use netgroup.byuser and netgroup.byhost maps to
 *		  get answers more efficiently than by recursive search.
 *
 * If more than one name (username or hostname) is specified, this approach
 * becomes less attractive;  at some point it's probably cheaper to do the
 * recursive search.  We don't know what the threshold is (among other things
 * it may depend on the site-specific struucture of netgroup information),
 * so here's a guesstimate.
 */

#define	NNAME_THRESHOLD	5

static int
easy_way(be, ia, argp, map, try_lc, statusp)
	struct nis_netgr_be	*be;
	struct nss_innetgr_args	*ia;
	struct nss_innetgr_1arg	*argp;
	const char		*map;
	int			try_lc;
	nss_status_t		*statusp;
{
	nss_innetgr_argc	nname = argp->argc;
	nss_innetgr_argv	pname = argp->argv;
	const char		*domain = ia->arg[NSS_NETGR_DOMAIN].argv[0];
	const char		*wild = "*";
	int			yperr;
	char			*val;
	int			vallen;
	char			*key;
	int			i;

	/* Our caller guaranteed that nname >= 1 */
	while (nname > 1) {
		struct nss_innetgr_1arg	just_one;

		if (nname > NNAME_THRESHOLD) {
			return (0);	/* May be cheaper to use 'netgroup' */
		}

		just_one.argc = 1;
		just_one.argv = pname;

		if (easy_way(be, ia, &just_one, map, try_lc, statusp) &&
		    ia->status == NSS_NETGR_FOUND) {
			return (1);
		}
		++pname;
		--nname;
		/* Fall through and do the last one inline */
	}

	if ((key = malloc(strlen(*pname) + strlen(domain) + 2)) == 0) {
		return (0);	/* Or maybe (1) and NSS_UNAVAIL */
	}

	for (i = 0;  i < (try_lc ? 6 : 4);  i++) {
		switch (i) {
		    case 0:
			makekey(key, *pname, domain);
			break;
		    case 1:
			makekey(key, wild, domain);
			break;
		    case 2:
			makekey(key, *pname, wild);
			break;
		    case 3:
			makekey(key, wild, wild);
			break;
		    case 4:
			if (!makekey_lc(key, *pname, domain)) {
				try_lc = 0;	/* Sleazy but effective */
				continue;	/*   i.e. quit looping  */
			}
			break;
		    case 5:
			(void) makekey_lc(key, *pname, wild);
			break;
		}
		*statusp = _nss_nis_ypmatch(be->domain, map, key,
					&val, &vallen, &yperr);
		if (*statusp == NSS_SUCCESS) {
			if (inlist(ia->groups.argc, ia->groups.argv, val)) {
				free(val);
				free(key);
				ia->status = NSS_NETGR_FOUND;
				return (1);
			} else {
				free(val);
			}
		} else {
#ifdef DEBUG
			syslog(LOG_WARNING,
				"innetgr: yp_match(%s,%s) failed: %s",
				map, key, yperr_string(yperr));
#endif
			if (yperr != YPERR_KEY)  {
				free(key);
				return (0);
			}
		}
	}

	free(key);

/* =====> is this (an authoritative "no") always the right thing to do?	*/
/*	  Answer:  yes, except for hostnames that aren't all lowercase	*/

	*statusp = NSS_SUCCESS;		/* Yup, three different flavours of */
	ia->status = NSS_NETGR_NO;	/*   status information, so-called. */
	return (1);			/*   Silly, innit?		    */
}


static nss_status_t
netgr_in(be, a)
	struct nis_netgr_be	*be;
	void			*a;
{
	struct nss_innetgr_args	*ia = (struct nss_innetgr_args *) a;
	nss_status_t		res;

	ia->status = NSS_NETGR_NO;

	/* Can we use netgroup.byhost or netgroup.byuser to speed things up? */

/* ====> diddle this to try fast path for domains.argc == 0 too */
	if (ia->arg[NSS_NETGR_DOMAIN].argc == 1) {
		if (ia->arg[NSS_NETGR_MACHINE].argc == 0 &&
		    ia->arg[NSS_NETGR_USER   ].argc != 0) {
			if (easy_way(be, ia, &ia->arg[NSS_NETGR_USER],
			    "netgroup.byuser", 0, &res)) {
				return (res);
			}
		} else if (ia->arg[NSS_NETGR_USER].argc == 0 &&
		    ia->arg[NSS_NETGR_MACHINE].argc != 0) {
			if (easy_way(be, ia, &ia->arg[NSS_NETGR_MACHINE],
			    "netgroup.byhost", 1, &res)) {
				return (res);
			}
		}
	}

	/* Nope, try the slow way */
	ia->status = NSS_NETGR_NO;
	res = top_down(be, (const char **)ia->groups.argv, ia->groups.argc,
	    match_triple, ia);
	return (res);
}


/*
 * (Almost) boilerplate for a switch backend
 */

/*ARGSUSED*/
nss_status_t
netgr_destr(be, dummy)
	struct nis_netgr_be	*be;
	void			*dummy;
{
	if (be != 0) {
		free(be);
	}
	return (NSS_SUCCESS);
}

static nis_netgr_op_t netgroup_ops[] = {
	netgr_destr,
	0,		/* No endent, because no setent/getent */
	0,		/* No setent;  setnetgrent() is really a getXbyY() */
	0,		/* No getent in the normal sense */

	netgr_in,	/* innetgr() */
	netgr_set,	/* setnetgrent() */
};

/*ARGSUSED*/
nss_backend_t *
_nss_nis_netgroup_constr(dummy1, dummy2, dummy3)
	const char	*dummy1, *dummy2, *dummy3;
{
	const char		*domain;
	struct nis_netgr_be	*be;

	if ((domain = _nss_nis_domain()) == 0 ||
	    (be = (struct nis_netgr_be *) malloc(sizeof (*be))) == 0) {
		return (0);
	}
	be->ops		= netgroup_ops;
	be->n_ops	= sizeof (netgroup_ops) / sizeof (netgroup_ops[0]);
	be->domain	= domain;

	return ((nss_backend_t *) be);
}