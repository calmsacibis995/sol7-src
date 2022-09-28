#
# Copyright (c) 1990-1992, 1994, 1997, by Sun Microsystems, Inc.
# All Rights Reserved.
#
#ident  "@(#)Makefile 1.15     97/10/30 SMI"
#
#
# lib/libmp/Makefile
#
# The builds occur in isa subdirectories.
#

include ../../Makefile.master

SUBDIRS = $(MACH) $(BUILD64) $(MACH64)

# conditional assignments
all :=		TARGET= all
install :=	TARGET= install
clean :=	TARGET= clean
clobber :=	TARGET= clobber
lint :=		TARGET= lint
test :=		TARGET= test

.KEEP_STATE:

all install clean clobber lint test: $(SUBDIRS)

$(MACH) $(MACH64): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
