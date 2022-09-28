#
#ident	"@(#)Makefile	1.2	97/12/23 SMI"
#
# Copyright (c) 1994-1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/ptools/pwdx/i386/Makefile
#

PROG = pwdx
LIBS = libcwd.so.1

include ../../../Makefile.cmd
include ../../Makefile.com

OBJS = cwd.o

SRCS = $(OBJS:%.o=../%.c)

$(ROOTPROCBINPROG32) := FILEMODE = 0555
$(ROOTPROCBINPROG32) := OWNER = bin
$(ROOTPROCBINPROG32) := GROUP = bin

$(PROG)	:= LDLIBS +=	-lproc
lint	:= LDLIBS +=	-lproc


# Optimization ruins libcwd
$(OBJS)	:= CFLAGS +=	-K PIC ../$(MACH).il

.KEEP_STATE:

%.o:	../%.c
	$(COMPILE.c) $<

all: $(PROG) $(LIBS)

$(PROG):	$(PROG).o
	$(LINK.c) $(PROG).o -o $@ $(LDLIBS)
	$(POST_PROCESS)

$(LIBS):	$(OBJS)
	$(LD) -B local -z defs -z text -i -o $(LIBS) -e cwd -G $(OBJS)

install: all $(ROOTPROCBINPROG32) $(ROOTPROCLIBLIB32)

CLOBBERFILES += $(LIBS) $(OBJS)

clean:
	$(RM) $(OBJS) $(PROG).o $(CLOBBERFILES)

lint:
	$(LINT.c) $(SRCS) $(LDLIBS)
	$(LINT.c) ../$(PROG).c $(LDLIBS)

include ../../../Makefile.targ
