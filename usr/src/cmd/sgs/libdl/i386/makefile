#
#ident	"@(#)Makefile	1.9	97/06/13 SMI"
#
# Copyright (c) 1994-1997, by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libdl/i386/Makefile

include		../Makefile.com

DYNFLAGS +=	-F /usr/lib/ld.so.1 $(MAPOPTS)

.KEEP_STATE:

include		../Makefile.targ


all:		$(DYNLIB) $(LINTLIB)

install:	all $(ROOTDYNLIB) $(ETCDYNLIB) $(ROOTLINKS) \
		$(ROOTLINTLIB)

lint:		$(LINTLIB)

