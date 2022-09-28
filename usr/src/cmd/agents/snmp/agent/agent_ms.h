/* Copyright 06/13/97 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)agent_msg.h	1.5 97/06/13 Sun Microsystems"

#ifndef _AGENT_MSG_H_
#define _AGENT_MSG_H_


/***** TRACING MESSAGES *****/

#define MSG_SIGHUP			"signal SIGHUP(%d) received"

#define MSG_READING_CONFIG		"re-reading its configuration file %s..."
#define MSG_CONFIG_READED		"...configuration re-read"


/***** SYSTEM ERRORS ****/

#define ERR_MSG_SOCKET			"socket() failed %s"
#define ERR_MSG_BIND			"bind() failed on UDP port %d %s"
#define ERR_MSG_SELECT			"select() failed %s"
#define ERR_MSG_FORK			"fork() failed %s"
#define ERR_MSG_FCLOSE			"fclose(%s) failed %s"
#define ERR_MSG_CHDIR			"chdir(%s) failed %s"
#define ERR_MSG_OPEN			"can't open config file %s %s"
#define ERR_MSG_FSTAT			"can't stat config file %s %s"
#define ERR_MSG_MMAP			"can't mmap config file %s %s"
#define ERR_MSG_MUNMAP			"munmap() failed %s"
#define ERR_MSG_CLOSE			"close() failed %s"


/***** PDU RELATED ERRORS *****/

#define ERR_MSG_PDU_RECEIVED		"error while receiving a pdu from %s: %s"
#define ERR_MSG_PDU_PROCESS		"unable to process a pdu from %s"
#define ERR_MSG_SNMP_ERROR		"SNMP error (%s, %lu) sent back to %s"
#define ERR_MSG_PDU_SEND		"error while sending a pdu back to %s: %s"


/***** MISCELLANEOUS *****/

#define ERR_MSG_ALLOC			"cannot allocate memory" 

#define ERR_MSG_MANAGER_DUP		"the manager %s already exists"
#define ERR_MSG_COMMUNITY_DUP		"the community %s already exists"

#define ERR_MSG_MY_IP_ADDRESS           "unable to get my IP address: %s"
 
#define ERR_MSG_VARBIND_LIMIT           "unable to handle SNMP request with more than 32 variables"
 
#define ERR_MSG_UNKNOWN_FRAGMENT        "unknown PDU fragment received from agent %s (%s)"
#define ERR_MSG_AGENT_NOT_RESPONDING    "agent %s not responding"
 

#endif
