#
#ident	"@(#)Makefile.com	1.1	97/12/22 SMI"
#
# Copyright (c) 1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libthread_psr/Makefile.com
#

#
#	Create default so empty rules don't
#	confuse make
#
CLASS		= 32

LIBRARY		= libthread_psr.a
VERS		= .1

include $(SRCDIR)/../Makefile.lib
include $(SRCDIR)/../../Makefile.psm

LIBTHREADSRC	= $(SRCDIR)/../libthread
LIBS		= $(DYNLIB)
IFLAGS		= -I$(LIBTHREADSRC)/inc \
			-I$(ROOT)/usr/platform/$(PLATFORM)/include\
			-I$(SRCDIR)/sparc/sun4u
CPPFLAGS	= -D_REENTRANT -D$(MACH) $(IFLAGS) $(CPPFLAGS.master)
ASDEFS		= -D__STDC__ -D_ASM $(CPPFLAGS) -DPSR
ASFLAGS		= -P $(ASDEFS)

#
# install rule
#
$(USR_PSM_LIB_DIR)/%: % $(USR_PSM_LIB_DIR)
	$(INS.file)

#
# build rules
#
pics/%.o: $(LIBTHREADSRC)/sparcv9/ml/%.s
	$(AS) $(ASFLAGS) $< -o $@
	$(POST_PROCESS_O)
