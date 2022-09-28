/*
 * Copyright (c) 1992-1997 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)udp.c	1.76	97/12/06 SMI"

#include <sys/types.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/strlog.h>
#define	_SUN_TPI_VERSION 2
#include <sys/tihdr.h>
#include <sys/timod.h>
#include <sys/ddi.h>
#include <sys/suntpi.h>
#include <sys/cmn_err.h>

#include <sys/socket.h>
#include <sys/vtrace.h>
#include <sys/debug.h>
#include <sys/isa_defs.h>
#include <netinet/in.h>

#include <inet/common.h>
#include <inet/ip.h>
#include <inet/mi.h>
#include <inet/mib2.h>
#include <inet/nd.h>
#include <inet/optcom.h>
#include <inet/snmpcom.h>

/*
 * Object to represent database of options to search passed to
 * {sock,tpi}optcom_req() interface routine to take care of option
 * management and associated methods.
 * XXX. These and other externs should really move to a udp header file.
 */
extern optdb_obj_t	udp_opt_obj;
extern u_int		udp_max_optbuf_len;

/*
 * Synchronization notes:
 *
 * UDP uses a combination of the queue-pair STREAMS perimeter and a global
 * lock to protect its data structures.
 *
 * The queue-pair perimeter is not acquired exclusively in the put procedures
 * thus when udp_rput or udp_wput needs exclusive access to the udp_t
 * instance structure it will use qwriter() (become_exclusive)
 * to asynchronously acquire exclusive access to the udp_t instance.
 *
 * When UDP global data needs to be modified the udp_g_lock mutex is acquired.
 */

/* UDP Protocol header */
typedef	struct udphdr_s {
	uint8_t	uh_src_port[2];		/* Source port */
	uint8_t	uh_dst_port[2];		/* Destination port */
	uint8_t	uh_length[2];		/* UDP length */
	uint8_t	uh_checksum[2];		/* UDP checksum */
} udph_t;
#define	UDPH_SIZE	8

/* UDP Protocol header aligned */
typedef	struct udpahdr_s {
	in_port_t	uha_src_port;		/* Source port */
	in_port_t	uha_dst_port;		/* Destination port */
	uint16_t	uha_length;		/* UDP length */
	uint16_t	uha_checksum;		/* UDP checksum */
} udpha_t;

/* Internal udp control structure, one per open stream */
typedef	struct ud_s {
	uint32_t udp_state;		/* TPI state */
	uint8_t	udp_pad[2];
	in_port_t udp_port;		/* Port number bound to this stream */
	ipaddr_t udp_src;		/* Source address of this stream */
	ipaddr_t udp_bound_src;		/* Explicitely bound to address */
	uint32_t udp_hdr_length; 	/* number of bytes used in udp_iphc */
	uint32_t udp_family;		/* Addr family used in bind, if any */
	uint32_t udp_ip_snd_options_len; /* Length of IP options supplied. */
	u_char	*udp_ip_snd_options;	/* Pointer to IP options supplied */
	uint32_t udp_ip_rcv_options_len; /* Length of IP options supplied. */
	u_char	*udp_ip_rcv_options;	/* Pointer to IP options supplied */
	union {
		u_char	udpu1_multicast_ttl;	/* IP_MULTICAST_TTL option */
		uint32_t udpu1_pad;
	} udp_u1;
#define	udp_multicast_ttl	udp_u1.udpu1_multicast_ttl
	ipaddr_t udp_multicast_if_addr;	/* IP_MULTICAST_IF option */
	udpha_t	*udp_udpha;		/* Connected header */
	mblk_t	*udp_hdr_mp;		/* T_UNIDATA_IND for connected */
	uint32_t udp_priv_stream : 1,	/* Stream opened by privileged user */
		udp_debug : 1,		/* SO_DEBUG "socket" option. */
		udp_dontroute : 1,	/* SO_DONTROUTE "socket" option. */
		udp_broadcast : 1,	/* SO_BROADCAST "socket" option. */

		udp_useloopback : 1,	/* SO_USELOOPBACK "socket" option. */
		udp_reuseaddr : 1,	/* SO_REUSEADDR "socket" option. */
		udp_multicast_loop : 1,	/* IP_MULTICAST_LOOP option */
		udp_dgram_errind : 1,	/* SO_DGRAM_ERRIND option */

		udp_recvdstaddr : 1,	/* IP_RECVDSTADDR option */
		udp_recvopts : 1,	/* IP_RECVOPTS option */
		udp_discon_pending : 1,	/* T_DISCON_REQ in progress */

		udp_pad_to_bit_31 : 21;
	union {
		char	udpu2_iphc[IP_MAX_HDR_LENGTH + UDPH_SIZE];
		ipha_t	udpu2_ipha;
		uint32_t udpu2_ipharr[7];
		double	udpu2_aligner;
	} udp_u2;
#define	udp_iphc	udp_u2.udpu2_iphc
#define	udp_ipha	udp_u2.udpu2_ipha
#define	udp_ipharr	udp_u2.udpu2_ipharr
	uint8_t	udp_pad2[2];
	uint8_t	udp_type_of_service;
	uint8_t	udp_ttl;
} udp_t;

/* Named Dispatch Parameter Management Structure */
typedef struct udpparam_s {
	uint32_t udp_param_min;
	uint32_t udp_param_max;
	uint32_t udp_param_value;
	char	*udp_param_name;
} udpparam_t;

static	void	udp_bind(queue_t *q, MBLKP mp);
static	int	udp_close(queue_t *q);
static	void	udp_connect(queue_t *q, MBLKP mp);
static	void	udp_disconnect(queue_t *q, MBLKP mp);
static void	udp_err_ack(queue_t *q, MBLKP mp, t_scalar_t t_error,
    int sys_error);
static	void	udp_err_ack_prim(queue_t *q, mblk_t *mp, int primitive,
    t_scalar_t tlierr, int unixerr);
static	int	udp_extra_priv_ports_get(queue_t *q, mblk_t *mp, caddr_t cp);
static	int	udp_extra_priv_ports_add(queue_t *q, mblk_t *mp,
    char *value, caddr_t cp);
static	int	udp_extra_priv_ports_del(queue_t *q, mblk_t *mp,
    char *value, caddr_t cp);
static	void	udp_capability_req(queue_t *q, MBLKP mp);
static	void	udp_info_req(queue_t *q, MBLKP mp);
static mblk_t	*udp_ip_bind_mp(udp_t *udp, t_scalar_t bind_prim,
    t_scalar_t addr_length);
static	void	udp_addr_req(queue_t *q, MBLKP mp);
static	int	udp_open(queue_t *q, dev_t *devp, int flag, int sflag,
    cred_t *credp);
static  int	udp_unitdata_opt_process(queue_t *q, mblk_t *mp,
    int *t_errorp);
static	boolean_t udp_allow_udropt_set(t_scalar_t level, t_scalar_t name);

int	udp_opt_default(queue_t *q, t_scalar_t level, t_scalar_t name,
    u_char *ptr);
int	udp_opt_get(queue_t *q, t_scalar_t level, t_scalar_t name, u_char *ptr);
int	udp_opt_set(queue_t *q, u_int mgmt_flags, t_scalar_t level,
    t_scalar_t name, u_int inlen, u_char *invalp, u_int *outlenp,
    u_char *outvalp);

static int	udp_param_get(queue_t *q, mblk_t *mp, caddr_t cp);
static boolean_t	udp_param_register(udpparam_t *udppa, int cnt);
static int	udp_param_set(queue_t *q, mblk_t *mp, char *value, caddr_t cp);
static	void	udp_rput(queue_t *q, MBLKP mp);
static	void	udp_rput_other(queue_t *q, MBLKP mp);
static	int	udp_snmp_get(queue_t *q, mblk_t *mpctl);
static	int	udp_snmp_set(queue_t *q, t_scalar_t level, t_scalar_t name,
    u_char *ptr, int len);
static	int	udp_status_report(queue_t *q, mblk_t *mp, caddr_t cp);
static void	udp_ud_err(queue_t *q, MBLKP mp, t_scalar_t err);
static	void	udp_unbind(queue_t *q, MBLKP mp);
static in_port_t udp_update_next_port(in_port_t port);
static	void	udp_wput(queue_t *q, MBLKP mp);
static	void	udp_wput_other(queue_t *q, MBLKP mp);

static struct module_info info =  {
	5607, "udp", 1, INFPSZ, 512, 128
};

static struct qinit rinit = {
	(pfi_t)udp_rput, nil(pfi_t), udp_open, udp_close, nil(pfi_t), &info
};

static struct qinit winit = {
	(pfi_t)udp_wput, nil(pfi_t), nil(pfi_t), nil(pfi_t), nil(pfi_t), &info
};

struct streamtab udpinfo = {
	&rinit, &winit
};

	int	udpdevflag = 0;

/* Protected by udp_g_lock */
static	void	*udp_g_head;	/* Head for list of open udp streams. */
static	in_port_t	udp_g_next_port_to_try;
kmutex_t	udp_g_lock;	/* Protects the above three variables */

/*
 * Extra privileged ports. In host byte order. Protected by udp_g_lock.
 */
#define	UDP_NUM_EPRIV_PORTS	64
static int	udp_g_num_epriv_ports = UDP_NUM_EPRIV_PORTS;
static in_port_t udp_g_epriv_ports[UDP_NUM_EPRIV_PORTS] = { 2049, 4045 };

/* Only modified during _init and _fini thus no locking is needed. */
static	IDP	udp_g_nd;	/* Points to table of UDP ND variables. */

/* MIB-2 stuff for SNMP */
static	mib2_udp_t	udp_mib;	/* SNMP fixed size info */


	/* Default structure copied into T_INFO_ACK messages */
static	struct T_info_ack udp_g_t_info_ack = {
	T_INFO_ACK,
	(64 * 1024) - (UDPH_SIZE + 20),	/* TSDU_size.  max ip less headers */
	T_INVALID,	/* ETSU_size.  udp does not support expedited data. */
	T_INVALID,	/* CDATA_size. udp does not support connect data. */
	T_INVALID,	/* DDATA_size. udp does not support disconnect data. */
	sizeof (ipa_t),	/* ADDR_size. */
	0,		/* OPT_size - not initialized here */
	(64 * 1024) - (UDPH_SIZE + 20),	/* TIDU_size.  max ip less headers */
	T_CLTS,		/* SERV_type.  udp supports connection-less. */
	TS_UNBND,	/* CURRENT_state.  This is set from udp_state. */
	(XPG4_1|SENDZERO) /* PROVIDER_flag */
};

/* largest UDP port number */
#define	UDP_MAX_PORT	65535

/*
 * Table of ND variables supported by udp.  These are loaded into udp_g_nd
 * in udp_open.
 * All of these are alterable, within the min/max values given, at run time.
 */
/* BEGIN CSTYLED */
static	udpparam_t	udp_param_arr[] = {
	/*min	max		value		name */
	{ 0L,	256,		32,		"udp_wroff_extra" },
	{ 1L,	255,		255,		"udp_def_ttl" },
	{ 1024,	(32 * 1024),	1024,		"udp_smallest_nonpriv_port" },
	{ 0,	1,		0,		"udp_trust_optlen" },
	{ 0,	1,		1,		"udp_do_checksum" },
	{ 1024,	UDP_MAX_PORT,	(32 * 1024),	"udp_smallest_anon_port" },
	{ 1024,	UDP_MAX_PORT,	UDP_MAX_PORT,	"udp_largest_anon_port" },
	{ 4096,	65536,		8192,		"udp_xmit_hiwat"},
	{ 0,	65536,		1024,		"udp_xmit_lowat"},
	{ 4096,	65536,		8192,		"udp_recv_hiwat"},
	{ 65536, 1024*1024*1024, 256*1024,	"udp_max_buf"},
};
/* END CSTYLED */
#define	udp_wroff_extra			udp_param_arr[0].udp_param_value
#define	udp_g_def_ttl			udp_param_arr[1].udp_param_value
#define	udp_smallest_nonpriv_port	udp_param_arr[2].udp_param_value
#define	udp_trust_optlen		udp_param_arr[3].udp_param_value
#define	udp_g_do_checksum		udp_param_arr[4].udp_param_value
#define	udp_smallest_anon_port		udp_param_arr[5].udp_param_value
#define	udp_largest_anon_port		udp_param_arr[6].udp_param_value
#define	udp_xmit_hiwat			udp_param_arr[7].udp_param_value
#define	udp_xmit_lowat			udp_param_arr[8].udp_param_value
#define	udp_recv_hiwat			udp_param_arr[9].udp_param_value
#define	udp_max_buf			udp_param_arr[10].udp_param_value

/*
 * This routine is called to handle each O_T_BIND_REQ/T_BIND_REQ message
 * passed to udp_wput.
 * It associates a port number and local address with the stream.
 * The O_T_BIND_REQ/T_BIND_REQ is passed downstream to ip with the UDP
 * protocol type (IPPROTO_UDP) placed in the message following the address.
 * A T_BIND_ACK message is passed upstream when ip acknowledges the request.
 * (Called as writer.)
 */
static void
udp_bind(queue_t *q, mblk_t *mp)
{
	ipa_t		*ipa;
	mblk_t		*mp1;
	in_port_t	port;		/* Host byte order */
	in_port_t	requested_port;	/* Host byte order */
	struct	T_bind_req *tbr;
	udp_t		*udp;
	int		count;
	ipaddr_t	src;
	int		bind_to_req_port_only;

	udp = (udp_t *)q->q_ptr;
	if ((mp->b_wptr - mp->b_rptr) < sizeof (*tbr)) {
		(void) mi_strlog(q, 1, SL_ERROR|SL_TRACE,
		    "udp_bind: bad req, len %ld", mp->b_wptr - mp->b_rptr);
		udp_err_ack(q, mp, TBADADDR, 0);
		return;
	}
	if (udp->udp_state != TS_UNBND) {
		(void) mi_strlog(q, 1, SL_ERROR|SL_TRACE,
		    "udp_bind: bad state, %u", udp->udp_state);
		udp_err_ack(q, mp, TOUTSTATE, 0);
		return;
	}
	/*
	 * Reallocate the message to make sure we have enough room for an
	 * address and the protocol type.
	 */
	mp1 = reallocb(mp, sizeof (struct T_bind_ack) + sizeof (ipa_t) + 1, 1);
	if (!mp1) {
		udp_err_ack(q, mp, TSYSERR, ENOMEM);
		return;
	}

	mp = mp1;
	tbr = (struct T_bind_req *)mp->b_rptr;
	switch (tbr->ADDR_length) {
	case 0:			/* Request for a generic port */
		tbr->ADDR_offset = sizeof (struct T_bind_req);
		tbr->ADDR_length = sizeof (ipa_t);
		ipa = (ipa_t *)&tbr[1];
		bzero((char *)ipa, sizeof (ipa_t));
		ipa->ip_family = AF_INET;
		mp->b_wptr = (u_char *)&ipa[1];
		port = 0;
		break;
	case sizeof (ipa_t):	/* Complete IP address */
		ipa = (ipa_t *)mi_offset_param(mp, tbr->ADDR_offset,
		    sizeof (ipa_t));
		if (!ipa) {
			udp_err_ack(q, mp, TSYSERR, EINVAL);
			return;
		}
		port = BE16_TO_U16(ipa->ip_port);
		break;
	default:		/* Invalid request */
		(void) mi_strlog(q, 1, SL_ERROR|SL_TRACE,
		    "udp_bind: bad ADDR_length %u", tbr->ADDR_length);
		udp_err_ack(q, mp, TBADADDR, 0);
		return;
	}

	requested_port = port;

	if (requested_port == 0 || tbr->PRIM_type == O_T_BIND_REQ)
		bind_to_req_port_only = 0;
	else			/* T_BIND_REQ and requested_port != 0 */
		bind_to_req_port_only = 1;

	mutex_enter(&udp_g_lock);

	if (requested_port == 0) {
		/*
		 * If the application passed in zero for the port number, it
		 * doesn't care which port number we bind to. Get one in the
		 * valid range.
		 */
		port = udp_update_next_port(udp_g_next_port_to_try);
	} else {

		/*
		 * If the port is in the well-known privileged range,
		 * make sure the stream was opened by superuser.
		 */
		if (!udp->udp_priv_stream) {
			int i;
			int priv = 0;

			if (port < udp_smallest_nonpriv_port) {
				priv = 1;
			} else {
				for (i = 0; i < udp_g_num_epriv_ports; i++) {
					if (port == udp_g_epriv_ports[i]) {
						priv = 1;
						break;
					}
				}
			}
			if (priv) {
				mutex_exit(&udp_g_lock);
				udp_err_ack(q, mp, TACCES, 0);
				return;
			}
		}
	}
	bcopy((char *)ipa->ip_addr, &src, IP_ADDR_LEN);

	/*
	 * If udp_reuseaddr is not set, then we have to make sure that
	 * the IP address and port number the application requested
	 * (or we selected for the application) is not being used by
	 * another stream.  If another stream is already using the
	 * requested IP address and port, the behavior depends on
	 * "bind_to_req_port_only". If set the bind fails; otherwise we
	 * search for any an unused port to bind to the the stream.
	 *
	 * As per the BSD semantics, as modified by the Deering multicast
	 * changes, if udp_reuseaddr is set, then we allow multiple binds
	 * to the same port independent of the local IP address.
	 *
	 * This is slightly different than in SunOS 4.X which did not
	 * support IP multicast. Note that the change implemented by the
	 * Deering multicast code effects all binds - not only binding
	 * to IP multicast addresses.
	 *
	 * Note that when binding to port zero we ignore SO_REUSEADDR in
	 * order to guarantee a unique port.
	 */
	count = 0;
	if (!udp->udp_reuseaddr || requested_port == 0) {
		for (;;) {
			udp_t		*udp1;
			ipaddr_t	src1;

			/*
			 * Walk through the list of open udp streams looking
			 * for another stream bound to this IP address
			 * and port number.
			 */
			for (udp1 = (udp_t *)mi_first_ptr(&udp_g_head);
			    udp1 != NULL;
			    udp1 = (udp_t *)mi_next_ptr(&udp_g_head,
							(IDP)udp1)) {
				if (udp1->udp_port != htons(port))
					continue;

				src1 = udp1->udp_bound_src;
				/*
				 * No socket option SO_REUSEADDR.
				 *
				 * If existing port is bound to a
				 * non-wildcard IP address and
				 * the requesting stream is bound to
				 * a distinct different IP addresses
				 * (non-wildcard, also), keep going.
				 */
				if (src != INADDR_ANY &&
				    src1 != INADDR_ANY && src1 != src)
					continue;
				break;
			}

			if (!udp1) {
				/*
				 * No other stream has this IP address
				 * and port number. We can use it.
				 */
				break;
			}

			if (bind_to_req_port_only) {
				/*
				 * We get here only when requested port
				 * is bound (and only first  of the for()
				 * loop iteration).
				 *
				 * The semantics of this bind request
				 * require it to fail so we return from
				 * the routine (and exit the loop).
				 *
				 */
				mutex_exit(&udp_g_lock);
				udp_err_ack(q, mp, TADDRBUSY, 0);
				return;
			}

			if ((count == 0) && (requested_port != 0)) {
				/*
				 * If the application wants us to find
				 * a port, get one to start with. Set
				 * requested_port to 0, so that we will
				 * update udp_g_next_port_to_try below.
				 */
				port = udp_update_next_port
				    (udp_g_next_port_to_try);
				requested_port = 0;
			} else {
				port = udp_update_next_port(port + 1);
			}

			if (++count >= (udp_largest_anon_port -
			    udp_smallest_anon_port + 1)) {
				/*
				 * We've tried every possible port number and
				 * there are none available, so send an error
				 * to the user.
				 */
				mutex_exit(&udp_g_lock);
				udp_err_ack(q, mp, TNOADDR, 0);
				return;
			}
		}
	}

	/*
	 * Copy the source address into our udp structure.  This address
	 * may still be zero; if so, ip will fill in the correct address
	 * each time an outbound packet is passed to it.
	 * If we are binding to a broadcast or multicast address udp_rput
	 * will clear the source address when it receives the T_BIND_ACK.
	 */
	udp->udp_ipha.ipha_src = udp->udp_bound_src = udp->udp_src = src;
	udp->udp_port = htons(port);

	/*
	 * Now reset the the next anonymous port if the application requested
	 * an anonymous port, or we handed out the next anonymous port.
	 */
	if (requested_port == 0) {
		udp_g_next_port_to_try = port + 1;
	}

	/* Initialize the O_T_BIND_REQ/T_BIND_REQ for ip. */
	bcopy((char *)&udp->udp_port, (char *)ipa->ip_port,
	    sizeof (udp->udp_port));
	udp->udp_family = ipa->ip_family;
	udp->udp_state = TS_IDLE;

	mutex_exit(&udp_g_lock);
	/* Pass the protocol number in the message following the address. */
	*mp->b_wptr++ = IPPROTO_UDP;
	if (src != INADDR_ANY) {
		/*
		 * Append a request for an IRE if src not 0 (INADDR_ANY)
		 */
		mp->b_cont = allocb(sizeof (ire_t), BPRI_HI);
		if (!mp->b_cont) {
			udp_err_ack(q, mp, TSYSERR, ENOMEM);
			return;
		}
		mp->b_cont->b_wptr += sizeof (ire_t);
		mp->b_cont->b_datap->db_type = IRE_DB_REQ_TYPE;
	}
	putnext(q, mp);
}

/*
 * This routine handles each T_CONN_REQ message passed to udp.  It
 * associates a default destination address with the stream.
 * A default IP header is created and placed into udp_iphc.
 * This header is prepended to subsequent M_DATA messages.
 *
 * This routine sends down a T_BIND_REQ to IP with the following mblks:
 *	T_BIND_REQ	- specifying local and remote address/port
 *	IRE_DB_REQ_TYPE	- to get an IRE back containing ire_type and src
 *	T_OK_ACK	- for the T_CONN_REQ
 *	T_CONN_CON	- to keep the TPI user happy
 *
 * The connect completes in udp_rput.
 * When a T_BIND_ACK is received information is extracted from the IRE
 * and the two appended messages are sent to the TPI user.
 * Should udp_rput receive T_ERROR_ACK for the T_BIND_REQ it will convert
 * it to an error ack for the appropriate primitive.
 */
static void
udp_connect(queue_t *q, mblk_t *mp)
{
	ipa_t	*ipa;
	ipha_t	*ipha;
	struct T_conn_req	*tcr;
	udp_t	*udp, *udp1;
	udpha_t	*udpha;
	mblk_t	*hdr_mp;
	mblk_t	*mp1, *mp2;

	udp = (udp_t *)q->q_ptr;
	tcr = (struct T_conn_req *)mp->b_rptr;

	/* Make sure the request contains an IP address. */
	if (tcr->DEST_length != sizeof (ipa_t) ||
	    (mp->b_wptr - mp->b_rptr <
		sizeof (struct T_conn_req) + sizeof (ipa_t))) {
		udp_err_ack(q, mp, TBADADDR, 0);
		return;
	}

	if (udp->udp_state == TS_DATA_XFER) {
		/* Already connected - clear out state */
		udp->udp_src = udp->udp_bound_src;
		udp->udp_state = TS_IDLE;
	}

	if (tcr->OPT_length != 0) {
		udp_err_ack(q, mp, TBADOPT, 0);
		return;
	}

	/*
	 * Create a default IP header with no IP options.
	 */
	ipha = &udp->udp_ipha;
#ifdef	_BIG_ENDIAN
	/* Set version, header length, and tos */
	*(uint16_t *)&ipha->ipha_version_and_hdr_length =
	    ((((IP_VERSION << 4) |
		(IP_SIMPLE_HDR_LENGTH>>2)) << 8) |
		udp->udp_type_of_service);
	/* Set ttl and protocol */
	*(uint16_t *)&ipha->ipha_ttl = (udp->udp_ttl << 8) | IPPROTO_UDP;
#else
	/* Set version, header length, and tos */
	*(uint16_t *)&ipha->ipha_version_and_hdr_length =
	    ((udp->udp_type_of_service << 8) |
		((IP_VERSION << 4) | (IP_SIMPLE_HDR_LENGTH>>2)));
	/* Set ttl and protocol */
	*(uint16_t *)&ipha->ipha_ttl = (IPPROTO_UDP << 8) | udp->udp_ttl;
#endif
	udp->udp_hdr_length = IP_SIMPLE_HDR_LENGTH + UDPH_SIZE;
	udp->udp_udpha = (udpha_t *)&udp->udp_iphc[IP_SIMPLE_HDR_LENGTH];

	/* Now, finish initializing the IP and UDP headers. */
	ipa = (ipa_t *)&mp->b_rptr[tcr->DEST_offset];
	ipha->ipha_fragment_offset_and_flags = 0;
	ipha->ipha_ident = 0;
	/*
	 * Copy the source address already bound to the stream.
	 * This may still be zero in which case ip will fill it in.
	 */
	ipha->ipha_src = udp->udp_src;

	/*
	 * Copy the destination address from the T_CONN_REQ message.
	 * Translate 0 to INADDR_LOOPBACK.
	 * Update the T_CONN_REQ since it is used to generate the T_CONN_CON.
	 */
	bcopy((char *)ipa->ip_addr, (char *)&ipha->ipha_dst, IP_ADDR_LEN);
	if (ipha->ipha_dst == INADDR_ANY) {
		ipha->ipha_dst = htonl(INADDR_LOOPBACK);
		bcopy((char *)&ipha->ipha_dst, (char *)ipa->ip_addr,
			IP_ADDR_LEN);
	}

	udpha = udp->udp_udpha;
	udpha->uha_src_port = udp->udp_port;
#define	udph	((udph_t *)udpha)
	udph->uh_dst_port[0] = ipa->ip_port[0];
	udph->uh_dst_port[1] = ipa->ip_port[1];
#undef	udph
	udpha->uha_checksum = 0;

	if (udpha->uha_dst_port == 0) {
		udp_err_ack(q, mp, TBADADDR, 0);
		return;
	}

	/*
	 * Verify that the src/port/dst/port is unique for all
	 * connections in TS_DATA_XFER
	 */
	mutex_enter(&udp_g_lock);
	udp1 = (udp_t *)mi_first_ptr(&udp_g_head);
	for (; udp1; udp1 = (udp_t *)mi_next_ptr(&udp_g_head, (IDP)udp1)) {
		if (udp1->udp_state != TS_DATA_XFER)
			continue;
		if (udp->udp_port == udp1->udp_port &&
		    udp->udp_src == udp1->udp_src &&
		    udp->udp_ipha.ipha_dst == udp1->udp_ipha.ipha_dst &&
		    udp->udp_udpha->uha_dst_port  ==
		    udp1->udp_udpha->uha_dst_port) {
			mutex_exit(&udp_g_lock);
			udp_err_ack(q, mp, TBADADDR, 0);
			return;
		}
	}
	mutex_exit(&udp_g_lock);

	/*
	 * Send down bind to IP to verify that there is a route
	 * and to determine the source address.
	 * This will come back as T_BIND_ACK with an IRE_DB_TYPE in rput.
	 */
	mp1 = udp_ip_bind_mp(udp, O_T_BIND_REQ, sizeof (ipa_conn_t));
	if (mp1 == NULL) {
		udp_err_ack(q, mp, TSYSERR, ENOMEM);
		return;
	}

	/*
	 * We also have to send a connection confirmation to
	 * keep TLI happy. Prepare it for udp_rput.
	 */
	mp2 = mi_tpi_conn_con(NULL, (char *)ipa, sizeof (*ipa), NULL, 0);
	if (mp2 == NULL) {
		freemsg(mp1);
		udp_err_ack(q, mp, TSYSERR, ENOMEM);
		return;
	}

	hdr_mp = copymsg(mp);
	if (hdr_mp == NULL) {
		freemsg(mp1);
		freemsg(mp2);
		udp_err_ack(q, mp, TSYSERR, ENOMEM);
		return;
	}

	mp = mi_tpi_ok_ack_alloc(mp);
	if (mp == NULL) {
		/* Unable to reuse the T_CONN_REQ for the ack. */
		freemsg(mp2);
		freemsg(hdr_mp);
		udp_err_ack_prim(q, mp1, T_CONN_REQ, TSYSERR, ENOMEM);
		return;
	}

	tcr = (struct T_conn_req *)hdr_mp->b_rptr;
	tcr->PRIM_type = T_UNITDATA_REQ;
	if (udp->udp_hdr_mp != NULL)
		freemsg(udp->udp_hdr_mp);
	udp->udp_hdr_mp = hdr_mp;

	udp->udp_state = TS_DATA_XFER;

	/* Hang onto the T_OK_ACK and T_CONN_CON for later. */
	linkb(mp1, mp);
	linkb(mp1, mp2);

	putnext(q, mp1);
}

/* This is the close routine for udp.  It frees the per-stream data. */
static int
udp_close(queue_t *q)
{
	udp_t	*udp = (udp_t *)q->q_ptr;

	TRACE_1(TR_FAC_UDP, TR_UDP_CLOSE,
		"udp_close: q %p", q);

	qprocsoff(q);

	mutex_enter(&udp_g_lock);
	/* Unlink the udp structure and release the minor device number. */
	mi_close_unlink(&udp_g_head, (IDP)udp);
	mutex_exit(&udp_g_lock);

	/* If there are any options associated with the stream, free them. */
	if (udp->udp_ip_snd_options)
		mi_free((char *)udp->udp_ip_snd_options);

	if (udp->udp_ip_rcv_options)
		mi_free((char *)udp->udp_ip_rcv_options);

	if (udp->udp_hdr_mp != NULL)
		freemsg(udp->udp_hdr_mp);

	/* Free the data structure */
	mi_close_free((IDP)udp);
	q->q_ptr = WR(q)->q_ptr = NULL;
	return (0);
}

/*
 * This routine handles each T_DISCON_REQ message passed to udp
 * as an indicating that UDP is no longer connected. This results
 * in sending a T_BIND_REQ to IP to restore the binding to just
 * the local address/port.
 *
 * This routine sends down a T_BIND_REQ to IP with the following mblks:
 *	T_BIND_REQ	- specifying just the local address/port
 *	T_OK_ACK	- for the T_DISCON_REQ
 *
 * The disconnect completes in udp_rput.
 * When a T_BIND_ACK is received the appended T_OK_ACK is sent to the TPI user.
 * Should udp_rput receive T_ERROR_ACK for the T_BIND_REQ it will convert
 * it to an error ack for the appropriate primitive.
 */
static void
udp_disconnect(queue_t *q, mblk_t *mp)
{
	udp_t	*udp;
	mblk_t	*mp1;

	udp = (udp_t *)q->q_ptr;

	if (udp->udp_state != TS_DATA_XFER) {
		(void) mi_strlog(q, 1, SL_ERROR|SL_TRACE,
		    "udp_disconnect: bad state, %u", udp->udp_state);
		udp_err_ack(q, mp, TOUTSTATE, 0);
		return;
	}
	/*
	 * Send down bind to IP to remove the full binding and revert
	 * to the local address binding.
	 */
	mp1 = udp_ip_bind_mp(udp, O_T_BIND_REQ, sizeof (ipa_t));
	if (mp1 == NULL) {
		udp_err_ack(q, mp, TSYSERR, ENOMEM);
		return;
	}
	mp = mi_tpi_ok_ack_alloc(mp);
	if (mp == NULL) {
		/* Unable to reuse the T_DISCON_REQ for the ack. */
		udp_err_ack_prim(q, mp1, T_DISCON_REQ, TSYSERR, ENOMEM);
		return;
	}

	mutex_enter(&udp_g_lock);
	if (udp->udp_hdr_mp != NULL) {
		freemsg(udp->udp_hdr_mp);
		udp->udp_hdr_mp = NULL;
	}

	udp->udp_ipha.ipha_src = udp->udp_src = udp->udp_bound_src;
	udp->udp_state = TS_IDLE;
	udp->udp_discon_pending = 1;
	mutex_exit(&udp_g_lock);

	/* Append the T_OK_ACK to the T_BIND_REQ for udp_rput */
	linkb(mp1, mp);
	putnext(q, mp1);
}

/* This routine creates a T_ERROR_ACK message and passes it upstream. */
static void
udp_err_ack(queue_t *q, mblk_t *mp, t_scalar_t t_error, int sys_error)
{
	if ((mp = mi_tpi_err_ack_alloc(mp, t_error, sys_error)) != NULL)
		qreply(q, mp);
}

/* Shorthand to generate and send TPI error acks to our client */
static void
udp_err_ack_prim(queue_t *q, mblk_t *mp, int primitive, t_scalar_t t_error,
    int sys_error)
{
	struct T_error_ack	*teackp;

	if ((mp = tpi_ack_alloc(mp, sizeof (struct T_error_ack),
	    M_PCPROTO, T_ERROR_ACK)) != NULL) {
		teackp = (struct T_error_ack *)mp->b_rptr;
		teackp->ERROR_prim = primitive;
		teackp->TLI_error = t_error;
		teackp->UNIX_error = sys_error;
		qreply(q, mp);
	}
}

/*ARGSUSED*/
static int
udp_extra_priv_ports_get(queue_t *q, mblk_t *mp, caddr_t cp)
{
	int i;

	for (i = 0; i < udp_g_num_epriv_ports; i++) {
		if (udp_g_epriv_ports[i] != 0)
			(void) mi_mpprintf(mp, "%d ", udp_g_epriv_ports[i]);
	}
	return (0);
}

/*
 * The callers holds udp_g_lock to prevent multiple
 * threads from changing udp_g_epriv_ports at the same time.
 */
/* ARGSUSED */
static int
udp_extra_priv_ports_add(queue_t *q, mblk_t *mp, char *value, caddr_t cp)
{
	char	*end;
	int	new_value;
	int	i;

	ASSERT(MUTEX_HELD(&udp_g_lock));
	/* Convert the value from a string into a 32-bit integer. */
	new_value = (int)mi_strtol(value, &end, 10);
	/*
	 * Fail the request if the new value does not lie within the
	 * port number limits.
	 */
	if (end == value || new_value <= 0 || new_value >= 65536)
		return (EINVAL);

	/* Check if the value is already in the list */
	for (i = 0; i < udp_g_num_epriv_ports; i++) {
		if (new_value == udp_g_epriv_ports[i])
			return (EEXIST);
	}
	/* Find an empty slot */
	for (i = 0; i < udp_g_num_epriv_ports; i++) {
		if (udp_g_epriv_ports[i] == 0)
			break;
	}
	if (i == udp_g_num_epriv_ports)
		return (EOVERFLOW);

	/* Set the new value */
	udp_g_epriv_ports[i] = (in_port_t)new_value;
	return (0);
}

/*
 * The callers holds udp_g_lock to prevent multiple
 * threads from changing udp_g_epriv_ports at the same time.
 */
/* ARGSUSED */
static int
udp_extra_priv_ports_del(queue_t *q, mblk_t *mp, char *value, caddr_t cp)
{
	char	*end;
	int	new_value;
	int	i;

	ASSERT(MUTEX_HELD(&udp_g_lock));
	/* Convert the value from a string into a 32-bit integer. */
	new_value = (int)mi_strtol(value, &end, 10);
	/*
	 * Fail the request if the new value does not lie within the
	 * port number limits.
	 */
	if (end == value || new_value <= 0 || new_value >= 65536)
		return (EINVAL);

	/* Check that the value is already in the list */
	for (i = 0; i < udp_g_num_epriv_ports; i++) {
		if (udp_g_epriv_ports[i] == new_value)
			break;
	}
	if (i == udp_g_num_epriv_ports)
		return (ESRCH);

	/* Clear the value */
	udp_g_epriv_ports[i] = 0;
	return (0);
}

/*
 * udp_icmp_error is called by udp_rput to process ICMP
 * messages passed up by IP.
 * Generates the appropriate T_UDERROR_IND.
 */
static void
udp_icmp_error(queue_t *q, mblk_t *mp)
{
	icmph_t *icmph;
	ipha_t	*ipha;
	int	iph_hdr_length;
	udpha_t	*udpha;
	ipa_t	ipaddr;
	mblk_t	*mp1;
	int	error = 0;
	udp_t	*udp = (udp_t *)q->q_ptr;

	ipha = (ipha_t *)mp->b_rptr;
	iph_hdr_length = IPH_HDR_LENGTH(ipha);
	icmph = (icmph_t *)&mp->b_rptr[iph_hdr_length];
	ipha = (ipha_t *)&icmph[1];
	iph_hdr_length = IPH_HDR_LENGTH(ipha);
	udpha = (udpha_t *)((char *)ipha + iph_hdr_length);

	switch (icmph->icmph_type) {
	case ICMP_DEST_UNREACHABLE:
		switch (icmph->icmph_code) {
		case ICMP_FRAGMENTATION_NEEDED:
			/*
			 * XXX do something with MTU in UDP?
			 */
			break;
		case ICMP_PORT_UNREACHABLE:
		case ICMP_PROTOCOL_UNREACHABLE:
			error = ECONNREFUSED;
			break;
		default:
			break;
		}
		break;
	}
	if (error == 0) {
		freemsg(mp);
		return;
	}
	/*
	 * Can not deliver T_UDERROR_IND except when upper layer or
	 * the application has asked for them.
	 */
	if (!udp->udp_dgram_errind) {
		freemsg(mp);
		return;
	}

	bzero((char *)&ipaddr, sizeof (ipaddr));

	ipaddr.ip_family = AF_INET;
	bcopy((char *)&ipha->ipha_dst,
	    (char *)ipaddr.ip_addr,
	    sizeof (ipaddr.ip_addr));
	bcopy((char *)&udpha->uha_dst_port,
	    (char *)ipaddr.ip_port,
	    sizeof (ipaddr.ip_port));
	mp1 = mi_tpi_uderror_ind((char *)&ipaddr, sizeof (ipaddr), NULL, 0,
	    error);
	if (mp1)
		putnext(q, mp1);
	freemsg(mp);
}

/*
 * This routine responds to T_ADDR_REQ messages.  It is called by udp_wput.
 * The local address is filled in if endpoint is bound. The remote address
 * is always null. (The concept of connected CLTS sockets is alien to TPI
 * and we do not currently implement it with UDP.
 */
static void
udp_addr_req(queue_t *q, mblk_t *mp)
{
	udp_t	*udp = (udp_t *)q->q_ptr;
	ipa_t	*ipa;
	mblk_t	*ackmp;
	struct T_addr_ack *taa;

	ackmp = reallocb(mp, sizeof (struct T_addr_ack) + sizeof (ipa_t), 1);
	if (! ackmp) {
		udp_err_ack(q, mp, TSYSERR, ENOMEM);
		return;
	}
	taa = (struct T_addr_ack *)ackmp->b_rptr;

	bzero((char *)taa, sizeof (struct T_addr_ack));
	ackmp->b_wptr = (u_char *)&taa[1];

	taa->PRIM_type = T_ADDR_ACK;
	ackmp->b_datap->db_type = M_PCPROTO;
	/*
	 * Note: Following code assumes 32 bit alignment of basic
	 * data structures like ipa_t and struct T_addr_ack.
	 */
	if (udp->udp_state == TS_IDLE) {
		/*
		 * Fill in local address
		 */
		taa->LOCADDR_length = sizeof (ipa_t);
		taa->LOCADDR_offset = sizeof (*taa);

		ipa = (ipa_t *)&taa[1];
		/* Fill zeroes and then initialize non-zero fields */
		bzero((char *)ipa, sizeof (ipa_t));

		ipa->ip_family = AF_INET;

		bcopy((char *)&udp->udp_src, (char *)ipa->ip_addr,
		    sizeof (ipa->ip_addr));
		bcopy((char *)&udp->udp_port, (char *)ipa->ip_port,
		    sizeof (ipa->ip_port));

		ackmp->b_wptr = (u_char *)&ipa[1];
		ASSERT(ackmp->b_wptr <= ackmp->b_datap->db_lim);
	}
	qreply(q, ackmp);
}

static void
udp_copy_info(struct T_info_ack *tap, udp_t *udp)
{
	*tap = udp_g_t_info_ack;
	tap->CURRENT_state = udp->udp_state;
	tap->OPT_size = udp_max_optbuf_len;
}

/*
 * This routine responds to T_CAPABILITY_REQ messages.  It is called by
 * udp_wput.  Much of the T_CAPABILITY_ACK information is copied from
 * udp_g_t_info_ack.  The current state of the stream is copied from
 * udp_state.
 */
static void
udp_capability_req(queue_t *q, mblk_t *mp)
{
	udp_t			*udp = (udp_t *)q->q_ptr;
	t_uscalar_t		cap_bits1;
	struct T_capability_ack	*tcap;

	cap_bits1 = ((struct T_capability_req *)mp->b_rptr)->CAP_bits1;

	mp = tpi_ack_alloc(mp, sizeof (struct T_capability_ack),
	    mp->b_datap->db_type, T_CAPABILITY_ACK);
	if (!mp)
		return;

	tcap = (struct T_capability_ack *)mp->b_rptr;
	tcap->CAP_bits1 = 0;

	if (cap_bits1 & TC1_INFO) {
		udp_copy_info(&tcap->INFO_ack, udp);
		tcap->CAP_bits1 |= TC1_INFO;
	}

	qreply(q, mp);
}

/*
 * This routine responds to T_INFO_REQ messages.  It is called by udp_wput.
 * Most of the T_INFO_ACK information is copied from udp_g_t_info_ack.
 * The current state of the stream is copied from udp_state.
 */
static void
udp_info_req(queue_t *q, mblk_t *mp)
{
	udp_t	*udp = (udp_t *)q->q_ptr;

	mp = tpi_ack_alloc(mp, sizeof (struct T_info_ack), M_PCPROTO,
	    T_INFO_ACK);
	if (!mp)
		return;
	udp_copy_info((struct T_info_ack *)mp->b_rptr, udp);
	qreply(q, mp);
}

/*
 * There are three types of binds that IP recognizes.  If we send
 * down a 0 length address, IP will send us packets for which it
 * has no more specific target than "some UDP port".  If we send
 * down a 4 byte address, IP will verify that the address given
 * is a valid local address.  If we send down a full 12 byte address,
 * IP validates both addresses, and then begins to send us only those
 * packets that match completely.  IP will also fill in the IRE
 * request mblk with information regarding our peer.  In all three
 * cases, we notify IP of our protocol type by appending a single
 * protocol byte to the bind request.
 */
static mblk_t *
udp_ip_bind_mp(udp_t *udp, t_scalar_t bind_prim, t_scalar_t addr_length)
{
	char	*cp;
	mblk_t	*mp;
	struct T_bind_req *tbr;
	ipa_conn_t	*ac;
	ipa_t		*ipa;

	ASSERT(bind_prim == O_T_BIND_REQ || bind_prim == T_BIND_REQ);

	mp = allocb(sizeof (*tbr) + addr_length + 1, BPRI_HI);
	if (!mp)
		return (mp);
	mp->b_datap->db_type = M_PROTO;
	tbr = (struct T_bind_req *)mp->b_rptr;
	tbr->PRIM_type = bind_prim;
	tbr->ADDR_offset = sizeof (*tbr);
	tbr->CONIND_number = 0;
	tbr->ADDR_length = addr_length;
	cp = (char *)&tbr[1];
	switch (addr_length) {
	case sizeof (ipa_conn_t):
		/* Append a request for an IRE */
		mp->b_cont = allocb(sizeof (ire_t), BPRI_HI);
		if (!mp->b_cont) {
			freemsg(mp);
			return (nilp(mblk_t));
		}
		mp->b_cont->b_wptr += sizeof (ire_t);
		mp->b_cont->b_datap->db_type = IRE_DB_REQ_TYPE;

		ASSERT(udp->udp_udpha != NULL);
		/* cp known to be 32 bit aligned */
		ac = (ipa_conn_t *)cp;
		ac->ac_laddr = udp->udp_src;
		ac->ac_faddr = udp->udp_ipha.ipha_dst;
		ac->ac_fport = udp->udp_udpha->uha_dst_port;
		ac->ac_lport = udp->udp_port;
		break;

	case sizeof (ipa_t):
		ipa = (ipa_t *)cp;

		bzero((char *)ipa, sizeof (*ipa));
		ipa->ip_family = AF_INET;
		*(ipaddr_t *)ipa->ip_addr = udp->udp_bound_src;
		*(in_port_t *)ipa->ip_port = udp->udp_port;
		break;

	case IP_ADDR_LEN:
		*(ipaddr_t *)cp = udp->udp_src;
		break;
	}
	cp[addr_length] = (char)IPPROTO_UDP;
	mp->b_wptr = (u_char *)&cp[addr_length + 1];
	return (mp);
}

/*
 * This is the open routine for udp.  It allocates a udp_t structure for
 * the stream and, on the first open of the module, creates an ND table.
 */
static int
udp_open(queue_t *q, dev_t *devp, int flag, int sflag, cred_t *credp)
{
	int	err;
	boolean_t	privy = drv_priv(credp) == 0;
	udp_t	*udp;

	TRACE_1(TR_FAC_UDP, TR_UDP_OPEN, "udp_open: q %p", q);

	/*
	 * Defer the qprocson until everything is initialized since
	 * we are D_MTPERQ and after qprocson the rput routine can
	 * run.
	 */

	/* If the stream is already open, return immediately. */
	if ((udp = (udp_t *)q->q_ptr) != 0) {
		if (udp->udp_priv_stream && !privy)
			return (EPERM);
		return (0);
	}

	/* If this is not a push of udp as a module, fail. */
	if (sflag != MODOPEN)
		return (EINVAL);

	/*
	 * Create and initialize a udp_t structure for this stream.
	 */
	udp = (udp_t *)mi_open_alloc(sizeof (udp_t));
	if (udp == NULL)
		return (ENOMEM);

	/* Set the initial state of the stream and the privilege status. */
	q->q_ptr = WR(q)->q_ptr = udp;
	udp->udp_state = TS_UNBND;
	udp->udp_hdr_length = IP_SIMPLE_HDR_LENGTH + UDPH_SIZE;

	/*
	 * The receive hiwat is only looked at on the stream head queue.
	 * Store in q_hiwat in order to return on SO_RCVBUF getsockopts.
	 */
	q->q_hiwat = udp_recv_hiwat;

	udp->udp_multicast_ttl = IP_DEFAULT_MULTICAST_TTL;
	udp->udp_multicast_loop = IP_DEFAULT_MULTICAST_LOOP;
	udp->udp_ttl = udp_g_def_ttl;
	udp->udp_type_of_service = 0;	/* XXX should have a global default */
	if (privy)
		udp->udp_priv_stream = 1;

	/*
	 * Acquire the lock and link it into the list of open streams.
	 */
	mutex_enter(&udp_g_lock);
	err = mi_open_link(&udp_g_head, (IDP)udp, devp, flag, sflag, credp);
	mutex_exit(&udp_g_lock);
	if (err) {
		mi_close_free((IDP)udp);
		q->q_ptr = WR(q)->q_ptr = NULL;
		return (err);
	}
	qprocson(q);

	/*
	 * The transmit hiwat/lowat is only looked at on IP's queue.
	 * Store in q_hiwat in order to return on SO_SNDBUF
	 * getsockopts.
	 */
	WR(q)->q_hiwat = udp_xmit_hiwat;
	WR(q)->q_next->q_hiwat = WR(q)->q_hiwat;
	WR(q)->q_lowat = udp_xmit_lowat;
	WR(q)->q_next->q_lowat = WR(q)->q_lowat;

	(void) mi_set_sth_wroff(q, udp->udp_hdr_length +
	    udp->udp_ip_snd_options_len + udp_wroff_extra);
	(void) mi_set_sth_hiwat(q, q->q_hiwat);
	return (0);
}

/*
 * Which UDP options OK to set through T_UNITDATA_REQ...
 */

/* ARGSUSED */
static boolean_t
udp_allow_udropt_set(t_scalar_t level, t_scalar_t name)
{

	return (true);
}



/*
 * This routine gets default values of certain options whose default
 * values are maintained by protcol specific code
 */

/* ARGSUSED */
int
udp_opt_default(queue_t	*q, t_scalar_t level, t_scalar_t name, u_char *ptr)
{
	switch (level) {
	case IPPROTO_IP:
		switch (name) {
		case IP_MULTICAST_TTL:
			/* XXX - ndd variable someday ? */
			*ptr = (u_char) IP_DEFAULT_MULTICAST_LOOP;
			return (sizeof (u_char));
		case IP_MULTICAST_LOOP:
			/* XXX - ndd variable someday ? */
			*ptr = (u_char) IP_DEFAULT_MULTICAST_LOOP;
			return (sizeof (u_char));
		default:
			return (-1);
		}
	default:
		return (-1);
	}
	/* NOTREACHED */
}

/*
 * This routine retrieves the current status of socket options.
 * It returns the size of the option retrieved.
 */
int
udp_opt_get(queue_t *q, t_scalar_t level, t_scalar_t name, u_char *ptr)
{
	int	*i1 = (int *)ptr;
	udp_t	*udp = (udp_t *)q->q_ptr;

	switch (level) {
	case SOL_SOCKET:
		switch (name) {
		case SO_DEBUG:
			*i1 = udp->udp_debug;
			break;
		case SO_REUSEADDR:
			*i1 = udp->udp_reuseaddr;
			break;
		case SO_TYPE:
			*i1 = SOCK_DGRAM;
			break;

		/*
		 * The following three items are available here,
		 * but are only meaningful to IP.
		 */
		case SO_DONTROUTE:
			*i1 = udp->udp_dontroute;
			break;
		case SO_USELOOPBACK:
			*i1 = udp->udp_useloopback;
			break;
		case SO_BROADCAST:
			*i1 = udp->udp_broadcast;
			break;

		/*
		 * The following four items can be manipulated,
		 * but changing them should do nothing.
		 */
		case SO_SNDBUF:
			*i1 = q->q_hiwat;
			break;
		case SO_RCVBUF:
			*i1 = RD(q)->q_hiwat;
			break;
		case SO_DGRAM_ERRIND:
			*i1 = udp->udp_dgram_errind;
			break;
		default:
			return (-1);
		}
		break;
	case IPPROTO_IP:
		switch (name) {
		case IP_OPTIONS:
			if (udp->udp_ip_rcv_options_len)
				bcopy((char *)udp->udp_ip_rcv_options,
				    (char *)ptr,
				    udp->udp_ip_rcv_options_len);
			return (udp->udp_ip_rcv_options_len);
		case IP_TOS:
			*i1 = (int)udp->udp_type_of_service;
			break;
		case IP_TTL:
			*i1 = (int)udp->udp_ttl;
			break;
		case IP_MULTICAST_IF:
			/* 0 address if not set */
			bcopy((char *)&udp->udp_multicast_if_addr, (char *)ptr,
			    sizeof (udp->udp_multicast_if_addr));
			return (sizeof (udp->udp_multicast_if_addr));
		case IP_MULTICAST_TTL:
			bcopy((char *)&udp->udp_multicast_ttl, (char *)ptr,
			    sizeof (udp->udp_multicast_ttl));
			return (sizeof (udp->udp_multicast_ttl));
		case IP_MULTICAST_LOOP:
			*ptr = udp->udp_multicast_loop;
			return (sizeof (uint8_t));
		case IP_RECVOPTS:
			*i1 = udp->udp_recvopts;
			break;
		case IP_RECVDSTADDR:
			*i1 = udp->udp_recvdstaddr;
			break;
		case IP_ADD_MEMBERSHIP:
		case IP_DROP_MEMBERSHIP:
			/* cannot "get" the value for these */
			return (-1);
		default:
			return (-1);
		}
		break;
	default:
		return (-1);
	}
	return (sizeof (int));
}

/* This routine sets socket options. */
int
udp_opt_set(queue_t *q, u_int mgmt_flags, t_scalar_t level, t_scalar_t name,
    u_int inlen, u_char *invalp, u_int *outlenp, u_char *outvalp)
{
	int	*i1 = (int *)invalp;
	udp_t	*udp = (udp_t *)q->q_ptr;
	int	checkonly;

	if (mgmt_flags == (T_NEGOTIATE|T_CHECK)) {
		/*
		 * both set - magic signal that
		 * negotiation not from T_OPTMGMT_REQ
		 *
		 * Negotiating local and "association-related" options
		 * through T_UNITDATA_REQ.
		 *
		 * Following routine can filter out ones we do not
		 * want to be "set" this way.
		 */
		if (! udp_allow_udropt_set(level, name)) {
			*outlenp = 0;
			return (EINVAL);
		}
	}

	if (mgmt_flags & T_NEGOTIATE) {
		ASSERT(mgmt_flags == T_NEGOTIATE ||
		    mgmt_flags == (T_CHECK|T_NEGOTIATE));

		checkonly = 0;

	} else {
		ASSERT(mgmt_flags == T_CHECK);

		checkonly = 1;

		/*
		 * Note: For T_CHECK,
		 * inlen != 0 implies value supplied and
		 * 	we have to "pretend" to set it.
		 * inlen == 0 implies that there is no
		 * 	value part in T_CHECK request just validation
		 * done elsewhere should be enough, we just return here.
		 */
		if (inlen == 0) {
			*outlenp = 0;
			return (0);
		}
	}

	ASSERT((mgmt_flags & T_NEGOTIATE) ||
		(mgmt_flags == T_CHECK && inlen != 0));

	/*
	 * For fixed length options, no sanity check
	 * of passed in length is done. It is assumed *_optcom_req()
	 * routines do the right thing.
	 */

	switch (level) {
	case SOL_SOCKET:
		switch (name) {
		case SO_REUSEADDR:
			if (! checkonly)
				udp->udp_reuseaddr = *i1;
			break;	/* goto sizeof (int) option return */
		case SO_DEBUG:
			if (! checkonly)
				udp->udp_debug = *i1;
			break;	/* goto sizeof (int) option return */
		/*
		 * The following three items are available here,
		 * but are only meaningful to IP.
		 */
		case SO_DONTROUTE:
			if (! checkonly)
				udp->udp_dontroute = *i1;
			break;	/* goto sizeof (int) option return */
		case SO_USELOOPBACK:
			if (! checkonly)
				udp->udp_useloopback = *i1;
			break;	/* goto sizeof (int) option return */
		case SO_BROADCAST:
			if (! checkonly)
				udp->udp_broadcast = *i1;
			break;	/* goto sizeof (int) option return */
		/*
		 * The following four items can be manipulated,
		 * but changing them should do nothing.
		 */
		case SO_SNDBUF:
			if (*i1 > udp_max_buf) {
				*outlenp = 0;
				return (ENOBUFS);
			}
			if (! checkonly) {
				q->q_hiwat = *i1;
				q->q_next->q_hiwat = *i1;
			}
			break;	/* goto sizeof (int) option return */
		case SO_RCVBUF:
			if (*i1 > udp_max_buf) {
				*outlenp = 0;
				return (ENOBUFS);
			}
			if (! checkonly) {
				RD(q)->q_hiwat = *i1;
				(void) mi_set_sth_hiwat(RD(q), *i1);
			}
			break;	/* goto sizeof (int) option return */
		case SO_DGRAM_ERRIND:
			if (! checkonly)
				udp->udp_dgram_errind = *i1;
			break;	/* goto sizeof (int) option return */
		default:
			*outlenp = 0;
			return (EINVAL);
		}
		break;
	case IPPROTO_IP:
		switch (name) {
		case IP_OPTIONS:
			/* Save options for use by IP. */

			if (checkonly) {
				if (inlen & 0x3) {
					/* validate as in real "set" */
					*outlenp = 0;
					return (EINVAL);
				}
				/*
				 * OK return - copy input buffer
				 * into output buffer
				 */
				if (invalp != outvalp) {
					/*
					 * don't trust bcopy for
					 * identical src/dst
					 */
					(void) bcopy((char *)invalp,
					    (char *)outvalp, inlen);
				}
				*outlenp = inlen;
				return (0);
			}
			if (inlen & 0x3) {
				/* XXX check overflow inlen too as in tcp.c ? */
				*outlenp = 0;

				return (EINVAL);
			}
			if (udp->udp_ip_snd_options) {
				mi_free((char *)udp->udp_ip_snd_options);
				udp->udp_ip_snd_options_len = 0;
				udp->udp_ip_snd_options = NULL;
			}
			if (inlen) {
				udp->udp_ip_snd_options =
					(u_char *)mi_alloc(inlen, BPRI_HI);
				if (udp->udp_ip_snd_options) {
					bcopy((char *)invalp,
					    (char *)udp->udp_ip_snd_options,
					    inlen);
					udp->udp_ip_snd_options_len = inlen;
				}
			}
			(void) mi_set_sth_wroff(RD(q), udp->udp_hdr_length +
			    udp->udp_ip_snd_options_len +
			    udp_wroff_extra);
			/* OK return - copy input buffer into output buffer */
			if (invalp != outvalp) {
				/* don't trust bcopy for identical src/dst */
				(void) bcopy((char *)invalp,
					(char *)outvalp, inlen);
			}
			*outlenp = inlen;
			return (0);
		case IP_TTL:
			if (! checkonly) {
				/*
				 * save ttl in udp state and connected
				 * ip header
				 */
				udp->udp_ttl = (u_char) *i1;
				udp->udp_ipha.ipha_ttl = (u_char) *i1;
			}
			break;	/* goto sizeof (int) option return */
		case IP_TOS:
			if (! checkonly) {
				/*
				 * save tos in udp state and connected ip
				 * header
				 */
				udp->udp_type_of_service = (u_char) *i1;
				udp->udp_ipha.ipha_type_of_service =
					(u_char) *i1;
			}
			break;	/* goto sizeof (int) option return */
		case IP_MULTICAST_IF: {
			/*
			 * TODO should check OPTMGMT reply and undo this if
			 * there is an error.
			 */
			struct in_addr *inap = (struct in_addr *)invalp;
			if (! checkonly) {
				udp->udp_multicast_if_addr = (ipaddr_t)
				    inap->s_addr;
			}
			/* struct copy */
			*(struct in_addr *)outvalp = *inap;
			*outlenp = sizeof (struct in_addr);
			return (0);
		}
		case IP_MULTICAST_TTL:
			if (! checkonly)
				udp->udp_multicast_ttl = *invalp;
			*outvalp = *invalp;
			*outlenp = sizeof (u_char);
			return (0);
		case IP_MULTICAST_LOOP:
			if (! checkonly)
				udp->udp_multicast_loop = *invalp;
			*outvalp = *invalp;
			*outlenp = sizeof (u_char);
			return (0);
		case IP_RECVOPTS:
			if (! checkonly)
				udp->udp_recvopts = *i1;
			break;	/* goto sizeof (int) option return */
		case IP_RECVDSTADDR:
			if (! checkonly)
				udp->udp_recvdstaddr = *i1;
			break;	/* goto sizeof (int) option return */
		case IP_ADD_MEMBERSHIP:
		case IP_DROP_MEMBERSHIP:
			/*
			 * "soft" error (negative)
			 * option not handled at this level
			 * Do not modify *outlenp.
			 */
			return (-EINVAL);
		default:
			*outlenp = 0;
			return (EINVAL);
		}
		break;
	default:
		*outlenp = 0;
		return (EINVAL);
	}
	/*
	 * Common case of return from an option that is sizeof (int)
	 */
	*(int *)outvalp = *i1;
	*outlenp = sizeof (int);
	return (0);
}

/*
 * This routine retrieves the value of an ND variable in a udpparam_t
 * structure.  It is called through nd_getset when a user reads the
 * variable.
 */
/* ARGSUSED */
static int
udp_param_get(queue_t *q, mblk_t *mp, caddr_t cp)
{
	udpparam_t *udppa = (udpparam_t *)cp;

	(void) mi_mpprintf(mp, "%d", udppa->udp_param_value);
	return (0);
}

/*
 * Walk through the param array specified registering each element with the
 * named dispatch (ND) handler.
 */
static boolean_t
udp_param_register(udpparam_t *udppa, int cnt)
{
	for (; cnt-- > 0; udppa++) {
		if (udppa->udp_param_name && udppa->udp_param_name[0]) {
			if (!nd_load(&udp_g_nd, udppa->udp_param_name,
			    udp_param_get, udp_param_set,
			    (caddr_t)udppa)) {
				nd_free(&udp_g_nd);
				return (false);
			}
		}
	}
	if (!nd_load(&udp_g_nd, "udp_extra_priv_ports",
	    udp_extra_priv_ports_get, nil(pfi_t), nil(caddr_t))) {
		nd_free(&udp_g_nd);
		return (false);
	}
	if (!nd_load(&udp_g_nd, "udp_extra_priv_ports_add",
	    nil(pfi_t), udp_extra_priv_ports_add, nil(caddr_t))) {
		nd_free(&udp_g_nd);
		return (false);
	}
	if (!nd_load(&udp_g_nd, "udp_extra_priv_ports_del",
	    nil(pfi_t), udp_extra_priv_ports_del, nil(caddr_t))) {
		nd_free(&udp_g_nd);
		return (false);
	}
	if (!nd_load(&udp_g_nd, "udp_status", udp_status_report, nil(pfi_t),
	    nil(caddr_t))) {
		nd_free(&udp_g_nd);
		return (false);
	}
	return (true);
}

/* This routine sets an ND variable in a udpparam_t structure. */
/* ARGSUSED */
static int
udp_param_set(queue_t *q, mblk_t *mp, char *value, caddr_t cp)
{
	char	*end;
	int	new_value;
	udpparam_t	*udppa = (udpparam_t *)cp;

	ASSERT(MUTEX_HELD(&udp_g_lock));
	/* Convert the value from a string into a 32-bit integer. */
	new_value = (int)mi_strtol(value, &end, 10);
	/*
	 * Fail the request if the new value does not lie within the
	 * required bounds.
	 */
	if (end == value || new_value < udppa->udp_param_min ||
	    new_value > udppa->udp_param_max)
		return (EINVAL);

	/* Set the new value */
	udppa->udp_param_value = new_value;
	return (0);
}

static void
udp_rput(queue_t *q, mblk_t *mp_orig)
{
	struct T_unitdata_ind	*tudi;
	mblk_t	*mp = mp_orig;
	u_char	*rptr;
	int	hdr_length;
	int	udi_size;	/* Size of T_unitdata_ind */
	udp_t	*udp;

	TRACE_2(TR_FAC_UDP, TR_UDP_RPUT_START,
	    "udp_rput_start: q %p db_type 0%o", q, mp->b_datap->db_type);

	rptr = mp->b_rptr;
	switch (mp->b_datap->db_type) {
	case M_DATA:
		/*
		 * M_DATA messages contain IP datagrams.  They are handled
		 * after this switch.
		 */
		hdr_length = ((rptr[0] & 0xF) << 2) + UDPH_SIZE;
		udp = (udp_t *)q->q_ptr;
		if ((hdr_length > IP_SIMPLE_HDR_LENGTH + UDPH_SIZE) ||
		    (udp->udp_ip_rcv_options_len)) {
			become_exclusive(q, mp_orig, udp_rput_other);
			TRACE_2(TR_FAC_UDP, TR_UDP_RPUT_END,
				"udp_rput_end: q %p (%S)", q, "end");
			return;
		}
		break;
	case M_PROTO:
	case M_PCPROTO:
		/* M_PROTO messages contain some type of TPI message. */
		if ((mp->b_wptr - rptr) < sizeof (int32_t)) {
			freemsg(mp);
			TRACE_2(TR_FAC_UDP, TR_UDP_RPUT_END,
				"udp_rput_end: q %p (%S)", q, "protoshort");
			return;
		}
		become_exclusive(q, mp_orig, udp_rput_other);
		TRACE_2(TR_FAC_UDP, TR_UDP_RPUT_END,
			"udp_rput_end: q %p (%S)", q, "proto");
		return;
	case M_FLUSH:
		if (*mp->b_rptr & FLUSHR)
			flushq(q, FLUSHDATA);
		putnext(q, mp);
		TRACE_2(TR_FAC_UDP, TR_UDP_RPUT_END,
			"udp_rput_end: q %p (%S)", q, "flush");
		return;
	case M_CTL:
		/*
		 * ICMP messages.
		 */
		udp_icmp_error(q, mp);
		TRACE_2(TR_FAC_UDP, TR_UDP_RPUT_END,
			"udp_rput_end: q %p (%S)", q, "m_ctl");
		return;
	default:
		putnext(q, mp);
		TRACE_2(TR_FAC_UDP, TR_UDP_RPUT_END,
			"udp_rput_end: q %p (%S)", q, "default");
		return;
	}
	/*
	 * This is the inbound data path.
	 * First, we make sure the data contains both IP and UDP headers.
	 */
	if ((mp->b_wptr - rptr) < hdr_length) {
		if (!pullupmsg(mp, hdr_length)) {
			freemsg(mp_orig);
			BUMP_MIB(udp_mib.udpInErrors);
			TRACE_2(TR_FAC_UDP, TR_UDP_RPUT_END,
				"udp_rput_end: q %p (%S)", q, "hdrshort");
			return;
		}
		rptr = mp->b_rptr;
	}
	/* Walk past the headers. */
	mp->b_rptr = rptr + hdr_length;

	/*
	 * Normally only send up the address.
	 * If IP_RECVDSTADDR is set we include the destination IP address
	 * as an option. With IP_RECVOPTS we include all the IP options.
	 * Only ip_rput_other() handles packets that contain IP options.
	 */
	udi_size = sizeof (struct T_unitdata_ind) + sizeof (ipa_t);
	if (udp->udp_recvdstaddr)
		udi_size += sizeof (struct T_opthdr) + sizeof (struct in_addr);
	ASSERT(IPH_HDR_LENGTH((ipha_t *)rptr) == IP_SIMPLE_HDR_LENGTH);

	/* Allocate a message block for the T_UNITDATA_IND structure. */
	mp = allocb(udi_size, BPRI_MED);
	if (!mp) {
		freemsg(mp_orig);
		TRACE_2(TR_FAC_UDP, TR_UDP_RPUT_END,
			"udp_rput_end: q %p (%S)", q, "allocbfail");
		return;
	}
	mp->b_cont = mp_orig;
	mp->b_datap->db_type = M_PROTO;
	tudi = (struct T_unitdata_ind *)mp->b_rptr;
	mp->b_wptr = (u_char *)tudi + udi_size;
	tudi->PRIM_type = T_UNITDATA_IND;
	tudi->SRC_length = sizeof (ipa_t);
	tudi->SRC_offset = sizeof (struct T_unitdata_ind);
	tudi->OPT_offset = sizeof (struct T_unitdata_ind) + sizeof (ipa_t);
	udi_size -= (sizeof (struct T_unitdata_ind) + sizeof (ipa_t));
	tudi->OPT_length = udi_size;
#define	ipa	((ipa_t *)&tudi[1])
	*(ipaddr_t *)&ipa->ip_addr[0] = ((ipaddr_t *)rptr)[3];
	*(in_port_t *)ipa->ip_port =		/* Source port */
	    ((in_port_t *)mp->b_cont->b_rptr)[-UDPH_SIZE/sizeof (in_port_t)];
	ipa->ip_family = ((udp_t *)q->q_ptr)->udp_family;
	*(uint32_t *)&ipa->ip_pad[0] = 0;
	*(uint32_t *)&ipa->ip_pad[4] = 0;

	/* Add options if IP_RECVDSTADDR has been set. */
	if (udi_size != 0) {
		/*
		 * Copy in destination address before options to avoid any
		 * padding issues.
		 */
		char *dstopt;

		dstopt = (char *)&ipa[1];
		if (udp->udp_recvdstaddr) {
			struct T_opthdr toh;
			ipaddr_t *dstptr;

			toh.level = IPPROTO_IP;
			toh.name = IP_RECVDSTADDR;
			toh.len = sizeof (struct T_opthdr) + sizeof (ipaddr_t);
			toh.status = 0;
			bcopy((char *)&toh, dstopt, sizeof (toh));
			dstopt += sizeof (toh);
			dstptr = (ipaddr_t *)dstopt;
			*dstptr = ((ipaddr_t *)rptr)[4];
			dstopt += sizeof (ipaddr_t);
			udi_size -= toh.len;
		}
		ASSERT(udi_size == 0);	/* "Consumed" all of allocated space */
	}
#undef	ipa

	BUMP_MIB(udp_mib.udpInDatagrams);
	TRACE_2(TR_FAC_UDP, TR_UDP_RPUT_END,
		"udp_rput_end: q %p (%S)", q, "end");
	putnext(q, mp);
}

static void
udp_rput_other(queue_t *q, mblk_t *mp_orig)
{
	struct T_unitdata_ind	*tudi;
	mblk_t	*mp = mp_orig;
	u_char	*rptr;
	int	hdr_length;
	int	udi_size;	/* Size of T_unitdata_ind */
	int	opt_len;	/* Length of IP options */
	struct T_error_ack	*tea;
	udp_t	*udp;
	mblk_t *mp1;
	ire_t *ire;

	TRACE_2(TR_FAC_UDP, TR_UDP_RPUT_START,
	    "udp_rput_other: q %p db_type 0%o", q, mp->b_datap->db_type);

	udp = (udp_t *)q->q_ptr;
	rptr = mp->b_rptr;

	switch (mp->b_datap->db_type) {
	case M_DATA:
		/*
		 * M_DATA messages contain IP datagrams.  They are handled
		 * after this switch.
		 */
		break;
	case M_PROTO:
	case M_PCPROTO:
		/* M_PROTO messages contain some type of TPI message. */
		ASSERT((uintptr_t)(mp->b_wptr - rptr) <= (uintptr_t)INT_MAX);
		if (mp->b_wptr - rptr < sizeof (((struct T_error_ack *)rptr)
		    ->PRIM_type)) {
			freemsg(mp);
			TRACE_2(TR_FAC_UDP, TR_UDP_RPUT_END,
			    "udp_rput_other_end: q %p (%S)", q, "protoshort");
			return;
		}
		tea = (struct T_error_ack *)rptr;

		switch (tea->PRIM_type) {
		case T_ERROR_ACK:
			switch (tea->ERROR_prim) {
			case O_T_BIND_REQ:
			case T_BIND_REQ:
				/*
				 * If our O_T_BIND_REQ/T_BIND_REQ fails,
				 * clear out the associated port and source
				 * address before passing the message
				 * upstream. If this was caused by a T_CONN_REQ
				 * revert back to bound state.
				 */
				mutex_enter(&udp_g_lock);
				if (udp->udp_state == TS_DATA_XFER) {
					tea->ERROR_prim = T_CONN_REQ;
					if (udp->udp_hdr_mp != NULL) {
						freemsg(udp->udp_hdr_mp);
						udp->udp_hdr_mp = NULL;
					}
					udp->udp_ipha.ipha_src = udp->udp_src =
						udp->udp_bound_src;
					udp->udp_state = TS_IDLE;
					mutex_exit(&udp_g_lock);
					break;
				}

				if (udp->udp_discon_pending) {
					tea->ERROR_prim = T_DISCON_REQ;
					udp->udp_discon_pending = 0;
				}
				udp->udp_port = 0;
				udp->udp_src = 0;
				udp->udp_bound_src = 0;
				udp->udp_ipha.ipha_src = 0;
				udp->udp_state = TS_UNBND;
				mutex_exit(&udp_g_lock);
				break;
			default:
				break;
			}
			break;
		case T_BIND_ACK:
			if (udp->udp_discon_pending)
				udp->udp_discon_pending = 0;

			/*
			 * If a broadcast/multicast address was bound set
			 * the source address to 0.
			 * This ensures no datagrams with broadcast address
			 * as source address are emitted (which would violate
			 * RFC1122 - Hosts requirements)
			 *
			 * Note that when connecting the returned IRE is
			 * for the destination address and we only perform
			 * the broadcast check for the source address (it
			 * is OK to connect to a broadcast/multicast address.)
			 */
			mp1 = mp->b_cont;
			if (mp1 != NULL &&
			    mp1->b_datap->db_type == IRE_DB_TYPE) {
				ire = (ire_t *)mp1->b_rptr;

				if (ire->ire_type == IRE_BROADCAST &&
				    udp->udp_state != TS_DATA_XFER) {
					udp->udp_src = 0;
					udp->udp_ipha.ipha_src = 0;
				} else if (udp->udp_src == INADDR_ANY) {
					udp->udp_ipha.ipha_src =
					    udp->udp_src = ire->ire_src_addr;
				}
				mp1 = mp1->b_cont;
			}
			/*
			 * Look for one or more appended ACK message added by
			 * udp_connect or udp_disconnect.
			 * If none found just send up the T_BIND_ACK.
			 * udp_connect has appended a T_OK_ACK and a T_CONN_CON.
			 * udp_disconnect has appended a T_OK_ACK.
			 */
			if (mp1 != NULL) {
				if (mp->b_cont == mp1)
					mp->b_cont = NULL;
				else {
					ASSERT(mp->b_cont->b_cont == mp1);
					mp->b_cont->b_cont = NULL;
				}
				freemsg(mp);
				mp = mp1;
				while (mp != NULL) {
					mp1 = mp->b_cont;
					mp->b_cont = NULL;
					putnext(q, mp);
					mp = mp1;
				}
				return;
			}
			freemsg(mp->b_cont);
			mp->b_cont = NULL;
			break;

		case T_OPTMGMT_ACK:
		case T_OK_ACK:
			break;
		default:
			freemsg(mp);
			return;
		}
		putnext(q, mp);
		return;
	}

	/*
	 * This is the inbound data path.
	 * First, we make sure the data contains both IP and UDP headers.
	 */
	hdr_length = ((rptr[0] & 0xF) << 2) + UDPH_SIZE;
	if (mp->b_wptr - rptr < hdr_length) {
		if (!pullupmsg(mp, hdr_length)) {
			freemsg(mp_orig);
			BUMP_MIB(udp_mib.udpInErrors);
			TRACE_2(TR_FAC_UDP, TR_UDP_RPUT_END,
			    "udp_rput_other_end: q %p (%S)", q, "hdrshort");
			return;
		}
		rptr = mp->b_rptr;
	}
	/* Walk past the headers. */
	mp->b_rptr = rptr + hdr_length;

	/* Save the options if any */
	opt_len = hdr_length - (IP_SIMPLE_HDR_LENGTH + UDPH_SIZE);
	if (opt_len > 0) {
		if (opt_len > udp->udp_ip_rcv_options_len) {
			if (udp->udp_ip_rcv_options_len)
				mi_free((char *)udp->udp_ip_rcv_options);
			udp->udp_ip_rcv_options_len = 0;
			udp->udp_ip_rcv_options =
			    (u_char *)mi_alloc(opt_len, BPRI_HI);
			if (udp->udp_ip_rcv_options)
				udp->udp_ip_rcv_options_len = opt_len;
		}
		if (udp->udp_ip_rcv_options_len) {
			bcopy((char *)rptr + IP_SIMPLE_HDR_LENGTH,
			    (char *)udp->udp_ip_rcv_options,
			    opt_len);
			/* Adjust length if we are resusing the space */
			udp->udp_ip_rcv_options_len = opt_len;
		}
	} else if (udp->udp_ip_rcv_options_len) {
		mi_free((char *)udp->udp_ip_rcv_options);
		udp->udp_ip_rcv_options = nilp(uint8_t);
		udp->udp_ip_rcv_options_len = 0;
	}

	/*
	 * Normally only send up the address.
	 * If IP_RECVDSTADDR is set we include the destination IP address
	 * as an option. With IP_RECVOPTS we include all the IP options.
	 */
	udi_size = sizeof (struct T_unitdata_ind) + sizeof (ipa_t);
	if (udp->udp_recvdstaddr)
		udi_size += sizeof (struct T_opthdr) + sizeof (struct in_addr);
	if (udp->udp_recvopts && opt_len > 0)
		udi_size += sizeof (struct T_opthdr) + opt_len;

	/* Allocate a message block for the T_UNITDATA_IND structure. */
	mp = allocb(udi_size, BPRI_MED);
	if (!mp) {
		freemsg(mp_orig);
		TRACE_2(TR_FAC_UDP, TR_UDP_RPUT_END,
			"udp_rput_other_end: q %p (%S)", q, "allocbfail");
		return;
	}
	mp->b_cont = mp_orig;
	mp->b_datap->db_type = M_PROTO;
	tudi = (struct T_unitdata_ind *)mp->b_rptr;
	mp->b_wptr = (u_char *)tudi + udi_size;
	tudi->PRIM_type = T_UNITDATA_IND;
	tudi->SRC_length = sizeof (ipa_t);
	tudi->SRC_offset = sizeof (struct T_unitdata_ind);
	tudi->OPT_offset = sizeof (struct T_unitdata_ind) + sizeof (ipa_t);
	udi_size -= (sizeof (struct T_unitdata_ind) + sizeof (ipa_t));
	tudi->OPT_length = udi_size;
#define	ipa	((ipa_t *)&tudi[1])
					/* First half of source addr */
	*(uint16_t *)&ipa->ip_addr[0] = ((uint16_t *)rptr)[6];
					/* Second half of source addr */
	*(uint16_t *)&ipa->ip_addr[2] = ((uint16_t *)rptr)[7];
	*(in_port_t *)ipa->ip_port =		/* Source port */
	    ((in_port_t *)mp->b_cont->b_rptr)[-UDPH_SIZE/sizeof (in_port_t)];
	ipa->ip_family = ((udp_t *)q->q_ptr)->udp_family;
	*(uint32_t *)&ipa->ip_pad[0] = 0;
	*(uint32_t *)&ipa->ip_pad[4] = 0;

	/* Add options if IP_RECVOPTS or IP_RECVDSTADDR has been set. */
	if (udi_size != 0) {
		/*
		 * Copy in destination address before options to avoid any
		 * padding issues.
		 */
		char *dstopt;

		dstopt = (char *)&ipa[1];
		if (udp->udp_recvdstaddr) {
			struct T_opthdr toh;
			ipaddr_t *dstptr;

			toh.level = IPPROTO_IP;
			toh.name = IP_RECVDSTADDR;
			toh.len = sizeof (struct T_opthdr) + sizeof (ipaddr_t);
			toh.status = 0;
			bcopy((char *)&toh, dstopt, sizeof (toh));
			dstopt += sizeof (toh);
			dstptr = (ipaddr_t *)dstopt;
			*dstptr = (((ipaddr_t *)rptr)[4]);
			dstopt += sizeof (ipaddr_t);
			udi_size -= toh.len;
		}
		if (udp->udp_recvopts && udi_size != 0) {
			struct T_opthdr toh;

			toh.level = IPPROTO_IP;
			toh.name = IP_RECVOPTS;
			toh.len = sizeof (struct T_opthdr) + opt_len;
			toh.status = 0;
			bcopy((char *)&toh, dstopt, sizeof (toh));
			dstopt += sizeof (toh);
			bcopy((char *)rptr + IP_SIMPLE_HDR_LENGTH, dstopt,
				opt_len);
			dstopt += opt_len;
			udi_size -= toh.len;
		}
		ASSERT(udi_size == 0);	/* "Consumed" all of allocated space */
	}
#undef	ipa
	BUMP_MIB(udp_mib.udpInDatagrams);
	TRACE_2(TR_FAC_UDP, TR_UDP_RPUT_END,
	    "udp_rput_other_end: q %p (%S)", q, "end");
	putnext(q, mp);
}

/*
 * return SNMP stuff in buffer in mpdata
 */
static	int
udp_snmp_get(queue_t *q, mblk_t *mpctl)
{
	mblk_t		*mpdata;
	mblk_t		*mp2ctl;
	struct opthdr	*optp;
	IDP		idp;
	udp_t		*udp;
	mib2_udpEntry_t	ude;

	if (!mpctl || !(mpdata = mpctl->b_cont) ||
	    !(mp2ctl = copymsg(mpctl)))
		return (0);

	optp = (struct opthdr *)&mpctl->b_rptr[sizeof (struct T_optmgmt_ack)];
	optp->level = MIB2_UDP;
	optp->name = 0;
	(void) snmp_append_data(mpdata, (char *)&udp_mib, sizeof (udp_mib));
	optp->len = msgdsize(mpdata);
	qreply(q, mpctl);

	mpctl = mp2ctl;
	mpdata = mp2ctl->b_cont;
	SET_MIB(udp_mib.udpEntrySize, sizeof (mib2_udpEntry_t));
	mutex_enter(&udp_g_lock);
	for (idp = mi_first_ptr(&udp_g_head);
	    (udp = (udp_t *)idp) != 0;
	    idp = mi_next_ptr(&udp_g_head, idp)) {
		/* Note that the port numbers are sent in host byte order */
		ude.udpLocalAddress = udp->udp_bound_src;
		ude.udpLocalPort = ntohs(udp->udp_port);
		if (udp->udp_state == TS_UNBND)
			ude.udpEntryInfo.ue_state = MIB2_UDP_unbound;
		else if (udp->udp_state == TS_IDLE)
			ude.udpEntryInfo.ue_state = MIB2_UDP_idle;
		else if (udp->udp_state == TS_DATA_XFER)
			ude.udpEntryInfo.ue_state = MIB2_UDP_connected;
		else
			ude.udpEntryInfo.ue_state = MIB2_UDP_unknown;
		if (udp->udp_state == TS_DATA_XFER) {
			ude.udpLocalAddress = udp->udp_src;
			ude.udpEntryInfo.ue_RemoteAddress =
				udp->udp_ipha.ipha_dst;
			ude.udpEntryInfo.ue_RemotePort =
				ntohs(udp->udp_udpha->uha_dst_port);
		}
		(void) snmp_append_data(mpdata, (char *)&ude,
		    sizeof (mib2_udpEntry_t));
	}
	mutex_exit(&udp_g_lock);
	optp = (struct opthdr *)&mpctl->b_rptr[sizeof (struct T_optmgmt_ack)];
	optp->level = MIB2_UDP;
	optp->name = MIB2_UDP_5;
	optp->len = msgdsize(mpdata);
	qreply(q, mpctl);
	return (1);
}

/*
 * Return 0 if invalid set request, 1 otherwise, including non-udp requests.
 * NOTE: Per MIB-II, UDP has no writable data.
 * TODO:  If this ever actually tries to set anything, it needs to be
 * to do the appropriate locking.
 */
/* ARGSUSED */
static	int
udp_snmp_set(queue_t *q, t_scalar_t level, t_scalar_t name, u_char *ptr,
    int len)
{
	switch (level) {
	case MIB2_UDP:
		return (0);
	default:
		return (1);
	}
}

/* Report for ndd "udp_status" */
/* ARGSUSED */
static	int
udp_status_report(queue_t *q, mblk_t *mp, caddr_t cp)
{
	IDP	idp;
	udp_t	*udp;
	udpha_t	*udpha;
	char	*state;
	uint	rport;
	ipaddr_t addr;

	/* BEGIN CSTYLED */
	(void) mi_mpprintf(mp,
	    "UDP     "
#ifdef _LP64
	    "        "
#endif
	            " lport src addr        dest addr       port  state");
	/*   12345678 12345 xxx.xxx.xxx.xxx xxx.xxx.xxx.xxx 12345 UNBOUND */
	/* END CSTYLED */

	ASSERT(MUTEX_HELD(&udp_g_lock));
	for (idp = mi_first_ptr(&udp_g_head);
	    (udp = (udp_t *)idp) != 0;
	    idp = mi_next_ptr(&udp_g_head, idp)) {
		if (udp->udp_state == TS_UNBND)
			state = "UNBOUND";
		else if (udp->udp_state == TS_IDLE)
			state = "IDLE";
		else if (udp->udp_state == TS_DATA_XFER)
			state = "CONNECTED";
		else
			state = "UnkState";
		addr = udp->udp_ipha.ipha_dst;
		rport = 0;
		if ((udpha = udp->udp_udpha) != NULL)
			rport = ntohs(udpha->uha_dst_port);
		(void) mi_mpprintf(mp,
		    "%0lx %05d %03d.%03d.%03d.%03d %03d.%03d.%03d.%03d "
		    "%05d %s",
		    udp, ntohs(udp->udp_port),
		    (udp->udp_bound_src >> 24) & 0xff,
		    (udp->udp_bound_src >> 16) & 0xff,
		    (udp->udp_bound_src >> 8) & 0xff,
		    (udp->udp_bound_src >> 0) & 0xff,
		    (addr >> 24) & 0xff,
		    (addr >> 16) & 0xff,
		    (addr >> 8) & 0xff,
		    (addr >> 0) & 0xff,
		    rport, state);
	}
	return (0);
}

/*
 * This routine creates a T_UDERROR_IND message and passes it upstream.
 * The address and options are copied from the T_UNITDATA_REQ message
 * passed in mp.  This message is freed.
 */
static void
udp_ud_err(queue_t *q, mblk_t *mp, t_scalar_t err)
{
	mblk_t	*mp1;
	char	*rptr = (char *)mp->b_rptr;
	struct T_unitdata_req	*tudr = (struct T_unitdata_req *)rptr;

	mp1 = mi_tpi_uderror_ind(&rptr[tudr->DEST_offset],
			tudr->DEST_length, &rptr[tudr->OPT_offset],
			tudr->OPT_length, err);
	if (mp1)
		qreply(q, mp1);
	freemsg(mp);
}

/*
 * This routine removes a port number association from a stream.  It
 * is called by udp_wput to handle T_UNBIND_REQ messages.
 */
static void
udp_unbind(queue_t *q, mblk_t *mp)
{
	udp_t	*udp;

	udp = (udp_t *)q->q_ptr;
	/* If a bind has not been done, we can't unbind. */
	if (udp->udp_state == TS_UNBND) {
		udp_err_ack(q, mp, TOUTSTATE, 0);
		return;
	}
	mutex_enter(&udp_g_lock);
	udp->udp_src = udp->udp_bound_src = 0;
	udp->udp_ipha.ipha_src = 0;
	udp->udp_port = 0;
	udp->udp_state = TS_UNBND;
	mutex_exit(&udp_g_lock);

	/* Pass the unbind to IP */
	putnext(q, mp);
}

/*
 * Don't let port fall into the privileged range.
 * Since the extra priviledged ports can be arbitrary we also
 * ensure that we exclude those from consideration.
 * udp_g_epriv_ports is not sorted thus we loop over it until
 * there are no changes.
 */
static in_port_t
udp_update_next_port(in_port_t port)
{
	int i;

retry:
	if (port < udp_smallest_anon_port || port > udp_largest_anon_port)
		port = udp_smallest_anon_port;

	if (port < udp_smallest_nonpriv_port)
		port = udp_smallest_nonpriv_port;

	for (i = 0; i < udp_g_num_epriv_ports; i++) {
		if (port == udp_g_epriv_ports[i]) {
			port++;
			/*
			 * Make sure that the port is in the
			 * valid range.
			 */
			goto retry;
		}
	}
	return (port);
}

/*
 * This routine handles all messages passed downstream.  It either
 * consumes the message or passes it downstream; it never queues a
 * a message.
 */
static void
udp_wput(queue_t *q, mblk_t *mp)
{
	u_char	*rptr = mp->b_rptr;
	struct datab *db;
	ipha_t	*ipha;
	udpha_t	*udpha;
	mblk_t	*mp1;
	int	ip_hdr_length;
#define	tudr ((struct T_unitdata_req *)rptr)
	uint32_t	u1;
	udp_t	*udp;

	TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_START,
		"udp_wput_start: q %p db_type 0%o",
		q, mp->b_datap->db_type);

	db = mp->b_datap;
	switch (db->db_type) {
	case M_PROTO:
	case M_PCPROTO:
		ASSERT((uintptr_t)(mp->b_wptr - rptr) <= (uintptr_t)INT_MAX);
		if (mp->b_wptr - rptr >=
		    sizeof (struct T_unitdata_req) + sizeof (ipa_t)) {
			/* Detect valid T_UNITDATA_REQ here */
			if (((union T_primitives *)rptr)->type
			    == T_UNITDATA_REQ)
				break;
		}
		/* FALLTHRU */
	default:
		become_exclusive(q, mp, udp_wput_other);
		return;
	}

	udp = (udp_t *)q->q_ptr;

	/* Handle UNITDATA_REQ messages here */
	if (udp->udp_state == TS_UNBND) {
		/* If a port has not been bound to the stream, fail. */
		udp_ud_err(q, mp, TOUTSTATE);
		TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_END,
			"udp_wput_end: q %p (%S)", q, "outstate");
		return;
	}
	if (tudr->DEST_length != sizeof (ipa_t) ||
	    !(mp1 = mp->b_cont)) {
		udp_ud_err(q, mp, TBADADDR);
		TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_END,
			"udp_wput_end: q %p (%S)", q, "badaddr");
		return;
	}

	/*
	 * If options passed in, feed it for verification and handling
	 */
	if (tudr->OPT_length != 0) {
		int t_error;

		if (udp_unitdata_opt_process(q, mp, &t_error) < 0) {
			/* failure */
			udp_ud_err(q, mp, t_error);
			return;
		}
		ASSERT(t_error == 0);
		/*
		 * Note: success in processing options.
		 * mp option buffer represented by
		 * OPT_length/offset now potentially modified
		 * and contain option setting results
		 */
	}

	/* If the user did not pass along an IP header, create one. */
	ip_hdr_length = udp->udp_hdr_length + udp->udp_ip_snd_options_len;
	ipha = (ipha_t *)&mp1->b_rptr[-ip_hdr_length];
	if ((mp1->b_datap->db_ref != 1) ||
	    ((u_char *)ipha < mp1->b_datap->db_base) ||
	    !OK_32PTR(ipha)) {
		u_char *wptr;

#ifdef DEBUG
		if (!OK_32PTR(ipha))
			printf("udp_wput: unaligned ptr 0x%p for 0x%x/%d\n",
			    (void *)ipha, ntohl(udp->udp_src),
			    ntohs(udp->udp_port));

#endif /* DEBUG */
		mp1 = allocb(ip_hdr_length + udp_wroff_extra, BPRI_LO);
		if (!mp1) {
			udp_ud_err(q, mp, TSYSERR);
			TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_END,
				"udp_wput_end: q %p (%S)", q, "allocbfail2");
			return;
		}
		mp1->b_cont = mp->b_cont;
		wptr = mp1->b_datap->db_lim;
		mp1->b_wptr = wptr;
		ipha = (ipha_t *)(wptr - ip_hdr_length);
	}
	ip_hdr_length -= UDPH_SIZE;
#ifdef	_BIG_ENDIAN
	/* Set version, header length, and tos */
	*(uint16_t *)&ipha->ipha_version_and_hdr_length =
	    ((((IP_VERSION << 4) | (ip_hdr_length>>2)) << 8) |
		udp->udp_type_of_service);
	/* Set ttl and protocol */
	*(uint16_t *)&ipha->ipha_ttl = (udp->udp_ttl << 8) | IPPROTO_UDP;
#else
	/* Set version, header length, and tos */
	*(uint16_t *)&ipha->ipha_version_and_hdr_length =
		((udp->udp_type_of_service << 8) |
		    ((IP_VERSION << 4) | (ip_hdr_length>>2)));
	/* Set ttl and protocol */
	*(uint16_t *)&ipha->ipha_ttl = (IPPROTO_UDP << 8) | udp->udp_ttl;
#endif
	/*
	 * Copy our address into the packet.  If this is zero,
	 * ip will fill in the real source address.
	 */
	ipha->ipha_src = udp->udp_src;
	ipha->ipha_fragment_offset_and_flags = 0;
	ipha->ipha_ident = 0;

	mp1->b_rptr = (u_char *)ipha;

	rptr = &rptr[tudr->DEST_offset];
	ASSERT((uintptr_t)(mp1->b_wptr - (u_char *)ipha) <=
	    (uintptr_t)UINT_MAX);
	u1 = (uint32_t)(mp1->b_wptr - (u_char *)ipha);
	{
		mblk_t	*mp2;
		if ((mp2 = mp1->b_cont) != NULL) {
			do {
				ASSERT((uintptr_t)(mp2->b_wptr - mp2->b_rptr)
				    <= (uintptr_t)UINT_MAX);
				u1 += (uint32_t)(mp2->b_wptr - mp2->b_rptr);
			} while ((mp2 = mp2->b_cont) != NULL);
		}
	}
	/*
	 * If the size of the packet is greater than the maximum allowed by
	 * ip, return an error. Passing this down could cause panics because
	 * the size will have wrapped and be inconsistent with the msg size.
	 */
	if (u1 > IP_MAXPACKET) {
		udp_ud_err(q, mp, TBUFOVFLW);
		TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_END,
		    "udp_wput_end: q %p (%S)", q, "IP length exceeded");
		return;
	}
	ipha->ipha_length = htons((uint16_t)u1);
	u1 -= ip_hdr_length;
	u1 = htons((uint16_t)u1);
	udpha = (udpha_t *)(((u_char *)ipha) + ip_hdr_length);
	/*
	 * Copy in the destination address from the T_UNITDATA
	 * request
	 */
	if (!OK_32PTR(rptr)) {
		/*
		 * Copy the long way if rptr is not aligned for 32-bit
		 * word access.
		 */
#define	ipa	((ipa_t *)rptr)
		bcopy((char *)ipa->ip_addr, (char *)&ipha->ipha_dst,
		    IP_ADDR_LEN);
		if (ipha->ipha_dst == INADDR_ANY)
			ipha->ipha_dst = htonl(INADDR_LOOPBACK);
#define	udph	((udph_t *)udpha)
		udph->uh_dst_port[0] = ipa->ip_port[0];
		udph->uh_dst_port[1] = ipa->ip_port[1];
#undef ipa
#undef	udph
	} else {
#define	ipa	((ipa_t *)rptr)
		ipha->ipha_dst = *(ipaddr_t *)ipa->ip_addr;
		if (ipha->ipha_dst == INADDR_ANY)
			ipha->ipha_dst = htonl(INADDR_LOOPBACK);
		udpha->uha_dst_port = *(in_port_t *)ipa->ip_port;
#undef ipa
	}

	udpha->uha_src_port = udp->udp_port;

	if (ip_hdr_length > IP_SIMPLE_HDR_LENGTH) {
		uint32_t	cksum;

		bcopy((char *)udp->udp_ip_snd_options,
		    (char *)&ipha[1], udp->udp_ip_snd_options_len);
		/*
		 * Massage source route putting first source route in ipha_dst.
		 * Ignore the destination in dl_unitdata_req.
		 * Create an adjustment for a source route, if any.
		 */
		cksum = ip_massage_options(ipha);
		cksum = (cksum & 0xFFFF) + (cksum >> 16);
		cksum -= ((ipha->ipha_dst >> 16) & 0xFFFF) +
		    (ipha->ipha_dst & 0xFFFF);
		if ((int)cksum < 0)
			cksum--;
		cksum = (cksum & 0xFFFF) + (cksum >> 16);
		/*
		 * IP does the checksum if uha_checksum is non-zero,
		 * We make it easy for IP to include our pseudo header
		 * by putting our length in uha_checksum.
		 */
		cksum += u1;
		cksum = (cksum & 0xFFFF) + (cksum >> 16);
#ifdef _LITTLE_ENDIAN
		if (udp_g_do_checksum)
			u1 = (cksum << 16) | u1;
#else
		if (udp_g_do_checksum)
			u1 = (u1 << 16) | cksum;
		else
			u1 <<= 16;
#endif
	} else {
		/*
		 * IP does the checksum if uha_checksum is non-zero,
		 * We make it easy for IP to include our pseudo header
		 * by putting our length in uha_checksum.
		 */
		if (udp_g_do_checksum)
			u1 |= (u1 << 16);
#ifndef _LITTLE_ENDIAN
		else
			u1 <<= 16;
#endif
	}
	/* Set UDP length and checksum */
	*((uint32_t *)&udpha->uha_length) = u1;

	freeb(mp);

	/* We're done.  Pass the packet to ip. */
	BUMP_MIB(udp_mib.udpOutDatagrams);
	TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_END,
		"udp_wput_end: q %p (%S)", q, "end");
	putnext(q, mp1);
#undef tudr
}

static void
udp_wput_other(queue_t *q, mblk_t *mp)
{
	u_char	*rptr = mp->b_rptr;
	struct datab *db;
	struct iocblk *iocp;
	mblk_t	*mp2;
	udp_t	*udp;

	TRACE_1(TR_FAC_UDP, TR_UDP_WPUT_OTHER_START,
		"udp_wput_other_start: q %p", q);

	udp = (udp_t *)q->q_ptr;
	db = mp->b_datap;

	switch (db->db_type) {
	case M_DATA:
		/* Prepend the "connected" header */
		if (udp->udp_hdr_mp == NULL) {
			/* Not connected */
			freemsg(mp);
			TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_OTHER_END,
				"udp_wput_other_end: q %p (%S)",
				q, "not-connected");
			return;
		}
		mp2 = dupb(udp->udp_hdr_mp);
		if (mp2 == NULL) {
			/* unitdata error indication? or M_ERROR? */
			freemsg(mp);
			return;
		}
		mp2->b_cont = mp;
		TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_END,
			"udp_wput_end: q %p (%S)", q, "mdata");
		udp_wput(q, mp2);
		return;
	case M_PROTO:
	case M_PCPROTO:
		if (mp->b_wptr - rptr <
		    sizeof (((union T_primitives *)rptr)->type)) {
			freemsg(mp);
			TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_OTHER_END,
				"udp_wput_other_end: q %p (%S)",
				q, "protoshort");
			return;
		}
		switch (((union T_primitives *)rptr)->type) {
		case T_ADDR_REQ:
			udp_addr_req(q, mp);
			TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_OTHER_END,
				"udp_wput_other_end: q %p (%S)", q, "addrreq");
			return;
		case O_T_BIND_REQ:
		case T_BIND_REQ:
			udp_bind(q, mp);
			TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_OTHER_END,
				"udp_wput_other_end: q %p (%S)", q, "bindreq");
			return;
		case T_CONN_REQ:
			udp_connect(q, mp);
			TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_OTHER_END,
				"udp_wput_other_end: q %p (%S)", q, "connreq");
			return;
		case T_CAPABILITY_REQ:
			udp_capability_req(q, mp);
			TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_OTHER_END,
				"udp_wput_other_end: q %p (%S)", q, "capabreq");
			return;
		case T_INFO_REQ:
			udp_info_req(q, mp);
			TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_OTHER_END,
				"udp_wput_other_end: q %p (%S)", q, "inforeq");
			return;
		case T_UNITDATA_REQ:
			/*
			 * If a T_UNITDATA_REQ gets here, the address must
			 * be bad.  Valid T_UNITDATA_REQs are handled
			 * in udp_wput.
			 */
			udp_ud_err(q, mp, TBADADDR);
			TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_OTHER_END,
				"udp_wput_other_end: q %p (%S)",
				q, "unitdatareq");
			return;
		case T_UNBIND_REQ:
			udp_unbind(q, mp);
			TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_OTHER_END,
			    "udp_wput_other_end: q %p (%S)", q, "unbindreq");
			return;
		case O_T_OPTMGMT_REQ:
			if (!snmpcom_req(q, mp, udp_snmp_set, udp_snmp_get,
			    udp->udp_priv_stream))
				svr4_optcom_req(q, mp, udp->udp_priv_stream,
				    &udp_opt_obj);
			TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_OTHER_END,
			    "udp_wput_other_end: q %p (%S)",
			    q, "optmgmtreq");
			return;

		case T_OPTMGMT_REQ:
			tpi_optcom_req(q, mp, udp->udp_priv_stream,
				&udp_opt_obj);
			TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_OTHER_END,
				"udp_wput_other_end: q %p (%S)",
				q, "optmgmtreq");
			return;

		case T_DISCON_REQ:
			udp_disconnect(q, mp);
			TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_OTHER_END,
				"udp_wput_other_end: q %p (%S)",
				q, "disconreq");
			return;

		/* The following TPI message is not supported by udp. */
		case O_T_CONN_RES:
		case T_CONN_RES:
			udp_err_ack(q, mp, TNOTSUPPORT, 0);
			TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_OTHER_END,
				"udp_wput_other_end: q %p (%S)",
				q, "connres/disconreq");
			return;

		/* The following 3 TPI messages are illegal for udp. */
		case T_DATA_REQ:
		case T_EXDATA_REQ:
		case T_ORDREL_REQ:
			freemsg(mp);
			(void) putctl1(RD(q), M_ERROR, EPROTO);
			TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_OTHER_END,
				"udp_wput_other_end: q %p (%S)",
				q, "data/exdata/ordrel");
			return;
		default:
			break;
		}
		break;
	case M_FLUSH:
		if (*rptr & FLUSHW)
			flushq(q, FLUSHDATA);
		break;
	case M_IOCTL:
		/* TODO: M_IOCTL access to udp_wants_header. */
		iocp = (struct iocblk *)mp->b_rptr;
		switch (iocp->ioc_cmd) {
		case TI_GETPEERNAME:
			if (udp->udp_udpha == NULL) {
				/*
				 * If a default destination address has not
				 * been associated with the stream, then we
				 * don't know the peer's name.
				 */
				iocp->ioc_error = ENOTCONN;
err_ret:;
				iocp->ioc_count = 0;
				mp->b_datap->db_type = M_IOCACK;
				qreply(q, mp);
				TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_OTHER_END,
					"udp_wput_other_end: q %p (%S)",
					q, "getpeername");
				return;
			}
			/* FALLTHRU */
		case TI_GETMYNAME: {
			/*
			 * For TI_GETPEERNAME and TI_GETMYNAME, we first
			 * need to copyin the user's strbuf structure.
			 * Processing will continue in the M_IOCDATA case
			 * below.
			 */
			mi_copyin(q, mp, nilp(char),
			    SIZEOF_STRUCT(strbuf, iocp->ioc_flag));
			TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_OTHER_END,
				"udp_wput_other_end: q %p (%S)",
				q, "getmyname");
			return;
			}
		case ND_SET:
			if (!udp->udp_priv_stream) {
				iocp->ioc_error = EPERM;
				goto err_ret;
			}
			/* FALLTHRU */
		case ND_GET:
			mutex_enter(&udp_g_lock);
			if (nd_getset(q, udp_g_nd, mp)) {
				mutex_exit(&udp_g_lock);
				qreply(q, mp);
				TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_OTHER_END,
					"udp_wput_other_end: q %p (%S)",
					q, "get");
				return;
			}
			mutex_exit(&udp_g_lock);
			break;
		default:
			break;
		}
		break;
	case M_IOCDATA:
		/* Make sure it is one of ours. */
		switch (((struct iocblk *)mp->b_rptr)->ioc_cmd) {
		case TI_GETMYNAME:
		case TI_GETPEERNAME:
			break;
		default:
			TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_OTHER_END,
				"udp_wput_other_end: q %p (%S)",
				q, "iocdatadef");
			putnext(q, mp);
			return;
		}
		switch (mi_copy_state(q, mp, &mp2)) {
		case -1:
			TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_OTHER_END,
				"udp_wput_other_end: q %p (%S)",
				q, "iocdataneg");
			return;
		case MI_COPY_CASE(MI_COPY_IN, 1): {
			/*
			 * Now we have the strbuf structure for TI_GETMYNAME
			 * and TI_GETPEERNAME.  Next we copyout the requested
			 * address and then we'll copyout the strbuf.
			 */
			ipa_t	*ipaddr;
			ipha_t	*ipha;
			udpha_t	*udpha1;
			STRUCT_HANDLE(strbuf, sb);
			STRUCT_SET_HANDLE(sb, ((struct iocblk *)mp->b_rptr)->
			    ioc_flag, (struct strbuf *)mp2->b_rptr);

			if (STRUCT_FGET(sb, maxlen) < (int)sizeof (ipa_t)) {
				mi_copy_done(q, mp, EINVAL);
				TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_OTHER_END,
				    "udp_wput_other_end: q %p (%S)",
				    q, "iocdatashort");
				return;
			}
			/*
			 * Create a message block to hold the addresses for
			 * copying out.
			 */
			mp2 = mi_copyout_alloc(q, mp, STRUCT_FGETP(sb, buf),
			    sizeof (ipa_t));
			if (!mp2) {
				TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_OTHER_END,
					"udp_wput_other_end: q %p (%S)",
					q, "allocbfail");
				return;
			}
			ipaddr = (ipa_t *)mp2->b_rptr;
			bzero((char *)ipaddr, sizeof (ipa_t));
			ipaddr->ip_family = AF_INET;
			switch (((struct iocblk *)mp->b_rptr)->ioc_cmd) {
			case TI_GETMYNAME:
				bcopy((char *)&udp->udp_src,
				    (char *)ipaddr->ip_addr,
				    sizeof (ipaddr->ip_addr));
				bcopy((char *)&udp->udp_port,
				    (char *)ipaddr->ip_port,
				    sizeof (ipaddr->ip_port));
				break;
			case TI_GETPEERNAME:
				ipha = &udp->udp_ipha;
				bcopy((char *)&ipha->ipha_dst,
				    (char *)ipaddr->ip_addr,
				    sizeof (ipaddr->ip_addr));
				udpha1 = udp->udp_udpha;
				bcopy((char *)&udpha1->uha_dst_port,
				    (char *)ipaddr->ip_port,
				    sizeof (ipaddr->ip_port));
				break;
			default:
				mi_copy_done(q, mp, EPROTO);
				TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_OTHER_END,
					"udp_wput_other_end: q %p (%S)",
					q, "default");
				return;
			}
			STRUCT_FSET(sb, len, sizeof (ipa_t));
			mp2->b_wptr = mp2->b_rptr + sizeof (ipa_t);
			/* Copy out the address */
			mi_copyout(q, mp);
			break;
			}
		case MI_COPY_CASE(MI_COPY_OUT, 1):
			/*
			 * The address has been copied out, so now
			 * copyout the strbuf.
			 */
			mi_copyout(q, mp);
			break;
		case MI_COPY_CASE(MI_COPY_OUT, 2):
			/*
			 * The address and strbuf have been copied out.
			 * We're done, so just acknowledge the original
			 * M_IOCTL.
			 */
			mi_copy_done(q, mp, 0);
			break;
		default:
			/*
			 * Something strange has happened, so acknowledge
			 * the original M_IOCTL with an EPROTO error.
			 */
			mi_copy_done(q, mp, EPROTO);
			break;
		}
		TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_OTHER_END,
			"udp_wput_other_end: q %p (%S)", q, "iocdata");
		return;
	default:
		/* Unrecognized messages are passed through without change. */
		break;
	}
	TRACE_2(TR_FAC_UDP, TR_UDP_WPUT_OTHER_END,
		"udp_wput_other_end: q %p (%S)", q, "end");
	putnext(q, mp);
}

static int
udp_unitdata_opt_process(queue_t *q, mblk_t *mp, int *t_errorp)
{
	udp_t	*udp;
	int retval;
	struct T_unitdata_req *udreqp;

	ASSERT(((union T_primitives *)mp->b_rptr)->type);

	udp = (udp_t *)q->q_ptr;

	udreqp = (struct T_unitdata_req *)mp->b_rptr;
	*t_errorp = 0;

	retval = tpi_optcom_buf(q, mp, &udreqp->OPT_length,
	    udreqp->OPT_offset, udp->udp_priv_stream, &udp_opt_obj);

	switch (retval) {
	case OB_SUCCESS:
		return (0);	/* success */
	case OB_BADOPT:
		*t_errorp = TBADOPT;
		break;
	case OB_NOMEM:
		*t_errorp = TSYSERR;
		break;
	case OB_NOACCES:
		*t_errorp = TACCES;
		break;
	case OB_ABSREQ_FAIL:
		/*
		 * no suitable error in t_errno namespace, really
		 */
		*t_errorp = TSYSERR;
		break;
	case OB_INVAL:
		*t_errorp = TSYSERR;
		break;
	}
	return (-1);		/* failure */
}

void
udp_ddi_init(void)
{
	mutex_init(&udp_g_lock, NULL, MUTEX_DEFAULT, NULL);

	udp_max_optbuf_len = optcom_max_optbuf_len(udp_opt_obj.odb_opt_des_arr,
	    udp_opt_obj.odb_opt_arr_cnt);

	(void) udp_param_register(udp_param_arr, A_CNT(udp_param_arr));
}

void
udp_ddi_destroy(void)
{
	nd_free(&udp_g_nd);

	mutex_destroy(&udp_g_lock);
}
