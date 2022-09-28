#
#pragma ident	"@(#)Makefile.comm	1.6	97/10/06 SMI"
#
include ../../../../../Makefile.cmd

CXXFLAGS=$(DEBUG) $(OPTIMIZE) $(WARN)
ALL_CXXFLAGS=$(CXXFLAGS) -I$(srcdir) -I$(srcdir)/../include $(INCLUDE) \
 $(DEFINES)
ALL_CFLAGS=$(CFLAGS) $(DEBUG) $(OPTIMIZE) $(INCLUDE) $(DEFINES)
MSGGENFLAGS=
AR=ar
RANLIB=:
M4=m4
GENSRCS=
OBJS=
COBJS=
PROG=
#PERL=perl

.SUFFIXES: .cxx .c .o .m4 .msg

.cxx.o:
	$(CCC) $(ALL_CXXFLAGS) $(CCFLAGS) $(CPPFLAGS) -c $(OUTPUT_OPTION) $<
	$(POST_PROCESS_O)

#.c.o:
#	$(CC) $(ALL_CFLAGS) -c $<

.m4.cxx:
	rm -f $@
	$(M4) $(srcdir)/../lib/instmac.m4 $< >$@
	chmod -w $@

# We don't use perl... just use static .h files
#.msg.h:
#	$(PERL) -w $(srcdir)/../msggen.pl $(MSGGENFLAGS) $<

depend_src: depend.temp
	mv depend.temp Makefile.dep

depend.temp: FORCE
	$(CXX) -MM $(ALL_CXXFLAGS) $(OBJS:.o=.cxx) \
	  | sed -e 's; \([^/ ][^/ ]*\)/; $$(srcdir)/\1/;g' >depend.temp

clean: FORCE
	-rm -f $(CLEANFILES)

clobber: FORCE
	-find . -name Templates.DB -print -exec rm -rf {} \;
	-rm -f $(CLEANFILES)

FORCE:

%: RCS/%,v
	test -w $@ || co -u $@
