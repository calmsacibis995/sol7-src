/*
 * Copyright (c) 1987-1993, by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef	_KVM_H
#define	_KVM_H

#pragma ident	"@(#)kvm.h	2.18	97/07/04 SMI"

#include <sys/types.h>
#include <nlist.h>
#include <sys/user.h>
#include <sys/proc.h>


#ifdef __cplusplus
extern "C" {
#endif

/* define a 'cookie' to pass around between user code and the library */
typedef struct _kvmd kvm_t;

/* libkvm routine definitions */

#ifdef __STDC__
extern kvm_t		*kvm_open(char *, char *, char *, int, char *);
extern int		 kvm_close(kvm_t *);
extern int		 kvm_nlist(kvm_t *, struct nlist []);
extern ssize_t		 kvm_read(kvm_t *, uintptr_t, void *, size_t);
extern ssize_t		 kvm_kread(kvm_t *, uintptr_t, void *, size_t);
extern ssize_t		 kvm_uread(kvm_t *, uintptr_t, void *, size_t);
extern ssize_t		 kvm_write(kvm_t *, uintptr_t, void *, size_t);
extern ssize_t		 kvm_kwrite(kvm_t *, uintptr_t, void *, size_t);
extern ssize_t		 kvm_uwrite(kvm_t *, uintptr_t, void *, size_t);
extern struct proc	*kvm_getproc(kvm_t *, pid_t);
extern struct proc	*kvm_nextproc(kvm_t *);
extern int		 kvm_setproc(kvm_t *);
extern struct user	*kvm_getu(kvm_t *, struct proc *);
extern int		 kvm_getcmd(kvm_t *, struct proc *, struct user *,
			    char ***, char ***);
#else

extern kvm_t		*kvm_open();
extern int		 kvm_close();
extern int		 kvm_nlist();
extern ssize_t		 kvm_read();
extern ssize_t		 kvm_kread();
extern ssize_t		 kvm_uread();
extern ssize_t		 kvm_write();
extern ssize_t		 kvm_kwrite();
extern ssize_t		 kvm_uwrite();
extern struct proc	*kvm_getproc();
extern struct proc	*kvm_nextproc();
extern int		 kvm_setproc();
extern struct user	*kvm_getu();
extern int		 kvm_getcmd();
#endif	/* __STDC__ */

#ifdef	__cplusplus
}
#endif

#endif	/* _KVM_H */
