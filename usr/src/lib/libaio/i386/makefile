#
#pragma ident	"@(#)Makefile	1.18	97/10/09 SMI"
#
# Copyright (c) 1995,1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# libaio/i386/Makefile

include		../Makefile.com

MAPFILES=       ../common/mapfile-vers  mapfile-vers
MAPOPTS=        $(MAPFILES:%=-M %)

DYNFLAGS += $(MAPOPTS)

.KEEP_STATE:

install:	all $(ROOTLIBS) $(ROOTLINKS) $(ROOTLINT)
