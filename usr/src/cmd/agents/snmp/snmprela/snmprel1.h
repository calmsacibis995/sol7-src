/* Copyright 02/20/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)snmprelay_msg.h	1.2 96/02/20 Sun Microsystems"

/* HISTORY
 * 5-20-96		Jerry Yeung	merge with agent_msg.h
 */

#ifndef _SNMPRELAY_MSG_H_
#define _SNMPRELAY_MSG_H_


/***** TRACING MESSAGES *****/

#define MSG_SIGHUP			"signal SIGHUP(%d) received"

#define MSG_READING_CONFIG		"re-reading its configuration directory %s..."
#define MSG_CONFIG_READED		"...configuration re-read"


/***** PDU RELATED ERRORS *****/

#define ERR_MSG_PDU_RECEIVED		"error while receiving a pdu from %s: %s"
#define ERR_MSG_SNMP_ERROR		"SNMP error (%s, %lu) sent back to %s"
#define ERR_MSG_PDU_SEND_BACK		"error while sending a pdu back to %s: %s"
#define ERR_MSG_PDU_SEND_TO		"error while sending a pdu to %s: %s"


/***** SYSTEM ERRORS *****/

#define ERR_MSG_SOCKET			"socket() failed %s"
#define ERR_MSG_BIND			"bind() failed on UDP port %d %s"
#define ERR_MSG_SELECT			"select() failed %s"
#define ERR_MSG_FORK			"fork() failed %s"
#define ERR_MSG_FCLOSE			"fclose(%s) failed %s"
#define ERR_MSG_CHDIR			"chdir(%s) failed %s"
#define ERR_MSG_OPENDIR			"can't read the directory %s %s"
#define ERR_MSG_OPEN			"can't open config file %s %s"
#define ERR_MSG_FSTAT			"can't stat config file %s %s"
#define ERR_MSG_MMAP			"can't mmap config file %s %s"
#define ERR_MSG_MUNMAP			"munmap() failed %s"
#define ERR_MSG_CLOSE			"close() failed %s"
#define ERR_MSG_CLOSEDIR		"closedir(%s) failed %s"


/***** MISCELLANEOUS ERRORS *****/

#define ERR_MSG_ALLOC			"unable to allocate memory"

#define ERR_MSG_MANAGER_DUP             "the manager %s already exists"
#define ERR_MSG_COMMUNITY_DUP           "the community %s already exists"

#define ERR_MSG_MY_IP_ADDRESS		"unable to get my IP address: %s"

#define ERR_MSG_VARBIND_LIMIT		"unable to handle SNMP request with more than 32 variables"

#define ERR_MSG_UNKNOWN_FRAGMENT	"unknown PDU fragment received from agent %s (%s)"
#define ERR_MSG_AGENT_NOT_RESPONDING	"agent %s not responding"


#endif
