/*
 * Copyright (c) 1990, 1996-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)l_strplumb.c	1.29	97/10/22 SMI"

#include	<sys/param.h>
#include	<sys/types.h>
#include	<sys/user.h>
#include	<sys/vfs.h>
#include	<sys/vnode.h>
#include	<sys/file.h>
#include	<sys/stream.h>
#include	<sys/stropts.h>
#include	<sys/strsubr.h>
#include	<sys/dlpi.h>
#include	<sys/vnode.h>
#include	<sys/socket.h>
#include	<sys/sockio.h>
#include	<sys/cmn_err.h>
#include	<net/if.h>
#include	<sys/sad.h>
#include	<sys/kstr.h>
#include	<sys/ddi.h>
#include	<sys/sunddi.h>

#include	<sys/cred.h>
#include	<sys/sysmacros.h>

#include	<sys/modctl.h>

/*
 * Routines to allow strplumb() legitimate access
 * to the kernel.
 */
int
kstr_open(major_t maj, minor_t min, vnode_t **vpp, int *fd)
{
	int		error;
	vnode_t		*vp;
	int		fdp;

	vp = makespecvp(makedevice(maj, min), VCHR);

	if (error = fassign(&vp, FREAD|FWRITE, &fdp)) {
		return (error);
	}

	if (vpp)
		*vpp = vp;
	if (fd)
		*fd = fdp;
	return (0);
}

int
kstr_plink(vp, fd, mux_id)
	vnode_t	*vp;
	int	fd;
	int	*mux_id;
{
	int	id;
	int	error;

	if (error = strioctl(vp, I_PLINK, (intptr_t)fd, 0,
			K_TO_K, CRED(), &id))
		return (error);
	if (mux_id)
		*mux_id = id;
	return (0);
}

int
kstr_unplink(vp, mux_id)
	vnode_t	*vp;
	int	mux_id;
{
	int	rval;

	return (strioctl(vp, I_PUNLINK, (intptr_t)mux_id, 0,
		K_TO_K, CRED(), &rval));
}

int
kstr_push(vp, mod)
	vnode_t	*vp;
	char	*mod;
{
	int	rval;

	return (strioctl(vp, I_PUSH, (intptr_t)mod, 0, K_TO_K, CRED(), &rval));
}

int
kstr_pop(vp)
	vnode_t	*vp;
{
	int	rval;

	return (strioctl(vp, I_POP, 0, 0, K_TO_K, CRED(), &rval));
}

int
kstr_close(vp, fd)
	vnode_t	*vp;
	int	fd;
{
	file_t	*fp;

	if (vp == (vnode_t *)NULL && fd == -1)
		return (EINVAL);

	if (fd != -1) {
		if (fp = getandset(fd)) {
			(void) closef(fp);
			return (0);
		} else {
			printf("close_fd: bad fd %d\n", fd);
			return (EINVAL);
		}

	} else
		return (VOP_CLOSE(vp, FREAD|FWRITE, 1, (offset_t)0, CRED()));
}

int
kstr_ioctl(struct vnode *vp, int cmd, intptr_t arg)
{
	int	rval;

	return (strioctl(vp, cmd, arg, 0, K_TO_K, CRED(), &rval));
}

/*
 * Optionally send data (if smp set) and optionally receive data (if rmp is
 * set). If timeo is NULL the reception will sleep until a message is
 * received; otherwise the sleep is limited to the specified amount of time.
 */
int
kstr_msg(vnode_t *vp, mblk_t *smp, mblk_t **rmp, timestruc_t *timeo)
{
	int			error;
	clock_t			timout;	/* milliseconds */
	u_char 			pri;
	int 			pflag;
	rval_t			rval;

	if (rmp == NULL && timeo != NULL &&
	    (timeo->tv_sec != 0 || timeo->tv_nsec != 0))
		return (EINVAL);

	if (smp == NULL && rmp == NULL)
		return (EINVAL);

	if (smp != NULL) {
		/* Send message while honoring flow control */
		(void) kstrputmsg(vp, smp, NULL, 0, 0,
				MSG_BAND | MSG_HOLDSIG | MSG_IGNERROR, 0);
	}

	if (rmp == NULL) {
		/* No reply wanted by caller */
		return (0);
	}

	/*
	 * Convert from nanoseconds to milliseconds.
	 */
	if (timeo != NULL) {
		timout = timeo->tv_sec * 1000 + timeo->tv_nsec / 1000000;
		if (timout > INT_MAX)
			return (EINVAL);
	} else
		timout = -1;

	/* Wait for timeout millseconds for a message */
	pflag = MSG_ANY;
	pri = 0;
	*rmp = NULL;
	error = kstrgetmsg(vp, rmp, NULL, &pri, &pflag, timout, &rval);
	/* Callers use *rmp == NULL to determine that there was a timeout */
	if (error == ETIME)
		error = 0;
	return (error);
}

int SAD_MAJOR;	/* major number for SAD device. */
/*
 * It is the callers responsibility to make sure that "mods"
 * conforms to what is required. We do not check it here.
 *
 * "maj", "min", and "lastmin" are value-result parameters.
 */
int
kstr_autopush(int op, major_t *maj, minor_t *min, minor_t *lastmin,
    char *mods[])
{
	struct strapush	push;
	int	error;
	vnode_t	*vp;
	int	i;

	SAD_MAJOR = ddi_name_to_major("sad");
	if (op == SET_AUTOPUSH || op == CLR_AUTOPUSH) {
		if (error = kstr_open(SAD_MAJOR, ADMMIN, &vp, NULL)) {
			printf("kstr_autopush: open failed error %d\n", error);
			return (error);
		}
	} else	{
		if (error = kstr_open(SAD_MAJOR, USRMIN, &vp, NULL)) {
			printf("kstr_autopush: open failed error %d\n", error);
			return (error);
		}
	}

	switch (op) {
	case GET_AUTOPUSH:
		/* Get autopush information */

		push.sap_major = *maj;
		push.sap_minor = *min;
		if (error = kstr_ioctl(vp, SAD_GAP, (intptr_t)&push)) {
			printf("kstr_autopush: ioctl failed, error %d\n",
			    error);
			(void) kstr_close(vp, -1);
			return (error);
		}
		switch (push.sap_cmd) {
		case SAP_ONE:
			*maj = push.sap_major;
			*min = push.sap_minor;
			*lastmin = 0;
			break;

		case SAP_RANGE:
			*maj = push.sap_major;
			*min = push.sap_minor;
			*lastmin = push.sap_lastminor;
			break;

		case SAP_ALL:
			*maj = push.sap_major;
			*min = (minor_t)-1;
			break;
		}
		if (push.sap_npush > 1) {
			for (i = 0; i < push.sap_npush &&
			    mods[i] != NULL; i++)
				(void) strcpy(mods[i], push.sap_list[i]);
			mods[i] = NULL;
		}
		(void) kstr_close(vp, -1);
		return (0);

	case CLR_AUTOPUSH:
		/* Remove autopush information */

		push.sap_cmd = SAP_CLEAR;
		push.sap_minor = *min;
		push.sap_major = *maj;

		if (error = kstr_ioctl(vp, SAD_SAP, (intptr_t)&push)) {
			printf("kstr_autopush: ioctl failed, error %d\n",
			    error);
		}
		(void) kstr_close(vp, -1);
		return (error);

	case SET_AUTOPUSH:
		/* Set autopush information */

		if (*min == (minor_t)-1) {
			push.sap_cmd = SAP_ALL;
		} else if (*lastmin == 0) {
			push.sap_cmd = SAP_ONE;
		} else	{
			push.sap_cmd = SAP_RANGE;
		}

		push.sap_minor = *min;
		push.sap_major = *maj;
		if (lastmin)
			push.sap_lastminor = *lastmin;
		else
			push.sap_lastminor = 0;

		/* pain */
		for (i = 0; i < MAXAPUSH && mods[i] != (char *)NULL; i++) {
			(void) strcpy(push.sap_list[i], mods[i]);
		}
		push.sap_npush = i;
		push.sap_list[i][0] = '\0';

		if (error = kstr_ioctl(vp, SAD_SAP, (intptr_t)&push)) {
			printf("kstr_autopush: ioctl failed, vp %p, error %d\n",
			    (void *)vp, error);
		}
		(void) kstr_close(vp, -1);
		return (error);

	default:
		(void) kstr_close(vp, -1);
		return (EINVAL);
	}
}
