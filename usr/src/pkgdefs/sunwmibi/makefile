# Copyright 01/13/97 Sun Microsystems, Inc. All Rights Reserved.
#pragma ident  "@(#)Makefile	1.13 97/01/13 Sun Microsystems"

include ../Makefile.com

DATAFILES += depend
 
.KEEP_STATE:
 
all: $(FILES) action

# action is a pseudotarget denoting completed work on the depend file
action: depend
	touch $@

install: all pkg
 
include ../Makefile.targ

