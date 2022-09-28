#
#ident	"@(#)Makefile	1.2	97/12/23 SMI"
#
# Copyright (c) 1994-1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/ptools/pstack/sparc/Makefile
#

PROG = pstack

include ../../../Makefile.cmd
include ../../Makefile.com

OBJS = pstack.o

SRCS = $(OBJS:%.o=../%.c)

FILEMODE = 0555
OWNER = bin
GROUP = bin

# XXX Why can't we say '-lthread_db'?
# XXX Having libthread_db.so a symlink to libthread_db.so.0 is ridiculous.
LDLIBS +=	$(ROOT)/usr/lib/libthread_db.so.1 -lproc

SOURCES = pstack.c
OBJECTS = pstack.o

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
	$(LINT.c) -u $(SRCS) $(LDLIBS)

include ../../../Makefile.targ
