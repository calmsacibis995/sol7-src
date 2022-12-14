/*
 * Copyright (c) 1992-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)ip.c	1.147	98/02/04 SMI"

#define	_IP_C

#include <sys/types.h>
#include <sys/stream.h>
#include <sys/dlpi.h>
#include <sys/stropts.h>
#include <sys/strsubr.h>
#include <sys/strlog.h>
#define	_SUN_TPI_VERSION 2
#include <sys/tihdr.h>
#include <sys/ddi.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>

#include <sys/systm.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/vtrace.h>
#include <sys/isa_defs.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/route.h>
#include <sys/sockio.h>
#include <netinet/in.h>
#include <net/if_dl.h>

#include <inet/common.h>
#include <inet/mi.h>
#include <inet/mib2.h>
#include <inet/nd.h>
#include <inet/arp.h>
#include <inet/snmpcom.h>
#include <sys/strick.h>

#include <netinet/igmp_var.h>
#include <inet/ip.h>
#include <inet/tcp.h>

/*
 * Synchronization notes:
 *
 * This modules uses a combination of a per-module STREAMS perimeter and
 * explicit locking. The perimeter is configured so that open, close, and
 * put procedures are hot i.e. can execute concurrently. Thus only the
 * service procedures and those places that explicitely use qwriter
 * (become_exclusive) will acquire exclusive access to
 * the IP perimeter.
 *
 * When the open and close procedures need exclusive access to IP (in the
 * non-performance critical cases) they qenable the write service procedure
 * and qwait until it is done.
 * When the put procedures need exclusive access to all of IP
 * we asynchronously pass a message to a procedure by invoking qwriter
 * ("become_exclusive") which will arrange to call the
 * routine only after all reader threads have exited the shared resource, and
 * the writer lock has been acquired.
 *
 * There are explicit locks in IP to handle:
 *  - the ip_g_head list maintained by mi_open_link() and friends
 *  - the reassembly data structures (one lock per hash bucket)
 *  - the hashes used to find the IP client streams (one lock per hash bucket)
 *  - the reference count in each IPC structure
 *  - the atomic increment of ire_atomic_ident
 *  - certain IGMP and multicast routing data structures
 *  etc.
 */


#define	ILL_FRAG_HASH(s, i) ((s ^ (i ^ (i >> 8))) % ILL_FRAG_HASH_TBL_COUNT)

#define	IP_TCP_CONN_HASH(ip_src, ports) \
	((unsigned)(ntohl(ip_src) ^ (ports >> 24) ^ (ports >> 16) \
	^ (ports >> 8) ^ ports) % A_CNT(ipc_tcp_conn_fanout))
#define	IP_TCP_LISTEN_HASH(lport)	\
	((unsigned)(((lport) >> 8) ^ (lport)) % A_CNT(ipc_tcp_listen_fanout))

#ifdef _BIG_ENDIAN
#define	IP_UDP_HASH(port)		((port) & 0xFF)
#else	/* _BIG_ENDIAN */
#define	IP_UDP_HASH(port)		(((uint16_t)(port)) >> 8)
#endif	/* _BIG_ENDIAN */

/*
 * Assumes that the caller passes in <fport, lport> as the uint32_t
 * parameter "ports".
 */
#define	IP_TCP_CONN_MATCH(ipc, ipha, ports)			\
	((ipc)->ipc_ports == (ports) &&				\
	    (ipc)->ipc_faddr == (ipha)->ipha_src &&		\
	    (ipc)->ipc_laddr == (ipha)->ipha_dst)

#define	IP_TCP_LISTEN_MATCH(ipc, lport, laddr)			\
	(((ipc)->ipc_lport == (lport)) &&			\
	    (((ipc)->ipc_laddr == 0) ||				\
	    ((ipc)->ipc_laddr == (laddr))))


/*
 * If ipc_faddr is non-zero check for a connected UDP socket.
 * This depends on the order of insertion in ip_bind() to ensure that
 * the most specific matches are first. Thus the insertion order in
 * the fanout buckets must be:
 *	1) Fully connected UDP sockets
 *	2) Bound to a local IP address
 *	3) Bound to INADDR_ANY
 */
#define	IP_UDP_MATCH(ipc, lport, laddr, fport, faddr)		\
	(((ipc)->ipc_lport == (lport)) &&			\
	    (((ipc)->ipc_laddr == 0) ||				\
	    (((ipc)->ipc_laddr == (laddr)) &&			\
	    (((ipc)->ipc_faddr == 0) || 			\
		((ipc)->ipc_faddr == (faddr) && (ipc)->ipc_fport == (fport))))))

#define	IS_SIMPLE_IPH(ipha)						\
	((ipha)->ipha_version_and_hdr_length == IP_SIMPLE_HDR_VERSION)

/* RFC1122 Conformance */
#define	IP_FORWARD_DEFAULT	IP_FORWARD_NEVER

/*
 * We don't use the iph in the following definition, but we might want
 * to some day.
 */
#define	IP_OK_TO_FORWARD_THRU(ipha, ire)				\
	(WE_ARE_FORWARDING)

#ifdef	_BIG_ENDIAN
#define	IP_HDR_CSUM_TTL_ADJUST	256
#define	IP_TCP_CSUM_COMP	IPPROTO_TCP
#define	IP_UDP_CSUM_COMP	IPPROTO_UDP
#else
#define	IP_HDR_CSUM_TTL_ADJUST	1
#define	IP_TCP_CSUM_COMP	(IPPROTO_TCP << 8)
#define	IP_UDP_CSUM_COMP	(IPPROTO_UDP << 8)
#endif

#define	TCP_MIN_HEADER_LENGTH		20
#define	TCP_CHECKSUM_OFFSET		16

#define	ILL_MAX_NAMELEN		63

#define	IRE_IS_LOCAL(ire)						\
	((ire) && ((ire)->ire_type & (IRE_LOCAL|IRE_LOOPBACK)))

/* Privilege check for an IP instance of unknown type.  wq is write side. */
#define	IS_PRIVILEGED_QUEUE(wq)						\
	((wq)->q_next ? (((ill_t *)(wq)->q_ptr)->ill_priv_stream) \
	    : (((ipc_t *)(wq)->q_ptr)->ipc_priv_stream))

/* Leave room for ip_newroute to tack on the src and target addresses */
#define	OK_RESOLVER_MP(mp)						\
	((mp) && ((mp)->b_wptr - (mp)->b_rptr) >= (2 * IP_ADDR_LEN))

#define	IPFT_EXTERNAL_ORIGIN	1	/* Marker destination */

/* Can't include these until after we have all the typedefs. */
#include <netinet/igmp.h>
#include <inet/ip_multi.h>
#include <inet/ip_if.h>
#include <inet/ip_ire.h>
#include <inet/ip_rts.h>
#include <inet/optcom.h>
#include <netinet/ip_mroute.h>

/*
 * Flags for the various ip_fanout_* routines.
 */
#define	IP_FF_SEND_ICMP		0x01	/* Send an ICMP error */
#define	IP_FF_HDR_COMPLETE	0x02	/* Call ip_hdr_complete if error */
#define	IP_FF_CKSUM		0x04	/* Recompute ipha_cksum if error */
#define	IP_FF_RAWIP		0x08	/* Use rawip mib variable */
#define	IP_FF_SRC_QUENCH	0x10	/* OK to send ICMP_SOURCE_QUENCH */
#define	IP_FF_SYN_ADDIRE	0x20	/* Add IRE if TCP syn packet */

static void	icmp_frag_needed(queue_t *, mblk_t *, int);
static void	icmp_inbound(queue_t *, mblk_t *, int, ipif_t *, int, uint32_t);
static void	icmp_inbound_error(queue_t *, mblk_t *);
static void	icmp_inbound_error_fanout(queue_t *, mblk_t *, icmph_t *,
		    ipha_t *, int, int);
static void	icmp_options_update(ipha_t *);
static void	icmp_param_problem(queue_t *, mblk_t *, uint8_t);
static void	icmp_pkt(queue_t *, mblk_t *, void *, size_t);
static mblk_t	*icmp_pkt_err_ok(mblk_t *);
static void	icmp_redirect(queue_t *, mblk_t *);
static void	icmp_send_redirect(queue_t *, mblk_t *, ipaddr_t);
static void	icmp_source_quench(queue_t *, mblk_t *);
void	icmp_time_exceeded(queue_t *, mblk_t *, int);
static void	icmp_unreachable(queue_t *, mblk_t *, int);

static void	igmp_timeout(void);

static void	ip_arp_news(queue_t *, mblk_t *);
static void	ip_bind(queue_t *, mblk_t *);
static mblk_t	*ip_carve_mp(mblk_t **, ssize_t);
extern	u_int	bcksum(u_char *, int, uint32_t);
static int	ip_close(queue_t *);
extern	u_int	ip_csum_hdr(ipha_t *);
mblk_t	*ip_dlpi_alloc(size_t, t_uscalar_t);
char	*ip_dot_addr(ipaddr_t, char *);
static char	*ip_dot_saddr(u_char *, char *);
void		ip_fanout_destroy(void);
void		ip_fanout_init(void);
static void	ip_fanout_proto(queue_t *, mblk_t *, ipha_t *, u_int);
static void	ip_fanout_tcp(queue_t *, mblk_t *, ipha_t *, uint32_t, u_int);
static void	ip_fanout_udp(queue_t *, mblk_t *, ipha_t *, uint32_t, u_short,
		    u_int);
static int	ip_hdr_complete(ipha_t *);
static void	ip_lrput(queue_t *, mblk_t *);
static void	ip_lwput(queue_t *, mblk_t *);
ipaddr_t	ip_massage_options(ipha_t *);
ipaddr_t	ip_net_mask(ipaddr_t);
static void	ip_newroute(queue_t *, mblk_t *, ipaddr_t);
static void	ip_newroute_ipif(queue_t *, mblk_t *, ipif_t *, ipaddr_t);

char	*ip_nv_lookup(nv_t *, int);
static int	ip_open(queue_t *, dev_t *, int, int, cred_t *);

static int	ip_optmgmt_writer(mblk_t *);

static void	ip_optcom_req(queue_t *, mblk_t *);
int	ip_opt_default(queue_t *, int, int, u_char *);
int	ip_opt_get(queue_t *, int, int, u_char *);
int	ip_opt_set(queue_t *, u_int, int, int, u_int, u_char *, u_int *,
	    u_char *);
static int	ip_param_get(queue_t *, mblk_t *, void *);
static boolean_t	ip_param_register(ipparam_t *, size_t);
static int	ip_param_set(queue_t *, mblk_t *, char *, void *);
static boolean_t	ip_reassemble(mblk_t *, ipf_t *, uint32_t, int);
void	ip_rput(queue_t *, mblk_t *);
static void	ip_rput_dlpi(queue_t *, mblk_t *);
static void	ip_rput_dlpi_writer(queue_t *, mblk_t *);
void	ip_rput_forward(ire_t *, ipha_t *, mblk_t *);
static int	ip_rput_forward_options(mblk_t *, ipha_t *, ire_t *);
void	ip_rput_local(queue_t *, mblk_t *, ipha_t *, ire_t *);
static int	ip_rput_local_options(queue_t *, mblk_t *, ipha_t *, ire_t *);
static ipaddr_t	ip_rput_options(queue_t *, mblk_t *, ipha_t *);
static void	ip_rput_other(queue_t *, mblk_t *);
static void	ip_rsrv(queue_t *);
static int	ip_snmp_get(queue_t *, mblk_t *);
static void	ip_snmp_get2(ire_t *, mblk_t **);
static int	ip_snmp_set(queue_t *, int, int, u_char *, int);
static boolean_t	ip_source_routed(ipha_t *);
static boolean_t	ip_source_route_included(ipha_t *);

static void	ip_trash(queue_t *, mblk_t *);
static void	ip_unbind(queue_t *, mblk_t *);
void	ip_wput(queue_t *, mblk_t *);
void	ip_wput_ire(queue_t *, mblk_t *, ire_t *);
static void	ip_wput_frag(ire_t *, mblk_t *, u_int *, uint32_t, uint32_t);
static mblk_t 	*ip_wput_frag_copyhdr(u_char *, ssize_t, int);
void	ip_wput_local(queue_t *, ipha_t *, mblk_t *, int, queue_t *);
static void	ip_wput_local_options(ipha_t *);
static void	ip_wput_nondata(queue_t *, mblk_t *);
static int	ip_wput_options(queue_t *, mblk_t *, ipha_t *);
static void	ip_wsrv(queue_t *);

void	ipc_hash_insert_connected(icf_t *, ipc_t *);
void	ipc_hash_insert_bound(icf_t *, ipc_t *);
void	ipc_hash_insert_wildcard(icf_t *, ipc_t *);
static void	ipc_hash_remove(ipc_t *);

static void	ipc_qenable(ipc_t *, void *);
void	ipc_walk(pfv_t, void *);
static void	ipc_walk_nontcp(pfv_t, void *);
static boolean_t	ipc_wantpacket(ipc_t *, ipaddr_t);

struct ill_s *ill_g_head;	/* ILL List Head */

ill_t	*ip_timer_ill;		/* ILL for IRE expiration timer. */
mblk_t	*ip_timer_mp;		/* IRE expiration timer. */
clock_t ip_ire_time_elapsed;	/* Time since IRE cache last flushed */
/* Time since redirect IREs last flushed */
static clock_t ip_ire_rd_time_elapsed;
static clock_t ip_ire_pmtu_time_elapsed; /* Time since path mtu increase */

ill_t	*igmp_timer_ill;	/* ILL for IGMP timer. */
mblk_t	*igmp_timer_mp;		/* IGMP timer */
int	igmp_timer_interval = IGMP_TIMEOUT_INTERVAL;

u_int	ip_def_gateway_count;	/* Number of IRE_DEFAULT entries. */
u_int	ip_def_gateway_index;	/* Walking index used to mod in */
ipaddr_t	ip_g_all_ones = IP_HOST_MASK;
clock_t icmp_pkt_err_last = 0;	/* Time since last icmp_pkt_err */

/* How long, in seconds, we allow frags to hang around. */
#define	IP_FRAG_TIMEOUT	60

static time_t	ip_g_frag_timeout = IP_FRAG_TIMEOUT;
static clock_t	ip_g_frag_timo_ms = IP_FRAG_TIMEOUT * 1000;

/* Protected by ip_mi_lock */
static void	*ip_g_head;		/* Instance Data List Head */
kmutex_t	ip_mi_lock;		/* Lock for list of instances */

/* Only modified during _init and _fini thus no locking is needed. */
caddr_t		ip_g_nd;		/* Named Dispatch List Head */


static long ip_rput_pullups;
int	ip_max_mtu;			/* Used by udp/icmp */
int	dohwcksum = 1;	/* use h/w cksum if supported by the hardware */

/*
 * MIB-2 stuff for SNMP (both IP and ICMP)
 */
mib2_ip_t	ip_mib;
static mib2_icmp_t	icmp_mib;

u_int loopback_packets = 0;	/* loopback interface statistics */

/*
 * XXX following really should only be in a header. Would need more
 * header and .c clean up first.
 */
extern optdb_obj_t	ip_opt_obj;

/*
 * Named Dispatch Parameter Table.
 * All of these are alterable, within the min/max values given, at run time.
 */
static ipparam_t	lcl_param_arr[] = {
	/* min	max	value	name */
	{  0,	1,	IP_FORWARD_DEFAULT,	"ip_forwarding"},
	{  0,	1,	0,	"ip_respond_to_address_mask_broadcast"},
	{  0,	1,	1,	"ip_respond_to_echo_broadcast"},
	{  0,	1,	1,	"ip_respond_to_timestamp"},
	{  0,	1,	1,	"ip_respond_to_timestamp_broadcast"},
	{  0,	1,	1,	"ip_send_redirects"},
	{  0,	1,	1,	"ip_forward_directed_broadcasts"},
	{  0,	10,	0,	"ip_debug"},
	{  0,	10,	0,	"ip_mrtdebug"},
	{  5000, 999999999,    30000, "ip_ire_cleanup_interval" },
	{  60000, 999999999, 1200000, "ip_ire_flush_interval" },
	{  60000, 999999999,   60000, "ip_ire_redirect_interval" },
	{  1,	255,	255,	"ip_def_ttl" },
	{  0,	1,	1,	"ip_forward_src_routed"},
	{  0,	256,	32,	"ip_wroff_extra" },
	{  5000, 999999999, 600000, "ip_ire_pathmtu_interval" },
	{  8,	65536,  64,	"ip_icmp_return_data_bytes" },
	{  0,	1,	0,	"ip_send_source_quench" },
	{  0,	1,	1,	"ip_path_mtu_discovery" },
	{  0,	240,	30,	"ip_ignore_delete_time" },
	{  0,	1,	0,	"ip_ignore_redirect" },
	{  0,	1,	1,	"ip_output_queue" },
	{  1,	254,	1,	"ip_broadcast_ttl" },
	{  0,	99999,	500,	"ip_icmp_err_interval" },
	{  0,	999999999,	1000000, "ip_reass_queue_bytes" },
	{  0,	1,	0,	"ip_strict_dst_multihoming" },
	{  1,	8192,	256,	"ip_addrs_per_if"},
};
	ipparam_t	*ip_param_arr = lcl_param_arr;

/*
 * ip_g_forward controls IP forwarding.  It takes two values:
 * 	0: IP_FORWARD_NEVER	Don't forward packets ever.
 *	1: IP_FORWARD_ALWAYS	Forward packets for elsewhere.
 *
 * RFC1122 says there must be a configuration switch to control forwarding,
 * but that the default MUST be to not forward packets ever.  Implicit
 * control based on configuration of multiple interfaces MUST NOT be
 * implemented (Section 3.1).  SunOS 4.1 did provide the "automatic" capability
 * and, in fact, it was the default.  That capability is now provided in the
 * /etc/rc2.d/S69inet script.
 */
#define	ip_g_forward			ip_param_arr[0].ip_param_value	/* */

/* Following line is external, and in ip.h.  Normally marked with * *. */
#define	ip_respond_to_address_mask_broadcast ip_param_arr[1].ip_param_value
#define	ip_g_resp_to_echo_bcast		ip_param_arr[2].ip_param_value
#define	ip_g_resp_to_timestamp		ip_param_arr[3].ip_param_value
#define	ip_g_resp_to_timestamp_bcast	ip_param_arr[4].ip_param_value
#define	ip_g_send_redirects		ip_param_arr[5].ip_param_value
#define	ip_g_forward_directed_bcast	ip_param_arr[6].ip_param_value
#define	ip_debug			ip_param_arr[7].ip_param_value	/* */
#define	ip_mrtdebug			ip_param_arr[8].ip_param_value	/* */
#define	ip_timer_interval		ip_param_arr[9].ip_param_value	/* */
#define	ip_ire_flush_interval		ip_param_arr[10].ip_param_value /* */
#define	ip_ire_redir_interval		ip_param_arr[11].ip_param_value
#define	ip_def_ttl			ip_param_arr[12].ip_param_value
#define	ip_forward_src_routed		ip_param_arr[13].ip_param_value
#define	ip_wroff_extra			ip_param_arr[14].ip_param_value
#define	ip_ire_pathmtu_interval		ip_param_arr[15].ip_param_value
#define	ip_icmp_return			ip_param_arr[16].ip_param_value
#define	ip_send_source_quench		ip_param_arr[17].ip_param_value
#define	ip_path_mtu_discovery		ip_param_arr[18].ip_param_value /* */
#define	ip_ignore_delete_time		ip_param_arr[19].ip_param_value /* */
#define	ip_ignore_redirect		ip_param_arr[20].ip_param_value
#define	ip_output_queue			ip_param_arr[21].ip_param_value
#define	ip_broadcast_ttl		ip_param_arr[22].ip_param_value
#define	ip_icmp_err_interval		ip_param_arr[23].ip_param_value
#define	ip_reass_queue_bytes		ip_param_arr[24].ip_param_value
#define	ip_strict_dst_multihoming	ip_param_arr[25].ip_param_value
#define	ip_addrs_per_if			ip_param_arr[26].ip_param_value

/*
 * ip_enable_group_ifs controls if logical interfaces should be
 * grouped together if they are on the same subnet.  By grouping
 * logical interfaces on the same subnet, the IP module can make an
 * attempt to balance traffic that leaves via a subnet across multiple
 * interfaces that are attached to it.  This allows a machine to
 * exploit switched Ethernet technology to allow 10n Mbit/sec, where n
 * is the number of physical taps on the Ethernet switch.
 *
 * If this variable is changed with NDD, the appropriate grouping/ungrouping
 * of interfaces will occur.
 */

int ip_enable_group_ifs = 1;

static icf_t ipc_tcp_conn_fanout[256];	/* TCP fanout hash list. */
static icf_t ipc_tcp_listen_fanout[256];
static icf_t ipc_udp_fanout[256];	/* UDP fanout hash list. */
static icf_t ipc_proto_fanout[256];	/* Misc. fanout hash list. */

	icf_t rts_clients;		/* Routing sockets */

u_int	ipif_g_count;			/* Count of IPIFs "up". */

static nv_t	ire_nv_arr[] = {
	{ IRE_BROADCAST, "BROADCAST" },
	{ IRE_LOCAL, "LOCAL" },
	{ IRE_LOOPBACK, "LOOPBACK" },
	{ IRE_CACHE, "CACHE" },
	{ IRE_DEFAULT, "DEFAULT" },
	{ IRE_PREFIX, "PREFIX" },
	{ IRE_IF_NORESOLVER, "IF_NORESOL" },
	{ IRE_IF_RESOLVER, "IF_RESOLV" },
	{ IRE_HOST, "HOST" },
	{ IRE_HOST_REDIRECT, "HOST_REDIRECT" },
	{ 0 }
};
	nv_t	*ire_nv_tbl = ire_nv_arr;

/* Simple ICMP IP Header Template */
static ipha_t icmp_ipha = {
	IP_SIMPLE_HDR_VERSION, 0, 0, 0, 0, 0, IPPROTO_ICMP
};

static struct module_info info = {
	0, "ip", 1, INFPSZ, 65536, 1024
};

static struct qinit rinit = {
	(pfi_t)ip_rput, (pfi_t)ip_rsrv, ip_open, ip_close, nil(pfi_t), &info
};

static struct qinit winit = {
	(pfi_t)ip_wput, (pfi_t)ip_wsrv, ip_open, ip_close, nil(pfi_t), &info
};

static struct qinit lrinit = {
	(pfi_t)ip_lrput, nil(pfi_t), ip_open, ip_close, nil(pfi_t), &info
};

static struct qinit lwinit = {
	(pfi_t)ip_lwput, nil(pfi_t), ip_open, ip_close, nil(pfi_t), &info
};

/* (Please don't reformat this.  gh likes it this way!) */
	struct streamtab ipinfo = {
		&rinit, &winit, &lrinit, &lwinit
};

	int	ipdevflag = 0;

/* Generate an ICMP fragmentation needed message. */
static void
icmp_frag_needed(queue_t *q, mblk_t *mp, int mtu)
{
	icmph_t	icmph;

	if (!(mp = icmp_pkt_err_ok(mp)))
		return;
	bzero(&icmph, sizeof (icmph_t));
	icmph.icmph_type = ICMP_DEST_UNREACHABLE;
	icmph.icmph_code = ICMP_FRAGMENTATION_NEEDED;
	icmph.icmph_du_mtu = htons((uint16_t)mtu);
	BUMP_MIB(icmp_mib.icmpOutFragNeeded);
	BUMP_MIB(icmp_mib.icmpOutDestUnreachs);
	icmp_pkt(q, mp, &icmph, sizeof (icmph_t));
}

/*
 * All arriving ICMP messages are sent here from ip_rput.
 * When ipif is NULL the queue has to be an "ill queue".
 */
static void
icmp_inbound(queue_t *q, mblk_t *mp, int ire_type, ipif_t *ipif,
    int sum_valid, uint32_t sum)
{
	icmph_t	*icmph;
	ipha_t	*ipha = (ipha_t *)mp->b_rptr;
	int	iph_hdr_length;
	boolean_t	interested;
	uint32_t	u1;
	u_char	*wptr;

	BUMP_MIB(icmp_mib.icmpInMsgs);
	iph_hdr_length = IPH_HDR_LENGTH(ipha);
	if ((mp->b_wptr - mp->b_rptr) < (iph_hdr_length + ICMPH_SIZE)) {
		/* Last chance to get real. */
		if (!pullupmsg(mp, iph_hdr_length + ICMPH_SIZE)) {
			BUMP_MIB(icmp_mib.icmpInErrors);
			freemsg(mp);
			return;
		}
		/* Refresh iph following the pullup. */
		ipha = (ipha_t *)mp->b_rptr;
	}
	/* ICMP header checksum, including checksum field, should be zero. */
	if (sum_valid ? (sum != 0 && sum != 0xFFFF) :
	    IP_CSUM(mp, iph_hdr_length, 0)) {
		BUMP_MIB(icmp_mib.icmpInCksumErrs);
		freemsg(mp);
		return;
	}
	/* The IP header will always be a multiple of four bytes */
	icmph = (icmph_t *)&mp->b_rptr[iph_hdr_length];
	wptr = (u_char *)icmph + ICMPH_SIZE;
	/* We will set "interested" to "true" if we want a copy */
	interested = false;
	switch (icmph->icmph_type) {
	case ICMP_ECHO_REPLY:
		BUMP_MIB(icmp_mib.icmpInEchoReps);
		break;
	case ICMP_DEST_UNREACHABLE:
		if (icmph->icmph_code == ICMP_FRAGMENTATION_NEEDED)
			BUMP_MIB(icmp_mib.icmpInFragNeeded);
		interested = true;	/* Pass up to transport */
		BUMP_MIB(icmp_mib.icmpInDestUnreachs);
		break;
	case ICMP_SOURCE_QUENCH:
		interested = true;	/* Pass up to transport */
		BUMP_MIB(icmp_mib.icmpInSrcQuenchs);
		break;
	case ICMP_REDIRECT:
		if (!ip_ignore_redirect)
			interested = true;
		BUMP_MIB(icmp_mib.icmpInRedirects);
		break;
	case ICMP_ECHO_REQUEST:
		/*
		 * Whether to respond to echo requests that come in as IP
		 * broadcasts is subject to debate (what isn't?).  We aim
		 * to please, you pick it.  Default is do it.
		 */
		if (ip_g_resp_to_echo_bcast || ire_type != IRE_BROADCAST)
			interested = true;
		BUMP_MIB(icmp_mib.icmpInEchos);
		break;
	case ICMP_ROUTER_ADVERTISEMENT:
	case ICMP_ROUTER_SOLICITATION:
		break;
	case ICMP_TIME_EXCEEDED:
		interested = true;	/* Pass up to transport */
		BUMP_MIB(icmp_mib.icmpInTimeExcds);
		break;
	case ICMP_PARAM_PROBLEM:
		interested = true;	/* Pass up to transport */
		BUMP_MIB(icmp_mib.icmpInParmProbs);
		break;
	case ICMP_TIME_STAMP_REQUEST:
		/* Response to Time Stamp Requests is local policy. */
		if (ip_g_resp_to_timestamp &&
		    /* So is whether to respond if it was an IP broadcast. */
		    (ire_type != IRE_BROADCAST ||
			ip_g_resp_to_timestamp_bcast) &&
		    /* Why do we think we can count on this ??? */
		    /* TODO m_pullup of complete header? */
		    (mp->b_datap->db_lim - wptr) >= (3 * sizeof (uint32_t)))
			interested = true;
		BUMP_MIB(icmp_mib.icmpInTimestamps);
		break;
	case ICMP_TIME_STAMP_REPLY:
		BUMP_MIB(icmp_mib.icmpInTimestampReps);
		break;
	case ICMP_INFO_REQUEST:
		/* Per RFC 1122 3.2.2.7, ignore this. */
	case ICMP_INFO_REPLY:
		break;
	case ICMP_ADDRESS_MASK_REQUEST:
		if ((ip_respond_to_address_mask_broadcast ||
		    ire_type != IRE_BROADCAST) &&
		    /* TODO m_pullup of complete header? */
		    (mp->b_datap->db_lim - wptr) >= IP_ADDR_LEN)
			interested = true;
		BUMP_MIB(icmp_mib.icmpInAddrMasks);
		break;
	case ICMP_ADDRESS_MASK_REPLY:
		BUMP_MIB(icmp_mib.icmpInAddrMaskReps);
		break;
	default:
		interested = true;	/* Pass up to transport */
		BUMP_MIB(icmp_mib.icmpInUnknowns);
		break;
	}
	/* See if there is an ICMP client. */
	if (ipc_proto_fanout[IPPROTO_ICMP].icf_ipc != NULL) {
		/* If there is an ICMP client and we want one too, copy it. */
		mblk_t *mp1;

		if (!interested) {
			ip_fanout_proto(q, mp, ipha, IP_FF_SEND_ICMP);
			return;
		}
		mp1 = copymsg(mp);
		if (mp1 != NULL) {
			ip_fanout_proto(q, mp1, ipha, IP_FF_SEND_ICMP);
		}
	} else if (!interested) {
		freemsg(mp);
		return;
	}
	/* We want to do something with it. */
	/* Check db_ref to make sure we can modify the packet. */
	if (mp->b_datap->db_ref > 1) {
		mblk_t	*mp1;

		mp1 = copymsg(mp);
		freemsg(mp);
		if (!mp1) {
			BUMP_MIB(icmp_mib.icmpOutDrops);
			return;
		}
		mp = mp1;
		ipha = (ipha_t *)mp->b_rptr;
		icmph = (icmph_t *)&mp->b_rptr[iph_hdr_length];
		wptr = (u_char *)icmph + ICMPH_SIZE;
	}
	switch (icmph->icmph_type) {
	case ICMP_ADDRESS_MASK_REQUEST:
		/* Set ipif for the non-broadcast case. */
		if (!ipif) {
			ill_t *ill = (ill_t *)q->q_ptr;

			if (ill->ill_ipif_up_count == 1)
				ipif = ill->ill_ipif;
			else {
				ipaddr_t	src;
				src = ipha->ipha_src;
				ipif = ipif_lookup_remote(ill, src);
				if (!ipif) {
					freemsg(mp);
					return;
				}
			}
		}
		icmph->icmph_type = ICMP_ADDRESS_MASK_REPLY;
		bcopy(&ipif->ipif_net_mask, wptr, IP_ADDR_LEN);
		BUMP_MIB(icmp_mib.icmpOutAddrMaskReps);
		break;
	case ICMP_ECHO_REQUEST:
		icmph->icmph_type = ICMP_ECHO_REPLY;
		BUMP_MIB(icmp_mib.icmpOutEchoReps);
		break;
	case ICMP_TIME_STAMP_REQUEST: {
		uint32_t *tsp;

		icmph->icmph_type = ICMP_TIME_STAMP_REPLY;
		tsp = (uint32_t *)wptr;
		tsp++;		/* Skip past 'originate time' */
		/* Compute # of milliseconds since midnight */
		u1 = (hrestime.tv_sec % (24 * 60 * 60)) * 1000 +
			hrestime.tv_nsec / (NANOSEC / MILLISEC);
		*tsp++ = htonl(u1);	/* Lay in 'receive time' */
		*tsp++ = htonl(u1);	/* Lay in 'send time' */
		BUMP_MIB(icmp_mib.icmpOutTimestampReps);
		break;
	}
	case ICMP_REDIRECT:
		become_exclusive(q, mp, (pfi_t)icmp_redirect);
		return;
	case ICMP_DEST_UNREACHABLE:
		if (icmph->icmph_code == ICMP_FRAGMENTATION_NEEDED)
			become_exclusive(q, mp, (pfi_t)icmp_inbound_error);
		else
			icmp_inbound_error(q, mp);
		return;
	default:
		icmp_inbound_error(q, mp);
		return;
	}
	icmph->icmph_checksum = 0;
	icmph->icmph_checksum = IP_CSUM(mp, iph_hdr_length, 0);
	if (ire_type == IRE_BROADCAST) {
		/*
		 * Make it look like it was directed to us, so we don't look
		 * like a fool with a broadcast source address.
		 * Note: ip_?put_local guarantees that ipif is set here.
		 */
		ipha->ipha_dst = ipif->ipif_local_addr;
	}
	/* Reset time to live. */
	ipha->ipha_ttl = (uint8_t)ip_def_ttl;

	{
		/* Swap source and destination addresses */
		ipaddr_t tmp;

		tmp = ipha->ipha_src;
		ipha->ipha_src = ipha->ipha_dst;
		ipha->ipha_dst = tmp;
	}
	ipha->ipha_ident = 0;
	if (!IS_SIMPLE_IPH(ipha))
		icmp_options_update(ipha);

	BUMP_MIB(icmp_mib.icmpOutMsgs);
	put(WR(q), mp);
}

/* Table from RFC 1191 */
static int icmp_frag_size_table[] =
{ 32000, 17914, 8166, 4352, 2002, 1496, 1006, 508, 296, 68 };

/*
 * Process received ICMP error packets including Dest Unreachables.
 * Must be called as a writer for fragmentation needed unreachables!
 */
static void
icmp_inbound_error(queue_t *q, mblk_t *mp)
{
	icmph_t *icmph;
	ipha_t	*ipha;
	int	iph_hdr_length;
	int	hdr_length;
	ire_t	*ire;
	int	mtu;

	if (!OK_32PTR(mp->b_rptr)) {
		if (!pullupmsg(mp, -1)) {
			BUMP_MIB(ip_mib.ipInDiscards);
			goto drop_pkt;
		}
	}
	ipha = (ipha_t *)mp->b_rptr;
	iph_hdr_length = IPH_HDR_LENGTH(ipha);
	if (((mp->b_wptr - mp->b_rptr) - iph_hdr_length) < sizeof (icmph_t)) {
		BUMP_MIB(icmp_mib.icmpInErrors);
		goto drop_pkt;
	}
	icmph = (icmph_t *)&mp->b_rptr[iph_hdr_length];
	ipha = (ipha_t *)&icmph[1];
	if ((u_char *)&ipha[1] > mp->b_wptr) {
		if (!pullupmsg(mp, (u_char *)&ipha[1] - mp->b_rptr)) {
			BUMP_MIB(ip_mib.ipInDiscards);
			goto drop_pkt;
		}
		icmph = (icmph_t *)&mp->b_rptr[iph_hdr_length];
		ipha = (ipha_t *)&icmph[1];
	}
	hdr_length = IPH_HDR_LENGTH(ipha);
	if ((u_char *)ipha + hdr_length > mp->b_wptr) {
		if (!pullupmsg(mp, (u_char *)ipha + hdr_length - mp->b_rptr)) {
			BUMP_MIB(ip_mib.ipInDiscards);
			goto drop_pkt;
		}
		icmph = (icmph_t *)&mp->b_rptr[iph_hdr_length];
		ipha = (ipha_t *)&icmph[1];
	}

	if (icmph->icmph_type == ICMP_DEST_UNREACHABLE &&
	    icmph->icmph_code == ICMP_FRAGMENTATION_NEEDED) {
		ire = ire_ctable_lookup(ipha->ipha_dst, 0, IRE_CACHE, NULL,
		    NULL, MATCH_IRE_TYPE);
		if (!ire) {
			ip1dbg(("icmp_unreach: no route for 0x%x\n",
			    ntohl(ipha->ipha_dst)));
			freemsg(mp);
			return;
		}
		/* Drop if the original packet contained a source route */
		if (ip_source_route_included(ipha)) {
			freemsg(mp);
			return;
		}
		/* Check for MTU discovery advice as described in RFC 1191 */
		mtu = ntohs(icmph->icmph_du_mtu);
		if (icmph->icmph_du_zero == 0 && mtu > 68) {
			/* Reduce the IRE max frag value as advised. */
			ire->ire_max_frag = MIN(ire->ire_max_frag, mtu);
			ip1dbg(("Received mtu from router: %d\n", mtu));
		} else {
			uint32_t length;
			int	i;

			/*
			 * Use the table from RFC 1191 to figure out
			 * the next "plateau" based on the length in
			 * the original IP packet.
			 */
			length = ntohs(ipha->ipha_length);
			if (ire->ire_max_frag <= length &&
			    ire->ire_max_frag >= length - hdr_length) {
				/*
				 * Handle broken BSD 4.2 systems that
				 * return the wrong iph_length in ICMP
				 * errors.
				 */
				ip1dbg(("Wrong mtu: sent %d, ire %d\n",
				    length, ire->ire_max_frag));
				length -= hdr_length;
			}
			for (i = 0; i < A_CNT(icmp_frag_size_table); i++) {
				if (length > icmp_frag_size_table[i])
					break;
			}
			if (i == A_CNT(icmp_frag_size_table)) {
				/* Smaller than 68! */
				ip1dbg(("Too big for packet size %d\n",
				    length));
				ire->ire_max_frag = MIN(ire->ire_max_frag,
				    576);
				ire->ire_frag_flag = 0;
			} else {
				mtu = icmp_frag_size_table[i];
				ip1dbg(("Calculated mtu %d, packet"
				    " size %d, before %d",
				    mtu, length, ire->ire_max_frag));
				ire->ire_max_frag = MIN(ire->ire_max_frag,
				    mtu);
				ip1dbg((", after %d\n", ire->ire_max_frag));
			}
		}
		/* Record the new max frag size for the ULP. */
		icmph->icmph_du_zero = 0;
		icmph->icmph_du_mtu = htons((uint16_t)ire->ire_max_frag);
	}
	icmp_inbound_error_fanout(q, mp, icmph, ipha,
	    iph_hdr_length, hdr_length);
	return;

drop_pkt:;
	ip1dbg(("icmp_inbound_error: drop pkt\n"));
	freemsg(mp);
}

/* Try to pass the ICMP message upstream in case the ULP cares. */
static void
icmp_inbound_error_fanout(queue_t *q, mblk_t *mp, icmph_t *icmph, ipha_t *ipha,
    int iph_hdr_length, int hdr_length)
{
	uint16_t *up;	/* Pointer to ports in ULP header */
	uint32_t ports;	/* reversed ports for fanout */
	ipha_t ripha;	/* With reversed addresses */

	switch (ipha->ipha_protocol) {
	case IPPROTO_UDP:
		/* Verify that we have at least 8 bytes of the UDP header */
		if ((u_char *)ipha + hdr_length + 8 > mp->b_wptr) {
			if (!pullupmsg(mp, (u_char *)ipha + hdr_length + 8 -
			    mp->b_rptr)) {
				BUMP_MIB(ip_mib.ipInDiscards);
				goto drop_pkt;
			}
			icmph = (icmph_t *)&mp->b_rptr[iph_hdr_length];
			ipha = (ipha_t *)&icmph[1];
		}
		up = (uint16_t *)((u_char *)ipha + hdr_length);

		/*
		 * Attempt to find a client stream based on port.
		 * Note that we do a reverse lookup since the header is
		 * in the form we sent it out.
		 * The ripha header is only used for the IP_UDP_MATCH and we
		 * only set the src and dst addresses and protocol.
		 */
		ripha.ipha_src = ipha->ipha_dst;
		ripha.ipha_dst = ipha->ipha_src;
		ripha.ipha_protocol = ipha->ipha_protocol;
		((uint16_t *)&ports)[0] = up[1];
		((uint16_t *)&ports)[1] = up[0];

		/* Have to change db_type after any pullupmsg */
		mp->b_datap->db_type = M_CTL;

		ip_fanout_udp(q, mp, &ripha, ports, IRE_LOCAL, 0);
		return;

	case IPPROTO_TCP:
		/* Verify that we have at least 8 bytes of the TCP header */
		if ((u_char *)ipha + hdr_length + 8 > mp->b_wptr) {
			if (!pullupmsg(mp, (u_char *)ipha + hdr_length + 8 -
			    mp->b_rptr)) {
				BUMP_MIB(ip_mib.ipInDiscards);
				goto drop_pkt;
			}
			icmph = (icmph_t *)&mp->b_rptr[iph_hdr_length];
			ipha = (ipha_t *)&icmph[1];
		}
		up = (uint16_t *)((u_char *)ipha + hdr_length);
		/*
		 * Find a TCP client stream for this packet.
		 * Note that we do a reverse lookup since the header is
		 * in the form we sent it out.
		 * The ripha header is only used for the IP_TCP_*_MATCH and we
		 * only set the src and dst addresses and protocol.
		 */
		ripha.ipha_src = ipha->ipha_dst;
		ripha.ipha_dst = ipha->ipha_src;
		ripha.ipha_protocol = ipha->ipha_protocol;
		((uint16_t *)&ports)[0] = up[1];
		((uint16_t *)&ports)[1] = up[0];

		/* Have to change db_type after any pullupmsg */
		mp->b_datap->db_type = M_CTL;

		ip_fanout_tcp(q, mp, &ripha, ports, 0);
		return;

	default:
		/*
		 * Handle protocols with which IP is less intimate.  There
		 * can be more than one stream bound to a particular
		 * protocol.  When this is the case, each one gets a copy
		 * of any incoming packets.
		 *
		 * The ripha header is only used for the lookup and we
		 * only set the src and dst addresses and protocol.
		 */
		ripha.ipha_src = ipha->ipha_dst;
		ripha.ipha_dst = ipha->ipha_src;
		ripha.ipha_protocol = ipha->ipha_protocol;

		/* Have to change db_type after any pullupmsg */
		mp->b_datap->db_type = M_CTL;

		ip_fanout_proto(q, mp, &ripha, 0);
		return;
	}
	/* NOTREACHED */
drop_pkt:;
	ip1dbg(("icmp_inbound_error: drop pkt\n"));
	freemsg(mp);
}

/*
 * Update any record route or timestamp options to include this host.
 * Reverse any source route option.
 */
static void
icmp_options_update(ipha_t *ipha)
{
	uint32_t	totallen;
	u_char		*opt;
	uint32_t	optval;
	uint32_t	optlen;
	ipaddr_t src;		/* Our local address */
	ipaddr_t dst;

	ip2dbg(("icmp_options_update\n"));
	src = ipha->ipha_src;
	dst = ipha->ipha_dst;
	totallen = ipha->ipha_version_and_hdr_length -
		(uint8_t)((IP_VERSION << 4) + IP_SIMPLE_HDR_LENGTH_IN_WORDS);
	opt = (u_char *)&ipha[1];
	totallen <<= 2;
	while (totallen != 0) {
		switch (optval = opt[IPOPT_POS_VAL]) {
		case IPOPT_EOL:
			return;
		case IPOPT_NOP:
			optlen = 1;
			break;
		default:
			optlen = opt[IPOPT_POS_LEN];
		}
		ip2dbg(("icmp_options_update: opt %d, len %d\n",
		    optval, optlen));

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
			 * address).
			 * The last entry should be the final destination.
			 */
			off1 = IPOPT_MINOFF_SR - 1;
			off2 = opt[IPOPT_POS_OFF] - IP_ADDR_LEN - 1;
			if (off2 < 0) {
				/* No entries in source route */
				ip1dbg((
				    "icmp_options_update: bad src route\n"));
				break;
			}
			bcopy((char *)opt + off2, &dst, IP_ADDR_LEN);
			bcopy(&ipha->ipha_dst, (char *)opt + off2, IP_ADDR_LEN);
			bcopy(&dst, &ipha->ipha_dst, IP_ADDR_LEN);
			off2 -= IP_ADDR_LEN;

			while (off1 < off2) {
				bcopy((char *)opt + off1, &src, IP_ADDR_LEN);
				bcopy((char *)opt + off2, (char *)opt + off1,
				    IP_ADDR_LEN);
				bcopy(&src, (char *)opt + off2, IP_ADDR_LEN);
				off1 += IP_ADDR_LEN;
				off2 -= IP_ADDR_LEN;
			}
			opt[IPOPT_POS_OFF] = IPOPT_MINOFF_SR;
			break;
		}
		totallen -= optlen;
		opt += optlen;
	}
}

/*
 * Process received ICMP Redirect messages.
 * Must be called as a writer!
 */
/* ARGSUSED */
static void
icmp_redirect(queue_t *q, mblk_t *mp)
{
	ipha_t	*ipha;
	int	iph_hdr_length;
	icmph_t	*icmph;
	ipha_t	*ipha_err;
	ire_t	*ire;
	ire_t	*prev_ire;
	ipaddr_t  src, dst, gateway;
	clock_t	prev_rtt;
	clock_t	prev_rtt_sd;

	if (!OK_32PTR(mp->b_rptr)) {
		if (!pullupmsg(mp, -1)) {
			BUMP_MIB(ip_mib.ipInDiscards);
			freemsg(mp);
			return;
		}
	}
	ipha = (ipha_t *)mp->b_rptr;
	iph_hdr_length = IPH_HDR_LENGTH(ipha);
	if (((mp->b_wptr - mp->b_rptr) - iph_hdr_length) <
	    sizeof (icmph_t) + IP_SIMPLE_HDR_LENGTH) {
		BUMP_MIB(icmp_mib.icmpInErrors);
		freemsg(mp);
		return;
	}
	icmph = (icmph_t *)&mp->b_rptr[iph_hdr_length];
	ipha_err = (ipha_t *)&icmph[1];
	src = ipha->ipha_src;
	dst = ipha_err->ipha_dst;
	gateway = icmph->icmph_rd_gateway;
	/* Make sure the new gateway is reachable somehow. */
	ire = ire_route_lookup(gateway, 0, 0, IRE_INTERFACE, NULL, NULL, NULL,
	    MATCH_IRE_TYPE);
	/*
	 * Make sure we had a route for the dest in question and that
	 * that route was pointing to the old gateway (the source of the
	 * redirect packet.)
	 */
	prev_ire = ire_route_lookup(dst, 0, src, 0, NULL, NULL, NULL,
	    MATCH_IRE_GW);
	/*
	 * Check that
	 *	the redirect was not from ourselves
	 *	the new gateway and the old gateway are directly reachable
	 */
	if (!prev_ire ||
	    !ire ||
	    ire->ire_type == IRE_LOCAL) {
		BUMP_MIB(icmp_mib.icmpInBadRedirects);
		freemsg(mp);
		return;
	}
	prev_rtt = prev_ire->ire_rtt;
	prev_rtt_sd = prev_ire->ire_rtt_sd;
	if (prev_ire->ire_type == IRE_CACHE)
		ire_delete(prev_ire);
	/*
	 * TODO: more precise handling for cases 0, 2, 3, the latter two
	 * require TOS routing
	 */
	switch (icmph->icmph_code) {
	case 0:
	case 1:
		/* TODO: TOS specificity for cases 2 and 3 */
	case 2:
	case 3:
		break;
	default:
		freemsg(mp);
		BUMP_MIB(icmp_mib.icmpInBadRedirects);
		return;
	}
	/*
	 * Create a Route Association.  This will allow us to remember that
	 * someone we believe told us to use the particular gateway.
	 */
	ire = ire_create(
		(u_char *)&dst,				/* dest addr */
		(u_char *)&ip_g_all_ones,		/* mask */
		(u_char *)&ire->ire_src_addr,		/* source addr */
		(u_char *)&gateway,    			/* gateway addr */
		ire->ire_max_frag,			/* max frag */
		nilp(mblk_t),				/* xmit header */
		nilp(queue_t),				/* no rfq */
		nilp(queue_t),				/* no stq */
		IRE_HOST_REDIRECT,
		prev_rtt,
		prev_rtt_sd,
		0,
		NULL,
		NULL,
		(RTF_DYNAMIC | RTF_GATEWAY | RTF_HOST));
	if (ire == NULL) {
		freemsg(mp);
		return;
	}
	(void) ire_add(ire);
	/* tell routing sockets that we received a redirect */
	ip_rts_change(RTM_REDIRECT, dst, gateway, IP_HOST_MASK, src,
	    (RTF_DYNAMIC | RTF_GATEWAY | RTF_HOST), 0,
	    (RTA_DST | RTA_GATEWAY | RTA_NETMASK));
	/*
	 * Delete any existing IRE_HOST_REDIRECT for this destination.
	 * This together with the added IRE has the effect of
	 * modifying an existing redirect.
	 */
	prev_ire = ire_ftable_lookup(dst, 0, src, IRE_HOST_REDIRECT, NULL, NULL,
	    NULL, (MATCH_IRE_GW | MATCH_IRE_TYPE));
	if (prev_ire)
		ire_delete(prev_ire);

	freemsg(mp);
}

/*
 * Generate an ICMP parameter problem message.
 * (May be called as writer.)
 */
static void
icmp_param_problem(queue_t *q, mblk_t *mp, uint8_t ptr)
{
	icmph_t	icmph;

	if (!(mp = icmp_pkt_err_ok(mp)))
		return;
	bzero(&icmph, sizeof (icmph_t));
	icmph.icmph_type = ICMP_PARAM_PROBLEM;
	icmph.icmph_pp_ptr = ptr;
	BUMP_MIB(icmp_mib.icmpOutParmProbs);
	icmp_pkt(q, mp, &icmph, sizeof (icmph_t));
}

/*
 * Build and ship an ICMP message using the packet data in mp, and the ICMP
 * header pointed to by "stuff".  (May be called as writer.)
 * Note: assumes that icmp_pkt_err_ok has been called to verify that
 * and icmp error packet can be sent.
 */
static void
icmp_pkt(queue_t *q, mblk_t *mp, void *stuff, size_t len)
{
	ipaddr_t dst;
	icmph_t	*icmph;
	ipha_t	*ipha;
	u_int	len_needed;
	size_t	msg_len;
	mblk_t	*mp1;

	ipha = (ipha_t *)mp->b_rptr;
	/* Remember our eventual destination */
	dst = ipha->ipha_src;

	/*
	 * Check if we can send back more then 8 bytes in addition
	 * to the IP header. We will include as much as 64 bytes.
	 */
	len_needed = IPH_HDR_LENGTH(ipha) + ip_icmp_return;
	msg_len = msgdsize(mp);
	if (msg_len > len_needed) {
		(void) adjmsg(mp, len_needed - msg_len);
		msg_len = len_needed;
	}
	mp1 = allocb(sizeof (icmp_ipha) + len, BPRI_HI);
	if (!mp1) {
		BUMP_MIB(icmp_mib.icmpOutErrors);
		freemsg(mp);
		return;
	}
	mp1->b_cont = mp;
	mp = mp1;
	ipha = (ipha_t *)mp->b_rptr;
	mp1->b_wptr = (u_char *)ipha + (sizeof (icmp_ipha) + len);
	*ipha = icmp_ipha;
	ipha->ipha_dst = dst;
	ipha->ipha_ttl = (uint8_t)ip_def_ttl;
	msg_len += sizeof (icmp_ipha) + len;
	if (msg_len > IP_MAXPACKET) {
		(void) adjmsg(mp, IP_MAXPACKET - msg_len);
		msg_len = IP_MAXPACKET;
	}
	ipha->ipha_length = htons((uint16_t)msg_len);
	icmph = (icmph_t *)&ipha[1];
	bcopy(stuff, icmph, len);
	icmph->icmph_checksum = 0;
	icmph->icmph_checksum = IP_CSUM(mp, (int32_t)sizeof (ipha_t), 0);
	BUMP_MIB(icmp_mib.icmpOutMsgs);
	put(q, mp);
}

/*
 * Check if it is ok to send an ICMP error packet in
 * response to the IP packet in mp.
 * Free the message and return null if no
 * ICMP error packet should be sent.
 */
static mblk_t *
icmp_pkt_err_ok(mblk_t *mp)
{
	icmph_t	*icmph;
	ipha_t	*ipha;
	u_int	len_needed;

	if (!mp)
		return (nilp(mblk_t));
	if (icmp_pkt_err_last > TICK_TO_MSEC(lbolt))
		/* 100HZ lbolt in ms for 32bit arch wraps every 49.7 days */
		icmp_pkt_err_last = 0;
	if (icmp_pkt_err_last + ip_icmp_err_interval > TICK_TO_MSEC(lbolt)) {
		/*
		 * Only send ICMP error packets every so often.
		 * This should be done on a per port/source basis,
		 * but for now this will due.
		 */
		freemsg(mp);
		return (nilp(mblk_t));
	}
	ipha = (ipha_t *)mp->b_rptr;
	if (ip_csum_hdr(ipha)) {
		BUMP_MIB(ip_mib.ipInCksumErrs);
		freemsg(mp);
		return (nilp(mblk_t));
	}
	if ((ire_ctable_lookup(ipha->ipha_dst, 0, IRE_BROADCAST, NULL, NULL,
	    MATCH_IRE_TYPE) != NULL) ||
	    (ire_ctable_lookup(ipha->ipha_src, 0, IRE_BROADCAST, NULL, NULL,
		MATCH_IRE_TYPE) != NULL) ||
	    CLASSD(ipha->ipha_dst) ||
	    CLASSD(ipha->ipha_src) ||
	    (ntohs(ipha->ipha_fragment_offset_and_flags) & IPH_OFFSET)) {
		/* Note: only errors to the fragment with offset 0 */
		BUMP_MIB(icmp_mib.icmpOutDrops);
		freemsg(mp);
		return (nilp(mblk_t));
	}
	if (ipha->ipha_protocol == IPPROTO_ICMP) {
		/*
		 * Check the ICMP type.  RFC 1122 sez:  don't send ICMP
		 * errors in response to any ICMP errors.
		 */
		len_needed = IPH_HDR_LENGTH(ipha) + ICMPH_SIZE;
		if (mp->b_wptr - mp->b_rptr < len_needed) {
			if (!pullupmsg(mp, len_needed)) {
				BUMP_MIB(icmp_mib.icmpInErrors);
				freemsg(mp);
				return (nilp(mblk_t));
			}
			ipha = (ipha_t *)mp->b_rptr;
		}
		icmph = (icmph_t *)
		    (&((char *)ipha)[IPH_HDR_LENGTH(ipha)]);
		switch (icmph->icmph_type) {
		case ICMP_DEST_UNREACHABLE:
		case ICMP_SOURCE_QUENCH:
		case ICMP_TIME_EXCEEDED:
		case ICMP_PARAM_PROBLEM:
		case ICMP_REDIRECT:
			BUMP_MIB(icmp_mib.icmpOutDrops);
			freemsg(mp);
			return (nilp(mblk_t));
		default:
			break;
		}
	}
	icmp_pkt_err_last = TICK_TO_MSEC(lbolt);
	return (mp);
}

/*
 * Generate an ICMP redirect message.
 */
static void
icmp_send_redirect(queue_t *q, mblk_t *mp, ipaddr_t gateway)
{
	icmph_t	icmph;

	if (!(mp = icmp_pkt_err_ok(mp)))
		return;
	bzero(&icmph, sizeof (icmph_t));
	icmph.icmph_type = ICMP_REDIRECT;
	icmph.icmph_code = 1;
	icmph.icmph_rd_gateway = gateway;
	BUMP_MIB(icmp_mib.icmpOutRedirects);
	icmp_pkt(q, mp, &icmph, sizeof (icmph_t));
}

/*
 * Generate an ICMP source quench message.
 * (May be called as writer.)
 */
static void
icmp_source_quench(queue_t *q, mblk_t *mp)
{
	icmph_t	icmph;

	if (!ip_send_source_quench) {
		freemsg(mp);
		return;
	}
	if (!(mp = icmp_pkt_err_ok(mp)))
		return;
	bzero(&icmph, sizeof (icmph_t));
	icmph.icmph_type = ICMP_SOURCE_QUENCH;
	BUMP_MIB(icmp_mib.icmpOutSrcQuenchs);
	icmp_pkt(q, mp, &icmph, sizeof (icmph_t));
}

/*
 * Generate an ICMP time exceeded message.
 * (May be called as writer.)
 */
void
icmp_time_exceeded(queue_t *q, mblk_t *mp, int code)
{
	icmph_t	icmph;

	if (!(mp = icmp_pkt_err_ok(mp)))
		return;
	bzero(&icmph, sizeof (icmph_t));
	icmph.icmph_type = ICMP_TIME_EXCEEDED;
	icmph.icmph_code = (uint8_t)code;
	BUMP_MIB(icmp_mib.icmpOutTimeExcds);
	icmp_pkt(q, mp, &icmph, sizeof (icmph_t));
}

/*
 * Generate an ICMP unreachable message.
 * (May be called as writer.)
 */
static void
icmp_unreachable(queue_t *q, mblk_t *mp, int code)
{
	icmph_t	icmph;

	if (!(mp = icmp_pkt_err_ok(mp)))
		return;
	bzero(&icmph, sizeof (icmph_t));
	icmph.icmph_type = ICMP_DEST_UNREACHABLE;
	icmph.icmph_code = (uint8_t)code;
	BUMP_MIB(icmp_mib.icmpOutDestUnreachs);
	icmp_pkt(q, mp, (char *)&icmph, sizeof (icmph_t));
}

/*
 * igmp_timeout
 */
static void
igmp_timeout(void)
{
	int next;

	next = igmp_timeout_handler();
	if (next != 0 && igmp_timer_ill)
		mi_timer(igmp_timer_ill->ill_rq, igmp_timer_mp,
		    next * igmp_timer_interval);
}

/*
 * igmp_timeout_start
 */
void
igmp_timeout_start(int next)
{
	if (next != 0 && igmp_timer_ill)
		mi_timer(igmp_timer_ill->ill_rq, igmp_timer_mp,
		    next * igmp_timer_interval);
}

/*
 * News from ARP.  ARP sends notification of interesting events down
 * to its clients using M_CTL messages with the interesting ARP packet
 * attached via b_cont.  We are called as writer in case we need to
 * blow away any IREs.
 */
static void
ip_arp_news(queue_t *q, mblk_t *mp)
{
	arcn_t	*arcn;
	arh_t	*arh;
	char	*cp1;
	u_char	*cp2;
	ire_t	*ire;
	int	i1;
	char	hbuf[128];
	char	sbuf[16];
	ipaddr_t src;

	if ((mp->b_wptr - mp->b_rptr) < sizeof (arcn_t)	|| !mp->b_cont) {
		if (q->q_next) {
			putnext(q, mp);
		} else
			freemsg(mp);
		return;
	}
	arh = (arh_t *)mp->b_cont->b_rptr;
	/* Is it one we are interested in? */
	if (BE16_TO_U16(arh->arh_proto) != IP_ARP_PROTO_TYPE) {
		freemsg(mp);
		return;
	}
	bcopy((char *)&arh[1] + (arh->arh_hlen & 0xFF), &src, IP_ADDR_LEN);
	ire = ire_route_lookup(src, 0, 0, 0, NULL, NULL, NULL,
	    MATCH_IRE_DSTONLY);

	arcn = (arcn_t *)mp->b_rptr;
	switch (arcn->arcn_code) {
	case AR_CN_BOGON:
		/*
		 * Someone is sending ARP packets with a source protocol
		 * address which we have published.  Either they are
		 * pretending to be us, or we have been asked to proxy
		 * for a machine that can do fine for itself, or two
		 * different machines are providing proxy service for the
		 * same protocol address, or something.  We try and do
		 * something appropriate here.
		 */
		cp2 = (u_char *)&arh[1];
		cp1 = hbuf;
		*cp1 = '\0';
		for (i1 = arh->arh_hlen; i1--; cp1 += 3)
			(void) mi_sprintf(cp1, "%02x:", *cp2++ & 0xff);
		if (cp1 != hbuf)
			cp1[-1] = '\0';
		(void) ip_dot_addr(src, sbuf);
		if (ire	&& IRE_IS_LOCAL(ire)) {
			/* mi_strlog(q, 1, SL_TRACE|SL_CONSOLE, */
			cmn_err(CE_WARN,
			    "IP: Hardware address '%s' trying"
			    " to be our address %s!",
			    hbuf, sbuf);
		} else {
			/* mi_strlog(q, 1, SL_TRACE, */
			cmn_err(CE_WARN,
			    "IP: Proxy ARP problem?  "
			    "Hardware address '%s' thinks it is %s",
			    hbuf, sbuf);
		}
		break;
	case AR_CN_ANNOUNCE:
		/*
		 * ARP gives us a copy of any broadcast packet with identical
		 * sender and receiver protocol address, in
		 * case we want to intuit something from it.  Such a packet
		 * usually means that a machine has just come up on the net.
		 * If we have an IRE_CACHE, we blow it away.  This way we will
		 * immediately pick up the rare case of a host changing
		 * hardware address.
		 */
		if (ire) {
			if (ire->ire_type == IRE_CACHE)
				ire_delete(ire);
			/*
			 * The address in "src" may be an entry for a router.
			 * (Default router, or non-default router.)  If
			 * that's true, then any off-net IRE_CACHE entries
			 * that go through the router with address "src"
			 * must be clobbered.  Use ire_walk to achieve this
			 * goal.
			 *
			 * It should be possible to determine if the address
			 * in src is or is not for a router.  This way,
			 * the ire_walk() isn't called all of the time here.
			 */

			ire_walk(ire_delete_route_gw, (char *)&src);
		}
		break;
	default:
		break;
	}
	freemsg(mp);
}

/*
 * Upper level protocols (ULP) pass through bind requests to IP for inspection
 * and to arrange for power-fanout assist.  The ULP is identified by
 * adding a single byte at the end of the original bind message.
 * A ULP other than UDP or TCP that wishes to be recognized passes
 * down a bind with a zero length address.
 *
 * The binding works as follows:
 * - A zero byte address means just bind to the protocol.
 * - A four byte address is treated as a request to validate
 *   that the address is a valid local address, appropriate for
 *   an application to bind to. This does not affect any fanout
 *   information in IP.
 * - A sizeof ipa_t byte address is used to bind to only the local address
 *   and port.
 * - A sizeof ipa_conn_t byte address contains complete fanout information
 *   consisting of local and remote addresses and ports.  In
 *   this case, the addresses are both validated as appropriate
 *   for this operation, and, if so, the information is retained
 *   for use in the inbound fanout.
 *
 * The ULP (except in the zero-length bind) can append an
 * additional mblk of db_type IRE_DB_REQ_TYPE to the T_BIND_REQ/O_T_BIND_REQ.
 * This indicates that the ULP wants a copy of the source or destination IRE
 * (source for local bind; destination for complete bind).
 */
static void
ip_bind(queue_t *q, mblk_t *mp)
{
	int		error = 0;
	ipc_t		*ipc = (ipc_t *)q->q_ptr;
	icf_t		*icf;
	ssize_t		len;
	int		protocol;
	struct T_bind_req	*tbr;
	ipa_t		*ipa;
	ipa_conn_t	*ac;
	ipaddr_t	src_addr;
	ipaddr_t	dst_addr;
	uint16_t		lport;
	uint16_t		fport;
	u_char		*ucp;
	ire_t   	*src_ire;
	ire_t		*dst_ire;
	mblk_t		*mp1;
	u_int		ire_requested;

	len = mp->b_wptr - mp->b_rptr;
	if (len < (sizeof (*tbr) + 1)) {
		(void) mi_strlog(q, 1, SL_ERROR|SL_TRACE,
		    "ip_bind: bogus msg, len %ld", len);
		freemsg(mp);
		return;
	}
	/* Back up and extract the protocol identifier. */
	mp->b_wptr--;
	protocol = *mp->b_wptr & 0xFF;
	tbr = (struct T_bind_req *)mp->b_rptr;
	/* Reset the message type in preparation for shipping it back. */
	mp->b_datap->db_type = M_PCPROTO;

	/*
	 * Check for a zero length address.  This is from a protocol that
	 * wants to register to receive all packets of its type.
	 */
	if (tbr->ADDR_length == 0) {
		/* No hash here really.  The table is big enough. */
		ipc->ipc_laddr = 0;
		icf = &ipc_proto_fanout[protocol];
		ipc_hash_insert_wildcard(icf, ipc);
		tbr->PRIM_type = T_BIND_ACK;
		qreply(q, mp);
		return;
	}

	/* Extract the address pointer from the message. */
	/* TODO alignment? */
	ucp = (u_char *)mi_offset_param(mp, tbr->ADDR_offset,
	    tbr->ADDR_length);
	if (ucp == NULL) {
		ip1dbg(("ip_bind: no address\n"));
		goto bad_addr;
	}

	if (tbr->ADDR_length < IP_ADDR_LEN) {
		ip1dbg(("ip_bind: bad address length %d\n",
		    (int)tbr->ADDR_length));
		goto bad_addr;
	}
	mp1 = mp->b_cont;	/* trailing mp if any */
	ire_requested = (mp1 && mp1->b_datap->db_type == IRE_DB_REQ_TYPE);
	src_ire = dst_ire = NULL;

	switch (tbr->ADDR_length) {
	default:
		goto bad_addr;

	case IP_ADDR_LEN:
		/* Verification of local address only */
		src_addr = *(ipaddr_t *)ucp;
		lport = 0;
		goto local_bind;

	case sizeof (ipa_t):
		/*
		 * Here address is verified to be a valid local address.
		 * If the IRE_DB_REQ_TYPE mp is present, a broadcast/multicast
		 * address is also considered a valid local address.
		 * In the case of a broadcast/multicast address, however, the
		 * upper protocol is expected to reset the src address
		 * to 0 if it sees a IRE_BROADCAST type returned so that
		 * no packets are emitted with broadcast address as
		 * source address (that violates hosts requirements RFC1122)
		 * The addresses valid for bind are:
		 * 	(1) - INADDR_ANY (0)
		 *	(2) - IP address of an UP interface
		 *	(3) - IP address of a DOWN interface
		 *	(4) - valid local IP broadcast addresses. In this case
		 *	the ipc will only receive packets destined to
		 *	the specified broadcast address.
		 *	(5) - a multicast address. In this case
		 *	the ipc will only receive packets destined to
		 *	the specified multicast address. Note: the
		 *	application still has to issue an
		 *	IP_ADD_MEMBERSHIP socket option.
		 *
		 */
		ipa = (ipa_t *)ucp;
		src_addr = *(ipaddr_t *)ipa->ip_addr;
		lport = *(uint16_t *)ipa->ip_port;

	local_bind:
		dst_addr = INADDR_ANY;
		fport = 0;

		if (src_addr) {
			src_ire = ire_route_lookup(src_addr, 0, 0, 0, NULL,
			    NULL, NULL, MATCH_IRE_DSTONLY);
			/*
			 * If an address other than 0.0.0.0 is requested,
			 * we verify that it is a valid address for bind
			 * Note: Following code is in if-else-if form for
			 * readability compared to a condition check.
			 */
			/* LINTED - statement has no consequent */
			if (IRE_IS_LOCAL(src_ire)) {
				/*
				 * (2) Bind to address of local UP interface
				 */
			} else if (src_ire &&
			    src_ire->ire_type == IRE_BROADCAST) {
				/*
				 * (4) Bind to broadcast address
				 * Note: permitted only from transports that
				 * request IRE
				 */
				if (!ire_requested)
					error = EADDRNOTAVAIL;
			} else if (ipif_lookup_addr(src_addr) != NULL) {
				/*
				 * (3) Bind to address of local DOWN interface
				 * (ipif_lookup_addr() looks up all interfaces
				 * but we do not get here for UP interfaces
				 * - case (2) above)
				 */
				/*EMPTY*/;
			} else if (CLASSD(src_addr)) {

				/*
				 * (5) bind to multicast address.
				 * Fake out the IRE returned to upper layer to
				 * be a broadcast IRE.
				 */
				src_ire = ire_ctable_lookup(INADDR_BROADCAST,
				    INADDR_ANY, IRE_BROADCAST, NULL, NULL,
				    MATCH_IRE_TYPE);
				if (src_ire == NULL || !ire_requested)
					error = EADDRNOTAVAIL;

			} else {
				/*
				 * Not a valid address for bind
				 */
				error = EADDRNOTAVAIL;
			}

			if (error) {
				/* Red Alert!  Attempting to be a bogon! */
				ip1dbg(("ip_bind: bad src address 0x%x\n",
				    ntohl(src_addr)));
				goto bad_addr;
			}
		}
		break;


	case sizeof (ipa_conn_t):
		/*
		 * Verify that both the source and destination addresses
		 * are valid.
		 * Note that we allow connect to broadcast and multicast
		 * addresses when ire_requested is set. Thus the ULP
		 * has to check for IRE_BROADCAST and multicast.
		 */
		ac = (ipa_conn_t *)ucp;
		src_addr = ac->ac_laddr;
		dst_addr = ac->ac_faddr;
		fport = ac->ac_fport;
		lport = ac->ac_lport;

		if (CLASSD(dst_addr)) {
			dst_ire = ire_route_lookup(ip_g_all_ones, 0, 0, 0, NULL,
			    NULL,  NULL,
			    MATCH_IRE_RECURSIVE| MATCH_IRE_DEFAULT);
		} else {
			dst_ire = ire_route_lookup(dst_addr, 0, 0, 0, NULL,
			    NULL,  NULL,
			    MATCH_IRE_RECURSIVE| MATCH_IRE_DEFAULT);
		}
		/* dst_ire can't be a broadcast when not ire_requested */
		if (dst_ire == NULL ||
		    ((dst_ire->ire_type & IRE_BROADCAST) && !ire_requested)) {
			ip1dbg(("ip_bind: bad connected dst 0x%x\n",
			    ntohl(dst_addr)));
			error = ENETUNREACH;
			goto bad_addr;
		}

		/*
		 * Supply a local source address such that
		 * interface group balancing happens.
		 */
		if (src_addr == INADDR_ANY) {
			/*
			 * Do the moral equivalent of parts of
			 * ip_newroute(), including the possible
			 * reassignment of dst_ire.  Reassignment
			 * should happen if it is enabled, and the
			 * logical interface in question isn't in
			 * a singleton group.
			 */
			ipif_t *dst_ipif = dst_ire->ire_ipif;
			ipif_t *sched_ipif;
			ire_t *sched_ire;

			if (ip_enable_group_ifs &&
			    dst_ipif->ipif_ifgrpnext != dst_ipif) {

				/* Reassign dst_ire based on ifgrp. */

				sched_ipif = ifgrp_scheduler(dst_ipif);
				if (sched_ipif != NULL) {
					sched_ire = ipif_to_ire(sched_ipif);
					/*
					 * Reassign dst_ire to
					 * correspond to the results
					 * of ifgrp scheduling.
					 */
					if (sched_ire != NULL)
						dst_ire = sched_ire;
				}
			}
			src_addr = dst_ire->ire_src_addr;
		}

		/*
		 * We do ire_route_lookup() here (and not
		 * interface lookup as we assert that
		 * src_addr should only come from an
		 * UP interface for hard binding.
		 */
		src_ire = ire_route_lookup(src_addr, 0, 0, 0, NULL,
		    NULL, NULL, MATCH_IRE_DSTONLY);

		/* src_ire can't be a broadcast. */
		if (!IRE_IS_TARGET(src_ire)) {
			ip1dbg(("ip_bind: bad connected src 0x%x\n",
			    ntohl(src_addr)));
			error = EADDRNOTAVAIL;
			goto bad_addr;
		}

		/*
		 * If the source address is a loopback address, the
		 * destination had best be local or multicast.
		 * The transports that can't handle multicast will reject
		 * those addresses.
		 */
		if (src_ire->ire_type == IRE_LOOPBACK &&
		    !(IRE_IS_LOCAL(dst_ire) || CLASSD(dst_addr))) {
			ip1dbg(("ip_bind: bad connected loopback\n"));
			goto bad_addr;
		}
		break;
	}

	if (tbr->ADDR_length == IP_ADDR_LEN) {
		/* No insertion in fanouts - just verification of address */
		goto done;
	}

	/*
	 * The addresses have been verified. Time to insert in the correct
	 * fanout list.
	 */
	ipc->ipc_laddr = src_addr;
	ipc->ipc_faddr = dst_addr;
	ipc->ipc_lport = lport;
	ipc->ipc_fport = fport;

	switch (protocol) {
	case IPPROTO_UDP:
	default:
		/*
		 * Note the requested port number and IP address for use
		 * in the inbound fanout.  Validation (and uniqueness) of
		 * the port/address request is UDPs business.
		 */
		if (protocol == IPPROTO_UDP) {
			icf = &ipc_udp_fanout[IP_UDP_HASH(ipc->ipc_lport)];
		} else {
			/* No hash here really.  The table is big enough. */
			icf = &ipc_proto_fanout[protocol];
		}
		/*
		 * Insert entries with a specified remote address first,
		 * followed by those with a specified local address and
		 * ending with those bound to INADDR_ANY. This ensures
		 * that the search from the beginning of a hash bucket
		 * will find the most specific match.
		 * IP_UDP_MATCH assumes this insertion order.
		 */
		if (dst_addr != INADDR_ANY)
			ipc_hash_insert_connected(icf, ipc);
		else if (src_addr != INADDR_ANY)
			ipc_hash_insert_bound(icf, ipc);
		else
			ipc_hash_insert_wildcard(icf, ipc);
		break;

	case IPPROTO_TCP:
		switch (tbr->ADDR_length) {
		case sizeof (ipa_conn_t):
			/* Insert the IPC in the TCP fanout hash table. */
			icf = &ipc_tcp_conn_fanout[
			    IP_TCP_CONN_HASH(ipc->ipc_faddr, ipc->ipc_ports)];
			ipc_hash_insert_connected(icf, ipc);
			break;

		case sizeof (ipa_t):
			/* Insert the IPC in the TCP listen hash table. */
			icf = &ipc_tcp_listen_fanout[
			    IP_TCP_LISTEN_HASH(ipc->ipc_lport)];
			if (ipc->ipc_laddr != INADDR_ANY)
				ipc_hash_insert_bound(icf, ipc);
			else
				ipc_hash_insert_wildcard(icf, ipc);
			break;
		}
		break;
	}

done:
	/*
	 * If IRE requested, send back a copy of the ire
	 */
	if (ire_requested) {
		ire_t *ire;

		ASSERT(mp1 != NULL);
		if (dst_ire != NULL)
			ire = dst_ire;
		else
			ire = src_ire;

		if (ire != NULL) {
			/*
			 * mp1 initialized above to IRE_DB_REQ_TYPE
			 * appended mblk. Its <upper protocol>'s
			 * job to make sure there is room.
			 */
			if ((mp1->b_datap->db_lim - mp1->b_rptr) <
			    sizeof (ire_t))
				goto bad_addr;
			mp1->b_datap->db_type = IRE_DB_TYPE;
			mp1->b_wptr = mp1->b_rptr + sizeof (ire_t);
			bcopy(ire, mp1->b_rptr, sizeof (ire_t));
		} else {
			/*
			 * No IRE was found. Free IRE request.
			 */
			mp->b_cont = NULL;
			freemsg(mp1);
		}
	}
	/* Send it home. */
	mp->b_datap->db_type = M_PCPROTO;
	tbr->PRIM_type = T_BIND_ACK;
	qreply(q, mp);
	return;

bad_addr:
	if (error)
		mp = mi_tpi_err_ack_alloc(mp, TSYSERR, error);
	else
		mp = mi_tpi_err_ack_alloc(mp, TBADADDR, 0);
	if (mp)
		qreply(q, mp);
}

/*
 * Carve "len" bytes out of an mblk chain, consuming any we empty, and duping
 * the final piece where we don't.  Return a pointer to the first mblk in the
 * result, and update the pointer to the next mblk to chew on.  If anything
 * goes wrong (i.e., dupb fails), we waste everything in sight and return a
 * nil pointer.
 */
static mblk_t *
ip_carve_mp(mblk_t **mpp, ssize_t len)
{
	mblk_t	*mp0;
	mblk_t	*mp1;
	mblk_t	*mp2;

	if (!len || !mpp || !(mp0 = *mpp))
		return (nilp(mblk_t));
	/* If we aren't going to consume the first mblk, we need a dup. */
	if (mp0->b_wptr - mp0->b_rptr > len) {
		mp1 = dupb(mp0);
		if (mp1) {
			/* Partition the data between the two mblks. */
			mp1->b_wptr = mp1->b_rptr + len;
			mp0->b_rptr = mp1->b_wptr;
			/*
			 * after adjustments if mblk not consumed is now
			 * unaligned, try to align it. If this fails free
			 * all messages and let upper layer recover.
			 */
			if (!OK_32PTR(mp0->b_rptr)) {
				if (!pullupmsg(mp0, -1)) {
					freemsg(mp0);
					freemsg(mp1);
					*mpp = nilp(mblk_t);
					return (nilp(mblk_t));
				}
			}
		}
		return (mp1);
	}
	/* Eat through as many mblks as we need to get len bytes. */
	len -= mp0->b_wptr - mp0->b_rptr;
	for (mp2 = mp1 = mp0; (mp2 = mp2->b_cont) != 0 && len; mp1 = mp2) {
		if (mp2->b_wptr - mp2->b_rptr > len) {
			/*
			 * We won't consume the entire last mblk.  Like
			 * above, dup and partition it.
			 */
			mp1->b_cont = dupb(mp2);
			mp1 = mp1->b_cont;
			if (!mp1) {
				/*
				 * Trouble.  Rather than go to a lot of
				 * trouble to clean up, we free the messages.
				 * This won't be any worse than losing it on
				 * the wire.
				 */
				freemsg(mp0);
				freemsg(mp2);
				*mpp = nilp(mblk_t);
				return (nilp(mblk_t));
			}
			mp1->b_wptr = mp1->b_rptr + len;
			mp2->b_rptr = mp1->b_wptr;
			/*
			 * after adjustments if mblk not consumed is now
			 * unaligned, try to align it. If this fails free
			 * all messages and let upper layer recover.
			 */
			if (!OK_32PTR(mp2->b_rptr)) {
				if (!pullupmsg(mp2, -1)) {
					freemsg(mp0);
					freemsg(mp2);
					*mpp = nilp(mblk_t);
					return (nilp(mblk_t));
				}
			}
			*mpp = mp2;
			return (mp0);
		}
		/* Decrement len by the amount we just got. */
		len -= mp2->b_wptr - mp2->b_rptr;
	}
	/*
	 * len should be reduced to zero now.  If not our caller has
	 * screwed up.
	 */
	if (len) {
		/* Shouldn't happen! */
		freemsg(mp0);
		*mpp = nilp(mblk_t);
		return (nilp(mblk_t));
	}
	/*
	 * We consumed up to exactly the end of an mblk.  Detach the part
	 * we are returning from the rest of the chain.
	 */
	mp1->b_cont = nilp(mblk_t);
	*mpp = mp2;
	return (mp0);
}

/*
 * IP has been configured as _D_QNEXTLESS for the client side i.e the driver
 * instance. This implies that
 * 1. IP cannot access the read side q_next pointer directly - it must
 *    use routines like putnext and canputnext.
 * 2. ip_close must ensure that all sources of messages being putnext upstream
 *    are gone before qprocsoff is called.
 *
 * #2 is handled by having ip_close do the ipc_hash_remove and wait for
 * ipc_refcnt to drop to zero before calling qprocsoff.
 */
static int
ip_close(queue_t *q)
{
	ill_t *ill;
	ipc_t *ipc;
	void  *ptr = q->q_ptr;

	TRACE_1(TR_FAC_IP, TR_IP_CLOSE, "ip_close: q %p", q);

	/*
	 * Call the appropriate delete routine depending on whether this is
	 * a module or device. Since we are _D_MTOCSHARED anything that
	 * needs to run as a writer is handled by the service
	 * procedure. The close routine only handles removing ipc's from
	 * the binding hash lists.
	 * The close routine uses ill_close_flags/ipc_close_flags to
	 * synchronize with ip_wsrv. These fields are only accessed
	 * by ip_open, ip_close, and ip_wsrv and the ordering of setting
	 * the flags ensures that the atomicify of 32 bit load/store
	 * is sufficient thus no locking is needed.
	 */
	if (WR(q)->q_next) {
		ill = (ill_t *)ptr;

		ill->ill_close_flags |= IPCF_CLOSING;
		while (!(ill->ill_close_flags & IPCF_CLOSE_DONE)) {
			qenable(ill->ill_wq);
			qwait(ill->ill_wq);
		}
	} else {
		ipc = (ipc_t *)ptr;

		/*
		 * First remove this ipc from any fanout list it is on.
		 * Then wait until the number of pending putnexts from
		 * the fanout code drops to zero.
		 * Finally, if some other cleanup is needed (multicast router,
		 * multicast membership, or pending interface bringup on this
		 * ipc) let the service procedure handle that since this
		 * close procedure is not running as a writer.
		 */
		ipc_hash_remove(ipc);
		mutex_enter(&ipc->ipc_reflock);
		while (ipc->ipc_refcnt != 0)
			cv_wait(&ipc->ipc_refcv, &ipc->ipc_reflock);
		mutex_exit(&ipc->ipc_reflock);

		mutex_destroy(&ipc->ipc_reflock);
		cv_destroy(&ipc->ipc_refcv);
		if (q == ip_g_mrouter || WR(q) == ip_g_mrouter ||
		    ipc->ipc_ilg_inuse != 0 || ipc->ipc_bind_ill != NULL) {
			ipc->ipc_close_flags |= IPCF_CLOSING;
			while (!(ipc->ipc_close_flags & IPCF_CLOSE_DONE)) {
				qenable(ipc->ipc_wq);
				qwait(ipc->ipc_wq);
			}
		}
	}
	qprocsoff(q);

	mutex_enter(&ip_mi_lock);
	mi_close_unlink(&ip_g_head, (IDP)ptr);
	mutex_exit(&ip_mi_lock);

	mi_close_free((IDP)ptr);
	q->q_ptr = WR(q)->q_ptr = NULL;

	return (0);
}

/* Return the IP checksum for the IP header at "iph". */
u_int
ip_csum_hdr(ipha_t *ipha)
{
	uint16_t	*uph;
	uint32_t	sum;
	int	opt_len;

	opt_len = (ipha->ipha_version_and_hdr_length & 0xF) -
	    IP_SIMPLE_HDR_LENGTH_IN_WORDS;
	uph = (uint16_t *)ipha;
	sum = uph[0] + uph[1] + uph[2] + uph[3] + uph[4] +
		uph[5] + uph[6] + uph[7] + uph[8] + uph[9];
	if (opt_len > 0) {
		do {
			sum += uph[10];
			sum += uph[11];
			uph += 2;
		} while (--opt_len);
	}
	sum = (sum & 0xFFFF) + (sum >> 16);
	sum = ~(sum + (sum >> 16)) & 0xFFFF;
	if (sum == 0xffff)
		sum = 0;
	return ((u_int)sum);
}

void
ip_ddi_destroy(void)
{
	extern kmutex_t igmp_ilm_lock;
	extern kmutex_t ire_handle_lock;
	extern kmutex_t ifgrp_l_mutex;

	nd_free(&ip_g_nd);

	mutex_destroy(&igmp_ilm_lock);
	mutex_destroy(&ire_handle_lock);
	mutex_destroy(&ifgrp_l_mutex);
	mutex_destroy(&ip_mi_lock);
	ip_fanout_destroy();
}

void
ip_ddi_init(void)
{
	extern kmutex_t igmp_ilm_lock;
	extern kmutex_t ire_handle_lock;
	extern kmutex_t ifgrp_l_mutex;

	if (!ip_g_nd) {
		if (!ip_param_register(lcl_param_arr, A_CNT(lcl_param_arr))) {
			nd_free(&ip_g_nd);
		}
	}

	mutex_init(&igmp_ilm_lock, NULL, MUTEX_DEFAULT, 0);
	mutex_init(&ire_handle_lock, NULL, MUTEX_DEFAULT, NULL);
	mutex_init(&ifgrp_l_mutex, NULL, MUTEX_DEFAULT, 0);
	mutex_init(&ip_mi_lock, NULL, MUTEX_DEFAULT, NULL);
	ip_fanout_init();
}


/*
 * Allocate and initialize a DLPI template of the specified length.  (May be
 * called as writer.)
 */
mblk_t *
ip_dlpi_alloc(size_t len, t_uscalar_t prim)
{
	mblk_t	*mp;

	mp = allocb(len, BPRI_MED);
	if (!mp)
		return (nilp(mblk_t));
	mp->b_datap->db_type = M_PROTO;
	mp->b_wptr = mp->b_rptr + len;
	bzero(mp->b_rptr, len);
	((dl_unitdata_req_t *)mp->b_rptr)->dl_primitive = prim;
	return (mp);
}

/*
 * Debug formatting routine.  Returns a character string representation of the
 * addr in buf, of the form xxx.xxx.xxx.xxx.  This routine takes the address
 * in the form of a ipaddr_t and calls ip_dot_saddr with a pointer.
 */
char *
ip_dot_addr(ipaddr_t addr, char *buf)
{
	return (ip_dot_saddr((u_char *)&addr, buf));
}

/*
 * Debug formatting routine.  Returns a character string representation of the
 * addr in buf, of the form xxx.xxx.xxx.xxx.  This routine takes the address
 * as a pointer.  The "xxx" parts including left zero padding so the final
 * string will fit easily in tables.  It would be nice to take a padding
 * length argument instead.
 */
static char *
ip_dot_saddr(u_char *addr, char *buf)
{
	(void) mi_sprintf(buf, "%03d.%03d.%03d.%03d",
	    addr[0] & 0xFF, addr[1] & 0xFF, addr[2] & 0xFF, addr[3] & 0xFF);
	return (buf);
}

void
ip_fanout_destroy(void)
{
	int i;

	for (i = 0; i < A_CNT(ipc_udp_fanout); i++) {
		mutex_destroy(&ipc_udp_fanout[i].icf_lock);
	}
	for (i = 0; i < A_CNT(ipc_tcp_conn_fanout); i++) {
		mutex_destroy(&ipc_tcp_conn_fanout[i].icf_lock);
	}
	for (i = 0; i < A_CNT(ipc_tcp_listen_fanout); i++) {
		mutex_destroy(&ipc_tcp_listen_fanout[i].icf_lock);
	}
	for (i = 0; i < A_CNT(ipc_proto_fanout); i++) {
		mutex_destroy(&ipc_proto_fanout[i].icf_lock);
	}

	mutex_destroy(&rts_clients.icf_lock);
}

void
ip_fanout_init(void)
{
	int i;

	for (i = 0; i < A_CNT(ipc_udp_fanout); i++) {
		mutex_init(&ipc_udp_fanout[i].icf_lock, NULL,
		    MUTEX_DEFAULT, NULL);
	}
	for (i = 0; i < A_CNT(ipc_tcp_conn_fanout); i++) {
		mutex_init(&ipc_tcp_conn_fanout[i].icf_lock, NULL,
		    MUTEX_DEFAULT, NULL);
	}
	for (i = 0; i < A_CNT(ipc_tcp_listen_fanout); i++) {
		mutex_init(&ipc_tcp_listen_fanout[i].icf_lock, NULL,
		    MUTEX_DEFAULT, NULL);
	}
	for (i = 0; i < A_CNT(ipc_proto_fanout); i++) {
		mutex_init(&ipc_proto_fanout[i].icf_lock, NULL,
		    MUTEX_DEFAULT, NULL);
	}

	mutex_init(&rts_clients.icf_lock, NULL, MUTEX_DEFAULT, NULL);
}

/*
 * Send an ICMP error after patching up the packet appropriately.
 */
static void
ip_fanout_send_icmp(queue_t *q, mblk_t *mp, u_int flags, Counter *mibincr,
		    u_int icmp_type, u_int icmp_code)
{
	ipha_t *ipha = (ipha_t *)mp->b_rptr;

	if (flags & IP_FF_SEND_ICMP) {
		BUMP_MIB(*mibincr);
		if (flags & IP_FF_HDR_COMPLETE) {
			if (ip_hdr_complete(ipha)) {
				freemsg(mp);
				return;
			}
		}
		if (flags & IP_FF_CKSUM) {
			/*
			 * Have to correct checksum since
			 * the packet might have been
			 * fragmented and the reassembly code in ip_rput
			 * does not restore the IP checksum.
			 */
			ipha->ipha_hdr_checksum = 0;
			ipha->ipha_hdr_checksum = (uint16_t)ip_csum_hdr(ipha);
		}
		switch (icmp_type) {
		case ICMP_DEST_UNREACHABLE:
			icmp_unreachable(WR(q), mp, icmp_code);
			break;
		case ICMP_SOURCE_QUENCH:
			icmp_source_quench(WR(q), mp);
			break;
		default:
			freemsg(mp);
			break;
		}
	} else {
		freemsg(mp);
	}
}


/*
 * Handle protocols with which IP is less intimate.  There
 * can be more than one stream bound to a particular
 * protocol.  When this is the case, each one gets a copy
 * of any incoming packets.
 */
static void
ip_fanout_proto(queue_t *q, mblk_t *mp, ipha_t *ipha, u_int flags)
{
	icf_t	*icf;
	ipc_t	*ipc, *first_ipc, *next_ipc;
	queue_t	*rq;
	mblk_t	*mp1;
	u_int	protocol = ipha->ipha_protocol;
	ipaddr_t dst = ipha->ipha_dst;

	icf = &ipc_proto_fanout[protocol];
	mutex_enter(&icf->icf_lock);
	ipc = (ipc_t *)&icf->icf_ipc;	/* ipc_hash_next will get first */
	do {
		ipc = ipc->ipc_hash_next;
		if (ipc == NULL) {
			/*
			 * No one bound to this port.  Is
			 * there a client that wants all
			 * unclaimed datagrams?
			 */
			mutex_exit(&icf->icf_lock);

			ip_fanout_send_icmp(q, mp, flags,
			    &ip_mib.ipInUnknownProtos,
			    ICMP_DEST_UNREACHABLE, ICMP_PROTOCOL_UNREACHABLE);
			return;
		}
	} while (!((ipc->ipc_laddr == 0 || ipc->ipc_laddr == dst) &&
	    (ipc_wantpacket(ipc, dst) || (protocol == IPPROTO_RSVP))));

	IPC_REFHOLD(ipc);
	first_ipc = ipc;
	ipc = ipc->ipc_hash_next;
	for (;;) {
		while (ipc != NULL) {
			if ((ipc->ipc_laddr == 0 || ipc->ipc_laddr == dst) &&
			    (ipc_wantpacket(ipc, dst) ||
			    (protocol == IPPROTO_RSVP)))
				break;
			ipc = ipc->ipc_hash_next;
		}
		if (ipc == NULL || (((mp1 = dupmsg(mp)) == NULL) &&
		    ((mp1 = copymsg(mp)) == NULL))) {
			/*
			 * No more intested clients or memory
			 * allocation failed
			 */
			ipc = first_ipc;
			break;
		}
		IPC_REFHOLD(ipc);
		mutex_exit(&icf->icf_lock);
		rq = ipc->ipc_rq;
		if (protocol != IPPROTO_TCP && !canputnext(rq)) {
			Counter *mibincr;

			if (flags & IP_FF_RAWIP)
				mibincr = &ip_mib.rawipInOverflows;
			else
				mibincr = &icmp_mib.icmpInOverflows;

			if (flags & IP_FF_SRC_QUENCH) {
				ip_fanout_send_icmp(q, mp1, flags, mibincr,
				    ICMP_SOURCE_QUENCH, 0);
			} else {
				BUMP_MIB(*mibincr);
				freemsg(mp1);
			}
		} else {
			BUMP_MIB(ip_mib.ipInDelivers);
			putnext(rq, mp1);
		}
		mutex_enter(&icf->icf_lock);
		/* Follow the next pointer before releasing the ipc. */
		next_ipc = ipc->ipc_hash_next;
		IPC_REFRELE(ipc);
		ipc = next_ipc;
	}

	/* Last one.  Send it upstream. */
	mutex_exit(&icf->icf_lock);
	rq = ipc->ipc_rq;
	if (protocol != IPPROTO_TCP && !canputnext(rq)) {
		Counter *mibincr;

		if (flags & IP_FF_RAWIP)
			mibincr = &ip_mib.rawipInOverflows;
		else
			mibincr = &icmp_mib.icmpInOverflows;

		if (flags & IP_FF_SRC_QUENCH) {
			ip_fanout_send_icmp(q, mp, flags, mibincr,
			    ICMP_SOURCE_QUENCH, 0);
		} else {
			BUMP_MIB(*mibincr);
			freemsg(mp);
		}
	} else {
		BUMP_MIB(ip_mib.ipInDelivers);
		putnext(rq, mp);
	}
	IPC_REFRELE(ipc);
}

/*
 * Fanout for TCP packets
 * The caller puts <fport, lport> in the ports parameter.
 */
static void
ip_fanout_tcp_listen(queue_t *q, mblk_t *mp, ipha_t *ipha,
    uint32_t ports, u_int flags)
{
	icf_t	*icf;
	ipc_t	*ipc;
	queue_t	*rq;
	uint16_t	dstport;
	int	hdr_len;
	tcph_t	*tcph;

	/* If this is a SYN packet attempt to add an IRE_DB_TYPE */
	hdr_len = IPH_HDR_LENGTH(mp->b_rptr);
	tcph = (tcph_t *)&mp->b_rptr[hdr_len];

	if ((flags & IP_FF_SYN_ADDIRE) &&
	    (tcph->th_flags[0] & (TH_SYN|TH_ACK)) == TH_SYN) {
		/* SYN without the ACK - add an IRE for the source */
		ip_ire_append(mp, ipha->ipha_src);
	}

	/* Extract port in net byte order */
	dstport = htons(ntohl(ports) & 0xFFFF);

	icf = &ipc_tcp_listen_fanout[IP_TCP_LISTEN_HASH(dstport)];
	mutex_enter(&icf->icf_lock);
	ipc = (ipc_t *)&icf->icf_ipc;	/* ipc_hash_next will get first */
	do {
		ipc = ipc->ipc_hash_next;
		if (ipc == NULL) {
			/*
			 * No match on local port. Look for a client
			 * that wants all unclaimed.  Note
			 * that TCP must normally make sure that
			 * there is such a stream, otherwise it
			 * will be tough to get inbound connections
			 * going.
			 */
			mutex_exit(&icf->icf_lock);
			if (ipc_proto_fanout[IPPROTO_TCP].icf_ipc != NULL) {
				ip_fanout_proto(q, mp, ipha,
				    flags | IP_FF_RAWIP);
			} else {
				ip_fanout_send_icmp(q, mp, flags,
				    &ip_mib.tcpInErrs,	/* XXX */
				    ICMP_DEST_UNREACHABLE,
				    ICMP_PORT_UNREACHABLE);
			}
			return;
		}
	} while (!IP_TCP_LISTEN_MATCH(ipc, dstport, ipha->ipha_dst));
	/* Got a client, up it goes. */
	IPC_REFHOLD(ipc);
	mutex_exit(&icf->icf_lock);
	rq = ipc->ipc_rq;
	BUMP_MIB(ip_mib.ipInDelivers);
	putnext(rq, mp);
	IPC_REFRELE(ipc);
}

/*
 * Fanout for TCP packets
 * The caller puts <fport, lport> in the ports parameter.
 */
static void
ip_fanout_tcp(queue_t *q, mblk_t *mp, ipha_t *ipha,
    uint32_t ports, u_int flags)
{
	icf_t	*icf;
	ipc_t	*ipc;
	queue_t	*rq;

	/* Find a TCP client stream for this packet. */
	icf = &ipc_tcp_conn_fanout[IP_TCP_CONN_HASH(ipha->ipha_src, ports)];
	mutex_enter(&icf->icf_lock);
	ipc = (ipc_t *)&icf->icf_ipc;	/* ipc_hash_next will get first */
	do {
		ipc = ipc->ipc_hash_next;
		if (ipc == NULL) {
			/*
			 * No hard-bound match.  Look for a
			 * stream bound to the local port only.
			 */
			dblk_t *dp = mp->b_datap;

			mutex_exit(&icf->icf_lock);

			if (dp->db_struioflag & STRUIO_IP) {
				/*
				 * Do the postponed checksum now.
				 */
				mblk_t *mp1;
				ssize_t off = dp->db_struioptr - mp->b_rptr;

				if (IP_CSUM(mp, (int32_t)off, 0)) {
					ipcsumdbg("swcksumerr1\n", mp);
					BUMP_MIB(ip_mib.tcpInErrs);
					freemsg(mp);
					return;
				}
				mp1 = mp;
				do {
					mp1->b_datap->db_struioflag &=
						~STRUIO_IP;
				} while ((mp1 = mp1->b_cont) != NULL);
			}
			ip_fanout_tcp_listen(q, mp, ipha, ports, flags);
			return;
		}
	} while (!IP_TCP_CONN_MATCH(ipc, ipha, ports));
	/* Got a client, up it goes. */
	IPC_REFHOLD(ipc);
	mutex_exit(&icf->icf_lock);
	rq = ipc->ipc_rq;
	BUMP_MIB(ip_mib.ipInDelivers);
	putnext(rq, mp);
	IPC_REFRELE(ipc);
}

/*
 * Fanout for UDP packets.
 * The caller puts <fport, lport> in the ports parameter.
 * ire_type must be IRE_BROADCAST for multicast and broadcast packets.
 *
 * If SO_REUSEADDR is set all multicast and broadcast packets
 * will be delivered to all streams bound to the same port.
 */
static void
ip_fanout_udp(queue_t *q, mblk_t *mp, ipha_t *ipha,
    uint32_t ports, u_short ire_type, u_int flags)
{
	icf_t	*icf;
	ipc_t	*ipc;
	queue_t	*rq;
	uint32_t	dstport, srcport;
	ipaddr_t dst;

	/* Extract ports in net byte order */
	dstport = htons(ntohl(ports) & 0xFFFF);
	srcport = htons(ntohl(ports) >> 16);
	dst = ipha->ipha_dst;

	/* Attempt to find a client stream based on destination port. */
	icf = &ipc_udp_fanout[IP_UDP_HASH(dstport)];
	mutex_enter(&icf->icf_lock);
	ipc = (ipc_t *)&icf->icf_ipc;	/* ipc_hash_next will get first */
	do {
		ipc = ipc->ipc_hash_next;
		if (ipc == NULL) {
			/*
			 * No one bound to this port.  Is
			 * there a client that wants all
			 * unclaimed datagrams?
			 */
			mutex_exit(&icf->icf_lock);

			if (ipc_proto_fanout[IPPROTO_UDP].icf_ipc != NULL) {
				ip_fanout_proto(q, mp, ipha,
				    flags | IP_FF_RAWIP);
			} else {
				ip_fanout_send_icmp(q, mp, flags,
				    &ip_mib.udpNoPorts,
				    ICMP_DEST_UNREACHABLE,
				    ICMP_PORT_UNREACHABLE);
			}
			return;
		}
	} while (!IP_UDP_MATCH(ipc, dstport, dst,
	    srcport, ipha->ipha_src));

	/* Found a client */
	if (ire_type != IRE_BROADCAST) {
		/* Not broadcast or multicast. Send to the one client */
		IPC_REFHOLD(ipc);
		mutex_exit(&icf->icf_lock);
		rq = ipc->ipc_rq;
		if (!canputnext(rq)) {
			ip_fanout_send_icmp(q, mp, flags,
			    &ip_mib.udpInOverflows, ICMP_SOURCE_QUENCH, 0);
		} else {
			BUMP_MIB(ip_mib.ipInDelivers);
			putnext(rq, mp);
		}
		IPC_REFRELE(ipc);
		return;
	}

	/*
	 * Broadcast and multicast case
	 * (CLASSD addresses will come in on an IRE_BROADCAST)
	 */
	if (ipc->ipc_reuseaddr) {
		ipc_t		*first_ipc = ipc;
		ipc_t		*next_ipc;
		mblk_t		*mp1;
		ipaddr_t	src = ipha->ipha_src;

		IPC_REFHOLD(ipc);
		ipc = ipc->ipc_hash_next;
		for (;;) {
			while (ipc != NULL) {
				if (IP_UDP_MATCH(ipc, dstport, dst,
				    srcport, src) &&
				    ipc_wantpacket(ipc, dst))
					break;
				ipc = ipc->ipc_hash_next;
			}
			if (ipc == NULL || (((mp1 = dupmsg(mp)) == NULL) &&
			    ((mp1 = copymsg(mp)) == NULL))) {
				/*
				 * No more intested clients or memory
				 * allocation failed
				 */
				ipc = first_ipc;
				break;
			}
			IPC_REFHOLD(ipc);
			mutex_exit(&icf->icf_lock);
			rq = ipc->ipc_rq;
			if (!canputnext(rq)) {
				BUMP_MIB(ip_mib.udpInOverflows);
				freemsg(mp1);
			} else {
				BUMP_MIB(ip_mib.ipInDelivers);
				putnext(rq, mp1);
			}
			mutex_enter(&icf->icf_lock);
			/* Follow the next pointer before releasing the ipc. */
			next_ipc = ipc->ipc_hash_next;
			IPC_REFRELE(ipc);
			ipc = next_ipc;
		}
	} else {
		IPC_REFHOLD(ipc);
	}

	/* Last one.  Send it upstream. */
	mutex_exit(&icf->icf_lock);
	rq = ipc->ipc_rq;
	if (!canputnext(rq)) {
		BUMP_MIB(ip_mib.udpInOverflows);
		freemsg(mp);
	} else {
		BUMP_MIB(ip_mib.ipInDelivers);
		putnext(rq, mp);
	}
	IPC_REFRELE(ipc);
}

/*
 * Complete the ip_wput header so that it
 * is possible to generated ICMP
 * errors.
 */
static int
ip_hdr_complete(ipha_t *ipha)
{
	ire_t	*ire = nilp(ire_t);

	if (ipha->ipha_src)
		ire = ire_route_lookup(ipha->ipha_src, 0, 0,
		    (IRE_LOCAL|IRE_LOOPBACK), NULL, NULL, NULL,
		    MATCH_IRE_TYPE);
	if (ire == NULL) {
		ire = ire_lookup_local();
	}
	if (!ire || (ire->ire_type & (IRE_LOCAL|IRE_LOOPBACK)) == 0) {
		ip0dbg(("ip_hdr_complete: no source IRE\n"));
		return (1);
	}
	ipha->ipha_src = ire->ire_addr;
	ipha->ipha_ttl = (uint8_t)ip_def_ttl;
	ipha->ipha_hdr_checksum = 0;
	ipha->ipha_hdr_checksum = (uint16_t)ip_csum_hdr(ipha);
	return (0);
}

/*
 * Nobody should be sending
 * packets up this stream
 */
static void
ip_lrput(queue_t *q, mblk_t *mp)
{
	mblk_t *mp1;

	switch (mp->b_datap->db_type) {
	case M_FLUSH:
		/* Turn around */
		if (*mp->b_rptr & FLUSHW) {
			*mp->b_rptr &= ~FLUSHR;
			qreply(q, mp);
			return;
		}
		break;
	}
	/* Could receive messages that passed through ar_rput */
	for (mp1 = mp; mp1; mp1 = mp1->b_cont)
		mp1->b_prev = mp1->b_next = nilp(mblk_t);
	freemsg(mp);
}

/* Nobody should be sending packets down this stream */
/* ARGSUSED */
static void
ip_lwput(queue_t *q, mblk_t *mp)
{
	freemsg(mp);
}

/*
 * Move the first hop in any source route to ipha_dst and remove that part of
 * the source route.  Called by other protocols.  Errors in option formatting
 * are ignored - will be handled by ip_wput_options Return the final
 * destination (either ipha_dst or the last entry in a source route.)
 */
ipaddr_t
ip_massage_options(ipha_t *ipha)
{
	uint32_t	totallen;
	u_char		*opt;
	uint32_t	optval;
	uint32_t	optlen;
	ipaddr_t	dst;
	int	i;

	ip2dbg(("ip_massage_options\n"));
	dst = ipha->ipha_dst;
	totallen = ipha->ipha_version_and_hdr_length -
		(uint8_t)((IP_VERSION << 4) + IP_SIMPLE_HDR_LENGTH_IN_WORDS);
	opt = (u_char *)&ipha[1];
	totallen <<= 2;
	while (totallen != 0) {
		switch (optval = opt[IPOPT_POS_VAL]) {
		case IPOPT_EOL:
			return (dst);
		case IPOPT_NOP:
			optlen = 1;
			break;
		default:
			optlen = opt[IPOPT_POS_LEN];
		}
		if (optlen == 0 || optlen > totallen)
			return (dst);

		switch (optval) {
			uint32_t off;
		case IPOPT_SSRR:
		case IPOPT_LSRR:
			off = opt[IPOPT_POS_OFF];
			if (off < IPOPT_MINOFF_SR) {
				ip1dbg(("ip_massage_options: bad option offset "
					"%d\n", off));
				return (dst);
			}
			off--;
			if (optlen < IP_ADDR_LEN ||
			    off > optlen - IP_ADDR_LEN) {
				/* End of source route */
				ip1dbg(("ip_massage_options: end of SR\n"));
				break;
			}
			bcopy((char *)opt + off, &dst, IP_ADDR_LEN);
			ip1dbg(("ip_massage_options: next hop 0x%x\n",
			    ntohl(dst)));
			/*
			 * Update ipha_dst to be the first hop and remove the
			 * first hop from the source route (by overwriting
			 * part of the option with NOP options)
			 */
			ipha->ipha_dst = dst;
			/* Put the last entry in dst */
			off = ((optlen - IP_ADDR_LEN - 3) & ~(IP_ADDR_LEN-1))
			    + 3;
			bcopy(&opt[off], &dst, IP_ADDR_LEN);
			ip1dbg(("ip_massage_options: last hop 0x%x\n",
			    ntohl(dst)));
			/* Move down and overwrite */
			opt[IP_ADDR_LEN] = opt[0];
			opt[IP_ADDR_LEN+1] = opt[IPOPT_POS_LEN] - IP_ADDR_LEN;
			opt[IP_ADDR_LEN+2] = opt[IPOPT_POS_OFF];
			for (i = 0; i < IP_ADDR_LEN; i++)
				opt[i] = IPOPT_NOP;
			break;
		}
		totallen -= optlen;
		opt += optlen;
	}
	return (dst);
}

/*
 * Return the network mask
 * associated with the specified address.
 */
ipaddr_t
ip_net_mask(ipaddr_t addr)
{
	u_char	*up = (u_char *)&addr;
	ipaddr_t mask = 0;
	u_char	*maskp = (u_char *)&mask;

#ifdef i386
#define	TOTALLY_BRAIN_DAMAGED_C_COMPILER
#endif
#ifdef  TOTALLY_BRAIN_DAMAGED_C_COMPILER
	maskp[0] = maskp[1] = maskp[2] = maskp[3] = 0;
#endif
	if (CLASSD(addr)) {
		maskp[0] = 0xF0;
		return (mask);
	}
	if (addr == 0)
		return (0);
	maskp[0] = 0xFF;
	if ((up[0] & 0x80) == 0)
		return (mask);

	maskp[1] = 0xFF;
	if ((up[0] & 0xC0) == 0x80)
		return (mask);

	maskp[2] = 0xFF;
	if ((up[0] & 0xE0) == 0xC0)
		return (mask);

	/* Must be experimental or multicast, indicate as much */
	return ((ipaddr_t)0);
}

static ipif_t *
ip_newroute_get_src_ipif(
    ipif_t *dst_ipif,
    boolean_t islocal,
    ipaddr_t src_addr)
{
	ipif_t *src_ipif;

	if (ip_enable_group_ifs && dst_ipif->ipif_ifgrpnext != dst_ipif)
		if (src_addr != INADDR_ANY && islocal) {
			/*
			 * We already have a source address, and the packet
			 * originated here.
			 *
			 * Perform the following sets of reality checks:
			 *	- Find an ipif that is up for this source
			 *	  address.
			 *	- If it is the same ipif as for the route,
			 *	  cool, set src_ipif = NULL and continue.
			 *	  (Except when instructed to do otherwise.)
			 *	- If the ipif is not in the same ifgrp as
			 *	  for the route, set src_ipif = NULL and
			 *	  continue, because the request source
			 *	  address doesn't seem to even come CLOSE
			 *	  to what routing says.
			 *	- If the ipif is in the same ifgrp but not
			 *	  the same ipif as the ire, set src_ipif to
			 *	  this ipif.  Most likely, this source
			 *	  address was set by bind() in user space or
			 *	  by a call to ifgrp_scheduler() in
			 *	  ip_bind() or ip_ire_req() because of TCP
			 *	  source address selection.
			 *
			 * REMEMBER, if there is no ipif for the source
			 * address, then the packet is bogus.  The islocal
			 * ensures that this is not a forwarded packet.
			 */
			ire_t *src_ire;

			src_ire = ire_ctable_lookup(src_addr, 0,
			    IRE_LOCAL, NULL, NULL, MATCH_IRE_TYPE);
			if (src_ire == NULL) {
				char abuf[30];

				/*
				 * Locally-originated packet with source
				 * address that's not attached to an up
				 * interface.  Possibly a deliberately forged
				 * IP datagram.
				 */
				ip1dbg(("Packet from me with non-up src!\n"));
				ip1dbg(("Address is %s.\n",
				    ip_dot_addr(src_addr, abuf)));
				src_ipif = ifgrp_scheduler(dst_ipif);
			} else {
				src_ipif = src_ire->ire_ipif;
				ASSERT(src_ipif != NULL);
			}

			if (src_ipif == dst_ipif ||
			    (src_ipif->ipif_local_addr &
				src_ipif->ipif_net_mask) !=
			    (dst_ipif->ipif_local_addr &
				dst_ipif->ipif_net_mask)) {
				/*
				 * Setting src_ipif to dst_ipif means to just
				 * use the ire obtained by the initial
				 * ire_lookup_loop.  We do this if the source
				 * address matches the ire's source address,
				 * or if the ire's outbound interface is not
				 * in the same ifgrp as the source address.
				 * (There is a possibility of multiple
				 * prefixes on the same interface, or
				 * interface group, but we punt on that for
				 * now.)
				 *
				 * We perform that last reality check by
				 * checking prefixes.
				 */
				src_ipif = dst_ipif;
			}

			/*
			 * If I reach here without explicitly scheduling
			 * src_ipif or setting it to dst_ipif, then the
			 * source address ipif is in the same interface
			 * group as the ire's ipif, but it is not the
			 * same actual ipif.  So basically fallthrough
			 * with src_ipif set to what the source address
			 * says.  This means the new route will go out
			 * the interface assigned to the (probably
			 * user-specified) source address.  This may
			 * upset the balance.
			 */
		} else  /* No specified source address or forwarded packet. */
			src_ipif = ifgrp_scheduler(dst_ipif);
	else src_ipif = dst_ipif;

	/*
	 * If the new source ipif isn't the same type as the dest, I can't
	 * send packets out the other interface in the group, because of
	 * potential link-level header differences, and a bunch of other
	 * cruft.
	 */
	if (dst_ipif->ipif_type != src_ipif->ipif_type)
		src_ipif = dst_ipif;

	return (src_ipif);
}

/*
 * ip_newroute is called by ip_rput or ip_wput whenever we need to send
 * out a packet to a destination address for which we do not have specific
 * routing information.
 */
static void
ip_newroute(queue_t *q, mblk_t *mp, ipaddr_t dst)
{
	areq_t	*areq;
	ipaddr_t gw = 0;
	ire_t	*ire;
	mblk_t	*res_mp;
	queue_t *stq;
	ipaddr_t *addrp;
	ipif_t  *src_ipif;
	ipha_t  *ipha = (ipha_t *)mp->b_rptr;
	u_int	ll_hdr_len;
	mblk_t	*ll_hdr_mp;
	ire_t	*sire = NULL;

	ip1dbg(("ip_newroute: dst 0x%x\n", ntohl(dst)));

	/* All multicast lookups come through ip_newroute_ipif() */
	if (CLASSD(dst)) {
		ip0dbg(("ip_newroute: CLASSD 0x%x (b_prev %p, b_next %p)\n",
		    ntohl(dst), (void *)mp->b_prev, (void *)mp->b_next));
		freemsg(mp);
		return;
	}
	/*
	 * Get what we can from ire_ftable_lookup which will follow an IRE
	 * chain until it gets the most specific information available.
	 * For example, we know that there is no IRE_CACHE for this dest,
	 * but there may be an IRE_PREFIX which specifies a gateway.
	 * ire_ftable_lookup will look up the gateway, etc.
	 */
	ire = ire_ftable_lookup(dst, 0, 0, 0, NULL, &sire, NULL,
	    (MATCH_IRE_RECURSIVE | MATCH_IRE_DEFAULT | MATCH_IRE_RJ_BHOLE));
	if (ire == NULL) {
		ip_rts_change(RTM_MISS, dst, 0, 0, 0, 0, 0, RTA_DST);
		goto err_ret;
	}
	if (ire->ire_flags & (RTF_REJECT | RTF_BLACKHOLE))
		goto err_ret;
	if (sire)
		gw = sire->ire_gateway_addr;

	ip2dbg(("\tire type %s (%d)\n",
	    ip_nv_lookup(ire_nv_tbl, ire->ire_type), (int)ire->ire_type));

	/* Reality check. */
	if (ire->ire_type != IRE_CACHE && ire->ire_type != IRE_IF_RESOLVER &&
	    ire->ire_type != IRE_IF_NORESOLVER)
		goto err_ret;

	/*
	 * Increment the ire_ob_pkt_count field for ire if it is an INTERFACE
	 * (IF_RESOLVER or IF_NORESOLVER) IRE type, and increment the same for
	 * the parent IRE, sire, if it is some sort of prefix IRE (which
	 * includes DEFAULT, PREFIX, HOST and HOST_REDIRECT).
	 */
	if ((ire->ire_type & IRE_INTERFACE) != 0)
		ire->ire_ob_pkt_count++;
	if (sire != NULL &&
	    (sire->ire_type & (IRE_CACHETABLE | IRE_INTERFACE)) == 0)
		sire->ire_ob_pkt_count++;

	/*
	 * If the interface belongs to an interface group, make sure the next
	 * possible interface in the group is used.  This encourages
	 * load-balancing among peers in an interface group.  Furthermore if
	 * a user has previously bound to a source address, try and use that
	 * interface if it makes good routing sense.
	 */

	/* Remember kids, mp->b_prev is the indicator of local origination! */
	src_ipif = ip_newroute_get_src_ipif(ire->ire_ipif,
	    (mp->b_prev == NULL), ipha->ipha_src);

	if (ire->ire_type != IRE_CACHE) {
		/*
		 * Other types (IRE_IF_*) I'm going to be brave and replace
		 * them with an ire determined by ipif_to_ire() call.
		 */
		ire = ipif_to_ire(src_ipif);
		if (ire == NULL)
			goto err_ret;
	}

	stq = ire->ire_stq;

	switch (ire->ire_type) {
	case IRE_CACHE:
		if (gw == 0)
			gw = ire->ire_gateway_addr;

		/*
		 * Assume DL_UNITDATA_REQ is same for all ifs
		 * in the ifgrp.  If it isn't, this code will
		 * have to be seriously rewhacked to allow the
		 * fastpath probing (such that I cache the link
		 * header in the IRE_CACHE) to work over ifgrps.
		 */
		if (ire->ire_ipif == src_ipif) {
			ll_hdr_len = ire->ire_ll_hdr_length;
			ll_hdr_mp = ire->ire_ll_hdr_mp;
		} else {
			ll_hdr_len = 0;
			if (ire->ire_ll_hdr_length != 0)
				ll_hdr_mp = ire->ire_ll_hdr_saved_mp;
			else
				ll_hdr_mp = ire->ire_ll_hdr_mp;
		}

		/* We have what we need to build an IRE_CACHE. */
		ire = ire_create(
			(u_char *)&dst,			/* dest address */
			(u_char *)&ip_g_all_ones,	/* mask */
			(u_char *)&src_ipif->ipif_local_addr,
			/* source address */
			(u_char *)&gw,			/* gateway address */
			ire->ire_max_frag,
			ll_hdr_mp,			/* xmit header */
			src_ipif->ipif_rq,		/* recv-from queue */
			src_ipif->ipif_wq,		/* send-to queue */
			IRE_CACHE,			/* IRE type */
			ire->ire_rtt,
			ire->ire_rtt_sd,
			ll_hdr_len,
			src_ipif,
			sire,				/* Parent ire */
			0);				/* flags if any */
		if (!ire)
			break;

		/* Remember the packet we want to xmit */
		ire->ire_mp->b_cont = mp;

		/*
		 * Need to become writer to add the IRE.  We continue
		 * in ire_add_then_put.
		 */
		become_exclusive(q, ire->ire_mp, (pfi_t)ire_add_then_put);
		return;
	case IRE_IF_NORESOLVER: {
		/* We have what we need to build an IRE_CACHE. */
		mblk_t	*ll_hdr_mp;
		ill_t	*ill;

		/*
		 * Create a new ll_hdr_mp with the
		 * IP gateway address in destination address in the DLPI hdr.
		 */
		ill = ire_to_ill(ire);
		if (!ill) {
			ip0dbg(("ip_newroute: ire_to_ill failed\n"));
			break;
		}
		if (ill->ill_phys_addr_length == IP_ADDR_LEN) {
			u_char *addr;

			if (gw)
				addr = (u_char *)&gw;
			else
				addr = (u_char *)&dst;
			ll_hdr_mp = ill_dlur_gen(addr,
			    ill->ill_phys_addr_length, ill->ill_sap,
			    ill->ill_sap_length);
		} else if (ire->ire_ll_hdr_mp)
			ll_hdr_mp = dupb(ire->ire_ll_hdr_mp);
		else
			break;

		if (!ll_hdr_mp)
			break;

		ire = ire_create(
			(u_char *)&dst,			/* dest address */
			(u_char *)&ip_g_all_ones,	/* mask */
			(u_char *)&ire->ire_src_addr,	/* source address */
			(u_char *)&gw,			/* gateway address */
			ire->ire_max_frag,
			ll_hdr_mp,			/* xmit header */
			ire->ire_rfq,			/* recv-from queue */
			stq,				/* send-to queue */
			IRE_CACHE,
			ire->ire_rtt,
			ire->ire_rtt_sd,
			ire->ire_ll_hdr_length,
			ire->ire_ipif,
			sire,				/* Parent ire */
			0);				/* flags if any */
		freeb(ll_hdr_mp);
		if (!ire)
			break;

		/* Remember the packet we want to xmit */
		ire->ire_mp->b_cont = mp;

		/*
		 * Need to become writer to add the IRE.  We continue
		 * in ire_add_then_put.
		 */
		become_exclusive(q, ire->ire_mp, (pfi_t)ire_add_then_put);
		return;
	}
	case IRE_IF_RESOLVER:
		/*
		 * We can't build an IRE_CACHE yet, but at least we found a
		 * resolver that can help.
		 */
		res_mp = ire->ire_ll_hdr_mp;
		if (!stq || !OK_RESOLVER_MP(res_mp) ||
		    !(res_mp = copyb(res_mp)))
			break;
		/*
		 * To be at this point in the code with a non-zero gw means
		 * that dst is reachable through a gateway that we have never
		 * resolved.  By changing dst to the gw addr we resolve the
		 * gateway first.  When ire_add_then_put() tries to put the IP
		 * dg to dst, it will reenter ip_newroute() at which time we
		 * will find the IRE_CACHE for the gw and create another
		 * IRE_CACHE in case IRE_CACHE above.
		 */
		if (gw) {
			dst = gw;
			gw = 0;
		}
		/* NOTE: a resolvers rfq is nil and its stq points upstream. */
		/*
		 * We obtain a partial IRE_CACHE which we will pass along with
		 * the resolver query.  When the response comes back it will be
		 * there ready for us to add.
		 */
		ire = ire_create(
			(u_char *)&dst,			/* dest address */
			(u_char *)&ip_g_all_ones,	/* mask */
			(u_char *)&ire->ire_src_addr,	/* source address */
			(u_char *)&gw,			/* gateway address */
			ire->ire_max_frag,
			res_mp,				/* xmit header */
			stq,				/* recv-from queue */
			OTHERQ(stq),			/* send-to queue */
			IRE_CACHE,
			ire->ire_rtt,
			ire->ire_rtt_sd,
			ire->ire_ll_hdr_length,
			ire->ire_ipif,
			sire,				/* Parent ire */
			0);				/* flags if any */
		freeb(res_mp);
		if (ire == NULL)
			break;

		/*
		 * Construct message chain for the resolver of the form:
		 * 	ARP_REQ_MBLK-->IRE_MBLK-->Packet
		 */
		ire->ire_mp->b_cont = mp;
		mp = ire->ire_ll_hdr_mp;
		ire->ire_ll_hdr_mp = nilp(mblk_t);
		linkb(mp, ire->ire_mp);

		/*
		 * Fill in the source and dest addrs for the resolver.
		 * NOTE: this depends on memory layouts imposed by ill_init().
		 */
		areq = (areq_t *)mp->b_rptr;
		addrp = (ipaddr_t *)((char *)areq +
		    areq->areq_sender_addr_offset);
		*addrp = ire->ire_src_addr;
		addrp = (ipaddr_t *)((char *)areq +
		    areq->areq_target_addr_offset);
		*addrp = dst;
		/* Up to the resolver. */
		putnext(stq, mp);
		/*
		 * The response will come back in ip_wput with db_type
		 * IRE_DB_TYPE.
		 */
		return;
	default:
		break;
	}

err_ret:
	ip1dbg(("ip_newroute: dropped\n"));
	/* Did this packet originate externally? */
	if (mp->b_prev) {
		mp->b_next = nilp(mblk_t);
		mp->b_prev = nilp(mblk_t);
		q = WR(q);
	} else {
		/*
		 * Since ip_wput() isn't close to finished, we fill
		 * in enough of the header for credible error reporting.
		 */
		if (ip_hdr_complete((ipha_t *)mp->b_rptr)) {
			/* Failed */
			freemsg(mp);
			return;
		}
	}
	if (ip_source_routed((ipha_t *)mp->b_rptr)) {
		icmp_unreachable(q, mp, ICMP_SOURCE_ROUTE_FAILED);
		return;
	}
	/*
	 * TODO: more precise characterization, host or net.  We can only send
	 * a net redirect if the destination network is not subnetted.
	 *
	 * At this point we will have ire only if RTF_BLACKHOLE
	 * or RTF_REJECT flags are set on the IRE. It will not
	 * generate ICMP_HOST_UNREACHABLE if RTF_BLACKHOLE is set.
	 */
	if (ire && (ire->ire_flags & RTF_BLACKHOLE)) {
		freemsg(mp);
		return;
	}
	icmp_unreachable(q, mp, ICMP_NET_UNREACHABLE);
}

/*
 * ip_newroute_ipif is called by ip_wput_multicast and
 * ip_rput_forward_multicast whenever we need to send
 * out a packet to a destination address for which we do not have specific
 * routing information. It is used when the packet will be sent out
 * on a specific interface.
 */
static void
ip_newroute_ipif(queue_t *q, mblk_t *mp, ipif_t *ipif, ipaddr_t dst)
{
	areq_t	*areq;
	ire_t	*ire;
	mblk_t	*res_mp;
	queue_t	*stq;
	ipaddr_t *addrp;

	ip1dbg(("ip_newroute_ipif: dst 0x%x, if %s\n", ntohl(dst),
	    ipif->ipif_ill->ill_name));
	/*
	 * If the interface is a pt-pt interface we look for an IRE_IF_RESOLVER
	 * or IRE_IF_NORESOLVER that matches both the local_address and the
	 * pt-pt destination address. Otherwise we just match the local address.
	 */
	if (!(ipif->ipif_flags & IFF_MULTICAST)) {
		ire = NULL;
		goto err_ret;
	}

	ire = ipif_to_ire(ipif);
	if (!ire)
		goto err_ret;

	ip1dbg(("ip_newroute_ipif: interface type %s (%d), address 0x%x\n",
	    ip_nv_lookup(ire_nv_tbl, ire->ire_type), ire->ire_type,
	    ntohl(ire->ire_src_addr)));
	stq = ire->ire_stq;
	switch (ire->ire_type) {
	case IRE_IF_NORESOLVER: {
		/* We have what we need to build an IRE_CACHE. */
		mblk_t	*ll_hdr_mp;
		ill_t	*ill;

		/*
		 * Create a new ll_hdr_mp with the
		 * IP gateway address in destination address in the DLPI hdr.
		 */
		ill = ire_to_ill(ire);
		if (!ill) {
			ip0dbg(("ip_newroute: ire_to_ill failed\n"));
			break;
		}
		if (ill->ill_phys_addr_length == IP_ADDR_LEN) {
			ll_hdr_mp = ill_dlur_gen((u_char *)&dst,
			    ill->ill_phys_addr_length, ill->ill_sap,
			    ill->ill_sap_length);
		} else if (ire->ire_ll_hdr_mp)
			ll_hdr_mp = dupb(ire->ire_ll_hdr_mp);
		else
			break;

		if (!ll_hdr_mp)
			break;

		ire = ire_create(
			(u_char *)&dst,			/* dest address */
			(u_char *)&ip_g_all_ones,	/* mask */
			(u_char *)&ire->ire_src_addr,	/* source address */
			nilp(u_char),			/* gateway address */
			ire->ire_max_frag,
			ll_hdr_mp,			/* xmit header */
			ire->ire_rfq,			/* recv-from queue */
			stq,				/* send-to queue */
			IRE_CACHE,
			ire->ire_rtt,
			ire->ire_rtt_sd,
			ire->ire_ll_hdr_length,
			ire->ire_ipif,
			ire,
			0);				/* flags if any */
		freeb(ll_hdr_mp);
		if (!ire)
			break;

		/* Remember the packet we want to xmit */
		ire->ire_mp->b_cont = mp;

		/*
		 * Need to become writer to add the IRE.  We continue
		 * in ire_add_then_put.
		 */
		become_exclusive(q, ire->ire_mp, (pfi_t)ire_add_then_put);
		return;
	}
	case IRE_IF_RESOLVER:
		/*
		 * We can't build an IRE_CACHE yet, but at least we found a
		 * resolver that can help.
		 */
		res_mp = ire->ire_ll_hdr_mp;
		if (!stq || !OK_RESOLVER_MP(res_mp) ||
		    !(res_mp = copyb(res_mp)))
			break;

		/* NOTE: a resolvers rfq is nil and its stq points upstream. */
		/*
		 * We obtain a partial IRE_CACHE which we will pass along with
		 * the resolver query.  When the response comes back it will be
		 * there ready for us to add.
		 */
		ire = ire_create(
			(u_char *)&dst,			/* dest address */
			(u_char *)&ip_g_all_ones,	/* mask */
			(u_char *)&ire->ire_src_addr,	/* source address */
			nilp(u_char),			/* gateway address */
			ire->ire_max_frag,
			res_mp,				/* xmit header */
			stq,				/* recv-from queue */
			OTHERQ(stq),			/* send-to queue */
			IRE_CACHE,
			ire->ire_rtt,
			ire->ire_rtt_sd,
			ire->ire_ll_hdr_length,
			ire->ire_ipif,
			ire,
			0);				/* flags if any */
		freeb(res_mp);
		if (!ire)
			break;

		/*
		 * Construct message chain for the resolver of the form:
		 * 	ARP_REQ_MBLK-->IRE_MBLK-->Packet
		 */
		ire->ire_mp->b_cont = mp;
		mp = ire->ire_ll_hdr_mp;
		ire->ire_ll_hdr_mp = nilp(mblk_t);
		linkb(mp, ire->ire_mp);

		/*
		 * Fill in the source and dest addrs for the resolver.
		 * NOTE: this depends on memory layouts imposed by ill_init().
		 */
		areq = (areq_t *)mp->b_rptr;
		addrp = (ipaddr_t *)((char *)areq +
		    areq->areq_sender_addr_offset);
		*addrp = ire->ire_src_addr;
		addrp = (ipaddr_t *)((char *)areq +
		    areq->areq_target_addr_offset);
		*addrp = dst;
		/* Up to the resolver. */
		putnext(stq, mp);
		/*
		 * The response will come back in ip_wput with db_type
		 * IRE_DB_TYPE.
		 */
		return;
	default:
		break;
	}

err_ret:
	ip1dbg(("ip_newroute_ipif: dropped\n"));
	/* Did this packet originate externally? */
	if (mp->b_prev || mp->b_next) {
		mp->b_next = nilp(mblk_t);
		mp->b_prev = nilp(mblk_t);
	} else {
		/*
		 * Since ip_wput() isn't close to finished, we fill
		 * in enough of the header for credible error reporting.
		 */
		if (ip_hdr_complete((ipha_t *)mp->b_rptr)) {
			/* Failed */
			freemsg(mp);
			return;
		}
	}
	/*
	 * TODO: more precise characterization, host or net.  We can only send
	 * a net redirect if the destination network is not subnetted.
	 *
	 * At this point we will have ire only if RTF_BLACKHOLE
	 * or RTF_REJECT flags are set on the IRE. It will not
	 * generate ICMP_HOST_UNREACHABLE if RTF_BLACKHOLE is set.
	 */
	if (ire && (ire->ire_flags & RTF_BLACKHOLE)) {
		freemsg(mp);
		return;
	}
	icmp_unreachable(q, mp, ICMP_HOST_UNREACHABLE);
}

/* Name/Value Table Lookup Routine */
char *
ip_nv_lookup(nv_t *nv, int value)
{
	if (!nv)
		return (nilp(char));
	for (; nv->nv_name; nv++) {
		if (nv->nv_value == value)
			return (nv->nv_name);
	}
	return ("unknown");
}

/* IP Module open routine. */
static int
ip_open(queue_t *q, dev_t *devp, int flag, int sflag, cred_t *credp)
{
	int	err;
	ipc_t	*ipc;
	ill_t	*ill;
	IDP	ptr;
	boolean_t	priv = false;

	TRACE_1(TR_FAC_IP, TR_IP_OPEN, "ip_open: q %p", q);

	/* Allow reopen. */
	if (q->q_ptr)
		return (0);

	/*
	 * We are either opening as a device or module.  In the device case,
	 * this is an IP client stream, and we allocate an ipc_t as the
	 * instance data.  If it is a module open, then this is a control
	 * stream for access to a DLPI device.  We allocate an ill_t as the
	 * instance data in this case.
	 *
	 * The open routine uses ill_close_flags to
	 * synchronize with ip_wsrv. These fields are only accessed
	 * by ip_open, ip_close, and ip_wsrv and the ordering of setting
	 * the flags ensures that the atomicify of 32 bit load/store
	 * is sufficient thus no locking is needed.
	 */
	if (credp && drv_priv(credp) == 0)
		priv = true;
	if (sflag & MODOPEN) {
		/*
		 * Have to defer the ill_init to ip_wsrv since it requires
		 * single-threading of all of ip.
		 * ip_rput checks for a NULL ill_ipif just in case the DLPI
		 * driver will send up data after the qprocson but before
		 * the ill has been initialized.
		 */
		if ((ill = (ill_t *)mi_open_alloc(sizeof (ill_t))) == NULL) {
			return (ENOMEM);
		}
		ptr = (IDP)ill;
		/* Initialize the new ILL. */
		q->q_ptr = WR(q)->q_ptr = ill;

		qprocson(q);

		ill->ill_error = 0;
		ill->ill_close_flags |= IPCF_OPENING;
		while (!(ill->ill_close_flags & IPCF_OPEN_DONE)) {
			qenable(WR(q));
			qwait(WR(q));
		}
		ill->ill_close_flags &= ~IPCF_OPENING;
		if ((err = ill->ill_error) != 0) {
			/* ill_init failed */
			(void) ip_close(q);
			return (err);
		}

		/* Wait for the DL_INFO_ACK */
		while (ill->ill_ipif == NULL) {
			if (!qwait_sig(ill->ill_wq)) {
				(void) ip_close(q);
				return (EINTR);
			}
		}
		if ((err = ill->ill_error) != 0) {
			/* ill_init failed */
			(void) ip_close(q);
			return (err);
		}
		if (priv)
			ill->ill_priv_stream = 1;
	} else {
		if ((ipc = (ipc_t *)mi_open_alloc(sizeof (ipc_t))) == NULL) {
			return (ENOMEM);
		}
		ptr = (IDP)ipc;
		/* Initialize the new IPC. */
		q->q_ptr = WR(q)->q_ptr = ipc;
		ipc->ipc_rq = q;
		ipc->ipc_wq = WR(q);
		/* Non-zero default values */
		ipc->ipc_multicast_ttl = IP_DEFAULT_MULTICAST_TTL;
		ipc->ipc_multicast_loop = IP_DEFAULT_MULTICAST_LOOP;
		if (priv)
			ipc->ipc_priv_stream = 1;
		mutex_init(&ipc->ipc_reflock, NULL, MUTEX_DEFAULT, NULL);
		cv_init(&ipc->ipc_refcv, NULL, CV_DEFAULT, NULL);
	}

	mutex_enter(&ip_mi_lock);
	err = mi_open_link(&ip_g_head, ptr, devp, flag, sflag, credp);
	mutex_exit(&ip_mi_lock);
	if (err) {
		if (sflag & MODOPEN) {
			(void) ip_close(q);
		} else {
			mi_close_free(ptr);
			q->q_ptr = WR(q)->q_ptr = NULL;
		}
		return (err);
	}

	/*
	 * Since D_MTOCSHARED is set the qprocson is deferred until
	 * we are ready to go for the ipc.
	 */
	if (!(sflag & MODOPEN))
		qprocson(q);
	return (0);
}

static void
ip_optcom_req(queue_t *q, mblk_t *mp)
{
	t_scalar_t optreq_prim = ((union T_primitives *)mp->b_rptr)->type;

	if (optreq_prim == T_OPTMGMT_REQ) {
		/*
		 * Note: No snmpcom_req support through new
		 * T_OPTMGMT_REQ.
		 */
		tpi_optcom_req(q, mp, IS_PRIVILEGED_QUEUE(q), &ip_opt_obj);
	} else {
		ASSERT(optreq_prim == O_T_OPTMGMT_REQ);
		if (!snmpcom_req(q, mp, ip_snmp_set, ip_snmp_get,
		    IS_PRIVILEGED_QUEUE(q)))
			svr4_optcom_req(q, mp, IS_PRIVILEGED_QUEUE(q),
			    &ip_opt_obj);
	}
}

/* This routine sets socket options. */
int
ip_opt_set(queue_t *q, u_int mgmt_flags, int level, int name, u_int inlen,
    u_char *invalp, u_int *outlenp, u_char *outvalp)
{
	int	*i1	= (int *)invalp;
	ipc_t	*ipc 	= (ipc_t *)q->q_ptr;
	int	error 	= 0;
	boolean_t priv	= IS_PRIVILEGED_QUEUE(q);
	int	checkonly;

	if (mgmt_flags == T_NEGOTIATE)
		checkonly = 0;
	else {
		ASSERT(mgmt_flags == T_CHECK);

		checkonly = 1;

		/*
		 * Note: For T_CHECK,
		 * inlen != 0 implies value supplied and
		 * 	we have to "pretend" to set it.
		 * inlen == 0 implies that there is no
		 * 	value part in T_CHECK request and udp_opt_chk()
		 * validation should be enough, we just return here.
		 */
		if (inlen == 0) {
			*outlenp = 0;
			return (0);
		}
	}

	/*
	 * For fixed length options, no sanity check
	 * of passed in length is done. It is assumed *_optcom_req()
	 * routines do the right thing.
	 */

	ASSERT(mgmt_flags == T_NEGOTIATE ||
	    (mgmt_flags == T_CHECK && inlen != 0));


	switch (level) {
	case SOL_SOCKET:
		switch (name) {
			/*
			 * XXX ipc_reflock is used to serialize the
			 * setting of socket options. The logic
			 * needs to be changed, when IP becomes MT hot.
			 */
		case SO_BROADCAST:
			if (! checkonly) {
				/* TODO: use value someplace? */
				mutex_enter(&ipc->ipc_reflock);
				ipc->ipc_broadcast = *i1 ? 1 : 0;
				mutex_exit(&ipc->ipc_reflock);
			}
			break;	/* goto sizeof (int) option return */
		case SO_USELOOPBACK:
			if (! checkonly) {
				/* TODO: use value someplace? */
				mutex_enter(&ipc->ipc_reflock);
				ipc->ipc_loopback = *i1 ? 1 : 0;
				mutex_exit(&ipc->ipc_reflock);
			}
			break;	/* goto sizeof (int) option return */
		case SO_DONTROUTE:
			if (! checkonly) {
				mutex_enter(&ipc->ipc_reflock);
				ipc->ipc_dontroute = *i1 ? 1 : 0;
				mutex_exit(&ipc->ipc_reflock);
			}
			break;	/* goto sizeof (int) option return */
		case SO_REUSEADDR:
			if (! checkonly) {
				mutex_enter(&ipc->ipc_reflock);
				ipc->ipc_reuseaddr = *i1 ? 1 : 0;
				mutex_exit(&ipc->ipc_reflock);
			}
			break;	/* goto sizeof (int) option return */
		default:
			/*
			 * "soft" error (negative)
			 * option not handled at this level
			 * Note: Do not modify *outlenp
			 */
			return (-EINVAL);
		}
		break;
	case IPPROTO_IP:
		switch (name) {
		case IP_MULTICAST_IF:
			ip2dbg(("ip_opt_set: MULTICAST IF\n"));
			if (checkonly) {
				/* T_CHECK case */
				if (ipif_lookup_addr((ipaddr_t)*i1) == NULL) {
					*outlenp = 0;
					error = EHOSTUNREACH;
					return (error);
				}
				break; /* goto sizeof (int) option return */
			}
			if (*i1 == 0) {	/* Reset */
				ipc->ipc_multicast_ipif = nilp(ipif_t);
				break; /* goto sizeof (int) option return */
			}
			ipc->ipc_multicast_ipif =
			    ipif_lookup_addr((ipaddr_t)*i1);
			if (!ipc->ipc_multicast_ipif) {
				*outlenp = 0;
				error = EHOSTUNREACH;
				return (error);
			}
			break;	/* goto sizeof (int) option return */
		case IP_MULTICAST_TTL:
			ip2dbg(("ip_opt_set: MULTICAST TTL\n"));
			if (! checkonly)
				ipc->ipc_multicast_ttl = *invalp;
			*outvalp = *invalp;
			*outlenp = sizeof (u_char);
			return (0);
		case IP_MULTICAST_LOOP:
			ip2dbg(("ip_opt_set: MULTICAST LOOP\n"));
			if (! checkonly)
				ipc->ipc_multicast_loop = *invalp ? 1 : 0;
			*outvalp = *invalp;
			*outlenp = sizeof (u_char);
			return (0);
		case IP_ADD_MEMBERSHIP: {
			struct ip_mreq *ip_mreqp = (struct ip_mreq *)i1;

			ip2dbg(("ip_opt_set: ADD MEMBER\n"));
			error = ip_opt_add_group(ipc, checkonly,
			    (ipaddr_t)ip_mreqp->imr_multiaddr.s_addr,
			    (ipaddr_t)ip_mreqp->imr_interface.s_addr);
			}
			if (error) {
				*outlenp = 0;
				return (error);
			}
			/* OK return - copy input buffer into output buffer */
			if (invalp != outvalp) {
				/* don't trust bcopy for identical src/dst */
				bcopy(invalp, outvalp, inlen);
			}
			*outlenp = inlen;
			return (0);
		case IP_DROP_MEMBERSHIP: {
			struct ip_mreq *ip_mreqp = (struct ip_mreq *)i1;

			ip2dbg(("ip_opt_set: DROP MEMBER\n"));
			error = ip_opt_delete_group(ipc, checkonly,
			    (ipaddr_t)ip_mreqp->imr_multiaddr.s_addr,
			    (ipaddr_t)ip_mreqp->imr_interface.s_addr);
			}
			if (error) {
				*outlenp = 0;
				return (error);
			}
			/* OK return - copy input buffer into output buffer */
			if (invalp != outvalp) {
				/* don't trust bcopy for identical src/dst */
				bcopy(invalp, outvalp, inlen);
			}
			*outlenp = inlen;
			return (0);
		case IP_HDRINCL:
		case IP_OPTIONS:
		case IP_TOS:
		case IP_TTL:
		case IP_RECVDSTADDR:
		case IP_RECVOPTS:
			/* OK return - copy input buffer into output buffer */
			if (invalp != outvalp) {
				/* don't trust bcopy for identical src/dst */
				bcopy(invalp, outvalp, inlen);
			}
			*outlenp = inlen;
			return (0);
		case MRT_INIT:
		case MRT_DONE:
		case MRT_ADD_VIF:
		case MRT_DEL_VIF:
		case MRT_ADD_MFC:
		case MRT_DEL_MFC:
		case MRT_ASSERT:
			if (!priv) {
				error = EPERM;
				*outlenp = 0;
				return (error);
			}
			error = ip_mrouter_set((int)name, q, checkonly,
			    (u_char *)invalp, inlen);
			if (error) {
				*outlenp = 0;
				return (error);
			}
			/* OK return - copy input buffer into output buffer */
			if (invalp != outvalp) {
				/* don't trust bcopy for identical src/dst */
				bcopy(invalp, outvalp, inlen);
			}
			*outlenp = inlen;
			return (0);
		default:
			/*
			 * "soft" error (negative)
			 * option not handled at this level
			 * Note: Do not modify *outlenp
			 */
			return (-EINVAL);
		}
		break;
	default:
		/*
		 * "soft" error (negative)
		 * option not handled at this level
		 * Note: Do not modify *outlenp
		 */
		return (-EINVAL);
	}
	/*
	 * Common case of return from an option that is sizeof (int)
	 */
	*(int *)outvalp = *i1;
	*outlenp = sizeof (int);
	return (0);
}

/*
 * This routine gets default values of certain options whose default
 * values are maintained by protocol specific code
 */

/* ARGSUSED */
int
ip_opt_default(queue_t *q, int level, int name, u_char *ptr)
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
 * This routine gets socket options.  For MRT_VERSION and MRT_ASSERT, error
 * checking of IS_PRIVILEGED(q) and that ip_g_mrouter is set should be done and
 * isn't.  This doesn't matter as the error checking is done properly for the
 * other MRT options coming in through ip_opt_set.
 */
int
ip_opt_get(queue_t *q, int level, int name, u_char *ptr)
{
	switch (level) {
	case IPPROTO_IP:
		switch (name) {
		case MRT_VERSION:
		case MRT_ASSERT:
			(void) ip_mrouter_get((int)name, (queue_t *)q,
			    (u_char *)ptr);
			return (sizeof (int));
		default:
			break;
		}

		break;
		/*
		 * Can not run this as become_exclusive due to
		 * reordering problems.
		 */
	default:
		break;
	}
	return (-1);
}

/*
 * Return 1 if there is something that requires the write lock in IP
 * Return 0 when the lock is not required. For a bad/invalid option
 * buffer also 0 is returned and the option processing routines will send
 * the appropriate error T_ERROR_ACK.
 */
static int
ip_optmgmt_writer(mblk_t *mp)
{
	u_char *optcp, *next_optcp, *opt_endcp;
	struct opthdr *opt;
	struct T_opthdr *topt;
	int opthdr_len;
	t_uscalar_t optname, optlevel;
	struct T_optmgmt_req *tor = (struct T_optmgmt_req *)mp->b_rptr;

	optcp = (u_char *) ((u_char *)mi_offset_param(mp,
	    tor->OPT_offset, tor->OPT_length));
	if (! ISALIGNED_TPIOPT(optcp))
		return (0);	/* misaligned buffer */
	opt_endcp = (u_char *)((u_char *)optcp + tor->OPT_length);
	if (tor->PRIM_type == T_OPTMGMT_REQ)
		opthdr_len = sizeof (struct T_opthdr);
	else {		/* O_OPTMGMT_REQ */
		ASSERT(tor->PRIM_type == O_T_OPTMGMT_REQ);
		opthdr_len = sizeof (struct opthdr);
	};
	for (; optcp < opt_endcp; optcp = next_optcp) {
		if (optcp + opthdr_len > opt_endcp)
			return (0); /* not enough option header */
		if (tor->PRIM_type == T_OPTMGMT_REQ) {
			topt = (struct T_opthdr *)optcp;
			optlevel = topt->level;
			optname = topt->name;
			next_optcp = optcp + ROUNDUP_TPIOPT(topt->len);
		} else {
			opt = (struct opthdr *)optcp;
			optlevel = opt->level;
			optname = opt->name;
			next_optcp = optcp + opthdr_len +
			    ROUNDUP_TPIOPT(opt->len);
		}
		if ((next_optcp < optcp) || /* wraparound pointer space */
		    ((next_optcp >= opt_endcp) && /* last option bad len */
			((next_optcp - opt_endcp) >= ALIGN_TPIOPT_size)))
			return (0); /* bad option buffer */
		switch (optlevel) {
		case SOL_SOCKET:
			switch (optname) {
			case SO_BROADCAST:
			case SO_USELOOPBACK:
			case SO_DONTROUTE:
			case SO_REUSEADDR:
			/*
			 * we return 0 here as ip_opt_set serializes
			 * the setting of these options thru ipc_reflock.
			 *
			 * XXX NOTE : When IP becomes MT hot, this needs
			 * to be re-visited.
			 */
				return (0);
			default:
				break;
			}
			break;
		case IPPROTO_IP:
			return (1);
		default:
			if ((optlevel >= MIB2_RANGE_START &&
			    optlevel <= MIB2_RANGE_END) ||
			    (optlevel >= EXPER_RANGE_START &&
				optlevel <= MIB2_RANGE_END)) {
				/* For snmpcom_req */
				return (1);
			}
			break;
		}
	}
	return (0);
}

/* Named Dispatch routine to get a current value out of our parameter table. */
/* ARGSUSED */
static int
ip_param_get(queue_t *q, mblk_t *mp, void *cp)
{
	ipparam_t *ippa = (ipparam_t *)cp;

	(void) mi_mpprintf(mp, "%d", ippa->ip_param_value);
	return (0);
}

/*
 * Walk through the param array specified registering each element with the
 * Named Dispatch handler.
 */
static boolean_t
ip_param_register(ipparam_t *ippa, size_t cnt)
{
	for (; cnt-- > 0; ippa++) {
		if (ippa->ip_param_name && ippa->ip_param_name[0]) {
			if (!nd_load(&ip_g_nd, ippa->ip_param_name,
			    ip_param_get, ip_param_set, (caddr_t)ippa)) {
				nd_free(&ip_g_nd);
				return (false);
			}
		}
	}

	if (!nd_load(&ip_g_nd, "ip_ill_status", ip_ill_report, nil(pfi_t),
	    nil(caddr_t))) {
		nd_free(&ip_g_nd);
		return (false);
	}

	if (!nd_load(&ip_g_nd, "ip_ipif_status", ip_ipif_report, nil(pfi_t),
	    nil(caddr_t))) {
		nd_free(&ip_g_nd);
		return (false);
	}

	if (!nd_load(&ip_g_nd, "ip_ire_status", ip_ire_report, nil(pfi_t),
	    nil(caddr_t))) {
		nd_free(&ip_g_nd);
		return (false);
	}

	if (!nd_load(&ip_g_nd, "ip_ipc_status", ip_ipc_report, nil(pfi_t),
	    nil(caddr_t))) {
		nd_free(&ip_g_nd);
		return (false);
	}

	if (!nd_load(&ip_g_nd, "ip_rput_pullups", nd_get_long, nd_set_long,
	    (caddr_t)&ip_rput_pullups)) {
		nd_free(&ip_g_nd);
		return (false);
	}

	if (!nd_load(&ip_g_nd, "ip_enable_group_ifs", ifgrp_get, ifgrp_set,
	    nil(caddr_t))) {
		nd_free(&ip_g_nd);
		return (false);
	}
	return (true);
}

/* Named Dispatch routine to negotiate a new value for one of our parameters. */
/* ARGSUSED */
static int
ip_param_set(queue_t *q, mblk_t *mp, char *value, void *cp)
{
	char	*end;
	int	new_value;
	ipparam_t	*ippa = (ipparam_t *)cp;

	new_value = (int)mi_strtol(value, &end, 10);
	if (end == value || new_value < ippa->ip_param_min ||
	    new_value > ippa->ip_param_max)
		return (EINVAL);
	ippa->ip_param_value = new_value;
	return (0);
}

/*
 * Hard case IP fragmentation reassembly.  If a fragment shows up out of order,
 * it gets passed in here.  When an ipf is passed here for the first time, if
 * we already have in-order fragments on the queue, we convert from the fast-
 * path reassembly scheme to the hard-case scheme.  From then on, additional
 * fragments are reassembled here.  We keep track of the start and end offsets
 * of each piece, and the number of holes in the chain.  When the hole count
 * goes to zero, we are done!
 *
 * The ipf_count will be updated to account for any mblk(s) added (pointed to
 * by mp) or subtracted (freeb()ed dups), upon return the caller must update
 * ipfb_count and ill_frag_count by the difference of ipf_count before and
 * after the call to ip_reassemble().
 */
static boolean_t
ip_reassemble(mblk_t *mp, ipf_t *ipf, uint32_t offset_and_flags,
    int stripped_hdr_len)
{
	intptr_t	end;
	mblk_t	*next_mp;
	mblk_t	*mp1;
	boolean_t	more = offset_and_flags & IPH_MF;
	intptr_t	start = (offset_and_flags & IPH_OFFSET) << 3;
	intptr_t	u1;

	if (ipf->ipf_end) {
		/*
		 * We were part way through in-order reassembly, but now there
		 * is a hole.  We walk through messages already queued, and
		 * mark them for hard case reassembly.  We know that up till
		 * now they were in order starting from offset zero.
		 */
		u1 = 0;
		for (mp1 = ipf->ipf_mp->b_cont; mp1; mp1 = mp1->b_cont) {
			IP_REASS_SET_START(mp1, u1);
			if (u1)
				u1 += mp1->b_wptr - mp1->b_rptr;
			else
				u1 = (mp1->b_wptr - mp1->b_rptr) -
					IPH_HDR_LENGTH(mp1->b_rptr);
			IP_REASS_SET_END(mp1, u1);
		}
		/* One hole at the end. */
		ipf->ipf_hole_cnt = 1;
		/* Brand it as a hard case, forever. */
		ipf->ipf_end = 0;
	}
	/* Walk through all the new pieces. */
	do {
		end = start + (mp->b_wptr - mp->b_rptr);
		if (start == 0)
			/* First segment */
			end -= IPH_HDR_LENGTH(mp->b_rptr);

		next_mp = mp->b_cont;
		if (start == end) {
			/* Empty.  Blast it. */
			IP_REASS_SET_START(mp, 0);
			IP_REASS_SET_END(mp, 0);
			/*
			 * If the ipf points to the mblk we are about to free,
			 * update ipf to point to the next mblk (or NULL
			 * if none).
			 */
			if (ipf->ipf_mp->b_cont == mp)
				ipf->ipf_mp->b_cont = next_mp;
			freeb(mp);
			continue;
		}
		/* Add in byte count */
		ipf->ipf_count += mp->b_datap->db_lim - mp->b_datap->db_base;
		mp->b_cont = nilp(mblk_t);
		IP_REASS_SET_START(mp, start);
		IP_REASS_SET_END(mp, end);
		if (!ipf->ipf_tail_mp) {
			ipf->ipf_tail_mp = mp;
			ipf->ipf_mp->b_cont = mp;
			if (start == 0 || !more) {
				ipf->ipf_hole_cnt = 1;
				/*
				 * if the first fragment comes in more than one
				 * mblk, this loop will be executed for each
				 * mblk. Need to adjust hole count so exiting
				 * this routine will leave hole count at 1.
				 */
				if (next_mp)
					ipf->ipf_hole_cnt++;
			} else
				ipf->ipf_hole_cnt = 2;
			ipf->ipf_stripped_hdr_len = stripped_hdr_len;
			continue;
		}
		/* New stuff at or beyond tail? */
		u1 = IP_REASS_END(ipf->ipf_tail_mp);
		if (start >= u1) {
			/* Link it on end. */
			ipf->ipf_tail_mp->b_cont = mp;
			ipf->ipf_tail_mp = mp;
			if (more) {
				if (start != u1)
					ipf->ipf_hole_cnt++;
			} else if (start == u1 && next_mp == NULL)
				ipf->ipf_hole_cnt--;
			continue;
		}
		mp1 = ipf->ipf_mp->b_cont;
		u1 = IP_REASS_START(mp1);
		/* New stuff at the front? */
		if (start < u1) {
			ipf->ipf_stripped_hdr_len = stripped_hdr_len;
			if (start == 0) {
				if (end >= u1) {
					/* Nailed the hole at the begining. */
					ipf->ipf_hole_cnt--;
				}
			} else if (end < u1) {
				/*
				 * A hole, stuff, and a hole where there used
				 * to be just a hole.
				 */
				ipf->ipf_hole_cnt++;
			}
			mp->b_cont = mp1;
			/* Check for overlap. */
			while (end > u1) {
				if (end < IP_REASS_END(mp1)) {
					mp->b_wptr -= end - u1;
					IP_REASS_SET_END(mp, u1);
					BUMP_MIB(ip_mib.ipReasmPartDups);
					break;
				}
				/* Did we cover another hole? */
				if ((mp1->b_cont &&
				    IP_REASS_END(mp1) !=
				    IP_REASS_START(mp1->b_cont) &&
				    end >= IP_REASS_START(mp1->b_cont)) ||
				    !more)
					ipf->ipf_hole_cnt--;
				/* Clip out mp1. */
				if ((mp->b_cont = mp1->b_cont) == NULL) {
					/*
					 * After clipping out mp1, this guy
					 * is now hanging off the end.
					 */
					ipf->ipf_tail_mp = mp;
				}
				IP_REASS_SET_START(mp1, 0);
				IP_REASS_SET_END(mp1, 0);
				/* Subtract byte count */
				ipf->ipf_count -= mp1->b_datap->db_lim -
				    mp1->b_datap->db_base;
				freeb(mp1);
				BUMP_MIB(ip_mib.ipReasmPartDups);
				mp1 = mp->b_cont;
				if (!mp1)
					break;
				u1 = IP_REASS_START(mp1);
			}
			ipf->ipf_mp->b_cont = mp;
			continue;
		}
		/*
		 * The new piece starts somewhere between the start of the head
		 * and before the end of the tail.
		 */
		for (; mp1; mp1 = mp1->b_cont) {
			u1 = IP_REASS_END(mp1);
			if (start < u1) {
				if (end <= u1) {
					/* Nothing new. */
					IP_REASS_SET_START(mp, 0);
					IP_REASS_SET_END(mp, 0);
					/* Subtract byte count */
					ipf->ipf_count -= mp->b_datap->db_lim -
					    mp->b_datap->db_base;
					freeb(mp);
					BUMP_MIB(ip_mib.ipReasmDuplicates);
					break;
				}
				/*
				 * Trim redundant stuff off beginning of new
				 * piece.
				 */
				IP_REASS_SET_START(mp, u1);
				mp->b_rptr += u1 - start;
				BUMP_MIB(ip_mib.ipReasmPartDups);
				start = u1;
				if (!mp1->b_cont) {
					/*
					 * After trimming, this guy is now
					 * hanging off the end.
					 */
					mp1->b_cont = mp;
					ipf->ipf_tail_mp = mp;
					if (!more)
						ipf->ipf_hole_cnt--;
					break;
				}
			}
			if (start >= IP_REASS_START(mp1->b_cont))
				continue;
			/* Fill a hole */
			if (start > u1)
				ipf->ipf_hole_cnt++;
			mp->b_cont = mp1->b_cont;
			mp1->b_cont = mp;
			mp1 = mp->b_cont;
			u1 = IP_REASS_START(mp1);
			if (end >= u1) {
				ipf->ipf_hole_cnt--;
				/* Check for overlap. */
				while (end > u1) {
					if (end < IP_REASS_END(mp1)) {
						mp->b_wptr -= end - u1;
						IP_REASS_SET_END(mp, u1);
						/*
						 * TODO we might bump
						 * this up twice if there is
						 * overlap at both ends.
						 */
						BUMP_MIB(\
						    ip_mib.ipReasmPartDups);
						break;
					}
					/* Did we cover another hole? */
					if ((mp1->b_cont &&
					    IP_REASS_END(mp1)
					    != IP_REASS_START(mp1->b_cont) &&
					    end >=
					    IP_REASS_START(mp1->b_cont)) ||
					    !more)
						ipf->ipf_hole_cnt--;
					/* Clip out mp1. */
					if ((mp->b_cont = mp1->b_cont) ==
					    NULL) {
						/*
						 * After clipping out mp1,
						 * this guy is now hanging
						 * off the end.
						 */
						ipf->ipf_tail_mp = mp;
					}
					IP_REASS_SET_START(mp1, 0);
					IP_REASS_SET_END(mp1, 0);
					/* Subtract byte count */
					ipf->ipf_count -=
					    mp1->b_datap->db_lim -
					    mp1->b_datap->db_base;
					freeb(mp1);
					BUMP_MIB(ip_mib.ipReasmPartDups);
					mp1 = mp->b_cont;
					if (!mp1)
						break;
					u1 = IP_REASS_START(mp1);
				}
			}
			break;
		}
	} while (start = end, mp = next_mp);
	/* Still got holes? */
	if (ipf->ipf_hole_cnt)
		return (false);
	/* Clean up overloaded fields to avoid upstream disasters. */
	for (mp1 = ipf->ipf_mp->b_cont; mp1; mp1 = mp1->b_cont) {
		IP_REASS_SET_START(mp1, 0);
		IP_REASS_SET_END(mp1, 0);
	}
	return (true);
}

/* Read side put procedure.  Packets coming from the wire arrive here. */
void
ip_rput(queue_t *q, mblk_t *mp)
{
	ipaddr_t	dst;
	ire_t	*ire;
	mblk_t	*mp1;
	ipha_t	*ipha;
	ill_t	*ill;
	u_int	pkt_len;
	ssize_t	len;
	u_int	u1;
	int	ll_multicast;

	TRACE_1(TR_FAC_IP, TR_IP_RPUT_START, "ip_rput_start: q %p", q);

#define	rptr	((u_char *)ipha)

	ill = (ill_t *)q->q_ptr;
	if (ill->ill_ipif == NULL) {
		/*
		 * Things are opening or closing - only accept DLPI
		 * ack messages
		 */
		if (mp->b_datap->db_type != M_PCPROTO) {
			freemsg(mp);
			TRACE_2(TR_FAC_IP, TR_IP_RPUT_END,
			    "ip_rput_end: q %p (%S)", q, "uninit");
			return;
		}
	}

	/*
	 * ip_rput fast path
	 */

	/*
	 * if db_ref > 1 then copymsg and free original. Packet may be
	 * changed and do not want other entity who has a reference to this
	 * message to trip over the changes. This is a blind change because
	 * trying to catch all places that might change packet is too
	 * difficult (since it may be a module above this one).
	 */
	if (mp->b_datap->db_ref > 1) {
		mblk_t  *mp1;

		mp1 = copymsg(mp);
		freemsg(mp);
		if (mp1 == NULL) {
			BUMP_MIB(ip_mib.ipInDiscards);
			TRACE_2(TR_FAC_IP, TR_IP_RPUT_END,
			    "ip_rput_end: q %p (%S)", q, "copymsg");
			return;
		}
		mp = mp1;
	}

	ipha = (ipha_t *)mp->b_rptr;
	/* mblk type is not M_DATA */
	if (mp->b_datap->db_type != M_DATA) goto notdata;

	ll_multicast = 0;
	BUMP_MIB(ip_mib.ipInReceives);
	len = mp->b_wptr - rptr;

	/* IP header ptr not aligned? */
	if (!OK_32PTR(rptr)) goto notaligned;

	/* IP header not complete in first mblock */
	if (len < IP_SIMPLE_HDR_LENGTH) goto notaligned;

	/* multiple mblk or too short */
	if (len - ntohs(ipha->ipha_length)) goto multimblk;

	/* IP version bad or there are IP options */
	if (ipha->ipha_version_and_hdr_length - (u_char) IP_SIMPLE_HDR_VERSION)
		goto ipoptions;

	dst = ipha->ipha_dst;

	/*
	 * If rsvpd is running, let RSVP daemon handle its processing
	 * and forwarding of RSVP multicast/unicast packets.
	 * If rsvpd is not running but mrouted is running, RSVP
	 * multicast packets are forwarded as multicast traffic
	 * and RSVP unicast packets are forwarded by unicast router.
	 * If neither rsvpd nor mrouted is running, RSVP multicast
	 * packets are not forwarded, but the unicast packets are
	 * forwarded like unicast traffic.
	 */
	if (ipha->ipha_protocol == IPPROTO_RSVP &&
	    ipc_proto_fanout[IPPROTO_RSVP].icf_ipc != NULL) {
		/* RSVP packet and rsvpd running. Treat as ours */
		goto ours;
	}

	/* packet is multicast */
	if (CLASSD(dst)) goto multicast;

	ire = ire_cache_lookup(dst);

	if (!ire)
	    goto noirefound;

	/* broadcast? */
	if (ire->ire_type == IRE_BROADCAST) goto broadcast;

	/* fowarding? */
	if (ire->ire_stq != 0) goto forward;

	/* packet not for us */
	if (ire->ire_rfq != q) goto notforus;

	TRACE_2(TR_FAC_IP, TR_IP_RPUT_END,
	    "ip_rput_end: q %p (%S)", q, "end");

	ip_rput_local(q, mp, ipha, ire);
	return;

notdata:
	switch (mp->b_datap->db_type) {
	case M_DATA:
		/*
		 * A fastpath device may send us M_DATA.  Fastpath messages
		 * start with the network header and are never used for packets
		 * that were broadcast or multicast by the link layer.
		 *
		 * M_DATA are also used to pass back decapsulated packets
		 * from ip_mroute_decap. For those packets we will force
		 * ll_multicast to one below.
		 */
		ll_multicast = 0;
		break;
	case M_PROTO:
	case M_PCPROTO:
		if (((dl_unitdata_ind_t *)rptr)->dl_primitive !=
			DL_UNITDATA_IND) {
			/* Go handle anything other than data elsewhere. */
			ip_rput_dlpi(q, mp);
			TRACE_2(TR_FAC_IP, TR_IP_RPUT_END,
			    "ip_rput_end: q %p (%S)", q, "proto");
			return;
		}
		ll_multicast = ((dl_unitdata_ind_t *)rptr)->dl_group_address;
		/* Ditch the DLPI header. */
		mp1 = mp;
		mp = mp->b_cont;
		freeb(mp1);
		ipha = (ipha_t *)mp->b_rptr;
		break;
	case M_BREAK:
		/*
		 * A packet arrives as M_BREAK following a cycle through
		 * ip_rput, ip_newroute, ... and finally ire_add_then_put.
		 * This is an IP datagram sans lower level header.
		 * M_BREAK are also used to pass back in multicast packets
		 * that are encapsulated with a source route.
		 */
		/* Ditch the M_BREAK mblk */
		mp1 = mp->b_cont;
		freeb(mp);
		mp = mp1;
		ipha = (ipha_t *)mp->b_rptr;
		/* Get the number of words of IP options in the IP header. */
		u1 = ipha->ipha_version_and_hdr_length - IP_SIMPLE_HDR_VERSION;
		dst = (ipaddr_t)mp->b_next;
		mp->b_next = nilp(mblk_t);
		ll_multicast = 0;
		goto hdrs_ok;
	case M_IOCACK:
		if (((struct iocblk *)mp->b_rptr)->ioc_cmd != DL_IOC_HDR_INFO) {
			putnext(q, mp);
			TRACE_2(TR_FAC_IP, TR_IP_RPUT_END,
			    "ip_rput_end: q %p (%S)", q, "iocack");
			return;
		}
		/* FALLTHRU */
	case M_ERROR:
	case M_HANGUP:
		become_exclusive(q, mp, ip_rput_other);
		TRACE_2(TR_FAC_IP, TR_IP_RPUT_END,
		    "ip_rput_end: q %p (%S)", q, "ip_rput_other");
		return;
	case M_CTL: {
		inetcksum_t *ick = (inetcksum_t *)mp->b_rptr;

		if ((mp->b_wptr - mp->b_rptr) == sizeof (*ick) &&
		    ick->ick_magic == ICK_M_CTL_MAGIC) {
			ill = (ill_t *)q->q_ptr;
			ill->ill_ick = *ick;
			freemsg(mp);
			return;
		} else {
			putnext(q, mp);
			TRACE_2(TR_FAC_IP, TR_IP_RPUT_END,
			    "ip_rput_end: q %p (%S)", q, "default");
			return;
		}
	}
	case M_IOCNAK:
		if (((struct iocblk *)mp->b_rptr)->ioc_cmd == DL_IOC_HDR_INFO) {
			freemsg(mp);
			TRACE_2(TR_FAC_IP, TR_IP_RPUT_END,
			    "ip_rput_end: q %p (%S)", q, "iocnak - hdr_info");
			return;
		}
		/* FALLTHRU */
	default:
		putnext(q, mp);
		TRACE_2(TR_FAC_IP, TR_IP_RPUT_END,
		    "ip_rput_end: q %p (%S)", q, "default");
		return;
	}
	/*
	 * Make sure that we at least have a simple IP header accessible and
	 * that we have at least long-word alignment.  If not, try a pullup.
	 */
	BUMP_MIB(ip_mib.ipInReceives);
	len = mp->b_wptr - rptr;
	if (!OK_32PTR(rptr) || len < IP_SIMPLE_HDR_LENGTH) {
notaligned:
		/* Guard against bogus device drivers */
		if (len < 0) {
			/* clear b_prev - used by ip_mroute_decap */
			mp->b_prev = NULL;
			goto in_hdr_errors;
		}
		if (ip_rput_pullups++ == 0) {
			ill = (ill_t *)q->q_ptr;
			(void) mi_strlog(q, 1, SL_ERROR|SL_TRACE,
			    "ip_rput: %s forced us to pullup pkt,"
			    " hdr len %ld, hdr addr %p",
			    ill->ill_name, len, ipha);
		}
		if (!pullupmsg(mp, IP_SIMPLE_HDR_LENGTH)) {
			/* clear b_prev - used by ip_mroute_decap */
			mp->b_prev = NULL;
			BUMP_MIB(ip_mib.ipInDiscards);
			goto drop_pkt;
		}
		ipha = (ipha_t *)mp->b_rptr;
		len = mp->b_wptr - rptr;
	}
multimblk:
ipoptions:
	pkt_len = ntohs(ipha->ipha_length);
	len -= pkt_len;
	if (len) {
		/*
		 * Make sure we have data length consistent with the IP header.
		 */
		if (!mp->b_cont) {
			if (len < 0 || pkt_len < IP_SIMPLE_HDR_LENGTH)
				goto in_hdr_errors;
			mp->b_wptr = rptr + pkt_len;
		} else if (len += msgdsize(mp->b_cont)) {
			if (len < 0 || pkt_len < IP_SIMPLE_HDR_LENGTH)
				goto in_hdr_errors;
			(void) adjmsg(mp, -len);
		}
	}
	/* Get the number of words of IP options in the IP header. */
	u1 = ipha->ipha_version_and_hdr_length - (u_char)IP_SIMPLE_HDR_VERSION;
	if (u1) {
		/* IP Options present!  Validate and process. */
		if (u1 > (15 - IP_SIMPLE_HDR_LENGTH_IN_WORDS)) {
			/* clear b_prev - used by ip_mroute_decap */
			mp->b_prev = NULL;
			goto in_hdr_errors;
		}
		/*
		 * Recompute complete header length and make sure we
		 * have access to all of it.
		 */
		len = ((size_t)u1 + IP_SIMPLE_HDR_LENGTH_IN_WORDS) << 2;
		if (len > (mp->b_wptr - rptr)) {
			if (len > pkt_len) {
				/* clear b_prev - used by ip_mroute_decap */
				mp->b_prev = NULL;
				goto in_hdr_errors;
			}
			if (!pullupmsg(mp, len)) {
				BUMP_MIB(ip_mib.ipInDiscards);
				/* clear b_prev - used by ip_mroute_decap */
				mp->b_prev = NULL;
				goto drop_pkt;
			}
			ipha = (ipha_t *)mp->b_rptr;
		}
		/*
		 * Go off to ip_rput_options which returns the next hop
		 * destination address, which may have been affected
		 * by source routing.
		 */
		dst = ip_rput_options(q, mp, ipha);
		if (dst == INADDR_BROADCAST) {
			TRACE_2(TR_FAC_IP, TR_IP_RPUT_END,
			    "ip_rput_end: q %p (%S)", q, "baddst");
			return;
		}
	} else {
		/* No options.  We know we have alignment so grap the dst. */
		dst = ipha->ipha_dst;
	}
hdrs_ok:;
	/*
	* If rsvpd is running, let RSVP daemon handle its processing
	* and forwarding of RSVP multicast/unicast packets.
	* If rsvpd is not running but mrouted is running, RSVP
	* multicast packets are forwarded as multicast traffic
	* and RSVP unicast packets are forwarded by unicast router.
	* If neither rsvpd nor mrouted is running, RSVP multicast
	* packets are not forwarded, but the unicast packets are
	* forwarded like unicast traffic.
	*/
	if (ipha->ipha_protocol == IPPROTO_RSVP &&
	    ipc_proto_fanout[IPPROTO_RSVP].icf_ipc != NULL) {
		/* RSVP packet and rsvpd running. Treat as ours */
		goto ours;
	}
	if (CLASSD(dst)) {
multicast:
		ill = (ill_t *)q->q_ptr;
		if (ip_g_mrouter) {
			int retval;

			retval = ip_mforward(ill, ipha, mp);
			/* ip_mforward updates mib variables if needed */
			/* clear b_prev - used by ip_mroute_decap */
			mp->b_prev = NULL;

			switch (retval) {
			case 0:
				/* pkt is okay and arrived on phyint */
				/*
				 * If we are running a multicast router
				 * we need to see all igmp packets.
				 */
				if (ipha->ipha_protocol == IPPROTO_IGMP)
					goto ours;
				break;
			case -1:
				/* pkt is mal-formed, toss it */
				goto drop_pkt;
			case 1:
				/* pkt is okay and arrived on a tunnel */
				/*
				 * If we are running a multicast router
				 *  we need to see all igmp packets.
				 */
				if (ipha->ipha_protocol == IPPROTO_IGMP)
					goto ours;

				goto drop_pkt;
			}
		}

		/*
		 * This could use ilm_lookup_exact but this doesn't really
		 * buy anything and it is more expensive since we would
		 * have to determine the ipif.
		 */
		if (!ilm_lookup(ill, dst)) {
			/*
			 * This might just be caused by the fact that
			 * multiple IP Multicast addresses map to the same
			 * link layer multicast - no need to increment counter!
			 */
			freemsg(mp);
			TRACE_2(TR_FAC_IP, TR_IP_RPUT_END,
			    "ip_rput_end: q %p (%S)", q, "badilm");
			return;
		}
	ours:
		ip2dbg(("ip_rput: multicast for us: 0x%x\n", ntohl(dst)));
		/*
		 * This assume the we deliver to all streams for multicast
		 * and broadcast packets.
		 * We have to force ll_multicast to 1 to handle the
		 * M_DATA messages passed in from ip_mroute_decap.
		 */
		dst = INADDR_BROADCAST;
		ll_multicast = 1;
	}

	ire = ire_cache_lookup(dst);
	if (!ire) {
noirefound:
		/*
		 * No IRE for this destination, so it can't be for us.
		 * Unless we are forwarding, drop the packet.
		 * We have to let source routed packets through
		 * since we don't yet know if they are 'ping -l'
		 * packets i.e. if they will go out over the
		 * same interface as they came in on.
		 */
		if (ll_multicast)
			goto drop_pkt;
		if (!WE_ARE_FORWARDING &&
		    !ip_source_routed(ipha)) {
			BUMP_MIB(ip_mib.ipForwProhibits);
			goto drop_pkt;
		}

		/* Mark this packet as having originated externally */
		mp->b_prev = (mblk_t *)q;

		/*
		 * Remember the dst, we may have gotten it from
		 * ip_rput_options.
		 */
		mp->b_next = (mblk_t *)dst;

		/*
		 * Now hand the packet to ip_newroute.
		 * It may come back.
		 */
		ip_newroute(q, mp, dst);
		TRACE_2(TR_FAC_IP, TR_IP_RPUT_END,
		    "ip_rput_end: q %p (%S)", q, "no ire");
		return;
	}
	/*
	 * We now have a final IRE for the destination address.  If ire_stq
	 * is non-nil, (and it is not a broadcast IRE), then this packet
	 * needs to be forwarded, if allowable.
	 */
	/*
	 * Directed broadcast forwarding: if the packet came in over a
	 * different interface then it is routed out over we can forward it.
	 */
	if (ire->ire_type == IRE_BROADCAST) {
broadcast:
		if (ip_g_forward_directed_bcast && !ll_multicast) {
			/*
			 * Verify that there are not more then one
			 * IRE_BROADCAST with this broadcast address which
			 * has ire_stq set.
			 * TODO: simplify, loop over all IRE's
			 */
			ire_t	*ire1;
			int	num_stq = 0;
			mblk_t	*mp1;

			/* Find the first one with ire_stq set */
			for (ire1 = ire; ire1 &&
			    !ire1->ire_stq && ire1->ire_addr == ire->ire_addr;
			    ire1 = ire1->ire_next)
				;
			if (ire1)
				ire = ire1;

			/* Check if there are additional ones with stq set */
			for (ire1 = ire; ire1; ire1 = ire1->ire_next) {
				if (ire->ire_addr != ire1->ire_addr)
					break;
				if (ire1->ire_stq) {
					num_stq++;
					break;
				}
			}
			if (num_stq == 1 && ire->ire_stq) {
				ip1dbg(("ip_rput: directed broadcast to 0x%x\n",
				    ntohl(ire->ire_addr)));
				mp1 = copymsg(mp);
				if (mp1)
					ip_rput_local(q, mp1,
					    (ipha_t *)mp1->b_rptr,
					    ire);
				/*
				 * Adjust ttl to 2 (1+1 - the forward engine
				 * will decrement it by one.
				 */
				if (ip_csum_hdr(ipha)) {
					BUMP_MIB(ip_mib.ipInCksumErrs);
					goto drop_pkt;
				}
				ipha->ipha_ttl = (uint8_t)ip_broadcast_ttl + 1;
				ipha->ipha_hdr_checksum = 0;
				ipha->ipha_hdr_checksum =
				    (uint16_t)ip_csum_hdr(ipha);
				goto forward;
			}
			ip1dbg(("ip_rput: NO directed broadcast to 0x%x\n",
			    ntohl(ire->ire_addr)));
		}
	} else if (ire->ire_stq) {
		queue_t	*dev_q;

		if (ll_multicast)
			goto drop_pkt;
	forward:
		/*
		 * Check if we want to forward this one at this time.
		 * We allow source routed packets on a host provided that
		 * the go out the same interface as they came in on.
		 */
		if (!IP_OK_TO_FORWARD_THRU(ipha, ire) &&
		    !(ip_source_routed(ipha) && ire->ire_rfq == q)) {
			BUMP_MIB(ip_mib.ipForwProhibits);
			if (ip_source_routed(ipha)) {
				q = WR(q);
				icmp_unreachable(q, mp,
				    ICMP_SOURCE_ROUTE_FAILED);
				TRACE_2(TR_FAC_IP, TR_IP_RPUT_END,
				    "ip_rput_end: q %p (%S)", q,
				    "route failed");
				return;
			}
			goto drop_pkt;
		}
		dev_q = ire->ire_stq->q_next;
		if ((dev_q->q_next || dev_q->q_first) && !canput(dev_q)) {
			BUMP_MIB(ip_mib.ipInDiscards);
			q = WR(q);
			icmp_source_quench(q, mp);
			TRACE_2(TR_FAC_IP, TR_IP_RPUT_END,
			    "ip_rput_end: q %p (%S)", q, "srcquench");
			return;
		}
		if (ire->ire_rfq == q && ip_g_send_redirects) {
			/*
			 * It wants to go out the same way it came in.
			 * Check the source address to see if it originated
			 * on the same logical subnet it is going back out on.
			 * If so, we should be able to send it a redirect.
			 * Avoid sending a redirect if the destination
			 * is directly connected (gw_addr == 0),
			 * or if the packet was source routed out this
			 * interface.
			 */
			ipaddr_t src;
			mblk_t	*mp1;

			src = ipha->ipha_src;
			if ((ire->ire_gateway_addr != 0) &&
			    (ire_ftable_lookup(src, 0, 0, IRE_INTERFACE,
				ire->ire_ipif, NULL, NULL,
				MATCH_IRE_IPIF | MATCH_IRE_TYPE)) &&
			    !ip_source_routed(ipha)) {
				/*
				 * The source is directly connected.  Just copy
				 * the ip header (which is in the first mblk)
				 */
				mp1 = copyb(mp);
				if (mp1)
					icmp_send_redirect(WR(q), mp1,
					    ire->ire_gateway_addr);
			}
		}

		TRACE_2(TR_FAC_IP, TR_IP_RPUT_END,
		    "ip_rput_end: q %p (%S)", q, "forward");
		ip_rput_forward(ire, ipha, mp);
		return;
	}
	/*
	 * It's for us.  We need to check to make sure the packet came in
	 * on the queue associated with the destination IRE.
	 * Note that for multicast packets and broadcast packets sent to
	 * a broadcast address which is shared between multiple interfaces
	 * we should not do this since we just got a random broadcast ire.
	 */
	if (ire->ire_rfq != q) {
notforus:
		if (ire->ire_rfq && ire->ire_type != IRE_BROADCAST) {
			/*
			 * This packet came in on an interface other than the
			 * one associated with the destination address.
			 * "Gateway" it to the appropriate interface here.
			 */
			if (ip_strict_dst_multihoming && !WE_ARE_FORWARDING) {
				/* Drop packet */
				BUMP_MIB(ip_mib.ipForwProhibits);
				freemsg(mp);
				TRACE_2(TR_FAC_IP, TR_IP_RPUT_END,
				    "ip_rput_end: q %p (%S)", q,
				    "strict_dst_multihoming");
				return;
			}

			/*
			 * Change the queue and ip_rput_local will be
			 * called with the right queue. We can do this
			 * because IP is D_MTPERMOD and close is exclusive
			 * for module instances pushed above the driver.
			 * Hence the ire_rfq->q_stream can't disappear while
			 * we are in q->q_stream.
			 */
			q = ire->ire_rfq;
		}
		/* Must be broadcast.  We'll take it. */
	}

	TRACE_2(TR_FAC_IP, TR_IP_RPUT_END,
	    "ip_rput_end: q %p (%S)", q, "end");
	ip_rput_local(q, mp, ipha, ire);
	return;

in_hdr_errors:;
	BUMP_MIB(ip_mib.ipInHdrErrors);
	/* FALLTHRU */
drop_pkt:;
	ip2dbg(("ip_rput: drop pkt\n"));
	freemsg(mp);
	TRACE_2(TR_FAC_IP, TR_IP_RPUT_END,
	    "ip_rput_end: q %p (%S)", q, "drop");
#undef	rptr
}

static void
ip_rput_unbind_done(ill_t *ill)
{
	mblk_t	*mp;

	ill->ill_unbind_pending = 0;
	if ((mp = ill->ill_detach_mp) != NULL) {
		ip1dbg(("ip_rput_unbind_done: detach\n"));
		ill->ill_detach_mp = mp->b_next;
		mp->b_next = nilp(mblk_t);
		putnext(ill->ill_wq, mp);
	}
	if ((mp = ill->ill_attach_mp) != NULL) {
		ip1dbg(("ip_rput_unbind_done: attach\n"));
		ill->ill_attach_mp = nilp(mblk_t);
		putnext(ill->ill_wq, mp);
	}
	if ((mp = ill->ill_bind_mp) != NULL) {
		ip1dbg(("ip_rput_unbind_done: bind\n"));
		ill->ill_bind_mp = nilp(mblk_t);
		putnext(ill->ill_wq, mp);
	}
}

/*
 * ip_rput_dlpi is called by ip_rput to handle all DLPI messages other
 * than DL_UNITDATA_IND messages.
 */
static void
ip_rput_dlpi(queue_t *q, mblk_t *mp)
{
	dl_ok_ack_t	*dloa = (dl_ok_ack_t *)mp->b_rptr;
	dl_error_ack_t	*dlea = (dl_error_ack_t *)dloa;
	char		*err_str = nilp(char);
	ill_t		*ill;

	ill = (ill_t *)q->q_ptr;
	switch (dloa->dl_primitive) {
	case DL_ERROR_ACK:
		ip0dbg(("ip_rput: DL_ERROR_ACK for %d, errno %d, unix %d\n",
		    (int)dlea->dl_error_primitive,
		    (int)dlea->dl_errno,
		    (int)dlea->dl_unix_errno));
		switch (dlea->dl_error_primitive) {
		case DL_UNBIND_REQ:
		case DL_BIND_REQ:
		case DL_ENABMULTI_REQ:
			become_exclusive(q, mp, ip_rput_dlpi_writer);
			return;
		case DL_DETACH_REQ:
			err_str = "DL_DETACH";
			break;
		case DL_ATTACH_REQ:
			ip0dbg(("ip_rput: attach failed on %s\n",
			    ill->ill_name));
			err_str = "DL_ATTACH";
			break;
		case DL_DISABMULTI_REQ:
			ip1dbg(("DL_ERROR_ACK to disabmulti\n"));
			freemsg(mp);	/* Don't want to pass this up */
			return;
		case DL_PROMISCON_REQ:
			ip1dbg(("DL_ERROR_ACK to promiscon\n"));
			err_str = "DL_PROMISCON_REQ";
			break;
		case DL_PROMISCOFF_REQ:
			ip1dbg(("DL_ERROR_ACK to promiscoff\n"));
			err_str = "DL_PROMISCOFF_REQ";
			break;
		default:
			err_str = "DL_???";
			break;
		}
		/*
		 * There is no IOCTL hanging out on these.  Log the
		 * problem and hope someone notices.
		 */
		(void) mi_strlog(q, 1, SL_ERROR|SL_TRACE,
		    "ip_rput_dlpi: %s failed dl_errno %d dl_unix_errno %d",
		    err_str, dlea->dl_errno, dlea->dl_unix_errno);
		freemsg(mp);
		return;
	case DL_INFO_ACK:
	case DL_BIND_ACK:
		become_exclusive(q, mp, ip_rput_dlpi_writer);
		return;
	case DL_OK_ACK:
#ifdef IP_DEBUG
		ip1dbg(("ip_rput: DL_OK_ACK for %d\n",
		    (int)dloa->dl_correct_primitive));
		switch (dloa->dl_correct_primitive) {
		case DL_UNBIND_REQ:
			ip1dbg(("ip_rput: UNBIND OK\n"));
			break;
		case DL_ENABMULTI_REQ:
		case DL_DISABMULTI_REQ:
			ip1dbg(("DL_OK_ACK to add/delmulti\n"));
			break;
		case DL_PROMISCON_REQ:
		case DL_PROMISCOFF_REQ:
			ip1dbg(("DL_OK_ACK to promiscon/off\n"));
			break;
		}
#endif	/* IP_DEBUG */
		if (dloa->dl_correct_primitive == DL_UNBIND_REQ) {
			become_exclusive(q, mp, ip_rput_dlpi_writer);
			return;
		}
		break;
	default:
		break;
	}
	freemsg(mp);
}

/*
 * Handling of DLPI messages that require exclusive access to IP
 */
static void
ip_rput_dlpi_writer(queue_t *q, mblk_t	*mp)
{
	dl_ok_ack_t	*dloa = (dl_ok_ack_t *)mp->b_rptr;
	dl_error_ack_t	*dlea = (dl_error_ack_t *)dloa;
	int		err = 0;
	char		*err_str = nilp(char);
	ill_t		*ill;
	ipif_t		*ipif;
	mblk_t		*mp1 = nilp(mblk_t);

	ill = (ill_t *)q->q_ptr;
	ipif = ill->ill_ipif_pending;
	switch (dloa->dl_primitive) {
	case DL_ERROR_ACK:
		switch (dlea->dl_error_primitive) {
		case DL_UNBIND_REQ:
			ip_rput_unbind_done(ill);
			err_str = "DL_UNBIND";
			break;
		case DL_BIND_REQ:
			/*
			 * Something went wrong with the bind.  We presumably
			 * have an IOCTL hanging out waiting for completion.
			 * Find it, take down the interface that was coming
			 * up, and complete the IOCTL with the error noted.
			 */
			mp1 = ill->ill_bind_pending;
			if (mp1) {
				ill->ill_ipif_pending = nilp(ipif_t);
				ipif_down(ipif);
				ill->ill_bind_pending = nilp(mblk_t);
				q = ill->ill_bind_pending_q;
				/*
				 * If the client stream is no longer around,
				 * then do not try and send a reply.
				 */
				if (q == nilp(queue_t)) {
					mblk_t	*bp = mp1;

					for (; bp; bp = bp->b_cont) {
						bp->b_prev = nil(MBLKP);
						bp->b_next = nil(MBLKP);
					}
					freemsg(mp1);
					return;
				}
				/*
				 * Clear fields here. Since we are holding
				 * the writers lock we can do it here even
				 * though we do the qreply below.
				 */
				ASSERT(((ipc_t *)q->q_ptr)->ipc_bind_ill
				    == ill);
				ill->ill_bind_pending_q = nilp(queue_t);
				((ipc_t *)q->q_ptr)->ipc_bind_ill = nilp(ill_t);
			}
			break;
		case DL_ENABMULTI_REQ:
			ip1dbg(("DL_ERROR_ACK to enabmulti\n"));
			freemsg(mp);	/* Don't want to pass this up */
			if (dlea->dl_errno != DL_SYSERR) {
				ipif_t *ipif;

				printf("ip: joining multicasts failed"
				    " on %s - will use link layer\n",
				    ill->ill_name);
				printf("broadcasts for multicast\n");
				for (ipif = ill->ill_ipif;
				    ipif; ipif = ipif->ipif_next) {
					ipif->ipif_flags |= IFF_MULTI_BCAST;
					(void) ipif_arp_up(ipif,
					    ipif->ipif_local_addr);
				}
			}
			return;
		}
		if (err_str) {
			/*
			 * There is no IOCTL hanging out on these.  Log the
			 * problem and hope someone notices.
			 */
			(void) mi_strlog(q, 1, SL_ERROR|SL_TRACE,
			"ip_rput_dlpi: %s failed dl_errno %d dl_unix_errno %d",
			    err_str, dlea->dl_errno, dlea->dl_unix_errno);
			freemsg(mp);
			return;
		}
		/* Note the error for IOCTL completion. */
		err = dlea->dl_unix_errno ? dlea->dl_unix_errno : ENXIO;
		break;
	case DL_INFO_ACK:
		/* Call a routine to handle this one. */
		ip_ll_subnet_defaults(ill, mp);
		return;
	case DL_BIND_ACK:
		/* We should have an IOCTL waiting on this. */
		mp1 = ill->ill_bind_pending;
		if (mp1) {
			ipif_t	*ipif;

			ill->ill_bind_pending = nilp(mblk_t);
			/* Retrieve the originating queue pointer. */
			q = ill->ill_bind_pending_q;
			/* Complete initialization of the interface. */
			ill->ill_ipif_up_count++;
			ipif = ill->ill_ipif_pending;
			ill->ill_ipif_pending = nilp(ipif_t);
			ipif->ipif_flags |= IFF_UP;
			/* Increment the global interface up count. */
			ipif_g_count++;
			if (ill->ill_bcast_mp)
				ill_fastpath_probe(ill, ill->ill_bcast_mp);
			/*
			 * Need to add all multicast addresses. This
			 * had to be deferred until we had attached.
			 */
			ill_add_multicast(ill);

			/* This had to be deferred until we had bound. */
			for (ipif = ill->ill_ipif; ipif; ipif =
			    ipif->ipif_next) {
				/*
				 * tell routing sockets that
				 * this interface is up
				 */
				ip_rts_ifmsg(ipif);
				ip_rts_newaddrmsg(RTM_ADD, 0, ipif);
				/* Broadcast an address mask reply. */
				ipif_mask_reply(ipif);
			    }
			/*
			 * If the client stream is no longer around,
			 * then do not try and send a reply.
			 */
			if (q == nilp(queue_t)) {
				mblk_t	*bp = mp1;

				for (; bp; bp = bp->b_cont) {
					bp->b_prev = nil(MBLKP);
					bp->b_next = nil(MBLKP);
				}
				freemsg(mp1);
				return;
			}
			/*
			 * Clear fields here. Since we are holding
			 * the writers lock we can do it here even
			 * though we do the qreply below.
			 */
			ASSERT(((ipc_t *)q->q_ptr)->ipc_bind_ill == ill);
			ill->ill_bind_pending_q = nilp(queue_t);
			((ipc_t *)q->q_ptr)->ipc_bind_ill = nilp(ill_t);
		}
		break;
	case DL_OK_ACK:
		if (dloa->dl_correct_primitive == DL_UNBIND_REQ)
			ip_rput_unbind_done(ill);
		break;
	default:
		break;
	}
	freemsg(mp);
	if (mp1) {
		/* Complete the waiting IOCTL. */
		mi_copy_done(q, mp1, err);
	}
}

/*
 * ip_rput_other is called by ip_rput to handle messages modifying the global
 * state in IP.  Always called as a writer.
 */
static void
ip_rput_other(queue_t *q, mblk_t *mp)
{
	ill_t	*ill;

	switch (mp->b_datap->db_type) {
	case M_ERROR:
	case M_HANGUP:
		/*
		 * The device has a problem.  We force the ILL down.  It can
		 * be brought up again manually using SIOCSIFFLAGS (via
		 * ifconfig or equivalent).
		 */
		ill = (ill_t *)q->q_ptr;
		if (mp->b_rptr < mp->b_wptr)
			ill->ill_error = (int)(*mp->b_rptr & 0xFF);
		if (ill->ill_error == 0)
			ill->ill_error = ENXIO;
		ill_down(ill);
		freemsg(mp);
		return;
	case M_IOCACK:
		if (((struct iocblk *)mp->b_rptr)->ioc_cmd == DL_IOC_HDR_INFO) {
			ill = (ill_t *)q->q_ptr;
			ill_fastpath_ack(ill, mp);
			return;
		}
		/* FALLTHRU */
	}
}

/* (May be called as writer.) */
void
ip_rput_forward(ire_t *ire, ipha_t *ipha, mblk_t *mp)
{
	mblk_t	*mp1;
	uint32_t	pkt_len;
	queue_t	*q;
	uint32_t	sum;
	uint32_t	u1;
#define	rptr	((u_char *)ipha)
	uint32_t	max_frag;

	pkt_len = ntohs(ipha->ipha_length);

	/* Adjust the checksum to reflect the ttl decrement. */
	sum = (int)ipha->ipha_hdr_checksum + IP_HDR_CSUM_TTL_ADJUST;
	ipha->ipha_hdr_checksum = (uint16_t)(sum + (sum >> 16));

	if (ipha->ipha_ttl-- <= 1) {
		if (ip_csum_hdr(ipha)) {
			BUMP_MIB(ip_mib.ipInCksumErrs);
			goto drop_pkt;
		}
		/*
		 * Note: ire_stq this will be nil for multicast
		 * datagrams using the long path through arp (the IRE
		 * is not an IRE_CACHE). This should not cause
		 * problems since we don't generate ICMP errors for
		 * multicast packets.
		 */
		q = ire->ire_stq;
		if (q)
			icmp_time_exceeded(q, mp, ICMP_TTL_EXCEEDED);
		else
			freemsg(mp);
		return;
	}

	/* Check if there are options to update */
	if (!IS_SIMPLE_IPH(ipha)) {
		if (ip_csum_hdr(ipha)) {
			BUMP_MIB(ip_mib.ipInCksumErrs);
			goto drop_pkt;
		}
		if (ip_rput_forward_options(mp, ipha, ire))
			return;

		ipha->ipha_hdr_checksum = 0;
		ipha->ipha_hdr_checksum = (uint16_t)ip_csum_hdr(ipha);
	}
	/* Packet is being forwarded. Turning off hwcksum flag. */
	mp->b_ick_flag &= ~ICK_VALID;
	max_frag = ire->ire_max_frag;
	if (pkt_len > max_frag) {
		/*
		 * It needs fragging on its way out.  We haven't
		 * verified the header checksum yet.  Since we
		 * are going to put a surely good checksum in the
		 * outgoing header, we have to make sure that it
		 * was good coming in.
		 */
		if (ip_csum_hdr(ipha)) {
			BUMP_MIB(ip_mib.ipInCksumErrs);
			goto drop_pkt;
		}
		ip_wput_frag(ire, mp, &ire->ire_ib_pkt_count, max_frag, 0);

		return;
	}
	u1 = ire->ire_ll_hdr_length;
	mp1 = ire->ire_ll_hdr_mp;
	/*
	 * If the driver accepts M_DATA prepends
	 * and we have enough room to lay it in ...
	 */
	if (u1 && (rptr - mp->b_datap->db_base) >= u1) {
		/* XXX: ipha is only used as an alias for rptr */
		ipha = (ipha_t *)(rptr - u1);
		mp->b_rptr = rptr;
		/* TODO: inline this small copy */
		bcopy(mp1->b_rptr, rptr, u1);
	} else {
		mp1 = copyb(mp1);
		if (!mp1) {
			BUMP_MIB(ip_mib.ipInDiscards);
			goto drop_pkt;
		}
		mp1->b_cont = mp;
		mp = mp1;
	}
	q = ire->ire_stq;
	/* TODO: make this atomic */
	ire->ire_ib_pkt_count++;
	BUMP_MIB(ip_mib.ipForwDatagrams);
	putnext(q, mp);
	return;

drop_pkt:;
	ip1dbg(("ip_rput_forward: drop pkt\n"));
	freemsg(mp);
#undef	rptr
}

void
ip_rput_forward_multicast(ipaddr_t dst, mblk_t *mp, ipif_t *ipif)
{
	ire_t	*ire;

	/*
	 * Find an IRE which matches the destination and the outgoing
	 * queue (i.e. the outgoing logical interface) in the cache table.
	 */
	if (ipif->ipif_flags & IFF_POINTOPOINT)
		dst = ipif->ipif_pp_dst_addr;
	ire = ire_ctable_lookup(dst, 0, 0, ipif, NULL, MATCH_IRE_IPIF);
	if (!ire) {
		/*
		 * Mark this packet to make it be delivered to
		 * ip_rput_forward after the new ire has been
		 * created.
		 */
		mp->b_prev = nilp(mblk_t);
		mp->b_next = mp;
		ip_newroute_ipif(ipif->ipif_ill->ill_wq, mp, ipif, dst);
	} else
		ip_rput_forward(ire, (ipha_t *)mp->b_rptr, mp);
}

/* Update any source route, record route or timestamp options */
static int
ip_rput_forward_options(mblk_t *mp, ipha_t *ipha, ire_t *ire)
{
	uint32_t	totallen;
	u_char	*opt;
	uint32_t	optval;
	uint32_t	optlen;
	ipaddr_t dst;
	uint32_t	u1;

	ip2dbg(("ip_rput_forward_options\n"));
	dst = ipha->ipha_dst;
	totallen = ipha->ipha_version_and_hdr_length -
		(uint8_t)((IP_VERSION << 4) + IP_SIMPLE_HDR_LENGTH_IN_WORDS);
	opt = (u_char *)&ipha[1];
	totallen <<= 2;
	while (totallen != 0) {
		switch (optval = opt[IPOPT_POS_VAL]) {
		case IPOPT_EOL:
			return (0);
		case IPOPT_NOP:
			optlen = 1;
			break;
		default:
			optlen = opt[IPOPT_POS_LEN];
		}
		ip2dbg(("ip_rput_forward_options: opt %d, len %d\n",
		    optval, optlen));

		if (optlen == 0 || optlen > totallen)
			break;

		switch (optval) {
			uint32_t off;
		case IPOPT_SSRR:
		case IPOPT_LSRR:
			/* Check if adminstratively disabled */
			if (!ip_forward_src_routed) {
				BUMP_MIB(ip_mib.ipForwProhibits);
				if (ire->ire_stq)
					icmp_unreachable(ire->ire_stq, mp,
					    ICMP_SOURCE_ROUTE_FAILED);
				else {
					ip0dbg(("ip_rput_fw_options: "
					    "unable to send unreach\n"));
					freemsg(mp);
				}
				return (-1);
			}

			if (!ire_ctable_lookup(dst, 0, IRE_LOCAL, NULL, NULL,
			    MATCH_IRE_TYPE)) {
				/*
				 * Must be partial since ip_rput_options
				 * checked for strict.
				 */
				break;
			}
			off = opt[IPOPT_POS_OFF];
			off--;
			if (optlen < IP_ADDR_LEN ||
			    off > optlen - IP_ADDR_LEN) {
				/* End of source route */
				ip1dbg((
				    "ip_rput_forward_options: end of SR\n"));
				break;
			}
			bcopy((char *)opt + off, &dst, IP_ADDR_LEN);
			bcopy(&ire->ire_src_addr, (char *)opt + off,
			    IP_ADDR_LEN);
			ip1dbg(("ip_rput_forward_options: next hop 0x%x\n",
			    ntohl(dst)));
			ipha->ipha_dst = dst;
			opt[IPOPT_POS_OFF] += IP_ADDR_LEN;
			break;
		case IPOPT_RR:
			off = opt[IPOPT_POS_OFF];
			off--;
			if (optlen < IP_ADDR_LEN ||
			    off > optlen - IP_ADDR_LEN) {
				/* No more room - ignore */
				ip1dbg((
				    "ip_rput_forward_options: end of RR\n"));
				break;
			}
			bcopy(&ire->ire_src_addr, (char *)opt + off,
			    IP_ADDR_LEN);
			opt[IPOPT_POS_OFF] += IP_ADDR_LEN;
			break;
		case IPOPT_IT:
			/* Insert timestamp if there is romm */
			switch (opt[IPOPT_POS_OV_FLG] & 0x0F) {
			case IPOPT_IT_TIME:
				off = IPOPT_IT_TIMELEN;
				break;
			case IPOPT_IT_SPEC:
#ifdef IPOPT_IT_SPEC_BSD
			case IPOPT_IT_SPEC_BSD:
#endif
				/* Verify that the address matched */
				off = opt[IPOPT_POS_OFF] - 1;
				bcopy((char *)opt + off, &dst, IP_ADDR_LEN);
				if (!ire_ctable_lookup(dst, 0, IRE_LOCAL, NULL,
				    NULL, MATCH_IRE_TYPE))
					/* Not for us */
					break;
				/* FALLTHRU */
			case IPOPT_IT_TIME_ADDR:
				off = IP_ADDR_LEN + IPOPT_IT_TIMELEN;
				break;
			default:
				/*
				 * ip_*put_options should have already
				 * dropped this packet.
				 */
				cmn_err(CE_PANIC, "ip_rput_forward_options: "
				    "unknown IT - bug in ip_rput_options?\n");
				return (0);	/* Keep "lint" happy */
			}
			if (opt[IPOPT_POS_OFF] - 1 + off > optlen) {
				/* Increase overflow counter */
				off = (opt[IPOPT_POS_OV_FLG] >> 4) + 1;
				opt[IPOPT_POS_OV_FLG] =
				    (uint8_t)((opt[IPOPT_POS_OV_FLG] & 0x0F) |
				    (off << 4));
				break;
			}
			off = opt[IPOPT_POS_OFF] - 1;
			switch (opt[IPOPT_POS_OV_FLG] & 0x0F) {
			case IPOPT_IT_SPEC:
#ifdef IPOPT_IT_SPEC_BSD
			case IPOPT_IT_SPEC_BSD:
#endif
			case IPOPT_IT_TIME_ADDR:
				bcopy(&ire->ire_src_addr,
				    (char *)opt + off, IP_ADDR_LEN);
				opt[IPOPT_POS_OFF] += IP_ADDR_LEN;
				/* FALLTHRU */
			case IPOPT_IT_TIME:
				off = opt[IPOPT_POS_OFF] - 1;
				/* Compute # of milliseconds since midnight */
				u1 = (hrestime.tv_sec % (24 * 60 * 60)) * 1000 +
					hrestime.tv_nsec / (NANOSEC / MILLISEC);
				bcopy(&u1, (char *)opt + off, IPOPT_IT_TIMELEN);
				opt[IPOPT_POS_OFF] += IPOPT_IT_TIMELEN;
				break;
			}
			break;
		}
		totallen -= optlen;
		opt += optlen;
	}
	return (0);
}

/* This routine is needed for loopback when forwarding multicasts. */
void
ip_rput_local(queue_t *q, mblk_t *mp, ipha_t *ipha, ire_t *ire)
{
	dblk_t	*dp;
	ipaddr_t	dst;
	ill_t	*ill;
	icf_t	*icf;
	ipc_t	*ipc;
	mblk_t	*mp1;
	uint32_t	sum;
	uint32_t	u1;
	uint32_t	u2;
	int	sum_valid;
	uint16_t	*up;
	int	offset;
	ssize_t	len;
	uint32_t	ports;

	TRACE_1(TR_FAC_IP, TR_IP_RPUT_LOCL_START,
	    "ip_rput_locl_start: q %p", q);

#define	rptr	((u_char *)ipha)
#define	UDPH_SIZE 8

	/*
	 * FAST PATH for udp packets
	 */

	/* u1 is # words of IP options */
	u1 = ipha->ipha_version_and_hdr_length - (u_char)((IP_VERSION << 4)
	    + IP_SIMPLE_HDR_LENGTH_IN_WORDS);

	/* Check the IP header checksum.  */
#define	uph	((uint16_t *)ipha)
	sum = uph[0] + uph[1] + uph[2] + uph[3] + uph[4] + uph[5] + uph[6] +
		uph[7] + uph[8] + uph[9];
#undef  uph
	/* IP options present (udppullup uses u1) */
	if (u1) goto ipoptions;

	/* finish doing IP checksum */
	sum = (sum & 0xFFFF) + (sum >> 16);
	sum = ~(sum + (sum >> 16)) & 0xFFFF;

	/* verify checksum */
	if (sum && sum != 0xFFFF) goto cksumerror;

	/* count for SNMP of inbound packets for ire */
	ire->ire_ib_pkt_count++;

	/* packet part of fragmented IP packet? */
	u2 = ntohs(ipha->ipha_fragment_offset_and_flags);
	sum_valid = 0;
	u1 = u2 & (IPH_MF | IPH_OFFSET);
	if (u1) goto fragmented;

	/* u1 = IP header length (20 bytes) */
	u1 = IP_SIMPLE_HDR_LENGTH;

	dst = ipha->ipha_dst;
	/* protocol type not UDP (ports used as temporary) */
	if ((ports = ipha->ipha_protocol) != IPPROTO_UDP) goto notudp;

	/* packet does not contain complete IP & UDP headers */
	if ((mp->b_wptr - rptr) < (IP_SIMPLE_HDR_LENGTH + UDPH_SIZE))
		goto udppullup;

	/* up points to UDP header */
	up = (uint16_t *)((u_char *)ipha + IP_SIMPLE_HDR_LENGTH);

#define	iphs	((uint16_t *)ipha)
	/* if udp hdr cksum != 0, then need to checksum udp packet */
	if (up[3] && IP_CSUM(mp, (int32_t)((u_char *)up - (u_char *)ipha),
	    IP_UDP_CSUM_COMP + iphs[6] + iphs[7] + iphs[8] +
	    iphs[9] + up[2])) {
		goto badchecksum;
	}
	/* u1 = UDP destination port */
	u1 = up[1];	/* Destination port in net byte order */
	/* u2 = UDP src port */
	u2 = up[0];

	/* broadcast IP packet? */
	if (ire->ire_type == IRE_BROADCAST) goto udpslowpath;

	/* look for matching stream based on UDP addresses */
	icf = &ipc_udp_fanout[IP_UDP_HASH(u1)];
	mutex_enter(&icf->icf_lock);
	ipc = (ipc_t *)&icf->icf_ipc;	/* ipc_hash_next will get first */
	do {
		ipc = ipc->ipc_hash_next;
		if (ipc == NULL) {
			mutex_exit(&icf->icf_lock);
			goto udpslowpath;
		}
	} while (!IP_UDP_MATCH(ipc, u1, dst, u2, ipha->ipha_src));

	IPC_REFHOLD(ipc);
	mutex_exit(&icf->icf_lock);
	q = ipc->ipc_rq;

	/* stream blocked? */
	if (!canputnext(q)) {
		IPC_REFRELE(ipc);
		goto udpslowpath;
	}

	/* statistics */
	BUMP_MIB(ip_mib.ipInDelivers);

	TRACE_2(TR_FAC_IP, TR_IP_RPUT_LOCL_END,
	    "ip_rput_locl_end: q %p (%S)", q, "xx-putnext up");

	/* pass packet up to the transport */
	putnext(q, mp);
	IPC_REFRELE(ipc);
	return;

	/*
	 * FAST PATH for tcp packets
	 */
notudp:
	/* if not TCP, then just use default code */
	if (ports != IPPROTO_TCP) goto nottcp;

	/* does packet contain IP+TCP headers? */
	len = mp->b_wptr - rptr;
	if (len < (IP_SIMPLE_HDR_LENGTH + TCP_MIN_HEADER_LENGTH))
		goto tcppullup;

	/* TCP options present? */
	offset = ((u_char *)ipha)[IP_SIMPLE_HDR_LENGTH + 12] >> 4;
	if (offset != 5) goto tcpoptions;

	/* multiple mblocks of tcp data? */
	if ((mp1 = mp->b_cont) != NULL) {
		/* more then two? */
		if (mp1->b_cont) goto multipkttcp;
		len += mp1->b_wptr - mp1->b_rptr;
	}
	up = (uint16_t *)(rptr + IP_SIMPLE_HDR_LENGTH);

	ports = *(uint32_t *)up;

	/* part of pseudo checksum */
	u1 = (uint)len - IP_SIMPLE_HDR_LENGTH;
#ifdef  _BIG_ENDIAN
	u1 += IPPROTO_TCP;
#else
	u1 = ((u1 >> 8) & 0xFF) + (((u1 & 0xFF) + IPPROTO_TCP) << 8);
#endif
	u1 += iphs[6] + iphs[7] + iphs[8] + iphs[9];
	/*
	 * If not a duped mblk, then either it's a driver checksummed mblk
	 * and we validate or postpone the checksum to TCP for single copy
	 * checksum, else just do the checksum.
	 * Note that we only honor HW cksum in the fastpath.
	 */
	dp = mp->b_datap;
	if (dp->db_ref == 1) {
		if ((mp->b_ick_flag & ICK_VALID) && dohwcksum &&
		    ((len = (u_char *)up - mp->b_ick_start) & 1) == 0) {
			/*
			 * A driver checksummed mblk and prepended extraneous
			 * data (if any) is a multiple of 16 bits in size.
			 * First calculate the cksum for any extraneous data.
			 * Then clear inetcksum flag (now a normal mblk).
			 *
			 * Note: we know from above that this is either a
			 *	 single mblk or a 2 mblk chain and that
			 *	 any extraneous data can only be prepended
			 *	 to the first mblk or the end of the last.
			 */
#ifdef ZC_TEST
			zckstat->zc_hwcksum_r.value.ul++;
#endif
			u1 += mp->b_ick_value;
			if (!mp1)
				mp1 = mp;
			if (len > 0)
				u2 = IP_BCSUM_PARTIAL(mp->b_ick_start,
				    (int32_t)len, 0);
			else
				u2 = 0;
			if ((len = mp1->b_ick_end - mp1->b_wptr) > 0) {
				uint32_t	u3;
				u3 = IP_BCSUM_PARTIAL(mp1->b_wptr,
				    (uint32_t)len, 0);
				if ((uintptr_t)mp1->b_wptr & 1)
					/*
					 * Postpended extraneous data was
					 * odd byte aligned, so swap the
					 * resulting cksum bytes.
					 */
					u2 += ((u3 << 8) & 0xffff) | (u3 >> 8);
				else
					u2 += u3;
				u2 = (u2 & 0xFFFF) + ((int)u2 >> 16);
			}
			/* One's complement subtract extraneous checksum */
			if (u2 >= u1)
				u1 = ~(u2 - u1) & 0xFFFF;
			else
				u1 -= u2;
			u1 = (u1 & 0xFFFF) + ((int)u1 >> 16);
			u1 += (u1 >> 16);
			if (~u1 & 0xFFFF) {
				ipcsumdbg("hwcksumerr\n", mp);
				goto tcpcksumerr;
			}
#if 0
			if (syncstream) {
				/*
				 * XXX - have to also set db_struioptr,
				 * db_struioflag... on every mblk.
				 */
				dp->db_struioflag |= STRUIO_SPEC;
				mp1->b_datap->db_struioflag |= STRUIO_SPEC;
			}
#endif
#ifdef ZC_TEST
		} else if (syncstream) {
#else
		} else {
#endif
			u1 = (u1 >> 16) + (u1 & 0xffff);
			u1 += (u1 >> 16);
			*(uint16_t *)dp->db_struioun.data = (uint16_t)u1;
			dp->db_struiobase = (u_char *)up;
			dp->db_struioptr = (u_char *)up;
			dp->db_struiolim = mp->b_wptr;
			dp->db_struioflag |= STRUIO_SPEC|STRUIO_IP;
#ifdef ZC_TEST
			zckstat->zc_swcksum_r.value.ul++;
#endif
			if (mp1) {
				*(uint16_t *)mp1->b_datap->db_struioun.data = 0;
				mp1->b_datap->db_struiobase = mp1->b_rptr;
				mp1->b_datap->db_struioptr = mp1->b_rptr;
				mp1->b_datap->db_struiolim = mp1->b_wptr;
				mp1->b_datap->db_struioflag |=
				    STRUIO_SPEC | STRUIO_IP;
			}
		}
#ifdef ZC_TEST
		else if (IP_CSUM(mp, (u_char *)up - rptr, u1)) {
			zckstat->zc_swcksum_r.value.ul++;
			ipcsumdbg("swcksumerr2\n", mp);
			goto tcpcksumerr;
		}
#endif
	} else {
#ifdef ZC_TEST
		zckstat->zc_swcksum_r.value.ul++;
#endif
		if (IP_CSUM(mp, (int32_t)((u_char *)up - rptr), u1)) {
			ipcsumdbg("swcksumerr\n", mp);
			goto tcpcksumerr;
		}
	}

	/* Find a TCP client stream for this packet. */
	icf = &ipc_tcp_conn_fanout[IP_TCP_CONN_HASH(ipha->ipha_src, ports)];
	mutex_enter(&icf->icf_lock);
	ipc = (ipc_t *)&icf->icf_ipc;	/* ipc_hash_next will get first */
	do {
		ipc = ipc->ipc_hash_next;
		if (ipc == NULL) {
			mutex_exit(&icf->icf_lock);
			goto notcpport;
		}
	} while (!IP_TCP_CONN_MATCH(ipc, ipha, ports));

	/* Got a client */
	IPC_REFHOLD(ipc);
	mutex_exit(&icf->icf_lock);
	q = ipc->ipc_rq;
	/* ip module statistics */
	BUMP_MIB(ip_mib.ipInDelivers);
	TRACE_2(TR_FAC_IP, TR_IP_RPUT_LOCL_END,
	    "ip_rput_locl_end: q %p (%S)", q, "tcp");
	putnext(q, mp);
	IPC_REFRELE(ipc);
	return;

ipoptions:
	{
		/* Add in IP options. */
		uint16_t	*uph = &((uint16_t *)ipha)[10];
		do {
			sum += uph[0];
			sum += uph[1];
			uph += 2;
		} while (--u1);
		if (ip_rput_local_options(q, mp, ipha, ire)) {
			TRACE_2(TR_FAC_IP, TR_IP_RPUT_LOCL_END,
			    "ip_rput_locl_end: q %p (%S)", q, "badopts");
			return;
		}
	}
	sum = (sum & 0xFFFF) + (sum >> 16);
	sum = ~(sum + (sum >> 16)) & 0xFFFF;
	if (sum && sum != 0xFFFF) {
cksumerror:
		BUMP_MIB(ip_mib.ipInCksumErrs);
		goto drop_pkt;
	}

	/* TODO: make this atomic */
	ire->ire_ib_pkt_count++;
	/* Check for fragmentation offset. */
	u2 = ntohs(ipha->ipha_fragment_offset_and_flags);
	sum_valid = 0;
	u1 = u2 & (IPH_MF | IPH_OFFSET);
	if (u1) {
		ipf_t	*ipf;
		ipf_t	**ipfp;
		ipfb_t	*ipfb;
		uint16_t	ident;
		uint32_t	offset;
		ipaddr_t src;
		u_int	hdr_length;
		uint32_t	end;
		uint32_t	proto;
		mblk_t	*mp1;
		size_t	count;

fragmented:
		ident = ipha->ipha_ident;
		offset = (u1 << 3) & 0xFFFF;
		src = ipha->ipha_src;
		hdr_length = (ipha->ipha_version_and_hdr_length & 0xF) << 2;
		end = ntohs(ipha->ipha_length) - hdr_length;

		/*
		 * if end == 0 then we have a packet with no data, so just
		 * free it.
		 */
		if (end == 0) {
			freemsg(mp);
			return;
		}
		proto = ipha->ipha_protocol;

		/*
		 * Fragmentation reassembly.  Each ILL has a hash table for
		 * queuing packets undergoing reassembly for all IPIFs
		 * associated with the ILL.  The hash is based on the packet
		 * IP ident field.  The ILL frag hash table was allocated
		 * as a timer block at the time the ILL was created.  Whenever
		 * there is anything on the reassembly queue, the timer will
		 * be running.
		 */
		ill = (ill_t *)q->q_ptr;
		/*
		 * Compute a partial checksum before acquiring the lock
		 */
		sum = IP_CSUM_PARTIAL(mp, hdr_length, 0);

		if (offset) {
			/*
			 * If this isn't the first piece, strip the header, and
			 * add the offset to the end value.
			 */
			mp->b_rptr += hdr_length;
			end += offset;
		}
		/*
		 * If the reassembly list for this ILL is to big, prune it.
		 */
		if (ill->ill_frag_count > ip_reass_queue_bytes)
			ill_frag_prune(ill, ip_reass_queue_bytes);
		ipfb = &ill->ill_frag_hash_tbl[ILL_FRAG_HASH(src, ident)];
		mutex_enter(&ipfb->ipfb_lock);
		ipfp = &ipfb->ipfb_ipf;
		/* Try to find an existing fragment queue for this packet. */
		for (;;) {
			ipf = ipfp[0];
			if (ipf) {
				/*
				 * It has to match on ident and source address.
				 */
				if (ipf->ipf_ident == ident &&
				    ipf->ipf_src == src &&
				    ipf->ipf_protocol == proto) {
					/* Found it. */
					break;
				}
				ipfp = &ipf->ipf_hash_next;
				continue;
			}
			/* New guy.  Allocate a frag message. */
			mp1 = allocb(sizeof (*ipf), BPRI_MED);
			if (!mp1) {
				BUMP_MIB(ip_mib.ipInDiscards);
				freemsg(mp);
				TRACE_2(TR_FAC_IP, TR_IP_RPUT_LOCL_ERR,
				    "ip_rput_locl_err: q %p (%S)", q,
				    "ALLOCBFAIL");
reass_done:;
				mutex_exit(&ipfb->ipfb_lock);
				TRACE_2(TR_FAC_IP, TR_IP_RPUT_LOCL_END,
				    "ip_rput_locl_end: q %p (%S)", q,
				    "reass_done");
				return;
			}
			BUMP_MIB(ip_mib.ipReasmReqds);
			mp1->b_cont = mp;

			/* Initialize the fragment header. */
			ipf = (ipf_t *)mp1->b_rptr;
			ipf->ipf_checksum = 0;
			ipf->ipf_checksum_valid = 1;
			ipf->ipf_mp = mp1;
			ipf->ipf_ptphn = ipfp;
			ipfp[0] = ipf;
			ipf->ipf_hash_next = nilp(ipf_t);
			ipf->ipf_ident = ident;
			ipf->ipf_protocol = (uint8_t)proto;
			ipf->ipf_src = src;
			/* Record reassembly start time. */
			ipf->ipf_timestamp = hrestime.tv_sec;
			/* Record ipf generation and account for frag header */
			ipf->ipf_gen = ill->ill_ipf_gen++;
			ipf->ipf_count = mp1->b_datap->db_lim -
			    mp1->b_datap->db_base;
			/*
			 * We handle reassembly two ways.  In the easy case,
			 * where all the fragments show up in order, we do
			 * minimal bookkeeping, and just clip new pieces on
			 * the end.  If we ever see a hole, then we go off
			 * to ip_reassemble which has to mark the pieces and
			 * keep track of the number of holes, etc.  Obviously,
			 * the point of having both mechanisms is so we can
			 * handle the easy case as efficiently as possible.
			 */
			if (offset == 0) {
				/* Easy case, in-order reassembly so far. */
				/* Compute a partial checksum and byte count */
				sum += ipf->ipf_checksum;
				sum = (sum & 0xFFFF) + (sum >> 16);
				sum = (sum & 0xFFFF) + (sum >> 16);
				ipf->ipf_checksum = (uint16_t)sum;
				ipf->ipf_count += mp->b_datap->db_lim -
				    mp->b_datap->db_base;
				while (mp->b_cont) {
					mp = mp->b_cont;
					ipf->ipf_count += mp->b_datap->db_lim -
					    mp->b_datap->db_base;
				}
				ipf->ipf_tail_mp = mp;
				/*
				 * Keep track of next expected offset in
				 * ipf_end.
				 */
				ipf->ipf_end = end;
				ipf->ipf_stripped_hdr_len = 0;
			} else {
				/* Hard case, hole at the beginning. */
				ipf->ipf_tail_mp = nilp(mblk_t);
				/*
				 * ipf_end == 0 means that we have given up
				 * on easy reassembly.
				 */
				ipf->ipf_end = 0;
				/*
				 * Toss the partial checksum since it is to
				 * hard to compute it with potentially
				 * overlapping fragments.
				 */
				ipf->ipf_checksum_valid = 0;
				/*
				 * ipf_hole_cnt and ipf_stripped_hdr_len are
				 * set by ip_reassemble.
				 *
				 * ipf_count is updated by ip_reassemble.
				 */
				(void) ip_reassemble(mp, ipf, u1, hdr_length);
			}
			/* Update per ipfb and ill byte counts */
			ipfb->ipfb_count += ipf->ipf_count;
			ill->ill_frag_count += ipf->ipf_count;
			/* If the frag timer wasn't already going, start it. */
			if (!ill->ill_frag_timer_running) {
				ill->ill_frag_timer_running = true;
				mi_timer(ill->ill_rq, ill->ill_frag_timer_mp,
				    ip_g_frag_timo_ms);
			}
			goto reass_done;
		}

		/*
		 * We have a new piece of a datagram which is already being
		 * reassembled.
		 */
		if (offset && ipf->ipf_end == offset) {
			/* The new fragment fits at the end */
			ipf->ipf_tail_mp->b_cont = mp;
			/* Update the partial checksum and byte count */
			sum += ipf->ipf_checksum;
			sum = (sum & 0xFFFF) + (sum >> 16);
			sum = (sum & 0xFFFF) + (sum >> 16);
			ipf->ipf_checksum = (uint16_t)sum;
			count = mp->b_datap->db_lim -
				mp->b_datap->db_base;
			while (mp->b_cont) {
				mp = mp->b_cont;
				count += mp->b_datap->db_lim -
				    mp->b_datap->db_base;
			}
			ipf->ipf_count += count;
			/* Update per ipfb and ill byte counts */
			ipfb->ipfb_count += count;
			ill->ill_frag_count += count;
			if (u1 & IPH_MF) {
				/* More to come. */
				ipf->ipf_end = end;
				ipf->ipf_tail_mp = mp;
				goto reass_done;
			}
		} else {
			/* Go do the hard cases. */
			/*
			 * Toss the partial checksum since it is to hard to
			 * compute it with potentially overlapping fragments.
			 * Call ip_reassable().
			 */
			boolean_t ret;

			/* Save current byte count */
			count = ipf->ipf_count;
			ipf->ipf_checksum_valid = 0;
			ret = ip_reassemble(mp, ipf, u1, offset ? hdr_length:0);
			/* Count of bytes added and subtracted (freeb()ed) */
			count = ipf->ipf_count - count;
			if (count) {
				/* Update per ipfb and ill byte counts */
				ipfb->ipfb_count += count;
				ill->ill_frag_count += count;
			}
			if (! ret)
				goto reass_done;
			/* Return value of 'true' means mp is complete. */
		}
		/*
		 * We have completed reassembly.  Unhook the frag header from
		 * the reassembly list.
		 */
		BUMP_MIB(ip_mib.ipReasmOKs);
		ipfp = ipf->ipf_ptphn;
		mp1 = ipf->ipf_mp;
		sum_valid = ipf->ipf_checksum_valid;
		sum = ipf->ipf_checksum;
		count = ipf->ipf_count;
		ipf = ipf->ipf_hash_next;
		if (ipf)
			ipf->ipf_ptphn = ipfp;
		ipfp[0] = ipf;
		ill->ill_frag_count -= count;
		ASSERT(ipfb->ipfb_count >= count);
		ipfb->ipfb_count -= count;
		mutex_exit(&ipfb->ipfb_lock);
		/* Ditch the frag header. */
		mp = mp1->b_cont;

		freeb(mp1);

		/* Restore original IP length in header. */
		u2 = (uint32_t)msgdsize(mp);
		if (u2 > IP_MAXPACKET) {
			freemsg(mp);
			BUMP_MIB(ip_mib.ipInHdrErrors);
			TRACE_2(TR_FAC_IP, TR_IP_RPUT_LOCL_END,
			    "ip_rput_locl_end: q %p (%S)", q, "ipReasmFails");
			return;
		}

		if (mp->b_datap->db_ref > 1) {
			mblk_t *mp2;

			mp2 = copymsg(mp);
			freemsg(mp);
			if (!mp2) {
				BUMP_MIB(ip_mib.ipInDiscards);
				return;
			}
			mp = mp2;
		}
		ipha = (ipha_t *)mp->b_rptr;

		ipha->ipha_length = htons((uint16_t)u2);
		/* We're now complete, zip the frag state */
		ipha->ipha_fragment_offset_and_flags = 0;
	}

	/* Now we have a complete datagram, destined for this machine. */
	u1 = (ipha->ipha_version_and_hdr_length & 0xF) << 2;
	dst = ipha->ipha_dst;
nottcp:
	switch (ipha->ipha_protocol) {
	case IPPROTO_ICMP:
	case IPPROTO_IGMP: {
		/* Figure out the the incoming logical interface */
		ipif_t	*ipif = nilp(ipif_t);

		if (ire->ire_type == IRE_BROADCAST) {
			ipaddr_t	src;

			if (q == NULL)
				(void) mi_panic("ip_rput_local");
			ill = (ill_t *)q->q_ptr;
			src = ipha->ipha_src;
			ipif = ipif_lookup_remote(ill, src);
			if (!ipif) {
#ifdef IP_DEBUG
				ipif_t	*ipif;
				ipif = ill->ill_ipif;
				ip0dbg(("ip_rput_local: "
				    "No source for broadcast/multicast:\n"
				    "\tsrc 0x%x dst 0x%x ill %p "
				    "ipif_local_addr 0x%x\n",
				    ntohl(src), ntohl(dst),
				    (void *)ill,
				    (ipif ? ipif->ipif_local_addr :
					INADDR_BROADCAST)));
#endif
				freemsg(mp);
				TRACE_2(TR_FAC_IP, TR_IP_RPUT_LOCL_END,
				    "ip_rput_locl_end: q %p (%S)", q,
				    "noipif");
				return;
			}
		}
		if (ipha->ipha_protocol == IPPROTO_ICMP) {
			icmp_inbound(q, mp, ire->ire_type, ipif, sum_valid,
			    sum);
			TRACE_2(TR_FAC_IP, TR_IP_RPUT_LOCL_END,
			    "ip_rput_locl_end: q %p (%S)", q, "icmp");
			return;
		}
		if (igmp_input(q, mp, ipif)) {
			/* Bad packet - discarded by igmp_input */
			TRACE_2(TR_FAC_IP, TR_IP_RPUT_LOCL_END,
			    "ip_rput_locl_end: q %p (%S)", q, "igmp");
			return;
		}
		if (ipc_proto_fanout[ipha->ipha_protocol].icf_ipc == NULL) {
			/* No user-level listener for IGMP packets */
			goto drop_pkt;
		}
		/* deliver to local raw users */
		break;
	}
	case IPPROTO_ENCAP:
		if (ip_g_mrouter) {
			mblk_t	*mp2;

			if (ipc_proto_fanout[IPPROTO_ENCAP].icf_ipc == NULL) {
				ip_mroute_decap(q, mp);
				return;
			}
			mp2 = dupmsg(mp);
			if (mp2)
				ip_mroute_decap(q, mp2);
		}
		break;
	case IPPROTO_UDP:
		{
		/* Pull up the UDP header, if necessary. */
		if ((mp->b_wptr - mp->b_rptr) < (u1 + 8)) {
udppullup:
			if (!pullupmsg(mp, u1 + 8)) {
				BUMP_MIB(ip_mib.ipInDiscards);
				freemsg(mp);
				TRACE_2(TR_FAC_IP, TR_IP_RPUT_LOCL_END,
				    "ip_rput_locl_end: q %p (%S)", q, "udp");
				return;
			}
			ipha = (ipha_t *)mp->b_rptr;
		}
		/*
		 * Validate the checksum.  This code is a bit funny looking
		 * but may help out the compiler in this crucial spot.
		 */
		up = (uint16_t *)((u_char *)ipha + u1);
		if (up[3]) {
			if (sum_valid) {
				sum += IP_UDP_CSUM_COMP + iphs[6] + iphs[7] +
				    iphs[8] + iphs[9] + up[2];
				sum = (sum & 0xFFFF) + (sum >> 16);
				sum = ~(sum + (sum >> 16)) & 0xFFFF;
				if (sum && sum != 0xFFFF) {
					ip1dbg(("ip_rput_local: "
					    "bad udp checksum 0x%x\n", sum));
					BUMP_MIB(ip_mib.udpInCksumErrs);
					goto drop_pkt;
				}
			} else {
				if (IP_CSUM(mp,
				    (int32_t)((u_char *)up - (u_char *)ipha),
				    IP_UDP_CSUM_COMP + iphs[6] +
				    iphs[7] + iphs[8] +
				    iphs[9] + up[2])) {
badchecksum:
					BUMP_MIB(ip_mib.udpInCksumErrs);
					goto drop_pkt;
				}
			}
		}
udpslowpath:
		ports = *(uint32_t *)up;
		ip_fanout_udp(q, mp, ipha, ports, ire->ire_type,
		    IP_FF_SEND_ICMP | IP_FF_CKSUM | IP_FF_SRC_QUENCH);
		TRACE_2(TR_FAC_IP, TR_IP_RPUT_LOCL_END,
		    "ip_rput_locl_end: q %p (%S)", q, "ip_fanout_udp");
		return;
	}

	case IPPROTO_TCP:
		{
		len = mp->b_wptr - mp->b_rptr;

		/* Pull up a minimal TCP header, if necessary. */
		if (len < (u1 + 20)) {
tcppullup:
			if (!pullupmsg(mp, u1 + 20)) {
				BUMP_MIB(ip_mib.ipInDiscards);
				freemsg(mp);
				TRACE_2(TR_FAC_IP, TR_IP_RPUT_LOCL_END,
				    "ip_rput_locl_end: q %p (%S)", q,
				    "tcp:badpullup");
				return;
			}
			ipha = (ipha_t *)mp->b_rptr;
			len = mp->b_wptr - mp->b_rptr;
		}

		/*
		 * Extract the offset field from the TCP header.  As usual, we
		 * try to help the compiler more than the reader.
		 */
		offset = ((u_char *)ipha)[u1 + 12] >> 4;
		if (offset != 5) {
tcpoptions:
			if (offset < 5) {
				freemsg(mp);
				TRACE_2(TR_FAC_IP, TR_IP_RPUT_LOCL_END,
				    "ip_rput_locl_end: q %p (%S)", q,
				    "tcp_badoff");
				return;
			}
			/*
			 * There must be TCP options.
			 * Make sure we can grab them.
			 */
			offset <<= 2;
			offset += u1;
			if (len < offset) {
				if (!pullupmsg(mp, offset)) {
					BUMP_MIB(ip_mib.ipInDiscards);
					freemsg(mp);
					TRACE_2(TR_FAC_IP, TR_IP_RPUT_LOCL_END,
					    "ip_rput_locl_end: q %p (%S)", q,
					    "tcp:pullupfailed");
					return;
				}
				ipha = (ipha_t *)mp->b_rptr;
				len = mp->b_wptr - rptr;
			}
		}

		/* Get the total packet length in len, including headers. */
		if (mp->b_cont) {
multipkttcp:
			len = msgdsize(mp);
		}
#ifdef ZC_TEST
		zckstat->zc_slowpath_r.value.ul++;
#endif
		/*
		 * Check the TCP checksum by pulling together the pseudo-
		 * header checksum, and passing it to ip_csum to be added in
		 * with the TCP datagram.
		 */
		up = (uint16_t *)(rptr + u1); /* TCP header pointer. */
		ports = *(uint32_t *)up;
		u1 = (uint32_t)(len - u1);	/* TCP datagram length. */
#ifdef	_BIG_ENDIAN
		u1 += IPPROTO_TCP;
#else
		u1 = ((u1 >> 8) & 0xFF) + (((u1 & 0xFF) + IPPROTO_TCP) << 8);
#endif
		u1 += iphs[6] + iphs[7] + iphs[8] + iphs[9];
		dp = mp->b_datap;
		if (dp->db_type != M_DATA || dp->db_ref > 1
		    /* BEGIN CSTYLED */
#ifdef ZC_TEST
		    || !syncstream
#endif
			) {
			/* END CSTYLED */
			/*
			 * Not M_DATA mblk or its a dup, so do the checksum now.
			 */
			if (IP_CSUM(mp, (int32_t)((u_char *)up - rptr), u1)) {
tcpcksumerr:
				BUMP_MIB(ip_mib.tcpInErrs);
				goto drop_pkt;
			}
		} else {
			/*
			 * M_DATA mblk and not a dup, so postpone the checksum.
			 */
			mblk_t	*mp1 = mp;
			dblk_t	*dp1;

			u1 = (u1 >> 16) + (u1 & 0xffff);
			u1 += (u1 >> 16);
			*(uint16_t *)dp->db_struioun.data = (uint16_t)u1;
			dp->db_struiobase = (u_char *)up;
			dp->db_struioptr = (u_char *)up;
			dp->db_struiolim = mp->b_wptr;
			dp->db_struioflag |= STRUIO_SPEC|STRUIO_IP;
			while ((mp1 = mp1->b_cont) != NULL) {
				dp1 = mp1->b_datap;
				*(uint16_t *)dp1->db_struioun.data = 0;
				dp1->db_struiobase = mp1->b_rptr;
				dp1->db_struioptr = mp1->b_rptr;
				dp1->db_struiolim = mp1->b_wptr;
				dp1->db_struioflag |= STRUIO_SPEC|STRUIO_IP;
			}
		}

notcpport:
		ip_fanout_tcp(q, mp, ipha, ports,
		    IP_FF_SEND_ICMP | IP_FF_CKSUM | IP_FF_SRC_QUENCH |
		    IP_FF_SYN_ADDIRE);
		TRACE_2(TR_FAC_IP, TR_IP_RPUT_LOCL_END,
		    "ip_rput_locl_end: q %p (%S)", q, "ip_fanout_tcp");
		return;
		}
	default:
		break;
	}
	/*
	 * Handle protocols with which IP is less intimate.  There
	 * can be more than one stream bound to a particular
	 * protocol.  When this is the case, each one gets a copy
	 * of any incoming packets.
	 */
	ip_fanout_proto(q, mp, ipha,
	    IP_FF_SEND_ICMP | IP_FF_CKSUM | IP_FF_SRC_QUENCH | IP_FF_RAWIP);
	TRACE_2(TR_FAC_IP, TR_IP_RPUT_LOCL_END,
	    "ip_rput_locl_end: q %p (%S)", q, "ip_fanout_proto");
	return;

drop_pkt:;
	freemsg(mp);
	TRACE_2(TR_FAC_IP, TR_IP_RPUT_LOCL_END,
	    "ip_rput_locl_end: q %p (%S)", q, "droppkt");
#undef	rptr
}

/* Update any source route, record route or timestamp options */
/* Check that we are at end of strict source route */
static int
ip_rput_local_options(queue_t *q, mblk_t *mp, ipha_t *ipha, ire_t *ire)
{
	uint32_t	totallen;
	u_char		*opt;
	uint32_t	optval;
	uint32_t	optlen;
	ipaddr_t 	dst;
	uint32_t	u1;

	ip2dbg(("ip_rput_local_options\n"));
	totallen = ipha->ipha_version_and_hdr_length -
		(uint8_t)((IP_VERSION << 4) + IP_SIMPLE_HDR_LENGTH_IN_WORDS);
	opt = (u_char *)&ipha[1];
	totallen <<= 2;
	while (totallen != 0) {
		switch (optval = opt[IPOPT_POS_VAL]) {
		case IPOPT_EOL:
			return (0);
		case IPOPT_NOP:
			optlen = 1;
			break;
		default:
			optlen = opt[IPOPT_POS_LEN];
		}
		ip2dbg(("ip_rput_local_options: opt %d, len %d\n",
		    optval, optlen));

		if (optlen == 0 || optlen > totallen)
			break;

		switch (optval) {
			uint32_t off;
		case IPOPT_SSRR:
		case IPOPT_LSRR:
			off = opt[IPOPT_POS_OFF];
			off--;
			if (optlen < IP_ADDR_LEN ||
			    off > optlen - IP_ADDR_LEN) {
				/* End of source route */
				ip1dbg(("ip_rput_local_options: end of SR\n"));
				break;
			}
			/*
			 * This will only happen if two consecutive entries
			 * in the source route contains our address or if
			 * it is a packet with a loose source route which
			 * reaches us before consuming the whole source route
			 */
			ip1dbg(("ip_rput_local_options: not end of SR\n"));
			if (optval == IPOPT_SSRR) {
				goto bad_src_route;
			}
			/*
			 * Hack: instead of dropping the packet truncate the
			 * source route to what has been used.
			 */
			bzero((char *)opt + off, optlen - off);
			opt[IPOPT_POS_LEN] = (uint8_t)off;
			break;
		case IPOPT_RR:
			off = opt[IPOPT_POS_OFF];
			off--;
			if (optlen < IP_ADDR_LEN ||
			    off > optlen - IP_ADDR_LEN) {
				/* No more room - ignore */
				ip1dbg((
				    "ip_rput_forward_options: end of RR\n"));
				break;
			}
			bcopy(&ire->ire_src_addr, (char *)opt + off,
			    IP_ADDR_LEN);
			opt[IPOPT_POS_OFF] += IP_ADDR_LEN;
			break;
		case IPOPT_IT:
			/* Insert timestamp if there is romm */
			switch (opt[IPOPT_POS_OV_FLG] & 0x0F) {
			case IPOPT_IT_TIME:
				off = IPOPT_IT_TIMELEN;
				break;
			case IPOPT_IT_SPEC:
#ifdef IPOPT_IT_SPEC_BSD
			case IPOPT_IT_SPEC_BSD:
#endif
				/* Verify that the address matched */
				off = opt[IPOPT_POS_OFF] - 1;
				bcopy((char *)opt + off, &dst, IP_ADDR_LEN);
				if (!ire_ctable_lookup(dst, 0, IRE_LOCAL, NULL,
				    NULL, MATCH_IRE_TYPE))
					/* Not for us */
					break;
				/* FALLTHRU */
			case IPOPT_IT_TIME_ADDR:
				off = IP_ADDR_LEN + IPOPT_IT_TIMELEN;
				break;
			default:
				/*
				 * ip_*put_options should have already
				 * dropped this packet.
				 */
				cmn_err(CE_PANIC, "ip_rput_local_options: "
				    "unknown IT - bug in ip_rput_options?\n");
				return (0);	/* Keep "lint" happy */
			}
			if (opt[IPOPT_POS_OFF] - 1 + off > optlen) {
				/* Increase overflow counter */
				off = (opt[IPOPT_POS_OV_FLG] >> 4) + 1;
				opt[IPOPT_POS_OV_FLG] =
				    (uint8_t)((opt[IPOPT_POS_OV_FLG] & 0x0F) |
				    (off << 4));
				break;
			}
			off = opt[IPOPT_POS_OFF] - 1;
			switch (opt[IPOPT_POS_OV_FLG] & 0x0F) {
			case IPOPT_IT_SPEC:
#ifdef IPOPT_IT_SPEC_BSD
			case IPOPT_IT_SPEC_BSD:
#endif
			case IPOPT_IT_TIME_ADDR:
				bcopy(&ire->ire_src_addr, (char *)opt + off,
				    IP_ADDR_LEN);
				opt[IPOPT_POS_OFF] += IP_ADDR_LEN;
				/* FALLTHRU */
			case IPOPT_IT_TIME:
				off = opt[IPOPT_POS_OFF] - 1;
				/* Compute # of milliseconds since midnight */
				u1 = (hrestime.tv_sec % (24 * 60 * 60)) * 1000 +
					hrestime.tv_nsec / (NANOSEC / MILLISEC);
				bcopy(&u1, (char *)opt + off, IPOPT_IT_TIMELEN);
				opt[IPOPT_POS_OFF] += IPOPT_IT_TIMELEN;
				break;
			}
			break;
		}
		totallen -= optlen;
		opt += optlen;
	}
	return (0);

bad_src_route:
	q = WR(q);
	icmp_unreachable(q, mp, ICMP_SOURCE_ROUTE_FAILED);
	return (-1);

}

/*
 * Process IP options in an inbound packet.  If an option affects the
 * effective destination address, return the next hop address.
 * Returns -1 if something fails in which case an ICMP error has been sent
 * and mp freed.
 */
static ipaddr_t
ip_rput_options(queue_t *q, mblk_t *mp, ipha_t *ipha)
{
	uint32_t	totallen;
	u_char		*opt;
	uint32_t	optval;
	uint32_t	optlen;
	ipaddr_t 	dst;
	intptr_t	code = 0;

	ip2dbg(("ip_rput_options\n"));
	dst = ipha->ipha_dst;
	totallen = ipha->ipha_version_and_hdr_length -
		(uint8_t)((IP_VERSION << 4) + IP_SIMPLE_HDR_LENGTH_IN_WORDS);
	opt = (u_char *)&ipha[1];
	totallen <<= 2;
	while (totallen != 0) {
		switch (optval = opt[IPOPT_POS_VAL]) {
		case IPOPT_EOL:
			return (dst);
		case IPOPT_NOP:
			optlen = 1;
			break;
		default:
			optlen = opt[IPOPT_POS_LEN];
		}
		if (optlen == 0 || optlen > totallen) {
			ip1dbg(("ip_rput_options: bad option len %d, %d\n",
			    optlen, totallen));
			code = (char *)&opt[IPOPT_POS_LEN] - (char *)ipha;
			goto param_prob;
		}
		ip2dbg(("ip_rput_options: opt %d, len %d\n",
		    optval, optlen));
		/*
		 * Note: we need to verify the checksum before we
		 * modify anything thus this routine only extracts the next
		 * hop dst from any source route.
		 */
		switch (optval) {
			uint32_t off;
		case IPOPT_SSRR:
		case IPOPT_LSRR:
			if (!ire_ctable_lookup(dst, 0, IRE_LOCAL, NULL, NULL,
			    MATCH_IRE_TYPE)) {
				if (optval == IPOPT_SSRR) {
					ip1dbg(("ip_rput_options: not next"
					    " strict source route 0x%x\n",
					    ntohl(dst)));
					code = (char *)&ipha->ipha_dst -
					    (char *)ipha;
					goto param_prob; /* RouterReq's */
				}
				ip2dbg(("ip_rput_options: "
				    "not next source route 0x%x\n",
				    ntohl(dst)));
				break;
			}
			off = opt[IPOPT_POS_OFF];
			if (off < IPOPT_MINOFF_SR) {
				ip1dbg(("ip_rput_options: bad option"
				    " offset %d\n",
				    off));
				code = (char *)&opt[IPOPT_POS_OFF] -
				    (char *)ipha;
				goto param_prob;
			}
			off--;
			if (optlen < IP_ADDR_LEN ||
			    off > optlen - IP_ADDR_LEN) {
				/* End of source route */
				ip1dbg(("ip_rput_options: end of SR\n"));
				break;
			}
			bcopy((char *)opt + off, &dst, IP_ADDR_LEN);
			ip1dbg(("ip_rput_options: next hop 0x%x\n",
			    ntohl(dst)));
			/*
			 * For strict: verify that dst is directly
			 * reachable.
			 */
			if (optval == IPOPT_SSRR &&
			    !ire_ftable_lookup(dst, 0, 0, IRE_INTERFACE, NULL,
				NULL, NULL, MATCH_IRE_TYPE)) {
				ip1dbg(("ip_rput_options: SSRR not directly"
				    " reachable: 0x%x\n",
				    ntohl(dst)));
				goto bad_src_route;
			}
			/*
			 * Defer update of the offset and the record route
			 * until the packet is forwarded.
			 */
			break;
		case IPOPT_RR:
			off = opt[IPOPT_POS_OFF];
			if (off < IPOPT_MINOFF_SR) {
				ip1dbg((
				    "ip_rput_options: bad option offset %d\n",
				    off));
				code = (char *)&opt[IPOPT_POS_OFF] -
				    (char *)ipha;
				goto param_prob;
			}
			break;
		case IPOPT_IT:
			/*
			 * Verify that length >= 5 and that there is either
			 * room for another timestamp or that the overflow
			 * counter is not maxed out.
			 */
			code = (char *)&opt[IPOPT_POS_LEN] - (char *)ipha;
			if (optlen < IPOPT_MINLEN_IT) {
				goto param_prob;
			}
			switch (opt[IPOPT_POS_OV_FLG] & 0x0F) {
			case IPOPT_IT_TIME:
				off = IPOPT_IT_TIMELEN;
				break;
			case IPOPT_IT_TIME_ADDR:
			case IPOPT_IT_SPEC:
#ifdef IPOPT_IT_SPEC_BSD
			case IPOPT_IT_SPEC_BSD:
#endif
				off = IP_ADDR_LEN + IPOPT_IT_TIMELEN;
				break;
			default:
				code = (char *)&opt[IPOPT_POS_OV_FLG] -
				    (char *)ipha;
				goto param_prob;
			}
			if (opt[IPOPT_POS_OFF] - 1 + off > optlen &&
			    (opt[IPOPT_POS_OV_FLG] & 0xF0) == 0xF0) {
				/*
				 * No room and the overflow counter is 15
				 * already.
				 */
				goto param_prob;
			}
			off = opt[IPOPT_POS_OFF];
			if (off < IPOPT_MINOFF_IT) {
				ip1dbg((
				    "ip_rput_options: bad option offset %d\n",
				    off));
				code = (char *)&opt[IPOPT_POS_OFF] -
					(char *)ipha;
				goto param_prob;
			}
			break;
		}
		totallen -= optlen;
		opt += optlen;
	}
	return (dst);

param_prob:
	q = WR(q);
	icmp_param_problem(q, mp, (uint8_t)code);
	return (INADDR_BROADCAST);

bad_src_route:
	q = WR(q);
	icmp_unreachable(q, mp, ICMP_SOURCE_ROUTE_FAILED);
	return (INADDR_BROADCAST);
}

/*
 * Read service routine.  We see three kinds of messages here.  The first is
 * fragmentation reassembly timer messages.  The second is data packets
 * that were queued to avoid possible intra-machine looping conditions
 * on Streams implementations that do not support QUEUEPAIR synchronization.
 * The third is the IRE expiration timer message.
 */
static void
ip_rsrv(queue_t *q)
{
	ill_t	*ill = (ill_t *)q->q_ptr;
	mblk_t	*mp;

	TRACE_1(TR_FAC_IP, TR_IP_RSRV_START, "ip_rsrv_start: q %p", q);

	while ((mp = getq(q)) != NULL) {
		/* TODO need define for M_PCSIG! */
		if (mp->b_datap->db_type == M_PCSIG) {
			/* Timer */
			if (!mi_timer_valid(mp))
				continue;
			if (mp == ill->ill_frag_timer_mp) {
				/*
				 * The frag timer.  If mi_timer_valid says it
				 * hasn't been reset since it was queued, call
				 * ill_frag_timeout to do garbage collection.
				 * ill_frag_timeout returns 'true' if there are
				 * still fragments left on the queue, in which
				 * case we restart the timer.
				 */
				if ((ill->ill_frag_timer_running =
				    ill_frag_timeout(ill, ip_g_frag_timeout)))
					mi_timer(q, mp,
					    ip_g_frag_timo_ms >> 1);
				continue;
			}
			if (mp == ip_timer_mp) {
				/* It's the IRE expiration timer. */
				become_exclusive(q, mp, (pfi_t)ip_trash);
				continue;
			}
			if (mp == igmp_timer_mp) {
				/* It's the IGMP timer. */
				igmp_timeout();
				continue;
			}
		}
		/* Intramachine packet forwarding. */
		putnext(q, mp);
	}
	TRACE_1(TR_FAC_IP, TR_IP_RSRV_END,
	    "ip_rsrv_end: q %p", q);
}

/*
 * IP & ICMP info in 5 msg's ...
 *  - ip fixed part (mib2_ip_t)
 *  - icmp fixed part (mib2_icmp_t)
 *  - ipAddrEntryTable (ip 20)		our ip_ipif_status
 *  - ipRouteEntryTable (ip 21)		our ip_ire_status for all IREs
 *  - ipNetToMediaEntryTable (ip 22)	ip 21 with different info?
 * ip_21 and ip_22 are augmented in arp to include arp cache entries not
 * already present.
 * NOTE: original mpctl is copied for msg's 2..5, since its ctl part
 * already filled in by caller.
 */
static int
ip_snmp_get(queue_t *q, mblk_t *mpctl)
{
	mblk_t			*mpdata[2];
	mblk_t			*mp2ctl;
	mblk_t			*mp_tail = NULL;
	struct opthdr		*optp;
	ill_t			*ill;
	ipif_t			*ipif;
	uint			bitval;
	mib2_ipAddrEntry_t	mae;

	if (!mpctl || !(mpdata[0] = mpctl->b_cont) ||
	    !(mp2ctl = copymsg(mpctl)))
		return (0);

	/* fixed length IP structure... */
	optp = (struct opthdr *)&mpctl->b_rptr[sizeof (struct T_optmgmt_ack)];
	optp->level = MIB2_IP;
	optp->name = 0;
	SET_MIB(ip_mib.ipForwarding,
	    (WE_ARE_FORWARDING ? 1 : 2));
	SET_MIB(ip_mib.ipDefaultTTL,
	    ip_def_ttl);
	SET_MIB(ip_mib.ipReasmTimeout,
	    ip_g_frag_timeout);
	SET_MIB(ip_mib.ipAddrEntrySize,
	    sizeof (mib2_ipAddrEntry_t));
	SET_MIB(ip_mib.ipRouteEntrySize,
	    sizeof (mib2_ipRouteEntry_t));
	SET_MIB(ip_mib.ipNetToMediaEntrySize,
	    sizeof (mib2_ipNetToMediaEntry_t));
	if (!snmp_append_data(mpdata[0], (char *)&ip_mib,
	    (int)sizeof (ip_mib))) {
		ip0dbg(("ip_snmp_get: failed %d bytes\n",
		    (int)sizeof (ip_mib)));
	}

	optp->len = (t_uscalar_t)msgdsize(mpdata[0]);
	ip1dbg(("ip_snmp_get: level %d, name %d, len %d\n",
	    (int)optp->level, (int)optp->name, (int)optp->len));
	qreply(q, mpctl);

	/* fixed length ICMP structure... */
	mpctl = mp2ctl;
	mpdata[0] = mpctl->b_cont;
	mp2ctl = copymsg(mpctl);
	optp = (struct opthdr *)&mpctl->b_rptr[sizeof (struct T_optmgmt_ack)];
	optp->level = MIB2_ICMP;
	optp->name = 0;
	if (!snmp_append_data(mpdata[0], (char *)&icmp_mib,
	    (int)sizeof (icmp_mib))) {
		ip0dbg(("ip_snmp_get: failed %d bytes\n",
		    (int)sizeof (icmp_mib)));
	}
	optp->len = (t_uscalar_t)msgdsize(mpdata[0]);
	ip1dbg(("ip_snmp_get: level %d, name %d, len %d\n",
	    (int)optp->level, (int)optp->name, (int)optp->len));
	qreply(q, mpctl);
	if (!mp2ctl)
		return (1);

	/* fixed length IGMP structure... */
	mpctl = mp2ctl;
	mpdata[0] = mpctl->b_cont;
	mp2ctl = copymsg(mpctl);
	optp = (struct opthdr *)&mpctl->b_rptr[sizeof (struct T_optmgmt_ack)];
	optp->level = EXPER_IGMP;
	optp->name = 0;
	if (!snmp_append_data(mpdata[0], (char *)&igmpstat,
	    (int)sizeof (igmpstat))) {
		ip0dbg(("ip_snmp_get: failed %d bytes\n",
		    (int)sizeof (igmpstat)));
	}
	optp->len = (t_uscalar_t)msgdsize(mpdata[0]);
	ip1dbg(("ip_snmp_get: level %d, name %d, len %d\n",
	    (int)optp->level, (int)optp->name, (int)optp->len));
	qreply(q, mpctl);
	if (!mp2ctl)
		return (1);

	/* fixed length multicast routing statistics structure... */
	mpctl = mp2ctl;
	mpdata[0] = mpctl->b_cont;
	mp2ctl = copymsg(mpctl);
	optp = (struct opthdr *)&mpctl->b_rptr[sizeof (struct T_optmgmt_ack)];
	if (!ip_mroute_stats(optp, mpdata[0])) {
		ip0dbg(("ip_mroute_stats: failed\n"));
	}
	optp->len = (t_uscalar_t)msgdsize(mpdata[0]);
	ip1dbg(("ip_snmp_get: level %d, name %d, len %d\n",
	    (int)optp->level, (int)optp->name, (int)optp->len));
	qreply(q, mpctl);
	if (!mp2ctl)
		return (1);

	/* ipAddrEntryTable */
	mpctl = mp2ctl;
	mpdata[0] = mpctl->b_cont;
	mp2ctl = copymsg(mpctl);
	optp = (struct opthdr *)&mpctl->b_rptr[sizeof (struct T_optmgmt_ack)];
	optp->level = MIB2_IP;
	optp->name = MIB2_IP_20;
	for (ill = ill_g_head; ill; ill = ill->ill_next) {
		for (ipif = ill->ill_ipif; ipif; ipif = ipif->ipif_next) {
			(void) ipif_get_name(ipif, mae.ipAdEntIfIndex.o_bytes,
			    OCTET_LENGTH);
			mae.ipAdEntIfIndex.o_length =
			    (int)mi_strlen(mae.ipAdEntIfIndex.o_bytes);
			mae.ipAdEntAddr = ipif->ipif_local_addr;
			mae.ipAdEntNetMask = ipif->ipif_net_mask;
			for (bitval = 1;
			    bitval && !(bitval & ipif->ipif_broadcast_addr);
			    bitval <<= 1)
				noop;
			mae.ipAdEntBcastAddr = bitval;
			mae.ipAdEntReasmMaxSize = 65535;
			mae.ipAdEntInfo.ae_mtu = ipif->ipif_mtu;
			mae.ipAdEntInfo.ae_metric  = ipif->ipif_metric;
			mae.ipAdEntInfo.ae_broadcast_addr =
			    ipif->ipif_broadcast_addr;
			mae.ipAdEntInfo.ae_pp_dst_addr = ipif->ipif_pp_dst_addr;
			mae.ipAdEntInfo.ae_flags = ipif->ipif_flags;
			if (!snmp_append_data2(mpdata[0], &mp_tail,
			    (char *)&mae, (int)sizeof (mib2_ipAddrEntry_t))) {
				ip0dbg(("ip_snmp_get: failed %d bytes\n",
				    (int)sizeof (mib2_ipAddrEntry_t)));
			}
		}
	}
	optp->len = (t_uscalar_t)msgdsize(mpdata[0]);
	ip1dbg(("ip_snmp_get: level %d, name %d, len %d\n",
	    (int)optp->level, (int)optp->name, (int)optp->len));
	qreply(q, mpctl);
	if (!mp2ctl)
		return (1);

	/* ipGroupMember table */
	mpctl = mp2ctl;
	mpdata[0] = mpctl->b_cont;
	mp2ctl = copymsg(mpctl);
	optp = (struct opthdr *)&mpctl->b_rptr[sizeof (struct T_optmgmt_ack)];
	optp->level = MIB2_IP;
	optp->name = EXPER_IP_GROUP_MEMBERSHIP;
	mp_tail = NULL;
	for (ill = ill_g_head; ill; ill = ill->ill_next) {
		ip_member_t	ipm;
		ilm_t		*ilm;

		for (ipif = ill->ill_ipif; ipif; ipif = ipif->ipif_next) {
			(void) ipif_get_name(ipif,
			    ipm.ipGroupMemberIfIndex.o_bytes, OCTET_LENGTH);
			ipm.ipGroupMemberIfIndex.o_length =
			    (int)mi_strlen(ipm.ipGroupMemberIfIndex.o_bytes);
			for (ilm = ill->ill_ilm; ilm; ilm = ilm->ilm_next) {
				if (ilm->ilm_ipif != ipif)
					continue;
				ipm.ipGroupMemberAddress = ilm->ilm_addr;
				ipm.ipGroupMemberRefCnt = ilm->ilm_refcnt;
				if (!snmp_append_data2(mpdata[0], &mp_tail,
				    (char *)&ipm, (int)sizeof (ipm))) {
					ip0dbg((
					    "ip_snmp_get: failed %d bytes\n",
					    (int)sizeof (ipm)));
				}
			}
		}
	}
	optp->len = (t_uscalar_t)msgdsize(mpdata[0]);
	ip1dbg(("ip_snmp_get: level %d, name %d, len %d\n",
	    (int)optp->level, (int)optp->name, (int)optp->len));
	qreply(q, mpctl);
	if (!mp2ctl)
		return (1);

	/* Create the multicast virtual interface table... */
	mpctl = mp2ctl;
	mpdata[0] = mpctl->b_cont;
	mp2ctl = copymsg(mpctl);
	optp = (struct opthdr *)&mpctl->b_rptr[sizeof (struct T_optmgmt_ack)];
	if (!ip_mroute_vif(optp, mpdata[0])) {
		ip0dbg(("ip_mroute_vif: failed\n"));
	}
	optp->len = (t_uscalar_t)msgdsize(mpdata[0]);
	ip1dbg(("ip_snmp_get: level %d, name %d, len %d\n",
	    (int)optp->level, (int)optp->name, (int)optp->len));
	qreply(q, mpctl);
	if (!mp2ctl)
		return (1);

	/* Create the multicast routing table... */
	mpctl = mp2ctl;
	mpdata[0] = mpctl->b_cont;
	mp2ctl = copymsg(mpctl);
	optp = (struct opthdr *)&mpctl->b_rptr[sizeof (struct T_optmgmt_ack)];
	if (!ip_mroute_mrt(optp, mpdata[0])) {
		ip0dbg(("ip_mroute_mrt: failed\n"));
	}
	optp->len = (t_uscalar_t)msgdsize(mpdata[0]);
	ip1dbg(("ip_snmp_get: level %d, name %d, len %d\n",
	    (int)optp->level, (int)optp->name, (int)optp->len));
	qreply(q, mpctl);
	if (!mp2ctl)
		return (1);

	/*
	 * create both ipRouteEntryTable (mpctl) &
	 * ipNetToMediaEntryTable (mp2ctl) in one IRE walk
	 */
	mpctl = mp2ctl;
	mpdata[0] = mpctl->b_cont;
	mp2ctl = copymsg(mpctl);
	if (!mp2ctl) {
		freemsg(mpctl);
		return (1);
	}
	mpdata[1] = mp2ctl->b_cont;
	ire_walk(ip_snmp_get2, (char *)mpdata);

	/* ipRouteEntryTable */
	optp = (struct opthdr *)&mpctl->b_rptr[sizeof (struct T_optmgmt_ack)];
	optp->level = MIB2_IP;
	optp->name = MIB2_IP_21;
	optp->len = (t_uscalar_t)msgdsize(mpdata[0]);
	ip1dbg(("ip_snmp_get: level %d, name %d, len %d\n",
	    (int)optp->level, (int)optp->name, (int)optp->len));
	qreply(q, mpctl);

	/* ipNetToMediaEntryTable */
	optp = (struct opthdr *)&mp2ctl->b_rptr[sizeof (struct T_optmgmt_ack)];
	optp->level = MIB2_IP;
	optp->name = MIB2_IP_22;
	optp->len = (t_uscalar_t)msgdsize(mpdata[1]);
	ip1dbg(("ip_snmp_get: level %d, name %d, len %d\n",
	    (int)optp->level, (int)optp->name, (int)optp->len));
	qreply(q, mp2ctl);
	return (1);
}

/*
 * ire_walk routine to create both ipRouteEntryTable &
 * ipNetToMediaEntryTable in one IRE walk
 */
static void
ip_snmp_get2(ire_t *ire, mblk_t *mpdata[])
{
	ill_t				*ill;
	ipif_t				*ipif;
	mblk_t				*llmp;
	dl_unitdata_req_t		*dlup;
	mib2_ipRouteEntry_t		re;
	mib2_ipNetToMediaEntry_t	ntme;
	/*
	 * Return all IRE types for route table... let caller pick and choose
	 */
	re.ipRouteDest = ire->ire_addr;
	ipif = ire->ire_ipif;
	re.ipRouteIfIndex.o_length = 0;
	if (ipif) {
		char buf[32];
		char *name;

		name = ipif_get_name(ipif, buf, (int)sizeof (buf));
		re.ipRouteIfIndex.o_length = (int)strlen(name) + 1;
		bcopy(name, re.ipRouteIfIndex.o_bytes,
		    re.ipRouteIfIndex.o_length);
	}
	re.ipRouteMetric1 = -1;
	re.ipRouteMetric2 = -1;
	re.ipRouteMetric3 = -1;
	re.ipRouteMetric4 = -1;
	if (ire->ire_type & (IRE_INTERFACE|IRE_LOOPBACK|IRE_BROADCAST))
		re.ipRouteNextHop = ire->ire_src_addr;
	else
		re.ipRouteNextHop = ire->ire_gateway_addr;
	/* indirect(4) or direct(3) */
	re.ipRouteType = ire->ire_gateway_addr ? 4 : 3;
	re.ipRouteProto = -1;
	re.ipRouteAge = hrestime.tv_sec - ire->ire_create_time;
	re.ipRouteMask = ire->ire_mask;
	re.ipRouteMetric5 = -1;
	re.ipRouteInfo.re_max_frag  = ire->ire_max_frag;
	re.ipRouteInfo.re_frag_flag = ire->ire_frag_flag;
	re.ipRouteInfo.re_rtt	    = ire->ire_rtt;
	if (ire->ire_ll_hdr_length)
		llmp = ire->ire_ll_hdr_saved_mp;
	else
		llmp = ire->ire_ll_hdr_mp;
	re.ipRouteInfo.re_ref	    = llmp ? llmp->b_datap->db_ref : 0;
	re.ipRouteInfo.re_src_addr  = ire->ire_src_addr;
	re.ipRouteInfo.re_ire_type  = ire->ire_type;
	re.ipRouteInfo.re_obpkt	    = ire->ire_ob_pkt_count;
	re.ipRouteInfo.re_ibpkt	    = ire->ire_ib_pkt_count;
	if (!snmp_append_data(mpdata[0], (char *)&re, (int)sizeof (re))) {
		ip0dbg(("ip_snmp_get2: failed %d bytes\n", (int)sizeof (re)));
	}

	if (ire->ire_type != IRE_CACHE || ire->ire_gateway_addr)
		return;
	/*
	 * only IRE_CACHE entries that are for a directly connected subnet
	 * get appended to net -> phys addr table
	 * (others in arp)
	 */
	ntme.ipNetToMediaIfIndex.o_length = 0;
	ill = ire_to_ill(ire);
	if (ill) {
		ntme.ipNetToMediaIfIndex.o_length = ill->ill_name_length;
		bcopy(ill->ill_name, ntme.ipNetToMediaIfIndex.o_bytes,
		    ntme.ipNetToMediaIfIndex.o_length);
	}
	ntme.ipNetToMediaPhysAddress.o_length = 0;
	if (llmp) {
		u_char *addr;

		dlup = (dl_unitdata_req_t *)llmp->b_rptr;
		/* Remove sap from  address */
		if (ill->ill_sap_length < 0)
			addr = llmp->b_rptr + dlup->dl_dest_addr_offset;
		else
			addr = llmp->b_rptr + dlup->dl_dest_addr_offset +
			    ill->ill_sap_length;

		ntme.ipNetToMediaPhysAddress.o_length =
		    MIN(OCTET_LENGTH, ill->ill_phys_addr_length);
		bcopy(addr, ntme.ipNetToMediaPhysAddress.o_bytes,
		    ntme.ipNetToMediaPhysAddress.o_length);
	}
	ntme.ipNetToMediaNetAddress = ire->ire_addr;
	/* assume dynamic (may be changed in arp) */
	ntme.ipNetToMediaType = 3;
	ntme.ipNetToMediaInfo.ntm_mask.o_length = sizeof (uint32_t);
	bcopy(&ire->ire_mask, ntme.ipNetToMediaInfo.ntm_mask.o_bytes,
	    ntme.ipNetToMediaInfo.ntm_mask.o_length);
	ntme.ipNetToMediaInfo.ntm_flags = ACE_F_RESOLVED;
	if (!snmp_append_data(mpdata[1], (char *)&ntme, (int)sizeof (ntme))) {
		ip0dbg(("ip_snmp_get2: failed %d bytes\n", (int)sizeof (ntme)));
	}
}

/*
 * return (0) if invalid set request, 1 otherwise, including non-tcp requests
 */

/* ARGSUSED */
static int
ip_snmp_set(queue_t *q, int level, int name, u_char *ptr, int len)
{
	switch (level) {
	case MIB2_IP:
	case MIB2_ICMP:
		switch (name) {
		default:
			break;
		}
		return (1);
	default:
		return (1);
	}
}

/*
 * Called before the options are updated to check if this packet will
 * be source routed from here.
 * This routine assumes that the options are well formed i.e. that they
 * have already been checked.
 */
static boolean_t
ip_source_routed(ipha_t *ipha)
{
	uint32_t	totallen;
	u_char		*opt;
	uint32_t	optval;
	uint32_t	optlen;
	ipaddr_t 	dst;

	ip2dbg(("ip_source_routed\n"));
	totallen = ipha->ipha_version_and_hdr_length -
	    (uint8_t)((IP_VERSION << 4) + IP_SIMPLE_HDR_LENGTH_IN_WORDS);
	if (totallen == 0)
		return (false);
	dst = ipha->ipha_dst;
	opt = (u_char *)&ipha[1];
	totallen <<= 2;
	while (totallen != 0) {
		switch (optval = opt[IPOPT_POS_VAL]) {
		case IPOPT_EOL:
			return (false);
		case IPOPT_NOP:
			optlen = 1;
			break;
		default:
			optlen = opt[IPOPT_POS_LEN];
		}
		ip2dbg(("ip_source_routed: opt %d, len %d\n",
		    optval, optlen));

		if (optlen == 0 || optlen > totallen)
			break;

		switch (optval) {
			uint32_t off;
		case IPOPT_SSRR:
		case IPOPT_LSRR:
			/*
			 * If dst is one of our addresses and there are some
			 * entries left in the source route return (true).
			 */
			if (!ire_ctable_lookup(dst, 0, IRE_LOCAL, NULL, NULL,
			    MATCH_IRE_TYPE)) {
				ip2dbg(("ip_source_routed: not next"
				    " source route 0x%x\n",
				    ntohl(dst)));
				return (false);
			}
			off = opt[IPOPT_POS_OFF];
			off--;
			if (optlen < IP_ADDR_LEN ||
			    off > optlen - IP_ADDR_LEN) {
				/* End of source route */
				ip1dbg(("ip_source_routed: end of SR\n"));
				return (false);
			}
			return (true);
		}
		totallen -= optlen;
		opt += optlen;
	}
	return (false);
}

/*
 * Check if the packet contains any source route.
 * This routine assumes that the options are well formed i.e. that they
 * have already been checked.
 */
static boolean_t
ip_source_route_included(ipha_t *ipha)
{
	uint32_t	totallen;
	u_char		*opt;
	uint32_t	optval;
	uint32_t	optlen;

	totallen = ipha->ipha_version_and_hdr_length -
	    (uint8_t)((IP_VERSION << 4) + IP_SIMPLE_HDR_LENGTH_IN_WORDS);
	if (totallen == 0)
		return (false);
	opt = (u_char *)&ipha[1];
	totallen <<= 2;
	while (totallen != 0) {
		switch (optval = opt[IPOPT_POS_VAL]) {
		case IPOPT_EOL:
			return (false);
		case IPOPT_NOP:
			optlen = 1;
			break;
		default:
			optlen = opt[IPOPT_POS_LEN];
		}

		if (optlen == 0 || optlen > totallen)
			break;

		switch (optval) {
		case IPOPT_SSRR:
		case IPOPT_LSRR:
			return (true);
		}
		totallen -= optlen;
		opt += optlen;
	}
	return (false);
}

/* Called as writer from ip_rsrv when the IRE expiration timer fires. */
/* ARGSUSED */
static void
ip_trash(queue_t *q, mblk_t *mp)
{
	int	flush_flag = 0;

	if (ip_ire_time_elapsed >= ip_ire_flush_interval) {
		/* Nuke 'em all this time.  We're compliant kind of guys. */
		flush_flag |= FLUSH_CACHE_TIME;
		ip_ire_time_elapsed = 0;
	}
	if (ip_ire_rd_time_elapsed >= ip_ire_redir_interval) {
		/* Nuke 'em all this time.  We're compliant kind of guys. */
		flush_flag |= FLUSH_REDIRECT_TIME;
		ip_ire_rd_time_elapsed = 0;
	}
	if (ip_ire_pmtu_time_elapsed >= ip_ire_pathmtu_interval) {
		/* Increase path mtu */
		flush_flag |= FLUSH_MTU_TIME;
		ip_ire_pmtu_time_elapsed = 0;
	}
	/*
	 * Walk all IRE's and nuke the ones that haven't been used since the
	 * previous interval.
	 */
	ire_walk(ire_expire, (char *)flush_flag);
	if (ip_timer_ill) {
		ip_ire_time_elapsed += ip_timer_interval;
		ip_ire_rd_time_elapsed += ip_timer_interval;
		ip_ire_pmtu_time_elapsed += ip_timer_interval;
		mi_timer(ip_timer_ill->ill_rq, ip_timer_mp, ip_timer_interval);
	}
}

/*
 * ip_unbind is called when a copy of an unbind request is received from the
 * upper level protocol.  We remove this ipc from any fanout hash list it is
 * on, and zero out the bind information.  No reply is expected up above.
 */
static void
ip_unbind(queue_t *q, mblk_t *mp)
{
	ipc_t	*ipc = (ipc_t *)q->q_ptr;

	ipc_hash_remove(ipc);
	bzero(&ipc->ipc_ipcu, sizeof (ipc->ipc_ipcu));

	/* Send a T_OK_ACK to the user */
	if ((mp = mi_tpi_ok_ack_alloc(mp)) != NULL)
		qreply(q, mp);
}

/*
 * Write side put procedure.  Outbound data, IOCTLs, responses from
 * resolvers, etc, come down through here.
 */
void
ip_wput(queue_t *q, mblk_t *mp)
{
	ipha_t	*ipha;
#define	rptr	((u_char *)ipha)
	ire_t	*ire;
	ipc_t	*ipc;
	uint32_t	v_hlen_tos_len;
	ipaddr_t	dst;

#ifdef	_BIG_ENDIAN
#define	V_HLEN	(v_hlen_tos_len >> 24)
#else
#define	V_HLEN	(v_hlen_tos_len & 0xFF)
#endif

	TRACE_1(TR_FAC_IP, TR_IP_WPUT_START,
	    "ip_wput_start: q %p", q);

	/*
	 * ip_wput fast path
	 */

	/* is packet from ARP ? */
	if (q->q_next) goto qnext;
	ipc = (ipc_t *)q->q_ptr;

	/* is queue flow controlled? */
	if (q->q_first && !ipc->ipc_draining) goto doputq;

	/* mblock type is not DATA */
	if (mp->b_datap->db_type != M_DATA) goto notdata;
	ipha = (ipha_t *)mp->b_rptr;

	/* is IP header non-aligned or mblock smaller than basic IP header */
#ifndef SAFETY_BEFORE_SPEED
	if (!OK_32PTR(rptr) ||
	    (mp->b_wptr - rptr) < IP_SIMPLE_HDR_LENGTH) goto hdrtoosmall;
#endif
	v_hlen_tos_len = ((uint32_t *)ipha)[0];

	/* is wrong version or IP options present */
	if (V_HLEN != IP_SIMPLE_HDR_VERSION) goto version_hdrlen_check;
	dst = ipha->ipha_dst;

	/* is packet multicast? */
	if (CLASSD(dst)) goto multicast;

	/* bypass routing checks and go directly to interface */
	if (ipc->ipc_dontroute) goto dontroute;

	ire = (ire_t *)ire_cache_lookup(dst);
	if (!ire)
		goto noirefound;

	TRACE_2(TR_FAC_IP, TR_IP_WPUT_END,
	    "ip_wput_end: q %p (%S)", q, "end");

	ip_wput_ire(q, mp, ire);
	return;

doputq:
	(void) putq(q, mp);
	return;

qnext:
	ipc = nilp(ipc_t);

	/*
	 * Upper Level Protocols pass down complete IP datagrams
	 * as M_DATA messages.	Everything else is a sideshow.
	 */
	if (mp->b_datap->db_type != M_DATA) {
	    notdata:
		ip_wput_nondata(q, mp);
		TRACE_2(TR_FAC_IP, TR_IP_WPUT_END,
		    "ip_wput_end: q %p (%S)", q, "nondata");
		return;
	}
	/* We have a complete IP datagram heading outbound. */
	ipha = (ipha_t *)mp->b_rptr;

#ifndef SPEED_BEFORE_SAFETY
	/*
	 * Make sure we have a full-word aligned message and that at least
	 * a simple IP header is accessible in the first message.  If not,
	 * try a pullup.
	 */
	if (!OK_32PTR(rptr) ||
	    (mp->b_wptr - rptr) < IP_SIMPLE_HDR_LENGTH) {
	    hdrtoosmall:
		if (!pullupmsg(mp, IP_SIMPLE_HDR_LENGTH)) {
			BUMP_MIB(ip_mib.ipOutDiscards);
			freemsg(mp);
			TRACE_2(TR_FAC_IP, TR_IP_WPUT_END,
			    "ip_wput_end: q %p (%S)", q, "pullupfailed");
			return;
		}
		ipha = (ipha_t *)mp->b_rptr;
	}
#endif

	/* Most of the code below is written for speed, not readability */
	v_hlen_tos_len = ((uint32_t *)ipha)[0];

	/*
	 * If ip_newroute() fails, we're going to need a full
	 * header for the icmp wraparound.
	 */
	if (V_HLEN != IP_SIMPLE_HDR_VERSION) {
		u_int	v_hlen;
	    version_hdrlen_check:
		v_hlen = V_HLEN;
		if ((v_hlen >> 4) != IP_VERSION) {
			BUMP_MIB(ip_mib.ipOutDiscards);
			freemsg(mp);
			TRACE_2(TR_FAC_IP, TR_IP_WPUT_END,
			    "ip_wput_end: q %p (%S)", q, "badvers");
			return;
		}
		/*
		 * Is the header length at least 20 bytes?
		 *
		 * Are there enough bytes accessible in the header?  If
		 * not, try a pullup.
		 */
		v_hlen &= 0xF;
		v_hlen <<= 2;
		if (v_hlen < IP_SIMPLE_HDR_LENGTH) {
			BUMP_MIB(ip_mib.ipOutDiscards);
			freemsg(mp);
			TRACE_2(TR_FAC_IP, TR_IP_WPUT_END,
			    "ip_wput_end: q %p (%S)", q, "badlen");
			return;
		}
		if (v_hlen > (mp->b_wptr - rptr)) {
			if (!pullupmsg(mp, v_hlen)) {
				BUMP_MIB(ip_mib.ipOutDiscards);
				freemsg(mp);
				TRACE_2(TR_FAC_IP, TR_IP_WPUT_END,
				    "ip_wput_end: q %p (%S)", q, "badpullup2");
				return;
			}
			ipha = (ipha_t *)mp->b_rptr;
		}
		/*
		 * Move first entry from any source route into ipha_dst and
		 * verify the options
		 */
		if (ip_wput_options(q, mp, ipha)) {
			TRACE_2(TR_FAC_IP, TR_IP_WPUT_END,
			    "ip_wput_end: q %p (%S)", q, "badopts");
			return;
		}
	}
	dst = ipha->ipha_dst;

	/*
	 * Try to get an IRE_CACHE for the destination address.	 If we can't,
	 * we have to run the packet through ip_newroute which will take
	 * the appropriate action to arrange for an IRE_CACHE, such as querying
	 * a resolver, or assigning a default gateway, etc.
	 */
	/* Guard against coming in from arp in which case ipc is nil. */
	if (CLASSD(dst) && ipc) {
		ipif_t	*ipif;

	    multicast:
		ip2dbg(("ip_wput: CLASSD\n"));
		ipif = ipc->ipc_multicast_ipif;
		if (!ipif) {
			/*
			 * We must do this check here else we could pass
			 * through ip_newroute and come back here without the
			 * ill context (we would have a lower ill).
			 *
			 * Note: we do late binding i.e. we bind to
			 * the interface when the first packet is sent.
			 * For performance reasons we do not rebind on
			 * each packet.
			 */
			ipif = ipif_lookup_group(htonl(INADDR_UNSPEC_GROUP));
			if (ipif == nilp(ipif_t)) {
				ip1dbg(("ip_wput: No ipif for multicast\n"));
				BUMP_MIB(ip_mib.ipOutNoRoutes);
				goto drop_pkt;
			}
			/* Bind until the next IP_MULTICAST_IF option */
			ipc->ipc_multicast_ipif = ipif;
		}
#ifdef IP_DEBUG
		else {
			ip2dbg(("ip_wput: multicast_ipif set: addr 0x%x\n",
			    ntohl(ipif->ipif_local_addr)));
		}
#endif
		ipha->ipha_ttl = (uint8_t)ipc->ipc_multicast_ttl;
		ip2dbg(("ip_wput: multicast ttl %d\n", ipha->ipha_ttl));
		/*
		 * Find an IRE which matches the destination and the outgoing
		 * queue (i.e. the outgoing interface.)
		 */
		if (ipif->ipif_flags & IFF_POINTOPOINT)
			dst = ipif->ipif_pp_dst_addr;
		ire = ire_ctable_lookup(dst, 0, 0, ipif, NULL, MATCH_IRE_IPIF);
		if (!ire) {
			/*
			 * Have to check local loopback etc at this
			 * point since when we come back through
			 * ire_add_then_put we no longer have the ipc
			 * so we can't check ipc_loopback.
			 */
			ill_t *ill = ipif->ipif_ill;

			/* Set the source address for loopback */
			ipha->ipha_src = ipif->ipif_local_addr;

			/*
			 * Note that the loopback function will not come
			 * in through ip_rput - it will only do the
			 * client fanout thus we need to do an mforward
			 * as well.  The is different from the BSD
			 * logic.
			 */
			if (ipc->ipc_multicast_loop &&
			    ilm_lookup(ill, ipha->ipha_dst)) {
				/* Pass along the virtual output q. */
				ip_multicast_loopback(ill->ill_rq, mp);
			}
			if (ipha->ipha_ttl == 0) {
				/*
				 * 0 => only to this host i.e. we are
				 * done.
				 */
				freemsg(mp);
				TRACE_2(TR_FAC_IP, TR_IP_WPUT_END,
				    "ip_wput_end: q %p (%S)",
				    q, "loopback");
				return;
			}
			/*
			 * IFF_MULTICAST is checked in ip_newroute
			 * i.e. we don't need to check it here since
			 * all IRE_CACHEs come from ip_newroute.
			 */
			/*
			 * For multicast traffic, SO_DONTROUTE is interpreted to
			 * mean only send the packet out the interface
			 * (optionally specified with IP_MULTICAST_IF)
			 * and do not forward it out additional interfaces.
			 * RSVP and the rsvp daemon is an example of a
			 * protocol and user level process that
			 * handles it's own routing. Hence, it uses the
			 * SO_DONTROUTE option to accomplish this.
			 */
			if (ip_g_mrouter && !ipc->ipc_dontroute) {
				/*
				 * The checksum has not been
				 * computed yet
				 */
				ipha->ipha_hdr_checksum = 0;
				ipha->ipha_hdr_checksum =
				    (uint16_t)ip_csum_hdr(ipha);

				if (ip_mforward(ill, ipha, mp)) {
					freemsg(mp);
					ip1dbg(("ip_wput: mforward failed\n"));
					TRACE_2(TR_FAC_IP,
					    TR_IP_WPUT_END,
					    "ip_wput_end: q %p (%S)",
					    q, "mforward failed");
					return;
				}
			}
			/*
			 * Mark this packet to make it be delivered to
			 * ip_wput_ire after the new ire has been
			 * created.
			 */
			mp->b_prev = nilp(mblk_t);
			mp->b_next = nilp(mblk_t);
			ip_newroute_ipif(q, mp, ipif, dst);
			TRACE_2(TR_FAC_IP, TR_IP_WPUT_END,
			    "ip_wput_end: q %p (%S)", q, "noire");
			return;
		}
	} else {
		/*
		 * Guard against coming in from arp in which case ipc is
		 * nil.
		 */
		if (ipc && ipc->ipc_dontroute) {
		    dontroute:
			/*
			 * Set TTL to 1 if SO_DONTROUTE is set to prevent
			 * routing protocols from seeing false direct
			 * connectivity.
			 */
			ipha->ipha_ttl = 1;
		}
		ire = ire_cache_lookup(dst);
		if (!ire) {
noirefound:
			/*
			 * Mark this packet as having originated on
			 * this machine.  This will be noted in
			 * ire_add_then_put, which needs to know
			 * whether to run it back through ip_wput or
			 * ip_rput following successful resolution.
			 */
			mp->b_prev = nilp(mblk_t);
			mp->b_next = nilp(mblk_t);
			ip_newroute(q, mp, dst);
			TRACE_2(TR_FAC_IP, TR_IP_WPUT_END,
			    "ip_wput_end: q %p (%S)", q, "newroute");
			return;
		}
	}
	/* We now know where we are going with it. */

	TRACE_2(TR_FAC_IP, TR_IP_WPUT_END,
	    "ip_wput_end: q %p (%S)", q, "end");
	ip_wput_ire(q, mp, ire);
	return;

drop_pkt:
	ip1dbg(("ip_wput: dropped packet\n"));
	freemsg(mp);
	TRACE_2(TR_FAC_IP, TR_IP_WPUT_END,
	    "ip_wput_end: q %p (%S)", q, "droppkt");
}

/* (May be called as writer.) */
void
ip_wput_ire(queue_t *q, mblk_t *mp, ire_t *ire)
{
	ipha_t	*ipha;
#define	rptr	((u_char *)ipha)
	mblk_t	*mp1;
	queue_t	*stq;
	ipc_t	*ipc;
	uint32_t	v_hlen_tos_len;
	uint32_t	ttl_protocol;
	ipaddr_t	src;
	ipaddr_t	dst;
	ipaddr_t	orig_src;
	ire_t	*ire1;
	mblk_t	*next_mp;
	u_int	hlen;
	uint16_t	*up;
	uint32_t	max_frag;
	ill_t	*ill = ire_to_ill(ire);

	int	no_tp_cksum;	/* Perform transport level checksum? */

	TRACE_1(TR_FAC_IP, TR_IP_WPUT_IRE_START,
	    "ip_wput_ire_start: q %p", q);
	/*
	 * Fast path for ip_wput_ire
	 */

	ipha = (ipha_t *)mp->b_rptr;
	v_hlen_tos_len = ((uint32_t *)ipha)[0];
	dst = ipha->ipha_dst;

	/*
	 * ICMP(RAWIP) module should set the ipha_ident to NO_IP_TP_CKSUM
	 * if the socket is a SOCK_RAW type. The transport checksum should
	 * be provided in the pre-built packet, so we don't need to compute it.
	 * Other transport MUST pass down zero.
	 */
	no_tp_cksum = ipha->ipha_ident;
	ASSERT(ipha->ipha_ident == 0 || ipha->ipha_ident == NO_IP_TP_CKSUM);

#ifdef IP_DEBUG
	if (CLASSD(dst)) {
		ip1dbg(("ip_wput_ire: to 0x%x ire %s addr 0x%x\n",
		    ntohl(dst),
		    ip_nv_lookup(ire_nv_tbl, ire->ire_type),
		    ntohl(ire->ire_addr)));
	}
#endif

/* Macros to extract header fields from data already in registers */
#ifdef	_BIG_ENDIAN
#define	V_HLEN	(v_hlen_tos_len >> 24)
#define	LENGTH	(v_hlen_tos_len & 0xFFFF)
#define	PROTO	(ttl_protocol & 0xFF)
#else
#define	V_HLEN	(v_hlen_tos_len & 0xFF)
#define	LENGTH	((v_hlen_tos_len >> 24) | ((v_hlen_tos_len >> 8) & 0xFF00))
#define	PROTO	(ttl_protocol >> 8)
#endif


	orig_src = src = ipha->ipha_src;
	/* (The loop back to "another" is explained down below.) */
another:;
	/*
	 * Assign an ident value for this packet.  We assign idents on a
	 * per destination basis out of the IRE.  There could be other threads
	 * targeting the same destination, so we have to arrange for a atomic
	 * increment, using whatever mechanism is most efficient for a
	 * particular machine, therefore ATOMIC_32_INC should be defined in
	 * an environment specific header.
	 *
	 * Note: use ttl_protocol as a temporary
	 */
	mutex_enter(&ire->ire_ident_lock);
	ATOMIC_32_INC(ttl_protocol, &ire->ire_atomic_ident);
	mutex_exit(&ire->ire_ident_lock);
#ifndef _BIG_ENDIAN
	ttl_protocol = (ttl_protocol << 8) | ((ttl_protocol >> 8) & 0xff);
#endif
	ipha->ipha_ident = (uint16_t)ttl_protocol;

	if (!src) {
		/*
		 * Assign the appropriate source address from the IRE
		 * if none was specified.
		 */
		src = ire->ire_src_addr;
		ipha->ipha_src = src;
	}
	stq = ire->ire_stq;

	ire1 = ire->ire_next;
	/*
	 * We only allow ire chains for broadcasts since there will
	 * be multiple IRE_CACHE entries for the same multicast
	 * address (one per ipif).
	 */
	next_mp = nilp(mblk_t);

	/* broadcast packet */
	if (ire->ire_type == IRE_BROADCAST) goto broadcast;

	/* loopback ? */
	if (!stq) goto nullstq;

	BUMP_MIB(ip_mib.ipOutRequests);
	ttl_protocol = ((uint16_t *)ipha)[4];

	/* pseudo checksum (do it in parts for IP header checksum) */
	dst = (dst >> 16) + (dst & 0xFFFF);
	src = (src >> 16) + (src & 0xFFFF);
	dst += src;

#define	IPH_UDPH_CHECKSUMP(ipha, hlen) \
	((uint16_t *)(((u_char *)ipha)+(hlen + 6)))
#define	IPH_TCPH_CHECKSUMP(ipha, hlen) \
	    ((uint16_t *)(((u_char *)ipha)+(hlen+TCP_CHECKSUM_OFFSET)))

	if (PROTO != IPPROTO_TCP) {
		queue_t *dev_q = stq->q_next;
		/* flow controlled */
		if ((dev_q->q_next || dev_q->q_first) &&
		    !canput(dev_q)) goto blocked;
		if (PROTO == IPPROTO_UDP && !no_tp_cksum) {
			hlen = (V_HLEN & 0xF) << 2;
			up = IPH_UDPH_CHECKSUMP(ipha, hlen);
			if (*up) {
				u_int   u1;
				u1 = IP_CSUM(mp, hlen,
				    dst+IP_UDP_CSUM_COMP);
				*up = (uint16_t)(u1 ? u1 : ~u1);
			}
		}
	} else if (!no_tp_cksum) {
		hlen = (V_HLEN & 0xF) << 2;
		up = IPH_TCPH_CHECKSUMP(ipha, hlen);
		if (ill && ill->ill_ick.ick_magic == ICK_M_CTL_MAGIC &&
		    dohwcksum && ill->ill_ick.ick_xmit == 0 &&
		    ire->ire_ll_hdr_length) {
			/*
			 * Underlying interface supports inetcksuming and
			 * M_DATA fast path, so postpone the cksum to the
			 * interface driver.
			 * XXX - we only need to set up b_ick_xxx on the
			 * first mblk.
			 */
			uint32_t	sum;
#ifdef ZC_TEST
			zckstat->zc_hwcksum_w.value.ul++;
#endif
			sum = *up + dst + IP_TCP_CSUM_COMP;
			sum = (sum & 0xFFFF) + (sum >> 16);
			*up = (sum & 0xFFFF) + (sum >> 16);
			mp->b_ick_start = rptr + hlen;
			mp->b_ick_stuff = (u_char *)up;
			mp->b_ick_end = mp->b_wptr;
			mp->b_ick_flag = ICK_VALID;
		} else {
#ifdef ZC_TEST
			zckstat->zc_swcksum_w.value.ul++;
#endif
			*up = IP_CSUM(mp, hlen, dst + IP_TCP_CSUM_COMP);
			mp->b_ick_flag = ICK_NONE;
			mp->b_ick_stuff = NULL;
		}
	}

	/* multicast packet? */
	if (CLASSD(ipha->ipha_dst) && !q->q_next) goto multi_loopback;

	/* checksum */
	dst += ttl_protocol;

	max_frag = ire->ire_max_frag;
	/* fragment the packet */
	if (max_frag < (unsigned int)LENGTH) goto fragmentit;

	/* checksum */
	dst += ipha->ipha_ident;

	if ((V_HLEN == IP_SIMPLE_HDR_VERSION ||
	    !ip_source_route_included(ipha)) &&
	    !CLASSD(ipha->ipha_dst))
		ipha->ipha_fragment_offset_and_flags |=
		    htons((uint16_t)ire->ire_frag_flag);
	/* checksum */
	dst += (v_hlen_tos_len >> 16)+(v_hlen_tos_len & 0xFFFF);
	dst += ipha->ipha_fragment_offset_and_flags;

	/* IP options present */
	hlen = (V_HLEN & 0xF) - IP_SIMPLE_HDR_LENGTH_IN_WORDS;
	if (hlen) goto checksumoptions;

	/* calculate hdr checksum */
	dst = ((dst & 0xFFFF) + (dst >> 16));
	dst = ~(dst + (dst >> 16));
	{
		ipaddr_t u1 = dst;
		ipha->ipha_hdr_checksum = (uint16_t)u1;
	}

	hlen = ire->ire_ll_hdr_length;
	mp1 = ire->ire_ll_hdr_mp;

	/* if not MDATA fast path or fp hdr doesn'f fit */
	if (!hlen || (rptr - mp->b_datap->db_base) < hlen) goto noprepend;

	ipha = (ipha_t *)(rptr - hlen);
	mp->b_rptr = rptr;
	bcopy(mp1->b_rptr, rptr, hlen);
	ire->ire_ob_pkt_count++;

	TRACE_2(TR_FAC_IP, TR_IP_WPUT_IRE_END,
	    "ip_wput_ire_end: q %p (%S)",
	    q, "last copy out");
	putnext(stq, mp);
	return;

	/*
	 * ire->ire_type == IRE_BROADCAST (minimize diffs)
	 */
broadcast:
	{
		/*
		 * Avoid broadcast storms by setting the ttl to 1
		 * for broadcasts
		 */
		ipha->ipha_ttl = (uint8_t)ip_broadcast_ttl;

		if (ire1 && ire1->ire_addr == dst) {
			/*
			 * IRE chain.  The next IRE has the same
			 * address as the current one thus we might want
			 * to send out multiple copies of this packets.
			 * This is used to send a single copy of a broadcast
			 * packet out all physical interfaces that have an
			 * matching IRE_BROADCAST while also looping
			 * back one copy (to ip_wput_local) for each
			 * matching physical interface. However, we avoid
			 * sending packets out different logical that match.
			 * Note that the check below uses ire_stq to ensure
			 * that there is a loopback copy.
			 *
			 * This feature is currently
			 * used to get broadcasts sent to multiple
			 * interfaces, when the broadcast address
			 * being used applies to multiple interfaces.
			 * For example, a whole net broadcast will be
			 * replicated on every connected subnet of
			 * the target net.
			 *
			 * This logic assumes that ire_add() groups the
			 * IRE_BROADCAST entries so that those with the same
			 * ire_addr are kept together and, within those with
			 * the same address, those with the same
			 * ire_ipif->ipif_ill are grouped together.
			 * This check also assumes that the the ire_stq = NULL
			 * entries (for broadcast loopback) show up
			 * before their corresponding ire_stq != NULL
			 * entries in the ire_next chain.
			 */
			while (ire1 != NULL && ire1->ire_addr == dst &&
			    (ire1->ire_ipif->ipif_ill ==
			    ire->ire_ipif->ipif_ill) &&
			    !(ire->ire_stq == NULL && ire1->ire_stq != NULL))
				ire1 = ire1->ire_next;
			if (ire1 != NULL && ire1->ire_addr == dst)
				next_mp = copymsg(mp);
		}
	}

	if (stq) {
		/*
		 * A non-nil send-to queue means this packet is going
		 * out of this machine.
		 */

		BUMP_MIB(ip_mib.ipOutRequests);
		ttl_protocol = ((uint16_t *)ipha)[4];
		/*
		 * We accumulate the pseudo header checksum in dst.
		 * This is pretty hairy code, so watch close.  One
		 * thing to keep in mind is that UDP and TCP have
		 * stored their respective datagram lengths in their
		 * checksum fields.  This lines things up real nice.
		 */
		dst = (dst >> 16) + (dst & 0xFFFF);
		src = (src >> 16) + (src & 0xFFFF);
		dst += src;
		/*
		 * We assume the udp checksum field contains the
		 * length, so to compute the pseudo header checksum,
		 * all we need is the protocol number and src/dst.
		 */
		/* Provide the checksums for UDP and TCP. */
		if (PROTO == IPPROTO_TCP && !no_tp_cksum) {
			/* hlen gets the number of u_chars in the IP header */
			hlen = (V_HLEN & 0xF) << 2;
			up = IPH_TCPH_CHECKSUMP(ipha, hlen);
			*up = IP_CSUM(mp, hlen, dst + IP_TCP_CSUM_COMP);
		} else {
			queue_t *dev_q = stq->q_next;
			if ((dev_q->q_next || dev_q->q_first) &&
			    !canput(dev_q)) {
			    blocked:
				if (next_mp)
					freemsg(next_mp);
				ipc = q->q_next ? NULL :
				    (ipc_t *)q->q_ptr;
				ipha->ipha_ident = (uint16_t)no_tp_cksum;
				if (ip_output_queue && ipc != NULL) {
					if (ipc->ipc_draining) {
						ipc->ipc_did_putbq = 1;
						(void) putbq(ipc->ipc_wq, mp);
					} else
						(void) putq(ipc->ipc_wq, mp);
					return;
				}
				BUMP_MIB(ip_mib.ipOutDiscards);
				if (ip_hdr_complete(ipha))
					freemsg(mp);
				else
					icmp_source_quench(q, mp);
				TRACE_2(TR_FAC_IP, TR_IP_WPUT_IRE_END,
				    "ip_wput_ire_end: q %p (%S)",
				    q, "discard");
				return;
			}
			if (PROTO == IPPROTO_UDP && !no_tp_cksum) {
				/*
				 * hlen gets the number of u_chars in the
				 * IP header
				 */
				hlen = (V_HLEN & 0xF) << 2;
				up = IPH_UDPH_CHECKSUMP(ipha, hlen);
				if (*up) {
					u_int	u1;
					/*
					 * NOTE: watch out for compiler high
					 * bits
					 */
					u1 = IP_CSUM(mp, hlen,
					    dst+IP_UDP_CSUM_COMP);
					*up = (uint16_t)(u1 ? u1 : ~u1);
				}
			}
		}
		/*
		 * Need to do this even when fragmenting. The local
		 * loopback can be done without computing checksums
		 * but forwarding out other interface must be done
		 * after the IP checksum (and ULP checksums) have been
		 * computed.
		 */
		if (CLASSD(ipha->ipha_dst) && !q->q_next) {
		    multi_loopback:
			ill = nilp(ill_t);
			ipc = (ipc_t *)q->q_ptr;
			/*
			 * Local loopback of multicasts?  Check the
			 * ill.
			 */
			ip2dbg(("ip_wput: multicast, loop %d\n",
			    ipc->ipc_multicast_loop));
			/*
			 * Note that the loopback function will not come
			 * in through ip_rput - it will only do the
			 * client fanout thus we need to do an mforward
			 * as well.  The is different from the BSD
			 * logic.
			 */
			if (ipc->ipc_multicast_loop &&
			    (ill = ire_to_ill(ire)) &&
			    ilm_lookup(ill, ipha->ipha_dst)) {
				/* Pass along the virtual output q. */
				ip_multicast_loopback(ill->ill_rq, mp);
			}
			if (ipha->ipha_ttl == 0) {
				/*
				 * 0 => only to this host i.e. we are
				 * done.
				 */
				freemsg(mp);
				TRACE_2(TR_FAC_IP, TR_IP_WPUT_IRE_END,
				    "ip_wput_ire_end: q %p (%S)",
				    q, "loopback");
				return;
			}
			/*
			 * IFF_MULTICAST is checked in ip_newroute
			 * i.e. we don't need to check it here since
			 * all IRE_CACHEs come from ip_newroute.
			 */
			/*
			 * For multicast traffic, SO_DONTROUTE is interpreted to
			 * mean only send the packet out the interface
			 * (optionally specified with IP_MULTICAST_IF)
			 * and do not forward it out additional interfaces.
			 * RSVP and the rsvp daemon is an example of a
			 * protocol and user level process that
			 * handles it's own routing. Hence, it uses the
			 * SO_DONTROUTE option to accomplish this.
			 */

			if (ip_g_mrouter && !ipc->ipc_dontroute &&
			    (ill || (ill = ire_to_ill(ire)))) {
				/* The checksum has not been computed yet */
				ipha->ipha_hdr_checksum = 0;
				ipha->ipha_hdr_checksum =
				    (uint16_t)ip_csum_hdr(ipha);

				if (ip_mforward(ill, ipha, mp)) {
					freemsg(mp);
					ip1dbg(("ip_wput: mforward failed\n"));
					TRACE_2(TR_FAC_IP, TR_IP_WPUT_IRE_END,
					    "ip_wput_ire_end: q %p (%S)",
					    q, "mforward failed");
					return;
				}
			}
		}

		max_frag = ire->ire_max_frag;
		dst += ttl_protocol;
		if (max_frag >= (unsigned int)LENGTH) {
			/* No fragmentation required for this one. */
			/* Complete the IP header checksum. */
			dst += ipha->ipha_ident;
			/*
			 * Don't use frag_flag if source routed or if
			 * multicast (since multicast packets do not solicit
			 * ICMP "packet too big" messages).
			 */
			if ((V_HLEN == IP_SIMPLE_HDR_VERSION ||
			    !ip_source_route_included(ipha)) &&
			    !CLASSD(ipha->ipha_dst))
				ipha->ipha_fragment_offset_and_flags |=
				    htons((uint16_t)ire->ire_frag_flag);

			dst += (v_hlen_tos_len >> 16)+(v_hlen_tos_len & 0xFFFF);
			dst += ipha->ipha_fragment_offset_and_flags;
			hlen = (V_HLEN & 0xF) - IP_SIMPLE_HDR_LENGTH_IN_WORDS;
			if (hlen) {
			    checksumoptions:
				/*
				 * Account for the IP Options in the IP
				 * header checksum.
				 */
				up = (uint16_t *)(rptr+IP_SIMPLE_HDR_LENGTH);
				do {
					dst += up[0];
					dst += up[1];
					up += 2;
				} while (--hlen);
			}
			dst = ((dst & 0xFFFF) + (dst >> 16));
			dst = ~(dst + (dst >> 16));
			/* Help some compilers treat dst as a register */
			{
				ipaddr_t u1 = dst;
				ipha->ipha_hdr_checksum = (uint16_t)u1;
			}

			hlen = ire->ire_ll_hdr_length;
			mp1 = ire->ire_ll_hdr_mp;
			/*
			 * If the driver accepts M_DATA prepends
			 * and we have enough room to lay it in ...
			 */
			if (hlen && (rptr - mp->b_datap->db_base) >= hlen) {
				/* XXX ipha is not aligned here */
				ipha = (ipha_t *)(rptr - hlen);
				mp->b_rptr = rptr;
				/* TODO: inline this small copy */
				bcopy(mp1->b_rptr, rptr, hlen);
				mp1 = mp;
			} else {
			    noprepend:
				mp1 = copyb(mp1);
				if (!mp1) {
					BUMP_MIB(ip_mib.ipOutDiscards);
					if (next_mp)
						freemsg(next_mp);
					if (ip_hdr_complete(ipha))
						freemsg(mp);
					else
						icmp_source_quench(q, mp);
					TRACE_2(TR_FAC_IP, TR_IP_WPUT_IRE_END,
					    "ip_wput_ire_end: q %p (%S)",
					    q, "discard MDATA");
					return;
				}
				mp1->b_cont = mp;
			}
			/* TODO: make this atomic */
			ire->ire_ob_pkt_count++;
			if (!next_mp) {
				/*
				 * Last copy going out (the ultra-common
				 * case).  Note that we intentionally replicate
				 * the putnext rather than calling it before
				 * the next_mp check in hopes of a little
				 * tail-call action out of the compiler.
				 */
				TRACE_2(TR_FAC_IP, TR_IP_WPUT_IRE_END,
				    "ip_wput_ire_end: q %p (%S)",
				    q, "last copy out(1)");
				putnext(stq, mp1);
				return;
			}
			/* More copies going out below. */
			putnext(stq, mp1);
		} else {
		    fragmentit:
			/* Pass it off to ip_wput_frag to chew up */
#ifndef SPEED_BEFORE_SAFETY
			/*
			 * Check that ipha_length is consistent with
			 * the mblk length
			 */
			if (LENGTH != (mp->b_cont ? msgdsize(mp) :
			    mp->b_wptr - rptr)) {
				ip0dbg(("Packet length mismatch: %d, %ld\n",
				    LENGTH, msgdsize(mp)));
				freemsg(mp);
				freemsg(next_mp);
				TRACE_2(TR_FAC_IP, TR_IP_WPUT_IRE_END,
				    "ip_wput_ire_end: q %p (%S)", q,
				    "packet length mismatch");
				return;
			}
#endif
			/*
			 * Don't use frag_flag if source routed or if
			 * multicast (since multicast packets do not solicit
			 * ICMP "packet too big" messages).
			 */
			if ((V_HLEN != IP_SIMPLE_HDR_VERSION &&
			    ip_source_route_included(ipha)) ||
			    CLASSD(ipha->ipha_dst)) {
				if (!next_mp) {
					TRACE_2(TR_FAC_IP, TR_IP_WPUT_IRE_END,
					    "ip_wput_ire_end: q %p (%S)",
					    q, "source routed");
					ip_wput_frag(ire, mp,
					    &ire->ire_ob_pkt_count,
					    max_frag, 0);
					return;
				}
				ip_wput_frag(ire, mp, &ire->ire_ob_pkt_count,
				    max_frag, 0);
			} else {
				if (!next_mp) {
					TRACE_2(TR_FAC_IP, TR_IP_WPUT_IRE_END,
					    "ip_wput_ire_end: q %p (%S)",
					    q, "not source routed");
					ip_wput_frag(ire, mp,
					    &ire->ire_ob_pkt_count,
					    max_frag,
					    ire->ire_frag_flag);
					return;
				}
				ip_wput_frag(ire, mp, &ire->ire_ob_pkt_count,
				    max_frag,
				    ire->ire_frag_flag);
			}
		}
	} else {
	    nullstq:
		/* A nil stq means the destination address is local. */
		/* TODO: make this atomic */
		ire->ire_ob_pkt_count++;
		if (!next_mp) {
			TRACE_2(TR_FAC_IP, TR_IP_WPUT_IRE_END,
			    "ip_wput_ire_end: q %p (%S)",
			    q, "local address");
			ip_wput_local(RD(q), ipha, mp, ire->ire_type,
			    ire->ire_rfq);
			return;
		}
		ip_wput_local(RD(q), ipha, mp, ire->ire_type, ire->ire_rfq);
	}

	/* More copies going out to additional interfaces. */
	ire = ire1;
	ASSERT(ire != NULL && next_mp != NULL);
	ill = ire_to_ill(ire);
	mp = next_mp;
	dst = ire->ire_addr;
	ipha = (ipha_t *)mp->b_rptr;
	/*
	 * Restore src so that we will pick up ire->ire_src_addr if src was 0.
	 * Restore ipha_ident "no checksum" flag.
	 */
	src = orig_src;
	ipha->ipha_ident = (uint16_t)no_tp_cksum;
	goto another;

#undef	rptr
}

/* Outbound IP fragmentation routine.  (May be called as writer.) */
static void
ip_wput_frag(ire_t *ire, mblk_t *mp_orig, u_int *pkt_count, uint32_t max_frag,
    uint32_t frag_flag)
{
	int	i1;
	mblk_t	*ll_hdr_mp;
	int	hdr_len;
	mblk_t	*hdr_mp;
	ipha_t	*ipha;
	int	ip_data_end;
	int	len;
	mblk_t	*mp = mp_orig;
	int	offset;
	queue_t	*q;
	uint32_t	v_hlen_tos_len;

	TRACE_0(TR_FAC_IP, TR_IP_WPUT_FRAG_START,
	    "ip_wput_frag_start:");

	ipha = (ipha_t *)mp->b_rptr;

	/*
	 * If the Don't Fragment flag is on, generate an ICMP destination
	 * unreachable, fragmentation needed.
	 */
	offset = ntohs(ipha->ipha_fragment_offset_and_flags);
	if (offset & IPH_DF) {
		BUMP_MIB(ip_mib.ipFragFails);
		/*
		 * Need to compute hdr checksum if called from ip_wput_ire.
		 * Note that ip_rput_forward verifies the checksum before
		 * calling this routine so in that case this is a noop.
		 */
		ipha->ipha_hdr_checksum = 0;
		ipha->ipha_hdr_checksum = (uint16_t)ip_csum_hdr(ipha);
		icmp_frag_needed(ire->ire_stq, mp, max_frag);
		TRACE_1(TR_FAC_IP, TR_IP_WPUT_FRAG_END,
		    "ip_wput_frag_end:(%S)",
		    "don't fragment");
		return;
	}
	/*
	 * Establish the starting offset.  May not be zero if we are fragging
	 * a fragment that is being forwarded.
	 */
	offset = offset & IPH_OFFSET;

	/* TODO why is this test needed? */
	v_hlen_tos_len = ((uint32_t *)ipha)[0];
	if (((max_frag - LENGTH) & ~7) < 8) {
		/* TODO: notify ulp somehow */
		BUMP_MIB(ip_mib.ipFragFails);
		freemsg(mp);
		TRACE_1(TR_FAC_IP, TR_IP_WPUT_FRAG_END,
		    "ip_wput_frag_end:(%S)",
		    "len < 8");
		return;
	}

	hdr_len = (V_HLEN & 0xF) << 2;
	ipha->ipha_hdr_checksum = 0;

	/* Get a copy of the header for the trailing frags */
	hdr_mp = ip_wput_frag_copyhdr((u_char *)ipha, hdr_len, offset);
	if (!hdr_mp) {
		BUMP_MIB(ip_mib.ipOutDiscards);
		freemsg(mp);
		TRACE_1(TR_FAC_IP, TR_IP_WPUT_FRAG_END,
		    "ip_wput_frag_end:(%S)",
		    "couldn't copy hdr");
		return;
	}

	/* Store the starting offset, with the MoreFrags flag. */
	i1 = offset | IPH_MF | frag_flag;
	ipha->ipha_fragment_offset_and_flags = htons((uint16_t)i1);

	/* Establish the ending byte offset, based on the starting offset. */
	offset <<= 3;
	ip_data_end = offset + ntohs((uint16_t)ipha->ipha_length) - hdr_len;

	/*
	 * Establish the number of bytes maximum per frag, after putting
	 * in the header.
	 */
	len = (max_frag - hdr_len) & ~7;

	/* Store the length of the first fragment in the IP header. */
	i1 = len + hdr_len;
	ASSERT(i1 <= IP_MAXPACKET);
	ipha->ipha_length = htons((uint16_t)i1);

	/*
	 * Compute the IP header checksum for the first frag.  We have to
	 * watch out that we stop at the end of the header.
	 */
	ipha->ipha_hdr_checksum = (uint16_t)ip_csum_hdr(ipha);

	/*
	 * Now carve off the first frag.  Note that this will include the
	 * original IP header.
	 */
	if (!(mp = ip_carve_mp(&mp_orig, i1))) {
		BUMP_MIB(ip_mib.ipOutDiscards);
		freeb(hdr_mp);
		freemsg(mp_orig);
		TRACE_1(TR_FAC_IP, TR_IP_WPUT_FRAG_END,
		    "ip_wput_frag_end:(%S)",
		    "couldn't carve first");
		return;
	}
	ll_hdr_mp = ire->ire_ll_hdr_mp;

	/* If there is a transmit header, get a copy for this frag. */
	/*
	 * TODO: should check db_ref before calling ip_carve_mp since
	 * it might give us a dup.
	 */
	if (!ll_hdr_mp) {
		/* No xmit header. */
		ll_hdr_mp = mp;
	} else if (mp->b_datap->db_ref == 1 &&
	    ire->ire_ll_hdr_length &&
	    ll_hdr_mp->b_wptr - ll_hdr_mp->b_rptr <
	    mp->b_rptr - mp->b_datap->db_base) {
		/* M_DATA fastpath */
		mp->b_rptr -= ll_hdr_mp->b_wptr - ll_hdr_mp->b_rptr;
		bcopy(ll_hdr_mp->b_rptr, mp->b_rptr,
		    ire->ire_ll_hdr_length);
		ll_hdr_mp = mp;
	} else if (!(ll_hdr_mp = copyb(ll_hdr_mp))) {
		BUMP_MIB(ip_mib.ipOutDiscards);
		freeb(hdr_mp);
		freemsg(mp);
		freemsg(mp_orig);
		TRACE_1(TR_FAC_IP, TR_IP_WPUT_FRAG_END,
		    "ip_wput_frag_end:(%S)",
		    "discard");
		return;
	} else
		ll_hdr_mp->b_cont = mp;

	q = ire->ire_stq;
	BUMP_MIB(ip_mib.ipFragCreates);
	putnext(q, ll_hdr_mp);
	pkt_count[0]++;

	/* Advance the offset to the second frag starting point. */
	offset += len;
	/*
	 * Update hdr_len from the copied header - there might be less options
	 * in the later fragments.
	 */
	hdr_len = (hdr_mp->b_rptr[0] & 0xF) << 2;
	/* Loop until done. */
	for (;;) {
		uint16_t	u1;

		if (ip_data_end - offset > len) {
			/*
			 * Carve off the appropriate amount from the original
			 * datagram.
			 */
			if (!(ll_hdr_mp = ip_carve_mp(&mp_orig, len))) {
				mp = NULL;
				break;
			}
			/*
			 * More frags after this one.  Get another copy
			 * of the header.
			 */
			if (ll_hdr_mp->b_datap->db_ref == 1 &&
			    hdr_mp->b_wptr - hdr_mp->b_rptr <
			    ll_hdr_mp->b_rptr - ll_hdr_mp->b_datap->db_base) {
				/* Inline IP header */
				ll_hdr_mp->b_rptr -= hdr_mp->b_wptr -
				    hdr_mp->b_rptr;
				bcopy(hdr_mp->b_rptr, ll_hdr_mp->b_rptr,
				    hdr_mp->b_wptr - hdr_mp->b_rptr);
				mp = ll_hdr_mp;
			} else {
				if (!(mp = copyb(hdr_mp))) {
					freemsg(ll_hdr_mp);
					break;
				}
				mp->b_cont = ll_hdr_mp;
			}
			ipha = (ipha_t *)mp->b_rptr;
			u1 = IPH_MF;
		} else {
			/*
			 * Last frag.  Consume the header. Set len to
			 * the length of this last piece.
			 */
			len = ip_data_end - offset;

			/*
			 * Carve off the appropriate amount from the original
			 * datagram.
			 */
			if (!(ll_hdr_mp = ip_carve_mp(&mp_orig, len))) {
				mp = NULL;
				break;
			}
			if (ll_hdr_mp->b_datap->db_ref == 1 &&
			    hdr_mp->b_wptr - hdr_mp->b_rptr <
			    ll_hdr_mp->b_rptr - ll_hdr_mp->b_datap->db_base) {
				/* Inline IP header */
				ll_hdr_mp->b_rptr -= hdr_mp->b_wptr -
				    hdr_mp->b_rptr;
				bcopy(hdr_mp->b_rptr, ll_hdr_mp->b_rptr,
				    hdr_mp->b_wptr - hdr_mp->b_rptr);
				mp = ll_hdr_mp;
				freeb(hdr_mp);
				hdr_mp = mp;
			} else {
				mp = hdr_mp;
				mp->b_cont = ll_hdr_mp;
			}
			ipha = (ipha_t *)mp->b_rptr;
			/* A frag of a frag might have IPH_MF non-zero */
			u1 = ntohs(ipha->ipha_fragment_offset_and_flags) &
			    IPH_MF;
		}
		u1 |= (uint16_t)(offset >> 3);
		u1 |= (uint16_t)frag_flag;
		/* Store the offset and flags in the IP header. */
		ipha->ipha_fragment_offset_and_flags = htons(u1);

		/* Store the length in the IP header. */
		u1 = (uint16_t)(len + hdr_len);
		ASSERT(u1 <= IP_MAXPACKET);
		ipha->ipha_length = htons(u1);

		/*
		 * Set the IP header checksum.	Note that mp is just
		 * the header, so this is easy to pass to ip_csum.
		 */
		ipha->ipha_hdr_checksum = (uint16_t)ip_csum_hdr(ipha);

		/* Attach a transmit header, if any, and ship it. */
		ll_hdr_mp = ire->ire_ll_hdr_mp;
		pkt_count[0]++;
		if (!ll_hdr_mp) {
			ll_hdr_mp = mp;
		} else if (mp->b_datap->db_ref == 1 &&
		    ire->ire_ll_hdr_length &&
		    ll_hdr_mp->b_wptr - ll_hdr_mp->b_rptr <
		    mp->b_rptr - mp->b_datap->db_base) {
			/* M_DATA fastpath */
			mp->b_rptr -= ll_hdr_mp->b_wptr - ll_hdr_mp->b_rptr;
			bcopy(ll_hdr_mp->b_rptr, mp->b_rptr,
			    ire->ire_ll_hdr_length);
			ll_hdr_mp = mp;
		} else if ((ll_hdr_mp = copyb(ll_hdr_mp)) != NULL) {
			ll_hdr_mp->b_cont = mp;
		} else
			break;
		BUMP_MIB(ip_mib.ipFragCreates);
		putnext(q, ll_hdr_mp);

		/* All done if we just consumed the hdr_mp. */
		if (mp == hdr_mp) {
			BUMP_MIB(ip_mib.ipFragOKs);
			TRACE_1(TR_FAC_IP, TR_IP_WPUT_FRAG_END,
			    "ip_wput_frag_end:(%S)",
			    "consumed hdr_mp");
			return;
		}
		/* Otherwise, advance and loop. */
		offset += len;
	}

	/* Clean up following allocation failure. */
	BUMP_MIB(ip_mib.ipOutDiscards);
	freemsg(mp);
	if (mp != hdr_mp)
		freeb(hdr_mp);
	if (mp != mp_orig)
		freemsg(mp_orig);
	TRACE_1(TR_FAC_IP, TR_IP_WPUT_FRAG_END,
	    "ip_wput_frag_end:(%S)",
	    "end--alloc failure");
}

/*
 * Copy the header plus those options which have the copy bit set
 */
static mblk_t *
ip_wput_frag_copyhdr(u_char *rptr, ssize_t hdr_len, int offset)
{
	mblk_t	*mp;
	u_char	*up;

	/*
	 * Quick check if we need to look for options without the copy bit
	 * set
	 */
	mp = allocb(ip_wroff_extra + hdr_len, BPRI_HI);
	if (!mp)
		return (mp);
	mp->b_rptr += ip_wroff_extra;
	if (hdr_len == IP_SIMPLE_HDR_LENGTH || offset != 0) {
		bcopy(rptr, mp->b_rptr, hdr_len);
		mp->b_wptr += hdr_len + ip_wroff_extra;
		return (mp);
	}
	up  = mp->b_rptr;
	bcopy(rptr, up, IP_SIMPLE_HDR_LENGTH);
	up += IP_SIMPLE_HDR_LENGTH;
	rptr += IP_SIMPLE_HDR_LENGTH;
	hdr_len -= IP_SIMPLE_HDR_LENGTH;
	while (hdr_len > 0) {
		uint32_t optval;
		uint32_t optlen;

		optval = *rptr;
		if (optval == IPOPT_EOL)
			break;
		if (optval == IPOPT_NOP)
			optlen = 1;
		else
			optlen = rptr[1];
		if (optval & IPOPT_COPY) {
			bcopy(rptr, up, optlen);
			up += optlen;
		}
		rptr += optlen;
		hdr_len -= optlen;
	}
	/*
	 * Make sure that we drop an even number of words by filling
	 * with EOL to the next word boundary.
	 */
	for (hdr_len = up - (mp->b_rptr + IP_SIMPLE_HDR_LENGTH);
	    hdr_len & 0x3; hdr_len++)
		*up++ = IPOPT_EOL;
	mp->b_wptr = up;
	/* Update header length */
	mp->b_rptr[0] = (uint8_t)((IP_VERSION << 4) | ((up - mp->b_rptr) >> 2));
	return (mp);
}

/*
 * Delivery to local recipients including fanout to multiple recipients.
 * Does not do checksumming of UDP/TCP.
 * Note: q should be the read side queue for either the ill or ipc.
 * Note: rq should be the read side q for the lower (ill) stream.
 * (May be called as writer.)
 */
void
ip_wput_local(queue_t *q, ipha_t *ipha, mblk_t *mp, int ire_type,
    queue_t *rq)
{
	uint32_t	protocol;

#define	rptr	((u_char *)ipha)

	TRACE_1(TR_FAC_IP, TR_IP_WPUT_LOCAL_START,
	    "ip_wput_local_start: q %p", q);

	loopback_packets++;

	ip2dbg(("ip_wput_local: from 0x%x to 0x%x\n",
	    ntohl(ipha->ipha_src), ntohl(ipha->ipha_dst)));
	if (!IS_SIMPLE_IPH(ipha)) {
		ip_wput_local_options(ipha);
	}
	protocol = ipha->ipha_protocol;
	switch (protocol) {
	case IPPROTO_ICMP:
	case IPPROTO_IGMP: {
		/* Figure out the the incoming logical interface */
		ipif_t	*ipif = nilp(ipif_t);

		if (ire_type == IRE_BROADCAST || rq == NULL) {
			ipaddr_t	src;

			src = ipha->ipha_src;
			if (rq == NULL)
				ipif = ipif_lookup_addr(src);
			else
				ipif = ipif_lookup_remote((ill_t *)rq->q_ptr,
				    src);
			if (!ipif) {
				ip1dbg(("ip_wput_local: No source"
				    " for broadcast/multicast\n"));
				freemsg(mp);
				TRACE_2(TR_FAC_IP, TR_IP_WPUT_LOCAL_END,
				    "ip_wput_local_end: q %p (%S)",
				    q, "no source for broadcast");
				return;
			}
			ip1dbg(("ip_wput_local:received %s:%d from 0x%x\n",
			    ipif->ipif_ill->ill_name, ipif->ipif_id,
			    ntohl(src)));
		}
		if (protocol == IPPROTO_ICMP) {
			if (rq == NULL)		/* For IRE_LOOPBACK */
				rq = q;
			icmp_inbound(rq, mp, ire_type, ipif, 0, 0);
			TRACE_2(TR_FAC_IP, TR_IP_WPUT_LOCAL_END,
			    "ip_wput_local_end: q %p (%S)",
			    q, "icmp");
			return;
		}
		if (igmp_input(q, mp, ipif)) {
			/* Bad packet - discarded by igmp_input */
			TRACE_2(TR_FAC_IP, TR_IP_WPUT_LOCAL_END,
			    "ip_wput_local_end: q %p (%S)",
			    q, "igmp_input--bad packet");
			return;
		}
		/* deliver to local raw users */
		break;
	}
	case IPPROTO_ENCAP:
		if (ip_g_mrouter) {
			mblk_t	*mp2;

			if (ipc_proto_fanout[IPPROTO_ENCAP].icf_ipc == NULL) {
				ip_mroute_decap(q, mp);
				return;
			}
			mp2 = dupmsg(mp);
			if (mp2)
				ip_mroute_decap(q, mp2);
		}
		break;
	case IPPROTO_UDP: {
		uint16_t	*up;
		uint32_t	ports;

		up = (uint16_t *)(rptr + IPH_HDR_LENGTH(ipha));
		/* Force a 'valid' checksum. */
		up[3] = 0;

		ports = *(uint32_t *)up;
		ip_fanout_udp(q, mp, ipha, ports, (u_short)ire_type,
		    IP_FF_SEND_ICMP | IP_FF_HDR_COMPLETE);
		TRACE_2(TR_FAC_IP, TR_IP_WPUT_LOCAL_END,
		    "ip_wput_local_end: q %p (%S)", q, "ip_fanout_udp");
		return;
	}
	case IPPROTO_TCP: {
		uint32_t	ports;

		if (mp->b_datap->db_type == M_DATA) {
			/*
			 * M_DATA mblk, so init mblk (chain) for no struio().
			 */
			mblk_t	*mp1 = mp;

			do
				mp1->b_datap->db_struioflag = 0;
			while ((mp1 = mp1->b_cont) != NULL);
		}
		ports = *(uint32_t *)(rptr + IPH_HDR_LENGTH(ipha));
		ip_fanout_tcp(q, mp, ipha, ports,
		    IP_FF_SEND_ICMP | IP_FF_HDR_COMPLETE |
		    IP_FF_SYN_ADDIRE);
		TRACE_2(TR_FAC_IP, TR_IP_WPUT_LOCAL_END,
		    "ip_wput_local_end: q %p (%S)", q, "ip_fanout_tcp");
		return;
	}
	default:
		break;
	}
	/*
	 * Find a client for some other protocol.  We give
	 * copies to multiple clients, if more than one is
	 * bound.
	 */
	ip_fanout_proto(q, mp, ipha,
	    IP_FF_SEND_ICMP | IP_FF_HDR_COMPLETE | IP_FF_RAWIP);
	TRACE_2(TR_FAC_IP, TR_IP_WPUT_LOCAL_END,
	    "ip_wput_local_end: q %p (%S)", q, "ip_fanout_proto");
#undef	rptr
}

/* Update any source route, record route or timestamp options */
/* Check that we are at end of strict source route */
static void
ip_wput_local_options(ipha_t *ipha)
{
	uint32_t	totallen;
	u_char		*opt;
	uint32_t	optval;
	uint32_t	optlen;
	ipaddr_t 	dst;
	uint32_t	u1;

	ip2dbg(("ip_wput_local_options\n"));
	totallen = ipha->ipha_version_and_hdr_length -
	    (uint8_t)((IP_VERSION << 4) + IP_SIMPLE_HDR_LENGTH_IN_WORDS);
	opt = (u_char *)&ipha[1];
	totallen <<= 2;
	while (totallen != 0) {
		switch (optval = opt[IPOPT_POS_VAL]) {
		case IPOPT_EOL:
			return;
		case IPOPT_NOP:
			optlen = 1;
			break;
		default:
			optlen = opt[IPOPT_POS_LEN];
		}
		if (optlen == 0 || optlen > totallen)
			break;

		switch (optval) {
			uint32_t off;
		case IPOPT_SSRR:
		case IPOPT_LSRR:
			off = opt[IPOPT_POS_OFF];
			off--;
			if (optlen < IP_ADDR_LEN ||
			    off > optlen - IP_ADDR_LEN) {
				/* End of source route */
				break;
			}
			/*
			 * This will only happen if two consecutive entries
			 * in the source route contains our address or if
			 * it is a packet with a loose source route which
			 * reaches us before consuming the whole source route
			 */
			ip1dbg(("ip_wput_local_options: not end of SR\n"));
			if (optval == IPOPT_SSRR) {
				return;
			}
			/*
			 * Hack: instead of dropping the packet truncate the
			 * source route to what has been used.
			 */
			bzero((char *)opt + off, optlen - off);
			opt[IPOPT_POS_LEN] = (uint8_t)off;
			break;
		case IPOPT_RR:
			off = opt[IPOPT_POS_OFF];
			off--;
			if (optlen < IP_ADDR_LEN ||
			    off > optlen - IP_ADDR_LEN) {
				/* No more room - ignore */
				ip1dbg((
				    "ip_wput_forward_options: end of RR\n"));
				break;
			}
			dst = htonl(INADDR_LOOPBACK);
			bcopy(&dst, (char *)opt + off, IP_ADDR_LEN);
			opt[IPOPT_POS_OFF] += IP_ADDR_LEN;
			break;
		case IPOPT_IT:
			/* Insert timestamp if there is romm */
			switch (opt[IPOPT_POS_OV_FLG] & 0x0F) {
			case IPOPT_IT_TIME:
				off = IPOPT_IT_TIMELEN;
				break;
			case IPOPT_IT_SPEC:
#ifdef IPOPT_IT_SPEC_BSD
			case IPOPT_IT_SPEC_BSD:
#endif
				/* Verify that the address matched */
				off = opt[IPOPT_POS_OFF] - 1;
				bcopy((char *)opt + off, &dst, IP_ADDR_LEN);
				if (!ire_ctable_lookup(dst, 0, IRE_LOCAL, NULL,
				    NULL, MATCH_IRE_TYPE))
					/* Not for us */
					break;
				/* FALLTHRU */
			case IPOPT_IT_TIME_ADDR:
				off = IP_ADDR_LEN + IPOPT_IT_TIMELEN;
				break;
			default:
				/*
				 * ip_*put_options should have already
				 * dropped this packet.
				 */
				cmn_err(CE_PANIC, "ip_wput_local_options: "
				    "unknown IT - bug in ip_wput_options?\n");
				return;	/* Keep "lint" happy */
			}
			if (opt[IPOPT_POS_OFF] - 1 + off > optlen) {
				/* Increase overflow counter */
				off = (opt[IPOPT_POS_OV_FLG] >> 4) + 1;
				opt[IPOPT_POS_OV_FLG] = (uint8_t)
				    ((opt[IPOPT_POS_OV_FLG] & 0x0F) |
				    (off << 4));
				break;
			}
			off = opt[IPOPT_POS_OFF] - 1;
			switch (opt[IPOPT_POS_OV_FLG] & 0x0F) {
			case IPOPT_IT_SPEC:
#ifdef IPOPT_IT_SPEC_BSD
			case IPOPT_IT_SPEC_BSD:
#endif
			case IPOPT_IT_TIME_ADDR:
				dst = htonl(INADDR_LOOPBACK);
				bcopy(&dst, (char *)opt + off, IP_ADDR_LEN);
				opt[IPOPT_POS_OFF] += IP_ADDR_LEN;
				/* FALLTHRU */
			case IPOPT_IT_TIME:
				off = opt[IPOPT_POS_OFF] - 1;
				/* Compute # of milliseconds since midnight */
				u1 = (hrestime.tv_sec % (24 * 60 * 60)) * 1000 +
					hrestime.tv_nsec / (NANOSEC / MILLISEC);
				bcopy(&u1, (char *)opt + off, IPOPT_IT_TIMELEN);
				opt[IPOPT_POS_OFF] += IPOPT_IT_TIMELEN;
				break;
			}
			break;
		}
		totallen -= optlen;
		opt += optlen;
	}
}

/*
 * Send out a multicast packet on interface ipif
 */
void
ip_wput_multicast(queue_t *q, mblk_t *mp, ipif_t *ipif)
{
	ipha_t	*ipha;
#define	rptr	((u_char *)ipha)
	ire_t	*ire;
	uint32_t	v_hlen_tos_len;
	ipaddr_t	dst;

	ipha = (ipha_t *)mp->b_rptr;

#ifdef	_BIG_ENDIAN
#define	V_HLEN	(v_hlen_tos_len >> 24)
#else
#define	V_HLEN	(v_hlen_tos_len & 0xFF)
#endif

#ifndef SPEED_BEFORE_SAFETY
	/*
	 * Make sure we have a full-word aligned message and that at least
	 * a simple IP header is accessible in the first message.  If not,
	 * try a pullup.
	 */
	if (!OK_32PTR(rptr) || (mp->b_wptr - rptr) < IP_SIMPLE_HDR_LENGTH) {
		if (!pullupmsg(mp, IP_SIMPLE_HDR_LENGTH)) {
			BUMP_MIB(ip_mib.ipOutDiscards);
			freemsg(mp);
			return;
		}
		ipha = (ipha_t *)mp->b_rptr;
	}
#endif

	/* Most of the code below is written for speed, not readability */
	v_hlen_tos_len = ((uint32_t *)ipha)[0];

#ifndef SPEED_BEFORE_SAFETY
	/*
	 * If ip_newroute() fails, we're going to need a full
	 * header for the icmp wraparound.
	 */
	if (V_HLEN != IP_SIMPLE_HDR_VERSION) {
		u_int	v_hlen = V_HLEN;
		if ((v_hlen >> 4) != IP_VERSION) {
			BUMP_MIB(ip_mib.ipOutDiscards);
			freemsg(mp);
			return;
		}
		/*
		 * Are there enough bytes accessible in the header?  If
		 * not, try a pullup.
		 */
		v_hlen &= 0xF;
		v_hlen <<= 2;
		if (v_hlen > (mp->b_wptr - rptr)) {
			if (!pullupmsg(mp, v_hlen)) {
				BUMP_MIB(ip_mib.ipOutDiscards);
				freemsg(mp);
				return;
			}
			ipha = (ipha_t *)mp->b_rptr;
		}
	}
#endif

	/*
	 * Find an IRE which matches the destination and the outgoing
	 * queue (i.e. the outgoing interface.)
	 */
	if (ipif->ipif_flags & IFF_POINTOPOINT)
		dst = ipif->ipif_pp_dst_addr;
	else
		dst = ipha->ipha_dst;
	ire = ire_ctable_lookup(dst, 0, 0, ipif, NULL, MATCH_IRE_IPIF);
	if (!ire) {
		/*
		 * Mark this packet to make it be delivered to
		 * ip_wput_ire after the new ire has been
		 * created.
		 */
		mp->b_prev = nilp(mblk_t);
		mp->b_next = nilp(mblk_t);
		ip_newroute_ipif(q, mp, ipif, dst);
		return;
	}
	ip_wput_ire(q, mp, ire);
}

/* Called from ip_wput for all non data messages */
static void
ip_wput_nondata(queue_t *q, mblk_t *mp)
{
	mblk_t	*mp1;

	switch (mp->b_datap->db_type) {
	case M_IOCTL:
		/*
		 * IOCTL processing begins in ip_sioctl_copyin_setup which
		 * will arrange to copy in associated control structures.
		 */
		ip_sioctl_copyin_setup(q, mp);
		return;
	case M_IOCDATA:
		/* IOCTL continuation following copyin or copyout. */
		if (mi_copy_state(q, mp, nilp(mblk_t *)) == -1) {
			/*
			 * The copy operation failed.  mi_copy_state already
			 * cleaned up, so we're out of here.
			 */
			return;
		}
		/*
		 * If we just completed a copy in, we become writer and
		 * continue processing in ip_sioctl_copyin_done.  If it
		 * was a copy out, we call mi_copyout again.  If there is
		 * nothing more to copy out, it will complete the IOCTL.
		 */
		if (MI_COPY_DIRECTION(mp) == MI_COPY_IN) {
			if (ip_sioctl_copyin_writer(mp))
				become_exclusive(q, mp,
				    (pfi_t)ip_sioctl_copyin_done);
			else
				ip_sioctl_copyin_done(q, mp);
		} else
			mi_copyout(q, mp);
		return;
	case M_IOCNAK:
		/*
		 * The only way we could get here is if a resolver didn't like
		 * an IOCTL we sent it.	 This shouldn't happen.
		 */
		(void) mi_strlog(q, 1, SL_ERROR|SL_TRACE,
		    "ip_wput: unexpected M_IOCNAK, ioc_cmd 0x%x",
		    ((struct iocblk *)mp->b_rptr)->ioc_cmd);
		freemsg(mp);
		return;
	case M_IOCACK:
		/* Finish socket ioctls passed through to ARP. */
		become_exclusive(q, mp, ip_sioctl_iocack);
		return;
	case M_FLUSH:
		if (*mp->b_rptr & FLUSHW)
			flushq(q, FLUSHALL);
		if (q->q_next) {
			putnext(q, mp);
			return;
		}
		if (*mp->b_rptr & FLUSHR) {
			*mp->b_rptr &= ~FLUSHW;
			qreply(q, mp);
			return;
		}
		freemsg(mp);
		return;
	case IRE_DB_REQ_TYPE:
		/* An Upper Level Protocol wants a copy of an IRE. */
		ip_ire_req(q, mp);
		return;
	case M_CTL:
		/* M_CTL messages are used by ARP to tell us things. */
		if ((mp->b_wptr - mp->b_rptr) < sizeof (arc_t))
			break;
		switch (((arc_t *)mp->b_rptr)->arc_cmd) {
		case AR_ENTRY_SQUERY:
			ip_wput_ctl(q, mp);
			return;
		case AR_CLIENT_NOTIFY:
			become_exclusive(q, mp, (pfi_t)ip_arp_news);
			return;
		default:
			break;
		}
		break;
	case M_PROTO:
	case M_PCPROTO:
		/*
		 * The only PROTO messages we expect are ULP binds and
		 * copies of option negotiation acknowledgements.
		 */
		switch (((union T_primitives *)mp->b_rptr)->type) {
		case O_T_BIND_REQ:
		case T_BIND_REQ:
			ip_bind(q, mp);
			return;
		case O_T_OPTMGMT_REQ:
			ip2dbg(("ip_wput: O_T_OPTMGMT_REQ\n"));

			if (ip_optmgmt_writer(mp))
				become_exclusive(q, mp, (pfi_t)ip_optcom_req);
			else {
				if (!snmpcom_req(q, mp, ip_snmp_set,
				    ip_snmp_get,
				    IS_PRIVILEGED_QUEUE(q)))
					/*
					 * Call svr4_optcom_req so that it can
					 * generate the ack.
					 */
					svr4_optcom_req(q, mp,
					    IS_PRIVILEGED_QUEUE(q),
					    &ip_opt_obj);
			}
			return;
		case T_OPTMGMT_REQ:
			ip2dbg(("ip_wput: T_OPTMGMT_REQ\n"));
			/*
			 * Note: No snmpcom_req support through new
			 * T_OPTMGMT_REQ.
			 */
			if (ip_optmgmt_writer(mp))
				become_exclusive(q, mp, (pfi_t)ip_optcom_req);
			else {
				/*
				 * Call tpi_optcom_req so that it can
				 * generate the ack.
				 */
				tpi_optcom_req(q, mp, IS_PRIVILEGED_QUEUE(q),
				    &ip_opt_obj);
			}
			return;
		case T_UNBIND_REQ:
			ip_unbind(q, mp);
			return;
		default:
			/*
			 * Have to drop any DLPI messages coming down from
			 * arp (such as an info_req which would cause ip
			 * to receive an extra info_ack if it was passed
			 * through.
			 */
			ip1dbg(("ip_wput_nondata: dropping M_PROTO %d\n",
			    (int)*(u_int *)mp->b_rptr));
			freemsg(mp);
			return;
		}
		/* NOTREACHED */
	case IRE_DB_TYPE:
		/*
		 * This is a response back from a resolver.  It
		 * consists of a message chain containing:
		 *	IRE_MBLK-->LL_HDR_MBLK->pkt
		 * The IRE_MBLK is the one we allocated in ip_newroute.
		 * The LL_HDR_MBLK is the DLPI header to use to get
		 * the attached packet, and subsequent ones for the
		 * same destination, transmitted.
		 *
		 * Rearrange the message into:
		 *	IRE_MBLK-->pkt
		 * and point the ire_ll_hdr_mp field of the IRE
		 * to LL_HDR_MBLK.  Then become writer and, in
		 * ire_add_then_put, the IRE will be added, and then
		 * the packet will be run back through ip_wput.
		 * This time it will make it to the wire.
		 */
		if ((mp->b_wptr - mp->b_rptr) != sizeof (ire_t))
			break;
		mp1 = mp->b_cont;
		((ire_t *)mp->b_rptr)->ire_ll_hdr_mp = mp1;
		mp->b_cont = mp1->b_cont;
		mp1->b_cont = nilp(mblk_t);
		become_exclusive(q, mp, (pfi_t)ire_add_then_put);
		return;
	default:
		break;
	}
	if (q->q_next) {
		putnext(q, mp);
	} else
		freemsg(mp);
}

/*
 * Process IP options in an outbound packet.  Modify the destination if there
 * is a source route option.
 * Returns non-zero if something fails in which case an ICMP error has been
 * sent and mp freed.
 */
static int
ip_wput_options(queue_t *q, mblk_t *mp, ipha_t *ipha)
{
	uint32_t	totallen;
	u_char		*opt;
	uint32_t	optval;
	uint32_t	optlen;
	ipaddr_t 	dst;
	intptr_t	code = 0;

	ip2dbg(("ip_wput_options\n"));
	dst = ipha->ipha_dst;
	totallen = ipha->ipha_version_and_hdr_length -
	    (uint8_t)((IP_VERSION << 4) + IP_SIMPLE_HDR_LENGTH_IN_WORDS);
	opt = (u_char *)&ipha[1];
	totallen <<= 2;
	while (totallen != 0) {
		switch (optval = opt[IPOPT_POS_VAL]) {
		case IPOPT_EOL:
			return (0);
		case IPOPT_NOP:
			optlen = 1;
			break;
		default:
			optlen = opt[IPOPT_POS_LEN];
		}
		if (optlen == 0 || optlen > totallen) {
			ip1dbg(("ip_wput_options: bad option len %d, %d\n",
			    optlen, totallen));
			code = (char *)&opt[IPOPT_POS_LEN] - (char *)ipha;
			goto param_prob;
		}
		ip2dbg(("ip_wput_options: opt %d, len %d\n",
		    optval, optlen));

		switch (optval) {
			uint32_t off;
		case IPOPT_SSRR:
		case IPOPT_LSRR:
			off = opt[IPOPT_POS_OFF];
			if (off < IPOPT_MINOFF_SR) {
				ip1dbg((
				    "ip_wput_options: bad option offset %d\n",
				    off));
				code = (char *)&opt[IPOPT_POS_OFF] -
				    (char *)ipha;
				goto param_prob;
			}
			ip1dbg(("ip_wput_options: next hop 0x%x\n",
			    ntohl(dst)));
			/*
			 * For strict: verify that dst is directly
			 * reachable.
			 */
			if (optval == IPOPT_SSRR &&
			    !ire_ftable_lookup(dst, 0, 0, IRE_INTERFACE, NULL,
				NULL, NULL, MATCH_IRE_TYPE)) {
				ip1dbg(("ip_wput_options: SSRR not"
				    " directly reachable: 0x%x\n",
				    ntohl(dst)));
				goto bad_src_route;
			}
			break;
		case IPOPT_RR:
			off = opt[IPOPT_POS_OFF];
			if (off < IPOPT_MINOFF_SR) {
				ip1dbg((
				    "ip_wput_options: bad option offset %d\n",
				    off));
				code = (char *)&opt[IPOPT_POS_OFF] -
				    (char *)ipha;
				goto param_prob;
			}
			break;
		case IPOPT_IT:
			/*
			 * Verify that length >=5 and that there is either
			 * room for another timestamp or that the overflow
			 * counter is not maxed out.
			 */
			code = (char *)&opt[IPOPT_POS_LEN] - (char *)ipha;
			if (optlen < IPOPT_MINLEN_IT) {
				goto param_prob;
			}
			switch (opt[IPOPT_POS_OV_FLG] & 0x0F) {
			case IPOPT_IT_TIME:
				off = IPOPT_IT_TIMELEN;
				break;
			case IPOPT_IT_TIME_ADDR:
			case IPOPT_IT_SPEC:
#ifdef IPOPT_IT_SPEC_BSD
			case IPOPT_IT_SPEC_BSD:
#endif
				off = IP_ADDR_LEN + IPOPT_IT_TIMELEN;
				break;
			default:
				code = (char *)&opt[IPOPT_POS_OV_FLG] -
				    (char *)ipha;
				goto param_prob;
			}
			if (opt[IPOPT_POS_OFF] - 1 + off > optlen &&
			    (opt[IPOPT_POS_OV_FLG] & 0xF0) == 0xF0) {
				/*
				 * No room and the overflow counter is 15
				 * already.
				 */
				goto param_prob;
			}
			off = opt[IPOPT_POS_OFF];
			if (off < IPOPT_MINOFF_IT) {
				code = (char *)&opt[IPOPT_POS_OFF] -
				    (char *)ipha;
				goto param_prob;
			}
			break;
		}
		totallen -= optlen;
		opt += optlen;
	}
	return (0);

param_prob:
	/*
	 * Since ip_wput() isn't close to finished, we fill
	 * in enough of the header for credible error reporting.
	 */
	if (ip_hdr_complete((ipha_t *)mp->b_rptr)) {
		/* Failed */
		freemsg(mp);
		return (-1);
	}
	icmp_param_problem(q, mp, (uint8_t)code);
	return (-1);

bad_src_route:
	/*
	 * Since ip_wput() isn't close to finished, we fill
	 * in enough of the header for credible error reporting.
	 */
	if (ip_hdr_complete((ipha_t *)mp->b_rptr)) {
		/* Failed */
		freemsg(mp);
		return (-1);
	}
	icmp_unreachable(q, mp, ICMP_SOURCE_ROUTE_FAILED);
	return (-1);
}

/*
 * Hash list insertion routine for IP client structures.
 * Used when fully connected. Insert at the front of the list so that
 * IP_UDP_MATCH will give preference to connected sockets.
 */
void
ipc_hash_insert_connected(icf_t *icf, ipc_t *ipc)
{
	ipc_t	**ipcp;
	ipc_t	*ipcnext;

	if (ipc->ipc_ptphn)
		ipc_hash_remove(ipc);
	ipcp = &icf->icf_ipc;
	mutex_enter(&icf->icf_lock);
	ipcnext = ipcp[0];
	if (ipcnext)
		ipcnext->ipc_ptphn = &ipc->ipc_hash_next;
	ipc->ipc_hash_next = ipcnext;
	ipc->ipc_ptphn = ipcp;
	ipcp[0] = ipc;
	ipc->ipc_fanout_lock = &icf->icf_lock;	/* For ipc_hash_remove */
	mutex_exit(&icf->icf_lock);
}

/*
 * Hash list insertion routine for IP client structures.
 * Used when bound to a specified interface address.
 */
void
ipc_hash_insert_bound(icf_t *icf, ipc_t *ipc)
{
	ipc_t	**ipcp;
	ipc_t	*ipcnext;

	if (ipc->ipc_ptphn)
		ipc_hash_remove(ipc);
	ipcp = &icf->icf_ipc;
	mutex_enter(&icf->icf_lock);
	/* Skip past all connected ipcs */
	while (ipcp[0] && ipcp[0]->ipc_faddr != INADDR_ANY)
		ipcp = &(ipcp[0]->ipc_hash_next);
	ipcnext = ipcp[0];
	if (ipcnext)
		ipcnext->ipc_ptphn = &ipc->ipc_hash_next;
	ipc->ipc_hash_next = ipcnext;
	ipc->ipc_ptphn = ipcp;
	ipcp[0] = ipc;
	ipc->ipc_fanout_lock = &icf->icf_lock;	/* For ipc_hash_remove */
	mutex_exit(&icf->icf_lock);
}

/*
 * Hash list insertion routine for IP client structures.
 * Used when bound to INADDR_ANY to make streams bound to a specified interface
 * address take precedence.
 */
void
ipc_hash_insert_wildcard(icf_t *icf, ipc_t *ipc)
{
	ipc_t	**ipcp;
	ipc_t	*ipcnext;

	if (ipc->ipc_ptphn)
		ipc_hash_remove(ipc);
	ipcp = &icf->icf_ipc;
	mutex_enter(&icf->icf_lock);
	/* Skip to end of list */
	while (ipcp[0])
		ipcp = &(ipcp[0]->ipc_hash_next);
	ipcnext = ipcp[0];
	if (ipcnext)
		ipcnext->ipc_ptphn = &ipc->ipc_hash_next;
	ipc->ipc_hash_next = ipcnext;
	ipc->ipc_ptphn = ipcp;
	ipcp[0] = ipc;
	ipc->ipc_fanout_lock = &icf->icf_lock;	/* For ipc_hash_remove */
	mutex_exit(&icf->icf_lock);
}

/*
 * Write service routine.
 * Handles backenabling when a device as been flow controlled by
 * backenabling all non-tcp upper layers.
 * Also handles operations that ip_open/ip_close needs help with;
 * those routines are hot and certain operations require exclusive access
 * in ip. These operations are:
 *	ip_open ill initialization
 *	ip_close ill removal
 *	ip_close ipc removal in the uncommon cases:
 *		The ipc has multicast group membership (ipc_ilg_inuse)
 *		There is a pending interface bringup on the ipc (ipc_bind_ill)
 *		The ipc is that of the multicast routing daemon (ip_g_mrouter)
 */
static void
ip_wsrv(queue_t *q)
{
	ipc_t	*ipc;
	ill_t	*ill;
	mblk_t	*mp;

	if (q->q_next) {
		ill = (ill_t *)q->q_ptr;

		/*
		 * The device flow control has opened up.
		 * Walk through all upper (ipc) streams and qenable
		 * those that have queued data.
		 * In the case of close synchronization this needs to
		 * be done to ensure that all upper layers blocked
		 * due to flow control to the closing device
		 * get unblocked.
		 */
		ip1dbg(("ip_wsrv: walking\n"));
		ipc_walk_nontcp(ipc_qenable, NULL);

		if (ill->ill_close_flags & IPCF_OPENING) {
			/* Initialize the new ILL. */
			ill->ill_error = ill_init(RD(q), ill);
			ill->ill_close_flags |= IPCF_OPEN_DONE;
			return;
		}
		if (ill->ill_close_flags & IPCF_CLOSING) {
			if (ill->ill_bind_pending_q != nilp(queue_t)) {
				ipc = (ipc_t *)(ill->ill_bind_pending_q->q_ptr);
				ASSERT(ipc->ipc_bind_ill == ill);
				ipc->ipc_bind_ill = nilp(ill_t);
			}
			ill->ill_bind_pending_q = nilp(queue_t);
			ill_delete(ill);
			ill->ill_close_flags |= IPCF_CLOSE_DONE;
			return;
		}
		return;
	}
	ipc = (ipc_t *)q->q_ptr;
	ip1dbg(("ip_wsrv: %p %p\n", (void *)q, (void *)ipc));

	if (ipc->ipc_close_flags & IPCF_CLOSING) {
		if (ipc->ipc_bind_ill != nilp(ill_t)) {
			ASSERT(ipc->ipc_bind_ill->ill_bind_pending_q == q);
			ipc->ipc_bind_ill->ill_bind_pending_q = nilp(queue_t);
		}
		ipc->ipc_bind_ill = nilp(ill_t);
		if (ipc->ipc_rq == ip_g_mrouter || ipc->ipc_wq == ip_g_mrouter)
			(void) ip_mrouter_done();
		ilg_delete_all(ipc);
		ipc->ipc_close_flags |= IPCF_CLOSE_DONE;
		return;
	}
	if (q->q_first == NULL)
		return;

	/*
	 * Set ipc_draining flag to make ip_wput pass messages through and
	 * do a putbq instead of a putq if flow controlled from the driver.
	 * noenable the queue so that a putbq from ip_wsrv does not reenable
	 * (causing an infinite loop).
	 * Note: this assumes that ip is configured such that no
	 * other thread can execute in ip_wput while ip_wsrv is running.
	 */
	ipc->ipc_draining = 1;
	noenable(q);
	while ((mp = getq(q)) != NULL) {
		ip_wput(q, mp);
		if (ipc->ipc_did_putbq) {
			/* ip_wput did a putbq */
			ipc->ipc_did_putbq = 0;
			break;
		}
	}
	enableok(q);
	ipc->ipc_draining = 0;
}

/*
 * Hash list removal routine for IP client structures.
 */
static void
ipc_hash_remove(ipc_t *ipc)
{
	ipc_t	*ipcnext;
	kmutex_t *lockp;

	/*
	 * Extract the lock pointer in case there are concurrent
	 * hash_remove's for this instance.
	 */
	lockp = ipc->ipc_fanout_lock;
	if (ipc->ipc_ptphn) {
		ASSERT(lockp != NULL);
		mutex_enter(lockp);
		if (ipc->ipc_ptphn) {
			ipcnext = ipc->ipc_hash_next;
			if (ipcnext) {
				ipcnext->ipc_ptphn = ipc->ipc_ptphn;
				ipc->ipc_hash_next = nilp(ipc_t);
			}
			*ipc->ipc_ptphn = ipcnext;
			ipc->ipc_ptphn = nilp(ipc_t *);
		}
		mutex_exit(lockp);
		ipc->ipc_fanout_lock = NULL;
	}
}

/*
 * qenable the write side queue.
 * Used when flow control is opened up on the device stream.
 * Note: it is not possible to restrict the enabling to those
 * queues that have data queued since that would introduce a race
 * condition.
 */
/* ARGSUSED */
static void
ipc_qenable(ipc_t *ipc, void *arg)
{
	qenable(ipc->ipc_wq);
}

/*
 * Walk the list of all IPC's calling the function provided with the
 * specified argument for each.	 Note that this only walks IPC's that
 * have been bound.
 */
void
ipc_walk(pfv_t func, void *arg)
{
	int	i;
	icf_t	*icf;
	ipc_t	*ipc;
	ipc_t	*ipc1;

#ifdef lint
	ipc1 = nilp(ipc_t);
#endif

	for (i = 0; i < A_CNT(ipc_udp_fanout); i++) {
		icf = &ipc_udp_fanout[i];
		mutex_enter(&icf->icf_lock);
		for (ipc = icf->icf_ipc; ipc; ipc = ipc1) {
			IPC_REFHOLD(ipc);
			mutex_exit(&icf->icf_lock);
			(*func)(ipc, arg);
			mutex_enter(&icf->icf_lock);
			ipc1 = ipc->ipc_hash_next;
			IPC_REFRELE(ipc);
		}
		mutex_exit(&icf->icf_lock);
	}
	for (i = 0; i < A_CNT(ipc_tcp_conn_fanout); i++) {
		icf = &ipc_tcp_conn_fanout[i];
		mutex_enter(&icf->icf_lock);
		for (ipc = icf->icf_ipc; ipc; ipc = ipc1) {
			IPC_REFHOLD(ipc);
			mutex_exit(&icf->icf_lock);
			(*func)(ipc, arg);
			mutex_enter(&icf->icf_lock);
			ipc1 = ipc->ipc_hash_next;
			IPC_REFRELE(ipc);
		}
		mutex_exit(&icf->icf_lock);
	}
	for (i = 0; i < A_CNT(ipc_tcp_listen_fanout); i++) {
		icf = &ipc_tcp_listen_fanout[i];
		mutex_enter(&icf->icf_lock);
		for (ipc = icf->icf_ipc; ipc; ipc = ipc1) {
			IPC_REFHOLD(ipc);
			mutex_exit(&icf->icf_lock);
			(*func)(ipc, arg);
			mutex_enter(&icf->icf_lock);
			ipc1 = ipc->ipc_hash_next;
			IPC_REFRELE(ipc);
		}
		mutex_exit(&icf->icf_lock);
	}
	for (i = 0; i < A_CNT(ipc_proto_fanout); i++) {
		icf = &ipc_proto_fanout[i];
		mutex_enter(&icf->icf_lock);
		for (ipc = icf->icf_ipc; ipc; ipc = ipc1) {
			IPC_REFHOLD(ipc);
			mutex_exit(&icf->icf_lock);
			(*func)(ipc, arg);
			mutex_enter(&icf->icf_lock);
			ipc1 = ipc->ipc_hash_next;
			IPC_REFRELE(ipc);
		}
		mutex_exit(&icf->icf_lock);
	}
}

/*
 * Walk the list of all IPC's except TCP IPC's calling the function
 * provided with the specified argument for each.
 * Note that this only walks IPC's that have been bound.
 */
void
ipc_walk_nontcp(pfv_t func, void *arg)
{
	int	i;
	icf_t	*icf;
	ipc_t	*ipc;
	ipc_t	*ipc1;

#ifdef lint
	ipc1 = nilp(ipc_t);
#endif

	for (i = 0; i < A_CNT(ipc_udp_fanout); i++) {
		icf = &ipc_udp_fanout[i];
		mutex_enter(&icf->icf_lock);
		for (ipc = icf->icf_ipc; ipc; ipc = ipc1) {
			IPC_REFHOLD(ipc);
			mutex_exit(&icf->icf_lock);
			(*func)(ipc, arg);
			mutex_enter(&icf->icf_lock);
			ipc1 = ipc->ipc_hash_next;
			IPC_REFRELE(ipc);
		}
		mutex_exit(&icf->icf_lock);
	}
	for (i = 0; i < A_CNT(ipc_proto_fanout); i++) {
		icf = &ipc_proto_fanout[i];
		mutex_enter(&icf->icf_lock);
		for (ipc = icf->icf_ipc; ipc; ipc = ipc1) {
			IPC_REFHOLD(ipc);
			mutex_exit(&icf->icf_lock);
			(*func)(ipc, arg);
			mutex_enter(&icf->icf_lock);
			ipc1 = ipc->ipc_hash_next;
			IPC_REFRELE(ipc);
		}
		mutex_exit(&icf->icf_lock);
	}
}

/* ipc_walk routine invoked for ip_ipc_report for each ipc. */
static void
ipc_report1(ipc_t *ipc, void *mp)
{
	char	buf1[16];
	char	buf2[16];

	(void) mi_mpprintf((mblk_t *)mp,
	    "%08p %08p %08p %s/%05d %s/%05d",
	    ipc, ipc->ipc_rq, ipc->ipc_wq,
	    ip_dot_addr(ipc->ipc_laddr, buf1), ipc->ipc_lport,
	    ip_dot_addr(ipc->ipc_faddr, buf2), ipc->ipc_fport);
}

/*
 * Named Dispatch routine to produce a formatted report on all IPCs
 * that are listed in one of the fanout tables.
 * This report is accessed by using the ndd utility to "get" ND variable
 * "ip_ipc_status".
 */
/* ARGSUSED */
int
ip_ipc_report(queue_t *q, mblk_t *mp, void *arg)
{
	(void) mi_mpprintf(mp,
	    "IPC      rfq      stq      addr            mask            "
	    "src             gateway         mxfrg rtt   ref "
	    "in/out/forward type");
	ipc_walk(ipc_report1, mp);
	return (0);
}



boolean_t
ipc_wantpacket(ipc_t *ipc, ipaddr_t dst)
{
	if (!CLASSD(dst) || ipc->ipc_multi_router)
		return (true);
	return (ilg_member(ipc, dst));
}
