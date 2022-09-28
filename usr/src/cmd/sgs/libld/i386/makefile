#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.10	98/01/15 SMI"

# Building SUNWonld results in a call to the `package' target.  Requirements
# needed to run this application on older releases are established:
#	i18n support requires libintl.so.1 prior to 2.6
package :=	INTLLIB = /usr/lib/libintl.so.1

include		$(SRC)/cmd/sgs/libld/Makefile.com

LDDYNFLAGS +=	-R$(SGSRPATH)

.KEEP_STATE:

all:		$(DYNLIB) $(LIBLINKS)

package install:	all $(ROOTDYNLIB)

include		$(SRC)/cmd/sgs/libld/Makefile.targ
