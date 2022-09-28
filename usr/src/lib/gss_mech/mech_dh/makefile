#
#ident	"@(#)Makefile	1.1	97/11/19 SMI"
#
# Copyright (c) 1997, by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/gss_mechs/mech_dh/Makefile
#

SUBDIRS= backend .WAIT dh192 dh640 dh1024 

all :=		TARGET= all
install :=	TARGET= install
clean :=	TARGET= clean
clobber :=	TARGET= clobber
lint :=		TARGET= lint

.KEEP_STATE:

all clean clobber lint: $(SUBDIRS)

install: $(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET); echo

FRC:
