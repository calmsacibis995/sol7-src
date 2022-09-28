/*
 * Copyright (c) 1991 by Sun Microsystems, Inc.
 */

#ifndef	_RPC_H
#define	_RPC_H

#pragma ident	"@(#)rpc.h	1.6	97/12/17 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Rather than include rpc/types.h, and run into the conflict of
 * the kernel/boot kmem_alloc prototypes, just include what we need
 * here.
 */
#ifndef	_RPC_TYPES_H
#define	_RPC_TYPES_H
#endif	/* _RPC_TYPES_H */

#define	bool_t	int
#define	enum_t	int
#define	__dontcare__	-1

#ifndef	TRUE
#define	TRUE	(1)
#endif

#ifndef	FALSE
#define	FALSE	(0)
#endif

#ifndef NULL
#define	NULL	0
#endif

/* needed for some of the function prototypes in the kernel rpc headers */
typedef uint32_t rpcprog_t;
typedef uint32_t rpcvers_t;
typedef uint32_t rpcproc_t;
typedef uint32_t rpcprot_t;
typedef uint32_t rpcport_t;

typedef int32_t rpc_inline_t;

#ifdef	__cplusplus
}
#endif

#include <sys/time.h>

#endif	/* _RPC_H */
