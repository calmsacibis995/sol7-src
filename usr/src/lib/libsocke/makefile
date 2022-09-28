#
#ident	"@(#)Makefile	1.33	98/01/19 SMI"
#
# Copyright (c) 1993-1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# lib/libsocket/Makefile
#

SUBDIRS =	$(MACH) $(BUILD64) $(MACH64)

SED =	sed
CP =	cp
GREP =	grep

ROOTLINTDIR= $(ROOTLIBDIR)

all :=		TARGET= all
clean :=	TARGET= clean
clobber :=	TARGET= clobber
delete :=	TARGET= delete
install :=	TARGET= install
lint :=		TARGET= lint
catalog :=	TARGET= catalog
package :=	TARGET= package

# include library definitions
include ../Makefile.lib

TEXT_DOMAIN = 	SUNW_OST_NETRPC
POFILE= libsocket.po
POFILES= generic.po 

.KEEP_STATE:

all install clean clobber delete lint catalog package:	$(SUBDIRS)


# install rule for lint library target
$(ROOTLINTDIR)/%: common/%
	$(INS.file)

_msg: $(MSGDOMAIN) $(POFILE)
	$(RM) $(MSGDOMAIN)/$(POFILE)
	$(CP) $(POFILE) $(MSGDOMAIN)

$(POFILE):	$(POFILES)
	$(RM) -f $@
	$(CAT) $(POFILES) > $@

generic.po:
	$(RM) messages.po
	$(XGETTEXT) -a $(XGETFLAGS) `$(GREP) -l gettext */*.[ch]`
	$(SED) "/^domain/d" messages.po > $@
	$(RM) messages.po

$(MSGDOMAIN):
	$(INS.dir)
 
$(MACH) $(MACH64):	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)
 
FRC:

