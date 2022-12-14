/*
 * Copyright (c) 1990-1997, Sun Microsystems, Inc.
 * All rights reserved.
 *
 * local.h contains definitions local to nfs_inet.
 */

#ifndef _LOCAL_H
#define	_LOCAL_H

#pragma ident	"@(#)local.h	1.16	97/04/29 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <in.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/if_ether.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <sys/saio.h>
#include <nfs_prot.h>

/*
 * NFS: This structure represents the current open file.
 */
struct nfs_file {
	int status;
	nfs_fh fh;
	int type;
	u_int offset;
	nfscookie cookie;
};

struct nfs_fid {
	u_short nf_len;
	u_short nf_pad;
	struct nfs_fh fh;
};

#define	READ_SIZE	8192	/* NFS readsize */
/*
 * The size of the rcv buffers. Note the int32_t added for alignment.
 */
#define	NFSBUF_SIZE	(READ_SIZE + sizeof (struct ether_header) + \
	sizeof (struct ip) + sizeof (struct udphdr) + \
	sizeof (struct rpc_msg) + sizeof (struct readres) + sizeof (int32_t))

/*
 * Device - this union stores information about our boot device.
 */
typedef	union device {
	int handle;		/* OBP machines */
	struct saioreq sa;	/* SUNMON machines */
} bootdev_t;

#ifdef	__cplusplus
}
#endif

#endif /* _LOCAL_H */
