#
# Copyright (c) 1997 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.7	97/12/08 SMI"

include ../Makefile.com

DATAFILES += depend i.initd

ACTION_all=grep SUNWter depend > /dev/null || \
	( chmod 666 depend; \
	echo "P SUNWmibii     Solstice Enterprise Agent SNMP daemon" >> depend; \
	echo "P SUNWsasnm     Solstice Enterprise Agent Simple Network Management Protocol" >> depend; \
	echo "P SUNWsadmi     Solstice Enterprise Agent Desktop Management Interface" >> depend; \
	echo "P SUNWlibC      SPARCompilers Bundled libC" >> depend; \
	chmod 444 depend )

.KEEP_STATE:

all: $(FILES) action

# action is a pseudotarget denoting completed work on the depend file
action: depend
	$(ACTION_all)
	touch $@

install: all pkg

include ../Makefile.targ
