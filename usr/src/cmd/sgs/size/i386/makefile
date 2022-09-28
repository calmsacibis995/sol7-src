#
#ident	"@(#)Makefile	1.2	97/11/23 SMI"
#
# Copyright (c) 1993 by Sun Microsystems, Inc.
#
# cmd/sgs/size/i386/Makefile
#

PROG=		size

include 	../../../Makefile.cmd

OBJS=		main.o process.o fcns.o

SRCS=		$(OBJS:%.o=../common/%.c)

INCLIST=	-I../common -I../../include -I../../include/i386
DEFLIST=	-DI386 -DELF
CPPFLAGS=	$(INCLIST) $(DEFLIST) $(CPPFLAGS.master)
LDLIBS +=	-lelf
LINTFLAGS +=	$(LDLIBS)

CLEANFILES +=	$(LINTOUT)

# Building SUNWonld results in a call to the `package' target.  Requirements
# needed to run this application on older releases are established:
#	i18n support requires libintl.so.1 prior to 2.6

package :=	LDLIBS += /usr/lib/libintl.so.1

%.o:		../common/%.c
		$(COMPILE.c) $<

.KEEP_STATE:

all:		$(PROG)

$(PROG):	$(OBJS)
		$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
		$(POST_PROCESS)

package \
install:	all $(ROOTCCSBINPROG)

clean:
		$(RM) $(OBJS) $(CLEANFILES)

lint:		$(LINTOUT)

$(LINTOUT):	$(SRCS)
		$(LINT.c) $(SRCS) > $(LINTOUT) 2>&1

include		../../../Makefile.targ
