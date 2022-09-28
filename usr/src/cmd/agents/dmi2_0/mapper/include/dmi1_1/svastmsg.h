/* Copyright 08/05/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)svastmsg_default.h	1.1 96/08/05 Sun Microsystems"

/***********************************************************************     */
/*                                                                           */
/*        SVASTMSG_DEFAULT.H -- Default messages for                         */
/*                      SystemView Agent scripts.                            */
/*                                                                           */
/*                                                                           */
/*        Author:   David W. Hayslett                                        */
/*                                                                           */
/***********************************************************************     */
/* $set SVASTART */
#define SVA_START_D         "Starting SVA Service Layer"
#define SVASL_RUNNING_D     "SVA Service Layer is already running, pid %s"
#define SNMPD_START_D       "Starting SNMPD"
#define SNMPD_RUNNING_D     "SNMPD is already running, pid %s"
#define DPID_START_D        "Starting DPID2"
#define DPID_RUNNING_D      "DPID2 is already running, pid %s"
#define MIB2_START_D        "Starting MIB_2"
#define MIB2_RUNNING_D      "MIB_2 is already running, pid %s"
#define DMISA_START_D       "Starting SVA SNMP Mapper"
#define DMISA_RUNNING_D     "SVA SNMP Mapper is already running, pid %s"
#define SNMPD_DEFAULT_D     "Using default SNMP configuration"
#define SNMPD_NATIVE_DPI_D  "Using SNMP with native DPI configuration"
/* $set SVASTOP */
#define DMISA_STOP_D        "Stopping SVA SNMP Mapper."
#define DMISA_NOT_RUNNING_D "SVA SNMP Mapper is not running"
#define DMISA_TO_STOP_D     "SVA SNMP Mapper process %s will be stopped"
#define SVASL_STOP_D        "Stopping SVA Service Layer."
#define SVASL_NOT_RUNNING_D "SVA Service Layer is not running"
#define SVASL_TO_STOP_D     "SVA Service Layer process %s will be stopped"
/* $set DMIXSTART */
#define BROWSER_START_D     "Starting DMI Browser"
/* $set DMISA_CONFIG */
#define SNMPD_PEERS_D       "Your old /etc/snmpd.peers file has been saved as /etc/snmpd.peers.sva"
#define SNMPD_CONF_D        "Your old /etc/snmpd.conf file has been saved as /etc/snmpd.conf.sva"
