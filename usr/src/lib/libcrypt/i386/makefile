#
#ident	"@(#)Makefile	1.4	97/08/28 SMI"
#
# Copyright (c) 1997 by Sun Microsystems, Inc.
#
# lib/libcrypt/i386/Makefile
#

include ../Makefile.com

.KEEP_STATE:

all: $(LIBS) $(LIB_INT_ST) $(LIB_INT_DY)

install: all $(ROOTLIBS) $(ROOTLINKS)
	$(INS) -s -m $(FILEMODE) -u $(OWNER) -g $(GROUP) -f $(ROOTLIBDIR) \
		libcrypt_i.a
	cd $(ROOTLIBDIR); $(RM) libcrypt.a; $(SYMLINK) libcrypt_i.a libcrypt.a
	$(INS) -s -m 755 -u $(OWNER) -g $(GROUP) -f $(ROOTLIBDIR) \
		libcrypt_i.so$(VERS)
	cd $(ROOTLIBDIR); $(RM) libcrypt.so$(VERS); \
		$(SYMLINK) libcrypt_i.so$(VERS) libcrypt.so$(VERS)
	cd $(ROOTLIBDIR); $(RM) libcrypt_i.so; \
		$(SYMLINK) libcrypt_i.so$(VERS) libcrypt_i.so
	cd $(ROOTLIBDIR); $(RM) libcrypt.so; \
		$(SYMLINK) libcrypt_i.so libcrypt.so
	cd $(ROOTLIBDIR); $(RM) llib-lcrypt.ln; \
		$(MV) llib-lcrypt_d.ln llib-lcrypt.ln
	$(INS) -s -m $(FILEMODE) -u $(OWNER) -g $(GROUP) -f $(ROOTLIBDIR) \
		../common/llib-lcrypt
