/*
 * Copyright (c) 1992-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_INET_TCP_H
#define	_INET_TCP_H

#pragma ident	"@(#)tcp.h	1.31	98/01/06 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Private (and possibly temporary) ioctl used by configuration code
 * to lock in the "default" stream for detached closes.
 */
#define	TCP_IOC_DEFAULT_Q	(('T' << 8) + 51)

/* #define	TCP_NODELAY	1 */
/* #define	TCP_MAXSEG	2 */

/* TCP states */
#define	TCPS_CLOSED		-6
#define	TCPS_IDLE		-5	/* idle (opened, but not bound) */
#define	TCPS_BOUND		-4	/* bound, ready to connect or accept */
#define	TCPS_LISTEN		-3	/* listening for connection */
#define	TCPS_SYN_SENT		-2	/* active, have sent syn */
#define	TCPS_SYN_RCVD		-1	/* have received syn (and sent ours) */
/* states < TCPS_ESTABLISHED are those where connections not established */
#define	TCPS_ESTABLISHED	0	/* established */
#define	TCPS_CLOSE_WAIT		1	/* rcvd fin, waiting for close */
/* states > TCPS_CLOSE_WAIT are those where user has closed */
#define	TCPS_FIN_WAIT_1		2	/* have closed and sent fin */
#define	TCPS_CLOSING		3	/* closed, xchd FIN, await FIN ACK */
#define	TCPS_LAST_ACK		4	/* had fin and close; await FIN ACK */
/* states > TCPS_CLOSE_WAIT && < TCPS_FIN_WAIT_2 await ACK of FIN */
#define	TCPS_FIN_WAIT_2		5	/* have closed, fin is acked */
#define	TCPS_TIME_WAIT		6	/* in 2*msl quiet wait after close */

/* Bit values in 'th_flags' field of the TCP packet header */
#define	TH_FIN			0x01	/* Sender will not send more */
#define	TH_SYN			0x02	/* Synchronize sequence numbers */
#define	TH_RST			0x04	/* Reset the connection */
#define	TH_PSH			0x08	/* This segment requests a push */
#define	TH_ACK			0x10	/* Acknowledgement field is valid */
#define	TH_URG			0x20	/* Urgent pointer field is valid */
/*
 * Internal flags used in conjunction with the packet header flags above.
 * Used in tcp_rput to keep track of what needs to be done.
 */
#define	TH_ACK_ACCEPTABLE	0x0400
#define	TH_XMIT_NEEDED		0x0800	/* Window opened - send queued data */
#define	TH_REXMIT_NEEDED	0x1000	/* Time expired for unacked data */
#define	TH_ACK_NEEDED		0x2000	/* Send an ack now. */
#define	TH_NEED_SACK_REXMIT	0x4000	/* Use SACK info to retransmission */
#define	TH_TIMER_NEEDED 0x8000	/* Start the delayed ack/push bit timer */
#define	TH_ORDREL_NEEDED	0x10000	/* Generate an ordrel indication */
#define	TH_MARKNEXT_NEEDED	0x20000	/* Data should have MSGMARKNEXT */
#define	TH_SEND_URP_MARK	0x40000	/* Send up tcp_urp_mark_mp */

#include <sys/inttypes.h>

#define	TCP_HDR_LENGTH(tcph) (((tcph)->th_offset_and_rsrvd[0] >>2) &(0xF << 2))
#define	TCP_MAX_COMBINED_HEADER_LENGTH	(64 + 64) /* Maxed out ip + tcp */
#define	TCP_MAX_IP_OPTIONS_LENGTH	(64 - IP_SIMPLE_HDR_LENGTH)
#define	TCP_MAX_HDR_LENGTH		64
#define	TCP_MAX_TCP_OPTIONS_LENGTH	(64 - sizeof (tcph_t))
#define	TCP_MIN_HEADER_LENGTH		20

#define	TCPIP_HDR_LENGTH(mp, n)					\
	(n) = IPH_HDR_LENGTH((mp)->b_rptr),			\
	(n) += TCP_HDR_LENGTH((tcph_t *)&(mp)->b_rptr[(n)])

/*
 * TCP sequence numbers are 32 bit integers operated
 * on with modular arithmetic.  These macros can be
 * used to compare such integers.
 */
#define	SEQ_LT(a, b)	((int32_t)((a)-(b)) < 0)
#define	SEQ_LEQ(a, b)	((int32_t)((a)-(b)) <= 0)
#define	SEQ_GT(a, b)	((int32_t)((a)-(b)) > 0)
#define	SEQ_GEQ(a, b)	((int32_t)((a)-(b)) >= 0)

/* TCP Protocol header */
typedef	struct tcphdr_s {
	uint8_t		th_lport[2];	/* Source port */
	uint8_t		th_fport[2];	/* Destination port */
	uint8_t		th_seq[4];	/* Sequence number */
	uint8_t		th_ack[4];	/* Acknowledgement number */
	uint8_t		th_offset_and_rsrvd[1]; /* Offset to the packet data */
	uint8_t		th_flags[1];
	uint8_t		th_win[2];	/* Allocation number */
	uint8_t		th_sum[2];	/* TCP checksum */
	uint8_t		th_urp[2];	/* Urgent pointer */
} tcph_t;

#define	TCP_HDR_LENGTH(tcph) (((tcph)->th_offset_and_rsrvd[0] >>2) &(0xF << 2))
#define	TCP_MAX_COMBINED_HEADER_LENGTH	(64 + 64) /* Maxed out ip + tcp */
#define	TCP_MAX_IP_OPTIONS_LENGTH	(64 - IP_SIMPLE_HDR_LENGTH)
#define	TCP_MAX_HDR_LENGTH		64
#define	TCP_MAX_TCP_OPTIONS_LENGTH	(64 - sizeof (tcph_t))
#define	TCP_MIN_HEADER_LENGTH		20
#define	TCP_MAXWIN			65535
#define	TCP_MAX_WINSHIFT		14
#define	TCP_MAX_LARGEWIN		(TCP_MAXWIN << TCP_MAX_WINSHIFT)

#define	TCPIP_HDR_LENGTH(mp, n)					\
	(n) = IPH_HDR_LENGTH((mp)->b_rptr),			\
	(n) += TCP_HDR_LENGTH((tcph_t *)&(mp)->b_rptr[(n)])

/* TCP Protocol header (used if the header is known to be 32-bit aligned) */
typedef	struct tcphdra_s {
	in_port_t	tha_lport;	/* Source port */
	in_port_t	tha_fport;	/* Destination port */
	uint32_t	tha_seq;	/* Sequence number */
	uint32_t	tha_ack;	/* Acknowledgement number */
	uint8_t tha_offset_and_reserved; /* Offset to the packet data */
	uint8_t		tha_flags;
	uint16_t	tha_win;	/* Allocation number */
	uint16_t	tha_sum;	/* TCP checksum */
	uint16_t	tha_urp;	/* Urgent pointer */
} tcpha_t;

#include <netinet/tcp.h>
#include <inet/tcp_sack.h>

/*
 * Control structure for each open TCP stream
 * NOTE: tcp_reinit_values MUST have a line for each field in this structure!
 */
typedef struct tcp_s {
	struct	tcp_s	*tcp_bind_hash; /* Bind hash chain */
	struct	tcp_s **tcp_ptpbhn; /* Pointer to previous bind hash next. */
	struct	tcp_s	*tcp_listen_hash; /* Listen hash chain */
	struct	tcp_s **tcp_ptplhn; /* Pointer to previous listen hash next. */
	struct	tcp_s	*tcp_conn_hash; /* Connect hash chain */
	struct	tcp_s **tcp_ptpchn; /* Pointer to previous conn hash next. */
	struct	tcp_s	*tcp_acceptor_hash; /* Acceptor hash chain */
	struct	tcp_s **tcp_ptpahn; /* Pointer to previous accept hash next. */
	struct	tcp_s   *tcp_time_wait_next; /* Pointer to next T/W block */
	struct	tcp_s *tcp_time_wait_prev; /* Pointer to previous T/W next */
	time_t	tcp_time_wait_expire; /* time in seconds when t/w expires */

	int32_t	tcp_state;
	queue_t	*tcp_rq;		/* Our upstream neighbor (client) */
	queue_t	*tcp_wq;		/* Our downstream neighbor */

	/* Fields arranged in approximate access order along main paths */
	mblk_t	*tcp_xmit_head;		/* Head of rexmit list */
	mblk_t	*tcp_xmit_last;		/* last valid data seen by tcp_wput */
	uint32_t tcp_unsent;		/* # of bytes in hand that are unsent */
	mblk_t	*tcp_xmit_tail;		/* Last rexmit data sent */
	uint32_t tcp_xmit_tail_unsent;	/* # of unsent bytes in xmit_tail */

	uint32_t tcp_snxt;		/* Senders next seq num */
	uint32_t tcp_suna;		/* Sender unacknowledged */
	uint32_t tcp_rexmit_nxt;	/* Next rexmit seq num */
	uint32_t tcp_rexmit_max;	/* Max retran seq num */
	int32_t	tcp_snd_burst;		/* Send burst factor */
	uint32_t tcp_swnd;		/* Senders window (relative to suna) */
	uint32_t tcp_cwnd;		/* Congestion window */
	uint32_t tcp_cwnd_cnt;		/* cwnd cnt in congestion avoidance */

	uint32_t tcp_ibsegs;		/* Inbound segments on this stream */
	uint32_t tcp_obsegs;		/* Outbound segments on this stream */

	uint32_t tcp_mss;		/* Max segment size */
	uint32_t tcp_naglim;		/* Tunable nagle limit */
	int32_t	tcp_hdr_len;		/* Byte len of combined TCP/IP hdr */
	tcph_t	*tcp_tcph;		/* tcp header within combined hdr */
	int32_t	tcp_tcp_hdr_len;	/* tcp header len within combined */
	uint32_t	tcp_valid_bits;
#define	TCP_ISS_VALID	0x1	/* Is the tcp_iss seq num active? */
#define	TCP_FSS_VALID	0x2	/* Is the tcp_fss seq num active? */
#define	TCP_URG_VALID	0x4	/* If the tcp_urg seq num active? */


	int32_t	tcp_xmit_hiwater;	/* Send buffer high water mark. */
	mblk_t	*tcp_flow_mp;		/* mp to exert flow control upstream */

	mblk_t	*tcp_timer_mp;		/* Control block for timer service */
	uchar_t	tcp_timer_backoff;	/* Backoff shift count. */
	clock_t tcp_last_recv_time;	/* Last time we receive a segment. */
	clock_t	tcp_dack_set_time;	/* When delayed ACK timer is set. */

	clock_t	tcp_last_rcv_lbolt; /* lbolt on last packet, used for PAWS */

	uint32_t
		tcp_urp_last_valid : 1,	/* Is tcp_urp_last valid? */
		tcp_hard_binding : 1,	/* If we've started a full bind */
		tcp_hard_bound : 1,	/* If we've done a full bind with IP */
		tcp_priv_stream : 1, 	/* If stream was opened by priv user */

		tcp_fin_acked : 1,	/* Has our FIN been acked? */
		tcp_fin_rcvd : 1,	/* Have we seen a FIN? */
		tcp_fin_sent : 1,	/* Have we sent our FIN yet? */
		tcp_ordrel_done : 1,	/* Have we sent the ord_rel upstream? */

		tcp_flow_stopped : 1,	/* Have we flow controlled xmitter? */
		tcp_debug : 1,		/* SO_DEBUG "socket" option. */
		tcp_dontroute : 1,	/* SO_DONTROUTE "socket" option. */
		tcp_broadcast : 1,	/* SO_BROADCAST "socket" option. */

		tcp_useloopback : 1,	/* SO_USELOOPBACK "socket" option. */
		tcp_reuseaddr : 1,	/* SO_REUSEADDR "socket" option. */
		tcp_oobinline : 1,	/* SO_OOBINLINE "socket" option. */
		tcp_dgram_errind : 1,	/* SO_DGRAM_ERRIND option */

		tcp_detached : 1,	/* If we're detached from a stream */
		tcp_bind_pending : 1,	/* Client is waiting for bind ack */
		tcp_unbind_pending : 1, /* Client sent T_UNBIND_REQ */
		tcp_deferred_clean_death : 1,
					/* defer tcp endpoint cleanup etc. */

		tcp_co_wakeq_done : 1,	/* A strwakeq() has been done */
		tcp_co_wakeq_force : 1,	/* A strwakeq() must be done */
		tcp_co_norm : 1,	/* In normal mode, putnext() done */
		tcp_co_wakeq_need : 1,	/* A strwakeq() needs to be done */

		tcp_snd_ws_ok : 1,	/* Received WSCALE from peer */
		tcp_snd_ts_ok : 1,	/* Received TSTAMP from peer */
		tcp_linger : 1,		/* SO_LINGER turned on */
		tcp_zero_win_probe: 1,	/* Zero win probing is in progress */

		tcp_loopback: 1,	/* src and dst are the same machine */
		tcp_localnet: 1,	/* src and dst are on the same subnet */
		tcp_syn_defense: 1,	/* For defense against SYN attack */
#define	tcp_dontdrop	tcp_syn_defense
		tcp_set_timer : 1;

	uint32_t
		tcp_active_open: 1,	/* This is a active open */
		tcp_timeout : 1,	/* qbufcall failed, qtimeout pending */
		tcp_ack_timer_running: 1,	/* Delayed ACK timer running */
		tcp_rexmit : 1,		/* TCP is retransmitting */

		tcp_snd_sack_ok : 1,	/* Can use SACK for this connection */
		tcp_2_junk_fill_thru_bit_31 : 27;

	int32_t	tcp_rcv_ws;		/* My window scale power */
	int32_t	tcp_snd_ws;		/* Sender's window scale power */
	uint32_t tcp_ts_recent;		/* Timestamp of earliest unacked */
					/*  data segment */
	uint32_t tcp_rnxt;		/* Seq we expect to recv next */
	uint32_t tcp_rwnd;		/* Current receive window */
	uint32_t tcp_rwnd_max;		/* Maximum receive window */

	mblk_t	*tcp_reass_head;	/* Out of order reassembly list head */
	mblk_t	*tcp_reass_tail;	/* Out of order reassembly list tail */

	tcp_sack_info_t	*tcp_sack_info;

#define	tcp_pipe	tcp_sack_info->tcp_pipe
#define	tcp_fack	tcp_sack_info->tcp_fack
#define	tcp_sack_snxt	tcp_sack_info->tcp_sack_snxt
#define	tcp_max_sack_blk	tcp_sack_info->tcp_max_sack_blk
#define	tcp_num_sack_blk	tcp_sack_info->tcp_num_sack_blk
#define	tcp_sack_list		tcp_sack_info->tcp_sack_list
#define	tcp_num_notsack_blk	tcp_sack_info->tcp_num_notsack_blk
#define	tcp_cnt_notsack_list	tcp_sack_info->tcp_cnt_notsack_list
#define	tcp_notsack_list		tcp_sack_info->tcp_notsack_list

	mblk_t	*tcp_rcv_head;		/* Queued until push, urgent data or */
	mblk_t	*tcp_rcv_tail;		/* the count exceeds */
	uint32_t tcp_rcv_cnt;		/* tcp_rcv_push_wait. */

	mblk_t	*tcp_co_head;		/* Co (copyout) queue head */
	mblk_t	*tcp_co_tail;		/*  "     "       "   tail */
	mblk_t	*tcp_co_tmp;		/* Co timer mblk */
	/*
	 * Note: tcp_co_imp is used to both indicate the read-side stream
	 *	 data flow state (synchronous/asynchronous) as well as a
	 *	 pointer to a reusable iocblk sized mblk.
	 *
	 *	 The mblk is allocated (if need be) at initialization time
	 *	 and is used by the read-side when a copyout queue eligible
	 *	 mblk arrives (synchronous data flow) but previously one or
	 *	 more mblk(s) have been putnext()ed (asynchronous data flow).
	 *	 In this case, the mblk pointed to by tcp_co_imp is putnext()ed
	 *	 as an M_IOCTL of I_SYNCSTR after first nullifying tcp_co_imp.
	 *	 The streamhead will putnext() the mblk down the write-side
	 *	 stream as an M_IOCNAK of I_SYNCSTR and when (if) it arrives at
	 *	 the write-side its pointer will be saved again in tcp_co_imp.
	 *
	 *	 If an instance of tcp is closed while its tcp_co_imp is null,
	 *	 then the mblk will be freed elsewhere in the stream. Else, it
	 *	 will be freed (close) or saved (reinit) for future use.
	 */
	mblk_t	*tcp_co_imp;		/* Co ioctl mblk */
	clock_t tcp_co_tintrvl;		/* Co timer interval */
	uint32_t tcp_co_rnxt;		/* Co seq we expect to recv next */
	int	tcp_co_cnt;		/* Co enqueued byte count */

	uint32_t tcp_cwnd_ssthresh;	/* Congestion window */
	uint32_t tcp_cwnd_max;
	uint32_t tcp_csuna;		/* Clear (no rexmits in window) suna */

	clock_t	tcp_rto;		/* Round trip timeout */
	clock_t	tcp_rtt_sa;		/* Round trip smoothed average */
	clock_t	tcp_rtt_sd;		/* Round trip smoothed deviation */
	clock_t	tcp_rtt_update;		/* Round trip update(s) */
	clock_t tcp_ms_we_have_waited;	/* Total retrans time */

	uint32_t tcp_swl1;		/* These help us avoid using stale */
	uint32_t tcp_swl2;		/*  packets to update state */

	uint32_t tcp_rack;		/* Seq # we have acked */
	uint32_t tcp_rack_cnt;		/* # of bytes we have deferred ack */
	uint32_t tcp_rack_cur_max;	/* # bytes we may defer ack for now */
	uint32_t tcp_rack_abs_max;	/* # of bytes we may defer ack ever */
	mblk_t	*tcp_ack_mp;		/* Delayed ACK timer block */

	uint32_t tcp_max_swnd;		/* Maximum swnd we have seen */

	struct tcp_s *tcp_listener;	/* Our listener */

	int32_t	tcp_xmit_lowater;	/* Send buffer low water mark. */

	uint32_t tcp_irs;		/* Initial recv seq num */
	uint32_t tcp_iss;		/* Initial send seq num */
	uint32_t tcp_fss;		/* Final/fin send seq num */
	uint32_t tcp_urg;		/* Urgent data seq num */

	int	tcp_ip_hdr_len;		/* Byte len of our current IP header */
	clock_t	tcp_first_timer_threshold;  /* When to prod IP */
	clock_t	tcp_second_timer_threshold; /* When to give up completely */
	clock_t	tcp_first_ctimer_threshold; /* 1st threshold while connecting */
	clock_t tcp_second_ctimer_threshold; /* 2nd ... while connecting */

	int	tcp_lingertime;		/* Close linger time (in seconds) */

	uint32_t tcp_urp_last;		/* Last urp for which signal sent */
	mblk_t	*tcp_urp_mp;		/* T_EXDATA_IND for urgent byte */
	mblk_t	*tcp_urp_mark_mp;	/* zero-length marked/unmarked msg */

	int tcp_conn_req_cnt_q0;	/* # of conn reqs in SYN_RCVD */
	int tcp_conn_req_cnt_q;	/* # of conn reqs in ESTABLISHED */
	int tcp_conn_req_max;	/* # of ESTABLISHED conn reqs allowed */
	t_scalar_t tcp_conn_req_seqnum;	/* Incrementing pending conn req ID */
#define	tcp_ip_addr_cache	tcp_reass_tail
					/* Cache ip addresses that */
					/* complete the 3-way handshake */
	struct tcp_s *tcp_eager_next_q; /* next eager in ESTABLISHED state */
	struct tcp_s *tcp_eager_next_q0; /* next eager in SYN_RCVD state */
	struct tcp_s *tcp_eager_prev_q0; /* prev eager in SYN_RCVD state */
					/* all eagers form a circular list */
	uint32_t tcp_syn_rcvd_timeout;	/* How many SYN_RCVD timeout in q0 */
	union {
	    mblk_t *tcp_eager_conn_ind; /* T_CONN_IND waiting for 3rd ack. */
	    mblk_t *tcp_opts_conn_req; /* T_CONN_REQ w/ options processed */
	} tcp_conn;

	int32_t	tcp_keepalive_intrvl;	/* Zero means don't bother */
	mblk_t	*tcp_keepalive_mp;	/* Timer block for keepalive */

	int32_t	tcp_client_errno;	/* How the client screwed up */

	union {
		iph_t	tcp_u_iph;		/* template ip header */
		ipha_t	tcp_u_ipha;
		char	tcp_u_buf[TCP_MAX_COMBINED_HEADER_LENGTH];
		double	tcp_u_aligner;
	} tcp_u;
#define	tcp_iph		tcp_u.tcp_u_iph
#define	tcp_ipha	tcp_u.tcp_u_ipha
#define	tcp_iphc	tcp_u.tcp_u_buf
	uint32_t tcp_sum;		/* checksum to compensate for source */
					/* routed packets. Host byte order */
	ipaddr_t tcp_remote;		/* true remote address - needed for */
					/* source routing. */
	ipaddr_t tcp_bound_source;	/* IP address in bind_req */
	uint16_t tcp_last_sent_len;	/* Record length for nagle */
	uint16_t tcp_dupack_cnt;	/* # of consequtive duplicate acks */
	inetcksum_t tcp_ill_ick;	/* Underlying ill ick (if any), may */
					/* be incorrect, but cause no harm. */
	/*
	 * These fields contain the same information as tcp_tcph->th_*port.
	 * However, the lookup functions can not use the header fields
	 * since during IP option manipulation the tcp_tcph pointer
	 * changes.
	 */
	union {
		struct {
			in_port_t	tcpu_fport;	/* Remote port */
			in_port_t	tcpu_lport;	/* Local port */
		} tcpu_ports1;
		uint32_t		tcpu_ports2;	/* Rem port, */
							/* local port */
					/* Used for TCP_MATCH performance */
	} tcp_tcpu;
#define	tcp_lport	tcp_tcpu.tcpu_ports1.tcpu_lport
#define	tcp_fport	tcp_tcpu.tcpu_ports1.tcpu_fport
#define	tcp_ports	tcp_tcpu.tcpu_ports2

	kmutex_t	tcp_reflock;	/* Protects tcp_refcnt */
	ushort_t	tcp_refcnt;	/* Number of pending upstream msg */
	kcondvar_t	tcp_refcv;	/* Wait for refcnt decrease */

	kmutex_t	*tcp_bind_lockp;	/* Ptr to tf_lock */
	kmutex_t	*tcp_listen_lockp;	/* Ptr to tf_lock */
	kmutex_t	*tcp_conn_lockp;	/* Ptr to tf_lock */
	kmutex_t	*tcp_acceptor_lockp;	/* Ptr to tf_lock */

	timeout_id_t	tcp_ordrelid;		/* qbufcall/qtimeout id */
	t_uscalar_t	tcp_acceptor_id;	/* ACCEPTOR_id */
} tcp_t;

extern int	tcpdevflag;

#ifdef	__cplusplus
}
#endif

#endif	/* _INET_TCP_H */
