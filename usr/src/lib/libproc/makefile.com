#
# Copyright (c) 1997-1998 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile.com	1.2	98/01/29 SMI"
#
# lib/libproc/Makefile.com

LIBRARY = libproc.a
VERS = .1

OBJECTS =	\
	Pcontrol.o	\
	Pisprocdir.o	\
	Pservice.o	\
	Psymtab.o	\
	Pscantext.o	\
	pr_door.o	\
	pr_exit.o	\
	pr_fcntl.o	\
	pr_getitimer.o	\
	pr_getrlimit.o	\
	pr_ioctl.o	\
	pr_lseek.o	\
	pr_memcntl.o	\
	pr_mmap.o	\
	pr_open.o	\
	pr_rename.o	\
	pr_sigaction.o	\
	pr_stat.o	\
	pr_statvfs.o	\
	pr_waitid.o	\
	proc_dirname.o	\
	proc_get_info.o	\
	proc_names.o	\
	proc_pidarg.o	\
	proc_read_string.o \
	proc_stack_iter.o

# include library definitions
include ../../Makefile.lib

MAPFILE =	../common/mapfile-vers
SRCS =		$(OBJECTS:%.o=../common/%.c)

LIBS = $(DYNLIB) $(LINTLIB)

# definitions for lint

LINTFLAGS =	-mux -I../common
LINTFLAGS64 =	-mux -D__sparcv9 -I../common
LINTOUT =	lint.out

LINTSRC =	$(LINTLIB:%.ln=%)
ROOTLINTDIR =	$(ROOTLIBDIR)
ROOTLINT =	$(LINTSRC:%=$(ROOTLINTDIR)/%)

CLEANFILES =	$(LINTOUT) $(LINTLIB)

CFLAGS +=	-v -I../common
CFLAGS64 +=	-v -I../common
DYNFLAGS +=	-M $(MAPFILE) -M mapfile-vers
LDLIBS +=	-lrtld_db -lelf -lc

$(LINTLIB) :=	SRCS = ../common/llib-lproc
$(LINTLIB) :=	LINTFLAGS = -nvx -I../common
$(LINTLIB) :=	LINTFLAGS64 = -nvx -D__sparcv9 -I../common

.KEEP_STATE:

all: $(LIBS)

lint:
	$(LINT.c) $(SRCS) $(LDLIBS)

$(DYNLIB):	$(MAPFILE)

# include library targets
include ../../Makefile.targ

objs/%.o pics/%.o: ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

# install rule for lint library target
$(ROOTLINTDIR)/%: ../common/%
	$(INS.file)
