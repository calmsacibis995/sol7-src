#
#ident	"@(#)Makefile	1.1	97/12/22 SMI"
#
# Copyright (c) 1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libthread_psr/Makefile
#

include ../Makefile.lib

SUBDIRS= $(MACH)

all :=		TARGET= all
install :=	TARGET= install
clean :=	TARGET= clean
clobber :=	TARGET= clobber
lint :=		TARGET= lint

.KEEP_STATE:

all install clean clobber lint : lib32

lib32:	$(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
