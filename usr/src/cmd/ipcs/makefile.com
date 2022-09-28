#
#ident	"@(#)Makefile.com	1.1	97/05/30 SMI"
#
# Copyright (c) 1990, 1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/ipcs/Makefile
#

PROG=		ipcs
OBJS=		$(PROG).o
SRCS=		$(OBJS:%.o=../%.c)

include ../../Makefile.cmd

CPPFLAGS +=	-D_KMEMUSER
CFLAGS	+=	-v
CFLAGS64 +=	-v
LINTFLAGS =	-x
LDLIBS +=	-lkvm -lelf

FILEMODE = 2555
GROUP = sys

.KEEP_STATE:

all:	$(PROG) 

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

lint:	lint_SRCS

%.o:	../%.c
	$(COMPILE.c) $<

clean:
	$(RM) $(CLEANFILES)

include ../../Makefile.targ
