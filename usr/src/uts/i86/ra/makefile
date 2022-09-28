#
# uts/i86/ra/Makefile
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident  "@(#)Makefile 1.4     97/11/12 SMI"
#
#	This file makes the ra resource allocator stub for x86 system
#
#	i86 architecture dependent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..
#
#	Define the module and object file sets.
#
MODULE		= ra
OBJECTS		= $(RA_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(RA_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_DRV_DIR)/$(MODULE)
CONF_SRCDIR	= $(UTSBASE)/common/pcmcia/ra

#
#	Include common rules.
#
include $(UTSBASE)/i86/Makefile.i86

#
#	Define targets
#
ALL_TARGET	= $(BINARY) $(SRC_CONFILE)
LINT_TARGET	= $(MODULE).lint
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE) $(ROOT_CONFFILE)

#
#	Override defaults to build a unique, local modstubs.o.
#
MODSTUBS_DIR	 = $(OBJS_DIR)
$(MODSTUBS_O)	:= AS_CPPFLAGS += -DRA_MODULE
CLEANFILES	+= $(MODSTUBS_O)

#
# lint pass one enforcement
#
CFLAGS += -v

#
#	Default build targets.
#
.KEEP_STATE:

def:		$(DEF_DEPS)

all:		$(ALL_DEPS)

clean:		$(CLEAN_DEPS)

clobber:	$(CLOBBER_DEPS)

lint:		$(LINT_DEPS)

modlintlib:	$(MODLINTLIB_DEPS)

clean.lint:	$(CLEAN_LINT_DEPS)

install:	$(INSTALL_DEPS)

#
#	Include common targets.
#
include $(UTSBASE)/i86/Makefile.targ
