# Copyright 01/13/97 Sun Microsystems, Inc. All Rights Reserved.
#pragma ident  "@(#)Makefile	1.4 97/01/13 Sun Microsystems"

include ../Makefile.com

DATAFILES += depend

ACTION_all=grep SUNWter depend > /dev/null || \
	( chmod 666 depend; \
	echo "P SUNWmibii     Solstice Enterprise Agent SNMP daemon" >> depend; \
	chmod 444 depend )

.KEEP_STATE:

all: $(FILES) action

# action is a pseudotarget denoting completed work on the depend file
action: depend
	$(ACTION_all)
	touch $@

install: all pkg

include ../Makefile.targ
