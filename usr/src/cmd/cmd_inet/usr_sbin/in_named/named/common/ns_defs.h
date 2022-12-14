/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

/*
 *	from ns.h	4.33 (Berkeley) 8/23/90
 *	$Id: ns_defs.h,v 8.33 1997/05/21 19:52:16 halley Exp $
 */

/*
 * Copyright (c) 1986
 *    The Regents of the University of California.  All rights reserved.
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
 * 	This product includes software developed by the University of
 * 	California, Berkeley and its contributors.
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

/* Portions Copyright (c) 1993 by Digital Equipment Corporation.
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Digital Equipment Corporation not be used in advertising or
 * publicity pertaining to distribution of the document or software without
 * specific, written prior permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND DIGITAL EQUIPMENT CORP. DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL DIGITAL EQUIPMENT
 * CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/*
 * Portions Copyright (c) 1996, 1997 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */
#pragma ident   "@(#)ns_defs.h 1.5     97/12/03 SMI"

/*
 * Global definitions for the name server.
 */

/*
 * Effort has been expended here to make all structure members 32 bits or
 * larger land on 32-bit boundaries; smaller structure members have been
 * deliberately shuffled and smaller integer sizes chosen where possible
 * to make sure this happens.  This is all meant to avoid structure member
 * padding which can cost a _lot_ of memory when you have hundreds of 
 * thousands of entries in your cache.
 */

/*
 * Timeout time should be around 1 minute or so.  Using the
 * the current simplistic backoff strategy, the sequence
 * retrys after 4, 8, and 16 seconds.  With 3 servers, this
 * dies out in a little more than a minute.
 * (sequence RETRYBASE, 2*RETRYBASE, 4*RETRYBASE... for MAXRETRY)
 */
#define MINROOTS	2	/* min number of root hints */
#define NSMAX		16	/* max number of NS addrs to try ([0..255]) */
#define RETRYBASE	4 	/* base time between retries */
#define	MAXCLASS	255	/* XXX - may belong elsewhere */
#define MAXRETRY	3	/* max number of retries per addr */
#define MAXCNAMES	8	/* max # of CNAMES tried per addr */
#define MAXQUERIES	20	/* max # of queries to be made */
#define	MAXQSERIAL	4	/* max # of outstanding QSERIAL's */
				/* (prevent "recursive" loops) */
#define	INIT_REFRESH	600	/* retry time for initial secondary */
				/* contact (10 minutes) */
#define MIN_REFRESH	2	/* never refresh more frequently than once */
				/* every MIN_REFRESH seconds */
#define MIN_RETRY	1	/* never retry more frequently than once */
				/* every MIN_RETRY seconds */
#define NADDRECS	20	/* max addt'l rr's per resp */

#define XFER_TIMER	120	/* named-xfer's connect timeout */
#define MAX_XFER_TIME	60*60*2	/* default max seconds for an xfer */
#define XFER_TIME_FUDGE	10	/* MAX_XFER_TIME fudge */
#define MAX_XFERS_RUNNING 20	/* max value of transfers_in */
#define DEFAULT_XFERS_RUNNING 10  /* default value of transfers_in */
#define DEFAULT_XFERS_PER_NS 2	  /* default # of xfers per peer nameserver */
#define	XFER_BUFSIZE	(16*1024) /* arbitrary but bigger than most MTU's */

#define ALPHA    0.7	/* How much to preserve of old response time */
#define	BETA	 1.2	/* How much to penalize response time on failure */
#define	GAMMA	 0.98	/* How much to decay unused response times */

	/* What maintainance operations need to be performed sometime soon? */
#define	MAIN_NEED_RELOAD	0x0001	/* db_reload() needed. */
#define	MAIN_NEED_MAINT		0x0002	/* ns_maint() needed. */
#define	MAIN_NEED_ENDXFER	0x0004	/* endxfer() needed. */
#define	MAIN_NEED_ZONELOAD	0x0008	/* loadxfer() needed. */
#define	MAIN_NEED_DUMP		0x0010	/* doadump() needed. */
#define	MAIN_NEED_STATSDUMP	0x0020	/* ns_stats() needed. */
#define	MAIN_NEED_EXIT		0x0040	/* exit() needed. */
#define	MAIN_NEED_QRYLOG	0x0080	/* toggle_qrylog() needed. */
#define	MAIN_NEED_DEBUG		0x0100	/* use_desired_debug() needed. */
#define	MAIN_NEED_NOTIFY	0x0200	/* do_notify_after_load() needed */

	/* What global options are set? */
#define	OPTION_NORECURSE	0x0001	/* Don't recurse even if asked. */
#define	OPTION_NOFETCHGLUE	0x0002	/* Don't fetch missing glue. */
#define	OPTION_FORWARD_ONLY	0x0004	/* Don't use NS RR's, just forward. */
#define	OPTION_FAKE_IQUERY	0x0008	/* Fake up bogus response to IQUERY. */
#define	OPTION_NONOTIFY		0x0010	/* Turn off notify */
#define	OPTION_NONAUTH_NXDOMAIN	0x0020	/* Generate non-auth NXDOMAINs? */
#define	OPTION_MULTIPLE_CNAMES	0x0040	/* Allow a name to have multiple
					 * CNAME RRs */

#ifdef BIND_UPDATE
#define SOAINCRINTVL 300 /* default value for the time after which
			  * the zone serial number must be incremented
			  * after a successful update has occurred */
#define DUMPINTVL 3600   /* default interval at which to dump changed zones
			  * randomized, not exact */
#define DEFERUPDCNT	100	/* default number of updates that can happen
				 * before the zone serial number will be
				 * incremented */
#define UPDATE_TIMER XFER_TIMER
#endif /* BIND_UPDATE */

#define	USE_MINIMUM	0xffffffff
#define	MAXIMUM_TTL	0x7fffffff

#define CLEAN_TIMER		0x01
#define INTERFACE_TIMER		0x02
#define STATS_TIMER		0x04

	/* IP address accessor, network byte order. */
#define	ina_ulong(ina)	(ina.s_addr)

	/* IP address accessor, host byte order, read only. */
#define	ina_hlong(ina)	ntohl(ina.s_addr)

	/* IP address equality. */
	/* XXX: assumes that network byte order won't affect equality. */
#define	ina_equal(a, b)	(ina_ulong(a) == ina_ulong(b))

	/* IP address equality with a mask. */
#define	ina_onnet(h, n, m) ((ina_ulong(h) & ina_ulong(m)) == ina_ulong(n))

	/* Sequence space arithmetic. */
#define SEQ_GT(a,b)	((int32_t)((a)-(b)) > 0)

	/* Cheap garbage collection. */
#define	FREE_ONCE(p)	do { if (p) { free(p); p = NULL; } } while (0)

	/* Like assert() but using log system rather than stderr. */
#define INSIST(c) if (!(c)) \
	ns_panic(ns_log_insist, 1, "%s:%d: insist '%s' failed", \
		 __FILE__, __LINE__, #c); else (void)NULL

#define INSIST_ERR(c) if (!(c)) \
	ns_panic(ns_log_insist, 1, "%s:%d: insist '%s' failed: %s", \
		 __FILE__, __LINE__, #c, strerror(errno)); else (void)NULL

enum severity { ignore, warn, fail, not_set };

enum znotify { znotify_use_default=0, znotify_yes, znotify_no };

enum axfr_format { axfr_use_default=0, axfr_one_answer, axfr_many_answers };

struct ip_match_direct {
	struct in_addr address;
	struct in_addr mask;
};

struct ip_match_indirect {
	struct ip_match_list *list;
};

typedef enum { ip_match_pattern, ip_match_indirect, ip_match_localhost,
	       ip_match_localnets } ip_match_type;

typedef struct ip_match_element {
	ip_match_type type;
	u_int flags;
	union {
		struct ip_match_direct direct;
		struct ip_match_indirect indirect;
	} u;
	struct ip_match_element *next;
} *ip_match_element;

/* Flags for ip_match_element */
#define IP_MATCH_NEGATE			0x01	/* match means deny access */

typedef struct ip_match_list {
	ip_match_element first;
	ip_match_element last;
} *ip_match_list;

typedef struct ztimer_info {
	char *name;
	int class;
	int type;
} *ztimer_info;

/* these fields are ordered to maintain word-alignment;
 * be careful about changing them.
 */
struct zoneinfo {
	char		*z_origin;	/* root domain name of zone */
	time_t		z_time;		/* time for next refresh */
	time_t		z_lastupdate;	/* time of last soa serial increment */
	u_int32_t	z_refresh;	/* refresh interval */
	u_int32_t	z_retry;	/* refresh retry interval */
	u_int32_t	z_expire;	/* expiration time for cached info */
	u_int32_t	z_minimum;	/* minimum TTL value */
	u_int32_t	z_serial;	/* changes if zone modified */
	char		*z_source;	/* source location of data */
	time_t		z_ftime;	/* modification time of source file */
	struct in_addr	z_xaddr;	/* override server for next xfer */
	struct in_addr	z_addr[NSMAX];	/* list of master servers for zone */
	u_char		z_addrcnt;	/* number of entries in z_addr[] */
	u_char		z_type;		/* type of zone; see below */
	u_int16_t	z_flags;	/* state bits; see below */
	pid_t		z_xferpid;	/* xfer child pid */
	int		z_class;	/* class of zone */
	int		z_numxfrs;	/* Ref count of concurrent xfrs. */
	enum severity	z_checknames;	/* How to handle non-RFC-compliant names */
#ifdef BIND_UPDATE
	time_t		z_dumptime;	/* randomized time for next zone dump
					 * if Z_NEED_DUMP is set */
	u_int32_t	z_dumpintvl;	/* time interval between zone dumps */
	time_t		z_soaincrintvl; /* interval for updating soa serial */
	time_t		z_soaincrtime;	/* time for soa increment */
	u_int32_t	z_deferupdcnt;  /* max number of updates before SOA
					 * serial number incremented */
	u_int32_t	z_updatecnt;    /* number of update requests processed
					 * since the last SOA serial update */
	char 		*z_updatelog;	/* log file for updates */
#endif	
	ip_match_list 	z_update_acl;  	/* list of who can issue dynamic
					   updates */
	ip_match_list	z_query_acl;	/* sites we'll answer questions for */
	ip_match_list	z_transfer_acl;	/* sites that may get a zone transfer
					   from us */
	long		z_max_transfer_time_in;	/* max num seconds for AXFR */
	enum znotify	z_notify;	/* Notify mode */
	struct in_addr	z_also_notify[NSMAX];  /* More nameservers to notify */
	int		z_notify_count;
	evTimerID	z_timer;	/* maintenance timer */
	ztimer_info	z_timerinfo;	/* UAP associated with timer */
	time_t		z_nextmaint;	/* time of next maintenance */
};

	/* zone types (z_type) */
enum zonetype { z_nil, z_master, z_slave, z_hint, z_stub, z_any };
#define	Z_NIL		z_nil		/* XXX */
#define Z_MASTER	z_master	/* XXX */
#define Z_PRIMARY	z_master	/* XXX */
#define Z_SLAVE		z_slave		/* XXX */
#define Z_SECONDARY	z_slave		/* XXX */
#define Z_HINT		z_hint		/* XXX */
#define Z_CACHE		z_hint		/* XXX */
#define Z_STUB		z_stub		/* XXX */
#define Z_ANY		z_any		/* XXX*2 */

	/* zone state bits (16 bits) */
#define	Z_AUTH		0x0001		/* zone is authoritative */
#define	Z_NEED_XFER	0x0002		/* waiting to do xfer */
#define	Z_XFER_RUNNING	0x0004		/* asynch. xfer is running */
#define	Z_NEED_RELOAD	0x0008		/* waiting to do reload */
#define	Z_SYSLOGGED	0x0010		/* have logged timeout */
#define	Z_QSERIAL	0x0020		/* sysquery()'ing for serial number */
#define	Z_FOUND		0x0040		/* found in boot file when reloading */
#define	Z_INCLUDE	0x0080		/* set if include used in file */
#define	Z_DB_BAD	0x0100		/* errors when loading file */
#define	Z_TMP_FILE	0x0200		/* backup file for xfer is temporary */
#ifdef BIND_UPDATE
#define	Z_DYNAMIC	0x0400		/* allow dynamic updates */
#define	Z_NEED_DUMP	0x0800		/* zone has changed, needs a dump */
#define	Z_NEED_SOAUPDATE 0x1000		/* soa serial number needs increment */
#endif /* BIND_UPDATE */
#define	Z_XFER_ABORTED	0x2000		/* zone transfer has been aborted */
#define	Z_XFER_GONE	0x4000		/* zone transfer process is gone */
#define	Z_TIMER_SET	0x8000		/* z_timer contains a valid id */

	/* named_xfer exit codes */
#define	XFER_UPTODATE	0		/* zone is up-to-date */
#define	XFER_SUCCESS	1		/* performed transfer successfully */
#define	XFER_TIMEOUT	2		/* no server reachable/xfer timeout */
#define	XFER_FAIL	3		/* other failure, has been logged */

/* XXX - "struct qserv" is deprecated in favor of "struct nameser" */
struct qserv {
	struct sockaddr_in
			ns_addr;	/* address of NS */
	struct databuf	*ns;		/* databuf for NS record */
	struct databuf	*nsdata;	/* databuf for server address */
	struct timeval	stime;		/* time first query started */
	int		nretry;		/* # of times addr retried */
};

/*
 * Structure for recording info on forwarded or generated queries.
 */
struct qinfo {
	u_int16_t	q_id;		/* id of query */
	u_int16_t	q_nsid;		/* id of forwarded query */
	struct sockaddr_in
			q_from;		/* requestor's address */
	u_char		*q_msg,		/* the message */
			*q_cmsg;	/* the cname message */
	int16_t		q_msglen,	/* len of message */
			q_cmsglen;	/* len of cname message */
	int16_t		q_dfd;		/* UDP file descriptor */
	struct fwdinfo	*q_fwd;		/* last	forwarder used */
	time_t		q_time;		/* time to retry */
	time_t		q_expire;	/* time to expire */
	struct qinfo	*q_next;	/* rexmit list (sorted by time) */
	struct qinfo	*q_link;	/* storage list (random order) */
	struct databuf	*q_usedns[NSMAX]; /* databuf for NS that we've tried */
	struct qserv	q_addr[NSMAX];	/* addresses of NS's */
#ifdef notyet
	struct nameser	*q_ns[NSMAX];	/* name servers */
#endif
	u_char		q_naddr;	/* number of addr's in q_addr */
	u_char		q_curaddr;	/* last addr sent to */
	u_char		q_nusedns;	/* number of elements in q_usedns[] */
	u_int8_t	q_flags;	/* see below */
	int16_t		q_cname;	/* # of cnames found */
	int16_t		q_nqueries;	/* # of queries required */
	struct qstream	*q_stream;	/* TCP stream, null if UDP */
	struct zoneinfo	*q_zquery;	/* Zone query is about (Q_ZSERIAL) */
	char		*q_domain;	/* domain of most enclosing zone cut */
	char		*q_name;	/* domain of query */
	u_int16_t	q_class;	/* class of query */
	u_int16_t	q_type;		/* type of query */
#ifdef BIND_NOTIFY
	int		q_notifyzone;	/* zone which needs a sysnotify()
					 * when the reply to this comes in.
					 */
#endif
};

	/* q_flags bits (8 bits) */
#define	Q_SYSTEM	0x01		/* is a system query */
#define	Q_PRIMING	0x02		/* generated during priming phase */
#define	Q_ZSERIAL	0x04		/* getting zone serial for xfer test */
#define	Q_USEVC		0x08		/* forward using tcp not udp */

#define	Q_NEXTADDR(qp,n)	\
	(((qp)->q_fwd == (struct fwdinfo *)0) ? \
	 &(qp)->q_addr[n].ns_addr : &(qp)->q_fwd->fwdaddr)

#define	RETRY_TIMEOUT	45

/*
 * Return codes from ns_forw:
 */
#define	FW_OK		0
#define	FW_DUP		1
#define	FW_NOSERVER	2
#define	FW_SERVFAIL	3

typedef void (*sq_closure)(struct qstream *qs);

#ifdef BIND_UPDATE
struct fdlist {
	int		fd;
	struct fdlist	*next;
};
#endif

struct qstream {
	int		s_rfd;		/* stream file descriptor */
	int		s_size;		/* expected amount of data to rcv */
	int		s_bufsize;	/* amount of data received in s_buf */
	u_char		*s_buf;		/* buffer of received data */
	u_char		*s_wbuf;	/* send buffer */
	u_char		*s_wbuf_send;	/* next sendable byte of send buffer */
	u_char		*s_wbuf_free;	/* next free byte of send buffer */
	u_char		*s_wbuf_end;	/* byte after end of send buffer */
	sq_closure	s_wbuf_closure;	/* callback for writable descriptor */
	struct qstream	*s_next;	/* next stream */
	struct sockaddr_in
			s_from;		/* address query came from */
	time_t		s_time;		/* time stamp of last transaction */
	int		s_refcnt;	/* number of outstanding queries */
	u_char		s_temp[HFIXEDSZ];
#ifdef BIND_UPDATE
	int		s_opcode;	/* type of request */
	int		s_linkcnt;	/* number of client connections using
					 * this connection to forward updates
					 * to the primary */
	struct fdlist	*s_fds;		/* linked list of connections to the
					 * primaries that have been used by
					 * the server to forward this client's
					 * update requests */
#endif
	evStreamID	evID_r;		/* read event. */
	evFileID	evID_w;		/* writable event handle. */
	evConnID	evID_c;		/* connect event handle */
	u_int		flags;		/* see below */
	struct qstream_xfr {
		enum { s_x_base, s_x_firstsoa, s_x_zone,
		       s_x_lastsoa, s_x_done }
				state;		/* state of transfer. */
		u_char		*msg,		/* current assembly message. */
				*cp,		/* where are we in msg? */
				*eom,		/* end of msg. */
				*ptrs[128];	/* ptrs for dn_comp(). */
		int		class,		/* class of an XFR. */
				id,		/* id of an XFR. */
				opcode,		/* opcode of an XFR. */
				zone;		/* zone being XFR'd. */
		struct namebuf	*top;		/* top np of an XFR. */
		struct qs_x_lev {		/* decompose the recursion. */
			enum {sxl_ns, sxl_all, sxl_sub}
					state;	/* what's this level doing? */
			int		flags;	/* see below (SXL_*). */
			char		dname[MAXDNAME];
			struct namebuf	*np,	/* this node. */
					*nnp,	/* next node to process. */
					**npp,	/* subs. */
					**npe;	/* end of subs. */
			struct databuf	*dp;	/* current rr. */
			struct qs_x_lev	*next;	/* link. */
		}		*lev;	/* LIFO. */
		enum axfr_format transfer_format;
	} xfr;
};
#define SXL_GLUING	0x01
#define SXL_ZONECUT	0x02

	/* flags */
#define STREAM_MALLOC		0x01
#define STREAM_WRITE_EV		0x02
#define STREAM_READ_EV		0x04
#define STREAM_CONNECT_EV	0x08
#define STREAM_DONE_CLOSE	0x10
#define STREAM_AXFR		0x20

struct netinfo {
	struct netinfo	*next;
	struct in_addr	addr,
			mask;
};

#define ALLOW_NETS	0x0001
#define	ALLOW_HOSTS	0x0002
#define	ALLOW_ALL	(ALLOW_NETS | ALLOW_HOSTS)

struct fwdinfo {
	struct fwdinfo	*next;
	struct sockaddr_in
			fwdaddr;
};

enum nameserStats {	nssRcvdR,	/* sent us an answer */
			nssRcvdNXD,	/* sent us a negative response */
			nssRcvdFwdR,	/* sent us a response we had to fwd */
			nssRcvdDupR,	/* sent us an extra answer */
			nssRcvdFail,	/* sent us a SERVFAIL */
			nssRcvdFErr,	/* sent us a FORMERR */
			nssRcvdErr,	/* sent us some other error */
			nssRcvdAXFR,	/* sent us an AXFR */
			nssRcvdLDel,	/* sent us a lame delegation */
			nssRcvdOpts,	/* sent us some IP options */
			nssSentSysQ,	/* sent them a sysquery */
			nssSentAns,	/* sent them an answer */
			nssSentFwdQ,	/* fwdd a query to them */
			nssSentDupQ,	/* sent them a retry */
			nssSendtoErr,	/* error in sendto */
#ifdef XSTATS
			nssRcvdQ,	/* sent us a query */
			nssRcvdIQ,	/* sent us an inverse query */
			nssRcvdFwdQ,	/* sent us a query we had to fwd */
			nssRcvdDupQ,	/* sent us a retry */
			nssRcvdTCP,	/* sent us a query using TCP */
			nssSentFwdR,	/* fwdd a response to them */
			nssSentFail,	/* sent them a SERVFAIL */
			nssSentFErr,	/* sent them a FORMERR */
			nssSentNaAns,   /* sent them a non autoritative answer */
			nssSentNXD,	/* sent them a negative response */
#endif
			nssLast };

struct nameser {
	struct in_addr	addr;		/* key */
	u_long		stats[nssLast];	/* statistics */
#ifdef notyet
	u_int32_t	rtt;		/* round trip time */
	/* XXX - need to add more stuff from "struct qserv", and use our rtt */
	u_int16_t	flags;		/* see below */
#endif
	u_int8_t	xfers;		/* #/xfers running right now */
};
		
enum transport { primary_trans, secondary_trans, response_trans, num_trans };

/* types used by the parser or config routines */

typedef struct zone_config {
	void *opaque;
} zone_config;

typedef struct listen_info {
	u_short port;
	ip_match_list list;
	struct listen_info *next;
} *listen_info;

typedef struct listen_info_list {
	listen_info first;
	listen_info last;
} *listen_info_list;

typedef struct options {
	u_int flags;
	char *directory;
	char *pid_filename;
	char *named_xfer;
	int transfers_in;
	int transfers_per_ns;
	int transfers_out;
	enum axfr_format transfer_format;
	long max_transfer_time_in;
	struct sockaddr_in query_source;
	ip_match_list query_acl;
	ip_match_list transfer_acl;
	ip_match_list topology;
	enum severity check_names[num_trans];
	u_long data_size;
	u_long stack_size;
	u_long core_size;
	u_long files;
	listen_info_list listen_list;
	struct fwdinfo *fwdtab;
	/* XXX need to add forward option */
	int clean_interval;
	int interface_interval;
	int stats_interval;
#ifdef SUNW_LISTEN_BACKLOG
	u_int listen_backlog;
#endif /* SUNW_LISTEN_BACKLOG */
#ifdef SUNW_POLL
	u_int pollfd_chunk_size;
#endif /* SUNW_POLL */
} *options;

typedef struct key_info {
	char *name;
	char *algorithm;
	char *secret;			/* XXX should be u_char? */
} *key_info;

typedef struct key_list_element {
	key_info info;
	struct key_list_element *next;
} *key_list_element;

typedef struct key_info_list {
	key_list_element first;
	key_list_element last;
} *key_info_list;

typedef struct topology_config {
	void *opaque;
} topology_config;

#define UNKNOWN_TOPOLOGY_DISTANCE	9998
#define MAX_TOPOLOGY_DISTANCE		9999

typedef struct topology_distance {
	ip_match_list patterns;
	struct topology_distance *next;
} *topology_distance;

typedef struct topology_context {
	topology_distance first;
	topology_distance last;
} *topology_context;

typedef struct acl_table_entry {
	char *name;
	ip_match_list list;
	struct acl_table_entry *next;
} *acl_table_entry;

typedef struct server_config {
	void *opaque;
} server_config;

#define SERVER_INFO_BOGUS	0x01

typedef struct server_info {
	struct in_addr address;
	u_int flags;
	int transfers;
	enum axfr_format transfer_format;
	key_info_list key_list;
	/* could move statistics to here, too */
	struct server_info *next;
} *server_info;

/*
 * enum <--> name translation
 */

struct ns_sym {
	int	number;		/* Identifying number, like ns_log_default */
	char *	name;		/* Its symbolic name, like "default" */
};

/*
 * Logging options
 */

typedef enum ns_logging_categories {
	ns_log_default = 0,
	ns_log_config,
	ns_log_parser,
	ns_log_queries,
	ns_log_lame_servers,
	ns_log_statistics,
	ns_log_panic,
	ns_log_update,
	ns_log_ncache,
	ns_log_xfer_in,
	ns_log_xfer_out,
	ns_log_db,
	ns_log_eventlib,
	ns_log_packet,
	ns_log_notify,
	ns_log_cname,
	ns_log_security,
	ns_log_os,
	ns_log_insist,
	ns_log_maint,
	ns_log_load,
	ns_log_resp_checks,
	ns_log_max_category
} ns_logging_categories;

typedef struct log_config {
	log_context log_ctx;
	log_channel eventlib_channel;
	log_channel packet_channel;
	int	    default_debug_active;
} *log_config;

struct map {
	char *			token;
	int			val;
};

#define NOERROR_NODATA   6	/* only used internally by the server, used for
				 * -ve $ing non-existence of records. 6 is not 
				 * a code used as yet anyway. anant@isi.edu
				 */

#define NTTL		600 /* ttl for negative data: 10 minutes? */

#define VQEXPIRY	900 /* a VQ entry expires in 15*60 = 900 seconds */

#ifdef BIND_UPDATE
enum req_action { Finish, Refuse, Return };
#endif

#ifdef BIND_NOTIFY
typedef struct notify_info {
	char *name;
	int class;
} *notify_info;
#endif

#ifdef INIT
	error "INIT already defined, check system include files"
#endif
#ifdef DECL
	error "DECL already defined, check system include files"
#endif

#ifdef MAIN_PROGRAM
#define INIT(x) = x
#define	DECL
#else
#define INIT(x)
#define DECL extern
#endif
