/*
 * Copyright (c) 1995,1996-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef	_DOOR_H
#define	_DOOR_H

#pragma ident	"@(#)door.h	1.6	98/01/06 SMI"

#include <sys/types.h>
#include <sys/door.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _ASM

/*
 * Doors API
 */
int	door_create(void (*)(), void *, uint_t);
int	door_revoke(int);
int	door_info(int, door_info_t *);
int	door_call(int, door_arg_t *);
int	door_return(char *, size_t, door_desc_t *, uint_t);
int	door_cred(door_cred_t *);
int	door_bind(int);
int	door_unbind(void);
void	(*door_server_create(void (*)())) (void (*)());

#endif /* _ASM */

#ifdef __cplusplus
}
#endif

#endif	/* _DOOR_H */
