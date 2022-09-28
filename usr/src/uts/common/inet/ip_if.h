/*
 * Copyright (c) 1992-1998 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_INET_IP_IF_H
#define	_INET_IP_IF_H

#pragma ident	"@(#)ip_if.h	1.19	98/01/06 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

extern	mblk_t	*ill_arp_alloc(ill_t *ill, uchar_t *template, ipaddr_t addr);

extern	void	ill_delete(ill_t *ill);

extern	mblk_t	*ill_dlur_gen(uchar_t *addr, uint_t addr_length,
				t_uscalar_t sap, t_scalar_t sap_length);

extern	void	ill_down(ill_t *ill);

extern	void	ill_fastpath_ack(ill_t *ill, mblk_t *mp);

extern	void	ill_fastpath_probe(ill_t *ill, mblk_t *dlur_mp);

extern	boolean_t	ill_frag_timeout(ill_t *ill, time_t dead_interval);

extern	void	ill_frag_prune(ill_t *ill, uint_t max_count);

extern	int	ill_init(queue_t *q, ill_t *ill);

extern	int	ill_dls_info(struct sockaddr_dl *sdl, ipif_t *ipif);

extern	ill_t	*ill_lookup_on_name(char *name, size_t namelen);

extern	int	ip_ill_report(queue_t *q, mblk_t *mp, void *arg);

extern	int	ip_ipif_report(queue_t *q, mblk_t *mp, void *arg);

extern	void	ip_ll_subnet_defaults(ill_t *ill, mblk_t *mp);

extern	void	ip_sioctl_copyin_done(queue_t *q, mblk_t *mp);

extern	void	ip_sioctl_copyin_setup(queue_t *q, mblk_t *mp);

extern	int	ip_sioctl_copyin_writer(mblk_t *mp);

extern	void	ip_sioctl_iocack(queue_t *q, mblk_t *mp);

extern	int	ip_rt_delete(ipaddr_t dst_addr, ipaddr_t mask,
			ipaddr_t gw_addr, uint_t rtm_addrs,
			int flags, boolean_t ioctl_msg);

extern	int	ip_rt_add(ipaddr_t dst_addr, ipaddr_t mask, ipaddr_t gw_addr,
			uint_t type, int flags, boolean_t ioctl_msg);

extern	boolean_t	ipif_arp_up(ipif_t *ipif, ipaddr_t addr);

extern	void	ipif_down(ipif_t *ipif);

extern	char	*ipif_get_name(ipif_t *ipif, char *buf, int len);

extern	boolean_t	ipif_loopback_init(void);

extern	void	ipif_mask_reply(ipif_t *ipif);

extern	ire_t	*ipif_to_ire(ipif_t *ipif);

extern	ipif_t	*ipif_lookup_group(ipaddr_t group);

extern	ipif_t	*ipif_lookup_addr(ipaddr_t addr);

extern	ipif_t	*ipif_lookup_remote(ill_t *ill, ipaddr_t addr);

extern	ipif_t	*ipif_lookup_interface(ipaddr_t if_addr, ipaddr_t dst);

extern	ipif_t	*ifgrp_scheduler(ipif_t *ipif);

extern int ifgrp_get(queue_t *q, mblk_t *mp, void *cp);
extern int ifgrp_set(queue_t *q, mblk_t *mp, char *value, void *cp);

#ifdef	__cplusplus
}
#endif

#endif	/* _INET_IP_IF_H */
