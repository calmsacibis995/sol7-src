#
# Copyright (c) 1997, by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.1	97/11/25 SMI"
#
# lib/libintl/i386/Makefile

include ../Makefile.com

DYNFLAGS +=	-F libc.so.1 $(MAPOPTS)

.KEEP_STATE:

all: $(LIBS)

install: all $(ROOTLIBS) $(ROOTLINKS) $(ROOTARLINK) $(ROOTLINT)

lint:	$(LINTLIB)
