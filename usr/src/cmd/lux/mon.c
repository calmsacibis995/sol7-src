/*
 * Copyright 1997 Sun Microsystems, Inc. All rights reserved.
 */

#pragma ident   "@(#)mon.c 1.34     98/02/09 SMI"

/*LINTLIBRARY*/


/*
 *	This module is part of the photon Command Line
 *	Interface program.
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
#include	<string.h>
#include	<ctype.h>
#include	<assert.h>
#include	<libintl.h>	    /* gettext */
#include	<sys/systeminfo.h>
#include	<sys/scsi/scsi.h>
#include	<dirent.h>	/* for DIR */
#include	<sys/vtoc.h>
#include	<sys/dkio.h>
#include	<nl_types.h>
#include	<strings.h>
#include	<sys/utsname.h>
#include	<sys/ddi.h>	/* for max */
#include	<sys/mode.h>
#include	"rom.h"
#include	"exec.h"
#include	"luxdef.h"
#include	"state.h"
#ifdef	DEVCTL_LIB
#include	<sys/devctl.h>
#endif	/* DEVCTL_LIB */


/* global variables */
char 	*l_error_msg_ptr;	/* pointer to error message */
extern	char	*p_error_msg_ptr;	/* SSA lib error pointer */
char	l_error_msg[80*200];	/* global error messsage - 200 lines long */
extern	nl_catd l_catd;
int	name_id;		/* Use id's in /devices names */

u_char l_switch_to_alpa[] = {
	0xef, 0xe8, 0xe4, 0xe2, 0xe1, 0xe0, 0xdc, 0xda, 0xd9, 0xd6,
	0xd5, 0xd4, 0xd3, 0xd2, 0xd1, 0xce, 0xcd, 0xcc, 0xcb, 0xca,
	0xc9, 0xc7, 0xc6, 0xc5, 0xc3, 0xbc, 0xba, 0xb9, 0xb6, 0xb5,
	0xb4, 0xb3, 0xb2, 0xb1, 0xae, 0xad, 0xac, 0xab, 0xaa, 0xa9,
	0xa7, 0xa6, 0xa5, 0xa3, 0x9f, 0x9e, 0x9d, 0x9b, 0x98, 0x97,
	0x90, 0x8f, 0x88, 0x84, 0x82, 0x81, 0x80, 0x7c, 0x7a, 0x79,
	0x76, 0x75, 0x74, 0x73, 0x72, 0x71, 0x6e, 0x6d, 0x6c, 0x6b,
	0x6a, 0x69, 0x67, 0x66, 0x65, 0x63, 0x5c, 0x5a, 0x59, 0x56,
	0x55, 0x54, 0x53, 0x52, 0x51, 0x4e, 0x4d, 0x4c, 0x4b, 0x4a,
	0x49, 0x47, 0x46, 0x45, 0x43, 0x3c, 0x3a, 0x39, 0x36, 0x35,
	0x34, 0x33, 0x32, 0x31, 0x2e, 0x2d, 0x2c, 0x2b, 0x2a, 0x29,
	0x27, 0x26, 0x25, 0x23, 0x1f, 0x1e, 0x1d, 0x1b, 0x18, 0x17,
	0x10, 0x0f, 0x08, 0x04, 0x02, 0x01
};

u_char l_sf_alpa_to_switch[] = {
	0x00, 0x7d, 0x7c, 0x00, 0x7b, 0x00, 0x00, 0x00, 0x7a, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x79, 0x78, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x77, 0x76, 0x00, 0x00, 0x75, 0x00, 0x74,
	0x73, 0x72, 0x00, 0x00, 0x00, 0x71, 0x00, 0x70, 0x6f, 0x6e,
	0x00, 0x6d, 0x6c, 0x6b, 0x6a, 0x69, 0x68, 0x00, 0x00, 0x67,
	0x66, 0x65, 0x64, 0x63, 0x62, 0x00, 0x00, 0x61, 0x60, 0x00,
	0x5f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5e, 0x00, 0x5d,
	0x5c, 0x5b, 0x00, 0x5a, 0x59, 0x58, 0x57, 0x56, 0x55, 0x00,
	0x00, 0x54, 0x53, 0x52, 0x51, 0x50, 0x4f, 0x00, 0x00, 0x4e,
	0x4d, 0x00, 0x4c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4b,
	0x00, 0x4a, 0x49, 0x48, 0x00, 0x47, 0x46, 0x45, 0x44, 0x43,
	0x42, 0x00, 0x00, 0x41, 0x40, 0x3f, 0x3e, 0x3d, 0x3c, 0x00,
	0x00, 0x3b, 0x3a, 0x00, 0x39, 0x00, 0x00, 0x00, 0x38, 0x37,
	0x36, 0x00, 0x35, 0x00, 0x00, 0x00, 0x34, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x33, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x31, 0x30, 0x00, 0x00, 0x2f, 0x00, 0x2e, 0x2d, 0x2c,
	0x00, 0x00, 0x00, 0x2b, 0x00, 0x2a, 0x29, 0x28, 0x00, 0x27,
	0x26, 0x25, 0x24, 0x23, 0x22, 0x00, 0x00, 0x21, 0x20, 0x1f,
	0x1e, 0x1d, 0x1c, 0x00, 0x00, 0x1b, 0x1a, 0x00, 0x19, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x17, 0x16, 0x15,
	0x00, 0x14, 0x13, 0x12, 0x11, 0x10, 0x0f, 0x00, 0x00, 0x0e,
	0x0d, 0x0c, 0x0b, 0x0a, 0x09, 0x00, 0x00, 0x08, 0x07, 0x00,
	0x06, 0x00, 0x00, 0x00, 0x05, 0x04, 0x03, 0x00, 0x02, 0x00,
	0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

/*	external functions */
extern	void	destroy_data(char *);
extern	int	p_get_drv_name(char *, char *);
extern	void	p_dump(char *, u_char *, int, int);
extern	int	scsi_ib_download_code_cmd(int, int, int, u_char *, int, int);
extern	int	scsi_inquiry_cmd(int, u_char *, int);
extern	int	scsi_read_capacity_cmd(int, u_char *, int);
extern	int	scsi_reserve_cmd(int);
extern	int	scsi_release_cmd(int);
extern	int	scsi_start_cmd(int);
extern	int	scsi_stop_cmd(int, int);
extern	int	scsi_rec_diag_cmd(int, u_char *, int, u_char);
extern	int	scsi_send_diag_cmd(int, u_char *, int);
extern	int	scsi_tur(int);
extern	int	scsi_mode_sense_cmd(int, u_char *, int, u_char, u_char);
extern	char	*get_physical_name_from_link(char *);
extern  int	object_open(char *, int);
extern	void	*zalloc(int);
extern	char	*alloc_string(char *);
extern	void	ll_to_str(u_char *, char *);
extern	int	scsi_reset(int);
extern	char	*get_physical_name(char *);
extern	int	scsi_persistent_reserve_in_cmd(int, u_char *, int, u_char);

/*
 * Global variables
 */
extern WWN_list	*g_WWN_list;

/*	internal functions */
static	int	get_ib_status(char *, struct l_state_struct *, int);
int		l_get_envsen_page(int, u_char *, int, u_char, int);
int		l_get_wwn(char *, u_char port_wwn[], u_char node_wwn[],
		int *, int);
int		l_get_inquiry(char *, L_inquiry *);
int		l_get_dev_map(char *, sf_al_map_t *, int);
int		l_get_ses_path(char *, char *, sf_al_map_t *, int);
int		l_get_target_id(char *, char *, int *, char *);
int		l_get_status(char *, struct l_state_struct *, int);
struct dlist	*l_get_multipath(char *, int);
struct dlist	*l_get_allses(char *, struct box_list_struct *, int);
void		l_free_multipath(struct dlist *);
int		l_get_slot(struct path_struct *, int);
int		l_dev_pwr_up_down(char *, char *, struct path_struct *,
		int, int);
int		l_pho_pwr_up_down(char *, char *, int, int);
int		l_pwr_up_down(char *, int, int, int, int);
int		l_get_port(char *, int *, int);
int		l_get_disk_port_status(char *, struct l_disk_state_struct *,
		int, int);
int		l_ex_open_test(struct dlist *, char *);


#define	VERBPRINT	if (verbose) (void) printf

#define	RD_PG		0x20120

/*
 * Read all mode pages.
 */
/*ARGSUSED*/
int
l_get_mode_pg(char *path, u_char **pg_buf, int verbose)
{
Mode_header_10	*mode_header_ptr;
int	status, size, fd;

	P_DPRINTF("  get_mode_pg: Reading Mode Sense pages.\n");

	/* open controller */
	if ((fd = object_open(path, O_NDELAY | O_RDWR)) == -1)
		return (errno);

	/*
	 * Read the first part of the page to get the page size
	 */
	size = 20;
	if ((*pg_buf = (u_char *)zalloc(size)) == NULL) {
	    (void) close(fd);
	    return (errno);
	}
	/* read page */
	if (status = scsi_mode_sense_cmd(fd, *pg_buf, size,
	    0, MODEPAGE_ALLPAGES)) {
	    (void) close(fd);
	    free((char *)*pg_buf);
	    return (status);
	}
	/* Now get the size for all pages */
	mode_header_ptr = (struct mode_header_10_struct *)(int)*pg_buf;
	size = mode_header_ptr->length + sizeof (mode_header_ptr->length);
	free((char *)*pg_buf);
	if ((*pg_buf = (u_char *)zalloc(size)) == NULL) {
	    (void) close(fd);
	    return (errno);
	}
	/* read all pages */
	if (status = scsi_mode_sense_cmd(fd, *pg_buf, size,
	    0, MODEPAGE_ALLPAGES)) {
	    (void) close(fd);
	    free((char *)*pg_buf);
	    return (status);
	}
	(void) close(fd);
	return (0);
}

/*
 * Get the indexes to the disk device elements in page 2,
 * based on the locations found in page 1.
 *
 * RETURN:
 *	0 = O.K.
 */
int
l_get_disk_element_index(struct l_state_struct *l_state, int *front_index,
	int *rear_index)
{
int	index = 0, front_flag = 0, local_front = 0, local_rear = 0;
int	i, rear_flag = 0;

	*front_index = *rear_index = 0;
	/* Get the indexes to the disk device elements */
	for (i = 0; i < (int)l_state->ib_tbl.config.enc_num_elem; i++) {
		if (l_state->ib_tbl.config.type_hdr[i].type == ELM_TYP_DD) {
			if (front_flag) {
				local_rear = index;
				rear_flag = 1;
				break;
			} else {
				local_front = index;
				front_flag = 1;
			}
		}
		index += l_state->ib_tbl.config.type_hdr[i].num;
		index++;		/* for global element */
	}
	if (getenv("_LUX_D_DEBUG") != NULL) {
		(void) fprintf(stdout,
			"  l_get_disk_element_index:"
			" Index to front disk elements 0x%x\n"
			"  l_get_disk_element_index:"
			" Index to rear disk elements 0x%x\n",
			local_front, local_rear);
	}
	if (!front_flag || !rear_flag) {
		(void) sprintf(l_error_msg,
		MSGSTR(-1, " could not find the disk elements"
		" in the Receive Diagnostic pages.\n"));
		l_error_msg_ptr = (char *)&l_error_msg;
		return (-1);
	}
	*front_index = local_front;
	*rear_index = local_rear;
	return (0);
}

/*
 * manage the device led's
 *
 */
int
l_led(struct path_struct *path_struct, int led_action,
	struct device_element *status,
	int verbose)
{
sf_al_map_t	map;
char		ses_path[MAXPATHLEN];
int		err;
u_char		*page_buf;
int 		write, fd, front_index, rear_index, offset;
unsigned short	page_len;
struct	device_element *elem;
L_state		l_state;

	/*
	 * Need to get a valid location, front/rear & slot.
	 *
	 * The path_struct will return a valid slot
	 * and the IB path or a disk path.
	 */

	if (!path_struct->ib_path_flag) {
		if (err = l_get_dev_map(path_struct->p_physical_path,
			&map, verbose)) {
			return (err);
		}
		if (err = l_get_ses_path(path_struct->p_physical_path,
			ses_path, &map, verbose)) {
			return (err);
		}
	} else {
		(void) strcpy(ses_path, path_struct->p_physical_path);
	}
	if (!path_struct->slot_valid) {
		/* We are passing the disks path */
		if (err = l_get_slot(path_struct, verbose)) {
				return (err);
		}
	}

	if ((page_buf = (u_char *)malloc(MAX_REC_DIAG_LENGTH)) == NULL) {
		return (-1);
	}

	if ((fd = object_open(ses_path, O_NDELAY | O_RDWR)) == -1) {
		(void) free(page_buf);
		return (errno);
	}

	if (err = l_get_envsen_page(fd, page_buf, MAX_REC_DIAG_LENGTH,
				L_PAGE_2, verbose)) {
		(void) close(fd);
		(void) free(page_buf);
		return (err);
	}

	page_len = (page_buf[2] << 8 | page_buf[3]) + HEADER_LEN;

	/* Get index to the disk we are interested in */
	if (err = l_get_status(ses_path, &l_state, verbose)) {
		(void) close(fd);
		(void) free(page_buf);
		return (err);
	}
	/* Double check slot as convert_name only does gross check */
	if (path_struct->slot >= l_state.total_num_drv/2) {
		(void) sprintf(l_error_msg,
			MSGSTR(-1,
			"Invalid path, invalid slot %d\n"),
			path_struct->slot);
		l_error_msg_ptr = (char *)&l_error_msg;
		return (L_INVALID_PATH);
	}

	if (err = l_get_disk_element_index(&l_state, &front_index,
		&rear_index)) {
		return (err);
	}
	/* Skip global element */
	front_index++;
	rear_index++;

	if (path_struct->f_flag) {
		offset = (8 + (front_index + path_struct->slot)*4);
	} else {
		offset = (8 + (rear_index + path_struct->slot)*4);
	}

	elem = (struct device_element *)((int)page_buf + offset);
	/*
	 * now do requested action.
	 */
	bcopy((const void *)elem, (void *)status,
		sizeof (struct device_element));	/* save status */
	bzero(elem, sizeof (struct device_element));
	elem->select = 1;
	elem->dev_off = status->dev_off;
	elem->en_bypass_a = status->en_bypass_a;
	elem->en_bypass_b = status->en_bypass_b;
	write = 1;

	switch (led_action) {
	case	L_LED_STATUS:
		write = 0;
		break;
	case	L_LED_RQST_IDENTIFY:
		elem->ident = 1;
		if (verbose) {
			(void) fprintf(stdout,
			MSGSTR(-1, "  Blinking LED for slot %d in enclosure"
			" %s\n"), path_struct->slot,
			l_state.ib_tbl.enclosure_name);
		}
		break;
	case	L_LED_OFF:
		if (verbose) {
			(void) fprintf(stdout,
			MSGSTR(-1, "  Turning off LED for slot %d in enclosure"
			" %s\n"), path_struct->slot,
			l_state.ib_tbl.enclosure_name);
		}
		break;
	default:
		(void) sprintf(l_error_msg,
			MSGSTR(-1,
			"Invalid LED request.\n"));
		l_error_msg_ptr = (char *)&l_error_msg;
		return (L_INVALID_PATH);
	} /* End of switch */

	if (write) {
		if (getenv("_LUX_D_DEBUG") != NULL) {
			p_dump("  l_led: Updating led state: "
			"Device Status Element ",
			(u_char *)elem, sizeof (struct device_element),
			HEX_ONLY);
		}
		if (err = scsi_send_diag_cmd(fd,
			(u_char *)page_buf, page_len)) {
			(void) close(fd);
			(void) free(page_buf);
			return (err);
		}

		bzero(page_buf, MAX_REC_DIAG_LENGTH);
		if (err = l_get_envsen_page(fd, page_buf, MAX_REC_DIAG_LENGTH,
					L_PAGE_2, verbose)) {
			(void) free(page_buf);
			(void) close(fd);
			return (err);
		}
		elem = (struct device_element *)((int)page_buf + offset);
		bcopy((const void *)elem, (void *)status,
			sizeof (struct device_element));
	}
	if (getenv("_LUX_D_DEBUG") != NULL) {
		p_dump("  l_led: Device Status Element ",
		(u_char *)status, sizeof (struct device_element),
		HEX_ONLY);
	}

	(void) close(fd);
	(void) free(page_buf);
	return (0);
}

/*
 * For 2.5.1 instead of offlining the drive we do an
 * exclusive open to see if it is busy.
 * We test all multipaths.
 */
int
l_ex_open_test(struct dlist *dl, char *device_name)
{
#ifndef DEVCTL_LIB
int		fd;
struct dlist	*dlh;
char		*part;
char		path[MAXPATHLEN];

	dlh = dl;
	while (dlh) {
		/*
		 * Note:  The dev_path should be a physical path of the form:
		 *
		 *	/devices/.../ssd@<X>,<Y>:c<,raw>
		 *
		 * So the character after the last colon should be the
		 * partition ID.
		 */
		(void) strcpy(path, dlh->dev_path);
		if ((part = strrchr(path, ':')) == NULL) {
			sprintf(l_error_msg,
				MSGSTR(-1, "%s -- invalid path\n"),
				path);
			l_error_msg_ptr = &l_error_msg[0];
			return (-1);
		}
		part++; /* We now point at the 'c' */
		for (*part = 'a'; *part < 'i'; (*part)++) {
			if ((fd = open(path, O_RDWR)) < 0 && errno != EBUSY) {
				/* Partition is not configured -- next */
				continue;
			}
			close(fd);
			if ((fd = open(path, O_RDWR | O_EXCL)) < 0) {
				sprintf(l_error_msg,
					MSGSTR(-1, "Could not open %s in "
					"exclusive mode.\n   "
					"May already be open.\n"),
					device_name);
				l_error_msg_ptr = &l_error_msg[0];
				return (-1);
			}
			close(fd);
		}
		*part = 'c';
		dlh = dlh->next;
	}
#endif
	return (0);
}

/*
 * Set the state of an individual disk in the Photon enclosure
 * the powered up/down mode.
 *
 * The path must point to a disk or the ib_path_flag must be set.
 */
int
l_dev_pwr_up_down(char *dev_name, char *path_phys,
	struct path_struct *path_struct, int power_off_flag, int verbose)
/*ARGSUSED*/
{
sf_al_map_t	map;
char		ses_path[MAXPATHLEN], dev_path[MAXPATHLEN];
char		dev_path1[MAXPATHLEN];
int		slot, err, port_a, port_b, port_a_b;
L_state		l_state;
struct dlist	*dl, *dl1;
#ifdef	DEVCTL_LIB
devctl_hdl_t	devhdl;
uint_t		dev_state;
#endif	/* DEVCTL_LIB */

	dl = (struct dlist *)NULL;

	/* Clear the old  error messages */
	l_error_msg_ptr = (char *)NULL;

	if (err = l_get_dev_map(path_struct->p_physical_path,
					&map, verbose)) {
		return (err);
	}
	if (err = l_get_ses_path(path_struct->p_physical_path,
				ses_path, &map, verbose)) {
		return (err);
	}
	if (err = l_get_status(ses_path, &l_state, verbose)) {
		return (err);
	}
	if (!path_struct->slot_valid) {
		/* We are passing the disks path */
		if (err = l_get_slot(path_struct, verbose)) {
			return (err);
		}
	}

	slot = path_struct->slot;
	(void) strcpy(dev_path, path_struct->p_physical_path);

	/*
	 * Check disk state
	 * before do a power off
	 *
	 */
	if (power_off_flag) {
		goto pre_pwr_dwn;
	} else {
		goto pwr_up_dwn;
	}

pre_pwr_dwn:

	/*
	 * Check whether disk
	 * is reserved by another
	 * host
	 */
	if (path_struct->f_flag) {
		if ((l_state.drv_front[slot].d_state_flags[PORT_A] &
						L_RESERVED) ||
		(l_state.drv_front[slot].d_state_flags[PORT_B] &
						L_RESERVED)) {
			(void) sprintf(l_error_msg,
				MSGSTR(-1,
				" Disk %s is reserved.\n\n"),
				dev_name);
			l_error_msg_ptr = l_error_msg;
			return (-1);
		}
		if (l_state.drv_front[slot].ib_status.code
					== S_NOT_INSTALLED) {
			(void) sprintf(l_error_msg,
				MSGSTR(-1,
				"Slot is empty.\n"));
			l_error_msg_ptr = l_error_msg;
			return (-1);
		}
	} else {
		if ((l_state.drv_rear[slot].d_state_flags[PORT_A] &
						L_RESERVED) ||
		(l_state.drv_rear[slot].d_state_flags[PORT_B] &
						L_RESERVED)) {
			(void) sprintf(l_error_msg,
				MSGSTR(-1,
				" Disk %s is reserved.\n\n"),
				dev_name);
			l_error_msg_ptr = l_error_msg;
			return (-1);
		}
		if (l_state.drv_rear[slot].ib_status.code
					== S_NOT_INSTALLED) {
			(void) sprintf(l_error_msg,
				MSGSTR(-1,
				"Slot is empty.\n"));
			l_error_msg_ptr = l_error_msg;
			return (-1);
		}
	}

	if ((dl = (struct dlist *)
			zalloc(sizeof (struct dlist))) == NULL) {
		(void) fprintf(stderr,
			MSGSTR(-1,
			" Out of memory.\n"));
			exit(-1);
	}

	dl->dev_path = dev_path;
	dl->multipath = l_get_multipath(dev_path, verbose);
	if (path_struct->f_flag) {
		port_a =
			l_state.drv_front[slot].d_state_flags[PORT_A];
		port_b =
			l_state.drv_front[slot].d_state_flags[PORT_B];
	} else {
		port_a =
			l_state.drv_rear[slot].d_state_flags[PORT_A];
		port_b =
			l_state.drv_rear[slot].d_state_flags[PORT_B];
	}
	port_a_b = port_a & port_b;
	if (!(port_a_b & L_NO_LOOP)) {
		if (port_a & L_NO_LOOP) {
			port_a = 0;
		}
		if (port_b & L_NO_LOOP) {
			port_b = 0;
		}
	}
	if (!(port_a_b & (L_NOT_READY | L_NOT_READABLE |
			L_SPUN_DWN_D | L_OPEN_FAIL |
			L_SCSI_ERR | L_NO_LOOP))) {
		if (l_ex_open_test(dl->multipath, dev_name)) {
			goto error;
		}
	} else if (!((port_a || port_b) &
			(L_NOT_READY | L_NOT_READABLE |
			L_SPUN_DWN_D | L_OPEN_FAIL |
			L_SCSI_ERR | L_NO_LOOP))) {
		if (l_ex_open_test(dl->multipath, dev_name)) {
				goto error;
		}
	}

#ifdef	DEVCTL_LIB
	dl1 = dl->multipath;
	while (dl1) {
		(void) strcpy(dev_path1, dl1->dev_path);
		if (devctl_acquire(dev_path1, DC_EXCL, &devhdl) == 0) {
			if (devctl_device_getstate(devhdl,
						&dev_state) == 0) {
				if (dev_state & DEVICE_BUSY) {
					(void) sprintf(l_error_msg,
						MSGSTR(-1,
						" Could not"
						" power off %s."
						" May be Busy.\n\n"),
						dev_name);
					l_error_msg_ptr = l_error_msg;
					(void) devctl_release(devhdl);
					goto error;
				}
			}
			(void) devctl_release(devhdl);
		}
		dl1 = dl1->next;
	}
#endif	/* DEVCTL_LIB */

pwr_up_dwn:

	return (l_pwr_up_down(ses_path, path_struct->f_flag, path_struct->slot,
		power_off_flag, verbose));

error:
	l_free_multipath(dl->multipath);
	free(dl);
	return (-1);
}


/*
 * Set the state of the Photon enclosure
 * the powered up/down mode.
 *
 * The path must point to an IB.
 */
int
l_pho_pwr_up_down(char *dev_name, char *path_phys, int power_off_flag,
		int verbose)
{
L_state		l_state;
int		i, err, port_a, port_b, port_a_b;
struct dlist	*dl, *dl1;
char		dev_path[MAXPATHLEN], dev_path1[MAXPATHLEN];
#ifdef	DEVCTL_LIB
devctl_hdl_t	devhdl;
uint_t		dev_state;
#endif	/* DEVCTL_LIB */

	dl = (struct dlist *)NULL;

	/* Clear the old error messages */
	l_error_msg_ptr = (char *)NULL;

	if (power_off_flag) {
		goto pre_pwr_dwn;
	} else {
		goto pwr_up_dwn;
	}

pre_pwr_dwn:
	if (err = l_get_status(path_phys, &l_state, verbose)) {
		return (err);
	}
	/*
	 * Check if any disk in this enclosure
	 * is reserved by another host before
	 * the power off.
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
				(void) sprintf(l_error_msg,
					MSGSTR(-1,
					" One or more disks in"
					" %s are reserved.\n\n"),
					dev_name);
				l_error_msg_ptr = l_error_msg;
				return (-1);
		}
	}

	/*
	 * Check if any disk in
	 * this enclosure is busy.
	 */
	for (i = 0; i < l_state.total_num_drv/2; i++) {
		if (*l_state.drv_front[i].physical_path) {
			memset(dev_path, 0, MAXPATHLEN);
			(void) strcpy(dev_path,
				(char *)&l_state.drv_front[i].physical_path);
			if ((dl = (struct dlist *)
				zalloc(sizeof (struct dlist))) == NULL) {
				(void) sprintf(l_error_msg,
					MSGSTR(-1,
					" Out of memory.\n"));
				l_error_msg_ptr = l_error_msg;
				return (-1);
			}
			dl->dev_path = dev_path;
			if ((dl->multipath =
				l_get_multipath(dev_path, verbose)) == NULL) {
				(void) sprintf(l_error_msg,
					MSGSTR(-1,
					" Cannot find valid path to"
					" %s.\n"), dev_path);
				l_error_msg_ptr = l_error_msg;
				free(dl);
				return (-1);
			}
			port_a =
				l_state.drv_front[i].d_state_flags[PORT_A];
			port_b =
				l_state.drv_front[i].d_state_flags[PORT_B];
			port_a_b = port_a & port_b;
			if (!(port_a_b & L_NO_LOOP)) {
				if (port_a & L_NO_LOOP) {
					port_a = 0;
				}
				if (port_b & L_NO_LOOP) {
					port_b = 0;
				}
			}
			if (!(port_a_b & (L_NOT_READY | L_NOT_READABLE |
						L_SPUN_DWN_D | L_OPEN_FAIL |
						L_SCSI_ERR | L_NO_LOOP))) {
				if (l_ex_open_test(dl->multipath, dev_name)) {
					goto error;
				}
			} else if (!((port_a || port_b) &
						(L_NOT_READY | L_NOT_READABLE |
						L_SPUN_DWN_D | L_OPEN_FAIL |
						L_SCSI_ERR | L_NO_LOOP))) {
				if (l_ex_open_test(dl->multipath, dev_name)) {
					goto error;
				}
			}

#ifdef	DEVCTL_LIB
			dl1 = dl->multipath;
			while (dl1) {
				(void) strcpy(dev_path1, dl1->dev_path);
				if (devctl_acquire(dev_path1, DC_EXCL,
							&devhdl) == 0) {
					if (devctl_device_getstate(
						devhdl, &dev_state) == 0) {
						if (dev_state & DEVICE_BUSY) {
							(void) sprintf(
								l_error_msg,
								MSGSTR(-1,
								" Could not"
								" power off %s."
								" May be busy."
								"\n\n"),
								dev_name);
							l_error_msg_ptr =
								l_error_msg;
							(void) devctl_release(
									devhdl);
							goto error;
						}
					}
					(void) devctl_release(devhdl);
				}
				dl1 = dl1->next;
			}

#endif	/* DEVCTL_LIB */
		}
		if (*l_state.drv_rear[i].physical_path) {
			memset(dev_path, 0, MAXPATHLEN);
			(void) strcpy(dev_path,
				(char *)&l_state.drv_rear[i].physical_path);
			if ((dl = (struct dlist *)
				zalloc(sizeof (struct dlist))) == NULL) {
				(void) sprintf(l_error_msg,
					MSGSTR(-1,
					" Out of memory.\n"));
				l_error_msg_ptr = l_error_msg;
				return (-1);
			}
			dl->dev_path = dev_path;
			if ((dl->multipath =
				l_get_multipath(dev_path, verbose)) == NULL) {
				(void) sprintf(l_error_msg,
					MSGSTR(-1,
					" Cannot find valid path to"
					" %s.\n"), dev_name);
				l_error_msg_ptr = l_error_msg;
				free(dl);
				return (-1);
			}
			port_a =
				l_state.drv_rear[i].d_state_flags[PORT_A];
			port_b =
				l_state.drv_rear[i].d_state_flags[PORT_B];
			port_a_b = port_a & port_b;
			if (!(port_a_b & L_NO_LOOP)) {
				if (port_a & L_NO_LOOP) {
					port_a = 0;
				}
				if (port_b & L_NO_LOOP) {
					port_b = 0;
				}
			}
			if (!(port_a_b & (L_NOT_READY | L_NOT_READABLE |
						L_SPUN_DWN_D | L_OPEN_FAIL |
						L_SCSI_ERR | L_NO_LOOP))) {
				if (l_ex_open_test(dl->multipath, dev_name)) {
					goto error;
				}
			} else if (!((port_a || port_b) &
						(L_NOT_READY | L_NOT_READABLE |
						L_SPUN_DWN_D | L_OPEN_FAIL |
						L_SCSI_ERR | L_NO_LOOP))) {
				if (l_ex_open_test(dl->multipath, dev_name)) {
					goto error;
				}
			}
#ifdef  DEVCTL_LIB
			dl1 = dl->multipath;
			while (dl1) {
				(void) strcpy(dev_path1, dl1->dev_path);
				if (devctl_acquire(dev_path1, DC_EXCL,
								&devhdl) == 0) {
					if (devctl_device_getstate(
						devhdl, &dev_state) == 0) {
						if (dev_state & DEVICE_BUSY) {
							(void) sprintf(
								l_error_msg,
								MSGSTR(-1,
								" Could not"
								" power off %s."
								" May be busy."
								"\n\n"),
								dev_name);
							l_error_msg_ptr =
								l_error_msg;
							(void) devctl_release(
									devhdl);
							goto error;
						}
					}
					(void) devctl_release(devhdl);
				}
				dl1 = dl1->next;
			}

#endif  /* DEVCTL_LIB */
		}
	}

pwr_up_dwn:

	return (l_pwr_up_down(path_phys, 0, -1, power_off_flag, verbose));

error:
	l_free_multipath(dl->multipath);
	free(dl);
	return (-1);
}


/*
 * Set the state of the Photon enclosure or disk
 * powered up/down mode.
 *
 * The path must point to an IB.
 * slot == -1 implies entire enclosure.
 */
int
l_pwr_up_down(char *path_phys, int front, int slot, int power_off_flag,
		int verbose)
{
L_inquiry	inq;
int		fd, status;
int		err;
u_char		*page_buf;
int 		front_index, rear_index, front_offset, rear_offset;
unsigned short	page_len;
struct	device_element *front_elem, *rear_elem;
L_state		l_state;

	memset(&inq, 0, sizeof (inq));
	if ((fd = object_open(path_phys, O_NDELAY | O_RDONLY)) == -1) {
		return (errno);
	}
	/* Verify it is a Photon */
	if (status = scsi_inquiry_cmd(fd,
		(u_char *)&inq, sizeof (struct l_inquiry_struct))) {
		(void) close(fd);
		return (status);
	}
	if ((strstr((char *)inq.inq_pid, ENCLOSURE_PROD_ID) == 0) &&
		(!(strncmp((char *)inq.inq_vid, "SUN     ",
		sizeof (inq.inq_vid)) &&
		(inq.inq_dtype == DTYPE_ESI)))) {
		(void) close(fd);
		(void) sprintf(l_error_msg,
			MSGSTR(-1,
			"Invalid path, device is not an %s\n"),
			ENCLOSURE_PROD_NAME);
		l_error_msg_ptr = (char *)&l_error_msg;
		return (L_INVALID_PATH);
	}

	/*
	 * To power up/down a Photon we use the Driver Off
	 * bit in the global device control element.
	 */
	if ((page_buf = (u_char *)malloc(MAX_REC_DIAG_LENGTH)) == NULL) {
		return (-1);
	}
	if (err = l_get_envsen_page(fd, page_buf, MAX_REC_DIAG_LENGTH,
				L_PAGE_2, verbose)) {
		(void) close(fd);
		(void) free(page_buf);
		return (err);
	}

	page_len = (page_buf[2] << 8 | page_buf[3]) + HEADER_LEN;

	/* Get index to the disk we are interested in */
	if (err = l_get_status(path_phys, &l_state, verbose)) {
		(void) close(fd);
		(void) free(page_buf);
		return (err);
	}
	/* Double check slot as convert_name only does gross check */
	if (slot >= l_state.total_num_drv/2) {
		(void) sprintf(l_error_msg,
			MSGSTR(-1,
			"Invalid path, invalid slot %d\n"),
			slot);
		l_error_msg_ptr = (char *)&l_error_msg;
		return (L_INVALID_PATH);
	}

	if (err = l_get_disk_element_index(&l_state, &front_index,
		&rear_index)) {
		return (err);
	}
	/* Skip global element */
	front_index++;
	rear_index++;

	front_offset = (8 + (front_index + slot)*4);
	rear_offset = (8 + (rear_index + slot)*4);

	front_elem = (struct device_element *)((int)page_buf + front_offset);
	rear_elem = (struct device_element *)((int)page_buf + rear_offset);

	if (front || slot == -1) {
		/*
		 * now do requested action.
		 */
		bzero(front_elem, sizeof (struct device_element));
		/* Set/reset power off bit */
		front_elem->dev_off = power_off_flag;
		front_elem->select = 1;
	}
	if (!front || slot == -1) {
		/* Now do rear */
		bzero(rear_elem, sizeof (struct device_element));
		/* Set/reset power off bit */
		rear_elem->dev_off = power_off_flag;
		rear_elem->select = 1;
	}

	if (getenv("_LUX_D_DEBUG") != NULL) {
		if (front || slot == -1) {
			p_dump("  l_pwr_up_down: "
				"Front Device Status Element ",
				(u_char *)front_elem,
				sizeof (struct device_element),
				HEX_ONLY);
		}
		if (!front || slot == -1) {
			p_dump("  l_pwr_up_down: "
				"Rear Device Status Element ",
				(u_char *)rear_elem,
				sizeof (struct device_element),
				HEX_ONLY);
		}
	}
	if (err = scsi_send_diag_cmd(fd,
		(u_char *)page_buf, page_len)) {
		(void) close(fd);
		(void) free(page_buf);
		return (err);
	}
	(void) close(fd);
	(void) free(page_buf);
	return (0);
}

/*
 * Set the password of the FPM by sending the password
 * in page 4 of the Send Diagnostic command.
 *
 * The path must point to an IB.
 *
 * The size of the password string must be <= 8 bytes.
 * The string can also be NULL. This is the way the user
 * chooses to not have a password.
 *
 * I then tell the photon by giving him 4 NULL bytes.
 */
int
l_new_password(char *path_phys, char *password)
{
Page4_name	page4;
L_inquiry	inq;
int		fd, status;

	memset(&inq, 0, sizeof (inq));
	memset(&page4, 0, sizeof (page4));

	if ((fd = object_open(path_phys, O_NDELAY | O_RDONLY)) == -1) {
		return (errno);
	}
	/* Verify it is a Photon */
	if (status = scsi_inquiry_cmd(fd,
		(u_char *)&inq, sizeof (struct l_inquiry_struct))) {
		(void) close(fd);
		return (status);
	}
	if ((strstr((char *)inq.inq_pid, ENCLOSURE_PROD_ID) == 0) &&
		(!(strncmp((char *)inq.inq_vid, "SUN     ",
		sizeof (inq.inq_vid)) &&
		(inq.inq_dtype == DTYPE_ESI)))) {
		(void) close(fd);
		(void) sprintf(l_error_msg,
			MSGSTR(-1,
			"Invalid path, device is not an %s\n"),
			ENCLOSURE_PROD_ID);
		l_error_msg_ptr = (char *)&l_error_msg;
		return (L_INVALID_PATH);
	}

	page4.page_code = L_PAGE_4;
	page4.page_len = (u_short)max((strlen(password) + 4), 8);
	/* Double check */
	if (strlen(password) > 8) {
		(void) sprintf(l_error_msg,
			MSGSTR(-1,
			"Invalid length password, %d bytes.\n"),
			strlen(password));
		l_error_msg_ptr = (char *)&l_error_msg;
		return (-1);
	}
	page4.string_code = L_PASSWORD;
	page4.enable = 1;
	(void) strcpy((char *)page4.name, password);

	if (status = scsi_send_diag_cmd(fd, (u_char *)&page4,
		page4.page_len + HEADER_LEN)) {
		(void) close(fd);
		return (status);
	}

	(void) close(fd);
	return (0);
}

/*
 * Set the name of the enclosure by sending the name
 * in page 4 of the Send Diagnostic command.
 *
 * The path must point to an IB.
 */
int
l_new_name(char *path_phys, char *name)
{
Page4_name	page4;
L_inquiry	inq;
int		fd, status;

	memset(&inq, 0, sizeof (inq));
	memset(&page4, 0, sizeof (page4));

	if ((fd = object_open(path_phys, O_NDELAY | O_RDONLY)) == -1) {
		return (errno);
	}
	/* Verify it is a Photon */
	if (status = scsi_inquiry_cmd(fd,
		(u_char *)&inq, sizeof (struct l_inquiry_struct))) {
		(void) close(fd);
		return (status);
	}
	if ((strstr((char *)inq.inq_pid, ENCLOSURE_PROD_ID) == 0) &&
		(!(strncmp((char *)inq.inq_vid, "SUN     ",
		sizeof (inq.inq_vid)) &&
		(inq.inq_dtype == DTYPE_ESI)))) {
		(void) close(fd);
		(void) sprintf(l_error_msg,
			MSGSTR(-1,
			"Invalid path, device is not an %s\n"),
			ENCLOSURE_PROD_ID);
		l_error_msg_ptr = (char *)&l_error_msg;
		return (L_INVALID_PATH);
	}

	page4.page_code = L_PAGE_4;
	page4.page_len = (u_short)((sizeof (struct page4_name) - 4));
	page4.string_code = L_ENCL_NAME;
	page4.enable = 1;
	strncpy((char *)page4.name, name, sizeof (page4.name));

	if (status = scsi_send_diag_cmd(fd, (u_char *)&page4,
		sizeof (page4))) {
		(void) close(fd);
		return (status);
	}

	/*
	 * Check the name really changed.
	 */
	if (status = scsi_inquiry_cmd(fd,
		(u_char *)&inq, sizeof (struct l_inquiry_struct))) {
		(void) close(fd);
		return (status);
	}
	if (strncmp((char *)inq.inq_box_name, name, sizeof (page4.name)) != 0) {
		char	name_buf[MAXNAMELEN];
		(void) close(fd);
		strncpy((char *)name_buf, (char *)inq.inq_box_name,
			sizeof (inq.inq_box_name));
		(void) sprintf(l_error_msg,
			MSGSTR(-1,
			"The name change failed.\n"
			"Name sent:  %s\nActual name:%s\n"),
			name, name_buf);
		l_error_msg_ptr = (char *)&l_error_msg;
		return (L_WARNING);
	}

	(void) close(fd);
	return (0);
}


/*
 * Get the path to the nexus driver
 *
 * This assumes the path looks something like this:
 * /devices/sbus@1f,0/SUNW,socal@1,0/SUNW,sf@0,0/ses@e,0:0
 * or maybe this
 * /devices/sbus@1f,0/SUNW,socal@1,0/SUNW,sf@1,0
 * or
 * /devices/sbus@1f,0/SUNW,socal@1,0
 * or
 * /devices/sbus@1f,0/SUNW,socal@1,0:1
 * or
 * /devices/sbus@1f,0/SUNW,socal@1,0/SUNW,sf@0,0:devctl
 * or
 * /devices/pci@4,2000/scsi@1/ses@w50800200000000d2,0:0
 *    which should resolve to
 *    /devices/pci@4,2000/scsi@1:devctl
 */
static	int
l_get_nexus_path(char *path_phys, char **nexus_path)
{
u_char		port = 0;
int		port_flag = 0;
char		*char_ptr;
char		drvr_path[MAXPATHLEN];
char		buf[MAXPATHLEN];
char		temp_buf[MAXPATHLEN];
struct	stat	stbuf;

	*nexus_path = NULL;
	(void) strcpy(drvr_path, path_phys);
	if (strstr(drvr_path, DRV_NAME_SSD) || strstr(drvr_path, SES_NAME)) {
		if ((char_ptr = strrchr(drvr_path, '/')) == NULL) {
			(void) sprintf(l_error_msg,
				MSGSTR(-1,
				"in the device physical path\n"));
			l_error_msg_ptr = (char *)&l_error_msg;
			return (L_INVALID_PATH);
		}
		*char_ptr = '\0';   /* Terminate string  */
	}
	if (strstr(drvr_path, FC_DRIVER)) {
		/* sf driver in path so capture the port # */
		if ((char_ptr = strstr(drvr_path, FC_DRIVER)) == NULL) {
			(void) sprintf(l_error_msg,
				MSGSTR(-1,
				"in the device physical path\n"));
			l_error_msg_ptr = (char *)&l_error_msg;
			return (L_INVALID_PATH);
		}
		port = atoi(char_ptr + 3);
		if (port > 1) {
			(void) sprintf(l_error_msg,
				MSGSTR(-1, "in the device physical path"
			": Invalid port number %d (Should be 0 or 1)\n"),
				port);
			l_error_msg_ptr = (char *)&l_error_msg;
			return (L_INVALID_PATH);
		}
		if ((char_ptr = strrchr(drvr_path, '/')) == NULL) {
			(void) sprintf(l_error_msg,
				MSGSTR(-1,
				"in the device physical path\n"));
			l_error_msg_ptr = (char *)&l_error_msg;
			return (L_INVALID_PATH);
		}
		*char_ptr = '\0';   /* Terminate sting  */
		port_flag++;
		L_DPRINTF("  l_get_nexus_path:"
			" sf driver in path so use port #%d.\n",
			port);
	}

	if (stat(drvr_path, &stbuf) != 0) {
		(void) sprintf(l_error_msg,
			MSGSTR(-1,
			"Cannot obtain the status of path %s\n"),
			drvr_path);
		l_error_msg_ptr = (char *)&l_error_msg; /* set error ptr */
		return (L_INVALID_PATH);
	}
	if ((stbuf.st_mode & S_IFMT) == S_IFDIR) {
		/*
		 * Found a directory
		 *
		 * So now we must append a port number or devctl
		 */
		if (port_flag) {
			/* append port */
			(void) sprintf(buf, ":%d", port);
		} else {
			/* Try adding port 0 and see if node exists. */
			(void) sprintf(temp_buf, "%s:0", drvr_path);
			if (stat(temp_buf, &stbuf) != 0) {
				/*
				 * Path we guessed at does not
				 * exist so it must be a
				 * driver that ends in :devctl.
				 */
				(void) sprintf(buf, ":devctl");
			} else {
				/*
				 * The path that was entered
				 * did not include a port number
				 * so the port was set to zero,
				 * and then checked.
				 * The default path did exist.
				 */
				(void) fprintf(stdout, MSGSTR(-1,
				"Since a complete path was not supplied "
				"a default path is being used:\n  %s\n"),
				temp_buf);
				(void) sprintf(buf, ":0");
			}
		}

		(void) strcat(drvr_path, buf);
	}
	*nexus_path = alloc_string(drvr_path);
	L_DPRINTF("  l_get_nexus_path: Nexus path = %s\n", drvr_path);
	return (0);
}

/*
 * Read the extended link error status block from the
 * specified device and Host Adapter.
 */
/*ARGSUSED*/
int
l_rdls(char *path_phys, struct al_rls **rls_ptr, int verbose)
{
char		nexus_path[MAXPATHLEN], *nexus_path_ptr;
int		fd;
int		err;
struct lilpmap	map;
AL_rls		*rls, *c1 = NULL, *c2 = NULL;
u_char		i;
sf_al_map_t	exp_map;
int		exp_map_flag = 0;
int		length;

	*rls_ptr = rls = NULL;
	(void) memset(&map, 0, sizeof (struct lilpmap));
	if ((err = l_get_nexus_path(path_phys, &nexus_path_ptr)) != 0) {
		return (err);
	}
	(void) strcpy(nexus_path, nexus_path_ptr);

	/* open driver */
	if ((fd = object_open(nexus_path, O_NDELAY | O_RDONLY)) == -1)
		return (errno);

	/* Get map of devices on this loop. */
	/*
	 * First try using the socal version of the map.
	 * If that fails get the expanded vesion.
	 */
	if (ioctl(fd, FCIO_GETMAP, &map) != 0) {
		I_DPRINTF("  FCIO_GETMAP ioctl failed.\n");
		if (ioctl(fd, SFIOCGMAP, &exp_map) != 0) {
			I_DPRINTF("  SFIOCGMAP ioctl failed.\n");
			(void) close(fd);
			return (errno);
		}
		/*
		 * Check for reasonableness.
		 */
		if ((exp_map.sf_count > 126) || (exp_map.sf_count < 0)) {
			(void) sprintf(l_error_msg,
				MSGSTR(-1, "Invalid loop map found.\n"));
			l_error_msg_ptr = (char *)&l_error_msg;
			(void) close(fd);
			return (L_LOOP_ERROR);
		}
		for (i = 0; i < exp_map.sf_count; i++) {
			if ((exp_map.sf_addr_pair[i].sf_al_pa > 0xef) ||
				(exp_map.sf_addr_pair[i].sf_al_pa < 0)) {
				(void) sprintf(l_error_msg,
				MSGSTR(-1, "Invalid loop map found.\n"));
				l_error_msg_ptr = (char *)&l_error_msg;
				(void) close(fd);
				return (L_LOOP_ERROR);
			}
		}
		length = exp_map.sf_count;
		exp_map_flag++;
	} else {
		I_DPRINTF("  l_rdls:"
			" FCIO_GETMAP ioctl returned %d entries.\n",
			map.lilp_length);
		/* Check for reasonableness. */
		if (map.lilp_length > sizeof (map.lilp_list)) {
			(void) sprintf(l_error_msg,
				MSGSTR(-1, "FCIO_GETMAP ioctl returned"
				" an invalid parameter:"
				" # entries to large.\n"));
			l_error_msg_ptr = (char *)&l_error_msg;
			(void) close(fd);
			return (L_TRANSFER_LEN);
		}
		length = map.lilp_length;
	}
	for (i = 0; i < length; i++) {
		if ((c2 = (struct al_rls *)
			zalloc(sizeof (struct al_rls))) == NULL) {
			(void) close(fd);
			return (errno);
		}
		if (rls == NULL) {
			c1 = rls = c2;
		} else {
			for (c1 = rls; c1->next; c1 =  c1->next);
			c1 = c1->next = c2;
		}
		(void) strcpy(c1->driver_path, nexus_path);
		if (exp_map_flag) {
			c1->payload.rls_portno = c1->al_ha =
				exp_map.sf_addr_pair[i].sf_al_pa;
		} else {
			c1->payload.rls_portno = c1->al_ha = map.lilp_list[i];
		}
		c1->payload.rls_linkfail =
			(u_int)0xff000000; /* get LESB for this port */
		I_DPRINTF("  l_rdls:"
			" al_pa 0x%x\n", c1->payload.rls_portno);
		if (ioctl(fd, FCIO_LINKSTATUS, &c1->payload) != 0) {
			I_DPRINTF("  FCIO_LINKSTATUS ioctl failed.\n");
			(void) close(fd);
			return (errno);
		}
		I_DPRINTF("  l_rdls: al_pa returned by ioctl 0x%x\n",
			c1->payload.rls_portno);
	}
	*rls_ptr = rls;	/* Pass back pointer */
	(void) close(fd);
	return (0);
}

/*
 * Issue a Loop Port enable Primitive sequence
 * to the device specified by the pathname.
 */
int
l_enable(char *path, int verbose)
/*ARGSUSED*/
{

	return (0);
}

/*
 * Issue a Loop Port Bypass Primitive sequence
 * to the device specified by the pathname. This requests the
 * device to set its L_Port into the bypass mode.
 */
int
l_bypass(char *path, int verbose)
/*ARGSUSED*/
{

	return (0);
}



/*
 * Force lip on loop specified by path.
 */
int
l_force_lip(char *path_phys, int verbose)
{
int		fd, err;
char		nexus_path[MAXPATHLEN], *nexus_path_ptr;


	if ((err = l_get_nexus_path(path_phys, &nexus_path_ptr)) != 0) {
		return (err);
	}
	(void) strcpy(nexus_path, nexus_path_ptr);

	P_DPRINTF("  l_force_lip: Force lip on: Path %s\n", nexus_path);

	/* open driver */
	if ((fd = object_open(nexus_path, O_NDELAY | O_RDONLY)) == -1)
		return (errno);

	if (verbose) {
		(void) fprintf(stdout,
		MSGSTR(-1, "  Forcing lip (Loop Initialization Protocol)"
			"\n  on loop at: %s\n"), nexus_path);
	}
	if (ioctl(fd, FCIO_FORCE_LIP) != 0) {
		I_DPRINTF("  FCIO_FORCE_LIP ioctl failed.\n");
		(void) close(fd);
		return (errno);
	}
	(void) close(fd);
	return (0);
}

/*
 * Create a linked list of all the WWN's for all FC_AL disks that
 * are attached to this host.
 *
 * RETURN VALUES: 0 O.K.
 *
 * wwn_list pointer:
 *			NULL: No enclosures found.
 *			!NULL: Enclosures found
 *                      wwn_list points to a linked list of wwn's.
 */
int
l_get_wwn_list(struct wwn_list_struct **wwn_list_ptr, int verbose)
{
char		*dev_name;
DIR		*dirp;
struct dirent	*entp;
char		namebuf[MAXPATHLEN];
char		disk_path[MAXPATHLEN];
char		last_disk_path[MAXPATHLEN];
struct stat	sb;
char		*result = NULL;
WWN_list	*wwn_list, *l1, *l2;
char		*char_ptr;
u_char		node_wwn[WWN_SIZE], port_wwn[WWN_SIZE];
int		al_pa;

	if (verbose) {
		(void) fprintf(stdout,
		MSGSTR(-1,
			"  Building list of WWN's of all FC_AL disks attached"
			" to the system.\n"));
	}
	P_DPRINTF("  l_get_wwn_list: Building list of WWN's of all "
		"FC_AL disks attached to the system\n");

	wwn_list = *wwn_list_ptr = NULL;
	if ((dev_name = (char *)zalloc(sizeof ("/dev/rdsk"))) == NULL) {
		return (errno);
	}
	(void) 	sprintf((char *)dev_name, "/dev/rdsk");

	if (verbose) {
		(void) fprintf(stdout,
		MSGSTR(-1,
			"  Searching directory %s for links to devices\n"),
			dev_name);
	}
	if ((dirp = opendir(dev_name)) == NULL) {
		free(dev_name);
		P_DPRINTF("  l_get_wwn_list: No disks found\n");
		return (0);
	}


	while ((entp = readdir(dirp)) != NULL) {
		if (strcmp(entp->d_name, ".") == 0 ||
			strcmp(entp->d_name, "..") == 0)
			continue;

		(void) sprintf(namebuf, "%s/%s", dev_name, entp->d_name);

		if ((lstat(namebuf, &sb)) < 0) {
			(void) fprintf(stderr,
			    MSGSTR(-1, "Warning: Cannot stat %s\n"), namebuf);
			continue;
		}

		if (!S_ISLNK(sb.st_mode)) {
			(void) fprintf(stderr,
			MSGSTR(-1, "Warning: %s is not a symbolic link\n"),
				namebuf);
			continue;
		}
		if ((result = get_physical_name_from_link(namebuf)) == NULL) {
			(void) fprintf(stderr,
			MSGSTR(-1, "  Warning: Get physical name from"
			" link failed. Link=%s\n"), namebuf);
			continue;
		}

		/* Found a disk. */
		(void) strcpy(disk_path, result);
		if ((char_ptr = strrchr(disk_path, ':')) == NULL) {
			continue;	/* Skip error */
		}
		*char_ptr = '\0';	/* Strip partition information */

		if (strcmp(disk_path, last_disk_path) == 0) {
			(void) free(result);
			continue;	/* skip other partitions */
		}
		(void) strcpy(last_disk_path, disk_path);
		if (strstr(disk_path, DRV_NAME_SSD) == 0) {
			(void) free(result);
			continue;	/* ignore non disk devices */
		}

		if (l_get_wwn(result, port_wwn, node_wwn,
			&al_pa, verbose)) {
			(void) free(result);
			l_error_msg_ptr = p_error_msg_ptr = NULL;
			continue;	/* Skip error */
		}

		/*
		 * Add information to the list.
		 */
		if ((l2 = (struct  wwn_list_struct *)
			zalloc(sizeof (struct  wwn_list_struct)))
			== NULL) {
			return (errno);
		}
		if (wwn_list == NULL) {
			l1 = wwn_list = l2;
		} else {
			for (l1 = wwn_list; l1->wwn_next;
				l1 = l1->wwn_next);
			l1 = l1->wwn_next = l2;
		}

		/* Fill in structure */
		(void) sprintf(l1->port_wwn_s,
		"%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x",
			port_wwn[0], port_wwn[1], port_wwn[2], port_wwn[3],
			port_wwn[4], port_wwn[5], port_wwn[6], port_wwn[7]);
		(void) sprintf(l1->node_wwn_s,
		"%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x",
			node_wwn[0], node_wwn[1], node_wwn[2], node_wwn[3],
			node_wwn[4], node_wwn[5], node_wwn[6], node_wwn[7]);

		(void) strcpy((char *)l1->physical_path, (char *)result);
		(void) strcpy((char *)l1->logical_path, (char *)namebuf);
		*wwn_list_ptr = wwn_list; /* pass back ptr to list */
		P_DPRINTF("  l_get_wwn_list: Found disk: port=%s "
		"Logical path=%s\n", l1->port_wwn_s, l1->logical_path);

		(void) free(result);
	}
exit:
	free(dev_name);
	closedir(dirp);
	return (0);
}

void
l_free_wwn_list(struct wwn_list_struct **wwn_list)
{
WWN_list	*next = NULL;
	for (; *wwn_list; *wwn_list = next) {
		next = (*wwn_list)->wwn_next;
		free(*wwn_list);
	}
}

/*
 * Create a linked list of all the Photon enclosures that
 * are attached to this host.
 *
 * RETURN VALUES: 0 O.K.
 *
 * box_list pointer:
 *			NULL: No enclosures found.
 *			!NULL: Enclosures found
 *                      box_list points to a linked list of boxes.
 */
int
l_get_box_list(struct box_list_struct **box_list_ptr, int verbose)
{
char		*dev_name;
DIR		*dirp;
struct dirent	*entp;
char		namebuf[MAXPATHLEN];
struct stat	sb;
char		*result = NULL;
int		fd, status;
L_inquiry	inq;
Box_list	*box_list, *l1, *l2;
IB_page_config	page1;
u_char		node_wwn[WWN_SIZE], port_wwn[WWN_SIZE];
int		al_pa;

	box_list = *box_list_ptr = NULL;
	if ((dev_name = (char *)zalloc(sizeof ("/dev/es"))) == NULL) {
		return (errno);
	}
	(void) sprintf((char *)dev_name, "/dev/es");

	if (verbose) {
		(void) fprintf(stdout,
		MSGSTR(-1,
			"  Searching directory %s for links to enclosures\n"),
			dev_name);
	}

	if ((dirp = opendir(dev_name)) == NULL) {
		free(dev_name);
		/* No Photons found */
		P_DPRINTF("  l_get_box_list: No Photons found\n");
		return (0);
	}


	while ((entp = readdir(dirp)) != NULL) {
		if (strcmp(entp->d_name, ".") == 0 ||
			strcmp(entp->d_name, "..") == 0)
			continue;

		(void) sprintf(namebuf, "%s/%s", dev_name, entp->d_name);

		if ((lstat(namebuf, &sb)) < 0) {
			(void) fprintf(stderr,
			    MSGSTR(-1, "Warning: Cannot stat %s\n"), namebuf);
			continue;
		}

		if (!S_ISLNK(sb.st_mode)) {
			(void) fprintf(stderr,
			MSGSTR(-1, "Warning: %s is not a symbolic link\n"),
				namebuf);
			continue;
		}
		if ((result = get_physical_name_from_link(namebuf)) == NULL) {
			(void) fprintf(stderr,
			MSGSTR(-1, "  Warning: Get physical name from"
			" link failed. Link=%s\n"), namebuf);
			continue;
		}

		/* Found a SES card. */
		P_DPRINTF("  l_get_box_list: Link to SES Card found: %s/%s\n",
			dev_name, entp->d_name);
		if ((fd = object_open(result, O_NDELAY | O_RDONLY)) == -1) {
			l_error_msg_ptr = p_error_msg_ptr = NULL;
			continue;	/* Ignore errors */
		}
		/* Get the box name */
		if (status = scsi_inquiry_cmd(fd,
			(u_char *)&inq, sizeof (struct l_inquiry_struct))) {
			(void) close(fd);
			l_error_msg_ptr = p_error_msg_ptr = NULL;
			continue;	/* Ignore errors */
		}


		if ((strstr((char *)inq.inq_pid, ENCLOSURE_PROD_ID) != 0) ||
			(strncmp((char *)inq.inq_vid, "SUN     ",
			sizeof (inq.inq_vid)) &&
			(inq.inq_dtype == DTYPE_ESI))) {
			/*
			 * Found Photon
			 */

			/* Get the port WWN from the IB, page 1 */
			if ((status = l_get_envsen_page(fd, (u_char *)&page1,
				sizeof (page1), 1, 0)) != NULL) {
				(void) close(fd);
				free(dev_name);
				closedir(dirp);
				return (status);
			}

			/*
			 * Build list of names.
			 */
			if ((l2 = (struct  box_list_struct *)
				zalloc(sizeof (struct  box_list_struct)))
				== NULL) {
				return (errno);
			}
			if (box_list == NULL) {
				l1 = box_list = l2;
			} else {
				for (l1 = box_list; l1->box_next;
					l1 = l1->box_next);
				l1 = l1->box_next = l2;
			}

			/* Fill in structure */
			(void) strcpy((char *)l1->b_physical_path,
				(char *)result);
			(void) strcpy((char *)l1->logical_path,
				(char *)namebuf);
			(void) sprintf(l1->b_node_wwn_s,
			"%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x",
				page1.enc_node_wwn[0],
				page1.enc_node_wwn[1],
				page1.enc_node_wwn[2],
				page1.enc_node_wwn[3],
				page1.enc_node_wwn[4],
				page1.enc_node_wwn[5],
				page1.enc_node_wwn[6],
				page1.enc_node_wwn[7]);
			strncpy((char *)l1->prod_id_s,
				(char *)inq.inq_pid,
				sizeof (inq.inq_pid));
			strncpy((char *)l1->b_name,
				(char *)inq.inq_box_name,
				sizeof (inq.inq_box_name));
			/* make sure null terminated */
			l1->b_name[sizeof (l1->b_name) - 1] = NULL;
			/*
			 * Now get the port WWN for the port
			 * we are connected to.
			 */
			if (status = l_get_wwn(result, port_wwn, node_wwn,
				&al_pa, verbose)) {
				(void) close(fd);
				free(dev_name);
				closedir(dirp);
				return (status);
			}
			(void) sprintf(l1->b_port_wwn_s,
			"%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x",
			port_wwn[0], port_wwn[1], port_wwn[2], port_wwn[3],
			port_wwn[4], port_wwn[5], port_wwn[6], port_wwn[7]);

			if (getenv("_LUX_P_DEBUG") != NULL) {
				(void) fprintf(stdout,
				"  l_get_box_list:"
				" Found enclosure named:%s\n", l1->b_name);
			}

			*box_list_ptr = box_list; /* pass back ptr to list */
		}
		(void) close(fd);
	}
exit:
	free(dev_name);
	closedir(dirp);
	return (0);
}

void
l_free_box_list(struct box_list_struct **box_list)
{
Box_list	*next = NULL;
	for (; *box_list; *box_list = next) {
		next = (*box_list)->box_next;
		free(*box_list);
	}
}

/*
 * Find out if there are any other boxes with the same name as "name".
 *
 * RETURNS: 0 - There are no other boxes with the same name.
 */
/*ARGSUSED*/
int
l_duplicate_names(Box_list *b_list, char wwn[], char *name, int verbose)
{
int	dup_flag = 0;

	while (b_list != NULL) {
		if ((strcmp(name, (const char *)b_list->b_name) == 0) &&
			(strcmp(b_list->b_node_wwn_s, wwn) != 0)) {
			dup_flag++;
			break;
		}
		b_list = b_list->box_next;
	}
	return (dup_flag);
}

#define	PLNDEF	"SUNW,pln"

/*
 * Check for a name conflict with an SSA cN type name.
 *
 * RETURN: 0 	No cN path found or error
 *         -1	Conflict found
 *         path	Path to an SSA
 */
static char *
l_get_conflict(char *name, int verbose)
{
char		s[MAXPATHLEN];
char		*p = NULL;
char		*pp = NULL;
Box_list	*box_list = NULL;
char		*result = NULL;
int		found_box = 0;

	(void) strcpy(s, name);
	if (!(((result = get_physical_name(s)) != NULL) &&
		(strstr((const char *)result, PLNDEF) != NULL))) {
		return (NULL);
	}
	P_DPRINTF("  l_get_conflict: Found "
		"SSA path using %s\n", s);
	/* Find path to IB */
	if (l_get_box_list(&box_list, verbose)) {
		return ((char *)-1);	/* Failure */
	}
	/*
	 * Valid cN type name found.
	 */
	while (box_list != NULL) {
		if ((strcmp((char *)s,
			(char *)box_list->b_name)) == 0) {
			found_box = 1;
			if (p == NULL) {
				if ((p = zalloc(strlen(
				box_list->b_physical_path)
				+ 2)) == NULL) {
				return ((char *)-1);
				}
			} else {
				if ((pp = zalloc(strlen(
				box_list->b_physical_path)
				+ strlen(p)
				+ 2)) == NULL) {
				return ((char *)-1);
				}
				(void) strcpy(pp, p);
				free(p);
				p = pp;
			}
			strcat(p, box_list->b_physical_path);
			strcat(p, "\n");
		}
		box_list = box_list->box_next;
	}
	if (found_box) {
		(void) sprintf(l_error_msg,
		MSGSTR(-1,
		"There is a conflict between the "
		"enclosure\nwith this name, %s, "
		"and a SSA name of the same form.\n"
		"Please use one of the following physical "
		"pathnames:\n%s\n%s\n"),
		s, result, p);
		l_error_msg_ptr = (char *)&l_error_msg;
		l_free_box_list(&box_list);
		free(p);
		return ((char *)-1);	/* failure */
	}
	l_free_box_list(&box_list);
	return (result);
}

/*
 * convert box name or WWN or logical path to physical path.
 *
 *	RETURNS:
 *		NULL = Error.
 */
char *
l_convert_name(char *name, struct path_struct **path_struct, int verbose)
{
char		s[MAXPATHLEN];
char		ses_path[MAXPATHLEN];
Box_list	*box_list = NULL;
Box_list	*orig_box_list = NULL;
WWN_list	*wwn_list;
char		*char_ptr, *ptr;
char		*result = NULL;
char		*box_result = NULL;
Path_struct	*path_ptr = NULL;
int		slot;
int		slot_flag = 0;
int		found_box = 0;
int		found_comma = 0;
L_state		l_state;

	if ((*path_struct = path_ptr = (struct path_struct *)
		zalloc(sizeof (struct path_struct))) == 0) {
		(void) sprintf(l_error_msg,
		MSGSTR(-1, "Memory allocation failed.\n"));
		l_error_msg_ptr = (char *)&l_error_msg;
		return (0);	/* Failure */
	}

	/*
	 * If the path contains a "/" then assume
	 * it is a logical or physical path as the
	 * box name or wwn can not contain "/"s.
	 */
	if (strchr(name, '/') != NULL) {
		result = get_physical_name(name);
		if (result == NULL) {
			(void) sprintf(l_error_msg,
			MSGSTR(-1, "Invalid path name (%s)\n"), name);
			l_error_msg_ptr = (char *)&l_error_msg;
			return (0);	/* Failure */
		}
		goto done;
	}

	(void) strcpy(s, name);
	if ((s[0] == 'c') &&
		((int)strlen(s) > 1) && ((int)strlen(s) < 5)) {
		result = l_get_conflict(s, verbose);
		if (result == (char *)-1) {
			return (0);	/* Failure */
		}
		if (result) {
			goto done;
		}
	}

	/*
	 * Check to see if we have a box or WWN name.
	 *
	 * If it contains a , then the format must be
	 *    box_name,f1 where f is front and 1 is the slot number
	 * or it is a format like
	 * ssd@w2200002037049adf,0:h,raw
	 * or
	 * SUNW,pln@a0000000,77791d:ctlr
	 */
	if (((char_ptr = strstr(s, ",")) != NULL) &&
		((*(char_ptr + 1) == 'f') || (*(char_ptr + 1) == 'r'))) {
		char_ptr++;	/* point to f/r */
		if (*char_ptr == 'f') {
			path_ptr->f_flag = 1;
		} else if (*char_ptr != 'r') {
			(void) sprintf(l_error_msg,
			MSGSTR(-1, "Invalid path format.\n"));
			l_error_msg_ptr = (char *)&l_error_msg;
			return (0);	/* Failure */
		}
		char_ptr++;
		slot = strtol(char_ptr, &ptr, 10);
		if ((slot < 0) || (ptr == char_ptr) ||
			(slot >= (MAX_DRIVES_PER_BOX/2))) {
			(void) sprintf(l_error_msg,
			MSGSTR(-1, "Invalid path format:"
			" invalid slot number.\n"));
			l_error_msg_ptr = (char *)&l_error_msg;
			return (0);	/* Failure */
		}
		/* Say slot valid. */
		slot_flag = path_ptr->slot_valid = 1;
		path_ptr->slot = slot;
	}

	if (((char_ptr = strstr(s, ",")) != NULL) &&
		((*(char_ptr + 1) == 'f') || (*(char_ptr + 1) == 'r'))) {
		*char_ptr = NULL; /* make just box name */
		found_comma = 1;
	}
	/* Find path to IB */
	if (l_get_box_list(&box_list, verbose)) {
		return (0);	/* Failure */
	}
	orig_box_list = box_list;
	/* Look for box name. */
	while (box_list != NULL) {
	    if ((strcmp((char *)s, (char *)box_list->b_name)) == 0) {
			result = box_result =
				alloc_string(box_list->b_physical_path);
			L_DPRINTF("  l_convert_name:"
			" Found subsystem: name %s  WWN %s\n",
			box_list->b_name, box_list->b_node_wwn_s);
			/*
			 * Check for another box with this name.
			 */
			if (l_duplicate_names(orig_box_list,
				box_list->b_node_wwn_s,
				(char *)box_list->b_name,
				verbose)) {
				(void) sprintf(l_error_msg,
				MSGSTR(-1,
				"There are two or more "
				"enclosures with the same name, %s.\n"
				"Please use a logical or physical "
				"pathname.\n\n"), s);
				l_error_msg_ptr = (char *)&l_error_msg;
				l_free_box_list(&box_list);
				return (0);	/* failure */
			}
			found_box = 1;
			break;
		}
		box_list = box_list->box_next;
	}
	/*
	 * Check to see if we must get individual disks path.
	 */
	if (found_box && slot_flag) {
		(void) strcpy(ses_path, result);
		if (l_get_status(ses_path, &l_state,
			verbose) != 0) {
			(void) sprintf(l_error_msg,
			MSGSTR(-1, "Get status failed."));
			l_error_msg_ptr = (char *)&l_error_msg;
			return (0);
		}
		if (path_ptr->f_flag) {
			if (*l_state.drv_front[slot].physical_path) {
				result =
		alloc_string(l_state.drv_front[slot].physical_path);
			} else {
				/* Result is the IB path */
				path_ptr->ib_path_flag = 1;
				path_ptr->p_physical_path = box_result;
				result = NULL;
			}
		} else {
			if (*l_state.drv_rear[slot].physical_path) {
				result =
		alloc_string(l_state.drv_rear[slot].physical_path);
			} else {
				/* Result is the IB path */
				path_ptr->p_physical_path = box_result;
				path_ptr->ib_path_flag = 1;
				result = NULL;
			}
		}
		/* Free box list */
		l_free_box_list(&box_list);
		assert(box_list == NULL);
		goto done;
	}
	if (found_box || found_comma) {
		/* Free box list */
		l_free_box_list(&box_list);
		assert(box_list == NULL);
		goto done;
	}
	/*
	 * No luck with the box name.
	 *
	 * Try WWN's
	 */
	/* Look for the SES's WWN */
	box_list = orig_box_list;
	while (box_list != NULL) {
	    if (((strcmp((char *)s,
		(char *)box_list->b_port_wwn_s)) == 0) ||
		((strcmp((char *)s,
		(char *)box_list->b_node_wwn_s)) == 0)) {
			result =
				alloc_string(box_list->b_physical_path);
			L_DPRINTF("  l_convert_name:"
			" Found subsystem using the WWN"
			": name %s  WWN %s\n",
			box_list->b_name, box_list->b_node_wwn_s);
			l_free_box_list(&box_list);
			assert(box_list == NULL);
			goto done;
		}
		box_list = box_list->box_next;
	}
	/* Look for a disks WWN */
	if (strlen(s) <= L_WWN_LENGTH) {
		if (g_WWN_list == (WWN_list *) NULL) {
			if (l_get_wwn_list(&g_WWN_list, verbose)) {
				return (0);	/* Failure */
			}
		}
		wwn_list = g_WWN_list;
		while (wwn_list != NULL) {
		    if (((strcmp((char *)s,
			(char *)wwn_list->node_wwn_s)) == 0) ||
			((strcmp((char *)s,
			(char *)wwn_list->port_wwn_s)) == 0)) {
			result = alloc_string(wwn_list->physical_path);
			L_DPRINTF("  l_convert_name:"
			"  Found disk: WWN %s Path %s\n",
			s, wwn_list->logical_path);
			goto done;
		    }
		    wwn_list = wwn_list->wwn_next;
		}
		}

	/*
	 * Try again in case we were in the /dev
	 * or /devices directory.
	 */
	result = get_physical_name(name);

done:
	path_ptr->argv = name;
	if (result == NULL) {
		(void) sprintf(l_error_msg,
			MSGSTR(-1, "Invalid path name (%s)\n"), name);
		l_error_msg_ptr = (char *)&l_error_msg; /* set error ptr */
		return (0);	/* Failure */
	}
	path_ptr->p_physical_path = result;
	L_DPRINTF("  l_convert_name: path_struct:\n\tphysical_path:\n\t %s\n"
		"\targv:\t\t%s"
		"\n\tslot_valid\t%d"
		"\n\tslot\t\t%d"
		"\n\tf_flag\t\t%d"
		"\n\tib_path_flag\t%d\n",
		path_ptr->p_physical_path,
		path_ptr->argv,
		path_ptr->slot_valid,
		path_ptr->slot,
		path_ptr->f_flag,
		path_ptr->ib_path_flag);

	return (result);
}

int
l_get_envsen_page(int fd, u_char *buf, int buf_size, u_char page_code,
	int verbose)
{
Rec_diag_hdr	hdr;
u_char	*pg;
int	size, new_size, status;

	if (verbose) {
		(void) fprintf(stdout,
		MSGSTR(-1, "  Reading SES page %x\n"), page_code);
	}

	(void) memset(&hdr, 0, sizeof (struct rec_diag_hdr));
	if (status = scsi_rec_diag_cmd(fd, (u_char *)&hdr,
		sizeof (struct rec_diag_hdr), page_code)) {
		return (status);
	}

	/* Check */
	if ((hdr.page_code != page_code) || (hdr.page_len == 0)) {
		(void) sprintf(l_error_msg,
		MSGSTR(-1, "reading page %x from IB\n"), page_code);
		l_error_msg_ptr = (char *)&l_error_msg;
		return (RD_PG);
	}
	size = HEADER_LEN + hdr.page_len;
	/*
	 * Because of a hardware restriction in the soc+ chip
	 * the transfers must be word aligned.
	 */
	while (size & 0x03) {
		size++;
		if (size > buf_size) {
			(void) sprintf(l_error_msg,
			MSGSTR(-1, "reading page %x from IB:"
			" Buffer size too small.\n"), page_code);
			l_error_msg_ptr = (char *)&l_error_msg;
			return (RD_PG);
		}
		P_DPRINTF("  l_get_envsen_page: Adjusting size of the "
			"scsi_rec_diag_cmd buffer.\n");
	}

	if ((pg = (u_char *)zalloc(size)) == NULL) {
		return (errno);
	}

	P_DPRINTF("  l_get_envsen_page: Reading page %x of size 0x%x\n",
		page_code, size);
	if (status = scsi_rec_diag_cmd(fd, pg, size, page_code)) {
		free((char *)pg);
		return (status);
	}

	new_size = MIN(size, buf_size);
	bcopy((const void *)pg, (void *)buf, (size_t)new_size);

	free(pg);
	return (0);
}


/*
 * Get consolidated copy of all environmental information
 * into buf structure.
 */

int
l_get_envsen(char *path_phys, u_char *buf, int size, int verbose)
{
int		fd;
int		rval;
u_char		page_code;
u_char		*local_buf_ptr = buf;
Rec_diag_hdr	*hdr = (struct rec_diag_hdr *)(int)buf;
u_char		*page_list_ptr;
u_short		num_pages;

	page_code = L_PAGE_PAGE_LIST;

	/* open IB */
	if ((fd = object_open(path_phys, O_NDELAY | O_RDONLY)) == -1)
		return (errno);

	P_DPRINTF("  l_get_envsen: Getting list of supported"
		" pages from IB\n");
	if (verbose) {
		(void) fprintf(stdout,
		MSGSTR(-1, "  Getting list of supported pages from IB\n"));
	}

	/* Get page 0 */
	if ((rval = l_get_envsen_page(fd, local_buf_ptr,
		size, page_code, verbose)) != NULL) {
		(void) close(fd);
		return (rval);
	}

	page_list_ptr = buf + HEADER_LEN + 1; /* +1 to skip page 0 */

	num_pages = hdr->page_len - 1;
	/* Align buffer */
	while (hdr->page_len & 0x03) {
		hdr->page_len++;
	}
	local_buf_ptr += HEADER_LEN + hdr->page_len;

	/*
	 * Getting all pages and appending to buf
	 */
	for (; num_pages--; page_list_ptr++) {
		/*
		 * The fifth byte of page 0 is the start
		 * of the list of pages not including page 0.
		 */
		page_code = *page_list_ptr;

		if ((rval = l_get_envsen_page(fd, local_buf_ptr,
			size, page_code, verbose)) != NULL) {
			(void) close(fd);
			return (rval);
		}
		hdr = (struct rec_diag_hdr *)(int)local_buf_ptr;
		local_buf_ptr += HEADER_LEN + hdr->page_len;
	}

	(void) close(fd);
	return (0);
}

/*ARGSUSED*/
int
l_get_limited_map(char *path, struct lilpmap *map_ptr, int verbose)
{
int	fd, i;
char	drvr_path[MAXPATHLEN];
struct	stat	stbuf;


	/* initialize map */
	(void) memset(map_ptr, 0, sizeof (struct lilpmap));

	(void) strcpy(drvr_path, path);
	/*
	 * Get the path to the :devctl driver
	 *
	 * This assumes the path looks something like this:
	 * /devices/sbus@1f,0/SUNW,socal@1,0:1
	 * or
	 * /devices/sbus@1f,0/SUNW,socal@1,0
	 * or
	 * a 1 level PCI type driver
	 */
	if (stat(drvr_path, &stbuf) < 0) {
		(void) sprintf(l_error_msg,
			MSGSTR(-1,
			"Cannot obtain the status of path %s\n"),
			drvr_path);
		l_error_msg_ptr = (char *)&l_error_msg;
		return (L_INVALID_PATH);
	}
	if ((stbuf.st_mode & S_IFMT) == S_IFDIR) {
		/* append a port. Just try 0 since they did not give us one */
		(void) strcat(drvr_path, ":0");
		(void) fprintf(stdout, MSGSTR(-1,
			"Selecting default path %s\n"), drvr_path);
	}

	P_DPRINTF("  l_get_limited_map: Geting drive map from:"
		" %s\n", drvr_path);

	/* open controller */
	if ((fd = object_open(drvr_path, O_NDELAY | O_RDONLY)) == -1)
		return (errno);


	if (ioctl(fd, FCIO_GETMAP, map_ptr) != 0) {
		I_DPRINTF("  FCIO_GETMAP ioctl failed\n");
		(void) close(fd);
		return (errno);
	}
	(void) close(fd);

	/*
	 * Check for reasonableness.
	 */
	if ((map_ptr->lilp_length > 126) || (map_ptr->lilp_magic != 0x1107)) {
		(void) sprintf(l_error_msg,
			MSGSTR(-1, "Invalid loop map found.\n"));
		l_error_msg_ptr = (char *)&l_error_msg; /* set error ptr */
		return (L_LOOP_ERROR);
	}
	for (i = 0; i < (u_int)map_ptr->lilp_length; i++) {
		if ((map_ptr->lilp_list[i] > 0xef) ||
			(map_ptr->lilp_list[i] < 0)) {
			(void) sprintf(l_error_msg,
			MSGSTR(-1, "Invalid loop map found.\n"));
			l_error_msg_ptr = (char *)&l_error_msg;
			return (L_LOOP_ERROR);
		}
	}

	return (0);
}

/*
 * Get device map from nexus driver
 *
 * INPUTS:
 *	- The path must be the physical path
 */
/*ARGSUSED*/
int
l_get_dev_map(char *path, sf_al_map_t *map_ptr, int verbose)
{
int	fd, i;
char	drvr_path[MAXPATHLEN];
char	*char_ptr;
struct	stat	stbuf;


	/* initialize map */
	(void) memset(map_ptr, 0, sizeof (struct sf_al_map));

	(void) strcpy(drvr_path, path);
	/*
	 * Get the path to the :devctl driver
	 *
	 * This assumes the path looks something like this:
	 * /devices/sbus@1f,0/SUNW,socal@1,0/SUNW,sf@0,0/ses@e,0:0
	 * or
	 * /devices/sbus@1f,0/SUNW,socal@1,0/SUNW,sf@0,0
	 * or
	 * /devices/sbus@1f,0/SUNW,socal@1,0/SUNW,sf@0,0:devctl
	 * or
	 * a 1 level PCI type driver but still :devctl
	 */
	if (strstr(drvr_path, DRV_NAME_SSD) || strstr(drvr_path, SES_NAME)) {
		if ((char_ptr = strrchr(drvr_path, '/')) == NULL) {
			(void) sprintf(l_error_msg,
				MSGSTR(-1,
				"in the device physical path\n"));
			l_error_msg_ptr = (char *)&l_error_msg;
			return (L_INVALID_PATH);
		}
		*char_ptr = '\0';   /* Terminate sting  */
		/* append controller */
		(void) strcat(drvr_path, FC_CTLR);
	} else {
		if (stat(drvr_path, &stbuf) < 0) {
			(void) sprintf(l_error_msg,
				MSGSTR(-1,
				"Cannot obtain the status of path %s\n"),
				drvr_path);
			l_error_msg_ptr = (char *)&l_error_msg;
			return (L_INVALID_PATH);
		}
		if ((stbuf.st_mode & S_IFMT) == S_IFDIR) {
			/* append controller */
			(void) strcat(drvr_path, FC_CTLR);
		}
	}

	P_DPRINTF("  l_get_dev_map: Geting drive map from:"
		" %s\n", drvr_path);

	/* open controller */
	if ((fd = object_open(drvr_path, O_NDELAY | O_RDONLY)) == -1)
		return (errno);


	if (ioctl(fd, SFIOCGMAP, map_ptr) != 0) {
		I_DPRINTF("  SFIOCGMAP ioctl failed.\n");
		(void) close(fd);
		return (errno);
	}
	(void) close(fd);

	/*
	 * Check for reasonableness.
	 */
	if ((map_ptr->sf_count > 126) || (map_ptr->sf_count < 0)) {
		(void) sprintf(l_error_msg,
			MSGSTR(-1, "Invalid loop map found.\n"));
		l_error_msg_ptr = (char *)&l_error_msg; /* set error ptr */
		return (L_LOOP_ERROR);
	}
	for (i = 0; i < map_ptr->sf_count; i++) {
		if ((map_ptr->sf_addr_pair[i].sf_al_pa > 0xef) ||
			(map_ptr->sf_addr_pair[i].sf_al_pa < 0)) {
			(void) sprintf(l_error_msg,
			MSGSTR(-1, "Invalid loop map found.\n"));
			l_error_msg_ptr = (char *)&l_error_msg;
			return (L_LOOP_ERROR);
		}
	}

	return (0);
}

/*
 * Get device World Wide Name and AL_PA for device at path
 *
 * RETURN: 0 O.K.
 *
 * INPUTS:
 *	- path_phys must be of a device, either an IB or disk.
 */
int
l_get_wwn(char *path_phys, u_char port_wwn[], u_char node_wwn[],
	int *al_pa, int verbose)
{
sf_al_map_t	map;
int	i, j, nu, err;
char	*char_ptr, *ptr;
int	found = 0;

	P_DPRINTF("  l_get_wwn: Getting device WWN"
			" and al_pa for device: %s\n",
			path_phys);

	if (err = l_get_dev_map(path_phys, &map, verbose)) {
		return (err);
	}

	/*
	 * Get the loop identifier (switch setting) from the path.
	 *
	 * This assumes the path looks something like this:
	 * /devices/.../SUNW,socal@3,0/SUNW,sf@0,0/SUNW,ssd@x,0
	 */
	if ((char_ptr = strrchr(path_phys, '@')) == NULL) {
		(void) sprintf(l_error_msg,
		MSGSTR(-1, "in the device physical path: no @ found\n"));
		l_error_msg_ptr = (char *)&l_error_msg; /* set error ptr */
		return (L_INVALID_PATH);
	}
	char_ptr++;	/* point to the loop identifier or WWN */
	if (name_id) {
		nu = strtol(char_ptr, &ptr, 16);
		if (ptr == char_ptr) {
			(void) sprintf(l_error_msg,
			MSGSTR(-1, "in the device physical path:"
				" no ID found\n"));
			l_error_msg_ptr = (char *)&l_error_msg;
			return (L_INVALID_PATH);
		}
		if ((nu > 0x7e) || (nu < 0)) {
			(void) sprintf(l_error_msg,
			MSGSTR(-1,
			"in the device physical path: invalid ID=0x%x\n"),
				nu);
			l_error_msg_ptr = (char *)&l_error_msg;
			return (L_INVALID_PATH);
		}
		for (i = 0; i < map.sf_count; i++) {
			*al_pa = map.sf_addr_pair[i].sf_al_pa;
			if (l_switch_to_alpa[nu] == *al_pa) {
				found = 1;
				break;
			}
		}
	} else {
		unsigned long long pwwn;
		u_char	*byte_ptr;

		/* Format of WWN is ssd@w2200002037000f96,0:a,raw */
		if (*char_ptr != 'w') {
			(void) sprintf(l_error_msg,
			MSGSTR(-1, "in the device physical path:"
			" invalid WWN format.\n"));
			l_error_msg_ptr = (char *)&l_error_msg;
			return (L_INVALID_PATH);
		}
		char_ptr++;
		pwwn = strtoull(char_ptr, &ptr, 16);
		if (ptr == char_ptr) {
			(void) sprintf(l_error_msg,
			MSGSTR(-1, "in the device physical path:"
				" no WWN found\n"));
			l_error_msg_ptr = (char *)&l_error_msg;
			return (L_INVALID_PATH);
		}
		P_DPRINTF("  l_get_wwn:  Looking for WWN "
		"0x%llx\n", pwwn);
		for (i = 0; i < map.sf_count; i++) {
			byte_ptr = (u_char *)&pwwn;
			for (j = 0; j < 8; j++) {
				if (map.sf_addr_pair[i].sf_port_wwn[j] !=
					*byte_ptr++) {
					found = 0;
					break;
				} else {
					found = 1;
				}
			}
			if (found) {
				break;
			}
		}
	}

	if (!found) {
		(void) sprintf(l_error_msg,
		MSGSTR(-1, "could not find the loop address for "
		"the device at path %s\n"),
		path_phys);
		l_error_msg_ptr = (char *)&l_error_msg; /* set error ptr */
		return (L_INVALID_PATH);
	}
	for (j = 0; j < 8; j++) {
		port_wwn[j] = map.sf_addr_pair[i].sf_port_wwn[j];
		node_wwn[j] = map.sf_addr_pair[i].sf_node_wwn[j];
	}
	*al_pa = map.sf_addr_pair[i].sf_al_pa;

	return (0);
}

/*
 * Get individual disk status.
 *
 * Path must be physical and point to a disk.
 *
 * This function updates the l_disk_state->state_flags,
 * the port WWN's and l_disk_state->num_blocks for
 * all ports accessable.
 */
int
l_get_disk_status(char *path, struct l_disk_state_struct *l_disk_state,
	int verbose)
{
struct dlist	*ml;
char		path_a[MAXPATHLEN];
char		path_b[MAXPATHLEN];
char		ses_path[MAXPATHLEN];
sf_al_map_t	map;
int		path_a_found = 0;
int		path_b_found = 0;
int		local_port_a_flag;
u_char		node_wwn[WWN_SIZE], port_wwn[WWN_SIZE];
int		al_pa;
int		err;

	*path_a = *path_b = NULL;
	l_disk_state->num_blocks = 0;	/* initialize */

	/* Get paths. */
	l_disk_state->multipath_list = l_get_multipath(path, verbose);

	ml = l_disk_state->multipath_list;
	if (ml == NULL) {
		l_disk_state->l_state_flag = L_NO_PATH_FOUND;
		G_DPRINTF("  l_get_disk_status: Error finding a "
			"multipath to the disk.\n");
		return (0);
	}

	while (ml && (!(path_a_found && path_b_found))) {
		if (err = l_get_dev_map(ml->dev_path, &map, verbose)) {
			return (err);
		}
		if (err = l_get_ses_path(ml->dev_path, ses_path,
			&map, verbose)) {
			return (err);
		}
		/*
		 * Get the port, A or B, of the disk,
		 * by passing the IB path.
		 */
		if (err = l_get_port(ses_path, &local_port_a_flag, verbose)) {
			return (err);
		}
		if (local_port_a_flag && (!path_a_found)) {
			G_DPRINTF("  l_get_disk_status: Path to Port A "
				"found: %s\n", ml->dev_path);
			if (err = l_get_disk_port_status(ml->dev_path,
				l_disk_state, local_port_a_flag, verbose)) {
				return (err);
			}
			if (err = l_get_wwn(ml->dev_path,
				port_wwn, node_wwn,
				&al_pa, verbose)) {
				return (err);
			}
			(void) sprintf(l_disk_state->port_a_wwn_s,
			"%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x",
			port_wwn[0], port_wwn[1], port_wwn[2], port_wwn[3],
			port_wwn[4], port_wwn[5], port_wwn[6], port_wwn[7]);

			l_disk_state->port_a_valid++;
			path_a_found++;
		}
		if ((!local_port_a_flag) && (!path_b_found)) {
			G_DPRINTF("  l_get_disk_status: Path to Port B "
				"found: %s\n", ml->dev_path);
			if (err = l_get_disk_port_status(ml->dev_path,
				l_disk_state, local_port_a_flag, verbose)) {
				return (err);
			}
			if (err = l_get_wwn(ml->dev_path,
				port_wwn, node_wwn,
				&al_pa, verbose)) {
				return (err);
			}
			(void) sprintf(l_disk_state->port_b_wwn_s,
			"%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x",
			port_wwn[0], port_wwn[1], port_wwn[2], port_wwn[3],
			port_wwn[4], port_wwn[5], port_wwn[6], port_wwn[7]);

			l_disk_state->port_b_valid++;
			path_b_found++;
		}
		ml = ml->next;
	}
	return (0);
}

/*
 * Check for Persistent Reservations.
 */
static int
l_persistent_check(int fd, struct l_disk_state_struct *l_disk_state,
	int verbose)
{
int	status;
Read_keys	read_key_buf;
Read_reserv	read_reserv_buf;

	(void) memset(&read_key_buf, 0, sizeof (struct  read_keys_struct));
	if ((status = scsi_persistent_reserve_in_cmd(fd,
		(u_char *)&read_key_buf, sizeof (struct  read_keys_struct),
		ACTION_READ_KEYS))) {
		return (status);
	}
	/* This means persistent reservations are supported by the disk. */
	l_disk_state->persistent_reserv_flag = 1;

	if (read_key_buf.rk_length) {
		l_disk_state->persistent_registered = 1;
	}

	(void) memset(&read_reserv_buf, 0, sizeof (struct  read_reserv_struct));
	if ((status = scsi_persistent_reserve_in_cmd(fd,
		(u_char *)&read_reserv_buf, sizeof (struct  read_reserv_struct),
		ACTION_READ_RESERV))) {
		return (status);
	}
	if (read_reserv_buf.rr_length) {
		l_disk_state->persistent_active = 1;
	}
	if (verbose) {
		(void) fprintf(stdout,
		MSGSTR(-1, "  Checking for Persistant "
			"Reservations:"));
		if (l_disk_state->persistent_reserv_flag) {
			(void) fprintf(stdout,
			MSGSTR(-1,
			"%s\n"), l_disk_state->persistent_active ? "active" :
			"registered");
		} else {
			(void) fprintf(stdout,
			MSGSTR(-1,
			" not being used\n"));
		}
	}
	return (0);
}

int
l_get_disk_port_status(char *path, struct l_disk_state_struct *l_disk_state,
	int port_a_flag, int verbose)
{
int		status = 0;
Read_capacity_data	capacity;	/* local read capacity buffer */
int		fd;
struct vtoc	vtoc;
int		local_state = 0;

	/*
	 * Try to open drive.
	 */
	if ((fd = object_open(path, O_RDONLY)) == -1) {
	    if ((fd = object_open(path,
		O_RDONLY | O_NDELAY)) == -1) {
		G_DPRINTF("  l_get_disk_port_status: Error "
			"opening drive %s\n", path);
		local_state = L_OPEN_FAIL;
	    } else {
		/* See if drive ready */
		if (status = scsi_tur(fd)) {
			if ((status & L_SCSI_ERROR) &&
				((status & ~L_SCSI_ERROR) == STATUS_CHECK)) {
				/*
				 * TBD
				 * This is where I should figure out
				 * if the device is Not Ready or whatever.
				 */
				local_state = L_NOT_READY;
			} else if ((status & L_SCSI_ERROR) &&
				((status & ~L_SCSI_ERROR) ==
				STATUS_RESERVATION_CONFLICT)) {
				/* mark reserved */
				local_state = L_RESERVED;
			} else {
				local_state = L_SCSI_ERR;
			}

		/*
		 * There may not be a label on the drive - check
		 */
		} else if (ioctl(fd, DKIOCGVTOC, &vtoc) == -1) {
		    I_DPRINTF("\t- DKIOCGVTOC ioctl failed: "
		    " invalid geometry\n");
		    local_state = L_NO_LABEL;
		} else {
			/*
			 * Sanity-check the vtoc
			 */
		    if (vtoc.v_sanity != VTOC_SANE ||
			vtoc.v_sectorsz != DEV_BSIZE) {
			local_state = L_NO_LABEL;
			G_DPRINTF("  l_get_disk_port_status: "
				"Checking vtoc - No Label found.\n");
		    }
		}
	    }
	}

	if ((local_state == 0) || (local_state == L_NO_LABEL)) {

	    if (status = scsi_read_capacity_cmd(fd, (u_char *)&capacity,
		sizeof (capacity))) {
			G_DPRINTF("  l_get_disk_port_status: "
				"Read Capacity failed.\n");
		if (status & L_SCSI_ERROR) {
		    if ((status & ~L_SCSI_ERROR) ==
			STATUS_RESERVATION_CONFLICT) {
			/* mark reserved */
			local_state |= L_RESERVED;
		    } else
			/* mark bad */
			local_state |= L_NOT_READABLE;
		} else {
			/*
			 * TBD
			 * Need a more complete state definition here.
			 */
			l_disk_state->d_state_flags[port_a_flag] = L_SCSI_ERR;
			(void) close(fd);
			return (0);
		}
	    } else {
		/* save capacity */
		l_disk_state->num_blocks = capacity.last_block_addr + 1;

		/* Check for Persistent Reservations. */
		if (l_persistent_check(fd, l_disk_state, verbose)) {
			/* Ignore errors. */
			p_error_msg_ptr = l_error_msg_ptr = NULL;
		}
	    }

	}
	(void) close(fd);

	l_disk_state->d_state_flags[port_a_flag] = local_state;
	G_DPRINTF("  l_get_disk_port_status: Individual Disk"
		" Status: 0x%x for"
		" port %s for path:"
		" %s\n", local_state,
		port_a_flag ? "A" : "B", path);

	return (0);
}

/*
 * Copy and format page 1 from big buffer to state structure.
 */

static int
copy_config_page(struct l_state_struct *l_state, u_char *from_ptr)
{
IB_page_config	*encl_ptr;
int		size;
int		i;


	encl_ptr = (struct ib_page_config *)(int)from_ptr;

	/* Sanity check. */
	if ((encl_ptr->enc_len > MAX_VEND_SPECIFIC_ENC) ||
		(encl_ptr->enc_len == 0)) {
		(void) sprintf(l_error_msg,
		MSGSTR(-1, "parsing page %x\n"), L_PAGE_1);
		l_error_msg_ptr = (char *)&l_error_msg;
		return (L_REC_DIAG_PG1);
	}
	if ((encl_ptr->enc_num_elem > MAX_IB_ELEMENTS) ||
		(encl_ptr->enc_num_elem == 0)) {
		(void) sprintf(l_error_msg,
		MSGSTR(-1, "parsing page %x\n"), L_PAGE_1);
		l_error_msg_ptr = (char *)&l_error_msg;
		return (L_REC_DIAG_PG1);
	}

	size = HEADER_LEN + 4 + HEADER_LEN + encl_ptr->enc_len;
	bcopy((void *)(from_ptr),
		(void *)&l_state->ib_tbl.config, (size_t)size);
	/*
	 * Copy Type Descriptors seperately to get aligned.
	 */
	from_ptr += size;
	size = (sizeof (struct	type_desc_hdr))*encl_ptr->enc_num_elem;
	bcopy((void *)(from_ptr),
		(void *)&l_state->ib_tbl.config.type_hdr, (size_t)size);

	/*
	 * Copy Text Descriptors seperately to get aligned.
	 *
	 * Must use the text size from the Type Descriptors.
	 */
	from_ptr += size;
	for (i = 0; i < (int)l_state->ib_tbl.config.enc_num_elem; i++) {
		size = l_state->ib_tbl.config.type_hdr[i].text_len;
		bcopy((void *)(from_ptr),
			(void *)&l_state->ib_tbl.config.text[i], (size_t)size);
		from_ptr += size;
	}
	return (0);
}

/*
 * Copy page 7 (Element Descriptor page) to state structure.
 *
 * Copy header then copy each element descriptor
 * seperately.
 */
static void
copy_page_7(struct l_state_struct *l_state, u_char *from_ptr)
{
u_char	*my_from_ptr;
int	size, j, k, p7_index;
	size = HEADER_LEN +
		sizeof (l_state->ib_tbl.p7_s.gen_code);
	bcopy((void *)(from_ptr),
		(void *)&l_state->ib_tbl.p7_s, (size_t)size);
	my_from_ptr = from_ptr + size;
	if (getenv("_LUX_D_DEBUG") != NULL) {
		p_dump("  copy_page_7: Page 7 header:  ",
		(u_char *)&l_state->ib_tbl.p7_s, size,
		HEX_ASCII);
		(void) fprintf(stdout,
			"  copy_page_7: Elements being stored "
			"in state table\n"
			"              ");
	}
	/* I am assuming page 1 has been read. */
	for (j = 0, p7_index = 0;
		j < (int)l_state->ib_tbl.config.enc_num_elem; j++) {
		/* Copy global element */
		size = HEADER_LEN +
			((*(my_from_ptr + 2) << 8) | *(my_from_ptr + 3));
		bcopy((void *)(my_from_ptr),
		(void *)&l_state->ib_tbl.p7_s.element_desc[p7_index++],
			(size_t)size);
		my_from_ptr += size;
		for (k = 0; k < (int)l_state->ib_tbl.config.type_hdr[j].num;
			k++) {
			/* Copy individual elements */
			size = HEADER_LEN +
				((*(my_from_ptr + 2) << 8) |
					*(my_from_ptr + 3));
			bcopy((void *)(my_from_ptr),
			(void *)&l_state->ib_tbl.p7_s.element_desc[p7_index++],
				(size_t)size);
			my_from_ptr += size;
			if (getenv("_LUX_D_DEBUG") != NULL) {
				(void) fprintf(stdout,
					".");
			}
		}
	}
	if (getenv("_LUX_D_DEBUG") != NULL) {
		(void) fprintf(stdout, "\n");
	}
}

static int
get_ib_status(char *path, struct l_state_struct *l_state,
	int verbose)
{
int		err;
u_char		*ib_buf, *from_ptr;
int		num_pages, i, size;
IB_page_2	*encl_ptr;

	/*
	* get big buffer
	*/
	if ((ib_buf = (u_char *)malloc(MAX_REC_DIAG_LENGTH)) == NULL) {
		(void) sprintf(l_error_msg,
		MSGSTR(-1, "unable to malloc more space.\n"));
		l_error_msg_ptr = (char *)&l_error_msg;
		return (errno);
	}

	/*
	 * Get IB information
	 * Even if there are 2 IB's in this box on this loop don't bother
	 * talking to the other one as both IB's in a box
	 * are supposed to report the same information.
	 */
	if (err = l_get_envsen(path, ib_buf, MAX_REC_DIAG_LENGTH,
		verbose)) {
		free(ib_buf);
		return (err);
	}

	/*
	 * Set up state structure
	 */
	bcopy((void *)ib_buf, (void *)&l_state->ib_tbl.p0,
		(size_t)sizeof (struct  ib_page_0));

	num_pages = l_state->ib_tbl.p0.page_len;
	from_ptr = ib_buf + HEADER_LEN + l_state->ib_tbl.p0.page_len;

	for (i = 1; i < num_pages; i++) {
		if (l_state->ib_tbl.p0.sup_page_codes[i] == L_PAGE_1) {
			if (err = copy_config_page(l_state, from_ptr)) {
				(void) sprintf(l_error_msg,
				MSGSTR(-1,
				"parsing page %x\n"), L_PAGE_1);
				l_error_msg_ptr = (char *)&l_error_msg;
				return (err);
			}
		} else if (l_state->ib_tbl.p0.sup_page_codes[i] ==
			L_PAGE_2) {
			encl_ptr = (struct ib_page_2 *)(int)from_ptr;
			size = HEADER_LEN + encl_ptr->page_len;
			bcopy((void *)(from_ptr),
				(void *)&l_state->ib_tbl.p2_s, (size_t)size);
			if (getenv("_LUX_D_DEBUG") != NULL) {
				p_dump("  get_ib_status: Page 2:  ",
				(u_char *)&l_state->ib_tbl.p2_s, size,
				HEX_ONLY);
			}

		} else if (l_state->ib_tbl.p0.sup_page_codes[i] ==
			L_PAGE_7) {
			(void) copy_page_7(l_state, from_ptr);
		}
		from_ptr += ((*(from_ptr + 2) << 8) | *(from_ptr + 3));
		from_ptr += HEADER_LEN;
	}
	free(ib_buf);
	G_DPRINTF("  get_ib_status: Read %d Receive Diagnostic pages "
		"from the IB.\n", num_pages);
	return (0);
}

/*
 * Given an IB path get the port, A or B.
 */
int
l_get_port(char *ses_path, int *port_a, int verbose)
{
L_state	ib_state;
Ctlr_elem_st	ctlr;
int	i, err;
int	elem_index = 0;

	if (err = get_ib_status(ses_path, &ib_state, verbose)) {
		return (err);
	}

	for (i = 0; i < (int)ib_state.ib_tbl.config.enc_num_elem; i++) {
	    elem_index++;		/* skip global */
	    if (ib_state.ib_tbl.config.type_hdr[i].type == ELM_TYP_IB) {
		bcopy((const void *)
			&ib_state.ib_tbl.p2_s.element[elem_index],
			(void *)&ctlr, sizeof (ctlr));
		break;
	    }
	    elem_index += ib_state.ib_tbl.config.type_hdr[i].num;
	}
	*port_a = ctlr.report;
	G_DPRINTF("  l_get_port: Found ses is the %s card.\n",
		ctlr.report ? "A" : "B");
	return (err);
}

/*
 * Get the individual drives status for device at path
 * where the path is of the IB.
 */
static	int
l_get_individual_state(char *path,
	struct l_disk_state_struct *state, int verbose)
{
int		j, select_id, err;
char		temp_path[MAXPATHLEN];
char		sbuf[MAXPATHLEN], *char_ptr;
sf_al_map_t	map;
int		found_flag = 0;
int		i, port_a_flag, not_null;

	if ((state->ib_status.code != S_NOT_INSTALLED) &&
		(state->ib_status.code != S_NOT_AVAILABLE)) {

/*
TBD
NOTE: Check to see if :c is correct?
Really I should find the first one.
*/
		/*
		 * Get a new map.
		 */
	    if (err = l_get_dev_map(path, &map, verbose)) {
		return (err);
	    }
	    for (j = 0; j < map.sf_count; j++) {

		/*
		 * Get a generic path to a device
		 *
		 * This assumes the path looks something like this:
		 * /devices/sbus@1f,0/SUNW,socal@1,0/SUNW,sf@0,0/ses@x,0:0
		 * then creates a path that looks like
		 * /devices/sbus@1f,0/SUNW,socal@1,0/SUNW,sf@0,0/ssd@
		 */
		(void) strcpy(temp_path, path);
		if ((char_ptr = strrchr(temp_path, '/')) == NULL) {
			(void) sprintf(l_error_msg,
				MSGSTR(-1, "in the device physical path\n"));
			l_error_msg_ptr = (char *)&l_error_msg;
			return (L_INVALID_PATH);
		}
		*char_ptr = '\0';   /* Terminate sting  */
		(void) strcat(temp_path, SLSH_DRV_NAME_SSD);
		/*
		 * Create complete path.
		 *
		 * Build entry ssd@xx,0:c,raw
		 * where xx is the AL_PA for sun4d or the WWN for
		 * all other architectures.
		 */
		select_id =
			l_sf_alpa_to_switch[map.sf_addr_pair[j].sf_al_pa];
		G_DPRINTF("  l_get_individual_state: Searching loop map "
			"to find disk: ID:0x%x"
			" AL_PA:0x%x\n", select_id,
			state->ib_status.sel_id);
		if (name_id) {
			(void) sprintf(sbuf, "%x,0:c,raw", select_id);
		} else {
			(void) sprintf(sbuf,
			"w%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x"
			",0:c,raw",
				map.sf_addr_pair[j].sf_port_wwn[0],
				map.sf_addr_pair[j].sf_port_wwn[1],
				map.sf_addr_pair[j].sf_port_wwn[2],
				map.sf_addr_pair[j].sf_port_wwn[3],
				map.sf_addr_pair[j].sf_port_wwn[4],
				map.sf_addr_pair[j].sf_port_wwn[5],
				map.sf_addr_pair[j].sf_port_wwn[6],
				map.sf_addr_pair[j].sf_port_wwn[7]);
		}
		(void) strcat(temp_path, sbuf);

		/*
		 * If we find a device on this loop in this box
		 * update its status.
		 */
		if (state->ib_status.sel_id == select_id) {
			/*
			 * Found a device on this loop in this box.
			 *
			 * Update state.
			 */
			(void) sprintf(state->node_wwn_s,
			"%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x%1.2x",
				map.sf_addr_pair[j].sf_node_wwn[0],
				map.sf_addr_pair[j].sf_node_wwn[1],
				map.sf_addr_pair[j].sf_node_wwn[2],
				map.sf_addr_pair[j].sf_node_wwn[3],
				map.sf_addr_pair[j].sf_node_wwn[4],
				map.sf_addr_pair[j].sf_node_wwn[5],
				map.sf_addr_pair[j].sf_node_wwn[6],
				map.sf_addr_pair[j].sf_node_wwn[7]);

			(void) strcpy(state->physical_path, temp_path);

			/* Bad if WWN is all zeros. */
			for (i = 0, not_null = 0; i < WWN_SIZE; i++) {
				if (map.sf_addr_pair[j].sf_node_wwn[i]) {
					not_null++;
					break;
				}
			}
			if (not_null == 0) {
				if (err = l_get_port(path, &port_a_flag,
					verbose)) {
					return (err);
				}
				state->l_state_flag = L_INVALID_WWN;
				G_DPRINTF("  l_get_individual_state: "
					"Disk state was "
					" Invalid WWN.\n");
				return (0);
			}

			/* get device status */
			if (err = l_get_disk_status(temp_path, state,
				verbose)) {
				return (err);
			}
			found_flag++;
			break;
		}
	    }
	    if (!found_flag) {
		/*
		 * We did not find a disk in this box
		 * on this loop.
		 *
		 */
		if (err = l_get_port(path, &port_a_flag,
			verbose)) {
			return (err);
		}
		state->l_state_flag = L_INVALID_MAP;

		G_DPRINTF("  l_get_individual_state: Disk state was "
			"Not in map.\n");
	    }

	} else {
		G_DPRINTF("  l_get_individual_state: Disk state was %s.\n",
			(state->ib_status.code == S_NOT_INSTALLED) ?
			"Not Installed" : "Not Available");
	}
	return (0);
}


/*
 * Get the global state of the photon.
 *
 * The path must be of the ses driver.
 * e.g.
 * /devices/sbus@1f,0/SUNW,socal@1,0/SUNW,sf@0,0/ses@e,0:0
 * or
 * /devices/sbus@1f,0/SUNW,socal@1,0/SUNW,sf@0,0/ses@WWN,0:0
 */
int
l_get_status(char *path, struct l_state_struct *l_state, int verbose)
{
int		err, i;
int		initial_update_flag = 1;
int		front_index, rear_index;
L_inquiry	inq;
sf_al_map_t	*map_ptr = NULL;
sf_al_map_t	map;
u_char		node_wwn[WWN_SIZE], port_wwn[WWN_SIZE];
int		al_pa, found_front, found_rear;
char		ses_path_front[MAXPATHLEN];
char		ses_path_rear[MAXPATHLEN];
Box_list	*b_list = NULL;
Box_list	*o_list = NULL;
char		node_wwn_s[(WWN_SIZE*2)+1];
u_int		select_id;

	G_DPRINTF("  l_get_status: Get Status for enclosure at: "
		" %s\n", path);

	if (initial_update_flag) {
		/* initialization */
		(void) memset(l_state, 0, sizeof (struct l_state_struct));
	}

	if (err = l_get_inquiry(path, &inq)) {
		return (err);
	}
	if ((strstr((char *)inq.inq_pid, ENCLOSURE_PROD_ID) == 0) &&
		(!(strncmp((char *)inq.inq_vid, "SUN     ",
		sizeof (inq.inq_vid)) &&
		(inq.inq_dtype == DTYPE_ESI)))) {
		(void) sprintf(l_error_msg,
		MSGSTR(-1, "Path does not specify a subsystem"
		" of type %s\n"), ENCLOSURE_PROD_ID);
		l_error_msg_ptr = (char *)&l_error_msg;
		return (L_INVALID_PATH);
	}

	strncpy((char *)l_state->ib_tbl.enclosure_name,
		(char *)inq.inq_box_name, sizeof (inq.inq_box_name));

	/*
	 * Get all of the IB Receive Diagnostic pages.
	 */
	if (err = get_ib_status(path, l_state, verbose)) {
		return (err);
	}

	/*
	 * Get the total number of drives per box.
	 * This assumes front & rear are the same.
	 */
	l_state->total_num_drv = 0; /* default to use as a flag */
	for (i = 0; i < (int)l_state->ib_tbl.config.enc_num_elem; i++) {
		if (l_state->ib_tbl.config.type_hdr[i].type == ELM_TYP_DD) {
			if (l_state->total_num_drv) {
				if (l_state->total_num_drv !=
				(l_state->ib_tbl.config.type_hdr[i].num * 2)) {
					(void) sprintf(l_error_msg,
				MSGSTR(-1, "The number of disks in the "
				"front & rear of the enclosure are different.\n"
				"This is not a supported configuration.\n"));
					l_error_msg_ptr = (char *)&l_error_msg;
					return (-1);
				}
			} else {
				l_state->total_num_drv =
				l_state->ib_tbl.config.type_hdr[i].num * 2;
			}
		}
	}

	/*
	 * transfer the individual drive Device Element information
	 * from IB state to drive state.
	 */
	if (err = l_get_disk_element_index(l_state, &front_index,
		&rear_index)) {
		return (err);
	}
	/* Skip global element */
	front_index++;
	rear_index++;
	for (i = 0; i < l_state->total_num_drv/2; i++) {
		bcopy((void *)&l_state->ib_tbl.p2_s.element[front_index + i],
		(void *)&l_state->drv_front[i].ib_status,
		(size_t)sizeof (struct device_element));
		bcopy((void *)&l_state->ib_tbl.p2_s.element[rear_index + i],
		(void *)&l_state->drv_rear[i].ib_status,
		(size_t)sizeof (struct device_element));
	}
	if (getenv("_LUX_D_DEBUG") != NULL) {
		p_dump("  l_get_status: disk elements:  ",
		(u_char *)&l_state->ib_tbl.p2_s.element[front_index],
		((sizeof (struct device_element)) * (l_state->total_num_drv)),
		HEX_ONLY);
	}

	/*
	 * Now get the individual devices information from
	 * the device itself.
	 *
	 * May need to use multiple paths to get to the
	 * front and rear drives in the box.
	 * If the loop is split some drives may not even be available
	 * from this host.
	 *
	 * The way this works is in the select ID the front disks
	 * are accessed via the IB with the bit 4 = 0
	 * and the rear disks by the IB with bit 4 = 1.
	 *
	 * First get device map from fc nexus driver for this loop.
	 */
	map_ptr = &map;
	if (err = l_get_dev_map(path, map_ptr, verbose)) {
		return (err);
	}
	/*
	 * Get the boxes node WWN & al_pa for this path.
	 */
	if (err = l_get_wwn(path, port_wwn, node_wwn, &al_pa, verbose)) {
		return (err);
	}
	if (err = l_get_box_list(&o_list, verbose)) {
		return (err);	/* Failure */
	}

	found_front = found_rear = 0;
	for (i = 0; i < WWN_SIZE; i++) {
		sprintf(&node_wwn_s[i << 1], "%02x", node_wwn[i]);
	}
	select_id = l_sf_alpa_to_switch[al_pa];
	l_state->ib_tbl.box_id = (select_id & BOX_ID_MASK) >> 5;

	G_DPRINTF("  l_get_status: Using this select_id 0x%x "
		"and node WWN %s\n",
		select_id, node_wwn_s);

	if (select_id & ALT_BOX_ID) {
		found_rear = 1;
		(void) strcpy(ses_path_rear, path);
		b_list = o_list;
		while (b_list) {
			if (strcmp(b_list->b_node_wwn_s, node_wwn_s) == 0) {
				if (err = l_get_wwn(b_list->b_physical_path,
					port_wwn, node_wwn,
					&al_pa, verbose)) {
					return (err);
				}
				select_id = l_sf_alpa_to_switch[al_pa];
				if (!(select_id & ALT_BOX_ID)) {
					(void) strcpy(ses_path_front,
					b_list->b_physical_path);
					found_front = 1;
					break;
				}
			}
			b_list = b_list->box_next;
		}
	} else {
		(void) strcpy(ses_path_front, path);
		found_front = 1;
		b_list = o_list;
		while (b_list) {
			if (strcmp(b_list->b_node_wwn_s, node_wwn_s) == 0) {
				if (err = l_get_wwn(b_list->b_physical_path,
					port_wwn, node_wwn,
					&al_pa, verbose)) {
					return (err);
				}
				select_id = l_sf_alpa_to_switch[al_pa];
				if (select_id & ALT_BOX_ID) {
					(void) strcpy(ses_path_rear,
					b_list->b_physical_path);
					found_rear = 1;
					break;
				}
			}
			b_list = b_list->box_next;
		}
	}
	l_free_box_list(&b_list);

	if (getenv("_LUX_G_DEBUG") != NULL) {
		if (!found_front) {
		printf("  l_get_status: Loop to front disks not found.\n");
		}
		if (!found_rear) {
		printf("  l_get_status: Loop to rear disks not found.\n");
		}
	}

	if (found_front) {
		for (i = 0; i < l_state->total_num_drv/2; i++) {
			if (err = l_get_individual_state(ses_path_front,
			(struct l_disk_state_struct *)&l_state->drv_front[i],
			verbose)) {
				return (err);
			}
		}
	} else {
		/* Set to loop not accessable. */
		for (i = 0; i < l_state->total_num_drv/2; i++) {
			l_state->drv_front[i].l_state_flag = L_NO_LOOP;
		}
	}
	if (found_rear) {
		for (i = 0; i < l_state->total_num_drv/2; i++) {
			if (err = l_get_individual_state(ses_path_rear,
			(struct l_disk_state_struct *)&l_state->drv_rear[i],
			verbose)) {
				return (err);
			}
		}
	} else {
		/* Set to loop not accessable. */
		for (i = 0; i < l_state->total_num_drv/2; i++) {
			l_state->drv_rear[i].l_state_flag = L_NO_LOOP;
		}
	}
	return (0);
}


int
l_get_inquiry(char *path, L_inquiry *l_inquiry)
{
int	    fd;
int 	status;

	P_DPRINTF("  l_get_inquiry: path: %s\n", path);
	if ((fd = object_open(path, O_NDELAY | O_RDONLY)) == -1)
		return (errno);
	status = scsi_inquiry_cmd(fd,
		(u_char *)l_inquiry, sizeof (struct l_inquiry_struct));
	(void) close(fd);
	return (status);
}


int
l_get_perf_statistics(char *path, u_char *perf_ptr)
{
int	fd;

	P_DPRINTF("  l_get_perf_statistics: Get Performance Statistics:"
		"\n  Path:%s\n",
		path);

	/* initialize tables */
	(void) memset(perf_ptr, 0, sizeof (int));

	/* open controller */
	if ((fd = object_open(path, O_NDELAY | O_RDONLY)) == -1)
		return (errno);


	/* update parameters in the performance table */

	/* get the period in seconds */


	(void) close(fd);

	return (0);
}

/*
 * Check the file for validity:
 *	- verify the size is that of 3 proms worth of text.
 *	- verify PROM_MAGIC.
 *	- verify (and print) the date.
 *	- verify the checksum.
 *	- verify the WWN == 0.
 * Since this requires reading the entire file, do it now and pass a pointer
 * to the allocated buffer back to the calling routine (which is responsible
 * for freeing it).  If the buffer is not allocated it will be NULL.
 */

static int
check_file(int fd, int verbose, u_char **buf_ptr, int dl_info_offset)
{
struct	exec	the_exec;
int		temp, i, j, *p, size, *start;
u_char		*buf;
char		*date_str;
struct	dl_info	*dl_info;

	*buf_ptr = NULL;

	/* read exec header */
	if (lseek(fd, 0, SEEK_SET) == -1)
		return (errno);
	if ((temp = read(fd, (char *)&the_exec, sizeof (the_exec))) == -1) {
	    (void) sprintf(l_error_msg,
		MSGSTR(-1, "reading download file exec header: %s\n"),
		strerror(errno));
	    l_error_msg_ptr = (char *)&l_error_msg; /* set error ptr */
	    return (errno);
	}
	if (temp != sizeof (the_exec)) {
	    (void) sprintf(l_error_msg, MSGSTR(-1, " reading exec header:"
			"incorrect number of bytes read.\n"));
	    l_error_msg_ptr = (char *)&l_error_msg; /* set error ptr */
	    return (L_DOWNLOAD_FILE);
	}

	if (the_exec.a_text != PROMSIZE) {
	    (void) sprintf(l_error_msg,
		MSGSTR(-1,
		"Text segment wrong size: 0x%x (expecting 0x%x)\n"),
		the_exec.a_text, PROMSIZE);
	    l_error_msg_ptr = (char *)&l_error_msg; /* set error ptr */
	    return (L_DOWNLOAD_FILE);
	}

	if (!(buf = (u_char *) zalloc(PROMSIZE)))
	    return (errno);
	if ((temp = read(fd, buf, PROMSIZE)) == -1) {
	    (void) sprintf(l_error_msg,
		MSGSTR(-1, "reading download file: %s\n"),
		strerror(errno));
	    l_error_msg_ptr = (char *)&l_error_msg; /* set error ptr */
	    return (errno);
	}

	if (temp != PROMSIZE) {
	    (void) sprintf(l_error_msg,
		MSGSTR(-1, "reading: incorrect number of bytes read.\n"));
	    l_error_msg_ptr = (char *)&l_error_msg; /* set error ptr */
	    return (L_DOWNLOAD_FILE);
	}



	/* check the IB firmware MAGIC */
	dl_info = (struct dl_info *)((int)buf + dl_info_offset);
	if (dl_info->magic != PROM_MAGIC) {
		(void) sprintf(l_error_msg,
			MSGSTR(-1, " Bad Firmware MAGIC.\n"));
		l_error_msg_ptr = (char *)&l_error_msg; /* set error ptr */
		return (L_DOWNLOAD_FILE);
	}

	/*
	* Get the date
	*/

	date_str = ctime((const long *)&dl_info->datecode);

	if (verbose) {
		(void) fprintf(stdout,
		MSGSTR(-1, "  IB Prom Date: %s"),
		date_str);
	}

	/*
	 * verify checksum
	*/

	if (dl_info_offset == FPM_DL_INFO) {
		start = (int *)((int)buf + FPM_OFFSET);
		size = FPM_SZ;
	} else {
		start = (int *)(int)buf;
		size = TEXT_SZ + IDATA_SZ;
	}

	for (j = 0, p = start, i = 0; i < (size/ 4); i++, j ^= *p++);

	if (j != 0) {
		(void) sprintf(l_error_msg,
			MSGSTR(-1, "Download file checksum failed.\n"));
		l_error_msg_ptr = (char *)&l_error_msg; /* set error ptr */
		return (L_DOWNLOAD_CHKSUM);
	}

	/* file verified */
	*buf_ptr = buf;

	return (0);
}

int
l_check_file(char *file, int verbose)
{
int	file_fd;
int	err;
u_char	*buf;

	if ((file_fd = object_open(file, O_RDONLY)) == -1) {
	    return (errno);
	}
	err = check_file(file_fd, verbose, &buf, FW_DL_INFO);
	if (buf)
		(void) destroy_data((char *)buf);
	return (err);
}

int
l_start(char *path)
{
int	status;
int	fd;

	P_DPRINTF("p_funct: Start: Path %s\n", path);
	if ((fd = object_open(path, O_NDELAY | O_RDONLY)) == -1)
		return (errno);
	status = scsi_start_cmd(fd);
	(void) close(fd);
	return (status);
}

int
l_stop(char *path, int immediate_flag)
{
int	status;
int	fd;
	P_DPRINTF("p_funct: Stop: Path %s\n", path);
	if ((fd = object_open(path, O_NDELAY | O_RDONLY)) == -1)
		return (errno);
	status = scsi_stop_cmd(fd, immediate_flag);
	(void) close(fd);
	return (status);
}

int
l_reserve(char *path)
{
int 	fd, status;

	P_DPRINTF("p_funct: Reserve: Path %s\n", path);
	if ((fd = object_open(path, O_NDELAY | O_RDONLY)) == -1)
		return (errno);
	status = scsi_reserve_cmd(fd);
	(void) close(fd);
	return (status);
}

int
l_release(char *path)
{
int 	fd, status;

	P_DPRINTF("p_funct: Release: Path %s\n", path);
	if ((fd = object_open(path, O_NDELAY | O_RDONLY)) == -1)
		return (errno);
	status = scsi_release_cmd(fd);
	(void) close(fd);
	return (status);
}

static char
ctoi(char c)
{
	if ((c >= '0') && (c <= '9'))
		c -= '0';
	else if ((c >= 'A') && (c <= 'F'))
		c = c - 'A' + 10;
	else if ((c >= 'a') && (c <= 'f'))
		c = c - 'a' + 10;
	else
		c = -1;
	return (c);
}

static int
string_to_wwn(u_char *wwn, u_char *wwnp)
{
	int	i;
	char	c, c1;

	*wwnp++ = 0;
	*wwnp++ = 0;
	for (i = 0; i < WWN_SIZE - 2; i++, wwnp++) {
		c = ctoi(*wwn++);
		c1 = ctoi(*wwn++);
		if (c == -1 || c1 == -1)
			return (-1);
		*wwnp = ((c << 4) + c1);
	}

	return (0);

}

/*
 * path		- physical path of Photon SES card
 * file		- input file for new code (may be NULL)
 * ps		- whether the "save" bit should be set
 * verbose	- to be verbose or not
 */
#define		DOWNLOAD_RETRIES 60*5	/* 5 minutes */
int
l_download(char *path_phys, char *file, int ps, int verbose)
{
int		file_fd, controller_fd;
int		err, status;
u_char		*buf_ptr;
char		printbuf[MAXPATHLEN];
char		*lc;
int		retry;
char		file_path[MAXPATHLEN];

	if (!file) {
		(void) strcpy(file_path, IBFIRMWARE_LOCALE);
		if ((lc = getenv("LANG")) == NULL) {
			lc = "C";		/* default to C */
		}
		strcat(file_path, lc);
		strcat(file_path, IBFIRMWARE_FILE);
		(void) fprintf(stdout,
		MSGSTR(-1, "  Using the default file %s.\n"), file_path);
	} else {
		strncpy(file_path, file, sizeof (file_path));
	}
	if (verbose)
		(void) fprintf(stdout, "%s\n",
			MSGSTR(-1, "  Opening the IB for I/O."));

	if ((controller_fd = object_open(path_phys, O_NDELAY | O_RDWR)) == -1)
		return (errno);

	(void) sprintf(printbuf, "  Doing download to:"
			"\n\t%s.\n  From file: %s.", path_phys, file_path);

	if (verbose)
		(void) fprintf(stdout, "%s\n", MSGSTR(-1, printbuf));
	P_DPRINTF("  Doing download to:"
			"\n\t%s\n  From file: %s\n", path_phys, file_path);

	if ((file_fd = object_open(file_path, O_NDELAY | O_RDONLY)) == -1) {
		return (errno);
	}
	if (err = check_file(file_fd, verbose, &buf_ptr, FW_DL_INFO)) {
		if (buf_ptr)
			(void) destroy_data((char *)buf_ptr);
		return (err);
	}
	if (verbose)
		(void) fprintf(stdout, "%s\n", MSGSTR(-1, "  Checkfile OK."));
	P_DPRINTF("  Checkfile OK.\n");
	(void) close(file_fd);

	if (verbose) {
		(void) fprintf(stdout, MSGSTR(-1,
			"  Verifying the IB is available.\n"));
	}

	retry = DOWNLOAD_RETRIES;
	while (retry) {
		if ((status = scsi_tur(controller_fd)) == 0) {
			break;
		} else {
			if ((retry % 30) == 0) {
				(void) fprintf(stdout,
				MSGSTR(-1,
				"  Waiting for the IB to be available.\n"));
			}
			(void) sleep(1);
		}
	}
	if (!retry) {
		if (buf_ptr)
			(void) destroy_data((char *)buf_ptr);
		(void) close(controller_fd);
		return (status);
	}

	if (verbose)
		(void) fprintf(stdout, "%s\n",
				MSGSTR(-1, "  Writing new text image to IB."));
	P_DPRINTF("  Writing new image to IB\n");
	status = scsi_ib_download_code_cmd(controller_fd, IBEEPROM, TEXT_OFFSET,
		(u_char *)((int)buf_ptr + (int)TEXT_OFFSET), TEXT_SZ, ps);
	if (status) {
		(void) close(controller_fd);
		(void) destroy_data((char *)buf_ptr);
		return (status);
	}
	if (verbose)
		(void) fprintf(stdout, "%s\n",
				MSGSTR(-1, "  Writing new data image to IB."));
	status = scsi_ib_download_code_cmd(controller_fd,
		IBEEPROM, IDATA_OFFSET,
		(u_char *)((int)buf_ptr + (int)IDATA_OFFSET), IDATA_SZ, ps);
	if (status) {
		(void) close(controller_fd);
		(void) destroy_data((char *)buf_ptr);
		return (status);
	}

	if (verbose) {
		(void) fprintf(stdout, MSGSTR(-1,
			"  Re-verifying the IB is available.\n"));
	}

	retry = DOWNLOAD_RETRIES;
	while (retry) {
		if ((status = scsi_tur(controller_fd)) == 0) {
			break;
		} else {
			if ((retry % 30) == 0) {
				(void) fprintf(stdout,
				MSGSTR(-1,
				"  Waiting for the IB to be available.\n"));
			}
			(void) sleep(1);
		}
		retry--;
	}
	if (!retry) {
		(void) close(controller_fd);
		(void) destroy_data((char *)buf_ptr);
		(void) sprintf(l_error_msg,
			MSGSTR(-1,
			"Error: Timed out in %d minutes waiting for the "
			"IB to become available.\n"),
			DOWNLOAD_RETRIES/60);
		l_error_msg_ptr = (char *)&l_error_msg;
		return (status);
	}

	if (verbose) {
		(void) fprintf(stdout, "%s\n",
			MSGSTR(-1, "  Writing new image to FPM."));
	}
	status = scsi_ib_download_code_cmd(controller_fd, MBEEPROM, FPM_OFFSET,
		(u_char *)((int)buf_ptr + FPM_OFFSET), FPM_SZ, ps);
	(void) destroy_data((char *)buf_ptr);

	if ((!status) && ps) {
		/*
		 * Reset the IB
		 */
		status = scsi_reset(controller_fd);
	}

	(void) close(controller_fd);
	return (status);
}

/*
 * Set the World Wide Name
 * in page 4 of the Send Diagnostic command.
 *
 * The path must point to an IB.
 */
int
l_set_wwn(char *path_phys, char *wwn)
{
Page4_name	page4;
L_inquiry	inq;
int		fd, status;
char		wwnp[WWN_SIZE];

	memset(&inq, 0, sizeof (inq));
	memset(&page4, 0, sizeof (page4));

	if ((fd = object_open(path_phys, O_NDELAY | O_RDONLY)) == -1) {
		return (errno);
	}
	/* Verify it is a Photon */
	if (status = scsi_inquiry_cmd(fd,
		(u_char *)&inq, sizeof (struct l_inquiry_struct))) {
		(void) close(fd);
		return (status);
	}
	if ((strstr((char *)inq.inq_pid, ENCLOSURE_PROD_ID) == 0) &&
		(!(strncmp((char *)inq.inq_vid, "SUN     ",
		sizeof (inq.inq_vid)) &&
		(inq.inq_dtype == DTYPE_ESI)))) {
		(void) close(fd);
		(void) sprintf(l_error_msg,
			MSGSTR(-1,
			"Invalid path, device is not an %s\n"),
			ENCLOSURE_PROD_ID);
		l_error_msg_ptr = (char *)&l_error_msg;
		return (L_INVALID_PATH);
	}

	page4.page_code = L_PAGE_4;
	page4.page_len = (u_short)((sizeof (struct page4_name) - 4));
	page4.string_code = L_WWN;
	page4.enable = 1;
	if (string_to_wwn((u_char *)wwn, (u_char *)&page4.name)) {
		close(fd);
		return (EINVAL);
	}
	bcopy((void *)wwnp, (void *)page4.name, (size_t)WWN_SIZE);

	if (status = scsi_send_diag_cmd(fd, (u_char *)&page4,
		sizeof (page4))) {
		(void) close(fd);
		return (status);
	}

	/*
	 * Check the wwn really changed.
	 */
	bzero((char *)page4.name, 32);
	if (status = scsi_rec_diag_cmd(fd, (u_char *)&page4,
				sizeof (page4), L_PAGE_4)) {
		(void) close(fd);
		return (status);
	}
	if (bcmp((char *)page4.name, wwnp, WWN_SIZE)) {
		(void) close(fd);
		(void) sprintf(l_error_msg,
			MSGSTR(-1,
			"The WWN change failed.\n"));
		l_error_msg_ptr = (char *)&l_error_msg;
		return (L_WARNING);
	}

	(void) close(fd);
	return (0);
}

/*
 * Use a physical path to a disk in a Photon box
 * as the base to genererate a path to a SES
 * card in this box.
 *
 * path_phys: Physical path to a Photon disk.
 * ses_path:  This must be a pointer to an already allocated path string.
 */
int
l_get_ses_path(char *path_phys, char *ses_path, sf_al_map_t *map,
	int verbose)
{
char	*char_ptr, *ptr, id_buf[MAXPATHLEN], wwn[20];
u_char	t_wwn[20], *ses_wwn, *ses_wwn1;
int	j, al_pa, al_pa1, box_id, nu, fd, disk_flag = 0, found = 0;

	(void) strcpy(ses_path, path_phys);
	if ((char_ptr = strrchr(ses_path, '/')) == NULL) {
		(void) sprintf(l_error_msg, MSGSTR(-1,
				"Error in the device physical path\n"));
			l_error_msg_ptr = (char *)&l_error_msg;
			return (L_INVALID_PATH);
	}
	disk_flag++;
	*char_ptr = '\0';   /* Terminate sting  */
	(void) strcat(ses_path, SLSH_SES_NAME);

	/*
	 * Figure out and create the boxes path name.
	 *
	 * NOTE: This uses the fact that the disks's
	 * AL_PA and the boxes AL_PA must match
	 * the assigned hard address in the current
	 * implementations. This may not be true in the
	 * future.
	 */
	if ((char_ptr = strrchr(path_phys, '@')) == NULL) {
		(void) sprintf(l_error_msg,
		MSGSTR(-1,
		"Error in the device physical path: no @ found\n"));
		l_error_msg_ptr = (char *)&l_error_msg;
		return (L_INVALID_PATH);
	}
	char_ptr++;	/* point to the loop identifier */
	if (name_id) {
		nu = strtol(char_ptr, &ptr, 16);
		if (ptr == char_ptr) {
			(void) sprintf(l_error_msg,
			MSGSTR(-1, "in the device physical path:"
				" no ID found\n"));
			l_error_msg_ptr = (char *)&l_error_msg;
			return (L_INVALID_PATH);
		}
		if ((nu > 0x7e) || (nu < 0)) {
			(void) sprintf(l_error_msg, MSGSTR(-1,
			"Error in the device physical path: invalid ID %d\n"),
				nu);
			l_error_msg_ptr = (char *)&l_error_msg;
			return (L_INVALID_PATH);
		}
		/*
		 * Mask out all but the box ID.
		 */
		nu &= BOX_ID_MASK;
		/*
		 * Or in the IB address.
		 */
		nu |= BOX_ID;

		(void) sprintf(id_buf, "%x,0:0", nu);
	} else {
		if (l_get_wwn(path_phys, t_wwn, t_wwn, &al_pa, verbose)) {
			(void) sprintf(l_error_msg, MSGSTR(-1,
			"Error in the device physical path: invalid wwn\n"));
			l_error_msg_ptr = (char *)&l_error_msg;
			return (L_INVALID_PATH);
		}
		box_id = l_sf_alpa_to_switch[al_pa] & BOX_ID_MASK;
		for (j = 0; j < map->sf_count; j++) {
			if (map->sf_addr_pair[j].sf_inq_dtype == DTYPE_ESI) {
				al_pa1 = map->sf_addr_pair[j].sf_al_pa;
				if (box_id == (l_sf_alpa_to_switch[al_pa1] &
						BOX_ID_MASK)) {
					if (!found) {
						ses_wwn =
					map->sf_addr_pair[j].sf_port_wwn;
						if (getenv("_LUX_P_DEBUG")) {
							ll_to_str(ses_wwn,
							(char *)t_wwn);
							(void) printf(
							"  l_get_ses_path: "
							"Found ses wwn = %s "
							"al_pa 0x%x\n",
							t_wwn, al_pa1);
						}
					} else {
						ses_wwn1 =
					map->sf_addr_pair[j].sf_port_wwn;
						if (getenv("_LUX_P_DEBUG")) {
							ll_to_str(ses_wwn1,
							(char *)t_wwn);
							(void) printf(
							"  l_get_ses_path: "
							"Found second ses "
							"wwn = %s "
							"al_pa 0x%x\n",
							t_wwn, al_pa1);
						}
					}
					found++;
				}
			}
		}
		if (!found) {
			(void) sprintf(l_error_msg, MSGSTR(-1,
			"Error in the device physical path: no SES\n"));
			l_error_msg_ptr = (char *)&l_error_msg;
			return (L_INVALID_PATH);
		}
		ll_to_str(ses_wwn, wwn);
		sprintf(id_buf, "w%s,0:0", wwn);
	}
	(void) strcat(ses_path, id_buf);
	if (verbose) {
		(void) fprintf(stdout,
			MSGSTR(-1, "  Creating enclosure path:\n    %s\n"),
			ses_path);
	}

	/*
	 * see if these paths exist.
	 */
	if ((fd = object_open(ses_path, O_NDELAY | O_RDONLY)) == -1) {
		char_ptr = strrchr(ses_path, '/');
		*char_ptr = '\0';
		(void) strcat(ses_path, SLSH_SES_NAME);
		if (name_id) {
			nu |= 0x10;	/* add alternate IB address bit */
			(void) sprintf(id_buf, "%x,0:0", nu);
			(void) strcat(ses_path, id_buf);
			return (0);
		} else {
			if (found > 1) {
				ll_to_str(ses_wwn1, wwn);
				P_DPRINTF("  l_get_ses_path: "
					"Using second path, ses wwn1 = %s\n",
					wwn);
				sprintf(id_buf, "w%s,0:0", wwn);
				strcat(ses_path, id_buf);
				return (0);
			} else {
				(void) sprintf(l_error_msg, MSGSTR(-1,
				"Error in the device physical path: no SES\n"));
				l_error_msg_ptr = (char *)&l_error_msg;
				return (L_INVALID_PATH);
			}
		}
	}
	close(fd);
	return (0);
}

/*
 * Get a valid location, front/rear & slot.
 *
 * path_struct->p_physical_path must be of a disk.
 *
 * OUTPUT: path_struct->slot_valid
 *	path_struct->slot
 *	path_struct->f_flag
 *
 * RETURN: 0 = O.K.
 */
int
l_get_slot(struct path_struct *path_struct, int verbose)
{
int		err, al_pa, slot;
u_char		node_wwn[8], port_wwn[8];
sf_al_map_t	map;
char		ses_path[MAXPATHLEN];
L_state		l_state;
int		found = 0;
u_int		select_id;

	/* Double check to see if we need to calculate. */
	if (path_struct->slot_valid) {
		return (0);
	}
	/* Programming error if this occures */
	assert(path_struct->ib_path_flag == NULL);
	if ((strstr(path_struct->p_physical_path, "ssd")) == NULL) {
	    (void) sprintf(l_error_msg,
	    MSGSTR(-1, "Physical path not of a disk: %s\n"),
		path_struct->p_physical_path);
	    /* set error ptr */
	    l_error_msg_ptr = (char *)&l_error_msg;
	    return (L_INVALID_PATH);
	}

	if (err = l_get_wwn(path_struct->p_physical_path, port_wwn, node_wwn,
		&al_pa, verbose)) {
		return (err);
	}
	if (err = l_get_dev_map(path_struct->p_physical_path,
		&map, verbose)) {
		return (err);
	}
	if (err = l_get_ses_path(path_struct->p_physical_path,
		ses_path, &map, verbose)) {
		return (err);
	}
	if ((err = l_get_status(ses_path, &l_state, verbose)) != 0) {
		return (err);
	}

	/*
	 * Find the slot by searching for the matching hard address.
	 */
	select_id = l_sf_alpa_to_switch[al_pa];
	P_DPRINTF("  l_get_slot: Searching Receive Diagnostic page 2, "
		"to find the slot number with this ID:0x%x\n",
		select_id);

	for (slot = 0; slot < l_state.total_num_drv/2; slot++) {
		if (l_state.drv_front[slot].ib_status.sel_id ==
			select_id) {
			path_struct->f_flag = 1;
			found = 1;
			break;
		} else if (l_state.drv_rear[slot].ib_status.sel_id ==
			select_id) {
			path_struct->f_flag = 0;
			found = 1;
			break;
		}
	}
	if (!found) {
		(void) sprintf(l_error_msg,
		MSGSTR(-1, "Unable to find the slot number.\n"));
		l_error_msg_ptr = (char *)&l_error_msg;
		return (-1);	/* Failure */
	}
	P_DPRINTF("  l_get_slot: Found slot %d %s.\n", slot,
		path_struct->f_flag ? "Front" : "Rear");
	path_struct->slot = slot;
	path_struct->slot_valid = 1;
	return (0);
}

/*
 * Get multiple paths to a given disk device.
 * The arg should be the physical path to device.
 *
 * NOTE: The caller must free the allocated lists.
 */
struct dlist *
l_get_multipath(char *path, int verbose)
{
	WWN_list	*wwn_list;
	char		*node_wwn_s = NULL;
	struct dlist	*dlh, *dlt, *dl;
	char		path1[MAXPATHLEN], *p;
	int		len;

	if (path == (char *)NULL)
		return (0);

	if (*path == '\0') {
		return (0);
	}

	/* Strip partition information. */
	p = strrchr(path, ':');
	if (p) {
		len = strlen(path) - strlen(p);
		strncpy(path1, path, len);
		path1[len] = '\0';
	} else {
		(void) strcpy(path1, path);
	}

	dlh  = dlt = (struct dlist *)NULL;

	H_DPRINTF("  l_get_multipath: Looking for multiple paths for"
		" device at path: %s\n", path);
	if (g_WWN_list == (WWN_list *) NULL) {
		if (l_get_wwn_list(&g_WWN_list, verbose)) {
			return (0);	/* Failure */
		}
	}
	wwn_list = g_WWN_list;
	while (wwn_list != (WWN_list *) NULL) {
		if (strstr(wwn_list->physical_path, path1)) {
			node_wwn_s = wwn_list->node_wwn_s;
			if ((dl = (struct dlist *)
				zalloc(sizeof (struct dlist))) == NULL) {
				return (NULL);
			}
			dl->dev_path = wwn_list->physical_path;
			dl->logical_path = wwn_list->logical_path;
			dlh = dlt = dl;
			break;
		}
		wwn_list = wwn_list->wwn_next;
	}
	if (node_wwn_s == NULL) {
		H_DPRINTF("node_wwn_s is NULL!\n");
		return (0);
	}
	wwn_list = g_WWN_list;
	while (wwn_list != (WWN_list *) NULL) {
		if ((strcmp(node_wwn_s, wwn_list->node_wwn_s) == 0) &&
			(strstr(wwn_list->physical_path, path1) == NULL)) {
			if ((dl = (struct dlist *)
				zalloc(sizeof (struct dlist))) == NULL) {
				while (dlh) {
					dl = dlh->next;
					free(dlh);
					dlh = dl;
				}
				return ((struct dlist *)NULL);
			}
			H_DPRINTF("  l_get_multipath: Found multipath=%s\n",
					wwn_list->physical_path);
			dl->dev_path = wwn_list->physical_path;
			dl->logical_path = wwn_list->logical_path;
			if (dlh == NULL) {
				dlh = dlt = dl;
			} else {
				dlt->next = dl;
				dl->prev = dlt;
				dlt = dl;
			}
		}
		wwn_list = wwn_list->wwn_next;
	}

	return (dlh);
}
/*
 * Get all ses paths paths to a given box.
 * The arg should be the physical path to one of the box's IB.
 *
 * NOTE: The caller must free the allocated lists.
 */
/*ARGSUSED*/
struct dlist *
l_get_allses(char *path, struct box_list_struct *box_list, int verbose)
{
	struct box_list_struct *box_list_head;
	char		*node_wwn_s = NULL;
	struct dlist	*dlh, *dlt, *dl;

	dlh  = dlt = (struct dlist *)NULL;

	H_DPRINTF("  l_get_allses: Looking for all ses paths for"
		" box at path: %s\n", path);

	box_list_head = box_list;

	while (box_list != (struct box_list_struct *)NULL) {
		H_DPRINTF("  l_get_allses: physical_path= %s\n",
				box_list->b_physical_path);
		if (strcmp(path, box_list->b_physical_path) == 0) {
			node_wwn_s = box_list->b_node_wwn_s;
			break;
		}
		box_list = box_list->box_next;
	}
	if (node_wwn_s == NULL) {
		H_DPRINTF("node_wwn_s is NULL!\n");
		return (0);
	}
	box_list = box_list_head;
	H_DPRINTF("  l_get_allses: node_wwn=%s\n", node_wwn_s);
	while (box_list_head != (struct box_list_struct *)NULL) {
		if (strcmp(node_wwn_s, box_list_head->b_node_wwn_s) == 0) {
			if ((dl = (struct dlist *)
				zalloc(sizeof (struct dlist))) == NULL) {
				while (dlh) {
					dl = dlh->next;
					free(dlh);
					dlh = dl;
				}
				dlh = NULL;
				goto done;
			}
			H_DPRINTF("  l_get_allses: Found ses=%s\n",
					box_list_head->b_physical_path);
			dl->dev_path = box_list_head->b_physical_path;
			dl->logical_path = box_list_head->logical_path;
			if (dlh == NULL) {
				dlh = dlt = dl;
			} else {
				dlt->next = dl;
				dl->prev = dlt;
				dlt = dl;
			}
		}
		box_list_head = box_list_head->box_next;
	}

done:
	return (dlh);
}
/*
 * Free a multipath list generated by l_get_multipath() or l_get_allses()
 *
 */
void
l_free_multipath(struct dlist *dlh)
{
	struct dlist *dl;

	while (dlh) {
		dl = dlh->next;
		free(dlh);
		dlh = dl;
	}
}
