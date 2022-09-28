#
# Copyright (c) 1988,1997 by Sun Microsystems, Inc.
# All rights reserved.
#
#pragma ident	"@(#)Makefile	1.69	97/03/17 SMI"
#
# lib/libbsm/Makefile
#

LIBRARY =	libbsm.a
VERS = 		.1
OBJECTS=	adr.o \
		adrf.o \
		adrm.o \
		au_open.o \
		au_preselect.o \
		au_to.o \
		au_usermask.o \
		audit_allocate.o \
		audit_class.o \
		audit_cron.o \
		audit_event.o \
		audit_ftpd.o \
		audit_halt.o \
		audit_inetd.o \
		audit_init.o \
		audit_login.o \
		audit_mountd.o \
		audit_passwd.o \
		audit_reboot.o \
		audit_rexd.o \
		audit_rexecd.o \
		audit_rshd.o \
		audit_shutdown.o \
		audit_su.o \
		audit_uadmin.o \
		audit_user.o \
		bsm.o \
		generic.o \
		getacinfo.o \
		getauditflags.o \
		getdaent.o \
		getdment.o \
		getfaudflgs.o

#
# Include common library definitions.
#
include ../../Makefile.lib

MAPFILE=	../common/mapfile-vers
MAPFILE64=	../common/mapfile-vers
SRCS=		$(OBJECTS:%.o=../common/%.c)

#LIBS += 	$(DYNLIB) $(LINTLIB)

LINTFLAGS=	-u
LINTFLAGS64=	-u -D__sparcv9
LINTOUT=	lint.out

LINTSRC= $(LINTLIB:%.ln=%)
$(LINTLIB) :=	SRCS = ../common/$(LINTSRC)
ROOTLINTDIR=	$(ROOTLIBDIR)
ROOTLINT=	$(LINTSRC:%=$(ROOTLINTDIR)/%)

CLEANFILES +=	$(LINTOUT) $(LINTLIB)

CFLAGS	+=	-v
CFLAGS64 +=	-v
DYNFLAGS +=	
DYNFLAGS32 =	-Wl,-M,$(MAPFILE)
DYNFLAGS64 =	-Wl,-M,$(MAPFILE64)
LDLIBS +=	-lnsl -lc

COMDIR=		../common

CPPFLAGS += -I$(COMDIR) $(CPPFLAGS.master)

.KEEP_STATE:

#all: $(LIBS)

lint: $(LINTLIB)

$(DYNLIB): 	$(MAPFILE)
$(DYNLIB64): 	$(MAPFILE64)

# Include library targets
#
include ../../Makefile.targ

objs/%.o pics/%.o: ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

# install rule for lint library target
$(ROOTLINTDIR)/%: ../common/%
	$(INS.file)
