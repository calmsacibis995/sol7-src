#
# Copyright (c) 1996-1997 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.5	97/12/08 SMI"

PROG=xntpd

include ../../Makefile.cmd
include ../Makefile.cmd

CLOBBERFILES = $(ROOT)/etc/rc2.d/S74$(PROG) \
	$(ROOT)/etc/rc1.d/K40$(PROG) \
	$(ROOT)/etc/rcS.d/K40$(PROG) \
	$(ROOT)/etc/rc0.d/K40$(PROG) \
	$(ROOTETCINITPROG)

FILEMODE=744
OWNER=root
GROUP=sys

.KEEP_STATE:

all lint clean:

install: $(ROOTETCINITPROG)
	$(RM) $(ROOT)/etc/rc2.d/S74$(PROG)
	$(LN) $(ROOTETCINIT)/$(PROG) $(ROOT)/etc/rc2.d/S74$(PROG)
	$(RM) $(ROOT)/etc/rc1.d/K40$(PROG)
	$(LN) $(ROOTETCINIT)/$(PROG) $(ROOT)/etc/rc1.d/K40$(PROG)
	$(RM) $(ROOT)/etc/rcS.d/K40$(PROG)
	$(LN) $(ROOTETCINIT)/$(PROG) $(ROOT)/etc/rcS.d/K40$(PROG)
	$(RM) $(ROOT)/etc/rc0.d/K40$(PROG)
	$(LN) $(ROOTETCINIT)/$(PROG) $(ROOT)/etc/rc0.d/K40$(PROG)

include ../../Makefile.targ
