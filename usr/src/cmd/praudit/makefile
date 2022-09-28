#
# @(#)Makefile 1.12 97/11/14 SMI
#
# Copyright (c) 1993 by Sun Microsystems, Inc.
#
# cmd/praudit/Makefile

PROG = praudit

include ../Makefile.cmd

TEXT_DOMAIN=SUNW_OST_OSCMD


LDLIBS += -lbsm -lsocket -lnsl

OBJS = praudit.o
SRCS = praudit.c
POFILE = praudit_all.po
POFILES = praudit.po structures.po

.KEEP_STATE:

all: $(PROG)

install: all $(ROOTUSRSBINPROG)

$(POFILE):	$(POFILES)
	$(RM) -f $@
	cat $(POFILES) > $@

clean:
	rm -f $(OBJS) $(PROG).po $(POFILE)

include ../Makefile.targ
