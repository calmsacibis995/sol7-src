#
#ident	"@(#)Makefile	1.26	97/10/28 SMI"
#
# Copyright (c) 1989, 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/awk/Makefile
#

# NOTE: awk is oawk.

PROG= nawk

OBJ1= b.o lib.o main.o parse.o proctab.o run.o tran.o
OBJ2= awk.g.o awk.lx.o
OBJS= $(OBJ2) $(OBJ1)
SRCS= $(OBJ1:%.o=%.c)

include ../Makefile.cmd
#
# Message catalog
#
POFILES= $(OBJS:%.o=%.po)
POFILE= awk.po
XGETFLAGS += -a -x awk.xcl
#

CPPFLAGS += -D_FILE_OFFSET_BITS=64
YFLAGS += -d
LDLIBS += -lm -ll
maketab := LDLIBS = -lm -ll -lintl
CLEANFILES= maketab proctab.c awk.g.c awk.lx.c y.tab.h

.KEEP_STATE:

all: $(PROG) 

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

#
# message catalog
#

$(POFILE):  y.tab.h $(POFILES)
	$(RM) $@
	cat $(POFILES) > $@
#

proctab.c: maketab
	rm -f $@; ./maketab > $@

maketab: maketab.c
	$(NATIVECC) -O maketab.c -o $@ $(LDLIBS)

install: all $(ROOTPROG) $(ROOTLINK)

clean:
	$(RM) $(OBJS) $(CLEANFILES)

lint: lint_SRCS

awk.g.c + y.tab.h: awk.g.y

awk.g.o: awk.g.c

awk.lx.c: awk.lx.l

proctab.o: proctab.c
	$(COMPILE.c) proctab.c
	$(POST_PROCESS_O)

include ../Makefile.targ
