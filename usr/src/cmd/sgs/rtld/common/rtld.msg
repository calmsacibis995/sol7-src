#
# Copyright (c) 1997 by Sun Microsystems, Inc.
# All rights reserved.
#
# ident	"@(#)rtld.msg	1.37	98/01/21 SMI"

@ _START_

# Message file for cmd/sgs/rtld (ld.so.1)

@ MSG_ID_RTLD

# Message formatting errors

@ MSG_FMT_LONG		"ld.so.1: msgbuf overflow.  error format string: \
			 `%s'"

# Argument processing errors

@ MSG_ARG_ILLMODE	"illegal mode: %d"
@ MSG_ARG_ILLPATH	"illegal pathname"
@ MSG_ARG_ILLSYM	"illegal symbol name"
@ MSG_ARG_INVADDR	"address 0x%llx does not fall within any mapped object"
@ MSG_ARG_INVHNDL	"invalid handle"
@ MSG_ARG_ILLVAL	"illegal request value"


# General error diagnostics

@ MSG_GEN_NULLFILE	"attempt to open file with null name"
@ MSG_GEN_NOFILE	"%s: can't find file"
@ MSG_GEN_CACHED	"%s: cached file in use"
@ MSG_GEN_UNKNFILE	"%s: unknown file type"
@ MSG_GEN_CORTRUNC	"%s: corrupt or truncated file"
@ MSG_GEN_NOSYM		"%s: can't find symbol"
# _CHKMSG_SKIP_BEGIN_	i386
# _CHKMSG_SKIP_BEGIN_	sparcv9
@ MSG_GEN_OLDREV	"%s: has older revision than expected %s"
# _CHKMSG_SKIP_END_	sparcv9
# _CHKMSG_SKIP_END_	i386
@ MSG_GEN_BADTYPE	"%s: bad ELF type: %s"
@ MSG_GEN_INVPRGHDR	"%s: invalid ELF program header, segments out of order"
@ MSG_GEN_NOLOADSEG	"%s: no loadable ELF segments found"
@ MSG_GEN_MAPINUSE	"%s: required mapping 0x%llx size 0x%llx, \
			 is already in use by file %s"
@ MSG_GEN_NOOPEN	"%s: DF_1_NOOPEN tagged object may not be dlopen()'ed"


# Relocation processing messages (some of these are required to satisfy
# do_reloc(), which is common code used by cmd/sgs/libld - make sure both
# message files remain consistant).

# _CHKMSG_SKIP_BEGIN_ i386
@ MSG_REL_ERR_STR	"relocation error: %s:"
@ MSG_REL_ERR_WITH_FILE	"relocation error: file %s: "
@ MSG_REL_ERR_FILE	" file %s: "
@ MSG_REL_ERR_SYM	" symbol %s: "
@ MSG_REL_ERR_VALUE	" value 0x%llx"
@ MSG_REL_ERR_OFF	" offset 0x%llx\n"
@ MSG_REL_UNIMPL	" unimplemented relocation type: %d"
@ MSG_REL_NONALIGN	" offset 0x%llx is non-aligned\n"
@ MSG_REL_UNNOBITS	" unsupported number of bits: %d"
@ MSG_REL_NOFIT		" value 0x%llx does not fit\n"
@ MSG_REL_LOOSEBITS	" looses %d bits at"
# _CHKMSG_SKIP_BEGIN_ sparcv9
@ MSG_REL_OVERFLOW	"relocation error: file %s: symbol %s: \
			 value 0x%x overflows %d bits at offset 0x%x"
# _CHKMSG_SKIP_END_ sparcv9
@ MSG_REL_PLTOFF	"non-aligned .plt offset"
# _CHKMSG_SKIP_END_ i386

# _CHKMSG_SKIP_BEGIN_ sparc
# _CHKMSG_SKIP_BEGIN_ sparcv9
@ MSG_REL_RELOFF	"non-aligned relocation offset"
@ MSG_REL_UNSUPSZ	"relocation error: %s: file %s: symbol %s: \
			 offset size (%d bytes) is not supported"
# _CHKMSG_SKIP_END_ sparcv9
# _CHKMSG_SKIP_END_ sparc

@ MSG_REL_NOSYM		"relocation error: file %s: symbol %s: \
			 referenced symbol not found"
@ MSG_REL_PLTENTRY	"relocation error: %s: unable to process .plt[%lld]: \
			 called from 0x%llx: %s"
@ MSG_REL_PLTREF	"unidentifiable procedure reference"


# SPARCV9-only messages

# _CHKMSG_SKIP_BEGIN_ sparc
# _CHKMSG_SKIP_BEGIN_ i386
@ MSG_REL_EXTND_PLTREF	"relocation error: %s: unable to process extended \
			plt entry .plt[%lld]"
# _CHKMSG_SKIP_END_ i386
# _CHKMSG_SKIP_END_ sparc


# System call messages.

@ MSG_SYS_OPEN		"%s: open failed: %s"
@ MSG_SYS_MMAP		"%s: mmap failed: %s"
@ MSG_SYS_MPROT		"%s: mprotect failed: %s"
@ MSG_SYS_PROC		"%s: /proc error: %s"


# Debugging initialization diagnostics and formats

@ MSG_DBG_DISABLE	"debugging disabled; unable to process debug library"

# Link Audit diagnostic message formats

@ MSG_AUD_NOOPEN	"audit_init: unable to open `%s'"
@ MSG_AUD_REQSYM	"audit_init: required audit library symbol `%s' \
			not present.  Unable to load library `%s'\n"
@ MSG_AUD_PROFAUDINC	"LD_AUDIT and LD_PROFILE are mutually exclusive\n"

# Versioning diagnostics.

@ MSG_VER_NFOUND	"%s: version `%s' not found (required by file %s)"


# Diagnostics generated under the control of ldd(1).

@ MSG_LDD_VER_FIND	"   find version=%s\n"
@ MSG_LDD_VER_NFOUND	"\t%s (%s) =>\t (version not found)\n"

# _CHKMSG_SKIP_BEGIN_ i386
@ MSG_LDD_SYM_NFOUND	"\tsymbol not found: %s\t\t(%s)\n"
# _CHKMSG_SKIP_END_ i386

@ MSG_LDD_PTH_TRYING	"    trying path=%s\n"
@ MSG_LDD_PTH_ENVDIR	"    search path=%s  (LD_LIBRARY_PATH)\n"
@ MSG_LDD_PTH_RPATH	"    search path=%s  (RPATH from file %s)\n"
@ MSG_LDD_PTH_BGNDFL	"    search path="
@ MSG_LDD_PTH_ENDDFL	" (default)\n"
@ MSG_LDD_PTH_IGNORE	"    ignore path=%s  (insecure directory name)\n"

@ MSG_LDD_FIL_FIND	"\n   find library=%s; required by %s\n"
@ MSG_LDD_FIL_NFOUND	"\t%s =>\t (file not found)\n"
@ MSG_LDD_FIL_CACHE	"  (cache)"

@ MSG_LDD_CPY_SIZDIF	"\tcopy relocation sizes differ: %s\n\
			 \t\t(file %s size=%llx; file %s size=%llx);\n"
@ MSG_LDD_CPY_INSDATA	"\t\t%s size used; possible insufficient data copied\n"
@ MSG_LDD_CPY_DATRUNC	"\t\t%s size used; possible data truncation\n"

@ MSG_LDD_INIT_FMT_01	"   init library=%s\n"
@ MSG_LDD_INIT_FMT_02	"   init library=%s (cyclic dependency on"


# Error diagnostic standard prefixes.

@ MSG_ERR_WARNING	"warning: "
@ MSG_ERR_FATAL		"fatal: "
@ MSG_ERR_ELF		"elf error: "

@ MSG_STR_UNKNOWN	"(unknown)"

@ _END_


@ MSG_LDD_FIL_PATH	"\t%s%s\n"
@ MSG_LDD_FIL_EQUIV	"\t%s =>\t %s%s\n"
@ MSG_LDD_FMT_FILE	"%s "
@ MSG_LDD_FMT_END	")\n"
@ MSG_LDD_VER_FOUND	"\t%s (%s) =>\t %s\n"



# The following strings represent reserved words, files, pathnames and symbols.
# Reference to this strings is via the MSG_ORIG() macro, and thus no message
# translation is required.

@ MSG_STR_EMPTY		""
@ MSG_STR_NEGATE	"-"
@ MSG_STR_ZERO		"0"
@ MSG_STR_HEX		"0x"
@ MSG_STR_ELF		"ELF"
@ MSG_STR_EMSGFOR1	"%s: %s: %s"
@ MSG_STR_EMSGFOR2	"%s: %s"
@ MSG_STR_HEXNUM	"0123456789abcdef"
@ MSG_STR_CFLAG		"\t(C)"
@ MSG_STR_NL		"\n"
@ MSG_STR_SLASH		"/"
# _CHKMSG_SKIP_BEGIN_	i386
# _CHKMSG_SKIP_BEGIN_	sparcv9
@ MSG_STR_AOUT		"AOUT"
# _CHKMSG_SKIP_END_	sparcv9
# _CHKMSG_SKIP_END_	i386


@ MSG_SUNW_OST_SGS	"SUNW_OST_SGS"
@ MSG_SUNW_OST_OSLIB	"SUNW_OST_OSLIB"

@ MSG_TKN_PLATFORM	"PLATFORM"
@ MSG_TKN_ORIGIN	"ORIGIN"
@ MSG_TKN_BINDINGS	"bindings"
@ MSG_TKN_POSIX		"POSIX"

@ MSG_FMT_PATH		"%s/%s"
@ MSG_FMT_CWD		"."
@ MSG_FMT_PARENT	"/.."
# _CHKMSG_SKIP_BEGIN_	i386
# _CHKMSG_SKIP_BEGIN_	sparcv9
@ MSG_FMT_4XPATH	"./%s"
@ MSG_FMT_4XLIB		"lib%s.so.%d.%d"
# _CHKMSG_SKIP_END_	sparcv9
# _CHKMSG_SKIP_END_	i386
@ MSG_FMT_PROC		"/proc/%d"
@ MSG_FMT_MSGFILE	"/usr/lib/locale/%s/LC_MESSAGES/%s.mo"

# _CHKMSG_SKIP_BEGIN_	i386
# _CHKMSG_SKIP_BEGIN_	sparcv9
@ MSG_FIL_DOTSODOT	".so."
@ MSG_FIL_LIB		"lib"
# _CHKMSG_SKIP_END_	sparcv9
# _CHKMSG_SKIP_END_	i386
@ MSG_FIL_LIBLD		"libld.so"
@ MSG_FIL_LIBDBG	"liblddbg.so"
@ MSG_FIL_LIBRTLD	"librtld.so"
@ MSG_FIL_RTLD		"ld.so.1"
@ MSG_FIL_CACHE		"ld.cache"
@ MSG_FIL_DEBUG		"ld.debug"
@ MSG_FIL_LDPROF	"ldprof.so.1"

@ MSG_SYM_ELFERRMSG	"elf_errmsg"
@ MSG_SYM_ELFERRNO	"elf_errno"
@ MSG_SYM_ELFPLTTRACE	"elf_plt_trace"
@ MSG_SYM_ENVIRON	"_environ"
#
# _CHKMSG_SKIP_BEGIN_ sparc
# _CHKMSG_SKIP_BEGIN_ sparcv9
#
@ MSG_SYM_CTYPE		"__ctype"
@ MSG_SYM_IOB		"__iob"
@ MSG_SYM_PLT		"_PROCEDURE_LINKAGE_TABLE_"
# _CHKMSG_SKIP_BEGIN_	i386
@ MSG_SYM_GOT		"_GLOBAL_OFFSET_TABLE_"
# _CHKMSG_SKIP_END_ sparc
# _CHKMSG_SKIP_END_ sparcv9
# _CHKMSG_SKIP_END_ i386

@ MSG_SYM_LAPREINIT	"la_preinit"
@ MSG_SYM_LAVERSION	"la_version"
@ MSG_SYM_LAOBJOPEN	"la_objopen"
@ MSG_SYM_LAOBJCLOSE	"la_objclose"
@ MSG_SYM_LAV8PLTENTER	"la_sparcv8_pltenter"
@ MSG_SYM_LAV9PLTENTER	"la_sparcv9_pltenter"
@ MSG_SYM_LAX86PLTENTER	"la_i86_pltenter"
@ MSG_SYM_LAPLTEXIT	"la_pltexit"
@ MSG_SYM_LAPLTEXIT64	"la_pltexit64"
@ MSG_SYM_LASYMBIND	"la_symbind32"
@ MSG_SYM_LASYMBIND64	"la_symbind64"
@ MSG_SYM_LADYNDATA	"la_dyndata"
@ MSG_SYM_PROFOPEN	"profile_open"
@ MSG_SYM_PROFCGINTRP	"plt_cg_interp"

@ MSG_SPECFIL_DYNPLT	"dyn_plt(ld.so.1)"

@ MSG_PTH_LIBSYS	"/usr/lib/libsys.so.1"
#
# _CHKMSG_SKIP_BEGIN_ sparc
# _CHKMSG_SKIP_BEGIN_ sparcv9
#
@ MSG_PTH_LIBC		"/usr/lib/libc.so.1"
# _CHKMSG_SKIP_END_	sparcv9
# _CHKMSG_SKIP_END_	sparc
@ MSG_PTH_RTLD		"/usr/lib/ld.so.1"
@ MSG_PTH_USRLIB	"/usr/lib"
# _CHKMSG_SKIP_BEGIN_ i386
# _CHKMSG_SKIP_BEGIN_ sparcv9
@ MSG_PTH_USRLCLIB	"/usr/local/lib"
@ MSG_PTH_USR4LIB	"/usr/4lib"
@ MSG_PTH_USRUCBLIB	"/usr/ucblib"
# _CHKMSG_SKIP_END_	sparcv9
# _CHKMSG_SKIP_END_	i386

@ MSG_PTH_RTLD_64	"/usr/lib/sparcv9/ld.so.1"
@ MSG_PTH_USRLIB64	"/usr/lib/sparcv9"
@ MSG_PTH_DEVNULL	"/dev/null"
@ MSG_PTH_DEVZERO	"/dev/zero"
@ MSG_PTH_CACHE		"/var/ld/default/ld.cache"
@ MSG_PTH_DEBUG		"/var/ld/default/ld.debug"

@ MSG_LD_LIBPATH	"LIBRARY_PATH="
@ MSG_LD_LIBPATH_64	"LIBRARY_PATH_64="
@ MSG_LD_LOADFLTR	"LOADFLTR="
@ MSG_LD_PRELOAD	"PRELOAD="
@ MSG_LD_TRACEOBJS	"TRACE_LOADED_OBJECTS"
@ MSG_LD_TRACE_E	"_E="
@ MSG_LD_TRACE_A	"_A="
@ MSG_LD_TRACEPTHS	"TRACE_SEARCH_PATHS="
@ MSG_LD_VERBOSE	"VERBOSE="
@ MSG_LD_WARN		"WARN="
@ MSG_LD_INIT		"INIT="
@ MSG_LD_BINDINGS	"BINDINGS="
@ MSG_LD_BIND_NOW	"BIND_NOW="
@ MSG_LD_BIND_NOT	"BIND_NOT="
@ MSG_LD_NOAUXFLTR	"NOAUXFLTR="
@ MSG_LD_NOVERSION	"NOVERSION="
@ MSG_LD_NOCACHE	"NOCACHE="
@ MSG_LD_CACHEDIR	"CACHEDIR="
@ MSG_LD_TIMING		"TIMING="
@ MSG_LD_PROFILE	"PROFILE"
@ MSG_LD_DEBUG		"DEBUG"
@ MSG_LD_OUTPUT		"_OUTPUT="
@ MSG_LD_AUDIT		"AUDIT="
@ MSG_LD_AUDIT_64	"AUDIT_64="
@ MSG_LD_AUDIT_ARGS	"AUDIT_ARGS="
@ MSG_LD_BREADTH	"BREADTH="
@ MSG_LD_ORIGIN		"ORIGIN="
@ MSG_LD_NODIRECT	"NODIRECT="
@ MSG_LD_FLAGS		"FLAGS="

@ MSG_LC_ALL		"ALL="
@ MSG_LC_MESSAGES	"MESSAGES="

@ MSG_LDFLG_NOLAZY	"nolazyload"
@ MSG_LDFLG_NODIRECT	"nodirect"

@ MSG_EMG_NOSPACE	"ld.so.1: internal: malloc failed"

@ MSG_DBG_FMT_DIAG	"%5.5d: "
@ MSG_DBG_FMT_FILE	"%s.%5.5d"
@ MSG_DBG_FMT_THREAD	"%5.5d: %d: "
