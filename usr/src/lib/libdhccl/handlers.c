/*
 * handlers.c: "Setup signal handling in agent".
 *
 * SYNOPSIS
 *    void handle_terminate_signals()
 *
 * DESCRIPTION
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#pragma ident   "@(#)handlers.c 1.3 96/11/25 SMI"

#include <signal.h>
#include <stdlib.h>
#include "client.h"

void
handle_terminate_signals(void)
{
	struct sigaction s;

	s.sa_flags = 0;
	sigemptyset(&s.sa_mask);
	sigaddset(&s.sa_mask, SIGINT);
	sigaddset(&s.sa_mask, SIGQUIT);
	sigaddset(&s.sa_mask, SIGTERM);
	sigaddset(&s.sa_mask, SIGUSR1);
	sigaddset(&s.sa_mask, SIGPOLL);
	sigaddset(&s.sa_mask, SIGALRM);
	sigaddset(&s.sa_mask, SIGPIPE);

	s.sa_handler = agent_exit;
	sigaction(SIGTERM, &s, 0);
	sigaction(SIGPIPE, &s, 0);

	/*
	 *  Don't exit on these signals. In normal mode, dhcpagent is a daemon
	 *  process, can be stopped cleanly with SIGTERM, and is immune to the
	 *  keyboard generated SIGINT and SIGQUIT. However, when debugging,
	 *  it is convenient to run dhcpagent in the foreground so that the
	 *  DHCP transactions can be viewed. Now, we are allowing ctrl-C
	 *  to kill ifconfig so that in the case where DHCP isn't available
	 *  we don't hang forever. But we don't want this to kill the agent
	 *  because we still require its services for a second, third..
	 *  interface. Note that since the agent will have to start as a
	 *  background process, the Bourne shell has already taken care
	 *  of SIGINT and SIGQUIT by ignoring them prior to exec'ing dhcpagent.
	 */
#if 0
	sigaction(SIGINT, &s, 0);
	sigaction(SIGQUIT, &s, 0);
#endif

	s.sa_flags = 0;
	s.sa_flags = SA_RESTART;
	sigemptyset(&s.sa_mask);
	sigaddset(&s.sa_mask, SIGPOLL);
	sigaddset(&s.sa_mask, SIGALRM);
	sigaddset(&s.sa_mask, SIGUSR1);
	s.sa_handler = dumpInternals;
	sigaction(SIGUSR1, &s, 0);

	sigemptyset(&s.sa_mask);
	s.sa_flags = 0;
	s.sa_handler = (void (*)())SIG_IGN;
	sigaction(SIGHUP, &s, 0);
	sigaction(SIGUSR2, &s, 0);
}
