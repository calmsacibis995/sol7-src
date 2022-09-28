#
# Copyright (c) 1997 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile.com	1.1	97/06/05 SMI"
#

PROG= lockstat
OBJS= lockstat.o sym.o
SRCS= $(OBJS:%.o=../%.c)

include ../../Makefile.cmd

LDLIBS += -lelf -lkstat
CFLAGS += -v
CFLAGS64 += -v

FILEMODE= 0555
OWNER= bin
GROUP= bin

CLEANFILES += $(OBJS)

.KEEP_STATE:

all: $(PROG)

$(PROG):	$(OBJS)
	$(LINK.c) -o $(PROG) $(OBJS) $(LDLIBS)
	$(POST_PROCESS)

clean:
	-$(RM) $(CLEANFILES)

lint:	lint_SRCS

%.o:    ../%.c
	$(COMPILE.c) $<

include ../../Makefile.targ
