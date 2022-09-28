#
# Copyright (c) 1997, by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident "@(#)Makefile	1.3 97/11/20 SMI"
#
# lib/fn/libxfn/sparcv9/Makefile

include ../Makefile.com
include ../../../Makefile.lib.64

MAPFILES=      ../common/mapfile-vers  ../$(MACH64)/mapfile-vers

.KEEP_STATE:

BUILDCCC.SO += $(CCFLAGS)

install debug tcov: $(ROOTLIBDIR64) \
      $(ROOTLIBS64) $(ROOTLINKSCCC64)
