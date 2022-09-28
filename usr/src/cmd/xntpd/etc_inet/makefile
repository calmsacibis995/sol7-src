#
#ident	"@(#)Makefile	1.5	97/02/28 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All Rights Reserved.
#

PROG=ntp.client ntp.server

include ../../Makefile.cmd
include ../Makefile.cmd

CLOBBERFILES = $(ROOTETCINETPROG)
FILEMODE=644
OWNER=root
GROUP=sys
VARNTPD=$(ROOT)/var/ntp
VARNTPSTATSD=$(VARNTPD)/ntpstats
DIRS=$(VARNTPD) $(VARNTPSTATSD)
$(DIRS) :=	OWNER= root
$(DIRS) :=	GROUP= sys
$(DIRS) :=	DIRMODE=755

.KEEP_STATE:

all clean lint:

install: $(DIRS) $(ROOTETCINETPROG)

$(DIRS):
	$(INS.dir)

include ../../Makefile.targ
