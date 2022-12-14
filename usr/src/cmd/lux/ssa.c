/*
 * Copyright 1997 Sun Microsystems, Inc. All rights reserved.
 */


#pragma	ident	"@(#)ssa.c 1.5     97/06/24 SMI"

/*LINTLIBRARY*/


/*
 *	Administration program for the SPARCstorage Array
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
#include	<dirent.h>
#include	<libintl.h>	/* gettext */
#include	<sys/sunddi.h>
#include	<sys/systeminfo.h>
#include	<sys/scsi/scsi.h>
#include	<sys/scsi/targets/sesio.h>

#include	"ssadef.h"
#include	"p_scsi.h"
#include	"p_state.h"


#define	SSASTR	"SSA"
#define	P_PMSG(MSG)	(void) fprintf(stdout, MSG)
#define	VERBPRINT	if (Options & PVERBOSE) (void) printf
#define	P_ERR_PRINT	if (p_error_msg_ptr == NULL) {	\
	    perror(whoami);	\
	} else {	\
	    (void) fprintf(stderr, "%s: %s", whoami, p_error_msg_ptr);	\
	}

/* global variables */
extern	char	*whoami;
extern	int	Options;
extern	const	OPTION_A;
extern	const	OPTION_C;
extern	const	OPTION_D;
extern	const	OPTION_E;
extern	const	OPTION_P;
extern	const	PVERBOSE;
extern	const	SAVE;
extern	const	OPTION_T;
extern	const	OPTION_L;
extern	const	OPTION_F;
extern	const	OPTION_W;
extern	const	OPTION_Z;
extern	const	OPTION_Y;
extern	const	OPTION_CAPF;

extern	char	lux_version[];


/* external functions */
extern  char 	*get_physical_name(char *);
extern  int	p_get_status(char *, P_state **,  int, int);
extern	int	p_download(char *, char *, int, int, char *);
extern	int	p_fast_write(char *, int, int, u_char);
extern	int	p_get_err_statistics(char *);
extern	int	p_purge(char *);
extern	int	p_reserve(char *);
extern	int	p_release(char *);
extern	int	p_set_perf_statistics(char *, int);
extern	int	p_get_perf_statistics(char *, P_perf_statistics **);
extern	int	p_start(char *);
extern	int	p_stop(char *);
extern	int	p_sync_cache(char *);
extern	int	p_check_file(char *, int);
extern	int	p_get_drv_name(char *, char *);
extern	int	p_get_wb_statistics(char *, Wb_log_entry **);
static	void	cli_display_wb_perf(struct wb_log_entry *, P_state *);
static	void	cli_display_nvram_data(char *, P_state *, int, u_char, u_char);
static	void	cli_print_wb_mode(int);
extern	void	fc_update(int, int, char *);
extern	int	setboot(unsigned, unsigned, char *);
extern	void	cli_display_envsen_data(char **, int);
extern	int	get_envsen_global_data(char **, int, char **, int *);
extern	struct	rsm_es_in *find_card(int);


/*
 *	Pluto CLI DISPLAY function
 *
 *	Display the status of all devices or individual devices.
 *
 *	    When the controller is specified the status
 *	    of the devices is displayed from a
 *	    messages string that is created by get_status
 *
 *	    When the path is of the controller the options are:
 *	    -p Display the performance statistics
 *
 *	    There are undocumented options in this function.
 *	    They are:
 *	    -a	    Generate all displays (controller, grouped
 *			drives & performance displays but not
 *			device displays).
 *	    -e	    Extend performance display
 *	    -l	    Loop on the performance display
 *	    -t	    Time to wait before getting performance status
 *		    (Minimum is 1 second, maximum is 20 seconds)
 *		    (Default is 10 seconds)
*
*/
static void
cli_ext_perf_disp(P_perf_statistics *perf_ptr, int p, int t)
{
	(void) fprintf(stdout,
		" %2d %2d %2d %2d  %2d %2d %2d %2d  ",
	perf_ptr->perf_details[p][t].num_lt_2k_reads,
	perf_ptr->perf_details[p][t].num_gt_2k_lt_8k_reads,
	perf_ptr->perf_details[p][t].num_8k_reads,
	perf_ptr->perf_details[p][t].num_gt_8k_reads,
	perf_ptr->perf_details[p][t].num_lt_2k_writes,
	perf_ptr->perf_details[p][t].num_gt_2k_lt_8k_writes,
	perf_ptr->perf_details[p][t].num_8k_writes,
	perf_ptr->perf_details[p][t].num_gt_8k_writes);
	(void) fprintf(stdout,
		"%2d %2d %2d %2d  %2d %2d %2d %2d  ",
	perf_ptr->perf_details[p + 2][t].num_lt_2k_reads,
	perf_ptr->perf_details[p + 2][t].num_gt_2k_lt_8k_reads,
	perf_ptr->perf_details[p + 2][t].num_8k_reads,
	perf_ptr->perf_details[p + 2][t].num_gt_8k_reads,
	perf_ptr->perf_details[p + 2][t].num_lt_2k_writes,
	perf_ptr->perf_details[p + 2][t].num_gt_2k_lt_8k_writes,
	perf_ptr->perf_details[p + 2][t].num_8k_writes,
	perf_ptr->perf_details[p + 2][t].num_gt_8k_writes);
	(void) fprintf(stdout,
		"%2d %2d %2d %2d  %2d %2d %2d %2d\n",
	perf_ptr->perf_details[p + 4][t].num_lt_2k_reads,
	perf_ptr->perf_details[p + 4][t].num_gt_2k_lt_8k_reads,
	perf_ptr->perf_details[p + 4][t].num_8k_reads,
	perf_ptr->perf_details[p + 4][t].num_gt_8k_reads,
	perf_ptr->perf_details[p + 4][t].num_lt_2k_writes,
	perf_ptr->perf_details[p + 4][t].num_gt_2k_lt_8k_writes,
	perf_ptr->perf_details[p + 4][t].num_8k_writes,
	perf_ptr->perf_details[p + 4][t].num_gt_8k_writes);
}

void
ssa_cli_display_config(char **argv, char *path_phys,
	int delay_time, int nvram_data, int argc)
{
P_state			*state_ptr = NULL;
P_perf_statistics	**perf_ptr_ptr;
P_perf_statistics	*perf_ptr = NULL;
struct wb_log_entry	*wle = NULL;
int	i, t, p;
char	*char_ptr;
int	initial_update = 1;
char	local_path[MAXNAMELEN];
int	device_status_flag = 0;
u_char	port, tgt, nt;
int	path_index = 0;
float	m;
int	not_first_time;
char *prod_id_ptr, *prod_str;

	(void) strcpy(local_path, path_phys);

	if ((char_ptr = strstr(path_phys, ":ctlr")) == NULL) {
		/*
		 * path is of a device, not the controller
		 */
	    char_ptr = strrchr(local_path, '/');   /* point to place to add */
	    *char_ptr = '\0';	/* Terminate sting  */
	    /* append to controller path */
	    (void) strcat(local_path, ":ctlr");
	    device_status_flag = 1;
	}

	/* Get global status */
	if (p_get_status(local_path, &state_ptr,
	    initial_update, (Options & PVERBOSE))) {
	    P_ERR_PRINT;
	    exit(-1);
	}


	if ((prod_str = strstr(state_ptr->c_tbl.c_id.prod_id,
	    SSASTR)) == NULL) {
		p_error_msg_ptr = "Invalid Product ID";
		P_ERR_PRINT;
		p_error_msg_ptr = NULL;
		exit(-1);
	}
	if (strchr(prod_str, ' '))
		*(strchr(prod_str, ' ')) = '\0';
	prod_id_ptr = prod_str + strlen(SSASTR);

	if (device_status_flag) {

	    /*	    Parse physical path name to get port & target address */
	    if ((char_ptr = strstr(path_phys, SLSH_DRV_NAME_SD)) == NULL) {
		if ((char_ptr = strstr(path_phys, SLSH_DRV_NAME_SSD)) == NULL) {
		    (void) fprintf(stderr,
		    "%s: Invalid path name (%s)\n", whoami, path_phys);
		    exit(-1);
		}
	    }

		/*
		 * point to next character past the @ character
		 * port and target
		 */
	    char_ptr = 1 + strstr(char_ptr, "@");
		/*
		 * Make sure we don't get core dump
		 * in case port and target not included in path
		 * (@p,t:x,raw)
		 */
	    if ((int)strlen(char_ptr) < 3) {
		(void) fprintf(stderr,
		"%s: Invalid path name (%s)\n", whoami, path_phys);
		exit(-1);
	    }
	    port = (u_char)atoi(char_ptr);
	    char_ptr += 2; 	/* point to port & target  */
	    tgt = (u_char)atoi(char_ptr);
	    P_DPRINTF("Getting port & target from physical"
		" path name: port %d target %d\n",
		port, tgt);
	    P_DPRINTF("Physical path name:%s\n", path_phys);

	    if (nvram_data) {
		cli_display_nvram_data(local_path, state_ptr,
				device_status_flag, port, tgt);
	    }
		/*
		 * Display drive information
		 */
	    (void) fprintf(stdout, "\nDEVICE PROPERTIES for device %s\n",
		    argv[path_index]);
	    (void) fprintf(stdout, " SCSI Port %d  Target %d\n",
		port, tgt);

	    (void) fprintf(stdout, " Status:        ");
	    if (state_ptr->drv[port][tgt].state_flags & DS_NDF) {
		    (void) fprintf(stdout, "No Drive Found\n");
	    } else if (state_ptr->drv[port][tgt].state_flags & DS_NDS) {
		(void) fprintf(stdout, "Drive did not respond to Select\n");
	    } else if (state_ptr->drv[port][tgt].state_flags & DS_DNR) {
		    (void) fprintf(stdout, "Drive not Ready\n");
	    } else if (state_ptr->drv[port][tgt].state_flags & DS_CNR) {
		    (void) fprintf(stdout, "Could not Read from Drive\n");
	    } else if (state_ptr->drv[port][tgt].state_flags & DS_SPUN_DWN) {
		    (void) fprintf(stdout, "Drive Spun Down\n");
	    } else if (state_ptr->drv[port][tgt].state_flags & DS_OPNF) {
		    (void) fprintf(stdout, "Could not Open Device\n");
	    } else if (state_ptr->drv[port][tgt].state_flags & DS_INQF) {
		    (void) fprintf(stdout, "INQUIRY cmd failed\n");
	    } else if (state_ptr->drv[port][tgt].reserved_flag) {
		    (void) fprintf(stdout, "Drive is RESERVED\n");
	    } else {
		if (state_ptr->drv[port][tgt].no_label_flag) {
		    (void) fprintf(stdout, "No UNIX Label\n");
		} else {
		    (void) fprintf(stdout, "O.K.\n");
		}
		(void) fprintf(stdout, " Vendor:        %s\n",
		    state_ptr->drv[port][tgt].id.vendor_id);
		(void) fprintf(stdout, " Product ID:    %s\n",
		    state_ptr->drv[port][tgt].id.prod_id);
		(void) fprintf(stdout, " Firmware Rev:  %s\n",
		    state_ptr->drv[port][tgt].id.revision);
		/*
		 * Only print the firmware rev & serial number
		 * for SUN drives since other drives may not have.
		 *
		 * This display matches the SUN drive spec not
		 * the structure definition.
		 */
		if ((strstr(state_ptr->drv[port][tgt].id.prod_id,
		    "SUN") != 0) ||
		    (strcmp(state_ptr->drv[port][tgt].id.vendor_id,
		    "SUN     ")) == 0) {
		    (void) fprintf(stdout, " Serial Num:    %s%s\n",
			&state_ptr->drv[port][tgt].id.firmware_rev[0],
			&state_ptr->drv[port][tgt].id.ser_num[0]);
		}
		m = state_ptr->drv[port][tgt].num_blocks;
		m /= 2048;	/* get mega bytes */
		(void) fprintf(stdout,
		    " Unformatted Capacity: %6.3f MByte\n", m);
		if (state_ptr->drv[port][tgt].state_flags & DS_PCFW) {
		    (void) fprintf(stdout,
		    " Fast Writes: Enabled per command\n");
		} else {
		    (void) fprintf(stdout, " Fast Writes:   %s\n",
			(state_ptr->drv[port][tgt].state_flags & DS_FWE) ?
			"Enabled" : "Disabled");
		}
	    }
	} else {
	    if (nvram_data) {
		cli_display_nvram_data(local_path, state_ptr, 0, 0, 0);
	    } else if ((Options & OPTION_A) || !(Options & OPTION_P)) {

		/* display general Configuration display */

		P_PMSG("\n                    SPARCstorage Array ");
		(void) fprintf(stdout, "%s", prod_id_ptr);
		P_PMSG(" Configuration\n");

		(void) fprintf(stdout, "                     (%s %s)\n",
			whoami, lux_version);
		(void) fprintf(stdout, "Controller path:%s\n", path_phys);
		if (state_ptr->c_tbl.hvac_fc)
		    (void) fprintf(stdout,
			"\nWARNING: Fans have failed "
			"-- The SSA may automatically shutdown\n");
		if (state_ptr->c_tbl.hvac_lobt)
		    (void) fprintf(stdout,
			"\nWARNING: NVRAM Battery has failed "
			"-- Fast writes are disabled\n");
		/* Print heading  for SSA100 or SSA200 */
		P_PMSG("                          DEVICE STATUS\n");
		if (*prod_id_ptr > '1') {
			char	*path1;
			int	port, ndevs;
			struct	rsm_es_in	*card_data, *card_data1,
				*card_data2, *card_data3, *card_data4,
				*card_data5;
			card_data = card_data1 = card_data2 = card_data3 =
			card_data4 = card_data5 = (struct rsm_es_in *)NULL;

			ndevs = get_envsen_global_data(argv, argc,
						&path1, &port);
			if (ndevs) {
				card_data = find_card(0);
				card_data1 = find_card(1);
				card_data2 = find_card(2);
				card_data3 = find_card(3);
				card_data4 = find_card(4);
				card_data5 = find_card(5);
			}
			P_PMSG("    TRAY 0            LED     "
			    "TRAY 2            LED     TRAY 4           LED");
			for (nt = 0, t = p = 0;
				nt < state_ptr->c_tbl.num_tgts; nt++) {
				(void) fprintf(stdout, "\n    ");
			    if (card_data) {
				if (card_data->devstat[t].dp)
					(void) fprintf(stdout, "%s %s      ",
						state_ptr->drv[p][t].id1,
						card_data->devstat[t].dl ?
						"Y" : "N");
				else
					(void) fprintf(stdout, "NOT PRESENT"
					"         ");
			    } else {
				(void) fprintf(stdout,
					"%s        ", state_ptr->drv[p][t].id1);
			    }
			    if (card_data2) {
				if (card_data2->devstat[t].dp)
					(void) fprintf(stdout, "%s %s     ",
						state_ptr->drv[p+2][t].id1,
						card_data2->devstat[t].dl ?
						"Y" : "N");
				else
					(void) fprintf(stdout, "NOT PRESENT"
					"         ");
			    } else {
					(void) fprintf(stdout, "%s        ",
					state_ptr->drv[p + 2][t].id1);
			    }
			    if (card_data4) {
				if (card_data4->devstat[t].dp)
					(void) fprintf(stdout, "%s %s ",
						state_ptr->drv[p+4][t].id1,
						card_data4->devstat[t].dl ?
						"Y" : "N");
				else
					(void) fprintf(stdout, "NOT PRESENT"
					"         ");
			    } else {
				(void) fprintf(stdout,
					"%s", state_ptr->drv[p + 4][t].id1);
			    }
			    t++;
			}
			P_PMSG("\n\n    TRAY 1            LED     "
			    "TRAY 3            LED     TRAY 5           LED");
			for (nt = 0, t = 0, p = 1;
				nt < state_ptr->c_tbl.num_tgts; nt++) {
			    (void) fprintf(stdout, "\n    ");
			    if (card_data1) {
				if (card_data1->devstat[t].dp)
					(void) fprintf(stdout, "%s %s      ",
						state_ptr->drv[p][t].id1,
						card_data1->devstat[t].dl ?
						"Y" : "N");
				else
					(void) fprintf(stdout, "NOT PRESENT"
					"         ");
			    } else {
					(void) fprintf(stdout,
					"%s        ", state_ptr->drv[p][t].id1);
			    }
			    if (card_data3) {
				if (card_data3->devstat[t].dp)
					(void) fprintf(stdout, "%s %s     ",
						state_ptr->drv[p+2][t].id1,
						card_data3->devstat[t].dl ?
						"Y" : "N");
				else
					(void) fprintf(stdout, "NOT PRESENT"
					"         ");
			    } else {
					(void) fprintf(stdout, "%s        ",
					state_ptr->drv[p + 2][t].id1);
			    }
			    if (card_data5) {
				if (card_data5->devstat[t].dp)
					(void) fprintf(stdout, "%s %s  ",
						state_ptr->drv[p+4][t].id1,
						card_data5->devstat[t].dl ?
						"Y" : "N");
				else
					(void) fprintf(stdout, "NOT PRESENT"
					"         ");
			    } else {
				(void) fprintf(stdout,
					"%s", state_ptr->drv[p + 4][t].id1);
			    }
			    t++;
			}

		} else {
			P_PMSG("      TRAY 1                 "
			    "TRAY 2                 TRAY 3\n");
			P_PMSG("slot");
			for (i = 0, t = p = 0; i < 10; i++) {
			    if (i == 9) {
				    (void) fprintf(stdout, "\n%d    ", i + 1);
			    } else {
				    (void) fprintf(stdout, "\n%d     ", i + 1);
			    }
			    (void) fprintf(stdout,
				"%s     ", state_ptr->drv[p][t].id1);
			    (void) fprintf(stdout, "%s     ",
				state_ptr->drv[p + 2][t].id1);
			    (void) fprintf(stdout,
				"%s", state_ptr->drv[p + 4][t].id1);
			    t++;
			    if (t > 4) { t = 0; p += 1; }
			}
		}

		P_PMSG("\n\n                          CONTROLLER STATUS\n");
		(void) fprintf(stdout, "Vendor:        %s\n",
		    &state_ptr->c_tbl.c_id.vendor_id[0]);
		(void) fprintf(stdout, "Product ID:    %s\n",
		    &state_ptr->c_tbl.c_id.prod_id[0]);
		(void) fprintf(stdout, "Product Rev:   %s\n",
		    &state_ptr->c_tbl.c_id.revision[0]);
		(void) fprintf(stdout, "Firmware Rev:  %s\n",
		    &state_ptr->c_tbl.c_id.firmware_rev[0]);
		(void) fprintf(stdout, "Serial Num:    %s\n",
		    &state_ptr->c_tbl.c_id.ser_num[0]);
		(void) fprintf(stdout,
		    "Accumulate Performance Statistics: %s\n",
			state_ptr->c_tbl.aps ? "Enabled" : "Disabled");
	    }
	    if (Options & (OPTION_P | OPTION_A | OPTION_Z)) {
		perf_ptr_ptr = &perf_ptr; /* pointer to a pointer */
		if (p_get_perf_statistics(local_path, perf_ptr_ptr)) {
		    P_ERR_PRINT;
		    exit(-1);
		}
		/* wait -t time or 10 seconds */
		if (!(Options & OPTION_T))  delay_time = 10;
		else {
		    if ((delay_time < 1) || (delay_time > 20)) {
			(void) fprintf(stderr,
			    "%s: Invalid delay time\n",
			    whoami);
			exit(-1);
		    }
		}
		P_PMSG("\n                          PERFORMANCE LOG\n");
		P_PMSG("                       ");
		(void) fprintf(stdout, "(Waiting %d seconds)\n", delay_time);

		/* loop */
		not_first_time = 0;
		do {
		    if ((Options & (OPTION_P | OPTION_A)) || not_first_time)
			(void) sleep(delay_time);
		    /* Reread the performance statistics to get real values */
		    if (Options & (OPTION_P | OPTION_A))
			if (p_get_perf_statistics(local_path, perf_ptr_ptr)) {
			    P_ERR_PRINT;
			    exit(-1);
			}
		    if (Options & (OPTION_Z | OPTION_A))
			if (p_get_wb_statistics(local_path, &wle)) {
			    P_ERR_PRINT;
			    exit(-1);
			}
		    /* format and print */
		    P_PMSG("                         ");
		    (void) fprintf(stdout,
			"BUSY: %d%% IOPS: %d\n",
			perf_ptr->ctlr_percent_busy,
			perf_ptr->ctlr_iops);
		    P_PMSG("                          DEVICE IOPS\n");
			if (*prod_id_ptr > '1') {
			P_PMSG("      TRAY 0                 "
				"TRAY 2                 TRAY 4");
			P_PMSG("\ntgt\n");
			for (tgt = port = 0; tgt < state_ptr->c_tbl.num_tgts;
				tgt++) {
			    (void) fprintf(stdout, "%2d     ", tgt);
			    (void) fprintf(stdout,
				"%3d                    ",
				perf_ptr->drive_iops[port][tgt]);
			    (void) fprintf(stdout,
				"%3d                    ",
				perf_ptr->drive_iops[port + 2][tgt]);
			    (void) fprintf(stdout,
				"%3d\n", perf_ptr->drive_iops[port + 4][tgt]);
			}
			P_PMSG("\n      TRAY 1                 "
			    "TRAY 3                 TRAY 5\n");
			P_PMSG("tgt\n");
			for (tgt = 0, port = 1; tgt < state_ptr->c_tbl.num_tgts;
				tgt++) {
			    (void) fprintf(stdout, "%2d     ", tgt);
			    (void) fprintf(stdout,
				"%3d                    ",
				perf_ptr->drive_iops[port][tgt]);
			    (void) fprintf(stdout,
				"%3d                    ",
				perf_ptr->drive_iops[port + 2][tgt]);
			    (void) fprintf(stdout,
				"%3d\n", perf_ptr->drive_iops[port + 4][tgt]);
			}
		    } else {
			P_PMSG("      TRAY 1                 "
			    "TRAY 2                 TRAY 3\n");

			P_PMSG("slot\n");
			for (i = 0, t = p = 0; i < 10; i++) {
			    (void) fprintf(stdout, "%2d     ", i + 1);
			    (void) fprintf(stdout, "%3d                    ",
				    perf_ptr->drive_iops[p][t]);
			    (void) fprintf(stdout, "%3d                    ",
				    perf_ptr->drive_iops[p + 2][t]);
			    (void) fprintf(stdout,
				    "%3d\n", perf_ptr->drive_iops[p + 4][t]);
			    t++;
			    if (t > 4) { t = 0; p += 1; }
			}
		    }
		    if (Options & OPTION_E) {
		    /* extended performance display */
		    P_PMSG("\n                            ");
		    P_PMSG("EXTENDED PERFORMANCE DISPLAY\n");
		    if (*prod_id_ptr > '1') {
			P_PMSG("            TRAY 0                      ");
			P_PMSG("TRAY 2                      TRAY 4\n");
			P_PMSG("       Reads        Writes       ");
		    /* offset for cstyle */
		    P_PMSG("Reads        Writes       Reads        Writes\n");
			P_PMSG("    <2 <8  8 >8  <2 <8  8 >8  <2 <8  ");
			P_PMSG("8 >8  <2 <8  8 >8  <2 <8  8 >8  <2 <8  8 >8\n");
			P_PMSG("tgt\n");
		    } else {
			P_PMSG("            TRAY 1                      ");
			P_PMSG("TRAY 2                      TRAY 3\n");
			P_PMSG("       Reads        Writes       ");
		    /* offset for cstyle */
		    P_PMSG("Reads        Writes       Reads        Writes\n");
			P_PMSG("    <2 <8  8 >8  <2 <8  8 >8  <2 <8  ");
			P_PMSG("8 >8  <2 <8  8 >8  <2 <8  8 >8  <2 <8  8 >8\n");
			P_PMSG("slot\n");
		    }
		    if (*prod_id_ptr > '1') {
			for (i = 0, t = p = 0;
				i < (int)state_ptr->c_tbl.num_tgts;
				i++, t++) {
			    (void) fprintf(stdout, "%2d ", i);
			    cli_ext_perf_disp(perf_ptr, p, t);
			}
			P_PMSG("\n            TRAY 1                      ");
			P_PMSG("TRAY 3                      TRAY 5\n");
			P_PMSG("       Reads        Writes       ");
		    /* offset for cstyle */
		    P_PMSG("Reads        Writes       Reads        Writes\n");
			P_PMSG("    <2 <8  8 >8  <2 <8  8 >8  <2 <8  ");
			P_PMSG("8 >8  <2 <8  8 >8  <2 <8  8 >8  <2 <8  8 >8\n");
			P_PMSG("tgt\n");
			for (i = t = 0, p = 1;
				i < (int)state_ptr->c_tbl.num_tgts;
				i++, t++) {
			    (void) fprintf(stdout, "%2d ", i);
			    cli_ext_perf_disp(perf_ptr, p, t);
			}
		    } else {
			for (i = 0, t = p = 0; i < 10; i++) {
			    (void) fprintf(stdout, "%2d ", i + 1);
			    cli_ext_perf_disp(perf_ptr, p, t);
			    t++;
			    if (t > 4) { t = 0; p += 1; }
			}
		    }
		    }
			(void) fprintf(stdout, "\n");
		    if (Options & (OPTION_Z | OPTION_A))
			cli_display_wb_perf(wle, state_ptr);
		    not_first_time = 1;
		} while (Options & OPTION_L);
	    }
	}
}


static void
cli_display_wb_perf(wle, state_ptr)
	P_state			*state_ptr;
	struct wb_log_entry	*wle;
{
	int		i, j, k;
	char		serno_str[13];
	char		*s;
	int		info_sum[P_NPORTS][P_NTARGETS];
	int		*sum;
	struct wb_log_drive_entry	*wde;

	fprintf(stdout, "WRITE BEHIND MODULE Stats\n\n");
	fprintf(stdout, "Elapsed time: %05d:%02d:%02d.%03d\n",
		wle->hours, wle->minutes, wle->seconds, wle->ms);
	fprintf(stdout, "\tbs_unwritten     = %8d\n", wle->bs_unwritten);
	fprintf(stdout, "\tbs_total         = %8d\n", wle->bs_total);
	fprintf(stdout, "\tbs_inprogress    = %8d\n", wle->bs_inprogress);
	fprintf(stdout, "\tbytes_inprogress = %8dK\n",
					wle->bytes_inprogress >> 10);
	fprintf(stdout, "\tbytes_total      = %8dK\n",
					wle->bytes_total >> 10);
	fprintf(stdout, "\tbytes_unwritten  = %8dK\n",
					wle->bytes_unwritten >> 10);
	fprintf(stdout, "\tbs_thresh        = %8d\n", wle->bs_thresh);
	fprintf(stdout, "\tbattery_ok       = %8d\n", wle->battery_ok);
	fprintf(stdout, "\tfree_bs_cnt      = %8d\n", wle->free_bs_cnt);
	fprintf(stdout, "\tfree_nvram_cnt   = %8d\n", wle->free_nvram_cnt);
	fprintf(stdout, "\tnv_avail         = %8dK\n", wle->nv_avail >> 10);

	fprintf(stdout, "\n");

	wde = &(wle->drv[0][0]);
	sum = &(info_sum[0][0]);
	for (i = 0; i < (int)state_ptr->c_tbl.num_ports; i ++)
	    for (j = 0; j < (int)state_ptr->c_tbl.num_tgts; j ++) {
		*sum = 0;
		*sum += wde->write_cnt;
		*sum += wde->read_cnt;
		*sum += wde->mode_bycmd_fw;
		*sum += wde->mode_bycmd_non_fw;
		*sum += wde->mode_all_fw;
		*sum += wde->mode_all_non_fw;
		*sum += wde->mode_none_fw;
		*sum += wde->mode_none_non_fw;
		*sum += wde->non_read_write;
		*sum += wde->bs_total;
		*sum += wde->bs_unwritten;
		*sum += wde->bs_inprogress;
		*sum += wde->bs_errored;
		*sum += wde->bytes_total;
		*sum += wde->bytes_unwritten;
		*sum += wde->bytes_inprogress;
		*sum += wde->error_cnt;
		*sum += wde->io_cnt;
		*sum += wde->io_nblocks;
		*sum += wde->io_bs_cnt;
		*sum += wde->read_full_overlap;
		*sum += wde->read_overlap_gone;
		*sum += wde->read_partial_overlap;
		*sum += wde->write_cancellations;
		wde ++;
		sum ++;
	}

/* print out table 1 */
#define	WB_HDR1A \
"                                    flush    idle    pend\n"
#define	WB_HDR1B \
"drive        serial#     mode      writes  writes   len  \n"
#define	WB_LINE1A \
"t%dd%d:    <%s>"
#define	WB_LINE1B \
"%8d%8d%8d\n"
	fprintf(stdout, WB_HDR1A);
	fprintf(stdout, WB_HDR1B);
	wde = &(wle->drv[0][0]);
	sum = &(info_sum[0][0]);
	for (i = 0; i < (int)state_ptr->c_tbl.num_ports; i ++)
	    for (j = 0; j < (int)state_ptr->c_tbl.num_tgts; j ++) {
		if (*sum != 0) {
		    for (k = 0, s = serno_str; k < 12; k ++) {
			if (wde->serial_no[k] == 0)
				*s++ = ' ';
			else
				*s++ = wde->serial_no[k];
		    }
		    *s = '\0';
		    fprintf(stdout, WB_LINE1A, i, j, serno_str);
		    cli_print_wb_mode(wde->wb_mode);
		    fprintf(stdout, WB_LINE1B,
			wde->flush_writes,
			wde->drive_idle_writes,
			wde->pend_len);
		}
		wde ++;
		sum ++;
	    }
	fprintf(stdout, "\n");


/* print out table 2 */
#define	WB_HDR2A \
"                             mode ALL      mode BY_CMD      mode NONE\n"
#define	WB_HDR2B \
"drive   #writes  #reads    fw    non-fw    fw    non"\
"-fw    fw    non-fw  non R/W\n"

#define	WB_LINE2 \
"t%dd%d: %8d%8d%8d%8d%8d%8d%8d%8d%8d\n"
	fprintf(stdout, WB_HDR2A);
	fprintf(stdout, WB_HDR2B);
	wde = &(wle->drv[0][0]);
	sum = &(info_sum[0][0]);
	for (i = 0; i < (int)state_ptr->c_tbl.num_ports; i ++)
	    for (j = 0; j < (int)state_ptr->c_tbl.num_tgts; j ++) {
		if (*sum != 0) {
		    fprintf(stdout, WB_LINE2, i, j,
			wde->write_cnt,
			wde->read_cnt,
			wde->mode_all_fw,
			wde->mode_all_non_fw,
			wde->mode_bycmd_fw,
			wde->mode_bycmd_non_fw,
			wde->mode_none_fw,
			wde->mode_none_non_fw,
			wde->non_read_write);
		}
		wde ++;
		sum ++;
	    }
	fprintf(stdout, "\n");


/* print out table 3 */
#define	WB_HDR3A \
"          bs       bs      bs      bs    bytes    bytes   bytes   total\n"
#define	WB_HDR3B \
"drive    total   ~wrote  inprog  errored total   ~wrote  inprog  err cnt\n"
#define	WB_LINE3 \
"t%dd%d: %8d%8d%8d%8d%7dK%7dK%7dK%8d\n"
	fprintf(stdout, WB_HDR3A);
	fprintf(stdout, WB_HDR3B);
	wde = &(wle->drv[0][0]);
	sum = &(info_sum[0][0]);
	for (i = 0; i < (int)state_ptr->c_tbl.num_ports; i ++)
	    for (j = 0; j < (int)state_ptr->c_tbl.num_tgts; j ++) {
		if (*sum != 0) {
		    fprintf(stdout, WB_LINE3, i, j,
			wde->bs_total,
			wde->bs_unwritten,
			wde->bs_inprogress,
			wde->bs_errored,
			wde->bytes_total >> 10,
			wde->bytes_unwritten >> 10,
			wde->bytes_inprogress >> 10,
			wde->error_cnt);
		}
		wde ++;
		sum ++;
	    }
	fprintf(stdout, "\n");

/* print out table 4 */
#define	WB_HDR4A \
"                                  full   partial  gone   write\n"
#define	WB_HDR4B \
"drive    io cnt  #blocks  #bs    overlap overlap overlap cancel\n"
#define	WB_LINE4 \
"t%dd%d: %8d%8d%8d%8d%8d%8d%8d\n"
	fprintf(stdout, WB_HDR4A);
	fprintf(stdout, WB_HDR4B);
	wde = &(wle->drv[0][0]);
	sum = &(info_sum[0][0]);
	for (i = 0; i < (int)state_ptr->c_tbl.num_ports; i ++)
	    for (j = 0; j < (int)state_ptr->c_tbl.num_tgts; j ++) {
		if (*sum != 0) {
		    fprintf(stdout, WB_LINE4, i, j,
			wde->io_cnt,
			wde->io_nblocks,
			wde->io_bs_cnt,
			wde->read_full_overlap,
			wde->read_overlap_gone,
			wde->read_partial_overlap,
			wde->write_cancellations);
		}
		wde ++;
		sum ++;
	    }
	fprintf(stdout, "\n");
}

static void
cli_print_wb_mode(mode)
int	mode;
{
	switch (mode) {
	case 1:		fprintf(stdout, " NONE   ");	break;
	case 2:		fprintf(stdout, " ALL    ");	break;
	case 4:		fprintf(stdout, " BY CMD ");	break;
	case 8:		fprintf(stdout, "MISSING ");	break;
	case 16:	fprintf(stdout, "ERRORED ");	break;
	case 32:	fprintf(stdout, "BATT BAD");	break;
	default:	fprintf(stdout, "Unknown ");	break;
	}
}

static void
cli_display_nvram_data(char *local_path, P_state *state_ptr,
		int disk, u_char port, u_char target)
{
	int i, j;
	struct wb_log_entry		*wle = NULL;
	struct wb_log_drive_entry	*wde;

	if (p_get_wb_statistics(local_path, &wle)) {
		    P_ERR_PRINT;
		    fprintf(stderr, "could not get NVRAM data\n");
		    return;
	}
	if (disk) {
		wde = &wle->drv[port][target];
		fprintf(stdout, "t%dd%d: Unwritten bytes = %d\n", port, target,
			wde->bytes_unwritten);
	} else {
		wde = &wle->drv[0][0];
		for (i = 0; i < (int)state_ptr->c_tbl.num_ports; i++)
			for (j = 0; j < (int)state_ptr->c_tbl.num_tgts; j++) {
			    fprintf(stdout, "t%dd%d: Unwritten bytes = %d\n",
				i, j, wde->bytes_unwritten);
			    wde++;
			}
	}
}

void
ssa_cli_start(char **argv, int tray_number)
{
int	i;
char	*path_phys;	/* physical path */
char	local_path[MAXNAMELEN];
char	temp_string[MAXNAMELEN];
char	drv_path[MAXNAMELEN];
int	fd;
int	nbr_trays = NBR_TRAYS;
int	nbr_drives_per_tray = NBR_DRIVES_PER_TRAY;
int	ssa200_flag = 0;
u_int	min_tray_nbr = 1;	/* default minimum tray number */
P_state	*state_ptr = NULL;
int	initial_update = 1;
u_char	port, tgt;
char *prod_id_ptr;

	if (Options & OPTION_T) {
	    if ((path_phys =
		get_physical_name(*argv)) == NULL) {
		(void) fprintf(stderr,
		    "%s: Invalid path name (%s)\n", whoami,
		    *argv);
		exit(-1);
	    }

	    (void) strcpy(local_path, path_phys);

	    /* Get global status */
	    if (p_get_status(local_path, &state_ptr,
		initial_update, (Options & PVERBOSE))) {
		P_ERR_PRINT;
		exit(-1);
	    }
	    if (strstr(state_ptr->c_tbl.c_id.prod_id, SSASTR) == NULL) {
		p_error_msg_ptr = "Invalid Product ID";
		P_ERR_PRINT;
		p_error_msg_ptr = NULL;
		exit(-1);
	    }
	    prod_id_ptr = strstr(state_ptr->c_tbl.c_id.prod_id, SSASTR) +
			    strlen(SSASTR);
		/*
		 * Check to see if SSA100 or SSA200
		 * If 200 then 6 trays.
		 */
	    if (*prod_id_ptr > '1') {
		nbr_trays = NBR_200_TRAYS - 1;
		nbr_drives_per_tray = state_ptr->c_tbl.num_tgts;
		min_tray_nbr = 0;
		ssa200_flag = 1;
	    }
	    if (tray_number < min_tray_nbr || tray_number > nbr_trays) {
		(void) fprintf(stderr,
		    "%s: Illegal tray number %x\n",
		    whoami, tray_number);
		exit(-1);
	    }
	    VERBPRINT("Starting all drives in tray %x:\n", tray_number);
	    if (p_get_drv_name(path_phys, drv_path)) {
		    P_ERR_PRINT;
		    exit(-1);
	    }

	    for (i = 0; i < nbr_drives_per_tray; i++) {
		/* create path name for unit */
		/* SUNW,pln@a5b1,298b42c3/SUNW,s[s]d@[0..x],[0..y]:[a..h],raw */
		(void) strcpy(local_path, drv_path);
		strcpy(temp_string, "p,t:c,raw");
		if (ssa200_flag) {
			port = (u_char)tray_number;
			tgt = (u_char)i;
		} else {
			port = (tray_number -1)*2 + i/5;
			tgt = i%5;
		}
		temp_string[0] = port + '0';
		temp_string[2] = tgt + '0';
		(void) strcat(local_path, temp_string);

		/*
		 * Skip drives that don't have path names or
		 * are No Drive Select.
		 * The boot won't create names for drives that
		 * don't exist.
		 */
		if (!(state_ptr->drv[port][tgt].state_flags & DS_NDS)) {
		    if ((fd = open(local_path, O_NDELAY | O_RDONLY)) == -1) {
			    P_DPRINTF("Skipping drive:\n %s\n", local_path);
			    continue;
		    } else {
			    (void) close(fd);
		    }
		    P_DPRINTF("Issuing start to drive:\n %s\n", local_path);

		    if (p_start(local_path))  {
			/*
			 * Note: ignoring errors
			 * We know the drive did exist and
			 * the open should not fail so the only
			 * failure is if the drive is broken.
			 * We want to start or stop all of the drives
			 * in the
			 * tray so continue even if one is broken.
			 */
			P_DPRINTF("Start cmd to drive failed:\n %s\n",
			local_path);
			VERBPRINT("Start cmd to drive failed:\n %s\n",
			local_path);
		    }
		}
	    }
	} else {
	    while (*argv != NULL) {
		if ((path_phys =
		    get_physical_name(*argv)) == NULL) {
		    (void) fprintf(stderr,
			"%s: Invalid path name (%s)\n", whoami,
			*argv);
		    exit(-1);
		}
		VERBPRINT("Issuing start to:\n %s\n", *argv);
		if (p_start(path_phys))  {
		    P_ERR_PRINT;
		    exit(-1);
		}
		(argv)++;
	    }
	}
}

void
ssa_cli_stop(char **argv, int tray_number)
{
int	i;
char	*path_phys;	/* physical path */
char	drv_path[MAXNAMELEN];
char	local_path[MAXNAMELEN];
char	temp_string[MAXNAMELEN];
int	fd;
int	nbr_trays = NBR_TRAYS;
int	nbr_drives_per_tray = NBR_DRIVES_PER_TRAY;
u_int	min_tray_nbr = 1;	/* default minimum tray number */
int	ssa200_flag = 0;
P_state	*state_ptr = NULL;
int	initial_update = 1;
u_char	port, tgt;
char *prod_id_ptr;

	if (Options & OPTION_T) {
	    if ((path_phys =
		get_physical_name(*argv)) == NULL) {
		(void) fprintf(stderr,
		    "%s: Invalid path name (%s)\n", whoami,
		    *argv);
		exit(-1);
	    }
	    (void) strcpy(local_path, path_phys);

	    /* Get global status */
	    if (p_get_status(local_path, &state_ptr,
		initial_update, (Options & PVERBOSE))) {
		P_ERR_PRINT;
		exit(-1);
	    }
	    if (strstr(state_ptr->c_tbl.c_id.prod_id, SSASTR) == NULL) {
		p_error_msg_ptr = "Invalid Product ID";
		P_ERR_PRINT;
		p_error_msg_ptr = NULL;
		exit(-1);
	    }
	    prod_id_ptr = strstr(state_ptr->c_tbl.c_id.prod_id, SSASTR) +
			    strlen(SSASTR);
		/*
		 * Check to see if SSA100 or SSA200
		 * If 200 then 6 trays.
		 */
	    if (*prod_id_ptr > '1') {
		nbr_trays = NBR_200_TRAYS - 1;
		nbr_drives_per_tray = state_ptr->c_tbl.num_tgts;
		min_tray_nbr = 0;
		ssa200_flag = 1;
	    }
	    if (tray_number < min_tray_nbr || tray_number > nbr_trays) {
		(void) fprintf(stderr,
		    "%s: Illegal tray number %x\n",
		    whoami, tray_number);
		exit(-1);
	    }
	    VERBPRINT("Stopping all drives in tray %x:\n", tray_number);
	    if (p_get_drv_name(path_phys, drv_path)) {
		    P_ERR_PRINT;
		    exit(-1);
	    }

	    for (i = 0; i < nbr_drives_per_tray; i++) {
		/* create path name for unit */
		/* SUNW,pln@a5b1,298b42c3/SUNW,s[s]d@[0..x],[0..y]:[a..h],raw */
		(void) strcpy(local_path, drv_path);
		strcpy(temp_string, "p,t:c,raw");
		if (ssa200_flag) {
			port = (u_char)tray_number;
			tgt = (u_char)i;
		} else {
			port = (tray_number -1)*2 + i/5;
			tgt = i%5;
		}
		temp_string[0] = port + '0';
		temp_string[2] = tgt + '0';
		(void) strcat(local_path, temp_string);

		/*
		 * Skip drives that don't have path names or
		 * are No Drive Select or for the stop skip
		 * drives that are already spun down.
		 * The boot won't create names for drives that
		 * don't exist.
		 */
		if (!(state_ptr->drv[port][tgt].state_flags &
			(DS_SPUN_DWN | DS_NDS))) {
		    if ((fd = open(local_path, O_NDELAY | O_RDONLY)) == -1) {
			    P_DPRINTF("Skipping drive:\n %s\n", local_path);
			    continue;
		    } else {
			    (void) close(fd);
		    }
		    P_DPRINTF("Issuing stop to drive:\n %s\n", local_path);

		    if (p_stop(local_path))  {
			/*
			 * Note: ignoring errors
			 * We know the drive did exist and
			 * the open should not fail so the only
			 * failure is if the drive is broken.
			 * We want to start or stop all of
			 * the drives in the
			 * tray so continue even if one is broken.
			 */
			P_DPRINTF("Stop cmd to drive failed:\n %s\n",
			local_path);
			VERBPRINT("Stop cmd to drive failed:\n %s\n",
			local_path);
		    }
		}
	    }
	} else {
	    while (*argv != NULL) {
		if ((path_phys =
		    get_physical_name(*argv)) == NULL) {
		    (void) fprintf(stderr,
			"%s: Invalid path name (%s)\n", whoami,
			*argv);
		    exit(-1);
		}
		VERBPRINT("Issuing stop to:\n %s\n", *argv);
		if (p_stop(path_phys))  {
		    P_ERR_PRINT;
		    exit(-1);
		}
		(argv)++;
	    }
	}
}

void
ssa_fast_write(char *path)
{
char 		*path_phys;	    /* physical path */
int		pcfw_flag, fwe_flag;

		if ((path_phys =
		    get_physical_name(path)) == NULL) {
		    (void) fprintf(stderr,
			"%s: Invalid path name (%s)\n", whoami,
			path);
		    exit(-1);
		}
		if (Options & OPTION_D) {
		    VERBPRINT("Disabling Fast Writes ");
		    pcfw_flag = fwe_flag = 0;
		}
		if (Options & OPTION_E)  {
		    VERBPRINT("Enabeling Fast Writes ");
		    pcfw_flag = 0;
		    fwe_flag = 1;
		}
		if (Options & OPTION_C)  {
		    VERBPRINT("The Fast Write function will "
		    "use the command bit ");
		    pcfw_flag = 1;
		}

		if (Options & SAVE) {
			VERBPRINT("(and saving)");
		}

		VERBPRINT(" for:\n %s\n", path);

		if (p_fast_write(path_phys, pcfw_flag, fwe_flag,
					((Options & SAVE) != 0)))  {
		    P_ERR_PRINT;
		    exit(-1);
		}
}

void
ssa_perf_statistics(char *path)
{
char 		*path_phys;	    /* physical path */
int		aps_flag;

		if (Options & OPTION_D) {
		    VERBPRINT("Disabling the accumulation of "
		    "Performance statistics");
		    aps_flag = 0;
		}
		if (Options & OPTION_E)  {
		    VERBPRINT("Enabeling the accumulation of "
		    "Performance statistics");
		    aps_flag = 1;
		}
		VERBPRINT(" and saving for:\n %s\n", path);
		if ((path_phys =
		    get_physical_name(path)) == NULL) {
		    (void) fprintf(stderr,
			"%s: Invalid path name (%s)\n", whoami,
			path);
		    exit(-1);
		}
		if ((strstr(path_phys, ":ctlr")) == NULL) {
		    (void) fprintf(stderr,
			"%s: Invalid path name (%s)\n", whoami,
			path);
		    exit(-1);
		}
		if (p_set_perf_statistics(path_phys, aps_flag)) {
		    P_ERR_PRINT;
		    exit(-1);
		}
}
