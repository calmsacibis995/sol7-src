#
# uts/i86/genunix/Makefile
# Copyright (c) 1991 by Sun Microsystems, Inc.
#
#ident	"@(#)Makefile	1.5	97/11/12 SMI"
#
#	This makefile drives the production of the generic
#	unix kernel module.
#
#	i86 implementation architecture dependent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Define the module and object file sets.
#
MODULE		= genunix

OBJECTS		= $(GENUNIX_OBJS:%=$(OBJS_DIR)/%) \
		  $(NOT_YET_KMODS:%=$(OBJS_DIR)/%)

LINTS		= $(GENUNIX_OBJS:%.o=$(LINTS_DIR)/%.ln) \
		  $(NOT_YET_KMODS:%.o=$(LINTS_DIR)/%.ln)

ROOTMODULE	= $(ROOT_MOD_DIR)/$(MODULE)

LIBGEN		= $(OBJS_DIR)/libgenunix.so
LIBSTUBS	= $(GENSTUBS_OBJS:%=$(OBJS_DIR)/%)
SONAME		= $(MODULE)

#
#	Include common rules.
#
include $(UTSBASE)/i86/Makefile.i86

#
#	Define targets
#
ALL_TARGET	= $(LIBGEN)
LINT_TARGET	= $(MODULE).lint
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE)

#
#	Overrides
#
CLEANFILES	+= $(LIBSTUBS) $(LIBGEN)

#
# lint pass one enforcement
#
CFLAGS		+= -v

#
#	Default build targets.
#
.KEEP_STATE:

.PARALLEL:	$(LIBSTUBS)

def:		$(DEF_DEPS)

all:		$(ALL_DEPS)

clean:		$(CLEAN_DEPS)

clobber:	$(CLOBBER_DEPS)

lint:		$(LINT_DEPS)

modlintlib:	$(MODLINTLIB_DEPS)

clean.lint:	$(CLEAN_LINT_DEPS)

install:	$(INSTALL_DEPS)

$(LIBGEN):	$(BINARY) $(LIBSTUBS)
		$(LD) -o $@ -G -h $(SONAME) $(BINARY) $(LIBSTUBS)

#
#	Include common targets.
#
include $(UTSBASE)/i86/Makefile.targ

#
# Override.
#
$(MODULE).lint := GEN_LINT_LIB =
