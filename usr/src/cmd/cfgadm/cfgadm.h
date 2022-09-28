/*
 * Copyright (c) 1998, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident   "@(#)cfgadm.h 1.1     98/01/14 SMI"

/*
 * Command line options
 */
#define	OPTIONS		"c:fhlno:s:tx:vy"

/*
 * Configuration operations
 */
typedef enum {
	CFGA_OP_NONE = 0,
	CFGA_OP_CHANGE_STATE,
	CFGA_OP_TEST,
	CFGA_OP_LIST,
	CFGA_OP_PRIVATE,
	CFGA_OP_HELP
} cfga_op_t;

/*
 * Names for -c functions
 */
static char *state_opts[] = {
	"",
	"insert",
	"remove",
	"connect",
	"disconnect",
	"configure",
	"unconfigure",
	NULL
};

/*
 * Attachment point specifier types.
 */
typedef enum {
	UNKNOWN_AP,
	LOGICAL_AP_ID,
	PHYSICAL_AP_ID,
	AP_TYPE
} cfga_ap_types_t;

/*
 * Confirm values.
 */
enum confirm { CONFIRM_DEFAULT, CONFIRM_NO, CONFIRM_YES };

/* Limit size of sysinfo return */
#define	SYSINFO_LENGTH	256
#define	YESNO_STR_MAX	127

/* exit codes */
#define	EXIT_OK		0
#define	EXIT_OPFAILED	1
#define	EXIT_NOTSUPP	2
#define	EXIT_ARGERROR	3

/* Macro to figure size of cfga_stat_data items */
#define	SZ_EL(EL)	(sizeof ((struct cfga_stat_data *)NULL)->EL)

/* Macro to figure size of all cfga_stat_data items */
#define	N_FIELDS	(sizeof (all_fields)/sizeof (all_fields[0]))

/* printing format controls */
#define	DEF_SORT_FIELDS		"ap_id"
#define	DEF_COLS		"ap_id:r_state:o_state:condition"
#define	DEF_COLS2		NULL
#define	DEF_COLS_VERBOSE	"ap_id:r_state:o_state:condition:info"
#define	DEF_COLS2_VERBOSE	"status_time:type:busy:physid"
#define	DEF_DELIM		" "

/* listing field delimiter */
#define	FDELIM		':'
#define	ARG_DELIM	' '

/* listing lengths for various fields */
#define	STATE_WIDTH	12	/* longest - "disconnected" */
#define	COND_WIDTH	10	/* longest is the heading - "condition" */
#define	TIME_WIDTH	12
#define	TIME_P_WIDTH	14	/* YYYYMMDDhhmmss */
/*	Date and time	formats	*/
/*
 * b --- abbreviated month name
 * e --- day number
 * Y --- year in the form ccyy
 * H --- hour(24-hour version)
 * M --- minute
 */
#define	FORMAT1	 "%b %e  %Y"
#define	FORMAT2  "%b %e %H:%M"

/* listing control data */
struct sort_el {
	int reverse;
	struct field_info *fld;
};

struct print_col {
	int width;
	struct field_info *line1;
	struct field_info *line2;
};

struct field_info {
	char *name;
	char *heading;
	int width;
	int (*compare)(struct cfga_stat_data *, struct cfga_stat_data *);
	void (*printfn)(struct cfga_stat_data *, int, char *);
};

/* list option strings */
static char *list_options[] = {
#define	LIST_SORT	0
	"sort",
#define	LIST_COLS	1
	"cols",
#define	LIST_COLS2	2
	"cols2",
#define	LIST_DELIM	3
	"delim",
#define	LIST_NOHEADINGS	4
	"noheadings",
	NULL
};
