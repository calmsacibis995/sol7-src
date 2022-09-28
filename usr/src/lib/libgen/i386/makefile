#
#pragma	ident	"@(#)Makefile	1.2	97/08/28 SMI"
#
# Copyright (c) 1997, by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libgen/i386/Makefile

include ../Makefile.com

.KEEP_STATE:

all: $(LIBS)

# The static library libgen.a only gets installed for 32-bit, and is 
# installed in /usr/ccs/lib rather than /usr/lib, hence the separate 
# rule. In addition, we need to link /usr/ccs/lib/libgen.so with
# /usr/lib/libgen.so.1 to accomodate the order search of the linker.
#
install: all $(STATICLIB) $(ROOTLIBS) $(ROOTLINKS) $(DYNLINKLIB) $(ROOTLINT)
