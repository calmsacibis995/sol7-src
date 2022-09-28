#ident	"@(#)Makefile.com	1.5	97/08/28 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#
# lib/libcrypt/Makefile.com
#
LIBRARY=	libcrypt_d.a
VERS=		.1


OBJECTS.i= \
	crypt.o \
	cryptio.o

OBJECTS=  \
	$(OBJECTS.i) \
	des_crypt.o \
	des.o \
	des_soft.o

# include library definitions
include ../../Makefile.lib

MAPFILE=        ../common/mapfile-vers_d
SRCS=           $(OBJECTS:%.o=../common/%.c)

LIBS +=          $(DYNLIB) $(LINTLIB)

# one object has both an international and a domestic version;
# one version must not overwrite the other
#
OBJ.i= objs.i/des_crypt.o
OBJS.i= $(OBJECTS.i:%=objs.i/%) $(OBJ.i)
OBJS= $(OBJECTS:%=objs/%)
PIC.i= pics.i/des_crypt.o
PICS.i= $(OBJECTS.i:%=pics.i/%) $(PIC.i)
PICS= $(OBJECTS:%=pics/%)

# definitions for lint

LINTFLAGS=      -u -I../inc
LINTFLAGS64=    -u -I../inc
LINTOUT=        lint.out

LINTSRC=        $(LINTLIB:%.ln=%)
ROOTLINTDIR=    $(ROOTLIBDIR)
ROOTLINT=       $(LINTSRC:%=$(ROOTLINTDIR)/%)

CLEANFILES +=   $(LINTOUT) $(LINTLIB)

CFLAGS +=	-v -I../inc
CFLAGS64 +=	-v -I../inc
CPPFLAGS +=	-D_REENTRANT
CPPFLAGS64 +=	-D_REENTRANT
DYNFLAGS +=     -M $(MAPFILE)
LDLIBS +=       -lc
RM = rm -rf
CLOBBERFILES = $(LIB_INT_ST) $(LIB_INT_DY) $(OBJS.i) $(PICS.i)

LIB_INT_ST=libcrypt_i.a
LIB_INT_DY=libcrypt_i.so$(VERS)
$(OBJS.i):= CPPFLAGS += -DINTERNATIONAL
$(LIB_INT_ST):= AROBJS = $(OBJS.i)
$(LIB_INT_DY):= PICS = $(PICS.i)
$(LIB_INT_DY):= SONAME = $(LIB_INT_DY)
$(LIB_INT_DY):= MAPFILE= ../common/mapfile-vers_i
$(LIB_INT_DY):= DYNFLAGS = -h $(LIB_INT_DY) -z text 
$(LIB_INT_DY):= DYNFLAGS += -M $(MAPFILE)

$(PICS.i):= CPPFLAGS += -DINTERNATIONAL -DPIC -D_TS_ERRNO
$(PICS.i):= sparc_CFLAGS = -xregs=no%appl -K pic
$(PICS.i):= i386_CFLAGS = -K pic
$(PICS.i):= sparcv9_CFLAGS += -xregs=no%appl -K PIC
$(PICS.i):= CCFLAGS += -pic

.KEEP_STATE:

lint: $(LINTLIB)

# include library targets
include ../../Makefile.targ

objs/%.o pics/%.o: ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

#$(OBJ.i): objs.i
#	$(COMPILE.c) -o $@ ../common/$(@F:.o=.c)
#	$(POST_PROCESS_O)

#$(PIC.i): pic.i
#	$(COMPILE.c) -o $@ ../common/$(@F:.o=.c)
#	$(POST_PROCESS_O)

objs.i/%.o: objs.i ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

pics.i/%.o: pics.i ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

$(LIB_INT_ST): objs.i $(OBJS.i)
	$(BUILD.AR)
	$(POST_PROCESS_A)

$(LIB_INT_DY): pics.i $(PICS.i)
	$(BUILD.SO)
	$(POST_PROCESS_SO)

pics.i objs.i:
	-@mkdir -p $@

