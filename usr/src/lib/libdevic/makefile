#
# Copyright (c) 1990-1997, by Sun Microsystems, Inc.
# All rights reserved.
#
#ident   "@(#)Makefile 1.3     97/11/05 SMI"
#
# lib/libdevice/Makefile

include ../../Makefile.master

SUBDIRS = $(MACH) $(BUILD64) $(MACH64)

# conditional assignments
all :=          TARGET= all
install :=      TARGET= install
clean :=        TARGET= clean
clobber :=      TARGET= clobber
lint :=         TARGET= lint
test :=         TARGET= test

# definitions for install_h target
HDRS=           libdevice.h
ROOTHDRDIR=     $(ROOT)/usr/include
ROOTHDRS=       $(HDRS:%=$(ROOTHDRDIR)/%)
CHECKHDRS=      $(HDRS:%.h=%.check)

.KEEP_STATE:
 
all install clean clobber lint: $(SUBDIRS)

#
# Need this empty target.
# It is called from above
#
install_h:

check: $(CHECKHDRS)

$(MACH) $(MACH64): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

