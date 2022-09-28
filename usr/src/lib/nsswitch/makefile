#
#ident	"@(#)Makefile	1.6	97/08/12 SMI"
#
# Copyright (c) 1991 - 1996, by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/nsswitch/Makefile


include $(SRC)/Makefile.master

SUBDIRS= files nis nisplus compat dns

all:=		TARGET= all
clean:=		TARGET= clean
clobber:=	TARGET= clobber
install:=	TARGET= install
lint:=		TARGET= lint

.KEEP_STATE:

all clean clobber install lint: $(SUBDIRS)

$(SUBDIRS): FRC
	@cd $@; $(MAKE) $(TARGET)

FRC:
