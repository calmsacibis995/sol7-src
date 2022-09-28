#
# uts/i86pc/profile/Makefile
# Copyright (c) 1991-1994, by Sun Microsystems, Inc.
#
#ident	"@(#)Makefile	1.7	97/11/12 SMI"
#
#	This makefile only installs profile.conf (which is
#	platform dependent).
#
#	i86pc implementation architecture dependent
#

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Define the module and object file sets.  (Continue to need to define
#	ROOTMODULE to define the target for the .conf file, even though the
#	`ROOTMODULE' isn't created or installed by this Makefile).
#
MODULE		= profile
ROOTMODULE	= $(ROOT_PSM_DRV_DIR)/$(MODULE)
CONF_SRCDIR	= $(UTSBASE)/i86pc/io

#
#	Include common rules.
#
include $(UTSBASE)/i86pc/Makefile.i86pc

#
#	Default build targets.
#
INSTALL_TARGET	= $(ROOT_CONFFILE)

#
#	Default build targets.
#
.KEEP_STATE:

def all:	$(SRC_CONFILE)

install:	$(INSTALL_DEPS)

#
#	Include common targets.
#
include $(UTSBASE)/i86pc/Makefile.targ
