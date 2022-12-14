/*
 * Copyright (c) 1993-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_ISA_DEFS_H
#define	_SYS_ISA_DEFS_H

#pragma ident	"@(#)isa_defs.h	1.15	97/11/22 SMI"

/*
 * This header file serves to group a set of well known defines and to
 * set these for each instruction set architecture.  These defines may
 * be divided into two groups;  characteristics of the processor and
 * implementation choices for Solaris on a processor.
 *
 * Processor Characteristics:
 *
 * _LITTLE_ENDIAN / _BIG_ENDIAN:
 *	The natural byte order of the processor.  A pointer to an int points
 *	to the least/most significant byte of that int.
 *
 * _STACK_GROWS_UPWARD / _STACK_GROWS_DOWNWARD:
 *	The processor specific direction of stack growth.  A push onto the
 *	stack increases/decreases the stack pointer, so it stores data at
 *	successively higher/lower addresses.  (Stackless machines ignored
 *	without regrets).
 *
 * _LONG_LONG_HTOL / _LONG_LONG_LTOH:
 *	A pointer to a long long points to the most/least significant long
 *	within that long long.
 *
 * _BIT_FIELDS_HTOL / _BIT_FIELDS_LTOH:
 *	The C compiler assigns bit fields from the high/low to the low/high end
 *	of an int (most to least significant vs. least to most significant).
 *
 * _IEEE_754:
 *	The processor (or supported implementations of the processor)
 *	supports the ieee-754 floating point standard.  No other floating
 *	point standards are supported (or significant).  Any other supported
 *	floating point formats are expected to be cased on the ISA processor
 *	symbol.
 *
 * _CHAR_IS_UNSIGNED / _CHAR_IS_SIGNED:
 *	The C Compiler implements objects of type `char' as `unsigned' or
 *	`signed' respectively.  This is really an implementation choice of
 *	the compiler writer, but it is specified in the ABI and tends to
 *	be uniform across compilers for an instruction set architecture.
 *	Hence, it has the properties of a processor characteristic.
 *
 * _CHAR_ALIGNMENT / _SHORT_ALIGNMENT / _INT_ALIGNMENT / _LONG_ALIGNMENT /
 * _LONG_LONG_ALIGNMENT / _DOUBLE_ALIGNMENT / _LONG_DOUBLE_ALIGNMENT /
 * _POINTER_ALIGNMENT:
 *	The ABI defines alignment requirements of each of the primitive
 *	object types.  Some, if not all, may be hardware requirements as
 * 	well.  The values are expressed in "byte-alignment" units.
 *
 * _MAX_ALIGNMENT:
 *	The most stringent alignment requirement as specified by the ABI.
 *	Equal to the maximum of all the above _XXX_ALIGNMENT values.
 *
 * _ALIGNMENT_REQUIRED:
 *	True or false (1 or 0) whether or not the hardware requires the ABI
 *	alignment.
 *
 *
 * Implementation Choices:
 *
 * _ILP32 / _LP64:
 *	This specifies the compiler data type implementation as specified in
 *	the relevant ABI.  The choice between these is strongly influenced
 *	by the underlying hardware, but is not absolutely tied to it.
 *	Currently only two data type models are supported:
 *
 *	_ILP32:
 *		Int/Long/Pointer are 32 bits.  This is the historical UNIX
 *		and Solaris implementation.  Due to its historical standing,
 *		this is the default case.
 *
 *	_LP64:
 *		Long/Pointer are 64 bits, Int is 32 bits.  This is the chosen
 *		implementation for 64-bit ABIs such as SPARC V9.
 *
 *	_I32LPx:
 *		A compilation environment where 'int' is 32-bit, and
 *		longs and pointers are simply the same size.
 *
 *	In all cases, Char is 8 bits and Short is 16 bits.
 *
 * _SUNOS_VTOC_8 / _SUNOS_VTOC_16 / _SVR4_VTOC_16:
 *	This specifies the form of the disk VTOC (or label):
 *
 *	_SUNOS_VTOC_8:
 *		This is a VTOC form which is upwardly compatible with the
 *		SunOS 4.x disk label and allows 8 partitions per disk.
 *
 *	_SUNOS_VTOC_16:
 *		In this format the incore vtoc image matches the ondisk
 *		version.  It allows 16 slices per disk, and is not
 *		compatible with the SunOS 4.x disk label.
 *
 *	Note that these are not the only two VTOC forms possible and
 *	additional forms may be added.  One possible form would be the
 *	SVr4 VTOC form.  The symbol for that is reserved now, although
 *	it is not implemented.
 *
 *	_SVR4_VTOC_16:
 *		This VTOC form is compatible with the System V Release 4
 *		VTOC (as implemented on the SVr4 Intel and 3b ports) with
 *		16 partitions per disk.
 *
 *
 * _DMA_USES_PHYSADDR / _DMA_USES_VIRTADDR
 *	This describes the type of addresses used by system DMA:
 *
 *	_DMA_USES_PHYSADDR:
 *		This type of DMA, used in the x86 implementation,
 *		requires physical addresses for DMA buffers.  The 24-bit
 *		addresses used by some legacy boards is the source of the
 *		"low-memory" (<16MB) requirement for some devices using DMA.
 *
 *	_DMA_USES_VIRTADDR:
 *		This method of DMA allows the use of virtual addresses for
 *		DMA transfers.
 *
 * _FIRMWARE_NEEDS_FDISK / _NO_FDISK_PRESENT
 *      This indicates the presence/absence of an fdisk table.
 *
 *      _FIRMWARE_NEEDS_FDISK
 *              The fdisk table is required by system firmware.  If present,
 *              it allows a disk to be subdivided into multiple fdisk
 *              partitions, each of which is equivalent to a separate,
 *              virtual disk.  This enables the co-existence of multiple
 *              operating systems on a shared hard disk.
 *
 *      _NO_FDISK_PRESENT
 *              If the fdisk table is absent, it is assumed that the entire
 *              media is allocated for a single operating system.
 */

#ifdef	__cplusplus
extern "C" {
#endif


/*
 * The following set of definitions characterize the Solaris on Intel systems.
 *
 * The feature test macro __i386 is generic for all processors implementing
 * the Intel 386 instruction set or a superset of it.  Specifically, this
 * includes all members of the 386, 486, and Pentium family of processors.
 */
#if defined(__i386) || defined(i386)

/*
 * Make sure that the ANSI-C "politically correct" symbol is defined.
 */
#if !defined(__i386)
#define	__i386
#endif

/*
 * Define the appropriate "processor characteristics"
 */
#define	_LITTLE_ENDIAN
#define	_STACK_GROWS_DOWNWARD
#define	_LONG_LONG_LTOH
#define	_BIT_FIELDS_LTOH
#define	_IEEE_754
#define	_CHAR_IS_SIGNED
#define	_CHAR_ALIGNMENT		1
#define	_SHORT_ALIGNMENT	2
#define	_INT_ALIGNMENT		4
#define	_LONG_ALIGNMENT		4
#define	_LONG_LONG_ALIGNMENT	4
#define	_DOUBLE_ALIGNMENT	4
#define	_LONG_DOUBLE_ALIGNMENT	4
#define	_POINTER_ALIGNMENT	4
#define	_MAX_ALIGNMENT		4
#define	_ALIGNMENT_REQUIRED	0

/*
 * Define the appropriate "implementation choices".
 */
#define	_ILP32
#if !defined(_I32LPx) && defined(_KERNEL)
#define	_I32LPx
#endif
#define	_SUNOS_VTOC_16
#define	_DMA_USES_PHYSADDR
#define	_FIRMWARE_NEEDS_FDISK

/*
 * The following set of definitions characterize the Solaris on SPARC systems.
 *
 * The symbol __sparc indicates any of the SPARC family of processor
 * architectures.  This includes SPARC V7, SPARC V8 and SPARC V9.
 *
 * The symbol sparc is a deprecated historical synonym for __sparc.
 *
 * The symbol __sparcv8 indicates the 32-bit SPARC V8 architecture as defined
 * by Version 8 of the SPARC Architecture Manual.  (SPARC V7 is close enough
 * to SPARC V8 for the former to be subsumed into the latter definition.)
 *
 * The symbol __sparcv9 indicates the 64-bit SPARC V9 architecture as defined
 * by Version 9 of the SPARC Architecture Manual.
 *
 * The symbols __sparcv8 and __sparcv9 are mutually exclusive, and are only
 * relevant when the symbol __sparc is defined.
 */
#elif defined(__sparc) || defined(sparc)

/*
 * Make sure that the ANSI-C "politically correct" symbol is defined.
 */
#if !defined(__sparc)
#define	__sparc
#endif

/*
 * You can be 32-bit or 64-bit, but not both at the same time.
 */
#if defined(__sparcv8) && defined(__sparcv9)
#error	"SPARC Versions 8 and 9 are mutually exclusive choices"
#endif

/*
 * Existing compilers do not set __sparcv8.  Years will transpire before
 * the compilers can be depended on to set the feature test macro. In
 * the interim, we'll set it here on the basis of historical behaviour;
 * if you haven't asked for SPARC V9, then you must've meant SPARC V8.
 */
#if !defined(__sparcv9) && !defined(__sparcv8)
#define	__sparcv8
#endif

/*
 * Define the appropriate "processor characteristics" shared between
 * all Solaris on SPARC systems.
 */
#define	_BIG_ENDIAN
#define	_STACK_GROWS_DOWNWARD
#define	_LONG_LONG_HTOL
#define	_BIT_FIELDS_HTOL
#define	_IEEE_754
#define	_CHAR_IS_SIGNED
#define	_CHAR_ALIGNMENT		1
#define	_SHORT_ALIGNMENT	2
#define	_INT_ALIGNMENT		4
#define	_LONG_LONG_ALIGNMENT	8
#define	_DOUBLE_ALIGNMENT	8
#define	_ALIGNMENT_REQUIRED	1

/*
 * Define the appropriate "implementation choices" shared between versions.
 */
#define	_SUNOS_VTOC_8
#define	_DMA_USES_VIRTADDR
#define	_NO_FDISK_PRESENT

/*
 * The following set of definitions characterize the implementation of
 * 32-bit Solaris on SPARC V8 systems.
 */
#if defined(__sparcv8)

/*
 * Define the appropriate "processor characteristics"
 */
#define	_LONG_ALIGNMENT		4
#define	_LONG_DOUBLE_ALIGNMENT	8
#define	_POINTER_ALIGNMENT	4
#define	_MAX_ALIGNMENT		8

/*
 * Define the appropriate "implementation choices"
 */
#define	_ILP32

#if !defined(_I32LPx) && defined(_KERNEL)
#define	_I32LPx
#endif

/*
 * The following set of definitions characterize the implementation of
 * 64-bit Solaris on SPARC V9 systems.
 */
#elif defined(__sparcv9)

/*
 * Define the appropriate "processor characteristics"
 */
#define	_LONG_ALIGNMENT		8
#define	_LONG_DOUBLE_ALIGNMENT	16
#define	_POINTER_ALIGNMENT	8
#define	_MAX_ALIGNMENT		16

/*
 * Define the appropriate "implementation choices"
 */
#define	_LP64
#if !defined(_I32LPx)
#define	_I32LPx
#endif
#define	_MULTI_DATAMODEL

#else
#error	"unknown SPARC version"
#endif

/*
 * #error is strictly ansi-C, but works as well as anything for K&R systems.
 */
#else
#error "ISA not supported"
#endif

#if defined(_ILP32) && defined(_LP64)
#error "Both _ILP32 and _LP64 are defined"
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_ISA_DEFS_H */
