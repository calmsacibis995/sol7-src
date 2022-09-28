#
# Copyright (c) 1992,1997 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.6	97/04/21 SMI"
#

PROG=	iostat
OBJS=	iostat.o dsr.o nfs.o
SRCS=	$(OBJS:.o=.c)

include ../Makefile.cmd

LDLIBS += -lkstat
FILEMODE= 0555
OWNER= bin
GROUP= bin

.KEEP_STATE:

all: $(PROG)

install: all $(ROOTPROG)

$(PROG):	$(OBJS)
	$(LINK.c) -o $(PROG) $(OBJS) $(LDLIBS)
	$(POST_PROCESS)
	
clean:
	-$(RM) $(OBJS)

lint:
	$(LINT.c) $(SRCS) $(LDLIBS)

include ../Makefile.targ
