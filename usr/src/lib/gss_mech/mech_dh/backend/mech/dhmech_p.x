/*
 * ident	"@(#)dhmech_prot.x	1.1	97/11/19 SMI"
 *
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * Diffie-Hellman GSS protocol descriptions
 */

#ifdef RPC_HDR
%#include <rpc/key_prot.h>
#endif

/* Token types */

enum token_type {
	DH_INIT_CNTX = 1,
	DH_ACCEPT_CNTX = 2,
	DH_MIC = 3,
	DH_WRAP = 4
};

const DH_MAX_CHECKSUM_SIZE = 128;
const DH_PROTO_VERSION = 0;
const DH_MAX_SESSION_KEYS = 64;

typedef opaque buffer_desc<>;
typedef buffer_desc *buffer_t;
typedef opaque signature<DH_MAX_CHECKSUM_SIZE>; /* Encrypted checksum */
typedef signature *signature_t;
typedef des_block key_set<DH_MAX_SESSION_KEYS>;
typedef key_set *key_set_t;
typedef unsigned int qop_t;

struct channel_binding_desc {
	unsigned initiator_addrtype;
	buffer_desc initiator_address;
	unsigned acceptor_addrtype;
	buffer_desc acceptor_address;
	buffer_desc application_data;
};
typedef channel_binding_desc *channel_binding_t;

struct context_desc {
	netnamestr remote;
	netnamestr local;
	unsigned flags;		/* Supported flag values from 
				 * gss_init_sec_context/gss_accept_sec_context
				 */
	unsigned expire;
	channel_binding_t channel;
};
typedef context_desc *context_t;

struct init_context_desc {
	context_desc	cntx;
	key_set keys;	/* Session keys encrypted 
			 * with the common key 
			 */
};
typedef init_context_desc *init_context_t;

struct accept_context_desc {
	context_desc cntx;
};
typedef accept_context_desc *accept_context_t;

struct mic_desc {
	qop_t qop;
	unsigned seqnum;
};
typedef mic_desc *mic_t;

struct wrap_desc {
	mic_desc mic;
	bool conf_flag;
	opaque body<>;		/* 
				 * If conf_flag, then body is an encrypted
				 * serialize opaque msg<>
				 */
};
typedef wrap_desc *wrap_t;

union token_body_desc switch (token_type type) {
	case DH_INIT_CNTX:
		init_context_desc init_context;
	case DH_ACCEPT_CNTX:
		accept_context_desc accept_context;
	case DH_MIC:
		mic_desc sign;
	case DH_WRAP:
		wrap_desc seal;
};
typedef token_body_desc *token_body_t;

struct token_desc {
	unsigned verno;
	token_body_desc body;
	signature verifier;	/* 
				 * Verifier is calculated over verno and body.
				 */
};
typedef token_desc *token_t;

/*
 * The token return from gss_init_sec_context will be as follows:
 *
 *	0x60	tag for APPLICATION 0, SEQUENCE  (constructed, definite length)
 * 	<length>  DER encoded
 *	0x06	tag for OID, the mech type.
 *	<mech type> DER encoded
 *	token_desc   XDR encoded
 */
