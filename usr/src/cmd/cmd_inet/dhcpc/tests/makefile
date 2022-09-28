#
#ident	"@(#)Makefile	1.7	96/11/26 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
#	cmd/cmd-inet/dhcpc/tests/Makefile
#

PROG =	bytetok clpolicy convsyms haddrtoa iftoppa inetaton macaddr \
	root tags seekdict tokenise 

OBJS = $(PROG:%=%.o)
SRCS = $(OBJS:%.o=%.c)
LINTPROGS = $(PROG:%=%.ln)

include ../../../Makefile.cmd

.KEEP_STATE:

LIBDHCCLNTDIR = ../../../../lib/libdhcclnt
LDLIBS += $(LIBDHCCLNTDIR)/libdhcclnt.a
CPPFLAGS += -I$(LIBDHCCLNTDIR) -DUSE_TCP=1
CLOBBERFILES += $(LINTPROGS)

all:	$(PROG)

clean:
	$(RM) $(OBJS)

install:	all nothing

nothing:
	@$(ECHO) "Nothing to Install"

lint:	$(LINTPROGS)

.c.ln:
	$(LINT.c) $< -L$(LIBDHCCLNTDIR) -ldhcclnt -lsocket -lc

include ../../../Makefile.targ
