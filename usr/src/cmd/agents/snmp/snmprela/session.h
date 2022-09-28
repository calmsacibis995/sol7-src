/* Copyright 07/01/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)session.h	1.3 96/07/01 Sun Microsystems"

/* HISTORY
 * 5-28-96      Jerry Yeung     Three phase set protocol(ThreePhase)
 */

#ifndef _SESSION_H_
#define _SESSION_H_

typedef enum _Phase { PHASE_1=1, PHASE_2, PHASE_3} Phase;


/***** GLOBAL CONSTANTS *****/

/* states of a request */
#define REQUEST_STARTED		1
#define REQUEST_COMPLETED	2


/***** GLOBAL TYPES *****/

typedef struct _Agent_List {
	struct _Agent_List	*next;
	struct _Agent		*agent;
} Agent_List;


typedef struct _Request {
	struct _Request *next_request;

	struct _Session *session;
	struct _Subtree *subtree;	/* associated subtree */
	Agent_List *visited_agent_list; /* list of the agents visited so far */

	u_long		request_id;

	SNMP_pdu	*pdu;		/* SNMP request */

	u_long		flags;		/* cf below */

	int		state;		/* STARTED or COMPLETED */
	SNMP_pdu	*response;	/* response of the agent to the pdu */

	struct timeval  time;		/* time when the pdu was sent */
	struct timeval  expire;		/* time when the Request will timeout */
} Request;


typedef struct _Session {
	struct _Session	*next_session;

	u_long		session_id;

	Address		address;	/* the address of the SNMP application */

	SNMP_pdu	*pdu;		/* the "original" SNMP request */

	int		n_variables;	/* number of variables in the	*/
					/* "original" SNMP request	*/

	u_long		o_flags;	/* cf below */
	u_long		i_flags;	/* cf below */

	struct _Request *first_request; /* the request list of the session */

} Session;

/* Three Phase */
typedef struct _Three_Phase {
        SNMP_pdu *origin_pdu;
        SNMP_pdu *cur_pdu;
	Phase	state; 
	SNMP_variable *variable;
	Session* session;
} Three_Phase;



/* explanation for the flags:				*/
/* --------------------------				*/
/*							*/
/* Each bit in a flags corresponds to a variable	*/
/* in the "original" SNMP request.			*/
/* For example, the o_flag 0x7 in the Session means	*/
/* that the "original" SNMP request contains 3		*/
/* variables, the flags 0x5 of the Request 0 means that */
/* this Request handles the 1st and the 3rd		*/
/* variable of the "original" SNMP request, the flags	*/
/* 0x2 of the Request 1 means that this Request		*/
/* handles the 2nd variable of the "original" SNMP	*/
/* request. When a Request is completed, its flags	*/
/* are ORed with the i_flags of its Session, so as soon */
/* as o_flags == i_flags, we known that all the Requests*/
/* are completed and we start to compute the response	*/
/* of the "original" SNMP request.			*/


/***** GLOBAL FUNCTIONS *****/

extern void trace_sessions();

/* session_list_delete() will delete the whole session list */
extern void session_list_delete();

extern void session_dispatch();
extern void session_read();

extern void session_select_info(struct timeval *tv);
extern void session_timeout();

extern int any_outstanding_session();

#endif



