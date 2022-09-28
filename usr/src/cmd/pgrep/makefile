#
# Copyright (c) 1997 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.1	97/12/08 SMI"

PROG = pgrep
PKILLFILE = pkill

ROOTLINKS = $(ROOTBIN)/$(PKILLFILE)

OBJS = pgrep.o idtab.o psexp.o utils.o
SRCS = $(OBJS:.o=.c)

lint := LINTFLAGS = -ux

include ../Makefile.cmd

CLOBBERFILES += $(PKILLFILE)
CFLAGS += -v

FILEMODE = 0555
OWNER = bin
GROUP = bin

.KEEP_STATE:

all: $(PROG) $(PKILLFILE)

$(PKILLFILE): $(PROG)
	$(RM) $@
	$(LN) $(PROG) $@

$(PROG): $(OBJS)
	$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTLINKS)

$(ROOTLINKS): $(ROOTPROG)
	$(RM) $@
	$(LN) $(ROOTPROG) $@

clean:
	$(RM) $(OBJS)

lint: lint_SRCS

include ../Makefile.targ
