#
#ident	"@(#)Makefile	1.1	97/07/09 SMI"
#
# Copyright (c) 1997 by Sun Microsystems, Inc.
#
# lib/libmalloc/i386/Makefile
#

include ../Makefile.com

.KEEP_STATE:

# The static library libgen.a only gets installed for 32-bit, and is
# installed in /usr/ccs/lib rather than /usr/lib, hence the separate
# rule. In addition, we need to link /usr/ccs/lib/libgen.so with
# /usr/lib/libgen.so.1 to accomodate the order search of the linker.

all: $(LIBS)

install: all $(STATICLIB) $(ROOTLIBS) $(ROOTLINKS) $(DYNLINKLIB) $(ROOTLINT)
