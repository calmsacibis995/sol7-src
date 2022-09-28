/*
 * Copyright (c) 1995, 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#pragma ident	"@(#)euc_info.c 1.2	96/12/20 SMI"

/*LINTLIBRARY*/

#include <sys/localedef.h>

/*
 * These two functions are project private functions of CSI project.
 * They should be used cautiously when dealing with CSIed code.
 */

int
_is_euc_fc(void)
{
	return (__lc_charmap->cm_fc_type == _FC_EUC);
}

int
_is_euc_pc(void)
{
	return (__lc_charmap->cm_pc_type == _PC_EUC);
}
