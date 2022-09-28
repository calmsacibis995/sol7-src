#
# Copyright (c) 1990-1992, 1994, 1997, by Sun Microsystems, Inc.
# All Rights Reserved.
#
#ident	"@(#)Makefile	1.24	97/10/30 SMI"
#
#
# lib/libkvm/Makefile
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

include ../../Makefile.master

# definitions for install_h target
#
# XXX kvm_kbi.h is not currently installed.
# When and if platform-dependent adjuncts to libkvm
# are developed, this header file should be installed
# and put in a package for use by PSM developers, but
# *not* packaged as part of the standard distribution.
#
HDRS=		kvm.h
ROOTHDRDIR=	$(ROOT)/usr/include
ROOTHDRS=	$(HDRS:%=$(ROOTHDRDIR)/%)
CHECKHDRS=	$(HDRS:%.h=%.check)

# install rule for install_h target
$(ROOTHDRDIR)/%: %
	$(INS.file)

.KEEP_STATE:

all install clean clobber lint test: $(SUBDIRS)

install_h: $(ROOTHDRS)

check:	$(CHECKHDRS)

$(MACH) $(MACH64): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

#
# The cscope.out file is made in the current directory and spans all
# supported architectures.
#
# Things to note:
#	1. We use relative names for cscope and tags.
#	2. We *don't* remove the old cscope.out file, because cscope is
#	   smart enough to only build what has changed.  It can be
#	   confused, however, if files are renamed or removed, so it may
#	   be necessary to manually remove cscope.out if a lot of
#	   reorganization has occured.
#
CSCOPE			= cscope
CSDIR			= .
CSINCS			= -I$(CSDIR) -I$(CSDIR)/common
CSPATHS			= $(CSDIR)

.PRECIOUS:	cscope.out

cscope.out: cscope.files FRC
	${CSCOPE} -b -f `pwd`/cscope.out

cscope.files: FRC
	@-$(RM) cscope.files
	echo "$(CSINCS)" > cscope.files
	find $(CSPATHS) -name SCCS -prune -o \
	    -type d -name '.del-*' -prune -o -type f \
	    \( -name '*.[csh]' -o -name 'Makefile*' \) \
	    -print >> cscope.files
	@wc -l cscope.files

FRC:

