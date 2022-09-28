#
#ident	"@(#)Makefile.com	1.3	97/06/23 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.

PROG=		mcs
STRIPFILE=	strip

ROOTLINKS=	$(ROOTCCSBIN)/$(STRIPFILE)

include		$(SRC)/cmd/Makefile.cmd
include		$(SRC)/cmd/sgs/Makefile.com

# avoid bootstrap problems
MCS=		/usr/ccs/bin/mcs

OBJS=		main.o		file.o		utils.o		global.o \
		message.o

CPPFLAGS=	-I../../include $(CPPFLAGS.master)
LDLIBS +=	-lelf
LINTFLAGS +=	$(LDLIBS)

SRCS=		$(OBJS:%.o=../common/%.c)

CLEANFILES +=	$(OBJS) $(LINTOUT)


# Building SUNWonld results in a call to the `package' target.  Requirements
# needed to run this application on older releases are established:
#	i18n support requires libintl.so.1 prior to 2.6

package :=	LDLIBS += /usr/lib/libintl.so.1
