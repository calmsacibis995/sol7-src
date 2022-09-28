#
#ident	"@(#)Makefile	1.9	97/06/03 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All Rights Reserved.
#

PROG=xntpd

all: $(PROG) 

include ../../Makefile.cmd
include ../Makefile.cmd

.KEEP_STATE:

CLOBBERFILES = $(ROOTINETLIBPROG)

install: all $(ROOTINETLIBPROG)

lint:
# lint:	lint_PROG

include ../../Makefile.targ

#
# xntpd - NTP daemon
#
LIBNTP=		../$(LIBNTP_A)
LIBPARSE=	../$(LIBPARSE_A)
LINTLIB=	../$(LLIBNTP_A)
LDLIBS +=		$(LIBNTP) $(LIBPARSE) $(LIBS)
#
SOURCE=	ntp_config.c ntp_control.c ntp_io.c ntp_leap.c \
	ntp_loopfilter.c ntp_monitor.c ntp_peer.c ntp_proto.c \
	ntp_refclock.c ntp_request.c ntp_restrict.c ntp_timer.c \
	ntp_unixclock.c ntp_util.c ntp_intres.c ntp_filegen.c ntpd.c \
	refclock_conf.c refclock_chu.c refclock_local.c \
	refclock_pst.c refclock_wwvb.c refclock_goes.c \
	refclock_mx4200.c refclock_parse.c refclock_as2201.c \
	refclock_omega.c refclock_tpro.c refclock_leitch.c \
	refclock_irig.c refclock_msfees.c refclock_gpstm.c \
	refclock_trak.c refclock_datum.c refclock_acts.c \
	refclock_heath.c refclock_nmea.c refclock_moto.c \
	refclock_atom.c refclock_ptbacts.c refclock_usno.c \
	refclock_true.c refclock_hpgps.c

REFCLK_OBJS= refclock_conf.o refclock_chu.o refclock_local.o \
	refclock_pst.o refclock_wwvb.o refclock_goes.o \
	refclock_mx4200.o refclock_as2201.o \
	refclock_omega.o refclock_tpro.o refclock_leitch.o \
	refclock_irig.o refclock_msfees.o refclock_gpstm.o \
	refclock_trak.o refclock_datum.o refclock_acts.o \
	refclock_heath.o refclock_nmea.o refclock_moto.o \
	refclock_atom.o refclock_ptbacts.o refclock_usno.o \
	refclock_true.o refclock_hpgps.o

# This is only used to drive in-kernel parsing (streams module)
# refclock_parse.o

OBJS=	ntp_config.o ntp_control.o ntp_io.o ntp_leap.o \
	ntp_loopfilter.o ntp_monitor.o ntp_peer.o ntp_proto.o \
	ntp_refclock.o ntp_request.o ntp_restrict.o ntp_timer.o \
	ntp_unixclock.o ntp_util.o ntp_intres.o ntp_filegen.o ntpd.o \
	version.o $(REFCLK_OBJS)


$(REFCLK_OBJS) :=	CFLAGS += $(CLOCKDEFS)

$(PROG): $(OBJS)
	$(LINK.c) -o $@ $(OBJS) $(LDLIBS)

lint:
	$(LINT.c) -x -u $(DEFS) $(INCL) $(LLIBNTP) $(SOURCE) >lint.errs

clean:
	$(RM) $(OBJS) lint.errs
