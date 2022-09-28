#
#ident	"@(#)Makefile	1.3	97/12/23 SMI"
#
# Copyright (c) 1989,1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/truss/sparcv9/Makefile
#

PROG=	truss

OBJS=	main.o listopts.o ipc.o actions.o expound.o codes.o print.o \
	ramdata.o systable.o procset.o xstat.o fcall.o

SRCS=	$(OBJS:%.o=../%.c)

include ../../Makefile.cmd
include ../../Makefile.cmd.64

CFLAGS64	+= -v

# XXX Why can't we say '-lthread_db'?
# XXX Having libthread_db.so a symlink to libthread_db.so.0 is ridiculous.
LDLIBS		+= -lproc $(ROOT)/usr/lib/sparcv9/libthread_db.so.1 -lrtld_db

lint	:= LINTFLAGS64 = -x -D__sparcv9

CPPFLAGS += -D_LARGEFILE64_SOURCE=1

.KEEP_STATE:

%.o:	../%.c
	$(COMPILE.c) $<

all: $(PROG)

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTPROG64)

clean:
	$(RM) $(OBJS)

lint:
	$(LINT.c) $(SRCS) $(LDLIBS)

include ../../Makefile.targ
