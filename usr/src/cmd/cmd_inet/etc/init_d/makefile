#
# Copyright (c) 1996-1997 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.10	97/12/08 SMI"

PROG1=	inetinit
PROG2=	inetsvc
PROG3=	dhcp
PROG=	$(PROG1) $(PROG2) $(PROG3)

include ../../../Makefile.cmd

STARTINET1= $(ROOTETC)/rc2.d/S69inet
STARTINET2= $(ROOTETC)/rc2.d/S72inetsvc

DFLTD= $(ROOTETC)/default
ETCDFLTPROG = $(PROG1:%=$(DFLTD)/%)
$(ETCDFLTPROG) := FILEMODE = $(LIBFILEMODE)
$(ETCDFLTPROG) := GROUP = sys

INITD= $(ROOTETC)/init.d
DIRS= $(INITD) $(ROOTETC)/rc2.d $(ROOTETC)/rc3.d $(DFLTD)
FILEMODE= 0744
OWNER= root
GROUP= sys

ETCINITPROG=	$(PROG:%=$(INITD)/%)

.KEEP_STATE:

all: $(PROG)

install: all $(DIRS) $(ETCINITPROG) $(ETCDFLTPROG) ln_PROG

$(INITD)/% : %
	$(INS.file)

$(DFLTD)/% : %
	$(MV) $(PROG1) $(PROG1).save
	$(LN) $(PROG1).dfl $(PROG1)
	$(INS.file)
	$(RM) $(PROG1)
	$(MV) $(PROG1).save $(PROG1)

ln_PROG :
	@for dir in rc0.d rcS.d rc1.d; do \
		echo $(LN) $(INITD)/$(PROG1) $(ROOTETC)/$$dir/K43inet; \
		$(RM) $(ROOTETC)/$$dir/K43inet; \
		$(LN) $(INITD)/$(PROG1) $(ROOTETC)/$$dir/K43inet; \
		echo $(LN) $(INITD)/$(PROG2) $(ROOTETC)/$$dir/K42inetsvc; \
		$(RM) $(ROOTETC)/$$dir/K42inetsvc; \
		$(LN) $(INITD)/$(PROG2) $(ROOTETC)/$$dir/K42inetsvc; \
	done
	-$(RM) $(STARTINET1)
	$(LN) $(INITD)/$(PROG1) $(STARTINET1)
	-$(RM) $(STARTINET2)
	$(LN) $(INITD)/$(PROG2) $(STARTINET2)

$(DIRS):
	$(INS.dir)

clean:

lint:

include ../../../Makefile.targ
