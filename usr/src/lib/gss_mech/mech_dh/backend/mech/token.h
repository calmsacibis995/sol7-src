/*
 *	token.h
 *
 *	Copyright (c) 1997, by Sun Microsystems, Inc.
 *	All rights reserved.
 *
 */

#ifndef _TOKEN_H_
#define	_TOKEN_H_

#pragma ident	"@(#)token.h	1.1	97/11/19 SMI"

#ifdef __cplusplus
extern "C" {
#endif

#include "dh_gssapi.h"
#include "dhmech_prot.h"

OM_uint32
__make_ap_token(gss_buffer_t, gss_OID, token_t, key_set_t);

OM_uint32
__make_token(gss_buffer_t, gss_buffer_t, token_t, key_set_t);

OM_uint32
__get_ap_token(gss_buffer_t, gss_OID, token_t, signature_t);

OM_uint32
__get_token(gss_buffer_t, gss_buffer_t, token_t, key_set_t);

#ifdef __cplusplus
}
#endif

#endif /* _TOKEN_H_ */
