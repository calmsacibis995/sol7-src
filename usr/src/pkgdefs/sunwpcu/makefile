#
#ident	"@(#)Makefile	1.3	97/05/28 SMI"
#
# Copyright (c) 1993 by Sun Microsystems, Inc.
#

include ../Makefile.com

DATAFILES += depend

ACTION_SUNWlibC=grep SUNWlibC depend > /dev/null || \
	( chmod 666 depend; \
	echo "P SUNWlibC        SPARCompilers Bundled libC" >> depend; \
	echo "P SUNWfns       Federated Naming Service" >> depend; \
	echo "P SUNWpcr       SunSoft Print - Client, (root)" >> depend; \
	echo "I SUNWlpu       LP Print Service, (usr)" >> depend; \
	chmod 444 depend );

.KEEP_STATE:

all: $(FILES) action
install: all pkg

# action is a pseudotarget denoting completed work on the depend file
action: depend
	$(ACTION_SUNWlibC)
	touch $@

include ../Makefile.targ
