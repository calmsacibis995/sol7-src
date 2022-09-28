#
#ident	"@(#)Makefile	1.3	97/11/23 SMI"
#
# Copyright (c) 1995 by Sun Microsystems, Inc.
# All Rights Reserved.
#
# cmd/sgs/nm/Makefile
#

include		../../../Makefile.master

SUBDIRS=	$(MACH)

CSRC=		common/nm.c

all :=		TARGET= all
install :=	TARGET= install
clean :=	TARGET= clean
clobber :=	TARGET= clobber
lint :=		TARGET= lint
package :=	TARGET= package

# for messaging catalog
#
POFILE= nm.po


.KEEP_STATE:

all clean clobber install lint package:	$(SUBDIRS)

catalog:	$(POFILE)

$(POFILE):	
	$(RM) $@
	$(XGETTEXT) $(XGETFLAGS) $(CSRC)
	sed "/^domain/d" < messages.po > $@
	$(RM) messages.po

$(SUBDIRS):	FRC
		@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
