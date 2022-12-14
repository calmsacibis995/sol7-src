#
#ident	"@(#)Makefile.com	1.1	97/11/19 SMI"
#
# Copyright (c) 1997, by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/gss_mechs/mech_dh/dh1024.so
#
#
# This make file will build dh1024.so.1. This shared object
# contains the functionality needed to initialize the  Diffie-Hellman GSS-API
# mechanism with 1024 bit key length. This library, in turn, loads the 
# generic Diffie-Hellman GSS-API backend, dhmech.so

LIBRARY= dh1024-0.a
VERS = .1

DH1024=	dh1024.o dh_common.o generic_key.o

OBJECTS= $(DH1024) 

# include library definitions
include ../../../../Makefile.lib


CPPFLAGS += -I../../backend/mech -I../../backend/crypto
CPPFLAGS += -I$(SRC)/lib/libnsl/include
CPPFLAGS += -I$(SRC)/uts/common/gssapi/include

$(PICS) := 	CFLAGS += -xF
$(PICS) := 	CCFLAGS += -xF
$(PICS) :=	CFLAGS64 += -xF
$(PICS) :=	CCFLAGS64 += -xF

LIBS = $(DYNLIB)
LIBNAME = $(LIBRARY:%.a=%)

MAPFILE = ../mapfile-vers
DYNFLAGS += -M $(MAPFILE)

LDLIBS += -lsocket -lnsl -lmp -ldl -lc

.KEEP_STATE:

SRCS=	../dh1024.c ../../dh_common/dh_common.c ../../dh_common/generic_key.c

ROOTLIBDIR = $(ROOT)/usr/lib/gss
ROOTLIBDIR64 = $(ROOT)/usr/lib/sparcv9/gss

#LINTFLAGS += -errfmt=simple
#LINTFLAGS64 += -errfmt=simple
LINTOUT =	lint.out
LINTSRC =	$(LINTLIB:%.ln=%)
ROOTLINTDIR =	$(ROOTLIBDIR)
#ROOTLINT = 	$(LINTSRC:%=$(ROOTLINTDIR)/%)

CLEANFILES += $(LINTOUT) $(LINTLIB)

lint: $(LINTLIB)

$(ROOTLIBDIR):
	$(INS.dir)

$(ROOTLIBDIR64):
	$(INS.dir)

# include library targets
include ../../../../Makefile.targ

objs/%.o profs/%.o pics/%.o: ../%.c
	$(COMPILE.c)  -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pics/%.o: ../../dh_common/%.c
	$(COMPILE.c)  -o $@ $<
	$(POST_PROCESS_O)

objs/%.o profs/%.o pics/%.o: ../profile/%.c
	$(COMPILE.c)  -o $@ $<
	$(POST_PROCESS_O)
