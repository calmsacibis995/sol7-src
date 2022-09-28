#
#ident	"@(#)Makefile	1.5	97/08/28 SMI"
#
# Copyright (c) 1997 by Sun Microsystems, Inc.
#
# lib/libcryt/sparc/Makefile
#

include ../Makefile.com
include ../../Makefile.lib.64

LIBS=         $(DYNLIB) $(LINTLIB)

.KEEP_STATE:

all: $(LIBS) $(LIB_INT_ST) $(LIB_INT_DY)

install: all $(ROOTLIBS64) $(ROOTLINKS64)
	$(INS) -s -m 755 -u $(OWNER) -g $(GROUP) -f $(ROOTLIBDIR64) \
		libcrypt_i.so$(VERS)
	cd $(ROOTLIBDIR64); $(RM) libcrypt.so$(VERS); \
		$(SYMLINK) libcrypt_i.so$(VERS) libcrypt.so$(VERS)
	cd $(ROOTLIBDIR64); $(RM) libcrypt_i.so; \
		$(SYMLINK) libcrypt_i.so$(VERS) libcrypt_i.so
	cd $(ROOTLIBDIR64); $(RM) libcrypt.so; \
		$(SYMLINK) libcrypt_i.so libcrypt.so
	cd $(ROOTLIBDIR64); $(RM) llib-lcrypt.ln; \
		$(MV) llib-lcrypt_d.ln llib-lcrypt.ln
