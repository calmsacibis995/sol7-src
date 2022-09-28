#
#ident	"@(#)Makefile	1.5	97/09/02 SMI"
#
# Copyright (c) 1997 by Sun Microsystems, Inc.
#
# lib/libmalloc/sparc/Makefile
#

include ../Makefile.com

.KEEP_STATE:

all: $(LIBS)

# The static library libmalloc.a only gets installed for 32-bit, and is
# installed in /usr/ccs/lib rather than /usr/lib, hence the separate
# rule. In addition, we need to link /usr/ccs/lib/libmalloc.so with
# /usr/lib/libmalloc.so.1 to accomodate the order search of the linker.
 
install: all $(STATICLIB) $(ROOTLIBS) $(ROOTLINKS) $(DYNLINKLIB) $(ROOTLINT)
