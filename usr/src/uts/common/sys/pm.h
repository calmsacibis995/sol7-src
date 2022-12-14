/*
 * Copyright (c) 1993 - 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_PM_H
#define	_SYS_PM_H

#pragma ident	"@(#)pm.h	1.14	97/07/17 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

typedef enum {
	PM_SCHEDULE,
	PM_GET_IDLE_TIME,
	PM_GET_NUM_CMPTS,
	PM_GET_THRESHOLD,
	PM_SET_THRESHOLD,
	PM_GET_NORM_PWR,
	PM_SET_CUR_PWR,
	PM_GET_CUR_PWR,
	PM_GET_NUM_DEPS,
	PM_GET_DEP,
	PM_ADD_DEP,
	PM_REM_DEP,
	PM_REM_DEVICE,
	PM_REM_DEVICES,
	PM_REPARSE_PM_PROPS,	/* used only by ddivs pm tests */
	PM_DISABLE_AUTOPM,
	PM_REENABLE_AUTOPM,
	PM_SET_NORM_PWR
} pm_cmds;

/*
 * Old name for these ioctls.
 */
#define	PM_GET_POWER	PM_GET_NORM_PWR
#define	PM_SET_POWER	PM_SET_CUR_PWR

typedef struct {
	caddr_t	who;		/* Device to configure */
	int	select;		/* Selects the component or dependent */
				/* of the device */
	int	level;		/* Power or threshold level */
	caddr_t dependent;	/* Buffer to hold name of dependent */
	int	size;		/* Size of dependent buffer */
} pm_request;

#ifdef _SYSCALL32
typedef struct {
	caddr32_t	who;	/* Device to configure */
	int32_t		select;	/* Selects the component or dependent */
				/* of the device */
	int32_t		level;	/* Power or threshold level */
	caddr32_t	dependent;	/* Buffer to hold name of */
					/* dependent */
	int32_t		size;	/* Size of dependent buffer */
} pm_request32;
#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PM_H */
