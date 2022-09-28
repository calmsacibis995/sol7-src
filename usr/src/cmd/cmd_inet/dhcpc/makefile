#
#ident	"@(#)Makefile	1.3	96/11/27 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/cmd-inet/dhcpc/Makefile

include		../../../Makefile.master

SUBDIRS=	etc sbin tests

all:=		TARGET=	all
clean:=		TARGET=	clean
clobber:=	TARGET=	clobber
install:=	TARGET=	install
lint:=		TARGET=	lint
_msg:=		TARGET=	_msg

.KEEP_STATE:

all clean clobber install lint _msg:	\
		$(SUBDIRS)

$(SUBDIRS):	FRC
		@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
