#
# Copyright (c) 1997, by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.3	97/11/06 SMI"
#
# lib/libgss/sparcv9/Makefile

include ../Makefile.com
include ../../Makefile.lib.64

GSSMECH_DIR_V9 = $(ROOT)/usr/lib/sparcv9/gss

LIBS=   $(DYNLIB)

.KEEP_STATE:

all: $(LIBS)

install: all $(GSSMECH_DIR_V9) $(ROOTLIBS64) $(ROOTLINKS64)

$(GSSMECH_DIR_V9):
	$(INS.dir)
