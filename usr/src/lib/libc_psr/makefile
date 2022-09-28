#
#ident	"@(#)Makefile 1.4 97/09/09 SMI"
#
# Copyright (c) 1994 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libc_psr/Makefile
#

include ../Makefile.lib

SUBDIRS= $(MACH)
SUBDIRS64= $(MACH64)

all :=		TARGET= all
install :=	TARGET= install
clean :=	TARGET= clean
clobber :=	TARGET= clobber
lint :=		TARGET= lint

.KEEP_STATE:

all install clean clobber lint : lib32 \
				$(BUILD64) lib64

lib32:	$(SUBDIRS)

lib64:	$(SUBDIRS64)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

$(SUBDIRS64): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
