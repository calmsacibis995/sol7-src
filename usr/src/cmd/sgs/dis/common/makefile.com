#
#ident	"@(#)Makefile.com	1.6	97/09/15 SMI"
#
# Copyright (c) 1993, 1994 by Sun Microsystems, Inc.
#
# cmd/sgs/dis/common/Makefile.com
#

PROG=		dis

include 	../../../Makefile.cmd

COMOBJS=	debug.o extn.o lists.o main.o utls.o

OBJS=		$(COMOBJS) $(MACHOBJS)
SRCS=		$(COMOBJS:%.o=../common/%.c) $(MACHOBJS:.o=.c)

INCLIST=	-I. -I../common -I../../include -I../../include/$(MACH)
CPPFLAGS=	$(INCLIST) $(DEFLIST) $(CPPFLAGS.master)
LDLIBS +=	../../sgsdemangler/`mach`/libdemangle.a -lelf -ldl
LINTFLAGS +=	-errfmt=simple -aux $(LDLIBS)
CLEANFILES +=	$(LINTOUT)

%.o:		../common/%.c
		$(COMPILE.c) $<

.KEEP_STATE:

all:		$(PROG)

$(PROG):	$(OBJS)
		$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
		$(POST_PROCESS)

install:	all $(ROOTCCSBINPROG)

clean:
		$(RM) $(OBJS) $(CLEANFILES)

lint:		$(LINTOUT)

$(LINTOUT):	$(SRCS)
		$(LINT.c)  $(SRCS)

include		../../../Makefile.targ
