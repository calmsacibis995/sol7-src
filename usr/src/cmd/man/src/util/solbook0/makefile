# ident	"@(#)Makefile	1.3	98/02/18 SMI"
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
#
# Makefile for cmd/man/src
#

include		../../../../Makefile.cmd

SOLBOOKV1 =	solbook.dtd

SGMLSHLIB =	$(ROOTSHLIB)/sgml
SGMLSHLIBLOCALE =	$(ROOTSHLIB)/sgml/locale
SGMLSHLIBLOCALEC =	$(ROOTSHLIB)/sgml/locale/C
SGMLDTDDIR = 	$(ROOTSHLIB)/sgml/locale/C/dtds
SOLBOOKV1DIR =  $(SGMLDTDDIR)/solbookv1

ROOTSOLBOOKV1 = $(SOLBOOKV1:%=$(SOLBOOKV1DIR)/%)

OWNER =		bin
GROUP =		bin
FILEMODE = 	0644

.KEEP_STATE :

install :	$(SOLBOOKV1DIR) $(ROOTSOLBOOKV1)

$(SOLBOOKV1DIR) :
		$(INS.dir) 

$(ROOTSOLBOOKV1):
		$(INS.file) $(@F)

