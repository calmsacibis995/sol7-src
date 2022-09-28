#
#ident	"@(#)Makefile	1.11	98/02/04 SMI"
#
#	Copyright (c) 1996 by Sun Microsystems, Inc.
#	All rights reserved.

# Object lists are organized into primary (most frequently used code) and
# secondary lists (less frequently used code, ie. a.out support).

P_COMOBJS=	analyze.o	elf.o		globals.o	malloc.o \
		mutex.o		paths.o		setup.o		util.o \
		dlfcns.o	cache_elf.o	locale.o	profile.o \
		tsort.o		getcwd.o

S_COMOBJS=	debug.o		object.o	audit.o


G_MACHOBJS=	doreloc.o

P_MACHOBJS=	sparc_elf.o	_setup.o

S_MACHOBJS=	

P_ASOBJS=	boot.o		boot_elf.o	caller.o	zero.o

S_ASOBJS=	note.o

CRTI=		pics/crti.o
CRTN=		pics/crtn.o
CRTS=		$(CRTI)		$(CRTN)

include		$(SRC)/cmd/sgs/rtld/Makefile.com
include		$(SRC)/lib/Makefile.lib.64


# Tell chmsg.csh that we want a 64-bit architecture.

SGSMSGCHKFLAGS =	-64


# Add any machine specific flags.

CFLAGS +=	-xregs=no%appl
ASFLAGS +=	-s -K pic -D__sparcv9 -D_ELF64 $(sparcv9_XARCH)
#ADBGENFLAGS +=	-erroff=%all
CPPFLAGS +=	-D_ELF64
LINTFLAGS64 +=	-D__sparcv9

DYNFLAGS=	-i -h /usr/lib/sparcv9/ld.so.1 -z text $(ZDEFS)
DYNFLAGS +=	-e _rt_boot -Bsymbolic -zcombreloc -M $(MAPFILE) -R$(SGSRPATH64)

LIBPROTOS =	-L ../../../../../../proto/root_sparc/usr/lib/sparcv9
CPICLIB =	$(LIBPROTOS) -L $(ROOT)/usr/lib/pics/$(MACH64) -L /usr/lib/pics/$(MACH64)
LDLIB =		-L ../../libld/$(MACH64)
DBGLIB =	-L ../../liblddbg/$(MACH64)
RTLDLIB =	-L ../../librtld/$(MACH64)
CONVLIB =	-L ../../libconv/$(MACH64)

ROOTDYNLIB64=	$(RTLD:%=$(ROOTLIBDIR64)/%)


.KEEP_STATE:

all:		$(RTLD)

install:	all $(ROOTDYNLIB64)

adbmacros:	adb .WAIT $(ADBSCRIPTS)

adbinstall:	adbmacros .WAIT $(ROOTADB)

include		$(SRC)/cmd/sgs/rtld/Makefile.targ
include		../../Makefile.sub.64
