/*
 * Copyright (c) 1994 - 1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)sun_compat.c	1.11	98/01/27 SMI"

#ifndef lint
static char sccsid[] = "@(#)sun_compat.c	1.11 (Sun) 01/27/98";
#endif /* not lint */

#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include "sendmail.h"
#include <sys/file.h>
#include <string.h>
#include <fcntl.h>
#include <ndbm.h>
#include <nsswitch.h>
#include <rpcsvc/ypclnt.h>
#ifdef INTER
#include <locale.h>
#endif

#undef NIS	/* symbol conflict in nis.h */
#undef T_UNSPEC	/* symbol conflict in nis.h -> ... -> sys/tiuser.h */
#include <rpcsvc/nis.h>
#include <rpcsvc/nislib.h>
#include <netdb.h>

#include <sys/mnttab.h> /* sysv */
#include <sys/mntent.h>
#include <string.h>

#define DOT_TERMINATED(x)	((x)[strlen(x) - 1] == '.')
#define DEF_ACTION		{1, 0, 0, 0}     /* name service switch data */
#define EN_len			zo_data.objdata_u.en_data.en_cols.en_cols_len
#define EN_col(col)		zo_data.objdata_u.en_data.en_cols.en_cols_val[(col)].ec_value.ec_value_val

extern int getdomainname();

#ifdef REMOTE_MODE
void verify_mail_server();
#endif

void
init_md_sun()
{
        struct stat sbuf;

        /* Check for large file descriptor */
        if (fstat(fileno(stdin), &sbuf) < 0)
        {
                if (errno == EOVERFLOW)
                {
                        perror("stdin");
                        exit(EX_NOINPUT);
                }
        }
#ifdef INTER
	setlocale(LC_ALL, "");
#endif
#ifdef V1SUN_COMPAT
	ConfigLevel = 1; /* default config level is 1 */
#endif
}


#ifdef SUN_INIT_DOMAIN
/* this is mainly for backward compatibility in Sun environment */
char *
sun_init_domain()
{
	/*
	 * Get the domain name from the kernel.
	 * If it does not start with a leading dot, then remove
	 * the first component.  Since leading dots are funny Unix
	 * files, we treat a leading "+" the same as a leading dot.
	 * Finally, force there to be at least one dot in the domain name
	 * (i.e. top-level domains are not allowed, like "com", must be
	 * something like "sun.com").
	 */
        char buf[MAXNAME];
        char *period, *autodomain;
        
        if (getdomainname(buf, sizeof buf) < 0)
                return NULL;

	if (tTd(0, 20))
		printf("domainname = %s\n", buf);

        if (buf[0] == '+')
                buf[0] = '.';
        period = strchr(buf, '.');
        if (period == NULL)
                autodomain = buf;
        else
		autodomain = period+1;
        if (strchr(autodomain, '.') == NULL)
                return newstr(buf);
	else
		return newstr(autodomain);
}
#endif /* SUN_INIT_DOMAIN */

#ifdef SUN_DEFAULT_VALUES
void
fix_macros(e)
	register ENVELOPE *e;
{
	char sbuf[MAXHOSTNAMELEN] = "";
	char *p;
	struct  utsname utsname;

	(void) strcpy(sbuf, long_host_name);
	if (p = strchr(sbuf, '.'))
		*p = '\0';
	
	/* fix $w: level 5 (and all Sun) configs have short name in $w */
	p = macvalue('w', e);
	if (p != NULL && (p = strchr(p, '.')) != NULL)
		*p = '\0';
	define('w', macvalue('w', e), e);

	/* fix $j */
	if (VENDORVER(VENDOR_SUN, == 1))
		/* default $j is a short host name in v1/sun */
		define('j', newstr(sbuf), e);
	else
	 	define('j', newstr(long_host_name), e);
	
#ifdef REMOTE_MODE
	/* fix $k */
	if (VENDORVER(VENDOR_SUN, == 1))
	{
		/* in V1/sun $k is mailbox server, not uucp node name */
		if (RemoteMode)
			verify_mail_server(sbuf, &RemoteMode);
		if (RemoteMode)
			define('k', newstr(RemoteMboxHost), e);
		else
			define('k', newstr(sbuf), e);
	}
	else
	{
		if (uname(&utsname) >= 0)
                        p = utsname.nodename;
                else
                {
                        if (tTd(0, 22))
                                printf("uname failed (%s)\n", errstring(errno));
                        makelower(long_host_name);
                        p = long_host_name;
                }
                if (tTd(0, 4))
                        printf("UUCP nodename: %s\n", p);
                p = newstr(p);
                define('k', p, CurEnv);
	}

	/* fix ${ms} */
	if (VENDORVER(VENDOR_SUN, > 1))
	{
		int mailserv;

		/* in V6/sun ${mailserver} needs to be initailized */
		if (RemoteMode)
			verify_mail_server(sbuf, &RemoteMode);
		mailserv = macid("{ms}", NULL);
		if (RemoteMode)
			define(mailserv, newstr(RemoteMboxHost), e);
		else
			define(mailserv, newstr(sbuf), e);
	}
#endif /* REMOTE_MODE */
}

#ifdef V1SUN_COMPAT
void
fix_options()
{
	MaxHopCount = 30; 		/* max hop count = 30 */
	MaxMciCache = 0; 		/* no cacheing */
	TimeOuts.to_ident = (time_t) 0; /* turn off "ident" proto */
	UseErrorsTo = TRUE;		/* turn on "error-to" processing */
	SafeAlias = 5 * 60; 		/* wait for @:@ */
	NoConnect = TRUE;		/* avoid connecting to "expensive" */
					/* mailers on initial submission   */
	/**************************************************************/
	/* Note also: the 'g' mailer flag is auto on : see usersmtp.c */
	/**************************************************************/
}
#endif /* V1SUN_COMPAT */

static void
fix_mailers()
{
	char buf[100];
	STAB *s;
	struct mailer *m;
#if defined(V1SUN_COMPAT) && defined(NO_CHK_FOR_BANG)
	struct mailer **mp;

	/* turn off M_UGLYUUCP flag so we don't check for ! in putfromline() */
	for (mp = Mailer; (m = *mp++) != NULL; )
		clrbitn(M_UGLYUUCP, m->m_flags);
#endif
	/*
	 * Since sendmail no longer computes the Content-Length: header,
	 * we have extended mail.local with a -F option to handle the file
	 * mailer function.  mail.local already knows how to compute the
	 * Content-Length: header.
	 */

	(void)strcpy(buf, "*file*, P=/usr/lib/mail.local, F=lsDFMPESouqn9, T=DNS/RFC822/X-Unix, A=mail.local -F \201u");
        makemailer(buf);

	/*
	 * Turn on the ProgMailer flag for the Content-Length: header.
	 */

	s = stab("prog", ST_MAILER, ST_FIND);
	if ((s != NULL) && ((m = s->s_mailer) != NULL) && 
			((m->m_mailer) != NULL))
		setbitn(M_CONTENT_LEN, m->m_flags);
}

void
show_sun_macros()
{
#ifdef REMOTE_MODE
	if (VENDORVER(VENDOR_SUN, == 1) && RemoteMode)
	{
		printf("\n            (mail server) $k = ");
		xputs(macvalue('k', CurEnv));
	}
	if (VENDORVER(VENDOR_SUN, > 1) && RemoteMode)
	{
		printf("\n             (mailserver) ${ms} = ");
		xputs(macvalue(macid("{ms}", NULL), CurEnv));
	}
#endif
}

void
sun_pre_defaults(e)
ENVELOPE *e;
{
	fix_macros(e);
	fix_options();
}

void
sun_post_defaults(e)
ENVELOPE *e;
{
	if (VENDORVER(VENDOR_SUN, >= 0))
		fix_mailers();
#ifdef V1SUN_COMPAT
	if (VENDORVER(VENDOR_SUN, == 1))
		MaxAliasRecursion = 50;
#endif
}
#endif /* SUN_DEFAULT_VALUES */

#ifdef REMOTE_MODE
static size_t preflen();

void
RemoteDefault()
{
#define mntent mnttab
#define mnt_fsname mnt_special
#define mnt_dir mnt_mountp
#define mnt_type mnt_fstype
#define mnt_opts mnt_mntopts

	/*
	 * Search through mtab to see which server /var/mail
	 * is mounted from.  Called when remote mode is set, but no
	 * server is specified.  Deliver locally if mailbox is local.
	 */
	FILE *mfp;
	struct mntent *mnt;
	struct mnttab sysvmnt;
	char *endhost;			/* points past the colon in name */
	size_t bestlen = 0;
	size_t len;
	int rl_res;
	char mailboxdir[256];		/* resolved symbolic link */
	static char bestname[256];	/* where the name ends up */
	char linkname[256];		/* for symbolic link chasing */
	struct stat sb;

	(void) strcpy(mailboxdir, "/var/mail");
	for (;;)
	{
		if ((rl_res = readlink(mailboxdir, linkname,
					sizeof (linkname))) < 0)
			break;
		len = (size_t)rl_res;
		linkname[len] = '\0';
		(void) strcpy(mailboxdir, linkname);
	}

	/*
	 * In case /var/mail is automounted, force a mount before we check
	 * the mount table.
	 */
	(void) stat("/var/mail/:saved", &sb);
	mfp = fopen(MNTTAB, "r");

	if (mfp == NULL)
	{
		printf("Unable to open mount table\n");
		return; /* (EX_OSERR) */
	}
	(void) strcpy(bestname, "");

	mnt = &sysvmnt;
	while ((getmntent(mfp, mnt)) >= 0)
	{
		len = preflen(mailboxdir, mnt->mnt_dir);
 		if ((len != 0) && (len >= bestlen) &&
		    (strcmp(mnt->mnt_type, "nfs") == 0) )
		{
			bestlen = len;
			(void) strncpy(bestname, mnt->mnt_fsname,
					sizeof bestname);
		}
	}
	(void) fclose(mfp);
	endhost = strchr(bestname, ':');
	if (endhost && bestlen > 4)
	{
		/*
		 * We found a remote mount-point for /var/spool/mail --
		 * save the host name.  The test against "4" is because we
		 * don't want to be fooled by mounting "/" or "/var" only.
		 */
		RemoteMboxHost = bestname;
		*endhost = 0;
		return; /* (EX_OK) */
	}
	/*
	 * No remote mounts - assume local
	 */
	RemoteMboxHost = NULL;
}

/*
 * Returns: length of second argument if it is a prefix of the
 * first argument, otherwise zero.
 */

size_t
preflen(str, pref)
	char *str, *pref;
{
        size_t len;

        len = strlen(pref);
        if (strncmp(str, pref, len) == 0)
                return (len);
        return (0);
}

void
verify_mail_server(myhostname, remote)
	char *myhostname;
	bool *remote;
{
	/* if there are no mail server or mail_server is myself */
	/* then turn off RemoteMode				 */
	if (!RemoteMboxHost || RemoteMboxHost[0] == '\0' ||
	    (strcasecmp(myhostname, RemoteMboxHost) == 0))
		*remote = FALSE;
	if (tTd(81, 1))
	{
		printf("verify_mail_server(): remote mode is %s\n",
			*remote ? "on" : "off");
		if (*remote)
			printf("mail server = %s\n", RemoteMboxHost);
	}
}

/*
 * This function returns 1 if "name" is a local alias
 */

is_implied_local_alias(name)
	char *name;
{
	static STAB *aliasmap;
	static char *argvect[2] = {"-D", NULL};
	struct address a;
	ENVELOPE dummy_env;
	int stat;
	char *str;

	(void) memset((char *) &dummy_env, 0, sizeof dummy_env);
	clearenvelope(&dummy_env, FALSE);

	/* check to see if it is a local name */
	if (parseaddr(name, &a, RF_COPYNONE, 0, '\0' , &dummy_env) == NULL) 
		return -1; /* unknown */

	if (bitset(QQUEUEUP, a.q_flags))
		return -1; /* unknown */

	if (a.q_mailer != LocalMailer)	
		return 0; /* not a local alias */

	/* it is a local name , now we check if it is already qualified */
	if (strcasecmp(a.q_user, a.q_paddr))
		return 0; /* not a local alias */
	
	/* it is a non-qualified local name     */
	/*  now we check if it is a local alias */
	if (aliasmap == NULL)
		aliasmap = stab("aliases", ST_MAP, ST_FIND);
	if (aliasmap == NULL)
		return -1; /* unknown */
	str = (*aliasmap->s_map.map_class->map_lookup)(&aliasmap->s_map,
						a.q_user, argvect, &stat);
	if (str == NULL)
		return -1; /* unknown */

	if (*str == 'U')
		return -1; /* unknown */

	if (*str == 'L')
		return 1;  /* is local alias */

	return 0; /* not a local alias */
}

/* make name explicit local by tagging on $w */
char *
make_explicit_local(name)
	char	*name;
{
	static char buf[MAXNAME];

	if (strlen(MyHostName) + strlen(name) + 1 >= sizeof buf)
	{
		syserr("name buffer overflow");
		return(name);
	}

	/* tag on MyhostName */
	(void) snprintf(buf, sizeof buf, "%s@%s", name, MyHostName);
	return buf;
}
#endif /* REMOTE_MODE */

#ifdef V1SUN_COMPAT
# ifdef PERCENT_MATCH

void lookup();

static void do_match();
static void name_service_match();
static void fix_mapname();

do_percent_match(avpp, tokbuf, rp)
	char ***avpp;
	char *tokbuf;
	char *rp;
{
	char **oldavp;
	register char **avp;
	int free;

	/* match any token in (not in) a NIS map. */
	oldavp = avp = *avpp;
	free = MAXNAME;
	tokbuf[0] = '\0';
	if ((rp[1] == 'y') ||	/* hosts map */
	    (rp[1] == 'x') ||	/* mx record  */
	    (rp[1] == 'l') )	/* local hosts list */
	{
		/* construct max qualify name   */
		/* string is expected to be 	*/
		/* terminated by:		*/
		/*  ".LOCAL" ".uucp"  or ">"	*/
		/* e.g 				*/
		/* "user@host.sub1.sub2.LOCAL"	*/
		/* "user@host.sub1.sub2.uucp"	*/
		/* or  "user<@host.sub1.sub2>" 	*/
		while (*avp != NULL)
		{
			if (!strcmp(avp[0], ">"))
				break;
			if (!strcmp(avp[0], ".") &&
			    (!strcmp(avp[1], "LOCAL") ||
			     !strcmp(avp[1], "uucp")))
				break;
			if ((free -= strlen(*avp)) >= 0)
				(void) strcat(tokbuf, *avp++);
			else
				syserr("554 do_percent_match: name too long");
		} 
	}
	else
	{ /* other map */
		/* just get the first token */
		if (*avp != NULL)
			if ((free -= strlen(*avp)) >= 0)
				(void) strcat(tokbuf, *avp++);
			else
				syserr("554 do_percent_match: name too long");
	}

	if (tokbuf[0] != '\0')
	{
		char *end;
		int stat;

		end = &tokbuf[strlen(tokbuf)];
		if (tTd(38, 13))
			printf( "trying do_match(%s, %c)...\n", tokbuf, rp[1]);

		stat = EX_OK;
		do_match(tokbuf, rp[1], &stat);
		while (stat == EX_NOTFOUND)
		{
			if  (avp != oldavp)
			{
				/* strip one token */
				end =  end - strlen(*(--avp));
				*end = '\0';
			}
			if ((avp != oldavp) &&
		    	!strcmp(avp[-1], "."))
			{
		    	/* if we end up with a  */
		    	/* trailing dot, strip  */
		    	/* one more token	    */
				end =  end - strlen(*(--avp));
				*end = '\0';
			}
			if (avp == oldavp)
				break;

			if (tTd(38, 13))
				printf("trying do_match(%s, %c)...\n",
				 	tokbuf, rp[1]);
	    		do_match(tokbuf, rp[1], &stat);
		}
		if ((stat != EX_OK) && (stat != EX_NOTFOUND))
			return -1; /* TEMPFAIL */
	}

	if (avp == oldavp)  /* no match */
	{
		if (tTd(38, 13))
			printf("do_match(): no match found\n");
		if ((*rp  & 0377) == MATCHPERCENT)
		{
			*avpp = avp;
			return  0; /*backup*/
		}
		else
		{
			/* just eat up one token */
			/* for MATCHNPERCENT case */
			avp++;
		}
	}
	else
	{ /* got match */
		if (tTd(38, 13))
			printf("do_match(%s, %c) match found\n", tokbuf, rp[1]);
		if ((*rp & 0377) == MATCHNPERCENT)
		{
			avp = oldavp;
			*avpp = avp;
			return  0; /*backup*/
		}
	}
	*avpp = avp;
	return 1;
}


/*
 * lookup a string in a name service 
 */

void
do_match(string, mac, statp)
    char *string;
    char mac;
    int *statp;
{

	*statp = EX_OK;
	if (mac == 'x') /* mx lookup */
	{
		int rcode;
		char *mxhosts[MAXMXHOSTS + 1];
		int nmx;

		rcode = EX_OK;
		nmx = getmxrr(string, mxhosts, TRUE, &rcode);
		if ((nmx <= 0) || (rcode != EX_OK))
			*statp = EX_NOTFOUND;
		return;
	}
	name_service_match(string, mac, statp);
}

/*
**  NAME_SERVICE_MATCH --look up any token in the Network Information Services.
**			or nis+
**
**	Parameters:
**		string - string to look up
**		mac - macro to use as name of map
**
**	Returns:
**		True if the value was found in the database
**
**	Side Effects:
**		Note the careful copying of the string, converting it
**		to lower case before doing the NIS lookup
**
*/

void
name_service_match(string, mac, statp)
	char *string;
	char mac;
	int *statp;
{
	char *mapname;
	char namebuf[MAXNAME];
	char *p = namebuf;

	*statp = EX_OK;
	(void) strncpy(namebuf, string, sizeof namebuf);
	p += strlen(p);
	makelower(namebuf);

	if ((mac == 'y') || (mac == 'l')) 
		mapname = "hosts.byname";
	else
		mapname = macvalue(mac, CurEnv);

	if (mapname == NULL)
	{
		syserr("name_service_match: Undefined mapname %%%c (0x%x)",
			mac, mac);
		return;
	}

	if (!strcmp("hosts.byname", mapname))
	{
		int len_m, len_n;
		char *m;
		extern h_errno;

		/* if you change this block of code */
		/* make sure it work for the address*/
		/* with abbreviated domain name:    */
		/* e.g awc@estelle.eng		    */
		m = macvalue('m', CurEnv);
		if (mac == 'l')
		{
			if (!DOT_TERMINATED(namebuf))
			{
				(void) snprintf(p, SPACELEFT(namebuf, p), ".");
				p += strlen(p);
			}
			(void) snprintf(p, SPACELEFT(namebuf, p), m);
			p += strlen(p);
		}

		h_errno = 0;
	 	if (getcanonname(namebuf, MAXNAME-1, TRUE))
			*statp = EX_OK;
		else
		{
			if (h_errno == 0)
				*statp = EX_OK;
			else if (h_errno == HOST_NOT_FOUND)
				*statp = EX_NOTFOUND;
			else
				*statp = EX_TEMPFAIL;
		}
			
		if (mac != 'l')
			return;

		len_m = strlen(m);
			len_n = strlen(namebuf);

		if (len_m >= len_n)
		{
			*statp = EX_NOTFOUND;
			return;
		}

		/* if namebuf end with $m subfix => local */
		if (!strcasecmp(m, &namebuf[len_n -len_m]))
			return;

		*statp = EX_NOTFOUND;
		return;
	}

	lookup(mapname, namebuf, NULL, 0, statp);
}

static void
yp_lookup(mapname, keybuf, ansbuf, ansbuflen, statp)
	char *mapname;
	char *keybuf;
	char *ansbuf;
	size_t ansbuflen;
	int *statp;
{
	char domain[MAXNAME];
	int keylen;
	char *vp;
	int vsize;
	int yperr;

	if (tTd(38, 13))
		printf("yp_lookup(%s, %s)\n", mapname, keybuf);

	getdomainname(domain, sizeof domain); 
	keylen = strlen(keybuf);
	yperr = yp_match(domain, mapname, keybuf, keylen, &vp, &vsize);
	if (yperr == YPERR_KEY)
	{
		keylen++;
		yperr = yp_match(domain, mapname, keybuf, keylen, &vp, &vsize);
	}
	if (yperr == 0)
	{
		if (ansbuf)
		{
			size_t anslen = (ansbuflen < vsize) ? ansbuflen : vsize;

			(void) snprintf(ansbuf, anslen, "%s", vp);
			ansbuf[anslen] = '\0';
		}
		*statp = EX_OK;
		return;
	}
	if (yperr == YPERR_KEY)
		*statp = EX_NOTFOUND;
	else
		*statp = EX_TEMPFAIL;
}

static void
nisplus_lookup(mapname, keybuf, ansbuf, ansbuflen, statp)
	char *mapname;
	char *keybuf;
	char *ansbuf;
	size_t ansbuflen;
	int *statp;
{
	char qbuf[MAXLINE + NIS_MAXNAMELEN];
	char *nisplus_mapname;
	char *key_column;
	int value_column_idx;
	char *vp;
        nis_result *result;
	extern char *nisplus_default_domain();

	value_column_idx = 1;
	fix_mapname(mapname, &nisplus_mapname, &key_column, &value_column_idx);

        /* construct the query */
        if (!DOT_TERMINATED(nisplus_mapname))
		(void) snprintf(qbuf, sizeof qbuf, "[%s=%s],%s.%s", key_column,
			keybuf, nisplus_mapname, nisplus_default_domain());
        else
		(void) snprintf(qbuf, sizeof qbuf, "[%s=%s],%s", key_column,
			keybuf, nisplus_mapname);

	if (tTd(38, 20)) 
		printf("nisplus_lookup(), qbuf=%s\n", qbuf); 
	
	result = nis_list(qbuf, FOLLOW_LINKS | FOLLOW_PATH, NULL, NULL); 
        if (result->status == NIS_SUCCESS)
	{
                int count;

        	if ((count = NIS_RES_NUMOBJ(result)) != 1)
		{
			if (LogLevel > 10)
                        syslog(LOG_WARNING,
                        	"%s: Lookup error, expected 1 entry, got (%d)",
                        	mapname, count);
			/* ignore second entry */
			if (tTd(38, 20))
				printf("nisplus_lookup(%s), got %d entries, addtional entries ignores\n",
					keybuf, count);
                } 

                vp = ((NIS_RES_OBJECT(result))->EN_col(value_column_idx));
		(void) snprintf(ansbuf, ansbuflen, "%s", vp);
		*statp = EX_OK;
		return;
	}
        if (result->status == NIS_NOTFOUND)
		*statp = EX_NOTFOUND;
	else
		*statp = EX_TEMPFAIL;

	if (tTd(38, 13))
		printf("nisplus_lookup(%s, %s) failed, error code = %d\n",
			 mapname, keybuf, result->status);
	return;
}
# endif /* PERCENT_MATCH */

# if defined(SUNLOOKUP) || defined(PERCENT_MATCH)
void
lookup(mapname, keybuf, ansbuf, ansbuflen, statp)
	char *mapname;
	char *keybuf;
	char *ansbuf;
	int *statp;
	size_t ansbuflen;
{
        struct __nsw_switchconfig *nsw_conf;
        enum __nsw_parse_err pserr;
        struct __nsw_lookup *lk;
	char tmpbuf[MAXNAME + 1];
	static struct __nsw_lookup lkp2 = { "nis", DEF_ACTION, NULL, NULL};
	static struct __nsw_lookup lkp1 = { "nisplus", DEF_ACTION, NULL, &lkp2};
	static struct __nsw_switchconfig lkp_default = { 0, "aliases" , 2, &lkp1};

	if (tTd(38, 20))
		printf("lookup(%s, %s)\n", mapname, keybuf);

	(void) strcpy(tmpbuf, keybuf);
	makelower(tmpbuf);

	/* this is really a hack, we are using the aliases entry in */
	/* /etc/nsswitch.conf, to guess wheather the user want yp   */
	/* or nisplus                                               */
        if ((nsw_conf = __nsw_getconfig("aliases", &pserr)) == NULL)
                nsw_conf = &lkp_default;
        lk = nsw_conf->lookups;

        while (lk)
	{
                if (!strcmp("nis", lk->service_name))
		{
			yp_lookup(mapname, tmpbuf, ansbuf, ansbuflen, statp);
			return;
		}
                if (!strcmp("nisplus", lk->service_name))
		{
			nisplus_lookup(mapname, tmpbuf, ansbuf, ansbuflen,
					statp);
			return;
		}
                lk = lk->next;
        }
}

/*
 * Map old yp map name to nisplus table name to ease the
 * transition pain of going from yp to nisplus
 */

void
fix_mapname(mapname, nisplus_mapname, key_column_name, value_column_idx)
	char *mapname;
	char **nisplus_mapname;
	char **key_column_name;
	int  *value_column_idx;
{
	if (!strcmp("mail.byaddr", mapname) ||
	    !strcmp("REVERSE.mail_aliases.org_dir", mapname))
	{
		*nisplus_mapname = "mail_aliases.org_dir";
		*key_column_name = "expansion";
		*value_column_idx = 0;
		return;
	}
	if (!strcmp("hosts.byaddr", mapname))
	{
		*nisplus_mapname = "hosts.org_dir";
		*key_column_name = "addr";
		*value_column_idx = 0;
		return;
	}
	if (!strcmp("mail.byname", mapname))
	{
		*nisplus_mapname = "mail_aliases.org_dir";
		*key_column_name = "alias";
		*value_column_idx = 1;
		return;
	}
	/* *IMPORTANT* all other user table */
	/* should be a "key"/"vlaue" table  */
	/* "key" column must be column zero */
	*nisplus_mapname = mapname;
	*key_column_name = "key";
	*value_column_idx = 1;
}
# endif  /* SUNLOOKUP || PERCENT_MATCH */
#endif /* V1SUN_COMPAT */

#ifdef SUN_LOOKUP_MACRO
static void files_TableLookUp();
static void nisplus_TableLookUp();

static char *
next_word(buf, word)
	char buf[];
	char **word;
{
	register char *p;
	register char *wd;
	char last_char;

	p = buf;
	while (*p != '\0' && isspace(*p))
		p++;
	wd = p;
	while (*p != '\0' && !isspace(*p))
		p++;
	last_char = *p;
	*p = '\0';
	*word = wd;
	if (last_char)
	{
		p++;
		while (*p != '\0' && isspace(*p))
			p++;
	}
	return (p);
}

/*
**  LOOKUP_DATA - lookup value of macro/class
**
*/

#ifndef NSW_SENDMAILVARS_DB
/* this define should be in /usr/include/nsswitch.h */
#define	NSW_SENDMAILVARS_DB "sendmailvars"
#endif
#define	LOOKUP_TIMEOUT	/* we need this only if the time out code in the */
			/* name service e.g nis+ does not work, this can */
			/* be undef once, nis+ fast fail is working	 */
#ifdef LOOKUP_TIMEOUT
static jmp_buf CtxLookUpTimeOut;
static int LookUpTimeOut = 60; /* 60 seconds */

static void
lookuptimeout()
{
	longjmp(CtxLookUpTimeOut, 1);
}
#endif

static struct __nsw_lookup lkp1 = { "nisplus", DEF_ACTION, NULL, NULL};
static struct __nsw_lookup lkp0 = { "files", DEF_ACTION, NULL, &lkp1};
static struct __nsw_switchconfig mailconf_default =
				    { 0, NSW_SENDMAILVARS_DB, 1, &lkp0};
static struct __nsw_lookup *mailconf_nsw = NULL;

static
lookup_data(search_key, answer_buf, bufsize)
	char search_key[];
	char answer_buf[];
	int  bufsize;
{
	int nserr;
	enum __nsw_parse_err pserr;
	struct __nsw_switchconfig *nsw_conf = NULL;
	struct __nsw_lookup *lk;

	if (!mailconf_nsw)
	{
		if (!(nsw_conf = __nsw_getconfig(NSW_SENDMAILVARS_DB, &pserr)))
			nsw_conf = &mailconf_default;
		mailconf_nsw = nsw_conf->lookups;
	}

	for (lk = mailconf_nsw; lk; lk = lk->next)
	{
#ifdef LOOKUP_TIMEOUT
		EVENT *ev;
		ev = setevent(LookUpTimeOut, lookuptimeout, 0);
		if (setjmp(CtxLookUpTimeOut) != 0)
		{
			nserr = __NSW_UNAVAIL;
			answer_buf[0] = '\0';
#ifdef notdef
			/* XXX fail siliently for now */
			syslog(LOG_CRIT,
		"can't Lookup \"%s\", no response from name service \"%s\"",
			    search_key, lk->service_name);
#endif
			if (__NSW_ACTION(lk, nserr) == __NSW_RETURN)
				return (nserr);
			continue;
		}
#endif
		nserr = __NSW_UNAVAIL;
		if (strcmp(lk->service_name, "nisplus") == 0)
			nisplus_TableLookUp(&nserr, search_key,
				    answer_buf, &bufsize);
		else
		{
			if (strcmp(lk->service_name, "files") == 0)
				files_TableLookUp(&nserr, search_key,
				    answer_buf, &bufsize);
			else
				syslog(LOG_CRIT,
			    	    "can't Lookup data via name service \"%s\"",
					lk->service_name);
		}
#ifdef LOOKUP_TIMEOUT
		clrevent(ev);
#endif
		if (__NSW_ACTION(lk, nserr) == __NSW_RETURN)
			break;
	}
	return (nserr);
}

/*
 * Parse the 'L' and 'G' lines from the config file.
 */

void
sun_lg_config_line(bp, e)
	char *bp;
	ENVELOPE *e;
{
	char answer[MAXLINE];
	char *search_key;
	char *word;
        auto char *ep;
        int mid;
        register char *p;

	mid = macid(&bp[1], &ep);
	if (mid == -1)
		return;

	(void)next_word(ep, &search_key);
	if (lookup_data(search_key, answer, MAXLINE))
	{
		/*
		 * XXX fail silently for now
		 * syslog(LOG_CRIT, "can't Lookup \"%s\"", search_key);
		 */
		return;
	}

	if (bp[0] ==  'L')
		define(bp[1], newstr(answer), e);
	else
	{	/* i.e bp[0] == 'G' */
		p = next_word(answer, &word);
		while (*word)
		{
			setclass(bp[1], word);
			p = next_word(p, &word);
		}
	}
}

void
files_TableLookUp(nserrp, search_key, answer_buf, bufsizep)
	int *nserrp;
	char search_key[];
	char answer_buf[];
	int *bufsizep;
{
	FILE *cf;
	char buf[MAXLINE];

	if ((search_key == NULL) || (answer_buf == NULL) || (*bufsizep <= 0))
	{
		*nserrp = -1; /* bad param */
		return;
	}

	answer_buf[0] = '\0';
	cf = fopen(SENDMAIL_MAP_FILE, "r");
	if (cf == NULL)
	{
		*nserrp = __NSW_UNAVAIL;
		return;
	}

	*nserrp = __NSW_NOTFOUND;
	while (fgetfolded(buf, sizeof (buf), cf) != NULL)
	{
		char *p;
		char last_char;

		p = buf;
		if ((*p == '#') || (*p == ' ') || (*p == '\t'))
			continue; /* skip comment/blank line */
		/* find end of first field */
		while (*p)
		{
			if ((*p == ' ') || (*p == '\t'))
				break;
			p++;
		}

		last_char = *p;
		*p = '\0';
		if (!strcmp(buf, search_key))
		{
			*nserrp = __NSW_SUCCESS;
			if (last_char)
				p++;
			/* strip leading spaces */
			while ((*p == ' ') || (*p == '\t'))
				p++;
			answer_buf[*bufsizep - 1]  = '\0';
			(void) strncpy(answer_buf, p, *bufsizep - 1);
			*bufsizep = strlen(answer_buf) + 1;
			break;
		}
	}
	(void) fclose(cf);
}

void
nisplus_TableLookUp(nserrp, search_key, answer_buf, bufsizep)
	int *nserrp;
	char search_key[];
	char answer_buf[];
	int *bufsizep;
{

	char table_name[MAXNAME];
	int  column_id;

	nis_result *result;
	char qbuf[MAXLINE];

	if ((search_key == NULL) || (answer_buf == NULL) || (*bufsizep <= 0))
	{
		*nserrp = -1; /* bad param */
		return;
	}

	*nserrp = __NSW_UNAVAIL;

	(void) snprintf(table_name, sizeof table_name, "%s.%s",
			SENDMAIL_MAP_NISPLUS, nis_local_directory());
	column_id = 1;

	/* construct the query */
	(void) snprintf(qbuf, sizeof qbuf, "[%s=%s],%s", "key", search_key,
		table_name);

	result = nis_list(qbuf, FOLLOW_LINKS | FOLLOW_PATH, NULL, NULL);
	if (result->status == NIS_SUCCESS)
	{
		int count;

		if ((count = (result->objects).objects_len) != 1)
			syslog(LOG_CRIT,
			    "Lookup error, expected 1 entry, got (%d)", count);
		else
		{
			if (NIS_RES_OBJECT(result)->EN_len <= column_id)
				syslog(LOG_CRIT,
				"Lookup error, no such column (%d)", column_id);
			else
			{
				*nserrp = __NSW_SUCCESS;
				answer_buf[*bufsizep - 1] = '\0';
				(void) strncpy(answer_buf,
				  ((NIS_RES_OBJECT(result))->EN_col(column_id)),
				  *bufsizep - 1);
				/* set the length of the result */
				*bufsizep = strlen(answer_buf) + 1;
			}
		}
	}
	else
		*nserrp = __NSW_NOTFOUND;
	nis_freeresult(result);
}
#endif /* SUN_LOOKUP_MACRO */
