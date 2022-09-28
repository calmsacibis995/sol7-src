#
# Copyright (c) 1997, by Sun Microsystems, Inc.
# All rights reserved.
#
#ident	"@(#)Makefile	1.6	97/11/14 SMI"
#
# lib/libcurses/sparcv9/Makefile

include ../Makefile.com
include ../../Makefile.lib.64

DYNFLAGS += $(DYNFLAGS64)

.KEEP_STATE:

$(ROOTLIBDIR64)/$(LIBLINKS) := INS.liblink64 = \
	-$(RM) $@; \
	cd $(ROOTLIBDIR64); \
	$(RM) libtermlib.so$(VERS) libtermcap.so$(VERS);\
	$(RM) libtermlib.so libtermcap.so; \
	$(SYMLINK) $(LIBLINKS)$(VERS) $(LIBLINKS); \
	$(SYMLINK) libcurses.so$(VERS) libtermlib.so$(VERS); \
	$(SYMLINK) libcurses.so$(VERS) libtermcap.so$(VERS); \
	$(SYMLINK) libtermcap.so$(VERS) libtermcap.so; \
	$(SYMLINK) libtermlib.so$(VERS) libtermlib.so

install: all $(ROOTLIBS64) $(ROOTLINKS64) $(ROOTLINT64)
