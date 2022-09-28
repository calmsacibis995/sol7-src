/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)port_after.h 1.1     97/12/03 SMI"

#undef HAS_SA_LEN
#define USE_POSIX
#define POSIX_SIGNALS
#define NETREAD_BROKEN
#define USE_WAITPID
#define HAVE_FCHMOD
#define NEED_PSELECT
#define SETGRENT_VOID
#define SETPWENT_VOID
#define SIOCGIFCONF_ADDR
#define IP_OPT_BUF_SIZE 40

#undef _PATH_NAMED
#define _PATH_NAMED	"/usr/sbin/named"
#undef _PATH_XFER
#define _PATH_XFER	"/usr/sbin/named-xfer"
#undef _PATH_PIDFILE
#define _PATH_PIDFILE	"/etc/named.pid"

#define PORT_NONBLOCK	O_NONBLOCK
#define PORT_WOULDBLK	EWOULDBLOCK
#define WAIT_T		int
#define INADDR_NONE	0xffffffff

#ifndef MIN
# define MIN(x, y)	((x > y) ?y :x)
#endif
#ifndef MAX
# define MAX(x, y)	((x > y) ?x :y)
#endif

/*
 * We need to know the IPv6 address family number even on IPv4-only systems.
 * Note that this is NOT a protocol constant, and that if the system has its
 * own AF_INET6, different from ours below, all of BIND's libraries and
 * executables will need to be recompiled after the system <sys/socket.h>
 * has had this type added.  The type number below is correct on most BSD-
 * derived systems for which AF_INET6 is defined.
 */
#ifndef AF_INET6
#define AF_INET6	24
#endif

#include <sys/types.h>
/* Solaris 2.6 defines gethostname() in unistd.h, so we do not define it here
*/
/* extern int gethostname(char *, size_t); */

#define NEED_STRSEP
extern char *strsep(char **, const char *);

#define NEED_DAEMON
int daemon(int nochdir, int noclose);

/*
 * Solaris defines this in <netdb.h> instead of in <sys/param.h>.  We don't
 * define it in our <netdb.h>, so we define it here.
 */
#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 256
#endif
