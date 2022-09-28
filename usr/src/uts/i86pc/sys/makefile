#
#ident	"@(#)Makefile	1.31	97/10/22 SMI"
#
# Copyright (c) 1996, by Sun Microsystems, Inc.
# All rights reserved.
#
# uts/i86pc/sys/Makefile
#
# include global definitions
include ../../../Makefile.master

HDRS=  \
	bpp_io.h	nvm.h		\
	clock.h		cram.h		ddi_subrdefs.h	eisarom.h	\
	machcpuvar.h	machparam.h	machthread.h	mcdma.h		\
	vm_machparam.h	xque.h		smp_impldefs.h	\
	asm_misc.h	rm_platter.h	x_call.h	xc_levels.h	\
	psm.h		psm_defs.h	psm_types.h	machsystm.h

ROOTDIR=	$(ROOT)/usr/include/sys
ROOTDIRS=	$(ROOTDIR)

ROOTHDRS= $(HDRS:%=$(ROOTDIR)/%)

CHECKHDRS= $(HDRS:%.h=%.check)

# install rules
$(ROOTDIR)/%: %
	$(INS.file)

.KEEP_STATE:

.PARALLEL: $(CHECKHDRS) $(ROOTHDRS)

install_h: $(ROOTDIRS) .WAIT $(ROOTHDRS)

$(ROOTDIRS):
	$(INS.dir)

check:	$(CHECKHDRS)
