#
#ident  "@(#)Makefile 1.12     97/11/13 SMI"
#
# Copyright (c) 1996,1997, by Sun Microsystems, Inc.
# All rights reserved.
#

include ../../Makefile.cmd

TXTS=
GSSTXTS= mech qop gsscred.conf

IETCFILES=			$(TXTS:%=$(ROOTETC)/%)
IETCGSSFILES=			$(GSSTXTS:%=$(ROOTETC)/gss/%)
GSSDIR=				$(ROOTETC)/gss

FILEMODE= 0644
OWNER= root
GROUP= sys


.KEEP_STATE:

all: $(TXTS) $(GSSTXTS) dummy_mech_token.conf

dummy_mech_token.conf:	dummy_mech_token.conf.sh
			$(RM) $@
			sh dummy_mech_token.conf.sh

install: all $(GSSDIR) $(IETCFILES) $(IETCGSSFILES)

install_h:

$(GSSDIR):
	$(INS.dir)

$(ROOTETC)/%: %
	$(INS.file)

$(ROOTETC)/gss/%: %
	$(INS.file)

FRC:

include ../../Makefile.targ

CLOBBERFILES += dummy_mech_token.conf

clean lint:
