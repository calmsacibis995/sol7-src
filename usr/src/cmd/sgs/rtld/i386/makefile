#
#ident	"@(#)Makefile	1.20	97/04/19 SMI"
#
#	Copyright (c) 1996 by Sun Microsystems, Inc.
#	All rights reserved.

# Object lists are organized into primary (most frequently used code) and
# secondary lists (less frequently used code, ie. profiling support).

P_COMOBJS=	analyze.o	elf.o		globals.o	malloc.o \
		mutex.o		paths.o		setup.o		util.o \
		dlfcns.o	cache_elf.o	locale.o	profile.o\
		tsort.o		getcwd.o

S_COMOBJS=	debug.o		object.o	audit.o

G_MACHOBJS=	doreloc.o

P_MACHOBJS=	i386_elf.o	_setup.o	zero.o

S_MACHOBJS=

P_ASOBJS=	boot.o		boot_elf.o	caller.o

S_ASOBJS=	note.o

include		$(SRC)/cmd/sgs/rtld/Makefile.com

.KEEP_STATE:

all:		$(RTLD)

install:	all $(ROOTDYNLIB) $(ETCDYNLIB)

adbmacros:	adb .WAIT $(ADBSCRIPTS)

adbinstall:	adbmacros .WAIT $(ROOTADB)

include		$(SRC)/cmd/sgs/rtld/Makefile.targ
