/* Copyright 06/13/97 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)snmp_msg.h	1.6 97/06/13 Sun Microsystems"


#ifndef _SNMP_MSG_H_
#define _SNMP_MSG_H_



#define ERR_MSG_ALLOC			"cannot allocate memory"


#define ERR_MSG_HOSTENT_BAD_IP_LENGTH		"length of IP address in the hostent structure is not 4: %d"
#define ERR_MSG_HOSTENT_MISSING_IP_ADDRESS	"no IP address in the hostent structure"

#define ERR_MSG_BAD_IP_ADDRESS		"%s is not a valid IP address"
#define ERR_MSG_BAD_HOSTNAME		"%s is not a valid hostname"

#define ERR_MSG_BAD_TRACE_LEVEL		"Bad trace level %d. Must be in (0..%d)"

#define ERR_MSG_TRAP_DEST_DUP		"the trap destinator %s already exists"

#define ERR_MSG_TIMEOUT			"timeout expired"
#define ERR_MSG_BAD_RESPONSE		"bad response"
#define ERR_MSG_BAD_VALUE		"bad value"


/***** SYSTEM ERROR MESSAGES *****/

#define ERR_MSG_FILE_CREATION		"cannot create file %s %s"
#define ERR_MSG_FILE_OPEN		"cannot open file %s %s"
#define ERR_MSG_UNAME			"uname() failed %s"
#define ERR_MSG_SOCKET			"socket() failed %s"
#define ERR_MSG_BIND			"bind() failed %s"
#define ERR_MSG_RECVFROM		"recvfrom() failed %s"
#define ERR_MSG_SENDTO			"sendto() failed %s"
#define ERR_MSG_SELECT			"select() failed %s"
#define ERR_MSG_SIGSET			"sigset(%d, %s) failed %s"


/***** HOST ERROR MESSAGE *****/

#define ERR_MSG_GETHOSTBYNAME		"gethostbyname(%s) failed %s"


/***** CODING/DECODING ERROR MESSAGES *****/

/* asn1.c */

#define ERR_MSG_NOT_LONG		"not long"
#define ERR_MSG_BAD_LENGTH		"bad length"
#define ERR_MSG_OVERFLOW		"overflow of message"
#define ERR_MSG_DONT_SUPPORT_LARGE_INT	"integers that large are not supported"
#define ERR_MSG_BUILD_LENGTH		"build_length"
#define ERR_MSG_SUBIDENTIFIER_TOO_LONG	"subidentifier too long"
#define ERR_MSG_DONT_SUPPORT_LARGE_STR	"strings that long are not supported"
#define ERR_MSG_DONT_SUPPORT_INDEF_LEN	"indefinite lengths are not supported"
#define ERR_MSG_DONT_SUPPORT_SUCH_LEN	"data lengths that long are not supported"
#define ERR_MSG_MALFORMED_NULL		"malformed NULL"
#define ERR_MSG_ASN_LEN_TOO_LONG	"asn length too long"
#define ERR_MSG_CANT_PROCESS_LONG_ID	"can't process ID >= 30"


/* pdu.c */


/***** SNMP API *****/

#define ERR_MSG_CAN_NOT_ABORT_SESSION	"Couldn't abort session: %s %s. Exiting\n"
#define ERR_MSG_RECEIVED_MANGLED_PACKET	"Received mangled SNMP packet: %s\n"


/***** MADMAN API *****/

#define ERR_MSG_ERROR_STATUS		"%s on the %dth variable"
#define ERR_MSG_MISSING_VARIABLES	"missing some variables"
#define ERR_MSG_BAD_VARIABLE_TYPE	"bad type (0x%x) for %s"


/***** LOG *****/

#define MSG_STARTED			"started\n"
#define LOG_MSG_STARTED			"*** started ***"

#define MSG_EXITING			"exiting\n"
#define LOG_MSG_EXITING			"*** exiting ***"


#endif

