#
# ident	"@(#)Makefile	1.10	97/05/27 SMI"
#
# Copyright (c) 1994 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/lp/lib/Makefile
#

SUBDIRS =		\
		access	\
		class	\
		filters	\
		forms	\
		lp	\
		msgs	\
		oam	\
		requests\
		secure	\
		systems	\
		printers\
		users


all :=		TARGET= all
install :=	TARGET= install
clean :=	TARGET= clean
clobber :=	TARGET= clobber
lint :=		TARGET= lint
strip :=	TARGET= strip
_msg :=		TARGET= _msg

# for messaging catalog
#
POFILE= lib.po
POFILES= $(SUBDIRS:%=%/%.po)

.KEEP_STATE:

all install:	$(SUBDIRS)

clean strip lint:	$(SUBDIRS)

clobber: $(SUBDIRS) local_clobber

local_clobber:
	$(RM) $(POFILE) $(POFILES) $(CLOBBERFILES)

_msg:        $(SUBDIRS)
	$(RM)	$(POFILE)
	cat $(POFILES)  > $(POFILE)

$(SUBDIRS):	FRC
		@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
