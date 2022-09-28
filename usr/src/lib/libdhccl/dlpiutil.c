/*
 * dlpiutil.c: "DLPI utilities".
 *
 * SYNOPSIS
 *
 * DESCRIPTION
 *
 * COPYRIGHT
 *    Copyright 1992-1996 Competitive Automation. All Rights Reserved
 */

#pragma ident	"@(#)dlpiutil.c	1.5	96/11/26 SMI"

#include <string.h>
#include "ca_dlpi.h"
#include <stropts.h>
#include <stdio.h>
#include "unixgen.h"
#include "utils.h"
#include "utiltxt.h"

#ifdef	DEBUG
extern char *sys_errlist[];

static const char *statestr[] = {
	"DL_UNATTACHED",		"DL_ATTACH_PENDING",
	"DL_DETACH_PENDING",		"DL_UNBOUND",
	"DL_BIND_PENDING",		"DL_UNBIND_PENDING",
	"DL_IDLE",			"DL_UDQOS_PENDING",
	"DL_OUTCON_PENDING",		"DL_INCON_PENDING",
	"DL_CONN_RES_PENDING",		"DL_DATAXFER",
	"DL_USER_RESET_PENDING",	"DL_PROV_RESET_PENDING",
	"DL_RESET_RES_PENDING",		"DL_DISCON8_PENDING",
	"DL_DISCON9_PENDING",		"DL_DISCON11_PENDING",
	"DL_DISCON12_PENDING",		"DL_DISCON13_PENDING",
	"DL_SUBS_BIND_PND",		"DL_SUBS_UNBIND_PND",
	"unknown"
};

static int stateary[] = {
	DL_UNATTACHED,		DL_ATTACH_PENDING,
	DL_DETACH_PENDING,	DL_UNBOUND,
	DL_BIND_PENDING,	DL_UNBIND_PENDING,
	DL_IDLE,		DL_UDQOS_PENDING,
	DL_OUTCON_PENDING,	DL_INCON_PENDING,
	DL_CONN_RES_PENDING,	DL_DATAXFER,
	DL_USER_RESET_PENDING,	DL_PROV_RESET_PENDING,
	DL_RESET_RES_PENDING,	DL_DISCON8_PENDING,
	DL_DISCON9_PENDING,	DL_DISCON11_PENDING,
	DL_DISCON12_PENDING,	DL_DISCON13_PENDING,
	DL_SUBS_BIND_PND,	DL_SUBS_UNBIND_PND,
	-1
};

static const char *
DLPIstateStr(int state)
{
	register int n;

	for (n = 0; stateary[n] >= 0 && stateary[n] != state; n++)
		;
	return (statestr[n]);
}

static const char *lerrstr[] = {
	"Improper permissions for request",
	"DLSAP addr in improper format or invalid",
	"Seq number not from outstand DL_CONN_IND",
	"User data exceeded provider limit",
	"Specified PPA was invalid",
	"Primitive received not known by provider",
	"QOS parameters contained invalid values",
	"QOS structure type is unknown",
	"Bad LSAP selector",
	"Token used not an active stream",
	"Attempted second bind with dl_max_conind",
	"Physical Link initialization failed",
	"Provider couldn't allocate alt. address",
	"Physical Link not initialized",
	"Primitive issued in improper state",
	"UNIX system error occurred",
	"Requested serv. not supplied by provider",
	"Previous data unit could not be delivered",
	"Primitive is known but not supported",
	"limit exceeded *"
	"Promiscuous mode not enabled",
	"Other streams for PPA in post-attached",
	"Automatic handling XID&TEST not supported",
	"Automatic handling of XID not supported",
	"Automatic handling of TEST not supported",
	"Automatic handling of XID response",
	"AUtomatic handling of TEST response",
	"Pending outstanding connect indications",
	"unknown"
};

static const char *errstr[] = {
	"DL_ACCESS",		"DL_BADADDR",		"DL_BADCORR",
	"DL_BADDATA",		"DL_BADPPA",		"DL_BADPRIM",
	"DL_BADQOSPARAM",	"DL_BADQOSTYPE",	"DL_BADSAP",
	"DL_BADTOKEN",		"DL_BOUND",		"DL_INITFAILED",
	"DL_NOADDR",		"DL_NOTINIT",		"DL_OUTSTATE",
	"DL_SYSERR",		"DL_UNSUPPORTED",	"DL_UNDELIVERABLE",
	"DL_NOTSUPPORTED",	"DL_TOOMANY",		"DL_NOTENAB",
	"DL_BUSY",		"DL_NOAUTO",		"DL_NOXIDAUTO",
	"DL_NOTESTAUTO",	"DL_XIDAUTO",		"DL_TESTAUTO",
	"DL_PENDING",
	"unknown"
};

static int errary[] = {
	DL_ACCESS,		DL_BADADDR,	DL_BADCORR,
	DL_BADDATA,		DL_BADPPA,	DL_BADPRIM,
	DL_BADQOSPARAM,		DL_BADQOSTYPE,	DL_BADSAP,
	DL_BADTOKEN,		DL_BOUND,	DL_INITFAILED,
	DL_NOADDR,		DL_NOTINIT,	DL_OUTSTATE,
	DL_SYSERR,		DL_UNSUPPORTED,	DL_UNDELIVERABLE,
	DL_NOTSUPPORTED,	DL_TOOMANY,	DL_NOTENAB,
	DL_BUSY,		DL_NOAUTO,	DL_NOXIDAUTO,
	DL_NOTESTAUTO,		DL_XIDAUTO,	DL_TESTAUTO,
	DL_PENDING,
	-1
};

static const char *
DLPIerrStr(int err)
{
	register int n;

	for (n = 0; errary[n] >= 0 && errary[n] != err; n++)
		;
	return (errstr[n]);
}

static const char *
DLPIlongerrStr(int err)
{
	register int n;

	for (n = 0; errary[n] >= 0 && errary[n] != err; n++)
		;
	return (lerrstr[n]);
}

static const char *mediastr[] = {
	"DL_CSMACD",	"DL_TPB",	"DL_TPR",	"DL_METRO",
	"DL_ETHER",	"DL_HDLC",	"DL_CHAR",	"DL_CTCA",
	"DL_FDDI",	"DL_OTHER",	"unknown"
};

static int mediaary[] = {
DL_CSMACD,	DL_TPB,		DL_TPR,		DL_METRO,	DL_ETHER,
DL_HDLC,	DL_CHAR,	DL_CTCA,	DL_FDDI,	DL_OTHER,
-1
};

static const char *
DLPImediaStr(int medium)
{
	register int n;

	for (n = 0; mediaary[n] >= 0 && mediaary[n] != medium; n++)
		;
	return (mediastr[n]);
}

static const char *stylestr[] = {
	"DL_STYLE1",	"DL_STYLE2",	"unknown"
};

static int styleary[] = {
	DL_STYLE1,	DL_STYLE2,	-1
};

static const char *
DLPIstyleStr(int style)
{
	register int n;

	for (n = 0; styleary[n] >= 0 && styleary[n] != style; n++)
		;
	return (stylestr[n]);
}

static const char *servicestr[] = {
	"DL_CODLS",	"DL_CLDLS",	"DL_ACLDLS",	"unknown"
};

static int serviceary[] = {
	DL_CODLS,	DL_CLDLS,	DL_ACLDLS,	-1
};

static const char *
DLPIserviceStr(int service)
{
	register int n;

	for (n = 0; serviceary[n] >= 0 && serviceary[n] != service; n++)
		;
	return (servicestr[n]);
}

static const char *primitivestr[] = {
	"DL_INFO_REQ",			"DL_INFO_ACK",
	"DL_ATTACH_REQ",		"DL_DETACH_REQ",
	"DL_BIND_REQ",			"DL_BIND_ACK",
	"DL_UNBIND_REQ",		"DL_OK_ACK",
	"DL_ERROR_ACK",			"DL_SUBS_BIND_REQ",
	"DL_SUBS_BIND_ACK",		"DL_SUBS_UNBIND_REQ",
	"DL_ENABMULTI_REQ",		"DL_DISABMULTI_REQ",
	"DL_PROMISCON_REQ",		"DL_PROMISCOFF_REQ",
	"DL_UNITDATA_REQ",		"DL_UNITDATA_IND",
	"DL_UDERROR_IND",		"DL_UDQOS_REQ",
	"DL_CONNECT_REQ",		"DL_CONNECT_IND",
	"DL_CONNECT_RES",		"DL_CONNECT_CON",
	"DL_TOKEN_REQ",			"DL_TOKEN_ACK",
	"DL_DISCONNECT_REQ",		"DL_DISCONNECT_IND",
	"DL_RESET_REQ",			"DL_RESET_IND",
	"DL_RESET_RES",			"DL_RESET_CON",
	"DL_DATA_ACK_REQ",		"DL_DATA_ACK_IND",
	"DL_DATA_ACK_STATUS_IND",	"DL_REPLY_REQ",
	"DL_REPLY_IND",			"DL_REPLY_STATUS_IND",
	"DL_REPLY_UPDATE_REQ",		"DL_REPLY_UPDATE_STATUS_IND",
	"DL_XID_REQ",			"DL_XID_IND",
	"DL_XID_RES",			"DL_XID_CON",
	"DL_TEST_REQ",			"DL_TEST_IND",
	"DL_TEST_RES",			"DL_TEST_CON",
	"DL_PHYS_ADDR_REQ",		"DL_PHYS_ADDR_ACK",
	"DL_SET_PHYS_ADDR_REQ",		"DL_GET_STATISTICS_REQ",
	"DL_GET_STATISTICS_ACK",	"unknown"
};

static int primitiveary[] = {
	DL_INFO_REQ,		DL_INFO_ACK,
	DL_ATTACH_REQ,		DL_DETACH_REQ,
	DL_BIND_REQ,		DL_BIND_ACK,
	DL_UNBIND_REQ,		DL_OK_ACK,
	DL_ERROR_ACK,		DL_SUBS_BIND_REQ,
	DL_SUBS_BIND_ACK,	DL_SUBS_UNBIND_REQ,
	DL_ENABMULTI_REQ,	DL_DISABMULTI_REQ,
	DL_PROMISCON_REQ,	DL_PROMISCOFF_REQ,
	DL_UNITDATA_REQ,	DL_UNITDATA_IND,
	DL_UDERROR_IND,		DL_UDQOS_REQ,
	DL_CONNECT_REQ,		DL_CONNECT_IND,
	DL_CONNECT_RES,		DL_CONNECT_CON,
	DL_TOKEN_REQ,		DL_TOKEN_ACK,
	DL_DISCONNECT_REQ,	DL_DISCONNECT_IND,
	DL_RESET_REQ,		DL_RESET_IND,
	DL_RESET_RES,		DL_RESET_CON,
	DL_DATA_ACK_REQ,	DL_DATA_ACK_IND,
	DL_DATA_ACK_STATUS_IND,	DL_REPLY_REQ,
	DL_REPLY_IND,		DL_REPLY_STATUS_IND,
	DL_REPLY_UPDATE_REQ,	DL_REPLY_UPDATE_STATUS_IND,
	DL_XID_REQ,		DL_XID_IND,
	DL_XID_RES,		DL_XID_CON,
	DL_TEST_REQ,		DL_TEST_IND,
	DL_TEST_RES,		DL_TEST_CON,
	DL_PHYS_ADDR_REQ,	DL_PHYS_ADDR_ACK,
	DL_SET_PHYS_ADDR_REQ,	DL_GET_STATISTICS_REQ,
	DL_GET_STATISTICS_ACK,	-1
};

static const char *
DLPIprimitiveStr(int primitive)
{
	register int n;

	for (n = 0; primitiveary[n] >= 0 && primitiveary[n] != primitive; n++)
		;
	return (primitivestr[n]);
}

static void
DLPIshowinfo(const dl_info_ack_t *d)
{
	char *v = (char *)d;

	logb("\tprimitive           = %lu (%s)\n",
	    d->dl_primitive, DLPIprimitiveStr(d->dl_primitive));
	logb("\tmax_sdu             = %lu\n", d->dl_max_sdu);
	logb("\tmin_sdu             = %lu\n", d->dl_min_sdu);
	logb("\tcurrent state       = %#lx (%s)\n",
	    d->dl_current_state, DLPIstateStr(d->dl_current_state));
	logb("\tservice_mode        = %#lx (%s)\n",
	    d->dl_service_mode, DLPIserviceStr(d->dl_service_mode));
	logb("\tqos length          = %lu\n", d->dl_qos_length);
	logb("\tqos offset          = %lu\n", d->dl_qos_offset);
	logb("\tqos range length    = %lu\n", d->dl_qos_range_length);
	logb("\tqos range offset    = %lu\n", d->dl_qos_range_offset);
	logb("\tprovider style      = %#lx (%s)\n",
	    d->dl_provider_style, DLPIstyleStr(d->dl_provider_style));
	logb("\tversion             = %lu\n", d->dl_version);
	logb("\tgrowth              = %lu\n", d->dl_growth);

	logb("\taddress type        = %lu (%s)\n",
	    d->dl_mac_type, DLPImediaStr(d->dl_mac_type));
	logb("\taddr offset         = %lu\n", d->dl_addr_offset);
	logb("\taddress length      = %lu\n", d->dl_addr_length);
	logb("\tsap_length          = %ld\n", d->dl_sap_length);
	logb("\tDLSAP addr          = %s\n",
	    addrstr(v + d->dl_addr_offset, d->dl_addr_length, TRUE, ':'));

	logb("\tbroadcast offset    = %lu\n", d->dl_brdcst_addr_offset);
	logb("\tbroadcast length    = %lu\n", d->dl_brdcst_addr_length);
	logb("\tbroadcast addr      = %s\n\n",
	    addrstr(v + d->dl_brdcst_addr_offset, d->dl_brdcst_addr_length,
	    TRUE, ':'));
}

static void
DLPIshowerrorack(const dl_error_ack_t *d)
{
	logb("\tprimitive           = %lu (%s)\n",
	    d->dl_primitive, DLPIprimitiveStr(d->dl_primitive));
	logb("\terror_primitive     = %lu (%s)\n",
	    d->dl_error_primitive, DLPIprimitiveStr(d->dl_error_primitive));
	logb("\terrno               = %lu (%s:  %s)\n",
	    d->dl_errno, DLPIerrStr(d->dl_errno), DLPIlongerrStr(d->dl_errno));
	logb("\tunix_errno          = %lu (%s)\n\n",
	    d->dl_unix_errno, sys_errlist[d->dl_unix_errno]);
}

static void
DLPIshowokack(const dl_ok_ack_t *d)
{
	logb("\tprimitive           = %lu (%s)\n",
	    d->dl_primitive, DLPIprimitiveStr(d->dl_primitive));
	logb("\tcorrect_primitive   = %lu (%s)\n\n",
	    d->dl_correct_primitive, DLPIprimitiveStr(d->dl_correct_primitive));
}

static void
DLPIshowphysaddrack(const dl_phys_addr_ack_t *d)
{
	char *v = (char *)d;

	logb("\tprimitive           = %lu (%s)\n",
	    d->dl_primitive, DLPIprimitiveStr(d->dl_primitive));
	logb("\tdl_addr_length      = %lu\n", d->dl_addr_length);
	logb("\tdl_addr_offset      = %lu\n", d->dl_addr_offset);
	logb("\tphysical addr       = %s\n\n",
	    addrstr(v + d->dl_addr_offset, d->dl_addr_length, TRUE, ':'));
}

static void
DLPIshowbindack(const dl_bind_ack_t *d)
{
	char *v = (char *)d;
	logb("\tprimitive           = %lu (%s)\n",
	    d->dl_primitive, DLPIprimitiveStr(d->dl_primitive));
	logb("\tdl_sap              = %lu\n", d->dl_sap);
	logb("\tdl_addr_length      = %lu\n", d->dl_addr_length);
	logb("\tdl_addr_offset      = %lu\n", d->dl_addr_offset);
	logb("\tdl_max_conind       = %lu\n", d->dl_max_conind);
	logb("\tdl_xidtest_flg      = %lu\n", d->dl_xidtest_flg);
	logb("\tDLSAP addr          = %s\n\n",
	    addrstr(v + d->dl_addr_offset, d->dl_addr_length, TRUE, ':'));
}

static void
DLPIshowunitdataind(const dl_unitdata_ind_t *d)
{
	char *v = (char *)d;

	logb("\tprimitive           = %lu (%s)\n",
	    d->dl_primitive, DLPIprimitiveStr(d->dl_primitive));
	logb("\tdl_dest_addr_length = %lu\n", d->dl_dest_addr_length);
	logb("\tdl_dest_addr_offset = %lu\n", d->dl_dest_addr_offset);
	logb("\tdest addr           = %s\n",
	    addrstr(v + d->dl_dest_addr_offset, d->dl_dest_addr_length,
	    TRUE, ':'));
	logb("\tdl_src_addr_length  = %lu\n", d->dl_src_addr_length);
	logb("\tdl_src_addr_offset  = %lu\n", d->dl_src_addr_offset);
	logb("\tsrc addr            = %s\n",
	    addrstr(v + d->dl_src_addr_offset, d->dl_src_addr_length,
	    TRUE, ':'));
	logb("\tdl_group_address    = %lu\n\n", d->dl_group_address);
}

void
DLPIshow(const union DL_primitives *d, int len)
{
	switch (d->dl_primitive) {
	case DL_INFO_ACK:
		DLPIshowinfo(&d->info_ack);
		break;
	case DL_OK_ACK:
		DLPIshowokack(&d->ok_ack);
		break;
	case DL_BIND_ACK:
		DLPIshowbindack(&d->bind_ack);
		break;
	case DL_PHYS_ADDR_ACK:
		DLPIshowphysaddrack(&d->physaddr_ack);
		break;
	case DL_ERROR_ACK:
		DLPIshowerrorack(&d->error_ack);
		break;
	case DL_UNITDATA_IND:
		DLPIshowunitdataind(&d->unitdata_ind);
		break;
	default:
		YCDISP(stdbug, d, "DLPI protocol message:\n", len);
		break;
	}
}
#endif	/* DEBUG */

#if DEBUG
int
getmsgDebug(int fd, struct strbuf *rc, struct strbuf *sc, int *flags)
{
	int rt;

	rt = getmsg(fd, rc, sc, flags);
	if (debug >= 6) {
		logb("getmsg return=%d", rt);
		if (rc)
			logb(" ctrl_len=%d", rc->len);
		if (sc)
			logb(" data_len=%d", sc->len);
		logb(" flags=%d\n", *flags);
	}
	return (rt);
}
#else
#define	getmsgDebug	getmsg
#endif

static void
dlmore(int fd, int oldrc)
{
	/* more stuff left: */
	int rt;
	int flags;
	union {
		uint32_t dl_primitive;
		char buf[1024];
	} u;
	struct strbuf rc;

	rc.buf = (char *)&u;
	rc.maxlen = sizeof (u);

	if ((oldrc & MORECTL) == MORECTL) {
#if DEBUG
		if (debug >= 6)
			logb("MORECTL\n");
#endif
		do {
			flags = 0;
			rc.len = 0;
			rt = getmsg(fd, &rc, NULL, &flags);

			if (rt < 0) {
				loge(UTILMSG14, SYSMSG);
				break;
			}

#if DEBUG
			if (debug >= 6)
				YCDISP(stdbug, u.buf, 0, rc.len);
#endif
		} while ((rt & MORECTL) == MORECTL);
	}

	if ((oldrc & MOREDATA) == MOREDATA) {
#if DEBUG
		if (debug >= 6)
			logb("MOREDATA\n");
#endif
		do {
			flags = 0;
			rc.len = 0;
			rt = getmsg(fd, NULL, &rc, &flags);

			if (rt < 0) {
				loge(UTILMSG14, SYSMSG);
				break;
			}

#if DEBUG
			if (debug >= 6)
				YCDISP(stdbug, u.buf, 0, rc.len);
#endif
		} while ((rt & MOREDATA) == MOREDATA);
	}
}

int
dlinfo(int fd, Jodlpiu *buf)
{
	int flags, rt;
	struct strbuf rc, sc;
	union DL_primitives *d = &buf->d;

	d->info_req.dl_primitive = DL_INFO_REQ;
	sc.buf = (char *)d;
	sc.len = sizeof (dl_info_req_t);

#if DEBUG
	if (debug >= 6)
		logb("putmsg DL_INFO_REQ\n");
#endif

	rt = putmsg(fd, &sc, NULL, 0);
	if (rt < 0) {
		loge(UTILMSG13, SYSMSG);
		return (rt);
	}

	rc.buf = (char *)d;
	rc.maxlen = DLPIBUFSZ;
	flags = 0;

	rt = getmsgDebug(fd, &rc, NULL, &flags);

	if (rt < 0) {
		loge(UTILMSG14, SYSMSG);
		return (rt);
	}

	if (d->dl_primitive != DL_INFO_ACK) {
		loge(UTILMSG15);
		return (-1);
	}

	dlmore(fd, rt);
	return (rt);
}

int
dlattach(int fd, int ppa)
{
	int flags, rt;
	struct strbuf rc, sc;
	Jodlpiu dlpibuf;
	union DL_primitives *d = &dlpibuf.d;

	sc.buf = (char *)d;
	sc.len = sizeof (dl_attach_req_t);
	d->attach_req.dl_primitive = DL_ATTACH_REQ;
	d->attach_req.dl_ppa = ppa;

#if DEBUG
	if (debug >= 6)
		logb("putmsg DL_ATTACH_REQ\n");
#endif

	rt = putmsg(fd, &sc, NULL, 0);
	if (rt < 0) {
		loge(UTILMSG13, SYSMSG);
		return (rt);
	}

	rc.buf = (char *)d;
	rc.maxlen = DLPIBUFSZ;
	flags = 0;

	rt = getmsgDebug(fd, &rc, NULL, &flags);
	if (rt < 0) {
		loge(UTILMSG14, SYSMSG);
		return (rt);
	}

#if DEBUG
	if (debug >= 6)
		DLPIshow(d, rc.len);
#endif

	if (d->dl_primitive != DL_OK_ACK)
		return (-1);

	dlmore(fd, rt);
	return (0);
}

int
dlgetphysaddr(int fd, void *addr, int laddr)
{
	int flags, rt;
	struct strbuf rc, sc;
	Jodlpiu dlpibuf;
	union DL_primitives *d = &dlpibuf.d;

	sc.buf = (char *)d;
	sc.len = sizeof (dl_phys_addr_req_t);
	d->physaddr_req.dl_primitive = DL_PHYS_ADDR_REQ;
	d->physaddr_req.dl_addr_type = DL_FACT_PHYS_ADDR;

#if DEBUG
	if (debug >= 6)
		logb("putmsg DL_PHYS_ADDR_REQ\n");
#endif

	rt = putmsg(fd, &sc, NULL, 0);
	if (rt < 0) {
		loge(UTILMSG13, SYSMSG);
		return (rt);
	}

	if (addr == 0)
		laddr = 0;

	rc.buf = (char *)d;
	rc.maxlen = DLPIBUFSZ;
	flags = 0;

	rt = getmsgDebug(fd, &rc, NULL, &flags);
	if (rt < 0) {
		loge(UTILMSG14, SYSMSG);
		return (rt);
	}

#if DEBUG
	if (debug >= 6)
		DLPIshow(d, rc.len);
#endif

	if (d->dl_primitive != DL_PHYS_ADDR_ACK)
		return (-1);

	if (laddr > 0) {
		if (d->physaddr_ack.dl_addr_length > laddr) {
		    loge(UTILMSG16, d->physaddr_ack.dl_addr_length);
		    return (-1);
		}
		memcpy(addr, rc.buf + d->physaddr_ack.dl_addr_offset,
		    d->physaddr_ack.dl_addr_length);
	}

	dlmore(fd, rt);
	return (0);
}

int
dlbind(int fd, int sap, int service)
{
	int flags, rt;
	struct strbuf rc, sc;
	Jodlpiu dlpibuf;
	union DL_primitives *d = &dlpibuf.d;

	sc.buf = (char *)d;
	sc.len = sizeof (dl_bind_req_t);
	d->bind_req.dl_primitive = DL_BIND_REQ;
	d->bind_req.dl_sap = sap;
	d->bind_req.dl_service_mode = service;
	d->bind_req.dl_max_conind = 0;
	d->bind_req.dl_conn_mgmt = 0;
	d->bind_req.dl_xidtest_flg = 0;

#if DEBUG
	if (debug >= 6)
		logb("putmsg DL_BIND_REQ\n");
#endif

	rt = putmsg(fd, &sc, NULL, 0);
	if (rt < 0) {
		loge(UTILMSG13, SYSMSG);
		return (rt);
	}

	rc.buf = (char *)d;
	rc.maxlen = DLPIBUFSZ;
	flags = 0;

	rt = getmsgDebug(fd, &rc, NULL, &flags);
	if (rt < 0) {
		loge(UTILMSG14, SYSMSG);
		return (rt);
	}

#if DEBUG
	if (debug >= 6)
		DLPIshow(d, rc.len);
#endif

	dlmore(fd, rt);

	if (d->dl_primitive != DL_BIND_ACK)
		return (-1);

	return (rt);
}

int
dlunitdatareq(int fd, const void *addrp, int addrlen, void *datap, int datalen)
{
	int32_t buf[DLPIBUFSZ / sizeof (int32_t)];
	union DL_primitives *dlp = (union DL_primitives *)buf;
	char *v = (char *)buf;
	struct strbuf rc, sc;
	int rt;

	dlp->unitdata_req.dl_primitive = DL_UNITDATA_REQ;
	dlp->unitdata_req.dl_dest_addr_length = addrlen;
	dlp->unitdata_req.dl_dest_addr_offset = sizeof (dl_unitdata_req_t);
	dlp->unitdata_req.dl_priority.dl_min = 0;
	dlp->unitdata_req.dl_priority.dl_max = 0;

	memcpy(v + sizeof (dl_unitdata_req_t), addrp, addrlen);

	sc.maxlen = 0;
	sc.len = sizeof (dl_unitdata_req_t) + addrlen;
	sc.buf = (char *)buf;

	rc.maxlen = 0;
	rc.len = datalen;
	rc.buf = (char *)datap;

	rt = putmsg(fd, &sc, &rc, 0);
	if (rt < 0)
		loge(UTILMSG13, SYSMSG);

	return (rt);
}
