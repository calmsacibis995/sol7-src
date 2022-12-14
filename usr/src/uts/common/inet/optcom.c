/*
 * Copyright (c) 1992-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)optcom.c	1.31	97/12/06 SMI"

/*
 * This file contains common code for handling Options Management requests.
 */

#include <sys/types.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/strlog.h>
#include <sys/errno.h>
#define	_SUN_TPI_VERSION 2
#include <sys/tihdr.h>
#include <sys/timod.h>
#include <sys/socket.h>
#include <sys/ddi.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>		/* for ASSERT */

#include <inet/common.h>
#include <inet/mi.h>
#include <inet/nd.h>
#include <inet/ip.h>
#include <inet/mib2.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/ip_mroute.h>
#include "optcom.h"

#include <inet/optcom.h>

/*
 * Function prototypes
 */
static t_scalar_t process_topthdrs_first_pass(mblk_t *, int, optdb_obj_t *,
    boolean_t *, size_t *);
static t_scalar_t do_options_second_pass(queue_t *, mblk_t *, mblk_t *,
    int, optdb_obj_t *, t_uscalar_t *);
static t_uscalar_t get_worst_status(t_uscalar_t, t_uscalar_t);
static int do_opt_default(queue_t *, struct T_opthdr *, u_char **,
    t_uscalar_t *, int, optdb_obj_t *);
static void do_opt_current(queue_t *, struct T_opthdr *, u_char **,
    t_uscalar_t *, int, optdb_obj_t *);
static void do_opt_check_or_negotiate(queue_t *, struct T_opthdr *,
    t_scalar_t, u_char **, t_uscalar_t *, int, optdb_obj_t *);
static opdes_t *opt_chk_lookup(t_uscalar_t, t_uscalar_t, opdes_t *, u_int);
static boolean_t opt_level_valid(t_uscalar_t, optlevel_t *, u_int);
static size_t opt_level_allopts_lengths(t_uscalar_t, opdes_t *, u_int);
static boolean_t opt_length_ok(opdes_t *, struct T_opthdr *);


/* Common code for sending back a T_ERROR_ACK. */
void
optcom_err_ack(queue_t *q, mblk_t *mp, t_scalar_t t_error, int sys_error)
{
	if ((mp = mi_tpi_err_ack_alloc(mp, t_error, sys_error)) != NULL)
		qreply(q, mp);
}

/*
 * The option management routines svr4_optcom_req() and tpi_optcom_req() use
 * callback functions as arguments. Here is the expected interfaces
 * assumed from the callback functions
 *
 *
 * (1) deffn(q, optlevel, optname, optvalp)
 *
 *	- Function only called when default value comes from protocol
 *	 specific code and not the option database table (indicated by
 *	  OP_DEF_FN property in option database.)
 *	- Error return is -1. Valid returns are >=0.
 *	- When valid, the return value represents the length used for storing
 *		the default value of the option.
 *      - Error return implies the called routine did not recognize this
 *              option. Something downstream could so input is left unchanged
 *              in request buffer.
 *
 * (2) getfn(q, optlevel, optname, optvalp)
 *
 *	- Error return is -1. Valid returns are >=0.
 *	- When valid, the return value represents the length used for storing
 *		the actual value of the option.
 *      - Error return implies the called routine did not recognize this
 *              option. Something downstream could so input is left unchanged
 *              in request buffer.
 *
 * (3) setfn(q, mgmt_flags, optlevel, optname, inlen, invalp,
 *	outlenp, outvalp);
 *
 *	- OK return is 0, Error code is returned as a non-zero argument.
 *      - If negative it is ignored by svr4_optcom_req(). If positive, error
 *        is returned. A negative return implies that option, while handled on
 *	  this stack is not handled at this level and will be handled further
 *	  downstream.
 *	- Both negative and positive errors are treats as errors in an
 *	  identical manner by tpi_optcom_req(). The errors affect "status"
 *	  field of each option's T_opthdr. If sucessfull, an appropriate sucess
 *	  result is carried. If error, it instantiated to "failure" at the
 *	  topmost level and left unchanged at other levels. (This "failure" can
 *	  turn to a success at another level).
 *	- mgmt_flags passed for tpi_optcom_req(). It is interpreted as:
 *        -  T_CHECK: semantics are to pretend to set the value and report
 *           back if it would be successful.
 *        - T_NEGOTIATE: set the value. Call from option management primitive
 *                        T_OPTMGMT_REQ.
 *        - (T_CHECK|T_NEGOTIATE): set but request came riding on one of
 *          the other option management primitives such as T_CONN_* and
 *           T_UNITDATA_*
 *	- inlen, invalp is the option length,value requested to be set.
 *	- outlenp, outvalp represent return parameters which contain the
 *	  value set and it might be different from one passed on input.
 *	- the caller might pass same buffers for input and output and the
 *	  routine should protect against this case by not updating output
 *	  buffers until it is done referencing input buffers and any other
 *	  issues (e.g. not use bcopy() if we do not trust what it does).
 *      - If option is not known, it returns error. We randomly pick EINVAL.
 *        It can however get called with options that are handled downstream
 *        opr upstream so for svr4_optcom_req(), it does not return error for
 *        negative return values.
 *
 */

/*
 * Upper Level Protocols call this routine when they receive
 * a O_T_OPTMGMT_REQ message.  They supply callback functions
 * for setting a new value for a single options, getting the
 * current value for a single option, and checking for support
 * of a single option.  svr4_optcom_req validates the option management
 * buffer passed in, and calls the appropriate routines to do the
 * job requested.
 */
void
svr4_optcom_req(queue_t *q, mblk_t *mp, int priv, optdb_obj_t *dbobjp)
{
	pfi_t	deffn = dbobjp->odb_deffn;
	pfi_t	getfn = dbobjp->odb_getfn;
	pfi_t	setfn = dbobjp->odb_setfn;
	opdes_t	*opt_arr = dbobjp->odb_opt_des_arr;
	u_int opt_arr_cnt = dbobjp->odb_opt_arr_cnt;
	boolean_t topmost_tpiprovider = dbobjp->odb_topmost_tpiprovider;

	t_uscalar_t max_optbuf_len;
	int len;
	mblk_t	*mp1 = nilp(mblk_t);
	struct opthdr *next_opt;
	struct opthdr *opt;
	struct opthdr *opt1;
	struct opthdr *opt_end;
	struct opthdr *opt_start;
	opdes_t	*optd;
	boolean_t	pass_to_next = false;
	struct T_optmgmt_ack *toa;
	struct T_optmgmt_req *tor =
		(struct T_optmgmt_req *)mp->b_rptr;

	/* Verify message integrity. */
	if (mp->b_wptr - mp->b_rptr < sizeof (struct T_optmgmt_req)) {
bad_opt:;
		optcom_err_ack(q, mp, TBADOPT, 0);
		return;
	}
	/* Verify MGMT_flags legal */
	switch (tor->MGMT_flags) {
	case T_DEFAULT:
	case T_NEGOTIATE:
	case T_CURRENT:
	case T_CHECK:
		/* OK - legal request flags */
		break;
	default:
bad_flag:;
		optcom_err_ack(q, mp, TBADFLAG, 0);
		return;
	}
	if (tor->MGMT_flags == T_DEFAULT) {
		/* Is it a request for default option settings? */

		/*
		 * Note: XXX TLI and TPI specification was unclear about
		 * semantics of T_DEFAULT and the following historical note
		 * and its interpretation is incorrect (it implies a request
		 * for default values of only the identified options not all.
		 * The semantics have been explained better in XTI spec.)
		 * However, we do not modify (comment or code) here to keep
		 * compatibility.
		 * We can rethink this if it ever becomes an issue.
		 * ----historical comment start------
		 * As we understand it, the input buffer is meaningless
		 * so we ditch the message.  A T_DEFAULT request is a
		 * request to obtain a buffer containing defaults for
		 * all supported options, so we allocate a maximum length
		 * reply.
		 * ----historical comment end -------
		 */
		/* T_DEFAULT not passed down */
		ASSERT(topmost_tpiprovider == true);
		freemsg(mp);
		max_optbuf_len = optcom_max_optbuf_len(opt_arr,
		    opt_arr_cnt);
		mp = allocb(max_optbuf_len, BPRI_MED);
		if (!mp) {
no_mem:;
			optcom_err_ack(q, mp, TSYSERR, ENOMEM);
			return;
		}

		/* Initialize the T_optmgmt_ack header. */
		toa = (struct T_optmgmt_ack *)mp->b_rptr;
		bzero((char *)toa, max_optbuf_len);
		toa->PRIM_type = T_OPTMGMT_ACK;
		toa->OPT_offset = (t_scalar_t)sizeof (struct T_optmgmt_ack);
		/* TODO: Is T_DEFAULT the right thing to put in MGMT_flags? */
		toa->MGMT_flags = T_DEFAULT;

		/* Now walk the table of options passed in */
		opt = (struct opthdr *)&toa[1];
		for (optd = opt_arr; optd < &opt_arr[opt_arr_cnt]; optd++) {
			/*
			 * All the options in the table of options passed
			 * in are by definition supported by the protocol
			 * calling this function.
			 */
			if (! OA_READ_PERMISSION(optd, priv))
				continue;
			opt->level = optd->opdes_level;
			opt->name = optd->opdes_name;
			if (!(optd->opdes_props & OP_DEF_FN) ||
			    ((len = (*deffn)(q, opt->level,
				opt->name, (u_char *)&opt[1])) < 0)) {
				/*
				 * Fill length and value from table.
				 *
				 * Default value not instantiated from function
				 * (or the protocol specific function failed it;
				 * In this interpretation of T_DEFAULT, this is
				 * the best we can do)
				 */
				switch (optd->opdes_size) {
				/*
				 * Since options are guaranteed aligned only
				 * on a 4 byte boundary (t_scalar_t) any
				 * option that is greater in size will default
				 * to the bcopy below
				 */
				case sizeof (int32_t):
					*(int32_t *)&opt[1] =
					    (int32_t)optd->opdes_default;
					break;
				case sizeof (int16_t):
					*(int16_t *)&opt[1] =
					    (int16_t)optd->opdes_default;
					break;
				case sizeof (int8_t):
					*(int8_t *)&opt[1] =
					    (int8_t)optd->opdes_default;
					break;
				default:
					/*
					 * other length but still assume
					 * fixed - use bcopy
					 */
					bcopy(optd->opdes_defbuf,
					    &opt[1], optd->opdes_size);
					break;
				}
				opt->len = optd->opdes_size;
			}
			else
				opt->len = (t_uscalar_t)len;
			opt = (struct opthdr *)((char *)&opt[1] +
			    ROUNDUP_TPIOPT(opt->len));
		}

		/* Now record the final length. */
		toa->OPT_length = (t_scalar_t)((char *)opt - (char *)&toa[1]);
		mp->b_wptr = (u_char *)opt;
		mp->b_datap->db_type = M_PCPROTO;
		/* Ship it back. */
		qreply(q, mp);
		return;
	}
	/* T_DEFAULT processing complete - no more T_DEFAULT */

	/*
	 * For T_NEGOTIATE, T_CURRENT, and T_CHECK requests, we make a
	 * pass through the input buffer validating the details and
	 * making sure each option is supported by the protocol.
	 */
	if ((opt_start = (struct opthdr *)mi_offset_param(mp,
	    tor->OPT_offset, tor->OPT_length)) == NULL)
		goto bad_opt;
	if (! ISALIGNED_TPIOPT(opt_start))
		goto bad_opt;

	opt_end = (struct opthdr *)((u_char *)opt_start +
	    tor->OPT_length);

	for (opt = opt_start; opt < opt_end; opt = next_opt) {
		/*
		 * Verify we have room to reference the option header
		 * fields in the option buffer.
		 */
		if ((u_char *)opt + sizeof (struct opthdr) > (u_char *)opt_end)
			goto bad_opt;
		/*
		 * We now compute pointer to next option in buffer 'next_opt'
		 * The next_opt computation above below 'opt->len' initialized
		 * by application which cannot be trusted. The usual value
		 * too large will be captured by the loop termination condition
		 * above. We check for the following which it will miss.
		 * 	-pointer space wraparound arithmetic overflow
		 *	-last option in buffer with 'opt->len' being too large
		 *	 (only reason 'next_opt' should equal or exceed
		 *	 'opt_end' for last option is roundup unless length is
		 *	 too-large/invalid)
		 */
		next_opt = (struct opthdr *)((u_char *)&opt[1] +
		    ROUNDUP_TPIOPT(opt->len));

		if ((u_char *)next_opt < (u_char *)&opt[1] ||
		    ((next_opt >= opt_end) &&
			(((u_char *)next_opt - (u_char *)opt_end) >=
			    ALIGN_TPIOPT_size)))
			goto bad_opt;

		/* sanity check */
		if (opt->name == T_ALLOPT)
			goto bad_opt;

		/* Find the option in the opt_arr. */
		if ((optd = opt_chk_lookup(opt->level, opt->name,
		    opt_arr, opt_arr_cnt)) == NULL) {
			/*
			 * Not found, that is a bad thing if
			 * the caller is a tpi provider
			 */
			if (topmost_tpiprovider)
				goto bad_opt;
			else
				continue; /* skip unmodified */
		}

		/* Additional checks dependent on operation. */
		switch (tor->MGMT_flags) {
		case T_NEGOTIATE:
			if (! OA_WRITE_OR_EXECUTE(optd, priv)) {
				/* can't negotiate option */
				if ((! priv) &&
				    OA_WX_ANYPRIV(optd)) {
					/*
					 * not privileged but privilege
					 * will help negotiate option.
					 */
					optcom_err_ack(q, mp, TACCES, 0);
					return;
				} else
					goto bad_opt;
			}
			/*
			 * Verify size for options
			 * Note: For retaining compatibility with historical
			 * behavior, variable lengths options will have their
			 * length verified in the setfn() processing.
			 * In order to be compatible with SunOS 4.X we return
			 * EINVAL errors for bad lengths.
			 */
			if (!(optd->opdes_props & OP_VARLEN)) {
				/* fixed length - size must match */
				if (opt->len != optd->opdes_size) {
					optcom_err_ack(q, mp, TSYSERR, EINVAL);
					return;
				}
			}
			break;

		case T_CHECK:
			if (! OA_RWX_ANYPRIV(optd))
				/* any of "rwx" permission but not not none */
				goto bad_opt;
			/*
			 * XXX Since T_CURRENT was not there in TLI and the
			 * official TLI inspired TPI standard, getsockopt()
			 * API uses T_CHECK (for T_CURRENT semantics)
			 * The following fallthru makes sense because of its
			 * historical use as semantic equivalent to T_CURRENT.
			 */
			/* FALLTHRU */
		case T_CURRENT:
			if (! OA_READ_PERMISSION(optd, priv)) {
				/* can't read option value */
				if ((! priv) &&
				    OA_R_ANYPRIV(optd)) {
					/*
					 * not privileged but privilege
					 * will help in reading option value.
					 */
					optcom_err_ack(q, mp, TACCES, 0);
					return;
				} else
					goto bad_opt;
			}
			break;

		default:
			goto bad_flag;
		}
		/* We liked it.  Keep going. */
	} /* end for loop scanning option buffer */

	/* Now complete the operation as required. */
	switch (tor->MGMT_flags) {
	case T_CHECK:
		/*
		 * Historically used same as T_CURRENT (which was added to
		 * standard later). Code retained for compatibility.
		 */
		/* FALLTHROUGH */
	case T_CURRENT:
		/*
		 * Allocate a maximum size reply.  Perhaps we are supposed to
		 * assume that the input buffer includes space for the answers
		 * as well as the opthdrs, but we don't know that for sure.
		 * So, instead, we create a new output buffer, using the
		 * input buffer only as a list of options.
		 */
		max_optbuf_len = optcom_max_optbuf_len(opt_arr,
		    opt_arr_cnt);
		mp1 = allocb(max_optbuf_len, BPRI_MED);
		if (!mp1)
			goto no_mem;
		/* Initialize the header. */
		mp1->b_datap->db_type = M_PCPROTO;
		mp1->b_wptr = &mp1->b_rptr[sizeof (struct T_optmgmt_ack)];
		toa = (struct T_optmgmt_ack *)mp1->b_rptr;
		toa->OPT_offset = (t_scalar_t)sizeof (struct T_optmgmt_ack);
		toa->MGMT_flags = tor->MGMT_flags;
		/*
		 * Walk through the input buffer again, this time adding
		 * entries to the output buffer for each option requested.
		 * Note, sanity of option header, last option etc, verified
		 * in first pass.
		 */
		opt1 = (struct opthdr *)&toa[1];

		for (opt = opt_start; opt < opt_end; opt = next_opt) {

		    next_opt = (struct opthdr *)((u_char *)&opt[1] +
			ROUNDUP_TPIOPT(opt->len));

			opt1->name = opt->name;
			opt1->level = opt->level;
			len = (*getfn)(q, opt->level,
			    opt->name, (u_char *)&opt1[1]);
			/*
			 * Failure means option is not recognized. Copy input
			 * buffer as is
			 */
			if (len < 0) {
				opt1->len = opt->len;
				bcopy(&opt[1], &opt1[1], opt->len);
			}
			else
				opt1->len = (t_uscalar_t)len;
			opt1 = (struct opthdr *)((u_char *)&opt1[1] +
			    ROUNDUP_TPIOPT(opt1->len));
		} /* end for loop */

		/* Record the final length. */
		toa->OPT_length = (t_scalar_t)((u_char *)opt1 -
		    (u_char *)&toa[1]);
		mp1->b_wptr = (u_char *)opt1;
		/* Ditch the input buffer. */
		freemsg(mp);
		mp = mp1;
		/* Always let the next module look at the option. */
		pass_to_next = true;
		break;

	case T_NEGOTIATE:
		/*
		 * Here we are expecting that the response buffer is exactly
		 * the same size as the input buffer.  We pass each opthdr
		 * to the protocol's set function.  If the protocol doesn't
		 * like it, it can update the value in it return argument.
		 */
		/*
		 * Pass each negotiated option through the protocol set
		 * function.
		 * Note: sanity check on option header values done in first
		 * pass and not repeated here.
		 */
		toa = (struct T_optmgmt_ack *)tor;

		for (opt = opt_start; opt < opt_end; opt = next_opt) {
			int error;

			next_opt = (struct opthdr *)((u_char *)&opt[1] +
			    ROUNDUP_TPIOPT(opt->len));

			error = (*setfn)(q, T_NEGOTIATE, opt->level, opt->name,
			    opt->len,  (u_char *)&opt[1],
			    &opt->len, (u_char *)&opt[1]);
			/*
			 * Treat positive "errors" as real.
			 * Note: negative errors are to be treated as
			 * non-fatal by svr4_optcom_req() and are
			 * returned by setfn() when it is passed an
			 * option it does not handle. Since the option
			 * passed opt_chk_lookup(), it is implied that
			 * it is valid but was either handled upstream
			 * or will be handled downstream.
			 */
			if (error > 0) {
				optcom_err_ack(q, mp, TSYSERR, error);
				return;
			}
		}
		pass_to_next = true;
		break;
	default:
		goto bad_flag;
	}

	if (pass_to_next && (q->q_next != NULL)) {
		/* Send it down to the next module and let it reply */
		toa->PRIM_type = O_T_OPTMGMT_REQ; /* Changed by IP to ACK */
		putnext(q, mp);
	} else {
		/* Set common fields in the header. */
		toa->MGMT_flags = T_SUCCESS;
		mp->b_datap->db_type = M_PCPROTO;
		toa->PRIM_type = T_OPTMGMT_ACK;
		qreply(q, mp);
	}
}

/*
 * New optcom_req inspired by TPI/XTI semantics
 */
void
tpi_optcom_req(queue_t *q, mblk_t *mp, int priv, optdb_obj_t *dbobjp)
{
	t_scalar_t t_error;
	mblk_t *toa_mp;
	boolean_t pass_to_next;
	size_t toa_len;
	t_uscalar_t worst_status;
	struct T_optmgmt_ack *toa;
	struct T_optmgmt_req *tor =
	    (struct T_optmgmt_req *)mp->b_rptr;

	/* Verify message integrity. */
	if ((mp->b_wptr - mp->b_rptr) < sizeof (struct T_optmgmt_req)) {
bad_opt:;
		optcom_err_ack(q, mp, TBADOPT, 0);
		return;
	}

	/* Verify MGMT_flags legal */
	switch (tor->MGMT_flags) {
	case T_DEFAULT:
	case T_NEGOTIATE:
	case T_CURRENT:
	case T_CHECK:
		/* OK - legal request flags */
		break;
	default:
bad_flag:;
		optcom_err_ack(q, mp, TBADFLAG, 0);
		return;
	}

	/*
	 * In this design, there are two passes required on the input buffer
	 * mostly to accomodate variable length options and "T_ALLOPT" option
	 * which has the semantics "all options of the specified level".
	 *
	 * For T_DEFAULT, T_NEGOTIATE, T_CURRENT, and T_CHECK requests, we make
	 * a pass through the input buffer validating the details and making
	 * sure each option is supported by the protocol. We also determine the
	 * length of the option buffer to return. (Variable length options and
	 * T_ALLOPT mean that length can be different for output buffer).
	 */

	pass_to_next = false;	/* initial value */
	toa_len = 0;		/* initial value */

	/*
	 * First pass, we do the following
	 *	- estimate cumulative length needed for results
	 *	- set "status" field based on permissions, option header check
	 *	  etc.
	 *	- determine "pass_to_next" whether we need to send request to
	 *	  downstream module/driver.
	 */
	if ((t_error = process_topthdrs_first_pass(mp, priv, dbobjp,
	    &pass_to_next, &toa_len)) != 0) {
		optcom_err_ack(q, mp, t_error, 0);
		return;
	}

	/*
	 * A validation phase of the input buffer is done. We have also
	 * obtained the length requirement and and other details about the
	 * input and we liked input buffer so far.  We make another scan
	 * through the input now and generate the output necessary to complete
	 * the operation.
	 */

	toa_mp = allocb(toa_len, BPRI_MED);
	if (! toa_mp) {
		optcom_err_ack(q, mp, TSYSERR, ENOMEM);
		return;
	}

	/*
	 * Set initial values for generating output.
	 */
	worst_status = T_SUCCESS; /* initial value */

	/*
	 * This routine makes another pass through the option buffer this
	 * time acting on the request based on "status" result in the
	 * first pass. It also performs "expansion" of T_ALLOPT into
	 * all options of a certain level and acts on each for this request.
	 */
	if ((t_error = do_options_second_pass(q, mp, toa_mp, priv, dbobjp,
	    &worst_status)) != 0) {
		freemsg(toa_mp);
		optcom_err_ack(q, mp, t_error, 0);
		return;
	}

	/*
	 * Following code relies on the coincidence that T_optmgmt_req
	 * and T_optmgmt_ack are identical in binary representation
	 */
	toa = (struct T_optmgmt_ack *)toa_mp->b_rptr;
	toa->OPT_length = (t_scalar_t)(toa_mp->b_wptr - (toa_mp->b_rptr +
	    sizeof (struct T_optmgmt_ack)));
	toa->OPT_offset = (t_scalar_t)sizeof (struct T_optmgmt_ack);

	toa->MGMT_flags = tor->MGMT_flags;

	freemsg(mp);		/* free input mblk */

	/*
	 * If there is atleast one option that requires a downstream
	 * forwarding and if it is possible, we forward the message
	 * downstream. Else we ack it.
	 */
	if (pass_to_next && (q->q_next != NULL)) {
		/*
		 * We pass it down as T_OPTMGMT_REQ. This code relies
		 * on the happy coincidence that T_optmgmt_req and
		 * T_optmgmt_ack are identical data structures
		 * at the binary representation level.
		 */
		toa_mp->b_datap->db_type = M_PROTO;
		toa->PRIM_type = T_OPTMGMT_REQ;
		putnext(q, toa_mp);
	} else {
		toa->PRIM_type = T_OPTMGMT_ACK;
		toa_mp->b_datap->db_type = M_PCPROTO;
		toa->MGMT_flags |= worst_status; /* XXX "worst" or "OR" TPI ? */
		qreply(q, toa_mp);
	}
}


/*
 * Following routine makes a pass through option buffer in mp and performs the
 * following tasks.
 *	- estimate cumulative length needed for results
 *	- set "status" field based on permissions, option header check
 *	  etc.
 *	- determine "pass_to_next" whether we need to send request to
 *	  downstream module/driver.
 */

static t_scalar_t
process_topthdrs_first_pass(mblk_t *mp, int priv, optdb_obj_t *dbobjp,
    boolean_t *pass_to_nextp, size_t *toa_lenp)
{
	opdes_t	*opt_arr = dbobjp->odb_opt_des_arr;
	u_int opt_arr_cnt = dbobjp->odb_opt_arr_cnt;
	boolean_t topmost_tpiprovider = dbobjp->odb_topmost_tpiprovider;
	optlevel_t *valid_level_arr = dbobjp->odb_valid_levels_arr;
	u_int valid_level_arr_cnt = dbobjp->odb_valid_levels_arr_cnt;
	struct T_opthdr *opt, *next_opt;
	struct T_opthdr *opt_start, *opt_end;
	opdes_t	*optd;
	size_t allopt_len;
	struct T_optmgmt_req *tor =
	    (struct T_optmgmt_req *)mp->b_rptr;

	*toa_lenp = sizeof (struct T_optmgmt_ack); /* initial value */

	if ((opt_start = (struct T_opthdr *)
	    mi_offset_param(mp, tor->OPT_offset, tor->OPT_length)) == NULL) {
bad_opt:;
		return (TBADOPT);
	}
	if (! ISALIGNED_TPIOPT(opt_start))
		goto bad_opt;

	opt_end = (struct T_opthdr *)((u_char *)opt_start + tor->OPT_length);

	for (opt = opt_start; opt < opt_end; opt = next_opt) {
		/*
		 * Verify we have room to reference the option header
		 * fields in the option buffer.
		 */
		if ((u_char *)opt + sizeof (struct T_opthdr) >
		    (u_char *)opt_end)
			goto bad_opt;
		/*
		 * We now compute pointer to next option in buffer 'next_opt'
		 * The next_opt computation above below 'opt->len' initialized
		 * by application which cannot be trusted. The usual value
		 * too large will be captured by the loop termination condition
		 * above. We check for the following which it will miss.
		 * 	-pointer space wraparound arithmetic overflow
		 *	-last option in buffer with 'opt->len' being too large
		 *	 (only reason 'next_opt' should equal or exceed
		 *	 'opt_end' for last option is roundup unless length is
		 *	 too-large/invalid)
		 */
		next_opt = (struct T_opthdr *)
		    ((u_char *)opt + ROUNDUP_TPIOPT(opt->len));

		if ((u_char *)next_opt < (u_char *)opt ||
		    ((next_opt >= opt_end) &&
			(((u_char *)next_opt - (u_char *)opt_end) >=
			    ALIGN_TPIOPT_size)))
			goto bad_opt;

		/* Find the option in the opt_arr. */
		if (opt->name != T_ALLOPT) {
			optd = opt_chk_lookup(opt->level, opt->name,
			    opt_arr, opt_arr_cnt);
			if (optd == NULL) {
				/*
				 * Option not found
				 *
				 * Verify if level is "valid" or not.
				 * Note: This check is required by XTI
				 *
				 * TPI provider always initializes
				 * the "not supported" (or whatever) status
				 * for the options. Other levels leave status
				 * unchanged if they do not understand an
				 * option.
				 */
				if (topmost_tpiprovider) {
					if (! opt_level_valid(opt->level,
					    valid_level_arr,
					    valid_level_arr_cnt))
						goto bad_opt;
					/*
					 * level is valid - initialize
					 * option as not supported
					 */
					opt->status = T_NOTSUPPORT;
				}

				*toa_lenp += ROUNDUP_TPIOPT(opt->len);
				continue;
			}
		} else {
			/*
			 * Handle T_ALLOPT case as a special case.
			 * Note: T_ALLOPT does not mean anything
			 * for T_CHECK operation.
			 */
			allopt_len = 0;
			if (tor->MGMT_flags == T_CHECK ||
			    ! topmost_tpiprovider ||
			    ((allopt_len = opt_level_allopts_lengths(opt->level,
				opt_arr, opt_arr_cnt)) == 0)) {
				/*
				 * This is confusing but correct !
				 * It is not valid to to use T_ALLOPT with
				 * T_CHECK flag.
				 *
				 * T_ALLOPT is assumed "expanded" at the
				 * topmost_tpiprovider level so it should not
				 * be there as an "option name" if this is not
				 * a topmost_tpiprovider call and we fail it.
				 *
				 * opt_level_allopts_lengths() is used to verify
				 * that "level" associated with the T_ALLOPT is
				 * supported.
				 *
				 */
				opt->status = T_FAILURE;
				*toa_lenp += ROUNDUP_TPIOPT(opt->len);
				continue;
			}
			ASSERT(allopt_len != 0); /* remove ? */

			*toa_lenp += allopt_len;
			opt->status = T_SUCCESS;
			/* XXX - always set T_ALLOPT 'pass_to_next' for now */
			*pass_to_nextp = true;
			continue;
		}
		/*
		 * Check if option wants to flow downstream
		 */
		if (optd->opdes_props & OP_PASSNEXT)
			*pass_to_nextp = true;

		/* Additional checks dependent on operation. */
		switch (tor->MGMT_flags) {
		case T_DEFAULT:
		case T_CURRENT:

			/*
			 * The opt_chk_lookup() routine call above approved of
			 * this option so we can work on the status for it
			 * based on the permissions for the operation. (This
			 * can override any status for it set at higher levels)
			 * We assume this override is OK since chkfn at this
			 * level approved of this option.
			 *
			 * T_CURRENT semantics:
			 * The read access is required. Else option
			 * status is T_NOTSUPPORT.
			 *
			 * T_DEFAULT semantics:
			 * Note: specification is not clear on this but we
			 * interpret T_DEFAULT semantics such that access to
			 * read value is required for access even the default
			 * value. Otherwise the option status is T_NOTSUPPORT.
			 */
			if (! OA_READ_PERMISSION(optd, priv)) {
				opt->status = T_NOTSUPPORT;
				*toa_lenp += ROUNDUP_TPIOPT(opt->len);
				/* skip to next */
				continue;
			}

			/*
			 * T_DEFAULT/T_CURRENT semantics:
			 * We know that read access is set. If no other access
			 * is set, then status is T_READONLY.
			 */
			if (OA_READONLY_PERMISSION(optd, priv))
				opt->status = T_READONLY;
			else
				opt->status = T_SUCCESS;
			/*
			 * Option passes all checks. Make room for it in the
			 * ack. Note: size stored in table does not include
			 * space for option header.
			 */
			*toa_lenp += sizeof (struct T_opthdr) +
			    ROUNDUP_TPIOPT(optd->opdes_size);
			break;

		case T_CHECK:
		case T_NEGOTIATE:

			/*
			 * T_NEGOTIATE semantics:
			 * If for fixed length option value on input is not the
			 * same as value supplied, then status is T_FAILURE.
			 *
			 * T_CHECK semantics:
			 * If value is supplied, semantics same as T_NEGOTIATE.
			 * It is however ok not to supply a value with T_CHECK.
			 */

			if (tor->MGMT_flags == T_NEGOTIATE ||
			    (opt->len != sizeof (struct T_opthdr))) {
				/*
				 * Implies "value" is specified in T_CHECK or
				 * it is a T_NEGOTIATE request.
				 * Verify size.
				 * Note: This can override anything about this
				 * option request done at a higher level.
				 */
				if (! opt_length_ok(optd, opt)) {
					/* bad size */
					*toa_lenp += ROUNDUP_TPIOPT(opt->len);
					opt->status = T_FAILURE;
					continue;
				}
			}
			/*
			 * The opt_chk_lookup()  routine above() approved of
			 * this option so we can work on the status for it based
			 * on the permissions for the operation. (This can
			 * override anything set at a higher level).
			 *
			 * T_CHECK/T_NEGOTIATE semantics:
			 * Set status to T_READONLY if read is the only access
			 * permitted
			 */
			if (OA_READONLY_PERMISSION(optd, priv)) {
				opt->status = T_READONLY;
				*toa_lenp += ROUNDUP_TPIOPT(opt->len);
				/* skip to next */
				continue;
			}

			/*
			 * T_CHECK/T_NEGOTIATE semantics:
			 * If write (or execute) access is not set, then status
			 * is T_NOTSUPPORT.
			 */
			if (! OA_WRITE_OR_EXECUTE(optd, priv)) {
				opt->status = T_NOTSUPPORT;
				*toa_lenp += ROUNDUP_TPIOPT(opt->len);
				/* skip to next option */
				continue;
			}
			/*
			 * Option passes all checks. Make room for it in the
			 * ack and set success in status.
			 * Note: size stored in table does not include header
			 * length.
			 */
			opt->status = T_SUCCESS;
			*toa_lenp += sizeof (struct T_opthdr) +
			    ROUNDUP_TPIOPT(optd->opdes_size);
			break;

		default:
			return (TBADFLAG);
		}
	} /* for loop scanning input buffer */

	return (0);		/* OK return */
}

/*
 * This routine makes another pass through the option buffer this
 * time acting on the request based on "status" result in the
 * first pass. It also performs "expansion" of T_ALLOPT into
 * all options of a certain level and acts on each for this request.
 */
static t_scalar_t
do_options_second_pass(queue_t *q, mblk_t *reqmp, mblk_t *ackmp, int priv,
    optdb_obj_t *dbobjp, t_uscalar_t *worst_statusp)
{
	boolean_t topmost_tpiprovider = dbobjp->odb_topmost_tpiprovider;

	int failed_option;
	struct T_opthdr *opt, *next_opt;
	struct T_opthdr *opt_start, *opt_end;
	u_char *optr;
	struct T_optmgmt_req *tor =
	    (struct T_optmgmt_req *)reqmp->b_rptr;

	optr = (u_char *)ackmp->b_rptr +
	    sizeof (struct T_optmgmt_ack); /* assumed int32_t aligned */

	/*
	 * Set initial values for scanning input
	 */
	if ((opt_start = (struct T_opthdr *)
	    mi_offset_param(reqmp, tor->OPT_offset, tor->OPT_length)) == NULL) {
bad_opt:;
		return (TBADOPT);
	}
	ASSERT(ISALIGNED_TPIOPT(opt_start)); /* verified in first pass */

	opt_end = (struct T_opthdr *)((u_char *)opt_start + tor->OPT_length);

	for (opt = opt_start; opt < opt_end; opt = next_opt) {

		/* verified in first pass */
		ASSERT(((u_char *)opt + sizeof (struct T_opthdr) <=
		    (u_char *)opt_end));

		next_opt = (struct T_opthdr *)((u_char *)opt +
		    ROUNDUP_TPIOPT(opt->len));

		/* verified in first pass */
		ASSERT((u_char *)next_opt >= (u_char *)opt &&
		    ((next_opt < opt_end) ||
			(((u_char *)next_opt - (u_char *)opt_end) <
			    ALIGN_TPIOPT_size)));

		/*
		 * If the first pass in process_topthdrs_first_pass()
		 * has marked the option as a failure case for the MGMT_flags
		 * semantics then there is not much to do.
		 *
		 * Note: For all practical purposes, T_READONLY status is
		 * a "success" for T_DEFAULT/T_CURRENT and "failure" for
		 * T_CHECK/T_NEGOTIATE
		 */
		failed_option =
		    (opt->status == T_NOTSUPPORT) ||
		    (opt->status == T_FAILURE) ||
		    ((tor->MGMT_flags & (T_NEGOTIATE|T_CHECK)) &&
			(opt->status == T_READONLY));

		if (failed_option) {
			/*
			 * According to T_DEFAULT/T_CURRENT semantics, the
			 * input values, even if present, are to be ignored.
			 * Note: Specification is not clear on this, but we
			 * interpret that even though we ignore the values, we
			 * can return them as is. So we process them similar to
			 * T_CHECK/T_NEGOTIATE case which has the semantics to
			 * return the values as is. XXX If interpretation is
			 * ever determined incorrect fill in appropriate code
			 * here to treat T_DEFAULT/T_CURRENT differently.
			 *
			 * According to T_CHECK/T_NEGOTIATE semantics,
			 * in the case of T_NOTSUPPORT/T_FAILURE/T_READONLY,
			 * the semantics are to return the "value" part of
			 * option untouched. So here we copy the option
			 * head including value part if any to output.
			 */

			bcopy(opt, optr, opt->len);
			optr += ROUNDUP_TPIOPT(opt->len);

			*worst_statusp = get_worst_status(opt->status,
			    *worst_statusp);

			/* skip to process next option in buffer */
			continue;

		} /* end if "failed option" */
		/*
		 * The status is T_SUCCESS or T_READONLY
		 * We process the value part here
		 */
		ASSERT(opt->status == T_SUCCESS || opt->status == T_READONLY);
		switch (tor->MGMT_flags) {
		case T_DEFAULT:
			/*
			 * We fill default value from table or protocol specific
			 * function. If this call fails, we pass input through.
			 */
			if (do_opt_default(q, opt, &optr, worst_statusp,
			    priv, dbobjp) < 0) {
				/* fail or pass transparently */
				if (topmost_tpiprovider)
					opt->status = T_FAILURE;
				bcopy(opt, optr, opt->len);
				optr += ROUNDUP_TPIOPT(opt->len);
				*worst_statusp = get_worst_status(opt->status,
				    *worst_statusp);
			}
			break;

		case T_CURRENT:

			do_opt_current(q, opt, &optr, worst_statusp, priv,
			    dbobjp);
			break;

		case T_CHECK:
		case T_NEGOTIATE:

			do_opt_check_or_negotiate(q, opt, tor->MGMT_flags,
			    &optr, worst_statusp, priv, dbobjp);
			break;
		default:
			return (TBADFLAG);
		}
	} /* end for loop scanning option buffer */

	ackmp->b_wptr = optr;
	ASSERT(ackmp->b_wptr <= ackmp->b_datap->db_lim);

	return (0);		/* OK return */
}


static t_uscalar_t
get_worst_status(t_uscalar_t status, t_uscalar_t current_worst_status)
{
	/*
	 * Return the "worst" among the arguments "status" and
	 * "current_worst_status".
	 *
	 * Note: Tracking "worst_status" can be made a bit simpler
	 * if we use the property that status codes are bitwise
	 * distinct.
	 *
	 * The pecking order is
	 *
	 * T_SUCCESS ..... best
	 * T_PARTSUCCESS
	 * T_FAILURE
	 * T_READONLY
	 * T_NOTSUPPORT... worst
	 */
	if (status == current_worst_status)
		return (current_worst_status);
	switch (current_worst_status) {
	case T_SUCCESS:
		if (status == T_PARTSUCCESS)
			return (T_PARTSUCCESS);
		/* FALLTHROUGH */
	case T_PARTSUCCESS:
		if (status == T_FAILURE)
			return (T_FAILURE);
		/* FALLTHROUGH */
	case T_FAILURE:
		if (status == T_READONLY)
			return (T_READONLY);
		/* FALLTHROUGH */
	case T_READONLY:
		if (status == T_NOTSUPPORT)
			return (T_NOTSUPPORT);
		/* FALLTHROUGH */
	case T_NOTSUPPORT:
	default:
		return (current_worst_status);
	}
}

static int
do_opt_default(queue_t *q, struct T_opthdr *reqopt, u_char **resptrp,
    t_uscalar_t *worst_statusp, int priv, optdb_obj_t *dbobjp)
{
	pfi_t	deffn = dbobjp->odb_deffn;
	opdes_t	*opt_arr = dbobjp->odb_opt_des_arr;
	u_int opt_arr_cnt = dbobjp->odb_opt_arr_cnt;
	boolean_t topmost_tpiprovider = dbobjp->odb_topmost_tpiprovider;

	struct T_opthdr *topth;
	opdes_t *optd;

	if (reqopt->name != T_ALLOPT) {
		/*
		 * lookup the option in the table and fill default value
		 */
		optd = opt_chk_lookup(reqopt->level, reqopt->name,
		    opt_arr, opt_arr_cnt);

		if (optd == NULL) {
			/*
			 * not found - fail this one. Should not happen
			 * for topmost_tpiprovider as calling routine
			 * should have verified it.
			 */
			ASSERT(! topmost_tpiprovider);
			return (-1);
		}

		topth = (struct T_opthdr *)(*resptrp);
		topth->level = reqopt->level;
		topth->name = reqopt->name;
		topth->status = reqopt->status;

		*worst_statusp = get_worst_status(reqopt->status,
		    *worst_statusp);

		if (optd->opdes_props & OP_NODEFAULT) {
			/* header only, no default "value" part */
			topth->len = sizeof (struct T_opthdr);
			*resptrp += sizeof (struct T_opthdr);
		} else {
			int deflen;

			if (optd->opdes_props & OP_DEF_FN) {
				deflen = (*deffn)(q, reqopt->level,
				    reqopt->name, (u_char *)&topth[1]);
				if (deflen >= 0) {
					topth->len = (t_uscalar_t)
					    (sizeof (struct T_opthdr) + deflen);
				} else {
					/*
					 * return error, this should 'pass
					 * through' the option and maybe some
					 * other level will fill it in or
					 * already did.
					 * (No change in 'resptrp' upto here)
					 */
					return (-1);
				}
			} else {
				/* fill length and value part */
				switch (optd->opdes_size) {
				/*
				 * Since options are guaranteed aligned only
				 * on a 4 byte boundary (t_scalar_t) any
				 * option that is greater in size will default
				 * to the bcopy below
				 */
				case sizeof (int32_t):
					*(int32_t *)&topth[1] =
					    (int32_t)optd->opdes_default;
					break;
				case sizeof (int16_t):
					*(int16_t *)&topth[1] =
					    (int16_t)optd->opdes_default;
					break;
				case sizeof (int8_t):
					*(int8_t *)&topth[1] =
					    (int8_t)optd->opdes_default;
					break;
				default:
					/*
					 * other length but still assume
					 * fixed - use bcopy
					 */
					bcopy(optd->opdes_defbuf,
					    &topth[1], optd->opdes_size);
					break;
				}
				topth->len = (t_uscalar_t)(optd->opdes_size +
				    sizeof (struct T_opthdr));
			}
			*resptrp += ROUNDUP_TPIOPT(topth->len);
		}
		return (0);	/* OK return */
	}

	/*
	 * T_ALLOPT processing
	 *
	 * lookup and stuff default values of all the options of the
	 * level specified
	 * Note: This expansion of T_ALLOPT should happen in
	 * a topmost_tpiprovider.
	 */
	ASSERT(topmost_tpiprovider);
	for (optd = opt_arr; optd < &opt_arr[opt_arr_cnt]; optd++) {
		if (reqopt->level != optd->opdes_level)
			continue;
		/*
		 *
		 * T_DEFAULT semantics:
		 * XXX: we interpret T_DEFAULT semantics such that access to
		 * read value is required for access even the default value.
		 * Else option is ignored for T_ALLOPT request.
		 */
		if (! OA_READ_PERMISSION(optd, priv))
			/* skip this one */
			continue;

		/*
		 * Found option of same level as T_ALLOPT request
		 * that we can return.
		 */

		topth = (struct T_opthdr *)(*resptrp);
		topth->level = optd->opdes_level;
		topth->name = optd->opdes_name;

		/*
		 * T_DEFAULT semantics:
		 * We know that read access is set. If no other access is set,
		 * then status is T_READONLY
		 */
		if (OA_READONLY_PERMISSION(optd, priv)) {
			topth->status = T_READONLY;
			*worst_statusp = get_worst_status(T_READONLY,
			    *worst_statusp);
		} else {
			topth->status = T_SUCCESS;
			/*
			 * Note: *worst_statusp has to be T_SUCCESS or
			 * worse so no need to adjust
			 */
		}

		if (optd->opdes_props & OP_NODEFAULT) {
			/* header only, no value part */
			topth->len = sizeof (struct T_opthdr);
			*resptrp += sizeof (struct T_opthdr);
		} else {
			int deflen;

			if (optd->opdes_props & OP_DEF_FN) {
				deflen = (*deffn)(q, reqopt->level,
				    reqopt->name, (u_char *)&topth[1]);
				if (deflen >= 0) {
					topth->len = (t_uscalar_t)(deflen +
					    sizeof (struct T_opthdr));
				} else {
					/*
					 * deffn failed.
					 * return just the header as T_ALLOPT
					 * expansion.
					 * Some other level deffn may
					 * supply value part.
					 */
					topth->len = sizeof (struct T_opthdr);
					topth->status = T_FAILURE;
					*worst_statusp =
					    get_worst_status(T_FAILURE,
						*worst_statusp);
				}
			} else {
				/*
				 * fill length and value part from
				 * table
				 */
				switch (optd->opdes_size) {
				/*
				 * Since options are guaranteed aligned only
				 * on a 4 byte boundary (t_scalar_t) any
				 * option that is greater in size will default
				 * to the bcopy below
				 */
				case sizeof (int32_t):
					*(int32_t *)&topth[1] =
					    (int32_t)optd->opdes_default;
					break;
				case sizeof (int16_t):
					*(int16_t *)&topth[1] =
					    (int16_t)optd->opdes_default;
					break;
				case sizeof (int8_t):
					*(int8_t *)&topth[1] =
					    (int8_t)optd->opdes_default;
					break;
				default:
					/*
					 * other length but still assume
					 * fixed - use bcopy
					 */
					bcopy(optd->opdes_defbuf,
					    &topth[1], optd->opdes_size);
				}
				topth->len = (t_uscalar_t)(optd->opdes_size +
				    sizeof (struct T_opthdr));
			}
			*resptrp += ROUNDUP_TPIOPT(topth->len);
		}
	}
	return (0);
}

static void
do_opt_current(queue_t *q, struct T_opthdr *reqopt, u_char **resptrp,
    t_uscalar_t *worst_statusp, int priv, optdb_obj_t *dbobjp)
{
	pfi_t	getfn = dbobjp->odb_getfn;
	opdes_t	*opt_arr = dbobjp->odb_opt_des_arr;
	u_int opt_arr_cnt = dbobjp->odb_opt_arr_cnt;
	boolean_t topmost_tpiprovider = dbobjp->odb_topmost_tpiprovider;

	struct T_opthdr *topth;
	opdes_t *optd;
	int optlen;
	u_char *initptr = *resptrp;

	/*
	 * We call getfn to get the current value of an option. The call may
	 * fail in which case we copy the values from the input buffer. Maybe
	 * something downstream will fill it in or something upstream did.
	 */

	if (reqopt->name != T_ALLOPT) {
		topth = (struct T_opthdr *)*resptrp;
		*resptrp += sizeof (struct T_opthdr);
		optlen = (*getfn)(q, reqopt->level, reqopt->name, *resptrp);
		if (optlen >= 0) {
			topth->len = (t_uscalar_t)(optlen +
			    sizeof (struct T_opthdr));
			topth->level = reqopt->level;
			topth->name = reqopt->name;
			topth->status = reqopt->status;
			*resptrp += ROUNDUP_TPIOPT(optlen);
			*worst_statusp = get_worst_status(topth->status,
			    *worst_statusp);
		} else {
			/* failed - reset "*resptrp" pointer */
			*resptrp -= sizeof (struct T_opthdr);
		}
	} else {		/* T_ALLOPT processing */
		ASSERT(topmost_tpiprovider == true);
		/* scan and get all options */
		for (optd = opt_arr; optd < &opt_arr[opt_arr_cnt]; optd++) {
			/* skip other levels */
			if (reqopt->level != optd->opdes_level)
				continue;

			if (! OA_READ_PERMISSION(optd, priv))
				/* skip this one */
				continue;

			topth = (struct T_opthdr *)*resptrp;
			*resptrp += sizeof (struct T_opthdr);

			/* get option of this level */
			optlen = (*getfn)(q, reqopt->level, optd->opdes_name,
			    *resptrp);
			if (optlen >= 0) {
				/* success */
				topth->len = (t_uscalar_t)(optlen +
				    sizeof (struct T_opthdr));
				topth->level = reqopt->level;
				topth->name = optd->opdes_name;
				if (OA_READONLY_PERMISSION(optd, priv))
					topth->status = T_READONLY;
				else
					topth->status = T_SUCCESS;
				*resptrp += ROUNDUP_TPIOPT(optlen);
			} else {
				/*
				 * failed, return as T_FAILURE and null value
				 * part. Maybe something downstream will
				 * handle this one and fill in a value. Here
				 * it is just part of T_ALLOPT expansion.
				 */
				topth->len = sizeof (struct T_opthdr);
				topth->level = reqopt->level;
				topth->name = optd->opdes_name;
				topth->status = T_FAILURE;
			}
			*worst_statusp = get_worst_status(topth->status,
			    *worst_statusp);
		} /* end for loop */
	}
	if (*resptrp == initptr) {
		/*
		 * getfn failed and does not want to handle this option. Maybe
		 * something downstream will or something upstream did. (If
		 * topmost_tpiprovider, initialize "status" to failure which
		 * can possibly change downstream). Copy the input "as is" from
		 * input option buffer if any to maintain transparency.
		 */
		if (topmost_tpiprovider)
			reqopt->status = T_FAILURE;
		bcopy(reqopt, *resptrp, reqopt->len);
		*resptrp += ROUNDUP_TPIOPT(reqopt->len);
		*worst_statusp = get_worst_status(reqopt->status,
		    *worst_statusp);
	}
}



static void
do_opt_check_or_negotiate(queue_t *q, struct T_opthdr *reqopt,
    t_scalar_t mgmt_flags, u_char **resptrp, t_uscalar_t *worst_statusp,
    int priv, optdb_obj_t *dbobjp)
{
	pfi_t	deffn = dbobjp->odb_deffn;
	pfi_t	setfn = dbobjp->odb_setfn;
	opdes_t	*opt_arr = dbobjp->odb_opt_des_arr;
	u_int opt_arr_cnt = dbobjp->odb_opt_arr_cnt;
	boolean_t topmost_tpiprovider = dbobjp->odb_topmost_tpiprovider;

	struct T_opthdr *topth;
	opdes_t *optd;
	int error, optlen, optsize;
	u_char *initptr = *resptrp;

	ASSERT(reqopt->status == T_SUCCESS);

	if (reqopt->name != T_ALLOPT) {
		topth = (struct T_opthdr *)*resptrp;
		*resptrp += sizeof (struct T_opthdr);
		error = (*setfn)(q, mgmt_flags, reqopt->level, reqopt->name,
		    reqopt->len - sizeof (struct T_opthdr),
		    (u_char *)&reqopt[1], &optlen, (u_char *)&topth[1]);
		if (error) {
			/* failed - reset "*resptrp" */
			*resptrp -= sizeof (struct T_opthdr);
		} else {
			/*
			 * success - "value" already filled in setfn()
			 */
			topth->len = (t_uscalar_t)(optlen +
			    sizeof (struct T_opthdr));
			topth->level = reqopt->level;
			topth->name = reqopt->name;
			topth->status = reqopt->status;
			*resptrp += ROUNDUP_TPIOPT(optlen);
			*worst_statusp = get_worst_status(topth->status,
			    *worst_statusp);
		}
	} else {		/* T_ALLOPT processing */
		/* only for T_NEGOTIATE case */
		ASSERT(mgmt_flags == T_NEGOTIATE);
		ASSERT(topmost_tpiprovider == true);

		/* scan and set all options to default value */
		for (optd = opt_arr; optd < &opt_arr[opt_arr_cnt]; optd++) {

			/* skip other levels */
			if (reqopt->level != optd->opdes_level)
				continue;

			if (OA_EXECUTE_PERMISSION(optd, priv)) {
				/*
				 * skip this one too. Does not make sense to
				 * set anything to default value for "execute"
				 * options.
				 */
				continue;
			}

			if (OA_READONLY_PERMISSION(optd, priv)) {
				/*
				 * Return with T_READONLY status (and no value
				 * part). Note: spec is not clear but
				 * XTI test suite needs this.
				 */
				topth = (struct T_opthdr *)*resptrp;
				topth->len = sizeof (struct T_opthdr);
				*resptrp += topth->len;
				topth->level = reqopt->level;
				topth->name = optd->opdes_name;
				topth->status = T_READONLY;
				*worst_statusp = get_worst_status(topth->status,
				    *worst_statusp);
				continue;
			}

			/*
			 * It is not read only or execute type
			 * the it must have write permission
			 */
			ASSERT(OA_WRITE_PERMISSION(optd, priv));

			topth = (struct T_opthdr *)*resptrp;
			*resptrp += sizeof (struct T_opthdr);

			topth->len = sizeof (struct T_opthdr);
			topth->level = reqopt->level;
			topth->name = optd->opdes_name;
			if (optd->opdes_props & OP_NODEFAULT) {
				/*
				 * Option of "no default value" so it does not
				 * make sense to try to set it. We just return
				 * header with status of T_SUCCESS
				 * XXX should this be failure ?
				 */
				topth->status = T_SUCCESS;
				continue; /* skip setting */
			}
			if (optd->opdes_props & OP_DEF_FN) {
				if ((optd->opdes_props & OP_VARLEN) ||
				    ((optsize = (*deffn)(q, reqopt->level,
					optd->opdes_name,
					(u_char *)optd->opdes_defbuf)) < 0)) {
					/* XXX - skip these too */
					topth->status = T_SUCCESS;
					continue; /* skip setting */
				}
			} else {
				optsize = optd->opdes_size;
			}


			/* set option of this level */
			error = (*setfn)(q, T_NEGOTIATE, reqopt->level,
			    optd->opdes_name, optsize,
			    (u_char *)optd->opdes_defbuf, &optlen,
			    (u_char *)&topth[1]);
			if (error) {
				/*
				 * failed, return as T_FAILURE and null value
				 * part. Maybe something downstream will
				 * handle this one and fill in a value. Here
				 * it is just part of T_ALLOPT expansion.
				 */
				topth->status = T_FAILURE;
				*worst_statusp = get_worst_status(topth->status,
				    *worst_statusp);
			} else {
				/* success */
				topth->len += optlen;
				topth->status = T_SUCCESS;
				*resptrp += ROUNDUP_TPIOPT(optlen);
			}
		} /* end for loop */
		/* END T_ALLOPT */
	}

	if (*resptrp == initptr) {
		/*
		 * setfn failed and does not want to handle this option. Maybe
		 * something downstream will or something upstream
		 * did. Copy the input as is from input option buffer if any to
		 * maintain transparency (maybe something at a level above
		 * did something.
		 */
		if (topmost_tpiprovider)
			reqopt->status = T_FAILURE;
		bcopy(reqopt, *resptrp, reqopt->len);
		*resptrp += ROUNDUP_TPIOPT(reqopt->len);
		*worst_statusp = get_worst_status(reqopt->status,
		    *worst_statusp);
	}
}

/*
 * The following routines process options buffer passed with
 * T_CONN_REQ, T_CONN_RES and T_UNITDATA_REQ.
 * This routine does the consistency check applied to the
 * sanity of formatting of multiple options packed in the
 * buffer.
 *
 * XTI brain damage alert:
 * XTI interface adopts the notion of an option being an
 * "absolute requirement" from OSI transport service (but applies
 * it to all transports including Internet transports).
 * The main effect of that is action on failure to "negotiate" a
 * requested option to the exact requested value
 *
 *          - if the option is an "absolute requirement", the primitive
 *            is aborted (e.g T_DISCON_REQ or T_UDERR generated)
 *          - if the option is NOT and "absolute requirement" it can
 *            just be ignored.
 *
 * We would not support "negotiating" of options on connection
 * primitives for Internet transports. However just in case we
 * forced to in order to pass strange test suites, the design here
 * tries to support these notions.
 *
 * tpi_optcom_buf(q, mp, opt_lenp, opt_offset, priv, dbobjp)
 *
 * - Verify the option buffer, if formatted badly, return error 1
 *
 * - If it is a "permissions" failure (read-only), return error 2
 *
 * - Else, process the option "in place", the following can happen,
 *	     - if a "privileged" option, mark it as "ignored".
 *	     - if "not supported", mark "ignored"
 *	     - if "supported" attempt negotiation and fill result in
 *	       the outcome
 *			- if "absolute requirement", return error 3
 *			- if NOT an "absolute requirement", then our
 *			  interpretation is to mark is at ignored if
 *			  negotiation fails (Spec allows partial success
 *			  as in OSI protocols but not failure)
 *
 *   Then delete "ignored" options from option buffer and return success.
 *
 */

int
tpi_optcom_buf(queue_t *q, mblk_t *mp, t_scalar_t *opt_lenp,
    t_scalar_t opt_offset, int priv, optdb_obj_t *dbobjp)
{
	pfi_t	setfn = dbobjp->odb_setfn;
	opdes_t *opt_arr = dbobjp->odb_opt_des_arr;
	u_int opt_arr_cnt = dbobjp->odb_opt_arr_cnt;
	struct T_opthdr *opt, *next_opt;
	struct T_opthdr *opt_start, *opt_end;
	mblk_t  *copy_mp_head;
	u_char *optr, *init_optr;
	opdes_t *optd;
	t_scalar_t setflag;
	int olen;
	int error = 0;
	int retval = 0;

	ASSERT((u_char *)opt_lenp > mp->b_rptr &&
	    (u_char *)opt_lenp < mp->b_wptr);

	copy_mp_head = NULL;

	if ((opt_start = (struct T_opthdr *)
	    mi_offset_param(mp, opt_offset, *opt_lenp)) == NULL) {
bad_opt:;
		retval = OB_BADOPT;
		goto error_ret;
	}
	if (! ISALIGNED_TPIOPT(opt_start))
		goto bad_opt;

	opt_end = (struct T_opthdr *)((u_char *)opt_start
	    + *opt_lenp);

	if ((copy_mp_head = copyb(mp)) == (mblk_t *)NULL) {
		retval = OB_NOMEM;
		goto error_ret;
	}

	init_optr = optr = (u_char *)&copy_mp_head->b_rptr[opt_offset];

	for (opt = opt_start; opt < opt_end; opt = next_opt) {
		/*
		 * Verify we have room to reference the option header
		 * fields in the option buffer.
		 */
		if ((u_char *)opt + sizeof (struct T_opthdr) >
		    (u_char *)opt_end)
			goto bad_opt;

		/*
		 * We now compute pointer to next option in buffer 'next_opt'
		 * The next_opt computation above below 'opt->len' initialized
		 * by application which cannot be trusted. The usual value
		 * too large will be captured by the loop termination condition
		 * above. We check for the following which it will miss.
		 * 	-pointer space wraparound arithmetic overflow
		 *	-last option in buffer with 'opt->len' being too large
		 *	 (only reason 'next_opt' should equal or exceed
		 *	 'opt_end' for last option is roundup unless length is
		 *	 too-large/invalid)
		 */
		next_opt = (struct T_opthdr *)((u_char *)opt +
		    ROUNDUP_TPIOPT(opt->len));

		if ((u_char *)next_opt < (u_char *)opt ||
		    ((next_opt >= opt_end) &&
			(((u_char *)next_opt - (u_char *)opt_end) >=
			    ALIGN_TPIOPT_size)))
			goto bad_opt;

		/* Find the option in the opt_arr. */
		optd = opt_chk_lookup(opt->level, opt->name,
		    opt_arr, opt_arr_cnt);

		if (optd == NULL) {
			/*
			 * Option not found
			 */
			opt->status = T_NOTSUPPORT;
			continue;
		}

		/*
		 * Weird but as in XTI spec.
		 * Sec 6.3.6 "Privileged and ReadOnly Options"
		 * Permission problems (e.g.readonly) fail with bad access
		 * BUT "privileged" option request from those NOT PRIVILEGED
		 * are to be merely "ignored".
		 * XXX Prevents "probing" of privileged options ?
		 */
		if (OA_READONLY_PERMISSION(optd, priv)) {
			retval = OB_NOACCES;
			goto error_ret;
		}
		if (priv) {
			/*
			 * For privileged options, we DO perform
			 * access checks as is common sense
			 */
			if (! OA_WX_ANYPRIV(optd)) {
				retval = OB_NOACCES;
				goto error_ret;
			}
		} else {
			/*
			 * For non privileged, we fail instead following
			 * "ignore" semantics dictated by XTI spec for
			 * permissions problems.
			 * Sec 6.3.6 "Privileged and ReadOnly Options"
			 * XXX Should we do "ignore" semantics ?
			 */
			if (! OA_WX_NOPRIV(optd)) { /* nopriv */
				opt->status = T_FAILURE;
				continue;
			}
		}
		/*
		 *
		 * If the negotiation fails, for options that
		 * are "absolute requirement", it is a fatal error.
		 * For options that are NOT "absolute requirements",
		 * and the value fails to negotiate, the XTI spec
		 * only considers the possibility of partial success
		 * (T_PARTSUCCES - not likely for Internet protocols).
		 * The spec is in denial about complete failure
		 * (T_FAILURE) to negotiate for options that are
		 * carried on T_CONN_REQ/T_CONN_RES/T_UNITDATA
		 * We interpret the T_FAILURE to negotiate an option
		 * that is NOT an absolute requirement that it is safe
		 * to ignore it.
		 */

		/* verify length */
		if (! opt_length_ok(optd, opt)) {
			/* bad size */
			if ((optd->opdes_props & OP_NOT_ABSREQ) == 0) {
				/* option is absolute requirement */
				retval = OB_ABSREQ_FAIL;
				goto error_ret;
			}
			opt->status = T_FAILURE;
			continue;
		}

		/*
		 * verified generic attributes. Now call set function.
		 * Note: We assume the following to simplify code.
		 * XXX If this is found not to be valid, this routine
		 * will need to be rewritten. At this point it would
		 * be premature to introduce more complexity than is
		 * needed.
		 * Assumption: For variable length options, we assume
		 * that the value returned will be same or less length
		 * (size does not increase). This makes it OK to pass the
		 * same space for output as it is on input.
		 */

		/*
		 * Set both T_NEGOTIATE and T_CHECK
		 * secret signal that it not call from usual
		 * T_OPTMGMT_REQ but "negotiate" from another
		 * primitive
		 */
		setflag = T_NEGOTIATE | T_CHECK;

		error = (*setfn)(q, setflag,
		    opt->level, opt->name,
		    opt->len - (t_uscalar_t)sizeof (struct T_opthdr),
		    (u_char *)&opt[1], &olen, (u_char *)&opt[1]);

		if (olen > (int)(opt->len - sizeof (struct T_opthdr))) {
			/*
			 * Space on output more than space on input. Should
			 * not happen and we consider it a bug/error.
			 * More of a restriction than an error in our
			 * implementation. Will see if we can live with this
			 * otherwise code will get more hairy with multiple
			 * passes.
			 */
			retval = OB_INVAL;
			goto error_ret;
		}
		if (error) {
			if ((optd->opdes_props & OP_NOT_ABSREQ) == 0) {
				/* option is absolute requirement. */
				retval = OB_ABSREQ_FAIL;
				goto error_ret;
			}
			/*
			 * failed - but option "not an absolute
			 * requirement"
			 */
			opt->status = T_FAILURE;
			continue;
		}
		/*
		 * Fill in the only possible successful result
		 * (Note: TPI allows for T_PARTSUCCESS - partial
		 * sucess result code which is relevant in OSI world
		 * and not possible in Internet code)
		 */
		opt->status = T_SUCCESS;

		/*
		 * Add T_SUCCESS result code options to the "output" options.
		 * No T_FAILURES or T_NOTSUPPORT here as they are to be
		 * ignored.
		 * This code assumes output option buffer will
		 * be <= input option buffer.
		 *
		 * Copy option header+value
		 */
		bcopy(opt, optr, opt->len);
		optr +=  ROUNDUP_TPIOPT(opt->len);
	}
	/*
	 * Overwrite the input mblk option buffer now with the output
	 * and update length, and contents in original mbl
	 * (offset remains unchanged).
	 */
	*opt_lenp = (t_scalar_t)(optr - init_optr);
	if (*opt_lenp > 0) {
		bcopy(init_optr, opt_start, *opt_lenp);
	}
	freeb(copy_mp_head);
	return (OB_SUCCESS);

error_ret:
	if (copy_mp_head != NULL)
		freeb(copy_mp_head);
	return (retval);
}

static opdes_t *
opt_chk_lookup(t_uscalar_t level, t_uscalar_t name, opdes_t *opt_arr,
    u_int opt_arr_cnt)
{
	opdes_t		*optd;

	for (optd = opt_arr; optd < &opt_arr[opt_arr_cnt];
	    optd++) {
		if (level == (u_int)optd->opdes_level &&
		    name == (u_int)optd->opdes_name)
			return (optd);
	}
	return (NULL);
}

static boolean_t
opt_level_valid(t_uscalar_t level, optlevel_t *valid_level_arr,
    u_int valid_level_arr_cnt)
{
	optlevel_t		*olp;

	for (olp = valid_level_arr;
	    olp < &valid_level_arr[valid_level_arr_cnt];
	    olp++) {
		if (level == (u_int)(*olp))
			return (true);
	}
	return (false);
}


static size_t
opt_level_allopts_lengths(t_uscalar_t level, opdes_t *opt_arr,
    u_int opt_arr_cnt)
{
	opdes_t		*optd;
	size_t allopt_len = 0;	/* 0 implies no option at this level */

	/*
	 * Scan opt_arr computing aggregate length
	 * requirement for storing values of all
	 * options.
	 * Note: we do not filter for permissions
	 * etc. This will be >= the real aggregate
	 * length required (upper bound).
	 */

	for (optd = opt_arr; optd < &opt_arr[opt_arr_cnt];
	    optd++) {
		if (level == optd->opdes_level) {
			allopt_len += sizeof (struct T_opthdr) +
			    ROUNDUP_TPIOPT(optd->opdes_size);
		}
	}
	return (allopt_len);	/* 0 implies level not found */
}

t_uscalar_t
optcom_max_optbuf_len(opdes_t *opt_arr, u_int opt_arr_cnt)
{
	t_uscalar_t max_optbuf_len = sizeof (struct T_info_ack);
	opdes_t		*optd;

	for (optd = opt_arr; optd < &opt_arr[opt_arr_cnt]; optd++) {
		max_optbuf_len += (t_uscalar_t)sizeof (struct T_opthdr) +
		    (t_uscalar_t)ROUNDUP_TPIOPT(optd->opdes_size);
	}
	return (max_optbuf_len);
}


static boolean_t
opt_length_ok(opdes_t *optd, struct T_opthdr *opt)
{
	/*
	 * Verify length.
	 * Value specified should match length of fixed length option or be
	 * less than maxlen of variable length option.
	 */
	if (optd->opdes_props & OP_VARLEN) {
		if (opt->len <= optd->opdes_size +
		    (t_uscalar_t)sizeof (struct T_opthdr))
			return (true);
	} else {
		/* fixed length option */
		if (opt->len == optd->opdes_size +
		    (t_uscalar_t)sizeof (struct T_opthdr))
			return (true);
	}
	return (false);
}
