#ident	"@(#)dhcp.c	1.86	98/01/13 SMI"

/*
 * Copyright (c) 1997 by Sun Microsystems, Inc. All rights reserved.
 */

/*
 * this file contains the implementation of the DHCP server.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <assert.h>
#include <sys/socket.h>
#include <sys/byteorder.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/errno.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/dhcp.h>
#include "dhcpd.h"
#include "per_network.h"
#include "interfaces.h"
#include "icmp.h"
#include <locale.h>

static int dhcp_offer(IF *, PKT_LIST *);
static void dhcp_req_ack(IF *, PKT_LIST *);
static void dhcp_dec_rel(IF *, PKT_LIST *, int);
static void dhcp_inform(IF *, PKT_LIST *);
static PKT *gen_reply_pkt(PKT_LIST *, int, u_int *, u_char **,
    struct in_addr *);
static u_char *get_option_code(ENCODE *, u_char, u_char *);
static int lease_negotiable(ENCODE *);
static time_t get_lease_option(ENCODE *);
static void set_lease_option(ENCODE **, time_t);
static int config_lease(PKT_LIST *, PN_REC *, ENCODE **, time_t);
static int is_option_requested(PKT_LIST *, u_short);
static void add_request_list(IF *, PKT_LIST *, ENCODE **, struct in_addr *);
static char *disp_client_msg(PKT_LIST *, char *, int);
static char *get_class_id(PKT_LIST *, char *, int);
static void add_offer(IF *, u_char *, int, PN_REC *);
static OFFLST *find_offer(IF *, u_char *, int);

/*
 * Dispatch the DHCP packet based on its type.
 *
 * Returns TRUE if the plp is to be released, FALSE if it is to be preserved.
 */
int
dhcp(IF *ifp, PKT_LIST *plp)
{
	int release = TRUE;
	char buf[BUFSIZ];

	if (plp->opts[CD_DHCP_TYPE]->len != 1) {
		dhcpmsg(LOG_INFO,
		    "Garbled DHCP Message type option from client: %s\n",
		    disp_cid(plp, buf, sizeof (buf)));
		return (release);
	}

	switch (*plp->opts[CD_DHCP_TYPE]->value) {
	case DISCOVER:
#ifdef	DEBUG
		dhcpmsg(LOG_DEBUG, "dhcp() - processing OFFER...\n");
#endif	/* DEBUG */
		release = dhcp_offer(ifp, plp);
#ifdef	DEBUG
		dhcpmsg(LOG_DEBUG, "dhcp() - processed OFFER.\n");
#endif	/* DEBUG */
		break;
	case REQUEST:
#ifdef	DEBUG
		dhcpmsg(LOG_DEBUG, "dhcp() - processing REQUEST...\n");
#endif	/* DEBUG */
		dhcp_req_ack(ifp, plp);
#ifdef	DEBUG
		dhcpmsg(LOG_DEBUG, "dhcp() - processed REQUEST.\n");
#endif	/* DEBUG */
		break;
	case DECLINE:
#ifdef	DEBUG
		dhcpmsg(LOG_DEBUG, "dhcp() - processing DECLINE...\n");
#endif	/* DEBUG */
		dhcp_dec_rel(ifp, plp, DECLINE);
#ifdef	DEBUG
		dhcpmsg(LOG_DEBUG, "dhcp() - processed DECLINE.\n");
#endif	/* DEBUG */
		break;
	case RELEASE:
#ifdef	DEBUG
		dhcpmsg(LOG_DEBUG, "dhcp() - processing RELEASE...\n");
#endif	/* DEBUG */
		dhcp_dec_rel(ifp, plp, RELEASE);
#ifdef	DEBUG
		dhcpmsg(LOG_DEBUG, "dhcp() - processed RELEASE.\n");
#endif	/* DEBUG */
		break;
	case INFORM:
#ifdef	DEBUG
		dhcpmsg(LOG_DEBUG, "dhcp() - processing INFORM...\n");
#endif	/* DEBUG */
		dhcp_inform(ifp, plp);
#ifdef	DEBUG
		dhcpmsg(LOG_DEBUG, "dhcp() - processed INFORM.\n");
#endif	/* DEBUG */
		break;
	default:
		dhcpmsg(LOG_INFO,
		    "Unexpected DHCP message type: %d from client: %s.\n",
		    plp->opts[CD_DHCP_TYPE]->value, disp_cid(plp, buf,
		    sizeof (buf)));
		break;
	}
	return (release);
}

/*
 * Responding to a DISCOVER message. icmp echo check (if done) is asynchronous.
 * This means that this function will actually be called twice - once to
 * register a candidate IP address for ICMP echo validation, and a second time
 * once the status of that candidate IP address is known. Both passes are
 * delineated by the validated_IP_address flag.
 *
 * Returns TRUE if the plp is to be released, FALSE if it is to be preserved.
 * (Preserved means it was put back on the interface's pkthead until the ICMP
 * echo check is completed.
 */
static int
dhcp_offer(IF *ifp, PKT_LIST *plp)
{
	int		validated_IP_address = FALSE;
	int		release = TRUE;	/* release the pkt by default */
	PN_REC		pn;
	struct in_addr	netaddr, subnetaddr;
	PER_NET_DB	pndb;
	u_char		cid[DT_MAX_CID_LEN];
	u_int		cid_len, replen;
	int		used_pkt_len;
	PKT 		*rep_pktp;
	u_char		*optp;
	ENCODE		*ecp, *vecp, *macro_ecp, *macro_vecp,
			*classid_ecp, *classid_vecp,
			*cid_ecp, *cid_vecp,
			*net_ecp, *net_vecp, *hecp;
	MACRO		*net_mp, *pkt_mp, *class_mp, *cid_mp;
	u_short		boot_secs;
	int		i;
	char		*class_id;
	time_t		now, newlease, oldlease = 0;
	OFFLST		*offerp = NULL;
	int		records;
	int		err = 0;
	int		exists;
	char		network[NTOABUF], ntoab[NTOABUF];
	char		buf[BUFSIZ];
	char		class_idbuf[DHCP_SCRATCH];
	struct		hostent	h, *hp;
	char		hbuf[BUFSIZ];

	boot_secs = ntohs(plp->pkt->secs);
	now = time(NULL);

	if (determine_network(ifp, plp, &netaddr, &subnetaddr) != 0)
		return (release);

	if ((err = open_per_net(&pndb, &netaddr, &subnetaddr)) != 0) {
		if (verbose && err == ENOENT) {
			netaddr.s_addr &= subnetaddr.s_addr;
			dhcpmsg(LOG_INFO,
"There is no %s dhcp-network table for DHCP client's network.\n",
			    inet_ntoa_r(netaddr, ntoab));
		}
		return (release);
	}

	/* don't need the giaddr/ifaddr... */
	netaddr.s_addr &= subnetaddr.s_addr;
	(void) inet_ntoa_r(netaddr, network);

	get_client_id(plp, cid, &cid_len);
	class_id = get_class_id(plp, class_idbuf, sizeof (class_idbuf));

	/*
	 * Let's check the status of ICMP ECHO validation. Note also that
	 * we'll scan the offer list for this client as well.
	 *
	 * We'll never see DHCP_ICMP_PENDING entries, because the packet
	 * demultiplexer in main.c bypasses these.
	 */

	assert(plp->d_icmpflag != DHCP_ICMP_PENDING);

	if (plp->d_icmpflag == DHCP_ICMP_IN_USE ||
	    plp->d_icmpflag == DHCP_ICMP_AVAILABLE ||
	    (offerp = find_offer(ifp, cid, cid_len)) != NULL) {
		if (offerp != NULL) {
			(void) memcpy((char *)&pn, (char *)&offerp->pn,
			    sizeof (PN_REC));
			(void) free(offerp); /* we'll refresh this later... */
			/*
			 * We've already validated this IP address in the
			 * past, and due to the OFFER list, we would not have
			 * offered this IP address to another client, so
			 * basically we remember we've validated this address.
			 */
			plp->off_ip.s_addr = pn.clientip.s_addr;
			plp->d_icmpflag = DHCP_ICMP_AVAILABLE;
		}

		if ((records = lookup_per_net(&pndb, PN_CLIENT_IP,
		    &plp->off_ip, sizeof (struct in_addr), NULL, &pn)) != 0) {
			if (plp->d_icmpflag == DHCP_ICMP_IN_USE) {
				dhcpmsg(LOG_ERR,
"ICMP ECHO reply to OFFER candidate: %s, disabling.\n",
				    inet_ntoa_r(pn.clientip, ntoab));
				pn.flags |= F_UNUSABLE;
				(void) put_per_net(&pndb, &pn, PN_CLIENT_IP);
				close_per_net(&pndb);
				return (release);
			}
			/*
			 * Recovered the ICMP validated record.
			 */
			if (debug) {
				dhcpmsg(LOG_DEBUG,
				    "Using ICMP validated address: %s\n",
				    inet_ntoa_r(plp->off_ip, ntoab));
			}
			validated_IP_address = TRUE;
		}
	} else
		validated_IP_address = FALSE;

	ecp = vecp = NULL;
	net_vecp = net_ecp = NULL;
	macro_vecp = macro_ecp = NULL;
	classid_vecp = classid_ecp = NULL;
	cid_vecp = cid_ecp = NULL;

	/*
	 * Setting ecp in two places looks strange, but note that
	 * ecp is used by lease_negotiable()!
	 */
	if (!no_dhcptab) {
		if ((net_mp = get_macro(network)) != NULL)
			net_ecp = net_mp->head;

		if ((pkt_mp = get_macro(pn.macro)) != NULL)
			macro_ecp = pkt_mp->head;

		if ((cid_mp = get_macro(disp_cid(plp, buf,
		    sizeof (buf)))) != NULL)
			cid_ecp = cid_mp->head;

		if (class_id != NULL) {
			if ((class_mp = get_macro(class_id)) != NULL) {
				classid_vecp = vendor_encodes(class_mp,
				    class_id);
				classid_ecp = class_mp->head;
			}
			if (net_mp != NULL)
				net_vecp = vendor_encodes(net_mp, class_id);
			if (pkt_mp != NULL)
				macro_vecp = vendor_encodes(pkt_mp, class_id);
			if (cid_mp != NULL)
				cid_vecp = vendor_encodes(cid_mp, class_id);

			vecp = combine_encodes(classid_vecp, net_vecp,
			    ENC_COPY);
			vecp = combine_encodes(vecp, macro_vecp, ENC_DONT_COPY);
			vecp = combine_encodes(vecp, cid_vecp, ENC_DONT_COPY);
		}

		if (classid_ecp != NULL)
			ecp = combine_encodes(classid_ecp, net_ecp, ENC_COPY);
		else
			ecp = copy_encode_list(net_ecp);

		ecp = combine_encodes(ecp, macro_ecp, ENC_DONT_COPY);
		ecp = combine_encodes(ecp, cid_ecp, ENC_DONT_COPY);
	}

	/*
	 * Macros are evaluated this way: First apply parameters from
	 * a client class macro (if present), then apply those from the
	 * network macro (if present), then apply those from the
	 * server macro (if present), and finally apply those from a
	 * client id macro (if present).
	 */

	if (!validated_IP_address) {
		/* Try to find an existing usable pn entry for the client. */
		exists = FALSE;
		if ((records = lookup_per_net(&pndb, PN_CID, cid, cid_len, NULL,
		    &pn)) > 0) {
			for (i = 0, err = 0; i < records && err == 0; i++,
			    err = get_per_net(&pndb, PN_CID, &pn)) {
				if ((pn.flags & F_UNUSABLE) == 0) {
					exists = TRUE;
					break;
				} else {
					dhcpmsg(LOG_NOTICE,
"Entry: %s currently marked as unusable.\n",
					    inet_ntoa_r(pn.clientip, ntoab));
					if (pn.flags & F_MANUAL) {
						dhcpmsg(LOG_NOTICE,
"Entry: %s was manually allocated. No dynamic address will be allocated.\n",
						    inet_ntoa_r(pn.clientip,
						    ntoab));
						close_per_net(&pndb);
						free_encode_list(ecp);
						free_encode_list(vecp);
						return (release);
					}
				}
			}
		}

		if (!exists) {
			if (select_offer(&pndb, plp, ifp, &pn) == 0) {
				dhcpmsg(LOG_ERR, "No more IP addresses on %1$s "
				    "network (%2$s)\n", network,
				    disp_cid(plp, buf, sizeof (buf)));
				close_per_net(&pndb);
				free_encode_list(ecp);
				free_encode_list(vecp);
				return (release);
			}
		}

		if (exists) {
			if (server_ip.s_addr != pn.serverip.s_addr &&
			    boot_secs < (u_short)DHCP_RENOG_WAIT) {
				/*
				 * An address, but not ours! We'll wait alittle
				 * to see if the "owner" server responds. If
				 * not, we'll respond.
				 */
				if (verbose) {
					dhcpmsg(LOG_INFO,
"Client: %1$s has a configuration owned by server: %2$s.\n",
					    disp_cid(plp, buf, sizeof (buf)),
					    inet_ntoa_r(pn.serverip, ntoab));
				}
				free_encode_list(ecp);
				free_encode_list(vecp);
				close_per_net(&pndb);
				return (release);
			}

			/*
			 * There was an existing entry for the client. This
			 * client could have amnesia. If the lease is ok,
			 * offer him his old configuration. If the lease is
			 * not ok, then check and see if the lease is
			 * negotiable. If lease is negotiable, and he asked
			 * for a new lease time, then offer him his old
			 * configuration with the new lease time. If the lease
			 * is not negotiable, then ignore the client.
			 */
			if ((u_long)ntohl(pn.lease) < (u_long)now &&
			    (!lease_negotiable(ecp) && (pn.flags & F_MANUAL))) {
				if (verbose) {
					dhcpmsg(LOG_INFO,
"Client: %1$s lease on %2$s expired.\n",
					    disp_cid(plp, buf, sizeof (buf)),
					    inet_ntoa_r(pn.clientip, ntoab));
				}
				free_encode_list(ecp);
				free_encode_list(vecp);
				close_per_net(&pndb);
				return (release);
			} else {
				if (ntohl(pn.lease) == DHCP_PERM || pn.flags &
				    F_AUTOMATIC) {
					oldlease = DHCP_PERM;
				} else {
					if ((u_long)ntohl(pn.lease) <
					    (u_long)now)
						oldlease = 0L;
					else {
						oldlease = (u_long)ntohl(
						    pn.lease) - (u_long)now;
					}
				}
			}
		}

		if ((ifp->flags & IFF_NOARP) == 0)
			(void) set_arp(ifp, &pn.clientip, NULL, 0,
			    DHCP_ARP_DEL);

		if (!noping) {
			/*
			 * If you can't create the ping thread, let the plp fall
			 * by the wayside. Otherwise pretend we're done.
			 */
			if (icmp_echo_register(ifp, &pn, plp) != 0) {
				dhcpmsg(LOG_WARNING,
"ICMP ECHO reply check cannot be registered for: %s, ignoring\n",
				    inet_ntoa_r(pn.clientip, ntoab));
			} else
				release = FALSE;	/* preserve */
			free_encode_list(ecp);
			free_encode_list(vecp);
			close_per_net(&pndb);
			return (release);
		}
	} else {
		/*
		 * ICMP validated IP address. We've been thru the above
		 * logic before, and have found an entry we can OFFER to
		 * this client. Is there a window where a slow server
		 * which owns an address will finally respond while we're
		 * offering its address to the client? Yes...
		 *
		 * Because the lease negotiable flag is encoded in the various
		 * macro definitions, we'll have to 'recreate' the reply
		 * destined to be offered to the client.
		 */
		if (lookup_per_net(&pndb, PN_CLIENT_IP, &plp->off_ip,
		    sizeof (struct in_addr), NULL, &pn) == 0) {
			dhcpmsg(LOG_ERR,
"ICMP validated IP %s not found in dhcp network table (DHCP client)\n",
			    inet_ntoa_r(plp->off_ip, ntoab));
			close_per_net(&pndb);
			free_encode_list(ecp);
			free_encode_list(vecp);
			return (release);
		}

		if ((ifp->flags & IFF_NOARP) == 0)
			(void) set_arp(ifp, &pn.clientip, NULL, 0,
			    DHCP_ARP_DEL);
	}

	if (!no_dhcptab) {
		/*
		 * If dhcptab configured to return hostname, do so.
		 */
		if (find_encode(ecp, CD_BOOL_HOSTNAME) != NULL) {
			hp = gethostbyaddr_r((char *)&pn.clientip,
			    sizeof (struct in_addr), AF_INET, &h, hbuf,
			    sizeof (hbuf), &err);
			if (hp != NULL) {
				hecp = make_encode(CD_HOSTNAME,
				    strlen(hp->h_name), hp->h_name, 1);
				replace_encode(&ecp, hecp, ENC_DONT_COPY);
			}
		}
	}

	/* First get a generic reply packet. */
	rep_pktp = gen_reply_pkt(plp, OFFER, &replen, &optp, &ifp->addr);

	/* Set the client's IP address */
	rep_pktp->yiaddr.s_addr = pn.clientip.s_addr;

	/* Calculate lease time. */
	newlease = config_lease(plp, &pn, &ecp, oldlease);

	/*
	 * Client is requesting specific options. let's try and ensure it
	 * gets what it wants, if at all possible.
	 */
	if (plp->opts[CD_REQUEST_LIST] != NULL)
		add_request_list(ifp, plp, &ecp, &pn.clientip);

	/* Now load all the asked for / configured options */
	used_pkt_len = load_options(DHCP_DHCP_CLNT | DHCP_SEND_LEASE, rep_pktp,
	    replen, optp, ecp, vecp);

	if (used_pkt_len < sizeof (PKT))
		used_pkt_len = sizeof (PKT);

	if (send_reply(ifp, rep_pktp, used_pkt_len, &pn.clientip) == 0) {
		if (newlease == DHCP_PERM)
			pn.lease = htonl(newlease);
		else
			pn.lease = htonl(now + newlease);
		add_offer(ifp, cid, cid_len, &pn);
	}

	free_encode_list(ecp);
	free_encode_list(vecp);
	free(rep_pktp);
	close_per_net(&pndb);
	return (release);
}

/*
 * Responding to REQUEST message.
 *
 * Very similar to dhcp_offer(), except that we need to be more
 * descriminating.
 *
 * The ciaddr field is TRUSTED. A INIT-REBOOTing client will place its
 * notion of its IP address in the requested IP address option. INIT
 * clients will place the value in the OFFERs yiaddr in the requested
 * IP address option. INIT-REBOOT packets are differentiated from INIT
 * packets in that the server id option is missing. ciaddr will only
 * appear from clients in the RENEW/REBIND states.
 *
 * Returns 0 always, although error messages may be generated. Database
 * write failures are no longer fatal, since we'll only respond to the
 * client if the write succeeds.
 */
static void
dhcp_req_ack(IF *ifp, PKT_LIST *plp)
{
	PN_REC		pn;
	struct in_addr	netaddr, subnetaddr, pernet, serverid, ciaddr,
			claddr;
	struct in_addr	*subnetp, dest_in;
	PER_NET_DB	pndb;
	u_char		cid[DT_MAX_CID_LEN];
	u_int		cid_len, replen;
	int		actual_len;
	int		pkt_type = ACK;
	PKT 		*rep_pktp;
	u_char		*optp;
	ENCODE		*ecp, *vecp,
			*classid_ecp, *classid_vecp,
			*net_ecp, *net_vecp,
			*macro_ecp, *macro_vecp,
			*cid_ecp, *cid_vecp, *hecp;
	MACRO		*class_mp, *pkt_mp, *net_mp, *cid_mp;
	char		*class_id;
	char		nak_mesg[DHCP_SCRATCH];
	time_t		newlease, oldlease, now;
	OFFLST		*offerp	= NULL;
	int		found = FALSE, err = 0, recs = 0;
	int		write_error = 0, i, clnt_state;
	u_short		boot_secs;
	char		ascii_ip[NTOABUF], network[NTOABUF], ntoab[NTOABUF];
	char		buf[BUFSIZ];
	char		class_idbuf[DHCP_SCRATCH];
	struct		hostent	h, *hp;
	char		hbuf[BUFSIZ];

	ciaddr.s_addr = plp->pkt->ciaddr.s_addr;
	boot_secs = ntohs(plp->pkt->secs);
	now = time(NULL);

	/*
	 * Trust client's notion of IP address if ciaddr is set. Use it
	 * to figure out correct dhcp-network database.
	 */
	if (ciaddr.s_addr == 0L) {
		if (determine_network(ifp, plp, &netaddr, &subnetaddr) != 0)
			return;
	} else {
		/*
		 * Calculate client's default net mask, consult netmasks
		 * database to see if net is further subnetted. Use resulting
		 * subnet mask with client's address to produce dhcp-network
		 * database name.
		 */
		netaddr.s_addr = ciaddr.s_addr;
		subnetp = &subnetaddr;
		(void) get_netmask(&netaddr, &subnetp);
	}
	pernet.s_addr = netaddr.s_addr & subnetaddr.s_addr;
	(void) inet_ntoa_r(pernet, network);

	if ((err = open_per_net(&pndb, &netaddr, &subnetaddr)) != 0) {
		if (verbose && err == ENOENT)
			dhcpmsg(LOG_INFO,
"There is no %s dhcp-network table for DHCP client's network.\n",
			    network);
		return;
	}

	get_client_id(plp, cid, &cid_len);
	class_id = get_class_id(plp, class_idbuf, sizeof (class_idbuf));

	/* Determine type of REQUEST we've got. */
	if (plp->opts[CD_SERVER_ID] != NULL) {
		/*
		 * Request in response to an OFFER. ciaddr must not
		 * be set. Requested IP address option will hold address
		 * we offered the client.
		 */
		clnt_state = INIT_STATE;
		(void) memcpy((void *)&serverid,
		    plp->opts[CD_SERVER_ID]->value, sizeof (struct in_addr));
		if (serverid.s_addr != ifp->addr.s_addr) {
			/*
			 * Someone else was selected. See if we made an
			 * offer, and clear it if we did. If offer expired
			 * before client responded, then no need to do
			 * anything.
			 */
			if ((offerp = find_offer(ifp, cid, cid_len)) != NULL)
				free(offerp);
			if (verbose) {
				dhcpmsg(LOG_INFO,
				    "Client: %1$s selected server: %2$s.\n",
				    disp_cid(plp, buf, sizeof (buf)),
				    inet_ntoa_r(serverid, ntoab));
			}
			close_per_net(&pndb);
			return;
		}
		/*
		 * If the offer expires before the client
		 * got around to requesting, we'll silently ignore the
		 * client, until it drops back and tries to discover
		 * again. We will print a message in debug mode however.
		 */
		if ((offerp = find_offer(ifp, cid, cid_len)) == NULL) {
			/*
			 * Hopefully, the timeout value is fairly long to
			 * prevent this.
			 */
			if (verbose) {
				dhcpmsg(LOG_INFO,
				    "Offer expired for client: %s\n",
				    disp_cid(plp, buf, sizeof (buf)));
			}
			close_per_net(&pndb);
			return;
		}

		/*
		 * The client selected us. Create a ACK, and send
		 * it off to the client, commit to permanent
		 * storage the new binding.
		 */
		(void) memcpy((char *)&pn, (char *)&offerp->pn,
		    sizeof (PN_REC));
		free(offerp);

		if (plp->opts[CD_REQUESTED_IP_ADDR] == NULL) {
			dhcpmsg(LOG_ERR,
			    "%s: REQUEST is missing requested IP option.\n",
			    disp_cid(plp, buf, sizeof (buf)));
			close_per_net(&pndb);
			return;
		}

		/*
		 * If client thinks we offered it a different address, then
		 * ignore it.
		 */
		if (memcmp((char *)&pn.clientip,
		    plp->opts[CD_REQUESTED_IP_ADDR]->value,
		    plp->opts[CD_REQUESTED_IP_ADDR]->len) != 0) {
			if (verbose) {
				dhcpmsg(LOG_INFO,
"%s: believes offered IP address is different than what was offered.\n",
				    disp_cid(plp, buf, sizeof (buf)));
			}
			close_per_net(&pndb);
			return;
		}

		/*
		 * Clear out any temporary ARP table entry we may have
		 * created during the offer.
		 */
		if ((ifp->flags & IFF_NOARP) == 0) {
			(void) set_arp(ifp, &pn.clientip, NULL, 0,
			    DHCP_ARP_DEL);
		}
	} else {
		/*
		 * Either a client in the INIT-REBOOT state, or one in
		 * either RENEW or REBIND states. The latter will have
		 * ciaddr set, whereas the former will place its concept
		 * of its IP address in the requested IP address option.
		 */
		clnt_state = INIT_REBOOT_STATE;
		if (ciaddr.s_addr == 0L) {
			/*
			 * Client isn't sure of its IP address. It's
			 * attempting to verify its address, thus requested
			 * IP option better be present, and correct.
			 */
			if (plp->opts[CD_REQUESTED_IP_ADDR] == NULL) {
				dhcpmsg(LOG_ERR,
"Client: %s REQUEST is missing requested IP option.\n",
				    disp_cid(plp, buf, sizeof (buf)));
				close_per_net(&pndb);
				return;
			}
			(void) memcpy(&claddr,
			    plp->opts[CD_REQUESTED_IP_ADDR]->value,
			    plp->opts[CD_REQUESTED_IP_ADDR]->len);

			if ((recs = lookup_per_net(&pndb, PN_CID,
			    (void *)cid, cid_len, NULL, &pn)) < 0) {
				close_per_net(&pndb);
				return;
			}

			for (i = 0, err = 0; i < recs && err == 0; i++,
			    err = get_per_net(&pndb, PN_CID, &pn)) {
				if ((pn.flags & F_UNUSABLE) == 0) {
					found = TRUE;
					break;
				}
			}
		} else {
			/*
			 * Client knows its IP address. It is trying to
			 * RENEW/REBIND (extend its lease). We trust ciaddr,
			 * and use it to locate the client's record. If we
			 * can't find the client's record, then we keep
			 * silent. If the client id of the record doesn't
			 * match this client, then the database is
			 * inconsistent, and we'll ignore it.
			 */
			if ((recs = lookup_per_net(&pndb, PN_CLIENT_IP,
			    &ciaddr, sizeof (struct in_addr), NULL,
			    &pn)) < 0) {
				close_per_net(&pndb);
				return;
			}

			if (recs != 0) {
				if (pn.flags & F_UNUSABLE) {
					dhcpmsg(LOG_NOTICE,
"Entry: %s currently marked as unusable.\n",
					    inet_ntoa_r(pn.clientip, ntoab));
					close_per_net(&pndb);
					return;
				}
				if (memcmp(cid, pn.cid, cid_len) != 0) {
					dhcpmsg(LOG_ERR,
"Client: %1$s has IP: %2$s, but is registered for: %3$s.\n",
					    disp_cid(plp, buf, sizeof (buf)),
					    inet_ntoa_r(ciaddr, ascii_ip),
					    inet_ntoa_r(pn.clientip, ntoab));
					close_per_net(&pndb);
					return;
				}
				found = TRUE;
			}
			claddr.s_addr = ciaddr.s_addr;
		}
		if (!found) {
			/*
			 * There is no such client registered for this
			 * address. Check if their address is on the correct
			 * net. If it is, then we'll assume that some other,
			 * non-database sharing DHCP server knows about this
			 * client. If the client is on the wrong net, NAK'em.
			 */
			if (recs == 0 && (claddr.s_addr &
			    subnetaddr.s_addr) == pernet.s_addr) {
				/* Right net, but no record of client. */
				if (verbose) {
					dhcpmsg(LOG_INFO,
"Client: %1$s is trying to verify unrecorded address: %2$s, ignored.\n",
					    disp_cid(plp, buf, sizeof (buf)),
					    inet_ntoa_r(claddr, ntoab));
				}
				close_per_net(&pndb);
				return;
			} else {
				if (ciaddr.s_addr == 0L) {
					(void) sprintf(nak_mesg, "No valid \
configuration exists on network: %s",
					    network);
					pkt_type = NAK;
				} else {
					if (verbose) {
						dhcpmsg(LOG_INFO,
"Client: %1$s is not recorded as having address: %2$s\n",
						    disp_cid(plp, buf,
						    sizeof (buf)),
						    inet_ntoa_r(ciaddr, ntoab));
					}
					close_per_net(&pndb);
					return;
				}
			}
		} else {
			if (claddr.s_addr != pn.clientip.s_addr) {
				/*
				 * Client has the wrong IP address. Nak.
				 */
				(void) sprintf(nak_mesg,
				    "Incorrect IP address.");
				pkt_type = NAK;
			} else {
				if ((pn.flags & F_AUTOMATIC) == 0 &&
				    (u_long)ntohl(pn.lease) < (u_long)now) {
					(void) sprintf(nak_mesg,
					    "Lease has expired.");
					pkt_type = NAK;
				}
			}
			/*
			 * If this address is not owned by this server,
			 * then don't respond until after DHCP_ time passes,
			 * to give the server that *OWNS* the address time
			 * to respond first.
			 */
			if (pn.serverip.s_addr != server_ip.s_addr &&
			    boot_secs < (u_short)DHCP_RENOG_WAIT) {
				if (verbose) {
					dhcpmsg(LOG_INFO,
"Client: %1$s is requesting verification of address owned by %2$s\n",
					    disp_cid(plp, buf, sizeof (buf)),
					    inet_ntoa_r(pn.serverip, ntoab));
				}
				close_per_net(&pndb);
				return;
			}
		}
	}

	/*
	 * Produce the appropriate response.
	 */
	if (pkt_type == NAK) {
		rep_pktp = gen_reply_pkt(plp, NAK, &replen, &optp,
		    &ifp->addr);
		/*
		 * Setting yiaddr to the client's ciaddr abuses the
		 * semantics of yiaddr, So we set this to 0L.
		 *
		 * We twiddle the broadcast flag to force the
		 * server/relay agents to broadcast the NAK.
		 *
		 * Exception: If a client's lease has expired, and it
		 * is still trying to renegotiate its lease, AND ciaddr
		 * is set, AND ciaddr is on a "remote" net, unicast the
		 * NAK. Gross, huh? But SPA could make this happen with
		 * super short leases.
		 */
		rep_pktp->yiaddr.s_addr = 0L;
		if (ciaddr.s_addr != 0L &&
		    (ciaddr.s_addr & subnetaddr.s_addr) != pernet.s_addr) {
			dest_in.s_addr = ciaddr.s_addr;
		} else {
			rep_pktp->flags |= htons(BCAST_MASK);
			dest_in.s_addr = INADDR_BROADCAST;
		}

		*optp++ = CD_MESSAGE;
		*optp++ = (u_char)strlen(nak_mesg);
		(void) memcpy(optp, nak_mesg, strlen(nak_mesg));
		optp += strlen(nak_mesg);
		*optp = CD_END;
		actual_len = BASE_PKT_SIZE + (u_int)(optp - rep_pktp->options);
		if (actual_len < sizeof (PKT))
			actual_len = sizeof (PKT);
		(void) send_reply(ifp, rep_pktp, actual_len, &dest_in);
	} else {
		rep_pktp = gen_reply_pkt(plp, ACK, &replen, &optp,
		    &ifp->addr);

		/* Set the client's IP address */
		rep_pktp->yiaddr.s_addr = pn.clientip.s_addr;
		dest_in.s_addr = pn.clientip.s_addr;

		ecp = vecp = NULL;
		classid_vecp = classid_ecp = NULL;
		net_vecp = net_ecp = NULL;
		macro_vecp = macro_ecp = NULL;
		cid_vecp = cid_ecp = NULL;

		if (!no_dhcptab) {
			if ((net_mp = get_macro(network)) != NULL)
				net_ecp = net_mp->head;

			if ((pkt_mp = get_macro(pn.macro)) != NULL)
				macro_ecp = pkt_mp->head;

			if ((cid_mp = get_macro(disp_cid(plp, buf,
			    sizeof (buf)))) != NULL)
				cid_ecp = cid_mp->head;

			/*
			 * Macros are evaluated this way: First apply parameters
			 * from a client class macro (if present), then apply
			 * those from the network macro (if present), then apply
			 * those from the server macro (if present), and finally
			 * apply those from a client id macro (if present).
			 */
			if (class_id != NULL) {
				if ((class_mp = get_macro(class_id)) != NULL) {
					classid_vecp = vendor_encodes(class_mp,
					    class_id);
					classid_ecp = class_mp->head;
				}
				if (net_mp != NULL) {
					net_vecp = vendor_encodes(net_mp,
					    class_id);
				}
				if (pkt_mp != NULL)
					macro_vecp = vendor_encodes(pkt_mp,
					    class_id);
				if (cid_mp != NULL) {
					cid_vecp = vendor_encodes(cid_mp,
					    class_id);
				}

				vecp = combine_encodes(classid_vecp, net_vecp,
				    ENC_COPY);
				vecp = combine_encodes(vecp, macro_vecp,
				    ENC_DONT_COPY);
				vecp = combine_encodes(vecp, cid_vecp,
				    ENC_DONT_COPY);
			}

			if (classid_ecp != NULL) {
				ecp = combine_encodes(classid_ecp, net_ecp,
				    ENC_COPY);
			} else
				ecp = copy_encode_list(net_ecp);

			ecp = combine_encodes(ecp, macro_ecp, ENC_DONT_COPY);
			ecp = combine_encodes(ecp, cid_ecp, ENC_DONT_COPY);

			/*
			 * If dhcptab configured to return hostname, do so.
			 */
			if (find_encode(ecp, CD_BOOL_HOSTNAME) != NULL) {
				hp = gethostbyaddr_r((char *)&pn.clientip,
				    sizeof (struct in_addr), AF_INET, &h, hbuf,
				    sizeof (hbuf), &err);
				if (hp != NULL) {
					hecp = make_encode(CD_HOSTNAME,
					    strlen(hp->h_name), hp->h_name, 1);
					replace_encode(&ecp, hecp,
					    ENC_DONT_COPY);
				}
			}
		}

		if (pn.flags & F_AUTOMATIC || pn.lease == DHCP_PERM)
			oldlease = DHCP_PERM;
		else {
			if (plp->opts[CD_SERVER_ID] != NULL) {
				/*
				 * Offered absolute Lease time is cached
				 * in the lease field of the record. If
				 * that's expired, then they'll get the
				 * policy value again here. Must have been
				 * LONG time between DISC/REQ!
				 */
				if ((u_long)ntohl(pn.lease) < (u_long)now)
					oldlease = 0L;
				else
					oldlease = ntohl(pn.lease) - now;
			} else
				oldlease = ntohl(pn.lease) - now;
		}

		/*
		 * This is a little longer than we offered (not taking into
		 * account * the secs field), but since I trust the UNIX
		 * clock better than the PC's, it is a good idea to give
		 * the PC a little more time than it thinks, just due to
		 * clock slop on PC's.
		 */
		newlease = config_lease(plp, &pn, &ecp, oldlease);

		if (newlease != DHCP_PERM)
			pn.lease = htonl(now + newlease);
		else
			pn.lease = DHCP_PERM;

		(void) memcpy(pn.cid, cid, cid_len);
		pn.cid_len = cid_len;

		/*
		 * It is critical to write the database record if the
		 * client is in the INIT state, so we don't reply to the
		 * client if this fails. However, if the client is simply
		 * trying to verify its address or extend its lease, then
		 * we'll reply regardless of the status of the write,
		 * although we'll return the old lease time.
		 *
		 * If the client is in the INIT_REBOOT state, and the
		 * lease time hasn't changed, we don't bother with the
		 * write, since nothing has changed.
		 */
		if (clnt_state == INIT_STATE || oldlease != newlease)
			write_error = put_per_net(&pndb, &pn, PN_CLIENT_IP);
		else {
			if (verbose) {
				dhcpmsg(LOG_INFO,
"Database write unnecessary for DHCP client: %1$s, %2$s\n",
				    disp_cid(plp, buf, sizeof (buf)),
				    inet_ntoa_r(pn.clientip, ntoab));
			}
		}

		if (write_error == 0 || clnt_state == INIT_REBOOT_STATE) {

			if (write_error)
				set_lease_option(&ecp, oldlease);

			if (plp->opts[CD_REQUEST_LIST])
				add_request_list(ifp, plp, &ecp, &pn.clientip);

			/* Now load all the asked for / configured options */
			actual_len = load_options(DHCP_DHCP_CLNT |
			    DHCP_SEND_LEASE, rep_pktp, replen, optp, ecp, vecp);
			if (actual_len < sizeof (PKT))
				actual_len = sizeof (PKT);
			if (verbose) {
				dhcpmsg(LOG_INFO,
				    "Client: %1$s maps to IP: %2$s\n",
				    disp_cid(plp, buf, sizeof (buf)),
				    inet_ntoa_r(pn.clientip, ntoab));
			}
			(void) send_reply(ifp, rep_pktp, actual_len,
			    &dest_in);
		}
		free_encode_list(ecp);
		free_encode_list(vecp);
	}
	free(rep_pktp);
	close_per_net(&pndb);
}

/* Reacting to a client's DECLINE or RELEASE. */
static void
dhcp_dec_rel(IF *ifp, PKT_LIST *plp, int type)
{
	char		*fmtp;
	PN_REC		pn;
	PER_NET_DB	pndb;
	struct in_addr	netaddr, subnetaddr, tmpip, *subnetp;
	int		records;
	int		err = 0;
	u_char		cid[DT_MAX_CID_LEN];
	u_int		cid_len;
	char		buf[BUFSIZ];
	char		ntoab[NTOABUF];

	/*
	 * Historical: We used to use ciaddr for the address being declined.
	 * Now the requested IP address option is used. XXXX Remove the
	 * ciaddr code after DHCP becomes full standard.
	 */
	if (type == DECLINE) {
		if (determine_network(ifp, plp, &netaddr, &subnetaddr) != 0)
			return;
		if (plp->opts[CD_REQUESTED_IP_ADDR] &&
		    plp->opts[CD_REQUESTED_IP_ADDR]->len ==
		    sizeof (struct in_addr)) {
			(void) memcpy((char *)&tmpip,
			    plp->opts[CD_REQUESTED_IP_ADDR]->value,
			    sizeof (struct in_addr));
		}
	} else {
		/*
		 * Trust client's notion of IP address if ciaddr is set. Use it
		 * to figure out correct dhcp-network database.
		 */
		tmpip.s_addr = plp->pkt->ciaddr.s_addr;
		if (tmpip.s_addr == 0L) {
			if (determine_network(ifp, plp, &netaddr,
			    &subnetaddr) != 0)
				return;
		} else {
			/*
			 * Consult netmasks table for client's mask, or
			 * accept default mask if client's mask is not
			 * in the netmasks table.
			 */
			netaddr.s_addr = tmpip.s_addr;
			subnetp = &subnetaddr;
			(void) get_netmask(&netaddr, &subnetp);
		}
	}


	if ((err = open_per_net(&pndb, &netaddr, &subnetaddr)) != 0) {
		if (verbose && err == ENOENT) {
			if (type == DECLINE) {
				fmtp =
"Client DECLINE message for unsupported net: %s\n";
			} else {
				fmtp =
"Client RELEASE message for unsupported net: %s\n";
			}
			dhcpmsg(LOG_INFO, fmtp, inet_ntoa_r(netaddr, ntoab));
		}
		return;
	}

	get_client_id(plp, cid, &cid_len);

	if ((records = lookup_per_net(&pndb, PN_CID, (void *)cid, cid_len,
	    NULL, &pn)) < 0) {
		close_per_net(&pndb);
		return;
	}

	if (records == 0) {
		if (verbose) {
			if (type == DECLINE) {
				fmtp =
"Unregistered client: %1$s is DECLINEing address: %2$s.\n";
			} else {
				fmtp =
"Unregistered client: %1$s is RELEASEing address: %2$s.\n";
			}
			dhcpmsg(LOG_INFO, fmtp, disp_cid(plp, buf,
			    sizeof (buf)), inet_ntoa_r(tmpip, ntoab));
		}
		close_per_net(&pndb);
		return;
	}

	/* If the entry is not one of ours, then give up. */
	if (pn.serverip.s_addr != server_ip.s_addr) {
		if (verbose) {
			if (type == DECLINE) {
				fmtp =
"Client: %1$s is DECLINEing: %2$s not owned by this server.\n";
			} else {
				fmtp =
"Client: %1$s is RELEASEing: %2$s not owned by this server.\n";
			}
			dhcpmsg(LOG_INFO, fmtp, disp_cid(plp, buf,
			    sizeof (buf)), inet_ntoa_r(tmpip, ntoab));
		}
		close_per_net(&pndb);
		return;
	}

	if (type == DECLINE) {
		dhcpmsg(LOG_ERR, "Client: %1$s DECLINED address: %2$s.\n",
		    disp_cid(plp, buf, sizeof (buf)), inet_ntoa_r(pn.clientip,
		    ntoab));
		dhcpmsg(LOG_ERR, "Client message: %s\n",
		    disp_client_msg(plp, buf, sizeof (buf)));
		pn.flags |= F_UNUSABLE;
	} else {
		if (pn.flags & F_MANUAL) {
			dhcpmsg(LOG_ERR,
"Client: %1$s is trying to RELEASE manual address: %2$s\n",
			    disp_cid(plp, buf, sizeof (buf)),
			    inet_ntoa_r(pn.clientip, ntoab));
			close_per_net(&pndb);
			return;
		}
		if (verbose) {
			dhcpmsg(LOG_INFO,
			    "Client: %s RELEASED address: %s\n",
			    disp_cid(plp, buf, sizeof (buf)),
			    inet_ntoa_r(pn.clientip, ntoab));
			if (plp->opts[CD_MESSAGE]) {
				dhcpmsg(LOG_INFO,
				    "RELEASE: client message: %s\n",
				    disp_client_msg(plp, buf, sizeof (buf)));

			}
		}
	}

	if ((pn.flags & F_MANUAL) == 0) {
		(void) memset(pn.cid, 0, pn.cid_len);
		pn.lease = 0L;
		pn.cid_len = 0L;
	}

	/*
	 * Ignore write errors. put_per_net will generate appropriate
	 * error message.
	 */
	(void) put_per_net(&pndb, &pn, PN_CLIENT_IP);

	close_per_net(&pndb);
}

/*
 * Responding to an INFORM message.
 *
 * INFORM messages are unicast, since client knows its address and subnetmask.
 * The server trusts clients notion of IP address. No dhcp-network database
 * access is done for requests of this type.
 */
static void
dhcp_inform(IF *ifp, PKT_LIST *plp)
{
	u_char		cid[DT_MAX_CID_LEN];
	struct in_addr	netaddr, subnetaddr;
	u_int		cid_len, replen;
	int		used_pkt_len;
	PKT 		*rep_pktp;
	u_char		*optp;
	ENCODE		*ecp, *vecp, *classid_ecp, *classid_vecp,
			*cid_ecp, *cid_vecp, *net_ecp, *net_vecp;
	MACRO		*net_mp, *class_mp, *cid_mp;
	char		*class_id;
	char		network[NTOABUF];
	char		buf[BUFSIZ];
	char		class_idbuf[DHCP_SCRATCH];

	get_client_id(plp, cid, &cid_len);
	class_id = get_class_id(plp, class_idbuf, sizeof (class_idbuf));

	if (determine_network(ifp, plp, &netaddr, &subnetaddr) != 0)
		return;

	netaddr.s_addr &= subnetaddr.s_addr;
	(void) inet_ntoa_r(netaddr, network);

	ecp = vecp = NULL;
	net_vecp = net_ecp = NULL;
	classid_vecp = classid_ecp = NULL;
	cid_vecp = cid_ecp = NULL;

	if (!no_dhcptab) {
		if ((net_mp = get_macro(network)) != NULL)
			net_ecp = net_mp->head;

		if ((cid_mp = get_macro(disp_cid(plp, buf,
		    sizeof (buf)))) != NULL)
			cid_ecp = cid_mp->head;

		/*
		 * Macros are evaluated this way: First apply parameters from
		 * a client class macro (if present), then apply those from the
		 * network macro (if present),  and finally apply those from a
		 * client id macro (if present).
		 */
		if (class_id != NULL) {
			if ((class_mp = get_macro(class_id)) != NULL) {
				classid_vecp = vendor_encodes(class_mp,
				    class_id);
				classid_ecp = class_mp->head;
			}
			if (net_mp != NULL)
				net_vecp = vendor_encodes(net_mp, class_id);
			if (cid_mp != NULL)
				cid_vecp = vendor_encodes(cid_mp, class_id);

			vecp = combine_encodes(classid_vecp, net_vecp,
			    ENC_COPY);
			vecp = combine_encodes(vecp, cid_vecp, ENC_DONT_COPY);
		}

		if (classid_ecp != NULL)
			ecp = combine_encodes(classid_ecp, net_ecp, ENC_COPY);
		else
			ecp = copy_encode_list(net_ecp);

		ecp = combine_encodes(ecp, cid_ecp, ENC_DONT_COPY);
	}

	/* First get a generic reply packet. */
	rep_pktp = gen_reply_pkt(plp, ACK, &replen, &optp, &ifp->addr);

	/*
	 * Client is requesting specific options. let's try and ensure it
	 * gets what it wants, if at all possible.
	 */
	if (plp->opts[CD_REQUEST_LIST] != NULL)
		add_request_list(ifp, plp, &ecp, &plp->pkt->ciaddr);

	/*
	 * Explicitly set the ciaddr to be that which the client gave
	 * us.
	 */
	rep_pktp->ciaddr.s_addr = plp->pkt->ciaddr.s_addr;

	/*
	 * Now load all the asked for / configured options. DONT send
	 * any lease time info!
	 */
	used_pkt_len = load_options(DHCP_DHCP_CLNT, rep_pktp, replen, optp,
	    ecp, vecp);

	if (used_pkt_len < sizeof (PKT))
		used_pkt_len = sizeof (PKT);

	(void) send_reply(ifp, rep_pktp, used_pkt_len, &plp->pkt->ciaddr);

	free_encode_list(ecp);
	free_encode_list(vecp);
	free(rep_pktp);
}

static char *
disp_client_msg(PKT_LIST *plp, char *bufp, int len)
{
	u_char tlen;

	bufp[0] = '\0';	/* null string */

	if (plp && plp->opts[CD_MESSAGE]) {
		tlen = ((u_char)len < plp->opts[CD_MESSAGE]->len) ?
		    (len - 1) : plp->opts[CD_MESSAGE]->len;
		(void) memcpy(bufp, plp->opts[CD_MESSAGE]->value, tlen);
		bufp[tlen] = '\0';
	}
	return (bufp);
}

static PKT *
gen_reply_pkt(PKT_LIST *plp, int type, u_int *len, u_char **optpp,
    struct in_addr *serverip)
{
	PKT	*reply_pktp;
	u_short		plen;

	/*
	 * We need to determine the packet size. Perhaps the client has told
	 * us?
	 */
	if (plp->opts[CD_MAX_DHCP_SIZE]) {
		(void) memcpy(&plen, plp->opts[CD_MAX_DHCP_SIZE]->value, 2);
		plen = ntohs(plen);
	} else {
		/*
		 * Define size to be a fixed length. Too hard to add up all
		 * possible class id, macro, and hostname/lease time options
		 * without doing just about as much work as constructing the
		 * whole reply packet.
		 */
		plen = DHCP_MAX_REPLY_SIZE;
	}

	/* Generate a generically initialized BOOTP packet */
	reply_pktp = gen_bootp_pkt(plen, plp->pkt);

	reply_pktp->op = BOOTREPLY;
	*optpp = reply_pktp->options;

	/*
	 * Set pkt type.
	 */
	*(*optpp)++ = (u_char)CD_DHCP_TYPE;
	*(*optpp)++ = (u_char)1;
	*(*optpp)++ = (u_char)type;

	/*
	 * All reply packets have server id set.
	 */
	*(*optpp)++ = (u_char)CD_SERVER_ID;
	*(*optpp)++ = (u_char)4;
#if	defined(_LITTLE_ENDIAN)
	*(*optpp)++ = (u_char)(serverip->s_addr & 0xff);
	*(*optpp)++ = (u_char)((serverip->s_addr >>  8) & 0xff);
	*(*optpp)++ = (u_char)((serverip->s_addr >> 16) & 0xff);
	*(*optpp)++ = (u_char)((serverip->s_addr >> 24) & 0xff);
#else
	*(*optpp)++ = (u_char)((serverip->s_addr >> 24) & 0xff);
	*(*optpp)++ = (u_char)((serverip->s_addr >> 16) & 0xff);
	*(*optpp)++ = (u_char)((serverip->s_addr >>  8) & 0xff);
	*(*optpp)++ = (u_char)(serverip->s_addr & 0xff);
#endif	/* _LITTLE_ENDIAN */

	*len = plen;
	return (reply_pktp);
}

/*
 * If the client requests it, and it isn't currently configured, provide
 * the option. Will also work for NULL ENCODE lists, but initializing them
 * to point to the requested options.
 *
 * If nsswitch contains host name services which hang, big problems occur
 * with dhcp server, since the main thread hangs waiting for that name
 * service's timeout.
 *
 * NOTE: this function should be called only after all other parameter
 * merges have taken place (combine_encode).
 */
static void
add_request_list(IF *ifp, PKT_LIST *plp, ENCODE **ecp, struct in_addr *ip)
{
	ENCODE	*ep, *ifecp, *end_ecp = NULL;
	struct hostent	h, *hp;
	char hbuf[BUFSIZ];
	int herrno;

	/* Find the end. */
	if (*ecp) {
		for (ep = *ecp; ep->next; ep = ep->next)
			/* null */;
		end_ecp = ep;
	}

	/* HOSTNAME */
	if (is_option_requested(plp, CD_HOSTNAME) && find_encode(*ecp,
	    CD_BOOL_HOSTNAME) == NULL) {
		hp = gethostbyaddr_r((char *)ip, sizeof (struct in_addr),
		    AF_INET, &h, hbuf, sizeof (hbuf), &herrno);
		if (hp != NULL) {
			if (end_ecp) {
				end_ecp->next = make_encode(CD_HOSTNAME,
				    strlen(hp->h_name), hp->h_name, 1);
				end_ecp = end_ecp->next;
			} else {
				end_ecp = make_encode(CD_HOSTNAME,
				    strlen(hp->h_name), hp->h_name, 1);
			}
		}
	}

	/*
	 * all bets off for the following if thru a relay agent.
	 */
	if (plp->pkt->giaddr.s_addr != 0L)
		return;

	/* SUBNET MASK */
	if (is_option_requested(plp, CD_SUBNETMASK) && find_encode(*ecp,
	    CD_SUBNETMASK) == NULL) {
		ifecp = find_encode(ifp->ecp, CD_SUBNETMASK);
		if (end_ecp) {
			end_ecp->next = dup_encode(ifecp);
			end_ecp = end_ecp->next;
		} else
			end_ecp = dup_encode(ifecp);
	}

	/* BROADCAST ADDRESS */
	if (is_option_requested(plp, CD_BROADCASTADDR) && find_encode(*ecp,
	    CD_BROADCASTADDR) == NULL) {
		ifecp = find_encode(ifp->ecp, CD_BROADCASTADDR);
		if (end_ecp) {
			end_ecp->next = dup_encode(ifecp);
			end_ecp = end_ecp->next;
		} else
			end_ecp = dup_encode(ifecp);
	}

	/* IP MTU */
	if (is_option_requested(plp, CD_MTU) && find_encode(*ecp,
	    CD_MTU) == NULL) {
		ifecp = find_encode(ifp->ecp, CD_MTU);
		if (end_ecp) {
			end_ecp->next = dup_encode(ifecp);
			end_ecp = end_ecp->next;
		} else
			end_ecp = dup_encode(ifecp);
	}

	if (*ecp == NULL)
		*ecp = end_ecp;
}

/*
 * Is a specific option requested? Returns True if so, False otherwise.
 */
static int
is_option_requested(PKT_LIST *plp, u_short code)
{
	u_char c, *tp;
	DHCP_OPT *cp = plp->opts[CD_REQUEST_LIST];

	for (c = 0, tp = (u_char *)cp->value; c < cp->len; c++, tp++) {
		if (*tp == (u_char)code)
			return (TRUE);
	}
	return (FALSE);
}

/*
 * Boolean: Returns TRUE if lease is negotiable, FALSE otherwise.
 */
static int
lease_negotiable(ENCODE *ecp)
{
	ENCODE	*ep;

	for (ep = ecp; ep; ep = ep->next) {
		if (ep->code == CD_BOOL_LEASENEG)
			return (TRUE);
	}
	return (FALSE);
}

/*
 * Returns the host order value of the LEASE TIME option. 0 if no such
 * option exists.
 */
static time_t
get_lease_option(ENCODE *ecp)
{
	time_t	retval = 0;
	u_char *data;
	u_char ulen;

	if ((data = get_option_code(ecp, CD_LEASE_TIME, &ulen)) != NULL &&
	    ulen == sizeof (time_t)) {
		(void) memcpy((void *)&retval, data, sizeof (time_t));
		retval = htonl(retval);
	}
	return (retval);
}

/*
 * Get random options in options doc format.
 */
static u_char *
get_option_code(ENCODE *ecp, u_char code, u_char *len)
{
	ENCODE *ep;
	u_char	*retp = NULL;

	for (ep = ecp; ep; ep = ep->next) {
		if (ep->code == code) {
			retp = ep->data;
			*len = ep->len;
			break;
		}
	}
	return (retp);
}

/*
 * Locates lease option, if possible, otherwise allocates an encode and
 * appends it to the end. Changes current lease setting.
 *
 * XXXX - ugh. We don't address the case where the Lease time changes, but
 * T1 and T2 don't. We don't want T1 or T2 to be greater than the lease
 * time! Perhaps T1 and T2 should be a percentage of lease time... Later..
 */
static void
set_lease_option(ENCODE **ecpp, time_t lease)
{
	u_char	*ltp;
	u_char		len;
	ENCODE	*ep, *prev_ep;

	lease = htonl(lease);

	if (ecpp != NULL && (ltp = get_option_code(*ecpp, CD_LEASE_TIME,
	    &len)) != NULL && len == sizeof (time_t)) {
		(void) memcpy(ltp, (void *)&lease, sizeof (time_t));
	} else {
		if (*ecpp != NULL) {
			for (prev_ep = ep = *ecpp; ep; ep = ep->next)
				prev_ep = ep;
			prev_ep->next = make_encode(CD_LEASE_TIME,
			    sizeof (time_t), (void *)&lease, 1);
		} else {
			*ecpp = make_encode(CD_LEASE_TIME,
			    sizeof (time_t), (void *)&lease, 1);
			(*ecpp)->next = NULL;
		}
	}
}
/*
 * Sets appropriate option in passed ENCODE list for lease. Returns
 * calculated relative lease time.
 */
static int
config_lease(PKT_LIST *plp, PN_REC *pnp, ENCODE **ecpp, time_t oldlease)
{
	int	negot = 0;
	time_t		newlease, rel_current;

	/*
	 * Calculate lease time.
	 */
	if (ecpp && lease_negotiable(*ecpp))
		++negot;

	if (ecpp == NULL || (rel_current = get_lease_option(*ecpp)) == 0)
		rel_current = (time_t)DEFAULT_LEASE;

	if (pnp->flags & F_AUTOMATIC || !negot) {
		if (pnp->flags & F_AUTOMATIC)
			newlease = ntohl(DHCP_PERM);
		else {
			/* sorry! */
			if (oldlease)
				newlease = oldlease;
			else
				newlease = rel_current;
		}
	} else {
		/*
		 * lease is not automatic and is negotiable!
		 * If the dhcp-network lease is bigger than the current
		 * policy value, then let the client benefit from this
		 * situation.
		 */
		if ((u_long)oldlease > (u_long)rel_current)
			rel_current = oldlease;

		if (plp->opts[CD_LEASE_TIME]) {
			/*
			 * Client is requesting a lease renegotiation.
			 */
			(void) memcpy((void *)&newlease,
			plp->opts[CD_LEASE_TIME]->value, sizeof (time_t));

			newlease = ntohl(newlease);

			/*
			 * Note that this comparison handles permanent
			 * leases as well. Limit lease to configured value.
			 */
			if ((u_long)newlease > (u_long)rel_current)
				newlease = rel_current;
		} else
			newlease = rel_current;
	}

	set_lease_option(ecpp, newlease);

	return (newlease);
}

/*
 * If a packet has the classid set, return the value, else return null.
 */
static char *
get_class_id(PKT_LIST *plp, char *bufp, int len)
{
	u_char	*ucp, ulen;
	char	*retp;

	if (plp->opts[CD_CLASS_ID]) {
		/*
		 * If the class id is set, see if there is a macro by this
		 * name. If so, then "OR" the ENCODE settings of the class
		 * macro with the packet macro. Settings in the packet macro
		 * OVERRIDE settings in the class macro.
		 */
		ucp = (u_char *)((u_int)plp->opts[CD_CLASS_ID] + 1);
		ulen = *ucp++;
		if (len < ulen)
			ulen = len;
		(void) memcpy(bufp, ucp, ulen);
		bufp[ulen] = '\0';

		retp = bufp;
	} else
		retp = NULL;

	return (retp);
}

/*
 * adds an offer to the end of an offer list. Lease time is expected to
 * be set by caller.
 */
static void
add_offer(IF *ifp, u_char *cid, int cid_len, PN_REC *pnp)
{
	OFFLST	*offp, *prevp, *tmpp;

	for (offp = prevp = ifp->of_head; offp; offp = offp->next)
		prevp = offp;

	/* LINTED [smalloc returns lw aligned values] */
	tmpp = (OFFLST *)smalloc(sizeof (OFFLST));

	(void) memcpy((char *)&tmpp->pn, pnp, sizeof (PN_REC));
	(void) memcpy((char *)&tmpp->pn.cid, cid, cid_len);
	tmpp->pn.cid_len = cid_len;
	tmpp->stamp = time(NULL) + off_secs;
	tmpp->next = NULL;

	if (prevp == NULL)
		ifp->of_head = tmpp;
	else
		prevp->next = tmpp;
}

/*
 * finds and extracts an offer from an ifp offer list. Returns NULL if
 * not found, OFFLST * if found. Caller is responsible for freeing it.
 */
static OFFLST *
find_offer(IF *ifp, u_char *cid, int cid_len)
{
	OFFLST *offp, *prevp;
	char	ntoab[NTOABUF];

	for (offp = prevp = ifp->of_head; offp; offp = offp->next) {
		if (offp->pn.cid_len == cid_len &&
		    memcmp(offp->pn.cid, cid, cid_len) == 0) {
			if (debug) {
				dhcpmsg(LOG_INFO, "Found offer for: %s\n",
				    inet_ntoa_r(offp->pn.clientip, ntoab));
			}
			if (offp == ifp->of_head)
				ifp->of_head = offp->next;
			else
				prevp->next = offp->next;
			break;
		} else
			prevp = offp;
	}
	return (offp);
}

/*
 * Given an IP address, check an interface's offer list. Returns 1 if an
 * offer exists of this address, 0 otherwise. Note that this same function
 * checks timeouts implicitly. (we're walking the list anyway) Timed out
 * entries are silently removed before the check is done, thus this function
 * serves as a "cleanup_offers" function as well, when called with a bogus
 * IP address.
 */
int
check_offers(IF *ifp, struct in_addr *ipp)
{
	OFFLST *offp, *prevp;
	time_t	now;
	int	found = FALSE;
	char	ntoab[NTOABUF];

	now = time(NULL);

	offp = prevp = ifp->of_head;
	while (offp) {
		if (offp->stamp <= now) {
			if (debug) {
				dhcpmsg(LOG_INFO, "Freeing offer for: %s\n",
				    inet_ntoa_r(offp->pn.clientip, ntoab));
			}
			if (offp == ifp->of_head) {
				ifp->of_head = offp->next;
				free(offp);
				offp = prevp = ifp->of_head;
			} else {
				prevp->next = offp->next;
				free(offp);
				offp = prevp->next;
			}
		} else {
			if (offp->pn.clientip.s_addr == ipp->s_addr) {
				found = TRUE;
				break;
			}
			prevp = offp;
			offp = offp->next;
		}
	}
	return (found);
}

/*
 * Free offers
 */
void
free_offers(IF *ifp)
{
	OFFLST	*offerp, *toffp;

	offerp = ifp->of_head;
	while (offerp) {
		toffp = offerp;
		offerp = offerp->next;
		free(toffp);
	}
	ifp->of_head = NULL;
}

/*
 * Allocate a new entry in the dhcp-network db for the cid, taking into
 * account requested IP address. Verify address.
 *
 * The network portion of the address doesn't have to be the same as ours,
 * just owned by us. We also make sure we don't select a record which is
 * currently undergoing ICMP validation (or has undergone ICMP validation)
 *
 * Returns:	1 if there's a usable entry for the client, 0
 *		if not. Places the record in the PN_REC structure
 *		handed in.
 */
int
select_offer(PER_NET_DB *pndbp, PKT_LIST *plp, IF *ifp, PN_REC *pnp)
{
	struct in_addr	req_ip;
	time_t		now, lru;
	struct in_addr	lru_cip;
	int		i, found = FALSE, err = 0, recs;
	int		zero = 0;

	/*
	 * Is the client requesting a specific address? Is so, and
	 * we can satisfy him, do so.
	 */
	if (plp->opts[CD_REQUESTED_IP_ADDR]) {
		(void) memcpy((void *)&req_ip,
		    plp->opts[CD_REQUESTED_IP_ADDR]->value,
		    sizeof (struct in_addr));

		/*
		 * first, check the ICMP list or offer list.
		 */
		if (check_offers(ifp, &req_ip)) {
			/* Offered to someone else. Sorry. */
			found = FALSE;
		} else {
			if ((recs = lookup_per_net(pndbp, PN_CLIENT_IP,
			    (void *)&req_ip, sizeof (struct in_addr),
			    &server_ip, pnp)) < 0) {
				return (0);
			}
			if (recs != 0) {
				/*
				 * Ok, the requested IP exists. But is it
				 * Available?
				 */
				if ((pnp->flags & (F_MANUAL | F_UNUSABLE)) ||
				    (pnp->cid_len != 0 &&
				    (pnp->flags & (F_AUTOMATIC |
				    F_BOOTP_ONLY))) || ((u_long)time(NULL) <
				    (u_long)ntohl(pnp->lease))) {
					/* can't use it */
					found = FALSE;
				} else {
					found = (icmp_echo_status(ifp,
					    &pnp->clientip, TRUE,
					    DHCP_ICMP_DONTCARE) == FALSE);
				}
			}
		}
	}

	if (!found) {
		/*
		 * Try to find a free entry. Look for an AVAILABLE entry
		 * (cid == 0x00, len == 1.
		 */
		if ((recs = lookup_per_net(pndbp, PN_CID, (void *)&zero, 1,
		    &server_ip, pnp)) < 0) {
			return (0);
		}

		for (i = 0, err = 0; i < recs && err == 0; i++,
		    err = get_per_net(pndbp, PN_CID, pnp)) {
			if ((pnp->flags & F_UNUSABLE) == 0 &&
			    !check_offers(ifp, &pnp->clientip) &&
			    !icmp_echo_status(ifp, &pnp->clientip, TRUE,
			    DHCP_ICMP_DONTCARE)) {
				if (plp->opts[CD_DHCP_TYPE] == NULL) {
					/* bootp client */
					if (pnp->flags & F_BOOTP_ONLY) {
						found = TRUE;
						break;
					}
				} else {
					/* dhcp client */
					if ((pnp->flags & F_BOOTP_ONLY) == 0) {
						found = TRUE;
						break;
					}
				}
			}
		}
	}
	if (!found && plp->opts[CD_DHCP_TYPE] != NULL) {
		/*
		 * Struck out. No usable available addresses. Let's look for
		 * the LRU expired address. Only makes sense for dhcp
		 * clients.
		 */
		now = time(NULL);
		if ((recs = lookup_per_net(pndbp, PN_DONTCARE, NULL, 0,
		    &server_ip, pnp)) < 0) {
			return (0);
		}

		for (i = 0, err = 0, lru = (time_t)0; i < recs && err == 0; i++,
		    err = get_per_net(pndbp, PN_DONTCARE, pnp)) {
			if (((pnp->flags & (F_UNUSABLE | F_BOOTP_ONLY |
			    F_MANUAL)) == 0) &&
			    !check_offers(ifp, &pnp->clientip) &&
			    !icmp_echo_status(ifp, &pnp->clientip, TRUE,
			    DHCP_ICMP_DONTCARE) && (u_long)ntohl(pnp->lease) <
			    (u_long)now) {
				if (lru != (time_t)0) {
					if ((u_long)ntohl(pnp->lease) <
					    (u_long)ntohl(lru)) {
						lru = pnp->lease;
						lru_cip.s_addr =
						    pnp->clientip.s_addr;
					}
				} else {
					lru = pnp->lease;
					lru_cip.s_addr =
					    pnp->clientip.s_addr;
				}
			}
		}

		if (err == 0 && lru != (time_t)0) {
			/*
			 * Get the least recently used address.
			 * Already known to be not an ICMP record.
			 */
			if ((recs = lookup_per_net(pndbp, PN_CLIENT_IP,
			    (void *)&lru_cip, sizeof (struct in_addr),
			    &server_ip, pnp)) < 0) {
				return (0);
			}

			if (recs != 0)
				found = 1;
		}
	}
	return (found);
}
