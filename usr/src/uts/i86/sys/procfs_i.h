/*
 * Copyright (c) 1996-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_PROCFS_ISA_H
#define	_SYS_PROCFS_ISA_H

#pragma ident	"@(#)procfs_isa.h	1.5	98/01/06 SMI"

/*
 * Instruction Set Architecture specific component of <sys/procfs.h>
 * i386 version
 */

#include <sys/regset.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Possible values of pr_dmodel.
 * This isn't isa-specific, but it needs to be defined here for other reasons.
 */
#define	PR_MODEL_UNKNOWN 0
#define	PR_MODEL_ILP32	1	/* process data model is ILP32 */
#define	PR_MODEL_LP64	2	/* process data model is LP64 */

/*
 * To determine whether application is running native.
 */
#if defined(_LP64)
#define	PR_MODEL_NATIVE	PR_MODEL_LP64
#elif defined(_ILP32)
#define	PR_MODEL_NATIVE	PR_MODEL_ILP32
#else
#error "No DATAMODEL_NATIVE specified"
#endif	/* _LP64 || _ILP32 */

/*
 * Holds one 386/486 instruction opcode
 */
typedef	uchar_t	instr_t;

#define	NPRGREG		_NGREG
#define	prgreg_t	greg_t
#define	prgregset_t	gregset_t
#define	prfpregset	fpu
#define	prfpregset_t	fpregset_t

#if defined(_SYSCALL32)
/*
 * kernel view of the _ILP32 register set
 * (there is no difference yet)
 */
#define	prgreg32_t	greg_t
#define	prgregset32_t	gregset_t
#define	prfpregset32	fpu
#define	prfpregset32_t	fpregset_t
#endif	/* _SYSCALL32 */

/*
 * The following defines are for portability (see <sys/reg.h>).
 */
#define	R_PC	EIP
#define	R_PS	EFL
#define	R_SP	UESP
#define	R_FP	EBP
#define	R_R0	EAX
#define	R_R1	EDX

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PROCFS_ISA_H */
