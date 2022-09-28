#
# ident	"@(#)Makefile	1.3	98/02/18 SMI"
#
# Copyright (c) 1998 by Sun Microsystems, Inc.
#
# Makefile for cmd/man/src
#

include		../../../../Makefile.cmd

SOLBOOKV2 =	solbook.dtd \
		solbook.dcl

SGMLSHLIB =	$(ROOTSHLIB)/sgml
SGMLSHLIBLOCALE =	$(ROOTSHLIB)/sgml/locale
SGMLSHLIBLOCALEC =	$(ROOTSHLIB)/sgml/locale/C
SGMLDTDDIR = 	$(ROOTSHLIB)/sgml/locale/C/dtds
SOLBOOKV2DIR =  $(SGMLDTDDIR)/solbookv2

ROOTSOLBOOKV2 = $(SOLBOOKV2:%=$(SOLBOOKV2DIR)/%)

OWNER =		bin
GROUP =		bin
FILEMODE = 	0644

.KEEP_STATE :

install :	$(SOLBOOKV2DIR) $(ROOTSOLBOOKV2)

$(SOLBOOKV2DIR) :
		$(INS.dir) 

$(ROOTSOLBOOKV2):
		$(INS.file) $(@F)

