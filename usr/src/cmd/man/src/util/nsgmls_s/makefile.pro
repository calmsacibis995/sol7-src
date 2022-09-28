#
#pragma ident	"@(#)Makefile.prog	1.8	97/05/09 SMI"
#
include ../../../../../Makefile.cmd
CLEANFILES=$(PROG)$(EXE) $(OBJS) core

all: $(PROG)$(EXE)

pure: $(PROG).pure

$(PROG)$(EXE): $(OBJS) $(COBJS) $(XLIBS)
	$(CCC) $(ALL_CXXFLAGS) $(CCFLAGS) $(CPPFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(COBJS) $(XLIBS) $(LIBS)
	cp $@ ../..

$(PROG).pure: $(OBJS) $(COBJS) $(XLIBS)
	$(PURIFY) $(CXX) $(ALL_CXXFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(COBJS) $(XLIBS) $(LIBS)

install: $(PROG)$(EXE)
	-test -d $(bindir) || mkdir $(bindir)
	-rm -f $(bindir)/$(PROG)$(EXE)
	$(INSTALL) $(PROG)$(EXE) $(bindir)/$(PROG)$(EXE)

depend: depend_src
depend.temp: $(GENSRCS)
gen: $(GENSRCS)
