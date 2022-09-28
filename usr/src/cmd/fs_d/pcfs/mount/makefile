#
#ident	"@(#)Makefile	1.9	96/11/22 SMI"
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#

FSTYPE=		pcfs
LIBPROG=	mount
ATTMK=		$(LIBPROG)
OBJS=		$(LIBPROG).o fslib.o
SRCS=		$(LIBPROG).c ../../fslib.c

include		../../Makefile.fstype

CPPFLAGS += -v -I../.. -o $@

$(LIBPROG):	$(OBJS)
		$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
		$(POST_PROCESS)

lint:		lint_SRCS

fslib.o: ../../fslib.c
	$(COMPILE.c) ../../fslib.c
