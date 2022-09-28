#
#ident	"@(#)Makefile	1.8	97/02/01 SMI"
#
# Copyright (c) 1989 by Sun Microsystems, Inc.
#

PROG= ps

include ../Makefile.cmd

CFLAGS += -v

XGETFLAGS += -a -x ps.xcl

FILEMODE= 04555
OWNER= root
GROUP= sys

.KEEP_STATE:

all: $(PROG) 

install: all $(ROOTPROG)

clean:

lint:	lint_PROG

include ../Makefile.targ
