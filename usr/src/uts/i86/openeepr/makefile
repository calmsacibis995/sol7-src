#
# uts/i86/openeepr/Makefile
# Copyright (c) 1996 by Sun Microsystems, Inc.
#
#ident	"@(#)Makefile	1.2	97/11/12 SMI"
#
#	This makefile drives the production of the openeepr driver
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
MODULE		= openeepr
OBJECTS		= $(OPENEEPR_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(OPENEEPR_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_PSM_DRV_DIR)/$(MODULE)
CONF_SRCDIR	= $(UTSBASE)/i86/io

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
