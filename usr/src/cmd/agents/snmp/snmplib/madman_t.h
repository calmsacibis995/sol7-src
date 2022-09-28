/* Copyright 07/01/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)madman_trap.h	1.3 96/07/01 Sun Microsystems"

#ifndef _MADMAN_TRAP_H_
#define _MADMAN_TRAP_H_

/***** GLOBAL CONSTANTS *****/


/* specific trap numbers */

#define TRAP_APPL_ALARM			1
#define TRAP_MTA_ALARM			2
#define TRAP_MSG_ALARM			3


/* alarm severity */

#define SEVERITY_LOW			1
#define SEVERITY_MEDIUM			2
#define SEVERITY_HIGH			3


/***** GLOBAL FUNCTIONS ******/

extern void send_trap_appl_status_changed(int applIndex, char *applName, int applOperStatus);
extern void send_trap_appl_alarm(int applIndex, char *applName, int alarmId, int alarmSeverity, char *alarmDescr);

extern char *alarmSeverity_string(int severity);


#endif
