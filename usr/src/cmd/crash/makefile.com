#
# Copyright (c) 1997 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile.com	1.1	97/09/07 SMI"
#

PROG= crash

OBJS=	main.o base.o buf.o callout.o class.o cpu.o disp.o \
	init.o kma.o lck.o lock.o lwp.o map.o \
	nfs.o page.o pcfs.o prnode.o proc.o pty.o \
	rt.o search.o size.o snode.o status.o stream.o socket.o \
	symtab.o thread.o ts.o tty.o u.o ufs_inode.o util.o var.o vfs.o \
	vfssw.o vtop.o

SRCS=	$(OBJS:%.o=../%.c)

include ../../Makefile.cmd

LDLIBS += -lkvm -lelf -lnsl -ldl
CFLAGS += -v
CFLAGS64 += -v
CPPFLAGS += -D_KMEMUSER -D__$(MACH)

FILEMODE = 755
OWNER = root
GROUP = sys

#
#	Silly compatibility link from /etc/crash to the binary
#
ROOTETCLINK=	$(ROOTETC)/$(PROG)
REL_ETC_SBIN=	../usr/sbin


.KEEP_STATE:

.PARALLEL:	$(OBJS)

all: $(PROG) 

$(PROG): $(OBJS)
	$(LINK.c) $(OBJS) -o $@ $(LDLIBS)
	$(POST_PROCESS)

clean: 
	-$(RM) $(OBJS)

lint:	lint_SRCS

%.o:	../%.c
	$(COMPILE.c) $<

$(ROOTETCLINK):
	$(RM) $@; $(SYMLINK) $(REL_ETC_SBIN)/$(PROG) $@

include ../../Makefile.targ
