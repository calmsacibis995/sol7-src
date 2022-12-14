/*
 * Copyright (c) 1994, by Sun Microsytems, Inc.
 */

#pragma ident	"@(#)close.c	1.21	98/01/21 SMI"

/*
 * Interface to close a tnfctl handle
 */

#include "tnfctl_int.h"
#include "kernel_int.h"
#include "dbg.h"

#include <stdlib.h>
#include <signal.h>
#include <errno.h>

/* for bug 1253419 - guard against multiple tracing */

extern mutex_t		_tnfctl_internalguard_lock;

/*
 * Close a tnfctl handle - close any open fds and free up any memory
 * that was allocated.
 */
tnfctl_errcode_t
tnfctl_close(tnfctl_handle_t *hdl, tnfctl_targ_op_t action)
{
	tnfctl_errcode_t	prexstat;
	prb_status_t	prbstat;
	prb_proc_ctl_t	*proc_p;
	tnfctl_probe_t	*probe_hdl, *tmp_hdl;

	if (hdl == NULL)
		return (TNFCTL_ERR_NONE);

	if (hdl->mode == KERNEL_MODE) {
		prexstat = _tnfctl_prbk_close(hdl);
		if (prexstat)
			return (prexstat);
	}

	if (hdl->mode == INTERNAL_MODE) {
		_tnfctl_internal_releaselock();
	} else if (hdl->mode != KERNEL_MODE) {
		_tnfctl_external_releaselock(hdl);
	}

	_tnfctl_free_objs_and_probes(hdl);

	/* free probe handles */
	probe_hdl = hdl->probe_handle_list_head;
	while (probe_hdl != NULL) {
		/* call the destructor function for client probe data */
		if (hdl->destroy_func)
			hdl->destroy_func(probe_hdl->client_registered_data);
		tmp_hdl = probe_hdl;
		probe_hdl = probe_hdl->next;
		free(tmp_hdl);
	}
	hdl->probe_handle_list_head = NULL;

	if (hdl->mode != DIRECT_MODE) {
		/* indirect, internal, or kernel mode */
		free(hdl);
		return (TNFCTL_ERR_NONE);
	}

	/* DIRECT_MODE */

	proc_p = hdl->proc_p;
	if (proc_p == NULL) {
		free(hdl);
		return (TNFCTL_ERR_NONE);
	}

	switch (action) {
	case TNFCTL_TARG_DEFAULT:
		break;
	case TNFCTL_TARG_KILL:
		prbstat = prb_proc_setklc(proc_p, B_TRUE);
		if (prbstat)
			return (_tnfctl_map_to_errcode(prbstat));
		prbstat = prb_proc_setrlc(proc_p, B_FALSE);
		if (prbstat)
			return (_tnfctl_map_to_errcode(prbstat));
		break;
	case TNFCTL_TARG_RESUME:
		prbstat = prb_proc_setklc(proc_p, B_FALSE);
		if (prbstat)
			return (_tnfctl_map_to_errcode(prbstat));
		prbstat = prb_proc_setrlc(proc_p, B_TRUE);
		if (prbstat)
			return (_tnfctl_map_to_errcode(prbstat));
		break;
	case TNFCTL_TARG_SUSPEND:
		prbstat = prb_proc_setklc(proc_p, B_FALSE);
		if (prbstat)
			return (_tnfctl_map_to_errcode(prbstat));
		prbstat = prb_proc_setrlc(proc_p, B_FALSE);
		if (prbstat)
			return (_tnfctl_map_to_errcode(prbstat));
		break;
	default:
		return (TNFCTL_ERR_BADARG);
	}
	prbstat = prb_proc_close(proc_p);
	free(hdl);
	return (_tnfctl_map_to_errcode(prbstat));
}

tnfctl_errcode_t
_tnfctl_internal_releaselock()
{
	mutex_lock(&_tnfctl_internalguard_lock);
	_tnfctl_internal_tracing_flag = 0;
	mutex_unlock(&_tnfctl_internalguard_lock);
	return (TNFCTL_ERR_NONE);
}

tnfctl_errcode_t
_tnfctl_external_releaselock(tnfctl_handle_t *hdl)
{
	tnfctl_errcode_t	prexstat;
	prb_status_t		prbstat;
	uintptr_t		targ_symbol_ptr;
	pid_t			pidzero = 0;

	prexstat = _tnfctl_sym_find(hdl, TNFCTL_EXTERNAL_TRACEDPID,
	    &targ_symbol_ptr);
	if (prexstat) {
	return (prexstat);
	}
	prbstat = hdl->p_write(hdl->proc_p, targ_symbol_ptr,
	&pidzero, sizeof (pid_t));
	if (prbstat) {
	return (_tnfctl_map_to_errcode(prbstat));
	}
	return (TNFCTL_ERR_NONE);
}    
