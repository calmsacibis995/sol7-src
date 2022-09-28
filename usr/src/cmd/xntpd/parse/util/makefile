#
#ident	"@(#)Makefile	1.2	96/11/12 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All Rights Reserved.
#

#
# /src/NTP/REPOSITORY/v3/parse/util/Makefile.tmpl,v 3.12 1994/01/25 19:05:39 kardel Exp
#
PROG=parsetest testdcf dcfd

include ../../Makefile.cmd
include ../../../Makefile.cmd

.KEEP_STATE:

# all: $(PROG) 
all:

# install: all $(ROOTINETLIBPROG)
install:

clean:

lint:	lint_PROG

include ../../../Makefile.targ

CLOCKDEFS=-DACTS -DAS2201 -DDATUM -DGOES -DGPSTM -DHEATH -DHPGPS -DLEITCH -DLOCAL_CLOCK -DOMEGA -DPARSE -DATOM -DCLOCK_MEINBERG -DCLOCK_SCHMID -DCLOCK_DCF7000 -DCLOCK_TRIMTAIP -DCLOCK_TRIMTSIP -DCLOCK_RAWDCF -DCLOCK_RCC8000 -DPST -DPTBACTS -DTRAK 
CFLAGS+=	$(COPTS) $(DEFS) -I../../include


notused:
	@echo $(DEFS) $(DEFS_LOCAL) $(CLOCKDEFS) | \
	 awk '/-DSTREAM/ && /-DPARSE/ && /-DCLOCK_RAWDCF/ && ( /-DSYS_SUNOS/ || /-DSYS_SOLARIS/ )  { makeit = 1 } \
	      END { if (makeit) \
		      { print "$(MAKE) parsetest"; } \
		  }' | \
	 sh
	@echo $(DEFS) $(DEFS_LOCAL) $(CLOCKDEFS) | \
	 awk '/-DPARSE/ && /-DCLOCK_RAWDCF/ && ( /-DSYS_SUNOS/ || /-DSYS_SOLARIS/ ) { makeit = 1 } \
	      END { if (makeit) \
		      { print "$(MAKE) testdcf"; } \
		  }' | \
	 sh
	@echo $(DEFS) $(DEFS_LOCAL) $(CLOCKDEFS) | \
	 awk '/-DPARSE/ && /-DCLOCK_RAWDCF/ && ( /-DSYS_SUNOS/  || /-DSYS_SOLARIS/ ) { makeit = 1 } \
	      END { if (makeit) \
		      { print "$(MAKE) dcfd"; } \
		  }' | \
	 sh
