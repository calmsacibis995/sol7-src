#
#ident	"@(#)Makefile	1.7	97/02/24 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All Rights Reserved.
#

PROG=ntpq

include ../../Makefile.cmd
include ../Makefile.cmd

.KEEP_STATE:

all: $(PROG) 

CLOBBERFILES = $(ROOTUSRSBINPROG)

install: all $(ROOTUSRSBINPROG)

clean:

lint:	lint_PROG

include ../../Makefile.targ

LDLIBS += $(LIBS) ../$(LIBNTP_A)
OBJS=	ntpq.o ntpq_ops.o version.o
SOURCE=	ntpq.c ntpq_ops.c

$(PROG): $(OBJS)
	$(LINK.c) -o $@ $(OBJS) $(LDLIBS)

lint:	$(LINTLIB)
	$(LINT.c) -x -u $(DEFS) $(INCL) ../$(LLIBNTP_A) $(SOURCE) >lint.errs

clean:
	$(RM) $(OBJS) $(PROG) lint.errs
