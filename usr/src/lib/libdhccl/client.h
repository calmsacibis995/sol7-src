/*
 * Copyright 1992-1996 Competitive Automation. All Rights Reserved
 * Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#ifndef	_CLIENT_H
#define	_CLIENT_H

#pragma ident	"@(#)client.h	1.6	96/11/26 SMI"

#include <sys/types.h>
#include <hflags.h>
#include <haddr.h>
#include <ifinst.h>
#include <signal.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	CONFIG_DIR	"/etc/dhcp"
#define	POLICY_FILE	"/dhcpagent.policy"
#define	CAHOSTNAMELEN	(64)
#define	INFINITY_TIME	((time_t)0x7fffffff)

/* Forward declarations: */

struct Host;
struct bootp;
struct dhcptimer;
union  dhcpipc;
struct in_addr;
struct pollfd;
struct shared_bindata;
struct sockaddr_in;

typedef struct dhcpclient {
	char *dhcpconfigdir;
	char *class_id;
	char *client_id;
	char hostname[CAHOSTNAMELEN];
	HFLAGS reqvec;
	HFLAGS hostvec;
	int32_t lease_desired;
	int32_t timewindow;
	uint32_t arp_timeout;
	uint16_t waitonboot;
	uint16_t retries;
	uint16_t boot;
	int16_t *timeouts;
	int32_t DontConfigure; /* if !=0 run the client without configuring */
	int32_t lastError;
	int32_t lasterrno;
	int32_t useNonVolatile;
	int32_t acceptBootp;
	sigset_t blockDuringTimeout;
	int sock;
	int controlSock;
	HADDR *UserDefinedMacAddr;
	int ifsConfigured;
	struct Host *globalh;
} DHCPCLIENT;

extern DHCPCLIENT client;

int agentInit(void);
void agent_exit(void);
int armIf(IFINSTANCE*, const HFLAGS, const struct in_addr *);
int arp(IFINSTANCE *);
void cancel_inactivity_timer(void);
int canonical_if(IFINSTANCE *, int);
int checkAddr(IFINSTANCE *);
int clientSocket(int, const struct in_addr *);
int clpolicy(const char *);
void configGateway(int, const struct Host *);
void configIf(const char *, const struct Host *);
int configHostname(const struct Host *);

#ifdef	__CODE_UNUSED
void configRoutes(int, const struct Host *);
#endif	/* __CODE_UNUSED */
int controlSocket();
void convertClass(char **, char *);
void convertClient(struct shared_bindata **, char *, const char *,
    const HADDR*, u_char);
void ctrl_reply(int, const union dhcpipc *);
void decline(IFINSTANCE *);
void discard_dhc(IFINSTANCE *);
void dropIf(IFINSTANCE *);
void dumpIf(IFINSTANCE *);
void dumpInternals(void);
int enableSigpoll(int fd);
void examineReply(const struct bootp *, int, const struct sockaddr_in *);
void expireIf(IFINSTANCE *);
void extend(IFINSTANCE *);
void fail(IFINSTANCE*, int);
void fromTimer(const dhcptimer*, struct Host *);
int fswok(const char *);
void handle_terminate_signals(void);
const char *ifBusyString(int);
const char *ifStateString(int);
int ifsetup(IFINSTANCE *);
void init(IFINSTANCE *);
void initFailure(IFINSTANCE *);
void initReboot(IFINSTANCE *);
void initSuccess(IFINSTANCE *);
void install_inactivity_timer(void);
const char *msgtxt(int);
IFINSTANCE *newIf(const char *);
void polltype(const struct pollfd *pfd);
void prepareDiscover(IFINSTANCE *);
void prepareRebind(IFINSTANCE *);
void prepareReboot(IFINSTANCE *);
void prepareRelease(IFINSTANCE *);
void prepareRenew(IFINSTANCE *);
void prepareRequest(IFINSTANCE *);
int read_dhc(const char *, const char *, struct Host **);
void readBOOTPsocket(int);
void readIFfd(int);
void rebind(IFINSTANCE *);
void reinit(IFINSTANCE *);
void releaseIf(IFINSTANCE *);
void renew(IFINSTANCE *);
void renewComplete(IFINSTANCE *);
void request(IFINSTANCE *);
void retry(IFINSTANCE*, int);
void save_dhc(IFINSTANCE *);
void seed(void);
int sendBp(IFINSTANCE*, int);
void setExpiration(struct Host *);
void setNewWakeupTime(IFINSTANCE *);
void setT1T2(struct Host *);
void startIf(IFINSTANCE *);
void timeout(void *);
void toTimer(dhcptimer*, const struct Host *);
void unlink_dhc(const char *, const char *);
void useNonVolatile(IFINSTANCE *);
void waitForEvent(void);
int write_dhc(const char *, const char *, const struct Host *);

#if	EMPLOY_BOOTP
void bootpComplete(IFINSTANCE *);
#endif

#ifdef	__cplusplus
}
#endif

#endif /* !_CLIENT_H */
