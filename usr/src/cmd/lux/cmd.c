/*
 * Copyright 1997 Sun Microsystems, Inc. All rights reserved.
 */

#pragma ident   "@(#)cmd.c 1.3     98/01/14 SMI"

/*LINTLIBRARY*/

/*
 *  This module is part of the photon Command Line
 *  Interface program.
 *
 */

#include	<stdlib.h>
#include	<stdio.h>
#include	<assert.h>
#include	<sys/types.h>
#include	<memory.h>
#include	<unistd.h>
#include	<errno.h>
#include	<string.h>
#include	<libintl.h>	/* gettext */
#include	<sys/dkio.h>
#include	<sys/dklabel.h>
#include	<sys/vtoc.h>
#include	<sys/scsi/scsi.h>
#include	<nl_types.h>

#include	"luxdef.h"
#include	"state.h"

extern	char	*scsi_find_command_name(int cmd);
extern	char	*l_error_msg_ptr;
extern	char	*p_error_msg_ptr;	/* SSA lib error pointer */
extern	char	l_error_msg[];		/* Use only one error msg */
extern	void	scsi_printerr(struct uscsi_cmd *,
		struct scsi_extended_sense *, int, char *);
extern	void	p_dump(char *, u_char *, int, int);
extern	char	*p_decode_sense(u_char);
extern	nl_catd l_catd;

/*
 * Execute a command and determine the result.
 */
int
cmd(int file, struct uscsi_cmd *command, int flag)
{
int	status, i;

	/*
	 * Set function flags for driver.
	 *
	 * Set don't retry flags
	 * Set Automatic request sense enable
	 *
	 */
	command->uscsi_flags = USCSI_ISOLATE | USCSI_DIAGNOSE |
		USCSI_RQENABLE;
	command->uscsi_flags |= flag;

	/* print command for debug */
	if (getenv("_LUX_S_DEBUG") != NULL) {
		if ((command->uscsi_cdb == NULL) ||
			(flag & USCSI_RESET) ||
			(flag & USCSI_RESET_ALL)) {
			if (flag & USCSI_RESET) {
				(void) printf("  Issuing a SCSI Reset.\n");
			}
			if (flag & USCSI_RESET_ALL) {
				(void) printf("  Issuing a SCSI Reset All.\n");
			}

		} else {
			(void) printf("  Issuing the following "
				"SCSI command: %s\n",
			scsi_find_command_name(command->uscsi_cdb[0]));
			(void) printf("	fd=0x%x cdb=", file);
			for (i = 0; i < (int)command->uscsi_cdblen; i++) {
				(void) printf("%x ", *(command->uscsi_cdb + i));
			}
			(void) printf("\n\tlen=0x%x bufaddr=0x%x buflen=0x%x"
				" flags=0x%x\n",
			command->uscsi_cdblen,
			(int)command->uscsi_bufaddr,
			command->uscsi_buflen, command->uscsi_flags);

			if ((command->uscsi_buflen > 0) &&
				((flag & USCSI_READ) == 0)) {
				(void) p_dump("  Buffer data: ",
				(u_char *)command->uscsi_bufaddr,
				MIN(command->uscsi_buflen, 16), HEX_ASCII);
			}
		}
		fflush(stdout);
	}


	/*
	 * Default command timeout in case command left it 0
	 */
	if (command->uscsi_timeout == 0) {
		command->uscsi_timeout = 60;
	}
	/*	Issue command - finally */
	status = ioctl(file, USCSICMD, command);
	if (status == 0 && command->uscsi_status == 0) {
		if (getenv("_LUX_S_DEBUG") != NULL) {
			if ((command->uscsi_buflen > 0) &&
				(flag & USCSI_READ)) {
				(void) p_dump("\tData read:",
				(u_char *)command->uscsi_bufaddr,
				MIN(command->uscsi_buflen, 16), HEX_ASCII);
			}
		}
		return (status);
	}
	if ((status != 0) && (command->uscsi_status == 0)) {
		if (getenv("_LUX_S_DEBUG") != NULL) {
			(void) printf("Unexpected USCSICMD ioctl error: %s\n",
				strerror(errno));
		}
		return (status);
	}

	/*
	 * Just a SCSI error, create error message
	 */
	if ((command->uscsi_rqbuf != NULL) &&
	    (((char)command->uscsi_rqlen - (char)command->uscsi_rqresid) > 0)) {
		scsi_printerr(command,
			(struct scsi_extended_sense *)command->uscsi_rqbuf,
			(int)(command->uscsi_rqlen - command->uscsi_rqresid),
			l_error_msg);
		l_error_msg_ptr = (char *)&l_error_msg; /* set error ptr */
		p_error_msg_ptr = (char *)&l_error_msg; /* set SSA lib ptr */
	} else {
		/* Print sense byte information */

		sprintf(l_error_msg, MSGSTR(-1,
			"SCSI Error - Sense Byte: %s\n"),
			p_decode_sense((u_char)command->uscsi_status));
		l_error_msg_ptr = (char *)&l_error_msg; /* set error ptr */
		p_error_msg_ptr = (char *)&l_error_msg; /* set SSA lib ptr */
	}
	if (getenv("_LUX_S_DEBUG") != NULL) {
		(void) fprintf(stdout, "  %s\n", l_error_msg);
	}
	return (L_SCSI_ERROR | command->uscsi_status);
}
