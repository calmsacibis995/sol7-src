/*
 *	nfs_tbind.h
 *
 *	Copyright (c) 1996 Sun Microsystems Inc
 *	All Rights Reserved.
 */

#ifndef	_NFS_TBIND_H
#define	_NFS_TBIND_H

#pragma ident	"@(#)nfs_tbind.h	1.2	97/09/24 SMI"

#include <netconfig.h>
#include <netdir.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Globals which should be initialised by daemon main().
 */
extern  size_t  end_listen_fds;
extern  size_t  num_fds;
extern	int	num_servers;
extern	int	listen_backlog;
extern	int	(*Mysvc)(int nservers, int fd, struct netbuf,
			struct netconfig *);
extern  int     max_conns_allowed;

/*
 * RPC protocol block.  Useful for passing registration information.
 */
struct protob {
	char *serv;		/* ASCII service name, e.g. "NFS" */
	int versmin;		/* minimum version no. to be registered */
	int versmax;		/* maximum version no. to be registered */
	int program;		/* program no. to be registered */
	struct protob *next;	/* next entry on list */
};

/*
 * Declarations for protocol types and comparison.
 */
#define NETSELDECL(x)   char    *x
#define NETSELPDECL(x)  char    **x
#define NETSELEQ(x, y)  (strcmp((x), (y)) == 0)

/*
 * nfs library routines
 */
extern int	nfslib_transport_open(struct netconfig *);
extern int	nfslib_bindit(struct netconfig *, struct netbuf **,
			struct nd_hostserv *, int);
extern void	nfslib_log_tli_error(char *, int, struct netconfig *);
extern int	do_all(int, struct protob *, int (*)());
extern void	do_one(int, char *, char *, struct protob *,
			int (*)(int, int, struct netbuf, struct netconfig *));
extern void	poll_for_action(void);

#ifdef __cplusplus
}
#endif

#endif	/* _NFS_TBIND_H */
