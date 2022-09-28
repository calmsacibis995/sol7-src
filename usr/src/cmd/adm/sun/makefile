#
#ident	"@(#)Makefile	1.11	97/09/18 SMI"
#
# Copyright (c) 1989, 1995, 1997 by Sun Microsystems, Inc.
# All Rights Reserved
#

ETCFILES= ioctl.syscon passwd shadow motd
KVMFILES= README

include ../../Makefile.cmd

ROOTETCFILES= $(ETCFILES:%=$(ROOTETC)/%)
ROOTUSRKVMFILES= $(KVMFILES:%=$(ROOTUSRKVM)/%)

FILEMODE= 0644
TOUCHTIME= 0101000070
OWNER= root
GROUP= sys

$(ROOTETC)/shadow :=	FILEMODE =	400

.KEEP_STATE:

all: $(ETCFILES) $(KVMFILES)

install: all $(ROOTETCFILES) $(ROOTUSRKVMFILES)

TOUCHIT=
$(CH)TOUCHIT= ; $(TOUCH) $(TOUCHTIME) $@

$(CH)$(ROOTETCFILES) := INS.file += $(TOUCHIT)

clean:

lint:

clobber:

motd:	FRC
	@-$(ECHO) "rebuilding motd"
	@$(RELEASE_BUILD)-$(ECHO) "Sun Microsystems Inc.\tSunOS $(RELEASE)\t$(VERSION)\t$(RELEASE_DATE)" > motd
	@$(NOT_RELEASE_BUILD)-$(ECHO) "Sun Microsystems Inc.\tSunOS $(RELEASE)\t$(VERSION)\t`date +'%h. %d, %Y'`" > motd
	@$(NOT_RELEASE_BUILD)-$(ECHO) $(DEV_CM) | sed -e "s/@(#)//" >> motd
	@-$(CAT) release_info >> motd

clean:

lint:

clobber:
	$(RM) motd

FRC:
