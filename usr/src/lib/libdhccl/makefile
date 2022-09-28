#
#ident	"@(#)Makefile	1.8	97/01/06 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc. All rights Reserved.
#
#	lib/libdhcclnt/Makefile
#
LIBRARY = libdhcclnt.a
OBJECTS = activity.o addrstr.o agentexit.o arp.o arpcheck.o bpcomp.o btoh.o \
	bytetok.o cfgatew.o cfhost.o cfif.o chkaddr.o clinit.o clpolicy.o \
	convsyms.o crsock.o ctrlfd.o daemon.o dcmsgstr.o dcuttxt.o \
	decline.o dhcops.o dhcpipc.o dlpitobp.o dlpiutil.o \
	drop.o dshared.o dumphost.o enabsig.o evntwait.o expire.o \
	extend.o fail.o freehost.o fswok.o ftoh.o getctime.o haddrtoa.o \
	handlers.o htob.o htof.o ifsetup.o iftoppa.o in_cksum.o inetaton.o \
	init.o initcomp.o internal.o ireboot.o lli.o loginit.o loglbyl.o \
	macaddr.o msgtxt.o newif.o polldbg.o prpacket.o readbp.o rebind.o \
	release.o renew.o reply.o request.o root.o savedhcs.o seed.o \
	seekdict.o sendbp.o setrt.o setwake.o showhdrs.o showvec.o smccif.o \
	startif.o statestr.o strioctl.o tags.o timefns.o timeout.o timers.o \
	tokenise.o utiltxt.o xgets.o xmalloc.o xrealloc.o xstrdup.o ycdisp.o

# include library definitions
include ../Makefile.lib

LIBS = $(LIBRARY)

# EMPLOY_BOOTP turns BOOTP support on, but it currently doesn't work...
CPPFLAGS += -I. -DUSE_TCP=1 -DEMPLOY_BOOTP=1
CLOBBERFILES += $(LINTLIB)

# workaround for optimizer bug 4015159
sparc_COPTFLAG = -xO1

.KEEP_STATE:

all: $(LIBS)

install: all

lint: llib-ldhcclnt.ln

# include library targets
include ../Makefile.targ
