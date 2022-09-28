/*
 * Copyright (c) 1992-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_INET_IP_H
#define	_INET_IP_H

#pragma ident	"@(#)ip.h	1.60	98/01/20 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/isa_defs.h>
#include <sys/strick.h>
#include <sys/types.h>

#define	IP_DEBUG

#ifndef	_IPADDR_T
#define	_IPADDR_T
typedef uint32_t ipaddr_t;
#endif

#define	IP_HOST_MASK		(ipaddr_t)0xffffffffU

#define	IP_CSUM(mp, off, sum)		(~ip_cksum(mp, off, sum) & 0xFFFF)
#define	IP_CSUM_PARTIAL(mp, off, sum)	ip_cksum(mp, off, sum)
#define	IP_BCSUM_PARTIAL(bp, len, sum)	bcksum(bp, len, sum)

/*
 * Flag to IP write side to not compute the TCP/UDP checksums.
 * Stored in ipha_ident (which is otherwise zero).
 */
#define	NO_IP_TP_CKSUM			0xFFFF

#define	ILL_FRAG_HASH_TBL_COUNT	((unsigned int)64)
#define	ILL_FRAG_HASH_TBL_SIZE	(ILL_FRAG_HASH_TBL_COUNT * sizeof (ipfb_t))

#define	IP_DEV_NAME			"/dev/ip"
#define	IP_MOD_NAME			"ip"
#define	IP_ADDR_LEN			4
#define	IP_ARP_PROTO_TYPE		0x0800

#define	IP_VERSION			4
#define	IP_SIMPLE_HDR_LENGTH_IN_WORDS	5
#define	IP_SIMPLE_HDR_LENGTH		20
#define	IP_MAX_HDR_LENGTH		64
/*
 * XXX IP_MAXPACKET is defined in <netinet/ip.h> as well. At some point the
 * 2 files should be cleaned up to remove all redundant definitions.
 */
#define	IP_MAXPACKET			65535
#define	IP_SIMPLE_HDR_VERSION \
	((IP_VERSION << 4) | IP_SIMPLE_HDR_LENGTH_IN_WORDS)

#define	IP_IOCTL			(('i'<<8)|'p')
#define	IP_IOC_IRE_DELETE		4
#define	IP_IOC_IRE_DELETE_NO_REPLY	5
#define	IP_IOC_IRE_ADVISE_NO_REPLY	6
#define	IP_IOC_RTS_REQUEST		7

/* IP Options */
#define	IPOPT_EOL		0x00
#define	IPOPT_NOP		0x01
#define	IPOPT_SEC		0x82
#define	IPOPT_LSRR		0x83
#define	IPOPT_IT		0x44
#define	IPOPT_RR		0x07
#define	IPOPT_SID		0x88
#define	IPOPT_SSRR		0x89

/* Bits in the option value */
#define	IPOPT_COPY		0x80

/* IP option header indexes */
#define	IPOPT_POS_VAL		0
#define	IPOPT_POS_LEN		1
#define	IPOPT_POS_OFF		2
#define	IPOPT_POS_OV_FLG	3

/* Minimum for src and record route options */
#define	IPOPT_MINOFF_SR	4

/* Minimum for timestamp option */
#define	IPOPT_MINOFF_IT	5
#define	IPOPT_MINLEN_IT	5

/* Timestamp option flag bits */
#define	IPOPT_IT_TIME		0	/* Only timestamp */
#define	IPOPT_IT_TIME_ADDR	1	/* Timestamp + IP address */
#define	IPOPT_IT_SPEC		3	/* Only listed routers */
#define	IPOPT_IT_SPEC_BSD	2	/* Defined fopr BSD compat */

#define	IPOPT_IT_TIMELEN	4	/* Timestamp size */

/* Controls forwarding of IP packets, set via ndd */
#define	IP_FORWARD_NEVER	0
#define	IP_FORWARD_ALWAYS	1

#define	WE_ARE_FORWARDING	(ip_g_forward == IP_FORWARD_ALWAYS)

#define	IPH_HDR_LENGTH(ipha)						\
	((int)(((iph_t *)ipha)->iph_version_and_hdr_length & 0xF) << 2)

/*
 * IP reassembly macros.  We hide starting and ending offsets in b_next and
 * b_prev of messages on the reassembly queue.	The messages are chained using
 * b_cont.  These macros are used in ip_reassemble() so we don't have to see
 * the ugly casts and assignments.
 */
#define	IP_REASS_START(mp)		((intptr_t)((mp)->b_next))
#define	IP_REASS_SET_START(mp, u)	((mp)->b_next = (mblk_t *)(u))
#define	IP_REASS_END(mp)		((intptr_t)((mp)->b_prev))
#define	IP_REASS_SET_END(mp, u)		((mp)->b_prev = (mblk_t *)(u))

#define	ATOMIC_32_INC(u1, p)		(u1 = ++(*(p)))
#define	ATOMIC_32_INIT(au32p)
#define	ATOMIC_32_ASSIGN(au32p, val)	((au32p)[0] = (val))
#define	ATOMIC_32_VAL(au32p)		((au32p)[0])

/*
 * Use a wrapping int unless some hardware level
 * instruction offers true atomic inc on 32 bitters.
 * Some compilers have difficulty generating efficient
 * code for 32 bitters.
 */
#ifndef	A_U32
#define	A_U32	uint32_t
#endif

#ifndef	IRE_DB_TYPE
#define	IRE_DB_TYPE	M_SIG
#endif

#ifndef	IRE_DB_REQ_TYPE
#define	IRE_DB_REQ_TYPE	M_PCSIG
#endif

#define	IRE_IS_TARGET(ire)	((ire)&& ((ire)->ire_type !=		\
				IRE_BROADCAST))

/* IP Fragmentation Reassembly Header */
typedef struct ipf_s {
	struct ipf_s	*ipf_hash_next;
	struct ipf_s	**ipf_ptphn;	/* Pointer to previous hash next. */
	u16	ipf_ident;	/* Ident to match. */
	u8	ipf_protocol;	/* Protocol to match. */
	ipaddr_t ipf_src;	/* Source address. */
	time_t	ipf_timestamp;	/* Reassembly start time. */
	mblk_t	*ipf_mp;	/* mblk we live in. */
	mblk_t	*ipf_tail_mp;	/* Frag queue tail pointer. */
	int	ipf_hole_cnt;	/* Number of holes (hard-case). */
	int	ipf_end;	/* Tail end offset (0 implies hard-case). */
	int	ipf_stripped_hdr_len;	/* What we've stripped from the */
					/* lead mblk. */
	u16	ipf_checksum;	/* Partial checksum of fragment data */
	u16	ipf_checksum_valid;
	uint_t	ipf_gen;	/* Frag queue generation */
	size_t	ipf_count;	/* Count of bytes used by frag */
} ipf_t;

/* IP packet count structure */
typedef struct ippc_s {
	ipaddr_t ippc_addr;
	uint_t	ippc_ib_pkt_count;
	uint_t	ippc_ob_pkt_count;
	uint_t	ippc_fo_pkt_count;
} ippc_t;

/* ICMP types */
#define	ICMP_ECHO_REPLY			0
#define	ICMP_DEST_UNREACHABLE		3
#define	ICMP_SOURCE_QUENCH		4
#define	ICMP_REDIRECT			5
#define	ICMP_ECHO_REQUEST		8
#define	ICMP_ROUTER_ADVERTISEMENT	9
#define	ICMP_ROUTER_SOLICITATION	10
#define	ICMP_TIME_EXCEEDED		11
#define	ICMP_PARAM_PROBLEM		12
#define	ICMP_TIME_STAMP_REQUEST		13
#define	ICMP_TIME_STAMP_REPLY		14
#define	ICMP_INFO_REQUEST		15
#define	ICMP_INFO_REPLY			16
#define	ICMP_ADDRESS_MASK_REQUEST	17
#define	ICMP_ADDRESS_MASK_REPLY		18

/* ICMP_TIME_EXCEEDED codes */
#define	ICMP_TTL_EXCEEDED		0
#define	ICMP_REASSEMBLY_TIME_EXCEEDED	1

/* ICMP_DEST_UNREACHABLE codes */
#define	ICMP_NET_UNREACHABLE		0
#define	ICMP_HOST_UNREACHABLE		1
#define	ICMP_PROTOCOL_UNREACHABLE	2
#define	ICMP_PORT_UNREACHABLE		3
#define	ICMP_FRAGMENTATION_NEEDED	4
#define	ICMP_SOURCE_ROUTE_FAILED	5

/* ICMP Header Structure */
typedef struct icmph_s {
	uchar_t	icmph_type;
	uchar_t	icmph_code;
	u16	icmph_checksum;
	union {
		struct { /* ECHO request/response structure */
			u16	u_echo_ident;
			u16	u_echo_seqnum;
		} u_echo;
		struct { /* Destination unreachable structure */
			u16	u_du_zero;
			u16	u_du_mtu;
		} u_du;
		struct { /* Parameter problem structure */
			u8	u_pp_ptr;
			u8	u_pp_rsvd[3];
		} u_pp;
		struct { /* Redirect structure */
			ipaddr_t u_rd_gateway;
		} u_rd;
	} icmph_u;
} icmph_t;
#define	icmph_echo_ident	icmph_u.u_echo.u_echo_ident
#define	icmph_echo_seqnum	icmph_u.u_echo.u_echo_seqnum
#define	icmph_du_zero		icmph_u.u_du.u_du_zero
#define	icmph_du_mtu		icmph_u.u_du.u_du_mtu
#define	icmph_pp_ptr		icmph_u.u_pp.u_pp_ptr
#define	icmph_rd_gateway	icmph_u.u_rd.u_rd_gateway
#define	ICMPH_SIZE	8

/* Unaligned IP header */
typedef struct iph_s {
	uchar_t	iph_version_and_hdr_length;
	uchar_t	iph_type_of_service;
	uchar_t	iph_length[2];
	uchar_t	iph_ident[2];
	uchar_t	iph_fragment_offset_and_flags[2];
	uchar_t	iph_ttl;
	uchar_t	iph_protocol;
	uchar_t	iph_hdr_checksum[2];
	uchar_t	iph_src[4];
	uchar_t	iph_dst[4];
} iph_t;

/* Aligned IP header */
typedef struct ipha_s {
	uchar_t	ipha_version_and_hdr_length;
	uchar_t	ipha_type_of_service;
	u16	ipha_length;
	u16	ipha_ident;
	u16	ipha_fragment_offset_and_flags;
	uchar_t	ipha_ttl;
	uchar_t	ipha_protocol;
	u16	ipha_hdr_checksum;
	ipaddr_t ipha_src;
	ipaddr_t ipha_dst;
} ipha_t;

#define	IPH_DF		0x4000	/* Don't fragment */
#define	IPH_MF		0x2000	/* More fragments to come */
#define	IPH_OFFSET	0x1FFF	/* Where the offset lives */

/* IP Mac info structure */
typedef struct ip_m_s {
	t_uscalar_t 	ip_m_mac_type;	/* From <sys/dlpi.h> */
	int		ip_m_type;	/* From <net/if_types.h> */
	t_uscalar_t	ip_m_sap;
	t_scalar_t	ip_m_sap_length;	/* From <sys/dlpi.h> */
	t_scalar_t	ip_m_brdcst_addr_length;
	uchar_t		*ip_m_brdcst_addr;
} ip_m_t;

/* Internet Routing Entry */
#ifdef _KERNEL
typedef struct ire_s {
	struct	ire_s	*ire_next;	/* The hash chain must be first. */
	struct	ire_s	**ire_ptpn;	/* Pointer to previous next. */
	mblk_t	*ire_mp;		/* mblk we are in. */
	mblk_t	*ire_ll_hdr_mp;		/* Resolver cookie for this IRE. */
	queue_t	*ire_rfq;		/* recv from this queue */
	queue_t	*ire_stq;		/* send to this queue */
	ipaddr_t ire_src_addr;		/* Source address to use. */
	uint_t	ire_max_frag;		/* MTU (next hop or path). */
	u32	ire_frag_flag;		/* IPH_DF or zero. */
	A_U32	ire_atomic_ident;	/* Per IRE IP ident. */
	u32	ire_tire_mark;		/* Saved ident for overtime parking. */
	ipaddr_t ire_mask;		/* Mask for matching this IRE. */
	ipaddr_t ire_addr;		/* Address this IRE represents. */
	ipaddr_t ire_gateway_addr;	/* Gateway address (if IRE_DEFAULT). */
	ushort_t ire_marks;		/* IRE_MARK_{PROTECTED,CONDEMNED} */
	ushort_t ire_type;
	clock_t	ire_rtt;		/* Guestimate in millisecs */
	uint_t	ire_ib_pkt_count;	/* Inbound packets for ire_addr */
	uint_t	ire_ob_pkt_count;	/* Outbound packets to ire_addr */
	uint_t	ire_ll_hdr_length;	/* Non-zero if we do M_DATA prepends */
	time_t	ire_create_time;	/* Time (in secs) IRE was created. */
	mblk_t	*ire_ll_hdr_saved_mp;	/* For SIOCGARP whe using fastpath */
	u32	ire_chandle;		/* Handle to associate prefix routes */
					/* to cache entries. */
	ipaddr_t ire_cmask;		/* mask from parent prefix route */
	struct ipif_s	*ire_ipif;	/* the interface that this ire uses */
	kmutex_t ire_ident_lock;	/* Protects ire_atomic_ident */
	u32	ire_flags;		/* flags related to route(RTF_*) */
	uint32_t ire_ssthresh;		/* Slow start threshold (for TCP) */
	clock_t	ire_rtt_sd;		/* Cached value of RTT variance */
} ire_t;
#endif

/* Router entry types */
#define	IRE_BROADCAST		0x0001	/* Route entry for broadcast address */
#define	IRE_DEFAULT		0x0002	/* Route entry for default gateway */
#define	IRE_LOCAL		0x0004	/* Route entry for local address */
#define	IRE_LOOPBACK		0x0008	/* Route entry for loopback address */
#define	IRE_PREFIX		0x0010	/* Route entry for prefix routes  */
#define	IRE_CACHE		0x0020	/* Cached Route entry */
#define	IRE_IF_NORESOLVER	0x0040	/* Route entry for local interface */
					/* net without any address mapping. */
#define	IRE_IF_RESOLVER		0x0080	/* Route entry for local interface */
					/* net with resolver. */
#define	IRE_HOST		0x0100	/* Host route entry */
#define	IRE_HOST_REDIRECT	0x0200	/* Host route entry from redirects */

#define	IRE_INTERFACE		(IRE_IF_NORESOLVER | IRE_IF_RESOLVER)
#define	IRE_CACHETABLE		(IRE_CACHE | IRE_BROADCAST | IRE_LOCAL | \
				IRE_LOOPBACK)

#define	IRE_MARK_PROTECTED	0x0001	/* Protected from untimely deletes. */
#define	IRE_MARK_CONDEMNED	0x0002  /* Protected, but should be deleted. */

/* Flags with ire_expire routine */

#define	FLUSH_CACHE_TIME	0x0001
#define	FLUSH_REDIRECT_TIME	0x0002
#define	FLUSH_MTU_TIME		0x0004

/* Arguments to ire_flush_cache() */

#define	IRE_FLUSH_DELETE	0
#define	IRE_FLUSH_ADD		1

/*
 * Open/close synchronization flags.
 * These are kept in a separate field in the ipc/ill and the synchronization
 * depends on the atomic 32 bit access to that field.
 */
#define	IPCF_CLOSING		0x01	/* ip_close waiting for ip_wsrv */
#define	IPCF_CLOSE_DONE		0x02	/* ip_wsrv signalling ip_close */
#define	IPCF_OPENING		0x04	/* ip_open waiting for ip_wsrv */
#define	IPCF_OPEN_DONE		0x08	/* ip_wsrv signalling ip_open */

/* Group membership list per upper ill */
typedef struct ilg_s {
	ipaddr_t	ilg_group;
	struct ipif_s	*ilg_lower;	/* Interface we are member on */
} ilg_t;

/* Multicast address list entry for lower ill */
typedef struct ilm_s {
	ipaddr_t	ilm_addr;
	int		ilm_refcnt;
	uint_t		ilm_timer;	/* IGMP */
	struct ipif_s	*ilm_ipif;	/* Back pointer to ipif */
	struct ilm_s	*ilm_next;	/* Linked list for each ill */
	uint_t		ilm_state;	/* state of the membership */
} ilm_t;

/* IP client structure, allocated when ip_open indicates a STREAM device */
#ifdef	_KERNEL
typedef struct ipc_s {
	struct	ipc_s	*ipc_hash_next; /* Hash chain must be first */
	struct	ipc_s	**ipc_ptphn;	/* Pointer to previous hash next. */
	queue_t	*ipc_rq;
	queue_t	*ipc_wq;
	kmutex_t *ipc_fanout_lock;	/* hash bucket lock when ptphn set */
	struct	ill_s	*ipc_bind_ill;

	ipaddr_t ipc_laddr;	/* Local address */
	ipaddr_t ipc_faddr;	/* Remote address. 0 => not connected */
	union {
		struct {
			u16	ipcu_fport;	/* Remote port */
			u16	ipcu_lport;	/* Local port */
		} ipcu_ports1;
		u32		ipcu_ports2;	/* Rem port, local port */
					/* Used for TCP_MATCH performance */
	} ipc_ipcu;
#define	ipc_lport	ipc_ipcu.ipcu_ports1.ipcu_lport
#define	ipc_fport	ipc_ipcu.ipcu_ports1.ipcu_fport
#define	ipc_ports	ipc_ipcu.ipcu_ports2

	kmutex_t	ipc_reflock;	/* Protects ipc_refcnt */
	ushort_t	ipc_refcnt;	/* Number of pending upstream msg */
	kcondvar_t	ipc_refcv;	/* Wait for refcnt decrease */

	ilg_t	*ipc_ilg;		/* Group memberships */
	int	ipc_ilg_allocated;	/* Number allocated */
	int	ipc_ilg_inuse;		/* Number currently used */
	struct ipif_s	*ipc_multicast_ipif;	/* IP_MULTICAST_IF */
	uint_t	ipc_multicast_ttl;	/* IP_MULTICAST_TTL */
	uint_t	ipc_close_flags;	/* IPCF_* flags for close synch */
	unsigned int
		ipc_dontroute : 1,	/* SO_DONTROUTE state */
		ipc_loopback : 1,	/* SO_LOOPBACK state */
		ipc_broadcast : 1,	/* SO_BROADCAST state */
		ipc_reuseaddr : 1,	/* SO_REUSEADDR state */

		ipc_multicast_loop : 1,	/* IP_MULTICAST_LOOP */
		ipc_multi_router : 1,	/* Wants all multicast packets */
		ipc_priv_stream : 1,	/* Privileged client? */
		ipc_draining : 1,	/* ip_wsrv running */

		ipc_did_putbq : 1,	/* ip_wput did a putbq */
		ipc_routing_sock : 1,	/* ip routing socket */

		ipc_pad_to_bit_31 : 22;
} ipc_t;

/*
 * Macros used when sending data upstream using the fanout lists.
 * Needed to prevent the ipc stream from closing while there
 * is a reference to its queue.
 *
 * The ipc_refcnt does not capture all threads accessing an ipc.
 * Those that are running in ip_open, ip_close, put, or srv in the
 * queues corresponding to the ipc do not hold a refcnt. The refcnt only
 * captures other threads (e.g. the fanout of inbound packets) that
 * need to access the ipc.
 *
 * Note: In order to guard against the hash table changing
 * the caller of IPC_REFHOLD must hold the lock on the hash bucket.
 */
#define	IPC_REFHOLD(ipc) {			\
	ASSERT(ipc->ipc_fanout_lock != NULL);	\
	ASSERT(MUTEX_HELD(ipc->ipc_fanout_lock)); \
	mutex_enter(&(ipc)->ipc_reflock);	\
	(ipc)->ipc_refcnt++;			\
	ASSERT((ipc)->ipc_refcnt != 0);		\
	mutex_exit(&(ipc)->ipc_reflock);	\
}

#define	IPC_REFRELE(ipc) {			\
	mutex_enter(&(ipc)->ipc_reflock);	\
	ASSERT((ipc)->ipc_refcnt != 0);		\
	(ipc)->ipc_refcnt--;			\
	cv_broadcast(&(ipc)->ipc_refcv);	\
	mutex_exit(&(ipc)->ipc_reflock);	\
}

/*
 * The IP Client Fanout structure.
 * The hash tables and their linkage (ipc_hash_next, ipc_hash_ptpn) are
 * protected by the per-bucket icf_lock. Each ipc_t inserted in
 * the list points back at this lock using ipc_fanout_lock.
 */
typedef struct icf_s {
	ipc_t		*icf_ipc;
	kmutex_t	icf_lock;
} icf_t;


/* IP interface structure, one per local address */
typedef struct ipif_s {
	struct ipif_s	*ipif_next;
	struct ill_s	*ipif_ill;	/* Back pointer to our ill */
	int	ipif_id;		/* Logical unit number */
	uint_t	ipif_mtu;		/* Starts at ipif_ill->ill_max_frag */
	ipaddr_t ipif_local_addr;	/* Local IP address for this if. */
	ipaddr_t ipif_net_mask;		/* Net mask for this interface. */
	ipaddr_t ipif_broadcast_addr;	/* Broadcast addr for this interface. */
	ipaddr_t ipif_pp_dst_addr;	/* Point-to-point dest address. */
	uint_t	ipif_flags;		/* Interface flags. */
	uint_t	ipif_metric;		/* BSD if metric, for compatibility. */
	uint_t	ipif_ire_type;		/* LOCAL or LOOPBACK */
	mblk_t	*ipif_arp_down_mp;	/* Allocated at time arp comes up to */
					/* prevent awkward out of mem */
					/* condition later */
	mblk_t  *ipif_arp_on_mp;	/* Allocated at time arp comes up to */
					/* prevent awkward out of mem */
					/* condition later */

	mblk_t	*ipif_saved_ire_mp;	/* Allocated for each extra */
					/* IRE_IF_NORESOLVER/IRE_IF_RESOLVER */
					/* on this interface so that they */
					/* can survive ifconfig down. */

	struct ipif_s	**ipif_ifgrpschednext;	/* Pointer to the pointer of */
						/* the next ipif to be */
						/* scheduled in the ipif's */
						/* interface group.  See */
						/* ip_if.c for more on */
						/* interface groups (ifgrps). */
	struct ipif_s	*ipif_ifgrpnext;	/* Next entry in my interface */
						/* group. */

	/*
	 * The packet counts in the ipif contain the sum of the
	 * packet counts in dead IREs that were affiliated with
	 * this ipif.
	 */
	uint_t	ipif_fo_pkt_count;	/* Forwarded thru our dead IREs */
	uint_t	ipif_ib_pkt_count;	/* Inbound packets for our dead IREs */
	uint_t	ipif_ob_pkt_count;	/* Outbound packets to our dead IREs */
	unsigned int
		ipif_multicast_up : 1,	/* We have joined the allhosts group */
		: 0;
} ipif_t;
/* Macros for easy backreferences to the ill. */
#define	ipif_wq			ipif_ill->ill_wq
#define	ipif_rq			ipif_ill->ill_rq
#define	ipif_net_type		ipif_ill->ill_net_type
#define	ipif_resolver_mp	ipif_ill->ill_resolver_mp
#define	ipif_ipif_up_count	ipif_ill->ill_ipif_up_count
#define	ipif_ipif_pending	ipif_ill->ill_ipif_pending
#define	ipif_bcast_mp		ipif_ill->ill_bcast_mp
#define	ipif_index		ipif_ill->ill_index
#define	ipif_type		ipif_ill->ill_type

/*
 * Fragmentation hash bucket
 */
typedef struct ipfb_s {
	struct ipf_s	*ipfb_ipf;	/* List of ... */
	size_t		ipfb_count;	/* Count of bytes used by frag(s) */
	kmutex_t	ipfb_lock;	/* Protect all ipf in list */
} ipfb_t;

/*
 * IP Lower level Structure.
 * Instance data structure in ip_open when there is a device below us.
 */
typedef struct ill_s {
	struct	ill_s	*ill_next;	/* Chained in at ill_g_head. */
	struct	ill_s	**ill_ptpn;	/* Pointer to previous next. */
	queue_t	*ill_rq;		/* Read queue. */
	queue_t	*ill_wq;		/* Write queue. */

	int	ill_error;		/* Error value sent up by device. */

	ipif_t	*ill_ipif;		/* Interface chain for this ILL. */
	uint_t	ill_ipif_up_count;	/* Number of IPIFs currently up. */
	uint_t	ill_max_frag;		/* Max IDU. */
	char	*ill_name;		/* Our name. */
	uint_t	ill_name_length;	/* Name length, incl. terminator. */
	uint_t	ill_net_type;		/* IRE_IF_RESOLVER/IRE_IF_NORESOLVER. */
	uint_t	ill_ppa;		/* Physical Point of Attachment num. */
	t_uscalar_t	ill_sap;
	t_scalar_t	ill_sap_length;	/* Including sign (for position) */
	uint_t	ill_phys_addr_length;	/* Excluding the sap. Only set when */
					/* the DL provider supports */
					/* broadcast. */

	mblk_t	*ill_frag_timer_mp;	/* Reassembly timer state. */
	ipfb_t	*ill_frag_hash_tbl;	/* Fragment hash list head. */

	queue_t	*ill_bind_pending_q;	/* Queue waiting for DL_BIND_ACK. */
	ipif_t	*ill_ipif_pending;	/* IPIF waiting for DL_BIND_ACK. */

	/*
	 * ill_hdr_length and ill_hdr_mp will be non zero if
	 * the underlying device supports the M_DATA fastpath
	 */
	size_t	ill_hdr_length;

	ilm_t	*ill_ilm;		/* Multicast mebership for lower ill */
	int	ill_multicast_type;	/* type of router which is querier */
					/* on this interface */

	int	ill_multicast_time;	/* # of slow timeouts since last */
					/* old query */

	/*
	 * All non-nil cells between 'ill_first_mp_to_free' and
	 * 'ill_last_mp_to_free' are freed in ill_delete.
	 */
#define	ill_first_mp_to_free	ill_hdr_mp
	mblk_t	*ill_hdr_mp;		/* Contains fastpath template */
	mblk_t	*ill_bcast_mp;		/* DLPI header for broadcasts. */
	mblk_t	*ill_bind_pending;	/* T_BIND_REQ awaiting completion. */
	mblk_t	*ill_resolver_mp;	/* Resolver template. */
	mblk_t	*ill_attach_mp;
	mblk_t	*ill_bind_mp;
	mblk_t	*ill_unbind_mp;
	mblk_t	*ill_detach_mp;
#define	ill_last_mp_to_free	ill_detach_mp

	inetcksum_t ill_ick;		/* Contains returned ick state */

	uint_t	ill_close_flags;	/* IPCF_* flags for open/close synch */
	uint_t
		ill_frag_timer_running : 1,
		ill_needs_attach : 1,
		ill_priv_stream : 1,
		ill_unbind_pending : 1,

		ill_pad_to_bit_31 : 28;
	/*
	 * Used in SIOCSIFMUXID and SIOCGIFMUXID for 'ifconfig unplumb'.
	 */
	int	ill_arp_muxid;		/* muxid returned from plink for arp */
	int	ill_ip_muxid;		/* muxid returned from plink for ip */
	/*
	 * Used for IP frag reassembly throttling on a per ILL basis.
	 *
	 * Note: frag_count is approximate, its added to and subtracted from
	 *	 without any locking, so simultaneous load/modify/stores can
	 *	 collide, also ill_frag_purge() recalculates its value by
	 *	 summing all the ipfb_count's without locking out updates
	 *	 to the ipfb's.
	 */
	uint_t	ill_ipf_gen;		/* Generation of next fragment queue */
	size_t	ill_frag_count;		/* Approx count of all mblk bytes */
	int	ill_index;		/* a unique value for each device */
	int	ill_type;		/* From <net/if_types.h> */
} ill_t;

/* Named Dispatch Parameter Management Structure */
typedef struct ipparam_s {
	uint_t	ip_param_min;
	uint_t	ip_param_max;
	uint_t	ip_param_value;
	char	*ip_param_name;
} ipparam_t;
#endif	/* _KERNEL */

/* Common fields for IP IOCTLs. */
typedef struct ipllcmd_s {
	uint_t	ipllc_cmd;
	uint_t	ipllc_name_offset;
	uint_t	ipllc_name_length;
} ipllc_t;

/* IP IRE Change Command Structure. */
typedef struct ipic_s {
	ipllc_t	ipic_ipllc;
	uint_t	ipic_ire_type;
	uint_t	ipic_max_frag;
	uint_t	ipic_addr_offset;
	uint_t	ipic_addr_length;
	uint_t	ipic_mask_offset;
	uint_t	ipic_mask_length;
	uint_t	ipic_src_addr_offset;
	uint_t	ipic_src_addr_length;
	uint_t	ipic_ll_hdr_offset;
	uint_t	ipic_ll_hdr_length;
	uint_t	ipic_gateway_addr_offset;
	uint_t	ipic_gateway_addr_length;
	clock_t	ipic_rtt;
	uint32_t ipic_ssthresh;
	clock_t	ipic_rtt_sd;
} ipic_t;
#define	ipic_cmd		ipic_ipllc.ipllc_cmd
#define	ipic_ll_name_length	ipic_ipllc.ipllc_name_length
#define	ipic_ll_name_offset	ipic_ipllc.ipllc_name_offset

/* IP IRE Delete Command Structure. */
typedef struct ipid_s {
	ipllc_t	ipid_ipllc;
	uint_t	ipid_ire_type;
	uint_t	ipid_addr_offset;
	uint_t	ipid_addr_length;
	uint_t	ipid_mask_offset;
	uint_t	ipid_mask_length;
} ipid_t;
#define	ipid_cmd		ipid_ipllc.ipllc_cmd

/* IP Address Structure. Identical to struct sockaddr_in */
typedef struct ipa_s {
	u16	ip_family;		/* Presumably AF_INET. */
	uchar_t	ip_port[2];		/* Port in net-byte order. */
	uchar_t	ip_addr[4];		/* Internet addr. in net-byte order. */
	uchar_t	ip_pad[8];
} ipa_t;

/* Address structure used for internal bind with IP */
typedef struct ipa_conn_s {
	ipaddr_t	ac_laddr;
	ipaddr_t	ac_faddr;
	u16		ac_fport;
	u16		ac_lport;
} ipa_conn_t;

#ifdef	_KERNEL
/* Name/Value Descriptor. */
typedef struct nv_s {
	int	nv_value;
	char	*nv_name;
} nv_t;

/* IP Forwarding Ticket */
typedef	struct ipftk_s {
	queue_t	*ipftk_queue;
	ipaddr_t ipftk_dst;
} ipftk_t;

#ifndef _IP_C
extern struct ill_s *ill_g_head;	/* ILL List Head */

extern ill_t	*ip_timer_ill;		/* ILL for IRE expiration timer. */
extern mblk_t	*ip_timer_mp;		/* IRE expiration timer. */
extern time_t	ip_ire_time_elapsed;	/* Time since IRE cache last flushed */

extern ill_t	*igmp_timer_ill;	/* ILL for IGMP timer. */
extern mblk_t	*igmp_timer_mp;	/* IGMP timer */
extern int	igmp_timer_interval;

extern uint_t	ip_def_gateway_count;	/* Number of gateways. */
extern uint_t	ip_def_gateway_index;	/* Walking index used to mod in */
extern ipaddr_t	ip_g_all_ones;

extern caddr_t	ip_g_nd;		/* Named Dispatch List Head */

extern int	ip_max_mtu;			/* Used by udp/icmp */

extern ipparam_t	*ip_param_arr;

#define	ip_g_forward			ip_param_arr[0].ip_param_value

#define	ip_respond_to_address_mask_broadcast ip_param_arr[1].ip_param_value
#define	ip_g_resp_to_address_mask	ip_param_arr[1].ip_param_value
#define	ip_debug			ip_param_arr[7].ip_param_value
#define	ip_mrtdebug			ip_param_arr[8].ip_param_value
#define	ip_timer_interval		ip_param_arr[9].ip_param_value
#define	ip_ire_flush_interval		ip_param_arr[10].ip_param_value
#define	ip_def_ttl			ip_param_arr[12].ip_param_value
#define	ip_path_mtu_discovery		ip_param_arr[18].ip_param_value
#define	ip_ignore_delete_time		ip_param_arr[19].ip_param_value
#define	ip_broadcast_ttl		ip_param_arr[22].ip_param_value
#define	ip_addrs_per_if			ip_param_arr[26].ip_param_value

extern	int	ip_enable_group_ifs;

extern int	dohwcksum;	/* use h/w cksum if supported by the h/w */

extern uint_t	ipif_g_count;			/* Count of IPIFs "up". */

extern nv_t	*ire_nv_tbl;

extern int	ipdevflag;

#endif

/*
 * Network byte order macros
 */
#ifdef	_BIG_ENDIAN
#define	N_IN_CLASSD_NET		IN_CLASSD_NET
#define	N_INADDR_UNSPEC_GROUP	INADDR_UNSPEC_GROUP
#else /* _BIG_ENDIAN */
#define	N_IN_CLASSD_NET		(ipaddr_t)0x000000f0U
#define	N_INADDR_UNSPEC_GROUP	(ipaddr_t)0x000000e0U
#endif /* _BIG_ENDIAN */
#define	CLASSD(addr)	(((addr) & N_IN_CLASSD_NET) == N_INADDR_UNSPEC_GROUP)

#ifdef IP_DEBUG
#include <sys/debug.h>
#include <sys/promif.h>

#define	ip0dbg(a)	printf a
#define	ip1dbg(a)	if (ip_debug > 2) printf a
#define	ip2dbg(a)	if (ip_debug > 3) printf a
#define	ip3dbg(a)	if (ip_debug > 4) printf a
#define	ipcsumdbg(a, b)	if (ip_debug == 1) prom_printf(a); \
	else if (ip_debug > 1) {prom_printf("mp=%p\n", b); debug_enter(a); }
#else
#define	ip0dbg(a)	/* */
#define	ip1dbg(a)	/* */
#define	ip2dbg(a)	/* */
#define	ip3dbg(a)	/* */
#define	ipcsumdbg(a, b)	/* */
#endif	/* IP_DEBUG */

extern	void	icmp_time_exceeded(queue_t *q, mblk_t *mp, int code);

extern	uint_t	ip_cksum(mblk_t *mp, int offset, uint32_t prev_sum);
extern	uint_t	ip_csum_hdr(ipha_t *ipha);
extern	void	ipc_hash_insert_connected(icf_t *, ipc_t *);
extern	void	ipc_hash_insert_bound(icf_t *, ipc_t *);
extern	void	ipc_hash_insert_wildcard(icf_t *, ipc_t *);

extern	mblk_t	*ip_dlpi_alloc(size_t len, t_uscalar_t prim);

extern	char	*ip_dot_addr(ipaddr_t addr, char *buf);

extern	mblk_t	*ip_forwarding_ticket(queue_t *q, ipaddr_t dst);

extern	int	ip_ipc_report(queue_t *q, mblk_t *mp, void *arg);

extern	ipaddr_t ip_massage_options(ipha_t *ipha);

extern	ipaddr_t ip_net_mask(ipaddr_t addr);

extern	char 	*ip_nv_lookup(nv_t *nv, int value);

extern	void	ip_rput(queue_t *q, mblk_t *mp);

extern	void	ip_rput_forward(ire_t *ire, ipha_t *ipha, mblk_t *mp);

extern	void	ip_rput_forward_multicast(ipaddr_t dst, mblk_t *mp,
						ipif_t *ipif);

extern	void	ip_rput_local(queue_t *q, mblk_t *mp, ipha_t *ipha,
				ire_t *ire);

extern	void	ip_wput(queue_t *q, mblk_t *mp);

extern	void	ip_wput_ire(queue_t *q, mblk_t *mp, ire_t *ire);

extern	void	ip_wput_local(queue_t *q, ipha_t *ipha, mblk_t *mp,
				int ire_type, queue_t *rq);

extern	void	ip_wput_multicast(queue_t *q, mblk_t *mp, ipif_t *ipif);

extern	void	ipc_walk(pfv_t func, void *arg);

#endif	/* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* _INET_IP_H */
