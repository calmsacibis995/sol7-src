#
#ident "@(#)Makefile	1.1	97/06/30 SMI"
#
# Copyright (c) 1994-1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# SPARCv9 architecture Makefile for Standalone Library
#

TOPDIR	=	../../..

# XXX - libsa.a is supposedly architecture-neutral; however, some more or
# less generic headers include machine specific headers like machparam.h.
# To satisfy such inclusions, even though the contents of those headers
# aren't used, we have to define $(KARCH) and $(ARCHVERS).
KARCH=	sun4u
ARCHVERS= v9

include $(TOPDIR)/Makefile.master
include $(TOPDIR)/lib/Makefile.lib

#
# Define the set of libraries made for this architecture.
#
LIBSA=		libsa.a
LIBSA_L=	llib-lsa.ln

LIBKADB=	libsadb.a
LIBKADB_L=	llib-lsadb.ln

LIBUFS=		libufs.a
LIBUFS_L=	llib-lufs.ln

LIBNFS_INET=	libnfs_inet.a
LIBNFS_INET_L=	llib-lnfs_inet.ln

LIBHSFS=	libhsfs.a
LIBHSFS_L=	llib-lhsfs.ln

LIBCACHEFS=	libcachefs.a
LIBCACHEFS_L=	llib-lcachefs.ln

ALL_LIBS=	$(LIBSA) $(LIBUFS) $(LIBHSFS) $(LIBNFS_INET) $(LIBKADB) \
		$(LIBCACHEFS)
ALL_LIBS_L=	$(LIBSA_L) $(LIBUFS_L) $(LIBHSFS_L) $(LIBNFS_INET_L) \
		$(LIBKADB_L) $(LIBCACHEFS_L)

STANDDIR=	../..
SYSDIR	=	$(TOPDIR)/uts

CPPDEFS	= 	$(ARCHOPTS) -D$(KARCH) -D_BOOT -D_KADB -D_KERNEL -D_MACHDEP
CPPINCS	= 	$(LOC_CPP) -I. -I$(STANDDIR) -I$(SYSDIR)/$(TARG_MACH) \
		-I$(SYSDIR)/$(KARCH) -I$(SYSDIR)/$(MACH)/$(ARCHVERS) \
		-I$(SYSDIR)/sun -I$(SYSDIR)/common
ASINCS	=	$(CPPINCS)
CPPFLAGS=	$(CPPDEFS) $(CPPINCS) $(CPPBOOT) $(CPPFLAGS.master)
AS_CPPFLAGS=	$(CPPFLAGS)
ASFLAGS=	-P -D__STDC__ -D_ASM

FSDIR=		../fs
FSCMNDIR=	$(FSDIR)/common
UFSDIR=		$(FSDIR)/ufs
HSFSDIR=	$(FSDIR)/hsfs
NFS_INETDIR=	$(FSDIR)/nfs_inet
CACHEFSDIR=	$(FSDIR)/cachefs

include $(UFSDIR)/Makefile.ufs
include $(HSFSDIR)/Makefile.hsfs
include $(NFS_INETDIR)/Makefile.nfs_inet
include $(CACHEFSDIR)/Makefile.cachefs

CMNDIR=		../common
CMNOBJ=		bcmp.o bcopy.o bzero.o \
		memlist.o printf.o \
		standalloc.o strcat.o strchr.o strcmp.o strcpy.o \
		strlen.o strncat.o strncmp.o strncpy.o strrchr.o \
		strstr.o
CMNOBJ_L=	bcmp.ln bcopy.ln bzero.ln \
		memlist.ln printf.ln \
		standalloc.ln strcat.ln strchr.ln strcmp.ln strcpy.ln \
		strlen.ln strncat.ln strncmp.ln strncpy.ln strrchr.ln \
		strstr.ln
CMNSRC=		$(CMNOBJ:%.o=$(CMNDIR)/%.c)

FSCMNOBJ=	cache.o diskread.o fsswitch.o
FSCMNOBJ_L=	cache.ln diskread.ln fsswitch.ln
FSCMNSRC=	$(FSCMNOBJ:%.o=$(FSCMNDIR)/%.c)

MACHOBJ=	_setjmp.o
MACHOBJ_L=	_setjmp.ln
MACHSRC=	_setjmp.s

GENOBJS=	$(CMNOBJ) $(FSCMNOBJ) $(MACHOBJ)
GENOBJS_L=	$(CMNOBJ_L) $(FSCMNOBJ_L) $(MACHOBJ_L)
PAROBJS=	$(GENOBJS:%=$(OBJSDIR)/%)
PAROBJS_L=	$(GENOBJS_L:%=$(OBJSDIR)/%)

#
# This is to allow parallel make to work ... the current
# version doesn't handle the dynamic dependency in Makefile.lib
#
.PARALLEL:	$(GENOBJS:%.o=objs/%.o)
.PARALLEL:	$(GENOBJS:%.o=kadbobjs/%.o)
.PARALLEL:	$(GENOBJS_L:%.ln=objs/%.ln)
.PARALLEL:	$(GENOBJS_L:%.ln=kadbobjs/%.ln)
.PARALLEL:	$(ALL_LIBS) $(ALL_LIBS_L)

# Arch-specific device configuration as listed in conf.c
CONFOBJ=

#
# OBJECTS are the union of .o's to build the libraries.
# SOURCES are the union of source files to build the lint libraries.
# OBJECTS_L  are the union of .ln's for lint pass 1 on the sources.
#
# The fs/* Makefiles include their own OBJECTS and SOURCES
# in these lists.
#
# SPRINTF is defined in Makefile.lib, depending
# on the library being built.
#
OBJECTS +=	$(GENOBJS) $(SPRINTF.o) 
OBJECTS_L +=	$(GENOBJS_L) $(SPRINTF.ln) 
SOURCES +=	$(CMNSRC) $(FSCMNSRC) $(MACHSRC) $(SPRINTF.c)

include $(TOPDIR)/psm/Makefile.psm.64

include ../Makefile.com

.KEEP_STATE:

