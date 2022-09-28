/*
 * Copyright (c) 1993-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)svc_run_as.c	1.5	98/01/14 SMI"

/* Taken from 4.1.3 ypserv resolver code. */

/*
 * This is an example of using rpc_as.h an asynchronous polling
 * mechanism,  asynchronously polled fds are combined with the
 * service fds.  The minimum timeout is calculated, and
 * the user waits for that timeout or activity on either the
 * async fdset or the svc fdset.
 */

#include <rpc/rpc.h>
#include <sys/errno.h>
#include "rpc_as.h"
#include <stropts.h>
#include <poll.h>

extern int dtbsize;

void
svc_run_as()
{
	fd_set		readfds;
	extern int	errno;
	struct timeval  timeout;
	int		i;
	int		selsize;
	struct pollfd svc_pollset[FD_SETSIZE];
	int nfds = 0;
	int pollret = 0;
	int readyfds;

	memset(svc_pollset, 0, sizeof (svc_pollset));
	selsize = dtbsize;
	if (selsize > FD_SETSIZE)
		selsize = FD_SETSIZE;

	for (;;) {
		readfds = rpc_as_get_fdset();

		for (i = 0; i < howmany(FD_SETSIZE, NFDBITS); i++)
			readfds.fds_bits[i] |= svc_fdset.fds_bits[i];

		timeout = rpc_as_get_timeout();
	nfds = __rpc_select_to_poll(selsize, &readfds, svc_pollset);
	if (nfds ==  0)
		break; /* None waiting, hence return */
	switch ((pollret = poll(svc_pollset, nfds,
					__rpc_timeval_to_msec(&timeout)))) {
		case -1:
			if (errno == EINTR) {
				continue;
			}
			perror("svc_run: - poll failed: ");
			return;
		case 0:
			rpc_as_timeout(timeout);
			break;
		default:
			/*
			 * Before we send readfds to rpc_as_rcvreqset
			 * we need to update it based on what we got
			 * from poll().  Note that poll() only updates
			 * svc_pollset.  Thus, we visit each svc_pollset
			 * and set the fd in readfds for each revent != 0
			 */
			FD_ZERO(&readfds);
			readyfds = 0;
			for (i = 0; i < FD_SETSIZE &&
					readyfds < pollret; i++) {
				if (svc_pollset[i].revents != 0) {
					readyfds++;
					if (!FD_ISSET(svc_pollset[i].fd,
					    &readfds))
						FD_SET(svc_pollset[i].fd,
						    &readfds);
				}
			}

			rpc_as_rcvreqset(&readfds);

			/*
			 * Now that rpc_as_rcvreqset() serviced the fds
			 * from readfds, we need to clear these fds from
			 * svc_pollset.
			 */
			for (i = 0; i < FD_SETSIZE && pollret > 0; i++) {
				if (svc_pollset[i].revents != 0 &&
						!FD_ISSET(svc_pollset[i].fd,
						    &readfds)) {
					memset(&svc_pollset[i], 0,
							sizeof (pollfd_t));
					pollret--;
				}
			}

			svc_getreq_poll(svc_pollset, pollret);
		}
	}
}
