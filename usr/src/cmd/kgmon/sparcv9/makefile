#
#ident	"@(#)Makefile	1.2	97/08/11 SMI"
#
# Copyright (c) 1997 by Sun Microsystems, Inc.
#

PROG= kgmon

include ../../Makefile.cmd
include ../../Makefile.cmd.64

OBJS = kgmon.o
SRCS = $(OBJS:%.o=../%.c)

LDLIBS += -lkvm -lelf

lint	:= LINTFLAGS64 = -x -D__sparcv9 -L$(ROOT)/usr/lib/sparcv9

CFLAGS64 += -v

.KEEP_STATE:

%.o:	../%.c
	$(COMPILE.c) $(INCS) $<

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
