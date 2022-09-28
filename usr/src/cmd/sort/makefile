#
#ident	"@(#)Makefile	1.12	97/05/14 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

PROG= sort
XPG4PROG= sort

include ../Makefile.cmd

.KEEP_STATE:

CFLAGS += -v
CPPFLAGS += -D_FILE_OFFSET_BITS=64
$(XPG4):= CPPFLAGS += -DXPG4
LDLIBS += -lmapmalloc
XGETFLAGS += -a -x sort.xcl


all: $(PROG) $(XPG4)

install: all $(ROOTPROG) $(ROOTXPG4PROG)

clean:

lint:	lint_PROG

include ../Makefile.targ
