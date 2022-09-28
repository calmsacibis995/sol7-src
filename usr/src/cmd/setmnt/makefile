#
#ident	"@(#)Makefile	1.8	97/03/28 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

PROG= setmnt 

include ../Makefile.cmd

CFLAGS += -v
CPPFLAGS += -D_FILE_OFFSET_BITS=64

.KEEP_STATE:

all: $(PROG) 

install: all  $(ROOTUSRSBINPROG)
	-$(RM) $(ROOTETCPROG)
	-$(SYMLINK) ../usr/sbin/$(PROG) $(ROOTETCPROG)

clean:

lint:	lint_PROG

include ../Makefile.targ
