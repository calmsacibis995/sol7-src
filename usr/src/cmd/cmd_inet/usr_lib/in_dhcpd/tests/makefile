#
#ident	"@(#)Makefile	1.7	97/04/28 SMI"
#
# Copyright (c) 1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/cmd-inet/usr.lib/in.dhcpd/tests/Makefile
#

PROG = test_client
OBJS = test_client.o
SRCS = $(OBJS:%.o=%.c)

include ../../../../Makefile.cmd

LDLIBS += -ldhcp -lposix4 -lsocket -lnsl -lthread
COPTFLAG = -Xt -g -D_REENTRANT
LINTFLAGS += -Xt

.KEEP_STATE:

all: $(PROG)

clean:
	$(RM) $(OBJS)

install:
	$(ECHO) "Nothing to Install" >&2


lint: 
	$(LINT.c) $(PROG).c $(ENVLDLIBS1) -ldhcp -lsocket -lnsl

include ../../../../Makefile.targ
