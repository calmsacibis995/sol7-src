#
#ident	"@(#)Makefile.com	1.30	98/02/20 SMI"
#
# Copyright (c) 1992-1998 by Sun Microsystems, Inc.
# All Rights Reserved.
#
# psm/stand/boot/i386/Makefile.com

include $(TOPDIR)/psm/stand/boot/Makefile.boot

BOOTSRCDIR	= ../..

CMN_DIR		= $(BOOTSRCDIR)/common
MACH_DIR	= ../common
PLAT_DIR	= .

CONF_SRC	= fsconf.c

CMN_C_SRC	= boot.c heap_kmem.c readfile.c bootflags.c

MACH_C_SRC	= a20enable.c arg.c ata.c boot_plat.c bootops.c bootprop.c
MACH_C_SRC	+= bsetup.c bsh.c bus_probe.c cbus.c chario.c cmds.c
MACH_C_SRC	+= cmdtbl.c compatboot.c delayed.c devtree.c disk.c
MACH_C_SRC	+= dosbootop.c dosdirs.c dosemul.c dosmem.c env.c
MACH_C_SRC	+= expr.c gets.c help.c i386_memlist.c i386_standalloc.c
MACH_C_SRC	+= if.c initsrc.c ix_alts.c load.c map.c memory.c misc_utls.c
MACH_C_SRC	+= net.c probe.c prom.c prop.c setcolor.c src.c var.c

MACH_Y_SRC	= exprgram.y

CONF_OBJS	= $(CONF_SRC:%.c=%.o)
CONF_L_OBJS	= $(CONF_OBJS:%.o=%.ln)

SRT0_OBJ	= $(SRT0_S:%.s=%.o)

C_SRC		= $(CMN_C_SRC) $(MACH_C_SRC) $(ARCH_C_SRC) $(PLAT_C_SRC)
S_SRC		= $(MACH_S_SRC) $(ARCH_S_SRC) $(PLAT_S_SRC)
Y_SRC		= $(MACH_Y_SRC)

OBJS		= $(C_SRC:%.c=%.o) $(S_SRC:%.s=%.o) $(Y_SRC:%.y=%.o)
L_OBJS		= $(OBJS:%.o=%.ln)

ELFCONV =	mkbin
INETCONV =	mkinet

UNIBOOT =	boot.bin

#
#  On x86, ufsboot is now delivered as the unified booter, boot.bin.
#  This delivery change is only happening on x86, otherwise we wouldn't
#  need to modify the macro for the ufsboot delivery item, instead
#  we would change the delivery item to be the unified booter at a
#  higher level in the Makefile hierarchy.
#
ROOT_PSM_UFSBOOT = $(ROOT_PSM_UNIBOOT)

.KEEP_STATE:

.PARALLEL:	$(OBJS) $(L_OBJS) $(SRT0_OBJ) .WAIT \
		$(UFSBOOT) $(NFSBOOT)

all: $(ELFCONV) $(UFSBOOT) $(INETCONV) $(NFSBOOT)

install: all

SYSDIR	=	$(TOPDIR)/uts
STANDSYSDIR=	$(TOPDIR)/stand
SYSLIBDIR=	$(TOPDIR)/stand/lib/$(MACH)

CPPDEFS		= $(ARCHOPTS) -D$(PLATFORM) -D_BOOT -D_KERNEL -D_MACHDEP
CPPINCS		+= -I.
CPPINCS		+= -I$(PSMSYSHDRDIR) -I$(STANDDIR)/$(MACH) -I$(STANDDIR)
CPPINCS		+= -I$(ROOT)/usr/platform/$(PLATFORM)/include

CPPFLAGS	= $(CPPDEFS) $(CPPFLAGS.master) $(CPPINCS)
CPPFLAGS	+= $(CCYFLAG)$(SYSDIR)/common
ASFLAGS =	$(CPPDEFS) -P -D__STDC__ -D_BOOT -D_ASM $(CPPINCS)

CFLAGS	=	../common/i86.il -O
#
# This should be globally enabled!
#
CFLAGS	+=	-v

YFLAGS	=	-d

#LDFLAGS		+= -L$(PSMNAMELIBDIR) -L$(SYSLIBDIR)
#LDLIBS		+= -lnames -lsa -lprom

#
# The following libraries are built in LIBNAME_DIR
#
LIBNAME_DIR     += $(PSMNAMELIBDIR)
LIBNAME_LIBS    += libnames.a
LIBNAME_L_LIBS  += $(LIBNAME_LIBS:lib%.a=llib-l%.ln)

#
# The following libraries are built in LIBPROM_DIR
#
LIBPROM_DIR     += $(PSMPROMLIBDIR)/$(PROMVERS)
LIBPROM_LIBS    += libprom.a
LIBPROM_L_LIBS  += $(LIBPROM_LIBS:lib%.a=llib-l%.ln)

#
# The following libraries are built in LIBSYS_DIR
#
LIBSYS_DIR      += $(SYSLIBDIR)
LIBSYS_LIBS     += libsa.a libufs.a libnfs_inet.a libpcfs.a libcompfs.a
LIBSYS_LIBS     += libhsfs.a libcachefs.a
LIBSYS_L_LIBS   += $(LIBSYS_LIBS:lib%.a=llib-l%.ln)

$(ELFCONV): $(MACH_DIR)/$$(@).c
	$(NATIVECC) -O -o $@ $(MACH_DIR)/$@.c

$(INETCONV): $(MACH_DIR)/$$(@).c
	$(NATIVECC) -O -o $@ $(MACH_DIR)/$@.c

#
# Unified booter:
#	4.2 ufs filesystem
#	nfs
#	hsfs
#	cachefs
#
LIBUNI_LIBS	= libcompfs.a libpcfs.a libufs.a libnfs_inet.a libhsfs.a
LIBUNI_LIBS	+= libprom.a libnames.a libsa.a libcachefs.a $(LIBPLAT_LIBS)
LIBUNI_L_LIBS	= $(LIBUNI_LIBS:lib%.a=llib-l%.ln)
UNI_LIBS	= $(LIBUNI_LIBS:lib%.a=-l%)
UNI_DIRS	= $(LIBNAME_DIR:%=-L%) $(LIBSYS_DIR:%=-L%)
UNI_DIRS	+= $(LIBPLAT_DIR:%=-L%) $(LIBPROM_DIR:%=-L%)

LIBDEPS=	$(SYSLIBDIR)/libcompfs.a \
		$(SYSLIBDIR)/libpcfs.a \
		$(SYSLIBDIR)/libufs.a \
		$(SYSLIBDIR)/libnfs_inet.a \
		$(SYSLIBDIR)/libhsfs.a \
		$(SYSLIBDIR)/libcachefs.a \
		$(SYSLIBDIR)/libsa.a \
		$(LIBPROM_DIR)/libprom.a \
		$(LIBNAME_DIR)/libnames.a

L_LIBDEPS=	$(SYSLIBDIR)/llib-lcompfs.ln \
		$(SYSLIBDIR)/llib-lpcfs.ln \
		$(SYSLIBDIR)/llib-lufs.ln \
		$(SYSLIBDIR)/llib-lnfs_inet.ln \
		$(SYSLIBDIR)/llib-lhsfs.ln \
		$(SYSLIBDIR)/llib-lcachefs.ln \
		$(SYSLIBDIR)/llib-lsa.ln \
		$(LIBPROM_DIR)/llib-lprom.ln \
		$(LIBNAME_DIR)/llib-lnames.ln

#
# Loader flags used to build unified boot
#
UNI_LOADMAP	= loadmap
UNI_MAPFILE	= $(MACH_DIR)/mapfile
UNI_LDFLAGS	= -dn -m -M $(UNI_MAPFILE) -e _start $(UNI_DIRS)
UNI_L_LDFLAGS	= $(UNI_DIRS)

#
# Object files used to build unified boot
#
UNI_SRT0	= $(SRT0_OBJ)
UNI_OBJS	= $(OBJS) fsconf.o
UNI_L_OBJS	= $(UNI_SRT0:%.o=%.ln) $(UNI_OBJS:%.o=%.ln)

$(UNIBOOT): $(ELFCONV) $(UNI_MAPFILE) $(UNI_SRT0) $(UNI_OBJS) $(LIBDEPS)
	$(LD) $(UNI_LDFLAGS) -o $@.elf $(UNI_SRT0) $(UNI_OBJS) $(LIBDEPS) \
		> $(UNI_LOADMAP)
	cp $@.elf $@.strip
	$(STRIP) $@.strip
	$(RM) $@; ./$(ELFCONV) $@.strip $@

$(UNIBOOT)_lint: $(UNI_L_OBJS) $(L_LIBDEPS)
	$(LINT.2) $(UNI_L_LDFLAGS) $(UNI_L_OBJS) $(UNI_LIBS)
	touch boot.bin_lint

#
# The UFS and NFS booters are simply copies of the unified booter.
#
$(UFSBOOT): $(UNIBOOT)
	cp $(UNIBOOT) $@

$(UFSBOOT)_lint: $(UNIBOOT)_lint
	cp $(UNIBOOT)_lint $@

$(NFSBOOT): $(INETCONV) $(UNIBOOT)
	./$(INETCONV) $(UNIBOOT) $@

$(NFSBOOT)_lint: $(UNIBOOT)_lint
	cp $(UNIBOOT)_lint $@

#
# expr.o depends on y.tab.o because expr.c includes y.tab.h,
# which is in turn generated by yacc.
#
expr.o: exprgram.o

#
# yacc automatically adds some #includes that aren't right in our
# stand-alone environment, so we use sed to change the generated C.
#
%.o: $(MACH_DIR)/%.y
	$(YACC.y) $<
	sed -e '/^#include.*<malloc.h>/d'\
		-e '/^#include.*<stdlib.h>/d'\
		-e '/^#include.*<string.h>/d' < y.tab.c > y.tab_incfix.c
	$(COMPILE.c) -o $@ y.tab_incfix.c
	$(POST_PROCESS_O)

%.ln: $(MACH_DIR)/%.y
	@($(YACC.y) $<)
	@(sed -e '/^#include.*<malloc.h>/d'\
		-e '/^#include.*<stdlib.h>/d'\
		-e '/^#include.*<string.h>/d' < y.tab.c > y.tab_incfix.c)
	@($(LHEAD) $(LINT.c) -c y.tab_incfix.c $(LTAIL))
	@(mv y.tab_incfix.ln $@)

include $(BOOTSRCDIR)/Makefile.rules

clean:
	$(RM) $(SRT0_OBJ) $(OBJS) $(CONF_OBJS)
	$(RM) $(L_OBJS) $(CONF_L_OBJS)
	$(RM) y.tab.* exprgram.c a.out core y.tmp.c
	$(RM) $(UNIBOOT).elf $(UNIBOOT).strip

clobber: clean
	$(RM) $(ELFCONV) $(INETCONV) $(UNIBOOT) $(UNI_LOADMAP)
	$(RM) $(UFSBOOT) $(HSFSBOOT) $(NFSBOOT) y.tab.c y.tab_incfix.c

lint: $(UFSBOOT)_lint $(NFSBOOT)_lint

include $(BOOTSRCDIR)/Makefile.targ

FRC:
