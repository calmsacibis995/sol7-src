#
#ident	"@(#)Makefile	1.2	97/08/11 SMI"
#
# Copyright (c) 1997 by Sun Microsystems, Inc.
#

PROG= kgmon

include ../../Makefile.cmd

OBJS = kgmon.o
SRCS = $(OBJS:%.o=../%.c)

LDLIBS += -lkvm -lelf

lint	:= LINTFLAGS = -x -L$(ROOT)/usr/lib

CFLAGS += -v

.KEEP_STATE:

%.o:	../%.c
	$(COMPILE.c) $<

all: $(PROG)

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTPROG32)

clean:
	$(RM) $(OBJS)

lint:
	$(LINT.c) $(SRCS) $(LDLIBS)

include ../../Makefile.targ
