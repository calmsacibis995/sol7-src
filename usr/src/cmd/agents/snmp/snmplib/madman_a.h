/* Copyright 07/01/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)madman_api.h	1.3 96/07/01 Sun Microsystems"


#ifndef _MADMAN_API_H_
#define _MADMAN_API_H_

#include "snmp_api.h"


/***** GLOBAL CONSTANTS *****/


/* predefined request values */

#define SYSUPTIME_REQ			1

#define APPL_ENTRY_REQ			11
#define ASSOC_ENTRY_REQ			12

#define MTA_ENTRY_REQ			21
#define MTA_GROUP_ENTRY_REQ		22
#define MTA_GROUP_ASSOCIATION_ENTRY_REQ	23

#define DSA_OPS_ENTRY_REQ		31
#define DSA_ENTRIES_ENTRY_REQ		32
#define DSA_INT_ENTRY_REQ		33

#define X4MS_MTA_ENTRY_REQ		101
#define X4MS_USER_ENTRY_PART1_REQ	102
#define X4MS_USER_ENTRY_PART2_REQ	103
#define X4MS_USER_ASSOCIATION_ENTRY_REQ	104

#define X4GRP_ENTRY_REQ			201
#define X4GRP_MAPPING_ENTRY_REQ		202

#define X5DSA_REFERENCE_ENTRY_REQ	401


/* applStatus values */

#define APPL_UP			1
#define APPL_DOWN		2
#define APPL_HALTED		3
#define APPL_CONGESTED		4
#define APPL_RESTARTING		5


/* assocApplicationType values */

#define ASSOC_UA_INITIATOR	1
#define ASSOC_UA_RESPONDER	2
#define ASSOC_PEER_INITIATOR	3
#define ASSOC_PEER_RESPONDER	4


/* x5dsaReferenceType values */

#define REFERENCE_SUPERIOR			1
#define REFERENCE_CROSS				2
#define REFERENCE_SUBORDINATE			3
#define REFERENCE_NON_SPECIFIC_SUBORDINATE	4


/***** GLOBAL TYPES *****/

/**********/
/* MIB II */
/**********/

typedef long SysUpTime;


/************/
/* RFC 1565 */
/************/

typedef struct _ApplEntry {
	long	applIndex;
	char	*applName;
	char	*applDirectoryName;
	char	*applVersion;
	long	applUptime;
	long	applOperStatus;
	long	applLastChange;
	long	applInboundAssociations;
	long	applOutboundAssociations;
	long	applAccumulatedInboundAssociations;
	long	applAccumulatedOutboundAssociations;
	long	applLastInboundActivity;
	long	applLastOutboundActivity;
	long	applRejectedInboundAssociations;
	long	applFailedOutboundAssociations;
} ApplEntry;

typedef struct _AssocEntry {
	long	applIndex;
	long	assocIndex;
	char	*assocRemoteApplication;
	Oid	*assocApplicationProtocol;
	long	assocApplicationType;
	long	assocDuration;
} AssocEntry;


/************/
/* RFC 1566 */
/************/

typedef struct _MtaEntry {
	long	applIndex;
	long	mtaReceivedMessages;
	long	mtaStoredMessages;
	long	mtaTransmittedMessages;
	long	mtaReceivedVolume;
	long	mtaStoredVolume;
	long	mtaTransmittedVolume;
	long	mtaReceivedRecipients;
	long	mtaStoredRecipients;
	long	mtaTransmittedRecipients;
} MtaEntry;

typedef struct _MtaGroupEntry {
	long	applIndex;
	long	mtaGroupIndex;
	long	mtaGroupReceivedMessages;
	long	mtaGroupRejectedMessages;
	long	mtaGroupStoredMessages;
	long	mtaGroupTransmittedMessages;
	long	mtaGroupReceivedVolume;
	long	mtaGroupStoredVolume;
	long	mtaGroupTransmittedVolume;
	long	mtaGroupReceivedRecipients;
	long	mtaGroupStoredRecipients;
	long	mtaGroupTransmittedRecipients;
	long	mtaGroupOldestMessageStored;
	long	mtaGroupInboundAssociations;
	long	mtaGroupOutboundAssociations;
	long	mtaGroupAccumulatedInboundAssociations;
	long	mtaGroupAccumulatedOutboundAssociations;
	long	mtaGroupLastInboundActivity;
	long	mtaGroupLastOutboundActivity;
	long	mtaGroupRejectedInboundAssociations;
	long	mtaGroupFailedOutboundAssociations;
	char	*mtaGroupInboundRejectionReason;
	char	*mtaGroupOutboundConnectFailureReason;
	long	mtaGroupScheduledRetry;
	Oid	*mtaGroupMailProtocol;
	char	*mtaGroupName;
} MtaGroupEntry;

typedef struct _MtaGroupAssociationEntry {
	long	applIndex;
	long	mtaGroupIndex;
	long	mtaGroupAssociationIndex;
} MtaGroupAssociationEntry;


/************/
/* RFC 1567 */
/************/

typedef struct _DsaOpsEntry {
	long	applIndex;
	long	dsaAnonymousBinds;
	long	dsaUnauthBinds;
	long	dsaSimpleAuthBinds;
	long	dsaStrongAuthBinds;
	long	dsaBindSecurityErrors;
	long	dsaInOps;
	long	dsaReadOps;
	long	dsaCompareOps;
	long	dsaAddEntryOps;
	long	dsaRemoveEntryOps;
	long	dsaModifyEntryOps;
	long	dsaModifyRDNOps;
	long	dsaListOps;
	long	dsaSearchOps;
	long	dsaOneLevelSearchOps;
	long	dsaWholeTreeSearchOps;
	long	dsaReferrals;
	long	dsaChainings;
	long	dsaSecurityErrors;
	long	dsaErrors;
} DsaOpsEntry;

typedef struct _DsaEntriesEntry {
	long	applIndex;
	long	dsaMasterEntries;
	long	dsaCopyEntries;
	long	dsaCacheEntries;
	long	dsaCacheHits;
	long	dsaSlaveHits;
} DsaEntriesEntry;

typedef struct _DsaIntEntry {
	long	applIndex;
	long	dsaIntIndex;
	char	*dsaName;
	long	dsaTimeOfCreation;
	long	dsaTimeOfLastAttempt;
	long	dsaTimeOfLastSuccess;
	long	dsaFailuresSinceLastSuccess;
	long	dsaFailures;
	long	dsaSuccesses;
} DsaIntEntry;


/************/
/* X4MS MIB */
/************/

typedef struct _X4msMtaEntry {
	long	x4msMtaIndex;
	char	*x4msMtaName;
} X4msMtaEntry;


typedef struct _X4msUserTablePart1 {
	long	x4msUserIndex;
	long	x4msUserTotalMessages;
	long	x4msUserTotalVolume;
	long	x4msUserP3Associations;
	long	x4msUserP7Associations;
	long	x4msUserLastP7Association;
	long	x4msUserAuthentificationFailures;
	char	*x4msUserAuthentificationFailureReason;
	char	*x4msUserName;
} X4msUserEntryPart1;

typedef struct _X4msUserEntryPart2 {
	long	x4msUserIndex;
	long	x4msUserNewMessages;
	long	x4msUserNewVolume;
	long	x4msUserListedMessages;
	long	x4msUserListedVolume;
	long	x4msUserProcessedMessages;
	long	x4msUserProcessedVolume;
	long	x4msUserMessagesOlderThanWeek;
	long	x4msUserVolumeOlderThanWeek;
	long	x4msUserMessagesOlderThanMonth;
	long	x4msUserVolumeOlderThanMonth;
	long	x4msUserMessagesOlderThanYear;
	long	x4msUserVolumeOlderThanYear;
	long	x4msUserP3InboundAssociations;
	long	x4msUserP7InboundAssociations;
	long	x4msUserP3OutboundAssociations;
	long	x4msUserAccumulatedP3InboundAssociations;
	long	x4msUserAccumulatedP7InboundAssociations;
	long	x4msUserAccumulatedP3OutboundAssociations;
	long	x4msUserLastP3InboundActivity;
	long	x4msUserLastP7InboundActivity;
	long	x4msUserLastP3OutboundActivity;
	long	x4msUserRejectedP3InboundAssociations;
	long	x4msUserRejectedP7InboundAssociations;
	long	x4msUserFailedP3OutboundAssociations;
	char	*x4msUserP3InboundRejectionReason;
	char	*x4msUserP7InboundRejectionReason;
	char	*x4msUserP3OutboundConnectFailureReason;
	long	x4msUserMtaIndex;
	char	*x4msUserORName;
} X4msUserEntryPart2;

typedef struct _X4msUserAssociationEntry {
	long	x4msUserIndex;
	long	x4msUserAssociationIndex;
} X4msUserAssociationEntry;


/*************/
/* X4GRP MIB */
/*************/

typedef struct _X4grpEntry {
	long	x4grpIndex;
	char	*x4grpName;
} X4grpEntry;


typedef struct _X4grpMappingEntry {
	long	x4grpIndex;
	long	x4grpMappingMSIndex;
	long	x4grpMappingMTAIndex;
} X4grpMappingEntry;


/*************/
/* X5DSA MIB */
/*************/

typedef struct _X5dsaReferenceEntry {
	long	x5dsaReferenceIndex;
	long	x5dsaReferenceType;
	char	*x5dsaReferenceNamingContext;
	char	*x5dsaReferenceSubordinate;
	char	*x5dsaReferenceName;
} X5dsaReferenceEntry;


/***** GLOBAL VARIABLES *****/

/* SMTP */
extern Oid smtp_name;
extern char smtp_string[];

/* P1 */
extern Oid id_ac_mts_transfer_name;
extern char id_ac_mts_transfer_string[];

/* P3 */
extern Oid id_ac_mts_access_name;
extern Oid id_ac_mts_forced_access_name;
extern Oid id_ac_mts_reliable_access_name;
extern Oid id_ac_mts_forced_reliable_access_name;

/* P7 */
extern Oid id_ac_ms_access_name;
extern Oid id_ac_ms_reliable_access_name;


/***** GLOBAL FUNCTIONS *****/

/**********/
/* MIB II */
/**********/

/* SysUpTime */

int sysUpTime_send_request(SNMP_session *session, char *error_label);
SysUpTime *sysUpTime_process_response(SNMP_session *session,
	SNMP_pdu *response, char *error_label);
void sysUpTime_free(SysUpTime *sysUpTime);
void sysUpTime_print(SysUpTime *sysUpTime);


/************/
/* RFC 1565 */
/************/

/* ApplEntry */

int applEntry_send_request(SNMP_session *session,
	u_char request_type, long applIndex, char *error_label);
ApplEntry *applEntry_process_response(SNMP_session *session,
	SNMP_pdu *response, char *error_label);
void applEntry_free(ApplEntry *applEntry);
void applEntry_print(ApplEntry *applEntry);

/* AssocEntry */

int assocEntry_send_request(SNMP_session *session,
	u_char request_type, long applIndex, long assocIndex, char *error_label);
AssocEntry *assocEntry_process_response(SNMP_session *session,
	SNMP_pdu *response, char *error_label);
void assocEntry_free(AssocEntry *assocEntry);
void assocEntry_print(AssocEntry *assocEntry);

/* miscellaneous */

char *applOperStatus_string(long applStatus);
char *assocApplicationType_string(long applStatus);


/************/
/* RFC 1566 */
/************/

/* MtaEntry */

int mtaEntry_send_request(SNMP_session *session,
	u_char request_type, long applIndex, char *error_label);
MtaEntry *mtaEntry_process_response(SNMP_session *session,
	SNMP_pdu *response, char *error_label);
void mtaEntry_free(MtaEntry *mtaEntry);
void mtaEntry_print(MtaEntry *mtaEntry);

/* MtaGroupEntry */

int mtaGroupEntry_send_request(SNMP_session *session,
	u_char request_type, long applIndex, long mtaGroupIndex, char *error_label);
MtaGroupEntry *mtaGroupEntry_process_response(SNMP_session *session,
	SNMP_pdu *response, char *error_label);
void mtaGroupEntry_free(MtaGroupEntry *mtaGroupEntry);
void mtaGroupEntry_print(MtaGroupEntry *mtaGroupEntry);

/* MtaGroupAssociationEntry */

int mtaGroupAssociationEntry_send_request(SNMP_session *session,
	u_char request_type, long applIndex, long mtaGroupIndex,
	long mtaGroupAssociationIndex, char *error_label);
MtaGroupAssociationEntry *mtaGroupAssociationEntry_process_response(SNMP_session *session,
	SNMP_pdu *response, char *error_label);
void mtaGroupAssociationEntry_free(MtaGroupAssociationEntry *mtaGroupAssociationEntry);
void mtaGroupAssociationEntry_print(MtaGroupAssociationEntry *mtaGroupAssociationEntry);


/************/
/* RFC 1567 */
/************/

/* DsaOpsEntry */

int dsaOpsEntry_send_request(SNMP_session *session,
	u_char request_type, long applIndex, char *error_label);
DsaOpsEntry *dsaOpsEntry_process_response(SNMP_session *session,
	SNMP_pdu *response, char *error_label);
void dsaOpsEntry_free(DsaOpsEntry *dsaOpsEntry);
void dsaOpsEntry_print(DsaOpsEntry *dsaOpsEntry);

/* DsaEntriesEntry */

int dsaEntriesEntry_send_request(SNMP_session *session,
	u_char request_type, long applIndex, char *error_label);
DsaEntriesEntry *dsaEntriesEntry_process_response(SNMP_session *session,
	SNMP_pdu *response, char *error_label);
void dsaEntriesEntry_free(DsaEntriesEntry *dsaEntriesEntry);
void dsaEntriesEntry_print(DsaEntriesEntry *dsaEntriesEntry);

/* DsaIntEntry */

int dsaIntEntry_send_request(SNMP_session *session,
	u_char request_type, long applIndex, long dsaIntIndex, char *error_label);
DsaIntEntry *dsaIntEntry_process_response(SNMP_session *session,
	SNMP_pdu *response, char *error_label);
void dsaIntEntry_free(DsaIntEntry *dsaIntEntry);
void dsaIntEntry_print(DsaIntEntry *dsaIntEntry);


/************/
/* X4MS MIB */
/************/

/* X4msMtaEntry */

int x4msMtaEntry_send_request(SNMP_session *session,
	u_char request_type, long x4msMtaIndex, char *error_label);
X4msMtaEntry *x4msMtaEntry_process_response(SNMP_session *session,
	SNMP_pdu *response, char *error_label);
void x4msMtaEntry_free(X4msMtaEntry *x4msMtaEntry);
void x4msMtaEntry_print(X4msMtaEntry *x4msMtaEntry);

/* X4msUserEntryPart1 */

int x4msUserEntryPart1_send_request(SNMP_session *session,
	u_char request_type, long x4msUserIndex, char *error_label);
X4msUserEntryPart1 *x4msUserEntryPart1_process_response(SNMP_session *session,
	SNMP_pdu *response, char *error_label);
void x4msUserEntryPart1_free(X4msUserEntryPart1 *x4msUserEntryPart1);
void x4msUserEntryPart1_print(X4msUserEntryPart1 *x4msUserEntryPart1);


/* X4msUserEntryPart2 */

int x4msUserEntryPart2_send_request(SNMP_session *session,
	u_char request_type, long x4msUserIndex, char *error_label);
X4msUserEntryPart2 *x4msUserEntryPart2_process_response(SNMP_session *session,
	SNMP_pdu *response, char *error_label);
void x4msUserEntryPart2_free(X4msUserEntryPart2 *x4msUserEntryPart2);
void x4msUserEntryPart2_print(X4msUserEntryPart2 *x4msUserEntryPart2);


/* X4msUserAssociationEntry */

int x4msUserAssociationEntry_send_request(SNMP_session *session,
	u_char request_type, long x4msUserIndex, long x4msUserAssociationIndex, char *error_label);
X4msUserAssociationEntry *x4msUserAssociationEntry_process_response(SNMP_session *session,
	SNMP_pdu *response, char *error_label);
void x4msUserAssociationEntry_free(X4msUserAssociationEntry *x4msUserAssociationEntry);
void x4msUserAssociationEntry_print(X4msUserAssociationEntry *x4msUserAssociationEntry);


/*************/
/* X4GRP MIB */
/*************/

/* X4grpEntry */

int x4grpEntry_send_request(SNMP_session *session,
	u_char request_type, long x4grpIndex, char *error_label);
X4grpEntry *x4grpEntry_process_response(SNMP_session *session,
	SNMP_pdu *response, char *error_label);
void x4grpEntry_free(X4grpEntry *x4grpEntry);
void x4grpEntry_print(X4grpEntry *x4grpEntry);

/* X4grpMappingEntry */

int x4grpMappingEntry_send_request(SNMP_session *session,
	u_char request_type, long x4grpIndex, long x4grpMappingMSIndex,
	long x4grpMappingMTAIndex, char *error_label);
X4grpMappingEntry *x4grpMappingEntry_process_response(SNMP_session *session,
	SNMP_pdu *response, char *error_label);
void x4grpMappingEntry_free(X4grpMappingEntry *x4grpMappingEntry);
void x4grpMappingEntry_print(X4grpMappingEntry *x4grpMappingEntry);


/*************/
/* X5DSA MIB */
/*************/

/* X5dsaReferenceEntry */

int x5dsaReferenceEntry_send_request(SNMP_session *session,
	u_char request_type, long x5dsaReferenceIndex, char *error_label);
X5dsaReferenceEntry *x5dsaReferenceEntry_process_response(SNMP_session *session,
	SNMP_pdu *response, char *error_label);
void x5dsaReferenceEntry_free(X5dsaReferenceEntry *x5dsaReferenceEntry);
void x5dsaReferenceEntry_print(X5dsaReferenceEntry *x5dsaReferenceEntry);

/* miscellaneous */

char *x5dsaReferenceType_string(long x5dsaReferenceType);


/*****************/
/* miscellaneous */
/*****************/

char *predefined_request_string(int predefined_id);


#endif

