/* Copyright 1988 - 10/02/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#ifdef SVR4
#pragma ident  "@(#)read_con.c	2.17 96/10/02 Sun Microsystems"
#else
static char sccsid[] = "@(#)read_con.c	2.17 96/10/02 Sun Microsystems";
#endif
#endif

/*
** Sun considers its source code as an unpublished, proprietary trade 
** secret, and it is available only under strict license provisions.  
** This copyright notice is placed here only to protect Sun in the event
** the source is deemed a published work.  Disassembly, decompilation, 
** or other means of reducing the object code to human readable form is 
** prohibited by the license agreement under which this code is provided
** to the user or company in possession of this copy.
** 
** RESTRICTED RIGHTS LEGEND: Use, duplication, or disclosure by the 
** Government is subject to restrictions as set forth in subparagraph 
** (c)(1)(ii) of the Rights in Technical Data and Computer Software 
** clause at DFARS 52.227-7013 and in similar clauses in the FAR and 
** NASA FAR Supplement.
*/
/****************************************************************************
 *     Copyright (c) 1988, 1989  Epilogue Technology Corporation
 *     All rights reserved.
 *
 *     This is unpublished proprietary source code of Epilogue Technology
 *     Corporation.
 *
 *     The copyright notice above does not evidence any actual or intended
 *     publication of such source code.
 ****************************************************************************/

/* $Header	*/
/*
 * $Log:   E:/SNMPV2/AGENT/SUN/READ_CON.C_V  $
 * 
 *    Rev 2.0   31 Mar 1990 15:34:14
 * Initial revision.
 * 
*/

#include <libfuncs.h>

#include <snmp.h>
#include <buildpkt.h>

#include <print.h>

#include <sys/types.h>
#include <stdio.h>
#include <ctype.h>
#include <fcntl.h>
#include <string.h>
#include <sys/time.h>

#include "snmpvars.h"

#include "agent.h"

#define then

extern int errno;

#if !defined(NO_PP)
static char *	to_l_case(char *);
#else	/* NO_PP */
static	void	set_trap_to();
static char *	to_l_case();
#endif	/* NO_PP */

/* Table of commands */
int set_conf_string(), set_conf_traps(), set_conf_managers();
int set_conf_word(), set_new_device();

struct cmd {
    char *name;
    int (*rtn)();
    char *arg1;
    int arg2;
} cmds[] = {
    { "sysdescr", set_conf_string, snmp_sysDescr, MAX_SYSDESCR },
    { "syscontact",  set_conf_string, snmp_sysContact, MAX_SYSCONTACT },
    { "syslocation", set_conf_string, snmp_sysLocation, MAX_SYSLOCATION },
    { "trap-community", set_conf_word, snmp_trap_community, SNMP_COMM_MAX },
    { "system-group-read-community", set_conf_word,
	 snmp_sysgrp_read_community, SNMP_COMM_MAX },
    { "system-group-write-community", set_conf_word,
	 snmp_sysgrp_write_community, SNMP_COMM_MAX },
    { "read-community", set_conf_word,
	 snmp_fullmib_read_community, SNMP_COMM_MAX },
    { "write-community", set_conf_word,
	 snmp_fullmib_write_community, SNMP_COMM_MAX },
    { "trap", set_conf_traps, 0, 0 },
    { "kernel-file", set_conf_word, kernel_file, MAX_KERN_FILE },
    { "managers", set_conf_managers, 0, 0 },
    { "newdevice", set_new_device, 0, 0 },
    { 0, 0, 0, 0 }
};

/**********************************************************************/
/*                                                                    */
/*  Read the configuration file                                       */
/*                                                                    */
/**********************************************************************/
int
read_config()
{
    FILE *	hostf;
    char	linebuff[256];
    struct cmd *cmd;
    
    TRC_PRT0(2, "Processing configuration file...\n");
    
    /* See if we can find the system in the snmp.hosts file */
    if ((hostf = fopen(config_file, "r")) == (FILE *)NULL)
      then {
	   PERROR("Can not open agent configuration file");
	   return -1;
           }
    
    for(;;)
      {
      char *ccp;
      int cmd_len;

    (void) fgets(linebuff, sizeof(linebuff), hostf);
    if (feof(hostf) || ferror(hostf))
      then {
           TRC_PRT0(2, "Hit EOF of config file\n");
	   break;
           }

	/* Weed out any comment text */
	if ((ccp = strchr(linebuff, '#')) != (char *)NULL)
	  then *ccp = '\0';
	  
	/* Zap the newline, if any */
	if ((ccp = strchr(linebuff, '\n')) != (char *)NULL)
	  then *ccp = '\0';
	  
	if ((linebuff[0] == '\0') || (linebuff[0] == '\n')) then continue;
	  
	/* Parse off the command name */
	cmd_len = strcspn(linebuff, " \t");
	ccp = linebuff + cmd_len + strspn(linebuff + cmd_len, " \t");

	/* Look up the command */
	for (cmd = cmds; cmd->name; cmd++) {
	    if ((strlen(cmd->name) == cmd_len) &&
		 (strnicmp(linebuff, cmd->name, cmd_len) == 0)) {
		(*cmd->rtn)(ccp, cmd);
		goto next;
	    }
	}

	PRNTF1("Unknown config command `%s'\n", linebuff);
    next:
	;
  }

    fclose(hostf);

    return 0;
}

int set_conf_word(string, cmd)
char *string;
struct cmd *cmd;
{
char *ccp;
/* Take only the first word */
if ((ccp = strchr(string, ' ')) != (char *)NULL)
   then *ccp = '\0';
if ((ccp = strchr(string, '\t')) != (char *)NULL)
   then *ccp = '\0';
strncpy(cmd->arg1, string, cmd->arg2);
return 1;
}

int set_conf_string(string, cmd)
char *string;
struct cmd *cmd;
{
char *ccp;
strncpy(cmd->arg1, string, cmd->arg2);
return 1;
}

/*ARGSUSED*/
int set_conf_traps(string, cmd)
char *string;
struct cmd *cmd;
{
    int nnames;
    int tlnum;
    int	thnum;
    char thost[MAX_TRAPS_TO][MAX_HOST_NAME_SZ];
	      
    /* The following scanf format is keyed to a value of	*/
    /* MAX_TRAPS_TO being 5.					*/
    /* It is also keyed to the value of MAX_HOST_NAME_SZ	*/
    nnames = sscanf(string, "%63s %63s %63s %63s %63s",
		    thost[0], thost[1], thost[2], thost[3], thost[4]);
    for (thnum = 0, tlnum = 0; thnum < nnames; thnum++) {
	if (name_to_ulong(thost[thnum], &(traplist[tlnum])) != -1)
	  tlnum++;
	else PRNTF1("Can not resolve trap receiver name \"%s\"\n", thost[thnum]);
    }
    trap_2_cnt = tlnum;
    return 1;
}


/*ARGSUSED*/
int set_conf_managers(string, cmd)
char *string;
struct cmd *cmd;
{
int nnames;
int mhnum;
char mhost[MAX_MGR_SCANF][MAX_HOST_NAME_SZ];
	      
/* The following scanf format is keyed to a value of	*/
/* MAX_MGR_SCANF being 5.					*/
/* It is also keyed to the value of MAX_HOST_NAME_SZ	*/
nnames = sscanf(string, "%63s %63s %63s %63s %63s",
		mhost[0], mhost[1], mhost[2], mhost[3], mhost[4]);

for (mhnum = 0; mhnum < nnames; mhnum++)
   {
   if (mgr_cnt >= MAX_MGR)
      then {
           PRNTF2("Manager name \"%s\" ignorred -- only %d names allowed\n",
		  mhost[mhnum], MAX_MGR);
	   }
      else {
           if (name_to_ulong(mhost[mhnum], &(mgr_list[mgr_cnt])) != -1)
	      then mgr_cnt++;
	      else PRNTF1("Can not resolve manager name \"%s\"\n", mhost[mhnum]);
	   }
  }
return 1;
}

/*ARGSUSED*/
int set_new_device(string, cmd)
char *string;
struct cmd *cmd;
{
char device_name[32];
int device_type;
long device_speed;

sscanf(string, "%d %l %s", &device_type, &device_speed, device_name);

if(new_device_pointer >= MAX_NEW_DEVICE) return 1;
strcpy(new_devices[new_device_pointer].name, device_name);
new_devices[new_device_pointer].type = device_type;
new_devices[new_device_pointer].speed = device_speed;
new_device_pointer++;
return 1;
}

