#
# Copyright (c) 1993, 1996-1997 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.6	97/12/08 SMI"

include ../Makefile.com

DATAFILES += i.preserve depend i.initd

ACTION_SUNWlibC=grep SUNWlibC depend > /dev/null || \
	( chmod 666 depend; \
	echo "P SUNWlibC        SPARCompilers Bundled libC" >> depend; \
	echo "I SUNWlpr LP Print Service, (root)" >> depend; \
	chmod 444 depend );

.KEEP_STATE:

all: $(FILES) action

install: all pkg

# action is a pseudotarget denoting completed work on the depend file
action: depend
	$(ACTION_SUNWlibC)
	touch $@

include ../Makefile.targ
