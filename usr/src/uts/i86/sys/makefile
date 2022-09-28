#
# Copyright (c) 1996-1997 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.55	97/11/12 SMI"
#
# uts/i86/sys/Makefile
#
# include global definitions
include ../../../Makefile.master

# NOTE: hrtcntl.h and hrtsys.h are present in this directory so that the
#	hrtsys system call can be built to facilitate transportability of
#	stock SVr4 programs.  Every effort is to be made to prevent objects
#	from being built, so these headers are not exported (installed).

HDRS=  \
	archsystm.h	asm_linkage.h	bootconf.h	\
	bootsvcs.h	cpu.h		\
	ddi_isa.h	debugreg.h	\
	dumphdr.h	fp.h		frame.h         \
	immu.h		inline.h        \
	machelf.h	\
	machlock.h	machsig.h	machtypes.h	\
	mmu.h		mutex_impl.h	obpdefs.h	\
	old_procfs.h	pcb.h		procfs_isa.h	\
	prom_isa.h	prom_plat.h	promif.h	promimpl.h	\
	psw.h		pte.h		reg.h		regset.h	\
	spl.h		stat.h		segment.h	\
	stack.h		synch32.h	sysconfig_impl.h sysi86.h	\
	trap.h	 	tss.h		ucontext.h	\
	vmparam.h	x86_archext.h

SUBHDRS=  \
	debug/debug.h		debug/debugger.h


ROOTDIR=	$(ROOT)/usr/include/sys
SCSIDIR=	$(ROOTDIR)/scsi
SCSIDIRS=	$(SCSIDIR) $(SCSIDIR)/conf $(SCSIDIR)/generic $(SCSIDIR)/impl
ROOTDIRS=	$(ROOTDIR) $(SCSIDIRS) $(ROOTDIR)/debug
ROOTHDRS= $(HDRS:%=$(ROOTDIR)/%) $(SUBHDRS:%=$(ROOTDIR)/%) 

CHECKHDRS= $(HDRS:%.h=%.check) $(SUBHDRS:%.h=%.check)

# install rules
$(ROOTDIR)/%: %
	$(INS.file)

.KEEP_STATE:

.PARALLEL: $(CHECKHDRS) $(ROOTHDRS)

install_h: $(ROOTDIRS) .WAIT $(ROOTHDRS)

$(ROOTDIRS):
	$(INS.dir)

check:	$(CHECKHDRS)
