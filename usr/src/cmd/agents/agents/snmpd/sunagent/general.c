/* Copyright 1988 - 07/23/96 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#ifdef SVR4
#pragma ident  "@(#)general.c	2.14 96/07/23 Sun Microsystems"
#else
static char sccsid[] = "@(#)general.c	2.14 96/07/23 Sun Microsystems";
#endif
#endif

/*
** Sun considers its source code as an unpublished, proprietary trade 
** secret, and it is available only under strict license provisions.  
** This copyright notice is placed here only to protect Sun in the event
** the source is deemed a published work.  Disassembly, decompilation, 
** or other means of reducing the object code to human readable form is 
** prohibited by the license agreement under which this code is provided
** to the user or company in possession of this copy.
** 
** RESTRICTED RIGHTS LEGEND: Use, duplication, or disclosure by the 
** Government is subject to restrictions as set forth in subparagraph 
** (c)(1)(ii) of the Rights in Technical Data and Computer Software 
** clause at DFARS 52.227-7013 and in similar clauses in the FAR and 
** NASA FAR Supplement.
*/
/****************************************************************************
 *     Copyright (c) 1988, 1989  Epilogue Technology Corporation
 *     All rights reserved.
 *
 *     This is unpublished proprietary source code of Epilogue Technology
 *     Corporation.
 *
 *     The copyright notice above does not evidence any actual or intended
 *     publication of such source code.
 ****************************************************************************/

/* $Header	*/
/*
 * $Log:   E:/SNMPV2/AGENT/SUN/GENERAL.C_V  $
 * 
 *    Rev 2.0   31 Mar 1990 15:34:20
 * Initial revision.
 * 
*/

#include <stdio.h>
#include <nlist.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <netinet/if_ether.h>
#include <syslog.h>

#include <asn1.h>
#include <snmp.h>
#include <libfuncs.h>

#include "agent.h"
#include "snmpvars.h"
#include "general.h"

static char cantfind[] = "  Couldn't find kernel variable `%s' in kernel file `%s'\n";

/* This routine needs to get into caching in a big way. */
off_t find_loc(sym)
struct kernel_symbol *sym;
{
    int rcode;
    struct nlist syms[2];

    if (sym->flags & KO_FAILED)
      return 0;
    if (sym->flags & KO_VALID)
      return sym->offset;

    memset((char *)syms, 0, sizeof(syms));

    syms[0].n_name = sym->name;

    if (trace_level > 0) {
	PRNTF1("Looking up kernel variable `%s'\n", sym->name);
    }
    rcode = nlist(kernel_file, syms);
    if ((rcode == -1) || (syms[0].n_value == 0)) {
	TRC_PRT2(0, cantfind, sym->name, kernel_file);
	SYSLOG2(cantfind, sym->name, kernel_file);
	sym->flags |= KO_FAILED;
	return 0;
    }
    else {
	sym->flags |= KO_VALID;
	if (trace_level > 2) {
	    PRNTF2("  Offset is %8.8X(hex)  %d(decimal)\n",
		   syms[0].n_value, syms[0].n_value);
	}
	return sym->offset = syms[0].n_value;
    }
}


static int kmem = -2;
static char kmem_file[] = "/dev/kmem";
static char no_kmem[] = "Can not open kernel memory";

int read_int(loc)
	off_t loc; {
	int ret_value;

	if(kmem == -2)
	  {
	  kmem = open(kmem_file, O_RDWR);
	  if (kmem == -1)
	    {
	    PERROR(no_kmem);
	    SYSLOG0(no_kmem);
	    closelog();
	    exit(20);
	    }
          }

	if(kmem >= 0) {
		lseek(kmem, (long)loc, 0);
		read(kmem, &ret_value, sizeof(int));
		return ret_value;
		}
	else
		return 0;
	}

int read_bytes(loc, buf, len)
	off_t loc;
	char *buf;
	int len; {

	if(kmem == -2)
	  kmem = open(kmem_file, O_RDWR);
	  if (kmem == -1)
	    {
	    PERROR(no_kmem);
	    SYSLOG0(no_kmem);
	    closelog();
	    exit(21);
	    }

	if(kmem >= 0) {
		lseek(kmem, (long)loc, 0);
		len = read(kmem, buf, len);
		return len;
		}
	else
		return 0;
	}

int write_bytes(loc, buf, len)
	off_t loc;
	char *buf;
	int len; {

	if(kmem == -2)
	  kmem = open(kmem_file, O_RDWR);
	  if (kmem == -1)
	    {
	    PERROR(no_kmem);
	    SYSLOG0(no_kmem);
	    closelog();
	    exit(22);
	    }

	if(kmem >= 0) {
		lseek(kmem, (long)loc, 0);
		len = write(kmem, buf, len);
		return len;
		}
	else
		return 0;
	}

int write_int(loc, val)
	off_t loc;
	int val; {

	if(kmem == -2)
	  kmem = open(kmem_file, O_RDWR);
	  if (kmem == -1)
	    {
	    PERROR(no_kmem);
	    SYSLOG0(no_kmem);
	    closelog();
	    exit(22);
	    }

	if(kmem >= 0) {
	        int len;
		lseek(kmem, (long)loc, 0);
		len = write(kmem, &val, sizeof(int));
		return len;
		}
	else
		return 0;
	}
