/* Copyright 1988 - 06/20/97 Sun Microsystems, Inc. All Rights Reserved.
 */

#if !defined(lint) && !defined(NOID)
#ifdef SVR4
#pragma ident  "@(#)general-svr4.c	1.16 97/06/20 Sun Microsystems"
#else
static char sccsid[] = "@(#)general-svr4.c	1.16 97/06/20 Sun Microsystems";
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
#include <inet/common.h>
#include <inet/mib2.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/tihdr.h>
#include <sys/tiuser.h>
#include <sys/timod.h>
#include <kstat.h>

#include <asn1.h>
#include <snmp.h>
#include <libfuncs.h>

#include "agent.h"
#include "snmpvars.h"
#include "general.h"
#ifndef T_CURRENT
#define T_CURRENT   MI_T_CURRENT
#endif

kstat_ctl_t	*kc = NULL;
static int	mibfd;

int sys_nerr;
extern char *sys_errlist[];
int errno;
 
static char *
sperror()
{
  if (errno <= sys_nerr)
      return sys_errlist[errno];
  else
      return "Unknown error";
}


kid_t
safe_kstat_read(kstat_ctl_t *kc, kstat_t *ksp, void *data)
{
    kid_t kstat_chain_id = kstat_read(kc, ksp, data);
    return (kstat_chain_id);
}



ulong_t
kstat_named_value(kstat_t *ksp, char *name)
{
    kstat_named_t *knp;
 
    if (ksp == NULL)
        return (0);
 
    knp = kstat_data_lookup(ksp, name);
    if (knp != NULL)
        return (knp->value.ul);
    else
        return (0);
}

static int mibfd = -2;

off_t mibopen()
{
 
	if (mibfd == -2 ) {
		mibfd = stream_open("/dev/ip", 2);
			if (mibfd == -1) {
			perror("ip open");
			stream_close(mibfd);
			return 0;
		}
		/* must be above ip, below tcp */
		if (stream_ioctl(mibfd, I_PUSH, "arp") == -1) { 
			perror("arp I_PUSH");
			stream_close(mibfd);
			return 0;
		}
		if (stream_ioctl(mibfd, I_PUSH, "tcp") == -1) {
			perror("tcp I_PUSH");
			stream_close(mibfd);
			return 0;
		}
		if (stream_ioctl(mibfd, I_PUSH, "udp") == -1) {
			perror("udp I_PUSH");
			stream_close(mibfd);
			return 0;
		}
	}
	return mibfd;
}

off_t mibclose()
{

                if (stream_ioctl(mibfd, I_POP, 0) == -1) {
                        perror("udp I_PUSH");
                        stream_close(mibfd);
                        return 0;
                }
                if (stream_ioctl(mibfd, I_POP, 0) == -1) {
                        perror("tcp I_PUSH");
                        stream_close(mibfd);
                        return 0;
                }
                if (stream_ioctl(mibfd, I_POP, 0) == -1) {
                        perror("arp I_PUSH");
                        stream_close(mibfd);
                        return 0;
                }
                close(mibfd);
		mibfd=-2;
		return 1;
}

/*
 * free the data associated with mib list
 */
void mibfree (mib_item_t *first)
{
    mib_item_t      *last;
    while (first) {
        last = first;
        first = first->next_item;
	free(last->valp);
        free(last);
    }
}


mib_item_t *
mibget (mibvalue)
int mibvalue;
{
    char            buf[512];
	char			msg[100];
    int         flags;
    int         i, j, getcode;
    struct strbuf       ctlbuf, databuf;
    struct T_optmgmt_req    * tor = (struct T_optmgmt_req *)buf;
    struct T_optmgmt_ack    * toa = (struct T_optmgmt_ack *)buf;
    struct T_error_ack  * tea = (struct T_error_ack *)buf;
    struct opthdr       * req;
    mib_item_t      * first_item = nilp(mib_item_t);
    mib_item_t      * last_item  = nilp(mib_item_t);
    mib_item_t      * temp;
 
    tor->PRIM_type = T_OPTMGMT_REQ;
    tor->OPT_offset = sizeof(struct T_optmgmt_req);
    tor->OPT_length = sizeof(struct opthdr);
    tor->MGMT_flags = T_CURRENT;
    req = (struct opthdr *)&tor[1];
    req->level = mibvalue;       /* any MIB2_xxx value ok here */
    req->name  = 0;
    req->len   = 0;
 
    ctlbuf.buf = buf;
    ctlbuf.len = tor->OPT_length + tor->OPT_offset;
    flags = 0;
    if (putmsg(mibfd, &ctlbuf, nilp(struct strbuf), flags) == -1) {
        sprintf(msg, "%s\n", "mibget: putmsg(ctl) failed");
        goto error_exit;
    }
    /*
     * each reply consists of a ctl part for one fixed structure
     * or table, as defined in mib2.h.  The format is a T_OPTMGMT_ACK,
     * containing an opthdr structure.  level/name identify the entry,
     * len is the size of the data part of the message.
     */
    req = (struct opthdr *)&toa[1];
    ctlbuf.maxlen = sizeof(buf);
    for (j=1; ; j++) {
        flags = 0;
        getcode = getmsg(mibfd, &ctlbuf, nilp(struct strbuf), &flags);
        if (getcode == -1) {
            sprintf(msg, "%s\n", "mibget: getmsg(ctl) failed");
            goto error_exit;
        }
        if (getcode == 0
        && ctlbuf.len >= sizeof(struct T_optmgmt_ack)
        && toa->PRIM_type == T_OPTMGMT_ACK
        && toa->MGMT_flags == T_SUCCESS
        && req->len == 0) {
            return first_item;
        }
 
        if (ctlbuf.len >= sizeof(struct T_error_ack)
        && tea->PRIM_type == T_ERROR_ACK) {
            sprintf(msg, "mibget %d gives T_ERROR_ACK: TLI_error = 0x%x, UNIX_error = 0x%x\n",
                j, getcode, tea->TLI_error, tea->UNIX_error);
            goto error_exit;
        }
           
        if (getcode != MOREDATA
        || ctlbuf.len < sizeof(struct T_optmgmt_ack)
        || toa->PRIM_type != T_OPTMGMT_ACK
        || toa->MGMT_flags != T_SUCCESS) {
            sprintf(msg,
                "mibget getmsg(ctl) %d returned %d, ctlbuf.len = %d, PRIM_type =%d\n",
                 j, getcode, ctlbuf.len, toa->PRIM_type);
            goto error_exit;
 
        }
 
        temp = (mib_item_t *)malloc(sizeof(mib_item_t));
        if (!temp) {
            sprintf(msg, "%s\n", "mibget malloc failed");
            goto error_exit;
        }
        if (last_item)
            last_item->next_item = temp;
        else
            first_item = temp;
        last_item = temp;
	last_item->next_item = nilp(mib_item_t);
        last_item->group = req->level;
        last_item->mib_id = req->name;
        last_item->length = req->len;
        last_item->valp = (char *)malloc(req->len);
 
        databuf.maxlen = last_item->length;
        databuf.buf    = last_item->valp;
        databuf.len    = 0;
        flags = 0;
        getcode = getmsg(mibfd, nilp(struct strbuf), &databuf, &flags);
        if (getcode == -1) {
            sprintf(msg, "%s\n", "mibget getmsg(data) failed");
            goto error_exit;
        } else if (getcode != 0) {
            sprintf( msg, "mibget getmsg(data) returned %d, databuf.maxlen = %d,databuf.len = %d\n",
                 getcode, databuf.maxlen, databuf.len);
            goto error_exit;
        }
    }
 
error_exit:;
    while (first_item) {
        last_item = first_item;
        first_item = first_item->next_item;
	free(last_item->valp);
        free(last_item);
    }
    return first_item;
}

