#
#ident	"@(#)Makefile	1.4	97/05/27 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/cmd-inet/usr.lib/Makefile

SUBDIRS=	in.dhcpd

include ../../Makefile.cmd

POFILES=        in.dhcpd/in.dhcpd.po
POFILE=         usr.lib.po

all:=		TARGET= all
install:=	TARGET= install
clean:=		TARGET= clean
clobber:=	TARGET= clobber
lint:=		TARGET= lint
_msg:=		TARGET= _msg

.KEEP_STATE:

all install clean clobber lint _msg: $(SUBDIRS) 

_msg:
	cd in.dhcpd; $(MAKE) $(MFLAGS) $(TARGET)
	$(RM) $(POFILE)
	cat $(POFILES)  > $(POFILE)

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
