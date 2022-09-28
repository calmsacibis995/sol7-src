#
#ident	"@(#)Makefile	1.9	97/09/24 SMI"
#
# Copyright (c) 1993 by Sun Microsystems, Inc.
#
# lib/librac/Makefile
#
include ../../Makefile.master

SUBDIRS=	$(MACH) $(BUILD64) $(MACH64)

# include library definitions
include ../Makefile.lib

MAPFILE=	mapfile-vers

all :=		TARGET= all
clean :=	TARGET= clean
clobber :=	TARGET= clobber
install :=	TARGET= install

.KEEP_STATE:

all:  $(LIBS) .WAIT $(SUBDIRS)

txts: $(TXTS)

install: all .WAIT $(SUBDIRS) 

clean clobber : $(SUBDIRS)

MAPFILE=	mapfile-vers

include ../Makefile.targ

$(MACH) $(MACH64):	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

