#
# Copyright (c) 1996-1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.167	98/01/30 SMI"
#
# cmd/Makefile
#
# include global definitions
include ../Makefile.master

#
#	The following command have been completely disabled inorder to
#	disable the sysadm build:
#	    bnu.admin   cfgintf     cfgmgmt     devintf     lp.admin
#	    nadmin.dfs  nadmin.nfs  netsel.adm  oamintf
#	    portmgmt
#
#	Note that 'lp' is first in the list, violating alphabetical
#	order.  This is because it is very long-running and should
#	be given the most wall-clock time for a parallel build.
#
#	Commands in the FIRST_SUBDIRS list are built before starting the
#	build of other commands.  Currently this includes only 'isaexec'.
#	This is necessary because $(ROOT)/usr/lib/isaexec must exist
#	when some other commands are built because their 'make install'
#	must create a hard link to $(ROOT)/usr/lib/isaexec.
#
#	Commands are listed one per line so that TeamWare can
#	auto-merge most changes.

FIRST_SUBDIRS=		\
	isaexec

COMMON_SUBDIRS=		\
	lp		\
	Adm		\
	acct		\
	adb		\
	agents		\
	arch		\
	asa		\
	aset		\
	asppp		\
	autopush	\
	awk		\
	awk_xpg4	\
	backup		\
	banner		\
	basename	\
	bc		\
	bdiff		\
	bfs		\
	bnu		\
	cal		\
	calendar	\
	captoinfo	\
	cat		\
	cfgadm		\
	checkeq		\
	checknr		\
	chgrp		\
	chmod		\
	chown		\
	chroot		\
	cksum		\
	clear		\
	cmd-inet	\
	cmp		\
	col		\
	comm		\
	compress	\
	cpio		\
	crash		\
	cron		\
	crypt		\
	csh		\
	csplit		\
	cut		\
	date		\
	dc		\
	dd		\
	deroff		\
	des		\
	devinfo		\
	devlinks	\
	devmgmt		\
	dfs.cmds	\
	diff		\
	diff3		\
	diffmk		\
	dircmp		\
	dirname		\
	dispadmin	\
	dmesg		\
	du		\
	dumpcs		\
	echo		\
	ed		\
	eeprom		\
	egrep		\
	env		\
	eqn		\
	expand		\
	expr		\
	exstr		\
	face		\
	factor		\
	false		\
	fdetach		\
	fdformat	\
	fdisk		\
	filesync	\
	fgrep		\
	file		\
	find		\
	fmli		\
	fmt		\
	fmthard		\
	fmtmsg		\
	fn		\
	fold		\
	format		\
	fs.d		\
	fstyp		\
	fuser		\
	gcore		\
	gencat		\
        genmsg		\
	getconf		\
	getent		\
	getfacl		\
	getmajor	\
	getopt		\
	gettext		\
	gettxt		\
	graph		\
	grep		\
	grep_xpg4	\
	groups		\
	grpck		\
	gss		\
	halt		\
	head		\
	hostid		\
	hostname	\
	iconv		\
	id		\
	infocmp		\
	init		\
	initpkg		\
	install.d	\
	iostat		\
	ipcrm		\
	ipcs		\
	isainfo		\
	isalist		\
	join		\
	kerbd		\
	keyserv		\
	kgmon		\
	killall		\
	ksh		\
	last		\
	lastcomm	\
	line		\
	link		\
	listen		\
	locale		\
	localedef	\
	localedef_xsh3	\
	lockstat	\
	logger		\
	login		\
	logins		\
	logname		\
	look		\
	ls		\
	mach		\
	machid		\
	mail		\
	mailx		\
	makekey		\
	man		\
	mesg		\
	mkdir		\
	mkfifo		\
	mkfile		\
	mkmsgs		\
	mknod		\
	modload		\
	more		\
	more_xpg4	\
	mpstat		\
	msgfmt		\
	mt		\
	mv		\
	mvdir		\
	netadm		\
	netfiles	\
	newform		\
	newgrp		\
	news		\
	nice		\
	nl		\
	nlsadmin	\
	nohup		\
	nsadmin		\
	nsadmin3b2	\
	nscd		\
	oampkg		\
	oamuser		\
	oawk		\
	od		\
	osadmin		\
	pack		\
	pagesize	\
	passmgmt	\
	passwd		\
	paste		\
	patch		\
	pathchk		\
	pax		\
	pbind		\
	pcmciad		\
	pgrep		\
	power		\
	pg		\
	pr		\
	print		\
	printf		\
	priocntl	\
	prtconf		\
	prtvtoc		\
	ps		\
	psradm		\
	psrinfo		\
	psrset		\
	pt_chmod	\
	ptools		\
	pwck		\
	pwconv		\
	pwd		\
	reboot		\
	refer		\
	regcmp		\
	renice		\
	rexd		\
	rm		\
	rmdir		\
	rmt		\
	rpcbind		\
	rpcgen		\
	rpcinfo		\
	rpcsvc		\
	rpld		\
	sa		\
	saf		\
	savecore	\
	script		\
	sdiff		\
	sed		\
	sed_xpg4	\
	sendmail	\
	setfacl		\
	setmnt		\
	setpgrp		\
	setuname	\
	sgs		\
	sh		\
	sleep		\
	soelim		\
	sort		\
	spell		\
	spline		\
	split		\
	srchtxt		\
	streams		\
	strings		\
	su		\
	sulogin		\
	sum		\
	sunpc		\
	swap		\
	sync		\
	sysdef		\
	syslogd		\
	tabs		\
	tail		\
	tar		\
	tbl		\
	tcopy		\
	tee		\
	terminfo	\
	tic		\
	time		\
	tip		\
	tnf		\
	touch		\
	tplot		\
	tput		\
	tr		\
	troff		\
	true		\
	truss		\
	tty		\
	ttymon		\
	uadmin		\
	ul		\
	uname		\
	uniq		\
	units		\
	unlink		\
	unpack		\
	users		\
	utmp_update	\
	utmpd		\
	valtools	\
	vgrind		\
	vi		\
	vmstat		\
	volmgt		\
	w		\
	wall		\
	wc		\
	which		\
	who		\
	whodo		\
	write		\
	xntpd		\
	xargs		\
	xstr		\
	ypcmd		\
	zdump		\
	zic		

i386_SUBDIRS=		\
	addbadsec	\
	diskscan	\
	rtc		

sparc_SUBDIRS=		\
	cvcd		\
	kbd		\
	loadkeys	\
	lux		\
	prtdiag		\
	ssa		\
	sx		

#
# commands that are messaged
# note that 'lp' comes first (see previous comment about 'lp')
#
MSGSUBDIRS=		\
	lp		\
	asa		\
	autopush	\
	awk		\
	awk_xpg4	\
	banner		\
	basename	\
	bnu		\
	cal		\
	cat		\
	cfgadm		\
	checkeq		\
	checknr		\
	chgrp		\
	chmod		\
	chown		\
	cksum		\
	cmd-inet	\
	cmp		\
	col		\
	comm		\
	compress	\
	cpio		\
	cron		\
	csh		\
	csplit		\
	cut		\
	date		\
	dc		\
	dd		\
	deroff		\
	devlinks	\
	diff		\
	diffmk		\
	du		\
	dumpcs		\
	ed		\
	env		\
	eqn		\
	expand		\
	expr		\
	fgrep		\
	file		\
	filesync	\
	find		\
	fold		\
	fs.d		\
        genmsg		\
	getconf		\
	getent		\
	gettext		\
	gettxt		\
	grep		\
	grep_xpg4	\
	grpck		\
	halt		\
	head		\
	hostname	\
	iconv		\
	id		\
	isaexec		\
	join		\
	ksh		\
	last		\
	locale		\
	logger		\
	logins		\
	logname		\
	ls		\
	mailx		\
	man		\
	mesg		\
	mkdir		\
	more		\
	more_xpg4	\
	mv		\
	newgrp		\
	nice		\
	nohup		\
	oampkg		\
	oawk		\
	od		\
	pack		\
	passwd		\
	passmgmt	\
	paste		\
	patch		\
	pathchk		\
	pax		\
	pg		\
	pr		\
	praudit		\
	print		\
	printf		\
	prtdiag		\
	ps		\
	pwconv		\
	pwd		\
	reboot		\
	refer		\
	regcmp		\
	renice		\
	rm		\
	rmdir		\
	script		\
	sed		\
	sed_xpg4	\
	sgs		\
	sh		\
	sleep		\
	sort		\
	split		\
	strings		\
	su		\
	sum		\
	tabs		\
	tail		\
	tar		\
	tbl		\
	tee		\
	time		\
	tnf		\
	touch		\
	tput		\
	tr		\
	troff		\
	tty		\
	ttymon		\
	ul		\
	uname		\
	uniq		\
	units		\
	unlink		\
	unpack		\
	vgrind		\
	vi		\
	volmgt		\
	w		\
	wc		\
	whodo		\
	write		\
	xargs		

#
# commands that use dcgettext for localized time, LC_TIME
#
DCSUBDIRS=		\
	cal		\
	ls		\
	pr		\
	tar		\
	w		\
	whodo		\
	write		

#
# commands that belong only to the basic security module
#
BSMSUBDIRS=		\
	allocate	\
	audit		\
	audit_warn	\
	auditconfig	\
	auditd		\
	auditreduce	\
	auditstat	\
	dminfo		\
	praudit		\
	bsmconv		\
	bsmunconv		

#
# commands not owned by the systems group
#
BWOSDIRS= 

all :=		TARGET= all
install :=	TARGET= install
install_h :=	TARGET= install_h
check :=	TARGET= check
clean :=	TARGET= clean
clobber :=	TARGET= clobber
lint :=		TARGET= lint
_msg :=		TARGET= _msg
_dc :=		TARGET= _dc

.KEEP_STATE:

SUBDIRS = $(COMMON_SUBDIRS)  $($(MACH)_SUBDIRS)

.PARALLEL:	$(BWOSDIRS) $(SUBDIRS) $(MSGSUBDIRS)  $(BSMSUBDIRS)

all install clean clobber lint: $(FIRST_SUBDIRS) .WAIT $(SUBDIRS) $(BSMSUBDIRS)

# those commands which install headers possibly needed by other commands.
install_h check:
	@cd mail; pwd; $(MAKE) $(TARGET)

_msg: $(MSGSUBDIRS) _dc

_dc: $(DCSUBDIRS)

$(FIRST_SUBDIRS) $(BWOSDIRS) $(SUBDIRS) $(BSMSUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
