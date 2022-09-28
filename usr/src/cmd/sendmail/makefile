#
#ident	"@(#)Makefile	1.38	97/12/08 SMI"
#
# Copyright (c) 1990, 1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/sendmail/Makefile
#
# Makefile for top level sendmail source directory
#

SUBDIRS=	lib src aux cf 


include 	../Makefile.cmd

ROOTDIRS=	$(ROOT)/var/spool/mqueue
$(ROOTDIRS):= 	OWNER = root
$(ROOTDIRS):=	DIRMODE = 0750

all:=           TARGET= all
install:=       TARGET= install
clean:=         TARGET= clean
clobber:=       TARGET= clobber
lint:=          TARGET= lint

.KEEP_STATE:

all:		$(SUBDIRS)

clean lint:	$(SUBDIRS)

clobber:	clean $(SUBDIRS)

install: 	$(ROOTDIRS) $(SUBDIRS)

$(ROOTDIRS):
	$(INS.dir)

$(SUBDIRS): 	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
