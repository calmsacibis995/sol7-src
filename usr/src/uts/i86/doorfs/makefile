#
# uts/i86/doorfs/Makefile
# Copyright (c) 1991-1994, by Sun Microsystems, Inc.
#
#ident	"@(#)Makefile 97/11/12 1.3 SMI"
#
#	This makefile drives the production of the semsys driver kernel module.
#
#	x86 architecture dependent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

DOORFS_OBJS	+= door_vnops.o	door_sys.o door_support.o

#
#	Define the module and object file sets.
#
MODULE		= doorfs
OBJECTS		= $(DOORFS_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(DOORFS_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_SYS_DIR)/$(MODULE)

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
#	Overrides.
#
MODSTUBS_DIR	 = $(OBJS_DIR)
$(MODSTUBS_O)	:= AS_CPPFLAGS +=
CLEANFILES	+= $(MODSTUBS_O)
CFLAGS		+= -v

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

#
# rules
#
$(OBJS_DIR)/door_sys.o:      $(UTSBASE)/common/fs/doorfs/door_sys.c
	$(COMPILE.c) -o $@ $(UTSBASE)/common/fs/doorfs/door_sys.c

$(OBJS_DIR)/door_vnops.o:     $(UTSBASE)/common/fs/doorfs/door_vnops.c
	$(COMPILE.c) -o $@ $(UTSBASE)/common/fs/doorfs/door_vnops.c
 
$(OBJS_DIR)/door_support.o:   $(UTSBASE)/i86/os/door_support.c
	$(COMPILE.c) -o $@ $(UTSBASE)/i86/os/door_support.c

#
# Lint targets
#
$(LINTS_DIR)/door_sys.ln:    $(UTSBASE)/common/fs/doorfs/door_sys.c
	@($(LHEAD) $(LINT.c) $(UTSBASE)/common/fs/doorfs/door_sys.c $(LTAIL))
 
$(LINTS_DIR)/door_vnops.ln:   $(UTSBASE)/common/fs/doorfs/door_vnops.c
	@($(LHEAD) $(LINT.c) $(UTSBASE)/common/fs/doorfs/door_vnops.c $(LTAIL))
 
$(LINTS_DIR)/door_support.ln: $(UTSBASE)/i86/os/door_support.c
	@($(LHEAD) $(LINT.c) $(UTSBASE)/i86/os/door_support.c $(LTAIL))

