#
# uts/i86/ppp/Makefile
# Copyright (c) 1993-1994, by Sun Microsystems, Inc.
#
#pragma ident	"@(#)Makefile	1.4	97/11/12 SMI"
#
#	This makefile drives the production of the ppp streams kernel
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
MODULE		= ppp
OBJECTS		= $(PPP_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(PPP_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(USR_STRMOD_DIR)/$(MODULE)

#
#	Include common rules.
#
include $(UTSBASE)/i86/Makefile.i86

#
#	Define targets
#
ALL_TARGET	= $(BINARY)
LINT_TARGET	= $(MODULE).lint
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE)

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
