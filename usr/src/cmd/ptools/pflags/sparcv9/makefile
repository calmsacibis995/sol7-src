#
#ident	"@(#)Makefile	1.3	97/12/23 SMI"
#
# Copyright (c) 1994-1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/ptools/pflags/sparcv9/Makefile
#

PROG = pflags

include ../../../Makefile.cmd
include ../../../Makefile.cmd.64
include ../../Makefile.com

OBJS = pflags.o

SRCS = $(OBJS:%.o=../%.c)

FILEMODE = 0555
OWNER = bin
GROUP = bin

LDLIBS += -lproc

.KEEP_STATE:

%.o:	../%.c
	$(COMPILE.c) $<

all: $(PROG) 

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTPROCBINPROG64)

clean:
	$(RM) $(OBJS) $(CLOBBERFILES)

lint:
	$(LINT.c) $(SRCS) $(LDLIBS)

include ../../../Makefile.targ
