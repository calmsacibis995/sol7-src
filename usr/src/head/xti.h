/*	Copyright (c) 1996 Sun Microsystems, Inc.	*/
/*	  All Rights Reserved  	*/


#ifndef _XTI_H
#define	_XTI_H

#pragma ident	"@(#)xti.h	1.5	97/04/15 SMI"

/*
 * The following include file has declarations needed by both the kernel
 * level transport providers and the user level library. This file includes
 * it to expose its namespaces to XTI user level interface.
 */
#include <sys/tpicommon.h>

/*
 * Include XTI interface level options management declarations
 */
#include <sys/xti_xtiopt.h>

/*
 * Include declarations related to OSI transport and management data
 * structures. Note: XTI spec currently requires these to be exposed
 * through the generic interface header.
 */
#include <sys/xti_osi.h>

/*
 * Include declarations related to Internet Protocol Suite.
 * Note: XTI spec currently requires these to be exposed
 * through the generic interface header.
 */
#include <sys/xti_inet.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * The following t_errno error codes are included in the namespace by
 * inclusion of <sys/tpicommon.h> above. The english language error strings
 * associated with the error values are reproduced here for easy reference.
 *
 * Error		Value	Error message string
 * ----			-----	--------------------
 * TBADADDR		1	Incorrect address format
 * TBADOPT		2	Incorrect options format
 * TACCES		3	Illegal permissions
 * TBADF		4	Illegal file descriptor
 * TNOADDR		5	Couldn't allocate address
 * TOUTSTATE		6	Routine will place interface out of state
 * TBADSEQ		7	Illegal called/calling sequence number
 * TSYSERR		8	System error
 * TLOOK		9	An event requires attention
 * TBADDATA		10	Illegal amount of data
 * TBUFOVFLW		11	Buffer not large enough
 * TFLOW		12	Can't send message - (blocked)
 * TNODATA		13	No message currently available
 * TNODIS		14	Disconnect message not found
 * TNOUDERR		15	Unitdata error message not found
 * TBADFLAG		16	Incorrect flags specified
 * TNOREL		17	Orderly release message not found
 * TNOTSUPPORT		18	Primitive not supported by provider
 * TSTATECHNG		19	State is in process of changing
 * TNOSTRUCTYPE		20	Unsupported structure type requested
 * TBADNAME		21	Invalid transport provider name
 * TBADQLEN		22	Listener queue length limit is zero
 * TADDRBUSY		23	Transport address is in use
 * TINDOUT		24	Outstanding connection indications
 * TPROVMISMATCH	25	Listener-acceptor transport provider mismatch
 * TRESQLEN		26	Connection acceptor has listen queue length
 *				limit greater than zero
 * TRESADDR		27	Connection acceptor-listener addresses not
 *				same but required by transport
 * TQFULL		28	Incoming connection queue is full
 * TPROTO		29	Protocol error on transport primitive
 *
 */

/*
 * The following are the events returned by t_look
 */
#define	T_LISTEN	0x0001	/* connection indication received	*/
#define	T_CONNECT	0x0002	/* connect confirmation received	*/
#define	T_DATA		0x0004	/* normal data received			*/
#define	T_EXDATA	0x0008	/* expedited data received		*/
#define	T_DISCONNECT	0x0010	/* disconnect received			*/
#define	T_UDERR		0x0040	/* data gram error indication		*/
#define	T_ORDREL	0x0080	/* orderly release indication		*/
#define	T_GODATA	0x0100	/* sending normal data is again possible */
#define	T_GOEXDATA	0x0200	/* sending expedited data is again possible */

/*
 * Flags for data primitives
 */
#define	T_MORE		0x001	/* more data		*/
#define	T_EXPEDITED	0x002	/* expedited data	*/

/*
 * XTI error return
 */
#if defined(_REENTRANT) || defined(_TS_ERRNO)
extern int	*__t_errno();
#define	t_errno (*(__t_errno()))
#else
extern int t_errno;
#endif	/* defined(_REENTRANT) || defined(_TS_ERRNO) */

/*
 * Translate source level interface to binary entry point names.
 *
 * Note: This is done to maintain co-existence of TLI and XTI
 * interfaces which have identical names for most functions but
 * different semantics. The XTI names are moved to the different
 * prefix space in the ABI. The #ifdef is required to make use of
 * of the compiler feature  to allow refeinition of external names
 * where available. Otherwise a simple #define is used when this
 * header is used with other compilers.
 * The use of #define also has the effect of renaming all names (not
 * just function names) to the new name. The TLI function names
 * (e.g. t_bind) can have identical names for structure names
 * (e.g struct t_bind). Therefore, this redefinition of names needs
 * to be before all structure and function name declarations in the header.
 */

#ifdef __PRAGMA_REDEFINE_EXTNAME

#pragma redefine_extname t_accept	_xti_accept
#pragma redefine_extname t_alloc	_xti_alloc
#pragma redefine_extname t_bind		_xti_bind
#pragma redefine_extname t_close	_xti_close
#pragma redefine_extname t_connect	_xti_connect
#pragma redefine_extname t_error	_xti_error
#pragma redefine_extname t_free		_xti_free
#pragma redefine_extname t_getinfo	_xti_getinfo
#pragma redefine_extname t_getstate	_xti_getstate
#pragma redefine_extname t_getprotaddr	_xti_getprotaddr
#pragma redefine_extname t_listen	_xti_listen
#pragma redefine_extname t_look		_xti_look
#pragma redefine_extname t_open		_xti_open
#pragma redefine_extname t_optmgmt	_xti_optmgmt
#pragma redefine_extname t_rcv		_xti_rcv
#pragma redefine_extname t_rcvconnect	_xti_rcvconnect
#pragma redefine_extname t_rcvdis	_xti_rcvdis
#pragma redefine_extname t_rcvrel	_xti_rcvrel
#pragma redefine_extname t_rcvudata	_xti_rcvudata
#pragma redefine_extname t_rcvuderr	_xti_rcvuderr
#pragma redefine_extname t_snd		_xti_snd
#pragma redefine_extname t_snddis	_xti_snddis
#pragma redefine_extname t_sndrel	_xti_sndrel
#pragma redefine_extname t_sndudata	_xti_sndudata
#pragma redefine_extname t_strerror	_xti_strerror
#pragma redefine_extname t_sync		_xti_sync
#pragma redefine_extname t_unbind	_xti_unbind

#else /* __PRAGMA_REDEFINE_EXTNAME */

#define	t_accept	_xti_accept
#define	t_alloc		_xti_alloc
#define	t_bind		_xti_bind
#define	t_close		_xti_close
#define	t_connect	_xti_connect
#define	t_error		_xti_error
#define	t_free		_xti_free
#define	t_getinfo	_xti_getinfo
#define	t_getstate	_xti_getstate
#define	t_getprotaddr	_xti_getprotaddr
#define	t_listen	_xti_listen
#define	t_look		_xti_look
#define	t_open		_xti_open
#define	t_optmgmt	_xti_optmgmt
#define	t_rcv		_xti_rcv
#define	t_rcvconnect	_xti_rcvconnect
#define	t_rcvdis	_xti_rcvdis
#define	t_rcvrel	_xti_rcvrel
#define	t_rcvudata	_xti_rcvudata
#define	t_rcvuderr	_xti_rcvuderr
#define	t_snd		_xti_snd
#define	t_snddis	_xti_snddis
#define	t_sndrel	_xti_sndrel
#define	t_sndudata	_xti_sndudata
#define	t_strerror	_xti_strerror
#define	t_sync		_xti_sync
#define	t_unbind	_xti_unbind

#endif /* __PRAGMA_REDEFINE_EXTNAME */

/*
 * protocol specific service limits
 */

struct t_info {
	t_scalar_t addr;	/* max size of protocol address		*/
	t_scalar_t options;	/* max size of protocol options		*/
	t_scalar_t tsdu;	/* max size of max transport service	*/
				/* data unit	*/
	t_scalar_t etsdu;	/* max size of max expedited tsdu	*/
	t_scalar_t connect;	/* max data for connection primitives	*/
	t_scalar_t discon;	/* max data for disconnect primitives	*/
	t_scalar_t servtype;	/* provider service type		*/
	t_scalar_t flags;	/* other info about transport providers	*/
};

/*
 * Flags definitions for the t_info structure
 */
#define	T_SENDZERO	0x001	/* supports 0-length TSDUs */
/*
 * netbuf structure
 */

struct netbuf {
	unsigned int maxlen;
	unsigned int len;
	char *buf;
};

/*
 * t_opthdr structure
 */
struct t_opthdr {
	t_uscalar_t len;	/* total length of option */
	t_uscalar_t level;	/* protocol level */
	t_uscalar_t name;	/* option name */
	t_uscalar_t status;	/* status value */
	/* followed by option value */
};

/*
 * t_bind - format of the addres and options arguments of bind
 */

struct t_bind {
	struct netbuf	addr;
	t_uscalar_t	qlen;
};

/*
 * options management
 */
struct t_optmgmt {
	struct netbuf	opt;
	t_scalar_t	flags;
};

/*
 * disconnect structure
 */
struct t_discon {
	struct netbuf udata;		/* user data		*/
	t_scalar_t reason;		/* reason code		*/
	t_scalar_t sequence;		/* sequence number	*/
};

/*
 * call structure
 */
struct t_call {
	struct netbuf addr;		/*  address		*/
	struct netbuf opt;		/* options		*/
	struct netbuf udata;		/* user data		*/
	t_scalar_t sequence;		/* sequence number	*/
};

/*
 * data gram structure
 */
struct t_unitdata {
	struct netbuf addr;		/*  address		*/
	struct netbuf opt;		/* options		*/
	struct netbuf udata;		/* user data		*/
};

/*
 * unitdata error
 */
struct t_uderr {
	struct netbuf addr;		/* address		*/
	struct netbuf opt;		/* options 		*/
	t_scalar_t	error;		/* error code		*/
};

/*
 * The following are structure types used when dynamically
 * allocating the above structures via t_structalloc().
 */
#define	T_BIND		1		/* struct t_bind	*/
#define	T_OPTMGMT	2		/* struct t_optmgmt	*/
#define	T_CALL		3		/* struct t_call	*/
#define	T_DIS		4		/* struct t_discon	*/
#define	T_UNITDATA	5		/* struct t_unitdata	*/
#define	T_UDERROR	6		/* struct t_uderr	*/
#define	T_INFO		7		/* struct t_info	*/

/*
 * The following bits specify which fields of the above
 * structures should be allocated by t_alloc().
 */
#define	T_ADDR	0x01			/* address		*/
#define	T_OPT	0x02			/* options		*/
#define	T_UDATA	0x04			/* user data		*/
#define	T_ALL	0xffff			/* all the above fields */


/*
 * the following are the states for the user
 */

#define	T_UNINIT	0		/* uninitialized		*/
#define	T_UNBND		1		/* unbound			*/
#define	T_IDLE		2		/* idle				*/
#define	T_OUTCON	3		/* outgoing connection pending 	*/
#define	T_INCON		4		/* incoming connection pending	*/
#define	T_DATAXFER	5		/* data transfer		*/
#define	T_OUTREL	6		/* outgoing release pending	*/
#define	T_INREL		7		/* incoming release pending	*/




#define	T_UNUSED		-1
#define	T_NULL			0


/*
 * Allegedly general purpose constant. Used with (and needs to be bitwise
 * distinct from) T_NOPROTECT, T_PASSIVEPROTECT and T_ACTIVEPROTECT
 * which are OSI specific constants but part of this header (defined
 * in <xti_osi.h> which is included in this header for historical
 * XTI specification reasons)
 */
#define	T_ABSREQ		0x8000

/*
 * General definitions for option management
 */

#define	T_ALIGN(p)	(((t_uscalar_t)(p) + (sizeof (t_scalar_t)-1))\
						& ~(sizeof (t_scalar_t)-1))
#define	OPT_NEXTHDR(pbuf, buflen, popt) \
		    (((char *)(popt) + T_ALIGN((popt)->len) <\
		    ((char *)(pbuf) + buflen)) ? \
		    (struct t_opthdr *)((char *)(popt)+T_ALIGN((popt)->len)) :\
		    (struct t_opthdr *)0)

/*
 * XTI LIBRARY FUNCTIONS
 */

#if defined(__STDC__)

extern int t_accept(int fildes, int resfd, const struct t_call *call);
extern char *t_alloc(int fildes, int struct_type, int fields);
extern int t_bind(int fildes, const struct t_bind *req, struct t_bind *ret);
extern int t_close(int fildes);
extern int t_connect(int fildes, const struct t_call *sndcall,
	struct t_call *rcvcall);
extern int t_error(const char *errmsg);
extern int t_free(void *ptr, int struct_type);
extern int t_getinfo(int fildes, struct t_info *info);
extern int t_getstate(int fildes);
extern int t_getprotaddr(int filedes, struct t_bind *boundaddr,
	struct t_bind *peer);
extern int t_listen(int fildes, struct t_call *call);
extern int t_look(int fildes);
extern int t_open(const char *path, int oflag, struct t_info *info);
extern int t_optmgmt(int fildes, const struct t_optmgmt *req,
	struct t_optmgmt *ret);
extern int t_rcv(int fildes, void *buf, unsigned int nbytes, int *flags);
extern int t_rcvconnect(int fildes, struct t_call *call);
extern int t_rcvdis(int fildes, struct t_discon *discon);
extern int t_rcvrel(int fildes);
extern int t_rcvudata(int fildes, struct t_unitdata *unitdata, int *flags);
extern int t_rcvuderr(int fildes, struct t_uderr *uderr);
extern int t_snd(int fildes, void *buf, unsigned int nbytes, int flags);
extern int t_snddis(int fildes, const struct t_call *call);
extern int t_sndrel(int fildes);
extern int t_sndudata(int fildes, const struct t_unitdata *unitdata);
extern const char *t_strerror(int errnum);
extern int t_sync(int fildes);
extern int t_unbind(int fildes);

#endif /* __STDC__ */

#ifdef	__cplusplus
}
#endif

#endif	/* _XTI_H */
