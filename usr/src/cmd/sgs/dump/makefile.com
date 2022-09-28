#
#ident	"@(#)Makefile.com	1.5	97/12/30 SMI"
#
# Copyright (c) 1994 by Sun Microsystems, Inc.
#
# cmd/sgs/dump/Makefile.com

PROG=		dump

include 	../../../Makefile.cmd

COMOBJS=	debug.o		dump.o		fcns.o		util.o

SRCS=		$(COMOBJS:%.o=../common/%.c)

OBJS =		$(COMOBJS)
.PARALLEL:	$(OBJS)

CPPFLAGS=	-I../common -I../../include -I../../include/$(MACH) \
		$(CPPFLAGS.master)
LDLIBS +=	-L../../sgsdemangler/`mach` -ldemangle -lelf -ldl
LINTFLAGS +=	$(LDLIBS)
CLEANFILES +=	$(LINTOUT)


# Building SUNWonld results in a call to the `package' target.  Requirements
# needed to run this application on older releases are established:
#	i18n support requires libintl.so.1 prior to 2.6

package :=	LDLIBS += /usr/lib/libintl.so.1
