/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_POWERIO_H
#define	_SYS_POWERIO_H

#pragma ident	"@(#)powerio.h	1.1	98/01/08 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	pIOC			('p' << 8)
#define	POWER_BUTTON_MON	(pIOC | 0)

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_POWERIO_H */
