#
#ident	"@(#)Makefile.cmd	1.6	97/06/02 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All Rights Reserved.
#

LIBS = -lsocket -lnsl -lelf -lposix4

LIBNTP_A= lib/libntp.a
LIBPARSE_A= parse/libparse.a
LLIBNTP_A= lib/llib-llibntp.ln

DEFS=       -DSTREAM -DSYS_SOLARIS -DADJTIME_IS_ACCURATE -DUSE_CLOCK_SETTIME -DSYSCALL_BUG -DMCAST -DREFCLOCK -DPPS -DUSE_PROTOTYPES -DNTP_SYSCALLS_LIBC -DKERNEL_PLL -DDES -DMD5 -DDEBUG

CLOCKDEFS=-DACTS -DAS2201 -DDATUM -DGOES -DGPSTM -DHEATH -DHPGPS -DLEITCH \
-DLOCAL_CLOCK -DOMEGA -DATOM -DCLOCK_MEINBERG -DCLOCK_SCHMID \
-DCLOCK_DCF7000 -DCLOCK_TRIMTAIP -DCLOCK_TRIMTSIP -DCLOCK_RAWDCF \
-DCLOCK_RCC8000 -DPST -DPTBACTS -DTRAK 

ROOTINETLIB=	$(ROOT)/usr/lib/inet
ROOTINETLIBPROG=	$(PROG:%=$(ROOTINETLIB)/%)

ROOTETCINET=	$(ROOT)/etc/inet
ROOTETCINETPROG=	$(PROG:%=$(ROOTETCINET)/%)

ROOTETCINIT=	$(ROOT)/etc/init.d
ROOTETCINITPROG=	$(PROG:%=$(ROOTETCINIT)/%)

INCL=	-I../include
CFLAGS += $(DEFS) $(INCL)

$(ROOTINETLIB):
	$(INS.dir)

$(ROOTINETLIB)/%: % $(ROOTINETLIB)
	$(INS.file)

$(ROOTETCINET):
	$(INS.dir)

$(ROOTETCINET)/%: % $(ROOTETCINET)
	$(INS.file)

$(ROOTETCINIT):
	$(INS.dir)

$(ROOTETCINIT)/%: % $(ROOTETCINIT)
	$(INS.file)
