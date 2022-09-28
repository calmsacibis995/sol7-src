#
# uts/i86pc/mmu36/Makefile
# Copyright (c) 1998 by Sun Microsystems, Inc.
#
#pragma ident	"@(#)Makefile	1.2	98/01/12 SMI"
#
#	This makefile drives the production of the i86 mmu36 driver module.
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
MODULE		= mmu36 
OBJECTS		= $(MMU36_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(MMU36_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_PSM_MMU_DIR)/$(MODULE)
MMU_DIR		= .


GENUNIX		= genunix
GENUNIX_DIR	= ../../i86/$(GENUNIX)

MMUOPTS		= -L ./$(OBJS_DIR) -l $(MMUNAME)

LIBOPTS		= -L $(GENUNIX_DIR)/$(OBJS_DIR) -l $(GENUNIX) $(MMUOPTS)
#
#	Include common rules.
#
include $(UTSBASE)/i86pc/Makefile.i86pc

#
#	Override defaults
#
CLEANFILES	+= $(MMULIB) $(SYM_MOD)

ALL_TARGET	= $(SYM_MOD)
LINT_TARGET	= $(MODULE).lint
INSTALL_TARGET	= def $(BINARY) $(ROOTMODULE)

#
# lint pass one enforcement
#
CFLAGS += -v -DPTE36

LINTFLAGS += -DPTE36

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

$(MMULIB):	$(BINARY)
	$(LD) -o $(MMULIB) -G $(BINARY)


$(SYM_MOD):	$(UNIX_O) $(MODSTUBS_O) $(LIBS) $(MMULIB)
	@echo "resolving symbols against unix.o"
	$(LD) -dy -b -o $(SYM_MOD) $(UNIX_O) $(MODSTUBS_O) $(LIBOPTS)


#	Include common targets.
#
include $(UTSBASE)/i86pc/Makefile.targ
