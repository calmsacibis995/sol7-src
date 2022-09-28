/*
 * Copyright 1997 Sun Microsystems, Inc. All rights reserved.
 */


#pragma ident   "@(#)adm.c 1.35    98/01/23 SMI"

/*LINTLIBRARY*/


/*
 *	Administration program for the photon
 */

#include	<stdlib.h>
#include	<stdio.h>
#include	<sys/file.h>
#include	<sys/errno.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include	<sys/param.h>
#include	<fcntl.h>
#include	<unistd.h>
#include	<errno.h>
#include	<malloc.h>
#include	<memory.h>
#include	<string.h>
#include	<ctype.h>
#include	<assert.h>
#include	<time.h>
#include	<libintl.h>	/* gettext */
#include	<sys/sunddi.h>
#include	<sys/ddi.h>	/* for min */
#include	<sys/systeminfo.h>
#include	<sys/scsi/scsi.h>
#include	<strings.h>
#include	<sys/stat.h>
#include	<kstat.h>
#include	<sys/mkdev.h>
#include	<sys/utsname.h>
#include	<locale.h>
#include	<nl_types.h>
#include	<dirent.h>
#include	<termio.h>	/* For password */
#ifdef	DEVCTL_LIB
#include	<sys/devctl.h>
#endif
#include	<signal.h>

#include	"luxdef.h"
#include	"state.h"



#define	USEAGE()	{(void) fprintf(stderr,  MSGSTR(-1, \
"Usage: %s [-v] subcommand [option...]" \
" {enclosure[,dev]... | pathname...}\n"), \
		whoami); \
(void) fflush(stderr); }
#define	E_USEAGE()	{(void) fprintf(stderr,  MSGSTR(-1, \
"Usage: %s [-v] -e subcommand [option...]" \
" {enclosure[,dev]... | pathname...}\n"), \
		whoami); \
(void) fflush(stderr); }
#define	VERBPRINT	if (Options & PVERBOSE) (void) printf
#define	L_ERR_PRINT \
	if ((l_error_msg_ptr == NULL) && (p_error_msg_ptr == NULL)) {  \
		perror("Error");     \
	} else if (l_error_msg_ptr != NULL) {        \
		(void) fprintf(stderr, "Error: %s", l_error_msg_ptr);  \
	} else {	\
	    (void) fprintf(stderr, "%s: %s", whoami, p_error_msg_ptr);	\
	} \
	p_error_msg_ptr = l_error_msg_ptr = NULL;

#define	L_WARNING_PRINT \
	(void) fprintf(stderr, "Warning: %s", l_error_msg_ptr); \
	l_error_msg_ptr = NULL;


#define	P_ERR_PRINT	if (p_error_msg_ptr == NULL) {	\
	    perror(whoami);	\
	} else {	\
	    (void) fprintf(stderr, "%s: %s", whoami, p_error_msg_ptr);	\
	}

static	char	*dtype[] = {
	"Disk device",
	"Tape device",
	"Printer device",
	"Processor device",
	"WORM device",
	"CD-ROM device",
	"Scanner device",
	"Optical memory device",
	"Medium changer device",
	"Communications device",
	"Graphic arts device",
	"Graphic arts device",
	"Array controller device",
	"SES device",
	"Reserved",
	"Reserved"
};

/*
 * cmd codes
 */
/* Primary commands */
#define	ENCLOSURE_NAMES	100
#define	DISPLAY		101
#define	DOWNLOAD	102
#define	FAST_WRITE	400	/* SSA */
#define	FCAL_UPDATE	103	/* Update the Fcode on Sbus soc card */
#define	FC_UPDATE	401	/* SSA */
#define	INQUIRY		105
#define	INSERT_DEVICE	106	/* Hot plug */
#define	LED		107
#define	LED_ON		108
#define	LED_OFF		109
#define	LED_BLINK	110
#define	NVRAM_DATA	402	/* SSA */
#define	POWER_OFF	403	/* SSA */
#define	POWER_ON	111
#define	PASSWORD	112
#define	PURGE		404	/* SSA */
#define	PERF_STATISTICS	405	/* SSA */
#define	PROBE		113
#define	REMOVE_DEVICE	114	/* hot plug */
#define	RELEASE		210
#define	RESERVE		211
#define	START		213
#define	STOP		214
#define	SYNC_CACHE	406	/* SSA */
#define	SET_BOOT_DEV	115	/* Set the boot-device variable in nvram */
/* Enclosure Specific */
#define	ALARM		407	/* SSA */
#define	ALARM_OFF	408	/* SSA */
#define	ALARM_ON	409	/* SSA */
#define	ALARM_SET	410	/* SSA */
#define	ENV_DISPLAY	411	/* SSA */
/* Expert commands */
#define	RDLS		215
#define	P_BYPASS	218
#define	P_ENABLE	219
#define	FORCELIP	220
/* Undocumented commands */
#define	DUMP		300
#define	CHECK_FILE	301	/* Undocumented - Check download file */
#define	DUMP_MAP	302	/* Dump map of loop */

/* for hotplugging */
#define	REPLACE_DEVICE	150
#ifdef	SSA_HOTPLUG
#define	DEV_ONLINE	155
#define	DEV_OFFLINE	156
#define	DEV_GETSTATE	157
#define	DEV_RESET	158
#define	BUS_QUIESCE	160
#define	BUS_UNQUIESCE	161
#define	BUS_GETSTATE	162
#define	BUS_RESET	163
#define	BUS_RESETALL	164
#endif	/* SSA_HOTPLUG */


/* global variables */
char	*whoami;
int	Options;
const	OPTION_A	    = 0x00000001;
const	OPTION_C	    = 0x00000002;
const	OPTION_D	    = 0x00000004;
const	OPTION_E	    = 0x00000008;
const	OPTION_F	    = 0x00000010;
const	OPTION_L	    = 0x00000020;
const	OPTION_P	    = 0x00000040;
const	OPTION_R	    = 0x00000080;
const	OPTION_T	    = 0x00000100;
const	OPTION_V	    = 0x00000200;
const	OPTION_W	    = 0x00000400;
const	OPTION_Z	    = 0x00000800;
const	OPTION_Y	    = 0x00001000;
const	OPTION_CAPF	    = 0x00002000;
const	PVERBOSE	    = 0x00004000;	/* -v */
const	SAVE		    = 0x00008000;	/* save */
const	EXPERT		    = 0x00010000;

char	lux_version[] = "version: 1.35 98/01/23";


/* Function definitions */
static void pho_display_config(char *);
static int encl_status_page_funcs(int, char *, int, char *,
	struct l_state_struct  *, int, int, int);
static void remove_nodes(struct dlist *);
static void remove_ses_nodes(struct dlist *);
static void l_insert_photon();
static void display_logical_nodes(struct dlist *);
static int  l_hotplug(int, char **, int);
#ifdef	SSA_HOTPLUG
static int  l_hotplug_e(int, char **, int);
extern void print_dev_state(char *, int);
extern void print_bus_state(char *, int);
extern int dev_handle_insert(char *);
extern int dev_handle_remove(char *);
extern int dev_handle_replace(char *);
#endif
static void l_pre_hotplug(struct hotplug_disk_list **, int, int);
static int  l_post_hotplug(struct hotplug_disk_list *, int, int);
static int  device_in_map(sf_al_map_t *, int);
static int  device_present(char *, int, sf_al_map_t *, int, char **);
static void make_node(char *, int, char *, sf_al_map_t *, int);
static int l_offline_photon(struct hotplug_disk_list *);
static int l_offline_drive(struct dlist *, char *);
static void l_online_drive(struct dlist *, char *);
static int print_list_warn();
static void l_forcelip_all(struct hotplug_disk_list *);
static void l_get_drive_name(char *, int, int, char *);
void	ll_to_str(u_char *, char *);
static void temperature_messages(struct l_state_struct *, int);
static void ctlr_messages(struct l_state_struct *, int, int);
static void fan_messages(struct l_state_struct *, int, int);
static void ps_messages(struct l_state_struct *, int, int);
static void abnormal_condition_display(struct l_state_struct *);
static void loop_messages(struct l_state_struct *, int, int);
static void revision_msg(struct l_state_struct *, int);
static int  l_dev_stop(char *, char *, int);
static int  l_dev_start(char *, char *, int);
static void mb_messages(struct l_state_struct *, int, int);
static void back_plane_messages(struct l_state_struct *, int, int);

/*
 * global pointer to the error message
 *
 * This pointer will be NULL if no error message is available.
 *
 * If it is not NULL it will point to a
 * error message (NULL terminated).
 * This error message will contain the reason the function returned
 * a non-zero status.
 *
 */
extern  char 	*get_physical_name(char *);
#ifdef	SSA_HOTPLUG
extern char	*get_dev_or_bus_phys_name(char *);
#endif
extern	char	*p_error_msg_ptr;
extern	char	*l_error_msg_ptr;
extern	char	l_error_msg[];


/* externs */
extern	void	p_dump(char *, u_char *, int, int);
extern	int	p_download(char *, char *, int, int, char *);
extern	void	ssa_fast_write(char *);
extern	void	ssa_perf_statistics(char *);
extern	void	ssa_cli_start(char **, int);
extern	void	ssa_cli_stop(char **, int);
extern	void	ssa_cli_display_config(char **argv, char *, int, int, int);
extern	void	cli_display_envsen_data(char **, int);
extern	int	p_sync_cache(char *);
extern	int	p_purge(char *);
extern	void	led(char *, int);
extern	void	alarm_enable(char **, int, int);
extern	void	alarm_set(char **, int);
extern	void	power_off(char **, int);

extern	int	l_start(char *);
extern	int	l_stop(char *, int);
extern	int	l_download(char *, char *, int, int);
extern	int	l_reserve(char *);
extern	int	l_release(char *);
extern	int	p_check_file(char *, int);
extern	void	fcal_update(int, char *);
extern	void	fc_update(int, int, char *);
extern	int	setboot(unsigned, unsigned, char *);

extern	int	l_get_inquiry(char *, L_inquiry *);
extern	int	l_get_envsen(char *, u_char *, int, int);
extern	int	l_get_wwn(char *, u_char port_wwn[], u_char node_wwn[],
		int *, int);
extern	int	l_get_wwn_list(struct wwn_list_struct **, int);
extern	void	l_free_wwn_list(struct wwn_list_struct **);
extern	int	l_get_dev_map(char *, sf_al_map_t *, int);
extern	int	l_get_disk_status(char *, struct l_disk_state_struct *, int);
extern	int	l_get_status(char *, struct l_state_struct *, int);
extern	int	l_check_file(char *, int);
extern  char	*l_convert_name(char *, struct  path_struct **, int);
extern	int	l_force_lip(char *, int);
extern	int	l_get_box_list(struct box_list_struct **, int);
extern	void	l_free_box_list(struct box_list_struct **);
extern	int	l_rdls(char *, struct al_rls **, int);
extern	int	l_new_name(char *, char *, int);
extern	int	l_pwr_up_down(char *, int, int, int, int);
extern	int	l_bypass(char *, int);
extern	int	l_enable(char *, int);
extern	int	object_open(char *, int);
extern	int	l_get_envsen_page(int, u_char *, int, u_char, int);
extern	int	scsi_send_diag_cmd(int, u_char *, int);
extern	u_char	l_switch_to_alpa[];
extern	u_char	l_sf_alpa_to_switch[];
extern	int	name_id;	/* Architecture variable */
extern	int	l_get_ses_path(char *, char *, sf_al_map_t *, int);
extern	int	l_get_slot(struct path_struct *, int);
extern	int	l_led(struct path_struct *, int, struct device_element *, int);
extern	struct	dlist *l_get_multipath(char *, int);
extern	void	l_free_multipath(struct dlist *);
extern	struct	dlist *l_get_allses(char *, struct box_list_struct *, int);
extern	void	*zalloc(int);
extern	int	l_duplicate_names(Box_list *, char wwn[], char *, int);
extern	int	l_get_disk_element_index(struct l_state_struct *, int *,
		int *);
extern	int	l_pho_pwr_up_down(char *, char *, int, int);
extern	int	l_dev_pwr_up_down(char *, char *, struct path_struct *,
		int, int);
extern	int	l_get_slot(Path_struct *, int);
extern	int	l_new_password(char *, char *, int);
extern	int	l_get_mode_pg(char *, u_char **, int);
extern	int	l_get_port(char *, int *, int);
extern	int	l_get_disk_port_status(char *, struct l_disk_state_struct *,
		int, int);
extern	int	l_get_limited_map(char *, struct lilpmap *, int);
extern	int	l_ex_open_test(struct dlist *, char *);


/* Globel variables */
nl_catd		l_catd;
static		struct termios	termios;
static		int termio_fd;
WWN_list	*g_WWN_list = NULL;
extern		u_char l_sf_alpa_to_switch[];


/*
 *	Download subsystem microcode.
 *
 *	Path must point to a LUX IB or SSA controller.
 */
#define	SSAFIRMWARE_FILE	"/usr/lib/firmware/ssa/ssafirmware"
static	void
adm_download(char **argv, char *file_name, char *wwn)
{
int		path_index = 0;
char		*path_phys;
L_inquiry	inq;
Path_struct	*path_struct;

	while (argv[path_index] != NULL) {
		/*
		 * See what kind of device we are talking to.
		 */
		if ((path_phys = l_convert_name(argv[path_index],
			&path_struct, Options & PVERBOSE)) == NULL) {
			L_ERR_PRINT;
			exit(-1);
		}
		if (l_get_inquiry(path_phys, &inq)) {
			L_ERR_PRINT;
			exit(-1);
		}
		if ((strstr((char *)inq.inq_pid, ENCLOSURE_PROD_ID) != 0) ||
			(strncmp((char *)inq.inq_vid, "SUN     ",
			sizeof (inq.inq_vid)) &&
			(inq.inq_dtype == DTYPE_ESI))) {
		/*
		 * Again this is like the ssaadm code in that the name
		 * is still not defined before this code must be released.
		 */
			/*
			 * Can only update the WWN on SSA's
			 */
			if (Options & OPTION_W) {
				(void) fprintf(stderr,
			MSGSTR(-1, "The WWN is not programmable "
				"on this subsystem.\n"));
				exit(-1);
			}
			if (l_download(path_phys,
				file_name, (Options & SAVE),
				(Options & PVERBOSE))) {
				L_ERR_PRINT;
				exit(-1);
			}
		} else {
			if (!file_name) {
				file_name = SSAFIRMWARE_FILE;
				(void) fprintf(stdout,
				"  Using file %s.\n", file_name);
			}

			if (p_download(path_phys,
				file_name, 1, (Options & PVERBOSE), wwn)) {
				(void) fprintf(stderr,
				MSGSTR(-1, "Download Failed.\n"));
				P_ERR_PRINT;
				exit(-1);
			}
		}
		path_index++;
	}
}

static	void
display_link_status(char **argv)
{
AL_rls		*rls = NULL, *n;
int		path_index = 0;
char		*path_phys;
Path_struct	*path_struct;


	while (argv[path_index] != NULL) {
		if ((path_phys = l_convert_name(argv[path_index],
			&path_struct, Options & PVERBOSE)) == NULL) {
			L_ERR_PRINT;
			exit(-1);
		}
		if (l_rdls(path_phys, &rls, Options & PVERBOSE)) {
		    L_ERR_PRINT;
		    exit(-1);
		}
		n = rls;
		if (n != NULL) {
			(void) fprintf(stdout,
			MSGSTR(-1, "\nLink Error Status "
				"information for loop:%s\n"),
				n->driver_path);
			(void) fprintf(stdout, MSGSTR(-1, "al_pa   lnk fail "
			"   sync loss   signal loss   sequence err"
			"   invalid word   CRC\n"));
		}
		while (n) {
			(void) fprintf(stdout,
			MSGSTR(-1, "%x\t%-12d%-12d%-14d%-15d%-15d%-12d\n"),
			n->al_ha,
			n->payload.rls_linkfail, n->payload.rls_syncfail,
			n->payload.rls_sigfail, n->payload.rls_primitiverr,
			n->payload.rls_invalidword, n->payload.rls_invalidcrc);
			n = n->next;
		}

		path_index++;
	}
	(void) fprintf(stdout,
		MSGSTR(-1, "NOTE: These LESB counts are not"
		" cleared by a reset, only power cycles.\n"
		"These counts must be compared"
		" to previously read counts.\n"));
}

/*
 * Check to see if IB 0 or 1 is present in the box.
 *
 * RETURN: 1 = present
 */
static	int
ib_present_chk(struct l_state_struct *l_state, int which_one)
{
Ctlr_elem_st	ctlr;
int	i;
int	elem_index = 0;
int	result = 1;

	for (i = 0; i < (int)l_state->ib_tbl.config.enc_num_elem; i++) {
	    elem_index++;		/* skip global */
	    if (l_state->ib_tbl.config.type_hdr[i].type == ELM_TYP_IB) {
		bcopy((const void *)
			&l_state->ib_tbl.p2_s.element[elem_index + which_one],
			(void *)&ctlr, sizeof (ctlr));
		if (ctlr.code == S_NOT_INSTALLED) {
			result = 0;
		}
		break;
	    }
	    elem_index += l_state->ib_tbl.config.type_hdr[i].num;
	}
	return (result);
}

/*
 * Print individual disk status.
 */
static void
print_individual_state(struct l_disk_state_struct *dsk_ptr,
	struct l_state_struct *l_state,
	int status, int port)
{
	if (status & L_OPEN_FAIL) {
		(void) fprintf(stdout,
		MSGSTR(-1,
				" (Open Failed)  "));
	} else if (status & L_NOT_READY) {
		(void) fprintf(stdout,
		MSGSTR(-1,
				" (Not Ready)    "));
	} else if (status & L_NOT_READABLE) {
		(void) fprintf(stdout,
		MSGSTR(-1,
				"(Not Readable)  "));
	} else if (status & L_SPUN_DWN_D) {
		(void) fprintf(stdout,
		MSGSTR(-1,
				" (Spun Down)    "));
	} else if (status & L_SCSI_ERR) {
		(void) fprintf(stdout,
		MSGSTR(-1,
				" (SCSI Error)   "));
	} else if (status & L_RESERVED) {
		if (port == PORT_A) {
			(void) fprintf(stdout,
			MSGSTR(-1,
				" (Rsrv cnflt:A) "));
		} else if (port == PORT_B) {
			(void) fprintf(stdout,
			MSGSTR(-1,
				" (Rsrv cnflt:B) "));
		} else {
			(void) fprintf(stdout,
			MSGSTR(-1,
				" (Reserve cnflt)"));
		}
	} else if (status & L_NO_LABEL) {
		(void) fprintf(stdout,
		MSGSTR(-1,
				"(No UNIX Label) "));
	/*
	 * Before printing that the port is bypassed
	 * verify that there is an IB for this port.
	 * If not then don't print.
	 */
	} else if (ib_present_chk(l_state, 0) &&
		dsk_ptr->ib_status.bypass_a_en) {
			(void) fprintf(stdout,
				MSGSTR(-1,
				" (Bypassed:A)   "));
	} else if (ib_present_chk(l_state, 1) &&
		dsk_ptr->ib_status.bypass_b_en) {
		(void) fprintf(stdout,
			MSGSTR(-1,
				" (Bypassed:B)   "));
	}
}

/*
 * Display condensed status for each disk.
 */
static	void
display_disk_msg(struct l_disk_state_struct *dsk_ptr,
	struct l_state_struct *l_state, int front_flag)
{
int	loop_flag = 0;
int	a_and_b = 0;
int	state_a = 0, state_b = 0;

	if (dsk_ptr->ib_status.code == S_NOT_INSTALLED) {
		(void) fprintf(stdout,
			MSGSTR(-1,
						"Not Installed "));
		if (dsk_ptr->ib_status.fault ||
			dsk_ptr->ib_status.fault_req) {
			(void) fprintf(stdout,
				MSGSTR(-1,
						"(Faulted)           "));
		} else if (dsk_ptr->ib_status.ident ||
			dsk_ptr->ib_status.rdy_to_ins ||
			dsk_ptr->ib_status.rmv) {
			(void) fprintf(stdout,
				MSGSTR(-1,
						"(LED Blinking)      "));
		} else {
			(void) fprintf(stdout,
						"                    ");
		}
	} else if (dsk_ptr->ib_status.dev_off) {
		(void) fprintf(stdout, MSGSTR(-1, "Off "));
		if (dsk_ptr->ib_status.fault || dsk_ptr->ib_status.fault_req) {
			(void) fprintf(stdout,
				MSGSTR(-1,
					"(Faulted)                     "));
		} else if (dsk_ptr->ib_status.bypass_a_en &&
			dsk_ptr->ib_status.bypass_b_en) {
			(void) fprintf(stdout,
				MSGSTR(-1,
					"(Bypassed:AB)                 "));
		} else if (dsk_ptr->ib_status.bypass_a_en) {
			(void) fprintf(stdout,
				MSGSTR(-1,
					"(Bypassed:port A)             "));
		} else if (dsk_ptr->ib_status.bypass_b_en) {
			(void) fprintf(stdout,
				MSGSTR(-1,
					"(Bypassed:port B)             "));
		} else {
			(void) fprintf(stdout,
					"                              ");
		}
	} else {
		(void) fprintf(stdout, MSGSTR(-1, "On"));

		if (dsk_ptr->ib_status.fault || dsk_ptr->ib_status.fault_req) {
			(void) fprintf(stdout,
				MSGSTR(-1,
						" (Faulted)      "));
		} else if (dsk_ptr->ib_status.bypass_a_en &&
			dsk_ptr->ib_status.bypass_b_en) {
			(void) fprintf(stdout,
				MSGSTR(-1,
						" (Bypassed:AB)  "));
		} else {
			state_a = dsk_ptr->d_state_flags[PORT_A];
			state_b = dsk_ptr->d_state_flags[PORT_B];
			a_and_b = state_a & state_b;

			if (dsk_ptr->l_state_flag & L_NO_LOOP) {
				(void) fprintf(stdout,
				MSGSTR(-1,
					" (Loop not accessible)"));
				loop_flag = 1;
			} else if (dsk_ptr->l_state_flag & L_INVALID_WWN) {
				(void) fprintf(stdout,
				MSGSTR(-1,
					" (Invalid WWN)  "));
			} else if (dsk_ptr->l_state_flag & L_INVALID_MAP) {
				(void) fprintf(stdout,
				MSGSTR(-1,
					" (Bad Loop map) "));
			} else if (dsk_ptr->l_state_flag & L_NO_PATH_FOUND) {
				(void) fprintf(stdout,
				MSGSTR(-1,
					" (No path found)"));
			} else if (a_and_b) {
				print_individual_state(dsk_ptr, l_state,
					a_and_b, PORT_A_B);
			} else if (state_a && (!state_b)) {
				print_individual_state(dsk_ptr,
					l_state, state_a, PORT_A);
			} else if ((!state_a) && state_b) {
				print_individual_state(dsk_ptr,
					l_state, state_b, PORT_B);
			} else if (state_a || state_b) {
				/* NOTE: Double state - should do 2 lines. */
				print_individual_state(dsk_ptr, l_state,
					state_a | state_b, PORT_A_B);
			} else {
				(void) fprintf(stdout,
				MSGSTR(-1,
						" (O.K.)         "));
			}
		}
		if (loop_flag) {
			(void) fprintf(stdout, "          ");
		} else if (strlen(dsk_ptr->node_wwn_s)) {
			(void) fprintf(stdout,
			MSGSTR(-1, "%s"), dsk_ptr->node_wwn_s);
		} else {
			(void) fprintf(stdout, "                 ");
		}
	}
	if (front_flag) {
		(void) fprintf(stdout, "    ");
	}
}

/*
 *	PHOTON DISPLAY function
 *
 */
static void
pho_display_config(char *path_phys)
{
L_state		l_state;
int		i;
int		elem_index;


	/* Get global status */
	if (l_get_status(path_phys, &l_state,
	    (Options & PVERBOSE))) {
	    L_ERR_PRINT;
	    exit(-1);
	}

	/*
	 * Look for abnormal status.
	 */
	if (l_state.ib_tbl.p2_s.ui.ab_cond) {
		abnormal_condition_display(&l_state);
	}

	(void) fprintf(stdout,
		MSGSTR(-1, "                                 DISK STATUS \n"
		"SLOT   FRONT DISKS       (Node WWN)         "
		" REAR DISKS        (Node WWN)\n"));
	/*
	 * Print the status for each disk
	 */
	for (i = 0; i < (int)l_state.total_num_drv/2; i++) {
		(void) fprintf(stdout, "%-2d     ", i);
		display_disk_msg(&l_state.drv_front[i], &l_state, 1);
		display_disk_msg(&l_state.drv_rear[i], &l_state, 0);
		(void) fprintf(stdout, "\n");
	}



	/*
	 * Display the subsystem status.
	 */
	(void) fprintf(stdout,
		MSGSTR(-1, "                                SUBSYSTEM STATUS\n"
			"FW Revision:"));
	for (i = 0; i < sizeof (l_state.ib_tbl.config.prod_revision); i++) {
		(void) fprintf(stdout, "%c",
			l_state.ib_tbl.config.prod_revision[i]);
	}
	(void) fprintf(stdout, MSGSTR(-1, "   Box ID:%d"),
		l_state.ib_tbl.box_id);
	(void) fprintf(stdout, MSGSTR(-1, "   Node WWN:"));
	for (i = 0; i < 8; i++) {
		(void) fprintf(stdout,
		"%1.2x", l_state.ib_tbl.config.enc_node_wwn[i]);
	}
	/* Make sure NULL terminated  although it is supposed to be */
	if (strlen((const char *)l_state.ib_tbl.enclosure_name) <=
		sizeof (l_state.ib_tbl.enclosure_name)) {
		(void) fprintf(stdout, MSGSTR(-1, "   Enclosure Name:%s\n"),
			l_state.ib_tbl.enclosure_name);
	}

	/*
	 *
	 */
	elem_index = 0;
	/* Get and print CONTROLLER messages */
	for (i = 0; i < (int)l_state.ib_tbl.config.enc_num_elem; i++) {
	    elem_index++;		/* skip global */
	    switch (l_state.ib_tbl.config.type_hdr[i].type) {
		case ELM_TYP_PS:
			ps_messages(&l_state, i, elem_index);
			break;
		case ELM_TYP_FT:
			fan_messages(&l_state, i, elem_index);
			break;
		case ELM_TYP_BP:
			back_plane_messages(&l_state, i, elem_index);
			break;
		case ELM_TYP_IB:
			ctlr_messages(&l_state, i, elem_index);
			break;
		case ELM_TYP_LN:
			/*
			 * NOTE: I just use the Photon's message
			 * string here and don't look at the
			 * language code. The string includes
			 * the language name.
			 */
			if (l_state.ib_tbl.config.type_hdr[i].text_len != 0) {
				(void) fprintf(stdout, "%s\t",
				l_state.ib_tbl.config.text[i]);
			}
			break;
		case ELM_TYP_LO:	/* Loop configuration */
			loop_messages(&l_state, i, elem_index);
			break;
		case ELM_TYP_MB:	/* Loop configuration */
			mb_messages(&l_state, i, elem_index);
			break;

	    }
		/*
		 * Calculate the index to each element.
		 */
		elem_index += l_state.ib_tbl.config.type_hdr[i].num;
	}
/*
	if (Options & OPTION_V) {
		adm_display_verbose(l_state);
	}
*/
	(void) fprintf(stdout, "\n");
}

/*
 * Change the FPM (Front Panel Module) password of the
 * subsystem associated with the IB addressed by the
 * enclosure or pathname to name.
 *
 */
static void
intfix(void)
{
	if (termio_fd) {
		termios.c_lflag |= ECHO;
		ioctl(termio_fd, TCSETS, &termios);
	}
	exit(SIGINT);
}

static void
up_password(char **argv)
{
int		path_index = 0;
char		password[1024];
char		input[1024];
int		i, j, matched, equal;
L_inquiry	inq;
void		(*sig)();
char		*path_phys;
Path_struct	*path_struct;


	if ((termio_fd = open("/dev/tty", O_RDONLY)) == NULL) {
		(void) fprintf(stderr,
		MSGSTR(-1, "Error: tty open failed.\n"));
		exit(-1);
	}
	ioctl(termio_fd, TCGETS, &termios);
	sig = sigset(SIGINT, (void (*)())intfix);
	/*
	 * Make sure path valid and is to a PHO
	 * before bothering operator.
	 */
	if ((path_phys = l_convert_name(argv[path_index],
		&path_struct, Options & PVERBOSE)) == NULL) {
		L_ERR_PRINT;
		exit(-1);
	}
	if (l_get_inquiry(path_phys, &inq)) {
		L_ERR_PRINT;
		exit(-1);
	}
	if ((strstr((char *)inq.inq_pid, ENCLOSURE_PROD_ID) == 0) &&
			(!(strncmp((char *)inq.inq_vid, "SUN     ",
			sizeof (inq.inq_vid)) &&
			(inq.inq_dtype == DTYPE_ESI)))) {
		/*
		 * Again this is like the ssaadm code in that the name
		 * is still not defined before this code must be released.
		 */
		(void) fprintf(stderr,
		MSGSTR(-1, "Error: Enclosure is not a %s\n"),
			ENCLOSURE_PROD_ID);
		exit(-1);
	}
	(void) fprintf(stdout,
	MSGSTR(-1, "Changing FPM password for subsystem %s\n"),
	argv[path_index]);

	equal = 0;
	while (!equal) {
		memset(input, 0, sizeof (input));
		memset(password, 0, sizeof (password));
		(void) fprintf(stdout,
		MSGSTR(-1, "New password: "));

		termios.c_lflag &= ~ECHO;
		ioctl(termio_fd, TCSETS, &termios);

		(void) gets(input);
		(void) fprintf(stdout,
		MSGSTR(-1, "\nRe-enter new password: "));
		(void) gets(password);
		termios.c_lflag |= ECHO;
		ioctl(termio_fd, TCSETS, &termios);
		for (i = 0; input[i]; i++) {
			if (!isdigit(input[i])) {
				(void) fprintf(stderr,
			MSGSTR(-1, "\nError: Invalid password."
			" The password"
			" must be 4 decimal-digit characters.\n"));
				exit(-1);
			}
		}
		if (i && (i != 4)) {
			(void) fprintf(stderr,
			MSGSTR(-1, "\nError: Invalid password."
			" The password"
			" must be 4 decimal-digit characters.\n"));
			exit(-1);
		}
		for (j = 0; password[j]; j++) {
			if (!isdigit(password[j])) {
				(void) fprintf(stderr,
			MSGSTR(-1, "\nError: Invalid password."
			" The password"
			" must be 4 decimal-digit characters.\n"));
				exit(-1);
			}
		}
		if (i != j) {
			matched = -1;
		} else for (i = matched = 0; password[i]; i++) {
			if (password[i] == input[i]) {
				matched++;
			}
		}
		if ((matched != -1) && (matched == i)) {
			equal = 1;
		} else {
			(void) fprintf(stdout,
			MSGSTR(-1, "\npassword: They don't match;"
			" try again.\n"));
		}
	}
	(void) fprintf(stdout, "\n");
	sscanf(input, "%s", password);
	(void) signal(SIGINT, sig);	/* restore signal handler */

	/*  Send new password to IB */
	if (l_new_password(path_phys, input, (Options & PVERBOSE))) {
		L_ERR_PRINT;
		exit(-1);
	}
}

/*
 * Update the enclosures logical name.
 */
static void
up_encl_name(char **argv, int argc)
{
int		path_index = 0;
char		name[1024];
int		i, rval, al_pa;
L_inquiry	inq;
Box_list	*b_list = NULL;
u_char		node_wwn[WWN_SIZE];
u_char		port_wwn[WWN_SIZE];
char		wwn1[(WWN_SIZE*2)+1];
char		*path_phys;
Path_struct	*path_struct;

	memset(name, 0, sizeof (name));
	memset(&inq, 0, sizeof (inq));
	sscanf(argv[path_index++], "%s", name);
	for (i = 0; name[i]; i++) {
		if ((!isalnum(name[i]) &&
			((name[i] != '#') &&
			(name[i] != '-') &&
			(name[i] != '_') &&
			(name[i] != '.'))) || i >= 16) {
			(void) fprintf(stderr,
			MSGSTR(-1, "Error: Invalid enclosure name.\n"
			"Usage: %s [-v] subcommand {a name consisting of"
			" 1-16 characters}"
			" {enclosure... | pathname...}\n"), whoami);
			exit(-1);
		}
	}

	if (((Options & PVERBOSE) && (argc != 5)) ||
		(!(Options & PVERBOSE) && (argc != 4))) {
		(void) fprintf(stderr,
		MSGSTR(-1, "Error: Incorrect number of arguments.\n"));
		(void) fprintf(stderr,  MSGSTR(-1,
		"Usage: %s [-v] subcommand {a name consisting of"
		" 1-16 alphanumeric characters}"
		" {enclosure... | pathname...}\n"), whoami);
		exit(-1);
	}

	if ((path_phys = l_convert_name(argv[path_index],
		&path_struct, Options & PVERBOSE)) == NULL) {
		L_ERR_PRINT;
		exit(-1);
	}
	/*
	 * Make sure we are talking to an IB.
	 */
	if (l_get_inquiry(path_phys, &inq)) {
		L_ERR_PRINT;
		exit(-1);
	}
	if ((strstr((char *)inq.inq_pid, ENCLOSURE_PROD_ID) == 0) &&
			(!(strncmp((char *)inq.inq_vid, "SUN     ",
			sizeof (inq.inq_vid)) &&
			(inq.inq_dtype == DTYPE_ESI)))) {
		/*
		 * Again this is like the ssaadm code in that the name
		 * is still not defined before this code must be released.
		 */
		(void) fprintf(stderr,
		MSGSTR(-1, "Error: Pathname does not point to a %s"
		" enclosure\n"), ENCLOSURE_PROD_NAME);
		exit(-1);
	}

	if (l_get_wwn(path_phys, port_wwn, node_wwn, &al_pa,
		Options & PVERBOSE)) {
		L_ERR_PRINT;
		exit(-1);
	}

	for (i = 0; i < WWN_SIZE; i++) {
		sprintf(&wwn1[i << 1], "%02x", node_wwn[i]);
	}
	if (l_get_box_list(&b_list, Options & PVERBOSE)) {
		L_ERR_PRINT;
		exit(-1);
	}
	if (b_list == NULL) {
		(void) fprintf(stdout,
			MSGSTR(-1, "No %s enclosures found "
			"in /dev/es\n"), ENCLOSURE_PROD_NAME);
		exit(-1);
	} else if (l_duplicate_names(b_list, wwn1, name,
		Options & PVERBOSE)) {
		(void) fprintf(stdout,
		MSGSTR(-1, "Warning: The name you selected, %s,"
		" is already being used.\n"
		"Please choose a unique name.\n"
		"You can use the \"probe\" subcommand to"
		" see all of the enclosure names.\n"),
		name);
		exit(-1);
	}
	l_free_box_list(&b_list);

	/*  Send new name to IB */
	if (rval = l_new_name(path_phys, name, (Options & PVERBOSE))) {
		if (rval == L_WARNING) {
			L_WARNING_PRINT;
		} else {
			L_ERR_PRINT;
		}
	    exit(-1);
	}
	if (Options & PVERBOSE) {
		(void) fprintf(stdout,
			MSGSTR(-1, "The enclosure has been renamed to %s\n"),
			name);
	}
}


static void
adm_power_off(char **argv, int argc, int off_flag)
{
int		path_index = 0;
L_inquiry	inq;
char		*path_phys;
Path_struct	*path_struct;

	while (argv[path_index] != NULL) {
		if ((path_phys = l_convert_name(argv[path_index],
			&path_struct, Options & PVERBOSE)) == NULL) {
			/*
			 * In case we did not find the device
			 * in the /devices directory.
			 *
			 * Only valid for path names like box,f1
			 */
			if (path_struct->ib_path_flag) {
				path_phys = path_struct->p_physical_path;
			} else {
				L_ERR_PRINT;
				path_index++; continue;
			}
		}
		if (path_struct->ib_path_flag) {
			/*
			 * We are addressing a disk using a path
			 * format type box,f1.
			 */
			if (l_dev_pwr_up_down(argv[path_index], path_phys,
				path_struct, off_flag, Options & PVERBOSE)) {
			    L_ERR_PRINT;
			}
			path_index++; continue;
		}

		if (l_get_inquiry(path_phys, &inq)) {
			L_ERR_PRINT;
			path_index++; continue;
		}
		if ((strstr((char *)inq.inq_pid, ENCLOSURE_PROD_ID) != 0) ||
			(strncmp((char *)inq.inq_vid, "SUN     ",
			sizeof (inq.inq_vid)) &&
			(inq.inq_dtype == DTYPE_ESI))) {
		/*
		 * Again this is like the ssaadm code in that the name
		 * is still not defined before this code must be released.
		 */
			if (l_pho_pwr_up_down(argv[path_index], path_phys,
				off_flag, Options & PVERBOSE)) {
			    L_ERR_PRINT;
			}
		} else if (inq.inq_dtype == DTYPE_DIRECT) {
			if (l_dev_pwr_up_down(argv[path_index], path_phys,
				path_struct, off_flag, Options & PVERBOSE)) {
			    L_ERR_PRINT;
			}
		} else {
			power_off(argv, argc);
		}
		path_index++;
	}
}

/*
 * The led_request subcommand requests the subsystem to display
 * the current state or turn off, on,
 * or blink the yellow LED associated with the disk
 * specified by the enclosure or pathname.
 */
static void
adm_led(char **argv, int led_action)
{
int		path_index = 0;
sf_al_map_t	map;
L_inquiry	inq;
Dev_elem_st	status;
char		*path_phys;
Path_struct	*path_struct;

	while (argv[path_index] != NULL) {
		if ((path_phys = l_convert_name(argv[path_index],
			&path_struct, Options & PVERBOSE)) == NULL) {
			/* Make sure we have a device path. */
			if (path_struct->ib_path_flag) {
				path_phys = path_struct->p_physical_path;
			} else {
				L_ERR_PRINT;
				exit(-1);
			}
		}
		if (!path_struct->ib_path_flag) {
			if (l_get_inquiry(path_phys, &inq)) {
				L_ERR_PRINT;
				exit(-1);
			}
			if (inq.inq_dtype != DTYPE_DIRECT) {
				(void) fprintf(stdout,
				MSGSTR(-1,
				"Error: pathname must be to a disk device\n"
				" %s\n"), argv[path_index]);
				exit(-1);
			}
		}

		/*
		 * See if we are in fact talking to a loop or not.
		 */
		if (!l_get_dev_map(path_phys, &map,
			(Options & PVERBOSE)) != 0) {

			if (led_action == L_LED_ON) {
				(void) fprintf(stdout,
				MSGSTR(-1,
				"The led_on functionality is not applicable "
				"to this subsystem.\n"));
				exit(-1);
			}

			if (l_led(path_struct, led_action, &status,
				(Options & PVERBOSE))) {
				L_ERR_PRINT;
				exit(-1);
			}
			switch (led_action) {
			case L_LED_STATUS:
			    if (status.fault || status.fault_req) {
				if (!path_struct->slot_valid) {
					(void) fprintf(stdout,
					MSGSTR(-1, "LED state is ON for "
					"device:\n  %s\n"), path_phys);
				} else {
					(void) fprintf(stdout,
					MSGSTR(-1, "LED state is ON for "
					"device in location: %s,slot %d\n"),
					(path_struct->f_flag) ?
					"front" : "rear",
					path_struct->slot);
				}
			    } else if (status.ident ||
				status.rdy_to_ins || status.rmv) {
				if (!path_struct->slot_valid) {
					(void) fprintf(stdout,
					MSGSTR(-1,
					"LED state is BLINKING for "
					"device:\n  %s\n"), path_phys);
				} else {
					(void) fprintf(stdout,
					MSGSTR(-1,
					"LED state is BLINKING for "
					"device in location: %s,slot %d\n"),
					(path_struct->f_flag) ?
					"front" : "rear",
					path_struct->slot);
				}
			    } else {
				if (!path_struct->slot_valid) {
					(void) fprintf(stdout,
					MSGSTR(-1, "LED state is OFF for "
					"device:\n  %s\n"), path_phys);
				} else {
					(void) fprintf(stdout,
					MSGSTR(-1, "LED state is OFF for "
					"device in location: %s,slot %d\n"),
					(path_struct->f_flag) ?
					"front" : "rear",
					path_struct->slot);
				}
			    }
			    break;
			}
		} else {
			Options &= ~(OPTION_E | OPTION_D);
			switch (led_action) {
			case L_LED_STATUS:
				break;
			case L_LED_RQST_IDENTIFY:
				(void) fprintf(stdout,
				MSGSTR(-1, "Blinking is not supported"
				" by this subsystem.\n"));
				exit(-1);
				/*NOTREACHED*/
			case L_LED_ON:
				Options |= OPTION_E;
				break;
			case L_LED_OFF:
				Options |= OPTION_D;
				break;
			}
			led(path_phys, Options);
		}
		path_index++;
	}
}


/*
 * Dump information
 */
static void
dump(char **argv)
{
u_char		*buf;
int		path_index = 0;
L_inquiry	inq;
char		hdr_buf[MAXNAMELEN];
Rec_diag_hdr	*hdr, *hdr_ptr;
char		*path_phys;
Path_struct	*path_struct;

	/*
	 * get big buffer
	 */
	if ((hdr = (struct rec_diag_hdr *)zalloc(MAX_REC_DIAG_LENGTH)) ==
		NULL) {
		(void) fprintf(stderr,
			MSGSTR(-1, "Error: unable to zalloc more space.\n"));
		exit(-1);
	}
	buf = (u_char *)hdr;

	while (argv[path_index] != NULL) {
		if ((path_phys = l_convert_name(argv[path_index],
			&path_struct, Options & PVERBOSE)) == NULL) {
			L_ERR_PRINT;
			exit(-1);
		}
		if (l_get_inquiry(path_phys, &inq)) {
			L_ERR_PRINT;
		} else {
			p_dump("INQUIRY data:   ",
			(u_char *)&inq, 5 + inq.inq_len, HEX_ASCII);
		}

		(void) memset(buf, 0, MAX_REC_DIAG_LENGTH);
		if (l_get_envsen(path_phys, buf, MAX_REC_DIAG_LENGTH,
			(Options & PVERBOSE))) {
		    L_ERR_PRINT;
		    exit(-1);
		}
		(void) fprintf(stdout,
			MSGSTR(-1, "\t\tEnvironmental Sense Information\n"));

		/*
		 * Dump all pages.
		 */
		hdr_ptr = hdr;

		while (hdr_ptr->page_len != 0) {
			(void) sprintf(hdr_buf, "Page %d:   ",
				hdr_ptr->page_code);
			p_dump(hdr_buf, (u_char *)hdr_ptr,
				HEADER_LEN + hdr_ptr->page_len, HEX_ASCII);
			hdr_ptr += ((HEADER_LEN + hdr_ptr->page_len) /
				sizeof (struct	rec_diag_hdr));
		}
		path_index++;
	}
	free(buf);
}

static void
adm_display_err(char *path_phys, int dtype, int verbose_flag)
{
int		i, inst, al_pa, len, socal_inst;
char		*sf_path, socal_path[MAXPATHLEN], ses_path[MAXPATHLEN];
struct		stat sbuf;
kstat_ctl_t	*kc;
kstat_t		*sf_ks, *socal_ks;
struct		sf_stats sf_stats;
struct socal_stats socal_stats;
sf_al_map_t	map;
u_char		node_wwn[WWN_SIZE];
u_char		port_wwn[WWN_SIZE];

	if ((kc = kstat_open()) == (kstat_ctl_t *)NULL) {
		sprintf(l_error_msg, "cannot open kstat\n");
		l_error_msg_ptr = &l_error_msg[0];
		L_ERR_PRINT;
		return;
	}

	if (l_get_dev_map(path_phys, &map, verbose_flag)) {
		sprintf(l_error_msg, "could not get map for %s\n",
				path_phys);
		l_error_msg_ptr = &l_error_msg[0];
		L_ERR_PRINT;
		return;
	}

	if (dtype == DTYPE_DIRECT) { /* info for device */
		if (!strstr(path_phys, "socal")) {
			sprintf(l_error_msg,
				MSGSTR(-1,
				"Driver \"socal\" not found %s\n"),
				path_phys);
			l_error_msg_ptr = &l_error_msg[0];
			L_ERR_PRINT;
			return;
		}
		if (l_get_ses_path(path_phys, ses_path, &map,
				verbose_flag)) {
			sprintf(l_error_msg,
				MSGSTR(-1, "could not get map for"
					" %s\n"), path_phys);
			l_error_msg_ptr = &l_error_msg[0];
			L_ERR_PRINT;
			return;
		}
	} else {
		strcpy(ses_path, path_phys);
	}

	if (!(sf_path = strrchr(ses_path, '/'))) {
		sprintf(l_error_msg, "invalid path %s\n", ses_path);
		l_error_msg_ptr = &l_error_msg[0];
		L_ERR_PRINT;
		return;
	}
	sf_path = strrchr(ses_path, '/');
	*sf_path = '\0';
	len = strlen(ses_path) - strlen(strrchr(ses_path, '/'));
	strncpy(socal_path, ses_path, len);
	socal_path[len] = '\0';
	strcat(ses_path, ":devctl");
	strcat(socal_path, ":0");

	if (stat(ses_path, &sbuf) < 0) {
		sprintf(l_error_msg, "could not stat %s\n", path_phys);
		l_error_msg_ptr = &l_error_msg[0];
		L_ERR_PRINT;
		return;
	}

	inst = minor(sbuf.st_rdev);

	if (stat(socal_path, &sbuf) < 0) {
		sprintf(l_error_msg, "could not stat %s\n", path_phys);
		l_error_msg_ptr = &l_error_msg[0];
		L_ERR_PRINT;
		return;
	}
	socal_inst = minor(sbuf.st_rdev)/2;


	if (!(sf_ks = kstat_lookup(kc, "sf", inst, "statistics"))) {
		sprintf(l_error_msg, "could not lookup sf%d\n", inst);
		l_error_msg_ptr = &l_error_msg[0];
		L_ERR_PRINT;
		return;
	}

	if (!(socal_ks = kstat_lookup(kc, "socal", socal_inst,
					"statistics"))) {
		sprintf(l_error_msg, "could not lookup socal%d\n",
						socal_inst);
		l_error_msg_ptr = &l_error_msg[0];
		L_ERR_PRINT;
		return;
	}

	if (kstat_read(kc, sf_ks, &sf_stats) < 0) {
		sprintf(l_error_msg, "could not read sf%d\n", inst);
		l_error_msg_ptr = &l_error_msg[0];
		L_ERR_PRINT;
		return;
	}

	if (kstat_read(kc, socal_ks, &socal_stats) < 0) {
		sprintf(l_error_msg, "could not read socal%d\n", inst);
		l_error_msg_ptr = &l_error_msg[0];
		L_ERR_PRINT;
		return;
	}

	(void) fprintf(stdout, MSGSTR(-1,
		"\tInformation for FC Loop on port %d of"
		" FC100/S Host Adapter\n\tat path: %s\n"),
		socal_inst, socal_path);

	(void) fprintf(stdout, "Version Resets  Req_Q_Intrpts  Qfulls"
		" Requests Sol_Resps Unsol_Resps Lips\n");

	(void) fprintf(stdout, "%4d%8d%11d%13d%8d%9d%11d%8d\n",
			socal_stats.version,
			socal_stats.resets,
			socal_stats.reqq_intrs,
			socal_stats.qfulls,
			socal_stats.pstats[inst].requests,
			socal_stats.pstats[inst].sol_resps,
			socal_stats.pstats[inst].unsol_resps,
			socal_stats.pstats[inst].lips);

	(void) fprintf(stdout, "Els_sent  Els_rcvd  Abts"
		"       Abts_ok Offlines Onlines Online_loops\n");

	(void) fprintf(stdout, "%4d%12d%7d%12d%9d%8d%11d\n",
			socal_stats.pstats[inst].els_sent,
			socal_stats.pstats[inst].els_rcvd,
			socal_stats.pstats[inst].abts,
			socal_stats.pstats[inst].abts_ok,
			socal_stats.pstats[inst].offlines,
			socal_stats.pstats[inst].onlines,
			socal_stats.pstats[inst].online_loops);

	(void) fprintf(stdout, "\t\tInformation from sf driver:\n");

	(void) fprintf(stdout, "Version  Lip_count  Lip_fail"
		" Alloc_fail  #_cmds "
		"Throttle_limit  Pool_size\n");

	(void) fprintf(stdout, "%4d%9d%12d%11d%10d%11d%12d\n",
			sf_stats.version,
			sf_stats.lip_count,
			sf_stats.lip_failures,
			sf_stats.cralloc_failures,
			sf_stats.ncmds,
			sf_stats.throttle_limit,
			sf_stats.cr_pool_size);

	(void) fprintf(stdout, "\t\tTARGET ERROR INFORMATION:\n");
	(void) fprintf(stdout, "AL_PA  Els_fail Timouts Abts_fail"
		" Tsk_m_fail "
		" Data_ro_mis Dl_len_mis Logouts\n");

	if (dtype == DTYPE_DIRECT) {
		if (l_get_wwn(path_phys, port_wwn, node_wwn, &al_pa,
			Options & PVERBOSE)) {
			L_ERR_PRINT;
			return;
		}
		for (i = 0; i < map.sf_count; i++) {
			if (map.sf_addr_pair[i].sf_al_pa ==
					al_pa) {
		(void) fprintf(stdout, "%3x%10d%8d%10d%11d%13d%11d%9d\n",
				map.sf_addr_pair[i].sf_al_pa,
				sf_stats.tstats[i].els_failures,
				sf_stats.tstats[i].timeouts,
				sf_stats.tstats[i].abts_failures,
				sf_stats.tstats[i].task_mgmt_failures,
				sf_stats.tstats[i].data_ro_mismatches,
				sf_stats.tstats[i].dl_len_mismatches,
				sf_stats.tstats[i].logouts_recvd);
				break;
			}
		}
		if (i >= map.sf_count) {
			sprintf(l_error_msg, "cannot find device %s"
				" in map\n", path_phys);
			l_error_msg_ptr = &l_error_msg[0];
			L_ERR_PRINT;
		}
		return;
	}

	for (i = 0; i < map.sf_count; i++) {
		(void) fprintf(stdout, "%3x%10d%8d%10d%11d%13d%11d%9d\n",
			map.sf_addr_pair[i].sf_al_pa,
			sf_stats.tstats[i].els_failures,
			sf_stats.tstats[i].timeouts,
			sf_stats.tstats[i].abts_failures,
			sf_stats.tstats[i].task_mgmt_failures,
			sf_stats.tstats[i].data_ro_mismatches,
			sf_stats.tstats[i].dl_len_mismatches,
			sf_stats.tstats[i].logouts_recvd);
	}

	(void) kstat_close(kc);
}

/*
 * Display extended information.
 */
#ifndef	MODEPAGE_CACHING
#undef	MODEPAGE_CACHING
#define	MODEPAGE_CACHING	0x08
#endif

/*ARGSUSED*/
static void
adm_display_verbose_disk(char *path, int verbose)
{
u_char		*pg_buf;
Mode_header_10	*mode_header_ptr;
Mp_01		*pg1_buf;
Mp_04		*pg4_buf;
struct my_mode_caching	*pg8_buf;
struct mode_page *pg_hdr;
int		offset, hdr_printed = 0;

	if (l_get_mode_pg(path, &pg_buf, verbose) == 0) {

		mode_header_ptr = (struct mode_header_10_struct *)(int)pg_buf;
		pg_hdr = ((struct mode_page *)((int)pg_buf +
		    (u_char)sizeof (struct mode_header_10_struct) +
		    (u_char *)(mode_header_ptr->bdesc_length)));
		offset = sizeof (struct mode_header_10_struct) +
		    mode_header_ptr->bdesc_length;
		while (offset < (mode_header_ptr->length +
			sizeof (mode_header_ptr->length))) {
			switch (pg_hdr->code) {
				case 0x01:
				pg1_buf = (struct mode_page_01_struct *)
					(int)pg_hdr;
				P_DPRINTF("  adm_display_verbose_disk:"
					"Mode Sense page 1 found.\n");
				if (hdr_printed++ == 0) {
					(void) fprintf(stdout,
						MSGSTR(-1,
						"  Mode Sense data:\n"));
				}
				(void) fprintf(stdout,
					MSGSTR(-1,
					"    AWRE:\t\t\t%d\n"
					"    ARRE:\t\t\t%d\n"
					"    Read Retry Count:\t\t"
					"%d\n"
					"    Write Retry Count:\t\t"
					"%d\n"),
					pg1_buf->awre,
					pg1_buf->arre,
					pg1_buf->read_retry_count,
					pg1_buf->write_retry_count);
				break;
				case MODEPAGE_GEOMETRY:
				pg4_buf = (struct mode_page_04_struct *)
					(int)pg_hdr;
				P_DPRINTF("  adm_display_verbose_disk:"
					"Mode Sense page 4 found.\n");
				if (hdr_printed++ == 0) {
					(void) fprintf(stdout,
						MSGSTR(-1,
						"  Mode Sense data:\n"));
				}
				if (pg4_buf->rpm) {
					(void) fprintf(stdout,
						MSGSTR(-1,
						"    Medium rotation rate:\t"
						"%d RPM\n"), pg4_buf->rpm);
				}
				break;
				case MODEPAGE_CACHING:
				pg8_buf = (struct my_mode_caching *)(int)pg_hdr;
				P_DPRINTF("  adm_display_verbose_disk:"
					"Mode Sense page 8 found.\n");
				if (hdr_printed++ == 0) {
					(void) fprintf(stdout,
						MSGSTR(-1,
						"  Mode Sense data:\n"));
				}
				if (pg8_buf->wce) {
					(void) fprintf(stdout,
						MSGSTR(-1,
						"    Write cache:\t\t"
						"\tEnabled\n"));
				}
				if (pg8_buf->rcd == 0) {
					(void) fprintf(stdout,
						MSGSTR(-1,
						"    Read cache:\t\t\t"
						"Enabled\n"));
					(void) fprintf(stdout,
						MSGSTR(-1,
						"    Minimum prefetch:"
						"\t\t0x%x\n"
						"    Maximum prefetch:"
						"\t\t0x%x\n"),
						pg8_buf->min_prefetch,
						pg8_buf->max_prefetch);
				}
				break;
			}
			offset += pg_hdr->length + sizeof (struct mode_page);
			pg_hdr = ((struct mode_page *)((int)pg_buf +
				(u_char)offset));
		}





	} else if (getenv("_LUX_P_DEBUG") != NULL) {
			L_ERR_PRINT;
	}
}

static void
dump_map(char **argv)
{
int	path_index = 0;
sf_al_map_t	map;
int	i;
char		*path_phys;
Path_struct	*path_struct;
int		limited_map_flag = 0;
struct lilpmap	limited_map;


	while (argv[path_index] != NULL) {
		if ((path_phys = l_convert_name(argv[path_index],
			&path_struct, Options & PVERBOSE)) == NULL) {
			L_ERR_PRINT;
			exit(-1);
		}
		if (l_get_dev_map(path_phys, &map, (Options & PVERBOSE))) {
			/*
			 * This did not work so try the FCIO_GETMAP
			 * type ioctl.
			 */
			p_error_msg_ptr = l_error_msg_ptr = NULL;
			if (l_get_limited_map(path_phys,
				&limited_map, (Options & PVERBOSE))) {
				L_ERR_PRINT;
				exit(-1);
			}
			limited_map_flag++;
		}

		if (limited_map_flag) {
			(void) fprintf(stdout,
				MSGSTR(-1,
				"Host Adapter AL_PA: %x\n"),
				limited_map.lilp_myalpa);

			(void) fprintf(stdout,
				MSGSTR(-1,
				"Pos AL_PA\n"));
			for (i = 0; i < (u_int)limited_map.lilp_length; i++) {
				(void) fprintf(stdout,
					"%-3d   %-2x\n",
					i, limited_map.lilp_list[i]);
			}
		} else {

		    (void) fprintf(stdout,
			MSGSTR(-1,
			"Pos AL_PA ID Hard_Addr "
			"Port WWN         Node WWN         Type\n"));

		    for (i = 0; i < map.sf_count; i++) {
			(void) fprintf(stdout,
			"%-3d   %-2x  %-2x    %-2x     "
			"%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x"
			" %1.2x%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x ",
			i, map.sf_addr_pair[i].sf_al_pa,
			l_sf_alpa_to_switch[map.sf_addr_pair[i].sf_al_pa],
			map.sf_addr_pair[i].sf_hard_address,
			map.sf_addr_pair[i].sf_port_wwn[0],
			map.sf_addr_pair[i].sf_port_wwn[1],
			map.sf_addr_pair[i].sf_port_wwn[2],
			map.sf_addr_pair[i].sf_port_wwn[3],
			map.sf_addr_pair[i].sf_port_wwn[4],
			map.sf_addr_pair[i].sf_port_wwn[5],
			map.sf_addr_pair[i].sf_port_wwn[6],
			map.sf_addr_pair[i].sf_port_wwn[7],

			map.sf_addr_pair[i].sf_node_wwn[0],
			map.sf_addr_pair[i].sf_node_wwn[1],
			map.sf_addr_pair[i].sf_node_wwn[2],
			map.sf_addr_pair[i].sf_node_wwn[3],
			map.sf_addr_pair[i].sf_node_wwn[4],
			map.sf_addr_pair[i].sf_node_wwn[5],
			map.sf_addr_pair[i].sf_node_wwn[6],
			map.sf_addr_pair[i].sf_node_wwn[7]);

			if (map.sf_addr_pair[i].sf_inq_dtype < 0x10) {
				(void) fprintf(stdout, "0x%-2x (%s)\n",
				map.sf_addr_pair[i].sf_inq_dtype,
				dtype[map.sf_addr_pair[i].sf_inq_dtype]);
			} else if (map.sf_addr_pair[i].sf_inq_dtype < 0x1f) {
				(void) fprintf(stdout, "0x%-2x %s\n",
					map.sf_addr_pair[i].sf_inq_dtype,
					"(Reserved)");
			} else {
				(void) fprintf(stdout, "0x%-2x %s\n",
					map.sf_addr_pair[i].sf_inq_dtype,
					"(Unknown Type)");
			}
		    }
		}
		limited_map_flag = 0;
		path_index++;
	}

}

static void
fc_probe()
{
Box_list		*b_list = NULL;
Box_list		*o_list = NULL;
Box_list		*c_list = NULL;
int			multi_path_flag, multi_print_flag;
int			duplicate_names_found = 0;

	if (l_get_box_list(&b_list, Options & PVERBOSE)) {
		L_ERR_PRINT;
		exit(-1);
	}
	if (b_list == NULL) {
		(void) fprintf(stdout,
			MSGSTR(-1, "No %s enclosures found "
			"in /dev/es\n"), ENCLOSURE_PROD_NAME);
	} else {
		o_list = b_list;
		(void) fprintf(stdout, MSGSTR(-1, "Found\n"));
		while (b_list != NULL) {
			/* Don't re-print multiple paths */
			c_list = o_list;
			multi_print_flag = 0;
			while (c_list != b_list) {
				if (strcmp(c_list->b_node_wwn_s,
					b_list->b_node_wwn_s) == 0) {
					multi_print_flag = 1;
					break;
				}
				c_list = c_list->box_next;
			}
			if (multi_print_flag) {
				b_list = b_list->box_next;
				continue;
			}
			(void) fprintf(stdout,
			MSGSTR(-1, "%s   Name:%s   Node WWN:%s   "),
			b_list->prod_id_s, b_list->b_name,
				b_list->b_node_wwn_s);
			/*
			 * Print logical path on same line if not multipathed.
			 */
			multi_path_flag = 0;
			c_list = o_list;
			while (c_list != NULL) {
				if ((c_list != b_list) &&
					(strcmp(c_list->b_node_wwn_s,
					b_list->b_node_wwn_s) == 0)) {
					multi_path_flag = 1;
				}
				c_list = c_list->box_next;
			}
			if (!multi_path_flag) {
				(void) fprintf(stdout,
				MSGSTR(-1, "Logical Path:%s"),
				b_list->logical_path);
			} else {
				(void) fprintf(stdout,
				MSGSTR(-1, "\n  Logical Path:%s"),
				b_list->logical_path);
			}
			if (Options & OPTION_P) {
				(void) fprintf(stdout,
				MSGSTR(-1, "\n  Physical Path:%s"),
				b_list->b_physical_path);
			}
			c_list = o_list;
			while (c_list != NULL) {
				if ((c_list != b_list) &&
				(strcmp(c_list->b_node_wwn_s,
					b_list->b_node_wwn_s) == 0)) {
					(void) fprintf(stdout,
					MSGSTR(-1, "\n  Logical Path:%s"),
					c_list->logical_path);
					if (Options & OPTION_P) {
						(void) fprintf(stdout,
						MSGSTR(-1,
						"\n  Physical Path:%s"),
						c_list->b_physical_path);
					}
				}
				c_list = c_list->box_next;
			}
			(void) fprintf(stdout, "\n");
			/* Check for duplicate names */
			if (l_duplicate_names(o_list, b_list->b_node_wwn_s,
				(char *)b_list->b_name,
				Options & PVERBOSE)) {
				duplicate_names_found++;
			}
			b_list = b_list->box_next;
		}
	}
	if (duplicate_names_found) {
		(void) fprintf(stdout,
			MSGSTR(-1, "\nWARNING: There are enclosures with "
			"the same names.\n"
			"You can not use the \"enclosure\""
			" name to specify these subsystems.\n"
			"Please use the \"enclosure_name\""
			" subcommand to select unique names.\n\n"));
	}
	l_free_box_list(&b_list);
}

/*
 * Display individual FC disk information.
 */
static void
display_fc_disk(struct path_struct *path_struct, struct l_inquiry_struct inq,
	int verbose)
{
L_disk_state	l_disk_state;
int		i;
float		m;
u_char		node_wwn[WWN_SIZE];
u_char		port_wwn[WWN_SIZE];
int		al_pa;
char		path_phys[MAXPATHLEN];
char		logical_path[MAXPATHLEN];
L_inquiry	local_inq;
char		ses_path[MAXPATHLEN];
char		name_buf[MAXNAMELEN];
sf_al_map_t	map;
struct dlist	*ml;
int		port_a_flag = 0;
int		no_path_flag = 0;

	strcpy(path_phys, path_struct->p_physical_path);

	(void) memset((char *)logical_path, 0, sizeof (logical_path));

	/*
	 * Get the WWN for our disk
	 */
	if (l_get_wwn(path_phys, port_wwn, node_wwn,
		&al_pa, (Options & PVERBOSE))) {
		L_ERR_PRINT;
		exit(-1);
	}

	/* Get logical path to the disk. */
	if (g_WWN_list == (WWN_list *) NULL) {
		if (l_get_wwn_list(&g_WWN_list, (Options & PVERBOSE))) {
			L_ERR_PRINT;
			exit(-1);
		}
	}


	/*
	 * Get the location information.
	 */
	if (l_get_dev_map(path_phys, &map, (Options & PVERBOSE))) {
		L_ERR_PRINT;
		exit(-1);
	}
	if (l_get_ses_path(path_phys, ses_path, &map,
		(Options & PVERBOSE))) {
		(void) sprintf(l_error_msg,
			" Could not obtain path "
			"to ses device.\n");
		l_error_msg_ptr = &l_error_msg[0];
		L_ERR_PRINT;
		exit(-1);
	}
	if (l_get_inquiry(ses_path, &local_inq)) {
		L_ERR_PRINT;
		exit(-1);
	}
	strncpy((char *)name_buf, (char *)local_inq.inq_box_name,
		sizeof (local_inq.inq_box_name));

	/*
	 * Get the disk status.
	 */
	(void) memset(&l_disk_state, 0, sizeof (struct l_disk_state_struct));
	if (l_get_disk_status(path_phys, &l_disk_state,
		(Options & PVERBOSE))) {
		L_ERR_PRINT;
		exit(-1);
	}

	if (l_disk_state.l_state_flag & L_NO_PATH_FOUND) {
		(void) fprintf(stderr, MSGSTR(-1,
			"\nWARNING: No path found "
			"in /dev/rdsk directory\n"
			"  Please check the logical links in /dev/rdsk\n"
			"  (It may be necessary to run the \"disks\" "
			"program.)\n\n"));

		/* Just call to get the status directly. */
		if (l_get_port(ses_path, &port_a_flag, verbose)) {
			L_ERR_PRINT;
			exit(-1);
		}
		if (l_get_disk_port_status(path_phys,
			&l_disk_state, port_a_flag,
			(Options & PVERBOSE))) {
			L_ERR_PRINT;
			exit(-1);
		}
		no_path_flag++;
	}

	(void) fprintf(stdout,
		MSGSTR(-1, "DEVICE PROPERTIES for disk: %s\n"),
			path_struct->argv);
	if (l_disk_state.port_a_valid) {
		(void) fprintf(stdout, MSGSTR(-1, "  Status(Port A):\t"));
		if (l_disk_state.d_state_flags[PORT_A] & L_OPEN_FAIL) {
			(void) fprintf(stdout, MSGSTR(-1, "Open Failed\n"));
		} else if (l_disk_state.d_state_flags[PORT_A] & L_NOT_READY) {
			(void) fprintf(stdout, MSGSTR(-1, "Not Ready\n"));
		} else if (l_disk_state.d_state_flags[PORT_A] &
			L_NOT_READABLE) {
			(void) fprintf(stdout, MSGSTR(-1, "Not Readable\n"));
		} else if (l_disk_state.d_state_flags[PORT_A] & L_SPUN_DWN_D) {
			(void) fprintf(stdout, MSGSTR(-1, "Spun Down\n"));
		} else if (l_disk_state.d_state_flags[PORT_A] & L_SCSI_ERR) {
			(void) fprintf(stdout, MSGSTR(-1, "SCSI Error\n"));
		} else if (l_disk_state.d_state_flags[PORT_A] & L_RESERVED) {
			(void) fprintf(stdout, MSGSTR(-1, "Reservation"
			" conflict\n"));
		} else if (l_disk_state.d_state_flags[PORT_A] & L_NO_LABEL) {
			(void) fprintf(stdout, MSGSTR(-1, "No UNIX Label\n"));
		} else {
			(void) fprintf(stdout, MSGSTR(-1, "O.K.\n"));
		}
	}
	if (l_disk_state.port_b_valid) {
		(void) fprintf(stdout, MSGSTR(-1, "  Status(Port B):\t"));
		if (l_disk_state.d_state_flags[PORT_B] & L_OPEN_FAIL) {
			(void) fprintf(stdout, MSGSTR(-1, "Open Failed\n"));
		} else if (l_disk_state.d_state_flags[PORT_B] & L_NOT_READY) {
			(void) fprintf(stdout, MSGSTR(-1, "Not Ready\n"));
		} else if (l_disk_state.d_state_flags[PORT_B] &
			L_NOT_READABLE) {
			(void) fprintf(stdout, MSGSTR(-1, "Not Readable\n"));
		} else if (l_disk_state.d_state_flags[PORT_B] & L_SPUN_DWN_D) {
			(void) fprintf(stdout, MSGSTR(-1, "Spun Down\n"));
		} else if (l_disk_state.d_state_flags[PORT_B] & L_SCSI_ERR) {
			(void) fprintf(stdout, MSGSTR(-1, "SCSI Error\n"));
		} else if (l_disk_state.d_state_flags[PORT_B] & L_RESERVED) {
			(void) fprintf(stdout, MSGSTR(-1, "Reservation"
			" conflict\n"));
		} else if (l_disk_state.d_state_flags[PORT_B] & L_NO_LABEL) {
			(void) fprintf(stdout, MSGSTR(-1, "No UNIX Label\n"));
		} else {
			(void) fprintf(stdout, MSGSTR(-1, "O.K.\n"));
		}
	}
	if (no_path_flag) {
		(void) fprintf(stdout, MSGSTR(-1, "  Status(Port %s):\t"),
		port_a_flag ? "B" : "A");
		if (l_disk_state.d_state_flags[port_a_flag] & L_OPEN_FAIL) {
			(void) fprintf(stdout, MSGSTR(-1, "Open Failed\n"));
		} else if (l_disk_state.d_state_flags[port_a_flag] &
			L_NOT_READY) {
			(void) fprintf(stdout, MSGSTR(-1, "Not Ready\n"));
		} else if (l_disk_state.d_state_flags[port_a_flag] &
			L_NOT_READABLE) {
			(void) fprintf(stdout, MSGSTR(-1, "Not Readable\n"));
		} else if (l_disk_state.d_state_flags[port_a_flag] &
			L_SPUN_DWN_D) {
			(void) fprintf(stdout, MSGSTR(-1, "Spun Down\n"));
		} else if (l_disk_state.d_state_flags[port_a_flag] &
			L_SCSI_ERR) {
			(void) fprintf(stdout, MSGSTR(-1, "SCSI Error\n"));
		} else if (l_disk_state.d_state_flags[port_a_flag] &
			L_RESERVED) {
			(void) fprintf(stdout, MSGSTR(-1, "Reservation"
			" conflict\n"));
		} else if (l_disk_state.d_state_flags[port_a_flag] &
			L_NO_LABEL) {
			(void) fprintf(stdout, MSGSTR(-1, "No UNIX Label\n"));
		} else {
			(void) fprintf(stdout, MSGSTR(-1, "O.K.\n"));
		}
	} else if ((!l_disk_state.port_a_valid) &&
			(!l_disk_state.port_b_valid)) {
		(void) fprintf(stdout, MSGSTR(-1, "  Status:\t\t"
		"No state available.\n"));
	}

	(void) fprintf(stdout, MSGSTR(-1, "  Vendor:\t\t"));
	for (i = 0; i < sizeof (inq.inq_vid); i++) {
		(void) fprintf(stdout, "%c", inq.inq_vid[i]);
	}
	(void) fprintf(stdout, MSGSTR(-1, "\n  Product ID:\t\t"));
	for (i = 0; i < sizeof (inq.inq_pid); i++) {
		(void) fprintf(stdout, "%c", inq.inq_pid[i]);
	}
	(void) fprintf(stdout, MSGSTR(-1, "\n  WWN(Node):"));
	(void) fprintf(stdout, "\t\t%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x",
			node_wwn[0], node_wwn[1], node_wwn[2], node_wwn[3],
			node_wwn[4], node_wwn[5], node_wwn[6], node_wwn[7]);

	if (l_disk_state.port_a_valid) {
		(void) fprintf(stdout, MSGSTR(-1, "\n  WWN(Port_A):"
				"\t\t%s"), l_disk_state.port_a_wwn_s);
	}
	if (l_disk_state.port_b_valid) {
		(void) fprintf(stdout, MSGSTR(-1, "\n  WWN(Port_B):"
				"\t\t%s"), l_disk_state.port_b_wwn_s);
	}
	(void) fprintf(stdout, MSGSTR(-1, "\n  Revision:\t\t"));
	for (i = 0; i < sizeof (inq.inq_revision); i++) {
	    (void) fprintf(stdout, "%c", inq.inq_revision[i]);
	}
	if ((strstr((char *)inq.inq_pid, "SUN") != 0) ||
		(strncmp((char *)inq.inq_vid, "SUN     ",
		sizeof (inq.inq_vid)) == 0)) {
		/*
		 * Only print the Serial Number
		 * if vendor ID is SUN or product ID
		 * contains SUN as other drives may
		 * not have the Serial Number fields defined
		 *
		 * NOTE: The Serial Number is stored in 2 fields??
		 *
		 */
		(void) fprintf(stdout, MSGSTR(-1, "\n  Serial Num:\t\t"));
		for (i = 0; i < sizeof (inq.inq_firmware_rev); i++) {
			(void) fprintf(stdout,
				"%c", inq.inq_firmware_rev[i]);
		}
		for (i = 0; i < sizeof (inq.inq_serial); i++) {
			(void) fprintf(stdout, "%c", inq.inq_serial[i]);
		}
	}
	/* Display disk capacity */
	m = l_disk_state.num_blocks;
	if (m) {
		m /= 2048;	/* get mega bytes */
		(void) fprintf(stdout,
		    MSGSTR(-1, "\n  Unformatted Capacity:\t%6.3f MByte\n"), m);
	} else {
		(void) fprintf(stdout, "\n");
	}

	if (l_disk_state.persistent_reserv_flag) {
		(void) fprintf(stdout, MSGSTR(-1, "  Persistant Reserve:\t"));
		if (l_disk_state.persistent_active) {
			(void) fprintf(stdout, MSGSTR(-1,
				"Active\n"));
		} else if (l_disk_state.persistent_registered) {
			(void) fprintf(stdout, MSGSTR(-1,
				"Registered\n"));
		} else {
			(void) fprintf(stdout, MSGSTR(-1,
				"Not being used\n"));
		}
	}

	(void) fprintf(stdout, MSGSTR(-1, "  Location:\t\t"));
	if (path_struct->slot_valid) {
		(void) fprintf(stdout, MSGSTR(-1,
			"In slot %d in the %s of the enclosure named: %s\n"),
			path_struct->slot,
			path_struct->f_flag ? "Front" : "Rear",
			name_buf);
	} else {
		(void) fprintf(stdout, MSGSTR(-1,
			"In the enclosure named: %s\n"),
			name_buf);
	}
	ml = l_disk_state.multipath_list;
	(void) fprintf(stdout, MSGSTR(-1, "  Path(s):\n"));
	while (ml) {
		(void) fprintf(stdout, MSGSTR(-1,
			"  %s\n  %s\n"), ml->logical_path, ml->dev_path);
		ml = ml->next;
	}
	l_free_multipath(l_disk_state.multipath_list);

	if (Options & OPTION_V) {
		/* Only bother if the state is O.K. */
		if (l_disk_state.port_a_valid) {
			if (l_disk_state.d_state_flags[PORT_A] == 0) {
				adm_display_verbose_disk(path_phys, verbose);
			}
		} else if (l_disk_state.port_b_valid) {
			if (l_disk_state.d_state_flags[PORT_B] == 0) {
				adm_display_verbose_disk(path_phys, verbose);
			}
		}
	}

	(void) fprintf(stdout, "\n");

}


/*
 *	DISPLAY function
 *
 */
static void
adm_display_config(char **argv, int option_t_input, int argc)
{
char		*path_phys;
L_inquiry	inq;
int		i;
int		path_index = 0;
sf_al_map_t	map;
Path_struct	*path_struct;



	while (argv[path_index] != NULL) {
	    VERBPRINT("  Displaying information for: %s\n", argv[path_index]);
	    if ((path_phys = l_convert_name(argv[path_index],
		&path_struct, Options & PVERBOSE)) == NULL) {
		L_ERR_PRINT;
		exit(-1);
	    }

	/*
	 * See what kind of device we are talking to.
	 */
	    if (l_get_inquiry(path_phys, &inq)) {
		/* INQUIRY failed - try ssa display */
		ssa_cli_display_config(argv, path_phys,
			option_t_input, 0, argc);
	    } else if ((strstr((char *)inq.inq_pid, ENCLOSURE_PROD_ID) != 0) ||
			(strncmp((char *)inq.inq_vid, "SUN     ",
			sizeof (inq.inq_vid)) &&
			(inq.inq_dtype == DTYPE_ESI))) {
		/*
		 * Again this is like the ssaadm code in that the name
		 * is still not defined before this code must be released.
		 */
		(void) fprintf(stdout,
		"\n                          (%s %s)\n", whoami, lux_version);
		(void) fprintf(stdout, "\t\t\t\t   ");
		for (i = 0; i < sizeof (inq.inq_pid); i++) {
			(void) fprintf(stdout, "%c", inq.inq_pid[i]);
		}

		(void) fprintf(stdout, "\n");
		if (Options & OPTION_R) {
			adm_display_err(path_phys,
				inq.inq_dtype, Options & PVERBOSE);
		} else {
			pho_display_config(path_phys);
		}

	    } else if (strstr((char *)inq.inq_pid, "SSA") != 0) {
		ssa_cli_display_config(argv, path_phys,
			option_t_input, 0, argc);

	    } else if (inq.inq_dtype == DTYPE_DIRECT) {
		/*
		 * See if we are in fact talking to a loop or not.
		 */
		if (l_get_dev_map(path_phys, &map,
			(Options & PVERBOSE)) != 0) {
			ssa_cli_display_config(argv, path_phys,
				option_t_input, 0, argc);
		} else {
			if (Options & OPTION_R) {
				(void) fprintf(stdout,
		"\n                          (%s %s)\n", whoami, lux_version);
				adm_display_err(path_phys,
				inq.inq_dtype, Options & PVERBOSE);
			} else {
				display_fc_disk(path_struct, inq,
					Options & PVERBOSE);
			}
		}
	    } else if (strstr((char *)inq.inq_pid, "SUN_SEN") != 0) {
			if (strcmp(argv[path_index], path_phys) != 0) {
				(void) fprintf(stdout,
				MSGSTR(-1,
				"  Physical path:\n  %s\n"), path_phys);
			}
			(void) fprintf(stdout, "DEVICE is a ");
			for (i = 0; i < sizeof (inq.inq_vid); i++) {
				(void) fprintf(stdout, "%c", inq.inq_vid[i]);
			}
			(void) fprintf(stdout, " ");
			for (i = 0; i < sizeof (inq.inq_pid); i++) {
				(void) fprintf(stdout, "%c", inq.inq_pid[i]);
			}
			(void) fprintf(stdout, " card.");
			if (inq.inq_len > 31) {
				(void) fprintf(stdout, "   Revision: ");
				for (i = 0; i < sizeof (inq.inq_revision);
					i++) {
					(void) fprintf(stdout, "%c",
					inq.inq_revision[i]);
				}
			}
			(void) fprintf(stdout, "\n");
		} else if (inq.inq_dtype < 0x10) {
			(void) fprintf(stdout,
				"DEVICE: %s\n",
				argv[path_index]);
			if (strcmp(argv[path_index], path_phys) != 0) {
				(void) fprintf(stdout,
				"  Physical path:\n  %s\n", path_phys);
			}
			(void) fprintf(stdout, "%s%s\n",
				"  Device type: ", dtype[inq.inq_dtype]);
	    } else if (inq.inq_dtype < 0x1f) {
			(void) fprintf(stdout, "%s%s\n",
			"  Device type: ", "Reserved");
	    } else {
			(void) fprintf(stdout, "%s%s\n",
			"  Device type: ", "Unknown device");
	    }
	    path_index++;
	}
}

/*
 * Display temperature bytes 1-3 state.
 */
static void
temp_decode(Temp_elem_st *temp)
{
	if (temp->ot_fail) {
		(void) fprintf(stdout, MSGSTR(-1,
			": FAILURE - Over Temperature"));
	}
	if (temp->ut_fail) {
		(void) fprintf(stdout, MSGSTR(-1,
			": FAILURE - Under Temperature"));
	}
	if (temp->ot_warn) {
		(void) fprintf(stdout, MSGSTR(-1,
			": WARNING - Over Temperature"));
	}
	if (temp->ut_warn) {
		(void) fprintf(stdout, MSGSTR(-1,
			": WARNING - Under Temperature"));
	}
}

/*
 * Display temperature in Degrees Celsius.
 */
static void
disp_degree(Temp_elem_st *temp)
{
int	t;

	t = temp->degrees;
	t -= 20;	/* re-adjust */
	/*
	 * NL_Comment
	 * The %c is the degree symbol.
	 */
	(void) fprintf(stdout, MSGSTR(-1, ":%1.2d%cC "), t, 186);
}

/*
 * Display tranceivers state.
 */
static void
trans_decode(Trans_elem_st *trans)
{
	if (trans->disabled) {
		(void) fprintf(stdout, MSGSTR(-1,
			": disabled"));
	}
	if (trans->lol) {
		(void) fprintf(stdout, MSGSTR(-1,
			": Not receiving a signal\n\t"));
	}
	if (trans->lsr_fail) {
		(void) fprintf(stdout, MSGSTR(-1,
			": Laser failed"));
	}
}

/*
 * Display tranceiver status.
 *
 * NOTE: The decoding of the status assumes that the elements
 * are in order with the first two elements are for the
 * "A" IB. It also assumes the tranceivers are numbered
 * 0 and 1.
 */
static void
trans_messages(struct l_state_struct *l_state, int ib_a_flag)
{
Trans_elem_st	trans;
int	i, j, k;
int	count = 0;
int	elem_index = 0;

	/* Get and print messages */
	for (i = 0; i < (int)l_state->ib_tbl.config.enc_num_elem; i++) {
	    elem_index++;
	    if (l_state->ib_tbl.config.type_hdr[i].type == ELM_TYP_FL) {

		if (l_state->ib_tbl.config.type_hdr[i].text_len != 0) {
			(void) fprintf(stdout, "\n\t\t%s\n",
			l_state->ib_tbl.config.text[i]);
		}
		count = k = 0;

		for (j = 0; j <
			(int)l_state->ib_tbl.config.type_hdr[i].num; j++) {
			/*
			 * Only display the status for the selected IB.
			 */
		    if ((count < 2 && ib_a_flag) ||
				(count >= 2 && !ib_a_flag)) {
			bcopy((const void *)
				&l_state->ib_tbl.p2_s.element[elem_index + j],
				(void *)&trans, sizeof (trans));

			if (k == 0) {
				(void) fprintf(stdout, "\t\t%d ", k);
			} else {
				(void) fprintf(stdout, "\t%d ", k);
			}
			if (trans.code == S_OK) {
				(void) fprintf(stdout,
				MSGSTR(-1, "O.K."));
				revision_msg(l_state, elem_index + j);
			} else if ((trans.code == S_CRITICAL) ||
				(trans.code == S_NONCRITICAL)) {
				(void) fprintf(stdout,
				MSGSTR(-1, "Failed"));
				revision_msg(l_state, elem_index + j);
				trans_decode(&trans);
			} else if (trans.code == S_NOT_INSTALLED) {
				(void) fprintf(stdout,
				MSGSTR(-1, "Not Installed"));
			} else if (trans.code == S_NOT_AVAILABLE) {
				(void) fprintf(stdout,
				MSGSTR(-1, "Disabled"));
				revision_msg(l_state, elem_index + j);
			} else {
				(void) fprintf(stdout,
				MSGSTR(-1, "Unknown status"));
			}
			k++;
		    }
		    count++;
		}
	    }
		/*
		 * Calculate the index to each element.
		 */
		elem_index += l_state->ib_tbl.config.type_hdr[i].num;
	}
	(void) fprintf(stdout, "\n");
}

/*
 * Display temperature status.
 */
static void
temperature_messages(struct l_state_struct *l_state, int rear_flag)
{
Temp_elem_st	temp;
int	i, j, last_ok = 0;
int	all_ok = 1;
int	elem_index = 0;

	/* Get and print messages */
	for (i = 0; i < (int)l_state->ib_tbl.config.enc_num_elem; i++) {
	    elem_index++;	/* skip global */
	    if (l_state->ib_tbl.config.type_hdr[i].type == ELM_TYP_TS) {
		if (!rear_flag) {
		rear_flag = 1;		/* only do front or rear backplane */
		if (l_state->ib_tbl.config.type_hdr[i].text_len != 0) {
			(void) fprintf(stdout, "\t  %s",
			l_state->ib_tbl.config.text[i]);
		}

		/*
		 * Check global status and if not all O.K.
		 * then print individually.
		 */
		bcopy((const void *)&l_state->ib_tbl.p2_s.element[i],
			(void *)&temp, sizeof (temp));
		for (j = 0; j <
			(int)l_state->ib_tbl.config.type_hdr[i].num; j++) {
			bcopy((const void *)
			&l_state->ib_tbl.p2_s.element[elem_index + j],
				(void *)&temp, sizeof (temp));

			if ((j == 0) && (temp.code == S_OK) &&
				(!(temp.ot_fail || temp.ot_warn ||
					temp.ut_fail || temp.ut_warn))) {
				(void) fprintf(stdout, "\n\t  %d", j);
			} else if ((j == 6) && (temp.code == S_OK) &&
				all_ok) {
				(void) fprintf(stdout, "\n\t  %d", j);
			} else if (last_ok && (temp.code == S_OK)) {
				(void) fprintf(stdout, "%d", j);
			} else {
				(void) fprintf(stdout, "\n\t\t%d", j);
			}
			if (temp.code == S_OK) {
				disp_degree(&temp);
				if (temp.ot_fail || temp.ot_warn ||
					temp.ut_fail || temp.ut_warn) {
					temp_decode(&temp);
					all_ok = 0;
					last_ok = 0;
				} else {
					last_ok++;
				}
			} else if (temp.code == S_CRITICAL) {
				(void) fprintf(stdout,
				MSGSTR(-1, " Critical failure"));
				last_ok = 0;
				all_ok = 0;
			} else if (temp.code == S_NONCRITICAL) {
				(void) fprintf(stdout,
				MSGSTR(-1, " Non-critical failure"));
				last_ok = 0;
				all_ok = 0;
			} else if (temp.code == S_NOT_INSTALLED) {
				(void) fprintf(stdout,
				MSGSTR(-1, " Not Installed"));
				last_ok = 0;
				all_ok = 0;
			} else if (temp.code == S_NOT_AVAILABLE) {
				(void) fprintf(stdout,
				MSGSTR(-1, " Disabled"));
				last_ok = 0;
				all_ok = 0;
			} else {
				(void) fprintf(stdout,
				MSGSTR(-1, " Unknown status"));
				last_ok = 0;
				all_ok = 0;
			}
		}
		if (all_ok) {
			(void) fprintf(stdout,
			MSGSTR(-1, " (All temperatures are "
			"NORMAL.)"));
		}
		all_ok = 1;
		(void) fprintf(stdout, "\n");
	    } else {
		rear_flag = 0;
	    }
	    }
	    elem_index += l_state->ib_tbl.config.type_hdr[i].num;
	}
}

/*
 * Display IB byte 3 state.
 */
static void
ib_decode(Ctlr_elem_st *ctlr)
{
	if (ctlr->overtemp_alart) {
		(void) fprintf(stdout, MSGSTR(-1,
			" - IB Over Temperature Alert "));
	}
	if (ctlr->ib_loop_1_fail) {
		(void) fprintf(stdout, MSGSTR(-1,
			" - IB Loop 1 has failed "));
	}
	if (ctlr->ib_loop_0_fail) {
		(void) fprintf(stdout, MSGSTR(-1,
			" - IB Loop 0 has failed "));
	}
}

/*
 * Display motherboard (interconnect assembly) messages.
 */
static void
mb_messages(struct l_state_struct *l_state, int index, int elem_index)
{
int		j;
Interconnect_st	interconnect;

	if (l_state->ib_tbl.config.type_hdr[index].text_len != 0) {
		(void) fprintf(stdout, "%s\n",
		l_state->ib_tbl.config.text[index]);
	}
	for (j = 0; j < (int)l_state->ib_tbl.config.type_hdr[index].num;
			j++) {
		bcopy((const void *)
			&l_state->ib_tbl.p2_s.element[elem_index + j],
			(void *)&interconnect, sizeof (interconnect));
		(void) fprintf(stdout, "\t");
		if (interconnect.code == S_OK) {
			(void) fprintf(stdout,
			MSGSTR(-1, "O.K."));
			revision_msg(l_state, elem_index + j);
			(void) fprintf(stdout, "\n");
		} else if (interconnect.code == S_NOT_INSTALLED) {
			(void) fprintf(stdout,
			MSGSTR(-1, "Not installed\n"));
		} else if (interconnect.code == S_CRITICAL) {
			(void) fprintf(stdout,
			MSGSTR(-1, "Critical Failure: %s failure"),
			interconnect.eprom_fail ? "EPROM" :
			"Unknown");
			revision_msg(l_state, elem_index + j);
			(void) fprintf(stdout, "\n");
		} else if (interconnect.code == S_NONCRITICAL) {
			(void) fprintf(stdout,
			MSGSTR(-1, "Non-Critical Failure: %s failure"),
			interconnect.eprom_fail ? "EPROM" :
			"Unknown");
			revision_msg(l_state, elem_index + j);
			(void) fprintf(stdout, "\n");
		} else if (interconnect.code == S_NOT_AVAILABLE) {
			(void) fprintf(stdout,
			MSGSTR(-1, "Disabled"));
			revision_msg(l_state, elem_index + j);
			(void) fprintf(stdout, "\n");
		} else {
			(void) fprintf(stdout,
			MSGSTR(-1, "Unknown Status\n"));
		}
	}


}

/*
 * Display back_plane messages including the temperature's.
 */
static void
back_plane_messages(struct l_state_struct *l_state, int index, int elem_index)
{
Bp_elem_st	bp;
int		j;

	if (l_state->ib_tbl.config.type_hdr[index].text_len != 0) {
		(void) fprintf(stdout, "%s\n",
		l_state->ib_tbl.config.text[index]);
	}
	for (j = 0; j < (int)l_state->ib_tbl.config.type_hdr[index].num;
			j++) {
		bcopy((const void *)
			&l_state->ib_tbl.p2_s.element[elem_index + j],
			(void *)&bp, sizeof (bp));
		if (j == 0) {
			(void) fprintf(stdout, "\tFront Backplane: ");
		} else {
			(void) fprintf(stdout, "\tRear Backplane:  ");
		}
		if (bp.code == S_OK) {
			(void) fprintf(stdout, MSGSTR(-1, "O.K."));
			revision_msg(l_state, elem_index + j);
			(void) fprintf(stdout, "\n");
			temperature_messages(l_state, j);
		} else if (bp.code == S_NOT_AVAILABLE) {
			(void) fprintf(stdout,
			MSGSTR(-1, "Disabled"));
			revision_msg(l_state, elem_index + j);
			(void) fprintf(stdout, "\n");
			temperature_messages(l_state, j);
		} else if (bp.code == S_NOT_INSTALLED) {
			(void) fprintf(stdout,
			MSGSTR(-1, "Not installed\n"));
		} else if (bp.code == S_NONCRITICAL) {
			(void) fprintf(stdout,
			MSGSTR(-1, "Noncritical failure: %s"),
			bp.byp_a_enabled ? "Bypass A enabled" :
			"Bypass B enabled");
			revision_msg(l_state, elem_index + j);
			(void) fprintf(stdout, "\n");
			temperature_messages(l_state, j);
		} else if (bp.code == S_CRITICAL) {
			(void) fprintf(stdout,
			MSGSTR(-1, "Critical Failure"));
			revision_msg(l_state, elem_index + j);
			(void) fprintf(stdout, "\n");
			temperature_messages(l_state, j);
		} else {
			(void) fprintf(stdout,
			MSGSTR(-1, "Unknown Status\n"));
		}
	}
}

/*
 * Display loop messages.
 */
static void
loop_messages(struct l_state_struct *l_state, int index, int elem_index)
{
Loop_elem_st	loop;
int		j;

	if (l_state->ib_tbl.config.type_hdr[index].text_len != 0) {
		(void) fprintf(stdout, "%s\n",
		l_state->ib_tbl.config.text[index]);
	}
	for (j = 0; j < (int)l_state->ib_tbl.config.type_hdr[index].num;
			j++) {
		bcopy((const void *)
			&l_state->ib_tbl.p2_s.element[elem_index + j],
			(void *)&loop, sizeof (loop));
		if (j == 0) {
			(void) fprintf(stdout, "\tLoop A");
		} else {
			(void) fprintf(stdout, "\tLoop B");
		}
		if (loop.code == S_NOT_INSTALLED) {
			(void) fprintf(stdout,
			MSGSTR(-1, " is not installed\n"));
		} else {
			(void) fprintf(stdout,
			MSGSTR(-1, " is configured as"));
			if (loop.split) {
				(void) fprintf(stdout,
				MSGSTR(-1, " two separate loops.\n"));
			} else {
				(void) fprintf(stdout,
				MSGSTR(-1, " a single loop.\n"));
			}
		}
	}
}

/*
 * Display ESI Controller status.
 */
static void
ctlr_messages(struct l_state_struct *l_state, int index, int elem_index)
{
Ctlr_elem_st	ctlr;
int		j;
int		ib_a_flag = 1;

	if (l_state->ib_tbl.config.type_hdr[index].text_len != 0) {
		(void) fprintf(stdout, "%s\n",
		l_state->ib_tbl.config.text[index]);
	}
	for (j = 0; j < (int)l_state->ib_tbl.config.type_hdr[index].num;
			j++) {
		bcopy((const void *)
			&l_state->ib_tbl.p2_s.element[elem_index + j],
			(void *)&ctlr, sizeof (ctlr));
		if (j == 0) {
			(void) fprintf(stdout, "\tA: ");
		} else {
			(void) fprintf(stdout, "\tB: ");
			ib_a_flag = 0;
		}
		if (ctlr.code == S_OK) {
			(void) fprintf(stdout, MSGSTR(-1, "O.K."));
			/* If any byte 3 bits set display */
			ib_decode(&ctlr);
			/* Display Version message */
			revision_msg(l_state, elem_index + j);
			/*
			 * Display the tranciver module state for this
			 * IB.
			 */
			trans_messages(l_state, ib_a_flag);
		} else if (ctlr.code == S_CRITICAL) {
			(void) fprintf(stdout,
			MSGSTR(-1, "Critical failure"));
			ib_decode(&ctlr);
			(void) fprintf(stdout, "\n");
		} else if (ctlr.code == S_NONCRITICAL) {
			(void) fprintf(stdout,
			MSGSTR(-1, "Non-critical failure"));
			ib_decode(&ctlr);
			(void) fprintf(stdout, "\n");
		} else if (ctlr.code == S_NOT_INSTALLED) {
			(void) fprintf(stdout,
			MSGSTR(-1, "Not Installed\n"));
		} else if (ctlr.code == S_NOT_AVAILABLE) {
			(void) fprintf(stdout,
			MSGSTR(-1, "Disabled\n"));
		} else {
			(void) fprintf(stdout,
			MSGSTR(-1, "Unknown status\n"));
		}
	}
}

/*
 * Display Fans bytes 1-3 state.
 */
static void
fan_decode(Fan_elem_st *fan)
{
	if (fan->fail) {
		(void) fprintf(stdout, MSGSTR(-1,
			":Yellow LED is on"));
	}
	if (fan->speed == 0) {
		(void) fprintf(stdout, MSGSTR(-1,
			":Fan stopped"));
	} else if (fan->speed < S_HI_SPEED) {
		(void) fprintf(stdout, MSGSTR(-1,
			":Fan speed Low"));
	} else {
		(void) fprintf(stdout, MSGSTR(-1,
			":Fan speed Hi"));
	}
}

/*
 * Display Fan status.
 */
static void
fan_messages(struct l_state_struct *l_state, int hdr_index, int elem_index)
{
Fan_elem_st	fan;
int	j;

	/* Get and print messages */
	if (l_state->ib_tbl.config.type_hdr[hdr_index].text_len != 0) {
		(void) fprintf(stdout, "%s\n",
		l_state->ib_tbl.config.text[hdr_index]);
	}
	for (j = 0; j < (int)l_state->ib_tbl.config.type_hdr[hdr_index].num;
			j++) {
		bcopy((const void *)
			&l_state->ib_tbl.p2_s.element[elem_index + j],
			(void *)&fan, sizeof (fan));
		(void) fprintf(stdout, "\t%d ", j);
		if (fan.code == S_OK) {
			(void) fprintf(stdout, MSGSTR(-1, "O.K."));
			revision_msg(l_state, elem_index + j);
		} else if (fan.code == S_CRITICAL) {
			(void) fprintf(stdout,
			MSGSTR(-1, "Critical failure"));
			fan_decode(&fan);
			revision_msg(l_state, elem_index + j);
		} else if (fan.code == S_NONCRITICAL) {
			(void) fprintf(stdout,
			MSGSTR(-1, "Non-critical failure"));
			fan_decode(&fan);
			revision_msg(l_state, elem_index + j);
		} else if (fan.code == S_NOT_INSTALLED) {
			(void) fprintf(stdout,
			MSGSTR(-1, "Not Installed"));
		} else if (fan.code == S_NOT_AVAILABLE) {
			(void) fprintf(stdout,
			MSGSTR(-1, "Disabled"));
			revision_msg(l_state, elem_index + j);
		} else {
			(void) fprintf(stdout,
			MSGSTR(-1, "Unknown status"));
		}
	}
	(void) fprintf(stdout, "\n");
}

/*
 * Display Power Supply bytes 1-3 state.
 */
static void
ps_decode(Ps_elem_st *ps)
{
	if (ps->dc_over) {
		(void) fprintf(stdout, MSGSTR(-1,
			": DC Voltage too high"));
	}
	if (ps->dc_under) {
		(void) fprintf(stdout, MSGSTR(-1,
			": DC Voltage too low"));
	}
	if (ps->dc_over_i) {
		(void) fprintf(stdout, MSGSTR(-1,
			": DC Current too high"));
	}
	if (ps->ovrtmp_fail || ps->temp_warn) {
		(void) fprintf(stdout, MSGSTR(-1,
			": Temperature too high"));
	}
	if (ps->ac_fail) {
		(void) fprintf(stdout, MSGSTR(-1,
			": AC Failed"));
	}
	if (ps->dc_fail) {
		(void) fprintf(stdout, MSGSTR(-1,
			": DC Failed"));
	}
}

/*
 * Print the revision message from page 7
 */
static	void
revision_msg(struct l_state_struct *l_state, int index)
{
	if (strlen((const char *)
		l_state->ib_tbl.p7_s.element_desc[index].desc_string)) {
		(void) fprintf(stdout, "(%s)",
		l_state->ib_tbl.p7_s.element_desc[index].desc_string);
	}
}



/*
 * Display Power Supply status.
 */
static void
ps_messages(struct l_state_struct *l_state, int	index, int elem_index)
{
Ps_elem_st	ps;
int	j;

	/* Get and print Power Supply messages */

	if (l_state->ib_tbl.config.type_hdr[index].text_len != 0) {
		(void) fprintf(stdout, "%s\n",
		l_state->ib_tbl.config.text[index]);
	}

	for (j = 0; j < (int)l_state->ib_tbl.config.type_hdr[index].num;
		j++) {
		bcopy((const void *)
			&l_state->ib_tbl.p2_s.element[elem_index + j],
			(void *)&ps, sizeof (ps));
		(void) fprintf(stdout, "\t%d ", j);
		if (ps.code == S_OK) {
			(void) fprintf(stdout, MSGSTR(-1, "O.K."));
			revision_msg(l_state, elem_index + j);
		} else if (ps.code == S_CRITICAL) {
			(void) fprintf(stdout,
			MSGSTR(-1, "Critical failure"));
			ps_decode(&ps);
			revision_msg(l_state, elem_index + j);
		} else if (ps.code == S_NONCRITICAL) {
			(void) fprintf(stdout,
			MSGSTR(-1, "Non-critical failure"));
			ps_decode(&ps);
			revision_msg(l_state, elem_index + j);
		} else if (ps.code == S_NOT_INSTALLED) {
			(void) fprintf(stdout,
			MSGSTR(-1, "Not Installed"));
		} else if (ps.code == S_NOT_AVAILABLE) {
			(void) fprintf(stdout,
			MSGSTR(-1, "Disabled"));
			revision_msg(l_state, elem_index + j);
		} else {
			(void) fprintf(stdout,
			MSGSTR(-1, "Unknown status"));
		}

	}
	(void) fprintf(stdout, "\n");
}

/*
 * Display any abnormal condition messages.
 */
static void
abnormal_condition_display(struct l_state_struct *l_state)
{

	(void) fprintf(stdout, "\n");
	if (l_state->ib_tbl.p2_s.ui.crit) {
		(void) fprintf(stdout,
			MSGSTR(-1, "                         "
			"CRITICAL CONDITION DETECTED\n"));
	}
	if (l_state->ib_tbl.p2_s.ui.crit) {
		(void) fprintf(stdout,
			MSGSTR(-1, "                   "
			"WARNING: NON-CRITICAL CONDITION DETECTED\n"));
	}
	if (l_state->ib_tbl.p2_s.ui.crit) {
		(void) fprintf(stdout,
			MSGSTR(-1, "                      "
			"WARNING: Invalid Operation bit set.\n"
			"\tThis means an Enclosure Control page"
			" or an Array Control page with an invalid\n"
			"\tformat has previously been transmitted to the"
			" Enclosure Services card by a\n\tSend Diagnostic"
			" SCSI command.\n"));
	}
	(void) fprintf(stdout, "\n");
}



static void
adm_inquiry(char **argv)
{
L_inquiry	*inquiry_ptr;
L_inquiry	inq;
int		path_index = 0;
char		**p;
u_char		*v_parm;
int		i, scsi_3, length;
char		byte_number[MAXNAMELEN];
char		*path_phys;
Path_struct	*path_struct;

static	char *scsi_inquiry_labels_2[] = {
	"Vendor:                     ",
	"Product:                    ",
	"Revision:                   ",
	"Firmware Revision           ",
	"Serial Number               ",
	"Device type:                ",
	"Removable media:            ",
	"ISO version:                ",
	"ECMA version:               ",
	"ANSI version:               ",
	"Async event notification:   ",
	"Terminate i/o process msg:  ",
	"Response data format:       ",
	"Additional length:          ",
	"Relative addressing:        ",
	"32 bit transfers:           ",
	"16 bit transfers:           ",
	"Synchronous transfers:      ",
	"Linked commands:            ",
	"Command queueing:           ",
	"Soft reset option:          "
};
static	char *scsi_inquiry_labels_3[] = {
	"Vendor:                     ",
	"Product:                    ",
	"Revision:                   ",
	"Firmware Revision           ",
	"Serial Number               ",
	"Device type:                ",
	"Removable media:            ",
	"Medium Changer Element:     ",
	"ISO version:                ",
	"ECMA version:               ",
	"ANSI version:               ",
	"Async event reporting:      ",
	"Terminate task:             ",
	"Normal ACA Supported:       ",
	"Response data format:       ",
	"Additional length:          ",
	"Cmd received on port:       ",
	"SIP Bits:                   ",
	"Relative addressing:        ",
	"Linked commands:            ",
	"Transfer Disable:           ",
	"Command queueing:           ",
};
static	char	*ansi_version[] = {
	" (Device might or might not comply to an ANSI version)",
	" (This code is reserved for historical uses)",
	" (Device complies to ANSI X3.131-1994 (SCSI-2))",
	" (Device complies to SCSI-3)"
};

	while (argv[path_index] != NULL) {
	    if ((path_phys = l_convert_name(argv[path_index],
		&path_struct, Options & PVERBOSE)) == NULL) {
		(void) fprintf(stderr, "\n");
		L_ERR_PRINT;
		(void) fprintf(stderr, "\n");
		path_index++; continue;
	    }
	    inquiry_ptr = &inq;
	    if (l_get_inquiry(path_phys, inquiry_ptr)) {
		(void) fprintf(stderr, "\n");
		L_ERR_PRINT;
		(void) fprintf(stderr, "\n");

	    /* Continue on in case of an error */
	    } else {

	    /* print inquiry information */

	    (void) fprintf(stdout, MSGSTR(-1, "\nINQUIRY:\n"));
	    if (strcmp(argv[path_index], path_phys) != 0) {
		(void) fprintf(stdout,
		MSGSTR(-1, "  Physical path:\n  %s\n"), path_phys);
	    }
	    if (inq.inq_ansi < 3) {
		p = scsi_inquiry_labels_2;
		scsi_3 = 0;
	    } else {
		p = scsi_inquiry_labels_3;
		scsi_3 = 1;
	    }
	    if (inq.inq_len < 11) {
		p += 1;
	    } else {
		/* */
		(void) fprintf(stdout, "%s", *p++);
		for (i = 0; i < sizeof (inq.inq_vid); i++) {
			(void) fprintf(stdout, "%c", inq.inq_vid[i]);
		}
		(void) fprintf(stdout, "\n");
	    }
	    if (inq.inq_len < 27) {
		p += 1;
	    } else {
		(void) fprintf(stdout, "%s", *p++);
		for (i = 0; i < sizeof (inq.inq_pid); i++) {
			(void) fprintf(stdout, "%c", inq.inq_pid[i]);
		}
		(void) fprintf(stdout, "\n");
	    }
	    if (inq.inq_len < 31) {
		p += 1;
	    } else {
		(void) fprintf(stdout, "%s", *p++);
		for (i = 0; i < sizeof (inq.inq_revision); i++) {
		    (void) fprintf(stdout, "%c", inq.inq_revision[i]);
		}
		(void) fprintf(stdout, "\n");
	    }
	    if (inq.inq_len < 39) {
		p += 2;
	    } else {
		/*
		 * If Pluto then print
		 * firmware rev & serial #.
		 */
		if (strstr((char *)inq.inq_pid, "SSA") != 0) {
			(void) fprintf(stdout, "%s", *p++);
			for (i = 0; i < sizeof (inq.inq_firmware_rev); i++) {
				(void) fprintf(stdout,
				"%c", inq.inq_firmware_rev[i]);
			}
			(void) fprintf(stdout, "\n");
			(void) fprintf(stdout, "%s", *p++);
			for (i = 0; i < sizeof (inq.inq_serial); i++) {
				(void) fprintf(stdout, "%c", inq.inq_serial[i]);
			}
			(void) fprintf(stdout, "\n");
		} else if (((strstr((char *)inq.inq_pid, "SUN") != 0) ||
			(strncmp((char *)inq.inq_vid, "SUN     ",
			sizeof (inq.inq_vid)) == 0)) &&
			(strstr((char *)inq.inq_pid,
			ENCLOSURE_PROD_ID) == 0)) {
			/*
			 * Only print the Serial Number
			 * if vendor ID is SUN or product ID
			 * contains SUN as other drives may
			 * not have the Serial Number fields defined
			 * and it is not a Photon SES card.
			 *
			 * NOTE: The Serial Number is stored in 2 fields??
			 *
			 */
			p++;
			(void) fprintf(stdout, "%s", *p++);
			for (i = 0; i < sizeof (inq.inq_firmware_rev); i++) {
				(void) fprintf(stdout,
				"%c", inq.inq_firmware_rev[i]);
			}
			for (i = 0; i < sizeof (inq.inq_serial); i++) {
				(void) fprintf(stdout, "%c", inq.inq_serial[i]);
			}
			(void) fprintf(stdout, "\n");
		    } else {
			p += 2;
		    }
	    }
	    if (inq.inq_dtype < 0x10) {
		(void) fprintf(stdout, "%s0x%x (%s)\n", *p++, inq.inq_dtype,
		dtype[inq.inq_dtype]);
	    } else if (inq.inq_dtype < 0x1f) {
		(void) fprintf(stdout, "%s0x%x %s\n", *p++, inq.inq_dtype,
		"(Reserved)");
	    } else {
		(void) fprintf(stdout, "%s0x%x %s\n", *p++, inq.inq_dtype,
		"(Unknown device)");
	    }
	    (void) fprintf(stdout, "%s%s\n", *p++, inq.inq_rmb ? "yes" : "no");
	    if (scsi_3) {
		(void) fprintf(stdout, "%s%s\n", *p++,
		inq.inq_mchngr ? "yes" : "no");
	    }
	    (void) fprintf(stdout, "%s%d\n", *p++, inq.inq_iso);
	    (void) fprintf(stdout, "%s%d\n", *p++, inq.inq_ecma);
	    if (inq.inq_ansi < 0x4) {
		(void) fprintf(stdout, "%s%d%s\n", *p++, inq.inq_ansi,
		ansi_version[inq.inq_ansi]);
	    } else
		(void) fprintf(stdout, "%s%d%s\n", *p++, inq.inq_ansi,
		"Reserved");

	    if (inq.inq_aenc) {
		(void) fprintf(stdout, "%s%s\n", *p++, "yes");
	    } else {
		p++;
	    }
	    if (scsi_3) {
		(void) fprintf(stdout, "%s%s\n", *p++,
		inq.inq_normaca ? "yes" : "no");
	    }
	    if (inq.inq_trmiop) {
		(void) fprintf(stdout, "%s%s\n", *p++, "yes");
	    } else {
		p++;
	    }
	    (void) fprintf(stdout, "%s%d\n", *p++, inq.inq_rdf);
	    (void) fprintf(stdout, "%s0x%x\n", *p++, inq.inq_len);
	    if (scsi_3) {
		if (inq.inq_dual_p) {
		    (void) fprintf(stdout, "%s%s\n", *p++,
			inq.inq_port ? "a" : "b");
		} else {
		    p++;
		}
	    }
	    if (scsi_3) {
		if (inq.inq_SIP_1 || inq.ui.inq_SIP_2 ||
			inq.ui.inq_SIP_3) {
		    (void) fprintf(stdout, "%s%d, %d, %d\n", *p++,
			inq.inq_SIP_1, inq.ui.inq_SIP_2, inq.ui.inq_SIP_3);
		} else {
		    p++;
		}
	    }

	    if (inq.ui.inq_2_reladdr) {
		(void) fprintf(stdout, "%s%s\n", *p++, "yes");
	    } else {
		p++;
	    }
	    if (!scsi_3) {

		    if (inq.ui.inq_wbus32) {
			(void) fprintf(stdout, "%s%s\n", *p++, "yes");
		    } else {
			p++;
		    }
		    if (inq.ui.inq_wbus16) {
			(void) fprintf(stdout, "%s%s\n", *p++, "yes");
		    } else {
			p++;
		    }
		    if (inq.ui.inq_sync) {
			(void) fprintf(stdout, "%s%s\n", *p++, "yes");
		    } else {
			p++;
		    }
	    }
	    if (inq.ui.inq_linked) {
		(void) fprintf(stdout, "%s%s\n", *p++, "yes");
	    } else {
		p++;
	    }
	    if (inq.ui.inq_cmdque) {
		(void) fprintf(stdout, "%s%s\n", *p++, "yes");
	    } else {
		p++;
	    }
	    if (scsi_3) {
		(void) fprintf(stdout,
			"%s%s\n", *p++, inq.ui.inq_trandis ? "yes" : "no");
	    }
	    if (!scsi_3) {
		    if (inq.ui.inq_sftre) {
			(void) fprintf(stdout, "%s%s\n", *p++, "yes");
		    } else {
			p++;
		    }
	    }

	/*
	 * Now print the vendor-specific data
	 */
	    v_parm = inq.inq_ven_specific_1;
	    if (inq.inq_len >= 32) {
		length = inq.inq_len - 31;
		if (strstr((char *)inq.inq_pid, "SSA") != 0) {
		(void) fprintf(stdout, "Number of Ports, Targets:   %d,%d\n",
			inq.inq_ssa_ports, inq.inq_ssa_tgts);
			v_parm += 20;
			length -= 20;
		} else if ((strstr((char *)inq.inq_pid, "SUN") != 0) ||
			(strncmp((char *)inq.inq_vid, "SUN     ",
			sizeof (inq.inq_vid)) == 0)) {
			v_parm += 16;
			length -= 16;
		}
		/*
		 * Do hex Dump of rest of the data.
		 *
		 */
		if (length > 0) {
			(void) fprintf(stdout,
				"              VENDOR-SPECIFIC PARAMETERS\n");
			(void) fprintf(stdout,
				"Byte#                  Hex Value            "
					"                 ASCII\n");
			(void) sprintf(byte_number,
			"%d    ", inq.inq_len - length + 4);
			p_dump(byte_number, v_parm,
				min(length, inq.inq_res3 - v_parm),
				HEX_ASCII);
		}
		/*
		 * Skip reserved bytes 56-95.
		 */
		length -= (inq.inq_box_name - v_parm);
		if (length > 0) {
			(void) sprintf(byte_number,
			"%d    ", inq.inq_len - length + 4);
			p_dump(byte_number, inq.inq_box_name,
				length, HEX_ASCII);
		}
	    }
	    }
	    path_index++;
	}
}

static void
adm_start(char **argv, int option_t_input)
{
char		*path_phys;
Path_struct	*path_struct;

	if (Options & OPTION_T) {
		ssa_cli_start(&(*argv), option_t_input);
	} else {
	    while (*argv != NULL) {
		if ((path_phys = l_convert_name(*argv,
			&path_struct, Options & PVERBOSE)) == NULL) {
			L_ERR_PRINT;
			(argv)++; continue;
		}
		VERBPRINT("  Issuing start to:\n %s\n", *argv);
		if (l_start(path_phys))  {
		    L_ERR_PRINT;
		    (argv)++; continue;
		}
		(argv)++;
	    }
	}
}

static void
adm_stop(char **argv, int option_t_input)
{
char		*path_phys;
Path_struct	*path_struct;

	if (Options & OPTION_T) {
		ssa_cli_stop(&(*argv), option_t_input);
	} else {
	    while (*argv != NULL) {
		if ((path_phys = l_convert_name(*argv,
			&path_struct, Options & PVERBOSE)) == NULL) {
			L_ERR_PRINT;
			(argv)++; continue;
		}
		VERBPRINT("  Issuing stop to:\n %s\n", *argv);
		if (l_stop(path_phys, 1))  {
		    L_ERR_PRINT;
		    (argv)++; continue;
		}
		(argv)++;
	    }
	}
}


/*
 * Definition of getaction() routine which does keyword parsing
 *
 * Operation: A character string containing the ascii cmd to be
 * parsed is passed in along with an array of structures.
 * The 1st struct element is a recognizable cmd string, the second
 * is the minimum number of characters from the start of this string
 * to succeed on a match. For example, { "offline", 3, ONLINE }
 * will match "off", "offli", "offline", but not "of" nor "offlinebarf"
 * The third element is the {usually but not necessarily unique}
 * integer to return on a successful match. Note: compares are cAsE insensitive.
 *
 * To change, extend or use this utility, just add or remove appropriate
 * lines in the structure initializer below and in the #define	s for the
 * return values.
 *
 *                              N O T E
 * Do not change the minimum number of characters to produce
 * a match as someone may be building scripts that use this
 * feature.
 */
struct keyword {
	char *match;		/* Character String to match against */
	int  num_match;		/* Minimum chars to produce a match */
	int  ret_code;		/* Value to return on a match */
};

static  struct keyword Keywords[] = {
	{"display",		2, DISPLAY},
	{"download",		3, DOWNLOAD},
	{"enclosure_names",	2, ENCLOSURE_NAMES},
	{"fast_write",		3, FAST_WRITE},
	{"fcal_s_download",	4, FCAL_UPDATE},
	{"fc_s_download",	4, FC_UPDATE},
	{"inquiry",		2, INQUIRY},
	{"insert_device",	3, INSERT_DEVICE},
	{"led",			3, LED},
	{"led_on",		5, LED_ON},
	{"led_off",		5, LED_OFF},
	{"led_blink",		5, LED_BLINK},
	{"nvram_data",		2, NVRAM_DATA},
	{"password",		2, PASSWORD},
	{"perf_statistics",	2, PERF_STATISTICS},
	{"power_on",		8, POWER_ON},
	{"power_off",		9, POWER_OFF},
	{"probe",		2, PROBE},
	{"purge",		2, PURGE},
	{"remove_device",	3, REMOVE_DEVICE},
	{"reserve",		5, RESERVE},
	{"release",		3, RELEASE},
	{"set_boot_dev",	5, SET_BOOT_DEV},
	{"start",		3, START},
	{"stop",		3, STOP},
	{"sync_cache",		2, SYNC_CACHE},
	{"env_display",		5, ENV_DISPLAY},
	{"alarm",		5, ALARM},
	{"alarm_off",		8, ALARM_OFF},
	{"alarm_on",		8, ALARM_ON},
	{"alarm_set",		9, ALARM_SET},
	{"rdls",		2, RDLS},
	{"p_bypass",		3, P_BYPASS},
	{"p_enable",		3, P_ENABLE},
	{"forcelip",		2, FORCELIP},
	{"dump",		2, DUMP},
	{"check_file",		2, CHECK_FILE},
	{"dump_map",		2, DUMP_MAP},
#ifdef	SSA_HOTPLUG
	/* hotplugging device operations */
	{"online",		2, DEV_ONLINE},
	{"offline",		2, DEV_OFFLINE},
	{"dev_getstate",	5, DEV_GETSTATE},
	{"dev_reset",		5, DEV_RESET},
	/* hotplugging bus operations */
	{"bus_quiesce",		5, BUS_QUIESCE},
	{"bus_unquiesce",	5, BUS_UNQUIESCE},
	{"bus_getstate",	5, BUS_GETSTATE},
	{"bus_reset",		9, BUS_RESET},
	{"bus_resetall",	12, BUS_RESETALL},
	/* hotplugging "helper" subcommands */
	{"replace_device",	3, REPLACE_DEVICE},
#endif	/* SSA_HOTPLUG */
	{ NULL,			0, 0}
};

#ifndef	EOK
static	const	EOK	= 0;	/* errno.h type success return code */
#endif

/*
 * function getaction() takes a character string, cmd, and
 * tries to match it against a passed structure of known cmd
 * character strings. If a match is found, corresponding code
 * is returned in retval. Status returns as follows:
 *   EOK	= Match found, look for cmd's code in retval
 *   EFAULT = One of passed parameters was bad
 *   EINVAL = cmd did not match any in list
 */
static int
getaction(char *cmd, struct keyword *matches, int  *retval)
{
int actlen;

	/* Idiot checking of pointers */
	if (! cmd || ! matches || ! retval ||
	    ! (actlen = strlen(cmd)))	/* Is there an cmd ? */
	    return (EFAULT);

	    /* Keep looping until NULL match string (end of list) */
	    while (matches->match) {
		/*
		 * Precedence: Make sure target is no longer than
		 * current match string
		 * and target is at least as long as
		 * minimum # match chars,
		 * then do case insensitive match
		 * based on actual target size
		 */
		if ((((int)strlen(matches->match)) >= actlen) &&
		    (actlen >= matches->num_match) &&
		    /* can't get strncasecmp to work on SCR4 */
		    /* (strncasecmp(matches->match, cmd, actlen) == 0) */
		    (strncmp(matches->match, cmd, actlen) == 0)) {
		    *retval = matches->ret_code;	/* Found our match */
		    return (EOK);
		} else {
		    matches++;		/* Next match string/struct */
		}
	    }	/* End of matches loop */
	return (EINVAL);

}	/* End of getaction() */


main(int argc, char **argv)
{
register	    c;
/* getopt varbs */
extern char *optarg;
int		path_index;
int		cmd = 0;	    /* Cmd verb from cmd line */
int		exit_code = 0;	    /* exit code for program */
char		*wwn = NULL;
char		*file_name = NULL;
struct		utsname	name;
int		option_t_input;
char		*path_phys;
Path_struct	*path_struct;
char		*get_phys_path;

	whoami = argv[0];
	l_error_msg_ptr = p_error_msg_ptr = NULL;

	/*
	 * Enable locale announcement
	 */
	if (setlocale(LC_ALL, "") == NULL) {
	    (void) fprintf(stderr,
		"%s: Cannot operate in the locale requested. "
		"Continuing in the default C locale\n",
		whoami);
	}
	l_catd = catopen("luxadm_i18n_cat", NL_CAT_LOCALE);

	if (uname(&name) == -1) {
		(void) fprintf(stderr,
		MSGSTR(-1, "uname function failed.\n"));
		exit(-1);
	}
	if ((strcmp(name.machine, L_ARCH_4D) == 0) ||
		(strcmp(name.machine, L_ARCH_4M) == 0)) {
		P_DPRINTF("  Using sun4d /devices names\n");
		name_id = 1;
	} else {
		P_DPRINTF("  Using /devices names with"
			" WWN in the device name.\n");
		name_id = 0;
	}

	while ((c = getopt(argc, argv, "ve"))
	    != EOF) {
	    switch (c) {
		case 'v':
		    Options |= PVERBOSE;
		    break;
		case 'e':
		    Options |= EXPERT;
		    break;
		default:
		    /* Note: getopt prints an error if invalid option */
		    USEAGE()
		    exit(-1);
	    } /* End of switch(c) */
	}


	/*
	 * Get subcommand.
	 */
	if ((getaction(argv[optind], Keywords, &cmd)) == EOK) {
		optind++;
		if ((cmd != PROBE) && (cmd != FCAL_UPDATE) &&
		(cmd != FC_UPDATE) && (cmd != INSERT_DEVICE) &&
		(optind >= argc)) {
			(void) fprintf(stderr,
			MSGSTR(-1,
			"Error: enclosure or pathname not specified\n"));
			USEAGE();
			exit(-1);
		}
	} else {
		(void) fprintf(stderr,
		MSGSTR(-1, "%s: subcommand not specified\n"),
		whoami);
		USEAGE();
		exit(-1);
	}

	/* Extract & Save subcommand options */
	while ((c = getopt(argc, argv, "Fryszaepcdlvt:f:w:"))
	    != EOF) {
	    switch (c) {
		case 'a':
		    Options |= OPTION_A;
		    break;
		case 'c':
		    Options |= OPTION_C;
		    break;
		case 'd':
		    Options |= OPTION_D;
		    break;
		case 'e':
		    Options |= OPTION_E;
		    break;
		case 'f':
		    Options |= OPTION_F;
		    file_name = optarg;
		    break;
		case 'F':
		    Options |= OPTION_CAPF;
		    break;
		case 'l':
		    Options |= OPTION_L;
		    break;
		case 'p':
		    Options |= OPTION_P;
		    break;
		case 'r':
		    Options |= OPTION_R;
		    break;
		case 's':
		    Options |= SAVE;
		    break;
		case 't':
		    Options |= OPTION_T;
		    option_t_input = atoi(optarg);
		    break;
		case 'v':
		    Options |= OPTION_V;
		    break;
		case 'w':
		    Options |= OPTION_W;
		    wwn = optarg;
		    break;
		case 'z':
		    Options |= OPTION_Z;
		    break;
		case 'y':
		    Options |= OPTION_Y;
		    break;
		default:
		    /* Note: getopt prints an error if invalid option */
		    USEAGE()
		    exit(-1);
	    } /* End of switch(c) */
	}
	if ((cmd != PROBE) && (cmd != FCAL_UPDATE) &&
	    (cmd != FC_UPDATE) && (cmd != INSERT_DEVICE) &&
	    (optind >= argc)) {
	    (void) fprintf(stderr,
		MSGSTR(-1,
		"Error: enclosure or pathname not specified\n"));
	    USEAGE();
	    exit(-1);
	}
	path_index = optind;


	switch (cmd)	{
	    case	DISPLAY:
		if (Options &
		    ~(PVERBOSE | OPTION_A | OPTION_Z | OPTION_R |
		    OPTION_P | OPTION_V | OPTION_L | OPTION_E | OPTION_T)) {
		    USEAGE();
		    exit(-1);
		}
		/* Display object(s) */
		adm_display_config(&argv[path_index],
		option_t_input, argc - path_index);
		break;

	    case	DOWNLOAD:
		    if (Options &
			~(PVERBOSE | OPTION_F | OPTION_W | SAVE)) {
			USEAGE();
			exit(-1);
		    }
		    adm_download(&argv[path_index], file_name, wwn);
		    break;

	    case	ENCLOSURE_NAMES:
		    if (Options & ~PVERBOSE) {
			USEAGE();
			exit(-1);
		    }
		    up_encl_name(&argv[path_index], argc);
		    break;

	    case	FAST_WRITE:
		    if (Options &
			~(PVERBOSE | OPTION_E | OPTION_D | OPTION_C | SAVE)) {
		    USEAGE();
		    exit(-1);
		}
		if (!((Options & (OPTION_E | OPTION_D | OPTION_C)) &&
		    !((Options & OPTION_E) &&
		    (Options & (OPTION_D | OPTION_C))) &&
		    !((Options & OPTION_D) && (Options & OPTION_C)))) {
		    USEAGE();
		    exit(-1);
		}
		ssa_fast_write(argv[path_index]);
		break;

	    case	INQUIRY:
		if (Options & ~(PVERBOSE)) {
			USEAGE();
			exit(-1);
		}
		adm_inquiry(&argv[path_index]);
		break;

	    case 	NVRAM_DATA:
		if (Options & ~(PVERBOSE)) {
			USEAGE();
			exit(-1);
		}
		while (argv[path_index] != NULL) {
			if ((get_phys_path =
			    get_physical_name(argv[path_index])) == NULL) {
			    (void) fprintf(stderr,
				"%s: Invalid path name (%s)\n", whoami,
				argv[path_index]);
			    exit(-1);
			}
			ssa_cli_display_config(&argv[path_index], get_phys_path,
				option_t_input, 1, argc - path_index);
			path_index++;
		}
		break;

	    case	PERF_STATISTICS:
		if (Options & ~(PVERBOSE | OPTION_D | OPTION_E)) {
		    USEAGE();
		    exit(-1);
		}
		if (!((Options & (OPTION_E | OPTION_D)) &&
		    !((Options & OPTION_E) && (Options & OPTION_D)))) {
		    USEAGE();
		    exit(-1);
		}
		(void) ssa_perf_statistics(argv[path_index]);
		break;

	    case	PURGE:
		if (Options & ~(PVERBOSE)) {
			USEAGE();
			exit(-1);
		}
		VERBPRINT("Throwing away all data in the NV_RAM for:\n %s\n",
		    argv[path_index]);
		if ((get_phys_path =
		    get_physical_name(argv[path_index])) == NULL) {
		    (void) fprintf(stderr,
			"%s: Invalid path name (%s)\n", whoami,
			argv[path_index]);
		    exit(-1);
		}
		if (p_purge(get_phys_path)) {
		    L_ERR_PRINT;
		    exit(-1);
		}
		break;

	    case	PROBE:
		if (Options & ~(PVERBOSE | OPTION_P)) {
			USEAGE();
			exit(-1);
		}
		/*
		 * A special check just in case someone entered
		 * any characters after the -p or the probe.
		 *
		 * (I know, a nit.)
		 */
		if (((Options & PVERBOSE) && (Options & OPTION_P) &&
			(argc != 4)) ||
			(!(Options & PVERBOSE) && (Options & OPTION_P) &&
			(argc != 3)) ||
			((Options & PVERBOSE) && (!(Options & OPTION_P)) &&
			(argc != 3)) ||
			(!(Options & PVERBOSE) && (!(Options & OPTION_P)) &&
			(argc != 2))) {
			(void) fprintf(stderr,
			MSGSTR(-1, "Error: Incorrect number of arguments.\n"));
			(void) fprintf(stderr,  MSGSTR(-1,
			"Usage: %s [-v] subcommand [option]\n"), whoami);
			exit(-1);
		}
		fc_probe();
		break;

	    case	FCAL_UPDATE:	/* Update Fcode in Sbus soc+ card */
			if ((Options & ~(PVERBOSE)) & ~(OPTION_F) ||
					argv[path_index]) {
				USEAGE();
				exit(-1);
			}
			(void) fcal_update(Options & PVERBOSE, file_name);
			break;

	    case	FC_UPDATE:	/* Update Fcode in Sbus soc card */
			if (((Options & ~(PVERBOSE)) & ~(OPTION_F) &
				~(OPTION_CAPF)) || argv[path_index]) {
				USEAGE();
				exit(-1);
			}
			(void) fc_update(Options & PVERBOSE,
				Options & OPTION_CAPF, file_name);
		break;

	    case	SET_BOOT_DEV:   /* Set boot-device variable in nvram */
			exit_code = setboot(Options & OPTION_Y,
				Options & PVERBOSE, argv[path_index]);
		break;

	    case	LED:
		if (Options & ~(PVERBOSE)) {
			USEAGE();
			exit(-1);
		}
		adm_led(&argv[path_index], L_LED_STATUS);
		break;
	    case	LED_ON:
		if (Options & ~(PVERBOSE)) {
			USEAGE();
			exit(-1);
		}
		adm_led(&argv[path_index], L_LED_ON);
		break;
	    case	LED_OFF:
		if (Options & ~(PVERBOSE)) {
			USEAGE();
			exit(-1);
		}
		adm_led(&argv[path_index], L_LED_OFF);
		break;
	    case	LED_BLINK:
		if (Options & ~(PVERBOSE)) {
			USEAGE();
			exit(-1);
		}
		adm_led(&argv[path_index], L_LED_RQST_IDENTIFY);
		break;
	    case	PASSWORD:
		if (Options & ~(PVERBOSE))  {
			USEAGE();
			exit(-1);
		}
		up_password(&argv[path_index]);
		break;

	    case	RESERVE:
		if (Options & (~PVERBOSE)) {
			USEAGE();
			exit(-1);
		}
		VERBPRINT("  Reserving: \n %s\n", argv[path_index]);
		if ((path_phys =
		    get_physical_name(argv[path_index])) == NULL) {
		    (void) fprintf(stderr,
			"%s: Invalid path name (%s)\n", whoami,
			argv[path_index]);
		    exit(-1);
		}
		if (l_reserve(path_phys)) {
		    L_ERR_PRINT;
		    exit(-1);
		}
		break;

	    case	RELEASE:
		if (Options & (~PVERBOSE)) {
			USEAGE();
			exit(-1);
		}
		VERBPRINT("  Canceling Reservation for:\n %s\n",
		    argv[path_index]);
		if ((path_phys =
		    get_physical_name(argv[path_index])) == NULL) {
		    (void) fprintf(stderr,
			"%s: Invalid path name (%s)\n", whoami,
			argv[path_index]);
		    exit(-1);
		}
		if (l_release(path_phys)) {
		    L_ERR_PRINT;
		    exit(-1);
		}
		break;

	    case	START:
		if (Options & ~(PVERBOSE | OPTION_T)) {
			USEAGE();
			exit(-1);
		}
		adm_start(&argv[path_index], option_t_input);
		break;

	    case	STOP:
		if (Options & ~(PVERBOSE | OPTION_T)) {
			USEAGE();
			exit(-1);
		}
		adm_stop(&argv[path_index], option_t_input);
		break;
	    case	SYNC_CACHE:
		if (Options & ~(PVERBOSE)) {
			USEAGE();
			exit(-1);
		}
		VERBPRINT("Flushing the NV_RAM buffer of "
		    "all writes for:\n %s\n",
		    argv[path_index]);
		if ((get_phys_path =
		    get_physical_name(argv[path_index])) == NULL) {
		    (void) fprintf(stderr,
			"%s: Invalid path name (%s)\n", whoami,
			argv[path_index]);
		    exit(-1);
		}
		if (p_sync_cache(get_phys_path)) {
		    L_ERR_PRINT;
		    exit(-1);
		}
		break;
	    case	ENV_DISPLAY:
		cli_display_envsen_data(&argv[path_index], argc - path_index);
		break;
	    case	ALARM:
		alarm_enable(&argv[path_index], 0, argc - path_index);
		break;
	    case	ALARM_OFF:
		alarm_enable(&argv[path_index], OPTION_D, argc - path_index);
		break;
	    case	ALARM_ON:
		alarm_enable(&argv[path_index], OPTION_E, argc - path_index);
		break;
	    case	ALARM_SET:
		alarm_set(&argv[path_index], argc - path_index);
		break;
	    case	POWER_OFF:
		if (Options & (~PVERBOSE)) {
			USEAGE();
			exit(-1);
		}
		adm_power_off(&argv[path_index], argc - path_index, 1);
		break;

	    case	POWER_ON:
		if (Options & (~PVERBOSE)) {
			USEAGE();
			exit(-1);
		}
		adm_power_off(&argv[path_index], argc - path_index, 0);
		break;

	/*
	 * EXPERT commands.
	 */

	    case	FORCELIP:
		if (!(Options & EXPERT) || (Options & ~(PVERBOSE | EXPERT))) {
			E_USEAGE();
			exit(-1);
		}
		if ((path_phys = l_convert_name(argv[path_index],
			&path_struct, Options & PVERBOSE)) == NULL) {
			L_ERR_PRINT;
			exit(-1);
		}
		if (l_force_lip(path_phys, Options & PVERBOSE)) {
		    L_ERR_PRINT;
		    exit(-1);
		}
		break;

	    case	P_BYPASS:
		if (!(Options & EXPERT) || (Options & ~(PVERBOSE | EXPERT))) {
			E_USEAGE();
			exit(-1);
		}
		if ((path_phys = l_convert_name(argv[path_index],
			&path_struct, Options & PVERBOSE)) == NULL) {
			L_ERR_PRINT;
			exit(-1);
		}
		if (l_bypass(path_phys, Options & PVERBOSE)) {
		    L_ERR_PRINT;
		    exit(-1);
		}
		break;

	    case	P_ENABLE:
		if (!(Options & EXPERT) || (Options & ~(PVERBOSE | EXPERT))) {
			E_USEAGE();
			exit(-1);
		}
		if ((path_phys = l_convert_name(argv[path_index],
			&path_struct, Options & PVERBOSE)) == NULL) {
			L_ERR_PRINT;
			exit(-1);
		}
		if (l_enable(path_phys, Options & PVERBOSE)) {
		    L_ERR_PRINT;
		    exit(-1);
		}
		break;


	    case	RDLS:
		if (!(Options & EXPERT) || (Options & ~(PVERBOSE | EXPERT))) {
			E_USEAGE();
			exit(-1);
		}
		display_link_status(&argv[path_index]);
		break;

	/*
	 * Undocumented commands.
	 */

	    case	CHECK_FILE:	    /* Undocumented Cmd */
		if (Options & ~(PVERBOSE)) {
			USEAGE();
			exit(-1);
		}
		/* check & display download file parameters */
		if (l_check_file(argv[path_index],
		    (Options & PVERBOSE))) {
		    L_ERR_PRINT;
		    exit(-1);
		}
		(void) fprintf(stdout, "Download file O.K. \n\n");
		break;

	    case	DUMP:		/* Undocumented Cmd */
		if (!(Options & EXPERT) || (Options & ~(PVERBOSE | EXPERT))) {
			USEAGE();
			exit(-1);
		}
		dump(&argv[path_index]);
		break;

	    case	DUMP_MAP:	/* Undocumented Cmd */
		if (!(Options & EXPERT) || (Options & ~(PVERBOSE | EXPERT))) {
			USEAGE();
			exit(-1);
		}
		dump_map(&argv[path_index]);
		break;


	/*
	 *  From here to the end of the module is mostly
	 *  hot-plug related code.
	 */
	    case	INSERT_DEVICE:
			if (l_hotplug(INSERT_DEVICE, &argv[path_index],
				(Options & PVERBOSE))) {
			    L_ERR_PRINT;
			    exit(-1);
			}
			break;
	    case	REMOVE_DEVICE:
			if (l_hotplug(REMOVE_DEVICE, &argv[path_index],
				(Options & PVERBOSE))) {
			    L_ERR_PRINT;
			    exit(-1);
			}
			break;

#ifdef	SSA_HOTPLUG
	case	REPLACE_DEVICE:
		if (l_hotplug(RPL_DEVICE, &argv[path_index],
		    (Options & PVERBOSE))) {
			L_ERR_PRINT;
			exit(-1);
		}
		break;

	/* for hotplug device operations */
	case DEV_ONLINE:
	case DEV_OFFLINE:
	case DEV_GETSTATE:
	case DEV_RESET:
	case BUS_QUIESCE:
	case BUS_UNQUIESCE:
	case BUS_GETSTATE:
	case BUS_RESET:
	case BUS_RESETALL:
		if (l_hotplug_e(cmd, &argv[path_index],
		    (Options & PVERBOSE)) != 0) {
			L_ERR_PRINT;
			return (-1);
		}
		break;
#endif	/* SSA_HOTPLUG */

	    default:
		(void) fprintf(stderr,
		    MSGSTR(-1, "%s: subcommand decode failed\n"),
		    whoami);
		USEAGE();
		exit(-1);
	}
	return (exit_code);
}


/*
 * handle helper-mode hotplug commands
 *
 * XXX: always returns 0
 */
static int
l_hotplug(int todo, char **argv, int verbose_flag)
{
	char			ses_path[MAXPATHLEN], dev_path[MAXPATHLEN];
	struct l_state_struct l_state;
	sf_al_map_t		map;
	struct hotplug_disk_list *disk_list, *disk_list_head, *disk_list_tail;
	int			tid, slot, path_index = 0, dtype;
	Path_struct		*path_struct;
	char			*path_phys;
	u_char			node_wwn[WWN_SIZE];
	u_char			port_wwn[WWN_SIZE];
	int			al_pa;
	struct box_list_struct *box_list;
	int			i;
	char			code;

	disk_list_head = disk_list_tail = (struct hotplug_disk_list *)NULL;

#ifdef	DEBUG
	(void) fprintf(stderr, "DEBUG: l_hotplug(): entering for \"%s\" ...\n",
	    argv[0] ? argv[0] : "<null ptr>");
#endif
	l_get_box_list(&box_list, 0);

	if (todo == REMOVE_DEVICE) {
		(void) fprintf(stdout,
		    "\n\n  WARNING!!! Please ensure that no "
			"filesystems are mounted ");
		(void) fprintf(stdout, "on these device(s).\n  All data on"
			" these devices should have been backed up.\n\n");
	}

	/*
	 * check for an insert action with no argument, which implies user
	 * wants to insert a whole photon box
	 */
	if ((todo == INSERT_DEVICE) && (argv[path_index] == NULL)) {
		l_insert_photon();
		return (0);
	}

	/*
	 * at this point user want to insert or remove one or more
	 * pathnames they've specified
	 */
	while (argv[path_index] != NULL) {
		if ((path_phys = l_convert_name(argv[path_index],
			&path_struct, Options & PVERBOSE)) == NULL) {
			/* Make sure we have a device path. */
			if (path_struct->ib_path_flag) {
				path_phys = path_struct->p_physical_path;
			} else {
				L_ERR_PRINT;
				exit(-1);
			}
		}
		if (path_struct->slot_valid ||
			strstr(path_phys, "ssd")) {
			dtype = DTYPE_DIRECT;
		} else {
			dtype = DTYPE_ESI;
		}

		if (l_get_dev_map(path_phys, &map, verbose_flag) != 0) {
#ifdef	SSA_HOTPLUG
			int	(*hpf)(char *);

			/*
			 * since we can't get the dev map perhaps we have
			 * a non-photon (e.g. Tabasco, ...)
			 */
			switch (todo) {
				case INSERT_DEVICE:
					hpf = dev_handle_insert;
					break;
				case REMOVE_DEVICE:
					hpf = dev_handle_remove;
					break;
				case RPL_DEVICE:
					hpf = dev_handle_replace;
					break;
				default:
					(void) fprintf(stderr,
						MSGSTR(-1,
						"Error:"
						" Could"
						" not get map for %s\n"),
						path_phys);
					exit(-1);
			} /* End of switch(c) */
			(void) (*hpf)(path_phys);
#else	/* SSA_HOTPLUG */
			(void) fprintf(stderr,
				MSGSTR(-1,
				"Error:"
				" Could"
				" not get map for %s\n"), path_phys);
#endif	/* SSA_HOTPLUG */
			exit(-1);
		}

#ifdef	SSA_HOTPLUG
		if (todo == RPL_DEVICE) {
			(void) fprintf(stderr,
				MSGSTR(-1,
				"Error:"
				" replace_device is not supported"
				" on this subsystem.\n"));
			exit(-1);
		}
#endif	/* SSA_HOTPLUG */

		strcpy(ses_path, path_phys);

		if (strstr(ses_path, "ses") == NULL &&
			l_get_ses_path(path_phys, ses_path, &map,
				verbose_flag)) {
			(void) fprintf(stderr,
				MSGSTR(-1,
				"Error:"
				" Could not obtain path "
				"to ses device\n"));
			exit(-1);
		}

		if (l_get_status(ses_path, &l_state, verbose_flag)) {
			L_ERR_PRINT;
			exit(-1);
		}

		if (dtype == DTYPE_ESI) {
			/* could be removing a photon */
			if (todo == REMOVE_DEVICE) {
				/*
				 * Need the select ID (tid) for the IB.
				 */
				if (l_get_wwn(ses_path, port_wwn, node_wwn,
					&al_pa, verbose_flag)) {
					L_ERR_PRINT;
					exit(-1);
				}
				tid = l_sf_alpa_to_switch[al_pa];
				*dev_path = '\0';
				/*
				 * Check if any disk in this photon
				 * is reserved by another host
				 */
				for (i = 0; i < l_state.total_num_drv/2; i++) {
			if ((l_state.drv_front[i].d_state_flags[PORT_A] &
						L_RESERVED) ||
				(l_state.drv_front[i].d_state_flags[PORT_B] &
						L_RESERVED) ||
				(l_state.drv_rear[i].d_state_flags[PORT_A] &
						L_RESERVED) ||
				(l_state.drv_rear[i].d_state_flags[PORT_B] &
						L_RESERVED)) {
						(void) fprintf(stderr,
						MSGSTR(-1,
						"Error:"
						" One or more disks in"
						" this %s are reserved.\n"),
						ENCLOSURE_PROD_NAME);
						exit(-1);
					}
				}
				goto l1;
			}
			(void) fprintf(stderr,
				MSGSTR(-1,
				"Error:"
				" %s"
				" already exists!!\n"), argv[path_index]);
			exit(-1);
		}

		if (!path_struct->slot_valid) {
			/* We are passing the disks path */
			if (l_get_slot(path_struct, verbose_flag)) {
				L_ERR_PRINT;
				exit(-1);
			}
		}

		slot = path_struct->slot;
		if (path_struct->f_flag) {
			tid = l_state.drv_front[slot].ib_status.sel_id;
			code = l_state.drv_front[slot].ib_status.code;
		} else {
			tid = l_state.drv_rear[slot].ib_status.sel_id;
			code = l_state.drv_rear[slot].ib_status.code;
		}

		/* Double check the slot as convert_name does a gross check */
		if ((slot >= l_state.total_num_drv/2) ||
			(slot < 0)) {
			(void) fprintf(stderr,
				MSGSTR(-1,
				"Error:"
				" invalid slot number %d\n"), slot);
			exit(-1);
		}

		make_node(ses_path, tid, dev_path, &map, 0);

		if (todo == INSERT_DEVICE && name_id == 0) {
			/*
			 * Since we are inserting a drive and the physical
			 * name has WWN in it, we cannot determine it at
			 * this time
			 */
			dev_path[0] = '\0';
		}

		if ((todo == INSERT_DEVICE) &&
			(device_in_map(&map, tid) ||
			(code != S_NOT_INSTALLED))) {
			(void) fprintf(stdout,
				MSGSTR(-1, "\nNotice: %s may "
				"already be present.\n"),
				argv[path_index]);
		}

		/* check if disk is reserved */
		if (todo == REMOVE_DEVICE) {
			if (path_struct->f_flag) {
			if ((l_state.drv_front[slot].d_state_flags[PORT_A] &
					L_RESERVED) ||
				(l_state.drv_front[slot].d_state_flags[PORT_B] &
						L_RESERVED)) {
					(void) fprintf(stderr,
						MSGSTR(-1,
						"Error:"
						" Disk %s is reserved\n"),
						argv[path_index]);
					exit(-1);
				}
			} else {
			if ((l_state.drv_rear[slot].d_state_flags[PORT_A] &
					L_RESERVED) ||
				(l_state.drv_rear[slot].d_state_flags[PORT_B] &
						L_RESERVED)) {
					(void) fprintf(stderr,
						MSGSTR(-1,
						"Error:"
						" Disk %s is reserved\n"),
						argv[path_index]);
					exit(-1);
				}
			}
		}

l1:
		if (!disk_list_head) {
			disk_list = (struct hotplug_disk_list *)
				zalloc(sizeof (struct hotplug_disk_list));
			disk_list->seslist = l_get_allses(ses_path,
						box_list, 0);
			disk_list->dlhead = l_get_multipath(dev_path, 0);
			strcpy(disk_list->box_name,
					(char *)l_state.ib_tbl.enclosure_name);
			disk_list->tid = tid;
			disk_list->dev_type = dtype;
			disk_list->slot = slot;
			disk_list->f_flag = path_struct->f_flag;
			disk_list_head = disk_list_tail = disk_list;
		} else {
			disk_list = (struct hotplug_disk_list *)
				zalloc(sizeof (struct hotplug_disk_list));
			disk_list->seslist = l_get_allses(ses_path,
							box_list, 0);
			disk_list->dlhead = l_get_multipath(dev_path, 0);
			strcpy(disk_list->box_name,
					(char *)l_state.ib_tbl.enclosure_name);
			disk_list->tid = tid;
			disk_list->dev_type = dtype;
			disk_list->slot = slot;
			disk_list->f_flag = path_struct->f_flag;
			disk_list->prev = disk_list_tail;
			disk_list_tail->next = disk_list;
			disk_list_tail = disk_list;
		}

		path_index++;
	}

	if (disk_list_head) {

		if (print_list_warn(disk_list_head, todo)) {
			return (0);
		}
		l_pre_hotplug(&disk_list_head, todo, verbose_flag);
		if (disk_list_head) {
			(void) fprintf(stdout,
			MSGSTR(-1, "\nHit <Return> after "
			"%s the device(s)."),
			(todo == REMOVE_DEVICE) ? "removing" :
			"inserting");
			(void) getchar();
			(void) fprintf(stdout, "\n");
			return (l_post_hotplug(disk_list_head, todo,
					verbose_flag));
		}
	}
	l_free_box_list(&box_list);
	return (0);

}


#ifdef	SSA_HOTPLUG

/*
 * handle expert-mode hotplug commands
 *
 * return 0 iff all is okay
 */
static int
l_hotplug_e(int todo, char **argv, int verbose_flag)
{
	char		*path_phys = NULL;
	int		exit_code;
	devctl_hdl_t	dcp;
	uint_t		devstate;



	if (!(Options & EXPERT)) {
		USEAGE();
		return (1);
	}

	switch (todo) {
	case DEV_ONLINE:
	case DEV_OFFLINE:
	case DEV_GETSTATE:
	case DEV_RESET:
		/* get physical name */
		if ((path_phys = get_physical_name(argv[0])) == NULL) {
			(void) fprintf(stderr, "%s: Invalid path name (%s)\n",
			    whoami, argv[0]);
			return (1);
		}

		VERBPRINT("phys path = \"%s\"\n", path_phys);

		/* acquire rights to hack on device */
		if (devctl_acquire(path_phys, DC_EXCL, &dcp) != 0) {
			(void) fprintf(stderr,
			    "%s: can't acquire \"%s\": %s\n", whoami,
			    path_phys, strerror(errno));
			return (1);
		}

		switch (todo) {
		case DEV_ONLINE:
			exit_code = devctl_device_online(dcp);
			break;
		case DEV_OFFLINE:
			exit_code = devctl_device_offline(dcp);
			break;
		case DEV_GETSTATE:
			if ((exit_code = devctl_device_getstate(dcp,
			    &devstate)) == 0) {
				print_dev_state(argv[0], devstate);
			}
			break;
		case DEV_RESET:
			exit_code = devctl_device_reset(dcp);
			break;
		}

		if (exit_code != 0) {
			perror("devctl");
		}

		/* all done now -- release device */
		devctl_release(dcp);
		break;

	/* for hotplugging bus operations */
	case BUS_QUIESCE:
	case BUS_UNQUIESCE:
	case BUS_GETSTATE:
	case BUS_RESET:
	case BUS_RESETALL:
		/* get physical name */
		if ((path_phys = get_dev_or_bus_phys_name(argv[0])) ==
		    NULL) {
			(void) fprintf(stderr, "%s: Invalid path name (%s)\n",
			    whoami, argv[0]);
			return (1);
		}
		VERBPRINT("phys path = \"%s\"\n", path_phys);

		/* acquire rights to hack on device */
		if (devctl_acquire(path_phys, DC_EXCL, &dcp) != 0) {
			(void) fprintf(stderr,
			    "%s: can't acquire \"%s\": %s\n", whoami,
			    path_phys, strerror(errno));
			return (1);
		}

		switch (todo) {
		case BUS_QUIESCE:
			exit_code = devctl_bus_quiesce(dcp);
			break;
		case BUS_UNQUIESCE:
			exit_code = devctl_bus_unquiesce(dcp);
			break;
		case BUS_GETSTATE:
			if ((exit_code = devctl_bus_getstate(dcp,
			    &devstate)) == 0) {
				print_bus_state(argv[0], devstate);
			}
			break;
		case BUS_RESET:
			exit_code = devctl_bus_reset(dcp);
			break;
		case BUS_RESETALL:
			exit_code = devctl_bus_resetall(dcp);
			break;
		}

		if (exit_code != 0) {
			perror("devctl");
		}

		/* all done now -- release device */
		devctl_release(dcp);
		break;
	}

	return (exit_code);
}
#endif	/* SSA_HOTPLUG */


static void
l_pre_hotplug(struct hotplug_disk_list **disk_list_head_ptr, int todo,
			int verbose_flag)
{
char *ses_path, *dev_path, code;
int slot, f_r;
char	device_name[MAXNAMELEN];
struct l_state_struct l_state;
struct dlist	*dl;
struct hotplug_disk_list *l1, *disk_list_head = *disk_list_head_ptr;
int	state_a, state_b, a_and_b;

	while (disk_list_head) {

		if (disk_list_head->dev_type == DTYPE_ESI) {
			/* entire photon is being removed */
			if (l_offline_photon(disk_list_head)) {
				L_ERR_PRINT;
				goto delete;
			}
			goto next;
		}

		dl = disk_list_head->seslist;
		while (dl) {
			ses_path = dl->dev_path;
			if (l_get_status(ses_path, &l_state, verbose_flag) == 0)
				break;
			dl = dl->next;
		}

		if (dl == NULL) {
			(void) fprintf(stderr,
				MSGSTR(-1,
				"Error:"
				" get_status failed.\n"));
			exit(-1);
		}

		f_r = disk_list_head->f_flag;
		slot = disk_list_head->slot;
		l_get_drive_name(device_name, slot,
			f_r, disk_list_head->box_name);

		switch (todo) {
			case INSERT_DEVICE:
				if (disk_list_head->f_flag) {
					code =
					l_state.drv_front[slot].ib_status.code;
				} else {
					code =
					l_state.drv_rear[slot].ib_status.code;
				}
				if (code & S_NOT_INSTALLED) {
				/*
				 * At this point we know that the drive is not
				 * there. Turn on the RQST INSERT bit to make
				 * the LED blink
				 */
					if (encl_status_page_funcs
						(SET_RQST_INSRT, 0, todo,
						ses_path, &l_state, f_r, slot,
						verbose_flag)) {
						L_WARNING_PRINT;
						(void) fprintf(stdout,
							MSGSTR(-1,
							" %s: could not turn "
							"on LED\n"),
							device_name);
					}
				} else {
					/*
					 * Drive is there so start it.
					 */
					if (encl_status_page_funcs
						(SET_DRV_ON, 0, todo,
						ses_path, &l_state, f_r, slot,
						verbose_flag)) {
						L_WARNING_PRINT;
						(void) fprintf(stdout,
							MSGSTR(-1,
							" could not enable"
							" %s\n"),
							device_name);
					}
				}

				break;
			case REMOVE_DEVICE:
				if (disk_list_head->f_flag) {
					if (
					l_state.drv_front[slot].ib_status.code
					== S_NOT_INSTALLED) {
						(void) fprintf(stderr,
							MSGSTR(-1,
							"Notice:"
							" %s may already "
							"be removed.\n"),
							device_name);
						goto next;
					}
				} else if (
					l_state.drv_rear[slot].ib_status.code
					== S_NOT_INSTALLED) {
					(void) fprintf(stderr,
						MSGSTR(-1,
						"Notice:"
						" %s may already "
						"be removed.\n"),
						device_name);
					goto next;
				}

				if (disk_list_head->f_flag) {
					state_a =
				l_state.drv_front[slot].d_state_flags[PORT_A];
					state_b =
				l_state.drv_front[slot].d_state_flags[PORT_B];
					a_and_b = state_a & state_b;

					if (!(a_and_b & L_NO_LOOP)) {
						if (state_a & L_NO_LOOP) {
							state_a = 0;
						}
						if (state_b & L_NO_LOOP) {
							state_b = 0;
						}
					}
					if (!(a_and_b &
						(L_NOT_READY | L_NOT_READABLE |
						L_SPUN_DWN_D | L_OPEN_FAIL |
						L_SCSI_ERR | L_NO_LOOP))) {
						if (l_ex_open_test(
							disk_list_head->
							dlhead,
							device_name)) {
							L_ERR_PRINT;
							goto delete;
						}
					} else if (!((state_a || state_b) &
						(L_NOT_READY | L_NOT_READABLE |
						L_SPUN_DWN_D | L_OPEN_FAIL |
						L_SCSI_ERR | L_NO_LOOP))) {
						if (l_ex_open_test(
							disk_list_head->
							dlhead,
							device_name)) {
							L_ERR_PRINT;
							goto delete;
						}
					}
				} else {
					state_a =
				l_state.drv_rear[slot].d_state_flags[PORT_A];
					state_b =
				l_state.drv_rear[slot].d_state_flags[PORT_B];
					a_and_b = state_a & state_b;

					if (!(a_and_b & L_NO_LOOP)) {
						if (state_a & L_NO_LOOP) {
							state_a = 0;
						}
						if (state_b & L_NO_LOOP) {
							state_b = 0;
						}
					}
					if (a_and_b & (!(a_and_b &
						(L_NOT_READY | L_NOT_READABLE |
						L_SPUN_DWN_D | L_OPEN_FAIL |
						L_SCSI_ERR | L_NO_LOOP)))) {
						if (l_ex_open_test(
							disk_list_head->
							dlhead,
							device_name)) {
							L_ERR_PRINT;
							goto delete;
						}
					} else if (!((state_a || state_b) &
						(L_NOT_READY | L_NOT_READABLE |
						L_SPUN_DWN_D | L_OPEN_FAIL |
						L_SCSI_ERR | L_NO_LOOP))) {
						if (l_ex_open_test(
							disk_list_head->
							dlhead,
							device_name)) {
							L_ERR_PRINT;
							goto delete;
						}
					}
				}
				if (disk_list_head->dlhead == NULL) {
					dev_path = NULL;
				} else {
					dev_path =
					disk_list_head->dlhead->dev_path;
				}
				if (l_dev_stop(dev_path,
					device_name, 0)) {
					L_ERR_PRINT;
					goto delete;
				}
				if (l_offline_drive(disk_list_head->dlhead,
							device_name)) {
					L_ERR_PRINT;
					l_online_drive(disk_list_head->dlhead,
							device_name);
					(void) l_dev_start(dev_path,
						device_name, 0);
					goto delete;
				}
				/*
				 * Take the drive off the loop
				 * and blink the LED.
				 */
				if (encl_status_page_funcs
					(SET_RQST_RMV, 0, todo,
					ses_path, &l_state, f_r, slot,
					verbose_flag)) {
					L_WARNING_PRINT;
					(void) fprintf(stdout,
						MSGSTR(-1,
						" %s: could not blink"
						" the yellow LED\n"),
						device_name);
				}
				break;
		}
next:
		disk_list_head = disk_list_head->next;
		continue;
delete:
		l1 = disk_list_head->prev;
		if (l1) {
			l1->next = disk_list_head->next;
			if (l1->next)
				l1->next->prev = l1;
		}
		l1 = disk_list_head;
		disk_list_head = disk_list_head->next;
		if (l1 == *disk_list_head_ptr)
			*disk_list_head_ptr = disk_list_head;
		l_free_multipath(l1->seslist);
		l_free_multipath(l1->dlhead);
		free(l1);
	}
}
static int
l_post_hotplug(struct hotplug_disk_list *disk_list_head, int todo,
		int verbose_flag)
{
	char *ses_path, *dev_path, device_name[MAXNAMELEN];
	int tid, slot, err, f_r, al_pa, timeout = 0;
	struct l_state_struct l_state;
	sf_al_map_t map;
	struct hotplug_disk_list *d1;
	struct dlist	*dl, *dl1;
	u_char		port_wwn[WWN_SIZE];
	u_char		node_wwn[WWN_SIZE];
	char		code;
	int		wait_spinup_flag = 0;
	int		wait_map_flag = 0;
	int		wait_node_flag = 0;

	/* Do a lip on every loop so that we get the latest loop maps */
	if (todo != INSERT_DEVICE) {
		l_forcelip_all(disk_list_head);
	}

	/*
	 * Version 0.16 of the SES firmware powers up disks in front/back pairs.
	 * However, the first disk inserted is usually spun up by itself, so
	 * we need to calculate a timeout for 22/2 + 1 = 12 disks.
	 *
	 * Measured times are about 40s/disk for a total of 40*12=8 min total
	 * The timeout assumes 10s/iteration or 4*12*10=8 min
	 */
#define	PHOTON_SPINUP_TIMEOUT	(4*12)
#define	PHOTON_SPINUP_DELAY	10

	while (disk_list_head) {

		dl = disk_list_head->seslist;
		tid = disk_list_head->tid;
		slot = disk_list_head->slot;
		f_r = disk_list_head->f_flag;

		if (disk_list_head->dev_type == DTYPE_ESI) {
			/*
			 * See if photon has really been removed. If not,
			 * try onlining the devices if applicable
			 */
			H_DPRINTF("  l_post_hotplug: Seeing if enclosure "
				"has really been removed:\n"
				"  tid=0x%x, ses_path %s\n",
				tid, dl->dev_path);
			while (dl) {
				ses_path = dl->dev_path;
				if (l_get_dev_map(ses_path, &map, 0) == 0) {
					if (l_get_wwn(ses_path, port_wwn,
						node_wwn, &al_pa,
						verbose_flag) == 0) {
						tid =
						l_sf_alpa_to_switch[al_pa];
						if (device_in_map(&map, tid))
							break;
					}
				}
				dl = dl->next;
			}
			if (dl) {
				(void) sprintf(l_error_msg,
					"Photon \"%s\" not removed."
					" Onlining Drives in photon.\n",
					disk_list_head->box_name);
				l_error_msg_ptr = &l_error_msg[0];
				L_WARNING_PRINT;
				for (dl = disk_list_head->dlhead; dl; ) {
					(void) l_online_drive(dl->multipath,
						NULL);
					l_free_multipath(dl->multipath);
					dl1 = dl;
					dl = dl->next;
					free(dl1);
				}
				disk_list_head->dlhead = NULL;
				goto loop1;
			}

			/* remove logical nodes for this photon */
			/* this includes ses and /dev/dsk entries */
			for (dl = disk_list_head->dlhead; dl; ) {
				remove_nodes(dl->multipath);
				l_free_multipath(dl->multipath);
				dl1 = dl;
				dl = dl->next;
				free(dl1);
			}
			disk_list_head->dlhead = NULL;
			remove_ses_nodes(disk_list_head->seslist);
			goto loop1;
		}
		(void) sprintf(device_name, "  Drive in Box Name \"%s\" %s"
				" slot %d", disk_list_head->box_name,
				(tid & 0x10) ? "rear" : "front", slot);
		(void) fprintf(stdout, "%s\n", device_name);

		dl = disk_list_head->seslist;
		while (dl) {
			ses_path = dl->dev_path;
			if (l_get_status(ses_path, &l_state, verbose_flag) == 0)
					break;
			dl = dl->next;
		}
		if (dl == NULL) {
			(void) fprintf(stderr,
				MSGSTR(-1,
				"Error:"
				" get_status failed.\n"));
			goto loop1;
		}

		code = 0;
		while (((err = encl_status_page_funcs(OVERALL_STATUS,
			&code, todo, ses_path, &l_state, f_r, slot,
			verbose_flag)) != 0) || (code != 0)) {
			if (err) {
				L_ERR_PRINT;
			} else if (todo == REMOVE_DEVICE) {
				if (code == S_OK) {
					(void) fprintf(stdout,
					MSGSTR(-1,
					"\n  Warning: Device has not been"
					" removed from the enclosure\n"
					"  and is still on the loop."));
					goto loop1;
				} else {
					(void) fprintf(stdout,
					MSGSTR(-1,
					"  Notice: Device has not been"
					" removed from the enclosure.\n"
					"  It has been removed from the"
					" loop and is ready to be\n"
					"  removed"
					" from the enclosure, and"
					" the LED is blinking.\n\n"));
				}
				goto loop2;
			} else if ((todo == INSERT_DEVICE) &&
				((code != S_NOT_AVAILABLE) ||
					(timeout >
					PHOTON_SPINUP_TIMEOUT) ||
					err)) {
				(void) fprintf(stdout,
					MSGSTR(-1,
					"\n  Warning: Disk status is"
					" Not OK!\n\n"));
				goto loop1;
			}
			sleep(PHOTON_SPINUP_DELAY);
			if (wait_spinup_flag++ == 0) {
				(void) fprintf(stdout, MSGSTR(-1,
				"  Waiting for the disk to spin up:"));
			} else {
				(void) fprintf(stdout, ".");
			}
			fflush(stdout);
			timeout++;
		}

		if (wait_spinup_flag) {
			(void) fprintf(stdout, "\n");
		}
loop2:
		switch (todo) {
			case INSERT_DEVICE:
				/* check loop map that drive is present */
				for (;;) {
					dl = disk_list_head->seslist;
					while (dl) {
						ses_path = dl->dev_path;
						if (l_get_dev_map(ses_path,
							&map,
							verbose_flag)) {
							(void) fprintf(stderr,
							MSGSTR(-1,
							"Error:"
							" could not get"
							" map for %s\n"),
							ses_path);
							L_ERR_PRINT;
							goto loop1;
						}
						if (device_in_map(&map, tid)) {
							goto loop3;
						}
						dl = dl->next;
					}

					if (timeout > PHOTON_SPINUP_TIMEOUT) {
						(void) fprintf(stdout,
							MSGSTR(-1,
							"\n   Warning: "
							"Device not in"
							" loop map\n"));
						goto loop1;
					}
					if (wait_map_flag++ == 0) {
						(void) fprintf(stdout,
						MSGSTR(-1,
						"  Waiting for the device "
						"to appear in the loop map:"));
					} else {
						(void) fprintf(stdout, ".");
					}
					fflush(stdout);
					timeout++;
					sleep(PHOTON_SPINUP_DELAY);
				}
loop3:
				if (wait_map_flag) {
					(void) fprintf(stdout, "\n");
				}

				/*
				 * Run drvconfig and disks to create
				 * logical nodes
				 */
				for (;;) {
					if (system("drvconfig -i ssd")) {
						(void) fprintf(stderr,
							MSGSTR(-1,
							"Error:"
							" Could not "
							"run drvconfig\n"));
						goto loop1;
					}

					if (device_present(ses_path, tid, &map,
						verbose_flag, &dev_path))
						break;
					if (timeout > PHOTON_SPINUP_TIMEOUT) {
						(void) fprintf(stdout,
							MSGSTR(-1,
							"\n   Warning: "
							"Could not find "
							"the node:\n%s\n"),
							dev_path);
						goto loop1;
					}
					if (wait_node_flag++ == 0) {
						(void) fprintf(stdout,
						MSGSTR(-1,
						"  Waiting for the logical "
						"node to be created:"));
					} else {
						(void) fprintf(stdout, ".");
					}
					fflush(stdout);
					timeout++;
					sleep(PHOTON_SPINUP_DELAY);
				}
				if (wait_node_flag) {
					(void) fprintf(stdout, "\n");
				}

				if (system("disks")) {
					(void) fprintf(stderr,
						MSGSTR(-1,
						"Error:"
						" Could not run disks.\n"));
					goto loop1;
				}
				l_free_wwn_list(&g_WWN_list);
				g_WWN_list = (struct wwn_list_struct *)NULL;
				dl = l_get_multipath(dev_path, 0);
				display_logical_nodes(dl);
				break;
			case REMOVE_DEVICE:
/*
 * TBD
 * Need to check all loops.
 */
				/* check that device is not in loop map */
				if (l_get_dev_map(ses_path, &map,
						verbose_flag)) {
					(void) fprintf(stderr,
						MSGSTR(-1,
						"Error:"
						" could not get map for %s\n"),
						ses_path);
					L_ERR_PRINT;
					goto loop1;
				}

				if (device_in_map(&map, tid)) {
					(void) sprintf(l_error_msg,
					"Device still in the loop map.\n");
					l_error_msg_ptr = &l_error_msg[0];
					L_WARNING_PRINT;
					goto loop1;
				}


				/* remove logical nodes */
				remove_nodes(disk_list_head->dlhead);

				break;
		}
loop1:
		d1 = disk_list_head;
		disk_list_head = disk_list_head->next;
		l_free_multipath(d1->seslist);
		l_free_multipath(d1->dlhead);
		free(d1);
	}

	return (0);
}

/*
 * If OVERALL_STATUS is sent as the "func" the
 * code pointer must be valid (non NULL).
 */
static int
encl_status_page_funcs(int func, char *code, int todo, char *ses_path,
	struct l_state_struct  *l_state,
	int f_flag, int slot, int verbose_flag)
{
u_char	*page_buf;
int 	fd, front_index, rear_index, offset, err;
unsigned short	page_len;
struct	device_element *elem;

	if ((page_buf = (u_char *)zalloc(MAX_REC_DIAG_LENGTH)) == NULL) {
		return (errno);
	}

	if ((fd = object_open(ses_path, O_NDELAY | O_RDWR)) == -1) {
		free(page_buf);
		return (errno);
	}

	if (err = l_get_envsen_page(fd, page_buf, MAX_REC_DIAG_LENGTH,
		L_PAGE_2, verbose_flag)) {
		(void) free(page_buf);
		(void) close(fd);
		return (err);
	}

	page_len = (page_buf[2] << 8 | page_buf[3]) + HEADER_LEN;

	if (err = l_get_disk_element_index(l_state, &front_index,
		&rear_index)) {
		(void) free(page_buf);
		(void) close(fd);
		return (err);
	}
	/* Skip global element */
	front_index++, rear_index++;

	if (f_flag) {
		offset = (8 + (front_index + slot)*4);
	} else {
		offset = (8 + (rear_index  + slot)*4);
	}

	elem = (struct device_element *)((int)page_buf + offset);

	switch (func) {
		case OVERALL_STATUS:
		    switch (todo) {
			case INSERT_DEVICE:
				*code = (elem->code != S_OK) ? elem->code : 0;
				(void) free(page_buf);
				(void) close(fd);
				return (0);
			case REMOVE_DEVICE:
				*code = (elem->code != S_NOT_INSTALLED) ?
					elem->code : 0;
				(void) free(page_buf);
				(void) close(fd);
				return (0);
		    }
		    /*NOTREACHED*/
		case SET_RQST_INSRT:
			bzero(elem, sizeof (struct device_element));
			elem->select = 1;
			elem->rdy_to_ins = 1;
			break;
		case SET_RQST_RMV:
			bzero(elem, sizeof (struct device_element));
			elem->select = 1;
			elem->rmv = 1;
			elem->dev_off = 1;
			elem->en_bypass_a = 1;
			elem->en_bypass_b = 1;
			break;
		case SET_FAULT:
			bzero(elem, sizeof (struct device_element));
			elem->select = 1;
			elem->fault_req = 1;
			elem->dev_off = 1;
			elem->en_bypass_a = 1;
			elem->en_bypass_b = 1;
			break;
		case SET_DRV_ON:
			bzero(elem, sizeof (struct device_element));
			elem->select = 1;
			break;
	}

	err = scsi_send_diag_cmd(fd, (u_char *)page_buf, page_len);
	free(page_buf);
	(void) close(fd);
	return (err);
}

static int
print_list_warn(struct hotplug_disk_list *disk_list_head, int todo)
{
	struct hotplug_disk_list *disk_list = disk_list_head;
	int i;
	char c;

	(void) fprintf(stdout,
		MSGSTR(-1, "\n\nThe list of devices which will be "));
	switch (todo) {
		case INSERT_DEVICE:
			(void) fprintf(stdout,
			MSGSTR(-1, "inserted is:\n"));
			break;
		case REMOVE_DEVICE:
			(void) fprintf(stdout,
			MSGSTR(-1, "removed is:\n"));
			break;
	}

	for (i = 1; disk_list; i++, disk_list = disk_list->next) {
		if (disk_list->dev_type != DTYPE_ESI) {
			(void) fprintf(stdout, "  %d: Box Name \"%s\" %s"
				" slot %d\n", i, disk_list->box_name,
				(disk_list->f_flag) ? "front" : "rear",
				disk_list->slot);
		} else {
			(void) fprintf(stdout, "  %d: Box: %s\n",
				i, disk_list->box_name);
		}
		if (getenv("_LUX_H_DEBUG") != NULL) {
			struct dlist *dl_ses, *dl_multi;
			(void) fprintf(stdout, "      Select ID:\t0x%x\n"
				"      Device type:\t0x%x\n",
				disk_list->tid, disk_list->dev_type);
				if (disk_list->dev_type != DTYPE_ESI) {
					(void) fprintf(stdout,
					"      Location:   \tSlot %d %s\n",
					disk_list->slot,
					disk_list->f_flag ? "front" : "rear");
				}
			dl_ses = disk_list_head->seslist;
			(void) fprintf(stdout, "      SES Paths:\n");
			while (dl_ses) {
				(void) fprintf(stdout,
				"           %s\n", dl_ses->dev_path);
				dl_ses = dl_ses->next;
			}
			dl_multi = disk_list_head->dlhead;
			(void) fprintf(stdout, "      Device Paths:\n");
			while (dl_multi) {
				(void) fprintf(stdout,
				"           %s\n", dl_multi->dev_path);
				dl_multi = dl_multi->next;
			}
		}
	}
	(void) fprintf(stdout, "\nPlease enter 'q' to Quit or <Return> to"
		" Continue: ");

	c = getchar();
	if (c == 'Q' || c == 'q') {
		return (-1);
	}
	(void) fprintf(stdout, "\n");
	return (0);
}
/*ARGSUSED*/
static int
device_present(char *ses_path, int tid, sf_al_map_t *map, int verbose_flag,
		char **dev_path)
{
	char	sf_path[MAXPATHLEN];
	u_char	wwn[40], c;
	char	cmd[MAXPATHLEN];
	int	len, i, j, k, fnib, snib, al_pa;
	char	ssd[30];

	if (name_id) {
		len = strlen(ses_path) - strlen(strrchr(ses_path, '/'));

		sprintf(ssd, "ssd@%x,0", tid);

		strncpy(sf_path, ses_path, len);
		sf_path[len] = '\0';
		P_DPRINTF("  device_present: tid=%x, sf_path=%s\n",
			tid, sf_path);
		*dev_path = zalloc(MAXPATHLEN);
		(void) sprintf(*dev_path, "%s/%s", sf_path, ssd);
		P_DPRINTF("  device_present: dev_path=%s\n", *dev_path);

		sprintf(cmd, "ls %s | grep %s > /dev/null", sf_path, ssd);

		if (system(cmd)) {
			return (0);
		}
		(void) strcat(*dev_path, ":c");

		return (1);
	} else {
		for (i = 0; i < map->sf_count; i++) {
			if (map->sf_addr_pair[i].sf_inq_dtype != DTYPE_ESI) {
				al_pa = map->sf_addr_pair[i].sf_al_pa;
				if (tid == l_sf_alpa_to_switch[al_pa]) {
					break;
				}
			}
		}
		if (i >= map->sf_count) {
			return (0);
		}
		/*
		 * Make sure that the port WWN is valid
		 */
		for (j = 0; j < 8; j++) {
			if (map->sf_addr_pair[i].sf_port_wwn[j] != '\0')
				break;
		}
		if (j >= 8) {
			return (0);
		}
		for (j = 0, k = 0; j < 8; j++) {
			c = map->sf_addr_pair[i].sf_port_wwn[j];
			fnib = (((int)(c & 0xf0)) >> 4);
			snib = (c & 0x0f);
			if (fnib >= 0 && fnib <= 9)
				wwn[k++] = '0' + fnib;
			else if (fnib >= 10 && fnib <= 15)
				wwn[k++] = 'a' + fnib - 10;
			if (snib >= 0 && snib <= 9)
				wwn[k++] = '0' + snib;
			else if (snib >= 10 && snib <= 15)
				wwn[k++] = 'a' + snib - 10;
		}
		wwn[k] = '\0';
		len = strlen(ses_path) - strlen(strrchr(ses_path, '/'));

		sprintf(ssd, "ssd@w%s,0", wwn);

		strncpy(sf_path, ses_path, len);
		sf_path[len] = '\0';
		P_DPRINTF("  device_present: wwn=%s, sf_path=%s\n",
			wwn, sf_path);

		sprintf(cmd, "ls %s | grep %s > /dev/null", sf_path, ssd);
		*dev_path = zalloc(MAXPATHLEN);
		(void) sprintf(*dev_path, "%s/%s", sf_path, ssd);
		P_DPRINTF("  device_present: dev_path=%s\n", *dev_path);


		if (system(cmd)) {
			return (0);
		}
		(void) strcat(*dev_path, ":c");

		return (1);
	}
}
static void
make_node(char *ses_path, int tid, char *dev_path, sf_al_map_t *map, int dtype)
{
	int	len, i, j;
	char	ssd[40], wwn[20];

	if (name_id) {
		len = strlen(ses_path) - strlen(strrchr(ses_path, '/'));
/*
TBD
Must find path, not just use :c
*/
		if (dtype != DTYPE_ESI) {
			sprintf(ssd, "/ssd@%x,0:c", tid);
		} else {
			sprintf(ssd, "/ses@%x,0:c", tid);
		}

		strncpy(dev_path, ses_path, len);
		dev_path[len] = '\0';

		strcat(dev_path, ssd);
	} else {
		for (i = 0; i < map->sf_count; i++) {
			if (map->sf_addr_pair[i].sf_al_pa ==
				l_switch_to_alpa[tid])
				break;
		}
		if (i >= map->sf_count) {
			*dev_path = '\0';
			return;
		} else {
			/*
			 * Make sure that the port WWN is valid
			 */
			for (j = 0; j < 8; j++) {
				if (map->sf_addr_pair[i].sf_port_wwn[j] != '\0')
					break;
			}
			if (j >= 8) {
				*dev_path = '\0';
				return;
			}
			ll_to_str(map->sf_addr_pair[i].sf_port_wwn, wwn);
			len = strlen(ses_path) - strlen(strrchr(ses_path, '/'));

			if (dtype != DTYPE_ESI) {
				sprintf(ssd, "/ssd@w%s,0:c", wwn);
			} else {
				sprintf(ssd, "/ses@w%s,0:c", wwn);
			}
/*
TBD
Must find path, not just use :c
*/

			strncpy(dev_path, ses_path, len);
			dev_path[len] = '\0';

			strcat(dev_path, ssd);
		}
	}
}

static void
remove_nodes(struct dlist *dl)
{
	char link[MAXPATHLEN], path[MAXPATHLEN], lname[MAXPATHLEN];
	DIR	*dir;
	struct dirent *dirent;
	struct dlist *dl1;
	char	*d1, *d2;

	if ((dir = opendir("/dev/dsk")) == NULL) {
		return;
	}

	(void) fprintf(stdout, "    Removing Logical Nodes: \n");

	while ((dirent = readdir(dir)) != (struct dirent *)NULL) {
		if (strcmp(dirent->d_name, ".") == 0 ||
			strcmp(dirent->d_name, "..") == 0) {
			continue;
		}
		(void) sprintf(lname, "/dev/dsk/%s", dirent->d_name);
		if (readlink((const char *)lname, (char *)link,
			(size_t)MAXPATHLEN) <= 0) {
			fprintf(stderr, "Could not read %s\n", lname);
			continue;
		}
		for (dl1 = dl; dl1; dl1 = dl1->next) {
			strcpy(path, dl1->dev_path);
			d1 = strrchr(path, ':');
			if (d1)
				*d1 = '\0';
			if (strstr(link, path)) {
				(void) unlink(lname);
				(void) sprintf(lname, "/dev/rdsk/%s",
							dirent->d_name);
				(void) fprintf(stdout, "\tRemoving %s\n",
							dirent->d_name);
				(void) unlink(lname);
			}
		}
	}

	closedir(dir);

	for (dl1 = dl; dl1; dl1 = dl1->next) {

		strcpy(path, dl1->dev_path);
		d1 = strrchr(path, '/');
		if (d1)
			*d1++ = '\0';
		d2 = strrchr(d1, ':');
		if (d2)
			*d2 = '\0';

		if ((dir = opendir(path)) == NULL) {
			continue;
		}

		while ((dirent = readdir(dir)) != (struct dirent *)NULL) {
			if (strcmp(dirent->d_name, ".") == 0 ||
				strcmp(dirent->d_name, "..") == 0) {
				continue;
			}
			if (strstr(dirent->d_name, d1)) {
				(void) sprintf(link, "%s/%s", path,
							dirent->d_name);
				(void) unlink(link);
			}
		}
		closedir(dir);
	}
}
static void
remove_ses_nodes(struct dlist *dl)
{
	char link[MAXPATHLEN], lname[MAXPATHLEN];
	DIR	*dir;
	struct dirent *dirent;
	struct	dlist	*dl1;

	if ((dir = opendir("/dev/es")) == NULL) {
		return;
	}

	(void) fprintf(stdout, "  Removing Ses Nodes:\n");

	dl1 = dl;
	while ((dirent = readdir(dir)) != (struct dirent *)NULL) {
		if (strcmp(dirent->d_name, ".") == 0 ||
			strcmp(dirent->d_name, "..") == 0) {
			continue;
		}
		(void) sprintf(lname, "/dev/es/%s", dirent->d_name);
		if (readlink((const char *)lname, (char *)link,
			(size_t)MAXPATHLEN) <= 0) {
			fprintf(stderr, "Could not read %s\n", lname);
			continue;
		}
		for (dl = dl1; dl; dl = dl->next) {
			if (strstr(link, dl->dev_path)) {
				(void) fprintf(stdout, "\tRemoving %s\n",
							lname);
				(void) unlink(lname);
			}
		}
	}
	l_free_multipath(dl1);
	closedir(dir);
}
static int
device_in_map(sf_al_map_t *map, int tid)
{
	int i, j;

	for (i = 0; i < map->sf_count; i++) {
		if (map->sf_addr_pair[i].sf_al_pa ==
			(int)l_switch_to_alpa[tid]) {
			/* Does not count if WWN == 0 */
			for (j = 0; j < WWN_SIZE; j++) {
				if (map->sf_addr_pair[i].sf_port_wwn[j] != 0) {
					return (1);
				}
			}
		}
	}
	return (0);
}

static void
display_logical_nodes(struct dlist *dl)
{
	char link[MAXPATHLEN], path[MAXPATHLEN], lname[MAXPATHLEN];
	DIR	*dir;
	struct dirent *dirent;
	struct dlist *dl1;
	char	*d1;

	if ((dir = opendir("/dev/dsk")) == NULL) {
		return;
	}
	fprintf(stdout, "  Logical Nodes under /dev/dsk and /dev/rdsk :\n");

	while ((dirent = readdir(dir)) != (struct dirent *)NULL) {
		if (strcmp(dirent->d_name, ".") == 0 ||
			strcmp(dirent->d_name, "..") == 0) {
			continue;
		}
		(void) sprintf(lname, "/dev/dsk/%s", dirent->d_name);
		if (readlink((const char *)lname, (char *)link,
			(size_t)MAXPATHLEN) <= 0) {
			(void) fprintf(stderr, "Could not read %s\n", lname);
			continue;
		}
		for (dl1 = dl; dl1; dl1 = dl1->next) {
			strcpy(path, dl1->dev_path);
			d1 = strrchr(path, ':');
			if (d1)
				*d1 = '\0';
			if (strstr(link, path)) {
			(void) fprintf(stdout, "\t%s\n", dirent->d_name);
			}
		}
	}

	closedir(dir);
}
static int
l_offline_photon(struct hotplug_disk_list *list)
{
int i;
struct dlist *dl_head, *dl_tail, *dl, *dl_ses, *dl1;
char	*dev_path, ses_path[MAXPATHLEN], device_name[MAXNAMELEN];
L_state		l_state;

	dl_head = dl_tail = (struct dlist *)NULL;
	/* Get global status for this Photon */
	dl_ses = list->seslist;
	while (dl_ses) {
		strcpy(ses_path, dl_ses->dev_path);
		if (l_get_status(ses_path, &l_state, (Options & PVERBOSE)) == 0)
			break;
		dl_ses = dl_ses->next;
	}
	/* Clear error messages we may gotten */
	l_error_msg_ptr = p_error_msg_ptr = NULL;
	if (dl_ses == NULL) {
		(void) sprintf(l_error_msg,
			"  Invalid Photon name.\n");
		l_error_msg_ptr = (char *)&l_error_msg;
		return (-1);
	}


	for (i = 0; i < l_state.total_num_drv/2; i++) {
		if (*l_state.drv_front[i].physical_path) {
			if (!(dev_path = zalloc(MAXPATHLEN))) {
				(void) sprintf(l_error_msg,
					MSGSTR(-1, "  Out of Memory.\n"));
				l_error_msg_ptr = (char *)&l_error_msg;
				goto error;
			}
			strcpy(dev_path,
				(char *)&l_state.drv_front[i].physical_path);
			if (!(dl = zalloc(sizeof (struct dlist)))) {
				free(dev_path);
				(void) sprintf(l_error_msg,
					MSGSTR(-1, "  Out of Memory.\n"));
				l_error_msg_ptr = (char *)&l_error_msg;
				goto error;
			}
			dl->dev_path = dev_path;
			dl->multipath = l_get_multipath(dev_path, 0);
			l_get_drive_name(device_name, i, 1, list->box_name);
			if (dl->multipath == NULL) {
				(void) sprintf(l_error_msg,
					"  cannot find valid path to"
					" %s\n", device_name);
				l_error_msg_ptr = (char *)&l_error_msg;
				free(dev_path);
				free(dl);
				goto error;
			}
			if (l_ex_open_test(dl->multipath,
					device_name)) {
				free(dev_path);
				l_free_multipath(dl->multipath);
				free(dl);
				goto error;
			}
			if (l_offline_drive(dl->multipath, device_name)) {
				free(dev_path);
				l_free_multipath(dl->multipath);
				free(dl);
				goto error;
			}
			if (dl_head) {
				dl_tail->next = dl;
				dl->prev = dl_tail;
				dl_tail = dl;
			} else {
				dl_head = dl;
				dl_tail = dl;
			}

			free(dev_path);
		}
		if (*l_state.drv_rear[i].physical_path) {
			if (!(dev_path = zalloc(MAXPATHLEN))) {
				(void) sprintf(l_error_msg,
					MSGSTR(-1, "  Out of Memory.\n"));
				l_error_msg_ptr = (char *)&l_error_msg;
				goto error;
			}
			strcpy(dev_path,
				(char *)&l_state.drv_rear[i].physical_path);
			if (!(dl = zalloc(sizeof (struct dlist)))) {
				(void) sprintf(l_error_msg,
					MSGSTR(-1, "  Out of Memory.\n"));
				l_error_msg_ptr = (char *)&l_error_msg;
				free(dev_path);
				goto error;
			}
			dl->dev_path = dev_path;
			dl->multipath = l_get_multipath(dev_path, 0);
			l_get_drive_name(device_name, i, 0, list->box_name);
			if (dl->multipath == NULL) {
				(void) sprintf(l_error_msg,
					"  cannot find valid path to"
					" %s\n", device_name);
				l_error_msg_ptr = (char *)&l_error_msg;
				free(dev_path);
				free(dl);
				goto error;
			}
			if (l_ex_open_test(dl->multipath, device_name)) {
				free(dev_path);
				l_free_multipath(dl->multipath);
				free(dl);
				goto error;
			}
			if (l_offline_drive(dl->multipath, device_name)) {
				free(dev_path);
				l_free_multipath(dl->multipath);
				free(dl);
				goto error;
			}
			if (dl_head) {
				dl_tail->next = dl;
				dl->prev = dl_tail;
				dl_tail = dl;
			} else {
				dl_head = dl;
				dl_tail = dl;
			}

			free(dev_path);
		}
	}


	list->dlhead = dl_head;
	l_error_msg[0] = '\0';
	return (0);
error:
	for (dl = dl_head; dl; ) {
		(void) l_online_drive(dl->multipath, NULL);
		l_free_multipath(dl->multipath);
		dl1 = dl;
		dl = dl->next;
		free(dl1);
	}
	return (-1);
}

/*ARGSUSED*/
static int
l_dev_start(char *drv_path, char *device_name, int verbose)
{
int	status;

	(void) fprintf(stdout, MSGSTR(-1, "starting:  %s...."), device_name);
	fflush(stdout);
	if ((drv_path != NULL) && (*drv_path != '\0')) {
		if (status = l_start(drv_path)) {
			(void) fprintf(stderr, "\n\n");
			return (status);
		}
	}
	(void) fprintf(stdout, MSGSTR(-1, "Done\n"));
	return (0);
}

/*
 * Stop the drive, not immediate.
 *
 * The reason we stop the drive and don't just have the IB do drv_off is
 * for timing purposes.
 */
/*ARGSUSED*/
static int
l_dev_stop(char *drv_path, char *device_name, int verbose)
{
int	status;

	(void) fprintf(stdout, MSGSTR(-1, "stopping:  %s...."), device_name);
	fflush(stdout);
	/* stop the device */
	/* Make the stop NOT immediate, so we wait. */
	if ((drv_path != NULL) && (*drv_path != '\0')) {
		if (status = l_stop(drv_path, 0)) {
			(void) fprintf(stderr, "\n\n");
			return (status);
		}
	}
	(void) fprintf(stdout, MSGSTR(-1, "Done\n"));
	return (0);
}

/*ARGSUSED*/
static int
l_offline_drive(struct dlist *dl, char *device_name)
{
#ifdef	DEVCTL_LIB
	char			dev_path1[MAXPATHLEN];
	devctl_hdl_t		devhdl;

	printf("offlining: %s....", device_name);
	fflush(stdout);
	while (dl) {
		strcpy(dev_path1, dl->dev_path);
		/* offline the device */
		if (devctl_acquire(dev_path1, DC_EXCL, &devhdl) == 0) {
			if (devctl_device_offline(devhdl)) {
				(void) fprintf(stderr, "\n\n");
				(void) sprintf(l_error_msg,
					MSGSTR(-1,
					" Could not offline device."
					" May be Busy.\n\n"));
				l_error_msg_ptr = &l_error_msg[0];
				(void) devctl_release(devhdl);
				return (-1);
			}
		(void) devctl_release(devhdl);
		}
		dl = dl->next;
	}
	(void) fprintf(stdout, MSGSTR(-1, "Done\n"));
#endif	/* DEVCTL_LIB */

	return (0);
}
/*ARGSUSED*/
static void
l_online_drive(struct dlist *dl, char *device_name)
{
#ifdef	DEVCTL_LIB
	char			dev_path1[MAXPATHLEN];
	devctl_hdl_t		devhdl;

	if (device_name)
		(void) fprintf(stdout, "onlining: %s\n", device_name);
	while (dl) {
		strcpy(dev_path1, dl->dev_path);
		if (devctl_acquire(dev_path1, DC_EXCL, &devhdl) == 0) {
			(void) devctl_device_online(devhdl);
			(void) devctl_release(devhdl);
		}
		dl = dl->next;
	}
#endif	/* DEVCTL_LIB */
}

#define	NODE_CREATION_TIME		60	/* # seconds */
static void
l_insert_photon()
{
	struct stat	ses_stat, dsk_stat;
	timestruc_t	ses_time, dsk_time;
	char lname[MAXPATHLEN], link[MAXPATHLEN];
	DIR	*dir;
	struct dirent *dirent;
	struct box_list_struct *bl1, *box_list = NULL;


	(void) l_get_box_list(&box_list, 0);
	if (stat("/dev/es", &ses_stat) < 0) {
		(void) fprintf(stderr, "Could not stat /dev/es\n");
		return;
	}
	if (stat("/dev/dsk", &dsk_stat) < 0) {
		(void) fprintf(stderr, "Could not stat /dev/dsk\n");
		return;
	}

	ses_time = ses_stat.st_mtim;
	dsk_time = dsk_stat.st_mtim;

	(void) fprintf(stdout, "Please hit <enter> when you have finished"
		" adding photon(s): ");
	getchar();
	printf("\nWaiting for Loop Initialization to complete...\n");
	/*
	 * We sleep here to let the system create nodes. Not sleeping
	 * could cause the drvconfig below to run too soon.
	 */

	sleep(NODE_CREATION_TIME);

	/*
	 * Run drvconfig and disks to create
	 * logical nodes
	 */

	if (system("drvconfig")) {
		(void) fprintf(stderr,
			MSGSTR(-1,
			"Error:"
			" Could not run drvconfig.\n"));
		return;
	}

	if (system("disks")) {
		(void) fprintf(stderr,
			MSGSTR(-1,
			"Error:"
			" Could not run disks.\n"));
		return;
	}

	if (system("devlinks")) {
		(void) fprintf(stderr,
			MSGSTR(-1,
			"Error:"
			" Could not run devlinks.\n"));
		return;
	}

	if ((dir = opendir("/dev/es")) == NULL) {
		return;
	}
	fprintf(stdout, "  New Logical Nodes under /dev/es:\n");

	while ((dirent = readdir(dir)) != (struct dirent *)NULL) {
		if (strcmp(dirent->d_name, ".") == 0 ||
			strcmp(dirent->d_name, "..") == 0) {
			continue;
		}
		(void) sprintf(lname, "/dev/es/%s", dirent->d_name);
		if (lstat(lname, &ses_stat) < 0) {
			(void) fprintf(stderr, "Could not stat %s\n", lname);
			continue;
		}

		for (bl1 = box_list; bl1; bl1 = bl1->box_next) {
			if (strstr(lname, bl1->b_physical_path))
				break;
		}

		if (box_list && bl1)
			continue;

		if (NEWER(ses_stat.st_ctim, ses_time)) {
			(void) fprintf(stdout, "\t%s\n", dirent->d_name);
		}
	}

	closedir(dir);

	l_free_box_list(&box_list);

	if ((dir = opendir("/dev/dsk")) == NULL) {
		return;
	}

	fprintf(stdout, "  New Logical Nodes under /dev/dsk and /dev/rdsk :\n");

	while ((dirent = readdir(dir)) != (struct dirent *)NULL) {
		if (strcmp(dirent->d_name, ".") == 0 ||
			strcmp(dirent->d_name, "..") == 0) {
			continue;
		}
		(void) sprintf(lname, "/dev/dsk/%s", dirent->d_name);
		if (lstat(lname, &dsk_stat) < 0) {
			(void) fprintf(stderr, "Could not stat %s\n", lname);
			continue;
		}
		if (readlink((const char *)lname, (char *)link,
			(size_t)MAXPATHLEN) <= 0) {
			(void) fprintf(stderr, "Could not read %s\n", lname);
			continue;
		}
		if (!strstr(link, "SUNW,socal")) {
			continue;
		}

		if (NEWER(dsk_stat.st_ctim, dsk_time)) {
			(void) fprintf(stdout, "\t%s\n", dirent->d_name);
		}
	}

	closedir(dir);
}
void
ll_to_str(u_char *wwn_ll, char	*wwn_str)
{
	int j, k, fnib, snib;
	u_char c;

	for (j = 0, k = 0; j < 8; j++) {
		c = wwn_ll[j];
		fnib = ((int)(c & 0xf0) >> 4);
		snib = (c & 0x0f);
		if (fnib >= 0 && fnib <= 9)
			wwn_str[k++] = '0' + fnib;
		else if (fnib >= 10 && fnib <= 15)
			wwn_str[k++] = 'a' + fnib - 10;
		if (snib >= 0 && snib <= 9)
			wwn_str[k++] = '0' + snib;
		else if (snib >= 10 && snib <= 15)
			wwn_str[k++] = 'a' + snib - 10;
	}
	wwn_str[k] = '\0';
}
static void
l_forcelip_all(struct hotplug_disk_list *disk_list)
{
	char	*p;
	int	len, ndevs = 0;
	struct	dlist *dl;
	struct loop_list {
		char adp_name[MAXPATHLEN];
		struct loop_list *next;
		struct loop_list *prev;
	} *llist_head, *llist_tail, *llist, *llist1;

	llist_head = llist_tail = (struct loop_list *)NULL;

	while (disk_list) {

		dl = disk_list->seslist;
		p = strstr(dl->dev_path, "/ses");
		if (p == NULL) {
			P_DPRINTF("  l_forcelip_all: Not able to do LIP "
				" on this SES path because path invalid.\n"
				"  Path: %s\n",
				dl->dev_path);
			goto next;
		}
		len = strlen(dl->dev_path) - strlen(p);

		if (llist_head == NULL) {
			if ((llist_head = (struct loop_list *)
				zalloc(sizeof (struct loop_list))) == NULL) {
				return;
			}
			strncpy(llist_head->adp_name, dl->dev_path, len);
			llist_head->adp_name[len] = '\0';
			llist_tail = llist_head;
			ndevs++;
		} else {
			llist1 = llist_head;
			while (llist1) {
				if (strncmp(llist1->adp_name,
					dl->dev_path, len) == 0) {
					goto next;
				}
				llist1 = llist1->next;
			}
			if ((llist = (struct loop_list *)
				zalloc(sizeof (struct loop_list))) == NULL) {
				return;
			}
			strncpy(llist->adp_name, dl->dev_path, len);
			llist->adp_name[len] = '\0';
			llist->prev = llist_tail;
			llist_tail->next = llist;
			llist_tail = llist;
			ndevs++;
		}
next:
		disk_list = disk_list->next;
	}
	while (llist_head) {
		l_force_lip(llist_head->adp_name, 0);
		llist = llist_head;
		llist_head = llist_head->next;
		free(llist);
	}
	sleep(ndevs*10);
}
static void
l_get_drive_name(char *drive_name, int slot, int f_flag, char *box_name)
{
	(void) sprintf(drive_name, "Drive in \"%s\" %s slot %d",
			box_name, f_flag ? "front" : "rear ", slot);
}
