#
#ident	"@(#)Makefile 1.1 97/12/22 SMI"
#
# Copyright (c) 1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libthread_psr/sparc/sun4u/Makefile
#
# Platform specific Makefile for libthread_psr.
#
# PLATFORM is the target for the binary installation.
#
SRCDIR		= ../..
PLATFORM	= sun4u

OBJECTS		= _mutex.o

include		$(SRCDIR)/Makefile.com

MAPFILES=	mapfile-vers  $(MAPFILE-FLTR)
MAPOPTS=	$(MAPFILES:%=-M %)
DYNFLAGS +=	$(MAPOPTS)

ASFLAGS	+=	-xarch=v8plusa

# Redefine shared object build rule to use $(LD) directly (this avoids .init
# and .fini sections being added).  Because we use a mapfile to create a
# single text segment, hide the warning from ld(1) regarding a zero _edata.
 
BUILD.SO=	$(LD) -o $@ -G $(DYNFLAGS) $(PICS) $(LDLIBS) 2>&1 | \
		fgrep -v "No read-write segments found" | cat

$(DYNLIB):	$(MAPFILES)

#
#       The following must be defined for all implementations:
#
#       GENCONST_SRC:   genconst.c
#       OFFSETS:        offsets.in
#
GENCONST_SRC     = genconst.c
OFFSETS          = offsets.in

.KEEP_STATE:

all:		assym.h $(LIBS)

$(ASSYM_DEPS:%=pics/%):	assym.h
$(ASSYM_DEPS:%=objs/%):	assym.h

#
#       genconst build.
#
genconst:    $(GENCONST_SRC)
	$(NATIVECC) $(CFLAGS) $(CPPFLAGS) -I../../../libthread/common \
		-I../../../libthread/inc \
		-I../../../libthread/$(MACH)/ml \
		$(NATIVE_INC_PATH) \
		-o genconst $(GENCONST_SRC)

#
#       Execute genassym and genconst to produce assym.h.
#
assym.h:	./genconst offsets.out
	@cp offsets.out assym.h
	./genconst >> assym.h
	@rm offsets.out genconst

offsets.out: $(OFFSETS)
	@grep "^#" $(OFFSETS) > offsets.c
	$(CC) $(CPPFLAGS) -g -S \
		-I../../../libthread/common \
		-I../../../libthread/inc \
		-I../../../libthread/$(MACH)/ml \
		-o offsets.s offsets.c
	@grep -v "^#" $(OFFSETS) > offsets.tmp

	$(STABS) -t genassym -m ilp32 offsets.tmp \
		< offsets.s > offsets.out
	@rm offsets.s offsets.c \
		offsets.tmp

install:	all $(USR_PSM_LIBS)

include		$(SRCDIR)/Makefile.targ
