#
#ident	"@(#)Makefile	1.46	97/05/27 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/cmd-inet/usr.lib/in.dhcpd/Makefile
#

PROG =		in.dhcpd
OBJS=		bootp.o convert.o dhcp.o dhcptab.o dlprims.o encode.o \
		ethers.o generic.o hash.o icmp.o interfaces.o main.o misc.o \
		per_network.o pf.o relay.o scan.o

SRCS=		$(OBJS:%.o=%.c)

include ../../../Makefile.cmd
include ../Makefile.lib

LDLIBS +=       -ldhcp -lsocket -lnsl -lthread
COPTFLAG +=	-DNDEBUG -D_REENTRANT
LINTFLAGS +=	-Xt -DNDEBUG -D_REENTRANT

# for messaging catalog
POFILE = ${PROG}.po
POFILES = $(OBJS:%.o=%.po)

.KEEP_STATE:

all:	$(PROG)

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

$(POFILE):	$(POFILES)
	$(RM)	$@
	cat $(POFILES) > $@

install:	all $(ROOTLIBINETPROG)

clean:
	$(RM) $(OBJS)

lint:
	$(LINT.c) $(SRCS) $(ENVLDLIBS1) -ldhcp -lsocket -lnsl

include ../../../Makefile.targ
