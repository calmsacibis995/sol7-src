#
#ident	"@(#)Makefile	1.42	98/01/29 SMI"
#
# Copyright (c) 1996 by Sun Microsystems, Inc.
# All rights reserved.
#
# cmd/sgs/packages/Makefile

include		$(SRC)/Makefile.master

SUBDIRS1= 	../libdl \
		../libelf \
		../libconv \
		../liblddbg \
		../librtld_db \
		.WAIT \
		../librtld

SUBDIRS2 =	../rtld \
		../rtld.4.x \
		../libldstab \
		../ldprof \
		../lddstub \
		../link_audit \
		../libldmake/$(MACH) \
		../0@0 \
		../gprof \
		../lorder \
		../tools \
		../librtld_db/rdb_demo/$(MACH)

INTLDIRS=	../libld \
		../dump \
		../elfdump \
		../ar \
		../ld \
		../ldd \
		../mcs \
		../nm \
		../pvs \
		../size

$(SUBDIRS1) $(SUBDIRS2) :=	TARGET = install
$(INTLDIRS) :=	TARGET = package


M32:sh=		if [ "$MACH" = "sparc" ]; \
		then echo sparc; \
		elif [ "$MACH" = "i386" ]; \
		then echo "i86"; \
		else echo "unknown"; \
		fi

../%:		FRC
		@ cd $@; pwd; $(MAKE) $(TARGET)

.KEEP_STATE_FILE: .make.state.$(MACH)
.KEEP_STATE:
.PARALLEL:	$(SUBDIRS1) $(INTLDIRS) $(SUBDIRS2)

all clean clobber install lint: FRC
		@ cd $(MACH); pwd; $(MAKE) $@

# 
# invoke `package' to build this package from scratch.  It will 
# perform make install in the directories that contains components 
# that this package is dependent upon 
# 
package:	package_dep .WAIT install

package_dep:	package_dep_ .WAIT $(SUBDIRS1) .WAIT \
		$(INTLDIRS) .WAIT $(SUBDIRS2)

package_dep_:	FRC
		@ cd ../tools; pwd; $(MAKE) proto && ./proto
		@ cd $(SRC)/head; pwd; $(MAKE) \
			$(ROOT)/usr/include/dlfcn.h \
			$(ROOT)/usr/include/gelf.h \
			$(ROOT)/usr/include/libelf.h \
			$(ROOT)/usr/include/proc_service.h \
			$(ROOT)/usr/include/rtld_db.h \
			$(ROOT)/usr/include/link.h
		@ cd $(SRC)/uts/common/sys; pwd; $(MAKE) \
			$(ROOT)/usr/include/sys/elf.h \
			$(ROOT)/usr/include/sys/elf_386.h \
			$(ROOT)/usr/include/sys/elf_M32.h \
			$(ROOT)/usr/include/sys/elf_SPARC.h \
			$(ROOT)/usr/include/sys/elf_notes.h \
			$(ROOT)/usr/include/sys/elftypes.h \
			$(ROOT)/usr/include/sys/auxv.h \
			$(ROOT)/usr/include/sys/link.h \
			$(ROOT)/usr/include/sys/note.h \
			$(ROOT)/usr/include/sys/systeminfo.h
		@ cd $(SRC)/uts/$(M32)/sys; pwd; $(MAKE) \
			$(ROOT)/usr/include/sys/machelf.h
		@ cd ../tools/$(MACH); pwd; $(MAKE) sgsmsg jive
		@ cd ..; pwd; $(MAKE) _msg
		@ cd ..; pwd; $(MAKE) native-add
		@ cd ../link_audit/$(MACH); pwd; $(MAKE) install_onld
		@ cd ../messages/$(MACH); pwd; $(MAKE) test
		@ cd ../sgsdemangler/$(MACH); pwd; $(MAKE) all
FRC:
