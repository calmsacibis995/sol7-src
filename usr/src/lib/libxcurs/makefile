#
#pragma ident	"@(#)Makefile 1.9	97/05/02 SMI"
#
# Copyright (c) 1995-1996 Sun Microsystems, Inc. All rights reserved.
#
# lib/libxcurses/Makefile
#
# This builds the X/Curses (SPEC 1170) version of libcurses
#
LIBRARY= libcurses.a
VERS=    .1

# objects are grouped by source directory
# all of the libxcurses source files are in src/libc

# XCURSES source files are in src/libc/xcurses
XCURSES= \
add_wch.o	dupwin.o	longname.o	strname.o	wbrdr.o \
addch.o		echo_wch.o	m_cc.o		termattr.o	wbrdr_st.o \
addchn.o	echochar.o	meta.o		tgetent.o	wchgat.o \
addnstr.o	endwin.o	move.o		tgetflag.o	wclear.o \
addnws.o	flushinp.o	mvcur.o		tgetnum.o	wclrbot.o \
addwchn.o	get_wch.o	mvwin.o		tgetstr.o	wclreol.o \
attr_on.o	getcchar.o	napms.o		tgoto.o		wdelch.o \
attron.o	getch.o		newpad.o	tigetfla.o	wget_wch.o \
baudrate.o	getn_ws.o	newterm.o	tigetnum.o	wgetch.o \
beep.o		getnstr.o	newwin.o	tigetstr.o	wgetn_ws.o \
bkgd.o		getwin.o	noecho.o	timeout.o	whln.o \
bkgdset.o	has.o		nonl.o		touched.o	whln_st.o \
bkgrnd.o	hln.o		numcode.o	touchwin.o	win_wch.o \
bkgrndst.o	hln_st.o	numfnam.o	tparm.o		win_wchn.o \
boolcode.o	in_wch.o	numname.o	tputs.o		winch.o \
boolfnam.o	in_wchn.o	overlay.o	trace.o		winchn.o \
boolname.o	inch.o		pecho_wc.o	unctrl.o	winnstr.o \
box.o		inchn.o		pechoch.o	vid_attr.o	winnwstr.o \
box_set.o	initscr.o	prefresh.o	vid_puts.o	wins_nws.o \
brdr.o		innstr.o	printw.o	vidattr.o	wins_wch.o \
brdr_st.o	innwstr.o	ptrmove.o	vw_print.o	winsch.o \
cbreak.o	ins_nws.o	qiflush.o	vw_scanw.o	winsdel.o \
chgat.o		ins_wch.o	redraw.o	vwprintw.o	winsnstr.o \
clear.o		insch.o		refresh.o	vwscanw.o	wmove.o \
clearok.o	insnstr.o	savetty.o	wacs.o		wredraw.o \
clrbot.o	intrflsh.o	scanw.o		wadd_wch.o	wrefresh.o \
clreol.o	isblank.o	scr_dump.o	waddch.o	wscrl.o \
color.o		isendwin.o	scrl.o		waddchn.o	wscrreg.o \
copywin.o	key_name.o	scrreg.o	waddnstr.o	wsyncdn.o \
curs_set.o	keyindex.o	setcchar.o	waddnws.o	wsyncup.o \
delay.o		keyname.o	setup.o		waddwchn.o	wtimeout.o \
delch.o		keypad.o	slk.o		wattr_on.o	wtouchln.o \
deleteln.o	killchar.o	strcode.o	wattron.o	wunctrl.o \
doupdate.o	killwch.o	strfnam.o	wbkgrnd.o

# GEN source files are in src/libc/gen
GEN= \
base.o		eprintf.o

# I18N source files are in src/libc/i18n
I18N= \
m_strmsg.o	m_text.o

# MKS source files are in src/libc/mks
MKS= \
m_cmdnam.o	m_errorx.o	m_ord.o		m_strdup.o \
m_crcpos.o	m_getenv.o	m_pathca.o	m_vsscan.o \
m_error.o	m_malloc.o	m_pathma.o

# NARROW source files are in src/libc/narrow
NARROW= \
#wctomb.o

# STDIO source files are in src/libc/stdio
STDIO= \
vfscanf.o

# WIDE source files are in src/libc/wide
WIDE= \
m_mbstow.o	m_wcsdup.o	wio_get.o	wio_put.o

OBJECTS= \
$(XCURSES)	$(GEN)		$(I18N)		$(MKS) \
$(NARROW)	$(STDIO)	$(WIDE)

# capability NAMES generated source
NAMES= \
src/libc/xcurses/boolcode.c	src/libc/xcurses/boolfnam.c \
src/libc/xcurses/boolname.c	src/libc/xcurses/numcode.c \
src/libc/xcurses/numfnam.c	src/libc/xcurses/numname.c \
src/libc/xcurses/strcode.c	src/libc/xcurses/strfnam.c \
src/libc/xcurses/strname.c

GENERATED= \
$(NAMES)	src/libc/xcurses/keyindex.c	h/term.h

# build rules for source held in a variety of directories
objs/%.o pics/%.o:	src/libc/xcurses/%.c h/term.h
	$(COMPILE.c) -Isrc/libc/xcurses -o $@ $<
	$(POST_PROCESS_O)

objs/%.o pics/%.o:	src/libc/gen/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o pics/%.o:	src/libc/i18n/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o pics/%.o:	src/libc/mks/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o pics/%.o:	src/libc/narrow/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o pics/%.o:	src/libc/stdio/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

objs/%.o pics/%.o:	src/libc/wide/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

# include common library definitions
include ../Makefile.lib

LIBS=		$(DYNLIB) $(LINTLIB)

MAPFILE=	mapfile-vers
MAPOPTS=	$(MAPFILE:%=-M %)
DYNFLAGS +=	$(MAPOPTS)

LDLIBS += -ldl -lc
CPPFLAGS= -Ih -Ih/solaris2 $(CPPFLAGS.master)

# definitions for install target
ROOTLIBDIR=	$(ROOT)/usr/xpg4/lib
ROOTLIBS=	$(LIBS:%=$(ROOTLIBDIR)/%)
LINTSRC=	$(LINTLIB:%.ln=%)
ROOTLINTDIR=	$(ROOTLIBDIR)
ROOTLINT=	$(LINTSRC:%=$(ROOTLINTDIR)/%)

# definitions for install_h target
HDRS=		h/term.h h/curses.h h/unctrl.h
ROOTHDRDIR=	$(ROOT)/usr/xpg4/include
ROOTHDRS=	$(HDRS:h/%=$(ROOTHDRDIR)/%)

# install rule for install target
$(ROOTLIBDIR)/%: %
	$(INS.file)

# install rule for lint target
$(ROOTLINTDIR)/%: src/libc/%
	$(INS.file)

# install rule for install_h target
$(ROOTHDRDIR)/%: h/%
	$(INS.file)

# extra clean-up work
CLOBBERFILES=	$(GENERATED)

$(DYNLIB):	$(MAPFILE)

.KEEP_STATE:

all: $(LIBS) $(ROOTLINT)

$(LINTLIB):= SRCS=src/libc/llib-lcurses
$(LINTLIB):= LINTFLAGS=-nvx
$(LINTLIB):= TARGET_ARCH=
$(LINTLIB):= CPPFLAGS += -D_XOPEN_SOURCE_EXTENDED 

install: all .WAIT $(ROOTLIBS) $(ROOTLINKS)

install_h: $(HDRS) .WAIT $(ROOTHDRS)

# still need to implement cstyle check of term.h
# do this when we go through and make this library cstyle clean.
check: $(HDRS)

# include common library targets
include ../Makefile.targ

# derived C files
# mknames.awk defines functions so needs nawk
$(NAMES) : src/libc/xcurses/mknames.awk src/libc/xcurses/caps
	$(RM) $(NAMES)
	cd src/libc/xcurses; nawk -f mknames.awk caps
 
h/term.h : src/libc/xcurses/mkterm.awk src/libc/xcurses/caps
	$(RM) $@
	cd src/libc/xcurses; awk -f mkterm.awk caps > term.h
	$(MV) src/libc/xcurses/term.h h/term.h

src/libc/xcurses/keyindex.c : src/libc/xcurses/mkkey.awk src/libc/xcurses/caps
	$(RM) $@
	cd src/libc/xcurses; nawk -f mkkey.awk caps > keyindex.c
