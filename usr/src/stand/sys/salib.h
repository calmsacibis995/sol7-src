/*
 * Copyright (c) 1996, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_SYS_SALIB_H
#define	_SYS_SALIB_H

#pragma ident	"@(#)salib.h	1.6	97/04/06 SMI"

#include <sys/types.h>

#ifdef	__cplusplus
extern "C" {
#endif

extern void bzero(void *, size_t);
extern void bcopy(const void *, void *, size_t);
extern int bcmp(const void *, const void *, size_t);

extern size_t strlen(const char *);
extern char *strcat(char *, const char *);
extern char *strncat(char *, const char *, size_t);
extern char *strcpy(char *, const char *);
extern char *strncpy(char *, const char *, size_t);
extern int strcmp(const char *, const char *);
extern int strncmp(const char *, const char *, size_t);
extern char *strchr(const char *, int);
extern char *strrchr(const char *, int);
extern char *strstr(const char *, const char *);

/*PRINTFLIKE1*/
extern void printf(char *, ...);
/*PRINTFLIKE2*/
extern char *sprintf(char *, char *, ...);

extern char *bkmem_alloc(unsigned int);
extern void bkmem_free(char *, u_int);

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_SALIB_H */
