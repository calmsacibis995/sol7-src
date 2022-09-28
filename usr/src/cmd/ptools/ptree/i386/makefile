#
#ident	"@(#)Makefile	1.2	97/12/23 SMI"
#
# Copyright (c) 1994-1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/ptools/ptree/i386/Makefile
#

PROG = ptree

include ../../../Makefile.cmd
include ../../Makefile.com

OBJS = ptree.o

SRCS = $(OBJS:%.o=../%.c)

FILEMODE = 0555
OWNER = bin
GROUP = bin

.KEEP_STATE:

%.o:	../%.c
	$(COMPILE.c) $<

all: $(PROG) 

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTPROCBINPROG32)

clean:
	$(RM) $(OBJS) $(CLOBBERFILES)

lint:
	$(LINT.c) $(SRCS) $(LDLIBS)

include ../../../Makefile.targ
