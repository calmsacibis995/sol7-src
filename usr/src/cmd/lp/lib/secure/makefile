#
# ident	"@(#)Makefile	1.10	97/06/06 SMI"	/* SVr4.0 1.13	*/
#
# Copyright (c) 1990 by Sun Microsystems, Inc.
#
# cmd/lp/lib/secure/Makefile
#

LIBRARY	=	liblpsec.a

OBJECTS =	secure.o


include		../../../../lib/Makefile.lib
include         ../../Makefile.lp

CPPFLAGS =	-I../../include $(CPPFLAGS.master) -K pic

.KEEP_STATE:
# for messaging catalog
#
POFILE= secure.po
SRCS= $(OBJECTS:%.o=%.c)
XGETFLAGS=-a -x $(LPROOT)/lp.xcl

_msg:        $(POFILE)

all install :	$(LIBS)

strip lint :

include		../../../../lib/Makefile.targ
