#
# Copyright (c) 1990-1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile.com	1.92	98/02/09 SMI"
#
# uts/adb/common/Makefile.com
#

PROGS		= adbgen adbgen1 adbgen3 adbgen4
OBJS		= adbsub.o
MACROGEN	= macrogen
MGENPP		= mgenpp
MACTMP		= ./.tmp

# NOTE: The following have been at least temporarily removed:
#	dir.adb
#	dir.nxt.adb
#	fpu.adb
#	mount.adb
#	pme.adb
#	stat.adb

SRCS += \
anon_hdr.dbg            anon_map.dbg            as.dbg \
bootobj.dbg             buf.dbg                 bufctl.dbg \
bufctl_audit.dbg        buflist.dbg             buflist.nxt.dbg \
buflistiter.nxt.dbg     cache_label.dbg         cache_usage.dbg \
cachefs_workq.dbg \
cachefsfsc.dbg          cachefsmeta.dbg         callbparams.dbg \
callout.dbg             callout.nxt.dbg         callout_bucket.nxt.dbg \
callout_table.dbg       callouts.dbg            calltrace.dbg \
calltrace.nxt.dbg \
cb_ops.dbg              cg.dbg                  cglist.dbg \
cglist.nxt.dbg          cglistchk.nxt.dbg       cglistiter.nxt.dbg \
cirbuf.dbg \
cnode.dbg               cpu.dbg                 cpu_dptbl.dbg \
cpu_dptbl.nxt.dbg       cpu_stat.dbg \
cpu_sysinfo.dbg         cpu_syswait.dbg \
cpu_vminfo.dbg          cpun.dbg                cpupart.dbg \
cpus.dbg                cpus.nxt.dbg            cred.dbg \
csum.dbg                dblk.dbg                dev_ops.dbg \
devinfo.dbg             dino.dbg                direct.dbg \
disp.dbg                dispq.dbg               dispq.nxt.dbg \
dispqtrace.dbg          dispqtrace.list.dbg     dispqtrace.nxt.dbg \
door.dbg                door_arg.dbg            door_data.dbg \
dqblk.dbg               dquot.dbg               dumphdr.dbg \
edge.dbg                eucioc.dbg              exdata.dbg \
fifonode.dbg            file.dbg                filsys.dbg \
findthreads.dbg         findthreads.nxt.dbg     fnnode.dbg \
fs.dbg                  graph.dbg               hash2ints.dbg \
hmelist.dbg             ic_acl.dbg              ifnet.dbg \
inode.dbg               inodelist.dbg           inodelist.nxt.dbg \
inodelistiter.nxt.dbg   iocblk.dbg              iovec.dbg \
ipc_perm.dbg            itimerval.dbg           kmastat.dbg \
kmastat.nxt.dbg         kmem_cache.dbg          kmem_cpu.dbg \
kmem_cpu.nxt.dbg        kmem_cpu_log.dbg        kmem_cpu_log.nxt.dbg \
kmem_log.dbg            ksiginfo.dbg            kstat_char.dbg \
kstat_i32.dbg           kstat_i64.dbg           kstat_named.dbg \
kstat_ui32.dbg          kstat_ui64.dbg          ldtermstd_state.dbg \
lock_descriptor.dbg     lockfs.dbg              loinfo.dbg \
lwp.dbg                 major.dbg               map.dbg \
mblk.dbg \
mblk.nxt.dbg            memlist.dbg             memlist.list.dbg \
memlist.nxt.dbg         ml_unit.dbg             ml_odunit.dbg \
mntinfo.dbg             modctl.dbg \
modctl.brief.dbg        modctl_list.dbg         modinfo.dbg \
modlinkage.dbg          module.dbg              modules.dbg \
modules.brief.dbg       modules.brief.nxt.dbg   modules.nxt.dbg \
mount.dbg               mount.nxt.dbg           msgbuf.wrap.dbg \
mt_map.dbg \
ncache.dbg              netbuf.dbg              opaque_auth.dbg \
page.dbg                pathname.dbg            pcb.dbg \
pid.dbg                 pid.print.dbg           pid2proc.dbg \
pid2proc.chain.dbg      polldat.dbg \
pollfd.dbg              pollhead.dbg            pollstate.dbg \
prcommon.dbg \
prgregset.dbg           prnode.dbg              proc.dbg \
proc2u.dbg              proc_edge.dbg           proc_tlist.dbg \
proc_tlist.nxt.dbg      proc_vertex.dbg         procargs.dbg \
procthreads.dbg         procthreads.list.dbg    putbuf.dbg \
putbuf.wrap.dbg         qinit.dbg               qproc.info.dbg \
qthread.info.dbg        queue.dbg               rlimit.dbg \
rlimit64.dbg            rnode.dbg               rpcerr.dbg \
rpctimer.dbg            rtproc.dbg              schedctl.dbg \
scsi_addr.dbg           scsi_arq_status.dbg     scsi_dev.dbg \
scsi_hba_tran.dbg       scsi_pkt.dbg            scsi_tape.dbg \
seg.dbg                 segdev.dbg              segkp.dbg \
segkp_data.dbg \
seglist.dbg             seglist.nxt.dbg         segmap.dbg \
segvn.dbg               sem.nxt.dbg             semid.dbg \
session.dbg             setproc.dbg             setproc.done.dbg \
setproc.nop.dbg         setproc.nxt.dbg         shmid.dbg \
shminfo.dbg \
si.dbg                  sigaltstack.dbg         slab.dbg \
sleepq.dbg              sleepq.nxt.dbg          slpqtrace.dbg \
slpqtrace.list.dbg      slpqtrace.nxt.dbg       smap.dbg \
smaphash.dbg            snode.dbg               sobj.dbg \
st_drivetype.dbg        stack.dbg \
stackregs.dbg           stacktrace.dbg          stacktrace.nxt.dbg \
stat.dbg                stat64.dbg              stdata.dbg \
strtab.dbg              svcfh.dbg               syncq.dbg \
sysinfo.dbg             tcpcb.dbg               tcpip.dbg \
termios.dbg             thread.dbg              thread.brief.dbg \
thread.link.dbg         thread.trace.dbg        threadlist.dbg \
threadlist.link.dbg     threadlist.nxt.dbg      tmount.dbg \
tmpnode.dbg             traceall.nxt.dbg        tsdpent.dbg \
tsproc.dbg              tune.dbg \
turnstile.dbg           u.dbg                   u.sizeof.dbg \
ucalltrace.dbg          ucalltrace.nxt.dbg      ufchunk.dbg \
ufchunk.nxt.dbg         ufs_acl.dbg             ufs_acllist.dbg \
ufs_aclmask.dbg         ufsq.dbg                ufsvfs.dbg \
uio.dbg                 ulockfs.dbg             ustack.dbg \
utsname.dbg             v.dbg                   v_call.dbg \
v_proc.dbg              vattr.dbg               vfs.dbg \
vfslist.dbg             vfslist.nxt.dbg         vnode.dbg \
vpages.dbg              vpages.nxt.dbg          winsize.dbg \
xdr.dbg

SCRIPTS		= $(SRCS:.dbg=)

include $(ADB_BASE_DIR)/../Makefile.uts

# Following grossness is added because the x86 people can't follow the
# naming guidelines...
# Should be simply:
# INCLUDES	= -I${SYSDIR}/${MACH} -I${SYSDIR}/sun
INCLUDES-i386	= -I${SYSDIR}/i86
INCLUDES-sparc	= -I${SYSDIR}/${MACH} -I${SYSDIR}/sun
INCLUDES	= ${INCLUDES-${MACH}}
INCDIR		= ${SYSDIR}/common
NATIVEDEFS	= -D${MACH} -D__${MACH} -D_KERNEL

# to pick up dummy.h for macrogen
INCLUDES += -I$(COMMONDIR)

ROOTUSRDIR	= $(ROOT)/usr
ROOTLIBDIR	= $(ROOTUSRDIR)/lib
ROOTADBDIR	= $(ROOTLIBDIR)/adb

ROOTPROGS	= $(PROGS:%=$(ROOTADBDIR)/%)
ROOTOBJS	= $(OBJS:%=$(ROOTADBDIR)/%)
ROOTSCRIPTS	= $(SCRIPTS:%=$(ROOTADBDIR)/%)

LDLIBS 		= $(ENVLDLIBS1)  $(ENVLDLIBS2)  $(ENVLDLIBS3)
LDFLAGS 	= $(STRIPFLAG) $(ENVLDFLAGS1) $(ENVLDFLAGS2) $(ENVLDFLAGS3)
CPPFLAGS	= $(CPPFLAGS.master)

$(ROOTOBJS)	:= FILEMODE = 644
$(ROOTSCRIPTS)	:= FILEMODE = 644

.KEEP_STATE:

MACH_FLAG=	__$(MACH)
NATIVEDEFS-i386=
NATIVEDEFS-sparc=
NATIVEDEFS =	-D${MACH} -D_KERNEL
NATIVEDEFS +=	${NATIVEDEFS-${MACH}}
NATIVEDIR	= $(ADB_BASE_DIR)/native/${NATIVE_MACH}
NATIVEPROGS	= $(PROGS:%=$(NATIVEDIR)/%)
NATIVEOBJS	= $(OBJS:%=$(NATIVEDIR)/%)

NATIVEMGENDIR	= $(ADB_BASE_DIR)/macrogen/${NATIVE_MACH}
NATIVEMACROGEN	= $(MACROGEN:%=$(NATIVEMGENDIR)/%)
NATIVEMGENPP	= $(MGENPP:%=$(NATIVEMGENDIR)/%)

.PARALLEL: $(PROGS) $(NATIVEPROGS) $(OBJS) $(NATIVEOBJS) $(SCRIPTS)

all lint: $(PROGS) $(OBJS) \
	$(NATIVEDIR) .WAIT \
	$(NATIVEPROGS) $(NATIVEOBJS) .WAIT \
	$(NATIVEMACROGEN) $(NATIVEMGENPP) .WAIT \
	$(MACTMP) .WAIT \
	$(SCRIPTS)

install: all .WAIT $(ROOTADBDIR) .WAIT $(ROOTPROGS) $(ROOTOBJS) $(ROOTSCRIPTS)

clean:
	-$(RM) $(OBJS) $(NATIVEOBJS)
	@(cd $(NATIVEMGENDIR); pwd; $(MAKE) $@)

clobber: clean
	-$(RM) $(PROGS) $(NATIVEPROGS)
	-$(RM) $(SCRIPTS)
	-$(RM) $(MACTMP)/*.tdbg $(MACTMP)/*.c $(MACTMP)/*.s \
					$(MACTMP)/*.tmp $(MACTMP)/*.res
	@(cd $(NATIVEMGENDIR); pwd; $(MAKE) $@)

# installation things

$(ROOTADBDIR)/%: %
	$(INS.file)

$(ROOTUSRDIR) $(ROOTLIBDIR) $(ROOTADBDIR):
	$(INS.dir)

# specific build rules

adbgen:		$(COMMONDIR)/adbgen.sh
	$(RM) $@
	cat $(COMMONDIR)/adbgen.sh >$@
	$(CHMOD) +x $@

adbgen%:	$(COMMONDIR)/adbgen%.c
	$(LINK.c) -o $@ $< $(LDLIBS)
	$(POST_PROCESS)

adbsub.o:	$(COMMONDIR)/adbsub.c
	$(COMPILE.c) $(OUTPUT_OPTION) $(COMMONDIR)/adbsub.c
	$(POST_PROCESS_O)

$(NATIVEDIR):
	$(INS.dir)

$(NATIVEDIR)/adbgen:	$(COMMONDIR)/adbgen.sh
	$(RM) $@
	cat $(COMMONDIR)/adbgen.sh >$@
	$(CHMOD) +x $@

$(NATIVEDIR)/adbgen%:	$(COMMONDIR)/adbgen%.c
	$(NATIVECC) -O -o $@ $<
	$(POST_PROCESS)

$(NATIVEDIR)/adbsub.o:	$(COMMONDIR)/adbsub.c $(NATIVEDIR)
	$(NATIVECC) -c -o $@ $(COMMONDIR)/adbsub.c
	$(POST_PROCESS_O)

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
	cat $< | $(NATIVEMGENPP) $(NATIVEDEFS) > $(MACTMP)/$@.tdbg
	grep '^#' $(MACTMP)/$@.tdbg > $(MACTMP)/$@.c
	$(CC) -O ${ARCHOPTS} $(NATIVEDEFS) $(INCLUDES) \
		$(CCYFLAG)$(INCDIR) -g -S -o $(MACTMP)/$@.s $(MACTMP)/$@.c
	grep -v '^#' $(MACTMP)/$@.tdbg > $(MACTMP)/$@.tmp
	$(NATIVEMACROGEN) $(MACTMP)/$@.tmp < $(MACTMP)/$@.s > $(MACTMP)/$@.res
	-$(RM) $(MACTMP)/$@.tmp $(MACTMP)/$@.c $(MACTMP)/$@.s \
		$(MACTMP)/$@.tdbg

%:	$(COMMONDIR)/%.dbg
	cat $< | $(NATIVEMGENPP) $(NATIVEDEFS) > $(MACTMP)/$@.tdbg
	grep '^#' $(MACTMP)/$@.tdbg > $(MACTMP)/$@.c
	$(CC) -O ${ARCHOPTS} $(NATIVEDEFS) $(INCLUDES) \
		$(CCYFLAG)$(INCDIR) -g -S -o $(MACTMP)/$@.s $(MACTMP)/$@.c
	grep -v '^#' $(MACTMP)/$@.tdbg > $(MACTMP)/$@.tmp
	$(NATIVEMACROGEN) $(MACTMP)/$@.tmp < $(MACTMP)/$@.s > $(MACTMP)/$@.res
	-$(RM) $(MACTMP)/$@.tmp $(MACTMP)/$@.c $(MACTMP)/$@.s \
		$(MACTMP)/$@.tdbg

check:
	@echo $(SCRIPTS) | tr ' ' '\012' | sed 's/$$/&.dbg/' |\
		sort > script.files
	@(cd $(ADB_BASE_DIR); ls *.dbg) > actual.files
	diff script.files actual.files
	-$(RM) script.files actual.files

# the macro list is platform-architecture specific too.
maclist1:
	@(dir=`pwd`; \
	for i in $(SCRIPTS); do \
		echo "$$dir $$i"; \
	done)

$(MACTMP):
	@pwd; mkdir -p $@

FRC:
