/*
 * Copyright (c) 1992,1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)snmpcom.c	1.14	97/12/06 SMI"

/*
 * This file contains common code for handling Options Management requests
 * for SNMP/MIB.
 */

#include <sys/types.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/errno.h>
#define	_SUN_TPI_VERSION 2
#include <sys/tihdr.h>
#include <sys/ddi.h>
#include <sys/cmn_err.h>

#include <sys/socket.h>
#include <netinet/in.h>

#include <inet/common.h>
#include <inet/mi.h>
#include <inet/mib2.h>
#include <inet/optcom.h>
#include <inet/snmpcom.h>

#define	DEFAULT_LENGTH	sizeof (long)
#define	DATA_MBLK_SIZE	1024
#define	TOAHDR_SIZE	(sizeof (struct T_optmgmt_ack) +\
	sizeof (struct opthdr))

/* SNMP Option Request Structure */
typedef struct sor_s {
	int	sor_group;
	int	sor_code;		/* MIB2 index value */
	int	sor_size;
} sor_t;

/*
 * Validation Table for set requests.
 */
static	sor_t	req_arr[] = {
	{ MIB2_IP,	1,	sizeof (int)			},
	{ MIB2_IP,	2,	sizeof (int)			},
	{ MIB2_IP,	21,	sizeof (mib2_ipRouteEntry_t)	},
	{ MIB2_IP,	22,	sizeof (mib2_ipNetToMediaEntry_t)},
	{ MIB2_TCP,	13,	sizeof (mib2_tcpConnEntry_t)	}
};

boolean_t snmpcom_req(queue_t *q, mblk_t *mp, pfi_t setfn, pfi_t getfn,
    int priv);
int	snmp_append_data(mblk_t *mpdata, char *blob, int len);

/*
 * Binary compatibility to what used to be T_CURRENT in older releases.
 * Unfortunately, the binary chosen for it was different and used by
 * T_PARTSUCCESS in the new name space. However T_PARTSUCESS is only
 * anticiapted in new T_OPTMGM_REQ (and not O_T_OPTMGMT_REQ messages).
 * Only a test for TBADFLAG which uses one of the MIB option levels
 * may have trouble with this provision for binary compatibility.
 */
#define	OLD_T_CURRENT	0x100	/* same value as T_PARTSUCCESS */

/*
 * MIB info returned in data part of M_PROTO msg.  All info for a single
 * request is appended in a chain of mblk's off of the M_PROTO T_OPTMGMT_ACK
 * ctl buffer.
 */
int
snmp_append_data(mpdata, blob, len)
	mblk_t	*mpdata;
	char	*blob;
	int	len;
{

	if (!mpdata)
		return (0);
	while (mpdata->b_cont)
		mpdata = mpdata->b_cont;
	if (mpdata->b_wptr + len >= mpdata->b_datap->db_lim) {
		mpdata->b_cont = allocb(DATA_MBLK_SIZE, BPRI_HI);
		mpdata = mpdata->b_cont;
		if (!mpdata)
			return (0);
	}
	bcopy(blob, (char *)mpdata->b_wptr, len);
	mpdata->b_wptr += len;
	return (1);
}

/*
 * Need a form which avoids O(n^2) behavior locating the end of the
 * chain every time.  This is it.
 */
int
snmp_append_data2(mblk_t *mpdata, mblk_t **last_mpp, char *blob, int len)
{

	if (!mpdata)
		return (0);
	if (*last_mpp == NULL) {
		while (mpdata->b_cont)
			mpdata = mpdata->b_cont;
		*last_mpp = mpdata;
	}
	if ((*last_mpp)->b_wptr + len >= (*last_mpp)->b_datap->db_lim) {
		(*last_mpp)->b_cont = allocb(DATA_MBLK_SIZE, BPRI_HI);
		*last_mpp = (*last_mpp)->b_cont;
		if (!*last_mpp)
			return (0);
	}
	bcopy(blob, (char *)(*last_mpp)->b_wptr, len);
	(*last_mpp)->b_wptr += len;
	return (1);
}

/*
 * SNMP requests are issued using putmsg() on a stream containing all
 * relevant modules.  The ctl part contains a O_T_OPTMGMT_REQ message,
 * and the data part is nilp.  Each module in the chain calls snmpcom_req()
 * to process this msg. If snmpcom_req() returns FALSE, then the module
 * will try optcom_req to see if its some sort of SOCKET or IP option.
 * snmpcom_req returns TRUE whenever the first option is recognized as
 * an SNMP request, even if a bad one.
 *
 * "get" is done by a single O_T_OPTMGMT_REQ with MGMT_flags set to T_CURRENT.
 * All modules respond with one or msg's about what they know.  Responses
 * are in T_OPTMGMT_ACK format.  The opthdr level/name fields identify what
 * is begin returned, the len field how big it is (in bytes).  The info
 * itself is in the data portion of the msg.  Fixed length info returned
 * in one msg; each table in a separate msg.
 *
 * setfn() returns 1 if things ok, 0 if set request invalid or otherwise
 * messed up.
 *
 * If the passed q is at the bottom of the module chain (q_next == nilp),
 * a ctl msg with req->name, level, len all zero is sent upstream.  This
 * is and EOD flag to the caller.
 *
 * IMPORTANT:
 * - The msg type is M_PROTO, not M_PCPROTO!!!  This is by design,
 *   since multiple messages will be sent to stream head and we want
 *   them queued for reading, not discarded.
 * - All requests which match a table entry are sent to all get/set functions
 *   of each module.  The functions must simply ignore requests not meant
 *   for them: getfn() returns 0, setfn() returns 1.
 */
boolean_t
snmpcom_req(q, mp, setfn, getfn, priv)
	queue_t	*q;
	mblk_t	*mp;
	pfi_t	setfn;
	pfi_t	getfn;
	int	priv;
{
	mblk_t			*mpctl;
	struct opthdr		*req;
	struct opthdr		*next_req;
	struct opthdr		*req_end;
	struct opthdr		*req_start;
	sor_t			*sreq;
	struct T_optmgmt_req	*tor =
		(struct T_optmgmt_req *)ALIGN32(mp->b_rptr);
	struct T_optmgmt_ack	*toa;

	if (mp->b_cont) {	/* don't deal with multiple mblk's */
		freemsg(mp->b_cont);
		mp->b_cont = (mblk_t *)0;
		optcom_err_ack(q, mp, TSYSERR, EBADMSG);
		return (true);
	}
	if ((mp->b_wptr - mp->b_rptr) < sizeof (struct T_optmgmt_req) ||
	    !(req_start = (struct opthdr *)ALIGN32(mi_offset_param(mp,
		tor->OPT_offset, tor->OPT_length))))
		goto bad_req1;
	/*
	 * if first option not in the MIB2 or EXPER range, return false so
	 * optcom_req can scope things out.  Otherwise it's passed to each
	 * calling module to process or ignore as it sees fit.
	 */
	if ((!(req_start->level >= MIB2_RANGE_START &&
			req_start->level <= MIB2_RANGE_END)) &&
	    (!(req_start->level >= EXPER_RANGE_START &&
			req_start->level <= EXPER_RANGE_END)))
		return (false);

	switch (tor->MGMT_flags) {

	case T_NEGOTIATE:
		if (!priv) {
			optcom_err_ack(q, mp, TACCES, 0);
			return (true);
		}
		req_end = (struct opthdr *)ALIGN32(((u_char *)req_start +
			tor->OPT_length));
		for (req = req_start; req < req_end; req = next_req) {
			next_req =
				(struct opthdr *)ALIGN32(((u_char *)&req[1] +
				ROUNDUP_TPIOPT(req->len)));
			if (next_req > req_end)
				goto bad_req2;
			for (sreq = req_arr; sreq < A_END(req_arr); sreq++) {
				if (req->level == sreq->sor_group &&
				    req->name == sreq->sor_code)
					break;
			}
			if (sreq >= A_END(req_arr))
				goto bad_req3;
			if (!(*setfn)(q, req->level, req->name,
				(u_char *)&req[1], req->len))
				goto bad_req4;
		}
		if (q->q_next)
			putnext(q, mp);
		else
			freemsg(mp);
		return (true);

	case OLD_T_CURRENT:
	case T_CURRENT:
		mpctl = allocb(TOAHDR_SIZE, BPRI_MED);
		if (!mpctl) {
			optcom_err_ack(q, mp, TSYSERR, ENOMEM);
			return (true);
		}
		mpctl->b_cont = allocb(DATA_MBLK_SIZE, BPRI_MED);
		if (!mpctl->b_cont) {
			freemsg(mpctl);
			optcom_err_ack(q, mp, TSYSERR, ENOMEM);
			return (true);
		}
		mpctl->b_datap->db_type = M_PROTO;
		mpctl->b_wptr += TOAHDR_SIZE;
		toa = (struct T_optmgmt_ack *)ALIGN32(mpctl->b_rptr);
		toa->PRIM_type = T_OPTMGMT_ACK;
		toa->OPT_offset = sizeof (struct T_optmgmt_ack);
		toa->OPT_length = sizeof (struct opthdr);
		toa->MGMT_flags = T_SUCCESS;
		if (!(*getfn)(q, mpctl))
			freemsg(mpctl);
		/*
		 * all data for this module has now been sent upstream.  If
		 * this is bottom module of stream, send up an EOD ctl msg,
		 * otherwise pass onto the next guy for processing.
		 */
		if (q->q_next) {
			putnext(q, mp);
			return (true);
		}
		if (mp->b_cont) {
			freemsg(mp->b_cont);
			mp->b_cont = nilp(mblk_t);
		}
		mpctl = reallocb(mp, TOAHDR_SIZE, 1);
		if (!mpctl) {
			optcom_err_ack(q, mp, TSYSERR, ENOMEM);
			return (true);
		}
		mpctl->b_datap->db_type = M_PROTO;
		mpctl->b_wptr = mpctl->b_rptr + TOAHDR_SIZE;
		toa = (struct T_optmgmt_ack *)ALIGN32(mpctl->b_rptr);
		toa->PRIM_type = T_OPTMGMT_ACK;
		toa->OPT_offset = sizeof (struct T_optmgmt_ack);
		toa->OPT_length = sizeof (struct opthdr);
		toa->MGMT_flags = T_SUCCESS;
		req = (struct opthdr *)&toa[1];
		req->level = 0;
		req->name = 0;
		req->len = 0;
		qreply(q, mpctl);
		return (true);

	default:
		optcom_err_ack(q, mp, TBADFLAG, 0);
		return (true);
	}

bad_req1:;
	printf("snmpcom bad_req1\n");
	goto bad_req;
bad_req2:;
	printf("snmpcom bad_req2\n");
	goto bad_req;
bad_req3:;
	printf("snmpcom bad_req3\n");
	goto bad_req;
bad_req4:;
	printf("snmpcom bad_req4\n");
	/* FALLTHRU */
bad_req:;
	optcom_err_ack(q, mp, TBADOPT, 0);
	return (true);

}
