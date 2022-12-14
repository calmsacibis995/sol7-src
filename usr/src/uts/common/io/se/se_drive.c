/*
 * Copyright (c) 1995-1997, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)se_driver.c	1.68	98/01/12 SMI"

/*
 * Serial driver for SAB 82532 chips. Sync and Async.
 * Handles normal UNIX support for terminals & modems
 * as well as hdlc synchronous protocol.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stream.h>
#include <sys/cred.h>
#include <sys/systm.h>		/* Must be before sunddi */
#include <sys/sunddi.h>
#include <sys/ddi.h>
#include <sys/conf.h>
#include <sys/modctl.h>
#include <sys/mkdev.h>
#include <sys/errno.h>
#include <sys/debug.h>
#include <sys/kmem.h>
#include <sys/consdev.h>
#include <sys/file.h>
#include <sys/stropts.h>
#include <sys/strsun.h>
#include <sys/dlpi.h>
#include <sys/stat.h>
#include <sys/ser_sync.h>
#include <sys/tty.h>
#include "sedev.h"	/* quotes instead of braces for ddict */

/*
 * Declaration of internal routines
 */

static int se_open(queue_t *, dev_t *, int, int, cred_t *);
static int se_wput(queue_t *, mblk_t *);
static int se_close(queue_t *, int);
static int se_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg,
		    void **result);
static int se_identify(dev_info_t *dip);
static int se_probe(dev_info_t *dip);
static int se_attach(dev_info_t *dip, ddi_attach_cmd_t cmd);
static int se_detach(dev_info_t *dip, ddi_detach_cmd_t cmd);

static void se_async_portint(se_ctl_t *, u_short);
static void se_hdlc_portint(se_ctl_t *, u_short);
static void se_async_softint(se_ctl_t *);
static void se_hdlc_softint(se_ctl_t *);
static void se_async_dslint(se_ctl_t *);
static void se_hdlc_dsrint(se_ctl_t *);
static int se_async_open(se_ctl_t *, queue_t *, int, cred_t *, dev_t *);
static int se_hdlc_open(se_ctl_t *, queue_t *, int, cred_t *, dev_t *);
static int se_clone_open(queue_t  *, dev_t *);
static int se_async_close(se_ctl_t *, queue_t *);
static int se_hdlc_close(se_ctl_t *, queue_t *);
static int se_clone_close(se_ctl_t *, queue_t *);
static int se_async_wput(se_ctl_t *, queue_t *, mblk_t *);
static int se_hdlc_wput(se_ctl_t *, queue_t *, mblk_t *);
static int se_clone_wput(se_ctl_t *, queue_t *, mblk_t *);
static void se_async_resume(se_ctl_t *);
static void se_hdlc_resume(se_ctl_t *);
static void se_async_suspend(se_ctl_t *);
static void se_hdlc_suspend(se_ctl_t *);

static int  se_null(se_ctl_t *);
static void se_null_int(se_ctl_t *);
static void se_null_portint(se_ctl_t *, u_short);

static void se_callback(void *);
static void se_kick_rcv(void *);
static void se_async_flowcontrol(se_ctl_t *, int);
static void se_async_start(se_ctl_t *);
static void se_async_ioctl(se_ctl_t *, queue_t *, mblk_t *);
static void se_async_program(se_ctl_t *, boolean_t);
static void se_hdlc_ioctl(se_ctl_t *, queue_t *, mblk_t *);
static void se_hdlc_start(se_ctl_t *);
static void se_hdlc_watchdog(void *);

static int se_create_ordinary_minor_nodes(dev_info_t *, se_ctl_t *, se_ctl_t *);
static int se_create_ucm_minor_nodes(dev_info_t *, se_ctl_t *, se_ctl_t *);

/*
 * Loadable module information
 */

static struct module_info se_module_info = {
	0x4747,			/* module ID number (is this ever non-zero?) */
	"se",			/* module name. */
	0,			/* Minimum packet size (none) */
	INFPSZ,			/* Maximum packet size (none) */
	12*1024,		/* queue high water mark (from zs_hdlc) */
	4*1024			/* queue low water mark  (from zs_hdlc) */
};

static struct qinit se_rinit = {
	putq,    NULL,	se_open, se_close, NULL, &se_module_info};

static struct qinit se_winit = {
	se_wput, NULL,	NULL,	NULL,	NULL,	&se_module_info};
static struct streamtab se_streamtab = {&se_rinit, &se_winit, NULL, NULL};

DDI_DEFINE_STREAM_OPS(se_dev_ops, \
			se_identify, \
			se_probe, \
			se_attach, \
			se_detach, \
			nodev, \
			se_info, \
			(D_NEW | D_MP), \
			&se_streamtab);

static struct modldrv modldrv = {
	&mod_driverops, "Siemens SAB 82532 ESCC2 1.68", &se_dev_ops
};

static struct modlinkage modlinkage = {
	MODREV_1, &modldrv, NULL
};

/*
 * Protocol-specific operations
 */

se_ops_t se_async_ops = {	se_async_portint,	se_async_softint,
	se_async_dslint,   	se_async_open,		se_async_close,
	se_async_wput,		se_async_resume,	se_async_suspend};

/* Operations for hdlc devices */

se_ops_t se_hdlc_ops = {	se_hdlc_portint,	se_hdlc_softint,
	se_hdlc_dsrint,		se_hdlc_open,		se_hdlc_close,
	se_hdlc_wput,		se_hdlc_resume,		se_hdlc_suspend};

/* Operations for the clone devices */

se_ops_t se_clone_ops = {	se_null_portint,	se_null_int,
	se_null_int,		se_null,		se_clone_close,
	se_clone_wput,		se_null_int,		se_null_int};

/* Null operation vector */

se_ops_t se_null_ops = {	se_null_portint,	se_null_int,
	se_null_int,		se_null,		se_null,
	se_null,		se_null_int,		se_null_int};

/* Fix some implementation ugliness */

#ifndef i386
#undef SE_CLK
#define	SE_CLK 29491200
#define	SE_REGISTER_FILE_NO 0
#define	SE_INTERRUPT_CONFIG SAB_IPC_LOW | SAB_IPC_VIS
#define	SE_ENDIANNESS DDI_STRUCTURE_BE_ACC
#define	SE_REGOFFSET 0x000000
#define	swapit(value) ((((value) & 0xff) << 8) | ((value) >> 8))
#else

/* Fix some implementation ugliness in non-positrons */


#ifndef SS2_HACK
#undef SE_CLK
#define	SE_CLK 7372800
#define	SE_REGISTER_FILE_NO 0
#define	SE_INTERRUPT_CONFIG SAB_IPC_LOW | SAB_IPC_VIS
#define	SE_ENDIANNESS DDI_STRUCTURE_LE_ACC
#define	SE_REGOFFSET 0x0
#define	swapit(value) (value)
#else
/* Specific to the SS2 hack, which has notably different hardware than */
/* the PCI Honeynut or Positron - all goes away with real sparc test   */
#undef SE_CLK
#define	SE_CLK 4915200
#define	SE_REGISTER_FILE_NO 0
#define	SE_INTERRUPT_CONFIG SAB_IPC_ODRN | SAB_IPC_VIS
#define	SE_ENDIANNESS DDI_NEVERSWAP_ACC
#define	SE_REGOFFSET 0x0
#define	swapit(value) ((((value) & 0xff) << 8) | ((value) >> 8))
#endif
#endif

#define	KICK_RCV_TIMER 20
/*
 * baud rate definitions
 */

#define	N_SE_SPEEDS 25	/* max # of speeds */

/* For v2.2 Siemens SAB 82532 ESCC2 at frequency 7.3728 MHz */
#define	SE_SPEED(speed) ((((SE_CLK / 32) / speed) - 1))

short v22_se_speeds[N_SE_SPEEDS] = {
	0,			/* 0 - MBZ */
	SE_SPEED(76800),	/* 1 - 50 baud unsupported */
	SE_SPEED(115200),	/* 2 - 75 baud unsupported */
	SE_SPEED(153600),	/* 3 - 110 baud unsupported */
	0,			/* 4 - 134.5 baud unsupported (yay!) */
	SE_SPEED(230400),	/* 5 - 150 baud unsupported */
	SE_SPEED(460800),	/* 6 - 200 baud unsupported */
	SE_SPEED(300),		/* 7 - 300 baud, Minimum rate supported */
	SE_SPEED(600),		/* 8 */
	SE_SPEED(1200),		/* 9 */
	SE_SPEED(1800),		/* 10 */
	SE_SPEED(2400),		/* 11 */
	SE_SPEED(4800),		/* 12 */
	SE_SPEED(9600),		/* 13 */
	SE_SPEED(19200),	/* 14 - Maximum rate comfortable with 8530s */
	SE_SPEED(38400),	/* 15 - Maximum rate supported with 8530s */
/* Extended baud rates */
	SE_SPEED(57600),	/* 16 */
	SE_SPEED(76800),	/* 17 - ZyXEL v.32 compressed speed */
	SE_SPEED(115200),	/* 18 - 115.2kb - PC/16550 maximum rate */
	SE_SPEED(153600),	/* 19 - 153600 unsupported */
	SE_SPEED(230400),	/* 20 - 230.4kb */
	SE_SPEED(307200),	/* 21 - 307.2kb unsupported */
	SE_SPEED(460800),	/* 22 - 460.8kb */
	SE_SPEED(614400),	/* 23 - 614.4kb */
	SE_SPEED(921600),	/* 24 - 921.6kb Ha! */
};


/* For v3.1 Siemens SAB 82532 ESCC2 at frequency 29.4912 MHz */
/* In 3.1, the baud rate divisor is in the form ((exponent << 6) * mantissa) */
/* The below hideous macro is the simplest form I could find to convert from */
/* baud rate to divisor. */

#define	SE_V31_CLOCK 29491200/16

#define	ZRATE(rate, div, exp) \
	((SE_V31_CLOCK/rate) < (64 * div)) ? \
	((exp << 6) + (((SE_V31_CLOCK-1) / (rate * div)) & 0x3f)) :

#undef SE_SPEED
#define	SE_SPEED(rate) \
    ZRATE(rate,    2,  1) \
    ZRATE(rate,    4,  2) \
    ZRATE(rate,    8,  3) \
    ZRATE(rate,   16,  4) \
    ZRATE(rate,   32,  5) \
    ZRATE(rate,   64,  6) \
    ZRATE(rate,  128,  7) \
    ZRATE(rate,  256,  8) \
    ZRATE(rate,  512,  9) \
    ZRATE(rate, 1024, 10) 0

short v31_se_speeds[N_SE_SPEEDS] = {
	0,			/* 0 - MBZ */
	SE_SPEED(50),		/* 1 */
	SE_SPEED(75),		/* 2 */
	SE_SPEED(110),		/* 3 - 110 - actually 109.09 */
	SE_SPEED(134),		/* 4 - 134.5 - actually 133.333 */
	SE_SPEED(150),		/* 5 */
	SE_SPEED(200),		/* 6 */
	SE_SPEED(300),		/* 7 */
	SE_SPEED(600),		/* 8 */
	SE_SPEED(1200),		/* 9 */
	SE_SPEED(1800),		/* 10 */
	SE_SPEED(2400),		/* 11 */
	SE_SPEED(4800),		/* 12 */
	SE_SPEED(9600),		/* 13 */
	SE_SPEED(19200),	/* 14 - Maximum rate comfortable with 8530s */
	SE_SPEED(38400),	/* 15 - Maximum rate supported with 8530s */
/* Extended baud rates */
	SE_SPEED(57600),	/* 16 - Common PC/16550 rate */
	SE_SPEED(76800),	/* 17 - ZyXEL v.32 compressed speed */
	SE_SPEED(115200),	/* 18 - 115.2kb - PC/16550 maximum rate */
	SE_SPEED(153600),	/* 19 - 153600 unsupported */
	SE_SPEED(230400),	/* 20 - 230.4kb */
	SE_SPEED(307200),	/* 21 - 307.2kb unsupported */
	SE_SPEED(460800),	/* 22 - 460.8kb */
	SE_SPEED(614400),	/* 23 - 614.4kb unsupported */
	SE_SPEED(921600),	/* 24 - 921.6kb unsupported */
};

/*
 * Macro to generate speed from divisor - used to compute 1 char delay
 * This macro essentially is the reverse of the calculation made by
 * SE_SPEED above.
 */
#define	DIV2RATE(d)	(29491200 / (16 * ((d & 0x3f) + 1) * (1 << (d >> 6))))

/*
 * Global variables
 */

static kmutex_t se_hi_excl;	/* driver-wide exclusion lock */

static se_chip_t *se_chips;	/* Head of linked list of per-chip items */
static se_ctl_t  *se_ctl_list;	/* Head of linked list of per-port items */

static int se_initialized = 0;	/* flag whether driver is initialized */
static int se_softpend = 0;	/* Flag indicating soft interrupt requested */

static int se_default_mru = 1024;   /* Default hdlc message size */
static int se_default_asybuf = 256; /* Default async buffer size */

static int se_default_dtrlow = 3;   /* How long to hold dtr down */

static ddi_iblock_cookie_t se_iblock;    /* Interrupt block cookies */
static ddi_iblock_cookie_t se_hi_iblock;

static ddi_softintr_t se_softintr_id;    /* return value from add_softintr */

static se_clone_t *se_clones = NULL; /* List of clone devices */
static int se_clone_number = 0;	/* Next available unit */

extern kcondvar_t lbolt_cv;	/* DDI/DKI uncleanliness? */

static int se_debug = 0;
static int se_invalid_frame = 0;
static int se_fast_edges = 0;

/* Statistics */
#ifdef STATS
#define	STAT_INCR(x) x++

int stat_rpfints = 0, stat_tcdints = 0, stat_txints = 0, stat_timeints = 0,
    stat_parints = 0, stat_dslints = 0, stat_rfoints = 0, stat_brktints = 0,
    stat_allsints = 0, stat_softints = 0, stat_highints = 0;

hrtime_t stat_maxhiint, stat_tothiint, stat_maxsoftint, stat_totsoftint;
#else
#define	STAT_INCR(x)
#endif

/*
 * Macro definitions to make life easy
 */

/* Check to see if we should wait for suspend to finish */
#define	SE_CHECK_SUSPEND(sc) {if (sc->sc_suspend) se_wait_suspend(sc); }

/* Allocate blocks for rstandby array */
#define	SE_FILLSBY(sc)							\
	if (sc->sc_rstandby_ptr < SE_MAX_RSTANDBY) se_fillsby(sc);

/* Take a block from the rstandy array and put into sc_rcvhead/blk */
#define	SE_TAKEBUFF(sc)							\
	if (sc->sc_rstandby_ptr > 0) {					\
		sc->sc_rstandby_ptr--;					\
		sc->sc_rcvhead = sc->sc_rcvblk =			\
			sc->sc_rstandby[sc->sc_rstandby_ptr];		\
		sc->sc_rstandby[sc->sc_rstandby_ptr] = NULL;		\
		sc->sc_rd_cur = sc->sc_rcvhead->b_wptr;			\
		sc->sc_rd_lim = sc->sc_rcvhead->b_datap->db_lim;	\
	}


/* Flag and request a software interrupt */
#define	SE_SETSOFT(sc)							\
{									\
	sc->sc_flag_softint = 1;					\
	if (!se_softpend) {						\
		se_softpend = 1;					\
		ddi_trigger_softintr(se_softintr_id);			\
} }

/* Add an item to the rdone queue */
#define	SE_PUTQ(sc, mp)							\
{									\
	mp->b_next = NULL;						\
	sc->sc_rdone_count++;						\
	if (sc->sc_rdone_tail == NULL)					\
		sc->sc_rdone_tail = sc->sc_rdone_head = mp;		\
	else {								\
		sc->sc_rdone_tail->b_next = mp;				\
	sc->sc_rdone_tail = mp;						\
} }

#define	SE_GIVE_RCV(sc)							\
	if ((sc->sc_rcvblk != NULL) &&					\
	    (sc->sc_rd_cur != sc->sc_rcvblk->b_wptr)) {			\
		sc->sc_rcvblk->b_wptr = sc->sc_rd_cur;			\
		sc->sc_rd_cur = sc->sc_rd_lim = NULL;			\
		SE_PUTQ(sc, sc->sc_rcvhead);				\
		sc->sc_rcvblk = sc->sc_rcvhead = NULL;			\
	}

/* Lock/unlock per-port datastructure */
#define	PORT_LOCK(sc)   mutex_enter(sc->h.sc_excl)
#define	PORT_UNLOCK(sc) mutex_exit(sc->h.sc_excl)

/* Lock/unlock entire driver */
#define	DRIVER_LOCK    mutex_enter(&se_hi_excl)
#define	DRIVER_UNLOCK  mutex_exit(&se_hi_excl)

/* Easy interfaces into DDI common get/put routines */
#define	REG_PUTB(sc, ptr, value) \
			ddi_put8(sc->sc_handle, &sc->sc_reg->ptr, value)
#define	REG_GETB(sc, ptr) \
			ddi_get8(sc->sc_handle, &sc->sc_reg->ptr)
#define	REG_PUTW(sc, ptr, value) \
			ddi_put16(sc->sc_handle, &sc->sc_reg->ptr, value)
#define	REG_GETW(sc, ptr) \
			ddi_get16(sc->sc_handle, &sc->sc_reg->ptr)

#define	DDI_LL_SIZE 4
#define	ULL uint32_t *

#define	MULT_GETL(sc, mem, dev, count) \
	ddi_rep_get32(sc->sc_handle, mem, dev, count, DDI_DEV_NO_AUTOINCR)
#define	MULT_PUTL(sc, mem, dev, count) \
	ddi_rep_put32(sc->sc_handle, mem, dev, count, DDI_DEV_NO_AUTOINCR)
#define	MULT_GETB(sc, mem, dev, count) \
	ddi_rep_get8(sc->sc_handle, mem, dev, count, DDI_DEV_NO_AUTOINCR)
#define	MULT_PUTB(sc, mem, dev, count) \
	ddi_rep_put8(sc->sc_handle, mem, dev, count, DDI_DEV_NO_AUTOINCR)

#define	PUT_CMDR(sc, value) \
	if (REG_GETB(sc, sab_star) & SAB_STAR_CEC) /* Is cmdr busy? */	\
		while (REG_GETB(sc, sab_star) & SAB_STAR_CEC) 		\
			drv_usecwait(1);				\
	REG_PUTB(sc, sab_cmdr, value)


/* Values for se_async_flowcontrol */
#define	FLOW_IN_STOP 1
#define	FLOW_IN_START 2

/*
 * Actual code starts here. Loadable driver jacket routines.
 */

int
_init(void)
{
	return (mod_install(&modlinkage));
}

int
_fini(void)
{
	return (mod_remove(&modlinkage));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

/*
 * Utility routines. Macro support.
 */

static void
se_wait_suspend(se_ctl_t *sc)
{
	mutex_exit(sc->h.sc_excl);
	(void) ddi_dev_is_needed(sc->sc_chip->sec_dip, sc->h.sc_unit, 1);
	mutex_enter(sc->h.sc_excl);
}

/* Allocate blocks for rstandby array */
static void
se_fillsby(se_ctl_t *sc)
{
	mblk_t *bp;
	while ((sc->sc_rstandby_ptr < SE_MAX_RSTANDBY) &&
	    (sc->sc_rdone_count < SE_MAX_RDONE)) {
		bp = allocb(sc->sc_bufsize, BPRI_MED);
		if (bp == NULL) {
			if (sc->sc_bufcid == 0)
				sc->sc_bufcid = bufcall(sc->sc_bufsize,
				    BPRI_MED, se_callback, sc);
			return;
		} else {
			DRIVER_LOCK;
			sc->sc_rstandby[sc->sc_rstandby_ptr] = bp;
			sc->sc_rstandby_ptr++;
			DRIVER_UNLOCK;
		} /* End if allocation */
	} /* End while */

	if (sc->sc_rstandby_ptr < SE_MAX_RSTANDBY) { /* if max_rdone... */
		if (sc->sc_kick_rcv_id == 0) /* schedule wakeup */
		    sc->sc_kick_rcv_id =
			timeout(se_kick_rcv, sc, KICK_RCV_TIMER);
	}
}

/* The following code is used for performance metering and debugging; */
/* This routine is invoked via "TIME_POINT(label)" macros, which will */
/* store the label and a timestamp. This allows seeing execution sequences */
/* and timestamps associated with them. */

#undef TPOINTS
#ifdef TPOINTS
/* Time trace points */
int time_point_active = 0;
int time_point_offset, time_point_loc;
#define	POINTS 1024
int se_time_points[POINTS];
#define	TIME_POINT(x) if (time_point_active) se_time_point(x);
void
se_time_point(int loc)
{
	static hrtime_t time_point_base;

	hrtime_t now;

	now = gethrtime();
	if (time_point_base == 0) {
		time_point_base = now;
		time_point_loc = loc;
		time_point_offset = 0;
	} else {
		se_time_points[time_point_offset] = loc;
		se_time_points[time_point_offset+1] =
		    (now - time_point_base) / 1000;
		time_point_offset += 2;
/* wrap at end */
		if (time_point_offset >= POINTS) time_point_offset = 0;
/* disable at end.  if (time_point_offset >= POINTS) time_point_active = 0; */
	}
}
#else
#define	TIME_POINT(x)
#endif

/*
 * Actual code starts here. Streams entry routines.
 */

static int
se_close(queue_t *rq, int flag)
{
	se_ctl_t *sc = rq->q_ptr;
	int rval;

	if (sc == NULL)
	    return (ENODEV);		/* Already closed once */

	PORT_LOCK(sc);
	rval = (*sc->h.sc_ops->close)(sc, rq, flag); /* Call service routine */
	PORT_UNLOCK(sc);

	/* Clear protocol-specifics */
	if (sc->h.sc_protocol != CLON_PROTOCOL) {
		bzero((caddr_t)&sc->z, sizeof (sc->z));

		/* Mark device as available for power management */
		(void) pm_idle_component(sc->sc_chip->sec_dip,
		    sc->h.sc_unit % 2 + 1);
	}
	return (rval);
}

static int
se_wput(queue_t *rq, mblk_t *mp)
{
	se_ctl_t *sc = rq->q_ptr;
	int rval;

	if (sc == NULL)
	    return (ENODEV);   /* Can't happen. */

	PORT_LOCK(sc);
	rval = (sc->h.sc_ops->wput)(sc, rq, mp);
	PORT_UNLOCK(sc);
	return (rval);
}

static int
se_open(queue_t *rq, dev_t *dev, int flag, int sflag, cred_t *cr)
{
	int unit, protocol, rval;
	se_ctl_t *sc;

	if (sflag == CLONEOPEN)	/* For clone devices, bypass below code */
	    return (se_clone_open(rq, dev));

	unit = getminor(*dev) & ~(OUTLINE | HDLC_DEVICE | UCM_DEVICE);
	protocol = getminor(*dev) & (OUTLINE | HDLC_DEVICE);

	/* Step through list of attached units, looking for this one */
	for (sc = se_ctl_list; sc != NULL; sc = sc->sc_next)
	    if (sc->h.sc_unit == unit) break;

	if (sc == NULL)
	    return (EBUSY);	/* If didn't find it, say busy */

	PORT_LOCK(sc);		/* Lock per-port datastructure */
	switch (protocol) {
	    case (ASYNC_DEVICE): /* Standard async protocol */
		if ((sc->h.sc_protocol == NULL_PROTOCOL) ||
		    (sc->h.sc_protocol == ASYN_PROTOCOL)) {
			sc->h.sc_protocol = ASYN_PROTOCOL;
			sc->h.sc_ops = &se_async_ops;
			rval = 0;
		} else
		    rval = EBUSY;
		break;

	    case (OUTLINE):	/* Outdial protocol */
		if ((sc->h.sc_protocol == NULL_PROTOCOL) ||
		    (sc->h.sc_protocol == OUTD_PROTOCOL)) {
			sc->h.sc_protocol = OUTD_PROTOCOL;
			sc->h.sc_ops = &se_async_ops;
			rval = 0;
		} else if ((sc->h.sc_protocol == ASYN_PROTOCOL) &&
		    sc->z.za.zas.zas_wopen) {
			/*
			 * If blocked in ASYN_PROTOCOL waiting for carrier
			 * allow open in OUTDIAL mode
			 */
			sc->h.sc_protocol = OUTD_PROTOCOL;
			sc->h.sc_ops = &se_async_ops;
			rval = 0;
		} else
		    rval = EBUSY;
		break;

	    case (HDLC_DEVICE):
		if ((sc->h.sc_protocol == NULL_PROTOCOL) ||
		    (sc->h.sc_protocol == HDLC_PROTOCOL)) {
			sc->h.sc_protocol = HDLC_PROTOCOL;
			sc->h.sc_ops = &se_hdlc_ops;
			rval = 0;
		} else
		    rval = EBUSY;
		break;

	    default:
		cmn_err(CE_WARN, "se%d: Unknown protocol", unit);
		rval = EBUSY;
	}

	if (sc->sc_closing)
		rval = EBUSY;

	/* Mark device as busy (for power management) */
	(void) pm_busy_component(sc->sc_chip->sec_dip, unit%2+1);
	(void) ddi_dev_is_needed(sc->sc_chip->sec_dip, unit%2+1, 1);

	if (rval == 0) do {
		rval = (sc->h.sc_ops->open)(sc, rq, flag, cr, dev); /* Open */
	} while (rval == -1);	/* Loop until open finishes or aborts */

	PORT_UNLOCK(sc);
	return (rval);
}

/*
 * Interrupt routines. Top-level chip interrupt.
 */

static u_int
se_high_intr(caddr_t ignore)
{
	u_int rval;		/* did/did not handle int. */
	se_chip_t *sec;		/* Pointer to our per-chip datastructure */
	se_ctl_t *sca, *scb;	/* Pointer to our per-port datastructures */
	u_char gis, pis;	/* Temporary holders for register data */
	u_short isr;		/* Temporary interrupt status register */

#ifdef lint
	ignore = ignore;
#endif
#ifdef STATS
	hrtime_t starttime, inttime;
	starttime = gethrtime(); /* Get current hrestime */
#endif

	DRIVER_LOCK;
	STAT_INCR(stat_highints);

	/* Loop through all known chips looking for one which needs service */
	for (sec = se_chips; sec != NULL; sec = sec->sec_next) {

		sca = sec->sec_porta;	/* Set up pointers to both ports */
		scb = sec->sec_portb;

		/* Loop until this chip doesn't demand service any more */
		while (gis = REG_GETB(sca, sab_gis)) {

			if (gis & (SAB_GIS_ISA1 | SAB_GIS_ISA0)) {
				isr = REG_GETW(sca, sab_isr);
				isr = swapit(isr) & sca->sc_imr;
				(sca->h.sc_ops->portint)(sca, isr);
			}
			if (gis & (SAB_GIS_ISB1 | SAB_GIS_ISB0)) {
				isr = REG_GETW(scb, sab_isr);
				isr = swapit(isr) & scb->sc_imr;
				(scb->h.sc_ops->portint)(scb, isr);
			}
			if (gis & SAB_GIS_PI) { /* PPort. Must be DSR */
				pis = REG_GETB(sca, sab_pis);
				pis &= sca->sc_chip->sec_pim;
				if (pis & SAB_PVR_DSRA)
				    (sca->h.sc_ops->dsrint)(sca);
				if (pis & SAB_PVR_DSRB)
				    (scb->h.sc_ops->dsrint)(scb);
			}
		}	/* End while chip requests interrupts */
	} /* End search for chip requesting interrupts */

	rval = DDI_INTR_CLAIMED; /* serviced interrupt */

	DRIVER_UNLOCK;
#ifdef STATS
	inttime = gethrtime() - starttime;
	if (inttime > stat_maxhiint)
	    stat_maxhiint = inttime;
	stat_tothiint += inttime;
#endif

	return (rval);		/* Tell framework whether we did anything */
}

/*
 * Interrupt routines. Software interrupt. Background tasks.
 */

static u_int
se_softint(caddr_t ignore)
{
	se_ctl_t *sc;
	int rv;

#ifdef lint
	ignore = ignore
#endif
#ifdef STATS
	hrtime_t starttime, inttime;
	starttime = gethrtime(); /* Get current hrestime */
#endif

	STAT_INCR(stat_softints);
	if (se_softpend)
	    rv = DDI_INTR_CLAIMED;
	else
	    rv = DDI_INTR_UNCLAIMED;
	se_softpend = 0;	/* No longer pending. */

	/* Loop through all ports looking for any which need service */
	for (sc = se_ctl_list; sc != NULL; sc = sc->sc_next)
	    if (sc->sc_flag_softint) {		/* This port need service? */
		    sc->sc_flag_softint = 0;	/* Not any more. */
		    PORT_LOCK(sc); 		/* Lock port datastructure */
		    (sc->h.sc_ops->softint)(sc);
		    PORT_UNLOCK(sc);
	    }; /* End for */


#ifdef STATS
	inttime = gethrtime() - starttime;
	if (inttime > stat_maxsoftint)
	    stat_maxsoftint = inttime;
	stat_totsoftint += inttime;
#endif

	return (rv);
}

/*
 * Service routine. Handle dataset leads.
 */

static u_int
se_mctl(se_ctl_t *sc, u_int bits, u_int how)
{
	u_int obits = 0, mbits;	/* modem status temp variables */
	time_t timeval;		/* Timestamp for dtrlow purposes */
	u_char pvr;		/* Copy of port value register */

	/* Make sure we belong here. */
	ASSERT(mutex_owned(&se_hi_excl));
	ASSERT(mutex_owned(sc->h.sc_excl));


	/* Fetch current value of five DSL bits scattered over four regs. */

	pvr = REG_GETB(sc, sab_pvr);	/* Fetch the parallel port bits */
	if (!(pvr & sc->sc_dsrbit))
	    obits |= TIOCM_DSR;
	if (!(pvr & sc->sc_dtrbit))
	    obits |= TIOCM_DTR;
	if (REG_GETB(sc, sab_star) & SAB_STAR_CTS)
	    obits |= TIOCM_CTS;
	if (sc->sc_chipv23 == 0x01) { /* Old way */
		if (REG_GETB(sc, sab_mode) & SAB_MODE_RTS)
		    obits |= TIOCM_RTS;
	} else {
		if ((REG_GETB(sc, sab_mode) & (SAB_MODE_RTS | SAB_MODE_FRTS))
		    == SAB_MODE_RTS)
		    obits |= TIOCM_RTS;	/* Unless pulled down, assume high */
	}

	if (!(REG_GETB(sc, sab_vstr) & SAB_VSTR_CD))
	    obits |= TIOCM_CD;

	/* Determine what we will do with retreived values */

	switch (how) {
	    case (DMGET):
		mbits = obits;	/* just return value, don't change it. */
		break;
	    case (DMSET):
		mbits = bits;	/* Ignore retrieved value, set to new */
		break;
	    case (DMBIS):
		mbits = obits | bits; /* bis wants to set some bits */
		break;
	    case (DMBIC):
		mbits = obits & ~bits; /* bic wants to clear some bits */
		break;
	    default:
		cmn_err(CE_WARN,  "se%d: Illegal mctl operation %d",
			sc->h.sc_unit, how);
	};

	/* Change RTS if needed */
	if ((mbits ^ obits) & TIOCM_RTS)
	    if (sc->sc_chipv23 == 0x01) { /* Old chip RTS */
		    if (mbits & TIOCM_RTS)
			REG_PUTB(sc, sab_mode,
			    REG_GETB(sc, sab_mode) | sc->z.za.za_flon_mask |
			    SAB_MODE_RTS);
		    else
			REG_PUTB(sc, sab_mode,
			    (REG_GETB(sc, sab_mode) | sc->z.za.za_flon_mask) &
			    ~SAB_MODE_RTS);
	    } else {		/* New chip handle RTS */
		    if (sc->z.za.za_ttycommon.t_cflag & CRTSXOFF) {
			    REG_PUTB(sc, sab_mode,
				    (REG_GETB(sc, sab_mode) & ~SAB_MODE_RTS)
				    | sc->z.za.za_flon_mask | SAB_MODE_FRTS);
		    } else if (mbits & TIOCM_RTS) {
			    REG_PUTB(sc, sab_mode,
				    (REG_GETB(sc, sab_mode) & ~SAB_MODE_FRTS)
				    | sc->z.za.za_flon_mask | SAB_MODE_RTS);
		    } else {
			    REG_PUTB(sc, sab_mode,
				    (REG_GETB(sc, sab_mode) |
				    sc->z.za.za_flon_mask) | SAB_MODE_RTS |
				    SAB_MODE_FRTS);
		    }
	    }

	/* Change DTR if needed */
	if ((mbits ^ obits) & TIOCM_DTR) { /* Does DTR need to change? */
		if (mbits & TIOCM_DTR) {	/* Raise DTR */

/*
 * Before raising DTR, make sure DTR has been low
 * long enough for a modem to notice.
 */

			(void) drv_getparm(TIME, &timeval);
			while (timeval <= (sc->sc_dtrlow + se_default_dtrlow)) {
				DRIVER_UNLOCK;
				cv_wait(&lbolt_cv, sc->h.sc_excl);
				SE_CHECK_SUSPEND(sc);
				DRIVER_LOCK;
				(void) drv_getparm(TIME, &timeval);
			}

			sc->sc_chip->sec_pvr &= ~sc->sc_dtrbit;
			REG_PUTB(sc, sab_pvr, sc->sc_chip->sec_pvr);

		} else {			/* Lower DTR */
			sc->sc_chip->sec_pvr |= sc->sc_dtrbit;
			REG_PUTB(sc, sab_pvr, sc->sc_chip->sec_pvr);

			(void) drv_getparm(TIME, &timeval);
			sc->sc_dtrlow = timeval;	/* %%%%CROCK%%%% */
		}
	} /* end if DTR needs to change */

	return (obits);
}

/*
 * Streams entries. Identify, info, probe routines.
 */

static int
se_identify(dev_info_t *dip)
{
	char *name;
	name = ddi_get_name(dip);

	if ((strcmp(name, "se") == 0) ||
	    (strcmp(name, "pci108e,1000") == 0)) {
		return (DDI_IDENTIFIED);
	} else
	    return (DDI_NOT_IDENTIFIED);
}

static int
se_info(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg, void **result)
{
	int unit;
	se_ctl_t *sc;
	dev_t dev = (dev_t)arg;
#ifdef lint
	dip = dip;
#endif
	unit = getminor(dev) & ~(OUTLINE | HDLC_DEVICE);

	switch (infocmd) {

	    case DDI_INFO_DEVT2DEVINFO:
		for (sc = se_ctl_list; sc != NULL; sc = sc->sc_next)
		    if (sc->h.sc_unit == unit) {
			    *result = (void *) sc->sc_chip->sec_dip;
			    return (DDI_SUCCESS);
		    }
		return (DDI_FAILURE);

	    case DDI_INFO_DEVT2INSTANCE:
		*result = (void *)unit;
		return (DDI_SUCCESS);
	    default:
		return (DDI_FAILURE);
	}
}

static int
se_probe(dev_info_t *dip)
{
	se_regs_t *rz;
	int	rval;
	ddi_acc_handle_t handle;
	ddi_device_acc_attr_t attr;

	attr.devacc_attr_version = DDI_DEVICE_ATTR_V0;
	attr.devacc_attr_endian_flags = SE_ENDIANNESS;
	attr.devacc_attr_dataorder = DDI_STRICTORDER_ACC;

	/*
	 * temporarily map in  registers
	 */
	if (ddi_regs_map_setup(dip, SE_REGISTER_FILE_NO, (caddr_t *)&rz,
			    SE_REGOFFSET, sizeof (se_regs_t) * 2,
			    &attr, &handle)) {
		cmn_err(CE_WARN, "se_probe: unable to map registers");
		return (DDI_FAILURE);
	}

	if (ddi_dev_is_sid(dip) == DDI_FAILURE) {
		rval = DDI_PROBE_SUCCESS;
	} else
	    rval = DDI_PROBE_SUCCESS;
	ddi_regs_map_free(&handle);
	return (rval);
}

/*
 * Streams entries. Attach.
 */

static int
se_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	int chipno;
	int i;
	se_chip_t *sec;
	se_ctl_t *sca, *scb;
	se_regs_t *rz;
	u_char chip_version;
	boolean_t is_ucm = B_FALSE;
	char *ucm_console_str = "ucm-console";
	char *ucm_control_str = "ucm-control";
	int ucm_console, ucm_control;
	int r;

	ddi_acc_handle_t handle;
	ddi_device_acc_attr_t attr;

	chipno = ddi_get_instance(dip); /* Which chip are we attaching */

	/* Find our chip structure if it already exists. */
	for (sec = se_chips; sec != NULL; sec = sec->sec_next)
	    if (sec->sec_chipno == chipno) break;

	/* Handle the easy case first - resume. */
	if (cmd == DDI_RESUME) {
		if (sec == NULL)
		    return (DDI_FAILURE);
		ddi_put8(sec->sec_porta->sc_handle,
			&sec->sec_porta->sc_reg->sab_pcr, sec->sec_pcr);
		ddi_put8(sec->sec_porta->sc_handle,
			&sec->sec_porta->sc_reg->sab_pvr, sec->sec_pvr);

		sec->sec_porta->sc_suspend = 0;
		(sec->sec_porta->h.sc_ops->resume)(sec->sec_porta);

		sec->sec_portb->sc_suspend = 0;
		(sec->sec_portb->h.sc_ops->resume)(sec->sec_portb);
		return (DDI_SUCCESS);
	} else if (cmd != DDI_ATTACH) {
		return (DDI_FAILURE);	/* RESUME and ATTACH are all I know. */
	}


	/*
	 * Check to see if this instance is dedicated to a ucm device.
	 */
	ucm_console = ddi_getprop(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
					ucm_console_str, -1);
	switch (ucm_console) {
	default:
		cmn_err(CE_WARN, "%s%d: invalid value (0x%x) for %s property\n",
			ddi_get_name(dip), ddi_get_instance(dip),
			ucm_console, ucm_console_str);
		return (DDI_FAILURE);
	case -1:
		/*
		 * No "ucm-console" property.  Assume this instance
		 * is being used for "generic" serial ports.
		 */
		break;
	case 0:
	case 1:
		/*
		 * The ucm-console property is ok.  Now check the
		 * "ucm-control" property.
		 */
		ucm_control = ddi_getprop(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
					ucm_control_str, -1);
		switch (ucm_control) {
		default:
			cmn_err(CE_WARN,
			    "%s%d: invalid value (0x%x) for %s property\n",
			    ddi_get_name(dip), ddi_get_instance(dip),
			    ucm_control, ucm_control_str);
			return (DDI_FAILURE);
		case -1:
			/*
			 * No "ucm-control" property.  This property should
			 * be present if a "ucm-console" property exists.
			 */
			cmn_err(CE_WARN,
			    "%s%d: %s property not found\n",
			    ddi_get_name(dip), ddi_get_instance(dip),
			    ucm_control_str);
			return (DDI_FAILURE);
		case 0:
		case 1:
			/*
			 * Now just make sure the console and control aren't
			 * set to the same port.
			 */
			if (ucm_console == ucm_control) {
				cmn_err(CE_WARN,
				    "%s%d: %s and %s properties are the same\n",
				    ddi_get_name(dip), ddi_get_instance(dip),
				    ucm_console_str, ucm_control_str);
				return (DDI_FAILURE);
			}
			is_ucm = B_TRUE;
		}
	}

	attr.devacc_attr_version = DDI_DEVICE_ATTR_V0;
	attr.devacc_attr_endian_flags = SE_ENDIANNESS;
	attr.devacc_attr_dataorder = DDI_STRICTORDER_ACC;

	/* Map the registers in - permanently. */
	if (ddi_regs_map_setup(dip, SE_REGISTER_FILE_NO, (caddr_t *)&rz,
			    SE_REGOFFSET, sizeof (se_regs_t) * 2,
			    &attr, &handle) != DDI_SUCCESS) {
		cmn_err(CE_WARN, "se_attach: Could not map registers");
		return (DDI_FAILURE);
	}

	if (ddi_add_intr(dip, 0, &se_hi_iblock, NULL, se_high_intr, NULL)
	    != DDI_SUCCESS) {
		cmn_err(CE_PANIC, "se_attach: cannot set high interrupt");
		/*NOTREACHED*/
	}


	if (!se_initialized) {	/* First pass through, set up globals */
		se_initialized++; /* Increment so we don't do it again */

		if (!is_ucm) {
			/* Create hdlc clone device - only one of them */
			if (ddi_create_minor_node(dip, "se_hdlc", S_IFCHR,
			    NULL, DDI_PSEUDO, CLONE_DEV) == DDI_FAILURE) {
				ddi_remove_minor_node(dip, NULL);
				cmn_err(CE_WARN, "se_attach: hdlc clone device"
				    "creation failed.\n");
				return (DDI_FAILURE);
			}
		}

		mutex_init(&se_hi_excl, NULL, MUTEX_DRIVER, se_hi_iblock);

		if (ddi_add_softintr(dip, DDI_SOFTINT_LOW, &se_softintr_id,
				    &se_iblock, NULL, se_softint, NULL)
		    != DDI_SUCCESS) {
			cmn_err(CE_PANIC,
				"se_attach: Cannot set soft interrupt");
			/*NOTREACHED*/
		}
	}

	chip_version = ddi_get8(handle, &rz->sab_vstr) & 0x0f;

/* Attach a device. This is probably always allocating a new device,    */
/* but not necessarily. Allow re-attaching an existing device to work. */

	if (sec == NULL) {	/* If we are adding a device, allocate */
		sec = kmem_zalloc(sizeof (se_chip_t), KM_SLEEP);
		sec->sec_chipno = chipno;	/* Initialize new block */
		sec->sec_dip = dip;

		sec->sec_porta = sca =
		    kmem_zalloc(sizeof (se_ctl_t), KM_SLEEP);
		sca->h.sc_excl =
		    (kmutex_t *)kmem_zalloc(sizeof (kmutex_t), KM_SLEEP);
		sca->h.sc_unit = chipno * SE_PORTS + 0;
		sca->h.sc_protocol = NULL_PROTOCOL;
		sca->h.sc_ops = &se_null_ops;
		mutex_init(sca->h.sc_excl, NULL, MUTEX_DRIVER, se_iblock);

		/* Set the softcd property by the prom characteristics */
		sca->sc_softcar = ddi_getprop(DDI_DEV_T_ANY, dip,
					DDI_PROP_DONTPASS,
					"port-a-ignore-cd", 0);

		sec->sec_portb = scb =
		    kmem_zalloc(sizeof (se_ctl_t), KM_SLEEP);
		scb->h.sc_excl =
		    (kmutex_t *)kmem_zalloc(sizeof (kmutex_t), KM_SLEEP);
		scb->h.sc_unit = chipno * SE_PORTS + 1;
		scb->h.sc_protocol = NULL_PROTOCOL;
		scb->h.sc_ops = &se_null_ops;
		mutex_init(scb->h.sc_excl, NULL, MUTEX_DRIVER, se_iblock);

		scb->sc_softcar = ddi_getprop(DDI_DEV_T_ANY, dip,
					DDI_PROP_DONTPASS,
					"port-b-ignore-cd", 0);

		/* Set information regarding UCM */
		sec->sec_is_ucm = is_ucm;
		if (is_ucm) {
			sec->sec_ucm_console = ucm_console == 0 ? sca : scb;
			sec->sec_ucm_control = ucm_control == 0 ? sca : scb;
		}

		DRIVER_LOCK;		/* make sure nobody gets in our way */
		sec->sec_next = se_chips; /* Add this chip to the list */
		se_chips = sec;

		sca->sc_next = scb; /* Add our port datastructures to list */
		scb->sc_next = se_ctl_list;
		se_ctl_list = sca;
		DRIVER_UNLOCK;
	}

	/* Initialize port datastructures */

	PORT_LOCK(sca);		/* Lock out any meddling */
	sca->sc_chip = sec;
	sca->sc_reg = rz;	/* Register pointer */
	sca->sc_handle = handle;
	sca->sc_wr_cur = sca->sc_wr_lim = NULL;
	sca->sc_rd_cur = sca->sc_rd_lim = NULL;
	cv_init(&sca->sc_flags_cv, NULL, CV_DEFAULT, NULL);
	sca->sc_rstandby_ptr = 0;	/* No buffers on list */
	sca->sc_dtrbit = SAB_PVR_DTRA;
	sca->sc_dsrbit = SAB_PVR_DSRA;
	sca->sc_rtsbit = SAB_PVR_RTSA;
	sca->sc_chipv23 = chip_version; /* Remember version */
	for (i = 0; i < SE_MAX_RSTANDBY; i++) sca->sc_rstandby[i] = NULL;
	DRIVER_LOCK;		/* Below items are protected by int. lock */
	sca->sc_rdone_head = sca->sc_rdone_tail = NULL;
	sca->sc_rdone_count = 0;
	sca->sc_rcvhead = sca->sc_rcvblk = NULL;
	sca->sc_xmithead = sca->sc_xmitblk = NULL;
	DRIVER_UNLOCK;
	sca->sc_bufcid = 0;		/* No buffer callback */
	sca->sc_bufsize = se_default_asybuf;
	sca->sc_char_pending = sca->sc_flag_softint = sca->sc_suspend =
	    sca->sc_xmit_active = sca->sc_soft_active = 0;
	bzero((caddr_t)&sca->z, sizeof (sca->z));
	PORT_UNLOCK(sca);

	/* Initialize port b */
	PORT_LOCK(scb);		/* Lock out any meddling */
	scb->sc_chip = sec;
	scb->sc_reg = rz + 1;		/* Register pointer - second half */
	scb->sc_handle = handle;
	scb->sc_wr_cur = scb->sc_wr_lim = NULL;
	scb->sc_rd_cur = scb->sc_rd_lim = NULL;
	cv_init(&scb->sc_flags_cv, NULL, CV_DEFAULT, NULL);
	scb->sc_rstandby_ptr = 0;	/* No buffers on list */
	scb->sc_dtrbit = SAB_PVR_DTRB;
	scb->sc_dsrbit = SAB_PVR_DSRB;
	scb->sc_rtsbit = SAB_PVR_RTSB;
	scb->sc_chipv23 = chip_version; /* Remember version */
	for (i = 0; i < SE_MAX_RSTANDBY; i++) scb->sc_rstandby[i] = NULL;
	DRIVER_LOCK;		/* Below items are protected by int. lock */
	scb->sc_rdone_head = scb->sc_rdone_tail = NULL;
	scb->sc_rdone_count = 0;
	scb->sc_rcvhead = scb->sc_rcvblk = NULL;
	scb->sc_xmithead = scb->sc_xmitblk = NULL;
	DRIVER_UNLOCK;
	scb->sc_bufcid = 0;		/* No buffer callback */
	scb->sc_bufsize = se_default_asybuf;
	scb->sc_char_pending = scb->sc_flag_softint = scb->sc_suspend =
	    scb->sc_xmit_active = scb->sc_soft_active = 0;

	sec->sec_pcr = SAB_PVR_DSRA | SAB_PVR_DSRB; /* two inputs */
	/*
	 * If this device is being used to control a UCM devcie, then
	 * make sure the controlling bit is an input to avoid driving
	 * the pin low.
	 */
	if (is_ucm)
		sec->sec_pcr |= SAB_PVR_UCM;
	sec->sec_pvr = sca->sc_dtrbit | scb->sc_dtrbit; /* default dtrs low */
	if (!se_fast_edges)
	    sec->sec_pvr |= SAB_PVR_FAST; /* Default to slow edges */
	ddi_put8(sca->sc_handle, &sca->sc_reg->sab_pcr, sec->sec_pcr);
	ddi_put8(sca->sc_handle, &sca->sc_reg->sab_pvr, sec->sec_pvr);

	bzero((caddr_t)&scb->z, sizeof (scb->z));
	PORT_UNLOCK(scb);

	/* Create the entries in /dev and /devices */

	if (is_ucm)
		r = se_create_ucm_minor_nodes(dip, sca, scb);
	else
		r = se_create_ordinary_minor_nodes(dip, sca, scb);

	if (r != DDI_SUCCESS)
		return (r);

	/*
	 * Add power management properties for all channels.
	 * All components are created idle.
	 */
	if (pm_create_components(dip, 3) != DDI_SUCCESS) {
		ddi_remove_minor_node(dip, NULL);
		return (DDI_FAILURE);
	}
	(void) pm_busy_component(dip, 0);
	pm_set_normal_power(dip, 0, 1);
	pm_set_normal_power(dip, 1, 1);
	pm_set_normal_power(dip, 2, 1);

	ddi_report_dev(dip);	/* Done. */

	return (DDI_SUCCESS);
}

/*
 * Streams entries. Detach.
 */

static int
se_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	int chipno;
	se_chip_t *sec;

	if (cmd == DDI_DETACH)
	    return (DDI_FAILURE); /* We don't allow ddi_detach */
	else if (cmd != DDI_SUSPEND)
	    return (DDI_FAILURE); /* We don't anything but suspend */

	chipno = ddi_get_instance(*dip);

	/* Find our datastructure */
	for (sec = se_chips; sec != NULL; sec = sec->sec_next)
	    if (sec->sec_chipno == chipno) break;

	if (sec == NULL)
	    return (DDI_FAILURE); /* Didn't find this device to suspend */

	/* Call both ports, asking them to suspend */
	(sec->sec_porta->h.sc_ops->suspend)(sec->sec_porta);
	(sec->sec_portb->h.sc_ops->suspend)(sec->sec_portb);
	return (DDI_SUCCESS);
}

/*
 * Utility routines. Ack, copy, callback.
 */

/*
 * This routine converts an incoming ioctl message block into a
 * return ioctl ack message block.
 */

static void
se_ack(mblk_t *mp, mblk_t *dp, u_int size)
{
	struct iocblk *iocp = (struct iocblk *)mp->b_rptr;

	mp->b_datap->db_type = M_IOCACK;

	iocp->ioc_count = size;
	iocp->ioc_error = 0;
	iocp->ioc_rval = 0;

	if (mp->b_cont != NULL)
	    freeb(mp->b_cont);	/* We're about to clobber this pointer */
	if (dp != NULL) {
		mp->b_cont = dp; /* If we have a data block, add it in. */
		dp->b_wptr += size;
	} else
	    mp->b_cont = NULL;	/* Indicate no data block */
}

/*
 * The following routine gets used a lot in TRANSPARENT mode, where
 * we have to go back and forth into user's space to retrieve arguments
 */

static void
se_copy(mblk_t *mp, mblk_t *dp, u_int size, u_char type)
{
	struct copyreq *cp = (struct copyreq *)mp->b_rptr;

	cp->cq_private = NULL;
	cp->cq_flag = 0;
	cp->cq_size = size;

	/*
	 * The following line of code is sure to have a bug in it.
	 * But I'm afraid to touch it.
	 */

	cp->cq_addr = (caddr_t)(*(long *)(mp->b_cont->b_rptr));

	/*
	 * Yuk!
	 */

	if (mp->b_cont != NULL)
	    freeb(mp->b_cont);	/* We're about to clobber this pointer */
	if (dp != NULL) {
		mp->b_cont = dp; /* If we have a data pointer, set it up */
		dp->b_wptr += size;
	} else
	    mp->b_cont = NULL;	/* Otherwise, just clobber it */
	mp->b_datap->db_type = type;
	mp->b_wptr = mp->b_rptr + sizeof (*cp);
}

/*
 * The following routine is called by streams when we were earlier unable
 * to allocate buffers, but will probably be able to do so now.
 */

static void
se_callback(void *arg)
{
	se_ctl_t *sc = arg;

	PORT_LOCK(sc);		/* Lock everyone else out */
	sc->sc_bufcid = 0;	/* We've gotten our callback, clobber this */

/*
 * Allocate as many buffers as needed. Note that if we can't allocate
 * enough, we will re-set the bufcall to call us back again.
 */

	SE_FILLSBY(sc);
	SE_SETSOFT(sc);		/* Ask for a soft interrupt to reprime pipe */

	PORT_UNLOCK(sc);
}

/* poll receiver when needed. Also ask for buffer allocation if needed. */
static void
se_kick_rcv(void *arg)
{
	se_ctl_t *sc = arg;

	PORT_LOCK(sc);
	DRIVER_LOCK;		/* Lock everyone else out */
	sc->sc_kick_rcv_id = 0; /* We've gotten our callback */
	DRIVER_UNLOCK;

	if (sc->sc_closing) {
		PORT_UNLOCK(sc);
		return;
	}

	/* If flowcontrolled, try to allocate buffers and restart input */
	if (sc->sc_rstandby_ptr < SE_MAX_RSTANDBY) {
		SE_FILLSBY(sc);
		if ((sc->h.sc_protocol != HDLC_PROTOCOL) &&
		    sc->z.za.zas.zas_flowctl && (sc->sc_rstandby_ptr > 1))
			se_async_flowcontrol(sc, FLOW_IN_START);
	}

	/* If data available, trigger soft interrupt to handle it */
	if ((sc->sc_rcvblk && (sc->sc_rd_cur != sc->sc_rcvblk->b_wptr)) ||
	    sc->sc_rdone_head) {
		DRIVER_LOCK;
		SE_SETSOFT(sc);
		DRIVER_UNLOCK;
	}

	PORT_UNLOCK(sc);
}


/*
 * Async-specific routines start here.
 */

/*
 * Async rpf (receive pipe full) interrupt.
 */

/*
 * This interrupt indicates we have exactly 32 characters of data
 * in the receive fifo. This will be the routine called when we are
 * doing intensive data receive, so optimizations are important.
 */

static void
se_async_rpfint(se_ctl_t *sc)
{
	int i;
	int escapes = 0;
	u_char local_buffer[SAB_FIFO_SIZE];

	STAT_INCR(stat_rpfints);

/*
 * Before we do anything, make sure we have room for minimum amount of data
 */
	if (((sc->sc_rd_lim - sc->sc_rd_cur) < SAB_FIFO_SIZE) ||
	    (sc->sc_rd_cur == NULL)) {
		SE_GIVE_RCV(sc); /* Close out current buffer */
		SE_TAKEBUFF(sc); /* Grab the next buffer if available */
		SE_SETSOFT(sc);	/* Notify upper layers now */
	}

/*
 * Read data from fifos. Exactly 32 bytes worth.
 */

	/* Try for fast and easy case. */
	if (!sc->z.za.zas.zas_do_esc && (sc->sc_rd_cur != NULL) &&
	    (((uintptr_t)sc->sc_rd_cur & 0x1f) == 0)) {
/*
 * Nothing complicated to do. No quoting needed, and we have buffer space.
 * Just copy data from fifo to buffer.
 */
		MULT_GETL(sc, (ULL)sc->sc_rd_cur,
			(ULL)&sc->sc_reg->sab_rfifo[0],
			SAB_FIFO_SIZE/DDI_LL_SIZE);
		sc->sc_rd_cur += SAB_FIFO_SIZE;
		PUT_CMDR(sc, SAB_CMDR_RMC); /* Allow chip to re-use fifo. */
	} else {
/*
 * Something complicated has to be resolved. Do it the hard way.
 */
		MULT_GETL(sc, (ULL)local_buffer,
			(ULL)&sc->sc_reg->sab_rfifo,
			SAB_FIFO_SIZE/DDI_LL_SIZE);
		PUT_CMDR(sc, SAB_CMDR_RMC); /* Allow chip to re-use fifo. */
/*
 * Count escapes if necessary
 */
		if (sc->z.za.zas.zas_do_esc)  /* If 0xff is special value */
		    for (i = 0; i < SAB_FIFO_SIZE; i++)
			if (local_buffer[i] == 0xff) escapes++;

/*
 * Deal with buffering. Find room for data, then copy, possibly quoting.
 */
		if ((sc->sc_rd_lim - sc->sc_rd_cur) <
		    (SAB_FIFO_SIZE + escapes))
		    SE_GIVE_RCV(sc); /* Close out current buffer */
		if (sc->sc_rd_cur == NULL)
		    SE_TAKEBUFF(sc); /* Grab a buffer if available */
		if (sc->sc_rd_cur == NULL) {
			sc->z.za.za_sw_overrun++; /* we will lose data */
		} else {
			if (escapes == 0) { /* How complicated is this copy? */
				/*
				 * Simple copy.
				 */
				for (i = 0; i < SAB_FIFO_SIZE; i++)
					sc->sc_rd_cur[i] = local_buffer[i];
				sc->sc_rd_cur += SAB_FIFO_SIZE;
			} else {
				/*
				 * Complicated. Copy byte by byte.
				 */
				register u_char * rd_cur = sc->sc_rd_cur;
				register u_char data_byte;
				for (i = 0; i < SAB_FIFO_SIZE; i++) {
					data_byte = local_buffer[i];
					*rd_cur++ = data_byte;
					if (data_byte == 0xff)
					    *rd_cur++ = data_byte; /* Twice */
				}
				sc->sc_rd_cur = rd_cur;
			} /* End if escape handling */
		} /* End of non-null receive buffer */
	} /* End of if easy copy */

	/*
	 * See if we should invoke flow control.
	 */
	if (sc->sc_rstandby_ptr == 0)
		se_async_flowcontrol(sc, FLOW_IN_STOP);

	if (sc->sc_kick_rcv_id == 0) /* If no pending rcv notification */
		SE_SETSOFT(sc);	/* Notify upper layers that data is avail. */
}

/*
 * Async tcd (termination character detected) interrupt.
 */

/*
 * This interrupt indicates we have less than 32 characters of data
 * in the receive fifo. This happens when we have issued a rfrd command
 * to the chip, usually in response to a time interrupt.
 *
 * This is less performance-intensive than the RPF interrupt, so we code
 * more for maintainability than raw performance.
 */

static void
se_async_rxint_tcd(se_ctl_t *sc)
{
	int data_count, i;
	u_char data_byte;

	u_char *rd_cur, *rd_lim;

	STAT_INCR(stat_tcdints);

	/*
	 * Register rbcl has the count of bytes of data available in the fifo.
	 * We have to mask it down to the range 0:SAB_FIFO_SIZE - 1
	 */

	data_count = REG_GETB(sc, sab_rbcl) & (SAB_FIFO_SIZE - 1);
	/*
	 * Read the data, byte by byte.
	 */
	rd_cur = sc->sc_rd_cur;
	rd_lim = sc->sc_rd_lim;

	for (i = 0; i < data_count; i++) {
		if ((rd_cur == rd_lim) || (rd_cur == NULL)) {
			sc->sc_rd_cur = rd_cur;	/* Update buffer pointers, */
			SE_GIVE_RCV(sc); /* close buffer and get another */
			SE_TAKEBUFF(sc);
			rd_cur = sc->sc_rd_cur;
			rd_lim = sc->sc_rd_lim;
		}

		data_byte = REG_GETB(sc, sab_rfifo[0]); /* Read a data byte */

		if (rd_cur == NULL)
			sc->z.za.za_sw_overrun++; /* If no buffer, flag error */
		else
			*rd_cur++ = data_byte; /* Store data byte */

		/*
		 * Handle escaped (quoted) character if necessary
		 */
		if ((sc->z.za.zas.zas_do_esc) && (data_byte == 0xff)) {
			/*
			 * Duplicate escape (0xff) value
			 */
			if (rd_cur == rd_lim) {	/* If no buffer space, */
				sc->sc_rd_cur = rd_cur;	/* Update pointers, */
				SE_GIVE_RCV(sc); /* get another buffer */
				SE_TAKEBUFF(sc);
				rd_cur = sc->sc_rd_cur;
				rd_lim = sc->sc_rd_lim;
			}
			if (rd_cur == NULL)
			    sc->z.za.za_sw_overrun++; /* No buf, lose data */
			else
			    *rd_cur++ = data_byte; /* Store 2nd copy of 0xff */
		}
	} /* End for data_count loop */

	/*
	 * We're done copying data out of buffer. Tell chip and clean up.
	 */

	PUT_CMDR(sc, SAB_CMDR_RMC); /* Allow chip to re-use fifo. */
	sc->sc_rd_cur = rd_cur;	/* Update buffer pointer */

	if (sc->z.za.zas.zas_parerr) { /* Did we see a parity error earlier? */
		sc->z.za.zas.zas_parerr = 0; /* Clear error flag */

/* %%% ALERT %%% ALERT %%% ALERT %%% ALERT %%% ALERT %%% ALERT %%% ALERT %%% */
/*
 * Bad code warning. We back up the current rd pointer and pick up the last
 * byte deposited. This is wasteful and potentially buggy.
 */
/* %%% ALERT %%% ALERT %%% ALERT %%% ALERT %%% ALERT %%% ALERT %%% ALERT %%% */
		if (sc->sc_rd_cur == NULL) {
			/*
			 * No more buffers : ignore collected characters and
			 * alert outside world via za_sw_overrun
			 */
			sc->z.za.za_sw_overrun++;
		} else {
			/*
			 * If the current buffer isn't empty, it's last
			 * character should be the one that had the parity
			 * error: remove it from the buffer, enqueue the
			 * edited buffer which now contains only
			 * "good" characters as an M_DATA streams message.
			 */
			if (sc->sc_rd_cur != sc->sc_rcvblk->b_wptr)
				data_byte = *(--sc->sc_rd_cur);
			else
				data_byte = 0x47; /* If no data, say "G" */

			if (sc->sc_rd_cur != sc->sc_rcvblk->b_wptr) {
				SE_GIVE_RCV(sc); /* Close current buffer */
				SE_TAKEBUFF(sc); /* Get another buffer */
			}

			/*
			 * Fill new buffer with a steams M_BREAK message
			 * containing the "bad" character,
			 * then enqueue it.
			 */
			if (sc->sc_rd_cur == NULL) {
				sc->z.za.za_sw_overrun++; /* No buffer */
			} else {
				*sc->sc_rd_cur++ = data_byte;
				sc->sc_rcvblk->b_datap->db_type = M_BREAK;
				SE_GIVE_RCV(sc); /* Close buffer */
				SE_TAKEBUFF(sc); /* Get another buffer */
			}
		}
	} /* End if parerr */

	/*
	 * See if we should invoke flow control.
	 */
	if (data_count && (sc->sc_rstandby_ptr == 0))
		se_async_flowcontrol(sc, FLOW_IN_STOP);

	SE_SETSOFT(sc);		/* Notify upper layers that data is avail. */
}

/*
 * Async transmit interrupt. Room for another 32 bytes of data.
 */

static void
se_async_txint(se_ctl_t *sc)
{
	int data_count;

	STAT_INCR(stat_txints);

	if ((sc->sc_wr_cur == NULL) || sc->z.za.zas.zas_stopped ||
	    !(REG_GETB(sc, sab_star) & SAB_STAR_XFW))
		return;			/* Nothing to do in this case. */

	data_count = sc->sc_wr_lim - sc->sc_wr_cur; /* data in buffer */

	if (data_count >= SAB_FIFO_SIZE) { /* A lot of data to send? */

		/*
		 * Easy case. Just copy a lot of data.
		 */
		MULT_PUTL(sc, (ULL)sc->sc_wr_cur,
			(ULL)&sc->sc_reg->sab_rfifo,
			SAB_FIFO_SIZE/DDI_LL_SIZE);
		sc->sc_wr_cur += SAB_FIFO_SIZE;
		PUT_CMDR(sc, SAB_CMDR_XF); /* Start data flowing */
		sc->sc_xmit_active = 1;	/* Flag that we are active */

	} else if (data_count) {	/* Do we have _any_ data to send? */

		/*
		 * We don't have a full fifo's data to send.
		 */

		MULT_PUTB(sc, sc->sc_wr_cur,
			&sc->sc_reg->sab_rfifo[0], data_count);
		sc->sc_wr_cur += data_count; /* Skip over data we just moved */
		PUT_CMDR(sc, SAB_CMDR_XF); /* Start data flowing */
		sc->sc_xmit_active = 1;	/* Flag that we are active */
	}

	/*
	 * Handle end-of-block, skipping to next block and requesting new data.
	 */

	if (sc->sc_wr_cur == sc->sc_wr_lim) {	/* Wrote all data in block? */

		if (sc->sc_xmitblk->b_cont != NULL) { /* continuation buf? */
			sc->sc_xmitblk = sc->sc_xmitblk->b_cont;
			sc->sc_wr_cur = sc->sc_xmitblk->b_rptr;
			sc->sc_wr_lim = sc->sc_xmitblk->b_wptr;
		} else {
			sc->sc_xmit_done = 1;	/* we need another buffer */
			SE_SETSOFT(sc);	/* request a softint to provide it */
		}
	} /* End of check next block checks. */
}

/*
 * Async port interrupt. Something happened for this port.
 */

static void
se_async_portint(se_ctl_t *sc, u_short isr)
{
	if (isr & SAB_ISR_RPF)
		se_async_rpfint(sc);	/* Receive pipe full */

	if (isr & SAB_ISR_TCD)
		se_async_rxint_tcd(sc);	/* Termination character detected */

	if (isr & (SAB_ISR_FERR | SAB_ISR_PERR)) {
		/* Parity or framing error */
		PUT_CMDR(sc, SAB_CMDR_RFRD); /* Gimme what data you have */
		sc->z.za.zas.zas_parerr = 1; /* Something in data is bad */
		STAT_INCR(stat_parints);
	} else if (isr & SAB_ISR_BRK) { /* Start of BREAK: empty fifo */
		PUT_CMDR(sc, SAB_CMDR_RFRD); /* Request fifo read enable */
	} else if (isr & SAB_ISR_TIME) {
		PUT_CMDR(sc, SAB_CMDR_RFRD); /* Request fifo read enable */
		STAT_INCR(stat_timeints);
	} else if (isr & SAB_ISR_XOFF) {
		PUT_CMDR(sc, SAB_CMDR_RFRD); /* Request fifo read enable */
		STAT_INCR(stat_timeints);
	} else if (isr & SAB_ISR_XON) {
		PUT_CMDR(sc, SAB_CMDR_RFRD); /* Request fifo read enable */
		STAT_INCR(stat_timeints);
	} else if (isr & (SAB_ISR_CDSC | SAB_ISR_CSC)) {
		se_async_dslint(sc);	/* Carrier detect or CTS changed */
		STAT_INCR(stat_dslints);
	}

	if (isr & SAB_ISR_RFO) {	/* Receive fifo overflow */
		sc->z.za.za_hw_overrun++; /* Hardware overrun */
		SE_SETSOFT(sc);		/* Request service */
		STAT_INCR(stat_rfoints);
	}

	if (isr & SAB_ISR_BRKT) {	/* Break condition ended */
		STAT_INCR(stat_brktints);
		/* Do something fancy for break on console */
		if ((sc->sc_dev == kbddev) || (sc->sc_dev == rconsdev) ||
		    (sc->sc_dev == stdindev)) {
			/* Enter kadb or prom */
			abort_sequence_enter((char *)NULL);
		} else {
			sc->z.za.zas.zas_break_rcv = 1; /* Flag break */
			SE_SETSOFT(sc);		/* Wake up lower layer */
		}
	}

	/*
	 * Note - We handle the ALLS interrupt before the XPR
	 * interrupt. If we get an interrupt with both bits set, we will
	 * clear xmit_active, and then txint can turn xmit_active back on
	 * if it starts things going again.
	 */

	if (isr & SAB_ISR_ALLS) { /* All sent. Chip inactive. */
		sc->sc_xmit_active = 0;	/* No longer active */
		SE_SETSOFT(sc);
		STAT_INCR(stat_allsints);
	}

	if (isr & SAB_ISR_XPR)
		se_async_txint(sc); /* Transmit pipe ready, send more data */
}

/*
 * se_async_softint: Background interrupt, do messy slow stuff.
 */

static void
se_async_softint(se_ctl_t *sc)
{
	queue_t *rq;
	mblk_t *bp;
	mblk_t *previous_bp;
	int mstat;
	int hangup = 0, unhangup = 0, sw_overrun = 0, hw_overrun = 0;
	int mbreak = 0, restart_output = 0;

	if (sc->sc_suspend)
		return; /* If suspended, don't service device */

	rq = sc->z.za.za_ttycommon.t_readq;

	/*
	 * We can get here without having the line fully open, if the open
	 * hangs waiting for CD to come up. Handle that case first, looking
	 * for CD and waking up the open if CD comes up.
	 */

	if (rq == NULL) {
		if (sc->z.za.zas.zas_wopen && sc->z.za.za_ext &&
		    !sc->z.za.zas.zas_carr_on) { /* dsl changed in open wait */

			DRIVER_LOCK; /* Keep modem bits steady */
			mstat = se_mctl(sc, 0, DMGET);
			sc->z.za.za_ext = 0; /* Clear status changed flag */
			DRIVER_UNLOCK;
			if ((mstat & TIOCM_CD) ||
			    (sc->z.za.za_ttycommon.t_flags & TS_SOFTCAR)) {
				/* If CD went up, record it and wakeup open */
				sc->z.za.zas.zas_carr_on = 1;
				cv_broadcast(&sc->sc_flags_cv);
			}
		}
		return;		/* Let the open finish */
	} /* End if rq is null */

	/*
	 * We're fully open. Handle external status changes for open line.
	 */

	if (sc->z.za.za_ext) {	/* External modem status change? */
		DRIVER_LOCK;	/* Keep modem bits steady */
		mstat = se_mctl(sc, 0, DMGET);
		sc->z.za.za_ext = 0; /* Clear status changed flag */
		DRIVER_UNLOCK;
		if ((mstat & TIOCM_CD) ||
		    (sc->z.za.za_ttycommon.t_flags & TS_SOFTCAR)) {
			sc->z.za.zas.zas_carr_on = 1;
			unhangup = 1;
		} else if (sc->z.za.zas.zas_carr_on &&
		    !(sc->z.za.za_ttycommon.t_cflag & CLOCAL)) {
			/*
			 * CD went away and we aren't a local line. Drop DTR.
			 */

			DRIVER_LOCK;
			if (sc->sc_xmit_active || sc->sc_wr_cur) {
				/* abort any transmit */
				PUT_CMDR(sc, SAB_CMDR_XRES);
				sc->sc_xmit_active = 0;
				if (sc->sc_xmitblk != NULL) {
					sc->sc_xmitblk->b_rptr = sc->sc_wr_cur;
					sc->sc_wr_cur = sc->sc_wr_lim = NULL;
				}
			}

			(void) se_mctl(sc, TIOCM_DTR, DMBIC); /* Drop DTR */
			hangup = 1;
			sc->z.za.zas.zas_stopped = 0;
			sc->z.za.zas.zas_carr_on = 0;
			sc->sc_xmit_done = 1; /* Request buffer cycling */
			DRIVER_UNLOCK;
		}
		cv_broadcast(&sc->sc_flags_cv);
	} /* End if external modem status change */

	/*
	 * Check mundane things like overruns
	 */

	if (sc->z.za.za_sw_overrun) {
		sc->z.za.za_sw_overrun = 0;
		sw_overrun = 1;
	}
	if (sc->z.za.za_hw_overrun) {
		sc->z.za.za_hw_overrun = 0;
		hw_overrun = 1;
	}
	if (sc->z.za.zas.zas_break_rcv) {
		mbreak = 1;
		sc->z.za.zas.zas_break_rcv = 0;
	}

	/*
	 * Check to see if transmit needs help.
	 */
	if (sc->sc_xmit_done) {
		DRIVER_LOCK;
		sc->sc_xmit_done = 0;
		bp = sc->sc_xmithead; /* Current buffer pointer */
		sc->sc_xmithead = sc->sc_xmitblk = NULL;
		sc->sc_wr_cur = sc->sc_wr_lim = NULL;
		restart_output = 1; /* Used later in this routine */
		DRIVER_UNLOCK;
	} else
		bp = NULL;		/* no message pointer otherwise */

	if (restart_output || !sc->sc_xmit_active)
		se_async_start(sc);	/* Send more data if we have it */

	sc->sc_soft_active = 1;	/* Flag that we are still in softint */
	PORT_UNLOCK(sc);	/* take away lock, but leave softint flag */

	/*
	 * Now we can do slow ugly things like talk to streams
	 */

	if (bp)
		freemsg(bp);	/* Free transmitted message, if any */

	if (unhangup || (sc->z.za.zas.zas_draining && !sc->sc_xmit_active))
		cv_broadcast(&sc->sc_flags_cv); /* Wakeup! */
	if (unhangup)
		/* Send a message saying unhangup */
		(void) putnextctl(rq, M_UNHANGUP);

	if (hangup)
		(void) putnextctl(rq, M_HANGUP);

	if (mbreak)
		(void) putnextctl(rq, M_BREAK);

	if (sw_overrun)
		cmn_err(CE_WARN, "se%d: Buffer overrun", sc->h.sc_unit);

	if (hw_overrun)
		cmn_err(CE_WARN, "se%d: fifo overrun", sc->h.sc_unit);

	/*
	 * Take care of any messages on the rdone queue
	 */

	PORT_LOCK(sc);
	while (canputnext(rq)) {
		DRIVER_LOCK;	/* Can't play with this list without locking */
		if (sc->sc_rdone_head) { /* Is something on the rdone list? */
			bp = sc->sc_rdone_head;
			sc->sc_rdone_head = bp->b_next;
			sc->sc_rdone_count--;	/* fewer buffers on list */
			bp->b_next = NULL;
			if (sc->sc_rdone_head == NULL) {
				sc->sc_rdone_tail = NULL; /* empty list */
				if (sc->sc_rdone_count)
				    cmn_err(CE_WARN,
					"se%d softint: Bad rdone count %d",
					sc->h.sc_unit, sc->sc_rdone_count);
			}
		} else if (sc->sc_rcvblk &&
			(sc->sc_rd_cur != sc->sc_rcvblk->b_wptr)) {
			/* No finished buffer, but active buffer with data */
			sc->sc_rcvblk->b_wptr = sc->sc_rd_cur;
			sc->sc_rd_cur = sc->sc_rd_lim = NULL;
			bp = sc->sc_rcvhead;
			sc->sc_rcvblk = sc->sc_rcvhead = NULL;
			SE_TAKEBUFF(sc); /* Get another buffer if available */
		} else {
			DRIVER_UNLOCK;
			break;	/* No buffer on list, no data on buffer */
		}
		DRIVER_UNLOCK;

		PORT_UNLOCK(sc);
		putnext(rq, bp); /* Give the message to upper layer */
		PORT_LOCK(sc);

		if (sc->sc_kick_rcv_id == 0)
		    sc->sc_kick_rcv_id = /* Delay future softints */
			timeout(se_kick_rcv, sc, 3);
	}

	if (sc->sc_rdone_head) {
		/*
		 * We still have streams messages queued up :
		 * Scan rdone queue for 1st remaining high priority message,
		 * send it immediately without reguard for the fullness of
		 * the upstream queue. Subsequent softints will empty the
		 * rdone queue of any remaining high priority messages.
		 */
		DRIVER_LOCK;
		previous_bp = NULL;
		bp = sc->sc_rdone_head;
		while (bp != NULL) {
			if (bp->b_datap->db_type >= QPCTL) {
				break; /* high priority message */
			} else	{
				previous_bp = bp;
				bp = bp->b_next;
			}
		}

		if (bp != NULL) { /* found one: remove from queue and send it */
			if (previous_bp == NULL)
				sc->sc_rdone_head   = bp->b_next;
			else	previous_bp->b_next = bp->b_next;

			if (sc->sc_rdone_tail == bp)
				sc->sc_rdone_tail = previous_bp;

			sc->sc_rdone_count--; /* decrement buffer cnt */
			bp->b_next = NULL;

			DRIVER_UNLOCK;
			PORT_UNLOCK(sc);
			putnext(rq, bp);
			PORT_LOCK(sc);
		} else	{
			DRIVER_UNLOCK;
		}
	}


	/* If we still have data queued up */
	if (sc->sc_rdone_head ||
	    (sc->sc_rcvblk && (sc->sc_rd_cur != sc->sc_rcvblk->b_wptr))) {
		if (sc->sc_kick_rcv_id == 0) /* wakeup scheduled? */
			sc->sc_kick_rcv_id = /* No wakeup, schedule one */
			    timeout(se_kick_rcv, sc, KICK_RCV_TIMER);
	}

	SE_FILLSBY(sc);		/* Fill up the rstandby array */
	if (sc->z.za.zas.zas_flowctl && (sc->sc_rstandby_ptr > 1))
		se_async_flowcontrol(sc, FLOW_IN_START); /* restart input */

	sc->sc_soft_active = 0;	/* Flag that we are not still in softint */
}

/*
 * se_async_flowcontrol: turn on/off input flowcontrol for a line
 */

static void
se_async_flowcontrol(se_ctl_t *sc, int onoff)
{
	int operation;
	u_char beep = 0x07;

/*
 * This routine enables and disables input flow control on a port.
 * It will handle eight seperate cases:
 *   0 - rtscts off v2.3 chip
 *   1 - rtscts off v2.2 chip
 *   2 - xoff any chip
 *   3 - no flow control (beep instead)
 *   4 - rtscts on v2.3 chip
 *   5 - rtscts on v2.2 chip
 *   6 - xon any chip
 *   7 - no flow control on/off (clear flag)
 */

	if ((onoff == FLOW_IN_STOP) & !sc->z.za.zas.zas_flowctl) {
		operation = 0;
		sc->z.za.zas.zas_flowctl = 1;
	} else if ((onoff == FLOW_IN_START) & sc->z.za.zas.zas_flowctl) {
		operation = 0x04;
		sc->z.za.zas.zas_flowctl = 0;
	} else
	    return;			/* Must be repeat of operation */

	if (sc->z.za.za_ttycommon.t_cflag & CRTSCTS) {
		if (sc->sc_chipv23 == 0x02)
		    operation += 0;		/* v2.3 chip */
		else if (sc->sc_chipv23 == 0x01)
		    operation += 1;		/* v2.2 chip */
		else
		    cmn_err(CE_WARN,
			    "se%d: Invalid chip version %d in flowcontrol",
			    sc->h.sc_unit, sc->sc_chipv23);
	} else if (sc->z.za.za_ttycommon.t_iflag & IXOFF)
	    operation += 2;		/* xon/xoff flow control */
	else
	    operation += 3;		/* No flow control. Just beep. */

	/*
	 * Now that we've figured out what we're going to do, do it.
	 */

	switch (operation) {
/*
 * Flow off operations
 */

	    case (0): {			/* drop rts v2.3 chip */
		    REG_PUTB(sc, sab_mode, REG_GETB(sc, sab_mode) |
			sc->z.za.za_flon_mask | SAB_MODE_RTS);
		    break;
	    }
	    case (1): {			/* drop rts v2.2 chip */
		    sc->sc_chip->sec_pvr &= ~sc->sc_rtsbit;	/* Drop RTS */
		    REG_PUTB(sc, sab_pvr, sc->sc_chip->sec_pvr);
		    break;
	    }
	    case (2): {			/* send xoff */
		    if (REG_GETB(sc, sab_star) & SAB_STAR_TEC) /* busy? */
			sc->sc_char_pending =
			    sc->z.za.za_ttycommon.t_stopc; /* Yes. wait. */
		    else
			REG_PUTB(sc, sab_tic, sc->z.za.za_ttycommon.t_stopc);
		    break;
	    }
	    case (3): {			/* no flowctl */
		    if (!(REG_GETB(sc, sab_star) & SAB_STAR_TEC))
			REG_PUTB(sc, sab_tic, beep); /* Send bell */
		    break;
	    }

/*
 * Flow on operations
 */

	    case (4): {	/* Put v2.3 chip back into auto RTS mode */
		    REG_PUTB(sc, sab_mode, (REG_GETB(sc, sab_mode) |
				sc->z.za.za_flon_mask) & ~SAB_MODE_RTS);
		    break;
	    }
	    case (5): {			/* raise rts v2.2 chip */
		    sc->sc_chip->sec_pvr |= sc->sc_rtsbit; /* raise RTS */
		    REG_PUTB(sc, sab_pvr, sc->sc_chip->sec_pvr);
		    break;
	    }
	    case (6): {			/* send xon */
		    if (REG_GETB(sc, sab_star) & SAB_STAR_TEC) /* busy? */
			sc->sc_char_pending = /* Bust. wait til avail. */
			    sc->z.za.za_ttycommon.t_startc;
		    else
			REG_PUTB(sc, sab_tic, sc->z.za.za_ttycommon.t_startc);
		    break;
	    }
	    case (7):
		break;	/* no flow control, don't do anything */
	    default:
		break;	/* Really should complain here. */
	}
}

/*
 * se_async_rcv_flags: Set mask and input flags for a line.
 */

static void
se_async_rcv_flags(se_ctl_t *sc)
{

	ASSERT(mutex_owned(&se_hi_excl));

/*
 * Set the input mask size
 */

	switch (sc->z.za.za_ttycommon.t_cflag & CSIZE) {
	    case (CS5):
		sc->z.za.za_rcv_mask = 0x1f; /* Five bit. Yeah, right. */
		break;
	    case (CS6):
		sc->z.za.za_rcv_mask = 0x3f; /* Six bit. Unh huh. */
		break;
	    case (CS7):
		sc->z.za.za_rcv_mask = 0x7f; /* Seven bit. old ascii */
		break;
	    case (CS8):
		sc->z.za.za_rcv_mask = 0xff; /* Eight bit. modern ascii. */
		break;
	    default:
		cmn_err(CE_WARN, "se%d: Impossible character size.",
			sc->h.sc_unit);
	}

/*
 * Figure out if parity will be reported to upper layers as escaped data
 */

	if ((sc->z.za.za_ttycommon.t_iflag & (PARMRK | IGNPAR | ISTRIP))
	    == PARMRK)
	    sc->z.za.zas.zas_do_esc = 1; /* Flag that we must escape 0xff's. */
	else
	    sc->z.za.zas.zas_do_esc = 0; /* Disable escaping. */
}

/*
 * se_break_end: Timeout call for us to stop sending a break
 */

static void
se_break_end(void *arg)
{
	se_ctl_t *sc = arg;

	PORT_LOCK(sc);
	REG_PUTB(sc, sab_dafo, REG_GETB(sc, sab_dafo) & ~SAB_DAFO_XBRK);
	sc->z.za.zas.zas_break = 0;
	sc->z.za.za_break_timer = 0;
	se_async_start(sc);	/* Attempt to restart output */
	PORT_UNLOCK(sc);
}


/*
 * se_async_start: Start sending data if appropriate
 */

static void
se_async_start(se_ctl_t *sc)
{
	queue_t *wq;
	mblk_t *mp;
	int data_count;

	ASSERT(mutex_owned(sc->h.sc_excl));
	if (sc->z.za.zas.zas_break || sc->z.za.zas.zas_draining)
	    return;			/* Do nothing at this point. */

	wq = sc->z.za.za_ttycommon.t_writeq;
	if (wq == NULL)
	    return;	/* Do nothing if no write queue */

	while (mp = getq(wq))	/* Loop taking messages off of write queue */
	    switch (mp->b_datap->db_type) {
		case (M_BREAK): {		/* Request to send a break */
			if (sc->sc_xmit_active) {
				(void) putbq(wq, mp); /* try later */
				return;
			}

			if (*(int *)mp->b_cont->b_rptr) break; /* ldterm hack */

			/* Start break sequence */
			DRIVER_LOCK;
			REG_PUTB(sc, sab_dafo, REG_GETB(sc, sab_dafo) |
				SAB_DAFO_XBRK);
			sc->z.za.zas.zas_break = 1;
			DRIVER_UNLOCK;
			se_ack(mp, NULL, 0); /* Turn message into an ack */
			DRIVER_LOCK;
			SE_PUTQ(sc, mp); /* Put message on return queue */
			SE_SETSOFT(sc);
			DRIVER_UNLOCK;

			/* Ask to be called back in 1/4 second. */
			sc->z.za.za_break_timer = timeout(se_break_end,
			    sc, hz / 4);
			break;
		}

		case (M_IOCTL): { /* Request to perform an ioctl */
			if (sc->sc_xmit_active) {
				(void) putbq(wq, mp); /* If busy, try later */
				return;
			}
			se_async_ioctl(sc, wq, mp);
			continue;
		}


		case (M_DATA): {

			/*
			 * if stopped or currently active try later
			 */
			if (sc->z.za.zas.zas_stopped ||
			    (sc->sc_wr_cur && sc->sc_xmit_active)) {
				(void) putbq(wq, mp); /* try later */
				return;
			}

			/*
			 * If not active and current xmit not complete
			 * start it up again
			 */
			if (sc->sc_wr_cur) {
				(void) putbq(wq, mp); /* try later */
				DRIVER_LOCK;
				se_async_txint(sc);
				DRIVER_UNLOCK;
				return;
			}

			data_count = mp->b_wptr - mp->b_rptr;
			if (data_count == 0) {
				freemsg(mp); /* Nothing to send, drop it */
				continue; /* Try again. */
			}

			DRIVER_LOCK;
			sc->sc_wr_cur = mp->b_rptr; /* Set up message */
			sc->sc_wr_lim = mp->b_wptr;
			sc->sc_xmithead = sc->sc_xmitblk = mp;

			se_async_txint(sc);	/* simulate an XFW interrupt */
			DRIVER_UNLOCK;
			continue; /* And try again - just in case. */
		}

		default:
		    cmn_err(CE_WARN,
			    "se%d: Illegal message type %d on write queue",
			    sc->h.sc_unit, mp->b_datap->db_type);
		    freemsg(mp);

	    } /* End while and switch */
}

/*
 * se_async_program: Set up chip for use as async line.
 */

static void
se_async_program(se_ctl_t *sc, boolean_t toggle_baud_rate)
{

	u_char mode, dafo, ccr2, ccr4;
	u_short imr;
	int baudrate, divisor;


	/* Make sure we belong here. */
	ASSERT(mutex_owned(&se_hi_excl));
	ASSERT(mutex_owned(sc->h.sc_excl));

	sc->z.za.zas.zas_draining = 1;

	while (sc->sc_xmit_active) {	/* Wait for things to quiet down */
		DRIVER_UNLOCK;
		PORT_UNLOCK(sc);
		DELAY(1000);
		PORT_LOCK(sc);
		DRIVER_LOCK;
	}

	sc->z.za.zas.zas_draining = 0;

	/* save new parameters */
	sc->z.za.za_iflag = sc->z.za.za_ttycommon.t_iflag;
	sc->z.za.za_cflag = sc->z.za.za_ttycommon.t_cflag;
	sc->z.za.za_stopc = sc->z.za.za_ttycommon.t_stopc;
	sc->z.za.za_startc = sc->z.za.za_ttycommon.t_startc;

	/* Lots of interrupts to pay attention to */
	imr =   SAB_ISR_TCD  | SAB_ISR_TIME | SAB_ISR_FERR | SAB_ISR_CDSC |
		SAB_ISR_RFO  | SAB_ISR_RPF  | SAB_ISR_BRKT | SAB_ISR_ALLS |
		SAB_ISR_TIN  | SAB_ISR_CSC  | SAB_ISR_XPR  | SAB_ISR_BRK;

	if (sc->z.za.za_ttycommon.t_iflag & IXON) {
		/*
		 * Don't set SAB_MODE_FLON feature if IXANY set,
		 * but do ask for interrupts
		 */
		if ((sc->z.za.za_ttycommon.t_iflag & IXANY) == 0)
		    sc->z.za.za_flon_mask = SAB_MODE_FLON; /* write-only bit */
		imr |= SAB_ISR_XOFF | SAB_ISR_XON; /* Ask for xon/xoff ints */
	} else
	    sc->z.za.za_flon_mask = 0;

	mode = sc->z.za.za_flon_mask;			/* Start empty */

	if ((sc->z.za.za_ttycommon.t_cflag & CREAD) ||
	    (sc->sc_dev == rconsdev) || (sc->sc_dev == kbddev) ||
	    (sc->sc_dev == stdindev))
	    mode |= SAB_MODE_RAC; /* Enable rcvr (always on special devices) */

	if (!(sc->z.za.za_ttycommon.t_cflag & CRTSCTS))
	    mode |= SAB_MODE_FCTS;	/* Ignore cts unless RTS flowctl */

	if (!(sc->z.za.za_ttycommon.t_cflag & CRTSXOFF))
	    mode |= SAB_MODE_FRTS;	/* Drop RTS when input buffer full */

	/* Set up baudrate divisor based on type of chip and clock */
	baudrate = sc->z.za.za_ttycommon.t_cflag & CBAUD;
	if (sc->z.za.za_ttycommon.t_cflag & CBAUDEXT)
	    baudrate += 16;
	if (baudrate > N_SE_SPEEDS) baudrate = B9600;
	if (sc->sc_chipv23 == 0x01) {
		divisor = v22_se_speeds[baudrate];
	} else {
		divisor = v31_se_speeds[baudrate];
		/* Set fast edges if baud rate ever exceeds 100kb */
		if (baudrate > B76800) sc->sc_chip->sec_pvr &= ~SAB_PVR_FAST;
	}

	/*
	 * set input speed same as output, as split speed not supported
	 */
	if (sc->z.za.za_ttycommon.t_cflag & (CIBAUD|CIBAUDEXT)) {
		sc->z.za.za_ttycommon.t_cflag &= ~(CIBAUD);
		if (baudrate > CBAUD) {
			sc->z.za.za_ttycommon.t_cflag |= CIBAUDEXT;
			sc->z.za.za_ttycommon.t_cflag |=
			    (((baudrate - CBAUD - 1) << IBSHIFT) & CIBAUD);
		} else {
			sc->z.za.za_ttycommon.t_cflag &= ~CIBAUDEXT;
			sc->z.za.za_ttycommon.t_cflag |=
			    ((baudrate << IBSHIFT) & CIBAUD);
		}
	}

	se_async_rcv_flags(sc);	/* Set up character masks */

	/* Set character size */
	switch (sc->z.za.za_ttycommon.t_cflag & CSIZE) {
	    case (CS5): dafo = SAB_DAFO_5BIT; break;
	    case (CS6): dafo = SAB_DAFO_6BIT; break;
	    case (CS7): dafo = SAB_DAFO_7BIT; break;
	    case (CS8): dafo = SAB_DAFO_8BIT; break;
	    default:   dafo = SAB_DAFO_8BIT;
	}

	if (sc->z.za.za_ttycommon.t_cflag & PARENB) {
		dafo |= SAB_DAFO_PARE;	/* Enable parity generation */
		if (sc->z.za.za_ttycommon.t_iflag & INPCK)
		    imr |= SAB_ISR_PERR;	/* parity detection */
		if (sc->z.za.za_ttycommon.t_cflag & PARODD)
		    dafo |= SAB_DAFO_ODD;	/* Parity type */
		else
		    dafo |= SAB_DAFO_EVEN;
	}

	if (sc->z.za.za_ttycommon.t_cflag & CSTOPB)
		dafo |= SAB_DAFO_STOP;  /* Set 2 stop bits */

	if (divisor >= 0) {
		ccr2 = SAB_CCR2_SSEL | SAB_CCR2_BDF;
		/* Following are two extention bits to the baud rate */
		if (divisor & 0x100) ccr2 |= SAB_CCR2_BR8;
		if (divisor & 0x200) ccr2 |= SAB_CCR2_BR9;
	} else {
		/* Disable brg, giving maximum clock rate */
		ccr2 = SAB_CCR2_SSEL;
		divisor = 0;
	}
	if (sc->sc_chipv23 > 0x01) {
		ccr4 = SAB_CCR4_MCK4 | SAB_CCR4_EBRG;
	} else {
		ccr4 = 0x0;
	}

	sc->sc_chip->sec_pim |= sc->sc_dsrbit; /* Interrupts for DSR */

/*
 * Now that we've set everything up, write out to the chip.
 */

	REG_PUTB(sc, sab_ccr0, SAB_CCR0_MCE | SAB_CCR0_NRZ | SAB_CCR0_ASY);
	(void) REG_GETW(sc, sab_isr);	/* Wipe pending interrupts */
	REG_PUTB(sc, sab_cmdr, SAB_CMDR_RRES | SAB_CMDR_XRES); /* Reset r&w */
	sc->sc_xmit_active = 0;	/* Not active any more */
	REG_PUTB(sc, sab_pcr,  sc->sc_chip->sec_pcr);
	REG_PUTB(sc, sab_ccr1, SAB_CCR1_ODS | SAB_CCR1_BCR | SAB_CCR1_CM7);
	REG_PUTB(sc, sab_pvr, sc->sc_chip->sec_pvr);

	/*
	 * Fix to Siemens 82532 V3.1 hdlc->async problem part 1: set the baud
	 * rate divisor to zero, forcing ghost characters which appear
	 * intermittantly when rapidly closing HDLC then opening ASYNC
	 * into the input fifo
	 */
	if (toggle_baud_rate) {
		REG_PUTB(sc, sab_bgr,  0);
	} else {
		REG_PUTB(sc, sab_bgr, (divisor & 0xFF));
	}

	REG_PUTB(sc, sab_ccr2, ccr2);
	REG_PUTB(sc, sab_ccr3, 0);
	REG_PUTB(sc, sab_ccr4, ccr4);
	REG_PUTB(sc, sab_mode, mode);
	REG_PUTB(sc, sab_dafo, dafo);
	REG_PUTB(sc, sab_rfc,  SAB_RFC_DPS | SAB_RFC_RF32);
	REG_PUTB(sc, sab_ipc,  SE_INTERRUPT_CONFIG);
	REG_PUTB(sc, sab_xbch, 0);
	REG_PUTB(sc, sab_mxn,  ~sc->z.za.za_rcv_mask);
	REG_PUTB(sc, sab_mxf,  ~sc->z.za.za_rcv_mask);
	REG_PUTB(sc, sab_xon,  sc->z.za.za_ttycommon.t_startc);
	REG_PUTB(sc, sab_xoff, sc->z.za.za_ttycommon.t_stopc);
	REG_PUTB(sc, sab_pim,  sc->sc_chip->sec_pim);
	REG_PUTB(sc, sab_ccr0, SAB_CCR0_PU | SAB_CCR0_MCE |
		SAB_CCR0_NRZ | SAB_CCR0_ASY);

	PUT_CMDR(sc, SAB_CMDR_RRES | SAB_CMDR_XRES);

	/*
	 * Fix to Siemens 82532 V3.1 hdlc->async problem part 2: wait 25
	 * microseconds, reset transmit & receive, wait another 25
	 * microseconds, then write the correct baud rate divisor to the
	 * bgr register and reset transmit & receive. This clears ghost
	 * characters from input fifo
	 */
	if (toggle_baud_rate) {
		drv_usecwait(25);
		PUT_CMDR(sc, SAB_CMDR_RRES | SAB_CMDR_XRES);
		drv_usecwait(25);
		REG_PUTB(sc, sab_bgr,  (divisor & 0xff));
		PUT_CMDR(sc, SAB_CMDR_RRES | SAB_CMDR_XRES);
	}

	/* Interrupt mask is a word, can't use REG_PUTB */
	sc->sc_imr = imr;
	imr = swapit(imr);
	REG_PUTW(sc, sab_imr, ~imr);

	if (baudrate)		/* Zero baudrate means drop dtr */
	    (void) se_mctl(sc, SE_ON, DMSET); /* Turn on RTS and DTR */
	else
	    (void) se_mctl(sc, SE_OFF, DMSET); /* Turn off RTS and DTR */
}

/*
 * se_async_suspend and se_async_resume: Allow CPR to start/stop us
 */

static void
se_async_suspend(se_ctl_t *sc)
{
	mblk_t *mp;

	PORT_LOCK(sc);
	DRIVER_LOCK;		/* Keep everyone else locked out */

	PUT_CMDR(sc, SAB_CMDR_RRES | SAB_CMDR_XRES);
	/* comment out this line when using tip line */
#if 0
	REG_PUTB(sc, sab_ccr0, 0); /* Power-on bit gets cleared */
#endif
	sc->sc_suspend = 1;	/* Flag that this device is hereby suspended */
	sc->sc_xmit_active = 0;	/* Not active any more */

	SE_GIVE_RCV(sc);	/* Close out any input buffer */
	mp = sc->sc_xmitblk;	/* Pick up any transmit buffer */
	sc->sc_xmitblk = sc->sc_xmithead = NULL;
	sc->sc_wr_cur = sc->sc_wr_lim = NULL;
	if (sc->z.za.zas.zas_break) {
		sc->z.za.zas.zas_break = 0;
		(void) untimeout(sc->z.za.za_break_timer);
	}

	DRIVER_UNLOCK;
	PORT_UNLOCK(sc);
	if (mp)
	    freemsg(mp);	/* Free transmit message, if any. */
}

static void
se_async_resume(se_ctl_t *sc)
{
	PORT_LOCK(sc);
	DRIVER_LOCK;

	/* We can only get here if we are open, so assume we were running. */

	sc->sc_xmit_active = 0;
	se_async_program(sc, B_TRUE); /* Initialize the chip */
	SE_SETSOFT(sc);		/* request background service */
	DRIVER_UNLOCK;

	SE_FILLSBY(sc);		/* reallocate input buffers */
	se_async_start(sc);	/* re-start transmit if possible */
	PORT_UNLOCK(sc);
}


/*
 * se_async_open: Initialize things for async.
 */

static int
se_async_open(se_ctl_t *sc, queue_t *rq, int flag, cred_t *cr, dev_t *dev)
{
	int    mbits, len;
	struct termios *termiosp;

	ASSERT(mutex_owned(sc->h.sc_excl));
	SE_CHECK_SUSPEND(sc);	/* Before we start, make sure not suspending */

/*
 * Sanity checks if we are already open
 */
	if (sc->z.za.zas.zas_isopen) {
		if ((sc->z.za.za_ttycommon.t_flags & TS_XCLUDE) &&
		    !drv_priv(cr))
		    return (EBUSY);
	}

	sc->sc_dev = *dev;
	if (!sc->z.za.zas.zas_isopen) { /* If not already open, initialize */

/*
 * Get prom settings for device.
 */
		if (ddi_getlongprop(DDI_DEV_T_ANY, ddi_root_node(), 0,
				    "ttymodes", (caddr_t)&termiosp, &len)
		    == DDI_PROP_SUCCESS && (len == sizeof (struct termios))) {
			sc->z.za.za_ttycommon.t_cflag = termiosp->c_cflag;
			kmem_free(termiosp, len);
		}

		if (sc->sc_softcar) /* Carry forward prom setting */
		    sc->z.za.za_ttycommon.t_flags |= TS_SOFTCAR;

/*
 * Zero remaining part of ttycommon.
 */

		sc->z.za.za_ttycommon.t_iflag = 0;
		sc->z.za.za_ttycommon.t_size.ws_row = 0;
		sc->z.za.za_ttycommon.t_size.ws_col = 0;
		sc->z.za.za_ttycommon.t_size.ws_xpixel = 0;
		sc->z.za.za_ttycommon.t_size.ws_ypixel = 0;
		sc->z.za.za_sw_overrun = 0;
		sc->z.za.za_hw_overrun = 0;
		sc->z.za.za_ttycommon.t_iocpending = NULL;
/*
 * Initialize per-open data
 */

		DRIVER_LOCK;		/* Lock interrupts out */
		sc->sc_bufsize = se_default_asybuf;
		se_async_program(sc, B_TRUE);	/* Initialize the chip */
		if (sc->h.sc_protocol == OUTD_PROTOCOL)
		    sc->z.za.zas.zas_out = 1;
	} else
		DRIVER_LOCK;		/* Lock interrupts out */

/*
 * Turn on modem control leads
 */

	mbits = se_mctl(sc, SE_ON, DMSET);
	if ((mbits & TIOCM_CD) || (sc->z.za.za_ttycommon.t_flags & TS_SOFTCAR))
	    sc->z.za.zas.zas_carr_on = 1;

	DRIVER_UNLOCK;		/* Allow interrupts again */

/*
 * See if we should wait to complete the open.
 * If the caller has not disallowed waiting, and it isn't a local line,
 * and carrier isn't up yet, and it isn't an outdial device, wait for carrier
 * or it is already open in outdial mode while async was waiting for carrier.
 */
	if (!(flag & (FNDELAY|FNONBLOCK)) &&
	    !(sc->z.za.za_ttycommon.t_cflag & CLOCAL) &&
	    ((!sc->z.za.zas.zas_carr_on && !sc->z.za.zas.zas_out) ||
	    (sc->z.za.zas.zas_out && !(*dev & OUTLINE)))) {
		sc->z.za.zas.zas_wopen = 1; /* - Waiting for open */
		if (cv_wait_sig(&sc->sc_flags_cv, sc->h.sc_excl) == 0) {
			/*
			 * Signal arrived : abort open
			 */
			if (!sc->z.za.zas.zas_isopen) {
				/*
				 * There are no other processes with current
				 * opens on this device : Clean up
				 */
				qprocson(rq); /* Keep qprocsoff from hanging */
				(void) se_async_close(sc, rq);
			}
			sc->z.za.zas.zas_wopen = 0; /* Not waiting any more. */
			return (EINTR);
		} else	{
			/*
			 * Carrier is now up: return -1, forcing streams to
			 * retry the open
			 */
			sc->z.za.zas.zas_wopen = 0; /* Not waiting any more. */

			/*
			 * If already open in outdial mode which can happen
			 * when setup for bidirectional mode, then indicate
			 * that the port is busy.
			 */
			if (!(*dev & OUTLINE) && sc->z.za.zas.zas_isopen &&
			    sc->z.za.zas.zas_out)
				return (EBUSY);
			else
				return (-1);
		}
	}

	/* We're open now. Finish setting up the device. */

	sc->z.za.za_ttycommon.t_readq = rq;
	sc->z.za.za_ttycommon.t_writeq = WR(rq);
	rq->q_ptr = WR(rq)->q_ptr = (void *)sc;
	sc->z.za.zas.zas_isopen = 1;
	SE_FILLSBY(sc);
	qprocson(rq);

	/*
	 * V3.1 & V3.2 of the Siemens chip occasionally looses characters
	 * unless there is a delay between the conclusion of the open and the
	 * first read. This delay value was arrived at empirically.
	 */
	delay(drv_usectohz(100000));


	return (0);
}

/*
 * se_async_close: Shut things down, clean up.
 */

static int
se_async_close(se_ctl_t *sc, queue_t *rq)
{
	mblk_t *bp, *bp1;
	time_t close_time, now;

	sc->sc_closing = 1;
	sc->z.za.zas.zas_draining = 1; /* Stop any further output */

/*
 * wait while output or break in progress, or while softint being handled
 */

	(void) drv_getparm(TIME, &close_time);

	while (sc->sc_soft_active ||
	    (sc->z.za.zas.zas_carr_on &&
	    (sc->sc_xmit_active || sc->z.za.zas.zas_break))) {

		if (cv_wait_sig(&sc->sc_flags_cv, sc->h.sc_excl) == 0)
			break;

		/* Get current time to re-compute */
		(void) drv_getparm(TIME, &now);
		if ((now - close_time) > 5)
			break; /* Don't wait forever. */

		SE_CHECK_SUSPEND(sc);	/* Allow CPR to hash system */
	}


	/* Disable all interrupts for this channel */
	REG_PUTW(sc, sab_imr, 0xffff);

/* If break in progress, stop it. Can only get here if carrier dropped. */

	if (sc->z.za.zas.zas_break) {
		(void) untimeout(sc->z.za.za_break_timer);
		sc->z.za.za_break_timer = 0;
		DRIVER_LOCK;
		REG_PUTB(sc, sab_dafo,
			REG_GETB(sc, sab_dafo) & ~SAB_DAFO_XBRK);
		sc->z.za.zas.zas_break = 0;
		DRIVER_UNLOCK;
	}

	/* Cancel buffer callback if any */
	if (sc->sc_bufcid) {
		unbufcall(sc->sc_bufcid);
		sc->sc_bufcid = 0;
	}

	/* Cancel kick_rcv callback if any */
	if (sc->sc_kick_rcv_id) {
		PORT_UNLOCK(sc);
		(void) untimeout(sc->sc_kick_rcv_id);
		PORT_LOCK(sc);
		sc->sc_kick_rcv_id = 0;
	}

	/* If non-console, disable rcv/xmit, and drop DTR/RTS */
	if ((sc->sc_dev != rconsdev) && (sc->sc_dev != kbddev) &&
	    (sc->sc_dev != stdindev)) {
		DRIVER_LOCK;
		REG_PUTB(sc, sab_mode,
			(REG_GETB(sc, sab_mode) | sc->z.za.za_flon_mask)
			& ~SAB_MODE_RAC);
		PUT_CMDR(sc, SAB_CMDR_XRES | SAB_CMDR_RRES);
		sc->sc_xmit_active = 0;
		if (sc->z.za.za_ttycommon.t_cflag & HUPCL)
		    (void) se_mctl(sc, SE_OFF, 0); /* Drop DTR and RTS */
		DRIVER_UNLOCK;
	}

/*
 * Tell upper layers that we are closed.
 * Cast the argument because of volatile.
 */

	ttycommon_close((tty_common_t *)&sc->z.za.za_ttycommon);

	sc->z.za.za_ttycommon.t_readq = sc->z.za.za_ttycommon.t_writeq = NULL;

	/* Clean up buffers */
	DRIVER_LOCK;

	/* Should already be null */
	sc->sc_xmithead = sc->sc_xmitblk = NULL;
	sc->sc_wr_lim = sc->sc_wr_cur = NULL;
	bp = sc->sc_rcvhead;		/* save current receive block */
	sc->sc_rcvhead = sc->sc_rcvblk = NULL;
	sc->sc_rd_lim = sc->sc_rd_cur = NULL;
	while (sc->sc_rstandby_ptr > 0) { /* Get standby buffers */
		sc->sc_rstandby_ptr--;	/* Point to available buffer */
		bp1 = sc->sc_rstandby[sc->sc_rstandby_ptr];
		sc->sc_rstandby[sc->sc_rstandby_ptr] = NULL;
		bp1->b_next = bp;
		bp = bp1;
	}
	if (sc->sc_rdone_head) {	/* Get completed input messages */
		for (bp1 = sc->sc_rdone_head; bp1 != NULL; bp1 = bp1->b_next) {
			sc->sc_rdone_count--;	/* Decrement by buffer */
		}
		if (sc->sc_rdone_count) {
			cmn_err(CE_WARN, "se%d close: Bad rdone count %d",
				sc->h.sc_unit, sc->sc_rdone_count);
			sc->sc_rdone_count = 0;
		}
		sc->sc_rdone_tail->b_next = bp;
		bp = sc->sc_rdone_head;
		sc->sc_rdone_head = sc->sc_rdone_tail = NULL;
	}
	DRIVER_UNLOCK;

	/* Free all the receive messages that we removed from queues */
	while (bp) {
		bp1 = bp->b_next;
		bp->b_next = NULL;
		freemsg(bp);
		bp = bp1;
	}

	qprocsoff(rq);		/* Disable streams activity */

	if (sc->z.za.zas.zas_out && sc->z.za.zas.zas_wopen) {
		/*
		 * an async open is blocked while being used for dialout
		 * setup to allow async open to continue
		 */
		sc->h.sc_ops = &se_async_ops;
		sc->h.sc_protocol = ASYN_PROTOCOL;
		sc->z.za.zas.zas_out = 0;
	} else {
		/* Not async operations any more. */
		sc->h.sc_ops = &se_null_ops;
		sc->h.sc_protocol = NULL_PROTOCOL;
	}
	sc->sc_closing = 0;

	cv_broadcast(&sc->sc_flags_cv); /* Notify any waiters */

	return (0);		/* Indicate we are closed */
}

/*
 * se_async_wput: Handle message directly from streams.
 */

static int
se_async_wput(se_ctl_t *sc, queue_t *wq, mblk_t *mp)
{

	mblk_t *bp;		/* Temporary buffer pointer */
/*
 * Handle according to message type
 */

	switch (mp->b_datap->db_type) {
	    case (M_DATA):	/* Send various forms of data */
	    case (M_DELAY):
	    case (M_BREAK):
		(void) putq(wq, mp); /* queue them up behind other activity */
		se_async_start(sc);	/* Kick the transmitter */
		break;

	    case (M_STARTI):	/* Send Start Character */
		while (REG_GETB(sc, sab_star) & SAB_STAR_TEC) {
			delay(hz/100); /* wait for TIC register to clear */
		}

		REG_PUTB(sc, sab_tic, sc->z.za.za_ttycommon.t_startc);
		freemsg(mp); /* Streams does not expect reply: Discard */
		break;

	    case (M_STOPI):	/* Send Stop Character */
		while (REG_GETB(sc, sab_star) & SAB_STAR_TEC) {
			delay(hz/100); /* wait for TIC register to clear */
		}

		REG_PUTB(sc, sab_tic, sc->z.za.za_ttycommon.t_stopc);
		freemsg(mp); /* Streams does not expect reply: Discard */
		break;

	    case (M_STOP):	/* Stop output in progress */
		if (!sc->z.za.zas.zas_stopped) {
			DRIVER_LOCK;
			sc->z.za.zas.zas_stopped = 1;
			DRIVER_UNLOCK;
		}
		break;

	    case (M_START):	/* re-start output */
		if (sc->z.za.zas.zas_stopped) {
			DRIVER_LOCK;
			sc->z.za.zas.zas_stopped = 0;
			DRIVER_UNLOCK;
		}
		se_async_start(sc);	/* kick the transmitter */
		break;

	    case (M_CTL):	/* Obsolete */
		freemsg(mp); /* Don't care about this. */
		break;

	    case (M_FLUSH):	/* Abort I/O. */
		if (*mp->b_rptr & FLUSHW) {	/* Flush output */

/*
 * Zap chip and clean up current transmit buffer
 */
			DRIVER_LOCK;
			bp = sc->sc_xmithead;
			sc->sc_xmithead = sc->sc_xmitblk = NULL;
			sc->sc_wr_lim = sc->sc_wr_cur = NULL;
			DRIVER_UNLOCK;

			if (bp)
			    freemsg(bp); /* free message if any */

			flushq(wq, FLUSHDATA); /* Flush data in the pipe */
			*mp->b_rptr &= ~FLUSHW; /* Clear flush write bit */
		}
		if (*mp->b_rptr & FLUSHR) {	/* Flush input */
			DRIVER_LOCK;
			if (sc->sc_rcvhead != NULL)
			    SE_GIVE_RCV(sc); /* Close current buffer */
			SE_SETSOFT(sc);
			DRIVER_UNLOCK;

			flushq(RD(wq), FLUSHDATA); /* Flush data in pipe */

			DRIVER_LOCK;
			SE_PUTQ(sc, mp); /* Return message to caller */
			DRIVER_UNLOCK;
		} else {	/* Do not flush input */
			freemsg(mp); /* Return to freepool */
		}

		se_async_start(sc);	/* restart output if any */
		break;

	    case (M_IOCDATA): { /* ioctl to play with dataset leads */
		    struct copyresp *rp = (void *)mp->b_rptr;
		    int operation;

		    if (rp->cp_rval) { /* Prefailed? */
			    freemsg(mp); /* Yup. Just free and go away. */
			    break;
		    }

		    switch (rp->cp_cmd) {
			case (TIOCMSET): operation = DMSET; break;
			case (TIOCMGET): operation = DMGET; break;
			case (TIOCMBIS): operation = DMBIS; break;
			case (TIOCMBIC): operation = DMBIC; break;
			default:
			    freemsg(mp);
			    break;
		    }

		    DRIVER_LOCK;
		    if (rp->cp_cmd != TIOCMGET) /* if change dsl */
			(void) se_mctl(sc, *(int *)mp->b_cont->b_rptr,
					operation);
		    DRIVER_UNLOCK;
		    se_ack(mp, NULL, 0); /* Make message into an ack */
		    DRIVER_LOCK;
		    SE_PUTQ(sc, mp); /* Put message on return queue */
		    SE_SETSOFT(sc);
		    DRIVER_UNLOCK;
		    break;
	    }

	    case (M_IOCTL): {
		    switch (((struct iocblk *)(mp->b_rptr))->ioc_cmd) {
			case (TCSETSW): /* to be queued behind data. */
			case (TCSETSF):
			case (TCSETAW):
			case (TCSETAF):
			case (TCSBRK):
			    (void) putq(wq, mp);
			    se_async_start(sc); /* wakeup transnmit */
			    break;

			default:		/* Anything else, do it now. */
			    se_async_ioctl(sc, wq, mp);
			    break;
		    }
		    break;
	    }

	    default:
		freemsg(mp); /* Dunno what this is. Ignore it. */
		break;
	}

	return (0);
}

/*
 * se_async_ioctl: Handle non-trival ioctls.
 */

static void
se_async_ioctl(se_ctl_t *sc, queue_t *wq, mblk_t *mp)
{
	struct iocblk *ioc = (void *)mp->b_rptr;
	int error;

	SE_CHECK_SUSPEND(sc);	/* Allow CPR to take system away */

	PORT_UNLOCK(sc);	/* Don't hold lock across this call */
	(void) ttycommon_ioctl((void *)&sc->z.za.za_ttycommon, wq, mp, &error);
	PORT_LOCK(sc);
	sc->sc_softcar = (sc->z.za.za_ttycommon.t_flags & TS_SOFTCAR) ? 1 : 0;

	DRIVER_LOCK;		/* Keep interrupts out of our hair */
	if (error == 0) {
		/*
		 * only re-program the chip if any of the
		 * fields that affect HW change
		 */
		if ((sc->z.za.za_iflag != sc->z.za.za_ttycommon.t_iflag) ||
		    (sc->z.za.za_cflag != sc->z.za.za_ttycommon.t_cflag) ||
		    (sc->z.za.za_stopc != sc->z.za.za_ttycommon.t_stopc) ||
		    (sc->z.za.za_startc != sc->z.za.za_ttycommon.t_startc)) {

			/* Assume chip needs to be told */
			se_async_program(sc, B_FALSE);
		}

#ifdef POSIX_INTERPRETATION_NEEDED
		/*
		 * If called with TCSETS, then enable the chip to be
		 * re_programmed immediately, by clearing the transmit
		 * fifos.
		 */
		if (ioc->ioc_cmd == TCSETS) {
			sc->sc_xmit_active = 0;
			REG_PUTB(sc, sab_cmdr, SAB_CMDR_XRES);
		}
		se_async_program(sc, B_FALSE); /* chip needs to be told */
#endif /* POSIX_INTERPRETATION_NEEDED */
	} else if (error < 0) {
		error = 0;	/* Go back to assuming no error */
		switch (ioc->ioc_cmd) { /* ttycommon didn't help. */

		    case (TCSBRK): /* Send a defined break */
			if (*(int *)mp->b_cont->b_rptr == 0) {
				int rate, speed;
				/*
				 * Delay 1 char time before xmitting the break
				 */
				rate = sc->z.za.za_ttycommon.t_cflag & CBAUD;
				if (sc->z.za.za_ttycommon.t_cflag & CBAUDEXT)
					rate += 16;
				if (rate > N_SE_SPEEDS)
					rate = B9600;
				speed = DIV2RATE(v31_se_speeds[rate]);
				DRIVER_UNLOCK;
				delay(((hz * NBBY)/speed) + 1);
				DRIVER_LOCK;

				REG_PUTB(sc, sab_dafo, REG_GETB(sc, sab_dafo)
					| SAB_DAFO_XBRK);
				/* Ask to be called back in 1/4 second. */
				sc->z.za.zas.zas_break = 1;
				sc->z.za.za_break_timer =
				    timeout(se_break_end, sc, hz/4);
			}
			DRIVER_UNLOCK;
			se_ack(mp, NULL, 0); /* Turn message into an ack */
			DRIVER_LOCK;
			break;

		    case (TIOCSBRK): /* Start an unlimited break */
			REG_PUTB(sc, sab_dafo, REG_GETB(sc, sab_dafo) |
				SAB_DAFO_XBRK);
			sc->z.za.zas.zas_break = 1;
			DRIVER_UNLOCK;
			se_ack(mp, NULL, 0);	/* Turn message into an ack */
			DRIVER_LOCK;
			break;

		    case (TIOCCBRK): /* end an unlimited break */
			REG_PUTB(sc, sab_dafo, REG_GETB(sc, sab_dafo) &
				~SAB_DAFO_XBRK);
			sc->z.za.zas.zas_break = 0;
			DRIVER_UNLOCK;
			se_ack(mp, NULL, 0);	/* Turn message into an ack */
			DRIVER_LOCK;
			break;

		    case (tIOC|109):		/* TIOCSILOOP */
			REG_PUTB(sc, sab_mode, REG_GETB(sc, sab_mode) |
				SAB_MODE_TLP);
			DRIVER_UNLOCK;
			se_ack(mp, NULL, 0);	/* Turn message into an ack */
			DRIVER_LOCK;
			break;

		    case (TIOCMSET):		/* Modem diddling */
		    case (TIOCMBIS):
		    case (TIOCMBIC):
			if (ioc->ioc_count == TRANSPARENT)
			    se_copy(mp, NULL, sizeof (int), M_COPYIN);
			else {
				int operation;
				switch (ioc->ioc_cmd) {
				    case (TIOCMSET):
					operation = DMSET; break;
				    case (TIOCMBIS):
					operation = DMBIS; break;
				    case (TIOCMBIC):
					operation = DMBIC; break;
				}
				(void) se_mctl(sc, *(int *)mp->b_cont->b_rptr,
					operation); /* diddle dsl */
			}
			break;

		    case (TIOCMGET): {
			    mblk_t *tmp; /* Reply message block */

			    DRIVER_UNLOCK; /* Do this without major lock */
			    tmp = allocb(sizeof (int), BPRI_MED);
			    if (tmp == NULL) {
				    error = EAGAIN; /* send back as a nak */
				    break;
			    }

			    if (ioc->ioc_count == TRANSPARENT)
				se_copy(mp, tmp, sizeof (int), M_COPYOUT);
			    else
				se_ack(mp, tmp, 0);

			    DRIVER_LOCK; /* Need lock again. */

			    *(int *)mp->b_cont->b_rptr = se_mctl(sc, 0, DMGET);
			    break;
		    }
		    case SE_IOC_UCM_RESET: {
			se_chip_t *sec = sc->sc_chip;

			/*
			 * This ioctl is only valid for the control line of
			 * a UCM device.
			 */
			if (!sec->sec_is_ucm ||
			    sec->sec_ucm_control != sc) {
				error = EINVAL;
				break;
			}

			/*
			 * Drive the UCM control bit low and then high.
			 */
			sec->sec_pcr &= ~SAB_PVR_UCM;
			ddi_put8(sc->sc_handle, &sc->sc_reg->sab_pcr,
			    sec->sec_pcr);
			sec->sec_pvr &= ~SAB_PVR_UCM;
			ddi_put8(sc->sc_handle, &sc->sc_reg->sab_pvr,
			    sec->sec_pvr);
			delay(drv_usectohz(1000));
			sec->sec_pvr |= SAB_PVR_UCM;
			ddi_put8(sc->sc_handle, &sc->sc_reg->sab_pvr,
			    sec->sec_pvr);
			sec->sec_pcr |= SAB_PVR_UCM;
			ddi_put8(sc->sc_handle, &sc->sc_reg->sab_pcr,
			    sec->sec_pcr);

			DRIVER_UNLOCK;
			se_ack(mp, NULL, 0); /* Turn message into an ack */
			DRIVER_LOCK;
			break;
		    }
		    default:
			error = EINVAL;	/* Flag we don't know what to do */
		} /* End switch statement */
	} /* end if error < 0 */
	if (error) {
		ioc->ioc_error = error;
		mp->b_datap->db_type = M_IOCNAK; /* Turn message into a nak */
	}

	SE_PUTQ(sc, mp);	/* Put this message on read (return) queue */
	SE_SETSOFT(sc);
	DRIVER_UNLOCK;		/* Allow interrupts again */
}

/*
 * se_async_dslint and se_async_resume
 */

static void
se_async_dslint(se_ctl_t *sc)
{
	sc->z.za.za_ext = 1;	/* Flag change for softint */
	PUT_CMDR(sc, SAB_CMDR_RFRD); /* Request fifo read enable */
	SE_SETSOFT(sc);
}

/*
 * HDLC-specific routines start here.
 */

/*
 * se_hdlc_open - initialize a device for hdlc use.
 */

static int
se_hdlc_open(se_ctl_t *sc, queue_t *rq, int flag, cred_t *cr, dev_t *dev)
{
#ifdef lint
	flag = flag;
	cr = cr;
#endif
	ASSERT(mutex_owned(sc->h.sc_excl));

	if (rq->q_ptr)
	    return (EBUSY);	/* Don't allow two opens of same device */

	sc->sc_dev = *dev;		/* Store this for later use */
	sc->sc_bufsize = se_default_mru; /* Set default buffer size */
	sc->z.zh.sl_wd_id = 0;		 /* clear watchdog timer id */
	sc->z.zh.sl_wd_txcnt = 0;	 /* watchdog timer's tx block counter */
	sc->z.zh.sl_wd_count = 0; /* watchdog timer error message counter */
	SE_FILLSBY(sc);		/* Fill in receive buffers */
	DRIVER_LOCK;		/* keep everyone else out... */
	SE_TAKEBUFF(sc);	/* Initialize one such buffer for receive */
	sc->z.zh.zhf.sf_fdxptp = 1; /* Assume full duplex point-to-point */
	DRIVER_UNLOCK;
	rq->q_ptr = WR(rq)->q_ptr = (void *)sc;
	sc->z.zh.sl_readq = rq;	/* Save queue pointers */
	sc->z.zh.sl_writeq = WR(rq);
	qprocson(rq);		/* Allow streams calls down to us */
	return (0);		/* indicate success */
}

/*
 * se_hdlc_close - initialize a device for hdlc use.
 */

static int
se_hdlc_close(se_ctl_t *sc, queue_t *rq)
{
	mblk_t *xhead, *rhead, *bp1;

	ASSERT(mutex_owned(sc->h.sc_excl));

	sc->sc_closing = 1;

	qprocsoff(rq);		/* no new business after this */

	if (sc->z.zh.sl_wd_id) {
		(void) untimeout(sc->z.zh.sl_wd_id); /* Stop the watchdog */
		sc->z.zh.sl_wd_id = 0;
	}
	if (sc->sc_bufcid) {
		unbufcall(sc->sc_bufcid); /* Stop buffer callback if any */
		sc->sc_bufcid = 0;
	}

	/* Cancel kick_rcv callback if any */
	if (sc->sc_kick_rcv_id) {
		PORT_UNLOCK(sc);
		(void) untimeout(sc->sc_kick_rcv_id);
		PORT_LOCK(sc);
		sc->sc_kick_rcv_id = 0;
	}

	DRIVER_LOCK;

	REG_PUTW(sc, sab_imr, 0xffff); /* Disable ints */
	(void) se_mctl(sc, SE_OFF, DMSET); /* Drop dsl signals */
	PUT_CMDR(sc, SAB_CMDR_XRES | SAB_CMDR_RRES); /* Zap receive & xmit */
	sc->sc_xmit_active = 0;	/* Transmit no longer active */
	REG_PUTB(sc, sab_ccr0, SAB_CCR0_MCE);	/* zap chip */

/*
 * Clear away active buffers.
 */
	xhead = sc->sc_xmithead; /* Copy buffer pointers */
	rhead = sc->sc_rcvhead;
	sc->sc_xmithead = sc->sc_xmitblk = NULL;
	sc->sc_rcvhead = sc->sc_rcvblk = NULL;
	sc->sc_wr_cur = sc->sc_wr_lim = NULL;
	sc->sc_rd_cur = sc->sc_rd_lim = NULL;


	while (sc->sc_rstandby_ptr > 0) { /* Get standby buffers */
		sc->sc_rstandby_ptr--;
		bp1 = sc->sc_rstandby[sc->sc_rstandby_ptr];
		sc->sc_rstandby[sc->sc_rstandby_ptr] = NULL;
		bp1->b_next = rhead;
		rhead = bp1;
	}

	if (sc->sc_rdone_head) { /* Get any pending received buffers, */
		for (bp1 = sc->sc_rdone_head; bp1 != NULL; bp1 = bp1->b_next) {
			sc->sc_rdone_count--;	/* Decrement by buffer */
		}
		if (sc->sc_rdone_count) {
			cmn_err(CE_WARN, "se%d close: Bad rdone count %d",
				sc->h.sc_unit, sc->sc_rdone_count);
			sc->sc_rdone_count = 0;
		}
		sc->sc_rdone_tail->b_next = rhead;
		rhead = sc->sc_rdone_head;
		sc->sc_rdone_head = sc->sc_rdone_tail = NULL;
	}

	DRIVER_UNLOCK;

	if (xhead)		/* Free active transmit buffer, if any */
	    freemsg(xhead);

	/* Free all the receive messages that we removed from queues */
	while (rhead) {
		bp1 = rhead->b_next;
		rhead->b_next = NULL;
		freemsg(rhead);
		rhead = bp1;
	}

	while (xhead = getq(sc->z.zh.sl_writeq)) /* Clean out write queue */
	    freemsg(xhead);

	if (sc->z.zh.sl_mstat) {
		freemsg(sc->z.zh.sl_mstat);
		sc->z.zh.sl_mstat = NULL;
	}

	flushq(WR(rq), FLUSHALL);

	rq->q_ptr = WR(rq)->q_ptr = NULL; /* zap streams pointers to driver */
	sc->h.sc_protocol = NULL_PROTOCOL; /* Release use of port */
	sc->h.sc_ops = &se_null_ops;
	sc->sc_closing = 0;
	return (0);		/* Return indicating success */
}


/*
 * se_hdlc_wput - Handle a message sent down by streams
 */

static int
se_hdlc_wput(se_ctl_t *sc, queue_t *wq, mblk_t *mp)
{
	mblk_t *bp;

	switch (mp->b_datap->db_type) {
	    case (M_DATA):
		if (!sc->z.zh.zhf.sf_initialized) {
			cmn_err(CE_WARN,
			"se_hdlc%d: not initialized, can't send message",
				sc->h.sc_unit);
			return (EINVAL); /* Don't allow data until initialzed */
		}

		while (mp->b_wptr == mp->b_rptr) {
			register mblk_t *mp1;
			mp1 = unlinkb(mp); /* If null segment, unlink it */
			freemsg(mp);
			mp = mp1;
			if (!mp)
			    return (0);
		}

		(void) putq(wq, mp); /* Stash this message on our queue */

		if (!sc->sc_xmit_active)
		    se_hdlc_start(sc);	/* Start transmit */
		return (0);	/* We've got a data message queued, go away */

	    case (M_PROTO):
		return (EINVAL); /* Only legal for clone device */

	    case (M_IOCTL):
		se_hdlc_ioctl(sc, wq, mp); /* handle the ioctl */
		break;

	    case (M_FLUSH):
		if (*mp->b_rptr & FLUSHW) {	/* Flush output */

/*
 * Zap chip and clean up current transmit buffer
 */
			DRIVER_LOCK;
			bp = sc->sc_xmithead;
			sc->sc_xmithead = sc->sc_xmitblk = NULL;
			sc->sc_wr_lim = sc->sc_wr_cur = NULL;
			PUT_CMDR(sc, SAB_CMDR_XRES); /* reset transmitter */
			sc->sc_xmit_active = 0;	/* Not active any more. */
			DRIVER_UNLOCK;

			if (bp)
			    freemsg(bp); /* free message if any */

			flushq(wq, FLUSHDATA); /* Flush data in the pipe */
			*mp->b_rptr &= ~FLUSHW;	/* Clear flush write bit */
		}
		if (*mp->b_rptr & FLUSHR) { /* Flush input */
			DRIVER_LOCK;
			if (sc->sc_rcvhead != NULL)
			    SE_GIVE_RCV(sc); /* Close current buffer */
			SE_SETSOFT(sc);
			DRIVER_UNLOCK;

			flushq(RD(wq), FLUSHDATA); /* Flush data in the pipe */
			qreply(wq, mp);	/* Return to caller */
		} else {	/* Do not flush input */
			freemsg(mp); /* Return to freepool */
		}
		break;

	    default:
		freemsg(mp);

	} /* end switch of message type */
	return (0);
}

/*
 * se_hdlc_program - initialize the chip for hdlc usage
 */

static void
se_hdlc_program(se_ctl_t *sc)
{

	u_char ccr0, ccr1, ccr2, ccr3, ccr4, bgr, mode;
	u_short imr;
	int divisor;

	/* Make sure we belong here. */
	ASSERT(mutex_owned(&se_hi_excl));
	ASSERT(mutex_owned(sc->h.sc_excl));

	ccr0 = SAB_CCR0_MCE | SAB_CCR0_HDLC; /* hdlc mode */
	if (!(sc->z.zh.sl_mode.sm_config & CONN_NRZI))
	    ccr0 |= SAB_CCR0_NRZ;	/* Add NRZ mode if not NRZI */

	/* syncoutput driver push/pull, Idle transmits "flag" chars */
	ccr1 = SAB_CCR1_ODS | SAB_CCR1_ITF | SAB_CCR1_SFLG;
	ccr1 += (sc->z.zh.sl_clockmode >> 4) & 0x0f;

	if ((sc->z.zh.sl_clockmode & 0x0f) == 0x0b)
	    ccr2 = SAB_CCR2_SSEL; /* clock type ending in "b" wants SSEL */
	else
	    ccr2 = 0;		/* clock type ending in "a". */

	/* Depending on clock mode enable TxClk as input or output */
	switch (sc->z.zh.sl_clockmode) {
	    case (0x0b):
	    case (0x6b):
	    case (0x7b):
		ccr2 |= SAB_CCR2_TOE | SAB_CCR2_BDF;   /* TxClk output  */
		break;
	    default:
		break;
	}

	ccr3 = SAB_CCR3_RADD | SAB_CCR3_EPT | SAB_CCR3_PRE2;
	ccr4 = SAB_CCR4_MCK4;	/* Master clock divide by 4 */

	/* Calculate baudrate divisor */
	if (sc->z.zh.sl_mode.sm_baudrate)
	    divisor = (SE_CLK/2 / sc->z.zh.sl_mode.sm_baudrate) - 1;
	else
	    divisor = 0;

	if (divisor > 0x3ff) {	/* Handle extra-low-speed mode */
		int exp = 1;
		ccr4 |= SAB_CCR4_EBRG; /* Enable extended baud rate mode */
		while ((divisor > 0x3f) && (exp < 15)) {
			divisor /= 2;
			exp++;
		}
		divisor = (divisor & 0x3f) | (exp << 6);
	}

	/* If we use extended bits of divisor, put them in ccr2 */
	if ((sc->z.zh.sl_clockmode != 0x0a) && (divisor & 0x300)) {
		if (divisor & 0x100) ccr2 |= SAB_CCR2_BR8;
		if (divisor & 0x200) ccr2 |= SAB_CCR2_BR9;
	}

	bgr = divisor & 0xff;

	/* Interrupt bits we care about */
	imr =   SAB_ISR_RME | SAB_ISR_CDSC | SAB_ISR_RFO | SAB_ISR_RPF |
		SAB_ISR_ALLS | SAB_ISR_XDU | SAB_ISR_TIN | SAB_ISR_XMR |
		SAB_ISR_XPR;

	mode = SAB_MODE_TRAN | SAB_MODE_RAC | SAB_MODE_RTS;
	/* Transparent mode and rcvr active */

	if (sc->z.zh.sl_mode.sm_config & CONN_LPBK)
	    mode |= SAB_MODE_TLP;	/* If loopback, tell the chip */

	/*
	 * Set DTR and also set fast-serial mode
	 */

	sc->sc_chip->sec_pvr &= ~(sc->sc_dtrbit | SAB_PVR_FAST);
	sc->sc_chip->sec_pim |= sc->sc_dsrbit; /* Add our DSR bit */

	/* Start putting values to the chip */

	REG_PUTB(sc, sab_ccr0, ccr0);
	REG_PUTB(sc, sab_ccr4, ccr4); /* Do this one early */
	REG_PUTB(sc, sab_ccr1, ccr1);
	REG_PUTB(sc, sab_ccr2, ccr2);
	REG_PUTB(sc, sab_ccr3, ccr3);
	(void) REG_GETW(sc, sab_isr);	/* Wipe any pre-existing error status */
	REG_PUTB(sc, sab_bgr,  bgr);
	REG_PUTB(sc, sab_mode, mode);
	REG_PUTB(sc, sab_cmdr, SAB_CMDR_RHR | SAB_CMDR_XRES);
	sc->sc_xmit_active = 0;	/* Just in case */
	REG_PUTB(sc, sab_rlcr, (sc->sc_bufsize / 32) - 1);
	REG_PUTB(sc, sab_aml,  0);	/* Address masks. Ignore 'em */
	REG_PUTB(sc, sab_amh,  0);
	REG_PUTB(sc, sab_ipc,  SE_INTERRUPT_CONFIG);
	REG_PUTB(sc, sab_pre, 0x7e); /* Flag character */
	REG_PUTB(sc, sab_pcr, sc->sc_chip->sec_pcr);
	REG_PUTB(sc, sab_pim, sc->sc_chip->sec_pim);
	REG_PUTB(sc, sab_pvr, sc->sc_chip->sec_pvr);
	REG_PUTB(sc, sab_ccr0, ccr0 | SAB_CCR0_PU);
	PUT_CMDR(sc, SAB_CMDR_RHR | SAB_CMDR_XRES); /* re-clear fifos */

	sc->sc_imr = imr;
	imr = swapit(imr);
	REG_PUTW(sc, sab_imr, ~imr);

	sc->z.zh.zhf.sf_initialized = 1; /* Indicate we have inited port */
}

/*
 * se_hdlc_txint - Interrupt indicating room for more xmit data
 */

static void
se_hdlc_txint(se_ctl_t *sc)
{
	int data_count, fifo_room;
	u_char cmdr;

	sc->z.zh.sl_wd_txcnt++; /* watchdog timer's tx block counter */

	if (sc->sc_wr_cur == NULL) {
		return;		/* Nothing to send, ignore this interrupt */
	}

	cmdr = SAB_CMDR_XTF;	/* command to start transmission */
	fifo_room = SAB_FIFO_SIZE; /* Max data we can move in one gulp */

	while (sc->sc_wr_cur && fifo_room) { /* move data from buf to fifo */
		data_count = sc->sc_wr_lim - sc->sc_wr_cur;
		if (data_count > fifo_room) data_count = fifo_room;
		MULT_PUTB(sc, sc->sc_wr_cur, sc->sc_reg->sab_rfifo,
			data_count);
		sc->sc_wr_cur += data_count; /* Skip over data we just moved */
		sc->z.zh.sl_st.ochar += data_count; /* statistics */
		fifo_room -= data_count;

		if (sc->sc_wr_cur == sc->sc_wr_lim) { /* end of segment? */
			if (sc->sc_xmitblk->b_cont) { /* next block? */
				sc->sc_xmitblk = sc->sc_xmitblk->b_cont;
				sc->sc_wr_cur = sc->sc_xmitblk->b_rptr;
				sc->sc_wr_lim = sc->sc_xmitblk->b_wptr;
			} else { /* No more segments, no more data */
				cmdr |= SAB_CMDR_XME; /* Indicate eom */
				sc->sc_wr_cur = sc->sc_wr_lim = NULL;
				sc->z.zh.sl_st.opack++;
			}
		}
	}

	PUT_CMDR(sc, cmdr);	/* Tell chip to move data */
	sc->sc_xmit_active = 1;	/* We are active again */
}


/*
 * se_hdlc_txint_bad - A frame was aborted during transmit.
 */

static void
se_hdlc_txint_bad(se_ctl_t *sc)
{
	PUT_CMDR(sc, SAB_CMDR_XRES); /* Reset transmitter */
	sc->sc_xmit_active = 0;	/* Not active any more */
	sc->sc_xmitblk = sc->sc_xmithead; /* Point back to start of message */
	if (sc->sc_xmitblk) {
		sc->sc_wr_cur = sc->sc_xmitblk->b_rptr;
		sc->sc_wr_lim = sc->sc_xmitblk->b_wptr;
		se_hdlc_txint(sc); /* Re-start transmit of same message */
	}
}




/*
 * se_hdlc_watchdog(sc): hdlc transmitter watchdog timeout routine, wakes up
 * SE_HDLC_WD_TIMER_TICK seconds after a packet should have been transmitted
 * via hdlc. If the packet has not been transmitted after
 * SE_HDLC_WD_FIRST_WARNING_CNT ticks it outputs a message to the console and
 * repeats this message every SE_HDLC_WD_REPEAT_WARNING_CNT ticks until the
 * problem is fixed.
 */
static void
se_hdlc_watchdog(void *arg)
{
	se_ctl_t *sc = arg;

	if (sc->z.zh.sl_wd_txcnt == sc->z.zh.sl_wd_txcnt_start) {

		if ((sc->z.zh.sl_wd_count == SE_HDLC_WD_FIRST_WARNING_CNT) ||
		    (sc->z.zh.sl_wd_count > SE_HDLC_WD_REPEAT_WARNING_CNT)) {

			cmn_err(CE_WARN, "se_hdlc%x: transmit hung",
			sc->h.sc_unit);
			sc->z.zh.sl_wd_count = SE_HDLC_WD_FIRST_WARNING_CNT;
		}
		sc->z.zh.sl_wd_count++;
	} else  {
		sc->z.zh.sl_wd_count = 0;
	}

	PORT_LOCK(sc);
	if (sc->sc_xmit_active)  { /* transmitter active : start new timeout */
		sc->z.zh.sl_wd_txcnt_start = sc->z.zh.sl_wd_txcnt;
		sc->z.zh.sl_wd_id = timeout(se_hdlc_watchdog, sc,
		    SE_HDLC_WD_TIMER_TICK);
	} else {
		sc->z.zh.sl_wd_id = 0; /* indicate that timeout is inactive */
		}
	PORT_UNLOCK(sc);
}





/*
 * se_hdlc_start - Start transmission of a message
 */

static void
se_hdlc_start(se_ctl_t *sc)
{
	mblk_t *mp;

	ASSERT(mutex_owned(sc->h.sc_excl));

	if ((sc->sc_xmit_active) || (sc->sc_xmitblk))
	    return;		/* If xmit busy, just return */

	/* Get the next message to play with */
	mp = getq(sc->z.zh.sl_writeq);
	if (mp == NULL)
	    return;

	/* Make sure it is a data message */
	if (mp->b_datap->db_type != M_DATA) {
		cmn_err(CE_WARN,
		    "se%d hdlc_start: Invalid message type %d on write queue",
			sc->h.sc_unit, mp->b_datap->db_type);
		freemsg(mp);		/* Avoid memory leak */
		return;
	}


	if (mp->b_rptr == mp->b_wptr) {
		cmn_err(CE_WARN, "se%d hdlc_start: Empty message!",
			sc->h.sc_unit);
		return;
	}

/*
 * We can only get here if transmit is idle. We can assume that any
 * pointers are empty.
 */

	DRIVER_LOCK;
	if ((sc->sc_xmit_active) || (sc->sc_xmitblk)) {
		DRIVER_UNLOCK;
		cmn_err(CE_WARN,
		    "se%d: Transmit became active, discarding hdlc message",
			sc->h.sc_unit);
		return;
	}
	sc->sc_xmithead = sc->sc_xmitblk = mp; /* Point to new message */
	sc->sc_wr_cur = mp->b_rptr; /* Set up message */
	sc->sc_wr_lim = mp->b_wptr;


	/*
	 * Kick off watchdog timer that monitors whether hdlc messages are
	 * really being sent over the wire
	 */
	if (sc->z.zh.sl_wd_id == 0) {
		sc->z.zh.sl_wd_txcnt_start = sc->z.zh.sl_wd_txcnt;
		sc->z.zh.sl_wd_id = timeout(se_hdlc_watchdog, sc,
		    SE_HDLC_WD_TIMER_TICK);
	}
	se_hdlc_txint(sc);	/* Pretend we got a transmit interrupt */
	DRIVER_UNLOCK;
}

/*
 * se_hdlc_rxint_bad - Something went wrong with current message.
 */


static void
se_hdlc_rxint_bad(se_ctl_t *sc)
{
	sc->z.zh.sl_st.ierror++; /* Increment number of input errors */
	PUT_CMDR(sc, SAB_CMDR_RHR); /* Reset HDLC receiver, drop message */
	if (sc->sc_rcvhead) {	/* If we have a buffer, handle it */
		sc->sc_rcvhead->b_datap->db_type = M_RSE; /* ignore msg. */
		SE_GIVE_RCV(sc); /* Put the message on the received queue */
	}
	SE_TAKEBUFF(sc);	/* Allocate new receive buffer. */
	SE_SETSOFT(sc);
}

/*
 * se_hdlc_rpfint - Interrupt indicating 32 bytes of data available
 */

static void
se_hdlc_rpfint(se_ctl_t *sc)
{

/*
 * Before we do anything, make sure we have room for this data.
 */
	if ((sc->sc_rd_lim - sc->sc_rd_cur) < SAB_FIFO_SIZE) {
		if (sc->sc_rstandby_ptr > 0) { /* Try to allocate a buffer */
			sc->sc_rstandby_ptr--;
			sc->sc_rcvblk = sc->sc_rstandby[sc->sc_rstandby_ptr];
			sc->sc_rstandby[sc->sc_rstandby_ptr] = NULL;
			sc->sc_rd_cur = sc->sc_rcvblk->b_wptr;
			sc->sc_rd_lim = sc->sc_rcvblk->b_datap->db_lim;
		} else {
			sc->z.zh.sl_st.nobuffers++;	/* statistics */
			if (se_debug)
			    cmn_err(CE_WARN, "se%d rpf nobuffers",
				    sc->h.sc_unit);
			se_hdlc_rxint_bad(sc); /* Discard data */
			return;
		}
	}

	MULT_GETL(sc, (ULL)sc->sc_rd_cur,
		(ULL)&sc->sc_reg->sab_rfifo,
		SAB_FIFO_SIZE/DDI_LL_SIZE);
	sc->sc_rd_cur += SAB_FIFO_SIZE;
	sc->z.zh.sl_st.ichar += SAB_FIFO_SIZE; /* statistics */

	PUT_CMDR(sc, SAB_CMDR_RMC); /* Allow chip to re-use fifo. */
}

/*
 * se_hdlc_rmeint - We have received an end-of-message.
 */

static void
se_hdlc_rmeint(se_ctl_t *sc)
{
	int data_count, rbcl;
	u_char status;

	rbcl = REG_GETB(sc, sab_rbcl);
	data_count = rbcl & (SAB_FIFO_SIZE - 1); /* Data modulo 32 */
	if (data_count == 0) data_count = SAB_FIFO_SIZE; /* 32 bytes */

	if (sc->sc_rcvhead == NULL) {	/* No buffer, bad message */
		sc->z.zh.sl_st.nobuffers++;	/* statistics */
		if (se_debug)
		    cmn_err(CE_WARN, "se%d rme nobuffers", sc->h.sc_unit);
		se_hdlc_rxint_bad(sc);
		return;
	}

	/* deal with buffering */

	if (sc->sc_rd_cur == 0) {
		if (sc->sc_rstandby_ptr > 0) { /* Try to allocate a buffer */
			sc->sc_rstandby_ptr--;
			sc->sc_rcvblk = sc->sc_rstandby[sc->sc_rstandby_ptr];
			sc->sc_rstandby[sc->sc_rstandby_ptr] = NULL;
			sc->sc_rd_cur = sc->sc_rcvblk->b_wptr;
			sc->sc_rd_lim = sc->sc_rcvblk->b_datap->db_lim;
		} else {
			sc->z.zh.sl_st.nobuffers++;	/* statistics */
			if (se_debug)
			    cmn_err(CE_WARN, "se%d rme2 nobuffers",
				    sc->h.sc_unit);
			se_hdlc_rxint_bad(sc);	/* Discard data */
			return;
		}
	}

	/*
	 * If data available, read it. Note: We may receive a message-end
	 * interrupt and already have read all the data. This is valid.
	 */
	if (data_count) {
		MULT_GETB(sc, sc->sc_rd_cur,
			&sc->sc_reg->sab_rfifo[0], data_count);
		sc->sc_rd_cur += data_count;
		sc->z.zh.sl_st.ichar += data_count; /* statistics */
	}

	/*
	 * Sanity check to make sure message size matches data read.
	 */

	if ((sc->sc_rd_cur - sc->sc_rcvblk->b_rptr & 0xff) != rbcl)
	    cmn_err(CE_WARN, "se%d rmeint bad sizes %d %ld", sc->h.sc_unit,
		rbcl, (ptrdiff_t)(sc->sc_rd_cur - sc->sc_rcvblk->b_rptr));

	/*
	 * Handle status byte and random bookkeeping.
	 */

	if (sc->sc_rd_cur != sc->sc_rcvblk->b_wptr) {
		/* If the buffer is not empty */
		sc->sc_rd_cur--;		/* Back up before last byte */
		status = *sc->sc_rd_cur;	/* Pick up the status byte */
	} else {
		status = 0;	/* Will cause below check to fail */
		cmn_err(CE_WARN, "se%d hdlc_rmeint: No status byte",
			sc->h.sc_unit);
	}

	/* Verify that HDLC engine thought message was good */
	if ((status &
	    (SAB_RSTA_VFR | SAB_RSTA_RDO | SAB_RSTA_CRC | SAB_RSTA_RAB))
	    != (SAB_RSTA_VFR | SAB_RSTA_CRC)) {
		if (!(status & SAB_RSTA_VFR) &&
		    (sc->sc_rd_cur == sc->sc_rcvblk->b_wptr)) {
			se_invalid_frame++;
			if (se_debug)
			    cmn_err(CE_WARN, "se%d invalid frame",
				    sc->h.sc_unit);
			PUT_CMDR(sc, SAB_CMDR_RMC);
			return;	/* Pretend this didn't happen */
		}
		if (se_debug)
		    cmn_err(CE_WARN, "se%d hdlc bad status %x",
			sc->h.sc_unit, status);
		if (status & SAB_RSTA_CRC)
		    sc->z.zh.sl_st.crc++; /* statistics */
		else if (status & SAB_RSTA_RAB)
		    sc->z.zh.sl_st.abort++; /* statistics */

		sc->sc_rcvhead->b_datap->db_type = M_RSE; /* Bad message */
	}
	sc->z.zh.sl_st.ipack++;	/* Increment number of receive packets */
	SE_GIVE_RCV(sc);	/* Close the buffer */
	SE_TAKEBUFF(sc);	/* Set up another buffer */
	SE_SETSOFT(sc);

	PUT_CMDR(sc, SAB_CMDR_RMC); /* We have retrieved the message */
}


/*
 * se_hdlc_setmstat - Send mstat message reporting DSL changes
 */

static void
se_hdlc_setmstat(se_ctl_t *sc, int event)
{
	struct sl_status *sls;
	mblk_t *mp;

	mp = sc->z.zh.sl_mstat;
	if (!(sc->z.zh.sl_mode.sm_config & (CONN_IBM | CONN_SIGNAL)) ||
	    mp == NULL)
	    return;		/* If we aren't interested, just return */

	sc->z.zh.sl_mstat = NULL; /* Prevent anyone else from using message */
	sls = (void*) mp->b_wptr; /* structured pointer to data region */
	if (sc->z.zh.sl_mode.sm_config & CONN_IBM)
	    sls->type = SLS_LINKERR; /* Different type of message for IBM */
	else
	    sls->type = SLS_MDMSTAT;

	sls->status = event;	/* tell which event actually occurred */

	/* get timestamp */
	(void) drv_getparm(TIME, &sls->tstamp.tv_sec);
	mp->b_datap->db_type = M_PROTO;
	mp->b_wptr += sizeof (*sls); /* skip datastructure we filled in */

	SE_PUTQ(sc, mp);	/* Send this status message on it's way */
	SE_SETSOFT(sc);		/* And wake up the upper layers */
}

/*
 * se_hdlc_portint - Interrupt for a specific port in hdlc mode.
 */

static void
se_hdlc_portint(se_ctl_t *sc, u_short isr)
{

/*
 * Note: Do not change the order of rpf and rme interrupt handling
 */

	if (isr & SAB_ISR_RFO) {
		sc->z.zh.sl_st.overrun++;
		if (se_debug)
		    cmn_err(CE_WARN, "se%d Frame Ovr", sc->h.sc_unit);
	} else if (isr & SAB_ISR_RDO) {
		sc->z.zh.sl_st.overrun++;
		if (se_debug)
		    cmn_err(CE_WARN, "se%d Overrun", sc->h.sc_unit);
	};

	if (isr & SAB_ISR_RPF)	/* Receive pipe full interrupt */
	    se_hdlc_rpfint(sc);

	if (isr & SAB_ISR_RME)	/* Receive message end interrupt */
	    se_hdlc_rmeint(sc);

	if (isr & SAB_ISR_CDSC) {
		if (REG_GETB(sc, sab_vstr) & SAB_VSTR_CD)
		    se_hdlc_setmstat(sc, CS_DCD_UP); /* Report carrier up */
		else {
			se_hdlc_setmstat(sc, CS_DCD_DOWN);
			sc->z.zh.sl_st.dcd++;	/* Increment DCD changes */
		}
	}

	if (isr & SAB_ISR_CSC) {
		if (REG_GETB(sc, sab_star) & SAB_STAR_CTS)
		    se_hdlc_setmstat(sc, CS_CTS_UP);
		else {
			se_hdlc_setmstat(sc, CS_CTS_DOWN);
			sc->z.zh.sl_st.cts++;
		}
	}

	if (isr & SAB_ISR_ALLS) {
		sc->sc_xmit_active = 0;	/* transmit no longer in progress */
		sc->sc_xmit_done = 1;	/* Indicate we need buffer recycling */
		SE_SETSOFT(sc);		/* Request next buffer be setup */
	}

	if (isr & SAB_ISR_XDU) {
		se_hdlc_txint_bad(sc);
		sc->z.zh.sl_st.underrun++; /* message lost by underrun */
	} else if (isr & SAB_ISR_XMR) {
		se_hdlc_txint_bad(sc);	/* Message lost due to carrier drop */
	} else if (isr & SAB_ISR_XPR) {
		se_hdlc_txint(sc); /* Room for more data - feed the chip */
	}
}
/*
 * se_hdlc_softint - Background interrupt for handling buffer tasks
 */

static void
se_hdlc_softint(se_ctl_t *sc)
{
	queue_t *rq;
	mblk_t *bp;

	sc->sc_soft_active = 1;	/* allow us to unlock w/o losing control */

/*
 * If we've used up the modem status buffer, allocate a new one.
 * If it fails and returns null, no harm - we'll get it next time.
 */

	if (sc->z.zh.sl_mstat == NULL)
	    sc->z.zh.sl_mstat = allocb(sizeof (struct sl_status), BPRI_MED);

	rq = sc->z.zh.sl_readq;	/* Pointer to our read queue */

/*
 * Handle receive buffers. Either pass up or discard.
 */

	while (sc->sc_rdone_head && canputnext(rq)) {
		DRIVER_LOCK;	/* Can't play with this list without locking */
		bp = sc->sc_rdone_head;
		sc->sc_rdone_head = bp->b_next; /* step past this buffer */
		bp->b_next = NULL;
		sc->sc_rdone_count--;	/* Decrement count of buffers */
		if (sc->sc_rdone_head == NULL) {
			sc->sc_rdone_tail = NULL; /* empty list */
			if (sc->sc_rdone_count)
			    cmn_err(CE_WARN,
				    "se%d hdlc_softint: Bad rdone count %d",
				    sc->h.sc_unit, sc->sc_rdone_count);
		}
		DRIVER_UNLOCK;
		PORT_UNLOCK(sc); /* don't keep our lock over this call */
		if (bp->b_datap->db_type == M_RSE) {
			freemsg(bp); /* Error message, just drop it. */
			if (se_debug)
			    cmn_err(CE_WARN, "se%d hdlc RSE\n", sc->h.sc_unit);
		} else if ((bp->b_datap->db_type == M_DATA) ||
			(bp->b_datap->db_type == M_PROTO)) {
			/* Give the message to upper layer */
			putnext(rq, bp);
		} else {
			cmn_err(CE_WARN,
				"se%d hdlc_softint: Invalid message type %d",
				sc->h.sc_unit, bp->b_datap->db_type);
			/* Give the message to upper layer */
			putnext(rq, bp);
		}
		PORT_LOCK(sc);
	}

/*
 * On transmit done, clean up current transnit message.
 */

	if (sc->sc_xmit_done) {
		DRIVER_LOCK;
		bp = sc->sc_xmithead;
		sc->sc_xmitblk = sc->sc_xmithead = NULL;
		sc->sc_wr_cur = sc->sc_wr_lim = NULL;
		sc->sc_xmit_done = 0;
		DRIVER_UNLOCK;
		if (bp) {
			freemsg(bp);
		}
	}

	if (!sc->sc_xmit_active)
	    se_hdlc_start(sc);

	SE_FILLSBY(sc);		/* re-fill the standby buffers */
	sc->sc_soft_active = 0;	/* Allow other routines to run again */
}

/*
 * se_hdlc_ioctl - Handle configuration requests from application.
 */

static void
se_hdlc_ioctl(se_ctl_t *sc, queue_t *wq, mblk_t *mp)
{
	struct iocblk   *iocp = (struct iocblk *)mp->b_rptr;
	struct scc_mode *sm;

	int error = 0;

	switch (iocp->ioc_cmd) {

	    case (S_IOCGETMODE):	/* Fetch current mode information */
		bcopy((caddr_t)&sc->z.zh.sl_mode,
			(caddr_t)mp->b_cont->b_rptr,
			sizeof (sc->z.zh.sl_mode));
		break;

	    case (S_IOCGETSTATS):	/* Fetch current statistics */
		bcopy((caddr_t)&sc->z.zh.sl_st, (caddr_t)mp->b_cont->b_rptr,
			sizeof (sc->z.zh.sl_st));
		break;

	    case (S_IOCGETSPEED):	/* Current baudrate, if set. */
		*(int *)mp->b_cont->b_rptr = sc->z.zh.sl_mode.sm_baudrate;
		break;

	    case (S_IOCGETMCTL):	/* Get current modem signals */
		DRIVER_LOCK;
		*(u_char *)mp->b_cont->b_rptr = se_mctl(sc, 0, DMGET);
		DRIVER_UNLOCK;
		break;

	    case (S_IOCGETMRU):		/* Get current max message size */
		*(int *)mp->b_cont->b_rptr = sc->sc_bufsize;
		break;

	    case (S_IOCCLRSTATS):	/* zero statistics */
		bzero((caddr_t)&sc->z.zh.sl_st, sizeof (sc->z.zh.sl_st));
		break;

	    case (S_IOCSETDTR):		/* play with dtr */
		DRIVER_LOCK;
		if (mp->b_cont) {
		    if (*(int *)mp->b_cont->b_rptr) {
			(void) se_mctl(sc, TIOCM_DTR, DMBIS);
		    } else {
			(void) se_mctl(sc, TIOCM_DTR, DMBIC);
		    }
		}
		DRIVER_UNLOCK;
		break;

	    case (S_IOCSETMRU):	{	/* Set max message size */
		mblk_t *rhead, *bp1;

		DRIVER_LOCK;
		sc->sc_bufsize = *(int *)mp->b_cont->b_rptr;

		/*
		 * Flush out the current buffers and allocate new
		 * buffers with the new message size
		 */
		rhead = NULL;
		if (sc->sc_rcvblk) {
			rhead = sc->sc_rcvblk;
			sc->sc_rd_cur = sc->sc_rd_lim = NULL;
			sc->sc_rcvblk = sc->sc_rcvhead = NULL;
		}

		/*
		 * get a list of standby buffers
		 */
		while (sc->sc_rstandby_ptr > 0) {
			sc->sc_rstandby_ptr--;
			bp1 = sc->sc_rstandby[sc->sc_rstandby_ptr];
			sc->sc_rstandby[sc->sc_rstandby_ptr] = NULL;
			bp1->b_next = rhead;
			rhead = bp1;
		}

		se_hdlc_program(sc);	/* Reset the chip */
		DRIVER_UNLOCK;

		/*
		 * Free all the messages from standby queue
		 */
		while (rhead) {
			bp1 = rhead->b_next;
			rhead->b_next = NULL;
			freemsg(rhead);
			rhead = bp1;
		}

		/*
		 * Reallocate buffers with new size
		 */
		SE_FILLSBY(sc);
		SE_TAKEBUFF(sc);
		}
		break;

	    case (S_IOCSETMODE):

		sm = (void *)mp->b_cont->b_rptr; /* message as scc_mode */

/*
 * Set clock mode. There are seven legal combinations of receive and
 * transmit clock. All others return SMERR_TXC.
 */

		if ((sm->sm_txclock == TXC_IS_TXC) &&
		    (sm->sm_rxclock == RXC_IS_RXC))
		    sc->z.zh.sl_clockmode = 0x0a; /* standard synchronous. */
		else if ((sm->sm_txclock == TXC_IS_RXC) &&
			(sm->sm_rxclock == RXC_IS_RXC))
		    sc->z.zh.sl_clockmode = 0x3b; /* only one clock avail. */
		else if ((sm->sm_txclock == TXC_IS_BAUD) &&
			(sm->sm_rxclock == RXC_IS_RXC))
		    sc->z.zh.sl_clockmode = 0x0b; /* tx clock is output */
		else if ((sm->sm_txclock == TXC_IS_BAUD) &&
			(sm->sm_rxclock == RXC_IS_BAUD))
		    sc->z.zh.sl_clockmode = 0x7b; /* Async-like operation. */
		else if ((sm->sm_txclock == TXC_IS_TXC) &&
			(sm->sm_rxclock == RXC_IS_PLL))
		    sc->z.zh.sl_clockmode = 0x6a; /* Not supported. RxPLL. */
		else if ((sm->sm_txclock == TXC_IS_BAUD) &&
			(sm->sm_rxclock == RXC_IS_PLL))
		    sc->z.zh.sl_clockmode = 0x6b; /* Not supported. RxPLL. */
		else if ((sm->sm_txclock == TXC_IS_PLL) &&
			(sm->sm_rxclock == RXC_IS_PLL))
		    sc->z.zh.sl_clockmode = 0x7a; /* Not supported TRxPLL. */
		else {
			sc->z.zh.sl_mode.sm_retval = SMERR_TXC;
			error = EINVAL;		/* Tell ioctl that we failed */
			break;
		}

		/* IBM mode isn't allowed to use either HDX or Multipoint */

		if (sm->sm_config & CONN_IBM) {
			if (sm->sm_config & CONN_HDX) {
				sc->z.zh.sl_mode.sm_retval = SMERR_HDX;
				error = EINVAL;
				break;
			}
			if (sm->sm_config & CONN_MPT) {
				sc->z.zh.sl_mode.sm_retval = SMERR_MPT;
				error = EINVAL;
				break;
			}
		}

		/* We don't have an external loopback. Disallow it. */
		if (sm->sm_config & CONN_ECHO) {
			sc->z.zh.sl_mode.sm_retval = SMERR_LPBKS;
			error = EINVAL;
			break;
		}

		/* In loopback mode, force clockmode to pure baudrate */
		if (sc->z.zh.sl_mode.sm_config & CONN_LPBK) {
			sc->z.zh.sl_clockmode = 0x7b;
		}

		/* Determine if we are in full duplex point-to-point mode */
		if (sm->sm_config & (CONN_HDX | CONN_MPT))
		    sc->z.zh.zhf.sf_fdxptp = 0;
		else
		    sc->z.zh.zhf.sf_fdxptp = 1;

		/* Copy the configuration block for later use */
		bcopy((char *)sm, (char *)&sc->z.zh.sl_mode, sizeof (*sm));

		/* Tell the chip about new settings */
		DRIVER_LOCK;
		se_hdlc_program(sc);
		DRIVER_UNLOCK;
		break;

	    default:
		error = EINVAL;

	} /* End of switch on ioctl type */

	iocp->ioc_error = error;
	mp->b_datap->db_type = (error) ? M_IOCNAK : M_IOCACK;

	qreply(wq, mp);		/* Send the reply back up. */
}

/*
 * Clone device support starts here
 */

/*
 * se_clone_open and se_clone_close - initialize and free clones
 */

static int
se_clone_open(queue_t *rq, dev_t *dev)
{

	se_clone_t *scl;

	DRIVER_LOCK;
	for (scl = se_clones; scl != NULL; scl = scl->scl_next) {
		if (scl->scl_in_use == 0) break; /* Found a free unit */
	}

	if (scl == NULL) {
		if (se_clone_number < 100) /* Limit us to 100 active clones */
		    scl = kmem_zalloc(sizeof (se_clone_t), KM_SLEEP);
		if (scl == NULL) {
			DRIVER_UNLOCK;
			cmn_err(CE_WARN,
				"se_hdlc clone open failed, no memory, rq=%lx",
				(unsigned long)rq);
			return (ENODEV); /* fail */
		}
		scl->h.sc_excl =
		    (kmutex_t *)kmem_zalloc(sizeof (kmutex_t), KM_SLEEP);
		mutex_init(scl->h.sc_excl, NULL, MUTEX_DRIVER, se_iblock);

		scl->h.sc_unit = CLONE_DEVICE + se_clone_number++;
		scl->h.sc_protocol = CLON_PROTOCOL;
		scl->h.sc_ops = &se_clone_ops;
		scl->scl_rq = rq; /* Save pointer to read queue */

		scl->scl_next = se_clones; /* Insert into linked list */
		se_clones = scl;
	} else {
		scl->scl_rq = rq; /* Save pointer to read queue */
	}

	rq->q_ptr = WR(rq)->q_ptr = (void *)scl;

	scl->scl_in_use = 1;	/* Flag that unit is in use */
	DRIVER_UNLOCK;

	*dev = makedevice(getmajor(*dev), scl->h.sc_unit); /* Create clone */

	qprocson(rq);		/* Allow streams to call us */
	return (0);
}

static int
se_clone_close(se_ctl_t *sc, queue_t *rq)
{
	/* Note - SC does _not_ point to a se_ctl_t */
	se_clone_t *scl = (void *)sc;

	qprocsoff(rq);		/* no new business after this */
	flushq(WR(rq), FLUSHALL);

	scl->scl_in_use = 0;
	scl->scl_rq = NULL;
	rq->q_ptr = WR(rq)->q_ptr = NULL;

	return (0);
}

/*
 * se_clone_wput - handle requests on the clone channel
 */

static int
se_clone_wput(se_ctl_t *sc, queue_t *wq, mblk_t *mp)
{
	/* Note - SC does _not_ point to a se_ctl_t */
	se_clone_t *scl = (void *)sc;
	se_ctl_t *scx;

	int error = 0;
	int prim, ppa;
	union DL_primitives *dlp;

	switch (mp->b_datap->db_type) {
	    case (M_FLUSH):
		if (*mp->b_rptr & FLUSHW) {
			flushq(wq, FLUSHDATA); /* Flush data in the pipe */
			*mp->b_rptr &= ~FLUSHW; /* Clear flush write bit */
		}

		if (*mp->b_rptr & FLUSHR) {
			qreply(wq, mp); /* Send the reply back up. */
		} else {
			freemsg(mp);
		}
		break;

	    case (M_PROTO):	/* Assign specific port to this clone */

		if (MBLKL(mp) >= DL_ATTACH_REQ_SIZE) {

			dlp = (union DL_primitives *)mp->b_rptr;
			prim = dlp->dl_primitive;
			if (prim == DL_ATTACH_REQ) { /* valid dl_attach */
				ppa = dlp->attach_req.dl_ppa;

				for (scx = se_ctl_list;
				    scx != NULL;
				    scx = scx->sc_next) {
					if (scx->h.sc_unit == ppa) break;
				}

				if (scx == NULL)
				    error = DL_BADPPA; /* Didn't find */
				else
				    scl->scl_sc = scx; /* Found unit */

			} else {
				error = DL_BADPRIM;	/* not dl_attach */
			}
		} else {
			prim = DL_ATTACH_REQ;
			error = DL_BADPRIM;
		}

		if (error)
		    dlerrorack(wq, mp, prim, error, 0); /* Complain */
		else
		    dlokack(wq, mp, DL_ATTACH_REQ);	/* We're happy */
		break;

	    case (M_IOCTL):	/* Perform an ioctl for assigned port */
		scx = scl->scl_sc; /* Get pointer to attached port */
		if ((scx != NULL) &&
		    ((scx->h.sc_protocol == HDLC_PROTOCOL) ||
		    (scx->h.sc_protocol == NULL_PROTOCOL))) {
			/* Make sure we're attached */
			PORT_UNLOCK(scl);	/* We don't need this mutex */
			PORT_LOCK(scx); /* We do need this mutex */
			se_hdlc_ioctl(scx, wq, mp); /* Issue the ioctl */
			PORT_UNLOCK(scx);
			PORT_LOCK(scl); /* Back to our mutex */
		} else {
			freemsg(mp);
			cmn_err(CE_WARN,
			"se_hdlc: clone device must be attached before use!");
			error = EPROTO;
		}

		if (error) {
			PORT_UNLOCK(sc);
			/* Complain */
			(void) putnextctl1(RD(wq), M_ERROR, error);
			PORT_LOCK(sc);
		}
		break;

	    default:
		PORT_UNLOCK(sc);
		(void) putnextctl1(RD(wq), M_ERROR, EPROTO); /* Complain */
		PORT_LOCK(sc);
	}
	return (0);
}

/*
 * se_hdlc_suspend, se_hdlc_resume, se_hdlc_dsrint
 */

static void
se_hdlc_suspend(se_ctl_t *sc)
{
	mblk_t *mp;

	PORT_LOCK(sc);
	DRIVER_LOCK;		/* Keep everyone else locked out */
	PUT_CMDR(sc, SAB_CMDR_RRES | SAB_CMDR_XRES);
	REG_PUTB(sc, sab_ccr0, 0); /* Power-on bit gets cleared */
	sc->sc_suspend = 1;	/* Flag that this device is hereby suspended */
	sc->sc_xmit_active = 0;	/* Not active any more */

	SE_GIVE_RCV(sc);	/* Close out any input buffer */
	mp = sc->sc_xmitblk;	/* Pick up any transmit buffer */
	sc->sc_xmitblk = sc->sc_xmithead = NULL;
	sc->sc_wr_cur = sc->sc_wr_lim = NULL;

	DRIVER_UNLOCK;
	PORT_UNLOCK(sc);
	if (mp)
	    freemsg(mp);	/* Free transmit message, if any. */
}

static void
se_hdlc_resume(se_ctl_t *sc)
{
	PORT_LOCK(sc);
	DRIVER_LOCK;

	se_hdlc_program(sc);		/* Initialize the chip */

	DRIVER_UNLOCK;

	SE_SETSOFT(sc);		/* request background service */
	SE_FILLSBY(sc);		/* reallocate input buffers */
	se_hdlc_start(sc);	/* re-start transmit if possible */
	PORT_UNLOCK(sc);
}

static void
se_hdlc_dsrint(se_ctl_t *sc)
{
#ifdef lint
	sc = sc;
#endif

				/* As it happens, we don't care. */
}

/*
 * Null routines. Should never be called.
 */

static int
se_null(se_ctl_t *sc)
{
	if (se_debug)
	    cmn_err(CE_WARN, "se%d se_null called", sc->h.sc_unit);
	return (DDI_FAILURE);
}

static void
se_null_int(se_ctl_t *sc)
{
	if (se_debug)
	    cmn_err(CE_WARN, "se%d se_null_int called", sc->h.sc_unit);
}

static void
se_null_portint(se_ctl_t *sc, u_short isr)
{
	if (se_debug)
	    cmn_err(CE_WARN, "se%d se_null_portint called, isr %x",
		sc->h.sc_unit, isr);
}

int
se_create_ordinary_minor_nodes(dev_info_t *dip, se_ctl_t *sca, se_ctl_t *scb)
{
	char name[32];
	char *format = "%s%d: Failed to create node %s";

	/*
	 * Create 'a' and 'b' devices.
	 */
	(void) sprintf(name, "%c", sca->h.sc_unit + 'a');
	if (ddi_create_minor_node(dip, name, S_IFCHR,
	    sca->h.sc_unit, DDI_NT_SERIAL_MB, NULL) == DDI_FAILURE) {
		cmn_err(CE_WARN, format,
			ddi_get_name(dip), ddi_get_instance(dip), name);
		return (DDI_FAILURE);
	}
	(void) sprintf(name, "%c", scb->h.sc_unit + 'a');
	if (ddi_create_minor_node(dip, name, S_IFCHR,
	    scb->h.sc_unit, DDI_NT_SERIAL_MB, NULL) == DDI_FAILURE) {
		cmn_err(CE_WARN, format,
			ddi_get_name(dip), ddi_get_instance(dip), name);
		ddi_remove_minor_node(dip, NULL);
		return (DDI_FAILURE);
	}

	/*
	 * Create "hdlc" nodes.
	 */
	(void) sprintf(name, "%c,hdlc", sca->h.sc_unit + '0');
	if (ddi_create_minor_node(dip, name, S_IFCHR,
	    sca->h.sc_unit | HDLC_DEVICE, DDI_PSEUDO, NULL) == DDI_FAILURE) {
		cmn_err(CE_WARN, format,
			ddi_get_name(dip), ddi_get_instance(dip), name);
		ddi_remove_minor_node(dip, NULL);
		return (DDI_FAILURE);
	}
	(void) sprintf(name, "%c,hdlc", scb->h.sc_unit + '0');
	if (ddi_create_minor_node(dip, name, S_IFCHR,
	    scb->h.sc_unit | HDLC_DEVICE, DDI_PSEUDO, NULL) == DDI_FAILURE) {
		cmn_err(CE_WARN, format,
			ddi_get_name(dip), ddi_get_instance(dip), name);
		ddi_remove_minor_node(dip, NULL);
		return (DDI_FAILURE);
	}

	/*
	 * Create the "cu" nodes.
	 */
	(void) sprintf(name, "%c,cu", sca->h.sc_unit + 'a');
	if (ddi_create_minor_node(dip, name, S_IFCHR, sca->h.sc_unit | OUTLINE,
	    DDI_NT_SERIAL_MB_DO, NULL) == DDI_FAILURE) {
		cmn_err(CE_WARN, format,
			ddi_get_name(dip), ddi_get_instance(dip), name);
		ddi_remove_minor_node(dip, NULL);
		return (DDI_FAILURE);
	}
	(void) sprintf(name, "%c,cu", scb->h.sc_unit + 'a');
	if (ddi_create_minor_node(dip, name, S_IFCHR, scb->h.sc_unit | OUTLINE,
	    DDI_NT_SERIAL_MB_DO, NULL) == DDI_FAILURE) {
		cmn_err(CE_WARN, format,
			ddi_get_name(dip), ddi_get_instance(dip), name);
		ddi_remove_minor_node(dip, NULL);
		return (DDI_FAILURE);
	}
	return (DDI_SUCCESS);
}


int
se_create_ucm_minor_nodes(dev_info_t *dip, se_ctl_t *sca, se_ctl_t *scb)
{
	char *format = "%s%d: Failed to create node %s";
	char *name;

	/*
	 * Create the "ucm" and "ucmctl" nodes.
	 */
	name = "ucm";
	if (ddi_create_minor_node(dip, name, S_IFCHR,
	    sca->h.sc_unit | UCM_DEVICE, DDI_PSEUDO, NULL)
	    == DDI_FAILURE) {
		cmn_err(CE_WARN, format,
			ddi_get_name(dip), ddi_get_instance(dip), name);
		return (DDI_FAILURE);
	}
	name = "ucmctl";
	if (ddi_create_minor_node(dip, name, S_IFCHR,
	    scb->h.sc_unit | UCM_DEVICE, DDI_PSEUDO, NULL)
	    == DDI_FAILURE) {
		cmn_err(CE_WARN, format,
			ddi_get_name(dip), ddi_get_instance(dip), name);
		ddi_remove_minor_node(dip, NULL);
		return (DDI_FAILURE);
	}
	return (DDI_SUCCESS);
}
