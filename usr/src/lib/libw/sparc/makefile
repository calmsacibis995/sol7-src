#
# Copyright (c) 1997, by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.2	97/12/02 SMI"
#
# lib/libw/sparc/Makefile

include ../Makefile.com

DYNFLAGS +=	-F libc.so.1 $(MAPOPTS)

.KEEP_STATE:

all: $(LIBS)

install: all $(ROOTLIBS) $(ROOTLINKS) $(ROOTARLINK)

lint:
