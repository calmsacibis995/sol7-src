#
#ident	"@(#)Makefile	1.2	97/09/11 SMI"
#
# Copyright (c) 1997 by Sun Microsystems, Inc.
#
# lib/libeti/menu/sparc/Makefile
#

include ../Makefile.com

.KEEP_STATE:

# The static library libmenu.a only gets installed for 32-bit, and is
# installed in /usr/ccs/lib rather than /usr/lib, hence the separate
# rule. In addition, we need to link /usr/ccs/lib/libmenu.so with
# /usr/lib/libmenu.so.1 to accomodate the order search of the linker.
#
install: all $(STATICLIB) $(ROOTLIBS) $(ROOTLINKS) $(DYNLINKLIB) $(ROOTLINT)

