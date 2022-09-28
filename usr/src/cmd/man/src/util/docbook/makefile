#
# ident	"@(#)Makefile	1.3	98/02/18 SMI"
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
#
# Makefile for cmd/man/src
#

include		../../../../Makefile.cmd

DOCBOOKDTD =	docbook.dtd

SGMLSHLIB =	$(ROOTSHLIB)/sgml
SGMLSHLIBLOCALE =	$(ROOTSHLIB)/sgml/locale
SGMLSHLIBLOCALEC =	$(ROOTSHLIB)/sgml/locale/C
SGMLDTDDIR = 	$(ROOTSHLIB)/sgml/locale/C/dtds
DOCBOOKDIR =	$(SGMLDTDDIR)/docbook

ROOTDOCBOOK = $(DOCBOOKDTD:%=$(DOCBOOKDIR)/%)

OWNER =		bin
GROUP =		bin
FILEMODE = 	0644

.KEEP_STATE :

install :	$(DOCBOOKDIR) $(ROOTDOCBOOK)

$(DOCBOOKDIR) :
		$(INS.dir) 

$(ROOTDOCBOOK):
		$(INS.file) $(@F)

