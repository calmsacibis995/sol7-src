/* Copyright 09/20/96 Sun Microsystems, Inc. All Rights Reserved.
 */
#pragma ident  "@(#)oid_ar_p.h	1.6 96/09/20 Sun Microsystems"


#ifndef _OID_AR_P_H_
#define _OID_AR_P_H_

static Subid reg_subagent_subids[] = { 1,3,6,1,4,1,42,2,15,8,1,0,0 };
static Oid reg_subagent_oid = {reg_subagent_subids, 13 };

static Subid agent_tbl_index_subids[] = { 1,3,6,1,4,1,42,2,15,9,0 };
static Oid agent_tbl_index_oid = { agent_tbl_index_subids, 11 };
static Subid tree_tbl_index_subids[] = { 1,3,6,1,4,1,42,2,15,13,0 };
static Oid tree_tbl_index_oid = { tree_tbl_index_subids, 11 };

static Subid reg_tree_ra_subids[] = { 1,3,6,1,4,1,42,2,15,12,1,3,0,0};
static Oid reg_tree_ra_oid = { reg_tree_ra_subids, 14};

static Subid ra_trap_port_subids[] = { 1,3,6,1,4,1,42,2,15,4,0};
static Oid ra_trap_port_oid = { ra_trap_port_subids, 11};

static Subid ra_check_point_subids[] = { 1,3,6,1,4,1,42,2,15,5,0};
static Oid ra_check_point_oid = { ra_check_point_subids, 11};

/* last three numbers are columar obj, agentid, table_id */
static Subid reg_shared_table_subids[] = { 1,3,6,1,4,1,42,2,15,10,1,0,0,0};
static Oid reg_shared_table_oid = { reg_shared_table_subids, 14};

static Subid snmp_subids[]={1,3,6,1,2,1,11};
static Oid snmp_oid={snmp_subids,7};
static Subid sun_subids[]={1,3,6,1,4,1,42};
static Oid sun_oid={sun_subids,7};

#endif
