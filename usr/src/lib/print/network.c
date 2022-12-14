/*
 * Copyright (c) 1994, 1995, 1996 by Sun Microsystems, Inc.
 * All Rights Reserved
 */

#pragma ident "@(#)network.c	1.5	97/06/04 SMI"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <syslog.h>

#include <print/network.h>
#include <print/misc.h>


/*
 *	This module implements a set of functions to handle network
 *	communications.  It attempts to hide any "uglyness" that might be
 *	necessary for such communications.
 */


/*
 *  null() is to be used as a signal handler that does nothing.  It is used in
 *	place of SIG_IGN, because we want the signal to be delivered and
 *	interupt the current system call.
 */
static void
null(int i)
{
	syslog(LOG_DEBUG, "null(%d)", i);
}


/*
 *  net_open() opens a tcp connection to the printer port on the host specified
 *	in the arguments passed in.  If the connection is not made in the
 *	timeout (in seconds) passed in, an error it returned.  If the host is
 *	unknown, an error is returned.  If all is well, a file descriptor is
 *	returned to be used for future communications.
 */
int
net_open(char *host, int timeout)
{
	struct hostent *hp;
	struct servent *sp;
	struct sockaddr_in sin;
	static void (*old_handler)();

	int	s,
		lport = IPPORT_RESERVED - 1,
		err;
	unsigned timo = 1;

	syslog(LOG_DEBUG, "net_open(%s, %d)", (host != NULL ? host : "NULL"),
		timeout);
	/*
	 * Get the host address and port number to connect to.
	 */
	if (host == NULL) {
		return (-1);
	}
	memset((char *)&sin, NULL, sizeof (sin));
	if ((hp = gethostbyname(host)) == NULL) {
		syslog(LOG_DEBUG|LOG_ERR, "unknown host %s", host);
		return (NETWORK_ERROR_HOST);
	}
	memcpy((caddr_t)&sin.sin_addr, hp->h_addr, hp->h_length);
	sin.sin_family = hp->h_addrtype;

	if ((sp = getservbyname("printer", "tcp")) == NULL) {
		syslog(LOG_DEBUG|LOG_ERR, "printer/tcp: unknown service");
		return (NETWORK_ERROR_SERVICE);
	}
	sin.sin_port = sp->s_port;

	/*
	 * Try connecting to the server.
	 */
retry:
	s = rresvport(&lport);
	if (s < 0)
		return (NETWORK_ERROR_PORT);

	old_handler = signal(SIGALRM, null);
	alarm(timeout);
	if (connect(s, (struct sockaddr *)&sin, sizeof (sin)) < 0) {
		alarm(0);
		(void) signal(SIGALRM, old_handler);
		err = errno;
		(void) close(s);
		errno = err;
		if (errno == EADDRINUSE) {
			lport--;
			goto retry;
		}
		if (errno == ECONNREFUSED && timo <= 16) {
			sleep(timo);
			timo *= 2;
			goto retry;
		}
		return (NETWORK_ERROR_UNKNOWN);
	}
	alarm(0);
	(void) signal(SIGALRM, old_handler);
	return (s);
}


/*
 *  net_close() closes a TCP connection opened by net_open()
 */
int
net_close(int nd)
{
	syslog(LOG_DEBUG, "net_close(%d)", nd);
	return (close(nd));
}


/*
 *  net_read() reads up to the length specified into the buffer supplied from
 *	the network connection specified
 */
int
net_read(int nd, char *buf, int len)
{
	syslog(LOG_DEBUG, "net_read(%d, 0x%x, %d)", nd, buf, len);
	return (read(nd, buf, len));
}


/*
 *  net_write() writes the buffer specified out to the network connection
 *	supplied.
 */
int
net_write(int nd, char *buf, int len)
{
	syslog(LOG_DEBUG, "net_write(%d, 0x%x, %d)", nd, buf, len);
	return (write(nd, buf, len));
}


/*
 *  net_response() reads in a byte from the network connection and returns
 *	returns -1 if it isn't 0.
 */
int
net_response(int nd)
{
	char	c;

	syslog(LOG_DEBUG, "net_response(%d)", nd);
	if ((net_read(nd, &c, 1) != 1) || c) {
		errno = EIO;
		return (c);
	}
	return (0);
}

/*
 *  net_printf() sends a text message out to the network connection supplied
 *	using the same semantics as printf(3C) for stdio.
 */
int
net_printf(int nd, char *fmt, ...)
{
	char	buf[BUFSIZ];
	va_list	ap;

	syslog(LOG_DEBUG, "net_printf(%d, %s, ...)", nd, fmt);
	va_start(ap, fmt);
	vsprintf(buf, fmt, ap);
	va_end(ap);
	return (net_write(nd, buf, (int)strlen(buf)));
}

/*
 *  net_gets() read from the network connection until either a newline
 *	is encountered, or the buffer passed in is full.  This is similiar
 *	to fgets(3C)
 */
char *
net_gets(char *buf, int bufSize, int nd)
{
	char	tmp;
	int	count = 0;

	syslog(LOG_DEBUG, "net_gets(0x%x, %d, %d)",  buf, bufSize, nd);
	memset(buf, NULL, bufSize);
	while ((count < bufSize) && (net_read(nd, &tmp, 1) > 0))
		if ((buf[count++] = tmp) == '\n') break;

	if (count != 0)
		return (buf);
	return (NULL);
}


/*
 *  net_send_message() sends a message out the network connection using
 *	net_printf() and returns the result from net_response()
 */
int
net_send_message(int nd, char *fmt, ...)
{
	char	buf[BUFSIZ];
	va_list	ap;

	syslog(LOG_DEBUG, "net_send_message(%d, %s, ...)", nd, fmt);
	va_start(ap, fmt);
	vsprintf(buf, fmt, ap);
	va_end(ap);

	if (net_write(nd, buf, (int)((strlen(buf) != 0) ? strlen(buf) : 1)) < 0)
		return (-1);
	return (net_response(nd));
}


/*
 *  net_send_file() sends the appropriate rfc1179 file transfer sub message
 *	to notify the remote side it is sending a file.  It then sends the
 *	file if the remote side responds that it is ready.  If the remote side
 *	can't accept the file an error is returned.  If the transfer fails,
 *	an error is returned.
 */
int
net_send_file(int nd, char *name, char *data, int data_size, int type)
{
	char	*truncated_name,
		*mptr,
		*fileBuf = NULL;
	int	count,
		size;

	syslog(LOG_DEBUG, "net_send_file(%d, %s, 0x%x, %d, %d)", nd,
		(name ? name : "NULL"), data, data_size, type);
	if ((truncated_name = (char *)strrchr(name, '/')) == NULL)
		truncated_name = name;
	else
		truncated_name++;


	if (data == NULL) {
		size = map_in_file(name, &fileBuf);
		mptr = fileBuf;
	} else {
		mptr = data;
		size = data_size;
	}

	if (size < 0) {
		syslog(LOG_DEBUG, "net_send_file(%d, %s, 0x%x, %d, %d): %m", nd,
			(name ? name : "NULL"), data, data_size, type);
		return (NETWORK_ERROR_UNKNOWN);
	}

						/* send XFER command */
	if (net_send_message(nd, "%c%d %s\n", type, size,
				truncated_name) != 0) {
		errno = EIO;
		munmap(fileBuf, size);
		return (NETWORK_ERROR_SEND_RESPONSE);
	}

						/* send DATA and ACK */
	count = size;
	while (count > 0) {
		int	rc;
		int	tmperrno;

		rc = net_write(nd, mptr, count);

		if (rc < 0) {
			/* save/restore errno; will lose if syslogd down */
			tmperrno = errno;
			syslog(LOG_DEBUG, "net_send_file error on write: %m");
			errno = tmperrno;
			return (-1);

		} else {
			count -= rc;
			mptr += rc;
		}
	}

	if (fileBuf != NULL) munmap(fileBuf, size);

	if (net_send_message(nd, "", NULL) != 0) {
		errno = EIO;
		return (NETWORK_ERROR_SEND_FAILED);
	}

	return (0);
}
