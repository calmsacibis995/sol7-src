/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

/*
 * Copyright (c) 1995,1996 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _SYS_TIHDR_H
#define	_SYS_TIHDR_H

#pragma ident	"@(#)tihdr.h	1.16	97/12/06 SMI"	/* from SVr4.0 11.4 */

/*
 * Include declarations implicit to TPI and shared with user level code
 */
#include <sys/tpicommon.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * The feature test macro, _SUN_TPI_VERSION makes some of additional
 * declarations available and changes some existing ones. There was
 * some changes done to this interface and this feature test macro
 * enables transitioning to those changes while maintaining retaining
 * backward compatibility.
 *
 * The following is all the information
 * needed by the Transport Service Interface.
 */

/*
 * The following are the definitions of the Transport
 * Service Interface primitives.
 */

/*
 * Primitives that are initiated by the transport user.
 */
#define	T_CONN_REQ	0	/* connection request		*/
#if _SUN_TPI_VERSION > 1
#define	O_T_CONN_RES	1	/* old connection response	*/
#else
#define	T_CONN_RES	1	/* connection response		*/
#endif /* _SUN_TPI_VERSION > 1 */
#define	T_DISCON_REQ	2	/* disconnect request		*/
#define	T_DATA_REQ	3	/* data request			*/
#define	T_EXDATA_REQ	4	/* expedited data request	*/
#define	T_INFO_REQ	5	/* information request		*/
/*
 * Bind Request primitive with TLI inspired
 * address binding semantics. If requested address is
 * found to be busy, an alternative free address is
 * returned. (Requires comparison of requested address to
 * returned address to verify if the requested address was
 * bound)
 */
#if _SUN_TPI_VERSION > 0
#define	O_T_BIND_REQ	6
#else
#define	T_BIND_REQ	6
#endif /* _SUN_TPI_VERSION > 0 */

#define	T_UNBIND_REQ	7	/* unbind request		*/
#define	T_UNITDATA_REQ	8	/* unitdata request		*/

/*
 * Option management request with TLI inspired semantics.
 * The packing of options in option buffer is private contract
 * between transport provider and its users and can differ
 * between different transports.
 */
#if _SUN_TPI_VERSION > 0
#define	O_T_OPTMGMT_REQ	9
#else
#define	T_OPTMGMT_REQ	9
#endif	/* _SUN_TPI_VERSION > 0 */

#define	T_ORDREL_REQ	10	/* orderly release req		*/

/*
 * Primitives that are initiated by the transport provider.
 */
#define	T_CONN_IND	11	/* connection indication	*/
#define	T_CONN_CON	12	/* connection confirmation	*/
#define	T_DISCON_IND	13	/* disconnect indication	*/
#define	T_DATA_IND	14	/* data indication		*/
#define	T_EXDATA_IND	15	/* expeditied data indication	*/
#define	T_INFO_ACK	16	/* information acknowledgment	*/
#define	T_BIND_ACK	17	/* bind acknowledment		*/
#define	T_ERROR_ACK	18	/* error acknowledgment		*/
#define	T_OK_ACK	19	/* ok acknowledgment		*/
#define	T_UNITDATA_IND	20	/* unitdata indication		*/
#define	T_UDERROR_IND	21	/* unitdata error indication	*/
#define	T_OPTMGMT_ACK	22	/* manage options ack		*/
#define	T_ORDREL_IND	23	/* orderly release ind		*/
/*
 * Primitives added to namespace and contain a mix of ones
 * initiated by transport user or provider.
 */
#define	T_ADDR_REQ	24	/* address request		*/
#define	T_ADDR_ACK	25	/* address acknowledgement	*/

#if _SUN_TPI_VERSION > 0
/*
 * Bind request primitive with XTI inspired (and better) address
 * binding semantics. If the requested address is found to be busy,
 * an error is returned. (No need to compare addresses on successful
 * bind acknowledgement).
 */
#define	T_BIND_REQ	26	/* bind request			*/

/*
 * Option management request with XTI inspired semantics.
 * The packing of options in option buffer is required to
 * be with 'struct T_opthdr' data structure defined later in
 * this header.
 */
#define	T_OPTMGMT_REQ	27 /* manage options req - T_opthdr option header */
#endif /* _SUN_TPI_VERSION > 0 */

#if _SUN_TPI_VERSION > 1
/*
 * The connection response that expects its ACCEPTOR_id to have been
 * filled in from the value supplied via a T_CAPABILITY_ACK.
 */
#define	T_CONN_RES	28	/* connection response		*/

/*
 * Capability request and ack.  These primitives are optional and
 * subsume the functionality of T_INFO_{REQ,ACK}.
 */
#define	T_CAPABILITY_REQ	30
#define	T_CAPABILITY_ACK	31
#endif /* _SUN_TPI_VERSION > 1 */

#ifdef _KERNEL
/*
 * Sun private TPI extensions. They are currently used for transparently
 * passing options through the connection-oriented loopback transport.
 * Values assigned to them may change.
 */
#define	T_OPTDATA_REQ	0x1001	/* data (with options) request	*/
#define	T_OPTDATA_IND	0x1002	/* data (with options) indication */
#endif /* _KERNEL */

/*
 * The following are the events that drive the state machine
 */
/* Initialization events */
#define	TE_BIND_REQ	0	/* bind request				*/
#define	TE_UNBIND_REQ	1	/* unbind request			*/
#define	TE_OPTMGMT_REQ	2	/* manage options req			*/
#define	TE_BIND_ACK	3	/* bind acknowledment			*/
#define	TE_OPTMGMT_ACK	4	/* manage options ack			*/
#define	TE_ERROR_ACK	5	/* error acknowledgment			*/
#define	TE_OK_ACK1	6	/* ok ack  seqcnt == 0			*/
#define	TE_OK_ACK2	7	/* ok ack  seqcnt == 1, q == resq	*/
#define	TE_OK_ACK3	8	/* ok ack  seqcnt == 1, q != resq	*/
#define	TE_OK_ACK4	9	/* ok ack  seqcnt > 1			*/

/* Connection oriented events */
#define	TE_CONN_REQ	10	/* connection request			*/
#define	TE_CONN_RES	11	/* connection response			*/
#define	TE_DISCON_REQ	12	/* disconnect request			*/
#define	TE_DATA_REQ	13	/* data request				*/
#define	TE_EXDATA_REQ	14	/* expedited data request		*/
#define	TE_ORDREL_REQ	15	/* orderly release req			*/
#define	TE_CONN_IND	16	/* connection indication		*/
#define	TE_CONN_CON	17	/* connection confirmation		*/
#define	TE_DATA_IND	18	/* data indication			*/
#define	TE_EXDATA_IND	19	/* expedited data indication		*/
#define	TE_ORDREL_IND	20	/* orderly release ind			*/
#define	TE_DISCON_IND1	21	/* disconnect indication seq == 0	*/
#define	TE_DISCON_IND2	22	/* disconnect indication seq == 1	*/
#define	TE_DISCON_IND3	23	/* disconnect indication seq > 1	*/
#define	TE_PASS_CONN	24	/* pass connection			*/

/* Unit data events */
#define	TE_UNITDATA_REQ	25	/* unitdata request			*/
#define	TE_UNITDATA_IND	26	/* unitdata indication			*/
#define	TE_UDERROR_IND	27	/* unitdata error indication		*/

#define	TE_NOEVENTS	28
/*
 * The following are the possible states of the Transport
 * Service Interface
 */

#define	TS_UNBND		0	/* unbound			*/
#define	TS_WACK_BREQ		1	/* waiting ack of BIND_REQ	*/
#define	TS_WACK_UREQ		2	/* waiting ack of UNBIND_REQ	*/
#define	TS_IDLE			3	/* idle				*/
#define	TS_WACK_OPTREQ		4	/* wait ack options request	*/
#define	TS_WACK_CREQ		5	/* waiting ack of CONN_REQ	*/
#define	TS_WCON_CREQ		6	/* waiting confirm of CONN_REQ	*/
#define	TS_WRES_CIND		7	/* waiting response of CONN_IND	*/
#define	TS_WACK_CRES		8	/* waiting ack of CONN_RES	*/
#define	TS_DATA_XFER		9	/* data transfer		*/
#define	TS_WIND_ORDREL		10	/* releasing rd but not wr	*/
#define	TS_WREQ_ORDREL		11	/* wait to release wr but not rd */
#define	TS_WACK_DREQ6		12	/* waiting ack of DISCON_REQ	*/
#define	TS_WACK_DREQ7		13	/* waiting ack of DISCON_REQ	*/
#define	TS_WACK_DREQ9		14	/* waiting ack of DISCON_REQ	*/
#define	TS_WACK_DREQ10		15	/* waiting ack of DISCON_REQ	*/
#define	TS_WACK_DREQ11		16	/* waiting ack of DISCON_REQ	*/

#define	TS_NOSTATES		17


/*
 * The following structure definitions define the format of the
 * stream message block of the above primitives.
 * (everything is declared t_scalar_t to ensure proper alignment
 * across different machines)
 */

/* connection request */

struct T_conn_req {
	t_scalar_t	PRIM_type;	/* always T_CONN_REQ		*/
	t_scalar_t	DEST_length;	/* dest addr length		*/
	t_scalar_t	DEST_offset;	/* dest addr offset		*/
	t_scalar_t	OPT_length;	/* options length		*/
	t_scalar_t	OPT_offset;	/* options offset		*/
};

/* connect response */

struct T_conn_res {
	t_scalar_t	PRIM_type;	/* always T_CONN_RES		*/
	t_uscalar_t	ACCEPTOR_id;	/* id of accepting endpoint	*/
	t_scalar_t	OPT_length;	/* options length		*/
	t_scalar_t	OPT_offset;	/* options offset		*/
	t_scalar_t	SEQ_number;	/* sequence number		*/
};

/* disconnect request */

struct T_discon_req {
	t_scalar_t	PRIM_type;	/* always T_DISCON_REQ		*/
	t_scalar_t	SEQ_number;	/* sequnce number		*/
};

/* data request */

struct T_data_req {
	t_scalar_t	PRIM_type;	/* always T_DATA_REQ		*/
	t_scalar_t	MORE_flag;	/* more data			*/
};

/* expedited data request */

struct T_exdata_req {
	t_scalar_t	PRIM_type;	/* always T_EXDATA_REQ		*/
	t_scalar_t	MORE_flag;	/* more data			*/
};

/* information request */

struct T_info_req {
	t_scalar_t	PRIM_type;	/* always T_INFO_REQ		*/
};

/* bind request */

struct T_bind_req {
	t_scalar_t	PRIM_type;	/* always T_BIND_REQ	*/
	t_scalar_t	ADDR_length;	/* addr length		*/
	t_scalar_t	ADDR_offset;	/* addr offset		*/
	t_uscalar_t	CONIND_number;	/* connect indications requested */
};

/* unbind request */

struct T_unbind_req {
	t_scalar_t	PRIM_type;	/* always T_UNBIND_REQ		*/
};

/* unitdata request */

struct T_unitdata_req {
	t_scalar_t	PRIM_type;	/* always T_UNITDATA_REQ	*/
	t_scalar_t	DEST_length;	/* dest addr length		*/
	t_scalar_t	DEST_offset;	/* dest addr offset		*/
	t_scalar_t	OPT_length;	/* options length		*/
	t_scalar_t	OPT_offset;	/* options offset		*/
};

/* manage options request */

struct T_optmgmt_req {
	t_scalar_t	PRIM_type;	/* always T_OPTMGMT_REQ		*/
	t_scalar_t	OPT_length;	/* options length		*/
	t_scalar_t	OPT_offset;	/* options offset		*/
	t_scalar_t	MGMT_flags;	/* options flags		*/
};

/* orderly release request */

struct T_ordrel_req {
	t_scalar_t	PRIM_type;	/* always T_ORDREL_REQ		*/
};

/* protocol address request */

struct T_addr_req {
	t_scalar_t	PRIM_type;	/* always T_ADDR_REQ		*/
};

/* connect indication */

struct T_conn_ind {
	t_scalar_t	PRIM_type;	/* always T_CONN_IND		*/
	t_scalar_t	SRC_length;	/* src addr length		*/
	t_scalar_t	SRC_offset;	/* src addr offset		*/
	t_scalar_t	OPT_length;	/* option length		*/
	t_scalar_t	OPT_offset;	/* option offset		*/
	t_scalar_t	SEQ_number;	/* sequnce number		*/
};

/* connect confirmation */

struct T_conn_con {
	t_scalar_t	PRIM_type;	/* always T_CONN_CON		*/
	t_scalar_t	RES_length;	/* responding addr length	*/
	t_scalar_t	RES_offset;	/* responding addr offset	*/
	t_scalar_t	OPT_length;	/* option length		*/
	t_scalar_t	OPT_offset;	/* option offset		*/
};

/* disconnect indication */

struct T_discon_ind {
	t_scalar_t	PRIM_type;	/* always T_DISCON_IND		*/
	t_scalar_t	DISCON_reason;	/* disconnect reason		*/
	t_scalar_t	SEQ_number;	/* sequnce number		*/
};

/* data indication */

struct T_data_ind {
	t_scalar_t	PRIM_type;	/* always T_DATA_IND		*/
	t_scalar_t	MORE_flag;	/* more data			*/
};

/* expedited data indication */

struct T_exdata_ind {
	t_scalar_t	PRIM_type;	/* always T_EXDATA_IND		*/
	t_scalar_t	MORE_flag;	/* more data			*/
};

/* information acknowledgment */

struct T_info_ack {
	t_scalar_t	PRIM_type;	/* always T_INFO_ACK		*/
	t_scalar_t	TSDU_size;	/* max TSDU size		*/
	t_scalar_t	ETSDU_size;	/* max ETSDU size		*/
	t_scalar_t	CDATA_size;	/* max connect data size	*/
	t_scalar_t	DDATA_size;	/* max discon data size		*/
	t_scalar_t	ADDR_size;	/* address size			*/
	t_scalar_t	OPT_size;	/* options size			*/
	t_scalar_t	TIDU_size;	/* max TIDU size		*/
	t_scalar_t	SERV_type;	/* provider service type	*/
	t_scalar_t	CURRENT_state;	/* current state		*/
	t_scalar_t	PROVIDER_flag;	/* provider flags		*/
};

/*
 * The following are definitions of flags available to the transport
 * provider to set in the PROVIDER_flag field of the T_info_ack
 * structure.
 */

#if _SUN_TPI_VERSION > 0
#define	SENDZERO	0x0001	/* provider can handle --length TSDUs */

#define	OLD_SENDZERO	0x1000	/* reserved for compatibility with */
				/* old providers- old value of */
				/* SENDZERO defined in <sys/timod.h> */
#else
#define	SENDZERO	0x1000	/* old SENDZERO value */
#endif /* _SUN_TPI_VERSION > 0 */

#define	EXPINLINE	0x0002	/* provider wants ETSDUs in band 0 */
#define	XPG4_1		0x0004	/* provider supports TPI modifications */
				/* motivated by and in conjunction with */
				/* XPG4 XTI and supports */
				/* primitives T_ADDR_REQ & T_ADDR_ACK */
				/* primitives O_T_BIND_REQ & T_BIND_REQ */
				/* primitives O_T_OPTMGMT_REQ & T_OPTMGMT_REQ */


/* bind acknowledgment */

struct T_bind_ack {
	t_scalar_t	PRIM_type;	/* always T_BIND_ACK		*/
	t_scalar_t	ADDR_length;	/* addr length			*/
	t_scalar_t	ADDR_offset;	/* addr offset			*/
	t_uscalar_t	CONIND_number;	/* connect ind to be queued	*/
};

/* error acknowledgment */

struct T_error_ack {
	t_scalar_t	PRIM_type;	/* always T_ERROR_ACK		*/
	t_scalar_t	ERROR_prim;	/* primitive in error		*/
	t_scalar_t	TLI_error;	/* TLI error code		*/
	t_scalar_t	UNIX_error;	/* UNIX error code		*/
};

/* ok acknowledgment */

struct T_ok_ack {
	t_scalar_t	PRIM_type;	/* always T_OK_ACK		*/
	t_scalar_t	CORRECT_prim;	/* correct primitive		*/
};

/* unitdata indication */

struct T_unitdata_ind {
	t_scalar_t	PRIM_type;	/* always T_UNITDATA_IND	*/
	t_scalar_t	SRC_length;	/* source addr length		*/
	t_scalar_t	SRC_offset;	/* source addr offset		*/
	t_scalar_t	OPT_length;	/* options length		*/
	t_scalar_t	OPT_offset;	/* options offset		*/
};

/* unitdata error indication */

struct T_uderror_ind {
	t_scalar_t	PRIM_type;	/* always T_UDERROR_IND		*/
	t_scalar_t	DEST_length;	/* dest addr length		*/
	t_scalar_t	DEST_offset;	/* dest addr offset		*/
	t_scalar_t	OPT_length;	/* options length		*/
	t_scalar_t	OPT_offset;	/* options offset		*/
	t_scalar_t	ERROR_type;	/* error type			*/
};

/* manage options ack */

struct T_optmgmt_ack {
	t_scalar_t	PRIM_type;	/* always T_OPTMGMT_ACK		*/
	t_scalar_t	OPT_length;	/* options length		*/
	t_scalar_t	OPT_offset;	/* options offset		*/
	t_scalar_t	MGMT_flags;	/* managment flags		*/
};

/* orderly release indication */

struct T_ordrel_ind {
	t_scalar_t	PRIM_type;	/* always T_ORDREL_IND		*/
};


/* protocol address acknowledgment */

struct T_addr_ack {
	t_scalar_t	PRIM_type;	/* always T_ADDR_ACK		*/
	t_scalar_t	LOCADDR_length;	/* length of local address	*/
	t_scalar_t	LOCADDR_offset;	/* offset of local address	*/
	t_scalar_t	REMADDR_length;	/* length of remote address	*/
	t_scalar_t	REMADDR_offset;	/* offset of remote address	*/
};

#if _SUN_TPI_VERSION > 1
/*
 * Capability request and ack.  These primitives are optional and
 * subsume the functionality of T_INFO_{REQ,ACK}.
 */
struct T_capability_req {
	t_scalar_t	PRIM_type;	/* always T_CAPABILITY_REQ 	*/
	t_uscalar_t	CAP_bits1;	/* capability bits #1		*/
};

struct T_capability_ack {
	t_scalar_t	PRIM_type;	/* always T_CAPABILITY_ACK 	*/
	t_uscalar_t	CAP_bits1;	/* capability bits #1		*/
	struct T_info_ack
			INFO_ack;	/* info acknowledgement		*/
	t_uscalar_t	ACCEPTOR_id;	/* accepting endpoint id	*/
};

#define	TC1_INFO	(1u << 0)	/* Info request/ack		*/
#define	TC1_ACCEPTOR_ID	(1u << 1)	/* Acceptor_id request/ack	*/
#define	TC1_CAP_BITS2	(1u << 31)	/* Reserved for future use	*/

#endif /* _SUN_TPI_VERSION > 1 */

#ifdef _KERNEL
/*
 * Private Sun TPI extensions.
 */

/* data (with options) request */
struct T_optdata_req {
	t_scalar_t	PRIM_type;	/* always T_OPTDATA_REQ		*/
	t_scalar_t	DATA_flag;	/* flags like "more data"	*/
	t_scalar_t	OPT_length;	/* options length		*/
	t_scalar_t	OPT_offset;	/* options offset		*/
};

/* data (with options) indication */
struct T_optdata_ind {
	t_scalar_t	PRIM_type;	/* always T_OPTDATA_IND		*/
	t_scalar_t	DATA_flag;	/* flags like "more data"	*/
	t_scalar_t	OPT_length;	/* options length		*/
	t_scalar_t	OPT_offset;	/* options offset		*/
};
#endif /* _KERNEL */

/*
 * The following is a union of the primitives
 */
union T_primitives {
	t_scalar_t		type;		/* primitive type	*/
	struct T_conn_req	conn_req;	/* connect request	*/
	struct T_conn_res	conn_res;	/* connect response	*/
	struct T_discon_req	discon_req;	/* disconnect request	*/
	struct T_data_req	data_req;	/* data request		*/
	struct T_exdata_req	exdata_req;	/* expedited data req	*/
	struct T_info_req	info_req;	/* information req	*/
	struct T_bind_req	bind_req;	/* bind request		*/
	struct T_unbind_req	unbind_req;	/* unbind request	*/
	struct T_unitdata_req	unitdata_req;	/* unitdata requset	*/
	struct T_optmgmt_req	optmgmt_req;	/* manage opt req	*/
	struct T_ordrel_req	ordrel_req;	/* orderly rel req	*/
	struct T_addr_req	addr_req;	/* address request	*/
	struct T_conn_ind	conn_ind;	/* connect indication	*/
	struct T_conn_con	conn_con;	/* connect corfirm	*/
	struct T_discon_ind	discon_ind;	/* discon indication	*/
	struct T_data_ind	data_ind;	/* data indication	*/
	struct T_exdata_ind	exdata_ind;	/* expedited data ind	*/
	struct T_info_ack	info_ack;	/* info ack		*/
	struct T_bind_ack	bind_ack;	/* bind ack		*/
	struct T_error_ack	error_ack;	/* error ack		*/
	struct T_ok_ack		ok_ack;		/* ok ack		*/
	struct T_unitdata_ind	unitdata_ind;	/* unitdata ind		*/
	struct T_uderror_ind	uderror_ind;	/* unitdata error ind	*/
	struct T_optmgmt_ack	optmgmt_ack;	/* manage opt ack	*/
	struct T_ordrel_ind	ordrel_ind;	/* orderly rel ind	*/
	struct T_addr_ack	addr_ack;	/* address ack		*/
#if _SUN_TPI_VERSION > 1
	struct T_capability_req	capability_req;	/* capability req	*/
	struct T_capability_ack	capability_ack;	/* capability ack	*/
#endif /* _SUN_TPI_VERSION > 1 */
#ifdef _KERNEL
	struct T_optdata_req	optdata_req;	/* option data request	*/
	struct T_optdata_ind	optdata_ind;	/* option data ind	*/
#endif /* _KERNEL */
};

/*
 * T_opthdr structure used to pack options in T_OPTMGMT_REQ/T_OPTMGMT_ACK
 * messages in buffer delimited by [OPT_offset,OPT_length] fields in
 * T_optmgmt_req/T_optmgmt_ack primitives
 * Needs to be on 32-bit word aligned boundary.
 *
 *
 * Note: O_T_OPTMGMT_REQ primitive need not use this data structure for
 *	for packing options. The format of option buffer is undefined
 *	by TPI for O_T_OPTMGMT_REQ.
 *
 * |<--------------first option---------------->|     |<--second option--...
 * ______________________________________ _ _ _ ____________________________
 * | len | level | name | status |  value.......| / / | len ...
 * -------------------------------------- - - - ----------------------------
 * |32bit| 32bit |32bit |  32bit |                 ^  | 32bit...
 *                                                 |
 *                                                 |
 *                                        alignment characters
 */
struct T_opthdr {
	t_uscalar_t	len;	/* total length of option (header+value) */
	t_uscalar_t	level;	/* protocol level */
	t_uscalar_t	name;	/* option name */
	t_uscalar_t	status;	/* status value */
	/* followed by option value */
};

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_TIHDR_H */
