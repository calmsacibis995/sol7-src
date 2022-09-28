/*
 * Copyright (c) 1997, by Sun Microsystems, Inc.
 * All rights reserved.
 *
 * ident  "@(#)dmech.c 1.16     98/02/02 SMI"
 *
 * A module that implement dummy security mechanism.
 * It's mainly used to test GSS-API application. Multiple tokens
 * exchanged during security context establishment can be
 * specified through dummy_mech.conf located in /etc.
 *
 */

char _depends_on[] = "misc/kgssapi";

#include <sys/types.h>
#include <sys/modctl.h>
#include <sys/errno.h>
#include <gssapiP_dummy.h>
#include <gssapi_err_generic.h>
#include <mechglueP.h>
#include <gssapi/kgssapi_defs.h>
#include <sys/debug.h>

/* private routines for dummy_mechanism */
static gss_buffer_desc make_dummy_token_msg(void *data, int datalen);

static int der_length_size(int);

static void der_write_length(unsigned char **, int);
static int der_read_length(unsigned char **, int *);
static int g_token_size(gss_OID mech, unsigned int body_size);
static void g_make_token_header(gss_OID mech, int body_size,
				unsigned char **buf, int tok_type);
static int g_verify_token_header(gss_OID mech, int *body_size,
				unsigned char **buf_in, int tok_type,
				int toksize);

/* private global variables */
static int dummy_token_nums;

/*
 * This OID:
 * { iso(1) org(3) internet(6) dod(1) private(4) enterprises(1) sun(42)
 * products(2) gssapi(26) mechtypes(1) dummy(2) }
 */

static struct gss_config dummy_mechanism =
	{{10, "\053\006\001\004\001\052\002\032\001\002"},
	NULL,	/* context */
	NULL,	/* next */
	TRUE,	/* uses_kmod */
	dummy_gss_delete_sec_context,
	dummy_gss_import_sec_context,
	dummy_gss_sign,
	dummy_gss_verify
};

static gss_mechanism
	gss_mech_initialize()
{
	dprintf("Entering gss_mech_initialize\n");

	if (dummy_token_nums == 0)
		dummy_token_nums = 1;

	dprintf("Leaving gss_mech_initialize\n");
	return (&dummy_mechanism);
}


/*
 * Module linkage information for the kernel.
 */
extern struct mod_ops mod_miscops;

static struct modlmisc modlmisc = {
	&mod_miscops, "in-kernel dummy GSS mechanism"
};

static struct modlinkage modlinkage = {
	MODREV_1,
	(void *)&modlmisc,
	NULL
};

static int dummy_fini_code = EBUSY;

_init()
{
	int retval;
	gss_mechanism mech, tmp;

	if ((retval = mod_install(&modlinkage)) != 0)
		return (retval);

	mech = gss_mech_initialize();

	mutex_enter(&__kgss_mech_lock);
	tmp = __kgss_get_mechanism(&mech->mech_type);
	if (tmp != NULL) {
		GSSLOG0(8,
			"dummy GSS mechanism: mechanism already in table.\n");
		if (tmp->uses_kmod == TRUE) {
			GSSLOG0(8, "dummy GSS mechanism: mechanism table"
				" supports kernel operations!\n");
		}
		/*
		 * keep us loaded, but let us be unloadable. This
		 * will give the developer time to trouble shoot
		 */
		dummy_fini_code = 0;
	} else {
		__kgss_add_mechanism(mech);
		ASSERT(__kgss_get_mechanism(&mech->mech_type) == mech);
	}
	mutex_exit(&__kgss_mech_lock);

	return (0);
}

_fini()
{
	int ret = dummy_fini_code;

	if (ret == 0) {
		ret = (mod_remove(&modlinkage));
	}
	return (ret);
}

_info(modinfop)
	struct modinfo *modinfop;
{
	return (mod_info(&modlinkage, modinfop));
}


/*ARGSUSED*/
static OM_uint32
dummy_gss_sign(ctx, minor_status, context_handle,
		qop_req, message_buffer, message_token,
		gssd_ctx_verifier)
	void *ctx;
	OM_uint32 *minor_status;
	gss_ctx_id_t context_handle;
	int qop_req;
	gss_buffer_t message_buffer;
	gss_buffer_t message_token;
	OM_uint32 gssd_ctx_verifier;
{
	char token_string[] = "dummy_gss_sign";

	dprintf("Entering gss_sign\n");

	if (context_handle == GSS_C_NO_CONTEXT)
		return (GSS_S_NO_CONTEXT);

	*message_token = make_dummy_token_msg(
				token_string, strlen(token_string));

	dprintf("Leaving gss_sign\n");
	return (GSS_S_COMPLETE);
}

/*ARGSUSED*/
static OM_uint32
	dummy_gss_verify(ctx, minor_status, context_handle,
		message_buffer, token_buffer, qop_state,
		gssd_ctx_verifier)
	void *ctx;
	OM_uint32 *minor_status;
	gss_ctx_id_t context_handle;
	gss_buffer_t message_buffer;
	gss_buffer_t token_buffer;
	int *qop_state;
	OM_uint32 gssd_ctx_verifier;
{
	unsigned char *ptr;
	int bodysize;
	int err;

	dprintf("Entering gss_verify\n");

	if (context_handle == GSS_C_NO_CONTEXT)
		return (GSS_S_NO_CONTEXT);

	/* Check for defective input token. */
	ptr = (unsigned char *) token_buffer->value;
	if (err = g_verify_token_header((gss_OID)gss_mech_dummy, &bodysize,
					&ptr, 0,
					token_buffer->length)) {
		*minor_status = err;
		return (GSS_S_DEFECTIVE_TOKEN);
	}

	*qop_state = GSS_C_QOP_DEFAULT;

	dprintf("Leaving gss_verify\n");
	return (GSS_S_COMPLETE);
}


/*ARGSUSED*/
OM_uint32
	dummy_gss_import_sec_context(ct, minor_status, interprocess_token,
					context_handle)
void *ct;
OM_uint32 *minor_status;
gss_buffer_t interprocess_token;
gss_ctx_id_t *context_handle;
{
	/* Assume that we got ctx from the interprocess token. */
	dummy_gss_ctx_id_t ctx;

	dprintf("Entering import_sec_context\n");

	ctx = (dummy_gss_ctx_id_t)MALLOC(sizeof (dummy_gss_ctx_id_rec));
	ctx->token_number = 0;
	ctx->established = 1;

	*context_handle = ctx;

	dprintf("Leaving import_sec_context\n");
	return (GSS_S_COMPLETE);
}

/*ARGSUSED*/
static OM_uint32
dummy_gss_delete_sec_context(ct, minor_status,
			context_handle, output_token,
			gssd_ctx_verifier)
void *ct;
OM_uint32 *minor_status;
gss_ctx_id_t *context_handle;
gss_buffer_t output_token;
OM_uint32 gssd_ctx_verifier;
{
	dummy_gss_ctx_id_t ctx;

	dprintf("Entering delete_sec_context\n");

	/* Make the length to 0, so the output token is not sent to peer */
	if (output_token) {
		output_token->length = 0;
		output_token->value = NULL;
	}

	if (*context_handle == GSS_C_NO_CONTEXT) {
		*minor_status = 0;
		return (GSS_S_COMPLETE);
	}

	ctx = *context_handle;
	FREE(ctx, sizeof (dummy_gss_ctx_id_rec));
	*context_handle = GSS_C_NO_CONTEXT;

	dprintf("Leaving delete_sec_context\n");
	return (GSS_S_COMPLETE);
}

/*ARGSUSED*/
OM_uint32
dummy_gss_wrap_size_limit(ct, minor_status, context_handle, conf_req_flag,
				qop_req, req_output_size, max_input_size)
	void		*ct;
	OM_uint32	*minor_status;
	gss_ctx_id_t	context_handle;
	int		conf_req_flag;
	gss_qop_t	qop_req;
	OM_uint32	req_output_size;
	OM_uint32	*max_input_size;
{
	dprintf("Entering wrap_size_limit\n");
	*max_input_size = req_output_size;
	dprintf("Leaving wrap_size_limit\n");
	return (GSS_S_COMPLETE);
}

static int
der_length_size(int length)
{
	if (length < (1<<7))
		return (1);
	else if (length < (1<<8))
		return (2);
	else if (length < (1<<16))
		return (3);
	else if (length < (1<<24))
		return (4);
	else
		return (5);
}

static void
der_write_length(unsigned char ** buf, int length)
{
	if (length < (1<<7)) {
		*(*buf)++ = (unsigned char) length;
	} else {
		*(*buf)++ = (unsigned char) (der_length_size(length)+127);
		if (length >= (1<<24))
			*(*buf)++ = (unsigned char) (length>>24);
		if (length >= (1<<16))
			*(*buf)++ = (unsigned char) ((length>>16)&0xff);
		if (length >= (1<<8))
			*(*buf)++ = (unsigned char) ((length>>8)&0xff);
		*(*buf)++ = (unsigned char) (length&0xff);
	}
}

static int
der_read_length(buf, bufsize)
unsigned char **buf;
int *bufsize;
{
	unsigned char sf;
	int ret;

	if (*bufsize < 1)
		return (-1);
	sf = *(*buf)++;
	(*bufsize)--;
	if (sf & 0x80) {
		if ((sf &= 0x7f) > ((*bufsize)-1))
			return (-1);
		if (sf > DUMMY_SIZE_OF_INT)
			return (-1);
		ret = 0;
		for (; sf; sf--) {
			ret = (ret<<8) + (*(*buf)++);
			(*bufsize)--;
		}
	} else {
		ret = sf;
	}

	return (ret);
}

static int
g_token_size(mech, body_size)
	gss_OID mech;
	unsigned int body_size;
{
	/* set body_size to sequence contents size */
	body_size += 4 + (int) mech->length;	/* NEED overflow check */
	return (1 + der_length_size(body_size) + body_size);
}

static void
g_make_token_header(mech, body_size, buf, tok_type)
	gss_OID mech;
	int body_size;
	unsigned char **buf;
	int tok_type;
{
	*(*buf)++ = 0x60;
	der_write_length(buf, 4 + mech->length + body_size);
	*(*buf)++ = 0x06;
	*(*buf)++ = (unsigned char) mech->length;
	TWRITE_STR(*buf, mech->elements, ((int) mech->length));
	*(*buf)++ = (unsigned char) ((tok_type>>8)&0xff);
	*(*buf)++ = (unsigned char) (tok_type&0xff);
}

static int
g_verify_token_header(mech, body_size, buf_in, tok_type, toksize)
	gss_OID mech;
	int *body_size;
	unsigned char **buf_in;
	int tok_type;
	int toksize;
{
	unsigned char *buf = *buf_in;
	int seqsize;
	gss_OID_desc toid;
	int ret = 0;

	if ((toksize -= 1) < 0)
		return (G_BAD_TOK_HEADER);
	if (*buf++ != 0x60)
		return (G_BAD_TOK_HEADER);

	if ((seqsize = der_read_length(&buf, &toksize)) < 0)
		return (G_BAD_TOK_HEADER);

	if (seqsize != toksize)
		return (G_BAD_TOK_HEADER);

	if ((toksize -= 1) < 0)
		return (G_BAD_TOK_HEADER);
	if (*buf++ != 0x06)
		return (G_BAD_TOK_HEADER);

	if ((toksize -= 1) < 0)
		return (G_BAD_TOK_HEADER);
	toid.length = *buf++;

	if ((toksize -= toid.length) < 0)
		return (G_BAD_TOK_HEADER);
	toid.elements = buf;
	buf += toid.length;

	if (! g_OID_equal(&toid, mech))
		ret = G_WRONG_MECH;

	/*
	 * G_WRONG_MECH is not returned immediately because it's more important
	 * to return G_BAD_TOK_HEADER if the token header is in fact bad
	 */

	if ((toksize -= 2) < 0)
		return (G_BAD_TOK_HEADER);

	if ((*buf++ != ((tok_type>>8)&0xff)) ||
	    (*buf++ != (tok_type&0xff)))
		return (G_BAD_TOK_HEADER);

	if (!ret) {
		*buf_in = buf;
		*body_size = toksize;
	}

	return (ret);
}

static gss_buffer_desc
make_dummy_token_msg(void *data, int dataLen)
{
	gss_buffer_desc buffer;
	int tlen;
	unsigned char *t;
	unsigned char *ptr;

	if (data == NULL) {
		buffer.length = 0;
		buffer.value = NULL;
		return (buffer);
	}

	tlen = g_token_size((gss_OID)gss_mech_dummy, dataLen);
	t = (unsigned char *) MALLOC(tlen);
	ptr = t;

	g_make_token_header((gss_OID)gss_mech_dummy, dataLen, &ptr, 0);
	memcpy(ptr, data, dataLen);

	buffer.length = tlen;
	buffer.value = (void *) t;
	return (buffer);
}
