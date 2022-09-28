#
# Copyright (c) 1997, by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile 1.1	97/08/29 SMI"
#
# lib/libbsm/sparcv9/Makefile

include ../Makefile.com
include ../../Makefile.lib.64

DYNFLAGS += $(DYNFLAGS64)

.KEEP_STATE:

LIBS = 	$(DYNLIB) $(LINTLIB)

all: $(LIBS)

install: all $(ROOTLIBS64) $(ROOTLINKS64)
