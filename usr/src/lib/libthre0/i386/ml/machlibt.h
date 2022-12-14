/*	Copyright (c) 1993 SMI	*/
/*	 All Rights Reserved	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SMI	*/
/*	The copyright notice above does not evidence any	*/
/*	actual or intended publication of such source code.	*/

#pragma	ident   "@(#)machlibthread.h 1.8     97/01/15     SMI"

/* Structure returned by fnstenv */
typedef struct fpuenv {
	int fctrl;	/* control word */
	int fstat;	/* status word (flags, etc) */
	int ftag;	/* tag of which regs busy */
	int misc[4];	/* other stuff, 28 bytes total */
} fpuenv_t;

typedef struct {
	long	rs_bp;
	long	rs_edi;
	long	rs_esi;
	long	rs_ebx;
	long	rs_sp;
	long	rs_pc;
	fpuenv_t  rs_fpuenv;
} resumestate_t;

#define	t_bp 	t_resumestate.rs_bp
#define	t_edi 	t_resumestate.rs_edi
#define	t_esi 	t_resumestate.rs_esi
#define	t_ebx 	t_resumestate.rs_ebx
#define	t_sp	t_resumestate.rs_sp
#define	t_pc	t_resumestate.rs_pc
#define	t_fpenv	t_resumestate.rs_fpuenv
#define	t_fctrl	t_resumestate.rs_fpuenv.fctrl
#define	t_fstat	t_resumestate.rs_fpuenv.fstat
