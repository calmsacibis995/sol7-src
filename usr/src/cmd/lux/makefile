#
# Copyright 1997 Sun Microsystems, Inc. All rights reserved.
#
#ident	"@(#)Makefile	1.11	97/05/20 SMI"
#
# cmd/lux/Makefile
#
#      luxadm (Administration program for the PHOTON) Makefile 
#

include	../Makefile.cmd

PROG		= luxadm
CPPFLAGS	+= -Ihdrs -DDEVCTL_LIB -DSSA_HOTPLUG
LDLIBS 		+= -ldevinfo -ldevice -lkstat
LINTFLAGS	+= -u

OBJECTS		= adm.o ssa.o funct.o mon.o io.o genf.o cmd.o \
		envsen.o setboot.o fcupdate.o fcalupdate.o devctl.o

LINT_OBJS	= $(OBJECTS:%.o=%.ln)

.KEEP_STATE:

all debug browser debug_browser: $(PROG)

$(PROG):	$(OBJECTS)
		$(LINK.c) -o $@ $(OBJECTS) $(LDLIBS)

debug	:= CFLAGS += -g
browser	:= CFLAGS += -xsb
debug_browser	:= CFLAGS += -xsb -g

lint: $(LINT_OBJS)
	$(LINT.c) $(LINTFLAGS) $(LINT_OBJS)

clean:
	$(RM) $(OBJECTS)
	$(RM) $(LINT_OBJS)

FILEMODE=       644
DIRMODE=        755
OWNER=          root
GROUP=          bin
FIRMWARED=	$(ROOTLIB)/firmware
FCODED=         $(ROOTLIB)/firmware/fc_s
FCODE=          $(FCODED)/fcal_s_fcode
LUXFIRMWARED=	$(ROOTLIB)/locale/C/LC_MESSAGES
LUXIBFIRMWARE=	$(LUXFIRMWARED)/ibfirmware

$(FIRMWARED) :=			OWNER=	bin
$(FIRMWARED) :=			GROUP=	bin
$(FCODED) :=			OWNER=	bin
$(FCODED) :=			GROUP=	bin
$(LUXFIRMWARED) :=		OWNER=	bin
$(LUXFIRMWARED) :=		GROUP=	bin
$(ROOTUSRSBINPROG) :=		OWNER=	bin
$(ROOTUSRSBINPROG) :=		GROUP=	bin
$(ROOTUSRSBINPROG) :=		FILEMODE=	555

install: all $(ROOTUSRSBINPROG) $(LUXIBFIRMWARE) $(FCODE)

$(FIRMWARED):
	$(INS.dir)

$(FCODED):	$(FIRMWARED)
	$(INS.dir)

$(FCODE):	$(FCODED) fcal_s_fcode
	$(INS.file) fcal_s_fcode

$(LUXFIRMWARED):
	$(INS.dir)

$(LUXIBFIRMWARE):	$(LUXFIRMWARED) ibfirmware
	$(INS.file) ibfirmware	

include	../Makefile.targ
