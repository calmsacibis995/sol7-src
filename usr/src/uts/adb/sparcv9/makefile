#
#ident	"@(#)Makefile	1.12	98/02/08 SMI"
#
# Copyright (c) 1997-1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# uts/adb/sparcv9/Makefile
#

ADB_BASE_DIR	= ..
ISADIR		= .
COMMONDIR	= $(ADB_BASE_DIR)/common
SYSDIR		= $(ADB_BASE_DIR)/..
NISADIR		= $(ADB_BASE_DIR)/sparc

MACROGEN= macrogen
MGENPP= mgenpp
MACTMP= ./.tmp

OBJS		= adbsub.o
CFLAGS64	+= -D__sparc -D__sparcv9 -D__sparcv9cpu

include $(SYSDIR)/Makefile.uts
include $(SYSDIR)/Makefile.uts.64

# NOTE: The following have been at least temporarily removed:
#	dir.dbg
#	dir.nxt.dbg
#	fpu.dbg
#	mount.dbg
#	pme.dbg
#	stat.dbg

# Macros common to all arch's.
#
#
DBGSRCS= \
	anon_map.dbg		\
	anon_hdr.dbg		\
	as.dbg			\
	bootobj.dbg		\
	buf.dbg			\
	bufctl.dbg		\
	bufctl_audit.dbg	\
	buflist.dbg		\
	buflist.nxt.dbg		\
	buflistiter.nxt.dbg	\
	cache_label.dbg		\
	cache_usage.dbg		\
	cachefs_workq.dbg	\
	cachefsfsc.dbg		\
	cachefsmeta.dbg		\
	callbparams.dbg		\
	callout.dbg		\
	callout.nxt.dbg		\
	callout_bucket.nxt.dbg	\
	callout_table.dbg	\
	callouts.dbg		\
	calltrace.dbg		\
	calltrace.nxt.dbg	\
	cb_ops.dbg		\
	cg.dbg			\
	cglist.dbg		\
	cglist.nxt.dbg		\
	cglistchk.nxt.dbg	\
	cglistiter.nxt.dbg	\
	cirbuf.dbg		\
	cnode.dbg		\
	cpu.dbg			\
	cpu_dptbl.dbg		\
	cpu_dptbl.nxt.dbg	\
	cpu_stat.dbg		\
	cpu_sysinfo.dbg		\
	cpu_syswait.dbg		\
	cpu_vminfo.dbg		\
	cpupart.dbg		\
	cpun.dbg		\
	cpus.dbg		\
	cred.dbg		\
	csum.dbg		\
	cpus.nxt.dbg		\
	dblk.dbg		\
	dev_ops.dbg		\
	devinfo.dbg		\
	dino.dbg		\
	direct.dbg		\
	disp.dbg		\
	dispq.dbg		\
	dispq.nxt.dbg		\
	dispqtrace.dbg		\
	dispqtrace.list.dbg	\
	dispqtrace.nxt.dbg	\
	door.dbg		\
	door_arg.dbg		\
	door_data.dbg		\
	dqblk.dbg		\
	dquot.dbg		\
	dumphdr.dbg		\
	edge.dbg		\
	eucioc.dbg		\
	exdata.dbg		\
	fifonode.dbg		\
	file.dbg		\
	filsys.dbg		\
	findthreads.dbg		\
	findthreads.nxt.dbg	\
	fnnode.dbg		\
	fs.dbg			\
	graph.dbg		\
	hash2ints.dbg		\
	hmelist.dbg		\
	ic_acl.dbg		\
	ifnet.dbg		\
	inode.dbg		\
	inodelist.dbg		\
	inodelist.nxt.dbg	\
	inodelistiter.nxt.dbg	\
	iocblk.dbg		\
	iovec.dbg		\
	ipc_perm.dbg		\
	itimerval.dbg		\
	kmastat.dbg		\
	kmastat.nxt.dbg		\
	ksiginfo.dbg		\
	kstat_char.dbg		\
	kstat_i32.dbg		\
	kstat_i64.dbg		\
	kstat_named.dbg		\
	kstat_ui32.dbg		\
	kstat_ui64.dbg		\
	kmem_cache.dbg		\
	kmem_cpu.dbg		\
	kmem_cpu.nxt.dbg	\
	kmem_cpu_log.dbg	\
	kmem_cpu_log.nxt.dbg	\
	kmem_log.dbg		\
	ldtermstd_state.dbg	\
	lock_descriptor.dbg	\
	lockfs.dbg		\
	loinfo.dbg		\
	lwp.dbg			\
	major.dbg		\
	map.dbg			\
	mblk.dbg		\
	mblk.nxt.dbg		\
	memlist.dbg		\
	memlist.list.dbg	\
	memlist.nxt.dbg		\
	ml_odunit.dbg		\
	ml_unit.dbg		\
	mntinfo.dbg		\
	modctl.dbg		\
	modctl.brief.dbg	\
	modctl_list.dbg		\
	modinfo.dbg		\
	modlinkage.dbg		\
	module.dbg		\
	modules.dbg		\
	modules.brief.dbg	\
	modules.brief.nxt.dbg	\
	modules.nxt.dbg		\
	mount.dbg		\
	mount.nxt.dbg		\
	msgbuf.wrap.dbg		\
	mt_map.dbg		\
	ncache.dbg		\
	netbuf.dbg		\
	opaque_auth.dbg		\
	page.dbg		\
	pathname.dbg		\
	pcb.dbg			\
	pid.dbg			\
	pid.print.dbg		\
	pid2proc.chain.dbg	\
	pid2proc.dbg		\
	polldat.dbg		\
	pollfd.dbg		\
	pollhead.dbg		\
	pollstate.dbg		\
	prcommon.dbg		\
	prgregset.dbg		\
	prnode.dbg		\
	proc.dbg		\
	proc_edge.dbg		\
	proc_tlist.dbg		\
	proc_tlist.nxt.dbg	\
	proc_vertex.dbg		\
	procargs.dbg		\
	procthreads.dbg		\
	procthreads.list.dbg	\
	proc2u.dbg		\
	putbuf.dbg		\
	putbuf.wrap.dbg		\
	qinit.dbg		\
	qproc.info.dbg		\
	qthread.info.dbg	\
	queue.dbg		\
	rlimit.dbg		\
	rlimit64.dbg		\
	rnode.dbg		\
	rpcerr.dbg		\
	rpctimer.dbg		\
	rtproc.dbg		\
	schedctl.dbg		\
	scsi_addr.dbg		\
	scsi_arq_status.dbg	\
	scsi_dev.dbg		\
	scsi_hba_tran.dbg	\
	scsi_pkt.dbg		\
	scsi_tape.dbg		\
	seg.dbg			\
	segdev.dbg		\
	segkp.dbg		\
	segkp_data.dbg		\
	seglist.dbg		\
	seglist.nxt.dbg		\
	segmap.dbg		\
	segvn.dbg		\
	sem.nxt.dbg		\
	semid.dbg		\
	session.dbg		\
	setproc.dbg		\
	setproc.done.dbg	\
	setproc.nop.dbg		\
	setproc.nxt.dbg		\
	shmid.dbg		\
	shminfo.dbg		\
	si.dbg			\
	sigaltstack.dbg		\
	slab.dbg		\
	sleepq.dbg		\
	sleepq.nxt.dbg		\
	slpqtrace.dbg		\
	slpqtrace.list.dbg	\
	slpqtrace.nxt.dbg	\
	smap.dbg		\
	smaphash.dbg		\
	snode.dbg		\
	sobj.dbg		\
	st_drivetype.dbg	\
	stack.dbg		\
	stackregs.dbg		\
	stacktrace.dbg		\
	stacktrace.nxt.dbg	\
	stat.dbg		\
	stat64.dbg		\
	stdata.dbg		\
	strtab.dbg		\
	svcfh.dbg		\
	syncq.dbg		\
	sysinfo.dbg		\
	tcpcb.dbg		\
	tcpip.dbg		\
	termios.dbg		\
	thread.dbg		\
	thread.brief.dbg	\
	thread.link.dbg		\
	thread.trace.dbg	\
	threadlist.dbg		\
	threadlist.link.dbg	\
	threadlist.nxt.dbg	\
	tmount.dbg		\
	tmpnode.dbg		\
	traceall.nxt.dbg	\
	tsdpent.dbg		\
	tsproc.dbg		\
	tune.dbg		\
	turnstile.dbg		\
	u.dbg			\
	u.sizeof.dbg		\
	ucalltrace.dbg		\
	ucalltrace.nxt.dbg	\
	ufchunk.dbg		\
	ufchunk.nxt.dbg		\
	ufs_acl.dbg		\
	ufs_acllist.dbg		\
	ufs_aclmask.dbg		\
	ufsq.dbg		\
	ufsvfs.dbg		\
	uio.dbg			\
	ulockfs.dbg		\
	ustack.dbg		\
	utsname.dbg		\
	v.dbg			\
	v_call.dbg		\
	v_proc.dbg		\
	vattr.dbg		\
	vfs.dbg			\
	vfslist.dbg		\
	vfslist.nxt.dbg		\
	vnode.dbg		\
	vpages.dbg		\
	vpages.nxt.dbg		\
	winsize.dbg		\
	xdr.dbg

# These sparc-specific macros existed in $(ISADIR) or $(COMMONDIR). Leave
# them here for now. Eventually, we should move everything to $(COMMONDIR),
# to reduce complication.
#
#
DBGSRCS += \
	glm.dbg			\
	glm_cmd.dbg		\
	glm_tagQ.dbg		\
	glm_dsa.dbg		\
	glm_unit.dbg		\
	le.dbg			\
	lestr.dbg		\
	stackcalls.dbg		\
	zscom.dbg

# This sparc-specific list existed only in $(ADB_BASE_DIR)/sparc. Pick
# them up from there for now. Eventually, even these should be moved
# to $(COMMONDIR).
#
#
SSDBGSRC= \
	ata_controller.dbg	\
	ata_drive.dbg		\
	ata_pkt.dbg		\
	cmd_ctl.dbg		\
	dcd_address.dbg		\
	dcd_cmd.dbg		\
	dcd_device.dbg		\
	dcd_disk.dbg		\
	dcd_hba_tran.dbg	\
	dcd_identify.dbg	\
	dcd_pkt.dbg		\
	dk_geom.dbg		\
	dk_label.dbg		\
	dk_vtoc.dbg		\
	esp.dbg			\
	esp_cb_info.dbg		\
	esp_cmd.dbg		\
	esp_tagQ.dbg		\
	fas.dbg			\
	fas_cmd.dbg		\
	fas_regs.dbg		\
	fas_tagQ.dbg		\
	ghd_cmd.dbg		\
	isp.dbg			\
	isp_cmd.dbg		\
	isp_request.dbg		\
	isp_response.dbg	\
	rwindow.dbg		\
	scsi_disk.dbg		\
	ssa_disk.dbg
DBGSRCS += $(SSDBGSRC)

SSDBGS= $(SSDBGSRC:.dbg=)
SCRIPTS64= $(DBGSRCS:.dbg=)


# compilation defines
#
DEFINES= -D_KERNEL -D$(MACH) -D__$(MACH) -D$(MACH64) -D__$(MACH64) -D_ELF64
INCDIR= ${SYSDIR}/common
INCS= -I$(SYSDIR)/$(MACH64) -I$(SYSDIR)/$(MACH) -I$(SYSDIR)/sun -I$(SRC)/head

# this is for macrogen to pickup dummy.h (to determine data model)
#
INCS += -I$(COMMONDIR)


# install paths and conditionals
#
ROOTUSRDIR  = $(ROOT)/usr
ROOTLIBDIR  = $(ROOTUSRDIR)/lib
ROOTADBDIR  = $(ROOTLIBDIR)/adb
ROOTADBDIR64  = $(ROOTADBDIR)/$(MACH64)

ROOTOBJS	= $(OBJS:%=$(ROOTADBDIR64)/%)
$(ROOTOBJS)	:= FILEMODE = 644

ROOTSCRIPTS64  = $(SCRIPTS64:%=$(ROOTADBDIR64)/%)
$(ROOTSCRIPTS64)	:= FILEMODE = 644


.KEEP_STATE:

.PARALLEL: $(OBJS)


# macrogen path
#
NATIVEMGENDIR  = $(ADB_BASE_DIR)/macrogen/${NATIVE_MACH}
NATIVEMACROGEN  = $(MACROGEN:%=$(NATIVEMGENDIR)/%)
NATIVEMGENPP	= $(MGENPP:%=$(NATIVEMGENDIR)/%)

# targets
#
all lint: $(OBJS) $(NATIVEMACROGEN) $(NATIVEMGENPP) $(MACTMP) .WAIT $(SCRIPTS64)

install: all .WAIT $(ROOTADBDIR64) .WAIT $(ROOTOBJS) .WAIT $(ROOTSCRIPTS64)

clean:
	-$(RM) $(OBJS)
	-$(RM) $(SCRIPTS64)
	@(cd $(NATIVEMGENDIR); pwd; $(MAKE) $@)


clobber: clean
	-$(RM) $(MACTMP)/*.res $(MACTMP)/*.tmp $(MACTMP)/*.s \
				    $(MACTMP)/*.c $(MACTMP)/*.tdbg
	@(cd $(NATIVEMGENDIR); pwd; $(MAKE) $@)


# installation rules
#
$(ROOTADBDIR64)/%: %
	$(INS.file)

$(ROOTUSRDIR) $(ROOTLIBDIR) $(ROOTADBDIR) $(ROOTADBDIR64):
	$(INS.dir.root.sys)

#
# rules to generate objects
#
adbsub.o:	$(COMMONDIR)/adbsub.c
	$(COMPILE.c) $(OUTPUT_OPTION) $(COMMONDIR)/adbsub.c
	$(POST_PROCESS_O)

# macrogen rules
#
$(NATIVEMACROGEN) + $(NATIVEMGENPP): FRC
	@(cd $(NATIVEMGENDIR); pwd; $(MAKE))

#
# rules to generate adb scripts using macrogen
#
# If it cannot find a match, grep returns 1 and make stops. For this
# reason, we need to ensure that both the grep's below always find
# a match (by explicitly including dummy.h in all dbg's).
#
%:	$(ISADIR)/%.dbg
	cat $< | $(NATIVEMGENPP) $(DEFINES) > $(MACTMP)/$@.tdbg
	grep '^#' $(MACTMP)/$@.tdbg > $(MACTMP)/$@.c
	$(CC) -O ${ARCHOPTS} $(DEFINES) $(INCS) $(CCYFLAG)$(INCDIR) \
		$(CFLAGS) -g -S -o $(MACTMP)/$@.s $(MACTMP)/$@.c
	grep -v '^#' $(MACTMP)/$@.tdbg > $(MACTMP)/$@.tmp
	$(NATIVEMACROGEN) $(MACTMP)/$@.tmp < $(MACTMP)/$@.s > $(MACTMP)/$@.res
	-$(RM) $(MACTMP)/$@.tmp $(MACTMP)/$@.c $(MACTMP)/$@.s \
		$(MACTMP)/$@.tdbg

%:	$(COMMONDIR)/%.dbg
	cat $< | $(NATIVEMGENPP) $(DEFINES) > $(MACTMP)/$@.tdbg
	grep '^#' $(MACTMP)/$@.tdbg > $(MACTMP)/$@.c
	$(CC) -O ${ARCHOPTS} $(DEFINES) $(INCS) $(CCYFLAG)$(INCDIR) \
		$(CFLAGS) -g -S -o $(MACTMP)/$@.s $(MACTMP)/$@.c
	grep -v '^#' $(MACTMP)/$@.tdbg > $(MACTMP)/$@.tmp
	$(NATIVEMACROGEN) $(MACTMP)/$@.tmp < $(MACTMP)/$@.s > $(MACTMP)/$@.res
	-$(RM) $(MACTMP)/$@.tmp $(MACTMP)/$@.c $(MACTMP)/$@.s \
		$(MACTMP)/$@.tdbg

$(SSDBGS):	$(NISADIR)/$$@.dbg
	cat $(NISADIR)/$@.dbg | $(NATIVEMGENPP) $(DEFINES) > $(MACTMP)/$@.tdbg
	grep '^#' $(MACTMP)/$@.tdbg > $(MACTMP)/$@.c
	$(CC) -O ${ARCHOPTS} $(DEFINES) $(INCS) $(CCYFLAG)$(INCDIR) \
		$(CFLAGS) -g -S -o $(MACTMP)/$@.s $(MACTMP)/$@.c
	grep -v '^#' $(MACTMP)/$@.tdbg > $(MACTMP)/$@.tmp
	$(NATIVEMACROGEN) $(MACTMP)/$@.tmp < $(MACTMP)/$@.s > $(MACTMP)/$@.res
	-$(RM) $(MACTMP)/$@.tmp $(MACTMP)/$@.c $(MACTMP)/$@.s \
		$(MACTMP)/$@.tdbg

check:

# macro list generation for kadb (platform-architecture specific)
#
maclist1:
	@(dir=`pwd`; \
	for i in $(SCRIPTS64); do \
		echo "$$dir $$i"; \
	done)

$(MACTMP):
	@pwd; mkdir -p $@

FRC:	
