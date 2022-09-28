#
#ident	"@(#)Makefile	1.2	97/11/23 SMI"
#
# Copyright (c) 1993 by Sun Microsystems, Inc.
#
# cmd/sgs/size/Makefile
#

include		../../../Makefile.master

SUBDIRS=	$(MACH)

all:=		TARGET= all
install:=	TARGET= install
clean:=		TARGET= clean
clobber:=	TARGET= clobber
lint:=		TARGET= lint
package:=	TARGET= package

.KEEP_STATE:

all clean clobber install lint package:	$(SUBDIRS)

$(SUBDIRS):	FRC
		@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
