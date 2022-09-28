#
# Copyright (c) 1997-1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident   "@(#)Makefile 1.7     98/02/08 SMI"
#
# uts/adb/platform/sparcv9/sun4u/Makefile
#

MACROGENDIR=	../../../macrogen
ADB_BASE_DIR =  ../../..
ADB_PLAT_DIR =  ../..
COMMONDIR =     $(ADB_BASE_DIR)/common

KARCH =         sun4u
PLATFORM =      $(KARCH)
ARCHOPTS =      -DMP -D__sparcv9cpu

INCLUDES =      -I${SYSDIR}/${KARCH} -I${SYSDIR}/srmmu \
                -I${SYSDIR}/${MACH} -I${SYSDIR}/${MACH}/v9 -I${SYSDIR}/sun

SRCS= adaptive_mutex.dbg machpcb.dbg msgbuf.dbg mutex.dbg regs.dbg \
	rwlock.dbg sema.dbg spin_mutex.dbg
DUMMIES= ctx hmetopmgrp pmgrp pmgseg

include $(ADB_PLAT_DIR)/Makefile.psm

# v9 includes and defines
#
include $(SYSDIR)/Makefile.uts.64
NATIVEDEFS += -D$(MACH64) -D__$(MACH64)

ISADIR =        $(ADB_BASE_DIR)/sparcv9

.KEEP_STATE:

.PARALLEL:      $(SCRIPTS) $(DUMMIES)

all: $(MACTMP) .WAIT $(SCRIPTS) $(DUMMIES)

install: all $(USR_PSM_ADB_DIR_64) .WAIT $(ROOTSCRIPTS64) $(ROOTDUMMIES64)

include $(ADB_PLAT_DIR)/Makefile.targ
