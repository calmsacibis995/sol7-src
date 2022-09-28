/*
 *	Copyright 1992-1996 Competitive Automation. All Rights Reserved
 *	Copyright (c) 1996 by Sun Microsystems, Inc. All Rights Reserved
 */

#ifndef _CA_UTILS_H
#define	_CA_UTILS_H

#pragma ident	"@(#)utils.h	1.5	96/12/19 SMI"

#include <catype.h>
#include <stdio.h>
#include <signal.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	ADDRSTR(a)	addrstr((a), 4, 0, '.')
#define	ITOA(i, j)	multiAddrstr((i), 4, 0, j, '.')
#define	MTOA(m, l, j)	multiAddrstr((m), l, 1, j, ':')
#define	YCDISP(a, b, c, d)	ycdisp((a), (b), (c), (d), 16, 0, 0)

struct ether_header;
struct ether_arp;
struct in_addr;
struct ip;
struct packetfilt;
struct udphdr;
struct sockaddr;
struct timeb;
struct haddr;

const char *addrstr(const void *, int, int, int);
int addrt(int, int, int, const struct sockaddr *, const struct sockaddr *);
int arpCheck(const char *, int32_t, const struct in_addr *, void *);
char *body(const char *, char *);
int bytetok(const char **, u_char *);
int cancelWakeup(int);
int countTokens(const char *, int, int);
int DLPItoBOOTPmediaType(int);
char *extn(char *);
int haddrlength(int);

#ifdef	__CODE_UNUSED
char *haddrtoa(const u_char *, int, int);
int delrt(int, int, const struct sockaddr *, const struct sockaddr *);
#endif	/* __CODE_UNUSED */

#ifdef	DEBUG
void showARPpacket(const struct ether_arp *);
void showEthernetHeader(const struct ether_header *);
void showIPheader(FILE *, const struct ip *);
void showUDPheader(FILE *, const struct udphdr *);
#endif	/* DEBUG */

int haddrtok(const char **, u_char *, int);
char *head(char *, char *);
void iftoppa(const char *, char **, int *, int *);
int in_cksum(u_short *, int);
int inet_aton(const char *, struct in_addr *);
void loginit(int, int, int, char *);
void logclose(void);
void loglbyl(void);
const char *multiAddrstr(const void *, int, int, int, int);
int myMacAddr(struct haddr *, const char *);
char *root(char *, char *);
int schedule(time_t, void (*)(void *), void *, const sigset_t *);
void showWakeups(void);
void showvec(FILE *, const char *, const u_char *, int);
int strioctl(int, int, int, int, void *);
char *tail(char *);
int tokeniseString(const char *, char **, int, int);
void *xmalloc(int);
void *xrealloc(void *, int);
char *xstrdup(const char *);
void ycdisp(FILE *, const void *, const char *, int, int, int, int);

#ifdef	__cplusplus
}
#endif

#endif /* _CA_UTILS_H */
