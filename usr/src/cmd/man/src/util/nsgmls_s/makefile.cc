#
#pragma ident	"@(#)Makefile.lib.CC	1.2	97/04/24 SMI"
#
all pure: lib$(LIB).a

# FIXME needs also to depend on template files

REPOSITORY=ptrepository
lib$(LIB).a: $(OBJS) $(COBJS) $(DEPLIBS)
	- $(CXX) $(ALL_CXXFLAGS) $(OBJS) $(COBJS) $(DEPLIBS)
	-rm -f a.out
	if test -d $(REPOSITORY); then \
	  i=1; \
	  tmpdir=/tmp/ar.$$$$; \
	  rm -fr $$tmpdir; \
	  mkdir $$tmpdir; \
	  for o in $(REPOSITORY)/*.o; do \
	    cp $$o $$tmpdir/$$i.o; \
	    i=`expr $$i + 1`; \
	  done ; \
	  $(AR) r $@ $$tmpdir/*.o ; \
	fi
	$(AR) r $@ $?

depend: depend_src
depend.temp: $(GENSRCS)
gen: $(GENSRCS)
