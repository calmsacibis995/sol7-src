/*
 * Copyright 1997 Sun Microsystems, Inc. All rights reserved.
 */

/*
 * PHOTON CONFIGURATION MANAGER
 * Common definitions
 */

#ifndef	_LUXDEF
#define	_LUXDEF

#pragma ident	"@(#)luxdef.h	1.11	98/01/18 SMI"

#include <sys/scsi/scsi.h>

/*
 * Include any headers you depend on.
 */

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Debug environmental flags.
 */
/* SCSI Commands */
#define	S_DPRINTF	if (getenv("_LUX_S_DEBUG") != NULL) (void) printf
/* General purpose */
#define	P_DPRINTF	if (getenv("_LUX_P_DEBUG") != NULL) (void) printf
/* Opens */
#define	O_DPRINTF	if (getenv("_LUX_O_DEBUG") != NULL) (void) printf
/* Ioctls */
#define	I_DPRINTF	if (getenv("_LUX_I_DEBUG") != NULL) (void) printf
/* Hot-Plug */
#define	H_DPRINTF	if (getenv("_LUX_H_DEBUG") != NULL) (void) printf
/* Convert Name debug variable. */
#define	L_DPRINTF	if (getenv("_LUX_L_DEBUG") != NULL) (void) printf
/* Getting status */
#define	G_DPRINTF	if (getenv("_LUX_G_DEBUG") != NULL) (void) printf

/* i18n */
#define	L_SET1			1	/* catalog set number */
#define	MSGSTR(Num, Str)	catgets(l_catd, L_SET1, Num, Str)

#define	MIN(a, b) (a < b ? a : b)


typedef	struct	wwn_list_struct {
	char	logical_path[MAXNAMELEN];
	char	physical_path[MAXNAMELEN];
	char	node_wwn_s[17];	/* NULL terminated string */
	char	port_wwn_s[17];	/* NULL terminated string */
	struct  wwn_list_struct	*wwn_next;
} WWN_list;


/*
 *		SCSI CDB structures
 */
typedef	struct	my_cdb_g0 {
	unsigned	char	cmd;
	unsigned	char	lba_msb;
	unsigned	char	lba;
	unsigned	char	lba_lsb;
	unsigned	char	count;
	unsigned	char	control;
	}my_cdb_g0;

typedef	struct	{
	unsigned	char	cmd;
	unsigned	char	byte1;
	unsigned	char	byte2;
	unsigned	char	byte3;
	unsigned	char	byte4;
	unsigned	char	byte5;
	unsigned	char	byte6;
	unsigned	char	byte7;
	unsigned	char	byte8;
	unsigned	char	byte9;
	}my_cdb_g1;

/*
 * NOTE: I use my own INQUIRY structure but it is based
 *	on the /scsi/generic/inquiry.h.
 *
 *	I use my own because  it is more complete.
 */

/*
 * Format of data returned as a result of an INQUIRY command.
 */

typedef struct l_inquiry_struct {
	/*
	* byte 0
	*
	* Bits 7-5 are the Peripheral Device Qualifier
	* Bits 4-0 are the Peripheral Device Type
	*
	*/
	u_char	inq_dtype;
	/* byte 1 */
	u_char	inq_rmb		: 1,	/* removable media */
		inq_qual	: 7;	/* device type qualifier */

	/* byte 2 */
	u_char	inq_iso		: 2,	/* ISO version */
		inq_ecma	: 3,	/* ECMA version */
		inq_ansi	: 3;	/* ANSI version */

	/* byte 3 */
#define	inq_aerc inq_aenc	/* SCSI-3 */
	u_char	inq_aenc	: 1,	/* async event notification cap. */
		inq_trmiop	: 1,	/* supports TERMINATE I/O PROC msg */
		inq_normaca	: 1,	/* Normal ACA Supported */
				: 1,	/* reserved */
		inq_rdf		: 4;	/* response data format */

	/* bytes 4-7 */
	u_char	inq_len;		/* additional length */
	u_char			: 8;	/* reserved */
	u_char			: 2,	/* reserved */
		inq_port	: 1,	/* Only defined when dual_p set */
		inq_dual_p	: 1,	/* Dual Port */
		inq_mchngr	: 1,	/* Medium Changer */
		inq_SIP_1	: 3;	/* Interlocked Protocol */

	union {
		u_char	inq_2_reladdr	: 1,	/* relative addressing */
			inq_wbus32	: 1,	/* 32 bit wide data xfers */
			inq_wbus16	: 1,	/* 16 bit wide data xfers */
			inq_sync	: 1,	/* synchronous data xfers */
			inq_linked	: 1,	/* linked commands */
			inq_res1	: 1,	/* reserved */
			inq_cmdque	: 1,	/* command queueing */
			inq_sftre	: 1;	/* Soft Reset option */
		u_char	inq_3_reladdr	: 1,	/* relative addressing */
			inq_SIP_2	: 3,	/* Interlocked Protocol */
			inq_3_linked	: 1,	/* linked commands */
			inq_trandis	: 1,	/* Transfer Disable */
			inq_3_cmdque	: 1,	/* command queueing */
			inq_SIP_3	: 1;	/* Interlocked Protocol */
	} ui;


	/* bytes 8-35 */

	u_char	inq_vid[8];		/* vendor ID */

	u_char	inq_pid[16];		/* product ID */

	u_char	inq_revision[4];	/* product revision level */

	/*
	 * Bytes 36-55 are vendor-specific parameter bytes
	 */

	/* SSA specific definitions */
	/* bytes 36 - 39 */
#define	inq_ven_specific_1 inq_firmware_rev
	u_char	inq_firmware_rev[4];	/* firmware revision level */

	/* bytes 40 - 52 */
	u_char	inq_serial[12];		/* serial number */

	/* bytes 52-53 */
	u_char	inq_res2[2];

	/* byte 54, 55 */
	u_char	inq_ssa_ports;		/* number of ports */
	u_char	inq_ssa_tgts;		/* number of targets */

	/*
	 * Bytes 56-95 are reserved.
	 */
	u_char	inq_res3[40];
	/*
	 * 96 to 'n' are vendor-specific parameter bytes
	 */
	u_char	inq_box_name[32];
} L_inquiry;

#define	MODEPAGE_GEOMETRY	0x04




/*
 *  structure for MODE SELECT/SENSE 10 byte page header
 *
 */
typedef struct mode_header_10_struct {
	u_short length;
	u_char medium_type; /* device specific */
	u_char device_specific; /* device specfic parameters */
	u_short	rsvdl;		/* reserved */
	u_short bdesc_length;	/* length of block descriptor(s), if any */
} Mode_header_10;

typedef	struct	mode_page_04_struct {
	struct	mode_page mode_page;	/* common mode page header */
	u_char	num_cylinders_hi;
	u_char	num_cylinders_mid;
	u_char	num_cylinders_lo;
	u_char	num_heads;
	u_char	write_precomp_hi;
	u_char	write_precomp_mid;
	u_char	write_precomp_lo;
	u_char	reduced_write_i_hi;
	u_char	reduced_write_i_mid;
	u_char	reduced_write_i_lo;
	u_short	step_rate;
	u_char	landing_zone_hi;
	u_char	landing_zone_mid;
	u_char	landing_zone_lo;
#if defined(_BIT_FIELDS_LTOH)
	u_char	rpl	: 2,	/* RPL */
			: 6;
#elif defined(_BIT_FIELDS_HTOL)
	u_char		: 6,
		rpl	: 2;    /* disable correction */
#else
#error  One of _BIT_FIELDS_LTOH or _BIT_FIELDS_HTOL must be defined
#endif  /* _BIT_FIELDS_LTOH */
	u_char	rot_offset;
	u_char	: 8;	/* reserved */
	u_short	rpm;
	u_char	: 8;	/* reserved */
	u_char	: 8;	/* reserved */
} Mp_04;


typedef	struct	mode_page_01_struct {
	struct	mode_page mode_page;	/* common mode page header */
#if defined(_BIT_FIELDS_LTOH)
	u_char	dcr	: 1,	/* disable correction */
		dte	: 1,	/* disable transfer on error */
		per	: 1,	/* post error */
		eec	: 1,	/* enable early correction */
		rc	: 1,	/* read continuous */
		tb	: 1,	/* transfer block */
		arre	: 1,	/* auto read realloc enabled */
		awre	: 1;	/* auto write realloc enabled */
#elif defined(_BIT_FIELDS_HTOL)
	u_char	awre	: 1,	/* auto write realloc enabled */
		arre	: 1,	/* auto read realloc enabled */
		tb	: 1,	/* transfer block */
		rc	: 1,	/* read continuous */
		eec	: 1,	/* enable early correction */
		per	: 1,	/* post error */
		dte	: 1,	/* disable transfer on error */
		dcr	: 1;    /* disable correction */
#else
#error  One of _BIT_FIELDS_LTOH or _BIT_FIELDS_HTOL must be defined
#endif  /* _BIT_FIELDS_LTOH */
	u_char	read_retry_count;
	u_char	correction_span;
	u_char	head_offset_count;
	u_char	strobe_offset_count;
	u_char			: 8;	/* reserved */
	u_char	write_retry_count;
	u_char			: 8;	/* reserved */
	u_short	recovery_time_limit;
} Mp_01;

/*
 * I define here for backward compatability
 * with 2.5.1
 */
struct my_mode_caching {
	struct	mode_page mode_page;	/* common mode page header */
#if defined(_BIT_FIELDS_LTOH)
	u_char	rcd		: 1,	/* Read Cache Disable */
		mf		: 1,	/* Multiplication Factor */
		wce		: 1,	/* Write Cache Enable */
				: 5;	/* Reserved */
	u_char	write_ret_prio	: 4,	/* Write Retention Priority */
		dmd_rd_ret_prio	: 4;	/* Demand Read Retention Priority */
#elif defined(_BIT_FIELDS_HTOL)
	u_char			: 5,	/* Reserved */
		wce		: 1,	/* Write Cache Enable */
		mf		: 1,	/* Multiplication Factor */
		rcd		: 1;	/* Read Cache Disable */
	u_char	dmd_rd_ret_prio	: 4,	/* Demand Read Retention Priority */
		write_ret_prio	: 4;	/* Write Retention Priority */
#else
#error	One of _BIT_FIELDS_LTOH or _BIT_FIELDS_HTOL must be defined
#endif	/* _BIT_FIELDS_LTOH */
	u_short	pf_dsbl_trans_len;	/* Disable prefetch transfer length */
	u_short	min_prefetch;		/* Minimum Prefetch */
	u_short	max_prefetch;		/* Maximum Prefetch */
	u_short	max_prefetch_ceiling;	/* Maximum Prefetch Ceiling */
};

/*		NOTE: These command op codes are not defined in commands.h */
#define		SCMD_SYNC_CACHE			0x35
#define		SCMD_LOG_SENSE			0x4d
#define		SCMD_PERS_RESERV_IN		0x5e
#define		SCMD_PERS_RESERV_OUT		0x5f


#define	MAX_MODE_SENSE_LEN	0xffff

/*
 * Define for physical name of children of fcp
 */
#define	FC_DRIVER	"sf@"
#define	FC_CTLR		":devctl"
#define	DRV_NAME_SD	"sd@"
#define	DRV_NAME_SSD	"ssd@"
#define	SLSH_DRV_NAME_SD	"/sd@"
#define	SLSH_DRV_NAME_SSD	"/ssd@"
#define	DRV_PART_NAME	",0:c,raw"
#define	SES_NAME	"ses@"
#define	SLSH_SES_NAME	"/ses@"

#define	IBFIRMWARE_FILE	"/LC_MESSAGES/ibfirmware"
#define	IBFIRMWARE_LOCALE	"/usr/lib/locale/"
#define	FCODE_FILE	"/usr/lib/firmware/fc_s/fcal_s_fcode"

/*
 * The following is for hotplug support
 */

#ifdef	SSA_HOTPLUG
#define	RPL_DEVICE	2
#endif

#define	SET_RQST_INSRT	0
#define	SET_RQST_RMV	1
#define	OVERALL_STATUS	2
#define	SET_FAULT	3
#define	SET_DRV_ON	4

#define	TARGET_ID(box_id, f_r, slot)    \
	((box_id | ((f_r == 'f' ? 0 : 1) << 4)) | (slot + 2))

#define	NEWER(time1, time2) 	(time1.tv_sec > time2.tv_sec)

struct hotplug_disk_list {
	struct dlist		*seslist;
	struct dlist		*dlhead;
	char			box_name[33];
	int			tid;
	int			slot;
	int			f_flag;	/* Front flag */
	int			dev_type;
	struct hotplug_disk_list *next;
	struct hotplug_disk_list *prev;
};

struct	dlist	{
	char	*dev_path;
	char	*logical_path;
	struct	dlist *multipath;
	struct	dlist *next;
	struct	dlist *prev;
};

#ifdef	__cplusplus
}
#endif

#endif	/* _LUXDEF */
