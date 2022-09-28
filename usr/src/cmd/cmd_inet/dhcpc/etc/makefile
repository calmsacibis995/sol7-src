#
#ident	"@(#)Makefile	1.5	96/12/02 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
#
#	cmd/cmd-inet/dhcpc/etc/Makefile
#

include ../../../Makefile.cmd

DHCPCFILES= dhcptags

ROOTETCDHCPC=    $(ROOTETC)/dhcp

ROOTDIRS= $(ROOTETCDHCPC)

IDHCPCFILES=			$(DHCPCFILES:%=$(ROOTETCDHCPC)/%)

DIRMODE= 0775
FILEMODE= 0644
OWNER= root
GROUP= sys

$(ROOTETCDHCPC)/%: %
	$(INS.file)

.KEEP_STATE:

all: $(DHCPCFILES)

install: all $(ROOTDIRS) $(IDHCPCFILES) 

$(ROOTDIRS):
	$(INS.dir)

FRC:

clean clobber lint _msg:

