/*
 * Copyright 1997 Sun Microsystems, Inc. All rights reserved.
 */

/*
 * SES State definitions
 */

#ifndef	_STATE_H
#define	_STATE_H

#pragma ident	"@(#)state.h	1.12	98/01/14 SMI"

/*
 * Include any headers you depend on.
 */
#include <sys/fc4/fcal_linkapp.h>
#include <sys/scsi/adapters/sfio.h>
#include <sys/socalio.h>

#ifdef	__cplusplus
extern "C" {
#endif

/* */
#define	ENCLOSURE_PROD_ID	"SENA"
#define	ENCLOSURE_PROD_NAME	"Network Array"
#define	MAX_DRIVES_PER_BOX	22
#define	L_WWN_LENGTH		16
#define	BOX_ID_MASK		0x60
#define	BOX_ID			0x0d
#define	ALT_BOX_ID		0x10
#define	L_ARCH_4D		"sun4d"
#define	L_ARCH_4M		"sun4m"
#define	WWN_SIZE		8

#define	L_LED_STATUS		0x00
#define	L_LED_RQST_IDENTIFY	0x01
#define	L_LED_ON		0x02
#define	L_LED_OFF		0x04


typedef	struct	box_list_struct {
	u_char	prod_id_s[17];	/* NULL terminated string */
	u_char	b_name[33];	/* NULL terminated string */
	char	logical_path[MAXNAMELEN];
	char	b_physical_path[MAXNAMELEN];
	char	b_node_wwn_s[17];	/* NULL terminated string */
	u_char	b_node_wwn[8];
	char	b_port_wwn_s[17];	/* NULL terminated string */
	u_char	b_port_wwn[8];
	struct  box_list_struct	*box_next;
} Box_list;

typedef	struct	path_struct {
	char	*p_physical_path;
	char	*argv;
	int	slot_valid;	/* Slot valid flag. */
	int	slot;
	int	f_flag;		/* Front/rear flag. 1 = front */
	int	ib_path_flag;
} Path_struct;

/*
 * device map
 */
typedef	struct	al_rls {
	char			driver_path[MAXNAMELEN];
	u_int			al_ha;
	struct rls_payload	payload;
	struct al_rls		*next;
} AL_rls;

/*
 * Definitions for Persistent Reservation commands.
 */
typedef	struct	read_keys_struct {
	int	rk_generation;
	int	rk_length;
	int	rk_key[256];
} Read_keys;

typedef	struct	read_reserv_struct {
	int	rr_generation;
	int	rr_length;
} Read_reserv;

#define	ACTION_READ_KEYS	0x00
#define	ACTION_READ_RESERV	0x01
#define	ACTION_REGISTER		0x00
#define	ACTION_RESERVE		0x01
#define	ACTION_RELEASE		0x02
#define	ACTION_CLEAR		0x03
#define	ACTION_PREEMPT		0x04
#define	ACTION_PREEMPT_CLR	0x05



/*
 * Definitions for send/receive diagnostic command
 */
#define	HEADER_LEN		4
#define	MAX_REC_DIAG_LENGTH	0xfffe

/* Page defines */
#define	L_PAGE_PAGE_LIST	0x00	/* Supported pages page */
#define	L_PAGE_CONFIG		0x01	/* Configuration page */
#define	L_PAGE_1		L_PAGE_CONFIG
#define	L_PAGE_ENCL_CTL		0x02	/* Enclosure Control page */
#define	L_PAGE_ENCL_STATUS	0x02	/* Enclosure status page */
#define	L_PAGE_2		L_PAGE_ENCL_STATUS
#define	L_PAGE_STRING		0x04
#define	L_PAGE_4		L_PAGE_STRING
#define	L_PAGE_7		0x07	/* Element Descriptor Page */

typedef struct	rec_diag_hdr {
	u_char	page_code;
	u_char	sub_enclosures;
	u_short	page_len;
} Rec_diag_hdr;


/*
 * Page 0
 */
typedef	struct	ib_page_0 {
	u_char		page_code;
	u_char		sub_enclosures;
	u_short		page_len;
	u_char		sup_page_codes[0x100];
} IB_page_0;

#define	MAX_POSSIBLE_ELEMENTS	255

/*
 * Page 1
 * Configuration page
 */
typedef	struct	type_desc_hdr {
	u_char	type;
	u_char	num;
	u_char	sub_id;
	u_char	text_len;
} Type_desc_hdr;

typedef	struct	type_desc_text {
	u_char	text_element[256];
} Type_desc_text;

#define	MAX_IB_ELEMENTS	50
#define	MAX_VEND_SPECIFIC_ENC	216
typedef	struct	ib_page_config {
	u_char		page_code;
	u_char		sub_enclosures;
	u_short		page_len;
	u_int		gen_code;
	/* Enclosure descriptor header */
	u_char		enc_res;
	u_char		enc_sub_id;
	u_char		enc_num_elem;
	u_char		enc_len;
	/* Enclosure descriptor */
	u_char		enc_node_wwn[8];
	u_char		vend_id[8];
	u_char		prod_id[16];
	u_char		prod_revision[4];
	u_char		res[MAX_VEND_SPECIFIC_ENC];
	Type_desc_hdr	type_hdr[MAX_IB_ELEMENTS];
	Type_desc_text	text[MAX_IB_ELEMENTS];
} IB_page_config;

/*
 *	FRU types internal and external (host SES type)
 */
#define	ELM_TYP_NONE	0x0	/* Unspecified */
#define	ELM_TYP_DD	0x01	/* Disk Drive - device */
#define	ELM_TYP_PS	0x02	/* Power Supply */
#define	ELM_TYP_FT	0x03	/* Fan Tray - cooling element */
#define	ELM_TYP_TS	0x04	/* Temperature Sensors */
#define	ELM_TYP_FP	0x0c	/* FPM screen - display */
#define	ELM_TYP_KP	0x0d	/* keypad on FPM - keypad device */
#define	ELM_TYP_FL	0x0f	/* Fibre Link module - SCSI port/trancvr */
#define	ELM_TYP_LN	0x10	/* Language */
#define	ELM_TYP_SP	0x11	/* Serial Port - communicaion port */
#define	ELM_TYP_MB	0x80	/* Motherboard/Centerplane */
#define	ELM_TYP_IB	0x81	/* IB(ESI) - controller electronics */
#define	ELM_TYP_BP	0x82	/* BackPlane */
#define	ELM_TYP_LO	0xa0	/* Loop Configuration */
#define	ELM_TYP_OR	0xa2	/* Orientation */


/*
 * Page 2
 * Enclosure status/control page
 */
/*
 * Loop Configuration.
 */
typedef struct	loop_element_status {
	u_char			: 1,		/* reserved */
		prd_fail	: 1,
				: 2,		/* reserved */
		code		: 4;
	u_char			: 8;		/* reserved */
	u_char			: 8;		/* reserved */
	u_char			: 7,		/* reserved */
		split		: 1;
} Loop_elem_st;

/*
 * Language
 */
typedef struct	language_element_status {
	u_char			: 1,		/* reserved */
		prd_fail	: 1,
				: 2,		/* reserved */
		code		: 4;
	u_char			: 8;		/* reserved */
	u_short		language_code;
} Lang_elem_st;

/*
 * Tranceiver status
 */
typedef struct	trans_element_status {
	u_char			: 1,		/* reserved */
		prd_fail	: 1,
				: 2,		/* reserved */
		code		: 4;
	u_char			: 8;		/* reserved */
	u_char			: 7,
		report		: 1;
	u_char			: 3,		/* reserved */
		disabled	: 1,
				: 2,
		lol		: 1,
		lsr_fail	: 1;
} Trans_elem_st;

/*
 * ESI Controller status
 */
typedef struct	ctlr_element_status {
	u_char			: 1,		/* reserved */
		prd_fail	: 1,
				: 2,		/* reserved */
		code		: 4;
	u_char			: 8;		/* reserved */
	u_char			: 7,		/* reserved */
		report		: 1;
	u_char			: 4,		/* reserved */
		overtemp_alart	: 1,
				: 1,		/* reserved */
		ib_loop_1_fail	: 1,
		ib_loop_0_fail	: 1;
} Ctlr_elem_st;

/*
 * Backplane status
 */
typedef struct	bp_element_status {
	u_char			: 1,		/* reserved */
		prd_fail	: 1,
				: 2,		/* reserved */
		code		: 4;
	u_char			: 8;		/* reserved */
	u_char			: 8;		/* reserved */
	u_char			: 3,		/* reserved */
		disabled	: 1,
				: 2,		/* reserved */
		byp_a_enabled	: 1,
		byp_b_enabled	: 1;

} Bp_elem_st;

/*
 * Temperature sensor status
 */
typedef struct	temp_element_status {
	u_char			: 1,		/* reserved */
		prd_fail	: 1,
				: 2,		/* reserved */
		code		: 4;
	u_char			: 8;		/* reserved */
	char			degrees;
	u_char			: 4,		/* reserved */
		ot_fail		: 1,
		ot_warn		: 1,
		ut_fail		: 1,
		ut_warn		: 1;
} Temp_elem_st;

typedef struct	fan_element_status {
	u_char			: 1,		/* reserved */
		prd_fail	: 1,
				: 2,		/* reserved */
		code		: 4;
	u_char			: 8;		/* reserved */
	u_char			: 8;		/* reserved */
	u_char			: 1,		/* reserved */
		fail		: 1,
		rqsted_on	: 1,
				: 2,
		speed		: 3;
} Fan_elem_st;
#define	S_HI_SPEED	0x5


typedef	struct	ps_element_status {
	u_char			: 1,		/* reserved */
		prd_fail	: 1,
				: 1,		/* reserved */
		swap		: 1,
		code		: 4;
	u_char			: 8;		/* reserved */
	u_char			: 4,		/* reserved */
		dc_over		: 1,
		dc_under	: 1,
		dc_over_i	: 1,
				: 1;		/* reserved */
	u_char			: 1,		/* reserved */
		fail		: 1,
		rqsted_on	: 1,
				: 1,
		ovrtmp_fail	: 1,
		temp_warn	: 1,
		ac_fail		: 1,
		dc_fail		: 1;
} Ps_elem_st;

/* code (status code) definitions */
#define	S_OK		0x01
#define	S_CRITICAL	0x02
#define	S_NONCRITICAL	0x03
#define	S_NOT_INSTALLED	0x05
#define	S_NOT_AVAILABLE	0x07


typedef	struct	device_element {
	u_char	select		: 1,
		prd_fail	: 1,
		disable		: 1,
		swap		: 1,
		code		: 4;
	u_char	sel_id;				/* Hard address */
	u_char			: 1,
		dont_remove	: 1,
				: 2,
		rdy_to_ins	: 1,
		rmv		: 1,
		ident		: 1,
		report		: 1;
	u_char			: 1,		/* reserved */
		fault		: 1,
		fault_req	: 1,
		dev_off		: 1,
		en_bypass_a	: 1,
		en_bypass_b	: 1,
		bypass_a_en	: 1,
		bypass_b_en	: 1;
} Dev_elem_st;

typedef struct	interconnect_assem_status {
	u_char			: 4,		/* reserved */
		code		: 4;
	u_char			: 8;		/* reserved */
	u_char			: 8;		/* reserved */
	u_char			: 7,		/* reserved */
		eprom_fail	: 1;
} Interconnect_st;

typedef	struct	ib_page_2 {
	u_char	page_code;
	union {
		u_char	res	: 3,	/* Reserved */
			invop	: 1,
			info	: 1,
			non_crit	: 1,
			crit	: 1,
			unrec	: 1;
		u_char	ab_cond;
	} ui;
	u_short	page_len;
	u_int	gen_code;
	u_int	element[MAX_POSSIBLE_ELEMENTS];
} IB_page_2;

/*
 * Page 4
 *
 * String page.
 */
typedef	struct page4_name {
	u_char		page_code;
	u_char		: 8;		/* reserved */
	u_short		page_len;
	u_char		string_code;
	u_char		: 7,
			enable	: 1;
	u_char		: 8;		/* reserved */
	u_char		: 8;		/* reserved */
	u_char		name[32];
} Page4_name;

/* String codes. */
#define	L_WWN		0x01
#define	L_PASSWORD	0x02
#define	L_ENCL_NAME	0x03
#define	L_BOX_ID	0x04
#define	L_AUTO_LIP	0x05


typedef	struct	element_descriptor {
	u_char		: 8;		/* reserved */
	u_char		: 8;		/* reserved */
	u_short		desc_len;
	u_char		desc_string[0xff];
} Elem_desc;

typedef	struct	ib_page_7 {
	u_char		page_code;
	u_char		: 8;		/* reserved */
	u_short		page_len;
	u_int		gen_code;
	Elem_desc	element_desc[MAX_POSSIBLE_ELEMENTS];
} IB_page_7;

/* ------------------------------------------------------------------------ */

/* structure for IB */
typedef struct ib_state_struct {
	u_char	enclosure_name[33];	/* extra character is NULL */
	IB_page_0	p0;
	IB_page_config	config;		/* Enclosure configuration page */
	IB_page_2	p2_s;		/* Enclosure status page */
	IB_page_7	p7_s;		/* Element descriptor page */
	int		res;
	int		box_id;
	struct dlist	*ib_multipath_list;
} Ib_state;

/* ------------------------------------------------------------------------ */

/*
 * We should use the scsi_capacity structure in impl/commands.h
 * but it uses u_long's to define 32 bit values.
 */
typedef	struct	capacity_data_struct {
	u_int	last_block_addr;
	u_int	block_size;
} Read_capacity_data;


/* Individual drive state */
typedef struct l_disk_state_struct {
	u_int		num_blocks;	/* Capacity */
	char		physical_path[MAXNAMELEN]; /* First one found */
	struct dlist	*multipath_list;
	Dev_elem_st	ib_status;
	char		node_wwn_s[17];		/* NULL terminated string */
	/* Persistant Reservations */
	int		persistent_reserv_flag;
	int		persistent_active, persistent_registered;
	int		l_state_flag;		/* loop state */
	/*
	 * State for each port
	 * index 1 is status for port A
	 */
#define	PORT_B			0x00
#define	PORT_A			0x01
#define	PORT_A_B		0x02
	int		d_state_flags[2];	/* Disk state */
	int		port_a_valid;		/* If disk state is valid */
	int		port_b_valid;		/* If disk state is valid */
	char		port_a_wwn_s[17];	/* NULL terminated string */
	char		port_b_wwn_s[17];	/* NULL terminated string */
} L_disk_state;


/* status flag definitions */
#define	L_OK			0x00	/* NOTE: Must be zero. */
#define	L_NOT_READY		0x01
#define	L_NOT_READABLE		0x02
#define	L_SPUN_DWN_D		0x04
#define	L_RESERVED		0x08
#define	L_OPEN_FAIL		0x10
#define	L_NO_LABEL		0x20
#define	L_SCSI_ERR		0x40
/* Applicable to loop state */
#define	L_NO_LOOP		0x80	/* drive not accessable */
#define	L_INVALID_WWN		0x100
#define	L_INVALID_MAP		0x200
#define	L_NO_PATH_FOUND		0x400

/* ------------------------------------------------------------------------ */

/*
 *		State of the Photon
 */
typedef struct l_state_struct {
	Ib_state	ib_tbl;	/* state of controller */

	int		total_num_drv;
	L_disk_state	drv_front[MAX_DRIVES_PER_BOX/2];
	L_disk_state	drv_rear[MAX_DRIVES_PER_BOX/2];
} L_state;



/*
 * Error definitions.
 */
#define	L_REC_DIAG_PG1	0x20600		/* Error with Rec Diag page 1 */
#define	L_INVALID_PATH	0x20200		/* Invalid path name */
#define	L_SCSI_ERROR	0x10000		/* SCSI error */
#define	L_DOWNLOAD_FILE		0x20300 /* Download file error */
#define	L_DOWNLOAD_CHKSUM	0x20301	/* Invalid download file checksum */
#define	L_TRANSFER_LEN	0x20601		/* Invalid transfer length */
#define	L_SCSI_ERROR	0x10000		/* SCSI Error */
#define	L_INVALID_PATH	0x20200		/* Invalid path name */
#define	L_DOWNLOAD_FILE	0x20300	 /* Download file error */
#define	L_LOOP_ERROR	0x20201		/* Loop error */
/* Warning define. */
#define	L_WARNING	0x30000


/*
 * Performance Statistics
 */

/*
 * format parameter to dump()
 */
#define	HEX_ONLY	0	/* print hex only */
#define	HEX_ASCII	1	/* hex and ascii */

#ifdef	__cplusplus
}
#endif

#endif	/* _STATE_H */
