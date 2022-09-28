#
#ident	"@(#)Makefile	1.17	97/06/25 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/cmd-inet/usr.bin/netstat/Makefile

SGIDPROG=	netstat
PROG=		$(SGIDPROG)

OBJS=		netstat.o unix.o
SRCS= $(OBJS:%.o=%.c)
LIBDHCCLNT = ../../../../lib/libdhcclnt/libdhcclnt.a

include ../../../Makefile.cmd

LDLIBS += $(LIBDHCCLNT) -lsocket -lnsl -lkstat

.KEEP_STATE:

all: $(SGIDPROG) $(NPROG) 

ROOTSGIDPROG=   $(SGIDPROG:%=$(ROOTBIN)/%)
$(ROOTSGIDPROG) :=      FILEMODE=       02555
$(ROOTSGIDPROG) :=      GROUP=          sys

$(PROG): $(OBJS) $(LIBDHCCLNT)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTSGIDPROG) 

clean:
	$(RM) $(OBJS)

lint:	lint_SRCS

include ../../../Makefile.targ

