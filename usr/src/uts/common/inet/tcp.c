/*
 * Copyright (c) 1992-1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)tcp.c	1.242	98/01/21 SMI"

const char tcp_version[] = "@(#)tcp.c	1.242	98/01/21 SMI";

#include <sys/types.h>
#include <sys/stream.h>
#include <sys/strsubr.h>
#include <sys/stropts.h>
#include <sys/strlog.h>
#define	_SUN_TPI_VERSION 2
#include <sys/tihdr.h>
#include <sys/timod.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/suntpi.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/vtrace.h>
#include <sys/kmem.h>
#include <sys/ethernet.h>
#include <sys/cpuvar.h>

#include <sys/errno.h>
#include <sys/cmn_err.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/isa_defs.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include <inet/common.h>
#include <inet/ip.h>
#include <inet/mi.h>
#include <inet/mib2.h>
#include <inet/nd.h>
#include <inet/optcom.h>
#include <inet/snmpcom.h>
#include <inet/md5.h>
#include <inet/tcp.h>

/*
 * Synchronization notes:
 *
 * The TCP module uses a combination of per queue pair STREAMS perimeters
 * and explicit locks to synchronize.
 *
 * The perimeters protect the per-connection state (each q_ptr == tcp_t
 * structure). Each tcp_t structure is associated with some queue i.e. some
 * STREAMS perimeter even if the tcp_t does not correspond to an open file
 * descriptor. The two cases when such detached connections occur is
 * as part of new connections before they have been accepted by the application
 * through a T_CONN_RES message, and after the file descriptor and stream
 * has been closed but the TCP state is still (re)transmitting packets.
 * In these cases the queue/perimeter is either that of the listener or
 * the default tcp queue (tcp_g_q).
 *
 * As a result of this the code in tcp_accept_swap and tcp_detach
 * has to change the association between tcp_t structures and queues/perimeters.
 * This relies on the various locks in TCP as well as order of instructions.
 * Note that tcp_accept_swap acquires tcp_mi_lock while holding a fanout
 * lock. This is the only place where both locks are held hence this
 * defines the locking hirarchy: a fanout lock can never be acquired while
 * holding the tcp_mi_lock.
 *
 * The explicit locks in TCP handle the global data structures such as the
 * 4 different hashes for looking up TCP connections (with one mutex per
 * hash bucket), the global list
 * maintained by mi_open_link and friends etc. There are also less used
 * locks to protect the tcp_g_q, the random number generator, the list
 * of detached TIME_WAIT connections, and the host_param data structures.
 *
 * Note that no locks are held when calling putnext() or put().
 * If locks are held we must use putq() and have the service procedure continue
 * the processing. However, this is no longer needed in tcp_rput*.
 * Note that it is safe to call putnext and put after having done a TCP_REFHOLD
 * since only tcp_close will wait for the reference count to drop.
 *
 * At any point in the code where we want to do a putnext into a queue other
 * than the one that we rode in on, we use the macro lateral_putnext rather
 * than call putnext directly.  In multithread streams environments that need
 * to do explicit dance steps to enter a particular stream, this macro can
 * be redefined to do the necessary synchronization work, or call a routine
 * to do it.  Note that we pass both the queue we are currently processing
 * and the one we want to hand to putnext, since they may be in the same,
 * or indeed the same queue, and we leave it to the implementation dependent
 * code to figure it out.
 *
 * Note that while walking tthe tcp mi list the tcp_mi_lock is dropped after
 * putting a reference hold on a tcp instance which will guarantee that tcp
 * won't blow away the mi instance data until after the reference release.
 */
#ifndef	lateral_putnext
#define	lateral_putnext(ourq, nextq, mp)	putnext(nextq, mp)
#endif
#ifndef	lateral_put
#define	lateral_put(ourq, nextq, mp)	put(nextq, mp)
#endif


/*
 * KLUDGE ALERT: a general purpose Synchronous STREAMS solution is needed
 *		 to deal with the switch over from normal STREAMS mode to
 *		 synchronous mode. For now the following ioctl is used.
 *
 * Synchronous STREAMS M_IOCTL is used by a read-side synchronous barrier
 * queue to synchronize with the streamhead the arrival of all previously
 * putnext()ed mblks. When the M_IOCTL mblk is processed by the streamhead
 * it will be sent back down the write-side as a M_IOCNAK.
 */
#define	I_SYNCSTR	-1		/* The unsupported ioctl */

/* Macros for timestamp comparisons */
#define	TSTMP_GEQ(a, b)	((int32_t)((a)-(b)) >= 0)
#define	TSTMP_LT(a, b)	((int32_t)((a)-(b)) < 0)

/*
 * Parameters for TCP Initial Sequence Number (ISS) generation.  The
 * ISS is calculated by adding two components: a time component which
 * grows by 128,000 every second; and an "extra" component that grows
 * either by 64,000 or by a random amount centered approximately on
 * 64,000, for every connection.  This causes the the ISS generator to
 * cycle every 9 hours if no TCP connections are made, and faster if
 * connections are made.
 *
 * A third method for generating ISS is prescribed by Steve Bellovin.
 * This involves adding time, the 64,000 per connection, and a
 * one-way hash (MD5) of the connection ID <sport, dport, src, dst>, a
 * "truly" random (per RFC 1750) number, and a console-entered password.
 */
#define	ISS_INCR (125*1024)
#define	ISS_NSEC_DIV  (8000)

static uint32_t tcp_iss_incr_extra;	/* Incremented for each connection */
static kmutex_t tcp_iss_key_lock;
static MD5_CTX tcp_iss_key;

/*
 * This implementation follows the 4.3BSD interpretation of the urgent
 * pointer and not RFC 1122. Switching to RFC 1122 behavior would cause
 * incompatible changes in protocols like telnet and rlogin.
 */
#define	TCP_OLD_URP_INTERPRETATION	1

#define	TCP_IS_DETACHED(tcp)		((tcp)&&((tcp)->tcp_detached))

/*
 * TCP reassembly macros.  We hide starting and ending sequence numbers in
 * b_next and b_prev of messages on the reassembly queue.  The messages are
 * chained using b_cont.  These macros are used in tcp_reass() so we don't
 * have to see the ugly casts and assignments.
 */
#define	TCP_REASS_SEQ(mp)		((uint32_t)((mp)->b_next))
#define	TCP_REASS_SET_SEQ(mp, u)	((mp)->b_next = (mblk_t *)(u))
#define	TCP_REASS_END(mp)		((uint32_t)((mp)->b_prev))
#define	TCP_REASS_SET_END(mp, u)	((mp)->b_prev = (mblk_t *)(u))

#define	TCP_TIMER_RESTART(tcp, intvl)			\
	mi_timer((tcp)->tcp_wq, (tcp)->tcp_timer_mp, (intvl))

#define	TCP_XMIT_LOWATER	2048
#define	TCP_XMIT_HIWATER	8192
#define	TCP_RECV_LOWATER	2048
#define	TCP_RECV_HIWATER	8192

/* IPPROTO_TCP User Set/Get Options */
#define	TCP_NOTIFY_THRESHOLD		0x10
#define	TCP_ABORT_THRESHOLD		0x11
#define	TCP_CONN_NOTIFY_THRESHOLD	0x12
#define	TCP_CONN_ABORT_THRESHOLD	0x13

/*
 *  PAWS needs a timer for 24 days.  This is the number of ticks in 24 days
 */
#define	PAWS_TIMEOUT	((clock_t)(24*24*60*60*hz))

#ifndef	STRMSGSZ
#define	STRMSGSZ	4096
#endif

/*
 * Note that in TCP_CONN_HASH() it would be more correctly coded as
 * ... % tcp_conn_fanout_size.  However, since we know it will always
 * be a power of two size, we can use the alternate form
 * ... & (tcp_conn_fanout_size - 1)
 * faddr should point to the remote IP address.
 * ports should point to the <remote port, local port>
 */
#define	IB2U(ptr, off)	((unsigned)(((u_char *)(ptr))[off]))

#define	TCP_CONN_HASH(faddr, ports)					\
	((IB2U(ports, 1) ^ IB2U(ports, 0) ^ IB2U(ports, 2) ^ IB2U(ports, 3) ^ \
	IB2U(faddr, 3) ^ 						\
	((IB2U(ports, 0) ^ IB2U(ports, 2) ^ IB2U(faddr, 2)) << 10) ^	\
	((IB2U(faddr, 1)) << 6)) & (tcp_conn_fanout_size - 1))

#define	TCP_BIND_HASH(lport)						\
		((IB2U(lport, 0) ^ IB2U(lport, 1)) % A_CNT(tcp_bind_fanout))
#define	TCP_LISTEN_HASH(lport)						\
		((IB2U(lport, 0) ^ IB2U(lport, 1)) % A_CNT(tcp_listen_fanout))
#ifdef	_ILP32
#define	TCP_ACCEPTOR_HASH(accid)					\
		(((uint)(accid) >> 8) % A_CNT(tcp_acceptor_fanout))
#else
#define	TCP_ACCEPTOR_HASH(accid)					\
		((uint)(accid) % A_CNT(tcp_acceptor_fanout))
#endif	/* _ILP32 */

#define	IP_ADDR_CACHE_SIZE	2048
#define	IP_ADDR_CACHE_HASH(faddr)					\
	((faddr) & (IP_ADDR_CACHE_SIZE -1))


/* Hash for HSPs uses all 32 bits, since both networks and hosts are in table */
#define	TCP_HSP_HASH_SIZE 256

#define	TCP_HSP_HASH(addr)  					\
	(((addr>>24) ^ (addr >>16) ^ 			\
	    (addr>>8) ^ (addr)) % TCP_HSP_HASH_SIZE)

/*
 * TCP options struct returned from tcp_parse_options.
 */
typedef struct tcp_opt_s {
	uint32_t	tcp_opt_mss;
	uint32_t	tcp_opt_wscale;
	uint32_t	tcp_opt_ts_val;
	uint32_t	tcp_opt_ts_ecr;
	tcp_t		*tcp;
} tcp_opt_t;

/*
 * RFC1323-recommended phrasing of TSTAMP option, for easier parsing
 */

#ifdef _BIG_ENDIAN
#define	TCPOPT_NOP_NOP_TSTAMP ((TCPOPT_NOP << 24) | (TCPOPT_NOP << 16) | \
	(TCPOPT_TSTAMP << 8) | 10)
#else
#define	TCPOPT_NOP_NOP_TSTAMP ((10 << 24) | (TCPOPT_TSTAMP << 16) | \
	(TCPOPT_NOP << 8) | TCPOPT_NOP)
#endif

/*
 * Flags returned from tcp_parse_options.
 */
#define	TCP_OPT_MSS_PRESENT	1
#define	TCP_OPT_WSCALE_PRESENT	2
#define	TCP_OPT_TSTAMP_PRESENT	4
#define	TCP_OPT_SACK_OK_PRESENT	8
#define	TCP_OPT_SACK_PRESENT	16

/* TCP option length */
#define	TCPOPT_NOP_LEN		1
#define	TCPOPT_MAXSEG_LEN	4
#define	TCPOPT_WS_LEN		3
#define	TCPOPT_REAL_WS_LEN	(TCPOPT_WS_LEN+1)
#define	TCPOPT_TSTAMP_LEN	10
#define	TCPOPT_REAL_TS_LEN	(TCPOPT_TSTAMP_LEN+2)
#define	TCPOPT_SACK_OK_LEN	2
#define	TCPOPT_REAL_SACK_OK_LEN	(TCPOPT_SACK_OK_LEN+2)
#define	TCPOPT_REAL_SACK_LEN	4
#define	TCPOPT_MAX_SACK_LEN	36
#define	TCPOPT_HEADER_LEN	2

/* TCP cwnd burst factor. */
#define	TCP_CWND_INFINITE	65535
#define	TCP_CWND_SS		3

/*
 * The TCP Fanout structure.
 * The hash tables and their linkage (tcp_*_hash_next, tcp_ptp*hn) are
 * protected by the per-bucket tf_lock. Each tcp_t inserted in
 * the list points back at this lock using tcp_*_lockp.
 */
typedef struct tf_s {
	tcp_t		*tf_tcp;
	kmutex_t	tf_lock;
} tf_t;


/*
 * Macros used when sending data upstream using the hashes.
 * Needed to prevent the tcp stream from closing while there
 * is a reference to its queue.
 *
 * The tcp_refcnt does not capture all threads accessing an tcp.
 * Those that are running in tcp_open, tcp_close, put, or srv in the
 * queues corresponding to the tcp do not hold a refcnt. The refcnt only
 * captures other threads (e.g. the fanout of inbound packets) that
 * need to access the tcp.
 *
 * tcp_refcnt is initialized to 1 to count the one reference from
 * tcp_g_head. When mi_close_unlink is called that refcnt is released.
 * This handles freeing detached structures when the last reference is
 * gone.
 *
 * tcp_close waits for the refcnt to drop to 1 before proceeding
 * to free the tcp_t. Also, tcp_detach will wait for the reference count to
 * drop to one before returning in order to prevent threads accessing the
 * old tcp_rq/wq that are being closed.
 *
 * Note: In order to guard against the hash table changing
 * the caller of TCP_REFHOLD must hold the lock on the hash bucket.
 * However, the macro can not assert on this since it doesn't
 * know which of the tcp_*_lockp that applies.
 */
#define	TCP_REFHOLD(tcp) {			\
	mutex_enter(&(tcp)->tcp_reflock);	\
	(tcp)->tcp_refcnt++;			\
	ASSERT((tcp)->tcp_refcnt != 0);		\
	mutex_exit(&(tcp)->tcp_reflock);	\
}

/* tcp_inactive destroys the mutex thus no mutex_exit is needed. */
#define	TCP_REFRELE(tcp) {			\
	mutex_enter(&(tcp)->tcp_reflock);	\
	ASSERT((tcp)->tcp_refcnt != 0);		\
	if (--(tcp)->tcp_refcnt == 0)		\
		tcp_inactive(tcp);		\
	else {					\
		cv_broadcast(&(tcp)->tcp_refcv);\
		mutex_exit(&(tcp)->tcp_reflock);\
	}					\
}

/* Named Dispatch Parameter Management Structure */
typedef struct tcpparam_s {
	u_int	tcp_param_min;
	u_int	tcp_param_max;
	u_int	tcp_param_val;
	char	*tcp_param_name;
} tcpparam_t;

/* TCP Timer control structure */
typedef struct tcpt_s {
	pfv_t	tcpt_pfv;	/* The routine we are to call */
	tcp_t	*tcpt_tcp;	/* The parameter we are to pass in */
} tcpt_t;

/* TCP Keepalive Timer Block */
typedef struct tcpka_s {
	tcpt_t	tcpka_tcpt;
	int32_t	tcpka_last_intrvl;	/* last probe interval */
	u_char	tcpka_probe_sent;	/* # of probe sent without ACK */
} tcpka_t;
#define	tcpka_tcp	tcpka_tcpt.tcpt_tcp

/* Host Specific Parameter structure */
typedef struct tcp_hsp {
	struct tcp_hsp	*tcp_hsp_next;
	ipaddr_t	tcp_hsp_addr;
	ipaddr_t	tcp_hsp_subnet;
	int32_t		tcp_hsp_sendspace;
	int32_t		tcp_hsp_recvspace;
	int32_t		tcp_hsp_tstamp;
} tcp_hsp_t;

int	tcp_random(void);
static	void	tcp_accept(queue_t *q, mblk_t *mp);
static	void	tcp_accept_comm(tcp_t *listener, tcp_t *acceptor,
    mblk_t *cr_pkt);
static void	tcp_accept_swap(tcp_t *listener, tcp_t *acceptor,
			tcp_t *eager);
static	int	tcp_adapt(tcp_t *tcp, mblk_t *ire_mp);
static char	*tcp_addr_sprintf(char *c, uint8_t *addr);
static	in_port_t tcp_bindi(tcp_t *tcp, in_port_t port, u_char *addr,
    int reuseaddr, int bind_to_req_port_only, int user_specified);
static	int	tcp_close(queue_t *q, int flag);
static	void	tcp_closei_local(tcp_t *tcp);
static	void	tcp_inactive(tcp_t *tcp);
static void	tcp_close_detached(tcp_t *tcp);
static boolean_t tcp_conn_con(tcp_t *tcp, iph_t *iph, tcph_t *tcph);
static	void	tcp_connect(queue_t *q, mblk_t *mp);
static void	tcp_conn_request(tcp_t *tcp, mblk_t *mp);
static	int	tcp_clean_death(tcp_t *tcp, int err);
static	void	tcp_def_q_set(queue_t *q, mblk_t *mp);
static boolean_t tcp_detach(tcp_t *tcp);
static	void	tcp_disconnect(queue_t *q, mblk_t *mp);
static	char	*tcp_display(tcp_t *tcp);
static	boolean_t tcp_eager_blowoff(tcp_t *listener, t_scalar_t seqnum);
static	void	tcp_eager_cleanup(tcp_t *listener, int q0_only);
static	void	tcp_eager_unlink(tcp_t *tcp);
static	void	tcp_err_ack(queue_t *q, mblk_t *mp, int tlierr,
    int unixerr);
static	void	tcp_err_ack_prim(queue_t *q, mblk_t *mp, int primitive,
    int tlierr, int unixerr);
static int	tcp_extra_priv_ports_get(queue_t *q, mblk_t *mp, caddr_t cp);
static int	tcp_extra_priv_ports_add(queue_t *q, mblk_t *mp,
    char *value, caddr_t cp);
static int	tcp_extra_priv_ports_del(queue_t *q, mblk_t *mp,
    char *value, caddr_t cp);
static	int	tcp_tpistate(tcp_t *tcp);
static	void	tcp_bind_hash_insert(tf_t *tf, tcp_t *tcp,
    int caller_holds_lock);
static	void	tcp_bind_hash_remove(tcp_t *tcp);
static	void	tcp_listen_hash_insert(tf_t *tf, tcp_t *tcp);
static	void	tcp_listen_hash_remove(tcp_t *tcp);
static	void	tcp_conn_hash_insert(tf_t *tf, tcp_t *tcp,
    int caller_holds_lock);
static	void	tcp_conn_hash_remove(tcp_t *tcp);
static	tcp_t	*tcp_acceptor_hash_lookup(t_uscalar_t id);
static	void	tcp_acceptor_hash_insert(t_uscalar_t id, tcp_t *tcp);
static	void	tcp_acceptor_hash_remove(tcp_t *tcp);
static	void	tcp_capability_req(tcp_t *tcp, mblk_t *mp);
static	void	tcp_info_req(tcp_t *tcp, mblk_t *mp);
static	void	tcp_addr_req(tcp_t *tcp, mblk_t *mp);
static	int	tcp_init(tcp_t *tcp, queue_t *q, mblk_t *timer_mp,
    mblk_t *tcp_ack_mp, mblk_t *flow_mp, mblk_t *co_tmp, mblk_t *co_imp);
static	void	tcp_init_values(tcp_t *tcp);
static	mblk_t	*tcp_ip_bind_mp(tcp_t *tcp, t_scalar_t bind_prim,
	t_scalar_t addr_length);
static	void	tcp_ip_notify(tcp_t *tcp);
static	int	tcp_ip_unbind(queue_t *wq);
static	mblk_t	*tcp_ire_mp(mblk_t *mp);
static	void    tcp_iss_init(tcp_t *tcp);
static	void	tcp_keepalive_killer(tcp_t *tcp);
static	void	tcp_lift_anchor(tcp_t *tcp);
static	tcp_t	*tcp_lookup(iph_t *iph, tcph_t *tcph, int min_state,
    queue_t **rqp);
static	tcp_t	*tcp_lookup_match(tf_t *tf, uint8_t *lport, uint8_t *laddr,
    uint8_t *fport, uint8_t *faddr, int min_state);
static tcp_t	*tcp_lookup_listener(uint8_t *lport, uint8_t *laddr);
static tcp_t	*tcp_lookup_reversed(iph_t *iph, tcph_t *tcph,
    int min_state);
static void	tcp_maxpsz_set(tcp_t *tcp);
static int	tcp_parse_options(tcph_t *tcph, tcp_opt_t *tcpopt);
static void	tcp_mss_set(tcp_t *tcp, uint32_t size);
static	int	tcp_open(queue_t *q, dev_t *devp, int flag, int sflag,
    cred_t *credp);
static tcp_t	*tcp_open_detached(queue_t *q);
static int	tcp_conprim_opt_process(queue_t *q, mblk_t *mp,
    int *do_disconnectp, int *t_errorp, int *sys_errorp);
static	boolean_t tcp_allow_connopt_set(int level, int name);
int	tcp_opt_default(queue_t *q, int level, int name, u_char *ptr);
int	tcp_opt_get(queue_t *q, int level, int name, u_char *ptr);
static	int	tcp_opt_get_user(iph_t *iph, u_char *ptr);
int	tcp_opt_set(queue_t *q, u_int mgmt_flags, int level, int name,
	u_int inlen, u_char *invalp, u_int *outlenp, u_char *outvalp);
static void	tcp_opt_reverse(tcp_t *tcp, iph_t *iph);
static int	tcp_opt_set_header(tcp_t *tcp, int checkonly,
    u_char *ptr, u_int len);
static int	tcp_param_get(queue_t *q, mblk_t *mp, caddr_t cp);
static boolean_t tcp_param_register(tcpparam_t *tcppa, int cnt);
static int	tcp_param_set(queue_t *q, mblk_t *mp, char *value,
    caddr_t cp);
static int	tcp_1948_phrase_set(queue_t *q, mblk_t *mp, char *value,
    caddr_t cp);
static mblk_t	*tcp_reass(tcp_t *tcp, mblk_t *mp, uint32_t start,
    u_int *flagsp);
static void	tcp_reass_elim_overlap(tcp_t *tcp, mblk_t *mp);
static void	tcp_reinit(tcp_t *tcp);
static void	tcp_reinit_values(tcp_t *tcp);
static void	tcp_report_item(mblk_t *mp, tcp_t *tcp, int hashval,
    tcp_t *thisstream);

static void	tcp_rput(queue_t *q, mblk_t *mp);
static void	tcp_rput_data(queue_t *q, mblk_t *mp, int isput);
static void	tcp_rput_other(queue_t *q, mblk_t *mp);
static void	tcp_rsrv(queue_t *q);
static	int	tcp_rwnd_set(tcp_t *tcp, uint32_t rwnd);
static	int	tcp_snmp_get(queue_t *q, mblk_t *mpctl);
static	int	tcp_snmp_set(queue_t *q, int level, int name, u_char *ptr,
    int len);
static int	tcp_snmp_state(tcp_t *tcp);
static	int	tcp_status_report(queue_t *q, mblk_t *mp, caddr_t cp);
static	int	tcp_bind_hash_report(queue_t *q, mblk_t *mp, caddr_t cp);
static	int	tcp_listen_hash_report(queue_t *q, mblk_t *mp, caddr_t cp);
static	int	tcp_conn_hash_report(queue_t *q, mblk_t *mp, caddr_t cp);
static	int	tcp_acceptor_hash_report(queue_t *q, mblk_t *mp, caddr_t cp);
static int	tcp_host_param_set(queue_t *q, mblk_t *mp, char *value,
    caddr_t cp);
static	int	tcp_host_param_report(queue_t *q, mblk_t *mp,
    caddr_t cp);
static	void	tcp_timer(tcp_t *tcp);
static mblk_t	*tcp_timer_alloc(tcp_t *tcp, pfv_t func, int extra);
static	in_port_t tcp_update_next_port(in_port_t port);
static	void	tcp_wput(queue_t *q, mblk_t *mp);
static	void	tcp_wput_slow(tcp_t *tcp, mblk_t *mp);
static	void	tcp_wput_flush(queue_t *q, mblk_t *mp);
static	void	tcp_wput_iocdata(queue_t *q, mblk_t *mp);
static	void	tcp_wput_ioctl(queue_t *q, mblk_t *mp);
static	void	tcp_wput_proto(queue_t *q, mblk_t *mp);
static	void	tcp_wsrv(queue_t *q);
static int	tcp_xmit_end(tcp_t *tcp);
static	void	tcp_xmit_listeners_reset(queue_t *rq, mblk_t *mp);

static mblk_t	*tcp_xmit_mp(tcp_t *, mblk_t *, int32_t, int32_t *,
	mblk_t **, uint32_t, int32_t);
static void	tcp_ack_timer(tcp_t *tcp);
static mblk_t	*tcp_ack_mp(tcp_t *tcp);
static void	tcp_xmit_early_reset(char *str, queue_t *q, mblk_t *mp,
    uint32_t seq, uint32_t ack, int ctl);
static void	tcp_xmit_ctl(char *str, tcp_t *tcp, mblk_t *mp,
    uint32_t seq, uint32_t ack, int ctl);
static	void	tcp_co_drain(tcp_t *tcp);
static	void	tcp_co_timer(tcp_t *tcp);
static	int	tcp_rinfop(queue_t *q, infod_t *dp);
static	int	tcp_rrw(queue_t *q, struiod_t *dp);
static	int	tcp_winfop(queue_t *q, infod_t *dp);
static	int	tcp_wrw(queue_t *q, struiod_t *dp);
static tcp_hsp_t *tcp_hsp_lookup(ipaddr_t addr);

static void	strwakeqclr(queue_t *q, int flag);
static int	struio_ioctl(queue_t *rq, mblk_t *mp);
static int	setmaxps(queue_t *q, int maxpsz);
static void	tcp_set_rto(tcp_t *, time_t);

static struct module_info tcp_rinfo =  {
#define	MODULE_ID	5105
	MODULE_ID, "tcp", 0, INFPSZ, TCP_RECV_HIWATER, TCP_RECV_LOWATER
};

static struct module_info tcp_winfo =  {
#define	MODULE_ID	5105
	MODULE_ID, "tcp", 0, INFPSZ, 127, 16
};

static struct qinit rinit = {
	(pfi_t)tcp_rput, (pfi_t)tcp_rsrv, tcp_open, tcp_close, (pfi_t)0,
	&tcp_rinfo, NULL, tcp_rrw, tcp_rinfop, STRUIOT_IP
};

static struct qinit winit = {
	(pfi_t)tcp_wput, (pfi_t)tcp_wsrv, (pfi_t)0, (pfi_t)0, (pfi_t)0,
	&tcp_winfo, NULL, tcp_wrw, tcp_winfop, STRUIOT_IP
};

struct streamtab tcpinfo = {
	&rinit, &winit
};

int tcpdevflag = 0;

/* Protected by tcp_g_q_lock */
static	queue_t	*tcp_g_q;	/* Default queue used during detached closes */
kmutex_t tcp_g_q_lock;

/* Protected by tcp_mi_lock */
static	void	*tcp_g_head;	/* Head of TCP instance data chain */
kmutex_t tcp_mi_lock;

/* Protected by tcp_hsp_lock */
/*
 * XXX The host param mechanism should go away and instead we should use
 * the metrics associated with the routes to determine the default sndspace
 * and rcvspace.
 */
static tcp_hsp_t 	**tcp_hsp_hash;	/* Hash table for HSPs */
krwlock_t tcp_hsp_lock;

/*
 * Extra privileged ports. In host byte order.
 * Protected by tcp_epriv_port_lock.
 */
#define	TCP_NUM_EPRIV_PORTS	64
static int	tcp_g_num_epriv_ports = TCP_NUM_EPRIV_PORTS;
static	u16	tcp_g_epriv_ports[TCP_NUM_EPRIV_PORTS] = { 2049, 4045 };
kmutex_t tcp_epriv_port_lock;

/* Only modified during _init and _fini thus no locking is needed. */
static	caddr_t	tcp_g_nd;	/* Head of 'named dispatch' variable list */

/* Hint not protected by any lock */
static	u_int	tcp_next_port_to_try;

/*
 * For scalability, we must not run a timer for every TCP connection
 * in TIME_WAIT state.  To see why, consider:
 * 	1000 connections/sec * 240 seconds/time wait = 240,000 active conn's
 *
 * This list is ordered by time, so you need only delete from the head
 * until you get to entries which aren't old enough to delete yet.
 * The list consists of only the detached TIME_WAIT connections.
 *
 * Note that the timer (tcp_time_wait_expire) is started when the tcp_t
 * becomes detached TIME_WAIT (either by changing the state and already
 * being detached or the other way around). This means that the time_wait
 * state can be extended (up to doubled) if the connection doesn't become
 * detached for a long time.
 *
 * The list manipulations (includingt tcp_time_wait_next/prev)
 * are protected by the tcp_time_wait_lock. The content of the
 * detached TIME_WAIT connections is protected by the normal perimeters.
 */
kmutex_t	tcp_time_wait_lock;	/* Protects the next 3 globals */
static	tcp_t	*tcp_time_wait_head;
static	tcp_t	*tcp_time_wait_tail;
static	timeout_id_t	tcp_time_wait_tid;	/* qtimeout id */

/* TCP bind hash list - all tcp_t with state >= BOUND. */
static	tf_t	tcp_bind_fanout[256];
/* TCP listen hash list - all tcp_t that has been in LISTEN state. */
static	tf_t	tcp_listen_fanout[256];
/* TCP queue hash list - all tcp_t in case they will be an acceptor. */
static	tf_t	tcp_acceptor_fanout[256];
/* TCP connection hash list - contains all tcp_t in connected states. */
static	tf_t	*tcp_conn_fanout;
static	int	tcp_conn_fanout_size;	/* Size of tcp_conn_fanout */

/*
 * /etc/system variable that can be modified to set the tcp_conn_fanout_size.
 * It is rounded up to the nearest power of two and will be at least 256.
 */
#define	TCP_CONN_HASH_SIZE	256	/* Default */
int tcp_conn_hash_size = TCP_CONN_HASH_SIZE;


/*
 * MIB-2 stuff for SNMP
 * Note: tcpInErrs {tcp 15} is accumulated in ip.c
 */
static	mib2_tcp_t	tcp_mib;	/* SNMP fixed size info */
extern	mib2_ip_t	ip_mib;

/*
 * Object to represent database of options to search passed to
 * {sock,tpi}optcom_req() interface routine to take care of option
 * management and associated methods.
 * XXX These and other externs should ideally move to a TCP header
 */
extern optdb_obj_t	tcp_opt_obj;
extern u_int 		tcp_max_optbuf_len;

/*
 * Following assumes TPI alignment requirements stay along 32 bit
 * boundaries
 */
#define	ROUNDUP32(x) \
	(((x) + (sizeof (int32_t) - 1)) & ~(sizeof (int32_t) - 1))

/* Template for response to info request. */
static	struct T_info_ack tcp_g_t_info_ack = {
	T_INFO_ACK,		/* PRIM_type */
	0,			/* TSDU_size */
	T_INFINITE,		/* ETSDU_size */
	T_INVALID,		/* CDATA_size */
	T_INVALID,		/* DDATA_size */
	sizeof (ipa_t),		/* ADDR_size */
	0,			/* OPT_size - not initialized here */
	STRMSGSZ,		/* TIDU_size */
	T_COTS_ORD,		/* SERV_type */
	TCPS_IDLE,		/* CURRENT_state */
	(XPG4_1|EXPINLINE)	/* PROVIDER_flag */
	};

#define	MS	1L
#define	SECONDS	(1000 * MS)
#define	MINUTES	(60 * SECONDS)
#define	HOURS	(60 * MINUTES)
#define	DAYS	(24 * HOURS)

#define	PARAM_MAX (~(uint32_t)0)

/* Max size IP datagram is 64k - 1 */
#define	TCP_MSS_MAX	((64 * 1024 - 1) - (sizeof (iph_t) + sizeof (tcph_t)))

/* Largest TCP port number */
#define	TCP_MAX_PORT	(64 * 1024 - 1)

/*
 * All of these are alterable, within the min/max values given, at run time.
 * Note that the default value of "tcp_close_wait_interval" is four minutes,
 * per the TCP spec.
 */
/* BEGIN CSTYLED */
static	tcpparam_t	tcp_param_arr[] = {
 /*min		max		value		name */
 { 1*SECONDS,	10*MINUTES,	4*MINUTES,	"tcp_close_wait_interval"},
 { 1,		PARAM_MAX,	128,		"tcp_conn_req_max_q" },
 { 0,		PARAM_MAX,	1024,		"tcp_conn_req_max_q0" },
 { 1,		1024,		1,		"tcp_conn_req_min" },
 { 0*MS,	20*SECONDS,	0*MS,		"tcp_conn_grace_period" },
 { 128, 	(1<<30),	256*1024,	"tcp_cwnd_max" },
 { 0,		10,		0,		"tcp_debug" },
 { 1024,	(32*1024),	1024,		"tcp_smallest_nonpriv_port"},
 { 1*SECONDS,	PARAM_MAX,	3*MINUTES,	"tcp_ip_abort_cinterval"},
 { 1*SECONDS,	PARAM_MAX,	3*MINUTES,	"tcp_ip_abort_linterval"},
 { 500*MS,	PARAM_MAX,	8*MINUTES,	"tcp_ip_abort_interval"},
 { 1*SECONDS,	PARAM_MAX,	10*SECONDS,	"tcp_ip_notify_cinterval"},
 { 500*MS,	PARAM_MAX,	10*SECONDS,	"tcp_ip_notify_interval"},
 { 1,		255,		255,		"tcp_ip_ttl"},
 { 10*SECONDS,	10*DAYS,	2*HOURS,	"tcp_keepalive_interval"},
 { 1,		100,		2,		"tcp_maxpsz_multiplier" },
 { 1,		TCP_MSS_MAX,	536,		"tcp_mss_def"},
 { 1,		TCP_MSS_MAX,	TCP_MSS_MAX,	"tcp_mss_max"},
 { 1,		TCP_MSS_MAX,	1,		"tcp_mss_min"},
 { 1,		(64*1024)-1,	(4*1024)-1,	"tcp_naglim_def"},
 { 1*MS,	20*SECONDS,	3*SECONDS,	"tcp_rexmit_interval_initial"},
 { 1*MS,	2*HOURS,	240*SECONDS,	"tcp_rexmit_interval_max"},
 { 1*MS,	2*HOURS,	200*MS,		"tcp_rexmit_interval_min"},
 { 0,		256,		32,		"tcp_wroff_xtra" },
 { 1*MS,	1*MINUTES,	50*MS,		"tcp_deferred_ack_interval" },
 { 0,		16,		0,		"tcp_snd_lowat_fraction" },
 { 0,		128000,		0,		"tcp_sth_rcv_hiwat" },
 { 0,		128000,		0,		"tcp_sth_rcv_lowat" },
 { 1,		10000,		3,		"tcp_dupack_fast_retransmit" },
 { 0,		1,		0,		"tcp_ignore_path_mtu" },
 { 0,		128*1024,	16384,		"tcp_rcv_push_wait" },
 { 1024,	TCP_MAX_PORT,	32*1024,	"tcp_smallest_anon_port"},
 { 1024,	TCP_MAX_PORT,	TCP_MAX_PORT,	"tcp_largest_anon_port"},
 { TCP_XMIT_HIWATER, (1<<30), TCP_XMIT_HIWATER,"tcp_xmit_hiwat"},
 { TCP_XMIT_LOWATER, (1<<30), TCP_XMIT_LOWATER,"tcp_xmit_lowat"},
 { TCP_RECV_HIWATER, (1<<30), TCP_RECV_HIWATER,"tcp_recv_hiwat"},
 { 1,		65536,		4,		"tcp_recv_hiwat_minmss"},
 { 1*SECONDS,	PARAM_MAX,	675*SECONDS,	"tcp_fin_wait_2_flush_interval"},
 { 0,		TCP_MSS_MAX,	64,		"tcp_co_min"},
 { 8192,	(1<<30),	1024*1024,	"tcp_max_buf"},
 { 1,		PARAM_MAX,	1,		"tcp_zero_win_probesize"},
/*
 * Question:  What default value should I set for tcp_strong_iss?
 */
 { 0,		2,		1,		"tcp_strong_iss"},
 { 0,		65536,		12,		"tcp_rtt_updates"},
 { 0,		1,		0,		"tcp_wscale_always"},
 { 0,		1,		0,		"tcp_tstamp_always"},
 { 0,		1,		0,		"tcp_tstamp_if_wscale"},
 { 0*MS,	2*HOURS,	0*MS,		"tcp_rexmit_interval_extra"},
 { 0,		16,		8,		"tcp_deferred_acks_max"},
 { 1,		16384,		2,		"tcp_slow_start_after_idle"},
 { 1,		4,		2,		"tcp_slow_start_initial"},
 { 10*MS,	50*MS,		20*MS,		"tcp_co_timer_interval"},
 { 0,		2,		1,		"tcp_sack_permitted"},
/*
 * tcp_drop_oob MUST be last in the list. This variable is only used
 * when using tcp to test another tcp. The need for it will go away
 * once we have packet shell scripts to test urgent pointers.
 */
#ifdef DEBUG
 { 0,		1,		0,		"tcp_drop_oob"},
#endif
};
/* END CSTYLED */

#define	tcp_close_wait_interval			tcp_param_arr[0].tcp_param_val
#define	tcp_conn_req_max_q			tcp_param_arr[1].tcp_param_val
#define	tcp_conn_req_max_q0			tcp_param_arr[2].tcp_param_val
#define	tcp_conn_req_min			tcp_param_arr[3].tcp_param_val
#define	tcp_conn_grace_period			tcp_param_arr[4].tcp_param_val
#define	tcp_cwnd_max_				tcp_param_arr[5].tcp_param_val
#define	tcp_dbg					tcp_param_arr[6].tcp_param_val
#define	tcp_smallest_nonpriv_port		tcp_param_arr[7].tcp_param_val
#define	tcp_ip_abort_cinterval			tcp_param_arr[8].tcp_param_val
#define	tcp_ip_abort_linterval			tcp_param_arr[9].tcp_param_val
#define	tcp_ip_abort_interval			tcp_param_arr[10].tcp_param_val
#define	tcp_ip_notify_cinterval			tcp_param_arr[11].tcp_param_val
#define	tcp_ip_notify_interval			tcp_param_arr[12].tcp_param_val
#define	tcp_ip_ttl				tcp_param_arr[13].tcp_param_val
#define	tcp_keepalive_interval			tcp_param_arr[14].tcp_param_val
#define	tcp_maxpsz_multiplier			tcp_param_arr[15].tcp_param_val
#define	tcp_mss_def				tcp_param_arr[16].tcp_param_val
#define	tcp_mss_max				tcp_param_arr[17].tcp_param_val
#define	tcp_mss_min				tcp_param_arr[18].tcp_param_val
#define	tcp_naglim_def				tcp_param_arr[19].tcp_param_val
#define	tcp_rexmit_interval_initial		tcp_param_arr[20].tcp_param_val
#define	tcp_rexmit_interval_max			tcp_param_arr[21].tcp_param_val
#define	tcp_rexmit_interval_min			tcp_param_arr[22].tcp_param_val
#define	tcp_wroff_xtra				tcp_param_arr[23].tcp_param_val
#define	tcp_deferred_ack_interval		tcp_param_arr[24].tcp_param_val
#define	tcp_snd_lowat_fraction			tcp_param_arr[25].tcp_param_val
#define	tcp_sth_rcv_hiwat			tcp_param_arr[26].tcp_param_val
#define	tcp_sth_rcv_lowat			tcp_param_arr[27].tcp_param_val
#define	tcp_dupack_fast_retransmit		tcp_param_arr[28].tcp_param_val
#define	tcp_ignore_path_mtu			tcp_param_arr[29].tcp_param_val
#define	tcp_rcv_push_wait			tcp_param_arr[30].tcp_param_val
#define	tcp_smallest_anon_port			tcp_param_arr[31].tcp_param_val
#define	tcp_largest_anon_port			tcp_param_arr[32].tcp_param_val
#define	tcp_xmit_hiwat				tcp_param_arr[33].tcp_param_val
#define	tcp_xmit_lowat				tcp_param_arr[34].tcp_param_val
#define	tcp_recv_hiwat				tcp_param_arr[35].tcp_param_val
#define	tcp_recv_hiwat_minmss			tcp_param_arr[36].tcp_param_val
#define	tcp_fin_wait_2_flush_interval		tcp_param_arr[37].tcp_param_val
#define	tcp_co_min				tcp_param_arr[38].tcp_param_val
#define	tcp_max_buf				tcp_param_arr[39].tcp_param_val
#define	tcp_zero_win_probesize			tcp_param_arr[40].tcp_param_val
#define	tcp_strong_iss				tcp_param_arr[41].tcp_param_val
#define	tcp_rtt_updates				tcp_param_arr[42].tcp_param_val
#define	tcp_wscale_always			tcp_param_arr[43].tcp_param_val
#define	tcp_tstamp_always			tcp_param_arr[44].tcp_param_val
#define	tcp_tstamp_if_wscale			tcp_param_arr[45].tcp_param_val
#define	tcp_rexmit_interval_extra		tcp_param_arr[46].tcp_param_val
#define	tcp_deferred_acks_max			tcp_param_arr[47].tcp_param_val
#define	tcp_slow_start_after_idle		tcp_param_arr[48].tcp_param_val
#define	tcp_slow_start_initial			tcp_param_arr[49].tcp_param_val
#define	tcp_co_timer_interval			tcp_param_arr[50].tcp_param_val
#define	tcp_sack_permitted			tcp_param_arr[51].tcp_param_val
#ifdef DEBUG
#define	tcp_drop_oob				tcp_param_arr[52].tcp_param_val
#else
#define	tcp_drop_oob				0
#endif

int zerocopy_prop;

#ifdef ZC_TEST
extern int noswcksum;
#endif

/*
 * Remove a connection from the list of detached TIME_WAIT connections.
 */
static void
tcp_time_wait_remove(tcp_t *tcp)
{
	if (tcp->tcp_time_wait_expire == 0) {
		ASSERT(tcp->tcp_time_wait_next == NULL);
		ASSERT(tcp->tcp_time_wait_prev == NULL);
		return;
	}
	ASSERT(TCP_IS_DETACHED(tcp));
	ASSERT(tcp->tcp_state == TCPS_TIME_WAIT);
	mutex_enter(&tcp_time_wait_lock);
	if (tcp == tcp_time_wait_head) {
		ASSERT(tcp->tcp_time_wait_prev == NULL);
		tcp_time_wait_head = tcp->tcp_time_wait_next;
		if (tcp_time_wait_head) {
			tcp_time_wait_head->tcp_time_wait_prev = NULL;
		} else {
			tcp_time_wait_tail = NULL;
		}
	} else if (tcp == tcp_time_wait_tail) {
		ASSERT(tcp != tcp_time_wait_head);
		ASSERT(tcp->tcp_time_wait_next == NULL);
		tcp_time_wait_tail = tcp->tcp_time_wait_prev;
		ASSERT(tcp_time_wait_tail != NULL);
		tcp_time_wait_tail->tcp_time_wait_next = NULL;
	} else {
		ASSERT(tcp->tcp_time_wait_prev->tcp_time_wait_next == tcp);
		ASSERT(tcp->tcp_time_wait_next->tcp_time_wait_prev == tcp);
		tcp->tcp_time_wait_prev->tcp_time_wait_next =
		    tcp->tcp_time_wait_next;
		tcp->tcp_time_wait_next->tcp_time_wait_prev =
		    tcp->tcp_time_wait_prev;
	}
	tcp->tcp_time_wait_next = NULL;
	tcp->tcp_time_wait_prev = NULL;
	tcp->tcp_time_wait_expire = 0;
	mutex_exit(&tcp_time_wait_lock);
}

/*
 * Add a connection to the list of detached TIME_WAIT connections
 * and set its time to expire ...
 */
static void
tcp_time_wait_append(tcp_t *tcp)
{
	(void) drv_getparm(TIME, &tcp->tcp_time_wait_expire);
	tcp->tcp_time_wait_expire += (tcp_close_wait_interval/1000);

	ASSERT(TCP_IS_DETACHED(tcp));
	ASSERT(tcp->tcp_state == TCPS_TIME_WAIT);
	ASSERT(tcp->tcp_time_wait_next == NULL);
	ASSERT(tcp->tcp_time_wait_prev == NULL);
	mutex_enter(&tcp_time_wait_lock);
	if (tcp_time_wait_head == NULL) {
		ASSERT(tcp_time_wait_tail == NULL);
		tcp_time_wait_head = tcp;
	} else {
		ASSERT(tcp_time_wait_tail != NULL);
		ASSERT(tcp_time_wait_tail->tcp_state == TCPS_TIME_WAIT);
		tcp_time_wait_tail->tcp_time_wait_next = tcp;
		tcp->tcp_time_wait_prev = tcp_time_wait_tail;
	}
	tcp_time_wait_tail = tcp;
	mutex_exit(&tcp_time_wait_lock);
}

/*
 * Do the necessary cleanup when a connection becomes a detached TIME_WAIT
 */
void
tcp_time_wait_set(tcp_t *tcp)
{
	ASSERT(tcp->tcp_co_tintrvl == -1);
	ASSERT(tcp->tcp_co_tmp != NULL);
	mi_timer_free(tcp->tcp_co_tmp);
	tcp->tcp_co_tmp = NULL;
	if (tcp->tcp_co_imp != NULL) {
		freemsg(tcp->tcp_co_imp);
		tcp->tcp_co_imp = NULL;
	}
	mi_timer_stop(tcp->tcp_timer_mp);
	mi_timer_free(tcp->tcp_timer_mp);
	tcp->tcp_timer_mp = NULL;
	mi_timer_stop(tcp->tcp_ack_mp);
	mi_timer_free(tcp->tcp_ack_mp);
	tcp->tcp_ack_mp = NULL;
	ASSERT(tcp->tcp_flow_stopped == 0);
	ASSERT(tcp->tcp_flow_mp == NULL);
}

/*
 * Periodic qtimeout routine run on the default queue.
 */
/* ARGSUSED */
void
tcp_time_wait_collector(void *arg)
{
	tcp_t *tcp;
	time_t now;

	(void) drv_getparm(TIME, &now);
	mutex_enter(&tcp_time_wait_lock);
	tcp_time_wait_tid = 0;
	while ((tcp = tcp_time_wait_head) != NULL) {
		if (now < tcp->tcp_time_wait_expire) {
			break;
		}
		mutex_exit(&tcp_time_wait_lock);
		(void) tcp_clean_death(tcp, 0);
		mutex_enter(&tcp_time_wait_lock);
	}
	if (tcp_g_q != NULL) {
		tcp_time_wait_tid = qtimeout(tcp_g_q,
		    tcp_time_wait_collector, NULL, drv_usectohz(1000000));
	}
	mutex_exit(&tcp_time_wait_lock);
}

/*
 * Reply to a clients T_CONN_RES TPI message
 */
static void
tcp_accept(queue_t *q, mblk_t *mp)
{
	tcp_t	*listener = (tcp_t *)q->q_ptr;
	tcp_t	*acceptor;
	tcp_t	*eager;
	struct T_conn_res	*tcr;
	t_uscalar_t	acceptor_id;
	t_scalar_t	seqnum;
	mblk_t	*bind_mp;
	mblk_t	*ok_mp;
	mblk_t	*discon_mp;
	mblk_t	*mp1;

	tcr = (struct T_conn_res *)mp->b_rptr;

	if ((mp->b_wptr - mp->b_rptr) < sizeof (*tcr)) {
		tcp_err_ack(q, mp, TSYSERR, EPROTO);
		return;
	}

	/*
	 * Under ILP32 the stream head points tcr->ACCEPTOR_id at the
	 * read side queue of the streams device underneath us i.e. the
	 * read side queue of 'ip'. Since we can't deference QUEUE_ptr we
	 * look it up in the queue_hash.  Under LP64 it sends down the
	 * minor_t of the accepting endpoint.
	 *
	 * Once the acceptor/eager are modified (in tcp_accept_swap) the
	 * fanout hash lock is held.
	 * This prevents any thread from entering the acceptor queue from
	 * below (since it has not been hard bound yet i.e. any inbound
	 * packets will arrive on the listener or default tcp queue and
	 * go through tcp_lookup).
	 * The TCP_REFHOLD will prevent the acceptor from closing.
	 *
	 * XXX It is still possible for a tli application to send down data
	 * on the accepting stream while another thread calls t_accept.
	 * This should not be a problem for well-behaved applications since
	 * the T_OK_ACK is sent after the queue swapping is completed.
	 *
	 * If the accepting fd is the same as the listening fd, avoid
	 * queue hash lookup since that will return an eager listener in a
	 * already established state.
	 */
	acceptor_id = tcr->ACCEPTOR_id;
	if (listener->tcp_acceptor_id == acceptor_id) {
		acceptor = listener;
		eager = listener->tcp_eager_next_q;
		/* only count how many T_CONN_INDs so don't count q0 */
		if ((listener->tcp_conn_req_cnt_q != 1) ||
		    (eager->tcp_conn_req_seqnum != tcr->SEQ_number)) {
			tcp_err_ack(listener->tcp_wq, mp, TBADF, 0);
			return;
		}
		if (listener->tcp_conn_req_cnt_q0 != 0) {
			/* Throw away all the eagers on q0. */
			tcp_eager_cleanup(listener, 1);
		}
		if (listener->tcp_syn_defense) {
			listener->tcp_syn_defense = false;
			if (listener->tcp_ip_addr_cache != NULL) {
				kmem_free((void *)listener->tcp_ip_addr_cache,
				    IP_ADDR_CACHE_SIZE * sizeof (ipaddr_t));
				listener->tcp_ip_addr_cache = NULL;
			}
		}
		/*
		 * Transfer tcp_conn_req_max to the eager so that when
		 * a disconnect occurs we can revert the endpoint to the
		 * listen state.
		 */
		eager->tcp_conn_req_max = listener->tcp_conn_req_max;
		tcp_listen_hash_insert(&tcp_listen_fanout[
			TCP_LISTEN_HASH((u_char *)&eager->tcp_lport)], eager);
		ASSERT(listener->tcp_conn_req_cnt_q0 == 0);
		/*
		 * Get a reference on the acceptor just like the
		 * tcp_acceptor_hash_lookup below.
		 */
		acceptor = listener;
		TCP_REFHOLD(acceptor);
	} else {
		acceptor = tcp_acceptor_hash_lookup(acceptor_id);
		if (!acceptor) {
			(void) mi_strlog(q, 1, SL_ERROR|SL_TRACE,
			    "tcp_accept: did not find acceptor 0x%x\n",
			    acceptor_id);
			tcp_err_ack(listener->tcp_wq, mp, TPROVMISMATCH, 0);
			return;
		}
	}

	switch (acceptor->tcp_state) {
	case TCPS_IDLE:
	case TCPS_BOUND:
	case TCPS_LISTEN:
		if (listener->tcp_state == TCPS_LISTEN)
			break;
		/* FALLTHRU */
	default:
		TCP_REFRELE(acceptor);
		tcp_err_ack(listener->tcp_wq, mp, TOUTSTATE, 0);
		return;
	}

	/*
	 * Rendezvous with an eager connection request packet hanging off
	 * 'tcp' that has the 'seqnum' tag.  We tagged the detached open
	 * tcp structure when the connection packet arrived in
	 * tcp_conn_request().
	 */
	seqnum = tcr->SEQ_number;
	eager = listener;
	do {
		eager = eager->tcp_eager_next_q;
		if (!eager) {
			TCP_REFRELE(acceptor);
			tcp_err_ack(listener->tcp_wq, mp, TBADSEQ, 0);
			return;
		}
	} while (eager->tcp_conn_req_seqnum != seqnum);
	/*
	 * Defer clearing eager->tcp_listener until it is removed from the
	 * tcp_conn_fanout table in tcp_accept_swap.
	 *
	 * Prevent the eager from closing until tcp_accept is done.
	 */
	TCP_REFHOLD(eager);

	discon_mp = NULL;	/* initialize properly */
	bind_mp = tcp_ip_bind_mp(eager, O_T_BIND_REQ, sizeof (ipa_conn_t));
	if (!bind_mp) {
		TCP_REFRELE(acceptor);
		TCP_REFRELE(eager);
		tcp_err_ack(listener->tcp_wq, mp, TSYSERR, ENOMEM);
		return;
	}

	/*
	 * We do not want to see another IRE come back in tcp_rput()
	 * since we have already adapted to this guy and we could
	 * mess up our tcp_rwnd.  We simply remove the IRE_DB_REQ here.
	 */
	freeb(tcp_ire_mp(bind_mp));

	/*
	 * Should allocate memory upfront so that we will not fail after
	 * doing tcp_accept_swap. mp is reused and clobbered by
	 * mi_tpi_ok_alloc. Do a copymsg() so that the code below can
	 * use it.
	 *
	 * XXX We are not handling the memory failures for option processing
	 * code below, as it can fail for other reasons after we do the
	 * tcp_accept_swap. If it fails, we may have to possibly go back
	 * to the state before tcp_accept_swap. This will be fixed in the
	 * future.
	 */
	if ((mp1 = copymsg(mp)) == NULL) {
		TCP_REFRELE(acceptor);
		TCP_REFRELE(eager);
		tcp_err_ack(listener->tcp_wq, mp, TSYSERR, ENOMEM);
		freemsg(bind_mp);
		return;
	}
	if ((ok_mp = mi_tpi_ok_ack_alloc(mp)) == NULL) {
		TCP_REFRELE(acceptor);
		TCP_REFRELE(eager);
		tcp_err_ack(listener->tcp_wq, mp1, TSYSERR, ENOMEM);
		freemsg(bind_mp);
		return;
	}

	/*
	 * If there are no options we know that the T_CONN_RES will
	 * succeed. However, we can't send the T_OK_ACK upstream until
	 * the tcp_accept_swap is done since it would be dangerous to
	 * let the application start using the new fd prior to the swap.
	 */
	tcp_accept_swap(listener, acceptor, eager);

	/*
	 * The eager is now associated with its own queue. Insert in
	 * the hash so that the connection can be reused for a future
	 * T_CONN_RES.
	 */
	tcp_acceptor_hash_insert(acceptor_id, eager);

	/*
	 * We now do the processing of options with T_CONN_RES.
	 * We delay till now since we wanted to have queue to pass to
	 * option processing routines that points back to the right
	 * instance structure which does not happen until after
	 * tcp_accept_swap().
	 *
	 * Note:
	 * The sanity of the logic here assumes that whatever options
	 * are appropriate to inherit from listner=>eager are done
	 * before this point, and whatever were to be overridden (or not)
	 * in transfer logic from eager=>acceptor in tcp_accept_swap().
	 * [ Warning: acceptor endpoint can have T_OPTMGMT_REQ done to it
	 *   before its ACCEPTOR_id comes down in T_CONN_RES ]
	 * This may not be true at this point in time but can be fixed
	 * independently. This option processing code starts with
	 * the instantiated acceptor instance and the final queue at
	 * this point.
	 *
	 * XXX Should really do this processing on the eager queue/perimeter!
	 */

	if (tcr->OPT_length != 0) {
		/* Options to process */
		int t_error = 0;
		int sys_error = 0;
		int do_disconnect = 0;

		if (tcp_conprim_opt_process(eager->tcp_wq, mp1,
		    &do_disconnect, &t_error, &sys_error) < 0) {
			if (do_disconnect) {
				/*
				 * An option failed which does not allow
				 * connection to be accepted.
				 *
				 * We allow T_CONN_RES to succeed and
				 * put a T_DISCON_IND on the eager queue.
				 */
				ASSERT(t_error == 0 && sys_error == 0);
				discon_mp = mi_tpi_discon_ind(nilp(mblk_t),
				    ENOPROTOOPT, seqnum);
				if (!discon_mp) {
					TCP_REFRELE(acceptor);
					TCP_REFRELE(eager)
					tcp_err_ack(listener->tcp_wq, mp1,
					    TSYSERR, ENOMEM);
					freemsg(bind_mp);
					freemsg(ok_mp);
					return;
				}
				freemsg(bind_mp);
				bind_mp = NULL;	/* de-initalize */
			} else {
				ASSERT(t_error != 0);
				TCP_REFRELE(acceptor);
				TCP_REFRELE(eager);
				tcp_err_ack(listener->tcp_wq, mp1, t_error,
				    sys_error);
				freemsg(bind_mp);
				freemsg(ok_mp);
				return;
			}
		}
		/*
		 * Most likely success in setting options (except if
		 * discon_mp is set).
		 * mp1 option buffer represented by OPT_length/offset
		 * potentially modified and contains results of setting
		 * options at this point
		 */
	}

	freemsg(mp1);

	putnext(listener->tcp_rq, ok_mp);

	/*
	 * Done with the acceptor - free it
	 */
	ASSERT(acceptor->tcp_detached);
	(void) tcp_clean_death(acceptor, 0);
	TCP_REFRELE(acceptor);

	/*
	 * In case we already received a FIN we have to make tcp_rput send
	 * the ordrel_ind. This will also send up a window update if the window
	 * has opened up.
	 *
	 * In the normal case of a successful connection acceptance
	 * we give the O_T_BIND_REQ to the read side put procedure as an
	 * indication that this was just accepted. This tells tcp_rput to
	 * pass up any data queued in tcp_rcv_head.
	 *
	 * In the fringe case where options sent with T_CONN_RES failed and
	 * we required, we would be indicating a T_DISCON_IND to blow
	 * away this connection.
	 */
	if (bind_mp != NULL) {
		/*
		 * The connection was just accepted. Send the
		 * O_T_BIND_REQ to tcp_rput on the right perimeter to
		 * force it to finish accept processing.
		 */
		lateral_put(q, eager->tcp_rq, bind_mp);
	} else {
		ASSERT(discon_mp != NULL);
		/* Send up the T_DISCON_IND on the eager */
		if (eager->tcp_state >= TCPS_ESTABLISHED) {
			/* Send M_FLUSH according to TPI */
			(void) putnextctl1(eager->tcp_rq, M_FLUSH, FLUSHRW);
		}
		putnext(eager->tcp_rq, discon_mp);
		tcp_xmit_ctl("tcp_rsrv:accept option neg fail abort",
		    eager, NULL, eager->tcp_snxt, 0, TH_RST);
		/* claim resources back */
		(void) tcp_clean_death(eager, 0);
	}
	/* Possible for the eager queue to close after this point. */
	TCP_REFRELE(eager);
}

/*
 * Swap information between the eager and acceptor.
 *
 * Uses the fact that the eager uses the same perimeter
 * as the listener to ensure that no threads are currently executing in
 * tcp_rput for the eager connection.
 * Uses the lock on the tcp_conn_hash bucket to prevent any threads from
 * entering while the swap is in progress.
 *
 * Note that the refcnt on the eager may or may not exceed 2. (tcp_accept
 * has a hold in the eager which is why is it at least two.) A larger
 * refcnt has no ill effects since it is just an indication that some
 * thread in tcp_rput_data is accessing the eager connection. This thread
 * only accesses tcp_rq and the change in tcp_rq below is atomic due
 * to the atomic 32-bit write. Thus either the lateral_put occurs before or
 * after the change to tcp_rq and in either case tcp_rput_data will
 * not process the packet the second time around until tcp_accept_swap has
 * dropped the lock on the hash bucket.
 */
static void
tcp_accept_swap(tcp_t *listener, tcp_t *acceptor, tcp_t *eager)
{
	tf_t	*tf;

	ASSERT(acceptor->tcp_ptpchn == NULL);
	ASSERT(eager->tcp_rq == listener->tcp_rq);
	ASSERT(eager->tcp_detached && !acceptor->tcp_detached);
	ASSERT(!eager->tcp_hard_bound);

	/*
	 * tcp_hard_bound is already cleared to force any tcp_rput threads to
	 * access the eager through the tcp_conn_hash. Then, in order to make
	 * the moving of the timers appear atomic with respect to arriving
	 * packets we hold the tf_lock and ensure that the refcnt is 1.
	 * Then all of the queue swapping, timer swapping and time_wait
	 * manipulation can be done without interference from threads
	 * in tcp_rput/tcp_wsrv.
	 */

	tf = &tcp_conn_fanout[TCP_CONN_HASH((u_char *)&eager->tcp_remote,
	    &eager->tcp_ports)];
	mutex_enter(&tf->tf_lock);
	/*
	 * Now no new threads can enter through tcp_rput for either the
	 * eager or the acceptor since the fanout lock is held and none
	 * of them are hard_bound. Since tcp_rput can't run tcp_rsrv can't
	 * be scheduled on the acceptor. (And the acceptor would not have
	 * already been scheduled since it is a "fresh" instance.)
	 * Also, tcp_wsrv/tcp_timer can not run on the old queue since the
	 * caller is holding the perimeter for that queue.
	 * However, once mi_timer has been called below they can run on the new
	 * which which is fine since by then everything is consistent.
	 * NOTE: The mi_timer calls have to be last in order for this to
	 * work!
	 *
	 * Any thread that enters tcp_rput_data (e.g. on the default queue -
	 * the listen queue perimeter is held by the thread calling tcp_accept)
	 * will first inspect tcp_detached and then inspect tcp_listener.
	 * Holding tf_lock ensures that no new threads can enter however, such
	 * a thread might have done a tcp_lookup before we grabbed the lock.
	 */
	mutex_enter(&tcp_mi_lock);
	mi_swap(&tcp_g_head, (IDP)acceptor, (IDP)eager);
	mutex_exit(&tcp_mi_lock);
	eager->tcp_detached = false;
	acceptor->tcp_detached = true;

	/* remove eager from listen list... */
	tcp_eager_unlink(eager);
	ASSERT(eager->tcp_eager_next_q == NULL);
	ASSERT((eager->tcp_eager_next_q0 == NULL &&
	    eager->tcp_eager_prev_q0 == NULL) ||
	    eager->tcp_eager_next_q0 == eager->tcp_eager_prev_q0);

	eager->tcp_rq = acceptor->tcp_rq;
	eager->tcp_wq = acceptor->tcp_wq;
	acceptor->tcp_rq = listener->tcp_rq;	/* Same as old eager->tcp_rq */
	acceptor->tcp_wq = listener->tcp_wq;	/* Same as old eager->tcp_wq */

	eager->tcp_rq->q_ptr = eager;
	eager->tcp_wq->q_ptr = eager;

	/*
	 * Move the timers to the new queues.
	 * mi_timer_move will moves the timer mblk to the correct queue
	 * even if the timer has fired i.e. the timer mblk has been putq'ed.
	 */
	mi_timer_move(eager->tcp_wq, eager->tcp_timer_mp);
	mi_timer_move(eager->tcp_wq, eager->tcp_ack_mp);
	if (eager->tcp_co_tmp != NULL)
		mi_timer_move(eager->tcp_wq, eager->tcp_co_tmp);
	if (eager->tcp_keepalive_mp != NULL)
		mi_timer_move(eager->tcp_wq, eager->tcp_keepalive_mp);

	mi_timer_move(acceptor->tcp_wq, acceptor->tcp_timer_mp);
	mi_timer_move(acceptor->tcp_wq, acceptor->tcp_ack_mp);
	if (acceptor->tcp_co_tmp != NULL)
		mi_timer_move(acceptor->tcp_wq, acceptor->tcp_co_tmp);
	if (acceptor->tcp_keepalive_mp != NULL)
		mi_timer_move(acceptor->tcp_wq, acceptor->tcp_keepalive_mp);
	mutex_exit(&tf->tf_lock);
}

/*
 * Common accept code.  Called by tcp_conn_request.
 */
static void
tcp_accept_comm(tcp_t *listener, tcp_t *acceptor, mblk_t *cr_pkt)
{
	ipha_t		*ipha;
	tcph_t		*tcph;
	tcp_opt_t	tcpopt;
	int		options;
	int32_t		newmss;

	/* Copy the IP+TCP header template from listener to acceptor */
	bcopy(listener->tcp_iphc, acceptor->tcp_iphc, listener->tcp_hdr_len);
	acceptor->tcp_lport = listener->tcp_lport;
	acceptor->tcp_hdr_len = listener->tcp_hdr_len;
	acceptor->tcp_ip_hdr_len = listener->tcp_ip_hdr_len;
	acceptor->tcp_tcp_hdr_len = listener->tcp_tcp_hdr_len;
	acceptor->tcp_tcph =
		(tcph_t *)&acceptor->tcp_iphc[acceptor->tcp_ip_hdr_len];
	/* Copy our new dest and fport from the connection request packet */
	ipha = (ipha_t *)cr_pkt->b_rptr;
	acceptor->tcp_ipha.ipha_dst = ipha->ipha_src;
	acceptor->tcp_remote = ipha->ipha_src;
	acceptor->tcp_ipha.ipha_src = ipha->ipha_dst;
	tcph = (tcph_t *)&cr_pkt->b_rptr[IPH_HDR_LENGTH(ipha)];
	bcopy(tcph->th_lport, acceptor->tcp_tcph->th_fport, 2);
	bcopy(acceptor->tcp_tcph->th_fport, &acceptor->tcp_fport, 2);

	/* Inherit window size and TCP options from the listener */
	acceptor->tcp_rwnd_max = listener->tcp_rwnd_max;
	acceptor->tcp_naglim = listener->tcp_naglim;
	acceptor->tcp_first_timer_threshold =
	    listener->tcp_first_timer_threshold;
	acceptor->tcp_second_timer_threshold =
	    listener->tcp_second_timer_threshold;

	acceptor->tcp_first_ctimer_threshold =
	    listener->tcp_first_ctimer_threshold;
	acceptor->tcp_second_ctimer_threshold =
	    listener->tcp_second_ctimer_threshold;

	tcp_conn_hash_insert(&tcp_conn_fanout[
	    TCP_CONN_HASH((uint8_t *)&acceptor->tcp_remote,
	    &acceptor->tcp_ports)], acceptor, 0);
	tcp_bind_hash_insert(&tcp_bind_fanout[
	    TCP_BIND_HASH((u_char *)&acceptor->tcp_lport)],
	    acceptor, 0);

	/* Source routing and other valid option copyover */
	tcp_opt_reverse(acceptor, (iph_t *)ipha);
	/*
	 * No need to check for multicast destination since ip will only pass
	 * up multicasts to those that have expressed interest
	 * TODO: what about rejecting broadcasts?
	 * Also check that source is not a multicast or broadcast address.
	 */
	acceptor->tcp_state = TCPS_LISTEN;

	/*
	 * tcp_conn_request() asked IP to append an IRE describing our
	 * peer onto the connection request packet.  Here we adapt our
	 * mss, ttl, ... according to information provided in that IRE.
	 */
	if (!tcp_adapt(acceptor, tcp_ire_mp(cr_pkt))) {
		/*
		 * tcp_conn_request verifies that the peer isn't a multicast
		 * or broadcast address hence tcp_adapt will never fail.
		 */
		cmn_err(CE_PANIC, "tcp: tcp_accept_comm failed");
	}

	/* Parse TCP options */
	tcpopt.tcp = NULL;
	options = tcp_parse_options(tcph, &tcpopt);
	if (!(options & TCP_OPT_MSS_PRESENT))
		tcpopt.tcp_opt_mss = tcp_mss_def;

	if (options & TCP_OPT_WSCALE_PRESENT) {
		acceptor->tcp_snd_ws = tcpopt.tcp_opt_wscale;
		acceptor->tcp_snd_ws_ok = 1;
	} else {
		acceptor->tcp_snd_ws = 0;
		acceptor->tcp_snd_ws_ok = 0;
		acceptor->tcp_rcv_ws = 0;
	}

	/* Process timestamp option */

	if (options & TCP_OPT_TSTAMP_PRESENT) {
		char *ptr = (char *)acceptor->tcp_tcph;

		acceptor->tcp_snd_ts_ok = 1;
		acceptor->tcp_ts_recent = tcpopt.tcp_opt_ts_val;
		acceptor->tcp_last_rcv_lbolt = lbolt;

		ASSERT(OK_32PTR(ptr));
		ASSERT(acceptor->tcp_tcp_hdr_len == TCP_MIN_HEADER_LENGTH);

		ptr += acceptor->tcp_tcp_hdr_len;
		ptr[0] = TCPOPT_NOP;
		ptr[1] = TCPOPT_NOP;
		ptr[2] = TCPOPT_TSTAMP;
		ptr[3] = TCPOPT_TSTAMP_LEN;

		acceptor->tcp_hdr_len += TCPOPT_REAL_TS_LEN;
		acceptor->tcp_tcp_hdr_len += TCPOPT_REAL_TS_LEN;
		acceptor->tcp_tcph->th_offset_and_rsrvd[0] += (3 << 4);
		/*
		 * Note that the mss option does not include the length
		 * of timestamp option, or any other options.
		 */
		tcpopt.tcp_opt_mss -= TCPOPT_REAL_TS_LEN;
	}
	else
		acceptor->tcp_snd_ts_ok = 0;

	if ((options & TCP_OPT_SACK_OK_PRESENT) && (tcp_sack_permitted != 0)) {
		acceptor->tcp_sack_info = kmem_zalloc(sizeof (tcp_sack_info_t),
		    KM_NOSLEEP);
		if (acceptor->tcp_sack_info == NULL) {
			acceptor->tcp_snd_sack_ok = 0;
		} else {
			acceptor->tcp_snd_sack_ok = 1;
		}
		if (acceptor->tcp_snd_ts_ok) {
			acceptor->tcp_max_sack_blk = 3;
		} else {
			acceptor->tcp_max_sack_blk = 4;
		}
	}

	/*
	 * Figure out a (potential) new MSS, taking into account the following:
	 * - If we're doing timestamps, it needs to be reduced by 12 bytes.
	 * - If we got an mss option from the other side with a lower mss,
	 *   we want to use that.
	 */
	newmss = acceptor->tcp_mss;
	if (options & TCP_OPT_TSTAMP_PRESENT)
		newmss -= TCPOPT_REAL_TS_LEN;
	if (tcpopt.tcp_opt_mss < newmss)
		newmss = tcpopt.tcp_opt_mss;

	/*
	 * Actually do the MSS adjustment if we need one.  Note also that if
	 * we have a larger than 16 bit window, but the other side didn't give
	 * us a window scale option, we need to clamp the window to 16 bits.
	 * If we call tcp_mss_set, this happens as a side effect, but if we're
	 * not going to, we need to call tcp_rwnd_set to get it done.
	 */
	if (newmss < acceptor->tcp_mss)
		tcp_mss_set(acceptor, newmss);
	else if (acceptor->tcp_rwnd_max > TCP_MAXWIN &&
	    acceptor->tcp_rcv_ws == 0)
		(void) tcp_rwnd_set(acceptor, acceptor->tcp_rwnd_max);
}

/*
 * Adapt to the mss, rtt and src_addr of this ire mblk and then free it.
 * Also use the HSP table to adjust the send/receive space.
 * Assumes the ire in the mblk is 32 bit aligned.
 *
 * Checks for multicast and broadcast destination address.
 * Returns zero on failure; non-zero if ok.
 */
static int
tcp_adapt(tcp_t *tcp, mblk_t *ire_mp)
{
	tcp_hsp_t	*hsp;
	int		i;
	uint32_t	rwin;

	ire_t	*ire = (ire_t *)ire_mp->b_rptr;
	uint32_t mss = tcp_mss_def;
	extern	ill_t *ire_to_ill(ire_t *ire);
	ill_t	*ill = ire_to_ill(ire);

	/*
	 * Make use of the cached rtt and rtt_sd values to calculate the
	 * initial RTO.  Note that they are already initialized in
	 * tcp_init_values().
	 */
	if (ire->ire_rtt != 0) {
		clock_t	rto;

		tcp->tcp_rtt_sa = ire->ire_rtt;
		tcp->tcp_rtt_sd = ire->ire_rtt_sd;
		rto = (tcp->tcp_rtt_sa >> 3) + tcp->tcp_rtt_sd +
		    tcp_rexmit_interval_extra + (tcp->tcp_rtt_sa >> 5);

		if (rto > tcp_rexmit_interval_max) {
			tcp->tcp_rto = tcp_rexmit_interval_max;
		} else if (rto < tcp_rexmit_interval_min) {
			tcp->tcp_rto = tcp_rexmit_interval_min;
		} else {
			tcp->tcp_rto = rto;
		}
	}
	if (ire->ire_ssthresh != 0)
		tcp->tcp_cwnd_ssthresh = ire->ire_ssthresh;
	else
		tcp->tcp_cwnd_ssthresh = TCP_MAX_LARGEWIN;
	if (ire->ire_max_frag)
		mss = ire->ire_max_frag - tcp->tcp_hdr_len;
	if (tcp->tcp_ipha.ipha_src == 0)
		tcp->tcp_ipha.ipha_src = ire->ire_src_addr;

	if (CLASSD(tcp->tcp_ipha.ipha_dst)) {
		freeb(ire_mp);
		return (0);
	}
	tcp->tcp_localnet = (ire->ire_gateway_addr == 0);

	/*
	 * Initialize the ISS here now that we have the full connection ID.
	 * The RFC 1948 method of initial sequence number generation requires
	 * knowledge of the full connection ID before setting the ISS.
	 */

	tcp_iss_init(tcp);

	switch (ire->ire_type) {
	case IRE_BROADCAST:
		freeb(ire_mp);
		return (0);

	case IRE_LOOPBACK:
	case IRE_LOCAL:
		tcp->tcp_loopback = true;
		if (tcp->tcp_co_head)
			tcp_co_drain(tcp);
		if (!TCP_IS_DETACHED(tcp)) {
			(void) strqset(tcp->tcp_wq, QSTRUIOT,
			    0, STRUIOT_STANDARD);
			/*
			 * For local loopback, we can only enable zero-copy on
			 * one side for each direction. We choose to enable
			 * zero-copy on the transmit side only since it takes
			 * more effort to do page flipping on the receive side
			 * (the kernel buffer has to be page aligned.)
			 */
			if (strzc_on && (zerocopy_prop & 1) != 0 &&
			    mss >= strzc_minblk) {
				(void) mi_set_sth_copyopt(tcp->tcp_rq, MAPINOK);
			}
		}
		break;
	default:
#ifdef ZC_TEST
		if (noswcksum && !TCP_IS_DETACHED(tcp)) {
			(void) strqset(tcp->tcp_wq, QSTRUIOT,
			    0, STRUIOT_STANDARD);
			(void) strqset(tcp->tcp_rq, QSTRUIOT,
			    0, STRUIOT_STANDARD);
		}
#endif
		/*
		 * Check if underlying ill supports checksumming, if so
		 * save a copy of it for reference, else just zero ours.
		 */
		if (ill && ill->ill_ick.ick_magic == ICK_M_CTL_MAGIC &&
		    dohwcksum) {
			tcp->tcp_ill_ick = ill->ill_ick;
			/* The acceptor queue is fixed in tcp_accept() */
			if (!TCP_IS_DETACHED(tcp)) {
				(void) strqset(tcp->tcp_wq, QSTRUIOT, 0,
				    STRUIOT_STANDARD);
#ifdef notneeded
				/*
				 * we don't need this because hardware
				 * checksummed mblks won't go through the
				 * sync-streams interface (STRUIO_SPEC).
				 */
				(void) strqset(tcp->tcp_rq, QSTRUIOT, 0,
				    STRUIOT_STANDARD);
#endif
			}
			if (zerocopy_prop != 0 && strzc_on) {
				/*
				 * If the platform is capable of zero-copy,
				 * truncate mss to a multiple of page size.
				 */
				int32_t	pz = ptob(1);
				if (mss > pz)
					mss &= ~(pz - 1);
			}
		} else
			tcp->tcp_ill_ick.ick_magic = 0;
		break;
	}

	ASSERT(tcp->tcp_rq->q_hiwat <= INT_MAX);
	rwin = tcp->tcp_rq->q_hiwat;
	if ((hsp = tcp_hsp_lookup(tcp->tcp_remote)) != 0) {
		/* Only modify if we're going to make them bigger */

		if (hsp->tcp_hsp_sendspace > tcp->tcp_xmit_hiwater) {
			tcp->tcp_xmit_hiwater = hsp->tcp_hsp_sendspace;
			if (tcp_snd_lowat_fraction != 0)
				tcp->tcp_xmit_lowater = tcp->tcp_xmit_hiwater /
					tcp_snd_lowat_fraction;
		}

		if (hsp->tcp_hsp_recvspace > tcp->tcp_rwnd_max)
			tcp->tcp_rwnd = tcp->tcp_rwnd_max = rwin =
			    hsp->tcp_hsp_recvspace;

		/* Copy timestamp flag */
		tcp->tcp_snd_ts_ok = hsp->tcp_hsp_tstamp;
	}

	/* Figure out what we'd like to use for our window shift */
	for (i = 0; rwin > TCP_MAXWIN && i < TCP_MAX_WINSHIFT; i++, rwin >>= 1)
		;

	tcp->tcp_rcv_ws = i;

	freeb(ire_mp);

	/*
	 * Note that this call to tcp_mss_set eventually takes care of any
	 * changes we made to tcp_xmit_highwater and tcp_rwnd_max.
	 */

	tcp_mss_set(tcp, mss);
	return (1);
}

/*
 * tcp_bind is called (holding the writer lock) by tcp_wput_slow to process a
 * O_T_BIND_REQ/T_BIND_REQ message.
 */
static void
tcp_bind(queue_t *q, mblk_t *mp)
{
	ipa_t	*ipa;
	mblk_t	*mp1;
	in_port_t requested_port;
	in_port_t allocated_port;
	struct T_bind_req *tbr;
	tcp_t	*tcp;
	int	bind_to_req_port_only;
	int	backlog_update = 0;
	int 	user_specified;

	tcp = (tcp_t *)q->q_ptr;
	ASSERT((uintptr_t)(mp->b_wptr - mp->b_rptr) <= (uintptr_t)INT_MAX);
	if ((mp->b_wptr - mp->b_rptr) < sizeof (*tbr)) {
		(void) mi_strlog(q, 1, SL_ERROR|SL_TRACE,
		    "tcp_bind: bad req, len %ld", mp->b_wptr - mp->b_rptr);
		tcp_err_ack(q, mp, TBADADDR, 0);
		return;
	}
	mp1 = reallocb(mp, sizeof (struct T_bind_ack) + sizeof (ipa_t) + 1, 1);
	if (!mp1) {
		tcp_err_ack(q, mp, TSYSERR, ENOMEM);
		return;
	}
	mp = mp1;
	tbr = (struct T_bind_req *)mp->b_rptr;
	if (tcp->tcp_state >= TCPS_BOUND) {
		if ((tcp->tcp_state == TCPS_BOUND ||
		    tcp->tcp_state == TCPS_LISTEN) &&
		    tcp->tcp_conn_req_max != tbr->CONIND_number &&
		    tbr->CONIND_number > 0) {
			/*
			 * Handle listen() increasing CONIND_number.
			 * This is more "liberal" then what the TPI spec
			 * requires but is needed to avoid a t_unbind
			 * when handling listen() since the port number
			 * might be "stolen" between the unbind and bind.
			 */
			backlog_update = 1;
			goto do_bind;
		}
		(void) mi_strlog(q, 1, SL_ERROR|SL_TRACE,
		    "tcp_bind: bad state, %d", tcp->tcp_state);
		tcp_err_ack(q, mp, TOUTSTATE, 0);
		return;
	}
	switch (tbr->ADDR_length) {
	case 0:			/* request for a generic port */
		tbr->ADDR_offset = sizeof (struct T_bind_req);
		tbr->ADDR_length = sizeof (ipa_t);
		ipa = (ipa_t *)&tbr[1];
		bzero(ipa, sizeof (*ipa));
		ipa->ip_family = AF_INET;
		mp->b_wptr = (u_char *)&ipa[1];
		requested_port = 0;
		break;
	case sizeof (ipa_t):	/* Complete IP address */
		ipa = (ipa_t *)mi_offset_param(mp, tbr->ADDR_offset,
		    sizeof (ipa_t));
		if (ipa == NULL) {
			(void) mi_strlog(q, 1, SL_ERROR|SL_TRACE,
			    "tcp_bind: bad address parameter, "
			    "offset %d, len %d",
			    tbr->ADDR_offset, tbr->ADDR_length);
			tcp_err_ack(q, mp, TSYSERR, EPROTO);
			return;
		}
		requested_port = BE16_TO_U16(ipa->ip_port);
		break;
	default:
		(void) mi_strlog(q, 1, SL_ERROR|SL_TRACE,
		    "tcp_bind: bad address length, %d", tbr->ADDR_length);
		tcp_err_ack(q, mp, TBADADDR, 0);
		return;
	}

	/*
	 * For O_T_BIND_REQ:
	 * Verify that the target port/addr is available, or choose
	 * another.
	 * For  T_BIND_REQ:
	 * Verify that the target port/addr is available or fail.
	 * In both cases when it succeeds the tcp is inserted in the
	 * bind hash table. This ensures that the operation is atomic
	 * under the lock on the hash bucket.
	 */
	if (requested_port == 0 || tbr->PRIM_type == O_T_BIND_REQ)
		bind_to_req_port_only = 0;
	else			/* T_BIND_REQ and requested_port != 0 */
		bind_to_req_port_only = 1;
	/*
	 * Get a valid port (within the anonymous range and should not
	 * be a privileged one) to use if the user has not given a port.
	 * If multiple threads are here, they may all start with
	 * with the same initial port. But, it should be fine as long as
	 * tcp_bindi will ensure that no two threads will be assigned
	 * the same port.
	 *
	 * NOTE : XXX If superuser asks for an anonymous port, we still
	 * check for ports only in the range > tcp_smallest_non_priv_port.
	 */
	if (requested_port == 0) {
		requested_port = tcp_update_next_port(tcp_next_port_to_try);
		user_specified = 0;
	} else {
		/*
		 * If the requested_port is in the well-known privileged range,
		 * verify that the stream was opened by a privileged user.
		 * Note: No locks are held when inspecting tcp_g_*epriv_ports
		 * but instead the code relies on:
		 * - the fact that the address of the array and its size never
		 *   changes
		 * - the atomic assignment of the elements of the array
		 */
		if (!tcp->tcp_priv_stream) {
			int i;
			int priv = 0;

			if (requested_port < tcp_smallest_nonpriv_port) {
				priv = 1;
			} else {
				for (i = 0; i < tcp_g_num_epriv_ports; i++) {
					if (requested_port ==
					    tcp_g_epriv_ports[i]) {
						priv = 1;
						break;
					}
				}
			}
			if (priv) {
				(void) mi_strlog(q, 1, SL_ERROR|SL_TRACE,
				    "tcp_bind: no priv for port %d",
				    requested_port);
				tcp_err_ack(q, mp, TACCES, 0);
				return;
			}
		}
		user_specified = 1;
	}

	bcopy(ipa->ip_addr, tcp->tcp_iph.iph_src, 4);
	tcp->tcp_bound_source = tcp->tcp_ipha.ipha_src;
	allocated_port = tcp_bindi(tcp, requested_port, ipa->ip_addr,
	    tcp->tcp_reuseaddr, bind_to_req_port_only, user_specified);

	if (allocated_port == 0) {
		if (bind_to_req_port_only) {
			(void) mi_strlog(q, 1, SL_ERROR|SL_TRACE,
			    "tcp_bind: requested addr busy");
			tcp_err_ack(q, mp, TADDRBUSY, 0);
		} else {
			/* If we are out of ports, fail the bind. */
			(void) mi_strlog(q, 1, SL_ERROR|SL_TRACE,
			    "tcp_bind: out of ports?");
			tcp_err_ack(q, mp, TNOADDR, 0);
		}
		return;
	}
	ASSERT(tcp->tcp_state == TCPS_BOUND);
do_bind:
	if (!backlog_update) {
		U16_TO_BE16(allocated_port, ipa->ip_port);
	}
	if (tbr->CONIND_number != 0)
		mp1 = tcp_ip_bind_mp(tcp, tbr->PRIM_type, sizeof (ipa_t));
	else
		mp1 = tcp_ip_bind_mp(tcp, tbr->PRIM_type, IP_ADDR_LEN);
	if (!mp1) {
		tcp_err_ack(q, mp, TSYSERR, ENOMEM);
		return;
	}

	tbr->PRIM_type = T_BIND_ACK;
	mp->b_datap->db_type = M_PCPROTO;

	/* Chain in the reply mp for tcp_rput() */
	mp1->b_cont = mp;
	mp = mp1;

	tcp->tcp_conn_req_max = tbr->CONIND_number;
	if (tcp->tcp_conn_req_max) {
		if (tcp->tcp_conn_req_max < tcp_conn_req_min)
			tcp->tcp_conn_req_max = tcp_conn_req_min;
		if (tcp->tcp_conn_req_max > tcp_conn_req_max_q)
			tcp->tcp_conn_req_max = tcp_conn_req_max_q;
		tcp->tcp_state = TCPS_LISTEN;
		tcp->tcp_eager_next_q0 = tcp->tcp_eager_prev_q0 = tcp;
		tcp->tcp_second_ctimer_threshold = tcp_ip_abort_linterval;
		tcp_listen_hash_insert(&tcp_listen_fanout[
			TCP_LISTEN_HASH((u_char *)&tcp->tcp_lport)], tcp);
	}

	/*
	 * Bind processing continues in tcp_rput() where IP passes us back
	 * an M_PROTO/T_BIND_ACK followed by the reply mp we chained in above.
	 */
	putnext(q, mp);
}

/*
 * If the "bind_to_req_port_only" parameter is set, if the requested port
 * number is available, return it, If not return 0
 *
 * If "bind_to_req_port_only" parameter is not set and
 * If the requested port number is available, return it.  If not, return
 * the first anonymous port we happen across.  If no anonymous ports are
 * available, return 0. addr is the requested local address, if any.
 *
 * In either case, when succeeding update the tcp_t to record the port number
 * and insert it in the bind hash table.
 */
static in_port_t
tcp_bindi(tcp_t *tcp, in_port_t port, u_char *addr, int reuseaddr,
    int bind_to_req_port_only, int user_specified)
{
	/* number of times we have run around the loop */
	int count = 0;
	/* maximum number of times to run around the loop */
	int loopmax;
	tcp_t	*ltcp;

	/*
	 * Lookup for free addresses is done in a loop and "loopmax"
	 * influences how long we spin in the loop
	 */
	if (bind_to_req_port_only) {
		/*
		 * If the requested port is busy, don't bother to look
		 * for a new one. Setting loop maximum count to 1 has
		 * that effect.
		 */
		loopmax = 1;
	} else {
		/*
		 * If the requested port is busy, look for a free one
		 * in the anonymous port range.
		 * Set loopmax appropriately so that one does not look
		 * forever in the case all of the anonymous ports are in use.
		 */
		loopmax = (tcp_largest_anon_port - tcp_smallest_anon_port + 1);
	}
	do {
		u_char		lport[2];
		ipaddr_t	src = 0;
		ipaddr_t	src1;
		tf_t	*tf;

		U16_TO_BE16(port, lport);

		if (addr) {
			/* we want the address as is, not swapped */
			UA32_TO_U32(addr, src);
		}

		/*
		 * Ensure that the tcp_t is not currently in the bind hash.
		 * Hold the lock on the hash bucket to ensure that
		 * the duplicate check plus the insertion is an atomic
		 * operation.
		 */
		tcp_bind_hash_remove(tcp);
		tf = &tcp_bind_fanout[TCP_BIND_HASH(lport)];
		mutex_enter(&tf->tf_lock);
		for (ltcp = tf->tf_tcp; ltcp != nilp(tcp_t);
		    ltcp = ltcp->tcp_bind_hash) {
			if (BE16_EQL(lport, (u_char *)&ltcp->tcp_lport)) {
				src1 = ltcp->tcp_bound_source;
				if (!reuseaddr) {
					/*
					 * No socket option SO_REUSEADDR.
					 *
					 * If existing port is bound to
					 * a non-wildcard IP address
					 * and the requesting stream is
					 * bound to a distinct
					 * different IP addresses
					 * (non-wildcard, also), keep
					 * going.
					 */
					if (src != INADDR_ANY &&
					    src1 != INADDR_ANY && src1 != src)
						continue;
					if (ltcp->tcp_state >= TCPS_BOUND) {
						/*
						 * This port is being used and
						 * its state is >= TCPS_BOUND,
						 * so we can't bind to it.
						 */
						break;
					}
				} else {
					/*
					 * socket option SO_REUSEADDR is set.
					 *
					 * If two streams are bound to
					 * same IP address or both src
					 * and src1 are wildcards(INADDR_ANY),
					 * we want to stop searching.
					 * We have found a match of IP source
					 * address and source port, which is
					 * refused regardless of the
					 * SO_REUSEADDR setting, so we break.
					 */
					if ((src == src1) &&
					    (ltcp->tcp_state == TCPS_LISTEN ||
					    ltcp->tcp_state == TCPS_BOUND))
						break;
				}
			}
		}
		if (ltcp != NULL) {
			/* The port number is busy */
			mutex_exit(&tf->tf_lock);
		} else {
			/*
			 * This port is ours. Insert in fanout and mark as
			 * bound to prevent others from getting the port
			 * number.
			 */
			tcp->tcp_state = TCPS_BOUND;
			U16_TO_ABE16(port, tcp->tcp_tcph->th_lport);
			tcp->tcp_lport = htons(port);

			ASSERT(&tcp_bind_fanout[TCP_BIND_HASH(
				(u_char *)&tcp->tcp_lport)] == tf);
			tcp_bind_hash_insert(tf, tcp, 1);

			mutex_exit(&tf->tf_lock);
			/*
			 * We don't want tcp_next_port_to_try to "inherit"
			 * a port number supplied by the user in a bind.
			 */
			if (user_specified != 0)
				return (port);

			/*
			 * This is the only place where tcp_next_port_to_try
			 * is updated. After the update, it may or may not
			 * be in the valid range.
			 */
			tcp_next_port_to_try = port + 1;
			return (port);
		}

		if ((count == 0) && (user_specified)) {
			/*
			 * We may have to return an anonymous port. So
			 * get one to start with.
			 */
			port = tcp_update_next_port(tcp_next_port_to_try);
			user_specified = 0;
		} else {
			port = tcp_update_next_port(port + 1);
		}

		/*
		 * Don't let this loop run forever in the case where
		 * all of the anonymous ports are in use.
		 */
	} while (++count < loopmax);
	return (0);
}


static void
tcp_listener_discon_ind(tcp_t *eager, int err)
{
	tcp_t	*listener;
	mblk_t  *mp;

	if (((listener = eager->tcp_listener) != NULL) && err) {
		/* state check instead of eager_conn_ind ? */
		if (eager->tcp_conn.tcp_eager_conn_ind != NULL)
			return;
		mp = mi_tpi_discon_ind(nilp(mblk_t), err,
		    eager->tcp_conn_req_seqnum);
		if (mp != NULL) {
			/*
			 * Eager and Listener point to same queue (invariant of
			 * this TCP implementation)
			 */
			ASSERT(listener->tcp_rq == eager->tcp_rq);
			lateral_putnext(eager->tcp_rq, listener->tcp_rq, mp);
		}
	}
}


/*
 * We are dying for some reason.  Try to do it gracefully.  (May be called
 * as writer.)
 *
 * Return -1 if the structure was not cleaned up (if the cleanup had to be
 * done by a service procedure).
 * TBD - Should the return value distinguish between the tcp_t being
 * freed and it being reinitialized?
 */
static int
tcp_clean_death(tcp_t *tcp, int err)
{
	mblk_t	*mp;
	queue_t	*q;

	/*
	 * Because they have no upstream client to rebind or tcp_close()
	 * them later, we axe detached state vectors here and now.
	 */
	if (TCP_IS_DETACHED(tcp)) {
		if (tcp->tcp_listener && err != 0)
			/*
			 * For detached eager endpoints for which
			 * connection indication has been sent up the
			 * listener, send up a T_DISCON_IND. (Only when
			 * requested with an error code).
			 */
			tcp_listener_discon_ind(tcp, err);
		tcp_close_detached(tcp);
		return (0);
	}

	/*
	 * If T_ORDREL_IND has not been sent yet (done when service routine
	 * is run) postpone cleaning up the endpoint until service routine
	 * has sent up the T_ORDREL_IND. Avoid clearing out an existing
	 * client_errno since tcp_close uses the client_errno field.
	 */
	if (tcp->tcp_fin_rcvd && !tcp->tcp_ordrel_done) {
		if (err != 0)
			tcp->tcp_client_errno = err;
		tcp->tcp_deferred_clean_death = true;
		return (-1);
	}

	q = tcp->tcp_rq;

	/* Trash all inbound data */
	flushq(q, FLUSHALL);

	/*
	 * If we are at least part way open and there is error
	 * (err==0 implies no error)
	 * notify our client by a T_DISCON_IND.
	 */
	if ((tcp->tcp_state >= TCPS_SYN_SENT) && err) {
		if (tcp->tcp_state >= TCPS_ESTABLISHED) {
			/* Send M_FLUSH according to TPI */
			(void) putnextctl1(q, M_FLUSH, FLUSHRW);
		}
		(void) mi_strlog(q, 1, SL_TRACE|SL_ERROR,
		    "tcp_clean_death: discon err %d", err);
		mp = mi_tpi_discon_ind(nilp(mblk_t), err, 0);
		if (mp) {
			putnext(q, mp);
		} else {
			(void) mi_strlog(q, 1, SL_ERROR|SL_TRACE,
			    "tcp_clean_death, sending M_ERROR");
			(void) putctl1(q, M_ERROR, EPROTO);
		}
		if (tcp->tcp_state <= TCPS_SYN_RCVD) {
			/* SYN_SENT or SYN_RCVD */
			BUMP_MIB(tcp_mib.tcpAttemptFails);
		} else if (tcp->tcp_state <= TCPS_CLOSE_WAIT) {
			/* ESTABLISHED or CLOSE_WAIT */
			BUMP_MIB(tcp_mib.tcpEstabResets);
		}
	}

	tcp_reinit(tcp);
	return (0);
}

/*
 * Function used by qtimeout just to trigger the implicit wakeup
 * of the qwait in tcp_close.
 */
static void
tcp_close_linger_timeout(void *q_arg)
{
	queue_t *q = (queue_t *)q_arg;
	tcp_t *tcp = (tcp_t *)q->q_ptr;

	tcp->tcp_client_errno = ETIMEDOUT;
}

/*
 * Called by streams when our client blows off her descriptor, we take
 * this to mean:
 *  "close the stream state NOW, close the tcp connection politely"
 * When SO_LINGER is set (with a non-zero linger time and it is not
 * a nonblocking socket) then this routine sleeps until the FIN is acked.
 *
 * NOTE: tcp_close potentially returns error when lingering.
 * However, the stream head currently does not pass these errors
 * to the application. 4.4BSD only returns EINTR and EWOULDBLOCK
 * errors to the application (from tsleep()) and not errors
 * like ECONNRESET caused by receiving a reset packet.
 *
 * The call to qprocsoff must be done after the qwait/qtimeout logic.
 * Since this is a D_MTQPAIR module the qprocsoff can be done
 * arbitrarily late in the close routine.
 */
static int
tcp_close(queue_t *q, int flag)
{
	char	*msg;
	tcp_t	*tcp = (tcp_t *)q->q_ptr;
	int	error = 0;

	/*
	 * Cancel any pending bufcall or timeout.
	 */
	if (tcp->tcp_timeout || tcp->tcp_ordrelid != 0) {
		if (tcp->tcp_timeout)
			(void) quntimeout(q, tcp->tcp_ordrelid);
		else
			qunbufcall(q, tcp->tcp_ordrelid);
		tcp->tcp_ordrelid = 0;
		tcp->tcp_timeout = false;
	}

	/*
	 * If we might anchor some detached tcp structures, let them
	 * know their anchor is lifting.
	 */
	if (q == tcp_g_q) {
		timeout_id_t id;

		/* Assumes that this store is atomic. */
		tcp_g_q = nilp(queue_t);
		mutex_enter(&tcp_time_wait_lock);
		id = tcp_time_wait_tid;
		tcp_time_wait_tid = 0;
		mutex_exit(&tcp_time_wait_lock);
		if (id != 0)
			(void) quntimeout(q, id);
		tcp_lift_anchor(tcp);
	}
	if (tcp->tcp_conn_req_cnt_q0 != 0 || tcp->tcp_conn_req_cnt_q != 0) {
		/* Cleanup for listener */
		tcp_eager_cleanup(tcp, 0);
	}
	msg = nilp(char);
	switch (tcp->tcp_state) {
	case TCPS_CLOSED:
	case TCPS_IDLE:
	case TCPS_BOUND:
	case TCPS_LISTEN:
		break;
	case TCPS_SYN_SENT:
		msg = "tcp_close, during connect";
		break;
	case TCPS_SYN_RCVD:
		/*
		 * Close during the connect 3-way handshake
		 * but here there may or may not be pending data
		 * already on queue. Process almost same as in
		 * the ESTABLISHED state.
		 */
		/* FALLTHRU */
	default:
		/*
		 * If SO_LINGER has set a zero linger time, abort the
		 * connection with a reset.
		 */
		if (tcp->tcp_linger && tcp->tcp_lingertime == 0) {
			msg = "tcp_close, zero lingertime";
			break;
		}

		/*
		 * Send an unbind to IP to make new packets (such as the final
		 * ACK) to arrive on the default TCP queue instead of ending
		 * up on our syncq (and being freed by the STREAMS framework
		 * as part of the close.
		 * Wait until we can an ack/nack for the unbind to ensure
		 * that IP is unlikely to send any more packets to this queue.
		 * (IP will send up packets if the IPC_REFHOLD occured prior
		 * to the unbind since the unbind processing in IP can't wait
		 * for the reference count to drop to zero.)
		 */
		ASSERT(tcp->tcp_hard_bound || tcp->tcp_hard_binding);
		if (tcp_ip_unbind(tcp->tcp_wq)) {
			/* Unbind sent - wait for ack */
			while (tcp->tcp_hard_bound || tcp->tcp_hard_binding)
				qwait(q);
		} else {
			/*
			 * Unbind could not be sent. This might result
			 * in packets being lost thus the peer might have
			 * to retransmit.
			 */
			tcp->tcp_hard_bound = tcp->tcp_hard_binding = false;
		}
		/*
		 * Abort connection if there is unread data queued.
		 * This check has to be done after unbind for two reasons:
		 * 1. Data can come in during qwait().
		 * 2. If data landed on co-queue and tcp is detached, when
		 * the co timer is fired, tcp_co_drain() -> tcp_rput_data()
		 * will cause the tcp structure to be freed right way. When
		 * control returns to tcp_co_drain() it will crash the system.
		 */
		if (tcp->tcp_rcv_head || tcp->tcp_reass_head ||
		    tcp->tcp_co_head) {
			msg = "tcp_close, unread data";
			break;
		}
		/*
		 * tcp_hard_bound is now cleared thus all packets go through
		 * tcp_lookup. This fact is used by tcp_detach below.
		 *
		 * We have done a qwait() above which could have possibly
		 * drained more messages in turn causing transition to a
		 * different state. Check whether we have to do the rest
		 * of the processing or not.
		 */
		if (tcp->tcp_state <= TCPS_LISTEN)
			break;

		/*
		 * Transmit the FIN before detaching the tcp_t.
		 * After tcp_detach returns this queue/perimeter
		 * no longer owns the tcp_t thus others can modify it.
		 */
		(void) tcp_xmit_end(tcp);

		/*
		 * If lingering on close then wait until the fin is acked,
		 * the SO_LINGER time passes, or a reset is sent/received.
		 */
		if (tcp->tcp_linger && tcp->tcp_lingertime > 0 &&
		    !(tcp->tcp_fin_acked) &&
		    tcp->tcp_state >= TCPS_ESTABLISHED) {
			clock_t stoptime; /* in ticks */
			clock_t time_left; /* in ticks */
			timeout_id_t id;

			tcp->tcp_client_errno = 0;
			if (flag & (FNDELAY|FNONBLOCK))
				tcp->tcp_client_errno = EWOULDBLOCK;

			stoptime = lbolt + (tcp->tcp_lingertime * hz);
			while (!(tcp->tcp_fin_acked) &&
			    tcp->tcp_state >= TCPS_ESTABLISHED &&
			    tcp->tcp_client_errno == 0 &&
			    (time_left = stoptime - lbolt) > 0) {
				id = qtimeout(q, tcp_close_linger_timeout,
				    q, time_left);
				if (qwait_sig(q) == 0)
					tcp->tcp_client_errno = EINTR;
				(void) quntimeout(q, id);
			}
			/* XXX MIB for linger time expired? */
			error = tcp->tcp_client_errno;
			tcp->tcp_client_errno = 0;

			/*
			 * Check if we need to detach or just close
			 * the instance.
			 */
			if (tcp->tcp_state <= TCPS_LISTEN)
				break;
		}

		/*
		 * Make sure that no other thread will access the tcp_rq of
		 * this instance (through lookups etc.) as tcp_rq will go
		 * away shortly.
		 */
		tcp_acceptor_hash_remove(tcp);

		/*
		 * Attempt to detach tcp for polite connection termination.
		 * Increment the reference count to avoid a RST that may
		 * get in the syncq or global queue which will free the tcp
		 * structure.  Otherwise, we may dereference a freed tcp
		 * structure while draining the syncq.  Decrement the
		 * count after doing that.
		 */
		TCP_REFHOLD(tcp);
		if (tcp_detach(tcp)) {
			/*
			 * Need to drain sync queue because packets
			 * may have come in before we finished dismantling
			 * the tcp structure. Note that the tcp_ip_unbind
			 * above ensures that IP is unlikely to send up any
			 * packets directly to this stream. (IP will send up
			 * packets if the IPC_REFHOLD occured prior to the
			 * unbind since the unbind processing in IP can't wait
			 * for the reference count to drop to zero.)
			 * In any case, tcp_rput_data
			 * will still perform lateral_put's to the queue so
			 * in order to avoid dropping packets we use qwait to
			 * drain those packets.
			 *
			 * At this point in time q->q_ptr->tcp_rq does not
			 * refer to the same queue thus tcp_rput has to
			 * handle this case.
			 */
			while (q->q_syncq->sq_head != NULL)
				qwait(q);
			TCP_REFRELE(tcp);
			qprocsoff(q);
			return (error);
		}
		TCP_REFRELE(tcp);
		/* Detach failed, abort unless we're in TIME_WAIT state */
		if (tcp->tcp_state != TCPS_TIME_WAIT) {
			msg = "tcp_close, couldn't detach";
		}
		break;
	}

	/* Detach did not complete. Still need to remove q from stream. */
	if (msg) {
		if (tcp->tcp_state == TCPS_ESTABLISHED ||
		    tcp->tcp_state == TCPS_CLOSE_WAIT)
			BUMP_MIB(tcp_mib.tcpEstabResets);
		if (tcp->tcp_state == TCPS_SYN_SENT ||
		    tcp->tcp_state == TCPS_SYN_RCVD)
			BUMP_MIB(tcp_mib.tcpAttemptFails);
		tcp_xmit_ctl(msg, tcp, nilp(mblk_t), tcp->tcp_snxt, 0, TH_RST);
	}

	/*
	 * Note that tcp_close is still holding the perimeter while
	 * doing the cv_wait. This can not lead to a deadlock since
	 * any thead having done a TCP_REFHOLD will not try to acquire
	 * the tcp perimeter in a blocking fashion. The nonblocking
	 * access (e.g. putnext leading to a fill_syncq) will cause
	 * such a thread to return and do the TCP_REFRELE.
	 */
	tcp_closei_local(tcp);
	mutex_enter(&tcp_mi_lock);
	mutex_enter(&tcp->tcp_reflock);
	while (tcp->tcp_refcnt != 1) {
		mutex_exit(&tcp_mi_lock);
		cv_wait(&tcp->tcp_refcv, &tcp->tcp_reflock);
		mutex_enter(&tcp_mi_lock);
	}
	/*
	 * Defer the mi_close_unlink until now when we know there is no
	 * other thread since another thread might be in tcp_accept and
	 * about to do an mi_swap which would be upset if the tcp_t has
	 * been unlinked already.
	 */
	mi_close_unlink(&tcp_g_head, (IDP)tcp);
	mutex_exit(&tcp_mi_lock);

	/*
	 * Inline version of TCP_REFRELE without dropping and reacquiring
	 * the lock. Note that tcp_inactive destroys the mutex thus
	 * no mutex_exit is needed.
	 */
	tcp->tcp_refcnt--;
	tcp_inactive(tcp);

	qprocsoff(q);
	q->q_ptr = WR(q)->q_ptr = NULL;
	return (error);
}

/*
 * Clean up the b_next and b_prev fields of every mblk pointed at by *mpp.
 * Some stream heads get upset if they see these later on as anything but nil.
 */
static void
tcp_close_mpp(mblk_t **mpp)
{
	mblk_t	*mp;

	if ((mp = *mpp) != NULL) {
		do {
			mp->b_next = nilp(mblk_t);
			mp->b_prev = nilp(mblk_t);
		} while ((mp = mp->b_cont) != NULL);
		freemsg(*mpp);
		*mpp = nilp(mblk_t);
	}
}

/*
 * Unlink from tcp_g_head and do the detached close.
 * Remove the refhold implicit in being on the tcp_g_head list.
 */
static void
tcp_close_detached(tcp_t *tcp)
{
	mutex_enter(&tcp_mi_lock);
	mi_close_unlink(&tcp_g_head, (IDP)tcp);
	mutex_exit(&tcp_mi_lock);
	tcp_closei_local(tcp);
	TCP_REFRELE(tcp);
}

/*
 * The tcp_t is going away. Remove it from all lists and set it
 * to TCPS_CLOSED. The caller has to remove it from the
 * tcp_g_head list. The freeing up of memory is deferred until
 * tcp_inactive. This is needed since a thread in tcp_rput might have
 * done a TCP_REFHOLD on this structure before it was removed from the
 * hashes.
 */
static void
tcp_closei_local(tcp_t *tcp)
{
	mblk_t	*mp;
	queue_t	*wq = tcp->tcp_wq;

	tcp_bind_hash_remove(tcp);
	tcp_listen_hash_remove(tcp);
	tcp_conn_hash_remove(tcp);
	tcp_acceptor_hash_remove(tcp);

	UPDATE_MIB(tcp_mib.tcpInSegs, tcp->tcp_ibsegs);
	tcp->tcp_ibsegs = 0;
	UPDATE_MIB(tcp_mib.tcpOutSegs, tcp->tcp_obsegs);
	tcp->tcp_obsegs = 0;
	/*
	 * If we are an eager connection hanging off a listener that hasn't
	 * formally accepted the connection yet, get off his list and blow off
	 * any data that we have accumulated.
	 */
	if (tcp->tcp_listener != NULL)
		tcp_eager_unlink(tcp);

	tcp_time_wait_remove(tcp);
	ASSERT(tcp->tcp_time_wait_next == NULL);
	ASSERT(tcp->tcp_time_wait_prev == NULL);
	ASSERT(tcp->tcp_time_wait_expire == 0);

	/* Stop and free the timers */
	if ((mp = tcp->tcp_timer_mp) != NULL) {
		mi_timer_free(mp);
		tcp->tcp_timer_mp = nilp(mblk_t);
	}
	if ((mp = tcp->tcp_co_tmp) != NULL) {
		tcp->tcp_co_tintrvl = -1l;
		mi_timer_free(mp);
		tcp->tcp_co_tmp = nilp(mblk_t);
	}
	if ((mp = tcp->tcp_co_imp) != NULL) {
		tcp->tcp_co_imp = nilp(mblk_t);
		freemsg(mp);
	}
	if ((mp = tcp->tcp_keepalive_mp) != NULL) {
		mi_timer_free(mp);
		tcp->tcp_keepalive_mp = nilp(mblk_t);
	}
	if ((mp = tcp->tcp_ack_mp) != NULL) {
		mi_timer_free(mp);
		tcp->tcp_ack_mp = NULL;
	}
	if (tcp->tcp_state == TCPS_LISTEN) {
		if (tcp->tcp_ip_addr_cache) {
			kmem_free((void *)tcp->tcp_ip_addr_cache,
			    IP_ADDR_CACHE_SIZE * sizeof (ipaddr_t));
			tcp->tcp_ip_addr_cache = NULL;
		}
	}
	if (tcp->tcp_flow_stopped) {
		ASSERT(tcp->tcp_flow_mp->b_prev != NULL ||
		    tcp->tcp_flow_mp->b_next != NULL ||
		    tcp->tcp_wq->q_first == tcp->tcp_flow_mp);
		rmvq(wq, tcp->tcp_flow_mp);
	}
	ASSERT(tcp->tcp_flow_mp == NULL ||
	    (tcp->tcp_flow_mp->b_prev == NULL &&
		tcp->tcp_flow_mp->b_next == NULL &&
		tcp->tcp_wq->q_first != tcp->tcp_flow_mp));

	tcp->tcp_state = TCPS_CLOSED;
}

/*
 * Last reference to the tcp_t is gone. Free all memory associated with it.
 * Called from TCP_REFRELE and tcp_close.
 */
static void
tcp_inactive(tcp_t *tcp)
{
	mblk_t	*mp;

	ASSERT(MUTEX_HELD(&tcp->tcp_reflock));
	ASSERT(tcp->tcp_refcnt == 0);

	ASSERT(tcp->tcp_ptpbhn == NULL && tcp->tcp_bind_hash == NULL);
	ASSERT(tcp->tcp_ptplhn == NULL && tcp->tcp_listen_hash == NULL);
	ASSERT(tcp->tcp_ptpchn == NULL && tcp->tcp_conn_hash == NULL);
	ASSERT(tcp->tcp_ptpahn == NULL && tcp->tcp_acceptor_hash == NULL);

	tcp_close_mpp(&tcp->tcp_flow_mp);
	tcp_close_mpp(&tcp->tcp_xmit_head);
	tcp_close_mpp(&tcp->tcp_reass_head);
	tcp->tcp_reass_tail = nilp(mblk_t);
	if ((mp = tcp->tcp_co_head) != NULL) {
		mblk_t	*mp1;
		do {
			mp1 = mp->b_next;
			mp->b_next = nilp(mblk_t);
			freemsg(mp);
		} while ((mp = mp1) != NULL);
		tcp->tcp_co_head = nilp(mblk_t);
		tcp->tcp_co_tail = nilp(mblk_t);
		tcp->tcp_co_cnt = 0;
	}
	if (tcp->tcp_rcv_head) {
		freemsg(tcp->tcp_rcv_head);
		tcp->tcp_rcv_head = nilp(mblk_t);
		tcp->tcp_rcv_cnt = 0;
	}
	if ((mp = tcp->tcp_urp_mp) != NULL) {
		freemsg(mp);
		tcp->tcp_urp_mp = NULL;
	}
	if ((mp = tcp->tcp_urp_mark_mp) != NULL) {
		freemsg(mp);
		tcp->tcp_urp_mark_mp = NULL;
	}

	if (tcp->tcp_sack_info != NULL) {
		if (tcp->tcp_notsack_list != NULL) {
			TCP_NOTSACK_REMOVE_ALL(tcp->tcp_notsack_list);
		}
		kmem_free(tcp->tcp_sack_info, sizeof (tcp_sack_info_t));
		tcp->tcp_sack_info = NULL;
	}

	/*
	 * Following is really a blowing away a union.
	 * It happens to have exactly two members of identical size
	 * the following code is enough.
	 */
	tcp_close_mpp(&tcp->tcp_conn.tcp_eager_conn_ind);

	mutex_destroy(&tcp->tcp_reflock);
	cv_destroy(&tcp->tcp_refcv);

	mi_close_free((IDP)tcp);
}

/*
 * Put a connection confirmation message upstream built from the
 * address information within 'iph' and 'tcph'.  Report our success or failure.
 */
static boolean_t
tcp_conn_con(tcp_t *tcp, iph_t *iph, tcph_t *tcph)
{
	ipa_t	ipa;
	mblk_t	*mp;
	char	*optp = NULL;
	int	optlen = 0;

	bzero(&ipa, sizeof (ipa));
	bcopy(iph->iph_src, ipa.ip_addr, sizeof (ipa.ip_addr));
	bcopy(tcph->th_lport, ipa.ip_port, sizeof (ipa.ip_port));
	ipa.ip_family = AF_INET;
	if (tcp->tcp_conn.tcp_opts_conn_req != NULL) {
		/*
		 * Return in T_CONN_CON results of option negotiation through
		 * the T_CONN_REQ. Note: If there is an real end-to-end option
		 * negotiation, then what is received from remote end needs
		 * to be taken into account but there is no such thing (yet?)
		 * in our TCP/IP.
		 * Note: We do not use mi_offset_param() here as
		 * tcp_opts_conn_req contents do not directly come from
		 * an application and are either generated in kernel or
		 * from user input that was already verified.
		 */
		mp = tcp->tcp_conn.tcp_opts_conn_req;
		optp = (char *)(mp->b_rptr +
		    ((struct T_conn_req *)mp->b_rptr)->OPT_offset);
		optlen = (int)
		    ((struct T_conn_req *)mp->b_rptr)->OPT_length;
	}
	mp = mi_tpi_conn_con(nilp(mblk_t), (char *)&ipa, (int)sizeof (ipa_t),
	    optp, optlen);
	if (!mp)
		return (false);
	putnext(tcp->tcp_rq, mp);
	if (tcp->tcp_conn.tcp_opts_conn_req != NULL)
		tcp_close_mpp(&tcp->tcp_conn.tcp_opts_conn_req);
	return (true);
}

/*
 * Defense for the SYN attack -
 * 1. When q0 is full, drop from the tail (tcp_eager_prev_q0) the oldest
 *    one that doesn't have the dontdrop bit set.
 * 2. Don't drop a SYN request before its first timeout. This gives every
 *    request at least til the first timeout to complete its 3-way handshake.
 * 3. Maintain tcp_syn_rcvd_timeout as an accurate count of how many
 *    requests currently on the queue that has timed out. This will be used
 *    as an indicator of whether an attack is under way, so that appropriate
 *    actions can be taken. (It's incremented in tcp_timer() and decremented
 *    either when eager goes into ESTABLISHED, or gets freed up.)
 * 4. The current threshold is - # of timeout > q0len/4 => SYN alert on
 *    # of timeout drops back to <= q0len/32 => SYN alert off
 */
static boolean_t
tcp_drop_q0(tcp_t *tcp)
{
	tcp_t	*eager;

	ASSERT(tcp->tcp_eager_next_q0 != tcp->tcp_eager_prev_q0);
	/* New one is added after next_q0 so prev_q0 points to the oldest */
	eager = tcp->tcp_eager_prev_q0;
	while (eager->tcp_dontdrop) {
		/* XXX should move the eager to the head */
		eager = eager->tcp_eager_prev_q0;
		if (eager == tcp) {
			eager = tcp->tcp_eager_prev_q0;
			break;
		}
	}
	if (eager->tcp_syn_rcvd_timeout == 0)
		return (false);

	(void) mi_strlog(tcp->tcp_rq, 3, SL_TRACE,
	    "tcp_drop_q0: listen half-open queue (max=%d) overflow"
	    " (%d pending) on %s, drop one", tcp_conn_req_max_q0,
	    tcp->tcp_conn_req_cnt_q0, tcp_display(tcp));

	BUMP_MIB(tcp_mib.tcpHalfOpenDrop);
	/* Delete the IRE created for this SYN request */
	tcp_ip_notify(eager);
	(void) tcp_clean_death(eager, ETIMEDOUT);
	return (true);
}

/* Process the connection request packet, mp, directed at the listener 'tcp' */
static void
tcp_conn_request(tcp_t *tcp, mblk_t *mp)
{
	ipa_t	ipa;
	ipha_t	*ipha;
	mblk_t	*mp1;
	tcph_t	*tcph;
	mblk_t	*tpi_mp;
	tcp_t	*eager;
	ire_t	*ire;

	if (tcp->tcp_conn_req_cnt_q >= tcp->tcp_conn_req_max) {
		freemsg(mp);
		BUMP_MIB(tcp_mib.tcpListenDrop);
		(void) mi_strlog(tcp->tcp_rq, 1, SL_TRACE|SL_ERROR,
		    "tcp_conn_request: listen backlog (max=%d) overflow"
		    " (%d pending) on %s", tcp->tcp_conn_req_max,
		    tcp->tcp_conn_req_cnt_q, tcp_display(tcp));
		return;
	}
	if (tcp->tcp_conn_req_cnt_q0 >=
	    tcp->tcp_conn_req_max + tcp_conn_req_max_q0) {
		/*
		 * Q0 is full. Drop a pending half-open req from the queue
		 * to make room for the new SYN req. Also mark the time we
		 * drop a SYN.
		 *
		 * A more aggressive defense against SYN attack will
		 * be to set the "tcp_syn_defense" flag now.
		 */
		tcp->tcp_last_rcv_lbolt = lbolt;
		if (!tcp_drop_q0(tcp)) {
			freemsg(mp);
			BUMP_MIB(tcp_mib.tcpListenDropQ0);
			(void) mi_strlog(tcp->tcp_rq, 3, SL_TRACE,
			    "tcp_conn_request: listen half-open queue (max=%d)"
			    " full (%d pending) on %s", tcp_conn_req_max_q0,
			    tcp->tcp_conn_req_cnt_q0, tcp_display(tcp));
			return;
		}
	}
	mp1 = tcp_ire_mp(mp);
	ipha = (ipha_t *)mp->b_rptr;
	if (!mp1) {
		/*
		 * If we have not yet received an IRE from IP send down
		 * a request. IP will return with an IRE_DB_TYPE at the
		 * end of the message.
		 */
		if (mp1 = allocb(sizeof (ire_t), BPRI_HI)) {
			ire_t	*ire;

			mp1->b_datap->db_type = IRE_DB_REQ_TYPE;
			mp1->b_wptr += sizeof (ire_t);
			ire = (ire_t *)mp1->b_rptr;
			ire->ire_addr = ipha->ipha_src;
			mp1->b_cont = mp;
			putnext(tcp->tcp_wq, mp1);
		} else
			freemsg(mp);
		return;
	}
	/*
	 * Verify that the IRE does not refer to a broadcast address or the
	 * source address is not a multicast address or that no IRE
	 * was found i.e. ire_type is zero.
	 */
	if (!OK_32PTR(mp1->b_rptr)) {
		freemsg(mp);
		freemsg(mp1);
		return;
	}
	ire = (ire_t *)mp1->b_rptr;
	if (ire->ire_type == 0 || (ire->ire_type & IRE_BROADCAST) ||
	    CLASSD(ipha->ipha_src)) {
		freemsg(mp);
		freemsg(mp1);
		return;
	}
	linkb(mp, mp1);
	bzero(&ipa, sizeof (ipa));
	tcph = (tcph_t *)&mp->b_rptr[IPH_HDR_LENGTH(ipha)];
	bcopy(&(ipha->ipha_src), ipa.ip_addr, sizeof (ipa.ip_addr));
	bcopy(tcph->th_lport, ipa.ip_port, sizeof (ipa.ip_port));
	ipa.ip_family = AF_INET;
	tpi_mp = mi_tpi_conn_ind(nilp(mblk_t), (char *)&ipa, sizeof (ipa_t),
	    nilp(char), 0, (t_scalar_t)tcp->tcp_conn_req_seqnum);
	if (!tpi_mp) {
		freemsg(mp);
		return;
	}
	/*
	 * We allow the connection to proceed
	 * by generating a detached tcp state vector which will be
	 * matched up with the accepting stream when/if the accept
	 * ever happens.  The message we are passed looks like:
	 * 	TPI_CONN_IND --> packet
	 */
	eager = tcp_open_detached(tcp->tcp_rq);
	if (!eager) {
		freemsg(mp);
		freemsg(tpi_mp);
		return;
	}
	tcp->tcp_eager_next_q0->tcp_eager_prev_q0 = eager;
	eager->tcp_eager_next_q0 = tcp->tcp_eager_next_q0;
	tcp->tcp_eager_next_q0 = eager;
	eager->tcp_eager_prev_q0 = tcp;

	/* Set tcp_listener before adding it to tcp_conn_fanout */
	eager->tcp_listener = tcp;
	/*
	 * Tag this detached tcp vector for later retrieval
	 * by our listener client in tcp_accept().
	 */
	eager->tcp_conn_req_seqnum = tcp->tcp_conn_req_seqnum;
	tcp->tcp_conn_req_cnt_q0++;
	if (++tcp->tcp_conn_req_seqnum == -1) {
		/*
		 * -1 is "special" and defined in TPI as something
		 * that should never be used in T_CONN_IND
		 */
		++tcp->tcp_conn_req_seqnum;
	}

	tcp_accept_comm(tcp, eager, mp);

	if (tcp->tcp_syn_defense) {
		/* Don't drop the SYN that comes from a good IP source */
		ipaddr_t *addr_cache = (ipaddr_t *)(tcp->tcp_ip_addr_cache);
		if (addr_cache != NULL && eager->tcp_remote ==
		    addr_cache[IP_ADDR_CACHE_HASH(eager->tcp_remote)]) {
			eager->tcp_dontdrop = true;
		}
	}
	/*
	 * Defer passing up T_CONN_IND until the 3-way handshake is complete.
	 */
	ASSERT(eager->tcp_conn.tcp_eager_conn_ind == NULL);
	eager->tcp_conn.tcp_eager_conn_ind = tpi_mp;

	/* OK - same queue since we have eager acceptor on listener */
	ASSERT(tcp->tcp_rq == eager->tcp_rq);
	lateral_put(tcp->tcp_rq, eager->tcp_rq, mp);
}

/*
 * Successful connect request processing begins when our client passes
 * a T_CONN_REQ message into tcp_wput_slow() and ends when tcp_rput() passes
 * our T_OK_ACK reply message upstream.  The control flow looks like this:
 *   upstream -> tcp_wput_slow() -> tcp_connect -> IP
 *   upstream <- tcp_rput()                <- IP
 * After various error checks are completed, tcp_connect() lays
 * the target address and port into the composite header template,
 * preallocates the T_OK_ACK reply message, construct a full 12 byte bind
 * request followed by an IRE request, and passes the three mblk message
 * down to IP looking like this:
 *   O_T_BIND_REQ for IP  --> IRE req --> T_OK_ACK for our client
 * Processing continues in tcp_rput() when we receive the following message:
 *   T_BIND_ACK from IP --> IRE ack --> T_OK_ACK for our client
 * After consuming the first two mblks, tcp_rput() calls tcp_timer(),
 * to fire off the connection request, and then passes the T_OK_ACK mblk
 * upstream that we filled in below.  There are, of course, numerous
 * error conditions along the way which truncate the processing described
 * above.
 */
static void
tcp_connect(queue_t *q, mblk_t *mp)
{
	ipa_t	*ipa;
	in_port_t port;
	mblk_t	*mp1;
	mblk_t	*ok_mp;
	mblk_t	*discon_mp;
	mblk_t  *conn_opts_mp;
	tcp_t	*tcp = (tcp_t *)q->q_ptr;
	tcph_t	*tcph;
	struct T_conn_req	*tcr;
	tcp_t	*ltcp;
	tf_t	*tf;

	tcr = (struct T_conn_req *)mp->b_rptr;

	ASSERT((uintptr_t)(mp->b_wptr - mp->b_rptr) <= (uintptr_t)INT_MAX);
	if ((mp->b_wptr - mp->b_rptr) < sizeof (*tcr) ||
	    ((ipa = (ipa_t *)mi_offset_param(mp, tcr->DEST_offset,
		tcr->DEST_length)) == NULL)) {
		tcp_err_ack(q, mp, TSYSERR, EPROTO);
		return;
	}

	/*
	 * XXX: The check for valid DEST_length was not there in
	 * earlier releases and some buggy TLI apps (e.g Sybase) got
	 * away with not feeding in ip_pad/sin_zero part of address.
	 * We allow that bug to keep those buggy apps humming.
	 * Test suites require this TBADADDR check.
	 */
	if ((tcr->DEST_length != sizeof (*ipa) &&
	    tcr->DEST_length != (sizeof (*ipa) - sizeof (ipa->ip_pad))) ||
	    (!ipa->ip_port[0] && !ipa->ip_port[1])) {
		tcp_err_ack(q, mp, TBADADDR, 0);
		return;
	}
	/*
	 * TODO: If someone in TCPS_TIME_WAIT has this dst/port we
	 * should key on their sequence number and cut them loose.
	 */

	/*
	 * If options passed in, feed it for verification and handling
	 */
	conn_opts_mp = NULL;
	if (tcr->OPT_length != 0) {
		int t_error, sys_error, do_disconnect;

		if (tcp_conprim_opt_process(q, mp, &do_disconnect, &t_error,
		    &sys_error) < 0) {
			if (do_disconnect) {
				ASSERT(t_error == 0 && sys_error == 0);
				discon_mp = mi_tpi_discon_ind(nilp(mblk_t),
				    ECONNREFUSED, 0);
				if (!discon_mp) {
					tcp_err_ack_prim(q, mp, T_CONN_REQ,
					    TSYSERR, ENOMEM);
					return;
				}
				ok_mp = mi_tpi_ok_ack_alloc(mp);
				if (! ok_mp) {
					tcp_err_ack_prim(q, NULL, T_CONN_REQ,
					    TSYSERR, ENOMEM);
					return;
				}
				qreply(q, ok_mp);
				qreply(q, discon_mp); /* no flush! */
			} else {
				ASSERT(t_error != 0);
				tcp_err_ack_prim(q, mp, T_CONN_REQ, t_error,
				    sys_error);
			}
			return;
		}
		/*
		 * Success in setting options, the mp option buffer represented
		 * by OPT_length/offset has been potentially modified and
		 * contains results of option processing. We copy it in
		 * another mp to save it for potentially influencing returning
		 * it in T_CONN_CONN.
		 */
		if (tcr->OPT_length != 0) { /* there are resulting options */
			conn_opts_mp = copyb(mp);
			if (! conn_opts_mp) {
				tcp_err_ack_prim(q, mp, T_CONN_REQ,
				    TSYSERR, ENOMEM);
				return;
			}
			ASSERT(tcp->tcp_conn.tcp_opts_conn_req == NULL);
			tcp->tcp_conn.tcp_opts_conn_req = conn_opts_mp;
			/*
			 * Note:
			 * These resulting option negotiation can include any
			 * end-to-end negotiation options but there no such
			 * thing (yet?) in our TCP/IP.
			 */
		}
	}

	switch (tcp->tcp_state) {
	case TCPS_IDLE:
		/*
		 * We support a quick connect capability here, allowing
		 * clients to transition directly from IDLE to SYN_SENT
		 * tcp_bindi will pick an unused port, insert the connection
		 * in the bind hash and transition to BOUND state.
		 */
		port = tcp_update_next_port(tcp_next_port_to_try);
		port = tcp_bindi(tcp, port, NULL, 0, 0, 0);
		if (port == 0) {
			mp = mi_tpi_err_ack_alloc(mp, TNOADDR, 0);
			break;
		}
		/* FALLTHRU */

	case TCPS_BOUND:
	case TCPS_LISTEN:

		/* Check for attempt to connect to INADDR_ANY */
		if (!ipa->ip_addr[0] && !ipa->ip_addr[1] && !ipa->ip_addr[2] &&
		    !ipa->ip_addr[3]) {
			/*
			 * SunOS 4.x and 4.3 BSD allow an application
			 * to connect a TCP socket to INADDR_ANY.
			 * When they do this, the kernel picks the
			 * address of one interface and uses it
			 * instead.  The kernel usually ends up
			 * picking the address of the loopback
			 * interface.  This is an undocumented feature.
			 * However, we provide the same thing here
			 * in order to have source and binary
			 * compatibility with SunOS 4.x.
			 */
			ipaddr_t inaddr_loopback = htonl(INADDR_LOOPBACK);

			bcopy(&inaddr_loopback, ipa->ip_addr, IP_ADDR_LEN);
		}

		/*
		 * Don't let an endpoint connect to itself.  Note that
		 * the test here does not catch the case where the
		 * source IP addr was left unspecified by the user. In
		 * this case, the source addr is set in tcp_adapt()
		 * using the reply to the T_BIND message that we send
		 * down to IP here.
		 */
		if (BE32_EQL(ipa->ip_addr, tcp->tcp_iph.iph_src) &&
		    BE16_EQL(ipa->ip_port, (u_char *)&tcp->tcp_lport)) {
			mp = mi_tpi_err_ack_alloc(mp, TBADADDR, 0);
			break;
		}

		/*
		 * Question:  What if a src was specified in bind
		 * that does not agree with our current route?  Do we
		 * 	a) Fail the connect
		 *	b) Use the address specified in bind
		 *	c) Change the addr, making it visible here
		 * We implement c) below.
		 */
		bcopy(ipa->ip_addr, tcp->tcp_iph.iph_dst, 4);
		tcp->tcp_remote = tcp->tcp_ipha.ipha_dst;
		/*
		 * Massage a source route if any putting the first hop
		 * in iph_dst. Compute a starting value for the checksum which
		 * takes into account that the original iph_dst should be
		 * included in the checksum but that ip will include the
		 * first hop in the source route in the tcp checksum.
		 */
		tcp->tcp_sum = ip_massage_options(&tcp->tcp_ipha);
		tcp->tcp_sum = (tcp->tcp_sum & 0xFFFF) + (tcp->tcp_sum >> 16);
		tcp->tcp_sum -= ((tcp->tcp_ipha.ipha_dst >> 16) +
		    (tcp->tcp_ipha.ipha_dst & 0xffff));
		if ((int)tcp->tcp_sum < 0)
			tcp->tcp_sum--;
		tcp->tcp_sum = (tcp->tcp_sum & 0xFFFF) + (tcp->tcp_sum >> 16);
		tcp->tcp_sum = ntohs((tcp->tcp_sum & 0xFFFF) +
		    (tcp->tcp_sum >> 16));
		tcph = tcp->tcp_tcph;
		tcph->th_fport[0] = ipa->ip_port[0];
		tcph->th_fport[1] = ipa->ip_port[1];
		bcopy(tcph->th_fport, &tcp->tcp_fport, 2);

		/*
		 * Don't allow this connection to completely duplicate
		 * an existing connection.
		 *
		 * Source address might not be set yet (not until tcp_adapt
		 * is run) but the source address is not part of the
		 * hash and tcp_lookup_match will wildcard if ipha_src is 0.
		 *
		 * Ensure that the duplicate check and insertion is atomic.
		 */
		tcp_conn_hash_remove(tcp);
		tf = &tcp_conn_fanout[TCP_CONN_HASH((uint8_t *)&tcp->tcp_remote,
		    &tcp->tcp_ports)];
		mutex_enter(&tf->tf_lock);
		ltcp = tcp_lookup_match(tf, (uint8_t *)&tcp->tcp_lport,
		    tcp->tcp_iph.iph_src, (uint8_t *)&tcp->tcp_fport,
		    (uint8_t *)&tcp->tcp_remote, TCPS_SYN_SENT);
		if (ltcp) {
			/* found a duplicate connection */
			mutex_exit(&tf->tf_lock);
			TCP_REFRELE(ltcp);
			mp = mi_tpi_err_ack_alloc(mp, TADDRBUSY, 0);
			break;
		}
		tcp_conn_hash_insert(tf, tcp, 1);
		mutex_exit(&tf->tf_lock);

		/*
		 * TODO: allow data with connect requests
		 * by unlinking M_DATA trailers here and
		 * linking them in behind the T_OK_ACK mblk.
		 * The tcp_rput() bind ack handler would then
		 * feed them to tcp_wput_slow() rather than call
		 * tcp_timer().
		 */
		mp = mi_tpi_ok_ack_alloc(mp);
		if (!mp)
			break;
		mp1 = tcp_ip_bind_mp(tcp, O_T_BIND_REQ, sizeof (ipa_conn_t));
		if (mp1) {
			tcp->tcp_state = TCPS_SYN_SENT;
			/* Hang onto the T_OK_ACK for later. */
			linkb(mp1, mp);
			putnext(tcp->tcp_wq, mp1);
			BUMP_MIB(tcp_mib.tcpActiveOpens);
			tcp->tcp_active_open = 1;
			return;
		}
		mp = mi_tpi_err_ack_alloc(mp, TSYSERR, ENOMEM);
		break;
	default:
		mp = mi_tpi_err_ack_alloc(mp, TOUTSTATE, 0);
		break;
	}
	/*
	 * Note: Code below is the "failure" case
	 */
	/* return error ack and blow away saved option results if any */
	if (mp != NULL)
		putnext(tcp->tcp_rq, mp);
	else {
		tcp_err_ack_prim(tcp->tcp_wq, NULL, T_CONN_REQ,
		    TSYSERR, ENOMEM);
	}
	if (tcp->tcp_conn.tcp_opts_conn_req != NULL)
		tcp_close_mpp(&tcp->tcp_conn.tcp_opts_conn_req);
}

/*
 * We need a stream q for detached closing tcp connections
 * to use.  Our client hereby indicates that this q is the
 * one to use.
 */

static void
tcp_def_q_set(queue_t *q, mblk_t *mp)
{
	struct iocblk *iocp = (struct iocblk *)mp->b_rptr;
	tcp_t	*tcp = (tcp_t *)q->q_ptr;

	mp->b_datap->db_type = M_IOCACK;
	iocp->ioc_count = 0;
	mutex_enter(&tcp_g_q_lock);
	if (tcp_g_q != NULL) {
		mutex_exit(&tcp_g_q_lock);
		iocp->ioc_error = EALREADY;
	} else {
		mblk_t *mp1;

		mp1 = tcp_ip_bind_mp(tcp, O_T_BIND_REQ, 0);
		if (mp1 == NULL) {
			mutex_exit(&tcp_g_q_lock);
			iocp->ioc_error = ENOMEM;
		} else {
			tcp_g_q = tcp->tcp_rq;
			mutex_exit(&tcp_g_q_lock);
			iocp->ioc_error = 0;
			iocp->ioc_rval = 0;

			/*
			 * Need to start the time wait collector timeouts
			 * on the default queue.
			 */
			mutex_enter(&tcp_time_wait_lock);
			ASSERT(tcp_time_wait_tid == 0);

			tcp_time_wait_tid = qtimeout(tcp_g_q,
			    tcp_time_wait_collector, NULL,
			    drv_usectohz(1000000));
			mutex_exit(&tcp_time_wait_lock);
			putnext(q, mp1);
		}
	}
	qreply(q, mp);
}

/*
 * tcp_detach is called from tcp_close when a stream is closing
 * before completion of an orderly TCP close sequence.  We detach the tcp
 * structure from its queues and point it at the default queue, tcp_g_q,
 * instead. The tcp structure and resources will be reclaimed eventually by
 * a call to tcp_close_detached.
 */
static boolean_t
tcp_detach(tcp_t *tcp)
{
	queue_t	*wq;
	tf_t	*tf;

	if (!tcp_g_q)
		return (false);
	wq = tcp->tcp_wq;

	if (tcp->tcp_flow_stopped) {
		ASSERT(tcp->tcp_flow_mp->b_prev != NULL ||
		    tcp->tcp_flow_mp->b_next != NULL ||
		    wq->q_first == tcp->tcp_flow_mp);
		rmvq(wq, tcp->tcp_flow_mp);
		tcp->tcp_flow_stopped = 0;
	}
	ASSERT(tcp->tcp_flow_mp == NULL ||
	    (tcp->tcp_flow_mp->b_prev == NULL &&
		tcp->tcp_flow_mp->b_next == NULL &&
		wq->q_first != tcp->tcp_flow_mp));
	/*
	 * There is no need to leave tcp_flow_mp in place (to prevent
	 * tcp_reinit from failing) since once we are detached tcp_reinit
	 * will never be called.
	 */
	tcp_close_mpp(&tcp->tcp_flow_mp);


	/*
	 * tcp_hard_bound is cleared above to force all tcp_rput threads to
	 * access the tcp_t through the tcp_conn_hash. Then, in order to make
	 * the moving of the timers appear atomic with respect to arriving
	 * packets we hold the tf_lock and ensure that the refcnt is 2.
	 * (We have incremented the count to 2 before tcp_detach()
	 * is called in tcp_close().)  Then all of the queue swapping, timer
	 * swapping and time_wait manipulation can be done without
	 * interference from threads in tcp_rput/tcp_wsrv.
	 */
	ASSERT(!tcp->tcp_hard_bound);
	ASSERT(!tcp->tcp_hard_binding);

	tf = &tcp_conn_fanout[TCP_CONN_HASH((u_char *)&tcp->tcp_remote,
	    &tcp->tcp_ports)];

	/* LINTED - constant in conditional context */
	while (1) {
		mutex_enter(&tf->tf_lock);
		mutex_enter(&tcp_mi_lock);
		mutex_enter(&tcp->tcp_reflock);
		/*
		 * Wait for 2 because we have incremented it in tcp_close().
		 * tcp_refcnt cannot go down to 1 since we have not flipped
		 * the queues so all packets should go to syncq waiting to
		 * be processed.
		 */
		if (tcp->tcp_refcnt == 2)
			break;

		/*
		 * Drop locks (so that they can be acquired in the
		 * correct order) and wait for the other reference(s) to
		 * go away.
		 */
		mutex_exit(&tcp_mi_lock);
		mutex_exit(&tf->tf_lock);
		while (tcp->tcp_refcnt > 2)
			cv_wait(&tcp->tcp_refcv, &tcp->tcp_reflock);
		mutex_exit(&tcp->tcp_reflock);
	}
	/* Mark the instance structure detached. */
	mi_detach(&tcp_g_head, (IDP)tcp);
	mutex_exit(&tcp->tcp_reflock);
	mutex_exit(&tcp_mi_lock);

	/*
	 * Now no threads can enter through tcp_rput since tcp_hard_bound is
	 * not set and we are holding the fanout lock. Since tcp_rput can't run
	 * tcp_rsrv can't be scheduled on the on the default queue
	 * with a message for this connection. And tcp_rsrv can't run
	 * on the old queue since we are holding the perimeter for that queue.
	 * Also, tcp_wsrv/tcp_timer can not run on the old queue since the
	 * caller is holding the perimeter for that queue.
	 * However, once mi_timer has been called below they can run on the new
	 * which which is fine since by then everything is consistent.
	 * NOTE: The mi_timer calls have to be last in order for this to
	 * work!
	 */
	tcp->tcp_detached = true;

	tcp->tcp_rq = tcp_g_q;
	tcp->tcp_wq = WR(tcp_g_q);

	if (tcp->tcp_state == TCPS_TIME_WAIT) {
		tcp_time_wait_set(tcp);
		tcp_time_wait_append(tcp);
	}

	/*
	 * Move the timers to the tcp_g_q.
	 * mi_timer_move will moves the timer mblk to the correct queue
	 * even if the timer has fired i.e. the timer mblk has been putq'ed.
	 *
	 * Do not move tcp_ack_mp since it should not be needed anyway.
	 */
	mi_timer_move(WR(tcp_g_q), tcp->tcp_timer_mp);
	mi_timer_move(WR(tcp_g_q), tcp->tcp_co_tmp);
	if (tcp->tcp_ack_mp != NULL) {
		mi_timer_stop(tcp->tcp_ack_mp);
		mi_timer_free(tcp->tcp_ack_mp);
		tcp->tcp_ack_mp = NULL;
	}
	if (tcp->tcp_keepalive_mp)
		mi_timer_move(WR(tcp_g_q), tcp->tcp_keepalive_mp);

	ASSERT(tcp->tcp_refcnt == 2);
	mutex_exit(&tf->tf_lock);
	return (true);
}

/*
 * Our client hereby directs us to reject the connection request
 * that tcp_conn_request() marked with 'seqnum'.  Rejection consists
 * of sending the appropriate RST, not an ICMP error.
 */
static void
tcp_disconnect(queue_t *q, mblk_t *mp)
{
	tcp_t	*tcp = (tcp_t *)q->q_ptr;
	tcp_t	*ltcp;
	t_scalar_t seqnum;

	ASSERT((uintptr_t)(mp->b_wptr - mp->b_rptr) <= (uintptr_t)INT_MAX);
	if ((mp->b_wptr - mp->b_rptr) < sizeof (struct T_discon_req)) {
		tcp_err_ack(q, mp, TSYSERR, EPROTO);
		return;
	}

	seqnum = ((struct T_discon_req *)mp->b_rptr)->SEQ_number;

	if (seqnum == -1 || tcp->tcp_conn_req_max == 0) {

		/*
		 * According to TPI, for non-listeners, ignore seqnum
		 * and disconnect.
		 * Following interpretation of -1 seqnum is historical
		 * and implied TPI ? (TPI only states that for T_CONN_IND,
		 * a valid seqnum should not be -1).
		 *
		 *	-1 means disconnect everything
		 *	regardless even on a listener.
		 */

		int	old_state = tcp->tcp_state;
		mi_timer_stop(tcp->tcp_timer_mp);
		mi_timer_stop(tcp->tcp_ack_mp);
		if (tcp->tcp_co_tintrvl != -1) {
			tcp->tcp_co_tintrvl = -1;
			mi_timer_stop(tcp->tcp_co_tmp);
		}
		if (tcp->tcp_keepalive_mp)
			mi_timer_stop(tcp->tcp_keepalive_mp);
		/*
		 * The connection can't be on the tcp_time_wait_head list
		 * since it is not detached.
		 */
		ASSERT(tcp->tcp_time_wait_next == NULL);
		ASSERT(tcp->tcp_time_wait_prev == NULL);
		ASSERT(tcp->tcp_time_wait_expire == 0);
		ltcp = NULL;
		if (tcp->tcp_conn_req_max &&
		    (ltcp = tcp_lookup_listener((u_char *)&tcp->tcp_lport,
		    tcp->tcp_iph.iph_src)) == NULL) {
			ASSERT(tcp->tcp_ptplhn != NULL);
			tcp->tcp_state = TCPS_LISTEN;
		} else if (old_state >= TCPS_BOUND) {
			tcp->tcp_conn_req_max = 0;
			tcp->tcp_state = TCPS_BOUND;
		}
		if (ltcp != NULL)
			TCP_REFRELE(ltcp);
		if (old_state == TCPS_SYN_SENT || old_state == TCPS_SYN_RCVD)
			BUMP_MIB(tcp_mib.tcpAttemptFails);
		else if (old_state == TCPS_ESTABLISHED ||
		    old_state == TCPS_CLOSE_WAIT)
			BUMP_MIB(tcp_mib.tcpEstabResets);
		if ((tcp->tcp_conn_req_cnt_q0 != 0) ||
		    (tcp->tcp_conn_req_cnt_q != 0))
			tcp_eager_cleanup(tcp, 0);
		tcp_xmit_ctl("tcp_disconnect", tcp, nilp(mblk_t),
		    tcp->tcp_snxt, tcp->tcp_rnxt, TH_RST | TH_ACK);

		tcp_reinit(tcp);
		if (old_state >= TCPS_ESTABLISHED) {
			/* Send M_FLUSH according to TPI */
			(void) putnextctl1(tcp->tcp_rq, M_FLUSH, FLUSHRW);
		}
		mp = mi_tpi_ok_ack_alloc(mp);
		if (mp)
			putnext(tcp->tcp_rq, mp);
		return;
	} else if (!tcp_eager_blowoff(tcp, seqnum)) {
		tcp_err_ack(tcp->tcp_wq, mp, TBADSEQ, 0);
		return;
	}
	if (tcp->tcp_state >= TCPS_ESTABLISHED) {
		/* Send M_FLUSH according to TPI */
		(void) putnextctl1(tcp->tcp_rq, M_FLUSH, FLUSHRW);
	}
	mp = mi_tpi_ok_ack_alloc(mp);
	if (mp)
		putnext(tcp->tcp_rq, mp);
}

/* Diagnostic routine used to return a string associated with the tcp state. */
static char *
tcp_display(tcp_t *tcp)
{
	char	buf1[30];
	static	char	buf[80];
	char	*cp;

	if (!tcp)
		return ("NULL_TCP");
	switch (tcp->tcp_state) {
	case TCPS_CLOSED:
		cp = "TCP_CLOSED";
		break;
	case TCPS_IDLE:
		cp = "TCP_IDLE";
		break;
	case TCPS_BOUND:
		cp = "TCP_BOUND";
		break;
	case TCPS_LISTEN:
		cp = "TCP_LISTEN";
		break;
	case TCPS_SYN_SENT:
		cp = "TCP_SYN_SENT";
		break;
	case TCPS_SYN_RCVD:
		cp = "TCP_SYN_RCVD";
		break;
	case TCPS_ESTABLISHED:
		cp = "TCP_ESTABLISHED";
		break;
	case TCPS_CLOSE_WAIT:
		cp = "TCP_CLOSE_WAIT";
		break;
	case TCPS_FIN_WAIT_1:
		cp = "TCP_FIN_WAIT_1";
		break;
	case TCPS_CLOSING:
		cp = "TCP_CLOSING";
		break;
	case TCPS_LAST_ACK:
		cp = "TCP_LAST_ACK";
		break;
	case TCPS_FIN_WAIT_2:
		cp = "TCP_FIN_WAIT_2";
		break;
	case TCPS_TIME_WAIT:
		cp = "TCP_TIME_WAIT";
		break;
	default:
		(void) mi_sprintf(buf1, "TCPUnkState(%d)", tcp->tcp_state);
		cp = buf1;
		break;
	}
	(void) mi_sprintf(buf, "[%u, %u] %s",
	    ntohs(tcp->tcp_lport), ntohs(tcp->tcp_fport), cp);
	return (buf);
}


/*
 * Reset any eager connection hanging off this listener marked
 * with 'seqnum' and then reclaim it's resources.
 */
static boolean_t
tcp_eager_blowoff(tcp_t	*listener, t_scalar_t seqnum)
{
	tcp_t	*eager;

	eager = listener;
	do {
		eager = eager->tcp_eager_next_q;
		if (!eager)
			return (false);
	} while (eager->tcp_conn_req_seqnum != seqnum);
	tcp_xmit_ctl("tcp_eager_blowoff, can't wait",
	    eager, nilp(mblk_t), eager->tcp_snxt, 0, TH_RST);
	tcp_close_detached(eager);
	return (true);
}

/*
 * Reset any eager connection hanging off this listener
 * and then reclaim it's resources.
 */
static void
tcp_eager_cleanup(tcp_t *listener, int q0_only)
{
	tcp_t	*eager;

	if (!q0_only) {
		/* First cleanup q */
		while ((eager = listener->tcp_eager_next_q) != NULL) {
			tcp_xmit_ctl("tcp_eager_cleanup, can't wait",
			    eager, nilp(mblk_t), eager->tcp_snxt, 0, TH_RST);
			tcp_close_detached(eager);
		}
	}
	/* Then cleanup q0 */
	while ((eager = listener->tcp_eager_next_q0) != listener) {
		tcp_xmit_ctl("tcp_eager_cleanup, can't wait",
		    eager, nilp(mblk_t), eager->tcp_snxt, 0, TH_RST);
		tcp_close_detached(eager);
	}
	ASSERT(listener->tcp_syn_rcvd_timeout == 0);
}

/*
 * If we are an eager connection hanging off a listener that hasn't
 * formally accepted the connection yet, get off his list and blow off
 * any data that we have accumulated.
 */
static void
tcp_eager_unlink(tcp_t *tcp)
{
	tcp_t	*listener = tcp->tcp_listener;

	ASSERT(listener != NULL);
	if (tcp->tcp_state == TCPS_SYN_RCVD) {
		ASSERT(tcp->tcp_eager_next_q0 != NULL);
		ASSERT(tcp->tcp_eager_prev_q0 != NULL);

		/* Remove the eager tcp from q0 */
		tcp->tcp_eager_next_q0->tcp_eager_prev_q0 =
		    tcp->tcp_eager_prev_q0;
		tcp->tcp_eager_prev_q0->tcp_eager_next_q0 =
		    tcp->tcp_eager_next_q0;
		listener->tcp_conn_req_cnt_q0--;

		if (tcp->tcp_syn_rcvd_timeout != 0) {
			/* we have timed out before */
			listener->tcp_syn_rcvd_timeout--;
		}
		ASSERT(listener->tcp_conn_req_cnt_q0 >=
		    listener->tcp_syn_rcvd_timeout);

	} else {
		tcp_t   **tcpp = &listener->tcp_eager_next_q;
		for (; tcpp[0]; tcpp = &tcpp[0]->tcp_eager_next_q) {
			if (tcpp[0] == tcp) {
				tcpp[0] = tcp->tcp_eager_next_q;
				tcp->tcp_eager_next_q = nilp(tcp_t);
				listener->tcp_conn_req_cnt_q--;
				break;
			}
		}
	}
	tcp->tcp_listener = NULL;
}

/* Shorthand to generate and send TPI error acks to our client */
static void
tcp_err_ack(queue_t *q, mblk_t *mp, int t_error, int sys_error)
{
	if ((mp = mi_tpi_err_ack_alloc(mp, t_error, sys_error)) != NULL)
		qreply(q, mp);
}

/* Shorthand to generate and send TPI error acks to our client */
static void
tcp_err_ack_prim(queue_t *q, mblk_t *mp, int primitive,
    int t_error, int sys_error)
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

/*
 * Note: No locks are held when inspecting tcp_g_*epriv_ports
 * but instead the code relies on:
 * - the fact that the address of the array and its size never changes
 * - the atomic assignment of the elements of the array
 */
/* ARGSUSED */
static int
tcp_extra_priv_ports_get(queue_t *q, mblk_t *mp, caddr_t cp)
{
	int i;

	for (i = 0; i < tcp_g_num_epriv_ports; i++) {
		if (tcp_g_epriv_ports[i] != 0)
			(void) mi_mpprintf(mp, "%d ", tcp_g_epriv_ports[i]);
	}
	return (0);
}

/*
 * Hold a lock while changing tcp_g_epriv_ports to prevent multiple
 * threads from changing it at the same time.
 */
/* ARGSUSED */
static int
tcp_extra_priv_ports_add(queue_t *q, mblk_t *mp, char *value, caddr_t cp)
{
	char	*end;
	int	new_value;
	int	i;

	new_value = (int)mi_strtol(value, &end, 10);

	/*
	 * Fail the request if the new value does not lie within the
	 * port number limits.
	 */
	if (end == value || new_value <= 0 || new_value >= 65536)
		return (EINVAL);

	mutex_enter(&tcp_epriv_port_lock);
	/* Check if the value is already in the list */
	for (i = 0; i < tcp_g_num_epriv_ports; i++) {
		if (new_value == tcp_g_epriv_ports[i]) {
			mutex_exit(&tcp_epriv_port_lock);
			return (EEXIST);
		}
	}
	/* Find an empty slot */
	for (i = 0; i < tcp_g_num_epriv_ports; i++) {
		if (tcp_g_epriv_ports[i] == 0)
			break;
	}
	if (i == tcp_g_num_epriv_ports) {
		mutex_exit(&tcp_epriv_port_lock);
		return (EOVERFLOW);
	}
	/* Set the new value */
	tcp_g_epriv_ports[i] = (u16)new_value;
	mutex_exit(&tcp_epriv_port_lock);
	return (0);
}

/*
 * Hold a lock while changing tcp_g_epriv_ports to prevent multiple
 * threads from changing it at the same time.
 */
/* ARGSUSED */
static int
tcp_extra_priv_ports_del(queue_t *q, mblk_t *mp, char *value, caddr_t cp)
{
	char	*end;
	int	new_value;
	int	i;

	new_value = (int)mi_strtol(value, &end, 10);

	/*
	 * Fail the request if the new value does not lie within the
	 * port number limits.
	 */
	if (end == value || new_value <= 0 || new_value >= 65536)
		return (EINVAL);

	mutex_enter(&tcp_epriv_port_lock);
	/* Check that the value is already in the list */
	for (i = 0; i < tcp_g_num_epriv_ports; i++) {
		if (tcp_g_epriv_ports[i] == new_value)
			break;
	}
	if (i == tcp_g_num_epriv_ports) {
		mutex_exit(&tcp_epriv_port_lock);
		return (ESRCH);
	}
	/* Clear the value */
	tcp_g_epriv_ports[i] = 0;
	mutex_exit(&tcp_epriv_port_lock);
	return (0);
}

/* Return the TPI/TLI equivalent of our current tcp_state */
static int
tcp_tpistate(tcp_t *tcp)
{
	switch (tcp->tcp_state) {
	case TCPS_IDLE:
		return (TS_UNBND);
	case TCPS_LISTEN:
		/*
		 * Return whether there are outstanding T_CONN_IND waiting
		 * for the matching T_CONN_RES. Therefore don't count q0.
		 */
		if (tcp->tcp_conn_req_cnt_q > 0)
			return (TS_WRES_CIND);
		else
			return (TS_IDLE);
	case TCPS_BOUND:
		return (TS_IDLE);
	case TCPS_SYN_SENT:
		return (TS_WCON_CREQ);
	case TCPS_SYN_RCVD:
		/*
		 * Note: assumption: this has to the active open SYN_RCVD.
		 * The passive instance is detached in SYN_RCVD stage of
		 * incoming connection processing so we cannot get request
		 * for T_info_ack on it.
		 */
		return (TS_WACK_CRES);
	case TCPS_ESTABLISHED:
		return (TS_DATA_XFER);
	case TCPS_CLOSE_WAIT:
		return (TS_WREQ_ORDREL);
	case TCPS_FIN_WAIT_1:
		return (TS_WIND_ORDREL);
	case TCPS_FIN_WAIT_2:
		return (TS_WIND_ORDREL);

	case TCPS_CLOSING:
	case TCPS_LAST_ACK:
	case TCPS_TIME_WAIT:
		/*
		 * Following TS_WACK_DREQ7 is a rendition of "not
		 * yet TS_IDLE" TPI state. There is no best match to any
		 * TPI state for TCPS_{CLOSING, LAST_ACK, TIME_WAIT} but we
		 * choose a value chosen that will map to TLI/XTI level
		 * state of TSTATECHNG (state is process of changing) which
		 * captures what this dummy state represents.
		 */
		return (TS_WACK_DREQ7);
	default:
		cmn_err(CE_WARN, "tcp_tpistate: strange state (%d) %s\n",
		    tcp->tcp_state, tcp_display(tcp));
		return (TS_UNBND);
	}
}

static void
tcp_copy_info(struct T_info_ack *tia, tcp_t *tcp)
{
	*tia = tcp_g_t_info_ack;
	tia->CURRENT_state = tcp_tpistate(tcp);
	tia->OPT_size = tcp_max_optbuf_len;
	if (tcp->tcp_mss == 0) /* Not yet set - tcp_open does not set mss */
		tia->TIDU_size = tcp_mss_def;
	else
		tia->TIDU_size = tcp->tcp_mss;
	/* TODO: Default ETSDU is 1.  Is that correct for tcp? */
}

#ifdef	TCAP_TEST
static int	tcp_capability_mode	= 0;
#endif	/* TCAP_TEST */

/*
 * This routine responds to T_CAPABILITY_REQ messages.  It is called by
 * tcp_wput.  Much of the T_CAPABILITY_ACK information is copied from
 * tcp_g_t_info_ack.  The current state of the stream is copied from
 * tcp_state.
 */
static void
tcp_capability_req(tcp_t *tcp, mblk_t *mp)
{
	t_uscalar_t		cap_bits1;
	struct T_capability_ack	*tcap;

#ifdef	TCAP_TEST
	/* Used to test the T_CAPABILITY_{REQ,ACK} functionality */
	switch (tcp_capability_mode) {
	case 1:
		(void) putctl1(tcp->tcp_rq, M_ERROR, EPROTO);
		/* FALLTHROUGH */
	case 2:
		freemsg(mp);
		return;

	case 3:
		tcp_err_ack(tcp->tcp_wq, mp, TNOTSUPPORT, 0);
		return;
	}
#endif	/* TCAP_TEST */

	cap_bits1 = ((struct T_capability_req *)mp->b_rptr)->CAP_bits1;

	mp = tpi_ack_alloc(mp, sizeof (struct T_capability_ack),
	    mp->b_datap->db_type, T_CAPABILITY_ACK);
	if (!mp)
		return;

	tcap = (struct T_capability_ack *)mp->b_rptr;
	tcap->CAP_bits1 = 0;

	if (cap_bits1 & TC1_INFO) {
		tcp_copy_info(&tcap->INFO_ack, tcp);
		tcap->CAP_bits1 |= TC1_INFO;
	}

	if (cap_bits1 & TC1_ACCEPTOR_ID) {
		tcap->ACCEPTOR_id = tcp->tcp_acceptor_id;
		tcap->CAP_bits1 |= TC1_ACCEPTOR_ID;
	}

	putnext(tcp->tcp_rq, mp);
}

/*
 * This routine responds to T_INFO_REQ messages.  It is called by tcp_wput.
 * Most of the T_INFO_ACK information is copied from tcp_g_t_info_ack.
 * The current state of the stream is copied from tcp_state.
 */
static void
tcp_info_req(tcp_t *tcp, mblk_t *mp)
{
	mp = tpi_ack_alloc(mp, sizeof (struct T_info_ack), M_PCPROTO,
	    T_INFO_ACK);
	if (!mp) {
		tcp_err_ack(tcp->tcp_wq, mp, TSYSERR, ENOMEM);
		return;
	}
	tcp_copy_info((struct T_info_ack *)mp->b_rptr, tcp);
	putnext(tcp->tcp_rq, mp);
}

/* Respond to the TPI addr request */
static void
tcp_addr_req(tcp_t *tcp, mblk_t *mp)
{
	ipa_t	*ipa;
	mblk_t	*ackmp;
	struct T_addr_ack *taa;
	char	*addr_cp;
	char	*port_cp;

	ackmp = reallocb(mp, sizeof (struct T_addr_ack) +
	    2 * sizeof (ipa_t), 1);
	if (! ackmp) {
		tcp_err_ack(tcp->tcp_wq, mp, TSYSERR, ENOMEM);
		return;
	}

	taa = (struct T_addr_ack *)ackmp->b_rptr;

	bzero(taa, sizeof (struct T_addr_ack));
	ackmp->b_wptr = (u_char *)&taa[1];

	taa->PRIM_type = T_ADDR_ACK;
	ackmp->b_datap->db_type = M_PCPROTO;

	/*
	 * Note: Following code assumes 32 bit alignment of basic
	 * data structures like ipa_t and struct T_addr_ack.
	 */
	if (tcp->tcp_state >= TCPS_BOUND) {
		/*
		 * Fill in local address
		 */
		taa->LOCADDR_length = sizeof (ipa_t);
		taa->LOCADDR_offset = sizeof (*taa);

		ipa = (ipa_t *)&taa[1];

		/* Fill zeroes and then intialize non-zero fields */
		bzero(ipa, sizeof (ipa_t));

		ipa->ip_family = AF_INET;

		addr_cp = (char *)tcp->tcp_iph.iph_src;
		port_cp = (char *)&tcp->tcp_lport;
		bcopy(addr_cp, &ipa->ip_addr, IP_ADDR_LEN);
		bcopy(port_cp, &ipa->ip_port, 2);

		ackmp->b_wptr = (u_char *)&ipa[1];

		if (tcp->tcp_state >= TCPS_SYN_RCVD) {
			/*
			 * Fill in Remote address
			 */
			taa->REMADDR_length = sizeof (ipa_t);
			taa->REMADDR_offset = ROUNDUP32(taa->LOCADDR_offset +
						taa->LOCADDR_length);

			ipa = (ipa_t *)(ackmp->b_rptr + taa->REMADDR_offset);

			/* Fill zeroes and then intialize non-zero fields */
			bzero(ipa, sizeof (ipa_t));

			ipa->ip_family = AF_INET;
			addr_cp = (char *)&tcp->tcp_remote;
			port_cp = (char *)&tcp->tcp_fport;
			bcopy(addr_cp, &ipa->ip_addr, IP_ADDR_LEN);
			bcopy(port_cp, &ipa->ip_port, 2);

			ackmp->b_wptr = (u_char *)&ipa[1];
		}
	}
	putnext(tcp->tcp_rq, ackmp);
}

/*
 * Handle reinitialization of a tcp structure.
 * Maintain "binding state" resetting the state to BOUND, LISTEN, or IDLE.
 * tcp_reinit is guaranteed not to fail if tcp_flow_mp and tcp_timer_mp
 * and tcp_co_tmp and tcp_co_imp already exist.
 */
static void
tcp_reinit(tcp_t *tcp)
{
	mblk_t	*mp;

	/* tcp_reinit should never be called for detached tcp_t's */
	ASSERT(!TCP_IS_DETACHED(tcp));
	ASSERT(tcp->tcp_listener == NULL);
	ASSERT(tcp->tcp_conn_req_max == 0 || tcp->tcp_ptplhn != NULL);

	/* Cancel outstanding timers */
	if (tcp->tcp_co_tintrvl != -1l) {
		tcp->tcp_co_tintrvl = -1l;
		mi_timer_stop(tcp->tcp_co_tmp);
	}
	mi_timer_stop(tcp->tcp_timer_mp);
	mi_timer_stop(tcp->tcp_ack_mp);
	if (tcp->tcp_keepalive_mp)
		mi_timer_stop(tcp->tcp_keepalive_mp);

	/*
	 * Get tcp_flow_mp off the write queue.
	 */
	if (tcp->tcp_flow_stopped) {
		ASSERT(tcp->tcp_flow_mp->b_prev != NULL ||
		    tcp->tcp_flow_mp->b_next != NULL ||
		    tcp->tcp_wq->q_first == tcp->tcp_flow_mp);
		rmvq(tcp->tcp_wq, tcp->tcp_flow_mp);
		tcp->tcp_flow_stopped = 0;
	}
	ASSERT(tcp->tcp_flow_mp == NULL ||
	    (tcp->tcp_flow_mp->b_prev == NULL &&
	    tcp->tcp_flow_mp->b_next == NULL &&
	    tcp->tcp_wq->q_first != tcp->tcp_flow_mp));

	/*
	 * Reset everything in the state vector, after updating global
	 * MIB data from instance counters.
	 */
	UPDATE_MIB(tcp_mib.tcpInSegs, tcp->tcp_ibsegs);
	tcp->tcp_ibsegs = 0;
	UPDATE_MIB(tcp_mib.tcpOutSegs, tcp->tcp_obsegs);
	tcp->tcp_obsegs = 0;

	tcp_close_mpp(&tcp->tcp_xmit_head);
	tcp->tcp_xmit_last = tcp->tcp_xmit_tail = NULL;
	tcp->tcp_unsent = tcp->tcp_xmit_tail_unsent = 0;
	tcp_close_mpp(&tcp->tcp_reass_head);
	tcp->tcp_reass_tail = nilp(mblk_t);
	if (tcp->tcp_co_head) {
		mblk_t	*mp = tcp->tcp_co_head;
		mblk_t	*mp1;
		do {
			mp1 = mp->b_next;
			mp->b_next = nilp(mblk_t);
			freemsg(mp);
		} while ((mp = mp1) != NULL);
		tcp->tcp_co_head = nilp(mblk_t);
		tcp->tcp_co_tail = nilp(mblk_t);
		tcp->tcp_co_cnt = 0;
	}
	if (tcp->tcp_rcv_head) {
		freemsg(tcp->tcp_rcv_head);
		tcp->tcp_rcv_head = nilp(mblk_t);
		tcp->tcp_rcv_cnt = 0;
	}
	tcp->tcp_rcv_tail = nilp(mblk_t);

	if (tcp->tcp_keepalive_mp) {
		mi_timer_free(tcp->tcp_keepalive_mp);
		tcp->tcp_keepalive_mp = nilp(mblk_t);
	}

	if ((mp = tcp->tcp_urp_mp) != NULL) {
		freemsg(mp);
		tcp->tcp_urp_mp = NULL;
	}
	if ((mp = tcp->tcp_urp_mark_mp) != NULL) {
		freemsg(mp);
		tcp->tcp_urp_mark_mp = NULL;
	}

	/*
	 * Following is a union with two members which are
	 * identical types and size so the following cleanup
	 * is enough.
	 */
	tcp_close_mpp(&tcp->tcp_conn.tcp_eager_conn_ind);

	/*
	 * Keep in bind, listen, and queue hash
	 */
	tcp_conn_hash_remove(tcp);

	/*
	 * The connection can't be on the tcp_time_wait_head list
	 * since it is not detached.
	 */
	ASSERT(tcp->tcp_time_wait_next == NULL);
	ASSERT(tcp->tcp_time_wait_prev == NULL);
	ASSERT(tcp->tcp_time_wait_expire == 0);

	/*
	 * Reset/preserve other values
	 */
	tcp_reinit_values(tcp);
	if (tcp->tcp_conn_req_max != 0)
		tcp->tcp_state = TCPS_LISTEN;
	else
		tcp->tcp_state = TCPS_BOUND;

	/*
	 * Initialize to default values
	 */
	tcp_init_values(tcp);

	/* Restore state in tcp_tcph */

	bcopy(&tcp->tcp_lport, tcp->tcp_tcph->th_lport, 2);
	bcopy(&tcp->tcp_bound_source, tcp->tcp_iph.iph_src, IP_ADDR_LEN);

	ASSERT(tcp->tcp_state != TCPS_LISTEN || tcp->tcp_ptplhn != NULL);
	ASSERT(tcp->tcp_ptpbhn != NULL);
	tcp->tcp_rq->q_hiwat = tcp_recv_hiwat;
	tcp_mss_set(tcp, tcp_mss_def);
}

/*
 * Force values to zero that need be zero.
 * Do not touch values asociated with the BOUND or LISTEN state
 * since the connection will end up in that state after the reinit.
 * NOTE: tcp_reinit_values MUST have a line for each field in the tcp_t
 * structure!
 */
static void
tcp_reinit_values(tcp)
	tcp_t *tcp;
{
#define	DONTCARE(x)
#define	PRESERVE(x)

	PRESERVE(tcp->tcp_bind_hash);
	PRESERVE(tcp->tcp_ptpbhn);
	PRESERVE(tcp->tcp_listen_hash);
	PRESERVE(tcp->tcp_ptplhn);
	PRESERVE(tcp->tcp_conn_hash);
	PRESERVE(tcp->tcp_ptpchn);
	PRESERVE(tcp->tcp_acceptor_hash);
	PRESERVE(tcp->tcp_ptpahn);

	/* Should be ASSERT NULL on these with new code! */
	ASSERT(tcp->tcp_time_wait_next == NULL);
	ASSERT(tcp->tcp_time_wait_prev == NULL);
	ASSERT(tcp->tcp_time_wait_expire == 0);
	PRESERVE(tcp->tcp_state);
	PRESERVE(tcp->tcp_rq);
	PRESERVE(tcp->tcp_wq);

	ASSERT(tcp->tcp_xmit_head == NULL);
	ASSERT(tcp->tcp_xmit_last == NULL);
	ASSERT(tcp->tcp_unsent == 0);
	ASSERT(tcp->tcp_xmit_tail == NULL);
	ASSERT(tcp->tcp_xmit_tail_unsent == 0);

	tcp->tcp_snxt = 0;			/* Displayed in mib */
	tcp->tcp_suna = 0;			/* Displayed in mib */
	tcp->tcp_swnd = 0;
	DONTCARE(tcp->tcp_cwnd);		/* Init in tcp_mss_set */

	ASSERT(tcp->tcp_ibsegs == 0);
	ASSERT(tcp->tcp_obsegs == 0);

	tcp->tcp_mss = tcp_mss_def;
	DONTCARE(tcp->tcp_naglim);		/* Init in tcp_init_values */
	DONTCARE(tcp->tcp_hdr_len);		/* Init in tcp_init_values */
	PRESERVE(tcp->tcp_tcph);
	DONTCARE(tcp->tcp_tcp_hdr_len);		/* Init in tcp_init_values */
	tcp->tcp_valid_bits = 0;

	DONTCARE(tcp->tcp_xmit_hiwater);	/* Init in tcp_init_values */
	ASSERT(tcp->tcp_flow_mp != NULL);

	ASSERT(tcp->tcp_timer_mp != NULL);
	DONTCARE(tcp->tcp_timer_backoff);	/* Init in tcp_init_values */
	DONTCARE(tcp->tcp_last_recv_time);	/* Init in tcp_init_values */
	tcp->tcp_last_rcv_lbolt = 0;

	tcp->tcp_urp_last_valid = 0;
	tcp->tcp_hard_binding = 0;
	tcp->tcp_hard_bound = 0;
	PRESERVE(tcp->tcp_priv_stream);

	tcp->tcp_fin_acked = 0;
	tcp->tcp_fin_rcvd = 0;
	tcp->tcp_fin_sent = 0;
	tcp->tcp_ordrel_done = 0;

	ASSERT(tcp->tcp_flow_stopped == 0);
	tcp->tcp_debug = 0;
	tcp->tcp_dontroute = 0;
	tcp->tcp_broadcast = 0;

	tcp->tcp_useloopback = 0;
	tcp->tcp_reuseaddr = 0;
	tcp->tcp_oobinline = 0;
	tcp->tcp_dgram_errind = 0;

	tcp->tcp_detached = 0;
	tcp->tcp_bind_pending = 0;
	tcp->tcp_unbind_pending = 0;

	tcp->tcp_deferred_clean_death = 0;
	tcp->tcp_co_wakeq_done = 0;
	tcp->tcp_co_wakeq_force = 0;
	tcp->tcp_co_norm = 0;

	tcp->tcp_co_wakeq_need = 0;
	tcp->tcp_snd_ws_ok = 0;
	tcp->tcp_snd_ts_ok = 0;
	tcp->tcp_linger = 0;

	tcp->tcp_zero_win_probe = 0;
	tcp->tcp_loopback = 0;
	tcp->tcp_syn_defense = 0;
	tcp->tcp_set_timer = 0;

	tcp->tcp_active_open = 0;
	ASSERT(tcp->tcp_timeout == false);
	tcp->tcp_ack_timer_running = 0;
	tcp->tcp_rexmit = 0;

	tcp->tcp_snd_sack_ok = 0;

	if (tcp->tcp_sack_info != NULL) {
		if (tcp->tcp_notsack_list != NULL) {
			TCP_NOTSACK_REMOVE_ALL(tcp->tcp_notsack_list);
		}
		kmem_free(tcp->tcp_sack_info, sizeof (tcp_sack_info_t));
		tcp->tcp_sack_info = NULL;
	}

	tcp->tcp_rcv_ws = 0;
	tcp->tcp_snd_ws = 0;
	tcp->tcp_ts_recent = 0;
	tcp->tcp_rnxt = 0;			/* Displayed in mib */
	tcp->tcp_rwnd = 0;
	tcp->tcp_rwnd_max = 0;

	ASSERT(tcp->tcp_reass_head == NULL);
	ASSERT(tcp->tcp_reass_tail == NULL);

	tcp->tcp_cwnd_cnt = 0;

	ASSERT(tcp->tcp_rcv_head == NULL);
	ASSERT(tcp->tcp_rcv_tail == NULL);
	ASSERT(tcp->tcp_rcv_cnt == 0);

	ASSERT(tcp->tcp_co_head == NULL);
	ASSERT(tcp->tcp_co_tail == NULL);
	ASSERT(tcp->tcp_co_tmp != NULL);
	PRESERVE(tcp->tcp_co_imp);		/* Could be in transit */

	ASSERT(tcp->tcp_co_tintrvl == -1);
	tcp->tcp_co_rnxt = 0;
	ASSERT(tcp->tcp_co_cnt == 0);

	DONTCARE(tcp->tcp_cwnd_ssthresh);	/* Init in tcp_adapt */
	DONTCARE(tcp->tcp_cwnd_max);		/* Init in tcp_init_values */
	tcp->tcp_csuna = 0;

	tcp->tcp_rto = 0;			/* Displayed in MIB */
	DONTCARE(tcp->tcp_rtt_sa);		/* Init in tcp_init_values */
	DONTCARE(tcp->tcp_rtt_sd);		/* Init in tcp_init_values */
	tcp->tcp_rtt_update = 0;

	DONTCARE(tcp->tcp_swl1); /* Init in case TCPS_LISTEN/TCPS_SYN_SENT */
	DONTCARE(tcp->tcp_swl2); /* Init in case TCPS_LISTEN/TCPS_SYN_SENT */

	tcp->tcp_rack = 0;			/* Displayed in mib */
	tcp->tcp_rack_cnt = 0;
	tcp->tcp_rack_cur_max = 0;
	tcp->tcp_rack_abs_max = 0;
	ASSERT(tcp->tcp_ack_mp != NULL);

	tcp->tcp_max_swnd = 0;

	ASSERT(tcp->tcp_listener == NULL);

	DONTCARE(tcp->tcp_xmit_lowater);	/* Init in tcp_init_values */

	DONTCARE(tcp->tcp_irs);			/* tcp_valid_bits cleared */
	DONTCARE(tcp->tcp_iss);			/* tcp_valid_bits cleared */
	DONTCARE(tcp->tcp_fss);			/* tcp_valid_bits cleared */
	DONTCARE(tcp->tcp_urg);			/* tcp_valid_bits cleared */

	ASSERT(tcp->tcp_conn_req_cnt_q == 0);
	ASSERT(tcp->tcp_conn_req_cnt_q0 == 0);
	PRESERVE(tcp->tcp_conn_req_max);
	PRESERVE(tcp->tcp_conn_req_seqnum);

	DONTCARE(tcp->tcp_ip_hdr_len);		/* Init in tcp_init_values */
	DONTCARE(tcp->tcp_first_timer_threshold); /* Init in tcp_init_values */
	DONTCARE(tcp->tcp_second_timer_threshold); /* Init in tcp_init_values */
	DONTCARE(tcp->tcp_first_ctimer_threshold); /* Init in tcp_init_values */
	DONTCARE(tcp->tcp_second_ctimer_threshold); /* in tcp_init_values */

	tcp->tcp_lingertime = 0;

	DONTCARE(tcp->tcp_urp_last);	/* tcp_urp_last_valid is cleared */
	ASSERT(tcp->tcp_urp_mp == NULL);
	ASSERT(tcp->tcp_urp_mark_mp == NULL);

	ASSERT(tcp->tcp_eager_next_q == NULL);
	ASSERT((tcp->tcp_eager_next_q0 == NULL &&
	    tcp->tcp_eager_prev_q0 == NULL) ||
	    tcp->tcp_eager_next_q0 == tcp->tcp_eager_prev_q0);
	ASSERT(tcp->tcp_conn.tcp_eager_conn_ind == NULL);

	DONTCARE(tcp->tcp_keepalive_intrvl);	/* Init in tcp_init_values */
	ASSERT(tcp->tcp_keepalive_mp == NULL);

	tcp->tcp_client_errno = 0;

	DONTCARE(tcp->tcp_u);			/* Init in tcp_init_values */
	DONTCARE(tcp->tcp_sum);			/* Init in tcp_init_values */

	tcp->tcp_remote = 0;			/* Displayed in MIB */

	PRESERVE(tcp->tcp_bound_source);
	tcp->tcp_last_sent_len = 0;
	tcp->tcp_dupack_cnt = 0;
	tcp->tcp_ill_ick.ick_magic = 0;

	tcp->tcp_fport = 0;			/* Displayed in MIB */
	PRESERVE(tcp->tcp_lport);

	PRESERVE(tcp->tcp_reflock);
	PRESERVE(tcp->tcp_refcnt);
	PRESERVE(tcp->tcp_refcv);

	PRESERVE(tcp->tcp_bind_lockp);
	PRESERVE(tcp->tcp_listen_lockp);
	PRESERVE(tcp->tcp_conn_lockp);
	PRESERVE(tcp->tcp_acceptor_lockp);

	ASSERT(tcp->tcp_ordrelid == 0);

#undef	DONTCARE
#undef	PRESERVE
}

/*
 * Allocate necessary resources and initialize state vector.
 * Guaranteed not to fail if timer_mp and flow_mp and co_tmp
 * and co_imp are non-null so that when an error is returned,
 * the caller doesn't need to do any additional cleanup.
 */
static int
tcp_init(tcp_t *tcp, queue_t *q, mblk_t *timer_mp, mblk_t *ack_mp,
    mblk_t *flow_mp, mblk_t *co_tmp, mblk_t *co_imp)
{
	if (!timer_mp || !ack_mp || !flow_mp || !co_tmp || !co_imp) {
		if (timer_mp)
			mi_timer_free(timer_mp);
		if (ack_mp != NULL)
			mi_timer_free(ack_mp);
		if (flow_mp)
			freemsg(flow_mp);
		if (co_tmp)
			mi_timer_free(co_tmp);
		if (co_imp)
			freemsg(co_imp);
		return (ENOMEM);
	}
	mutex_init(&tcp->tcp_reflock, NULL, MUTEX_DEFAULT, NULL);
	cv_init(&tcp->tcp_refcv, NULL, CV_DEFAULT, NULL);
	tcp->tcp_refcnt = 1;
	tcp->tcp_co_tmp = co_tmp;
	tcp->tcp_co_imp = co_imp;
	tcp->tcp_timer_mp = timer_mp;
	tcp->tcp_ack_mp = ack_mp;
	tcp->tcp_flow_mp = flow_mp;
	flow_mp->b_wptr = flow_mp->b_datap->db_lim;

	tcp->tcp_rq = q;
	tcp->tcp_wq = WR(q);
	tcp->tcp_state = TCPS_IDLE;
	tcp_init_values(tcp);
	return (0);
}

static void
tcp_init_values(tcp_t *tcp)
{
	tcph_t	*tcph;
	u32	sum;

	tcp->tcp_co_tintrvl = -1l;
	tcp->tcp_co_norm = 1;

	/*
	 * Initialize tcp_rtt_sa and tcp_rtt_sd so that the calculated RTO
	 * will be close to tcp_rexmit_interval_initial.  By doing this, we
	 * allow the algorithm to adjust slowly to large fluctuations of RTT
	 * during first few transmissions of a connection as seen in slow
	 * links.
	 */
	tcp->tcp_rtt_sa = tcp_rexmit_interval_initial << 2;
	tcp->tcp_rtt_sd = tcp_rexmit_interval_initial >> 1;
	tcp->tcp_rto = (tcp->tcp_rtt_sa >> 3) + tcp->tcp_rtt_sd +
	    tcp_rexmit_interval_extra + (tcp->tcp_rtt_sa >> 5) +
	    tcp_conn_grace_period;
	tcp->tcp_timer_backoff = 0;
	tcp->tcp_ms_we_have_waited = 0;
	tcp->tcp_last_recv_time = lbolt;
	tcp->tcp_cwnd_max = tcp_cwnd_max_;
	tcp->tcp_snd_burst = TCP_CWND_INFINITE;

	tcp->tcp_first_timer_threshold = tcp_ip_notify_interval;
	tcp->tcp_first_ctimer_threshold = tcp_ip_notify_cinterval;
	tcp->tcp_second_timer_threshold = tcp_ip_abort_interval;
	/*
	 * Fix it to tcp_ip_abort_linterval later if it turns out to be a
	 * passive open.
	 */
	tcp->tcp_second_ctimer_threshold = tcp_ip_abort_cinterval;
	tcp->tcp_keepalive_intrvl = tcp_keepalive_interval;

	tcp->tcp_naglim = tcp_naglim_def;

	/* NOTE:  ISS is now set in tcp_adapt(). */

	tcp->tcp_hdr_len = sizeof (iph_t) + sizeof (tcph_t);
	tcp->tcp_tcp_hdr_len = sizeof (tcph_t);
	tcp->tcp_ip_hdr_len = sizeof (iph_t);
	/*
	 * Init the window scale to the max so tcp_rwnd_set() won't pare
	 * down tcp_rwnd_max. tcp_adapt() will set the right value later.
	 */
	tcp->tcp_rcv_ws = TCP_MAX_WINSHIFT;

	tcp->tcp_xmit_lowater = tcp_xmit_lowat;
	tcp->tcp_xmit_hiwater = tcp_xmit_hiwat;

	/* Initialize the header template */
	U16_TO_BE16(sizeof (iph_t) + sizeof (tcph_t), tcp->tcp_iph.iph_length);
	tcp->tcp_iph.iph_version_and_hdr_length
		= (IP_VERSION << 4) | IP_SIMPLE_HDR_LENGTH_IN_WORDS;
	tcp->tcp_iph.iph_ttl = tcp_ip_ttl;
	tcp->tcp_iph.iph_protocol = IPPROTO_TCP;

	tcph = (tcph_t *)&tcp->tcp_iphc[sizeof (iph_t)];
	tcp->tcp_tcph = tcph;
	tcph->th_offset_and_rsrvd[0] = (5 << 4);
	/*
	 * IP wants our header length in the checksum field to
	 * allow it to perform a single pseudo-header+checksum
	 * calculation on behalf of TCP.
	 * Include the adjustment for a source route if any.
	 */
	sum = sizeof (tcph_t) + tcp->tcp_sum;
	sum = (sum >> 16) + (sum & 0xFFFF);
	U16_TO_ABE16(sum, tcph->th_sum);
}

/*
 * tcp_icmp_error is called by tcp_rput_other to process ICMP error messages
 * passed up by IP.  The queue is the default queue.  We need to find a tcp_t
 * that corresponds to the returned datagram.  Passes the message back in on
 * the correct queue once it has located the connection.
 */
static void
tcp_icmp_error(queue_t *q, mblk_t *mp)
{
	icmph_t *icmph;
	iph_t	*iph;
	int	iph_hdr_length;
	tcp_t	*tcp;
	tcph_t	*tcph;

	iph = (iph_t *)mp->b_rptr;
	iph_hdr_length = IPH_HDR_LENGTH(iph);
	icmph = (icmph_t *)&mp->b_rptr[iph_hdr_length];
	iph = (iph_t *)&icmph[1];
	iph_hdr_length = IPH_HDR_LENGTH(iph);
	tcph = (tcph_t *)((char *)iph + iph_hdr_length);
	tcp = tcp_lookup_reversed(iph, tcph, TCPS_LISTEN);
	if (!tcp) {
		freemsg(mp);
		return;
	}
	if (tcp->tcp_rq != q) {
		lateral_put(q, tcp->tcp_rq, mp);
		TCP_REFRELE(tcp);
		return;
	}
	/* We are on the correct queue */
	TCP_REFRELE(tcp);
	switch (icmph->icmph_type) {
	case ICMP_DEST_UNREACHABLE:
		switch (icmph->icmph_code) {
		case ICMP_FRAGMENTATION_NEEDED:
			/*
			 * Reduce the MSS based on the new MTU.  This will
			 * eliminate any fragmentation locally.
			 * N.B.  There may well be some funny side-effects on
			 * the local send policy and the remote receive policy.
			 * Pending further research, we provide
			 * tcp_ignore_path_mtu just in case this proves
			 * disastrous somewhere.
			 *
			 * After updating the MSS, retransmit part of the
			 * dropped segment using the new mss by calling
			 * tcp_wput_slow().  Need to adjust all those
			 * params to make sure tcp_wput_slow() work properly.
			 */
			if (!tcp_ignore_path_mtu) {
				uint32_t new_mss;

				new_mss = ntohs(icmph->icmph_du_mtu) -
				    tcp->tcp_hdr_len;

				/*
				 * Only update the MSS if the new one is
				 * smaller than the previous one.  This is
				 * to avoid problems when getting multiple
				 * ICMP errors for the same MTU.
				 */
				if (new_mss < tcp->tcp_mss) {
					tcp_mss_set(tcp, new_mss);

					/*
					 * Make sure we have something to
					 * send.
					 */
					if (SEQ_LT(tcp->tcp_suna,
					    tcp->tcp_snxt) &&
					    (tcp->tcp_xmit_head != NULL)) {
						tcp->tcp_cwnd = tcp->tcp_mss;
						if ((tcp->tcp_valid_bits &
						    TCP_FSS_VALID) &&
						    (tcp->tcp_unsent == 0)) {
							tcp->tcp_rexmit_max =
							    tcp->tcp_snxt - 1;
						} else {
							tcp->tcp_rexmit_max =
							    tcp->tcp_snxt;
						}
						tcp->tcp_rexmit_nxt =
						    tcp->tcp_suna;
						tcp->tcp_rexmit = 1;
						tcp_wput_slow(tcp, NULL);
					}
				}
			}
			break;
		case ICMP_PORT_UNREACHABLE:
		case ICMP_PROTOCOL_UNREACHABLE:
			(void) tcp_clean_death(tcp, ECONNREFUSED);
			break;
		case ICMP_HOST_UNREACHABLE:
		case ICMP_NET_UNREACHABLE:
			/* Record the error in case we finally time out. */
			if (icmph->icmph_code == ICMP_HOST_UNREACHABLE)
				tcp->tcp_client_errno = EHOSTUNREACH;
			else
				tcp->tcp_client_errno = ENETUNREACH;
			if (tcp->tcp_state == TCPS_SYN_RCVD) {
				if (tcp->tcp_listener != NULL &&
				    tcp->tcp_listener->tcp_syn_defense) {
					/*
					 * Ditch the half-open connection if we
					 * suspect a SYN attack is under way.
					 */
					tcp_ip_notify(tcp);
					(void) tcp_clean_death(tcp,
					    tcp->tcp_client_errno);
				}
			}
			break;
		default:
			break;
		}
		break;
	case ICMP_SOURCE_QUENCH: {
		/* Reduce the sending rate as if we got a retransmit timeout */
		uint32_t npkt;

		npkt = (MIN(tcp->tcp_cwnd, tcp->tcp_swnd) >> 1) / tcp->tcp_mss;
		if (npkt < 2)
			npkt = 2;
		tcp->tcp_cwnd_ssthresh = npkt * tcp->tcp_mss;
		tcp->tcp_cwnd = tcp->tcp_mss;
		tcp->tcp_cwnd_cnt = 0;
		break;
	}
	}
	freemsg(mp);
}

/*
 * There are three types of binds that IP recognizes.  If we send
 * down a 0 length address, IP will send us packets for which it
 * has no more specific target than "some TCP port".  If we send
 * down a 4 byte address, IP will verify that the address given
 * is a valid local address.  If we send down a full 12 byte address,
 * IP validates both addresses, and then begins to send us only those
 * packets that match completely.  IP will also fill in the IRE
 * request mblk with information regarding our peer.  In all three
 * cases, we notify IP of our protocol type by appending a single
 * protocol byte to the bind request.
 */
static mblk_t *
tcp_ip_bind_mp(tcp_t *tcp, t_scalar_t bind_prim, t_scalar_t addr_length)
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

		/* cp known to be 32 bit aligned */
		ac = (ipa_conn_t *)cp;
		ac->ac_laddr = tcp->tcp_ipha.ipha_src;
		ac->ac_faddr = tcp->tcp_remote;
		ac->ac_fport = tcp->tcp_fport;
		ac->ac_lport = tcp->tcp_lport;
		tcp->tcp_hard_binding = 1;
		break;

	case sizeof (ipa_t):
		ipa = (ipa_t *)cp;

		bzero(ipa, sizeof (*ipa));
		ipa->ip_family = AF_INET;
		*(ipaddr_t *)ipa->ip_addr = tcp->tcp_bound_source;
		*(in_port_t *)ipa->ip_port = tcp->tcp_lport;
		break;

	case IP_ADDR_LEN:
		*(uint32_t *)cp = tcp->tcp_ipha.ipha_src;
		break;
	}
	cp[addr_length] = (char)IPPROTO_TCP;
	mp->b_wptr = (u_char *)&cp[addr_length + 1];
	return (mp);
}

/*
 * Notify IP that we are having trouble with this connection.  IP should
 * blow the IRE away and start over.
 */
static void
tcp_ip_notify(tcp_t *tcp)
{
	struct iocblk	*iocp;
	iph_t	*iph;
	ipid_t	*ipid;
	mblk_t	*mp;

	mp = mkiocb(IP_IOCTL);
	if (!mp)
		return;

	iocp = (struct iocblk *)mp->b_rptr;
	iocp->ioc_count = sizeof (ipid_t) + sizeof (iph->iph_dst);

	mp->b_cont = allocb(iocp->ioc_count, BPRI_HI);
	if (!mp->b_cont) {
		freeb(mp);
		return;
	}

	ipid = (ipid_t *)mp->b_cont->b_rptr;
	mp->b_cont->b_wptr += iocp->ioc_count;
	bzero(ipid, sizeof (*ipid));
	ipid->ipid_cmd = IP_IOC_IRE_DELETE_NO_REPLY;
	ipid->ipid_ire_type = IRE_CACHE;
	ipid->ipid_addr_offset = sizeof (ipid_t);
	ipid->ipid_addr_length = sizeof (tcp->tcp_iph.iph_dst);
	iph = &tcp->tcp_iph;
	/*
	 * Note: in the case of source routing we want to blow away the
	 * route to the first source route hop.
	 */
	bcopy(&iph->iph_dst, &ipid[1], sizeof (iph->iph_dst));

	putnext(tcp->tcp_wq, mp);
}

/*
 * Unbind this stream with IP.
 * Return non-zero if success.
 */
static int
tcp_ip_unbind(queue_t *wq)
{
	mblk_t	*mp;
	struct T_unbind_req	*tur;

	mp = allocb(sizeof (*tur), BPRI_HI);
	if (!mp)
		return (0);
	mp->b_datap->db_type = M_PROTO;
	tur = (struct T_unbind_req *)mp->b_rptr;
	tur->PRIM_type = T_UNBIND_REQ;
	mp->b_wptr += sizeof (*tur);
	putnext(wq, mp);
	return (1);
}

/* Unlink and return any mblk that looks like it contains an ire */
static mblk_t *
tcp_ire_mp(mblk_t *mp)
{
	mblk_t	*prev_mp;

	for (;;) {
		prev_mp = mp;
		mp = mp->b_cont;
		if (!mp)
			break;
		switch (mp->b_datap->db_type) {
		case IRE_DB_TYPE:
		case IRE_DB_REQ_TYPE:
			if (prev_mp)
				prev_mp->b_cont = mp->b_cont;
			mp->b_cont = nilp(mblk_t);
			return (mp);
		default:
			break;
		}
	}
	return (mp);
}

/*
 * Timer callback routine for keepalive probe.  We do a fake resend of
 * last ACKed byte.  Then set a timer using RTO.  When the timer expires,
 * check to see if we have heard anything from the other end for the last
 * RTO period.  If we have, set the timer to expire for another
 * tcp_keepalive_intrvl and check again.  If we have not, set a timer using
 * RTO << 1 and check again when it expires.  Keep exponentially increasing
 * the timeout if we have not heard from the other side.  If for more than
 * (tcp_keepalive_intrvl + tcp_ip_abort_interval) we have not heard anything,
 * kill the connection.
 */
static void
tcp_keepalive_killer(tcp_t *tcp)
{
	mblk_t	*mp;
	tcpka_t	*tcpka;
	int32_t	firetime;
	int32_t	idletime;
	int32_t	ka_intrvl;

	/*
	 * Keepalive probe should only be sent if the application has not
	 * done a close on the connection.
	 */
	if (!tcp->tcp_keepalive_mp || tcp->tcp_state > TCPS_CLOSE_WAIT) {
		return;
	}
	BUMP_MIB(tcp_mib.tcpTimKeepalive);
	ka_intrvl = tcp->tcp_keepalive_intrvl;

	/* Timer fired too early, restart it. */
	if (tcp->tcp_state < TCPS_ESTABLISHED) {
		mi_timer(tcp->tcp_wq, tcp->tcp_keepalive_mp, ka_intrvl);
		return;
	}
	tcpka = (tcpka_t *)tcp->tcp_keepalive_mp->b_rptr;
	idletime = TICK_TO_MSEC(lbolt - tcp->tcp_last_recv_time);
	/*
	 * If we have not heard from the other side for a long
	 * time, kill the connection.
	 */
	if (idletime > (ka_intrvl + tcp->tcp_second_timer_threshold)) {
		BUMP_MIB(tcp_mib.tcpTimKeepaliveDrop);
		(void) tcp_clean_death(tcp, tcp->tcp_client_errno ?
		    tcp->tcp_client_errno : ETIMEDOUT);
		return;
	}

	if (tcp->tcp_snxt == tcp->tcp_suna &&
	    idletime >= ka_intrvl) {
		/* Fake resend of last ACKed byte. */
		mblk_t	*mp1 = allocb(1, BPRI_LO);

		if (mp1) {
			*mp1->b_wptr++ = '\0';
			mp = tcp_xmit_mp(tcp, mp1, 1, NULL, NULL,
			    tcp->tcp_suna - 1, 0);
			freeb(mp1);
			if (mp) {
				putnext(tcp->tcp_wq, mp);
				BUMP_MIB(tcp_mib.tcpTimKeepaliveProbe);
				if (tcpka->tcpka_probe_sent) {
					/*
					 * We should probe again at least
					 * in ka_intrvl, but not more than
					 * tcp_rexmit_interval_max.
					 */
					firetime = MIN(ka_intrvl - 1,
					    tcpka->tcpka_last_intrvl << 1);
					if (firetime > tcp_rexmit_interval_max)
						firetime =
						    tcp_rexmit_interval_max;
				} else {
					firetime = tcp->tcp_rto;
				}
				mi_timer(tcp->tcp_wq,  tcp->tcp_keepalive_mp,
				    firetime);
				tcpka->tcpka_probe_sent++;
				tcpka->tcpka_last_intrvl = firetime;
				return;
			}
		}
	} else {
		tcpka->tcpka_probe_sent = 0;
		tcpka->tcpka_last_intrvl = 0;
	}

	/* firetime can be negative if (mp1 == NULL || mp == NULL) */
	if ((firetime = ka_intrvl - idletime) < 0) {
		firetime = ka_intrvl;
	}
	mi_timer(tcp->tcp_wq, tcp->tcp_keepalive_mp, firetime);
}

/*
 * Walk the list of instantiations and blow off every detached
 * tcp depending on the anchor passed in.
 * Only used when tcp_g_q closes.
 */
static void
tcp_lift_anchor(tcp_t *tcp)
{
	tcp_t	*tcp1, * tcp2;

restart:
	mutex_enter(&tcp_mi_lock);
	/* BEGIN CSTYLED */
	for (tcp2 = (tcp_t *) mi_first_ptr(&tcp_g_head);
	    (tcp1 = tcp2) != NULL; ) {
		/* END CSTYLED */
		tcp2 = (tcp_t *)mi_next_ptr(&tcp_g_head, (IDP)tcp2);
		if (!TCP_IS_DETACHED(tcp1))
			continue;
		if (tcp1->tcp_wq != tcp->tcp_wq && tcp1->tcp_rq != tcp->tcp_rq)
			continue;
		/*
		 * We can drop the lock without tcp1 going away since
		 * it is associated with the same queue as tcp.
		 * However, we have to start the scan all over since
		 * tcp2 might no longer be valid.
		 */
		mutex_exit(&tcp_mi_lock);
		if (tcp1->tcp_state != TCPS_TIME_WAIT) {
			tcp_xmit_ctl("tcp_lift_anchor, can't wait",
			    tcp1, nilp(mblk_t), tcp1->tcp_snxt, 0, TH_RST);
		}
		tcp_close_detached(tcp1);
		goto restart;
	}
	mutex_exit(&tcp_mi_lock);
}

/*
 * Find an exact src/dst/lport/fport match for an incoming datagram.
 * Returns with a TCP_REFHOLD tcp structure. Caller must do a TCP_REFRELE.
 * If rqp is non-NULL it will be set to tcp_rq while tf_lock is held.
 * This is used to atomically (with respect to tcp_eager_swap)
 * get tcp_rq.
 */
static tcp_t *
tcp_lookup(iph_t *iph, tcph_t *tcph, int min_state, queue_t **rqp)
{
	tf_t	*tf;
	tcp_t	*tcp;
	iph_t	*iph1;
	in_port_t ports[2];

	bcopy(tcph->th_fport, &ports[0], sizeof (ports[0]));
	bcopy(tcph->th_lport, &ports[1], sizeof (ports[1]));

	tf = &tcp_conn_fanout[TCP_CONN_HASH(iph->iph_src, (uint8_t *)ports)];
	mutex_enter(&tf->tf_lock);
	for (tcp = tf->tf_tcp; tcp != nilp(tcp_t); tcp = tcp->tcp_conn_hash) {
		iph1 = &tcp->tcp_iph;
		if (BE16_EQL((u_char *)&tcp->tcp_fport, tcph->th_lport) &&
		    BE16_EQL((u_char *)&tcp->tcp_lport, tcph->th_fport) &&
		    BE32_EQL(iph1->iph_src, iph->iph_dst) &&
		    BE32_EQL(&tcp->tcp_remote, iph->iph_src) &&
		    tcp->tcp_state >= min_state) {
			TCP_REFHOLD(tcp);
			if (rqp != NULL)
				*rqp = tcp->tcp_rq;
			mutex_exit(&tf->tf_lock);
			return (tcp);
		}
	}
	mutex_exit(&tf->tf_lock);
	return (NULL);
}

/*
 * Assumes that the caller holds the lock on the hash bucket.
 * Returns with a TCP_REFHOLD tcp structure. Caller must do a TCP_REFRELE.
 */
static tcp_t *
tcp_lookup_match(tf_t *tf, uint8_t *lport, uint8_t *laddr, uint8_t *fport,
    uint8_t *faddr, int min_state)
{
	tcp_t		*tcp;
	in_port_t	ports[2];
	ipaddr_t	src = 0;

	if (laddr) {
		/* we want the address as is, not swapped */
		UA32_TO_U32(laddr, src);
	}

	bcopy(lport, &ports[0], sizeof (ports[0]));
	bcopy(fport, &ports[1], sizeof (ports[1]));

	ASSERT(tf == &tcp_conn_fanout[TCP_CONN_HASH(faddr, (uint8_t *)ports)]);
	ASSERT(MUTEX_HELD(&tf->tf_lock));
	for (tcp = tf->tf_tcp; tcp != nilp(tcp_t); tcp = tcp->tcp_conn_hash) {
		if (BE16_EQL((u_char *)&tcp->tcp_fport, fport) &&
		    BE16_EQL((u_char *)&tcp->tcp_lport, lport) &&
		    (src == 0 || tcp->tcp_ipha.ipha_src == 0 ||
			src == tcp->tcp_ipha.ipha_src) &&
		    BE32_EQL(&tcp->tcp_remote, faddr) &&
		    tcp->tcp_state >= min_state) {
			TCP_REFHOLD(tcp);
			return (tcp);
		}
	}
	return (NULL);
}

/*
 * Find an exact src/dst/lport/fport match for a bounced datagram.
 * Returns with a TCP_REFHOLD tcp structure. Caller must do a TCP_REFRELE.
 */
static tcp_t *
tcp_lookup_reversed(iph_t *iph, tcph_t *tcph, int min_state)
{
	tf_t	*tf;
	tcp_t	*tcp;
	iph_t	*iph1;

	tf = &tcp_conn_fanout[TCP_CONN_HASH(iph->iph_dst, tcph->th_lport)];
	mutex_enter(&tf->tf_lock);
	for (tcp = tf->tf_tcp; tcp != nilp(tcp_t); tcp = tcp->tcp_conn_hash) {
		iph1 = &tcp->tcp_iph;
		if (BE16_EQL((u_char *)&tcp->tcp_fport, tcph->th_fport) &&
		    BE16_EQL((u_char *)&tcp->tcp_lport, tcph->th_lport) &&
		    BE32_EQL(iph1->iph_src, iph->iph_src) &&
		    BE32_EQL(&tcp->tcp_remote, iph->iph_dst) &&
		    tcp->tcp_state >= min_state) {
			TCP_REFHOLD(tcp);
			mutex_exit(&tf->tf_lock);
			return (tcp);
		}
	}
	mutex_exit(&tf->tf_lock);
	return (NULL);
}


/*
 * Find a tcp listening on the specified port.
 * Give preference to exact matches on the IP address.
 * Returns with a TCP_REFHOLD tcp structure. Caller must do a TCP_REFRELE.
 */
static tcp_t *
tcp_lookup_listener(uint8_t *lport, uint8_t *laddr)
{
	tf_t	*tf;
	tcp_t		*tcp;
	ipaddr_t	src = 0;

	if (laddr) {
		/* we want the address as is, not swapped */
		UA32_TO_U32(laddr, src);
	}
	/*
	 * Avoid false matches for packets sent to an IP destination of
	 * all zeros.
	 */
	if (src == 0)
		return (nilp(tcp_t));

	/*
	 * Skip all eager connections which might be in LISTEN state
	 * by checking that tcp_listener is non-NULL.
	 */
	tf = &tcp_listen_fanout[TCP_LISTEN_HASH(lport)];
	mutex_enter(&tf->tf_lock);
	for (tcp = tf->tf_tcp; tcp != nilp(tcp_t);
	    tcp = tcp->tcp_listen_hash) {
		if (BE16_EQL(lport, (u_char *)&tcp->tcp_lport) &&
		    (tcp->tcp_ipha.ipha_src == INADDR_ANY ||
			tcp->tcp_ipha.ipha_src == src) &&
		    tcp->tcp_state == TCPS_LISTEN &&
		    tcp->tcp_listener == NULL) {
			TCP_REFHOLD(tcp);
			mutex_exit(&tf->tf_lock);
			return (tcp);
		}
	}
	mutex_exit(&tf->tf_lock);
	return (NULL);
}

/*
 * Adjust the maxpsz (and maxblk) variables in the stream head to
 * match this connection.
 */
static void
tcp_maxpsz_set(tcp_t *tcp)
{
	if (!TCP_IS_DETACHED(tcp)) {
		queue_t	*q = tcp->tcp_rq;
		int32_t	mss = tcp->tcp_mss;
		int 	maxpsz;

		/*
		 * Set maxpsz to approx half the (receivers) buffer (and a
		 * multiple of the mss). This will allow acks to enter
		 * when the sending thread leaves tcp_wput_slow to go up
		 * to the stream head for more data.
		 * TODO: we don't know the receive buffer size at the remote so
		 * we approximate that with our sndbuf size!
		 * XXX - Ideally the multiplier should be dynamic.
		 */
		maxpsz = tcp_maxpsz_multiplier * mss;
		if (maxpsz > tcp->tcp_xmit_hiwater/2) {
			maxpsz = tcp->tcp_xmit_hiwater/2;
			/* Round up to nearest mss */
			maxpsz = (((maxpsz - 1) / mss) + 1) * mss;
		}
		/*
		 * XXX see comments for setmaxps
		 */
		(void) setmaxps(q, maxpsz);
		tcp->tcp_wq->q_maxpsz = maxpsz;

		(void) mi_set_sth_maxblk(q, mss);
	}
}

/*
 * Extract option values from a tcp header.  We put any found values into the
 * tcpopt struct and return a bitmask saying which options were found.
 */
static int
tcp_parse_options(tcph_t *tcph, tcp_opt_t *tcpopt)
{
	u_char		*endp;
	int		len;
	uint32_t	mss;
	u_char		*up = (u_char *)tcph;
	int		found = 0;
	int32_t		sack_len;
	tcp_seq		sack_begin, sack_end;
	tcp_t		*tcp;

	endp = up + TCP_HDR_LENGTH(tcph);
	up += TCP_MIN_HEADER_LENGTH;
	while (up < endp) {
		len = endp - up;
		switch (*up) {
		case TCPOPT_EOL:
			break;

		case TCPOPT_NOP:
			up++;
			continue;

		case TCPOPT_MAXSEG:
			if (len < TCPOPT_MAXSEG_LEN ||
			    up[1] != TCPOPT_MAXSEG_LEN)
				break;

			mss = BE16_TO_U16(up+2);

			if (mss < tcp_mss_min)
				mss = tcp_mss_min;
			else if (mss > tcp_mss_max)
				mss = tcp_mss_max;

			tcpopt->tcp_opt_mss = mss;
			found |= TCP_OPT_MSS_PRESENT;

			up += TCPOPT_MAXSEG_LEN;
			continue;

		case TCPOPT_WSCALE:
			if (len < TCPOPT_WS_LEN || up[1] != TCPOPT_WS_LEN)
				break;

			if (up[2] > TCP_MAX_WINSHIFT)
				tcpopt->tcp_opt_wscale = TCP_MAX_WINSHIFT;
			else
				tcpopt->tcp_opt_wscale = up[2];
			found |= TCP_OPT_WSCALE_PRESENT;

			up += TCPOPT_WS_LEN;
			continue;

		case TCPOPT_SACK_PERMITTED:
			if (len < TCPOPT_SACK_OK_LEN ||
			    up[1] != TCPOPT_SACK_OK_LEN)
				break;
			found |= TCP_OPT_SACK_OK_PRESENT;
			up += TCPOPT_SACK_OK_LEN;
			continue;

		case TCPOPT_SACK:
			if (len <= 2 || up[1] <= 2 || len < up[1])
				break;

			/* If TCP is not interested in SACK blks... */
			if ((tcp = tcpopt->tcp) == NULL) {
				up += up[1];
				continue;
			}
			sack_len = up[1] - TCPOPT_HEADER_LEN;
			up += TCPOPT_HEADER_LEN;

			/*
			 * If the list is empty, allocate one and assume
			 * nothing is sack'ed.
			 */
			ASSERT(tcp->tcp_sack_info != NULL);
			if (tcp->tcp_notsack_list == NULL) {
				tcp_notsack_update(&(tcp->tcp_notsack_list),
				    tcp->tcp_suna, tcp->tcp_snxt,
				    &(tcp->tcp_num_notsack_blk),
				    &(tcp->tcp_cnt_notsack_list));

				/*
				 * Make sure tcp_notsack_list is not NULL.
				 * This happens when kmem_alloc(KM_NOSLEEP)
				 * returns NULL.
				 */
				if (tcp->tcp_notsack_list == NULL) {
					up += sack_len;
					continue;
				}
				tcp->tcp_fack = tcp->tcp_suna;
			}

			while (sack_len > 0) {
				if (up + 8 > endp) {
					up = endp;
					break;
				}
				sack_begin = BE32_TO_U32(up);
				up += 4;
				sack_end = BE32_TO_U32(up);
				up += 4;
				sack_len -= 8;
				/*
				 * Bounds checking.  Make sure the SACK
				 * info is within tcp_suna and tcp_snxt.
				 * If this SACK blk is out of bound, ignore
				 * it but continue to parse the following
				 * blks.
				 */
				if (SEQ_LEQ(sack_end, sack_begin) ||
				    SEQ_LT(sack_begin, tcp->tcp_suna) ||
				    SEQ_GT(sack_end, tcp->tcp_snxt)) {
					continue;
				}
				tcp_notsack_insert(&(tcp->tcp_notsack_list),
				    sack_begin, sack_end,
				    &(tcp->tcp_num_notsack_blk),
				    &(tcp->tcp_cnt_notsack_list));
				if (SEQ_GT(sack_end, tcp->tcp_fack)) {
					tcp->tcp_fack = sack_end;
				}
			}
			found |= TCP_OPT_SACK_PRESENT;
			continue;

		case TCPOPT_TSTAMP:
			if (len < TCPOPT_TSTAMP_LEN ||
			    up[1] != TCPOPT_TSTAMP_LEN)
				break;

			tcpopt->tcp_opt_ts_val = BE32_TO_U32(up+2);
			tcpopt->tcp_opt_ts_ecr = BE32_TO_U32(up+6);

			found |= TCP_OPT_TSTAMP_PRESENT;

			up += TCPOPT_TSTAMP_LEN;
			continue;

		default:
			if (len <= 1 || len < (int)up[1] || up[1] == 0)
				break;
			up += up[1];
			continue;
		}
		break;
	}
	return (found);
}

/*
 * Set the mss associated with a particular tcp based on its current value,
 * and a new one passed in. Observe minimums and maximums, and reset
 * other state variables that we want to view as multiples of mss.
 *
 * This function is called in various places mainly because
 * 1) tcp_mss has to be initialized correctly on the local side before
 *    the first packet (SYN/SYN-ACK) goes out, since only a SYN/SYN-ACK
 *    packet may carry a mss option.
 * 2) tcp_mss may needs to adjust when the other side's SYN/SYN-ACK
 *    packet arrives and is smaller than the local one.
 * 3) tcp_accept() call it to set q_maxpsz and q_hiwat on the correct,
 *    acceptor stream.
 */
static void
tcp_mss_set(tcp_t *tcp, uint32_t mss)
{
	if (mss < tcp_mss_min)
		mss = tcp_mss_min;
	if (mss > tcp_mss_max)
		mss = tcp_mss_max;
	/*
	 * Unless naglim has been set by our client to
	 * a non-mss value, force naglim to track mss.
	 * This can help to aggregate small writes.
	 */
	if (mss < tcp->tcp_naglim || tcp->tcp_mss == tcp->tcp_naglim)
		tcp->tcp_naglim = mss;
	if (mss > tcp->tcp_xmit_hiwater)
		tcp->tcp_xmit_hiwater = mss;
	tcp->tcp_mss = mss;
	/*
	 * BSD derived TCP stacks have the interesting feature that if
	 * the connection is established thru passive open, the initial
	 * cwnd will be equal to 2 MSS.  We make this value controllable
	 * by a ndd param.
	 *
	 * Also refer to Sally Floyd's proposal of larger initial cwnd
	 * size.
	 */
	tcp->tcp_cwnd = MIN(tcp_slow_start_initial * mss, tcp->tcp_cwnd_max);
	tcp->tcp_cwnd_cnt = 0;
	/*
	 * Always use q_hiwat from tcp_rq with tcp_recv_hiwat_minmss * mss
	 * as a lower bound. This makes the acceptor inherits listner's
	 * q_hiwat automatically, or in the case of a detached tcp hanging
	 * off tcp_g_q, use tcp_g_q->q_hiwat, which has the default
	 * tcp_recv_hiwat.
	 */
	(void) tcp_rwnd_set(tcp,
	    MAX(tcp->tcp_rq->q_hiwat, tcp_recv_hiwat_minmss * mss));
	tcp_maxpsz_set(tcp);
}

/*
 * Called by STREAMS to associate a new
 * tcp instantiation with the q passed in.
 */
static int
tcp_open(queue_t *q, dev_t *devp, int flag, int sflag, cred_t *credp)
{
	int	err;
	tcp_t	*tcp;

	if (q->q_ptr)
		return (0);
	if (sflag != MODOPEN)
		return (EINVAL);

	/*
	 * We are D_MTQPAIR so it is safe to do qprocson before allocating
	 * q_ptr. Needed by the time the tcp_acceptor_hash_insert is done.
	 */
	qprocson(q);

	tcp = (tcp_t *)mi_open_alloc(sizeof (tcp_t));
	if (tcp == NULL) {
		qprocsoff(q);
		return (ENOMEM);
	}
	q->q_ptr = WR(q)->q_ptr = tcp;
	err = tcp_init(tcp, q, tcp_timer_alloc(tcp, tcp_timer, 0),
	    tcp_timer_alloc(tcp, tcp_ack_timer, 0),
	    allocb(tcp_winfo.mi_hiwat + 1, BPRI_LO),
	    tcp_timer_alloc(tcp, tcp_co_timer, 0),
	    mkiocb(I_SYNCSTR));
	if (err != 0) {
		mi_close_free((IDP)tcp);
		q->q_ptr = WR(q)->q_ptr = NULL;
		qprocsoff(q);
		return (err);
	}

	tcp->tcp_mss = tcp_mss_def;
	RD(q)->q_hiwat = tcp_recv_hiwat;
	if (drv_priv(credp) == 0)
		tcp->tcp_priv_stream = 1;

	/*
	 * Modify global lists that require the global lock
	 */
	mutex_enter(&tcp_mi_lock);
	err = mi_open_link(&tcp_g_head, (IDP)tcp, devp, flag, sflag, credp);
	mutex_exit(&tcp_mi_lock);
	if (err != 0) {
		tcp_closei_local(tcp);
		TCP_REFRELE(tcp);
		q->q_ptr = WR(q)->q_ptr = NULL;
		qprocsoff(q);
		return (err);
	}
#ifdef	_ILP32
	tcp->tcp_acceptor_id = (t_uscalar_t)backq(tcp->tcp_rq);
#else
	tcp->tcp_acceptor_id = (t_uscalar_t)getminor(*devp);
#endif	/* _ILP32 */
	tcp_acceptor_hash_insert(tcp->tcp_acceptor_id, tcp);
	return (0);
}

/*
 * Called when we need a new tcp instantiation but don't really have a
 * new q to hang it off of. Copy the priv flag from the passed in structure.
 */
static tcp_t *
tcp_open_detached(queue_t *q)
{
	tcp_t	*ptcp, *tcp;
	int err;

	ptcp = (tcp_t *)q->q_ptr;

	tcp = (tcp_t *)mi_open_alloc(sizeof (tcp_t));
	if (tcp == NULL) {
		return (NULL);
	}
	err = tcp_init(tcp, ptcp->tcp_rq, tcp_timer_alloc(tcp, tcp_timer, 0),
	    tcp_timer_alloc(tcp, tcp_ack_timer, 0),
	    allocb(tcp_winfo.mi_hiwat + 1, BPRI_LO),
	    tcp_timer_alloc(tcp, tcp_co_timer, 0),
	    mkiocb(I_SYNCSTR));
	if (err != 0) {
		mi_close_free((IDP)tcp);
		return (NULL);
	}

	/* Inherit information from the "parent" */
	tcp->tcp_mss = tcp_mss_def;
	tcp->tcp_detached = true;
	tcp->tcp_priv_stream = ptcp->tcp_priv_stream;

	/*
	 * Modify global lists that require the global lock
	 */
	mutex_enter(&tcp_mi_lock);
	err = mi_open_link(&tcp_g_head, (IDP)tcp, NULL, 0, 0, NULL);
	mutex_exit(&tcp_mi_lock);
	if (err != 0) {
		tcp_closei_local(tcp);
		TCP_REFRELE(tcp);
		return (NULL);
	}
	return (tcp);
}

/*
 * Some TCP options can be "set" by requesting them in the option
 * buffer. This is needed for XTI feature test though we do not
 * allow it in general. We interpret that this mechanism is more
 * applicable to OSI protocols and need not be allowed in general.
 * This routine filters out options for which it is not allowed (most)
 * and lets through those (few) for which it is. [ The XTI interface
 * test suite specifics will imply that any XTI_GENERIC level XTI_* if
 * ever implemented will have to be allowed here ].
 */
static boolean_t
tcp_allow_connopt_set(int level, int name)
{

	switch (level) {
	case IPPROTO_TCP:
		switch (name) {
		case TCP_NODELAY:
			return (true);
		default:
			return (false);
		}
		/*NOTREACHED*/
	default:
		return (false);
	}
	/*NOTREACHED*/
}

/*
 * This routine gets default values of certain options whose default
 * values are maintained by protocol specific code
 */

/* ARGSUSED */
int
tcp_opt_default(queue_t *q, int level, int name, u_char *ptr)
{
	int32_t	*i1 = (int32_t *)ptr;

	switch (level) {
	case IPPROTO_TCP:
		switch (name) {
		case TCP_NOTIFY_THRESHOLD:
			*i1 = tcp_ip_notify_interval;
			break;
		case TCP_ABORT_THRESHOLD:
			*i1 = tcp_ip_abort_interval;
			break;
		case TCP_CONN_NOTIFY_THRESHOLD:
			*i1 = tcp_ip_notify_cinterval;
			break;
		case TCP_CONN_ABORT_THRESHOLD:
			*i1 = tcp_ip_abort_cinterval;
			break;
		default:
			return (-1);
		}
		break;
	default:
		return (-1);
	}
	return (sizeof (int));
}


/*
 * TCP routine to get the values of options.
 */
int
tcp_opt_get(queue_t *q, int level, int	name, u_char *ptr)
{
	int32_t	*i1 = (int32_t *)ptr;
	tcp_t	*tcp = (tcp_t *)q->q_ptr;

	switch (level) {
	case SOL_SOCKET:
		switch (name) {
		case SO_LINGER:	{
			struct linger *lgr = (struct linger *)ptr;

			lgr->l_onoff = tcp->tcp_linger ? SO_LINGER : 0;
			lgr->l_linger = tcp->tcp_lingertime;
			}
			return (sizeof (struct linger));
		case SO_DEBUG:
			*i1 = tcp->tcp_debug ? SO_DEBUG : 0;
			break;
		case SO_KEEPALIVE:
			*i1 = tcp->tcp_keepalive_mp ? SO_KEEPALIVE : 0;
			break;
		case SO_DONTROUTE:
			*i1 = tcp->tcp_dontroute ? SO_DONTROUTE : 0;
			break;
		case SO_USELOOPBACK:
			*i1 = tcp->tcp_useloopback ? SO_USELOOPBACK : 0;
			break;
		case SO_BROADCAST:
			*i1 = tcp->tcp_broadcast ? SO_BROADCAST : 0;
			break;
		case SO_REUSEADDR:
			*i1 = tcp->tcp_reuseaddr ? SO_REUSEADDR : 0;
			break;
		case SO_OOBINLINE:
			*i1 = tcp->tcp_oobinline ? SO_OOBINLINE : 0;
			break;
		case SO_DGRAM_ERRIND:
			*i1 = tcp->tcp_dgram_errind ? SO_DGRAM_ERRIND : 0;
			break;
		case SO_TYPE:
			*i1 = SOCK_STREAM;
			break;
		case SO_SNDBUF:
			*i1 = tcp->tcp_xmit_hiwater;
			break;
		case SO_RCVBUF:
			*i1 = RD(q)->q_hiwat;
			break;
		default:
			return (-1);
		}
		break;
	case IPPROTO_TCP:
		switch (name) {
		case TCP_NODELAY:
			*i1 = (tcp->tcp_naglim == 1) ? TCP_NODELAY : 0;
			break;
		case TCP_MAXSEG:
			*i1 = tcp->tcp_mss;
			break;
		case TCP_NOTIFY_THRESHOLD:
			*i1 = (int)tcp->tcp_first_timer_threshold;
			break;
		case TCP_ABORT_THRESHOLD:
			*i1 = tcp->tcp_second_timer_threshold;
			break;
		case TCP_CONN_NOTIFY_THRESHOLD:
			*i1 = tcp->tcp_first_ctimer_threshold;
			break;
		case TCP_CONN_ABORT_THRESHOLD:
			*i1 = tcp->tcp_second_ctimer_threshold;
			break;
		default:
			return (-1);
		}
		break;
	case IPPROTO_IP:
		switch (name) {
		case IP_OPTIONS: {
			/*
			 * This is compatible with BSD in that in only return
			 * the reverse source route with the final destination
			 * as the last entry. The first 4 bytes of the option
			 * will contain the final destination.
			 */
			char	*opt_ptr;
			int	opt_len;
			opt_ptr = (char *)&tcp->tcp_iph + IP_SIMPLE_HDR_LENGTH;
			opt_len = (char *)tcp->tcp_tcph - opt_ptr;
			/* Caller ensures enough space */
			if (opt_len > 0) {
				/*
				 * TODO: Do we have to handle getsockopt on an
				 * initiator as well?
				 */
				return (tcp_opt_get_user(&tcp->tcp_iph, ptr));
			}
			return (0);
			}
		case IP_TOS:
			*i1 = (int)tcp->tcp_iph.iph_type_of_service;
			break;
		case IP_TTL:
			*i1 = (int)tcp->tcp_iph.iph_ttl;
			break;
		default:
			return (-1);
		}
		break;
	default:
		return (-1);
	}
	return (sizeof (int));
}

/*
 * We declare as 'int' rather than 'void' to satisfy pfi_t arg requirements.
 * Parameters are assumed to be verified by the caller.
 */

int
tcp_opt_set(queue_t *q, u_int mgmt_flags, int level, int name, u_int inlen,
    u_char *invalp, u_int *outlenp, u_char *outvalp)
{
	mblk_t	*mp1;
	int32_t	*i1 = (int32_t *)invalp;
	tcp_t	*tcp = (tcp_t *)q->q_ptr;
	tcpka_t *tcpka;
	int	reterr;
	int	checkonly;

	if (mgmt_flags == (T_NEGOTIATE|T_CHECK)) {
		/*
		 * both set - magic signal that
		 * negotiation not from T_OPTMGMT_REQ
		 *
		 * Negotiating local and "association-related" options
		 * from other (T_CONN_REQ, T_CONN_RES,T_UNITDATA_REQ)
		 * primitives is allowed by XTI, but we choose
		 * to not implement this style negotiation for Internet
		 * protocols (We interpret it is a must for OSI world but
		 * optional for Internet protocols) for all options.
		 * [ Will do only for the few options that enable test
		 * suites that our XTI implementation of this feature
		 * works for transports that do allow it ]
		 */
		if (! tcp_allow_connopt_set(level, name)) {
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
		 * 	value part in T_CHECK request and just validation
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
		case SO_LINGER: {
			struct linger *lgr = (struct linger *)invalp;

			if (! checkonly) {
				if (lgr->l_onoff) {
					tcp->tcp_linger = 1;
					tcp->tcp_lingertime = lgr->l_linger;
				} else {
					tcp->tcp_linger = 0;
					tcp->tcp_lingertime = 0;
				}
				/* struct copy */
				*(struct linger *)outvalp = *lgr;
			} else {
				if (! lgr->l_onoff) {
				    ((struct linger *)outvalp)->l_onoff = 0;
				    ((struct linger *)outvalp)->l_linger = 0;
				} else {
				    /* struct copy */
				    *(struct linger *)outvalp = *lgr;
				}
			}
			*outlenp = sizeof (struct linger);
			return (0);
		}
		case SO_DEBUG:
			if (! checkonly)
				tcp->tcp_debug = *i1 ? 1 : 0;
			break; /* goto sizeof(int) option return */
		case SO_KEEPALIVE:
			if (checkonly) {
				/* T_CHECK case */
				break; /* goto sizeof (int) option return */
			}
			mp1 = tcp->tcp_keepalive_mp;
			if (*i1) {
				if (!mp1) {
					/* Crank up the keepalive timer */
					mp1 = tcp_timer_alloc(tcp,
					    tcp_keepalive_killer,
					    sizeof (tcpka_t));
					if (!mp1) {
						*outlenp = 0;
						return (ENOMEM);
					}
					tcpka = (tcpka_t *)mp1->b_rptr;
					tcpka->tcpka_tcp = tcp;
					tcpka->tcpka_probe_sent = 0;
					tcpka->tcpka_last_intrvl =
					    tcp->tcp_keepalive_intrvl;
					tcp->tcp_keepalive_mp = mp1;
					mi_timer(tcp->tcp_wq,
					    tcp->tcp_keepalive_mp,
					    tcp->tcp_keepalive_intrvl);
				}
			} else {
				if (mp1) {
					/* Shut down the keepalive timer */
					mi_timer_free(mp1);
					tcp->tcp_keepalive_mp = nilp(mblk_t);
				}
			}
			break;	/* goto sizeof(int) option return */
		case SO_DONTROUTE:
			/*
			 * SO_DONTROUTE, SO_USELOOPBACK and SO_BROADCAST are
			 * only of interest to IP.  We track them here only so
			 * that we can report their current value.
			 */
			if (! checkonly)
				tcp->tcp_dontroute = *i1 ? 1 : 0;
			break;	/* goto sizeof (int) option return */
		case SO_USELOOPBACK:
			if (! checkonly)
				tcp->tcp_useloopback = *i1 ? 1 : 0;
			break;	/* goto sizeof (int) option return */
		case SO_BROADCAST:
			if (! checkonly)
				tcp->tcp_broadcast = *i1 ? 1 : 0;
			break;	/* goto sizeof (int) option return */
		case SO_REUSEADDR:
			if (! checkonly)
				tcp->tcp_reuseaddr = *i1 ? 1 : 0;
			break;	/* goto sizeof (int) option return */
		case SO_OOBINLINE:
			if (! checkonly)
				tcp->tcp_oobinline = *i1 ? 1 : 0;
			break;	/* goto sizeof (int) option return */
		case SO_DGRAM_ERRIND:
			if (! checkonly)
				tcp->tcp_dgram_errind = *i1 ? 1 : 0;
			break;	/* goto sizeof (int) option return */
		case SO_SNDBUF:
			if (*i1 > tcp_max_buf) {
				*outlenp = 0;
				return (ENOBUFS);
			}
			if (! checkonly) {
				tcp->tcp_xmit_hiwater = *i1;
				if (tcp_snd_lowat_fraction != 0)
					tcp->tcp_xmit_lowater =
					    tcp->tcp_xmit_hiwater /
					    tcp_snd_lowat_fraction;
				tcp_maxpsz_set(tcp);
			}
			break;	/* goto sizeof (int) option return */
		case SO_RCVBUF:
			if (*i1 > tcp_max_buf) {
				*outlenp = 0;
				return (ENOBUFS);
			}
			if (! checkonly) {
				RD(q)->q_hiwat = *i1;
				(void) tcp_rwnd_set(tcp, *i1);
			}
			/*
			 * XXX should we return the rwnd here
			 * and tcp_opt_get ?
			 */
			break;	/* goto sizeof (int) option return */
		default:
			*outlenp = 0;
			return (EINVAL);
		}
		break;
	case IPPROTO_TCP:
		switch (name) {
		case TCP_NODELAY:
			if (! checkonly)
				tcp->tcp_naglim = *i1 ? 1 : tcp->tcp_mss;
			break;	/* goto sizeof (int) option return */
		case TCP_NOTIFY_THRESHOLD:
			if (! checkonly)
				tcp->tcp_first_timer_threshold = *i1;
			break;	/* goto sizeof (int) option return */
		case TCP_ABORT_THRESHOLD:
			if (! checkonly)
				tcp->tcp_second_timer_threshold = *i1;
			break;	/* goto sizeof (int) option return */
		case TCP_CONN_NOTIFY_THRESHOLD:
			if (! checkonly)
				tcp->tcp_first_ctimer_threshold = *i1;
			break;	/* goto sizeof (int) option return */
		case TCP_CONN_ABORT_THRESHOLD:
			if (! checkonly)
				tcp->tcp_second_ctimer_threshold = *i1;
			break;	/* goto sizeof (int) option return */
		default:
			*outlenp = 0;
			return (EINVAL);
		}
		break;
	case IPPROTO_IP:
		switch (name) {
		case IP_OPTIONS:
			reterr = tcp_opt_set_header(tcp, checkonly,
			    invalp, inlen);
			if (reterr) {
				*outlenp = 0;
				return (reterr);
			}
			/* OK return - copy input buffer into output buffer */
			if (invalp != outvalp) {
				/* don't trust bcopy for identical src/dst */
				(void) bcopy(invalp, outvalp, inlen);
			}
			*outlenp = inlen;
			return (0);
		case IP_TOS:
			if (! checkonly)
				tcp->tcp_iph.iph_type_of_service =
				    (u_char) *i1;
			break;	/* goto sizeof (int) option return */
		case IP_TTL:
			if (! checkonly)
				tcp->tcp_iph.iph_ttl = (u_char) *i1;
			break;	/* goto sizeof (int) option return */
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
 * Use the outgoing IP header to create an IP_OPTIONS option the way
 * it was passed down from the application.
 */
static int
tcp_opt_get_user(iph_t *iph, u_char *buf)
{
	u_char		*opt;
	int		totallen;
	uint32_t	optval;
	uint32_t	optlen;
	uint32_t	len = 0;
	u_char	*buf1 = buf;

	buf += IP_ADDR_LEN;	/* Leave room for final destination */
	len += IP_ADDR_LEN;
	bzero(buf1, IP_ADDR_LEN);

	totallen = iph->iph_version_and_hdr_length -
		(uint8_t)((IP_VERSION << 4) + IP_SIMPLE_HDR_LENGTH_IN_WORDS);
	opt = (u_char *)&iph[1];
	totallen <<= 2;
	while (totallen != 0) {
		switch (optval = opt[IPOPT_POS_VAL]) {
		case IPOPT_EOL:
			goto done;
		case IPOPT_NOP:
			optlen = 1;
			break;
		default:
			optlen = opt[IPOPT_POS_LEN];
		}
		if (optlen == 0 || optlen > totallen)
			break;

		switch (optval) {
			int	off;
		case IPOPT_SSRR:
		case IPOPT_LSRR:

			/*
			 * Insert iph_dst as the first entry in the source
			 * route and move down the entries on step.
			 * The last entry gets placed at buf1.
			 */
			buf[IPOPT_POS_VAL] = optval;
			buf[IPOPT_POS_LEN] = optlen;
			buf[IPOPT_POS_OFF] = optlen;

			off = optlen - IP_ADDR_LEN;
			if (off < 0) {
				/* No entries in source route */
				break;
			}
			/* Last entry in source route */
			bcopy(opt + off, buf1, IP_ADDR_LEN);
			off -= IP_ADDR_LEN;

			while (off > 0) {
				bcopy(opt + off, buf + off + IP_ADDR_LEN,
				    IP_ADDR_LEN);
				off -= IP_ADDR_LEN;
			}
			/* iph_dst into first slot */
			bcopy(iph->iph_dst, buf + off + IP_ADDR_LEN,
			    IP_ADDR_LEN);
			buf += optlen;
			len += optlen;
			break;
		default:
			bcopy(opt, buf, optlen);
			buf += optlen;
			len += optlen;
			break;
		}
		totallen -= optlen;
		opt += optlen;
	}
done:
	/* Pad the resulting options */
	while (len & 0x3) {
		*buf++ = IPOPT_EOL;
		len++;
	}
	return (len);
}

/*
 * Transfer any source route option from iph to buf/dst in reversed form.
 */
static int
tcp_opt_rev_src_route(iph_t *iph, char *buf, u_char *dst)
{
	uint32_t	totallen;
	u_char		*opt;
	uint32_t	optval;
	uint32_t	optlen;
	uint32_t	len = 0;

	totallen = iph->iph_version_and_hdr_length -
		(uint8_t)((IP_VERSION << 4) + IP_SIMPLE_HDR_LENGTH_IN_WORDS);
	opt = (u_char *)&iph[1];
	totallen <<= 2;
	while (totallen != 0) {
		switch (optval = opt[IPOPT_POS_VAL]) {
		case IPOPT_EOL:
			goto done;
		case IPOPT_NOP:
			optlen = 1;
			break;
		default:
			optlen = opt[IPOPT_POS_LEN];
		}
		if (optlen == 0 || optlen > totallen)
			break;

		switch (optval) {
			int	off1, off2;
		case IPOPT_SSRR:
		case IPOPT_LSRR:

			/* Reverse source route */
			/*
			 * First entry should be the next to last one in the
			 * current source route (the last entry is our
			 * address.)
			 * The last entry should be the final destination.
			 */
			buf[IPOPT_POS_VAL] = (uint8_t)optval;
			buf[IPOPT_POS_LEN] = (uint8_t)optlen;
			off1 = IPOPT_MINOFF_SR - 1;
			off2 = opt[IPOPT_POS_OFF] - IP_ADDR_LEN - 1;
			if (off2 < 0) {
				/* No entries in source route */
				break;
			}
			bcopy(opt + off2, dst, IP_ADDR_LEN);
			/*
			 * Note: use src since iph has not had its src
			 * and dst reversed (it is in the state it was
			 * received.
			 */
			bcopy(iph->iph_src, buf + off2, IP_ADDR_LEN);
			off2 -= IP_ADDR_LEN;

			while (off2 > 0) {
				bcopy(opt + off2, buf + off1, IP_ADDR_LEN);
				off1 += IP_ADDR_LEN;
				off2 -= IP_ADDR_LEN;
			}
			buf[IPOPT_POS_OFF] = IPOPT_MINOFF_SR;
			buf += optlen;
			len += optlen;
			break;
		}
		totallen -= optlen;
		opt += optlen;
	}
done:
	/* Pad the resulting options */
	while (len & 0x3) {
		*buf++ = IPOPT_EOL;
		len++;
	}
	return (len);
}


/*
 * Extract and revert a source route from iph (if any)
 * and then update the relevant fields in both tcp_t and the standard header.
 */
static void
tcp_opt_reverse(tcp_t *tcp, iph_t *iph)
{
	char	buf[TCP_MAX_HDR_LENGTH];
	u_int	tcph_len;
	int	len;

	len = IPH_HDR_LENGTH(iph);
	if (len == IP_SIMPLE_HDR_LENGTH)
		/* Nothing to do */
		return;
	if (len > IP_SIMPLE_HDR_LENGTH + TCP_MAX_IP_OPTIONS_LENGTH ||
	    (len & 0x3))
		return;

	tcph_len = tcp->tcp_tcp_hdr_len;
	bcopy(tcp->tcp_tcph, buf, tcph_len);
	tcp->tcp_sum = (tcp->tcp_ipha.ipha_dst >> 16) +
		(tcp->tcp_ipha.ipha_dst & 0xffff);
	len = tcp_opt_rev_src_route(iph, &tcp->tcp_iphc[IP_SIMPLE_HDR_LENGTH],
	    tcp->tcp_iph.iph_dst);
	len += IP_SIMPLE_HDR_LENGTH;
	tcp->tcp_sum -= ((tcp->tcp_ipha.ipha_dst >> 16) +
	    (tcp->tcp_ipha.ipha_dst & 0xffff));
	if ((int)tcp->tcp_sum < 0)
		tcp->tcp_sum--;
	tcp->tcp_sum = (tcp->tcp_sum & 0xFFFF) + (tcp->tcp_sum >> 16);
	tcp->tcp_sum = ntohs((tcp->tcp_sum & 0xFFFF) + (tcp->tcp_sum >> 16));
	tcp->tcp_tcph = (tcph_t *)&tcp->tcp_iphc[len];
	bcopy(buf, tcp->tcp_tcph, tcph_len);
	tcp->tcp_ip_hdr_len = len;
	tcp->tcp_iph.iph_version_and_hdr_length =
	    (IP_VERSION << 4) | (len >> 2);
	len += tcph_len;
	tcp->tcp_hdr_len = len;
}

/*
 * Copy the standard header into its new location,
 * lay in the new options and then update the relevant
 * fields in both tcp_t and the standard header.
 * NOTE: this could be simpler if we trusted bcopy()
 * with overlapping src/dst.
 */
static int
tcp_opt_set_header(tcp_t *tcp, int checkonly, u_char *ptr, u_int len)
{
	char	buf[TCP_MAX_HDR_LENGTH];
	u_int	tcph_len;

	if (checkonly) {
		/*
		 * do not really set, just pretend to - T_CHECK
		 */
		if (len != 0) {
			/*
			 * there is value supplied, validate it as if
			 * for a real set operation.
			 */
			if (len > TCP_MAX_IP_OPTIONS_LENGTH || (len & 0x3))
				return (EINVAL);
		}
		return (0);
	}

	if (len > TCP_MAX_IP_OPTIONS_LENGTH || (len & 0x3))
		return (EINVAL);
	tcph_len = tcp->tcp_tcp_hdr_len;
	bcopy(tcp->tcp_tcph, buf, tcph_len);
	bcopy(ptr, &tcp->tcp_iphc[IP_SIMPLE_HDR_LENGTH], len);
	len += IP_SIMPLE_HDR_LENGTH;
	tcp->tcp_tcph = (tcph_t *)&tcp->tcp_iphc[len];
	bcopy(buf, tcp->tcp_tcph, tcph_len);
	tcp->tcp_ip_hdr_len = len;
	tcp->tcp_iph.iph_version_and_hdr_length =
		(IP_VERSION << 4) | (len >> 2);
	len += tcph_len;
	tcp->tcp_hdr_len = len;
	if (!TCP_IS_DETACHED(tcp)) {
		/* Always allocate room for all options. */
		(void) mi_set_sth_wroff(tcp->tcp_rq,
		    TCP_MAX_COMBINED_HEADER_LENGTH + tcp_wroff_xtra);
	}
	return (0);
}

/* Get callback routine passed to nd_load by tcp_param_register */
/* ARGSUSED */
static int
tcp_param_get(queue_t *q, mblk_t *mp, caddr_t cp)
{
	tcpparam_t	*tcppa = (tcpparam_t *)cp;

	(void) mi_mpprintf(mp, "%ld", tcppa->tcp_param_val);
	return (0);
}

/*
 * Walk through the param array specified registering each element with the
 * named dispatch handler.
 */
static boolean_t
tcp_param_register(tcpparam_t *tcppa, int cnt)
{
	for (; cnt-- > 0; tcppa++) {
		if (tcppa->tcp_param_name && tcppa->tcp_param_name[0]) {
			if (!nd_load(&tcp_g_nd, tcppa->tcp_param_name,
			    tcp_param_get, tcp_param_set,
			    (caddr_t)tcppa)) {
				nd_free(&tcp_g_nd);
				return (false);
			}
		}
	}
	if (!nd_load(&tcp_g_nd, "tcp_extra_priv_ports",
	    tcp_extra_priv_ports_get, nil(pfi_t), nil(caddr_t))) {
		nd_free(&tcp_g_nd);
		return (false);
	}
	if (!nd_load(&tcp_g_nd, "tcp_extra_priv_ports_add",
	    nil(pfi_t), tcp_extra_priv_ports_add, nil(caddr_t))) {
		nd_free(&tcp_g_nd);
		return (false);
	}
	if (!nd_load(&tcp_g_nd, "tcp_extra_priv_ports_del",
	    nil(pfi_t), tcp_extra_priv_ports_del, nil(caddr_t))) {
		nd_free(&tcp_g_nd);
		return (false);
	}
	if (!nd_load(&tcp_g_nd, "tcp_status", tcp_status_report, nil(pfi_t),
	    nil(caddr_t))) {
		nd_free(&tcp_g_nd);
		return (false);
	}
	if (!nd_load(&tcp_g_nd, "tcp_bind_hash", tcp_bind_hash_report,
	    nil(pfi_t), nil(caddr_t))) {
		nd_free(&tcp_g_nd);
		return (false);
	}
	if (!nd_load(&tcp_g_nd, "tcp_listen_hash", tcp_listen_hash_report,
	    nil(pfi_t), nil(caddr_t))) {
		nd_free(&tcp_g_nd);
		return (false);
	}
	if (!nd_load(&tcp_g_nd, "tcp_conn_hash", tcp_conn_hash_report,
	    nil(pfi_t), nil(caddr_t))) {
		nd_free(&tcp_g_nd);
		return (false);
	}
	if (!nd_load(&tcp_g_nd, "tcp_acceptor_hash", tcp_acceptor_hash_report,
	    nil(pfi_t), nil(caddr_t))) {
		nd_free(&tcp_g_nd);
		return (false);
	}
	if (!nd_load(&tcp_g_nd, "tcp_host_param", tcp_host_param_report,
	    tcp_host_param_set, nil(caddr_t))) {
		nd_free(&tcp_g_nd);
		return (false);
	}
	if (!nd_load(&tcp_g_nd, "tcp_1948_phrase", NULL, tcp_1948_phrase_set,
	    NULL)) {
		nd_free(&tcp_g_nd);
		return (false);
	}
	return (true);
}

/* Set callback routine passed to nd_load by tcp_param_register */
/* ARGSUSED */
static int
tcp_param_set(queue_t *q, mblk_t *mp, char *value, caddr_t cp)
{
	char	*end;
	int32_t	new_value;
	tcpparam_t	*tcppa = (tcpparam_t *)cp;

	new_value = mi_strtol(value, &end, 10);
	if (end == value || new_value < tcppa->tcp_param_min ||
	    new_value > tcppa->tcp_param_max)
		return (EINVAL);
	tcppa->tcp_param_val = new_value;
	return (0);
}

/*
 * Add a new piece to the tcp reassembly queue.  If the gap at the beginning
 * is filled, return as much as we can.  The message passed in may be
 * multi-part, chained using b_cont.  "start" is the starting sequence
 * number for this piece.
 */
static mblk_t *
tcp_reass(tcp_t *tcp, mblk_t *mp, uint32_t start, u_int *flagsp)
{
	uint32_t	end;
	mblk_t		*mp1;
	mblk_t		*mp2;
	mblk_t		*next_mp;
	uint32_t	u1;

	/* Walk through all the new pieces. */
	do {
		ASSERT((uintptr_t)(mp->b_wptr - mp->b_rptr) <=
		    (uintptr_t)INT_MAX);
		end = start + (int)(mp->b_wptr - mp->b_rptr);
		next_mp = mp->b_cont;
		if (start == end) {
			/* Empty.  Blast it. */
			freeb(mp);
			continue;
		}
		mp->b_cont = nilp(mblk_t);
		TCP_REASS_SET_SEQ(mp, start);
		TCP_REASS_SET_END(mp, end);
		mp1 = tcp->tcp_reass_tail;
		if (!mp1) {
			tcp->tcp_reass_tail = mp;
			tcp->tcp_reass_head = mp;
			/* A fresh gap.	 Make sure we get an ACK out. */
			*flagsp |= TH_ACK_NEEDED;
			BUMP_MIB(tcp_mib.tcpInDataUnorderSegs);
			UPDATE_MIB(tcp_mib.tcpInDataUnorderBytes, end - start);
			continue;
		}
		/* New stuff completely beyond tail? */
		if (SEQ_GEQ(start, TCP_REASS_END(mp1))) {
			/* Link it on end. */
			mp1->b_cont = mp;
			tcp->tcp_reass_tail = mp;
			BUMP_MIB(tcp_mib.tcpInDataUnorderSegs);
			UPDATE_MIB(tcp_mib.tcpInDataUnorderBytes, end - start);
			continue;
		}
		mp1 = tcp->tcp_reass_head;
		u1 = TCP_REASS_SEQ(mp1);
		/* New stuff at the front? */
		if (SEQ_LT(start, u1)) {
			/* Yes... Check for overlap. */
			mp->b_cont = mp1;
			tcp->tcp_reass_head = mp;
			tcp_reass_elim_overlap(tcp, mp);
			continue;
		}
		/*
		 * The new piece fits somewhere between the head and tail.
		 * We find our slot, where mp1 precedes us and mp2 trails.
		 */
		for (; (mp2 = mp1->b_cont) != NULL; mp1 = mp2) {
			u1 = TCP_REASS_SEQ(mp2);
			if (SEQ_LEQ(start, u1))
				break;
		}
		/* Link ourselves in */
		mp->b_cont = mp2;
		mp1->b_cont = mp;

		/* Trim overlap with following mblk(s) first */
		tcp_reass_elim_overlap(tcp, mp);

		/* Trim overlap with preceding mblk */
		tcp_reass_elim_overlap(tcp, mp1);

	} while (start = end, mp = next_mp);
	mp1 = tcp->tcp_reass_head;
	/* Anything ready to go? */
	if (TCP_REASS_SEQ(mp1) != tcp->tcp_rnxt)
		return (nilp(mblk_t));
	/* Eat what we can off the queue */
	for (;;) {
		mp = mp1->b_cont;
		end = TCP_REASS_END(mp1);
		TCP_REASS_SET_SEQ(mp1, 0);
		TCP_REASS_SET_END(mp1, 0);
		if (!mp) {
			tcp->tcp_reass_tail = nilp(mblk_t);
			break;
		}
		if (end != TCP_REASS_SEQ(mp)) {
			mp1->b_cont = nilp(mblk_t);
			break;
		}
		mp1 = mp;
	}
	mp1 = tcp->tcp_reass_head;
	tcp->tcp_reass_head = mp;
	if (mp) {
		/*
		 * We filled in the hole at the front, but there is still
		 * a gap.  Make sure another ACK goes out.
		 */
		*flagsp |= TH_ACK_NEEDED;
	}
	return (mp1);
}

/* Eliminate any overlap that mp may have over later mblks */
static void
tcp_reass_elim_overlap(tcp_t *tcp, mblk_t *mp)
{
	uint32_t	end;
	mblk_t		*mp1;
	uint32_t	u1;

	end = TCP_REASS_END(mp);
	while ((mp1 = mp->b_cont) != NULL) {
		u1 = TCP_REASS_SEQ(mp1);
		if (!SEQ_GT(end, u1))
			break;
		if (!SEQ_GEQ(end, TCP_REASS_END(mp1))) {
			mp->b_wptr -= end - u1;
			TCP_REASS_SET_END(mp, u1);
			BUMP_MIB(tcp_mib.tcpInDataPartDupSegs);
			UPDATE_MIB(tcp_mib.tcpInDataPartDupBytes, end - u1);
			break;
		}
		mp->b_cont = mp1->b_cont;
		TCP_REASS_SET_SEQ(mp1, 0);
		TCP_REASS_SET_END(mp1, 0);
		freeb(mp1);
		BUMP_MIB(tcp_mib.tcpInDataDupSegs);
		UPDATE_MIB(tcp_mib.tcpInDataDupBytes, end - u1);
	}
	if (!mp1)
		tcp->tcp_reass_tail = mp;
}

/*
 * tcp_co_drain is called to drain the co queue.
 */

#define	CCS_STATS 0

#if CCS_STATS
struct {
	struct {
		int64_t count, bytes;
	} hit, rrw, mis, spc, seq, len, uio, tim, uer, cer, oth;
} rrw_stats;
#endif

static void
tcp_co_drain(tcp_t *tcp)
{
	queue_t	*q = tcp->tcp_rq;
	mblk_t	*mp;
	mblk_t	*mp1;

	if (tcp->tcp_co_tintrvl != -1L) {
		/*
		 * Cancel outstanding co timer.
		 */
		tcp->tcp_co_tintrvl = -1L;
		mi_timer_stop(tcp->tcp_co_tmp);
	}
	/*
	 * About to putnext() some message(s) causing a transition to
	 * normal STREAMS mode, so cleanup any co queue state first.
	 */
	tcp->tcp_co_wakeq_force = 0;
	tcp->tcp_co_wakeq_need = 0;
	tcp->tcp_co_wakeq_done = 0;
	while ((mp = tcp->tcp_co_head) != NULL) {
		if ((tcp->tcp_co_head = mp->b_next) == NULL)
			tcp->tcp_co_tail = nilp(mblk_t);
		mp->b_next = nilp(mblk_t);
		if (mp->b_datap->db_struioflag & STRUIO_IP) {
			/*
			 * Delayed IP checksum required.
			 */
			int off;
			ASSERT((uintptr_t)(mp->b_datap->db_struiobase -
			    mp->b_rptr) <= (uintptr_t)INT_MAX);
			off = (int)(mp->b_datap->db_struiobase -
			    mp->b_rptr);

			if (IP_CSUM(mp, off, 0)) {
				/*
				 * Checksum error, so drop it.
				 */
				BUMP_MIB(ip_mib.tcpInErrs);
				ipcsumdbg("tcp_co_drain: cksumerr\n", mp);
				freemsg(mp);
				continue;
			}
		}
		/*
		 * A normal mblk now, so clear the struioflag and rput() it.
		 */
		mp1 = mp;
		do
			mp1->b_datap->db_struioflag &= ~(STRUIO_IP|STRUIO_SPEC);
		while ((mp1 = mp1->b_cont) != NULL);
		tcp_rput_data(q, mp, -1);
	}
	tcp->tcp_co_cnt = 0;
	if ((mp = tcp->tcp_rcv_head) != NULL) {
		/*
		 * The rcv push queue has mblks, putnext() them.
		 */
		tcp->tcp_rcv_head = nilp(mblk_t);
		tcp->tcp_rcv_cnt = 0;
		tcp->tcp_co_norm = 1;
		putnext(q, mp);
	}
}

/*
 * tcp_co_timer is the timer service routine for the co queue.
 * It handles timer events for a tcp instance setup by tcp_rput().
 */
static void
tcp_co_timer(tcp_t *tcp)
{
	tcp->tcp_co_tintrvl = -1;
#if CCS_STATS
	rrw_stats.tim.count++;
	rrw_stats.tim.bytes += tcp->tcp_co_cnt;
#endif
	tcp_co_drain(tcp);
}

/* The read side info procedure. */
static int
tcp_rinfop(queue_t *q, infod_t *dp)
{
	tcp_t		*tcp = (tcp_t *)q->q_ptr;
	mblk_t		*mp = tcp->tcp_rcv_head;
	unsigned	cmd = dp->d_cmd;
	unsigned	res = 0;
	int		list = 0;
	int		n;

	/*
	 * We have two list of mblk(s) to scan (rcv (push) & co).
	 *
	 * Note: mblks on the co still have IP and TCP headers, so
	 *	 the header lengths must be accounted for !!!
	 */
	if (! mp) {
		list++;
		mp = tcp->tcp_co_head;
	}
	if (mp) {
		/*
		 * We have enqueued mblk(s).
		 */
		if (cmd & INFOD_COUNT) {
			/*
			 * Count one msg for either push or co queue, and two
			 * for both. The co queue may end up being pushed as
			 * multiple messages, but underestimating should be ok.
			 */
			dp->d_count++;
			if (! list && tcp->tcp_co_head)
				dp->d_count++;
			res |= INFOD_COUNT;
		}
		if (cmd & INFOD_FIRSTBYTES) {
			dp->d_bytes = msgdsize(mp);
			if (list) {
				TCPIP_HDR_LENGTH(mp, n);
				dp->d_bytes -= n;
			}
			res |= INFOD_FIRSTBYTES;
			dp->d_cmd &= ~INFOD_FIRSTBYTES;
		}
		if (cmd & INFOD_COPYOUT) {
			mblk_t	*mp1 = mp;
			u_char	*rptr = mp1->b_rptr;
			int error;

			if (list) {
				if (mp->b_datap->db_struioflag & STRUIO_IP) {
					/*
					 * Delayed IP checksum required.
					 */
					int off;
					ASSERT((uintptr_t)
					    (mp->b_datap->db_struiobase -
					    mp->b_rptr) <= (uintptr_t)INT_MAX);
					off = (int)
					    (mp->b_datap->db_struiobase -
					    mp->b_rptr);

					if (IP_CSUM(mp, off, 0))
						/*
						 * Bad checksum, so just
						 * skip the INFOD_COPYOUT.
						 */
						goto skip;
				}
				TCPIP_HDR_LENGTH(mp1, n);
				rptr += n;
			}
			while (dp->d_uiop->uio_resid) {
				n = MIN(dp->d_uiop->uio_resid,
				    mp1->b_wptr - rptr);
				if (n != 0 && (error = uiomove((char *)rptr, n,
				    UIO_READ, dp->d_uiop)) != 0)
					return (error);
				if ((mp1 = mp1->b_cont) == NULL)
					break;
				rptr = mp1->b_rptr;
			}
			res |= INFOD_COPYOUT;
			dp->d_cmd &= ~INFOD_COPYOUT;
		skip:;
		}
		if (cmd & INFOD_BYTES) {
			do {
				if (cmd & INFOD_BYTES) {
					dp->d_bytes += msgdsize(mp);
					if (list) {
						TCPIP_HDR_LENGTH(mp, n);
						dp->d_bytes -= n;
					}
				}
				if (list)
					mp = mp->b_next;
				else {
					list++;
					mp = tcp->tcp_co_head;
				}
			} while (mp);
			res |= INFOD_BYTES;
		}
	}
	if (res)
		dp->d_res |= res;

	if (isuioq(q))
		/*
		 * This is the struio() Q (last), nothing more todo.
		 */
		return (0);

	if (dp->d_cmd)
		/*
		 * Need to look at all mblk(s) or haven't completed
		 * all cmds, so pass info request on.
		 */
		return (infonext(q, dp));

	return (0);
}

/* The read side r/w procedure. */
static int
tcp_rrw(queue_t *q, struiod_t *dp)
{
	tcp_t	*tcp = (tcp_t *)q->q_ptr;
	uio_t	*uiop = &dp->d_uio;
	mblk_t	*mp;
	mblk_t	*mp1;
	mblk_t	*mp2;
	int	needstruio;
	int	needcksum;
	int	err = 0;

	if (tcp->tcp_co_norm || tcp->tcp_co_imp == NULL)
		/*
		 * The stream is currently in normal mode or in transition to
		 * synchronous mode, so just return EBUSY.
		 */
		return (EBUSY);
	if ((mp = tcp->tcp_rcv_head) != NULL) {
		/*
		 * The rcv push queue has mblk(s), so account for them in the
		 * uio, they will be returned ahead of any struioput() mblk(s)
		 * below.
		 *
		 * Note: we assume that the rcv push queue count is
		 *	 accurate and only M_DATA mblk(s) are enqueued.
		 */
		uioskip(uiop, tcp->tcp_rcv_cnt);
	}
	/*
	 * While uio and a co enqueued segment:
	 */
	while (uiop->uio_resid > 0 && (mp = tcp->tcp_co_head)) {
		if (tcp->tcp_co_tintrvl != -1L) {
			/*
			 * Cancel outstanding co timer.
			 */
			tcp->tcp_co_tintrvl = -1L;
			mi_timer_stop(tcp->tcp_co_tmp);
		}
		/*
		 * Dequeue segment from the co queue.
		 */
		if ((tcp->tcp_co_head = mp->b_next) == NULL)
			tcp->tcp_co_tail = nilp(mblk_t);
		mp->b_next = nilp(mblk_t);
		/*
		 * Find last mblk of the segment (mblk chain),
		 * also see if we need to do struioput() and if
		 * we need to check the resulting IP checksum.
		 */
		mp1 = mp;
		needcksum = needstruio = 0;
		do {
			if (mp1->b_datap->db_struioflag & STRUIO_SPEC) {
				needstruio++;
				if (mp1->b_datap->db_struioflag & STRUIO_IP)
					needcksum++;
			}
			mp2 = mp1;
		} while ((mp1 = mp1->b_cont) != NULL);

		if (needstruio && (err = struioput(q, mp, dp, 1))) {
			/*
			 * Uio error of some sort, so process
			 * this segment and drain the co queue.
			 */
			if (needcksum && IP_CSUM(mp,
			    (int)(mp->b_datap->db_struiobase - mp->b_rptr),
			    0)) {
				/*
				 * Checksum error, so drop it.
				 */
				BUMP_MIB(ip_mib.tcpInErrs);
				ipcsumdbg("tcp_rrw: cksumerr\n", mp);
				freemsg(mp);
			} else
				tcp_rput_data(q, mp, 0);
#if CCS_STATS
			rrw_stats.uer.count++;
			rrw_stats.uer.bytes += tcp->tcp_co_cnt;
#endif
			/*
			 * Reset tcp_co_cnt here if this is the last mblk
			 * because we are breaking out of the loop and don't
			 * get a chance to call tcp_rput_data() to adjust it.
			 */
			if (tcp->tcp_co_head)
				tcp_co_drain(tcp);
			else
				tcp->tcp_co_cnt = 0;
			break;
		}
		if (needcksum) {
			int off;
			ASSERT((uintptr_t)(mp->b_datap->db_struiobase -
			    mp->b_rptr) <= (uintptr_t)INT_MAX);
			off = (int)(mp->b_datap->db_struiobase - mp->b_rptr);

			if (IP_CSUM(mp, off, 0)) {
				/*
				 * Checksum error, so drop it.
				 */
				BUMP_MIB(ip_mib.tcpInErrs);
				ipcsumdbg("tcp_rrw: cksumerr2\n", mp);
				freemsg(mp);
#if CCS_STATS
				rrw_stats.cer.count++;
				rrw_stats.cer.bytes += tcp->tcp_co_cnt;
#endif
				if (tcp->tcp_co_head)
					tcp_co_drain(tcp);
				else
					tcp->tcp_co_cnt = 0;
				break;
			}
		}
		/*
		 * Process the segment, our segment will be added to the
		 * end of the rcv push queue if no putnext()s are done.
		 */
		tcp_rput_data(q, mp, 0);
		if (tcp->tcp_rcv_tail != mp2) {
			/*
			 * The last mblk of the rcv push queue isn't the last
			 * mblk of the segment just processed, so its time to
			 * return the rcv push list.
			 */
			break;
		}
	}

	if ((mp = tcp->tcp_co_head) != NULL) {
		/*
		 * The co queue still has mblk(s).
		 */
		if (tcp->tcp_co_tintrvl == -1) {
			/*
			 * Restart a timer to drain the co queue,
			 * in case a rwnext() doesn't make its
			 * way down here again in a timely fashion.
			 */
			tcp->tcp_co_tintrvl = (clock_t)tcp_co_timer_interval;
			mi_timer(tcp->tcp_wq, tcp->tcp_co_tmp,
			    tcp->tcp_co_tintrvl);
		}
		if (tcp->tcp_co_cnt >= tcp->tcp_rack_cur_max ||
		    tcp->tcp_co_wakeq_force) {
			/*
			 * A rwnext() from the streamhead is
			 * still needed, so do a strwakeq().
			 */
			strwakeq(tcp->tcp_rq, QWANTR);
			tcp->tcp_co_wakeq_done = 1;
		} else {
			strwakeqclr(tcp->tcp_rq, QWANTR);
			tcp->tcp_co_wakeq_done = 0;
		}
	} else {
		strwakeqclr(tcp->tcp_rq, QWANTR);
		tcp->tcp_co_wakeq_done = 0;
		tcp->tcp_co_wakeq_force = 0;
	}

	if ((mp = tcp->tcp_rcv_head) != NULL) {
		/*
		 * The rcv push queue has mblk(s), return them.
		 */
#if CCS_STATS
		rrw_stats.rrw.count++;
		rrw_stats.rrw.bytes += tcp->tcp_rcv_cnt;
#endif
		tcp->tcp_rcv_head = nilp(mblk_t);
		tcp->tcp_rcv_cnt = 0;
	}
	dp->d_mp = mp;
	return (err);
}

/*
 * The read side put procedure.
 * The packets passed up by ip are assume to be aligned according to
 * OK_32PTR and the IP+TCP headers fitting in the first mblk.
 */
static void
tcp_rput(queue_t *q, mblk_t *mp)
{
	tcp_t	*tcp = (tcp_t *)q->q_ptr;

	TRACE_3(TR_FAC_TCP, TR_TCP_RPUT_IN,
	    "tcp_rput start:  q %p mp %p db_type 0%o",
	    q, mp, mp->b_datap->db_type);

	ASSERT(tcp != NULL);
	if (mp->b_datap->db_type == M_DATA) {
		tcp_rput_data(q, mp, 1);
		TRACE_1(TR_FAC_TCP, TR_TCP_RPUT_OUT, "tcp_rput end:  q %p",
		    q);
		return;
	}

	/*
	 * After tcp_detach is done and tcp_close calls qwait messages
	 * can arrive here but on the wrong queue. This
	 * means that tcp->tcp_rq no longer matches q. For control
	 * messages we drop the message. M_DATA are handled in tcp_rput_data.
	 */
	if (tcp->tcp_rq != q) {
		freemsg(mp);
		TRACE_1(TR_FAC_TCP, TR_TCP_RPUT_OUT, "tcp_rput end:  q %p",
		    q);
		return;
	}
#if CCS_STATS
	rrw_stats.oth.count++;
	rrw_stats.oth.bytes += tcp->tcp_co_cnt;
#endif
	if (tcp->tcp_co_head)
		tcp_co_drain(tcp);

	tcp_rput_other(q, mp);

	TRACE_1(TR_FAC_TCP, TR_TCP_RPUT_OUT, "tcp_rput end:  q %p", q);
}


/* The minimum of smoothed mean deviation in RTO calculation. */
#define	TCP_SD_MIN	400

/*
 * Set RTO for this connection.  The formula is from Jacobson and Karels'
 * "Congestion Avoidance and Control" in SIGCOMM '88.  The variable names
 * are the same as those in Appendix A.2 of that paper.
 *
 * m = new measurement
 * sa = smoothed RTT average (8 * average estimates).
 * sv = smoothed mean deviation (mdev) of RTT (4 * deviation estimates).
 */
static void
tcp_set_rto(tcp_t *tcp, clock_t rtt)
{
	long m = TICK_TO_MSEC(rtt);
	clock_t sa = tcp->tcp_rtt_sa;
	clock_t sv = tcp->tcp_rtt_sd;
	clock_t rto;

	BUMP_MIB(tcp_mib.tcpRttUpdate);
	tcp->tcp_rtt_update++;

	/* tcp_rtt_sa is not 0 means this is a new sample. */
	if (sa != 0) {
		/*
		 * Update average estimator:
		 *	new rtt = 7/8 old rtt + 1/8 Error
		 */

		/* m is now Error in estimate. */
		m -= sa >> 3;
		if ((sa += m) <= 0) {
			/*
			 * Don't allow the smoothed average to be negative.
			 * We use 0 to denote reinitialization of the
			 * variables.
			 */
			sa = 1;
		}

		/*
		 * Update deviation estimator:
		 *	new mdev = 3/4 old mdev + 1/4 (abs(Error) - old mdev)
		 */
		if (m < 0)
			m = -m;
		m -= sv >> 2;
		sv += m;
	} else {
		/*
		 * This follows BSD's implementation.  So the reinitialized
		 * RTO is 3 * m.  We cannot go less than 2 because if the
		 * link is bandwidth dominated, doubling the window size
		 * during slow start means doubling the RTT.  We want to be
		 * more conservative when we reinitialize our estimates.  3
		 * is just a convenient number.
		 */
		sa = m << 3;
		sv = m << 1;
	}
	if (sv < TCP_SD_MIN) {
		/*
		 * We do not know that if sa captures the delay ACK
		 * effect as in a long train of segments, a receiver
		 * does not delay its ACKs.  So set the minimum of sv
		 * to be TCP_SD_MIN, which is default to 400 ms, twice
		 * of BSD DATO.  That means the minimum of mean
		 * deviation is 100 ms.
		 *
		 */
		sv = TCP_SD_MIN;
	}
	tcp->tcp_rtt_sa = sa;
	tcp->tcp_rtt_sd = sv;
	/*
	 * RTO = average estimates (sa / 8) + 4 * deviation estimates (sv)
	 *
	 * Add tcp_rexmit_interval extra in case of extreme environment
	 * where the algorithm fails to work.  The default value of
	 * tcp_rexmit_interval_extra should be 0.
	 *
	 * As we use a finer grained clock than BSD and update
	 * RTO for every ACKs, add in another .25 of RTT to the
	 * deviation of RTO to accomodate burstiness of 1/4 of
	 * window size.
	 */
	rto = (sa >> 3) + sv + tcp_rexmit_interval_extra + (sa >> 5);

	if (rto > tcp_rexmit_interval_max) {
		tcp->tcp_rto = tcp_rexmit_interval_max;
	} else if (rto < tcp_rexmit_interval_min) {
		tcp->tcp_rto = tcp_rexmit_interval_min;
	} else {
		tcp->tcp_rto = rto;
	}

	/* Now, we can reset tcp_timer_backoff to use the new RTO... */
	tcp->tcp_timer_backoff = 0;
}

/*
 * tcp_get_seg_mp() is called to get the pointer to a segment in the
 * send queue which starts at the given seq. no.  This function is
 * useful for SACK when we put SACK in.
 *
 * Parameters:
 *	tcp_t *tcp: the tcp instance pointer.
 *	uint32_t seq: the starting seq. no of the requested segment.
 *	int32_t *off: after the execution, *off will be the offset to
 *		the returned mblk which points to the requested seq no.
 *
 * Return:
 *	A mblk_t pointer pointing to the requested segment in send queue.
 */
static mblk_t *
tcp_get_seg_mp(tcp_t *tcp, uint32_t seq, int32_t *off)
{
	int32_t	cnt;
	mblk_t	*mp;

	if (SEQ_LT(seq, tcp->tcp_suna) || SEQ_GEQ(seq, tcp->tcp_snxt) ||
	    ! off) {
		return (NULL);
	}
	cnt = seq - tcp->tcp_suna;
	mp = tcp->tcp_xmit_head;
	while (cnt > 0 && mp) {
		cnt -= mp->b_wptr - mp->b_rptr;
		if (cnt < 0) {
			cnt += mp->b_wptr - mp->b_rptr;
			break;
		}
		mp = mp->b_cont;
	}
	ASSERT(mp != NULL);
	*off = cnt;
	return (mp);
}


/*
 * This function handles all retransmissions if SACK is enabled for this
 * connection.  First it calculates how many segments can be retransmitted
 * based on tcp_pipe.  Then it goes thru the notsack list to find eligible
 * segments.  A segment is eligible if sack_cnt for that segment is greater
 * than or equal tcp_dupack_fast_retransmit.  After it has retransmitted
 * all eligible segments, it checks to see if TCP can send some new segments
 * (fast recovery).  If it can, it returns 1.  Otherwise it returns 0.
 *
 * Parameters:
 *	tcp_t *tcp: the tcp structure of the connection.
 *
 * Return:
 *	1 if the pipe is not full (new data can be sent), 0 otherwise
 */
static int32_t
tcp_sack_rxmit(tcp_t *tcp)
{
	notsack_blk_t	*notsack_blk;
	int32_t		usable_swnd;
	int32_t		mss;
	uint32_t	seg_len;
	mblk_t		*xmit_mp;

	ASSERT(tcp->tcp_sack_info != NULL);
	ASSERT(tcp->tcp_notsack_list != NULL);
	ASSERT(tcp->tcp_rexmit == 0);

	/* Defensive coding in case there is a bug... */
	if (tcp->tcp_notsack_list == NULL) {
		return (0);
	}
	notsack_blk = tcp->tcp_notsack_list;
	mss = tcp->tcp_mss;

	/*
	 * Limit the num of outstanding data in the network to be
	 * tcp_cwnd_ssthresh, which is half of the original congestion wnd.
	 */
	usable_swnd = tcp->tcp_cwnd_ssthresh - tcp->tcp_pipe;

	/* At least retransmit 1 MSS of data. */
	if (usable_swnd <= 0) {
		usable_swnd = mss;
	}

	/* Make sure no new RTT samples will be taken. */
	tcp->tcp_csuna = tcp->tcp_snxt;

	notsack_blk = tcp->tcp_notsack_list;
	while (usable_swnd > 0) {
		mblk_t		*snxt_mp, *tmp_mp;
		tcp_seq		begin = tcp->tcp_sack_snxt;
		tcp_seq		end;
		int32_t		off;

		for (; notsack_blk != NULL; notsack_blk = notsack_blk->next) {
			if (SEQ_GT(notsack_blk->end, begin) &&
			    (notsack_blk->sack_cnt >=
			    tcp_dupack_fast_retransmit)) {
				end = notsack_blk->end;
				if (SEQ_LT(begin, notsack_blk->begin)) {
					begin = notsack_blk->begin;
				}
				break;
			}
		}
		/*
		 * All holes are filled.  Manipulate tcp_cwnd to send more
		 * if we can.  Note that after the SACK recovery, tcp_cwnd is
		 * set to tcp_cwnd_ssthresh.
		 */
		if (notsack_blk == NULL) {
			usable_swnd = tcp->tcp_cwnd_ssthresh - tcp->tcp_pipe;
			if (usable_swnd <= 0) {
				tcp->tcp_cwnd = tcp->tcp_snxt - tcp->tcp_suna;
				return (0);
			} else {
				usable_swnd = usable_swnd / mss;
				tcp->tcp_cwnd = tcp->tcp_snxt - tcp->tcp_suna +
				    MAX(usable_swnd * mss, mss);
				return (1);
			}
		}

		/*
		 * Note that we may send more than usable_swnd allows here
		 * because of round off, but no more than 1 MSS of data.
		 */
		seg_len = end - begin;
		if (seg_len > mss)
			seg_len = mss;
		snxt_mp = tcp_get_seg_mp(tcp, begin, &off);
		ASSERT(snxt_mp != NULL);
		/* This should not happen.  Defensive coding again... */
		if (snxt_mp == NULL) {
			return (0);
		}

		xmit_mp = tcp_xmit_mp(tcp, snxt_mp, seg_len, &off,
		    &tmp_mp, begin, 1);
		if (xmit_mp == NULL) {
			return (0);
		}

		seg_len = msgdsize(xmit_mp->b_cont);
		usable_swnd -= seg_len;
		tcp->tcp_pipe += seg_len;
		tcp->tcp_sack_snxt = begin + seg_len;
		(void) putnext(tcp->tcp_wq, xmit_mp);

		/*
		 * Update the send timestamp to avoid false retransmission.
		 */
		snxt_mp->b_prev = (mblk_t *)lbolt;

		BUMP_MIB(tcp_mib.tcpRetransSegs);
		UPDATE_MIB(tcp_mib.tcpRetransBytes, seg_len);
		BUMP_MIB(tcp_mib.tcpOutSackRetransSegs);
		/*
		 * Update tcp_rexmit_max to extend this SACK recovery phase.
		 * This happens when new data sent during fast recovery is
		 * also lost.  If TCP retransmits those new data, it needs
		 * to extend SACK recover phase to avoid starting another
		 * fast retransmit/recovery unnecessarily.
		 */
		if (SEQ_GT(tcp->tcp_sack_snxt, tcp->tcp_rexmit_max)) {
			tcp->tcp_rexmit_max = tcp->tcp_sack_snxt;
		}
	}
	return (0);
}


/*
 * Handle M_DATA messages from IP.
 *
 * After tcp_detach is done and tcp_close calls qwait messages
 * can arrive here but on the wrong queue. This
 * means that tcp->tcp_rq no longer matches q.
 */
/* isput: -1 = recursive rput, 0 = co queue, 1 = rput */
static void
tcp_rput_data(queue_t *q, mblk_t *mp, int isput)
{
	int32_t		bytes_acked;
	int32_t		gap;
	mblk_t		*mp1;
	u_int		flags;
	uint32_t	new_swnd = 0;
	u_char		*orptr = mp->b_rptr;
	u_char		*rptr = mp->b_rptr;
	int32_t		rgap;
	uint32_t	seg_ack;
	int		seg_len;
	uint32_t	seg_seq;
	tcp_t		*tcp = (tcp_t *)q->q_ptr;
	tcph_t		*tcph;
	int		urp;
	int		rcv_cnt;
	tcp_opt_t	tcpopt;
	int		options;
	dblk_t		*dp = mp->b_datap;

	seg_len = IPH_HDR_LENGTH(rptr);
	tcph = (tcph_t *)&rptr[seg_len];
	if (!OK_32PTR(rptr)) {
		seg_seq = BE32_TO_U32(tcph->th_seq);
		seg_ack = BE32_TO_U32(tcph->th_ack);
	} else {
		seg_seq = ABE32_TO_U32(tcph->th_seq);
		seg_ack = ABE32_TO_U32(tcph->th_ack);
	}
	seg_len += TCP_HDR_LENGTH(tcph);
	seg_len = -seg_len;
	ASSERT((uintptr_t)(mp->b_wptr - rptr) <= (uintptr_t)INT_MAX);
	seg_len += (int)(mp->b_wptr - rptr);
	if ((mp1 = mp->b_cont) != NULL && mp1->b_datap->db_type == M_DATA) {
		do {
			ASSERT((uintptr_t)(mp1->b_wptr - mp1->b_rptr) <=
			    (uintptr_t)INT_MAX);
			seg_len += (int)(mp1->b_wptr - mp1->b_rptr);
		} while ((mp1 = mp1->b_cont) != NULL &&
		    mp1->b_datap->db_type == M_DATA);
	}
	if (isput > 0) {
		/*
		 * This is the correct place to update tcp_last_recv_time
		 * even though we may delay the processing of this segment
		 * for tcp_co_timer_interval.  Note that it is also
		 * updated for tcp structure that belongs to global and
		 * listener queues which do not really need updating.  But
		 * that should not cause any harm.  And it is updated for
		 * all kinds of incoming segments, not only for data segments.
		 */
		tcp->tcp_last_recv_time = lbolt;

		if (tcp->tcp_rq != q)
			goto not_hard_bound;

		/*
		 * Called from put, so check for co eligibility.
		 * Note that if the tcp is not hard bound, then it either
		 * belongs to the global queue or a listener queue. It can
		 * not be the right tcp for us. We'll have to do the IP cksum
		 * if delayed, and go on to not_hard_bound to find the right
		 * tcp.
		 */
		if (!tcp->tcp_hard_bound ||
		    !(dp->db_struioflag & STRUIO_SPEC) ||
		    seg_seq != (tcp->tcp_co_tail ? tcp->tcp_co_rnxt
			: tcp->tcp_rnxt) ||
		    seg_len < tcp_co_min ||
		    tcp->tcp_reass_head ||
		    (tcph->th_flags[0] & TH_URG) ||
		    ! isuioq(q)) {
			/*
			 * Segment not co eligible, drain co queue,
			 * if need be, then process this segment.
			 */
#if CCS_STATS
			rrw_stats.mis.count++;
			rrw_stats.mis.bytes += seg_len;
			if (! (dp->db_struioflag & STRUIO_SPEC)) {
				rrw_stats.spc.count++;
				rrw_stats.spc.bytes += seg_len;
				rrw_stats.spc.bytes += tcp->tcp_co_cnt;
			} else if (seg_seq != (tcp->tcp_co_tail ?
			    tcp->tcp_co_rnxt : tcp->tcp_rnxt)) {
				rrw_stats.seq.count++;
				rrw_stats.seq.bytes += seg_len;
				rrw_stats.seq.bytes += tcp->tcp_co_cnt;
			} else if (seg_len < tcp_co_min) {
				rrw_stats.len.count++;
				rrw_stats.len.bytes += seg_len;
				rrw_stats.len.bytes += tcp->tcp_co_cnt;
			} else {
				rrw_stats.uio.count++;
				rrw_stats.uio.bytes += seg_len;
				rrw_stats.uio.bytes += tcp->tcp_co_cnt;
			}
#endif
			if (tcp->tcp_co_head)
				tcp_co_drain(tcp);

			if (dp->db_struioflag & STRUIO_IP) {
				/*
				 * Delayed IP checksum required.
				 */
				int off;
				ASSERT((uintptr_t)(dp->db_struiobase - rptr) <=
				    (uintptr_t)INT_MAX);
				off = (int)(dp->db_struiobase - rptr);

				if (IP_CSUM(mp, off, 0)) {
					/*
					 * Checksum error, so drop it.
					 */
					BUMP_MIB(ip_mib.tcpInErrs);
					ipcsumdbg(
					    "tcp_rput_data: cksumerr\n", mp);
					freemsg(mp);
					return;
				}
			}
			/*
			 * A normal mblk now, so clear the struioflag.
			 */
			mp1 = mp;
			do
				mp1->b_datap->db_struioflag &=
				    ~(STRUIO_IP|STRUIO_SPEC);
			while ((mp1 = mp1->b_cont) != NULL);
		} else {
			/*
			 * Segment is co eligible, enqueue it on the co queue.
			 */
#if CCS_STATS
			rrw_stats.hit.count++;
			rrw_stats.hit.bytes += seg_len;
#endif
			if (dp->db_struioflag & STRUIO_IP) {
				/*
				 * IP postponed checksum, IP header has been
				 * checksummed so do the TCP header and init
				 * the mblk (chain) for struioput().
				 */
				int off;
				ASSERT((uintptr_t)(dp->db_struiobase - rptr) <=
				    (uintptr_t)INT_MAX);
				off = (int)(dp->db_struiobase - rptr);

				dp->db_struiobase += TCP_HDR_LENGTH(tcph);
				mp1 = mp;
				do
					mp1->b_datap->db_struioptr =
					    mp1->b_datap->db_struiolim;
				while ((mp1 = mp1->b_cont) != NULL);
				*(uint16_t *)dp->db_struioun.data =
				    IP_CSUM_PARTIAL(mp, off, 0);
				mp1 = mp;
				do
					mp1->b_datap->db_struioptr =
					    mp1->b_datap->db_struiobase;
				while ((mp1 = mp1->b_cont) != NULL);
			} else {
				/*
				 * IP has inited the mblk (chain) for use by
				 * struioput(), so adjust the first (only)
				 * mblk to account for the TCP header.
				 */
				dp->db_struiobase += TCP_HDR_LENGTH(tcph);
				dp->db_struioptr = dp->db_struiobase;
			}
			if (tcp->tcp_co_tail) {
				/*
				 * Another seg, enqueue on tail, and
				 * update rnxt.
				 */
				tcp->tcp_co_tail->b_next = mp;
				tcp->tcp_co_rnxt += seg_len;
			} else {
				/*
				 * First seg, enqueue, and init rnxt.
				 */
				tcp->tcp_co_head = mp;
				tcp->tcp_co_rnxt = tcp->tcp_rnxt + seg_len;
				if (tcp->tcp_co_norm &&
				    (mp1 = tcp->tcp_co_imp) != NULL) {
					/*
					 * May be putnext()ed mblk(s) in flight
					 * above us, so send up an M_IOCTL and
					 * the streamhead will send an M_IOCNAK
					 * back done. Until then no rwnext().
					 */
					tcp->tcp_co_imp = nilp(mblk_t);
					tcp->tcp_co_norm = 0;
					(void) struio_ioctl(tcp->tcp_rq, mp1);
				}
			}
			tcp->tcp_co_tail = mp;
			mp->b_next = 0;
			tcp->tcp_co_cnt += seg_len;
			if (tcp->tcp_co_tintrvl == -1) {
				/*
				 * Start a timer to drain the co queue,
				 * in case a rwnext() doesn't make its
				 * way down here in a timely fashion.
				 */
				tcp->tcp_co_tintrvl =
				    (clock_t)tcp_co_timer_interval;
				mi_timer(tcp->tcp_wq, tcp->tcp_co_tmp,
				    tcp->tcp_co_tintrvl);
			}
			if ((! tcp->tcp_co_wakeq_done &&
			    tcp->tcp_co_cnt >= tcp->tcp_rack_cur_max) ||
			    (tcph->th_flags[0] & (TH_PSH|TH_FIN)) ||
			    seg_len < tcp->tcp_mss) {
				/*
				 * A rwnext() from the streamhead is needed.
				 */
				if (tcph->th_flags[0] & (TH_PSH|TH_FIN))
					tcp->tcp_co_wakeq_force = 1;
				if (tcp->tcp_co_imp == NULL) {
					/*
					 * Waiting for the I_SYNCSTR from above
					 * to return, so defer the strwakeq().
					 */
					tcp->tcp_co_wakeq_need = 1;
				} else {
					tcp->tcp_co_wakeq_done = 1;
					strwakeq(tcp->tcp_rq, QWANTR);
				}
			}
			return;
		}
	} else if (isput == 0)
		/*
		 * Called to process a dequeued co queue
		 * segment, adjust co queue byte count.
		 */
		tcp->tcp_co_cnt -= seg_len;

	/*
	 * If hard bound, the message is known to be for the instance on
	 * which it was delivered.  Otherwise, we have to find out which
	 * queue it really belongs on, and deliver it there.  IP only knows
	 * how to fan out to the fully established connections.  Anything
	 * else, it hands to the first non-hard-bound stream.
	 */
	if (!tcp->tcp_hard_bound) {
		tcp_t	*tcp1;
		queue_t	*rq;	/* tcp1->tcp_rq acquired while tf_lock was */
				/* held. */
not_hard_bound:
		/*
		 * Try for an exact match.  If we find one, and it is the
		 * stream we are already on, then get to work.
		 * Note: since a tcp_accept_swap might occur after
		 * tcp_lookup returns it is important that the code
		 * below only checks TCP_IS_DETACHED, tcp_listener, and
		 * tcp_rq. tcp_accept_swap might change those fields while
		 * we are running here but since tcp_rq is acquired
		 * atomically with respect to tcp_accept_swap this
		 * will always result in a lateral_put and the next
		 * tcp_rput_data will then block in tcp_lookup until
		 * tcp_accept_swap is done.
		 */
		tcp1 = tcp_lookup((iph_t *)rptr, tcph, TCPS_LISTEN, &rq);
		if (tcp1 != NULL) {
			if (rq != q) {
				/*
				 * Arrived on the wrong queue/perimeter.
				 */
				lateral_put(q, rq, mp);
				TCP_REFRELE(tcp1);
				TRACE_1(TR_FAC_TCP, TR_TCP_RPUT_OUT,
				"tcp_rput end:  q %p", q);
				return;
			}
			TCP_REFRELE(tcp1);
			/*
			 * Already on the correct queue/perimeter.
			 * If this is a detached connection and not an eager
			 * connection hanging off a listener then new data
			 * (past the FIN) will cause a reset.
			 * We do a special check here where it
			 * is out of the main line, rather than check
			 * if we are detached every time we see new
			 * data down below.
			 */
			if (tcp1->tcp_detached && tcp1->tcp_listener == NULL &&
			    seg_len > 0 &&
			    SEQ_GT(seg_seq + seg_len, tcp1->tcp_rnxt)) {
				BUMP_MIB(tcp_mib.tcpInClosed);
				tcp_xmit_ctl("new data when detached",
				    tcp1, mp, tcp1->tcp_snxt, 0, TH_RST);
				(void) tcp_clean_death(tcp1, EPROTO);
				TRACE_1(TR_FAC_TCP, TR_TCP_RPUT_OUT,
				"tcp_rput end:  q %p", q);
				return;
			}
			tcp = tcp1;
			goto do_detached;
		}
		/* Try to find a listener */
		tcp1 = tcp_lookup_listener(tcph->th_fport,
		    ((iph_t *)rptr)->iph_dst);
		if (!tcp1) {
			/* No takers.  Generate a proper Reset. */
			tcp_xmit_listeners_reset(q, mp);
			TRACE_1(TR_FAC_TCP, TR_TCP_RPUT_OUT,
				"tcp_rput end:  q %p", q);
			return;
		}
		/*
		 * Found someone interested.  If it isn't the current
		 * queue, put it on the right one.
		 */
		if (tcp1->tcp_rq != q) {
			lateral_put(q, tcp1->tcp_rq, mp);
			TCP_REFRELE(tcp1);
			TRACE_1(TR_FAC_TCP, TR_TCP_RPUT_OUT,
				"tcp_rput end:  q %p", q);
			return;
		}
		tcp = tcp1;
		TCP_REFRELE(tcp1);
	}
do_detached:;
	ASSERT(tcp->tcp_rq == q);
	BUMP_LOCAL(tcp->tcp_ibsegs);
	flags = (unsigned int)tcph->th_flags[0] & 0xFF;
	if ((flags & TH_URG) && isput != -1) {
		/*
		 * TCP can't handle urgent pointers that arrive before
		 * the connection has been accept()ed since it can't
		 * buffer OOB data.  Discard segment if this happens.
		 *
		 * Nor can it reassemble urgent pointers, so discard
		 * if it's not the next segment expected.
		 *
		 * Otherwise, collapse chain into one mblk (discard if
		 * that fails).  This makes sure the headers, retransmitted
		 * data, and new data all are in the same mblk.
		 */
		ASSERT(mp != NULL);
		if (tcp->tcp_listener || !pullupmsg(mp, -1)) {
			freemsg(mp);
			TRACE_1(TR_FAC_TCP, TR_TCP_RPUT_OUT,
				"tcp_rput end:  q %p", q);
			return;
		}
		/* Update pointers into message */
		orptr = rptr = mp->b_rptr;
		tcph = (tcph_t *)&rptr[IPH_HDR_LENGTH(rptr)];
		if (SEQ_GT(seg_seq, tcp->tcp_rnxt)) {
			/*
			 * Since we can't handle any data with this urgent
			 * pointer that is out of sequence, we expunge
			 * the data.  This allows us to still register
			 * the urgent mark and generate the M_PCSIG,
			 * which we can do.
			 */
			mp->b_wptr = (u_char *)tcph + TCP_HDR_LENGTH(tcph);
			seg_len = 0;
		}
	}

	switch (tcp->tcp_state) {
	case TCPS_LISTEN:
		if ((flags & (TH_RST | TH_ACK | TH_SYN)) != TH_SYN) {
			if (flags & TH_RST) {
				freemsg(mp);
				TRACE_1(TR_FAC_TCP, TR_TCP_RPUT_OUT,
					"tcp_rput end:  q %p", q);
				return;
			}
			if (flags & TH_ACK) {
				tcp_xmit_early_reset("TCPS_LISTEN-TH_ACK",
				    q, mp, seg_ack, 0, TH_RST);
				TRACE_1(TR_FAC_TCP, TR_TCP_RPUT_OUT,
				    "tcp_rput end:  q %p", q);
				return;
			}
			if (!(flags & TH_SYN)) {
				freemsg(mp);
				TRACE_1(TR_FAC_TCP, TR_TCP_RPUT_OUT,
					"tcp_rput end:  q %p", q);
				return;
			}
		}
		if (tcp->tcp_conn_req_max > 0) {
			tcp_conn_request(tcp, mp);
			TRACE_1(TR_FAC_TCP, TR_TCP_RPUT_OUT,
			    "tcp_rput end:  q %p", q);
			return;
		}
		tcp->tcp_irs = seg_seq;
		tcp->tcp_rack = seg_seq;
		tcp->tcp_rnxt = seg_seq + 1;
		U32_TO_ABE32(tcp->tcp_rnxt, tcp->tcp_tcph->th_ack);
		BUMP_MIB(tcp_mib.tcpPassiveOpens);
		goto syn_rcvd;
	case TCPS_SYN_SENT:
		if (flags & TH_ACK) {
			if (SEQ_LEQ(seg_ack, tcp->tcp_iss) ||
			    SEQ_GT(seg_ack, tcp->tcp_snxt)) {
				if (flags & TH_RST) {
					freemsg(mp);
					TRACE_1(TR_FAC_TCP, TR_TCP_RPUT_OUT,
					    "tcp_rput end:  q %p", q);
					return;
				}
				tcp_xmit_ctl("TCPS_SYN_SENT-Bad_seq",
				    tcp, mp, seg_ack, 0, TH_RST);
				TRACE_1(TR_FAC_TCP, TR_TCP_RPUT_OUT,
					"tcp_rput end:  q %p", q);
				return;
			}
			if (SEQ_LEQ(tcp->tcp_suna, seg_ack))
				flags |= TH_ACK_ACCEPTABLE;
		}
		if (flags & TH_RST) {
			freemsg(mp);
			if (flags & TH_ACK_ACCEPTABLE)
				(void) tcp_clean_death(tcp, ECONNREFUSED);
			TRACE_1(TR_FAC_TCP, TR_TCP_RPUT_OUT,
				"tcp_rput end:  q %p", q);
			return;
		}
		if (!(flags & TH_SYN)) {
			freemsg(mp);
			TRACE_1(TR_FAC_TCP, TR_TCP_RPUT_OUT,
				"tcp_rput end:  q %p", q);
			return;
		}

		tcpopt.tcp = NULL;
		options = tcp_parse_options(tcph, &tcpopt);
		if (! (options & TCP_OPT_MSS_PRESENT))
			tcpopt.tcp_opt_mss = tcp_mss_def;

		if (tcp->tcp_snd_ts_ok && (options & TCP_OPT_TSTAMP_PRESENT)) {
			char *ptr = (char *)tcp->tcp_tcph;

			tcp->tcp_ts_recent = tcpopt.tcp_opt_ts_val;
			tcp->tcp_last_rcv_lbolt = lbolt;
			ASSERT(OK_32PTR(ptr));
			ASSERT(tcp->tcp_tcp_hdr_len == TCP_MIN_HEADER_LENGTH);

			ptr += tcp->tcp_tcp_hdr_len;
			ptr[0] = TCPOPT_NOP;
			ptr[1] = TCPOPT_NOP;
			ptr[2] = TCPOPT_TSTAMP;
			ptr[3] = TCPOPT_TSTAMP_LEN;
			tcp->tcp_hdr_len += TCPOPT_REAL_TS_LEN;
			tcp->tcp_tcp_hdr_len += TCPOPT_REAL_TS_LEN;
			tcp->tcp_tcph->th_offset_and_rsrvd[0] += (3 << 4);

			/*
			 * Note that the mss option does not include the length
			 * of timestamp option, or any other options.
			 */
			tcpopt.tcp_opt_mss -= TCPOPT_REAL_TS_LEN;
		} else {
			/*
			 * The other side does not take timestamp option.
			 * Have we decremented tcp_mss?  If yes,
			 * increment it.
			 */
			if (tcp->tcp_snd_ts_ok) {
				tcp->tcp_mss += TCPOPT_REAL_TS_LEN;
			}
			tcp->tcp_snd_ts_ok = 0;
		}

		if (tcp->tcp_snd_sack_ok) {
			if (options & TCP_OPT_SACK_OK_PRESENT) {
				if (tcp->tcp_snd_ts_ok) {
					tcp->tcp_max_sack_blk = 3;
				} else {
					tcp->tcp_max_sack_blk = 4;
				}
			} else {
				/*
				 * Resetting tcp_snd_sack_ok to 0 so that no
				 * sack info will be used for this connection.
				 * This assumes that sack usage permission is
				 * negotiated.  This may need to be changed
				 * once this is clarified.
				 */
				tcp->tcp_snd_sack_ok = 0;
				if (tcp->tcp_sack_info != NULL) {
					kmem_free(tcp->tcp_sack_info,
					    sizeof (tcp_sack_info_t));
					tcp->tcp_sack_info = NULL;
				}
			}
		}

		if (options & TCP_OPT_WSCALE_PRESENT) {
			tcp->tcp_snd_ws = tcpopt.tcp_opt_wscale;
			tcp->tcp_snd_ws_ok = 1;
		} else {
			tcp->tcp_snd_ws = 0;
			tcp->tcp_snd_ws_ok = 0;
			tcp->tcp_rcv_ws = 0;
		}

		/*
		 * If we got a new MSS or we can use timestamp option, set
		 * a new MSS.  We didn't call tcp_mss_set() when we decremented
		 * tcp_mss for timestamp option.  So we need to call it here
		 * to notify STREAM head.  If we have a larger-than-16-bit
		 * window but the other side didn't want to do window scale,
		 * call tcp_rwnd_set to clamp the window to 16 bits.  (Note
		 * that tcp_mss_set calls tcp_rwnd_set, so we don't need to
		 * do both.)
		 */

		if (tcpopt.tcp_opt_mss < tcp->tcp_mss) {
			tcp_mss_set(tcp, tcpopt.tcp_opt_mss);
		} else if (tcp->tcp_snd_ts_ok != 0) {
			tcp_mss_set(tcp, tcp->tcp_mss);
		} else if (tcp->tcp_rwnd_max > TCP_MAXWIN &&
		    tcp->tcp_rcv_ws == 0) {
			(void) tcp_rwnd_set(tcp, tcp->tcp_rwnd_max);
		}

		if (tcp->tcp_ill_ick.ick_magic == ICK_M_CTL_MAGIC &&
		    strzc_on) {
			ushort	copyopt = 0;

			if ((zerocopy_prop & 1) != 0 &&
			    tcp->tcp_mss >= strzc_minblk)
				copyopt = MAPINOK;
			if ((zerocopy_prop & 2) != 0 &&
			    tcp->tcp_mss >= ptob(1))
				copyopt |= REMAPOK;
			if (copyopt)
				(void) mi_set_sth_copyopt(tcp->tcp_rq, copyopt);
		}
		tcp->tcp_irs = seg_seq;
		tcp->tcp_rack = seg_seq;
		tcp->tcp_rnxt = seg_seq + 1;
		U32_TO_ABE32(tcp->tcp_rnxt, tcp->tcp_tcph->th_ack);
		if (!TCP_IS_DETACHED(tcp)) {
			/* Allocate room for SACK options if needed. */
			if (tcp->tcp_snd_sack_ok) {
				(void) mi_set_sth_wroff(tcp->tcp_rq,
				    tcp->tcp_hdr_len + TCPOPT_MAX_SACK_LEN +
				    (tcp->tcp_loopback ? 0 : tcp_wroff_xtra));
			} else {
				(void) mi_set_sth_wroff(tcp->tcp_rq,
				    tcp->tcp_hdr_len +
				    (tcp->tcp_loopback ? 0 : tcp_wroff_xtra));
			}
		}
		if (flags & TH_ACK_ACCEPTABLE) {
			/*
			 * If we can't get the confirmation upstream, pretend
			 * we didn't even see this one.
			 *
			 * XXX: how can we pretend we didn't see it if we
			 * have updated rnxt et. al.
			 */
			if (!tcp_conn_con(tcp, (iph_t *)mp->b_rptr, tcph)) {
				freemsg(mp);
				TRACE_1(TR_FAC_TCP, TR_TCP_RPUT_OUT,
					"tcp_rput end:  q %p", q);
				return;
			}
			/* One for the SYN */
			tcp->tcp_suna = tcp->tcp_iss + 1;
			tcp->tcp_valid_bits &= ~TCP_ISS_VALID;
			tcp->tcp_state = TCPS_ESTABLISHED;

			/*
			 * If SYN was retransmitted, need to reset all
			 * retransmission info.  This is because this
			 * segment will be treated as a dup ACK.
			 */
			if (tcp->tcp_rexmit) {
				tcp->tcp_rexmit = 0;
				tcp->tcp_rexmit_nxt = tcp->tcp_snxt;
				tcp->tcp_rexmit_max = tcp->tcp_snxt;
				tcp->tcp_snd_burst = TCP_CWND_INFINITE;
			}

			tcp->tcp_swl1 = seg_seq;
			tcp->tcp_swl2 = seg_ack;

			new_swnd = BE16_TO_U16(tcph->th_win);
			tcp->tcp_swnd = new_swnd;
			if (new_swnd > tcp->tcp_max_swnd)
				tcp->tcp_max_swnd = new_swnd;

			/*
			 * Always send the three-way handshake ack immediately
			 * in order to make the connection complete as soon as
			 * possible on the accepting host.
			 */
			flags |= TH_ACK_NEEDED;
			flags &= ~(TH_SYN | TH_ACK_ACCEPTABLE);
			seg_seq++;
			break;
		}
		syn_rcvd:
		tcp->tcp_state = TCPS_SYN_RCVD;
		mp1 = tcp_xmit_mp(tcp, tcp->tcp_xmit_head, tcp->tcp_mss,
		    NULL, NULL, tcp->tcp_iss, 0);
		if (mp1) {
			putnext(tcp->tcp_wq, mp1);
			TCP_TIMER_RESTART(tcp, tcp->tcp_rto);
		}
#if 0
		/* TODO: check out the entire 'quick connect' sequence */
		if (seg_len > 0 || (flags & TH_FIN)) {
			/* TODO: how do we get this off again? */
			noenable(tcp->tcp_rq);
			(void) putq(tcp->tcp_rq, mp);
		} else
#endif
			freemsg(mp);
		TRACE_1(TR_FAC_TCP, TR_TCP_RPUT_OUT, "tcp_rput end:  q %p",
		    q);
		return;
	case TCPS_TIME_WAIT:
		if ((flags & TH_FIN) && (seg_seq == tcp->tcp_rnxt - 1)) {
			/*
			 * When TCP receives a duplicate FIN in TIME_WAIT
			 * state, restart the 2 MSL timer.  See page 73
			 * in RFC 793.  Make sure this TCP is already on
			 * the TIME-WAIT list.  If not, just restart the
			 * timer.
			 */
			if (TCP_IS_DETACHED(tcp)) {
				tcp_time_wait_remove(tcp);
				tcp_time_wait_append(tcp);
			} else {
				TCP_TIMER_RESTART(tcp,
				    tcp_close_wait_interval);
			}
			flags |= TH_ACK_NEEDED;
			if (mp != NULL)
				freemsg(mp);
			BUMP_MIB(tcp_mib.tcpInDataDupSegs);
			goto ack_check;
		}
		if (!(flags & TH_SYN))
			break;
		gap = seg_seq - tcp->tcp_rnxt;
		rgap = tcp->tcp_rwnd - (gap + seg_len);
		if (gap > 0 && rgap < 0) {
			/*
			 * Make sure that when we accept the connection pick
			 * a number greater then the rnxt for the old
			 * connection.
			 *
			 * First, calculate a minimal iss value.
			 *
			 */
			time_t adj = (tcp->tcp_rnxt + ISS_INCR);

			if (tcp_strong_iss == 1) {
				/* Subtract out min random next iss */
				adj -= gethrtime()/ISS_NSEC_DIV;
				adj -= tcp_iss_incr_extra + 1;
			} else if (tcp_strong_iss == 2) {
				uint32_t answer[4];
				struct {
					uint32_t ports;
					ipaddr_t src;
					ipaddr_t dst;
				} arg;
				MD5_CTX context;

				mutex_enter(&tcp_iss_key_lock);
				context = tcp_iss_key;
				mutex_exit(&tcp_iss_key_lock);
				arg.ports = tcp->tcp_ports;
				arg.src = tcp->tcp_ipha.ipha_src;
				arg.dst = tcp->tcp_ipha.ipha_dst;
				MD5Update(&context, (u_char *)&arg,
				    sizeof (arg));
				MD5Final((u_char *)answer, &context);
				answer[0] ^= answer[1] ^ answer[2] ^ answer[3];
				adj -= ISS_INCR/2;
				adj -= gethrtime()/ISS_NSEC_DIV + answer[0] +
				    tcp_iss_incr_extra;
			} else {
				/* Subtract out next iss */
				adj -= ISS_INCR/2;
				adj -= (int32_t)hrestime.tv_sec * ISS_INCR +
				    tcp_iss_incr_extra;
			}
			if (adj > 0) {
				/*
				 * New iss not guaranteed to be ISS_INCR
				 * ahead of the current rnxt, so add the
				 * difference to incr_extra just in case.
				 */
				tcp_iss_incr_extra += adj;
			}
			/*
			 * If tcp_clean_death() can not perform the task now,
			 * drop the SYN packet and let the other side re-xmit.
			 * Otherwise pass the SYN packet back in, since the
			 * old tcp state has been cleaned up or freed.
			 */
			if (tcp_clean_death(tcp, 0) == -1)
				freemsg(mp);
			else
				lateral_put(q, q, mp);
			TRACE_1(TR_FAC_TCP, TR_TCP_RPUT_OUT,
				"tcp_rput end:  q %p", q);
			return;
		}
		break;
	default:
		break;
	}
	mp->b_rptr = (u_char *)tcph + TCP_HDR_LENGTH(tcph);
	urp = BE16_TO_U16(tcph->th_urp) - TCP_OLD_URP_INTERPRETATION;
	new_swnd = BE16_TO_U16(tcph->th_win) <<
	    ((tcph->th_flags[0] & TH_SYN) ? 0 : tcp->tcp_snd_ws);

	if (tcp->tcp_snd_ts_ok) {
		/*
		 * If timestamp option is aligned nicely, get values inline,
		 * otherwise call general routine to parse.  Only do that
		 * if timestamp is the only option.
		 */

		uint8_t *up;

		if (TCP_HDR_LENGTH(tcph) == (uint32_t)TCP_MIN_HEADER_LENGTH +
		    TCPOPT_REAL_TS_LEN &&
		    OK_32PTR((up = ((uint8_t *)tcph) +
		    TCP_MIN_HEADER_LENGTH)) &&
		    *(uint32_t *)up == TCPOPT_NOP_NOP_TSTAMP) {
			tcpopt.tcp_opt_ts_val = ABE32_TO_U32((up+4));
			tcpopt.tcp_opt_ts_ecr = ABE32_TO_U32((up+8));

			options = TCP_OPT_TSTAMP_PRESENT;
		} else {
			if (tcp->tcp_snd_sack_ok) {
				tcpopt.tcp = tcp;
			} else {
				tcpopt.tcp = NULL;
			}
			options = tcp_parse_options(tcph, &tcpopt);
		}

		if ((options & TCP_OPT_TSTAMP_PRESENT) == 0) {
			/*
			 * If we don't get a timestamp on every packet, we
			 * figure we can't really trust 'em, so we stop sending
			 * and parsing them.
			 */

			tcp->tcp_snd_ts_ok = 0;
			tcp->tcp_hdr_len -= TCPOPT_REAL_TS_LEN;
			tcp->tcp_tcp_hdr_len -= TCPOPT_REAL_TS_LEN;
			tcp->tcp_tcph->th_offset_and_rsrvd[0] -= (3 << 4);
			tcp_mss_set(tcp, tcp->tcp_mss + TCPOPT_REAL_TS_LEN);
			if (tcp->tcp_snd_sack_ok) {
				ASSERT(tcp->tcp_sack_info != NULL);
				tcp->tcp_max_sack_blk = 4;
			}
		} else {
			/*
			 *  Do PAWS per RFC 1323 section 4.2.
			 */
			if (TSTMP_LT(tcpopt.tcp_opt_ts_val,
			    tcp->tcp_ts_recent) &&
			    SEQ_LT(lbolt,
				tcp->tcp_last_rcv_lbolt + PAWS_TIMEOUT)) {
				goto unacceptable;
			}
		}
	} else if (tcp->tcp_snd_sack_ok) {
		ASSERT(tcp->tcp_sack_info != NULL);
		tcpopt.tcp = tcp;
		options = tcp_parse_options(tcph, &tcpopt);
	}

try_again:;
	gap = seg_seq - tcp->tcp_rnxt;
	rgap = tcp->tcp_rwnd - (gap + seg_len);
	/*
	 * gap is the amount of sequence space between what we expect to see
	 * and what we got for seg_seq.  A positive value for gap means
	 * something got lost.  A negative value means we got some old stuff.
	 */
	if (gap < 0) {
		/* Old stuff present.  Is the SYN in there? */
		if (seg_seq == tcp->tcp_irs && (flags & TH_SYN) &&
		    (seg_len != 0)) {
			flags &= ~TH_SYN;
			seg_seq++;
			urp--;
			/* Recompute the gaps after noting the SYN. */
			goto try_again;
		}
		BUMP_MIB(tcp_mib.tcpInDataDupSegs);
		UPDATE_MIB(tcp_mib.tcpInDataDupBytes,
		    (seg_len > -gap ? -gap : seg_len));
		/* Remove the old stuff from seg_len. */
		seg_len += gap;
		/*
		 * Anything left?
		 * Make sure to check for unack'd FIN when rest of data
		 * has been previously ack'd.
		 */
		if (seg_len < 0 || (seg_len == 0 && !(flags & TH_FIN))) {
			/*
			 * The arriving of dup data packets indicate that we
			 * may have postponed an ack for too long, or the other
			 * side's RTT estimate is out of shape. Start acking
			 * more often.
			 */
			if (SEQ_GEQ(seg_seq + seg_len - gap, tcp->tcp_rack) &&
			    tcp->tcp_rack_cnt >= tcp->tcp_mss &&
			    tcp->tcp_rack_abs_max > (tcp->tcp_mss << 1))
				tcp->tcp_rack_abs_max -= tcp->tcp_mss;
unacceptable:;
			/*
			 * This segment is "unacceptable".  None of its
			 * sequence space lies within our advertized window.
			 *
			 * Adjust seg_len to be the original value.
			 */
			if (gap < 0)
				seg_len -= gap;
			(void) mi_strlog(tcp->tcp_rq, 1, SL_TRACE,
			    "tcp_rput: unacceptable, gap %d, rgap %d, "
			    "flags 0x%x, seg_seq %u, seg_ack %u, seg_len %d, "
			    "rnxt %u, snxt %u, %s",
			    gap, rgap, flags, seg_seq, seg_ack, seg_len,
			    tcp->tcp_rnxt, tcp->tcp_snxt,
			    tcp_display(tcp));

			tcp->tcp_rack_cur_max = tcp->tcp_mss;

			/*
			 * Resets are only valid if they lie within our offered
			 * window.  If the RST bit is set, we just ignore this
			 * segment.
			 */
			if (flags & TH_RST) {
				TRACE_1(TR_FAC_TCP, TR_TCP_RPUT_OUT,
					"tcp_rput end:  q %p", q);
				freemsg(mp);
				return;
			}

			/*
			 * Arrange to send an ACK in response to the
			 * unacceptable segment per RFC 793 page 69. There
			 * is only one small difference between ours and the
			 * acceptability test in the RFC - we accept ACK-only
			 * packet with SEG.SEQ = RCV.NXT+RCV.WND and no ACK
			 * will be generated.
			 *
			 * Note that we have to ACK an ACK-only packet at least
			 * for stacks that send 0-length keep-alives with
			 * SEG.SEQ = SND.NXT-1 as recommended by RFC1122,
			 * section 4.2.3.6. As long as we don't ever generate
			 * an unacceptable packet in response to an incoming
			 * packet that is unacceptable, it should not cause
			 * "ACK wars".
			 */

			flags |=  TH_ACK_NEEDED;

			/*
			 * Send SIGURG as soon as possible i.e. even
			 * if the TH_URG was delivered in a window probe
			 * packet (which will be unacceptable).
			 *
			 * We generate a signal if none has been generated
			 * for this connection or if this is a new urgent
			 * byte. Also send a zero-length "unmarked" message
			 * to inform SIOCATMARK that this is not the mark.
			 *
			 * tcp_urp_last_valid is cleared when the T_exdata_ind
			 * is sent up. This plus the check for old data
			 * (gap >= 0) handles the wraparound of the sequence
			 * number space without having to always track the
			 * correct MAX(tcp_urp_last, tcp_rnxt). (BSD tracks
			 * this max in its rcv_up variable).
			 *
			 * This prevents duplicate SIGURGS due to a "late"
			 * zero-window probe when the T_EXDATA_IND has already
			 * been sent up.
			 */
			if ((flags & TH_URG) && gap >= 0 &&
			    (!tcp->tcp_urp_last_valid || SEQ_GT(urp + seg_seq,
			    tcp->tcp_urp_last))) {
				mp1 = allocb(0, BPRI_MED);
				if (mp1 == NULL ||
				    !putnextctl1(q, M_PCSIG, SIGURG)) {
					/* Try again on the rexmit. */
					freemsg(mp1);
					freemsg(mp);
					TRACE_1(TR_FAC_TCP, TR_TCP_RPUT_OUT,
					    "tcp_rput end:  q %p", q);
					return;
				}
				/*
				 * If the next byte would be the mark
				 * then mark with MARKNEXT else mark
				 * with NOTMARKNEXT.
				 */
				if (gap == 0 && urp == 0)
					mp1->b_flag |= MSGMARKNEXT;
				else
					mp1->b_flag |= MSGNOTMARKNEXT;
				freemsg(tcp->tcp_urp_mark_mp);
				tcp->tcp_urp_mark_mp = mp1;
				flags |= TH_SEND_URP_MARK;
#ifdef DEBUG
				(void) mi_strlog(tcp->tcp_rq, 1, SL_TRACE,
				    "tcp_rput: sent M_PCSIG seq %x urp "
				    "%x last %x queued %d, %s\n",
				    seg_seq, urp, tcp->tcp_urp_last,
				    tcp->tcp_rcv_cnt, tcp_display(tcp));
#endif /* DEBUG */
				tcp->tcp_urp_last_valid = true;
				tcp->tcp_urp_last = urp + seg_seq;
			}
			/*
			 * Continue processing this segment in order to use the
			 * ACK information it contains, but skip all other
			 * sequence-number processing.	Processing the ACK
			 * information is necessary is necessary in order to
			 * re-synchronize connections that may have lost
			 * synchronization.
			 * We clear seg_len and flag fields related to
			 * sequence number processing as they are not
			 * to be trusted for an unacceptable segment.
			 */
			seg_len = 0;
			flags &= ~(TH_SYN | TH_FIN | TH_URG);
			goto process_ack;
		}

		/* Fix seg_seq, and chew the gap off the front. */
		seg_seq = tcp->tcp_rnxt;
		urp += gap;
		do {
			mblk_t	*mp2;
			ASSERT((uintptr_t)(mp->b_wptr - mp->b_rptr) <=
			    (uintptr_t)UINT_MAX);
			gap += (u_int)(mp->b_wptr - mp->b_rptr);
			if (gap > 0) {
				mp->b_rptr = mp->b_wptr - gap;
				break;
			}
			mp2 = mp;
			mp = mp->b_cont;
			freeb(mp2);
		} while (gap < 0);
		/*
		 * If the urgent data has already been acknowledged, we
		 * should ignore TH_URG below
		 */
		if (urp < 0)
			flags &= ~TH_URG;
	}
	/*
	 * rgap is the amount of stuff received out of window.  A negative
	 * value is the amount out of window.
	 */
	if (rgap < 0) {
		mblk_t	*mp2;
		if (tcp->tcp_rwnd == 0)
			BUMP_MIB(tcp_mib.tcpInWinProbe);
		else {
			BUMP_MIB(tcp_mib.tcpInDataPastWinSegs);
			UPDATE_MIB(tcp_mib.tcpInDataPastWinBytes, -rgap);
		}
		if (flags & TH_FIN) {
			/*
			 * seg_len does not include the FIN, so if more than
			 * just the FIN is out of window, we act like we don't
			 * see it.  (If just the FIN is out of window, rgap
			 * will be zero and we will go ahead and acknowledge
			 * the FIN.)
			 */
			flags &= ~TH_FIN;
		}
		/* Fix seg_len and make sure there is something left. */
		seg_len += rgap;
		if (seg_len <= 0) {
			/* Adjust seg_len to be the original value */
			seg_len -= rgap;
			goto unacceptable;
		}
		/* Pitch out of window stuff off the end. */
		rgap = seg_len;
		mp2 = mp;
		do {
			ASSERT((uintptr_t)(mp2->b_wptr - mp2->b_rptr) <=
			    (uintptr_t)INT_MAX);
			rgap -= (int)(mp2->b_wptr - mp2->b_rptr);
			if (rgap < 0) {
				mp2->b_wptr += rgap;
				if ((mp1 = mp2->b_cont) != NULL) {
					mp2->b_cont = nilp(mblk_t);
					freemsg(mp1);
				}
				break;
			}
		} while ((mp2 = mp2->b_cont) != NULL);
	}
ok:;
	/*
	 * Check whether we can update tcp_ts_recent.  This test is
	 * NOT the one in RFC 1323 3.4.  It is from Braden, 1993, "TCP
	 * Extensions for High Performance: An Update", Internet Draft.
	 */
	if (tcp->tcp_snd_ts_ok &&
	    TSTMP_GEQ(tcpopt.tcp_opt_ts_val, tcp->tcp_ts_recent) &&
	    SEQ_LEQ(seg_seq, tcp->tcp_rack)) {
		tcp->tcp_ts_recent = tcpopt.tcp_opt_ts_val;
		tcp->tcp_last_rcv_lbolt = lbolt;
	}

	if (seg_seq != tcp->tcp_rnxt || tcp->tcp_reass_head) {
		/*
		 * Clear the FIN bit in case it was set since we can not
		 * handle out-of-order FIN yet. This will cause the remote
		 * to retransmit the FIN.
		 * TODO: record the out-of-order FIN in the reassembly
		 * queue to avoid the remote having to retransmit.
		 */
		flags &= ~TH_FIN;
		if (seg_len > 0) {
			u_int	lflags = flags;

			/* Fill in the SACK blk list. */
			if (tcp->tcp_snd_sack_ok) {
				ASSERT(tcp->tcp_sack_info != NULL);
				tcp_sack_insert(tcp->tcp_sack_list,
				    seg_seq, seg_seq + seg_len,
				    &(tcp->tcp_num_sack_blk));
			}

			/*
			 * Attempt reassembly and see if we have something
			 * ready to go.
			 */
			mp = tcp_reass(tcp, mp, seg_seq, &lflags);
			flags = lflags;
			/* Always ack out of order packets */
			flags |= TH_ACK_NEEDED | TH_PSH;
			if (mp) {
				ASSERT((uintptr_t)(mp->b_wptr - mp->b_rptr) <=
				    (uintptr_t)INT_MAX);
				seg_len = mp->b_cont ? msgdsize(mp) :
					(int)(mp->b_wptr - mp->b_rptr);
				seg_seq = tcp->tcp_rnxt;
			} else {
				/*
				 * Keep going even with nil mp.
				 * There may be a useful ACK or something else
				 * we don't want to miss.
				 */
				seg_len = 0;
			}
		}
	} else if (seg_len > 0) {
		BUMP_MIB(tcp_mib.tcpInDataInorderSegs);
		UPDATE_MIB(tcp_mib.tcpInDataInorderBytes, seg_len);
	}
	if ((flags & (TH_RST | TH_SYN | TH_URG | TH_ACK)) != TH_ACK) {
	if (flags & TH_RST) {
		if (mp)
			freemsg(mp);
		switch (tcp->tcp_state) {
		case TCPS_SYN_RCVD:
			(void) tcp_clean_death(tcp, ECONNREFUSED);
			break;
		case TCPS_ESTABLISHED:
		case TCPS_FIN_WAIT_1:
		case TCPS_FIN_WAIT_2:
		case TCPS_CLOSE_WAIT:
			(void) tcp_clean_death(tcp, ECONNRESET);
			break;
		case TCPS_CLOSING:
		case TCPS_LAST_ACK:
		case TCPS_TIME_WAIT:
			(void) tcp_clean_death(tcp, 0);
			break;
		default:
			(void) tcp_clean_death(tcp, ENXIO);
			break;
		}
		TRACE_1(TR_FAC_TCP, TR_TCP_RPUT_OUT, "tcp_rput end:  q %p",
		    q);
		return;
	}
	if (flags & TH_SYN) {
		if (seg_seq == tcp->tcp_irs) {
			flags &= ~TH_SYN;
			seg_seq++;
		} else {
			if (mp != NULL) {
				freemsg(mp);
			}
			/* See RFC793, Page 71 */
			tcp_xmit_ctl("TH_SYN", tcp, NULL, seg_ack, 0, TH_RST);
			/*
			 * Do not delete the TCP structure if it is in
			 * TIME_WAIT state.  Refer to RFC 1122, 4.2.2.13.
			 */
			if (tcp->tcp_state != TCPS_TIME_WAIT) {
				(void) tcp_clean_death(tcp, ECONNRESET);
			}
			TRACE_1(TR_FAC_TCP, TR_TCP_RPUT_OUT,
				"tcp_rput end:  q %p", q);
			return;
		}
	}
	/*
	 * urp could be -1 when the urp field in the packet is 0
	 * and TCP_OLD_URP_INTERPRETATION is set. This implies that the urgent
	 * byte was at seg_seq - 1, in which case we ignore the urgent flag.
	 */
	if (flags & TH_URG && urp >= 0) {
		if (!tcp->tcp_urp_last_valid ||
		    SEQ_GT(urp + seg_seq, tcp->tcp_urp_last)) {
			/*
			 * If we haven't generated the signal yet for this
			 * urgent pointer value, do it now.  Also, send up a
			 * zero-length M_DATA indicating whether or not this is
			 * the mark. The latter is not needed when a
			 * T_EXDATA_IND is sent up. However, if there are
			 * allocation failures this code relies on the sender
			 * retransmitting and the socket code for determining
			 * the mark should not block waiting for the peer to
			 * transmit. Thus, for simplicity we always send up the
			 * mark indication.
			 */
			mp1 = allocb(0, BPRI_MED);
			if (mp1 == NULL ||
			    !putnextctl1(q, M_PCSIG, SIGURG)) {
				/* Try again on the rexmit. */
				freemsg(mp1);
				freemsg(mp);
				TRACE_1(TR_FAC_TCP, TR_TCP_RPUT_OUT,
					"tcp_rput end:  q %p", q);
				return;
			}
			/*
			 * Mark with NOTMARKNEXT for now.
			 * The code below will change this to MARKNEXT
			 * if we are at the mark.
			 *
			 * If there are allocation failures (e.g. in dupmsg
			 * below) the next time tcp_rput_data sees the urgent
			 * segment it will send up the MSG*MARKNEXT message.
			 */
			mp1->b_flag |= MSGNOTMARKNEXT;
			freemsg(tcp->tcp_urp_mark_mp);
			tcp->tcp_urp_mark_mp = mp1;
			flags |= TH_SEND_URP_MARK;
#ifdef DEBUG
			(void) mi_strlog(tcp->tcp_rq, 1, SL_TRACE,
			    "tcp_rput: sent M_PCSIG 2 seq %x urp %x "
			    "last %x, %s",
			    seg_seq, urp, tcp->tcp_urp_last,
			    tcp_display(tcp));
#endif /* DEBUG */
			tcp->tcp_urp_last_valid = true;
			tcp->tcp_urp_last = urp + seg_seq;
		} else if (tcp->tcp_urp_mark_mp != NULL) {
			/*
			 * An allocation failure prevented the previous
			 * tcp_rput_data from sending up the allocated
			 * MSG*MARKNEXT message - send it up this time
			 * around.
			 */
			flags |= TH_SEND_URP_MARK;
		}

		/*
		 * If the urgent byte is in this segment, make sure that it is
		 * all by itself.  This makes it much easier to deal with the
		 * possibility of an allocation failure on the T_exdata_ind.
		 * Note that seg_len is the number of bytes in the segment, and
		 * urp is the offset into the segment of the urgent byte.
		 * urp < seg_len means that the urgent byte is in this segment.
		 */
		if (urp < seg_len) {
			if (seg_len != 1) {
				/* Break it up and feed it back in. */
				mp->b_rptr = orptr;
				if (urp > 0) {
					/*
					 * There is stuff before the urgent
					 * byte.
					 */
					mp1 = dupmsg(mp);
					if (!mp1) {
						/*
						 * Trim from urgent byte on.
						 * The rest will come back.
						 */
						(void) adjmsg(mp,
						    urp - seg_len);
						tcp_rput_data(q, mp, -1);
						TRACE_1(TR_FAC_TCP,
						    TR_TCP_RPUT_OUT,
						    "tcp_rput end:  q %p", q);
						return;
					}
					(void) adjmsg(mp1, urp - seg_len);
					/* Feed this piece back in. */
					tcp_rput_data(q, mp1, -1);
				}
				if (urp != seg_len - 1) {
					/*
					 * There is stuff after the urgent
					 * byte.
					 */
					mp1 = dupmsg(mp);
					if (!mp1) {
						/*
						 * Trim everything beyond the
						 * urgent byte.  The rest will
						 * come back.
						 */
						(void) adjmsg(mp,
						    urp + 1 - seg_len);
						tcp_rput_data(q, mp, -1);
						TRACE_1(TR_FAC_TCP,
						    TR_TCP_RPUT_OUT,
						    "tcp_rput end:  q %p", q);
						return;
					}
					(void) adjmsg(mp1, urp + 1 - seg_len);
					tcp_rput_data(q, mp1, -1);
				}
				tcp_rput_data(q, mp, -1);
				TRACE_1(TR_FAC_TCP, TR_TCP_RPUT_OUT,
					"tcp_rput end:  q %p", q);
				return;
			}
			/*
			 * This segment contains only the urgent byte.  We
			 * have to allocate the T_exdata_ind, if we can.
			 */
			if (!tcp->tcp_urp_mp) {
				struct T_exdata_ind *tei;
				mp1 = allocb(sizeof (struct T_exdata_ind),
				    BPRI_MED);
				if (!mp1) {
					/*
					 * Sigh... It'll be back.
					 * Generate any MSG*MARK message now.
					 */
					freemsg(mp);
					mp = NULL;
					seg_len = 0;
					if (flags & TH_SEND_URP_MARK) {


						ASSERT(tcp->tcp_urp_mark_mp);
						tcp->tcp_urp_mark_mp->b_flag &=
							~MSGNOTMARKNEXT;
						tcp->tcp_urp_mark_mp->b_flag |=
							MSGMARKNEXT;
					}
					goto ack_check;
				}
				mp1->b_datap->db_type = M_PROTO;
				tei = (struct T_exdata_ind *)mp1->b_rptr;
				tei->PRIM_type = T_EXDATA_IND;
				tei->MORE_flag = 0;
				mp1->b_wptr = (u_char *)&tei[1];
				tcp->tcp_urp_mp = mp1;
#ifdef DEBUG
				(void) mi_strlog(tcp->tcp_rq, 1, SL_TRACE,
				    "tcp_rput: allocated exdata_ind %s",
				    tcp_display(tcp));
#endif /* DEBUG */
				/*
				 * There is no need to send a separate MSG*MARK
				 * message since the T_EXDATA_IND will be sent
				 * now.
				 */
				flags &= ~TH_SEND_URP_MARK;
				freemsg(tcp->tcp_urp_mark_mp);
				tcp->tcp_urp_mark_mp = NULL;
			}
			/*
			 * Now we are all set.  On the next putnext upstream,
			 * tcp_urp_mp will be non-nil and will get prepended
			 * to what has to be this piece containing the urgent
			 * byte.  If for any reason we abort this segment below,
			 * if it comes back, we will have this ready, or it
			 * will get blown off in close.
			 */
		} else if (urp == seg_len) {
			/*
			 * The urgent byte is the next byte after this sequence
			 * number. If there is data it is marked with
			 * MSGMARKNEXT and any tcp_urp_mark_mp is discarded
			 * since it is not needed. Otherwise, if the code
			 * above just allocated a zero-length tcp_urp_mark_mp
			 * message, that message is tagged with MSGMARKNEXT.
			 * Sending up these MSGMARKNEXT messages makes
			 * SIOCATMARK work correctly even though
			 * the T_EXDATA_IND will not be sent up until the
			 * urgent byte arrives.
			 */
			if (seg_len != 0) {
				flags |= TH_MARKNEXT_NEEDED;
				freemsg(tcp->tcp_urp_mark_mp);
				tcp->tcp_urp_mark_mp = NULL;
				flags &= ~TH_SEND_URP_MARK;
			} else if (tcp->tcp_urp_mark_mp != NULL) {
				flags |= TH_SEND_URP_MARK;
				tcp->tcp_urp_mark_mp->b_flag &=
					~MSGNOTMARKNEXT;
				tcp->tcp_urp_mark_mp->b_flag |= MSGMARKNEXT;
			}
#ifdef DEBUG
			(void) mi_strlog(tcp->tcp_rq, 1, SL_TRACE,
			    "tcp_rput: AT MARK, len %d, flags 0x%x, %s",
			    seg_len, flags,
			    tcp_display(tcp));
#endif /* DEBUG */
		} else {
			/* Data left until we hit mark */
#ifdef DEBUG
			(void) mi_strlog(tcp->tcp_rq, 1, SL_TRACE,
			    "tcp_rput: URP %d bytes left, %s",
			    urp - seg_len, tcp_display(tcp));
#endif /* DEBUG */
		}
	}

process_ack:
	if (!(flags & TH_ACK)) {
		if (mp)
			freemsg(mp);
		goto xmit_check;
	}
	}
	bytes_acked = (int)(seg_ack - tcp->tcp_suna);
	if (tcp->tcp_state == TCPS_SYN_RCVD) {
		/*
		 * NOTE: RFC 793 pg. 72 says this should be 'bytes_acked < 0'
		 * but that would mean we have an ack that ignored our SYN.
		 */
		if (bytes_acked < 1 || SEQ_GT(seg_ack, tcp->tcp_snxt)) {
			freemsg(mp);
			tcp_xmit_ctl("TCPS_SYN_RCVD-bad_ack", tcp, NULL,
			    seg_ack, 0, TH_RST);
			TRACE_1(TR_FAC_TCP, TR_TCP_RPUT_OUT,
				"tcp_rput end:  q %p", q);
			return;
		}

		if (tcp->tcp_conn.tcp_eager_conn_ind != NULL) {
			/* 3-way handshake complete - pass up the T_CONN_IND */
			tcp_t	*listener = tcp->tcp_listener;
			ipaddr_t *addr_cache;

			putnext(tcp->tcp_rq,
			    tcp->tcp_conn.tcp_eager_conn_ind);
			tcp->tcp_conn.tcp_eager_conn_ind = nilp(mblk_t);

			ASSERT(q == listener->tcp_rq);
			listener->tcp_conn_req_cnt_q0--;
			listener->tcp_conn_req_cnt_q++;

			/* Move from SYN_RCVD to ESTABLISHED list  */
			tcp->tcp_eager_next_q0->tcp_eager_prev_q0 =
			    tcp->tcp_eager_prev_q0;
			tcp->tcp_eager_prev_q0->tcp_eager_next_q0 =
			    tcp->tcp_eager_next_q0;
			tcp->tcp_eager_prev_q0 = NULL;
			tcp->tcp_eager_next_q0 = NULL;

			tcp->tcp_eager_next_q = listener->tcp_eager_next_q;
			listener->tcp_eager_next_q = tcp;

			/* we have timed out before */
			if (tcp->tcp_syn_rcvd_timeout != 0) {
				tcp->tcp_syn_rcvd_timeout = 0;
				listener->tcp_syn_rcvd_timeout--;
				if (listener->tcp_syn_defense &&
				    listener->tcp_syn_rcvd_timeout <=
				    (tcp_conn_req_max_q0 >> 5) &&
				    10*MINUTES < TICK_TO_MSEC(lbolt -
				    listener->tcp_last_rcv_lbolt)) {
					/*
					 * Turn off the defense mode if we
					 * believe the SYN attack is over.
					 */
					listener->tcp_syn_defense = false;
					if (listener->tcp_ip_addr_cache) {
						kmem_free((void *)listener->
						    tcp_ip_addr_cache,
						    IP_ADDR_CACHE_SIZE *
						    sizeof (ipaddr_t));
						listener->tcp_ip_addr_cache =
						    NULL;
					}
				}
			}
			addr_cache = (ipaddr_t *)(listener->tcp_ip_addr_cache);
			if (addr_cache) {
				/*
				 * We have finished a 3-way handshake with this
				 * remote host. This proves the IP addr is good.
				 * Cache it!
				 */
				addr_cache[IP_ADDR_CACHE_HASH(
				    tcp->tcp_remote)] = tcp->tcp_remote;
			}
		}

		if (tcp->tcp_active_open) {
			/*
			 * We are seeing the final ack in the three way
			 * hand shake of a active open'ed connection
			 * so we must send up a T_CONN_CON
			 */
			if (!tcp_conn_con(tcp, (iph_t *)mp->b_rptr, tcph)) {
				freemsg(mp);
				TRACE_1(TR_FAC_TCP, TR_TCP_RPUT_OUT,
					"tcp_rput end:  q %X", q);
				return;
			}
		}

		tcp->tcp_suna = tcp->tcp_iss + 1;	/* One for the SYN */
		bytes_acked--;

		/*
		 * If SYN was retransmitted, need to reset all
		 * retransmission info as this segment will be
		 * treated as a dup ACK.
		 */
		if (tcp->tcp_rexmit) {
			tcp->tcp_rexmit = 0;
			tcp->tcp_rexmit_nxt = tcp->tcp_snxt;
			tcp->tcp_rexmit_max = tcp->tcp_snxt;
			tcp->tcp_snd_burst = TCP_CWND_INFINITE;
		}

		/*
		 * We set the send window to zero here.
		 * This is needed if there is data to be
		 * processed already on the queue.
		 * Later (at swnd_update label), the
		 * "new_swnd > tcp_swnd" condition is satisfied
		 * the XMIT_NEEDED flag is set in the current
		 * (SYN_RCVD) state. This ensures tcp_wput_slow is
		 * called if there is already data on queue in
		 * this state.
		 */
		tcp->tcp_swnd = 0;

		if (new_swnd > tcp->tcp_max_swnd)
			tcp->tcp_max_swnd = new_swnd;
		tcp->tcp_swl1 = seg_seq;
		tcp->tcp_swl2 = seg_ack;
		tcp->tcp_state = TCPS_ESTABLISHED;
		tcp->tcp_valid_bits &= ~TCP_ISS_VALID;
#if 0
		/*
		 * TODO: check out entire 'quick connect' sequence, we are
		 * probably better off hand concatenating these two than
		 * recurring.
		 */
		enableok(tcp->tcp_rq);
		if (mp1 = getq(tcp->tcp_rq)) {
			rptr = mp1->b_rptr;
			tcph = (tcph_t *)&rptr[IPH_HDR_LENGTH((iph_t *)rptr)];
			if (!(tcph->th_flags[0] & TH_ACK)) {
				u_long	dummy_ack = tcp->tcp_suna - 1;
				U32_TO_BE32(dummy_ack, tcph->th_ack);
				tcph->th_flags[0] |= TH_ACK;
			}
			tcph->th_flags[0] &= ~TH_SYN;
			/* TODO: recursion problems?? */
			tcp_rput(tcp->tcp_rq, mp1);
		}
#endif
	}
	/* This code follows 4.4BSD-Lite2 mostly. */
	if (bytes_acked < 0)
		goto est;
	mp1 = tcp->tcp_xmit_head;
	if (bytes_acked == 0) {
		if (seg_len == 0 && new_swnd == tcp->tcp_swnd) {
			BUMP_MIB(tcp_mib.tcpInDupAck);
			/*
			 * Fast retransmit.  When we have seen exactly three
			 * identical ACKs while we have unacked data
			 * outstanding we take it as a hint that our peer
			 * dropped something.
			 *
			 * If TCP is retransmitting, don't do fast retransmit.
			 */
			if (mp1 && tcp->tcp_suna != tcp->tcp_snxt &&
			    tcp->tcp_rexmit == 0) {
				if (++tcp->tcp_dupack_cnt ==
				    tcp_dupack_fast_retransmit) {
				int npkt;
				BUMP_MIB(tcp_mib.tcpOutFastRetrans);
				/*
				 * Adjust cwnd since the duplicate
				 * ack indicates that a packet was
				 * dropped (due to congestion.)
				 *
				 * Here we perform congestion avoidance,
				 * but NOT slow start. This is known
				 * as the Fast Recovery Algorithm.
				 */
				npkt = (MIN(tcp->tcp_cwnd,
				    tcp->tcp_swnd) >> 1) / tcp->tcp_mss;
				if (npkt < 2)
					npkt = 2;
				tcp->tcp_cwnd_ssthresh = npkt * tcp->tcp_mss;
				tcp->tcp_cwnd = (npkt + tcp->tcp_dupack_cnt) *
					tcp->tcp_mss;

				if (tcp->tcp_cwnd > tcp->tcp_cwnd_max)
					tcp->tcp_cwnd = tcp->tcp_cwnd_max;

				/*
				 * We do Hoe's algorithm.  Refer to her
				 * paper "Improving the Start-up Behavior
				 * of a Congestoin Control Scheme for TCP,"
				 * appeared in SINGCOMM'96.
				 *
				 * Save highest seq no we have sent so far.
				 * Be careful about the invisible FIN byte.
				 */
				if ((tcp->tcp_valid_bits & TCP_FSS_VALID) &&
				    (tcp->tcp_unsent == 0)) {
					tcp->tcp_rexmit_max = tcp->tcp_snxt -
					    1;
				} else {
					tcp->tcp_rexmit_max = tcp->tcp_snxt;
				}

				/*
				 * Do not allow bursty traffic during.
				 * fast recovery.  Refer to Fall and Floyd's
				 * paper "Simulation-based Comparisons of
				 * Tahoe, Reno and SACK TCP" (in CCR ??)
				 * This is a best current practise.
				 */
				tcp->tcp_snd_burst = TCP_CWND_SS;

				/*
				 * For SACK:
				 * Calculate tcp_pipe, which is the
				 * estimated number of bytes in
				 * network.
				 *
				 * tcp_fack is the highest sack'ed seq num
				 * TCP has received.
				 *
				 * tcp_pipe is explained in the above quoted
				 * Fall and Floyd's paper.  tcp_fack is
				 * explained in Mathis and Mahdavi's
				 * "Forward Acknowledgment: Refining TCP
				 * Congestion Control" in SIGCOMM '96.
				 */
				if (tcp->tcp_snd_sack_ok) {
					ASSERT(tcp->tcp_sack_info != NULL);
					if (tcp->tcp_notsack_list != NULL) {
						tcp->tcp_pipe = tcp->tcp_snxt -
						    tcp->tcp_fack;
						tcp->tcp_sack_snxt = seg_ack;
						flags |= TH_NEED_SACK_REXMIT;
					} else {
						/*
						 * Always initialize tcp_pipe
						 * even though we don't have
						 * any SACK info.  If later
						 * we get SACK info and
						 * tcp_pipe is not initialized,
						 * funny things will happen.
						 */
						tcp->tcp_pipe =
						    tcp->tcp_cwnd_ssthresh;
					}
				} else {
					flags |= TH_REXMIT_NEEDED;
				}

				} else if (tcp->tcp_dupack_cnt >
				    tcp_dupack_fast_retransmit) {
					if (tcp->tcp_snd_sack_ok &&
					    tcp->tcp_notsack_list != NULL) {
						flags |= TH_NEED_SACK_REXMIT;
						tcp->tcp_pipe -= tcp->tcp_mss;
						if (tcp->tcp_pipe < 0)
							tcp->tcp_pipe = 0;
					} else {
					/*
					 * We know that one more packet has
					 * left the pipe thus we can update
					 * cwnd.
					 */
					uint32_t cwnd = tcp->tcp_cwnd;
					cwnd += tcp->tcp_mss;
					if (cwnd > tcp->tcp_cwnd_max)
						cwnd = tcp->tcp_cwnd_max;
					tcp->tcp_cwnd = cwnd;
					flags |= TH_XMIT_NEEDED;
					}

				}
			}
		} else if (tcp->tcp_zero_win_probe) {
			/*
			 * If the window has opened, need to arrange
			 * to send additional data.
			 */
			if (new_swnd != 0) {
				/* tcp_suna != tcp_snxt */
				/* Packet contains a window update */
				BUMP_MIB(tcp_mib.tcpInWinUpdate);
				tcp->tcp_zero_win_probe = 0;
				tcp->tcp_timer_backoff = 0;
				tcp->tcp_ms_we_have_waited = 0;

				/*
				 * Transmit starting with tcp_suna since
				 * the one byte probe is not ack'ed.
				 * If TCP has sent more than one identical
				 * probe, tcp_rexmit will be set.  That means
				 * tcp_wput_slow() will send out the one
				 * byte along with new data.  Otherwise,
				 * fake the retransmission.
				 */
				flags |= TH_XMIT_NEEDED;
				if (tcp->tcp_rexmit == 0) {
					tcp->tcp_rexmit = 1;
					tcp->tcp_rexmit_nxt = tcp->tcp_suna;
					tcp->tcp_rexmit_max = tcp->tcp_suna +
					    tcp_zero_win_probesize;
				}
			}
		}
		goto swnd_update;
	}

	/*
	 * Check for "acceptability" of ACK value per RFC 793, pages 72 - 73.
	 * If the ACK value acks something that we have not yet sent, it might
	 * be an old duplicate segment.  Send an ACK to re-synchronize the
	 * other side.
	 * Note: reset in response to unacceptable ACK in SYN_RECEIVE
	 * state is handled above, so we can always just drop the segment and
	 * send an ACK here.
	 *
	 * Should we send ACKs in response to ACK only segments?
	 */
	if (SEQ_GT(seg_ack, tcp->tcp_snxt)) {
		/* drop the received segment */
		if (mp)
			freemsg(mp);

		/* send an ACK */
		mp = tcp_ack_mp(tcp);
		if (mp) {
			putnext(tcp->tcp_wq, mp);
			BUMP_LOCAL(tcp->tcp_obsegs);
			BUMP_MIB(tcp_mib.tcpOutAck);
		}
		TRACE_1(TR_FAC_TCP, TR_TCP_RPUT_OUT,
			"tcp_rput end:  q %p", q);
		return;
	}

	/*
	 * TCP gets a new ACK, update the notsack'ed list to delete those
	 * blocks that are covered by this ACK.
	 */
	if (tcp->tcp_snd_sack_ok && tcp->tcp_notsack_list != NULL) {
		tcp_notsack_remove(&(tcp->tcp_notsack_list), seg_ack,
		    &(tcp->tcp_num_notsack_blk), &(tcp->tcp_cnt_notsack_list));
	}

	/*
	 * If we got an ACK after fast retransmit, check to see
	 * if it is a partial ACK.  If it is not and the congestion
	 * window was inflated to account for the other side's
	 * cached packets, retract it.  If it is, do Hoe's algorithm.
	 */
	if (tcp->tcp_dupack_cnt >= tcp_dupack_fast_retransmit) {
		if (SEQ_GEQ(seg_ack, tcp->tcp_rexmit_max)) {
			tcp->tcp_dupack_cnt = 0;
			/*
			 * Restore the orig tcp_cwnd_ssthresh after
			 * fast retransmit phase.
			 */
			if (tcp->tcp_cwnd > tcp->tcp_cwnd_ssthresh) {
				tcp->tcp_cwnd = tcp->tcp_cwnd_ssthresh;
			}
			tcp->tcp_rexmit_max = seg_ack;
			tcp->tcp_cwnd_cnt = 0;
			tcp->tcp_snd_burst = TCP_CWND_INFINITE;

			/*
			 * Remove all notsack info to avoid confusion with
			 * the next fast retrasnmit/recovery phase.
			 */
			if (tcp->tcp_snd_sack_ok &&
			    tcp->tcp_notsack_list != NULL) {
				TCP_NOTSACK_REMOVE_ALL(tcp->tcp_notsack_list);
			}
		} else {
			if (tcp->tcp_snd_sack_ok &&
			    tcp->tcp_notsack_list != NULL) {
				flags |= TH_NEED_SACK_REXMIT;
				tcp->tcp_pipe -= tcp->tcp_mss;
				if (tcp->tcp_pipe < 0)
					tcp->tcp_pipe = 0;
			} else {
				/*
				 * Hoe's algorithm:
				 *
				 * Retransmit the unack'ed segment and
				 * restart fast recovery.  Note that we
				 * need to scale back tcp_cwnd to the
				 * original value when we started fast
				 * recovery.  This is to prevent overly
				 * aggressive behaviour in sending new
				 * segments.
				 */
				tcp->tcp_cwnd = tcp->tcp_cwnd_ssthresh +
					tcp_dupack_fast_retransmit *
					tcp->tcp_mss;
				tcp->tcp_cwnd_cnt = tcp->tcp_cwnd;
				BUMP_MIB(tcp_mib.tcpOutFastRetrans);
				flags |= TH_REXMIT_NEEDED;
			}
		}
	} else {
		tcp->tcp_dupack_cnt = 0;
		if (tcp->tcp_rexmit) {
			/*
			 * TCP is retranmitting.  If the ACK ack's all
			 * outstanding data, update tcp_rexmit_max and
			 * tcp_rexmit_nxt.  Otherwise, update tcp_rexmit_nxt
			 * to the correct value.
			 *
			 * Note that SEQ_LEQ() is used.  This is to avoid
			 * unnecessary fast retransmit caused by dup ACKs
			 * received when TCP does slow start retransmission
			 * after a time out.  During this phase, TCP may
			 * send out segments which are already received.
			 * This causes dup ACKs to be sent back.
			 */
			if (SEQ_LEQ(seg_ack, tcp->tcp_rexmit_max)) {
				if (SEQ_GT(seg_ack, tcp->tcp_rexmit_nxt)) {
					tcp->tcp_rexmit_nxt = seg_ack;
				}
				if (seg_ack != tcp->tcp_rexmit_max) {
					flags |= TH_XMIT_NEEDED;
				}
			} else {
				tcp->tcp_rexmit = 0;
				tcp->tcp_rexmit_nxt = tcp->tcp_snxt;
				tcp->tcp_snd_burst = TCP_CWND_INFINITE;
			}
			tcp->tcp_ms_we_have_waited = 0;
		}
	}

	BUMP_MIB(tcp_mib.tcpInAckSegs);
	UPDATE_MIB(tcp_mib.tcpInAckBytes, bytes_acked);
	tcp->tcp_suna = seg_ack;
	if (tcp->tcp_zero_win_probe != 0) {
		tcp->tcp_zero_win_probe = 0;
		tcp->tcp_timer_backoff = 0;
	}
	if (!mp1) {
		/*
		 * Something was acked, but we had no transmitted data
		 * outstanding.  Either the FIN or the SYN must have been
		 * acked.  If it was the FIN, we want to note that.
		 * The TCP_FSS_VALID bit will be on in this case.  Otherwise
		 * it must have been the SYN acked, and we have nothing
		 * special to do here.
		 */
		if (tcp->tcp_fin_sent)
			tcp->tcp_fin_acked = true;
		else if (!(tcp->tcp_valid_bits & TCP_ISS_VALID))
			BUMP_MIB(tcp_mib.tcpInAckUnsent);
		goto swnd_update;
	}

	/* Update the congestion window */
	{
	uint32_t cwnd = tcp->tcp_cwnd;
	uint32_t add = tcp->tcp_mss;

	if (cwnd >= tcp->tcp_cwnd_ssthresh) {
		/*
		 * This is to prevent an increase of less than 1 MSS of
		 * tcp_cwnd.  With partial increase, tcp_wput_slow() may
		 * send out tinygrams in order to preserve mblk boundaries.
		 * By initializing tcp_cwnd_cnt to new tcp_cwnd and
		 * decrementing it by 1 MSS for every ACKs, tcp_cwnd is
		 * increased by 1 MSS for every RTTs.
		 */
		if (tcp->tcp_cwnd_cnt <= 0) {
			tcp->tcp_cwnd_cnt = cwnd + add;
		} else {
			tcp->tcp_cwnd_cnt -= add;
			add = 0;
		}
	}

	tcp->tcp_cwnd = MIN(cwnd + add, tcp->tcp_cwnd_max);
	}

	/* See if the latest urgent data has been acknowledged */
	if ((tcp->tcp_valid_bits & TCP_URG_VALID) &&
	    SEQ_GT(seg_ack, tcp->tcp_urg))
		tcp->tcp_valid_bits &= ~TCP_URG_VALID;

	/* Can we update the RTT estimates? */
	if (tcp->tcp_snd_ts_ok) {
		tcp_set_rto(tcp, (int32_t)lbolt -
		    (int32_t)tcpopt.tcp_opt_ts_ecr);

		/* If needed, restart the timer. */
		if (tcp->tcp_set_timer == 1) {
			TCP_TIMER_RESTART(tcp, tcp->tcp_rto);
			tcp->tcp_set_timer = 0;
		}
		/*
		 * Update tcp_csuna in case the other side stops sending
		 * us timestamps.
		 */
		tcp->tcp_csuna = tcp->tcp_snxt;
	} else if (SEQ_GT(seg_ack, tcp->tcp_csuna)) {
		/*
		 * An ACK sequence we haven't seen before, so get the RTT
		 * and update the RTO.
		 */
		tcp_set_rto(tcp, (int32_t)lbolt - (int32_t)mp1->b_prev);

		/* Remeber the last sequence to be ACKed */
		tcp->tcp_csuna = seg_ack;
		if (tcp->tcp_set_timer == 1) {
			TCP_TIMER_RESTART(tcp, tcp->tcp_rto);
			tcp->tcp_set_timer = 0;
		}
	} else {
		BUMP_MIB(tcp_mib.tcpRttNoUpdate);
	}

	/* Eat acknowledged bytes off the xmit queue. */
	for (;;) {
		mblk_t	*mp2;
		u_char	*wptr;

		wptr = mp1->b_wptr;
		ASSERT((uintptr_t)(wptr - mp1->b_rptr) <= (uintptr_t)INT_MAX);
		bytes_acked -= (int)(wptr - mp1->b_rptr);
		if (bytes_acked < 0) {
			mp1->b_rptr = wptr + bytes_acked;
			break;
		}
		mp1->b_prev = nilp(mblk_t);
		mp2 = mp1;
		mp1 = mp1->b_cont;
		freeb(mp2);
		if (bytes_acked == 0) {
			if (!mp1)
				goto pre_swnd_update;
			if (mp2 != tcp->tcp_xmit_tail)
				break;
			tcp->tcp_xmit_tail = mp1;
			ASSERT((uintptr_t)(mp1->b_wptr - mp1->b_rptr) <=
			    (uintptr_t)INT_MAX);
			tcp->tcp_xmit_tail_unsent = (int)(mp1->b_wptr -
			    mp1->b_rptr);
			break;
		}
		if (!mp1) {
			/* TODO check that tcp_fin_sent has been set */
			/*
			 * More was acked but there is nothing more
			 * outstanding.  This means that the FIN was
			 * just acked or that we're talking to a clown.
			 */
			if (tcp->tcp_fin_sent)
				tcp->tcp_fin_acked = true;
			else
				BUMP_MIB(tcp_mib.tcpInAckUnsent);
			goto pre_swnd_update;
		}
		ASSERT(mp2 != tcp->tcp_xmit_tail);
	}
	if (tcp->tcp_unsent) {
		flags |= TH_XMIT_NEEDED;
	}
pre_swnd_update:
	tcp->tcp_xmit_head = mp1;
swnd_update:
	if (SEQ_LT(tcp->tcp_swl1, seg_seq) || tcp->tcp_swl1 == seg_seq &&
	    (SEQ_LT(tcp->tcp_swl2, seg_ack) ||
	    tcp->tcp_swl2 == seg_ack && new_swnd > tcp->tcp_swnd)) {
		/*
		 * A segment in, or immediately to the right of, the window
		 * with a seq > then the last window update seg or a dup
		 * seq and either a ack > then the last window update seg
		 * or a dup seq with a larger window.
		 */
		if (tcp->tcp_unsent && new_swnd > tcp->tcp_swnd)
			flags |= TH_XMIT_NEEDED;
		tcp->tcp_swnd = new_swnd;
		tcp->tcp_swl1 = seg_seq;
		tcp->tcp_swl2 = seg_ack;
	}
est:
	if (tcp->tcp_state > TCPS_ESTABLISHED) {
		switch (tcp->tcp_state) {
		case TCPS_FIN_WAIT_1:
			if (tcp->tcp_fin_acked) {
				tcp->tcp_state = TCPS_FIN_WAIT_2;
				/*
				 * We implement the non-standard BSD/SunOS
				 * FIN_WAIT_2 flushing algorithm.
				 * If there is no user attached to this
				 * TCP endpoint, then this TCP struct
				 * could hang around forever in FIN_WAIT_2
				 * state if the peer forgets to send us
				 * a FIN.  To prevent this, we wait only
				 * 2*MSL (a convenient time value) for
				 * the FIN to arrive.  If it doesn't show up,
				 * we flush the TCP endpoint.  This algorithm,
				 * though a violation of RFC-793, has worked
				 * for over 10 years in BSD systems.
				 * Note: SunOS 4.x waits 675 seconds before
				 * flushing the FIN_WAIT_2 connection.
				 */
				TCP_TIMER_RESTART(tcp,
				    tcp_fin_wait_2_flush_interval);
			}
			break;
		case TCPS_FIN_WAIT_2:
			break;	/* Shutdown hook? */
		case TCPS_LAST_ACK:
			if (mp) {
				freemsg(mp);
				mp = NULL;
			}
			if (tcp->tcp_fin_acked) {
				(void) tcp_clean_death(tcp, 0);
				TRACE_1(TR_FAC_TCP, TR_TCP_RPUT_OUT,
				    "tcp_rput end:  q %p", q);
				return;
			}
			goto xmit_check;
		case TCPS_CLOSING:
			if (tcp->tcp_fin_acked) {
				tcp->tcp_state = TCPS_TIME_WAIT;
				if (!TCP_IS_DETACHED(tcp)) {
					TCP_TIMER_RESTART(tcp,
					    tcp_close_wait_interval);
				} else {
					tcp_time_wait_set(tcp);
					tcp_time_wait_append(tcp);
				}
			}
			/*FALLTHRU*/
		case TCPS_TIME_WAIT:
		case TCPS_CLOSE_WAIT:
			if (mp) {
				freemsg(mp);
				mp = NULL;
			}
			goto xmit_check;
		default:
			break;
		}
	}
	if (flags & TH_FIN) {
		/* Make sure we ack the fin */
		flags |= TH_ACK_NEEDED;
		if (!tcp->tcp_fin_rcvd) {
			tcp->tcp_fin_rcvd = true;
			tcp->tcp_rnxt++;
			tcph = tcp->tcp_tcph;
			U32_TO_ABE32(tcp->tcp_rnxt, tcph->th_ack);

			/*
			 * Generate the ordrel_ind at the end unless we
			 * are an eager guy.
			 * In the eager case tcp_rsrv will do this when run
			 * after tcp_accept is done.
			 */
			if (tcp->tcp_listener == NULL)
				flags |= TH_ORDREL_NEEDED;
			switch (tcp->tcp_state) {
			case TCPS_SYN_RCVD:
			case TCPS_ESTABLISHED:
				tcp->tcp_state = TCPS_CLOSE_WAIT;
				/* Keepalive? */
				break;
			case TCPS_FIN_WAIT_1:
				if (!tcp->tcp_fin_acked) {
					tcp->tcp_state = TCPS_CLOSING;
					break;
				}
				/* FALLTHRU */
			case TCPS_FIN_WAIT_2:
				tcp->tcp_state = TCPS_TIME_WAIT;
				if (!TCP_IS_DETACHED(tcp)) {
					TCP_TIMER_RESTART(tcp,
					    tcp_close_wait_interval);
				} else {
					tcp_time_wait_set(tcp);
					tcp_time_wait_append(tcp);
				}
				if (seg_len) {
					/*
					 * implies data piggybacked on FIN.
					 * break to handle data.
					 */
					break;
				}
				if (mp)
					freemsg(mp);
				mp = NULL;
				goto ack_check;
			}
		}
	}
	if (mp == NULL)
		goto xmit_check;
	if (seg_len == 0) {
		freemsg(mp);
		mp = NULL;
		goto xmit_check;
	}
	if (mp->b_rptr == mp->b_wptr) {
		/*
		 * The header has been consumed, so we remove the
		 * zero-length mblk here.
		 */
		mp1 = mp;
		mp = mp->b_cont;
		freeb(mp1);
	}
	tcph = tcp->tcp_tcph;
	tcp->tcp_rack_cnt += seg_len;
	{
		uint32_t cur_max;
		cur_max = tcp->tcp_rack_cur_max;
		if (tcp->tcp_rack_cnt >= cur_max) {
			/*
			 * We have more unacked data than we should - send
			 * an ACK now.
			 */
			flags |= TH_ACK_NEEDED;
			cur_max += tcp->tcp_mss;
			if (cur_max > tcp->tcp_rack_abs_max)
				cur_max = tcp->tcp_rack_abs_max;
			tcp->tcp_rack_cur_max = cur_max;
		} else if (seg_len < tcp->tcp_mss) {
			/*
			 * If we get a segment that is less than an mss, and we
			 * already have unacknowledged data, and the amount
			 * unacknowledged is not a multiple of mss, then we
			 * better generate an ACK now.  Otherwise, this may be
			 * the tail piece of a transaction, and we would rather
			 * wait for the response.
			 */
			uint32_t udif;
			ASSERT((uintptr_t)(tcp->tcp_rnxt - tcp->tcp_rack) <=
			    (uintptr_t)INT_MAX);
			udif = (int)(tcp->tcp_rnxt - tcp->tcp_rack);
			if (udif && (udif % tcp->tcp_mss))
				flags |= TH_ACK_NEEDED;
			else
				flags |= TH_TIMER_NEEDED;
		} else {
			/* Start delayed ack timer */
			flags |= TH_TIMER_NEEDED;
		}
	}
	tcp->tcp_rnxt += seg_len;
	U32_TO_ABE32(tcp->tcp_rnxt, tcph->th_ack);

	/* Update SACK list */
	if (tcp->tcp_snd_sack_ok && tcp->tcp_num_sack_blk > 0) {
		tcp_sack_remove(tcp->tcp_sack_list, tcp->tcp_rnxt,
		    &(tcp->tcp_num_sack_blk));
	}

	if (tcp->tcp_urp_mp) {
		tcp->tcp_urp_mp->b_cont = mp;
		mp = tcp->tcp_urp_mp;
		tcp->tcp_urp_mp = nilp(mblk_t);
		/* Ready for a new signal. */
		tcp->tcp_urp_last_valid = false;
#ifdef DEBUG
		(void) mi_strlog(tcp->tcp_rq, 1, SL_TRACE,
		    "tcp_rput: sending exdata_ind %s",
		    tcp_display(tcp));
#endif /* DEBUG */
	}

	if (tcp->tcp_listener) {
		/* XXX: handle urgent data here? */
		/*
		 * Side queue inbound data until the accept happens.
		 * tcp_rsrv drains this when the accept happens.
		 */
		if (tcp->tcp_rcv_head == nilp(mblk_t))
			tcp->tcp_rcv_head = mp;
		else
			tcp->tcp_rcv_tail->b_cont = mp;
		while (mp->b_cont)
			mp = mp->b_cont;
		tcp->tcp_rcv_tail = mp;
		tcp->tcp_rcv_cnt += seg_len;
		tcp->tcp_rwnd -= seg_len;
		tcph = tcp->tcp_tcph;
		U32_TO_ABE16(tcp->tcp_rwnd >> tcp->tcp_rcv_ws, tcph->th_win);
	} else {
		if (!canputnext(q)) {
			/*
			 * When canputnext fails, we continue to call putnext,
			 * since there is no benefit to holding on to the
			 * data here. However, we begin to shrink the
			 * advertised receive window. When we get back-enabled,
			 * we will reopen the window.
			 */
			tcp->tcp_rwnd -= seg_len;
			U32_TO_ABE16(tcp->tcp_rwnd >> tcp->tcp_rcv_ws,
			    tcp->tcp_tcph->th_win);
		}
		rcv_cnt = tcp->tcp_rcv_cnt;
		if (mp->b_datap->db_type != M_DATA ||
		    (flags & TH_MARKNEXT_NEEDED)) {
			if (rcv_cnt != 0) {
				putnext(q, tcp->tcp_rcv_head);
				tcp->tcp_rcv_head = nilp(mblk_t);
				rcv_cnt = 0;
			}
			ASSERT(tcp->tcp_rcv_head == NULL);
			if (flags & TH_MARKNEXT_NEEDED) {
#ifdef DEBUG
				(void) mi_strlog(tcp->tcp_rq, 1, SL_TRACE,
				    "tcp_rput: sending MSGMARKNEXT %s",
				    tcp_display(tcp));
#endif /* DEBUG */
				mp->b_flag |= MSGMARKNEXT;
				flags &= ~TH_MARKNEXT_NEEDED;
			}
			putnext(q, mp);
			tcp->tcp_co_norm = 1;
		} else if ((flags & (TH_PSH|TH_FIN)) ||
		    rcv_cnt + seg_len >= tcp_rcv_push_wait) {
			if (isput) {
				if (rcv_cnt != 0) {
					tcp->tcp_rcv_tail->b_cont = mp;
					mp = tcp->tcp_rcv_head;
					tcp->tcp_rcv_head = nilp(mblk_t);
					rcv_cnt = 0;
				}
				ASSERT(tcp->tcp_rcv_head == NULL);
				putnext(q, mp);
				tcp->tcp_co_norm = 1;
			} else {
				/*
				 * Processing an mblk from the co queue, just
				 * enqueue it, the caller will do the
				 * push later.
				 */
				if (rcv_cnt != 0) {
					tcp->tcp_rcv_tail->b_cont = mp;
				} else {
					tcp->tcp_rcv_head = mp;
				}
				while (mp->b_cont)
					mp = mp->b_cont;
				tcp->tcp_rcv_tail = mp;
				rcv_cnt += seg_len;
			}
		} else {
			if (rcv_cnt != 0) {
				tcp->tcp_rcv_tail->b_cont = mp;
			} else {
				tcp->tcp_rcv_head = mp;
			}
			while (mp->b_cont)
				mp = mp->b_cont;
			tcp->tcp_rcv_tail = mp;
			rcv_cnt += seg_len;
		}
		tcp->tcp_rcv_cnt = rcv_cnt;
		/*
		 * Make sure the timer is running if we have data waiting
		 * for a push bit. This provides resiliency against
		 * implementations that do not correctly generate push bits.
		 */
		if (rcv_cnt && isput)
			flags |= TH_TIMER_NEEDED;
	}
xmit_check:
	/* Is there anything left to do? */
	ASSERT(!(flags & TH_MARKNEXT_NEEDED));
	if ((flags & (TH_REXMIT_NEEDED|TH_XMIT_NEEDED|TH_ACK_NEEDED|
	    TH_NEED_SACK_REXMIT|TH_TIMER_NEEDED|TH_ORDREL_NEEDED|
	    TH_SEND_URP_MARK)) == 0)
		goto done;

	/* Any transmit work to do and a non-zero window? */
	if ((flags & (TH_REXMIT_NEEDED|TH_XMIT_NEEDED|TH_NEED_SACK_REXMIT)) &&
	    tcp->tcp_swnd != 0) {
		if (flags & TH_REXMIT_NEEDED) {
			uint32_t mss = tcp->tcp_snxt - tcp->tcp_suna;
			if (mss > tcp->tcp_mss)
				mss = tcp->tcp_mss;
			if (mss > tcp->tcp_swnd)
				mss = tcp->tcp_swnd;
			mp1 = tcp_xmit_mp(tcp, tcp->tcp_xmit_head, mss,
			    NULL, NULL, tcp->tcp_suna, 1);
			if (mp1) {
				tcp->tcp_xmit_head->b_prev = (mblk_t *)lbolt;
				tcp->tcp_csuna = tcp->tcp_snxt;
				BUMP_MIB(tcp_mib.tcpRetransSegs);
				UPDATE_MIB(tcp_mib.tcpRetransBytes,
				    msgdsize(mp1->b_cont));
				putnext(tcp->tcp_wq, mp1);
			}
		}
		if (flags & TH_NEED_SACK_REXMIT) {
			if (tcp_sack_rxmit(tcp) != 0) {
				flags |= TH_XMIT_NEEDED;
			}
		}
		if (flags & TH_XMIT_NEEDED)
			tcp_wput_slow(tcp, NULL);

		/* Anything more to do? */
		if ((flags & (TH_ACK_NEEDED|TH_TIMER_NEEDED|
		    TH_ORDREL_NEEDED|TH_SEND_URP_MARK)) == 0)
			goto done;
	}
ack_check:
	if (flags & TH_SEND_URP_MARK) {
		ASSERT(tcp->tcp_urp_mark_mp);
		/*
		 * Send up any queued data and then send the mark message
		 */

		if (tcp->tcp_rcv_cnt != 0) {
			putnext(q, tcp->tcp_rcv_head);
			tcp->tcp_rcv_head = nilp(mblk_t);
			tcp->tcp_rcv_cnt = 0;
		}
		ASSERT(tcp->tcp_rcv_head == NULL);

		mp1 = tcp->tcp_urp_mark_mp;
		tcp->tcp_urp_mark_mp = NULL;
#ifdef DEBUG
		(void) mi_strlog(tcp->tcp_rq, 1, SL_TRACE,
		    "tcp_rput: sending zero-length %s %s",
		    ((mp1->b_flag & MSGMARKNEXT) ? "MSGMARKNEXT" :
		    "MSGNOTMARKNEXT"),
		    tcp_display(tcp));
#endif /* DEBUG */
		putnext(q, mp1);
		flags &= ~TH_SEND_URP_MARK;
	}
	if (flags & TH_ACK_NEEDED) {
		/*
		 * Time to send an ack for some reason.
		 */
		mp1 = tcp_ack_mp(tcp);

		if (mp1) {
			putnext(tcp->tcp_wq, mp1);
			BUMP_LOCAL(tcp->tcp_obsegs);
			BUMP_MIB(tcp_mib.tcpOutAck);
		}
	}
	if (flags & TH_TIMER_NEEDED) {
		/*
		 * Arrange for deferred ACK or push wait timeout.
		 * Start timer if it is not already running.
		 */
		if (tcp->tcp_ack_timer_running == 0) {
			mi_timer(tcp->tcp_wq, tcp->tcp_ack_mp,
			    (clock_t)tcp_deferred_ack_interval);
			tcp->tcp_ack_timer_running = 1;
		}
		/* Record the time when the delayed timer is set. */
		if (tcp->tcp_rnxt == tcp->tcp_rack + seg_len) {
			tcp->tcp_dack_set_time = lbolt;
		}
	}
	if (flags & TH_ORDREL_NEEDED) {
		/*
		 * Send up the ordrel_ind unless we are an eager guy.
		 * In the eager case tcp_rsrv will do this when run
		 * after tcp_accept is done.
		 */
		ASSERT(tcp->tcp_listener == NULL);
		if ((mp = tcp->tcp_rcv_head) != nilp(mblk_t)) {
			/*
			 * Push any mblk(s) enqueued from co processing.
			 */
			tcp->tcp_rcv_head = nilp(mblk_t);
			tcp->tcp_rcv_cnt = 0;
			putnext(q, mp);
			tcp->tcp_co_norm = 1;
		}
		ASSERT(tcp->tcp_rcv_head == NULL);
		mp1 = mi_tpi_ordrel_ind();
		if (mp1) {
			tcp->tcp_ordrel_done = true;
			putnext(q, mp1);
			tcp->tcp_co_norm = 1;
			if (tcp->tcp_deferred_clean_death) {
				/*
				 * tcp_clean_death was deferred
				 * for T_ORDREL_IND - do it now
				 */
				(void) tcp_clean_death(tcp,
				    tcp->tcp_client_errno);
				tcp->tcp_deferred_clean_death =	false;
			}
		} else {
			/*
			 * Run the orderly release in the
			 * service routine.
			 */
			qenable(q);
			/*
			 * Caveat(XXX): The machine may be so
			 * overloaded that tcp_rsrv() is not scheduled
			 * until after the endpoint has transitioned
			 * to TCPS_TIME_WAIT
			 * and tcp_close_wait_interval expires. Then
			 * tcp_timer() will blow away state in tcp_t
			 * and T_ORDREL_IND will never be delivered
			 * upstream. Unlikely but potentially
			 * a problem.
			 */
		}
	}
done:
	ASSERT(!(flags & TH_MARKNEXT_NEEDED));
	TRACE_1(TR_FAC_TCP, TR_TCP_RPUT_OUT, "tcp_rput end:  q %p", q);
}

/*
 * Handle a *T_BIND_REQ that has failed either due to a T_ERROR_ACK
 * or a "bad" IRE detected by tcp_adapt.
 */
static void
tcp_bind_failed(queue_t *q, mblk_t *mp, int error)
{
	tcp_t *tcp = q->q_ptr;
	tcph_t	*tcph;
	struct T_error_ack *tea;

	ASSERT(mp->b_datap->db_type == M_PCPROTO);

	if (mp->b_cont) {
		freemsg(mp->b_cont);
		mp->b_cont = nilp(mblk_t);
	}
	tea = (struct T_error_ack *)mp->b_rptr;
	switch (tea->PRIM_type) {
	case T_BIND_ACK:
		/*
		 * Need to unbind with IP since we were just told that our
		 * bind succeeded.
		 */
		tcp->tcp_hard_bound = false;
		tcp->tcp_hard_binding = false;
		(void) tcp_ip_unbind(tcp->tcp_wq);
		/* Reuse the mblk if possible */
		ASSERT(mp->b_datap->db_lim - mp->b_datap->db_base >=
			sizeof (*tea));
		mp->b_rptr = mp->b_datap->db_base;
		mp->b_wptr = mp->b_rptr + sizeof (*tea);
		tea = (struct T_error_ack *)mp->b_rptr;
		tea->PRIM_type = T_ERROR_ACK;
		tea->TLI_error = TSYSERR;
		tea->UNIX_error = error;
		if (tcp->tcp_state >= TCPS_SYN_SENT) {
			tea->ERROR_prim = T_CONN_REQ;
		} else {
			tea->ERROR_prim = O_T_BIND_REQ;	/* XXX */
		}
		break;

	case T_ERROR_ACK:
		if (tcp->tcp_state >= TCPS_SYN_SENT)
			tea->ERROR_prim = T_CONN_REQ;
		break;
	default:
		cmn_err(CE_PANIC, "tcp_bind_failed: unexpected TPI type\n");
		break;
	}

	tcp->tcp_state = TCPS_IDLE;
	tcp->tcp_ipha.ipha_src = 0;

	tcph = tcp->tcp_tcph;
	tcph->th_lport[0] = 0;
	tcph->th_lport[1] = 0;
	tcp->tcp_lport = 0;
	/* blow away saved option results if any */
	if (tcp->tcp_conn.tcp_opts_conn_req != NULL)
		tcp_close_mpp(&tcp->tcp_conn.tcp_opts_conn_req);

	putnext(q, mp);
}
/*
 * tcp_rput_other is called by tcp_rput to handle everything other than M_DATA
 * messages.
 */
static void
tcp_rput_other(queue_t *q, mblk_t *mp)
{
	mblk_t	*mp1;
	u_char	*rptr = mp->b_rptr;
	tcp_t	*tcp = (tcp_t *)q->q_ptr;
	struct T_error_ack *tea;

	switch (mp->b_datap->db_type) {
	case M_PROTO:
	case M_PCPROTO:
		ASSERT((uintptr_t)(mp->b_wptr - rptr) <= (uintptr_t)INT_MAX);
		if ((mp->b_wptr - rptr) < sizeof (t_scalar_t))
			break;
		tea = (struct T_error_ack *)rptr;
		switch (tea->PRIM_type) {
		case T_BIND_ACK:
			if (tcp->tcp_hard_binding) {
				tcp->tcp_hard_binding = false;
				tcp->tcp_hard_bound = true;
			}
			if (mp1 = tcp_ire_mp(mp)) {
				if (!tcp_adapt(tcp, mp1)) {
					tcp_bind_failed(q, mp,
					    (int)((tcp->tcp_state >=
						TCPS_SYN_SENT) ?
						ENETUNREACH : EADDRNOTAVAIL));
					tcp->tcp_co_norm = 1;
					return;
				}
				if ((tcp->tcp_ipha.ipha_dst ==
				    tcp->tcp_ipha.ipha_src) &&
				    (BE16_EQL(tcp->tcp_tcph->th_lport,
				    tcp->tcp_tcph->th_fport))) {
				    tcp_bind_failed(q, mp, (int)EADDRNOTAVAIL);
				    tcp->tcp_co_norm = 1;
				    return;
				}
				if (tcp->tcp_state == TCPS_SYN_SENT) {
					mblk_t *syn_mp;

					/*
					 * We need to adjust tcp_mss.
					 * The original tcp_mss should be equal
					 * to max interface mss here after
					 * tcp_adapt().  If we are using any
					 * options, say timestamp option, we
					 * need to subtract the length of
					 * those options from tcp_mss as
					 * tcp_mss should be equal to the
					 * max lenght of user data.  TCP
					 * options are not part of user data.
					 */
					if (tcp_tstamp_always ||
					    (tcp->tcp_rcv_ws &&
					    tcp_tstamp_if_wscale)) {
						/*
						 * For timestamp option, we
						 * take out TCPOPT_REAL_TS_LEN.
						 */
						tcp->tcp_mss -=
						    TCPOPT_REAL_TS_LEN;
					}
					TCP_TIMER_RESTART(tcp, tcp->tcp_rto);
					syn_mp = tcp_xmit_mp(tcp, nilp(mblk_t),
					    0, NULL, NULL, tcp->tcp_iss, 0);
					if (syn_mp) {
						putnext(tcp->tcp_wq,
						    syn_mp);
					}
				}
			}
			/*
			 * A trailer mblk indicates a waiting client upstream.
			 * We complete here the processing begun in
			 * either tcp_bind() or tcp_connect() by passing
			 * upstream the reply message they supplied.
			 */
			mp1 = mp;
			mp = mp->b_cont;
			freeb(mp1);
			if (mp)
				break;
			return;
		case T_ERROR_ACK:
			(void) mi_strlog(q, 1, SL_TRACE|SL_ERROR,
			    "tcp_rput_other: case T_ERROR_ACK, "
			    "ERROR_prim == %d",
			    tea->ERROR_prim);
			if (tea->ERROR_prim == O_T_BIND_REQ ||
			    tea->ERROR_prim == T_BIND_REQ) {
				tcp_bind_failed(q, mp,
				    (int)((tcp->tcp_state >= TCPS_SYN_SENT) ?
				    ENETUNREACH : EADDRNOTAVAIL));
				tcp->tcp_co_norm = 1;
				return;
			}
			if (tea->ERROR_prim == T_UNBIND_REQ) {
				tcp->tcp_hard_binding = false;
				tcp->tcp_hard_bound = false;
				if (tcp->tcp_unbind_pending)
					tcp->tcp_unbind_pending = 0;
				else {
					/* From tcp_ip_unbind() - free */
					freemsg(mp);
					return;
				}
			}
			break;
		case T_OK_ACK: {
			struct T_ok_ack *toa;

			ASSERT((uintptr_t)(mp->b_wptr - rptr) <=
			    (uintptr_t)INT_MAX);
			if ((mp->b_wptr - rptr) < sizeof (t_scalar_t))
				break;
			toa = (struct T_ok_ack *)rptr;

			if (toa->CORRECT_prim == T_UNBIND_REQ) {
				tcp->tcp_hard_binding = false;
				tcp->tcp_hard_bound = false;
				if (tcp->tcp_unbind_pending)
					tcp->tcp_unbind_pending = 0;
				else {
					/* From tcp_ip_unbind() - free */
					freemsg(mp);
					return;
				}
			}
			break;
		}
		case O_T_BIND_REQ:
			/*
			 * Tail end of tcp_accept processing.
			 * tcp_accept does all the processing that can be
			 * done without running in the acceptors perimeter and
			 * here the rest are done in the acceptors perimeter.
			 *
			 * Send down the bind_req to ip and adjust mss etc.
			 * Send up any received data and or fin.
			 * Adjust receive window in case it had descreased
			 * while the connection was detached.
			 */
			putnext(tcp->tcp_wq, mp);
			mp = NULL;
			tcp_mss_set(tcp, tcp->tcp_mss);
			/*
			 * This is the first time we run on the correct
			 * queue after tcp_accept. So fix all the q parameters
			 * here.
			 */
			/* Allocate room for SACK options if needed. */
			if (tcp->tcp_snd_sack_ok) {
				(void) mi_set_sth_wroff(tcp->tcp_rq,
				    tcp->tcp_hdr_len + TCPOPT_MAX_SACK_LEN +
				    (tcp->tcp_loopback ? 0 : tcp_wroff_xtra));
			} else {
				(void) mi_set_sth_wroff(tcp->tcp_rq,
				    tcp->tcp_hdr_len +
				    (tcp->tcp_loopback ? 0 : tcp_wroff_xtra));
			}
			if (tcp->tcp_ill_ick.ick_magic == ICK_M_CTL_MAGIC) {
				(void) strqset(tcp->tcp_wq, QSTRUIOT, 0,
				    STRUIOT_STANDARD);
#if 0
				/*
				 * We don't need this because hardware
				 * checksummed mblks won't go through
				 * the synchronous streams interface
				 * (STRUIO_SPEC).
				 */
				(void) strqset(tcp->tcp_rq, QSTRUIOT, 0,
				    STRUIOT_STANDARD);
#endif
				if (strzc_on) {
					ushort	copyopt = 0;
					if ((zerocopy_prop & 1) != 0 &&
					    tcp->tcp_mss >= strzc_minblk)
						copyopt = MAPINOK;
					if ((zerocopy_prop & 2) != 0 &&
					    tcp->tcp_mss >= ptob(1)) {
						/*
						 * We need at least a full page
						 * size to do page flipping.
						 */
						copyopt |= REMAPOK;
					}
					if (copyopt)
						(void) mi_set_sth_copyopt(
						    tcp->tcp_rq, copyopt);
				}
			} else if (tcp->tcp_loopback) {
				(void) strqset(tcp->tcp_wq, QSTRUIOT, 0,
				    STRUIOT_STANDARD);
			}
#ifdef ZC_TEST
			/*
			 * Don't use combined copy/checksum (uioipcopyin/out)
			 * if s/w checksum is disabled.
			 */
			if (noswcksum) {
				(void) strqset(tcp->tcp_wq, QSTRUIOT, 0,
				    STRUIOT_STANDARD);
				(void) strqset(tcp->tcp_rq, QSTRUIOT, 0,
			    STRU	IOT_STANDARD);
			}
#endif
			if (canputnext(q)) {
				uint32_t rwnd = tcp->tcp_rwnd;
				tcp->tcp_rwnd = tcp->tcp_rwnd_max;
				U32_TO_ABE16(tcp->tcp_rwnd >> tcp->tcp_rcv_ws,
				    tcp->tcp_tcph->th_win);
				if (tcp->tcp_state >= TCPS_ESTABLISHED &&
				    (rwnd <= tcp->tcp_rwnd_max >> 1 ||
				    rwnd < tcp->tcp_mss)) {
					tcp_xmit_ctl(nilp(char), tcp,
					    nilp(mblk_t),
					    (tcp->tcp_swnd == 0) ?
					    tcp->tcp_suna :
					    tcp->tcp_snxt,
					    tcp->tcp_rnxt, TH_ACK);
					BUMP_MIB(tcp_mib.tcpOutWinUpdate);
				}
			}
			/*
			 * Pass up any data and/or a fin that has been
			 * received.
			 */
			if ((mp = tcp->tcp_rcv_head) != nilp(mblk_t)) {
				putnext(q, mp);
				tcp->tcp_co_norm = 1;
				tcp->tcp_rcv_head = nilp(mblk_t);
				tcp->tcp_rcv_cnt = 0;
			}
			if (tcp->tcp_fin_rcvd && !tcp->tcp_ordrel_done) {
				mp = mi_tpi_ordrel_ind();
				if (mp) {
					tcp->tcp_ordrel_done = true;
					putnext(q, mp);
					tcp->tcp_co_norm = 1;
					if (tcp->tcp_deferred_clean_death) {
						/*
						 * tcp_clean_death was deferred
						 * for T_ORDREL_IND - do it now
						 */
						(void) tcp_clean_death(tcp,
						    tcp->tcp_client_errno);
						tcp->tcp_deferred_clean_death =
						    false;
					}
				} else {
					/*
					 * Run the orderly release in the
					 * service routine.
					 */
					qenable(q);
				}
			}
			return;
		default:
			break;
		}
		break;
	case M_CTL:
		/*
		 * ICMP messages.
		 * tcp_icmp_error passes the message back in on the correct
		 * queue if it arrived on the wrong one.
		 */
		tcp_icmp_error(q, mp);
		return;
	case M_FLUSH:
		if (*rptr & FLUSHR)
			flushq(q, FLUSHDATA);
		break;
	default:
		break;
	}
	/*
	 * Make sure we set this bit before sending the ACK for
	 * bind. Otherwise  accept could possibly run and free
	 * this tcp struct.
	 */
	tcp->tcp_co_norm = 1;
	putnext(q, mp);
}

/*
 * Called as the result of a qbufcall or a qtimeout to remedy a failure
 * to allocate a T_ordrel_ind in tcp_rsrv().  qenable(q) will make
 * tcp_rsrv() try again.
 */
static void
tcp_ordrel_kick(void *arg)
{
	queue_t *q = arg;
	tcp_t	*tcp	= (tcp_t *)q->q_ptr;

	tcp->tcp_ordrelid = 0;
	tcp->tcp_timeout = false;
	qenable(q);
}

/*
 * The read side service routine is called mostly when we get back-enabled as a
 * result of flow control relief.  Since we don't actually queue anything in
 * TCP, we have no data to send out of here.  What we do is clear the receive
 * window, and send out a window update.
 * This routine is also called to drive an orderly release message upstream
 * if the attempt in tcp_rput failed.
 */
static void
tcp_rsrv(queue_t *q)
{
	tcp_t	*tcp = (tcp_t *)q->q_ptr;
	mblk_t	*mp;

	TRACE_1(TR_FAC_TCP, TR_TCP_RSRV_IN, "tcp_rsrv start:  q %p", q);

	/* No code does a putq on the read side */
	ASSERT(q->q_first == NULL);

	/* Nothing to do for the default queue */
	if (q == tcp_g_q) {
		TRACE_1(TR_FAC_TCP, TR_TCP_RSRV_OUT, "tcp_rsrv end:  q %p",
		    q);
		return;
	}

	if (canputnext(q)) {
		uint32_t rwnd = tcp->tcp_rwnd;
		tcp->tcp_rwnd = tcp->tcp_rwnd_max;
		U32_TO_ABE16(tcp->tcp_rwnd >> tcp->tcp_rcv_ws,
		    tcp->tcp_tcph->th_win);
		if (tcp->tcp_state >= TCPS_ESTABLISHED &&
		    (rwnd <= tcp->tcp_rwnd_max >> 1 ||
			rwnd < tcp->tcp_mss)) {
			tcp_xmit_ctl(nilp(char), tcp, nilp(mblk_t),
			    (tcp->tcp_swnd == 0) ? tcp->tcp_suna :
			    tcp->tcp_snxt, tcp->tcp_rnxt, TH_ACK);
			BUMP_MIB(tcp_mib.tcpOutWinUpdate);
		}
	}
	/* Handle a failure to allocate a T_ORDREL_IND here */
	if (tcp->tcp_fin_rcvd && !tcp->tcp_ordrel_done) {
		ASSERT(tcp->tcp_listener == NULL);
		if ((mp = tcp->tcp_rcv_head) != nilp(mblk_t)) {
			putnext(q, mp);
			tcp->tcp_co_norm = 1;
			tcp->tcp_rcv_head = nilp(mblk_t);
			tcp->tcp_rcv_cnt = 0;
		}
		mp = mi_tpi_ordrel_ind();
		if (mp) {
			tcp->tcp_ordrel_done = true;
			putnext(q, mp);
			tcp->tcp_co_norm = 1;
			if (tcp->tcp_deferred_clean_death) {
				/*
				 * tcp_clean_death was deferred for
				 * T_ORDREL_IND - do it now
				 */
				(void) tcp_clean_death(tcp,
				    tcp->tcp_client_errno);
				tcp->tcp_deferred_clean_death = false;
			}
		} else if (!tcp->tcp_timeout && tcp->tcp_ordrelid == 0) {
			/*
			 * If there isn't already a qbufcall/qtimeout running
			 * start one.  If qbufcall() fails use a 4 second
			 * qtimeout() as a fallback since it can't fail.
			 */
			ASSERT(!TCP_IS_DETACHED(tcp));

			if ((tcp->tcp_ordrelid = qbufcall(q,
			    sizeof (struct T_ordrel_ind), BPRI_HI,
			    tcp_ordrel_kick, q)) == 0) {
				tcp->tcp_timeout = true;
				tcp->tcp_ordrelid = qtimeout(q, tcp_ordrel_kick,
				    q, MSEC_TO_TICK(4000));
			}
		}
	}
	TRACE_1(TR_FAC_TCP, TR_TCP_RSRV_OUT, "tcp_rsrv end:  q %p", q);
}

/*
 * tcp_rwnd_set is called to adjust the receive window to a desired value.
 * We do not allow the receive window to shrink.  If the requested value is
 * not a multiple of the current mss, we round it up to the next higher
 * multiple of mss.
 *
 * This function is called before data transfer begins through
 * tcp_mss_set() to make sure rwnd is always an integer multiple of mss.
 * The default rwnd size is set to (see tcp_mss_set())
 *
 *	MAX(tcp->tcp_rq->q_hiwat, tcp_recv_hiwat_minmss * mss)
 *
 * It can be altered (only increased) through SO_RCVBUF/SO_SNDBUF options.
 *
 * XXX - Should allow a lower rwnd than tcp_recv_hiwat_minmss * mss if the
 * user requests so.
 */
static int
tcp_rwnd_set(tcp_t *tcp, uint32_t rwnd)
{
	uint32_t	mss = tcp->tcp_mss;
	uint32_t	old_max_rwnd = tcp->tcp_rwnd_max;
	uint32_t	max_transmittable_rwnd;

	/* Insist on a receive window that is a multiple of mss. */
	rwnd = (((rwnd - 1) / mss) + 1) * mss;
	/* Monotonically increasing tcp_rwnd ensures no reneg */
	if (rwnd < old_max_rwnd)
		rwnd = (((old_max_rwnd - 1) / mss) + 1) * mss;

	/*
	 * If we're far enough into the connection that we could have sent or
	 * received a window scale option, then make sure new window can be
	 * legally transmitted, taking window scale into account.
	 */

	max_transmittable_rwnd = TCP_MAXWIN << tcp->tcp_rcv_ws;
	if (rwnd > max_transmittable_rwnd) {
		rwnd = max_transmittable_rwnd -
		    (max_transmittable_rwnd % mss);
		if (rwnd < mss)
			rwnd = max_transmittable_rwnd;
		/*
		 * We set all three so that the increment below has no
		 * effect.
		 */

		tcp->tcp_rwnd = old_max_rwnd = rwnd;
	}
	if (tcp->tcp_localnet) {
		tcp->tcp_rack_abs_max =
		    MIN(tcp_deferred_acks_max, rwnd / mss / 2) * mss;
	} else {
		/*
		 * For a remote host on a different subnet (through a router),
		 * we ack every other packet to be conforming to RFC1122.
		 */
		tcp->tcp_rack_abs_max = MIN(tcp_deferred_acks_max, 2) * mss;
	}
	if (tcp->tcp_rack_cur_max > tcp->tcp_rack_abs_max)
		tcp->tcp_rack_cur_max = tcp->tcp_rack_abs_max;
	else
		tcp->tcp_rack_cur_max = 0;
	/*
	 * Increment the current rwnd by the amount the maximum grew (we
	 * can not overwrite it since we might be in the middle of a
	 * connection.)
	 */
	tcp->tcp_rwnd += rwnd - old_max_rwnd;

	U32_TO_ABE16(tcp->tcp_rwnd >> tcp->tcp_rcv_ws, tcp->tcp_tcph->th_win);
	if ((tcp->tcp_rcv_ws > 0) && rwnd > tcp->tcp_cwnd_max)
		tcp->tcp_cwnd_max = rwnd;

	tcp->tcp_rwnd_max = rwnd;
	if (TCP_IS_DETACHED(tcp))
		return (rwnd);
	/*
	 * We set the maximum receive window into rq->q_hiwat.
	 * This is not actually used for flow control.
	 */
	tcp->tcp_rq->q_hiwat = rwnd;
	/*
	 * Set the Stream head high water mark. This doesn't have to be
	 * here, since we are simply using default values, but we would
	 * prefer to choose these values algorithmically, with a likely
	 * relationship to rwnd.
	 */
	(void) mi_set_sth_hiwat(tcp->tcp_rq, MAX(rwnd, tcp_sth_rcv_hiwat));
	return (rwnd);
}

/* Return SNMP stuff in buffer in mpdata. */
static int
tcp_snmp_get(queue_t *q, mblk_t *mpctl)
{
	mblk_t			*mpdata;
	mblk_t			*mp2ctl;
	mblk_t			*mp2data;
	mblk_t			*mp_tail = NULL;
	struct opthdr		*optp;
	IDP			idp;
	tcp_t			*tcp;
	mib2_tcpConnEntry_t	tce;
	boolean_t ispriv;

	if (!mpctl || !(mpdata = mpctl->b_cont) || !(mp2ctl = copymsg(mpctl)))
		return (0);

	/* build table of connections -- need count in fixed part */
	mp2data = mp2ctl->b_cont;
	SET_MIB(tcp_mib.tcpRtoAlgorithm, 4);   /* vanj */
	SET_MIB(tcp_mib.tcpRtoMin, tcp_rexmit_interval_min);
	SET_MIB(tcp_mib.tcpRtoMax, tcp_rexmit_interval_max);
	SET_MIB(tcp_mib.tcpMaxConn, -1);
	SET_MIB(tcp_mib.tcpCurrEstab, 0);

	tcp = (tcp_t *)q->q_ptr;
	ispriv = tcp->tcp_priv_stream;

	mutex_enter(&tcp_mi_lock);
	idp = mi_first_ptr(&tcp_g_head);
	while ((tcp = (tcp_t *)idp) != NULL) {
		TCP_REFHOLD(tcp);
		mutex_exit(&tcp_mi_lock);
		UPDATE_MIB(tcp_mib.tcpInSegs, tcp->tcp_ibsegs);
		tcp->tcp_ibsegs = 0;
		UPDATE_MIB(tcp_mib.tcpOutSegs, tcp->tcp_obsegs);
		tcp->tcp_obsegs = 0;
		tce.tcpConnState = tcp_snmp_state(tcp);
		if (tce.tcpConnState == MIB2_TCP_established ||
		    tce.tcpConnState == MIB2_TCP_closeWait)
			BUMP_MIB(tcp_mib.tcpCurrEstab);
		tce.tcpConnLocalAddress = tcp->tcp_ipha.ipha_src;
		tce.tcpConnLocalPort = ntohs(tcp->tcp_lport);
		tce.tcpConnRemAddress = tcp->tcp_remote;
		tce.tcpConnRemPort = ntohs(tcp->tcp_fport);

		/* Don't want just anybody seeing these... */
		if (ispriv) {
			tce.tcpConnEntryInfo.ce_snxt = tcp->tcp_snxt;
			tce.tcpConnEntryInfo.ce_suna = tcp->tcp_suna;
			tce.tcpConnEntryInfo.ce_rnxt = tcp->tcp_rnxt;
			tce.tcpConnEntryInfo.ce_rack = tcp->tcp_rack;
		} else {
			/*
			 * Netstat, unfortunately, uses this to
			 * get send/receive queue sizes.  How to fix?
			 * Why not compute the difference only?
			 */
			tce.tcpConnEntryInfo.ce_snxt =
			    tcp->tcp_snxt - tcp->tcp_suna;
			tce.tcpConnEntryInfo.ce_suna = 0;
			tce.tcpConnEntryInfo.ce_rnxt =
			    tcp->tcp_rnxt - tcp->tcp_rack;
			tce.tcpConnEntryInfo.ce_rack = 0;
		}

		tce.tcpConnEntryInfo.ce_swnd = tcp->tcp_swnd;
		tce.tcpConnEntryInfo.ce_rwnd = tcp->tcp_rwnd;
		tce.tcpConnEntryInfo.ce_rto =  tcp->tcp_rto;
		tce.tcpConnEntryInfo.ce_mss =  tcp->tcp_mss;
		tce.tcpConnEntryInfo.ce_state = tcp->tcp_state;
		(void) snmp_append_data2(mp2data, &mp_tail, (char *)&tce,
		    sizeof (tce));
		mutex_enter(&tcp_mi_lock);
		idp = mi_next_ptr(&tcp_g_head, idp);
		TCP_REFRELE(tcp);
	}
	mutex_exit(&tcp_mi_lock);
	/* fixed length structure... */
	SET_MIB(tcp_mib.tcpConnTableSize, sizeof (mib2_tcpConnEntry_t));
	optp = (struct opthdr *)&mpctl->b_rptr[sizeof (struct T_optmgmt_ack)];
	optp->level = MIB2_TCP;
	optp->name = 0;
	(void) snmp_append_data(mpdata, (char *)&tcp_mib, sizeof (tcp_mib));
	optp->len = msgdsize(mpdata);
	qreply(q, mpctl);

	/* table of connections... */
	optp = (struct opthdr *)&mp2ctl->b_rptr[sizeof (struct T_optmgmt_ack)];
	optp->level = MIB2_TCP;
	optp->name = MIB2_TCP_13;
	optp->len = msgdsize(mp2data);
	qreply(q, mp2ctl);
	return (1);
}

/* Return 0 if invalid set request, 1 otherwise, including non-tcp requests  */
/* ARGSUSED */
static	int
tcp_snmp_set(queue_t *q, int level, int name, u_char *ptr, int len)
{
	mib2_tcpConnEntry_t	*tce = (mib2_tcpConnEntry_t *)ptr;

	switch (level) {
	case MIB2_TCP:
		switch (name) {
		case 13:
			if (tce->tcpConnState != MIB2_TCP_deleteTCB)
				return (0);
			/* TODO: delete entry defined by tce */
			return (1);
		default:
			return (0);
		}
	default:
		return (1);
	}
}

/* Translate TCP state to MIB2 TCP state. */
static int
tcp_snmp_state(tcp_t *tcp)
{
	if (!tcp)
		return (0);
	switch (tcp->tcp_state) {
	case TCPS_CLOSED:
	case TCPS_IDLE:	/* RFC1213 doesn't have analogue for IDLE & BOUND */
	case TCPS_BOUND:
		return (MIB2_TCP_closed);
	case TCPS_LISTEN:
		return (MIB2_TCP_listen);
	case TCPS_SYN_SENT:
		return (MIB2_TCP_synSent);
	case TCPS_SYN_RCVD:
		return (MIB2_TCP_synReceived);
	case TCPS_ESTABLISHED:
		return (MIB2_TCP_established);
	case TCPS_CLOSE_WAIT:
		return (MIB2_TCP_closeWait);
	case TCPS_FIN_WAIT_1:
		return (MIB2_TCP_finWait1);
	case TCPS_CLOSING:
		return (MIB2_TCP_closing);
	case TCPS_LAST_ACK:
		return (MIB2_TCP_lastAck);
	case TCPS_FIN_WAIT_2:
		return (MIB2_TCP_finWait2);
	case TCPS_TIME_WAIT:
		return (MIB2_TCP_timeWait);
	default:
		return (0);
	}
}

static char tcp_report_header[] = "TCP      "
#ifdef _LP64
	"        "
#endif
	"dest            snxt     suna     "
	"swnd       rnxt     rack     rwnd       rto   mss   w sw rw t "
	"recent   [lport,fport] state";

/* TCP status report triggered via the Named Dispatch mechanism. */
/* ARGSUSED */
static	void
tcp_report_item(mblk_t *mp, tcp_t *tcp, int hashval, tcp_t *thisstream)
{
	char hash[10], addrbuf[16];
	boolean_t ispriv = thisstream->tcp_priv_stream;

	if (hashval >= 0)
		(void) sprintf(hash, "%03d ", hashval);
	else
		hash[0] = '\0';

	/*
	 * NOTE: the ispriv checks are so that normal users cannot determine
	 *	 sequence number information using NDD.
	 */

	(void) mi_mpprintf(mp, "%s%08lx %s %08x %08x %010d %08x %08x "
	    "%010d %05d %05d %1d %02d %02d %1d %08x %s%c",
	    hash,
	    tcp,
	    tcp_addr_sprintf(addrbuf, (u_char *)&tcp->tcp_iph.iph_dst),
	    (ispriv)?tcp->tcp_snxt:0,
	    (ispriv)?tcp->tcp_suna:0,
	    tcp->tcp_swnd,
	    (ispriv)?tcp->tcp_rnxt:0,
	    (ispriv)?tcp->tcp_rack:0,
	    tcp->tcp_rwnd, tcp->tcp_rto,
	    tcp->tcp_mss,
	    tcp->tcp_snd_ws_ok,
	    tcp->tcp_snd_ws,
	    tcp->tcp_rcv_ws,
	    tcp->tcp_snd_ts_ok,
	    tcp->tcp_ts_recent,
	    tcp_display(tcp), TCP_IS_DETACHED(tcp) ? '*' : ' ');
}

/*
 * TCP status report (for listeners only) triggered via the Named Dispatch
 * mechanism.
 */
/* ARGSUSED */
static	void
tcp_report_listener(mblk_t *mp, tcp_t *tcp, int hashval)
{
	char addrbuf[16];

	(void) mi_mpprintf(mp, "%03d %0lx %s %05u %08u %d/%d/%d%c",
	    hashval, tcp,
	    tcp_addr_sprintf(addrbuf, (uint8_t *)&tcp->tcp_iph.iph_src),
	    (u_int)BE16_TO_U16(tcp->tcp_tcph->th_lport),
	    tcp->tcp_conn_req_seqnum,
	    tcp->tcp_conn_req_cnt_q0, tcp->tcp_conn_req_cnt_q,
	    tcp->tcp_conn_req_max,
	    tcp->tcp_syn_defense ? '*' : ' ');
}

/* TCP status report triggered via the Named Dispatch mechanism. */
/* ARGSUSED */
static	int
tcp_status_report(queue_t *q, mblk_t *mp, caddr_t cp)
{
	IDP	idp;
	tcp_t	*tcp;

	(void) mi_mpprintf(mp, "%s", tcp_report_header);

	mutex_enter(&tcp_mi_lock);
	idp = mi_first_ptr(&tcp_g_head);
	while ((tcp = (tcp_t *)idp) != NULL) {
		TCP_REFHOLD(tcp);
		mutex_exit(&tcp_mi_lock);
		tcp_report_item(mp, tcp, -1, (tcp_t *)q->q_ptr);
		mutex_enter(&tcp_mi_lock);
		idp = mi_next_ptr(&tcp_g_head, idp);
		TCP_REFRELE(tcp);
	}
	mutex_exit(&tcp_mi_lock);
	return (0);
}

/* TCP status report triggered via the Named Dispatch mechanism. */
/* ARGSUSED */
static	int
tcp_bind_hash_report(queue_t *q, mblk_t *mp, caddr_t cp)
{
	tf_t	*tf;
	tcp_t	*tcp;
	int	i;

	(void) mi_mpprintf(mp, "    %s", tcp_report_header);

	for (i = 0; i < A_CNT(tcp_bind_fanout); i++) {
		tf = &tcp_bind_fanout[i];
		mutex_enter(&tf->tf_lock);
		for (tcp = tf->tf_tcp; tcp != NULL; tcp = tcp->tcp_bind_hash)
			tcp_report_item(mp, tcp, i, (tcp_t *)q->q_ptr);
		mutex_exit(&tf->tf_lock);
	}
	return (0);
}

/* TCP status report triggered via the Named Dispatch mechanism. */
/* ARGSUSED */
static	int
tcp_listen_hash_report(queue_t *q, mblk_t *mp, caddr_t cp)
{
	tf_t	*tf;
	tcp_t	*tcp;
	int	i;

	(void) mi_mpprintf(mp, "    TCP     "
#ifdef _LP64
	    "        "
#endif
	    " IP addr         port  seqnum   backlog (q0/q/max)");

	for (i = 0; i < A_CNT(tcp_listen_fanout); i++) {
		tf = &tcp_listen_fanout[i];
		mutex_enter(&tf->tf_lock);
		for (tcp = tf->tf_tcp; tcp != NULL;
		    tcp = tcp->tcp_listen_hash)
			tcp_report_listener(mp, tcp, i);
		mutex_exit(&tf->tf_lock);
	}
	return (0);
}

/* TCP status report triggered via the Named Dispatch mechanism. */
/* ARGSUSED */
static	int
tcp_conn_hash_report(queue_t *q, mblk_t *mp, caddr_t cp)
{
	tf_t	*tf;
	tcp_t	*tcp;
	int	i;

	(void) mi_mpprintf(mp, "tcp_conn_hash_size = %d", tcp_conn_hash_size);
	(void) mi_mpprintf(mp, "    %s", tcp_report_header);

	for (i = 0; i < tcp_conn_fanout_size; i++) {
		tf = &tcp_conn_fanout[i];
		mutex_enter(&tf->tf_lock);
		for (tcp = tf->tf_tcp; tcp != NULL; tcp = tcp->tcp_conn_hash) {
			tcp_report_item(mp, tcp, i, (tcp_t *)q->q_ptr);
		}
		mutex_exit(&tf->tf_lock);
	}
	return (0);
}

/* TCP status report triggered via the Named Dispatch mechanism. */
/* ARGSUSED */
static	int
tcp_acceptor_hash_report(queue_t *q, mblk_t *mp, caddr_t cp)
{
	tf_t	*tf;
	tcp_t	*tcp;
	int	i;

	(void) mi_mpprintf(mp, "    %s", tcp_report_header);

	for (i = 0; i < A_CNT(tcp_acceptor_fanout); i++) {
		tf = &tcp_acceptor_fanout[i];
		mutex_enter(&tf->tf_lock);
		for (tcp = tf->tf_tcp; tcp != NULL;
		    tcp = tcp->tcp_acceptor_hash)
			tcp_report_item(mp, tcp, i, (tcp_t *)q->q_ptr);
		mutex_exit(&tf->tf_lock);
	}
	return (0);
}

/*
 * tcp_timer is the timer service routine.  It handles all timer events for
 * a tcp instance except keepalives.  It figures out from the state of the
 * tcp instance what kind of action needs to be done at the time it is called.
 */
static void
tcp_timer(tcp_t	*tcp)
{
	mblk_t		*mp;
	clock_t		first_threshold;
	clock_t		second_threshold;
	clock_t		ms;
	uint32_t	mss;

	first_threshold =  tcp->tcp_first_timer_threshold;
	second_threshold = tcp->tcp_second_timer_threshold;
	switch (tcp->tcp_state) {
	case TCPS_IDLE:
	case TCPS_BOUND:
	case TCPS_LISTEN:
		return;
	case TCPS_SYN_RCVD: {
		tcp_t	*listener = tcp->tcp_listener;

		if (tcp->tcp_syn_rcvd_timeout == 0 && (listener != NULL)) {
			ASSERT(tcp->tcp_rq == listener->tcp_rq);
			/* it's our first timeout */
			tcp->tcp_syn_rcvd_timeout = 1;
			listener->tcp_syn_rcvd_timeout++;
			if (!listener->tcp_syn_defense &&
			    (listener->tcp_syn_rcvd_timeout >
			    (tcp_conn_req_max_q0 >> 2)) &&
			    (tcp_conn_req_max_q0 > 200)) {
				/* We may be under attack. Put on a defense. */
				listener->tcp_syn_defense = true;
				cmn_err(CE_WARN, "High TCP connect timeout "
				    "rate! System (port %d) may be under a "
				    "SYN flood attack!",
				    BE16_TO_U16(listener->tcp_tcph->th_lport));

				listener->tcp_ip_addr_cache = kmem_zalloc(
				    IP_ADDR_CACHE_SIZE * sizeof (ipaddr_t),
				    KM_NOSLEEP);
			}
		}
	}
		/* FALLTHRU */
	case TCPS_SYN_SENT:
		first_threshold =  tcp->tcp_first_ctimer_threshold;
		second_threshold = tcp->tcp_second_ctimer_threshold;
		break;
	case TCPS_ESTABLISHED:
	case TCPS_FIN_WAIT_1:
	case TCPS_CLOSING:
	case TCPS_CLOSE_WAIT:
	case TCPS_LAST_ACK:
		/* If we have data to rexmit */
		if (tcp->tcp_suna != tcp->tcp_snxt) {
			clock_t	time_to_wait;

			BUMP_MIB(tcp_mib.tcpTimRetrans);
			if (!tcp->tcp_xmit_head)
				break;
			time_to_wait = lbolt -
			    (clock_t)tcp->tcp_xmit_head->b_prev;
			time_to_wait = MSEC_TO_TICK(tcp->tcp_rto) -
			    time_to_wait;
			if (time_to_wait > 0) {
				/*
				 * Timer fired too early, so restart it.
				 */
				TCP_TIMER_RESTART(tcp,
				    TICK_TO_MSEC(time_to_wait));
				return;
			}
			/*
			 * When we probe zero windows, we force the swnd open.
			 * If our peer acks with a closed window swnd will be
			 * set to zero by tcp_rput(). As long as we are
			 * receiving acks tcp_rput will
			 * reset 'tcp_ms_we_have_waited' so as not to trip the
			 * first and second interval actions.  NOTE: the timer
			 * interval is allowed to continue its exponential
			 * backoff.
			 */
			if (tcp->tcp_swnd == 0 || tcp->tcp_zero_win_probe) {
				(void) mi_strlog(tcp->tcp_rq, 1, SL_TRACE,
				    "tcp_timer: zero win");
			} else {
				/*
				 * After retransmission, we need to do
				 * slow start.  Set the ssthresh to one
				 * half of current effective window and
				 * cwnd to one MSS.  Also reset
				 * tcp_dupack_cnt and tcp_ss_burst.
				 */
				uint32_t npkt;

				npkt = (MIN((tcp->tcp_timer_backoff ?
				    tcp->tcp_cwnd_ssthresh : tcp->tcp_cwnd),
				    tcp->tcp_swnd) >> 1) / tcp->tcp_mss;
				if (npkt < 2)
					npkt = 2;
				tcp->tcp_cwnd_ssthresh = npkt * tcp->tcp_mss;
				tcp->tcp_cwnd = tcp->tcp_mss;
				tcp->tcp_cwnd_cnt = 0;
				tcp->tcp_dupack_cnt = 0;
			}
			break;
		}
		/* TODO: source quench, sender silly window, ... */
		/* If we have a zero window */
		if (tcp->tcp_unsent && tcp->tcp_swnd == 0) {
			/* Extend window for probe */
			tcp->tcp_swnd += MIN(tcp->tcp_mss,
			    tcp_zero_win_probesize);
			tcp->tcp_zero_win_probe = 1;
			BUMP_MIB(tcp_mib.tcpOutWinProbe);
			tcp_wput_slow(tcp, nilp(mblk_t));
			return;
		}
		/* Handle timeout from sender SWS avoidance. */
		if (tcp->tcp_unsent != 0) {
			/*
			 * Reset our knowledge of the max send window since
			 * the receiver might have reduced its receive buffer.
			 * Avoid setting tcp_max_swnd to one since that
			 * will essentially disable the SWS checks.
			 */
			tcp->tcp_max_swnd = MAX(tcp->tcp_swnd, 2);
			tcp_wput_slow(tcp, nilp(mblk_t));
			return;
		}
		/* Is there a FIN that needs to be to re retransmitted? */
		if ((tcp->tcp_valid_bits & TCP_FSS_VALID) &&
		    !tcp->tcp_fin_acked)
			break;
		/* Nothing to do, return without restarting timer. */
		return;
	case TCPS_FIN_WAIT_2:
		/*
		 * User closed the TCP endpoint and peer ACK'ed our FIN.
		 * We waited some time for for peer's FIN, but it hasn't
		 * arrived.  We flush the connection now to avoid
		 * case where the peer has rebooted.
		 */
		if (TCP_IS_DETACHED(tcp))
			(void) tcp_clean_death(tcp, 0);
		else
			TCP_TIMER_RESTART(tcp, tcp_fin_wait_2_flush_interval);
		return;
	case TCPS_TIME_WAIT:
		ASSERT(!TCP_IS_DETACHED(tcp));
		(void) tcp_clean_death(tcp, 0);
		return;
	default:
		(void) mi_strlog(tcp->tcp_wq, 1, SL_TRACE|SL_ERROR,
		    "tcp_timer: strange state (%d) %s",
		    tcp->tcp_state, tcp_display(tcp));
		return;
	}
	if ((ms = tcp->tcp_ms_we_have_waited) > second_threshold) {
		/*
		 * For zero window probe, we need to send indefinitely,
		 * unless we have not heard from the other side for some
		 * time...
		 */
		if ((tcp->tcp_zero_win_probe == 0) ||
		    (TICK_TO_MSEC(lbolt - tcp->tcp_last_recv_time) >
		    second_threshold)) {
			BUMP_MIB(tcp_mib.tcpTimRetransDrop);
			/*
			 * If TCP is in SYN_RCVD state, send back a
			 * RST|ACK as BSD does.  Note that tcp_zero_win_probe
			 * should be zero in TCPS_SYN_RCVD state.
			 */
			if (tcp->tcp_state == TCPS_SYN_RCVD) {
				tcp_xmit_ctl("tcp_timer: RST sent on timeout "
				    "in SYN_RCVD",
				    tcp, NULL, tcp->tcp_snxt, tcp->tcp_rnxt,
				    TH_RST | TH_ACK);
			}
			(void) tcp_clean_death(tcp, tcp->tcp_client_errno ?
			    tcp->tcp_client_errno : ETIMEDOUT);
			return;
		} else {
			/*
			 * Need to keep tcp_timer_backoff and
			 * tcp_rexmit_time constant to avoid overflow.
			 */
			tcp->tcp_timer_backoff--;
			tcp->tcp_ms_we_have_waited = second_threshold;
		}
	} else if (ms > first_threshold && tcp->tcp_rtt_sa != 0) {
		/*
		 * We have been retransmitting for too long...  The RTT
		 * we calculated is probably incorrect.  Reinitialize it.
		 * Need to compensate for 0 tcp_rtt_sa.  Reset
		 * tcp_rtt_update so that we won't accidentally cache a
		 * bad value.
		 */
		tcp->tcp_rtt_sd += (tcp->tcp_rtt_sa >> 3);
		tcp->tcp_rtt_sa = 0;
		tcp_ip_notify(tcp);
		tcp->tcp_rtt_update = 0;
	}
	tcp->tcp_timer_backoff++;
	if ((ms = (tcp->tcp_rtt_sa >> 3) + tcp->tcp_rtt_sd +
	    tcp_rexmit_interval_extra + (tcp->tcp_rtt_sa >> 5)) <
	    tcp_rexmit_interval_min) {
		/*
		 * This means the original RTO is tcp_rexmit_interval_min.
		 * So we will use tcp_rexmit_interval_min as the RTO value
		 * and do the backoff.
		 */
		ms = tcp_rexmit_interval_min << tcp->tcp_timer_backoff;
	} else {
		ms <<= tcp->tcp_timer_backoff;
	}
	if (ms > tcp_rexmit_interval_max) {
		ms = tcp_rexmit_interval_max;
	}
	tcp->tcp_ms_we_have_waited += ms;
	if (tcp->tcp_zero_win_probe == 0) {
		tcp->tcp_rto = ms;
	}
	TCP_TIMER_RESTART(tcp, ms);
	/*
	 * This is after a timeout and tcp_rto is backed off.  Set
	 * tcp_set_timer to 1 so that next time RTO is updated, we will
	 * restart the timer with a correct value.
	 */
	tcp->tcp_set_timer = 1;
	mss = tcp->tcp_snxt - tcp->tcp_suna;
	if (mss > tcp->tcp_mss)
		mss = tcp->tcp_mss;
	if (mss > tcp->tcp_swnd && tcp->tcp_swnd != 0)
		mss = tcp->tcp_swnd;

	if ((mp = tcp->tcp_xmit_head) != NULL)
		mp->b_prev = (mblk_t *)lbolt;
	mp = tcp_xmit_mp(tcp, mp, mss, NULL, NULL, tcp->tcp_suna, 1);
	if (mp) {
		tcp->tcp_csuna = tcp->tcp_snxt;
		BUMP_MIB(tcp_mib.tcpRetransSegs);
		UPDATE_MIB(tcp_mib.tcpRetransBytes, msgdsize(mp->b_cont));
		putnext(tcp->tcp_wq, mp);
		/*
		 * When slow start after retransmission begins, start with
		 * this seq no.  tcp_rexmit_max marks the end of special slow
		 * start phase.  tcp_snd_burst controls how many segments
		 * can be sent because of an ack.
		 */
		tcp->tcp_rexmit_nxt = tcp->tcp_suna;
		tcp->tcp_snd_burst = TCP_CWND_SS;
		if ((tcp->tcp_valid_bits & TCP_FSS_VALID) &&
		    (tcp->tcp_unsent == 0)) {
			tcp->tcp_rexmit_max = tcp->tcp_snxt - 1;
		} else {
			tcp->tcp_rexmit_max = tcp->tcp_snxt;
		}
		tcp->tcp_rexmit = 1;

		/*
		 * Remove all rexmit SACK blk to start from fresh.
		 */
		if (tcp->tcp_snd_sack_ok && tcp->tcp_notsack_list != NULL) {
			TCP_NOTSACK_REMOVE_ALL(tcp->tcp_notsack_list);
			tcp->tcp_num_notsack_blk = 0;
			tcp->tcp_cnt_notsack_list = 0;
		}
	}
}

/*
 * tcp_timer_alloc is called by tcp_init to allocate and initialize a
 * tcp timer.
 */
static mblk_t *
tcp_timer_alloc(tcp_t *tcp, pfv_t func, int extra)
{
	mblk_t	*mp;

	mp = mi_timer_alloc(sizeof (tcpt_t) + extra);
	if (mp) {
		tcpt_t	*tcpt = (tcpt_t *)mp->b_rptr;
		tcpt->tcpt_tcp = tcp;
		tcpt->tcpt_pfv = func;
	}
	return (mp);
}

/* tcp_unbind is called by tcp_wput_proto to handle T_UNBIND_REQ messages. */
static void
tcp_unbind(queue_t *q, mblk_t *mp)
{
	tcp_t	*tcp;

	tcp = (tcp_t *)q->q_ptr;
	switch (tcp->tcp_state) {
	case TCPS_BOUND:
	case TCPS_LISTEN:
		break;
	default:
		tcp_err_ack(tcp->tcp_wq, mp, TOUTSTATE, 0);
		return;
	}
	bzero(tcp->tcp_iph.iph_src, sizeof (tcp->tcp_iph.iph_src));
	bzero(tcp->tcp_tcph->th_lport, sizeof (tcp->tcp_tcph->th_lport));
	tcp->tcp_lport = 0;
	tcp->tcp_state = TCPS_IDLE;
	tcp->tcp_unbind_pending = 1;
	/* Send M_FLUSH according to TPI */
	(void) putnextctl1(tcp->tcp_rq, M_FLUSH, FLUSHRW);
	/* Pass the unbind to IP */
	putnext(q, mp);
}

/*
 * Don't let port fall into the privileged range.
 * Since the extra privileged ports can be arbitrary we also
 * ensure that we exclude those from consideration.
 * tcp_g_epriv_ports is not sorted thus we loop over it until
 * there are no changes.
 *
 * Note: No locks are held when inspecting tcp_g_*epriv_ports
 * but instead the code relies on:
 * - the fact that the address of the array and its size never changes
 * - the atomic assignment of the elements of the array
 */
static in_port_t
tcp_update_next_port(in_port_t port)
{
	int i;

retry:
	if (port < tcp_smallest_anon_port || port > tcp_largest_anon_port)
		port = tcp_smallest_anon_port;

	if (port < tcp_smallest_nonpriv_port)
		port = tcp_smallest_nonpriv_port;

	for (i = 0; i < tcp_g_num_epriv_ports; i++) {
		if (port == tcp_g_epriv_ports[i]) {
			port++;
			/*
			 * Make sure whether the port is in the
			 * valid range.
			 *
			 * XXX Note that if tcp_g_epriv_ports contains
			 * all the anonymous ports this will be an
			 * infinite loop.
			 */
			goto retry;
		}
	}
	return (port);
}

/* The write side info procedure. */
static int
tcp_winfop(queue_t *q, infod_t *dp)
{
	return (infonext(q, dp));
}

/* The write side r/w procedure. */

#if CCS_STATS
struct {
	struct {
		int64_t count, bytes;
	} tot, hit;
} wrw_stats;
#endif

static int
tcp_wrw(queue_t *q, struiod_t *dp)
{
	mblk_t	*mp = dp->d_mp;
	int	error;

	if (isuioq(q) && (error = struioget(q, mp, dp, 1)))
		/*
		 * Uio error of some sort, so just return the error.
		 */
		return (error);
	/*
	 * Pass the mblk (chain) onto wput(), then return success.
	 */
	dp->d_mp = 0;
#if CCS_STATS
	wrw_stats.hit.count++;
	wrw_stats.hit.bytes += msgdsize(mp);
#endif
	tcp_wput(q, mp);
	return (0);
}

/*
 * The TCP fast path write put procedure.
 * NOTE: the logic of the fast path is duplicated from tcp_wput_slow()
 */
static void
tcp_wput(queue_t *q, mblk_t *mp)
{
	tcp_t		*tcp;
	int		len;
	int		hdrlen;
	mblk_t		*mp1;
	u_char		*rptr;
	uint32_t	snxt;
	tcph_t		*tcph;
	struct datab 	*db;
	uint32_t	suna;
	uint32_t	mss;

	TRACE_3(TR_FAC_TCP, TR_TCP_WPUT_IN,
		"tcp_wput start:  q %p db_type 0%o tcp %p",
	    q, mp->b_datap->db_type, q->q_ptr);

	tcp = (tcp_t *)q->q_ptr;
	mss = tcp->tcp_mss;
	ASSERT((uintptr_t)(mp->b_wptr - mp->b_rptr) <= (uintptr_t)INT_MAX);
	len = (int)(mp->b_wptr - mp->b_rptr);

	/*
	 * Criteria for fast path:
	 *
	 *   1. mblk type is M_DATA
	 *   2. single mblk in request
	 *   3. connection established
	 *   4. no unsent data
	 *   5. data in mblk
	 *   6. len <= mss
	 *   7. no tcp_valid bits
	 */

	if ((mp->b_datap->db_type == M_DATA) &&
	    (mp->b_cont == 0) &&
	    (tcp->tcp_state == TCPS_ESTABLISHED) &&
	    (tcp->tcp_unsent == 0) &&
	    (len > 0) &&
	    (len <= mss) &&
	    (tcp->tcp_valid_bits == 0)) {

		ASSERT(tcp->tcp_xmit_tail_unsent == 0);
		ASSERT(tcp->tcp_fin_sent == 0);

		/* queue new packet onto retransmission queue */
		if (tcp->tcp_xmit_head == 0) {
			tcp->tcp_xmit_head = mp;
		} else {
			tcp->tcp_xmit_last->b_cont = mp;
		}
		tcp->tcp_xmit_last = mp;
		tcp->tcp_xmit_tail = mp;

		/* find out how much we can send */
		/* BEGIN CSTYLED */
		/*
		 *    un-acked           usable
		 *  |--------------|-----------------|
		 *  tcp_suna       tcp_snxt          tcp_suna+tcp_swnd
		 */
		/* END CSTYLED */

		/* start sending from tcp_snxt */
		snxt = tcp->tcp_snxt;

		/*
		 * Check to see if this connection has been idled for some
		 * time and no ACK is expected.  If it is, we need to slow
		 * start again to get back the connection's "self-clock" as
		 * described in VJ's paper.
		 *
		 * Use tcp_slow_start_after_idle to determine the slow start
		 * cwnd size.  Sally Floyd proposed using MAX(2*MSS, 4380)
		 * as the cwnd size of all slow start.  We only do that
		 * for slow start after idle, we may want to change that
		 * later.  And by setting tcp_slow_start_after_idle to a
		 * large value, slow start can be disabled.
		 */
		if ((tcp->tcp_suna == snxt) &&
		    (TICK_TO_MSEC(lbolt - tcp->tcp_last_recv_time) >=
		    tcp->tcp_rto)) {
			tcp->tcp_cwnd = MIN(tcp->tcp_cwnd,
			    tcp_slow_start_after_idle * mss);
		}

		{
		int	usable;
		usable = tcp->tcp_swnd;		/* tcp window size */
		if (usable > tcp->tcp_cwnd)
			usable = tcp->tcp_cwnd;	/* congestion window smaller */
		usable -= snxt;		/* subtract stuff already sent */
		suna = tcp->tcp_suna;
		usable += suna;
		/* usable can be < 0 if the congestion window is smaller */
		if (len > usable)
			/* Can't send complete M_DATA in one shot */
			goto slow;
		}

		/*
		 * determine if anything to send (Nagle).
		 *
		 *   1. len < tcp_mss (i.e. small)
		 *   2. unacknowledged data present
		 *   3. len < nagle limit
		 *   4. last packet sent < nagle limit (previous packet sent)
		 */
		if ((len < mss) && (snxt != suna) &&
		    (len < (int)tcp->tcp_naglim) &&
		    (tcp->tcp_last_sent_len < tcp->tcp_naglim)) {
			/*
			 * This was the first unsent packet and normally
			 * mss < xmit_hiwater so there is no need to worry
			 * about flow control. The next packet will go
			 * through the flow control check in tcp_wput_slow.
			 */
			/* leftover work from above */
			tcp->tcp_unsent = len;
			tcp->tcp_xmit_tail_unsent = len;

			TRACE_1(TR_FAC_TCP, TR_TCP_WPUT_OUT,
			    "tcp_wput end (nagle):  q %p", q);
			return;
		}

		/* len <= tcp->tcp_mss && len == unsent so no silly window */

		if (snxt == suna) {
			TCP_TIMER_RESTART(tcp, tcp->tcp_rto);
		}

		/* we have always sent something */
		tcp->tcp_rack_cnt = 0;

		tcp->tcp_snxt = snxt + len;
		tcp->tcp_rexmit_nxt = tcp->tcp_snxt;
		tcp->tcp_rack = tcp->tcp_rnxt;

		if ((mp1 = dupb(mp)) == 0)
			goto no_memory;
		mp->b_prev = (mblk_t *)lbolt;

		/* adjust tcp header information */
		tcph = tcp->tcp_tcph;
		tcph->th_flags[0] = (TH_ACK|TH_PSH);

		{
			uint32_t	sum;
			sum = len + tcp->tcp_tcp_hdr_len + tcp->tcp_sum;
			sum = (sum >> 16) + (sum & 0xFFFF);
			U16_TO_ABE16(sum, tcph->th_sum);
		}

		U32_TO_ABE32(snxt, tcph->th_seq);

		BUMP_MIB(tcp_mib.tcpOutDataSegs);
		UPDATE_MIB(tcp_mib.tcpOutDataBytes, len);
		BUMP_LOCAL(tcp->tcp_obsegs);

		tcp->tcp_last_sent_len = (u_short)len;

		hdrlen = len + tcp->tcp_hdr_len;
		U16_TO_ABE16(hdrlen, tcp->tcp_iph.iph_length);

		/* see if we need to allocate a mblk for the headers */
		hdrlen = tcp->tcp_hdr_len;
		rptr = mp1->b_rptr - hdrlen;
		db = mp1->b_datap;
		if ((db->db_ref != 2) || rptr < db->db_base ||
		    (!OK_32PTR(rptr))) {
			/* NOTE: we assume allocb returns an OK_32PTR */
			mp = allocb(TCP_MAX_COMBINED_HEADER_LENGTH +
				tcp_wroff_xtra, BPRI_MED);
			if (!mp) {
				freemsg(mp1);
				goto no_memory;
			}
			mp->b_cont = mp1;
			mp1 = mp;
			/* Leave room for Link Level header */
			/* hdrlen = tcp->tcp_hdr_len; */
			rptr = &mp1->b_rptr[tcp_wroff_xtra];
			mp1->b_wptr = &rptr[hdrlen];
		}
		mp1->b_rptr = rptr;

		/* Fill in the timestamp option. */
		if (tcp->tcp_snd_ts_ok) {
			U32_TO_BE32((uint32_t)lbolt,
			    (char *)tcph+TCP_MIN_HEADER_LENGTH+4);
			U32_TO_BE32(tcp->tcp_ts_recent,
			    (char *)tcph+TCP_MIN_HEADER_LENGTH+8);
		} else {
			ASSERT(tcp->tcp_tcp_hdr_len == TCP_MIN_HEADER_LENGTH);
		}

		/* copy header into outgoing packet */
		{
			ipaddr_t	*dst = (ipaddr_t *)rptr;
			ipaddr_t	*src = (ipaddr_t *)tcp->tcp_iphc;
			dst[0] = src[0];
			dst[1] = src[1];
			dst[2] = src[2];
			dst[3] = src[3];
			dst[4] = src[4];
			dst[5] = src[5];
			dst[6] = src[6];
			dst[7] = src[7];
			dst[8] = src[8];
			dst[9] = src[9];
			if (hdrlen -= 40) {
				hdrlen >>= 2;
				dst += 10;
				src += 10;
				do {
					*dst++ = *src++;
				} while (--hdrlen);
			}
		}
		TRACE_1(TR_FAC_TCP, TR_TCP_WPUT_OUT, "tcp_wput end:  q %p",
		    q);
		putnext(tcp->tcp_wq, mp1);

		return;

		/*
		 * If we ran out of memory, we pretend to have sent the packet
		 * and that it was lost on the wire.
		 */
no_memory:;

		TRACE_1(TR_FAC_TCP, TR_TCP_WPUT_OUT, "tcp_wput end:  q %p",
		    q);
		return;

slow:;
		/* leftover work from above */
		tcp->tcp_unsent = len;
		tcp->tcp_xmit_tail_unsent = len;
		TRACE_1(TR_FAC_TCP, TR_TCP_WPUT_OUT, "tcp_wput end:  q %p", q);
		tcp_wput_slow(tcp, (mblk_t *)0);
		return;
	}

	TRACE_1(TR_FAC_TCP, TR_TCP_WPUT_OUT, "tcp_wput end:  q %p", q);
	tcp_wput_slow(tcp, mp);
}

/*
 * The TCP slow path write put procedure.
 * NOTE: the logic of the fast path is duplicated from tcp_wput_slow()
 */
static void
tcp_wput_slow(tcp_t *tcp, mblk_t *mp)
{
	int		len;
	mblk_t		*local_time;
	mblk_t		*mp1;
	u_char		*rptr;
	uint32_t	snxt;
	int		tail_unsent;
	int		tcp_state;
	int		usable = 0;
	mblk_t		*xmit_tail;
	queue_t 	*q = tcp->tcp_wq;
	int32_t		num_burst_seg;
	int32_t		mss;
	int32_t		num_sack_blk = 0;
	int32_t		tcp_hdr_len;
	int32_t		tcp_tcp_hdr_len;

	if (!mp) {
		/* Really tacky... but we need this for detached closes. */
		len = tcp->tcp_unsent;
		tcp_state = tcp->tcp_state;
		TRACE_3(TR_FAC_TCP, TR_TCP_WPUT_SLOW_IN,
			"tcp_wput_slow start:  q %p db_type 0%o tcp %p",
			0, 0, tcp);
	} else {
#if CCS_STATS
	wrw_stats.tot.count++;
	wrw_stats.tot.bytes += msgdsize(mp);
#endif
	tcp = (tcp_t *)q->q_ptr;
	TRACE_3(TR_FAC_TCP, TR_TCP_WPUT_SLOW_IN,
		"tcp_wput_slow start:  q %p db_type 0%o tcp %p",
		q, mp->b_datap->db_type, tcp);

	switch (mp->b_datap->db_type) {
	case M_DATA:
		break;
	case M_PROTO:
	case M_PCPROTO:
		rptr = mp->b_rptr;
		ASSERT((uintptr_t)(mp->b_wptr - rptr) <= (uintptr_t)INT_MAX);
		if ((mp->b_wptr - rptr) >=
		    sizeof (((union T_primitives *)0)->type)) {
			t_scalar_t type;

			type = ((union T_primitives *)rptr)->type;
			if (type == T_EXDATA_REQ) {
				len = msgdsize(mp->b_cont) - 1;
				if (len < 0) {
					freemsg(mp);
					TRACE_1(TR_FAC_TCP,
					    TR_TCP_WPUT_SLOW_OUT,
					    "tcp_wput_slow end:  q %p", q);
					return;
				}
				/*
				 * Try to force urgent data out on the wire.
				 * Even if we have unsent data this will
				 * at least send the urgent flag.
				 * XXX does not handle more flag correctly.
				 */
				usable = 1;
				len += tcp->tcp_unsent;
				len += tcp->tcp_snxt;
				tcp->tcp_urg = len;
				tcp->tcp_valid_bits |= TCP_URG_VALID;
				if (tcp_drop_oob) {
					/*
					 * For testing. Drop the data but set
					 * and send the mark without the data.
					 */
					mp->b_cont->b_wptr =
					    mp->b_cont->b_rptr;
					freemsg(mp->b_cont->b_cont);
					mp->b_cont->b_cont = NULL;
				}
			} else if (type != T_DATA_REQ) {
				tcp_wput_proto(q, mp);
				TRACE_1(TR_FAC_TCP, TR_TCP_WPUT_SLOW_OUT,
				"tcp_wput_slow end:  q %p", q);
				return;
			}
			/* TODO: options, flags, ... from user */
			/* Set length to zero for reclamation below */
			mp->b_wptr = mp->b_rptr;
			break;
		}
		/* FALLTHRU */
	default:
		if (q->q_next) {
			putnext(q, mp);
			TRACE_1(TR_FAC_TCP, TR_TCP_WPUT_SLOW_OUT,
				"tcp_wput_slow end:  q %p", q);
			return;
		}
		(void) mi_strlog(q, 1, SL_ERROR|SL_TRACE,
		    "tcp_wput_slow, dropping one...");
		freemsg(mp);
		TRACE_1(TR_FAC_TCP, TR_TCP_WPUT_SLOW_OUT,
			"tcp_wput_slow end:  q %p", q);
		return;
	case M_IOCTL:
		tcp_wput_ioctl(q, mp);
		TRACE_1(TR_FAC_TCP, TR_TCP_WPUT_SLOW_OUT,
			"tcp_wput_slow end:  q %p", q);
		return;
	case M_IOCDATA:
		tcp_wput_iocdata(q, mp);
		TRACE_1(TR_FAC_TCP, TR_TCP_WPUT_SLOW_OUT,
			"tcp_wput_slow end:  q %p", q);
		return;
	case M_FLUSH:
		tcp_wput_flush(q, mp);
		TRACE_1(TR_FAC_TCP, TR_TCP_WPUT_SLOW_OUT,
			"tcp_wput_slow end:  q %p", q);
		return;
	case M_IOCNAK:
		if (((struct iocblk *)mp->b_rptr)->ioc_cmd == I_SYNCSTR) {
			/*
			 * The M_IOCTL of I_SYNCSTR has made
			 * it back done as an M_IOCNAK.
			 */
			if (tcp->tcp_co_norm && isuioq(tcp->tcp_rq)) {
				/*
				 * Additional mblk(s) have been putnext()ed
				 * since the I_SYNCSTR was putnext()ed and we
				 * are still in SYNCSTR mode, so do it again.
				 */
				tcp->tcp_co_norm = 0;
				(void) struio_ioctl(tcp->tcp_rq, mp);
			} else {
				/*
				 * No additional mblk(s) putnext()ed,
				 * so save the mblk for reuse (switch
				 * to synchronous  mode) and check for
				 * a deferred strwakeq() needed.
				 */
				tcp->tcp_co_imp = mp;
				if (tcp->tcp_co_wakeq_need) {
					tcp->tcp_co_wakeq_need = 0;
					tcp->tcp_co_wakeq_done = 1;
					strwakeq(tcp->tcp_rq, QWANTR);
				}
			}
			TRACE_1(TR_FAC_TCP, TR_TCP_WPUT_SLOW_OUT,
				"tcp_wput_slow end:  q %p", q);
			return;
		}
		if (q->q_next) {
			putnext(q, mp);
			TRACE_1(TR_FAC_TCP, TR_TCP_WPUT_SLOW_OUT,
				"tcp_wput_slow end:  q %p", q);
			return;
		}
		(void) mi_strlog(q, 1, SL_ERROR|SL_TRACE,
		    "tcp_wput_slow, dropping one...");
		freemsg(mp);
		TRACE_1(TR_FAC_TCP, TR_TCP_WPUT_SLOW_OUT,
			"tcp_wput_slow end:  q %p", q);
		return;
	}
	tcp_state = tcp->tcp_state;
	/*
	 * Don't allow data after T_ORDREL_REQ or T_DISCON_REQ,
	 * or before a connection attempt has begun.
	 */
	if (tcp_state < TCPS_SYN_SENT || tcp_state > TCPS_CLOSE_WAIT ||
	    (tcp->tcp_valid_bits & TCP_FSS_VALID) != 0) {
		if ((tcp->tcp_valid_bits & TCP_FSS_VALID) != 0) {
#ifdef DEBUG
			cmn_err(CE_WARN,
			    "tcp_wput_slow: data after ordrel, %s\n",
			    tcp_display(tcp));
#else
			(void) mi_strlog(q, 1, SL_TRACE|SL_ERROR,
			    "tcp_wput_slow: data after ordrel, %s\n",
			    tcp_display(tcp));
#endif /* DEBUG */
		}
		freemsg(mp);
		TRACE_1(TR_FAC_TCP, TR_TCP_WPUT_SLOW_OUT,
			"tcp_wput_slow end:  q %p", q);
		return;
	}

	/* Strip empties */
	for (;;) {
		ASSERT((uintptr_t)(mp->b_wptr - mp->b_rptr) <=
		    (uintptr_t)INT_MAX);
		len = (int)(mp->b_wptr - mp->b_rptr);
		if (len > 0)
			break;
		mp1 = mp;
		mp = mp->b_cont;
		freeb(mp1);
		if (!mp) {
			if (tcp_drop_oob &&
			    (tcp->tcp_valid_bits & TCP_URG_VALID)) {
				mp = tcp_xmit_mp(tcp, NULL, 0, NULL, NULL,
				    tcp->tcp_snxt, 0);
				if (mp) {
					BUMP_LOCAL(tcp->tcp_obsegs);
					putnext(tcp->tcp_wq, mp);
				}
			}
			TRACE_1(TR_FAC_TCP, TR_TCP_WPUT_SLOW_OUT,
				"tcp_wput_slow end:  q %p", q);
			return;
		}
	}

	/* If we are the first on the list ... */
	if (!tcp->tcp_xmit_head) {
		tcp->tcp_xmit_head = mp;
		tcp->tcp_xmit_tail = mp;
		tcp->tcp_xmit_tail_unsent = len;
	} else {
		tcp->tcp_xmit_last->b_cont = mp;
		len += tcp->tcp_unsent;
	}

	/* Tack on however many more positive length mblks we have */
	if ((mp1 = mp->b_cont) != NULL) {
		do {
			int tlen;
			ASSERT((uintptr_t)(mp1->b_wptr - mp1->b_rptr) <=
			    (uintptr_t)INT_MAX);
			tlen = (int)(mp1->b_wptr - mp1->b_rptr);
			if (tlen <= 0) {
				mp->b_cont = mp1->b_cont;
				freeb(mp1);
			} else {
				len += tlen;
				mp = mp1;
			}
		} while ((mp1 = mp->b_cont) != NULL);
	}
	tcp->tcp_xmit_last = mp;
	tcp->tcp_unsent = len;
	}

	/*
	 * Check if this is retransmission.  Note that tcp_rexmit can be
	 * set even if tcp_rexmit_nxt == tcp_snxt.  In this case, it means that
	 * TCP has retransmitted everything when the timeout occurs but TCP
	 * only gets a partial ACK.  And we can send out new data now.
	 */
	if (tcp->tcp_rexmit != 0 && tcp->tcp_rexmit_nxt != tcp->tcp_snxt) {
		snxt = tcp->tcp_rexmit_nxt;
		xmit_tail = tcp_get_seg_mp(tcp, snxt, &tail_unsent);
		ASSERT(xmit_tail != NULL);
		if (xmit_tail == NULL) {
			/*
			 * Defensive coding.  something is wrong.  Set
			 * tcp_rexmit to 0 to make sure the next call is OK.
			 */
			tcp->tcp_rexmit = 0;
			return;
		}
		tail_unsent = xmit_tail->b_wptr - xmit_tail->b_rptr -
		    tail_unsent;
	} else {
		snxt = tcp->tcp_snxt;
		xmit_tail = tcp->tcp_xmit_tail;
		tail_unsent = tcp->tcp_xmit_tail_unsent;
	}

	if (tcp->tcp_snd_sack_ok && tcp->tcp_num_sack_blk > 0) {
		int32_t	opt_len;

		num_sack_blk = MIN(tcp->tcp_max_sack_blk,
		    tcp->tcp_num_sack_blk);
		opt_len = num_sack_blk * sizeof (sack_blk_t) + TCPOPT_NOP_LEN *
		    2 + TCPOPT_HEADER_LEN;
		mss = tcp->tcp_mss - opt_len;
		tcp_hdr_len = tcp->tcp_hdr_len + opt_len;
		tcp_tcp_hdr_len = tcp->tcp_tcp_hdr_len + opt_len;
	} else {
		mss = tcp->tcp_mss;
		tcp_hdr_len = tcp->tcp_hdr_len;
		tcp_tcp_hdr_len = tcp->tcp_tcp_hdr_len;
	}

	if ((tcp->tcp_suna == snxt) &&
	    (TICK_TO_MSEC(lbolt - tcp->tcp_last_recv_time) >= tcp->tcp_rto)) {
		tcp->tcp_cwnd = MIN(tcp->tcp_cwnd, tcp_slow_start_after_idle *
		    tcp->tcp_mss);
	}
	if (tcp_state == TCPS_SYN_RCVD) {
		/*
		 * The three-way connection establishment handshake is not
		 * complete yet. We want to queue the data for transmission
		 * after entering ESTABLISHED state (RFC793). Setting usable to
		 * zero cause a jump to "done" label effectively leaving data
		 * on the queue.
		 */

		usable = 0;

	} else {
		int usable_r = tcp->tcp_swnd;
		/* usable = MIN(swnd, cwnd) - unacked_bytes */
		if (usable_r > tcp->tcp_cwnd)
			usable_r = tcp->tcp_cwnd;

		/* NOTE: trouble if xmitting while SYN not acked? */
		usable_r -= snxt;
		usable_r += tcp->tcp_suna;

		/* usable = MIN(usable, unsent) */
		if (usable_r > len)
			usable_r = len;
		/* usable = MIN(usable, {1 for urgent, 0 for data}) */
		if (usable_r != 0)
			usable = usable_r;
	}

	local_time = (mblk_t *)lbolt;

	/* Check nagle limit */
	len = mss;
	if (len > usable) {
		if (usable < (int)tcp->tcp_naglim &&
		    tcp->tcp_naglim > tcp->tcp_last_sent_len &&
		    snxt != tcp->tcp_suna) {
			/*
			 * Send urgent data ignoring the Nagle algorithm.
			 * This reduces the probability that urgent
			 * bytes get "merged" together.
			 */
			if (!(tcp->tcp_valid_bits & TCP_URG_VALID))
				goto done;
		}
	}

	num_burst_seg = tcp->tcp_snd_burst;
	for (;;) {
		struct datab	*db;
		tcph_t		*tcph;
		uint32_t	sum;

		if (num_burst_seg-- == 0)
			goto done;

		len = mss;
		if (len > usable) {
			len = usable;
			if (len <= 0) {
				/* Terminate the loop */
				goto done;
			}
			/*
			 * Sender silly-window avoidance.
			 * Ignore this if we are going to send a
			 * zero window probe out.
			 *
			 * TODO: force data into microscopic window ??
			 *	==> (!pushed || (unsent > usable))
			 */
			if (len < (tcp->tcp_max_swnd >> 1) &&
			    (tcp->tcp_unsent - (snxt - tcp->tcp_snxt)) > len &&
			    !((tcp->tcp_valid_bits & TCP_URG_VALID) &&
			    len == 1) && (! tcp->tcp_zero_win_probe)) {
				/*
				 * If the retransmit timer is not running
				 * we start it so that we will retransmit
				 * in the case when the the receiver has
				 * decremented the window.
				 */
				if (snxt == tcp->tcp_snxt &&
				    snxt == tcp->tcp_suna) {
					/*
					 * We are not supposed to send
					 * anything.  So let's wait a little
					 * bit longer before breaking SWS
					 * avoidance.
					 *
					 * What should the value be?
					 * Suggestion: MAX(init rexmit time,
					 * tcp->tcp_rto)
					 */
					TCP_TIMER_RESTART(tcp, tcp->tcp_rto);
				}
				goto done;
			}
		}

		tcph = tcp->tcp_tcph;

		usable -= len;	/* Approximate - can be adjusted later */
		if (usable)
			tcph->th_flags[0] = TH_ACK;
		else
			tcph->th_flags[0] = (TH_ACK | TH_PSH);

		/*
		 * Prime pump for IP's checksumming on our behalf
		 * Include the adjustment for a source route if any.
		 */
		sum = len + tcp_tcp_hdr_len + tcp->tcp_sum;
		sum = (sum >> 16) + (sum & 0xFFFF);
		U16_TO_ABE16(sum, tcph->th_sum);

		U32_TO_ABE32(snxt, tcph->th_seq);

		if (tcp->tcp_valid_bits) {
			u_char		*prev_rptr = xmit_tail->b_rptr;
			uint32_t	prev_snxt = tcp->tcp_snxt;

			if (tail_unsent == 0) {
				xmit_tail = xmit_tail->b_cont;
				ASSERT(xmit_tail != NULL);
				prev_rptr = xmit_tail->b_rptr;
			} else {
				xmit_tail->b_rptr = xmit_tail->b_wptr -
				    tail_unsent;
			}
			mp = tcp_xmit_mp(tcp, xmit_tail, len, NULL, NULL,
			    snxt, 0);
			/* Restore tcp_snxt so we get amount sent right. */
			tcp->tcp_snxt = prev_snxt;
			if (prev_rptr == xmit_tail->b_rptr)
				xmit_tail->b_prev = local_time;
			else
				xmit_tail->b_rptr = prev_rptr;
			if (!mp)
				break;

			mp1 = mp->b_cont;
			/*
			 * tcp_xmit_mp might not give us all of len
			 * in order to preserve mblk boundaries.
			 */
			len = msgdsize(mp1);
			snxt += len;
			tcp->tcp_last_sent_len = (u_short)len;
			while (mp1->b_cont) {
				xmit_tail = xmit_tail->b_cont;
				xmit_tail->b_prev = local_time;
				mp1 = mp1->b_cont;
			}
			tail_unsent = xmit_tail->b_wptr - mp1->b_wptr;
			BUMP_LOCAL(tcp->tcp_obsegs);
			BUMP_MIB(tcp_mib.tcpOutDataSegs);
			UPDATE_MIB(tcp_mib.tcpOutDataBytes, len);
			putnext(tcp->tcp_wq, mp);
			continue;
		}

		snxt += len;	/* Adjust later if we don't send all of len */
		BUMP_MIB(tcp_mib.tcpOutDataSegs);
		UPDATE_MIB(tcp_mib.tcpOutDataBytes, len);

		if (tail_unsent) {
			/* Are the bytes above us in flight? */
			rptr = xmit_tail->b_wptr - tail_unsent;
			if (rptr != xmit_tail->b_rptr) {
				tail_unsent -= len;
				len += tcp_hdr_len;
				U16_TO_ABE16(len, tcp->tcp_iph.iph_length);
				mp = dupb(xmit_tail);
				if (!mp)
					break;
				mp->b_rptr = rptr;
				goto must_alloc;
			}
		} else {
			xmit_tail = xmit_tail->b_cont;
			ASSERT((uintptr_t)(xmit_tail->b_wptr -
			    xmit_tail->b_rptr) <= (uintptr_t)INT_MAX);
			tail_unsent = (int)(xmit_tail->b_wptr -
			    xmit_tail->b_rptr);
		}

		tail_unsent -= len;
		tcp->tcp_last_sent_len = (u_short)len;

		len += tcp_hdr_len;
		U16_TO_ABE16(len, tcp->tcp_iph.iph_length);

		xmit_tail->b_prev = local_time;

		mp = dupb(xmit_tail);
		if (!mp)
			break;

		len = tcp_hdr_len;
		/*
		 * There are four reasons to allocate a new hdr mblk:
		 *  1) The bytes above us are in use by another packet
		 *  2) We don't have good alignment
		 *  3) The mblk is being shared
		 *  4) We don't have enough room for a header
		 */
		rptr = mp->b_rptr - len;
		if (!OK_32PTR(rptr) ||
		    ((db = mp->b_datap), db->db_ref != 2) ||
		    rptr < db->db_base) {
			/* NOTE: we assume allocb returns an OK_32PTR */

		must_alloc:;
			mp1 = allocb(TCP_MAX_COMBINED_HEADER_LENGTH +
			    tcp_wroff_xtra, BPRI_MED);
			if (!mp1)
				break;
			mp1->b_cont = mp;
			mp = mp1;
			/* Leave room for Link Level header */
			len = tcp_hdr_len;
			rptr = &mp->b_rptr[tcp_wroff_xtra];
			mp->b_wptr = &rptr[len];
		}

		if (tcp->tcp_snd_ts_ok) {
			U32_TO_BE32((uint32_t)local_time,
				(char *)tcph+TCP_MIN_HEADER_LENGTH+4);
			U32_TO_BE32(tcp->tcp_ts_recent,
			    (char *)tcph+TCP_MIN_HEADER_LENGTH+8);
		} else {
			ASSERT(tcp->tcp_tcp_hdr_len == TCP_MIN_HEADER_LENGTH);
		}

		mp->b_rptr = rptr;
		{
		ipaddr_t	*dst = (ipaddr_t *)rptr;
		ipaddr_t	*src = (ipaddr_t *)tcp->tcp_iphc;
		dst[0] = src[0];
		dst[1] = src[1];
		dst[2] = src[2];
		dst[3] = src[3];
		dst[4] = src[4];
		dst[5] = src[5];
		dst[6] = src[6];
		dst[7] = src[7];
		dst[8] = src[8];
		dst[9] = src[9];
		len = tcp->tcp_hdr_len;
		if (len -= 40) {
			len >>= 2;
			dst += 10;
			src += 10;
			do {
				*dst++ = *src++;
			} while (--len);
		}
		}

		/* Fill in SACK options */
		if (num_sack_blk > 0) {
			u_char *wptr = rptr + tcp->tcp_hdr_len;
			sack_blk_t *tmp;
			int32_t	i;

			wptr[0] = TCPOPT_NOP;
			wptr[1] = TCPOPT_NOP;
			wptr[2] = TCPOPT_SACK;
			wptr[3] = TCPOPT_HEADER_LEN + num_sack_blk *
			    sizeof (sack_blk_t);
			wptr += TCPOPT_REAL_SACK_LEN;

			tmp = tcp->tcp_sack_list;
			for (i = 0; i < num_sack_blk; i++) {
				U32_TO_BE32(tmp[i].begin, wptr);
				wptr += sizeof (tcp_seq);
				U32_TO_BE32(tmp[i].end, wptr);
				wptr += sizeof (tcp_seq);
			}
			tcph = (tcph_t *)(rptr + tcp->tcp_ip_hdr_len);
			tcph->th_offset_and_rsrvd[0] += ((num_sack_blk * 2 + 1)
			    << 4);
		}

		if (tail_unsent) {
			mp1 = mp->b_cont;
			if (!mp1)
				mp1 = mp;
			/*
			 * If we're a little short, tack on more mblks
			 * as long as we don't need to split an mblk.
			 */
			if (tail_unsent < 0 &&
			    tail_unsent + (int)(xmit_tail->b_cont->b_wptr -
			    xmit_tail->b_cont->b_rptr) <= 0) {
				do {
					xmit_tail = xmit_tail->b_cont;
					/* Stash for rtt use later */
					xmit_tail->b_prev = local_time;
					mp1->b_cont = dupb(xmit_tail);
					mp1 = mp1->b_cont;
					ASSERT((uintptr_t)(xmit_tail->b_wptr -
					    xmit_tail->b_rptr) <=
					    (uintptr_t)INT_MAX);
					tail_unsent += (int)
					    (xmit_tail->b_wptr -
					    xmit_tail->b_rptr);
					if (!mp1)
						goto out_of_mem;
				} while (tail_unsent < 0 &&
				    tail_unsent +
				    (int)(xmit_tail->b_cont->b_wptr -
				    xmit_tail->b_cont->b_rptr) <= 0);
			}
			/* Trim back any surplus on the last mblk */
			if (tail_unsent > 0)
				mp1->b_wptr -= tail_unsent;
			if (tail_unsent < 0) {
				/*
				 * We did not send everything we could in
				 * order to preserve mblk boundaries.
				 */
				usable -= tail_unsent;
				snxt += tail_unsent;
				tcp->tcp_last_sent_len += tail_unsent;
				UPDATE_MIB(tcp_mib.tcpOutDataBytes,
				    tail_unsent);
				/*
				 * Adjust the checksum
				 */
				tcph = (tcph_t *)(rptr + tcp->tcp_ip_hdr_len);
				sum += tail_unsent;
				sum = (sum >> 16) + (sum & 0xFFFF);
				U16_TO_ABE16(sum, tcph->th_sum);
#ifdef _BIG_ENDIAN
				((ipha_t *)rptr)->ipha_length += tail_unsent;
#else
				/* for little endian systems need to swap */
				sum = BE16_TO_U16(((iph_t *)rptr)->iph_length)
				    + tail_unsent;
				U16_TO_BE16(sum, ((iph_t *)rptr)->iph_length);
#endif
				tail_unsent = 0;
			}
		}

		BUMP_LOCAL(tcp->tcp_obsegs);
		putnext(tcp->tcp_wq, mp);
	}
out_of_mem:;
	if (mp)
		freemsg(mp);
	/* Pretend that all we were trying to send really got sent */
	if (tail_unsent < 0) {
		do {
			xmit_tail = xmit_tail->b_cont;
			xmit_tail->b_prev = local_time;
			ASSERT((uintptr_t)(xmit_tail->b_wptr -
			    xmit_tail->b_rptr) <= (uintptr_t)INT_MAX);
			tail_unsent += (int)(xmit_tail->b_wptr -
			    xmit_tail->b_rptr);
		} while (tail_unsent < 0);
	}
done:;
	/* Was new data sent? */
	if (SEQ_GEQ(snxt, tcp->tcp_snxt)) {
		tcp->tcp_xmit_tail = xmit_tail;
		tcp->tcp_xmit_tail_unsent = tail_unsent;
		len = tcp->tcp_snxt - snxt;
		if (len) {
			/*
			 * If new data was sent, need to update the notsack
			 * list, which is, afterall, data blocks that have
			 * not been sack'ed by the receiver.  New data is
			 * not sack'ed.
			 */
			if (tcp->tcp_snd_sack_ok &&
			    tcp->tcp_notsack_list != NULL) {
				/* len is a negative value. */
				tcp->tcp_pipe -= len;
				tcp_notsack_update(&(tcp->tcp_notsack_list),
				    tcp->tcp_snxt, snxt,
				    &(tcp->tcp_num_notsack_blk),
				    &(tcp->tcp_cnt_notsack_list));
			}
			tcp->tcp_snxt = snxt + tcp->tcp_fin_sent;
			tcp->tcp_rack = tcp->tcp_rnxt;
			tcp->tcp_rack_cnt = 0;
			if ((snxt + len) == tcp->tcp_suna)
				TCP_TIMER_RESTART(tcp, tcp->tcp_rto);
		} else if (snxt == tcp->tcp_suna && tcp->tcp_swnd == 0) {
			/*
			 * Didn't send anything. Make sure the timer is running
			 * so that we will probe a zero window.
			 */
			TCP_TIMER_RESTART(tcp, tcp->tcp_rto);
		}
		tcp->tcp_rexmit_nxt = tcp->tcp_snxt;
		/*
		 * Note that len is the amount we just sent but with a
		 * negative sign
		 */
		len += tcp->tcp_unsent;
		tcp->tcp_unsent = len;
		if (tcp->tcp_flow_stopped) {
			if (len <= tcp->tcp_xmit_lowater) {
				tcp->tcp_flow_stopped = false;
				enableok(tcp->tcp_wq);
				rmvq(tcp->tcp_wq, tcp->tcp_flow_mp);
			}
		} else {
			/* The tcp_flow_mp of detached folks is nil */
			if (len >= tcp->tcp_xmit_hiwater &&
			    tcp->tcp_flow_mp) {
				tcp->tcp_flow_stopped = true;
				noenable(tcp->tcp_wq);
				(void) putq(tcp->tcp_wq, tcp->tcp_flow_mp);
			}
		}
	} else {
		/* This time len is positive. */
		len = snxt - tcp->tcp_rexmit_nxt;
		if (len) {
			tcp->tcp_rack = tcp->tcp_rnxt;
			tcp->tcp_rack_cnt = 0;
			tcp->tcp_rexmit_nxt = snxt;
			if ((snxt - len) == tcp->tcp_suna)
				TCP_TIMER_RESTART(tcp, tcp->tcp_rto);
			UPDATE_MIB(tcp_mib.tcpRetransSegs, tcp->tcp_snd_burst -
			    num_burst_seg);
			UPDATE_MIB(tcp_mib.tcpRetransBytes, len);
		}
	}
	TRACE_1(TR_FAC_TCP, TR_TCP_WPUT_SLOW_OUT, "tcp_wput_slow end:  q %X",
	    q);
}

/* tcp_wput_flush is called by tcp_wput_slow to handle M_FLUSH messages. */
static void
tcp_wput_flush(queue_t *q, mblk_t *mp)
{
	u_char	fval = *mp->b_rptr;
	mblk_t	*tail;
	tcp_t	*tcp = (tcp_t *)q->q_ptr;

	/* TODO: How should flush interact with urgent data? */
	if ((fval & FLUSHW) && tcp->tcp_xmit_head &&
	    !(tcp->tcp_valid_bits & TCP_URG_VALID)) {
		/*
		 * Flush only data that has not yet been put on the wire.  If
		 * we flush data that we have already transmitted, life, as we
		 * know it, may come to an end.
		 */
		tail = tcp->tcp_xmit_tail;
		tail->b_wptr -= tcp->tcp_xmit_tail_unsent;
		tcp->tcp_xmit_tail_unsent = 0;
		tcp->tcp_unsent = 0;
		if (tail->b_wptr != tail->b_rptr)
			tail = tail->b_cont;
		if (tail) {
			mblk_t **excess = &tcp->tcp_xmit_head;
			for (;;) {
				mblk_t *mp1 = *excess;
				if (mp1 == tail)
					break;
				tcp->tcp_xmit_tail = mp1;
				tcp->tcp_xmit_last = mp1;
				excess = &mp1->b_cont;
			}
			*excess = nilp(mblk_t);
			tcp_close_mpp(&tail);
		}
		/*
		 * We have no unsent data, so unsent must be less than
		 * tcp_xmit_lowater, so re-enable flow.
		 */
		if (tcp->tcp_flow_stopped) {
			tcp->tcp_flow_stopped = false;
			enableok(tcp->tcp_wq);
			rmvq(tcp->tcp_wq, tcp->tcp_flow_mp);
		}
	}
	/*
	 * TODO: you can't just flush these, you have to increase rwnd for one
	 * thing.  For another, how should urgent data interact?
	 */
	if (fval & FLUSHR) {
		*mp->b_rptr = fval & ~FLUSHW;
		qreply(q, mp);
		return;
	}
	freemsg(mp);
}

/*
 * tcp_wput_iocdata is called by tcp_wput_slow to handle all M_IOCDATA
 * messages.
 */
static void
tcp_wput_iocdata(queue_t *q, mblk_t *mp)
{
	char	*addr_cp;
	ipa_t	*ipaddr;
	mblk_t	*mp1;
	STRUCT_HANDLE(strbuf, sb);
	char	*port_cp;
	tcp_t	*tcp;

	/* Make sure it is one of ours. */
	switch (((struct iocblk *)mp->b_rptr)->ioc_cmd) {
	case TI_GETMYNAME:
	case TI_GETPEERNAME:
		break;
	default:
		putnext(q, mp);
		return;
	}
	switch (mi_copy_state(q, mp, &mp1)) {
	case -1:
		return;
	case MI_COPY_CASE(MI_COPY_IN, 1):
		break;
	case MI_COPY_CASE(MI_COPY_OUT, 1):
		/* Copy out the strbuf. */
		mi_copyout(q, mp);
		return;
	case MI_COPY_CASE(MI_COPY_OUT, 2):
		/* All done. */
		mi_copy_done(q, mp, 0);
		return;
	default:
		mi_copy_done(q, mp, EPROTO);
		return;
	}
	STRUCT_SET_HANDLE(sb, ((struct iocblk *)mp->b_rptr)->ioc_flag,
	    (void *)mp1->b_rptr);
	if (STRUCT_FGET(sb, maxlen) < (int)sizeof (ipa_t)) {
		mi_copy_done(q, mp, EINVAL);
		return;
	}
	tcp = (tcp_t *)q->q_ptr;
	switch (((struct iocblk *)mp->b_rptr)->ioc_cmd) {
	case TI_GETMYNAME:
		addr_cp = (char *)tcp->tcp_iph.iph_src;
		port_cp = (char *)&tcp->tcp_lport;
		break;
	case TI_GETPEERNAME:
		addr_cp = (char *)&tcp->tcp_remote;
		port_cp = (char *)&tcp->tcp_fport;
		break;
	default:
		mi_copy_done(q, mp, EPROTO);
		return;
	}
	mp1 = mi_copyout_alloc(q, mp, STRUCT_FGETP(sb, buf), sizeof (ipa_t));
	if (!mp1)
		return;
	STRUCT_FSET(sb, len, (int)sizeof (ipa_t));
	ipaddr = (ipa_t *)mp1->b_rptr;
	mp1->b_wptr = (u_char *)&ipaddr[1];
	bzero(ipaddr, sizeof (ipa_t));
	ipaddr->ip_family = AF_INET;
	bcopy(addr_cp, &ipaddr->ip_addr, IP_ADDR_LEN);
	bcopy(port_cp, &ipaddr->ip_port, 2);
	/* Copy out the address */
	mi_copyout(q, mp);
}

/* tcp_wput_ioctl is called by tcp_wput_slow to handle all M_IOCTL messages. */
static void
tcp_wput_ioctl(queue_t *q, mblk_t *mp)
{
	tcp_t	*tcp = (tcp_t *)q->q_ptr;
	struct iocblk	*iocp;

	iocp = (struct iocblk *)mp->b_rptr;
	switch (iocp->ioc_cmd) {
	case TCP_IOC_DEFAULT_Q:
		/* Wants to be the default wq. */
		if (!tcp->tcp_priv_stream) {
			iocp->ioc_error = EPERM;
			goto err_ret;
		}
		tcp_def_q_set(q, mp);
		return;
	case TI_GETPEERNAME:
		if (tcp->tcp_state < TCPS_SYN_RCVD) {
			iocp->ioc_error = ENOTCONN;
err_ret:;
			iocp->ioc_count = 0;
			mp->b_datap->db_type = M_IOCACK;
			qreply(q, mp);
			return;
		}
		/* FALLTHRU */
	case TI_GETMYNAME:
		mi_copyin(q, mp, NULL, SIZEOF_STRUCT(strbuf, iocp->ioc_flag));
		return;
	case ND_SET:
		if (!tcp->tcp_priv_stream) {
			iocp->ioc_error = EPERM;
			goto err_ret;
		}
		/* FALLTHRU */

	case ND_GET:
		if (!nd_getset(q, tcp_g_nd, mp)) {
			break;
		}
		qreply(q, mp);
		return;
	}
	putnext(q, mp);
}

/*
 * This routine is called by tcp_wput_slow to handle all TPI requests other
 * than T_DATA_REQ.
 */
static void
tcp_wput_proto(queue_t *q, mblk_t *mp)
{
	tcp_t	*tcp = (tcp_t *)q->q_ptr;
	union T_primitives *tprim = (union T_primitives *)mp->b_rptr;

	switch ((int)tprim->type) {
	case O_T_BIND_REQ:	/* bind request */
	case T_BIND_REQ:	/* new semantics bind request */
		tcp_bind(q, mp);
		return;
	case T_UNBIND_REQ:	/* unbind request */
		tcp_unbind(q, mp);
		return;
	case O_T_CONN_RES:	/* old connection response XXX */
	case T_CONN_RES:	/* connection response */
		tcp_accept(q, mp);
		return;
	case T_CONN_REQ:	/* connection request */
		tcp_connect(q, mp);
		return;
	case T_DISCON_REQ:	/* disconnect request */
		tcp_disconnect(q, mp);
		return;
	case T_CAPABILITY_REQ:
		tcp_capability_req(tcp, mp);	/* capability request */
		return;
	case T_INFO_REQ:	/* information request */
		tcp_info_req(tcp, mp);
		return;
	case O_T_OPTMGMT_REQ:	/* manage options req */
		if (!snmpcom_req(q, mp, tcp_snmp_set, tcp_snmp_get,
		    tcp->tcp_priv_stream))
			svr4_optcom_req(tcp->tcp_wq, mp, tcp->tcp_priv_stream,
			    &tcp_opt_obj);
		return;
	case T_OPTMGMT_REQ:
		/*
		 * Note:  no support for snmpcom_req() through new
		 * T_OPTMGMT_REQ. See comments in ip.c
		 */
		tpi_optcom_req(tcp->tcp_wq, mp, tcp->tcp_priv_stream,
		    &tcp_opt_obj);
		return;

	case T_UNITDATA_REQ:	/* unitdata request */
		tcp_err_ack(tcp->tcp_wq, mp, TNOTSUPPORT, 0);
		return;
	case T_ORDREL_REQ:	/* orderly release req */
		freemsg(mp);
		if (tcp_xmit_end(tcp) != 0) {
			(void) mi_strlog(q, 1, SL_ERROR|SL_TRACE,
			    "tcp_wput_slow, T_ORDREL_REQ out of state %s",
			    tcp_display(tcp));
			(void) putctl1(tcp->tcp_rq, M_ERROR, EPROTO);
		}
		return;
	case T_ADDR_REQ:
		tcp_addr_req(tcp, mp);
		return;
	default:
		(void) mi_strlog(q, 1, SL_ERROR|SL_TRACE,
		    "tcp_wput_slow, bogus TPI msg, type %d", tprim->type);
		/*
		 * We used to M_ERROR.  Sending TNOTSUPPORT gives the user
		 * to recover.
		 */
		tcp_err_ack(tcp->tcp_wq, mp, TNOTSUPPORT, 0);
		return;
	}
}

/*
 * The TCP write service routine.  The only thing we expect to see here is
 * timer messages.
 */
static void
tcp_wsrv(queue_t *q)
{
	mblk_t	*mp;
	tcp_t	*tcp = (tcp_t *)q->q_ptr;

	TRACE_1(TR_FAC_TCP, TR_TCP_WSRV_IN, "tcp_wsrv start:  q %p", q);

	/*
	 * Write side has is always noenable()'d so that putting tcp_flow_mp
	 * on the queue will not cause the service procedure to run.
	 * Avoid removing tcp_flow_mp to avoid spurious backenabling.
	 */
	while ((q->q_first == NULL || q->q_first != tcp->tcp_flow_mp) &&
	    (mp = getq(q)) != NULL) {
		if (mp->b_datap->db_type != M_PCSIG) {
			ASSERT(mp != tcp->tcp_flow_mp);
			(void) putbq(q, mp);
			break;
		}
		if (mi_timer_valid(mp)) {
			tcpt_t	*tcpt = (tcpt_t *)mp->b_rptr;
			tcp_t	*tcp = tcpt->tcpt_tcp;

			ASSERT(tcp->tcp_wq == q);
			(*tcpt->tcpt_pfv)(tcp);
		}
	}
	TRACE_1(TR_FAC_TCP, TR_TCP_WSRV_OUT, "tcp_wsrv end:  q %p", q);
}

/* Non overlapping byte exchanger */
static void
tcp_xchg(u_char	*a, u_char *b, int len)
{
	u_char	uch;

	while (len-- > 0) {
		uch = a[len];
		a[len] = b[len];
		b[len] = uch;
	}
}

/*
 * Send out a control packet on the tcp connection specified.  This routine
 * is typically called where we need a simple ACK or RST generated.
 */
static void
tcp_xmit_ctl(char *str, tcp_t *tcp, mblk_t *mp, uint32_t seq, uint32_t ack,
    int ctl)
{
	u_char		*rptr;
	tcph_t		*tcph;
	iph_t		*iph;
	uint32_t	sum;

	/*
	 * Save sum for use in source route later.
	 */
	sum = tcp->tcp_tcp_hdr_len + tcp->tcp_sum;

	if (mp) {
		iph = (iph_t *)mp->b_rptr;
		ASSERT(((iph->iph_version_and_hdr_length) & 0xf0) == 0x40);
		tcph = (tcph_t *)(mp->b_rptr + IPH_HDR_LENGTH(iph));
		if (tcph->th_flags[0] & TH_RST) {
			freemsg(mp);
			return;
		}
		freemsg(mp);
	}
	/* If a text string is passed in with the request, pass it to strlog. */
	if (str) {
		(void) mi_strlog(tcp->tcp_rq, 1, SL_TRACE,
		    "tcp_xmit_ctl: '%s', seq 0x%x, ack 0x%x, ctl 0x%x",
		    str, seq, ack, ctl);
	}
	mp = allocb(TCP_MAX_COMBINED_HEADER_LENGTH + tcp_wroff_xtra, BPRI_MED);
	if (!mp)
		return;
	rptr = &mp->b_rptr[tcp_wroff_xtra];
	mp->b_rptr = rptr;
	mp->b_wptr = &rptr[tcp->tcp_hdr_len];
	bcopy(tcp->tcp_iphc, rptr, tcp->tcp_hdr_len);
	iph = (iph_t *)rptr;
	U16_TO_BE16(tcp->tcp_hdr_len, iph->iph_length);
	tcph = (tcph_t *)&rptr[tcp->tcp_ip_hdr_len];
	tcph->th_flags[0] = (uint8_t)ctl;
	if (ctl & TH_RST) {
		BUMP_MIB(tcp_mib.tcpOutRsts);
		BUMP_MIB(tcp_mib.tcpOutControl);
		/*
		 * Don't send TSopt w/ TH_RST packets per RFC 1323.
		 */
		if (tcp->tcp_snd_ts_ok) {
			mp->b_wptr = &rptr[tcp->tcp_hdr_len -
			    TCPOPT_REAL_TS_LEN];
			*(mp->b_wptr) = TCPOPT_EOL;
			U16_TO_BE16(tcp->tcp_hdr_len-TCPOPT_REAL_TS_LEN,
			    iph->iph_length);
			tcph->th_offset_and_rsrvd[0] -= (3 << 4);
			sum -= TCPOPT_REAL_TS_LEN;
		}
	}
	if (ctl & TH_ACK) {
		if (tcp->tcp_snd_ts_ok) {
			U32_TO_BE32(lbolt,
			    (char *)tcph+TCP_MIN_HEADER_LENGTH+4);
			U32_TO_BE32(tcp->tcp_ts_recent,
			    (char *)tcph+TCP_MIN_HEADER_LENGTH+8);
		}
		tcp->tcp_rack = ack;
		tcp->tcp_rack_cnt = 0;
		BUMP_MIB(tcp_mib.tcpOutAck);
	}
	BUMP_LOCAL(tcp->tcp_obsegs);
	U32_TO_BE32(seq, tcph->th_seq);
	U32_TO_BE32(ack, tcph->th_ack);
	/*
	 * Include the adjustment for a source route if any.
	 */
	sum = (sum >> 16) + (sum & 0xFFFF);
	U16_TO_BE16(sum, tcph->th_sum);
	putnext(tcp->tcp_wq, mp);
}

/*
 * Generate a reset based on an inbound packet for which there is no active
 * tcp state that we can find.
 */
static void
tcp_xmit_early_reset(char *str, queue_t *q, mblk_t *mp, uint32_t seq,
    uint32_t ack, int ctl)
{
	ipha_t	*ipha;
	u_short	len;
	tcph_t	*tcph;
	int	i;
	ipaddr_t addr;

	if (str && q) {
		(void) mi_strlog(q, 1, SL_TRACE,
		    "tcp_xmit_early_reset: '%s', seq 0x%x, ack 0x%x, "
		    "flags 0x%x",
		    str, seq, ack, ctl);
	}
	if (mp->b_datap->db_ref != 1) {
		mblk_t *mp1 = copyb(mp);
		freemsg(mp);
		mp = mp1;
		if (!mp)
			return;
	} else if (mp->b_cont) {
		freemsg(mp->b_cont);
		mp->b_cont = nilp(mblk_t);
	}
	/*
	 * We skip reversing source route here.
	 * (for now we replace all IP options with EOL)
	 */
	ipha = (ipha_t *)mp->b_rptr;
	len = IPH_HDR_LENGTH(ipha);
	for (i = IP_SIMPLE_HDR_LENGTH; i < (int)len; i++)
		mp->b_rptr[i] = IPOPT_EOL;
	tcph = (tcph_t *)&mp->b_rptr[len];
	if (tcph->th_flags[0] & TH_RST) {
		freemsg(mp);
		return;
	}
	tcph->th_offset_and_rsrvd[0] = (5 << 4);
	len += sizeof (tcph_t);
	mp->b_wptr = &mp->b_rptr[len];
	ipha->ipha_length = htons(len);
	/* Swap addresses */
	addr = ipha->ipha_src;
	ipha->ipha_src = ipha->ipha_dst;
	ipha->ipha_dst = addr;
	ipha->ipha_ident = 0;
	tcp_xchg(tcph->th_fport, tcph->th_lport, 2);
	U32_TO_BE32(ack, tcph->th_ack);
	U32_TO_BE32(seq, tcph->th_seq);
	U16_TO_BE16(0, tcph->th_win);
	U16_TO_BE16(sizeof (tcph_t), tcph->th_sum);
	tcph->th_flags[0] = (uint8_t)ctl;
	if (ctl & TH_RST) {
		BUMP_MIB(tcp_mib.tcpOutRsts);
		BUMP_MIB(tcp_mib.tcpOutControl);
	}
	qreply(q, mp);
}

/*
 * Initiate closedown sequence on an active connection.  (May be called as
 * writer.)  Return value zero for OK return, non-zero for error return.
 */
static int
tcp_xmit_end(tcp_t *tcp)
{
	struct iocblk	*ioc;
	ipic_t	*ipic;
	mblk_t	*mp;
	mblk_t	*mp1;

	if (tcp->tcp_state < TCPS_SYN_RCVD ||
	    tcp->tcp_state > TCPS_CLOSE_WAIT) {
		/*
		 * Invalid state, only states TCPS_SYN_RCVD,
		 * TCPS_ESTABLISHED and TCPS_CLOSE_WAIT are valid
		 */
		return (-1);
	}

	tcp->tcp_fss = tcp->tcp_snxt + tcp->tcp_unsent;
	tcp->tcp_valid_bits |= TCP_FSS_VALID;
	/*
	 * If there is nothing more unsent, send the FIN now.
	 * Otherwise, it will go out with the last segment.
	 */
	if (tcp->tcp_unsent == 0) {
		mp = tcp_xmit_mp(tcp, nilp(mblk_t), 0, NULL, NULL,
		    tcp->tcp_fss, 0);
		if (mp) {
			putnext(tcp->tcp_wq, mp);
		} else {
			/*
			 * Couldn't allocate msg.  Pretend we got it out.
			 * Wait for rexmit timeout.
			 */
			tcp->tcp_snxt = tcp->tcp_fss + 1;
			TCP_TIMER_RESTART(tcp, tcp->tcp_rto);
		}
	}

	/*
	 * Don't record ssthresh unless it's been modified.
	 * Don't allow folks to set the rtt unless they have some experience.
	 * NOTE: do we need to exclude old data? (remember when rtt was set)
	 */
	if ((tcp->tcp_cwnd_ssthresh == TCP_MAX_LARGEWIN) &&
	    (!tcp_rtt_updates || tcp->tcp_rtt_update < tcp_rtt_updates))
		return (0);

	/*
	 * NOTE: should not update if source routes i.e. if tcp_remote if
	 * different from iph_dst.
	 */
	if (tcp->tcp_remote !=  tcp->tcp_ipha.ipha_dst) {
		return (0);
	}

	/* Record route attributes in the IRE for use by future connections. */
	mp = allocb(sizeof (ipic_t) + IP_ADDR_LEN, BPRI_HI);
	if (!mp)
		return (0);
	bzero(mp->b_rptr, sizeof (ipic_t) + IP_ADDR_LEN);
	ipic = (ipic_t *)mp->b_rptr;
	ipic->ipic_cmd = IP_IOC_IRE_ADVISE_NO_REPLY;
	ipic->ipic_addr_offset = sizeof (ipic_t);
	ipic->ipic_addr_length = IP_ADDR_LEN;

	if (tcp_rtt_updates != 0 && tcp->tcp_rtt_update >= tcp_rtt_updates) {
		ipic->ipic_rtt = tcp->tcp_rtt_sa;
		ipic->ipic_rtt_sd = tcp->tcp_rtt_sd;
	}
	if (tcp->tcp_cwnd_ssthresh != TCP_MAX_LARGEWIN) {
		ipic->ipic_ssthresh = MAX(tcp->tcp_cwnd_ssthresh,
		    (MIN(tcp->tcp_cwnd, tcp->tcp_swnd) / 2));
	}
	bcopy(&tcp->tcp_iph.iph_dst, &ipic[1], IP_ADDR_LEN);
	mp->b_wptr = &mp->b_rptr[sizeof (ipic_t) + IP_ADDR_LEN];

	mp1 = mkiocb(IP_IOCTL);
	if (!mp1) {
		freemsg(mp);
		return (0);
	}
	mp1->b_cont = mp;
	ioc = (struct iocblk *)mp1->b_rptr;
	ioc->ioc_count = sizeof (ipic_t) + IP_ADDR_LEN;

	putnext(tcp->tcp_wq, mp1);
	return (0);
}

/*
 * Generate a "no listener here" reset in response to the
 * connection request contained within 'mp'
 */
static void
tcp_xmit_listeners_reset(queue_t *rq, mblk_t *mp)
{
	u_char		*rptr	= mp->b_rptr;
	uint32_t	seg_len = IPH_HDR_LENGTH(rptr);
	tcph_t		*tcph	= (tcph_t *)&rptr[seg_len];
	uint32_t	seg_seq = BE32_TO_U32(tcph->th_seq);
	uint32_t	seg_ack = BE32_TO_U32(tcph->th_ack);
	u_int		flags = tcph->th_flags[0];

	seg_len = msgdsize(mp) - (TCP_HDR_LENGTH(tcph) + seg_len);
	if (flags & TH_RST)
		freemsg(mp);
	else if (flags & TH_ACK) {
		tcp_xmit_early_reset("no tcp, reset",
		    rq, mp, seg_ack, 0, TH_RST);
	} else {
		if (flags & TH_SYN)
			seg_len++;
		tcp_xmit_early_reset("no tcp, reset/ack", rq,
		    mp, 0, seg_seq + seg_len,
		    TH_RST | TH_ACK);
	}
}

/*
 * tcp_xmit_mp is called to return a pointer to an mblk chain complete with
 * ip and tcp header ready to pass down to IP.  If the mp passed in is
 * non-nil, then up to max_to_send bytes of data will be dup'ed off that
 * mblk. (If sendall is not set the dup'ing will stop at an mblk boundary
 * otherwise it will dup partial mblks.)
 * Otherwise, an appropriate ACK packet will be generated.  This
 * routine is not usually called to send new data for the first time.  It
 * is mostly called out of the timer for retransmits, and to generate ACKs.
 *
 * If offset is not NULL, the returned mblk chain's first mblk's b_rptr will
 * be adjusted by *offset.  And after dupb(), the offset and the ending mblk
 * of the original mblk chain will be returned in *offset and *end_mp.
 */
static mblk_t *
tcp_xmit_mp(tcp_t *tcp, mblk_t *mp, int32_t max_to_send, int32_t *offset,
    mblk_t **end_mp, uint32_t seq, int32_t sendall)
{
	int	data_length;
	int32_t	off = 0;
	u_int	flags;
	mblk_t	*mp1;
	mblk_t	*mp2;
	u_char	*rptr;
	tcph_t	*tcph;
	int32_t	num_sack_blk = 0;
	int32_t	sack_opt_len = 0;

	/* Allocate for our maximum TCP header + link-level */
	mp1 = allocb(TCP_MAX_COMBINED_HEADER_LENGTH + tcp_wroff_xtra,
	    BPRI_MED);
	if (!mp1)
		return (nilp(mblk_t));
	data_length = 0;

	if (tcp->tcp_snd_sack_ok && tcp->tcp_num_sack_blk > 0) {
		num_sack_blk = MIN(tcp->tcp_max_sack_blk,
		    tcp->tcp_num_sack_blk);
		sack_opt_len = num_sack_blk * sizeof (sack_blk_t) +
		    TCPOPT_NOP_LEN * 2 + TCPOPT_HEADER_LEN;
		if (max_to_send + sack_opt_len > tcp->tcp_mss)
			max_to_send -= sack_opt_len;
	}

	if (offset != NULL) {
		off = *offset;
		/* We use offset as an indicator that end_mp is not NULL. */
		*end_mp = NULL;
	}
	for (mp2 = mp1; mp && data_length != max_to_send; mp = mp->b_cont) {
		/* This could be faster with cooperation from downstream */
		if (mp2 != mp1 && sendall == 0 &&
		    data_length + (int)(mp->b_wptr - mp->b_rptr) >
		    max_to_send)
			/*
			 * Don't send the next mblk since the whole mblk
			 * does not fit.
			 */
			break;
		mp2->b_cont = dupb(mp);
		mp2 = mp2->b_cont;
		if (!mp2) {
			freemsg(mp1);
			return (nilp(mblk_t));
		}
		mp2->b_rptr += off;
		ASSERT((uintptr_t)(mp2->b_wptr - mp2->b_rptr) <=
		    (uintptr_t)INT_MAX);

		data_length += (int)(mp2->b_wptr - mp2->b_rptr);
		if (data_length > max_to_send) {
			mp2->b_wptr -= data_length - max_to_send;
			data_length = max_to_send;
			off = mp2->b_wptr - mp->b_rptr;
			break;
		} else {
			off = 0;
		}
	}
	if (offset != NULL) {
		*offset = off;
		*end_mp = mp;
	}

	rptr = mp1->b_rptr + tcp_wroff_xtra;
	mp1->b_rptr = rptr;
	mp1->b_wptr = rptr + tcp->tcp_hdr_len + sack_opt_len;
	bcopy(tcp->tcp_iphc, rptr, tcp->tcp_hdr_len);
	tcph = (tcph_t *)&rptr[tcp->tcp_ip_hdr_len];
	U32_TO_ABE32(seq, tcph->th_seq);
	/*
	 * Use tcp_unsent to determine if the PSH bit should be used assumes
	 * that this function was called from tcp_wput_slow. Thus, when called
	 * to retransmit data the setting of the PSH bit may appear some
	 * what random in that it might get set when it should not. This
	 * should not pose any performance issues.
	 */
	if (data_length != 0 && (tcp->tcp_unsent == 0 ||
	    tcp->tcp_unsent == data_length))
		flags = TH_ACK | TH_PSH;
	else
		flags = TH_ACK;
	if (tcp->tcp_valid_bits) {
		uint32_t u1;

		if ((tcp->tcp_valid_bits & TCP_ISS_VALID) &&
		    seq == tcp->tcp_iss) {
			u_char	*wptr;
			switch (tcp->tcp_state) {
			case TCPS_SYN_SENT:
				flags = TH_SYN;

				if (tcp->tcp_rcv_ws || tcp_wscale_always) {
					U32_TO_ABE16(MIN(TCP_MAXWIN,
					    tcp->tcp_rwnd_max), tcph->th_win);
					wptr = mp1->b_wptr;
					wptr[0] =  TCPOPT_NOP;
					wptr[1] =  TCPOPT_WSCALE;
					wptr[2] =  TCPOPT_WS_LEN;
					wptr[3] = (u_char) tcp->tcp_rcv_ws;
					mp1->b_wptr += TCPOPT_REAL_WS_LEN;
					tcph->th_offset_and_rsrvd[0] +=
					    (1 << 4);
				}

				if (tcp->tcp_snd_ts_ok || tcp_tstamp_always ||
				    (tcp_tstamp_if_wscale && tcp->tcp_rcv_ws)) {
					uint32_t llbolt = (uint32_t)lbolt;

					tcp->tcp_snd_ts_ok = 1;
					wptr = mp1->b_wptr;
					wptr[0] = TCPOPT_NOP;
					wptr[1] = TCPOPT_NOP;
					wptr[2] = TCPOPT_TSTAMP;
					wptr[3] = TCPOPT_TSTAMP_LEN;
					wptr += 4;
					U32_TO_BE32(llbolt, wptr);
					wptr += 4;
					ASSERT(tcp->tcp_ts_recent == 0);
					U32_TO_BE32(0L, wptr);
					mp1->b_wptr += TCPOPT_REAL_TS_LEN;
					tcph->th_offset_and_rsrvd[0] +=
					    (3 << 4);
				}

				if (tcp_sack_permitted == 2) {
					tcp->tcp_sack_info = kmem_zalloc(
					    sizeof (tcp_sack_info_t),
					    KM_NOSLEEP);
					if (tcp->tcp_sack_info != NULL) {
						tcp->tcp_snd_sack_ok = 1;
					}
					wptr = mp1->b_wptr;
					wptr[0] = TCPOPT_NOP;
					wptr[1] = TCPOPT_NOP;
					wptr[2] = TCPOPT_SACK_PERMITTED;
					wptr[3] = TCPOPT_SACK_OK_LEN;
					mp1->b_wptr += TCPOPT_REAL_SACK_OK_LEN;
					tcph->th_offset_and_rsrvd[0] +=
					    (1 << 4);
				}
				break;
			case TCPS_SYN_RCVD:
				flags |= TH_SYN;

				if (tcp->tcp_rcv_ws)
					U32_TO_ABE16(MIN(TCP_MAXWIN,
					    tcp->tcp_rwnd_max), tcph->th_win);

				if (tcp->tcp_snd_ws_ok) {
				    wptr = mp1->b_wptr;
				    wptr[0] =  TCPOPT_NOP;
				    wptr[1] =  TCPOPT_WSCALE;
				    wptr[2] =  TCPOPT_WS_LEN;
				    wptr[3] = (u_char)tcp->tcp_rcv_ws;
				    mp1->b_wptr += TCPOPT_REAL_WS_LEN;
				    tcph->th_offset_and_rsrvd[0] += (1 << 4);
				}

				if (tcp->tcp_snd_sack_ok) {
					wptr = mp1->b_wptr;
					wptr[0] = TCPOPT_NOP;
					wptr[1] = TCPOPT_NOP;
					wptr[2] = TCPOPT_SACK_PERMITTED;
					wptr[3] = TCPOPT_SACK_OK_LEN;
					mp1->b_wptr += TCPOPT_REAL_SACK_OK_LEN;
					tcph->th_offset_and_rsrvd[0] +=
					    (1 << 4);
				}
				break;
			default:
				break;
			}

			/* Tack on the mss option */
			wptr = mp1->b_wptr;
			wptr[0] = TCPOPT_MAXSEG;
			wptr[1] = TCPOPT_MAXSEG_LEN;
			wptr += 2;
			/* Need to adjust tcp_mss to the max interface mss. */
			u1 = (tcp->tcp_snd_ts_ok == 0) ? tcp->tcp_mss:
			    (tcp->tcp_mss + TCPOPT_REAL_TS_LEN);
			U16_TO_BE16(u1, wptr);
			mp1->b_wptr = wptr + 2;

			/* Update the offset to cover the additional word */
			tcph->th_offset_and_rsrvd[0] += (1 << 4);

			/* allocb() of adequate mblk assures space */
			ASSERT((uintptr_t)(mp1->b_wptr - mp1->b_rptr) <=
			    (uintptr_t)INT_MAX);
			u1 = (int)(mp1->b_wptr - mp1->b_rptr);
			/*
			 * Get IP set to checksum on our behalf
			 * Include the adjustment for a source route if any.
			 */
			u1 += tcp->tcp_sum;
			u1 = (u1 >> 16) + (u1 & 0xFFFF);
			U16_TO_BE16(u1, tcph->th_sum);
			if (tcp->tcp_state < TCPS_ESTABLISHED)
				flags |= TH_SYN;
			if (flags & TH_SYN)
				BUMP_MIB(tcp_mib.tcpOutControl);
		}
		if ((tcp->tcp_valid_bits & TCP_FSS_VALID) &&
		    (seq + data_length) == tcp->tcp_fss) {
			if (!tcp->tcp_fin_acked) {
				flags |= TH_FIN;
				BUMP_MIB(tcp_mib.tcpOutControl);
			}
			if (!tcp->tcp_fin_sent) {
				tcp->tcp_fin_sent = true;
				switch (tcp->tcp_state) {
				case TCPS_SYN_RCVD:
				case TCPS_ESTABLISHED:
					tcp->tcp_state = TCPS_FIN_WAIT_1;
					break;
				case TCPS_CLOSE_WAIT:
					tcp->tcp_state = TCPS_LAST_ACK;
					break;
				}
				if (tcp->tcp_suna == tcp->tcp_snxt)
					TCP_TIMER_RESTART(tcp, tcp->tcp_rto);
				tcp->tcp_snxt = tcp->tcp_fss + 1;
			}
		}
		u1 = tcp->tcp_urg - seq + TCP_OLD_URP_INTERPRETATION;
		if ((tcp->tcp_valid_bits & TCP_URG_VALID) &&
		    u1 < (uint32_t)(64 * 1024)) {
			flags |= TH_URG;
			BUMP_MIB(tcp_mib.tcpOutUrg);
			U32_TO_ABE16(u1, tcph->th_urp);
		}
	}
	tcph->th_flags[0] = (u_char)flags;
	tcp->tcp_rack = tcp->tcp_rnxt;
	tcp->tcp_rack_cnt = 0;

	if (tcp->tcp_snd_ts_ok) {
		if (tcp->tcp_state != TCPS_SYN_SENT) {
			uint32_t llbolt = (uint32_t)lbolt;

			U32_TO_BE32(llbolt,
			    (char *)tcph+TCP_MIN_HEADER_LENGTH+4);
			U32_TO_BE32(tcp->tcp_ts_recent,
			    (char *)tcph+TCP_MIN_HEADER_LENGTH+8);
		}
	}

	if (num_sack_blk > 0) {
		u_char *wptr = (u_char *)tcph + tcp->tcp_tcp_hdr_len;
		sack_blk_t *tmp;
		int32_t	i;

		wptr[0] = TCPOPT_NOP;
		wptr[1] = TCPOPT_NOP;
		wptr[2] = TCPOPT_SACK;
		wptr[3] = TCPOPT_HEADER_LEN + num_sack_blk *
		    sizeof (sack_blk_t);
		wptr += TCPOPT_REAL_SACK_LEN;

		tmp = tcp->tcp_sack_list;
		for (i = 0; i < num_sack_blk; i++) {
			U32_TO_BE32(tmp[i].begin, wptr);
			wptr += sizeof (tcp_seq);
			U32_TO_BE32(tmp[i].end, wptr);
			wptr += sizeof (tcp_seq);
		}
		tcph->th_offset_and_rsrvd[0] += ((num_sack_blk * 2 + 1) << 4);
	}
	ASSERT((uintptr_t)(mp1->b_wptr - rptr) <= (uintptr_t)INT_MAX);
	data_length += (int)(mp1->b_wptr - rptr);
	U16_TO_ABE16(data_length, ((iph_t *)rptr)->iph_length);

	/*
	 * Prime pump for IP
	 * Include the adjustment for a source route if any.
	 */
	data_length -= tcp->tcp_ip_hdr_len;
	data_length += tcp->tcp_sum;
	data_length = (data_length >> 16) + (data_length & 0xFFFF);
	U16_TO_ABE16(data_length, tcph->th_sum);
	return (mp1);
}


/*
 * This function handles delayed ACK timeout.
 */
static void
tcp_ack_timer(tcp_t *tcp)
{
	mblk_t *mp;
	clock_t time_gap;

	/* The timer is also set to drain the received (push) queue.  */
	if ((mp = tcp->tcp_rcv_head) != NULL &&
	    tcp->tcp_co_head == NULL &&
	    tcp->tcp_listener == NULL) {
		putnext(tcp->tcp_rq, mp);
		tcp->tcp_co_norm = 1;
		tcp->tcp_rcv_head = NULL;
		tcp->tcp_rcv_cnt = 0;
	}

	tcp->tcp_ack_timer_running = 0;
	/*
	 * Do not send ACK if there is no outstanding unack'ed data.
	 */
	if (tcp->tcp_rnxt == tcp->tcp_rack) {
		return;
	}

	/*
	 * Because we do not reset the timer, check to see when the
	 * last delayed ACK timer was set.  If it was just set recently,
	 * restart a timer.
	 */
	if ((time_gap = TICK_TO_MSEC(lbolt - tcp->tcp_dack_set_time)) <
	    tcp_deferred_ack_interval) {
		mi_timer(tcp->tcp_wq, tcp->tcp_ack_mp,
		    (clock_t)tcp_deferred_ack_interval - time_gap);
		tcp->tcp_ack_timer_running = 1;
		return;
	}

	if ((tcp->tcp_rnxt - tcp->tcp_rack) > tcp->tcp_mss) {
		/*
		 * Make sure we don't allow deferred ACKs to result in
		 * timer-based ACKing.  If we have held off an ACK
		 * when there was more than an mss here, and the timer
		 * goes off, we have to worry about the possibility
		 * that the sender isn't doing slow-start, or is out
		 * of step with us for some other reason.  We fall
		 * permanently back in the direction of
		 * ACK-every-other-packet as suggested in RFC 1122.
		 */
		if (tcp->tcp_rack_abs_max > (tcp->tcp_mss << 1))
			tcp->tcp_rack_abs_max -= tcp->tcp_mss;
		tcp->tcp_rack_cur_max = tcp->tcp_mss << 1;
	}
	mp = tcp_ack_mp(tcp);
	if (mp) {
		putnext(tcp->tcp_wq, mp);
		BUMP_LOCAL(tcp->tcp_obsegs);
		BUMP_MIB(tcp_mib.tcpOutAck);
		BUMP_MIB(tcp_mib.tcpOutAckDelayed);
	}
}


/* Generate an ACK-only (no data) segment for a TCP endpoint */
static mblk_t *
tcp_ack_mp(tcp_t *tcp)
{

	if (tcp->tcp_valid_bits) {
		/*
		 * For the complex case where we have to send some
		 * controls (FIN or SYN), let tcp_xmit_mp do it.
		 * When sending an ACK-only segment (no data)
		 * into a zero window, always set the seq number to
		 * suna, since snxt will be extended past the window.
		 * If we used snxt, the receiver might consider the ACK
		 * unacceptable.
		 */
		return (tcp_xmit_mp(tcp, nilp(mblk_t), 0, NULL, NULL,
		    (tcp->tcp_zero_win_probe) ?
		    tcp->tcp_suna :
		    tcp->tcp_snxt, 0));
	} else {
		/* Generate a simple ACK */
		int	data_length;
		u_char	*rptr;
		tcph_t	*tcph;
		mblk_t	*mp1;
		int32_t	tcp_hdr_len;
		int32_t	tcp_tcp_hdr_len;
		int32_t	num_sack_blk = 0;
		int32_t sack_opt_len;

		/*
		 * Allocate space for TCP + IP headers
		 * and link-level header
		 */
		if (tcp->tcp_snd_sack_ok && tcp->tcp_num_sack_blk > 0) {
			num_sack_blk = MIN(tcp->tcp_max_sack_blk,
			    tcp->tcp_num_sack_blk);
			sack_opt_len = num_sack_blk * sizeof (sack_blk_t) +
			    TCPOPT_NOP_LEN * 2 + TCPOPT_HEADER_LEN;
			tcp_hdr_len = tcp->tcp_hdr_len + sack_opt_len;
			tcp_tcp_hdr_len = tcp->tcp_tcp_hdr_len + sack_opt_len;
		} else {
			tcp_hdr_len = tcp->tcp_hdr_len;
			tcp_tcp_hdr_len = tcp->tcp_tcp_hdr_len;
		}
		mp1 = allocb(tcp_hdr_len + tcp_wroff_xtra, BPRI_MED);
		if (!mp1)
			return (nilp(mblk_t));

		/* copy in prototype TCP + IP header */
		rptr = mp1->b_rptr + tcp_wroff_xtra;
		mp1->b_rptr = rptr;
		mp1->b_wptr = rptr + tcp_hdr_len;
		bcopy(tcp->tcp_iphc, rptr, tcp->tcp_hdr_len);

		tcph = (tcph_t *)&rptr[tcp->tcp_ip_hdr_len];

		/*
		 * Set the TCP sequence number.
		 * When sending an ACK-only segment (no data)
		 * into a zero window, always set the seq number to
		 * suna, since snxt will be extended past the window.
		 * If we used snxt, the receiver might consider the ACK
		 * unacceptable.
		 */
		U32_TO_ABE32((tcp->tcp_zero_win_probe) ?
		    tcp->tcp_suna : tcp->tcp_snxt, tcph->th_seq);

		/* set the TCP ACK flag */
		tcph->th_flags[0] = (u_char) TH_ACK;
		tcp->tcp_rack = tcp->tcp_rnxt;
		tcp->tcp_rack_cnt = 0;

		/* fill in timestamp option if in use */
		if (tcp->tcp_snd_ts_ok) {
			uint32_t llbolt = (uint32_t)lbolt;

			U32_TO_BE32(llbolt,
			    (char *)tcph+TCP_MIN_HEADER_LENGTH+4);
			U32_TO_BE32(tcp->tcp_ts_recent,
			    (char *)tcph+TCP_MIN_HEADER_LENGTH+8);
		}

		/* Fill in SACK options */
		if (num_sack_blk > 0) {
			u_char *wptr = (u_char *)tcph + tcp->tcp_tcp_hdr_len;
			sack_blk_t *tmp;
			int32_t	i;

			wptr[0] = TCPOPT_NOP;
			wptr[1] = TCPOPT_NOP;
			wptr[2] = TCPOPT_SACK;
			wptr[3] = TCPOPT_HEADER_LEN + num_sack_blk *
			    sizeof (sack_blk_t);
			wptr += TCPOPT_REAL_SACK_LEN;

			tmp = tcp->tcp_sack_list;
			for (i = 0; i < num_sack_blk; i++) {
				U32_TO_BE32(tmp[i].begin, wptr);
				wptr += sizeof (tcp_seq);
				U32_TO_BE32(tmp[i].end, wptr);
				wptr += sizeof (tcp_seq);
			}
			tcph->th_offset_and_rsrvd[0] += ((num_sack_blk * 2 + 1)
			    << 4);
		}

		/*
		 * set IP total length field equal to
		 * size of TCP + IP headers.
		 */
		U16_TO_ABE16(tcp_hdr_len, ((iph_t *)rptr)->iph_length);

		/*
		 * Prime pump for checksum calculation in IP.  Include the
		 * adjustment for a source route if any.
		 */
		data_length = tcp_tcp_hdr_len + tcp->tcp_sum;
		data_length = (data_length >> 16) + (data_length & 0xFFFF);
		U16_TO_ABE16(data_length, tcph->th_sum);

		return (mp1);
	}
}


/*
 * Hash list insertion routine for tcp_t structures.
 * Inserts entries with the ones bound to a specific IP address first
 * followed by those bound to INADDR_ANY.
 */
static void
tcp_bind_hash_insert(tf_t *tf, tcp_t *tcp, int caller_holds_lock)
{
	tcp_t	**tcpp;
	tcp_t	*tcpnext;

	if (tcp->tcp_ptpbhn != NULL) {
		ASSERT(!caller_holds_lock);
		tcp_bind_hash_remove(tcp);
	}
	tcpp = &tf->tf_tcp;
	if (!caller_holds_lock)
		mutex_enter(&tf->tf_lock);
	else
		ASSERT(MUTEX_HELD(&tf->tf_lock));
	tcpnext = tcpp[0];
	if (tcpnext) {
		/*
		 * If the new tcp bound to the INADDR_ANY address
		 * and the first one in the list is not bound to
		 * INADDR_ANY we skip all entries until we find the
		 * first one bound to INADDR_ANY.
		 * This makes sure that applications binding to a
		 * specific address get preference over those binding to
		 * INADDR_ANY.
		 */
		if (tcp->tcp_bound_source == INADDR_ANY &&
		    tcpnext->tcp_bound_source != INADDR_ANY) {
			while ((tcpnext = tcpp[0]) != NULL &&
			    tcpnext->tcp_bound_source != INADDR_ANY)
				tcpp = &(tcpnext->tcp_bind_hash);
			if (tcpnext)
				tcpnext->tcp_ptpbhn = &tcp->tcp_bind_hash;
		} else
			tcpnext->tcp_ptpbhn = &tcp->tcp_bind_hash;
	}
	tcp->tcp_bind_hash = tcpnext;
	tcp->tcp_ptpbhn = tcpp;
	tcpp[0] = tcp;
	tcp->tcp_bind_lockp = &tf->tf_lock;	/* For tcp_*_hash_remove */
	if (!caller_holds_lock)
		mutex_exit(&tf->tf_lock);
}

/*
 * Hash list removal routine for tcp_t structures.
 */
static void
tcp_bind_hash_remove(tcp_t *tcp)
{
	tcp_t	*tcpnext;
	kmutex_t *lockp;

	/*
	 * Extract the lock pointer in case there are concurrent
	 * hash_remove's for this instance.
	 */
	lockp = tcp->tcp_bind_lockp;

	if (tcp->tcp_ptpbhn == NULL)
		return;

	ASSERT(lockp != NULL);
	mutex_enter(lockp);
	if (tcp->tcp_ptpbhn) {
		tcpnext = tcp->tcp_bind_hash;
		if (tcpnext) {
			tcpnext->tcp_ptpbhn = tcp->tcp_ptpbhn;
			tcp->tcp_bind_hash = nilp(tcp_t);
		}
		*tcp->tcp_ptpbhn = tcpnext;
		tcp->tcp_ptpbhn = nilp(tcp_t *);
	}
	mutex_exit(lockp);
	tcp->tcp_bind_lockp = NULL;
}

/*
 * Hash list insertion routine for tcp_t structures.
 * Inserts entries with the ones bound to a specific IP address first
 * followed by those bound to INADDR_ANY.
 */
static void
tcp_listen_hash_insert(tf_t *tf, tcp_t *tcp)
{
	tcp_t	**tcpp;
	tcp_t	*tcpnext;

	if (tcp->tcp_ptplhn != NULL)
		tcp_listen_hash_remove(tcp);
	tcpp = &tf->tf_tcp;
	mutex_enter(&tf->tf_lock);
	tcpnext = tcpp[0];
	if (tcpnext) {
		/*
		 * If the new tcp bound to the INADDR_ANY address
		 * and the first one in the list is not bound to
		 * INADDR_ANY we skip all entries until we find the
		 * first one bound to INADDR_ANY.
		 * This makes sure that applications binding to a
		 * specific address get preference over those binding to
		 * INADDR_ANY.
		 */
		if (tcp->tcp_bound_source == INADDR_ANY &&
		    tcpnext->tcp_bound_source != INADDR_ANY) {
			while ((tcpnext = tcpp[0]) != NULL &&
			    tcpnext->tcp_bound_source != INADDR_ANY)
				tcpp = &(tcpnext->tcp_listen_hash);
			if (tcpnext)
				tcpnext->tcp_ptplhn = &tcp->tcp_listen_hash;
		} else
			tcpnext->tcp_ptplhn = &tcp->tcp_listen_hash;
	}
	tcp->tcp_listen_hash = tcpnext;
	tcp->tcp_ptplhn = tcpp;
	tcpp[0] = tcp;
	tcp->tcp_listen_lockp = &tf->tf_lock;	/* For tcp_*_hash_remove */
	mutex_exit(&tf->tf_lock);
}

/*
 * Hash list removal routine for tcp_t structures.
 */
static void
tcp_listen_hash_remove(tcp_t *tcp)
{
	tcp_t	*tcpnext;
	kmutex_t *lockp;

	/*
	 * Extract the lock pointer in case there are concurrent
	 * hash_remove's for this instance.
	 */
	lockp = tcp->tcp_listen_lockp;

	if (tcp->tcp_ptplhn == NULL)
		return;

	ASSERT(lockp != NULL);
	mutex_enter(lockp);
	if (tcp->tcp_ptplhn) {
		tcpnext = tcp->tcp_listen_hash;
		if (tcpnext) {
			tcpnext->tcp_ptplhn = tcp->tcp_ptplhn;
			tcp->tcp_listen_hash = nilp(tcp_t);
		}
		*tcp->tcp_ptplhn = tcpnext;
		tcp->tcp_ptplhn = nilp(tcp_t *);
	}
	mutex_exit(lockp);
	tcp->tcp_listen_lockp = NULL;
}

/*
 * Hash list insertion routine for tcp_t structures.
 */
static void
tcp_conn_hash_insert(tf_t *tf, tcp_t *tcp, int caller_holds_lock)
{
	tcp_t	**tcpp;
	tcp_t	*tcpnext;

	if (tcp->tcp_ptpchn != NULL) {
		ASSERT(!caller_holds_lock);
		tcp_conn_hash_remove(tcp);
	}
	tcpp = &tf->tf_tcp;
	if (!caller_holds_lock)
		mutex_enter(&tf->tf_lock);
	else
		ASSERT(MUTEX_HELD(&tf->tf_lock));
	tcpnext = tcpp[0];
	if (tcpnext)
		tcpnext->tcp_ptpchn = &tcp->tcp_conn_hash;
	tcp->tcp_conn_hash = tcpnext;
	tcp->tcp_ptpchn = tcpp;
	tcpp[0] = tcp;
	tcp->tcp_conn_lockp = &tf->tf_lock;	/* For tcp_*_hash_remove */
	if (!caller_holds_lock)
		mutex_exit(&tf->tf_lock);
}

/*
 * Hash list removal routine for tcp_t structures.
 */
static void
tcp_conn_hash_remove(tcp_t *tcp)
{
	tcp_t	*tcpnext;
	kmutex_t *lockp;

	/*
	 * Extract the lock pointer in case there are concurrent
	 * hash_remove's for this instance.
	 */
	lockp = tcp->tcp_conn_lockp;

	if (tcp->tcp_ptpchn == NULL)
		return;

	ASSERT(lockp != NULL);
	mutex_enter(lockp);
	if (tcp->tcp_ptpchn) {
		tcpnext = tcp->tcp_conn_hash;
		if (tcpnext) {
			tcpnext->tcp_ptpchn = tcp->tcp_ptpchn;
			tcp->tcp_conn_hash = nilp(tcp_t);
		}
		*tcp->tcp_ptpchn = tcpnext;
		tcp->tcp_ptpchn = nilp(tcp_t *);
	}
	mutex_exit(lockp);
	tcp->tcp_conn_lockp = NULL;
}

/*
 * Hash list lookup routine for tcp_t structures.
 * Returns with a TCP_REFHOLD tcp structure. Caller must do a TCP_REFRELE.
 */
static	tcp_t *
tcp_acceptor_hash_lookup(t_uscalar_t id)
{
	tf_t	*tf;
	tcp_t	*tcp;

	tf = &tcp_acceptor_fanout[TCP_ACCEPTOR_HASH(id)];
	mutex_enter(&tf->tf_lock);
	for (tcp = tf->tf_tcp; tcp != nilp(tcp_t);
	    tcp = tcp->tcp_acceptor_hash) {
		if (tcp->tcp_acceptor_id == id) {
			TCP_REFHOLD(tcp);
			mutex_exit(&tf->tf_lock);
			return (tcp);
		}
	}
	mutex_exit(&tf->tf_lock);
	return (NULL);
}


/*
 * Hash list insertion routine for tcp_t structures.
 */
static void
tcp_acceptor_hash_insert(t_uscalar_t id, tcp_t *tcp)
{
	tf_t	*tf;
	tcp_t	**tcpp;
	tcp_t	*tcpnext;

	tf = &tcp_acceptor_fanout[TCP_ACCEPTOR_HASH(id)];

	if (tcp->tcp_ptpahn != NULL)
		tcp_acceptor_hash_remove(tcp);
	tcpp = &tf->tf_tcp;
	mutex_enter(&tf->tf_lock);
	tcpnext = tcpp[0];
	if (tcpnext)
		tcpnext->tcp_ptpahn = &tcp->tcp_acceptor_hash;
	tcp->tcp_acceptor_hash = tcpnext;
	tcp->tcp_ptpahn = tcpp;
	tcpp[0] = tcp;
	tcp->tcp_acceptor_lockp = &tf->tf_lock;	/* For tcp_*_hash_remove */
	mutex_exit(&tf->tf_lock);
}

/*
 * Hash list removal routine for tcp_t structures.
 */
static void
tcp_acceptor_hash_remove(tcp_t *tcp)
{
	tcp_t	*tcpnext;
	kmutex_t *lockp;

	/*
	 * Extract the lock pointer in case there are concurrent
	 * hash_remove's for this instance.
	 */
	lockp = tcp->tcp_acceptor_lockp;

	if (tcp->tcp_ptpahn == NULL)
		return;

	ASSERT(lockp != NULL);
	mutex_enter(lockp);
	if (tcp->tcp_ptpahn) {
		tcpnext = tcp->tcp_acceptor_hash;
		if (tcpnext) {
			tcpnext->tcp_ptpahn = tcp->tcp_ptpahn;
			tcp->tcp_acceptor_hash = nilp(tcp_t);
		}
		*tcp->tcp_ptpahn = tcpnext;
		tcp->tcp_ptpahn = nilp(tcp_t *);
	}
	mutex_exit(lockp);
	tcp->tcp_acceptor_lockp = NULL;
}

/*
 * KLUDGE ALERT: the following code needs to disappear in the future, its
 *		 functionality needs to be moved into the appropriate STREAMS
 *		 frame work file. This code makes assumptions based on the
 *		 current implementation of Synchronous STREAMS.
 */

/*
 * Send an M_IOCTL of I_SYNCSTR up the read-side, when it comes back down
 * the write-side wput() will clear the co_norm bit and free the mblk.
 */
static int
struio_ioctl(queue_t *rq, mblk_t *mp)
{
	/*
	 * The mblk_t passed in was allocated during initialization of this
	 * tcp connection and is reused whenever it is necessary to resync
	 * the streams, it goes up as M_IOCTL and comes back down as M_IOCNAK.
	 */
	ASSERT(mp != NULL &&
	    ((struct iocblk *)mp->b_rptr)->ioc_cmd == I_SYNCSTR);

	mp->b_datap->db_type = M_IOCTL;

	putnext(rq, mp);
	return (1);
}

/*
 * Function clears the appropriate bit in sd_wakeq set in strwakeq().
 */
static void
strwakeqclr(queue_t *q, int flag)
{
	stdata_t *stp = STREAM(q);

	mutex_enter(&stp->sd_lock);
	if (flag & QWANTWSYNC)
		stp->sd_wakeq &= ~WSLEEP;
	else if (flag & QWANTR)
		stp->sd_wakeq &= ~RSLEEP;
	else if (flag & QWANTW)
		stp->sd_wakeq &= ~WSLEEP;
	mutex_exit(&stp->sd_lock);
}

static char *
tcp_addr_sprintf(char *c, uint8_t *addr)
{
	(void) sprintf(c, "%03d.%03d.%03d.%03d",
	    addr[0], addr[1], addr[2], addr[3]);
	return (c);
}

/* Set callback routine passed to nd_load by tcp_param_register. */
/* ARGSUSED */
static int
tcp_host_param_set(queue_t *q, mblk_t *mp, char *value, caddr_t cp)
{
	int i;
	int byte;
	int error = 0;
	char *end;

	tcp_hsp_t *hsp;
	tcp_hsp_t *hspprev;

	ipaddr_t addr = 0;		/* Address we're looking for */
	uint32_t hash;			/* Hash of that address */

	/*
	 * If the following variables are still zero after parsing the input
	 * string, the user didn't specify them and we don't change them in
	 * the HSP.
	 */

	ipaddr_t mask = 0;		/* Subnet mask */
	int sendspace = 0;		/* Send buffer size */
	int recvspace = 0;		/* Receive buffer size */
	int timestamp = 0;	/* Originate TCP TSTAMP option, 1 = yes */
	int delete = 0;			/* User asked to delete this HSP */


	rw_enter(&tcp_hsp_lock, RW_WRITER);

	/* Parse and validate address */

	for (i = 0; i < 4; i++) {
		byte = mi_strtol(value, &end, 10);
		if (byte < 0 || byte > 255) {
			error = EINVAL;
			goto done;
		}
		addr = (addr << 8) | byte;
		if (i < 3) {
			if (*end != '.') {
				error = EINVAL;
				goto done;
			}
			else
				value = end+1;
		}
		else
			value = end;
	}

	/* Parse individual keywords, set variables if found */

	while (*value) {
		/* Skip leading blanks */

		while (*value == ' ' || *value == '\t')
			value++;

		/* If at end of string, we're done */

		if (!*value)
			break;

		/* We have a word, figure out what it is */

		if (strncmp("mask", value, 4) == 0) {
			value += 4;

			/* Parse subnet mask */

			for (i = 0; i < 4; i++) {
				byte = mi_strtol(value, &end, 10);
				if (byte < 0 || byte > 255) {
					error = EINVAL;
					goto done;
				}
				mask = (mask << 8) | byte;
				if (i < 3) {
					if (*end != '.') {
						error = EINVAL;
						goto done;
					}
					else
						value = end+1;
				}
				else
					value = end;
			}
		} else if (strncmp("sendspace", value, 9) == 0) {
			value += 9;

			sendspace = mi_strtol(value, &end, 0);
			if (sendspace < TCP_XMIT_HIWATER ||
			    sendspace >= (1L<<30)) {
				error = EINVAL;
				goto done;
			}
			value = end;
		} else if (strncmp("recvspace", value, 9) == 0) {
			value += 9;

			recvspace = mi_strtol(value, &end, 0);
			if (recvspace < TCP_RECV_HIWATER ||
			    recvspace >= (1L<<30)) {
				error = EINVAL;
				goto done;
			}
			value = end;
		} else if (strncmp("timestamp", value, 9) == 0) {
			value += 9;

			timestamp = mi_strtol(value, &end, 0);
			if (timestamp < 0 || timestamp > 1) {
				error = EINVAL;
				goto done;
			}

			/*
			 * We increment timestamp so we know it's been set;
			 * this is undone when we put it in the HSP
			 */
			timestamp++;
			value = end;
		} else if (strncmp("delete", value, 6) == 0) {
			value += 6;
			delete = 1;
		} else {
			error = EINVAL;
			goto done;
		}
	}

	/* Hash address for lookup */

	hash = TCP_HSP_HASH(addr);

	if (delete) {

		/*
		 * Note that deletes don't return an error if the thing
		 * we're trying to delete isn't there.
		 */

		if (tcp_hsp_hash) {
			hsp = tcp_hsp_hash[hash];

			if (hsp) {
				if (hsp->tcp_hsp_addr == addr) {
					tcp_hsp_hash[hash] = hsp->tcp_hsp_next;
					mi_free((char *)hsp);
				} else {
					hspprev = hsp;
					while ((hsp = hsp->tcp_hsp_next) !=
					    nilp(tcp_hsp_t)) {
						if (hsp->tcp_hsp_addr == addr) {
							hspprev->tcp_hsp_next =
							    hsp->tcp_hsp_next;
							mi_free((char *)hsp);
							break;
						}
						hspprev = hsp;
					}
				}
			}
		}

	} else {

		/*
		 * We're adding/modifying an HSP.  If we haven't already done
		 * so, allocate the hash table.
		 */

		if (!tcp_hsp_hash) {
			tcp_hsp_hash = (tcp_hsp_t **)
			    mi_zalloc(sizeof (tcp_hsp_t *) * TCP_HSP_HASH_SIZE);
			if (!tcp_hsp_hash) {
				error = EINVAL;
				goto done;
			}
		}

		/* Get head of hash chain */

		hsp = tcp_hsp_hash[hash];

		/* Try to find pre-existing hsp on hash chain */

		while (hsp) {
			if (hsp->tcp_hsp_addr == addr)
				break;
			hsp = hsp->tcp_hsp_next;
		}

		/*
		 * If we didn't, create one with default values and put it
		 * at head of hash chain
		 */

		if (!hsp) {
			hsp = (tcp_hsp_t *)mi_zalloc(sizeof (tcp_hsp_t));
			if (!hsp) {
				error = EINVAL;
				goto done;
			}
			hsp->tcp_hsp_next = tcp_hsp_hash[hash];
			tcp_hsp_hash[hash] = hsp;
		}

		/* Set values that the user asked us to change */

		hsp->tcp_hsp_addr = addr;
		if (mask)
			hsp->tcp_hsp_subnet = mask;
		if (sendspace)
			hsp->tcp_hsp_sendspace = sendspace;
		if (recvspace)
			hsp->tcp_hsp_recvspace = recvspace;
		if (timestamp)
			hsp->tcp_hsp_tstamp = timestamp - 1;
	}

done:
	rw_exit(&tcp_hsp_lock);
	return (error);
}

/* TCP host parameters report triggered via the Named Dispatch mechanism. */
/* ARGSUSED */
static	int
tcp_host_param_report(queue_t *q, mblk_t *mp, caddr_t cp)
{
	tcp_hsp_t *hsp;
	int i;
	char addrbuf[16], subnetbuf[16];

	rw_enter(&tcp_hsp_lock, RW_READER);
	(void) mi_mpprintf(mp, "Hash HSP     "
#ifdef _LP64
	    "        "
#endif
	    "Address         Subnet Mask     Send       Receive    TStamp");
	if (tcp_hsp_hash) {
		for (i = 0; i < TCP_HSP_HASH_SIZE; i++) {
			hsp = tcp_hsp_hash[i];
			while (hsp) {
				(void) mi_mpprintf(mp, " %03d %0lx"
				    " %s %s %010d %010d      %d",
				    i,
				    hsp,
				    tcp_addr_sprintf(addrbuf,
					(uint8_t *)&hsp->tcp_hsp_addr),
				    tcp_addr_sprintf(subnetbuf,
					(uint8_t *)&hsp->tcp_hsp_subnet),
				    hsp->tcp_hsp_sendspace,
				    hsp->tcp_hsp_recvspace,
				    hsp->tcp_hsp_tstamp);

				hsp = hsp->tcp_hsp_next;
			}
		}
	}
	rw_exit(&tcp_hsp_lock);
	return (0);
}


/* Data for fast netmask macro used by tcp_hsp_lookup */

static ipaddr_t netmasks[] = {
	IN_CLASSA_NET, IN_CLASSA_NET, IN_CLASSB_NET,
	IN_CLASSC_NET | IN_CLASSD_NET  /* Class C,D,E */
};

#define	netmask(addr) (netmasks[(ipaddr_t)(addr) >> 30])

/*
 * XXX This routine should go away and instead we should use the metrics
 * associated with the routes to determine the default sndspace and rcvspace.
 */
static tcp_hsp_t *
tcp_hsp_lookup(ipaddr_t addr)
{
	tcp_hsp_t *hsp = nilp(tcp_hsp_t);

	/* Quick check without acquiring the lock. */
	if (tcp_hsp_hash == NULL)
		return (NULL);

	rw_enter(&tcp_hsp_lock, RW_READER);

	/* This routine finds the best-matching HSP for address addr. */

	if (tcp_hsp_hash) {
		int i;
		ipaddr_t srchaddr;
		tcp_hsp_t *hsp_net;

		/* We do three passes: host, network, and subnet. */

		srchaddr = addr;

		for (i = 1; i <= 3; i++) {
			/* Look for exact match on srchaddr */

			hsp = tcp_hsp_hash[TCP_HSP_HASH(srchaddr)];
			while (hsp) {
				if (hsp->tcp_hsp_addr == srchaddr)
					break;
				hsp = hsp->tcp_hsp_next;
			}

			/*
			 * If this is the first pass:
			 *   If we found a match, great, return it.
			 *   If not, search for the network on the second pass.
			 */

			if (i == 1)
				if (hsp)
					break;
				else
				{
					srchaddr = addr & netmask(addr);
					continue;
				}

			/*
			 * If this is the second pass:
			 *   If we found a match, but there's a subnet mask,
			 *    save the match but try again using the subnet
			 *    mask on the third pass.
			 *   Otherwise, return whatever we found.
			 */

			if (i == 2)
				if (hsp && hsp->tcp_hsp_subnet) {
					hsp_net = hsp;
					srchaddr = addr & hsp->tcp_hsp_subnet;
					continue;
				}
				else
					break;

			/*
			 * This must be the third pass.  If we didn't find
			 * anything, return the saved network HSP instead.
			 */

			if (!hsp)
				hsp = hsp_net;
		}
	}

	rw_exit(&tcp_hsp_lock);
	return (hsp);
}

/*
 * Type three generator adapted from the random() function in 4.4 BSD:
 */

/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* Type 3 -- x**31 + x**3 + 1 */
#define	DEG_3		31
#define	SEP_3		3


/* Protected by tcp_random_lock */
static int tcp_randtbl[DEG_3 + 1];

static int *tcp_random_fptr = &tcp_randtbl[SEP_3 + 1];
static int *tcp_random_rptr = &tcp_randtbl[1];

static int *tcp_random_state = &tcp_randtbl[1];
static int *tcp_random_end_ptr = &tcp_randtbl[DEG_3 + 1];

kmutex_t tcp_random_lock;

void
tcp_random_init(void)
{
	int i;
	hrtime_t hrt;

	/*
	 * Use high-res timer for seed.  Gethrtime() returns a longlong,
	 * which may contain resolution down to nanoseconds.  Convert
	 * this to a 32-bit value by multiplying the high-order 32-bits by
	 * the low-order 32-bits.
	 */
	hrt = gethrtime();
	mutex_enter(&tcp_random_lock);
	tcp_random_state[0] = ((hrt >> 32) & 0xffffffff) * (hrt & 0xffffffff);

	for (i = 1; i < DEG_3; i++)
		tcp_random_state[i] = 1103515245 * tcp_random_state[i - 1]
			+ 12345;
	tcp_random_fptr = &tcp_random_state[SEP_3];
	tcp_random_rptr = &tcp_random_state[0];
	mutex_exit(&tcp_random_lock);
	for (i = 0; i < 10 * DEG_3; i++)
		(void) tcp_random();
}

/*
 * tcp_random: Return a random number in the range [1 - (128K + 1)].
 * This range is selected to be approximately centered on TCP_ISS / 2,
 * and easy to compute. We get this value by generating a 32-bit random
 * number, selecting out the high-order 17 bits, and then adding one so
 * that we never return zero.
 */

int
tcp_random(void)
{
	int i;

	mutex_enter(&tcp_random_lock);
	*tcp_random_fptr += *tcp_random_rptr;

	/*
	 * The high-order bits are more random than the low-order bits,
	 * so we select out the high-order 17 bits and add one so that
	 * we never return zero.
	 */
	i = ((*tcp_random_fptr >> 15) & 0x1ffff) + 1;
	if (++tcp_random_fptr >= tcp_random_end_ptr) {
		tcp_random_fptr = tcp_random_state;
		++tcp_random_rptr;
	} else if (++tcp_random_rptr >= tcp_random_end_ptr)
		tcp_random_rptr = tcp_random_state;

	mutex_exit(&tcp_random_lock);
	return (i);
}
/*
 * XXX This will go away when TPI is extended to send
 * info reqs to sockfs/timod .....
 * Given a queue, set the max packet size for the write
 * side of the queue below stream head.  This value is
 * cached on the stream head.
 * Returns 1 on success, 0 otherwise.
 */
static int
setmaxps(queue_t *q, int maxpsz)
{
	struct stdata 	*stp;
	queue_t 	*wq;
	stp = STREAM(q);

	/*
	 * At this point change of a queue parameter is not allowed
	 * when a multiplexor is sitting on top.
	 */
	if (stp->sd_flag & STPLEX)
		return (0);

	wq = stp->sd_wrq->q_next;
	ASSERT(wq != NULL);
	(void) strqset(wq, QMAXPSZ, 0, maxpsz);
	return (1);
}

static int
tcp_conprim_opt_process(queue_t *q, mblk_t *mp, int *do_disconnectp,
    int *t_errorp, int *sys_errorp)
{
	tcp_t	*tcp;
	int retval;
	t_scalar_t *opt_lenp;
	t_scalar_t opt_offset;
	int prim_type;
	struct T_conn_req *tcreqp;
	struct T_conn_res *tcresp;

	tcp = (tcp_t *)q->q_ptr;

	prim_type = ((union T_primitives *)mp->b_rptr)->type;
	ASSERT(prim_type == T_CONN_REQ || prim_type == O_T_CONN_RES ||
	    prim_type == T_CONN_RES);

	switch (prim_type) {
	case T_CONN_REQ:
		tcreqp = (struct T_conn_req *)mp->b_rptr;
		opt_offset = tcreqp->OPT_offset;
		opt_lenp = (t_scalar_t *)&tcreqp->OPT_length;
		break;
	case O_T_CONN_RES:
	case T_CONN_RES:
		tcresp = (struct T_conn_res *)mp->b_rptr;
		opt_offset = tcresp->OPT_offset;
		opt_lenp = (t_scalar_t *)&tcresp->OPT_length;
		break;
	}

	*t_errorp = 0;
	*sys_errorp = 0;
	*do_disconnectp = 0;

	retval = tpi_optcom_buf(tcp->tcp_wq, mp, opt_lenp,
	    opt_offset, tcp->tcp_priv_stream, &tcp_opt_obj);

	switch (retval) {
	case OB_SUCCESS:
		return (0);
	case OB_BADOPT:
		*t_errorp = TBADOPT;
		break;
	case OB_NOMEM:
		*t_errorp = TSYSERR; *sys_errorp = ENOMEM;
		break;
	case OB_NOACCES:
		*t_errorp = TACCES;
		break;
	case OB_ABSREQ_FAIL:
		/*
		 * The connection request should get the local ack
		 * T_OK_ACK and then a T_DISCON_IND.
		 */
		*do_disconnectp = 1;
		break;
	case OB_INVAL:
		*t_errorp = TSYSERR; *sys_errorp = EINVAL;
		break;
	default:
		break;
	}
	return (-1);
}

/*
 * Split this function out so that if the secret changes, I'm okay.
 *
 * Initialize the tcp_iss_cookie and tcp_iss_key.
 */

#define	PASSWD_SIZE 16  /* MUST be multiple of 4 */

static void
tcp_iss_key_init(uint8_t *phrase, int len)
{
	struct {
		int32_t current_time;
		uint32_t randnum;
		uint16_t pad;
		uint8_t ether[6];
		uint8_t passwd[PASSWD_SIZE];
	} tcp_iss_cookie;
	time_t t;

	/*
	 * Start with the current absolute time.
	 */
	(void) drv_getparm(TIME, &t);
	tcp_iss_cookie.current_time = t;

	/*
	 * XXX - Need a more random number per RFC 1750, not this crap.
	 * OTOH, if what follows is pretty random, then I'm in better shape.
	 */
	tcp_iss_cookie.randnum = (uint32_t)(gethrtime() + tcp_random());
	tcp_iss_cookie.pad = 0x365c;  /* Picked from HMAC pad values. */

	/*
	 * The cpu_type_info is pretty non-random.  Ugggh.  It does serve
	 * as a good template.
	 */
	bcopy(&cpu_list->cpu_type_info, &tcp_iss_cookie.passwd,
	    min(PASSWD_SIZE, sizeof (cpu_list->cpu_type_info)));

	/*
	 * The pass-phrase.  Normally this is supplied by user-called NDD.
	 */
	bcopy(phrase, &tcp_iss_cookie.passwd, min(PASSWD_SIZE, len));

	/*
	 * See 4010593 if this section becomes a problem again,
	 * but the local ethernet address is useful here.
	 */
	(void) localetheraddr(NULL,
	    (struct ether_addr *)&tcp_iss_cookie.ether);

	/*
	 * Hash 'em all together.  The MD5Final is called per-connection.
	 */
	mutex_enter(&tcp_iss_key_lock);
	MD5Init(&tcp_iss_key);
	MD5Update(&tcp_iss_key, (u_char *)&tcp_iss_cookie,
	    sizeof (tcp_iss_cookie));
	mutex_exit(&tcp_iss_key_lock);
}

/*
 * Set the RFC 1948 pass phrase
 */
/* ARGSUSED */
static int
tcp_1948_phrase_set(queue_t *q, mblk_t *mp, char *value, caddr_t cp)
{
	/*
	 * Basically, value contains a new pass phrase.  Pass it along!
	 */
	tcp_iss_key_init((uint8_t *)value, strlen(value));
	return (0);
}

void
tcp_ddi_init(void)
{
	int i;

	/* Initialize locks */
	mutex_init(&tcp_mi_lock, NULL, MUTEX_DEFAULT, NULL);
	rw_init(&tcp_hsp_lock, NULL, RW_DEFAULT, NULL);
	mutex_init(&tcp_time_wait_lock, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&tcp_g_q_lock, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&tcp_random_lock, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&tcp_epriv_port_lock, NULL, MUTEX_DEFAULT, NULL);

	for (i = 0; i < A_CNT(tcp_bind_fanout); i++) {
		mutex_init(&tcp_bind_fanout[i].tf_lock, NULL,
		    MUTEX_DEFAULT, NULL);
	}

	for (i = 0; i < A_CNT(tcp_listen_fanout); i++) {
		mutex_init(&tcp_listen_fanout[i].tf_lock, NULL,
		    MUTEX_DEFAULT, NULL);
	}

	for (i = 0; i < A_CNT(tcp_acceptor_fanout); i++) {
		mutex_init(&tcp_acceptor_fanout[i].tf_lock, NULL,
		    MUTEX_DEFAULT, NULL);
	}

	/* initialize the random number generator */
	tcp_random_init();

	/*
	 * initialize RFC 1948 secret values.  This will probably be
	 * reset once by the boot scripts.
	 *
	 * Use NULL name, as the name is caught by the new lockstats.
	 *
	 * Initialize with some random, non-guessable string, like the
	 * global T_INFO_ACK.
	 */
	mutex_init(&tcp_iss_key_lock, NULL, MUTEX_DEFAULT, NULL);
	tcp_iss_key_init((uint8_t *)&tcp_g_t_info_ack,
	    sizeof (tcp_g_t_info_ack));

	/*
	 * Allocate the tcp_conn_fanout based on the tcp_conn_hash_size
	 * /etc/system variable.
	 * If the variable is not set in /etc/system we use the default 256.
	 * Note that 256 buckets performs well for up to
	 * few thousands of connections.
	 *
	 * The tcp_conn_fanout_size must be a power of two. For performance
	 * reasons we never let it be set below 256.
	 */
	if (tcp_conn_hash_size & (tcp_conn_hash_size - 1)) {
		/* Not a power of two. Round up to nearest power of two */
		for (i = 0; i < 31; i++) {
			if (tcp_conn_hash_size < (1 << i))
				break;
		}
		tcp_conn_hash_size = 1 << i;
	}
	if (tcp_conn_hash_size < TCP_CONN_HASH_SIZE) {
		tcp_conn_hash_size = TCP_CONN_HASH_SIZE;

		cmn_err(CE_CONT, "?tcp: using tcp_conn_hash_size = %d\n",
		    tcp_conn_hash_size);
	}

	tcp_conn_fanout_size = tcp_conn_hash_size;
	tcp_conn_fanout =
	    (tf_t *)kmem_zalloc(tcp_conn_fanout_size * sizeof (tf_t), KM_SLEEP);

	for (i = 0; i < tcp_conn_fanout_size; i ++) {
		mutex_init(&tcp_conn_fanout[i].tf_lock, NULL,
		    MUTEX_DEFAULT, NULL);
	}

	if (!tcp_g_nd) {
		if (!tcp_param_register(tcp_param_arr, A_CNT(tcp_param_arr))) {
			nd_free(&tcp_g_nd);
		}
	}
	if (tcp_g_q != NULL) {
		tcp_time_wait_tid = qtimeout(tcp_g_q,
		    tcp_time_wait_collector, NULL, drv_usectohz(1000000));
	}
	/*
	 * Note: To really walk the device tree you need the devinfo
	 * pointer to your device which is only available after probe/attach.
	 * The following is safe only because it uses ddi_root_node()
	 */
	zerocopy_prop = ddi_prop_get_int(DDI_DEV_T_ANY, ddi_root_node(),
	    DDI_PROP_DONTPASS, "zerocopy-capability", 0);
	tcp_max_optbuf_len = optcom_max_optbuf_len(tcp_opt_obj.odb_opt_des_arr,
	    tcp_opt_obj.odb_opt_arr_cnt);

}

void
tcp_ddi_destroy(void)
{
	int i;

	nd_free(&tcp_g_nd);

	for (i = 0; i < A_CNT(tcp_bind_fanout); i++) {
		mutex_destroy(&tcp_bind_fanout[i].tf_lock);
	}
	for (i = 0; i < A_CNT(tcp_listen_fanout); i++) {
		mutex_destroy(&tcp_listen_fanout[i].tf_lock);
	}
	for (i = 0; i < A_CNT(tcp_acceptor_fanout); i++) {
		mutex_destroy(&tcp_acceptor_fanout[i].tf_lock);
	}

	for (i = 0; i < tcp_conn_fanout_size; i++) {
		mutex_destroy(&tcp_conn_fanout[i].tf_lock);
	}
	kmem_free(tcp_conn_fanout, tcp_conn_fanout_size * sizeof (tcp_t *));
	tcp_conn_fanout = NULL;
	tcp_conn_fanout_size = 0;

	mutex_destroy(&tcp_iss_key_lock);
	mutex_destroy(&tcp_mi_lock);
	rw_destroy(&tcp_hsp_lock);
	mutex_destroy(&tcp_time_wait_lock);
	mutex_destroy(&tcp_g_q_lock);
	mutex_destroy(&tcp_random_lock);
	mutex_destroy(&tcp_epriv_port_lock);
}

/*
 * Generate ISS, taking into account NDD changes may happen halfway through.
 * (If the iss is not zero, set it.)
 */

static void
tcp_iss_init(tcp_t *tcp)
{
	hrtime_t hrt;
	MD5_CTX context;
	struct { uint32_t ports; ipaddr_t src; ipaddr_t dst; } arg;
	uint32_t answer[4];

	switch (tcp_strong_iss) {
	case 2:
		tcp_iss_incr_extra += ISS_INCR/2;
		mutex_enter(&tcp_iss_key_lock);
		context = tcp_iss_key;
		mutex_exit(&tcp_iss_key_lock);
		arg.ports = tcp->tcp_ports;
		arg.src = tcp->tcp_ipha.ipha_src;
		arg.dst = tcp->tcp_ipha.ipha_dst;
		MD5Update(&context, (u_char *)&arg, sizeof (arg));
		MD5Final((u_char *)answer, &context);
		answer[0] ^= answer[1] ^ answer[2] ^ answer[3];
		hrt = gethrtime();
		tcp->tcp_iss = hrt/ISS_NSEC_DIV + answer[0] +
		    tcp_iss_incr_extra;
		break;
	case 1:
		tcp_iss_incr_extra += tcp_random();
		hrt = gethrtime();
		tcp->tcp_iss = hrt/ISS_NSEC_DIV + tcp_iss_incr_extra;
		break;
	default:
		tcp_iss_incr_extra += ISS_INCR/2;
		tcp->tcp_iss = (u_int)hrestime.tv_sec * ISS_INCR +
		    tcp_iss_incr_extra;
		break;
	}
	tcp->tcp_valid_bits = TCP_ISS_VALID;
	tcp->tcp_fss = tcp->tcp_iss - 1;
	tcp->tcp_suna = tcp->tcp_iss;
	tcp->tcp_snxt = tcp->tcp_iss + 1;
	tcp->tcp_rexmit_nxt = tcp->tcp_snxt;
	tcp->tcp_csuna = tcp->tcp_snxt;
}
