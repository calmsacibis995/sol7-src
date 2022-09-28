/*
 *	dh_gssapi.h
 *
 *	Copyright (c) 1997, by Sun Microsystems, Inc.
 *	All rights reserved.
 *
 */

#ifndef _DH_GSSAPI_H_
#define	_DH_GSSAPI_H_

#pragma ident	"@(#)dh_gssapi.h	1.1	97/11/19 SMI"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gssapi/gssapi.h>
#include <mechglueP.h>
#include <rpc/rpc.h>
#include <time.h>
#include "error.h"
#include "token.h"
#include "oid.h"
#include "crypto.h"

#define	New(T, n) ((T *)calloc(n, sizeof (T)))
#define	Free(p) free(p)

#define	DH_NO_SECRETKEY 1
#define	DH_NO_NETNAME 2
#define	DH_VALIDATE_FAILURE 3

#define	DH_MECH_QOP 0

typedef struct keyopts_desc {
	int (*key_encryptsessions)(const char *remotename,
	    des_block deskeys[], int no_keys);
	int (*key_decryptsessions)(const char *remotename,
	    des_block deskeys[], int no_keys);
	int (*key_gendeskeys)(des_block *deskeys, int no_keys);
	int (*key_secretkey_is_set)(void);
	char *(*get_principal)(void);
} keyopts_desc, *keyopts_t;

typedef char *dh_principal;

typedef struct dh_cred_id_desc {
	uid_t uid;
	gss_cred_usage_t usage;
	dh_principal  principal;    /* RPC netname */
	time_t expire;
} dh_cred_id_desc, *dh_cred_id_t;


typedef struct dh_context_desc {
	gss_OID mech;
	keyopts_t keyopts;
} dh_context_desc, *dh_context_t;


typedef struct dh_gss_context_desc {
	int established;
	int initiate;   /* 1 intiates, 0 accepts */
	dh_principal remote;
	dh_principal local;
	int export_level;
	int default_qop;
	/* Three session keys to support 3 DES */
	int no_keys;
	des_block *keys;   /* session keys */
	OM_uint32 flags;
	OM_uint32 seqno;  /* Last seqno seen */
	OM_uint32 next_seqno;  /* next seqno to send */
	time_t expire;
} dh_gss_context_desc, *dh_gss_context_t;


/* declarations of internal name mechanism functions */

gss_mechanism
__dh_generic_initialize(gss_mechanism, gss_OID_desc, keyopts_t);

OM_uint32
__dh_gss_acquire_cred(void *, OM_uint32*, gss_name_t, OM_uint32, gss_OID_set,
    gss_cred_usage_t, gss_cred_id_t *, gss_OID_set *, OM_uint32 *);

OM_uint32
__dh_gss_release_cred(void *, OM_uint32 *, gss_cred_id_t *);

OM_uint32
__dh_gss_init_sec_context(void *, OM_uint32 *, gss_cred_id_t, gss_ctx_id_t *,
    gss_name_t, gss_OID, OM_uint32, OM_uint32, gss_channel_bindings_t,
    gss_buffer_t, gss_OID *, gss_buffer_t, OM_uint32 *, OM_uint32 *);

OM_uint32
__dh_gss_accept_sec_context(void *, OM_uint32 *, gss_ctx_id_t *, gss_cred_id_t,
    gss_buffer_t, gss_channel_bindings_t, gss_name_t *, gss_OID *,
    gss_buffer_t, OM_uint32 *, OM_uint32 *, gss_cred_id_t *);

OM_uint32
__dh_gss_process_context_token(void *, OM_uint32 *,
    gss_ctx_id_t, gss_buffer_t);

OM_uint32
__dh_gss_delete_sec_context(void *, OM_uint32 *, gss_ctx_id_t *, gss_buffer_t);

OM_uint32
__dh_gss_context_time(void *, OM_uint32 *, gss_ctx_id_t, OM_uint32 *);

OM_uint32
__dh_gss_sign(void *, OM_uint32 *, gss_ctx_id_t,
    int, gss_buffer_t, gss_buffer_t);

OM_uint32
__dh_gss_verify(void *, OM_uint32 *, gss_ctx_id_t,
    gss_buffer_t, gss_buffer_t, int *);

OM_uint32
__dh_gss_seal(void *, OM_uint32 *, gss_ctx_id_t,
    int, int, gss_buffer_t, int *, gss_buffer_t);

OM_uint32
__dh_gss_unseal(void *, OM_uint32 *, gss_ctx_id_t,
    gss_buffer_t, gss_buffer_t, int *, int *);

OM_uint32
__dh_gss_display_status(void *, OM_uint32 *, OM_uint32,
    int, gss_OID, OM_uint32 *, gss_buffer_t);

OM_uint32
__dh_gss_indicate_mechs(void *, OM_uint32 *, gss_OID_set *);

OM_uint32
__dh_gss_compare_name(void *, OM_uint32 *, gss_name_t, gss_name_t, int *);

OM_uint32
__dh_gss_display_name(void *, OM_uint32 *,
    gss_name_t, gss_buffer_t, gss_OID *);

OM_uint32
__dh_gss_import_name(void *, OM_uint32 *, gss_buffer_t, gss_OID, gss_name_t *);

OM_uint32
__dh_gss_release_name(void *, OM_uint32 *, gss_name_t *);

OM_uint32
__dh_gss_inquire_cred(void *, OM_uint32 *, gss_cred_id_t, gss_name_t *,
    OM_uint32 *, gss_cred_usage_t *, gss_OID_set *);

OM_uint32
__dh_gss_inquire_context(void *, OM_uint32 *, gss_ctx_id_t, gss_name_t *,
    gss_name_t *, OM_uint32 *, gss_OID *, OM_uint32 *, int *, int *);

/* New V2 entry points */
OM_uint32
__dh_gss_get_mic(void *, OM_uint32 *, gss_ctx_id_t,
    gss_qop_t, gss_buffer_t, gss_buffer_t);

OM_uint32
__dh_gss_verify_mic(void *, OM_uint32 *, gss_ctx_id_t, gss_buffer_t,
    gss_buffer_t, gss_qop_t *);

OM_uint32
__dh_gss_wrap(void *, OM_uint32 *, gss_ctx_id_t, int, gss_qop_t,
    gss_buffer_t, int *, gss_buffer_t);

OM_uint32
__dh_gss_unwrap(void *, OM_uint32 *, gss_ctx_id_t, gss_buffer_t,
    gss_buffer_t, int *, gss_qop_t *);

OM_uint32
__dh_gss_wrap_size_limit(void *, OM_uint32 *, gss_ctx_id_t, int,
    gss_qop_t, OM_uint32, OM_uint32 *);

OM_uint32
__dh_gss_import_name_object(void *, OM_uint32 *,
    void *, gss_OID, gss_name_t *);

OM_uint32
__dh_gss_export_name_object(void *, OM_uint32 *, gss_name_t, gss_OID, void **);

OM_uint32
__dh_gss_add_cred(void *, OM_uint32 *, gss_cred_id_t, gss_name_t, gss_OID,
    gss_cred_usage_t, OM_uint32, OM_uint32, gss_cred_id_t *, gss_OID_set *,
    OM_uint32 *, OM_uint32 *);

OM_uint32
__dh_gss_inquire_cred_by_mech(void *, OM_uint32  *, gss_cred_id_t, gss_OID,
    gss_name_t *, OM_uint32 *, OM_uint32 *, gss_cred_usage_t *);

OM_uint32
__dh_gss_export_sec_context(void *, OM_uint32 *, gss_ctx_id_t *, gss_buffer_t);

OM_uint32
__dh_gss_import_sec_context(void *, OM_uint32 *, gss_buffer_t, gss_ctx_id_t *);

OM_uint32
__dh_gss_internal_release_oid(void *, OM_uint32 *, gss_OID *);

OM_uint32
__dh_gss_inquire_names_for_mech(void *, OM_uint32 *, gss_OID, gss_OID_set *);

/* Principal to uid mapping */
OM_uint32
__dh_pname_to_uid(void *ctx, OM_uint32 *minor,
    const gss_name_t pname, uid_t *uid);

OM_uint32
__dh_gss_export_name(void *ctx, OM_uint32 *minor,
    const gss_name_t input_name, gss_buffer_t exported_name);

OM_uint32
__dh_validate_context(dh_gss_context_t);

OM_uint32
__dh_validate_cred(dh_cred_id_t);

OM_uint32
__dh_validate_principal(dh_principal);

#ifdef __cplusplus
}
#endif

#endif /* _DH_GSSAPI_H_ */
