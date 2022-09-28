/*	Copyright (c) 1996 Sun Microsystems, Inc.	*/
/*	  All Rights Reserved  	*/

#pragma ident	"@(#)icmp_opt_data.c	1.5	97/12/06 SMI"

#include <sys/types.h>
#include <sys/stream.h>
#define	_SUN_TPI_VERSION 2
#include <sys/tihdr.h>
#include <sys/socket.h>
#include <sys/xti_xtiopt.h>

#include <inet/common.h>
#include <inet/ip.h>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/ip_mroute.h>
#include "optcom.h"


extern	int icmp_opt_default(queue_t *, t_uscalar_t, t_uscalar_t, u_char *);
extern	int icmp_opt_get(queue_t *, t_uscalar_t, t_uscalar_t, u_char *);
extern	int icmp_opt_set(queue_t *, t_scalar_t, t_uscalar_t, t_uscalar_t,
    t_uscalar_t, u_char *, t_uscalar_t *, u_char *);

/*
 * Table of all known options handled on a ICMP protocol stack.
 *
 * Note: This table contains options processed by both ICMP and IP levels
 *       and is the superset of options that can be performed on a ICMP over IP
 *       stack.
 */
opdes_t	icmp_opt_arr[] = {

{ SO_DEBUG,	SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ SO_DONTROUTE,	SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ SO_USELOOPBACK, SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ SO_BROADCAST,	SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ SO_REUSEADDR, SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },

#ifdef	SO_PROTOTYPE
	/* icmp will only allow IPPROTO_ICMP for non-priviledged streams */
{ SO_PROTOTYPE, SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
#endif

{ SO_TYPE,	SOL_SOCKET, OA_R, OA_R, OP_PASSNEXT, sizeof (int), 0 },
{ SO_SNDBUF,	SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ SO_RCVBUF,	SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ SO_DGRAM_ERRIND, SOL_SOCKET, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },

{ IP_OPTIONS,	IPPROTO_IP, OA_RW, OA_RW, (OP_PASSNEXT|OP_VARLEN|OP_NODEFAULT),
	40, -1 /* not initialized */ },

{ IP_HDRINCL,	IPPROTO_IP, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ IP_TOS,	IPPROTO_IP, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },
{ IP_TTL,	IPPROTO_IP, OA_RW, OA_RW, OP_PASSNEXT, sizeof (int), 0 },

{ IP_MULTICAST_IF, IPPROTO_IP, OA_RW, OA_RW, OP_PASSNEXT,
	sizeof (struct in_addr), 0 /* INADDR_ANY */ },

{ IP_MULTICAST_LOOP, IPPROTO_IP, OA_RW, OA_RW, (OP_PASSNEXT|OP_DEF_FN),
	sizeof (u_char), -1 /* not initialized */},

{ IP_MULTICAST_TTL, IPPROTO_IP, OA_RW, OA_RW, (OP_PASSNEXT|OP_DEF_FN),
	sizeof (u_char), -1 /* not initialized */ },

{ IP_ADD_MEMBERSHIP, IPPROTO_IP, OA_X, OA_X, (OP_PASSNEXT|OP_NODEFAULT),
	sizeof (struct ip_mreq), -1 /* not initialized */ },

{ IP_DROP_MEMBERSHIP, 	IPPROTO_IP, OA_X, OA_X, (OP_PASSNEXT|OP_NODEFAULT),
	sizeof (struct ip_mreq), 0 },

{ MRT_INIT, IPPROTO_IP, 0, OA_X, (OP_PASSNEXT|OP_NODEFAULT), sizeof (int),
	-1 /* not initialized */ },

{ MRT_DONE, IPPROTO_IP, 0, OA_X, (OP_PASSNEXT|OP_NODEFAULT), 0,
	-1 /* not initialized */ },

{ MRT_ADD_VIF, IPPROTO_IP, 0, OA_X, (OP_PASSNEXT|OP_NODEFAULT),
	sizeof (struct vifctl), -1 /* not initialized */ },

{ MRT_DEL_VIF, 	IPPROTO_IP, 0, OA_X, (OP_PASSNEXT|OP_NODEFAULT),
	sizeof (vifi_t), -1 /* not initialized */ },

{ MRT_ADD_MFC, 	IPPROTO_IP, 0, OA_X, (OP_PASSNEXT|OP_NODEFAULT),
	sizeof (struct mfcctl), -1 /* not initialized */ },

{ MRT_DEL_MFC, 	IPPROTO_IP, 0, OA_X, (OP_PASSNEXT|OP_NODEFAULT),
	sizeof (struct mfcctl), -1 /* not initialized */ },

{ MRT_VERSION, 	IPPROTO_IP, OA_R, OA_R, (OP_PASSNEXT|OP_NODEFAULT),
	sizeof (int), -1 /* not initialized */ },

{ MRT_ASSERT, 	IPPROTO_IP, 0, OA_RW, (OP_PASSNEXT|OP_NODEFAULT),
	sizeof (int), -1 /* not initialized */ },
};

/*
 * Table of all supported levels
 * Note: Some levels (e.g. XTI_GENERIC) may be valid but may not have
 * any supported options so we need this info separately.
 *
 * This is needed only for topmost tpi providers.
 */
optlevel_t	icmp_valid_levels_arr[] = {
	XTI_GENERIC,
	SOL_SOCKET,
	IPPROTO_ICMP,
	IPPROTO_IP
};

#define	ICMP_VALID_LEVELS_CNT	A_CNT(icmp_valid_levels_arr)
#define	ICMP_OPT_ARR_CNT		A_CNT(icmp_opt_arr)

t_uscalar_t icmp_max_optbuf_len; /* initialized when ICMP driver is loaded */

/*
 * Initialize option database object for ICMP
 *
 * This object represents database of options to search passed to
 * {sock,tpi}optcom_req() interface routine to take care of option
 * management and associated methods.
 */

optdb_obj_t icmp_opt_obj = {
	icmp_opt_default,	/* ICMP default value function pointer */
	icmp_opt_get,		/* ICMP get function pointer */
	icmp_opt_set,		/* ICMP set function pointer */
	true,			/* ICMP is tpi provider */
	ICMP_OPT_ARR_CNT,	/* ICMP option database count of entries */
	icmp_opt_arr,		/* ICMP option database */
	ICMP_VALID_LEVELS_CNT,	/* ICMP valid level count of entries */
	icmp_valid_levels_arr	/* ICMP valid level array */
};
