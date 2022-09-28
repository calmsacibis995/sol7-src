#
#ident	"@(#)Makefile	1.6	97/10/28 SMI"
#
# Copyright (c) 1994 by Sun Microsystems, Inc.
#

include		../Makefile.com

.KEEP_STATE:

ASFLAGS +=	-D__sparcv9 $(sparcv9_XARCH)

install:	all $(ROOTLIB64)/$(PROG)

include		../Makefile.targ

# This stuff is in Makefile.master.64, but for some reason we don't see it
WYOMING_TOOLS=  $(BUILD_TOOLS)/misc/bin
LD=             $(WYOMING_TOOLS)/ld
MCS=            $(WYOMING_TOOLS)/mcs
