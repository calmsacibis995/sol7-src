#
#ident	"@(#)Makefile	1.3	97/08/21 SMI"
#
# Copyright (c) 1989-1993, by Sun Microsystems, Inc.
#
# cmd/savecore/Makefile
#

PROG= savecore

COMMON= ..

OBJS= savecore.o

include ../../Makefile.cmd
include ../../Makefile.cmd.64

LDLIBS64 += -lkvm -lelf
DEFINES = -D__sparcv9 -D_ELF64
CPPFLAGS += $(DEFINES) $(INCS)

%.o : $(COMMON)/%.c
	$(COMPILE.c) $<

.KEEP_STATE:

all: $(PROG)

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)

install: all $(ROOTPROG64)

clean:

lint:	lint_PROG

include ../../Makefile.targ
