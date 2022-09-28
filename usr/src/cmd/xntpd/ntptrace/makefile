#
#ident	"@(#)Makefile	1.7	97/02/24 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All Rights Reserved.
#

PROG=ntptrace

include ../../Makefile.cmd
include ../Makefile.cmd

.KEEP_STATE:

all: $(PROG) 

CLOBBERFILES = $(ROOTUSRSBINPROG)

install: all $(ROOTUSRSBINPROG)

clean:

lint:	lint_PROG

include ../../Makefile.targ
#
OBJS=	ntptrace.o version.o
SOURCE=	ntptrace.c
LDLIBS += $(LIBS) ../$(LIBNTP_A)

$(PROG): $(OBJS)
	$(LINK.c) -o $@ $(OBJS) $(LDLIBS)

lint:
	$(LINT.c) -x -u $(DEFS) $(INCL) ../$(LLIBNTP_A) $(SOURCE) >lint.errs

clean:
	$(RM) $(OBJS) $(PROG) lint.errs
