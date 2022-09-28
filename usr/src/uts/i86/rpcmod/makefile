#
# uts/i86/rpcmod/Makefile
# Copyright (c) 1991-1994, 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.4	97/11/12 SMI"
#
#	This makefile drives the production of the rpcmod streams kernel
#	module.
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
MODULE		= rpcmod
OBJECTS		= $(RPCMOD_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(RPCMOD_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_STRMOD_DIR)/$(MODULE)
ROOTLINK	= $(ROOT_SYS_DIR)/$(MODULE)

#
#	Include common rules.
#
include $(UTSBASE)/i86/Makefile.i86

#
#	Define targets
#
ALL_TARGET	= $(BINARY)
LINT_TARGET	= $(MODULE).lint
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE) $(ROOTLINK)

#
#	Overrides.
#
MODSTUBS_DIR	 = $(OBJS_DIR)
$(MODSTUBS_O)	:= AS_CPPFLAGS += -DRPCMOD_MODULE
CLEANFILES	+= $(MODSTUBS_O)

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

$(ROOTLINK):	$(ROOT_SYS_DIR) $(ROOTMODULE)
		-$(RM) $@; ln $(ROOTMODULE) $@

#
#	Include common targets.
#
include $(UTSBASE)/i86/Makefile.targ
