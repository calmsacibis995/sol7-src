#
# Copyright (c) 1996-1998, by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.2	98/01/21 SMI"
#
# cmd/cfgadm/Makefile

PROG= cfgadm 

include ../Makefile.cmd

LDLIBS += -lcfgadm -ldevinfo
LINTFLAGS += $(ENVLDFLAGS1) $(ENVLDFLAGS2) $(ENVLDFLAGS3) $(LDLIBS)

.KEEP_STATE:

all: $(PROG) 

install: all $(ROOTUSRSBINPROG)
	-$(RM) $(ROOTETCPROG)
	$(SYMLINK) ../usr/sbin/$(PROG) $(ROOTETCPROG)

clean:

lint:	lint_PROG

include ../Makefile.targ
